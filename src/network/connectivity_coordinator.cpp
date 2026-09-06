#include "connectivity_coordinator.h"

#include "../config.h"
#include "../storage/eeprom_store.h"

#include "mqtt_service.h"

namespace {
  String formatIpv4(const WifiController::Ipv4Address& address) {
    return String(address.octets[0]) + "." + address.octets[1] + "." + address.octets[2] + "." + address.octets[3];
  }
} // namespace

void ConnectivityCoordinator::load() {
  wifiConfig_ = eeprom_.readWifiConfig();
  mqttConfig_ = eeprom_.readMqttConfig();
  otaEnabled_ = eeprom_.readOtaEnabled();
}

ConnectivityStatus ConnectivityCoordinator::status() const {
  const WifiController::Snapshot snapshot = wifi_.snapshot();
  ConnectivityStatus status;
  status.deviceId = snapshot.deviceId;
  status.wifiSsid = snapshot.wifiSsid;
  status.localIp = formatIpv4(snapshot.localIp);
  status.gateway = formatIpv4(snapshot.gateway);
  status.mac = snapshot.mac;
  status.rssi = snapshot.rssi;
  status.channel = snapshot.channel;
  return status;
}

WifiController::Config ConnectivityCoordinator::wifiRuntimeConfig() const {
  WifiController::Config config{};
  const uint8_t apIp[4] = AP_IP;
  config.deviceId = DEVICE_NAME;
  config.staSsid = wifiConfig_.ssid;
  config.staPassword = wifiConfig_.password;
  config.apSsid = AP_SSID;
  config.apPassword = AP_PASS;
  config.apIp = WifiController::Ipv4Address{{apIp[0], apIp[1], apIp[2], apIp[3]}};
  return config;
}

bool ConnectivityCoordinator::saveWifiConfig(const char* ssid, const char* password) {
  if (!eeprom_.writeWifiConfig(ssid, password)) return false;

  wifiConfig_ = eeprom_.readWifiConfig();
  return true;
}

bool ConnectivityCoordinator::saveAndApplyMqttConfig(
  const char* host, uint16_t port, const char* user, const char* password
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
