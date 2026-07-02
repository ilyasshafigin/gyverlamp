#pragma once

#include <FastLED.h>

// Указатель на таблицу HSV-точек градиента.
//
// Каждая строка таблицы содержит 4 байта:
//   { индекс, смещениеОттенка, насыщенность, яркость }
//
// Таблица обычно хранится в PROGMEM.
// Это не готовая FastLED-палитра, а компактное описание,
// из которого buildShiftedHsvGradientPalette() собирает CRGBPalette16.
using HsvGradientStopTable = const uint8_t(*)[4];

// Собирает 16-цветную RGB-палитру FastLED из компактных HSV-точек градиента.
//
// Каждая строка stops:
//   { индекс, смещениеОттенка, насыщенность, яркость }
//
// Функция:
//   - сдвигает оттенок каждой точки на baseHue;
//   - если invertHue == true, отражает направление оттенков;
//   - конвертирует HSV-точки в RGB через FastLED hsv2rgb_spectrum();
//   - заполняет CRGBPalette16 RGB-градиентами между точками.
//
// Аргументы:
//   out       - RGB-палитра FastLED, куда записать результат.
//   stops     - компактные HSV-точки градиента из PROGMEM.
//   baseHue   - общий сдвиг оттенка для всех точек.
//   stopCount - количество строк в stops.
//   invertHue - false: baseHue + смещениеОттенка;
//               true:  baseHue - смещениеОттенка.
void buildHsvGradientPalette( // aka fillMyPal16, fillMyPal16_2
  CRGBPalette16& out,
  HsvGradientStopTable palette,
  uint8_t baseHue,
  uint8_t stopCount,
  bool invertHue = false
);
