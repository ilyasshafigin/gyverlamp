#pragma once

#include <Arduino.h>

constexpr uint8_t WIFI_SSID_LEN = 33;
constexpr uint8_t WIFI_PASS_LEN = 33;

struct WifiConfig {
  char ssid[WIFI_SSID_LEN];
  char password[WIFI_PASS_LEN];
};
