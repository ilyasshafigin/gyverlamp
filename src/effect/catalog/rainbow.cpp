#include "rainbow.h"
#include "../shared.h"

// Effect: Rainbow
// Authors: MishanyaTS
// Based on:
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L515

void EffectRainbow::setup(EffectContext& ctx) {
  hue = 0;
  resetEveryMs(step, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs);
  if (ctx.palette == nullptr) {
    rgbPalette = CRGBPalette16(RainbowColors_p);
  }
}

void EffectRainbow::render(EffectContext& ctx) {
  if (everyMs(step, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs)) {
    hue += 4U;
  }

  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : rgbPalette;

  if (ctx.scale < 85U) {
    for (uint8_t y = 0U; y < HEIGHT; y++) {
      CRGB thisColor = ColorFromPalette(palette, static_cast<uint8_t>(hue + y * (ctx.scale % 67U) * 2U));
      for (uint8_t x = 0U; x < WIDTH; x++) {
        ctx.led.drawPixel(x, y, thisColor);
      }
    }
  } else if (ctx.scale > 170U) {
    for (uint8_t x = 0U; x < WIDTH; x++) {
      CRGB thisColor = ColorFromPalette(palette, static_cast<uint8_t>(hue + x * (ctx.scale % 67U) * 2U));
      for (uint8_t y = 0U; y < HEIGHT; y++) {
        ctx.led.drawPixel(x, y, thisColor);
      }
    }
  } else {
    const float ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
    const float invDim = 255.0 / static_cast<float>(MAX_DIMENSION);

    for (uint8_t x = 0U; x < WIDTH; x++) {
      for (uint8_t y = 0U; y < HEIGHT; y++) {
        float twirlFactor = 9.0F * ((ctx.scale - 85) / 255.0F); // на сколько оборотов будет закручена матрица, [0..3]
        const float palettePosition = hue + (ratio * x + y * twirlFactor) * invDim;
        const uint8_t paletteIndex = static_cast<uint8_t>(static_cast<uint16_t>(palettePosition));
        CRGB thisColor = ColorFromPalette(palette, paletteIndex);
        ctx.led.drawPixel(x, y, thisColor);
      }
    }
  }
}
