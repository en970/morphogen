/* Schelling's segregation model.
 *
 * Two kinds of household on a grid, with some empty houses. Every household
 * looks at its eight neighbours and asks one question: what fraction of them
 * are like me? If that fraction is below its tolerance threshold tau, it is
 * unhappy and moves to a randomly chosen empty house. That is all.
 *
 * Nobody in this model wants segregation. Set tau to 0.35 and you have written
 * down a population in which every single household is perfectly content to be
 * outnumbered nearly two to one by the other kind — a population more tolerant
 * than any real society has ever been. Run it. It segregates completely, into
 * solid blocks, with a segregation index around 0.85.
 *
 * The mechanism is worth spelling out, because it is not obvious and it is the
 * point. A household that moves because it was slightly uncomfortable makes its
 * old neighbourhood slightly more uniform for the ones it left behind, and
 * slightly more mixed for the ones it lands among — some of whom now become
 * unhappy and move in turn. The system ratchets. Individual preferences that
 * are mild, reasonable, and nowhere near segregationist compose into a
 * collective outcome that nobody chose and nobody wants.
 *
 * Schelling's own conclusion, in 1971, was that you cannot infer the
 * preferences of individuals from the pattern of the aggregate. It is the
 * single most useful thing this lab has to say about people, and he got a Nobel
 * for it.
 *
 * Turn tau up past about 0.7 and the system gridlocks instead: there is no
 * arrangement in which everyone is happy, and the households churn forever.
 *
 * References
 *   Schelling, T. C. "Dynamic models of segregation." Journal of Mathematical
 *     Sociology 1(2), 143-186 (1971).
 *   Schelling, T. C. Micromotives and Macrobehavior. Norton (1978).
 */
#include "../model.h"
#include "../obs.h"

#include <string.h>

enum { P_TAU = 0, P_EMPTY, P_RATIO, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"tau",   P_FLOAT, 0.0f, 1.0f, 0.01f, 0.35f, 0, "tolerance. a household is unhappy if fewer than this fraction of its neighbours are like it."},
    {"empty", P_FLOAT, 0.02f, 0.45f, 0.01f, 0.15f, 0, "fraction of houses left vacant. with nowhere to move, nothing happens."},
    {"ratio", P_FLOAT, 0.1f, 0.9f, 0.01f, 0.50f, 0, "share of the occupied houses belonging to the first group."},
};

typedef struct {
    int8_t *t;        /* -1 empty, 0 or 1 */
    int32_t *empty;   /* indices of vacant houses */
    int n_empty;
    int32_t *unhappy;
    int n_unhappy;
} St;

static size_t sch_mem(int w, int h) {
    size_t n = (size_t)w * h;
    return n * sizeof(int8_t) + n * 2 * sizeof(int32_t) + 4096;
}

static void sch_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int n = wo->w * wo->h;

    s->t = (int8_t *)w_alloc(wo, (size_t)n);
    s->empty = (int32_t *)w_alloc(wo, (size_t)n * sizeof(int32_t));
    s->unhappy = (int32_t *)w_alloc(wo, (size_t)n * sizeof(int32_t));

    const float pe = wo->p[P_EMPTY];
    const float ratio = wo->p[P_RATIO];

    s->n_empty = 0;
    for (int i = 0; i < n; ++i) {
        if (pcg32_f(&wo->rng) < pe) {
            s->t[i] = -1;
            s->empty[s->n_empty++] = i;
        } else {
            s->t[i] = pcg32_f(&wo->rng) < ratio ? 0 : 1;
        }
    }
}

/* Happy if the share of like-minded neighbours is at least tau. A household
 * with no neighbours at all is happy — there is nothing for it to object to. */
static int happy_as(const St *s, int w, int h, int x, int y, int8_t me, float tau) {
    int same = 0, occupied = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0) nx += w;
            if (nx >= w) nx -= w;
            if (ny < 0) ny += h;
            if (ny >= h) ny -= h;
            int8_t u = s->t[ny * w + nx];
            if (u < 0) continue;
            occupied++;
            if (u == me) same++;
        }
    }
    if (occupied == 0) return 1;
    return (float)same / (float)occupied >= tau;
}

static int happy(const St *s, int w, int h, int x, int y, float tau) {
    return happy_as(s, w, h, x, y, s->t[y * w + x], tau);
}

static void sch_step(World *wo) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h, n = w * h;
    const float tau = wo->p[P_TAU];

    s->n_unhappy = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            if (s->t[i] < 0) continue;
            if (!happy(s, w, h, x, y, tau)) s->unhappy[s->n_unhappy++] = i;
        }

    /* Everyone unhappy moves, in a shuffled order.
     *
     * Where to? Schelling's households do not relocate blindly; they look for
     * somewhere they would actually be content. So each one samples vacancies
     * and takes the first that would make it happy, falling back to a random
     * vacancy if it cannot find one — which is what keeps the system churning
     * rather than freezing when tau is set impossibly high. Relocating purely at
     * random instead is a common simplification, and it converges to the same
     * place, just far more slowly and less sharply. */
    shuffle_i32(&wo->rng, s->unhappy, s->n_unhappy);

    const int TRIES = 12;

    for (int k = 0; k < s->n_unhappy && s->n_empty > 0; ++k) {
        const int from = s->unhappy[k];
        const int8_t me = s->t[from];
        if (me < 0) continue;

        uint32_t pick = pcg32_below(&wo->rng, (uint32_t)s->n_empty);
        for (int t = 0; t < TRIES; ++t) {
            const uint32_t cand = pcg32_below(&wo->rng, (uint32_t)s->n_empty);
            const int c = s->empty[cand];
            if (happy_as(s, w, h, c % w, c / w, me, tau)) { pick = cand; break; }
        }

        const int to = s->empty[pick];
        s->t[to] = me;
        s->t[from] = -1;
        s->empty[pick] = from;
    }

    wo->obs[0] = segregation(s->t, w, h);
    wo->obs[1] = (float)s->n_unhappy / (float)n;
    wo->obs[2] = (float)s->n_unhappy;
}

static void sch_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        out[i * 4 + 0] = s->t[i] == 0 ? 255 : 0;
        out[i * 4 + 1] = s->t[i] == 1 ? 255 : 0;
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = 0;
    }
}

const Model MODEL_SCHELLING = {
    .id = "schelling",
    .name = "Segregation",
    .def_w = 160,
    .def_h = 160,
    .n_inks = 2,
    .ink_names = {"group A", "group B", 0, 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"segregation", "unhappy", "moves", 0, 0, 0},
    .n_obs = 3,
    .mem = sch_mem,
    .init = sch_init,
    .step = sch_step,
    .ink = sch_ink,
    .paint = 0,
};
