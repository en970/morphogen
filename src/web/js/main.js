import createMorphogen from '../morphogen.js';
import { Halftone } from './halftone.js';
import { makeRow, fmt } from './rail.js';
import { Plots } from './plots.js';
import { INFO } from './models.js';

// ── state ──────────────────────────────────────────────────────────────────

const style = {
  paper: '#f2f1e8',
  pitch: 1.0,      // one dot per cell
  radius: 1.15,
  contrast: 0.35,
  hex: 1,
  gooey: 0,
  grainMixer: 0.18,
  grainOverlay: 0.22,
  grainSize: 1.6,
};

const STYLE_DEFS = [
  { id: 'grid', kind: 2, opts: 'hex|square', min: 0, max: 1, step: 1, key: 'hex',
    help: 'the halftone lattice. a hex screen is what a press would use; a square one lines the dots up with the cells.' },
  { id: 'dots', kind: 2, opts: 'classic|gooey', min: 0, max: 1, step: 1, key: 'gooey',
    help: 'classic keeps every cell legible. gooey lets neighbouring dots merge like wet ink, at the cost of being able to read individual cells.' },
  { id: 'radius', kind: 0, min: 0.3, max: 2.0, step: 0.05, key: 'radius',
    help: 'dot size. above about 1.4 the dots touch and the halftone closes up into solid ink.' },
  { id: 'contrast', kind: 0, min: 0, max: 1, step: 0.01, key: 'contrast',
    help: 'a sigmoid on the cell value before it becomes a radius. wound up, the halftone becomes a threshold.' },
  { id: 'pitch', kind: 0, min: 0.5, max: 4, step: 0.1, key: 'pitch',
    help: 'screen pitch, in cells per dot. above 1 the screen is coarser than the simulation and starts to alias, which is exactly what a real halftone does.' },
  { id: 'grainMixer', kind: 0, min: 0, max: 1, step: 0.01, key: 'grainMixer',
    help: 'noise on the edges of the dots themselves: ink soaking into the tooth of the paper.' },
  { id: 'grainOverlay', kind: 0, min: 0, max: 1, step: 0.01, key: 'grainOverlay',
    help: 'noise over the whole sheet: the grain of the film you photographed it with.' },
];

const app = {
  M: null,
  models: [],       // the catalogue, as declared by the C
  mi: 0,
  running: true,
  speed: 1,
  seed: 1n,
  view: { zoom: 2, panX: 0, panY: 0 },
  grid: { w: 0, h: 0 },
  fps: 0,
  brush: 4,
};

// ── boot ───────────────────────────────────────────────────────────────────

const canvas = document.getElementById('sim');
const gl = new Halftone(canvas);
const plots = new Plots(document.getElementById('plots'));

const Module = await createMorphogen();

const call = {
  boot: Module._mg_boot,
  select: Module._mg_select,
  reset: Module._mg_reset,
  step: Module._mg_step,
  render: Module._mg_render,
  inkPtr: Module._mg_ink_ptr,
  obsPtr: Module._mg_obs_ptr,
  setParam: Module._mg_set_param,
  paramPtr: Module._mg_param_ptr,
  width: Module._mg_width,
  height: Module._mg_height,
  gen: Module._mg_gen,
  paint: Module._mg_paint,
  meta: Module._mg_meta,
};

if (!call.boot()) throw new Error('could not allocate the simulation arena');
app.models = JSON.parse(Module.UTF8ToString(call.meta()));

// ── the run ────────────────────────────────────────────────────────────────

function model() { return app.models[app.mi]; }

// The seed is split into two 32-bit halves on the way across, because a JS
// number cannot hold a 64-bit integer exactly and quietly rounding the seed
// would break the one promise this whole thing makes: that a link reproduces a
// run.
function seedParts() {
  return [Number(app.seed & 0xffffffffn), Number((app.seed >> 32n) & 0xffffffffn)];
}

function selectModel(i, { keepParams = false, params = null } = {}) {
  app.mi = i;
  const m = model();
  const [lo, hi] = seedParts();

  call.select(i, m.w, m.h, lo, hi);
  app.grid = { w: call.width(), h: call.height() };

  if (params) applyParams(params);
  if (params || keepParams) call.reset(app.grid.w, app.grid.h, lo, hi);

  fitView();
  buildRail();
  plots.setup(m.obs);
  plots.clear();
  paintFrame();
}

function reseed(newSeed) {
  if (newSeed !== undefined) app.seed = newSeed;
  const [lo, hi] = seedParts();
  call.reset(app.grid.w, app.grid.h, lo, hi);
  plots.clear();
  document.getElementById('seed').value = app.seed.toString(16);
  paintFrame();
}

function params() {
  const p = call.paramPtr() >> 2;
  return Module.HEAPF32.subarray(p, p + 20);
}

function applyParams(obj) {
  const defs = model().params;
  for (const [k, v] of Object.entries(obj)) {
    const i = defs.findIndex((d) => d.id === k);
    if (i >= 0) call.setParam(i, v);
  }
}

// The ink buffer is a view straight into the WebAssembly heap. It is never
// copied: C writes the coverage of each ink into it, and the same bytes go to
// the GPU.
function paintFrame() {
  call.render();
  const ptr = call.inkPtr();
  const n = app.grid.w * app.grid.h * 4;
  const pixels = Module.HEAPU8.subarray(ptr, ptr + n);
  gl.upload(pixels, app.grid.w, app.grid.h);
  gl.draw(app.view, style, model().inks, app.grid);
}

let last = performance.now();
let frames = 0;
let fpsAcc = 0;

function frame(now) {
  const dt = now - last;
  last = now;
  fpsAcc += dt;
  frames++;
  if (fpsAcc > 500) {
    app.fps = (frames * 1000) / fpsAcc;
    fpsAcc = 0;
    frames = 0;
  }

  if (app.running) {
    // A wall-clock budget, not a step count. If somebody sets the speed to sixty
    // generations a frame on a model that cannot manage it, the simulation slows
    // down — it does not lock up the browser and take the interface with it.
    const deadline = performance.now() + 10;
    let done = 0;
    while (done < app.speed) {
      call.step(1);
      done++;
      if (performance.now() > deadline) break;
    }
    readObservables();
  }

  paintFrame();
  plots.draw();
  updateStatus();
  requestAnimationFrame(frame);
}

function readObservables() {
  const m = model();
  const p = call.obsPtr() >> 2;
  const o = Module.HEAPF32.subarray(p, p + m.obs.length);
  plots.push(Array.from(o));
}

// ── status strip ───────────────────────────────────────────────────────────

const $gen = document.getElementById('st-gen');
const $obs = document.getElementById('st-obs');
const $fps = document.getElementById('st-fps');
const $grid = document.getElementById('st-grid');

function updateStatus() {
  const m = model();
  $gen.textContent = Math.round(call.gen()).toLocaleString('en-US');
  $fps.textContent = app.fps.toFixed(0);
  $grid.textContent = `${app.grid.w}×${app.grid.h}`;

  const p = call.obsPtr() >> 2;
  const o = Module.HEAPF32.subarray(p, p + m.obs.length);
  // Two observables in the strip; the rest live in the readout, so the strip
  // stays a strip.
  let html = '';
  for (let i = 0; i < Math.min(2, m.obs.length); i++) {
    html += `<span class="k">${m.obs[i]}</span> <span class="v">${short(o[i])}</span> `;
  }
  $obs.innerHTML = html;
}

function short(v) {
  if (!Number.isFinite(v)) return '—';
  const a = Math.abs(v);
  if (a >= 10000) return `${(v / 1000).toFixed(1)}k`;
  if (a >= 100) return String(Math.round(v));
  if (a >= 1) return v.toFixed(2);
  return v.toFixed(3);
}

// ── the rail ───────────────────────────────────────────────────────────────

const $params = document.getElementById('params');
const $styleParams = document.getElementById('style-params');
const $recipes = document.getElementById('recipes');
const $select = document.getElementById('model-select');
const $tagline = document.getElementById('tagline');

for (const [i, m] of app.models.entries()) {
  const op = document.createElement('option');
  op.value = String(i);
  op.textContent = m.name;
  $select.appendChild(op);
}
$select.addEventListener('change', () => {
  selectModel(parseInt($select.value, 10));
  writeHash();
});

let rows = [];

function buildRail() {
  const m = model();
  $select.value = String(app.mi);
  const info = INFO[m.id] || {};
  $tagline.textContent = info.tagline || '';

  $params.innerHTML = '';
  rows = m.params.map((def, i) => {
    const row = makeRow(
      def,
      () => params()[i],
      (v) => { call.setParam(i, v); onParamChanged(def); },
    );
    $params.appendChild(row);
    return row;
  });

  $recipes.innerHTML = '';
  for (const r of info.try || []) {
    const b = document.createElement('button');
    b.className = 'recipe';
    b.innerHTML = `${escapeHtml(r.label)}<span class="rn">${escapeHtml(squash(r.note))}</span>`;
    b.addEventListener('click', () => {
      applyParams(r.params || {});
      // Recipes that change the initial condition have to restart the run;
      // ones that only change the dynamics can be dropped into a live system,
      // which is often the more interesting thing to watch.
      if (needsReset(r.params || {})) reseed();
      syncRows();
      writeHash();
      app.running = true;
      updatePlayButton();
    });
    $recipes.appendChild(b);
  }

  if (!$styleParams.childElementCount) buildStyleRail();
}

// Which parameters only bite at initialisation? Changing the feed rate of a
// running Gray-Scott is a real experiment; changing the initial soup density is
// not, unless you restart.
const INIT_ONLY = new Set(['density', 'init', 'n0', 'empty', 'ratio', 'pop0',
  'maxVision', 'maxMetab', 'rule', 'ants', 'R', 'beta', 'core']);

function needsReset(obj) {
  return Object.keys(obj).some((k) => INIT_ONLY.has(k));
}

function onParamChanged(def) {
  if (def.id === 'R' || def.id === 'beta' || def.id === 'core') {
    // Lenia rebuilds its kernel when these change, which it does on the next
    // step by itself; nothing to do here.
  }
  writeHash();
}

function buildStyleRail() {
  $styleParams.innerHTML = '';
  for (const def of STYLE_DEFS) {
    const row = makeRow(
      def,
      () => style[def.key],
      (v) => { style[def.key] = v; writeHash(); },
    );
    $styleParams.appendChild(row);
  }
}

function syncRows() {
  for (const r of rows) r.sync();
}

function escapeHtml(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}
function squash(s) {
  return String(s).replace(/\s+/g, ' ').trim();
}

// ── the info drawer ────────────────────────────────────────────────────────

const $railBody = document.getElementById('rail-body');
const $info = document.getElementById('info');

function showInfo() {
  const m = model();
  const info = INFO[m.id] || {};
  document.getElementById('info-title').textContent = m.name;
  document.getElementById('info-what').innerHTML = prose(info.what);
  document.getElementById('info-how').innerHTML = prose(info.how);
  const refs = document.getElementById('info-refs');
  refs.innerHTML = '';
  for (const r of info.refs || []) {
    const li = document.createElement('li');
    li.textContent = r;
    refs.appendChild(li);
  }
  $railBody.hidden = true;
  $info.hidden = false;
}

function prose(text) {
  if (!text) return '';
  return squash(text)
    .split('\\n')
    .map((s) => s.trim())
    .filter(Boolean)
    .map((p) => (looksLikeMaths(p) ? `<span class="eq">${escapeHtml(p)}</span>` : `<p>${escapeHtml(p)}</p>`))
    .join('');
}
function looksLikeMaths(p) {
  return /[∇←∗≈²·]|d[uv]\/dt|>>/.test(p) && p.length < 160;
}

document.getElementById('btn-info').addEventListener('click', showInfo);
document.getElementById('btn-back').addEventListener('click', () => {
  $info.hidden = true;
  $railBody.hidden = false;
});

// ── view: zoom, pan, brush ─────────────────────────────────────────────────

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  canvas.width = Math.floor(window.innerWidth * dpr);
  canvas.height = Math.floor(window.innerHeight * dpr);
}

function fitView() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  // Fit the grid to the window, then back off a little so the dish has a margin
  // and you can see that it has an edge.
  const zx = (window.innerWidth * dpr) / app.grid.w;
  const zy = (window.innerHeight * dpr) / app.grid.h;
  app.view.zoom = Math.min(zx, zy) * 0.86;
  app.view.panX = app.grid.w / 2;
  app.view.panY = app.grid.h / 2;
}

window.addEventListener('resize', () => { resize(); });
resize();

// screen -> cell
function cellAt(ev) {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const fx = ev.clientX * dpr;
  const fy = (window.innerHeight - ev.clientY) * dpr;
  const wx = (fx - canvas.width / 2) / app.view.zoom + app.view.panX;
  const wy = (fy - canvas.height / 2) / app.view.zoom + app.view.panY;
  return { x: Math.floor(wx), y: Math.floor(app.grid.h - wy) };
}

let painting = 0;
let panning = false;
let panStart = null;

canvas.addEventListener('pointerdown', (e) => {
  canvas.setPointerCapture(e.pointerId);
  if (e.button === 1 || e.altKey) {
    panning = true;
    panStart = { x: e.clientX, y: e.clientY, panX: app.view.panX, panY: app.view.panY };
    return;
  }
  painting = e.button === 2 ? 2 : 1;
  brushAt(e);
});
canvas.addEventListener('pointermove', (e) => {
  if (panning && panStart) {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    app.view.panX = panStart.panX - ((e.clientX - panStart.x) * dpr) / app.view.zoom;
    app.view.panY = panStart.panY + ((e.clientY - panStart.y) * dpr) / app.view.zoom;
    return;
  }
  if (painting) brushAt(e);
});
const stopPointer = (e) => {
  painting = 0;
  panning = false;
  try { canvas.releasePointerCapture(e.pointerId); } catch (_) { /* gone */ }
};
canvas.addEventListener('pointerup', stopPointer);
canvas.addEventListener('pointercancel', stopPointer);
canvas.addEventListener('contextmenu', (e) => e.preventDefault());

function brushAt(e) {
  const { x, y } = cellAt(e);
  if (x < 0 || y < 0 || x >= app.grid.w || y >= app.grid.h) return;
  call.paint(x, y, app.brush, painting === 2 ? 1 : 0);
  if (!app.running) paintFrame();
}

canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const before = cellAt(e);
  const k = Math.exp(-e.deltaY * 0.0016);
  app.view.zoom = Math.max(0.4, Math.min(64, app.view.zoom * k));
  const after = cellAt(e);
  // zoom about the cursor: keep the cell under the pointer under the pointer
  app.view.panX += before.x - after.x;
  app.view.panY -= before.y - after.y;
  void dpr;
}, { passive: false });

// ── controls ───────────────────────────────────────────────────────────────

const $play = document.getElementById('btn-play');
function updatePlayButton() { $play.textContent = app.running ? '❚❚' : '▶'; }

$play.addEventListener('click', () => { app.running = !app.running; updatePlayButton(); });
document.getElementById('btn-step').addEventListener('click', () => {
  app.running = false;
  updatePlayButton();
  call.step(1);
  readObservables();
  paintFrame();
});
document.getElementById('btn-reset').addEventListener('click', () => reseed());
document.getElementById('btn-reseed').addEventListener('click', () => {
  reseed(BigInt(Math.floor(Math.random() * 0xffffffff)) | (BigInt(Math.floor(Math.random() * 0xffffffff)) << 32n));
  writeHash();
});

const $seed = document.getElementById('seed');
$seed.addEventListener('change', () => {
  try {
    app.seed = BigInt(`0x${$seed.value.replace(/[^0-9a-f]/gi, '') || '1'}`);
  } catch (_) { app.seed = 1n; }
  reseed();
  writeHash();
});

const $speed = document.getElementById('speed');
const $speedV = document.getElementById('speed-v');
$speed.addEventListener('input', () => {
  app.speed = parseInt($speed.value, 10);
  $speedV.textContent = String(app.speed);
});

const $readout = document.getElementById('readout');
const togglePlots = () => { $readout.hidden = !$readout.hidden; };
document.getElementById('btn-plots').addEventListener('click', togglePlots);
document.getElementById('btn-plots-close').addEventListener('click', togglePlots);

const $rail = document.getElementById('rail');
const $showRail = document.getElementById('btn-show');
const toggleRail = () => {
  const gone = $rail.classList.toggle('gone');
  $showRail.hidden = !gone;
};
document.getElementById('btn-hide').addEventListener('click', toggleRail);
$showRail.addEventListener('click', toggleRail);

document.getElementById('btn-link').addEventListener('click', async () => {
  writeHash();
  try {
    await navigator.clipboard.writeText(location.href);
    toast('link copied — it reproduces this exact run');
  } catch (_) {
    toast(location.href);
  }
});

document.getElementById('btn-png').addEventListener('click', savePng);

function savePng() {
  paintFrame();
  canvas.toBlob((blob) => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = `morphogen-${model().id}-${Math.round(call.gen())}.png`;
    a.click();
    URL.revokeObjectURL(a.href);
  });
}

const $toast = document.getElementById('toast');
let toastTimer = 0;
function toast(msg) {
  $toast.textContent = msg;
  $toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { $toast.hidden = true; }, 2200);
}

window.addEventListener('keydown', (e) => {
  // Never swallow a keystroke that was meant for a text field: typing B3/S23
  // into the rule box should not pause the simulation and reset it.
  if (e.target.matches('input, select, textarea')) return;
  switch (e.key) {
    case ' ': e.preventDefault(); app.running = !app.running; updatePlayButton(); break;
    case '.': app.running = false; updatePlayButton(); call.step(1); readObservables(); paintFrame(); break;
    case 'r': reseed(); break;
    case 'R': reseed(BigInt(Math.floor(Math.random() * 0xffffffff))); writeHash(); break;
    case 'p': togglePlots(); break;
    case 'i': $info.hidden ? showInfo() : (($info.hidden = true), ($railBody.hidden = false)); break;
    case 'f': toggleRail(); break;
    case 's': savePng(); break;
    case '[': app.brush = Math.max(0, app.brush - 1); toast(`brush ${app.brush}`); break;
    case ']': app.brush = Math.min(40, app.brush + 1); toast(`brush ${app.brush}`); break;
    default: break;
  }
});

// ── the address bar is the state ───────────────────────────────────────────
//
// Not a picture of a run: the run. Model, seed, every parameter and the print
// settings all go into the fragment, and a seeded PRNG in the C means that
// loading them back gives you the same cells in the same places at the same
// generation. As far as I can tell no other cellular-automata site on the web
// does this, and it is the single feature I would keep if I had to drop all the
// others.

let hashTimer = 0;
function writeHash() {
  clearTimeout(hashTimer);
  hashTimer = setTimeout(() => {
    const m = model();
    const p = params();
    const parts = [`m=${m.id}`, `seed=${app.seed.toString(16)}`, `speed=${app.speed}`];
    m.params.forEach((d, i) => {
      const v = p[i];
      if (Math.abs(v - d.def) > 1e-9) parts.push(`${d.id}=${trim(v)}`);
    });
    for (const d of STYLE_DEFS) {
      const v = style[d.key];
      parts.push(`${d.id}=${trim(v)}`);
    }
    history.replaceState(null, '', `#${parts.join('&')}`);
  }, 200);
}

function trim(v) {
  return String(Math.round(v * 1000) / 1000);
}

function readHash() {
  const h = location.hash.slice(1);
  if (!h) return null;
  const kv = {};
  for (const part of h.split('&')) {
    const [k, v] = part.split('=');
    if (k) kv[k] = v;
  }
  return kv;
}

// ── go ─────────────────────────────────────────────────────────────────────

function boot() {
  const kv = readHash();
  let mi = 0;
  let ps = null;

  if (kv) {
    if (kv.m) {
      const i = app.models.findIndex((m) => m.id === kv.m);
      if (i >= 0) mi = i;
    }
    if (kv.seed) {
      try { app.seed = BigInt(`0x${kv.seed}`); } catch (_) { /* keep the default */ }
    }
    if (kv.speed) {
      const s = parseInt(kv.speed, 10);
      if (Number.isFinite(s) && s >= 1) {
        app.speed = Math.min(60, s);
        $speed.value = String(app.speed);
        $speedV.textContent = String(app.speed);
      }
    }
    for (const d of STYLE_DEFS) {
      if (kv[d.id] !== undefined) {
        const v = parseFloat(kv[d.id]);
        if (Number.isFinite(v)) style[d.key] = v;
      }
    }
    ps = {};
    for (const d of app.models[mi].params) {
      if (kv[d.id] !== undefined) {
        const v = parseFloat(kv[d.id]);
        if (Number.isFinite(v)) ps[d.id] = v;
      }
    }
    if (!Object.keys(ps).length) ps = null;
  }

  $seed.value = app.seed.toString(16);
  selectModel(mi, { params: ps });
  buildStyleRail();
  updatePlayButton();
  requestAnimationFrame(frame);
}

boot();
