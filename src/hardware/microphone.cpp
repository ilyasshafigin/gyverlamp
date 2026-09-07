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

} // namespace

void Microphone::init() {
#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(10);
  analogSetPinAttenuation(MIC_PIN, ADC_11db);
#endif

  vol_.setDt(0);
  vol_.setPeriod(5);
  vol_.setWindow(4);
  vol_.setVolK(26);
  vol_.setTrsh(12);
  vol_.setVolMin(0);
  vol_.setVolMax(255);

  low_.setDt(0);
  low_.setPeriod(0);
  low_.setWindow(0);
  low_.setVolK(26);
  low_.setTrsh(50);
  low_.setVolMin(0);
  low_.setVolMax(255);

  high_.setDt(0);
  high_.setPeriod(0);
  high_.setWindow(0);
  high_.setVolK(26);
  high_.setTrsh(50);
  high_.setVolMin(0);
  high_.setVolMax(255);

  frame_ = AudioFrame{};
  lastTickMs_ = 0;
}

void Microphone::tick() {
#ifdef SIMULATOR_AUDIO_INPUT
  if (!sim::audioEnabled()) {
    frame_ = AudioFrame{};
    return;
  }
#endif

  const uint32_t nowMs = millis();
  if (nowMs - lastTickMs_ < 30) return;
  lastTickMs_ = nowMs;

#ifdef SIMULATOR_AUDIO_INPUT
  sim::audioClearUnderrun();
#endif

  int32_t raw[FFT_SIZE];
  uint32_t spectr[FFT_SIZE];
  int32_t rawMin = PLATFORM_ADC_INPUT_MAX;
  int32_t rawMax = PLATFORM_ADC_INPUT_MIN;
  int32_t rawSum = 0;

  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    const int32_t sample = analogRead(MIC_PIN);
    raw[i] = sample;
    rawSum += sample;

    if (sample < rawMin) rawMin = sample;
    if (sample > rawMax) rawMax = sample;
  }

#ifdef SIMULATOR_AUDIO_INPUT
  if (sim::audioHadUnderrun()) {
    frame_ = AudioFrame{};
    return;
  }
#endif

  const int16_t dcOffset = rawSum / FFT_SIZE;
  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    raw[i] -= dcOffset;
  }

  const uint32_t levelRaw = rawMax - rawMin;
  vol_.tickSample(levelRaw);

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
  low_.tickSample(lowRaw);
  high_.tickSample(highRaw);

#ifdef SIMULATOR_AUDIO_INPUT
  static uint32_t simLevelPeak = 0;
  static uint32_t simLowPeak = 0;
  static uint32_t simHighPeak = 0;
  const uint8_t newLevel = scaleSimAudioValue(levelRaw, simLevelPeak, 16);
  const uint8_t newBass = scaleSimAudioValue(lowRaw, simLowPeak, 1);
  const uint8_t newTreble = scaleSimAudioValue(highRaw, simHighPeak, 1);
#else
  const uint8_t newLevel = constrain(vol_.getVol(), 0, 255);
  const uint8_t newBass = constrain(low_.getVol(), 0, 255);
  const uint8_t newTreble = constrain(high_.getVol(), 0, 255);
#endif

  frame_.level = smoothAudio(frame_.level, newLevel);
  frame_.bass = smoothAudio(frame_.bass, newBass);
  frame_.treble = smoothAudio(frame_.treble, newTreble);
  frame_.beat = vol_.getPulse();
  frame_.available = true;
}

#else

void Microphone::init() {
}

void Microphone::tick() {
  frame_.available = false;
}

#endif
