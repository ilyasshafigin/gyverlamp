#include <Arduino.h>

#include "../config.h"
#include "../audio/audio_config.h"
#include "../audio/audio_modulation.h"
#include "../audio/audio_service.h"
#include "../hardware/led.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "../time/time_service.h"
#include "../util/loop_profiler.h"
#include "catalog.h"
#include "controller.h"
#include "palette_catalog.h"

void EffectController::init() {
  if (!setEffectImmediate(_eeprom.readCurrentEffectId())) {
    setEffectImmediate(Effects::fallback());
  }
}

bool EffectController::render(bool force) {
  if (!_outputEnabled) return false;

  if (!force) {
    if (millis() - _tickTimer < FRAME_MS) {
      return false;
    }
  }

  _tickTimer = millis();

  const uint32_t nowMs = millis();
  uint32_t deltaMs = nowMs - _lastRenderMs;
  if (deltaMs > 100U) deltaMs = 100U;
  _lastRenderMs = nowMs;

  const EffectSettings& settings = _settings.getEffectSettings(_currentEffectId);
  const CRGBPalette16* palette = Palettes::getPalette(_settings.getSelectedPalette());
  const AudioFrame& audio = _audio.frame();
  const AudioConfig& audioConfig = _audio.config();

  RuntimeEffectSettings runtimeSettings = AudioModulation::applyModulation(settings, audio, audioConfig);

  _runtimeBrightness = runtimeSettings.brightness;
  _runtimeBrightnessValid = true;

  if (_currentEffect) {
    EffectContext ctx(
      runtimeSettings.brightness,
      runtimeSettings.speed,
      runtimeSettings.scale,
      _red, _green, _blue,
      nowMs, deltaMs,
      palette,
      audio,
      audioConfig,
      _led,
      _time
    );

    LoopProfiler::measure(LoopProfiler::EFFECT_RENDER, [&]() {
      _currentEffect->render(ctx);
      });

    return true;
  }

  return force;
}

void EffectController::setupCurrentEffect() {
  if (!_currentEffect) return;

  _lastRenderMs = millis();
  const uint32_t nowMs = millis();
  const uint32_t deltaMs = 0;

  const EffectSettings& settings = _settings.getEffectSettings(_currentEffectId);
  const RuntimeEffectSettings runtimeSettings = RuntimeEffectSettings::fromSettings(settings);
  const CRGBPalette16* palette = Palettes::getPalette(_settings.getSelectedPalette());
  const AudioFrame& audio = _audio.frame();
  const AudioConfig& audioConfig = _audio.config();

  _runtimeBrightness = runtimeSettings.brightness;
  _runtimeBrightnessValid = true;

  EffectContext ctx(
    runtimeSettings.brightness,
    runtimeSettings.speed,
    runtimeSettings.scale,
    _red, _green, _blue,
    nowMs, deltaMs,
    palette,
    audio,
    audioConfig,
    _led,
    _time
  );
  _currentEffect->setup(ctx);
}

bool EffectController::setEffect(Effects::Id effectId) {
  if (!Effects::isValid(effectId)) return false;

  if (_transitionPhase == TransitionPhase::Idle) {
    if (effectId == _currentEffectId) {
      _pendingEffectId = Effects::Id::INVALID;
      return true;
    }
  } else {
    if (effectId == _pendingEffectId) return true;
  }

  _pendingEffectId = effectId;
  return true;
}

bool EffectController::setEffectImmediate(Effects::Id effectId) {
  if (!Effects::isValid(effectId)) return false;

  switchEffectNow(effectId);
  _transitionOpacity.snapTo(255);
  _transitionPhase = TransitionPhase::Idle;
  _pendingEffectId = Effects::Id::INVALID;
  return true;
}

bool EffectController::switchEffectNow(Effects::Id effectId) {
  if (_currentEffect) {
    _currentEffect->~Effect();
    _currentEffect = nullptr;
  }

  _currentEffectId = effectId;
  _currentEffect = Effects::createEffect(_currentEffectId, _effectBuffer);

  _led.clearLeds();
  _runtimeBrightnessValid = false;
  setupCurrentEffect();
  _settings.markEffectSettingsChanged();
  return true;
}

bool EffectController::updateTransition() {
  const uint32_t nowMs = millis();
  _transitionOpacity.tick(nowMs);

  if (_transitionPhase == TransitionPhase::Idle) {
    if (_pendingEffectId != Effects::Id::INVALID && _pendingEffectId != _currentEffectId) {
      _transitionOpacity.fadeTo(0, EFFECT_FADE_OUT_MS, nowMs);
      _transitionPhase = TransitionPhase::FadingOut;
      return false;
    }
  } else if (_transitionPhase == TransitionPhase::FadingOut) {
    if (_transitionOpacity.value() == 0 && _transitionOpacity.target() == 0) {
      switchEffectNow(_pendingEffectId);
      _transitionOpacity.fadeTo(255, EFFECT_FADE_IN_MS, nowMs);
      _transitionPhase = TransitionPhase::FadingIn;
      return true;
    }
  } else if (_transitionPhase == TransitionPhase::FadingIn) {
    if (_transitionOpacity.value() == 255 && _transitionOpacity.target() == 255) {
      _transitionPhase = TransitionPhase::Idle;
      if (_pendingEffectId == _currentEffectId) {
        _pendingEffectId = Effects::Id::INVALID;
      }
      return false;
    }
  }

  return false;
}

bool EffectController::resetEffectSettingsToDefaults() {
  EffectSettings defaults[Effects::COUNT];
  for (uint8_t i = 0; i < Effects::COUNT; i++) {
    defaults[i] = EffectSettings::fromSpec(Effects::getEffectSettingsSpec(Effects::toId(i)));
  }

  if (!_settings.resetEffectSettingsToDefaults(defaults)) return false;

  setupCurrentEffect();
  if (_outputEnabled) {
    render(true);
  }
  return true;
}

void EffectController::setNextEffect() {
  const Effects::Id current = getSelectedEffectId();
  for (uint8_t i = 0; i < Effects::DISPLAY_COUNT; i++) {
    if (Effects::DISPLAY_ORDER[i] != current) continue;

    const uint8_t nextIndex = (i + 1 >= Effects::DISPLAY_COUNT) ? 0 : i + 1;
    setEffect(Effects::DISPLAY_ORDER[nextIndex]);
    return;
  }

  setEffect(Effects::DISPLAY_ORDER[0]);
}

void EffectController::setPreviousEffect() {
  const Effects::Id current = getSelectedEffectId();
  for (uint8_t i = 0; i < Effects::DISPLAY_COUNT; i++) {
    if (Effects::DISPLAY_ORDER[i] != current) continue;

    const uint8_t previousIndex = (i == 0) ? Effects::DISPLAY_COUNT - 1 : i - 1;
    setEffect(Effects::DISPLAY_ORDER[previousIndex]);
    return;
  }

  setEffect(Effects::DISPLAY_ORDER[0]);
}

void EffectController::setRandomEffect() {
  const Effects::Id effectId = Effects::DISPLAY_ORDER[random(0, Effects::DISPLAY_COUNT)];
  setEffect(effectId);
}

uint8_t EffectController::getEffectBrightness() const {
  return _settings.getEffectSettings(getSelectedEffectId()).brightness;
}

uint8_t EffectController::getOutputBrightness() const {
  if (_runtimeBrightnessValid) {
    return _runtimeBrightness;
  }
  return _settings.getEffectSettings(_currentEffectId).brightness;
}

void EffectController::setEffectBrightness(uint8_t value) {
  setEffectParam(&EffectSettings::brightness, value, 0);
}

void EffectController::setEffectSpeed(uint8_t value) {
  setEffectParam(&EffectSettings::speed, value, EFFECT_PARAM_SPEED);
}

void EffectController::setEffectScale(uint8_t value) {
  setEffectParam(&EffectSettings::scale, value, EFFECT_PARAM_SCALE);
}

void EffectController::setEffectParam(uint8_t EffectSettings::* field, uint8_t value, uint8_t changedParam) {
  const Effects::Id effectId = getSelectedEffectId();
  EffectSettings& effectSettings = _settings.getEffectSettings(effectId);
  if (effectSettings.*field == value) return;
  effectSettings.*field = value;

  if (field == &EffectSettings::brightness && _currentEffectId == effectId) {
    _runtimeBrightness = value;
    _runtimeBrightnessValid = true;
  }

  if (_currentEffectId == effectId && changedParam != 0) {
    const EffectSettingsSpec spec = Effects::getEffectSettingsSpec(effectId);
    if ((spec.resetOnChange & changedParam) != 0) {
      setupCurrentEffect();
    }
  }
  _settings.markEffectSettingsChanged();
}

Palettes::Id EffectController::getSelectedPalette() const { return _settings.getSelectedPalette(); }

void EffectController::setPalette(Palettes::Id paletteId) {
  const Effects::Id effectId = getSelectedEffectId();
  _settings.setPalette(paletteId);
  if (_currentEffectId == effectId) {
    setupCurrentEffect();
  }
}
