#include "world.h"

/* The narrative order of the lab, and the order the panel lists them in:
 * rules, then chaos, then chemistry, then colonies, then ecosystems, then
 * societies, then criticality. */
extern const Model MODEL_COLONY;
extern const Model MODEL_GRAYSCOTT;
extern const Model MODEL_LENIA;
extern const Model MODEL_RPS;
extern const Model MODEL_SUGARSCAPE;
extern const Model MODEL_SCHELLING;
extern const Model MODEL_LIFE;
extern const Model MODEL_ECA;
extern const Model MODEL_ANT;
extern const Model MODEL_FORESTFIRE;

const Model *const MODELS[] = {
    &MODEL_COLONY,
    &MODEL_GRAYSCOTT,
    &MODEL_LENIA,
    &MODEL_RPS,
    &MODEL_SUGARSCAPE,
    &MODEL_SCHELLING,
    &MODEL_LIFE,
    &MODEL_ECA,
    &MODEL_ANT,
    &MODEL_FORESTFIRE,
};

const int MODEL_COUNT = (int)(sizeof(MODELS) / sizeof(MODELS[0]));
