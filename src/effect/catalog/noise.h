#pragma once

#include "../effect.h"

class EffectNoise : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Noise;
  static constexpr const char* kName = "Noise";
  static constexpr float kAudioGain = 1.5f;
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    15,  // speed
    40,  // scale
    Effects::EFFECT_SPEC_USES_AUDIO,
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
