/* Pseudorandom number generation.
 *
 * Every stochastic model in this lab must be reproducible: a seed and a
 * generation count should identify a run exactly, on any machine, in any
 * browser. That rules out rand(), which is implementation-defined, and it rules
 * out seeding from the clock.
 *
 * Two generators are provided.
 *
 * pcg32 (O'Neill 2014) is the sequential workhorse: small, fast, and it passes
 * TestU01 BigCrush, which xorshift and the usual LCGs do not. Each logical
 * consumer (initial condition, agent motion, mutation) gets its own stream via
 * the `seq` argument, so adding a new consumer somewhere does not perturb the
 * others.
 *
 * splitmix64 is used counter-based: hash(seed, generation, x, y) gives a cell
 * its own random number without any sequential state. The result is then
 * independent of the order in which cells are visited, which matters because
 * several models here update cells in a shuffled order.
 *
 * O'Neill, M. E. "PCG: A Family of Simple Fast Space-Efficient Statistically
 * Good Algorithms for Random Number Generation." HMC-CS-2014-0905 (2014).
 */
#ifndef MORPHOGEN_RNG_H
#define MORPHOGEN_RNG_H

#include <stdint.h>

typedef struct {
    uint64_t state;
    uint64_t inc;
} pcg32_t;

static inline uint32_t pcg32_next(pcg32_t *r) {
    uint64_t old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

static inline void pcg32_seed(pcg32_t *r, uint64_t seed, uint64_t seq) {
    r->state = 0u;
    r->inc = (seq << 1u) | 1u;
    pcg32_next(r);
    r->state += seed;
    pcg32_next(r);
}

/* Uniform in [0,1). 24 bits of mantissa, which is all a float can hold. */
static inline float pcg32_f(pcg32_t *r) {
    return (float)(pcg32_next(r) >> 8) * (1.0f / 16777216.0f);
}

/* Uniform in [0,n). Lemire's debiased multiply-shift; no modulo, no rejection
 * loop in the common case. */
static inline uint32_t pcg32_below(pcg32_t *r, uint32_t n) {
    uint64_t m = (uint64_t)pcg32_next(r) * (uint64_t)n;
    uint32_t l = (uint32_t)m;
    if (l < n) {
        uint32_t t = (uint32_t)(-(int32_t)n) % n;
        while (l < t) {
            m = (uint64_t)pcg32_next(r) * (uint64_t)n;
            l = (uint32_t)m;
        }
    }
    return (uint32_t)(m >> 32);
}

static inline uint64_t splitmix64(uint64_t z) {
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Order-independent per-cell randomness. */
static inline float hash_f(uint64_t seed, uint64_t gen, int x, int y) {
    uint64_t h = splitmix64(seed ^ splitmix64(gen * 0x9E3779B97F4A7C15ULL +
                                              (uint64_t)y * 0x100000000ULL +
                                              (uint64_t)x));
    return (float)(h >> 40) * (1.0f / 16777216.0f);
}

/* Fisher-Yates. Used wherever a model specifies asynchronous update over a
 * shuffled agent list (Wa-Tor, Schelling, Sugarscape, the colony model): the
 * order is part of the model, so it must come from the seeded stream. */
static inline void shuffle_i32(pcg32_t *r, int32_t *a, int n) {
    for (int i = n - 1; i > 0; --i) {
        uint32_t j = pcg32_below(r, (uint32_t)(i + 1));
        int32_t t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

#endif
