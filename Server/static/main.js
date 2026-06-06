// EIT Live Dashboard – main.js
// Handles SSE connection, heatmap rendering, exam replay

// ── Colormap (Viridis, 256 entries sampled) ──────────────────────────────────
const VIRIDIS_DATA = [
  [68,1,84],[68,2,86],[69,4,87],[69,5,89],[70,7,90],[70,8,92],[70,10,93],[70,11,94],
  [71,13,96],[71,14,97],[71,16,99],[71,17,100],[71,19,101],[72,20,103],[72,22,104],[72,23,105],
  [72,24,106],[72,26,108],[72,27,109],[72,28,110],[72,30,111],[72,31,112],[72,32,113],[72,34,115],
  [72,35,116],[72,36,117],[72,38,118],[72,39,119],[72,40,120],[72,42,121],[72,43,122],[72,44,123],
  [72,46,124],[72,47,125],[72,48,126],[72,50,127],[72,51,128],[72,52,129],[72,54,130],[72,55,131],
  [72,56,132],[72,58,133],[72,59,134],[72,60,134],[72,62,135],[72,63,136],[72,64,137],[72,66,137],
  [72,67,138],[72,68,139],[72,70,140],[72,71,141],[71,72,141],[71,74,142],[71,75,143],[71,76,143],
  [71,78,144],[71,79,145],[70,80,145],[70,82,146],[70,83,147],[70,84,147],[70,86,148],[69,87,148],
  [69,88,149],[69,90,150],[68,91,150],[68,92,151],[68,94,151],[67,95,152],[67,96,152],[67,98,153],
  [66,99,153],[66,100,154],[66,102,154],[65,103,154],[65,104,155],[64,106,155],[64,107,156],[64,108,156],
  [63,110,156],[63,111,157],[62,112,157],[62,114,157],[61,115,158],[61,116,158],[60,118,158],[60,119,159],
  [59,120,159],[59,122,159],[58,123,159],[58,124,160],[57,126,160],[57,127,160],[56,128,160],[56,130,161],
  [55,131,161],[55,132,161],[54,134,161],[54,135,161],[53,136,162],[53,138,162],[52,139,162],[52,140,162],
  [51,142,162],[51,143,162],[50,144,162],[50,146,163],[49,147,163],[49,148,163],[48,150,163],[48,151,163],
  [47,152,163],[47,154,163],[46,155,163],[46,156,163],[45,158,163],[45,159,163],[44,160,163],[44,162,163],
  [43,163,163],[43,164,163],[42,166,163],[42,167,163],[41,169,163],[41,170,163],[40,171,163],[40,173,163],
  [39,174,163],[39,175,162],[38,177,162],[38,178,162],[37,180,162],[37,181,162],[36,182,161],[36,184,161],
  [35,185,161],[35,186,161],[34,188,160],[34,189,160],[34,190,160],[33,192,159],[33,193,159],[32,195,159],
  [32,196,158],[32,197,158],[31,199,157],[31,200,157],[30,201,157],[30,203,156],[30,204,156],[29,205,155],
  [29,207,155],[29,208,154],[28,209,154],[28,211,153],[28,212,153],[28,213,152],[28,215,152],[28,216,151],
  [28,217,151],[28,219,150],[28,220,150],[28,221,149],[28,223,148],[28,224,148],[28,225,147],[28,227,147],
  [28,228,146],[29,229,145],[29,231,145],[29,232,144],[30,233,143],[30,235,143],[31,236,142],[31,237,141],
  [32,239,141],[32,240,140],[33,241,139],[34,243,138],[35,244,138],[36,245,137],[37,247,136],[38,248,135],
  [40,249,135],[41,251,134],[42,252,133],[44,253,132],[46,254,132],[47,255,131],[49,255,130],[51,255,129],
  [53,255,128],[55,254,127],[57,254,126],[59,253,125],[61,253,124],[63,252,123],[65,252,122],[67,251,121],
  [69,251,120],[72,250,119],[74,250,118],[76,249,117],[78,248,116],[80,248,115],[83,247,114],[85,246,112],
  [87,246,111],[90,245,110],[92,244,109],[95,244,108],[97,243,107],[99,242,106],[102,242,104],[104,241,103],
  [107,240,102],[109,240,101],[112,239,99],[114,238,98],[117,237,97],[120,237,96],[122,236,94],[125,235,93],
  [127,234,92],[130,233,90],[133,233,89],[135,232,88],[138,231,86],[141,230,85],[143,229,84],[146,228,82],
  [149,227,81],[151,226,79],[154,225,78],[157,224,76],[159,224,75],[162,223,73],[165,222,72],[167,221,70],
  [170,220,68],[173,219,67],[175,218,65],[178,217,63],[181,216,62],[183,215,60],[186,214,58],[189,212,57],
  [191,211,55],[194,210,53],[197,209,51],[199,208,50],[202,207,48],[205,205,46],[207,204,44],[210,203,42],
  [213,202,41],[215,200,39],[218,199,37],[221,197,35],[223,196,33],[226,195,31],[229,193,29],[231,192,28],
  [234,190,26],[236,189,24],[239,187,22],[241,186,20],[244,184,18],[246,183,16],[249,181,14],[251,180,12],
  [253,178,11],[254,177,9],[254,175,8],[254,174,6],[254,172,5],[254,170,4],[254,169,4],[254,167,3],
  [253,165,3],[253,163,3],[253,162,3],[252,160,3],[252,158,4],[252,156,4],[251,155,5],[251,153,6]
];

function viridis(t) {
  const i = Math.max(0, Math.min(255, Math.round(t * 255)));
  return VIRIDIS_DATA[i] || [0, 0, 0];
}

// ── Canvas helpers ────────────────────────────────────────────────────────────
function renderPixels(canvas, pixels, imageSize) {
  if (!pixels || pixels.length === 0) return;
  const sz = imageSize || Math.round(Math.sqrt(pixels.length));
  canvas.width  = sz;
  canvas.height = sz;

  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(sz, sz);
  for (let i = 0; i < sz * sz; i++) {
    const v = isNaN(pixels[i]) ? 0 : Math.max(0, Math.min(1, pixels[i]));
    const [r, g, b] = viridis(v);
    img.data[4 * i]     = r;
    img.data[4 * i + 1] = g;
    img.data[4 * i + 2] = b;
    img.data[4 * i + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function renderColorbar(canvas) {
  canvas.width  = 256;
  canvas.height = 1;
  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(256, 1);
  for (let i = 0; i < 256; i++) {
    const [r, g, b] = viridis(i / 255);
    img.data[4 * i]     = r;
    img.data[4 * i + 1] = g;
    img.data[4 * i + 2] = b;
    img.data[4 * i + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

// ── Log ───────────────────────────────────────────────────────────────────────
const logEl = document.getElementById('log');
function log(msg, cls = '') {
  const p = document.createElement('p');
  if (cls) p.className = cls;
  p.textContent = `${new Date().toLocaleTimeString()}  ${msg}`;
  logEl.prepend(p);
  while (logEl.children.length > 80) logEl.lastChild.remove();
}

// ── DOM refs ──────────────────────────────────────────────────────────────────
const badgeEl       = document.getElementById('status-badge');
const connLabel     = document.getElementById('conn-label');
const statFps       = document.getElementById('stat-fps');
const statFrames    = document.getElementById('stat-frames');
const statImgSize   = document.getElementById('stat-image-size');
const statExamFr    = document.getElementById('stat-exam-frames');
const heatmapCanvas = document.getElementById('heatmap-canvas');
const colorbarCanvas= document.getElementById('colorbar');
const examPanel     = document.getElementById('exam-panel');
const examCanvas    = document.getElementById('exam-canvas');
const examSlider    = document.getElementById('exam-slider');
const examFrameLbl  = document.getElementById('exam-frame-label');

// ── Exam replay ───────────────────────────────────────────────────────────────
let examFrames = [];
let examImageSize = 32;

examSlider.addEventListener('input', () => {
  const idx = parseInt(examSlider.value, 10);
  examFrameLbl.textContent = `${idx + 1} / ${examFrames.length}`;
  const fr = examFrames[idx];
  if (fr && fr.pixels) renderPixels(examCanvas, fr.pixels, examImageSize);
});

// ── SSE ───────────────────────────────────────────────────────────────────────
let frameCount = 0;

function connectSSE() {
  const es = new EventSource('/api/events');

  es.addEventListener('open', () => {
    connLabel.textContent = 'Connected';
    log('SSE connected', 'ok');
  });

  es.addEventListener('message', (ev) => {
    let d;
    try { d = JSON.parse(ev.data); } catch { return; }

    if (d.type === 'frame') {
      frameCount++;
      statFps.textContent     = d.fps != null ? d.fps.toFixed(1) : '--';
      statFrames.textContent  = d.frame_count ?? frameCount;
      statImgSize.textContent = d.image_size ? `${d.image_size}×${d.image_size}` : '--';

      if (d.ref) {
        setBadge('streaming', '● STREAMING (ref)');
        log(`Reference frame received  f=${d.f}`, 'ok');
      } else {
        setBadge('streaming', '● LIVE');
      }

      if (d.pixels && d.pixels.length > 0) {
        renderPixels(heatmapCanvas, d.pixels, d.image_size);
      }
    }

    if (d.type === 'exam_complete') {
      setBadge('exam', '● EXAM');
      const validFrames = (d.frames || []).filter(f => f.pixels && f.pixels.length > 0);
      examFrames    = validFrames;
      examImageSize = d.image_size || 32;

      statExamFr.textContent = d.total_frames ?? examFrames.length;

      if (examFrames.length > 0) {
        examSlider.max   = examFrames.length - 1;
        examSlider.value = 0;
        examFrameLbl.textContent = `1 / ${examFrames.length}`;
        renderPixels(examCanvas, examFrames[0].pixels, examImageSize);
        examPanel.classList.add('visible');
      }

      log(`Exam received: ${d.total_frames} frame(s)`, 'ok');
    }
  });

  es.addEventListener('error', () => {
    connLabel.textContent = 'Reconnecting…';
    setBadge('waiting', 'Waiting');
    log('SSE disconnected – retrying…', 'err');
    es.close();
    setTimeout(connectSSE, 3000);
  });
}

function setBadge(type, text) {
  badgeEl.textContent = text;
  badgeEl.className = `badge badge-${type}`;
}

// ── Init ──────────────────────────────────────────────────────────────────────
renderColorbar(colorbarCanvas);
connectSSE();
log('Dashboard loaded');
