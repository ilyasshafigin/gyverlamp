#pragma once

#include <Arduino.h>
#include "../audio/audio_config.h"
#include "../core/rotation_mode.h"
#include "../effect/ids.h"
#include "../effect/palette_ids.h"
#include "../effect/settings.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"
#include "../notification/quiet_hours.h"

class EepromStore {
public:
  EepromStore() {};
  void init();

  const WifiConfig& readWifiConfig();
  bool writeWifiConfig(const char* ssid, const char* password);

  const MqttConfig& readMqttConfig();
  bool writeMqttConfig(const char* host, const char* port, const char* user, const char* password);

  uint8_t readBootCount();
  void writeBootCount(uint8_t bootCount);

  bool readPowerState();
  void writePowerState(bool powerOn);

  uint16_t readAutoOffMinutes();
  bool writeAutoOffMinutes(uint16_t minutes);

  bool readButtonEnabled();
  bool writeButtonEnabled(bool enabled);

  RotationMode readRotationMode();
  bool writeRotationMode(RotationMode mode);

  uint16_t readRotationIntervalSec();
  bool writeRotationIntervalSec(uint16_t seconds);

  Palettes::Id readGlobalPaletteId();
  bool writeGlobalPaletteId(Palettes::Id paletteId);

  NotificationQuietHours readNotificationQuietHours();
  bool writeNotificationQuietHours(const NotificationQuietHours& settings);

  AudioConfig readAudioConfig();
  bool writeAudioConfig(const AudioConfig& config);

  Effects::Id readCurrentEffectId();
  void ensureEffectSettings(const EffectSettings* effects);
  void readEffectSettings(const Effects::Id effectId, EffectSettings& settings);
  void writeEffectSettings(const Effects::Id effectId, const EffectSettings& effectSettings);
  bool writeAllEffectSettings(const EffectSettings* effects);

private:
  WifiConfig _wifiConfigCache = {};
  MqttConfig _mqttConfigCache = {};

  void ensureLayoutVersion();
  bool writeLayoutVersion();
  bool initializeLayout();
};
