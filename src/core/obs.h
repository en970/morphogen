/* The instrument bus.
 *
 * What separates a laboratory from a screensaver is that it measures. Each
 * model streams a handful of scalars out of every step; these are the routines
 * that compute the ones worth having, and several of them have known correct
 * answers that we can and do test against:
 *
 *   box_dimension  of a diffusion-limited colony    -> 1.71   (Witten & Sander 1981)
 *   box_dimension  of a real B. subtilis colony     -> 1.73   (Fujikawa & Matsushita 1989)
 *   box_dimension  of rule 90 from a single cell    -> 1.585  (log3/log2, Sierpinski)
 *   gini           of Sugarscape from equal starts  -> ~0.5   (Epstein & Axtell 1996)
 *
 * Those numbers are the test suite.
 */
#ifndef MORPHOGEN_OBS_H
#define MORPHOGEN_OBS_H

#include <stdint.h>

/* Shannon entropy, in bits, of a histogram of k states. */
float shannon(const int *counts, int k, int total);

/* Box-counting (Minkowski-Bouligand) dimension of a binary mask.
 *
 * Cover the occupied cells with boxes of side e = 1,2,4,8,... and count the
 * boxes that contain anything. If the set is fractal, N(e) ~ e^-D, so the slope
 * of log N against log(1/e) is D.
 *
 * The honest caveat, which the UI states rather than hides: on a finite lattice
 * this estimator is biased at both ends. At e = 1 you are just counting cells,
 * and at e comparable to the system size you have one box. Only the middle of
 * the range is scaling, so we fit over boxes from `emin` to `emax` and the panel
 * plots the local slope so you can see for yourself where the scaling region
 * actually is. A single number with no plot behind it would be a lie.
 *
 * Returns D; writes the per-scale counts into `n_out` (length `levels`) if
 * non-null, so the panel can draw the log-log plot and the fitted line. */
float box_dimension(const uint8_t *mask, int w, int h, int *n_out, int *levels_out);

/* Gini coefficient of a non-negative distribution, by the sorted-array formula
 *   G = (2 * sum(i * x_i)) / (n * sum(x)) - (n + 1) / n
 * which is exact and O(n log n), rather than the O(n^2) double sum.
 * Destroys the order of `x`. */
float gini(float *x, int n);

/* Mean fraction of a cell's occupied neighbours that share its type: the
 * segregation index. Schelling's order parameter. `type` is -1 for empty. */
float segregation(const int8_t *type, int w, int h);

#endif
