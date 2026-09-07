#pragma once

#include <FastLED.h>

#include "../config.h"

// **************** НАСТРОЙКА МАТРИЦЫ ****************
#if (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 0)
#define ORIENTATION 0
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y

#elif (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 1)
#define ORIENTATION 1
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y x

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 0)
#define ORIENTATION 2
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 3)
#define ORIENTATION 3
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y x

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 2)
#define ORIENTATION 4
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 3)
#define ORIENTATION 5
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y (WIDTH - x - 1)

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 2)
#define ORIENTATION 6
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y y

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 1)
#define ORIENTATION 7
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y (WIDTH - x - 1)

#else
#define ORIENTATION 8
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y
#pragma message "Wrong matrix parameters! Set to default"

#endif

class Led {
public:
  Led();

  void init();

  CRGB* leds() { return leds_; }
  CRGB* ledsBuff() { return ledsbuff_; }
  const CRGB* leds() const { return leds_; }
  const CRGB* ledsBuff() const { return ledsbuff_; }

  CRGB& led(size_t index) { return leds_[index]; }
  CRGB& ledBuff(size_t index) { return ledsbuff_[index]; }
  const CRGB& led(size_t index) const { return leds_[index]; }
  const CRGB& ledBuff(size_t index) const { return ledsbuff_[index]; }

  void setLed(size_t index, const CRGB& color) { leds_[index] = color; }
  void setLedBuff(size_t index, const CRGB& color) { ledsbuff_[index] = color; }

  // Очищает все светодиоды. Чтобы увидеть, нужно вызвать showLeds()
  void clearLeds();
  // Отправляет все что отрисовано на светодиоды с указанной яркостью
  void showLeds(uint8_t brightness);
  // Очищает буфер и сразу отправляет черный кадр на ленту
  void blackout();

  // Работа с внутренним буфером. Он не связан со светодиодами напрямую, нужно вручную записывать в leds
  void clearLedsBuff();
  void copyLedsBuffToLeds();

  uint32_t pixelColor(int x, int y) const {
    if (!isValidXY(x, y)) return 0;
    return packColor(leds_[pixelNumber(x, y)]);
  }
  uint32_t pixelColorBuff(int x, int y) const {
    if (!isValidXY(x, y)) return 0;
    return packColor(ledsbuff_[pixelNumber(x, y)]);
  }
  uint32_t pixelColor(uint16_t pixelNum) const {
    if (pixelNum >= NUM_LEDS) return 0;
    return packColor(leds_[pixelNum]);
  }
  uint32_t pixelColorBuff(uint16_t pixelNum) const {
    if (pixelNum >= NUM_LEDS) return 0;
    return packColor(ledsbuff_[pixelNum]);
  }
  uint32_t pixelColor(const CRGB& pixel) const { return packColor(pixel); }

  uint16_t pixelNumber(int x, int y) const {
    if (THIS_Y % 2 == 0) {
      return (THIS_Y * _WIDTH + THIS_X);
    }
    return (THIS_Y * _WIDTH + _WIDTH - THIS_X - 1);
  }

  CRGB& pixelSafe(int x, int y) {
    if (!isValidXY(x, y)) return dummyLed_;
    return leds_[pixelNumber(x, y)];
  }
  const CRGB& pixelSafe(int x, int y) const {
    if (!isValidXY(x, y)) return dummyLed_;
    return leds_[pixelNumber(x, y)];
  }

  CRGB& pixel(uint8_t x, uint8_t y) { return leds_[pixelNumber(x, y)]; }
  const CRGB& pixel(uint8_t x, uint8_t y) const { return leds_[pixelNumber(x, y)]; }
  CRGB& pixelBuff(uint8_t x, uint8_t y) { return ledsbuff_[pixelNumber(x, y)]; }
  const CRGB& pixelBuff(uint8_t x, uint8_t y) const { return ledsbuff_[pixelNumber(x, y)]; }

  void drawPixel(uint8_t x, uint8_t y, const CRGB& color) { leds_[pixelNumber(x, y)] = color; }
  void drawPixelBuff(uint8_t x, uint8_t y, const CRGB& color) { ledsbuff_[pixelNumber(x, y)] = color; }
  void drawPixelSafe(int x, int y, const CRGB& color) { drawPixelSafe(leds_, x, y, color); }
  void drawPixelSafeBuff(int x, int y, const CRGB& color) { drawPixelSafe(ledsbuff_, x, y, color); }
  void drawPixelSafe(float x, float y, const CRGB& color) { drawPixelSafe(leds_, x, y, color); }
  void drawPixelSafeBuff(float x, float y, const CRGB& color) { drawPixelSafe(ledsbuff_, x, y, color); }

  // Плавно гасит цвета для всех пикселей
  void fadePixelToBlack(uint8_t x, uint8_t y, uint8_t step);
  void fadeBuffPixelToBlack(uint8_t x, uint8_t y, uint8_t step);

  // Плавно гасит цвет пикселя
  void fadeToBlack(uint8_t step);
  void fadeBuffToBlack(uint8_t step);

  // Масштабирует яркость коэффициентом все пиксели
  void scale(uint8_t value);
  void scaleBuff(uint8_t value);

  // Заполняет все пиксели заданным цветом
  void fill(const CRGB& color);
  void fillBuff(const CRGB& color);

  // Добавляет цвет ко всем цветам
  void add(const CRGB& color);
  void addBuff(const CRGB& color);

  // Добавляет цвет к пикселю
  void addPixel(uint8_t x, uint8_t y, const CRGB& color);

  // Размытие
  void blur(fract8 amount);
  void blurBuff(fract8 amount);

  void gradientDownTop(uint8_t bottom, const CHSV& bottomColor, uint8_t top, const CHSV& topColor);
  void drawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, const CRGB& color) {
    drawLine(leds_, x1, y1, x2, y2, color);
  }
  void drawLineBuff(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, const CRGB& color) {
    drawLine(ledsbuff_, x1, y1, x2, y2, color);
  }
  void drawLine(float x1, float y1, float x2, float y2, const CRGB& color) { drawLine(leds_, x1, y1, x2, y2, color); }
  void drawLineBuff(float x1, float y1, float x2, float y2, const CRGB& color) {
    drawLine(ledsbuff_, x1, y1, x2, y2, color);
  }
  void drawCircle(float x0, float y0, float radius, const CRGB& color) { drawCircle(leds_, x0, y0, radius, color); }
  void drawCircleBuff(float x0, float y0, float radius, const CRGB& color) {
    drawCircle(ledsbuff_, x0, y0, radius, color);
  }

  const fl::XYMap& xyMap() const { return xyMap_; }

private:
  CRGB leds_[NUM_LEDS];
  CRGB ledsbuff_[NUM_LEDS];
  CRGB dummyLed_ = CRGB::Black;
  fl::XYMap xyMap_;

  static bool isValidXY(int x, int y) { return static_cast<unsigned>(x) < WIDTH && static_cast<unsigned>(y) < HEIGHT; }

  static uint32_t packColor(const CRGB& pixel) {
    return (static_cast<uint32_t>(pixel.r) << 16) | (static_cast<uint32_t>(pixel.g) << 8) |
           static_cast<uint32_t>(pixel.b);
  }

  uint32_t pixelColor(CRGB* buff, int x, int y) const {
    if (!isValidXY(x, y)) return 0;
    return packColor(buff[pixelNumber(x, y)]);
  }

  void drawPixelSafe(CRGB* buff, int x, int y, const CRGB& color) {
    if (!isValidXY(x, y)) return;
    buff[pixelNumber(x, y)] = color;
  }
  void drawPixelSafe(CRGB* buff, float x, float y, const CRGB& color);

  void drawLine(CRGB* buff, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, const CRGB& color);
  void drawLine(CRGB* buff, float x1, float y1, float x2, float y2, const CRGB& color);
  void drawCircle(CRGB* buff, float x0, float y0, float radius, const CRGB& color);
};
