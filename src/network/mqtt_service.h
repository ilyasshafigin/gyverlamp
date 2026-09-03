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
public:
  enum class State : uint8_t {
    Disabled,
    WaitingForWifi,
    DisconnectBarrier,
    RetryWait,
    ConnectPrepare,
    Connecting,
    Online,
    ConfigError,
  };

private:
  State state_ = State::Disabled;

#ifdef USE_MQTT
  static constexpr uint16_t kWifiClientTimeoutMs = 2000;
  static constexpr uint16_t kMqttSocketTimeoutSeconds = 2;
  static constexpr uint32_t kReconnectBaseMs = 5000;
  static constexpr uint32_t kReconnectMaxMs = 60000;

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
  Timer telemetryTimer_;
  Timer stateRefreshTimer_;

  MqttConfig requestedConfig_{};
  MqttConfig activeConfig_{};
  MqttConfig attemptConfig_{};

  bool requestedEnabled_ = false;
  bool bufferReady_ = true;
  bool registered_ = false;
  bool lifecycleStarted_ = false;
  bool barrierPulsed_ = false;
  bool retryPending_ = false;
  bool resetRetryAfterBarrier_ = false;
  bool reconnectImmediately_ = false;

  uint32_t requestedGeneration_ = 0;
  uint32_t handledGeneration_ = 0;
  uint32_t requestedConfigGeneration_ = 0;
  uint32_t activeConfigGeneration_ = 0;
  uint32_t attemptGeneration_ = 0;

  uint32_t reconnectTiming_ = 0;
  uint32_t reconnectTimeout_ = kReconnectBaseMs;

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

  bool isConfigValid(const MqttConfig& config, uint16_t& port) const;
  bool isPersistedConfigEnabled(const MqttConfig& config) const;
  bool activateRequestedConfig();
  void setState(State state);
  void beginDisconnectBarrier(bool disconnectClient, bool abortTransport);
  void completeDisconnectBarrier();
  bool handleRequestedCommands();
  void connect();
  bool shouldReconnect(uint32_t now) const;
  void resetReconnectBackoff();
  void registerReconnectFailure(uint32_t now);

  void telemetryTimerCallback();
  void stateRefreshTimerCallback();
  void syncLightState();
  void syncSelectedEffectState();
  void syncQuietHoursState();
  void syncUserNotificationState();
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

  void requestApply(const MqttConfig& config);
  void requestEnabled(bool enabled);
  void requestRestart();

  State state() const { return state_; }
  const char* stateName() const;
  bool isConnected() const { return state_ == State::Online; }

#ifdef USE_MQTT
  void haCallback(HAEntity* entity, char* topic, byte* payload, unsigned int length);
  bool isEnabled() const { return requestedEnabled_; }
  bool isMqttConnected() const { return isConnected(); }
#else
  bool isEnabled() const { return false; }
  bool isMqttConnected() const { return false; }
#endif
};
