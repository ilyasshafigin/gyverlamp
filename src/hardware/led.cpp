#include "led.h"
#include "../config.h"

static uint16_t ledXYFunction(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  (void)width;
  (void)height;
  // логика из getPixelNumber: THIS_X, THIS_Y, _WIDTH
  if (THIS_Y % 2 == 0) {
    return THIS_Y * _WIDTH + THIS_X;
  } else {
    return THIS_Y * _WIDTH + _WIDTH - THIS_X - 1;
  }
}

Led::Led()
  : _xyMap(fl::XYMap::constructWithUserFunction(WIDTH, HEIGHT, ledXYFunction)) {
}

void Led::init() {
  fill_solid(_leds, NUM_LEDS, CRGB::Black);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(_leds, NUM_LEDS);
  FastLED.setBrightness(255);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.show(0);
}

//void Led::clearLeds() { fill_solid(_leds, NUM_LEDS, CRGB::Black); } - не работает
void Led::clearLeds() {
  FastLED.clear();
}
void Led::showLeds(uint8_t brightness) {
  FastLED.show(brightness);
}
void Led::blackout() {
  FastLED.clear(true);
}

void Led::clearLedsBuff() {
  fill_solid(_ledsbuff, NUM_LEDS, CRGB::Black);
}

void Led::copyLedsBuffToLeds() {
  fl::memcpy(_leds, _ledsbuff, sizeof(_leds));
}

// по мотивам
// https://gist.github.com/sutaburosu/32a203c2efa2bb584f4b846a91066583
void Led::drawPixelSafe(CRGB* buff, float x, float y, const CRGB& color) {
  float xt = x + 1, yt = y + 1;
  if (xt < 0 || yt < 0) return; //не похоже, чтобы отрицательные значения хоть как-нибудь учитывались тут
  // extract the fractional parts and derive their inverses
  uint8_t xx = (xt - static_cast<int>(xt)) * 255, yy = (yt - static_cast<int>(yt)) * 255, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
#define WU_WEIGHT(a, b) ((uint8_t)(((a) * (b) + (a) + (b)) >> 8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy), WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    int16_t xn = x + (i & 1), yn = y + ((i >> 1) & 1);
    CRGB clr = getPixelColor(buff, xn, yn);
    clr.r = qadd8(clr.r, (color.r * wu[i]) >> 8);
    clr.g = qadd8(clr.g, (color.g * wu[i]) >> 8);
    clr.b = qadd8(clr.b, (color.b * wu[i]) >> 8);
    drawPixelSafe(buff, xn, yn, clr);
  }
}

void Led::fadeToBlack(uint8_t step) {
  fadeToBlackBy(_leds, NUM_LEDS, step);
}

void Led::fadeBuffToBlack(uint8_t step) {
  fadeToBlackBy(_ledsbuff, NUM_LEDS, step);
}

void Led::fadePixelToBlack(uint8_t x, uint8_t y, uint8_t step) {
  if (!isValidXY(x, y)) return;
  CRGB& pixel = _leds[getPixelNumber(x, y)];
  if (pixel == CRGB::Black) return;
  if (pixel.r >= 30 || pixel.g >= 30 || pixel.b >= 30) {
    pixel.fadeToBlackBy(step);
  } else {
    pixel = CRGB::Black;
  }
}

void Led::fadeBuffPixelToBlack(uint8_t x, uint8_t y, uint8_t step) {
  if (!isValidXY(x, y)) return;
  CRGB& pixel = _ledsbuff[getPixelNumber(x, y)];
  if (pixel == CRGB::Black) return;
  if (pixel.r >= 30 || pixel.g >= 30 || pixel.b >= 30) {
    pixel.fadeToBlackBy(step);
  } else {
    pixel = CRGB::Black;
  }
}

void Led::scale(uint8_t value) {
  nscale8(_leds, NUM_LEDS, value);
}

void Led::scaleBuff(uint8_t value) {
  nscale8(_ledsbuff, NUM_LEDS, value);
}

void Led::fill(const CRGB& color) {
  fill_solid(_leds, NUM_LEDS, color);
}

void Led::fillBuff(const CRGB& color) {
  fill_solid(_ledsbuff, NUM_LEDS, color);
}

void Led::add(const CRGB& color) {
  for (uint16_t i = 0U; i < NUM_LEDS; i++) {
    _leds[i] += color;
  }
}

void Led::addBuff(const CRGB& color) {
  for (uint16_t i = 0U; i < NUM_LEDS; i++) {
    _ledsbuff[i] += color;
  }
}

void Led::addPixel(uint8_t x, uint8_t y, const CRGB& color) {
  if (!isValidXY(x, y)) return;
  CRGB& pixel = _leds[getPixelNumber(x, y)];
  pixel += color;
}

void Led::blur(fract8 amount) {
  blur2d(_leds, WIDTH, HEIGHT, amount, _xyMap);
}

void Led::blurBuff(fract8 amount) {
  blur2d(_ledsbuff, WIDTH, HEIGHT, amount, _xyMap);
}

void Led::gradientDownTop(uint8_t bottom, const CHSV& bottomColor, uint8_t top, const CHSV& topColor) {
  //  FORWARD_HUES: hue always goes clockwise
  //  BACKWARD_HUES: hue always goes counter-clockwise
  //  SHORTEST_HUES: hue goes whichever way is shortest
  //  LONGEST_HUES: hue goes whichever way is longest
  if (ORIENTATION < 3 || ORIENTATION == 7) {
    // STRIP_DIRECTION to UP ========
    fill_gradient(_leds, top * WIDTH, topColor, bottom * WIDTH, bottomColor, SHORTEST_HUES);
  } else {
    // STRIP_DIRECTION to DOWN ======
    fill_gradient(_leds, NUM_LEDS - bottom * WIDTH - 1, bottomColor, NUM_LEDS - top * WIDTH, topColor, SHORTEST_HUES);
  }
}

void Led::drawLine(CRGB* buff, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, const CRGB& color) {
  int deltaX = abs(x2 - x1);
  int deltaY = abs(y2 - y1);
  int signX = x1 < x2 ? 1 : -1;
  int signY = y1 < y2 ? 1 : -1;
  int error = deltaX - deltaY;

  drawPixelSafe(buff, x2, y2, color);
  while (x1 != x2 || y1 != y2) {
    drawPixelSafe(buff, x1, y1, color);
    int error2 = error * 2;
    if (error2 > -deltaY) {
      error -= deltaY;
      x1 += signX;
    }
    if (error2 < deltaX) {
      error += deltaX;
      y1 += signY;
    }
  }
}

void Led::drawLine(CRGB* buff, float x1, float y1, float x2, float y2, const CRGB& color) {
  float deltaX = fabs(x2 - x1);
  float deltaY = fabs(y2 - y1);
  float error = deltaX - deltaY;

  float signX = x1 < x2 ? 0.5 : -0.5;
  float signY = y1 < y2 ? 0.5 : -0.5;

  while (x1 != x2 || y1 != y2) {
    if ((signX > 0 && x1 > x2 + signX) || (signX < 0 && x1 < x2 + signX)) break;
    if ((signY > 0 && y1 > y2 + signY) || (signY < 0 && y1 < y2 + signY)) break;
    drawPixelSafe(buff, x1, y1, color);
    float error2 = error;
    if (error2 > -deltaY) {
      error -= deltaY;
      x1 += signX;
    }
    if (error2 < deltaX) {
      error += deltaX;
      y1 += signY;
    }
  }
}

void Led::drawCircle(CRGB* buff, float x0, float y0, float radius, const CRGB& color) {
  float x = 0, y = radius, error = 0;
  float delta = 1.0 - 2.0 * radius;

  while (y >= 0) {
    drawPixelSafe(buff, fmod(x0 + x + WIDTH, WIDTH), y0 + y, color);
    drawPixelSafe(buff, fmod(x0 + x + WIDTH, WIDTH), y0 - y, color);
    drawPixelSafe(buff, fmod(x0 - x + WIDTH, WIDTH), y0 + y, color);
    drawPixelSafe(buff, fmod(x0 - x + WIDTH, WIDTH), y0 - y, color);
    error = 2.0 * (delta + y) - 1.0;
    if (delta < 0 && error <= 0) {
      ++x;
      delta += 2.0 * x + 1.0;
      continue;
    }
    error = 2.0 * (delta - x) - 1.0;
    if (delta > 0 && error > 0) {
      --y;
      delta += 1.0 - 2.0 * y;
      continue;
    }
    ++x;
    delta += 2.0 * (x - y);
    --y;
  }
}
