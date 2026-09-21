#include <stdio.h>
#include <string.h>

#define private public
#include "core/lamp.h"
#undef private

#include "effect/palette_catalog.h"

namespace {

  int failures = 0;

  struct Calls {
    uint16_t powerOn;
    uint16_t powerOff;
    uint16_t effectNext;
    uint16_t effectPrevious;
    uint16_t rotationEnabled;
    uint16_t rotationDisabled;
    uint16_t parameterBrightness;
    uint16_t parameterSpeed;
    uint16_t parameterScale;
    uint16_t rotationManual;
    uint16_t setPalette;
    uint16_t resetEffectSettings;
    uint8_t brightness;
    uint8_t speed;
    uint8_t scale;
    Palettes::Id palette;
  } calls = {};

  StateNotifier* controllerStateNotifier = nullptr;
  EffectSettings testEffectSettings = {};

  void expect(bool condition, const char* name) {
    if (!condition) {
      ++failures;
      printf("FAIL: %s\n", name);
    }
  }

  uint16_t visualCalls() {
    return calls.powerOn + calls.powerOff + calls.effectNext + calls.effectPrevious + calls.rotationEnabled +
           calls.rotationDisabled + calls.parameterBrightness + calls.parameterSpeed + calls.parameterScale;
  }

  struct Fixture {
    alignas(PowerController) uint8_t powerStorage[sizeof(PowerController)] = {};
    alignas(EffectController) uint8_t effectsStorage[sizeof(EffectController)] = {};
    alignas(RotationController) uint8_t rotationStorage[sizeof(RotationController)] = {};
    alignas(SettingsRepository) uint8_t settingsStorage[sizeof(SettingsRepository)] = {};
    alignas(NotificationController) uint8_t notificationsStorage[sizeof(NotificationController)] = {};
    StateNotifier stateNotifier = {};
    PowerController& power = *reinterpret_cast<PowerController*>(powerStorage);
    EffectController& effects = *reinterpret_cast<EffectController*>(effectsStorage);
    RotationController& rotation = *reinterpret_cast<RotationController*>(rotationStorage);
    SettingsRepository& settings = *reinterpret_cast<SettingsRepository*>(settingsStorage);
    NotificationController& notifications = *reinterpret_cast<NotificationController*>(notificationsStorage);
    Lamp lamp{power, effects, rotation, settings, notifications, stateNotifier};

    void reset() {
      calls = Calls{};
      testEffectSettings = EffectSettings{};
      power.on_ = false;
      rotation.mode_ = RotationMode::Off;
      settings.globalBrightness_ = 128;
      stateNotifier.changed_ = false;
      controllerStateNotifier = &stateNotifier;
    }
  };

  bool dispatch(
    Fixture& fixture,
    ControlPadService::CommandType type,
    ControlPadService::ParameterTarget parameter = ControlPadService::ParameterTarget::None,
    int8_t steps = 0
  ) {
    const ControlPadService::CommandEvent event{type, parameter, steps};
    return Lamp::onControlPadCommand(event, &fixture.lamp);
  }

  void testLampControlPadOrchestration() {
    Fixture fixture;
    fixture.reset();

    expect(dispatch(fixture, ControlPadService::CommandType::TogglePower), "power-on event applies");
    expect(
      fixture.power.isOn() && calls.powerOn == 1 && fixture.stateNotifier.consumeChanged(),
      "power-on notifies and propagates state once"
    );
    expect(dispatch(fixture, ControlPadService::CommandType::TogglePower), "power-off event applies");
    expect(
      !fixture.power.isOn() && calls.powerOff == 1 && fixture.stateNotifier.consumeChanged(),
      "power-off notifies and propagates state once"
    );

    expect(dispatch(fixture, ControlPadService::CommandType::NextEffect), "next-effect event applies");
    expect(
      calls.rotationManual == 1 && calls.effectNext == 1 && fixture.stateNotifier.consumeChanged(),
      "next effect ends manual rotation, notifies, and propagates state"
    );
    expect(dispatch(fixture, ControlPadService::CommandType::PreviousEffect), "previous-effect event applies");
    expect(
      calls.rotationManual == 2 && calls.effectPrevious == 1 && fixture.stateNotifier.consumeChanged(),
      "previous effect ends manual rotation, notifies, and propagates state"
    );

    expect(dispatch(fixture, ControlPadService::CommandType::ToggleRotation), "rotation-enable event applies");
    expect(
      fixture.rotation.isActive() && calls.rotationEnabled == 1 && fixture.stateNotifier.consumeChanged(),
      "rotation enable reports post-state notification and controller state"
    );
    expect(dispatch(fixture, ControlPadService::CommandType::ToggleRotation), "rotation-disable event applies");
    expect(
      !fixture.rotation.isActive() && calls.rotationDisabled == 1 && fixture.stateNotifier.consumeChanged(),
      "rotation disable reports post-state notification and controller state"
    );

    const uint16_t visualsBeforeSilentEvents = visualCalls();
    expect(dispatch(fixture, ControlPadService::CommandType::NextPalette), "next-palette event applies");
    expect(calls.setPalette == 1 && fixture.stateNotifier.consumeChanged(), "next palette propagates state");
    expect(dispatch(fixture, ControlPadService::CommandType::SetPaletteAuto), "palette-auto event applies");
    expect(
      calls.palette == Palettes::Id::Auto && fixture.stateNotifier.consumeChanged(), "palette auto propagates state"
    );
    expect(dispatch(fixture, ControlPadService::CommandType::ResetCurrentEffectSettings), "effect-reset event applies");
    expect(calls.resetEffectSettings == 1 && fixture.stateNotifier.consumeChanged(), "effect reset propagates state");
    expect(visualCalls() == visualsBeforeSilentEvents, "palette and reset events stay visually silent");

    expect(
      dispatch(
        fixture, ControlPadService::CommandType::SelectParameter, ControlPadService::ParameterTarget::Brightness
      ),
      "brightness select event applies"
    );
    expect(
      calls.parameterBrightness == 1 && fixture.stateNotifier.consumeChanged(),
      "brightness select shows BRI and propagates state"
    );
    expect(
      dispatch(fixture, ControlPadService::CommandType::SelectParameter, ControlPadService::ParameterTarget::Speed),
      "speed select event applies"
    );
    expect(
      calls.parameterSpeed == 1 && fixture.stateNotifier.consumeChanged(), "speed select shows SPD and propagates state"
    );
    expect(
      dispatch(fixture, ControlPadService::CommandType::SelectParameter, ControlPadService::ParameterTarget::Scale),
      "scale select event applies"
    );
    expect(
      calls.parameterScale == 1 && fixture.stateNotifier.consumeChanged(), "scale select shows SCL and propagates state"
    );

    const uint16_t visualsBeforeAdjust = visualCalls();
    fixture.settings.globalBrightness_ = 250;
    expect(
      dispatch(
        fixture, ControlPadService::CommandType::AdjustParameter, ControlPadService::ParameterTarget::Brightness, 1
      ),
      "brightness adjustment applies"
    );
    expect(
      calls.brightness == 255 && fixture.stateNotifier.consumeChanged(),
      "brightness adjustment uses step factor and clamp"
    );
    testEffectSettings.speed = 5;
    expect(
      dispatch(fixture, ControlPadService::CommandType::AdjustParameter, ControlPadService::ParameterTarget::Speed, -1),
      "speed adjustment applies"
    );
    expect(calls.speed == 1 && fixture.stateNotifier.consumeChanged(), "speed adjustment uses step factor and clamp");
    testEffectSettings.scale = 250;
    expect(
      dispatch(fixture, ControlPadService::CommandType::AdjustParameter, ControlPadService::ParameterTarget::Scale, 1),
      "scale adjustment applies"
    );
    expect(calls.scale == 255 && fixture.stateNotifier.consumeChanged(), "scale adjustment uses step factor and clamp");
    expect(visualCalls() == visualsBeforeAdjust, "parameter adjustments stay visually silent");
  }

  void testPaletteSuccessor() {
    expect(
      Palettes::next(Palettes::Id::Auto) == Palettes::kSelectableOrder[0], "Auto advances to first selectable palette"
    );
    expect(
      Palettes::next(Palettes::kSelectableOrder[Palettes::kSelectableCount - 1]) == Palettes::kSelectableOrder[0],
      "last selectable palette wraps to first"
    );
    expect(
      Palettes::next(static_cast<Palettes::Id>(255)) == Palettes::kSelectableOrder[0],
      "non-selectable palette falls back to first selectable palette"
    );
  }

} // namespace

void PowerController::on() {
  on_ = true;
  controllerStateNotifier->stateChanged();
}

void PowerController::off() {
  on_ = false;
  controllerStateNotifier->stateChanged();
}

void EffectController::setNextEffect() {
}

void EffectController::setPreviousEffect() {
}

void EffectController::setGlobalBrightness(uint8_t value) {
  calls.brightness = value;
}

void EffectController::setEffectSpeed(uint8_t value) {
  calls.speed = value;
}

void EffectController::setEffectScale(uint8_t value) {
  calls.scale = value;
}

void EffectController::setPalette(Palettes::Id palette) {
  ++calls.setPalette;
  calls.palette = palette;
}

Palettes::Id EffectController::selectedPalette() const {
  return Palettes::Id::Auto;
}

void EffectController::resetCurrentEffectSettingsToDefaults() {
  ++calls.resetEffectSettings;
}

void RotationController::onManualRotation() {
  ++calls.rotationManual;
}

void RotationController::setMode(RotationMode mode) {
  mode_ = mode;
  controllerStateNotifier->stateChanged();
}

void RotationController::disable() {
  mode_ = RotationMode::Off;
  controllerStateNotifier->stateChanged();
}

EffectSettings& SettingsRepository::effectSettings(Effects::Id) {
  return testEffectSettings;
}

void NotificationController::startUserTextNotification(const String& text, const CRGB&, uint32_t) {
  if (strcmp(text.c_str(), "BRI") == 0) ++calls.parameterBrightness;
  else if (strcmp(text.c_str(), "SPD") == 0)
    ++calls.parameterSpeed;
  else if (strcmp(text.c_str(), "SCL") == 0)
    ++calls.parameterScale;
}

void NotificationController::onButtonPowerOn() {
  ++calls.powerOn;
}

void NotificationController::onButtonPowerOff() {
  ++calls.powerOff;
}

void NotificationController::onEffectNext() {
  ++calls.effectNext;
}

void NotificationController::onEffectPrevious() {
  ++calls.effectPrevious;
}

void NotificationController::onRotationEnabled() {
  ++calls.rotationEnabled;
}

void NotificationController::onRotationDisabled() {
  ++calls.rotationDisabled;
}

int main() {
  testPaletteSuccessor();
  testLampControlPadOrchestration();
  if (failures != 0) {
    printf("FAILED: %d lamp Control Pad orchestration assertion(s)\n", failures);
    return 1;
  }
  printf("PASS: lamp Control Pad command orchestration fixture\n");
  return 0;
}
