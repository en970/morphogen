#include "world.h"

#include <string.h>

void *w_alloc(World *w, size_t bytes) {
    size_t aligned = (bytes + 15u) & ~(size_t)15u;
    if (w->arena_used + aligned > w->arena_cap) return 0; /* caller declared too
                                                           * little in mem(); a
                                                           * null here will trap
                                                           * immediately rather
                                                           * than corrupt */
    void *p = w->arena + w->arena_used;
    w->arena_used += aligned;
    memset(p, 0, aligned);
    return p;
}

const Model *model_by_id(const char *id) {
    for (int i = 0; i < MODEL_COUNT; ++i)
        if (strcmp(MODELS[i]->id, id) == 0) return MODELS[i];
    return 0;
}

void world_reset(World *w, const Model *m, int gw, int gh, uint64_t seed) {
    if (gw < 16) gw = 16;
    if (gh < 16) gh = 16;
    if (gw > MAX_DIM) gw = MAX_DIM;
    if (gh > MAX_DIM) gh = MAX_DIM;

    w->model = m;
    w->w = gw;
    w->h = gh;
    w->seed = seed;
    w->gen = 0;
    w->arena_used = 0;
    w->st = 0;

    memset(w->obs, 0, sizeof(w->obs));

    /* Stream 0 is the model's own sequential stream. Models that need more
     * (initial condition versus per-step motion, say) seed their own from the
     * same master seed with a different stream index, so that adding a random
     * draw in one place cannot silently change the numbers everywhere else. */
    pcg32_seed(&w->rng, seed, 0);

    memset(w->ink, 0, (size_t)gw * gh * 4);

    m->init(w);
}

void world_set_defaults(World *w, const Model *m) {
    for (int i = 0; i < m->n_params; ++i) w->p[i] = m->params[i].def;
}

void world_step(World *w, int n) {
    for (int i = 0; i < n; ++i) {
        w->model->step(w);
        w->gen++;
    }
}

/* FNV-1a over the ink buffer. Not a checksum of the internal state — of what
 * the model actually shows. That is the thing a regression test should pin:
 * "seed 42, generation 10000, this model, must look like this." The native test
 * runner asserts these and CI runs it on every push. */
uint64_t world_hash(World *w) {
    w->model->ink(w, w->ink);
    uint64_t h = 1469598103934665603ULL;
    size_t n = (size_t)w->w * w->h * 4;
    for (size_t i = 0; i < n; ++i) {
        h ^= w->ink[i];
        h *= 1099511628211ULL;
    }
    return h;
}
