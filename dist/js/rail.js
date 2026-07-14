// The parameter panel.
//
// A row is: the identifier, a track you can drag anywhere on, and the number.
// There is no thumb, no shadow, and no border — the fill *is* the control. This
// is the Leva anatomy, hand-rolled, because pulling in a React control library
// to draw sixty rectangles would be an odd trade.
//
// Dragging is relative and unbounded: the pointer is locked to the page, so you
// can keep pulling past the edge of the track and the value keeps going until it
// hits the parameter's own limit. Values are also typeable, because sometimes
// you know that you want F = 0.026 exactly and hunting for it with a mouse is
// no fun.

const KIND_FLOAT = 0;
const KIND_INT = 1;
const KIND_ENUM = 2;

function decimals(step) {
  if (step >= 1) return 0;
  const s = String(step);
  const dot = s.indexOf('.');
  if (dot < 0) return 0;
  return Math.min(6, s.length - dot - 1);
}

export function fmt(v, def) {
  if (def.kind === KIND_INT || def.kind === KIND_ENUM) return String(Math.round(v));
  return v.toFixed(decimals(def.step));
}

export function makeRow(def, get, set) {
  const row = document.createElement('div');
  row.className = 'param';

  const label = document.createElement('label');
  label.textContent = def.id;
  label.title = def.help || def.id;
  row.appendChild(label);

  if (def.kind === KIND_ENUM) {
    const sel = document.createElement('select');
    sel.className = 'select';
    sel.title = def.help || '';
    const opts = (def.opts || '').split('|');
    opts.forEach((o, i) => {
      const op = document.createElement('option');
      op.value = String(i);
      op.textContent = o;
      sel.appendChild(op);
    });
    sel.value = String(Math.round(get()));
    sel.addEventListener('change', () => set(parseFloat(sel.value)));
    row.appendChild(sel);
    // an enum has no track, so the select spans the remaining two columns
    sel.style.gridColumn = '2 / 4';
    row.sync = () => { sel.value = String(Math.round(get())); };
    return row;
  }

  const track = document.createElement('div');
  track.className = 'track';
  track.title = def.help || '';
  const fill = document.createElement('div');
  fill.className = 'fill';
  const tick = document.createElement('div');
  tick.className = 'tick';
  track.append(fill, tick);
  row.appendChild(track);

  const num = document.createElement('input');
  num.className = 'num';
  num.spellcheck = false;
  row.appendChild(num);

  const span = def.max - def.min;

  const paint = () => {
    const v = get();
    const pct = span > 0 ? ((v - def.min) / span) * 100 : 0;
    fill.style.width = `${Math.max(0, Math.min(100, pct))}%`;
    tick.style.left = `${Math.max(0, Math.min(100, pct))}%`;
    if (document.activeElement !== num) num.value = fmt(v, def);
  };

  const clampSnap = (v) => {
    v = Math.max(def.min, Math.min(def.max, v));
    if (def.step > 0) v = def.min + Math.round((v - def.min) / def.step) * def.step;
    return Math.max(def.min, Math.min(def.max, v));
  };

  // Drag. The distance across the track maps to the full range, but the pointer
  // is captured, so overshooting the ends just pins you there rather than
  // stopping the gesture.
  let dragging = false;
  let startX = 0;
  let startV = 0;

  track.addEventListener('pointerdown', (e) => {
    dragging = true;
    startX = e.clientX;
    startV = get();
    track.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  track.addEventListener('pointermove', (e) => {
    if (!dragging) return;
    const width = track.getBoundingClientRect().width || 1;
    // shift slows the gesture down by a factor of eight, for the parameters
    // where the interesting region is very narrow — Lenia's sigma, for one
    const gain = e.shiftKey ? 0.125 : 1;
    const dv = ((e.clientX - startX) / width) * span * gain;
    set(clampSnap(startV + dv));
    paint();
  });
  const end = (e) => {
    if (!dragging) return;
    dragging = false;
    try { track.releasePointerCapture(e.pointerId); } catch (_) { /* already gone */ }
  };
  track.addEventListener('pointerup', end);
  track.addEventListener('pointercancel', end);

  num.addEventListener('change', () => {
    const v = parseFloat(num.value);
    if (Number.isFinite(v)) set(clampSnap(v));
    paint();
  });
  num.addEventListener('blur', paint);

  row.sync = paint;
  paint();
  return row;
}
