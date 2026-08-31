#include "ota_service.h"

#include <stdint.h>

#ifdef USE_OTA

#include <ArduinoOTA.h>

void OtaService::init(const char* hostname) {
  ArduinoOTA.setHostname(hostname);

  ArduinoOTA.onStart([this]() {
    Serial.println("[OTA] OTA Start");

    if (startCallback_) startCallback_();
  });

  ArduinoOTA.onEnd([this]() {
    Serial.println("[OTA] OTA End");

    if (endCallback_) endCallback_();
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const unsigned int percent =
      total == 0 ? 0 : static_cast<unsigned int>((static_cast<uint64_t>(progress) * 100U) / total);

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

    if (errorCallback_) errorCallback_();
  });
}

void OtaService::tick(bool isStaConnected) {
  if (!isStaConnected) {
    if (beginAttempted_) {
      ArduinoOTA.end();
    }
    beginAttempted_ = false;
    lastBeginAttemptAt_ = 0;
    return;
  }

  const unsigned long now = millis();
  if (!beginAttempted_ || now - lastBeginAttemptAt_ >= kBeginRetryIntervalMs) {
    ArduinoOTA.begin(false);
    beginAttempted_ = true;
    lastBeginAttemptAt_ = millis();
  }

  ArduinoOTA.handle();
}

#else

void OtaService::init(const char*) {
}
void OtaService::tick(bool) {
}

#endif
