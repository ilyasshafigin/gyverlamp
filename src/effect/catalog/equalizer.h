#pragma once

#include "../effect.h"

class EffectEqualizer : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Equalizer;
  static constexpr const char* NAME = "Equalizer";
  static constexpr EffectSettingsSpec SETTINGS = {
    255,  // brightness
    128,  // speed
    40,   // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;

private:
  uint8_t _level = 0;
  uint8_t _bass = 0;
  uint8_t _treble = 0;
  uint8_t _peak[WIDTH] = {};
  uint8_t _sparkX = 0;
};
