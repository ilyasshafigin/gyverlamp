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

  const char* USER_NOTIFICATION_OFF = "Off";
  const char* USER_NOTIFICATION_WARNING = "Warning";
  const char* USER_NOTIFICATION_ALARM = "Alarm";
  const char* USER_NOTIFICATION_TEXT = "Text";
  const char* USER_NOTIFICATION_OPTIONS[] = {
    USER_NOTIFICATION_OFF, USER_NOTIFICATION_TEXT, USER_NOTIFICATION_WARNING, USER_NOTIFICATION_ALARM
  };
  constexpr uint8_t USER_NOTIFICATION_OPTIONS_COUNT =
    sizeof(USER_NOTIFICATION_OPTIONS) / sizeof(USER_NOTIFICATION_OPTIONS[0]);

  static const char* AUDIO_MODE_OFF = "Off";
  static const char* AUDIO_MODE_BRIGHTNESS = "Brightness";
  static const char* AUDIO_MODE_SPEED = "Speed";
  static const char* AUDIO_MODE_SCALE = "Scale";
  static const char* AUDIO_MODE_EFFECT = "Effect";

  static const char* AUDIO_MODE_OPTIONS[] = {
    AUDIO_MODE_OFF,
    AUDIO_MODE_BRIGHTNESS,
    AUDIO_MODE_SPEED,
    AUDIO_MODE_SCALE,
    AUDIO_MODE_EFFECT,
  };

  static const char* AUDIO_BAND_LEVEL = "Level";
  static const char* AUDIO_BAND_BASS = "Bass";
  static const char* AUDIO_BAND_TREBLE = "Treble";

  static const char* AUDIO_BAND_OPTIONS[] = {
    AUDIO_BAND_LEVEL,
    AUDIO_BAND_BASS,
    AUDIO_BAND_TREBLE,
  };

  static const char* audioModeName(AudioMode mode) {
    switch (mode) {
      case AudioMode::Brightness: return AUDIO_MODE_BRIGHTNESS;
      case AudioMode::Speed: return AUDIO_MODE_SPEED;
      case AudioMode::Scale: return AUDIO_MODE_SCALE;
      case AudioMode::Effect: return AUDIO_MODE_EFFECT;
      case AudioMode::Off:
      default: return AUDIO_MODE_OFF;
    }
  }

  static AudioMode parseAudioMode(const char* value) {
    if (strcmp(value, AUDIO_MODE_BRIGHTNESS) == 0) return AudioMode::Brightness;
    if (strcmp(value, AUDIO_MODE_SPEED) == 0) return AudioMode::Speed;
    if (strcmp(value, AUDIO_MODE_SCALE) == 0) return AudioMode::Scale;
    if (strcmp(value, AUDIO_MODE_EFFECT) == 0) return AudioMode::Effect;
    return AudioMode::Off;
  }

  static const char* audioBandName(AudioBand band) {
    switch (band) {
      case AudioBand::Bass: return AUDIO_BAND_BASS;
      case AudioBand::Treble: return AUDIO_BAND_TREBLE;
      case AudioBand::Level:
      default: return AUDIO_BAND_LEVEL;
    }
  }

  static AudioBand parseAudioBand(const char* value) {
    if (strcmp(value, AUDIO_BAND_BASS) == 0) return AudioBand::Bass;
    if (strcmp(value, AUDIO_BAND_TREBLE) == 0) return AudioBand::Treble;
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
    publishTimer_(60000),
    clientId_("GyverLamp-" + String(ESP.getChipId(), HEX)),
    haDevice_(clientId_.c_str(), DEVICE_NAME, FIRMWARE_VERSION, FIRMWARE_MANUFACTURER, "Gyver Lamp"),
    haLight_("_light", "Gyver Lamp", haDevice_),
    haRotationSwitch_("_rotation", "Rotation", haDevice_),
    haRotationInterval_("_rotation_interval", "Rotation Interval", haDevice_, ROTATION_PRESET_COUNT),
    haButtonSwitch_("_button", "Touch Button", haDevice_),
    haEffectScale_("_effect_scale", "Effect Scale", haDevice_, 1, 255, 1),
    haEffectSpeed_("_effect_speed", "Effect Speed", haDevice_, 1, 255, 1),
    haEffectBrightness_("_effect_brightness", "Effect brightness", haDevice_, 0, 255, 1),
    haAutoOff_("_auto_off_minutes", "Auto Off Minutes", haDevice_, AUTO_OFF_MINUTES_MIN, AUTO_OFF_MINUTES_MAX, 1),
    haAutoOffRemaining_("_auto_off_remaining", "Auto Off Remaining", haDevice_, "s", 0),
    haPalette_("_palette", "Palette", haDevice_, Palettes::COUNT),
    haUserNotification_(
      "_user_notification", "User Notification", haDevice_, USER_NOTIFICATION_OPTIONS_COUNT, USER_NOTIFICATION_OPTIONS
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
    haAudioMode_("_audio_mode", "Audio Mode", haDevice_, 5, AUDIO_MODE_OPTIONS),
    haAudioBand_("_audio_band", "Audio Band", haDevice_, 3, AUDIO_BAND_OPTIONS),
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

  haUserNotification_.setState(USER_NOTIFICATION_OFF);
  haUserNotificationDuration_.setState(0);
  haPalette_.setState(Palettes::getPaletteName(Palettes::Id::Auto));
  haPalette_.addOption(Palettes::getPaletteName(Palettes::Id::Auto));
  for (uint8_t i = 0; i < Palettes::SELECTABLE_COUNT; i++) {
    haPalette_.addOption(Palettes::getPaletteName(Palettes::SELECTABLE_ORDER[i]));
  }

  for (uint8_t i = 0; i < ROTATION_PRESET_COUNT; i++) {
    haRotationInterval_.addOption(ROTATION_PRESET_LABELS[i]);
  }
  haRotationInterval_.setState(rotationPresetLabelForSeconds(rotation_.getIntervalSec()));

  const AudioConfig& audioConfig = audio_.config();
  haAudioMode_.setState(audioModeName(audioConfig.mode));
  haAudioBand_.setState(audioBandName(audioConfig.band));
  haAudioAmount_.setState(audioConfig.amount);

  mqttHost_[0] = '\0';
  strlcpy(mqttUser_, "user", MQTT_USER_LEN);
  strlcpy(mqttPassword_, "pass", MQTT_PASS_LEN);
  strlcpy(mqttPort_, "1883", MQTT_PORT_LEN);
}

void MqttService::init() {
  wifiClient_.setTimeout(WIFI_CLIENT_TIMEOUT_MS);
  client_.setSocketTimeout(MQTT_SOCKET_TIMEOUT_SECONDS);

  const MqttConfig& mqttConfig = eeprom_.readMqttConfig();

  if (strlen(mqttConfig.host) > 0) {
    setMqttHost(mqttConfig.host);
    setMqttPort(mqttConfig.port);
    setMqttUser(mqttConfig.user);
    setMqttPassword(mqttConfig.password);
  }

  if (strcmp(mqttHost_, "none") == 0 || strlen(mqttHost_) == 0) {
    enabled_ = false;
    notifications_.onMqttDisabled();
    Serial.println(F("[MQTT] MQTT server is disabled."));
  }

  if (enabled_) {
    haEffectList_ = "";
    for (uint8_t i = 0; i < Effects::DISPLAY_COUNT; i++) {
      if (i > 0) haEffectList_ += ',';
      haEffectList_ += Effects::getEffectName(Effects::DISPLAY_ORDER[i]);
    }
    haLight_.setEffectList(haEffectList_.c_str());

    // Capacity must match the number of addEntity() calls below
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

    // Увеличиваем, так как payload из-за списка эффектов большой
    client_.setBufferSize(HA_MAX_PAYLOAD_LENGTH);
  }

  publishTimer_.setOnTimer([this]() { this->publishTimerCallback(); });
  publishTimer_.start();
}

void MqttService::tick() {
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() { publishTimer_.update(); });

  if (enabled_ && wifi_.isStaConnected() && !HAMQTT.connected()) {
    reconnect();
  }

  if (enabled_ && wifi_.isStaConnected()) {
    LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() { HAMQTT.loop(); });
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
    haUserNotification_.setState(USER_NOTIFICATION_ALARM);
  } else if (userNotificationType == UserNotificationType::Warning) {
    haUserNotification_.setState(USER_NOTIFICATION_WARNING);
  } else if (userNotificationType == UserNotificationType::Text) {
    haUserNotification_.setState(USER_NOTIFICATION_TEXT);
  } else {
    haUserNotification_.setState(USER_NOTIFICATION_OFF);
  }

  const AudioConfig& audioConfig = audio_.config();
  const AudioFrame& audioFrame = audio_.frame();

  haAudioMode_.setState(audioModeName(audioConfig.mode));
  haAudioBand_.setState(audioBandName(audioConfig.band));
  haAudioAmount_.setState(audioConfig.amount);
  haAudioAvailable_.setState(audioFrame.available ? "yes" : "no");
}

void MqttService::reconnect() {
  uint32_t now = millis();
  if (!shouldReconnect(now) || !wifi_.isStaConnected()) {
    return;
  }

  notifications_.onMqttConnecting();

  const MqttConfig& mqttConfig = eeprom_.readMqttConfig();
  client_.setServer(mqttConfig.host, atoi(mqttConfig.port));

  Serial.printf(
    "[MQTT] Attempting MQTT connection to %s on port %s as %s ...", mqttConfig.host, mqttConfig.port, mqttConfig.user
  );

  if (HAMQTT.connect(clientId_.c_str(), mqttConfig.user, mqttConfig.password)) {
    Serial.println(F("[MQTT] connected!"));

    notifications_.onMqttConnected();
    resetReconnectBackoff();
    updateStates();
  } else {
    registerReconnectFailure(millis());
    notifications_.onMqttError();

    if (shouldDisableAfterReconnectFailures()) {
      Serial.println(F("[MQTT] Can not establish a connection, disabling MQTT."));
      notifications_.onMqttDisabled();
      enabled_ = false;
      return;
    }

    Serial.print(F("[MQTT] failed, rc="));
    Serial.print(client_.state());
    Serial.printf(" try again in %d seconds\n", reconnectTimeout_ / 1000);
  }
}

bool MqttService::shouldReconnect(uint32_t now) const {
  return enabled_ && (now - reconnectTiming_ > reconnectTimeout_);
}

void MqttService::resetReconnectBackoff() {
  reconnectTimeout_ = 5000;
  reconnectCount_ = 0;
}

void MqttService::registerReconnectFailure(uint32_t now) {
  reconnectTiming_ = now;
  reconnectCount_ += 1;
  reconnectTimeout_ *= 2;
  if (reconnectTimeout_ > 60000) reconnectTimeout_ = 60000;
}

bool MqttService::shouldDisableAfterReconnectFailures() const {
  return reconnectCount_ >= 9;
}

void MqttService::publishTimerCallback() {
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
    updateStates();
  } else if (entity == &haRotationInterval_) {
    rotation_.setIntervalSec(
      rotationPresetSecondsForIndex(rotationPresetIndexForLabel(haRotationInterval_.getState()))
    );
    updateStates();
  } else if (entity == &haButtonSwitch_) {
    button_.setEnabled(haButtonSwitch_.getState());
    updateStates();
  } else if (entity == &haEffectScale_) {
    effects_.setEffectScale(haEffectScale_.getState());
    updateStates();
  } else if (entity == &haEffectSpeed_) {
    effects_.setEffectSpeed(haEffectSpeed_.getState());
    updateStates();
  } else if (entity == &haEffectBrightness_) {
    effects_.setEffectBrightness(haEffectBrightness_.getState());
    updateStates();
  } else if (entity == &haPalette_) {
    onPaletteCommand(haPalette_.getState());
  } else if (entity == &haAutoOff_) {
    power_.setAutoOffMinutes(haAutoOff_.getState());
    updateStates();
  } else if (entity == &haAudioMode_) {
    audio_.setMode(parseAudioMode(haAudioMode_.getState()));
    updateStates();
  } else if (entity == &haAudioBand_) {
    audio_.setBand(parseAudioBand(haAudioBand_.getState()));
    updateStates();
  } else if (entity == &haAudioAmount_) {
    audio_.setAmount(haAudioAmount_.getState());
    updateStates();
  } else if (entity == &haUserNotification_) {
    const char* notification = haUserNotification_.getState();
    const uint32_t durationMs = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;

    if (strcmp(notification, USER_NOTIFICATION_OFF) == 0) {
      notifications_.stopUserNotification();
    } else if (strcmp(notification, USER_NOTIFICATION_TEXT) == 0) {
      String text = String(haUserNotificationText_.getState()).substring(0, 64);
      notifications_.startUserTextNotification(text, CRGB::White, durationMs);
      haUserNotificationText_.setState("");
    } else if (strcmp(notification, USER_NOTIFICATION_WARNING) == 0) {
      notifications_.startUserNotification(UserNotificationType::Warning, durationMs);
    } else if (strcmp(notification, USER_NOTIFICATION_ALARM) == 0) {
      notifications_.startUserNotification(UserNotificationType::Alarm, durationMs);
    }

    updateStates();
  } else if (entity == &haUserNotificationText_) {
    const char* text = haUserNotificationText_.getState();
    const uint32_t durationMs = static_cast<uint32_t>(haUserNotificationDuration_.getState()) * 1000UL;

    if (strlen(text) == 0) {
      notifications_.stopUserNotification();
    } else {
      notifications_.startUserTextNotification(String(text).substring(0, 64), CRGB::White, durationMs);
      haUserNotificationText_.setState("");
    }

    updateStates();
  } else if (entity == &haUserNotify_) {
    notifications_.startUserNotification(UserNotificationType::Notify);
    updateStates();
  } else if (entity == &haNextEffect_) {
    effects_.setNextEffect();
    notifications_.onEffectNext();
    rotation_.onManualRotation();
    updateStates();
  } else if (entity == &haPrevEffect_) {
    effects_.setPreviousEffect();
    notifications_.onEffectPrevious();
    rotation_.onManualRotation();
    updateStates();
  } else if (entity == &haRandomEffect_) {
    effects_.setRandomEffect();
    notifications_.onEffectNext();
    rotation_.onManualRotation();
    updateStates();
  } else if (entity == &haResetAllEffectSettings_) {
    effects_.resetEffectSettingsToDefaults();
    updateStates();
  } else if (entity == &haResetCurrentEffectSettings_) {
    effects_.resetCurrentEffectSettingsToDefaults();
    updateStates();
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
    updateStates();
  }
}

void MqttService::onLightCommand(bool on, uint8_t brightness) {
  power_.setOn(on);
  settings_.setGlobalBrightness(brightness);
  updateStates();
}

void MqttService::onEffectCommand(const char* effectName) {
  Serial.print(F("[MQTT] Command arrived: effect set to "));
  Serial.println(effectName);

  Effects::Id effectId = Effects::getEffectId(effectName);
  if (effectId == Effects::Id::INVALID) return;

  rotation_.disable();
  effects_.setEffect(effectId);
  notifications_.onEffectNext();
  updateStates();
}

void MqttService::onPaletteCommand(const char* paletteName) {
  Serial.print(F("[MQTT] Command arrived: palette set to "));
  Serial.println(paletteName);

  Palettes::Id paletteId = Palettes::parsePaletteName(paletteName);
  effects_.setPalette(paletteId);
  updateStates();
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
  updateStates();
}

#else

MqttService::MqttService(
  EepromStore& eeprom,
  EffectController& effects,
  NotificationController& notifications,
  PowerController& power,
  RotationController& rotation,
  SettingsRepository& settings,
  TouchButton& button,
  WifiService& wifi
) {
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

#endif
