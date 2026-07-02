#pragma once

#include <Arduino.h>

class PeriodicTimer {
public:
  explicit PeriodicTimer(uint32_t intervalMs);
  void setInterval(uint32_t intervalMs);
  bool isReady();
  void reset();

private:
  uint32_t _lastTick = 0;
  uint32_t _intervalMs = 0;
};

inline PeriodicTimer::PeriodicTimer(uint32_t intervalMs)
  : _lastTick(millis()),
    _intervalMs(intervalMs) {}

inline void PeriodicTimer::setInterval(uint32_t intervalMs) {
  _intervalMs = intervalMs;
}

inline bool PeriodicTimer::isReady() {
  const uint32_t now = millis();
  if (now - _lastTick >= _intervalMs) {
    _lastTick = now;
    return true;
  }
  return false;
}

inline void PeriodicTimer::reset() {
  _lastTick = millis();
}
