#pragma once

#ifdef USE_MQTT
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "mqtt_bridge.h"
#include "mqtt_config.h"
#include "mqtt_state.h"

class WifiService;

class MqttRuntime {
public:
  MqttRuntime(MqttBridge& bridge, WifiService& wifi);
  MqttRuntime(const MqttRuntime&) = delete;
  MqttRuntime& operator=(const MqttRuntime&) = delete;

  void init(const MqttConfig& config, const char* clientId);
  void activateCallbackTarget();
  void tick();
  void requestApply(const MqttConfig& config);
  void requestEnabled(bool enabled);
  void requestRestart();

  MqttState state() const { return state_; }
  bool isEnabled() const { return requestedEnabled_; }
  static MqttRuntime* callbackTarget() { return callbackTarget_; }
  void dispatchCallback(HAEntity* entity, char* topic, byte* payload, unsigned int length);

private:
  static constexpr uint16_t kWifiClientTimeoutMs = 2000;
  static constexpr uint16_t kMqttSocketTimeoutSeconds = 2;
  static constexpr uint32_t kReconnectBaseMs = 5000;
  static constexpr uint32_t kReconnectMaxMs = 60000;
  static MqttRuntime* callbackTarget_;

  MqttBridge& bridge_;
  WifiService& wifi_;
  const char* clientId_ = nullptr;
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  MqttState state_ = MqttState::Disabled;
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

  bool isConfigValid(const MqttConfig& config, uint16_t& port) const;
  bool isPersistedConfigEnabled(const MqttConfig& config) const;
  bool activateRequestedConfig();
  void setState(MqttState state);
  void beginDisconnectBarrier(bool disconnectClient, bool abortTransport);
  void completeDisconnectBarrier();
  bool handleRequestedCommands();
  void connect();
  bool shouldReconnect(uint32_t now) const;
  void resetReconnectBackoff();
  void registerReconnectFailure(uint32_t now);
};
#endif
