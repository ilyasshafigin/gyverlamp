#pragma once

#include "../effect.h"

class EffectTwinkles : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Twinkles;
  static constexpr const char* kName = "Twinkles";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    200, // speed
    100, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
