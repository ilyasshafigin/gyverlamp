#include "stained_glass.h"

#include "../shared.h"

// Effect: Stained Glass - Живой витраж
// Матрица превращается в витраж с крупными мягкими ячейками. Границы ячеек не резкие чёрные линии, а приглушённые тёмные прожилки.
// Сами цветные области очень медленно «дышат»: растягиваются, сжимаются и перетекают друг в друга, сохраняя ощущение единой композиции.
// Authors: ilyasshafigin + Codex

namespace {

  constexpr uint8_t kPointCount = 6;
  constexpr float kBassWeight[kPointCount] = {1.0F, 0.15F, 0.35F, 0.85F, 0.10F, 0.55F};
  constexpr float kTrebleWeight[kPointCount] = {0.10F, 0.85F, 0.45F, 0.15F, 1.0F, 0.35F};
  constexpr float kRelease[kPointCount] = {420, 260, 510, 650, 340, 750};
  constexpr float kMoveX[kPointCount] = {-0.8F, 0.55F, -0.45F, 0.65F, -0.6F, 0.65F};
  constexpr float kMoveY[kPointCount] = {0.45F, -0.4F, 0.6F, -0.45F, 0.45F, -0.5F};
  constexpr uint8_t kCenterX[kPointCount] = {2, 7, 12, 4, 10, 14};
  constexpr uint8_t kCenterY[kPointCount] = {3, 2, 4, 10, 12, 9};
  constexpr uint8_t kPhaseX[kPointCount] = {0, 43, 91, 137, 181, 223};
  constexpr uint8_t kPhaseY[kPointCount] = {69, 151, 233, 37, 119, 201};
  constexpr uint8_t kColor[kPointCount] = {8, 49, 92, 137, 181, 224};

  void follow(float& value, float target, float dt, float attack, float release) {
    value += (target - value) * (1.0F - expf(-dt / (target > value ? attack : release)));
  }

  float wrappedDistance(float x, float pointX) {
    float distance = fabsf(x - pointX);
    return min(distance, static_cast<float>(WIDTH) - distance);
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
  // Owned while this effect is active: phase, level/bass/treble, breath, and six cell envelopes.
  ff_x = 0;
  emitterX = emitterY = emitterZ = speedfactor = 0;
  for (uint8_t i = 0; i < kPointCount; ++i)
    trackingObjectShift[i] = 0;
}

void EffectStainedGlass::render(EffectContext& ctx) {
  // Shared workspace aliases: level, bass, treble, breath, then per-cell light.
  float& levelEnv = emitterX;
  float& bassEnv = emitterY;
  float& trebleEnv = emitterZ;
  float& breathEnv = speedfactor;
  float* cellEnv = trackingObjectShift;
  const bool active = ctx.audioConfig.mode == AudioMode::Effect && ctx.audio.available;
  const float amount = active ? ctx.audioConfig.amount / 255.0F : 0.0F;
  const float dt = ctx.deltaMs;
  follow(levelEnv, active ? ctx.audio.level / 255.0F : 0, dt, 100, 400);
  follow(bassEnv, active ? ctx.audio.bass / 255.0F : 0, dt, 80, 260);
  follow(trebleEnv, active ? ctx.audio.treble / 255.0F : 0, dt, 80, 220);
  follow(breathEnv, sqrtf(bassEnv), dt, 120, 750);
  for (uint8_t i = 0; i < kPointCount; ++i) {
    const float target = constrain(kBassWeight[i] * bassEnv + kTrebleWeight[i] * trebleEnv, 0.0F, 1.0F);
    follow(cellEnv[i], target, dt, 100, kRelease[i]);
  }
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

  for (uint8_t i = 0; i < kPointCount; ++i) {
    pointX[i] = wrapX(pointX[i] + 5.4F * amount * breathEnv * kMoveX[i]);
    pointY[i] += 5.4F * amount * breathEnv * kMoveY[i];
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
      const float lit =
        0.32F + 0.68F * (1.0F - (1.0F - 0.80F * sqrtf(cellEnv[cell])) * (1.0F - 0.70F * sqrtf(levelEnv)));
      const float gain = 1.0F - amount * (1.0F - lit);
      pixel.r = static_cast<uint8_t>(constrain(pixel.r * gain, 0.0F, 255.0F));
      pixel.g = static_cast<uint8_t>(constrain(pixel.g * gain, 0.0F, 255.0F));
      pixel.b = static_cast<uint8_t>(constrain(pixel.b * gain, 0.0F, 255.0F));
      ctx.led.drawPixel(x, y, pixel);
    }
  }
}
