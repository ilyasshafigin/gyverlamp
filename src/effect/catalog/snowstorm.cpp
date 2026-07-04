#include "snowstorm.h"
#include "../shared.h"

// Effect: Snowstorm - метель
// Authors: PalPalych
// Based on:
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L1761

#define SNOW_DENSE (60U)      // плотность снега
#define SNOW_TAIL_STEP (100U) // длина хвоста
#define SNOW_SATURATION (0U)  // насыщенность (от 0 до 255)

void EffectSnowstorm::setup(EffectContext& ctx) {
  ctx.led.clearLedsBuff();
  resetEveryMs(step, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs);
}

static void stepSnowstorm(Led& led, uint8_t scale, const CRGBPalette16* palette) {
  for (uint8_t x = 0U; x < WIDTH; x++) {
    const bool leftEmpty = x == 0U || led.getPixelBuff(x - 1U, HEIGHT - 1U) == CRGB::Black;
    const bool rightEmpty = x >= WIDTH - 1U || led.getPixelBuff(x + 1U, HEIGHT - 1U) == CRGB::Black;
    if (
      led.getPixelBuff(x, HEIGHT - 1U) == CRGB::Black && (random(0, map(scale, 0U, 255U, 10U, 120U)) == 0U) &&
      leftEmpty && rightEmpty
    ) {
      if (palette) {
        led.getPixelBuff(x, HEIGHT - 1U) = ColorFromPalette(*palette, random(0, 240), 255U);
      } else {
        led.getPixelBuff(x, HEIGHT - 1U) = CHSV(random(0, 200), SNOW_SATURATION, 255U);
      }
    }
  }

  // сдвигаем по диагонали
  for (uint8_t y = 0U; y < HEIGHT - 1U; y++) {
    for (uint8_t x = WIDTH - 1U; x > 0U; x--) {
      led.getPixelBuff(x, y) = led.getPixelBuff(x - 1U, y + 1U);
    }
    led.getPixelBuff(0, y) = led.getPixelBuff(WIDTH - 1U, y + 1U);
  }

  // уменьшаем яркость верхней линии, формируем "хвосты"
  for (uint8_t x = 0U; x < WIDTH; x++) {
    led.fadeBuffPixelToBlack(x, HEIGHT - 1U, SNOW_TAIL_STEP);
  }
}

void EffectSnowstorm::render(EffectContext& ctx) {
  if (everyMs(step, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs)) {
    stepSnowstorm(ctx.led, ctx.scale, ctx.palette);
  }

  ctx.led.copyLedsBuffToLeds();
}
