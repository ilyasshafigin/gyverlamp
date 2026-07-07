#pragma once

#include <Arduino.h>
#include <FastLED.h>

enum class ConnectionState : uint8_t {
  Idle,
  Connecting,
  Connected,
  Error,
  Disabled,
};

enum class OtaState : uint8_t {
  Idle,
  Running,
  Success,
  Error,
};

enum class UserNotificationType : uint8_t {
  None,
  Notify,
  Text,
  Warning,
  Alarm,
};

enum class IndicatorType : uint8_t {
  None,
  PowerOn,
  PowerOff,
  Dismiss,
  NextEffect,
  PreviousEffect,
  Brightness,
  RotationOn,
  RotationOff,
};

enum class NotificationSource : uint8_t {
  None,
  User,
  Button,
  Wifi,
  Mqtt,
  Ota,
};

struct NotificationSnapshot {
  NotificationSource source = NotificationSource::None;

  UserNotificationType userType = UserNotificationType::None;
  ConnectionState connectionState = ConnectionState::Idle;
  OtaState otaState = OtaState::Idle;

  uint32_t startedMs = 0;
  uint32_t durationMs = 0;
  uint8_t targetDim = 0;
  uint8_t otaPercent = 0;

  const String* text = nullptr;
  CRGB color = CRGB::White;

  IndicatorType indicatorType = IndicatorType::None;
  uint8_t buttonValue = 0;     // brightness 0..255
  bool buttonDirection = true; // true = clockwise/up, false = counter/down
  uint8_t buttonPressCount = 0;
  uint32_t buttonPressMs = 0;
  bool buttonPressing = false;

  bool isActive() const { return source != NotificationSource::None; }
  bool isTimed() const { return durationMs > 0; }
};

struct NotificationFrame {
  NotificationSnapshot snapshot;
  uint8_t opacity = 0;
  uint8_t backdropDim = 0;

  bool isVisible() const { return snapshot.isActive() && opacity > 0; }
};
