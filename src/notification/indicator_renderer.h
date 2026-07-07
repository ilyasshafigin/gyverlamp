#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "overlay.h"
#include "types.h"

class IndicatorRenderer {
public:
  explicit IndicatorRenderer() {}

  void render(NotificationOverlay& overlay, const NotificationSnapshot& notification);

private:
  void renderPowerOn(NotificationOverlay& overlay, uint32_t startedMs);
  void renderDismiss(NotificationOverlay& overlay, uint32_t startedMs);
  void renderBrightness(NotificationOverlay& overlay, uint8_t brightness, bool increasing, uint32_t startedMs);
  void renderSwitchSweep(NotificationOverlay& overlay, uint32_t startedMs, bool clockwise, const CRGB& color);
  void renderRotationDots(NotificationOverlay& overlay, uint32_t startedMs, bool clockwise, const CRGB& color);
  void renderPressEcho(NotificationOverlay& overlay, uint8_t count, uint32_t pressMs, bool pressing, const CRGB& color);
};
