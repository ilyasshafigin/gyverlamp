#pragma once

#include <Arduino.h>
#include "../util/timer.h"
#include "rotation_mode.h"

class EepromStore;
class EffectController;
class StateNotifier;

class RotationController {
public:
  explicit RotationController(EepromStore& eepromStore, EffectController& effects, StateNotifier& stateNotifier)
    : eepromStore_(eepromStore),
      effects_(effects),
      stateNotifier_(stateNotifier),
      timer_(ROTATION_INTERVAL_SEC_DEFAULT * 1000UL) {}

  void init();
  void tick(bool powerOn);

  RotationMode getMode() const { return mode_; }
  bool isActive() const { return mode_ != RotationMode::Off; }
  uint16_t getIntervalSec() const { return intervalSec_; }

  void setMode(RotationMode mode);
  void setIntervalSec(uint16_t seconds);
  void disable();
  void onManualRotation();
  void setEnabled(bool value) { value ? setMode(RotationMode::Random) : disable(); }

private:
  EepromStore& eepromStore_;
  EffectController& effects_;
  StateNotifier& stateNotifier_;
  Timer timer_;
  RotationMode mode_ = RotationMode::Off;
  uint16_t intervalSec_ = ROTATION_INTERVAL_SEC_DEFAULT;
  bool powerWasOn_ = false;

  void timerCallback();
  void restartTimer();
  void rotateNow();
};
