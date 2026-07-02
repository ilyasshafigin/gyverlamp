#pragma once

#include <Arduino.h>
#include "../hardware/led.h"

class NotificationDimmer {
public:
  explicit NotificationDimmer(Led& led) : _led(led) {}

  void prepareFrame(bool backgroundUpdated, uint8_t targetDim, uint32_t durationMs = 0);
  void prepareEndingFrame(bool backgroundUpdated, uint8_t targetDim, uint32_t endingStartedMs);
  void reset();

  static constexpr uint16_t FADE_IN_MS = 600;
  static constexpr uint16_t FADE_OUT_MS = 600;

private:
  Led& _led;
  bool _wasActive = false;
  uint32_t _startedMs = 0;
  uint8_t _appliedDim = 0;

  uint8_t currentAmount(uint8_t targetDim, uint32_t durationMs) const;
  void apply(uint8_t amount, bool backgroundUpdated);
};
