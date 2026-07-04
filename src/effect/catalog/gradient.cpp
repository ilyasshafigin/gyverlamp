#include "gradient.h"
#include "../shared.h"

// Effect: Gradient

void EffectGradient::setup(EffectContext& ctx) {
  if (ctx.palette == nullptr) {
    rgbPalette = CRGBPalette16(RainbowColors_p);
  }
}

void EffectGradient::render(EffectContext& ctx) {
  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : rgbPalette;
  const uint16_t scaleSpan = static_cast<uint16_t>(ctx.scale) * 19U / 10U + 25U;
  const int16_t speedOffset = static_cast<int16_t>(ctx.speed) - 128;
  const int32_t timeOffset = static_cast<int32_t>(ctx.nowMs >> 3) * speedOffset / 128;

  for (uint8_t y = 0; y < HEIGHT; y++) {
    const uint8_t paletteIndex = static_cast<uint8_t>(static_cast<int32_t>(y) * scaleSpan / HEIGHT + timeOffset);
    CRGB thisColor = ColorFromPalette(
      palette, // (x*1.9 + 25) / 255 - быстрый мап 0..255 в 0.1..2
      paletteIndex,
      255U,
      LINEARBLEND
    );

    for (uint8_t x = 0; x < WIDTH; x++) {
      ctx.led.drawPixel(x, y, thisColor);
    }
  }
}
