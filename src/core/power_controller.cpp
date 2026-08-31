#include <Arduino.h>

#include "../effect/controller.h"
#include "../storage/eeprom_store.h"
#include "auto_off_config.h"
#include "power_controller.h"
#include "state_notifier.h"

namespace {
  constexpr uint16_t kFadeOnMs = 400;
  constexpr uint16_t kFadeOffMs = 350;
} // namespace

void PowerController::init() {
  on_ = eeprom_.readPowerState();
  autoOffMinutes_ = eeprom_.readAutoOffMinutes();
  effectOpacity_.snapTo(on_ ? 255 : 0);
  effects_.setOutputEnabled(on_);

  if (on_) {
    resetAutoOffTimer();
  }
}

void PowerController::on() {
  if (on_ && effectOpacity_.target() == 255) {
    resetAutoOffTimer(); // опционально. Если команда "on" должна продлевать auto-off
    return;
  }

  on_ = true;
  effects_.setOutputEnabled(true);
  effectOpacity_.fadeTo(255, kFadeOnMs);
  resetAutoOffTimer();

  eeprom_.writePowerState(on_);
  stateNotifier_.stateChanged();
}

void PowerController::off() {
  if (!on_ && effectOpacity_.target() == 0) {
    return;
  }

  on_ = false;
  effectOpacity_.fadeTo(0, kFadeOffMs);

  eeprom_.writePowerState(on_);
  stateNotifier_.stateChanged();
}

bool PowerController::tick() {
  const bool changed = effectOpacity_.tick();

  if (!on_ && effectOpacity_.value() == 0) {
    effects_.setOutputEnabled(false);
  }

  if (on_ && autoOffMinutes_ > 0) {
    const uint32_t timeoutMs = static_cast<uint32_t>(autoOffMinutes_) * 60000UL;
    if (millis() - turnedOnAtMs_ >= timeoutMs) {
      off();
    }
  }

  return changed;
}

bool PowerController::setAutoOffMinutes(int minutes) {
  if (minutes < kAutoOffMinutesMin) minutes = kAutoOffMinutesMin;
  if (minutes > kAutoOffMinutesMax) minutes = kAutoOffMinutesMax;
  if (autoOffMinutes_ == static_cast<uint16_t>(minutes)) return true;

  const uint16_t clampedMinutes = static_cast<uint16_t>(minutes);
  if (!eeprom_.writeAutoOffMinutes(clampedMinutes)) return false;

  autoOffMinutes_ = clampedMinutes;
  return true;
}

uint32_t PowerController::getAutoOffRemainingSeconds() const {
  if (!on_ || autoOffMinutes_ == 0) return 0;

  const uint32_t timeoutMs = static_cast<uint32_t>(autoOffMinutes_) * 60000UL;
  const uint32_t elapsedMs = millis() - turnedOnAtMs_;
  if (elapsedMs >= timeoutMs) return 0;

  return (timeoutMs - elapsedMs + 999UL) / 1000UL;
}

void PowerController::resetAutoOffTimer() {
  turnedOnAtMs_ = millis();
}
