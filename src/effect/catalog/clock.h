#pragma once

#include <Arduino.h>

#include "../effect.h"

class EffectClock : public Effect {
public:
  static constexpr Effects::Id kId = Effects::Id::Clock;
  static constexpr const char* kName = "Clock";
  static inline constexpr EffectSettingsSpec kSettings = {
    255, // brightness
    170, // speed (scroll speed)
    40,  // scale (hue), 1 - white
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;

private:
  static constexpr uint8_t kTextY = 4;

  String text_;
  int16_t offset_ = 0;
  uint8_t lastMinute_ = 255;
  uint8_t lastSecond_ = 255;
  bool separatorVisible_ = true;
  uint32_t scrollTimer_ = 0;

  void rebuildText(EffectContext& ctx);
};
