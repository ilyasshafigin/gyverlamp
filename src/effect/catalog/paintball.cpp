#include "paintball.h"
#include "../shared.h"

// Effect: Paintball
// Based on:
//   - Whilser/GyverLamp
//     https://github.com/Whilser/GyverLamp/blob/e7545c18f75911df6fc017d3e18c52cdd994cfc9/firmware/GyverLamp_v1.6_MQTT/effects.ino#L427
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L2217

// глубина бордюра для размытия яркой частицы:
// - 0U - без границы (резкие края)
// - 1U - 1 пиксель (среднее размытие)
// - 2U - 2 пикселя (глубокое размытие)
#define BORDERTHICKNESS (1U)
const uint8_t paintWidth = WIDTH - BORDERTHICKNESS * 2;
const uint8_t paintHeight = HEIGHT - BORDERTHICKNESS * 2;

// blur() runs FastLED blur2d — a full 2-pass matrix convolution over all
// pixels — and is by far the most expensive op in this effect. On ESP8266
// running it every frame blows the 20ms (50 FPS) budget, the call to show()
// gets skipped, and the strip never updates. Throttle to one blur per
// interval; the 4 moving balls are still drawn every frame, so motion stays
// smooth. Trails get slightly longer, which is the accepted trade-off.
#define BLUR_INTERVAL_MS (30U)

void EffectPaintball::setup(EffectContext& ctx) {
  if (ctx.palette == nullptr) {
    rgbPalette = CRGBPalette16(RainbowColors_p);
  }
  resetEveryMs(step, BLUR_INTERVAL_MS, ctx.nowMs);
}

void EffectPaintball::render(EffectContext& ctx) {
  // Apply some blurring to whatever's already on the matrix
  // Note that we never actually clear the matrix, we just constantly
  // blur it repeatedly.  Since the blurring is 'lossy', there's
  // an automatic trend toward black -- by design.
  if (everyMs(step, BLUR_INTERVAL_MS, ctx.nowMs)) {
    ctx.led.blur(dim8_raw(beatsin8(3, 64, 100)));
  }

  // Ball position = sin16 of a FastLED beat. The first beatsin16() arg is the
  // beat rate (~oscillations/min): bigger = faster ball. ctx.speed scales that
  // rate, so at the default speed=30 the balls drift slowly; raise the speed
  // slider to liven them up. Each ball keeps its own base+offset so the four
  // never sync. Range: speed=0 -> very slow drift, speed=255 -> original pace.
  const uint8_t spd = map(ctx.speed, 0, 255, 0, 80);
  uint16_t i = beatsin16(20 + spd, 0, 255);
  uint16_t j = beatsin16(17 + spd, 0, 255);
  uint16_t k = beatsin16(13 + spd, 0, 255);
  uint16_t m = beatsin16(25 + spd, 0, 255);

  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : rgbPalette;

  // The color of each point shifts over time, each at a different speed.
  uint32_t ms = ctx.nowMs / (ctx.scale / 4 + 1);
  ctx.led.getPixelSafe(highByte(i * paintWidth) + BORDERTHICKNESS, highByte(j * paintHeight) + BORDERTHICKNESS) +=
    ColorFromPalette(palette, ms / 29);
  ctx.led.getPixelSafe(highByte(j * paintWidth) + BORDERTHICKNESS, highByte(k * paintHeight) + BORDERTHICKNESS) +=
    ColorFromPalette(palette, ms / 41);
  ctx.led.getPixelSafe(highByte(k * paintWidth) + BORDERTHICKNESS, highByte(m * paintHeight) + BORDERTHICKNESS) +=
    ColorFromPalette(palette, ms / 37);
  ctx.led.getPixelSafe(highByte(m * paintWidth) + BORDERTHICKNESS, highByte(i * paintHeight) + BORDERTHICKNESS) +=
    ColorFromPalette(palette, ms / 53);
}
