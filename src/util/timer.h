#pragma once

#include <Arduino.h>
#include <functional>

class Timer {
public:
  typedef std::function<void()> CallBackType;

  explicit Timer(unsigned long intervalMs)
    : interval(intervalMs),
      callback(nullptr),
      lastRun(0),
      running(false) {}

  Timer(unsigned long intervalMs, CallBackType cb)
    : interval(intervalMs),
      callback(cb),
      lastRun(0),
      running(false) {}

  void setOnTimer(CallBackType cb) { callback = cb; }

  void setInterval(unsigned long intervalMs) { interval = intervalMs; }

  void start() {
    lastRun = millis();
    running = true;
  }

  void stop() { running = false; }

  void update() {
    if (!running || callback == nullptr) {
      return;
    }

    const unsigned long now = millis();
    if (now - lastRun >= interval) {
      lastRun = now;
      callback();
    }
  }

private:
  unsigned long interval;
  CallBackType callback;
  unsigned long lastRun;
  bool running;
};
