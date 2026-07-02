#pragma once

#include "../effect.h"

class EffectPaintball : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Paintball;
  static constexpr const char* NAME = "Paintball";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    30,   // speed
    40,   // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
