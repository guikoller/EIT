"""
EIT Firmware Image Validation Script
======================================

Validates BMP images **saved by the microcontroller** against a Python LBP
reference reconstruction.

For every firmware BMP found in ``firmware_images/`` the script:
  1. Loads the firmware BMP as raw RGB pixels
  2. Runs the same dataset through the Python LBP pipeline
  3. Applies the EXACT same blue-black-red colourmap the firmware uses
  4. Upscales + simulates RGB565 quantisation to produce an identical
     reference image
  5. Compares the two RGB images pixel-by-pixel

Metrics per image:
  - CC   Pearson Correlation Coefficient   (closer to 1  = better)
  - RIE  Relative Image Error              (lower = better)
  - PSNR Peak Signal-to-Noise Ratio in dB  (higher = better)

Workflow
--------
1. On the STM32, open each dataset and press SAVE.
   The firmware writes:  ``datamat_X_Y_LBP.bmp``
2. Copy all BMPs from the SD card into the ``firmware_images/`` folder
   next to this script.
3. Run:  python validate_firmware.py
"""

import os, sys, re, json, struct
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
from PIL import Image

# ---------------------------------------------------------------------------
#  Import existing EIT helpers
# ---------------------------------------------------------------------------
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from eit import load_mat_all

# ===================================================================
#  Configuration
# ===================================================================
SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
DATA_DIR         = os.path.join(SCRIPT_DIR, "data_mat_files")
BIN_DIR          = os.path.join(SCRIPT_DIR, "data_bin_files")
FIRMWARE_IMG_DIR = os.path.join(SCRIPT_DIR, "firmware_images")
REFERENCE_FILE   = "datamat_1_0.mat"
SENS_BIN_FILE    = os.path.join(BIN_DIR, "sensitivity_matrix.bin")
N_ELECTRODES     = 16
IMAGE_SIZE       = 32          # EIT_IMAGE_SIZE on the firmware
MIN_DIST         = 0.4
GRID_EXTENT      = 1.1


# ===================================================================
#  Load the EXACT same sensitivity matrix that the firmware uses
# ===================================================================
def load_firmware_sensitivity_matrix(path: str):
    """Read the binary sensitivity_matrix.bin with its header.
    Returns (S, n_inj, n_meas, image_size).
    """
    with open(path, "rb") as f:
        hdr = f.read(32)
        (magic, n_measurements, n_pixels, image_size,
         n_inj, n_meas, _, _) = struct.unpack("<8I", hdr)
        if magic != 0x53454E53:
            raise ValueError(f"Bad magic: 0x{magic:08X}")
        S = np.fromfile(f, dtype=np.float32).reshape(n_measurements, n_pixels)
    print(f"Loaded firmware S: shape {S.shape}, "
          f"n_inj={n_inj}, n_meas={n_meas}, image_size={image_size}")
    return S.astype(np.float64), n_inj, n_meas, image_size


# ===================================================================
#  BMP loading — raw RGB pixels (no colourmap inversion)
# ===================================================================
def load_firmware_bmp_rgb(path: str) -> np.ndarray:
    """Load the firmware BMP as an (H, W, 3) uint8 RGB array."""
    img = Image.open(path).convert("RGB")
    return np.array(img, dtype=np.uint8)          # (H, W, 3)


# ===================================================================
#  Apply the EXACT same colourmap the firmware uses
# ===================================================================
def firmware_colourmap(image: np.ndarray, mask: np.ndarray,
                       display_size: int) -> np.ndarray:
    """Replicate the firmware's blue-black-red colourmap + RGB565
    quantisation pipeline.  Vectorised with numpy for speed.

    Input : image (IMAGE_SIZE x IMAGE_SIZE float), circular mask
    Output: (display_size x display_size x 3) uint8 RGB image
    """
    h, w = image.shape
    scale = display_size // h

    # Symmetric range (exactly like the firmware)
    vals = image[mask & ~np.isnan(image)]
    vabs = max(np.max(np.abs(vals)), 1e-30) if vals.size else 1.0

    # Nearest-neighbour upscale
    idx = np.arange(display_size) // scale
    up = image[np.ix_(idx, idx)]               # (disp, disp)

    # Normalise to [-1, +1]
    norm = np.clip(up / vabs, -1.0, 1.0)
    nan_mask = np.isnan(norm)
    norm = np.nan_to_num(norm, nan=0.0)

    # Blue-black-red colourmap
    r8 = np.where(norm >= 0, (norm * 255).astype(np.int32), 0)
    g8 = np.zeros_like(r8)
    b8 = np.where(norm < 0, ((-norm) * 255).astype(np.int32), 0)

    # Simulate RGB565 quantisation round-trip
    r5 = (r8 >> 3) & 0x1F;  r8 = (r5 << 3) | (r5 >> 2)
    g6 = (g8 >> 2) & 0x3F;  g8 = (g6 << 2) | (g6 >> 4)
    b5 = (b8 >> 3) & 0x1F;  b8 = (b5 << 3) | (b5 >> 2)

    out = np.stack([r8, g8, b8], axis=-1).astype(np.uint8)
    out[nan_mask] = 0                          # NaN pixels → black
    return out


# ===================================================================
#  Parse firmware BMP filename -> dataset stem + algorithm
# ===================================================================
_FW_NAME_RE = re.compile(
    r"^(?P<stem>datamat_\d+_\d+)_(?P<algo>LBP|DBar)(?:_\d+)?\.bmp$",
    re.IGNORECASE,
)

def parse_fw_filename(name: str):
    m = _FW_NAME_RE.match(name)
    if not m:
        return None, None
    return m.group("stem"), m.group("algo").upper()


# ===================================================================
#  Python reference LBP
# ===================================================================
def python_reference_lbp(stem, S, n_inj, n_meas, U_ref, mask):
    """Run Python LBP using the SAME S and delta_v ordering as the firmware.
    
    Firmware delta_v order: for inj in 0..n_inj-1: for meas in 0..n_meas-1:
        delta_v[idx] = Uel[meas, inj] - ref[meas, inj]
    which is:  (Uel - ref)[:, :n_inj].T.flatten()
    """
    mat_path = os.path.join(DATA_DIR, stem + ".mat")
    if not os.path.isfile(mat_path):
        return None
    tgt = load_mat_all(mat_path)
    if "Uel" not in tgt:
        return None
    delta_full = tgt["Uel"] - U_ref      # (n_meas_total, n_inj_total)
    n_inj_use = min(delta_full.shape[1], n_inj)
    # Injection-major ordering (same as firmware)
    delta_v = delta_full[:, :n_inj_use].T.flatten()
    if S.shape[0] != delta_v.size:
        print(f"  DIM MISMATCH: S rows={S.shape[0]} vs delta_v={delta_v.size}")
        return None
    raw = (S.T @ delta_v).reshape(IMAGE_SIZE, IMAGE_SIZE)
    raw = -raw  # Firmware output is sign-flipped (empirically confirmed)
    # Apply the EXACT same circular mask as the firmware:
    #   center = (IMAGE_SIZE - 1) / 2.0, radius = IMAGE_SIZE / 2.0
    center = (IMAGE_SIZE - 1) / 2.0
    radius = IMAGE_SIZE / 2.0
    yy, xx = np.mgrid[0:IMAGE_SIZE, 0:IMAGE_SIZE]
    outside = np.sqrt((xx - center)**2 + (yy - center)**2) > radius
    raw[outside] = np.nan
    return raw


# ===================================================================
#  Helpers
# ===================================================================
def make_grid(size, extent=1.1):
    x = np.linspace(-extent, extent, size)
    X, Y = np.meshgrid(x, x)
    return X, Y, (np.sqrt(X**2 + Y**2) <= 1.0)


# ===================================================================
#  Image-level comparison metrics  (operate on uint8 RGB arrays)
# ===================================================================
def _domain_mask_display(size: int, src_size: int = IMAGE_SIZE) -> np.ndarray:
    """Boolean mask (size×size) matching the firmware's pixel-based circle.
    Firmware: center = (IMAGE_SIZE-1)/2, radius = IMAGE_SIZE/2, then upscaled."""
    scale = size // src_size
    center = (src_size - 1) / 2.0
    radius = src_size / 2.0
    idx = np.arange(size) // scale          # map display coord to source coord
    sx = idx[np.newaxis, :] - center        # dx for each column
    sy = idx[:, np.newaxis] - center        # dy for each row
    return np.sqrt(sx**2 + sy**2) <= radius


def _extract_domain(rgb: np.ndarray, mask: np.ndarray) -> np.ndarray:
    """Pull out the RGB values inside the domain mask as a flat float64 vector."""
    return rgb[mask].astype(np.float64).ravel()


def image_correlation(fw_rgb: np.ndarray, py_rgb: np.ndarray,
                      mask: np.ndarray) -> float:
    """Pearson r over RGB pixels inside the circular domain."""
    a = _extract_domain(fw_rgb, mask)
    b = _extract_domain(py_rgb, mask)
    if len(a) < 2:
        return np.nan
    return float(np.corrcoef(a, b)[0, 1])


def image_rie(fw_rgb: np.ndarray, py_rgb: np.ndarray,
              mask: np.ndarray) -> float:
    """Relative image error inside domain: ||fw-py|| / ||py||."""
    a = _extract_domain(fw_rgb, mask)
    b = _extract_domain(py_rgb, mask)
    nb = np.linalg.norm(b)
    if nb < 1e-30:
        return np.nan
    return float(np.linalg.norm(a - b) / nb)


def image_psnr(fw_rgb: np.ndarray, py_rgb: np.ndarray,
               mask: np.ndarray) -> float:
    """PSNR (dB) over domain pixels only."""
    a = _extract_domain(fw_rgb, mask)
    b = _extract_domain(py_rgb, mask)
    mse = np.mean((a - b) ** 2)
    if mse < 1e-30:
        return float('inf')
    return float(10.0 * np.log10(255.0**2 / mse))


# ===================================================================
#  Main
# ===================================================================
def run_validation():
    # ---- Check folder ----
    if not os.path.isdir(FIRMWARE_IMG_DIR):
        os.makedirs(FIRMWARE_IMG_DIR, exist_ok=True)
        print(f"Created empty folder: {FIRMWARE_IMG_DIR}")
        print("Copy your firmware BMP files there and re-run.")
        return

    bmp_files = sorted(f for f in os.listdir(FIRMWARE_IMG_DIR)
                       if f.lower().endswith(".bmp"))
    if not bmp_files:
        print(f"No BMP files found in {FIRMWARE_IMG_DIR}")
        print("Copy the saved firmware images there and re-run.")
        return

    # ---- Keep only files that match the naming convention ----
    entries = []
    for f in bmp_files:
        stem, algo = parse_fw_filename(f)
        if stem is None:
            print(f"  SKIP {f}: name doesn't match datamat_X_Y_LBP.bmp")
            continue
        entries.append((f, stem, algo))

    if not entries:
        print("No valid firmware BMP files found.")
        return

    print(f"\nFound {len(entries)} firmware image(s) in {FIRMWARE_IMG_DIR}\n")

    # ---- Build Python reference using FIRMWARE's own sensitivity matrix ----
    ref = load_mat_all(os.path.join(DATA_DIR, REFERENCE_FILE))
    U_ref = ref["Uel"]

    # Load the EXACT same sensitivity matrix the firmware loads from SD
    S, n_inj, n_meas, img_sz = load_firmware_sensitivity_matrix(SENS_BIN_FILE)

    # Grid + mask at source resolution (IMAGE_SIZE)
    Xs, Ys, mask_s = make_grid(IMAGE_SIZE, GRID_EXTENT)

    # ---- Process only the images that are in the folder ----
    results = []

    for fname, stem, algo in entries:
        fpath = os.path.join(FIRMWARE_IMG_DIR, fname)
        fw_rgb = load_firmware_bmp_rgb(fpath)           # (H, W, 3) uint8
        fw_h, fw_w = fw_rgb.shape[:2]

        row = {"file": fname, "stem": stem, "algo": algo, "size": fw_h}

        # Python LBP → same colourmap → same size → compare as RGB
        py_img = python_reference_lbp(
            stem, S, n_inj, n_meas, U_ref, mask_s)

        if py_img is not None:
            py_rgb = firmware_colourmap(py_img, mask_s, fw_h)  # (H, W, 3)
            dmask = _domain_mask_display(fw_h, IMAGE_SIZE)

            # Try 4 orientations to detect any flip/rotation mismatch
            orientations = {
                "original":  fw_rgb,
                "flip_Y":    fw_rgb[::-1, :, :],
                "flip_X":    fw_rgb[:, ::-1, :],
                "rot180":    fw_rgb[::-1, ::-1, :],
            }
            best_cc, best_orient = -2.0, "original"
            for oname, orgb in orientations.items():
                cc_o = image_correlation(orgb, py_rgb, dmask)
                if not np.isnan(cc_o) and cc_o > best_cc:
                    best_cc, best_orient = cc_o, oname

            # Use the best orientation for all metrics
            fw_best = orientations[best_orient]
            row["CC"]   = image_correlation(fw_best, py_rgb, dmask)
            row["RIE"]  = image_rie(fw_best, py_rgb, dmask)
            row["PSNR"] = image_psnr(fw_best, py_rgb, dmask)
            row["orient"] = best_orient
            row["fw_rgb_best"] = fw_best  # for plotting
        else:
            row["CC"]   = np.nan
            row["RIE"]  = np.nan
            row["PSNR"] = np.nan
            row["orient"] = "N/A"
            row["fw_rgb_best"] = fw_rgb
            print(f"  WARNING: no .mat file for {stem}, skipping Python comparison")

        results.append(row)

    # ---- Print table ----
    print(f"\n{'='*98}")
    print(f" Firmware vs Python LBP  —  {len(results)} image(s)")
    print(f"{'='*98}")
    print(f"{'File':<36} | {'Size':>4} | {'CC':>8} | {'RIE':>8} | {'PSNR':>8} | {'Orientation':<10}")
    print("-" * 98)

    for r in results:
        cc   = r["CC"]
        rie  = r["RIE"]
        psnr = r["PSNR"]
        cc_s   = f"{cc:8.4f}"   if not np.isnan(cc)   else "     N/A"
        rie_s  = f"{rie:8.4f}"  if not np.isnan(rie)  else "     N/A"
        psnr_s = f"{psnr:8.2f}" if not np.isnan(psnr) and not np.isinf(psnr) else "     inf"
        print(f"{r['file']:<36} | {r['size']:>4} | {cc_s} | {rie_s} | {psnr_s} | {r['orient']:<10}")

    print(f"{'='*98}\n")

    # ---- Summary ----
    cc_vals   = [r["CC"]   for r in results if not np.isnan(r["CC"])]
    rie_vals  = [r["RIE"]  for r in results if not np.isnan(r["RIE"])]
    psnr_vals = [r["PSNR"] for r in results if not np.isnan(r["PSNR"]) and not np.isinf(r["PSNR"])]

    if cc_vals:
        print(f"  CC   : mean = {np.mean(cc_vals):.4f},  std = {np.std(cc_vals):.4f}")
    if rie_vals:
        print(f"  RIE  : mean = {np.mean(rie_vals):.4f},  std = {np.std(rie_vals):.4f}")
    if psnr_vals:
        print(f"  PSNR : mean = {np.mean(psnr_vals):.2f} dB,  std = {np.std(psnr_vals):.2f}")
    print()
    print("  CC   (Correlation)         : Pearson r on RGB pixels  [closer to 1 = better]")
    print("  RIE  (Relative Img Error)  : ||fw-py||/||py||         [lower = better]")
    print("  PSNR (Peak SNR)            : dB                       [higher = better, >30 is good]\n")

    # ---- Visual side-by-side (one row per image in the folder) ----
    n_plots = len(results)
    fig, axes = plt.subplots(n_plots, 2, figsize=(10, 4 * n_plots), squeeze=False)
    fig.suptitle("Firmware LBP  vs  Python LBP  (same colourmap)", fontsize=14, y=1.01)

    for idx, r in enumerate(results):
        fw_rgb = r.get("fw_rgb_best", load_firmware_bmp_rgb(
            os.path.join(FIRMWARE_IMG_DIR, r["file"])))
        fw_h = fw_rgb.shape[0]

        # Left: firmware image (raw BMP pixels)
        ax = axes[idx, 0]
        ax.imshow(fw_rgb, origin="lower",
                  extent=[-GRID_EXTENT, GRID_EXTENT]*2)
        ax.add_artist(Circle((0, 0), 1, color="w", fill=False, lw=1.5))
        title = f"FW: {r['file']} [{r['orient']}]"
        if not np.isnan(r["CC"]):
            title += f"\nCC={r['CC']:.3f}  RIE={r['RIE']:.3f}  PSNR={r['PSNR']:.1f}dB"
        ax.set_title(title, fontsize=9)
        ax.set_aspect("equal")
        ax.set_xlim(-1.3, 1.3); ax.set_ylim(-1.3, 1.3)

        # Right: Python reference (same colourmap)
        ax2 = axes[idx, 1]
        py_img = python_reference_lbp(
            r["stem"], S, n_inj, n_meas, U_ref, mask_s)
        if py_img is not None:
            py_rgb = firmware_colourmap(py_img, mask_s, fw_h)
            ax2.imshow(py_rgb, origin="lower",
                       extent=[-GRID_EXTENT, GRID_EXTENT]*2)
            ax2.add_artist(Circle((0, 0), 1, color="w", fill=False, lw=1.5))
            ax2.set_title(f"Python LBP: {r['stem']}", fontsize=9)
        else:
            ax2.text(0.5, 0.5, "No .mat data", transform=ax2.transAxes, ha="center")
            ax2.set_title(f"Python LBP: {r['stem']}", fontsize=9)
        ax2.set_aspect("equal")
        ax2.set_xlim(-1.3, 1.3); ax2.set_ylim(-1.3, 1.3)

    plt.tight_layout()
    out_png = os.path.join(SCRIPT_DIR, "firmware_validation_results.png")
    plt.savefig(out_png, dpi=150, bbox_inches="tight")
    print(f"Saved plot   : {out_png}")

    # ---- JSON report ----
    out_json = os.path.join(SCRIPT_DIR, "firmware_validation_results.json")
    jdata = []
    for r in results:
        jr = {k: (None if isinstance(v, float) and np.isnan(v) else v)
              for k, v in r.items() if k != "fw_rgb_best"}
        jdata.append(jr)
    with open(out_json, "w") as f:
        json.dump(jdata, f, indent=2)
    print(f"Saved report : {out_json}")
    plt.show()


if __name__ == "__main__":
    run_validation()
