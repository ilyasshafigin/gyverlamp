#pragma once

#include <Arduino.h>
#include "../effect/ids.h"
#include "../effect/settings.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"

static_assert(sizeof(EffectSettings) == 3, "EEPROM layout expects 3-uint8_t EffectSettings");

// EEPROM.begin(kEepromSize) выделяет адреса 0..kEepromSize-1.
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
//            otaPolicy(1) + powerState(1) + buttonEnabled(1) + globalBrightness(1).
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

constexpr int kEepromSize = 512;
constexpr uint32_t kEepromLayoutMagic = 0x474C4D50; // "GLMP"
constexpr uint8_t kEepromLayoutVersionCurrent = 4;

constexpr int kEepromLayoutMetaAddr = 0;
constexpr int kEepromLayoutMetaSize = 5;

constexpr int kEepromReservedTopAddr = 5;
constexpr int kEepromReservedTopSize = 10;

constexpr int kEepromWifiBlockAddr = 15;
constexpr int kEepromWifiBlockSize = 80;
constexpr int kEepromWifiConfigSize = kWifiSsidLen + kWifiPassLen;
constexpr int kEepromWifiConfigAddr = kEepromWifiBlockAddr;

constexpr int kEepromMqttBlockAddr = kEepromWifiBlockAddr + kEepromWifiBlockSize; // 95
constexpr int kEepromMqttBlockSize = 140;
constexpr int kEepromMqttConfigSize = kMqttHostLen + kMqttPortLen + kMqttUserLen + kMqttPassLen;
constexpr int kEepromMqttConfigAddr = kEepromMqttBlockAddr;

constexpr int kEepromSystemStateBlockAddr = kEepromMqttBlockAddr + kEepromMqttBlockSize; // 235
constexpr int kEepromSystemStateBlockSize = 10;
constexpr int kEepromOtaPolicyAddr = kEepromSystemStateBlockAddr;
constexpr uint8_t kEepromOtaPolicyEnabled = 0xA5;
constexpr uint8_t kEepromOtaPolicyDisabled = 0x5A;
constexpr int kEepromPowerStateAddr = kEepromSystemStateBlockAddr + 1;
constexpr int kEepromButtonEnabledAddr = kEepromSystemStateBlockAddr + 2;
constexpr int kEepromGlobalBrightnessAddr = kEepromSystemStateBlockAddr + 3;

constexpr int kEepromEffectStateBlockAddr = kEepromSystemStateBlockAddr + kEepromSystemStateBlockSize; // 245
constexpr int kEepromEffectStateBlockSize = 10;
constexpr int kEepromCurrentModeAddr = kEepromEffectStateBlockAddr;
constexpr int kEepromGlobalPaletteIdAddr = kEepromEffectStateBlockAddr + 1;
constexpr int kEepromEffectSettingsCountAddr = kEepromEffectStateBlockAddr + 2;

constexpr int kEepromRotationBlockAddr = kEepromEffectStateBlockAddr + kEepromEffectStateBlockSize; // 255
constexpr int kEepromRotationBlockSize = 10;
constexpr int kEepromRotationModeAddr = kEepromRotationBlockAddr;
constexpr int kEepromRotationIntervalSecAddr = kEepromRotationBlockAddr + 1;

constexpr int kEepromAutoPowerBlockAddr = kEepromRotationBlockAddr + kEepromRotationBlockSize; // 265
constexpr int kEepromAutoPowerBlockSize = 10;
constexpr int kEepromAutoOffMinutesAddr = kEepromAutoPowerBlockAddr;

constexpr int kEepromNotificationBlockAddr = kEepromAutoPowerBlockAddr + kEepromAutoPowerBlockSize; // 275
constexpr int kEepromNotificationBlockSize = 10;
constexpr int kEepromNotificationQuietEnabledAddr = kEepromNotificationBlockAddr;
constexpr int kEepromNotificationQuietStartAddr = kEepromNotificationBlockAddr + 1;
constexpr int kEepromNotificationQuietEndAddr = kEepromNotificationBlockAddr + 3;

constexpr int kEepromAudioBlockAddr = kEepromNotificationBlockAddr + kEepromNotificationBlockSize; // 285
constexpr int kEepromAudioBlockSize = 15;
constexpr int kEepromAudioMarkerAddr = kEepromAudioBlockAddr;
constexpr int kEepromAudioModeAddr = kEepromAudioBlockAddr + 1;
constexpr int kEepromAudioBandAddr = kEepromAudioBlockAddr + 2;
constexpr int kEepromAudioAmountAddr = kEepromAudioBlockAddr + 3;
constexpr uint8_t kEepromAudioMarker = 0xA6;

constexpr int kEepromEffectSettingsBase = kEepromAudioBlockAddr + kEepromAudioBlockSize; // 300
constexpr uint8_t kEepromEffectSettingsCapacity = 50;

static_assert(
  kEepromLayoutMetaAddr + kEepromLayoutMetaSize <= kEepromWifiBlockAddr,
  "EEPROM layout metadata must not overlap WiFi block"
);
static_assert(kEepromWifiConfigSize <= kEepromWifiBlockSize, "EEPROM WiFi payload must fit into WiFi block");
static_assert(
  kEepromWifiBlockAddr + kEepromWifiBlockSize <= kEepromMqttBlockAddr, "EEPROM WiFi block must not overlap MQTT block"
);
static_assert(kEepromMqttConfigSize <= kEepromMqttBlockSize, "EEPROM MQTT payload must fit into MQTT block");
static_assert(
  kEepromMqttBlockAddr + kEepromMqttBlockSize <= kEepromSystemStateBlockAddr,
  "EEPROM MQTT block must not overlap system state block"
);
static_assert(
  kEepromSystemStateBlockAddr + kEepromSystemStateBlockSize <= kEepromEffectStateBlockAddr,
  "EEPROM system state block must not overlap effect state block"
);
static_assert(
  kEepromEffectStateBlockAddr + kEepromEffectStateBlockSize <= kEepromRotationBlockAddr,
  "EEPROM effect state block must not overlap rotation block"
);
static_assert(
  kEepromRotationBlockAddr + kEepromRotationBlockSize <= kEepromAutoPowerBlockAddr,
  "EEPROM rotation block must not overlap auto power block"
);
static_assert(
  kEepromAutoPowerBlockAddr + kEepromAutoPowerBlockSize <= kEepromNotificationBlockAddr,
  "EEPROM auto power block must not overlap notification block"
);
static_assert(
  kEepromNotificationBlockAddr + kEepromNotificationBlockSize <= kEepromAudioBlockAddr,
  "EEPROM notification block must not overlap audio config"
);
static_assert(
  kEepromAudioBlockAddr + kEepromAudioBlockSize <= kEepromEffectSettingsBase,
  "EEPROM audio config must not overlap effect settings"
);
static_assert(
  Effects::kCount <= kEepromEffectSettingsCapacity, "EEPROM effect settings capacity is too small for Effects::kCount"
);
static_assert(
  kEepromEffectSettingsBase + kEepromEffectSettingsCapacity * sizeof(EffectSettings) <= kEepromSize,
  "EEPROM effect settings must fit into kEepromSize"
);

constexpr int kEepromEffectSettingsAddr(int effectIndex) {
  return kEepromEffectSettingsBase + 3 * effectIndex;
}
