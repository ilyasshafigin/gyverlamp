#include "rotation_controller.h"

#include <Arduino.h>

#include "../effect/controller.h"
#include "../storage/eeprom_store.h"

#include "rotation_presets.h"
#include "state_notifier.h"

void RotationController::init() {
  mode_ = eepromStore_.readRotationMode();
  intervalSec_ = eepromStore_.readRotationIntervalSec();
  // One-shot: migrate legacy non-preset value to the nearest preset.
  // Fires once on first boot after the upgrade; subsequent boots hit the
  // equality guard below because EEPROM now stores a snapped preset.
  const uint16_t snapped = rotationPresetSnapSeconds(intervalSec_);
  if (snapped != intervalSec_) {
    intervalSec_ = snapped;
    eepromStore_.writeRotationIntervalSec(intervalSec_);
  }
  timer_.setOnTimer([this]() { this->timerCallback(); });
  restartTimer();
}

void RotationController::tick(bool powerOn) {
  if (!powerOn) {
    powerWasOn_ = false;
    return;
  }

  if (!powerWasOn_) {
    powerWasOn_ = true;
    if (mode_ != RotationMode::Off) restartTimer();
  }

  timer_.update();
}

void RotationController::setMode(RotationMode mode) {
  if (mode == mode_) return;

  mode_ = mode;
  eepromStore_.writeRotationMode(mode_);

  if (mode_ == RotationMode::Off) {
    timer_.stop();
    stateNotifier_.stateChanged();
    return;
  }

  restartTimer();
  rotateNow();
  stateNotifier_.stateChanged();
}

void RotationController::setIntervalSec(uint16_t seconds) {
  // Snap first; presets are guaranteed within [MIN, MAX] via static_assert in
  // rotation_presets.h, so the previous explicit clamps were dead code.
  seconds = rotationPresetSnapSeconds(seconds);
  if (seconds == intervalSec_) return;

  intervalSec_ = seconds;
  eepromStore_.writeRotationIntervalSec(intervalSec_);

  if (mode_ != RotationMode::Off) {
    restartTimer();
  }
  stateNotifier_.stateChanged();
}

void RotationController::disable() {
  if (mode_ == RotationMode::Off) return;

  mode_ = RotationMode::Off;
  eepromStore_.writeRotationMode(mode_);
  timer_.stop();
  stateNotifier_.stateChanged();
}

void RotationController::restartTimer() {
  timer_.setInterval(static_cast<unsigned long>(intervalSec_) * 1000UL);
  timer_.start();
}

void RotationController::rotateNow() {
  if (mode_ == RotationMode::Random) {
    effects_.setRandomEffect();
  } else if (mode_ == RotationMode::Sequential) {
    effects_.setNextEffect();
  }
}

void RotationController::onManualRotation() {
  if (mode_ == RotationMode::Off) return;
  restartTimer();
}

void RotationController::timerCallback() {
  if (mode_ == RotationMode::Off) return;

  rotateNow();
  stateNotifier_.stateChanged();
}
