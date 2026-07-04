#pragma once

#include <FastLED.h>
#include "../config.h"
#include "../audio/audio_config.h"
#include "../audio/audio_frame.h"
#include "../hardware/led.h"
#include "settings.h"
#include "ids.h"
#include "palette_ids.h"

class Led;
class TimeService;

class EffectContext {
public:
  static constexpr uint8_t width = WIDTH;
  static constexpr uint8_t height = HEIGHT;
  static constexpr uint16_t numLeds = NUM_LEDS;

  EffectContext(
    uint8_t brightness_,
    uint8_t speed_,
    uint8_t scale_,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint32_t nowMs,
    uint32_t deltaMs,
    const CRGBPalette16* palette_,
    const AudioFrame& audio_,
    const AudioConfig& audioConfig_,
    Led& led_,
    TimeService& time_
  )
    : brightness(brightness_),
      speed(speed_),
      scale(scale_),
      red(r),
      green(g),
      blue(b),
      nowMs(nowMs),
      deltaMs(deltaMs),
      palette(palette_),
      audio(audio_),
      audioConfig(audioConfig_),
      led(led_),
      time(time_) {}

  const uint8_t brightness;
  const uint8_t speed;
  const uint8_t scale;
  const uint8_t red;
  const uint8_t green;
  const uint8_t blue;
  // Текущее время в миллисекундах
  const uint32_t nowMs;
  // Пройденное время в прошлого кадра
  const uint32_t deltaMs;
  // Текущая политра, может быть nullptr
  const CRGBPalette16* palette;

  const AudioFrame& audio;
  const AudioConfig& audioConfig;
  Led& led;
  const TimeService& time;
};

class Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::INVALID;
  static constexpr const char* NAME = "";

  virtual ~Effect() = default;
  // Вызывается при установке эффекта
  virtual void setup(EffectContext& ctx) {};
  // Отрисовывает эффект
  virtual void render(EffectContext& ctx) = 0;
};
