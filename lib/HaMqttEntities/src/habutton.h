#pragma once

#include <haentity.h>
#include <string.h>

class PubSubClient;
class HADevice;

/**
 * @brief Button entity for Home Assistant
 *
 *  Button does not store the state, the event must be handled in the callback
 *  or in the derived class method onStateChange
 *
 *  HA send by default the string "PRESS", but this class not check the payload,
 *  it assumes that the payload is "PRESS"
 */

class HAButton : public HAEntity {
protected:
  static const char* const component;

public:
  HAButton(const char* unique_id, const char* name, HADevice& device);
  HAButton(const char* unique_id, const char* name);

  void onConnect(PubSubClient* client);
  bool discoveryHasStateTopic() const override { return false; }
  uint8_t stateStepCount() const override { return 0; }
  bool onReceivedTopic(PubSubClient* client, byte* payload, unsigned int length) {
    (void)client;
    return length == 5 && memcmp(payload, "PRESS", 5) == 0;
  }
};
