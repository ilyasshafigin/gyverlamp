#pragma once
#include <Arduino.h>
#include "fast_filter.h"

// From https://github.com/AlexGyver/GyverLamp2/blob/main/firmware/GyverLamp2/VolAnalyzer.h
// (c) AlexGyver

class VolAnalyzer {
public:
  VolAnalyzer(int16_t pin = -1) {
    volF_.setDt(20);
    volF_.setPass(FF_PASS_MAX);
    maxF_.setPass(FF_PASS_MAX);
    setVolK(25);
    setAmpliK(31);
    if (pin != -1) setPin(pin);
  }

  void setPin(int16_t pin) {
    pin_ = pin;
    pinMode(pin_, INPUT);
  }
  void setDt(uint32_t dt) { dt_ = dt; }
  void setPeriod(uint32_t period) { period_ = period; }
  void setVolDt(uint32_t volDt) { volF_.setDt(volDt); }
  void setAmpliDt(uint32_t ampliDt) { ampliDt_ = ampliDt; }
  void setWindow(uint16_t window) { window_ = window; }
  void setVolK(uint8_t k) { volF_.setK(k); }
  void setAmpliK(uint8_t k) {
    maxF_.setK(k);
    minF_.setK(k);
  }
  void setVolMin(int32_t scale) { volMin_ = scale; }
  void setVolMax(int32_t scale) { volMax_ = scale; }
  void setTrsh(int32_t trsh) { trsh_ = trsh; }

  int32_t getRaw() { return raw_; }
  int32_t getRawMax() { return rawMax_; }
  int32_t getVol() { return volF_.getFil(); }
  int32_t getMin() { return minF_.getFil(); }
  int32_t getMax() { return maxF_.getFil(); }
  bool getPulse() {
    if (pulse_) {
      pulse_ = false;
      return true;
    }
    return false;
  }

  bool tick() {
    if (pin_ < 0) return false;
    return tickSample(analogRead(pin_));
  }

  bool tickSample(int32_t sample) {
    const uint32_t nowMs = millis();
    const uint32_t nowUs = micros();

    volF_.compute(nowMs);
    // период сглаживания амплитуды
    if (nowMs - tmr3_ >= ampliDt_) {
      tmr3_ = nowMs;
      maxF_.setRaw(maxs_);
      minF_.setRaw(mins_);
      maxF_.compute(nowMs);
      minF_.compute(nowMs);
      maxs_ = 0;
      mins_ = 1023;
    }
    // период между захватом сэмплов
    if (period_ == 0 || nowMs - tmr1_ >= period_) {
      // период выборки
      if (dt_ == 0 || nowUs - tmr2_ >= dt_) {
        tmr2_ = nowUs;

        // ищем максимум
        if (sample > max_) max_ = sample;

        if (!first_) {
          first_ = 1;
          maxF_.setFil(sample);
          minF_.setFil(sample);
        }

        // выборка завершена
        if (++count_ >= window_) {
          tmr1_ = nowMs;
          raw_ = max_;
          // максимумы среди максимумов
          if (max_ > maxs_) maxs_ = max_;
          // минимумы реди максимумов
          if (max_ < mins_) mins_ = max_;
          rawMax_ = maxs_;
          // проверка выше максимума
          maxF_.checkPass(max_);
          const int32_t minValue = minF_.getFil();
          const int32_t maxValue = maxF_.getFil();
          const int32_t range = maxValue > minValue ? maxValue - minValue : 0;
          // если окно громкости меньше порого то 0
          if (range < trsh_) {
            max_ = 0;
          }
          // перевод в громкость
          else {
            max_ = map(constrain(max_, minValue, maxValue), minValue, maxValue, volMin_, volMax_);
          }
          // фильтр столбика громкости
          volF_.setRaw(max_);
          // проверка выше максимума
          if (volF_.checkPass(max_)) pulse_ = 1;

          // выборка завершена
          max_ = count_ = 0;

          return true;
        }
      }
    }
    return false;
  }

private:
  int16_t pin_ = -1;
  uint32_t dt_ = 500;   // 500 мкс между сэмплами достаточно для музыки
  uint32_t period_ = 4; // 4 мс между выборами достаточно
  uint32_t ampliDt_ = 150;
  uint16_t window_ = 20; // при таком размере окна получаем длительность оцифровки вполне хватает
  uint32_t tmr1_ = 0, tmr2_ = 0, tmr3_ = 0;
  int32_t raw_ = 0;
  int32_t rawMax_ = 0;
  int32_t max_ = 0;
  uint16_t count_ = 0;
  int32_t maxs_ = 0;
  int32_t mins_ = 1023;
  int32_t volMin_ = 0;
  int32_t volMax_ = 100;
  int32_t trsh_ = 30;
  bool pulse_ = 0, first_ = 0;
  FastFilter minF_, maxF_, volF_;
};
