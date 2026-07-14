# morphogen — working notes

An interactive cellular-automata laboratory. C simulation cores compiled to
WebAssembly, rendered as halftone separations, measured live, and checked against
the primary literature. Live at https://en970.github.io/morphogen.

## Commands

```sh
make test     # build src/core natively and check it against the literature
make          # build the wasm module and assemble dist/
make serve    # and serve dist/ on :8000
make paper    # regenerate every figure, then typeset paper/morphogen.pdf
```

`make test` is the one that matters. It is not a self-consistency check: it
asserts numbers other people measured, several of them with actual bacteria. If
one drifts, it is a bug, not a tolerance to widen.

## Layout

```
src/core/       the science. Portable C99, no dependency on Emscripten, on
                JavaScript, or on the existence of a browser. This is deliberate:
                it means the models can be profiled with native tools, swept at
                full speed, and tested in CI with no browser in the loop.
  model.h         the interface all ten models implement
  models/         one file per model, each with its rule and its citation at the top
  obs.c           entropy, box-counting dimension, Gini, segregation
src/wasm/api.c  ~12 exported symbols. The boundary is crossed three times a frame.
src/web/        no framework, no bundler, no dependencies. Copying is the build.
tools/figures.c generates every number and figure in the paper
tests/main.c    the literature, as assertions
dist/           committed, so `python3 -m http.server` works with no toolchain
```

## Things that will bite you

**A passing metric is not a working model.** Every real bug in this project was
invisible to the tests and obvious in the picture. The colony model's first
version produced a diffuse gas of wandering cells — nothing like a colony — and
box-counted to D ≈ 1.71, exactly the Witten–Sander value the test asserts. A
scattered point cloud is fractal too. **Render it and look at it.** `tools/` dumps
PGM natively; a headless browser will screenshot the live thing.

**Do not enable `ALLOW_MEMORY_GROWTH`.** A `memory.grow` silently detaches every
typed-array view JavaScript holds into linear memory, including the one the
renderer uploads from, and the symptom is a blank canvas with no error anywhere.
The heap is allocated once, up front.

**The heap is paid for once per worker.** The sweep engine gives each worker its
own WebAssembly instance (SharedArrayBuffer is unavailable on GitHub Pages — no
COOP/COEP headers — so sharing was never an option). Keep `ARENA_BYTES` and
`INITIAL_MEMORY` tight; the largest grid any model needs is about 7 MB.

**Parameter ranges are written down in exactly one place**, the `ParamDef` table
in the model's own source file. The browser's control panel is generated from it
at run time. Do not duplicate a range into the JavaScript.

**Determinism is the product, not a nicety.** A run is a pure function of its seed
and its parameters, both of which live in the URL fragment, so a link reproduces a
run cell for cell. Therefore: no wall-clock in any update; PCG32 seeded per
stream; compact agent lists *after* a step rather than swap-removing during one
(swap-removal silently reorders the agents that have not yet acted, which makes
the run a function of the death pattern as well as the seed).

## Voice

The README, the paper, the UI text and the code comments are written like a
methods section: plain, declarative, reasoning stated. Comments explain *why* — the
physics, the failure mode being avoided — never what the next line does. No
marketing register, no feature-bullets, no AI attribution anywhere, including in
commit trailers.
