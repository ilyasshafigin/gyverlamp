#pragma once

#include <stdint.h>

class WifiController {
public:
  static constexpr uint8_t kDeviceIdCapacity = 64;
  static constexpr uint8_t kSsidCapacity = 33;
  static constexpr uint8_t kPasswordCapacity = 65;
  static constexpr uint8_t kMacCapacity = 18;

  struct Ipv4Address {
    uint8_t octets[4]{};
  };

  struct Config {
    const char* deviceId = nullptr;
    const char* staSsid = nullptr;
    const char* staPassword = nullptr;
    const char* apSsid = nullptr;
    const char* apPassword = nullptr;
    Ipv4Address apIp{};
    uint32_t staReconnectIntervalMs = 5000;
    uint32_t apRetryIntervalMs = 5000;
    uint32_t fallbackApIdleTimeoutMs = 5UL * 60UL * 1000UL;
    uint32_t staAttemptTimeoutMs = 60UL * 1000UL;
    uint32_t fallbackApDelayMs = 60UL * 1000UL;
  };

  enum class State : uint8_t {
    Provisioning,
    Connecting,
    Connected,
    RetryWait,
  };

  enum class EventType : uint8_t {
    Connecting,
    Connected,
    Error,
    Disabled,
  };

  struct Event {
    EventType type;
  };

  struct Snapshot {
    char deviceId[kDeviceIdCapacity]{};
    char wifiSsid[kSsidCapacity]{};
    Ipv4Address localIp{};
    Ipv4Address gateway{};
    char mac[kMacCapacity]{};
    int32_t rssi = 0;
    int32_t channel = 0;
  };

  using EventHandler = void (*)(const Event&, void* context);

  WifiController() = default;
  ~WifiController();
  WifiController(const WifiController&) = delete;
  WifiController& operator=(const WifiController&) = delete;
  WifiController(WifiController&&) = delete;
  WifiController& operator=(WifiController&&) = delete;

  bool begin(const Config& config, EventHandler eventHandler, void* eventContext);
  void tick();

  State state() const { return staState_; }
  Snapshot snapshot() const;
  bool staConnected() const;

private:
  enum class ApState : uint8_t {
    Inactive,
    Active,
    RetryWait,
  };

  enum class StaFailureCause : uint8_t {
    DisconnectEvent,
    Deadline,
  };

  struct OwnedConfig {
    char deviceId[kDeviceIdCapacity]{};
    char staSsid[kSsidCapacity]{};
    char staPassword[kPasswordCapacity]{};
    char apSsid[kSsidCapacity]{};
    char apPassword[kPasswordCapacity]{};
    Ipv4Address apIp{};
    uint32_t staReconnectIntervalMs = 0;
    uint32_t apRetryIntervalMs = 0;
    uint32_t fallbackApIdleTimeoutMs = 0;
    uint32_t staAttemptTimeoutMs = 0;
    uint32_t fallbackApDelayMs = 0;
  };

  OwnedConfig config_{};
  EventHandler eventHandler_ = nullptr;
  void* eventContext_ = nullptr;
  State staState_ = State::Provisioning;
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
  EventType pendingEvents_[4]{};
  uint8_t pendingEventCount_ = 0;
  bool deliveringEvents_ = false;

  static bool copyString(char* destination, uint8_t capacity, const char* source);
  static bool isFastFailDisconnectReason(uint16_t reason);
  bool copyConfig(const Config& runtimeConfig);
  void processPlatformEvents();
  void emit(EventType type);
  void deliverEvents();
  bool startAp();
  void requestAp();
  void stopAp();
  void startStaConnection();
  void stopStaConnection();
  void startStaCampaign();
  void endStaCampaign();
  void checkFallbackAp();
  void failStaConnection(StaFailureCause cause, uint16_t reason);
  void checkStaConnecting();
  void checkStaRetryWait();
  void onStaConnected();
  void checkApRetry();
  void checkApTimeout();
};
