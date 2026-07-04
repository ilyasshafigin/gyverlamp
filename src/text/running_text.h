#pragma once

#include <FastLED.h>
#include <WString.h>

class Led;
class NotificationOverlay;

class RunningText {
public:
  RunningText(Led& led, int width)
    : _led(led),
      _width(width),
      _offset(width) {}

  bool isActive() const { return _active; }
  void reset();
  void start(const String& text, const CRGB& color, bool loop = false);
  bool render();
  bool render(NotificationOverlay& overlay);

private:
  static constexpr uint16_t SCROLL_INTERVAL_MS = 50;

  Led& _led;
  int _width = 0;
  int _offset = 0;
  uint32_t _frameTimer = 0;
  uint32_t _scrollTimer = 0;
  bool _active = false;
  bool _loop = false;
  String _text;
  CRGB _color = CRGB::White;

  bool advance(const String& text);
};
