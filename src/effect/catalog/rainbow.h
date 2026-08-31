#pragma once

#include "../effect.h"

class EffectRainbow : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Rainbow;
  static constexpr const char* kName = "Rainbow";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    200, // speed
    120, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
