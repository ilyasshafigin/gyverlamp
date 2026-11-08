#pragma once

#include "../effect.h"

class EffectNorthernLights : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::NorthernLights;
  static constexpr const char* kName = "Northern Lights";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    120, // speed
    100, // scale: 255 - плавное изменение цвета, остальное - настройка цвета
    Effects::EFFECT_SPEC_USES_AUDIO,
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
