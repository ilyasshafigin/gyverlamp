#include "MqttController.h"

#ifdef USE_MQTT
#include <HaMqttEntities.h>
#include <PubSubClient.h>
#include <string.h>

MqttController::MqttController(HAMQTTController& controller, PubSubClient& client, LinkHooks hooks)
  : controller_(controller),
    client_(client),
    hooks_(hooks) {
}

void MqttController::begin(const Config& config, bool prerequisitesReady) {
  prerequisitesReady_ = prerequisitesReady && hooksReady();
  requestedConfigValid_ = copyConfig(requestedConfig_, config);
  requestedEnabled_ = requestedConfigValid_ && requestedConfig_.host[0] != '\0';
  initialized_ = true;
  initialPending_ = true;
  stateReported_ = false;
  retryPending_ = false;
  resetRetryAfterBarrier_ = false;
  reconnectImmediately_ = false;
  requestedGeneration_ = 0;
  handledGeneration_ = 0;
  pendingEvents_ = 0;
}

void MqttController::tick(uint32_t now) {
  if (!initialized_) return;
  if (!prerequisitesReady_) {
    initialPending_ = false;
    setState(State::ConfigError);
    return;
  }
  if (initialPending_) {
    initialPending_ = false;
    if (!prerequisitesReady_ || !requestedConfigValid_) setState(State::ConfigError);
    else if (!requestedEnabled_)
      setState(State::Disabled);
    else
      enterEnabledLifecycle(now);
    return;
  }

  if (state_ == State::DisconnectBarrier) {
    if (barrierPhase_ == BarrierPhase::AwaitPulse) {
      controller_.tick(now);
      barrierPhase_ = BarrierPhase::Complete;
    } else
      completeDisconnectBarrier(now);
    return;
  }

  if (handleRequestedCommands(now)) return;

  switch (state_) {
    case State::Disabled:
    case State::ConfigError: return;
    case State::WaitingForWifi:
      if (hooks_.staConnected(hooks_.context)) {
        resetReconnectBackoff();
        reconnectAt_ = now;
        reconnectImmediately_ = true;
        setState(State::RetryWait);
      }
      return;
    case State::RetryWait:
      if (!hooks_.staConnected(hooks_.context)) startDisconnectBarrier(true);
      else if (reconnectDue(now)) {
        reconnectImmediately_ = false;
        setState(State::ConnectPrepare);
      }
      return;
    case State::ConnectPrepare:
      if (!hooks_.staConnected(hooks_.context)) startDisconnectBarrier(true);
      else {
        controller_.tick(now);
        setState(State::Connecting);
      }
      return;
    case State::Connecting:
      if (!hooks_.staConnected(hooks_.context)) startDisconnectBarrier(true);
      else
        connectBlocking();
      return;
    case State::Online:
      if (!hooks_.staConnected(hooks_.context)) startDisconnectBarrier(true);
      else if (!client_.connected()) {
        registerReconnectFailure(now);
        retryPending_ = true;
        emitEvent(EventType::TransportFailure);
        startDisconnectBarrier(false);
      } else
        controller_.tick(now);
      return;
    case State::DisconnectBarrier: return;
  }
}

void MqttController::requestApply(const Config& config) {
  requestedConfigValid_ = copyConfig(requestedConfig_, config);
  requestedEnabled_ = true;
  resetRetryAfterBarrier_ = true;
  requestedGeneration_++;
}

void MqttController::requestEnabled(bool enabled) {
  if (requestedEnabled_ == enabled) return;
  requestedEnabled_ = enabled;
  if (enabled) resetRetryAfterBarrier_ = true;
  requestedGeneration_++;
}

void MqttController::requestRestart() {
  resetRetryAfterBarrier_ = true;
  requestedGeneration_++;
}

void MqttController::consumeEvents(EventHandler handler, void* context) {
  if (handler == nullptr) return;
  const uint8_t events = pendingEvents_;
  pendingEvents_ = 0;
  if (events & (1 << static_cast<uint8_t>(EventType::TransportFailure)))
    handler(context, Event{EventType::TransportFailure, state_});
  if (events & (1 << static_cast<uint8_t>(EventType::StateChanged)))
    handler(context, Event{EventType::StateChanged, state_});
  if (events & (1 << static_cast<uint8_t>(EventType::BecameOnline)))
    handler(context, Event{EventType::BecameOnline, state_});
}

bool MqttController::copyString(char* destination, size_t capacity, const char* source) {
  if (destination == nullptr || source == nullptr || capacity == 0) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  memcpy(destination, source, length + 1);
  return true;
}

bool MqttController::copyConfig(OwnedConfig& destination, const Config& source) {
  OwnedConfig copy{};
  if (
    !copyString(copy.host, sizeof(copy.host), source.host) ||
    !copyString(copy.clientId, sizeof(copy.clientId), source.clientId) ||
    !copyString(copy.user, sizeof(copy.user), source.user) ||
    !copyString(copy.password, sizeof(copy.password), source.password)
  )
    return false;
  copy.port = source.port;
  destination = copy;
  return true;
}

bool MqttController::hooksReady() const {
  return hooks_.staConnected != nullptr && hooks_.abortTransport != nullptr && hooks_.now != nullptr;
}

bool MqttController::isConfigUsable() const {
  return requestedConfigValid_ && requestedConfig_.host[0] != '\0' && requestedConfig_.port != 0;
}

bool MqttController::applyRequestedConfig() {
  if (!isConfigUsable()) return false;
  activeConfig_ = requestedConfig_;
  client_.setServer(activeConfig_.host, activeConfig_.port);
  return true;
}

void MqttController::setState(State state) {
  if (stateReported_ && state_ == state) return;
  state_ = state;
  stateReported_ = true;
  emitEvent(EventType::StateChanged);
  if (state == State::Online) emitEvent(EventType::BecameOnline);
}

void MqttController::emitEvent(EventType event) {
  pendingEvents_ |= 1 << static_cast<uint8_t>(event);
}

void MqttController::startDisconnectBarrier(bool abortTransport) {
  if (state_ == State::DisconnectBarrier) return;
  if (abortTransport) hooks_.abortTransport(hooks_.context);
  else if (client_.connected())
    client_.disconnect();
  barrierPhase_ = BarrierPhase::AwaitPulse;
  setState(State::DisconnectBarrier);
}

void MqttController::completeDisconnectBarrier(uint32_t now) {
  handledGeneration_ = requestedGeneration_;
  if (resetRetryAfterBarrier_) {
    retryPending_ = false;
    resetRetryAfterBarrier_ = false;
  }
  if (!requestedEnabled_) {
    setState(State::Disabled);
    return;
  }
  if (!applyRequestedConfig()) {
    setState(State::ConfigError);
    return;
  }
  if (!hooks_.staConnected(hooks_.context)) {
    retryPending_ = false;
    setState(State::WaitingForWifi);
    return;
  }
  if (retryPending_) {
    setState(State::RetryWait);
    return;
  }
  resetReconnectBackoff();
  reconnectAt_ = now;
  setState(State::ConnectPrepare);
}

bool MqttController::handleRequestedCommands(uint32_t now) {
  if (handledGeneration_ == requestedGeneration_) return false;
  if (state_ != State::Disabled) {
    startDisconnectBarrier(false);
    return true;
  }
  handledGeneration_ = requestedGeneration_;
  if (resetRetryAfterBarrier_) {
    retryPending_ = false;
    resetRetryAfterBarrier_ = false;
  }
  if (!requestedEnabled_) {
    setState(State::Disabled);
    return true;
  }
  enterEnabledLifecycle(now);
  return true;
}

void MqttController::enterEnabledLifecycle(uint32_t now) {
  if (!applyRequestedConfig()) {
    setState(State::ConfigError);
    return;
  }
  if (!hooks_.staConnected(hooks_.context)) {
    retryPending_ = false;
    setState(State::WaitingForWifi);
    return;
  }
  if (!retryPending_) {
    resetReconnectBackoff();
    reconnectAt_ = now;
    reconnectImmediately_ = true;
  }
  setState(State::RetryWait);
}

void MqttController::connectBlocking() {
  attemptGeneration_ = requestedGeneration_;
  const bool connected = controller_.connect(activeConfig_.clientId, activeConfig_.user, activeConfig_.password);
  const uint32_t now = hooks_.now(hooks_.context);
  const bool stale =
    attemptGeneration_ != requestedGeneration_ || !requestedEnabled_ || !hooks_.staConnected(hooks_.context);
  if (stale) {
    startDisconnectBarrier(!hooks_.staConnected(hooks_.context));
    return;
  }
  if (connected) {
    retryPending_ = false;
    resetReconnectBackoff();
    setState(State::Online);
    return;
  }
  registerReconnectFailure(now);
  retryPending_ = true;
  emitEvent(EventType::TransportFailure);
  startDisconnectBarrier(false);
}

void MqttController::resetReconnectBackoff() {
  reconnectTimeout_ = kReconnectBaseMs;
  reconnectImmediately_ = false;
}

void MqttController::registerReconnectFailure(uint32_t now) {
  reconnectAt_ = now;
  reconnectImmediately_ = false;
  if (!retryPending_ || reconnectTimeout_ < kReconnectBaseMs) reconnectTimeout_ = kReconnectBaseMs;
  else {
    reconnectTimeout_ *= 2;
    if (reconnectTimeout_ > kReconnectMaxMs) reconnectTimeout_ = kReconnectMaxMs;
  }
}

bool MqttController::reconnectDue(uint32_t now) const {
  return reconnectImmediately_ || now - reconnectAt_ >= reconnectTimeout_;
}

#else

MqttController::MqttController(HAMQTTController& controller, PubSubClient& client, LinkHooks hooks)
  : controller_(controller),
    client_(client),
    hooks_(hooks) {
}

void MqttController::begin(const Config&, bool) {
}
void MqttController::tick(uint32_t) {
}
void MqttController::requestApply(const Config&) {
}
void MqttController::requestEnabled(bool) {
}
void MqttController::requestRestart() {
}
void MqttController::consumeEvents(EventHandler, void*) {
}

#endif
