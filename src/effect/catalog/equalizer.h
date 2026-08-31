#pragma once

#include "../effect.h"

class EffectEqualizer : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Equalizer;
  static constexpr const char* kName = "Equalizer";
  static constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    128, // speed
    40,  // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;

private:
  uint8_t level_ = 0;
  uint8_t bass_ = 0;
  uint8_t treble_ = 0;
  uint8_t peak_[WIDTH] = {};
  uint8_t sparkX_ = 0;
};
