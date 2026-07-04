#pragma once

#include "../effect.h"

class EffectNoise : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Noise;
  static constexpr const char* NAME = "Noise";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    15,  // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
