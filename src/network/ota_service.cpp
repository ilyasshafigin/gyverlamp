#include "ota_service.h"

#ifdef USE_OTA

#include <ArduinoOTA.h>

#include "../core/frame_renderer.h"
#include "../core/power_controller.h"
#include "../notification/controller.h"
#include "wifi_service.h"

void OtaService::init() {
  ArduinoOTA.onStart([this]() {
    Serial.println("[OTA] OTA Start");

    notifications_.onOtaStart();
    frameRenderer_.renderNow();
  });

  ArduinoOTA.onEnd([this]() {
    Serial.println("[OTA] OTA End");

    notifications_.onOtaEnd();
    frameRenderer_.renderNow();
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const unsigned int percent =
      total == 0 ? 0 : static_cast<unsigned int>((static_cast<uint64_t>(progress) * 100U) / total);

    notifications_.onOtaProgress(percent);
    frameRenderer_.render();

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

    notifications_.onOtaError();
    frameRenderer_.renderNow();
  });
}

void OtaService::tick() {
  if (!begun_) {
    if (!wifi_.isStaConnected()) {
      return;
    }
    ArduinoOTA.begin();
    begun_ = true;
  }
  ArduinoOTA.handle();
}

#else

void OtaService::init() {
}
void OtaService::tick() {
}

#endif
