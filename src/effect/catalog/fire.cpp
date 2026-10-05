#include "fire.h"

#include "../shared.h"

// Effect: Fire - Огонь
// Authors: ilyasshafigin + Codex

namespace {

  constexpr uint16_t kFireStepMs = 20U;
  constexpr uint16_t kPhaseOneQ8 = 256U;
  constexpr uint16_t kPhaseMaxQ8 = 4U * kPhaseOneQ8 + kPhaseOneQ8 / 2U;
  constexpr uint16_t kPhaseRangeQ8 = kPhaseMaxQ8 - kPhaseOneQ8;
  constexpr uint8_t kVisibleHeat = 22U;
  constexpr uint8_t kWhiteCoreHeat = 236U;

  uint16_t phaseStepQ8(uint8_t speed) {
    // Keep speed 0/1 at the old 1.0 rate, and reach exactly 4.5 at 255.
    if (speed <= 1U) return kPhaseOneQ8;

    const uint32_t progress = static_cast<uint32_t>(speed - 1U) * kPhaseRangeQ8;
    return kPhaseOneQ8 + (progress + 127U) / 254U;
  }

  CRGB fireColor(uint8_t heat, const CRGBPalette16* palette) {
    if (heat < kVisibleHeat) return CRGB::Black;

    if (palette) {
      return ColorFromPalette(*palette, heat, heat);
    }

    // Keep the normal flame in black-red-orange-yellow. Only exceptional peaks
    // reach the warm-white core in Auto mode, rather than making the whole base pale.
    if (heat >= kWhiteCoreHeat) {
      return CRGB(scale8(255U, heat), scale8(244U, heat), scale8(205U, heat));
    }

    const uint8_t paletteIndex = map(heat, 0U, kWhiteCoreHeat, 0U, 180U);
    return ColorFromPalette(HeatColors_p, paletteIndex, heat);
  }

  uint8_t wrapColumn(int16_t x) {
    int16_t wrapped = x % WIDTH;
    if (wrapped < 0) wrapped += WIDTH;
    return static_cast<uint8_t>(wrapped);
  }

  uint8_t ringTurbulence(uint8_t x, uint8_t y, uint8_t radiusStep, uint8_t phase) {
    // Embed X on a circle before sampling 3D noise. At the conceptual X=WIDTH
    // endpoint angle wraps to zero, so both the field and advection join there.
    const uint8_t angle = static_cast<uint8_t>((static_cast<uint32_t>(x) * 256U) / WIDTH);

    // Two periodic, height-dependent waves bend the sampling angle over time.
    // Their different rates prevent the whole circumference moving in lockstep.
    const uint8_t waveA = sin8(static_cast<uint8_t>(angle * 3U + y * 11U + phase));
    const uint8_t waveB = sin8(static_cast<uint8_t>(angle * 5U - y * 7U - phase * 2U));
    const int16_t angularWarp = (static_cast<int16_t>(waveA) - 128) / 5 + (static_cast<int16_t>(waveB) - 128) / 10;
    const uint8_t warpedAngle = static_cast<uint8_t>(angle + angularWarp);

    const int16_t circleX = (static_cast<int16_t>(cos8(warpedAngle)) - 128) * radiusStep;
    const int16_t circleY = (static_cast<int16_t>(sin8(warpedAngle)) - 128) * radiusStep;
    const uint16_t noiseX = static_cast<uint16_t>(32768 + circleX);
    const uint16_t noiseY = static_cast<uint16_t>(32768 + circleY);
    return inoise8(noiseX, noiseY, static_cast<uint16_t>(y * 18U));
  }

} // namespace

void EffectFire::setup(EffectContext&) {
  // Reuse the shared counters while Fire is active: ff_x is an 8.8 phase,
  // ff_y its sub-step millisecond accumulator, and ff_z its heat-layer index.
  ff_x = 0U;
  ff_y = 0U;
  ff_z = 0U;

  // Shared heat layers can contain any previous effect's state.
  for (int16_t x = 0; x < WIDTH; x++) {
    for (int16_t y = 0; y < HEIGHT; y++) {
      noise3d[0][x][y] = 0U;
      noise3d[1][x][y] = 0U;
    }
  }
}

void EffectFire::render(EffectContext& ctx) {
  // A fixed simulation step keeps the flame independent of render cadence.
  // deltaMs is capped at 100 ms, so ff_y remains below 120 before subtraction.
  const uint16_t elapsedMs = ctx.deltaMs > 100U ? 100U : ctx.deltaMs;
  ff_y += elapsedMs;

  const uint8_t minimumHeight = HEIGHT < 6 ? (HEIGHT > 1 ? HEIGHT - 1 : 1) : HEIGHT / 2;
  const uint8_t flameHeight = map(ctx.scale, 0U, 255U, minimumHeight, HEIGHT);
  const uint8_t radiusStep = map(ctx.scale, 0U, 255U, 4U, 10U);
  const uint8_t scaleIgnitionHeat = map(ctx.scale, 0U, 255U, 126U, 150U);
  const uint8_t highScaleRelief = ctx.scale > 128U ? map(ctx.scale, 128U, 255U, 0U, 4U) : 0U;
  const uint8_t ignitionHeat = scaleIgnitionHeat - highScaleRelief;

  // Speed changes phase evolution, never the number of simulation steps.
  const uint16_t phaseAdvanceQ8 = phaseStepQ8(ctx.speed);

  while (ff_y >= kFireStepMs) {
    ff_y -= kFireStepMs;
    ff_x += phaseAdvanceQ8;
    const uint8_t phase = static_cast<uint8_t>(ff_x >> 8U);

    const uint8_t sourceLayer = static_cast<uint8_t>(ff_z);
    const uint8_t destinationLayer = 1U - sourceLayer;

    for (int16_t y = 0; y < HEIGHT; y++) {
      for (int16_t x = 0; x < WIDTH; x++) {
        const uint8_t turbulence = ringTurbulence(x, y, radiusStep, phase);
        const int16_t flicker = (static_cast<int16_t>(turbulence) - 128) / 5;
        uint8_t heat = 0U;

        if (y == 0) {
          // Fuel spans the whole ring, but coherent noise varies its strength
          // so it reads as an uneven ignition line, not a uniform bright band.
          const uint16_t fuel = 24U + (static_cast<uint16_t>(turbulence) * 7U) / 8U;
          heat = fuel > 255U ? 255U : static_cast<uint8_t>(fuel);
        } else if (y < flameHeight) {
          // Carry heat upward with a small cyclic lateral drift. The weighted
          // center tap keeps tongues narrow while side taps let them split.
          int16_t flowX = x;
          if (turbulence > 176U) {
            flowX++;
          } else if (turbulence < 80U) {
            flowX--;
          }

          const uint8_t center = wrapColumn(flowX);
          const uint8_t left = wrapColumn(static_cast<int16_t>(center) - 1);
          const uint8_t right = wrapColumn(static_cast<int16_t>(center) + 1);
          const uint8_t belowY = y - 1;
          const uint16_t transported = static_cast<uint16_t>(noise3d[sourceLayer][center][belowY]) * 6U +
                                       noise3d[sourceLayer][left][belowY] + noise3d[sourceLayer][right][belowY];
          const uint8_t risingHeat = transported / 8U;

          // Scale sets target height and turbulence coarseness. Cooling grows
          // with altitude so the tops fade instead of ending as a flat cut.
          const uint16_t cooling = 120U / flameHeight + (y * 6U) / flameHeight;
          const int16_t cooled = static_cast<int16_t>(risingHeat) - cooling + flicker;
          const bool ignites = y > 1 || risingHeat >= ignitionHeat;
          if (ignites && risingHeat >= kVisibleHeat && cooled > 0) {
            heat = cooled > 255 ? 255U : static_cast<uint8_t>(cooled);
          }
        }

        noise3d[destinationLayer][x][y] = heat;
      }
    }

    ff_z = destinationLayer;
  }

  const uint8_t displayLayer = static_cast<uint8_t>(ff_z);
  for (int16_t x = 0; x < WIDTH; x++) {
    for (int16_t y = 0; y < HEIGHT; y++) {
      ctx.led.pixel(x, y) = fireColor(noise3d[displayLayer][x][y], ctx.palette);
    }
  }
}
