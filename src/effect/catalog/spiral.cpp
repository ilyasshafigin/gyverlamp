#include "spiral.h"
#include "../shared.h"

// Effect: Spiral - эффект спирали
// Authors: Jason Coon (Aurora); adapted by SottNick
// Based on:
//   - Aurora PatternSpiro
//     https://github.com/pixelmatix/aurora/blob/sm3.0-64x64/PatternSpiro.h
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L2862
// Notes:
//   - Неполная адаптация SottNick

uint8_t spirotheta1 = 0;
uint8_t spirotheta2 = 0;
uint8_t spirohueoffset = 0;

const uint8_t spiroradiusx = WIDTH / 4;
const uint8_t spiroradiusy = HEIGHT / 4;

const uint8_t spirocenterX = WIDTH / 2;
const uint8_t spirocenterY = HEIGHT / 2;

const uint8_t spirominx = spirocenterX - spiroradiusx;
const uint8_t spiromaxx = spirocenterX + spiroradiusx + 1;
const uint8_t spirominy = spirocenterY - spiroradiusy;
const uint8_t spiromaxy = spirocenterY + spiroradiusy + 1;

uint8_t spirocount = 1;
uint8_t spirooffset = 256U / spirocount;
boolean spiroincrement = false;

boolean spirohandledChange = false;

void EffectSpiral::setup(EffectContext& ctx) {
  resetEveryMs(step, 12U, ctx.nowMs);
  resetEveryMs(pcnt, 75U, ctx.nowMs);
  resetEveryMs(deltaValue, 33U, ctx.nowMs);
}

void EffectSpiral::render(EffectContext& ctx) {
  const CRGBPalette16& palette =
    ctx.palette ? *ctx.palette : *Palettes::getPaletteByScale(ctx.scale);

  ctx.led.scale(250);

  boolean change = false;

  for (int i = 0; i < spirocount; i++) {
    uint8_t x = mapsin8(spirotheta1 + i * spirooffset, spirominx, spiromaxx);
    uint8_t y = mapcos8(spirotheta1 + i * spirooffset, spirominy, spiromaxy);

    uint8_t x2 = mapsin8(spirotheta2 + i * spirooffset, x - spiroradiusx, x + spiroradiusx);
    uint8_t y2 = mapcos8(spirotheta2 + i * spirooffset, y - spiroradiusy, y + spiroradiusy);

    CRGB color = ColorFromPalette(palette, (spirohueoffset + i * spirooffset), 128U);
    ctx.led.getPixelSafe(x2, y2) += color;

    if ((x2 == spirocenterX && y2 == spirocenterY) || (x2 == spirocenterX && y2 == spirocenterY)) {
      change = true;
    }
  }

  spirotheta2 += 2;

  if (everyMs(step, 12U, ctx.nowMs)) {
    spirotheta1 += 1;
  }

  if (everyMs(pcnt, 75U, ctx.nowMs)) {
    if (change && !spirohandledChange) {
      spirohandledChange = true;

      if (spirocount >= WIDTH || spirocount == 1) {
        spiroincrement = !spiroincrement;
      }

      if (spiroincrement) {
        if (spirocount >= 4) spirocount *= 2;
        else spirocount += 1;
      } else {
        if (spirocount > 4) spirocount /= 2;
        else spirocount -= 1;
      }

      spirooffset = 256 / spirocount;
    }

    if (!change) spirohandledChange = false;
  }

  if (everyMs(deltaValue, 33U, ctx.nowMs)) {
    spirohueoffset += 1;
  }
}
