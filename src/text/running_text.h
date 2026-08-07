#pragma once

#include <FastLED.h>
#include <WString.h>

class Led;
class NotificationOverlay;

class RunningText {
public:
  RunningText(Led& led, int width)
    : led_(led),
      width_(width),
      offset_(width) {}

  bool isActive() const { return active_; }
  void reset();
  void start(const String& text, const CRGB& color, bool loop = false);
  bool render();
  bool render(NotificationOverlay& overlay);

private:
  static constexpr uint16_t SCROLL_INTERVAL_MS = 50;

  Led& led_;
  int width_ = 0;
  int offset_ = 0;
  uint32_t frameTimer_ = 0;
  uint32_t scrollTimer_ = 0;
  bool active_ = false;
  bool loop_ = false;
  String text_;
  CRGB color_ = CRGB::White;

  bool advance(const String& text);
};
