#include "octopus.h"
#include "../shared.h"

// Effect: Octupus - Осьминог
// Authors: Stepko and Sutaburosu, Adapted and modifed © alvikskor
// Based on:
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects_new.ino#L2401
// Notes:
//   - thanks ldirko
//   - thanks Sutaburosu

void EffectOctopus::setup(EffectContext& ctx) {
  for (int16_t x = -CENTER_X_MAJOR; x < CENTER_X_MAJOR + (WIDTH % 2); x++) {
    for (int16_t y = -CENTER_Y_MAJOR; y < CENTER_Y_MAJOR + (HEIGHT % 2); y++) {
      noise3d[0][x + CENTER_X_MAJOR][y + CENTER_Y_MAJOR] = (atan2(x, y) / PI) * 128 + 127; // thanks ldirko
      noise3d[1][x + CENTER_X_MAJOR][y + CENTER_Y_MAJOR] = hypot(x, y); // thanks Sutaburosu
    }
  }
  resetEveryMs(pcnt, speedToIntervalMs(ctx.speed, 60U, 20U), ctx.nowMs);
  ff_x = 0;
  ff_y = 0;
}

void EffectOctopus::render(EffectContext& ctx) {
  uint8_t legs = ctx.scale / 25;

  if (everyMs(pcnt, speedToIntervalMs(ctx.speed, 60U, 10U), ctx.nowMs)) {
    ff_x++;
  }

  step = ctx.scale % 25;
  if (step < 5) ff_y = ff_x / (3 - step / 2);
  else ff_y = ff_x * (step / 2 - 1);

  for (uint8_t x = 0; x < WIDTH; x++) {
    for (uint8_t y = 0; y < HEIGHT; y++) {
      uint8_t angle = noise3d[0][x][y];
      uint8_t radius = noise3d[1][x][y];
      uint8_t hue = ff_y - radius * (255 / WIDTH);
      uint8_t bri = sin8(sin8((angle * 4 - (radius * (255 / WIDTH))) / 4 + ff_x) + radius * (255 / WIDTH) - ff_x * 2 + angle * legs);
      ctx.led.drawPixel(x, y, ctx.palette ? ColorFromPalette(*ctx.palette, hue, bri) : CHSV(hue, 255, bri));
    }
  }
}
