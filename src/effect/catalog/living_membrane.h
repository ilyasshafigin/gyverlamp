#pragma once

#include "../effect.h"

class EffectLivingMembrane : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::LivingMembrane;
  static constexpr const char* kName = "Living Membrane";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    128, // speed
    160, // scale
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;

private:
  uint32_t randomState_ = 1;
  uint16_t accumulated_ = 0;
  uint32_t paletteClockMs_ = 0;
  uint8_t displayOffsetX_ = 0;
  float brightnessGain_ = 1.0F;

  uint32_t randomBits();
  uint8_t quantize(int32_t value);
  void advance(uint16_t diffusionU, uint16_t diffusionV);
};
