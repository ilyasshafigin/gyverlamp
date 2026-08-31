#pragma once

#include "../effect.h"

class EffectPaintball : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Paintball;
  static constexpr const char* kName = "Paintball";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    100, // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
