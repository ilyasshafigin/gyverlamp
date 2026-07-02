#include "matrix.h"
#include "../shared.h"

// Effect: Matrix
// Based on:
//   - Whilser/GyverLamp
//     https://github.com/Whilser/GyverLamp/blob/e7545c18f75911df6fc017d3e18c52cdd994cfc9/firmware/GyverLamp_v1.6_MQTT/effects.ino#L277

void EffectMatrix::setup(EffectContext& ctx) {
  ctx.led.clearLedsBuff();
  resetEveryMs(step, speedToIntervalMs(ctx.speed, 80U, 20U), ctx.nowMs);
}

static void stepMatrix(Led& led, uint8_t scale, const CRGBPalette16* palette) {
  for (uint8_t x = 0; x < WIDTH; x++) {
    // заполняем случайно верхнюю строку
    CRGB& pixel = led.getPixelBuff(x, HEIGHT - 1);
    if (palette) {
      if (pixel.getAverageLight() < 4) {
        pixel = (random(0, scale) == 0) ? ColorFromPalette(*palette, random(0, 240), 255U) : CRGB::Black;
      } else {
        pixel.fadeToBlackBy(32);
      }
    } else {
      if (pixel == 0) {
        pixel = 0x00FF00 * (random(0, scale) == 0);
      } else if (pixel < 0x002000) {
        pixel = 0;
      } else {
        pixel = pixel - 0x002000;
      }
    }
  }

  // сдвигаем всё вниз
  for (uint8_t x = 0; x < WIDTH; x++) {
    for (uint8_t y = 0; y < HEIGHT - 1; y++) {
      led.getPixelBuff(x, y) = led.getPixelBuff(x, y + 1);
    }
  }
}

void EffectMatrix::render(EffectContext& ctx) {
  if (everyMs(step, speedToIntervalMs(ctx.speed, 80U, 20U), ctx.nowMs)) {
    stepMatrix(ctx.led, ctx.scale, ctx.palette);
  }

  ctx.led.copyLedsBuffToLeds();
}
