#pragma once

#include <Arduino.h>

struct EffectSettingsSpec {
  uint8_t defaultBrightness;
  uint8_t defaultSpeed;
  uint8_t defaultScale;
};

struct EffectSettings {
  uint8_t brightness = 0;
  uint8_t speed = 0;
  uint8_t scale = 0;

  static inline EffectSettings fromSpec(const EffectSettingsSpec& spec) {
    return {
      spec.defaultBrightness,
      spec.defaultSpeed,
      spec.defaultScale,
    };
  }
};

struct RuntimeEffectSettings {
  uint8_t brightness;
  uint8_t speed;
  uint8_t scale;

  static inline RuntimeEffectSettings fromSettings(const EffectSettings& settings) {
    return {
      settings.brightness,
      settings.speed,
      settings.scale,
    };
  }
};
