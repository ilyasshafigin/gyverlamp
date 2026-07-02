#include "ota_service.h"

#ifdef USE_OTA

#include <ArduinoOTA.h>

#include "../core/frame_renderer.h"
#include "../core/power_controller.h"
#include "../notification/controller.h"

void OtaService::init() {
  ArduinoOTA.onStart([this]() {
    Serial.println("[OTA] OTA Start");

    _notifications.onOtaStart();
    _frameRenderer.renderNow();
    });

  ArduinoOTA.onEnd([this]() {
    Serial.println("[OTA] OTA End");

    _notifications.onOtaEnd();
    _frameRenderer.renderNow();
    });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const unsigned int percent = total == 0
      ? 0
      : static_cast<unsigned int>((static_cast<uint64_t>(progress) * 100U) / total);

    _notifications.onOtaProgress(percent);
    _frameRenderer.render();

    Serial.printf("[OTA] Progress: %u%%\n\r", percent);
    });

  ArduinoOTA.onError([this](ota_error_t error) {
    Serial.printf("[OTA] OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("[OTA] Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("[OTA] Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("[OTA] Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("[OTA] Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("[OTA] End Failed");

    _notifications.onOtaError();
    _frameRenderer.renderNow();
    });

  ArduinoOTA.begin();
}

void OtaService::tick() {
  ArduinoOTA.handle();
}

#else

void OtaService::init() {}
void OtaService::tick() {}

#endif
