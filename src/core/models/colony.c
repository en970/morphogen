/* Nutrient-limited bacterial colony growth.
 *
 * This is the model the lab is built around, because it is the cleanest answer
 * anybody has written down to the question "how does life find food?".
 *
 * Put a drop of Bacillus subtilis in the middle of an agar plate and wait. What
 * grows is not a disc. Starve it and you get a fractal — a branched dendrite
 * with a measured dimension of about 1.7, which is the value Witten and Sander
 * derived for diffusion-limited aggregation from an argument about random
 * walkers that has nothing to do with biology. Feed it well and you get a
 * compact disc of dimension 2. Nothing about the bacteria changed. You changed
 * how much food was in the dish.
 *
 * The mechanism is diffusion, and it is worth being precise about it. Bacteria
 * eat, so a colony digs a depletion halo around itself, and the only food left
 * is beyond that halo. Now consider a bump on the edge of the colony. It pokes
 * further out into fresh nutrient than the flat parts do, so it eats better, so
 * it grows faster, so it becomes a bigger bump. That runaway is the
 * Mullins-Sekerka instability. It is the same mathematics as a snowflake, a
 * lightning strike, and oil fingering through water.
 *
 * Whether the instability bites is decided by the *diffusion length*: how far
 * nutrient travels between one division and the next, which here is Dn times
 * substeps. Make it long and the nutrient field around the colony relaxes into
 * the smooth harmonic field that a DLA cluster grows in — tips see far more of
 * it than hollows do, the lightning-rod effect kicks in, and the colony branches.
 * Make it short and the front only ever knows about the food immediately in
 * front of it, no bump gets an advantage over any other, and the colony fills in
 * as a disc.
 *
 * One design decision carries the whole model: the cells are STATIC. The colony
 * is a structure, not a swarm. A real colony is mostly a scaffold of spent,
 * dormant cells with a thin living rind at the surface, and if you let the cells
 * wander freely you do not get a colony at all — you get a gas, which is exactly
 * what the first draft of this file produced. Growth happens where the living
 * rind touches food, and that is the whole of it.
 *
 * References
 *   Ben-Jacob, E., Schochet, O., Tenenbaum, A., Cohen, I., Czirók, A. & Vicsek,
 *     T. "Generic modelling of cooperative growth patterns in bacterial
 *     colonies." Nature 368, 46-49 (1994).
 *   Golding, I., Kozlovsky, Y., Cohen, I. & Ben-Jacob, E. "Studies of bacterial
 *     branching growth using reaction-diffusion models for colonial
 *     development." Physica A 260, 510-554 (1998).
 *   Fujikawa, H. & Matsushita, M. "Fractal growth of Bacillus subtilis on agar
 *     plates." J. Phys. Soc. Jpn. 58, 3875-3878 (1989).
 *   Matsushita, M. et al. "Interface growth and pattern formation in bacterial
 *     colonies." Physica A 249, 517-524 (1998).
 *   Witten, T. A. & Sander, L. M. "Diffusion-limited aggregation, a kinetic
 *     critical phenomenon." Phys. Rev. Lett. 47, 1400-1403 (1981).
 */
#include "../model.h"
#include "../field.h"
#include "../obs.h"

#include <math.h>
#include <string.h>

enum {
    P_N0 = 0,     /* initial nutrient: the food axis */
    /* Dn and substeps together set the diffusion length, which is the axis that
     * decides whether the growth front is stable. */
    P_DN,
    P_SUBSTEPS,
    P_CHI_N,      /* chemotaxis up the food gradient */
    P_CHI_R,      /* chemotaxis away from the distress signal */
    P_GAMMA_R,
    P_CMAX,
    P_EM,
    P_EDIV,
    P_NPARAM
};

/* A note on the numbers, because they are not arbitrary.
 *
 * A cell on the growing rind is fed by diffusion from the untouched agar beyond
 * it, and that flux is of order Dn * substeps * n0 — a few hundredths of a
 * nutrient unit per step. The metabolic cost `em` has to sit well *below* that
 * or the rind runs at exactly break-even: it eats precisely what it burns, banks
 * nothing, never reaches Ediv, never divides, and the colony freezes as a tiny
 * disc in the middle of a plate still full of food. So em is two orders of
 * magnitude under Cmax. The final size of the colony is then roughly (total
 * nutrient)/Ediv, which is what makes n0 the food axis quantitatively and not
 * just decoratively. */
static const ParamDef PARAMS[] = {
    {"n0",       P_FLOAT, 0.03f, 1.00f, 0.005f, 0.15f, 0, "initial nutrient. low: starved, and it must branch to find food. high: fed, and it fills in as a disc."},
    {"Dn",       P_FLOAT, 0.02f, 0.24f, 0.005f, 0.22f, 0, "nutrient diffusivity. with substeps, this sets the diffusion length, and the diffusion length decides whether the front is stable."},
    {"substeps", P_INT,   1.0f, 16.0f, 1.0f, 8.0f, 0, "nutrient diffusion steps per bacterial step. the nutrient must equilibrate much faster than the cells grow, or there is no gradient for a tip to exploit."},
    {"chiN",     P_FLOAT, 0.0f, 40.0f, 0.5f, 0.0f, 0, "food chemotaxis. newborn cells climb the nutrient gradient: sharper, faster, straighter tips."},
    {"chiR",     P_FLOAT, 0.0f, 40.0f, 0.5f, 0.0f, 0, "repulsive chemotaxis. cells flee the chemorepellent, so branches push each other apart and space themselves evenly."},
    {"gammaR",   P_FLOAT, 0.0f, 1.0f, 0.01f, 0.30f, 0, "how loudly a starving cell signals its distress."},
    {"Cmax",     P_FLOAT, 0.01f, 0.40f, 0.005f, 0.12f, 0, "the most nutrient a cell can take up in one step."},
    {"em",       P_FLOAT, 0.0001f, 0.010f, 0.0001f, 0.0005f, 0, "metabolic cost per step. a cell that cannot cover it starves and sporulates into the scaffold."},
    {"Ediv",     P_FLOAT, 0.10f, 2.00f, 0.02f, 0.50f, 0, "energy a cell must bank before it divides."},
};

enum { EMPTY = 0, LIVE = 1, SPORE = 2 };

#define SEED_R 3
#define DEAD (-1.0e30f)

typedef struct {
    int32_t i;   /* cell index */
    float e;     /* banked energy */
} Cell;

typedef struct {
    Lat lat;
    float *n;      /* nutrient */
    float *r;      /* chemorepellent */
    float *tmp;
    uint8_t *b;    /* EMPTY / LIVE / SPORE */
    uint8_t *mask; /* scratch for box counting */

    Cell *c;
    int nc, cap;
    int32_t *order;

    float d_cached;
    int d_age;
} St;

static size_t colony_mem(int w, int h) {
    size_t n = (size_t)w * h;
    return n * (3 * sizeof(float) + 2) + n * sizeof(Cell) + n * sizeof(int32_t) +
           4096 + (size_t)(w + h) * 4 * sizeof(int);
}

static void add_cell(St *s, int i, float e) {
    if (s->nc >= s->cap) return;
    s->c[s->nc].i = i;
    s->c[s->nc].e = e;
    s->nc++;
    s->b[i] = LIVE;
}

static void colony_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int w = wo->w, h = wo->h, n = w * h;

    /* A Petri dish is closed: nutrient that reaches the wall stays in the dish.
     * The boundary is no-flux, not periodic. */
    lat_init(wo, &s->lat, BC_NOFLUX);

    s->n = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->r = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->tmp = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->b = (uint8_t *)w_alloc(wo, (size_t)n);
    s->mask = (uint8_t *)w_alloc(wo, (size_t)n);
    s->c = (Cell *)w_alloc(wo, (size_t)n * sizeof(Cell));
    s->order = (int32_t *)w_alloc(wo, (size_t)n * sizeof(int32_t));
    s->cap = n;
    s->nc = 0;
    s->d_cached = 0.0f;
    s->d_age = 0;

    const float n0 = wo->p[P_N0];
    for (int i = 0; i < n; ++i) s->n[i] = n0;

    /* The inoculum: a few cells in the middle of the plate, arriving nearly
     * ready to divide, as a drop off a loop from an overnight culture does. */
    const int cx = w / 2, cy = h / 2;
    for (int dy = -SEED_R; dy <= SEED_R; ++dy)
        for (int dx = -SEED_R; dx <= SEED_R; ++dx) {
            if (dx * dx + dy * dy > SEED_R * SEED_R) continue;
            add_cell(s, (cy + dy) * w + (cx + dx), wo->p[P_EDIV] * 0.9f);
        }
}

/* Choose an empty neighbour, weighted by the gradients the cell can smell.
 * Returns -1 if the site is walled in — which is what makes the interior of the
 * colony go dormant, and is real biology, not a limitation. */
static int pick_empty(World *wo, St *s, int w, int h, int x, int y,
                      float chiN, float chiR) {
    float wt[8];
    int idx[8];
    int cnt = 0;
    float sum = 0.0f;
    const int i = y * w + x;

    for (int dy = -1; dy <= 1; ++dy) {
        const int yy = y + dy;
        if (yy < 0 || yy >= h) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            const int xx = x + dx;
            if (xx < 0 || xx >= w) continue;
            const int j = yy * w + xx;
            if (s->b[j] != EMPTY) continue;

            float g = chiN * (s->n[j] - s->n[i]) - chiR * (s->r[j] - s->r[i]);
            if (g > 12.0f) g = 12.0f;
            if (g < -12.0f) g = -12.0f;
            const float e = expf(g);
            wt[cnt] = e;
            idx[cnt] = j;
            sum += e;
            cnt++;
        }
    }
    if (!cnt || sum <= 0.0f) return -1;

    float t = pcg32_f(&wo->rng) * sum;
    for (int k = 0; k < cnt; ++k) {
        t -= wt[k];
        if (t <= 0.0f) return idx[k];
    }
    return idx[cnt - 1];
}

static void colony_step(World *wo) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;

    const float Dn = wo->p[P_DN];
    const int sub = (int)wo->p[P_SUBSTEPS];
    const float chiN = wo->p[P_CHI_N];
    const float chiR = wo->p[P_CHI_R];
    const float gammaR = wo->p[P_GAMMA_R];
    const float Cmax = wo->p[P_CMAX];
    const float em = wo->p[P_EM];
    const float Ediv = wo->p[P_EDIV];

    /* Nutrient and signal relax on a much faster clock than the cells divide on.
     * That separation of timescales is not an optimisation; it is the physics
     * that produces the halo, and the halo is what produces the branches. */
    for (int k = 0; k < sub; ++k) diffuse(&s->lat, s->n, s->tmp, Dn);
    if (chiR > 0.0f && gammaR > 0.0f)
        for (int k = 0; k < sub; ++k) diffuse_decay(&s->lat, s->r, s->tmp, 0.20f, 0.02f);

    /* Cells act one at a time, in an order drawn from the seeded stream: a
     * synchronous update would let two cells divide into the same empty site. */
    for (int i = 0; i < s->nc; ++i) s->order[i] = i;
    shuffle_i32(&wo->rng, s->order, s->nc);

    const int nc0 = s->nc;
    for (int oi = 0; oi < nc0; ++oi) {
        Cell *c = &s->c[s->order[oi]];
        if (c->e == DEAD) continue;

        const int i = c->i;
        const int x = i % w, y = i / w;

        /* eat */
        const float uptake = s->n[i] < Cmax ? s->n[i] : Cmax;
        s->n[i] -= uptake;
        c->e += uptake;

        /* pay the rent */
        c->e -= em;

        if (c->e < 0.0f) {
            /* Starved. The cell sporulates: it stops metabolising and becomes
             * part of the permanent scaffold. Most of what you see when you look
             * at a colony on a plate is this — the spent. */
            s->b[i] = SPORE;
            if (gammaR > 0.0f) s->r[i] += gammaR;
            c->e = DEAD;
            continue;
        }

        /* Hungry, but not dead yet. Cells in the starved interior complain, and
         * cells elsewhere move away from the complaining. */
        if (gammaR > 0.0f && c->e < Ediv * 0.25f) s->r[i] += gammaR * 0.1f;

        /* divide */
        if (c->e > Ediv) {
            const int j = pick_empty(wo, s, w, h, x, y, chiN, chiR);
            if (j >= 0) {
                c->e *= 0.5f;
                add_cell(s, j, c->e);
            }
            /* Nowhere to put a daughter: the cell simply waits. The interior of a
             * real colony goes dormant for exactly this reason. */
        }
    }

    /* Compact out the cells that starved this step. Tombstoning and compacting
     * once, rather than swap-removing mid-iteration, keeps the run a function of
     * the seed alone and not of the order in which cells happened to die. */
    {
        int k = 0;
        for (int i = 0; i < s->nc; ++i)
            if (s->c[i].e != DEAD) s->c[k++] = s->c[i];
        s->nc = k;
    }

    /* observables */
    {
        const int n = w * h;
        long biomass = 0;
        double nut = 0.0;
        for (int i = 0; i < n; ++i) {
            if (s->b[i] != EMPTY) biomass++;
            nut += s->n[i];
        }
        wo->obs[0] = (float)biomass;
        wo->obs[1] = (float)s->nc;
        wo->obs[3] = (float)(nut / (double)n);

        /* Box counting is the expensive one, and a colony does not change shape
         * appreciably in a single generation, so it runs every 24 steps. */
        if (--s->d_age <= 0) {
            for (int i = 0; i < n; ++i) s->mask[i] = s->b[i] != EMPTY;
            s->d_cached = box_dimension(s->mask, w, h, 0, 0);
            s->d_age = 24;
        }
        wo->obs[2] = s->d_cached;
    }
}

static void colony_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    const float n0 = wo->p[P_N0] > 1e-6f ? wo->p[P_N0] : 1.0f;
    const int show_r = wo->p[P_CHI_R] > 0.0f && wo->p[P_GAMMA_R] > 0.0f;

    for (int i = 0; i < n; ++i) {
        /* black: the colony. The living rind prints a little darker than the
         * spent scaffold behind it, so you can see where it is still growing. */
        int col = 0;
        if (s->b[i] == LIVE) col = 255;
        else if (s->b[i] == SPORE) col = 190;

        /* blue: the nutrient. The colony eats a hole in it, and that hole — the
         * depletion halo — is the thing doing all the work. Watch the pale ring
         * travel outward just ahead of the black.
         *
         * Printed at well under full strength, deliberately: the nutrient is the
         * ground the colony is drawn on, not the subject. */
        float nn = s->n[i] / n0;
        if (nn < 0.0f) nn = 0.0f;
        if (nn > 1.0f) nn = 1.0f;

        /* red: the distress signal, and only when it is actually doing something.
         * Printing a field that nothing reads would be decoration. */
        int rep = 0;
        if (show_r) {
            float rr = s->r[i] * 3.0f;
            if (rr > 1.0f) rr = 1.0f;
            rep = (int)(rr * 210.0f);
        }

        out[i * 4 + 0] = (uint8_t)col;
        out[i * 4 + 1] = (uint8_t)(nn * 105.0f);
        out[i * 4 + 2] = (uint8_t)rep;
        out[i * 4 + 3] = 0;
    }
}

static void colony_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            const int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= w || y < 0 || y >= h) continue;
            const int i = y * w + x;
            if (erase) {
                /* the brush drops fresh nutrient: feed the colony by hand and
                 * watch a branch turn towards your finger */
                s->n[i] = wo->p[P_N0];
            } else if (s->b[i] == EMPTY) {
                add_cell(s, i, wo->p[P_EDIV] * 0.9f);
            }
        }
}

const Model MODEL_COLONY = {
    .id = "colony",
    .name = "Bacterial colony",
    .def_w = 320,
    .def_h = 320,
    .n_inks = 3,
    .ink_names = {"colony", "nutrient", "signal", 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0xd4573f, 0x000000},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"biomass", "growing", "D", "nutrient", 0, 0},
    .n_obs = 4,
    .mem = colony_mem,
    .init = colony_init,
    .step = colony_step,
    .ink = colony_ink,
    .paint = colony_paint,
};
