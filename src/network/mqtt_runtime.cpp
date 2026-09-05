#include "mqtt_runtime.h"

#ifdef USE_MQTT
#include <HaMqttEntities.h>
#include "wifi_service.h"
#include "../util/loop_profiler.h"

namespace {
  void haCallbackForward(HAEntity* entity, char* topic, byte* payload, unsigned int length) {
    MqttRuntime* runtime = MqttRuntime::callbackTarget();
    if (runtime != nullptr) runtime->dispatchCallback(entity, topic, payload, length);
  }
} // namespace

MqttRuntime* MqttRuntime::callbackTarget_ = nullptr;

MqttRuntime::MqttRuntime(MqttBridge& bridge, WifiService& wifi)
  : bridge_(bridge),
    wifi_(wifi) {
}

void MqttRuntime::activateCallbackTarget() {
  callbackTarget_ = this;
}

void MqttRuntime::init(const MqttConfig& config, const char* clientId) {
  activateCallbackTarget();
  clientId_ = clientId;
  wifiClient_.setTimeout(kWifiClientTimeoutMs);
  client_.setSocketTimeout(kMqttSocketTimeoutSeconds);
  requestedConfig_ = config;
  requestedConfigGeneration_ = 1;
  requestedEnabled_ = isPersistedConfigEnabled(requestedConfig_);
  bufferReady_ = client_.setBufferSize(HA_MAX_PAYLOAD_LENGTH);
  if (bufferReady_) {
    HAMQTT.begin(client_, bridge_.entityCount());
    bridge_.registerEntities();
    HAMQTT.setCallback(haCallbackForward);
    registered_ = true;
  }

  if (!bufferReady_) {
    Serial.println(F("[MQTT] MQTT buffer allocation failed."));
    setState(MqttState::ConfigError);
  } else if (!requestedEnabled_) {
    Serial.println(F("[MQTT] MQTT server is disabled."));
    setState(MqttState::Disabled);
  } else if (!activateRequestedConfig()) {
    Serial.println(F("[MQTT] MQTT configuration is invalid."));
    setState(MqttState::ConfigError);
  } else {
    setState(MqttState::ConnectPrepare);
  }
}

void MqttRuntime::tick() {
  if (!bufferReady_ || !registered_) return;
  if (state_ != MqttState::DisconnectBarrier && handleRequestedCommands()) return;

  switch (state_) {
    case MqttState::Disabled:
    case MqttState::ConfigError: return;
    case MqttState::DisconnectBarrier:
      if (!barrierPulsed_) {
        LoopProfiler::measure(LoopProfiler::MQTT_LOOP, []() { HAMQTT.loop(); });
        barrierPulsed_ = true;
        return;
      }
      completeDisconnectBarrier();
      return;
    case MqttState::WaitingForWifi:
      if (wifi_.isStaConnected()) {
        resetReconnectBackoff();
        reconnectTiming_ = millis();
        reconnectImmediately_ = true;
        setState(MqttState::RetryWait);
      }
      return;
    case MqttState::RetryWait:
      if (!wifi_.isStaConnected()) {
        beginDisconnectBarrier(false, true);
      } else if (shouldReconnect(millis())) {
        reconnectImmediately_ = false;
        setState(MqttState::ConnectPrepare);
      }
      return;
    case MqttState::ConnectPrepare:
      LoopProfiler::measure(LoopProfiler::MQTT_LOOP, []() { HAMQTT.loop(); });
      setState(MqttState::Connecting);
      return;
    case MqttState::Connecting:
      if (!wifi_.isStaConnected()) beginDisconnectBarrier(false, true);
      else
        connect();
      return;
    case MqttState::Online:
      if (!wifi_.isStaConnected()) {
        beginDisconnectBarrier(false, true);
      } else if (!client_.connected()) {
        registerReconnectFailure(millis());
        retryPending_ = true;
        bridge_.onTransportFailure();
        beginDisconnectBarrier(false, false);
      } else {
        LoopProfiler::measure(LoopProfiler::MQTT_LOOP, []() { HAMQTT.loop(); });
      }
      return;
  }
}

void MqttRuntime::requestApply(const MqttConfig& config) {
  requestedConfig_ = config;
  requestedConfigGeneration_ += 1;
  requestedEnabled_ = true;
  resetRetryAfterBarrier_ = true;
  requestedGeneration_ += 1;
}

void MqttRuntime::requestEnabled(bool enabled) {
  if (requestedEnabled_ == enabled) return;
  requestedEnabled_ = enabled;
  if (enabled) resetRetryAfterBarrier_ = true;
  requestedGeneration_ += 1;
}

void MqttRuntime::requestRestart() {
  resetRetryAfterBarrier_ = true;
  requestedGeneration_ += 1;
}

void MqttRuntime::dispatchCallback(HAEntity* entity, char* topic, byte* payload, unsigned int length) {
  bridge_.dispatchMessage(client_, entity, topic, payload, length);
}

bool MqttRuntime::isConfigValid(const MqttConfig& config) const {
  return config.host[0] != '\0' && strcmp(config.host, "none") != 0 && config.port != 0;
}

bool MqttRuntime::isPersistedConfigEnabled(const MqttConfig& config) const {
  return config.host[0] != '\0' && strcmp(config.host, "none") != 0;
}

bool MqttRuntime::activateRequestedConfig() {
  if (activeConfigGeneration_ == requestedConfigGeneration_) {
    return isConfigValid(activeConfig_);
  }
  activeConfig_ = requestedConfig_;
  activeConfigGeneration_ = requestedConfigGeneration_;
  if (!isConfigValid(activeConfig_)) return false;
  client_.setServer(activeConfig_.host, activeConfig_.port);
  return true;
}

void MqttRuntime::setState(MqttState state) {
  if (lifecycleStarted_ && state_ == state) return;
  state_ = state;
  lifecycleStarted_ = true;
  bridge_.onTransportState(state_);
}

void MqttRuntime::beginDisconnectBarrier(bool disconnectClient, bool abortTransport) {
  if (state_ == MqttState::DisconnectBarrier) return;
  if (abortTransport) wifiClient_.abort();
  else if (disconnectClient && client_.connected())
    client_.disconnect();
  barrierPulsed_ = false;
  setState(MqttState::DisconnectBarrier);
}

void MqttRuntime::completeDisconnectBarrier() {
  handledGeneration_ = requestedGeneration_;
  if (resetRetryAfterBarrier_) {
    retryPending_ = false;
    resetRetryAfterBarrier_ = false;
  }
  if (!requestedEnabled_) {
    setState(MqttState::Disabled);
    return;
  }
  if (!activateRequestedConfig()) {
    setState(MqttState::ConfigError);
    return;
  }
  if (!wifi_.isStaConnected()) {
    retryPending_ = false;
    setState(MqttState::WaitingForWifi);
    return;
  }
  if (!retryPending_) {
    resetReconnectBackoff();
    reconnectTiming_ = millis();
    reconnectImmediately_ = true;
  }
  setState(MqttState::RetryWait);
}

bool MqttRuntime::handleRequestedCommands() {
  if (handledGeneration_ == requestedGeneration_) return false;
  if (state_ == MqttState::Disabled) {
    handledGeneration_ = requestedGeneration_;
    resetRetryAfterBarrier_ = false;
    if (!requestedEnabled_) return true;
    if (!activateRequestedConfig()) {
      setState(MqttState::ConfigError);
      return true;
    }
    retryPending_ = false;
    resetReconnectBackoff();
    reconnectTiming_ = millis();
    reconnectImmediately_ = true;
    setState(wifi_.isStaConnected() ? MqttState::RetryWait : MqttState::WaitingForWifi);
    return true;
  }
  retryPending_ = false;
  beginDisconnectBarrier(true, false);
  return true;
}

void MqttRuntime::connect() {
  attemptConfig_ = activeConfig_;
  attemptGeneration_ = requestedGeneration_;
  Serial.printf(
    "[MQTT] Attempting MQTT connection to %s on port %u as %s ...",
    attemptConfig_.host,
    static_cast<unsigned>(attemptConfig_.port),
    attemptConfig_.user
  );
  const bool connected = HAMQTT.connect(clientId_, attemptConfig_.user, attemptConfig_.password);
  const bool stale = attemptGeneration_ != requestedGeneration_ || !requestedEnabled_ || !wifi_.isStaConnected();
  if (stale) {
    beginDisconnectBarrier(client_.connected(), !wifi_.isStaConnected());
    return;
  }
  if (connected) {
    Serial.println(F("[MQTT] connected!"));
    retryPending_ = false;
    resetReconnectBackoff();
    setState(MqttState::Online);
    bridge_.fullRefresh();
    return;
  }
  registerReconnectFailure(millis());
  retryPending_ = true;
  bridge_.onTransportFailure();
  Serial.print(F("[MQTT] failed, rc="));
  Serial.print(client_.state());
  Serial.printf(" try again in %d seconds\n", reconnectTimeout_ / 1000);
  beginDisconnectBarrier(false, false);
}

bool MqttRuntime::shouldReconnect(uint32_t now) const {
  return reconnectImmediately_ || now - reconnectTiming_ >= reconnectTimeout_;
}

void MqttRuntime::resetReconnectBackoff() {
  reconnectTimeout_ = kReconnectBaseMs;
  reconnectImmediately_ = false;
}

void MqttRuntime::registerReconnectFailure(uint32_t now) {
  reconnectTiming_ = now;
  reconnectImmediately_ = false;
  if (!retryPending_ || reconnectTimeout_ < kReconnectBaseMs) reconnectTimeout_ = kReconnectBaseMs;
  else {
    reconnectTimeout_ *= 2;
    if (reconnectTimeout_ > kReconnectMaxMs) reconnectTimeout_ = kReconnectMaxMs;
  }
}
#endif
