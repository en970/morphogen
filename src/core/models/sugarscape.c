/* Sugarscape.
 *
 * A landscape with two mountains of sugar. Agents scattered across it, each
 * with three numbers drawn at random when it is born: how far it can see, how
 * much sugar it burns each tick, and how much it starts with. Each tick, an
 * agent looks along the four compass directions as far as its vision allows,
 * goes to the unoccupied cell with the most sugar, eats all of it, and pays its
 * metabolism. If its stock ever falls below zero it starves. The sugar grows
 * back at a fixed rate.
 *
 * That is the entire specification, and it is scrupulously fair: nobody is
 * favoured, nobody inherits, nobody cheats, nobody is taxed, the rules are the
 * same for everyone, and the initial endowments are drawn from the same uniform
 * distribution. Epstein and Axtell then ask what the distribution of wealth
 * looks like after a while.
 *
 * It is not uniform. It is not even symmetric. It becomes strongly right-skewed
 * — a small number of very rich agents and a long tail of poor ones — with a
 * Gini coefficient that settles around 0.5, which is roughly that of a real
 * economy. The lab plots the Lorenz curve and the Gini live, next to the grid,
 * so you can watch a fair world produce an unfair distribution and see how fast
 * it happens.
 *
 * The other thing to watch is the population, which nobody set. Start with four
 * hundred agents or with fifty; either way the number converges to the same
 * value, the carrying capacity of the landscape, which is a property of the
 * sugar and the metabolisms and not of anything anyone decided. Turn the
 * regrowth rate down and watch the ceiling drop.
 *
 * A detail that is easy to get wrong and changes the results: vision is along
 * the four principal directions only, not the diagonals. Epstein and Axtell are
 * explicit about it.
 *
 * References
 *   Epstein, J. M. & Axtell, R. Growing Artificial Societies: Social Science
 *     from the Bottom Up. Brookings Institution Press / MIT Press (1996).
 *     Chapter II, rules G and M.
 */
#include "../model.h"
#include "../obs.h"

#include <math.h>
#include <string.h>

enum { P_GROWBACK = 0, P_POP, P_MAXVISION, P_MAXMETAB, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"growback",  P_FLOAT, 0.05f, 4.0f, 0.05f, 1.0f, 0, "sugar regrown per cell per tick. abundance, in one number."},
    {"pop0",      P_INT,   50.0f, 1200.0f, 10.0f, 400.0f, 0, "agents at the start. the population will ignore this and find its own level."},
    {"maxVision", P_INT,   1.0f, 12.0f, 1.0f, 6.0f, 0, "vision is drawn uniformly from 1 to this. it is the only thing resembling talent."},
    {"maxMetab",  P_INT,   1.0f, 8.0f, 1.0f, 4.0f, 0, "metabolism is drawn uniformly from 1 to this. it is the only thing resembling need."},
};

#define MAXCAP 4096

typedef struct {
    int32_t x, y;
    int32_t vision, metab;
    float wealth;
    uint8_t alive;
} Agent;

typedef struct {
    float *sugar;
    float *cap;      /* the landscape: maximum sugar each cell can hold */
    int32_t *occ;    /* agent index at each cell, or -1 */
    Agent *a;
    int na;
    float *wbuf;     /* scratch for the Gini */
    int32_t *order;
} St;

static size_t sug_mem(int w, int h) {
    size_t n = (size_t)w * h;
    return n * 2 * sizeof(float) + n * sizeof(int32_t) +
           MAXCAP * (sizeof(Agent) + sizeof(float) + sizeof(int32_t)) + 4096;
}

/* Epstein and Axtell's landscape is two radially symmetric peaks, north-east
 * and south-west, quantised to four levels. This reproduces its shape rather
 * than the exact bitmap from the book. */
static float landscape(int x, int y, int w, int h) {
    const float px1 = 0.72f * w, py1 = 0.28f * h;
    const float px2 = 0.28f * w, py2 = 0.72f * h;
    const float sd = 0.28f * (float)((w < h ? w : h));

    const float d1 = ((x - px1) * (x - px1) + (y - py1) * (y - py1)) / (sd * sd);
    const float d2 = ((x - px2) * (x - px2) + (y - py2) * (y - py2)) / (sd * sd);
    const float f = expf(-d1) + expf(-d2);

    float lvl = f * 4.6f;
    if (lvl > 4.0f) lvl = 4.0f;
    return floorf(lvl + 0.35f); /* four discrete terraces, as in the book */
}

static void sug_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int w = wo->w, h = wo->h, n = w * h;

    s->sugar = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->cap = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->occ = (int32_t *)w_alloc(wo, (size_t)n * sizeof(int32_t));
    s->a = (Agent *)w_alloc(wo, MAXCAP * sizeof(Agent));
    s->wbuf = (float *)w_alloc(wo, MAXCAP * sizeof(float));
    s->order = (int32_t *)w_alloc(wo, MAXCAP * sizeof(int32_t));

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            s->cap[i] = landscape(x, y, w, h);
            s->sugar[i] = s->cap[i];  /* the plate starts full */
            s->occ[i] = -1;
        }

    int pop = (int)wo->p[P_POP];
    if (pop > MAXCAP) pop = MAXCAP;
    const int maxv = (int)wo->p[P_MAXVISION];
    const int maxm = (int)wo->p[P_MAXMETAB];

    s->na = 0;
    for (int k = 0; k < pop; ++k) {
        for (int tries = 0; tries < 64; ++tries) {
            const int x = (int)pcg32_below(&wo->rng, (uint32_t)w);
            const int y = (int)pcg32_below(&wo->rng, (uint32_t)h);
            const int i = y * w + x;
            if (s->occ[i] >= 0) continue;
            Agent *a = &s->a[s->na];
            a->x = x;
            a->y = y;
            a->vision = 1 + (int)pcg32_below(&wo->rng, (uint32_t)maxv);
            a->metab = 1 + (int)pcg32_below(&wo->rng, (uint32_t)maxm);
            /* Endowments are uniform on [5, 25]: everybody starts from roughly
             * the same place. Remember that when you look at the Gini later. */
            a->wealth = 5.0f + (float)pcg32_below(&wo->rng, 21);
            a->alive = 1;
            s->occ[i] = s->na;
            s->na++;
            break;
        }
    }
}

static void sug_step(World *wo) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h, n = w * h;
    const float g = wo->p[P_GROWBACK];

    /* rule G: growback */
    for (int i = 0; i < n; ++i) {
        s->sugar[i] += g;
        if (s->sugar[i] > s->cap[i]) s->sugar[i] = s->cap[i];
    }

    for (int i = 0; i < s->na; ++i) s->order[i] = i;
    shuffle_i32(&wo->rng, s->order, s->na);

    /* rule M: look, move, eat, pay */
    for (int k = 0; k < s->na; ++k) {
        Agent *a = &s->a[s->order[k]];
        if (!a->alive) continue;

        int bx = a->x, by = a->y;
        float best = s->sugar[a->y * w + a->x];
        int bestd = 0;
        uint32_t tie = 1;

        static const int DX[4] = {1, -1, 0, 0};
        static const int DY[4] = {0, 0, 1, -1};

        for (int d = 0; d < 4; ++d) {
            for (int r = 1; r <= a->vision; ++r) {
                int x = a->x + DX[d] * r;
                int y = a->y + DY[d] * r;
                if (x < 0) x += w;
                if (x >= w) x -= w;
                if (y < 0) y += h;
                if (y >= h) y -= h;
                const int i = y * w + x;
                if (s->occ[i] >= 0) continue;   /* occupied sites are not options */
                const float su = s->sugar[i];
                if (su > best || (su == best && r < bestd)) {
                    best = su;
                    bx = x;
                    by = y;
                    bestd = r;
                    tie = 1;
                } else if (su == best && r == bestd) {
                    /* ties go to a uniformly random one of the tied sites */
                    tie++;
                    if (pcg32_below(&wo->rng, tie) == 0) { bx = x; by = y; }
                }
            }
        }

        const int from = a->y * w + a->x;
        const int to = by * w + bx;
        if (to != from) {
            s->occ[from] = -1;
            s->occ[to] = s->order[k];
            a->x = bx;
            a->y = by;
        }

        a->wealth += s->sugar[to];
        s->sugar[to] = 0.0f;
        a->wealth -= (float)a->metab;

        if (a->wealth < 0.0f) {
            a->alive = 0;
            s->occ[to] = -1;
        }
    }

    /* compact the dead out */
    {
        int k = 0;
        for (int i = 0; i < s->na; ++i) {
            if (!s->a[i].alive) continue;
            if (k != i) {
                s->a[k] = s->a[i];
                s->occ[s->a[k].y * w + s->a[k].x] = k;
            }
            k++;
        }
        s->na = k;
    }

    /* observables: the population finds its own carrying capacity, and the
     * wealth distribution goes lopsided all by itself */
    double tw = 0.0, tv = 0.0;
    for (int i = 0; i < s->na; ++i) {
        s->wbuf[i] = s->a[i].wealth;
        tw += s->a[i].wealth;
        tv += s->a[i].vision;
    }
    wo->obs[0] = (float)s->na;
    wo->obs[1] = s->na ? gini(s->wbuf, s->na) : 0.0f;   /* note: gini() sorts wbuf */
    wo->obs[2] = s->na ? (float)(tw / s->na) : 0.0f;
    wo->obs[3] = s->na ? (float)(tv / s->na) : 0.0f;
}

static void sug_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        /* blue: the sugar left in the ground. black: the people standing on it. */
        const float su = s->cap[i] > 0.0f ? s->sugar[i] / 4.0f : 0.0f;
        out[i * 4 + 0] = s->occ[i] >= 0 ? 255 : 0;
        out[i * 4 + 1] = (uint8_t)(su * 190.0f);
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = 0;
    }
}

const Model MODEL_SUGARSCAPE = {
    .id = "sugarscape",
    .name = "Sugarscape",
    .def_w = 128,
    .def_h = 128,
    .n_inks = 2,
    .ink_names = {"agents", "sugar", 0, 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"population", "gini", "meanWealth", "meanVision", 0, 0},
    .n_obs = 4,
    .mem = sug_mem,
    .init = sug_init,
    .step = sug_step,
    .ink = sug_ink,
    .paint = 0,
};
