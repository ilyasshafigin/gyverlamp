#pragma once

#include "../hardware/microphone.h"
#include "audio_config.h"
#include "audio_frame.h"

class EepromStore;

class AudioService {
public:
  explicit AudioService(EepromStore& eeprom)
    : _eeprom(eeprom),
      _microphone() {}

  void init();
  void tick(bool readEnabled = true);

  const AudioFrame& frame() const { return _frame; }
  const AudioConfig& config() const { return _config; }

  void setFrame(const AudioFrame& frame) { _frame = frame; }
  void clearFrame() { _frame = AudioFrame{}; }

  void setConfig(const AudioConfig& config);
  void setMode(AudioMode mode);
  void setBand(AudioBand band);
  void setAmount(uint8_t amount);

private:
  EepromStore& _eeprom;
  Microphone _microphone;
  AudioFrame _frame;
  AudioConfig _config;

  bool _configChanged = false;
  uint32_t _configPersistTimer = 0;

  void markConfigChanged();
  void persistConfigIfNeeded(uint32_t now);
};
