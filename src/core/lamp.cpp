#include "lamp.h"

#ifdef USE_CONTROL_PAD
#include "../effect/palette_catalog.h"

namespace {

  uint8_t clampControlPadParameter(int value) {
    if (value < 1) return 1;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
  }

} // namespace
#endif

#if !defined(CONTROL_PAD_LAMP_HOST_TEST)
void Lamp::setup() {
  led.init();

  Serial.println();
  delay(1000);

  if (!eeprom.init()) {
    led.blackout();
    safeMode_ = true;
    Serial.println(F("[SAFE MODE] EEPROM initialization failed; settings services disabled"));
    return;
  }
  connectivity.load();
  settings.init();
  audio.init();
  notifications.init();
  effects.init();
  power.init();
  button.init();

  wifi.begin(connectivity.wifiRuntimeConfig(), onWifiEvent, this);
  upd.init();
  const WifiController::Snapshot wifiSnapshot = wifi.snapshot();
  const OtaController::Config otaConfig{wifiSnapshot.deviceId, 0, connectivity.otaEnabled(), nullptr, nullptr};
  ota.begin(otaConfig, onOtaEvent, this);
  mqtt.init(connectivity.mqttConfig());
  time.init();
#ifdef USE_CONTROL_PAD
  controlPad.setCommandHandler(onControlPadCommand, this);
  controlPad.init();
#endif
  web.init();
  rotation.init();
}

void Lamp::loop() {
  if (safeMode_) {
    yield();
    return;
  }

  LoopProfiler::measure(LoopProfiler::LOOP, [this]() {
    LoopProfiler::measure(LoopProfiler::WIFI, [this]() { wifi.tick(); });
    LoopProfiler::measure(LoopProfiler::OTA, [this]() { ota.tick(wifi.staConnected()); });

    power.tick();

    LoopProfiler::measure(LoopProfiler::ROTATION, [this]() { rotation.tick(power.isOn()); });

    const AudioMode audioMode = audio.config().mode;
    bool audioReadEnabled = power.isOn() && audioMode != AudioMode::Off;
    if (audioReadEnabled && audioMode == AudioMode::Effect) {
      const EffectSettingsSpec activeEffectSpec = effects.activeSettingsSpec();
      audioReadEnabled = (activeEffectSpec.flags & Effects::EFFECT_SPEC_USES_AUDIO) != 0;
    }
    LoopProfiler::measure(LoopProfiler::AUDIO, [this, audioReadEnabled]() { audio.tick(audioReadEnabled); });
    yield();
    LoopProfiler::measure(LoopProfiler::RENDER, [this]() { frameRenderer.render(); });
    LoopProfiler::measure(LoopProfiler::SETTINGS, [this]() { settings.tick(effects.activeEffectId()); });
    LoopProfiler::measure(LoopProfiler::TIME, [this]() { time.tick(); });
    LoopProfiler::measure(LoopProfiler::BUTTON, [this]() { button.tick(); });
    LoopProfiler::measure(LoopProfiler::UDP, [this]() { upd.tick(); });
#ifdef USE_CONTROL_PAD
    controlPad.tick();
#endif
    LoopProfiler::measure(LoopProfiler::WEB, [this]() { web.tick(); });
    yield();
    LoopProfiler::measure(LoopProfiler::MQTT, [this]() { mqtt.tick(); });

    if (stateNotifier.consumeChanged()) {
      mqtt.updateStates();
    }
  });

  LoopProfiler::tick();
  yield();
}

void Lamp::onWifiEvent(const WifiController::Event& event, void* context) {
  auto* lamp = static_cast<Lamp*>(context);
  switch (event.type) {
    case WifiController::EventType::Connecting: lamp->notifications.onWifiConnecting(); break;
    case WifiController::EventType::Connected: lamp->notifications.onWifiConnected(); break;
    case WifiController::EventType::Error: lamp->notifications.onWifiError(); break;
    case WifiController::EventType::Disabled: lamp->notifications.onWifiDisabled(); break;
  }
}

void Lamp::onOtaEvent(const OtaController::Event& event, void* context) {
  auto* lamp = static_cast<Lamp*>(context);
  switch (event.type) {
    case OtaController::EventType::Start:
      lamp->notifications.onOtaStart();
      lamp->frameRenderer.renderNow();
      break;
    case OtaController::EventType::Progress: lamp->notifications.onOtaProgress(event.progress);
#ifdef OTA_FORCE_RENDER
      lamp->frameRenderer.renderNow();
#endif
      break;
    case OtaController::EventType::End: lamp->notifications.onOtaEnd(); break;
    case OtaController::EventType::Error:
      lamp->notifications.onOtaError();
      lamp->frameRenderer.renderNow();
      break;
  }
}
#endif

#ifdef USE_CONTROL_PAD
bool Lamp::onControlPadCommand(const ControlPadService::CommandEvent& event, void* context) {
  auto* lamp = static_cast<Lamp*>(context);
  switch (event.type) {
    case ControlPadService::CommandType::TogglePower: {
      const bool wasOn = lamp->power.isOn();
      lamp->power.toggle();
      if (!wasOn && lamp->power.isOn()) lamp->notifications.onButtonPowerOn();
      else if (wasOn && !lamp->power.isOn())
        lamp->notifications.onButtonPowerOff();
      return true;
    }
    case ControlPadService::CommandType::NextEffect:
      lamp->rotation.onManualRotation();
      lamp->effects.setNextEffect();
      lamp->notifications.onEffectNext();
      lamp->stateNotifier.stateChanged();
      return true;
    case ControlPadService::CommandType::PreviousEffect:
      lamp->rotation.onManualRotation();
      lamp->effects.setPreviousEffect();
      lamp->notifications.onEffectPrevious();
      lamp->stateNotifier.stateChanged();
      return true;
    case ControlPadService::CommandType::ToggleRotation: {
      const bool wasActive = lamp->rotation.isActive();
      lamp->rotation.setEnabled(!wasActive);
      if (lamp->rotation.isActive() && !wasActive) lamp->notifications.onRotationEnabled();
      else if (!lamp->rotation.isActive() && wasActive)
        lamp->notifications.onRotationDisabled();
      return true;
    }
    case ControlPadService::CommandType::NextPalette:
      lamp->effects.setPalette(Palettes::next(lamp->effects.selectedPalette()));
      lamp->stateNotifier.stateChanged();
      return true;
    case ControlPadService::CommandType::SetPaletteAuto:
      lamp->effects.setPalette(Palettes::Id::Auto);
      lamp->stateNotifier.stateChanged();
      return true;
    case ControlPadService::CommandType::ResetCurrentEffectSettings:
      lamp->effects.resetCurrentEffectSettingsToDefaults();
      lamp->stateNotifier.stateChanged();
      return true;
    case ControlPadService::CommandType::SelectParameter:
      switch (event.parameter) {
        case ControlPadService::ParameterTarget::Brightness:
          lamp->notifications.startUserTextNotification("BRI", CRGB::White, 1200);
          break;
        case ControlPadService::ParameterTarget::Speed:
          lamp->notifications.startUserTextNotification("SPD", CRGB::White, 1200);
          break;
        case ControlPadService::ParameterTarget::Scale:
          lamp->notifications.startUserTextNotification("SCL", CRGB::White, 1200);
          break;
        case ControlPadService::ParameterTarget::None: return false;
      }
      lamp->stateNotifier.stateChanged();
      return true;
    case ControlPadService::CommandType::AdjustParameter: {
      const int delta = static_cast<int>(event.steps) * 15;
      switch (event.parameter) {
        case ControlPadService::ParameterTarget::Brightness:
          lamp->effects.setGlobalBrightness(
            clampControlPadParameter(static_cast<int>(lamp->settings.globalBrightness()) + delta)
          );
          break;
        case ControlPadService::ParameterTarget::Speed:
          lamp->effects.setEffectSpeed(clampControlPadParameter(
            static_cast<int>(lamp->settings.effectSettings(lamp->effects.selectedEffectId()).speed) + delta
          ));
          break;
        case ControlPadService::ParameterTarget::Scale:
          lamp->effects.setEffectScale(clampControlPadParameter(
            static_cast<int>(lamp->settings.effectSettings(lamp->effects.selectedEffectId()).scale) + delta
          ));
          break;
        case ControlPadService::ParameterTarget::None: return false;
      }
      lamp->stateNotifier.stateChanged();
      return true;
    }
  }
  return false;
}
#endif
