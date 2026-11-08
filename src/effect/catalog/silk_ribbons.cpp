#include "silk_ribbons.h"

#include "../palette_catalog.h"
#include "../shared.h"

// Effect: Silk Ribbons - Шёлковые ленты
// Несколько широких полупрозрачных лент текут через матрицу по диагонали. Каждая лента изгибается синусоидой, немного меняет ширину и медленно смещается.
// В местах пересечения появляется более яркий, насыщенный цвет — эффект глубины, будто свет проходит сквозь слои шёлка.
// Authors: ilyasshafigin + Codex

namespace {

  uint8_t circularDistance(uint8_t first, uint8_t second) {
    const uint8_t clockwise = first - second;
    const uint8_t counterClockwise = second - first;
    return min(clockwise, counterClockwise);
  }

  void follow(float& value, float target, float dt, float attack, float release) {
    // Exponential time correction retains the prototype's coefficients at 50 ms.
    const float coefficient = target > value ? attack : release;
    value += (target - value) * (1.0F - powf(1.0F - coefficient, dt / 50.0F));
  }

} // namespace

void EffectSilkRibbons::setup(EffectContext&) {
  emitterX = emitterY = emitterZ = speedfactor = 0;
}

void EffectSilkRibbons::render(EffectContext& ctx) {
  // Shared workspace owned while active: level, bass, treble and beat envelopes.
  float& levelEnv = emitterX;
  float& bassEnv = emitterY;
  float& trebleEnv = emitterZ;
  float& beatEnv = speedfactor;
  const bool active = ctx.audioConfig.mode == AudioMode::Effect && ctx.audio.available && ctx.audioConfig.amount > 0;
  const float amount = active ? ctx.audioConfig.amount / 255.0F : 0.0F;
  if (active) {
    follow(levelEnv, ctx.audio.level / 255.0F, ctx.deltaMs, 0.55F, 0.16F);
    follow(bassEnv, ctx.audio.bass / 255.0F, ctx.deltaMs, 0.58F, 0.15F);
    follow(beatEnv, ctx.audio.beat ? 1.0F : 0.0F, ctx.deltaMs, 0.75F, 0.11F);
    follow(trebleEnv, ctx.audio.treble / 255.0F, ctx.deltaMs, 0.65F, 0.16F);
  } else {
    levelEnv = bassEnv = trebleEnv = beatEnv = 0;
  }
  const float gain = 1.0F - amount * 0.5F * (1.0F - levelEnv);
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
        const int16_t bend = (static_cast<int16_t>(sin8(y * 10U + sway + ribbon * 49U)) - 128) * bassEnv * amount *
                             (ribbon % 2 ? 62 : 46) / 128;
        const int16_t push = (ribbon % 2 ? -1 : 1) * beatEnv * amount * 30;
        const uint8_t center =
          drift + ribbon * spacing + sin8(x * 16U + y * 4U + sway + ribbon * 53U) / 5U + bend + push;
        const uint8_t distance = circularDistance(diagonal, center);
        if (distance >= rowHalfWidth) continue;

        const uint8_t edge = 255U - static_cast<uint16_t>(distance) * 255U / rowHalfWidth;
        const uint8_t opacity = scale8(ease8InOutApprox(edge), 176U);
        const uint8_t baseIndex = ribbon * 54U + y * 3U;
        const uint8_t paletteIndex = static_cast<uint8_t>(baseIndex + (255U - baseIndex) * 0.8F * trebleEnv * amount);
        const CRGB color = ColorFromPalette(palette, paletteIndex, 210U, LINEARBLEND_NOWRAP);
        nblend(pixel, color, opacity);
      }

      if (active) {
        pixel.r = static_cast<uint8_t>(pixel.r * gain);
        pixel.g = static_cast<uint8_t>(pixel.g * gain);
        pixel.b = static_cast<uint8_t>(pixel.b * gain);
      }
      ctx.led.drawPixel(x, y, pixel);
    }
  }
}
