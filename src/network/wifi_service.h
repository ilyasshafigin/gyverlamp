#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>

class EepromStore;
class FrameRenderer;
class NotificationController;
class SettingsRepository;

class WifiService {
public:
  explicit WifiService(
    EepromStore& eeprom,
    FrameRenderer& frameRenderer,
    NotificationController& notifications,
    SettingsRepository& settings
  )
    : _eeprom(eeprom),
      _frameRenderer(frameRenderer),
      _notifications(notifications),
      _settings(settings),
      _deviceId(DEVICE_NAME) {}

  void init();
  void tick();

  bool isStaConnected() const { return WiFi.isConnected(); }
  const String& getDeviceId() const { return _deviceId; }

private:
  EepromStore& _eeprom;
  FrameRenderer& _frameRenderer;
  NotificationController& _notifications;
  SettingsRepository& _settings;
  String _deviceId;

  bool _staWasConnected = false;
  bool _apActive = false;
  uint32_t _apStartedAt = 0;
  uint32_t _reconnectTimer = 0;
  static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
  static constexpr uint32_t AP_TIMEOUT_MS = 5UL * 60UL * 1000UL;

  void startAp();
  void stopAp();
  bool initSta();
  void checkStaReconnect();
  void checkApTimeout();
  void resetSavedWifiIfNeeded();
};
