#pragma once

#include "../effect.h"

class EffectLiquidLamp : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::LiquidLamp;
  static constexpr const char* NAME = "Liquid Lamp";
  static constexpr EffectSettingsSpec SETTINGS = {
    255,  // brightness
    120,  // speed
    40,   // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
