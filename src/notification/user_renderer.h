#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "overlay.h"
#include "types.h"
#include "user_state.h"

class RunningText;

class UserNotificationRenderer {
public:
  explicit UserNotificationRenderer(
    RunningText& runningText
  ) :
    _runningText(runningText) {
  }

  void render(NotificationOverlay& overlay, const NotificationSnapshot& notification);

private:
  RunningText& _runningText;
  String _lastText;
  uint32_t _lastTextStartedMs = 0;

  void renderAlarm(NotificationOverlay& overlay, uint32_t startedMs);
  void renderWarning(NotificationOverlay& overlay, uint32_t startedMs);
  void renderNotify(NotificationOverlay& overlay, uint32_t startedMs);
  void renderText(NotificationOverlay& overlay, const String& text, const CRGB& color, uint32_t startedMs);
};
