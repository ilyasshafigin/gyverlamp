#pragma once

#include "../effect.h"

class EffectGradient : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Gradient;
  static constexpr const char* kName = "Gradient";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    30,  // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
