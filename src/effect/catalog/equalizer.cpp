#include "equalizer.h"
#include "../shared.h"

#include <Arduino.h>

namespace {

  enum class EqualizerMode : uint8_t {
    Classic,
    Mirror,
    Fire,
    Plasma,
  };

  inline EqualizerMode modeFromScale(uint8_t scale) {
    if (scale < 64) return EqualizerMode::Classic;
    if (scale < 128) return EqualizerMode::Mirror;
    if (scale < 192) return EqualizerMode::Fire;
    return EqualizerMode::Plasma;
  }

  inline uint8_t modeAmount(uint8_t scale) {
    return (scale & 0x3F) << 2;  // 0..252 inside selected mode
  }

  inline uint8_t smoothVisual(uint8_t current, uint8_t target) {
    const uint8_t k = target > current ? 180 : 96;
    return current + ((static_cast<int16_t>(target) - current) * k) / 255;
  }

  CRGB paletteColor(const CRGBPalette16* palette, uint8_t index, uint8_t value) {
    return palette ? ColorFromPalette(*palette, index, value) : CHSV(index, 255, value);
  }

  uint8_t columnIndex(uint8_t y, uint8_t height, uint8_t energy, uint8_t bass, uint8_t treble, uint8_t power, uint32_t nowMs) {
    if (height <= 1) return 0;

    // Чем громче, тем сильнее палитра растягивается вверх.
    const uint8_t stretch = map(energy, 0, 255, 80, 190);
    // Bass толкает низ в тёплые зоны, treble сдвигает верх.
    const uint8_t audioShift = qadd8(scale8(bass, 45), scale8(treble, 80));
    // Очень медленный drift, чтобы палитра не была прибита гвоздями.
    const uint8_t drift = (nowMs / map(power, 0, 255, 90, 25)) & 0xFF;
    const uint8_t local = map(y, 0, height - 1, 0, 255);

    return scale8(local, stretch) + audioShift + scale8(drift, 35);
  }

  CRGB classicColor(
    const CRGBPalette16* palette,
    uint8_t y,
    uint8_t height,
    uint8_t energy,
    uint8_t bass,
    uint8_t treble,
    uint8_t power,
    uint32_t nowMs,
    uint8_t value
  ) {
    const uint8_t index = columnIndex(y, height, energy, bass, treble, power, nowMs);
    return paletteColor(palette, index, value);
  }

  CRGB mirrorColor(
    const CRGBPalette16* palette,
    uint8_t distance,
    uint8_t height,
    uint8_t energy,
    uint8_t bass,
    uint8_t treble,
    uint8_t power,
    uint32_t nowMs,
    uint8_t value
  ) {
    const uint8_t local = height <= 1 ? 0 : map(distance, 0, height - 1, 0, 255);
    const uint8_t pulse = beatsin8(map(power, 0, 255, 8, 28), 0, 80);
    const uint8_t index = scale8(local, map(energy, 0, 255, 90, 220)) + pulse + scale8(treble, 90);

    return paletteColor(palette, index, value);
  }

  CRGB fireColor(
    const CRGBPalette16* palette,
    uint8_t x,
    uint8_t y,
    uint8_t heat,
    uint8_t bass,
    uint8_t treble,
    uint8_t power,
    uint32_t nowMs
  ) {
    if (palette) {
      // Fire специально НЕ локальная колонка. Это поле жара.
      const uint8_t flicker = inoise8(
        x * map(power, 0, 255, 20, 55),
        y * 38,
        nowMs / map(power, 0, 255, 45, 14)
      );

      const uint8_t lift = scale8(bass, 80);
      const uint8_t sparks = y > HEIGHT / 2 ? scale8(treble, 55) : 0;

      uint8_t index = qadd8(heat, scale8(flicker, 90));
      index = qadd8(index, lift);
      index = qadd8(index, sparks);

      // compress into warm-ish palette range, but still palette-aware.
      index = scale8(index, 180);

      const uint8_t value = qadd8(20, qadd8(scale8(heat, 190), scale8(flicker, 70)));
      return paletteColor(palette, index, value);
    } else {
      const uint8_t index = scale8(heat, 235);  // не лезем часто в белый верх
      const uint8_t value = qadd8(20, scale8(heat, 210));
      return ColorFromPalette(HeatColors_p, index, value);
    }
  }

  CRGB beatColor(const CRGBPalette16* palette, uint8_t value) {
    return paletteColor(palette, 32, value);
  }

  uint8_t fakeSignal(uint32_t now, uint8_t speed, uint8_t salt, uint8_t minValue, uint8_t maxValue) {
    const uint8_t wave = beatsin8(speed, minValue, maxValue, 0, salt);
    const uint8_t noise = inoise8(now / 8, salt * 31);
    return qadd8(scale8(wave, 180), scale8(noise, 75));
  }

  void drawClassicColumn(
    EffectContext& ctx,
    uint8_t x,
    uint8_t height,
    uint8_t value,
    uint8_t energy,
    uint8_t bass,
    uint8_t treble,
    uint8_t power
  ) {
    for (uint8_t y = 0; y < height; y++) {
      ctx.led.drawPixel(x, y, classicColor(ctx.palette, y, height, energy, bass, treble, power, ctx.nowMs, value));
    }
  }

  void drawMirrorColumn(
    EffectContext& ctx,
    uint8_t x,
    uint8_t height,
    uint8_t value,
    uint8_t energy,
    uint8_t bass,
    uint8_t treble,
    uint8_t power
  ) {
    for (uint8_t i = 0; i < height; i++) {
      const CRGB color = mirrorColor(ctx.palette, i, height, energy, bass, treble, power, ctx.nowMs, value);

      if (CENTER_Y_MINOR >= i) {
        ctx.led.drawPixel(x, CENTER_Y_MINOR - i, color);
      }

      const uint8_t upperY = CENTER_Y_MAJOR + i;
      if (upperY < HEIGHT) {
        ctx.led.drawPixel(x, upperY, color);
      }
    }
  }

  void drawFireColumn(EffectContext& ctx, uint8_t x, uint8_t energy, uint8_t bass, uint8_t treble, uint8_t power) {
    const uint8_t wind = map(power, 0, 255, 14, 42);
    const uint16_t time = ctx.nowMs / map(ctx.speed, 0, 255, 42, 11);

    // Bass = топливо снизу.
    const uint8_t fuel = qadd8(scale8(energy, 120), scale8(bass, 135));

    for (uint8_t y = 0; y < HEIGHT; y++) {
      // Чем выше, тем сильнее охлаждение.
      const uint8_t falloff = (static_cast<uint16_t>(y) * map(power, 0, 255, 175, 245)) / HEIGHT;

      uint8_t heat = qsub8(fuel, falloff);

      // Языки пламени. Шум добавляет форму, но не забивает всё поле.
      const uint8_t tongue = inoise8(x * wind, y * 45, time);
      heat = qadd8(heat, scale8(tongue, 75));

      // Treble даёт верхние всполохи, не белые искры.
      if (y >= HEIGHT / 2) {
        heat = qadd8(heat, scale8(treble, 35));
      }

      // Верх дополнительно гасим, чтобы вся матрица не заливалась.
      if (y >= (HEIGHT * 3) / 4) {
        heat = scale8(heat, 150);
      }

      if (heat < 38) continue;

      ctx.led.drawPixel(x, y, fireColor(ctx.palette, x, y, heat, bass, treble, power, ctx.nowMs));
    }
  }

  void drawPlasmaColumn(EffectContext& ctx, uint8_t x, uint8_t level, uint8_t bass, uint8_t treble, uint8_t modePower) {
    const uint8_t scale = map(modePower, 0, 255, 12, 38);
    const uint8_t speedDiv = map(ctx.speed, 0, 255, 60, 9);
    const uint16_t time = ctx.nowMs / speedDiv;

    const uint8_t audioPower = qadd8(scale8(level, 130), qadd8(scale8(bass, 80), scale8(treble, 90)));

    // Audio deforms plasma field.
    const uint8_t bassWarp = scale8(bass, 55);
    const uint8_t trebleWarp = scale8(treble, 75);
    const uint8_t levelGlow = scale8(level, 85);

    for (uint8_t y = 0; y < HEIGHT; y++) {
      const uint8_t nx = x * scale + bassWarp + sin8(time + y * 19) / 4;
      const uint8_t ny = y * scale + trebleWarp + cos8(time + x * 17) / 4;

      const uint8_t n = inoise8(nx, ny, time + scale8(audioPower, 55));

      const uint8_t wave1 = sin8(x * 22 + time + scale8(bass, 100));
      const uint8_t wave2 = sin8(y * 34 + time * 2 + scale8(treble, 120));
      const uint8_t waveMix = qadd8(scale8(wave1, 95), scale8(wave2, 95));

      // Radial-ish pulse from center, stronger with bass/level.
      const int8_t dx = static_cast<int8_t>(x) - static_cast<int8_t>(WIDTH / 2);
      const int8_t dy = static_cast<int8_t>(y) - static_cast<int8_t>(HEIGHT / 2);
      const uint8_t dist = abs(dx) * 22 + abs(dy) * 28;
      const uint8_t ripple = sin8(dist + time * 3 + scale8(bass, 130));

      uint8_t index = n;
      index += scale8(waveMix, 80);
      index += scale8(ripple, scale8(audioPower, 70));

      uint8_t value = 25;
      value = qadd8(value, scale8(n, 85));
      value = qadd8(value, scale8(waveMix, 70));
      value = qadd8(value, levelGlow);
      value = qadd8(value, scale8(ripple, scale8(bass, 80)));

      // Treble = sharper bright flecks, but not white sparks.
      if (treble > 120 && ((x * 13 + y * 7 + time) & 0x1F) == 0) {
        value = qadd8(value, scale8(treble, 70));
        index += scale8(treble, 60);
      }

      ctx.led.drawPixel(x, y, paletteColor(ctx.palette, index, value));
    }
  }
}

void EffectEqualizer::setup(EffectContext& ctx) {
  _level = 0;
  _bass = 0;
  _treble = 0;
  _sparkX = 0;

  for (uint8_t x = 0; x < WIDTH; x++) {
    _peak[x] = 0;
  }
}

void EffectEqualizer::render(EffectContext& ctx) {
  const EqualizerMode mode = modeFromScale(ctx.scale);
  const uint8_t power = modeAmount(ctx.scale);

  const uint8_t fadeAmount = map(ctx.speed, 0, 255, 90, 35);
  ctx.led.fadeToBlack(fadeAmount);

  uint8_t targetLevel;
  uint8_t targetBass;
  uint8_t targetTreble;
  bool beat;

  const bool useRealAudio = ctx.audio.available && ctx.audioConfig.mode != AudioMode::Off;

  if (useRealAudio) {
    targetLevel = ctx.audio.level;
    targetBass = ctx.audio.bass;
    targetTreble = ctx.audio.treble;
    beat = ctx.audio.beat;
  } else {
    targetBass = fakeSignal(ctx.nowMs, 19, 3, 20, 220);
    targetLevel = fakeSignal(ctx.nowMs, 13, 7, 40, 200);
    targetTreble = fakeSignal(ctx.nowMs, 37, 11, 10, 180);
    beat = targetBass > 205;
  }

  _level = smoothVisual(_level, targetLevel);
  _bass = smoothVisual(_bass, targetBass);
  _treble = smoothVisual(_treble, targetTreble);

  const uint16_t noiseTime = ctx.nowMs / map(ctx.speed, 0, 255, 35, 8);
  const uint8_t noiseScale = map(power, 0, 255, 12, 55);

  for (uint8_t x = 0; x < WIDTH; x++) {
    const uint8_t wave = inoise8(x * noiseScale, noiseTime);

    uint8_t energy = scale8(_level, 170);
    energy = qadd8(energy, scale8(wave, 70));

    if (x < WIDTH / 3) {
      energy = qadd8(energy, scale8(_bass, 90));
    } else if (x >= (WIDTH * 2) / 3) {
      energy = qadd8(energy, scale8(_treble, 90));
    } else {
      energy = qadd8(energy, scale8(_level, 60));
    }

    const uint8_t value = qadd8(70, scale8(energy, 185));

    switch (mode) {
    case EqualizerMode::Classic: {
      const uint8_t height = min<uint8_t>(map8(energy, 0, HEIGHT), HEIGHT);
      drawClassicColumn(ctx, x, height, value, energy, _bass, _treble, power);

      if (height > _peak[x]) {
        _peak[x] = height;
      } else if (_peak[x] > 0 && (ctx.nowMs & 1)) {
        _peak[x]--;
      }

      if (_peak[x] > HEIGHT) {
        _peak[x] = HEIGHT;
      }

      if (_peak[x] > 0 && value > 150) {
        ctx.led.drawPixel(x, _peak[x] - 1, paletteColor(ctx.palette, 240, scale8(value, 70)));
      }
      break;
    }

    case EqualizerMode::Mirror: {
      const uint8_t height = min<uint8_t>(map8(energy, 0, HALF_HEIGHT), HALF_HEIGHT);
      drawMirrorColumn(ctx, x, height, value, energy, _bass, _treble, power);
      _peak[x] = 0;
      break;
    }

    case EqualizerMode::Fire:
      drawFireColumn(ctx, x, energy, _bass, _treble, power);
      _peak[x] = 0;
      break;

    case EqualizerMode::Plasma:
      drawPlasmaColumn(ctx, x, _level, _bass, _treble, power);
      _peak[x] = 0;
      break;
    }
  }

  if (beat) {
    switch (mode) {
    case EqualizerMode::Classic:
    case EqualizerMode::Fire:
      for (uint8_t x = 0; x < WIDTH; x++) {
        ctx.led.addPixel(x, 0, beatColor(ctx.palette, 32));
        if (HEIGHT > 1) {
          ctx.led.addPixel(x, 1, beatColor(ctx.palette, 16));
        }
      }
      break;

    case EqualizerMode::Mirror: {
      for (uint8_t x = 0; x < WIDTH; x++) {
        ctx.led.addPixel(x, CENTER_Y_MINOR, beatColor(ctx.palette, 32));
        if (CENTER_Y_MAJOR != CENTER_Y_MINOR) {
          ctx.led.addPixel(x, CENTER_Y_MAJOR, beatColor(ctx.palette, 32));
        }
      }
      break;
    }

    case EqualizerMode::Plasma:
      // Plasma already reacts through full-field brightness.
      break;
    }
  }
}
