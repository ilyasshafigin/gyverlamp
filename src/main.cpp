

// Ссылка для менеджера плат:
// http://arduino.esp8266.com/stable/package_esp8266com_index.json

// Для WEMOS выбираем плату LOLIN(WEMOS) D1 R2 & mini
// Для NodeMCU выбираем NodeMCU 1.0 (ESP-12E Module)

#include <Arduino.h>
#include "core/lamp.h"

#ifndef USE_ADC
ADC_MODE(ADC_VCC);
#endif

static Lamp lamp;

void setup() {
  Serial.begin(115200);
  delay(100);
  lamp.setup();
}

void loop() {
  lamp.loop();
}
