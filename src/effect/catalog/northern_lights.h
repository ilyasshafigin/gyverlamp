#pragma once

#include "../effect.h"

class EffectNorthernLights : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::NorthernLights;
  static constexpr const char* NAME = "Northern Lights";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    120, // speed
    100, // scale: 255 - плавное изменение цвета, остальное - настройка цвета
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
