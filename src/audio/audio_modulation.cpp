#include "audio_modulation.h"

namespace AudioModulation {

  uint8_t selectAudioBand(const AudioFrame& audio, AudioBand band) {
    switch (band) {
      case AudioBand::Bass: return audio.bass;
      case AudioBand::Treble: return audio.treble;
      case AudioBand::Level:
      default: return audio.level;
    }
  }

  uint8_t applyAudioBoost(uint8_t base, uint8_t signal, uint8_t amount) {
    const uint16_t boost = (static_cast<uint16_t>(255U - base) * signal * amount) / 255U / 255U;
    return base + boost;
  }

  RuntimeEffectSettings applyModulation(EffectSettings settings, const AudioFrame& audio, const AudioConfig& config) {
    RuntimeEffectSettings runtime = RuntimeEffectSettings::fromSettings(settings);

    if (!audio.available || config.mode == AudioMode::Off || config.mode == AudioMode::Effect) {
      return runtime;
    }

    const uint8_t signal = selectAudioBand(audio, config.band);

    switch (config.mode) {
      case AudioMode::Brightness:
        runtime.brightness = applyAudioBoost(settings.brightness, signal, config.amount);
        break;

      case AudioMode::Speed: runtime.speed = applyAudioBoost(settings.speed, signal, config.amount); break;

      case AudioMode::Scale: runtime.scale = applyAudioBoost(settings.scale, signal, config.amount); break;

      default: break;
    }

    return runtime;
  }

} // namespace AudioModulation
