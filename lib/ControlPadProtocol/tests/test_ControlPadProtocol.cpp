#include "ControlPadProtocol/ControlPadProtocol.h"

#include <stdint.h>
#include <stdio.h>

namespace {

  using ControlPadProtocol::BindingRecord;
  using ControlPadProtocol::Command;
  using ControlPadProtocol::CommandAck;
  using ControlPadProtocol::CommandAckStatus;
  using ControlPadProtocol::CommandCode;
  using ControlPadProtocol::DerivedKeys;
  using ControlPadProtocol::Mac;
  using ControlPadProtocol::Message;
  using ControlPadProtocol::MessageType;
  using ControlPadProtocol::Nonce;
  using ControlPadProtocol::ParameterTarget;

  int gFailures = 0;

  void expect(bool condition, const char* name) {
    if (!condition) {
      ++gFailures;
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

  int hexValue(char character) {
    if (character >= '0' && character <= '9') {
      return character - '0';
    }
    if (character >= 'A' && character <= 'F') {
      return character - 'A' + 10;
    }
    return -1;
  }

  bool decodeHex(const char* text, uint8_t* output, size_t outputLength) {
    for (size_t index = 0; index < outputLength; ++index) {
      const int high = hexValue(text[index * 2]);
      const int low = hexValue(text[index * 2 + 1]);
      if (high < 0 || low < 0) {
        return false;
      }
      output[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return text[outputLength * 2] == '\0';
  }

  void rewriteBindingCrc(uint8_t record[ControlPadProtocol::kBindingRecordSize]) {
    const uint32_t crc = ControlPadProtocol::crc32IsoHdlc(record, 28);
    record[28] = static_cast<uint8_t>(crc & 0xff);
    record[29] = static_cast<uint8_t>((crc >> 8) & 0xff);
    record[30] = static_cast<uint8_t>((crc >> 16) & 0xff);
    record[31] = static_cast<uint8_t>((crc >> 24) & 0xff);
  }

  bool rejectsToUnpaired(const uint8_t record[ControlPadProtocol::kBindingRecordSize]) {
    BindingRecord decoded = {};
    return !ControlPadProtocol::decodeBindingRecord(record, ControlPadProtocol::kBindingRecordSize, &decoded) &&
           !decoded.enabled && decoded.flags == 0 && decoded.lastChannel == 0 &&
           ControlPadProtocol::isAllZero(decoded.panelMac.bytes, sizeof(decoded.panelMac.bytes));
  }

  // Test-only SHA-256/HMAC provider. The protocol library has no SHA-256 code.
  struct Sha256 {
    uint32_t state[8];
    uint64_t bitLength;
    uint8_t block[64];
    size_t blockLength;
  };

  uint32_t rotateRight(uint32_t value, uint8_t bits) {
    return (value >> bits) | (value << (32 - bits));
  }

  void transform(Sha256* context, const uint8_t block[64]) {
    static const uint32_t kRoundConstants[64] = {
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
      0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
      0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
      0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
      0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
      0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
      0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
      0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };
    uint32_t words[64];
    for (size_t index = 0; index < 16; ++index) {
      words[index] = (static_cast<uint32_t>(block[index * 4]) << 24) |
                     (static_cast<uint32_t>(block[index * 4 + 1]) << 16) |
                     (static_cast<uint32_t>(block[index * 4 + 2]) << 8) | static_cast<uint32_t>(block[index * 4 + 3]);
    }
    for (size_t index = 16; index < 64; ++index) {
      const uint32_t sigma0 =
        rotateRight(words[index - 15], 7) ^ rotateRight(words[index - 15], 18) ^ (words[index - 15] >> 3);
      const uint32_t sigma1 =
        rotateRight(words[index - 2], 17) ^ rotateRight(words[index - 2], 19) ^ (words[index - 2] >> 10);
      words[index] = words[index - 16] + sigma0 + words[index - 7] + sigma1;
    }
    uint32_t a = context->state[0];
    uint32_t b = context->state[1];
    uint32_t c = context->state[2];
    uint32_t d = context->state[3];
    uint32_t e = context->state[4];
    uint32_t f = context->state[5];
    uint32_t g = context->state[6];
    uint32_t h = context->state[7];
    for (size_t index = 0; index < 64; ++index) {
      const uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
      const uint32_t choose = (e & f) ^ ((~e) & g);
      const uint32_t temporary1 = h + sum1 + choose + kRoundConstants[index] + words[index];
      const uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
      const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temporary2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
  }

  void shaInit(Sha256* context) {
    static const uint32_t kInitialState[8] = {
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    for (size_t index = 0; index < 8; ++index) {
      context->state[index] = kInitialState[index];
    }
    context->bitLength = 0;
    context->blockLength = 0;
  }

  void shaUpdate(Sha256* context, const uint8_t* data, size_t length) {
    for (size_t index = 0; index < length; ++index) {
      context->block[context->blockLength++] = data[index];
      if (context->blockLength == sizeof(context->block)) {
        transform(context, context->block);
        context->bitLength += 512;
        context->blockLength = 0;
      }
    }
  }

  void shaFinal(Sha256* context, uint8_t output[32]) {
    context->bitLength += static_cast<uint64_t>(context->blockLength) * 8U;
    context->block[context->blockLength++] = 0x80;
    if (context->blockLength > 56) {
      while (context->blockLength < 64) {
        context->block[context->blockLength++] = 0;
      }
      transform(context, context->block);
      context->blockLength = 0;
    }
    while (context->blockLength < 56) {
      context->block[context->blockLength++] = 0;
    }
    for (size_t index = 0; index < 8; ++index) {
      context->block[63 - index] = static_cast<uint8_t>(context->bitLength >> (index * 8));
    }
    transform(context, context->block);
    for (size_t index = 0; index < 8; ++index) {
      output[index * 4] = static_cast<uint8_t>(context->state[index] >> 24);
      output[index * 4 + 1] = static_cast<uint8_t>(context->state[index] >> 16);
      output[index * 4 + 2] = static_cast<uint8_t>(context->state[index] >> 8);
      output[index * 4 + 3] = static_cast<uint8_t>(context->state[index]);
    }
  }

  bool
  testHmacSha256(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t dataLength, uint8_t output[32]) {
    if (key == 0 || data == 0 || output == 0) {
      return false;
    }
    uint8_t keyBlock[64] = {0};
    if (keyLength > sizeof(keyBlock)) {
      Sha256 hash;
      shaInit(&hash);
      shaUpdate(&hash, key, keyLength);
      shaFinal(&hash, keyBlock);
    } else {
      for (size_t index = 0; index < keyLength; ++index) {
        keyBlock[index] = key[index];
      }
    }
    uint8_t innerPad[64];
    uint8_t outerPad[64];
    for (size_t index = 0; index < sizeof(keyBlock); ++index) {
      innerPad[index] = static_cast<uint8_t>(keyBlock[index] ^ 0x36U);
      outerPad[index] = static_cast<uint8_t>(keyBlock[index] ^ 0x5cU);
    }
    uint8_t innerDigest[32];
    Sha256 inner;
    shaInit(&inner);
    shaUpdate(&inner, innerPad, sizeof(innerPad));
    shaUpdate(&inner, data, dataLength);
    shaFinal(&inner, innerDigest);
    Sha256 outer;
    shaInit(&outer);
    shaUpdate(&outer, outerPad, sizeof(outerPad));
    shaUpdate(&outer, innerDigest, sizeof(innerDigest));
    shaFinal(&outer, output);
    return true;
  }

  Message makeMessage(
    MessageType type,
    const Mac& panel,
    const Mac& lamp,
    const Nonce& panelNonce,
    const Nonce& lampNonce,
    const Nonce& probeNonce
  ) {
    Message message = {};
    message.type = type;
    message.panelMac = panel;
    message.lampMac = lamp;
    message.channel = 6;
    if (type == MessageType::Probe || type == MessageType::ProbeAck) {
      message.firstNonce = probeNonce;
    } else {
      message.firstNonce = panelNonce;
      message.secondNonce = lampNonce;
    }
    return message;
  }

  void testArtifactAndCrc(const uint8_t pairKey[16]) {
    char artifact[ControlPadProtocol::kArtifactBufferSize];
    expect(ControlPadProtocol::formatArtifact(pairKey, artifact, sizeof(artifact)), "artifact formats");
    expect(
      equalBytes(
        reinterpret_cast<const uint8_t*>(artifact),
        reinterpret_cast<const uint8_t*>("GLCP1-000102030405060708090A0B0C0D0E0F-6C3C9323"),
        ControlPadProtocol::kArtifactTextLength
      ),
      "artifact KAT"
    );
    uint8_t parsed[16] = {};
    expect(
      ControlPadProtocol::parseArtifact(artifact, ControlPadProtocol::kArtifactTextLength, parsed), "artifact parses"
    );
    expect(equalBytes(parsed, pairKey, sizeof(parsed)), "artifact round trip");
    artifact[46] = '2';
    expect(
      !ControlPadProtocol::parseArtifact(artifact, ControlPadProtocol::kArtifactTextLength, parsed),
      "artifact checksum rejects"
    );
    artifact[46] = '3';
    artifact[6] = 'a';
    expect(
      !ControlPadProtocol::parseArtifact(artifact, ControlPadProtocol::kArtifactTextLength, parsed),
      "artifact lowercase rejects"
    );
  }

  void testKdf(
    const uint8_t pairKey[16], const Mac& panel, const Mac& lamp, const Nonce& panelNonce, const Nonce& lampNonce
  ) {
    DerivedKeys keys = {};
    expect(
      ControlPadProtocol::deriveKeys(pairKey, panel, lamp, panelNonce, lampNonce, testHmacSha256, &keys), "KDF derives"
    );
    uint8_t expectedBoot[32];
    uint8_t expectedPmk[16];
    uint8_t expectedLmk[16];
    uint8_t expectedConfirm[32];
    uint8_t expectedProbe[32];
    expect(
      decodeHex(
        "41BB7100CCCCA8684A808C42A6F1E9C8BDE7FA691B271B73F2883B4BF4C8AA49", expectedBoot, sizeof(expectedBoot)
      ) &&
        decodeHex("F5984D988723FE4D88ABBF0B40530A72", expectedPmk, sizeof(expectedPmk)) &&
        decodeHex("A82C130CBBA492822C5FBAD17B20E346", expectedLmk, sizeof(expectedLmk)) &&
        decodeHex(
          "13A93B8613041F2357E8D298B670FC982F13D7CE808B0EC750A0D755DEEF2366", expectedConfirm, sizeof(expectedConfirm)
        ) &&
        decodeHex(
          "5A60EE196F10F827EF69491F1BD0C18FB2A43FC9B99164FA4A95969C150C64ED", expectedProbe, sizeof(expectedProbe)
        ),
      "KDF fixture decodes"
    );
    expect(equalBytes(keys.boot, expectedBoot, sizeof(keys.boot)), "K_boot KAT");
    expect(equalBytes(keys.pmk, expectedPmk, sizeof(keys.pmk)), "PMK KAT");
    expect(equalBytes(keys.lmk, expectedLmk, sizeof(keys.lmk)), "LMK KAT");
    expect(equalBytes(keys.confirm, expectedConfirm, sizeof(keys.confirm)), "K_confirm KAT");
    expect(equalBytes(keys.probe, expectedProbe, sizeof(keys.probe)), "K_probe KAT");

    Mac swappedPanel = lamp;
    Mac swappedLamp = panel;
    uint8_t swappedLmk[16] = {};
    uint8_t swappedConfirm[32] = {};
    uint8_t swappedProbe[32] = {};
    expect(
      ControlPadProtocol::deriveLmk(pairKey, swappedPanel, swappedLamp, testHmacSha256, swappedLmk) &&
        ControlPadProtocol::deriveConfirmKey(
          pairKey, swappedPanel, swappedLamp, panelNonce, lampNonce, testHmacSha256, swappedConfirm
        ) &&
        ControlPadProtocol::deriveProbeKey(pairKey, swappedPanel, swappedLamp, testHmacSha256, swappedProbe),
      "swapped MAC KDF derives"
    );
    expect(!equalBytes(keys.lmk, swappedLmk, sizeof(keys.lmk)), "swapped LMK differs");
    expect(!equalBytes(keys.confirm, swappedConfirm, sizeof(keys.confirm)), "swapped confirm differs");
    expect(!equalBytes(keys.probe, swappedProbe, sizeof(keys.probe)), "swapped probe differs");
  }

  void testFrames(
    const uint8_t pairKey[16],
    const Mac& panel,
    const Mac& lamp,
    const Nonce& panelNonce,
    const Nonce& lampNonce,
    const Nonce& probeNonce
  ) {
    static const char* const kFrames[] = {
      "4750010126000000020000000001101112131415161718191A1B1C1D1E1FF415FD627D410CA9888DB7640FD32558",
      "475001023D000000020000000001020000000002101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F0614A59F"
      "889246F67FBE612D9B6A1FA59D",
      "475001033D000100020000000001020000000002101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F063758F8"
      "02C9ACDA4BE323CB8C3C6FC612",
      "475001043D000100020000000001020000000002101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F06D1D7FC"
      "218448B82361B7B36763A56B24",
      "475001052C000100020000000001020000000002303132333435363738393A3B3C3D3E3F13767876B3EE3E15185EACDFF9FE6936",
      "475001062D000100020000000001020000000002303132333435363738393A3B3C3D3E3F06F0835514CD2972646C58595BAACD8903"
    };
    static const MessageType kTypes[] = {
      MessageType::PairHello,
      MessageType::PairAccept,
      MessageType::EncryptedConfirm,
      MessageType::ConfirmAck,
      MessageType::Probe,
      MessageType::ProbeAck
    };
    for (size_t index = 0; index < sizeof(kTypes) / sizeof(kTypes[0]); ++index) {
      Message message = makeMessage(kTypes[index], panel, lamp, panelNonce, lampNonce, probeNonce);
      expect(ControlPadProtocol::signMessage(&message, pairKey, testHmacSha256), "frame signs");
      uint8_t expected[ControlPadProtocol::kMaxFrameSize] = {};
      const size_t expectedLength = ControlPadProtocol::kHeaderSize + (kTypes[index] == MessageType::PairHello  ? 38
                                                                       : kTypes[index] == MessageType::Probe    ? 44
                                                                       : kTypes[index] == MessageType::ProbeAck ? 45
                                                                                                                : 61);
      expect(decodeHex(kFrames[index], expected, expectedLength), "frame fixture decodes");
      uint8_t encoded[ControlPadProtocol::kMaxFrameSize] = {};
      size_t encodedLength = 0;
      expect(ControlPadProtocol::encodeMessage(message, encoded, sizeof(encoded), &encodedLength), "frame encodes");
      expect(encodedLength == expectedLength && equalBytes(encoded, expected, expectedLength), "frame KAT");
      Message decoded = {};
      expect(ControlPadProtocol::decodeMessage(encoded, encodedLength, &decoded), "frame decodes");
      expect(ControlPadProtocol::authenticateMessage(decoded, pairKey, testHmacSha256), "frame authenticates");
      encoded[encodedLength - 1] ^= 0x01;
      expect(
        ControlPadProtocol::decodeMessage(encoded, encodedLength, &decoded) &&
          !ControlPadProtocol::authenticateMessage(decoded, pairKey, testHmacSha256),
        "invalid tag rejects"
      );
    }
  }

  Command makeCommand(const Nonce& bootNonce) {
    Command command = {};
    command.panelBootNonce = bootNonce;
    command.sequence = 0x01020304U;
    command.code = CommandCode::AdjustParameter;
    command.target = ParameterTarget::Speed;
    command.delta = -4;
    return command;
  }

  void testCommandFrames(const uint8_t pairKey[16], const Mac& panel, const Mac& lamp) {
    Nonce bootNonce = {};
    uint8_t expectedKey[32] = {};
    uint8_t expectedCommand[47] = {};
    uint8_t expectedAck[45] = {};
    expect(
      decodeHex("404142434445464748494A4B4C4D4E4F", bootNonce.bytes, sizeof(bootNonce.bytes)) &&
        decodeHex(
          "4D5E21321524733F8BE0C8293515A39E2A26C445C7B9D4F1454A6A6BFC3B7621", expectedKey, sizeof(expectedKey)
        ) &&
        decodeHex(
          "4750010727000100404142434445464748494A4B4C4D4E4F040302010602FC1C79049A1CAE16327546F16DD7BC34A1",
          expectedCommand,
          sizeof(expectedCommand)
        ) &&
        decodeHex(
          "4750010825000100404142434445464748494A4B4C4D4E4F04030201018C95915997786CEC5DA63439032C9791",
          expectedAck,
          sizeof(expectedAck)
        ),
      "command fixtures decode"
    );
    uint8_t commandKey[32] = {};
    expect(
      ControlPadProtocol::deriveCommandKey(pairKey, panel, lamp, testHmacSha256, commandKey) &&
        equalBytes(commandKey, expectedKey, sizeof(commandKey)),
      "K_command KAT"
    );

    Command command = makeCommand(bootNonce);
    expect(
      ControlPadProtocol::isValidCommand(command) &&
        ControlPadProtocol::signCommand(&command, pairKey, panel, lamp, testHmacSha256),
      "command signs"
    );
    uint8_t encodedCommand[ControlPadProtocol::kMaxFrameSize] = {};
    size_t commandLength = 0;
    expect(
      ControlPadProtocol::encodeCommand(command, encodedCommand, sizeof(encodedCommand), &commandLength) &&
        commandLength == sizeof(expectedCommand) &&
        equalBytes(encodedCommand, expectedCommand, sizeof(expectedCommand)),
      "Command KAT"
    );
    Command decodedCommand = {};
    expect(
      ControlPadProtocol::decodeCommand(encodedCommand, commandLength, &decodedCommand) &&
        ControlPadProtocol::authenticateCommand(decodedCommand, pairKey, panel, lamp, testHmacSha256),
      "command round trip authenticates"
    );

    CommandAck ack = {};
    ack.panelBootNonce = bootNonce;
    ack.sequence = command.sequence;
    ack.status = CommandAckStatus::Applied;
    expect(
      ControlPadProtocol::isValidCommandAck(ack) &&
        ControlPadProtocol::signCommandAck(&ack, pairKey, panel, lamp, testHmacSha256),
      "command ack signs"
    );
    uint8_t encodedAck[ControlPadProtocol::kMaxFrameSize] = {};
    size_t ackLength = 0;
    expect(
      ControlPadProtocol::encodeCommandAck(ack, encodedAck, sizeof(encodedAck), &ackLength) &&
        ackLength == sizeof(expectedAck) && equalBytes(encodedAck, expectedAck, sizeof(expectedAck)),
      "CommandAck KAT"
    );
    CommandAck decodedAck = {};
    expect(
      ControlPadProtocol::decodeCommandAck(encodedAck, ackLength, &decodedAck) &&
        ControlPadProtocol::authenticateCommandAck(decodedAck, pairKey, panel, lamp, testHmacSha256),
      "command ack round trip authenticates"
    );

    Mac swappedPanel = lamp;
    Mac swappedLamp = panel;
    uint8_t swappedKey[32] = {};
    expect(
      ControlPadProtocol::deriveCommandKey(pairKey, swappedPanel, swappedLamp, testHmacSha256, swappedKey) &&
        !equalBytes(commandKey, swappedKey, sizeof(commandKey)) &&
        !ControlPadProtocol::authenticateCommand(command, pairKey, swappedPanel, swappedLamp, testHmacSha256) &&
        !ControlPadProtocol::authenticateCommandAck(ack, pairKey, swappedPanel, swappedLamp, testHmacSha256),
      "command MAC context rejects source mismatch"
    );

    Command tamperedCommand = command;
    tamperedCommand.panelBootNonce.bytes[0] ^= 0x01;
    expect(
      !ControlPadProtocol::authenticateCommand(tamperedCommand, pairKey, panel, lamp, testHmacSha256),
      "command nonce tamper rejects"
    );
    tamperedCommand = command;
    ++tamperedCommand.sequence;
    expect(
      !ControlPadProtocol::authenticateCommand(tamperedCommand, pairKey, panel, lamp, testHmacSha256),
      "command sequence tamper rejects"
    );
    tamperedCommand = command;
    tamperedCommand.code = CommandCode::SelectParameter;
    tamperedCommand.delta = 0;
    expect(
      !ControlPadProtocol::authenticateCommand(tamperedCommand, pairKey, panel, lamp, testHmacSha256),
      "command code tamper rejects"
    );
    tamperedCommand = command;
    tamperedCommand.target = ParameterTarget::Scale;
    expect(
      !ControlPadProtocol::authenticateCommand(tamperedCommand, pairKey, panel, lamp, testHmacSha256),
      "command target tamper rejects"
    );
    tamperedCommand = command;
    tamperedCommand.delta = -3;
    expect(
      !ControlPadProtocol::authenticateCommand(tamperedCommand, pairKey, panel, lamp, testHmacSha256),
      "command delta tamper rejects"
    );
    tamperedCommand = command;
    tamperedCommand.tag.bytes[0] ^= 0x01;
    expect(
      !ControlPadProtocol::authenticateCommand(tamperedCommand, pairKey, panel, lamp, testHmacSha256),
      "command tag tamper rejects"
    );
    CommandAck tamperedAck = ack;
    tamperedAck.status = CommandAckStatus::Expired;
    expect(
      !ControlPadProtocol::authenticateCommandAck(tamperedAck, pairKey, panel, lamp, testHmacSha256),
      "command ack status tamper rejects"
    );
    tamperedAck = ack;
    tamperedAck.tag.bytes[0] ^= 0x01;
    expect(
      !ControlPadProtocol::authenticateCommandAck(tamperedAck, pairKey, panel, lamp, testHmacSha256),
      "command ack tag tamper rejects"
    );
  }

  void testMalformedCommands() {
    Nonce bootNonce = {};
    expect(
      decodeHex("404142434445464748494A4B4C4D4E4F", bootNonce.bytes, sizeof(bootNonce.bytes)),
      "command nonce fixture decodes"
    );
    Command command = makeCommand(bootNonce);
    CommandAck ack = {};
    ack.panelBootNonce = bootNonce;
    ack.sequence = command.sequence;
    ack.status = CommandAckStatus::Applied;
    uint8_t commandFrame[47] = {};
    size_t commandLength = 0;
    expect(
      ControlPadProtocol::encodeCommand(command, commandFrame, sizeof(commandFrame), &commandLength),
      "unsigned command encodes for malformed tests"
    );
    uint8_t validCommandFrame[47] = {};
    for (size_t index = 0; index < commandLength; ++index) {
      validCommandFrame[index] = commandFrame[index];
    }
    Command commandOutput = {};
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength - 1, &commandOutput),
      "command truncated frame rejects"
    );
    commandFrame[0] = 'X';
    expect(!ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput), "command magic rejects");
    commandFrame[0] = 'G';
    commandFrame[2] = 2;
    expect(!ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput), "command version rejects");
    commandFrame[2] = 1;
    commandFrame[3] = static_cast<uint8_t>(MessageType::CommandAck);
    expect(!ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput), "command type rejects");
    commandFrame[3] = static_cast<uint8_t>(MessageType::Command);
    commandFrame[4] = 38;
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput), "command payload length rejects"
    );
    commandFrame[4] = 39;
    commandFrame[6] = 0;
    expect(!ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput), "command flags reject");
    commandFrame[6] = 1;
    commandFrame[7] = 1;
    expect(!ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput), "command reserved rejects");
    for (size_t index = 0; index < commandLength; ++index) {
      commandFrame[index] = validCommandFrame[index];
    }
    for (size_t index = 8; index < 24; ++index) {
      commandFrame[index] = 0;
    }
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput),
      "command decoded zero nonce rejects"
    );
    for (size_t index = 0; index < commandLength; ++index) {
      commandFrame[index] = validCommandFrame[index];
    }
    for (size_t index = 24; index < 28; ++index) {
      commandFrame[index] = 0;
    }
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput),
      "command decoded zero sequence rejects"
    );
    for (size_t index = 0; index < commandLength; ++index) {
      commandFrame[index] = validCommandFrame[index];
    }
    commandFrame[28] = 7;
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput),
      "command decoded unknown code rejects"
    );
    commandFrame[28] = static_cast<uint8_t>(CommandCode::TogglePower);
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput),
      "command decoded basic target rejects"
    );
    for (size_t index = 0; index < commandLength; ++index) {
      commandFrame[index] = validCommandFrame[index];
    }
    commandFrame[30] = 0;
    expect(
      !ControlPadProtocol::decodeCommand(commandFrame, commandLength, &commandOutput),
      "command decoded zero delta rejects"
    );

    Command invalid = command;
    for (size_t index = 0; index < sizeof(invalid.panelBootNonce.bytes); ++index) {
      invalid.panelBootNonce.bytes[index] = 0;
    }
    expect(!ControlPadProtocol::isValidCommand(invalid), "command zero nonce rejects");
    invalid = command;
    invalid.sequence = 0;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command zero sequence rejects");
    invalid = command;
    invalid.code = static_cast<CommandCode>(7);
    expect(!ControlPadProtocol::isValidCommand(invalid), "command unknown code rejects");
    invalid = command;
    invalid.target = static_cast<ParameterTarget>(4);
    expect(!ControlPadProtocol::isValidCommand(invalid), "command unknown target rejects");
    invalid = command;
    invalid.code = CommandCode::TogglePower;
    invalid.target = ParameterTarget::Speed;
    invalid.delta = 0;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command basic target rejects");
    invalid.target = ParameterTarget::None;
    invalid.delta = 1;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command basic delta rejects");
    invalid = command;
    invalid.code = CommandCode::SelectParameter;
    invalid.target = ParameterTarget::None;
    invalid.delta = 0;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command select none rejects");
    invalid.target = ParameterTarget::Speed;
    invalid.delta = 1;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command select delta rejects");
    invalid = command;
    invalid.target = ParameterTarget::None;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command adjust none rejects");
    invalid = command;
    invalid.delta = 0;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command adjust zero rejects");
    invalid.delta = 9;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command adjust high delta rejects");
    invalid.delta = -9;
    expect(!ControlPadProtocol::isValidCommand(invalid), "command adjust low delta rejects");

    CommandAck invalidAck = ack;
    for (size_t index = 0; index < sizeof(invalidAck.panelBootNonce.bytes); ++index) {
      invalidAck.panelBootNonce.bytes[index] = 0;
    }
    expect(!ControlPadProtocol::isValidCommandAck(invalidAck), "ack zero nonce rejects");
    invalidAck = ack;
    invalidAck.sequence = 0;
    expect(!ControlPadProtocol::isValidCommandAck(invalidAck), "ack zero sequence rejects");
    invalidAck.status = static_cast<CommandAckStatus>(4);
    expect(!ControlPadProtocol::isValidCommandAck(invalidAck), "ack unknown status rejects");
    uint8_t ackFrame[45] = {};
    size_t ackLength = 0;
    expect(
      ControlPadProtocol::encodeCommandAck(ack, ackFrame, sizeof(ackFrame), &ackLength),
      "unsigned ack encodes for malformed tests"
    );
    uint8_t validAckFrame[45] = {};
    for (size_t index = 0; index < ackLength; ++index) {
      validAckFrame[index] = ackFrame[index];
    }
    CommandAck ackOutput = {};
    ackFrame[4] = 36;
    expect(!ControlPadProtocol::decodeCommandAck(ackFrame, ackLength, &ackOutput), "ack payload length rejects");
    for (size_t index = 0; index < ackLength; ++index) {
      ackFrame[index] = validAckFrame[index];
    }
    for (size_t index = 8; index < 24; ++index) {
      ackFrame[index] = 0;
    }
    expect(!ControlPadProtocol::decodeCommandAck(ackFrame, ackLength, &ackOutput), "ack decoded zero nonce rejects");
    for (size_t index = 0; index < ackLength; ++index) {
      ackFrame[index] = validAckFrame[index];
    }
    ackFrame[28] = 4;
    expect(
      !ControlPadProtocol::decodeCommandAck(ackFrame, ackLength, &ackOutput), "ack decoded unknown status rejects"
    );
  }

  void testMalformedFrames() {
    uint8_t frame[46] = {};
    frame[0] = 'G';
    frame[1] = 'P';
    frame[2] = 1;
    frame[3] = 1;
    frame[4] = 38;
    Message output = {};
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame) - 1, &output), "truncated frame rejects");
    frame[0] = 'X';
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame), &output), "magic rejects");
    frame[0] = 'G';
    frame[2] = 2;
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame), &output), "version rejects");
    frame[2] = 1;
    frame[3] = 7;
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame), &output), "type rejects");
    frame[3] = 1;
    frame[4] = 37;
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame), &output), "payload length rejects");
    frame[4] = 38;
    frame[6] = 1;
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame), &output), "flags rejects");
    frame[6] = 0;
    frame[7] = 1;
    expect(!ControlPadProtocol::decodeMessage(frame, sizeof(frame), &output), "reserved rejects");
  }

  void testBindingRecords(const uint8_t pairKey[16], const Mac& panel) {
    static const char kPaired[] = "C10101010200000000010601000102030405060708090A0B0C0D0E0FA0FC28BF";
    static const char kUnpaired[] = "C10100010000000000000000000000000000000000000000000000007BC843FD";
    BindingRecord paired = {};
    paired.enabled = true;
    paired.protocolVersion = 1;
    paired.panelMac = panel;
    paired.lastChannel = 6;
    paired.flags = ControlPadProtocol::kBindingFlagBound;
    for (size_t index = 0; index < sizeof(paired.pairKey); ++index) {
      paired.pairKey[index] = pairKey[index];
    }
    uint8_t encoded[ControlPadProtocol::kBindingRecordSize] = {};
    uint8_t expectedPaired[ControlPadProtocol::kBindingRecordSize] = {};
    uint8_t expectedUnpaired[ControlPadProtocol::kBindingRecordSize] = {};
    expect(
      decodeHex(kPaired, expectedPaired, sizeof(expectedPaired)) &&
        decodeHex(kUnpaired, expectedUnpaired, sizeof(expectedUnpaired)),
      "EEPROM fixtures decode"
    );
    expect(
      ControlPadProtocol::encodeBindingRecord(paired, encoded) && equalBytes(encoded, expectedPaired, sizeof(encoded)),
      "paired EEPROM CRC KAT"
    );
    BindingRecord decoded = {};
    expect(
      ControlPadProtocol::decodeBindingRecord(encoded, sizeof(encoded), &decoded) && decoded.enabled &&
        decoded.flags == ControlPadProtocol::kBindingFlagBound && equalBytes(decoded.pairKey, pairKey, 16),
      "paired EEPROM round trip"
    );

    BindingRecord boundary = paired;
    boundary.lastChannel = 0;
    expect(ControlPadProtocol::encodeBindingRecord(boundary, encoded), "binding channel zero accepts");
    boundary.lastChannel = 14;
    expect(ControlPadProtocol::encodeBindingRecord(boundary, encoded), "binding channel fourteen accepts");
    boundary.lastChannel = 15;
    expect(!ControlPadProtocol::encodeBindingRecord(boundary, encoded), "binding channel above fourteen rejects");

    expect(ControlPadProtocol::encodeBindingRecord(paired, encoded), "paired EEPROM prepares malformed records");
    for (size_t index = 4; index < 10; ++index) {
      encoded[index] = 0;
    }
    rewriteBindingCrc(encoded);
    expect(rejectsToUnpaired(encoded), "binding all-zero MAC rejects");

    expect(ControlPadProtocol::encodeBindingRecord(paired, encoded), "paired EEPROM prepares multicast record");
    encoded[4] = static_cast<uint8_t>(encoded[4] | 0x01U);
    rewriteBindingCrc(encoded);
    expect(rejectsToUnpaired(encoded), "binding multicast MAC rejects");

    expect(ControlPadProtocol::encodeBindingRecord(paired, encoded), "paired EEPROM prepares broadcast record");
    for (size_t index = 4; index < 10; ++index) {
      encoded[index] = 0xff;
    }
    rewriteBindingCrc(encoded);
    expect(rejectsToUnpaired(encoded), "binding broadcast MAC rejects");

    expect(ControlPadProtocol::encodeBindingRecord(paired, encoded), "paired EEPROM prepares invalid channel record");
    encoded[10] = 15;
    rewriteBindingCrc(encoded);
    expect(rejectsToUnpaired(encoded), "binding stored channel above fourteen rejects");

    BindingRecord unpaired = {};
    ControlPadProtocol::makeUnpairedBinding(&unpaired);
    expect(
      ControlPadProtocol::encodeBindingRecord(unpaired, encoded) &&
        equalBytes(encoded, expectedUnpaired, sizeof(encoded)),
      "unpaired EEPROM CRC KAT"
    );
    encoded[31] ^= 0x01;
    expect(
      !ControlPadProtocol::decodeBindingRecord(encoded, sizeof(encoded), &decoded) && !decoded.enabled &&
        decoded.flags == 0,
      "EEPROM checksum rejects to unpaired"
    );
  }

  void testAllZeroKey(
    const Mac& panel, const Mac& lamp, const Nonce& panelNonce, const Nonce& lampNonce, const Nonce& probeNonce
  ) {
    uint8_t zeroKey[16] = {};
    uint8_t output[32] = {};
    char artifact[ControlPadProtocol::kArtifactBufferSize] = {};
    Message message = makeMessage(MessageType::PairHello, panel, lamp, panelNonce, lampNonce, probeNonce);
    expect(!ControlPadProtocol::deriveBootstrapKey(zeroKey, testHmacSha256, output), "all-zero KDF key rejects");
    expect(!ControlPadProtocol::formatArtifact(zeroKey, artifact, sizeof(artifact)), "all-zero artifact key rejects");
    static const char kAllZeroArtifact[] = "GLCP1-00000000000000000000000000000000-C9EFF1BD";
    expect(
      !ControlPadProtocol::parseArtifact(kAllZeroArtifact, ControlPadProtocol::kArtifactTextLength, zeroKey),
      "all-zero artifact parse rejects"
    );
    expect(!ControlPadProtocol::signMessage(&message, zeroKey, testHmacSha256), "all-zero tag key rejects");
    Command command = makeCommand(panelNonce);
    expect(
      !ControlPadProtocol::signCommand(&command, zeroKey, panel, lamp, testHmacSha256), "all-zero command key rejects"
    );
    BindingRecord invalid = {};
    invalid.enabled = true;
    invalid.protocolVersion = 1;
    invalid.panelMac = panel;
    invalid.flags = ControlPadProtocol::kBindingFlagBound;
    uint8_t encoded[ControlPadProtocol::kBindingRecordSize] = {};
    expect(!ControlPadProtocol::encodeBindingRecord(invalid, encoded), "all-zero binding key rejects");
  }

} // namespace

int main() {
  uint8_t pairKey[16];
  Mac panel = {};
  Mac lamp = {};
  Nonce panelNonce = {};
  Nonce lampNonce = {};
  Nonce probeNonce = {};
  expect(
    decodeHex("000102030405060708090A0B0C0D0E0F", pairKey, sizeof(pairKey)) &&
      decodeHex("020000000001", panel.bytes, sizeof(panel.bytes)) &&
      decodeHex("020000000002", lamp.bytes, sizeof(lamp.bytes)) &&
      decodeHex("101112131415161718191A1B1C1D1E1F", panelNonce.bytes, sizeof(panelNonce.bytes)) &&
      decodeHex("202122232425262728292A2B2C2D2E2F", lampNonce.bytes, sizeof(lampNonce.bytes)) &&
      decodeHex("303132333435363738393A3B3C3D3E3F", probeNonce.bytes, sizeof(probeNonce.bytes)),
    "base fixtures decode"
  );
  testArtifactAndCrc(pairKey);
  testKdf(pairKey, panel, lamp, panelNonce, lampNonce);
  testFrames(pairKey, panel, lamp, panelNonce, lampNonce, probeNonce);
  testCommandFrames(pairKey, panel, lamp);
  testMalformedCommands();
  testMalformedFrames();
  testBindingRecords(pairKey, panel);
  testAllZeroKey(panel, lamp, panelNonce, lampNonce, probeNonce);
  if (gFailures != 0) {
    printf("FAILED: %d test assertion(s)\n", gFailures);
    return 1;
  }
  printf("PASS: ControlPadProtocol KAT and malformed-input tests\n");
  return 0;
}
