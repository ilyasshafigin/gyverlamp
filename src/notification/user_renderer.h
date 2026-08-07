#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "overlay.h"
#include "types.h"
#include "user_state.h"

class RunningText;

class UserNotificationRenderer {
public:
  explicit UserNotificationRenderer(RunningText& runningText)
    : runningText_(runningText) {}

  void render(NotificationOverlay& overlay, const NotificationSnapshot& notification);

private:
  RunningText& runningText_;
  String lastText_;
  uint32_t lastTextStartedMs_ = 0;

  void renderAlarm(NotificationOverlay& overlay, uint32_t startedMs);
  void renderWarning(NotificationOverlay& overlay, uint32_t startedMs);
  void renderNotify(NotificationOverlay& overlay, uint32_t startedMs);
  void renderText(NotificationOverlay& overlay, const String& text, const CRGB& color, uint32_t startedMs);
};
