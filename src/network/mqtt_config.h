#pragma once

#include <Arduino.h>

constexpr uint8_t kMqttHostLen = 33;
constexpr uint8_t kMqttPortLen = 10;
constexpr uint8_t kMqttUserLen = 33;
constexpr uint8_t kMqttPassLen = 33;

struct MqttConfig {
  char host[kMqttHostLen];
  char port[kMqttPortLen];
  char user[kMqttUserLen];
  char password[kMqttPassLen];
};
