/* The common interface every model implements.
 *
 * The shell around this — the renderer, the parameter panel, the plots, the
 * sweep engine — knows nothing about any particular model. It knows only that a
 * model declares some parameters, some observables, and up to four ink
 * channels, and that it can be initialised, stepped, and painted into.
 *
 * That is the whole reason the lab reads as one instrument with ten models
 * plugged into it rather than as ten separate demos.
 */
#ifndef MORPHOGEN_MODEL_H
#define MORPHOGEN_MODEL_H

#include <stdint.h>
#include <stddef.h>
#include "rng.h"

#define MAX_PARAMS 20
#define MAX_INKS 4
#define MAX_OBS 6
#define MAX_DIM 512

/* How a parameter presents itself in the panel. Labels are the literal
 * identifiers used in the code and in the papers: `mu`, `sigma`, `F`, `k`,
 * `tau`. Renaming them to "Growth Centre" would be friendlier and would also
 * quietly sever the link between the control and the literature. */
typedef enum {
    P_FLOAT,  /* continuous slider */
    P_INT,    /* integer slider */
    P_ENUM    /* discrete choice; `opts` is a "a|b|c" string */
} ParamKind;

typedef struct {
    const char *id;
    ParamKind kind;
    float min, max, step, def;
    const char *opts;  /* P_ENUM only */
    const char *help;  /* one line, shown on hover and in the info drawer */
} ParamDef;

typedef struct World World;

typedef struct {
    const char *id;
    const char *name;

    int def_w, def_h;

    /* Ink channels. A model with one ink prints in black on paper. A model with
     * three (rock-paper-scissors, say) prints as three separations at different
     * screen angles, the way a press would. */
    int n_inks;
    const char *ink_names[MAX_INKS];
    uint32_t ink_colors[MAX_INKS]; /* 0xRRGGBB */

    const ParamDef *params;
    int n_params;

    const char *obs_names[MAX_OBS];
    int n_obs;

    /* Bytes of scratch this model needs at the given grid size. The arena is
     * handed out by the world; models never call malloc, so a reset can never
     * leak and a sweep can reuse one allocation for thousands of runs. */
    size_t (*mem)(int w, int h);

    void (*init)(World *);
    void (*step)(World *);

    /* Write w*h*4 bytes: one byte of coverage per ink channel per cell. This is
     * the only thing the renderer ever reads. */
    void (*ink)(World *, uint8_t *out);

    /* Optional. Brush interaction; `val` is 0..1, button 0 = add, 1 = erase. */
    void (*paint)(World *, int cx, int cy, int radius, int erase);
} Model;

struct World {
    int w, h;
    uint64_t gen;
    uint64_t seed;

    const Model *model;
    float p[MAX_PARAMS];
    float obs[MAX_OBS];

    pcg32_t rng;   /* the model's main stream */

    void *st;      /* model state, carved out of the arena */

    uint8_t *arena;
    size_t arena_cap, arena_used;

    uint8_t *ink;  /* w*h*4, handed to the renderer as a texture */
};

/* Bump allocation out of the arena. Zeroed. Alignment is 16 so that a float
 * field can be handed straight to SIMD loads. */
void *w_alloc(World *w, size_t bytes);

/* Parameter lookup by index; the JS side and the sweep engine address
 * parameters positionally, which keeps the URL hash short. */
static inline float pget(const World *w, int i) { return w->p[i]; }

#endif
