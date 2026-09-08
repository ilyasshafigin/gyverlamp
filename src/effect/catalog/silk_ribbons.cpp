#include "silk_ribbons.h"

#include "../palette_catalog.h"

namespace {

  uint8_t circularDistance(uint8_t first, uint8_t second) {
    const uint8_t clockwise = first - second;
    const uint8_t counterClockwise = second - first;
    return min(clockwise, counterClockwise);
  }

} // namespace

void EffectSilkRibbons::render(EffectContext& ctx) {
  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : *Palettes::paletteByScale(ctx.scale);
  const uint8_t ribbonCount = 2U + static_cast<uint16_t>(ctx.scale) * 3U / 256U;
  const uint8_t halfWidth = map(ctx.scale, 0U, 255U, 78U, 46U);
  const uint8_t spacing = 256U / ribbonCount;
  const uint8_t drift = static_cast<uint8_t>((ctx.nowMs >> 6) * (1U + ctx.speed / 85U));
  const uint8_t sway = static_cast<uint8_t>((ctx.nowMs >> 7) * (1U + ctx.speed / 96U));

  for (uint8_t y = 0; y < HEIGHT; y++) {
    const int16_t wave = (static_cast<int16_t>(sin8(y * 9U + sway)) - 128) * 68 / 128;
    const uint8_t rowHalfWidth = halfWidth - scale8(y * 17U, 12U);

    for (uint8_t x = 0; x < WIDTH; x++) {
      CRGB pixel(8U, 16U, 24U);
      pixel += ColorFromPalette(palette, y * 5U, 26U, LINEARBLEND_NOWRAP);
      const uint8_t diagonal = static_cast<uint8_t>(static_cast<int16_t>(x) * 16 + static_cast<int16_t>(y) * 16 + wave);

      for (uint8_t ribbon = 0; ribbon < ribbonCount; ribbon++) {
        const uint8_t center = drift + ribbon * spacing + sin8(x * 16U + y * 4U + sway + ribbon * 53U) / 5U;
        const uint8_t distance = circularDistance(diagonal, center);
        if (distance >= rowHalfWidth) continue;

        const uint8_t edge = 255U - static_cast<uint16_t>(distance) * 255U / rowHalfWidth;
        const uint8_t opacity = scale8(ease8InOutApprox(edge), 176U);
        const CRGB color = ColorFromPalette(palette, ribbon * 54U + y * 3U, 210U, LINEARBLEND_NOWRAP);
        nblend(pixel, color, opacity);
      }

      ctx.led.drawPixel(x, y, pixel);
    }
  }
}
