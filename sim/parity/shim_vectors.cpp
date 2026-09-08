#include "FastLED.h"

#include "vector_main.h"

uint32_t sim_millis = 0;

namespace fastled_parity {
using XYFunction = uint16_t (*)(uint16_t, uint16_t, uint16_t, uint16_t);
void fastled_parity_set_millis(uint32_t value) { sim_millis = value; }
uint16_t fastled_parity_map_to_index(uint8_t width, uint8_t height, XYFunction function, uint16_t x, uint16_t y) {
  const fl::XYMap map = fl::XYMap::constructWithUserFunction(width, height, function);
  return map(x, y);
}
void fastled_parity_blur1d(CRGB* leds, uint16_t count, uint8_t amount) { blur1d(leds, count, amount); }
void fastled_parity_blur2d(CRGB* leds, uint8_t width, uint8_t height, uint8_t amount, XYFunction function) {
  const fl::XYMap map = fl::XYMap::constructWithUserFunction(width, height, function);
  blur2d(leds, width, height, amount, map);
}
} // namespace fastled_parity

int main() {
  return fastled_parity::run();
}
