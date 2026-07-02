#pragma once

#include "audio_config.h"
#include "audio_frame.h"
#include "../effect/settings.h"

namespace AudioModulation {

  uint8_t selectAudioBand(const AudioFrame& audio, AudioBand band);
  uint8_t applyAudioBoost(uint8_t base, uint8_t signal, uint8_t amount);

  RuntimeEffectSettings applyModulation(
    EffectSettings settings,
    const AudioFrame& audio,
    const AudioConfig& config
  );

}
