#pragma once

#include <stddef.h>
#include <stdint.h>

class HAMQTTController;
class PubSubClient;

class MqttController {
public:
  enum class State : uint8_t {
    Disabled,
    WaitingForWifi,
    DisconnectBarrier,
    RetryWait,
    ConnectPrepare,
    Connecting,
    Online,
    ConfigError,
  };

  enum class EventType : uint8_t {
    StateChanged,
    BecameOnline,
    TransportFailure,
  };

  struct Event {
    EventType type;
    State state;
  };

  struct Config {
    const char* host = nullptr;
    const char* clientId = nullptr;
    const char* user = nullptr;
    const char* password = nullptr;
    uint16_t port = 0;
  };

  struct LinkHooks {
    void* context = nullptr;
    bool (*staConnected)(void* context) = nullptr;
    void (*abortTransport)(void* context) = nullptr;
    uint32_t (*now)(void* context) = nullptr;
  };

  using EventHandler = void (*)(void*, const Event&);

  MqttController(HAMQTTController& controller, PubSubClient& client, LinkHooks hooks);
  ~MqttController() = default;
  MqttController(const MqttController&) = delete;
  MqttController& operator=(const MqttController&) = delete;
  MqttController(MqttController&&) = delete;
  MqttController& operator=(MqttController&&) = delete;

  void begin(const Config& config, bool prerequisitesReady);
  void tick(uint32_t now);
  void requestApply(const Config& config);
  void requestEnabled(bool enabled);
  void requestRestart();
  void consumeEvents(EventHandler handler, void* context);

  State state() const { return state_; }
  bool isConnected() const { return state_ == State::Online; }
  bool isEnabled() const { return requestedEnabled_; }

private:
  static constexpr uint16_t kReconnectBaseMs = 5000;
  static constexpr uint16_t kReconnectMaxMs = 60000;

  struct OwnedConfig {
    char host[256]{};
    char clientId[128]{};
    char user[128]{};
    char password[128]{};
    uint16_t port = 0;
  };

  enum class BarrierPhase : uint8_t { AwaitPulse, Complete };

  HAMQTTController& controller_;
  PubSubClient& client_;
  LinkHooks hooks_;
  State state_ = State::Disabled;
  OwnedConfig requestedConfig_{};
  OwnedConfig activeConfig_{};
  bool initialized_ = false;
  bool prerequisitesReady_ = false;
  bool requestedConfigValid_ = false;
  bool requestedEnabled_ = false;
  bool initialPending_ = false;
  bool stateReported_ = false;
  bool retryPending_ = false;
  bool resetRetryAfterBarrier_ = false;
  bool reconnectImmediately_ = false;
  BarrierPhase barrierPhase_ = BarrierPhase::AwaitPulse;
  uint32_t requestedGeneration_ = 0;
  uint32_t handledGeneration_ = 0;
  uint32_t attemptGeneration_ = 0;
  uint32_t reconnectAt_ = 0;
  uint32_t reconnectTimeout_ = kReconnectBaseMs;
  uint8_t pendingEvents_ = 0;

  static bool copyString(char* destination, size_t capacity, const char* source);
  static bool copyConfig(OwnedConfig& destination, const Config& source);
  bool hooksReady() const;
  bool isConfigUsable() const;
  bool applyRequestedConfig();
  void setState(State state);
  void emitEvent(EventType event);
  void startDisconnectBarrier(bool abortTransport);
  void completeDisconnectBarrier(uint32_t now);
  bool handleRequestedCommands(uint32_t now);
  void enterEnabledLifecycle(uint32_t now);
  void connectBlocking();
  void resetReconnectBackoff();
  void registerReconnectFailure(uint32_t now);
  bool reconnectDue(uint32_t now) const;
};
