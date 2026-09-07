#pragma once

#include <FastLED.h>
#include "palette_ids.h"

#define PALETTE_REGISTRY(X)                                      \
  X(Palettes::Id::Rainbow, "Rainbow", RainbowColors_p)           \
  X(Palettes::Id::Party, "Party", PartyColors_p)                 \
  X(Palettes::Id::Ocean, "Ocean", OceanColors_p)                 \
  X(Palettes::Id::Lava, "Lava", LavaColors_p)                    \
  X(Palettes::Id::Heat, "Heat", HeatColors_p)                    \
  X(Palettes::Id::Forest, "Forest", ForestColors_p)              \
  X(Palettes::Id::Clouds, "Clouds", CloudColors_p)               \
  X(Palettes::Id::Sunset, "Sunset", Sunset_gp)                   \
  X(Palettes::Id::OptimusPrime, "OptimusPrime", OptimusPrime_gp) \
  X(Palettes::Id::Warm, "Warm", Warm_gp)                         \
  X(Palettes::Id::Cold, "Cold", Cold_gp)                         \
  X(Palettes::Id::Hot, "Hot", Hot_gp)                            \
  X(Palettes::Id::Pink, "Pink", Pink_gp)                         \
  X(Palettes::Id::Comfy, "Comfy", Comfy_gp)                      \
  X(Palettes::Id::Cyberpank, "Cyberpank", Cyperpunk_gp)          \
  X(Palettes::Id::Xmas, "Xmas", Xmas_gp)                         \
  X(Palettes::Id::Acid, "Acid", Acid_gp)                         \
  X(Palettes::Id::Gummy, "Gummy", Gummy_gp)

#define PALETTE_REGISTRY_AUTO(X) X(Palettes::Id::Auto, "Auto", nullptr)

#define PALETTE_REGISTRY_ALL(X) \
  PALETTE_REGISTRY_AUTO(X)      \
  PALETTE_REGISTRY(X)

namespace Palettes {

  constexpr Id kSelectableOrder[] = {
#define PALETTE_SELECTABLE_ID(ID, NAME, PTR) ID,
    PALETTE_REGISTRY(PALETTE_SELECTABLE_ID)
#undef PALETTE_SELECTABLE_ID
  };

  constexpr Id kAutoOrder[] = {
#define PALETTE_AUTO_ID(ID, NAME, PTR) ID,
    PALETTE_REGISTRY_AUTO(PALETTE_AUTO_ID)
#undef PALETTE_AUTO_ID
  };

  constexpr uint8_t kSelectableCount = sizeof(kSelectableOrder) / sizeof(kSelectableOrder[0]);
  constexpr uint8_t kAutoCount = sizeof(kAutoOrder) / sizeof(kAutoOrder[0]);
  constexpr uint8_t kCount = kSelectableCount + kAutoCount;

  constexpr bool isValid(uint8_t raw) {
    return raw < kCount;
  }
  constexpr Id clamp(uint8_t raw) {
    return isValid(raw) ? static_cast<Id>(raw) : Id::Auto;
  }
  constexpr Id clamp(Id id) {
    return clamp(toIndex(id));
  }

  inline Id paletteIdByIndex(uint8_t index) {
    return index >= kSelectableCount ? Id::Auto : kSelectableOrder[index];
  }

  const char* paletteName(Id id);
  Id parsePaletteName(const char* name);

  const CRGBPalette16* palette(Id id);
  const CRGBPalette16* paletteByScale(uint8_t scale);
  const CRGBPalette16* resolvePalette(Id selected, const CRGBPalette16* autoPalette);

} // namespace Palettes
