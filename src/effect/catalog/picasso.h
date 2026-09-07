#pragma once

#include "../effect.h"

class EffectPicasso : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Picasso;
  static constexpr const char* kName = "Picasso";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    120, // speed
    100, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
