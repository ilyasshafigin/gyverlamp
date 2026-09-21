#include <Arduino.h>
#include <uButton.h>
#include <uEncButton.h>

#include "panel_credentials.h"
#include "panel_pins.h"
#include "panel_radio_espnow.h"

namespace {

  using namespace PanelRadio;

  constexpr uint16_t kShortClickMaxMs = 350;

#define PANEL_ACTION_NO_ACTION 0
#define PANEL_ACTION_TOGGLE_POWER 1
#define PANEL_ACTION_NEXT_EFFECT 2
#define PANEL_ACTION_PREVIOUS_EFFECT 3
#define PANEL_ACTION_TOGGLE_ROTATION 4
#define PANEL_ACTION_NEXT_PALETTE 7
#define PANEL_ACTION_SET_PALETTE_AUTO 8
#define PANEL_ACTION_RESET_CURRENT_EFFECT_SETTINGS 9

#ifndef PANEL_B1_SINGLE
#define PANEL_B1_SINGLE PANEL_ACTION_TOGGLE_POWER
#endif
#ifndef PANEL_B1_DOUBLE
#define PANEL_B1_DOUBLE PANEL_ACTION_RESET_CURRENT_EFFECT_SETTINGS
#endif
#ifndef PANEL_B1_TRIPLE
#define PANEL_B1_TRIPLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B1_HOLD
#define PANEL_B1_HOLD PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B2_SINGLE
#define PANEL_B2_SINGLE PANEL_ACTION_NEXT_EFFECT
#endif
#ifndef PANEL_B2_DOUBLE
#define PANEL_B2_DOUBLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B2_TRIPLE
#define PANEL_B2_TRIPLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B2_HOLD
#define PANEL_B2_HOLD PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B3_SINGLE
#define PANEL_B3_SINGLE PANEL_ACTION_PREVIOUS_EFFECT
#endif
#ifndef PANEL_B3_DOUBLE
#define PANEL_B3_DOUBLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B3_TRIPLE
#define PANEL_B3_TRIPLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B3_HOLD
#define PANEL_B3_HOLD PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B4_SINGLE
#define PANEL_B4_SINGLE PANEL_ACTION_NEXT_PALETTE
#endif
#ifndef PANEL_B4_DOUBLE
#define PANEL_B4_DOUBLE PANEL_ACTION_SET_PALETTE_AUTO
#endif
#ifndef PANEL_B4_TRIPLE
#define PANEL_B4_TRIPLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B4_HOLD
#define PANEL_B4_HOLD PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B5_SINGLE
#define PANEL_B5_SINGLE PANEL_ACTION_TOGGLE_ROTATION
#endif
#ifndef PANEL_B5_DOUBLE
#define PANEL_B5_DOUBLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B5_TRIPLE
#define PANEL_B5_TRIPLE PANEL_ACTION_NO_ACTION
#endif
#ifndef PANEL_B5_HOLD
#define PANEL_B5_HOLD PANEL_ACTION_NO_ACTION
#endif

  MillisClock clockSource;
  EspRandom randomSource;
  EspNowPanelTransport transport;
  PreferencesBindingStore bindingStore;
  FrozenProtocol protocol(PanelCredentials::K_PAIR);
  PanelRadioRuntime* runtime = nullptr;

  uEncButton encoder(PanelPins::ENCODER_A, PanelPins::ENCODER_B, PanelPins::ENCODER_SWITCH, INPUT_PULLUP, INPUT_PULLUP);
  uButton button1(PanelPins::BUTTON_1);
  uButton button2(PanelPins::BUTTON_2);
  uButton button3(PanelPins::BUTTON_3);
  uButton button4(PanelPins::BUTTON_4);
  uButton button5(PanelPins::BUTTON_5);

  struct Button {
    uButton& input;
    CommandCode single;
    CommandCode doubleClick;
    CommandCode triple;
    CommandCode hold;
    uint32_t pressedAt;
    bool shortClick;
  };

  Button buttons[] = {
    {button1,
     static_cast<CommandCode>(PANEL_B1_SINGLE),
     static_cast<CommandCode>(PANEL_B1_DOUBLE),
     static_cast<CommandCode>(PANEL_B1_TRIPLE),
     static_cast<CommandCode>(PANEL_B1_HOLD),
     0,
     false},
    {button2,
     static_cast<CommandCode>(PANEL_B2_SINGLE),
     static_cast<CommandCode>(PANEL_B2_DOUBLE),
     static_cast<CommandCode>(PANEL_B2_TRIPLE),
     static_cast<CommandCode>(PANEL_B2_HOLD),
     0,
     false},
    {button3,
     static_cast<CommandCode>(PANEL_B3_SINGLE),
     static_cast<CommandCode>(PANEL_B3_DOUBLE),
     static_cast<CommandCode>(PANEL_B3_TRIPLE),
     static_cast<CommandCode>(PANEL_B3_HOLD),
     0,
     false},
    {button4,
     static_cast<CommandCode>(PANEL_B4_SINGLE),
     static_cast<CommandCode>(PANEL_B4_DOUBLE),
     static_cast<CommandCode>(PANEL_B4_TRIPLE),
     static_cast<CommandCode>(PANEL_B4_HOLD),
     0,
     false},
    {button5,
     static_cast<CommandCode>(PANEL_B5_SINGLE),
     static_cast<CommandCode>(PANEL_B5_DOUBLE),
     static_cast<CommandCode>(PANEL_B5_TRIPLE),
     static_cast<CommandCode>(PANEL_B5_HOLD),
     0,
     false},
  };

  uint32_t encoderPressedAt = 0;
  bool encoderShortClick = false;
  PairingCombo pairingCombo;
  uint32_t pairButtonsConsumedUntil = 0;
  ParameterTarget selectedTarget = ParameterTarget::Brightness;
  Telemetry lastTelemetry = {};
  bool telemetryPrinted = false;

#ifdef DEBUG
  void debugAction(const char* action, unsigned index);
#endif

  bool isCommandAction(CommandCode command) {
    switch (command) {
      case CommandCode::TogglePower:
      case CommandCode::NextEffect:
      case CommandCode::PreviousEffect:
      case CommandCode::ToggleRotation:
      case CommandCode::NextPalette:
      case CommandCode::SetPaletteAuto:
      case CommandCode::ResetCurrentEffectSettings: return true;
      case CommandCode::NoAction:
      case CommandCode::SelectParameter:
      case CommandCode::AdjustParameter: return false;
    }
    return false;
  }

  bool pairingButton(const Button& button) {
    return &button.input == &button1 || &button.input == &button3;
  }

  void enqueueButtonCommand(const Button& button, CommandCode command, uint8_t index, uint32_t now) {
    if (!isCommandAction(command)) {
#ifdef DEBUG
      debugAction("B no-op", index + 1);
#endif
      return;
    }
    if (pairingButton(button) && static_cast<int32_t>(now - pairButtonsConsumedUntil) < 0) {
#ifdef DEBUG
      debugAction("B consumed", index + 1);
#endif
      return;
    }
#ifdef DEBUG
    Serial.printf("ACT B%u command=%u\n", static_cast<unsigned>(index + 1), static_cast<unsigned>(command));
#endif
    runtime->enqueue(command, ParameterTarget::None, 0);
  }

#ifdef DEBUG
  void debugAction(const char* action, unsigned index) {
    Serial.printf("ACT %s=%u\n", action, index);
  }
#endif

  bool pairingTelemetryChanged(const Telemetry& telemetry) {
    return !telemetryPrinted || telemetry.state != lastTelemetry.state ||
           telemetry.pairAcceptDecoded != lastTelemetry.pairAcceptDecoded ||
           telemetry.pairAcceptDecodeFailed != lastTelemetry.pairAcceptDecodeFailed ||
           telemetry.pairAcceptAuthFailed != lastTelemetry.pairAcceptAuthFailed ||
           telemetry.pairAcceptGuardRejected != lastTelemetry.pairAcceptGuardRejected ||
           telemetry.encryptedPeerFailed != lastTelemetry.encryptedPeerFailed ||
           telemetry.confirmEncodeFailed != lastTelemetry.confirmEncodeFailed ||
           telemetry.confirmEnqueueAttempts != lastTelemetry.confirmEnqueueAttempts ||
           telemetry.confirmEnqueueFailed != lastTelemetry.confirmEnqueueFailed ||
           telemetry.txSucceeded != lastTelemetry.txSucceeded || telemetry.txFailed != lastTelemetry.txFailed;
  }

  void printPairingTelemetry() {
    const Telemetry telemetry = runtime->telemetry();
    if (!pairingTelemetryChanged(telemetry)) return;
    Serial.printf(
      "PAIR runtime=confirm-clock-order-v1 state=%u accept ok=%lu decode=%lu auth=%lu guard=%lu peer=%lu "
      "confirm=%lu/%lu encode=%lu tx=%lu/%lu\n",
      static_cast<unsigned>(telemetry.state),
      static_cast<unsigned long>(telemetry.pairAcceptDecoded),
      static_cast<unsigned long>(telemetry.pairAcceptDecodeFailed),
      static_cast<unsigned long>(telemetry.pairAcceptAuthFailed),
      static_cast<unsigned long>(telemetry.pairAcceptGuardRejected),
      static_cast<unsigned long>(telemetry.encryptedPeerFailed),
      static_cast<unsigned long>(telemetry.confirmEnqueueAttempts),
      static_cast<unsigned long>(telemetry.confirmEnqueueFailed),
      static_cast<unsigned long>(telemetry.confirmEncodeFailed),
      static_cast<unsigned long>(telemetry.txSucceeded),
      static_cast<unsigned long>(telemetry.txFailed)
    );
    lastTelemetry = telemetry;
    telemetryPrinted = true;
  }

  void tickButtons() {
    const uint32_t now = millis();
    for (uint8_t index = 0; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
      Button& button = buttons[index];
      if (button.input.press()) {
        button.pressedAt = now;
        button.shortClick = false;
#ifdef DEBUG
        debugAction("B press", index + 1);
#endif
      }
      if (button.input.release()) button.shortClick = !elapsed(now, button.pressedAt, kShortClickMaxMs + 1);
    }
    if (button1.pressing() && button3.pressing()) pairButtonsConsumedUntil = now + 400;
    if (pairingCombo.tick(now, button1.pressing(), button3.pressing())) {
#ifdef DEBUG
      Serial.println("ACT combo=B1+B3 pairing=start");
#endif
      runtime->startPairing();
    }
    for (uint8_t index = 0; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
      Button& button = buttons[index];
      if (button.input.hold()) enqueueButtonCommand(button, button.hold, index, now);
      if (!button.shortClick) continue;
      if (button.input.hasClicks(3)) enqueueButtonCommand(button, button.triple, index, now);
      else if (button.input.hasClicks(2))
        enqueueButtonCommand(button, button.doubleClick, index, now);
      else if (button.input.hasClicks(1))
        enqueueButtonCommand(button, button.single, index, now);
    }
  }

  void tickEncoder() {
    const uint32_t now = millis();
    if (encoder.press()) {
      encoderPressedAt = now;
      encoderShortClick = false;
#ifdef DEBUG
      Serial.println("ACT ENC press");
#endif
    }
    if (encoder.release()) encoderShortClick = !elapsed(now, encoderPressedAt, kShortClickMaxMs + 1);
    if (encoder.turn()) {
      if (!encoder.pressing() && !encoder.encHolding()) {
#ifdef DEBUG
        Serial.printf("ACT ENC rotation=%d\n", static_cast<int>(encoder.dir()));
#endif
        runtime->onEncoderRawTurn(encoder.dir());
      } else {
#ifdef DEBUG
        Serial.println("ACT ENC rotation=consumed switch");
#endif
      }
    }
    if (encoder.hasClicks(2) && encoderShortClick) {
      selectedTarget = ParameterTarget::Brightness;
      const bool queued = runtime->selectParameter(selectedTarget);
#ifdef DEBUG
      Serial.printf("ACT ENC mode=%u queue=%s\n", static_cast<unsigned>(selectedTarget), queued ? "ok" : "reject");
#endif
    } else if (encoder.hasClicks(1) && encoderShortClick) {
      selectedTarget =
        selectedTarget == ParameterTarget::Brightness
          ? ParameterTarget::Speed
          : (selectedTarget == ParameterTarget::Speed ? ParameterTarget::Scale : ParameterTarget::Brightness);
      const bool queued = runtime->selectParameter(selectedTarget);
#ifdef DEBUG
      Serial.printf("ACT ENC mode=%u queue=%s\n", static_cast<unsigned>(selectedTarget), queued ? "ok" : "reject");
#endif
    }
  }

  void printBoot() {
    Serial.println("ESP32-C3 control-pad radio");
    Serial.println("PANEL_RADIO_RUNTIME=confirm-clock-order-v1");
    Serial.printf(
      "BUTTON 1..5: GPIO%d, GPIO%d, GPIO%d, GPIO%d, GPIO%d\n",
      PanelPins::BUTTON_1,
      PanelPins::BUTTON_2,
      PanelPins::BUTTON_3,
      PanelPins::BUTTON_4,
      PanelPins::BUTTON_5
    );
    Serial.printf(
      "ENCODER A/B/SW: GPIO%d, GPIO%d, GPIO%d\n", PanelPins::ENCODER_A, PanelPins::ENCODER_B, PanelPins::ENCODER_SWITCH
    );
    Serial.printf(
      "INITIAL BUTTON 1..5: %s, %s, %s, %s, %s\n",
      button1.readButton() ? "LOW" : "HIGH",
      button2.readButton() ? "LOW" : "HIGH",
      button3.readButton() ? "LOW" : "HIGH",
      button4.readButton() ? "LOW" : "HIGH",
      button5.readButton() ? "LOW" : "HIGH"
    );
    Serial.printf("INITIAL ENCODER SWITCH: %s\n", encoder.readButton() ? "LOW" : "HIGH");
    Serial.printf(
      "INITIAL ENCODER A/B: %s/%s\n",
      digitalRead(PanelPins::ENCODER_A) == LOW ? "LOW" : "HIGH",
      digitalRead(PanelPins::ENCODER_B) == LOW ? "LOW" : "HIGH"
    );
    Serial.println(PanelPins::STATUS_LED < 0 ? "STATUS LED: disabled" : "STATUS LED: not driven");
  }

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  runtime = new PanelRadioRuntime(clockSource, randomSource, transport, bindingStore, protocol);
  runtime->begin();
  printBoot();
}

void loop() {
  encoder.tick();
  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();
  button5.tick();
  tickButtons();
  tickEncoder();
  runtime->tick();
  printPairingTelemetry();
}
