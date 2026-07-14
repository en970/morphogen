/* Cyclic competition: rock, paper, scissors, played on a lattice.
 *
 * Three species. A beats B, B beats C, C beats A, and nobody is best. In a
 * well-mixed flask this is a losing proposition: the populations oscillate with
 * growing amplitude until one of them hits zero, and then a second one follows
 * it, and you are left with a monoculture. Cyclic dominance, on its own, does
 * not preserve diversity.
 *
 * On a lattice it does, and Reichenbach, Mobilia and Frey showed exactly when.
 * Give the organisms a mobility — a rate at which neighbours swap places — and
 * that one number decides everything. Below a critical mobility the system
 * organises itself into a mesh of rotating spiral waves, each species forever
 * chasing the one it beats and fleeing the one that beats it, and all three
 * survive indefinitely. Above it, the spirals grow larger than the world can
 * hold, the pattern washes out, and two species go extinct.
 *
 * The wavelength of the spirals scales as the square root of the mobility, so
 * the transition is simply the point where the spirals outgrow the dish. Drag
 * the `eps` slider up slowly and you can watch the spiral arms fatten until
 * they no longer fit, and then watch biodiversity collapse. It is a genuine
 * phase transition and you can see it happen.
 *
 * This is not a metaphor for bacteria, incidentally. Kerr and colleagues did it
 * with real E. coli: a colicin-producing strain, a resistant strain, and a
 * sensitive strain, which really do play rock-paper-scissors. In a flask, one
 * strain took over. On a plate, where dispersal is local, all three persisted.
 * Same result, in a Petri dish.
 *
 * Rules, per elementary interaction: pick a site and one of its four
 * neighbours, then apply one of
 *
 *   selection    (rate sigma)  AB -> A0    B is killed, leaving an empty site
 *   reproduction (rate mu)     A0 -> AA    an empty site is colonised
 *   exchange     (rate eps)    XY -> YX    the two swap, for any X, Y
 *
 * cyclically in A -> B -> C -> A. One generation is L^2 such interactions.
 *
 * References
 *   Reichenbach, T., Mobilia, M. & Frey, E. "Mobility promotes and jeopardizes
 *     biodiversity in rock-paper-scissors games." Nature 448, 1046-1049 (2007).
 *   Kerr, B., Riley, M. A., Feldman, M. W. & Bohannan, B. J. M. "Local dispersal
 *     promotes biodiversity in a real-life game of rock-paper-scissors." Nature
 *     418, 171-174 (2002).
 */
#include "../model.h"
#include "../field.h"

enum { P_EPS = 0, P_SIGMA, P_MU, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"eps",   P_FLOAT, 0.0f, 60.0f, 0.5f, 3.0f, 0, "exchange rate: how far organisms wander. the whole phase transition lives on this slider."},
    {"sigma", P_FLOAT, 0.1f,  4.0f, 0.1f, 1.0f, 0, "selection rate: how fast the dominant species kills."},
    {"mu",    P_FLOAT, 0.1f,  4.0f, 0.1f, 1.0f, 0, "reproduction rate: how fast an empty site is recolonised."},
};

typedef struct {
    Lat lat;
    uint8_t *s;  /* 0 = empty, 1..3 = species */
} St;

static size_t rps_mem(int w, int h) {
    return (size_t)w * h + 4096 + (size_t)(w + h) * 4 * sizeof(int);
}

static void rps_init(World *wo) {
    St *st = (St *)w_alloc(wo, sizeof(St));
    wo->st = st;
    const int n = wo->w * wo->h;
    lat_init(wo, &st->lat, BC_TORUS);
    st->s = (uint8_t *)w_alloc(wo, (size_t)n);
    for (int i = 0; i < n; ++i) st->s[i] = (uint8_t)pcg32_below(&wo->rng, 4);
}

/* Does a beat b, in the cycle 1 -> 2 -> 3 -> 1? */
static inline int beats(int a, int b) {
    return (a == 1 && b == 2) || (a == 2 && b == 3) || (a == 3 && b == 1);
}

static void rps_step(World *wo) {
    St *st = (St *)wo->st;
    const Lat *L = &st->lat;
    const int w = L->w, h = L->h, n = w * h;

    const float sigma = wo->p[P_SIGMA];
    const float mu = wo->p[P_MU];
    const float eps = wo->p[P_EPS];

    /* The model is defined by rates, not by a synchronous sweep: the update is
     * asynchronous, over randomly chosen neighbouring pairs. Sweeping the
     * lattice in order instead would be a different model with different
     * physics, and it is a common way to get this wrong.
     *
     * The reactions and the swaps are counted separately, and this matters. If
     * you draw all three events from one pool with weights sigma : mu : eps,
     * then cranking the mobility up to eps = 55 means 96 per cent of your events
     * are swaps and the selection clock nearly stops — so the ecology looks
     * stable at high mobility for the entirely artificial reason that you are no
     * longer simulating much of it. Reactions run at a fixed rate per
     * generation; mobility adds swaps on top. */
    const float preact = sigma / (sigma + mu);

    for (int k = 0; k < n; ++k) {
        const int x = (int)pcg32_below(&wo->rng, (uint32_t)w);
        const int y = (int)pcg32_below(&wo->rng, (uint32_t)h);
        const int i = y * w + x;

        int nx = x, ny = y;
        switch (pcg32_below(&wo->rng, 4)) {
            case 0: nx = L->xm[x]; break;
            case 1: nx = L->xp[x]; break;
            case 2: ny = L->ym[y]; break;
            default: ny = L->yp[y]; break;
        }
        const int j = ny * w + nx;
        const int a = st->s[i], b = st->s[j];

        if (pcg32_f(&wo->rng) < preact) {
            if (a && b && beats(a, b)) st->s[j] = 0;
            else if (a && b && beats(b, a)) st->s[i] = 0;
        } else {
            if (a && !b) st->s[j] = (uint8_t)a;
            else if (!a && b) st->s[i] = (uint8_t)b;
        }
    }

    /* Exchange: this is the diffusion, and it is the only thing on the slider. */
    long nswap = (long)((double)n * (double)eps / (double)(sigma + mu));
    if (nswap > 24L * n) nswap = 24L * n;   /* a compute cap, not a physical one */
    for (long k = 0; k < nswap; ++k) {
        const int x = (int)pcg32_below(&wo->rng, (uint32_t)w);
        const int y = (int)pcg32_below(&wo->rng, (uint32_t)h);
        const int i = y * w + x;
        int nx = x, ny = y;
        switch (pcg32_below(&wo->rng, 4)) {
            case 0: nx = L->xm[x]; break;
            case 1: nx = L->xp[x]; break;
            case 2: ny = L->ym[y]; break;
            default: ny = L->yp[y]; break;
        }
        const int j = ny * w + nx;
        const uint8_t t = st->s[i];
        st->s[i] = st->s[j];
        st->s[j] = t;
    }

    int c[4] = {0, 0, 0, 0};
    for (int i = 0; i < n; ++i) c[st->s[i]]++;
    wo->obs[0] = (float)c[1] / (float)n;
    wo->obs[1] = (float)c[2] / (float)n;
    wo->obs[2] = (float)c[3] / (float)n;
    /* The paper's dimensionless mobility, M = 2*eps/N. The critical value is
     * around 4.5e-4; watch what happens as the readout crosses it. */
    wo->obs[3] = 2.0f * eps / (float)n;
}

static void rps_ink(World *wo, uint8_t *out) {
    St *st = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        const int s = st->s[i];
        out[i * 4 + 0] = s == 1 ? 255 : 0;
        out[i * 4 + 1] = s == 2 ? 255 : 0;
        out[i * 4 + 2] = s == 3 ? 255 : 0;
        out[i * 4 + 3] = 0;
    }
}

static void rps_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *st = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0) x += w;
            if (x >= w) x -= w;
            if (y < 0) y += h;
            if (y >= h) y -= h;
            st->s[y * w + x] = erase ? 0 : (uint8_t)(1 + pcg32_below(&wo->rng, 3));
        }
    }
}

const Model MODEL_RPS = {
    .id = "rps",
    .name = "Cyclic competition",
    .def_w = 256,
    .def_h = 256,
    .n_inks = 3,
    .ink_names = {"rock", "paper", "scissors", 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0xd4573f, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"rock", "paper", "scissors", "M", 0, 0},
    .n_obs = 4,
    .mem = rps_mem,
    .init = rps_init,
    .step = rps_step,
    .ink = rps_ink,
    .paint = rps_paint,
};
