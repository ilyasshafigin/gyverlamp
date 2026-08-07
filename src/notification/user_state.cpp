#include "user_state.h"

void UserNotificationState::start(UserNotificationType type, uint32_t durationMs) {
  type_ = type;
  startedMs_ = millis();
  durationMs_ = durationMs;
}

void UserNotificationState::startText(const String& text, const CRGB& color, uint32_t durationMs) {
  type_ = UserNotificationType::Text;
  startedMs_ = millis();
  durationMs_ = durationMs;
  text_ = text;
  color_ = color;
}

void UserNotificationState::stop() {
  type_ = UserNotificationType::None;
  startedMs_ = 0;
  durationMs_ = 0;
  // text_ не чистить: нужен для fade-out кадра
  color_ = CRGB::White;
}

bool UserNotificationState::tick(uint32_t now) {
  if (type_ == UserNotificationType::None) return false;
  if (durationMs_ == 0) return false;
  if (now - startedMs_ < durationMs_) return false;

  stop();
  return true;
}
