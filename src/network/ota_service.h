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
  FrameRenderer& frameRenderer_;
  NotificationController& notifications_;
  WifiService& wifi_;
  bool begun_ = false;
#endif
};
