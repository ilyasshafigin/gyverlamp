#include "haswitch.h"
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <PubSubClient.h>

#include "hadevice.h"

const char* const HASwitch::component PROGMEM = "switch";

HASwitch::HASwitch(const char* unique_id, const char* name, HADevice& device)
  : HASwitch(unique_id, name) {
  this->device = &device;
}

HASwitch::HASwitch(const char* unique_id, const char* name)
  : HAEntity(unique_id, name, component) {
  this->dirty = false;
  this->state = false;
}

// send config and send state
void HASwitch::onConnect(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH], payload[HA_MAX_PAYLOAD_LENGTH];
  getCommandTopic(topic);
  client->subscribe(topic);

  getConfigTopic(topic);
  getConfigPayload(payload, true, true);
  client->publish(topic, payload);
}

bool HASwitch::sendState(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH];
  getStateTopic(topic);
  const bool published = client->publish(topic, this->state ? "ON" : "OFF");
  if (published) dirty = false;
  return published;
}

void HASwitch::setState(bool state) {
  if (state == this->state) return;
  dirty = true;
  this->state = state;
  this->onStateChange();
}

bool HASwitch::onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;
  // Default values in homeassistant are "ON" and "OFF"
  if (length == 2 && payload[0] == 'O' && payload[1] == 'N') {
    this->setState(true);
    return true;
  }
  if (length == 3 && payload[0] == 'O' && payload[1] == 'F' && payload[2] == 'F') {
    this->setState(false);
    return true;
  }
  return false;
}
