#include "liquid_lamp.h"
#include "../shared.h"

// Effect: Liquid Lamp - Жидкая лампа - Лавовая лампа
// Authors: obliterator; palette generator by SottNick
// Based on:
//   - DmytroKorniienko/FireLamp_JeeUI
//     https://github.com/DmytroKorniienko/FireLamp_JeeUI/commit/9bad25adc2c917fbf3dfa97f4c498769aaf76ebe
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L6240

// Масштабируемые константы (вычисляются при инициализации)

// Вспомогательная константа для получения масштабируемого значения
static const float SCALE_FACTOR = min(WIDTH, HEIGHT) / 16.0f; // 16x16 — базовый размер

// Масса пузырей: масштабируется с площадью матрицы
static const unsigned MASS_MIN = max(5U, static_cast<unsigned>(10 * SCALE_FACTOR));
static const unsigned MASS_MAX = max(20U, static_cast<unsigned>(50 * SCALE_FACTOR));
// Радиус пузыря: 12.5%-18.75% от меньшей стороны матрицы
static const float BASE_RADIUS_MIN = 0.125f * min(WIDTH, HEIGHT);
static const float BASE_RADIUS_MAX = 0.1875f * min(WIDTH, HEIGHT);
// Сила возмущения: масштабируется с размером
static const float BASE_FORCE_MIN = 40.0f * SCALE_FACTOR;
static const float BASE_FORCE_MAX = 80.0f * SCALE_FACTOR;
// Радиус возмущения: 37.5%-62.5% от меньшей стороны
static const float BASE_DISTURB_MIN = 0.375f * min(WIDTH, HEIGHT);
static const float BASE_DISTURB_MAX = 0.625f * min(WIDTH, HEIGHT);
// Отступ от границ для отключения физики: ~18.75% от высоты
static const float BOUNDARY_MARGIN = max(2.0f, 0.1875f * HEIGHT);

float liquidLampHot[enlargedObjectMaxCount];
float liquidLampSpf[enlargedObjectMaxCount];
unsigned liquidLampMX[enlargedObjectMaxCount];
unsigned liquidLampSC[enlargedObjectMaxCount];
unsigned liquidLampTR[enlargedObjectMaxCount];

inline void LiquidLampPosition() {
  for (uint8_t i = 0; i < enlargedObjectNum; i++) {
    // Термический подъём: масштабируется с высотой матрицы
    liquidLampHot[i] += mapcurve(trackingObjectPosY[i], 0, HEIGHT - 1, 5, -5, InOutQuad) * speedfactor;

    float heat = (liquidLampHot[i] / trackingObjectState[i]) - 1;
    if (heat > 0 && trackingObjectPosY[i] < HEIGHT - 1) {
      trackingObjectSpeedY[i] += heat * liquidLampSpf[i];
    }

    // Гравитация: масштабируется с высотой для постоянной скорости падения
    if (trackingObjectPosY[i] > 0) {
      trackingObjectSpeedY[i] -= 0.07f * SCALE_FACTOR;
    }

    if (trackingObjectSpeedY[i]) trackingObjectSpeedY[i] *= 0.85;
    trackingObjectPosY[i] += trackingObjectSpeedY[i] * speedfactor;

    // Горизонтальное движение
    if (trackingObjectSpeedX[i]) trackingObjectSpeedX[i] *= 0.7;
    trackingObjectPosX[i] += trackingObjectSpeedX[i] * speedfactor;

    // Бесшовное зацикливание по X
    if (trackingObjectPosX[i] >= WIDTH) trackingObjectPosX[i] -= WIDTH;
    else if (trackingObjectPosX[i] < 0)
      trackingObjectPosX[i] += WIDTH;

    // Ограничение по Y
    if (trackingObjectPosY[i] > HEIGHT - 1) trackingObjectPosY[i] = HEIGHT - 1;
    if (trackingObjectPosY[i] < 0) trackingObjectPosY[i] = 0;
  }
}

void LiquidLampPhysic() {
  for (uint8_t i = 0; i < enlargedObjectNum; i++) {
    // Отключаем физику у границ
    if (trackingObjectPosY[i] < BOUNDARY_MARGIN || trackingObjectPosY[i] > HEIGHT - 1 - BOUNDARY_MARGIN) continue;

    for (uint8_t j = i + 1; j < enlargedObjectNum; j++) { // Оптимизация: j = i+1
      if (trackingObjectPosY[j] < BOUNDARY_MARGIN || trackingObjectPosY[j] > HEIGHT - 1 - BOUNDARY_MARGIN) continue;

      // Радиус взаимодействия масштабируется с размером матрицы
      float radius = (trackingObjectShift[i] + trackingObjectShift[j]) * 0.5f;

      // Быстрая проверка коллизий
      if (
        fabs(trackingObjectPosX[i] - trackingObjectPosX[j]) > radius * 2 ||
        fabs(trackingObjectPosY[i] - trackingObjectPosY[j]) > radius * 2
      )
        continue;

      // Бесшовное расстояние по X
      float dx = min(
        fabs(trackingObjectPosX[i] - trackingObjectPosX[j]), WIDTH - fabs(trackingObjectPosX[i] - trackingObjectPosX[j])
      );
      float dy = fabs(trackingObjectPosY[i] - trackingObjectPosY[j]);
      float dist = sqrt3(dx * dx + dy * dy);

      if (dist <= radius && dist > 0.01f) { // Защита от деления на ноль
        float nx = (trackingObjectPosX[j] - trackingObjectPosX[i]) / dist;
        float ny = (trackingObjectPosY[j] - trackingObjectPosY[i]) / dist;

        // Импульс с учётом массы
        float p = 2 *
                  (trackingObjectSpeedX[i] * nx + trackingObjectSpeedY[i] * ny - trackingObjectSpeedX[j] * nx -
                   trackingObjectSpeedY[j] * ny) /
                  (trackingObjectState[i] + trackingObjectState[j]);

        float pnx = p * nx, pny = p * ny;

        trackingObjectSpeedX[i] -= pnx * trackingObjectState[i];
        trackingObjectSpeedY[i] -= pny * trackingObjectState[i];
        trackingObjectSpeedX[j] += pnx * trackingObjectState[j];
        trackingObjectSpeedY[j] += pny * trackingObjectState[j];
      }
    }
  }
}

// генератор палитр для Жидкой лампы (c) SottNick
// HSV-точки градиента: { индекс, смещениеОттенка, насыщенность, яркость }.
static const uint8_t MBVioletColors_arr[5][4] PROGMEM = {
  {0, 0, 255, 255},     //  0, 255,   0,   0, // red
                        //{1  , 108, 161, 122}, //  1,  46, 123,  87, // seaBlue
  {1, 155, 209, 255},   //  1,  46, 124, 255, // сделал поярче цвет воды
  {80, 170, 255, 140},  // 80,   0,   0, 139, // DarkBlue
  {150, 213, 255, 128}, //150, 128,   0, 128, // purple
  {255, 0, 255, 255}    //255, 255,   0,   0  // red again
};

static const bool isColored = true;

void EffectLiquidLamp::setup(EffectContext& ctx) {
  if (isColored) {
    hue = ctx.scale;
    deltaHue = !(hue & 0x01);

    uint16_t objectCount =
      constrain((static_cast<uint16_t>(WIDTH) * static_cast<uint16_t>(HEIGHT)) / 10U, 2U, enlargedObjectMaxCount);
    enlargedObjectNum = static_cast<uint8_t>(objectCount);
  } else {
    hue = random8();
    deltaHue = random8(2U);
  }

  buildHsvGradientPalette(rgbPalette, MBVioletColors_arr, hue, 5U, deltaHue);

  // Инициализация пузырей с масштабируемыми параметрами
  for (uint8_t i = 0; i < enlargedObjectMaxCount; i++) {
    trackingObjectPosX[i] = random8(WIDTH);
    trackingObjectPosY[i] = 0;

    // Масса: в диапазоне MASS_MIN..MASS_MAX
    trackingObjectState[i] = random(MASS_MIN, MASS_MAX);

    // Скорость плавучести: обратно пропорциональна массе, с учётом масштаба
    liquidLampSpf[i] = fmap(trackingObjectState[i], MASS_MIN, MASS_MAX, 0.0015f / SCALE_FACTOR, 0.0005f / SCALE_FACTOR);

    // Радиус пузыря: в диапазоне BASE_RADIUS_MIN..BASE_RADIUS_MAX
    trackingObjectShift[i] = fmap(trackingObjectState[i], MASS_MIN, MASS_MAX, BASE_RADIUS_MIN, BASE_RADIUS_MAX);

    // Сила возмущения поля
    liquidLampMX[i] = fmap(trackingObjectState[i], MASS_MIN, MASS_MAX, BASE_FORCE_MIN, BASE_FORCE_MAX);

    // Радиус возмущения
    liquidLampSC[i] = fmap(trackingObjectState[i], MASS_MIN, MASS_MAX, BASE_DISTURB_MIN, BASE_DISTURB_MAX);

    // Порог оптимизации (2/3 от радиуса возмущения)
    liquidLampTR[i] = liquidLampSC[i] * 2.0f / 3.0f;
  }
}

void EffectLiquidLamp::render(EffectContext& ctx) {
  // Speedfactor: масштабируется с размером для постоянной визуальной скорости
  speedfactor = (ctx.speed / 64.0f + 0.1f) / SCALE_FACTOR; // Компенсация размера матрицы

  bool rebuildPalette = false;

  // Анимация палитры для монохромного режима
  if (isColored) {
    if (ctx.scale != hue) {
      hue = ctx.scale;
      deltaHue = !(hue & 0x01);
      rebuildPalette = true;
    }
  } else {
    enlargedObjectNum = map8(ctx.scale, 2U, enlargedObjectMaxCount);

    hue2++;
    if (hue2 % 0x10 == 0U) {
      hue++;
      rebuildPalette = true;
    }
  }

  if (rebuildPalette) {
    buildHsvGradientPalette(rgbPalette, MBVioletColors_arr, hue, 5U, deltaHue);
  }

  LiquidLampPosition();
  LiquidLampPhysic();

  // Рендеринг: расчёт влияния каждого пузыря на каждый пиксель
  for (uint8_t x = 0; x < WIDTH; x++) {
    for (uint8_t y = 0; y < HEIGHT; y++) {
      float sum = 0;

      for (uint8_t i = 0; i < enlargedObjectNum; i++) {
        // Быстрое отсечение: если пиксель далеко от пузыря — пропускаем
        if (fabs(x - trackingObjectPosX[i]) > liquidLampTR[i] || fabs(y - trackingObjectPosY[i]) > liquidLampTR[i])
          continue;

        // Бесшовное расстояние по X
        float dx = min(
          fabs(trackingObjectPosX[i] - static_cast<float>(x)),
          WIDTH - fabs(trackingObjectPosX[i] - static_cast<float>(x))
        );
        float dy = fabs(trackingObjectPosY[i] - static_cast<float>(y));
        float d = sqrt3(dx * dx + dy * dy);

        if (d < trackingObjectShift[i]) {
          // Внутри пузыря: яркость растёт к центру
          sum += mapcurve(d, 0, trackingObjectShift[i], 255, liquidLampMX[i], InQuad);
        } else if (d < liquidLampSC[i]) {
          // В зоне возмущения: яркость спадает к краям
          sum += mapcurve(d, trackingObjectShift[i], liquidLampSC[i], liquidLampMX[i], 0, OutQuart);
        }

        if (sum >= 255) {
          sum = 255;
          break;
        }
      }

      // Минимальная яркость для избежания артефактов палитры
      if (sum < 16) sum = 16;

      ctx.led.drawPixel(x, y, ColorFromPalette(rgbPalette, static_cast<uint8_t>(sum)));
    }
  }
}
