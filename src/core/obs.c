#include "obs.h"

#include <math.h>
#include <stdlib.h>

float shannon(const int *counts, int k, int total) {
    if (total <= 0) return 0.0f;
    float h = 0.0f;
    for (int i = 0; i < k; ++i) {
        if (counts[i] <= 0) continue;
        float p = (float)counts[i] / (float)total;
        h -= p * log2f(p);
    }
    return h;
}

float box_dimension(const uint8_t *mask, int w, int h, int *n_out, int *levels_out) {
    /* Scales 1,2,4,...  up to a quarter of the smaller side. Beyond that there
     * are too few boxes for the count to mean anything. */
    int small = (w < h) ? w : h;
    int levels = 0;
    int counts[16];
    int sizes[16];

    for (int e = 1; e <= small / 4 && levels < 16; e <<= 1) {
        int bw = (w + e - 1) / e;
        int bh = (h + e - 1) / e;
        int c = 0;
        for (int by = 0; by < bh; ++by) {
            for (int bx = 0; bx < bw; ++bx) {
                int x0 = bx * e, y0 = by * e;
                int x1 = x0 + e, y1 = y0 + e;
                if (x1 > w) x1 = w;
                if (y1 > h) y1 = h;
                int hit = 0;
                for (int y = y0; y < y1 && !hit; ++y)
                    for (int x = x0; x < x1; ++x)
                        if (mask[y * w + x]) { hit = 1; break; }
                c += hit;
            }
        }
        sizes[levels] = e;
        counts[levels] = c;
        if (n_out) n_out[levels] = c;
        levels++;
    }
    if (levels_out) *levels_out = levels;
    if (levels < 3) return 0.0f;

    /* Least-squares slope of log N against log(1/e), dropping the first and
     * last point: those are the two ends where the finite lattice bites. */
    int lo = 1, hi = levels - 1;
    if (hi - lo < 2) { lo = 0; hi = levels; }
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (int i = lo; i < hi; ++i) {
        if (counts[i] <= 0) continue;
        double x = -log((double)sizes[i]);
        double y = log((double)counts[i]);
        sx += x; sy += y; sxx += x * x; sxy += x * y;
        n++;
    }
    if (n < 2) return 0.0f;
    double denom = n * sxx - sx * sx;
    if (denom == 0.0) return 0.0f;
    return (float)((n * sxy - sx * sy) / denom);
}

static int cmp_f(const void *a, const void *b) {
    float fa = *(const float *)a, fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

float gini(float *x, int n) {
    if (n < 2) return 0.0f;
    qsort(x, (size_t)n, sizeof(float), cmp_f);
    double sum = 0.0, weighted = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += x[i];
        weighted += (double)(i + 1) * x[i];
    }
    if (sum <= 0.0) return 0.0f;
    double g = (2.0 * weighted) / ((double)n * sum) - (double)(n + 1) / (double)n;
    if (g < 0.0) g = 0.0;
    if (g > 1.0) g = 1.0;
    return (float)g;
}

float segregation(const int8_t *type, int w, int h) {
    long same = 0, tot = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int8_t t = type[y * w + x];
            if (t < 0) continue;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0) nx += w;
                    if (nx >= w) nx -= w;
                    if (ny < 0) ny += h;
                    if (ny >= h) ny -= h;
                    int8_t u = type[ny * w + nx];
                    if (u < 0) continue;
                    tot++;
                    if (u == t) same++;
                }
            }
        }
    }
    return tot ? (float)((double)same / (double)tot) : 0.0f;
}
