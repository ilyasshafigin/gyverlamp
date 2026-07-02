#pragma once

#include <cstdint>

// Global host-emulated millis used by the Arduino shim (millis()/micros()).
// Defined in one shared translation unit so the WASM runner sees a consistent
// time source.
extern uint32_t sim_millis;
