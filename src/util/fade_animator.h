#pragma once

#include <Arduino.h>

class FadeAnimator {
public:
  void snapTo(uint8_t value);
  void fadeTo(uint8_t target, uint16_t durationMs, uint32_t now = millis());

  bool tick(uint32_t now = millis());

  uint8_t value() const { return _value; }
  uint8_t target() const { return _to; }
  bool isRunning() const { return _value != _to; }

private:
  uint8_t _from = 0;
  uint8_t _to = 0;
  uint8_t _value = 0;
  uint32_t _startedMs = 0;
  uint16_t _durationMs = 0;
};
