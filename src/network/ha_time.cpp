#include "ha_time.h"

#ifdef USE_MQTT

#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <hadevice.h>

namespace {

  bool isDigit(char c) {
    return c >= '0' && c <= '9';
  }

  // Parse time string and normalize to HH:MM.
  // Accepted inputs: H:MM, HH:MM, H:MM:SS, HH:MM:SS
  // Returns true if valid and writes canonical "HH:MM" to normalized (must hold 6 bytes).
  bool parseTime(const char* input, char* normalized) {
    if (input == nullptr) {
      return false;
    }

    int hours = -1;
    int minutes = -1;
    int seconds = 0;

    const char* p = input;

    // Hours: one or two digits
    if (!isDigit(*p)) {
      return false;
    }
    hours = *p++ - '0';
    if (isDigit(*p)) {
      hours = hours * 10 + (*p++ - '0');
    }

    if (*p++ != ':') {
      return false;
    }

    // Minutes: exactly two digits
    if (!isDigit(p[0]) || !isDigit(p[1])) {
      return false;
    }
    minutes = (p[0] - '0') * 10 + (p[1] - '0');
    p += 2;

    // Optional seconds
    if (*p == ':') {
      p++;
      if (!isDigit(p[0]) || !isDigit(p[1])) {
        return false;
      }
      seconds = (p[0] - '0') * 10 + (p[1] - '0');
      p += 2;
    }

    // Must be end of string
    if (*p != '\0') {
      return false;
    }

    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 || seconds > 59) {
      return false;
    }

    snprintf(normalized, 6, "%02d:%02d", hours, minutes);
    return true;
  }

}

const char* const HATime::component PROGMEM = "time";

HATime::HATime(const char* unique_id, const char* name, HADevice& device) :
  HATime(unique_id, name) {
  this->device = &device;
}

HATime::HATime(const char* unique_id, const char* name) :
  HAEntity(unique_id, name, component) {
  this->dirty = false;
  this->state[0] = '\0';
  this->commandCallback = nullptr;
}

void HATime::setState(const char* timeState) {
  if (timeState == nullptr) {
    return;
  }

  char normalized[6];
  if (!parseTime(timeState, normalized)) {
    return;
  }

  if (strcmp(this->state, normalized) == 0) {
    return;
  }

  dirty = true;
  strlcpy(this->state, normalized, sizeof(this->state));
  this->onStateChange();
}

void HATime::onCommand(void (*callback)(const char* timeState)) {
  this->commandCallback = callback;
}

void HATime::onConnect(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH];
  char payload[HA_MAX_PAYLOAD_LENGTH];

  getCommandTopic(topic);
  client->subscribe(topic);

  getConfigTopic(topic);
  getConfigPayload(payload, true, true);
  client->publish(topic, payload);
}

void HATime::sendState(PubSubClient* client) {
  dirty = false;
  char topic[HA_MAX_TOPIC_LENGTH];
  getStateTopic(topic);
  client->publish(topic, state);
}

void HATime::onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;

  if (length < 1 || length > 8) {
    return;
  }

  char buffer[9];
  strncpy(buffer, reinterpret_cast<const char*>(payload), length);
  buffer[length] = '\0';

  char normalized[6];
  if (!parseTime(buffer, normalized)) {
    return;
  }

  setState(normalized);

  if (commandCallback != nullptr) {
    commandCallback(state);
  }
}

bool HATime::dispatchCommand(PubSubClient* client, const char* topic, byte* payload, unsigned int length) {
  char buffer[HA_MAX_TOPIC_LENGTH];
  getCommandTopic(buffer);
  if (strcmp(buffer, topic) != 0) {
    return false;
  }

  onReceivedTopic(client, payload, length);
  sendState(client);
  return true;
}

#endif
