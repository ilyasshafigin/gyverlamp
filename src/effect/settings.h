#pragma once

#include <Arduino.h>

enum EffectParam : uint8_t {
  EFFECT_PARAM_BRIGHTNESS = 1 << 0,
  EFFECT_PARAM_SPEED = 1 << 1,
  EFFECT_PARAM_SCALE = 1 << 2,
};

struct EffectSettingsSpec {
  uint8_t defaultBrightness;
  uint8_t defaultSpeed;
  uint8_t defaultScale;
  uint8_t resetOnChange;
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
