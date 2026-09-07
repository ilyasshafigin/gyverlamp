#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "../config.h"
#include "../util/fade_animator.h"

#include "indicator_renderer.h"
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
    : eeprom_(eeprom),
      power_(power),
      stateNotifier_(stateNotifier),
      time_(time),
      systemRenderer_(),
      userRenderer_(runningText),
      indicatorRenderer_(),
      userState_() {}

  void init();
  void tick();

  const NotificationFrame& frame() const { return frame_; }

  void renderOverlay(NotificationOverlay& overlay, const NotificationFrame& frame);

  bool isActive() const;
  bool isUserNotificationActive() const { return userState_.isActive(); }
  UserNotificationType userNotificationType() const { return userState_.type(); }
  uint32_t userNotificationRemainingSeconds() const;

  const NotificationQuietHours& quietHours() const { return quietHours_; }
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
  void onEffectNext();
  void onEffectPrevious();
  void onButtonBrightness(uint8_t brightness, bool increasing);
  void onRotationEnabled();
  void onRotationDisabled();

private:
  static constexpr uint16_t kSystemSuccessVisibleMs = 1500;
  static constexpr uint16_t kSystemErrorVisibleMs = 10000;
  static constexpr uint16_t kUserNotifyDefaultMs = 2200;
  static constexpr uint16_t kSystemHighPriorityDim = 210;
  static constexpr uint16_t kSystemLowPriorityDim = 190;
  static constexpr uint16_t kUserAlertDim = 230;
  static constexpr uint16_t kUserNotifyDim = 210;
  static constexpr uint16_t kNotificationFadeInMs = 600;
  static constexpr uint16_t kNotificationFadeOutMs = 600;
  static constexpr uint16_t kButtonPressEchoMs = 180;

  EepromStore& eeprom_;
  PowerController& power_;
  StateNotifier& stateNotifier_;
  TimeService& time_;

  SystemNotificationRenderer systemRenderer_;
  UserNotificationRenderer userRenderer_;
  IndicatorRenderer indicatorRenderer_;
  UserNotificationState userState_;
  NotificationQuietHours quietHours_;
  bool userNotificationStopped_ = false;
  ConnectionState wifiState_ = ConnectionState::Idle;
  ConnectionState mqttState_ = ConnectionState::Idle;
  OtaState otaState_ = OtaState::Idle;
  uint8_t otaPercent_ = 0;

  FadeAnimator opacity_;
  NotificationFrame frame_;
  NotificationSnapshot activeNotification_;
  NotificationSnapshot drawableNotification_;

  uint32_t lastWifiChangeMs_ = 0;
  uint32_t lastMqttChangeMs_ = 0;
  uint32_t lastOtaChangeMs_ = 0;

  IndicatorType indicatorType_ = IndicatorType::None;
  uint32_t lastIndicatorChangeMs_ = 0;
  uint8_t buttonValue_ = 0;
  bool buttonDirection_ = true;
  uint8_t buttonPressCount_ = 0;
  uint32_t lastButtonPressMs_ = 0;
  bool buttonPressing_ = false;

  void setWifiState(ConnectionState state);
  void setMqttState(ConnectionState state);

  NotificationSnapshot resolveCurrentNotification(uint32_t now) const;
  NotificationSnapshot filterMuted(NotificationSnapshot n) const;

  bool isRecently(uint32_t sinceMs, uint32_t durationMs) const;
  bool sameNotification(const NotificationSnapshot& a, const NotificationSnapshot& b) const;
  static uint8_t userNotificationPriority(UserNotificationType type);

  bool shouldPreFadeOut(const NotificationSnapshot& n, uint32_t now) const;
  bool shouldMuteNotification(const NotificationSnapshot& n) const;
  bool canBypassMute(const NotificationSnapshot& n) const;

  void startIndicator(IndicatorType type, uint8_t value = 0, bool direction = true);
  static uint32_t indicatorDuration(IndicatorType type);
};
