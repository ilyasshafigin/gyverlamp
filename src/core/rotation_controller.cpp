#include <Arduino.h>
#include "../effect/controller.h"
#include "../storage/eeprom_store.h"
#include "rotation_controller.h"
#include "rotation_presets.h"
#include "state_notifier.h"

void RotationController::init() {
  _mode = _eepromStore.readRotationMode();
  _intervalSec = _eepromStore.readRotationIntervalSec();
  // One-shot: migrate legacy non-preset value to the nearest preset.
  // Fires once on first boot after the upgrade; subsequent boots hit the
  // equality guard below because EEPROM now stores a snapped preset.
  const uint16_t snapped = rotationPresetSnapSeconds(_intervalSec);
  if (snapped != _intervalSec) {
    _intervalSec = snapped;
    _eepromStore.writeRotationIntervalSec(_intervalSec);
  }
  _timer.setOnTimer([this]() { this->timerCallback(); });
  restartTimer();
}

void RotationController::tick() {
  _timer.update();
}

void RotationController::setMode(RotationMode mode) {
  if (mode == _mode) return;

  _mode = mode;
  _eepromStore.writeRotationMode(_mode);

  if (_mode == RotationMode::Off) {
    _timer.stop();
    _stateNotifier.stateChanged();
    return;
  }

  restartTimer();
  rotateNow();
  _stateNotifier.stateChanged();
}

void RotationController::setIntervalSec(uint16_t seconds) {
  // Snap first; presets are guaranteed within [MIN, MAX] via static_assert in
  // rotation_presets.h, so the previous explicit clamps were dead code.
  seconds = rotationPresetSnapSeconds(seconds);
  if (seconds == _intervalSec) return;

  _intervalSec = seconds;
  _eepromStore.writeRotationIntervalSec(_intervalSec);

  if (_mode != RotationMode::Off) {
    restartTimer();
  }
  _stateNotifier.stateChanged();
}

void RotationController::disable() {
  if (_mode == RotationMode::Off) return;

  _mode = RotationMode::Off;
  _eepromStore.writeRotationMode(_mode);
  _timer.stop();
  _stateNotifier.stateChanged();
}

void RotationController::restartTimer() {
  _timer.setInterval(static_cast<unsigned long>(_intervalSec) * 1000UL);
  _timer.start();
}

void RotationController::rotateNow() {
  if (_mode == RotationMode::Random) {
    _effects.setRandomEffect();
  } else if (_mode == RotationMode::Sequential) {
    _effects.setNextEffect();
  }
}

void RotationController::onManualRotation() {
  if (_mode == RotationMode::Off) return;
  restartTimer();
}

void RotationController::timerCallback() {
  if (_mode == RotationMode::Off) return;

  rotateNow();
  _stateNotifier.stateChanged();
}
