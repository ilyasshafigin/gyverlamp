#pragma once

#include <SettingsAsync.h>
#include <OtaController.h>
#include <WifiController.h>
#include "../config.h"
#include "../audio/audio_service.h"
#include "../effect/controller.h"
#include "../hardware/button.h"
#include "../hardware/led.h"
#include "../network/connectivity_coordinator.h"
#include "../network/upd_service.h"
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

#include "../network/mqtt_service.h"

class Lamp {
public:
  EepromStore eeprom;
  Led led;
  TimeService time;
  StateNotifier stateNotifier;
  SettingsAsync webSettings;
  SettingsRepository settings{eeprom};
  RunningText runningText{led, WIDTH};
  AudioService audio{eeprom};
  EffectController effects{audio, eeprom, led, settings, time};
  PowerController power{eeprom, effects, stateNotifier};
  NotificationController notifications{eeprom, power, runningText, stateNotifier, time};
  FrameRenderer frameRenderer{effects, led, notifications, power, stateNotifier};
  RotationController rotation{eeprom, effects, stateNotifier};
  WifiController wifi;
  TouchButton button{eeprom, effects, notifications, power, rotation, settings, stateNotifier, BTN_PIN};
  UpdService upd{effects, power, settings, stateNotifier, time, button, UDP_PORT};
  OtaController ota;
  MqttService mqtt{audio, effects, notifications, power, rotation, settings, button, wifi};
  ConnectivityCoordinator connectivity{eeprom, wifi, ota, mqtt};
  WebService web{
    audio, connectivity, effects, notifications, power, rotation, webSettings, settings, stateNotifier, time, button
  };

  void setup();
  void loop();

private:
  static void onWifiEvent(const WifiController::Event& event, void* context);
  static void onOtaEvent(const OtaController::Event& event, void* context);
};
