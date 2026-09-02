#pragma once

#include <Arduino.h>

class LinearU8RateLimiter {
public:
  void snapTo(uint8_t value, uint32_t now = millis()) {
    from_ = value;
    target_ = value;
    value_ = value;
    startedMs_ = now;
    durationMs_ = 0;
  }

  void setTarget(uint8_t target, uint16_t ratePerSecond, uint8_t snapThreshold, uint32_t now = millis()) {
    tick(now);
    if (target == target_) return;

    const uint16_t delta = target > value_ ? target - value_ : value_ - target;
    if (delta <= snapThreshold || ratePerSecond == 0) {
      snapTo(target, now);
      return;
    }

    from_ = value_;
    target_ = target;
    startedMs_ = now;
    durationMs_ = (static_cast<uint32_t>(delta) * 1000UL + ratePerSecond - 1U) / ratePerSecond;
  }

  bool tick(uint32_t now = millis()) {
    if (value_ == target_) return false;

    if (durationMs_ == 0) {
      value_ = target_;
      return true;
    }

    const uint32_t elapsed = now - startedMs_;
    if (elapsed >= durationMs_) {
      value_ = target_;
      return true;
    }

    const int16_t delta = static_cast<int16_t>(target_) - static_cast<int16_t>(from_);
    const int32_t offset =
      (static_cast<int32_t>(delta) * static_cast<int32_t>(elapsed)) / static_cast<int32_t>(durationMs_);
    value_ = static_cast<uint8_t>(static_cast<int16_t>(from_) + offset);
    return true;
  }

  uint8_t value() const { return value_; }
  bool isRunning() const { return value_ != target_; }

private:
  uint8_t from_ = 0;
  uint8_t target_ = 0;
  uint8_t value_ = 0;
  uint32_t startedMs_ = 0;
  uint32_t durationMs_ = 0;
};
