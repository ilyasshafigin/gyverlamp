#pragma once

#include "../util/fade_animator.h"
#include <Arduino.h>

class EffectController;
class EepromStore;
class StateNotifier;

class PowerController {
public:
  explicit PowerController(EepromStore& eeprom, EffectController& effects, StateNotifier& stateNotifier)
    : eeprom_(eeprom),
      effects_(effects),
      stateNotifier_(stateNotifier) {}

  void init();

  uint16_t getAutoOffMinutes() const { return autoOffMinutes_; }
  bool setAutoOffMinutes(int minutes);
  uint32_t getAutoOffRemainingSeconds() const;

  bool isOn() const { return on_; }
  bool isEffectVisible() const { return effectOpacity_.value() > 0; }
  bool isFullyOff() const { return !on_ && effectOpacity_.value() == 0; }
  uint8_t getEffectOpacity() const { return effectOpacity_.value(); }
  bool isFading() const { return effectOpacity_.isRunning(); }

  void on();
  void off();
  void toggle() { on_ ? off() : on(); }
  void setOn(bool value) { value ? on() : off(); }
  bool tick();

private:
  EepromStore& eeprom_;
  EffectController& effects_;
  StateNotifier& stateNotifier_;

  bool on_ = false;
  FadeAnimator effectOpacity_;

  uint16_t autoOffMinutes_ = 0;
  uint32_t turnedOnAtMs_ = 0;

  void resetAutoOffTimer();
};
