#pragma once

#include <stdint.h>

#include <functional>

class OtaService {
public:
  explicit OtaService() {}

  using VoidCallback = std::function<void()>;
  using ProgressCallback = std::function<void(uint8_t)>;

  void setStartHandler(VoidCallback callback) { startCallback_ = callback; }
  void setProgressHandler(ProgressCallback callback) { progressCallback_ = callback; }
  void setEndHandler(VoidCallback callback) { endCallback_ = callback; }
  void setErrorHandler(VoidCallback callback) { errorCallback_ = callback; }

  void init(const char* hostname);
  void tick(bool isStaConnected);

private:
#ifdef USE_OTA
  static constexpr unsigned long kBeginRetryIntervalMs = 5000;

  bool beginAttempted_ = false;
  unsigned long lastBeginAttemptAt_ = 0;
#endif

  VoidCallback startCallback_;
  ProgressCallback progressCallback_;
  VoidCallback endCallback_;
  VoidCallback errorCallback_;
};
