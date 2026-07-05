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

  uint8_t applyAudioCrossfade(uint8_t base, uint8_t signal, uint8_t amount) {
    const uint16_t b = base;
    const uint16_t s = signal;
    const uint16_t a = amount;
    return static_cast<uint8_t>((b * (255U - a) + s * a) / 255U);
  }

  RuntimeEffectSettings applyModulation(EffectSettings settings, const AudioFrame& audio, const AudioConfig& config) {
    RuntimeEffectSettings runtime = RuntimeEffectSettings::fromSettings(settings);

    if (!audio.available || config.mode == AudioMode::Off || config.mode == AudioMode::Effect) {
      return runtime;
    }

    const uint8_t signal = selectAudioBand(audio, config.band);

    switch (config.mode) {
      case AudioMode::Brightness:
        runtime.brightness = applyAudioCrossfade(settings.brightness, signal, config.amount);
        break;

      case AudioMode::Speed: runtime.speed = applyAudioCrossfade(settings.speed, signal, config.amount); break;

      case AudioMode::Scale: runtime.scale = applyAudioCrossfade(settings.scale, signal, config.amount); break;

      default: break;
    }

    return runtime;
  }

} // namespace AudioModulation
