#include "butterflys.h"
#include "../shared.h"

// Effect: Butterflys - Светлячки 2 - Светлячки в банке - Мотыльки - Лампа с мотыльками
// Authors: SottNick
// Based on:
//   - Whilser/GyverLamp
//     https://github.com/Whilser/GyverLamp/blob/e7545c18f75911df6fc017d3e18c52cdd994cfc9/firmware/GyverLamp_v1.6_MQTT/butterflys.ino#L1
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L1874

#define BUTTERFLY_FIX_COUNT           (20U) // количество мотыльков для режима, конда бегунок Масштаб регулирует цвет

void EffectButterflys::setup(EffectContext& ctx) {
  randomSeed(ctx.nowMs);
  // для режима смены цвета фона фиксируем количество мотыльков
  deltaValue = BUTTERFLY_FIX_COUNT;

  for (uint8_t i = 0U; i < trackingObjectMaxCount; i++) {
    trackingObjectPosX[i] = random8(WIDTH);
    trackingObjectPosY[i] = random8(HEIGHT);
    trackingObjectSpeedX[i] = 0;
    trackingObjectSpeedY[i] = 0;
    trackingObjectShift[i] = 0;
    trackingObjectHue[i] = 255U;
    trackingObjectState[i] = 255U;
  }
  if (ctx.scale == 1U) {
    hue = random8();
  }
}

void EffectButterflys::render(EffectContext& ctx) {
  bool isWings = ctx.speed & 0x01;
  ctx.led.clearLeds();

  float speedFactor = static_cast<float>(ctx.speed) / 2048.0f + 0.001f;

  if (++step >= deltaValue) {
    step = 0U;
  }

  for (uint8_t i = 0U; i < deltaValue; i++) {
    trackingObjectPosX[i] += trackingObjectSpeedX[i] * speedFactor;
    trackingObjectPosY[i] += trackingObjectSpeedY[i] * speedFactor;

    if (trackingObjectPosX[i] < 0) trackingObjectPosX[i] = static_cast<float>(WIDTH - 1) + trackingObjectPosX[i];
    if (trackingObjectPosX[i] > WIDTH - 1) trackingObjectPosX[i] = trackingObjectPosX[i] + 1 - WIDTH;

    if (trackingObjectPosY[i] < 0) {
      trackingObjectPosY[i] = -trackingObjectPosY[i];
      trackingObjectSpeedY[i] = -trackingObjectSpeedY[i];
      //trackingObjectSpeedX[i] = -trackingObjectSpeedX[i];
    }
    if (trackingObjectPosY[i] > HEIGHT - 1U) {
      trackingObjectPosY[i] = (HEIGHT << 1U) - 2U - trackingObjectPosY[i];
      trackingObjectSpeedY[i] = -trackingObjectSpeedY[i];
      //trackingObjectSpeedX[i] = -trackingObjectSpeedX[i];
    }

    //проворот траектории
    float maxspeed = fabs(trackingObjectSpeedX[i]) + fabs(trackingObjectSpeedY[i]); // максимальная суммарная скорость
    if (maxspeed == fabs(trackingObjectSpeedX[i] + trackingObjectSpeedY[i])) {
      // правый верхний сектор вектора
      if (trackingObjectSpeedX[i] > 0) {
        trackingObjectSpeedX[i] += trackingObjectShift[i];
        // если вектор переехал вниз
        if (trackingObjectSpeedX[i] > maxspeed) {
          trackingObjectSpeedX[i] = maxspeed + maxspeed - trackingObjectSpeedX[i];
          trackingObjectSpeedY[i] = trackingObjectSpeedX[i] - maxspeed;
        } else
          trackingObjectSpeedY[i] = maxspeed - fabs(trackingObjectSpeedX[i]);
      }
      // левый нижний сектор
      else {
        trackingObjectSpeedX[i] -= trackingObjectShift[i];
        if (trackingObjectSpeedX[i] + maxspeed < 0) // если вектор переехал вверх
        {
          trackingObjectSpeedX[i] = 0 - trackingObjectSpeedX[i] - maxspeed - maxspeed;
          trackingObjectSpeedY[i] = maxspeed - fabs(trackingObjectSpeedX[i]);
        } else
          trackingObjectSpeedY[i] = fabs(trackingObjectSpeedX[i]) - maxspeed;
      }
    }
    // левый верхний и правый нижний секторы вектора
    else {
      // правый нижний сектор
      if (trackingObjectSpeedX[i] > 0) {
        trackingObjectSpeedX[i] -= trackingObjectShift[i];
        // если вектор переехал наверх
        if (trackingObjectSpeedX[i] > maxspeed) {
          trackingObjectSpeedX[i] = maxspeed + maxspeed - trackingObjectSpeedX[i];
          trackingObjectSpeedY[i] = maxspeed - trackingObjectSpeedX[i];
        } else {
          trackingObjectSpeedY[i] = fabs(trackingObjectSpeedX[i]) - maxspeed;
        }
      }
      // левый верхний сектор
      else {
        trackingObjectSpeedX[i] += trackingObjectShift[i];
        // если вектор переехал вниз
        if (trackingObjectSpeedX[i] + maxspeed < 0) {
          trackingObjectSpeedX[i] = 0 - trackingObjectSpeedX[i] - maxspeed - maxspeed;
          trackingObjectSpeedY[i] = 0 - trackingObjectSpeedX[i] - maxspeed;
        } else {
          trackingObjectSpeedY[i] = maxspeed - fabs(trackingObjectSpeedX[i]);
        }
      }
    }

    if (trackingObjectState[i] == 255U) {
      if (step == i && random8(2U) == 0U) {//(step == 0U && ((pcnt + i) & 0x01))
        trackingObjectState[i] = random8(220U, 244U);
        trackingObjectSpeedX[i] = static_cast<float>(random8(101U)) / 20.0f + 1.0f;
        if (random8(2U) == 0U) trackingObjectSpeedX[i] = -trackingObjectSpeedX[i];
        trackingObjectSpeedY[i] = static_cast<float>(random8(101U)) / 20.0f + 1.0f;
        if (random8(2U) == 0U) trackingObjectSpeedY[i] = -trackingObjectSpeedY[i];
        // проворот траектории
        //trackingObjectShift[i] = static_cast<float>(random8((fabs(trackingObjectSpeedX[i])+fabs(trackingObjectSpeedY[i]))*2.0+2.0)) / 40.0f;
        trackingObjectShift[i] = static_cast<float>(random8((fabs(trackingObjectSpeedX[i]) + fabs(trackingObjectSpeedY[i])) * 20.0f + 2.0f)) / 200.0f;
        if (random8(2U) == 0U) trackingObjectShift[i] = -trackingObjectShift[i];
      }
    } else {
      if (step == i) trackingObjectState[i]++;
      uint8_t tmp = 255U - trackingObjectState[i];
      if (tmp == 0U || (static_cast<uint16_t>(trackingObjectPosX[i] * tmp) % tmp == 0U && static_cast<uint16_t>(trackingObjectPosY[i] * tmp) % tmp == 0U)) {
        trackingObjectPosX[i] = round(trackingObjectPosX[i]);
        trackingObjectPosY[i] = round(trackingObjectPosY[i]);
        trackingObjectSpeedX[i] = 0;
        trackingObjectSpeedY[i] = 0;
        trackingObjectShift[i] = 0;
        trackingObjectState[i] = 255U;
      }
    }

    const uint8_t value = isWings
      ? ((trackingObjectState[i] == 255U) ? 255U : 128U + random8(2U) * 111U)
      : trackingObjectState[i];

    ctx.led.drawPixelSafe(trackingObjectPosX[i], trackingObjectPosY[i], CHSV(trackingObjectHue[i], 255U, value));
  }

  if (ctx.scale == 255U) { // вместо белого будет желтоватая лампа
    hue = 31U;
    hue2 = 170U;
  } else if (ctx.scale == 1U) {
    if (++deltaHue == 0U) hue++;
    hue2 = 255U;
  } else {
    hue = ctx.scale;
    hue2 = 255U;
  }

  for (uint16_t i = 0U; i < NUM_LEDS; i++) {
    const uint8_t value = 255U - ctx.led.getLed(i).r;
    if (ctx.palette) {
      ctx.led.setLed(i, ColorFromPalette(*ctx.palette, hue + ctx.scale, value));
    } else {
      ctx.led.setLed(i, CHSV(hue, hue2, value));
    }
  }
}
