#pragma once

#include <SettingsAsync.h>
#include "../config.h"
#include "../audio/audio_service.h"
#include "../effect/controller.h"
#include "../hardware/button.h"
#include "../hardware/led.h"
#include "../network/mqtt_service.h"
#include "../network/upd_service.h"
#include "../network/wifi_service.h"
#include "../network/ota_service.h"
#include "../network/web_service.h"
#include "../notification/controller.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "../text/running_text.h"
#include "../time/time_service.h"
#include "../util/loop_profiler.h"
#include "frame_renderer.h"
#include "power_controller.h"
#include "rotation_controller.h"
#include "state_notifier.h"

class Lamp {
public:
  EepromStore eeprom;
  Led led;
  TimeService time;
  StateNotifier stateNotifier;
  SettingsAsync webSettings;
  SettingsRepository settings;
  RunningText runningText;
  AudioService audio;
  EffectController effects;
  PowerController power;
  NotificationController notifications;
  FrameRenderer frameRenderer;
  RotationController rotation;
  WifiService wifi;
  TouchButton button;
  UpdService upd;
  OtaService ota;
  MqttService mqtt;
  WebService web;

  explicit Lamp() :
    eeprom(),
    led(),
    time(),
    stateNotifier(),
    webSettings(),
    settings(eeprom),
    runningText(led, WIDTH),
    audio(eeprom),
    effects(audio, eeprom, led, settings, time),
    power(eeprom, effects, stateNotifier),
    notifications(eeprom, power, runningText, stateNotifier, time),
    frameRenderer(effects, led, notifications, power, stateNotifier),
    rotation(eeprom, effects, stateNotifier),
    wifi(eeprom, frameRenderer, notifications, settings),
    button(eeprom, effects, notifications, power, rotation, settings, stateNotifier, BTN_PIN),
    upd(effects, power, settings, stateNotifier, time, button, UDP_PORT),
    ota(frameRenderer, notifications),
    mqtt(audio, eeprom, effects, notifications, power, rotation, settings, button, wifi),
    web(audio, eeprom, effects, mqtt, notifications, power, rotation, webSettings, settings, stateNotifier, time, button, wifi) {
  }

  void setup();
  void loop();
};
