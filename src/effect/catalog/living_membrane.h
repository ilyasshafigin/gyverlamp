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
    Effects::EFFECT_SPEC_USES_AUDIO,
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;

private:
  uint32_t randomState_ = 1;
  uint16_t accumulated_ = 0;
  uint32_t paletteClockMs_ = 0;
  uint8_t displayOffsetX_ = 0;
  float brightnessGain_ = 1.0F;
  uint32_t birthRandomState_ = 1;
  uint16_t birthStep_ = 0xffff;
  uint16_t birthWeakSteps_ = 0;
  uint16_t birthCooldownSteps_ = 0;
  uint8_t birthCenterX_ = 0xff;
  uint8_t birthCenterY_ = 0xff;

  void tickAudio(const EffectContext& ctx);
  uint32_t randomBits();
  uint8_t quantize(int32_t value);
  void advance(uint16_t diffusionU, uint16_t diffusionV);
  void resetBirthState();
  uint32_t birthRandom();
  void chooseBirthCenter();
  void applyBirthSource();
  void tickBirth();
};
