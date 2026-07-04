#pragma once

#include "../effect.h"

class EffectColorChange : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::ColorChange;
  static constexpr const char* NAME = "Color Change";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    30,  // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
