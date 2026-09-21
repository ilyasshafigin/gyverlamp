#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ControlPadProtocol {

  static const size_t kPairKeySize = 16;
  static const size_t kMacSize = 6;
  static const size_t kNonceSize = 16;
  static const size_t kTagSize = 16;
  static const size_t kSha256Size = 32;
  static const size_t kHeaderSize = 8;
  static const size_t kArtifactTextLength = 47;
  static const size_t kArtifactBufferSize = kArtifactTextLength + 1;
  static const size_t kBindingRecordSize = 32;
  static const size_t kMaxFrameSize = 69;

  static const uint8_t kProtocolVersion = 1;
  static const uint8_t kEncryptedFlag = 0x01;
  static const uint8_t kBindingMarker = 0xC1;
  static const uint8_t kBindingRecordVersion = 1;
  static const uint8_t kBindingFlagBound = 0x01;

  using HmacSha256Provider =
    bool (*)(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t dataLength, uint8_t out[32]);

  enum class MessageType : uint8_t {
    PairHello = 1,
    PairAccept = 2,
    EncryptedConfirm = 3,
    ConfirmAck = 4,
    Probe = 5,
    ProbeAck = 6,
    Command = 7,
    CommandAck = 8,
  };

  enum class CommandCode : uint8_t {
    NoAction = 0,
    TogglePower = 1,
    NextEffect = 2,
    PreviousEffect = 3,
    ToggleRotation = 4,
    SelectParameter = 5,
    AdjustParameter = 6,
    NextPalette = 7,
    SetPaletteAuto = 8,
    ResetCurrentEffectSettings = 9,
  };

  enum class ParameterTarget : uint8_t {
    None = 0,
    Brightness = 1,
    Speed = 2,
    Scale = 3,
  };

  enum class CommandAckStatus : uint8_t {
    Applied = 1,
    Expired = 2,
    Stale = 3,
  };

  struct Mac {
    uint8_t bytes[kMacSize];
  };

  struct Nonce {
    uint8_t bytes[kNonceSize];
  };

  struct Tag {
    uint8_t bytes[kTagSize];
  };

  struct WireHeader {
    uint8_t version;
    MessageType type;
    uint16_t payloadLength;
    uint8_t flags;
    uint8_t reserved;
  };

  // firstNonce is the panel nonce for pairing messages and the probe nonce for
  // recovery messages. secondNonce is used only by pairing messages.
  struct Message {
    MessageType type;
    Mac panelMac;
    Mac lampMac;
    Nonce firstNonce;
    Nonce secondNonce;
    uint8_t channel;
    Tag tag;
  };

  struct Command {
    Nonce panelBootNonce;
    uint32_t sequence;
    CommandCode code;
    ParameterTarget target;
    int8_t delta;
    Tag tag;
  };

  struct CommandAck {
    Nonce panelBootNonce;
    uint32_t sequence;
    CommandAckStatus status;
    Tag tag;
  };

  struct DerivedKeys {
    uint8_t boot[kSha256Size];
    uint8_t pmk[kPairKeySize];
    uint8_t lmk[kPairKeySize];
    uint8_t confirm[kSha256Size];
    uint8_t probe[kSha256Size];
  };

  struct BindingRecord {
    bool enabled;
    uint8_t protocolVersion;
    Mac panelMac;
    uint8_t lastChannel;
    uint8_t flags;
    uint8_t pairKey[kPairKeySize];
  };

  bool isAllZero(const uint8_t* data, size_t length);
  bool constantTimeEquals(const uint8_t* left, const uint8_t* right, size_t length);
  bool constantTimeTagEquals(const Tag& left, const Tag& right);

  uint32_t crc32IsoHdlc(const uint8_t* data, size_t length);

  bool formatArtifact(const uint8_t pairKey[kPairKeySize], char* output, size_t outputSize);
  bool parseArtifact(const char* input, size_t inputLength, uint8_t pairKey[kPairKeySize]);

  bool
  deriveBootstrapKey(const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider, uint8_t output[kSha256Size]);
  bool derivePmk(const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider, uint8_t output[kPairKeySize]);
  bool deriveLmk(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    uint8_t output[kPairKeySize]
  );
  bool deriveConfirmKey(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    const Nonce& panelNonce,
    const Nonce& lampNonce,
    HmacSha256Provider provider,
    uint8_t output[kSha256Size]
  );
  bool deriveProbeKey(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    uint8_t output[kSha256Size]
  );
  bool deriveCommandKey(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    uint8_t output[kSha256Size]
  );
  bool deriveKeys(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    const Nonce& panelNonce,
    const Nonce& lampNonce,
    HmacSha256Provider provider,
    DerivedKeys* output
  );

  bool encodeHeader(MessageType type, uint8_t output[kHeaderSize]);
  bool decodeHeader(const uint8_t* data, size_t length, WireHeader* output);
  bool encodeMessage(const Message& message, uint8_t* output, size_t outputCapacity, size_t* outputLength);
  bool decodeMessage(const uint8_t* data, size_t length, Message* output);

  bool calculateMessageTag(
    const Message& message, const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider, Tag* output
  );
  bool signMessage(Message* message, const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider);
  bool authenticateMessage(const Message& message, const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider);

  bool isValidCommand(const Command& command);
  bool isValidCommandAck(const CommandAck& ack);
  bool encodeCommand(const Command& command, uint8_t* output, size_t outputCapacity, size_t* outputLength);
  bool decodeCommand(const uint8_t* data, size_t length, Command* output);
  bool encodeCommandAck(const CommandAck& ack, uint8_t* output, size_t outputCapacity, size_t* outputLength);
  bool decodeCommandAck(const uint8_t* data, size_t length, CommandAck* output);
  bool calculateCommandTag(
    const Command& command,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    Tag* output
  );
  bool signCommand(
    Command* command,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  );
  bool authenticateCommand(
    const Command& command,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  );
  bool calculateCommandAckTag(
    const CommandAck& ack,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    Tag* output
  );
  bool signCommandAck(
    CommandAck* ack,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  );
  bool authenticateCommandAck(
    const CommandAck& ack,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  );

  void makeUnpairedBinding(BindingRecord* output);
  bool encodeBindingRecord(const BindingRecord& record, uint8_t output[kBindingRecordSize]);
  bool decodeBindingRecord(const uint8_t* data, size_t length, BindingRecord* output);

} // namespace ControlPadProtocol
