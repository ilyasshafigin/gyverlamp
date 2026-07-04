#pragma once

#include <stdint.h>

class EffectController;
class Led;
class NotificationController;
class PowerController;
class SettingsRepository;
class StateNotifier;

class FrameRenderer {
public:
  explicit FrameRenderer(
    EffectController& effects,
    Led& led,
    NotificationController& notifications,
    PowerController& power,
    StateNotifier& stateNotifier
  )
    : _effects(effects),
      _led(led),
      _notifications(notifications),
      _power(power),
      _stateNotifier(stateNotifier) {}

  void render(bool forceShow = false);
  void renderNow() { render(true); }

private:
  EffectController& _effects;
  Led& _led;
  NotificationController& _notifications;
  PowerController& _power;
  StateNotifier& _stateNotifier;

  uint32_t _lastFrameMs = 0;
  bool _offFrameCleared = false;

  void showOrBlackout(bool forceShow, bool frameChanged, bool visible);
};
