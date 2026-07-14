#include "field.h"

void lat_init(World *wo, Lat *L, int bc) {
    int w = wo->w, h = wo->h;
    L->w = w;
    L->h = h;
    L->n = w * h;
    L->xm = (int *)w_alloc(wo, (size_t)w * sizeof(int));
    L->xp = (int *)w_alloc(wo, (size_t)w * sizeof(int));
    L->ym = (int *)w_alloc(wo, (size_t)h * sizeof(int));
    L->yp = (int *)w_alloc(wo, (size_t)h * sizeof(int));

    for (int x = 0; x < w; ++x) {
        if (bc == BC_TORUS) {
            L->xm[x] = (x == 0) ? w - 1 : x - 1;
            L->xp[x] = (x == w - 1) ? 0 : x + 1;
        } else { /* no-flux: the edge is its own neighbour, so the gradient
                  * across the wall is zero and nothing leaks out */
            L->xm[x] = (x == 0) ? 0 : x - 1;
            L->xp[x] = (x == w - 1) ? w - 1 : x + 1;
        }
    }
    for (int y = 0; y < h; ++y) {
        if (bc == BC_TORUS) {
            L->ym[y] = (y == 0) ? h - 1 : y - 1;
            L->yp[y] = (y == h - 1) ? 0 : y + 1;
        } else {
            L->ym[y] = (y == 0) ? 0 : y - 1;
            L->yp[y] = (y == h - 1) ? h - 1 : y + 1;
        }
    }
}

static void lap_into(const Lat *L, const float *f, float *out) {
    const int w = L->w, h = L->h;
    const int *xm = L->xm, *xp = L->xp, *ym = L->ym, *yp = L->yp;

    for (int y = 0; y < h; ++y) {
        const int r0 = ym[y] * w;
        const int r1 = y * w;
        const int r2 = yp[y] * w;
        for (int x = 0; x < w; ++x) {
            const int a = xm[x], b = xp[x];
            const float orth = f[r1 + a] + f[r1 + b] + f[r0 + x] + f[r2 + x];
            const float diag = f[r0 + a] + f[r0 + b] + f[r2 + a] + f[r2 + b];
            out[r1 + x] = 0.20f * orth + 0.05f * diag - f[r1 + x];
        }
    }
}

void diffuse(const Lat *L, float *f, float *tmp, float rate) {
    lap_into(L, f, tmp);
    const int n = L->n;
    for (int i = 0; i < n; ++i) f[i] += rate * tmp[i];
}

void diffuse_decay(const Lat *L, float *f, float *tmp, float rate, float lambda) {
    lap_into(L, f, tmp);
    const int n = L->n;
    for (int i = 0; i < n; ++i) {
        f[i] += rate * tmp[i] - lambda * f[i];
        if (f[i] < 0.0f) f[i] = 0.0f;
    }
}
