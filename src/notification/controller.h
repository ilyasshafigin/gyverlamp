#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "../config.h"
#include "../util/fade_animator.h"
#include "button_renderer.h"
#include "quiet_hours.h"
#include "system_renderer.h"
#include "types.h"
#include "user_renderer.h"
#include "user_state.h"

class EepromStore;
class Led;
class NotificationOverlay;
class PowerController;
class RunningText;
class StateNotifier;
class TimeService;

class NotificationController {
public:
  explicit NotificationController(
    EepromStore& eeprom,
    PowerController& power,
    RunningText& runningText,
    StateNotifier& stateNotifier,
    TimeService& time
  )
    : _eeprom(eeprom),
      _power(power),
      _stateNotifier(stateNotifier),
      _time(time),
      _systemRenderer(),
      _userRenderer(runningText),
      _buttonRenderer(),
      _userState() {}

  void init();
  void tick();

  const NotificationFrame& frame() const { return _frame; }

  void renderOverlay(NotificationOverlay& overlay, const NotificationFrame& frame);

  bool isActive() const;
  bool isUserNotificationActive() const { return _userState.isActive(); }
  UserNotificationType getUserNotificationType() const { return _userState.getType(); }
  uint32_t getUserNotificationRemainingSeconds() const;

  const NotificationQuietHours& getQuietHours() const { return _quietHours; }
  bool setQuietHours(const NotificationQuietHours& settings);
  bool isMutedNow() const;

  void startUserNotification(UserNotificationType type, uint32_t durationMs = 0);
  void startUserTextNotification(const String& text, const CRGB& color, uint32_t durationMs = 0);
  void stopUserNotification();

  void onWifiConnecting();
  void onWifiConnected();
  void onWifiError();
  void onWifiDisabled();

  void onMqttConnecting();
  void onMqttConnected();
  void onMqttError();
  void onMqttDisabled();

  void onOtaStart();
  void onOtaProgress(uint8_t percent);
  void onOtaEnd();
  void onOtaError();

  void onButtonPress(uint8_t count);
  void onButtonRelease();
  void onButtonPowerOn();
  void onButtonPowerOff();
  void onButtonDismiss();
  void onButtonNextEffect();
  void onButtonPreviousEffect();
  void onButtonBrightness(uint8_t brightness, bool increasing);

private:
  static constexpr uint16_t SYSTEM_SUCCESS_VISIBLE_MS = 1500;
  static constexpr uint16_t SYSTEM_ERROR_VISIBLE_MS = 10000;
  static constexpr uint16_t USER_NOTIFY_DEFAULT_MS = 2200;
  static constexpr uint16_t SYSTEM_HIGH_PRIORITY_DIM = 210;
  static constexpr uint16_t SYSTEM_LOW_PRIORITY_DIM = 190;
  static constexpr uint16_t USER_ALERT_DIM = 230;
  static constexpr uint16_t USER_NOTIFY_DIM = 210;
  static constexpr uint16_t NOTIFICATION_FADE_IN_MS = 600;
  static constexpr uint16_t NOTIFICATION_FADE_OUT_MS = 600;
  static constexpr uint16_t BUTTON_PRESS_ECHO_MS = 180;

  EepromStore& _eeprom;
  PowerController& _power;
  StateNotifier& _stateNotifier;
  TimeService& _time;

  SystemNotificationRenderer _systemRenderer;
  UserNotificationRenderer _userRenderer;
  ButtonNotificationRenderer _buttonRenderer;
  UserNotificationState _userState;
  NotificationQuietHours _quietHours;
  bool _userNotificationStopped = false;
  ConnectionState _wifiState = ConnectionState::Idle;
  ConnectionState _mqttState = ConnectionState::Idle;
  OtaState _otaState = OtaState::Idle;
  uint8_t _otaPercent = 0;

  FadeAnimator _opacity;
  NotificationFrame _frame;
  NotificationSnapshot _activeNotification;
  NotificationSnapshot _drawableNotification;

  uint32_t _lastWifiChangeMs = 0;
  uint32_t _lastMqttChangeMs = 0;
  uint32_t _lastOtaChangeMs = 0;

  ButtonNotificationType _buttonType = ButtonNotificationType::None;
  uint32_t _lastButtonChangeMs = 0;
  uint8_t _buttonValue = 0;
  bool _buttonDirection = true;
  uint8_t _buttonPressCount = 0;
  uint32_t _lastButtonPressMs = 0;
  bool _buttonPressing = false;

  void setWifiState(ConnectionState state);
  void setMqttState(ConnectionState state);

  NotificationSnapshot resolveCurrentNotification(uint32_t now) const;
  NotificationSnapshot filterMuted(NotificationSnapshot n) const;

  bool isRecently(uint32_t sinceMs, uint32_t durationMs) const;
  bool sameNotification(const NotificationSnapshot& a, const NotificationSnapshot& b) const;
  static uint8_t getUserNotificationPriority(UserNotificationType type);

  bool shouldPreFadeOut(const NotificationSnapshot& n, uint32_t now) const;
  bool shouldMuteNotification(const NotificationSnapshot& n) const;
  bool canBypassMute(const NotificationSnapshot& n) const;

  void startButtonNotification(ButtonNotificationType type, uint8_t value = 0, bool direction = true);
  static uint32_t getButtonNotificationDuration(ButtonNotificationType type);
};
