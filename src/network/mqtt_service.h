#pragma once

#include <Arduino.h>
#ifdef USE_MQTT
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <HaMqttEntities.h>
#include "ha_light.h"
#include "ha_time.h"
#endif

#include "../util/timer.h"
#include "mqtt_config.h"

class AudioService;
class EepromStore;
class EffectController;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class TouchButton;
class WifiService;

class MqttService {
private:
#ifdef USE_MQTT
  static constexpr uint16_t kWifiClientTimeoutMs = 2000;
  static constexpr uint16_t kMqttSocketTimeoutSeconds = 2;

  AudioService& audio_;
  EepromStore& eeprom_;
  EffectController& effects_;
  NotificationController& notifications_;
  PowerController& power_;
  RotationController& rotation_;
  SettingsRepository& settings_;
  TouchButton& button_;
  WifiService& wifi_;
  WiFiClient wifiClient_;

  PubSubClient client_;
  Timer publishTimer_;

  char mqttHost_[kMqttHostLen];
  char mqttUser_[kMqttUserLen];
  char mqttPassword_[kMqttPassLen];
  char mqttPort_[kMqttPortLen];

  bool enabled_ = true;

  uint32_t reconnectTiming_ = 0;
  uint32_t reconnectTimeout_ = 5000;
  uint8_t reconnectCount_ = 0;

  String clientId_;
  String haEffectList_;

  HADevice haDevice_;
  HALight haLight_;
  HASwitch haRotationSwitch_;
  HASelect haRotationInterval_;
  HASwitch haButtonSwitch_;
  HANumber haEffectScale_;
  HANumber haEffectSpeed_;
  HANumber haEffectBrightness_;
  HANumber haAutoOff_;
  HASensorNumeric haAutoOffRemaining_;
  HASelect haPalette_;
  HASelect haUserNotification_;
  HANumber haUserNotificationDuration_;
  HASensorNumeric haUserNotificationRemaining_;
  HAButton haUserNotify_;
  HAButton haNextEffect_;
  HAButton haPrevEffect_;
  HAButton haRandomEffect_;
  HAButton haResetAllEffectSettings_;
  HAButton haResetCurrentEffectSettings_;
  HAText haUserNotificationText_;
  HASwitch haNotificationQuietHours_;
  HATime haNotificationQuietStart_;
  HATime haNotificationQuietEnd_;
  HASensorText haNotificationMuteState_;
  HASelect haAudioMode_;
  HASelect haAudioBand_;
  HANumber haAudioAmount_;
  HASensorText haAudioAvailable_;
  HASensorNumeric haUptime_;
  HASensorNumeric haRssi_;
  HASensorNumeric haRssiPct_;
  HASensorNumeric haChannel_;
  HASensorNumeric haVcc_;
  HASensorText haResetReason_;

  void setMqttHost(const char* host) { strlcpy(mqttHost_, host, kMqttHostLen); }
  void setMqttPort(const char* port) { strlcpy(mqttPort_, port, kMqttPortLen); }
  void setMqttUser(const char* user) { strlcpy(mqttUser_, user, kMqttUserLen); }
  void setMqttPassword(const char* password) { strlcpy(mqttPassword_, password, kMqttPassLen); }

  void reconnect();

  bool shouldReconnect(uint32_t now) const;
  void resetReconnectBackoff();
  void registerReconnectFailure(uint32_t now);
  bool shouldDisableAfterReconnectFailures() const;

  void publishTimerCallback();
  void onLightCommand(bool on, uint8_t brightness);
  void onEffectCommand(const char* effectName);
  void onColorCommand(uint8_t r, uint8_t g, uint8_t b);
  void onPaletteCommand(const char* paletteName);
#endif

public:
  explicit MqttService(
    AudioService& audio,
    EepromStore& eeprom,
    EffectController& effects,
    NotificationController& notifications,
    PowerController& power,
    RotationController& rotation,
    SettingsRepository& settings,
    TouchButton& button,
    WifiService& wifi
  );

  void init();
  void tick();
  void updateStates();

#ifdef USE_MQTT
  void haCallback(HAEntity* entity, char* topic, byte* payload, unsigned int length);
  bool isMqttConnected() { return client_.connected(); }
#else
  bool isMqttConnected() { return false; }
#endif
};
