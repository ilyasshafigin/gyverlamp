#pragma once

#include <Arduino.h>
#include "../config.h"

class LoopProfiler {
public:
  enum Section : uint8_t {
    ROTATION = 0,
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
    MQTT_TIMER,
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

  static void begin(Section section);
  static void end(Section section);
  static void resetMax();
  static void tick();

  template <typename Func> static void measure(Section section, Func&& func) {
    begin(section);
    func();
    end(section);
  }

  static const Sample& get(Section section) { return _samples[section]; }

private:
  static Sample _samples[Section::SECTION_COUNT];
  static uint32_t _startUs;
  static Section _current;
  static uint32_t _resetTimer;
  static constexpr uint32_t RESET_INTERVAL_MS = 10000;

#else
  static const char* sectionName(Section) { return ""; }
  static void begin(Section) {}
  static void end(Section) {}
  static void resetMax() {}
  static void tick() {}
  template <typename Func> static void measure(Section section, Func&& func) { func(); }
#endif
};
