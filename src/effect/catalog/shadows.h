#pragma once

#include "../effect.h"

class EffectShadows : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Shadows;
  static constexpr const char* kName = "Shadows";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    200, // speed
    200, // scale
  };

  void render(EffectContext& ctx) override;
};
