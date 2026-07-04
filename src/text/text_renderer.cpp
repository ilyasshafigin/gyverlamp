#include "../hardware/led.h"
#include "../notification/overlay.h"
#include "text_renderer.h"
#include "font_5x8.h"

// **************** НАСТРОЙКИ ****************
#define TEXT_DIRECTION 1 // 1 - по горизонтали, 0 - по вертикали
#define MIRR_V 0         // отразить текст по вертикали (0 / 1)
#define MIRR_H 0         // отразить текст по горизонтали (0 / 1)

uint8_t TextRenderer::normalizeCp1251Byte(uint8_t value) {
  if (value >= 0xC0 && value <= 0xEF) return value - 0x30; // А-п -> UTF-8 second byte equivalent
  if (value >= 0xF0) return value - 0x70;                  // р-я -> UTF-8 second byte equivalent
  if (value == 0xA8) return 'E';                           // Ё fallback
  if (value == 0xB8) return 'e';                           // ё fallback
  return '?';
}

uint8_t TextRenderer::readFontByte(const String& text, uint16_t& index) {
  const uint8_t value = static_cast<uint8_t>(text[index++]);
  if (value < 0x80) return value;
  if ((value == 0xD0 || value == 0xD1) && text[index] != '\0') {
    const uint8_t next = static_cast<uint8_t>(text[index++]);
    if (value == 0xD0 && next == 0x81) return 'E';
    if (value == 0xD1 && next == 0x91) return 'e';
    if (value == 0xD0 && next >= 0x90 && next <= 0xBF) return next;
    if (value == 0xD1 && next >= 0x80 && next <= 0x8F) return next;
    return '?';
  }

  return normalizeCp1251Byte(value);
}

template <typename Target>
void TextRenderer::drawCharTo(Target& target, int16_t x, int16_t y, uint8_t letter, const CRGB& color, bool wrapX) {
  int8_t start_pos = 0;
  int8_t finish_pos = LET_WIDTH;

  if (x <= -LET_WIDTH || x >= WIDTH) return;

  if (!wrapX) {
    if (x < 0) start_pos = -x;
    if (x > WIDTH - LET_WIDTH) finish_pos = WIDTH - x;
  }

  for (uint8_t i = start_pos; i < finish_pos; i++) {
    int thisByte;
    if (MIRR_V) {
      thisByte = getFont(static_cast<uint8_t>(letter), LET_WIDTH - 1 - i);
    } else {
      thisByte = getFont(static_cast<uint8_t>(letter), i);
    }

    for (uint8_t j = 0; j < LET_HEIGHT; j++) {
      bool thisBit;
      if (MIRR_H) {
        thisBit = thisByte & (1 << j);
      } else {
        thisBit = thisByte & (1 << (LET_HEIGHT - 1 - j));
      }

      if (!thisBit) continue;

      if (TEXT_DIRECTION) {
        int16_t pixelX = x + i;
        if (wrapX) {
          pixelX %= WIDTH;
          if (pixelX < 0) pixelX += WIDTH;
        }
        target.drawPixelSafe(pixelX, y + j, color);
      } else {
        target.drawPixelSafe(i, x + y + j, color);
      }
    }
  }
}

void TextRenderer::drawChar(Led& led, int16_t x, int16_t y, uint8_t letter, const CRGB& color, bool wrapX) {
  drawCharTo(led, x, y, letter, color, wrapX);
}

void TextRenderer::drawChar(
  NotificationOverlay& overlay, int16_t x, int16_t y, uint8_t letter, const CRGB& color, bool wrapX
) {
  drawCharTo(overlay, x, y, letter, color, wrapX);
}

int16_t TextRenderer::stringWidth(const String& text) {
  uint16_t len = 0;
  uint16_t index = 0;
  while (text[index] != '\0') {
    readFontByte(text, index);
    len++;
  }
  if (len == 0) return 0;
  return len * LET_WIDTH + (len - 1) * SPACE;
}

void TextRenderer::drawString(Led& led, int16_t x, int16_t y, const String& text, const CRGB& color, bool wrapX) {
  uint16_t index = 0;
  int16_t cursor = x;
  while (text[index] != '\0') {
    const uint8_t letter = readFontByte(text, index);
    drawChar(led, cursor, y, letter, color, wrapX);
    cursor += LET_WIDTH + SPACE;
  }
}

void TextRenderer::drawString(
  NotificationOverlay& overlay, int16_t x, int16_t y, const String& text, const CRGB& color
) {
  uint16_t index = 0;
  int16_t cursor = x;
  while (text[index] != '\0') {
    const uint8_t letter = readFontByte(text, index);
    drawChar(overlay, cursor, y, letter, color, true);
    cursor += LET_WIDTH + SPACE;
  }
}

// интерпретатор кода символа в массиве fontHEX
uint8_t TextRenderer::getFont(uint8_t letter, uint8_t row) {
  uint8_t code = letter - '0' + 16; // перевод код символа из таблицы ASCII в номер согласно нумерации массива
  if (code <= 90) {
    return pgm_read_byte(&(font5x8[code][row]));
  } else if (code >= 112 && code <= 159) {
    return pgm_read_byte(&(font5x8[code - 17][row]));
  } else if (code >= 96 && code <= 111) {
    return pgm_read_byte(&(font5x8[code + 47][row]));
  } else {
    return 0;
  }
}
