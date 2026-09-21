#pragma once

#ifndef PANEL_BUTTON_1_PIN
#define PANEL_BUTTON_1_PIN 1
#endif

#ifndef PANEL_BUTTON_2_PIN
#define PANEL_BUTTON_2_PIN 2
#endif

#ifndef PANEL_BUTTON_3_PIN
#define PANEL_BUTTON_3_PIN 3
#endif

#ifndef PANEL_BUTTON_4_PIN
#define PANEL_BUTTON_4_PIN 4
#endif

#ifndef PANEL_BUTTON_5_PIN
#define PANEL_BUTTON_5_PIN 5
#endif

#ifndef PANEL_ENCODER_A_PIN
#define PANEL_ENCODER_A_PIN 7
#endif

#ifndef PANEL_ENCODER_B_PIN
#define PANEL_ENCODER_B_PIN 10
#endif

#ifndef PANEL_ENCODER_SWITCH_PIN
#define PANEL_ENCODER_SWITCH_PIN 6
#endif

#ifndef PANEL_STATUS_LED_PIN
#define PANEL_STATUS_LED_PIN -1
#endif

#ifndef PANEL_STATUS_LED_ACTIVE_HIGH
#define PANEL_STATUS_LED_ACTIVE_HIGH 1
#endif

#if PANEL_STATUS_LED_ACTIVE_HIGH != 0 && PANEL_STATUS_LED_ACTIVE_HIGH != 1
#error "PANEL_STATUS_LED_ACTIVE_HIGH must be 0 or 1"
#endif

namespace PanelPins {

  constexpr int BUTTON_1 = PANEL_BUTTON_1_PIN;
  constexpr int BUTTON_2 = PANEL_BUTTON_2_PIN;
  constexpr int BUTTON_3 = PANEL_BUTTON_3_PIN;
  constexpr int BUTTON_4 = PANEL_BUTTON_4_PIN;
  constexpr int BUTTON_5 = PANEL_BUTTON_5_PIN;

  constexpr int ENCODER_A = PANEL_ENCODER_A_PIN;
  constexpr int ENCODER_B = PANEL_ENCODER_B_PIN;
  constexpr int ENCODER_SWITCH = PANEL_ENCODER_SWITCH_PIN;

  constexpr int STATUS_LED = PANEL_STATUS_LED_PIN;
  constexpr bool STATUS_LED_ACTIVE_HIGH = PANEL_STATUS_LED_ACTIVE_HIGH == 1;

} // namespace PanelPins
