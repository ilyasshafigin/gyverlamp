#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "types.h"

class UserNotificationState {
public:
  explicit UserNotificationState() {}

  void start(UserNotificationType type, uint32_t durationMs = 0);
  void startText(const String& text, const CRGB& color, uint32_t durationMs = 0);
  void stop();

  // Возвращает true, если состояние изменилось из-за timeout.
  bool tick(uint32_t now);

  bool isActive() const { return _type != UserNotificationType::None; }
  bool isNotifyActive() const { return _type == UserNotificationType::Notify; }
  bool isWarningActive() const { return _type == UserNotificationType::Warning; }
  bool isAlarmActive() const { return _type == UserNotificationType::Alarm; }
  bool isTextActive() const { return _type == UserNotificationType::Text; }
  bool isAlertActive() const { return isWarningActive() || isAlarmActive(); }

  UserNotificationType getType() const { return _type; }
  uint32_t getStartedMs() const { return _startedMs; }
  uint32_t getDurationMs() const { return _durationMs; }
  const String& getText() const { return _text; }
  const CRGB& getColor() const { return _color; }
  bool isTimed() const { return _durationMs > 0; }

private:
  UserNotificationType _type = UserNotificationType::None;
  uint32_t _startedMs = 0;
  uint32_t _durationMs = 0;
  String _text;
  CRGB _color = CRGB::White;
};
