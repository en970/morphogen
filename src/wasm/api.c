/* The boundary between the simulation and the browser.
 *
 * It is deliberately narrow. C owns the grid, the agents, the fields, and the
 * ink buffer; JavaScript owns the screen and the pointer. Per frame, exactly
 * three calls cross the line: step, ink, and a read of the observables. Nothing
 * is marshalled per cell, no strings are passed in the hot path, and the pixel
 * data is never copied — JavaScript takes a typed-array view straight into
 * linear memory and hands it to the GPU.
 *
 * These are raw exported symbols rather than Embind or ccall wrappers. Embind
 * is pleasant and would cost us tens of kilobytes of glue and a marshalling
 * layer we would then spend our time working around; ccall re-does its argument
 * conversion on every single call. A WebAssembly export is a function call.
 *
 * The one thing that does get marshalled is the model catalogue, and it happens
 * once at startup: mg_meta() renders every model, parameter, range, default and
 * help string as JSON, so the panel can build itself from the C definitions
 * instead of duplicating them. There is exactly one place in this project where
 * a parameter's range is written down, and it is the ParamDef table in the
 * model's own source file.
 */
#include "../core/world.h"
#include "../core/model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define API EMSCRIPTEN_KEEPALIVE
#else
#define API
#endif

/* Sized for the largest grid any model asks for. Growth is switched off in the
 * build (-sALLOW_MEMORY_GROWTH=0) because a memory.grow silently detaches every
 * typed-array view JavaScript holds — including the one inside the texture
 * upload — and the resulting bug is a blank canvas with no error. Allocating
 * once, up front, deletes the entire failure mode. */
#define ARENA_BYTES (48u << 20)

static World G;
static uint8_t *g_arena;
static uint8_t *g_ink;
static char g_meta[24000];

API int mg_boot(void) {
    g_arena = (uint8_t *)malloc(ARENA_BYTES);
    g_ink = (uint8_t *)malloc((size_t)MAX_DIM * MAX_DIM * 4);
    if (!g_arena || !g_ink) return 0;
    G.arena = g_arena;
    G.arena_cap = ARENA_BYTES;
    G.ink = g_ink;
    return 1;
}

API int mg_model_count(void) { return MODEL_COUNT; }

API void mg_select(int idx, int gw, int gh, double seed_lo, double seed_hi) {
    if (idx < 0 || idx >= MODEL_COUNT) idx = 0;
    const Model *m = MODELS[idx];
    if (gw <= 0) gw = m->def_w;
    if (gh <= 0) gh = m->def_h;

    /* Defaults go in before init(), because most models read their parameters
     * while building the initial condition. */
    world_set_defaults(&G, m);
    uint64_t seed = ((uint64_t)(uint32_t)seed_hi << 32) | (uint32_t)seed_lo;
    world_reset(&G, m, gw, gh, seed);
}

/* Re-run the initial condition, keeping whatever the user has set on the
 * sliders. This is what the `r` key does, and because the seed goes in
 * explicitly, the same seed and the same parameters give back the same run,
 * cell for cell, on any machine. */
API void mg_reset(int gw, int gh, double seed_lo, double seed_hi) {
    if (!G.model) return;
    uint64_t seed = ((uint64_t)(uint32_t)seed_hi << 32) | (uint32_t)seed_lo;
    world_reset(&G, G.model, gw > 0 ? gw : G.w, gh > 0 ? gh : G.h, seed);
}

API void mg_step(int n) { world_step(&G, n); }

API void mg_render(void) { G.model->ink(&G, G.ink); }

API uint8_t *mg_ink_ptr(void) { return G.ink; }
API float *mg_obs_ptr(void) { return G.obs; }
API float *mg_param_ptr(void) { return G.p; }

API int mg_width(void) { return G.w; }
API int mg_height(void) { return G.h; }
API double mg_gen(void) { return (double)G.gen; }

API void mg_set_param(int i, float v) {
    if (i < 0 || i >= MAX_PARAMS) return;
    G.p[i] = v;
}

API void mg_paint(int x, int y, int r, int erase) {
    if (G.model && G.model->paint) G.model->paint(&G, x, y, r, erase);
}

API double mg_hash(void) { return (double)(world_hash(&G) & 0x1FFFFFFFFFFFFFULL); }

/* --- the catalogue, rendered once ------------------------------------- */

static void jstr(char **p, char *end, const char *s) {
    while (*s && *p < end - 2) {
        if (*s == '"' || *s == '\\') *(*p)++ = '\\';
        *(*p)++ = *s++;
    }
}

API const char *mg_meta(void) {
    char *p = g_meta;
    char *end = g_meta + sizeof(g_meta);

    p += snprintf(p, (size_t)(end - p), "[");
    for (int i = 0; i < MODEL_COUNT; ++i) {
        const Model *m = MODELS[i];
        p += snprintf(p, (size_t)(end - p),
                      "%s{\"id\":\"%s\",\"name\":\"", i ? "," : "", m->id);
        jstr(&p, end, m->name);
        p += snprintf(p, (size_t)(end - p), "\",\"w\":%d,\"h\":%d,\"inks\":[",
                      m->def_w, m->def_h);
        for (int k = 0; k < m->n_inks; ++k)
            p += snprintf(p, (size_t)(end - p), "%s{\"name\":\"%s\",\"color\":\"#%06x\"}",
                          k ? "," : "", m->ink_names[k] ? m->ink_names[k] : "",
                          m->ink_colors[k]);
        p += snprintf(p, (size_t)(end - p), "],\"obs\":[");
        for (int k = 0; k < m->n_obs; ++k)
            p += snprintf(p, (size_t)(end - p), "%s\"%s\"", k ? "," : "", m->obs_names[k]);
        p += snprintf(p, (size_t)(end - p), "],\"params\":[");
        for (int k = 0; k < m->n_params; ++k) {
            const ParamDef *d = &m->params[k];
            p += snprintf(p, (size_t)(end - p),
                          "%s{\"id\":\"%s\",\"kind\":%d,\"min\":%g,\"max\":%g,"
                          "\"step\":%g,\"def\":%g,\"opts\":\"%s\",\"help\":\"",
                          k ? "," : "", d->id, (int)d->kind, d->min, d->max,
                          d->step, d->def, d->opts ? d->opts : "");
            jstr(&p, end, d->help ? d->help : "");
            p += snprintf(p, (size_t)(end - p), "\"}");
        }
        p += snprintf(p, (size_t)(end - p), "]}");
    }
    snprintf(p, (size_t)(end - p), "]");
    return g_meta;
}
