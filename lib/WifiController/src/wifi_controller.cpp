#include "WifiController.h"

#include "detail/wifi_platform.h"

#ifdef DEBUG
#include <Arduino.h>
#endif

namespace {
  using wifi_controller::detail::Mode;
  using wifi_controller::detail::PlatformEvent;
  using wifi_controller::detail::StaLinkStatus;

  bool elapsed(uint32_t now, uint32_t startedAt, uint32_t intervalMs) {
    return now - startedAt >= intervalMs;
  }

#ifdef DEBUG
  void logText(const __FlashStringHelper* text) {
    Serial.print(F("[WIFI] "));
    Serial.println(text);
  }

  void logHostname(const __FlashStringHelper* label, const char* hostname) {
    Serial.print(F("[WIFI] "));
    Serial.print(label);
    if (hostname == nullptr) {
      Serial.println(F("(null)"));
    } else {
      Serial.println(hostname);
    }
  }

  void logHostnameOutcome(const __FlashStringHelper* label, bool applied) {
    Serial.print(F("[WIFI] "));
    Serial.print(label);
    Serial.println(applied ? F(" ok") : F(" failed"));
  }

  void logRetry(const __FlashStringHelper* label, uint32_t intervalMs) {
    Serial.print(F("[WIFI] "));
    Serial.print(label);
    Serial.print(F(" retry in "));
    Serial.print(intervalMs);
    Serial.println(F(" ms"));
  }
#endif
} // namespace

bool WifiController::begin(const Config& runtimeConfig, EventHandler eventHandler, void* eventContext) {
  eventHandler_ = eventHandler;
  eventContext_ = eventContext;
  pendingEventCount_ = 0;
  staState_ = State::Provisioning;
  apState_ = ApState::Inactive;
  acceptingStaDisconnectEvents_ = false;
  pendingDisconnectValid_ = false;
  hasStaCredentials_ = false;
  staCampaignActive_ = false;
  fallbackApRequested_ = false;

  if (!copyConfig(runtimeConfig)) {
#ifdef DEBUG
    logHostname(F("config rejected hostname="), runtimeConfig.deviceId);
#endif
    emit(EventType::Error);
    deliverEvents();
    return false;
  }

#ifdef DEBUG
  logHostname(F("config accepted hostname="), config_.deviceId);
#endif
  wifi_controller::detail::platformInitialize();
  wifi_controller::detail::platformSetAutoReconnect(false);

  const bool staHostnameApplied = wifi_controller::detail::platformSetStaHostname(config_.deviceId);
#ifdef DEBUG
  logHostnameOutcome(F("STA hostname setup"), staHostnameApplied);
#endif

  PlatformEvent ignoredEvent{};
  while (wifi_controller::detail::platformNextEvent(ignoredEvent)) {
  }

  hasStaCredentials_ = config_.staSsid[0] != '\0';
  if (!hasStaCredentials_) {
    wifi_controller::detail::platformSetMode(Mode::Ap);
    requestAp();
    emit(EventType::Disabled);
  } else {
    wifi_controller::detail::platformSetMode(Mode::Sta);
    startStaCampaign();
    startStaConnection();
  }

  deliverEvents();
  return true;
}

void WifiController::tick() {
  processPlatformEvents();

  switch (staState_) {
    case State::Provisioning: break;

    case State::Connecting:
      checkStaConnecting();
      checkFallbackAp();
      break;

    case State::Connected:
      if (!staConnected()) {
        stopStaConnection();
        startStaCampaign();
        staState_ = State::RetryWait;
        retryStartedAt_ = wifi_controller::detail::platformMillis();
#ifdef DEBUG
        logText(F("STA link lost"));
        logRetry(F("STA"), config_.staReconnectIntervalMs);
#endif
      }
      break;

    case State::RetryWait:
      checkFallbackAp();
      checkStaRetryWait();
      break;
  }

  if (staState_ != State::Connected && !staConnected()) checkApRetry();
  if (hasStaCredentials_ && staCampaignActive_ && !staConnected()) checkApTimeout();

  wifi_controller::detail::platformTickNetworkServices(config_.deviceId);
  deliverEvents();
}

WifiController::Snapshot WifiController::snapshot() const {
  Snapshot result{};
  wifi_controller::detail::platformSnapshot(result);
  copyString(result.deviceId, sizeof(result.deviceId), config_.deviceId);
  return result;
}

bool WifiController::staConnected() const {
  return wifi_controller::detail::platformStaLinkStatus() == StaLinkStatus::Connected;
}

bool WifiController::copyString(char* destination, uint8_t capacity, const char* source) {
  if (destination == nullptr || source == nullptr || capacity == 0) return false;

  uint8_t index = 0;
  while (source[index]) {
    if (index + 1 >= capacity) {
      destination[0] = '\0';
      return false;
    }
    destination[index] = source[index];
    ++index;
  }
  destination[index] = '\0';
  return true;
}

bool WifiController::isValidHostname(const char* hostname) {
  if (hostname == nullptr) return false;

  uint8_t length = 0;
  while (hostname[length] != '\0') {
    const char character = hostname[length];
    const bool isAlphanumeric = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                                (character >= '0' && character <= '9');
    if (!isAlphanumeric && character != '-') {
      return false;
    }
    ++length;
    if (length > 24) return false;
  }

  if (length == 0) return false;

  const char first = hostname[0];
  const char last = hostname[length - 1];
  return ((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || (first >= '0' && first <= '9')) &&
         ((last >= 'a' && last <= 'z') || (last >= 'A' && last <= 'Z') || (last >= '0' && last <= '9'));
}

bool WifiController::copyConfig(const Config& runtimeConfig) {
  OwnedConfig candidate{};
  if (
    !copyString(candidate.deviceId, sizeof(candidate.deviceId), runtimeConfig.deviceId) ||
    !copyString(candidate.staSsid, sizeof(candidate.staSsid), runtimeConfig.staSsid) ||
    !copyString(candidate.staPassword, sizeof(candidate.staPassword), runtimeConfig.staPassword) ||
    !copyString(candidate.apSsid, sizeof(candidate.apSsid), runtimeConfig.apSsid) ||
    !copyString(candidate.apPassword, sizeof(candidate.apPassword), runtimeConfig.apPassword)
  ) {
    return false;
  }
  if (candidate.apSsid[0] == '\0' || !isValidHostname(candidate.deviceId)) return false;
  if (
    runtimeConfig.staReconnectIntervalMs == 0 || runtimeConfig.apRetryIntervalMs == 0 ||
    runtimeConfig.fallbackApIdleTimeoutMs == 0 || runtimeConfig.staAttemptTimeoutMs == 0 ||
    runtimeConfig.fallbackApDelayMs == 0
  ) {
    return false;
  }

  candidate.apIp = runtimeConfig.apIp;
  candidate.staReconnectIntervalMs = runtimeConfig.staReconnectIntervalMs;
  candidate.apRetryIntervalMs = runtimeConfig.apRetryIntervalMs;
  candidate.fallbackApIdleTimeoutMs = runtimeConfig.fallbackApIdleTimeoutMs;
  candidate.staAttemptTimeoutMs = runtimeConfig.staAttemptTimeoutMs;
  candidate.fallbackApDelayMs = runtimeConfig.fallbackApDelayMs;
  config_ = candidate;
  return true;
}

bool WifiController::isFastFailDisconnectReason(uint16_t reason) {
  return reason == 2 || reason == 4 || reason == 15 || reason == 23 || reason == 201 || reason == 202 ||
         reason == 203 || reason == 204;
}

void WifiController::processPlatformEvents() {
  PlatformEvent event{};
  while (wifi_controller::detail::platformNextEvent(event)) {
#ifdef DEBUG
    Serial.print(F("[WIFI] STA disconnect reason="));
    Serial.println(event.disconnectReason);
#endif
    if (staState_ == State::Connecting && acceptingStaDisconnectEvents_ && !pendingDisconnectValid_) {
      pendingDisconnectValid_ = true;
      pendingDisconnectAttemptId_ = activeAttemptId_;
      pendingDisconnectReason_ = event.disconnectReason;
    }
  }
}

void WifiController::emit(EventType type) {
  if (pendingEventCount_ >= sizeof(pendingEvents_) / sizeof(pendingEvents_[0])) return;
  pendingEvents_[pendingEventCount_++] = type;
}

void WifiController::deliverEvents() {
  if (deliveringEvents_) return;

  deliveringEvents_ = true;
  while (pendingEventCount_ > 0) {
    const EventType type = pendingEvents_[0];
    for (uint8_t index = 1; index < pendingEventCount_; ++index)
      pendingEvents_[index - 1] = pendingEvents_[index];
    --pendingEventCount_;
    if (eventHandler_) eventHandler_(Event{type}, eventContext_);
  }
  deliveringEvents_ = false;
}

bool WifiController::startAp() {
  if (apState_ == ApState::Active) return true;
  if (!wifi_controller::detail::platformStartAp(config_.apSsid, config_.apPassword, config_.apIp)) return false;

  const bool apHostnameApplied = wifi_controller::detail::platformSetApHostname(config_.deviceId);
#ifdef DEBUG
  logHostnameOutcome(F("AP hostname setup"), apHostnameApplied);
#endif

  apState_ = ApState::Active;
  apStartedAt_ = wifi_controller::detail::platformMillis();
  return true;
}

void WifiController::requestAp() {
  if (startAp()) return;

  apState_ = ApState::RetryWait;
  apRetryStartedAt_ = wifi_controller::detail::platformMillis();
#ifdef DEBUG
  logRetry(F("AP start failed;"), config_.apRetryIntervalMs);
#endif
}

void WifiController::stopAp() {
  if (apState_ == ApState::Inactive) return;

  if (apState_ == ApState::RetryWait) {
    apState_ = ApState::Inactive;
    wifi_controller::detail::platformSetMode(hasStaCredentials_ ? Mode::Sta : Mode::Off);
    return;
  }

  apState_ = ApState::Inactive;
#ifdef DEBUG
  logText(F("AP stop"));
#endif
  wifi_controller::detail::platformStopAp();
  wifi_controller::detail::platformSetMode(hasStaCredentials_ ? Mode::Sta : Mode::Off);
}

void WifiController::startStaConnection() {
  if (staState_ == State::Connecting) return;
  if (!hasStaCredentials_) {
    staState_ = State::Provisioning;
    return;
  }

  PlatformEvent ignoredEvent{};
  while (wifi_controller::detail::platformNextEvent(ignoredEvent)) {
  }

  pendingDisconnectValid_ = false;
  activeAttemptId_ = ++nextAttemptId_;
  connectStartedAt_ = wifi_controller::detail::platformMillis();
  staState_ = State::Connecting;
  acceptingStaDisconnectEvents_ = true;
  emit(EventType::Connecting);
#ifdef DEBUG
  Serial.print(F("[WIFI] STA attempt #"));
  Serial.println(activeAttemptId_);
#endif
  const bool staHostnameApplied =
    wifi_controller::detail::platformBeginSta(config_.staSsid, config_.staPassword, config_.deviceId);
#ifdef DEBUG
  logHostnameOutcome(F("STA hostname attempt"), staHostnameApplied);
#endif
}

void WifiController::stopStaConnection() {
  wifi_controller::detail::platformSetAutoReconnect(false);
  wifi_controller::detail::platformDisconnectSta();
}

void WifiController::startStaCampaign() {
  staCampaignActive_ = true;
  fallbackApRequested_ = false;
  staCampaignStartedAt_ = wifi_controller::detail::platformMillis();
}

void WifiController::endStaCampaign() {
  staCampaignActive_ = false;
  stopAp();
}

void WifiController::checkFallbackAp() {
  if (!staCampaignActive_ || fallbackApRequested_ || staConnected()) return;
  const uint32_t now = wifi_controller::detail::platformMillis();
  if (!elapsed(now, staCampaignStartedAt_, config_.fallbackApDelayMs)) return;

  fallbackApRequested_ = true;
  wifi_controller::detail::platformSetMode(Mode::ApSta);
  requestAp();
}

void WifiController::failStaConnection(StaFailureCause cause, uint16_t reason) {
  (void)reason;
  acceptingStaDisconnectEvents_ = false;
  pendingDisconnectValid_ = false;
  staState_ = State::RetryWait;
  retryStartedAt_ = wifi_controller::detail::platformMillis();
  if (cause == StaFailureCause::Deadline) stopStaConnection();
#ifdef DEBUG
  if (cause == StaFailureCause::Deadline) {
    logText(F("STA attempt timeout"));
  } else {
    Serial.print(F("[WIFI] STA attempt failed reason="));
    Serial.println(reason);
  }
  logRetry(F("STA"), config_.staReconnectIntervalMs);
#endif
  emit(EventType::Error);
}

void WifiController::checkStaConnecting() {
  if (staConnected()) {
    acceptingStaDisconnectEvents_ = false;
    pendingDisconnectValid_ = false;
    onStaConnected();
    return;
  }

  if (pendingDisconnectValid_ && pendingDisconnectAttemptId_ == activeAttemptId_) {
    const uint16_t reason = pendingDisconnectReason_;
    pendingDisconnectValid_ = false;
    if (isFastFailDisconnectReason(reason)) {
      failStaConnection(StaFailureCause::DisconnectEvent, reason);
      return;
    }
  }

  const uint32_t now = wifi_controller::detail::platformMillis();
  if (!elapsed(now, connectStartedAt_, config_.staAttemptTimeoutMs)) return;
  failStaConnection(StaFailureCause::Deadline, 0);
}

void WifiController::checkStaRetryWait() {
  if (staConnected()) {
    onStaConnected();
    return;
  }

  const uint32_t now = wifi_controller::detail::platformMillis();
  if (!elapsed(now, retryStartedAt_, config_.staReconnectIntervalMs)) return;
  startStaConnection();
}

void WifiController::onStaConnected() {
  acceptingStaDisconnectEvents_ = false;
  pendingDisconnectValid_ = false;
  staState_ = State::Connected;
  endStaCampaign();
#ifdef DEBUG
  Snapshot snapshot{};
  wifi_controller::detail::platformSnapshot(snapshot);
  Serial.print(F("[WIFI] STA connected IP="));
  Serial.print(snapshot.localIp.octets[0]);
  Serial.print('.');
  Serial.print(snapshot.localIp.octets[1]);
  Serial.print('.');
  Serial.print(snapshot.localIp.octets[2]);
  Serial.print('.');
  Serial.print(snapshot.localIp.octets[3]);
  Serial.print(F(" gateway="));
  Serial.print(snapshot.gateway.octets[0]);
  Serial.print('.');
  Serial.print(snapshot.gateway.octets[1]);
  Serial.print('.');
  Serial.print(snapshot.gateway.octets[2]);
  Serial.print('.');
  Serial.print(snapshot.gateway.octets[3]);
  Serial.print(F(" RSSI="));
  Serial.println(snapshot.rssi);
#endif
  emit(EventType::Connected);
  nextAttemptId_ = 0;
  activeAttemptId_ = 0;
}

void WifiController::checkApRetry() {
  if (apState_ != ApState::RetryWait) return;
  const uint32_t now = wifi_controller::detail::platformMillis();
  if (!elapsed(now, apRetryStartedAt_, config_.apRetryIntervalMs)) return;
#ifdef DEBUG
  logText(F("AP retry"));
#endif
  requestAp();
}

void WifiController::checkApTimeout() {
  if (apState_ != ApState::Active) return;
  const uint32_t now = wifi_controller::detail::platformMillis();
  if (wifi_controller::detail::platformApClientCount() > 0) {
    apStartedAt_ = now;
    return;
  }
  if (!elapsed(now, apStartedAt_, config_.fallbackApIdleTimeoutMs)) return;
  stopAp();
}
