#include "../config.h"
#include "../text/running_text.h"
#include "render_utils.h"
#include "user_renderer.h"

void UserNotificationRenderer::render(NotificationOverlay& overlay, const NotificationSnapshot& notification) {
  if (notification.userType == UserNotificationType::Alarm) {
    renderAlarm(overlay, notification.startedMs);
  } else if (notification.userType == UserNotificationType::Warning) {
    renderWarning(overlay, notification.startedMs);
  } else if (notification.userType == UserNotificationType::Notify) {
    renderNotify(overlay, notification.startedMs);
  } else if (notification.userType == UserNotificationType::Text && notification.text) {
    renderText(overlay, *notification.text, notification.color, notification.startedMs);
  }
}

void UserNotificationRenderer::renderAlarm(NotificationOverlay& overlay, uint32_t startedMs) {
  overlay.drawFullPulse(CRGB::Red, startedMs, 900, 20, 220);
}

void UserNotificationRenderer::renderWarning(NotificationOverlay& overlay, uint32_t startedMs) {
  overlay.drawFullPulse(CRGB(255, 120, 0), startedMs, 1800, 10, 150);
}

void UserNotificationRenderer::renderNotify(NotificationOverlay& overlay, uint32_t startedMs) {
  overlay.drawVerticalWave(CRGB(120, 210, 255), startedMs, 1800, 0, 250);
}

void UserNotificationRenderer::renderText(
  NotificationOverlay& overlay, const String& text, const CRGB& color, uint32_t startedMs
) {
  if (_lastText != text || _lastTextStartedMs != startedMs) {
    _lastText = text;
    _lastTextStartedMs = startedMs;
    _runningText.start(text, color, true);
  }

  _runningText.render(overlay);
}
