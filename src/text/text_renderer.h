#pragma once

#include <FastLED.h>
#include <WString.h>

class Led;
class NotificationOverlay;

class TextRenderer {
public:
  static constexpr uint8_t LET_WIDTH = 5;
  static constexpr uint8_t LET_HEIGHT = 8;
  static constexpr uint8_t SPACE = 1;

  // Отрисовать один символ левым верхним углом в (x, y).
  static void drawChar(Led& led, int16_t x, int16_t y, uint8_t letter, const CRGB& color, bool wrapX = false);
  static void
  drawChar(NotificationOverlay& overlay, int16_t x, int16_t y, uint8_t letter, const CRGB& color, bool wrapX = false);

  // Ширина строки в пикселях (символы + пробелы).
  static int16_t stringWidth(const String& text);

  // Отрисовать строку. x — левая грань первого символа.
  static void drawString(Led& led, int16_t x, int16_t y, const String& text, const CRGB& color, bool wrapX = false);
  static void drawString(NotificationOverlay& overlay, int16_t x, int16_t y, const String& text, const CRGB& color);

  // Прочитать следующий байт шрифта из строки (UTF-8 / CP1251 fallback).
  static uint8_t readFontByte(const String& text, uint16_t& index);

private:
  static uint8_t getFont(uint8_t letter, uint8_t row);
  static uint8_t normalizeCp1251Byte(uint8_t value);

  template <typename Target>
  static void drawCharTo(Target& target, int16_t x, int16_t y, uint8_t letter, const CRGB& color, bool wrapX);
};
