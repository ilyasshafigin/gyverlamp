#pragma once

#include "../effect.h"

class EffectLiquidLamp : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::LiquidLamp;
  static constexpr const char* kName = "Liquid Lamp";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    120, // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
