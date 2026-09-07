#pragma once

#include "../effect.h"

class EffectThunderstorm : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Thunderstorm;
  static constexpr const char* kName = "Thunderstorm";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    140, // speed
    100, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
