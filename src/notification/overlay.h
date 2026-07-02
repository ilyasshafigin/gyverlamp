#pragma once

#include <FastLED.h>

class Led;

class NotificationOverlay {
public:
  NotificationOverlay(
    Led& led, uint8_t opacity, uint8_t holeDim) :
    _led(led),
    _opacity(opacity),
    _holeDim(holeDim) {
  }

  void drawPixel(uint8_t x, uint8_t y, CRGB color);
  void drawPixelSafe(int x, int y, CRGB color);
  void addPixel(uint8_t x, uint8_t y, CRGB color);
  void fill(CRGB color);

  void clearTop();
  void drawTopSolid(const CRGB& color);
  void drawTopPulse(const CRGB& color, uint32_t startedMs, uint16_t periodMs, uint8_t minBrightness, uint8_t maxBrightness);
  void drawTopSpinner(const CRGB& color, uint32_t startedMs);
  void drawTopProgress(const CRGB& color, uint32_t startedMs, uint8_t percent);
  void drawTopDoublePulse(const CRGB& color, uint32_t startedMs, uint16_t periodMs);
  void drawFullPulse(const CRGB& color, uint32_t startedMs, uint16_t periodMs, uint8_t minBrightness, uint8_t maxBrightness);
  void drawVerticalWave(const CRGB& color, uint32_t startedMs, uint16_t periodMs, uint8_t minBrightness, uint8_t maxBrightness);

private:
  Led& _led;
  uint8_t _opacity = 255;
  uint8_t _holeDim = 255;
};
