#include "../hardware/led.h"
#include "../notification/overlay.h"
#include "running_text.h"
#include "text_renderer.h"

#define TEXT_HEIGHT 2     // высота, на которой бежит текст (от низа матрицы)

void RunningText::reset() {
  _offset = _width - 1;
  _frameTimer = 0;
  _scrollTimer = 0;
}

void RunningText::start(const String& text, const CRGB& color, bool loop) {
  _text = text;
  _color = color;
  _loop = loop;
  _active = true;
  reset();
}

bool RunningText::render() {
  if (!_active) return false;
  advance(_text);
  TextRenderer::drawString(_led, _offset, TEXT_HEIGHT, _text, _color);
  return true;
}

bool RunningText::render(NotificationOverlay& overlay) {
  if (!_active) return false;
  advance(_text);
  TextRenderer::drawString(overlay, _offset, TEXT_HEIGHT, _text, _color);
  return true;
}

bool RunningText::advance(const String& text) {
  const uint32_t now = millis();

  if (now - _scrollTimer >= SCROLL_INTERVAL_MS) {
    _scrollTimer = now;

    const int16_t textWidth = TextRenderer::stringWidth(text);
    _offset--;
    if (_offset < -textWidth) {
      _offset = _width - 1;
      if (!_loop) _active = false;
    }
  }

  return true;
}
