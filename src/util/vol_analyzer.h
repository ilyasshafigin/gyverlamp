#pragma once
#include <Arduino.h>
#include "fast_filter.h"

// From https://github.com/AlexGyver/GyverLamp2/blob/main/firmware/GyverLamp2/VolAnalyzer.h
// (c) AlexGyver

class VolAnalyzer {
public:
  VolAnalyzer(int16_t pin = -1) {
    volF.setDt(20);
    volF.setPass(FF_PASS_MAX);
    maxF.setPass(FF_PASS_MAX);
    setVolK(25);
    setAmpliK(31);
    if (pin != -1) setPin(pin);
  }

  void setPin(int16_t pin) {
    _pin = pin;
    pinMode(_pin, INPUT);
  }
  void setDt(uint32_t dt) { _dt = dt; }
  void setPeriod(uint32_t period) { _period = period; }
  void setVolDt(uint32_t volDt) { volF.setDt(volDt); }
  void setAmpliDt(uint32_t ampliDt) { _ampliDt = ampliDt; }
  void setWindow(uint16_t window) { _window = window; }
  void setVolK(uint8_t k) { volF.setK(k); }
  void setAmpliK(uint8_t k) { maxF.setK(k); minF.setK(k); }
  void setVolMin(int32_t scale) { _volMin = scale; }
  void setVolMax(int32_t scale) { _volMax = scale; }
  void setTrsh(int32_t trsh) { _trsh = trsh; }

  int32_t getRaw() { return raw; }
  int32_t getRawMax() { return rawMax; }
  int32_t getVol() { return volF.getFil(); }
  int32_t getMin() { return minF.getFil(); }
  int32_t getMax() { return maxF.getFil(); }
  bool getPulse() {
    if (_pulse) {
      _pulse = false;
      return true;
    }
    return false;
  }

  bool tick() {
    if (_pin < 0) return false;
    return tickSample(analogRead(_pin));
  }

  bool tickSample(int32_t sample) {
    const uint32_t nowMs = millis();
    const uint32_t nowUs = micros();

    volF.compute(nowMs);
    // период сглаживания амплитуды
    if (nowMs - tmr3 >= _ampliDt) {
      tmr3 = nowMs;
      maxF.setRaw(maxs);
      minF.setRaw(mins);
      maxF.compute(nowMs);
      minF.compute(nowMs);
      maxs = 0;
      mins = 1023;
    }
    // период между захватом сэмплов
    if (_period == 0 || nowMs - tmr1 >= _period) {
      // период выборки
      if (_dt == 0 || nowUs - tmr2 >= _dt) {
        tmr2 = nowUs;

        // ищем максимум
        if (sample > max) max = sample;

        if (!_first) {
          _first = 1;
          maxF.setFil(sample);
          minF.setFil(sample);
        }

        // выборка завершена
        if (++count >= _window) {
          tmr1 = nowMs;
          raw = max;
          // максимумы среди максимумов
          if (max > maxs) maxs = max;
          // минимумы реди максимумов
          if (max < mins) mins = max;
          rawMax = maxs;
          // проверка выше максимума
          maxF.checkPass(max);
          const int32_t minValue = minF.getFil();
          const int32_t maxValue = maxF.getFil();
          const int32_t range = maxValue > minValue ? maxValue - minValue : 0;
          // если окно громкости меньше порого то 0
          if (range < _trsh) {
            max = 0;
          }
          // перевод в громкость
          else {
            max = map(constrain(max, minValue, maxValue), minValue, maxValue, _volMin, _volMax);
          }
          // фильтр столбика громкости
          volF.setRaw(max);
          // проверка выше максимума
          if (volF.checkPass(max)) _pulse = 1;

          // выборка завершена
          max = count = 0;

          return true;
        }
      }
    }
    return false;
  }

private:
  int16_t _pin = -1;
  uint32_t _dt = 500;           // 500 мкс между сэмплами достаточно для музыки
  uint32_t _period = 4;         // 4 мс между выборами достаточно
  uint32_t _ampliDt = 150;
  uint16_t _window = 20;        // при таком размере окна получаем длительность оцифровки вполне хватает
  uint32_t tmr1 = 0, tmr2 = 0, tmr3 = 0;
  int32_t raw = 0;
  int32_t rawMax = 0;
  int32_t max = 0;
  uint16_t count = 0;
  int32_t maxs = 0;
  int32_t mins = 1023;
  int32_t _volMin = 0;
  int32_t _volMax = 100;
  int32_t _trsh = 30;
  bool _pulse = 0, _first = 0;
  FastFilter minF, maxF, volF;
};
