#pragma once

#include <cstring>
#include <FastLED.h>
#include "math.h"
#include "palette_catalog.h"
#include "palette_utils.h"
#include "../config.h"

#define MAX_DIMENSION (max(WIDTH, HEIGHT))

#define CENTER_X_MINOR        (static_cast<uint8_t>((WIDTH / 2U) - ((WIDTH - 1U) & 0x01U)))
#define CENTER_Y_MINOR        (static_cast<uint8_t>((HEIGHT / 2U) - ((HEIGHT - 1U) & 0x01U)))
#define CENTER_X_MAJOR        (static_cast<uint8_t>(WIDTH / 2U + (WIDTH % 2U)))
#define CENTER_Y_MAJOR        (static_cast<uint8_t>(HEIGHT / 2U + (HEIGHT % 2U)))
#define HALF_HEIGHT           (static_cast<uint8_t>((HEIGHT + 1) / 2))

constexpr uint8_t NUM_LAYERSMAX = 2;

// несколько общих переменных и буферов, которые могут использоваться в любом эффекте

// постепенный сдвиг оттенка или какой-нибудь другой цикличный счётчик
extern uint8_t hue, hue2;
// ещё пара таких же, когда нужно много
extern uint8_t deltaHue, deltaHue2;
// какой-нибудь счётчик кадров или последовательностей операций
extern uint8_t step;
// какой-то счётчик какого-то прогресса
extern uint8_t pcnt;
// просто повторно используемая переменная
extern uint8_t deltaValue;
// регулятор скорости в эффектах реального времени
extern float speedfactor;
// какие-то динамичные координаты
extern float emitterX, emitterY;
// большие счётчики
extern uint16_t ff_x, ff_y, ff_z;

// двухслойная маска или хранилище свойств в размер всей матрицы
extern uint8_t noise3d[NUM_LAYERSMAX][WIDTH][HEIGHT];
// свойство пикселей в размер строки матрицы
extern uint8_t line[WIDTH];
// свойство пикселей в размер столбца матрицы
extern uint8_t shiftHue[HEIGHT];
// свойство пикселей в размер столбца матрицы ещё одно
extern uint8_t shiftValue[HEIGHT];

//массивы состояния объектов, которые могут использоваться в любом эффекте

// максимальное количество отслеживаемых объектов (очень влияет на расход памяти)
constexpr uint8_t trackingObjectMaxCount = 100U;
extern float   trackingObjectPosX[trackingObjectMaxCount];
extern float   trackingObjectPosY[trackingObjectMaxCount];
extern float   trackingObjectSpeedX[trackingObjectMaxCount];
extern float   trackingObjectSpeedY[trackingObjectMaxCount];
extern float   trackingObjectShift[trackingObjectMaxCount];
extern uint8_t trackingObjectHue[trackingObjectMaxCount];
extern uint8_t trackingObjectState[trackingObjectMaxCount];
extern bool    trackingObjectIsShift[trackingObjectMaxCount];
// максимальное количество сложных отслеживаемых объектов (меньше, чем trackingObjectMaxCount)
constexpr uint8_t enlargedObjectMaxCount = WIDTH * 2;
// используемое в эффекте количество объектов
extern uint8_t enlargedObjectNum;

//
extern CRGBPalette16 rgbPalette; // aka cPalette

// Лёгкий аналог EVERY_N_MILLIS.
// Проверяет переход на новый шаг intervalMs. Состояние хранится в uint8_t.
// При паузе дольше 256 * intervalMs одно срабатывание может быть пропущено.
bool everyMs(uint8_t& latch, uint16_t intervalMs, uint32_t nowMs = millis());

// Сбрасывает шаг на текущий момент, чтобы не было немедленного срабатывания.
void resetEveryMs(uint8_t& latch, uint16_t intervalMs, uint32_t nowMs = millis());

// Масштабирует скорость (1-255) в интервал
static inline uint16_t speedToIntervalMs(
  uint8_t speed,
  uint16_t slowMs,
  uint16_t fastMs
) {
  return map(speed, 1U, 255U, slowMs, fastMs);
}
