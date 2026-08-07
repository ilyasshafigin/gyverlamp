#include "loop_profiler.h"

#ifdef PROFILE_LOOP

LoopProfiler::Sample LoopProfiler::samples_[LoopProfiler::SECTION_COUNT];
uint32_t LoopProfiler::startUs_ = 0;
LoopProfiler::Section LoopProfiler::current_ = LoopProfiler::SECTION_COUNT;
uint32_t LoopProfiler::resetTimer_ = 0;

const char* LoopProfiler::sectionName(Section section) {
  switch (section) {
    case ROTATION: return "rotation";
    case AUDIO: return "audio";
    case RENDER: return "render";
    case EFFECT_RENDER: return "effect_render";
    case LEDS_SHOW: return "leds_show";
    case SETTINGS: return "settings";
    case TIME: return "time";
    case BUTTON: return "button";
    case WIFI: return "wifi";
    case WEB: return "web";
    case MQTT: return "mqtt";
    case MQTT_TIMER: return "mqtt_timer";
    case MQTT_LOOP: return "mqtt_loop";
    case UDP: return "udp";
    default: return "unknown";
  }
}

void LoopProfiler::begin(Section section) {
  current_ = section;
  startUs_ = micros();
}

void LoopProfiler::end(Section section) {
  if (section != current_) return;
  uint32_t elapsed = micros() - startUs_;
  Sample& s = samples_[section];
  s.lastUs = elapsed;
  if (elapsed > s.maxUs) s.maxUs = elapsed;
  current_ = SECTION_COUNT;
}

void LoopProfiler::resetMax() {
  for (uint8_t i = 0; i < SECTION_COUNT; i++) {
    samples_[i].maxUs = 0;
  }
}

void LoopProfiler::tick() {
  const uint32_t now = millis();
  if (now - resetTimer_ >= RESET_INTERVAL_MS) {
    resetTimer_ = now;
    resetMax();
  }
}

#endif
