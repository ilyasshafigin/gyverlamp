#pragma once

#include "../effect/ids.h"
#include "../effect/palette_ids.h"
#include "../effect/settings.h"

class EepromStore;

class SettingsRepository {
public:
  explicit SettingsRepository(EepromStore& eeprom)
    : _eeprom(eeprom) {}

  void init();
  void tick(Effects::Id currentEffectId);

  EffectSettings& getEffectSettings(Effects::Id effectId);
  EffectSettings& getEffectSettingsByIndex(uint8_t index);
  const EffectSettings& getEffectSettings(Effects::Id effectId) const;
  const EffectSettings& getEffectSettingsByIndex(uint8_t index) const;

  uint8_t getBootCount() const { return _bootCount; }
  void resetBootCount();

  void markEffectSettingsChanged();
  bool resetEffectSettingsToDefaults(const EffectSettings* defaults);

  Palettes::Id getSelectedPalette() const { return _selectedPalette; }
  void setPalette(Palettes::Id paletteId);

  uint8_t getGlobalBrightness() const { return _globalBrightness; }
  void setGlobalBrightness(uint8_t value);

private:
  EepromStore& _eeprom;
  EffectSettings _effects[Effects::COUNT];
  uint8_t _bootCount = 0;
  bool _effectSettingsChanged = false;
  uint32_t _persistTimer = 0;
  Palettes::Id _selectedPalette = Palettes::Id::Auto;
  bool _paletteChanged = false;
  uint8_t _globalBrightness = 255;
  bool _globalBrightnessChanged = false;

  bool shouldPersistEffectSettings(uint32_t now);
  bool shouldPersistPalette(uint32_t now);
  bool shouldPersistGlobalBrightness(uint32_t now);
};
