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
  connected_ = !digitalRead(pin_);
#ifdef DEBUG
  if (connected_) {
    Serial.println("[BUTTON] Touch button detected.");
  } else {
    Serial.println("[BUTTON] No touch button detected, touch button control disabled.");
  }
#endif
}

void TouchButton::init() {
  enabled_ = eeprom_.readButtonEnabled();
  button_.init(pin_, INPUT, HIGH);
  button_.setStepTimeout(100);
  button_.setClickTimeout(500);
}

bool TouchButton::setEnabled(bool enabled) {
  enabled_ = enabled;
  return eeprom_.writeButtonEnabled(enabled);
}

void TouchButton::tick() {
  if (!connected_) return;

  button_.tick();

  if (!enabled_) return;

  if (button_.press()) {
    notifications_.onButtonPress(button_.getClicks() + 1);
  }

  if (button_.release()) {
    notifications_.onButtonRelease();
  }

  if (button_.hasClicks(1)) {
    Serial.println("[BUTTON] Single tap detected");
    if (notifications_.isUserNotificationActive()) {
      notifications_.stopUserNotification();
      notifications_.onButtonDismiss();
    } else {
      if (power_.isOn() && rotation_.isActive()) {
        rotation_.disable();
        notifications_.onRotationDisabled();
      } else {
        const bool wasOn = power_.isOn();
        power_.toggle();

        if (!wasOn && power_.isOn()) {
          notifications_.onButtonPowerOn();
        } else {
          notifications_.onButtonPowerOff();
        }
      }
    }
  }

  if (power_.isOn()) {
    if (button_.hasClicks(2)) {
      Serial.println("[BUTTON] Double tap detected");
      rotation_.onManualRotation();
      effects_.setNextEffect();
      notifications_.onEffectNext();
      stateNotifier_.stateChanged();
    }

    if (button_.hasClicks(3)) {
      Serial.println("[BUTTON] Triple tap detected");
      rotation_.onManualRotation();
      effects_.setPreviousEffect();
      notifications_.onEffectPrevious();
      stateNotifier_.stateChanged();
    }

    // вывод IP на лампу
    if (button_.hasClicks(5)) {
      Serial.println("[BUTTON] 5 taps detected");
      notifications_.startUserTextNotification(WiFi.localIP().toString(), CRGB::Green, 15000);
    }

    if (button_.hold()) {
      brightDirection_ = !brightDirection_;
      const EffectSettings& s = settings_.getEffectSettings(effects_.getSelectedEffectId());
      notifications_.onButtonBrightness(s.brightness, brightDirection_);
    }

    if (button_.step()) {
      const EffectSettings& effectSettings = settings_.getEffectSettings(effects_.getSelectedEffectId());
      uint8_t newBrightness = effectSettings.brightness;
      if (brightDirection_) {
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
      effects_.setEffectBrightness(newBrightness);
      notifications_.onButtonBrightness(newBrightness, brightDirection_);
      stateNotifier_.stateChanged();
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
