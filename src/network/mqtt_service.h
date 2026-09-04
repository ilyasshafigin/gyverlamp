#pragma once

#include <Arduino.h>

#include "mqtt_config.h"
#include "mqtt_state.h"

#ifdef USE_MQTT
#include "lamp_ha_mqtt_bridge.h"
#include "mqtt_runtime.h"
#endif

class AudioService;
class EffectController;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class TouchButton;
class WifiService;

class MqttService {
public:
  using State = MqttState;

  explicit MqttService(
    AudioService& audio,
    EffectController& effects,
    NotificationController& notifications,
    PowerController& power,
    RotationController& rotation,
    SettingsRepository& settings,
    TouchButton& button,
    WifiService& wifi
  );

  MqttService(const MqttService&) = delete;
  MqttService& operator=(const MqttService&) = delete;

  void init(const MqttConfig& config);
  void tick();
  void updateStates();
  void requestApply(const MqttConfig& config);
  void requestEnabled(bool enabled);
  void requestRestart();

  State state() const;
  const char* stateName() const;
  bool isConnected() const { return state() == State::Online; }
  bool isEnabled() const;
  bool isMqttConnected() const { return isConnected(); }

private:
#ifdef USE_MQTT
  LampHaMqttBridge bridge_;
  MqttRuntime runtime_;
#endif
};
