#include "haswitch.h"
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <PubSubClient.h>

#include <hatext.h>

const char* const HAText::component PROGMEM = "text";

HAText::HAText(const char* unique_id, const char* name, HADevice& device, unsigned int max_size)
  : HAText(unique_id, name, max_size) {
  this->device = &device;
}

HAText::HAText(const char* unique_id, const char* name, unsigned int max_size)
  : HAEntity(unique_id, name, component) {
  this->maxSize = max_size;
  this->state = new char[max_size];
  this->state[0] = 0;
  this->dirty = false;
}

void HAText::onConnect(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH], payload[HA_MAX_PAYLOAD_LENGTH];
  getCommandTopic(topic);
  client->subscribe(topic);

  this->getConfigTopic(topic);
  this->getConfigPayload(payload, true, true);
  client->publish(topic, payload);
}

bool HAText::sendState(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH];
  getStateTopic(topic);
  const bool published = client->publish(topic, state);
  if (published) dirty = false;
  return published;
}

void HAText::setState(const char* txt) {
  if (strcmp(this->state, txt) == 0) return;
  dirty = true;
  strncpy(this->state, txt, this->maxSize);
  this->state[this->maxSize - 1] = 0;
  this->onStateChange();
}

bool HAText::onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;
  if (length >= this->maxSize) return false;
  memcpy(this->state, payload, length);
  this->state[length] = 0;
  dirty = true;
  this->onStateChange();
  return true;
}
