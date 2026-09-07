#pragma once

#include "../hardware/microphone.h"

#include "audio_config.h"
#include "audio_frame.h"

class EepromStore;

class AudioService {
public:
  explicit AudioService(EepromStore& eeprom)
    : eeprom_(eeprom),
      microphone_() {}

  void init();
  void tick(bool readEnabled = true);

  const AudioFrame& frame() const { return frame_; }
  const AudioConfig& config() const { return config_; }

  void setFrame(const AudioFrame& frame) { frame_ = frame; }
  void clearFrame() { frame_ = AudioFrame{}; }

  void setConfig(const AudioConfig& config);
  void setMode(AudioMode mode);
  void setBand(AudioBand band);
  void setAmount(uint8_t amount);

private:
  EepromStore& eeprom_;
  Microphone microphone_;
  AudioFrame frame_;
  AudioConfig config_;

  bool configChanged_ = false;
  uint32_t configPersistTimer_ = 0;

  void markConfigChanged();
  void persistConfigIfNeeded(uint32_t now);
};
