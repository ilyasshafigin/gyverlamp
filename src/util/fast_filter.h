#pragma once
#include <Arduino.h>

// From https://github.com/AlexGyver/GyverLamp2/blob/main/firmware/GyverLamp2/FastFilter.h
// (c) AlexGyver

#define FF_SCALE 0
#define FF_PASS_MAX 1
#define FF_PASS_MIN 2

class FastFilter {
public:
  FastFilter(uint8_t k = 20, uint32_t dt = 0) {
    setK(k);
    setDt(dt);
  }

  void setK(uint8_t k) {
    k1_ = k;
    k2_ = 32 - k;
  }
  void setDt(uint32_t dt) { dt_ = dt; }
  void setPass(uint8_t pass) { pass_ = pass; }
  void setRaw(int32_t raw) { raw_ = raw; }
  void setFil(int32_t fil) { rawF_ = fil; }
  int32_t raw() { return raw_; }
  int32_t fil() { return rawF_; }

  bool checkPass(int32_t val) {
    const bool passed = (pass_ == FF_PASS_MAX && val > rawF_) || (pass_ == FF_PASS_MIN && val < rawF_);
    if (!passed) return false;

    rawF_ = val;
    return true;
  }

  void compute(const uint32_t nowMs) {
    if (dt_ == 0 || nowMs - tmr_ >= dt_) {
      tmr_ = nowMs;
      rawF_ = (k1_ * rawF_ + k2_ * raw_) >> 5;
      //rawF_ = static_cast<int32_t>((static_cast<int64_t>(k1_) * rawF_ + static_cast<int64_t>(k2_) * raw_) >> 5);
    }
  }

private:
  uint32_t tmr_ = 0;
  uint32_t dt_ = 0;
  uint8_t k1_ = 20, k2_ = 12;
  uint8_t pass_ = 0;
  int32_t rawF_ = 0, raw_ = 0;
};
