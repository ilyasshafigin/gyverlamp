#pragma once

#include "../effect.h"

class EffectWarmLight : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::WarmLight;
  static constexpr const char* kName = "Warm Light";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    220, // speed
    40,  // scale
  };

  void render(EffectContext& ctx) override;
};
