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
    _k1 = k;
    _k2 = 32 - k;
  }
  void setDt(uint32_t dt) { _dt = dt; }
  void setPass(uint8_t pass) { _pass = pass; }
  void setRaw(int32_t raw) { _raw = raw; }
  void setFil(int32_t fil) { _raw_f = fil; }
  int32_t getRaw() { return _raw; }
  int32_t getFil() { return _raw_f; }

  bool checkPass(int32_t val) {
    if (_pass == FF_PASS_MAX && val > _raw_f) {
      _raw_f = val;
      return true;
    } else if (_pass == FF_PASS_MIN && val < _raw_f) {
      _raw_f = val;
      return true;
    }
    return false;
  }

  void compute(const uint32_t nowMs) {
    if (_dt == 0 || nowMs - _tmr >= _dt) {
      _tmr = nowMs;
      _raw_f = (_k1 * _raw_f + _k2 * _raw) >> 5;
      //_raw_f = static_cast<int32_t>((static_cast<int64_t>(_k1) * _raw_f + static_cast<int64_t>(_k2) * _raw) >> 5);
    }
  }

private:
  uint32_t _tmr = 0;
  uint32_t _dt = 0;
  uint8_t _k1 = 20, _k2 = 12;
  uint8_t _pass = 0;
  int32_t _raw_f = 0, _raw = 0;
};
