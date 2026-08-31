#pragma once

class FrameRenderer;
class NotificationController;
class WifiService;

class OtaService {
public:
  explicit OtaService(FrameRenderer& frameRenderer, NotificationController& notifications, WifiService& wifi)
#ifdef USE_OTA
    : frameRenderer_(frameRenderer),
      notifications_(notifications),
      wifi_(wifi) {
  }
#else
  {
    (void)frameRenderer;
    (void)notifications;
    (void)wifi;
  }
#endif

  void init();
  void tick();

private:
#ifdef USE_OTA
  static constexpr unsigned long BEGIN_RETRY_INTERVAL_MS = 5000;

  FrameRenderer& frameRenderer_;
  NotificationController& notifications_;
  WifiService& wifi_;

  bool beginAttempted_ = false;
  unsigned long lastBeginAttemptAt_ = 0;
#endif
};
