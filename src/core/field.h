/* Lattice geometry and the shared diffusion kernel.
 *
 * Five of the ten models here carry a diffusing scalar field: nutrient and
 * chemorepellent in the bacterial colony, the two chemical species in
 * Gray-Scott, sugar in Sugarscape (as a regrowth rather than a diffusion), heat
 * in Daisyworld. They all want the same Laplacian, so it lives here once.
 *
 * Boundary conditions are not a detail. A Petri dish is a closed, no-flux
 * domain: nutrient that reaches the wall stays in the dish. A torus is what you
 * want when you are studying bulk behaviour and do not want an edge to nucleate
 * anything. The sandpile *requires* an open, dissipative boundary or it never
 * reaches a steady state. So the boundary condition is exposed as a parameter
 * wherever it changes the physics, and each model documents which one its
 * source paper used.
 */
#ifndef MORPHOGEN_FIELD_H
#define MORPHOGEN_FIELD_H

#include "model.h"

enum { BC_TORUS = 0, BC_NOFLUX = 1 };

/* Neighbour index tables. The alternative — computing (x + dx + w) % w inside
 * the inner loop — costs an integer division per neighbour per cell, which is
 * 20-40 cycles each and would dominate the entire step. A table lookup is one
 * L1 hit, and under no-flux the table simply points the edge back at itself,
 * which is exactly the zero-flux condition and needs no branch. */
typedef struct {
    int w, h, n;
    int *xm, *xp; /* length w */
    int *ym, *yp; /* length h */
} Lat;

void lat_init(World *wo, Lat *L, int bc);

/* One explicit Euler step of  df/dt = D * laplacian(f).
 *
 * The stencil is the nine-point form
 *
 *     0.05  0.20  0.05
 *     0.20 -1.00  0.20
 *     0.05  0.20  0.05
 *
 * rather than the naive five-point one. The five-point Laplacian on a square
 * lattice is anisotropic at order h^2: it diffuses faster along the axes than
 * along the diagonals, and in a growth model that artefact is not cosmetic — it
 * makes colonies and reaction fronts grow visible plus-shaped lobes aligned
 * with the grid. The nine-point weighting cancels the leading anisotropy.
 *
 * `rate` is D*dt/dx^2 and must satisfy rate <= 0.25 for stability. Callers that
 * want a larger effective D take several sub-steps instead, which is also
 * physically right: the nutrient field equilibrates much faster than the
 * colony grows, and that separation of timescales is what sets the width of the
 * depletion halo, which in turn is what makes the colony branch. */
void diffuse(const Lat *L, float *f, float *tmp, float rate);

/* As above, with first-order decay: df/dt = D*lap(f) - lambda*f. Used by the
 * chemorepellent and chemoattractant fields, which must decay or they saturate
 * the dish and stop carrying a gradient. */
void diffuse_decay(const Lat *L, float *f, float *tmp, float rate, float lambda);

#endif
