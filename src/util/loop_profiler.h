#pragma once

#include <Arduino.h>

#include "../config.h"

class LoopProfiler {
public:
  enum Section : uint8_t {
    LOOP = 0,
    ROTATION,
    AUDIO,
    RENDER,
    EFFECT_RENDER,
    LEDS_SHOW,
    SETTINGS,
    TIME,
    BUTTON,
    WIFI,
    OTA,
    WEB,
    MQTT,
    MQTT_TELEMETRY_TIMER,
    MQTT_STATE_REFRESH_TIMER,
    MQTT_LOOP,
    UDP,
    SECTION_COUNT
  };

#ifdef PROFILE_LOOP
  struct Sample {
    uint32_t lastUs = 0;
    uint32_t maxUs = 0;
  };

  static const char* sectionName(Section section);

  static void tick();

  template <typename Func> static void measure(Section section, Func&& func) {
    const uint32_t startUs = micros();
    func();
    record(section, micros() - startUs);
  }

  static const Sample& sample(Section section) { return samples_[section]; }

private:
  static Sample samples_[Section::SECTION_COUNT];
  static void record(Section section, uint32_t elapsedUs);
  static void resetMax();
  static uint32_t resetTimer_;
  static constexpr uint32_t kResetIntervalMs = 10000;

#else
  static const char* sectionName(Section) { return ""; }
  static void tick() {}
  template <typename Func> static void measure(Section, Func&& func) { func(); }
#endif
};
