#pragma once

#include <Arduino.h>

class FadeAnimator {
public:
  void snapTo(uint8_t value);
  void fadeTo(uint8_t target, uint16_t durationMs, uint32_t now = millis());

  bool tick(uint32_t now = millis());

  uint8_t value() const { return value_; }
  uint8_t target() const { return to_; }
  bool isRunning() const { return value_ != to_; }

private:
  uint8_t from_ = 0;
  uint8_t to_ = 0;
  uint8_t value_ = 0;
  uint32_t startedMs_ = 0;
  uint16_t durationMs_ = 0;
};
