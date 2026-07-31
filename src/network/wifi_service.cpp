#include "wifi_service.h"
#include "../config.h"
#include "../notification/controller.h"
#include "../storage/eeprom_store.h"

void WifiService::init() {
  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    WiFi.mode(WIFI_AP);
    requestAp();
    _staState = StaState::Provisioning;
    _notifications.onWifiDisabled();
    Serial.println("[WIFI] No STA config, AP open for setup");
  } else {
    WiFi.mode(WIFI_AP_STA);
    requestAp();
    startStaConnection();
  }
}

void WifiService::tick() {
  switch (_staState) {
    case StaState::Provisioning: checkApTimeout(); break;

    case StaState::Connecting: checkStaConnecting(); break;

    case StaState::Connected:
      if (!isStaConnected()) {
        _staState = StaState::RetryWait;
        _retryStartedAt = millis();
      }
      break;

    case StaState::RetryWait:
      checkStaRetryWait();
      checkApTimeout();
      break;
  }

  if (_staState != StaState::Connected && !isStaConnected()) {
    checkApRetry();
  }
}

bool WifiService::startAp() {
  if (_apState == ApState::Active) return true;

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

  _apState = ApState::Active;
  _apStartedAt = millis();

  Serial.println("[WIFI] Access point mode");
  Serial.print("[WIFI] AP IP: ");
  Serial.println(WiFi.softAPIP());
  return true;
}

void WifiService::requestAp() {
  if (startAp()) {
    return;
  }

  _apState = ApState::RetryWait;
  _apRetryStartedAt = millis();
}

void WifiService::stopAp() {
  _apState = ApState::Inactive;
  WiFi.softAPdisconnect(true);

  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  WiFi.mode(strlen(wifiConfig.ssid) == 0 ? WIFI_OFF : WIFI_STA);

  Serial.println("[WIFI] Access point stopped");
}

void WifiService::startStaConnection() {
  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    _staState = StaState::Provisioning;
    return;
  }

  _notifications.onWifiConnecting();
  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  _staState = StaState::Connecting;
  _connectStartedAt = millis();
  Serial.println("[WIFI] Connecting to STA (background)");
}

void WifiService::checkStaConnecting() {
  if (isStaConnected()) {
    onStaConnected();
    return;
  }

  if (millis() - _connectStartedAt < STA_CONNECT_TIMEOUT_MS) return;

  _staState = StaState::RetryWait;
  _retryStartedAt = millis();
  _notifications.onWifiError();
  Serial.println("[WIFI] STA connection failed, keeping AP for setup");
}

void WifiService::checkStaRetryWait() {
  if (isStaConnected()) {
    onStaConnected();
    return;
  }

  if (millis() - _retryStartedAt < RECONNECT_INTERVAL_MS) return;
  startStaConnection();
}

void WifiService::onStaConnected() {
  _staState = StaState::Connected;
  _notifications.onWifiConnected();
  Serial.print("[WIFI] STA IP: ");
  Serial.println(WiFi.localIP());
  stopAp();
}

void WifiService::checkApRetry() {
  if (_apState != ApState::RetryWait) return;
  if (millis() - _apRetryStartedAt < AP_RETRY_INTERVAL_MS) return;

  requestAp();
}

void WifiService::checkApTimeout() {
  if (_apState != ApState::Active) return;
  if (millis() - _apStartedAt < AP_TIMEOUT_MS) return;

  if (WiFi.softAPgetStationNum() > 0) {
    _apStartedAt = millis();
    return;
  }

  Serial.println("[WIFI] AP setup timeout");
  stopAp();
}
