// The sweep engine, and the phase diagrams it draws.
//
// This is the thing that separates a laboratory from an exhibit. Watching one run
// of a model tells you what that run did. Running it a hundred times across a
// parameter range and plotting the result tells you what the model *is* — where
// the transitions are, whether they are sharp, and whether the number you get out
// is the number somebody else measured.
//
// A pool of workers runs the model headlessly, one point of the parameter grid
// per job. The core has no browser dependency, so a worker can step it a few
// thousand generations with nothing attached and simply report the observables at
// the end. The figures in the paper were produced by exactly this method, but
// natively, from tools/figures.c; these are the same sweeps, in the browser, at a
// resolution chosen so they finish while you are still interested.

const MAX_WORKERS = 4; // each holds its own wasm heap; four is plenty and polite

// ── the figures ────────────────────────────────────────────────────────────
//
// Each of these regenerates a figure from the paper, which is to say from the
// source paper of the model it belongs to. They are the point of the panel: a
// person who has never heard of Witten and Sander can press one button and watch
// a fractal dimension of 1.71 assemble itself out of a hundred simulations.

export const FIGURES = {
  colony: [
    {
      id: 'D-vs-food',
      label: 'fractal dimension against food',
      note: `The colony's box-counting dimension, swept across the nutrient it was
        given. It should fall from 2.0 (a compact disc) towards 1.71 — the value
        Witten and Sander derived for diffusion-limited aggregation from an argument
        about random walkers with no biology in it, and the value Fujikawa and
        Matsushita measured on a real plate of Bacillus subtilis. Nothing about the
        cells changes across this curve. Only the food does.`,
      grid: 128,
      gens: 2200,
      reps: 1,
      x: { param: 'n0', from: 0.05, to: 0.7, steps: 12, log: true },
      metric: { name: 'D', obs: 2 },
      guides: [
        { y: 2.0, label: 'compact, D = 2' },
        { y: 1.71, label: 'DLA, D = 1.71' },
      ],
    },
  ],

  schelling: [
    {
      id: 'segregation-vs-tau',
      label: 'segregation against tolerance',
      note: `Every household needs a fraction tau of its neighbours to be like it.
        A randomly mixed city scores 0.5 by chance. Watch where the curve leaves
        that line — it is nowhere near tau = 0.5. Households perfectly content to be
        outnumbered two to one still produce a badly divided city, and that gap
        between what people want and what they get is Schelling's whole result.`,
      grid: 96,
      gens: 300,
      reps: 3,
      x: { param: 'tau', from: 0.05, to: 0.8, steps: 16 },
      metric: { name: 'segregation', obs: 0 },
      guides: [{ y: 0.5, label: 'chance (0.50)' }],
    },
  ],

  rps: [
    {
      id: 'biodiversity-vs-mobility',
      label: 'biodiversity against mobility',
      note: `The density of the rarest of the three species, against how far they
        wander. Below the transition the lattice fills with spiral waves, nobody is
        rare, and everybody lives. Above it the spirals outgrow the world, wash
        out, and the curve falls to zero: two species are gone. Kerr and colleagues
        got the same answer with real E. coli — in a flask one strain took over, on
        a plate all three persisted.`,
      // The order parameter is the density of the *rarest* species, not the
      // probability of extinction, and the difference is worth explaining because
      // the first version got it wrong.
      //
      // Extinction is a bit: the species either died or they did not. Averaging
      // bits over N seeds gives an estimate whose standard error is
      // sqrt(p(1-p)/N), which at the transition — the only part of the curve
      // anybody came to see — is about 0.16 even with ten replicates. The curve
      // came out visibly non-monotonic. That is not what the model does; it is
      // what ten coin flips do, and no reasonable number of replicates fixes it.
      //
      // The rarest species' density is a real number, and every run reports one.
      // It sits near 0.25 while all three coexist and falls to zero when the
      // spirals wash out, so it carries the same information with a fraction of
      // the variance — and it says something the bit could not, which is *how
      // close* to collapse the ecosystem is before it collapses.
      grid: 96,
      gens: 1200,
      reps: 4,
      x: { param: 'eps', from: 0.5, to: 55, steps: 11, log: true },
      metric: {
        name: 'rarest species',
        fn: (o) => Math.min(o[0], o[1], o[2]),
      },
    },
  ],

  lenia: [
    {
      id: 'niche',
      label: 'where Orbium survives',
      note: `The creature is alive only inside a thin filament of the (mu, sigma)
        plane. Outside it, one of two things happens, and both are deaths: it
        evaporates, or it blooms into structureless mush that fills the world. This
        is what people mean by the "edge of chaos", except that here it is a
        measurement rather than a slogan, and you can see how narrow it is.`,
      grid: 64,
      gens: 220,
      reps: 1,
      x: { param: 'mu', from: 0.06, to: 0.28, steps: 14 },
      y: { param: 'sigma', from: 0.004, to: 0.036, steps: 12 },
      // Alive means: still here, still bounded, still itself.
      metric: {
        name: 'alive',
        fn: (o1, o0) =>
          o1[0] > 0.35 * o0[0] && o1[0] < 3.0 * o0[0] && o1[1] < 0.3 ? 1 : 0,
      },
    },
  ],

  forestfire: [
    {
      id: 'density-vs-fp',
      label: 'critical density against lightning',
      note: `The forest fills up until it is just on the edge of percolating and
        then stays there, without anybody tuning it. Sweep the lightning rate and
        watch the density it settles on: over orders of magnitude of f, it barely
        moves. That is the "self-organised" in self-organised criticality.`,
      grid: 96,
      gens: 1200,
      reps: 2,
      x: { param: 'f', from: 0.00001, to: 0.002, steps: 10, log: true },
      metric: { name: 'tree density', obs: 0 },
    },
  ],

  grayscott: [
    {
      id: 'F-k',
      label: "Pearson's (F, k) map",
      note: `The whole of Gray-Scott lives in two numbers. Each square is a separate
        run of the reaction, coloured by how much the field varies from place to
        place — which is what a pattern is. Both uniform states, the empty dish and
        the full one, are white. Only the band between them has structure in it, and
        that band is what Pearson mapped in 1993 and lettered with Greek. Pick a dark
        square, read off its F and k, and go and set them.`,
      grid: 96,
      gens: 900,
      reps: 1,
      x: { param: 'F', from: 0.005, to: 0.08, steps: 14 },
      y: { param: 'k', from: 0.04, to: 0.07, steps: 12 },
      metric: { name: 'structure', obs: 3 },
    },
  ],

  life: [
    {
      id: 'ash-vs-density',
      label: 'ash density against initial soup',
      note: `Conway's Life, run to exhaustion from random soups of every density.
        The remarkable thing is how little the answer depends on the question: from
        almost any starting density the debris settles at about 3% alive. The rule
        has an attractor, and the initial condition mostly does not matter.`,
      grid: 128,
      gens: 500,
      reps: 3,
      x: { param: 'density', from: 0.05, to: 0.95, steps: 14 },
      metric: { name: 'final density', obs: 1 },
      guides: [{ y: 0.03, label: 'ash ≈ 0.03' }],
    },
  ],

  sugarscape: [
    {
      id: 'gini-vs-growback',
      label: 'inequality against abundance',
      note: `The Gini coefficient of the wealth distribution, against how fast the
        sugar grows back. The rules are identical and scrupulously fair at every
        point on this curve; the agents inherit nothing and cheat nobody. Abundance
        softens the outcome. It does not abolish it.`,
      grid: 96,
      gens: 250,
      reps: 2,
      x: { param: 'growback', from: 0.1, to: 4, steps: 12 },
      metric: { name: 'Gini', obs: 1 },
    },
  ],
};

// ── the pool ───────────────────────────────────────────────────────────────

export class Sweep {
  constructor(canvas, onProgress) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.onProgress = onProgress;
    this.workers = [];
    this.cancelled = false;
    this.running = false;
  }

  async ensureWorkers() {
    if (this.workers.length) return;
    const n = Math.max(1, Math.min(MAX_WORKERS, (navigator.hardwareConcurrency || 4) - 1));
    this.workers = await Promise.all(
      Array.from({ length: n }, () =>
        new Promise((resolve, reject) => {
          const w = new Worker(new URL('./sweep-worker.js', import.meta.url), { type: 'module' });
          w.onmessage = (e) => {
            if (e.data.type === 'ready') resolve(w);
          };
          w.onerror = (e) => reject(new Error(e.message || 'worker failed'));
          w.postMessage({ type: 'boot' });
        })),
    );
  }

  cancel() {
    this.cancelled = true;
  }

  /* Build the job list, hand it to the pool, and draw as the results arrive.
   * Points come back out of order — a job that lands on a fast corner of the
   * parameter space finishes first — so the plot fills in patchily, which is
   * honest and, I think, rather nice to watch. */
  async run(fig, modelMeta, mi, baseParams) {
    if (this.running) return;
    this.running = true;
    this.cancelled = false;

    await this.ensureWorkers();

    const pidx = (id) => modelMeta.params.findIndex((p) => p.id === id);
    const axis = (a) => {
      const out = [];
      for (let i = 0; i < a.steps; i++) {
        const t = a.steps === 1 ? 0 : i / (a.steps - 1);
        out.push(a.log
          ? a.from * Math.pow(a.to / a.from, t)
          : a.from + (a.to - a.from) * t);
      }
      return out;
    };

    const xs = axis(fig.x);
    const ys = fig.y ? axis(fig.y) : [null];
    const reps = fig.reps || 1;

    const jobs = [];
    let id = 0;
    for (let yi = 0; yi < ys.length; yi++) {
      for (let xi = 0; xi < xs.length; xi++) {
        for (let r = 0; r < reps; r++) {
          const sets = [];
          // Start from whatever the user has on the sliders, so a sweep explores
          // the neighbourhood of the run they are actually looking at.
          modelMeta.params.forEach((p, i) => sets.push({ i, v: baseParams[i] }));
          sets.push({ i: pidx(fig.x.param), v: xs[xi] });
          if (fig.y) sets.push({ i: pidx(fig.y.param), v: ys[yi] });

          jobs.push({
            id: id++, xi, yi,
            mi, grid: fig.grid, gens: fig.gens,
            seed: 1000 + r * 7919,
            sets, nObs: modelMeta.obs.length,
          });
        }
      }
    }

    // accumulate mean over replicates
    const sum = new Float64Array(xs.length * ys.length);
    const cnt = new Int32Array(xs.length * ys.length);
    const grid = { xs, ys, sum, cnt, fig };
    this.grid = grid;

    let next = 0;
    let done = 0;

    const reduce = (obs1, obs0) =>
      fig.metric.fn ? fig.metric.fn(obs1, obs0) : obs1[fig.metric.obs];

    await new Promise((resolve) => {
      const pump = (w) => {
        if (this.cancelled || next >= jobs.length) {
          if (done >= jobs.length || this.cancelled) resolve();
          return;
        }
        const job = jobs[next++];
        w.onmessage = (e) => {
          const m = e.data;
          if (m.type !== 'done') return;
          if (!m.error) {
            const k = job.yi * xs.length + job.xi;
            sum[k] += reduce(m.obs1, m.obs0);
            cnt[k]++;
          }
          done++;
          this.onProgress(done / jobs.length);
          this.draw();
          pump(w);
        };
        w.postMessage({ type: 'run', job });
      };
      for (const w of this.workers) pump(w);
    });

    this.running = false;
    this.draw();
  }

  // ── drawing ──────────────────────────────────────────────────────────────

  draw() {
    if (!this.grid) return;
    const { xs, ys, sum, cnt, fig } = this.grid;
    const ctx = this.ctx;
    const W = this.canvas.width;
    const H = this.canvas.height;
    const dpr = this.dpr || 1;

    const L = 44 * dpr;
    const R = 10 * dpr;
    const T = 10 * dpr;
    const B = 30 * dpr;
    const pw = W - L - R;
    const ph = H - T - B;

    ctx.clearRect(0, 0, W, H);

    const val = (xi, yi) => {
      const k = yi * xs.length + xi;
      return cnt[k] ? sum[k] / cnt[k] : null;
    };

    // value range
    let lo = Infinity;
    let hi = -Infinity;
    for (let i = 0; i < sum.length; i++) {
      if (!cnt[i]) continue;
      const v = sum[i] / cnt[i];
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    for (const g of fig.guides || []) {
      if (g.y < lo) lo = g.y;
      if (g.y > hi) hi = g.y;
    }
    if (!Number.isFinite(lo)) { lo = 0; hi = 1; }
    if (hi - lo < 1e-9) { lo -= 0.5; hi += 0.5; }

    ctx.strokeStyle = 'rgba(43,43,43,0.35)';
    ctx.fillStyle = 'rgba(43,43,43,0.55)';
    ctx.lineWidth = dpr;
    ctx.font = `${9 * dpr}px "IBM Plex Mono", monospace`;

    const fmtx = (v) => (Math.abs(v) >= 100 || (Math.abs(v) < 0.01 && v !== 0)
      ? v.toExponential(0) : String(Math.round(v * 1000) / 1000));

    if (fig.y) {
      // ── a heatmap ────────────────────────────────────────────────────────
      const cw = pw / xs.length;
      const ch = ph / ys.length;
      for (let yi = 0; yi < ys.length; yi++) {
        for (let xi = 0; xi < xs.length; xi++) {
          const v = val(xi, yi);
          const x = L + xi * cw;
          const y = T + ph - (yi + 1) * ch;
          if (v === null) {
            ctx.fillStyle = 'rgba(43,43,43,0.05)';
          } else {
            const t = (v - lo) / (hi - lo);
            ctx.fillStyle = `rgba(43,43,43,${(0.06 + 0.94 * t).toFixed(3)})`;
          }
          ctx.fillRect(x + 0.5, y + 0.5, cw - 1, ch - 1);
        }
      }
      ctx.fillStyle = 'rgba(43,43,43,0.55)';
      ctx.textAlign = 'center';
      ctx.fillText(fmtx(xs[0]), L, H - 16 * dpr);
      ctx.fillText(fmtx(xs[xs.length - 1]), L + pw, H - 16 * dpr);
      ctx.fillText(`${fig.x.param} →`, L + pw / 2, H - 4 * dpr);
      ctx.save();
      ctx.translate(11 * dpr, T + ph / 2);
      ctx.rotate(-Math.PI / 2);
      ctx.fillText(`${fig.y.param} →`, 0, 0);
      ctx.restore();
      ctx.textAlign = 'right';
      ctx.fillText(fmtx(ys[0]), L - 4 * dpr, T + ph);
      ctx.fillText(fmtx(ys[ys.length - 1]), L - 4 * dpr, T + 8 * dpr);
      return;
    }

    // ── a curve ────────────────────────────────────────────────────────────
    const px = (i) => L + (xs.length === 1 ? pw / 2 : (i / (xs.length - 1)) * pw);
    const py = (v) => T + ph - ((v - lo) / (hi - lo)) * ph;

    // axes
    ctx.beginPath();
    ctx.moveTo(L, T);
    ctx.lineTo(L, T + ph);
    ctx.lineTo(L + pw, T + ph);
    ctx.strokeStyle = 'rgba(43,43,43,0.3)';
    ctx.stroke();

    for (const g of fig.guides || []) {
      ctx.save();
      ctx.setLineDash([3 * dpr, 3 * dpr]);
      ctx.strokeStyle = 'rgba(212,87,63,0.75)';
      ctx.beginPath();
      ctx.moveTo(L, py(g.y));
      ctx.lineTo(L + pw, py(g.y));
      ctx.stroke();
      ctx.restore();
      ctx.fillStyle = 'rgba(212,87,63,0.9)';
      ctx.textAlign = 'right';
      ctx.fillText(g.label, L + pw, py(g.y) - 3 * dpr);
    }

    ctx.beginPath();
    let started = false;
    for (let i = 0; i < xs.length; i++) {
      const v = val(i, 0);
      if (v === null) { started = false; continue; }
      if (!started) { ctx.moveTo(px(i), py(v)); started = true; }
      else ctx.lineTo(px(i), py(v));
    }
    ctx.strokeStyle = '#2b2b2b';
    ctx.lineWidth = 1.4 * dpr;
    ctx.stroke();

    ctx.fillStyle = '#2b2b2b';
    for (let i = 0; i < xs.length; i++) {
      const v = val(i, 0);
      if (v === null) continue;
      ctx.beginPath();
      ctx.arc(px(i), py(v), 2 * dpr, 0, Math.PI * 2);
      ctx.fill();
    }

    ctx.fillStyle = 'rgba(43,43,43,0.55)';
    ctx.textAlign = 'center';
    ctx.fillText(fmtx(xs[0]), L, H - 16 * dpr);
    ctx.fillText(fmtx(xs[xs.length - 1]), L + pw, H - 16 * dpr);
    ctx.fillText(`${fig.x.param} →`, L + pw / 2, H - 4 * dpr);
    ctx.textAlign = 'right';
    ctx.fillText(fmtx(hi), L - 4 * dpr, T + 8 * dpr);
    ctx.fillText(fmtx(lo), L - 4 * dpr, T + ph);
    ctx.save();
    ctx.translate(11 * dpr, T + ph / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.textAlign = 'center';
    ctx.fillText(fig.metric.name, 0, 0);
    ctx.restore();
  }

  resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    this.dpr = dpr;
    const r = this.canvas.getBoundingClientRect();
    this.canvas.width = Math.max(200, r.width) * dpr;
    this.canvas.height = Math.max(140, r.height) * dpr;
    this.draw();
  }
}
