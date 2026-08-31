#pragma once

#include <Arduino.h>

constexpr uint8_t kWifiSsidLen = 33;
constexpr uint8_t kWifiPassLen = 33;

struct WifiConfig {
  char ssid[kWifiSsidLen];
  char password[kWifiPassLen];
};
