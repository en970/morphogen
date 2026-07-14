/* Lenia.
 *
 * Take Conway's Life and refuse to accept any of its discreteness. The cell's
 * state is not 0 or 1 but a real number in [0,1]. The neighbourhood is not the
 * eight adjacent cells but a smooth, ring-shaped kernel of radius R, and the
 * "neighbour count" is the convolution of that kernel with the field. The birth
 * and survival sets are not {3} and {2,3} but a smooth bump: a growth function
 * G that is positive when the neighbourhood sum is near mu and negative
 * otherwise. And time is not a tick but a small step 1/T.
 *
 *     U = K * A                              (the ring-kernel neighbourhood sum)
 *     G(u) = 2*exp(-(u - mu)^2 / (2*sigma^2)) - 1
 *     A <- clip(A + G(U)/T, 0, 1)
 *
 * Every discrete choice in Life has been dissolved, and something strange
 * survives the dissolution. In a narrow region of the (mu, sigma) plane the
 * field spontaneously organises into localised, coherent, self-maintaining
 * patterns that hold their shape, move at a steady speed in a direction of
 * their own choosing, recover when you damage them, and die if you push the
 * parameters out of their range. Bert Chan found hundreds of them, gave them
 * Linnaean names, and arranged them into a taxonomy, because that turned out to
 * be the natural way to describe what he had.
 *
 * The default here is Orbium unicaudatus, the first and simplest: a
 * self-propelled glider with a single tail. It is not a glider in Conway's
 * sense — it does not travel along a lattice direction at a rational speed,
 * because there is no lattice for it to travel along. It just swims.
 *
 * The thing to do is to take sigma and move it. The region of parameter space
 * in which Orbium survives at all is a thin filament: a little too narrow and
 * it starves and evaporates, a little too wide and it blooms into featureless
 * mush that fills the world. Life, in this system, occupies a knife-edge
 * between extinction and cancer, and you can find the edges of it with a slider
 * in about ten seconds. That is as good a demonstration of the "edge of chaos"
 * as anything in the literature, and unlike most of them, you do not have to
 * take anyone's word for it.
 *
 * References
 *   Chan, B. W.-C. "Lenia - Biology of Artificial Life." Complex Systems 28(3),
 *     251-286 (2019). arXiv:1812.05433.
 *   Chan, B. W.-C. "Lenia and Expanded Universe." ALIFE 2020. arXiv:2005.03742.
 *   Rafler, S. "Generalization of Conway's Game of Life to a continuous domain -
 *     SmoothLife." arXiv:1111.1567 (2011). — the immediate ancestor.
 */
#include "../model.h"
#include "../field.h"

#include <math.h>
#include <string.h>

enum { P_MU = 0, P_SIGMA, P_R, P_T, P_BETA, P_CORE, P_INIT, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"mu",    P_FLOAT, 0.02f, 0.50f, 0.001f, 0.15f, 0, "growth centre. the neighbourhood sum a cell most wants to see."},
    {"sigma", P_FLOAT, 0.001f, 0.10f, 0.0005f, 0.015f, 0, "growth width. the knife-edge: too narrow and life starves, too wide and it turns to mush."},
    {"R",     P_INT,   6.0f, 18.0f, 1.0f, 13.0f, 0, "kernel radius, in cells. the size of a creature's world."},
    {"T",     P_INT,   2.0f, 30.0f, 1.0f, 10.0f, 0, "time resolution. the update adds 1/T of the growth, so larger T is a smaller step."},
    {"beta",  P_ENUM,  0.0f, 3.0f, 1.0f, 0.0f, "1|1/2,1|1/2,1,2/3|1,2/3,1/3",
     "kernel shells: the relative weights of concentric rings. new shells, new species."},
    {"core",  P_ENUM,  0.0f, 1.0f, 1.0f, 0.0f, "poly|exp",
     "kernel and growth shape. poly is what Chan's species were found with; exp is the form the paper writes."},
    {"init",  P_ENUM,  0.0f, 1.0f, 1.0f, 0.0f, "orbium|noise",
     "orbium unicaudatus, or a random field, to see what crawls out of it."},
};

#define MAXR 18
#define MAXTAPS ((2 * MAXR + 1) * (2 * MAXR + 1))

typedef struct {
    Lat lat;
    float *a, *u;
    /* The kernel, flattened into a list of (offset, weight) pairs. Only the
     * cells inside the disc are stored, so the inner loop has no bounds test and
     * no zero-weight taps. */
    int32_t kdx[MAXTAPS], kdy[MAXTAPS];
    float kw[MAXTAPS];
    int ntaps;
    int built_R;
    int built_beta;
    int built_core;
} St;

static size_t lenia_mem(int w, int h) {
    return (size_t)w * h * 2 * sizeof(float) + 8192 + (size_t)(w + h) * 4 * sizeof(int);
}

/* The kernel core: a bump on (0,1), zero at both ends, peaking at r = 1/2.
 *
 * There are two of them in Chan's code and it matters which you use. The paper
 * writes the exponential form, exp(4 - 1/(r(1-r))), and that is the one every
 * secondhand description of Lenia repeats. But the reference implementation
 * defaults to the polynomial (4r(1-r))^4, and every creature in Chan's species
 * list — Orbium included — was found under the polynomial. Load Orbium with the
 * exponential core and it does not glide; it wobbles and dies. Both are offered
 * here, and the default is the one the animals actually live in. */
static float kcore(float r, int core) {
    if (r <= 0.0f || r >= 1.0f) return 0.0f;
    if (core == 1) return expf(4.0f - 1.0f / (r * (1.0f - r)));
    const float q = 4.0f * r * (1.0f - r);
    const float q2 = q * q;
    return q2 * q2;
}

/* The growth function, likewise. Polynomial: a quartic bump of half-width 3s.
 * Exponential: the Gaussian the paper prints. */
static float growth(float u, float mu, float sigma, int core) {
    const float d = u - mu;
    if (core == 1) return 2.0f * expf(-d * d / (2.0f * sigma * sigma)) - 1.0f;
    float q = 1.0f - (d * d) / (9.0f * sigma * sigma);
    if (q < 0.0f) q = 0.0f;
    const float q2 = q * q;
    return 2.0f * q2 * q2 - 1.0f;
}

static void beta_of(int idx, float *b, int *nb) {
    switch (idx) {
        case 1:  b[0] = 0.5f; b[1] = 1.0f; *nb = 2; break;
        case 2:  b[0] = 0.5f; b[1] = 1.0f; b[2] = 2.0f / 3.0f; *nb = 3; break;
        case 3:  b[0] = 1.0f; b[1] = 2.0f / 3.0f; b[2] = 1.0f / 3.0f; *nb = 3; break;
        default: b[0] = 1.0f; *nb = 1; break;
    }
}

static void build_kernel(St *s, int R, int betaIdx, int core) {
    float b[4];
    int nb;
    beta_of(betaIdx, b, &nb);

    s->ntaps = 0;
    double sum = 0.0;

    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            /* r is the distance from the centre, in units of R. The cutoff is on
             * r, not on the shell coordinate. */
            const float r = sqrtf((float)(dx * dx + dy * dy)) / (float)R;
            if (r >= 1.0f) continue;
            const float br = r * (float)nb;
            int shell = (int)br;
            if (shell >= nb) shell = nb - 1;
            const float w = b[shell] * kcore(br - (float)shell, core);
            if (w <= 0.0f) continue;
            s->kdx[s->ntaps] = dx;
            s->kdy[s->ntaps] = dy;
            s->kw[s->ntaps] = w;
            sum += w;
            s->ntaps++;
        }
    }
    /* Normalised to sum to one, so U is a weighted average and mu lives on the
     * same scale as the field itself. */
    if (sum > 0.0)
        for (int i = 0; i < s->ntaps; ++i) s->kw[i] = (float)(s->kw[i] / sum);

    s->built_R = R;
    s->built_beta = betaIdx;
    s->built_core = core;
}

/* Orbium unicaudatus, decoded from the record in Chan's animals.json:
 *
 *   {"code":"O2u", "name":"Orbium unicaudatus",
 *    "params":{"R":13,"T":10,"b":"1","m":0.15,"s":0.015,"kn":1,"gn":1},
 *    "cells":"7.MD6.qL$6.pKqEqFURpApBRAqQ$..."}
 *
 * Twenty by twenty, at 0-255, divided by 255 on load. This is the actual
 * creature, not a redrawing of it: paste these numbers into the reference
 * implementation and you get the same animal. It is placed at R = 13, the
 * radius it was found at; scale R away from 13 and Orbium will not survive,
 * which is the point of the exercise. */
static const uint8_t ORBIUM[20][20] = {
{  0,  0,  0,  0,  0,  0,  0, 13,  4,  0,  0,  0,  0,  0,  0, 60,  0,  0,  0,  0},
{  0,  0,  0,  0,  0,  0, 35, 53, 54, 21, 18, 25, 26, 18,  1, 65,  0,  0,  0,  0},
{  0,  0,  0,  0,  0, 22, 68, 91, 98, 87, 48, 47, 44, 47, 45, 27, 89,  0,  0,  0},
{  0,  0,  0,  0,  3, 17, 89,116,119, 97, 33, 20, 14, 16, 31, 55,180,  0,  0,  0},
{  0,  0,  0,  9, 33, 47, 87,103, 98, 72, 34,  0,  0,  0,  0, 12,102, 84,  0,  0},
{  1,  0,  4, 35, 43, 34, 28, 63, 69, 67, 53,  0,  0,  0,  0,  0,  5,220,  0,  0},
{ 60,  0, 26, 44, 20,  0,  0, 51, 79, 94, 95, 61,  0,  0,  0,  0,  0,116, 40,  0},
{  0, 31, 47, 28,  0,  0,  0, 69,109,129,137,130,  0,  0,  0,  0,  0,  0,132,  0},
{  0,150, 55,  8,  0,  0,  0, 48,135,162,174,174,107,  0,  0,  0,  0,  0,109,  0},
{  0,141, 60,  0,  0,  0,  0,  7,158,193,214,218,206, 27,  0,  0,  0,  0, 72, 25},
{  0,  0,152,  0,  0,  0,  0,  0,170,223,245,253,248,143,  0,  0,  0,  0, 57, 36},
{  0,  0,214,  0,  0,  0,  0,  0,129,247,255,255,255,233, 71,  0,  0,  6, 56, 34},
{  0,  0,141, 19,  0,  0,  0,  0, 85,255,255,250,255,248,142, 40, 13, 30, 62, 22},
{  0,  0,  8,114,  0,  0,  0,  0, 45,217,255,228,220,221,166, 85, 50, 55, 59, 10},
{  0,  0,  0,108, 29,  0,  0,  0, 29,158,224,210,199,189,156,104, 75, 68, 42,  0},
{  0,  0,  0, 20, 85, 19,  0,  0, 30,108,172,184,173,160,134,103, 79, 57, 16,  0},
{  0,  0,  0,  0, 42, 66, 38, 30, 44, 86,127,142,139,127,109, 86, 62, 30,  0,  0},
{  0,  0,  0,  0,  0, 37, 59, 60, 66, 81, 99,108,105, 92, 78, 58, 32,  5,  0,  0},
{  0,  0,  0,  0,  0,  0, 18, 43, 58, 64, 70, 71, 66, 59, 42, 24,  5,  0,  0,  0},
{  0,  0,  0,  0,  0,  0,  0,  0, 15, 26, 33, 34, 30, 20, 11,  0,  0,  0,  0,  0},
};

static void lenia_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int w = wo->w, h = wo->h, n = w * h;

    lat_init(wo, &s->lat, BC_TORUS);
    s->a = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->u = (float *)w_alloc(wo, (size_t)n * sizeof(float));

    build_kernel(s, (int)wo->p[P_R], (int)wo->p[P_BETA], (int)wo->p[P_CORE]);

    if ((int)wo->p[P_INIT] == 0) {
        const int ox = w / 2 - 10, oy = h / 2 - 10;
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 20; ++x) {
                const int px = ox + x, py = oy + y;
                if (px < 0 || px >= w || py < 0 || py >= h) continue;
                s->a[py * w + px] = (float)ORBIUM[y][x] * (1.0f / 255.0f);
            }
    } else {
        /* A random field, smoothed enough that the kernel can get a grip on it.
         * Perfectly white noise convolves to a flat U and simply dies. */
        const int bs = 8;
        for (int y = 0; y < h; y += bs)
            for (int x = 0; x < w; x += bs) {
                const float v = pcg32_f(&wo->rng);
                for (int j = 0; j < bs; ++j)
                    for (int i = 0; i < bs; ++i) {
                        const int px = x + i, py = y + j;
                        if (px >= w || py >= h) continue;
                        s->a[py * w + px] = v;
                    }
            }
    }
}

static void lenia_step(World *wo) {
    St *s = (St *)wo->st;
    const Lat *L = &s->lat;
    const int w = L->w, h = L->h, n = w * h;

    const int R = (int)wo->p[P_R];
    const int betaIdx = (int)wo->p[P_BETA];
    const int core = (int)wo->p[P_CORE];
    if (R != s->built_R || betaIdx != s->built_beta || core != s->built_core)
        build_kernel(s, R, betaIdx, core);

    const float mu = wo->p[P_MU];
    const float sigma = wo->p[P_SIGMA];
    const float invT = 1.0f / wo->p[P_T];

    const int ntaps = s->ntaps;
    const float *a = s->a;
    float *u = s->u;

    /* The convolution. This is the whole cost of the model: a few hundred taps
     * per cell. It is done directly rather than by FFT because at these grid
     * sizes the FFT's constant factor eats the asymptotic win, and because a
     * direct sum is exact and reproducible while an FFT is neither. */
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float acc = 0.0f;
            for (int t = 0; t < ntaps; ++t) {
                int px = x + s->kdx[t];
                int py = y + s->kdy[t];
                if (px < 0) px += w;
                else if (px >= w) px -= w;
                if (py < 0) py += h;
                else if (py >= h) py -= h;
                acc += s->kw[t] * a[py * w + px];
            }
            u[y * w + x] = acc;
        }
    }

    double mass = 0.0;
    for (int i = 0; i < n; ++i) {
        const float g = growth(u[i], mu, sigma, core);
        float v = s->a[i] + invT * g;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        s->a[i] = v;
        mass += v;
    }

    /* Mass is the observable that tells you whether the creature is alive.
     * Bounded and steady: a creature. Falling to zero: it starved. Climbing
     * without limit: the parameters have tipped into unbounded growth and the
     * world is filling up. */
    wo->obs[0] = (float)mass;
    wo->obs[1] = (float)(mass / (double)n);
}

static void lenia_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        float v = s->a[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        out[i * 4 + 0] = (uint8_t)(v * 255.0f);
        out[i * 4 + 1] = 0;
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = 0;
    }
}

static void lenia_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            const float d2 = (float)(dx * dx + dy * dy);
            if (d2 > (float)(radius * radius)) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0) x += w;
            if (x >= w) x -= w;
            if (y < 0) y += h;
            if (y >= h) y -= h;
            const int i = y * w + x;
            if (erase) {
                s->a[i] = 0.0f;
            } else {
                /* a soft blob, since a hard-edged one is not something the
                 * kernel can do much with */
                const float f = 1.0f - sqrtf(d2) / (float)(radius + 1);
                if (f > s->a[i]) s->a[i] = f;
            }
        }
}

const Model MODEL_LENIA = {
    .id = "lenia",
    .name = "Lenia",
    .def_w = 160,
    .def_h = 160,
    .n_inks = 1,
    .ink_names = {"field", 0, 0, 0},
    .ink_colors = {0x2b2b2b, 0, 0, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"mass", "density", 0, 0, 0, 0},
    .n_obs = 2,
    .mem = lenia_mem,
    .init = lenia_init,
    .step = lenia_step,
    .ink = lenia_ink,
    .paint = lenia_paint,
};
