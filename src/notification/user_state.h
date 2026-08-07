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

  bool isActive() const { return type_ != UserNotificationType::None; }
  bool isNotifyActive() const { return type_ == UserNotificationType::Notify; }
  bool isWarningActive() const { return type_ == UserNotificationType::Warning; }
  bool isAlarmActive() const { return type_ == UserNotificationType::Alarm; }
  bool isTextActive() const { return type_ == UserNotificationType::Text; }
  bool isAlertActive() const { return isWarningActive() || isAlarmActive(); }

  UserNotificationType getType() const { return type_; }
  uint32_t getStartedMs() const { return startedMs_; }
  uint32_t getDurationMs() const { return durationMs_; }
  const String& getText() const { return text_; }
  const CRGB& getColor() const { return color_; }
  bool isTimed() const { return durationMs_ > 0; }

private:
  UserNotificationType type_ = UserNotificationType::None;
  uint32_t startedMs_ = 0;
  uint32_t durationMs_ = 0;
  String text_;
  CRGB color_ = CRGB::White;
};
