#include <FastLED.h>

#include "fade_animator.h"

void FadeAnimator::snapTo(uint8_t value) {
  from_ = value;
  to_ = value;
  value_ = value;
  startedMs_ = millis();
  durationMs_ = 0;
}

void FadeAnimator::fadeTo(uint8_t target, uint16_t durationMs, uint32_t now) {
  if (to_ == target && value_ == target) return;

  from_ = value_;
  to_ = target;
  startedMs_ = now;
  durationMs_ = durationMs;

  if (durationMs_ == 0) {
    snapTo(target);
  }
}

bool FadeAnimator::tick(uint32_t now) {
  if (value_ == to_) return false;

  if (durationMs_ == 0) {
    value_ = to_;
    return true;
  }

  const uint32_t elapsed = now - startedMs_;
  if (elapsed >= durationMs_) {
    value_ = to_;
    return true;
  }

  const uint8_t progress = static_cast<uint32_t>(elapsed) * 255 / durationMs_;
  const uint8_t eased = ease8InOutQuad(progress);

  const int16_t delta = static_cast<int16_t>(to_) - static_cast<int16_t>(from_);
  value_ = static_cast<uint8_t>(static_cast<int16_t>(from_) + (delta * eased) / 255);

  return true;
}
