// Live measurement.
//
// This is what makes the thing a laboratory rather than a screensaver, so the
// plots are not decoration and they are not smoothed. A sparkline per observable,
// autoscaled, with the current value in tabular figures beside it.
//
// The most important one is the colony's D. It is the box-counting dimension of
// the colony, computed from scratch every twenty-four generations, and when you
// starve the colony you can watch it walk down from 2.0 to about 1.71 — which is
// the number Witten and Sander derived for diffusion-limited aggregation and the
// number Fujikawa and Matsushita measured on a real Petri dish. Watching a
// simulation converge on a value that somebody else obtained from actual
// bacteria is a different experience from watching a pretty pattern.

const N = 240;

export class Plots {
  constructor(root) {
    this.root = root;
    this.series = [];
  }

  setup(names) {
    this.root.innerHTML = '';
    this.series = names.map((name) => {
      const wrap = document.createElement('div');
      wrap.className = 'plot';

      const head = document.createElement('div');
      head.className = 'plot-head';
      const k = document.createElement('span');
      k.className = 'pk';
      k.textContent = name;
      const v = document.createElement('span');
      v.className = 'pv';
      v.textContent = '—';
      head.append(k, v);

      const cv = document.createElement('canvas');
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      cv.width = 296 * dpr;
      cv.height = 34 * dpr;

      wrap.append(head, cv);
      this.root.appendChild(wrap);

      return { name, cv, ctx: cv.getContext('2d'), val: v, data: [], dpr };
    });
  }

  push(values) {
    for (let i = 0; i < this.series.length; i++) {
      const s = this.series[i];
      const v = values[i];
      s.data.push(v);
      if (s.data.length > N) s.data.shift();
      s.val.textContent = format(v);
    }
  }

  draw() {
    for (const s of this.series) {
      const { ctx, cv, data, dpr } = s;
      const w = cv.width;
      const h = cv.height;
      ctx.clearRect(0, 0, w, h);
      if (data.length < 2) continue;

      let lo = Infinity;
      let hi = -Infinity;
      for (const v of data) {
        if (v < lo) lo = v;
        if (v > hi) hi = v;
      }
      // A flat line should sit in the middle and look flat, not get autoscaled
      // into meaningless noise.
      if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
      const pad = (hi - lo) * 0.12;
      lo -= pad;
      hi += pad;

      const x = (i) => (i / (N - 1)) * w;
      const y = (v) => h - ((v - lo) / (hi - lo)) * h;

      // the fill under the trace, very faint
      ctx.beginPath();
      ctx.moveTo(x(0), h);
      for (let i = 0; i < data.length; i++) ctx.lineTo(x(i), y(data[i]));
      ctx.lineTo(x(data.length - 1), h);
      ctx.closePath();
      ctx.fillStyle = 'rgba(43,43,43,0.07)';
      ctx.fill();

      ctx.beginPath();
      for (let i = 0; i < data.length; i++) {
        const px = x(i);
        const py = y(data[i]);
        if (i === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      }
      ctx.strokeStyle = '#2b2b2b';
      ctx.lineWidth = 1.25 * dpr;
      ctx.lineJoin = 'round';
      ctx.stroke();
    }
  }

  clear() {
    for (const s of this.series) s.data = [];
  }
}

function format(v) {
  if (!Number.isFinite(v)) return '—';
  const a = Math.abs(v);
  if (a === 0) return '0';
  if (a >= 100000) return v.toExponential(2);
  if (a >= 1000) return Math.round(v).toLocaleString('en-US');
  if (a >= 10) return v.toFixed(1);
  if (a >= 1) return v.toFixed(2);
  return v.toFixed(4);
}
