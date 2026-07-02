#pragma once

// Minimal mirror of src/effect/shared.h for the simulator.
// Keeps the same global names and functions used by the first integrated
// effect (EffectColorChange).

#include "FastLED.h"
#include "config.h"

constexpr uint8_t NUM_LAYERSMAX = 2;

extern uint8_t hue, hue2;
extern uint8_t deltaHue, deltaHue2;
extern uint8_t step;
extern uint8_t pcnt;
extern uint8_t deltaValue;

extern CRGBPalette16 rgbPalette;

bool everyMs(uint8_t& latch, uint16_t intervalMs, uint32_t nowMs);
void resetEveryMs(uint8_t& latch, uint16_t intervalMs, uint32_t nowMs);
