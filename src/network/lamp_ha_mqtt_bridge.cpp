#include "lamp_ha_mqtt_bridge.h"

#ifdef USE_MQTT
#include <ESP8266WiFi.h>
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
  String formatHaTime(uint16_t minutes) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02u:%02u", (minutes % 1440) / 60, minutes % 60);
    return String(buf);
  }
} // namespace

LampHaMqttBridge* LampHaMqttBridge::callbackTarget_ = nullptr;

LampHaMqttBridge::LampHaMqttBridge(
  AudioService& audio,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button
)
  : audio_(audio),
    effects_(effects),
    notifications_(notifications),
    power_(power),
    rotation_(rotation),
    settings_(settings),
    button_(button),
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
}

void LampHaMqttBridge::initialize() {
  haLight_.onCommand([](bool on, uint8_t brightness) {
    if (callbackTarget_ != nullptr) callbackTarget_->onLightCommand(on, brightness);
  });
  haLight_.onEffectCommand([](const char* name) {
    if (callbackTarget_ != nullptr) callbackTarget_->onEffectCommand(name);
  });
  haLight_.onColorCommand([](uint8_t r, uint8_t g, uint8_t b) {
    if (callbackTarget_ != nullptr) callbackTarget_->onColorCommand(r, g, b);
  });
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

void LampHaMqttBridge::activateCallbackTarget() {
  callbackTarget_ = this;
}

uint8_t LampHaMqttBridge::entityCount() const {
  return 34;
}

void LampHaMqttBridge::registerEntities() {
  haEffectList_ = "";
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    if (i) haEffectList_ += ',';
    haEffectList_ += Effects::getEffectName(Effects::kDisplayOrder[i]);
  }
  haLight_.setEffectList(haEffectList_.c_str());
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
}

void LampHaMqttBridge::tickTimers() {
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { telemetryTimer_.update(); });
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { stateRefreshTimer_.update(); });
}
void LampHaMqttBridge::onTransportState(MqttState state) {
  switch (state) {
    case MqttState::Disabled: notifications_.onMqttDisabled(); break;
    case MqttState::Connecting: notifications_.onMqttConnecting(); break;
    case MqttState::Online: notifications_.onMqttConnected(); break;
    case MqttState::ConfigError: notifications_.onMqttError(); break;
    default: break;
  }
}
void LampHaMqttBridge::onTransportFailure() {
  notifications_.onMqttError();
}

void LampHaMqttBridge::fullRefresh() {
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
  haNotificationQuietStart_.setState(formatHaTime(q.startMinutes).c_str());
  haNotificationQuietEnd_.setState(formatHaTime(q.endMinutes).c_str());
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");
  syncUserNotificationState();
  const AudioConfig& a = audio_.config();
  haAudioMode_.setState(audioModeName(a.mode));
  haAudioBand_.setState(audioBandName(a.band));
  haAudioAmount_.setState(a.amount);
  haAudioAvailable_.setState(audio_.frame().available ? "yes" : "no");
}
void LampHaMqttBridge::telemetryTimerCallback() {
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
void LampHaMqttBridge::stateRefreshTimerCallback() {
  fullRefresh();
}
void LampHaMqttBridge::syncLightState() {
  haLight_.setState(power_.isOn());
  haLight_.setBrightness(settings_.getGlobalBrightness());
}
void LampHaMqttBridge::syncSelectedEffectState() {
  const Effects::Id id = effects_.getSelectedEffectId();
  const EffectSettings& s = settings_.getEffectSettings(id);
  haLight_.setEffect(Effects::getEffectName(id));
  haEffectScale_.setState(s.scale);
  haEffectSpeed_.setState(s.speed);
  haEffectBrightness_.setState(s.brightness);
}
void LampHaMqttBridge::syncQuietHoursState() {
  const NotificationQuietHours& q = notifications_.getQuietHours();
  haNotificationQuietHours_.setState(q.enabled);
  haNotificationQuietStart_.setState(formatHaTime(q.startMinutes).c_str());
  haNotificationQuietEnd_.setState(formatHaTime(q.endMinutes).c_str());
  haNotificationMuteState_.setState(notifications_.isMutedNow() ? "muted" : "active");
}
void LampHaMqttBridge::syncUserNotificationState() {
  const UserNotificationType type = notifications_.getUserNotificationType();
  haUserNotification_.setState(
    type == UserNotificationType::Alarm     ? kUserNotificationAlarm
    : type == UserNotificationType::Warning ? kUserNotificationWarning
    : type == UserNotificationType::Text    ? kUserNotificationText
                                            : kUserNotificationOff
  );
  haUserNotificationRemaining_.setState(notifications_.getUserNotificationRemainingSeconds());
}

void LampHaMqttBridge::onLightCommand(bool on, uint8_t brightness) {
  power_.setOn(on);
  effects_.setGlobalBrightness(brightness);
  syncLightState();
}
void LampHaMqttBridge::onEffectCommand(const char* name) {
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
void LampHaMqttBridge::onColorCommand(uint8_t r, uint8_t g, uint8_t b) {
  Serial.printf("[MQTT] Command arrived: rgb %u,%u,%u\n", r, g, b);
  rotation_.disable();
  effects_.setColor(r, g, b);
  effects_.setEffect(Effects::fallback());
  haRotationSwitch_.setState(rotation_.isActive());
  haLight_.setColor(effects_.getRed(), effects_.getGreen(), effects_.getBlue());
  syncSelectedEffectState();
}
void LampHaMqttBridge::onPaletteCommand(const char* name) {
  Serial.print(F("[MQTT] Command arrived: palette set to "));
  Serial.println(name);
  effects_.setPalette(Palettes::parsePaletteName(name));
  haPalette_.setState(Palettes::getPaletteName(effects_.getSelectedPalette()));
}

void LampHaMqttBridge::dispatchMessage(
  PubSubClient& client, HAEntity* entity, char* topic, byte* payload, unsigned int length
) {
  if (haLight_.dispatchCommand(&client, topic, payload, length)) return;
  if (entity == &haRotationSwitch_) {
    bool old = rotation_.isActive();
    rotation_.setEnabled(haRotationSwitch_.getState());
    if (rotation_.isActive() && !old) notifications_.onRotationEnabled();
    else if (!rotation_.isActive() && old)
      notifications_.onRotationDisabled();
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
  } else if (entity == &haPalette_)
    onPaletteCommand(haPalette_.getState());
  else if (entity == &haAutoOff_) {
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
  } else if (entity == &haUserNotificationText_) {
    const uint32_t duration = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;
    const char* text = haUserNotificationText_.getState();
    if (strlen(text) == 0) notifications_.stopUserNotification();
    else {
      notifications_.startUserTextNotification(String(text).substring(0, 64), CRGB::White, duration);
      haUserNotificationText_.setState("");
    }
    syncUserNotificationState();
  } else if (entity == &haUserNotify_) {
    notifications_.startUserNotification(UserNotificationType::Notify);
    syncUserNotificationState();
  } else if (entity == &haNextEffect_ || entity == &haPrevEffect_ || entity == &haRandomEffect_) {
    if (entity == &haNextEffect_) {
      effects_.setNextEffect();
      notifications_.onEffectNext();
    } else if (entity == &haPrevEffect_) {
      effects_.setPreviousEffect();
      notifications_.onEffectPrevious();
    } else {
      effects_.setRandomEffect();
      notifications_.onEffectNext();
    }
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
    uint16_t start = q.startMinutes, end = q.endMinutes;
    if (parseHaTime(haNotificationQuietStart_.getState(), start)) q.startMinutes = start;
    if (parseHaTime(haNotificationQuietEnd_.getState(), end)) q.endMinutes = end;
    notifications_.setQuietHours(q);
    syncQuietHoursState();
  }
}
#endif
