#include "OtaController.h"

#include <Arduino.h>

#ifdef USE_OTA
#include <ArduinoOTA.h>
#endif

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
    Serial.println("[OTA] OTA Start");
    updating_ = true;
    lastProgressCallbackAt_ = 0;
    lastProgressPercent_ = 0xFF;
    emit(EventType::Start);
  });

  ArduinoOTA.onEnd([this]() {
    Serial.println("[OTA] OTA End");
    updating_ = false;
    emit(EventType::End);
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const uint8_t percent = total == 0 ? 0 : static_cast<uint8_t>((static_cast<uint64_t>(progress) * 100U) / total);
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
    Serial.printf("[OTA] Progress: %u%%\n\r", percent);
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    Serial.printf("[OTA] OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("[OTA] Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("[OTA] Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("[OTA] Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("[OTA] Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("[OTA] End Failed");

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
