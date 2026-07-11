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
    : _eepromStore(eepromStore),
      _effects(effects),
      _stateNotifier(stateNotifier),
      _timer(ROTATION_INTERVAL_SEC_DEFAULT * 1000UL) {}

  void init();
  void tick(bool powerOn);

  RotationMode getMode() const { return _mode; }
  bool isActive() const { return _mode != RotationMode::Off; }
  uint16_t getIntervalSec() const { return _intervalSec; }

  void setMode(RotationMode mode);
  void setIntervalSec(uint16_t seconds);
  void disable();
  void onManualRotation();
  void setEnabled(bool value) { value ? setMode(RotationMode::Random) : disable(); }

private:
  EepromStore& _eepromStore;
  EffectController& _effects;
  StateNotifier& _stateNotifier;
  Timer _timer;
  RotationMode _mode = RotationMode::Off;
  uint16_t _intervalSec = ROTATION_INTERVAL_SEC_DEFAULT;
  bool _powerWasOn = false;

  void timerCallback();
  void restartTimer();
  void rotateNow();
};
