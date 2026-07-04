#include "palette_utils.h"

// Не используем здесь FastLED fill_gradient_RGB().
// На ESP8266 + FastLED 3.10.3 после пересборки этой палитры он может ломать
// последующие вызовы 3D inoise8(). Похоже на UB в signed fixed-point math
// внутри FastLED: отрицательная дельта цвета сдвигается влево.
// Палитра всего на 16 слотов, поэтому безопасная ручная интерполяция через
// blend() достаточно быстрая и предсказуемая.
// static void fillGradientRgbSafe(
//   CRGBPalette16& out,
//   uint8_t start,
//   const CRGB& startColor,
//   uint8_t end,
//   const CRGB& endColor
// ) {
//   if (end < start) {
//     uint8_t t = start;
//     start = end;
//     end = t;
//   }
//   const uint8_t distance = end - start;
//   if (distance == 0) {
//     out.entries[start] = startColor;
//     return;
//   }
//   for (uint8_t i = start; i <= end; i++) {
//     const uint8_t amount = static_cast<uint16_t>(i - start) * 255U / distance;
//     out.entries[i] = blend(startColor, endColor, amount);
//   }
// }

void buildHsvGradientPalette(
  CRGBPalette16& out, HsvGradientStopTable palette, uint8_t hue, uint8_t rows, bool isInvert
) {
  int8_t lastSlotUsed = -1;
  uint8_t istart8, iend8;
  CRGB rgbstart, rgbend;

  // начинаем с нуля
  if (isInvert) {
    hsv2rgb_spectrum(
      CHSV(256 + hue - pgm_read_byte(&palette[0][1]), pgm_read_byte(&palette[0][2]), pgm_read_byte(&palette[0][3])),
      rgbstart
    );
  } else {
    hsv2rgb_spectrum(
      CHSV(hue + pgm_read_byte(&palette[0][1]), pgm_read_byte(&palette[0][2]), pgm_read_byte(&palette[0][3])), rgbstart
    );
  }

  // начальный индекс палитры
  int indexstart = 0;

  for (uint8_t i = 1U; i < rows; i++) {
    int indexend = pgm_read_byte(&palette[i][0]);

    if (isInvert) {
      hsv2rgb_spectrum(
        CHSV(256 + hue - pgm_read_byte(&palette[i][1]), pgm_read_byte(&palette[i][2]), pgm_read_byte(&palette[i][3])),
        rgbend
      );
    } else {
      hsv2rgb_spectrum(
        CHSV(hue + pgm_read_byte(&palette[i][1]), pgm_read_byte(&palette[i][2]), pgm_read_byte(&palette[i][3])), rgbend
      );
    }

    istart8 = indexstart / 16;
    iend8 = indexend / 16;

    if ((istart8 <= lastSlotUsed) && (lastSlotUsed < 15)) {
      istart8 = lastSlotUsed + 1;
      if (iend8 < istart8) iend8 = istart8;
    }
    lastSlotUsed = iend8;
    //fillGradientRgbSafe(out, istart8, rgbstart, iend8, rgbend);
    fill_gradient_RGB(out.entries, istart8, rgbstart, iend8, rgbend);
    indexstart = indexend;
    rgbstart = rgbend;
  }
}
