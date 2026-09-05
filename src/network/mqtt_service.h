#pragma once

#include <Arduino.h>

#include "mqtt_config.h"
#include "mqtt_state.h"

#ifdef USE_MQTT
#include <HaMqttEntities.h>

#include "mqtt_runtime.h"
#include "../util/timer.h"
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
  MqttService(MqttService&&) = delete;
  MqttService& operator=(MqttService&&) = delete;

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
  static constexpr unsigned long kTelemetryIntervalMs = 60000;
  static constexpr unsigned long kStateRefreshIntervalMs = 30000;

  AudioService& audio_;
  EffectController& effects_;
  NotificationController& notifications_;
  PowerController& power_;
  RotationController& rotation_;
  SettingsRepository& settings_;
  TouchButton& button_;

  String clientId_;
  String haEffectList_;

  Timer telemetryTimer_{kTelemetryIntervalMs};
  Timer stateRefreshTimer_{kStateRefreshIntervalMs};

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

  HAEntity* entityRegistry_[34] = {};
  HAMQTTController controller_;
  MqttRuntime runtime_;

  void initializeAdapter();
  bool registerEntities();
  void tickTimers();
  void fullRefresh();
  void onTransportState(MqttState state);
  void onTransportFailure();
  static void haCallbackForward(void* context, HAEntity& entity, char* topic, byte* payload, size_t length);
  void handleHaCommand(HAEntity& entity, char* topic, byte* payload, size_t length);
  void consumeRuntimeEvents();
  void telemetryTimerCallback();
  void stateRefreshTimerCallback();
  void syncLightState();
  void syncSelectedEffectState();
  void syncQuietHoursState();
  void syncUserNotificationState();
  void dispatchLightCommand(char* topic);
  void onLightCommand(bool on, uint8_t brightness);
  void onEffectCommand(const char* effectName);
  void onColorCommand(uint8_t r, uint8_t g, uint8_t b);
  void onPaletteCommand(const char* paletteName);
#endif
};
