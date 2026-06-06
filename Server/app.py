"""
EIT Flask Server
================
Receives live EIT frames from MCU via HTTP POST and streams them to the
browser via Server-Sent Events (SSE).  Also accepts full exam sessions.

Endpoints
---------
GET  /                    Live dashboard
GET  /api/events          SSE stream  (browser subscribes here)
GET  /api/status          JSON status snapshot
POST /api/frame           Single live frame  {"f","t","ref","nm","ni","uel","px"}
POST /api/exam            Full exam JSON (same format as the MCU's RECORD output)
POST /api/exam/begin      Start a multi-frame exam session → {"session_id": "..."}
POST /api/exam/frame      One frame for an open session
POST /api/exam/end        Close session, reconstruct all frames

Usage
-----
  pip install -r requirements.txt
  python app.py

Then open http://localhost:5000 in a browser.
Set the MCU's WiFi server URL to  http://<your-pc-ip>:5000/api/exam  (for exam send)
The streaming feature uses /api/frame automatically (derived from the same host:port).
"""

import json
import math
import os
import struct
import threading
import time
from pathlib import Path
from queue import Empty, Queue

from flask import Flask, Response, jsonify, render_template, request, stream_with_context

app = Flask(__name__)

# ── Global state ──────────────────────────────────────────────────────────────

_lock = threading.Lock()

# Per-frame live state
_ref_uel: list = []
_frame_count: int = 0
_last_frame_ts: float = 0.0
_fps: float = 0.0
_streaming: bool = False

# Sensitivity matrix (loaded once at startup)
_S = None          # numpy ndarray (n_measurements, n_pixels) or None
_S_n_inj: int = 0
_S_n_meas: int = 0
_S_image_size: int = 32

# SSE subscriber queues
_sse_queues: list = []

# Exam sessions  {session_id: {"frames": [...], "ref_uel": [...], meta...}}
_exam_sessions: dict = {}

# Latest exam replay data (from POST /api/exam or closed session)
_exam_replay: dict = {}   # {"frames": [...reconstructed...], "image_size": N}

# ── Sensitivity matrix ────────────────────────────────────────────────────────

def _load_sensitivity_matrix() -> bool:
    global _S, _S_n_meas, _S_n_inj, _S_image_size

    try:
        import numpy as np
    except ImportError:
        print("[Server] numpy not installed – server-side LBP disabled")
        return False

    candidates = [
        Path(__file__).parent.parent / "FIPS Data" / "data_bin_files" / "sensitivity_matrix.bin",
        Path("FIPS Data/data_bin_files/sensitivity_matrix.bin"),
        Path("sensitivity_matrix.bin"),
    ]
    for path in candidates:
        if not path.exists():
            continue
        try:
            with open(path, "rb") as f:
                hdr = f.read(32)
                if len(hdr) < 32:
                    continue
                magic, n_total, n_pixels, img_sz, n_inj, n_meas, _, _ = struct.unpack("<8I", hdr)
                if magic != 0x53454E53:
                    continue
                raw = f.read()
                _S = np.frombuffer(raw, dtype=np.float32).reshape(n_total, n_pixels)
                _S_n_meas = n_meas
                _S_n_inj = n_inj
                _S_image_size = img_sz
            print(f"[Server] Sensitivity matrix loaded: {_S.shape}  from {path}")
            return True
        except Exception as exc:
            print(f"[Server] Failed to load {path}: {exc}")

    print("[Server] WARNING: No sensitivity matrix found – server-side LBP disabled.")
    print("[Server] Run:  python 'FIPS Data/generate_sensitivity_matrix.py'  to generate it.")
    return False


def _lbp_reconstruct(ref_uel: list, tgt_uel: list) -> list:
    """Return flat list of image_size² reconstructed pixel values in [0,1], or []."""
    if _S is None or not ref_uel or not tgt_uel:
        return []
    try:
        import numpy as np
        ref = np.asarray(ref_uel, dtype=np.float32)
        tgt = np.asarray(tgt_uel, dtype=np.float32)
        if ref.shape != tgt.shape or ref.shape[0] != _S.shape[0]:
            return []
        dv = tgt - ref
        ds = _S.T @ dv
        lo, hi = ds.min(), ds.max()
        rng = hi - lo
        if rng > 1e-12:
            ds = (ds - lo) / rng
        else:
            ds = np.zeros_like(ds)
        return ds.tolist()
    except Exception:
        return []

# ── SSE helpers ───────────────────────────────────────────────────────────────

def _broadcast(payload: dict) -> None:
    """Push a JSON event to all SSE subscribers."""
    msg = f"data: {json.dumps(payload)}\n\n"
    with _lock:
        dead = []
        for q in _sse_queues:
            try:
                q.put_nowait(msg)
            except Exception:
                dead.append(q)
        for d in dead:
            _sse_queues.remove(d)

# ── Routes ────────────────────────────────────────────────────────────────────

@app.before_request
def _log_request():
    print(f"[req] {request.method} {request.path}  "
          f"from={request.remote_addr}  "
          f"content-length={request.content_length}  "
          f"content-type={request.content_type}",
          flush=True)

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/events")
def events():
    """SSE endpoint.  Browser subscribes here for live heatmap updates."""
    q: Queue = Queue(maxsize=64)

    with _lock:
        _sse_queues.append(q)

    def generate():
        try:
            yield ": connected\n\n"
            while True:
                try:
                    yield q.get(timeout=20)
                except Empty:
                    yield ": keepalive\n\n"
        except GeneratorExit:
            pass
        finally:
            with _lock:
                if q in _sse_queues:
                    _sse_queues.remove(q)

    return Response(
        stream_with_context(generate()),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


@app.route("/api/status")
def status():
    with _lock:
        return jsonify(
            {
                "streaming": _streaming,
                "frame_count": _frame_count,
                "fps": round(_fps, 2),
                "s_matrix_loaded": _S is not None,
                "image_size": _S_image_size,
            }
        )


@app.route("/api/frame", methods=["POST"])
def receive_frame():
    """Receive a single EIT frame during live streaming."""
    global _ref_uel, _frame_count, _last_frame_ts, _fps, _streaming

    data = request.get_json(force=True, silent=True)
    if data is None:
        return jsonify({"error": "invalid JSON"}), 400

    is_ref  = int(data.get("ref", 0))
    uel     = data.get("uel", [])
    px_mcu  = data.get("px",  [])
    frame_n = int(data.get("f", 0))
    ts_ms   = int(data.get("t", 0))

    _streaming = True

    with _lock:
        now = time.time()
        if _last_frame_ts > 0:
            dt = now - _last_frame_ts
            if dt > 0:
                _fps = 0.8 * _fps + 0.2 * (1.0 / dt)
        _last_frame_ts = now
        _frame_count += 1
        frame_count = _frame_count
        fps = round(_fps, 2)

    if is_ref:
        with _lock:
            _ref_uel = uel
        px_server = []
    else:
        with _lock:
            ref = list(_ref_uel)
        px_server = _lbp_reconstruct(ref, uel)

    pixels = px_server if px_server else px_mcu

    _broadcast(
        {
            "type": "frame",
            "f": frame_n,
            "t": ts_ms,
            "ref": is_ref,
            "fps": fps,
            "frame_count": frame_count,
            "pixels": pixels,
            "image_size": _S_image_size,
        }
    )
    return jsonify({"ok": 1})


@app.route("/api/exam", methods=["POST"])
def receive_exam():
    """
    Accept the full exam JSON that the MCU's SEND button generates.
    The format matches build_and_save_json() output:
    {
      "device_id": ..., "timestamp": ..., "frame_number": ...,
      "config": {"n_meas": N, "n_inj": M, "image_size": K, ...},
      "source_file": ...,
      "measurements": {"uel_count": ..., "uel": [...]},
      "reconstruction": {"algorithm": ..., "vmin": ..., "vmax": ...}
    }
    """
    data = request.get_json(force=True, silent=True)
    if data is None:
        return jsonify({"error": "invalid JSON"}), 400

    cfg   = data.get("config", {})
    meas  = data.get("measurements", {})
    uel   = meas.get("uel", [])
    px_server = _lbp_reconstruct(_ref_uel if _ref_uel else uel, uel)

    frame = {
        "f":      data.get("frame_number", 0),
        "source": data.get("source_file", ""),
        "pixels": px_server,
        "image_size": cfg.get("image_size", _S_image_size),
        "n_meas": cfg.get("n_meas", 0),
        "n_inj":  cfg.get("n_inj", 0),
    }

    with _lock:
        global _exam_replay
        _exam_replay = {
            "frames": [frame],
            "image_size": frame["image_size"],
            "source": frame["source"],
        }

    _broadcast({"type": "exam_complete", "total_frames": 1, "frames": [frame],
                "image_size": frame["image_size"]})
    return jsonify({"ok": 1})


# ── Multi-frame exam session endpoints ────────────────────────────────────────

@app.route("/api/exam/begin", methods=["POST"])
def exam_begin():
    data = request.get_json(force=True, silent=True) or {}
    sid  = str(int(time.time() * 1000))
    with _lock:
        _exam_sessions[sid] = {
            "frames":     [],
            "ref_uel":    None,
            "n_meas":     data.get("n_meas", 0),
            "n_inj":      data.get("n_inj", 0),
            "image_size": data.get("image_size", _S_image_size),
            "created_at": time.time(),
        }
    print(f"[Server] Exam session started: {sid}")
    return jsonify({"session_id": sid})


@app.route("/api/exam/frame", methods=["POST"])
def exam_frame():
    data = request.get_json(force=True, silent=True)
    if data is None:
        return jsonify({"error": "invalid JSON"}), 400

    sid = data.get("session", "")
    with _lock:
        sess = _exam_sessions.get(sid)
    if sess is None:
        return jsonify({"error": "unknown session"}), 404

    uel    = data.get("uel", [])
    is_ref = int(data.get("ref", 0))
    if is_ref:
        with _lock:
            sess["ref_uel"] = uel

    with _lock:
        sess["frames"].append({"f": data.get("f", len(sess["frames"])),
                                "t": data.get("t", 0),
                                "ref": is_ref, "uel": uel})
        n = len(sess["frames"])
    return jsonify({"ok": 1, "frame_count": n})


@app.route("/api/exam/end", methods=["POST"])
def exam_end():
    data = request.get_json(force=True, silent=True) or {}
    sid  = data.get("session", "")
    with _lock:
        sess = _exam_sessions.pop(sid, None)
    if sess is None:
        return jsonify({"error": "unknown session"}), 404

    ref_uel = sess.get("ref_uel") or []
    reconstructed = []
    for fr in sess["frames"]:
        if fr.get("ref"):
            reconstructed.append({"f": fr["f"], "pixels": None, "ref": 1})
            continue
        px = _lbp_reconstruct(ref_uel, fr["uel"])
        reconstructed.append({"f": fr["f"], "pixels": px, "ref": 0})

    with _lock:
        global _exam_replay
        _exam_replay = {
            "frames":     reconstructed,
            "image_size": sess["image_size"],
        }

    _broadcast({
        "type":         "exam_complete",
        "session_id":   sid,
        "total_frames": len(reconstructed),
        "image_size":   sess["image_size"],
        "frames":       reconstructed,
    })
    print(f"[Server] Exam session ended: {sid}  frames={len(reconstructed)}")
    return jsonify({"ok": 1, "total_frames": len(reconstructed)})


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    _load_sensitivity_matrix()
    print("[Server] Starting on http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
