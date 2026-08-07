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
    : effects_(effects),
      led_(led),
      notifications_(notifications),
      power_(power),
      stateNotifier_(stateNotifier) {}

  void render(bool forceShow = false);
  void renderNow() { render(true); }

private:
  EffectController& effects_;
  Led& led_;
  NotificationController& notifications_;
  PowerController& power_;
  StateNotifier& stateNotifier_;

  uint32_t lastFrameMs_ = 0;
  bool offFrameCleared_ = false;

  void showOrBlackout(bool forceShow, bool frameChanged, bool visible);
};
