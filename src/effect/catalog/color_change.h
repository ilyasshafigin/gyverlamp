#pragma once

#include "../effect.h"

class EffectColorChange : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::ColorChange;
  static constexpr const char* kName = "Color Change";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    30,  // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
