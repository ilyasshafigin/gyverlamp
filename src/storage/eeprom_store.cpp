#include <EEPROM.h>
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

  uint16_t readMqttV4Port() {
    uint32_t value = 0;
    bool hasDigit = false;

    for (int i = 0; i < kEepromMqttV4PortTextSize; ++i) {
      const uint8_t raw = EEPROM.read(kEepromMqttV4PortAddr + i);
      if (raw == 0) return hasDigit ? static_cast<uint16_t>(value) : 0;
      if (raw == 0xFF || raw < '0' || raw > '9') return 0;

      const uint8_t digit = raw - '0';
      if (value > (65535U - digit) / 10U) return 0;
      value = value * 10U + digit;
      hasDigit = true;
    }

    return 0;
  }

  MqttConfig readMqttConfigV4() {
    MqttConfig config = {};
    int address = kEepromMqttV4HostAddr;
    readFieldAt(address, config.host, MqttConfig::kMqttHostLen);
    config.port = readMqttV4Port();
    address = kEepromMqttV4UserAddr;
    readFieldAt(address, config.user, MqttConfig::kMqttUserLen);
    address = kEepromMqttV4PasswordAddr;
    readFieldAt(address, config.password, MqttConfig::kMqttPassLen);
    return config;
  }

  WifiConfig readWifiConfigV4() {
    WifiConfig config = {};
    int address = kEepromWifiConfigAddr;
    readFieldAt(address, config.ssid, WifiConfig::kWifiSsidLen);
    readFieldAt(address, config.password, WifiConfig::kWifiPassLen);
    return config;
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

  struct V4MigrationSnapshot {
    WifiConfig wifi;
    MqttConfig mqtt;
    uint16_t autoOffMinutes;
    RotationMode rotationMode;
    uint16_t rotationIntervalSec;
    bool buttonEnabled;
    Palettes::Id paletteId;
    NotificationQuietHours quietHours;
    AudioConfig audio;
    uint8_t currentMode;
  };

  V4MigrationSnapshot readV4MigrationSnapshot() {
    V4MigrationSnapshot snapshot = {};
    snapshot.wifi = readWifiConfigV4();
    snapshot.mqtt = readMqttConfigV4();

    EEPROM.get(kEepromAutoOffMinutesAddr, snapshot.autoOffMinutes);
    if (!isValidAutoOffMinutes(snapshot.autoOffMinutes)) snapshot.autoOffMinutes = kAutoOffMinutesDefault;

    const uint8_t rotationMode = EEPROM.read(kEepromRotationModeAddr);
    snapshot.rotationMode = rotationMode > static_cast<uint8_t>(RotationMode::Random)
                              ? RotationMode::Off
                              : static_cast<RotationMode>(rotationMode);

    EEPROM.get(kEepromRotationIntervalSecAddr, snapshot.rotationIntervalSec);
    if (
      snapshot.rotationIntervalSec < kRotationIntervalSecMin || snapshot.rotationIntervalSec > kRotationIntervalSecMax
    ) {
      snapshot.rotationIntervalSec = kRotationIntervalSecDefault;
    }

    const uint8_t buttonEnabled = EEPROM.read(kEepromButtonEnabledAddr);
    snapshot.buttonEnabled = buttonEnabled != 0 && buttonEnabled != 1 ? true : buttonEnabled == 1;

    snapshot.paletteId = Palettes::clamp(EEPROM.read(kEepromGlobalPaletteIdAddr));

    snapshot.quietHours = defaultNotificationQuietHours();
    const uint8_t quietEnabled = EEPROM.read(kEepromNotificationQuietEnabledAddr);
    uint16_t quietStart = snapshot.quietHours.startMinutes;
    uint16_t quietEnd = snapshot.quietHours.endMinutes;
    EEPROM.get(kEepromNotificationQuietStartAddr, quietStart);
    EEPROM.get(kEepromNotificationQuietEndAddr, quietEnd);
    if ((quietEnabled == 0 || quietEnabled == 1) && isValidMinuteOfDay(quietStart) && isValidMinuteOfDay(quietEnd)) {
      snapshot.quietHours.enabled = quietEnabled == 1;
      snapshot.quietHours.startMinutes = quietStart;
      snapshot.quietHours.endMinutes = quietEnd;
    }

    if (EEPROM.read(kEepromAudioMarkerAddr) == kEepromAudioMarker) {
      snapshot.audio.mode = clampAudioMode(EEPROM.read(kEepromAudioModeAddr));
      snapshot.audio.band = clampAudioBand(EEPROM.read(kEepromAudioBandAddr));
      snapshot.audio.amount = EEPROM.read(kEepromAudioAmountAddr);
    }

    snapshot.currentMode = EEPROM.read(kEepromCurrentModeAddr);
    return snapshot;
  }

  void stageLayoutVersion() {
    EEPROM.put(kEepromLayoutMetaAddr, kEepromLayoutMagic);
    EEPROM.put(kEepromLayoutMetaAddr + sizeof(kEepromLayoutMagic), kEepromLayoutVersionCurrent);
  }

  void stageLayoutInitialization() {
    for (int address = 0; address < kEepromSize; ++address) {
      EEPROM.write(address, 0);
    }

    EEPROM.write(kEepromPowerStateAddr, 0);
    EEPROM.write(kEepromButtonEnabledAddr, 1);
    EEPROM.write(kEepromOtaPolicyAddr, kEepromOtaPolicyDisabled);
    EEPROM.put(kEepromAutoOffMinutesAddr, kAutoOffMinutesDefault);
    EEPROM.write(kEepromCurrentModeAddr, 0);
    EEPROM.write(kEepromEffectSettingsCountAddr, 0);
    EEPROM.write(kEepromGlobalBrightnessAddr, 255);
    EEPROM.write(kEepromRotationModeAddr, static_cast<uint8_t>(RotationMode::Off));
    EEPROM.put(kEepromRotationIntervalSecAddr, kRotationIntervalSecDefault);
  }

  void stageWifiConfig(const WifiConfig& config) {
    int address = kEepromWifiConfigAddr;
    writeFieldAt(address, config.ssid, WifiConfig::kWifiSsidLen);
    writeFieldAt(address, config.password, WifiConfig::kWifiPassLen);
  }

  void stageMqttConfig(const MqttConfig& config) {
    int address = kEepromMqttConfigAddr;
    writeFieldAt(address, config.host, MqttConfig::kMqttHostLen);
    EEPROM.put(address, config.port);
    address += sizeof(config.port);
    writeFieldAt(address, config.user, MqttConfig::kMqttUserLen);
    writeFieldAt(address, config.password, MqttConfig::kMqttPassLen);
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
  if (version == kEepromLayoutVersionV4) {
    if (!migrateLayoutV4ToV5()) {
      Serial.println(F("[EEPROM] Layout v4-to-v5 migration failed"));
    }
    return;
  }

  Serial.println(F("[EEPROM] Unsupported layout version, initializing defaults"));
  if (!initializeLayout()) {
    Serial.println(F("[EEPROM] Layout initialization failed"));
  }
}

bool EepromStore::migrateLayoutV4ToV5() {
  // Snapshot all preserved v4 values before staging any EEPROM changes.
  // Effect settings and global brightness intentionally reset as in prior
  // version migrations.
  Serial.println(F("[EEPROM] Migrating layout v4 to v5"));

  const V4MigrationSnapshot snapshot = readV4MigrationSnapshot();

  stageLayoutInitialization();
  stageWifiConfig(snapshot.wifi);
  stageMqttConfig(snapshot.mqtt);
  EEPROM.put(kEepromAutoOffMinutesAddr, snapshot.autoOffMinutes);
  EEPROM.write(kEepromRotationModeAddr, static_cast<uint8_t>(snapshot.rotationMode));
  EEPROM.put(kEepromRotationIntervalSecAddr, snapshot.rotationIntervalSec);
  EEPROM.write(kEepromButtonEnabledAddr, snapshot.buttonEnabled ? 1 : 0);
  EEPROM.write(kEepromGlobalPaletteIdAddr, static_cast<uint8_t>(snapshot.paletteId));
  EEPROM.write(kEepromNotificationQuietEnabledAddr, snapshot.quietHours.enabled ? 1 : 0);
  EEPROM.put(kEepromNotificationQuietStartAddr, snapshot.quietHours.startMinutes);
  EEPROM.put(kEepromNotificationQuietEndAddr, snapshot.quietHours.endMinutes);
  EEPROM.write(kEepromAudioMarkerAddr, kEepromAudioMarker);
  EEPROM.write(kEepromAudioModeAddr, static_cast<uint8_t>(snapshot.audio.mode));
  EEPROM.write(kEepromAudioBandAddr, static_cast<uint8_t>(snapshot.audio.band));
  EEPROM.write(kEepromAudioAmountAddr, snapshot.audio.amount);
  EEPROM.write(kEepromCurrentModeAddr, snapshot.currentMode);
  stageLayoutVersion();

  if (!EEPROM.commit()) return false;

  wifiConfigCache_ = snapshot.wifi;
  mqttConfigCache_ = snapshot.mqtt;
  return true;
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
  EEPROM.write(kEepromOtaPolicyAddr, kEepromOtaPolicyDisabled);
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
  readFieldAt(eeAddress, wifiConfigCache_.ssid, WifiConfig::kWifiSsidLen);
  readFieldAt(eeAddress, wifiConfigCache_.password, WifiConfig::kWifiPassLen);
  return wifiConfigCache_;
}

bool EepromStore::writeWifiConfig(const char* ssid, const char* password) {
  WifiConfig wifiConfig = {};
  strlcpy(wifiConfig.ssid, ssid, WifiConfig::kWifiSsidLen);
  strlcpy(wifiConfig.password, password, WifiConfig::kWifiPassLen);

  int eeAddress = kEepromWifiConfigAddr;
  writeFieldAt(eeAddress, wifiConfig.ssid, WifiConfig::kWifiSsidLen);
  writeFieldAt(eeAddress, wifiConfig.password, WifiConfig::kWifiPassLen);

  const bool committed = EEPROM.commit();
  if (committed) {
    wifiConfigCache_ = wifiConfig;
  }
  return committed;
}

const MqttConfig& EepromStore::readMqttConfig() {
  int eeAddress = kEepromMqttConfigAddr;
  readFieldAt(eeAddress, mqttConfigCache_.host, MqttConfig::kMqttHostLen);
  EEPROM.get(eeAddress, mqttConfigCache_.port);
  eeAddress += sizeof(mqttConfigCache_.port);
  readFieldAt(eeAddress, mqttConfigCache_.user, MqttConfig::kMqttUserLen);
  readFieldAt(eeAddress, mqttConfigCache_.password, MqttConfig::kMqttPassLen);
  return mqttConfigCache_;
}

bool EepromStore::writeMqttConfig(const char* host, uint16_t port, const char* user, const char* password) {
  MqttConfig mqttConfig = {};
  strlcpy(mqttConfig.host, host, MqttConfig::kMqttHostLen);
  mqttConfig.port = port;
  strlcpy(mqttConfig.user, user, MqttConfig::kMqttUserLen);
  strlcpy(mqttConfig.password, password, MqttConfig::kMqttPassLen);

  int eeAddress = kEepromMqttConfigAddr;
  writeFieldAt(eeAddress, mqttConfig.host, MqttConfig::kMqttHostLen);
  EEPROM.put(eeAddress, mqttConfig.port);
  eeAddress += sizeof(mqttConfig.port);
  writeFieldAt(eeAddress, mqttConfig.user, MqttConfig::kMqttUserLen);
  writeFieldAt(eeAddress, mqttConfig.password, MqttConfig::kMqttPassLen);

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

bool EepromStore::readOtaEnabled() {
  return EEPROM.read(kEepromOtaPolicyAddr) == kEepromOtaPolicyEnabled;
}

bool EepromStore::writeOtaEnabled(bool enabled) {
  const uint8_t value = enabled ? kEepromOtaPolicyEnabled : kEepromOtaPolicyDisabled;
  if (EEPROM.read(kEepromOtaPolicyAddr) == value) return true;

  EEPROM.write(kEepromOtaPolicyAddr, value);
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
