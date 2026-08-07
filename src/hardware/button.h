#pragma once

#include <Arduino.h>
#ifdef USE_BUTTON
#include <EncButton.h>
#endif

class EepromStore;
class EffectController;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class StateNotifier;

class TouchButton {
public:
  explicit TouchButton(
    EepromStore& eeprom,
    EffectController& effects,
    NotificationController& notifications,
    PowerController& power,
    RotationController& rotation,
    SettingsRepository& settings,
    StateNotifier& stateNotifier,
    int8_t pin
  )
#ifdef USE_BUTTON
    : eeprom_(eeprom),
      effects_(effects),
      notifications_(notifications),
      power_(power),
      rotation_(rotation),
      settings_(settings),
      stateNotifier_(stateNotifier),
      pin_(pin) {
  }
#else
  {
    (void)eeprom;
    (void)effects;
    (void)notifications;
    (void)power;
    (void)rotation;
    (void)settings;
    (void)stateNotifier;
    (void)pin;
  }
#endif

  void detect();
  void init();
  void tick();

#ifdef USE_BUTTON
  bool isConnected() const { return connected_; }
  bool isEnabled() const { return enabled_; }
#else
  bool isConnected() const { return false; }
  bool isEnabled() const { return false; }
#endif
  bool setEnabled(bool enabled);

private:
#ifdef USE_BUTTON
  EepromStore& eeprom_;
  EffectController& effects_;
  NotificationController& notifications_;
  PowerController& power_;
  RotationController& rotation_;
  SettingsRepository& settings_;
  StateNotifier& stateNotifier_;
  Button button_;
  int8_t pin_;
  bool connected_ = false;
  bool enabled_ = true;
  bool brightDirection_ = false;
#endif
};
