#include "../effect/catalog.h"
#include "../effect/palette_catalog.h"
#include "settings_repository.h"
#include "eeprom_store.h"

void SettingsRepository::init() {
  _bootCount = _eeprom.readBootCount();
  _bootCount++;
  _eeprom.writeBootCount(_bootCount);

  // Чуть ждем после увеличения bootCount на случай следующей перезагрузки
  delay(50);

  _selectedPalette = _eeprom.readGlobalPaletteId();

  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    _effects[i] = EffectSettings::fromSpec(Effects::getEffectSettingsSpec(Effects::toId(i)));
  }

  _eeprom.ensureEffectSettings(_effects);

  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    Effects::Id effectId = Effects::toId(i);
    _eeprom.readEffectSettings(effectId, _effects[i]);
  }
}

void SettingsRepository::tick(const Effects::Id currentEffectId) {
  const uint32_t now = millis();
  bool saved = false;
  if (shouldPersistEffectSettings(now)) {
    _eeprom.writeEffectSettings(currentEffectId, getEffectSettings(currentEffectId));
    saved = true;
  }
  if (shouldPersistPalette(now)) {
    _eeprom.writeGlobalPaletteId(_selectedPalette);
    saved = true;
  }
  if (saved) {
    _persistTimer = now;
  }
}

EffectSettings& SettingsRepository::getEffectSettings(Effects::Id effectId) {
  return _effects[Effects::toIndex(Effects::clamp(effectId))];
}

EffectSettings& SettingsRepository::getEffectSettingsByIndex(uint8_t index) {
  return _effects[Effects::toIndex(Effects::clamp(Effects::toId(index)))];
}

const EffectSettings& SettingsRepository::getEffectSettings(Effects::Id effectId) const {
  return _effects[Effects::toIndex(Effects::clamp(effectId))];
}

const EffectSettings& SettingsRepository::getEffectSettingsByIndex(uint8_t index) const {
  return _effects[Effects::toIndex(Effects::clamp(Effects::toId(index)))];
}

void SettingsRepository::resetBootCount() {
  _bootCount = 0;
  _eeprom.writeBootCount(0);
}

void SettingsRepository::markEffectSettingsChanged() {
  _effectSettingsChanged = true;
  _persistTimer = millis();
}

void SettingsRepository::setPalette(Palettes::Id paletteId) {
  paletteId = Palettes::clamp(paletteId);
  if (paletteId == _selectedPalette) return;

  _selectedPalette = paletteId;
  _paletteChanged = true;
  _persistTimer = millis();
}

bool SettingsRepository::resetEffectSettingsToDefaults(const EffectSettings* defaults) {
  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    _effects[i] = defaults[i];
  }

  const bool saved = _eeprom.writeAllEffectSettings(_effects);
  if (saved) {
    _effectSettingsChanged = false;
  }
  return saved;
}

bool SettingsRepository::shouldPersistEffectSettings(uint32_t now) {
  if (_effectSettingsChanged && now - _persistTimer > 30000) {
    _effectSettingsChanged = false;
    return true;
  } else {
    return false;
  }
}

bool SettingsRepository::shouldPersistPalette(uint32_t now) {
  if (_paletteChanged && now - _persistTimer > 30000) {
    _paletteChanged = false;
    return true;
  }
  return false;
}
