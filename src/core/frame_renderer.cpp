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
  const bool effectChanged = effects_.tick();
  if (effectChanged) {
    stateNotifier_.stateChanged();
  }
  notifications_.tick();

  const NotificationFrame& notification = notifications_.frame();
  const bool transitionActive = effects_.isTransitioning();
  const bool parameterTransitionActive = effects_.isParameterTransitioning();
  const bool forceEffectRender =
    power_.isFading() || transitionActive || parameterTransitionActive || notification.isVisible();
  const bool visualActive = forceEffectRender;

  const uint32_t now = millis();
  const bool frameDue = forceShow || !visualActive || now - lastFrameMs_ >= FRAME_MS;

  if (!frameDue) return;

  if (visualActive) {
    lastFrameMs_ = now;
  }

  bool frameChanged = false;

  if (power_.isEffectVisible()) {
    LoopProfiler::measure(LoopProfiler::EFFECT_RENDER, [&]() { frameChanged = effects_.render(forceEffectRender); });

    const uint8_t combinedOpacity = scale8(power_.effectOpacity(), effects_.transitionOpacity());
    if (combinedOpacity < 255) {
      if (combinedOpacity == 0) {
        led_.clearLeds();
      } else {
        led_.scale(combinedOpacity);
      }
    }

    if (transitionActive) {
      frameChanged = true;
    }
  } else {
    led_.clearLeds();
    frameChanged = true;
  }

  if (notification.isVisible()) {
    if (notification.backdropDim > 0) {
      led_.fadeToBlack(notification.backdropDim);
    }
    NotificationOverlay overlay(led_, notification.opacity, notification.backdropDim);
    notifications_.renderOverlay(overlay, notification);
    frameChanged = true;
  }

  const bool visible = power_.isEffectVisible() || notification.isVisible();
  showOrBlackout(forceShow, frameChanged, visible);
}

void FrameRenderer::showOrBlackout(bool forceShow, bool frameChanged, bool visible) {
  if (visible) {
    if (forceShow || frameChanged) {
      LoopProfiler::measure(LoopProfiler::LEDS_SHOW, [this]() { led_.showLeds(effects_.outputBrightness()); });
      offFrameCleared_ = false;
    }
    return;
  }

  if (!offFrameCleared_) {
    led_.blackout();
    offFrameCleared_ = true;
  }
}
