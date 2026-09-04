#pragma once

#ifdef USE_MQTT
#include <Arduino.h>
#include <HaMqttEntities.h>
#include <PubSubClient.h>

#include "mqtt_state.h"

class MqttBridge {
public:
  virtual ~MqttBridge() = default;

  virtual uint8_t entityCount() const = 0;
  virtual void registerEntities() = 0;
  virtual void
  dispatchMessage(PubSubClient& client, HAEntity* entity, char* topic, byte* payload, unsigned int length) = 0;
  virtual void fullRefresh() = 0;
  virtual void onTransportState(MqttState state) = 0;
  virtual void onTransportFailure() = 0;
};
#endif
