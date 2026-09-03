#pragma once

#include <Arduino.h>
#include <SettingsAsync.h>

#include "../config.h"
#include "../core/rotation_mode.h"
#include "../core/rotation_presets.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"
#include "../effect/ids.h"
#include "../effect/palette_ids.h"

class AudioService;
class EffectController;
class EepromStore;
class MqttService;
class NotificationController;
class OtaService;
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
    OtaService& ota,
    PowerController& power,
    RotationController& rotation,
    SettingsAsync& webSettings,
    SettingsRepository& settings,
    StateNotifier& stateNotifier,
    TimeService& time,
    TouchButton& button,
    WifiService& wifi
  )
    : audio_(audio),
      eeprom_(eeprom),
      effects_(effects),
      mqtt_(mqtt),
      notifications_(notifications),
      ota_(ota),
      power_(power),
      rotation_(rotation),
      webSettings_(webSettings),
      settings_(settings),
      stateNotifier_(stateNotifier),
      time_(time),
      wifi_(wifi),
      button_(button) {
  }

  void init();
  void tick();

private:
  AudioService& audio_;
  EepromStore& eeprom_;
  EffectController& effects_;
  MqttService& mqtt_;
  NotificationController& notifications_;
  OtaService& ota_;
  PowerController& power_;
  RotationController& rotation_;
  SettingsAsync& webSettings_;
  SettingsRepository& settings_;
  StateNotifier& stateNotifier_;
  TimeService& time_;
  WifiService& wifi_;
  TouchButton& button_;

  char inputWifiSsid_[kWifiSsidLen];
  char inputWifiPass_[kWifiPassLen];
  char inputMqttHost_[kMqttHostLen];
  char inputMqttPort_[kMqttPortLen];
  char inputMqttUser_[kMqttUserLen];
  char inputMqttPass_[kMqttPassLen];

  bool powerOn_ = false;
  uint8_t rotationModeIndex_ = 0;
  uint8_t rotationIntervalPresetIndex_ = kRotationPresetDefaultIndex;
  bool buttonEnabled_ = true;
  uint8_t selectedEffectIndex_ = 0;
  uint8_t globalBrightness_ = 0;
  uint8_t brightness_ = 0;
  uint8_t speed_ = 0;
  uint8_t scale_ = 0;
  uint32_t color_ = 0;
  uint16_t autoOffMinutes_ = 0;
  uint8_t selectedPaletteIndex_ = 0;
  bool notificationQuietEnabled_ = false;
  uint32_t notificationQuietStartSeconds_ = 23UL * 60UL * 60UL;
  uint32_t notificationQuietEndSeconds_ = 8UL * 60UL * 60UL;
  String effectOptions_;
  String paletteOptions_;
  String rotationModeOptions_;
  String rotationIntervalOptions_;
  uint8_t audioModeIndex_ = 0;
  uint8_t audioBandIndex_ = 0;
  uint8_t audioAmount_ = 128;
  String audioModeOptions_ = "Off;Brightness;Speed;Scale;Effect";
  String audioBandOptions_ = "Level;Bass;Treble";
  bool otaEnabled_ = false;

#ifdef TEST_NOTIFICATIONS
  uint16_t notificationWarningDurationSec_ = 30;
  uint16_t notificationAlarmDurationSec_ = 30;
  char notificationText_[65] = "Hello";
  uint16_t notificationTextDurationSec_ = 0;
  uint8_t notificationButtonCount_ = 0;
#endif

  void settingsBuilder(sets::Builder& b);
  void settingsUpdate(sets::Updater& u);
  uint8_t effectDisplayIndex(Effects::Id id) const;
  uint8_t paletteDisplayIndex(Palettes::Id id) const;

  void loadNotificationQuietHoursForUi();
  bool saveNotificationQuietHoursFromUi();
};
