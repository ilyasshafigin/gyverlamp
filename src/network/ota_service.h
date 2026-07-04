#pragma once

class FrameRenderer;
class NotificationController;

class OtaService {
public:
  explicit OtaService(FrameRenderer& frameRenderer, NotificationController& notifications)
#ifdef USE_OTA
    : _frameRenderer(frameRenderer),
      _notifications(notifications) {
  }
#else
  {
    (void)frameRenderer;
    (void)notifications;
  }
#endif

  void init();
  void tick();

private:
#ifdef USE_OTA
  FrameRenderer& _frameRenderer;
  NotificationController& _notifications;
#endif
};
