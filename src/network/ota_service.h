#pragma once

#ifdef USE_OTA
#include <stdint.h>
#include <functional>
#endif

class EepromStore;

class OtaService {
public:
#ifdef USE_OTA
  enum class State : uint8_t {
    Disabled,
    WaitingForSta,
    Listening,
    Updating,
  };

  explicit OtaService(EepromStore& eeprom)
    : eeprom_(eeprom) {}
#else
  explicit OtaService(EepromStore&) {}
#endif

  void init(const char* hostname);
  void tick(bool isStaConnected);

#ifdef USE_OTA
  using VoidCallback = std::function<void()>;
  using ProgressCallback = std::function<void(uint8_t)>;

  void setStartHandler(VoidCallback callback) { startCallback_ = callback; }
  void setProgressHandler(ProgressCallback callback) { progressCallback_ = callback; }
  void setEndHandler(VoidCallback callback) { endCallback_ = callback; }
  void setErrorHandler(VoidCallback callback) { errorCallback_ = callback; }

  bool isEnabled() const { return enabled_; }
  State state() const;
  const char* stateName() const;

  void requestEnabled(bool enabled);
  void requestRestart();

private:
  EepromStore& eeprom_;
  bool enabled_ = false;

  static constexpr unsigned long kBeginRetryIntervalMs = 5000;
  static constexpr unsigned long kProgressIntervalMs = 150;

  bool beginAttempted_ = false;
  bool listenerActive_ = false;
  bool updating_ = false;
  bool enableRequestPending_ = false;
  bool requestedEnabled_ = false;
  bool restartRequested_ = false;
  unsigned long lastBeginAttemptAt_ = 0;
  unsigned long lastProgressCallbackAt_ = 0;
  uint8_t lastProgressPercent_ = 0xFF;

  void stopListener();

  VoidCallback startCallback_;
  ProgressCallback progressCallback_;
  VoidCallback endCallback_;
  VoidCallback errorCallback_;
#endif
};
