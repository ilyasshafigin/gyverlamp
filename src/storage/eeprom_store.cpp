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

  constexpr uint16_t MINUTES_PER_DAY = 24 * 60;

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
    if (minutes < AUTO_OFF_MINUTES_MIN) return AUTO_OFF_MINUTES_MIN;
    if (minutes > AUTO_OFF_MINUTES_MAX) return AUTO_OFF_MINUTES_MAX;
    return minutes;
  }

  bool isValidAutoOffMinutes(uint16_t minutes) {
    return minutes >= AUTO_OFF_MINUTES_MIN && minutes <= AUTO_OFF_MINUTES_MAX;
  }

  NotificationQuietHours defaultNotificationQuietHours() {
    NotificationQuietHours settings;
    settings.enabled = DEFAULT_QUIET_ENABLED;
    settings.startMinutes = DEFAULT_QUIET_START_MINUTES;
    settings.endMinutes = DEFAULT_QUIET_END_MINUTES;
    return settings;
  }

  bool isValidMinuteOfDay(uint16_t minutes) {
    return minutes < MINUTES_PER_DAY;
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

}

void EepromStore::init() {
  EEPROM.begin(EEPROM_SIZE);
  ensureLayoutVersion();
}

void EepromStore::ensureLayoutVersion() {
  uint32_t magic = 0;
  uint8_t version = 0;
  EEPROM.get(EEPROM_LAYOUT_META_ADDR, magic);
  EEPROM.get(EEPROM_LAYOUT_META_ADDR + sizeof(magic), version);

  if (magic != EEPROM_LAYOUT_MAGIC) {
    // Legacy EEPROM from pre-versioned firmware is not migrated: start clean with current layout.
    if (!initializeLayout()) {
      Serial.println(F("[EEPROM] Layout initialization failed"));
    }
    return;
  }

  if (version == EEPROM_LAYOUT_VERSION_CURRENT) return;

  Serial.println(F("[EEPROM] Unsupported layout version, resetting EEPROM layout"));
  initializeLayout();
}

bool EepromStore::writeLayoutVersion() {
  EEPROM.put(EEPROM_LAYOUT_META_ADDR, EEPROM_LAYOUT_MAGIC);
  EEPROM.put(EEPROM_LAYOUT_META_ADDR + sizeof(EEPROM_LAYOUT_MAGIC), EEPROM_LAYOUT_VERSION_CURRENT);
  return EEPROM.commit();
}

bool EepromStore::initializeLayout() {
  for (int address = 0; address < EEPROM_SIZE; ++address) {
    EEPROM.write(address, 0);
  }

  EEPROM.write(EEPROM_BOOT_COUNT_ADDR, 0);
  EEPROM.write(EEPROM_POWER_STATE_ADDR, 0);
  EEPROM.write(EEPROM_BUTTON_ENABLED_ADDR, 1);
  EEPROM.put(EEPROM_AUTO_OFF_MINUTES_ADDR, AUTO_OFF_MINUTES_DEFAULT);
  EEPROM.write(EEPROM_CURRENT_MODE_ADDR, 0);
  EEPROM.write(EEPROM_EFFECT_SETTINGS_COUNT_ADDR, 0);
  EEPROM.write(EEPROM_ROTATION_MODE_ADDR, static_cast<uint8_t>(RotationMode::Off));
  EEPROM.put(EEPROM_ROTATION_INTERVAL_SEC_ADDR, ROTATION_INTERVAL_SEC_DEFAULT);
  return writeLayoutVersion();
}

const WifiConfig& EepromStore::readWifiConfig() {
  int eeAddress = EEPROM_WIFI_CONFIG_ADDR;
  readFieldAt(eeAddress, _wifiConfigCache.ssid, WIFI_SSID_LEN);
  readFieldAt(eeAddress, _wifiConfigCache.password, WIFI_PASS_LEN);
  return _wifiConfigCache;
}

bool EepromStore::writeWifiConfig(const char* ssid, const char* password) {
  WifiConfig wifiConfig = {};
  strlcpy(wifiConfig.ssid, ssid, WIFI_SSID_LEN);
  strlcpy(wifiConfig.password, password, WIFI_PASS_LEN);

  int eeAddress = EEPROM_WIFI_CONFIG_ADDR;
  writeFieldAt(eeAddress, wifiConfig.ssid, WIFI_SSID_LEN);
  writeFieldAt(eeAddress, wifiConfig.password, WIFI_PASS_LEN);

  const bool committed = EEPROM.commit();
  if (committed) {
    _wifiConfigCache = wifiConfig;
  }
  return committed;
}

const MqttConfig& EepromStore::readMqttConfig() {
  int eeAddress = EEPROM_MQTT_CONFIG_ADDR;
  readFieldAt(eeAddress, _mqttConfigCache.host, MQTT_HOST_LEN);
  readFieldAt(eeAddress, _mqttConfigCache.port, MQTT_PORT_LEN);
  readFieldAt(eeAddress, _mqttConfigCache.user, MQTT_USER_LEN);
  readFieldAt(eeAddress, _mqttConfigCache.password, MQTT_PASS_LEN);
  return _mqttConfigCache;
}

bool EepromStore::writeMqttConfig(const char* host, const char* port, const char* user, const char* password) {
  MqttConfig mqttConfig = {};
  strlcpy(mqttConfig.host, host, MQTT_HOST_LEN);
  strlcpy(mqttConfig.port, port, MQTT_PORT_LEN);
  strlcpy(mqttConfig.user, user, MQTT_USER_LEN);
  strlcpy(mqttConfig.password, password, MQTT_PASS_LEN);

  int eeAddress = EEPROM_MQTT_CONFIG_ADDR;
  writeFieldAt(eeAddress, mqttConfig.host, MQTT_HOST_LEN);
  writeFieldAt(eeAddress, mqttConfig.port, MQTT_PORT_LEN);
  writeFieldAt(eeAddress, mqttConfig.user, MQTT_USER_LEN);
  writeFieldAt(eeAddress, mqttConfig.password, MQTT_PASS_LEN);

  const bool committed = EEPROM.commit();
  if (committed) {
    _mqttConfigCache = mqttConfig;
  }
  return committed;
}

uint8_t EepromStore::readBootCount() {
  return EEPROM.read(EEPROM_BOOT_COUNT_ADDR);
}

void EepromStore::writeBootCount(uint8_t bootCount) {
  EEPROM.write(EEPROM_BOOT_COUNT_ADDR, bootCount);
  EEPROM.commit();
}

bool EepromStore::readPowerState() {
  return EEPROM.read(EEPROM_POWER_STATE_ADDR);
}

void EepromStore::writePowerState(bool powerOn) {
  EEPROM.write(EEPROM_POWER_STATE_ADDR, static_cast<uint8_t>(powerOn)); EEPROM.commit();
}

uint16_t EepromStore::readAutoOffMinutes() {
  uint16_t minutes = AUTO_OFF_MINUTES_DEFAULT;
  EEPROM.get(EEPROM_AUTO_OFF_MINUTES_ADDR, minutes);
  return isValidAutoOffMinutes(minutes) ? minutes : AUTO_OFF_MINUTES_DEFAULT;
}

bool EepromStore::writeAutoOffMinutes(uint16_t minutes) {
  EEPROM.put(EEPROM_AUTO_OFF_MINUTES_ADDR, clampAutoOffMinutes(minutes));
  return EEPROM.commit();
}

RotationMode EepromStore::readRotationMode() {
  const uint8_t value = EEPROM.read(EEPROM_ROTATION_MODE_ADDR);
  if (value > static_cast<uint8_t>(RotationMode::Random)) {
    writeRotationMode(RotationMode::Off);
    return RotationMode::Off;
  }
  return static_cast<RotationMode>(value);
}

bool EepromStore::writeRotationMode(RotationMode mode) {
  EEPROM.write(EEPROM_ROTATION_MODE_ADDR, static_cast<uint8_t>(mode));
  return EEPROM.commit();
}

uint16_t EepromStore::readRotationIntervalSec() {
  uint16_t seconds = ROTATION_INTERVAL_SEC_DEFAULT;
  EEPROM.get(EEPROM_ROTATION_INTERVAL_SEC_ADDR, seconds);
  if (seconds < ROTATION_INTERVAL_SEC_MIN || seconds > ROTATION_INTERVAL_SEC_MAX) {
    writeRotationIntervalSec(ROTATION_INTERVAL_SEC_DEFAULT);
    return ROTATION_INTERVAL_SEC_DEFAULT;
  }
  return seconds;
}

bool EepromStore::writeRotationIntervalSec(uint16_t seconds) {
  if (seconds < ROTATION_INTERVAL_SEC_MIN) seconds = ROTATION_INTERVAL_SEC_MIN;
  if (seconds > ROTATION_INTERVAL_SEC_MAX) seconds = ROTATION_INTERVAL_SEC_MAX;
  EEPROM.put(EEPROM_ROTATION_INTERVAL_SEC_ADDR, seconds);
  return EEPROM.commit();
}

bool EepromStore::readButtonEnabled() {
  const uint8_t value = EEPROM.read(EEPROM_BUTTON_ENABLED_ADDR);
  if (value != 0 && value != 1) {
    writeButtonEnabled(true);
    return true;
  }
  return value == 1;
}

bool EepromStore::writeButtonEnabled(bool enabled) {
  EEPROM.write(EEPROM_BUTTON_ENABLED_ADDR, enabled ? 1 : 0);
  return EEPROM.commit();
}

Palettes::Id EepromStore::readGlobalPaletteId() {
  const uint8_t value = EEPROM.read(EEPROM_GLOBAL_PALETTE_ID_ADDR);
  return Palettes::clamp(value);
}

bool EepromStore::writeGlobalPaletteId(Palettes::Id paletteId) {
  const uint8_t raw = static_cast<uint8_t>(paletteId);
  if (EEPROM.read(EEPROM_GLOBAL_PALETTE_ID_ADDR) != raw) {
    EEPROM.write(EEPROM_GLOBAL_PALETTE_ID_ADDR, raw);
    return EEPROM.commit();
  }
  return true;
}

NotificationQuietHours EepromStore::readNotificationQuietHours() {
  NotificationQuietHours settings = defaultNotificationQuietHours();

  const uint8_t enabled = EEPROM.read(EEPROM_NOTIFICATION_QUIET_ENABLED_ADDR);
  uint16_t startMinutes = settings.startMinutes;
  uint16_t endMinutes = settings.endMinutes;

  EEPROM.get(EEPROM_NOTIFICATION_QUIET_START_ADDR, startMinutes);
  EEPROM.get(EEPROM_NOTIFICATION_QUIET_END_ADDR, endMinutes);

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
  EEPROM.write(EEPROM_NOTIFICATION_QUIET_ENABLED_ADDR, settings.enabled ? 1 : 0);
  EEPROM.put(EEPROM_NOTIFICATION_QUIET_START_ADDR, clampMinuteOfDay(settings.startMinutes));
  EEPROM.put(EEPROM_NOTIFICATION_QUIET_END_ADDR, clampMinuteOfDay(settings.endMinutes));
  return EEPROM.commit();
}

AudioConfig EepromStore::readAudioConfig() {
  AudioConfig config;

  if (EEPROM.read(EEPROM_AUDIO_MARKER_ADDR) != EEPROM_AUDIO_MARKER) {
    writeAudioConfig(config);
    return config;
  }

  config.mode = clampAudioMode(EEPROM.read(EEPROM_AUDIO_MODE_ADDR));
  config.band = clampAudioBand(EEPROM.read(EEPROM_AUDIO_BAND_ADDR));
  config.amount = EEPROM.read(EEPROM_AUDIO_AMOUNT_ADDR);

  return config;
}

bool EepromStore::writeAudioConfig(const AudioConfig& config) {
  EEPROM.write(EEPROM_AUDIO_MARKER_ADDR, EEPROM_AUDIO_MARKER);
  EEPROM.write(EEPROM_AUDIO_MODE_ADDR, static_cast<uint8_t>(config.mode));
  EEPROM.write(EEPROM_AUDIO_BAND_ADDR, static_cast<uint8_t>(config.band));
  EEPROM.write(EEPROM_AUDIO_AMOUNT_ADDR, config.amount);
  return EEPROM.commit();
}

void EepromStore::ensureEffectSettings(const EffectSettings* effects) {
  uint8_t initializedCount = EEPROM.read(EEPROM_EFFECT_SETTINGS_COUNT_ADDR);
  if (initializedCount == 0xFF || initializedCount > static_cast<uint8_t>(EEPROM_EFFECT_SETTINGS_CAPACITY)) initializedCount = 0;
  if (initializedCount >= Effects::COUNT) return;

  for (uint8_t i = initializedCount; i < Effects::COUNT; i++) {
    EEPROM.put(EEPROM_EFFECT_SETTINGS_ADDR(i), effects[i]);
  }

  EEPROM.write(EEPROM_EFFECT_SETTINGS_COUNT_ADDR, Effects::COUNT);
  EEPROM.commit();
}

void EepromStore::readEffectSettings(const Effects::Id effectId, EffectSettings& effectSettings) {
  EEPROM.get(EEPROM_EFFECT_SETTINGS_ADDR(Effects::toIndex(effectId)), effectSettings);
}

void EepromStore::writeEffectSettings(const Effects::Id effectId, const EffectSettings& effectSettings) {
  uint8_t effectIndex = Effects::toIndex(effectId);
  EEPROM.put(EEPROM_EFFECT_SETTINGS_ADDR(effectIndex), effectSettings);
  if (EEPROM.read(EEPROM_CURRENT_MODE_ADDR) != effectIndex) {
    EEPROM.write(EEPROM_CURRENT_MODE_ADDR, effectIndex);
  }
  EEPROM.commit();
}

bool EepromStore::writeAllEffectSettings(const EffectSettings* effects) {
  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    EEPROM.put(EEPROM_EFFECT_SETTINGS_ADDR(i), effects[i]);
  }
  EEPROM.write(EEPROM_EFFECT_SETTINGS_COUNT_ADDR, Effects::COUNT);
  return EEPROM.commit();
}

Effects::Id EepromStore::readCurrentEffectId() {
  return Effects::toId(EEPROM.read(EEPROM_CURRENT_MODE_ADDR));
}
