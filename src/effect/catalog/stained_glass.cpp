#include "stained_glass.h"

#include "../shared.h"

namespace {

  constexpr uint8_t kPointCount = 6;
  constexpr uint8_t kCenterX[kPointCount] = {2, 7, 12, 4, 10, 14};
  constexpr uint8_t kCenterY[kPointCount] = {3, 2, 4, 10, 12, 9};
  constexpr uint8_t kPhaseX[kPointCount] = {0, 43, 91, 137, 181, 223};
  constexpr uint8_t kPhaseY[kPointCount] = {69, 151, 233, 37, 119, 201};
  constexpr uint8_t kColor[kPointCount] = {8, 49, 92, 137, 181, 224};

  float wrappedDistance(float x, float pointX) {
    float distance = fabsf(x - pointX);
    return min(distance, static_cast<float>(WIDTH) - distance);
  }

  float wrapX(float x) {
    while (x < 0.0F)
      x += WIDTH;
    while (x >= WIDTH)
      x -= WIDTH;
    return x;
  }

  float waveOffset(uint8_t phase, float amplitude) {
    return (static_cast<int16_t>(sin8(phase)) - 128) * amplitude / 128.0F;
  }

  uint8_t softenVein(float gap) {
    constexpr float kVeinWidth = 7.0F;
    const float blend = constrain(gap / kVeinWidth, 0.0F, 1.0F);
    const float eased = blend * blend * (3.0F - 2.0F * blend);
    return static_cast<uint8_t>(112.0F + eased * 126.0F);
  }

} // namespace

void EffectStainedGlass::setup(EffectContext&) {
  ff_x = 0;
}

void EffectStainedGlass::render(EffectContext& ctx) {
  const uint16_t phaseAdvance = static_cast<uint16_t>(ctx.deltaMs) * (64U + ctx.speed) / 64U;
  ff_x += phaseAdvance;

  const uint8_t phase = ff_x >> 8U;
  const float scale = fmap(ctx.scale, 0.0F, 255.0F, 0.82F, 1.28F);
  float pointX[kPointCount];
  float pointY[kPointCount];

  for (uint8_t i = 0; i < kPointCount; i++) {
    pointX[i] =
      wrapX(kCenterX[i] + waveOffset(phase + kPhaseX[i], 1.45F * scale) + waveOffset(phase * 2U + kPhaseY[i], 0.45F));
    pointY[i] =
      kCenterY[i] + waveOffset(phase + kPhaseY[i], 1.20F * scale) + waveOffset(phase * 2U + kPhaseX[i], 0.35F);
  }

  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      float nearest = 10000.0F;
      float nextNearest = 10000.0F;
      uint8_t cell = 0;

      for (uint8_t i = 0; i < kPointCount; i++) {
        const float dx = wrappedDistance(x, pointX[i]);
        const float dy = static_cast<float>(y) - pointY[i];
        const float distance = dx * dx + dy * dy;

        if (distance < nearest) {
          nextNearest = nearest;
          nearest = distance;
          cell = i;
        } else if (distance < nextNearest) {
          nextNearest = distance;
        }
      }

      const uint8_t vein = softenVein(nextNearest - nearest);
      const uint8_t angleX = static_cast<uint16_t>(x) * 256U / WIDTH;
      const uint8_t glow = 210U + scale8(sin8(phase + cell * 37U + angleX + y * 3U), 34U);
      CRGB pixel = CHSV(kColor[cell] + phase / 12U, 176U, 255U);
      if (ctx.palette) {
        nblend(pixel, ColorFromPalette(*ctx.palette, kColor[cell], 255U, LINEARBLEND), 96U);
      }
      pixel.nscale8(scale8(vein, glow));
      ctx.led.drawPixel(x, y, pixel);
    }
  }
}
