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
  AudioService& _audio;
  EepromStore& _eeprom;
  EffectController& _effects;
  NotificationController& _notifications;
  PowerController& _power;
  RotationController& _rotation;
  SettingsRepository& _settings;
  TouchButton& _button;
  WifiService& _wifi;
  WiFiClient _wifiClient;

  PubSubClient _client;
  Timer _publishTimer;

  char _mqttHost[MQTT_HOST_LEN];
  char _mqttUser[MQTT_USER_LEN];
  char _mqttPassword[MQTT_PASS_LEN];
  char _mqttPort[MQTT_PORT_LEN];

  bool _enabled = true;

  uint32_t _reconnectTiming = 0;
  uint32_t _reconnectTimeout = 5000;
  uint8_t _reconnectCount = 0;

  String _clientId;
  String _haEffectList;

  HADevice _haDevice;
  HALight _haLight;
  HASwitch _haRotationSwitch;
  HASelect _haRotationInterval;
  HASwitch _haButtonSwitch;
  HANumber _haEffectScale;
  HANumber _haEffectSpeed;
  HANumber _haEffectBrightness;
  HANumber _haAutoOff;
  HASensorNumeric _haAutoOffRemaining;
  HASelect _haPalette;
  HASelect _haUserNotification;
  HANumber _haUserNotificationDuration;
  HASensorNumeric _haUserNotificationRemaining;
  HAButton _haUserNotify;
  HAButton _haNextEffect;
  HAButton _haPrevEffect;
  HAButton _haRandomEffect;
  HAText _haUserNotificationText;
  HASwitch _haNotificationQuietHours;
  HATime _haNotificationQuietStart;
  HATime _haNotificationQuietEnd;
  HASensorText _haNotificationMuteState;
  HASelect _haAudioMode;
  HASelect _haAudioBand;
  HANumber _haAudioAmount;
  HASensorText _haAudioAvailable;
  HASensorNumeric _haUptime;
  HASensorNumeric _haRssi;
  HASensorNumeric _haRssiPct;
  HASensorNumeric _haChannel;
  HASensorNumeric _haVcc;
  HASensorNumeric _haBootCount;
  HASensorText _haResetReason;

  void setMqttHost(const char* host) { strlcpy(_mqttHost, host, MQTT_HOST_LEN); }
  void setMqttPort(const char* port) { strlcpy(_mqttPort, port, MQTT_PORT_LEN); }
  void setMqttUser(const char* user) { strlcpy(_mqttUser, user, MQTT_USER_LEN); }
  void setMqttPassword(const char* password) { strlcpy(_mqttPassword, password, MQTT_PASS_LEN); }

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
  bool isMqttConnected() { return _client.connected(); }
#else
  bool isMqttConnected() { return false; }
#endif
};
