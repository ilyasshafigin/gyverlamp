#include <Arduino.h>
#include <uButton.h>
#include <uEncButton.h>

#include "panel_pins.h"
#include "panel_radio_logic.h"

#ifdef PANEL_PROVISIONED
#include "panel_credentials.h"
#endif

namespace {

  constexpr uint16_t kShortClickMaxMs = 350;

  uEncButton encoder(PanelPins::ENCODER_A, PanelPins::ENCODER_B, PanelPins::ENCODER_SWITCH, INPUT_PULLUP, INPUT_PULLUP);
  uButton button1(PanelPins::BUTTON_1);
  uButton button2(PanelPins::BUTTON_2);
  uButton button3(PanelPins::BUTTON_3);
  uButton button4(PanelPins::BUTTON_4);
  uButton button5(PanelPins::BUTTON_5);

  PanelRadio::PairingCombo pairingCombo;
  uint32_t encoderPressedAt = 0;
  bool encoderShortClick = false;

  struct ButtonInput {
    uButton& button;
    const char* name;
    int pin;
    const char* action;
    uint32_t pressedAt;
    bool shortClick;

    ButtonInput(uButton& button, const char* name, int pin, const char* action)
      : button(button),
        name(name),
        pin(pin),
        action(action),
        pressedAt(0),
        shortClick(false) {}
  };

  ButtonInput buttons[] = {
    {button1, "BUTTON 1", PanelPins::BUTTON_1, "power/reset effect"},
    {button2, "BUTTON 2", PanelPins::BUTTON_2, "next effect"},
    {button3, "BUTTON 3", PanelPins::BUTTON_3, "previous effect"},
    {button4, "BUTTON 4", PanelPins::BUTTON_4, "NoOp"},
    {button5, "BUTTON 5", PanelPins::BUTTON_5, "rotation toggle"},
  };

  void printCommandLockout(const char* input, const char* action) {
    Serial.printf("INPUT %s: %s (command locked; radio disabled)\n", input, action);
  }

  void printInitialInput(const char* name, int pin, bool pressed) {
    Serial.printf(
      "INITIAL %s GPIO%d: %s (%s)\n", name, pin, pressed ? "LOW" : "HIGH", pressed ? "PRESSED" : "RELEASED"
    );
  }

  void reportButton(ButtonInput& input) {
    if (input.button.press()) {
      input.pressedAt = millis();
      input.shortClick = false;
      Serial.printf("INPUT %s: pressed\n", input.name);
    }
    if (input.button.release()) {
      const uint32_t duration = millis() - input.pressedAt;
      input.shortClick = duration <= kShortClickMaxMs;
      Serial.printf(
        "INPUT %s: released after %lums (%s)\n",
        input.name,
        static_cast<unsigned long>(duration),
        input.shortClick ? "short candidate" : "no short action"
      );
    }
    if (input.button.hold()) {
      Serial.printf("INPUT %s: hold (command locked; radio disabled)\n", input.name);
    }
    if (input.button.hasClicks(1) && input.shortClick) {
      printCommandLockout(input.name, input.action);
    }
  }

  void printStartup() {
    Serial.println();
    Serial.println("ESP32-C3 control-pad production input diagnostics");
    Serial.println("Input events are command-locked; Wi-Fi and ESP-NOW are disabled.");
    Serial.printf("UX: debounce=%ums hold=%ums\n", UB_DEB_TIME, UB_HOLD_TIME);
    Serial.printf("SHORT CLICK MAX: %ums\n", kShortClickMaxMs);
    Serial.printf("MULTI-CLICK WINDOW: %ums\n", UB_CLICK_TIME);
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
    for (const ButtonInput& input : buttons) {
      printInitialInput(input.name, input.pin, input.button.readButton());
    }
    printInitialInput("ENCODER SWITCH", PanelPins::ENCODER_SWITCH, encoder.readButton());
    Serial.printf(
      "INITIAL ENCODER A/B: GPIO%d=%s GPIO%d=%s\n",
      PanelPins::ENCODER_A,
      digitalRead(PanelPins::ENCODER_A) == LOW ? "LOW" : "HIGH",
      PanelPins::ENCODER_B,
      digitalRead(PanelPins::ENCODER_B) == LOW ? "LOW" : "HIGH"
    );
    Serial.println("STATUS LED: disabled by panel_input build flag.");
    Serial.println("Ready: production input events are reported without sending commands.");
  }

  enum class EncoderMode : uint8_t {
    Brightness,
    Speed,
    Scale,
  };

  EncoderMode encoderMode = EncoderMode::Brightness;

  const char* encoderModeName() {
    switch (encoderMode) {
      case EncoderMode::Brightness: return "Brightness";
      case EncoderMode::Speed: return "Speed";
      case EncoderMode::Scale: return "Scale";
    }
    return "unknown";
  }

  void advanceEncoderMode() {
    switch (encoderMode) {
      case EncoderMode::Brightness: encoderMode = EncoderMode::Speed; break;
      case EncoderMode::Speed: encoderMode = EncoderMode::Scale; break;
      case EncoderMode::Scale: encoderMode = EncoderMode::Brightness; break;
    }
  }

  void reportEncoder() {
    if (encoder.press()) {
      encoderPressedAt = millis();
      encoderShortClick = false;
      Serial.println("ENCODER SWITCH: pressed");
    }
    if (encoder.release()) {
      const uint32_t duration = millis() - encoderPressedAt;
      encoderShortClick = duration <= kShortClickMaxMs;
      Serial.printf(
        "ENCODER SWITCH: released after %lums (%s)\n",
        static_cast<unsigned long>(duration),
        encoderShortClick ? "short candidate" : "no mode change"
      );
    }
    if (encoder.turn() && !encoder.pressing() && !encoder.encHolding()) {
      Serial.printf(
        "ENCODER: %s detent in %s mode (command locked; radio disabled)\n",
        encoder.dir() > 0 ? "CW" : "CCW",
        encoderModeName()
      );
    }
    if (encoder.hasClicks(1) && encoderShortClick) {
      advanceEncoderMode();
      Serial.printf("ENCODER SWITCH: mode %s (command locked; radio disabled)\n", encoderModeName());
    }
    if (encoder.hold()) {
      Serial.println("ENCODER SWITCH: hold feedback (command locked; radio disabled)");
    }
  }

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  printStartup();
}

void loop() {
  encoder.tick();
  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();
  button5.tick();

  reportEncoder();
  for (ButtonInput& input : buttons) {
    reportButton(input);
  }
  if (pairingCombo.tick(millis(), button1.pressing(), button3.pressing())) {
    Serial.println("BUTTON 1 + BUTTON 3: pairing requested (locked; radio disabled)");
  }
}
