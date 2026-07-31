#pragma once

class FrameRenderer;
class NotificationController;
class WifiService;

class OtaService {
public:
  explicit OtaService(FrameRenderer& frameRenderer, NotificationController& notifications, WifiService& wifi)
#ifdef USE_OTA
    : _frameRenderer(frameRenderer),
      _notifications(notifications),
      _wifi(wifi) {
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
  FrameRenderer& _frameRenderer;
  NotificationController& _notifications;
  WifiService& _wifi;
#endif
};
