#include "twinkles.h"
#include "../shared.h"

// Effect: Twinkles - ЭФФЕКТ МЕРЦАНИЕ
// Authors: SottNick
// Based on:
//   - Whilser/GyverLamp
//     https://github.com/Whilser/GyverLamp/blob/e7545c18f75911df6fc017d3e18c52cdd994cfc9/firmware/GyverLamp_v1.6_MQTT/twinkles.ino#L7
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L4665

#define TWINKLES_SPEEDS 4     // всего 4 варианта скоростей мерцания
#define TWINKLES_MULTIPLIER 6 // слишком медленно, если на самой медленной просто по единичке добавлять

void EffectTwinkles::setup(EffectContext& ctx) {
  hue = 0U;
  for (uint32_t idx = 0; idx < NUM_LEDS; idx++) {
    CRGB& led = ctx.led.getLedBuff(idx);
    if (random8(ctx.scale % 11U) == 0) {
      led.r = random8();                           // оттенок пикселя
      led.g = random8(1, TWINKLES_SPEEDS * 2 + 1); // скорость и направление (нарастает 1-4 или угасает 5-8)
      led.b = random8();                           // яркость
    } else
      ctx.led.setLedBuff(idx, CRGB::Black); // всё выкл
  }
  resetEveryMs(step, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs);
}

static void stepTwinkles(Led& led, uint8_t scale) {
  for (uint32_t idx = 0; idx < NUM_LEDS; idx++) {
    CRGB& pixel = led.getLedBuff(idx);
    if (pixel.b == 0) {
      if (random8(scale % 11U) == 0 && hue > 0) {  // если пиксель ещё не горит, зажигаем каждый ХЗй
        pixel.r = random8();                       // оттенок пикселя
        pixel.g = random8(1, TWINKLES_SPEEDS + 1); // скорость и направление (нарастает 1-4, но не угасает 5-8)
        pixel.b = pixel.g;                         // яркость
        hue--;                                     // уменьшаем количество погасших пикселей
      }
    } else if (pixel.g <= TWINKLES_SPEEDS) {                // если нарастание яркости
      if (pixel.b > 255U - pixel.g - TWINKLES_MULTIPLIER) { // если досигнут максимум
        pixel.b = 255U;
        pixel.g = pixel.g + TWINKLES_SPEEDS;
      } else {
        pixel.b = pixel.b + pixel.g + TWINKLES_MULTIPLIER;
      }
    } else {                                                            // если угасание яркости
      if (pixel.b <= pixel.g - TWINKLES_SPEEDS + TWINKLES_MULTIPLIER) { // если досигнут минимум
        pixel.b = 0;                                                    // всё выкл
        hue++;                                                          // считаем количество погасших пикселей
      } else {
        pixel.b = pixel.b - pixel.g + TWINKLES_SPEEDS - TWINKLES_MULTIPLIER;
      }
    }
  }
}

static void drawTwinkles(Led& led, const CRGBPalette16& palette) {
  for (uint32_t idx = 0; idx < NUM_LEDS; idx++) {
    CRGB& pixel = led.getLedBuff(idx);
    if (pixel.b == 0) {
      led.setLed(idx, CRGB::Black);
    } else {
      led.setLed(idx, ColorFromPalette(palette, pixel.r, pixel.b));
    }
  }
}

void EffectTwinkles::render(EffectContext& ctx) {
  if (everyMs(step, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs)) {
    stepTwinkles(ctx.led, ctx.scale);
  }

  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : *Palettes::getPaletteByScale(ctx.scale);

  drawTwinkles(ctx.led, palette);
}
