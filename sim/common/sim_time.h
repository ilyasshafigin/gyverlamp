#pragma once

#include <cstdint>

// Global host-emulated millis used by the Arduino shim (millis()/micros()).
// Defined in one shared translation unit so the WASM runner sees a consistent
// time source.
extern uint32_t sim_millis;

// UTC Unix epoch milliseconds used by the host TimeService. It never reads the
// machine clock, so calendar-dependent effects are reproducible across hosts.
void sim_set_clock_start_utc_ms(uint64_t epoch_ms);
uint64_t sim_clock_utc_ms();
void sim_set_deterministic_clock(bool enabled);
bool sim_uses_deterministic_clock();
