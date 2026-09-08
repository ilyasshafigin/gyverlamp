#pragma once

#include "../effect.h"

class EffectStainedGlass : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::StainedGlass;
  static constexpr const char* kName = "Stained Glass";
  static inline constexpr EffectSettingsSpec kSettings = {
    210, // brightness
    56,  // speed: slow cell drift
    128, // scale: cell size
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
