#pragma once

#include "../effect.h"

class EffectTwinkles : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Twinkles;
  static constexpr const char* NAME = "Twinkles";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    200,  // speed
    100,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
