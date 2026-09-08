#pragma once

#include <cstdint>

// Shared Arduino-compatible pseudo-random state for every simulator
// translation unit. Effects which call randomSeed() intentionally replace it.
void sim_random_seed(uint32_t seed);
uint32_t sim_random_uint32();
