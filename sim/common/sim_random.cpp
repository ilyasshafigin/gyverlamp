#include "sim_random.h"

namespace {
  constexpr uint32_t kDefaultSeed = 123456789;
  uint32_t g_rngState = kDefaultSeed;
} // namespace

void sim_random_seed(uint32_t seed) {
  g_rngState = seed ? seed : kDefaultSeed;
}

uint32_t sim_random_uint32() {
  uint32_t value = g_rngState;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  g_rngState = value;
  return value;
}
