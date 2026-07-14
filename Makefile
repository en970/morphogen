# morphogen
#
#   make          build the wasm module into dist/
#   make test     build and run the native test suite
#   make serve    build, then serve dist/ on :8000
#   make clean
#
# src/core is portable C99 and has no Emscripten dependency, so `make test`
# builds exactly the same simulation code with the system compiler and runs it
# outside the browser. Everything the models do can therefore be profiled with
# real tools and checked against the literature in CI, which is the entire
# reason the core does not know that WebAssembly exists.

CORE := $(wildcard src/core/*.c) $(wildcard src/core/models/*.c)
WEB  := $(wildcard src/web/*.html) $(wildcard src/web/*.js) \
        $(wildcard src/web/js/*.js) $(wildcard src/web/css/*.css)

EMCC ?= emcc
CC   ?= cc

WARN := -Wall -Wextra -Wno-unused-parameter

# -O3, not -Os: this is a compute kernel and the binary is a few tens of KB
#   either way, so there is nothing to gain by trading speed for size.
# -msimd128 also switches on LLVM's autovectoriser, which the Gray-Scott and
#   Lenia inner loops take for free.
# ALLOW_MEMORY_GROWTH=0 is deliberate. A memory.grow detaches every typed-array
#   view JavaScript holds into linear memory, including the one the renderer
#   uploads from, and the symptom is a blank canvas with no error anywhere.
#   Allocating once, up front, removes the failure mode rather than papering
#   over it.
EMFLAGS := -O3 -flto -msimd128 \
	-sINITIAL_MEMORY=50331648 \
	-sALLOW_MEMORY_GROWTH=0 \
	-sMALLOC=emmalloc \
	-sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createMorphogen \
	-sENVIRONMENT=web,worker \
	-sFILESYSTEM=0 \
	-sASSERTIONS=0 \
	-sEXPORTED_RUNTIME_METHODS=['HEAPU8','HEAPF32','UTF8ToString'] \
	-sDISABLE_EXCEPTION_CATCHING=1 \
	--closure=0

.PHONY: all wasm web test serve clean

all: wasm web

wasm: dist/morphogen.js

dist/morphogen.js: $(CORE) src/wasm/api.c Makefile
	@mkdir -p dist
	$(EMCC) $(WARN) $(EMFLAGS) $(CORE) src/wasm/api.c -o dist/morphogen.js
	@ls -l dist/morphogen.wasm | awk '{printf "  wasm: %.1f KB\n", $$5/1024}'

# The site is static files; there is no bundler and there never will be. Copying
# is the build step.
#
# The paper is served from the site as well as living in the repository. GitHub's
# own PDF viewer gives up on it often enough to be a nuisance ("unable to render"),
# whereas a PDF served over plain HTTP is handled by the browser's built-in viewer,
# which never fails. So the canonical link people click is the hosted one.
web: $(WEB)
	@mkdir -p dist
	@cp -r src/web/. dist/
	@cp -f paper/morphogen.pdf dist/morphogen.pdf 2>/dev/null || true
	@touch dist/.nojekyll

test: tests/run
	@./tests/run

tests/run: $(CORE) tests/main.c
	$(CC) $(WARN) -O2 -std=c99 -Isrc/core $(CORE) tests/main.c -lm -o tests/run

serve: all
	@echo "http://localhost:8000"
	@cd dist && python3 -m http.server 8000

clean:
	rm -rf dist tests/run

# ── the paper ─────────────────────────────────────────────────────────────
#
# Every number and every figure in it is produced by tools/figures.c, which
# links against the same src/core the browser runs. There is no separate figure
# code that could drift away from the thing being described: if a plot in the
# paper is wrong, the simulation is wrong.

.PHONY: paper figures

tools/figures: $(CORE) tools/figures.c
	$(CC) $(WARN) -O2 -std=c99 -Isrc/core $(CORE) tools/figures.c -lm -o $@

figures: tools/figures
	@mkdir -p paper/data paper/fig
	./tools/figures
	@python3 tools/pgm2png.py paper/fig

paper: figures
	cd paper && tectonic -X compile morphogen.tex --outdir .
	@ls -l paper/morphogen.pdf | awk '{printf "  paper: %.0f KB\n", $$5/1024}'
