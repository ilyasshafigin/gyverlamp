#include "clock.h"
#include "../effect.h"
#include "../shared.h"
#include "../../text/text_renderer.h"
#include "../../time/time_service.h"

// Effect: Clock

namespace {

  String formatClock(uint8_t hrs, uint8_t mins, bool showSeparator) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u%c%02u      ",
      static_cast<unsigned>(hrs),
      showSeparator ? ':' : ' ',
      static_cast<unsigned>(mins));
    return String(buf);
  }

}  // namespace

void EffectClock::setup(EffectContext& ctx) {
  _offset = ctx.width;
  _lastMinute = 255;
  _lastSecond = 255;
  _separatorVisible = true;
  _scrollTimer = ctx.nowMs;
  rebuildText(ctx);
}

void EffectClock::render(EffectContext& ctx) {
  const uint8_t mins = ctx.time.getMinutes();
  const uint8_t secs = ctx.time.getSeconds();

  if (mins != _lastMinute || secs != _lastSecond) {
    _separatorVisible = !_separatorVisible;
    _lastMinute = mins;
    _lastSecond = secs;
    rebuildText(ctx);
  }

  const uint16_t scrollInterval = speedToIntervalMs(ctx.speed, 100, 20);

  if (ctx.nowMs - _scrollTimer >= scrollInterval) {
    _scrollTimer = ctx.nowMs;

    const int16_t textWidth = TextRenderer::stringWidth(_text);
    _offset--;
    if (_offset < -textWidth) {
      _offset = ctx.width;
    }
  }

  ctx.led.clearLeds();

  if (ctx.palette) {
    TextRenderer::drawString(ctx.led, _offset, TEXT_Y, _text, ColorFromPalette(*ctx.palette, ctx.scale), false);
  } else {
    const uint8_t hue = ctx.scale;
    if (hue == 1U) {
      TextRenderer::drawString(ctx.led, _offset, TEXT_Y, _text, CRGB::White, false);
    } else {
      TextRenderer::drawString(ctx.led, _offset, TEXT_Y, _text, CHSV(hue, 255, 255), false);
    }
  }
}

void EffectClock::rebuildText(EffectContext& ctx) {
  const uint8_t hrs = ctx.time.getHours();
  const uint8_t mins = ctx.time.getMinutes();
  _text = formatClock(hrs, mins, _separatorVisible);
}

