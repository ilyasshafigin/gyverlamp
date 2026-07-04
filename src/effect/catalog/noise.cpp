#include "noise.h"
#include "../shared.h"

// Effect: Noise effects
// Based on:
//   - Whilser/GyverLamp

uint8_t colorLoop = 1;

void EffectNoise::setup(EffectContext& ctx) {
  colorLoop = 1;
  hue = 0;
  ff_x = 0;
  ff_y = 0;
  ff_z = 0;
  memset(noise3d[0], 0, sizeof(noise3d[0]));
}

void EffectNoise::render(EffectContext& ctx) {
  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : *Palettes::getPaletteByScale(ctx.scale);

  uint8_t dataSmoothing = 0;
  if (ctx.speed < 50) {
    dataSmoothing = 200 - (ctx.speed * 4);
  }
  for (uint8_t i = 0; i < WIDTH; i++) {
    int ioffset = ctx.scale * i;
    for (uint8_t j = 0; j < HEIGHT; j++) {
      int joffset = ctx.scale * j;

      uint8_t data = inoise8(ff_x + ioffset, ff_y + joffset, ff_z);

      data = qsub8(data, 16);
      data = qadd8(data, scale8(data, 39));

      if (dataSmoothing) {
        uint8_t olddata = noise3d[0][i][j];
        uint8_t newdata = scale8(olddata, dataSmoothing) + scale8(data, 256 - dataSmoothing);
        data = newdata;
      }

      noise3d[0][i][j] = data;
    }
  }
  ff_z += ctx.speed / 4;

  // apply slow drift to X and Y, just for visual variation.
  ff_x += ctx.speed / 16;
  ff_y -= ctx.speed / 32;

  for (uint8_t i = 0; i < WIDTH; i++) {
    for (uint8_t j = 0; j < HEIGHT; j++) {
      uint8_t index = noise3d[0][j][i];
      uint8_t bri = noise3d[0][i][j];
      // if this palette is a 'loop', add a slowly-changing base value
      if (colorLoop) {
        index += hue;
      }
      // brighten up, as the color palette itself often contains the
      // light/dark dynamic range desired
      if (bri > 127) {
        bri = 255;
      } else {
        bri = dim8_raw(bri * 2);
      }
      CRGB color = ColorFromPalette(palette, index, bri);
      ctx.led.drawPixel(i, j, color);
    }
  }
  hue += 1;
}
