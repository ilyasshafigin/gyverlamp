#pragma once

#include "../effect.h"

class EffectOctopus : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Octopus;
  static constexpr const char* kName = "Octopus";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    200, // speed
    30,  // scale
  };
  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
