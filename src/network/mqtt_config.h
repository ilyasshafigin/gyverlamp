#pragma once

#include <Arduino.h>

constexpr uint8_t MQTT_HOST_LEN = 33;
constexpr uint8_t MQTT_PORT_LEN = 10;
constexpr uint8_t MQTT_USER_LEN = 33;
constexpr uint8_t MQTT_PASS_LEN = 33;

struct MqttConfig {
  char host[MQTT_HOST_LEN];
  char port[MQTT_PORT_LEN];
  char user[MQTT_USER_LEN];
  char password[MQTT_PASS_LEN];
};
