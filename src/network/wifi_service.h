#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>

class EepromStore;
class NotificationController;

class WifiService {
public:
  explicit WifiService(EepromStore& eeprom, NotificationController& notifications)
    : _eeprom(eeprom),
      _notifications(notifications),
      _deviceId(DEVICE_NAME) {}

  void init();
  void tick();

  bool isStaConnected() const { return WiFi.isConnected(); }
  const String& getDeviceId() const { return _deviceId; }

private:
  enum class StaState : uint8_t {
    Provisioning,
    Connecting,
    Connected,
    RetryWait,
  };

  enum class ApState : uint8_t {
    Inactive,
    Active,
    RetryWait,
  };

  EepromStore& _eeprom;
  NotificationController& _notifications;
  String _deviceId;

  StaState _staState = StaState::Provisioning;
  ApState _apState = ApState::Inactive;
  uint32_t _apStartedAt = 0;
  uint32_t _apRetryStartedAt = 0;
  uint32_t _connectStartedAt = 0;
  uint32_t _retryStartedAt = 0;
  static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
  static constexpr uint32_t AP_RETRY_INTERVAL_MS = 5000;
  static constexpr uint32_t AP_TIMEOUT_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 10000;

  bool startAp();
  void requestAp();
  void stopAp();
  void startStaConnection();
  void checkStaConnecting();
  void checkStaRetryWait();
  void onStaConnected();
  void checkApRetry();
  void checkApTimeout();
};
