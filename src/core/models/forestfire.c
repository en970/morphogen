/* The Drossel-Schwabl forest fire, and self-organised criticality.
 *
 * Three states. An empty cell grows a tree with probability p. A tree catches
 * fire if any neighbour is burning, or, with probability f, if it is struck by
 * lightning. A burning cell burns out and becomes empty.
 *
 * Now the interesting part, which is about the ratio f/p and not about either
 * of them separately. Make lightning very rare compared with growth — f/p of
 * one in a thousand or less — and the forest does something no one told it to
 * do. It fills up until it is *just* on the edge of percolating, and it stays
 * there. Fires then come in every size: mostly tiny ones that take out a couple
 * of trees, occasionally a middling one, and every so often a fire that
 * consumes the entire continent. The distribution of fire sizes is a power law,
 * which is to say there is no typical fire.
 *
 * Nobody tuned anything to make that happen. In ordinary critical phenomena you
 * have to set the temperature to exactly T_c to see scale-free behaviour, and
 * an experimenter has to work to hold it there. Here the system drives itself
 * to its own critical point and sits on it. That is self-organised criticality,
 * and it is Bak, Tang and Wiesenfeld's answer to a genuinely deep question: why
 * is the natural world so full of power laws — earthquakes, avalanches,
 * extinctions, forest fires, market crashes — when criticality is supposed to
 * be a knife-edge that nothing should be balanced on?
 *
 * The lab histograms the fire sizes and plots them on log-log axes. The
 * exponent should come out near 1.15. Push f/p up and watch the power law
 * disappear: with frequent lightning the forest never gets dense enough to
 * carry a big fire, every fire is small, and the distribution acquires a scale.
 * The scale-free behaviour is not a property of the rules. It is a property of
 * the rules *plus* the separation of timescales.
 *
 * References
 *   Drossel, B. & Schwabl, F. "Self-organized critical forest-fire model."
 *     Phys. Rev. Lett. 69, 1629-1632 (1992).
 *   Bak, P., Tang, C. & Wiesenfeld, K. "Self-organized criticality: An
 *     explanation of 1/f noise." Phys. Rev. Lett. 59, 381-384 (1987).
 *   Clauset, A., Shalizi, C. R. & Newman, M. E. J. "Power-law distributions in
 *     empirical data." SIAM Review 51, 661-703 (2009). — how to fit the
 *     exponent honestly, which is not by regressing on the histogram.
 */
#include "../model.h"
#include "../field.h"

#include <math.h>

enum { P_P = 0, P_F, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"p", P_FLOAT, 0.0005f, 0.100f, 0.0005f, 0.020f, 0, "probability an empty cell grows a tree."},
    {"f", P_FLOAT, 0.0f,   0.0020f, 0.000002f, 0.00002f, 0, "probability a tree is struck by lightning. criticality needs f << p."},
};

enum { EMPTY = 0, TREE = 1, FIRE = 2 };

/* Log-spaced bins for the fire-size distribution. */
#define NBINS 24

/* Fires are tracked individually.
 *
 * The obvious shortcut — treat every cell burning anywhere on the map at a given
 * moment as one fire, and close the books when the last ember goes out — is
 * wrong, and wrong in a way that silently destroys the only observable that
 * matters here. In the critical regime lightning strikes often enough that two
 * fires are usually burning somewhere at once, so the map is never completely
 * cold, so the counter never closes, and a run of fifteen hundred steps reports
 * exactly one enormous fire. The power law, which is the entire point of the
 * model, disappears.
 *
 * So each strike opens a numbered fire, the fire's number is inherited by every
 * tree it spreads to, and the fire is only closed and binned when the last cell
 * carrying *its* number stops burning. */
#define MAXFIRES 2048

typedef struct {
    Lat lat;
    uint8_t *cur, *nxt;
    int32_t *fid;       /* which fire is burning in this cell */

    long fsize[MAXFIRES];   /* trees consumed so far by fire f */
    int32_t falive[MAXFIRES]; /* cells of fire f burning right now */
    uint8_t factive[MAXFIRES];
    int32_t next_id;

    long bins[NBINS];
    long total_fires;
    long largest;
} St;

static size_t ff_mem(int w, int h) {
    return (size_t)w * h * (2 + sizeof(int32_t)) + 4096 +
           (size_t)(w + h) * 4 * sizeof(int);
}

static void ff_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int n = wo->w * wo->h;

    lat_init(wo, &s->lat, BC_TORUS);
    s->cur = (uint8_t *)w_alloc(wo, (size_t)n);
    s->nxt = (uint8_t *)w_alloc(wo, (size_t)n);
    s->fid = (int32_t *)w_alloc(wo, (size_t)n * sizeof(int32_t));

    /* Start sparse and let the forest find its own density. Seeding it dense
     * would put it above the critical point, and it would burn back down to the
     * same place anyway, which is itself worth watching. */
    for (int i = 0; i < n; ++i) s->cur[i] = pcg32_f(&wo->rng) < 0.2f ? TREE : EMPTY;

    s->next_id = 0;
    s->total_fires = 0;
    s->largest = 0;
}

static int32_t new_fire(St *s) {
    for (int k = 0; k < MAXFIRES; ++k) {
        const int32_t f = (s->next_id + k) % MAXFIRES;
        if (!s->factive[f]) {
            s->next_id = (f + 1) % MAXFIRES;
            s->factive[f] = 1;
            s->fsize[f] = 0;
            s->falive[f] = 0;
            return f;
        }
    }
    return -1; /* every slot in use: the map is ablaze, drop this strike */
}

static void ff_step(World *wo) {
    St *s = (St *)wo->st;
    const Lat *L = &s->lat;
    const int w = L->w, h = L->h, n = w * h;
    const float p = wo->p[P_P];
    const float f = wo->p[P_F];

    int trees = 0, burning = 0;

    for (int i = 0; i < MAXFIRES; ++i) s->falive[i] = 0;

    for (int y = 0; y < h; ++y) {
        const int r0 = L->ym[y] * w, r1 = y * w, r2 = L->yp[y] * w;
        for (int x = 0; x < w; ++x) {
            const int a = L->xm[x], b = L->xp[x];
            const int i = r1 + x;
            const uint8_t c = s->cur[i];

            if (c == FIRE) {
                s->nxt[i] = EMPTY;
            } else if (c == TREE) {
                /* Which fire, if any, is reaching this tree? The neighbours'
                 * fire ids are still those of the previous step, because a cell
                 * that is burning now is not written this step. */
                int32_t from = -1;
                if (s->cur[r1 + a] == FIRE) from = s->fid[r1 + a];
                else if (s->cur[r1 + b] == FIRE) from = s->fid[r1 + b];
                else if (s->cur[r0 + x] == FIRE) from = s->fid[r0 + x];
                else if (s->cur[r2 + x] == FIRE) from = s->fid[r2 + x];

                if (from < 0 && pcg32_f(&wo->rng) < f) from = new_fire(s);

                if (from >= 0) {
                    s->nxt[i] = FIRE;
                    s->fid[i] = from;
                    s->fsize[from]++;
                    s->falive[from]++;
                    burning++;
                } else {
                    s->nxt[i] = TREE;
                    trees++;
                }
            } else {
                if (pcg32_f(&wo->rng) < p) {
                    s->nxt[i] = TREE;
                    trees++;
                } else {
                    s->nxt[i] = EMPTY;
                }
            }
        }
    }

    uint8_t *t = s->cur;
    s->cur = s->nxt;
    s->nxt = t;

    /* Close out the fires that have just gone cold, and bin them by size. */
    for (int32_t fi = 0; fi < MAXFIRES; ++fi) {
        if (!s->factive[fi] || s->falive[fi] > 0) continue;
        if (s->fsize[fi] > 0) {
            int bin = (int)log2((double)s->fsize[fi]);
            if (bin < 0) bin = 0;
            if (bin >= NBINS) bin = NBINS - 1;
            s->bins[bin]++;
            s->total_fires++;
            if (s->fsize[fi] > s->largest) s->largest = s->fsize[fi];
        }
        s->factive[fi] = 0;
    }

    wo->obs[0] = (float)trees / (float)n;   /* hovers at the critical density */
    wo->obs[1] = (float)burning;
    wo->obs[2] = (float)s->total_fires;
    wo->obs[3] = (float)s->largest;
}

static void ff_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        const uint8_t c = s->cur[i];
        out[i * 4 + 0] = c == TREE ? 210 : 0;   /* forest, in ink */
        out[i * 4 + 1] = 0;
        out[i * 4 + 2] = c == FIRE ? 255 : 0;   /* fire, in red */
        out[i * 4 + 3] = 0;
    }
}

/* The panel reads the histogram out of here to draw the log-log plot. */
const long *ff_bins(const World *wo, int *nbins) {
    const St *s = (const St *)wo->st;
    *nbins = NBINS;
    return s->bins;
}

static void ff_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0) x += w;
            if (x >= w) x -= w;
            if (y < 0) y += h;
            if (y >= h) y -= h;
            const int i = y * w + x;
            /* The brush is a box of matches: it sets fire to living trees. */
            if (erase) s->cur[i] = EMPTY;
            else if (s->cur[i] == TREE) s->cur[i] = FIRE;
        }
}

const Model MODEL_FORESTFIRE = {
    .id = "forestfire",
    .name = "Forest fire",
    .def_w = 256,
    .def_h = 256,
    .n_inks = 3,
    .ink_names = {"forest", "", "fire", 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0xd4573f, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"density", "burning", "fires", "largest", 0, 0},
    .n_obs = 4,
    .mem = ff_mem,
    .init = ff_init,
    .step = ff_step,
    .ink = ff_ink,
    .paint = ff_paint,
};
