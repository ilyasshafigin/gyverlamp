#pragma once

#include <Arduino.h>

class PeriodicTimer {
public:
  explicit PeriodicTimer(uint32_t intervalMs);
  void setInterval(uint32_t intervalMs);
  bool isReady();
  void reset();

private:
  uint32_t lastTick_ = 0;
  uint32_t intervalMs_ = 0;
};

inline PeriodicTimer::PeriodicTimer(uint32_t intervalMs)
  : lastTick_(millis()),
    intervalMs_(intervalMs) {
}

inline void PeriodicTimer::setInterval(uint32_t intervalMs) {
  intervalMs_ = intervalMs;
}

inline bool PeriodicTimer::isReady() {
  const uint32_t now = millis();
  if (now - lastTick_ >= intervalMs_) {
    lastTick_ = now;
    return true;
  }
  return false;
}

inline void PeriodicTimer::reset() {
  lastTick_ = millis();
}
