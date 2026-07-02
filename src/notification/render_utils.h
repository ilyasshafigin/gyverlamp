#pragma once

#include <Arduino.h>
#include <FastLED.h>

inline uint8_t notificationBreathe(
  uint32_t startedMs,
  uint16_t periodMs,
  uint8_t minBrightness,
  uint8_t maxBrightness
) {
  const uint32_t elapsed = millis() - startedMs;
  const uint16_t phase = (elapsed % periodMs) * 255UL / periodMs;

  // начинается с минимума
  const uint8_t wave = sin8(phase + 192);

  return minBrightness + scale8(wave, maxBrightness - minBrightness);
}
