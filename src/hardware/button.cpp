#include "button.h"

#ifdef USE_BUTTON
#include <ESP8266WiFi.h>

#include "../core/power_controller.h"
#include "../core/rotation_controller.h"
#include "../core/state_notifier.h"
#include "../effect/controller.h"
#include "../effect/effects.h"
#include "../notification/controller.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"

void TouchButton::detect() {
  _connected = !digitalRead(_pin);
#ifdef DEBUG
  if (_connected) {
    Serial.println("[BUTTON] Touch button detected.");
  } else {
    Serial.println("[BUTTON] No touch button detected, touch button control disabled.");
  }
#endif
}

void TouchButton::init() {
  _enabled = _eeprom.readButtonEnabled();
  _button.init(_pin, INPUT, HIGH);
  _button.setStepTimeout(100);
  _button.setClickTimeout(500);
}

bool TouchButton::setEnabled(bool enabled) {
  _enabled = enabled;
  return _eeprom.writeButtonEnabled(enabled);
}

void TouchButton::tick() {
  if (!_connected) return;

  // Необходимое время для калибровки сенсорной кнопки при плотном прилегании к стеклу
  //if (millis() < 10000) return;

  _button.tick();

  if (!_enabled) return;

  if (_button.press()) {
    _notifications.onButtonPress(_button.getClicks() + 1);
  }

  if (_button.release()) {
    _notifications.onButtonRelease();
  }

  if (_button.hasClicks(1)) {
    Serial.println("[BUTTON] Single tap detected");
    if (_notifications.isUserNotificationActive()) {
      _notifications.stopUserNotification();
      _notifications.onButtonDismiss();
    } else if (_rotation.isActive()) {
      _rotation.disable();
      _notifications.onRotationDisabled();
    } else {
      const bool wasOn = _power.isOn();
      _power.toggle();

      if (!wasOn && _power.isOn()) {
        _notifications.onButtonPowerOn();
      } else {
        _notifications.onButtonPowerOff();
      }
    }
  }

  if (_power.isOn()) {
    if (_button.hasClicks(2)) {
      Serial.println("[BUTTON] Double tap detected");
      _rotation.onManualRotation();
      _effects.setNextEffect();
      _notifications.onEffectNext();
      _stateNotifier.stateChanged();
    }

    if (_button.hasClicks(3)) {
      Serial.println("[BUTTON] Triple tap detected");
      _rotation.onManualRotation();
      _effects.setPreviousEffect();
      _notifications.onEffectPrevious();
      _stateNotifier.stateChanged();
    }

    // вывод IP на лампу
    if (_button.hasClicks(5)) {
      Serial.println("[BUTTON] 5 taps detected");
      _notifications.startUserTextNotification(WiFi.localIP().toString(), CRGB::Green, 15000);
    }

    if (_button.hold()) {
      _brightDirection = !_brightDirection;
      const EffectSettings& s = _settings.getEffectSettings(_effects.getSelectedEffectId());
      _notifications.onButtonBrightness(s.brightness, _brightDirection);
    }

    if (_button.step()) {
      const EffectSettings& effectSettings = _settings.getEffectSettings(_effects.getSelectedEffectId());
      uint8_t newBrightness = effectSettings.brightness;
      if (_brightDirection) {
        if (effectSettings.brightness < 10U) newBrightness = effectSettings.brightness + 1U;
        else if (effectSettings.brightness < 250U)
          newBrightness = effectSettings.brightness + 5U;
        else
          newBrightness = 255U;
      } else {
        if (effectSettings.brightness > 15U) newBrightness = effectSettings.brightness - 5U;
        else if (effectSettings.brightness > 1U)
          newBrightness = effectSettings.brightness - 1U;
        else
          newBrightness = 1U;
      }
      _effects.setEffectBrightness(newBrightness);
      _notifications.onButtonBrightness(newBrightness, _brightDirection);
      _stateNotifier.stateChanged();
    }
  }
}

#else

void TouchButton::detect() {
}
void TouchButton::init() {
}
bool TouchButton::setEnabled(bool enabled) {
}
void TouchButton::tick() {
}

#endif
