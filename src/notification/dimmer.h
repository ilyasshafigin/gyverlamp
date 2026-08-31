#pragma once

#include <Arduino.h>
#include "../hardware/led.h"

class NotificationDimmer {
public:
  explicit NotificationDimmer(Led& led)
    : led_(led) {}

  void prepareFrame(bool backgroundUpdated, uint8_t targetDim, uint32_t durationMs = 0);
  void prepareEndingFrame(bool backgroundUpdated, uint8_t targetDim, uint32_t endingStartedMs);
  void reset();

  static constexpr uint16_t kFadeInMs = 600;
  static constexpr uint16_t kFadeOutMs = 600;

private:
  Led& led_;
  bool wasActive_ = false;
  uint32_t startedMs_ = 0;
  uint8_t appliedDim_ = 0;

  uint8_t currentAmount(uint8_t targetDim, uint32_t durationMs) const;
  void apply(uint8_t amount, bool backgroundUpdated);
};
