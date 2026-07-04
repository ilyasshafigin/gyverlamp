#include "warmlight.h"
#include "../shared.h"

// Effect: Warm Light
// Based on:
//   - Whilser/GyverLamp

void EffectWarmLight::render(EffectContext& ctx) {
  uint8_t centerY = CENTER_Y_MINOR;
  uint8_t bottomOffset = static_cast<uint8_t>(!(HEIGHT & 1) && (HEIGHT > 1));
  for (int16_t y = centerY; y >= 0; y--) {
    uint8_t value = y == centerY                                                                ? 255U
                    : (ctx.scale / 100.0F) > ((centerY + 1.0F) - (y + 1.0F)) / (centerY + 1.0F) ? 255U
                                                                                                : 0U;
    CRGB color;
    if (ctx.palette) {
      color = ColorFromPalette(*ctx.palette, map(y, 0, centerY, 0, 240), value);
    } else {
      color = CHSV(45U, map(ctx.speed, 0U, 255U, 0U, 170U), value);
    }
    for (uint8_t x = 0U; x < WIDTH; x++) {
      ctx.led.drawPixel(x, y, color);
      ctx.led.drawPixel(x, max(static_cast<uint8_t>(HEIGHT - 1U) - (y + 1U) + bottomOffset, 0U), color);
    }
  }
}
