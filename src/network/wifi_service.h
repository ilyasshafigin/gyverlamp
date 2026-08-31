#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <functional>

class EepromStore;

class WifiService {
public:
  explicit WifiService(EepromStore& eeprom)
    : eeprom_(eeprom),
      deviceId_(DEVICE_NAME) {}

  using VoidCallback = std::function<void()>;

  void setConnectingHandler(VoidCallback callback) { connectingHandler_ = callback; }
  void setConnectedHandler(VoidCallback callback) { connectedHandler_ = callback; }
  void setErrorHandler(VoidCallback callback) { errorHandler_ = callback; }
  void setDisabledHandler(VoidCallback callback) { disabledHandler_ = callback; }

  void init();
  void tick();

  bool isStaConnected() const { return WiFi.isConnected(); }
  const String& getDeviceId() const { return deviceId_; }

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

  static constexpr uint32_t kReconnectIntervalMs = 5000;
  static constexpr uint32_t kApRetryIntervalMs = 5000;
  static constexpr uint32_t kApTimeoutMs = 5UL * 60UL * 1000UL;
  static constexpr uint32_t kStaConnectTimeoutMs = 10000;

  EepromStore& eeprom_;
  String deviceId_;

  VoidCallback connectingHandler_;
  VoidCallback connectedHandler_;
  VoidCallback errorHandler_;
  VoidCallback disabledHandler_;

  StaState staState_ = StaState::Provisioning;
  ApState apState_ = ApState::Inactive;
  uint32_t apStartedAt_ = 0;
  uint32_t apRetryStartedAt_ = 0;
  uint32_t connectStartedAt_ = 0;
  uint32_t retryStartedAt_ = 0;

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
