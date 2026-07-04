#pragma once

#include "../effect.h"

class EffectOctopus : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Octopus;
  static constexpr const char* NAME = "Octopus";
  static constexpr EffectSettingsSpec SETTINGS = {
    255,  // brightness
    200,  // speed
    30,   // scale
  };
  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
