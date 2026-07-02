#include "mqtt_service.h"

#ifdef USE_MQTT
#include <EEPROM.h>
#include <uptime_formatter.h>
#include <cstdlib>
#include <cctype>
#include <HaMqttEntities.h>

#include "../audio/audio_service.h"
#include "../core/auto_off_config.h"
#include "../core/power_controller.h"
#include "../core/rotation_controller.h"
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
    USER_NOTIFICATION_OFF,
    USER_NOTIFICATION_TEXT,
    USER_NOTIFICATION_WARNING,
    USER_NOTIFICATION_ALARM
  };
  constexpr uint8_t USER_NOTIFICATION_OPTIONS_COUNT = sizeof(USER_NOTIFICATION_OPTIONS) / sizeof(USER_NOTIFICATION_OPTIONS[0]);

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
}

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
) :
  _audio(audio),
  _eeprom(eeprom),
  _effects(effects),
  _notifications(notifications),
  _power(power),
  _rotation(rotation),
  _settings(settings),
  _button(button),
  _wifi(wifi),
  _client(_wifiClient),
  _publishTimer(60000),
  _clientId("GyverLamp-" + String(ESP.getChipId(), HEX)),
  _haDevice(_clientId.c_str(), DEVICE_NAME, FIRMWARE_VERSION, FIRMWARE_MANUFACTURER, "Gyver Lamp"),
  _haLight("_light", "Gyver Lamp", _haDevice),
  _haRotationSwitch("_rotation", "Rotation", _haDevice),
  _haRotationInterval("_rotation_interval_minutes", "Rotation Interval Minutes", _haDevice, ROTATION_INTERVAL_MIN_MIN, ROTATION_INTERVAL_MIN_MAX, 1),
  _haButtonSwitch("_button", "Button", _haDevice),
  _haEffectScale("_effect_scale", "Effect Scale", _haDevice, 1, 255, 1),
  _haEffectSpeed("_effect_speed", "Effect Speed", _haDevice, 1, 255, 1),
  _haAutoOff("_auto_off_minutes", "Auto Off Minutes", _haDevice, AUTO_OFF_MINUTES_MIN, AUTO_OFF_MINUTES_MAX, 1),
  _haAutoOffRemaining("_auto_off_remaining", "Auto Off Remaining", _haDevice, "s", 0),
  _haPalette("_palette", "Palette", _haDevice, Palettes::COUNT),
  _haUserNotification("_user_notification", "User Notification", _haDevice, USER_NOTIFICATION_OPTIONS_COUNT, USER_NOTIFICATION_OPTIONS),
  _haUserNotificationDuration("_user_notification_duration", "User Notification Duration", _haDevice, 0, 3600, 1),
  _haUserNotificationRemaining("_user_notification_remaining", "User Notification Remaining", _haDevice, "s", 0),
  _haUserNotify("_user_notify", "User Notify", _haDevice),
  _haUserNotificationText("_user_notification_text", "User Notification Text", _haDevice, 64),
  _haNotificationQuietHours("_notification_quiet_hours", "Notification Quiet Hours", _haDevice),
  _haNotificationQuietStart("_notification_quiet_start", "Notification Quiet Start", _haDevice),
  _haNotificationQuietEnd("_notification_quiet_end", "Notification Quiet End", _haDevice),
  _haNotificationMuteState("_notification_mute_state", "Notification Mute State", _haDevice, 16),
  _haAudioMode("_audio_mode", "Audio Mode", _haDevice, 5, AUDIO_MODE_OPTIONS),
  _haAudioBand("_audio_band", "Audio Band", _haDevice, 3, AUDIO_BAND_OPTIONS),
  _haAudioAmount("_audio_amount", "Audio Amount", _haDevice, 0, 255, 1),
  _haAudioAvailable("_audio_available", "Audio Available", _haDevice, 8),
  _haUptime("_uptime", "Uptime", _haDevice, "s", 0),
  _haRssi("_rssi", "RSSI", _haDevice, "dBm", 0),
  _haRssiPct("_rssi_pct", "RSSI %", _haDevice, "%", 0),
  _haChannel("_channel", "WiFi Channel", _haDevice, nullptr, 0),
  _haVcc("_vcc", "VCC", _haDevice, "V", 3),
  _haBootCount("_boot_count", "Boot Count", _haDevice, nullptr, 0),
  _haResetReason("_reset_reason", "Reset Reason", _haDevice, 64) {
  gMqttService = this;

  _haLight.onCommand([](bool on, uint8_t brightness) {
    if (gMqttService != nullptr) {
      gMqttService->onLightCommand(on, brightness);
    }
    });

  _haLight.onEffectCommand([](const char* effectName) {
    if (gMqttService != nullptr) {
      gMqttService->onEffectCommand(effectName);
    }
    });

  _haLight.onColorCommand([](uint8_t r, uint8_t g, uint8_t b) {
    if (gMqttService != nullptr) {
      gMqttService->onColorCommand(r, g, b);
    }
    });

  _haUserNotification.setState(USER_NOTIFICATION_OFF);
  _haUserNotificationDuration.setState(0);
  _haPalette.setState(Palettes::getPaletteName(Palettes::Id::Auto));
  _haPalette.addOption(Palettes::getPaletteName(Palettes::Id::Auto));
  for (uint8_t i = 0; i < Palettes::SELECTABLE_COUNT; i++) {
    _haPalette.addOption(Palettes::getPaletteName(Palettes::SELECTABLE_ORDER[i]));
  }

  const AudioConfig& audioConfig = _audio.config();
  _haAudioMode.setState(audioModeName(audioConfig.mode));
  _haAudioBand.setState(audioBandName(audioConfig.band));
  _haAudioAmount.setState(audioConfig.amount);

  _mqttHost[0] = '\0';
  strlcpy(_mqttUser, "user", MQTT_USER_LEN);
  strlcpy(_mqttPassword, "pass", MQTT_PASS_LEN);
  strlcpy(_mqttPort, "1883", MQTT_PORT_LEN);
}

void MqttService::init() {
  const MqttConfig& mqttConfig = _eeprom.readMqttConfig();

  if (strlen(mqttConfig.host) > 0) {
    setMqttHost(mqttConfig.host);
    setMqttPort(mqttConfig.port);
    setMqttUser(mqttConfig.user);
    setMqttPassword(mqttConfig.password);
  }

  if (strcmp(_mqttHost, "none") == 0 || strlen(_mqttHost) == 0) {
    _enabled = false;
    _notifications.onMqttDisabled();
    Serial.println(F("[MQTT] MQTT server is disabled."));
  }

  if (_enabled) {
    _haEffectList = "";
    for (uint8_t i = 0; i < Effects::DISPLAY_COUNT; i++) {
      if (i > 0) _haEffectList += ',';
      _haEffectList += Effects::getEffectName(Effects::DISPLAY_ORDER[i]);
    }
    _haLight.setEffectList(_haEffectList.c_str());

    HAMQTT.begin(_client, 29);
    HAMQTT.addEntity(_haLight);
    HAMQTT.addEntity(_haRotationSwitch);
    HAMQTT.addEntity(_haRotationInterval);
    HAMQTT.addEntity(_haButtonSwitch);
    HAMQTT.addEntity(_haEffectScale);
    HAMQTT.addEntity(_haEffectSpeed);
    HAMQTT.addEntity(_haAutoOff);
    HAMQTT.addEntity(_haAutoOffRemaining);
    HAMQTT.addEntity(_haPalette);
    HAMQTT.addEntity(_haUserNotification);
    HAMQTT.addEntity(_haUserNotificationDuration);
    HAMQTT.addEntity(_haUserNotificationRemaining);
    HAMQTT.addEntity(_haUserNotify);
    HAMQTT.addEntity(_haUserNotificationText);
    HAMQTT.addEntity(_haNotificationQuietHours);
    HAMQTT.addEntity(_haNotificationQuietStart);
    HAMQTT.addEntity(_haNotificationQuietEnd);
    HAMQTT.addEntity(_haNotificationMuteState);
    HAMQTT.addEntity(_haAudioMode);
    HAMQTT.addEntity(_haAudioBand);
    HAMQTT.addEntity(_haAudioAmount);
    HAMQTT.addEntity(_haAudioAvailable);
    HAMQTT.addEntity(_haUptime);
    HAMQTT.addEntity(_haRssi);
    HAMQTT.addEntity(_haRssiPct);
    HAMQTT.addEntity(_haChannel);
    HAMQTT.addEntity(_haVcc);
    HAMQTT.addEntity(_haBootCount);
    HAMQTT.addEntity(_haResetReason);
    HAMQTT.setCallback(haCallbackForward);

    // Увеличиваем, так как payload из-за списка эффектов большой
    _client.setBufferSize(HA_MAX_PAYLOAD_LENGTH);
  }

  _publishTimer.setOnTimer([this]() { this->publishTimerCallback(); });
  _publishTimer.start();
}

void MqttService::tick() {
  LoopProfiler::measure(LoopProfiler::MQTT_TIMER, [this]() {
    _publishTimer.update();
    });

  if (_enabled && _wifi.isStaConnected() && !HAMQTT.connected()) {
    reconnect();
  }

  if (_enabled && _wifi.isStaConnected()) {
    LoopProfiler::measure(LoopProfiler::MQTT_LOOP, [this]() {
      HAMQTT.loop();
      });
  }
}

void MqttService::updateStates() {
  const Effects::Id effectId = _effects.getSelectedEffectId();
  const EffectSettings& effectSettings = _settings.getEffectSettings(effectId);
  const UserNotificationType userNotificationType = _notifications.getUserNotificationType();
  const NotificationQuietHours& notificationsQuietHours = _notifications.getQuietHours();

  _haLight.setState(_power.isOn());
  _haLight.setBrightness(effectSettings.brightness);
  _haLight.setEffect(Effects::getEffectName(effectId));
  _haLight.setColor(_effects.getRed(), _effects.getGreen(), _effects.getBlue());
  _haPalette.setState(Palettes::getPaletteName(_effects.getSelectedPalette()));
  _haRotationSwitch.setState(_rotation.isActive());
  _haRotationInterval.setState((_rotation.getIntervalSec() + 59) / 60);
  _haButtonSwitch.setState(_button.isEnabled());
  _haEffectScale.setState(effectSettings.scale);
  _haEffectSpeed.setState(effectSettings.speed);
  _haAutoOff.setState(_power.getAutoOffMinutes());
  _haAutoOffRemaining.setState(_power.getAutoOffRemainingSeconds());
  _haUserNotificationRemaining.setState(_notifications.getUserNotificationRemainingSeconds());
  _haNotificationQuietHours.setState(notificationsQuietHours.enabled);
  _haNotificationQuietStart.setState(formatHaTime(notificationsQuietHours.startMinutes).c_str());
  _haNotificationQuietEnd.setState(formatHaTime(notificationsQuietHours.endMinutes).c_str());
  _haNotificationMuteState.setState(_notifications.isMutedNow() ? "muted" : "active");

  if (userNotificationType == UserNotificationType::Alarm) {
    _haUserNotification.setState(USER_NOTIFICATION_ALARM);
  } else if (userNotificationType == UserNotificationType::Warning) {
    _haUserNotification.setState(USER_NOTIFICATION_WARNING);
  } else if (userNotificationType == UserNotificationType::Text) {
    _haUserNotification.setState(USER_NOTIFICATION_TEXT);
  } else {
    _haUserNotification.setState(USER_NOTIFICATION_OFF);
  }

  const AudioConfig& audioConfig = _audio.config();
  const AudioFrame& audioFrame = _audio.frame();

  _haAudioMode.setState(audioModeName(audioConfig.mode));
  _haAudioBand.setState(audioBandName(audioConfig.band));
  _haAudioAmount.setState(audioConfig.amount);
  _haAudioAvailable.setState(audioFrame.available ? "yes" : "no");
}

void MqttService::reconnect() {
  uint32_t now = millis();
  if (!shouldReconnect(now) || !_wifi.isStaConnected()) {
    return;
  }

  _notifications.onMqttConnecting();

  const MqttConfig& mqttConfig = _eeprom.readMqttConfig();
  _client.setServer(mqttConfig.host, atoi(mqttConfig.port));

  Serial.printf("[MQTT] Attempting MQTT connection to %s on port %s as %s ...", mqttConfig.host, mqttConfig.port, mqttConfig.user);

  if (HAMQTT.connect(_clientId.c_str(), mqttConfig.user, mqttConfig.password)) {
    Serial.println(F("[MQTT] connected!"));

    _notifications.onMqttConnected();
    resetReconnectBackoff();
    updateStates();
  } else {
    registerReconnectFailure(now);
    _notifications.onMqttError();

    if (shouldDisableAfterReconnectFailures()) {
      Serial.println(F("[MQTT] Can not establish a connection, disabling MQTT."));
      _notifications.onMqttDisabled();
      _enabled = false;
      return;
    }

    Serial.print(F("[MQTT] failed, rc=")); Serial.print(_client.state()); Serial.printf(" try again in %d seconds\n", _reconnectTimeout / 1000);
  }
}

bool MqttService::shouldReconnect(uint32_t now) const {
  return _enabled && (now - _reconnectTiming > _reconnectTimeout);
}

void MqttService::resetReconnectBackoff() {
  _reconnectTimeout = 5000;
  _reconnectCount = 0;
}

void MqttService::registerReconnectFailure(uint32_t now) {
  _reconnectTiming = now;
  _reconnectCount += 1;
  _reconnectTimeout *= 2;
  if (_reconnectTimeout > 60000) _reconnectTimeout = 60000;
}

bool MqttService::shouldDisableAfterReconnectFailures() const {
  return _reconnectCount >= 9;
}

void MqttService::publishTimerCallback() {
  _haAudioAvailable.setState(_audio.frame().available ? "yes" : "no");

  _haUptime.setState(millis() / 1000);
  _haRssi.setState(WiFi.RSSI());
  _haRssiPct.setState(2 * (WiFi.RSSI() + 100));
  _haChannel.setState(WiFi.channel());
  _haVcc.setState(static_cast<float>(ESP.getVcc()) / 1000.0f);
  _haBootCount.setState(_settings.getBootCount());
  _haNotificationMuteState.setState(_notifications.isMutedNow() ? "muted" : "active");

  char resetReason[64];
  ESP.getResetReason().toCharArray(resetReason, sizeof(resetReason));
  _haResetReason.setState(resetReason);

  if (_settings.getBootCount() > 1) {
    _settings.resetBootCount();
  }
}

void MqttService::haCallback(HAEntity* entity, char* topic, byte* payload, unsigned int length) {
  if (_haLight.dispatchCommand(&_client, topic, payload, length)) {
    return;
  }

  if (entity == &_haRotationSwitch) {
    _rotation.setEnabled(_haRotationSwitch.getState());
    updateStates();
  } else if (entity == &_haRotationInterval) {
    _rotation.setIntervalSec(_haRotationInterval.getState() * 60U);
    updateStates();
  } else if (entity == &_haButtonSwitch) {
    _button.setEnabled(_haButtonSwitch.getState());
    updateStates();
  } else if (entity == &_haEffectScale) {
    _effects.setEffectScale(_haEffectScale.getState());
    updateStates();
  } else if (entity == &_haEffectSpeed) {
    _effects.setEffectSpeed(_haEffectSpeed.getState());
    updateStates();
  } else if (entity == &_haPalette) {
    onPaletteCommand(_haPalette.getState());
  } else if (entity == &_haAutoOff) {
    _power.setAutoOffMinutes(_haAutoOff.getState());
    updateStates();
  } else if (entity == &_haAudioMode) {
    _audio.setMode(parseAudioMode(_haAudioMode.getState()));
    updateStates();
  } else if (entity == &_haAudioBand) {
    _audio.setBand(parseAudioBand(_haAudioBand.getState()));
    updateStates();
  } else if (entity == &_haAudioAmount) {
    _audio.setAmount(_haAudioAmount.getState());
    updateStates();
  } else if (entity == &_haUserNotification) {
    const char* notification = _haUserNotification.getState();
    const uint32_t durationMs = static_cast<uint32_t>(_haUserNotificationDuration.getState()) * 1000UL;

    if (strcmp(notification, USER_NOTIFICATION_OFF) == 0) {
      _notifications.stopUserNotification();
    } else if (strcmp(notification, USER_NOTIFICATION_TEXT) == 0) {
      String text = String(_haUserNotificationText.getState()).substring(0, 64);
      _notifications.startUserTextNotification(text, CRGB::White, durationMs);
      _haUserNotificationText.setState("");
    } else if (strcmp(notification, USER_NOTIFICATION_WARNING) == 0) {
      _notifications.startUserNotification(UserNotificationType::Warning, durationMs);
    } else if (strcmp(notification, USER_NOTIFICATION_ALARM) == 0) {
      _notifications.startUserNotification(UserNotificationType::Alarm, durationMs);
    }

    updateStates();
  } else if (entity == &_haUserNotificationText) {
    const char* text = _haUserNotificationText.getState();
    const uint32_t durationMs = static_cast<uint32_t>(_haUserNotificationDuration.getState()) * 1000UL;

    if (strlen(text) == 0) {
      _notifications.stopUserNotification();
    } else {
      _notifications.startUserTextNotification(String(text).substring(0, 64), CRGB::White, durationMs);
      _haUserNotificationText.setState("");
    }

    updateStates();
  } else if (entity == &_haUserNotify) {
    _notifications.startUserNotification(UserNotificationType::Notify);
    updateStates();
  } else if (
    entity == &_haNotificationQuietHours ||
    entity == &_haNotificationQuietStart ||
    entity == &_haNotificationQuietEnd
    ) {
    NotificationQuietHours q = _notifications.getQuietHours();
    q.enabled = _haNotificationQuietHours.getState();
    uint16_t startMinutes = q.startMinutes;
    uint16_t endMinutes = q.endMinutes;
    if (parseHaTime(_haNotificationQuietStart.getState(), startMinutes)) {
      q.startMinutes = startMinutes;
    }
    if (parseHaTime(_haNotificationQuietEnd.getState(), endMinutes)) {
      q.endMinutes = endMinutes;
    }

    _notifications.setQuietHours(q);
    updateStates();
  }
}

void MqttService::onLightCommand(bool on, uint8_t brightness) {
  _power.setOn(on);
  _effects.setEffectBrightness(brightness);
  updateStates();
}

void MqttService::onEffectCommand(const char* effectName) {
  Serial.print(F("[MQTT] Command arrived: effect set to ")); Serial.println(effectName);

  Effects::Id effectId = Effects::getEffectId(effectName);
  if (effectId == Effects::Id::INVALID) return;

  _rotation.disable();
  _effects.setEffect(effectId);
  updateStates();
}

void MqttService::onPaletteCommand(const char* paletteName) {
  Serial.print(F("[MQTT] Command arrived: palette set to ")); Serial.println(paletteName);

  Palettes::Id paletteId = Palettes::parsePaletteName(paletteName);
  _effects.setPalette(paletteId);
  updateStates();
}

void MqttService::onColorCommand(uint8_t r, uint8_t g, uint8_t b) {
  Serial.print(F("[MQTT] Command arrived: rgb ")); Serial.print(r);
  Serial.print(','); Serial.print(g); Serial.print(','); Serial.println(b);

  _rotation.disable();
  _effects.setColor(r, g, b);
  _effects.setEffect(Effects::fallback());
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

void MqttService::init() {}
void MqttService::tick() {}
void MqttService::updateStates() {}

#endif
