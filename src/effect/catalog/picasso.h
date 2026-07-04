#pragma once

#include "../effect.h"

class EffectPicasso : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Picasso;
  static constexpr const char* NAME = "Picasso";
  static constexpr EffectSettingsSpec SETTINGS = {
    255,  // brightness
    120,  // speed
    100,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
