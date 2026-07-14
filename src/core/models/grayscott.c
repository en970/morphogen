/* Gray-Scott reaction-diffusion.
 *
 * Two chemicals. U is fed into the dish at a constant rate and V eats it,
 * turning U into more V:
 *
 *     U + 2V -> 3V          (V is autocatalytic: it needs itself to be made)
 *     V      -> P           (V decays into an inert product and is removed)
 *
 * which gives
 *
 *     du/dt = Du*lap(u) - u*v^2 + F*(1 - u)
 *     dv/dt = Dv*lap(v) + u*v^2 - (F + k)*v
 *
 * That is the entire model. There is no notion of a cell, an organism, a
 * boundary, or a rule. And yet, depending on just two numbers — the feed rate F
 * and the kill rate k — it produces spots that divide like dividing cells,
 * fingerprints, coral, mazes, and localised blobs that crawl around the dish,
 * collide, and annihilate.
 *
 * Pearson's 1993 Science paper mapped the (F, k) plane and labelled the regimes
 * with Greek letters. The lab ships his coordinates as presets. The one to look
 * at first is lambda, at F = 0.026, k = 0.061: a spot grows, elongates, pinches
 * in the middle, and becomes two spots, which then do it again. It is mitosis,
 * performed by a differential equation that has never heard of a cell.
 *
 * Two implementation notes that matter.
 *
 * Pearson is explicit that the patterns arise only in response to a
 * finite-amplitude perturbation: start the dish perfectly uniform and it stays
 * perfectly uniform forever, because the homogeneous state is linearly stable.
 * You have to poke it. So the initial condition is u = 1, v = 0 everywhere with
 * a seeded square in the middle, plus a little noise — and if you clear the
 * canvas you get nothing at all until you paint something, which is the correct
 * behaviour and is worth noticing.
 *
 * Du = 2*Dv is not arbitrary. The inhibitor has to outrun the activator or the
 * pattern cannot select a wavelength; this is Turing's condition, and it is the
 * same "local activation, long-range inhibition" that gives a leopard its
 * spots.
 *
 * References
 *   Pearson, J. E. "Complex patterns in a simple system." Science 261, 189-192
 *     (1993). arXiv:patt-sol/9304003.
 *   Gray, P. & Scott, S. K. "Autocatalytic reactions in the isothermal,
 *     continuous stirred tank reactor." Chem. Eng. Sci. 38, 29-43 (1983).
 *   Munafo, R. "Reaction-Diffusion by the Gray-Scott Model: Pearson's
 *     Parametrization." mrob.com/pub/comp/xmorphia — an exhaustive reproduction
 *     of Pearson's figure 3, and the source of several presets here.
 *   Turing, A. M. "The chemical basis of morphogenesis." Phil. Trans. R. Soc. B
 *     237, 37-72 (1952).
 */
#include "../model.h"
#include "../field.h"

#include <math.h>

enum { P_F = 0, P_K, P_DU, P_DV, P_ITER, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"F",     P_FLOAT, 0.000f, 0.110f, 0.0005f, 0.026f, 0, "feed rate. U is replenished at this rate."},
    {"k",     P_FLOAT, 0.030f, 0.075f, 0.0005f, 0.061f, 0, "kill rate. V is removed at rate F+k."},
    {"Du",    P_FLOAT, 0.05f,  0.40f,  0.005f,  0.16f,  0, "diffusivity of U."},
    {"Dv",    P_FLOAT, 0.02f,  0.20f,  0.005f,  0.08f,  0, "diffusivity of V. must be well below Du or no pattern can form."},
    {"iters", P_INT,   1.0f,   40.0f,  1.0f,   16.0f,   0, "solver steps per displayed generation. the patterns take thousands of steps to develop, so this is how impatient you are."},
};

typedef struct {
    Lat lat;
    float *u, *v, *lu, *lv;
} St;

static size_t gs_mem(int w, int h) {
    return (size_t)w * h * 4 * sizeof(float) + 4096 + (size_t)(w + h) * 4 * sizeof(int);
}

static void gs_seed_square(World *wo, St *s) {
    const int w = wo->w, h = wo->h;
    int r = w / 16;
    if (r < 4) r = 4;
    for (int y = h / 2 - r; y < h / 2 + r; ++y) {
        for (int x = w / 2 - r; x < w / 2 + r; ++x) {
            int i = y * w + x;
            s->u[i] = 0.50f;
            s->v[i] = 0.25f;
        }
    }
    /* Pearson perturbs by about one per cent. Without this the seeded square is
     * perfectly symmetric and stays perfectly symmetric, which is a beautiful
     * thing to see once and useless thereafter. */
    const int n = w * h;
    for (int i = 0; i < n; ++i) {
        s->u[i] += 0.02f * (pcg32_f(&wo->rng) - 0.5f);
        s->v[i] += 0.02f * (pcg32_f(&wo->rng) - 0.5f);
        if (s->u[i] < 0.0f) s->u[i] = 0.0f;
        if (s->v[i] < 0.0f) s->v[i] = 0.0f;
    }
}

static void gs_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int n = wo->w * wo->h;

    lat_init(wo, &s->lat, BC_TORUS); /* Pearson uses periodic boundaries */

    s->u = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->v = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->lu = (float *)w_alloc(wo, (size_t)n * sizeof(float));
    s->lv = (float *)w_alloc(wo, (size_t)n * sizeof(float));

    for (int i = 0; i < n; ++i) { s->u[i] = 1.0f; s->v[i] = 0.0f; }
    gs_seed_square(wo, s);
}

static void gs_step(World *wo) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    const float F = wo->p[P_F], k = wo->p[P_K];
    const float Du = wo->p[P_DU], Dv = wo->p[P_DV];
    const int iters = (int)wo->p[P_ITER];

    for (int it = 0; it < iters; ++it) {
        /* The Laplacians are wanted, not applied, so diffuse() is not what we
         * need here: the reaction term has to be added in the same update. */
        const Lat *L = &s->lat;
        const int w = L->w, h = L->h;
        for (int y = 0; y < h; ++y) {
            const int r0 = L->ym[y] * w, r1 = y * w, r2 = L->yp[y] * w;
            for (int x = 0; x < w; ++x) {
                const int a = L->xm[x], b = L->xp[x];
                const int i = r1 + x;
                s->lu[i] = 0.20f * (s->u[r1 + a] + s->u[r1 + b] + s->u[r0 + x] + s->u[r2 + x]) +
                           0.05f * (s->u[r0 + a] + s->u[r0 + b] + s->u[r2 + a] + s->u[r2 + b]) -
                           s->u[i];
                s->lv[i] = 0.20f * (s->v[r1 + a] + s->v[r1 + b] + s->v[r0 + x] + s->v[r2 + x]) +
                           0.05f * (s->v[r0 + a] + s->v[r0 + b] + s->v[r2 + a] + s->v[r2 + b]) -
                           s->v[i];
            }
        }
        for (int i = 0; i < n; ++i) {
            const float u = s->u[i], v = s->v[i];
            const float uvv = u * v * v;
            float nu = u + Du * s->lu[i] - uvv + F * (1.0f - u);
            float nv = v + Dv * s->lv[i] + uvv - (F + k) * v;
            if (nu < 0.0f) nu = 0.0f;
            if (nu > 1.0f) nu = 1.0f;
            if (nv < 0.0f) nv = 0.0f;
            if (nv > 1.0f) nv = 1.0f;
            s->u[i] = nu;
            s->v[i] = nv;
        }
    }

    double sv = 0.0, su = 0.0, svv = 0.0;
    int active = 0;
    for (int i = 0; i < n; ++i) {
        sv += s->v[i];
        svv += (double)s->v[i] * s->v[i];
        su += s->u[i];
        if (s->v[i] > 0.15f) active++;
    }
    const double mean = sv / n;
    double var = svv / n - mean * mean;
    if (var < 0.0) var = 0.0;

    wo->obs[0] = (float)mean;
    wo->obs[1] = (float)(su / n);
    wo->obs[2] = (float)active / (float)n;

    /* The spatial standard deviation of V, and it is the observable that actually
     * answers "is there a pattern here?".
     *
     * The mean of V does not. It is high when the dish is full of structure and it
     * is equally high when the dish is uniformly full of V and there is no
     * structure at all — so a phase map coloured by the mean shows a smooth
     * gradient where the truth is a sharp band. The standard deviation is zero in
     * *both* uniform states, empty and full, and large only where the field varies
     * from place to place, which is what a pattern is. */
    wo->obs[3] = (float)sqrt(var);
}

static void gs_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int n = wo->w * wo->h;
    for (int i = 0; i < n; ++i) {
        /* V is the thing that looks alive, so V gets the ink. U is printed
         * faintly in blue underneath so you can see the substrate it is eating. */
        float v = s->v[i] * 3.2f;
        if (v > 1.0f) v = 1.0f;
        float u = 1.0f - s->u[i];
        if (u < 0.0f) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        out[i * 4 + 0] = (uint8_t)(v * 255.0f);
        out[i * 4 + 1] = (uint8_t)(u * 90.0f);
        out[i * 4 + 2] = 0;
        out[i * 4 + 3] = 0;
    }
}

static void gs_paint(World *wo, int cx, int cy, int radius, int erase) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0) x += w;
            if (x >= w) x -= w;
            if (y < 0) y += h;
            if (y >= h) y -= h;
            int i = y * w + x;
            if (erase) { s->u[i] = 1.0f; s->v[i] = 0.0f; }
            else       { s->u[i] = 0.50f; s->v[i] = 0.25f; }
        }
    }
}

const Model MODEL_GRAYSCOTT = {
    .id = "grayscott",
    .name = "Gray-Scott",
    .def_w = 320,
    .def_h = 320,
    .n_inks = 2,
    .ink_names = {"V", "U", 0, 0},
    .ink_colors = {0x2b2b2b, 0x81acec, 0x000000, 0x000000},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"V", "U", "active", "structure", 0, 0},
    .n_obs = 4,
    .mem = gs_mem,
    .init = gs_init,
    .step = gs_step,
    .ink = gs_ink,
    .paint = gs_paint,
};
