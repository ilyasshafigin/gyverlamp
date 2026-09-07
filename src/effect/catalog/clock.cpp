#include "clock.h"

#include "../effect.h"
#include "../shared.h"
#include "../../text/text_renderer.h"
#include "../../time/time_service.h"

// Effect: Clock

namespace {

  String formatClock(uint8_t hrs, uint8_t mins, bool showSeparator) {
    char buf[16];
    snprintf(
      buf,
      sizeof(buf),
      "%02u%c%02u      ",
      static_cast<unsigned>(hrs),
      showSeparator ? ':' : ' ',
      static_cast<unsigned>(mins)
    );
    return String(buf);
  }

} // namespace

void EffectClock::setup(EffectContext& ctx) {
  offset_ = ctx.width;
  lastMinute_ = 255;
  lastSecond_ = 255;
  separatorVisible_ = true;
  scrollTimer_ = ctx.nowMs;
  rebuildText(ctx);
}

void EffectClock::render(EffectContext& ctx) {
  const uint8_t mins = ctx.time.minutes();
  const uint8_t secs = ctx.time.seconds();

  if (mins != lastMinute_ || secs != lastSecond_) {
    separatorVisible_ = !separatorVisible_;
    lastMinute_ = mins;
    lastSecond_ = secs;
    rebuildText(ctx);
  }

  const uint16_t scrollInterval = speedToIntervalMs(ctx.speed, 100, 20);

  if (ctx.nowMs - scrollTimer_ >= scrollInterval) {
    scrollTimer_ = ctx.nowMs;

    const int16_t textWidth = TextRenderer::stringWidth(text_);
    offset_--;
    if (offset_ < -textWidth) {
      offset_ = ctx.width;
    }
  }

  ctx.led.clearLeds();

  if (ctx.palette) {
    TextRenderer::drawString(ctx.led, offset_, kTextY, text_, ColorFromPalette(*ctx.palette, ctx.scale), false);
  } else {
    const uint8_t hue = ctx.scale;
    if (hue == 1U) {
      TextRenderer::drawString(ctx.led, offset_, kTextY, text_, CRGB::White, false);
    } else {
      TextRenderer::drawString(ctx.led, offset_, kTextY, text_, CHSV(hue, 255, 255), false);
    }
  }
}

void EffectClock::rebuildText(EffectContext& ctx) {
  const uint8_t hrs = ctx.time.hours();
  const uint8_t mins = ctx.time.minutes();
  text_ = formatClock(hrs, mins, separatorVisible_);
}
