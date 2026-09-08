#pragma once

#include <cstdint>

using byte = uint8_t;
using word = uint16_t;

#define PROGMEM
#define PGM_P const char*
#define pgm_read_byte(addr) (*(reinterpret_cast<const uint8_t*>(addr)))
#define pgm_read_dword(addr) (*(reinterpret_cast<const uint32_t*>(addr)))

extern uint32_t fastled_parity_millis;

extern "C" inline uint32_t millis(void) { return fastled_parity_millis; }
