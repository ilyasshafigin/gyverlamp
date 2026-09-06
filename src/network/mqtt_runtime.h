#pragma once

#ifdef USE_MQTT
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <HaMqttEntities.h>
#include <PubSubClient.h>
#include <stddef.h>

#include "mqtt_config.h"
#include "mqtt_state.h"

class WifiController;

class MqttRuntime {
public:
  enum class Event : uint8_t {
    StateChanged = 1 << 0,
    BecameOnline = 1 << 1,
    TransportFailure = 1 << 2,
  };

  MqttRuntime(
    HAMQTTController& controller, HAEntity** entityRegistry, size_t entityRegistryCapacity, WifiController& wifi
  );
  MqttRuntime(const MqttRuntime&) = delete;
  MqttRuntime& operator=(const MqttRuntime&) = delete;

  void init(const MqttConfig& config, const char* clientId);
  void completeEntityRegistration(bool registered);
  void tick();
  void requestApply(const MqttConfig& config);
  void requestEnabled(bool enabled);
  void requestRestart();

  MqttState state() const { return state_; }
  bool isEnabled() const { return requestedEnabled_; }
  bool isControllerReady() const { return bufferReady_; }
  bool consumeEvent(Event event);
  PubSubClient& client() { return client_; }

private:
  static constexpr uint16_t kWifiClientTimeoutMs = 2000;
  static constexpr uint16_t kMqttSocketTimeoutSeconds = 2;
  static constexpr uint32_t kReconnectBaseMs = 5000;
  static constexpr uint32_t kReconnectMaxMs = 60000;

  HAMQTTController& controller_;
  HAEntity** entityRegistry_;
  size_t entityRegistryCapacity_;
  WifiController& wifi_;
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
  uint8_t pendingEvents_ = 0;

  bool isConfigValid(const MqttConfig& config) const;
  bool isPersistedConfigEnabled(const MqttConfig& config) const;
  bool activateRequestedConfig();
  void setState(MqttState state);
  void emitEvent(Event event);
  void emitTransportFailure();
  void beginDisconnectBarrier(bool disconnectClient, bool abortTransport);
  void completeDisconnectBarrier();
  bool handleRequestedCommands();
  void connectBlocking();
  bool shouldReconnect(uint32_t now) const;
  void resetReconnectBackoff();
  void registerReconnectFailure(uint32_t now);
};
#endif
