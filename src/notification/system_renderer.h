#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "overlay.h"
#include "types.h"

class SystemNotificationRenderer {
public:
  explicit SystemNotificationRenderer() {}

  void renderWifi(NotificationOverlay& overlay, ConnectionState state, uint32_t startedMs);
  void renderMqtt(NotificationOverlay& overlay, ConnectionState state, uint32_t startedMs);
  void renderOta(NotificationOverlay& overlay, OtaState state, uint8_t percent, uint32_t startedMs);
};
