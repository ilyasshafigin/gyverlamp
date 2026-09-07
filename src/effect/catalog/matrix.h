#pragma once

#include "../effect.h"

class EffectMatrix : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Matrix;
  static constexpr const char* kName = "Matrix";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    16,  // speed
    80,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
