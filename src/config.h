#pragma once

#include <Arduino.h>

// ============= НАСТРОЙКИ =============

#ifndef DEVICE_NAME
#define DEVICE_NAME "GyverLamp" // задается в platformio.local.ini
#endif

// -------- ВРЕМЯ -------

// Часовой пояс (москва 3)
#ifndef GMT
#define GMT 3   // задается в platformio.local.ini
#endif
// Сервер времени
#define NTP_ADDRESS  "ru.pool.ntp.org"
#define NTP_INTERVAL 600 * 1000           // обновление (10 минут)

// ---------- МАТРИЦА ---------

// Лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит
#ifndef CURRENT_LIMIT
#define CURRENT_LIMIT 3000 // задается в platformio.local.ini
#endif

// Ширина матрицы
#ifndef WIDTH
#define WIDTH 16
#endif
// Высота матрицы
#ifndef HEIGHT
#define HEIGHT 16
#endif
// Количество светодиодов
#define NUM_LEDS static_cast<uint16_t>(WIDTH * HEIGHT)
// Порядок цветов на ленте. Если цвет отображается некорректно - меняйте. Начать можно с RGB
#define COLOR_ORDER GRB
// Пин ленты
#define LED_PIN 2
// Тип светодиодов
#define LED_TYPE WS2812

// Угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#ifndef CONNECTION_ANGLE
#define CONNECTION_ANGLE 1
#endif
// Направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз
#ifndef STRIP_DIRECTION
#define STRIP_DIRECTION 3
#endif

// Частота кадров переписовки матрицы
#define FRAME_RATE  50U
// Время кадра в мс (1000 / FPS)
#define FRAME_MS    static_cast<uint16_t>(1000U / FRAME_RATE)

// при неправильной настройке матрицы вы получите предупреждение "Wrong matrix parameters! Set to default"
// шпаргалка по настройке матрицы здесь! https://alexgyver.ru/matrix_guide/

// --------- Кнопка ----------

// Будет ли поддержка кнопки
//#define USE_BUTTON - задается в platformio.local.ini
// Пин сенсорной кнопки
#define BTN_PIN 4

// -------- Микрофон ---------

// Будет ли поддержка микрофона
//#define USE_ADC - задается в platformio.local.ini

// ----- AP (точка доступа) -------

// Название точки доступа
#ifndef AP_SSID
#define AP_SSID DEVICE_NAME // задается в platformio.local.ini
#endif
// Пароль точки доступа
#ifndef AP_PASS
#define AP_PASS "12345678" // задается в platformio.local.ini
#endif
// IP точки доступа лампы. Должен быть в отдельной подсети от домашнего WiFi
#define AP_IP { 192, 168, 4, 1 }

// ----- MQTT -----
// Будет ли поддержка MQTT для управления лампой из Home Assistant
//#define USE_MQTT - задается в platformio.local.ini

// ----- OTA -----
// Будет ли поддержка OTA обновление по Wifi
//#define USE_OTA - задается в platformio.local.ini

// ----- UDP -----
// Будет ли использоваться UPD протокол для управления их мобильных приложений
//#define USE_UDP - задается в platformio.local.ini
// Порт UDP
#define UDP_PORT (8888U)

// ============= ДЛЯ РАЗРАБОТЧИКОВ =============

// Версия прошивки
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev" // задается в platformio.local.ini
#endif
// Разработчик устройства / прошивки
#define FIRMWARE_MANUFACTURER "Alex Gyver / Ilyas Shafigin"

// Режим дебага, будет печатать в Serial
//#define DEBUG - задается в platformio.local.ini
// Профайлер, будет отдавать информацию о времени работы loop в web ui
//#define PROFILE_LOOP - задается в platformio.local.ini
// Тестирование уведомлений. Будет возможность в web ui устанавиливать любые состояния уведомлениям
//#define TEST_NOTIFICATIONS - задается в platformio.local.ini
