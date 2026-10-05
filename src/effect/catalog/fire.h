#pragma once

#include "../effect.h"

class EffectFire : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Fire;
  static constexpr const char* kName = "Fire";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    80, // speed
    128, // scale: flame height and texture
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
