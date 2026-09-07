#include "controller.h"

#include "../core/power_controller.h"
#include "../core/state_notifier.h"
#include "../storage/eeprom_store.h"
#include "../time/time_service.h"

void NotificationController::init() {
  quietHours_ = eeprom_.readNotificationQuietHours();

  wifiState_ = ConnectionState::Idle;
  mqttState_ = ConnectionState::Idle;
  otaState_ = OtaState::Idle;
  otaPercent_ = 0;

  lastWifiChangeMs_ = millis();
  lastMqttChangeMs_ = millis();
  lastOtaChangeMs_ = millis();
}

void NotificationController::tick() {
  const uint32_t now = millis();

  if (userState_.tick(now)) {
    stateNotifier_.stateChanged();
  }

  const NotificationSnapshot current = resolveCurrentNotification(now);

  if (current.isActive()) {
    const bool changed = !sameNotification(activeNotification_, current);

    if (changed) {
      if (opacity_.value() == 0) {
        opacity_.snapTo(1);
      }
      opacity_.fadeTo(255, kNotificationFadeInMs, now);
    }

    if (shouldPreFadeOut(current, now)) {
      const uint32_t elapsed = now - current.startedMs;
      const uint32_t remaining = elapsed >= current.durationMs ? 0 : current.durationMs - elapsed;

      if (remaining == 0) {
        opacity_.snapTo(0);
      } else if (opacity_.target() != 0) {
        opacity_.fadeTo(0, min<uint32_t>(remaining, kNotificationFadeOutMs), now);
      }
    }

    activeNotification_ = current;
    drawableNotification_ = current;
  } else {
    if (activeNotification_.isActive()) {
      opacity_.fadeTo(0, kNotificationFadeOutMs, now);
      activeNotification_ = {};
    }

    if (opacity_.value() == 0 && !opacity_.isRunning()) {
      drawableNotification_ = {};
    }
  }

  opacity_.tick(now);

  frame_.snapshot = drawableNotification_;
  frame_.opacity = opacity_.value();
  frame_.backdropDim = frame_.snapshot.isActive() ? scale8(frame_.snapshot.targetDim, frame_.opacity) : 0;
}

void NotificationController::renderOverlay(NotificationOverlay& overlay, const NotificationFrame& frame) {
  const NotificationSnapshot& snaphot = frame.snapshot;
  switch (snaphot.source) {
    case NotificationSource::User: userRenderer_.render(overlay, snaphot); break;
    case NotificationSource::Button: indicatorRenderer_.render(overlay, snaphot); break;
    case NotificationSource::Wifi:
      systemRenderer_.renderWifi(overlay, snaphot.connectionState, snaphot.startedMs);
      break;
    case NotificationSource::Mqtt:
      systemRenderer_.renderMqtt(overlay, snaphot.connectionState, snaphot.startedMs);
      break;
    case NotificationSource::Ota:
      systemRenderer_.renderOta(overlay, snaphot.otaState, snaphot.otaPercent, snaphot.startedMs);
      break;
    case NotificationSource::None: break;
  }
}

bool NotificationController::isActive() const {
  return resolveCurrentNotification(millis()).isActive();
}

uint32_t NotificationController::userNotificationRemainingSeconds() const {
  if (!userState_.isActive() || !userState_.isTimed()) return 0;

  const uint32_t durationMs = userState_.durationMs();
  const uint32_t elapsedMs = millis() - userState_.startedMs();
  if (elapsedMs >= durationMs) return 0;

  return (durationMs - elapsedMs + 999UL) / 1000UL;
}

bool NotificationController::setQuietHours(const NotificationQuietHours& settings) {
  quietHours_ = settings;
  return eeprom_.writeNotificationQuietHours(quietHours_);
}

bool NotificationController::isMutedNow() const {
  if (!quietHours_.enabled) return false;
  if (power_.isOn()) return false;
  if (!time_.isSynced()) return false;

  return quietHours_.isInQuietHours(time_.minutesOfDay());
}

void NotificationController::startUserNotification(UserNotificationType type, uint32_t durationMs) {
  if (type == UserNotificationType::None) {
    stopUserNotification();
    return;
  }

  if (userNotificationPriority(type) < userNotificationPriority(userState_.type())) {
    return;
  }

  if (type == UserNotificationType::Notify /* && durationMs == 0*/) {
    durationMs = kUserNotifyDefaultMs;
  }

  userState_.start(type, durationMs);
  stateNotifier_.stateChanged();
}

void NotificationController::startUserTextNotification(const String& text, const CRGB& color, uint32_t durationMs) {
  if (text.length() == 0) {
    if (userState_.isTextActive()) {
      stopUserNotification();
    }
    return;
  }

  if (userNotificationPriority(UserNotificationType::Text) < userNotificationPriority(userState_.type())) {
    return;
  }

  userState_.startText(text, color, durationMs);
  stateNotifier_.stateChanged();
}

void NotificationController::stopUserNotification() {
  if (!userState_.isActive()) return;
  userState_.stop();
  userNotificationStopped_ = true;
  stateNotifier_.stateChanged();
}

void NotificationController::onWifiConnecting() {
  setWifiState(ConnectionState::Connecting);
}
void NotificationController::onWifiConnected() {
  setWifiState(ConnectionState::Connected);
}
void NotificationController::onWifiError() {
  setWifiState(ConnectionState::Error);
}
void NotificationController::onWifiDisabled() {
  setWifiState(ConnectionState::Disabled);
}

void NotificationController::onMqttConnecting() {
  setMqttState(ConnectionState::Connecting);
}
void NotificationController::onMqttConnected() {
  setMqttState(ConnectionState::Connected);
}
void NotificationController::onMqttError() {
  setMqttState(ConnectionState::Error);
}
void NotificationController::onMqttDisabled() {
  setMqttState(ConnectionState::Disabled);
}

void NotificationController::setWifiState(ConnectionState state) {
  if (wifiState_ == state) return;
  wifiState_ = state;
  lastWifiChangeMs_ = millis();
}

void NotificationController::setMqttState(ConnectionState state) {
  if (mqttState_ == state) return;
  mqttState_ = state;
  lastMqttChangeMs_ = millis();
}

void NotificationController::onButtonPress(uint8_t count) {
  buttonPressCount_ = count;
  lastButtonPressMs_ = millis();
  buttonPressing_ = true;

  // Если нет action notification, создаём короткий Button frame,
  // чтобы press echo мог отрисоваться сам по себе.
  if (indicatorType_ == IndicatorType::None) {
    indicatorType_ = IndicatorType::None;
    lastIndicatorChangeMs_ = lastButtonPressMs_;
  }
}

void NotificationController::onButtonRelease() {
  buttonPressing_ = false;
  lastButtonPressMs_ = millis();
}

void NotificationController::onButtonPowerOn() {
  startIndicator(IndicatorType::PowerOn);
}

void NotificationController::onButtonPowerOff() {
  // Пока намеренно no-op: выключение без визуального feedback.
}

void NotificationController::onButtonDismiss() {
  startIndicator(IndicatorType::Dismiss);
}

void NotificationController::onEffectNext() {
  startIndicator(IndicatorType::NextEffect);
}

void NotificationController::onEffectPrevious() {
  startIndicator(IndicatorType::PreviousEffect);
}

void NotificationController::onRotationEnabled() {
  startIndicator(IndicatorType::RotationOn);
}

void NotificationController::onRotationDisabled() {
  startIndicator(IndicatorType::RotationOff);
}

void NotificationController::onButtonBrightness(uint8_t brightness, bool increasing) {
  startIndicator(IndicatorType::Brightness, brightness, increasing);
}

void NotificationController::startIndicator(IndicatorType type, uint8_t value, bool direction) {
  if (type == IndicatorType::None || type == IndicatorType::PowerOff) {
    indicatorType_ = IndicatorType::None;
    return;
  }

  indicatorType_ = type;
  buttonValue_ = value;
  buttonDirection_ = direction;
  lastIndicatorChangeMs_ = millis();
}

void NotificationController::onOtaStart() {
  otaState_ = OtaState::Running;
  otaPercent_ = 0;
  lastOtaChangeMs_ = millis();
}

void NotificationController::onOtaProgress(uint8_t percent) {
  otaState_ = OtaState::Running;
  otaPercent_ = percent > 100 ? 100 : percent;
}

void NotificationController::onOtaEnd() {
  otaState_ = OtaState::Success;
  otaPercent_ = 100;
  lastOtaChangeMs_ = millis();
}

void NotificationController::onOtaError() {
  otaState_ = OtaState::Error;
  lastOtaChangeMs_ = millis();
}

NotificationSnapshot NotificationController::resolveCurrentNotification(uint32_t now) const {
  NotificationSnapshot snaphot;

  if (otaState_ == OtaState::Running) {
    snaphot.source = NotificationSource::Ota;
    snaphot.otaState = otaState_;
    snaphot.startedMs = lastOtaChangeMs_;
    snaphot.targetDim = kSystemHighPriorityDim;
    snaphot.otaPercent = otaPercent_;
    return filterMuted(snaphot);
  }

  if (otaState_ == OtaState::Error && isRecently(lastOtaChangeMs_, kSystemErrorVisibleMs)) {
    snaphot.source = NotificationSource::Ota;
    snaphot.otaState = otaState_;
    snaphot.startedMs = lastOtaChangeMs_;
    snaphot.durationMs = kSystemErrorVisibleMs;
    snaphot.targetDim = kSystemHighPriorityDim;
    return filterMuted(snaphot);
  }

  if (otaState_ == OtaState::Success && isRecently(lastOtaChangeMs_, kSystemSuccessVisibleMs)) {
    snaphot.source = NotificationSource::Ota;
    snaphot.otaState = otaState_;
    snaphot.startedMs = lastOtaChangeMs_;
    snaphot.durationMs = kSystemSuccessVisibleMs;
    snaphot.targetDim = kSystemHighPriorityDim;
    snaphot.otaPercent = 100;
    return filterMuted(snaphot);
  }

  if (userState_.isAlertActive()) {
    snaphot.source = NotificationSource::User;
    snaphot.userType = userState_.type();
    snaphot.startedMs = userState_.startedMs();
    snaphot.durationMs = userState_.durationMs();
    snaphot.targetDim = kUserAlertDim;
    return filterMuted(snaphot);
  }

  if (userState_.isTextActive()) {
    snaphot.source = NotificationSource::User;
    snaphot.userType = UserNotificationType::Text;
    snaphot.startedMs = userState_.startedMs();
    snaphot.durationMs = userState_.durationMs();
    snaphot.targetDim = kUserNotifyDim;
    snaphot.text = &userState_.text();
    snaphot.color = userState_.color();
    return filterMuted(snaphot);
  }

  const bool hasPressEcho =
    buttonPressCount_ > 0 && (buttonPressing_ || isRecently(lastButtonPressMs_, kButtonPressEchoMs));
  const uint32_t buttonDurationMs = indicatorDuration(indicatorType_);
  const bool hasButtonAction = indicatorType_ != IndicatorType::None && buttonDurationMs > 0 &&
                               isRecently(lastIndicatorChangeMs_, buttonDurationMs);

  if (hasButtonAction || hasPressEcho) {
    snaphot.source = NotificationSource::Button;

    if (hasButtonAction) {
      snaphot.indicatorType = indicatorType_;
      snaphot.buttonValue = buttonValue_;
      snaphot.buttonDirection = buttonDirection_;
      snaphot.startedMs = lastIndicatorChangeMs_;
      snaphot.durationMs = buttonDurationMs;
    } else {
      snaphot.indicatorType = IndicatorType::None;
      snaphot.startedMs = lastButtonPressMs_;
      snaphot.durationMs = kButtonPressEchoMs;
    }

    snaphot.buttonPressCount = buttonPressCount_;
    snaphot.buttonPressMs = lastButtonPressMs_;
    snaphot.buttonPressing = buttonPressing_;
    snaphot.targetDim = 0;
    return filterMuted(snaphot);
  }

  if (wifiState_ == ConnectionState::Connecting || wifiState_ == ConnectionState::Error) {
    snaphot.source = NotificationSource::Wifi;
    snaphot.connectionState = wifiState_;
    snaphot.startedMs = lastWifiChangeMs_;
    snaphot.durationMs = wifiState_ == ConnectionState::Error ? kSystemErrorVisibleMs : 0;
    snaphot.targetDim = kSystemLowPriorityDim;
    return filterMuted(snaphot);
  }

  if (mqttState_ == ConnectionState::Connecting || mqttState_ == ConnectionState::Error) {
    snaphot.source = NotificationSource::Mqtt;
    snaphot.connectionState = mqttState_;
    snaphot.startedMs = lastMqttChangeMs_;
    snaphot.durationMs = mqttState_ == ConnectionState::Error ? kSystemErrorVisibleMs : 0;
    snaphot.targetDim = kSystemLowPriorityDim;
    return filterMuted(snaphot);
  }

  if (userState_.isNotifyActive()) {
    snaphot.source = NotificationSource::User;
    snaphot.userType = UserNotificationType::Notify;
    snaphot.startedMs = userState_.startedMs();
    snaphot.durationMs = userState_.durationMs();
    snaphot.targetDim = kUserNotifyDim;
    return filterMuted(snaphot);
  }

  if (wifiState_ == ConnectionState::Connected && isRecently(lastWifiChangeMs_, kSystemSuccessVisibleMs)) {
    snaphot.source = NotificationSource::Wifi;
    snaphot.connectionState = wifiState_;
    snaphot.startedMs = lastWifiChangeMs_;
    snaphot.durationMs = kSystemSuccessVisibleMs;
    snaphot.targetDim = kSystemLowPriorityDim;
    return filterMuted(snaphot);
  }

  if (mqttState_ == ConnectionState::Connected && isRecently(lastMqttChangeMs_, kSystemSuccessVisibleMs)) {
    snaphot.source = NotificationSource::Mqtt;
    snaphot.connectionState = mqttState_;
    snaphot.startedMs = lastMqttChangeMs_;
    snaphot.durationMs = kSystemSuccessVisibleMs;
    snaphot.targetDim = kSystemLowPriorityDim;
    return filterMuted(snaphot);
  }

  return filterMuted(snaphot);
}

NotificationSnapshot NotificationController::filterMuted(NotificationSnapshot snaphot) const {
  if (!snaphot.isActive()) return snaphot;
  if (!shouldMuteNotification(snaphot)) return snaphot;
  return {};
}

bool NotificationController::isRecently(uint32_t sinceMs, uint32_t durationMs) const {
  return millis() - sinceMs < durationMs;
}

bool NotificationController::sameNotification(const NotificationSnapshot& a, const NotificationSnapshot& b) const {
  if (a.source != b.source) return false;

  if (a.source == NotificationSource::Button) {
    return a.indicatorType == b.indicatorType && a.startedMs == b.startedMs && a.buttonValue == b.buttonValue &&
           a.buttonDirection == b.buttonDirection && a.buttonPressCount == b.buttonPressCount &&
           a.buttonPressMs == b.buttonPressMs && a.buttonPressing == b.buttonPressing;
  }

  return a.userType == b.userType && a.connectionState == b.connectionState && a.otaState == b.otaState;
}

uint8_t NotificationController::userNotificationPriority(UserNotificationType type) {
  switch (type) {
    case UserNotificationType::Alarm: return 4;
    case UserNotificationType::Warning: return 3;
    case UserNotificationType::Text: return 2;
    case UserNotificationType::Notify: return 1;
    case UserNotificationType::None:
    default: return 0;
  }
}

bool NotificationController::shouldPreFadeOut(const NotificationSnapshot& snaphot, uint32_t now) const {
  if (snaphot.source == NotificationSource::Button) return false;
  if (!snaphot.isTimed()) return false;
  if (snaphot.durationMs <= kNotificationFadeOutMs) return false;

  const uint32_t elapsed = now - snaphot.startedMs;
  if (elapsed >= snaphot.durationMs) return true;

  const uint32_t remaining = snaphot.durationMs - elapsed;
  return remaining <= kNotificationFadeOutMs;
}

bool NotificationController::shouldMuteNotification(const NotificationSnapshot& snaphot) const {
  return isMutedNow() && !canBypassMute(snaphot);
}

bool NotificationController::canBypassMute(const NotificationSnapshot& snaphot) const {
  if (snaphot.source == NotificationSource::Ota) return true;
  return snaphot.source == NotificationSource::User &&
         (snaphot.userType == UserNotificationType::Alarm || snaphot.userType == UserNotificationType::Warning);
}

uint32_t NotificationController::indicatorDuration(IndicatorType type) {
  switch (type) {
    case IndicatorType::PowerOn: return 700;
    case IndicatorType::Dismiss: return 450;
    case IndicatorType::NextEffect:
    case IndicatorType::PreviousEffect: return 900;
    case IndicatorType::Brightness: return 1200;
    case IndicatorType::RotationOn:
    case IndicatorType::RotationOff: return 900;
    case IndicatorType::PowerOff:
    case IndicatorType::None:
    default: return 0;
  }
}
