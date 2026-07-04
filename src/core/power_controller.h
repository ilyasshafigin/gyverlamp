#pragma once

#include "../util/fade_animator.h"
#include <Arduino.h>

class EffectController;
class EepromStore;
class StateNotifier;

class PowerController {
public:
  explicit PowerController(EepromStore& eeprom, EffectController& effects, StateNotifier& stateNotifier)
    : _eeprom(eeprom),
      _effects(effects),
      _stateNotifier(stateNotifier) {}

  void init();

  uint16_t getAutoOffMinutes() const { return _autoOffMinutes; }
  bool setAutoOffMinutes(int minutes);
  uint32_t getAutoOffRemainingSeconds() const;

  bool isOn() const { return _on; }
  bool isEffectVisible() const { return _effectOpacity.value() > 0; }
  bool isFullyOff() const { return !_on && _effectOpacity.value() == 0; }
  uint8_t getEffectOpacity() const { return _effectOpacity.value(); }
  bool isFading() const { return _effectOpacity.isRunning(); }

  void on();
  void off();
  void toggle() { _on ? off() : on(); }
  void setOn(bool value) { value ? on() : off(); }
  bool tick();

private:
  EepromStore& _eeprom;
  EffectController& _effects;
  StateNotifier& _stateNotifier;

  bool _on = false;
  FadeAnimator _effectOpacity;

  uint16_t _autoOffMinutes = 0;
  uint32_t _turnedOnAtMs = 0;

  void resetAutoOffTimer();
};
