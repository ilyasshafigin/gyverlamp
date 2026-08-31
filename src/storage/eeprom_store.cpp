#include <EEPROM.h>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "../core/auto_off_config.h"
#include "../core/quiet_hours_config.h"
#include "../network/mqtt_config.h"
#include "../effect/palette_catalog.h"
#include "eeprom_layout.h"
#include "eeprom_store.h"

namespace {

  constexpr uint16_t kMinutesPerDay = 24 * 60;

  void readFieldAt(int& address, char* dest, size_t len) {
    size_t i = 0;
    for (; i < len - 1; ++i) {
      uint8_t value = EEPROM.read(address + i);
      if (value == 0 || value == 0xFF) break;
      dest[i] = char(value);
    }
    dest[i] = '\0';
    address += len;
  }

  void writeFieldAt(int& address, const char* src, size_t len) {
    size_t i = 0;
    for (; i < len - 1 && src[i] != '\0'; ++i) {
      EEPROM.write(address + i, src[i]);
    }
    for (; i < len; ++i) {
      EEPROM.write(address + i, 0);
    }
    address += len;
  }

  uint16_t clampAutoOffMinutes(uint16_t minutes) {
    if (minutes < kAutoOffMinutesMin) return kAutoOffMinutesMin;
    if (minutes > kAutoOffMinutesMax) return kAutoOffMinutesMax;
    return minutes;
  }

  bool isValidAutoOffMinutes(uint16_t minutes) {
    return minutes >= kAutoOffMinutesMin && minutes <= kAutoOffMinutesMax;
  }

  NotificationQuietHours defaultNotificationQuietHours() {
    NotificationQuietHours settings;
    settings.enabled = kDefaultQuietEnabled;
    settings.startMinutes = kDefaultQuietStartMinutes;
    settings.endMinutes = kDefaultQuietEndMinutes;
    return settings;
  }

  bool isValidMinuteOfDay(uint16_t minutes) {
    return minutes < kMinutesPerDay;
  }

  uint16_t clampMinuteOfDay(uint16_t minutes) {
    return isValidMinuteOfDay(minutes) ? minutes : 0;
  }

  AudioMode clampAudioMode(uint8_t raw) {
    if (raw > static_cast<uint8_t>(AudioMode::Effect)) return AudioMode::Off;
    return static_cast<AudioMode>(raw);
  }

  AudioBand clampAudioBand(uint8_t raw) {
    if (raw > static_cast<uint8_t>(AudioBand::Treble)) return AudioBand::Level;
    return static_cast<AudioBand>(raw);
  }

} // namespace

void EepromStore::init() {
  EEPROM.begin(kEepromSize);
  ensureLayoutVersion();
}

void EepromStore::ensureLayoutVersion() {
  uint32_t magic = 0;
  uint8_t version = 0;
  EEPROM.get(kEepromLayoutMetaAddr, magic);
  EEPROM.get(kEepromLayoutMetaAddr + sizeof(magic), version);

  if (magic != kEepromLayoutMagic) {
    // Legacy EEPROM from pre-versioned firmware is not migrated: start clean with current layout.
    if (!initializeLayout()) {
      Serial.println(F("[EEPROM] Layout initialization failed"));
    }
    return;
  }

  if (version == kEepromLayoutVersionCurrent) return;

  // Version bump: snapshot user-tunable settings, wipe layout, then restore the
  // snapshot so the device keeps WiFi/MQTT credentials and other config across
  // version bumps. Effect settings are intentionally NOT restored — initializeLayout
  // clears them so fresh effect defaults (brightness=255) apply on next
  // ensureEffectSettings. globalBrightness is left at its new default (255).
  Serial.println(F("[EEPROM] Layout version changed, migrating user settings"));

  const WifiConfig preservedWifi = readWifiConfig();
  const MqttConfig preservedMqtt = readMqttConfig();
  const uint16_t preservedAutoOff = readAutoOffMinutes();
  const RotationMode preservedRotationMode = readRotationMode();
  const uint16_t preservedRotationInterval = readRotationIntervalSec();
  const bool preservedButton = readButtonEnabled();
  const Palettes::Id preservedPalette = readGlobalPaletteId();
  const NotificationQuietHours preservedQuiet = readNotificationQuietHours();
  const AudioConfig preservedAudio = readAudioConfig();
  const uint8_t preservedCurrentMode = EEPROM.read(kEepromCurrentModeAddr);

  initializeLayout();

  writeWifiConfig(preservedWifi.ssid, preservedWifi.password);
  writeMqttConfig(preservedMqtt.host, preservedMqtt.port, preservedMqtt.user, preservedMqtt.password);
  writeAutoOffMinutes(preservedAutoOff);
  writeRotationMode(preservedRotationMode);
  writeRotationIntervalSec(preservedRotationInterval);
  writeButtonEnabled(preservedButton);
  writeGlobalPaletteId(preservedPalette);
  writeNotificationQuietHours(preservedQuiet);
  writeAudioConfig(preservedAudio);
  EEPROM.write(kEepromCurrentModeAddr, preservedCurrentMode);
  EEPROM.commit();
}

bool EepromStore::writeLayoutVersion() {
  EEPROM.put(kEepromLayoutMetaAddr, kEepromLayoutMagic);
  EEPROM.put(kEepromLayoutMetaAddr + sizeof(kEepromLayoutMagic), kEepromLayoutVersionCurrent);
  return EEPROM.commit();
}

bool EepromStore::initializeLayout() {
  for (int address = 0; address < kEepromSize; ++address) {
    EEPROM.write(address, 0);
  }

  EEPROM.write(kEepromPowerStateAddr, 0);
  EEPROM.write(kEepromButtonEnabledAddr, 1);
  EEPROM.put(kEepromAutoOffMinutesAddr, kAutoOffMinutesDefault);
  EEPROM.write(kEepromCurrentModeAddr, 0);
  EEPROM.write(kEepromEffectSettingsCountAddr, 0);
  EEPROM.write(kEepromGlobalBrightnessAddr, 255);
  EEPROM.write(kEepromRotationModeAddr, static_cast<uint8_t>(RotationMode::Off));
  EEPROM.put(kEepromRotationIntervalSecAddr, kRotationIntervalSecDefault);
  return writeLayoutVersion();
}

const WifiConfig& EepromStore::readWifiConfig() {
  int eeAddress = kEepromWifiConfigAddr;
  readFieldAt(eeAddress, wifiConfigCache_.ssid, kWifiSsidLen);
  readFieldAt(eeAddress, wifiConfigCache_.password, kWifiPassLen);
  return wifiConfigCache_;
}

bool EepromStore::writeWifiConfig(const char* ssid, const char* password) {
  WifiConfig wifiConfig = {};
  strlcpy(wifiConfig.ssid, ssid, kWifiSsidLen);
  strlcpy(wifiConfig.password, password, kWifiPassLen);

  int eeAddress = kEepromWifiConfigAddr;
  writeFieldAt(eeAddress, wifiConfig.ssid, kWifiSsidLen);
  writeFieldAt(eeAddress, wifiConfig.password, kWifiPassLen);

  const bool committed = EEPROM.commit();
  if (committed) {
    wifiConfigCache_ = wifiConfig;
  }
  return committed;
}

const MqttConfig& EepromStore::readMqttConfig() {
  int eeAddress = kEepromMqttConfigAddr;
  readFieldAt(eeAddress, mqttConfigCache_.host, kMqttHostLen);
  readFieldAt(eeAddress, mqttConfigCache_.port, kMqttPortLen);
  readFieldAt(eeAddress, mqttConfigCache_.user, kMqttUserLen);
  readFieldAt(eeAddress, mqttConfigCache_.password, kMqttPassLen);
  return mqttConfigCache_;
}

bool EepromStore::writeMqttConfig(const char* host, const char* port, const char* user, const char* password) {
  MqttConfig mqttConfig = {};
  strlcpy(mqttConfig.host, host, kMqttHostLen);
  strlcpy(mqttConfig.port, port, kMqttPortLen);
  strlcpy(mqttConfig.user, user, kMqttUserLen);
  strlcpy(mqttConfig.password, password, kMqttPassLen);

  int eeAddress = kEepromMqttConfigAddr;
  writeFieldAt(eeAddress, mqttConfig.host, kMqttHostLen);
  writeFieldAt(eeAddress, mqttConfig.port, kMqttPortLen);
  writeFieldAt(eeAddress, mqttConfig.user, kMqttUserLen);
  writeFieldAt(eeAddress, mqttConfig.password, kMqttPassLen);

  const bool committed = EEPROM.commit();
  if (committed) {
    mqttConfigCache_ = mqttConfig;
  }
  return committed;
}

bool EepromStore::readPowerState() {
  return EEPROM.read(kEepromPowerStateAddr);
}

void EepromStore::writePowerState(bool powerOn) {
  EEPROM.write(kEepromPowerStateAddr, static_cast<uint8_t>(powerOn));
  EEPROM.commit();
}

uint16_t EepromStore::readAutoOffMinutes() {
  uint16_t minutes = kAutoOffMinutesDefault;
  EEPROM.get(kEepromAutoOffMinutesAddr, minutes);
  return isValidAutoOffMinutes(minutes) ? minutes : kAutoOffMinutesDefault;
}

bool EepromStore::writeAutoOffMinutes(uint16_t minutes) {
  EEPROM.put(kEepromAutoOffMinutesAddr, clampAutoOffMinutes(minutes));
  return EEPROM.commit();
}

RotationMode EepromStore::readRotationMode() {
  const uint8_t value = EEPROM.read(kEepromRotationModeAddr);
  if (value > static_cast<uint8_t>(RotationMode::Random)) {
    writeRotationMode(RotationMode::Off);
    return RotationMode::Off;
  }
  return static_cast<RotationMode>(value);
}

bool EepromStore::writeRotationMode(RotationMode mode) {
  EEPROM.write(kEepromRotationModeAddr, static_cast<uint8_t>(mode));
  return EEPROM.commit();
}

uint16_t EepromStore::readRotationIntervalSec() {
  uint16_t seconds = kRotationIntervalSecDefault;
  EEPROM.get(kEepromRotationIntervalSecAddr, seconds);
  if (seconds < kRotationIntervalSecMin || seconds > kRotationIntervalSecMax) {
    writeRotationIntervalSec(kRotationIntervalSecDefault);
    return kRotationIntervalSecDefault;
  }
  return seconds;
}

bool EepromStore::writeRotationIntervalSec(uint16_t seconds) {
  if (seconds < kRotationIntervalSecMin) seconds = kRotationIntervalSecMin;
  if (seconds > kRotationIntervalSecMax) seconds = kRotationIntervalSecMax;
  EEPROM.put(kEepromRotationIntervalSecAddr, seconds);
  return EEPROM.commit();
}

bool EepromStore::readButtonEnabled() {
  const uint8_t value = EEPROM.read(kEepromButtonEnabledAddr);
  if (value != 0 && value != 1) {
    writeButtonEnabled(true);
    return true;
  }
  return value == 1;
}

bool EepromStore::writeButtonEnabled(bool enabled) {
  EEPROM.write(kEepromButtonEnabledAddr, enabled ? 1 : 0);
  return EEPROM.commit();
}

Palettes::Id EepromStore::readGlobalPaletteId() {
  const uint8_t value = EEPROM.read(kEepromGlobalPaletteIdAddr);
  return Palettes::clamp(value);
}

bool EepromStore::writeGlobalPaletteId(Palettes::Id paletteId) {
  const uint8_t raw = static_cast<uint8_t>(paletteId);
  if (EEPROM.read(kEepromGlobalPaletteIdAddr) != raw) {
    EEPROM.write(kEepromGlobalPaletteIdAddr, raw);
    return EEPROM.commit();
  }
  return true;
}

uint8_t EepromStore::readGlobalBrightness() {
  return EEPROM.read(kEepromGlobalBrightnessAddr);
}

bool EepromStore::writeGlobalBrightness(uint8_t value) {
  if (EEPROM.read(kEepromGlobalBrightnessAddr) != value) {
    EEPROM.write(kEepromGlobalBrightnessAddr, value);
    return EEPROM.commit();
  }
  return true;
}

NotificationQuietHours EepromStore::readNotificationQuietHours() {
  NotificationQuietHours settings = defaultNotificationQuietHours();

  const uint8_t enabled = EEPROM.read(kEepromNotificationQuietEnabledAddr);
  uint16_t startMinutes = settings.startMinutes;
  uint16_t endMinutes = settings.endMinutes;

  EEPROM.get(kEepromNotificationQuietStartAddr, startMinutes);
  EEPROM.get(kEepromNotificationQuietEndAddr, endMinutes);

  if (enabled != 0 && enabled != 1) {
    writeNotificationQuietHours(settings);
    return settings;
  }

  if (!isValidMinuteOfDay(startMinutes) || !isValidMinuteOfDay(endMinutes)) {
    writeNotificationQuietHours(settings);
    return settings;
  }

  settings.enabled = enabled == 1;
  settings.startMinutes = startMinutes;
  settings.endMinutes = endMinutes;
  return settings;
}

bool EepromStore::writeNotificationQuietHours(const NotificationQuietHours& settings) {
  EEPROM.write(kEepromNotificationQuietEnabledAddr, settings.enabled ? 1 : 0);
  EEPROM.put(kEepromNotificationQuietStartAddr, clampMinuteOfDay(settings.startMinutes));
  EEPROM.put(kEepromNotificationQuietEndAddr, clampMinuteOfDay(settings.endMinutes));
  return EEPROM.commit();
}

AudioConfig EepromStore::readAudioConfig() {
  AudioConfig config;

  if (EEPROM.read(kEepromAudioMarkerAddr) != kEepromAudioMarker) {
    writeAudioConfig(config);
    return config;
  }

  config.mode = clampAudioMode(EEPROM.read(kEepromAudioModeAddr));
  config.band = clampAudioBand(EEPROM.read(kEepromAudioBandAddr));
  config.amount = EEPROM.read(kEepromAudioAmountAddr);

  return config;
}

bool EepromStore::writeAudioConfig(const AudioConfig& config) {
  EEPROM.write(kEepromAudioMarkerAddr, kEepromAudioMarker);
  EEPROM.write(kEepromAudioModeAddr, static_cast<uint8_t>(config.mode));
  EEPROM.write(kEepromAudioBandAddr, static_cast<uint8_t>(config.band));
  EEPROM.write(kEepromAudioAmountAddr, config.amount);
  return EEPROM.commit();
}

void EepromStore::ensureEffectSettings(const EffectSettings* effects) {
  uint8_t initializedCount = EEPROM.read(kEepromEffectSettingsCountAddr);
  if (initializedCount == 0xFF || initializedCount > static_cast<uint8_t>(kEepromEffectSettingsCapacity))
    initializedCount = 0;
  if (initializedCount >= Effects::kCount) return;

  for (uint8_t i = initializedCount; i < Effects::kCount; i++) {
    EEPROM.put(kEepromEffectSettingsAddr(i), effects[i]);
  }

  EEPROM.write(kEepromEffectSettingsCountAddr, Effects::kCount);
  EEPROM.commit();
}

void EepromStore::readEffectSettings(const Effects::Id effectId, EffectSettings& effectSettings) {
  EEPROM.get(kEepromEffectSettingsAddr(Effects::toIndex(effectId)), effectSettings);
}

void EepromStore::writeEffectSettings(const Effects::Id effectId, const EffectSettings& effectSettings) {
  uint8_t effectIndex = Effects::toIndex(effectId);
  EEPROM.put(kEepromEffectSettingsAddr(effectIndex), effectSettings);
  if (EEPROM.read(kEepromCurrentModeAddr) != effectIndex) {
    EEPROM.write(kEepromCurrentModeAddr, effectIndex);
  }
  EEPROM.commit();
}

bool EepromStore::writeAllEffectSettings(const EffectSettings* effects) {
  for (uint8_t i = 0; i < Effects::kCount; i++) {
    EEPROM.put(kEepromEffectSettingsAddr(i), effects[i]);
  }
  EEPROM.write(kEepromEffectSettingsCountAddr, Effects::kCount);
  return EEPROM.commit();
}

Effects::Id EepromStore::readCurrentEffectId() {
  return Effects::toId(EEPROM.read(kEepromCurrentModeAddr));
}
