#pragma once

#include "../effect.h"

class EffectFire : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Fire;
  static constexpr const char* NAME = "Fire";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    160,  // speed
    15,   // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
