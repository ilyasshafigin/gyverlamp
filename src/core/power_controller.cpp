#include <Arduino.h>

#include "../effect/controller.h"
#include "../storage/eeprom_store.h"
#include "auto_off_config.h"
#include "power_controller.h"
#include "state_notifier.h"

namespace {
  constexpr uint16_t FADE_ON_MS = 400;
  constexpr uint16_t FADE_OFF_MS = 350;
} // namespace

void PowerController::init() {
  _on = _eeprom.readPowerState();
  _autoOffMinutes = _eeprom.readAutoOffMinutes();
  _effectOpacity.snapTo(_on ? 255 : 0);
  _effects.setOutputEnabled(_on);

  if (_on) {
    resetAutoOffTimer();
  }
}

void PowerController::on() {
  if (_on && _effectOpacity.target() == 255) {
    resetAutoOffTimer(); // опционально. Если команда "on" должна продлевать auto-off
    return;
  }

  _on = true;
  _effects.setOutputEnabled(true);
  _effectOpacity.fadeTo(255, FADE_ON_MS);
  resetAutoOffTimer();

  _eeprom.writePowerState(_on);
  _stateNotifier.stateChanged();
}

void PowerController::off() {
  if (!_on && _effectOpacity.target() == 0) {
    return;
  }

  _on = false;
  _effectOpacity.fadeTo(0, FADE_OFF_MS);

  _eeprom.writePowerState(_on);
  _stateNotifier.stateChanged();
}

bool PowerController::tick() {
  const bool changed = _effectOpacity.tick();

  if (!_on && _effectOpacity.value() == 0) {
    _effects.setOutputEnabled(false);
  }

  if (_on && _autoOffMinutes > 0) {
    const uint32_t timeoutMs = static_cast<uint32_t>(_autoOffMinutes) * 60000UL;
    if (millis() - _turnedOnAtMs >= timeoutMs) {
      off();
    }
  }

  return changed;
}

bool PowerController::setAutoOffMinutes(int minutes) {
  if (minutes < AUTO_OFF_MINUTES_MIN) minutes = AUTO_OFF_MINUTES_MIN;
  if (minutes > AUTO_OFF_MINUTES_MAX) minutes = AUTO_OFF_MINUTES_MAX;
  if (_autoOffMinutes == static_cast<uint16_t>(minutes)) return true;

  const uint16_t clampedMinutes = static_cast<uint16_t>(minutes);
  if (!_eeprom.writeAutoOffMinutes(clampedMinutes)) return false;

  _autoOffMinutes = clampedMinutes;
  return true;
}

uint32_t PowerController::getAutoOffRemainingSeconds() const {
  if (!_on || _autoOffMinutes == 0) return 0;

  const uint32_t timeoutMs = static_cast<uint32_t>(_autoOffMinutes) * 60000UL;
  const uint32_t elapsedMs = millis() - _turnedOnAtMs;
  if (elapsedMs >= timeoutMs) return 0;

  return (timeoutMs - elapsedMs + 999UL) / 1000UL;
}

void PowerController::resetAutoOffTimer() {
  _turnedOnAtMs = millis();
}
