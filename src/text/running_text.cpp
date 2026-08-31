#include "../hardware/led.h"
#include "../notification/overlay.h"
#include "running_text.h"
#include "text_renderer.h"

#define TEXT_HEIGHT 2 // высота, на которой бежит текст (от низа матрицы)

void RunningText::reset() {
  offset_ = width_ - 1;
  frameTimer_ = 0;
  scrollTimer_ = 0;
}

void RunningText::start(const String& text, const CRGB& color, bool loop) {
  text_ = text;
  color_ = color;
  loop_ = loop;
  active_ = true;
  reset();
}

bool RunningText::render() {
  if (!active_) return false;
  advance(text_);
  TextRenderer::drawString(led_, offset_, TEXT_HEIGHT, text_, color_);
  return true;
}

bool RunningText::render(NotificationOverlay& overlay) {
  if (!active_) return false;
  advance(text_);
  TextRenderer::drawString(overlay, offset_, TEXT_HEIGHT, text_, color_);
  return true;
}

bool RunningText::advance(const String& text) {
  const uint32_t now = millis();

  if (now - scrollTimer_ >= kScrollIntervalMs) {
    scrollTimer_ = now;

    const int16_t textWidth = TextRenderer::stringWidth(text);
    offset_--;
    if (offset_ < -textWidth) {
      offset_ = width_ - 1;
      if (!loop_) active_ = false;
    }
  }

  return true;
}
