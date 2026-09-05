#include "haentity.h"
#include "hadevice.h"
#include "haconsts.h"

#include <PubSubClient.h>

/* HA discovery template is
 <discovery_prefix>/<component>/[<node_id>/]<object_id>/config node_id is
 optional and will ignored in this implementation
*/

// Configuration topic
const char* const HAEntity::configTopicTemplate PROGMEM = "homeassistant/%s/%s/config";

// Configuration payload parts
const char* const HAEntity::configPayloadTemplate PROGMEM = "{\
\"~\":\"homeassistant/%s/%s\",\
\"name\":\"%s\",\
\"uniq_id\":\"%s\"";

const char* const HAEntity::commandTopicTemplate PROGMEM = "homeassistant/%s/%s/set";
const char* const HAEntity::stateTopicTemplate PROGMEM = "homeassistant/%s/%s/state";
const char* const HAEntity::availabilityTopicTemplate PROGMEM = "homeassistant/%s/%s/available";

const char* const HAEntity::featureKeys[] PROGMEM = {"dev_cla", "ic", "ent_cat", "mode"};

HAEntity::HAEntity(const char* id, const char* name, const char* component, HADevice* ha_device) {
  this->uniqueId = NULL;
  this->id = id;
  this->component = component;
  this->name = name;
  this->device = ha_device;
  this->available = HA_AVTY_DISABLED;
  this->features = NULL;
}

const char* HAEntity::getUniqueId() {
  if (this->device == NULL) return this->id;
  if (this->uniqueId == NULL) {
    this->uniqueId = new char[strlen(this->id) + strlen(this->device->getIdentifier()) + 2];
    sprintf(this->uniqueId, "%s%s", this->device->getIdentifier(), this->id);
  }
  return this->uniqueId;
}

void HAEntity::prepare() {
  getUniqueId();
}

char* HAEntity::getConfigPayload(char* buffer, bool add_command_topic, bool add_state_topic) {
  int len;
  sprintf(buffer, configPayloadTemplate, component, getUniqueId(), name, getUniqueId());
  len = strlen(buffer);
  if (this->device != NULL) {
    buffer[len] = ',';
    this->device->getConfigPayload(buffer + len + 1);
    len = strlen(buffer);
  }
  if (add_command_topic) {
    buffer[len] = ',';
    sprintf(buffer + len + 1, PSTR("\"cmd_t\":\"~/set\""));
    len = strlen(buffer);
  }
  if (add_state_topic) {
    buffer[len] = ',';
    sprintf(buffer + len + 1, PSTR("\"stat_t\":\"~/state\""));
    len = strlen(buffer);
  }
  // Availability if set in the entity or in the device (common to all entities)
  if (this->available != HA_AVTY_DISABLED) {
    buffer[len] = ',';
    sprintf(buffer + len + 1, PSTR("\"avty_t\":\"~/available\""));
    len = strlen(buffer);
  } else if (this->device != NULL && this->device->getAvailability()) {
    buffer[len] = ',';
    sprintf(buffer + len + 1, PSTR("\"avty_t\":\""));
    len = strlen(buffer);
    this->device->getAvailabilityTopic(buffer + len);
    len = strlen(buffer);
    sprintf(buffer + len, "\"");
    len = strlen(buffer);
  }
  // Add features
  HAKVPairList* pair = features;
  while (pair != NULL && pair->getKey() != NULL) {
    buffer[len] = ',';
    if (pair->getKey() == featureKeys[HA_FEATURE_AVAILABILITY])
      sprintf(buffer + len + 1, PSTR("\"%s\":\"available\""), pair->getKey());
    else
      sprintf(buffer + len + 1, PSTR("\"%s\":\"%s\""), pair->getKey(), pair->getValue());
    len = strlen(buffer);
    pair = pair->getNext();
  }

  buffer[len] = '}';
  buffer[len + 1] = '\0';
  return buffer;
}

char* HAEntity::getConfigTopic(char* buffer) {
  sprintf(buffer, configTopicTemplate, this->component, getUniqueId());
  return buffer;
}

char* HAEntity::getCommandTopic(char* buffer) {
  sprintf(buffer, commandTopicTemplate, component, getUniqueId());
  return buffer;
}

bool HAEntity::handleCommand(PubSubClient* client, char* topic, byte* payload, size_t length) {
  char commandTopic[HA_MAX_TOPIC_LENGTH];
  const char* command = getCommandTopic(commandTopic);
  if (command == NULL || strcmp(command, topic) != 0) return false;
  return onReceivedTopic(client, payload, static_cast<unsigned int>(length));
}

uint8_t HAEntity::discoveryStepCount() {
  char topic[HA_MAX_TOPIC_LENGTH];
  return getCommandTopic(topic) == NULL ? 1 : 2;
}

HAOperationResult HAEntity::discoveryStep(PubSubClient* client, uint8_t step) {
  char topic[HA_MAX_TOPIC_LENGTH];
  const char* commandTopic = getCommandTopic(topic);
  if (commandTopic != NULL && step == 0) {
    if (client->subscribe(commandTopic)) return HAOperationResult::Done;
    return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
  }
  if (step != (commandTopic == NULL ? 0 : 1)) return HAOperationResult::Fatal;

  char payload[HA_MAX_PAYLOAD_LENGTH];
  getConfigTopic(topic);
  getConfigPayload(payload, commandTopic != NULL, discoveryHasStateTopic());
  if (client->publish(topic, payload)) return HAOperationResult::Done;
  return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
}

HAOperationResult HAEntity::stateStep(PubSubClient* client, uint8_t step, uint32_t revision) {
  (void)revision;
  if (step != 0) return HAOperationResult::Fatal;
  if (sendState(client)) return HAOperationResult::Done;
  return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
}

char* HAEntity::getStateTopic(char* buffer) {
  sprintf(buffer, stateTopicTemplate, component, getUniqueId());
  return buffer;
}

void HAEntity::addFeature(int key, const char* value) {
  if (key == HA_FEATURE_AVAILABILITY) this->available = HA_AVTY_PENDING_ON;
  else
    this->addFeature(featureKeys[key], value);
}

void HAEntity::addFeature(const char* key, const char* value) {
  if (this->features == NULL) this->features = new HAKVPairList(key, value);
  else
    features->append(key, value);
}

void HAEntity::setAvailable(bool available) {
  if (this->available == HA_AVTY_DISABLED) return;
  if (available && this->available != HA_AVTY_ON) this->available = HA_AVTY_PENDING_ON;
  else if (!available && this->available != HA_AVTY_OFF)
    this->available = HA_AVTY_PENDING_OFF;
}

void HAEntity::sendAvailable(PubSubClient* mqttClient, bool force) {
  if (this->available == HA_AVTY_DISABLED) return;
  if (force && this->available == HA_AVTY_ON) this->available = HA_AVTY_PENDING_ON;
  else if (force && this->available == HA_AVTY_OFF)
    this->available = HA_AVTY_PENDING_OFF;

  if (this->available != HA_AVTY_PENDING_ON && this->available != HA_AVTY_PENDING_OFF) return;

  char topic[HA_MAX_TOPIC_LENGTH];
  getAvailabilityTopic(topic);
  if (this->available == HA_AVTY_PENDING_ON) {
    if (!mqttClient->publish(topic, "online")) return;
    this->available = HA_AVTY_ON;
  } else if (this->available == HA_AVTY_PENDING_OFF) {
    if (!mqttClient->publish(topic, "offline")) return;
    this->available = HA_AVTY_OFF;
  }
}

char* HAEntity::getAvailabilityTopic(char* buffer) {
  sprintf(buffer, availabilityTopicTemplate, component, getUniqueId());
  return buffer;
}
