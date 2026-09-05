#include "hanumber.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <Arduino.h>
#include <PubSubClient.h>

const char* const HANumber::component PROGMEM = "number";

HANumber::HANumber(const char* unique_id, const char* name, HADevice& device, int min, int max, const int step)
  : HANumber(unique_id, name, min, max, step) {
  this->device = &device;
}

HANumber::HANumber(const char* unique_id, const char* name, int min, int max, const int step)
  : HAEntity(unique_id, name, component) {
  this->dirty = false;
  this->max = max;
  this->min = min;
  this->step = step;
  this->state = min;
}

void HANumber::onConnect(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH], payload[HA_MAX_PAYLOAD_LENGTH];
  getCommandTopic(topic);
  client->subscribe(topic);

  getConfigTopic(topic);
  getConfigPayload(payload, true, true);
  sprintf(payload + strlen(payload) - 1, ",\"min\":%d,\"max\":%d,\"step\":%d}", this->min, this->max, this->step);
  client->publish(topic, payload);
}

HAOperationResult HANumber::discoveryStep(PubSubClient* client, uint8_t step) {
  if (step == 0) {
    char topic[HA_MAX_TOPIC_LENGTH];
    getCommandTopic(topic);
    if (client->subscribe(topic)) return HAOperationResult::Done;
    return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
  }
  if (step != 1) return HAOperationResult::Fatal;
  char topic[HA_MAX_TOPIC_LENGTH], payload[HA_MAX_PAYLOAD_LENGTH];
  getConfigTopic(topic);
  getConfigPayload(payload, true, true);
  sprintf(payload + strlen(payload) - 1, ",\"min\":%d,\"max\":%d,\"step\":%d}", this->min, this->max, this->step);
  if (client->publish(topic, payload)) return HAOperationResult::Done;
  return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
}

bool HANumber::sendState(PubSubClient* client) {
  char payload[10];
  char topic[HA_MAX_TOPIC_LENGTH];
  getStateTopic(topic);
  sprintf(payload, "%d", this->state);
  const bool published = client->publish(topic, payload);
  if (published) dirty = false;
  return published;
}

void HANumber::setState(int state) {
  if (state == this->state) return;
  dirty = true;
  if (state > this->max) state = this->max;
  if (state < this->min) state = this->min;
  this->state = state;
  this->onStateChange();
}

bool HANumber::onReceivedTopic(PubSubClient* client, uint8_t* payload, unsigned int length) {
  (void)client;
  char buff[15];
  if (length < 1 || length >= sizeof(buff)) return false;
  memcpy(buff, payload, length);
  buff[length] = '\0';
  char* end = nullptr;
  errno = 0;
  const long value = strtol(buff, &end, 10);
  if (errno == ERANGE || end == buff || *end != '\0' || value < INT_MIN || value > INT_MAX) return false;
  this->setState(static_cast<int>(value));
  return true;
}
