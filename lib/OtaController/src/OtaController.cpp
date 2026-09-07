#include "OtaController.h"

#include <Arduino.h>

#ifdef USE_OTA
#include <ArduinoOTA.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <Updater.h>
#endif
#endif

namespace {

#if defined(USE_OTA) && defined(ARDUINO_ARCH_ESP8266)
  void logOtaSuccess(unsigned long elapsedMs, uint8_t progress) {
    Serial.printf(
      "[OTA] success elapsed=%lums progress=%u%% rssi=%ddBm heap=%u max=%u frag=%u%%\n",
      elapsedMs,
      progress,
      WiFi.RSSI(),
      ESP.getFreeHeap(),
      ESP.getMaxFreeBlockSize(),
      ESP.getHeapFragmentation()
    );
  }

  void logOtaError(ota_error_t error, unsigned long elapsedMs, uint8_t progress) {
    Serial.printf(
      "[OTA] error code=%u elapsed=%lums progress=%u%% rssi=%ddBm heap=%u max=%u frag=%u%%\n",
      static_cast<unsigned int>(error),
      elapsedMs,
      progress,
      WiFi.RSSI(),
      ESP.getFreeHeap(),
      ESP.getMaxFreeBlockSize(),
      ESP.getHeapFragmentation()
    );

    if (Update.getError() != 0) Update.printError(Serial);
  }
#elif defined(USE_OTA)
  void logOtaSuccess(unsigned long elapsedMs, uint8_t progress) {
    Serial.printf("[OTA] success elapsed=%lums progress=%u%%\n", elapsedMs, progress);
  }

  void logOtaError(ota_error_t error, unsigned long elapsedMs, uint8_t progress) {
    Serial.printf(
      "[OTA] error code=%u elapsed=%lums progress=%u%%\n", static_cast<unsigned int>(error), elapsedMs, progress
    );
  }
#endif

} // namespace

void OtaController::copyString(char* destination, uint8_t capacity, const char* source) {
  if (capacity == 0) return;

  uint8_t index = 0;
  if (source) {
    while (source[index] && index + 1 < capacity) {
      destination[index] = source[index];
      ++index;
    }
  }
  destination[index] = '\0';
}

void OtaController::begin(const Config& config, EventHandler eventHandler, void* context) {
  if (initialized_) return;

  copyString(hostname_, sizeof(hostname_), config.hostname);
  copyString(password_, sizeof(password_), config.password);
  copyString(passwordHash_, sizeof(passwordHash_), config.passwordHash);
  port_ = config.port == 0 ? 8266 : config.port;
  desiredEnabled_ = config.enabled;
  effectiveEnabled_ = config.enabled;
  eventHandler_ = eventHandler;
  eventContext_ = context;
  initialized_ = true;

#ifdef USE_OTA
  ArduinoOTA.setHostname(hostname_);
  ArduinoOTA.setPort(port_);
  if (passwordHash_[0]) {
    ArduinoOTA.setPasswordHash(passwordHash_);
  } else if (password_[0]) {
    ArduinoOTA.setPassword(password_);
  }

  ArduinoOTA.onStart([this]() {
    updating_ = true;
    lastProgressCallbackAt_ = 0;
    lastProgressPercent_ = 0xFF;
    uploadStartedAt_ = millis();
    finalProgressPercent_ = 0;
    emit(EventType::Start);
  });

  ArduinoOTA.onEnd([this]() {
    logOtaSuccess(millis() - uploadStartedAt_, finalProgressPercent_);
    updating_ = false;
    emit(EventType::End);
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const uint8_t percent = total == 0 ? 0 : static_cast<uint8_t>((static_cast<uint64_t>(progress) * 100U) / total);
    finalProgressPercent_ = percent;
    const unsigned long now = millis();
    if (
      percent == lastProgressPercent_ ||
      (percent != 100 && lastProgressCallbackAt_ != 0 && now - lastProgressCallbackAt_ < kProgressIntervalMs)
    ) {
      return;
    }

    lastProgressPercent_ = percent;
    lastProgressCallbackAt_ = now;
    emit(EventType::Progress, percent);
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    logOtaError(error, millis() - uploadStartedAt_, finalProgressPercent_);
    updating_ = false;
    emit(EventType::Error, 0, static_cast<uint8_t>(error));
  });
#endif
}

void OtaController::tick(bool staConnected) {
#ifdef USE_OTA
  if (updating_) {
    ArduinoOTA.handle();
    if (updating_) return;
  }

  if (enableRequestPending_) {
    enableRequestPending_ = false;
    effectiveEnabled_ = desiredEnabled_;
  }

  if (!effectiveEnabled_ || !staConnected) {
    stopListener();
    restartRequested_ = false;
    return;
  }

  if (restartRequested_) {
    stopListener();
    restartRequested_ = false;
  }

  const unsigned long now = millis();
  if (!listenerStartIssued_ || now - lastBeginAttemptAt_ >= kBeginRetryIntervalMs) {
#if defined(ARDUINO_ARCH_ESP8266)
    ArduinoOTA.begin(false);
#elif defined(ARDUINO_ARCH_ESP32)
    ArduinoOTA.setMdnsEnabled(false);
    ArduinoOTA.begin();
#else
    ArduinoOTA.begin();
#endif
    listenerStartIssued_ = true;
    lastBeginAttemptAt_ = millis();
  }

  ArduinoOTA.handle();
#else
  (void)staConnected;
#endif
}

void OtaController::requestEnabled(bool enabled) {
#ifdef USE_OTA
  desiredEnabled_ = enabled;
  enableRequestPending_ = true;
#else
  (void)enabled;
#endif
}

void OtaController::requestRestart() {
#ifdef USE_OTA
  if (desiredEnabled_) restartRequested_ = true;
#endif
}

bool OtaController::isEnabled() const {
#ifdef USE_OTA
  return desiredEnabled_;
#else
  return false;
#endif
}

OtaController::State OtaController::state() const {
#ifdef USE_OTA
  if (updating_) return State::Updating;
  if (!effectiveEnabled_) return State::Disabled;
  if (!listenerStartIssued_) return State::WaitingForSta;
  return State::ListenerStartIssued;
#else
  return State::Disabled;
#endif
}

const char* OtaController::stateName() const {
  switch (state()) {
    case State::Disabled: return "Disabled";
    case State::WaitingForSta: return "Waiting for STA";
    case State::ListenerStartIssued: return "OTA listener start requested";
    case State::Updating: return "Updating";
  }
  return "Disabled";
}

void OtaController::stopListener() {
#ifdef USE_OTA
  if (listenerStartIssued_) ArduinoOTA.end();
#endif
  listenerStartIssued_ = false;
  lastBeginAttemptAt_ = 0;
}

void OtaController::emit(EventType type, uint8_t progress, uint8_t errorCode) {
  if (eventHandler_) eventHandler_(Event{type, progress, errorCode}, eventContext_);
}
