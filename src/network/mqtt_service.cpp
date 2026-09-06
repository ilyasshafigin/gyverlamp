#include "mqtt_service.h"

#ifdef USE_MQTT
#include <ESP8266WiFi.h>
#include <WifiController.h>
#include <cctype>
#include <uptime_formatter.h>

#include "../audio/audio_service.h"
#include "../core/auto_off_config.h"
#include "../core/power_controller.h"
#include "../core/rotation_controller.h"
#include "../core/rotation_presets.h"
#include "../effect/catalog.h"
#include "../effect/controller.h"
#include "../effect/palette_catalog.h"
#include "../hardware/button.h"
#include "../notification/controller.h"
#include "../notification/quiet_hours.h"
#include "../storage/settings_repository.h"
#include "../util/loop_profiler.h"

namespace {
  const char* kUserNotificationOff = "Off";
  const char* kUserNotificationWarning = "Warning";
  const char* kUserNotificationAlarm = "Alarm";
  const char* kUserNotificationText = "Text";
  const char* kUserNotificationOptions[] = {
    kUserNotificationOff, kUserNotificationText, kUserNotificationWarning, kUserNotificationAlarm
  };
  constexpr uint8_t kUserNotificationOptionsCount =
    sizeof(kUserNotificationOptions) / sizeof(kUserNotificationOptions[0]);
  const char* kAudioModeOptions[] = {"Off", "Brightness", "Speed", "Scale", "Effect"};
  const char* kAudioBandOptions[] = {"Level", "Bass", "Treble"};
  const char* audioModeName(AudioMode mode) {
    return kAudioModeOptions[static_cast<uint8_t>(mode)];
  }
  AudioMode parseAudioMode(const char* value) {
    for (uint8_t i = 0; i <= static_cast<uint8_t>(AudioMode::Effect); i++)
      if (strcmp(value, kAudioModeOptions[i]) == 0) return static_cast<AudioMode>(i);
    return AudioMode::Off;
  }
  const char* audioBandName(AudioBand band) {
    return kAudioBandOptions[static_cast<uint8_t>(band)];
  }
  AudioBand parseAudioBand(const char* value) {
    for (uint8_t i = 0; i <= static_cast<uint8_t>(AudioBand::Treble); i++)
      if (strcmp(value, kAudioBandOptions[i]) == 0) return static_cast<AudioBand>(i);
    return AudioBand::Level;
  }
  bool parseHaTime(const char* value, uint16_t& minutes) {
    if (value == nullptr) return false;
    int h = -1, m = -1, s = 0;
    const char* p = value;
    if (!isdigit((unsigned char)*p)) return false;
    h = *p++ - '0';
    if (isdigit((unsigned char)*p)) h = h * 10 + (*p++ - '0');
    if (*p++ != ':' || !isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return false;
    m = (p[0] - '0') * 10 + p[1] - '0';
    p += 2;
    if (*p == ':') {
      p++;
      if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return false;
      s = (p[0] - '0') * 10 + p[1] - '0';
      p += 2;
    }
    if (*p || h > 23 || m > 59 || s > 59) return false;
    minutes = static_cast<uint16_t>(h * 60 + m);
    return true;
  }
  void formatHaTime(uint16_t minutes, char* buffer) {
    snprintf(buffer, 6, "%02u:%02u", (minutes % 1440) / 60, minutes % 60);
  }
} // namespace

MqttService::MqttService(
  AudioService& audio,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button,
  WifiController& wifi
)
  : audio_(audio),
    effects_(effects),
    notifications_(notifications),
    power_(power),
    rotation_(rotation),
    settings_(settings),
    button_(button),
    wifi_(wifi),
    clientId_("GyverLamp-" + String(ESP.getChipId(), HEX)),
    haDevice_(clientId_.c_str(), DEVICE_NAME, FIRMWARE_VERSION, FIRMWARE_MANUFACTURER, "Gyver Lamp"),
    haLight_("_light", "Gyver Lamp", haDevice_),
    haRotationSwitch_("_rotation", "Rotation", haDevice_),
    haRotationInterval_("_rotation_interval", "Rotation Interval", haDevice_, kRotationPresetCount),
    haButtonSwitch_("_button", "Touch Button", haDevice_),
    haEffectScale_("_effect_scale", "Effect Scale", haDevice_, 1, 255, 1),
    haEffectSpeed_("_effect_speed", "Effect Speed", haDevice_, 1, 255, 1),
    haEffectBrightness_("_effect_brightness", "Effect brightness", haDevice_, 0, 255, 1),
    haAutoOff_("_auto_off_minutes", "Auto Off Minutes", haDevice_, kAutoOffMinutesMin, kAutoOffMinutesMax, 1),
    haAutoOffRemaining_("_auto_off_remaining", "Auto Off Remaining", haDevice_, "s", 0),
    haPalette_("_palette", "Palette", haDevice_, Palettes::kCount),
    haUserNotification_(
      "_user_notification", "User Notification", haDevice_, kUserNotificationOptionsCount, kUserNotificationOptions
    ),
    haUserNotificationDuration_("_user_notification_duration", "User Notification Duration", haDevice_, 0, 3600, 1),
    haUserNotificationRemaining_("_user_notification_remaining", "User Notification Remaining", haDevice_, "s", 0),
    haUserNotify_("_user_notify", "User Notify", haDevice_),
    haNextEffect_("_next_effect", "Next Effect", haDevice_),
    haPrevEffect_("_prev_effect", "Previous Effect", haDevice_),
    haRandomEffect_("_random_effect", "Random Effect", haDevice_),
    haResetAllEffectSettings_("_reset_all_effect_settings", "Reset all effect settings", haDevice_),
    haResetCurrentEffectSettings_("_reset_current_effect_settings", "Reset current effect settings", haDevice_),
    haUserNotificationText_("_user_notification_text", "User Notification Text", haDevice_, 64),
    haNotificationQuietHours_("_notification_quiet_hours", "Notification Quiet Hours", haDevice_),
    haNotificationQuietStart_("_notification_quiet_start", "Notification Quiet Start", haDevice_),
    haNotificationQuietEnd_("_notification_quiet_end", "Notification Quiet End", haDevice_),
    haNotificationMuteState_("_notification_mute_state", "Notification Mute State", haDevice_, 16),
    haAudioMode_("_audio_mode", "Audio Mode", haDevice_, 5, kAudioModeOptions),
    haAudioBand_("_audio_band", "Audio Band", haDevice_, 3, kAudioBandOptions),
    haAudioAmount_("_audio_amount", "Audio Amount", haDevice_, 0, 255, 1),
    haAudioAvailable_("_audio_available", "Audio Available", haDevice_, 8),
    haUptime_("_uptime", "Uptime", haDevice_, "s", 0),
    haRssi_("_rssi", "RSSI", haDevice_, "dBm", 0),
    haRssiPct_("_rssi_pct", "RSSI %", haDevice_, "%", 0),
    haChannel_("_channel", "WiFi Channel", haDevice_, nullptr, 0),
    haVcc_("_vcc", "VCC", haDevice_, "V", 3),
    haResetReason_("_reset_reason", "Reset Reason", haDevice_, 64),
    mqttController_(
      controller_, client_, MqttController::LinkHooks{this, staConnectedForward, abortTransportForward, nowForward}
    ) {
}

void MqttService::init(const MqttConfig& config) {
  wifiClient_.setTimeout(2000);
  client_.setSocketTimeout(2);
  initializeAdapter();
  const bool controllerReady =
    controller_.begin(client_, entityRegistry_, sizeof(entityRegistry_) / sizeof(entityRegistry_[0]));
  const bool registered = controllerReady && registerEntities() && controller_.registrationComplete();
  if (registered) controller_.setCallback(this, haCallbackForward);
  mqttController_.begin(controllerConfig(config), registered);
}

void MqttService::tick() {
  tickTimers();
  {
    LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this] { mqttController_.tick(millis()); });
  }
  consumeControllerEvents();
}

MqttController::Config MqttService::controllerConfig(const MqttConfig& config) const {
  return {
    strcmp(config.host, "none") == 0 ? "" : config.host,
    clientId_.c_str(),
    config.user,
    config.password,
    config.port,
  };
}

bool MqttService::staConnectedForward(void* context) {
  return static_cast<MqttService*>(context)->wifi_.staConnected();
}

void MqttService::abortTransportForward(void* context) {
  static_cast<MqttService*>(context)->wifiClient_.abort();
}

uint32_t MqttService::nowForward(void*) {
  return millis();
}

void MqttService::mqttEventForward(void* context, const MqttController::Event& event) {
  MqttService* service = static_cast<MqttService*>(context);
  switch (event.type) {
    case MqttController::EventType::StateChanged: service->onTransportState(event.state); break;
    case MqttController::EventType::BecameOnline: service->fullRefresh(); break;
    case MqttController::EventType::TransportFailure: service->onTransportFailure(); break;
  }
}

void MqttService::haCallbackForward(void* context, HAEntity& entity, char* topic, byte* payload, size_t length) {
  static_cast<MqttService*>(context)->handleHaCommand(entity, topic, payload, length);
}

void MqttService::handleHaCommand(HAEntity& entity, char* topic, byte* payload, size_t length) {
  (void)payload;
  (void)length;
  if (&entity == &haLight_) {
    dispatchLightCommand(topic);
  } else if (&entity == &haRotationSwitch_) {
    bool old = rotation_.isActive();
    rotation_.setEnabled(haRotationSwitch_.getState());
    if (rotation_.isActive() && !old) notifications_.onRotationEnabled();
    else if (!rotation_.isActive() && old)
      notifications_.onRotationDisabled();
    haRotationSwitch_.setState(rotation_.isActive());
  } else if (&entity == &haRotationInterval_) {
    rotation_.setIntervalSec(
      rotationPresetSecondsForIndex(rotationPresetIndexForLabel(haRotationInterval_.getState()))
    );
    haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));
  } else if (&entity == &haButtonSwitch_) {
    button_.setEnabled(haButtonSwitch_.getState());
    haButtonSwitch_.setState(button_.isEnabled());
  } else if (&entity == &haEffectScale_) {
    effects_.setEffectScale(haEffectScale_.getState());
    haEffectScale_.setState(settings_.getEffectSettings(effects_.getSelectedEffectId()).scale);
  } else if (&entity == &haEffectSpeed_) {
    effects_.setEffectSpeed(haEffectSpeed_.getState());
    haEffectSpeed_.setState(settings_.getEffectSettings(effects_.getSelectedEffectId()).speed);
  } else if (&entity == &haEffectBrightness_) {
    effects_.setEffectBrightness(haEffectBrightness_.getState());
    haEffectBrightness_.setState(settings_.getEffectSettings(effects_.getSelectedEffectId()).brightness);
  } else if (&entity == &haPalette_)
    onPaletteCommand(haPalette_.getState());
  else if (&entity == &haAutoOff_) {
    power_.setAutoOffMinutes(haAutoOff_.getState());
    haAutoOff_.setState(power_.getAutoOffMinutes());
  } else if (&entity == &haAudioMode_) {
    audio_.setMode(parseAudioMode(haAudioMode_.getState()));
    haAudioMode_.setState(audioModeName(audio_.config().mode));
  } else if (&entity == &haAudioBand_) {
    audio_.setBand(parseAudioBand(haAudioBand_.getState()));
    haAudioBand_.setState(audioBandName(audio_.config().band));
  } else if (&entity == &haAudioAmount_) {
    audio_.setAmount(haAudioAmount_.getState());
    haAudioAmount_.setState(audio_.config().amount);
  } else if (&entity == &haUserNotification_) {
    const char* notification = haUserNotification_.getState();
    uint32_t duration = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;
    if (strcmp(notification, kUserNotificationOff) == 0) notifications_.stopUserNotification();
    else if (strcmp(notification, kUserNotificationText) == 0) {
      notifications_.startUserTextNotification(
        String(haUserNotificationText_.getState()).substring(0, 64), CRGB::White, duration
      );
      haUserNotificationText_.setState("");
    } else if (strcmp(notification, kUserNotificationWarning) == 0)
      notifications_.startUserNotification(UserNotificationType::Warning, duration);
    else if (strcmp(notification, kUserNotificationAlarm) == 0)
      notifications_.startUserNotification(UserNotificationType::Alarm, duration);
    syncUserNotificationState();
  } else if (&entity == &haUserNotificationText_) {
    const uint32_t duration = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;
    const char* text = haUserNotificationText_.getState();
    if (strlen(text) == 0) notifications_.stopUserNotification();
    else {
      notifications_.startUserTextNotification(String(text).substring(0, 64), CRGB::White, duration);
      haUserNotificationText_.setState("");
    }
    syncUserNotificationState();
  } else if (&entity == &haUserNotify_) {
    notifications_.startUserNotification(UserNotificationType::Notify);
    syncUserNotificationState();
  } else if (&entity == &haNextEffect_ || &entity == &haPrevEffect_ || &entity == &haRandomEffect_) {
    if (&entity == &haNextEffect_) {
      effects_.setNextEffect();
      notifications_.onEffectNext();
    } else if (&entity == &haPrevEffect_) {
      effects_.setPreviousEffect();
      notifications_.onEffectPrevious();
    } else {
      effects_.setRandomEffect();
      notifications_.onEffectNext();
    }
    rotation_.onManualRotation();
    syncSelectedEffectState();
  } else if (&entity == &haResetAllEffectSettings_) {
    effects_.resetEffectSettingsToDefaults();
    syncSelectedEffectState();
  } else if (&entity == &haResetCurrentEffectSettings_) {
    effects_.resetCurrentEffectSettingsToDefaults();
    syncSelectedEffectState();
  } else if (
    &entity == &haNotificationQuietHours_ || &entity == &haNotificationQuietStart_ ||
    &entity == &haNotificationQuietEnd_
  ) {
    NotificationQuietHours q = notifications_.getQuietHours();
    q.enabled = haNotificationQuietHours_.getState();
    uint16_t start = q.startMinutes, end = q.endMinutes;
    if (parseHaTime(haNotificationQuietStart_.getState(), start)) q.startMinutes = start;
    if (parseHaTime(haNotificationQuietEnd_.getState(), end)) q.endMinutes = end;
    notifications_.setQuietHours(q);
    syncQuietHoursState();
  }
}

void MqttService::initializeAdapter() {
  haUserNotification_.setState(kUserNotificationOff);
  haUserNotificationDuration_.setState(0);
  haPalette_.setState(Palettes::getPaletteName(Palettes::Id::Auto));
  haPalette_.addOption(Palettes::getPaletteName(Palettes::Id::Auto));
  for (uint8_t i = 0; i < Palettes::kSelectableCount; i++)
    haPalette_.addOption(Palettes::getPaletteName(Palettes::kSelectableOrder[i]));
  for (uint8_t i = 0; i < kRotationPresetCount; i++)
    haRotationInterval_.addOption(kRotationPresetLabels[i]);
  haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));
  const AudioConfig& config = audio_.config();
  haAudioMode_.setState(audioModeName(config.mode));
  haAudioBand_.setState(audioBandName(config.band));
  haAudioAmount_.setState(config.amount);
  telemetryTimer_.setOnTimer([this]() { telemetryTimerCallback(); });
  telemetryTimer_.start();
  stateRefreshTimer_.setOnTimer([this]() { stateRefreshTimerCallback(); });
  stateRefreshTimer_.start();
}

bool MqttService::registerEntities() {
  size_t effectListCapacity = 0;
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    effectListCapacity += strlen(Effects::getEffectName(Effects::kDisplayOrder[i]));
    if (i) effectListCapacity++;
  }
  if (!haEffectList_.reserve(effectListCapacity)) return false;
  haEffectList_ = "";
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    if (i) haEffectList_ += ',';
    haEffectList_ += Effects::getEffectName(Effects::kDisplayOrder[i]);
  }
  haLight_.setEffectList(haEffectList_.c_str());
  HAEntity* const entities[] = {
    &haLight_,
    &haRotationSwitch_,
    &haRotationInterval_,
    &haButtonSwitch_,
    &haEffectScale_,
    &haEffectSpeed_,
    &haEffectBrightness_,
    &haAutoOff_,
    &haAutoOffRemaining_,
    &haPalette_,
    &haUserNotification_,
    &haUserNotificationDuration_,
    &haUserNotificationRemaining_,
    &haUserNotify_,
    &haNextEffect_,
    &haPrevEffect_,
    &haRandomEffect_,
    &haResetAllEffectSettings_,
    &haResetCurrentEffectSettings_,
    &haUserNotificationText_,
    &haNotificationQuietHours_,
    &haNotificationQuietStart_,
    &haNotificationQuietEnd_,
    &haNotificationMuteState_,
    &haAudioMode_,
    &haAudioBand_,
    &haAudioAmount_,
    &haAudioAvailable_,
    &haUptime_,
    &haRssi_,
    &haRssiPct_,
    &haChannel_,
    &haVcc_,
    &haResetReason_,
  };
  static_assert(
    sizeof(entities) / sizeof(entities[0]) == sizeof(entityRegistry_) / sizeof(entityRegistry_[0]),
    "HA entity catalog size does not match fixed registry"
  );
  for (HAEntity* entity : entities)
    if (!controller_.addEntity(*entity)) return false;
  return true;
}

void MqttService::tickTimers() {
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { telemetryTimer_.update(); });
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { stateRefreshTimer_.update(); });
}

void MqttService::onTransportState(State state) {
  switch (state) {
    case State::Disabled: notifications_.onMqttDisabled(); break;
    case State::Connecting: notifications_.onMqttConnecting(); break;
    case State::Online: notifications_.onMqttConnected(); break;
    case State::ConfigError: notifications_.onMqttError(); break;
    default: break;
  }
}

void MqttService::onTransportFailure() {
  notifications_.onMqttError();
}

void MqttService::fullRefresh() {
  const Effects::Id id = effects_.getSelectedEffectId();
  const EffectSettings& s = settings_.getEffectSettings(id);
  const NotificationQuietHours& q = notifications_.getQuietHours();
  haLight_.setState(power_.isOn());
  haLight_.setBrightness(settings_.getGlobalBrightness());
  haLight_.setEffect(Effects::getEffectName(id));
  haLight_.setColor(effects_.getRed(), effects_.getGreen(), effects_.getBlue());
  haPalette_.setState(Palettes::getPaletteName(effects_.getSelectedPalette()));
  haRotationSwitch_.setState(rotation_.isActive());
  haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));
  haButtonSwitch_.setState(button_.isEnabled());
  haEffectScale_.setState(s.scale);
  haEffectSpeed_.setState(s.speed);
  haEffectBrightness_.setState(s.brightness);
  haAutoOff_.setState(power_.getAutoOffMinutes());
  haAutoOffRemaining_.setState(power_.getAutoOffRemainingSeconds());
  haUserNotificationRemaining_.setState(notifications_.getUserNotificationRemainingSeconds());
  haNotificationQuietHours_.setState(q.enabled);
  char quietStart[6];
  char quietEnd[6];
  formatHaTime(q.startMinutes, quietStart);
  formatHaTime(q.endMinutes, quietEnd);
  haNotificationQuietStart_.setState(quietStart);
  haNotificationQuietEnd_.setState(quietEnd);
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");
  syncUserNotificationState();
  const AudioConfig& a = audio_.config();
  haAudioMode_.setState(audioModeName(a.mode));
  haAudioBand_.setState(audioBandName(a.band));
  haAudioAmount_.setState(a.amount);
  haAudioAvailable_.setState(audio_.frame().available ? "yes" : "no");
}

void MqttService::telemetryTimerCallback() {
  haAudioAvailable_.setState(audio_.frame().available ? "yes" : "no");
  haUptime_.setState(millis() / 1000);
  haRssi_.setState(WiFi.RSSI());
  haRssiPct_.setState(2 * (WiFi.RSSI() + 100));
  haChannel_.setState(WiFi.channel());
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");
  uint16_t vcc = ESP.getVcc();
  haVcc_.setState(vcc != 0 && vcc >= 2500 && vcc <= 3700 ? static_cast<float>(vcc) / 1000.0f : 0);
  char reason[64];
  ESP.getResetReason().toCharArray(reason, sizeof(reason));
  haResetReason_.setState(reason);
}

void MqttService::stateRefreshTimerCallback() {
  fullRefresh();
}

void MqttService::syncLightState() {
  haLight_.setState(power_.isOn());
  haLight_.setBrightness(settings_.getGlobalBrightness());
}

void MqttService::syncSelectedEffectState() {
  const Effects::Id id = effects_.getSelectedEffectId();
  const EffectSettings& s = settings_.getEffectSettings(id);
  haLight_.setEffect(Effects::getEffectName(id));
  haEffectScale_.setState(s.scale);
  haEffectSpeed_.setState(s.speed);
  haEffectBrightness_.setState(s.brightness);
}

void MqttService::syncQuietHoursState() {
  const NotificationQuietHours& q = notifications_.getQuietHours();
  haNotificationQuietHours_.setState(q.enabled);
  char quietStart[6];
  char quietEnd[6];
  formatHaTime(q.startMinutes, quietStart);
  formatHaTime(q.endMinutes, quietEnd);
  haNotificationQuietStart_.setState(quietStart);
  haNotificationQuietEnd_.setState(quietEnd);
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");
}

void MqttService::syncUserNotificationState() {
  const UserNotificationType type = notifications_.getUserNotificationType();
  haUserNotification_.setState(
    type == UserNotificationType::Alarm     ? kUserNotificationAlarm
    : type == UserNotificationType::Warning ? kUserNotificationWarning
    : type == UserNotificationType::Text    ? kUserNotificationText
                                            : kUserNotificationOff
  );
  haUserNotificationRemaining_.setState(notifications_.getUserNotificationRemainingSeconds());
}

void MqttService::onLightCommand(bool on, uint8_t brightness) {
  power_.setOn(on);
  effects_.setGlobalBrightness(brightness);
  syncLightState();
}

void MqttService::onEffectCommand(const char* name) {
  Serial.print(F("[MQTT] Command arrived: effect set to "));
  Serial.println(name);
  Effects::Id id = Effects::getEffectId(name);
  if (id == Effects::Id::INVALID) return;
  rotation_.disable();
  effects_.setEffect(id);
  notifications_.onEffectNext();
  haRotationSwitch_.setState(rotation_.isActive());
  syncSelectedEffectState();
}

void MqttService::onColorCommand(uint8_t r, uint8_t g, uint8_t b) {
  Serial.printf("[MQTT] Command arrived: rgb %u,%u,%u\n", r, g, b);
  rotation_.disable();
  effects_.setColor(r, g, b);
  effects_.setEffect(Effects::fallback());
  haRotationSwitch_.setState(rotation_.isActive());
  haLight_.setColor(effects_.getRed(), effects_.getGreen(), effects_.getBlue());
  syncSelectedEffectState();
}

void MqttService::dispatchLightCommand(char* topic) {
  if (haLight_.isEffectCommandTopic(topic)) onEffectCommand(haLight_.getEffect());
  else if (haLight_.isColorCommandTopic(topic))
    onColorCommand(haLight_.getRed(), haLight_.getGreen(), haLight_.getBlue());
  else
    onLightCommand(haLight_.getState(), haLight_.getBrightness());
}

void MqttService::onPaletteCommand(const char* name) {
  Serial.print(F("[MQTT] Command arrived: palette set to "));
  Serial.println(name);
  effects_.setPalette(Palettes::parsePaletteName(name));
  haPalette_.setState(Palettes::getPaletteName(effects_.getSelectedPalette()));
}

void MqttService::consumeControllerEvents() {
  mqttController_.consumeEvents(mqttEventForward, this);
}

void MqttService::updateStates() {
  fullRefresh();
}

void MqttService::requestApply(const MqttConfig& config) {
  mqttController_.requestApply(controllerConfig(config));
}

void MqttService::requestEnabled(bool enabled) {
  mqttController_.requestEnabled(enabled);
}

void MqttService::requestRestart() {
  mqttController_.requestRestart();
}

MqttService::State MqttService::state() const {
  return mqttController_.state();
}

bool MqttService::isEnabled() const {
  return mqttController_.isEnabled();
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
  WifiController& wifi
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
