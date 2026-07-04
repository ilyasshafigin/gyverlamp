#include "system_renderer.h"
#include "render_utils.h"
#include "../config.h"

void SystemNotificationRenderer::renderWifi(NotificationOverlay& overlay, ConnectionState state, uint32_t startedMs) {
  if (state == ConnectionState::Connecting) {
    // WiFi подключается — синий мягкий пульс
    overlay.clearTop();
    overlay.drawTopPulse(CRGB::Blue, startedMs, 1800, 8, 130);
    return;
  }

  if (state == ConnectionState::Connected) {
    // WiFi подключен — зеленый мягкий пульс
    overlay.clearTop();
    overlay.drawTopPulse(CRGB::Green, startedMs, 1200, 10, 120);
    return;
  }

  if (state == ConnectionState::Error) {
    // WiFi ошибка — оранжевый медленный пульс
    overlay.clearTop();
    overlay.drawTopPulse(CRGB(255, 80, 0), startedMs, 2600, 5, 150);
    return;
  }
}

void SystemNotificationRenderer::renderMqtt(NotificationOverlay& overlay, ConnectionState state, uint32_t startedMs) {
  if (state == ConnectionState::Connecting) {
    // MQTT подключается — фиолетовый пульс
    overlay.clearTop();
    overlay.drawTopPulse(CRGB(130, 0, 255), startedMs, 1800, 8, 130);
    return;
  }

  if (state == ConnectionState::Connected) {
    // MQTT подключен — бирюзовый пульс
    overlay.clearTop();
    overlay.drawTopPulse(CRGB(0, 180, 90), startedMs, 1200, 10, 120);
    return;
  }

  if (state == ConnectionState::Error) {
    // MQTT ошибка — янтарный редкий пульс
    overlay.clearTop();
    overlay.drawTopPulse(CRGB(255, 160, 0), startedMs, 3000, 5, 130);
    return;
  }
}

void SystemNotificationRenderer::renderOta(
  NotificationOverlay& overlay, OtaState state, uint8_t percent, uint32_t startedMs
) {
  if (state == OtaState::Running) {
    // OTA — бирюзовый прогресс по верхнему кольцу.
    // Если прогресс почему-то 0, всё равно видно вращение.
    overlay.clearTop();
    if (percent > 0) {
      overlay.drawTopProgress(CRGB(0, 180, 255), startedMs, percent);
    } else {
      overlay.drawTopSpinner(CRGB(0, 180, 255), startedMs);
    }
    return;
  }

  if (state == OtaState::Success) {
    overlay.clearTop();
    overlay.drawTopPulse(CRGB::Green, startedMs, 900, 20, 180);
    return;
  }

  if (state == OtaState::Error) {
    overlay.clearTop();
    overlay.drawTopDoublePulse(CRGB::Red, startedMs, 1800);
    return;
  }
}
