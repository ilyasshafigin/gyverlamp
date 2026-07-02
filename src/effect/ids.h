#pragma once

#include <Arduino.h>

namespace Effects {

  enum class Id : uint8_t {
    Color = 0,
    ColorChange,
    Gradient,
    Fire,
    Rainbow,
    Noise,
    Matrix,
    Paintball,
    Spiral,
    WarmLight,
    Twinkles,
    Shadows,
    Butterflys,
    Thunderstorm,
    Nexus,
    Snowstorm,
    Clock,
    LiquidLamp,
    NorthernLights,
    Picasso,
    Equalizer,
    Octopus,
    COUNT,
    INVALID = 255,
  };

  constexpr uint8_t toIndex(Id id) { return static_cast<uint8_t>(id); }
  constexpr Id toId(uint8_t raw) { return static_cast<Id>(raw); }

  constexpr uint8_t COUNT = toIndex(Id::COUNT);
  constexpr Id DEFAULT_ID = Id::Color;

}
