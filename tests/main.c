/* Native test runner.
 *
 * Two kinds of check.
 *
 * The first is determinism: the same seed, the same parameters, and the same
 * number of generations must produce a bit-identical grid, always. This runs in
 * CI on every push, and if it ever fails it means a run is no longer
 * reproducible from its permalink, which would quietly make the whole lab
 * useless as an instrument.
 *
 * The second is that the models agree with the literature. These are not
 * self-consistency checks — they are numbers other people measured, some of them
 * in a laboratory with actual bacteria in it, and if our code does not
 * reproduce them then our code is wrong:
 *
 *   a starved colony box-counts to D ~ 1.7        Witten & Sander 1981;
 *                                                 Fujikawa & Matsushita 1989
 *   a well-fed colony box-counts to D ~ 2.0       (compact: it is a disc)
 *   Sugarscape's Gini rises to ~0.5               Epstein & Axtell 1996
 *   Schelling at tau=0.35 segregates to ~0.85     Schelling 1971
 *   rule 90 from one cell is a Sierpinski triangle,
 *     of dimension log3/log2 = 1.585
 *   Conway's Life from a random soup settles to
 *     a density near 0.03                         (the standard "ash" density)
 *   Orbium survives 400 steps with its mass bounded and nonzero
 *
 * When one of these drifts, it is a bug, not a tolerance to be widened.
 */
#include "world.h"
#include "obs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                        \
    do {                                                        \
        checks++;                                               \
        if (cond) {                                             \
            printf("  ok    ");                                 \
        } else {                                                \
            printf("  FAIL  ");                                 \
            failures++;                                         \
        }                                                       \
        printf(__VA_ARGS__);                                    \
        printf("\n");                                           \
    } while (0)

#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static World W;

static void boot(void) {
    W.arena = (uint8_t *)malloc(48u << 20);
    W.arena_cap = 48u << 20;
    W.ink = (uint8_t *)malloc((size_t)MAX_DIM * MAX_DIM * 4);
}

static const Model *M(const char *id) {
    const Model *m = model_by_id(id);
    if (!m) {
        printf("  FAIL  no such model: %s\n", id);
        exit(1);
    }
    return m;
}

/* ------------------------------------------------------------------ */

static void t_determinism(void) {
    printf("determinism\n");
    for (int i = 0; i < MODEL_COUNT; ++i) {
        const Model *m = MODELS[i];

        world_set_defaults(&W, m);
        world_reset(&W, m, 96, 96, 42);
        world_step(&W, 60);
        uint64_t h1 = world_hash(&W);

        world_set_defaults(&W, m);
        world_reset(&W, m, 96, 96, 42);
        world_step(&W, 60);
        uint64_t h2 = world_hash(&W);

        world_set_defaults(&W, m);
        world_reset(&W, m, 96, 96, 43);
        world_step(&W, 60);
        uint64_t h3 = world_hash(&W);

        CHECK(h1 == h2, "%-11s seed 42 reproduces (%016llx)", m->id,
              (unsigned long long)h1);
        /* A different seed must give a different run — except for the three
         * models whose default initial condition contains no randomness at all:
         * the ant starts on a blank grid, rule 30 starts from one live cell, and
         * Lenia starts from Orbium. Those are deterministic by construction, and
         * a seed can only change them once you ask for a random start. */
        if (strcmp(m->id, "ant") && strcmp(m->id, "eca") && strcmp(m->id, "lenia"))
            CHECK(h1 != h3, "%-11s seed 43 differs from seed 42", m->id);
    }
}

static void t_colony_fractal(void) {
    printf("\ncolony: the morphology diagram\n");
    const Model *m = M("colony");

    /* Starved: nutrient is scarce, the depletion halo is wide, the growth front
     * is unstable, and the colony must branch to find food. Witten and Sander's
     * diffusion-limited aggregate has D = 1.71; Fujikawa and Matsushita measured
     * 1.73 on an actual plate of B. subtilis. We should land in between. */
    world_set_defaults(&W, m);
    W.p[0] = 0.10f;   /* n0: starve it */
    world_reset(&W, m, 256, 256, 7);
    world_step(&W, 5000);
    const float d_starved = W.obs[2];
    const float bio_starved = W.obs[0];

    /* Fed: diffusion keeps up with consumption, no bump on the front gets an
     * advantage over any other, the instability never bites, and the colony
     * fills in as a disc. */
    world_set_defaults(&W, m);
    W.p[0] = 0.45f;   /* n0: feed it */
    world_reset(&W, m, 256, 256, 7);
    world_step(&W, 3000);
    const float d_fed = W.obs[2];

    printf("        starved D = %.3f (biomass %.0f), fed D = %.3f\n",
           d_starved, bio_starved, d_fed);

    CHECK(bio_starved > 3000.0f, "the starved colony still grows (biomass %.0f)",
          bio_starved);
    CHECK(d_starved > 1.45f && d_starved < 1.85f,
          "starved, it is a fractal, not a disc: D = %.2f", d_starved);
    CHECK(d_fed > 1.95f, "fed, it is a compact disc: D = %.2f", d_fed);
    CHECK(d_fed - d_starved > 0.20f,
          "food alone moves the dimension from %.2f to %.2f", d_starved, d_fed);
}

static void t_sierpinski(void) {
    printf("\neca: rule 90 from a single cell\n");
    const Model *m = M("eca");
    world_set_defaults(&W, m);
    W.p[0] = 90.0f;  /* rule */
    W.p[1] = 0.0f;   /* init: single cell */
    world_reset(&W, m, 256, 256, 1);
    world_step(&W, 255);

    m->ink(&W, W.ink);
    uint8_t *mask = (uint8_t *)calloc((size_t)W.w * W.h, 1);
    for (int i = 0; i < W.w * W.h; ++i) mask[i] = W.ink[i * 4] ? 1 : 0;
    const float d = box_dimension(mask, W.w, W.h, 0, 0);
    free(mask);

    printf("        D = %.3f (Sierpinski: log3/log2 = 1.585)\n", d);
    CHECK(NEAR(d, 1.585, 0.10), "rule 90 draws a Sierpinski triangle: D = %.3f", d);
}

static void t_sugarscape_gini(void) {
    printf("\nsugarscape: inequality from a fair world\n");
    const Model *m = M("sugarscape");
    world_set_defaults(&W, m);
    world_reset(&W, m, 128, 128, 3);
    world_step(&W, 300);

    const float pop = W.obs[0];
    const float g = W.obs[1];
    printf("        population %.0f, Gini %.3f\n", pop, g);

    CHECK(pop > 50.0f, "the population survives and finds a carrying capacity (%.0f)", pop);
    CHECK(g > 0.25f, "wealth becomes unequal from equal starts: Gini = %.2f", g);
    CHECK(g < 0.80f, "and not absurdly so: Gini = %.2f < 0.80", g);
}

static void t_schelling(void) {
    printf("\nschelling: mild preferences, total segregation\n");
    const Model *m = M("schelling");

    /* A randomly mixed city of two equal groups scores 0.5: half your
     * neighbours look like you, by chance. That is the baseline everything below
     * is measured against.
     *
     * The system stops as soon as every household is content, so tau does not
     * merely influence the outcome, it *sets* it: the city segregates exactly as
     * far as it must to satisfy the preference, and no further. Which is why
     * even a very mild preference produces a badly divided city. */
    const float taus[] = {0.15f, 0.35f, 0.50f};
    float seg[3];
    for (int i = 0; i < 3; ++i) {
        world_set_defaults(&W, m);
        W.p[0] = taus[i];
        world_reset(&W, m, 128, 128, 5);
        world_step(&W, 400);
        seg[i] = W.obs[0];
        printf("        tau = %.2f -> segregation %.3f (chance would give 0.50)\n",
               taus[i], seg[i]);
    }

    CHECK(seg[0] < 0.65f,
          "tolerant (tau 0.15): the city stays close to mixed (%.2f)", seg[0]);
    CHECK(seg[1] > 0.72f,
          "mildly choosy (tau 0.35): happy to be outnumbered two to one, and the "
          "city still ends up %.0f%% segregated", seg[1] * 100.0f);
    CHECK(seg[2] > 0.85f,
          "wanting a bare majority (tau 0.50): near-total segregation (%.2f)", seg[2]);
    CHECK(seg[1] > seg[0] && seg[2] > seg[1], "segregation rises with tau");
}

static void t_life(void) {
    printf("\nlife: a random soup settles into ash\n");
    const Model *m = M("life");
    world_set_defaults(&W, m);
    world_reset(&W, m, 256, 256, 11);
    world_step(&W, 600);
    const float density = W.obs[1];
    printf("        density %.4f (the standard ash density is about 0.03)\n", density);
    CHECK(density > 0.015f && density < 0.06f,
          "Conway's ash density is near 0.03: got %.3f", density);

    /* B3/S23 with a glider must translate it, not kill it. */
    world_set_defaults(&W, m);
    W.p[2] = 0.0f;  /* empty soup */
    world_reset(&W, m, 64, 64, 1);
    /* a glider */
    m->paint(&W, 10, 10, 0, 0);
    m->paint(&W, 11, 11, 0, 0);
    m->paint(&W, 9, 12, 0, 0);
    m->paint(&W, 10, 12, 0, 0);
    m->paint(&W, 11, 12, 0, 0);
    world_step(&W, 4);
    CHECK(W.obs[0] == 5.0f, "a glider still has five cells after four generations (%.0f)",
          W.obs[0]);
    world_step(&W, 4 * 15);
    CHECK(W.obs[0] == 5.0f, "and after sixty-four (%.0f)", W.obs[0]);
}

static void t_lenia(void) {
    printf("\nlenia: orbium survives\n");
    const Model *m = M("lenia");
    world_set_defaults(&W, m);
    world_reset(&W, m, 128, 128, 1);
    world_step(&W, 1);
    const float mass_start = W.obs[0];
    world_step(&W, 400);
    const float mass_end = W.obs[0];

    printf("        mass %.1f -> %.1f\n", mass_start, mass_end);
    CHECK(mass_start > 10.0f, "orbium is there to begin with (%.0f)", mass_start);
    CHECK(mass_end > 0.5f * mass_start && mass_end < 2.0f * mass_start,
          "it is still alive and still itself after 400 steps: mass %.0f vs %.0f",
          mass_end, mass_start);

    /* Push sigma outside the niche and the creature must die. This is the claim
     * the whole "edge of chaos" story rests on, so it should be tested. */
    world_set_defaults(&W, m);
    W.p[1] = 0.005f;  /* sigma far too narrow */
    world_reset(&W, m, 128, 128, 1);
    world_step(&W, 300);
    printf("        with sigma = 0.005, mass = %.1f\n", W.obs[0]);
    CHECK(W.obs[0] < 0.3f * mass_start,
          "starved out of its niche, orbium dies (mass %.1f)", W.obs[0]);
}

static void t_rps(void) {
    printf("\nrps: mobility destroys biodiversity\n");
    const Model *m = M("rps");

    /* Low mobility: spirals form, all three species persist. */
    world_set_defaults(&W, m);
    W.p[0] = 1.0f;  /* eps */
    world_reset(&W, m, 128, 128, 9);
    world_step(&W, 400);
    const float a = W.obs[0], b = W.obs[1], c = W.obs[2];
    const int coexist = (a > 0.02f && b > 0.02f && c > 0.02f);
    printf("        eps = 1:  %.3f %.3f %.3f\n", a, b, c);
    CHECK(coexist, "at low mobility all three species survive");

    /* High mobility: the spirals outgrow the world and diversity collapses. */
    world_set_defaults(&W, m);
    W.p[0] = 55.0f;
    world_reset(&W, m, 128, 128, 9);
    world_step(&W, 400);
    const float A = W.obs[0], B = W.obs[1], C = W.obs[2];
    const int survivors = (A > 0.02f) + (B > 0.02f) + (C > 0.02f);
    printf("        eps = 55: %.3f %.3f %.3f\n", A, B, C);
    CHECK(survivors < 3, "at high mobility, at least one species is driven out");
}

static void t_forestfire(void) {
    printf("\nforest fire: the forest finds its own critical density\n");
    const Model *m = M("forestfire");
    world_set_defaults(&W, m);
    world_reset(&W, m, 192, 192, 4);
    world_step(&W, 1500);
    const float dens = W.obs[0];
    const float fires = W.obs[2];
    printf("        density %.3f after 1500 steps, %.0f fires recorded\n", dens, fires);
    CHECK(dens > 0.20f && dens < 0.75f,
          "density self-organises to a critical value, not to 0 or 1 (%.2f)", dens);
    CHECK(fires > 3.0f, "fires happen and are counted (%.0f)", fires);
}

int main(void) {
    boot();
    printf("morphogen: %d models\n\n", MODEL_COUNT);

    t_determinism();
    t_colony_fractal();
    t_sierpinski();
    t_sugarscape_gini();
    t_schelling();
    t_life();
    t_lenia();
    t_rps();
    t_forestfire();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
