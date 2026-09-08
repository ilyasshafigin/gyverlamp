#pragma once

#include "../effect.h"

class EffectVelvetFolds : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::VelvetFolds;
  static constexpr const char* kName = "Velvet Folds";
  static inline constexpr EffectSettingsSpec kSettings = {
    220, // brightness
    52,  // speed: fold drift
    112, // scale: fold density
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
