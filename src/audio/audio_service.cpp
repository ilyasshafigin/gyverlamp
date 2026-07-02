
#include <FastLED.h>
#include "../storage/eeprom_store.h"
#include "audio_service.h"

void AudioService::init() {
  _microphone.init();
  _config = _eeprom.readAudioConfig();
}

void AudioService::tick() {
  _microphone.tick();
  _frame = _microphone.frame();

  persistConfigIfNeeded(millis());
}

void AudioService::setConfig(const AudioConfig& config) {
  if (_config.mode == config.mode &&
    _config.band == config.band &&
    _config.amount == config.amount) {
    return;
  }

  _config = config;
  markConfigChanged();
}

void AudioService::setMode(AudioMode mode) {
  if (_config.mode == mode) return;
  _config.mode = mode;
  markConfigChanged();
}

void AudioService::setBand(AudioBand band) {
  if (_config.band == band) return;
  _config.band = band;
  markConfigChanged();
}

void AudioService::setAmount(uint8_t amount) {
  if (_config.amount == amount) return;
  _config.amount = amount;
  markConfigChanged();
}

void AudioService::markConfigChanged() {
  _configChanged = true;
  _configPersistTimer = millis();
}

void AudioService::persistConfigIfNeeded(uint32_t now) {
  if (!_configChanged) return;
  if (now - _configPersistTimer < 1500) return;

  if (_eeprom.writeAudioConfig(_config)) {
    _configChanged = false;
  }
}
