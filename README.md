# morphogen

An interactive laboratory for cellular automata and agent-based models.

**[Open the lab →](https://en970.github.io/morphogen)**  ·  **[Read the paper (PDF) →](https://en970.github.io/morphogen/morphogen.pdf)**

Ten models, from Wolfram's elementary rules to a nutrient-limited bacterial
colony, each with the parameters its authors used and the citation it came from.
The simulation cores are C, compiled to WebAssembly. Everything the models
measure — population, entropy, fractal dimension, Gini coefficient — is computed
live and plotted next to the grid, because a simulation you cannot measure is a
screensaver.

The thread running through the ten is a single question: **how does something
alive — a bacterial colony, an ecosystem, a society — find food, survive, adapt,
and organise itself, when nothing in it can see further than its own
neighbours?**

---

## What is in it

| | Model | The point |
|---|---|---|
| ★ | **Bacterial colony** | A nutrient field, and cells that eat it. Starve the colony and it branches into a fractal of dimension ≈1.7. Feed it and it fills in as a disc of dimension 2. Nothing about the cells changed. |
| ★ | **Gray–Scott** | Two chemicals, no biology, and spots that divide like cells. |
| ★ | **Lenia** | Conway's Life with every discrete thing about it dissolved. Something survives the dissolution, and it swims. |
| ★ | **Cyclic competition** | Rock, paper, scissors on a lattice. Whether all three species survive is decided by one number: how far they wander. |
| ★ | **Sugarscape** | A scrupulously fair world. Watch the Gini coefficient climb to 0.5. |
| | **Segregation** | Nobody in this model is a segregationist. The city segregates anyway. |
| | **Life-like** | Not the Game of Life — all 262,144 of them, in one branchless kernel. |
| | **Elementary CA** | Eight bits of rule. One of them is a universal computer. |
| | **Langton's ant** | Ten thousand steps of chaos, and then, for no reason anyone can state, a road. |
| | **Forest fire** | Why the world is full of power laws, when criticality is supposed to be a knife-edge nothing can balance on. |

Each model has a panel with the rule stated plainly, three or four *try this*
recipes that take you straight to the regime worth seeing, and the reference it
came out of.

---

## What makes it a laboratory

**It measures.** Every model streams observables into a common bus, plotted live.
The bacterial colony's box-counting dimension is the one to watch: starve the
colony and you can see D walk down from 2.0 towards 1.71 — which is the value
Witten and Sander derived in 1981 for diffusion-limited aggregation, from an
argument about random walkers that has nothing to do with biology, and the value
Fujikawa and Matsushita measured in 1989 on an actual plate of *Bacillus
subtilis*. Watching a simulation converge on a number somebody else got out of a
Petri dish is a different experience from watching a pretty pattern.

**It is reproducible.** Every run is a pure function of its seed and its
parameters, and all of them live in the address bar. Copy the link and you are
not sharing a picture of a run — you are sharing the run, cell for cell. The
generators are PCG32, seeded per stream; no wall-clock ever enters an update; the
speed control changes how many generations happen per frame and never what
happens in one.

**It is answerable to the literature.** These are not tolerances invented to make
the tests pass. They are numbers other people measured, and if the code stops
reproducing them then the code is wrong:

| Check | Expected | Source |
|---|---|---|
| Starved colony, box dimension | fractal, D → ~1.7 | Witten & Sander 1981; Fujikawa & Matsushita 1989 |
| Fed colony, box dimension | compact, D = 2.0 | — |
| Rule 90 from one live cell | Sierpiński triangle, D = log3/log2 = 1.585 | Wolfram 1983 |
| Conway's Life from random soup | ash density ≈ 0.03 | — |
| A glider, after 64 generations | still five cells | — |
| Sugarscape, from equal endowments | Gini ≈ 0.4–0.5 | Epstein & Axtell 1996 |
| Schelling at τ = 0.35 | segregation ≈ 0.77, from a chance baseline of 0.50 | Schelling 1971 |
| Orbium, 400 steps | alive, mass bounded and nonzero | Chan 2019 |
| Orbium with σ = 0.005 | dead | Chan 2019 |
| Rock–paper–scissors, high mobility | at least one species extinct | Reichenbach et al. 2007 |
| Forest fire | self-organises to a critical density | Drossel & Schwabl 1992 |

`make test` runs all of them natively, and so does CI on every push.

---

## The paper

[**morphogen: a browser-native laboratory for cellular automata and agent-based
models**](https://en970.github.io/morphogen/morphogen.pdf) — the write-up, in the
form it wants to be in: the architecture, the measurements, the validation against
the literature, and a section on the things that did not work.

*(That link is the copy served from the site, because GitHub's own PDF viewer
gives up on this file often enough to be annoying. The same file is in
[`paper/`](paper/) if you would rather have it from the repository.)*

Every number and every figure in it is generated by `tools/figures.c`, which links
against the same `src/core` the browser runs. There is no separate figure code
that could quietly drift away from the thing being described: if a plot in the
paper is wrong, the simulation is wrong. `make paper` rebuilds it from scratch.

The most useful section is probably the one on negative results. The first version
of the colony model let its cells wander freely, produced a diffuse cloud that was
nothing like a colony, and box-counted to D ≈ 1.71 — right on the Witten–Sander
value, and completely wrong. A scattered point cloud is fractal too. The test
passed and the physics was absent, which is worth knowing about any growth model
validated on a single scalar.

---

## Building

`src/core/` is portable C99 and has no Emscripten dependency, so the same
simulation code compiles with the system compiler and runs outside the browser.
That is the point of the layout: the models can be profiled with real tools and
checked against the literature without a browser anywhere in the loop, and
WebAssembly is just one more target.

```sh
make test     # build the core natively and check it against the literature
make          # build the wasm module and assemble dist/
make serve    # and serve it on :8000
```

Building the wasm needs [Emscripten](https://emscripten.org). The built module is
committed to `dist/`, so if you only want to run the thing or work on the
interface, `python3 -m http.server` inside `dist/` is enough and you need no
toolchain at all.

```
src/core/       the science. C99, no dependencies.
  rng.h           pcg32, and a counter-based hash for order-independent randomness
  field.c         halo-free neighbour tables and the shared diffusion kernel
  obs.c           entropy, box-counting dimension, Gini, segregation
  model.h         the interface all ten models implement
  models/         one file per model, each with its rule and its citation at the top
src/wasm/api.c  ~12 exported symbols. The boundary is crossed three times a frame.
src/web/        the interface. No framework, no bundler, no dependencies.
  js/halftone.js  the renderer: a WebGL2 halftone screen, one per ink
tests/main.c    the literature, as assertions
```

---

## Notes on the implementation

**The renderer prints rather than draws.** Each model exposes up to four *ink*
channels, and each is rendered as its own halftone screen — a lattice of dots
whose radius encodes the value — laid down at its own screen angle. That last
part is not decoration: printing two screens at the same angle collides them into
a moiré, so a press rotates them apart and the dots interleave into a rosette
instead. Cyclic competition has three species and prints as three separations.
Dot radius goes as the square root of the value, because a dot's optical density
is proportional to its area, and a linear mapping crushes the midtones into mud.

**The simulation never copies a pixel.** C owns an ink buffer inside the
WebAssembly heap; JavaScript takes a typed-array view straight into it and hands
that to the GPU. The only copy in the pipeline is the driver's upload to VRAM.
Memory growth is switched off, deliberately: a `memory.grow` silently detaches
every typed-array view held into linear memory, including that one, and the
symptom is a blank canvas with no error anywhere. Allocating once removes the
failure mode instead of papering over it.

**Two things in here are easy to get wrong, and I got both wrong first.** Lenia's
Orbium was found under the *polynomial* kernel and growth function, not the
exponential ones the paper writes down; load it under the exponential core and
the creature does not glide, it wobbles and dies (the lab offers both, so you can
watch that happen). And in the forest fire, treating every cell burning anywhere
on the map at a given moment as one fire — the obvious shortcut — silently
destroys the power law, because in the critical regime two fires are almost
always burning somewhere at once, so the counter never closes and fifteen hundred
generations report a single enormous fire. Each fire is numbered.

---

## References

The full citation for each model is in the panel, in the source file, and here.

- Ben-Jacob, E., Schochet, O., Tenenbaum, A., Cohen, I., Czirók, A. & Vicsek, T. "Generic modelling of cooperative growth patterns in bacterial colonies." *Nature* **368**, 46–49 (1994).
- Witten, T. A. & Sander, L. M. "Diffusion-limited aggregation, a kinetic critical phenomenon." *Phys. Rev. Lett.* **47**, 1400–1403 (1981).
- Fujikawa, H. & Matsushita, M. "Fractal growth of *Bacillus subtilis* on agar plates." *J. Phys. Soc. Jpn.* **58**, 3875–3878 (1989).
- Pearson, J. E. "Complex patterns in a simple system." *Science* **261**, 189–192 (1993).
- Turing, A. M. "The chemical basis of morphogenesis." *Phil. Trans. R. Soc. B* **237**, 37–72 (1952).
- Chan, B. W.-C. "Lenia — Biology of Artificial Life." *Complex Systems* **28**(3), 251–286 (2019). arXiv:1812.05433.
- Reichenbach, T., Mobilia, M. & Frey, E. "Mobility promotes and jeopardizes biodiversity in rock–paper–scissors games." *Nature* **448**, 1046–1049 (2007).
- Kerr, B., Riley, M. A., Feldman, M. W. & Bohannan, B. J. M. "Local dispersal promotes biodiversity in a real-life game of rock–paper–scissors." *Nature* **418**, 171–174 (2002).
- Epstein, J. M. & Axtell, R. *Growing Artificial Societies: Social Science from the Bottom Up.* Brookings / MIT Press (1996).
- Schelling, T. C. "Dynamic models of segregation." *J. Mathematical Sociology* **1**(2), 143–186 (1971).
- Gardner, M. "Mathematical Games: The fantastic combinations of John Conway's new solitaire game 'life'." *Scientific American* **223**(4), 120–123 (1970).
- Wolfram, S. "Statistical mechanics of cellular automata." *Rev. Mod. Phys.* **55**, 601–644 (1983).
- Cook, M. "Universality in Elementary Cellular Automata." *Complex Systems* **15**, 1–40 (2004).
- Wuensche, A. "Classifying cellular automata automatically." *Complexity* **4**(3), 47–66 (1999).
- Langton, C. G. "Studying artificial life with cellular automata." *Physica D* **22**, 120–149 (1986).
- Drossel, B. & Schwabl, F. "Self-organized critical forest-fire model." *Phys. Rev. Lett.* **69**, 1629–1632 (1992).
- Bak, P., Tang, C. & Wiesenfeld, K. "Self-organized criticality: An explanation of 1/f noise." *Phys. Rev. Lett.* **59**, 381–384 (1987).
- Clauset, A., Shalizi, C. R. & Newman, M. E. J. "Power-law distributions in empirical data." *SIAM Review* **51**, 661–703 (2009).

## Licence

MIT. The Orbium seed data is transcribed from Bert Chan's
[Lenia](https://github.com/Chakazul/Lenia) species list (MIT).
