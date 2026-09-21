#pragma once

#include <stdint.h>

namespace PanelRadio {

constexpr uint8_t kCommandQueueCapacity = 8;

inline bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

inline int8_t turnMultiplier(uint32_t intervalMs) {
  return intervalMs < 60 ? 4 : (intervalMs < 120 ? 2 : 1);
}

inline int8_t clampPacketDelta(int16_t delta) {
  return delta < -8 ? -8 : (delta > 8 ? 8 : static_cast<int8_t>(delta));
}

class PairingCombo {
 public:
  PairingCombo() : overlapping_(false), handled_(false), lockedUntil_(0), startedAt_(0) {}

  bool tick(uint32_t now, bool button1Pressed, bool button3Pressed) {
    if (!button1Pressed || !button3Pressed) {
      overlapping_ = false;
      handled_ = false;
      return false;
    }
    if (!overlapping_) {
      overlapping_ = true;
      startedAt_ = now;
    }
    if (!handled_ && static_cast<int32_t>(now - lockedUntil_) >= 0 && elapsed(now, startedAt_, 2500)) {
      handled_ = true;
      lockedUntil_ = now + 5000;
      return true;
    }
    return false;
  }

  bool overlapping() const { return overlapping_; }
  bool locked(uint32_t now) const { return static_cast<int32_t>(now - lockedUntil_) < 0; }

 private:
  bool overlapping_;
  bool handled_;
  uint32_t lockedUntil_;
  uint32_t startedAt_;
};

} // namespace PanelRadio
