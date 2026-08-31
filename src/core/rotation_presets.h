#pragma once

#include <Arduino.h>
#include <string.h>

#include "rotation_mode.h"

constexpr uint8_t kRotationPresetCount = 9;

// Index used when no preset applies (unknown label, out-of-range, etc.).
// Index 3 = "1m" (60s), matches kRotationIntervalSecDefault.
constexpr uint8_t kRotationPresetDefaultIndex = 3;

constexpr uint16_t kRotationPresetSeconds[kRotationPresetCount] = {15, 30, 45, 60, 120, 180, 300, 600, 900};

constexpr const char* const kRotationPresetLabels[kRotationPresetCount] = {
  "15s", "30s", "45s", "1m", "2m", "3m", "5m", "10m", "15m"
};

// Presets must stay inside the EEPROM-validated range so snapped values
// never violate the [MIN, MAX] guard expected elsewhere.
static_assert(kRotationPresetSeconds[0] >= kRotationIntervalSecMin, "smallest preset below EEPROM min");
static_assert(
  kRotationPresetSeconds[kRotationPresetCount - 1] <= kRotationIntervalSecMax, "largest preset above EEPROM max"
);

// Returns index of nearest preset. On tie, the smaller index wins
// (scan ascending and replace only on strict `<`).
inline uint8_t rotationPresetIndexForSeconds(uint16_t seconds) {
  uint8_t best = 0;
  uint32_t bestDist = static_cast<uint32_t>(
    seconds > kRotationPresetSeconds[0] ? seconds - kRotationPresetSeconds[0] : kRotationPresetSeconds[0] - seconds
  );
  for (uint8_t i = 1; i < kRotationPresetCount; i++) {
    uint32_t d = static_cast<uint32_t>(
      seconds > kRotationPresetSeconds[i] ? seconds - kRotationPresetSeconds[i] : kRotationPresetSeconds[i] - seconds
    );
    if (d < bestDist) {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

inline uint16_t rotationPresetSnapSeconds(uint16_t seconds) {
  return kRotationPresetSeconds[rotationPresetIndexForSeconds(seconds)];
}

inline uint16_t rotationPresetSecondsForIndex(uint8_t index) {
  if (index >= kRotationPresetCount) index = kRotationPresetCount - 1;
  return kRotationPresetSeconds[index];
}

inline const char* rotationPresetLabelForIndex(uint8_t index) {
  if (index >= kRotationPresetCount) index = kRotationPresetCount - 1;
  return kRotationPresetLabels[index];
}

// strcmp loop over labels. If not found, falls back to index 3 ("1m"),
// the previous default, so unknown strings resolve to a sane interval.
inline uint8_t rotationPresetIndexForLabel(const char* label) {
  if (label != nullptr) {
    for (uint8_t i = 0; i < kRotationPresetCount; i++) {
      if (strcmp(label, kRotationPresetLabels[i]) == 0) {
        return i;
      }
    }
  }
  return kRotationPresetDefaultIndex;
}

inline const char* rotationPresetLabelForSeconds(uint16_t seconds) {
  return rotationPresetLabelForIndex(rotationPresetIndexForSeconds(seconds));
}
