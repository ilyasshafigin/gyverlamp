#include "microphone.h"

#ifdef USE_ADC

#include <FastLED.h>
#include "../config.h"
#include "../util/fft.h"

#ifdef SIMULATOR_AUDIO_INPUT
#include "sim_audio_input.h"
#endif

namespace {

  // Сглаживание звука. Вверх - выстро, вниз - медленно
  static uint8_t smoothAudio(uint8_t current, uint8_t target) {
    const uint8_t k = target > current ? 128 : 64;
    return current + ((static_cast<int16_t>(target) - current) * k) / 255;
  }

#ifdef SIMULATOR_AUDIO_INPUT
  static uint8_t scaleSimAudioValue(uint32_t raw, uint32_t& peak, uint32_t noiseFloor) {
    if (raw > peak) {
      peak = raw;
    } else {
      peak -= peak / 48;
    }
    if (peak < noiseFloor * 2) peak = noiseFloor * 2;

    if (raw <= noiseFloor) return 0;
    const uint32_t range = peak > noiseFloor ? peak - noiseFloor : 1;
    const uint32_t scaled = ((raw - noiseFloor) * 255UL) / range;
    return constrain(scaled, 0UL, 255UL);
  }
#endif

}

void Microphone::init() {
  _vol.setDt(0);
  _vol.setPeriod(5);
  _vol.setWindow(4);
  _vol.setVolK(26);
  _vol.setTrsh(12);
  _vol.setVolMin(0);
  _vol.setVolMax(255);

  _low.setDt(0);
  _low.setPeriod(0);
  _low.setWindow(0);
  _low.setVolK(26);
  _low.setTrsh(50);
  _low.setVolMin(0);
  _low.setVolMax(255);

  _high.setDt(0);
  _high.setPeriod(0);
  _high.setWindow(0);
  _high.setVolK(26);
  _high.setTrsh(50);
  _high.setVolMin(0);
  _high.setVolMax(255);

  _frame = AudioFrame{};
  _lastTickMs = 0;
}

void Microphone::tick() {
#ifdef SIMULATOR_AUDIO_INPUT
  if (!sim::audioEnabled()) {
    _frame = AudioFrame{};
    return;
  }
#endif

  const uint32_t nowMs = millis();
  if (nowMs - _lastTickMs < 30) return;
  _lastTickMs = nowMs;

#ifdef SIMULATOR_AUDIO_INPUT
  sim::audioClearUnderrun();
#endif

  int32_t raw[FFT_SIZE];
  uint32_t spectr[FFT_SIZE];
  int32_t rawMin = 1023;
  int32_t rawMax = 0;
  int32_t rawSum = 0;

  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    const int32_t sample = analogRead(A0);
    raw[i] = sample;
    rawSum += sample;

    if (sample < rawMin) rawMin = sample;
    if (sample > rawMax) rawMax = sample;
  }

#ifdef SIMULATOR_AUDIO_INPUT
  if (sim::audioHadUnderrun()) {
    _frame = AudioFrame{};
    return;
  }
#endif

  const int16_t dcOffset = rawSum / FFT_SIZE;
  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    raw[i] -= dcOffset;
  }

  const uint32_t levelRaw = rawMax - rawMin;
  _vol.tickSample(levelRaw);

  FFT(raw, spectr);
  int32_t lowRaw = 0;
  int32_t highRaw = 0;
  for (uint16_t i = 1; i < FFT_SIZE / 2; i++) {
    spectr[i] = (spectr[i] * (i + 2)) >> 1;
    if (i < 3) {
      lowRaw += spectr[i];
    } else {
      highRaw += spectr[i];
    }
  }
  _low.tickSample(lowRaw);
  _high.tickSample(highRaw);

#ifdef SIMULATOR_AUDIO_INPUT
  static uint32_t simLevelPeak = 0;
  static uint32_t simLowPeak = 0;
  static uint32_t simHighPeak = 0;
  const uint8_t newLevel = scaleSimAudioValue(levelRaw, simLevelPeak, 16);
  const uint8_t newBass = scaleSimAudioValue(lowRaw, simLowPeak, 1);
  const uint8_t newTreble = scaleSimAudioValue(highRaw, simHighPeak, 1);
#else
  const uint8_t newLevel = constrain(_vol.getVol(), 0, 255);
  const uint8_t newBass = constrain(_low.getVol(), 0, 255);
  const uint8_t newTreble = constrain(_high.getVol(), 0, 255);
#endif

  _frame.level = smoothAudio(_frame.level, newLevel);
  _frame.bass = smoothAudio(_frame.bass, newBass);
  _frame.treble = smoothAudio(_frame.treble, newTreble);
  _frame.beat = _vol.getPulse();
  _frame.available = true;
}

#else

void Microphone::init() {}

void Microphone::tick() {
  _frame.available = false;
}

#endif
