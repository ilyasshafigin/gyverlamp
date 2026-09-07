#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP8266)
#define PLATFORM_LED_PIN 2
#define PLATFORM_BUTTON_PIN 4
#define PLATFORM_MICROPHONE_PIN A0
#elif defined(ARDUINO_ARCH_ESP32)
#define PLATFORM_LED_PIN 18
#define PLATFORM_BUTTON_PIN 27
#define PLATFORM_MICROPHONE_PIN 34
#elif defined(SIMULATOR_AUDIO_INPUT)
#define PLATFORM_LED_PIN 2
#define PLATFORM_BUTTON_PIN 4
#define PLATFORM_MICROPHONE_PIN A0
#else
#error "Unsupported controller architecture"
#endif

// Audio analysis uses the same 10-bit range on every supported controller.
#define PLATFORM_ADC_INPUT_MIN 0
#define PLATFORM_ADC_INPUT_MAX 1023
