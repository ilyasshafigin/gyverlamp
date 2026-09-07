#pragma once

#include <Arduino.h>
#include <MqttController.h>

#include "mqtt_config.h"

#ifdef USE_MQTT
static_assert(MqttConfig::kMqttHostLen <= MqttController::kHostCapacity, "MQTT host exceeds controller storage");
static_assert(MqttConfig::kMqttUserLen <= MqttController::kUserCapacity, "MQTT user exceeds controller storage");
static_assert(
  MqttConfig::kMqttPassLen <= MqttController::kPasswordCapacity, "MQTT password exceeds controller storage"
);

#include "../platform/wifi_headers.h"
#include <HaMqttEntities.h>
#include <PubSubClient.h>
#include "../util/timer.h"
#endif

class AudioService;
class EffectController;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class TouchButton;
class WifiController;

class MqttService {
public:
  using State = MqttController::State;

  explicit MqttService(
    AudioService& audio,
    EffectController& effects,
    NotificationController& notifications,
    PowerController& power,
    RotationController& rotation,
    SettingsRepository& settings,
    TouchButton& button,
    WifiController& wifi
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
  static constexpr unsigned long kTelemetryIntervalMs = 60UL * 1000UL;
  static constexpr unsigned long kStateRefreshIntervalMs = 5UL * 60UL * 1000UL;

  AudioService& audio_;
  EffectController& effects_;
  NotificationController& notifications_;
  PowerController& power_;
  RotationController& rotation_;
  SettingsRepository& settings_;
  TouchButton& button_;
  WifiController& wifi_;

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
#if defined(ARDUINO_ARCH_ESP32)
  HASensorText haVcc_;
#else
  HASensorNumeric haVcc_;
#endif
  HASensorText haResetReason_;

  HAEntity* entityRegistry_[34] = {};
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  HAMQTTController controller_;
  MqttController mqttController_;

  void initializeAdapter();
  bool registerEntities();
  void tickTimers();
  void fullRefresh();
  MqttController::Config controllerConfig(const MqttConfig& config) const;
  void onTransportState(State state);
  void onTransportFailure();
  static bool staConnectedForward(void* context);
  static void abortTransportForward(void* context);
  static uint32_t nowForward(void* context);
  static void mqttEventForward(const MqttController::Event& event, void* context);
  static void haCallbackForward(void* context, HAEntity& entity, char* topic, byte* payload, size_t length);
  void handleHaCommand(HAEntity& entity, char* topic, byte* payload, size_t length);
  void consumeControllerEvents();
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
