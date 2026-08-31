#include "wifi_service.h"
#include "../config.h"
#include "../storage/eeprom_store.h"

void WifiService::init() {
  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    WiFi.mode(WIFI_AP);
    requestAp();
    staState_ = StaState::Provisioning;

    if (disabledHandler_) disabledHandler_();

    Serial.println("[WIFI] No STA config, AP open for setup");
  } else {
    WiFi.mode(WIFI_AP_STA);
    requestAp();
    startStaConnection();
  }
}

void WifiService::tick() {
  switch (staState_) {
    case StaState::Provisioning: checkApTimeout(); break;

    case StaState::Connecting: checkStaConnecting(); break;

    case StaState::Connected:
      if (!isStaConnected()) {
        staState_ = StaState::RetryWait;
        retryStartedAt_ = millis();
      }
      break;

    case StaState::RetryWait:
      checkStaRetryWait();
      checkApTimeout();
      break;
  }

  if (staState_ != StaState::Connected && !isStaConnected()) {
    checkApRetry();
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
  apState_ = ApState::Inactive;
  WiFi.softAPdisconnect(true);

  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  WiFi.mode(strlen(wifiConfig.ssid) == 0 ? WIFI_OFF : WIFI_STA);

  Serial.println("[WIFI] Access point stopped");
}

void WifiService::startStaConnection() {
  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    staState_ = StaState::Provisioning;
    return;
  }

  if (connectingHandler_) connectingHandler_();

  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  staState_ = StaState::Connecting;
  connectStartedAt_ = millis();
  Serial.println("[WIFI] Connecting to STA (background)");
}

void WifiService::checkStaConnecting() {
  if (isStaConnected()) {
    onStaConnected();
    return;
  }

  if (millis() - connectStartedAt_ < kStaConnectTimeoutMs) return;

  staState_ = StaState::RetryWait;
  retryStartedAt_ = millis();

  if (errorHandler_) errorHandler_();

  Serial.println("[WIFI] STA connection failed, keeping AP for setup");
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
  staState_ = StaState::Connected;

  if (connectedHandler_) connectedHandler_();

  Serial.print("[WIFI] STA IP: ");
  Serial.println(WiFi.localIP());
  stopAp();
}

void WifiService::checkApRetry() {
  if (apState_ != ApState::RetryWait) return;
  if (millis() - apRetryStartedAt_ < kApRetryIntervalMs) return;

  requestAp();
}

void WifiService::checkApTimeout() {
  if (apState_ != ApState::Active) return;
  if (millis() - apStartedAt_ < kApTimeoutMs) return;

  if (WiFi.softAPgetStationNum() > 0) {
    apStartedAt_ = millis();
    return;
  }

  Serial.println("[WIFI] AP setup timeout");
  stopAp();
}
