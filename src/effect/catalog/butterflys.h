#pragma once

#include "../effect.h"

class EffectButterflys : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Butterflys;
  static constexpr const char* NAME = "Butterflys";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    140,  // speed
    255,  // scale: 1 - цвет плавно меняется, 255 - желтоватый цвет, остальное - настройка цвета
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
