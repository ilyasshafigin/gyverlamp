#include "nexus.h"
#include "../shared.h"

// Effect: Nexus
// Authors: kostyamat
// Based on:
//   - DmytroKorniienko/FireLamp_JeeUI
//     https://github.com/DmytroKorniienko/FireLamp_JeeUI/blob/master/src/effects.cpp

static void nexusReset(uint8_t i) {
  trackingObjectHue[i] = random8();
  trackingObjectState[i] = random8(4);
  //trackingObjectSpeedX[i] = (255. + random8()) / 255.;
  trackingObjectSpeedX[i] =
    static_cast<float>(random8(5, 11)) / 70 +
    speedfactor; // делаем частицам немного разное ускорение и сразу пересчитываем под общую скорость
  switch (trackingObjectState[i]) {
    case B01:
      trackingObjectPosY[i] = HEIGHT;
      trackingObjectPosX[i] = random8(WIDTH);
      break;
    case B00:
      trackingObjectPosY[i] = -1;
      trackingObjectPosX[i] = random8(WIDTH);
      break;
    case B10:
      trackingObjectPosX[i] = WIDTH;
      trackingObjectPosY[i] = random8(HEIGHT);
      break;
    case B11:
      trackingObjectPosX[i] = -1;
      trackingObjectPosY[i] = random8(HEIGHT);
      break;
  }
}

void EffectNexus::setup(EffectContext& ctx) {
  speedfactor = fmap(ctx.speed, 1, 255, 0.1, 0.33);
  for (uint8_t i = 0; i < enlargedObjectMaxCount; i++) {
    trackingObjectPosX[i] = random8(WIDTH);
    trackingObjectPosY[i] = random8(HEIGHT);
    trackingObjectSpeedX[i] =
      static_cast<float>(random8(5, 11)) / 70 +
      speedfactor; // делаем частицам немного разное ускорение и сразу пересчитываем под общую скорость
    trackingObjectHue[i] = random8();
    trackingObjectState[i] = random8(4); // задаем направление
  }
  deltaValue = 255U - map(ctx.speed, 1, 255, 11, 33);
}

void EffectNexus::render(EffectContext& ctx) {
  ctx.led.scale(deltaValue);

  enlargedObjectNum = map8(ctx.scale, 1U, enlargedObjectMaxCount);

  for (uint8_t i = 0; i < enlargedObjectNum; i++) {
    switch (trackingObjectState[i]) {
      case B01:
        trackingObjectPosY[i] -= trackingObjectSpeedX[i];
        if (trackingObjectPosY[i] <= -1) nexusReset(i);
        break;
      case B00:
        trackingObjectPosY[i] += trackingObjectSpeedX[i];
        if (trackingObjectPosY[i] >= HEIGHT) nexusReset(i);
        break;
      case B10:
        trackingObjectPosX[i] -= trackingObjectSpeedX[i];
        if (trackingObjectPosX[i] <= -1) nexusReset(i);
        break;
      case B11:
        trackingObjectPosX[i] += trackingObjectSpeedX[i];
        if (trackingObjectPosX[i] >= WIDTH) nexusReset(i);
        break;
    }
    if (ctx.palette) {
      ctx.led.drawPixelSafe(
        trackingObjectPosX[i], trackingObjectPosY[i], ColorFromPalette(*ctx.palette, trackingObjectHue[i], 255U)
      );
    } else {
      ctx.led.drawPixelSafe(trackingObjectPosX[i], trackingObjectPosY[i], CHSV(trackingObjectHue[i], 255U, 255));
    }
  }
}
