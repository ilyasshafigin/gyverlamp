#pragma once

#include "hasensor.h"

class PubSubClient;
class HADevice;

class HASensorText : public HASensor {
protected:
  char* state;
  int maxSize;

public:
  HASensorText(const char* unique_id, const char* name, HADevice& device, int max_size);
  HASensorText(const char* unique_id, const char* name, int max_size);

  const char* getState() { return this->state; };
  void setState(const char* state);
  bool onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) {
    (void)client;
    (void)payload;
    (void)length;
    return false;
  }

  bool sendState(PubSubClient* client) override;
};
