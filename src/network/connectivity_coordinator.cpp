#include "connectivity_coordinator.h"

#include <ESP8266WiFi.h>

#include "wifi_service.h"
#include "../storage/eeprom_store.h"

#include "mqtt_service.h"
#include "ota_service.h"

void ConnectivityCoordinator::load() {
  wifiConfig_ = eeprom_.readWifiConfig();
  mqttConfig_ = eeprom_.readMqttConfig();
  otaEnabled_ = eeprom_.readOtaEnabled();
}

ConnectivityStatus ConnectivityCoordinator::status() const {
  ConnectivityStatus status;
  status.deviceId = wifi_.getDeviceId();
  status.wifiSsid = WiFi.SSID();
  status.localIp = WiFi.localIP().toString();
  status.gateway = WiFi.gatewayIP().toString();
  status.mac = WiFi.macAddress();
  status.rssi = WiFi.RSSI();
  status.channel = WiFi.channel();
  return status;
}

bool ConnectivityCoordinator::saveWifiConfig(const char* ssid, const char* password) {
  if (!eeprom_.writeWifiConfig(ssid, password)) return false;

  wifiConfig_ = eeprom_.readWifiConfig();
  return true;
}

bool ConnectivityCoordinator::saveAndApplyMqttConfig(
  const char* host, const char* port, const char* user, const char* password
) {
  if (!eeprom_.writeMqttConfig(host, port, user, password)) return false;

  mqttConfig_ = eeprom_.readMqttConfig();
  mqtt_.requestApply(mqttConfig_);
  return true;
}

void ConnectivityCoordinator::requestMqttEnabled(bool enabled) {
  mqtt_.requestEnabled(enabled);
}

void ConnectivityCoordinator::requestMqttRestart() {
  mqtt_.requestRestart();
}

bool ConnectivityCoordinator::isMqttEnabled() const {
  return mqtt_.isEnabled();
}

const char* ConnectivityCoordinator::mqttStateName() const {
  return mqtt_.stateName();
}

bool ConnectivityCoordinator::isOtaEnabled() const {
  return ota_.isEnabled();
}

const char* ConnectivityCoordinator::otaStateName() const {
  return ota_.stateName();
}

bool ConnectivityCoordinator::requestOtaEnabled(bool enabled) {
  if (!eeprom_.writeOtaEnabled(enabled)) return false;

  otaEnabled_ = enabled;
  ota_.requestEnabled(enabled);
  return true;
}

void ConnectivityCoordinator::requestOtaRestart() {
  ota_.requestRestart();
}
