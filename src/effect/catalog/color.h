#pragma once

#include "../effect.h"

class EffectColor : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Color;
  static constexpr const char* NAME = "Color";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    30,   // speed
    40,   // scale
  };

  void render(EffectContext& ctx) override;
};
