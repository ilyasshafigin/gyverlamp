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
  if (!setEffectImmediate(eeprom_.readCurrentEffectId())) {
    setEffectImmediate(Effects::fallback());
  }
}

bool EffectController::render(bool force) {
  if (!outputEnabled_) return false;

  if (!force) {
    if (millis() - tickTimer_ < FRAME_MS) {
      return false;
    }
  }

  tickTimer_ = millis();

  const uint32_t nowMs = millis();
  uint32_t deltaMs = nowMs - lastRenderMs_;
  if (deltaMs > 100U) deltaMs = 100U;
  lastRenderMs_ = nowMs;

  const EffectSettings& settings = settings_.getEffectSettings(currentEffectId_);
  const CRGBPalette16* palette = Palettes::getPalette(settings_.getSelectedPalette());
  const AudioFrame& audio = audio_.frame();
  const AudioConfig& audioConfig = audio_.config();

  RuntimeEffectSettings runtimeSettings = AudioModulation::applyModulation(settings, audio, audioConfig);

  runtimeBrightness_ = runtimeSettings.brightness;
  runtimeBrightnessValid_ = true;

  if (currentEffect_) {
    EffectContext ctx(
      runtimeSettings.brightness,
      runtimeSettings.speed,
      runtimeSettings.scale,
      red_,
      green_,
      blue_,
      nowMs,
      deltaMs,
      palette,
      audio,
      audioConfig,
      led_,
      time_
    );

    LoopProfiler::measure(LoopProfiler::EFFECT_RENDER, [&]() { currentEffect_->render(ctx); });

    return true;
  }

  return force;
}

void EffectController::setupCurrentEffect() {
  if (!currentEffect_) return;

  lastRenderMs_ = millis();
  const uint32_t nowMs = millis();
  const uint32_t deltaMs = 0;

  const EffectSettings& settings = settings_.getEffectSettings(currentEffectId_);
  const RuntimeEffectSettings runtimeSettings = RuntimeEffectSettings::fromSettings(settings);
  const CRGBPalette16* palette = Palettes::getPalette(settings_.getSelectedPalette());
  const AudioFrame& audio = audio_.frame();
  const AudioConfig& audioConfig = audio_.config();

  runtimeBrightness_ = runtimeSettings.brightness;
  runtimeBrightnessValid_ = true;

  EffectContext ctx(
    runtimeSettings.brightness,
    runtimeSettings.speed,
    runtimeSettings.scale,
    red_,
    green_,
    blue_,
    nowMs,
    deltaMs,
    palette,
    audio,
    audioConfig,
    led_,
    time_
  );
  currentEffect_->setup(ctx);
}

bool EffectController::setEffect(Effects::Id effectId) {
  if (!Effects::isValid(effectId)) return false;

  if (transitionPhase_ == TransitionPhase::Idle) {
    if (effectId == currentEffectId_) {
      pendingEffectId_ = Effects::Id::INVALID;
      return true;
    }
  } else {
    if (effectId == pendingEffectId_) return true;
  }

  pendingEffectId_ = effectId;
  return true;
}

bool EffectController::setEffectImmediate(Effects::Id effectId) {
  if (!Effects::isValid(effectId)) return false;

  switchEffectNow(effectId);
  transitionOpacity_.snapTo(255);
  transitionPhase_ = TransitionPhase::Idle;
  pendingEffectId_ = Effects::Id::INVALID;
  return true;
}

bool EffectController::switchEffectNow(Effects::Id effectId) {
  if (currentEffect_) {
    currentEffect_->~Effect();
    currentEffect_ = nullptr;
  }

  currentEffectId_ = effectId;
  currentEffect_ = Effects::createEffect(currentEffectId_, effectBuffer_);

  led_.clearLeds();
  runtimeBrightnessValid_ = false;
  setupCurrentEffect();
  settings_.markEffectSettingsChanged();
  return true;
}

bool EffectController::updateTransition() {
  const uint32_t nowMs = millis();
  transitionOpacity_.tick(nowMs);

  if (transitionPhase_ == TransitionPhase::Idle) {
    if (pendingEffectId_ != Effects::Id::INVALID && pendingEffectId_ != currentEffectId_) {
      transitionOpacity_.fadeTo(0, kEffectFadeOutMs, nowMs);
      transitionPhase_ = TransitionPhase::FadingOut;
      return false;
    }
  } else if (transitionPhase_ == TransitionPhase::FadingOut) {
    if (transitionOpacity_.value() == 0 && transitionOpacity_.target() == 0) {
      switchEffectNow(pendingEffectId_);
      transitionOpacity_.fadeTo(255, kEffectFadeInMs, nowMs);
      transitionPhase_ = TransitionPhase::FadingIn;
      return true;
    }
  } else if (transitionPhase_ == TransitionPhase::FadingIn) {
    if (transitionOpacity_.value() == 255 && transitionOpacity_.target() == 255) {
      transitionPhase_ = TransitionPhase::Idle;
      if (pendingEffectId_ == currentEffectId_) {
        pendingEffectId_ = Effects::Id::INVALID;
      }
      return false;
    }
  }

  return false;
}

bool EffectController::resetEffectSettingsToDefaults() {
  EffectSettings defaults[Effects::kCount];
  for (uint8_t i = 0; i < Effects::kCount; i++) {
    defaults[i] = EffectSettings::fromSpec(Effects::getEffectSettingsSpec(Effects::toId(i)));
  }

  if (!settings_.resetEffectSettingsToDefaults(defaults)) return false;

  setupCurrentEffect();
  if (outputEnabled_) {
    render(true);
  }
  return true;
}

void EffectController::resetCurrentEffectSettingsToDefaults() {
  const Effects::Id effectId = getSelectedEffectId();
  const EffectSettings defaults = EffectSettings::fromSpec(Effects::getEffectSettingsSpec(effectId));
  settings_.resetEffectSettingsToDefaults(effectId, defaults);

  if (currentEffectId_ == effectId) {
    setupCurrentEffect();
    if (outputEnabled_) {
      render(true);
    }
  }
}

void EffectController::setNextEffect() {
  const Effects::Id current = getSelectedEffectId();
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    if (Effects::kDisplayOrder[i] != current) continue;

    const uint8_t nextIndex = (i + 1 >= Effects::kDisplayCount) ? 0 : i + 1;
    setEffect(Effects::kDisplayOrder[nextIndex]);
    return;
  }

  setEffect(Effects::kDisplayOrder[0]);
}

void EffectController::setPreviousEffect() {
  const Effects::Id current = getSelectedEffectId();
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    if (Effects::kDisplayOrder[i] != current) continue;

    const uint8_t previousIndex = (i == 0) ? Effects::kDisplayCount - 1 : i - 1;
    setEffect(Effects::kDisplayOrder[previousIndex]);
    return;
  }

  setEffect(Effects::kDisplayOrder[0]);
}

void EffectController::setRandomEffect() {
  const Effects::Id effectId = Effects::kDisplayOrder[random(0, Effects::kDisplayCount)];
  setEffect(effectId);
}

uint8_t EffectController::getEffectBrightness() const {
  return settings_.getEffectSettings(getSelectedEffectId()).brightness;
}

uint8_t EffectController::getOutputBrightness() const {
  const uint8_t globalBrightness = settings_.getGlobalBrightness();
  if (runtimeBrightnessValid_) {
    return scale8(runtimeBrightness_, globalBrightness);
  }
  return scale8(settings_.getEffectSettings(currentEffectId_).brightness, globalBrightness);
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
  EffectSettings& effectSettings = settings_.getEffectSettings(effectId);
  if (effectSettings.*field == value) return;
  effectSettings.*field = value;

  if (field == &EffectSettings::brightness && currentEffectId_ == effectId) {
    runtimeBrightness_ = value;
    runtimeBrightnessValid_ = true;
  }

  if (currentEffectId_ == effectId && changedParam != 0) {
    const EffectSettingsSpec spec = Effects::getEffectSettingsSpec(effectId);
    if ((spec.resetOnChange & changedParam) != 0) {
      setupCurrentEffect();
    }
  }
  settings_.markEffectSettingsChanged();
}

Palettes::Id EffectController::getSelectedPalette() const {
  return settings_.getSelectedPalette();
}

void EffectController::setPalette(Palettes::Id paletteId) {
  const Effects::Id effectId = getSelectedEffectId();
  settings_.setPalette(paletteId);
  if (currentEffectId_ == effectId) {
    setupCurrentEffect();
  }
}
