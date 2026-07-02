#include "overlay.h"
#include "render_utils.h"
#include "../hardware/led.h"

static constexpr uint8_t TOP_Y = HEIGHT - 1;

void NotificationOverlay::drawPixel(uint8_t x, uint8_t y, CRGB color) {
  color.nscale8(_opacity);

  CRGB& bg = _led.getPixel(x, y);
  bg.fadeToBlackBy(_holeDim);
  bg += color;
}

void NotificationOverlay::drawPixelSafe(int x, int y, CRGB color) {
  color.nscale8(_opacity);

  CRGB& bg = _led.getPixelSafe(x, y);
  bg.fadeToBlackBy(_holeDim);
  bg += color;
}

void NotificationOverlay::addPixel(uint8_t x, uint8_t y, CRGB color) {
  color.nscale8(_opacity);

  _led.addPixel(x, y, color);
}

void NotificationOverlay::fill(CRGB color) {
  color.nscale8(_opacity);

  _led.fadeToBlack(_holeDim);
  _led.add(color);
}

void NotificationOverlay::clearTop() {
  for (uint8_t x = 0; x < WIDTH; x++) {
    _led.drawPixel(x, TOP_Y, CRGB::Black);
    // захватим еще одну строку
    _led.drawPixel(x, TOP_Y - 1, CRGB::Black);
  }
}

void NotificationOverlay::drawTopSolid(const CRGB& color) {
  for (uint8_t x = 0; x < WIDTH; x++) {
    drawPixel(x, TOP_Y, color);
  }
}

void NotificationOverlay::drawTopSpinner(const CRGB& color, uint32_t startedMs) {
  const uint8_t head = ((millis() - startedMs) / 90) % WIDTH;

  for (uint8_t x = 0; x < WIDTH; x++) {
    const uint8_t distance = (x + WIDTH - head) % WIDTH;

    CRGB pixelColor = CRGB::Black;
    if (distance == 0) {
      pixelColor = color;
      pixelColor.nscale8(220);
    } else if (distance == 1 || distance == WIDTH - 1) {
      pixelColor = color;
      pixelColor.nscale8(100);
    } else if (distance == 2 || distance == WIDTH - 2) {
      pixelColor = color;
      pixelColor.nscale8(35);
    }

    if (pixelColor != CRGB::Black) {
      drawPixel(x, TOP_Y, pixelColor);
    }
  }
}

void NotificationOverlay::drawTopProgress(const CRGB& color, uint32_t startedMs, uint8_t percent) {
  if (percent > 100) percent = 100;

  const uint8_t lit = static_cast<uint16_t>(WIDTH) * percent / 100;

  for (uint8_t x = 0; x < WIDTH; x++) {
    CRGB pixelColor = color;
    if (x < lit) {
      pixelColor.nscale8(180);
    } else {
      pixelColor.nscale8(25);
    }
    drawPixel(x, TOP_Y, pixelColor);
  }

  // Живой огонёк поверх прогресса, чтобы было видно, что OTA не зависла
  drawTopSpinner(color, startedMs);
}

void NotificationOverlay::drawTopPulse(
  const CRGB& color,
  uint32_t startedMs,
  uint16_t periodMs,
  uint8_t minBrightness,
  uint8_t maxBrightness
) {
  CRGB pulse = color;
  pulse.nscale8(notificationBreathe(startedMs, periodMs, minBrightness, maxBrightness));
  drawTopSolid(pulse);
}

void NotificationOverlay::drawTopDoublePulse(
  const CRGB& color,
  uint32_t startedMs,
  uint16_t periodMs
) {
  const uint16_t t = (millis() - startedMs) % periodMs;
  uint8_t value = 0;
  if (t < 180) {
    value = map(t, 0, 180, 0, 180);
  } else if (t < 360) {
    value = map(t, 180, 360, 180, 0);
  } else if (t < 540) {
    value = map(t, 360, 540, 0, 180);
  } else if (t < 720) {
    value = map(t, 540, 720, 180, 0);
  } else {
    value = 0;
  }
  CRGB pulse = color;
  drawTopSolid(pulse.nscale8(value));
}

void NotificationOverlay::drawFullPulse(
  const CRGB& color,
  uint32_t startedMs,
  uint16_t periodMs,
  uint8_t minBrightness,
  uint8_t maxBrightness
) {
  CRGB pixel = color;
  fill(pixel.nscale8(notificationBreathe(startedMs, periodMs, minBrightness, maxBrightness)));
}

void NotificationOverlay::drawVerticalWave(
  const CRGB& color,
  uint32_t startedMs,
  uint16_t periodMs,
  uint8_t minBrightness,
  uint8_t maxBrightness
) {
  const uint32_t elapsedMs = millis() - startedMs;
  const uint32_t progressMs = elapsedMs > periodMs ? periodMs : elapsedMs;

  const uint8_t waveHeight = max<uint8_t>(2, HEIGHT / 3);
  const int32_t halfWaveQ8 = static_cast<int32_t>(waveHeight) * 128;
  const int32_t startQ8 = (static_cast<int32_t>(HEIGHT - 1) * 256) + halfWaveQ8;
  const int32_t endQ8 = -halfWaveQ8;
  const int32_t centerQ8 = startQ8 - ((startQ8 - endQ8) * static_cast<int32_t>(progressMs) / periodMs);

  const uint16_t fadeMs = max<uint16_t>(1, periodMs / 5);
  uint8_t envelope = 255;
  if (progressMs < fadeMs) {
    envelope = progressMs * 255UL / fadeMs;
  } else if (periodMs - progressMs < fadeMs) {
    envelope = (periodMs - progressMs) * 255UL / fadeMs;
  }

  for (uint8_t y = 0; y < HEIGHT; y++) {
    const int32_t distanceQ8 = abs(static_cast<int32_t>(y) * 256 - centerQ8);
    if (distanceQ8 >= halfWaveQ8) continue;

    uint8_t softness = 255 - (distanceQ8 * 255UL / halfWaveQ8);
    softness = scale8(softness, softness);

    const uint8_t wave = scale8(softness, envelope);
    const uint8_t brightness = minBrightness + scale8(wave, maxBrightness - minBrightness);
    if (brightness == 0) continue;

    CRGB pixel = color;
    pixel.nscale8(brightness);

    for (uint8_t x = 0; x < WIDTH; x++) {
      addPixel(x, y, pixel);
    }
  }
}
