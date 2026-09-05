#pragma once

#include <Arduino.h>

struct MqttConfig {
  static constexpr uint8_t kMqttHostLen = 33;
  static constexpr uint8_t kMqttUserLen = 33;
  static constexpr uint8_t kMqttPassLen = 33;
  static constexpr uint8_t kMqttPortTextLen = sizeof("65535");

  char host[kMqttHostLen];
  uint16_t port;
  char user[kMqttUserLen];
  char password[kMqttPassLen];
};
