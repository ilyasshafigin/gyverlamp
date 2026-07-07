#include "indicator_renderer.h"

#include "../config.h"
#include "render_utils.h"

static constexpr uint8_t TOP_Y = HEIGHT - 1;

void IndicatorRenderer::render(NotificationOverlay& overlay, const NotificationSnapshot& notification) {
  switch (notification.indicatorType) {
    case IndicatorType::PowerOn: renderPowerOn(overlay, notification.startedMs); break;
    case IndicatorType::Dismiss: renderDismiss(overlay, notification.startedMs); break;
    case IndicatorType::NextEffect:
      renderSwitchSweep(overlay, notification.startedMs, true, CRGB(200, 230, 255));
      break;
    case IndicatorType::PreviousEffect:
      renderSwitchSweep(overlay, notification.startedMs, false, CRGB(200, 230, 255));
      break;
    case IndicatorType::Brightness:
      renderBrightness(overlay, notification.buttonValue, notification.buttonDirection, notification.startedMs);
      break;
    case IndicatorType::RotationOn:
      renderRotationDots(overlay, notification.startedMs, true, CRGB(80, 255, 80));
      break;
    case IndicatorType::RotationOff:
      renderRotationDots(overlay, notification.startedMs, false, CRGB(255, 80, 80));
      break;
    case IndicatorType::PowerOff:
    case IndicatorType::None:
    default: break;
  }

  if (notification.buttonPressCount > 0) {
    renderPressEcho(
      overlay, notification.buttonPressCount, notification.buttonPressMs, notification.buttonPressing, CRGB::White
    );
  }
}

void IndicatorRenderer::renderPowerOn(NotificationOverlay& overlay, uint32_t startedMs) {
  overlay.clearTop();

  CRGB color = CRGB(120, 255, 120);
  color.nscale8(notificationBreathe(startedMs, 700, 20, 180));
  overlay.drawTopSolid(color);
}

void IndicatorRenderer::renderDismiss(NotificationOverlay& overlay, uint32_t startedMs) {
  overlay.clearTop();

  const uint32_t elapsed = millis() - startedMs;
  uint8_t brightness = 0;

  if (elapsed < 120) {
    brightness = 220;
  } else if (elapsed < 450) {
    brightness = map(elapsed, 120, 450, 220, 0);
  }

  CRGB color = CRGB::White;
  color.nscale8(brightness);
  overlay.drawTopSolid(color);
}

void IndicatorRenderer::renderBrightness(
  NotificationOverlay& overlay, uint8_t brightness, bool increasing, uint32_t startedMs
) {
  (void)startedMs;

  overlay.clearTop();

  const CRGB color = increasing ? CRGB(255, 210, 80) : CRGB(80, 140, 255);
  const uint8_t lit = max<uint8_t>(1, static_cast<uint16_t>(WIDTH) * brightness / 255);
  const uint8_t marker = min<uint8_t>(WIDTH - 1, lit - 1);

  for (uint8_t x = 0; x < WIDTH; x++) {
    CRGB pixel = color;

    if (x < lit) {
      pixel.nscale8(120);
    } else {
      pixel.nscale8(8);
    }

    overlay.drawPixel(x, TOP_Y, pixel);
  }

  CRGB markerColor = color;
  markerColor.nscale8(240);
  overlay.drawPixel(marker, TOP_Y, markerColor);
}

void IndicatorRenderer::renderSwitchSweep(
  NotificationOverlay& overlay, uint32_t startedMs, bool clockwise, const CRGB& color
) {
  overlay.clearTop();

  // Комета: яркая голова движется вдоль ряда, за ней затухающий хвост
  // длиной в полкольца (WIDTH/2). После выхода за край хвост стирается.
  constexpr uint16_t sweepMs = 800;
  constexpr uint8_t tailLen = WIDTH / 2;
  const uint32_t elapsed = millis() - startedMs;
  if (elapsed >= sweepMs) return;

  // Голова проходит виртуальные позиции [0, WIDTH + tailLen) за sweepMs:
  // входит с x=0, выходит за x=WIDTH-1, увлекая хвост.
  const int16_t p = static_cast<int16_t>(static_cast<int32_t>(elapsed) * (WIDTH + tailLen) / sweepMs);

  for (uint8_t t = 0; t < tailLen; t++) {
    const int16_t x_signed = clockwise ? (p - t) : (static_cast<int16_t>(WIDTH) - 1 - p + t);
    if (x_signed < 0 || x_signed >= WIDTH) continue;
    const uint8_t x = static_cast<uint8_t>(x_signed);

    // Голова (t=0) яркая, к концу хвоста альфа падает до 0.
    uint8_t tailAlpha = static_cast<uint8_t>(255U - static_cast<uint16_t>(t) * 255U / tailLen);

    CRGB pixel = color;
    pixel.nscale8(tailAlpha);
    overlay.drawPixel(x, HEIGHT - 1, pixel);
  }
}

void IndicatorRenderer::renderRotationDots(
  NotificationOverlay& overlay, uint32_t startedMs, bool clockwise, const CRGB& color
) {
  // Три точки, распределённые равномерно по верхнему кольцу (ряду с wrap).
  // Каждая пробегает полкольца (WIDTH/2 пикселей) за DURATION_MS.
  constexpr uint32_t DURATION_MS = 900;
  constexpr uint32_t FADE_IN_MS = 120;
  constexpr uint32_t FADE_OUT_MS = 200;
  // Короткий затухающий след за каждой точкой — подчёркивает направление движения.
  constexpr uint8_t TRAIL_LEN = 2;

  const uint32_t elapsed = millis() - startedMs;
  if (elapsed >= DURATION_MS) return;

  overlay.clearTop();

  uint8_t alpha = 255;
  if (elapsed < FADE_IN_MS) {
    alpha = static_cast<uint8_t>(elapsed * 255UL / FADE_IN_MS);
  } else if (elapsed > DURATION_MS - FADE_OUT_MS) {
    alpha = static_cast<uint8_t>((DURATION_MS - elapsed) * 255UL / FADE_OUT_MS);
  }

  const int8_t dir = clockwise ? 1 : -1;
  const int16_t offset = static_cast<int16_t>(static_cast<int32_t>(elapsed) * (WIDTH / 2) / DURATION_MS);

  for (uint8_t i = 0; i < 3; i++) {
    const int16_t base = static_cast<int16_t>(i) * WIDTH / 3;

    for (uint8_t t = 0; t <= TRAIL_LEN; t++) {
      int16_t pos = (base + dir * offset - dir * t) % WIDTH;
      if (pos < 0) pos += WIDTH;

      uint8_t local = alpha;
      if (t > 0) {
        // Голова (t=0) яркая, точки следа плавно затухают.
        local = scale8(alpha, static_cast<uint8_t>(110U - t * 35U));
      }

      CRGB pixel = color;
      pixel.nscale8(local);
      overlay.drawPixel(static_cast<uint8_t>(pos), HEIGHT - 1, pixel);
    }
  }
}

void IndicatorRenderer::renderPressEcho(
  NotificationOverlay& overlay, uint8_t count, uint32_t pressMs, bool pressing, const CRGB& color
) {
  if (count == 0) return;
  count = min<uint8_t>(count, 6);

  const uint32_t elapsed = millis() - pressMs;
  uint8_t alpha = pressing ? 255 : 0;

  if (!pressing) {
    if (elapsed >= 180) return;
    alpha = map(elapsed, 0, 180, 255, 0);
  }

  for (uint8_t tap = 1; tap <= count; tap++) {
    const uint8_t center = static_cast<uint16_t>(tap - 1) * WIDTH / 5;

    for (int8_t dx = -1; dx <= 1; dx++) {
      const uint8_t x = (center + WIDTH + dx) % WIDTH;

      uint8_t local = dx == 0 ? 230 : 90;

      // текущий tap ярче, предыдущие спокойнее
      if (tap < count) {
        local = dx == 0 ? 120 : 35;
      }

      CRGB pixel = color;
      pixel.nscale8(scale8(local, alpha));
      overlay.drawPixel(x, HEIGHT - 1, pixel);
    }
  }
}
