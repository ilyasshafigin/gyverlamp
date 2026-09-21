#include "ControlPadProtocol/ControlPadProtocol.h"

namespace ControlPadProtocol {
  namespace {

    static const uint8_t kMagic0 = 'G';
    static const uint8_t kMagic1 = 'P';
    static const uint8_t kArtifactSchemaVersion = 0x01;
    static const char kBootstrapLabel[] = "GyverLamp/control-pad/bootstrap/v1";
    static const char kPmkLabel[] = "GyverLamp/control-pad/pmk/v1";
    static const char kLmkLabel[] = "GyverLamp/control-pad/lmk/v1";
    static const char kConfirmLabel[] = "GyverLamp/control-pad/confirm/v1";
    static const char kProbeLabel[] = "GyverLamp/control-pad/probe/v1";
    static const char kCommandLabel[] = "GyverLamp/control-pad/command/v1";
    static const size_t kMaxPrfInputSize = 96;

    void copyBytes(uint8_t* destination, const uint8_t* source, size_t length) {
      for (size_t index = 0; index < length; ++index) {
        destination[index] = source[index];
      }
    }

    void zeroBytes(uint8_t* destination, size_t length) {
      for (size_t index = 0; index < length; ++index) {
        destination[index] = 0;
      }
    }

    bool isKnownMessageType(MessageType type) {
      switch (type) {
        case MessageType::PairHello:
        case MessageType::PairAccept:
        case MessageType::EncryptedConfirm:
        case MessageType::ConfirmAck:
        case MessageType::Probe:
        case MessageType::ProbeAck:
        case MessageType::Command:
        case MessageType::CommandAck: return true;
      }
      return false;
    }

    size_t payloadLengthFor(MessageType type) {
      switch (type) {
        case MessageType::PairHello: return 38;
        case MessageType::PairAccept:
        case MessageType::EncryptedConfirm:
        case MessageType::ConfirmAck: return 61;
        case MessageType::Probe: return 44;
        case MessageType::ProbeAck: return 45;
        case MessageType::Command: return 39;
        case MessageType::CommandAck: return 37;
      }
      return 0;
    }

    uint8_t flagsFor(MessageType type) {
      switch (type) {
        case MessageType::PairHello:
        case MessageType::PairAccept: return 0;
        case MessageType::EncryptedConfirm:
        case MessageType::ConfirmAck:
        case MessageType::Probe:
        case MessageType::ProbeAck:
        case MessageType::Command:
        case MessageType::CommandAck: return kEncryptedFlag;
      }
      return 0xff;
    }

    bool isPairingMessageType(MessageType type) {
      return type == MessageType::PairHello || type == MessageType::PairAccept ||
             type == MessageType::EncryptedConfirm || type == MessageType::ConfirmAck || type == MessageType::Probe ||
             type == MessageType::ProbeAck;
    }

    bool isKnownCommandCode(CommandCode code) {
      switch (code) {
        case CommandCode::TogglePower:
        case CommandCode::NextEffect:
        case CommandCode::PreviousEffect:
        case CommandCode::ToggleRotation:
        case CommandCode::SelectParameter:
        case CommandCode::AdjustParameter:
        case CommandCode::NextPalette:
        case CommandCode::SetPaletteAuto:
        case CommandCode::ResetCurrentEffectSettings: return true;
        case CommandCode::NoAction: return false;
      }
      return false;
    }

    bool isKnownParameterTarget(ParameterTarget target) {
      switch (target) {
        case ParameterTarget::None:
        case ParameterTarget::Brightness:
        case ParameterTarget::Speed:
        case ParameterTarget::Scale: return true;
      }
      return false;
    }

    bool isKnownCommandAckStatus(CommandAckStatus status) {
      switch (status) {
        case CommandAckStatus::Applied:
        case CommandAckStatus::Expired:
        case CommandAckStatus::Stale: return true;
      }
      return false;
    }

    void writeUint32Le(uint8_t* output, uint32_t value) {
      output[0] = static_cast<uint8_t>(value & 0xffU);
      output[1] = static_cast<uint8_t>((value >> 8) & 0xffU);
      output[2] = static_cast<uint8_t>((value >> 16) & 0xffU);
      output[3] = static_cast<uint8_t>((value >> 24) & 0xffU);
    }

    uint32_t readUint32Le(const uint8_t* input) {
      return static_cast<uint32_t>(input[0]) | (static_cast<uint32_t>(input[1]) << 8) |
             (static_cast<uint32_t>(input[2]) << 16) | (static_cast<uint32_t>(input[3]) << 24);
    }

    bool append(uint8_t* destination, size_t capacity, size_t* offset, const uint8_t* source, size_t length) {
      if (*offset > capacity || length > capacity - *offset) {
        return false;
      }
      copyBytes(destination + *offset, source, length);
      *offset += length;
      return true;
    }

    bool prf(
      const uint8_t pairKey[kPairKeySize],
      const char* label,
      size_t labelLength,
      const uint8_t* context,
      size_t contextLength,
      HmacSha256Provider provider,
      uint8_t output[kSha256Size]
    ) {
      if (pairKey == 0 || label == 0 || provider == 0 || output == 0 || isAllZero(pairKey, kPairKeySize)) {
        return false;
      }

      uint8_t input[kMaxPrfInputSize];
      size_t offset = 0;
      if (!append(input, sizeof(input), &offset, reinterpret_cast<const uint8_t*>(label), labelLength)) {
        return false;
      }
      const uint8_t separator = 0;
      if (!append(input, sizeof(input), &offset, &separator, 1)) {
        return false;
      }
      if (contextLength != 0 && !append(input, sizeof(input), &offset, context, contextLength)) {
        return false;
      }
      return provider(pairKey, kPairKeySize, input, offset, output);
    }

    bool deriveForMessage(
      const Message& message,
      const uint8_t pairKey[kPairKeySize],
      HmacSha256Provider provider,
      uint8_t output[kSha256Size],
      size_t* keyLength
    ) {
      switch (message.type) {
        case MessageType::PairHello:
        case MessageType::PairAccept: *keyLength = kSha256Size; return deriveBootstrapKey(pairKey, provider, output);
        case MessageType::EncryptedConfirm:
        case MessageType::ConfirmAck:
          *keyLength = kSha256Size;
          return deriveConfirmKey(
            pairKey, message.panelMac, message.lampMac, message.firstNonce, message.secondNonce, provider, output
          );
        case MessageType::Probe:
        case MessageType::ProbeAck:
          *keyLength = kSha256Size;
          return deriveProbeKey(pairKey, message.panelMac, message.lampMac, provider, output);
        case MessageType::Command:
        case MessageType::CommandAck: return false;
      }
      return false;
    }

    bool bindingMacIsValid(const Mac& mac) {
      return !isAllZero(mac.bytes, kMacSize) && (mac.bytes[0] & 0x01U) == 0;
    }

    bool bindingShapeIsValid(const BindingRecord& record) {
      if (
        record.protocolVersion != kProtocolVersion || (record.flags & static_cast<uint8_t>(~kBindingFlagBound)) != 0
      ) {
        return false;
      }
      const bool bound = (record.flags & kBindingFlagBound) != 0;
      if (record.enabled && !bound) {
        return false;
      }
      if (bound) {
        return !isAllZero(record.pairKey, kPairKeySize) && bindingMacIsValid(record.panelMac) &&
               record.lastChannel <= 14;
      }
      return !record.enabled && record.lastChannel == 0 && isAllZero(record.panelMac.bytes, kMacSize) &&
             isAllZero(record.pairKey, kPairKeySize);
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

    char hexCharacter(uint8_t value) {
      static const char kHex[] = "0123456789ABCDEF";
      return kHex[value & 0x0f];
    }

  } // namespace

  bool isAllZero(const uint8_t* data, size_t length) {
    if (data == 0) {
      return true;
    }
    uint8_t combined = 0;
    for (size_t index = 0; index < length; ++index) {
      combined = static_cast<uint8_t>(combined | data[index]);
    }
    return combined == 0;
  }

  bool constantTimeEquals(const uint8_t* left, const uint8_t* right, size_t length) {
    if (left == 0 || right == 0) {
      return false;
    }
    volatile uint8_t difference = 0;
    for (size_t index = 0; index < length; ++index) {
      difference = static_cast<uint8_t>(difference | (left[index] ^ right[index]));
    }
    return difference == 0;
  }

  bool constantTimeTagEquals(const Tag& left, const Tag& right) {
    return constantTimeEquals(left.bytes, right.bytes, kTagSize);
  }

  uint32_t crc32IsoHdlc(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffffUL;
    for (size_t index = 0; index < length; ++index) {
      crc ^= data[index];
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint32_t mask = static_cast<uint32_t>(-(crc & 1U));
        crc = (crc >> 1) ^ (0xedb88320UL & mask);
      }
    }
    return ~crc;
  }

  bool formatArtifact(const uint8_t pairKey[kPairKeySize], char* output, size_t outputSize) {
    if (pairKey == 0 || output == 0 || outputSize < kArtifactBufferSize || isAllZero(pairKey, kPairKeySize)) {
      return false;
    }
    static const char kPrefix[] = "GLCP1-";
    for (size_t index = 0; index < sizeof(kPrefix) - 1; ++index) {
      output[index] = kPrefix[index];
    }
    for (size_t index = 0; index < kPairKeySize; ++index) {
      output[6 + index * 2] = hexCharacter(pairKey[index] >> 4);
      output[7 + index * 2] = hexCharacter(pairKey[index]);
    }
    output[38] = '-';
    uint8_t crcInput[kPairKeySize + 1];
    crcInput[0] = kArtifactSchemaVersion;
    copyBytes(crcInput + 1, pairKey, kPairKeySize);
    const uint32_t crc = crc32IsoHdlc(crcInput, sizeof(crcInput));
    for (size_t index = 0; index < 8; ++index) {
      const uint8_t shift = static_cast<uint8_t>((7 - index) * 4);
      output[39 + index] = hexCharacter(static_cast<uint8_t>(crc >> shift));
    }
    output[kArtifactTextLength] = '\0';
    return true;
  }

  bool parseArtifact(const char* input, size_t inputLength, uint8_t pairKey[kPairKeySize]) {
    if (input == 0 || pairKey == 0 || inputLength != kArtifactTextLength) {
      return false;
    }
    static const char kPrefix[] = "GLCP1-";
    for (size_t index = 0; index < sizeof(kPrefix) - 1; ++index) {
      if (input[index] != kPrefix[index]) {
        return false;
      }
    }
    if (input[38] != '-') {
      return false;
    }
    uint8_t parsedKey[kPairKeySize];
    for (size_t index = 0; index < kPairKeySize; ++index) {
      const int high = hexValue(input[6 + index * 2]);
      const int low = hexValue(input[7 + index * 2]);
      if (high < 0 || low < 0) {
        return false;
      }
      parsedKey[index] = static_cast<uint8_t>((high << 4) | low);
    }
    if (isAllZero(parsedKey, sizeof(parsedKey))) {
      return false;
    }
    uint32_t suppliedCrc = 0;
    for (size_t index = 0; index < 8; ++index) {
      const int nibble = hexValue(input[39 + index]);
      if (nibble < 0) {
        return false;
      }
      suppliedCrc = (suppliedCrc << 4) | static_cast<uint32_t>(nibble);
    }
    uint8_t crcInput[kPairKeySize + 1];
    crcInput[0] = kArtifactSchemaVersion;
    copyBytes(crcInput + 1, parsedKey, kPairKeySize);
    if (crc32IsoHdlc(crcInput, sizeof(crcInput)) != suppliedCrc) {
      return false;
    }
    copyBytes(pairKey, parsedKey, sizeof(parsedKey));
    return true;
  }

  bool
  deriveBootstrapKey(const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider, uint8_t output[kSha256Size]) {
    return prf(pairKey, kBootstrapLabel, sizeof(kBootstrapLabel) - 1, 0, 0, provider, output);
  }

  bool derivePmk(const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider, uint8_t output[kPairKeySize]) {
    if (output == 0) {
      return false;
    }
    uint8_t fullOutput[kSha256Size];
    if (!prf(pairKey, kPmkLabel, sizeof(kPmkLabel) - 1, 0, 0, provider, fullOutput)) {
      return false;
    }
    copyBytes(output, fullOutput, kPairKeySize);
    return true;
  }

  bool deriveLmk(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    uint8_t output[kPairKeySize]
  ) {
    if (output == 0) {
      return false;
    }
    uint8_t context[kMacSize * 2];
    copyBytes(context, panelMac.bytes, kMacSize);
    copyBytes(context + kMacSize, lampMac.bytes, kMacSize);
    uint8_t fullOutput[kSha256Size];
    if (!prf(pairKey, kLmkLabel, sizeof(kLmkLabel) - 1, context, sizeof(context), provider, fullOutput)) {
      return false;
    }
    copyBytes(output, fullOutput, kPairKeySize);
    return true;
  }

  bool deriveConfirmKey(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    const Nonce& panelNonce,
    const Nonce& lampNonce,
    HmacSha256Provider provider,
    uint8_t output[kSha256Size]
  ) {
    uint8_t context[kMacSize * 2 + kNonceSize * 2];
    size_t offset = 0;
    copyBytes(context + offset, panelMac.bytes, kMacSize);
    offset += kMacSize;
    copyBytes(context + offset, lampMac.bytes, kMacSize);
    offset += kMacSize;
    copyBytes(context + offset, panelNonce.bytes, kNonceSize);
    offset += kNonceSize;
    copyBytes(context + offset, lampNonce.bytes, kNonceSize);
    return prf(pairKey, kConfirmLabel, sizeof(kConfirmLabel) - 1, context, sizeof(context), provider, output);
  }

  bool deriveProbeKey(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    uint8_t output[kSha256Size]
  ) {
    uint8_t context[kMacSize * 2];
    copyBytes(context, panelMac.bytes, kMacSize);
    copyBytes(context + kMacSize, lampMac.bytes, kMacSize);
    return prf(pairKey, kProbeLabel, sizeof(kProbeLabel) - 1, context, sizeof(context), provider, output);
  }

  bool deriveCommandKey(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    uint8_t output[kSha256Size]
  ) {
    uint8_t context[kMacSize * 2];
    copyBytes(context, panelMac.bytes, kMacSize);
    copyBytes(context + kMacSize, lampMac.bytes, kMacSize);
    return prf(pairKey, kCommandLabel, sizeof(kCommandLabel) - 1, context, sizeof(context), provider, output);
  }

  bool deriveKeys(
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    const Nonce& panelNonce,
    const Nonce& lampNonce,
    HmacSha256Provider provider,
    DerivedKeys* output
  ) {
    if (output == 0) {
      return false;
    }
    return deriveBootstrapKey(pairKey, provider, output->boot) && derivePmk(pairKey, provider, output->pmk) &&
           deriveLmk(pairKey, panelMac, lampMac, provider, output->lmk) &&
           deriveConfirmKey(pairKey, panelMac, lampMac, panelNonce, lampNonce, provider, output->confirm) &&
           deriveProbeKey(pairKey, panelMac, lampMac, provider, output->probe);
  }

  bool encodeHeader(MessageType type, uint8_t output[kHeaderSize]) {
    if (output == 0 || !isKnownMessageType(type)) {
      return false;
    }
    const size_t payloadLength = payloadLengthFor(type);
    output[0] = kMagic0;
    output[1] = kMagic1;
    output[2] = kProtocolVersion;
    output[3] = static_cast<uint8_t>(type);
    output[4] = static_cast<uint8_t>(payloadLength & 0xff);
    output[5] = static_cast<uint8_t>(payloadLength >> 8);
    output[6] = flagsFor(type);
    output[7] = 0;
    return true;
  }

  bool decodeHeader(const uint8_t* data, size_t length, WireHeader* output) {
    if (
      data == 0 || output == 0 || length < kHeaderSize || data[0] != kMagic0 || data[1] != kMagic1 ||
      data[2] != kProtocolVersion || data[7] != 0
    ) {
      return false;
    }
    const MessageType type = static_cast<MessageType>(data[3]);
    if (!isKnownMessageType(type)) {
      return false;
    }
    const uint16_t payloadLength = static_cast<uint16_t>(data[4]) | (static_cast<uint16_t>(data[5]) << 8);
    if (payloadLength != payloadLengthFor(type) || data[6] != flagsFor(type)) {
      return false;
    }
    output->version = data[2];
    output->type = type;
    output->payloadLength = payloadLength;
    output->flags = data[6];
    output->reserved = data[7];
    return true;
  }

  bool encodeMessage(const Message& message, uint8_t* output, size_t outputCapacity, size_t* outputLength) {
    if (output == 0 || outputLength == 0 || !isPairingMessageType(message.type)) {
      return false;
    }
    const size_t payloadLength = payloadLengthFor(message.type);
    const size_t frameLength = kHeaderSize + payloadLength;
    if (outputCapacity < frameLength || !encodeHeader(message.type, output)) {
      return false;
    }
    size_t offset = kHeaderSize;
    switch (message.type) {
      case MessageType::PairHello:
        copyBytes(output + offset, message.panelMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.firstNonce.bytes, kNonceSize);
        offset += kNonceSize;
        break;
      case MessageType::PairAccept:
      case MessageType::EncryptedConfirm:
      case MessageType::ConfirmAck:
        copyBytes(output + offset, message.panelMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.lampMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.firstNonce.bytes, kNonceSize);
        offset += kNonceSize;
        copyBytes(output + offset, message.secondNonce.bytes, kNonceSize);
        offset += kNonceSize;
        output[offset++] = message.channel;
        break;
      case MessageType::Probe:
        copyBytes(output + offset, message.panelMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.lampMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.firstNonce.bytes, kNonceSize);
        offset += kNonceSize;
        break;
      case MessageType::ProbeAck:
        copyBytes(output + offset, message.panelMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.lampMac.bytes, kMacSize);
        offset += kMacSize;
        copyBytes(output + offset, message.firstNonce.bytes, kNonceSize);
        offset += kNonceSize;
        output[offset++] = message.channel;
        break;
      case MessageType::Command:
      case MessageType::CommandAck: return false;
    }
    copyBytes(output + offset, message.tag.bytes, kTagSize);
    offset += kTagSize;
    *outputLength = offset;
    return offset == frameLength;
  }

  bool decodeMessage(const uint8_t* data, size_t length, Message* output) {
    if (data == 0 || output == 0) {
      return false;
    }
    WireHeader header;
    if (
      !decodeHeader(data, length, &header) || !isPairingMessageType(header.type) ||
      length != kHeaderSize + header.payloadLength
    ) {
      return false;
    }
    Message parsed;
    zeroBytes(reinterpret_cast<uint8_t*>(&parsed), sizeof(parsed));
    parsed.type = header.type;
    size_t offset = kHeaderSize;
    switch (parsed.type) {
      case MessageType::PairHello:
        copyBytes(parsed.panelMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.firstNonce.bytes, data + offset, kNonceSize);
        offset += kNonceSize;
        break;
      case MessageType::PairAccept:
      case MessageType::EncryptedConfirm:
      case MessageType::ConfirmAck:
        copyBytes(parsed.panelMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.lampMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.firstNonce.bytes, data + offset, kNonceSize);
        offset += kNonceSize;
        copyBytes(parsed.secondNonce.bytes, data + offset, kNonceSize);
        offset += kNonceSize;
        parsed.channel = data[offset++];
        break;
      case MessageType::Probe:
        copyBytes(parsed.panelMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.lampMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.firstNonce.bytes, data + offset, kNonceSize);
        offset += kNonceSize;
        break;
      case MessageType::ProbeAck:
        copyBytes(parsed.panelMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.lampMac.bytes, data + offset, kMacSize);
        offset += kMacSize;
        copyBytes(parsed.firstNonce.bytes, data + offset, kNonceSize);
        offset += kNonceSize;
        parsed.channel = data[offset++];
        break;
      case MessageType::Command:
      case MessageType::CommandAck: return false;
    }
    copyBytes(parsed.tag.bytes, data + offset, kTagSize);
    offset += kTagSize;
    if (offset != length) {
      return false;
    }
    *output = parsed;
    return true;
  }

  bool calculateMessageTag(
    const Message& message, const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider, Tag* output
  ) {
    if (pairKey == 0 || provider == 0 || output == 0 || isAllZero(pairKey, kPairKeySize)) {
      return false;
    }
    Message unsignedMessage = message;
    zeroBytes(unsignedMessage.tag.bytes, kTagSize);
    uint8_t frame[kMaxFrameSize];
    size_t frameLength = 0;
    if (!encodeMessage(unsignedMessage, frame, sizeof(frame), &frameLength) || frameLength < kTagSize) {
      return false;
    }
    uint8_t authenticationKey[kSha256Size];
    size_t keyLength = 0;
    if (!deriveForMessage(message, pairKey, provider, authenticationKey, &keyLength)) {
      return false;
    }
    uint8_t digest[kSha256Size];
    if (!provider(authenticationKey, keyLength, frame, frameLength - kTagSize, digest)) {
      return false;
    }
    copyBytes(output->bytes, digest, kTagSize);
    return true;
  }

  bool signMessage(Message* message, const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider) {
    if (message == 0) {
      return false;
    }
    Tag tag;
    if (!calculateMessageTag(*message, pairKey, provider, &tag)) {
      return false;
    }
    message->tag = tag;
    return true;
  }

  bool authenticateMessage(const Message& message, const uint8_t pairKey[kPairKeySize], HmacSha256Provider provider) {
    Tag expected;
    return calculateMessageTag(message, pairKey, provider, &expected) && constantTimeTagEquals(message.tag, expected);
  }

  bool isValidCommand(const Command& command) {
    if (
      isAllZero(command.panelBootNonce.bytes, kNonceSize) || command.sequence == 0 ||
      !isKnownCommandCode(command.code) || !isKnownParameterTarget(command.target)
    ) {
      return false;
    }
    if (command.code == CommandCode::AdjustParameter) {
      return command.target != ParameterTarget::None && command.delta >= -8 && command.delta <= 8 && command.delta != 0;
    }
    if (command.code == CommandCode::SelectParameter) {
      return command.target != ParameterTarget::None && command.delta == 0;
    }
    return command.target == ParameterTarget::None && command.delta == 0;
  }

  bool isValidCommandAck(const CommandAck& ack) {
    return !isAllZero(ack.panelBootNonce.bytes, kNonceSize) && ack.sequence != 0 && isKnownCommandAckStatus(ack.status);
  }

  bool encodeCommand(const Command& command, uint8_t* output, size_t outputCapacity, size_t* outputLength) {
    const size_t frameLength = kHeaderSize + payloadLengthFor(MessageType::Command);
    if (
      output == 0 || outputLength == 0 || outputCapacity < frameLength || !isValidCommand(command) ||
      !encodeHeader(MessageType::Command, output)
    ) {
      return false;
    }
    size_t offset = kHeaderSize;
    copyBytes(output + offset, command.panelBootNonce.bytes, kNonceSize);
    offset += kNonceSize;
    writeUint32Le(output + offset, command.sequence);
    offset += sizeof(command.sequence);
    output[offset++] = static_cast<uint8_t>(command.code);
    output[offset++] = static_cast<uint8_t>(command.target);
    output[offset++] = static_cast<uint8_t>(command.delta);
    copyBytes(output + offset, command.tag.bytes, kTagSize);
    offset += kTagSize;
    *outputLength = offset;
    return offset == frameLength;
  }

  bool decodeCommand(const uint8_t* data, size_t length, Command* output) {
    if (data == 0 || output == 0) {
      return false;
    }
    WireHeader header;
    if (
      !decodeHeader(data, length, &header) || header.type != MessageType::Command ||
      length != kHeaderSize + payloadLengthFor(MessageType::Command)
    ) {
      return false;
    }
    Command parsed = {};
    size_t offset = kHeaderSize;
    copyBytes(parsed.panelBootNonce.bytes, data + offset, kNonceSize);
    offset += kNonceSize;
    parsed.sequence = readUint32Le(data + offset);
    offset += sizeof(parsed.sequence);
    parsed.code = static_cast<CommandCode>(data[offset++]);
    parsed.target = static_cast<ParameterTarget>(data[offset++]);
    parsed.delta = static_cast<int8_t>(data[offset++]);
    copyBytes(parsed.tag.bytes, data + offset, kTagSize);
    offset += kTagSize;
    if (offset != length || !isValidCommand(parsed)) {
      return false;
    }
    *output = parsed;
    return true;
  }

  bool encodeCommandAck(const CommandAck& ack, uint8_t* output, size_t outputCapacity, size_t* outputLength) {
    const size_t frameLength = kHeaderSize + payloadLengthFor(MessageType::CommandAck);
    if (
      output == 0 || outputLength == 0 || outputCapacity < frameLength || !isValidCommandAck(ack) ||
      !encodeHeader(MessageType::CommandAck, output)
    ) {
      return false;
    }
    size_t offset = kHeaderSize;
    copyBytes(output + offset, ack.panelBootNonce.bytes, kNonceSize);
    offset += kNonceSize;
    writeUint32Le(output + offset, ack.sequence);
    offset += sizeof(ack.sequence);
    output[offset++] = static_cast<uint8_t>(ack.status);
    copyBytes(output + offset, ack.tag.bytes, kTagSize);
    offset += kTagSize;
    *outputLength = offset;
    return offset == frameLength;
  }

  bool decodeCommandAck(const uint8_t* data, size_t length, CommandAck* output) {
    if (data == 0 || output == 0) {
      return false;
    }
    WireHeader header;
    if (
      !decodeHeader(data, length, &header) || header.type != MessageType::CommandAck ||
      length != kHeaderSize + payloadLengthFor(MessageType::CommandAck)
    ) {
      return false;
    }
    CommandAck parsed = {};
    size_t offset = kHeaderSize;
    copyBytes(parsed.panelBootNonce.bytes, data + offset, kNonceSize);
    offset += kNonceSize;
    parsed.sequence = readUint32Le(data + offset);
    offset += sizeof(parsed.sequence);
    parsed.status = static_cast<CommandAckStatus>(data[offset++]);
    copyBytes(parsed.tag.bytes, data + offset, kTagSize);
    offset += kTagSize;
    if (offset != length || !isValidCommandAck(parsed)) {
      return false;
    }
    *output = parsed;
    return true;
  }

  bool calculateCommandTag(
    const Command& command,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    Tag* output
  ) {
    if (pairKey == 0 || provider == 0 || output == 0 || isAllZero(pairKey, kPairKeySize)) {
      return false;
    }
    Command unsignedCommand = command;
    zeroBytes(unsignedCommand.tag.bytes, kTagSize);
    uint8_t frame[kMaxFrameSize];
    size_t frameLength = 0;
    if (!encodeCommand(unsignedCommand, frame, sizeof(frame), &frameLength)) {
      return false;
    }
    uint8_t key[kSha256Size];
    if (!deriveCommandKey(pairKey, panelMac, lampMac, provider, key)) {
      return false;
    }
    uint8_t digest[kSha256Size];
    if (!provider(key, sizeof(key), frame, frameLength - kTagSize, digest)) {
      return false;
    }
    copyBytes(output->bytes, digest, kTagSize);
    return true;
  }

  bool signCommand(
    Command* command,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  ) {
    if (command == 0) {
      return false;
    }
    Tag tag;
    if (!calculateCommandTag(*command, pairKey, panelMac, lampMac, provider, &tag)) {
      return false;
    }
    command->tag = tag;
    return true;
  }

  bool authenticateCommand(
    const Command& command,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  ) {
    Tag expected;
    return calculateCommandTag(command, pairKey, panelMac, lampMac, provider, &expected) &&
           constantTimeTagEquals(command.tag, expected);
  }

  bool calculateCommandAckTag(
    const CommandAck& ack,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider,
    Tag* output
  ) {
    if (pairKey == 0 || provider == 0 || output == 0 || isAllZero(pairKey, kPairKeySize)) {
      return false;
    }
    CommandAck unsignedAck = ack;
    zeroBytes(unsignedAck.tag.bytes, kTagSize);
    uint8_t frame[kMaxFrameSize];
    size_t frameLength = 0;
    if (!encodeCommandAck(unsignedAck, frame, sizeof(frame), &frameLength)) {
      return false;
    }
    uint8_t key[kSha256Size];
    if (!deriveCommandKey(pairKey, panelMac, lampMac, provider, key)) {
      return false;
    }
    uint8_t digest[kSha256Size];
    if (!provider(key, sizeof(key), frame, frameLength - kTagSize, digest)) {
      return false;
    }
    copyBytes(output->bytes, digest, kTagSize);
    return true;
  }

  bool signCommandAck(
    CommandAck* ack,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  ) {
    if (ack == 0) {
      return false;
    }
    Tag tag;
    if (!calculateCommandAckTag(*ack, pairKey, panelMac, lampMac, provider, &tag)) {
      return false;
    }
    ack->tag = tag;
    return true;
  }

  bool authenticateCommandAck(
    const CommandAck& ack,
    const uint8_t pairKey[kPairKeySize],
    const Mac& panelMac,
    const Mac& lampMac,
    HmacSha256Provider provider
  ) {
    Tag expected;
    return calculateCommandAckTag(ack, pairKey, panelMac, lampMac, provider, &expected) &&
           constantTimeTagEquals(ack.tag, expected);
  }

  void makeUnpairedBinding(BindingRecord* output) {
    if (output == 0) {
      return;
    }
    output->enabled = false;
    output->protocolVersion = kProtocolVersion;
    zeroBytes(output->panelMac.bytes, kMacSize);
    output->lastChannel = 0;
    output->flags = 0;
    zeroBytes(output->pairKey, kPairKeySize);
  }

  bool encodeBindingRecord(const BindingRecord& record, uint8_t output[kBindingRecordSize]) {
    if (output == 0 || !bindingShapeIsValid(record)) {
      return false;
    }
    output[0] = kBindingMarker;
    output[1] = kBindingRecordVersion;
    output[2] = record.enabled ? 1 : 0;
    output[3] = record.protocolVersion;
    copyBytes(output + 4, record.panelMac.bytes, kMacSize);
    output[10] = record.lastChannel;
    output[11] = record.flags;
    copyBytes(output + 12, record.pairKey, kPairKeySize);
    const uint32_t crc = crc32IsoHdlc(output, 28);
    output[28] = static_cast<uint8_t>(crc & 0xff);
    output[29] = static_cast<uint8_t>((crc >> 8) & 0xff);
    output[30] = static_cast<uint8_t>((crc >> 16) & 0xff);
    output[31] = static_cast<uint8_t>((crc >> 24) & 0xff);
    return true;
  }

  bool decodeBindingRecord(const uint8_t* data, size_t length, BindingRecord* output) {
    if (output == 0) {
      return false;
    }
    makeUnpairedBinding(output);
    if (data == 0 || length != kBindingRecordSize || data[0] != kBindingMarker || data[1] != kBindingRecordVersion) {
      return false;
    }
    const uint32_t suppliedCrc = static_cast<uint32_t>(data[28]) | (static_cast<uint32_t>(data[29]) << 8) |
                                 (static_cast<uint32_t>(data[30]) << 16) | (static_cast<uint32_t>(data[31]) << 24);
    if (crc32IsoHdlc(data, 28) != suppliedCrc || data[2] > 1) {
      return false;
    }
    BindingRecord parsed;
    parsed.enabled = data[2] != 0;
    parsed.protocolVersion = data[3];
    copyBytes(parsed.panelMac.bytes, data + 4, kMacSize);
    parsed.lastChannel = data[10];
    parsed.flags = data[11];
    copyBytes(parsed.pairKey, data + 12, kPairKeySize);
    if (!bindingShapeIsValid(parsed)) {
      return false;
    }
    *output = parsed;
    return true;
  }

} // namespace ControlPadProtocol
