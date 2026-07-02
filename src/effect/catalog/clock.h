#pragma once

#include "../effect.h"

class EffectClock : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Clock;
  static constexpr const char* NAME = "Clock";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,   // brightness
    170,   // speed (scroll speed)
    40,    // scale (hue), 1 - white
  };

  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;

private:
  static constexpr uint8_t TEXT_Y = 4;

  String _text;
  int16_t _offset = 0;
  uint8_t _lastMinute = 255;
  uint8_t _lastSecond = 255;
  bool _separatorVisible = true;
  uint32_t _scrollTimer = 0;

  void rebuildText(EffectContext& ctx);
};
