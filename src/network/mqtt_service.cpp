#include "mqtt_service.h"

#ifdef USE_MQTT
#include <EEPROM.h>
#include <HaMqttEntities.h>
#include <cctype>
#include <cstdlib>
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
#include "../network/wifi_service.h"
#include "../notification/controller.h"
#include "../notification/quiet_hours.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "../util/loop_profiler.h"

namespace {
  MqttService* gMqttService = nullptr;

  const char* kUserNotificationOff = "Off";
  const char* kUserNotificationWarning = "Warning";
  const char* kUserNotificationAlarm = "Alarm";
  const char* kUserNotificationText = "Text";
  const char* kUserNotificationOptions[] = {
    kUserNotificationOff, kUserNotificationText, kUserNotificationWarning, kUserNotificationAlarm
  };
  constexpr uint8_t kUserNotificationOptionsCount =
    sizeof(kUserNotificationOptions) / sizeof(kUserNotificationOptions[0]);

  static const char* kAudioModeOff = "Off";
  static const char* kAudioModeBrightness = "Brightness";
  static const char* kAudioModeSpeed = "Speed";
  static const char* kAudioModeScale = "Scale";
  static const char* kAudioModeEffect = "Effect";

  static const char* kAudioModeOptions[] = {
    kAudioModeOff,
    kAudioModeBrightness,
    kAudioModeSpeed,
    kAudioModeScale,
    kAudioModeEffect,
  };

  static const char* kAudioBandLevel = "Level";
  static const char* kAudioBandBass = "Bass";
  static const char* kAudioBandTreble = "Treble";

  static const char* kAudioBandOptions[] = {
    kAudioBandLevel,
    kAudioBandBass,
    kAudioBandTreble,
  };

  static const char* audioModeName(AudioMode mode) {
    switch (mode) {
      case AudioMode::Brightness: return kAudioModeBrightness;
      case AudioMode::Speed: return kAudioModeSpeed;
      case AudioMode::Scale: return kAudioModeScale;
      case AudioMode::Effect: return kAudioModeEffect;
      case AudioMode::Off:
      default: return kAudioModeOff;
    }
  }

  static AudioMode parseAudioMode(const char* value) {
    if (strcmp(value, kAudioModeBrightness) == 0) return AudioMode::Brightness;
    if (strcmp(value, kAudioModeSpeed) == 0) return AudioMode::Speed;
    if (strcmp(value, kAudioModeScale) == 0) return AudioMode::Scale;
    if (strcmp(value, kAudioModeEffect) == 0) return AudioMode::Effect;
    return AudioMode::Off;
  }

  static const char* audioBandName(AudioBand band) {
    switch (band) {
      case AudioBand::Bass: return kAudioBandBass;
      case AudioBand::Treble: return kAudioBandTreble;
      case AudioBand::Level:
      default: return kAudioBandLevel;
    }
  }

  static AudioBand parseAudioBand(const char* value) {
    if (strcmp(value, kAudioBandBass) == 0) return AudioBand::Bass;
    if (strcmp(value, kAudioBandTreble) == 0) return AudioBand::Treble;
    return AudioBand::Level;
  }

  void haCallbackForward(HAEntity* entity, char* topic, byte* payload, unsigned int length) {
    if (gMqttService != nullptr) {
      gMqttService->haCallback(entity, topic, payload, length);
    }
  }

  bool parseHaTime(const char* value, uint16_t& minutes) {
    if (value == nullptr) return false;

    int hours = -1;
    int mins = -1;
    int seconds = 0;

    const char* p = value;

    // Hours: one or two digits
    if (!isdigit((unsigned char)*p)) return false;
    hours = *p++ - '0';
    if (isdigit((unsigned char)*p)) {
      hours = hours * 10 + (*p++ - '0');
    }

    if (*p++ != ':') return false;

    // Minutes: exactly two digits
    if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return false;
    mins = (p[0] - '0') * 10 + (p[1] - '0');
    p += 2;

    // Optional seconds
    if (*p == ':') {
      p++;
      if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return false;
      seconds = (p[0] - '0') * 10 + (p[1] - '0');
      p += 2;
    }

    if (*p != '\0') return false;
    if (hours < 0 || hours > 23 || mins < 0 || mins > 59 || seconds < 0 || seconds > 59) return false;

    minutes = static_cast<uint16_t>(hours * 60 + mins);
    return true;
  }

  String formatHaTime(uint16_t minutes) {
    minutes %= 24 * 60;

    char buf[6];
    snprintf(buf, sizeof(buf), "%02u:%02u", minutes / 60, minutes % 60);
    return String(buf);
  }
} // namespace

MqttService::MqttService(
  AudioService& audio,
  EepromStore& eeprom,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button,
  WifiService& wifi
)
  : audio_(audio),
    eeprom_(eeprom),
    effects_(effects),
    notifications_(notifications),
    power_(power),
    rotation_(rotation),
    settings_(settings),
    button_(button),
    wifi_(wifi),
    client_(wifiClient_),
    telemetryTimer_(60000),
    stateRefreshTimer_(30000),
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
    haResetReason_("_reset_reason", "Reset Reason", haDevice_, 64) {
  gMqttService = this;

  haLight_.onCommand([](bool on, uint8_t brightness) {
    if (gMqttService != nullptr) {
      gMqttService->onLightCommand(on, brightness);
    }
  });

  haLight_.onEffectCommand([](const char* effectName) {
    if (gMqttService != nullptr) {
      gMqttService->onEffectCommand(effectName);
    }
  });

  haLight_.onColorCommand([](uint8_t r, uint8_t g, uint8_t b) {
    if (gMqttService != nullptr) {
      gMqttService->onColorCommand(r, g, b);
    }
  });

  haUserNotification_.setState(kUserNotificationOff);
  haUserNotificationDuration_.setState(0);
  haPalette_.setState(Palettes::getPaletteName(Palettes::Id::Auto));
  haPalette_.addOption(Palettes::getPaletteName(Palettes::Id::Auto));
  for (uint8_t i = 0; i < Palettes::kSelectableCount; i++) {
    haPalette_.addOption(Palettes::getPaletteName(Palettes::kSelectableOrder[i]));
  }

  for (uint8_t i = 0; i < kRotationPresetCount; i++) {
    haRotationInterval_.addOption(kRotationPresetLabels[i]);
  }
  haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));

  const AudioConfig& audioConfig = audio_.config();
  haAudioMode_.setState(audioModeName(audioConfig.mode));
  haAudioBand_.setState(audioBandName(audioConfig.band));
  haAudioAmount_.setState(audioConfig.amount);
}

void MqttService::init() {
  wifiClient_.setTimeout(kWifiClientTimeoutMs);
  client_.setSocketTimeout(kMqttSocketTimeoutSeconds);

  const MqttConfig& mqttConfig = eeprom_.readMqttConfig();
  requestedConfig_ = mqttConfig;
  requestedConfigGeneration_ = 1;
  requestedEnabled_ = isPersistedConfigEnabled(requestedConfig_);

  // Увеличиваем, так как payload из-за списка эффектов большой.
  // Буфер должен быть выделен до begin(), который нельзя безопасно повторить.
  bufferReady_ = client_.setBufferSize(HA_MAX_PAYLOAD_LENGTH);

  if (bufferReady_) {
    haEffectList_ = "";
    for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
      if (i > 0) haEffectList_ += ',';
      haEffectList_ += Effects::getEffectName(Effects::kDisplayOrder[i]);
    }
    haLight_.setEffectList(haEffectList_.c_str());

    // Capacity must match the number of addEntity() calls below.
    HAMQTT.begin(client_, 34);
    HAMQTT.addEntity(haLight_);
    HAMQTT.addEntity(haRotationSwitch_);
    HAMQTT.addEntity(haRotationInterval_);
    HAMQTT.addEntity(haButtonSwitch_);
    HAMQTT.addEntity(haEffectScale_);
    HAMQTT.addEntity(haEffectSpeed_);
    HAMQTT.addEntity(haEffectBrightness_);
    HAMQTT.addEntity(haAutoOff_);
    HAMQTT.addEntity(haAutoOffRemaining_);
    HAMQTT.addEntity(haPalette_);
    HAMQTT.addEntity(haUserNotification_);
    HAMQTT.addEntity(haUserNotificationDuration_);
    HAMQTT.addEntity(haUserNotificationRemaining_);
    HAMQTT.addEntity(haUserNotify_);
    HAMQTT.addEntity(haNextEffect_);
    HAMQTT.addEntity(haPrevEffect_);
    HAMQTT.addEntity(haRandomEffect_);
    HAMQTT.addEntity(haResetAllEffectSettings_);
    HAMQTT.addEntity(haResetCurrentEffectSettings_);
    HAMQTT.addEntity(haUserNotificationText_);
    HAMQTT.addEntity(haNotificationQuietHours_);
    HAMQTT.addEntity(haNotificationQuietStart_);
    HAMQTT.addEntity(haNotificationQuietEnd_);
    HAMQTT.addEntity(haNotificationMuteState_);
    HAMQTT.addEntity(haAudioMode_);
    HAMQTT.addEntity(haAudioBand_);
    HAMQTT.addEntity(haAudioAmount_);
    HAMQTT.addEntity(haAudioAvailable_);
    HAMQTT.addEntity(haUptime_);
    HAMQTT.addEntity(haRssi_);
    HAMQTT.addEntity(haRssiPct_);
    HAMQTT.addEntity(haChannel_);
    HAMQTT.addEntity(haVcc_);
    HAMQTT.addEntity(haResetReason_);
    HAMQTT.setCallback(haCallbackForward);
    registered_ = true;
  }

  telemetryTimer_.setOnTimer([this]() { this->telemetryTimerCallback(); });
  telemetryTimer_.start();
  stateRefreshTimer_.setOnTimer([this]() { this->stateRefreshTimerCallback(); });
  stateRefreshTimer_.start();

  if (!bufferReady_) {
    Serial.println(F("[MQTT] MQTT buffer allocation failed."));
    setState(State::ConfigError);
  } else if (!requestedEnabled_) {
    Serial.println(F("[MQTT] MQTT server is disabled."));
    setState(State::Disabled);
  } else if (!activateRequestedConfig()) {
    Serial.println(F("[MQTT] MQTT configuration is invalid."));
    setState(State::ConfigError);
  } else {
    setState(State::ConnectPrepare);
  }
}

void MqttService::tick() {
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { telemetryTimer_.update(); });
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { stateRefreshTimer_.update(); });

  if (!bufferReady_ || !registered_) return;

  if (state_ != State::DisconnectBarrier && handleRequestedCommands()) return;

  switch (state_) {
    case State::Disabled:
    case State::ConfigError: return;

    case State::DisconnectBarrier:
      if (!barrierPulsed_) {
        LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { HAMQTT.loop(); });
        barrierPulsed_ = true;
        return;
      }
      completeDisconnectBarrier();
      return;

    case State::WaitingForWifi:
      if (wifi_.isStaConnected()) {
        resetReconnectBackoff();
        reconnectTiming_ = millis();
        reconnectImmediately_ = true;
        setState(State::RetryWait);
      }
      return;

    case State::RetryWait:
      if (!wifi_.isStaConnected()) {
        beginDisconnectBarrier(false, true);
      } else if (shouldReconnect(millis())) {
        reconnectImmediately_ = false;
        setState(State::ConnectPrepare);
      }
      return;

    case State::ConnectPrepare:
      // Every attempt starts with a fresh disconnected controller pulse.
      LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { HAMQTT.loop(); });
      setState(State::Connecting);
      return;

    case State::Connecting:
      if (!wifi_.isStaConnected()) {
        beginDisconnectBarrier(false, true);
      } else {
        connect();
      }
      return;

    case State::Online:
      if (!wifi_.isStaConnected()) {
        beginDisconnectBarrier(false, true);
      } else if (!client_.connected()) {
        registerReconnectFailure(millis());
        retryPending_ = true;
        notifications_.onMqttError();
        beginDisconnectBarrier(false, false);
      } else {
        LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { HAMQTT.loop(); });
      }
      return;
  }
}

void MqttService::updateStates() {
  const Effects::Id effectId = effects_.getSelectedEffectId();
  const EffectSettings& effectSettings = settings_.getEffectSettings(effectId);
  const UserNotificationType userNotificationType = notifications_.getUserNotificationType();
  const NotificationQuietHours& notificationsQuietHours = notifications_.getQuietHours();

  haLight_.setState(power_.isOn());
  haLight_.setBrightness(settings_.getGlobalBrightness());
  haLight_.setEffect(Effects::getEffectName(effectId));
  haLight_.setColor(effects_.getRed(), effects_.getGreen(), effects_.getBlue());
  haPalette_.setState(Palettes::getPaletteName(effects_.getSelectedPalette()));
  haRotationSwitch_.setState(rotation_.isActive());
  haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));
  haButtonSwitch_.setState(button_.isEnabled());
  haEffectScale_.setState(effectSettings.scale);
  haEffectSpeed_.setState(effectSettings.speed);
  haEffectBrightness_.setState(effectSettings.brightness);
  haAutoOff_.setState(power_.getAutoOffMinutes());
  haAutoOffRemaining_.setState(power_.getAutoOffRemainingSeconds());
  haUserNotificationRemaining_.setState(notifications_.getUserNotificationRemainingSeconds());
  haNotificationQuietHours_.setState(notificationsQuietHours.enabled);
  haNotificationQuietStart_.setState(formatHaTime(notificationsQuietHours.startMinutes).c_str());
  haNotificationQuietEnd_.setState(formatHaTime(notificationsQuietHours.endMinutes).c_str());
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");

  if (userNotificationType == UserNotificationType::Alarm) {
    haUserNotification_.setState(kUserNotificationAlarm);
  } else if (userNotificationType == UserNotificationType::Warning) {
    haUserNotification_.setState(kUserNotificationWarning);
  } else if (userNotificationType == UserNotificationType::Text) {
    haUserNotification_.setState(kUserNotificationText);
  } else {
    haUserNotification_.setState(kUserNotificationOff);
  }

  const AudioConfig& audioConfig = audio_.config();
  const AudioFrame& audioFrame = audio_.frame();

  haAudioMode_.setState(audioModeName(audioConfig.mode));
  haAudioBand_.setState(audioBandName(audioConfig.band));
  haAudioAmount_.setState(audioConfig.amount);
  haAudioAvailable_.setState(audioFrame.available ? "yes" : "no");
}

void MqttService::requestApply(const MqttConfig& config) {
  requestedConfig_ = config;
  requestedConfigGeneration_ += 1;
  requestedEnabled_ = true;
  resetRetryAfterBarrier_ = true;
  requestedGeneration_ += 1;
}

void MqttService::requestEnabled(bool enabled) {
  if (requestedEnabled_ == enabled) return;

  requestedEnabled_ = enabled;
  if (enabled) resetRetryAfterBarrier_ = true;
  requestedGeneration_ += 1;
}

void MqttService::requestRestart() {
  resetRetryAfterBarrier_ = true;
  requestedGeneration_ += 1;
}

bool MqttService::isConfigValid(const MqttConfig& config, uint16_t& port) const {
  if (config.host[0] == '\0' || strcmp(config.host, "none") == 0 || config.port[0] == '\0') return false;

  uint32_t value = 0;
  for (const char* p = config.port; *p != '\0'; p++) {
    if (!isdigit(static_cast<unsigned char>(*p))) return false;

    value = value * 10 + static_cast<uint32_t>(*p - '0');
    if (value > 65535) return false;
  }

  if (value == 0) return false;

  port = static_cast<uint16_t>(value);
  return true;
}

bool MqttService::isPersistedConfigEnabled(const MqttConfig& config) const {
  return config.host[0] != '\0' && strcmp(config.host, "none") != 0;
}

bool MqttService::activateRequestedConfig() {
  if (activeConfigGeneration_ == requestedConfigGeneration_) {
    uint16_t port;
    return isConfigValid(activeConfig_, port);
  }

  activeConfig_ = requestedConfig_;
  activeConfigGeneration_ = requestedConfigGeneration_;

  uint16_t port;
  if (!isConfigValid(activeConfig_, port)) return false;

  client_.setServer(activeConfig_.host, port);
  return true;
}

void MqttService::setState(State state) {
  if (lifecycleStarted_ && state_ == state) return;

  state_ = state;
  lifecycleStarted_ = true;

  switch (state_) {
    case State::Disabled: notifications_.onMqttDisabled(); break;
    case State::Connecting: notifications_.onMqttConnecting(); break;
    case State::Online: notifications_.onMqttConnected(); break;
    case State::ConfigError: notifications_.onMqttError(); break;
    default: break;
  }
}

void MqttService::beginDisconnectBarrier(bool disconnectClient, bool abortTransport) {
  if (state_ == State::DisconnectBarrier) return;

  if (abortTransport) {
    wifiClient_.abort();
  } else if (disconnectClient && client_.connected()) {
    client_.disconnect();
  }

  barrierPulsed_ = false;
  setState(State::DisconnectBarrier);
}

void MqttService::completeDisconnectBarrier() {
  handledGeneration_ = requestedGeneration_;
  if (resetRetryAfterBarrier_) {
    retryPending_ = false;
    resetRetryAfterBarrier_ = false;
  }

  if (!requestedEnabled_) {
    setState(State::Disabled);
    return;
  }

  if (!activateRequestedConfig()) {
    setState(State::ConfigError);
    return;
  }

  if (!wifi_.isStaConnected()) {
    retryPending_ = false;
    setState(State::WaitingForWifi);
    return;
  }

  if (!retryPending_) {
    resetReconnectBackoff();
    reconnectTiming_ = millis();
    reconnectImmediately_ = true;
  }
  setState(State::RetryWait);
}

bool MqttService::handleRequestedCommands() {
  if (handledGeneration_ == requestedGeneration_) return false;

  if (state_ == State::Disabled) {
    handledGeneration_ = requestedGeneration_;
    resetRetryAfterBarrier_ = false;

    if (!requestedEnabled_) return true;

    if (!activateRequestedConfig()) {
      setState(State::ConfigError);
      return true;
    }

    retryPending_ = false;
    resetReconnectBackoff();
    reconnectTiming_ = millis();
    reconnectImmediately_ = true;
    setState(wifi_.isStaConnected() ? State::RetryWait : State::WaitingForWifi);
    return true;
  }

  retryPending_ = false;
  beginDisconnectBarrier(true, false);
  return true;
}

void MqttService::connect() {
  attemptConfig_ = activeConfig_;
  attemptGeneration_ = requestedGeneration_;

  Serial.printf(
    "[MQTT] Attempting MQTT connection to %s on port %s as %s ...",
    attemptConfig_.host,
    attemptConfig_.port,
    attemptConfig_.user
  );

  const bool connected = HAMQTT.connect(clientId_.c_str(), attemptConfig_.user, attemptConfig_.password);
  const bool stale = attemptGeneration_ != requestedGeneration_ || !requestedEnabled_ || !wifi_.isStaConnected();

  if (stale) {
    beginDisconnectBarrier(client_.connected(), !wifi_.isStaConnected());
    return;
  }

  if (connected) {
    Serial.println(F("[MQTT] connected!"));
    retryPending_ = false;
    resetReconnectBackoff();
    setState(State::Online);
    updateStates();
    return;
  }

  registerReconnectFailure(millis());
  retryPending_ = true;
  notifications_.onMqttError();
  Serial.print(F("[MQTT] failed, rc="));
  Serial.print(client_.state());
  Serial.printf(" try again in %d seconds\n", reconnectTimeout_ / 1000);
  beginDisconnectBarrier(false, false);
}

bool MqttService::shouldReconnect(uint32_t now) const {
  return reconnectImmediately_ || now - reconnectTiming_ >= reconnectTimeout_;
}

void MqttService::resetReconnectBackoff() {
  reconnectTimeout_ = kReconnectBaseMs;
  reconnectImmediately_ = false;
}

void MqttService::registerReconnectFailure(uint32_t now) {
  reconnectTiming_ = now;
  reconnectImmediately_ = false;
  if (!retryPending_ || reconnectTimeout_ < kReconnectBaseMs) {
    reconnectTimeout_ = kReconnectBaseMs;
  } else {
    reconnectTimeout_ *= 2;
    if (reconnectTimeout_ > kReconnectMaxMs) reconnectTimeout_ = kReconnectMaxMs;
  }
}

void MqttService::telemetryTimerCallback() {
  haAudioAvailable_.setState(audio_.frame().available ? "yes" : "no");

  haUptime_.setState(millis() / 1000);
  haRssi_.setState(WiFi.RSSI());
  haRssiPct_.setState(2 * (WiFi.RSSI() + 100));
  haChannel_.setState(WiFi.channel());
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");

  uint16_t vcc = ESP.getVcc();
  bool vccAvailable = (vcc != 0 && vcc >= 2500 && vcc <= 3700);
  haVcc_.setState(vccAvailable ? static_cast<float>(ESP.getVcc()) / 1000.0f : 0);

  char resetReason[64];
  ESP.getResetReason().toCharArray(resetReason, sizeof(resetReason));
  haResetReason_.setState(resetReason);
}

void MqttService::stateRefreshTimerCallback() {
  updateStates();
}

void MqttService::syncLightState() {
  haLight_.setState(power_.isOn());
  haLight_.setBrightness(settings_.getGlobalBrightness());
}

void MqttService::syncSelectedEffectState() {
  const Effects::Id effectId = effects_.getSelectedEffectId();
  const EffectSettings& effectSettings = settings_.getEffectSettings(effectId);

  haLight_.setEffect(Effects::getEffectName(effectId));
  haEffectScale_.setState(effectSettings.scale);
  haEffectSpeed_.setState(effectSettings.speed);
  haEffectBrightness_.setState(effectSettings.brightness);
}

void MqttService::syncQuietHoursState() {
  const NotificationQuietHours& quietHours = notifications_.getQuietHours();

  haNotificationQuietHours_.setState(quietHours.enabled);
  haNotificationQuietStart_.setState(formatHaTime(quietHours.startMinutes).c_str());
  haNotificationQuietEnd_.setState(formatHaTime(quietHours.endMinutes).c_str());
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");
}

void MqttService::syncUserNotificationState() {
  const UserNotificationType type = notifications_.getUserNotificationType();

  if (type == UserNotificationType::Alarm) {
    haUserNotification_.setState(kUserNotificationAlarm);
  } else if (type == UserNotificationType::Warning) {
    haUserNotification_.setState(kUserNotificationWarning);
  } else if (type == UserNotificationType::Text) {
    haUserNotification_.setState(kUserNotificationText);
  } else {
    haUserNotification_.setState(kUserNotificationOff);
  }
  haUserNotificationRemaining_.setState(notifications_.getUserNotificationRemainingSeconds());
}

void MqttService::haCallback(HAEntity* entity, char* topic, byte* payload, unsigned int length) {
  if (haLight_.dispatchCommand(&client_, topic, payload, length)) {
    return;
  }

  if (entity == &haRotationSwitch_) {
    const bool wasActive = rotation_.isActive();
    rotation_.setEnabled(haRotationSwitch_.getState());
    if (rotation_.isActive() && !wasActive) {
      notifications_.onRotationEnabled();
    } else if (!rotation_.isActive() && wasActive) {
      notifications_.onRotationDisabled();
    }
    haRotationSwitch_.setState(rotation_.isActive());
  } else if (entity == &haRotationInterval_) {
    rotation_.setIntervalSec(
      rotationPresetSecondsForIndex(rotationPresetIndexForLabel(haRotationInterval_.getState()))
    );
    haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));
  } else if (entity == &haButtonSwitch_) {
    button_.setEnabled(haButtonSwitch_.getState());
    haButtonSwitch_.setState(button_.isEnabled());
  } else if (entity == &haEffectScale_) {
    effects_.setEffectScale(haEffectScale_.getState());
    haEffectScale_.setState(settings_.getEffectSettings(effects_.getSelectedEffectId()).scale);
  } else if (entity == &haEffectSpeed_) {
    effects_.setEffectSpeed(haEffectSpeed_.getState());
    haEffectSpeed_.setState(settings_.getEffectSettings(effects_.getSelectedEffectId()).speed);
  } else if (entity == &haEffectBrightness_) {
    effects_.setEffectBrightness(haEffectBrightness_.getState());
    haEffectBrightness_.setState(settings_.getEffectSettings(effects_.getSelectedEffectId()).brightness);
  } else if (entity == &haPalette_) {
    onPaletteCommand(haPalette_.getState());
  } else if (entity == &haAutoOff_) {
    power_.setAutoOffMinutes(haAutoOff_.getState());
    haAutoOff_.setState(power_.getAutoOffMinutes());
  } else if (entity == &haAudioMode_) {
    audio_.setMode(parseAudioMode(haAudioMode_.getState()));
    haAudioMode_.setState(audioModeName(audio_.config().mode));
  } else if (entity == &haAudioBand_) {
    audio_.setBand(parseAudioBand(haAudioBand_.getState()));
    haAudioBand_.setState(audioBandName(audio_.config().band));
  } else if (entity == &haAudioAmount_) {
    audio_.setAmount(haAudioAmount_.getState());
    haAudioAmount_.setState(audio_.config().amount);
  } else if (entity == &haUserNotification_) {
    const char* notification = haUserNotification_.getState();
    const uint32_t durationMs = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;

    if (strcmp(notification, kUserNotificationOff) == 0) {
      notifications_.stopUserNotification();
    } else if (strcmp(notification, kUserNotificationText) == 0) {
      String text = String(haUserNotificationText_.getState()).substring(0, 64);
      notifications_.startUserTextNotification(text, CRGB::White, durationMs);
      haUserNotificationText_.setState("");
    } else if (strcmp(notification, kUserNotificationWarning) == 0) {
      notifications_.startUserNotification(UserNotificationType::Warning, durationMs);
    } else if (strcmp(notification, kUserNotificationAlarm) == 0) {
      notifications_.startUserNotification(UserNotificationType::Alarm, durationMs);
    }

    syncUserNotificationState();
  } else if (entity == &haUserNotificationText_) {
    const char* text = haUserNotificationText_.getState();
    const uint32_t durationMs = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;

    if (strlen(text) == 0) {
      notifications_.stopUserNotification();
    } else {
      notifications_.startUserTextNotification(String(text).substring(0, 64), CRGB::White, durationMs);
      haUserNotificationText_.setState("");
    }

    syncUserNotificationState();
  } else if (entity == &haUserNotify_) {
    notifications_.startUserNotification(UserNotificationType::Notify);
    syncUserNotificationState();
  } else if (entity == &haNextEffect_) {
    effects_.setNextEffect();
    notifications_.onEffectNext();
    rotation_.onManualRotation();
    syncSelectedEffectState();
  } else if (entity == &haPrevEffect_) {
    effects_.setPreviousEffect();
    notifications_.onEffectPrevious();
    rotation_.onManualRotation();
    syncSelectedEffectState();
  } else if (entity == &haRandomEffect_) {
    effects_.setRandomEffect();
    notifications_.onEffectNext();
    rotation_.onManualRotation();
    syncSelectedEffectState();
  } else if (entity == &haResetAllEffectSettings_) {
    effects_.resetEffectSettingsToDefaults();
    syncSelectedEffectState();
  } else if (entity == &haResetCurrentEffectSettings_) {
    effects_.resetCurrentEffectSettingsToDefaults();
    syncSelectedEffectState();
  } else if (
    entity == &haNotificationQuietHours_ || entity == &haNotificationQuietStart_ || entity == &haNotificationQuietEnd_
  ) {
    NotificationQuietHours q = notifications_.getQuietHours();
    q.enabled = haNotificationQuietHours_.getState();
    uint16_t startMinutes = q.startMinutes;
    uint16_t endMinutes = q.endMinutes;
    if (parseHaTime(haNotificationQuietStart_.getState(), startMinutes)) {
      q.startMinutes = startMinutes;
    }
    if (parseHaTime(haNotificationQuietEnd_.getState(), endMinutes)) {
      q.endMinutes = endMinutes;
    }

    notifications_.setQuietHours(q);
    syncQuietHoursState();
  }
}

void MqttService::onLightCommand(bool on, uint8_t brightness) {
  power_.setOn(on);
  effects_.setGlobalBrightness(brightness);
  syncLightState();
}

void MqttService::onEffectCommand(const char* effectName) {
  Serial.print(F("[MQTT] Command arrived: effect set to "));
  Serial.println(effectName);

  Effects::Id effectId = Effects::getEffectId(effectName);
  if (effectId == Effects::Id::INVALID) return;

  rotation_.disable();
  effects_.setEffect(effectId);
  notifications_.onEffectNext();
  haRotationSwitch_.setState(rotation_.isActive());
  syncSelectedEffectState();
}

void MqttService::onPaletteCommand(const char* paletteName) {
  Serial.print(F("[MQTT] Command arrived: palette set to "));
  Serial.println(paletteName);

  Palettes::Id paletteId = Palettes::parsePaletteName(paletteName);
  effects_.setPalette(paletteId);
  haPalette_.setState(Palettes::getPaletteName(effects_.getSelectedPalette()));
}

void MqttService::onColorCommand(uint8_t r, uint8_t g, uint8_t b) {
  Serial.print(F("[MQTT] Command arrived: rgb "));
  Serial.print(r);
  Serial.print(',');
  Serial.print(g);
  Serial.print(',');
  Serial.println(b);

  rotation_.disable();
  effects_.setColor(r, g, b);
  effects_.setEffect(Effects::fallback());
  haRotationSwitch_.setState(rotation_.isActive());
  haLight_.setColor(effects_.getRed(), effects_.getGreen(), effects_.getBlue());
  syncSelectedEffectState();
}

#else

MqttService::MqttService(
  AudioService& audio,
  EepromStore& eeprom,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button,
  WifiService& wifi
) {
  (void)audio;
  (void)eeprom;
  (void)effects;
  (void)notifications;
  (void)power;
  (void)rotation;
  (void)settings;
  (void)button;
  (void)wifi;
}

void MqttService::init() {
}
void MqttService::tick() {
}
void MqttService::updateStates() {
}

void MqttService::requestApply(const MqttConfig& config) {
  (void)config;
}

void MqttService::requestEnabled(bool enabled) {
  (void)enabled;
}

void MqttService::requestRestart() {
}

#endif

const char* MqttService::stateName() const {
  switch (state_) {
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
