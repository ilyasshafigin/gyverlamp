#pragma once

#include "../effect.h"

class EffectNexus : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Nexus;
  static constexpr const char* kName = "Nexus";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    70,  // speed
    100, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
