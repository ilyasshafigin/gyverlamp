#include "wifi_service.h"
#include "../config.h"
#include "../storage/eeprom_store.h"

void WifiService::init() {
  if (!wifiEventsRegistered_) {
    stationConnectedEventHandler_ = WiFi.onStationModeConnected([](const WiFiEventStationModeConnected& event) {
      Serial.printf(
        "[WIFI][%lu] EVENT STA_CONNECTED ssid=%s channel=%u\n", millis(), event.ssid.c_str(), event.channel
      );
    });
    stationDisconnectedEventHandler_ =
      WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& event) {
        Serial.printf(
          "[WIFI][%lu] EVENT STA_DISCONNECTED reason=%u\n", millis(), static_cast<unsigned int>(event.reason)
        );

        if (staState_ == StaState::Connecting && acceptingStaDisconnectEvents_ && !pendingDisconnectValid_) {
          pendingDisconnectValid_ = true;
          pendingDisconnectAttemptId_ = activeAttemptId_;
          pendingDisconnectReason_ = static_cast<uint16_t>(event.reason);
        }
      });
    stationGotIpEventHandler_ = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
      Serial.printf(
        "[WIFI][%lu] EVENT STA_GOT_IP ip=%s mask=%s gateway=%s\n",
        millis(),
        event.ip.toString().c_str(),
        event.mask.toString().c_str(),
        event.gw.toString().c_str()
      );
    });
    stationDhcpTimeoutEventHandler_ =
      WiFi.onStationModeDHCPTimeout([]() { Serial.printf("[WIFI][%lu] EVENT STA_DHCP_TIMEOUT\n", millis()); });
    wifiEventsRegistered_ = true;
  }

  WiFi.setAutoReconnect(false);

  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  hasStaCredentials_ = strlen(wifiConfig.ssid) > 0;
  if (!hasStaCredentials_) {
    WiFi.mode(WIFI_AP);
    requestAp();
    staState_ = StaState::Provisioning;

    if (disabledHandler_) disabledHandler_();

    Serial.println("[WIFI] No STA config, AP open for setup");
  } else {
    WiFi.mode(WIFI_STA);
    startStaCampaign();
    startStaConnection();
  }
}

void WifiService::tick() {
  switch (staState_) {
    case StaState::Provisioning: break;

    case StaState::Connecting:
      checkStaConnecting();
      checkFallbackAp();
      break;

    case StaState::Connected:
      if (!isStaConnected()) {
        stopStaConnection();
        startStaCampaign();
        staState_ = StaState::RetryWait;
        retryStartedAt_ = millis();
      }
      break;

    case StaState::RetryWait:
      checkFallbackAp();
      checkStaRetryWait();
      break;
  }

  if (staState_ != StaState::Connected && !isStaConnected()) {
    checkApRetry();
  }

  if (hasStaCredentials_ && staCampaignActive_ && !isStaConnected()) {
    checkApTimeout();
  }
}

bool WifiService::startAp() {
  if (apState_ == ApState::Active) return true;

  const uint8_t ipAp[4] = AP_IP;
  const IPAddress apIp(ipAp[0], ipAp[1], ipAp[2], ipAp[3]);
  if (!WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0))) {
    Serial.println("[WIFI] Failed to configure AP");
    return false;
  }

  if (!WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.println("[WIFI] Failed to start AP");
    return false;
  }

  apState_ = ApState::Active;
  apStartedAt_ = millis();

  Serial.println("[WIFI] Access point mode");
  Serial.print("[WIFI] AP IP: ");
  Serial.println(WiFi.softAPIP());
  return true;
}

void WifiService::requestAp() {
  if (startAp()) {
    return;
  }

  apState_ = ApState::RetryWait;
  apRetryStartedAt_ = millis();
}

void WifiService::stopAp() {
  if (apState_ == ApState::Inactive) return;

  if (apState_ == ApState::RetryWait) {
    apState_ = ApState::Inactive;
    WiFi.mode(hasStaCredentials_ ? WIFI_STA : WIFI_OFF);
    return;
  }

  apState_ = ApState::Inactive;
  WiFi.softAPdisconnect(true);
  WiFi.mode(hasStaCredentials_ ? WIFI_STA : WIFI_OFF);

  Serial.println("[WIFI] Access point stopped");
}

void WifiService::startStaConnection() {
  if (staState_ == StaState::Connecting) return;

  if (!hasStaCredentials_) {
    staState_ = StaState::Provisioning;
    return;
  }

  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();

  if (connectingHandler_) connectingHandler_();

  pendingDisconnectValid_ = false;
  activeAttemptId_ = ++nextAttemptId_;
  connectStartedAt_ = millis();
  staState_ = StaState::Connecting;
  acceptingStaDisconnectEvents_ = true;
  Serial.printf(
    "[WIFI][%lu] STA_BEGIN attempt=%lu mode=%u status=%d ap_clients=%u\n",
    millis(),
    static_cast<unsigned long>(activeAttemptId_),
    static_cast<unsigned int>(WiFi.getMode()),
    static_cast<int>(WiFi.status()),
    WiFi.softAPgetStationNum()
  );
  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  Serial.println("[WIFI] Connecting to STA");
}

void WifiService::stopStaConnection() {
  // Keep saved SDK credentials and STA mode intact while RetryWait owns reconnects.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
}

void WifiService::startStaCampaign() {
  staCampaignActive_ = true;
  fallbackApRequested_ = false;
  staCampaignStartedAt_ = millis();
}

void WifiService::endStaCampaign() {
  staCampaignActive_ = false;
  stopAp();
}

void WifiService::checkFallbackAp() {
  if (!staCampaignActive_ || fallbackApRequested_ || isStaConnected()) return;
  if (millis() - staCampaignStartedAt_ < kFallbackApDelayMs) return;

  fallbackApRequested_ = true;
  WiFi.mode(WIFI_AP_STA);
  requestAp();
  Serial.println("[WIFI] STA campaign fallback AP requested");
}

void WifiService::failStaConnection(StaFailureCause cause, wl_status_t status, uint16_t reason) {
  const uint32_t elapsedMs = millis() - connectStartedAt_;
  acceptingStaDisconnectEvents_ = false;
  pendingDisconnectValid_ = false;
  staState_ = StaState::RetryWait;
  retryStartedAt_ = millis();

  if (cause == StaFailureCause::Deadline) {
    stopStaConnection();
  }

  Serial.printf(
    "[WIFI][%lu] STA_FAILURE attempt=%lu cause=%s reason=%u status=%d elapsed=%lums\n",
    millis(),
    static_cast<unsigned long>(activeAttemptId_),
    cause == StaFailureCause::DisconnectEvent ? "disconnect_event" : "deadline",
    static_cast<unsigned int>(reason),
    static_cast<int>(status),
    static_cast<unsigned long>(elapsedMs)
  );

  if (errorHandler_) errorHandler_();

  Serial.println("[WIFI] STA connection failed");
}

void WifiService::checkStaConnecting() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    acceptingStaDisconnectEvents_ = false;
    pendingDisconnectValid_ = false;
    onStaConnected();
    return;
  }

  if (pendingDisconnectValid_ && pendingDisconnectAttemptId_ == activeAttemptId_) {
    const uint16_t reason = pendingDisconnectReason_;
    pendingDisconnectValid_ = false;
    if (
      reason == 2 || reason == 4 || reason == 15 || reason == 23 || reason == 201 || reason == 202 || reason == 203 ||
      reason == 204
    ) {
      failStaConnection(StaFailureCause::DisconnectEvent, status, reason);
      return;
    }
  }

  if (millis() - connectStartedAt_ < kStaAttemptTimeoutMs) return;
  failStaConnection(StaFailureCause::Deadline, status, 0);
}

void WifiService::checkStaRetryWait() {
  if (isStaConnected()) {
    onStaConnected();
    return;
  }

  if (millis() - retryStartedAt_ < kReconnectIntervalMs) return;
  startStaConnection();
}

void WifiService::onStaConnected() {
  acceptingStaDisconnectEvents_ = false;
  pendingDisconnectValid_ = false;
  staState_ = StaState::Connected;
  endStaCampaign();

  if (connectedHandler_) connectedHandler_();

  Serial.print("[WIFI] STA IP: ");
  Serial.println(WiFi.localIP());
  nextAttemptId_ = 0;
  activeAttemptId_ = 0;
}

void WifiService::checkApRetry() {
  if (apState_ != ApState::RetryWait) return;
  if (millis() - apRetryStartedAt_ < kApRetryIntervalMs) return;

  requestAp();
}

void WifiService::checkApTimeout() {
  if (apState_ != ApState::Active) return;

  if (WiFi.softAPgetStationNum() > 0) {
    // Timeout measures continuous AP idleness, not total AP uptime.
    apStartedAt_ = millis();
    return;
  }

  if (millis() - apStartedAt_ < kApTimeoutMs) return;

  Serial.println("[WIFI] AP setup timeout");
  stopAp();
}
