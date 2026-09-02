#include <SettingsAsync.h>
#include <uptime_formatter.h>

#include "../audio/audio_config.h"
#include "../audio/audio_service.h"
#include "../core/auto_off_config.h"
#include "../core/power_controller.h"
#include "../core/rotation_controller.h"
#include "../core/rotation_presets.h"
#include "../core/state_notifier.h"
#include "../effect/catalog.h"
#include "../effect/controller.h"
#include "../effect/palette_catalog.h"
#include "../hardware/button.h"
#include "../network/mqtt_service.h"
#include "../network/wifi_service.h"
#include "../notification/controller.h"
#include "../notification/quiet_hours.h"
#include "../notification/types.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "../time/time_service.h"
#include "../util/loop_profiler.h"
#include "web_service.h"

namespace {
  constexpr uint32_t kSecondsPerDay = 24UL * 60UL * 60UL;

  uint32_t minutesToSeconds(uint16_t minutes) {
    return static_cast<uint32_t>(minutes) * 60UL;
  }
  uint16_t secondsToMinutes(uint32_t seconds) {
    seconds %= kSecondsPerDay;
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

} // namespace

void WebService::init() {
  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  if (strlen(wifiConfig.ssid) > 0) {
    strlcpy(inputWifiSsid_, wifiConfig.ssid, kWifiSsidLen);
    strlcpy(inputWifiPass_, wifiConfig.password, kWifiPassLen);
  }

  const MqttConfig& mqttConfig = eeprom_.readMqttConfig();
  if (strlen(mqttConfig.host) > 0) {
    strlcpy(inputMqttHost_, mqttConfig.host, kMqttHostLen);
    strlcpy(inputMqttPort_, mqttConfig.port, kMqttPortLen);
    strlcpy(inputMqttUser_, mqttConfig.user, kMqttUserLen);
    strlcpy(inputMqttPass_, mqttConfig.password, kMqttPassLen);
  }

  webSettings_.begin(true, wifi_.getDeviceId().c_str());
  webSettings_.onBuild([this](sets::Builder& b) { settingsBuilder(b); });
  webSettings_.onUpdate([this](sets::Updater& upd) { settingsUpdate(upd); });
  webSettings_.setTitle(DEVICE_NAME);
  webSettings_.setVersion(FIRMWARE_VERSION);

  effectOptions_ = "";
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    if (i > 0) effectOptions_ += ';';
    effectOptions_ += Effects::getEffectName(Effects::kDisplayOrder[i]);
  }

  paletteOptions_ = Palettes::getPaletteName(Palettes::Id::Auto);
  for (uint8_t i = 0; i < Palettes::kSelectableCount; i++) {
    paletteOptions_ += ';';
    paletteOptions_ += Palettes::getPaletteName(Palettes::kSelectableOrder[i]);
  }

  rotationModeOptions_ = "Off;Sequential;Random";
  rotationIntervalOptions_ = String(kRotationPresetLabels[0]);
  for (uint8_t i = 1; i < kRotationPresetCount; i++) {
    rotationIntervalOptions_ += ';';
    rotationIntervalOptions_ += kRotationPresetLabels[i];
  }
}

void WebService::tick() {
  webSettings_.tick();
}

uint8_t WebService::effectDisplayIndex(Effects::Id id) const {
  for (uint8_t i = 0; i < Effects::kDisplayCount; i++) {
    if (Effects::kDisplayOrder[i] == id) return i;
  }
  return 0;
}

uint8_t WebService::paletteDisplayIndex(Palettes::Id id) const {
  if (id == Palettes::Id::Auto) return 0;
  for (uint8_t i = 0; i < Palettes::kSelectableCount; i++) {
    if (Palettes::kSelectableOrder[i] == id) return i + 1;
  }
  return 0;
}

void WebService::settingsBuilder(sets::Builder& b) {
  {
    sets::Group g(b, "Lamp");

    if (b.build.isBuild()) {
      powerOn_ = power_.isOn();
      selectedEffectIndex_ = effectDisplayIndex(effects_.getSelectedEffectId());
      selectedPaletteIndex_ = paletteDisplayIndex(effects_.getSelectedPalette());
      buttonEnabled_ = button_.isEnabled();
      globalBrightness_ = settings_.getGlobalBrightness();
    }

    if (b.Switch("Power", &powerOn_)) {
      power_.setOn(powerOn_);
      stateNotifier_.stateChanged();
    }

    if (b.Slider("Brightness", 0, 255, 1, "", &globalBrightness_)) {
      effects_.setGlobalBrightness(globalBrightness_);
      stateNotifier_.stateChanged();
    }

    if (b.Select("Effect", effectOptions_, &selectedEffectIndex_)) {
      rotation_.disable();
      Effects::Id effectId = Effects::kDisplayOrder[selectedEffectIndex_];
      if (!effects_.setEffect(effectId)) {
        effects_.setEffect(Effects::fallback());
      }
      notifications_.onEffectNext();
      stateNotifier_.stateChanged();
      b.reload();
    }

    {
      sets::Buttons g(b);

      if (b.Button("Prev")) {
        effects_.setPreviousEffect();
        notifications_.onEffectPrevious();
        rotation_.onManualRotation();
        selectedEffectIndex_ = effectDisplayIndex(effects_.getSelectedEffectId());
        stateNotifier_.stateChanged();
        b.reload();
      }

      if (b.Button("Next")) {
        effects_.setNextEffect();
        notifications_.onEffectNext();
        rotation_.onManualRotation();
        selectedEffectIndex_ = effectDisplayIndex(effects_.getSelectedEffectId());
        stateNotifier_.stateChanged();
        b.reload();
      }

      if (b.Button("Random")) {
        effects_.setRandomEffect();
        notifications_.onEffectNext();
        rotation_.onManualRotation();
        selectedEffectIndex_ = effectDisplayIndex(effects_.getSelectedEffectId());
        stateNotifier_.stateChanged();
        b.reload();
      }
    }

    if (b.Select("Palette", paletteOptions_, &selectedPaletteIndex_)) {
      Palettes::Id paletteId = Palettes::Id::Auto;
      if (selectedPaletteIndex_ > 0 && selectedPaletteIndex_ - 1 < Palettes::kSelectableCount) {
        paletteId = Palettes::kSelectableOrder[selectedPaletteIndex_ - 1];
      }
      effects_.setPalette(paletteId);
      stateNotifier_.stateChanged();
      b.reload();
    }

    if (b.Switch("Touch button", &buttonEnabled_)) {
      button_.setEnabled(buttonEnabled_);
      stateNotifier_.stateChanged();
    }
  }
  {
    sets::Group g(b, "Effect");

    if (b.build.isBuild()) {
      const EffectSettings& settings = settings_.getEffectSettings(effects_.getSelectedEffectId());
      brightness_ = settings.brightness;
      speed_ = settings.speed;
      scale_ = settings.scale;
      color_ = ((uint32_t)effects_.getRed() << 16) | ((uint32_t)effects_.getGreen() << 8) | effects_.getBlue();
    }

    if (b.Slider("Brightness", 0, 255, 1, "", &brightness_)) {
      effects_.setEffectBrightness(brightness_);
      stateNotifier_.stateChanged();
    }

    if (b.Slider("Speed", 0, 255, 1, "", &speed_)) {
      effects_.setEffectSpeed(speed_);
      stateNotifier_.stateChanged();
    }

    if (b.Slider("Scale", 0, 255, 1, "", &scale_)) {
      effects_.setEffectScale(scale_);
      stateNotifier_.stateChanged();
    }

    if (b.Color("Color", &color_)) {
      rotation_.disable();
      effects_.setColor((color_ >> 16) & 0xFF, (color_ >> 8) & 0xFF, color_ & 0xFF);
      stateNotifier_.stateChanged();
    }

    if (b.Button("Reset all effect settings")) {
      effects_.resetEffectSettingsToDefaults();
      stateNotifier_.stateChanged();
      b.reload();
    }

    if (b.Button("Reset current effect settings")) {
      effects_.resetCurrentEffectSettingsToDefaults();
      stateNotifier_.stateChanged();
      b.reload();
    }
  }
  {
    sets::Group g(b, "Rotation");

    if (b.build.isBuild()) {
      rotationModeIndex_ = static_cast<uint8_t>(rotation_.getMode());
      rotationIntervalPresetIndex_ = rotationPresetIndexForSeconds(rotation_.getIntervalSec());
    }

    if (b.Select("Rotation", rotationModeOptions_, &rotationModeIndex_)) {
      const bool wasActive = rotation_.isActive();
      rotation_.setMode(static_cast<RotationMode>(rotationModeIndex_));
      if (rotation_.isActive() && !wasActive) {
        notifications_.onRotationEnabled();
      } else if (!rotation_.isActive() && wasActive) {
        notifications_.onRotationDisabled();
      }
      stateNotifier_.stateChanged();
    }

    if (b.Select("Rotation interval", rotationIntervalOptions_, &rotationIntervalPresetIndex_)) {
      rotation_.setIntervalSec(rotationPresetSecondsForIndex(rotationIntervalPresetIndex_));
      stateNotifier_.stateChanged();
    }
  }
  {
    sets::Group g(b, "Auto off");

    if (b.build.isBuild()) {
      autoOffMinutes_ = power_.getAutoOffMinutes();
    }

    if (b.Number("Auto-off, min", &autoOffMinutes_, kAutoOffMinutesMin, kAutoOffMinutesMax)) {
      power_.setAutoOffMinutes(autoOffMinutes_);
      stateNotifier_.stateChanged();
    }
    b.Label("Auto-off remaining", formatRemainingTime(power_.getAutoOffRemainingSeconds()));
  }
  {
    sets::Group g(b, "Quiet mode");

    if (b.build.isBuild()) {
      loadNotificationQuietHoursForUi();
    }

    if (b.Switch("Quiet hours", &notificationQuietEnabled_)) {
      saveNotificationQuietHoursFromUi();
    }
    if (b.Time("Mute from", &notificationQuietStartSeconds_)) {
      saveNotificationQuietHoursFromUi();
    }
    if (b.Time("Mute to", &notificationQuietEndSeconds_)) {
      saveNotificationQuietHoursFromUi();
    }
    b.Label("Muted now", notifications_.isMutedNow() ? "yes" : "no");
    b.Label("Notification remaining", formatRemainingTime(notifications_.getUserNotificationRemainingSeconds()));
  }
  {
    sets::Group g(b, "Audio");

    if (b.build.isBuild()) {
      const AudioConfig& config = audio_.config();
      audioModeIndex_ = static_cast<uint8_t>(config.mode);
      audioBandIndex_ = static_cast<uint8_t>(config.band);
      audioAmount_ = config.amount;
    }

    if (b.Select("Audio mode", audioModeOptions_, &audioModeIndex_)) {
      audio_.setMode(static_cast<AudioMode>(audioModeIndex_));
      stateNotifier_.stateChanged();
    }

    if (b.Select("Audio band", audioBandOptions_, &audioBandIndex_)) {
      audio_.setBand(static_cast<AudioBand>(audioBandIndex_));
      stateNotifier_.stateChanged();
    }

    if (b.Slider("Audio amount", 0, 255, 1, "", &audioAmount_)) {
      audio_.setAmount(audioAmount_);
      stateNotifier_.stateChanged();
    }

    const AudioFrame& audio = audio_.frame();
    b.Label("Audio available", audio.available ? "yes" : "no");
    b.Label("Audio level/bass/treble", String(audio.level) + "/" + String(audio.bass) + "/" + String(audio.treble));
    b.Label("Audio beat", audio.beat ? "yes" : "no");
  }
  {
    sets::Menu g(b, "WiFi");
    b.Input("SSID", inputWifiSsid_);
    b.Pass("Password", inputWifiPass_);
    if (b.Button("Save and restart")) {
      eeprom_.writeWifiConfig(inputWifiSsid_, inputWifiPass_);
      ESP.restart();
    }
  }
  {
    sets::Menu g(b, "MQTT");
    b.Input("Host", inputMqttHost_);
    b.Number("Port", inputMqttPort_);
    b.Input("User", inputMqttUser_);
    b.Pass("Password", inputMqttPass_);
    if (b.Button("Save and restart")) {
      eeprom_.writeMqttConfig(inputMqttHost_, inputMqttPort_, inputMqttUser_, inputMqttPass_);
      ESP.restart();
    }
  }
  {
    sets::Menu g(b, "Information");

    uint16_t vcc = ESP.getVcc();
    bool vccAvailable = (vcc != 0 && vcc >= 2500 && vcc <= 3700);

    b.Label("Lamp ID", String(ESP.getChipId(), HEX));
    b.Label("Device ID", wifi_.getDeviceId());
    b.Label("Wi-Fi", WiFi.SSID());
    b.Label("WiFi RSSI", String(2 * (WiFi.RSSI() + 100)) + "%");
    b.Label("IP Local", WiFi.localIP().toString());
    b.Label("IP Gateway", WiFi.gatewayIP().toString());
    b.Label("MAC", WiFi.macAddress());
    b.Label("Wi-Fi channel", String(WiFi.channel()));
    b.Label("Wi-Fi RSSI", String(WiFi.RSSI()));
    b.Label("Wi-Fi RSSI %", String(2 * (WiFi.RSSI() + 100)));
    if (vccAvailable) b.Label("VCC", String(static_cast<float>(ESP.getVcc()) / 1000.0f));
    b.Label("Reset reason", ESP.getResetReason());
    b.Label("Core version", ESP.getCoreVersion());
    b.Label("CPU freq", String(ESP.getCpuFreqMHz()) + " MHz");
    b.Label("Sketch size", String(ESP.getSketchSize() / 1024) + " kb");
    b.Label("Flash size", String(ESP.getFlashChipSize() / 1024 / 8) + " kb");
    b.Label("Free sketch space", String(ESP.getFreeSketchSpace() / 1024 / 8) + " kb");
    b.Label("Free heap", String(ESP.getFreeHeap() / 1024) + "kb");
    b.Label("Max free block size", String(ESP.getMaxFreeBlockSize() / 1024) + "kb");
    b.Label("Heap fragmentaion", String(ESP.getHeapFragmentation()) + "%");
    b.Label("MQTT host", String(eeprom_.readMqttConfig().host));
    b.Label("MQTT connection", mqtt_.isMqttConnected() ? "on" : "off");
    b.Label("Uptime", uptime_formatter::getUptime());
    b.Label("Time", time_.getTimeStampString());

    if (b.Button("Restart")) {
      webSettings_.reload();
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
        notifications_.onWifiConnecting();
      }
      if (b.Button("Connected")) {
        notifications_.onWifiConnected();
      }
      if (b.Button("Error")) {
        notifications_.onWifiError();
      }
      if (b.Button("Disabled")) {
        notifications_.onWifiDisabled();
      }
    }
    {
      sets::Row g(b, "MQTT");
      if (b.Button("Connecting")) {
        notifications_.onMqttConnecting();
      }
      if (b.Button("Connected")) {
        notifications_.onMqttConnected();
      }
      if (b.Button("Error")) {
        notifications_.onMqttError();
      }
      if (b.Button("Disabled")) {
        notifications_.onMqttDisabled();
      }
    }
    {
      sets::Row g(b, "OTA");
      if (b.Button("Start")) {
        notifications_.onOtaStart();
      }
      if (b.Button("Progress")) {
        notifications_.onOtaProgress(35U);
      }
      if (b.Button("Error")) {
        notifications_.onOtaError();
      }
      if (b.Button("End")) {
        notifications_.onOtaEnd();
      }
    }

    {
      sets::Group g(b, "Button");
      {
        sets::Row g(b);
        if (b.Button("Press")) {
          notificationButtonCount_ = notificationButtonCount_ >= 6 ? 0 : notificationButtonCount_ + 1;
          notifications_.onButtonPress(notificationButtonCount_);
        }
        if (b.Button("Release")) {
          notifications_.onButtonRelease();
        }
        if (b.Button("Dismiss")) {
          notifications_.onButtonDismiss();
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Power on")) {
          notifications_.onButtonPowerOn();
        }
        if (b.Button("Power off")) {
          notifications_.onButtonPowerOff();
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Next Effect")) {
          notifications_.onEffectNext();
        }
        if (b.Button("Prev Effect")) {
          notifications_.onEffectPrevious();
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Brightness increasing")) {
          notifications_.onButtonBrightness(77, true);
        }
        if (b.Button("Brightness decreasing")) {
          notifications_.onButtonBrightness(180, false);
        }
      }
      {
        sets::Row g(b);
        if (b.Button("Rotation enabled")) {
          notifications_.onRotationEnabled();
        }
        if (b.Button("Rotation disabled")) {
          notifications_.onRotationDisabled();
        }
      }
    }

    {
      sets::Group g(b, "Warning");

      b.Number("Warning duration, s", &notificationWarningDurationSec_, 0, 3600);
      if (b.Button("Start warning")) {
        notifications_.startUserNotification(
          UserNotificationType::Warning, static_cast<uint32_t>(notificationWarningDurationSec_ * 1000)
        );
      }
    }

    {
      sets::Group g(b, "Alarm");

      b.Number("Alarm duration, s", &notificationAlarmDurationSec_, 0, 3600);
      if (b.Button("Start alarm")) {
        notifications_.startUserNotification(
          UserNotificationType::Alarm, static_cast<uint32_t>(notificationAlarmDurationSec_ * 1000)
        );
      }
    }

    {
      sets::Group g(b, "Text");

      b.Input("Notification text", notificationText_);
      b.Number("Text duration, s", &notificationTextDurationSec_, 0, 3600);
      if (b.Button("Start text")) {
        notifications_.startUserTextNotification(
          String(notificationText_).substring(0, 64),
          CRGB::White,
          static_cast<uint32_t>(notificationTextDurationSec_) * 1000UL
        );
        notificationText_[0] = '\0';
        b.reload();
      }
    }

    if (b.Button("Notify")) {
      notifications_.startUserNotification(UserNotificationType::Notify);
    }

    if (b.Button("Stop user notification")) {
      notifications_.stopUserNotification();
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
  const NotificationQuietHours& q = notifications_.getQuietHours();
  notificationQuietEnabled_ = q.enabled;
  notificationQuietStartSeconds_ = minutesToSeconds(q.startMinutes);
  notificationQuietEndSeconds_ = minutesToSeconds(q.endMinutes);
}

bool WebService::saveNotificationQuietHoursFromUi() {
  NotificationQuietHours q;
  q.enabled = notificationQuietEnabled_;
  q.startMinutes = secondsToMinutes(notificationQuietStartSeconds_);
  q.endMinutes = secondsToMinutes(notificationQuietEndSeconds_);

  if (!notifications_.setQuietHours(q)) return false;

  stateNotifier_.stateChanged();
  return true;
}
