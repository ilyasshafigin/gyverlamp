#include "noise.h"

#include "../shared.h"

// Effect: Noise effects
// Based on:
//   - Whilser/GyverLamp

uint8_t colorLoop = 1;

namespace {
  float envelope(float current, uint8_t target, uint32_t deltaMs) {
    const float tau = target > current ? 20.0f : 120.0f;
    current += (target - current) * (float(deltaMs) / (tau + float(deltaMs)));
    return current < 0.25f ? 0.0f : current;
  }

  // Q8 display coordinates, clamped at this effect's edges. Never alter noise3d.
  uint8_t displaySample(int x, int y) {
    x = constrain(x, 0, (WIDTH - 1) * 256);
    y = constrain(y, 0, (HEIGHT - 1) * 256);
    const int x0 = x / 256, y0 = y / 256;
    const int x1 = min(x0 + 1, WIDTH - 1), y1 = min(y0 + 1, HEIGHT - 1);
    const unsigned fx = x & 255, fy = y & 255;
    const unsigned a = noise3d[0][x0][y0] * (256 - fx) + noise3d[0][x1][y0] * fx;
    const unsigned b = noise3d[0][x0][y1] * (256 - fx) + noise3d[0][x1][y1] * fx;
    return (a * (256 - fy) + b * fy) >> 16;
  }
} // namespace

void EffectNoise::setup(EffectContext& ctx) {
  // Shared audio envelopes: Level, Bass, Treble.
  emitterX = emitterY = emitterZ = 0;
  colorLoop = 1;
  hue = 0;
  ff_x = 0;
  ff_y = 0;
  ff_z = 0;
  memset(noise3d[0], 0, sizeof(noise3d[0]));
}

void EffectNoise::render(EffectContext& ctx) {
  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : *Palettes::paletteByScale(ctx.scale);
  if (ctx.audioConfig.mode == AudioMode::Effect && ctx.audio.available && ctx.audioConfig.amount) {
    emitterX = envelope(emitterX, ctx.audio.level, ctx.deltaMs);
    emitterY = envelope(emitterY, ctx.audio.bass, ctx.deltaMs);
    emitterZ = envelope(emitterZ, ctx.audio.treble, ctx.deltaMs);
  } else {
    emitterX = emitterY = emitterZ = 0;
  }
  const float amount = ctx.audioConfig.amount / 255.0f * kAudioGain;
  const uint8_t level = min(255.0f, emitterX * amount);
  const uint8_t bass = min(255.0f, emitterY * amount);
  const uint8_t treble = min(255.0f, emitterZ * amount);
  const uint8_t phase = ctx.nowMs / (60 - unsigned(ctx.speed) * 45 / 255);
  const uint8_t wavelength = 16 + ctx.scale / 8;

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
      int indexShift = 0;
      int brightnessShift = 0;
      if (level || bass || treble) {
        const int cx = int(i) - 8, cy = int(j) - 8;
        const int ripple = int(sin8(abs(cx) * 22 + abs(cy) * 28 - phase * 3 + bass / 2)) - 128;
        const int waveX = int(sin8(j * wavelength + phase + bass / 3)) - 128;
        const int waveY = int(sin8(i * wavelength - phase * 2 + bass / 3)) - 128;
        const int fineX = int(sin8((i + j) * (wavelength + 24) + phase * 2 + treble / 2)) - 128;
        const int fineY = int(sin8((int(i) - j) * (wavelength + 24) - phase * 3)) - 128;
        const int fine = (fineX + fineY) / 2;
        const int broad = (waveX + waveY + ripple) / 3;
        // Bass: 2 px/axis wave + radial displacement. Treble: 0.75 px/axis
        // intersecting waves. Combined vector <= sqrt(2)*2.75 = 3.89 px.
        const int dx =
          ((waveX + ripple * cx / 8) / 2) * int(bass) * 512 / (128 * 255) + fineX * int(treble) * 192 / (128 * 255);
        const int dy =
          ((waveY + ripple * cy / 8) / 2) * int(bass) * 512 / (128 * 255) + fineY * int(treble) * 192 / (128 * 255);
        index = displaySample(j * 256 + dy, i * 256 + dx);
        bri = displaySample(i * 256 + dx, j * 256 + dy);
        // Palette travel and signed spatial contrast remain audible even where
        // the legacy brightness curve is already saturated. No white overlay.
        indexShift = (ripple * int(bass) * 64 + fine * int(treble) * 80 + broad * int(level) * 36) / (128 * 255);
        brightnessShift = (ripple * int(bass) * 80 + fine * int(treble) * 90 + broad * int(level) * 112) / (128 * 255);
      }
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
      index += indexShift;
      bri = constrain(int(bri) + brightnessShift + (int(bri) - 128) * int(level) / 510, 0, 255);
      CRGB color = ColorFromPalette(palette, index, bri);
      ctx.led.drawPixel(i, j, color);
    }
  }
  hue += 1;
}
