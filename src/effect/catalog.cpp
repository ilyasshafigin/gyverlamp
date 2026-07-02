#include <new>
#include "catalog.h"
#include "effects.h"

namespace Effects {

  Effect* createEffect(Id id, void* buffer) {
    switch (id) {
#define EFFECT_CASE(T) case T::ID: return new (buffer) T();
      EFFECT_REGISTRY(EFFECT_CASE)
#undef EFFECT_CASE
    default:
#define EFFECT_FALLBACK(T) return new (buffer) T();
      EFFECT_REGISTRY(EFFECT_FALLBACK)
#undef EFFECT_FALLBACK
    }
  }

  EffectSettingsSpec getEffectSettingsSpec(Id id) {
#define EFFECT_SETTINGS_SPEC(T) case T::ID: return T::SETTINGS;
    switch (id) { EFFECT_REGISTRY(EFFECT_SETTINGS_SPEC) default: return {180, 30, 40, EFFECT_PARAM_SPEED | EFFECT_PARAM_SCALE}; }
#undef EFFECT_SETTINGS_SPEC
  }

  const char* getEffectName(Id id) {
#define EFFECT_NAME(T) case T::ID: return T::NAME;
    switch (id) { EFFECT_REGISTRY(EFFECT_NAME) default: return ""; }
#undef EFFECT_NAME
  }

  Id getEffectId(const String& effect) {
#define EFFECT_COMPARE(T) if (effect.equals(T::NAME)) return T::ID;
    EFFECT_REGISTRY(EFFECT_COMPARE)
#undef EFFECT_COMPARE
      return Id::INVALID;
  }

  Id getEffectId(const char* effect) {
    if (effect == nullptr) return Id::INVALID;
#define EFFECT_COMPARE(T) if (strcmp(effect, T::NAME) == 0) return T::ID;
    EFFECT_REGISTRY(EFFECT_COMPARE)
#undef EFFECT_COMPARE
      return Id::INVALID;
  }

  Id fallback() {
    return isValid(DEFAULT_ID) ? DEFAULT_ID : DISPLAY_ORDER[0];
  }

  bool isValid(Id id) {
    if (toIndex(id) >= COUNT) return false;
    for (uint8_t i = 0; i < DISPLAY_COUNT; i++) {
      if (DISPLAY_ORDER[i] == id) return true;
    }
    return false;
  }

  bool isValid(uint8_t raw) {
    return raw < COUNT && isValid(toId(raw));
  }

  Id clamp(Id id) {
    return isValid(id) ? id : fallback();
  }

  Id clamp(uint8_t raw) {
    return isValid(raw) ? toId(raw) : fallback();
  }

}
