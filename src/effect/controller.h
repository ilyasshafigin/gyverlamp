#pragma once

#include "catalog.h"
#include "effect.h"
#include "ids.h"
#include "palette_ids.h"
#include "settings.h"
#include "../util/fade_animator.h"
#include "../util/linear_u8_rate_limiter.h"

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
    : audio_(audio),
      eeprom_(eeprom),
      led_(led),
      settings_(settings),
      time_(time) {}

  void init();
  bool tick();
  bool render(bool force = false);

  Effects::Id getActiveEffectId() const { return currentEffectId_; }
  Effects::Id getSelectedEffectId() const {
    return pendingEffectId_ != Effects::Id::INVALID ? pendingEffectId_ : currentEffectId_;
  }
  uint8_t getRed() const { return red_; }
  uint8_t getGreen() const { return green_; }
  uint8_t getBlue() const { return blue_; }
  EffectSettingsSpec getActiveSettingsSpec() const { return Effects::getEffectSettingsSpec(currentEffectId_); }
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
  void setGlobalBrightness(uint8_t value);
  void setEffectBrightness(uint8_t value);
  void setEffectSpeed(uint8_t value);
  void setEffectScale(uint8_t value);
  bool resetEffectSettingsToDefaults();
  void resetCurrentEffectSettingsToDefaults();
  void setColor(uint8_t r, uint8_t g, uint8_t b) {
    red_ = r;
    green_ = g;
    blue_ = b;
  }
  void setOutputEnabled(bool enabled) { outputEnabled_ = enabled; }

  bool isTransitioning() const { return transitionPhase_ != TransitionPhase::Idle; }
  bool isParameterTransitioning() const;
  uint8_t getTransitionOpacity() const { return transitionOpacity_.value(); }

private:
  AudioService& audio_;
  EepromStore& eeprom_;
  Led& led_;
  SettingsRepository& settings_;
  TimeService& time_;
  Effect* currentEffect_ = nullptr;
  Effects::Id currentEffectId_ = Effects::kDefaultId;
  Effects::Id pendingEffectId_ = Effects::Id::INVALID;
  bool outputEnabled_ = false;
  uint8_t red_ = 255;
  uint8_t green_ = 255;
  uint8_t blue_ = 255;
  uint32_t tickTimer_ = 0;
  uint32_t lastRenderMs_ = 0;

  LinearU8RateLimiter globalBrightness_;
  LinearU8RateLimiter effectBrightness_;
  LinearU8RateLimiter effectSpeed_;
  LinearU8RateLimiter effectScale_;

  uint8_t runtimeBrightness_ = 0;
  bool runtimeBrightnessValid_ = false;

  RuntimeEffectSettings getAppliedSettings() const;
  void snapActiveEffectSettings(uint32_t now = millis());
  void retargetActiveEffectSettings(uint32_t now = millis());
  void setupCurrentEffect();
  bool switchEffectNow(Effects::Id effectId);
  void setEffectParam(uint8_t EffectSettings::* field, uint8_t value);
  bool updateTransition(uint32_t nowMs);

  static constexpr uint16_t kEffectFadeOutMs = 350;
  static constexpr uint16_t kEffectFadeInMs = 350;
  static constexpr uint8_t kParameterSnapThreshold = 5;
  static constexpr uint16_t kBrightnessRatePerSecond = 255;
  static constexpr uint16_t kSpeedScaleRatePerSecond = 255;

  enum class TransitionPhase : uint8_t { Idle, FadingOut, FadingIn };

  // Буфер для placement new. Размер вычисляется по самому большому effect.
  /*alignas(alignof(std::max_align_t)) */
  alignas(8) char effectBuffer_[Effects::kStorageSize];

  FadeAnimator transitionOpacity_;
  TransitionPhase transitionPhase_ = TransitionPhase::Idle;
};
