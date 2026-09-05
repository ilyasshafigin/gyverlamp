#pragma once

#include "haentity.h"

class PubSubClient;
class HADevice;
/** Base class for sensor, it may not instantiate */
class HASensor : public HAEntity {
protected:
  static const char* const component;
  bool dirty;

public:
  HASensor(const char* unique_id, const char* name, const char* component = HASensor::component);
  inline bool isDirty() { return this->dirty; };
  char* getCommandTopic(char* buffer) override {
    (void)buffer;
    return NULL;
  }
  virtual void onConnect(PubSubClient* client);
  virtual bool onReceivedTopic(PubSubClient*, byte* payload, unsigned int length) {
    (void)payload;
    (void)length;
    return false;
  }
};
