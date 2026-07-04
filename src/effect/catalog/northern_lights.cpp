#include "northern_lights.h"
#include "../shared.h"

// Effect: Northern Lights
// Authors: kostyamat
// Based on:
//   - Reddit r/FastLED / ldirko
//     https://www.reddit.com/r/FastLED/comments/jyly1e/challenge_fastled_sketch_that_fits_entirely_in_a/
//   - Yaroslaw Turbin aka ldirko
//     https://www.reddit.com/user/ldirko/
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L7928
// Notes:
//   - вместо набора палитр в оригинальном эффекте сделан генератор палитр
//   - генератор палитр для Северного сияния (c) SottNick

#define AURORA_COLOR_RANGE \
  10 // (+/-10 единиц оттенка) диапазон, в котором плавает цвет сияния относительно выбранного оттенка
#define AURORA_COLOR_PERIOD \
  2 // (2 раза в минуту) частота, с которой происходит колебание выбранного оттенка в разрешённом диапазоне

// HSV-точки градиента: { индекс, смещениеОттенка, насыщенность, яркость }.
static const uint8_t MBAuroraColors_arr[5][4] PROGMEM = {
  {0, 0, 255, 0}, // black
  {80, 0, 255, 255},
  {130, 25, 220, 255},
  {180, 25, 185, 255},
  {255, 25, 155, 255} //245
};
/*
  {
  {0  , 0 , 255,   0},// black
  {60 , 1 , 255, 222},
  {80 , 1 , 210, 255},
  {180, 11, 175, 255},
  {255, 11 ,135, 255} //245
  };
*/

uint32_t polarTimer;
//float adjastHeight; // используем ff_x
//uint16_t adjScale; // используем ff_y

void EffectNorthernLights::setup(EffectContext& ctx) {
  //emitterX = fmap((float)HEIGHT, 8, 32, 28, 12); такое работало с горем пополам только для матриц до 32 пикселей в высоту
  //emitterX = 512. / HEIGHT - 0.0001; // это максимально возможное значение
  emitterX = 400. / HEIGHT; // а это - максимум без яркой засветки крайних рядов матрицы (сверху и снизу)

  hue = ctx.scale;
  if (ctx.scale == 255U) {
    deltaHue = 0;
  } else {
    deltaHue = AURORA_COLOR_RANGE - beatsin8(AURORA_COLOR_PERIOD, 0U, AURORA_COLOR_RANGE + AURORA_COLOR_RANGE);
  }

  buildHsvGradientPalette(rgbPalette, MBAuroraColors_arr, hue + deltaHue, 5U, hue & 0x01);

  ff_y = map(WIDTH, 8, 64, 310, 63);
  //ff_z = map(ctx.scale, 1, 255, 30, ff_y);
  ff_z = ff_y;
  resetEveryMs(step, 120U, ctx.nowMs);
}

void EffectNorthernLights::render(EffectContext& ctx) {
  speedfactor = map(ctx.speed, 1, 255, 128, 16);

  bool rebuildPalette = false;

  if (ctx.scale == 255U) {
    hue = ctx.scale;
    if (everyMs(step, 120U, ctx.nowMs)) {
      deltaHue++;
      rebuildPalette = true;
    }
  } else {
    if (ctx.scale != hue) {
      hue = ctx.scale;
      deltaHue = AURORA_COLOR_RANGE - beatsin8(AURORA_COLOR_PERIOD, 0U, AURORA_COLOR_RANGE + AURORA_COLOR_RANGE);
      rebuildPalette = true;
    }
  }

  if (rebuildPalette) {
    buildHsvGradientPalette(rgbPalette, MBAuroraColors_arr, hue + deltaHue, 5U, hue & 0x01);
  }

  for (uint8_t y = 0; y < HEIGHT; y++) {
    const uint8_t t = fabs(static_cast<float>(HEIGHT) / 2.0 - static_cast<float>(y)) * emitterX;
    for (uint8_t x = 0; x < WIDTH; x++) {
      polarTimer++;
      ctx.led.drawPixel(
        x,
        y,
        ColorFromPalette(
          rgbPalette, qsub8(inoise8(polarTimer % 2 + x * ff_z, y * 16 + polarTimer % 16, polarTimer / speedfactor), t)
        )
      );
    }
  }
}
