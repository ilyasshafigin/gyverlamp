#include "palette_catalog.h"
#include "palettes.h"
#include <string.h>

namespace Palettes {

  const char* getPaletteName(Id id) {
    switch (id) {
#define PALETTE_NAME(ID, NAME, PTR) \
  case ID: return NAME;
      PALETTE_REGISTRY_ALL(PALETTE_NAME)
#undef PALETTE_NAME
      default: return "Auto";
    }
  }

  Id parsePaletteName(const char* name) {
    if (!name) return Id::Auto;
#define PALETTE_COMPARE(ID, NAME, PTR) \
  if (strcmp(name, NAME) == 0) return ID;
    PALETTE_REGISTRY_ALL(PALETTE_COMPARE)
#undef PALETTE_COMPARE
    return Id::Auto;
  }

  const CRGBPalette16* getPalette(Id id) {
    if (id == Id::Auto) return nullptr;
    switch (id) {
#define PALETTE_PTR(ID, NAME, SRC)           \
  case ID: {                                 \
    static const CRGBPalette16 palette(SRC); \
    return &palette;                         \
  }
      PALETTE_REGISTRY(PALETTE_PTR)
#undef PALETTE_PTR
      default: return nullptr;
    }
  }

  const CRGBPalette16* getPaletteByScale(uint8_t scale) {
    uint8_t index = static_cast<uint8_t>(map(scale, 0U, 255U, 0U, SELECTABLE_COUNT - 1));
    return Palettes::getPalette(Palettes::getPaletteIdByIndex(index));
  }

  const CRGBPalette16* resolvePalette(Id selected, const CRGBPalette16* autoPalette) {
    if (selected == Id::Auto) return autoPalette;
    return getPalette(selected);
  }

} // namespace Palettes
