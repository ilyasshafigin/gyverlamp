#include <Arduino.h>

#include "panel_pins.h"

namespace {

  constexpr uint32_t kDebounceMs = 25;

  struct DebouncedInput {
    const char* name;
    int pin;
    int stableLevel;
    int candidateLevel;
    uint32_t candidateSince;
  };

  DebouncedInput buttons[] = {
    {"BUTTON 1", PanelPins::BUTTON_1, HIGH, HIGH, 0},
    {"BUTTON 2", PanelPins::BUTTON_2, HIGH, HIGH, 0},
    {"BUTTON 3", PanelPins::BUTTON_3, HIGH, HIGH, 0},
    {"BUTTON 4", PanelPins::BUTTON_4, HIGH, HIGH, 0},
    {"BUTTON 5", PanelPins::BUTTON_5, HIGH, HIGH, 0},
    {"ENCODER SWITCH", PanelPins::ENCODER_SWITCH, HIGH, HIGH, 0},
  };

  uint8_t encoderState = 0;
  int8_t encoderTransitions = 0;

  void printInputEdge(const DebouncedInput& input) {
    Serial.printf(
      "%s GPIO%d: %s (raw=%s)\n",
      input.name,
      input.pin,
      input.stableLevel == LOW ? "PRESSED" : "RELEASED",
      input.stableLevel == LOW ? "LOW" : "HIGH"
    );
  }

  void printInputInitialLevel(const DebouncedInput& input) {
    Serial.printf(
      "INITIAL %s GPIO%d: %s (%s)\n",
      input.name,
      input.pin,
      input.stableLevel == LOW ? "LOW" : "HIGH",
      input.stableLevel == LOW ? "PRESSED" : "RELEASED"
    );
  }

  void updateInput(DebouncedInput& input, uint32_t now) {
    const int rawLevel = digitalRead(input.pin);

    if (rawLevel != input.candidateLevel) {
      input.candidateLevel = rawLevel;
      input.candidateSince = now;
    }

    if (input.candidateLevel != input.stableLevel && now - input.candidateSince >= kDebounceMs) {
      input.stableLevel = input.candidateLevel;
      printInputEdge(input);
    }
  }

  uint8_t readEncoderState() {
    return (digitalRead(PanelPins::ENCODER_A) == HIGH ? 0b10 : 0) |
           (digitalRead(PanelPins::ENCODER_B) == HIGH ? 0b01 : 0);
  }

  void updateEncoder() {
    static constexpr int8_t kTransitionDelta[] = {
      0,
      -1,
      1,
      0,
      1,
      0,
      0,
      -1,
      -1,
      0,
      0,
      1,
      0,
      1,
      -1,
      0,
    };

    const uint8_t currentState = readEncoderState();
    if (currentState == encoderState) {
      return;
    }

    const int8_t transition = kTransitionDelta[(encoderState << 2) | currentState];
    encoderState = currentState;

    if (transition == 0) {
      encoderTransitions = 0;
      return;
    }

    encoderTransitions += transition;
    if (encoderTransitions >= 4 || encoderTransitions <= -4) {
      const int direction = encoderTransitions > 0 ? 1 : -1;
      Serial.printf(
        "ENCODER GPIO%d/GPIO%d: step %+d (%s leads)\n",
        PanelPins::ENCODER_A,
        PanelPins::ENCODER_B,
        direction,
        direction > 0 ? "A" : "B"
      );
      encoderTransitions = 0;
    }
  }

  void printStartup() {
    Serial.println();
    Serial.println("ESP32-C3 control-pad wiring diagnostics");
    Serial.println("Inputs use INPUT_PULLUP: pressed/active means LOW.");
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
      "STATUS LED: GPIO%d, active %s (not configured or driven)\n",
      PanelPins::STATUS_LED,
      PanelPins::STATUS_LED_ACTIVE_HIGH ? "HIGH" : "LOW"
    );
    for (const DebouncedInput& input : buttons) {
      printInputInitialLevel(input);
    }
    Serial.println("Ready: press buttons, press encoder, and rotate encoder.");
  }

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  for (DebouncedInput& input : buttons) {
    pinMode(input.pin, INPUT_PULLUP);
    input.stableLevel = digitalRead(input.pin);
    input.candidateLevel = input.stableLevel;
    input.candidateSince = millis();
  }

  pinMode(PanelPins::ENCODER_A, INPUT_PULLUP);
  pinMode(PanelPins::ENCODER_B, INPUT_PULLUP);
  encoderState = readEncoderState();

  printStartup();
}

void loop() {
  const uint32_t now = millis();
  for (DebouncedInput& input : buttons) {
    updateInput(input, now);
  }
  updateEncoder();
}
