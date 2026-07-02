#include "../config.h"
#include "../core/frame_renderer.h"
#include "../notification/controller.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "wifi_service.h"

void WifiService::init() {
  resetSavedWifiIfNeeded();

  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    WiFi.mode(WIFI_AP);
    startAp();
    _notifications.onWifiDisabled();
    Serial.println("[WIFI] No STA config, AP open for setup");
    return;
  }

  WiFi.mode(WIFI_STA);
  if (!initSta()) {
    WiFi.mode(WIFI_AP_STA);
    startAp();
  }
}

void WifiService::tick() {
  checkStaReconnect();
  checkApTimeout();
}

void WifiService::startAp() {
  if (_apActive) return;

  const uint8_t ipAp[4] = AP_IP;
  const IPAddress apIp(ipAp[0], ipAp[1], ipAp[2], ipAp[3]);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  _apActive = true;
  _apStartedAt = millis();

  Serial.println("[WIFI] Access point mode");
  Serial.print("[WIFI] AP IP: "); Serial.println(WiFi.softAPIP());
}

void WifiService::stopAp() {
  if (!_apActive) return;

  WiFi.softAPdisconnect(true);
  _apActive = false;

  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  WiFi.mode(strlen(wifiConfig.ssid) == 0 ? WIFI_OFF : WIFI_STA);

  Serial.println("[WIFI] Access point stopped");
}

bool WifiService::initSta() {
  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    _notifications.onWifiDisabled();
    Serial.println("[WIFI] No STA config");
    return false;
  }

  _notifications.onWifiConnecting();
  _frameRenderer.renderNow();

  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  Serial.print("[WIFI] Connecting to STA");

  uint8_t tries = 20;
  uint32_t lastCheckMs = millis();

  while (WiFi.status() != WL_CONNECTED && tries > 0) {
    _frameRenderer.renderNow();

    const uint32_t now = millis();
    if (now - lastCheckMs >= 500) {
      lastCheckMs = now;

      Serial.print('.');
      tries--;

      if (WiFi.status() == WL_CONNECTED) {
        break;
      }
    }

    delay(1);
  }
  Serial.println();

  if (isStaConnected()) {
    _staWasConnected = true;
    _notifications.onWifiConnected();
    Serial.print("[WIFI] STA IP: "); Serial.println(WiFi.localIP());
    return true;
  }

  _staWasConnected = false;
  _notifications.onWifiError();
  Serial.println("[WIFI] STA connection failed, opening AP for setup");
  return false;
}

void WifiService::resetSavedWifiIfNeeded() {
  if (_settings.getBootCount() >= 5) {
    _settings.resetBootCount();
    WiFi.disconnect(true);
    delay(1000);
    Serial.println("[WIFI] Saved WiFi settings reset");
  }
}

void WifiService::checkStaReconnect() {
  const bool connected = isStaConnected();

  if (connected) {
    if (!_staWasConnected) {
      _notifications.onWifiConnected();
    }

    stopAp();

    _staWasConnected = true;
    _reconnectTimer = millis();
    return;
  }

  _staWasConnected = false;

  if (millis() - _reconnectTimer < RECONNECT_INTERVAL_MS) return;
  _reconnectTimer = millis();

  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) return;

  _notifications.onWifiConnecting();
  Serial.println("[WIFI] Reconnecting STA");
  WiFi.reconnect();
}

void WifiService::checkApTimeout() {
  if (!_apActive) return;
  if (millis() - _apStartedAt < AP_TIMEOUT_MS) return;

  if (WiFi.softAPgetStationNum() > 0) {
    _apStartedAt = millis();
    return;
  }

  Serial.println("[WIFI] AP setup timeout");
  stopAp();
}
