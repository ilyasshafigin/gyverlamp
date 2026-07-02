#include "color.h"

// Effect: Color

void EffectColor::render(EffectContext& ctx) {
  ctx.led.fill(CRGB(ctx.red, ctx.green, ctx.blue));
}
