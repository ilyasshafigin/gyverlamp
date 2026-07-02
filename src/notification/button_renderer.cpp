#include "button_renderer.h"

#include "../config.h"
#include "render_utils.h"

static constexpr uint8_t TOP_Y = HEIGHT - 1;

void ButtonNotificationRenderer::render(NotificationOverlay& overlay, const NotificationSnapshot& notification) {
  switch (notification.buttonType) {
  case ButtonNotificationType::PowerOn:
    renderPowerOn(overlay, notification.startedMs);
    break;
  case ButtonNotificationType::Dismiss:
    renderDismiss(overlay, notification.startedMs);
    break;
  case ButtonNotificationType::NextEffect:
    renderSwitchSweep(overlay, notification.startedMs, true, CRGB(180, 255, 0));
    break;
  case ButtonNotificationType::PreviousEffect:
    renderSwitchSweep(overlay, notification.startedMs, false, CRGB(180, 255, 0));
    break;
  case ButtonNotificationType::Brightness:
    renderBrightness(overlay, notification.buttonValue, notification.buttonDirection, notification.startedMs);
    break;
  case ButtonNotificationType::PowerOff:
  case ButtonNotificationType::None:
  default:
    break;
  }

  if (notification.buttonPressCount > 0) {
    renderPressEcho(
      overlay,
      notification.buttonPressCount,
      notification.buttonPressMs,
      notification.buttonPressing,
      CRGB::White
    );
  }
}

void ButtonNotificationRenderer::renderPowerOn(NotificationOverlay& overlay, uint32_t startedMs) {
  overlay.clearTop();

  CRGB color = CRGB(120, 255, 120);
  color.nscale8(notificationBreathe(startedMs, 700, 20, 180));
  overlay.drawTopSolid(color);
}

void ButtonNotificationRenderer::renderDismiss(NotificationOverlay& overlay, uint32_t startedMs) {
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

void ButtonNotificationRenderer::renderBrightness(
  NotificationOverlay& overlay,
  uint8_t brightness,
  bool increasing,
  uint32_t startedMs
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

void ButtonNotificationRenderer::renderSwitchSweep(
  NotificationOverlay& overlay,
  uint32_t startedMs,
  bool clockwise,
  const CRGB& color
) {
  overlay.clearTop();

  const uint16_t sweepMs = 650;
  const uint16_t fadeMs = 220;
  const uint32_t elapsed = millis() - startedMs;

  uint8_t lit = WIDTH;
  uint8_t alpha = 255;

  if (elapsed < sweepMs) {
    lit = max<uint8_t>(1, static_cast<uint32_t>(WIDTH) * elapsed / sweepMs);
  } else if (elapsed < sweepMs + fadeMs) {
    lit = WIDTH;
    alpha = map(elapsed, sweepMs, sweepMs + fadeMs, 255, 0);
  } else {
    alpha = 0;
  }

  for (uint8_t i = 0; i < lit; i++) {
    const uint8_t x = clockwise ? i : (WIDTH - 1 - i);

    CRGB pixel = color;
    pixel.nscale8(alpha);

    // голова ярче
    if (i + 1 == lit && elapsed < sweepMs) {
      pixel = color;
      pixel.nscale8(255);
    }

    overlay.drawPixel(x, HEIGHT - 1, pixel);
  }
}

void ButtonNotificationRenderer::renderPressEcho(
  NotificationOverlay& overlay,
  uint8_t count,
  uint32_t pressMs,
  bool pressing,
  const CRGB& color
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
