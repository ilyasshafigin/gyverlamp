#include "dimmer.h"

void NotificationDimmer::prepareFrame(bool backgroundUpdated, uint8_t targetDim, uint32_t durationMs) {
  if (!wasActive_) {
    wasActive_ = true;
    startedMs_ = millis();
    appliedDim_ = 0;
  }

  const uint8_t dim = currentAmount(targetDim, durationMs);
  apply(dim, backgroundUpdated);
}

void NotificationDimmer::prepareEndingFrame(bool backgroundUpdated, uint8_t targetDim, uint32_t endingStartedMs) {
  const uint32_t elapsed = millis() - endingStartedMs;

  uint8_t fadeOut = 0;
  if (elapsed < FADE_OUT_MS) {
    const uint8_t progress = static_cast<uint32_t>(elapsed) * 255 / FADE_OUT_MS;
    fadeOut = 255 - ease8InOutQuad(progress);
  }

  apply(scale8(targetDim, fadeOut), backgroundUpdated);
}

uint8_t NotificationDimmer::currentAmount(uint8_t targetDim, uint32_t durationMs) const {
  const uint32_t elapsed = millis() - startedMs_;

  uint8_t fadeIn = 255;
  if (elapsed < FADE_IN_MS) {
    fadeIn = ease8InOutQuad(static_cast<uint32_t>(elapsed) * 255 / FADE_IN_MS);
  }

  uint8_t fadeOut = 255;
  if (durationMs > 0 && elapsed + FADE_OUT_MS >= durationMs) {
    const uint32_t remaining = elapsed >= durationMs ? 0 : durationMs - elapsed;
    fadeOut = ease8InOutQuad(static_cast<uint32_t>(remaining) * 255 / FADE_OUT_MS);
  }

  return scale8(targetDim, min(fadeIn, fadeOut));
}

void NotificationDimmer::reset() {
  wasActive_ = false;
  appliedDim_ = 0;
}

void NotificationDimmer::apply(uint8_t amount, bool backgroundUpdated) {
  if (backgroundUpdated) {
    led_.fadeToBlack(amount);
    appliedDim_ = amount;
    return;
  }

  if (amount <= appliedDim_) return;

  const uint8_t delta = amount - appliedDim_;
  led_.fadeToBlack(delta);
  appliedDim_ = amount;
}
