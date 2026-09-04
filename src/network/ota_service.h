#pragma once

#include <stdint.h>
#include <functional>

class OtaService {
public:
  enum class State : uint8_t {
    Disabled,
    WaitingForSta,
    Listening,
    Updating,
  };

  void init(const char* hostname, bool enabled);
  void tick(bool isStaConnected);
  bool isEnabled() const;
  const char* stateName() const;
  void requestEnabled(bool enabled);
  void requestRestart();

  using VoidCallback = std::function<void()>;
  using ProgressCallback = std::function<void(uint8_t)>;

  void setStartHandler(VoidCallback callback);
  void setProgressHandler(ProgressCallback callback);
  void setEndHandler(VoidCallback callback);
  void setErrorHandler(VoidCallback callback);

  State state() const;

private:
#ifdef USE_OTA
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
