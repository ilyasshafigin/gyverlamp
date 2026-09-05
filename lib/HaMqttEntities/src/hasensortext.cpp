#include <Arduino.h>
#include <PubSubClient.h>

#include <hasensortext.h>

HASensorText::HASensorText(const char* unique_id, const char* name, HADevice& device, int max_size)
  : HASensorText(unique_id, name, max_size) {
  this->device = &device;
}

HASensorText::HASensorText(const char* unique_id, const char* name, int max_size)
  : HASensor(unique_id, name) {
  this->maxSize = max_size;
  this->state = new char[max_size + 1];
  this->state[0] = 0;
}

bool HASensorText::sendState(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH];
  getStateTopic(topic);
  const bool published = client->publish(topic, this->state);
  if (published) dirty = false;
  return published;
}

void HASensorText::setState(const char* text) {
  if (strcmp(this->state, text) == 0) return;
  dirty = true;
  strncpy(this->state, text, this->maxSize);
  state[this->maxSize] = 0;
  this->onStateChange();
}
