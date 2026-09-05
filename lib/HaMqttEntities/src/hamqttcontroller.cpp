#include "hamqttcontroller.h"
#include "hadevice.h"

#define HA_DELAY_RECONNECT 5000
#define HA_DELAY_SEND_STATES 3000
#define HA_RETRY_BASE_DELAY 100
#define HA_RETRY_MAX_DELAY 5000

const char* const HAMQTTController::topicHass PROGMEM = "homeassistant/status";

HAMQTTController::HAMQTTController() {
  this->mqttClient = NULL;
  this->entities = NULL;
  this->entityCounter = 0;
  this->entityCapacity = 0;
  this->registrationFailed = false;
  this->callback = NULL;
  this->callbackContext = NULL;
  this->lastWillDevice = NULL;
  resetScheduler(0);
  observability.discoveryPackets = 0;
  observability.statePackets = 0;
  observability.failedOperations = 0;
  observability.lastFailedEntity = 0;
  observability.lastFailedSubstep = 0;
  observability.lastFailedOperation = HAOperationKind::Discovery;
}

bool HAMQTTController::begin(PubSubClient& mqtt_client, HAEntity** registry, size_t capacity) {
  if (this->mqttClient != NULL || registry == NULL || capacity == 0) return false;

  this->mqttClient = &mqtt_client;
  this->entities = registry;
  this->entityCapacity = capacity;
  this->entityCounter = 0;
  this->registrationFailed = false;
  this->mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->mqttOnReceived(topic, payload, length);
  });
  if (
    this->mqttClient->getBufferSize() < HA_MAX_PAYLOAD_LENGTH && !this->mqttClient->setBufferSize(HA_MAX_PAYLOAD_LENGTH)
  )
    return false;
  return true;
}

bool HAMQTTController::addEntity(HAEntity& entity) {
  if (this->mqttClient == NULL || this->registrationFailed || this->entityCounter >= this->entityCapacity) {
    this->registrationFailed = true;
    return false;
  }
  entity.prepare();
  this->entities[this->entityCounter] = &entity;
  this->entityCounter++;
  if (entity.getDevice() != NULL && entity.getDevice()->getAvailability()) this->setLastWillDevice(*entity.getDevice());
  return true;
}

bool HAMQTTController::registrationComplete() const {
  return this->mqttClient != NULL && !this->registrationFailed && this->entityCounter == this->entityCapacity;
}

void HAMQTTController::setLastWillDevice(HADevice& device) {
  this->lastWillDevice = &device;
}

boolean HAMQTTController::connect(const char* id, const char* user, const char* pass) {
  if (this->mqttClient == NULL) return false;
  if (this->lastWillDevice != NULL) {
    char willTopic[HA_MAX_TOPIC_LENGTH];
    this->lastWillDevice->getAvailabilityTopic(willTopic);
    if (!this->mqttClient->connect(id, user, pass, willTopic, 0, false, "offline")) return false;
  } else if (!this->mqttClient->connect(id, user, pass))
    return false;
  if (this->mqttClient->connected()) startDiscovery(millis());
  return this->mqttClient->connected();
}

void HAMQTTController::resetScheduler(uint32_t now) {
  syncPhase = SyncPhase::Disconnected;
  phaseStartedAt = now;
  retryAt = 0;
  retryDelay = HA_RETRY_BASE_DELAY;
  cursorEntity = 0;
  cursorSubstep = 0;
  priorityEntity = entityCounter;
  prioritySubstep = 0;
  priorityRevision = 0;
  stateRevision = 0;
  stateCursorActive = false;
  priorityCursorActive = false;
  initialStatePass = false;
  homeAssistantStatusSubscribed = false;
  lastWillAvailabilityPending = false;
  observability.phase = syncPhase;
  observability.entityIndex = 0;
  observability.substep = 0;
}

void HAMQTTController::startDiscovery(uint32_t now) {
  syncPhase = SyncPhase::DiscoveryDelay;
  phaseStartedAt = now;
  retryAt = 0;
  retryDelay = HA_RETRY_BASE_DELAY;
  cursorEntity = 0;
  cursorSubstep = 0;
  priorityEntity = entityCounter;
  prioritySubstep = 0;
  priorityRevision = 0;
  stateCursorActive = false;
  priorityCursorActive = false;
  initialStatePass = false;
  homeAssistantStatusSubscribed = false;
  lastWillAvailabilityPending = lastWillDevice != NULL;
  observability.phase = syncPhase;
  observability.entityIndex = cursorEntity;
  observability.substep = cursorSubstep;
}

void HAMQTTController::startStatePass(bool initial) {
  syncPhase = initial ? SyncPhase::InitialState : SyncPhase::Online;
  cursorEntity = 0;
  cursorSubstep = 0;
  stateCursorActive = false;
  initialStatePass = initial;
  observability.phase = syncPhase;
  observability.entityIndex = cursorEntity;
  observability.substep = cursorSubstep;
}

void HAMQTTController::advanceCursor() {
  cursorSubstep++;
  if (cursorEntity >= entityCounter || cursorSubstep >= entities[cursorEntity]->discoveryStepCount()) {
    cursorEntity++;
    cursorSubstep = 0;
  }
  observability.entityIndex = cursorEntity;
  observability.substep = cursorSubstep;
}

void HAMQTTController::applyResult(
  HAOperationResult result, HAOperationKind operation, uint32_t now, size_t entityIndex, uint8_t substep
) {
  if (result == HAOperationResult::Done) {
    retryDelay = HA_RETRY_BASE_DELAY;
    retryAt = 0;
    if (operation == HAOperationKind::Discovery) observability.discoveryPackets++;
    else
      observability.statePackets++;
    return;
  }
  observability.failedOperations++;
  observability.lastFailedEntity = entityIndex;
  observability.lastFailedSubstep = substep;
  observability.lastFailedOperation = operation;
  if (result == HAOperationResult::RetryLater) {
    retryAt = now + retryDelay;
    retryDelay = retryDelay < HA_RETRY_MAX_DELAY / 2 ? retryDelay * 2 : HA_RETRY_MAX_DELAY;
  } else if (result == HAOperationResult::TransportLost) {
    resetScheduler(now);
  } else {
    syncPhase = SyncPhase::SyncError;
    observability.phase = syncPhase;
  }
}

bool HAMQTTController::runDiscovery(uint32_t now) {
  if (retryAt != 0 && now - retryAt > 0x80000000UL) return false;
  if (lastWillAvailabilityPending) {
    const bool published = lastWillDevice->sendAvailable(mqttClient, true);
    applyResult(
      published ? HAOperationResult::Done
                : (mqttClient->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost),
      HAOperationKind::Discovery,
      now,
      entityCounter,
      0
    );
    if (published) lastWillAvailabilityPending = false;
    return true;
  }
  if (!homeAssistantStatusSubscribed) {
    if (mqttClient->subscribe(topicHass)) {
      homeAssistantStatusSubscribed = true;
      applyResult(HAOperationResult::Done, HAOperationKind::Discovery, now, entityCounter, 0);
    } else
      applyResult(
        mqttClient->connected() ? HAOperationResult::RetryLater : HAOperationResult::TransportLost,
        HAOperationKind::Discovery,
        now,
        entityCounter,
        0
      );
    return true;
  }
  if (cursorEntity >= entityCounter) {
    syncPhase = SyncPhase::InitialStateDelay;
    phaseStartedAt = now;
    observability.phase = syncPhase;
    return false;
  }
  HAOperationResult result = entities[cursorEntity]->discoveryStep(mqttClient, cursorSubstep);
  applyResult(result, HAOperationKind::Discovery, now, cursorEntity, cursorSubstep);
  if (result == HAOperationResult::Done) advanceCursor();
  return true;
}

bool HAMQTTController::runPriorityState(uint32_t now) {
  if (retryAt != 0 && now - retryAt > 0x80000000UL) return false;
  if (priorityEntity >= entityCounter) return false;
  if (!priorityCursorActive) {
    if (entities[priorityEntity]->stateStepCount() == 0) {
      priorityEntity = entityCounter;
      return false;
    }
    prioritySubstep = 0;
    priorityRevision = entities[priorityEntity]->stateRevision();
    priorityCursorActive = true;
  }
  HAOperationResult result = entities[priorityEntity]->stateStep(mqttClient, prioritySubstep, priorityRevision);
  applyResult(result, HAOperationKind::State, now, priorityEntity, prioritySubstep);
  if (result != HAOperationResult::Done) return true;
  prioritySubstep++;
  if (prioritySubstep >= entities[priorityEntity]->stateStepCount()) {
    priorityEntity = entityCounter;
    prioritySubstep = 0;
    priorityCursorActive = false;
  }
  observability.entityIndex = priorityEntity;
  observability.substep = prioritySubstep;
  return true;
}

bool HAMQTTController::runState(uint32_t now) {
  if (retryAt != 0 && now - retryAt > 0x80000000UL) return false;
  if (runPriorityState(now)) return true;
  if (!stateCursorActive) {
    while (cursorEntity < entityCounter &&
           (entities[cursorEntity]->stateStepCount() == 0 || (!initialStatePass && !entities[cursorEntity]->isDirty())))
      cursorEntity++;
    if (cursorEntity >= entityCounter) {
      if (initialStatePass) {
        initialStatePass = false;
        syncPhase = SyncPhase::Online;
        cursorEntity = 0;
        observability.phase = syncPhase;
        observability.entityIndex = 0;
        observability.substep = 0;
      } else {
        cursorEntity = 0;
        observability.entityIndex = 0;
      }
      return false;
    }
    cursorSubstep = 0;
    stateRevision = entities[cursorEntity]->stateRevision();
    stateCursorActive = true;
  }
  HAOperationResult result = entities[cursorEntity]->stateStep(mqttClient, cursorSubstep, stateRevision);
  applyResult(result, HAOperationKind::State, now, cursorEntity, cursorSubstep);
  if (result != HAOperationResult::Done) return true;
  cursorSubstep++;
  if (cursorSubstep >= entities[cursorEntity]->stateStepCount()) {
    stateCursorActive = false;
    cursorEntity++;
    cursorSubstep = 0;
  }
  observability.entityIndex = cursorEntity;
  observability.substep = cursorSubstep;
  return true;
}

void HAMQTTController::queueState(size_t entityIndex) {
  if (entityIndex >= entityCounter) return;
  priorityEntity = entityIndex;
  prioritySubstep = 0;
  priorityRevision = 0;
  priorityCursorActive = false;
}

void HAMQTTController::onConnect() {
  startDiscovery(millis());
}

void HAMQTTController::sendAllStates() {
  requestFullSync();
}

void HAMQTTController::requestFullSync() {
  if (mqttClient == NULL || !mqttClient->connected() || syncPhase == SyncPhase::Disconnected) return;
  if (syncPhase == SyncPhase::SyncError) {
    startDiscovery(millis());
    return;
  }
  startStatePass(true);
}

void HAMQTTController::tick(uint32_t now) {
  if (mqttClient == NULL || !mqttClient->connected()) {
    resetScheduler(now);
    return;
  }
  if (!mqttClient->loop()) {
    resetScheduler(now);
    return;
  }
  switch (syncPhase) {
    case SyncPhase::Disconnected: return;
    case SyncPhase::DiscoveryDelay:
      if (now - phaseStartedAt >= HA_DELAY_RECONNECT) {
        syncPhase = SyncPhase::Discovery;
        observability.phase = syncPhase;
      }
      return;
    case SyncPhase::Discovery: runDiscovery(now); return;
    case SyncPhase::InitialStateDelay:
      if (now - phaseStartedAt >= HA_DELAY_SEND_STATES) startStatePass(true);
      return;
    case SyncPhase::InitialState:
    case SyncPhase::Online: runState(now); return;
    case SyncPhase::SyncError: return;
  }
}

bool HAMQTTController::mqttOnReceived(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, topicHass) == 0 && length == 6 && memcmp(payload, "online", 6) == 0) {
    startDiscovery(millis());
    return false;
  }
  for (size_t i = 0; i < this->entityCounter; i++) {
    HAEntity* entity = this->entities[i];
    if (entity->handleCommand(this->mqttClient, topic, payload, length)) {
      if (this->callback != NULL) this->callback(this->callbackContext, *entity, topic, payload, length);
      queueState(i);
      return true;
    }
  }
  return false;
}

void HAMQTTController::setCallback(void* context, HAMQTT_CALLBACK_SIGNATURE(callback)) {
  this->callbackContext = context;
  this->callback = callback;
}

void HAMQTTController::setAvailable(bool available, HADevice& device) {
  device.setAvailable(available);
  for (size_t i = 0; i < this->entityCounter; i++)
    if (this->entities[i]->getDevice() == &device) this->entities[i]->setAvailable(available);
}
