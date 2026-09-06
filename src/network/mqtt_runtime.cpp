#include "mqtt_runtime.h"

#ifdef USE_MQTT
#include <HaMqttEntities.h>
#include <WifiController.h>
#include "../util/loop_profiler.h"

MqttRuntime::MqttRuntime(
  HAMQTTController& controller, HAEntity** entityRegistry, size_t entityRegistryCapacity, WifiController& wifi
)
  : controller_(controller),
    entityRegistry_(entityRegistry),
    entityRegistryCapacity_(entityRegistryCapacity),
    wifi_(wifi) {
}

void MqttRuntime::init(const MqttConfig& config, const char* clientId) {
  clientId_ = clientId;
  wifiClient_.setTimeout(kWifiClientTimeoutMs);
  client_.setSocketTimeout(kMqttSocketTimeoutSeconds);
  requestedConfig_ = config;
  requestedConfigGeneration_ = 1;
  requestedEnabled_ = isPersistedConfigEnabled(requestedConfig_);
  bufferReady_ = controller_.begin(client_, entityRegistry_, entityRegistryCapacity_);
}

void MqttRuntime::completeEntityRegistration(bool registered) {
  registered_ = bufferReady_ && registered;
  if (!bufferReady_) {
    Serial.println(F("[MQTT] MQTT buffer allocation failed."));
    setState(MqttState::ConfigError);
  } else if (!registered_) {
    Serial.println(F("[MQTT] MQTT entity registration failed."));
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
        LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { controller_.tick(millis()); });
        barrierPulsed_ = true;
        return;
      }
      completeDisconnectBarrier();
      return;
    case MqttState::WaitingForWifi:
      if (wifi_.staConnected()) {
        resetReconnectBackoff();
        reconnectTiming_ = millis();
        reconnectImmediately_ = true;
        setState(MqttState::RetryWait);
      }
      return;
    case MqttState::RetryWait:
      if (!wifi_.staConnected()) {
        beginDisconnectBarrier(false, true);
      } else if (shouldReconnect(millis())) {
        reconnectImmediately_ = false;
        setState(MqttState::ConnectPrepare);
      }
      return;
    case MqttState::ConnectPrepare:
      LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { controller_.tick(millis()); });
      setState(MqttState::Connecting);
      return;
    case MqttState::Connecting:
      if (!wifi_.staConnected()) beginDisconnectBarrier(false, true);
      else
        connectBlocking();
      return;
    case MqttState::Online:
      if (!wifi_.staConnected()) {
        beginDisconnectBarrier(false, true);
      } else if (!client_.connected()) {
        registerReconnectFailure(millis());
        retryPending_ = true;
        emitTransportFailure();
        beginDisconnectBarrier(false, false);
      } else {
        LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { controller_.tick(millis()); });
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
  emitEvent(Event::StateChanged);
  if (state_ == MqttState::Online) emitEvent(Event::BecameOnline);
}

bool MqttRuntime::consumeEvent(Event event) {
  const uint8_t mask = static_cast<uint8_t>(event);
  if ((pendingEvents_ & mask) == 0) return false;
  pendingEvents_ &= ~mask;
  return true;
}

void MqttRuntime::emitEvent(Event event) {
  pendingEvents_ |= static_cast<uint8_t>(event);
}

void MqttRuntime::emitTransportFailure() {
  emitEvent(Event::TransportFailure);
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
  if (!wifi_.staConnected()) {
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
    setState(wifi_.staConnected() ? MqttState::RetryWait : MqttState::WaitingForWifi);
    return true;
  }
  retryPending_ = false;
  beginDisconnectBarrier(true, false);
  return true;
}

void MqttRuntime::connectBlocking() {
  attemptConfig_ = activeConfig_;
  attemptGeneration_ = requestedGeneration_;
  Serial.printf(
    "[MQTT] Attempting MQTT connection to %s on port %u as %s ...",
    attemptConfig_.host,
    static_cast<unsigned>(attemptConfig_.port),
    attemptConfig_.user
  );
  const uint32_t connectStartedAt = millis();
  const bool connected = controller_.connect(clientId_, attemptConfig_.user, attemptConfig_.password);
  const uint32_t connectDurationMs = millis() - connectStartedAt;
  Serial.printf(" [MQTT] connect blocked for %lu ms\n", static_cast<unsigned long>(connectDurationMs));
  const bool stale = attemptGeneration_ != requestedGeneration_ || !requestedEnabled_ || !wifi_.staConnected();
  if (stale) {
    beginDisconnectBarrier(client_.connected(), !wifi_.staConnected());
    return;
  }
  if (connected) {
    Serial.println(F("[MQTT] connected!"));
    retryPending_ = false;
    resetReconnectBackoff();
    setState(MqttState::Online);
    return;
  }
  registerReconnectFailure(millis());
  retryPending_ = true;
  emitTransportFailure();
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
