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

  void updateAudioEnvelope(float& envelope, uint8_t source, float amount, float attack, float release) {
    const float target = source / 255.0F * amount;
    envelope += (target - envelope) * (target > envelope ? attack : release);
    if (target == 0.0F && envelope < 0.0001F) envelope = 0.0F;
  }

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
  // Shared audio envelopes: Level, Bass, Treble.
  emitterX = emitterY = emitterZ = 0.0F;
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
  const uint16_t baseCooling = 120U / flameHeight;

  // All three inputs have independent roles; band selection and beat are unused.
  const bool audioEnabled =
    ctx.audioConfig.mode == AudioMode::Effect && ctx.audio.available && ctx.audioConfig.amount > 0;
  if (!audioEnabled) emitterX = emitterY = emitterZ = 0.0F;
  const float amount = ctx.audioConfig.amount / 255.0F;

  while (ff_y >= kFireStepMs) {
    ff_y -= kFireStepMs;
    if (audioEnabled) {
      // Fixed-step coefficients: 1 - exp(-20 / tauMs). No render-rate dependence.
      updateAudioEnvelope(emitterX, ctx.audio.level, amount, 0.10516068F, 0.03921056F);  // 180/500 ms
      updateAudioEnvelope(emitterY, ctx.audio.bass, amount, 0.48658288F, 0.07688365F);   // 30/250 ms
      updateAudioEnvelope(emitterZ, ctx.audio.treble, amount, 0.48658288F, 0.13312210F); // 30/140 ms
    }
    ff_x += phaseAdvanceQ8;
    const uint8_t phase = static_cast<uint8_t>(ff_x >> 8U);

    const uint8_t sourceLayer = static_cast<uint8_t>(ff_z);
    const uint8_t destinationLayer = 1U - sourceLayer;

    for (int16_t y = 0; y < HEIGHT; y++) {
      const uint16_t heightCooling = (y * 6U) / flameHeight;
      for (int16_t x = 0; x < WIDTH; x++) {
        if (y >= flameHeight) {
          noise3d[destinationLayer][x][y] = 0U;
          continue;
        }

        const uint8_t turbulence = ringTurbulence(x, y, radiusStep, phase);
        const int16_t flicker = (static_cast<int16_t>(turbulence) - 128) / 5;
        uint8_t heat = 0U;

        if (y == 0) {
          // Fuel spans the whole ring, but coherent noise varies its strength
          // so it reads as an uneven ignition line, not a uniform bright band.
          const uint16_t fuel = 24U + (static_cast<uint16_t>(turbulence) * 7U) / 8U;
          heat = fuel > 255U ? 255U : static_cast<uint8_t>(fuel);
        } else {
          // Carry heat upward with a small cyclic lateral drift. The weighted
          // center tap keeps tongues narrow while side taps let them split.
          int16_t flowX = x;
          if (turbulence > 176U) {
            flowX++;
          } else if (turbulence < 80U) {
            flowX--;
          }

          const uint8_t center = static_cast<uint8_t>(wrapX(flowX));
          const uint8_t left = static_cast<uint8_t>(wrapX(static_cast<int16_t>(center) - 1));
          const uint8_t right = static_cast<uint8_t>(wrapX(static_cast<int16_t>(center) + 1));
          const uint8_t belowY = y - 1;
          const uint16_t transported = static_cast<uint16_t>(noise3d[sourceLayer][center][belowY]) * 6U +
                                       noise3d[sourceLayer][left][belowY] + noise3d[sourceLayer][right][belowY];
          const uint8_t risingHeat = transported / 8U;

          // Scale sets target height and turbulence coarseness. Cooling grows
          // with altitude so the tops fade instead of ending as a flat cut.
          const uint16_t cooling = baseCooling + heightCooling;
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
  const uint8_t phase = static_cast<uint8_t>(ff_x >> 8U);
  for (int16_t x = 0; x < WIDTH; x++) {
    for (int16_t y = 0; y < HEIGHT; y++) {
      const uint8_t heat = noise3d[displayLayer][x][y];
      uint8_t displayHeat = heat;
      if (heat >= kVisibleHeat && y > 0 && y < flameHeight && (emitterX > 0.0F || emitterY > 0.0F || emitterZ > 0.0F)) {
        // Modulate only existing flame pixels; the simulation and black sky stay intact.
        const uint8_t turbulence = ringTurbulence(x, y, radiusStep, phase);
        const float tongue = constrain((turbulence - 80.0F) / 112.0F, 0.0F, 1.0F);
        const float upper = constrain((4.0F * y / flameHeight - 0.5F) / 1.5F, 0.0F, 1.0F);
        const uint8_t tipNoise = ringTurbulence(x, y, radiusStep, static_cast<uint8_t>(phase * 3U + 67U));
        const float tip = constrain((tipNoise - 80.0F) / 112.0F, 0.0F, 1.0F);
        const float glow =
          85.0F * emitterX * (0.55F + 0.45F * tongue) + 150.0F * emitterY * tongue + 190.0F * emitterZ * upper * tip;
        // Do not turn highlights white or dim an existing naturally hot core.
        const int16_t headroom = heat < 200U ? 200 - heat : 0;
        int16_t boost = static_cast<int16_t>(glow + 0.5F);
        if (boost > headroom) boost = headroom;
        displayHeat = heat + boost;
      }
      ctx.led.pixel(x, y) = fireColor(displayHeat, ctx.palette);
    }
  }
}
