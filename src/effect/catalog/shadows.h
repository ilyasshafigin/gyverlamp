#pragma once

#include "../effect.h"

class EffectShadows : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Shadows;
  static constexpr const char* NAME = "Shadows";
  static constexpr EffectSettingsSpec SETTINGS = {
    255, // brightness
    200, // speed
    200, // scale
  };

  void render(EffectContext& ctx) override;
};
