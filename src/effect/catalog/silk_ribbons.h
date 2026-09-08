#pragma once

#include "../effect.h"

class EffectSilkRibbons : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::SilkRibbons;
  static constexpr const char* kName = "Silk Ribbons";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    80,  // speed
    128, // scale: ribbon density and width
  };

  void render(EffectContext& ctx) override;
};
