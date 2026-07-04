#pragma once

#include "../effect.h"

class EffectSnowstorm : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Snowstorm;
  static constexpr const char* NAME = "Snowstorm";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    180, // speed
    100, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
