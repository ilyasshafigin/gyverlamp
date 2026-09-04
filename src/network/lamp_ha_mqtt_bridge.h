#pragma once

#ifdef USE_MQTT
#include <Arduino.h>
#include <HaMqttEntities.h>
#include <PubSubClient.h>

#include "ha_light.h"
#include "ha_time.h"
#include "mqtt_bridge.h"
#include "mqtt_state.h"
#include "../util/timer.h"

class AudioService;
class EffectController;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class TouchButton;

class LampHaMqttBridge : public MqttBridge {
public:
  LampHaMqttBridge(
    AudioService& audio,
    EffectController& effects,
    NotificationController& notifications,
    PowerController& power,
    RotationController& rotation,
    SettingsRepository& settings,
    TouchButton& button
  );

  void initialize();
  void activateCallbackTarget();
  uint8_t entityCount() const override;
  void registerEntities() override;
  void tickTimers();
  void fullRefresh() override;
  void
  dispatchMessage(PubSubClient& client, HAEntity* entity, char* topic, byte* payload, unsigned int length) override;
  void onTransportState(MqttState state) override;
  void onTransportFailure() override;

  const char* clientId() const { return clientId_.c_str(); }
  void onLightCommand(bool on, uint8_t brightness);
  void onEffectCommand(const char* effectName);
  void onColorCommand(uint8_t r, uint8_t g, uint8_t b);

private:
  static constexpr unsigned long kTelemetryIntervalMs = 60000;
  static constexpr unsigned long kStateRefreshIntervalMs = 30000;

  static LampHaMqttBridge* callbackTarget_;

  AudioService& audio_;
  EffectController& effects_;
  NotificationController& notifications_;
  PowerController& power_;
  RotationController& rotation_;
  SettingsRepository& settings_;
  TouchButton& button_;

  Timer telemetryTimer_{kTelemetryIntervalMs};
  Timer stateRefreshTimer_{kStateRefreshIntervalMs};

  String clientId_;
  String haEffectList_;

  HADevice haDevice_;
  HALight haLight_;
  HASwitch haRotationSwitch_;
  HASelect haRotationInterval_;
  HASwitch haButtonSwitch_;
  HANumber haEffectScale_;
  HANumber haEffectSpeed_;
  HANumber haEffectBrightness_;
  HANumber haAutoOff_;
  HASensorNumeric haAutoOffRemaining_;
  HASelect haPalette_;
  HASelect haUserNotification_;
  HANumber haUserNotificationDuration_;
  HASensorNumeric haUserNotificationRemaining_;
  HAButton haUserNotify_;
  HAButton haNextEffect_;
  HAButton haPrevEffect_;
  HAButton haRandomEffect_;
  HAButton haResetAllEffectSettings_;
  HAButton haResetCurrentEffectSettings_;
  HAText haUserNotificationText_;
  HASwitch haNotificationQuietHours_;
  HATime haNotificationQuietStart_;
  HATime haNotificationQuietEnd_;
  HASensorText haNotificationMuteState_;
  HASelect haAudioMode_;
  HASelect haAudioBand_;
  HANumber haAudioAmount_;
  HASensorText haAudioAvailable_;
  HASensorNumeric haUptime_;
  HASensorNumeric haRssi_;
  HASensorNumeric haRssiPct_;
  HASensorNumeric haChannel_;
  HASensorNumeric haVcc_;
  HASensorText haResetReason_;

  void telemetryTimerCallback();
  void stateRefreshTimerCallback();
  void syncLightState();
  void syncSelectedEffectState();
  void syncQuietHoursState();
  void syncUserNotificationState();
  void onPaletteCommand(const char* paletteName);
};
#endif
