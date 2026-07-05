#pragma once

#include <Arduino.h>
#include <string.h>

#include "rotation_mode.h"

constexpr uint8_t ROTATION_PRESET_COUNT = 9;

// Index used when no preset applies (unknown label, out-of-range, etc.).
// Index 3 = "1m" (60s), matches ROTATION_INTERVAL_SEC_DEFAULT.
constexpr uint8_t ROTATION_PRESET_DEFAULT_INDEX = 3;

constexpr uint16_t ROTATION_PRESET_SECONDS[ROTATION_PRESET_COUNT] = {15, 30, 45, 60, 120, 180, 300, 600, 900};

constexpr const char* const ROTATION_PRESET_LABELS[ROTATION_PRESET_COUNT] = {
  "15s", "30s", "45s", "1m", "2m", "3m", "5m", "10m", "15m"
};

// Presets must stay inside the EEPROM-validated range so snapped values
// never violate the [MIN, MAX] guard expected elsewhere.
static_assert(ROTATION_PRESET_SECONDS[0] >= ROTATION_INTERVAL_SEC_MIN, "smallest preset below EEPROM min");
static_assert(ROTATION_PRESET_SECONDS[ROTATION_PRESET_COUNT - 1] <= ROTATION_INTERVAL_SEC_MAX, "largest preset above EEPROM max");

// Returns index of nearest preset. On tie, the smaller index wins
// (scan ascending and replace only on strict `<`).
inline uint8_t rotationPresetIndexForSeconds(uint16_t seconds) {
  uint8_t best = 0;
  uint32_t bestDist = static_cast<uint32_t>(
    seconds > ROTATION_PRESET_SECONDS[0] ? seconds - ROTATION_PRESET_SECONDS[0] : ROTATION_PRESET_SECONDS[0] - seconds
  );
  for (uint8_t i = 1; i < ROTATION_PRESET_COUNT; i++) {
    uint32_t d = static_cast<uint32_t>(
      seconds > ROTATION_PRESET_SECONDS[i] ? seconds - ROTATION_PRESET_SECONDS[i] : ROTATION_PRESET_SECONDS[i] - seconds
    );
    if (d < bestDist) {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

inline uint16_t rotationPresetSnapSeconds(uint16_t seconds) {
  return ROTATION_PRESET_SECONDS[rotationPresetIndexForSeconds(seconds)];
}

inline uint16_t rotationPresetSecondsForIndex(uint8_t index) {
  if (index >= ROTATION_PRESET_COUNT) index = ROTATION_PRESET_COUNT - 1;
  return ROTATION_PRESET_SECONDS[index];
}

inline const char* rotationPresetLabelForIndex(uint8_t index) {
  if (index >= ROTATION_PRESET_COUNT) index = ROTATION_PRESET_COUNT - 1;
  return ROTATION_PRESET_LABELS[index];
}

// strcmp loop over labels. If not found, falls back to index 3 ("1m"),
// the previous default, so unknown strings resolve to a sane interval.
inline uint8_t rotationPresetIndexForLabel(const char* label) {
  if (label != nullptr) {
    for (uint8_t i = 0; i < ROTATION_PRESET_COUNT; i++) {
      if (strcmp(label, ROTATION_PRESET_LABELS[i]) == 0) {
        return i;
      }
    }
  }
  return ROTATION_PRESET_DEFAULT_INDEX;
}

inline const char* rotationPresetLabelForSeconds(uint16_t seconds) {
  return rotationPresetLabelForIndex(rotationPresetIndexForSeconds(seconds));
}
