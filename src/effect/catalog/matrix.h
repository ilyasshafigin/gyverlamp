#pragma once

#include "../effect.h"

class EffectMatrix : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Matrix;
  static constexpr const char* NAME = "Matrix";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    16,   // speed
    80,   // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
