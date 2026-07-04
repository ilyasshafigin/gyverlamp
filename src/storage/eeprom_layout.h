#pragma once

#include <Arduino.h>
#include "../effect/ids.h"
#include "../effect/settings.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"

static_assert(sizeof(EffectSettings) == 3, "EEPROM layout expects 3-uint8_t EffectSettings");

// EEPROM.begin(EEPROM_SIZE) выделяет адреса 0..EEPROM_SIZE-1.
// Layout v2 не мигрирует старые карты адресов: при несовпадении magic/version
// EepromStore очищает EEPROM и пишет defaults текущей версии.
//
//  0..4      Метаданные layout, 5 байт: magic(4) + version(1).
//
//  5..14     Резерв, 10 байт.
//
//  15..94    Блок WiFi config, 80 байт:
//            payload ssid(33) + password(33).
//  95..234   Блок MQTT config, 140 байт:
//            payload host(33) + port(10) + user(33) + password(33).
//
//  235..244  Блок системного состояния, 10 байт:
//            bootCount(1) + powerState(1) + buttonEnabled(1) + globalBrightness(1).
//  245..254  Блок состояния эффектов, 10 байт:
//            currentEffect(1) + globalPaletteId(1) + effectSettingsCount(1).
//  255..264  Блок ротации эффектов, 10 байт:
//            rotationMode(1) + rotationIntervalSec(2).
//  265..274  Блок автовыключения, 10 байт:
//            autoOffMinutes(2).
//  275..284  Блок тихих часов уведомлений, 10 байт:
//            quietHoursEnabled(1) + quietStartMinutes(2) + quietEndMinutes(2).
//  285..299  Блок audio config, 15 байт:
//            marker(1) + mode(1) + band(1) + amount(1) + reserve(11).
//
//  300..449  Настройки эффектов, 150 байт:
//            50 слотов * sizeof(EffectSettings), по 3 байта на.
//            Адрес эффекта: 300 + 3 * effectIndex.
//
//  450..511  Резерв, 62 байт.

constexpr int EEPROM_SIZE = 512;
constexpr uint32_t EEPROM_LAYOUT_MAGIC = 0x474C4D50; // "GLMP"
constexpr uint8_t EEPROM_LAYOUT_VERSION_CURRENT = 4;

constexpr int EEPROM_LAYOUT_META_ADDR = 0;
constexpr int EEPROM_LAYOUT_META_SIZE = 5;

constexpr int EEPROM_RESERVED_TOP_ADDR = 5;
constexpr int EEPROM_RESERVED_TOP_SIZE = 10;

constexpr int EEPROM_WIFI_BLOCK_ADDR = 15;
constexpr int EEPROM_WIFI_BLOCK_SIZE = 80;
constexpr int EEPROM_WIFI_CONFIG_SIZE = WIFI_SSID_LEN + WIFI_PASS_LEN;
constexpr int EEPROM_WIFI_CONFIG_ADDR = EEPROM_WIFI_BLOCK_ADDR;

constexpr int EEPROM_MQTT_BLOCK_ADDR = EEPROM_WIFI_BLOCK_ADDR + EEPROM_WIFI_BLOCK_SIZE; // 95
constexpr int EEPROM_MQTT_BLOCK_SIZE = 140;
constexpr int EEPROM_MQTT_CONFIG_SIZE = MQTT_HOST_LEN + MQTT_PORT_LEN + MQTT_USER_LEN + MQTT_PASS_LEN;
constexpr int EEPROM_MQTT_CONFIG_ADDR = EEPROM_MQTT_BLOCK_ADDR;

constexpr int EEPROM_SYSTEM_STATE_BLOCK_ADDR = EEPROM_MQTT_BLOCK_ADDR + EEPROM_MQTT_BLOCK_SIZE; // 235
constexpr int EEPROM_SYSTEM_STATE_BLOCK_SIZE = 10;
constexpr int EEPROM_BOOT_COUNT_ADDR = EEPROM_SYSTEM_STATE_BLOCK_ADDR;
constexpr int EEPROM_POWER_STATE_ADDR = EEPROM_SYSTEM_STATE_BLOCK_ADDR + 1;
constexpr int EEPROM_BUTTON_ENABLED_ADDR = EEPROM_SYSTEM_STATE_BLOCK_ADDR + 2;
constexpr int EEPROM_GLOBAL_BRIGHTNESS_ADDR = EEPROM_SYSTEM_STATE_BLOCK_ADDR + 3;

constexpr int EEPROM_EFFECT_STATE_BLOCK_ADDR = EEPROM_SYSTEM_STATE_BLOCK_ADDR + EEPROM_SYSTEM_STATE_BLOCK_SIZE; // 245
constexpr int EEPROM_EFFECT_STATE_BLOCK_SIZE = 10;
constexpr int EEPROM_CURRENT_MODE_ADDR = EEPROM_EFFECT_STATE_BLOCK_ADDR;
constexpr int EEPROM_GLOBAL_PALETTE_ID_ADDR = EEPROM_EFFECT_STATE_BLOCK_ADDR + 1;
constexpr int EEPROM_EFFECT_SETTINGS_COUNT_ADDR = EEPROM_EFFECT_STATE_BLOCK_ADDR + 2;

constexpr int EEPROM_ROTATION_BLOCK_ADDR = EEPROM_EFFECT_STATE_BLOCK_ADDR + EEPROM_EFFECT_STATE_BLOCK_SIZE; // 255
constexpr int EEPROM_ROTATION_BLOCK_SIZE = 10;
constexpr int EEPROM_ROTATION_MODE_ADDR = EEPROM_ROTATION_BLOCK_ADDR;
constexpr int EEPROM_ROTATION_INTERVAL_SEC_ADDR = EEPROM_ROTATION_BLOCK_ADDR + 1;

constexpr int EEPROM_AUTO_POWER_BLOCK_ADDR = EEPROM_ROTATION_BLOCK_ADDR + EEPROM_ROTATION_BLOCK_SIZE; // 265
constexpr int EEPROM_AUTO_POWER_BLOCK_SIZE = 10;
constexpr int EEPROM_AUTO_OFF_MINUTES_ADDR = EEPROM_AUTO_POWER_BLOCK_ADDR;

constexpr int EEPROM_NOTIFICATION_BLOCK_ADDR = EEPROM_AUTO_POWER_BLOCK_ADDR + EEPROM_AUTO_POWER_BLOCK_SIZE; // 275
constexpr int EEPROM_NOTIFICATION_BLOCK_SIZE = 10;
constexpr int EEPROM_NOTIFICATION_QUIET_ENABLED_ADDR = EEPROM_NOTIFICATION_BLOCK_ADDR;
constexpr int EEPROM_NOTIFICATION_QUIET_START_ADDR = EEPROM_NOTIFICATION_BLOCK_ADDR + 1;
constexpr int EEPROM_NOTIFICATION_QUIET_END_ADDR = EEPROM_NOTIFICATION_BLOCK_ADDR + 3;

constexpr int EEPROM_AUDIO_BLOCK_ADDR = EEPROM_NOTIFICATION_BLOCK_ADDR + EEPROM_NOTIFICATION_BLOCK_SIZE; // 285
constexpr int EEPROM_AUDIO_BLOCK_SIZE = 15;
constexpr int EEPROM_AUDIO_MARKER_ADDR = EEPROM_AUDIO_BLOCK_ADDR;
constexpr int EEPROM_AUDIO_MODE_ADDR = EEPROM_AUDIO_BLOCK_ADDR + 1;
constexpr int EEPROM_AUDIO_BAND_ADDR = EEPROM_AUDIO_BLOCK_ADDR + 2;
constexpr int EEPROM_AUDIO_AMOUNT_ADDR = EEPROM_AUDIO_BLOCK_ADDR + 3;
constexpr uint8_t EEPROM_AUDIO_MARKER = 0xA6;

constexpr int EEPROM_EFFECT_SETTINGS_BASE = EEPROM_AUDIO_BLOCK_ADDR + EEPROM_AUDIO_BLOCK_SIZE; // 300
constexpr uint8_t EEPROM_EFFECT_SETTINGS_CAPACITY = 50;


static_assert(EEPROM_LAYOUT_META_ADDR + EEPROM_LAYOUT_META_SIZE <= EEPROM_WIFI_BLOCK_ADDR, "EEPROM layout metadata must not overlap WiFi block");
static_assert(EEPROM_WIFI_CONFIG_SIZE <= EEPROM_WIFI_BLOCK_SIZE, "EEPROM WiFi payload must fit into WiFi block");
static_assert(EEPROM_WIFI_BLOCK_ADDR + EEPROM_WIFI_BLOCK_SIZE <= EEPROM_MQTT_BLOCK_ADDR, "EEPROM WiFi block must not overlap MQTT block");
static_assert(EEPROM_MQTT_CONFIG_SIZE <= EEPROM_MQTT_BLOCK_SIZE, "EEPROM MQTT payload must fit into MQTT block");
static_assert(EEPROM_MQTT_BLOCK_ADDR + EEPROM_MQTT_BLOCK_SIZE <= EEPROM_SYSTEM_STATE_BLOCK_ADDR, "EEPROM MQTT block must not overlap system state block");
static_assert(EEPROM_SYSTEM_STATE_BLOCK_ADDR + EEPROM_SYSTEM_STATE_BLOCK_SIZE <= EEPROM_EFFECT_STATE_BLOCK_ADDR, "EEPROM system state block must not overlap effect state block");
static_assert(EEPROM_EFFECT_STATE_BLOCK_ADDR + EEPROM_EFFECT_STATE_BLOCK_SIZE <= EEPROM_ROTATION_BLOCK_ADDR, "EEPROM effect state block must not overlap rotation block");
static_assert(EEPROM_ROTATION_BLOCK_ADDR + EEPROM_ROTATION_BLOCK_SIZE <= EEPROM_AUTO_POWER_BLOCK_ADDR, "EEPROM rotation block must not overlap auto power block");
static_assert(EEPROM_AUTO_POWER_BLOCK_ADDR + EEPROM_AUTO_POWER_BLOCK_SIZE <= EEPROM_NOTIFICATION_BLOCK_ADDR, "EEPROM auto power block must not overlap notification block");
static_assert(EEPROM_NOTIFICATION_BLOCK_ADDR + EEPROM_NOTIFICATION_BLOCK_SIZE <= EEPROM_AUDIO_BLOCK_ADDR, "EEPROM notification block must not overlap audio config");
static_assert(EEPROM_AUDIO_BLOCK_ADDR + EEPROM_AUDIO_BLOCK_SIZE <= EEPROM_EFFECT_SETTINGS_BASE, "EEPROM audio config must not overlap effect settings");
static_assert(Effects::COUNT <= EEPROM_EFFECT_SETTINGS_CAPACITY, "EEPROM effect settings capacity is too small for Effects::COUNT");
static_assert(EEPROM_EFFECT_SETTINGS_BASE + EEPROM_EFFECT_SETTINGS_CAPACITY * sizeof(EffectSettings) <= EEPROM_SIZE, "EEPROM effect settings must fit into EEPROM_SIZE");

constexpr int EEPROM_EFFECT_SETTINGS_ADDR(int effectIndex) {
  return EEPROM_EFFECT_SETTINGS_BASE + 3 * effectIndex;
}
