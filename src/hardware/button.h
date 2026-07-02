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
    : _eeprom(eeprom),
    _effects(effects),
    _notifications(notifications),
    _power(power),
    _rotation(rotation),
    _settings(settings),
    _stateNotifier(stateNotifier),
    _pin(pin) {
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
  bool isConnected() const { return _connected; }
  bool isEnabled() const { return _enabled; }
#else
  bool isConnected() const { return false; }
  bool isEnabled() const { return false; }
#endif
  bool setEnabled(bool enabled);

private:
#ifdef USE_BUTTON
  EepromStore& _eeprom;
  EffectController& _effects;
  NotificationController& _notifications;
  PowerController& _power;
  RotationController& _rotation;
  SettingsRepository& _settings;
  StateNotifier& _stateNotifier;
  Button _button;
  int8_t _pin;
  bool _connected = false;
  bool _enabled = true;
  bool _brightDirection = false;
#endif
};
