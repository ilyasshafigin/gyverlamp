#include "mqtt_service.h"

#ifdef USE_MQTT

MqttService::MqttService(
  AudioService& audio,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button,
  WifiService& wifi
)
  : bridge_(audio, effects, notifications, power, rotation, settings, button),
    runtime_(bridge_, wifi) {
}

void MqttService::init(const MqttConfig& config) {
  bridge_.activateCallbackTarget();
  bridge_.initialize();
  runtime_.init(config, bridge_.clientId());
}

void MqttService::tick() {
  bridge_.tickTimers();
  runtime_.tick();
}

void MqttService::updateStates() {
  bridge_.fullRefresh();
}

void MqttService::requestApply(const MqttConfig& config) {
  runtime_.requestApply(config);
}

void MqttService::requestEnabled(bool enabled) {
  runtime_.requestEnabled(enabled);
}

void MqttService::requestRestart() {
  runtime_.requestRestart();
}

MqttService::State MqttService::state() const {
  return runtime_.state();
}

bool MqttService::isEnabled() const {
  return runtime_.isEnabled();
}

#else

MqttService::MqttService(
  AudioService& audio,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button,
  WifiService& wifi
) {
  (void)audio;
  (void)effects;
  (void)notifications;
  (void)power;
  (void)rotation;
  (void)settings;
  (void)button;
  (void)wifi;
}
void MqttService::init(const MqttConfig&) {
}
void MqttService::tick() {
}
void MqttService::updateStates() {
}
void MqttService::requestApply(const MqttConfig&) {
}
void MqttService::requestEnabled(bool) {
}
void MqttService::requestRestart() {
}
MqttService::State MqttService::state() const {
  return State::Disabled;
}

bool MqttService::isEnabled() const {
  return false;
}
#endif

const char* MqttService::stateName() const {
  switch (state()) {
    case State::Disabled: return "Disabled";
    case State::WaitingForWifi: return "Waiting for WiFi";
    case State::DisconnectBarrier: return "Disconnecting";
    case State::RetryWait: return "Retry waiting";
    case State::ConnectPrepare: return "Preparing connection";
    case State::Connecting: return "Connecting";
    case State::Online: return "Online";
    case State::ConfigError: return "Configuration error";
  }
  return "Unknown";
}
