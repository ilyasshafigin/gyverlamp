#pragma once

#include "haentity.h"
#include <PubSubClient.h>
#include <stddef.h>

/* Callback signature:
    void callback(void*, HAEntity&, char* topic, byte* payload, size_t payload_length)

    Arguments are borrowed for callback duration. Callback must not recursively
    call controller lifecycle or loop APIs.
*/
#define HAMQTT_CALLBACK_SIGNATURE(f_name) void (*f_name)(void*, HAEntity&, char*, byte*, size_t)

class HAMQTTController {
public:
  enum class SyncPhase : uint8_t {
    Disconnected,
    DiscoveryDelay,
    Discovery,
    InitialStateDelay,
    InitialState,
    Online,
    SyncError
  };
  struct Observability {
    SyncPhase phase;
    size_t entityIndex;
    uint8_t substep;
    uint16_t discoveryPackets;
    uint16_t statePackets;
    uint16_t failedOperations;
    size_t lastFailedEntity;
    uint8_t lastFailedSubstep;
    HAOperationKind lastFailedOperation;
  };

private:
  static const char* const topicHass;
  HAMQTT_CALLBACK_SIGNATURE(callback);
  void* callbackContext;

  uint32_t phaseStartedAt;
  uint32_t retryAt;
  uint16_t retryDelay;
  HADevice* lastWillDevice;

protected:
  PubSubClient* mqttClient;
  HAEntity** entities;
  size_t entityCapacity;
  size_t entityCounter;
  bool registrationFailed;
  SyncPhase syncPhase;
  size_t cursorEntity;
  uint8_t cursorSubstep;
  size_t priorityEntity;
  uint8_t prioritySubstep;
  uint32_t priorityRevision;
  uint32_t stateRevision;
  bool stateCursorActive;
  bool priorityCursorActive;
  bool initialStatePass;
  bool homeAssistantStatusSubscribed;
  bool lastWillAvailabilityPending;
  Observability observability;

  void resetScheduler(uint32_t now);
  void startDiscovery(uint32_t now);
  void startStatePass(bool initial);
  void advanceCursor();
  void
  applyResult(HAOperationResult result, HAOperationKind operation, uint32_t now, size_t entityIndex, uint8_t substep);
  bool runDiscovery(uint32_t now);
  bool runPriorityState(uint32_t now);
  bool runState(uint32_t now);
  void queueState(size_t entityIndex);

public:
  HAMQTTController();
  ~HAMQTTController() = default;
  HAMQTTController(const HAMQTTController&) = delete;
  HAMQTTController& operator=(const HAMQTTController&) = delete;
  HAMQTTController(HAMQTTController&&) = delete;
  HAMQTTController& operator=(HAMQTTController&&) = delete;

  bool begin(PubSubClient& mqtt_client, HAEntity** registry, size_t capacity);
  bool addEntity(HAEntity& entity);
  bool registrationComplete() const;
  void setLastWillDevice(HADevice& device);

  /** Connect to the MQTT broker using the PubSubClient instance.
         *
         * On success, it calls onConnect. Using this method is optional,
         * but if it is not used, after connecting to MQTT, the onConnect
         * method must be called manually.
         */
  boolean connect(const char* id, const char* user, const char* pass);
  inline boolean connected() { return this->mqttClient != NULL && this->mqttClient->connected(); }
  void onConnect();
  void tick(uint32_t now);
  void loop() { tick(millis()); }
  void sendAllStates();
  void requestFullSync();
  const Observability& getObservability() const { return observability; }

  // It must be called when a mqtt message is received
  // return true if the message is for one of the components
  bool mqttOnReceived(char* topic, byte* payload, unsigned int length);

  /**
         * Set the MQTT callback
         *
         * When is not set the messages are only processed in each
         * component in the mqttOnReceived method
         *
         * When is set, first the message is processed in each component
         * in the mqttOnReceived method and then the callback is called
         *
         */
  void setCallback(void* context, HAMQTT_CALLBACK_SIGNATURE(callback));

  void setAvailable(bool available, HADevice& device);
};
