#include "color_change.h"
#include "../shared.h"

// Effect: Color Change

void EffectColorChange::setup(EffectContext& ctx) {
  resetEveryMs(step, 255 - ctx.speed, ctx.nowMs);
  if (ctx.palette == nullptr) {
    rgbPalette = CRGBPalette16(RainbowColors_p);
  }
}

void EffectColorChange::render(EffectContext& ctx) {
  const CRGBPalette16& palette = ctx.palette ? *ctx.palette : rgbPalette;

  if (everyMs(step, 255 - ctx.speed, ctx.nowMs)) {
    hue++;
  }

  ctx.led.fill(ColorFromPalette(palette, hue + ctx.scale));
}
