/* Elementary cellular automata: Wolfram's 256 rules.
 *
 * A line of cells, each 0 or 1. Each cell looks at itself and its two
 * neighbours — eight possible patterns — and a rule is nothing more than a
 * choice of output bit for each of those eight. Eight bits, so 256 rules, and
 * the rule's name is that byte read as a number:
 *
 *     next = (rule >> ((left << 2) | (self << 1) | right)) & 1
 *
 * Time runs downward, so the picture is the whole history of the line.
 *
 * This is the simplest system in the lab and it is also the one that should
 * unsettle you the most. Rule 90 is `left XOR right` and from a single black
 * cell it draws the Sierpinski triangle — a fractal, from a rule you can hold
 * in your head. Rule 30 is chaotic enough that Mathematica used it as a random
 * number generator for years. Rule 110 supports gliders that collide and
 * interact, and Matthew Cook proved that those collisions can be arranged to
 * carry out any computation at all: the rule is Turing-complete. Three bytes
 * out of two hundred and fifty-six, and one of them is a universal computer.
 *
 * Wolfram sorted the rules into four classes: those that die, those that freeze
 * into periodic structures, those that boil chaotically, and the rare fourth
 * kind that do neither and support long-lived travelling structures. The lab
 * classifies them for you, using Wuensche's method rather than by eye. At each
 * step, count how many cells matched each of the eight neighbourhood patterns
 * and take the Shannon entropy of that histogram: the *input entropy*. Rules
 * that freeze drive it to a low, flat value. Rules that boil hold it high and
 * flat. Class IV rules — the interesting ones — sit in between with a large
 * variance, because the entropy jumps around as gliders form, collide, and die.
 * The variance of the input entropy is a glider detector, and it costs almost
 * nothing.
 *
 * References
 *   Wolfram, S. "Statistical mechanics of cellular automata." Rev. Mod. Phys.
 *     55, 601-644 (1983).
 *   Cook, M. "Universality in Elementary Cellular Automata." Complex Systems
 *     15, 1-40 (2004).
 *   Wuensche, A. "Classifying cellular automata automatically: finding gliders,
 *     filtering, and relating space-time patterns, attractor basins, and the Z
 *     parameter." Complexity 4(3), 47-66 (1999).
 */
#include "../model.h"
#include "../obs.h"

#include <math.h>
#include <string.h>

enum { P_RULE = 0, P_INIT, P_DENSITY, P_NPARAM };

static const ParamDef PARAMS[] = {
    {"rule",    P_INT,   0.0f, 255.0f, 1.0f, 30.0f, 0, "the rule number. its eight bits are the eight outputs."},
    {"init",    P_ENUM,  0.0f, 1.0f,   1.0f,  0.0f, "single|random", "a single live cell, or a random line. rule 90 from a single cell is a Sierpinski triangle."},
    {"density", P_FLOAT, 0.0f, 1.0f,   0.01f, 0.5f, 0, "fraction alive, when starting from a random line."},
};

#define ENT_WINDOW 64

typedef struct {
    uint8_t *rows;  /* h rows of w cells, used as a ring */
    uint8_t *line;  /* the current line */
    uint8_t *next;
    int head;       /* index of the row the newest line was written to */
    float ent[ENT_WINDOW];
    int ent_n;
} St;

static size_t eca_mem(int w, int h) {
    return (size_t)w * h + (size_t)w * 2 + 4096;
}

static void eca_reset_line(World *wo, St *s) {
    const int w = wo->w;
    memset(s->line, 0, (size_t)w);
    if ((int)wo->p[P_INIT] == 0) {
        s->line[w / 2] = 1;
    } else {
        const float d = wo->p[P_DENSITY];
        for (int x = 0; x < w; ++x) s->line[x] = pcg32_f(&wo->rng) < d ? 1 : 0;
    }
}

static void eca_init(World *wo) {
    St *s = (St *)w_alloc(wo, sizeof(St));
    wo->st = s;
    const int w = wo->w, h = wo->h;

    s->rows = (uint8_t *)w_alloc(wo, (size_t)w * h);
    s->line = (uint8_t *)w_alloc(wo, (size_t)w);
    s->next = (uint8_t *)w_alloc(wo, (size_t)w);
    s->head = 0;
    s->ent_n = 0;

    eca_reset_line(wo, s);
    memcpy(s->rows, s->line, (size_t)w);
}

static void eca_step(World *wo) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    const uint32_t rule = (uint32_t)wo->p[P_RULE];

    int hist[8] = {0};
    int pop = 0;

    for (int x = 0; x < w; ++x) {
        /* the line is a ring: Wolfram's standard boundary */
        const int l = s->line[x == 0 ? w - 1 : x - 1];
        const int c = s->line[x];
        const int r = s->line[x == w - 1 ? 0 : x + 1];
        const int idx = (l << 2) | (c << 1) | r;
        hist[idx]++;
        const uint8_t v = (uint8_t)((rule >> idx) & 1u);
        s->next[x] = v;
        pop += v;
    }
    memcpy(s->line, s->next, (size_t)w);

    s->head = (s->head + 1) % h;
    memcpy(s->rows + (size_t)s->head * w, s->line, (size_t)w);

    /* Wuensche's input entropy, and the running variance that identifies the
     * class IV rules. */
    const float e = shannon(hist, 8, w);
    s->ent[s->ent_n % ENT_WINDOW] = e;
    s->ent_n++;

    const int m = s->ent_n < ENT_WINDOW ? s->ent_n : ENT_WINDOW;
    double mean = 0.0;
    for (int i = 0; i < m; ++i) mean += s->ent[i];
    mean /= (double)m;
    double var = 0.0;
    for (int i = 0; i < m; ++i) {
        double d = s->ent[i] - mean;
        var += d * d;
    }
    var /= (double)m;

    wo->obs[0] = (float)mean;
    wo->obs[1] = (float)var;
    wo->obs[2] = (float)pop / (float)w;
}

static void eca_ink(World *wo, uint8_t *out) {
    St *s = (St *)wo->st;
    const int w = wo->w, h = wo->h;
    /* Newest line at the bottom, history scrolling up. */
    for (int y = 0; y < h; ++y) {
        int src = (s->head - (h - 1 - y)) % h;
        if (src < 0) src += h;
        const uint8_t *row = s->rows + (size_t)src * w;
        uint8_t *dst = out + (size_t)y * w * 4;
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = row[x] ? 255 : 0;
            dst[x * 4 + 1] = 0;
            dst[x * 4 + 2] = 0;
            dst[x * 4 + 3] = 0;
        }
    }
}

static void eca_paint(World *wo, int cx, int cy, int radius, int erase) {
    (void)cy;
    St *s = (St *)wo->st;
    const int w = wo->w;
    for (int dx = -radius; dx <= radius; ++dx) {
        int x = cx + dx;
        if (x < 0 || x >= w) continue;
        s->line[x] = erase ? 0 : 1;
    }
}

const Model MODEL_ECA = {
    .id = "eca",
    .name = "Elementary CA",
    .def_w = 384,
    .def_h = 256,
    .n_inks = 1,
    .ink_names = {"cell", 0, 0, 0},
    .ink_colors = {0x2b2b2b, 0, 0, 0},
    .params = PARAMS,
    .n_params = P_NPARAM,
    .obs_names = {"entropy", "entropyVar", "density", 0, 0, 0},
    .n_obs = 3,
    .mem = eca_mem,
    .init = eca_init,
    .step = eca_step,
    .ink = eca_ink,
    .paint = eca_paint,
};
