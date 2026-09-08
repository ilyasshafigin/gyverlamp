#include "velvet_folds.h"

#include "../shared.h"

namespace {

  constexpr uint32_t kSpeedSquareMax = 255U * 255U;
  constexpr uint16_t kScaleUnitsPerSecond = 96U;

  float phaseRate(uint8_t speed, uint8_t basePerSecond, uint8_t spanPerSecond) {
    const uint32_t speedSquare = static_cast<uint32_t>(speed) * speed;
    return static_cast<float>(basePerSecond * kSpeedSquareMax + spanPerSecond * speedSquare) /
           (kSpeedSquareMax * 1000U);
  }

  void advancePhase(float& phase, float rate, uint32_t elapsedMs) {
    const float next = phase + rate * elapsedMs;
    const uint32_t turns = static_cast<uint32_t>(next / 256.0F);
    phase = next - turns * 256.0F;
  }

} // namespace

void EffectVelvetFolds::setup(EffectContext& ctx) {
  emitterX = 0.0F;
  emitterY = 0.0F;
  speedfactor = ctx.scale;
}

void EffectVelvetFolds::render(EffectContext& ctx) {
  advancePhase(emitterX, phaseRate(ctx.speed, 6U, 72U), ctx.deltaMs);
  advancePhase(emitterY, phaseRate(ctx.speed, 1U, 11U), ctx.deltaMs);

  const float scaleStep = static_cast<float>(ctx.deltaMs) * kScaleUnitsPerSecond / 1000.0F;
  if (speedfactor < ctx.scale) {
    speedfactor = min<float>(speedfactor + scaleStep, ctx.scale);
  } else {
    speedfactor = max<float>(speedfactor - scaleStep, ctx.scale);
  }

  const uint8_t scale = static_cast<uint8_t>(speedfactor);
  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : *Palettes::paletteByScale(ctx.scale);
  const uint8_t verticalPhase = map(scale, 0U, 255U, 14U, 20U);
  const uint8_t densityMix = scale8(scale, 144U);
  // Six to 78 phase units per second: calm at zero, clearly flowing at full speed.
  const uint8_t drift = static_cast<uint8_t>(emitterX);
  const uint8_t paletteDrift = static_cast<uint8_t>(emitterY);
  const uint8_t weaveDrift = sin8(drift);
  const uint8_t sheenDrift = sin8(drift + 64U);

  for (uint8_t y = 0; y < HEIGHT; y++) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      // One full angular turn across WIDTH makes x == WIDTH meet x == 0.
      const uint8_t horizontalAngle = static_cast<uint16_t>(x) * 256U / WIDTH;
      const uint8_t diagonal = horizontalAngle + y * verticalPhase;
      const uint8_t denserDiagonal = horizontalAngle * 2U + y * (verticalPhase + 2U);
      const uint8_t transverse = horizontalAngle * 2U - y * 16U;
      const int16_t weave = (static_cast<int16_t>(sin8(transverse + weaveDrift)) - 128) / 5;
      const uint8_t broadFold = sin8(diagonal + drift + weave);
      const uint8_t denseFold = sin8(denserDiagonal + drift + weave);
      const uint8_t fold = lerp8by8(broadFold, denseFold, densityMix);
      const uint8_t sheen = sin8(diagonal + transverse / 2U + sheenDrift);
      const uint8_t light = qadd8(30U, scale8(ease8InOutApprox(fold), 190U));
      const uint8_t colorIndex = paletteDrift + scale8(diagonal, 38U) + scale8(sheen, 24U);
      CRGB pixel = ColorFromPalette(palette, colorIndex, 255U, LINEARBLEND);

      pixel.nscale8(light);
      ctx.led.drawPixel(x, y, pixel);
    }
  }
}
