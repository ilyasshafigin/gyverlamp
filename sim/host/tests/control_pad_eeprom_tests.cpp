#include <EEPROM.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "storage/eeprom_layout.h"
#include "storage/eeprom_store.h"

namespace {

  int failures = 0;
  uint8_t parityImage[kEepromSize] = {};
  bool parityImageReady = false;

  void expect(bool condition, const char* name) {
    if (!condition) {
      ++failures;
      printf("FAIL: %s\n", name);
    }
  }

  bool equalBytes(const uint8_t* left, const uint8_t* right, size_t length) {
    uint8_t difference = 0;
    for (size_t index = 0; index < length; ++index) {
      difference = static_cast<uint8_t>(difference | (left[index] ^ right[index]));
    }
    return difference == 0;
  }

  void writeMagicAndVersion(uint8_t image[kEepromSize], uint8_t version) {
    const uint32_t magic = kEepromLayoutMagic;
    memcpy(image + kEepromLayoutMetaAddr, &magic, sizeof(magic));
    image[kEepromLayoutMetaAddr + sizeof(magic)] = version;
  }

  void writeText(uint8_t image[kEepromSize], int address, size_t size, const char* text) {
    memset(image + address, 0, size);
    size_t length = strlen(text);
    if (length >= size) length = size - 1;
    memcpy(image + address, text, length);
  }

  void writeUint16(uint8_t image[kEepromSize], int address, uint16_t value) {
    image[address] = static_cast<uint8_t>(value & 0xffU);
    image[address + 1] = static_cast<uint8_t>(value >> 8);
  }

  void canonicalUnpaired(uint8_t output[kEepromControlPadBindingSize]) {
    ControlPadProtocol::BindingRecord binding;
    ControlPadProtocol::makeUnpairedBinding(&binding);
    const bool encoded = ControlPadProtocol::encodeBindingRecord(binding, output);
    expect(encoded, "canonical unpaired record encodes");
  }

  bool persistedBindingEquals(const uint8_t expected[kEepromControlPadBindingSize]) {
    uint8_t actual[kEepromControlPadBindingSize];
    for (size_t index = 0; index < sizeof(actual); ++index) {
      actual[index] = EEPROM.persistedAt(kEepromControlPadBindingAddr + static_cast<int>(index));
    }
    return equalBytes(actual, expected, sizeof(actual));
  }

  bool stagedBindingEquals(const uint8_t expected[kEepromControlPadBindingSize]) {
    uint8_t staged[kEepromSize];
    EEPROM.copyStaged(staged, sizeof(staged));
    return equalBytes(staged + kEepromControlPadBindingAddr, expected, kEepromControlPadBindingSize);
  }

  ControlPadProtocol::BindingRecord pairedBinding() {
    ControlPadProtocol::BindingRecord binding = {};
    binding.enabled = true;
    binding.protocolVersion = ControlPadProtocol::kProtocolVersion;
    binding.panelMac.bytes[0] = 0x02;
    binding.panelMac.bytes[5] = 0x01;
    binding.lastChannel = 6;
    binding.flags = ControlPadProtocol::kBindingFlagBound;
    for (uint8_t index = 0; index < ControlPadProtocol::kPairKeySize; ++index) {
      binding.pairKey[index] = index;
    }
    return binding;
  }

  void testFreshAndUnsupported() {
    uint8_t canonical[kEepromControlPadBindingSize];
    canonicalUnpaired(canonical);

    EEPROM.reset();
    EepromStore fresh;
    expect(fresh.init(), "fresh EEPROM initializes");
    expect(
      EEPROM.persistedAt(kEepromLayoutMetaAddr + sizeof(uint32_t)) == kEepromLayoutVersionCurrent &&
        persistedBindingEquals(canonical) && EEPROM.commitCount() == 1,
      "fresh EEPROM writes v6 and canonical binding once"
    );

    uint8_t unsupported[kEepromSize];
    memset(unsupported, 0xA5, sizeof(unsupported));
    writeMagicAndVersion(unsupported, 9);
    EEPROM.loadPersisted(unsupported, sizeof(unsupported));
    EepromStore store;
    expect(store.init(), "unsupported layout initializes");
    expect(
      EEPROM.persistedAt(kEepromLayoutMetaAddr + sizeof(uint32_t)) == kEepromLayoutVersionCurrent &&
        persistedBindingEquals(canonical) && EEPROM.persistedAt(511) == 0 && EEPROM.commitCount() == 1,
      "unsupported layout resets to v6 defaults once"
    );
  }

  void testV5Migration() {
    uint8_t image[kEepromSize];
    uint8_t before[kEepromSize];
    for (size_t index = 0; index < sizeof(image); ++index) {
      image[index] = static_cast<uint8_t>(index ^ 0xA5U);
    }
    writeMagicAndVersion(image, kEepromLayoutVersionV5);
    memcpy(before, image, sizeof(before));
    EEPROM.loadPersisted(image, sizeof(image));

    EepromStore store;
    expect(store.init(), "v5 migration succeeds");
    uint8_t persisted[kEepromSize];
    EEPROM.copyPersisted(persisted, sizeof(persisted));
    uint8_t canonical[kEepromControlPadBindingSize];
    canonicalUnpaired(canonical);
    expect(
      equalBytes(persisted + 5, before + 5, 445) && equalBytes(persisted + 482, before + 482, 30) &&
        persisted[4] == kEepromLayoutVersionCurrent &&
        equalBytes(persisted + kEepromControlPadBindingAddr, canonical, sizeof(canonical)) && EEPROM.commitCount() == 1,
      "v5 migration preserves required bytes and commits once"
    );
    memcpy(parityImage, persisted, sizeof(parityImage));
    parityImageReady = true;
  }

  void testV4MigrationAndV6Records() {
    uint8_t image[kEepromSize];
    memset(image, 0, sizeof(image));
    writeMagicAndVersion(image, kEepromLayoutVersionV4);
    writeText(image, kEepromWifiConfigAddr, WifiConfig::kWifiSsidLen, "wifi-v4-distinct");
    writeText(
      image, kEepromWifiConfigAddr + WifiConfig::kWifiSsidLen, WifiConfig::kWifiPassLen, "wifi-pass-v4-distinct"
    );
    writeText(image, kEepromMqttV4HostAddr, MqttConfig::kMqttHostLen, "mqtt-v4-distinct");
    writeText(image, kEepromMqttV4PortAddr, kEepromMqttV4PortTextSize, "1883");
    writeText(image, kEepromMqttV4UserAddr, MqttConfig::kMqttUserLen, "mqtt-user-v4");
    writeText(image, kEepromMqttV4PasswordAddr, MqttConfig::kMqttPassLen, "mqtt-pass-v4");
    image[kEepromButtonEnabledAddr] = 0;
    image[kEepromGlobalBrightnessAddr] = 71;
    image[kEepromCurrentModeAddr] = static_cast<uint8_t>(Effects::Id::Rainbow);
    image[kEepromGlobalPaletteIdAddr] = static_cast<uint8_t>(Palettes::Id::Xmas);
    image[kEepromEffectSettingsCountAddr] = 2;
    image[kEepromRotationModeAddr] = static_cast<uint8_t>(RotationMode::Random);
    writeUint16(image, kEepromRotationIntervalSecAddr, 321);
    writeUint16(image, kEepromAutoOffMinutesAddr, 987);
    image[kEepromNotificationQuietEnabledAddr] = 1;
    writeUint16(image, kEepromNotificationQuietStartAddr, 101);
    writeUint16(image, kEepromNotificationQuietEndAddr, 1234);
    image[kEepromAudioMarkerAddr] = kEepromAudioMarker;
    image[kEepromAudioModeAddr] = static_cast<uint8_t>(AudioMode::Effect);
    image[kEepromAudioBandAddr] = static_cast<uint8_t>(AudioBand::Treble);
    image[kEepromAudioAmountAddr] = 213;
    image[kEepromEffectSettingsBase] = 0xA1;
    image[kEepromEffectSettingsBase + 1] = 0xB2;
    image[kEepromEffectSettingsBase + 2] = 0xC3;
    EEPROM.loadPersisted(image, sizeof(image));
    EepromStore v4;
    expect(v4.init(), "v4 migration succeeds");
    uint8_t canonical[kEepromControlPadBindingSize];
    canonicalUnpaired(canonical);
    expect(
      EEPROM.persistedAt(4) == kEepromLayoutVersionCurrent && persistedBindingEquals(canonical) &&
        EEPROM.commitCount() == 1,
      "v4 migration stages v6 binding and version in one commit"
    );
    const WifiConfig& wifi = v4.readWifiConfig();
    const MqttConfig& mqtt = v4.readMqttConfig();
    const NotificationQuietHours quietHours = v4.readNotificationQuietHours();
    const AudioConfig audio = v4.readAudioConfig();
    expect(
      strcmp(wifi.ssid, "wifi-v4-distinct") == 0 && strcmp(wifi.password, "wifi-pass-v4-distinct") == 0 &&
        strcmp(mqtt.host, "mqtt-v4-distinct") == 0 && mqtt.port == 1883 && strcmp(mqtt.user, "mqtt-user-v4") == 0 &&
        strcmp(mqtt.password, "mqtt-pass-v4") == 0,
      "v4 migration preserves WiFi and textual-port MQTT fields"
    );
    expect(
      v4.readRotationMode() == RotationMode::Random && v4.readRotationIntervalSec() == 321 &&
        v4.readAutoOffMinutes() == 987 && !v4.readButtonEnabled() && v4.readGlobalPaletteId() == Palettes::Id::Xmas &&
        quietHours.enabled && quietHours.startMinutes == 101 && quietHours.endMinutes == 1234 &&
        audio.mode == AudioMode::Effect && audio.band == AudioBand::Treble && audio.amount == 213 &&
        v4.readCurrentEffectId() == Effects::Id::Rainbow,
      "v4 migration preserves controller and notification fields"
    );
    expect(
      v4.readGlobalBrightness() == 255 && EEPROM.persistedAt(kEepromEffectSettingsCountAddr) == 0 &&
        EEPROM.persistedAt(kEepromEffectSettingsBase) == 0 && EEPROM.persistedAt(kEepromEffectSettingsBase + 1) == 0 &&
        EEPROM.persistedAt(kEepromEffectSettingsBase + 2) == 0,
      "v4 migration intentionally resets brightness and effect settings"
    );

    uint8_t validV6[kEepromSize];
    memset(validV6, 0x5A, sizeof(validV6));
    writeMagicAndVersion(validV6, kEepromLayoutVersionCurrent);
    memcpy(validV6 + kEepromControlPadBindingAddr, canonical, sizeof(canonical));
    EEPROM.loadPersisted(validV6, sizeof(validV6));
    EepromStore valid;
    expect(valid.init() && EEPROM.commitCount() == 0, "valid v6 does not rewrite EEPROM");

    validV6[kEepromControlPadBindingAddr] ^= 0x01;
    EEPROM.loadPersisted(validV6, sizeof(validV6));
    EepromStore malformed;
    expect(
      malformed.init() && EEPROM.commitCount() == 0 && !malformed.readControlPadBinding().enabled &&
        EEPROM.persistedAt(kEepromControlPadBindingAddr) == validV6[kEepromControlPadBindingAddr],
      "malformed v6 binding reads safe-unpaired without reset"
    );
  }

  void testFailedMigrationAndBindingRollback() {
    uint8_t v5[kEepromSize];
    memset(v5, 0x3C, sizeof(v5));
    writeMagicAndVersion(v5, kEepromLayoutVersionV5);
    EEPROM.loadPersisted(v5, sizeof(v5));
    EEPROM.failNextCommit();
    EepromStore failedMigration;
    expect(!failedMigration.init(), "failed v5 migration propagates failure");
    uint8_t persisted[kEepromSize];
    EEPROM.copyPersisted(persisted, sizeof(persisted));
    expect(equalBytes(persisted, v5, sizeof(v5)), "failed migration leaves persisted image unchanged");

    EEPROM.reset();
    EepromStore store;
    expect(store.init(), "binding rollback fixture initializes");
    uint8_t unpaired[kEepromControlPadBindingSize];
    canonicalUnpaired(unpaired);
    const ControlPadProtocol::BindingRecord paired = pairedBinding();
    uint8_t pairedEncoded[kEepromControlPadBindingSize];
    expect(ControlPadProtocol::encodeBindingRecord(paired, pairedEncoded), "paired binding fixture encodes");

    EEPROM.failNextCommit();
    expect(
      !store.writeControlPadBinding(paired) && persistedBindingEquals(unpaired) && stagedBindingEquals(unpaired),
      "failed binding write rolls staged bytes back"
    );
    expect(
      store.writeControlPadBinding(paired) && persistedBindingEquals(pairedEncoded),
      "binding write persists after successful commit"
    );
    EEPROM.failNextCommit();
    expect(
      !store.clearControlPadBinding() && persistedBindingEquals(pairedEncoded) && stagedBindingEquals(pairedEncoded),
      "failed binding clear rolls staged bytes back"
    );
  }

} // namespace

int main(int argc, char* argv[]) {
  const char* parityPath = nullptr;
  if (argc == 3 && strcmp(argv[1], "--parity-image") == 0) {
    parityPath = argv[2];
  } else if (argc != 1) {
    printf("Usage: %s [--parity-image <path>]\n", argv[0]);
    return 2;
  }
  testFreshAndUnsupported();
  testV5Migration();
  testV4MigrationAndV6Records();
  testFailedMigrationAndBindingRollback();
  if (parityPath != nullptr) {
    FILE* parityFile = fopen(parityPath, "wb");
    expect(
      parityImageReady && parityFile != nullptr &&
        fwrite(parityImage, 1, sizeof(parityImage), parityFile) == sizeof(parityImage),
      "writes deterministic v6 parity image"
    );
    if (parityFile != nullptr) fclose(parityFile);
  }
  if (failures != 0) {
    printf("FAILED: %d EEPROM fixture assertion(s)\n", failures);
    return 1;
  }
  printf("PASS: EEPROM v6 migration and binding rollback fixtures\n");
  return 0;
}
