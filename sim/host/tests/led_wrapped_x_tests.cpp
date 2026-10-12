#include "hardware/led.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

  constexpr CRGB Color(240, 120, 60);

  bool sameFrame(const Led& first, const Led& second) {
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        if (first.pixelColor(x, y) != second.pixelColor(x, y)) return false;
      }
    }
    return true;
  }

  uint32_t frameRed(const Led& led) {
    uint32_t sum = 0;
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x)
        sum += (led.pixelColor(x, y) >> 16) & 0xff;
    }
    return sum;
  }

  void testIntegerWrapAndClip() {
    Led led;
    led.drawPixelSafe(-1, 1, Color, true);
    assert(led.pixelColor(WIDTH - 1, 1) == led.pixelColor(Color));
    led.clearLeds();
    led.drawPixelSafe(-WIDTH * 3, 1, Color, true);
    assert(led.pixelColor(0, 1) == led.pixelColor(Color));
    led.clearLeds();
    led.drawPixelSafe(WIDTH * 4 + 2, 1, Color, true);
    assert(led.pixelColor(2, 1) == led.pixelColor(Color));
    led.clearLeds();
    led.drawPixelSafe(0, -1, Color, true);
    led.drawPixelSafe(0, HEIGHT, Color, true);
    assert(frameRed(led) == 0);

    Led bufferParity;
    bufferParity.drawPixelSafe(-WIDTH * 2 - 2, 1, Color, true);
    bufferParity.drawPixelSafeBuff(-WIDTH * 2 - 2, 1, Color, true);
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        assert(bufferParity.pixelColor(x, y) == bufferParity.pixelColorBuff(x, y));
      }
    }
  }

  void testFloatWrapAndParity() {
    Led seam, translated, buff;
    seam.drawPixelSafe(-0.25f, 1.25f, Color, true);
    translated.drawPixelSafe(static_cast<float>(WIDTH) - 0.25f, 1.25f, Color, true);
    assert(sameFrame(seam, translated));
    assert(seam.pixelColor(0, 1) != 0);
    assert(seam.pixelColor(WIDTH - 1, 1) != 0);

    buff.drawPixelSafeBuff(-static_cast<float>(WIDTH) * 3.0f - 0.25f, 1.25f, Color, true);
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        assert(buff.pixelColorBuff(x, y) == seam.pixelColor(x, y));
      }
    }

    Led positive, positiveTranslated;
    positive.drawPixelSafe(0.25f, 1.25f, Color, true);
    positiveTranslated.drawPixelSafe(static_cast<float>(WIDTH) + 0.25f, 1.25f, Color, true);
    assert(sameFrame(positive, positiveTranslated));
    assert(frameRed(positive) == frameRed(seam));

    Led saturated;
    saturated.led(saturated.pixelNumber(WIDTH - 1, 1)) = CRGB(250, 0, 0);
    saturated.drawPixelSafe(-0.25f, 1.0f, CRGB(255, 0, 0), true);
    assert(saturated.pixelColor(WIDTH - 1, 1) == 0xFF0000);
  }

  void testVerticalClippingAndInvalidCoords() {
    Led top, bottom, invalid;
    top.drawPixelSafe(1.5f, -0.25f, Color, true);
    bottom.drawPixelSafe(1.5f, static_cast<float>(HEIGHT) - 0.25f, Color, true);
    for (int x = 0; x < WIDTH; ++x) {
      assert(top.pixelColor(x, 0) != 0 || x != 1 && x != 2);
      assert(bottom.pixelColor(x, HEIGHT - 1) != 0 || x != 1 && x != 2);
    }
    for (int y = 1; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x)
        assert(top.pixelColor(x, y) == 0);
    }
    for (int y = 0; y < HEIGHT - 1; ++y) {
      for (int x = 0; x < WIDTH; ++x)
        assert(bottom.pixelColor(x, y) == 0);
    }

    invalid.drawPixelSafe(std::numeric_limits<float>::quiet_NaN(), 0.5f, Color, true);
    invalid.drawPixelSafe(0.5f, std::numeric_limits<float>::infinity(), Color, true);
    invalid.drawPixelSafe(0.5f, -std::numeric_limits<float>::infinity(), Color, true);
    assert(frameRed(invalid) == 0);
  }

  void testSafeRendererUnchanged() {
    Led led;
    led.drawPixelSafe(-1, 1, Color);
    assert(frameRed(led) == 0);
    led.drawPixelSafe(-0.25f, 1.25f, Color);
    assert(led.pixelColor(WIDTH - 1, 1) == 0);
    led.clearLeds();
    led.drawPixelSafe(-1.5f, 0.5f, Color);
    assert(frameRed(led) == 0);
    led.drawPixelSafe(WIDTH, 1, Color);
    assert(frameRed(led) == 0);
    led.drawPixelSafe(1.25f, 1.5f, Color);
    assert(led.pixelColor(1, 1) != 0);
    assert(led.pixelColor(2, 1) != 0);
    assert(led.pixelColor(1, 2) != 0);
    assert(led.pixelColor(2, 2) != 0);
  }

  void testWidthOneAccumulatesWeights() {
    if (WIDTH != 1) return;
    Led actual, reference;
    actual.drawPixelSafe(0.4f, 0.3f, Color, true);
    reference.drawPixelSafe(0.0f, 0.3f, Color, true);
    assert(actual.pixelColor(0, 0) > reference.pixelColor(0, 0));
    assert(actual.pixelColor(0, 1) > reference.pixelColor(0, 1));
  }

  void testCircleWrapDefaultsAndBufferParity() {
    Led defaultCircle, explicitlyWrapped;
    defaultCircle.drawCircle(0.25f, 1.0f, 1.0f, Color);
    explicitlyWrapped.drawCircle(0.25f, 1.0f, 1.0f, Color, true);

    Led clipped, clippedBuff, defaultClippedBuff;
    clipped.drawCircle(0.25f, 1.0f, 1.0f, Color, false);
    clippedBuff.drawCircleBuff(0.25f, 1.0f, 1.0f, Color, false);
    defaultClippedBuff.drawCircleBuff(0.25f, 1.0f, 1.0f, Color);
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        assert(clipped.pixelColor(x, y) == clippedBuff.pixelColorBuff(x, y));
      }
    }
    for (int y = 0; y < HEIGHT; ++y) {
      for (int x = 0; x < WIDTH; ++x) {
        assert(clipped.pixelColor(x, y) == defaultClippedBuff.pixelColorBuff(x, y));
      }
    }
    assert(clipped.pixelColor(WIDTH - 1, 1) == 0);
    assert(explicitlyWrapped.pixelColor(WIDTH - 1, 1) != 0);

    assert(sameFrame(defaultCircle, clipped));
  }

} // namespace

int main() {
  testIntegerWrapAndClip();
  testFloatWrapAndParity();
  testVerticalClippingAndInvalidCoords();
  testSafeRendererUnchanged();
  testWidthOneAccumulatesWeights();
  testCircleWrapDefaultsAndBufferParity();
}
