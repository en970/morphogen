#ifndef MORPHOGEN_WORLD_H
#define MORPHOGEN_WORLD_H

#include "model.h"

extern const Model *const MODELS[];
extern const int MODEL_COUNT;

const Model *model_by_id(const char *id);

void world_reset(World *w, const Model *m, int gw, int gh, uint64_t seed);
void world_set_defaults(World *w, const Model *m);
void world_step(World *w, int n);
uint64_t world_hash(World *w);

#endif
