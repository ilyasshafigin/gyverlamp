#pragma once

#include "../effect.h"

class EffectThunderstorm : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Thunderstorm;
  static constexpr const char* NAME = "Thunderstorm";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    140,  // speed
    100,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
