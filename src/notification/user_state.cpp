#include "user_state.h"

void UserNotificationState::start(UserNotificationType type, uint32_t durationMs) {
  _type = type;
  _startedMs = millis();
  _durationMs = durationMs;
}

void UserNotificationState::startText(const String& text, const CRGB& color, uint32_t durationMs) {
  _type = UserNotificationType::Text;
  _startedMs = millis();
  _durationMs = durationMs;
  _text = text;
  _color = color;
}

void UserNotificationState::stop() {
  _type = UserNotificationType::None;
  _startedMs = 0;
  _durationMs = 0;
  // _text не чистить: нужен для fade-out кадра
  _color = CRGB::White;
}

bool UserNotificationState::tick(uint32_t now) {
  if (_type == UserNotificationType::None) return false;
  if (_durationMs == 0) return false;
  if (now - _startedMs < _durationMs) return false;

  stop();
  return true;
}
