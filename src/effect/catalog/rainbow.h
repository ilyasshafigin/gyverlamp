#pragma once

#include "../effect.h"

class EffectRainbow : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Rainbow;
  static constexpr const char* NAME = "Rainbow";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    200, // speed
    120, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
