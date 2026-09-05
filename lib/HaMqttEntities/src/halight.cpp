#include "halight.h"

#ifdef USE_MQTT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <hadevice.h>

namespace {

  bool parseRgbCsv(const char* payload, uint8_t& r, uint8_t& g, uint8_t& b) {
    int ir = -1;
    int ig = -1;
    int ib = -1;

    if (sscanf(payload, " %d , %d , %d", &ir, &ig, &ib) != 3) {
      return false;
    }

    if (ir < 0) ir = 0;
    if (ig < 0) ig = 0;
    if (ib < 0) ib = 0;
    if (ir > 255) ir = 255;
    if (ig > 255) ig = 255;
    if (ib > 255) ib = 255;

    r = static_cast<uint8_t>(ir);
    g = static_cast<uint8_t>(ig);
    b = static_cast<uint8_t>(ib);
    return true;
  }

  bool parseRgbJson(const char* payload, uint8_t& r, uint8_t& g, uint8_t& b) {
    int ir = -1;
    int ig = -1;
    int ib = -1;

    const char* p = payload;
    while (*p) {
      if (*p == '"' && (p[1] == 'r' || p[1] == 'g' || p[1] == 'b') && p[2] == '"') {
        char channel = p[1];
        p += 3;
        while (*p && (*p == ' ' || *p == ':' || *p == '\t')) {
          p++;
        }
        int value = 0;
        bool found = false;
        while (*p >= '0' && *p <= '9') {
          value = value * 10 + (*p - '0');
          found = true;
          p++;
        }
        if (found) {
          if (value < 0) value = 0;
          if (value > 255) value = 255;
          if (channel == 'r') ir = value;
          else if (channel == 'g')
            ig = value;
          else if (channel == 'b')
            ib = value;
        }
      } else {
        p++;
      }
    }

    if (ir >= 0 && ig >= 0 && ib >= 0) {
      r = static_cast<uint8_t>(ir);
      g = static_cast<uint8_t>(ig);
      b = static_cast<uint8_t>(ib);
      return true;
    }
    return false;
  }

  bool parseRgbPayload(const char* payload, uint8_t& r, uint8_t& g, uint8_t& b) {
    return parseRgbCsv(payload, r, g, b) || parseRgbJson(payload, r, g, b);
  }

} // namespace

HALight::HALight(const char* unique_id, const char* name, HADevice& device)
  : HALight(unique_id, name) {
  this->device = &device;
}

HALight::HALight(const char* unique_id, const char* name)
  : HAEntity(unique_id, name, "light") {
  this->dirty = false;
  this->revision = 0;
  this->state = false;
  this->brightness = 255;
  this->red = 255;
  this->green = 255;
  this->blue = 255;
  this->effect[0] = '\0';
  this->commandCallback = nullptr;
  this->effectCommandCallback = nullptr;
  this->colorCommandCallback = nullptr;
}

char* HALight::getCommandTopic(char* buffer) {
  (void)buffer;
  return nullptr;
}

char* HALight::getOnOffCommandTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/set", component, getUniqueId());
  return buffer;
}

char* HALight::getBrightnessCommandTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/brightness/set", component, getUniqueId());
  return buffer;
}

char* HALight::getBrightnessStateTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/brightness/state", component, getUniqueId());
  return buffer;
}

char* HALight::getEffectCommandTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/effect/set", component, getUniqueId());
  return buffer;
}

char* HALight::getEffectStateTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/effect/state", component, getUniqueId());
  return buffer;
}

char* HALight::getColorCommandTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/color/set", component, getUniqueId());
  return buffer;
}

char* HALight::getColorStateTopic(char* buffer) {
  sprintf(buffer, "homeassistant/%s/%s/color/state", component, getUniqueId());
  return buffer;
}

bool HALight::isBrightnessCommandTopic(const char* topic) {
  char buffer[HA_MAX_TOPIC_LENGTH];
  getBrightnessCommandTopic(buffer);
  return strcmp(buffer, topic) == 0;
}

bool HALight::isEffectCommandTopic(const char* topic) {
  char buffer[HA_MAX_TOPIC_LENGTH];
  getEffectCommandTopic(buffer);
  return strcmp(buffer, topic) == 0;
}

bool HALight::isColorCommandTopic(const char* topic) {
  char buffer[HA_MAX_TOPIC_LENGTH];
  getColorCommandTopic(buffer);
  return strcmp(buffer, topic) == 0;
}

void HALight::onConnect(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH];
  char payload[HA_MAX_PAYLOAD_LENGTH];

  getOnOffCommandTopic(topic);
  client->subscribe(topic);

  getBrightnessCommandTopic(topic);
  client->subscribe(topic);

  if (effectList.length() > 0) {
    getEffectCommandTopic(topic);
    client->subscribe(topic);
  }

  getColorCommandTopic(topic);
  client->subscribe(topic);

  getConfigTopic(topic);
  getConfigPayload(payload, true, true);

  size_t len = strlen(payload);
  if (len > 0 && payload[len - 1] == '}') {
    payload[len - 1] = '\0';
    len--;
  }

  snprintf(
    payload + len,
    sizeof(payload) - len,
    ",\"brightness\":true,\"bri_cmd_t\":\"~/brightness/set\",\"bri_stat_t\":\"~/brightness/state\",\"bri_scl\":255"
  );
  len = strlen(payload);

  if (effectList.length() > 0) {
    snprintf(
      payload + len,
      sizeof(payload) - len,
      ",\"fx_cmd_t\":\"~/effect/set\",\"fx_stat_t\":\"~/effect/state\",\"fx_list\":[%s]",
      effectList.c_str()
    );
    len = strlen(payload);
  }

  snprintf(
    payload + len,
    sizeof(payload) - len,
    ",\"supported_color_modes\":[\"rgb\"],\"rgb_cmd_t\":\"~/color/set\",\"rgb_stat_t\":\"~/color/state\"}"
  );

  client->publish(topic, payload);
}

bool HALight::sendState(PubSubClient* client) {
  char topic[HA_MAX_TOPIC_LENGTH];
  char payload[64];

  getStateTopic(topic);
  bool published = client->publish(topic, state ? "ON" : "OFF");

  getBrightnessStateTopic(topic);
  sprintf(payload, "%d", brightness);
  published = client->publish(topic, payload) && published;

  if (effectList.length() > 0) {
    getEffectStateTopic(topic);
    published = client->publish(topic, effect) && published;
  }

  getColorStateTopic(topic);
  snprintf(payload, sizeof(payload), "%d,%d,%d", red, green, blue);
  published = client->publish(topic, payload) && published;
  if (published) dirty = false;
  return published;
}

void HALight::setState(bool state) {
  if (state == this->state) return;
  dirty = true;
  revision++;
  this->state = state;
  this->onStateChange();
}

void HALight::setBrightness(uint8_t brightness) {
  if (brightness == this->brightness) return;
  dirty = true;
  revision++;
  this->brightness = brightness;
  this->onStateChange();
}

void HALight::setEffectList(const char* commaSeparatedEffects) {
  effectList = "";
  if (commaSeparatedEffects == nullptr || commaSeparatedEffects[0] == '\0') {
    return;
  }

  size_t effectCount = 1;
  for (const char* p = commaSeparatedEffects; *p; p++)
    if (*p == ',') effectCount++;
  effectList.reserve(strlen(commaSeparatedEffects) + effectCount * 2);

  const char* p = commaSeparatedEffects;
  while (true) {
    const char* comma = strchr(p, ',');
    size_t len = comma ? static_cast<size_t>(comma - p) : strlen(p);

    while (len > 0 && (p[0] == ' ' || p[0] == '\t')) {
      p++;
      len--;
    }
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
      len--;
    }

    if (len > 0) {
      if (effectList.length() > 0) {
        effectList += ',';
      }
      effectList += '"';
      char buf[64];
      if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
      }
      strncpy(buf, p, len);
      buf[len] = '\0';
      effectList += buf;
      effectList += '"';
    }

    if (!comma) {
      break;
    }
    p = comma + 1;
  }
}

void HALight::setEffect(const char* effectName) {
  if (effectName == nullptr) {
    return;
  }
  if (strcmp(effect, effectName) == 0) {
    return;
  }
  dirty = true;
  revision++;
  strlcpy(effect, effectName, sizeof(effect));
  onStateChange();
}

void HALight::setColor(uint8_t r, uint8_t g, uint8_t b) {
  if (r == red && g == green && b == blue) {
    return;
  }
  dirty = true;
  revision++;
  red = r;
  green = g;
  blue = b;
  onStateChange();
}

void HALight::onCommand(void (*callback)(bool on, uint8_t brightness)) {
  this->commandCallback = callback;
}

void HALight::onEffectCommand(void (*callback)(const char* effectName)) {
  this->effectCommandCallback = callback;
}

void HALight::onColorCommand(void (*callback)(uint8_t r, uint8_t g, uint8_t b)) {
  this->colorCommandCallback = callback;
}

bool HALight::onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;

  if (length == 2 && payload[0] == 'O' && payload[1] == 'N') {
    setState(true);
  } else if (length == 3 && payload[0] == 'O' && payload[1] == 'F' && payload[2] == 'F') {
    setState(false);
  } else {
    return false;
  }

  if (commandCallback != nullptr) {
    commandCallback(state, brightness);
  }
  return true;
}

bool HALight::onReceivedBrightnessTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;

  char buff[8];
  if (length < 1 || length > 3) return false;

  strncpy(buff, reinterpret_cast<const char*>(payload), length);
  buff[length] = '\0';

  char* end = nullptr;
  long value = strtol(buff, &end, 10);
  if (end == buff || *end != '\0') return false;
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  setBrightness(static_cast<uint8_t>(value));

  if (commandCallback != nullptr) {
    commandCallback(state, brightness);
  }
  return true;
}

bool HALight::onReceivedEffectTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;

  char name[sizeof(effect)];
  if (length >= sizeof(name)) return false;
  strncpy(name, reinterpret_cast<const char*>(payload), length);
  name[length] = '\0';

  setEffect(name);

  if (effectCommandCallback != nullptr) {
    effectCommandCallback(name);
  }
  return true;
}

bool HALight::onReceivedColorTopic(PubSubClient* client, byte* payload, unsigned int length) {
  (void)client;

  char json[64];
  if (length >= sizeof(json)) return false;
  strncpy(json, reinterpret_cast<const char*>(payload), length);
  json[length] = '\0';

  uint8_t r = red;
  uint8_t g = green;
  uint8_t b = blue;
  if (!parseRgbPayload(json, r, g, b)) {
    return false;
  }

  setColor(r, g, b);

  if (colorCommandCallback != nullptr) {
    colorCommandCallback(r, g, b);
  }
  return true;
}

bool HALight::dispatchCommand(PubSubClient* client, const char* topic, byte* payload, unsigned int length) {
  if (!handleCommand(client, const_cast<char*>(topic), payload, length)) {
    return false;
  }
  return true;
}

bool HALight::handleCommand(PubSubClient* client, char* topic, byte* payload, size_t length) {
  char buffer[HA_MAX_TOPIC_LENGTH];

  getOnOffCommandTopic(buffer);
  if (strcmp(buffer, topic) == 0) {
    if (!((length == 2 && payload[0] == 'O' && payload[1] == 'N') ||
          (length == 3 && payload[0] == 'O' && payload[1] == 'F' && payload[2] == 'F')))
      return false;
    return onReceivedTopic(client, payload, length);
  }

  getBrightnessCommandTopic(buffer);
  if (strcmp(buffer, topic) == 0) {
    if (length < 1 || length > 3) return false;
    return onReceivedBrightnessTopic(client, payload, length);
  }

  if (effectList.length() > 0) {
    getEffectCommandTopic(buffer);
    if (strcmp(buffer, topic) == 0) {
      return onReceivedEffectTopic(client, payload, length);
    }
  }

  getColorCommandTopic(buffer);
  if (strcmp(buffer, topic) == 0) {
    char json[64];
    if (length >= sizeof(json)) return false;
    strncpy(json, reinterpret_cast<const char*>(payload), length);
    json[length] = '\0';
    uint8_t r = red;
    uint8_t g = green;
    uint8_t b = blue;
    if (!parseRgbPayload(json, r, g, b)) return false;
    return onReceivedColorTopic(client, payload, length);
  }

  return false;
}

uint8_t HALight::discoveryStepCount() {
  return effectList.length() > 0 ? 5 : 4;
}

HAOperationResult HALight::discoveryStep(PubSubClient* client, uint8_t step) {
  char topic[HA_MAX_TOPIC_LENGTH];
  const bool hasEffects = effectList.length() > 0;
  if (step == 0) {
    getOnOffCommandTopic(topic);
    if (client->subscribe(topic)) return HAOperationResult::Done;
  } else if (step == 1) {
    getBrightnessCommandTopic(topic);
    if (client->subscribe(topic)) return HAOperationResult::Done;
  } else if (hasEffects && step == 2) {
    getEffectCommandTopic(topic);
    if (client->subscribe(topic)) return HAOperationResult::Done;
  } else if (step == (hasEffects ? 3 : 2)) {
    getColorCommandTopic(topic);
    if (client->subscribe(topic)) return HAOperationResult::Done;
  } else if (step == (hasEffects ? 4 : 3)) {
    char payload[HA_MAX_PAYLOAD_LENGTH];
    getConfigTopic(topic);
    getConfigPayload(payload, true, true);
    size_t len = strlen(payload);
    if (len > 0 && payload[len - 1] == '}') {
      payload[len - 1] = '\0';
      len--;
    }
    snprintf(
      payload + len,
      sizeof(payload) - len,
      ",\"brightness\":true,\"bri_cmd_t\":\"~/brightness/set\",\"bri_stat_t\":\"~/brightness/state\",\"bri_scl\":255"
    );
    len = strlen(payload);
    if (hasEffects) {
      snprintf(
        payload + len,
        sizeof(payload) - len,
        ",\"fx_cmd_t\":\"~/effect/set\",\"fx_stat_t\":\"~/effect/state\",\"fx_list\":[%s]",
        effectList.c_str()
      );
      len = strlen(payload);
    }
    snprintf(
      payload + len,
      sizeof(payload) - len,
      ",\"supported_color_modes\":[\"rgb\"],\"rgb_cmd_t\":\"~/color/set\",\"rgb_stat_t\":\"~/color/state\"}"
    );
    if (client->publish(topic, payload)) return HAOperationResult::Done;
  } else
    return HAOperationResult::Fatal;
  return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
}

uint8_t HALight::stateStepCount() const {
  return effectList.length() > 0 ? 4 : 3;
}

HAOperationResult HALight::stateStep(PubSubClient* client, uint8_t step, uint32_t expectedRevision) {
  char topic[HA_MAX_TOPIC_LENGTH];
  char payload[64];
  const bool hasEffects = effectList.length() > 0;
  bool published = false;
  if (step == 0) {
    getStateTopic(topic);
    published = client->publish(topic, state ? "ON" : "OFF");
  } else if (step == 1) {
    getBrightnessStateTopic(topic);
    snprintf(payload, sizeof(payload), "%d", brightness);
    published = client->publish(topic, payload);
  } else if (hasEffects && step == 2) {
    getEffectStateTopic(topic);
    published = client->publish(topic, effect);
  } else if (step == (hasEffects ? 3 : 2)) {
    getColorStateTopic(topic);
    snprintf(payload, sizeof(payload), "%d,%d,%d", red, green, blue);
    published = client->publish(topic, payload);
  } else
    return HAOperationResult::Fatal;
  if (!published) return client->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost;
  if (step + 1 == stateStepCount() && revision == expectedRevision) dirty = false;
  return HAOperationResult::Done;
}

#endif
