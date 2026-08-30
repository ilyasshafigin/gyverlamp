#pragma once

#include "../effect/ids.h"
#include "../effect/palette_ids.h"
#include "../effect/settings.h"

class EepromStore;

class SettingsRepository {
public:
  explicit SettingsRepository(EepromStore& eeprom)
    : eeprom_(eeprom) {}

  void init();
  void tick(Effects::Id currentEffectId);

  EffectSettings& getEffectSettings(Effects::Id effectId);
  EffectSettings& getEffectSettingsByIndex(uint8_t index);
  const EffectSettings& getEffectSettings(Effects::Id effectId) const;
  const EffectSettings& getEffectSettingsByIndex(uint8_t index) const;

  void markEffectSettingsChanged();
  bool resetEffectSettingsToDefaults(const EffectSettings* defaults);
  void resetEffectSettingsToDefaults(Effects::Id effectId, const EffectSettings& defaults);

  Palettes::Id getSelectedPalette() const { return selectedPalette_; }
  void setPalette(Palettes::Id paletteId);

  uint8_t getGlobalBrightness() const { return globalBrightness_; }
  void setGlobalBrightness(uint8_t value);

private:
  EepromStore& eeprom_;
  EffectSettings effects_[Effects::COUNT];
  bool effectSettingsChanged_ = false;
  uint32_t persistTimer_ = 0;
  Palettes::Id selectedPalette_ = Palettes::Id::Auto;
  bool paletteChanged_ = false;
  uint8_t globalBrightness_ = 255;
  bool globalBrightnessChanged_ = false;

  bool shouldPersistEffectSettings(uint32_t now);
  bool shouldPersistPalette(uint32_t now);
  bool shouldPersistGlobalBrightness(uint32_t now);
};
