// One worker owns one WebAssembly instance and runs the simulation headlessly.
//
// No canvas, no rendering, no rAF: just step the model to the horizon and report
// the observables. The core does not know or care that nobody is watching, which
// is the whole reason it was written without a browser dependency.
//
// Each worker instantiates its own module rather than sharing one. That costs
// memory, but SharedArrayBuffer is unavailable on GitHub Pages (no COOP/COEP
// headers), so sharing was never on the table — and a private instance is the
// simpler thing anyway: no locking, no aliasing, and a crashed run takes down one
// worker instead of the pool.

import createMorphogen from '../morphogen.js';

let Module = null;
let call = null;

async function boot() {
  Module = await createMorphogen();
  call = {
    boot: Module._mg_boot,
    select: Module._mg_select,
    reset: Module._mg_reset,
    step: Module._mg_step,
    setParam: Module._mg_set_param,
    obsPtr: Module._mg_obs_ptr,
    meta: Module._mg_meta,
  };
  if (!call.boot()) throw new Error('arena');
  return JSON.parse(Module.UTF8ToString(call.meta()));
}

function obs(n) {
  const p = call.obsPtr() >> 2;
  return Array.from(Module.HEAPF32.subarray(p, p + n));
}

/* Run one point of the sweep.
 *
 * `sets` is a list of {index, value} applied after the model is selected and
 * before it is initialised, because most models read their parameters while
 * building the initial condition — which is exactly why the sweep cannot simply
 * poke a running simulation and must reset for every point. */
function runPoint(job) {
  const { mi, grid, seed, sets, gens, nObs } = job;

  call.select(mi, grid, grid, seed & 0xffffffff, 0);
  for (const s of sets) call.setParam(s.i, s.v);
  call.reset(grid, grid, seed & 0xffffffff, 0);

  call.step(1);
  const obs0 = obs(nObs);
  call.step(Math.max(0, gens - 1));
  const obs1 = obs(nObs);

  return { obs0, obs1 };
}

let models = null;

self.onmessage = async (e) => {
  const msg = e.data;

  if (msg.type === 'boot') {
    models = await boot();
    self.postMessage({ type: 'ready', models });
    return;
  }

  if (msg.type === 'run') {
    try {
      const r = runPoint(msg.job);
      self.postMessage({ type: 'done', id: msg.job.id, ...r });
    } catch (err) {
      self.postMessage({ type: 'done', id: msg.job.id, error: String(err) });
    }
  }
};
