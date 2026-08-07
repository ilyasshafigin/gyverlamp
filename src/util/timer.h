#pragma once

#include <Arduino.h>
#include <functional>

class Timer {
public:
  typedef std::function<void()> CallBackType;

  explicit Timer(unsigned long intervalMs)
    : interval_(intervalMs),
      callback_(nullptr),
      lastRun_(0),
      running_(false) {}

  Timer(unsigned long intervalMs, CallBackType cb)
    : interval_(intervalMs),
      callback_(cb),
      lastRun_(0),
      running_(false) {}

  void setOnTimer(CallBackType cb) { callback_ = cb; }

  void setInterval(unsigned long intervalMs) { interval_ = intervalMs; }

  void start() {
    lastRun_ = millis();
    running_ = true;
  }

  void stop() { running_ = false; }

  void update() {
    if (!running_ || callback_ == nullptr) {
      return;
    }

    const unsigned long now = millis();
    if (now - lastRun_ >= interval_) {
      lastRun_ = now;
      callback_();
    }
  }

private:
  unsigned long interval_;
  CallBackType callback_;
  unsigned long lastRun_;
  bool running_;
};
