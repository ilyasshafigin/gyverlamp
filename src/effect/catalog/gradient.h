#pragma once

#include "../effect.h"

class EffectGradient : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Gradient;
  static constexpr const char* NAME = "Gradient";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    30,   // speed
    40,   // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
