#pragma once

#include <Arduino.h>

struct WifiConfig {
  static constexpr uint8_t kWifiSsidLen = 33;
  static constexpr uint8_t kWifiPassLen = 33;

  char ssid[kWifiSsidLen];
  char password[kWifiPassLen];
};
