#include <SettingsAsync.h>
#include <uptime_formatter.h>

#include "../audio/audio_config.h"
#include "../audio/audio_service.h"
#include "../core/auto_off_config.h"
#include "../core/power_controller.h"
#include "../core/rotation_controller.h"
#include "../core/state_notifier.h"
#include "../effect/catalog.h"
#include "../effect/controller.h"
#include "../effect/palette_catalog.h"
#include "../hardware/button.h"
#include "../network/mqtt_service.h"
#include "../network/wifi_service.h"
#include "../notification/controller.h"
#include "../notification/types.h"
#include "../notification/quiet_hours.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "../time/time_service.h"
#include "../util/loop_profiler.h"
#include "web_service.h"

namespace {
  constexpr uint32_t SECONDS_PER_DAY = 24UL * 60UL * 60UL;

  uint32_t minutesToSeconds(uint16_t minutes) { return static_cast<uint32_t>(minutes) * 60UL; }
  uint16_t secondsToMinutes(uint32_t seconds) {
    seconds %= SECONDS_PER_DAY;
    return static_cast<uint16_t>(seconds / 60UL);
  }

  String formatRemainingTime(uint32_t seconds) {
    if (seconds == 0) return "-";

    const uint32_t hours = seconds / 3600UL;
    const uint32_t minutes = (seconds % 3600UL) / 60UL;
    const uint32_t secs = seconds % 60UL;

    if (hours > 0) {
      return String(hours) + "h " + String(minutes) + "m";
    }

    if (minutes > 0) {
      return String(minutes) + "m " + String(secs) + "s";
    }

    return String(secs) + "s";
  }

}

void WebService::init() {
  const WifiConfig& wifiConfig = _eeprom.readWifiConfig();
  if (strlen(wifiConfig.ssid) > 0) {
    strlcpy(_inputWifiSsid, wifiConfig.ssid, WIFI_SSID_LEN);
    strlcpy(_inputWifiPass, wifiConfig.password, WIFI_PASS_LEN);
  }

  const MqttConfig& mqttConfig = _eeprom.readMqttConfig();
  if (strlen(mqttConfig.host) > 0) {
    strlcpy(_inputMqttHost, mqttConfig.host, MQTT_HOST_LEN);
    strlcpy(_inputMqttPort, mqttConfig.port, MQTT_PORT_LEN);
    strlcpy(_inputMqttUser, mqttConfig.user, MQTT_USER_LEN);
    strlcpy(_inputMqttPass, mqttConfig.password, MQTT_PASS_LEN);
  }

  _webSettings.begin(true, _wifi.getDeviceId().c_str());
  _webSettings.onBuild([this](sets::Builder& b) { settingsBuilder(b); });
  _webSettings.onUpdate([this](sets::Updater& upd) { settingsUpdate(upd); });
  _webSettings.setTitle(DEVICE_NAME);
  _webSettings.setVersion(FIRMWARE_VERSION);

  _effectOptions = "";
  for (uint8_t i = 0; i < Effects::DISPLAY_COUNT; i++) {
    if (i > 0) _effectOptions += ';';
    _effectOptions += Effects::getEffectName(Effects::DISPLAY_ORDER[i]);
  }

  _paletteOptions = Palettes::getPaletteName(Palettes::Id::Auto);
  for (uint8_t i = 0; i < Palettes::SELECTABLE_COUNT; i++) {
    _paletteOptions += ';';
    _paletteOptions += Palettes::getPaletteName(Palettes::SELECTABLE_ORDER[i]);
  }

  _rotationModeOptions = "Off;Sequential;Random";
}

void WebService::tick() {
  _webSettings.tick();
}

uint8_t WebService::effectDisplayIndex(Effects::Id id) const {
  for (uint8_t i = 0; i < Effects::DISPLAY_COUNT; i++) {
    if (Effects::DISPLAY_ORDER[i] == id) return i;
  }
  return 0;
}

uint8_t WebService::paletteDisplayIndex(Palettes::Id id) const {
  if (id == Palettes::Id::Auto) return 0;
  for (uint8_t i = 0; i < Palettes::SELECTABLE_COUNT; i++) {
    if (Palettes::SELECTABLE_ORDER[i] == id) return i + 1;
  }
  return 0;
}

void WebService::settingsBuilder(sets::Builder& b) {
  {
    sets::Group g(b, "Settings");

    if (b.build.isBuild()) {
      _powerOn = _power.isOn();
      _rotationModeIndex = static_cast<uint8_t>(_rotation.getMode());
      _rotationIntervalMin = (_rotation.getIntervalSec() + 59) / 60;
      _buttonEnabled = _button.isEnabled();
      _selectedEffectIndex = effectDisplayIndex(_effects.getSelectedEffectId());
      _selectedPaletteIndex = paletteDisplayIndex(_effects.getSelectedPalette());
      const EffectSettings& settings = _settings.getEffectSettings(_effects.getSelectedEffectId());
      _brightness = settings.brightness;
      _speed = settings.speed;
      _scale = settings.scale;
      _color = ((uint32_t)_effects.getRed() << 16) | ((uint32_t)_effects.getGreen() << 8) | _effects.getBlue();
    }

    if (b.Switch("Power", &_powerOn)) {
      _power.setOn(_powerOn);
      _stateNotifier.stateChanged();
    }

    if (b.Switch("Touch button", &_buttonEnabled)) {
      _button.setEnabled(_buttonEnabled);
      _stateNotifier.stateChanged();
    }

    if (b.Select("Effect", _effectOptions, &_selectedEffectIndex)) {
      _rotation.disable();
      Effects::Id effectId = Effects::DISPLAY_ORDER[_selectedEffectIndex];
      if (!_effects.setEffect(effectId)) {
        _effects.setEffect(Effects::fallback());
      }
      _stateNotifier.stateChanged();
      b.reload();
    }

    if (b.Select("Palette", _paletteOptions, &_selectedPaletteIndex)) {
      Palettes::Id paletteId = Palettes::Id::Auto;
      if (_selectedPaletteIndex > 0 && _selectedPaletteIndex - 1 < Palettes::SELECTABLE_COUNT) {
        paletteId = Palettes::SELECTABLE_ORDER[_selectedPaletteIndex - 1];
      }
      _effects.setPalette(paletteId);
      _stateNotifier.stateChanged();
      b.reload();
    }

    if (b.Slider("Brightness", 0, 255, 1, "", &_brightness)) {
      _effects.setEffectBrightness(_brightness);
      _stateNotifier.stateChanged();
    }

    if (b.Slider("Speed", 0, 255, 1, "", &_speed)) {
      _effects.setEffectSpeed(_speed);
      _stateNotifier.stateChanged();
    }

    if (b.Slider("Scale", 0, 255, 1, "", &_scale)) {
      _effects.setEffectScale(_scale);
      _stateNotifier.stateChanged();
    }

    if (b.Color("Color", &_color)) {
      _rotation.disable();
      _effects.setColor((_color >> 16) & 0xFF, (_color >> 8) & 0xFF, _color & 0xFF);
      _stateNotifier.stateChanged();
    }

    b.Label("Notification remaining", formatRemainingTime(_notifications.getUserNotificationRemainingSeconds()));

    if (b.Button("Reset effect settings")) {
      _effects.resetEffectSettingsToDefaults();
      _stateNotifier.stateChanged();
      b.reload();
    }
  }
  {
    sets::Group g(b, "Rotation");

    if (b.build.isBuild()) {
      _rotationModeIndex = static_cast<uint8_t>(_rotation.getMode());
      _rotationIntervalMin = (_rotation.getIntervalSec() + 59) / 60;
    }

    if (b.Select("Rotation", _rotationModeOptions, &_rotationModeIndex)) {
      _rotation.setMode(static_cast<RotationMode>(_rotationModeIndex));
      _stateNotifier.stateChanged();
    }

    if (b.Number("Rotation interval, min", &_rotationIntervalMin, ROTATION_INTERVAL_MIN_MIN, ROTATION_INTERVAL_MIN_MAX)) {
      _rotation.setIntervalSec(_rotationIntervalMin * 60U);
      _stateNotifier.stateChanged();
    }
  }
  {
    sets::Group g(b, "Auto off");

    if (b.build.isBuild()) {
      _autoOffMinutes = _power.getAutoOffMinutes();
    }

    if (b.Number("Auto-off, min", &_autoOffMinutes, AUTO_OFF_MINUTES_MIN, AUTO_OFF_MINUTES_MAX)) {
      _power.setAutoOffMinutes(_autoOffMinutes);
      _stateNotifier.stateChanged();
    }
    b.Label("Auto-off remaining", formatRemainingTime(_power.getAutoOffRemainingSeconds()));
  }
  {
    sets::Group g(b, "Quiet mode");

    if (b.build.isBuild()) {
      loadNotificationQuietHoursForUi();
    }

    if (b.Switch("Quiet hours", &_notificationQuietEnabled)) {
      saveNotificationQuietHoursFromUi();
    }
    if (b.Time("Mute from", &_notificationQuietStartSeconds)) {
      saveNotificationQuietHoursFromUi();
    }
    if (b.Time("Mute to", &_notificationQuietEndSeconds)) {
      saveNotificationQuietHoursFromUi();
    }
    b.Label("Muted now", _notifications.isMutedNow() ? "yes" : "no");
  }
  {
    sets::Group g(b, "Audio");

    if (b.build.isBuild()) {
      const AudioConfig& config = _audio.config();
      _audioModeIndex = static_cast<uint8_t>(config.mode);
      _audioBandIndex = static_cast<uint8_t>(config.band);
      _audioAmount = config.amount;
    }

    if (b.Select("Audio mode", _audioModeOptions, &_audioModeIndex)) {
      _audio.setMode(static_cast<AudioMode>(_audioModeIndex));
      _stateNotifier.stateChanged();
    }

    if (b.Select("Audio band", _audioBandOptions, &_audioBandIndex)) {
      _audio.setBand(static_cast<AudioBand>(_audioBandIndex));
      _stateNotifier.stateChanged();
    }

    if (b.Slider("Audio amount", 0, 255, 1, "", &_audioAmount)) {
      _audio.setAmount(_audioAmount);
      _stateNotifier.stateChanged();
    }

    const AudioFrame& audio = _audio.frame();
    b.Label("Audio available", audio.available ? "yes" : "no");
    b.Label("Audio level/bass/treble", String(audio.level) + "/" + String(audio.bass) + "/" + String(audio.treble));
    b.Label("Audio beat", audio.beat ? "yes" : "no");
  }
  {
    sets::Menu g(b, "WiFi");
    b.Input("SSID", _inputWifiSsid);
    b.Pass("Password", _inputWifiPass);
    if (b.Button("Save and restart")) {
      _eeprom.writeWifiConfig(_inputWifiSsid, _inputWifiPass);
      ESP.restart();
    }
  }
  {
    sets::Menu g(b, "MQTT");
    b.Input("Host", _inputMqttHost);
    b.Number("Port", _inputMqttPort);
    b.Input("User", _inputMqttUser);
    b.Pass("Password", _inputMqttPass);
    if (b.Button("Save and restart")) {
      _eeprom.writeMqttConfig(_inputMqttHost, _inputMqttPort, _inputMqttUser, _inputMqttPass);
      ESP.restart();
    }
  }
  {
    sets::Menu g(b, "Information");

    b.Label("Lamp ID", String(ESP.getChipId(), HEX));
    b.Label("Device ID", _wifi.getDeviceId());
    b.Label("Wi-Fi", WiFi.SSID());
    b.Label("WiFi RSSI", String(2 * (WiFi.RSSI() + 100)) + "%");
    b.Label("IP Local", WiFi.localIP().toString());
    b.Label("IP Gateway", WiFi.gatewayIP().toString());
    b.Label("MAC", WiFi.macAddress());
    b.Label("Wi-Fi channel", String(WiFi.channel()));
    b.Label("Reset reason", ESP.getResetReason());
    b.Label("Core version", ESP.getCoreVersion());
    b.Label("CPU freq", String(ESP.getCpuFreqMHz()) + " MHz");
    b.Label("Sketch size", String(ESP.getSketchSize() / 1024) + " kb");
    b.Label("Flash size", String(ESP.getFlashChipSize() / 1024 / 8) + " kb");
    b.Label("Free sketch space", String(ESP.getFreeSketchSpace() / 1024 / 8) + " kb");
    b.Label("Free heap", String(ESP.getFreeHeap() / 1024) + "kb");
    b.Label("Max free block size", String(ESP.getMaxFreeBlockSize() / 1024) + "kb");
    b.Label("Heap fragmentaion", String(ESP.getHeapFragmentation()) + "%");
    b.Label("MQTT host", String(_eeprom.readMqttConfig().host));
    b.Label("MQTT connection", _mqtt.isMqttConnected() ? "on" : "off");
    b.Label("Uptime", uptime_formatter::getUptime());
    b.Label("Time", _time.getTimeStampString());

    if (b.Button("Restart")) {
      _webSettings.reload();
      delay(2000);
      ESP.restart();
    }
  }

#ifdef TEST_NOTIFICATIONS
  {
    sets::Menu g(b, "Notification Test");

    if (b.build.isBuild()) {
      // Не обязательно сбрасывать значения каждый build.
      // Пусть select показывает последнее выбранное тестовое значение.
    }

    {
      sets::Row g(b, "WiFi");
      if (b.Button("Connecting")) {
        _notifications.onWifiConnecting();
      }
      if (b.Button("Connected")) {
        _notifications.onWifiConnected();
      }
      if (b.Button("Error")) {
        _notifications.onWifiError();
      }
      if (b.Button("Disabled")) {
        _notifications.onWifiDisabled();
      }
    }
    {
      sets::Row g(b, "MQTT");
      if (b.Button("Connecting")) {
        _notifications.onMqttConnecting();
      }
      if (b.Button("Connected")) {
        _notifications.onMqttConnected();
      }
      if (b.Button("Error")) {
        _notifications.onMqttError();
      }
      if (b.Button("Disabled")) {
        _notifications.onMqttDisabled();
      }
    }
    {
      sets::Row g(b, "OTA");
      if (b.Button("Start")) {
        _notifications.onOtaStart();
      }
      if (b.Button("Progress")) {
        _notifications.onOtaProgress(35U);
      }
      if (b.Button("Error")) {
        _notifications.onOtaError();
      }
      if (b.Button("End")) {
        _notifications.onOtaEnd();
      }
    }

    {
      sets::Group g(b, "Button");
      {
        sets::Row g(b);
        if (b.Button("Press")) {
          _notificationButtonCount = _notificationButtonCount >= 6 ? 0 : _notificationButtonCount + 1;
          _notifications.onButtonPress(_notificationButtonCount);
        }
        if (b.Button("Release")) {
          _notifications.onButtonRelease();
        }
        if (b.Button("Dismiss")) {
          _notifications.onButtonDismiss();
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Power on")) {
          _notifications.onButtonPowerOn();
        }
        if (b.Button("Power off")) {
          _notifications.onButtonPowerOff();
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Next Effect")) {
          _notifications.onButtonNextEffect();
        }
        if (b.Button("Prev Effect")) {
          _notifications.onButtonPreviousEffect();
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Brightness increasing")) {
          _notifications.onButtonBrightness(77, true);
        }
        if (b.Button("Brightness decreasing")) {
          _notifications.onButtonBrightness(180, false);
        }
      }
    }

    {
      sets::Group g(b, "Warning");

      b.Number("Warning duration, s", &_notificationWarningDurationSec, 0, 3600);
      if (b.Button("Start warning")) {
        _notifications.startUserNotification(UserNotificationType::Warning, static_cast<uint32_t>(_notificationWarningDurationSec * 1000));
      }
    }

    {
      sets::Group g(b, "Alarm");

      b.Number("Alarm duration, s", &_notificationAlarmDurationSec, 0, 3600);
      if (b.Button("Start alarm")) {
        _notifications.startUserNotification(UserNotificationType::Alarm, static_cast<uint32_t>(_notificationAlarmDurationSec * 1000));
      }
    }

    {
      sets::Group g(b, "Text");

      b.Input("Notification text", _notificationText);
      b.Number("Text duration, s", &_notificationTextDurationSec, 0, 3600);
      if (b.Button("Start text")) {
        _notifications.startUserTextNotification(
          String(_notificationText).substring(0, 64),
          CRGB::White,
          static_cast<uint32_t>(_notificationTextDurationSec) * 1000UL
        );
        _notificationText[0] = '\0';
        b.reload();
      }
    }

    if (b.Button("Notify")) {
      _notifications.startUserNotification(UserNotificationType::Notify);
    }

    if (b.Button("Stop user notification")) {
      _notifications.stopUserNotification();
      b.reload();
    }
  }
#endif

#ifdef PROFILE_LOOP
  {
    sets::Menu g(b, "Profiler (last/max us)");

    for (uint8_t i = 0; i < LoopProfiler::SECTION_COUNT; i++) {
      LoopProfiler::Section section = static_cast<LoopProfiler::Section>(i);
      const LoopProfiler::Sample& sample = LoopProfiler::get(section);
      b.Label(String(LoopProfiler::sectionName(section)), String(sample.lastUs) + " / " + String(sample.maxUs));
    }
  }
#endif
}

void WebService::settingsUpdate(sets::Updater& u) {
}

void WebService::loadNotificationQuietHoursForUi() {
  const NotificationQuietHours& q = _notifications.getQuietHours();
  _notificationQuietEnabled = q.enabled;
  _notificationQuietStartSeconds = minutesToSeconds(q.startMinutes);
  _notificationQuietEndSeconds = minutesToSeconds(q.endMinutes);
}

bool WebService::saveNotificationQuietHoursFromUi() {
  NotificationQuietHours q;
  q.enabled = _notificationQuietEnabled;
  q.startMinutes = secondsToMinutes(_notificationQuietStartSeconds);
  q.endMinutes = secondsToMinutes(_notificationQuietEndSeconds);

  if (!_notifications.setQuietHours(q)) return false;

  _stateNotifier.stateChanged();
  return true;
}
