#include "../effect/catalog.h"
#include "../effect/palette_catalog.h"
#include "settings_repository.h"
#include "eeprom_store.h"

void SettingsRepository::init() {
  selectedPalette_ = eeprom_.readGlobalPaletteId();
  globalBrightness_ = eeprom_.readGlobalBrightness();

  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    effects_[i] = EffectSettings::fromSpec(Effects::getEffectSettingsSpec(Effects::toId(i)));
  }

  eeprom_.ensureEffectSettings(effects_);

  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    Effects::Id effectId = Effects::toId(i);
    eeprom_.readEffectSettings(effectId, effects_[i]);
  }
}

void SettingsRepository::tick(const Effects::Id currentEffectId) {
  const uint32_t now = millis();
  bool saved = false;
  if (shouldPersistEffectSettings(now)) {
    eeprom_.writeEffectSettings(currentEffectId, getEffectSettings(currentEffectId));
    saved = true;
  }
  if (shouldPersistPalette(now)) {
    eeprom_.writeGlobalPaletteId(selectedPalette_);
    saved = true;
  }
  if (shouldPersistGlobalBrightness(now)) {
    eeprom_.writeGlobalBrightness(globalBrightness_);
    saved = true;
  }
  if (saved) {
    persistTimer_ = now;
  }
}

EffectSettings& SettingsRepository::getEffectSettings(Effects::Id effectId) {
  return effects_[Effects::toIndex(Effects::clamp(effectId))];
}

EffectSettings& SettingsRepository::getEffectSettingsByIndex(uint8_t index) {
  return effects_[Effects::toIndex(Effects::clamp(Effects::toId(index)))];
}

const EffectSettings& SettingsRepository::getEffectSettings(Effects::Id effectId) const {
  return effects_[Effects::toIndex(Effects::clamp(effectId))];
}

const EffectSettings& SettingsRepository::getEffectSettingsByIndex(uint8_t index) const {
  return effects_[Effects::toIndex(Effects::clamp(Effects::toId(index)))];
}

void SettingsRepository::markEffectSettingsChanged() {
  effectSettingsChanged_ = true;
  persistTimer_ = millis();
}

void SettingsRepository::setPalette(Palettes::Id paletteId) {
  paletteId = Palettes::clamp(paletteId);
  if (paletteId == selectedPalette_) return;

  selectedPalette_ = paletteId;
  paletteChanged_ = true;
  persistTimer_ = millis();
}

void SettingsRepository::setGlobalBrightness(uint8_t value) {
  if (value == globalBrightness_) return;

  globalBrightness_ = value;
  globalBrightnessChanged_ = true;
  persistTimer_ = millis();
}

bool SettingsRepository::resetEffectSettingsToDefaults(const EffectSettings* defaults) {
  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    effects_[i] = defaults[i];
  }

  const bool saved = eeprom_.writeAllEffectSettings(effects_);
  if (saved) {
    effectSettingsChanged_ = false;
  }
  return saved;
}

bool SettingsRepository::shouldPersistEffectSettings(uint32_t now) {
  if (effectSettingsChanged_ && now - persistTimer_ > 30000) {
    effectSettingsChanged_ = false;
    return true;
  } else {
    return false;
  }
}

bool SettingsRepository::shouldPersistPalette(uint32_t now) {
  if (paletteChanged_ && now - persistTimer_ > 30000) {
    paletteChanged_ = false;
    return true;
  }
  return false;
}

bool SettingsRepository::shouldPersistGlobalBrightness(uint32_t now) {
  if (globalBrightnessChanged_ && now - persistTimer_ > 30000) {
    globalBrightnessChanged_ = false;
    return true;
  }
  return false;
}
