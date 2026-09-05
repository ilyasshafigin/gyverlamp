#pragma once

#include <Arduino.h>

#include "mqtt_config.h"
#include "wifi_config.h"

class EepromStore;
class MqttService;
class OtaService;
class WifiService;

struct ConnectivityStatus {
  String deviceId;
  String wifiSsid;
  String localIp;
  String gateway;
  String mac;
  int32_t rssi = 0;
  int32_t channel = 0;
};

class ConnectivityCoordinator {
public:
  ConnectivityCoordinator(EepromStore& eeprom, WifiService& wifi, OtaService& ota, MqttService& mqtt)
    : eeprom_(eeprom),
      wifi_(wifi),
      ota_(ota),
      mqtt_(mqtt) {}

  void load();

  WifiConfig wifiConfig() const { return wifiConfig_; }
  MqttConfig mqttConfig() const { return mqttConfig_; }
  bool otaEnabled() const { return otaEnabled_; }
  ConnectivityStatus status() const;

  bool saveWifiConfig(const char* ssid, const char* password);
  bool saveAndApplyMqttConfig(const char* host, uint16_t port, const char* user, const char* password);
  void requestMqttEnabled(bool enabled);
  void requestMqttRestart();

  bool isMqttEnabled() const;
  const char* mqttStateName() const;

  bool isOtaEnabled() const;
  const char* otaStateName() const;
  bool requestOtaEnabled(bool enabled);
  void requestOtaRestart();

private:
  EepromStore& eeprom_;
  WifiService& wifi_;
  OtaService& ota_;
  MqttService& mqtt_;
  WifiConfig wifiConfig_{};
  MqttConfig mqttConfig_{};
  bool otaEnabled_ = false;
};
