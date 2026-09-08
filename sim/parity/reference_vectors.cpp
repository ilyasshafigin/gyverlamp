#include <FastLED.h>

// The reference executable deliberately uses host headers. These checks lock
// its claimed shared FastLED implementation branches: platforms/math8.h routes
// every non-AVR target to platforms/shared/math8.h, and platforms/trig8.h routes
// every target without USE_SIN_32 or __AVR__ to platforms/shared/trig8.h.
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
#error "Host reference must not claim ESP8266 platform headers"
#endif

#if SKETCH_HAS_LARGE_MEMORY != 0
#error "Reference requires SKETCH_HAS_LARGE_MEMORY=0"
#endif

#if !defined(SCALE8_C) || SCALE8_C != 1
#error "Reference requires FastLED shared scale8 C branch"
#endif

#if defined(__AVR__) || defined(USE_SIN_32)
#error "Reference requires FastLED shared trig8 branch"
#endif

#if FASTLED_SCALE8_FIXED != 1 || FASTLED_BLEND_FIXED != 1
#error "Reference requires fixed FastLED scale and blend configuration"
#endif

#if FASTLED_NOISE_FIXED != 1 || FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW != 0
#error "Reference requires fixed non-overflow FastLED noise configuration"
#endif

uint32_t fastled_parity_millis = 0;

namespace fl {
u32 millis() FL_NO_EXCEPT { return fastled_parity_millis; }
} // namespace fl

namespace fastled_parity {
void fastled_parity_set_millis(uint32_t value) { fastled_parity_millis = value; }
} // namespace fastled_parity

namespace fl {
void* memmove(void* destination, const void* source, size_t size) {
  return __builtin_memmove(destination, source, size);
}
} // namespace fl

CRGB& CRGB::nscale8(fl::u8 scaledown) {
  // Exact body from installed FastLED src/crgb.cpp.hpp.  Defining only this
  // dependency keeps the reference producer linkable without its unrelated
  // host-incompatible CRGB geometry and string implementations.
  nscale8x3(r, g, b, scaledown);
  return *this;
}

CRGB& CRGB::operator+=(const CRGB& rhs) {
  r = qadd8(r, rhs.r);
  g = qadd8(g, rhs.g);
  b = qadd8(b, rhs.b);
  return *this;
}

#include "hsv2rgb.cpp.hpp"
#include "fl/gfx/colorutils.cpp.hpp"
#include "fl/stl/shared_ptr.cpp.hpp"
#include "fl/math/xymap.cpp.hpp"
#include "fl/gfx/blur.cpp.hpp"
#include "noise.cpp.hpp"

namespace fastled_parity {
using XYFunction = uint16_t (*)(uint16_t, uint16_t, uint16_t, uint16_t);

uint16_t fastled_parity_map_to_index(uint8_t width, uint8_t height, XYFunction function, uint16_t x, uint16_t y) {
  const fl::XYMap map = fl::XYMap::constructWithUserFunction(width, height, function);
  return map.mapToIndex(x, y);
}
void fastled_parity_blur1d(CRGB* leds, uint16_t count, uint8_t amount) { fl::blur1d(leds, count, amount); }
void fastled_parity_blur2d(CRGB* leds, uint8_t width, uint8_t height, uint8_t amount, XYFunction function) {
  const fl::XYMap map = fl::XYMap::constructWithUserFunction(width, height, function);
  fl::blur2d(leds, width, height, amount, map);
}
} // namespace fastled_parity

#include "vector_main.h"

int main() {
  return fastled_parity::run();
}
