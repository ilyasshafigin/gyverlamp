#pragma once

#include "../effect.h"

class EffectButterflys : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Butterflys;
  static constexpr const char* kName = "Butterflys";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    140, // speed
    255, // scale: 1 - цвет плавно меняется, 255 - желтоватый цвет, остальное - настройка цвета
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
