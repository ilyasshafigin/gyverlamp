#include "frame_renderer.h"

#include <Arduino.h>

#include "../config.h"
#include "../effect/controller.h"
#include "../hardware/led.h"
#include "../notification/controller.h"
#include "../notification/types.h"
#include "../storage/settings_repository.h"
#include "../util/loop_profiler.h"
#include "power_controller.h"
#include "state_notifier.h"

void FrameRenderer::render(bool forceShow) {
  if (_effects.updateTransition()) {
    _stateNotifier.stateChanged();
  }
  _notifications.tick();

  const NotificationFrame& notification = _notifications.frame();
  const bool transitionActive = _effects.isTransitioning();
  const bool forceEffectRender = _power.isFading() || transitionActive || notification.isVisible();
  const bool visualActive = forceEffectRender;

  const uint32_t now = millis();
  const bool frameDue = forceShow || !visualActive || now - _lastFrameMs >= FRAME_MS;

  if (!frameDue) return;

  if (visualActive) {
    _lastFrameMs = now;
  }

  bool frameChanged = false;

  if (_power.isEffectVisible()) {
    LoopProfiler::measure(LoopProfiler::EFFECT_RENDER, [&]() {
      frameChanged = _effects.render(forceEffectRender);
      });

    const uint8_t combinedOpacity = scale8(_power.getEffectOpacity(), _effects.getTransitionOpacity());
    if (combinedOpacity < 255) {
      if (combinedOpacity == 0) {
        _led.clearLeds();
      } else {
        _led.scale(combinedOpacity);
      }
    }

    if (transitionActive) {
      frameChanged = true;
    }
  } else {
    _led.clearLeds();
    frameChanged = true;
  }

  if (notification.isVisible()) {
    if (notification.backdropDim > 0) {
      _led.fadeToBlack(notification.backdropDim);
    }
    NotificationOverlay overlay(_led, notification.opacity, notification.backdropDim);
    _notifications.renderOverlay(overlay, notification);
    frameChanged = true;
  }

  const bool visible = _power.isEffectVisible() || notification.isVisible();
  showOrBlackout(forceShow, frameChanged, visible);
}

void FrameRenderer::showOrBlackout(bool forceShow, bool frameChanged, bool visible) {
  if (visible) {
    if (forceShow || frameChanged) {
      LoopProfiler::measure(LoopProfiler::LEDS_SHOW, [this]() {
        _led.showLeds(_effects.getOutputBrightness());
        });
      _offFrameCleared = false;
    }
    return;
  }

  if (!_offFrameCleared) {
    _led.blackout();
    _offFrameCleared = true;
  }
}
