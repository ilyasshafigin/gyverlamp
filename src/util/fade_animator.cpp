#include <FastLED.h>

#include "fade_animator.h"

void FadeAnimator::snapTo(uint8_t value) {
  _from = value;
  _to = value;
  _value = value;
  _startedMs = millis();
  _durationMs = 0;
}

void FadeAnimator::fadeTo(uint8_t target, uint16_t durationMs, uint32_t now) {
  if (_to == target && _value == target) return;

  _from = _value;
  _to = target;
  _startedMs = now;
  _durationMs = durationMs;

  if (_durationMs == 0) {
    snapTo(target);
  }
}

bool FadeAnimator::tick(uint32_t now) {
  if (_value == _to) return false;

  if (_durationMs == 0) {
    _value = _to;
    return true;
  }

  const uint32_t elapsed = now - _startedMs;
  if (elapsed >= _durationMs) {
    _value = _to;
    return true;
  }

  const uint8_t progress = static_cast<uint32_t>(elapsed) * 255 / _durationMs;
  const uint8_t eased = ease8InOutQuad(progress);

  const int16_t delta = static_cast<int16_t>(_to) - static_cast<int16_t>(_from);
  _value = static_cast<uint8_t>(static_cast<int16_t>(_from) + (delta * eased) / 255);

  return true;
}
