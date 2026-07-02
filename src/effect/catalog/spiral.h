#pragma once

#include "../effect.h"

class EffectSpiral : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Spiral;
  static constexpr const char* NAME = "Spiral";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    30,   // speed
    100,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
