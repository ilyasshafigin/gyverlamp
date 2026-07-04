#pragma once

#include "../effect.h"

class EffectNexus : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Nexus;
  static constexpr const char* NAME = "Nexus";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    70,  // speed
    100, // scale
    EFFECT_PARAM_SPEED
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
