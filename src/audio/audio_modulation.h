#pragma once

#include "../effect/settings.h"
#include "audio_config.h"
#include "audio_frame.h"

namespace AudioModulation {

  uint8_t selectAudioBand(const AudioFrame& audio, AudioBand band);

  // Crossfade between base and audio signal driven by amount:
  //   amount=0   -> base (audio off)
  //   amount=255 -> signal (audio takes over fully)
  //   middle     -> linear blend
  // Unlike a headroom-based boost this keeps audio influence meaningful across
  // the full base range (no collapse at base=255).
  uint8_t applyAudioCrossfade(uint8_t base, uint8_t signal, uint8_t amount);

  RuntimeEffectSettings applyModulation(EffectSettings settings, const AudioFrame& audio, const AudioConfig& config);

} // namespace AudioModulation
