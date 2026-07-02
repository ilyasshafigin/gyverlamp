#include "controller.h"

#include "../core/power_controller.h"
#include "../core/state_notifier.h"
#include "../storage/eeprom_store.h"
#include "../time/time_service.h"

void NotificationController::init() {
  _quietHours = _eeprom.readNotificationQuietHours();

  _wifiState = ConnectionState::Idle;
  _mqttState = ConnectionState::Idle;
  _otaState = OtaState::Idle;
  _otaPercent = 0;

  _lastWifiChangeMs = millis();
  _lastMqttChangeMs = millis();
  _lastOtaChangeMs = millis();
}

void NotificationController::tick() {
  const uint32_t now = millis();

  if (_userState.tick(now)) {
    _stateNotifier.stateChanged();
  }

  const NotificationSnapshot current = resolveCurrentNotification(now);

  if (current.isActive()) {
    const bool changed = !sameNotification(_activeNotification, current);

    if (changed) {
      if (_opacity.value() == 0) {
        _opacity.snapTo(1);
      }
      _opacity.fadeTo(255, NOTIFICATION_FADE_IN_MS, now);
    }

    if (shouldPreFadeOut(current, now)) {
      const uint32_t elapsed = now - current.startedMs;
      const uint32_t remaining = elapsed >= current.durationMs ? 0 : current.durationMs - elapsed;

      if (remaining == 0) {
        _opacity.snapTo(0);
      } else if (_opacity.target() != 0) {
        _opacity.fadeTo(0, min<uint32_t>(remaining, NOTIFICATION_FADE_OUT_MS), now);
      }
    }

    _activeNotification = current;
    _drawableNotification = current;
  } else {
    if (_activeNotification.isActive()) {
      _opacity.fadeTo(0, NOTIFICATION_FADE_OUT_MS, now);
      _activeNotification = {};
    }

    if (_opacity.value() == 0 && !_opacity.isRunning()) {
      _drawableNotification = {};
    }
  }

  _opacity.tick(now);

  _frame.snapshot = _drawableNotification;
  _frame.opacity = _opacity.value();
  _frame.backdropDim = _frame.snapshot.isActive()
    ? scale8(_frame.snapshot.targetDim, _frame.opacity)
    : 0;
}

void NotificationController::renderOverlay(NotificationOverlay& overlay, const NotificationFrame& frame) {
  const NotificationSnapshot& snaphot = frame.snapshot;
  switch (snaphot.source) {
  case NotificationSource::User:
    _userRenderer.render(overlay, snaphot);
    break;
  case NotificationSource::Button:
    _buttonRenderer.render(overlay, snaphot);
    break;
  case NotificationSource::Wifi:
    _systemRenderer.renderWifi(overlay, snaphot.connectionState, snaphot.startedMs);
    break;
  case NotificationSource::Mqtt:
    _systemRenderer.renderMqtt(overlay, snaphot.connectionState, snaphot.startedMs);
    break;
  case NotificationSource::Ota:
    _systemRenderer.renderOta(overlay, snaphot.otaState, snaphot.otaPercent, snaphot.startedMs);
    break;
  case NotificationSource::None:
    break;
  }
}

bool NotificationController::isActive() const {
  return resolveCurrentNotification(millis()).isActive();
}

uint32_t NotificationController::getUserNotificationRemainingSeconds() const {
  if (!_userState.isActive() || !_userState.isTimed()) return 0;

  const uint32_t durationMs = _userState.getDurationMs();
  const uint32_t elapsedMs = millis() - _userState.getStartedMs();
  if (elapsedMs >= durationMs) return 0;

  return (durationMs - elapsedMs + 999UL) / 1000UL;
}

bool NotificationController::setQuietHours(const NotificationQuietHours& settings) {
  _quietHours = settings;
  return _eeprom.writeNotificationQuietHours(_quietHours);
}

bool NotificationController::isMutedNow() const {
  if (!_quietHours.enabled) return false;
  if (_power.isOn()) return false;
  if (!_time.isSynced()) return false;

  return _quietHours.isInQuietHours(_time.getMinutesOfDay());
}

void NotificationController::startUserNotification(UserNotificationType type, uint32_t durationMs) {
  if (type == UserNotificationType::None) {
    stopUserNotification();
    return;
  }

  if (getUserNotificationPriority(type) < getUserNotificationPriority(_userState.getType())) {
    return;
  }

  if (type == UserNotificationType::Notify/* && durationMs == 0*/) {
    durationMs = USER_NOTIFY_DEFAULT_MS;
  }

  _userState.start(type, durationMs);
  _stateNotifier.stateChanged();
}

void NotificationController::startUserTextNotification(const String& text, const CRGB& color, uint32_t durationMs) {
  if (text.length() == 0) {
    if (_userState.isTextActive()) {
      stopUserNotification();
    }
    return;
  }

  if (getUserNotificationPriority(UserNotificationType::Text) < getUserNotificationPriority(_userState.getType())) {
    return;
  }

  _userState.startText(text, color, durationMs);
  _stateNotifier.stateChanged();
}

void NotificationController::stopUserNotification() {
  if (!_userState.isActive()) return;
  _userState.stop();
  _userNotificationStopped = true;
  _stateNotifier.stateChanged();
}

void NotificationController::onWifiConnecting() { setWifiState(ConnectionState::Connecting); }
void NotificationController::onWifiConnected() { setWifiState(ConnectionState::Connected); }
void NotificationController::onWifiError() { setWifiState(ConnectionState::Error); }
void NotificationController::onWifiDisabled() { setWifiState(ConnectionState::Disabled); }

void NotificationController::onMqttConnecting() { setMqttState(ConnectionState::Connecting); }
void NotificationController::onMqttConnected() { setMqttState(ConnectionState::Connected); }
void NotificationController::onMqttError() { setMqttState(ConnectionState::Error); }
void NotificationController::onMqttDisabled() { setMqttState(ConnectionState::Disabled); }

void NotificationController::setWifiState(ConnectionState state) {
  if (_wifiState == state) return;
  _wifiState = state;
  _lastWifiChangeMs = millis();
}

void NotificationController::setMqttState(ConnectionState state) {
  if (_mqttState == state) return;
  _mqttState = state;
  _lastMqttChangeMs = millis();
}

void NotificationController::onButtonPress(uint8_t count) {
  _buttonPressCount = count;
  _lastButtonPressMs = millis();
  _buttonPressing = true;

  // Если нет action notification, создаём короткий Button frame,
  // чтобы press echo мог отрисоваться сам по себе.
  if (_buttonType == ButtonNotificationType::None) {
    _buttonType = ButtonNotificationType::None;
    _lastButtonChangeMs = _lastButtonPressMs;
  }
}

void NotificationController::onButtonRelease() {
  _buttonPressing = false;
  _lastButtonPressMs = millis();
}

void NotificationController::onButtonPowerOn() {
  startButtonNotification(ButtonNotificationType::PowerOn);
}

void NotificationController::onButtonPowerOff() {
  // Пока намеренно no-op: выключение без визуального feedback.
}

void NotificationController::onButtonDismiss() {
  startButtonNotification(ButtonNotificationType::Dismiss);
}

void NotificationController::onButtonNextEffect() {
  startButtonNotification(ButtonNotificationType::NextEffect);
}

void NotificationController::onButtonPreviousEffect() {
  startButtonNotification(ButtonNotificationType::PreviousEffect);
}

void NotificationController::onButtonBrightness(uint8_t brightness, bool increasing) {
  startButtonNotification(ButtonNotificationType::Brightness, brightness, increasing);
}

void NotificationController::startButtonNotification(ButtonNotificationType type, uint8_t value, bool direction) {
  if (type == ButtonNotificationType::None || type == ButtonNotificationType::PowerOff) {
    _buttonType = ButtonNotificationType::None;
    return;
  }

  _buttonType = type;
  _buttonValue = value;
  _buttonDirection = direction;
  _lastButtonChangeMs = millis();
}

void NotificationController::onOtaStart() {
  _otaState = OtaState::Running;
  _otaPercent = 0;
  _lastOtaChangeMs = millis();
}

void NotificationController::onOtaProgress(uint8_t percent) {
  _otaState = OtaState::Running;
  _otaPercent = percent > 100 ? 100 : percent;
}

void NotificationController::onOtaEnd() {
  _otaState = OtaState::Success;
  _otaPercent = 100;
  _lastOtaChangeMs = millis();
}

void NotificationController::onOtaError() {
  _otaState = OtaState::Error;
  _lastOtaChangeMs = millis();
}

NotificationSnapshot NotificationController::resolveCurrentNotification(uint32_t now) const {
  NotificationSnapshot snaphot;

  if (_otaState == OtaState::Running) {
    snaphot.source = NotificationSource::Ota;
    snaphot.otaState = _otaState;
    snaphot.startedMs = _lastOtaChangeMs;
    snaphot.targetDim = SYSTEM_HIGH_PRIORITY_DIM;
    snaphot.otaPercent = _otaPercent;
    return filterMuted(snaphot);
  }

  if (_otaState == OtaState::Error && isRecently(_lastOtaChangeMs, SYSTEM_ERROR_VISIBLE_MS)) {
    snaphot.source = NotificationSource::Ota;
    snaphot.otaState = _otaState;
    snaphot.startedMs = _lastOtaChangeMs;
    snaphot.durationMs = SYSTEM_ERROR_VISIBLE_MS;
    snaphot.targetDim = SYSTEM_HIGH_PRIORITY_DIM;
    return filterMuted(snaphot);
  }

  if (_otaState == OtaState::Success && isRecently(_lastOtaChangeMs, SYSTEM_SUCCESS_VISIBLE_MS)) {
    snaphot.source = NotificationSource::Ota;
    snaphot.otaState = _otaState;
    snaphot.startedMs = _lastOtaChangeMs;
    snaphot.durationMs = SYSTEM_SUCCESS_VISIBLE_MS;
    snaphot.targetDim = SYSTEM_HIGH_PRIORITY_DIM;
    snaphot.otaPercent = 100;
    return filterMuted(snaphot);
  }

  if (_userState.isAlertActive()) {
    snaphot.source = NotificationSource::User;
    snaphot.userType = _userState.getType();
    snaphot.startedMs = _userState.getStartedMs();
    snaphot.durationMs = _userState.getDurationMs();
    snaphot.targetDim = USER_ALERT_DIM;
    return filterMuted(snaphot);
  }

  if (_userState.isTextActive()) {
    snaphot.source = NotificationSource::User;
    snaphot.userType = UserNotificationType::Text;
    snaphot.startedMs = _userState.getStartedMs();
    snaphot.durationMs = _userState.getDurationMs();
    snaphot.targetDim = USER_NOTIFY_DIM;
    snaphot.text = &_userState.getText();
    snaphot.color = _userState.getColor();
    return filterMuted(snaphot);
  }

  const bool hasPressEcho = _buttonPressCount > 0 && (_buttonPressing || isRecently(_lastButtonPressMs, BUTTON_PRESS_ECHO_MS));
  const uint32_t buttonDurationMs = getButtonNotificationDuration(_buttonType);
  const bool hasButtonAction = _buttonType != ButtonNotificationType::None && buttonDurationMs > 0 && isRecently(_lastButtonChangeMs, buttonDurationMs);

  if (hasButtonAction || hasPressEcho) {
    snaphot.source = NotificationSource::Button;

    if (hasButtonAction) {
      snaphot.buttonType = _buttonType;
      snaphot.buttonValue = _buttonValue;
      snaphot.buttonDirection = _buttonDirection;
      snaphot.startedMs = _lastButtonChangeMs;
      snaphot.durationMs = buttonDurationMs;
    } else {
      snaphot.buttonType = ButtonNotificationType::None;
      snaphot.startedMs = _lastButtonPressMs;
      snaphot.durationMs = BUTTON_PRESS_ECHO_MS;
    }

    snaphot.buttonPressCount = _buttonPressCount;
    snaphot.buttonPressMs = _lastButtonPressMs;
    snaphot.buttonPressing = _buttonPressing;
    snaphot.targetDim = 0;
    return filterMuted(snaphot);
  }

  if (_wifiState == ConnectionState::Connecting || _wifiState == ConnectionState::Error) {
    snaphot.source = NotificationSource::Wifi;
    snaphot.connectionState = _wifiState;
    snaphot.startedMs = _lastWifiChangeMs;
    snaphot.durationMs = _wifiState == ConnectionState::Error ? SYSTEM_ERROR_VISIBLE_MS : 0;
    snaphot.targetDim = SYSTEM_LOW_PRIORITY_DIM;
    return filterMuted(snaphot);
  }

  if (_mqttState == ConnectionState::Connecting || _mqttState == ConnectionState::Error) {
    snaphot.source = NotificationSource::Mqtt;
    snaphot.connectionState = _mqttState;
    snaphot.startedMs = _lastMqttChangeMs;
    snaphot.durationMs = _mqttState == ConnectionState::Error ? SYSTEM_ERROR_VISIBLE_MS : 0;
    snaphot.targetDim = SYSTEM_LOW_PRIORITY_DIM;
    return filterMuted(snaphot);
  }

  if (_userState.isNotifyActive()) {
    snaphot.source = NotificationSource::User;
    snaphot.userType = UserNotificationType::Notify;
    snaphot.startedMs = _userState.getStartedMs();
    snaphot.durationMs = _userState.getDurationMs();
    snaphot.targetDim = USER_NOTIFY_DIM;
    return filterMuted(snaphot);
  }

  if (_wifiState == ConnectionState::Connected && isRecently(_lastWifiChangeMs, SYSTEM_SUCCESS_VISIBLE_MS)) {
    snaphot.source = NotificationSource::Wifi;
    snaphot.connectionState = _wifiState;
    snaphot.startedMs = _lastWifiChangeMs;
    snaphot.durationMs = SYSTEM_SUCCESS_VISIBLE_MS;
    snaphot.targetDim = SYSTEM_LOW_PRIORITY_DIM;
    return filterMuted(snaphot);
  }

  if (_mqttState == ConnectionState::Connected && isRecently(_lastMqttChangeMs, SYSTEM_SUCCESS_VISIBLE_MS)) {
    snaphot.source = NotificationSource::Mqtt;
    snaphot.connectionState = _mqttState;
    snaphot.startedMs = _lastMqttChangeMs;
    snaphot.durationMs = SYSTEM_SUCCESS_VISIBLE_MS;
    snaphot.targetDim = SYSTEM_LOW_PRIORITY_DIM;
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

bool NotificationController::sameNotification(
  const NotificationSnapshot& a,
  const NotificationSnapshot& b
) const {
  if (a.source != b.source) return false;

  if (a.source == NotificationSource::Button) {
    return
      a.buttonType == b.buttonType &&
      a.startedMs == b.startedMs &&
      a.buttonValue == b.buttonValue &&
      a.buttonDirection == b.buttonDirection &&
      a.buttonPressCount == b.buttonPressCount &&
      a.buttonPressMs == b.buttonPressMs &&
      a.buttonPressing == b.buttonPressing;
  }

  return
    a.userType == b.userType &&
    a.connectionState == b.connectionState &&
    a.otaState == b.otaState;
}

uint8_t NotificationController::getUserNotificationPriority(UserNotificationType type) {
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
  if (snaphot.durationMs <= NOTIFICATION_FADE_OUT_MS) return false;

  const uint32_t elapsed = now - snaphot.startedMs;
  if (elapsed >= snaphot.durationMs) return true;

  const uint32_t remaining = snaphot.durationMs - elapsed;
  return remaining <= NOTIFICATION_FADE_OUT_MS;
}

bool NotificationController::shouldMuteNotification(const NotificationSnapshot& snaphot) const {
  return isMutedNow() && !canBypassMute(snaphot);
}

bool NotificationController::canBypassMute(const NotificationSnapshot& snaphot) const {
  if (snaphot.source == NotificationSource::Ota) return true;
  return snaphot.source == NotificationSource::User &&
    (snaphot.userType == UserNotificationType::Alarm ||
      snaphot.userType == UserNotificationType::Warning);
}

uint32_t NotificationController::getButtonNotificationDuration(ButtonNotificationType type) {
  switch (type) {
  case ButtonNotificationType::PowerOn: return 700;
  case ButtonNotificationType::Dismiss: return 450;
  case ButtonNotificationType::NextEffect: return 900;
  case ButtonNotificationType::PreviousEffect: return 900;
  case ButtonNotificationType::Brightness: return 1200;
  case ButtonNotificationType::PowerOff:
  case ButtonNotificationType::None:
  default:
    return 0;
  }
}
