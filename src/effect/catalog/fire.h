#pragma once

#include "../effect.h"

class EffectFire : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Fire;
  static constexpr const char* kName = "Fire";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    160, // speed
    15,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
