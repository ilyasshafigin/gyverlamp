#include "math.h"

uint8_t mapsin8(uint8_t theta, uint8_t lowest, uint8_t highest) {
  uint8_t beatsin = sin8(theta);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatsin, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

uint8_t mapcos8(uint8_t theta, uint8_t lowest, uint8_t highest) {
  uint8_t beatcos = cos8(theta);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatcos, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

uint8_t beatcos8(accum88 beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t timebase, uint8_t phase_offset) {
  uint8_t beat = beat8(beats_per_minute, timebase);
  uint8_t beatcos = cos8(beat + phase_offset);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatcos, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

uint8_t scaleToWave8(uint8_t x) {
  uint8_t x8 = x % 8U;
  uint8_t x4 = x8 % 4U;
  if (x4 == 0U)
    if (x8 == 0U) return 0U;
    else
      return 255U;
  else if (x8 < 4U)
    // всего 7шт по 36U + 3U лишних = 255U (чтобы восхождение по синусоиде не было зеркально спуску)
    return 1U + x4 * 72U;
  return 253U - x4 * 72U; // 253U = 255U - 2U
}
