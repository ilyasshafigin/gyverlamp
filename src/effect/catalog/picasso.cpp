#include "picasso.h"
#include "../shared.h"

// Effect: Picasso
// Authors: unknown; possibly @obliterator
// Based on:
//   - DmytroKorniienko/FireLamp_JeeUI
//     https://github.com/DmytroKorniienko/FireLamp_JeeUI/blob/templ/src/effects.cpp
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L5436-L5455

//вместо класса Particle будем повторно использовать переменные из эффекта мячики и мотыльки
//        float position_x = 0;
//float trackingObjectPosX[enlargedOBJECT_MAX_COUNT];
//        float position_y = 0;
//float trackingObjectPosY[enlargedOBJECT_MAX_COUNT];
//        float speed_x = 0;
////float trackingObjectSpeedY[enlargedOBJECT_MAX_COUNT];                   // As time goes on the impact velocity will change, so make an array to store those values
//        float speed_y = 0;
////float trackingObjectShift[enlargedOBJECT_MAX_COUNT];                       // Coefficient of Restitution (bounce damping)
//        CHSV color;
////uint8_t trackingObjectHue[enlargedOBJECT_MAX_COUNT];
//        uint8_t hue_next = 0;
//uint8_t trackingObjectState[enlargedOBJECT_MAX_COUNT] ;                       // прикручено при адаптации для распределения мячиков по радиусу лампы
//        int8_t hue_step = 0;
//float   trackingObjectSpeedX[trackingOBJECT_MAX_COUNT];                       // The integer position of the dot on the strip (LED index)

void PicassoGenerate(bool reset) {
  for (uint8_t i = 0; i < enlargedObjectNum; i++) {
    if (reset) {
      trackingObjectState[i] = random8();
      trackingObjectSpeedX[i] = (trackingObjectState[i] - trackingObjectHue[i]) / 25;
    }
    if (trackingObjectState[i] != trackingObjectHue[i] && trackingObjectSpeedX[i]) {
      trackingObjectHue[i] += trackingObjectSpeedX[i];
    }
  }
}

void PicassoPosition() {
  for (uint8_t i = 0; i < enlargedObjectNum; i++) {
    if (
      trackingObjectPosX[i] + trackingObjectSpeedY[i] >= WIDTH || trackingObjectPosX[i] + trackingObjectSpeedY[i] < 0
    ) {
      trackingObjectSpeedY[i] = -trackingObjectSpeedY[i];
    }

    if (
      trackingObjectPosY[i] + trackingObjectShift[i] >= HEIGHT || trackingObjectPosY[i] + trackingObjectShift[i] < 0
    ) {
      trackingObjectShift[i] = -trackingObjectShift[i];
    }

    trackingObjectPosX[i] += trackingObjectSpeedY[i];
    trackingObjectPosY[i] += trackingObjectShift[i];
  }
}

void PicassoRoutine1(EffectContext& ctx) {
  PicassoGenerate(false);
  PicassoPosition();

  for (uint8_t i = 0; i < enlargedObjectNum - 2U; i += 2) {
    if (ctx.palette) {
      ctx.led.drawLineBuff(
        trackingObjectPosX[i],
        trackingObjectPosY[i],
        trackingObjectPosX[i + 1U],
        trackingObjectPosY[i + 1U],
        ColorFromPalette(*ctx.palette, trackingObjectHue[i])
      );
    } else {
      ctx.led.drawLineBuff(
        trackingObjectPosX[i],
        trackingObjectPosY[i],
        trackingObjectPosX[i + 1U],
        trackingObjectPosY[i + 1U],
        CHSV(trackingObjectHue[i], 255U, 255U)
      );
    }
  }

  if (everyMs(deltaValue, 20000U, ctx.nowMs)) {
    PicassoGenerate(true);
  }

  ctx.led.blurBuff(80);
}

void PicassoRoutine2(EffectContext& ctx) {
  PicassoGenerate(false);
  PicassoPosition();
  ctx.led.scaleBuff(180);

  for (uint8_t i = 0; i < enlargedObjectNum - 1U; i++) {
    if (ctx.palette) {
      ctx.led.drawLineBuff(
        trackingObjectPosX[i],
        trackingObjectPosY[i],
        trackingObjectPosX[i + 1U],
        trackingObjectPosY[i + 1U],
        ColorFromPalette(*ctx.palette, trackingObjectHue[i])
      );
    } else {
      ctx.led.drawLineBuff(
        trackingObjectPosX[i],
        trackingObjectPosY[i],
        trackingObjectPosX[i + 1U],
        trackingObjectPosY[i + 1U],
        CHSV(trackingObjectHue[i], 255U, 255U)
      );
    }
  }

  if (everyMs(deltaValue, 20000U, ctx.nowMs)) {
    PicassoGenerate(true);
  }

  ctx.led.blurBuff(80);
}

void PicassoRoutine3(EffectContext& ctx) {
  PicassoGenerate(false);
  PicassoPosition();
  ctx.led.scaleBuff(180);

  for (uint8_t i = 0; i < enlargedObjectNum - 2U; i += 2) {
    if (ctx.palette) {
      ctx.led.drawCircleBuff(
        fabs(trackingObjectPosX[i] - trackingObjectPosX[i + 1U]),
        fabs(trackingObjectPosY[i] - trackingObjectPosX[i + 1U]),
        fabs(trackingObjectPosX[i] - trackingObjectPosY[i]),
        ColorFromPalette(*ctx.palette, trackingObjectHue[i])
      );
    } else {
      ctx.led.drawCircleBuff(
        fabs(trackingObjectPosX[i] - trackingObjectPosX[i + 1U]),
        fabs(trackingObjectPosY[i] - trackingObjectPosX[i + 1U]),
        fabs(trackingObjectPosX[i] - trackingObjectPosY[i]),
        CHSV(trackingObjectHue[i], 255U, 255U)
      );
    }
  }

  if (everyMs(deltaValue, 20000U, ctx.nowMs)) {
    PicassoGenerate(true);
  }

  ctx.led.blurBuff(80);
}

void EffectPicasso::setup(EffectContext& ctx) {
  double minSpeed = 0.2, maxSpeed = 0.8;

  for (uint8_t i = 0; i < enlargedObjectMaxCount; i++) {
    trackingObjectPosX[i] = random8(WIDTH);
    trackingObjectPosY[i] = random8(HEIGHT);

    trackingObjectHue[i] = random8();

    trackingObjectSpeedY[i] = +((-maxSpeed / 3) + (maxSpeed * static_cast<float>(random8(1, 100)) / 100));
    trackingObjectSpeedY[i] += trackingObjectSpeedY[i] > 0 ? minSpeed : -minSpeed;

    trackingObjectShift[i] = +((-maxSpeed / 2) + (maxSpeed * static_cast<float>(random8(1, 100)) / 100));
    trackingObjectShift[i] += trackingObjectShift[i] > 0 ? minSpeed : -minSpeed;

    trackingObjectState[i] = trackingObjectHue[i];
  }

  ctx.led.clearLedsBuff();
  resetEveryMs(pcnt, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs);
  resetEveryMs(deltaValue, 20000U, ctx.nowMs);
}

void EffectPicasso::render(EffectContext& ctx) {
  if (ctx.scale < 85U) {
    enlargedObjectNum = static_cast<uint8_t>(map(ctx.scale, 0U, 85U, 3U, enlargedObjectMaxCount));
  } else if (ctx.scale >= 170U) {
    enlargedObjectNum = static_cast<uint8_t>(map(ctx.scale, 170U, 255U, 3U, enlargedObjectMaxCount));
  } else {
    enlargedObjectNum = static_cast<uint8_t>(map(ctx.scale, 86U, 169U, 3U, enlargedObjectMaxCount));
  }

  if (everyMs(pcnt, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs)) {
    if (ctx.scale < 85U) {
      PicassoRoutine1(ctx);
    } else if (ctx.scale > 170U) {
      PicassoRoutine3(ctx);
    } else {
      PicassoRoutine2(ctx);
    }
  }

  ctx.led.copyLedsBuffToLeds();
}
