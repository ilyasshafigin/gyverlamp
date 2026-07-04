#pragma once

#include "catalog.h"
#include "effect.h"
#include "ids.h"
#include "palette_ids.h"
#include "settings.h"
#include "../util/fade_animator.h"

class AudioService;
class EepromStore;
class Led;
class SettingsRepository;
class TimeService;

class EffectController {
public:
  explicit EffectController(
    AudioService& audio, EepromStore& eeprom, Led& led, SettingsRepository& settings, TimeService& time
  )
    : _audio(audio),
      _eeprom(eeprom),
      _led(led),
      _settings(settings),
      _time(time) {}

  void init();
  bool render(bool force = false);

  Effects::Id getActiveEffectId() const { return _currentEffectId; }
  Effects::Id getSelectedEffectId() const {
    return _pendingEffectId != Effects::Id::INVALID ? _pendingEffectId : _currentEffectId;
  }
  uint8_t getRed() const { return _red; }
  uint8_t getGreen() const { return _green; }
  uint8_t getBlue() const { return _blue; }
  EffectSettingsSpec getActiveSettingsSpec() const { return Effects::getEffectSettingsSpec(_currentEffectId); }
  EffectSettingsSpec getSelectedSettingsSpec() const { return Effects::getEffectSettingsSpec(getSelectedEffectId()); }

  Palettes::Id getSelectedPalette() const;
  void setPalette(Palettes::Id paletteId);

  bool setEffect(Effects::Id effectId);
  bool setEffectImmediate(Effects::Id effectId);
  void setNextEffect();
  void setPreviousEffect();
  void setRandomEffect();
  uint8_t getEffectBrightness() const;
  uint8_t getOutputBrightness() const;
  void setEffectBrightness(uint8_t value);
  void setEffectSpeed(uint8_t value);
  void setEffectScale(uint8_t value);
  bool resetEffectSettingsToDefaults();
  void setColor(uint8_t r, uint8_t g, uint8_t b) {
    _red = r;
    _green = g;
    _blue = b;
  }
  void setOutputEnabled(bool enabled) { _outputEnabled = enabled; }

  bool updateTransition();
  bool isTransitioning() const { return _transitionPhase != TransitionPhase::Idle; }
  uint8_t getTransitionOpacity() const { return _transitionOpacity.value(); }

private:
  AudioService& _audio;
  EepromStore& _eeprom;
  Led& _led;
  SettingsRepository& _settings;
  TimeService& _time;
  Effect* _currentEffect = nullptr;
  Effects::Id _currentEffectId = Effects::DEFAULT_ID;
  Effects::Id _pendingEffectId = Effects::Id::INVALID;
  bool _outputEnabled = false;
  uint8_t _red = 255;
  uint8_t _green = 255;
  uint8_t _blue = 255;
  uint32_t _tickTimer = 0;
  uint32_t _lastRenderMs = 0;

  uint8_t _runtimeBrightness = 0;
  bool _runtimeBrightnessValid = false;

  void setupCurrentEffect();
  bool switchEffectNow(Effects::Id effectId);
  void setEffectParam(uint8_t EffectSettings::* field, uint8_t value, uint8_t changedParam);

  static constexpr uint16_t EFFECT_FADE_OUT_MS = 350;
  static constexpr uint16_t EFFECT_FADE_IN_MS = 350;

  enum class TransitionPhase : uint8_t { Idle, FadingOut, FadingIn };

  // Буфер для placement new. Размер вычисляется по самому большому effect.
  /*alignas(alignof(std::max_align_t)) */
  alignas(8) char _effectBuffer[Effects::STORAGE_SIZE];

  FadeAnimator _transitionOpacity;
  TransitionPhase _transitionPhase = TransitionPhase::Idle;
};
