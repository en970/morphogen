/* Life-like cellular automata: the whole B/S rulespace, not just Conway's.
 *
 * A cell counts its eight neighbours. If it is dead it is born when that count
 * is in the birth set B; if it is alive it survives when the count is in the
 * survival set S; otherwise it dies. Conway's rule is B3/S23. There are 2^18
 * such rules and this one kernel runs all of them, because B and S are just two
 * nine-bit masks and the rule reduces to
 *
 *     next = (alive ? (S >> n) : (B >> n)) & 1
 *
 * with no branch in sight. Switching rules is a store of two integers, not a
 * recompile, which is what makes the rulespace explorable rather than a list.
 *
 * Worth trying, and shipped as presets:
 *
 *   B3/S23          Conway's Life. Gliders, guns, universal computation.
 *   B36/S23         HighLife, which contains a replicator: a small pattern that
 *                   makes copies of itself. Self-reproduction, in a rule that
 *                   differs from Conway's by one digit.
 *   B2/S            Seeds. Nothing ever survives; everything is born. The result
 *                   is not death but an explosion.
 *   B3/S012345678   Life without Death. Nothing ever dies. Grows mazes forever.
 *   B4678/S35678    Anneal. Domains coarsen and their boundaries shorten, as if
 *                   the pattern had surface tension. It does not. It has a rule.
 *   B3/S45678       Coral. Slow dendritic growth — a preview of the colony model.
 *
 * A note on what is not here. This is a straightforward byte-per-cell kernel
 * with neighbour-index tables, not a bit-packed SWAR one (which does 64 cells
 * per word) and not HashLife (which runs Life for 2^60 generations by
 * memoising the quadtree). Both are wonderful and both are the wrong tool: at
 * the grid sizes a halftone screen can actually resolve, this kernel is already
 * far below the frame budget, and it stays readable, which for the pedagogical
 * centrepiece of the lab is worth more than the speed.
 *
 * References
 *   Gardner, M. "Mathematical Games: The fantastic combinations of John Conway's
 *     new solitaire game 'life'." Scientific American 223(4), 120-123 (1970).
 *   Berlekamp, E. R., Conway, J. H. & Guy, R. K. Winning Ways for Your
 *     Mathematical Plays, Vol. 2. Academic Press (1982). — the universality proof.
 */
#include "../model.h"
#include "../field.h"

enum { P_B = 0, P_S, P_DENSITY, P_WRAP, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"B",       P_INT,  0.0f, 511.0f, 1.0f,   8.0f, 0, "birth mask. bit n set means a dead cell with n live neighbours is born. 8 = B3."},
    {"S",       P_INT,  0.0f, 511.0f, 1.0f,  12.0f, 0, "survival mask. bit n set means a live cell with n live neighbours survives. 12 = S23."},
    {"density", P_FLOAT, 0.0f, 1.0f,  0.01f,  0.35f, 0, "fraction of cells alive in the initial random soup."},
    {"wrap",    P_ENUM, 0.0f, 1.0f,   1.0f,   0.0f, "torus|dead", "torus: the grid wraps, and a glider comes back to meet its own wreckage."},
};

typedef struct {
    Lat lat;
    uint8_t *cur, *nxt;
    int pop, activity;
} St;

static size_t life_mem(int w, int h) {
    return (size_t)w * h * 2 + 4096 + (size_t)(w + h) * 4 * sizeof(int);
}

static void life_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int n = wo->w * wo->h;

    lat_init(wo, &s->lat, (int)wo->p[P_WRAP] == 0 ? BC_TORUS : BC_NOFLUX);

    s->cur = (uint8_t *)w_alloc(wo, (size_t)n);
    s->nxt = (uint8_t *)w_alloc(wo, (size_t)n);

    const float d = wo->p[P_DENSITY];
    for (int i = 0; i < n; ++i) s->cur[i] = pcg32_f(&wo->rng) < d ? 1 : 0;
}

static void life_step(World *wo) {
    St *s = (St *)wo->st;
    const Lat *L = &s->lat;
    const int w = L->w, h = L->h;
    const uint32_t B = (uint32_t)wo->p[P_B];
    const uint32_t S = (uint32_t)wo->p[P_S];
    const int dead_edge = (int)wo->p[P_WRAP] != 0;

    const uint8_t *c = s->cur;
    uint8_t *nx = s->nxt;
    int pop = 0, act = 0;

    for (int y = 0; y < h; ++y) {
        const int r0 = L->ym[y] * w, r1 = y * w, r2 = L->yp[y] * w;
        /* Under a dead boundary the lattice tables clamp instead of wrapping,
         * which would make an edge cell count itself as its own neighbour. So
         * the edge rows and columns are simply held dead. A real dish has a
         * wall; this is that wall. */
        const int edge_row = dead_edge && (y == 0 || y == h - 1);
        for (int x = 0; x < w; ++x) {
            const int i = r1 + x;
            if (edge_row || (dead_edge && (x == 0 || x == w - 1))) {
                nx[i] = 0;
                continue;
            }
            const int a = L->xm[x], b = L->xp[x];
            const int n = c[r1 + a] + c[r1 + b] + c[r0 + a] + c[r0 + x] +
                          c[r0 + b] + c[r2 + a] + c[r2 + x] + c[r2 + b];
            const uint8_t alive = c[i];
            const uint8_t next = (uint8_t)(((alive ? S : B) >> n) & 1u);
            nx[i] = next;
            pop += next;
            act += (next != alive);
        }
    }

    uint8_t *t = s->cur;
    s->cur = s->nxt;
    s->nxt = t;

    s->pop = pop;
    s->activity = act;
    wo->obs[0] = (float)pop;
    wo->obs[1] = (float)pop / (float)(w * h);
    /* Activity — how many cells changed — is the cheapest useful observable in
     * the whole lab. It goes to zero when the pattern freezes, saturates when
     * the rule is chaotic, and for the interesting rules it stays intermittent:
     * quiet, then a glider hits something, then quiet again. */
    wo->obs[2] = (float)act;
}

static void life_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        out[i * 4 + 0] = s->cur[i] ? 255 : 0;
        out[i * 4 + 1] = 0;
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = 0;
    }
}

static void life_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= w || y < 0 || y >= h) continue;
            s->cur[y * w + x] = erase ? 0 : 1;
        }
    }
}

const Model MODEL_LIFE = {
    .id = "life",
    .name = "Life-like",
    .def_w = 256,
    .def_h = 256,
    .n_inks = 1,
    .ink_names = {"alive", 0, 0, 0},
    .ink_colors = {0x2b2b2b, 0, 0, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"population", "density", "activity", 0, 0, 0},
    .n_obs = 3,
    .mem = life_mem,
    .init = life_init,
    .step = life_step,
    .ink = life_ink,
    .paint = life_paint,
};
