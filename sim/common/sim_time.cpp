#include "sim_time.h"

uint32_t sim_millis = 0;

namespace {
  uint64_t g_clockStartUtcMs = 0;
  bool g_deterministicClock = false;
} // namespace

void sim_set_clock_start_utc_ms(uint64_t epoch_ms) {
  g_clockStartUtcMs = epoch_ms;
}

uint64_t sim_clock_utc_ms() {
  return g_clockStartUtcMs + sim_millis;
}

void sim_set_deterministic_clock(bool enabled) {
  g_deterministicClock = enabled;
}

bool sim_uses_deterministic_clock() {
  return g_deterministicClock;
}
