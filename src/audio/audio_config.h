#pragma once

#include <Arduino.h>

enum class AudioMode : uint8_t {
  Off,
  Brightness,
  Speed,
  Scale,
  Effect,
};

enum class AudioBand : uint8_t {
  Level,
  Bass,
  Treble,
};

struct AudioConfig {
  AudioMode mode = AudioMode::Off;
  AudioBand band = AudioBand::Level;
  uint8_t amount = 128;
};
