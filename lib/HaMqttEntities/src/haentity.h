/*
    Warning:  Not use ids (unique_id,device_id) with more than 50 characters
*/
#pragma once

#include <Arduino.h>
#include "hakvpairlist.h"

#ifndef HA_MAX_TOPIC_LENGTH
#define HA_MAX_TOPIC_LENGTH 128
#endif
#ifndef HA_MAX_PAYLOAD_LENGTH
#define HA_MAX_PAYLOAD_LENGTH 1536
#endif

class PubSubClient;
class HADevice;

enum class HAOperationResult : uint8_t { Done, RetryLater, TransportLost, Fatal };
enum class HAOperationKind : uint8_t { Discovery, State };

/** Abstract class for HA entities

*/
class HAEntity {
protected:
  static const char* const configTopicTemplate;
  static const char* const configPayloadTemplate;
  static const char* const availabilityTopicTemplate;
  static const char* const featureKeys[];

  static const char* const commandTopicTemplate;
  static const char* const stateTopicTemplate;

  HADevice* device;
  const char* id;
  char* uniqueId; /** Derived from id and device */
  const char* name;
  const char* component;
  HAKVPairList* features;
  int8_t available;

  char* getConfigTopic(char* buffer);
  char* getConfigPayload(char* buffer, bool add_command_topic, bool add_state_topic);

  /// Build default state topic
  char* getStateTopic(char* buffer);

public:
  HAEntity(const char* unique_id, const char* name, const char* component, HADevice* device = NULL);

  const char* getUniqueId();
  void prepare();
  inline HADevice* getDevice() { return device; }

  /// Some entities do not have a command topic and returns NULL
  virtual char* getCommandTopic(char* buffer);
  virtual bool handleCommand(PubSubClient*, char* topic, byte* payload, size_t length);
  virtual uint8_t discoveryStepCount();
  virtual HAOperationResult discoveryStep(PubSubClient*, uint8_t step);
  virtual uint8_t stateStepCount() const { return discoveryHasStateTopic() ? 1 : 0; }
  virtual uint32_t stateRevision() const { return 0; }
  virtual HAOperationResult stateStep(PubSubClient*, uint8_t step, uint32_t revision);
  virtual bool discoveryHasStateTopic() const { return true; }
  virtual bool sendState(PubSubClient*) { return true; }
  virtual void setState() { ; }

  /// Flag used in controller indicating if the state has changed
  /// and needs to be sent to the broker
  virtual bool isDirty() { return false; }

  virtual void onConnect(PubSubClient*) = 0;
  virtual bool onReceivedTopic(PubSubClient*, byte* payload, unsigned int length) = 0;

  /// Action to be performed when the state changes by default it does nothing
  /// It can be redefined in the derived class
  virtual void onStateChange() { ; }

  /// Add predefined feature to the entity
  void addFeature(int key, const char* value = NULL);

  /// Add not predefined feature to the entity
  void addFeature(const char* key, const char* value);

  /// It only works if previously the availability feature has been added
  void setAvailable(bool available);
  void sendAvailable(PubSubClient*, bool force = false);
  char* getAvailabilityTopic(char* buffer);
};
