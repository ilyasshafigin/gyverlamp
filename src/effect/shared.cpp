#include "shared.h"

uint8_t hue, hue2;
uint8_t deltaHue, deltaHue2;
uint8_t step;
uint8_t pcnt;
uint8_t deltaValue;
float speedfactor;
float emitterX, emitterY;
uint16_t ff_x, ff_y, ff_z;

uint8_t noise3d[NUM_LAYERSMAX][WIDTH][HEIGHT];
uint8_t line[WIDTH];
uint8_t shiftHue[HEIGHT];
uint8_t shiftValue[HEIGHT];

float trackingObjectPosX[trackingObjectMaxCount];
float trackingObjectPosY[trackingObjectMaxCount];
float trackingObjectSpeedX[trackingObjectMaxCount];
float trackingObjectSpeedY[trackingObjectMaxCount];
float trackingObjectShift[trackingObjectMaxCount];
uint8_t trackingObjectHue[trackingObjectMaxCount];
uint8_t trackingObjectState[trackingObjectMaxCount];
bool trackingObjectIsShift[trackingObjectMaxCount];
uint8_t enlargedObjectNum;

CRGBPalette16 rgbPalette(PartyColors_p);

bool everyMs(uint8_t& latch, uint16_t intervalMs, uint32_t nowMs) {
  if (intervalMs == 0U) intervalMs = 1U;

  const uint8_t tick = static_cast<uint8_t>(nowMs / intervalMs);
  const bool elapsed = tick != latch;
  latch = tick;
  return elapsed;
}

void resetEveryMs(uint8_t& latch, uint16_t intervalMs, uint32_t nowMs) {
  if (intervalMs == 0U) intervalMs = 1U;
  latch = static_cast<uint8_t>(nowMs / intervalMs);
}
