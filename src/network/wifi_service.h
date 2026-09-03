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

  enum class StaFailureCause : uint8_t {
    DisconnectEvent,
    Deadline,
  };

  static constexpr uint32_t kReconnectIntervalMs = 5000;
  static constexpr uint32_t kApRetryIntervalMs = 5000;
  static constexpr uint32_t kApTimeoutMs = 5UL * 60UL * 1000UL;
  static constexpr uint32_t kStaAttemptTimeoutMs = 60UL * 1000UL;
  static constexpr uint32_t kFallbackApDelayMs = 60UL * 1000UL;

  EepromStore& eeprom_;
  String deviceId_;

  VoidCallback connectingHandler_;
  VoidCallback connectedHandler_;
  VoidCallback errorHandler_;
  VoidCallback disabledHandler_;

  WiFiEventHandler stationConnectedEventHandler_;
  WiFiEventHandler stationDisconnectedEventHandler_;
  WiFiEventHandler stationGotIpEventHandler_;
  WiFiEventHandler stationDhcpTimeoutEventHandler_;
  bool wifiEventsRegistered_ = false;

  StaState staState_ = StaState::Provisioning;
  ApState apState_ = ApState::Inactive;
  uint32_t apStartedAt_ = 0;
  uint32_t apRetryStartedAt_ = 0;
  uint32_t connectStartedAt_ = 0;
  uint32_t retryStartedAt_ = 0;
  uint32_t nextAttemptId_ = 0;
  uint32_t activeAttemptId_ = 0;
  uint32_t pendingDisconnectAttemptId_ = 0;
  uint16_t pendingDisconnectReason_ = 0;
  bool acceptingStaDisconnectEvents_ = false;
  bool pendingDisconnectValid_ = false;
  bool hasStaCredentials_ = false;
  bool staCampaignActive_ = false;
  bool fallbackApRequested_ = false;
  uint32_t staCampaignStartedAt_ = 0;

  bool startAp();
  void requestAp();
  void stopAp();
  void startStaConnection();
  void stopStaConnection();
  void startStaCampaign();
  void endStaCampaign();
  void checkFallbackAp();
  void failStaConnection(StaFailureCause cause, wl_status_t status, uint16_t reason);
  void checkStaConnecting();
  void checkStaRetryWait();
  void onStaConnected();
  void checkApRetry();
  void checkApTimeout();
};
