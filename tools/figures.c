/* Generates every number and every image in the paper.
 *
 * It links against the same src/core that the browser runs, so there is no
 * separate "figure code" that could drift away from the thing being described.
 * If a figure in the paper is wrong, the simulation is wrong.
 *
 * Output: CSV into paper/data/ (plotted by pgfplots, so the plots are typeset
 * rather than rasterised) and PGM into paper/fig/ (converted to PNG by
 * tools/pgm2png.py, which uses nothing outside the Python standard library).
 */
#include "world.h"
#include "obs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static World W;

static void boot(void) {
    W.arena = malloc(64u << 20);
    W.arena_cap = 64u << 20;
    W.ink = malloc((size_t)MAX_DIM * MAX_DIM * 4);
}

static const Model *M(const char *id) {
    const Model *m = model_by_id(id);
    if (!m) { fprintf(stderr, "no model %s\n", id); exit(1); }
    return m;
}

static int pidx(const Model *m, const char *id) {
    for (int i = 0; i < m->n_params; ++i)
        if (!strcmp(m->params[i].id, id)) return i;
    fprintf(stderr, "no param %s on %s\n", id, m->id);
    exit(1);
}

static FILE *csv(const char *name, const char *header) {
    char path[256];
    snprintf(path, sizeof(path), "paper/data/%s.csv", name);
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    fprintf(f, "%s\n", header);
    return f;
}

/* Write the first ink channel as an 8-bit greyscale image, ink on paper. */
static void image(const char *name, const Model *m) {
    m->ink(&W, W.ink);
    char path[256];
    snprintf(path, sizeof(path), "paper/fig/%s.pgm", name);
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    fprintf(f, "P5\n%d %d\n255\n", W.w, W.h);
    for (int i = 0; i < W.w * W.h; ++i) fputc(255 - W.ink[i * 4], f);
    fclose(f);
    printf("  fig/%s.pgm\n", name);
}

/* ------------------------------------------------------------------ */
/* 1. The colony: food sets the morphology.                            */

static void fig_colony(void) {
    const Model *m = M("colony");
    const int i_n0 = pidx(m, "n0");
    const int i_dn = pidx(m, "Dn");
    const int i_sub = pidx(m, "substeps");

    printf("colony: D against nutrient\n");
    FILE *f = csv("colony_food", "n0,D,biomass");
    const float n0s[] = {0.05f, 0.07f, 0.09f, 0.11f, 0.13f, 0.16f, 0.20f,
                         0.25f, 0.30f, 0.36f, 0.45f, 0.60f, 0.80f};
    for (unsigned i = 0; i < sizeof(n0s) / sizeof(*n0s); ++i) {
        world_set_defaults(&W, m);
        W.p[i_n0] = n0s[i];
        world_reset(&W, m, 256, 256, 7);
        world_step(&W, 5000);
        fprintf(f, "%.3f,%.4f,%.0f\n", n0s[i], W.obs[2], W.obs[0]);
        printf("  n0=%.2f  D=%.3f  biomass=%.0f\n", n0s[i], W.obs[2], W.obs[0]);
    }
    fclose(f);

    /* The two limits, as pictures. */
    world_set_defaults(&W, m);
    W.p[i_n0] = 0.10f;
    world_reset(&W, m, 256, 256, 7);
    world_step(&W, 5000);
    image("colony_starved", m);

    /* Caught at 250 generations, deliberately. A well-fed colony reaches the walls
     * of the dish by generation 450 and the figure becomes a solid black square:
     * true, and it tells the reader nothing. The subject of the picture is the
     * shape of the growth front — smooth, convex, no fingers — and the front has
     * to still exist for it to be photographed. The D = 2.0 in the plot is from
     * the fully grown colony. */
    world_set_defaults(&W, m);
    W.p[i_n0] = 0.45f;
    world_reset(&W, m, 256, 256, 7);
    world_step(&W, 250);
    image("colony_fed", m);

    /* And the diffusion length, at fixed food: this is the claim that the
     * branching is caused by the instability and not merely by hunger. */
    printf("colony: D against diffusion length, at fixed food\n");
    f = csv("colony_difflen", "difflen,D,biomass");
    const float dns[] = {0.03f, 0.05f, 0.08f, 0.12f, 0.16f, 0.20f, 0.24f};
    const float subs[] = {1, 2, 4, 8};
    for (unsigned a = 0; a < sizeof(dns) / sizeof(*dns); ++a)
        for (unsigned b = 0; b < sizeof(subs) / sizeof(*subs); ++b) {
            world_set_defaults(&W, m);
            W.p[i_n0] = 0.16f;
            W.p[i_dn] = dns[a];
            W.p[i_sub] = subs[b];
            world_reset(&W, m, 256, 256, 7);
            world_step(&W, 4000);
            fprintf(f, "%.3f,%.4f,%.0f\n", dns[a] * subs[b], W.obs[2], W.obs[0]);
        }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* 2. Schelling: the gap between what people want and what they get.   */

static void fig_schelling(void) {
    const Model *m = M("schelling");
    const int i_tau = pidx(m, "tau");
    printf("schelling: segregation against tolerance\n");
    FILE *f = csv("schelling", "tau,segregation,sd");

    for (float tau = 0.05f; tau <= 0.801f; tau += 0.05f) {
        double sum = 0.0, sum2 = 0.0;
        const int reps = 5;
        for (int r = 0; r < reps; ++r) {
            world_set_defaults(&W, m);
            W.p[i_tau] = tau;
            world_reset(&W, m, 128, 128, 100 + r);
            world_step(&W, 500);
            sum += W.obs[0];
            sum2 += (double)W.obs[0] * W.obs[0];
        }
        const double mean = sum / reps;
        const double var = sum2 / reps - mean * mean;
        fprintf(f, "%.3f,%.4f,%.4f\n", tau, mean, sqrt(var > 0 ? var : 0));
        printf("  tau=%.2f  seg=%.3f\n", tau, mean);
    }
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* 3. Sugarscape: a fair world, and the distribution it produces.      */

static void fig_sugarscape(void) {
    const Model *m = M("sugarscape");
    printf("sugarscape: inequality over time\n");
    FILE *f = csv("sugarscape", "t,gini,population,meanWealth");

    world_set_defaults(&W, m);
    world_reset(&W, m, 128, 128, 3);
    for (int t = 0; t <= 400; ++t) {
        if (t) world_step(&W, 1);
        if (t % 4 == 0)
            fprintf(f, "%d,%.4f,%.0f,%.3f\n", t, W.obs[1], W.obs[0], W.obs[2]);
    }
    printf("  final: gini=%.3f  pop=%.0f\n", W.obs[1], W.obs[0]);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* 4. Rock-paper-scissors: biodiversity against mobility.              */

static void fig_rps(void) {
    const Model *m = M("rps");
    const int i_eps = pidx(m, "eps");
    printf("rps: survival against mobility\n");
    FILE *f = csv("rps", "eps,survivors,extinction");

    const float epss[] = {0.5f, 1, 2, 4, 8, 12, 16, 22, 30, 40, 55};
    for (unsigned i = 0; i < sizeof(epss) / sizeof(*epss); ++i) {
        const int reps = 8;
        int lost = 0;
        double surv = 0;
        for (int r = 0; r < reps; ++r) {
            world_set_defaults(&W, m);
            W.p[i_eps] = epss[i];
            world_reset(&W, m, 128, 128, 200 + r);
            world_step(&W, 1500);
            const int n = (W.obs[0] > 0.005f) + (W.obs[1] > 0.005f) + (W.obs[2] > 0.005f);
            surv += n;
            if (n < 3) lost++;
        }
        fprintf(f, "%.1f,%.3f,%.3f\n", epss[i], surv / reps, (double)lost / reps);
        printf("  eps=%5.1f  survivors=%.2f  P(extinction)=%.2f\n",
               epss[i], surv / reps, (double)lost / reps);
    }
    fclose(f);

    world_set_defaults(&W, m);
    W.p[i_eps] = 2.0f;
    world_reset(&W, m, 256, 256, 9);
    world_step(&W, 2000);
    image("rps_spirals", m);
}

/* ------------------------------------------------------------------ */
/* 5. All 256 elementary rules, classified by input entropy.           */

static void fig_eca(void) {
    const Model *m = M("eca");
    const int i_rule = pidx(m, "rule");
    const int i_init = pidx(m, "init");
    printf("eca: classifying all 256 rules\n");
    FILE *f = csv("eca", "rule,entropy,variance");

    for (int rule = 0; rule < 256; ++rule) {
        world_set_defaults(&W, m);
        W.p[i_rule] = (float)rule;
        W.p[i_init] = 1.0f;   /* random line, as Wuensche's method assumes */
        world_reset(&W, m, 400, 200, 12);
        world_step(&W, 400);  /* let the transient pass, then measure */
        fprintf(f, "%d,%.5f,%.6f\n", rule, W.obs[0], W.obs[1]);
    }
    fclose(f);

    /* The three that everybody quotes, as pictures. */
    const int shown[] = {30, 110, 90};
    const char *names[] = {"eca_r30", "eca_r110", "eca_r90"};
    for (int i = 0; i < 3; ++i) {
        world_set_defaults(&W, m);
        W.p[i_rule] = (float)shown[i];
        W.p[i_init] = (shown[i] == 110) ? 1.0f : 0.0f;
        world_reset(&W, m, 256, 256, 12);
        world_step(&W, 255);
        image(names[i], m);
    }
}

/* ------------------------------------------------------------------ */
/* 6. Lenia: the niche is a filament.                                  */

static void fig_lenia(void) {
    const Model *m = M("lenia");
    const int i_mu = pidx(m, "mu");
    const int i_sig = pidx(m, "sigma");
    printf("lenia: the survival region in (mu, sigma)\n");
    FILE *f = csv("lenia", "mu,sigma,alive");

    /* The kernel is a few hundred taps per cell, so this sweep is the most
     * expensive thing in the paper by a wide margin. The grid is cut to the
     * smallest box Orbium can live in without seeing its own tail across the
     * torus, and the runs are cut to the shortest horizon that still separates
     * the three outcomes (persists / evaporates / blooms). Both were checked
     * against the full-size version on a few points before being turned down. */
    for (float mu = 0.06f; mu <= 0.281f; mu += 0.0125f) {
        for (float sg = 0.004f; sg <= 0.0361f; sg += 0.0015f) {
            world_set_defaults(&W, m);
            W.p[i_mu] = mu;
            W.p[i_sig] = sg;
            world_reset(&W, m, 72, 72, 1);
            world_step(&W, 1);
            const float m0 = W.obs[0];
            world_step(&W, 250);
            const float m1 = W.obs[0];
            /* Alive means: still here, still itself. Dead is mass -> 0;
             * cancerous is mass -> the whole world. Both are failures. */
            const int alive = (m1 > 0.35f * m0) && (m1 < 3.0f * m0) && (W.obs[1] < 0.30f);
            fprintf(f, "%.3f,%.4f,%d\n", mu, sg, alive);
        }
    }
    fclose(f);

    /* The smallest box the creature fits in without meeting its own tail across
     * the torus. On the 96-cell grid used for the sweep it is a speck. */
    world_set_defaults(&W, m);
    world_reset(&W, m, 64, 64, 1);
    world_step(&W, 40);
    image("lenia_orbium", m);
}

/* ------------------------------------------------------------------ */
/* 7. The forest fire: the distribution of fire sizes.                 */

static void fig_forestfire(void) {
    const Model *m = M("forestfire");
    const int i_p = pidx(m, "p");
    const int i_f = pidx(m, "f");
    printf("forest fire: fire-size distribution\n");

    /* Two regimes: a wide separation of timescales, and none at all. */
    const float fs[] = {0.00002f, 0.001f};
    const char *tag[] = {"critical", "driven"};
    extern const long *ff_bins(const World *, int *);

    for (int r = 0; r < 2; ++r) {
        char name[64];
        snprintf(name, sizeof(name), "forestfire_%s", tag[r]);
        FILE *out = csv(name, "size,count");

        world_set_defaults(&W, m);
        W.p[i_p] = 0.02f;
        W.p[i_f] = fs[r];
        world_reset(&W, m, 256, 256, 4);
        world_step(&W, 20000);

        int nb = 0;
        const long *bins = ff_bins(&W, &nb);
        for (int b = 0; b < nb; ++b) {
            if (!bins[b]) continue;
            /* octave bins: the representative size is the geometric centre */
            const double size = pow(2.0, b + 0.5);
            fprintf(out, "%.1f,%ld\n", size, bins[b]);
        }
        fclose(out);
        printf("  %s: density=%.3f fires=%.0f largest=%.0f\n",
               tag[r], W.obs[0], W.obs[2], W.obs[3]);
    }

    world_set_defaults(&W, m);
    W.p[i_p] = 0.02f;
    W.p[i_f] = 0.00002f;
    world_reset(&W, m, 256, 256, 4);
    world_step(&W, 4000);
    image("forestfire", m);
}

/* ------------------------------------------------------------------ */

static void fig_grayscott(void) {
    const Model *m = M("grayscott");
    const int i_F = pidx(m, "F");
    const int i_k = pidx(m, "k");
    printf("gray-scott: three of Pearson's regimes\n");

    const float Fs[] = {0.026f, 0.029f, 0.058f};
    const float ks[] = {0.061f, 0.057f, 0.065f};
    const char *nm[] = {"gs_mitosis", "gs_maze", "gs_worms"};
    for (int i = 0; i < 3; ++i) {
        world_set_defaults(&W, m);
        W.p[i_F] = Fs[i];
        W.p[i_k] = ks[i];
        world_reset(&W, m, 256, 256, 7);
        world_step(&W, 4000);
        image(nm[i], m);
    }
}

static const struct {
    const char *name;
    void (*fn)(void);
} SECTIONS[] = {
    {"colony", fig_colony},
    {"schelling", fig_schelling},
    {"sugarscape", fig_sugarscape},
    {"rps", fig_rps},
    {"eca", fig_eca},
    {"lenia", fig_lenia},
    {"forestfire", fig_forestfire},
    {"grayscott", fig_grayscott},
};

int main(int argc, char **argv) {
    boot();
    const int n = (int)(sizeof(SECTIONS) / sizeof(*SECTIONS));
    for (int i = 0; i < n; ++i) {
        int wanted = (argc < 2);
        for (int a = 1; a < argc; ++a)
            if (!strcmp(argv[a], SECTIONS[i].name)) wanted = 1;
        if (wanted) SECTIONS[i].fn();
    }
    printf("\ndone\n");
    return 0;
}
