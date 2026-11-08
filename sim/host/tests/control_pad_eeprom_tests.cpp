#include <EEPROM.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "storage/eeprom_layout.h"
#include "storage/eeprom_store.h"
#include "effect/catalog.h"

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

  // Focused equivalent of SettingsRepository::init's defaults/ensure/read path
  // with the real catalog specs, without linking effect renderers.
  void completeEffectDefaults(EepromStore& store) {
    const EffectSettingsSpec specs[] = {
      EffectColor::kSettings,        EffectColorChange::kSettings,    EffectGradient::kSettings,
      EffectFire::kSettings,         EffectRainbow::kSettings,        EffectNoise::kSettings,
      EffectMatrix::kSettings,       EffectPaintball::kSettings,      EffectWarmLight::kSettings,
      EffectTwinkles::kSettings,     EffectButterflys::kSettings,     EffectThunderstorm::kSettings,
      EffectClock::kSettings,        EffectLiquidLamp::kSettings,     EffectNorthernLights::kSettings,
      EffectEqualizer::kSettings,    EffectOctopus::kSettings,        EffectSilkRibbons::kSettings,
      EffectStainedGlass::kSettings, EffectLivingMembrane::kSettings,
    };
    static_assert(sizeof(specs) / sizeof(specs[0]) == Effects::kCount, "fixture covers catalog");
    EffectSettings defaults[Effects::kCount];
    for (uint8_t index = 0; index < Effects::kCount; ++index) {
      defaults[index] = EffectSettings::fromSpec(specs[index]);
    }
    store.ensureEffectSettings(defaults);
    for (uint8_t index = 0; index < Effects::kCount; ++index) {
      EffectSettings actual;
      store.readEffectSettings(Effects::toId(index), actual);
      expect(memcmp(&actual, &defaults[index], sizeof(actual)) == 0, "boot loads actual catalog defaults");
      expect(
        EEPROM.persistedAt(kEepromEffectSettingsAddr(index)) == defaults[index].brightness &&
          EEPROM.persistedAt(kEepromEffectSettingsAddr(index) + 1) == defaults[index].speed &&
          EEPROM.persistedAt(kEepromEffectSettingsAddr(index) + 2) == defaults[index].scale,
        "boot persists actual catalog defaults"
      );
    }
    expect(EEPROM.persistedAt(kEepromEffectSettingsCountAddr) == Effects::kCount, "all current slots initialized");
  }

  bool isEffectResetAddress(int address) {
    return address == kEepromCurrentModeAddr || address == kEepromEffectSettingsCountAddr;
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
      "fresh EEPROM writes v7 and canonical binding once"
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
      "unsupported layout resets to v7 defaults once"
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
      persisted[4] == kEepromLayoutVersionCurrent &&
        equalBytes(persisted + kEepromControlPadBindingAddr, canonical, sizeof(canonical)) && EEPROM.commitCount() == 1,
      "v5 migration preserves required bytes and commits once"
    );
    for (int address = 5; address < kEepromSize; ++address) {
      if (
        isEffectResetAddress(address) || (address >= kEepromControlPadBindingAddr &&
                                          address < kEepromControlPadBindingAddr + kEepromControlPadBindingSize)
      )
        continue;
      expect(persisted[address] == before[address], "v5 preserves every unrelated byte");
    }
    expect(
      store.readCurrentEffectId() == Effects::kDefaultId && persisted[kEepromEffectSettingsCountAddr] == 0,
      "v5 stages selected default and invalidates old settings"
    );
    completeEffectDefaults(store);
    expect(EEPROM.commitCount() == 2, "v5 boot completes defaults in second commit");
    EEPROM.copyPersisted(persisted, sizeof(persisted));
    memcpy(parityImage, persisted, sizeof(parityImage));
    parityImageReady = true;
  }

  void testV4MigrationAndCurrentRecords() {
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
      "v4 migration stages v7 binding and version in one commit"
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
        v4.readCurrentEffectId() == Effects::kDefaultId,
      "v4 migration preserves controller and notification fields"
    );
    expect(
      v4.readGlobalBrightness() == 255 && EEPROM.persistedAt(kEepromEffectSettingsCountAddr) == 0 &&
        EEPROM.persistedAt(kEepromEffectSettingsBase) == 0 && EEPROM.persistedAt(kEepromEffectSettingsBase + 1) == 0 &&
        EEPROM.persistedAt(kEepromEffectSettingsBase + 2) == 0,
      "v4 migration intentionally resets brightness and effect settings"
    );

    completeEffectDefaults(v4);
    expect(EEPROM.commitCount() == 2, "v4 boot completes defaults in second commit");

    uint8_t current[kEepromSize];
    memset(current, 0x5A, sizeof(current));
    writeMagicAndVersion(current, kEepromLayoutVersionCurrent);
    memcpy(current + kEepromControlPadBindingAddr, canonical, sizeof(canonical));
    EEPROM.loadPersisted(current, sizeof(current));
    EepromStore valid;
    expect(valid.init() && EEPROM.commitCount() == 0, "valid v7 does not rewrite EEPROM");

    current[kEepromControlPadBindingAddr] ^= 0x01;
    EEPROM.loadPersisted(current, sizeof(current));
    EepromStore malformed;
    expect(
      malformed.init() && EEPROM.commitCount() == 0 && !malformed.readControlPadBinding().enabled &&
        EEPROM.persistedAt(kEepromControlPadBindingAddr) == current[kEepromControlPadBindingAddr],
      "malformed v7 binding reads safe-unpaired without reset"
    );
  }

  void testV6MigrationAndBoot() {
    uint8_t image[kEepromSize];
    for (int address = 0; address < kEepromSize; ++address) {
      image[address] = static_cast<uint8_t>(address * 37 + 19);
    }
    writeMagicAndVersion(image, kEepromLayoutVersionV6);
    const auto binding = pairedBinding();
    expect(
      ControlPadProtocol::encodeBindingRecord(binding, image + kEepromControlPadBindingAddr),
      "v6 paired binding fixture encodes"
    );
    image[kEepromEffectSettingsCountAddr] = 22;
    image[kEepromCurrentModeAddr] = 21;
    EEPROM.loadPersisted(image, sizeof(image));
    EepromStore store;
    expect(store.init() && EEPROM.commitCount() == 1, "v6 migration commits once");
    uint8_t persisted[kEepromSize];
    EEPROM.copyPersisted(persisted, sizeof(persisted));
    for (int address = 0; address < kEepromSize; ++address) {
      if (address == 4 || isEffectResetAddress(address)) continue;
      expect(persisted[address] == image[address], "v6 migration preserves every unrelated byte");
    }
    expect(
      persisted[4] == 7 && persisted[kEepromEffectSettingsCountAddr] == 0 &&
        store.readCurrentEffectId() == Effects::kDefaultId && store.readControlPadBinding().enabled,
      "v6 migration invalidates old slots and retains paired binding"
    );
    completeEffectDefaults(store);
    expect(EEPROM.commitCount() == 2, "v6 boot completes defaults in second commit");
    EEPROM.copyPersisted(persisted, sizeof(persisted));
    for (int address = 0; address < kEepromSize; ++address) {
      if (
        address == 4 || isEffectResetAddress(address) ||
        (address >= kEepromEffectSettingsBase && address < kEepromEffectSettingsAddr(Effects::kCount))
      )
        continue;
      expect(persisted[address] == image[address], "full v6 boot preserves every non-target byte");
    }
    EEPROM.loadPersisted(persisted, sizeof(persisted));
    EepromStore reboot;
    expect(reboot.init(), "current v7 reboot succeeds");
    completeEffectDefaults(reboot);
    uint8_t rebooted[kEepromSize];
    EEPROM.copyPersisted(rebooted, sizeof(rebooted));
    expect(
      EEPROM.commitCount() == 0 && equalBytes(rebooted, persisted, sizeof(persisted)),
      "current v7 full boot is idempotent"
    );

    for (uint8_t version = 4; version <= 6; ++version) {
      writeMagicAndVersion(image, version);
      EEPROM.loadPersisted(image, sizeof(image));
      EEPROM.failNextCommit();
      EepromStore failed;
      expect(
        !failed.init() && !failed.isReady() && EEPROM.commitCount() == 1, "failed migration leaves store unavailable"
      );
      EEPROM.copyPersisted(rebooted, sizeof(rebooted));
      expect(equalBytes(rebooted, image, sizeof(image)), "failed migration preserves entire persisted image");
      EEPROM.loadPersisted(rebooted, sizeof(rebooted));
      EepromStore retry;
      expect(retry.init(), "migration retries after reboot");
      completeEffectDefaults(retry);
    }

    writeMagicAndVersion(image, 6);
    EEPROM.loadPersisted(image, sizeof(image));
    EepromStore interrupted;
    expect(interrupted.init(), "interrupted default initialization fixture migrates");
    EEPROM.copyPersisted(persisted, sizeof(persisted));
    EEPROM.failNextCommit();
    EffectSettings defaults[Effects::kCount] = {};
    interrupted.ensureEffectSettings(defaults);
    EEPROM.copyPersisted(rebooted, sizeof(rebooted));
    expect(
      equalBytes(rebooted, persisted, sizeof(persisted)) && rebooted[kEepromEffectSettingsCountAddr] == 0,
      "failed defaults commit retains persisted zero count for retry"
    );
    EEPROM.loadPersisted(rebooted, sizeof(rebooted));
    EepromStore retryDefaults;
    expect(retryDefaults.init() && EEPROM.commitCount() == 0, "v7 zero count resumes without migration");
    completeEffectDefaults(retryDefaults);
    expect(EEPROM.commitCount() == 1, "resumed boot commits defaults once");
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
  testV4MigrationAndCurrentRecords();
  testV6MigrationAndBoot();
  testFailedMigrationAndBindingRollback();
  if (parityPath != nullptr) {
    FILE* parityFile = fopen(parityPath, "wb");
    expect(
      parityImageReady && parityFile != nullptr &&
        fwrite(parityImage, 1, sizeof(parityImage), parityFile) == sizeof(parityImage),
      "writes deterministic v7 parity image"
    );
    if (parityFile != nullptr) fclose(parityFile);
  }
  if (failures != 0) {
    printf("FAILED: %d EEPROM fixture assertion(s)\n", failures);
    return 1;
  }
  printf("PASS: EEPROM v7 migration, catalog defaults and binding rollback fixtures\n");
  return 0;
}
