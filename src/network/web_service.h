#pragma once

#include <Arduino.h>
#include <SettingsAsync.h>

#include "../config.h"
#include "../core/rotation_mode.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"
#include "../effect/ids.h"
#include "../effect/palette_ids.h"

class AudioService;
class EffectController;
class EepromStore;
class MqttService;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class StateNotifier;
class TimeService;
class TouchButton;
class WifiService;

class WebService {
public:
  explicit WebService(
    AudioService& audio,
    EepromStore& eeprom,
    EffectController& effects,
    MqttService& mqtt,
    NotificationController& notifications,
    PowerController& power,
    RotationController& rotation,
    SettingsAsync& webSettings,
    SettingsRepository& settings,
    StateNotifier& stateNotifier,
    TimeService& time,
    TouchButton& button,
    WifiService& wifi
  ) :
    _audio(audio),
    _eeprom(eeprom),
    _effects(effects),
    _mqtt(mqtt),
    _notifications(notifications),
    _power(power),
    _rotation(rotation),
    _webSettings(webSettings),
    _settings(settings),
    _stateNotifier(stateNotifier),
    _time(time),
    _wifi(wifi),
    _button(button) {
  }

  void init();
  void tick();

private:
  AudioService& _audio;
  EepromStore& _eeprom;
  EffectController& _effects;
  MqttService& _mqtt;
  NotificationController& _notifications;
  PowerController& _power;
  RotationController& _rotation;
  SettingsAsync& _webSettings;
  SettingsRepository& _settings;
  StateNotifier& _stateNotifier;
  TimeService& _time;
  WifiService& _wifi;
  TouchButton& _button;

  char _inputWifiSsid[WIFI_SSID_LEN];
  char _inputWifiPass[WIFI_PASS_LEN];
  char _inputMqttHost[MQTT_HOST_LEN];
  char _inputMqttPort[MQTT_PORT_LEN];
  char _inputMqttUser[MQTT_USER_LEN];
  char _inputMqttPass[MQTT_PASS_LEN];

  bool _powerOn = false;
  uint8_t _rotationModeIndex = 0;
  uint16_t _rotationIntervalMin = ROTATION_INTERVAL_MIN_DEFAULT;
  bool _buttonEnabled = true;
  uint8_t _selectedEffectIndex = 0;
  uint8_t _brightness = 0;
  uint8_t _speed = 0;
  uint8_t _scale = 0;
  uint32_t _color = 0;
  uint16_t _autoOffMinutes = 0;
  uint8_t _selectedPaletteIndex = 0;
  bool _notificationQuietEnabled = false;
  uint32_t _notificationQuietStartSeconds = 23UL * 60UL * 60UL;
  uint32_t _notificationQuietEndSeconds = 8UL * 60UL * 60UL;
  String _effectOptions;
  String _paletteOptions;
  String _rotationModeOptions;
  uint8_t _audioModeIndex = 0;
  uint8_t _audioBandIndex = 0;
  uint8_t _audioAmount = 128;
  String _audioModeOptions = "Off;Brightness;Speed;Scale;Effect";
  String _audioBandOptions = "Level;Bass;Treble";

#ifdef TEST_NOTIFICATIONS
  uint16_t _notificationWarningDurationSec = 30;
  uint16_t _notificationAlarmDurationSec = 30;
  char _notificationText[65] = "Hello";
  uint16_t _notificationTextDurationSec = 0;
  uint8_t _notificationButtonCount = 0;
#endif

  void settingsBuilder(sets::Builder& b);
  void settingsUpdate(sets::Updater& u);
  uint8_t effectDisplayIndex(Effects::Id id) const;
  uint8_t paletteDisplayIndex(Palettes::Id id) const;

  void loadNotificationQuietHoursForUi();
  bool saveNotificationQuietHoursFromUi();
};
