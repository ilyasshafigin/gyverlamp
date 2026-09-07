#pragma once

#include "../effect.h"

class EffectColor : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Color;
  static constexpr const char* kName = "Color";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    30,  // speed
    40,  // scale
  };

  void render(EffectContext& ctx) override;
};
