#pragma once

#include "../effect.h"

class EffectWarmLight : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::WarmLight;
  static constexpr const char* NAME = "Warm Light";
  static constexpr EffectSettingsSpec SETTINGS = {
    150,  // brightness
    220,  // speed
    40,   // scale
  };

  void render(EffectContext& ctx) override;
};
