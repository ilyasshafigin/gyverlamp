#include "loop_profiler.h"

#ifdef PROFILE_LOOP

LoopProfiler::Sample LoopProfiler::samples_[LoopProfiler::SECTION_COUNT];
uint32_t LoopProfiler::resetTimer_ = 0;

const char* LoopProfiler::sectionName(Section section) {
  switch (section) {
    case LOOP: return "loop";
    case ROTATION: return "rotation";
    case AUDIO: return "audio";
    case RENDER: return "render";
    case EFFECT_RENDER: return "effect_render";
    case LEDS_SHOW: return "leds_show";
    case SETTINGS: return "settings";
    case TIME: return "time";
    case BUTTON: return "button";
    case WIFI: return "wifi";
    case OTA: return "ota";
    case WEB: return "web";
    case MQTT: return "mqtt";
    case MQTT_TELEMETRY_TIMER: return "mqtt_telemetry_timer";
    case MQTT_STATE_REFRESH_TIMER: return "mqtt_state_refresh_timer";
    case MQTT_LOOP: return "mqtt_loop";
    case UDP: return "udp";
    default: return "unknown";
  }
}

void LoopProfiler::record(Section section, uint32_t elapsedUs) {
  Sample& s = samples_[section];
  s.lastUs = elapsedUs;
  if (elapsedUs > s.maxUs) s.maxUs = elapsedUs;
}

void LoopProfiler::resetMax() {
  for (uint8_t i = 0; i < SECTION_COUNT; i++) {
    samples_[i].maxUs = 0;
  }
}

void LoopProfiler::tick() {
  const uint32_t now = millis();
  const uint32_t elapsed = now - resetTimer_;
  if (elapsed < kResetIntervalMs) return;

  Serial.print(F("[PROFILE 10s last/max us]"));
  for (uint8_t i = 0; i < SECTION_COUNT; i++) {
    const Sample& sample = samples_[i];
    Serial.print(' ');
    Serial.print(sectionName(static_cast<Section>(i)));
    Serial.print('=');
    Serial.print(sample.lastUs);
    Serial.print('/');
    Serial.print(sample.maxUs);
  }
  Serial.println();

  resetMax();
  resetTimer_ += (elapsed / kResetIntervalMs) * kResetIntervalMs;
}

#endif
