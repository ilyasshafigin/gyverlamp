#include "audio_service.h"

#include <FastLED.h>

#include "../storage/eeprom_store.h"

void AudioService::init() {
  microphone_.init();
  config_ = eeprom_.readAudioConfig();
}

void AudioService::tick(bool readEnabled) {
  if (readEnabled) {
    microphone_.tick();
    frame_ = microphone_.frame();
  } else {
    clearFrame();
  }

  persistConfigIfNeeded(millis());
}

void AudioService::setConfig(const AudioConfig& config) {
  if (config_.mode == config.mode && config_.band == config.band && config_.amount == config.amount) {
    return;
  }

  config_ = config;
  markConfigChanged();
}

void AudioService::setMode(AudioMode mode) {
  if (config_.mode == mode) return;
  config_.mode = mode;
  markConfigChanged();
}

void AudioService::setBand(AudioBand band) {
  if (config_.band == band) return;
  config_.band = band;
  markConfigChanged();
}

void AudioService::setAmount(uint8_t amount) {
  if (config_.amount == amount) return;
  config_.amount = amount;
  markConfigChanged();
}

void AudioService::markConfigChanged() {
  configChanged_ = true;
  configPersistTimer_ = millis();
}

void AudioService::persistConfigIfNeeded(uint32_t now) {
  if (!configChanged_) return;
  if (now - configPersistTimer_ < 1500) return;

  if (eeprom_.writeAudioConfig(config_)) {
    configChanged_ = false;
  }
}
