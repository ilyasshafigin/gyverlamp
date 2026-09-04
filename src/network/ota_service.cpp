#include "ota_service.h"

#ifdef USE_OTA

#include <stdint.h>

#include <ArduinoOTA.h>

void OtaService::init(const char* hostname, bool enabled) {
  enabled_ = enabled;

  ArduinoOTA.setHostname(hostname);

  ArduinoOTA.onStart([this]() {
    Serial.println("[OTA] OTA Start");
    updating_ = true;
    lastProgressCallbackAt_ = 0;
    lastProgressPercent_ = 0xFF;

    if (startCallback_) startCallback_();
  });

  ArduinoOTA.onEnd([this]() {
    Serial.println("[OTA] OTA End");
    updating_ = false;

    if (endCallback_) endCallback_();
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

    if (progressCallback_) progressCallback_(percent);

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

    if (errorCallback_) errorCallback_();
  });
}

void OtaService::tick(bool isStaConnected) {
  if (updating_) {
    ArduinoOTA.handle();
    if (updating_) return;
  }

  if (enableRequestPending_) {
    enableRequestPending_ = false;
    enabled_ = requestedEnabled_;
  }

  if (!enabled_ || !isStaConnected) {
    stopListener();
    restartRequested_ = false;
    return;
  }

  if (restartRequested_) {
    stopListener();
    restartRequested_ = false;
  }

  const unsigned long now = millis();
  if (!beginAttempted_ || now - lastBeginAttemptAt_ >= kBeginRetryIntervalMs) {
    ArduinoOTA.begin(false);
    beginAttempted_ = true;
    listenerActive_ = true;
    lastBeginAttemptAt_ = millis();
  }

  ArduinoOTA.handle();
}

bool OtaService::isEnabled() const {
  return enabled_;
}

void OtaService::stopListener() {
  if (listenerActive_) ArduinoOTA.end();

  beginAttempted_ = false;
  listenerActive_ = false;
  lastBeginAttemptAt_ = 0;
}

OtaService::State OtaService::state() const {
  if (updating_) return State::Updating;
  if (!enabled_) return State::Disabled;
  if (!listenerActive_) return State::WaitingForSta;
  return State::Listening;
}

void OtaService::requestEnabled(bool enabled) {
  requestedEnabled_ = enabled;
  enableRequestPending_ = true;
}

void OtaService::requestRestart() {
  if (enabled_) restartRequested_ = true;
}

void OtaService::setStartHandler(VoidCallback callback) {
  startCallback_ = callback;
}

void OtaService::setProgressHandler(ProgressCallback callback) {
  progressCallback_ = callback;
}

void OtaService::setEndHandler(VoidCallback callback) {
  endCallback_ = callback;
}

void OtaService::setErrorHandler(VoidCallback callback) {
  errorCallback_ = callback;
}

#else

void OtaService::init(const char*, bool) {
}

void OtaService::tick(bool) {
}

bool OtaService::isEnabled() const {
  return false;
}

void OtaService::requestEnabled(bool) {
}

void OtaService::requestRestart() {
}

OtaService::State OtaService::state() const {
  return State::Disabled;
}

void OtaService::setStartHandler(VoidCallback) {
}

void OtaService::setProgressHandler(ProgressCallback) {
}

void OtaService::setEndHandler(VoidCallback) {
}

void OtaService::setErrorHandler(VoidCallback) {
}

#endif

const char* OtaService::stateName() const {
  switch (state()) {
    case State::Disabled: return "Disabled";
    case State::WaitingForSta: return "Waiting for STA";
    case State::Listening: return "Listening";
    case State::Updating: return "Updating";
  }
  return "Disabled";
}
