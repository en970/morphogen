/* Langton's ant, and turmites generally.
 *
 * An ant walks on a grid of coloured cells. It has no memory, no plan, and no
 * parameters. On a white cell it turns right, flips the cell to black, and
 * steps forward. On a black cell it turns left, flips the cell to white, and
 * steps forward. That is the complete rule; there is nothing else to it, and it
 * is the shortest interesting program in this lab by a wide margin.
 *
 * Run it on an empty grid and three things happen, in order.
 *
 * For the first few hundred steps the trail is small and roughly symmetric.
 * Then, for about ten thousand steps, it is a mess: a growing, chaotic blot
 * with no discernible structure, and if you did not know better you would say
 * the ant was simply drawing noise forever.
 *
 * And then, without anything changing, it stops. The ant falls into a cycle of
 * 104 steps that displaces it two cells diagonally, and it builds a perfectly
 * straight, perfectly periodic highway, and it goes on building that highway
 * for ever.
 *
 * Nobody put the highway in the rule. Nobody can currently look at the rule and
 * predict the highway; the only way to find out that it is there is to run the
 * ant. Bunimovich and Troubetzkoy proved that the trajectory must be unbounded
 * — the ant cannot stay in a finite region — but the highway itself is, as far
 * as anyone knows, simply a fact about this rule that has to be discovered
 * empirically. Watch it happen once. Order, arriving from nowhere in
 * particular, with no parameter tuned to make it arrive.
 *
 * The generalisation is a turmite: a rule string over {L, R, N, U} with one
 * symbol per colour. On colour c the ant turns by the c-th symbol, sets the
 * cell to colour c+1, and moves. `RL` is Langton's ant. `RLR` fills space with
 * a chaotic but bounded-looking growth. `LLRR` grows a symmetric cardioid.
 * `LRRRRRLLR` builds a highway too, after a much longer wait.
 *
 * References
 *   Langton, C. G. "Studying artificial life with cellular automata." Physica D
 *     22, 120-149 (1986).
 *   Bunimovich, L. A. & Troubetzkoy, S. E. "Recurrence properties of Lorentz
 *     lattice gas cellular automata." J. Stat. Phys. 67, 289-302 (1992).
 */
#include "../model.h"

#include <string.h>

enum { P_RULE = 0, P_ANTS, P_SPEED, P_NPARAM };

/* The turmite rule, as an index into a table of strings. Exposing it as free
 * text would be nicer and is a job for the panel, not the kernel. */
static const char *RULES[] = {
    "RL",          /* Langton's ant */
    "RLR",
    "LLRR",
    "LRRRRRLLR",
    "RRLLLRLLLRRR",
    "LLRRRLRLRLLR",
};
#define N_RULES 6

static const ParamDef PARAMS[] = {
    {"rule",  P_ENUM,  0.0f, (float)(N_RULES - 1), 1.0f, 0.0f,
     "RL|RLR|LLRR|LRRRRRLLR|RRLLLRLLLRRR|LLRRRLRLRLLR",
     "the turmite rule. one symbol per colour: R turn right, L turn left. RL is Langton's ant."},
    {"ants",  P_INT,   1.0f, 8.0f, 1.0f, 1.0f, 0, "how many ants. two ants will eventually erase each other's highways."},
    {"speed", P_INT,   1.0f, 400.0f, 1.0f, 60.0f, 0, "ant steps per generation. the highway needs about 10,000 steps; be patient or turn this up."},
};

#define MAX_ANTS 8

typedef struct {
    uint8_t *c;   /* cell colour */
    int32_t ax[MAX_ANTS], ay[MAX_ANTS], ad[MAX_ANTS]; /* 0=N 1=E 2=S 3=W */
    int n_colors;
    const char *rule;
    long steps;
} St;

static size_t ant_mem(int w, int h) { return (size_t)w * h + 4096; }

static void ant_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int w = wo->w, h = wo->h;

    s->c = (uint8_t *)w_alloc(wo, (size_t)w * h);
    s->rule = RULES[(int)wo->p[P_RULE]];
    s->n_colors = (int)strlen(s->rule);
    s->steps = 0;

    const int na = (int)wo->p[P_ANTS];
    for (int i = 0; i < na; ++i) {
        if (i == 0) {
            s->ax[i] = w / 2;
            s->ay[i] = h / 2;
            s->ad[i] = 0;
        } else {
            s->ax[i] = (int)pcg32_below(&wo->rng, (uint32_t)w);
            s->ay[i] = (int)pcg32_below(&wo->rng, (uint32_t)h);
            s->ad[i] = (int)pcg32_below(&wo->rng, 4);
        }
    }
}

static void ant_step(World *wo) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    const int na = (int)wo->p[P_ANTS];
    const int speed = (int)wo->p[P_SPEED];
    const int nc = s->n_colors;

    static const int DX[4] = {0, 1, 0, -1};
    static const int DY[4] = {-1, 0, 1, 0};

    for (int k = 0; k < speed; ++k) {
        for (int a = 0; a < na; ++a) {
            const int i = s->ay[a] * w + s->ax[a];
            const int col = s->c[i];
            const char turn = s->rule[col % nc];

            switch (turn) {
                case 'R': s->ad[a] = (s->ad[a] + 1) & 3; break;
                case 'L': s->ad[a] = (s->ad[a] + 3) & 3; break;
                case 'U': s->ad[a] = (s->ad[a] + 2) & 3; break;
                default: break; /* 'N': carry straight on */
            }

            s->c[i] = (uint8_t)((col + 1) % nc);

            int x = s->ax[a] + DX[s->ad[a]];
            int y = s->ay[a] + DY[s->ad[a]];
            /* The ant's path is provably unbounded, so a finite grid must wrap.
             * Left long enough the highway will drive off one edge and come back
             * in the other, and eventually crash into its own road. */
            if (x < 0) x += w;
            if (x >= w) x -= w;
            if (y < 0) y += h;
            if (y >= h) y -= h;
            s->ax[a] = x;
            s->ay[a] = y;
        }
        s->steps++;
    }

    long painted = 0;
    const int n = w * h;
    for (int i = 0; i < n; ++i) painted += (s->c[i] != 0);
    wo->obs[0] = (float)s->steps;
    wo->obs[1] = (float)painted;
}

static void ant_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h, n = w * h;
    const int nc = s->n_colors;
    const int na = (int)wo->p[P_ANTS];

    for (int i = 0; i < n; ++i) {
        const int c = s->c[i];
        /* Colour 0 is bare paper. The rest ramp up in ink density, so a
         * many-colour turmite prints as a grey scale. */
        out[i * 4 + 0] = c ? (uint8_t)(70 + 185 * c / (nc > 1 ? nc : 1)) : 0;
        out[i * 4 + 1] = 0;
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = 0;
    }
    /* The ant itself, in red, so you can find it. */
    for (int a = 0; a < na; ++a) {
        const int i = s->ay[a] * w + s->ax[a];
        if (i >= 0 && i < n) out[i * 4 + 2] = 255;
    }
}

static void ant_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            const int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= w || y < 0 || y >= h) continue;
            s->c[y * w + x] = erase ? 0 : 1;
        }
}

const Model MODEL_ANT = {
    .id = "ant",
    .name = "Langton's ant",
    .def_w = 320,
    .def_h = 320,
    .n_inks = 3,
    .ink_names = {"trail", "", "ant", 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0xd4573f, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"steps", "painted", 0, 0, 0, 0},
    .n_obs = 2,
    .mem = ant_mem,
    .init = ant_init,
    .step = ant_step,
    .ink = ant_ink,
    .paint = ant_paint,
};
