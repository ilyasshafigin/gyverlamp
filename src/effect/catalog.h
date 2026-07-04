#pragma once

#include <cstddef>
#include <Arduino.h>
#include "effect.h"
#include "effects.h"
#include "ids.h"
#include "settings.h"

#define EFFECT_REGISTRY(X) \
  X(EffectColor)           \
  X(EffectColorChange)     \
  X(EffectGradient)        \
  X(EffectFire)            \
  X(EffectRainbow)         \
  X(EffectNoise)           \
  X(EffectSnowstorm)       \
  X(EffectMatrix)          \
  X(EffectPaintball)       \
  X(EffectSpiral)          \
  X(EffectWarmLight)       \
  X(EffectTwinkles)        \
  X(EffectShadows)         \
  X(EffectButterflys)      \
  X(EffectThunderstorm)    \
  X(EffectNexus)           \
  X(EffectClock)           \
  X(EffectLiquidLamp)      \
  X(EffectNorthernLights)  \
  X(EffectPicasso)         \
  X(EffectEqualizer)       \
  X(EffectOctopus)

namespace Effects {

  namespace detail {
    struct EffectStorageMinimum {
      char value;
    };

    template <typename T, typename... Rest> struct MaxEffectSize {
      static constexpr size_t tail = MaxEffectSize<Rest...>::value;
      static constexpr size_t value = sizeof(T) > tail ? sizeof(T) : tail;
    };

    template <typename T> struct MaxEffectSize<T> {
      static constexpr size_t value = sizeof(T);
    };
  } // namespace detail

  constexpr size_t STORAGE_SIZE = detail::MaxEffectSize<
#define EFFECT_STORAGE_TYPE(T) T,
    EFFECT_REGISTRY(EFFECT_STORAGE_TYPE)
#undef EFFECT_STORAGE_TYPE
      detail::EffectStorageMinimum>::value;

  static_assert(STORAGE_SIZE <= 64, "Effect storage grew; increase budget intentionally");

  constexpr Id DISPLAY_ORDER[] = {
#define EFFECT_DISPLAY_ID(T) T::ID,
    EFFECT_REGISTRY(EFFECT_DISPLAY_ID)
#undef EFFECT_DISPLAY_ID
  };

  constexpr uint8_t DISPLAY_COUNT = sizeof(DISPLAY_ORDER) / sizeof(DISPLAY_ORDER[0]);
  static_assert(DISPLAY_COUNT > 0, "Effect registry must include at least one effect");
  static_assert(DISPLAY_COUNT <= COUNT, "Effect registry cannot include more effects than known ids");

  Effect* createEffect(Id id, void* buffer);

  EffectSettingsSpec getEffectSettingsSpec(Id id);

  const char* getEffectName(Id id);
  Id getEffectId(const String& effect);
  Id getEffectId(const char* effect);

  Id fallback();
  bool isValid(Id id);
  bool isValid(uint8_t raw);
  Id clamp(Id id);
  Id clamp(uint8_t raw);
} // namespace Effects
