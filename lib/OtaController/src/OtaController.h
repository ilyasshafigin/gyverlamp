#pragma once

#include <stdint.h>

class OtaController {
public:
  // Single-instance process-lifetime controller. ArduinoOTA retains callbacks;
  // begin() configures them once and no asynchronous teardown is provided.
  struct Config {
    const char* hostname;
    uint16_t port;
    bool enabled;
    const char* password;
    const char* passwordHash;
  };

  enum class EventType : uint8_t {
    Start,
    Progress,
    End,
    Error,
  };

  struct Event {
    EventType type;
    uint8_t progress;
    uint8_t errorCode;
  };

  using EventHandler = void (*)(const Event&, void* context);

  enum class State : uint8_t {
    Disabled,
    WaitingForSta,
    ListenerStartIssued,
    Updating,
  };

  OtaController() = default;
  OtaController(const OtaController&) = delete;
  OtaController& operator=(const OtaController&) = delete;
  OtaController(OtaController&&) = delete;
  OtaController& operator=(OtaController&&) = delete;

  // passwordHash takes precedence when both password fields are non-empty.
  void begin(const Config& config, EventHandler eventHandler, void* context);
  void tick(bool staConnected);
  void requestEnabled(bool enabled);
  void requestRestart();

  bool isEnabled() const;
  State state() const;
  const char* stateName() const;

private:
  static constexpr uint8_t kHostnameCapacity = 64;
  static constexpr uint8_t kPasswordCapacity = 65;
  static constexpr unsigned long kBeginRetryIntervalMs = 5000;
  static constexpr unsigned long kProgressIntervalMs = 150;

  char hostname_[kHostnameCapacity]{};
  char password_[kPasswordCapacity]{};
  char passwordHash_[kPasswordCapacity]{};
  uint16_t port_ = 8266;
  bool initialized_ = false;
  bool desiredEnabled_ = false;
  bool effectiveEnabled_ = false;
  bool listenerStartIssued_ = false;
  bool updating_ = false;
  bool enableRequestPending_ = false;
  bool restartRequested_ = false;
  unsigned long lastBeginAttemptAt_ = 0;
  unsigned long lastProgressCallbackAt_ = 0;
  unsigned long uploadStartedAt_ = 0;
  uint8_t lastProgressPercent_ = 0xFF;
  uint8_t finalProgressPercent_ = 0;
  EventHandler eventHandler_ = nullptr;
  void* eventContext_ = nullptr;

  static void copyString(char* destination, uint8_t capacity, const char* source);
  void stopListener();
  void emit(EventType type, uint8_t progress = 0, uint8_t errorCode = 0);
};
