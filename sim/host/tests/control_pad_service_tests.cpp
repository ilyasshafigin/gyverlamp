#include <EEPROM.h>

#include <openssl/hmac.h>

#include <stdio.h>
#include <string.h>

#define private public
#include "network/control_pad_service.h"
#undef private

#include "control_pad_host_test.h"
#include <WifiController.h>

uint32_t sim_millis = 0;

namespace {

  int failures = 0;

  struct CommandEvents {
    uint16_t count;
    bool accept;
    ControlPadService::CommandEvent last;
  } commandEvents = {};

  void expect(bool condition, const char* name) {
    if (!condition) {
      ++failures;
      printf("FAIL: %s\n", name);
    }
  }

  bool sameBytes(const uint8_t* left, const uint8_t* right, size_t length) {
    return left != nullptr && right != nullptr && memcmp(left, right, length) == 0;
  }

  void copyMac(ControlPadProtocol::Mac* destination, const uint8_t source[6]) {
    memcpy(destination->bytes, source, sizeof(destination->bytes));
  }

  void fillNonce(ControlPadProtocol::Nonce* nonce, uint8_t first) {
    for (uint8_t index = 0; index < sizeof(nonce->bytes); ++index)
      nonce->bytes[index] = static_cast<uint8_t>(first + index);
  }

  ControlPadProtocol::BindingRecord pairedBinding(const uint8_t panelMac[6], const uint8_t key[16]) {
    ControlPadProtocol::BindingRecord binding = {};
    binding.enabled = true;
    binding.protocolVersion = ControlPadProtocol::kProtocolVersion;
    copyMac(&binding.panelMac, panelMac);
    binding.lastChannel = 6;
    binding.flags = ControlPadProtocol::kBindingFlagBound;
    memcpy(binding.pairKey, key, sizeof(binding.pairKey));
    return binding;
  }

  ControlPadService makeService(EepromStore& store, WifiController& wifi) {
    return ControlPadService(store, wifi);
  }

  bool captureCommandEvent(const ControlPadService::CommandEvent& event, void* context) {
    auto* events = static_cast<CommandEvents*>(context);
    ++events->count;
    events->last = event;
    return events->accept;
  }

  bool encodeMessage(const ControlPadProtocol::Message& message, uint8_t output[69], size_t* length) {
    return ControlPadProtocol::encodeMessage(message, output, 69, length);
  }

  void resetFixture() {
    EEPROM.reset();
    control_pad_host_test::reset();
    WifiController::connected = true;
    WifiController::activeChannel = 6;
    sim_millis = 1;
    commandEvents = CommandEvents{};
    commandEvents.accept = true;
  }

  bool
  queuePairAccept(ControlPadService& service, const uint8_t key[16], const uint8_t panelMac[6], uint8_t nonceFirst) {
    char artifact[ControlPadProtocol::kArtifactBufferSize] = {};
    if (!ControlPadProtocol::formatArtifact(key, artifact, sizeof(artifact)) || !service.requestOpenPairing(artifact))
      return false;
    service.tick();
    ControlPadProtocol::Message hello = {};
    hello.type = ControlPadProtocol::MessageType::PairHello;
    copyMac(&hello.panelMac, panelMac);
    fillNonce(&hello.firstNonce, nonceFirst);
    uint8_t frame[69] = {};
    size_t length = 0;
    if (
      !ControlPadProtocol::signMessage(&hello, key, control_pad_host_hmac_sha256) ||
      !encodeMessage(hello, frame, &length)
    )
      return false;
    control_pad_host_test::deliver(panelMac, frame, length);
    ++sim_millis;
    service.tick();
    return service.candidate_.active && service.pairAcceptTxPending_;
  }

  bool makeCandidateConfirm(
    const ControlPadService& service, const uint8_t key[16], uint8_t output[69], size_t* outputLength
  ) {
    ControlPadProtocol::Message confirm = {};
    confirm.type = ControlPadProtocol::MessageType::EncryptedConfirm;
    confirm.panelMac = service.candidate_.panelMac;
    confirm.lampMac = service.lampMac_;
    confirm.firstNonce = service.candidate_.panelNonce;
    confirm.secondNonce = service.candidate_.lampNonce;
    confirm.channel = service.candidate_.channel;
    return ControlPadProtocol::signMessage(&confirm, key, control_pad_host_hmac_sha256) &&
           encodeMessage(confirm, output, outputLength);
  }

  void testPairingTranscriptCommitAndDuplicate() {
    constexpr uint8_t panelMac[6] = {0x24, 0x6f, 0x28, 0x10, 0x20, 0x30};
    uint8_t key[16] = {};
    for (uint8_t index = 0; index < sizeof(key); ++index)
      key[index] = static_cast<uint8_t>(0xa0 + index);

    resetFixture();
    EepromStore store;
    WifiController wifi;
    expect(store.init(), "pairing fixture initializes EEPROM");
    ControlPadService service = makeService(store, wifi);
    service.init();
    char artifact[ControlPadProtocol::kArtifactBufferSize] = {};
    expect(ControlPadProtocol::formatArtifact(key, artifact, sizeof(artifact)), "pairing artifact formats");
    expect(service.requestOpenPairing(artifact), "pairing request is accepted");
    service.tick();

    ControlPadProtocol::Message hello = {};
    hello.type = ControlPadProtocol::MessageType::PairHello;
    copyMac(&hello.panelMac, panelMac);
    fillNonce(&hello.firstNonce, 0x10);
    expect(ControlPadProtocol::signMessage(&hello, key, control_pad_host_hmac_sha256), "hello signs");
    uint8_t helloFrame[69] = {};
    size_t helloLength = 0;
    expect(encodeMessage(hello, helloFrame, &helloLength), "hello encodes");
    control_pad_host_test::deliver(panelMac, helloFrame, helloLength);
    ++sim_millis;
    service.tick();

    constexpr uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    expect(
      control_pad_host_test::peerAdds.size() == 1 &&
        sameBytes(control_pad_host_test::peerAdds[0].mac, broadcast, sizeof(broadcast)) &&
        !control_pad_host_test::peerAdds[0].encrypted && control_pad_host_test::peerRemovals.empty() &&
        !service.candidate_.encryptedPeerInstalled,
      "broadcast peer stays installed while PairAccept transmission is pending"
    );
    expect(
      control_pad_host_test::sends.size() == 1 &&
        sameBytes(control_pad_host_test::sends[0].destination, broadcast, sizeof(broadcast)),
      "PairAccept is sent to the broadcast destination"
    );
    ControlPadProtocol::Message accept = {};
    expect(
      ControlPadProtocol::decodeMessage(
        control_pad_host_test::sends[0].data.data(), control_pad_host_test::sends[0].data.size(), &accept
      ) &&
        accept.type == ControlPadProtocol::MessageType::PairAccept &&
        ControlPadProtocol::authenticateMessage(accept, key, control_pad_host_hmac_sha256),
      "PairAccept preserves authenticated pairing transcript"
    );

    control_pad_host_test::completeNextSend(true);
    ++sim_millis;
    service.tick();
    expect(
      control_pad_host_test::peerAdds.size() == 2 && control_pad_host_test::peerRemovals.size() == 1 &&
        sameBytes(control_pad_host_test::peerRemovals[0].mac, broadcast, sizeof(broadcast)) &&
        sameBytes(control_pad_host_test::peerAdds[1].mac, panelMac, sizeof(panelMac)) &&
        control_pad_host_test::peerAdds[1].encrypted && service.candidate_.encryptedPeerInstalled,
      "successful PairAccept transmission replaces broadcast peer with encrypted candidate peer"
    );

    ControlPadProtocol::Message confirm = {};
    confirm.type = ControlPadProtocol::MessageType::EncryptedConfirm;
    confirm.panelMac = service.candidate_.panelMac;
    confirm.lampMac = service.lampMac_;
    confirm.firstNonce = service.candidate_.panelNonce;
    confirm.secondNonce = service.candidate_.lampNonce;
    confirm.channel = service.candidate_.channel;
    expect(ControlPadProtocol::signMessage(&confirm, key, control_pad_host_hmac_sha256), "confirm signs");
    uint8_t confirmFrame[69] = {};
    size_t confirmLength = 0;
    expect(encodeMessage(confirm, confirmFrame, &confirmLength), "confirm encodes");
    const uint32_t commitsBeforeConfirm = EEPROM.commitCount();
    control_pad_host_test::deliver(panelMac, confirmFrame, confirmLength);
    ++sim_millis;
    service.tick();
    expect(
      store.readControlPadBinding().enabled && EEPROM.commitCount() == commitsBeforeConfirm + 1 &&
        control_pad_host_test::sends.size() == 2 &&
        control_pad_host_test::sends.back().commitCountAtSend == commitsBeforeConfirm + 1,
      "binding commits before ConfirmAck is emitted"
    );
    const uint32_t commitsAfterConfirm = EEPROM.commitCount();
    control_pad_host_test::deliver(panelMac, confirmFrame, confirmLength);
    ++sim_millis;
    service.tick();
    expect(
      control_pad_host_test::sends.size() == 3 && EEPROM.commitCount() == commitsAfterConfirm,
      "exact duplicate Confirm receives cached Ack without another commit"
    );
    expect(
      service.status().pairingPhase == ControlPadService::PairingPhase::ConfirmAccepted &&
        service.status().rootFailure == ControlPadService::Failure::None &&
        service.status().terminalFailure == ControlPadService::Failure::None &&
        service.status().pairingCounters.confirmAccepted == 1,
      "successful pairing exposes a completed redacted diagnostic state"
    );
    expect(service.requestOpenPairing(artifact), "reopen request is accepted after successful pairing");
    ++sim_millis;
    service.tick();
    expect(
      service.status().pairingPhase == ControlPadService::PairingPhase::Idle &&
        service.status().rootFailure == ControlPadService::Failure::None &&
        service.status().pairingCounters.confirmAccepted == 0,
      "reopening pairing resets session diagnostics"
    );
    service.requestClear();
    ++sim_millis;
    service.tick();
    expect(
      !store.readControlPadBinding().enabled &&
        service.status().pairingPhase == ControlPadService::PairingPhase::Idle &&
        service.status().rootFailure == ControlPadService::Failure::None,
      "clearing pairing resets binding and diagnostics"
    );
  }

  void testCommandExpiryDedupAndStaLifecycle() {
    constexpr uint8_t panelMac[6] = {0x24, 0x6f, 0x28, 0x11, 0x22, 0x33};
    uint8_t key[16] = {};
    for (uint8_t index = 0; index < sizeof(key); ++index)
      key[index] = static_cast<uint8_t>(index + 1);

    resetFixture();
    EepromStore store;
    WifiController wifi;
    expect(store.init() && store.writeControlPadBinding(pairedBinding(panelMac, key)), "bound fixture persists");
    ControlPadService service = makeService(store, wifi);
    service.init();
    service.retryIndex_ = 3;
    control_pad_host_test::failPeerAdds = 1;
    service.tick();
    expect(
      !service.radioOnline_ && service.retryIndex_ == 4 && service.retryAtMs_ == sim_millis + 8000,
      "bound-peer initialization failure keeps accumulated retry backoff"
    );
    sim_millis += 8000;
    service.tick();
    expect(
      service.radioOnline_ && service.peerInstalled_ && service.retryIndex_ == 0 && service.retryAtMs_ == 0,
      "retry backoff resets only after complete bound-peer initialization"
    );

    ControlPadProtocol::Command command = {};
    fillNonce(&command.panelBootNonce, 0x50);
    command.sequence = 2;
    command.code = ControlPadProtocol::CommandCode::AdjustParameter;
    command.target = ControlPadProtocol::ParameterTarget::Brightness;
    command.delta = 1;
    expect(
      ControlPadProtocol::signCommand(
        &command, key, service.binding_.panelMac, service.lampMac_, control_pad_host_hmac_sha256
      ),
      "expired command signs"
    );
    uint8_t commandFrame[47] = {};
    size_t commandLength = 0;
    expect(
      ControlPadProtocol::encodeCommand(command, commandFrame, sizeof(commandFrame), &commandLength), "command encodes"
    );
    control_pad_host_test::deliver(panelMac, commandFrame, commandLength);
    sim_millis += 501;
    service.tick();
    const size_t sendsAfterExpiry = control_pad_host_test::sends.size();
    ControlPadProtocol::CommandAck expiredAck = {};
    expect(
      ControlPadProtocol::decodeCommandAck(
        control_pad_host_test::sends.back().data.data(), control_pad_host_test::sends.back().data.size(), &expiredAck
      ) &&
        expiredAck.status == ControlPadProtocol::CommandAckStatus::Expired,
      "expired command consumes its sequence and receives Expired Ack"
    );
    control_pad_host_test::deliver(panelMac, commandFrame, commandLength);
    ++sim_millis;
    service.tick();
    expect(
      control_pad_host_test::sends.size() == sendsAfterExpiry + 1 &&
        control_pad_host_test::sends.back().data == control_pad_host_test::sends[sendsAfterExpiry - 1].data,
      "exact expired command duplicate is re-ACKed without reapplication"
    );

    WifiController::connected = false;
    ++sim_millis;
    service.tick();
    expect(!service.radioOnline_ && control_pad_host_test::deinitCalls != 0, "STA loss tears down radio and peer");
    WifiController::connected = true;
    WifiController::activeChannel = 11;
    ++sim_millis;
    service.tick();
    expect(
      service.radioOnline_ && service.activeChannel_ == 11 && service.peerInstalled_ &&
        control_pad_host_test::peerAdds.back().encrypted,
      "STA reconnect on changed channel reinitializes encrypted bound peer"
    );
  }

  void testPairAcceptFailureCommitAndChannelSafety() {
    constexpr uint8_t panelMac[6] = {0x24, 0x6f, 0x28, 0x44, 0x55, 0x66};
    uint8_t key[16] = {};
    for (uint8_t index = 0; index < sizeof(key); ++index)
      key[index] = static_cast<uint8_t>(0x40 + index);

    resetFixture();
    EepromStore failedSendStore;
    WifiController failedSendWifi;
    expect(failedSendStore.init(), "failed-send fixture initializes EEPROM");
    ControlPadService failedSend = makeService(failedSendStore, failedSendWifi);
    failedSend.init();
    expect(queuePairAccept(failedSend, key, panelMac, 0x30), "failed-send fixture queues PairAccept");
    control_pad_host_test::completeNextSend(false);
    ++sim_millis;
    failedSend.tick();
    expect(
      !failedSend.candidate_.encryptedPeerInstalled && !failedSendStore.readControlPadBinding().enabled &&
        failedSend.status().rootFailure == ControlPadService::Failure::PairAcceptTxFailed &&
        failedSend.status().terminalFailure == ControlPadService::Failure::PairAcceptTxFailed,
      "failed PairAccept transmission does not install candidate peer or commit binding"
    );
    sim_millis += 9999;
    failedSend.tick();
    expect(
      failedSend.status().rootFailure == ControlPadService::Failure::PairAcceptTxFailed &&
        failedSend.status().terminalFailure == ControlPadService::Failure::CandidateTimeout &&
        failedSend.status().pairingPhase == ControlPadService::PairingPhase::PairAcceptQueued,
      "Candidate timeout retains an earlier PairAccept transmit failure as root cause"
    );

    resetFixture();
    EepromStore timeoutStore;
    WifiController timeoutWifi;
    expect(timeoutStore.init(), "PairAccept timeout fixture initializes EEPROM");
    ControlPadService timeout = makeService(timeoutStore, timeoutWifi);
    timeout.init();
    expect(queuePairAccept(timeout, key, panelMac, 0x40), "PairAccept timeout fixture queues transmission");
    sim_millis += 10000;
    timeout.tick();
    expect(
      timeout.status().rootFailure == ControlPadService::Failure::PairAcceptTxTimeout &&
        timeout.status().terminalFailure == ControlPadService::Failure::CandidateTimeout &&
        timeout.status().pairingCounters.pairAcceptTxTimedOut == 1 &&
        timeout.status().pairingCounters.pairAcceptSendCallbacks == 0 &&
        timeout.status().pairingCounters.pairAcceptTxProcessed == 0,
      "one late tick retains missing PairAccept callback as the root timeout cause"
    );

    resetFixture();
    EepromStore noConfirmStore;
    WifiController noConfirmWifi;
    expect(noConfirmStore.init(), "no-Confirm fixture initializes EEPROM");
    ControlPadService noConfirm = makeService(noConfirmStore, noConfirmWifi);
    noConfirm.init();
    expect(queuePairAccept(noConfirm, key, panelMac, 0x45), "no-Confirm fixture queues PairAccept");
    control_pad_host_test::completeNextSend(true);
    const ControlPadService::Status latchedStatus = noConfirm.status();
    expect(
      latchedStatus.pairAcceptTxPending && latchedStatus.pairAcceptTxCompletionLatched &&
        latchedStatus.pairingCounters.pairAcceptSendCallbacks == 1 &&
        latchedStatus.pairingCounters.pairAcceptBroadcastMacMatches == 1 &&
        latchedStatus.pairingCounters.pairAcceptTxLatched == 1 &&
        latchedStatus.pairingCounters.pairAcceptTxProcessed == 0,
      "send callback exposes a redacted latched completion before tick processing"
    );
    sim_millis += 10000;
    noConfirm.tick();
    const ControlPadService::Status noConfirmStatus = noConfirm.status();
    expect(
      noConfirmStatus.pairingPhase == ControlPadService::PairingPhase::CandidatePeerInstalled &&
        noConfirmStatus.rootFailure == ControlPadService::Failure::CandidateTimeout &&
        noConfirmStatus.terminalFailure == ControlPadService::Failure::CandidateTimeout &&
        noConfirmStatus.pairingCounters.pairAcceptSendCallbacks == 1 &&
        noConfirmStatus.pairingCounters.pairAcceptBroadcastMacMatches == 1 &&
        noConfirmStatus.pairingCounters.pairAcceptTxLatched == 1 &&
        noConfirmStatus.pairingCounters.pairAcceptTxProcessed == 1 &&
        noConfirmStatus.pairingCounters.pairAcceptTxSucceeded == 1 &&
        noConfirmStatus.pairingCounters.candidatePeerInstalled == 1 && control_pad_host_test::peerAdds.size() == 2 &&
        control_pad_host_test::peerRemovals.size() == 2,
      "late TX-success is processed before timeout and completes the broadcast-to-candidate peer lifecycle"
    );

    resetFixture();
    EepromStore failedCommitStore;
    WifiController failedCommitWifi;
    expect(failedCommitStore.init(), "failed-commit fixture initializes EEPROM");
    ControlPadService failedCommit = makeService(failedCommitStore, failedCommitWifi);
    failedCommit.init();
    expect(queuePairAccept(failedCommit, key, panelMac, 0x50), "failed-commit fixture queues PairAccept");
    control_pad_host_test::completeNextSend(true);
    ++sim_millis;
    failedCommit.tick();
    uint8_t confirmFrame[69] = {};
    size_t confirmLength = 0;
    expect(makeCandidateConfirm(failedCommit, key, confirmFrame, &confirmLength), "failed-commit confirm encodes");
    const size_t sendsBeforeCommit = control_pad_host_test::sends.size();
    EEPROM.failNextCommit();
    control_pad_host_test::deliver(panelMac, confirmFrame, confirmLength);
    ++sim_millis;
    failedCommit.tick();
    expect(
      !failedCommitStore.readControlPadBinding().enabled && !failedCommit.candidate_.active &&
        control_pad_host_test::sends.size() == sendsBeforeCommit &&
        failedCommit.status().rootFailure == ControlPadService::Failure::BindingCommitFailed &&
        failedCommit.status().terminalFailure == ControlPadService::Failure::BindingCommitFailed,
      "failed binding commit emits no ConfirmAck and restores an uncommitted pairing state"
    );

    resetFixture();
    EepromStore channelStore;
    WifiController channelWifi;
    expect(channelStore.init(), "channel-change fixture initializes EEPROM");
    ControlPadService channelChange = makeService(channelStore, channelWifi);
    channelChange.init();
    expect(queuePairAccept(channelChange, key, panelMac, 0x60), "channel-change fixture queues PairAccept");
    control_pad_host_test::completeNextSend(true);
    ++sim_millis;
    channelChange.tick();
    expect(
      channelChange.candidate_.encryptedPeerInstalled, "channel-change fixture installs candidate peer after TX success"
    );
    uint8_t wrongChannelConfirmFrame[69] = {};
    size_t wrongChannelConfirmLength = 0;
    expect(
      makeCandidateConfirm(channelChange, key, wrongChannelConfirmFrame, &wrongChannelConfirmLength),
      "wrong-channel confirm encodes"
    );
    ControlPadProtocol::Message wrongChannelConfirm = {};
    expect(
      ControlPadProtocol::decodeMessage(wrongChannelConfirmFrame, wrongChannelConfirmLength, &wrongChannelConfirm),
      "wrong-channel confirm decodes"
    );
    wrongChannelConfirm.channel = 7;
    expect(
      ControlPadProtocol::signMessage(&wrongChannelConfirm, key, control_pad_host_hmac_sha256) &&
        encodeMessage(wrongChannelConfirm, wrongChannelConfirmFrame, &wrongChannelConfirmLength),
      "wrong-channel confirm re-signs"
    );
    control_pad_host_test::deliver(panelMac, wrongChannelConfirmFrame, wrongChannelConfirmLength);
    ++sim_millis;
    channelChange.tick();
    expect(
      channelChange.candidate_.active && !channelStore.readControlPadBinding().enabled &&
        channelChange.status().rootFailure == ControlPadService::Failure::ConfirmRejected &&
        channelChange.status().terminalFailure == ControlPadService::Failure::ConfirmRejected,
      "Confirm channel must match the pinned candidate channel"
    );
    sim_millis += 9999;
    channelChange.tick();
    expect(
      !channelChange.candidate_.active && !channelStore.readControlPadBinding().enabled &&
        channelChange.status().rootFailure == ControlPadService::Failure::ConfirmRejected &&
        channelChange.status().terminalFailure == ControlPadService::Failure::CandidateTimeout &&
        channelChange.status().pairingPhase == ControlPadService::PairingPhase::ConfirmReceived &&
        channelChange.status().pairingCounters.confirmRejected == 1,
      "Candidate timeout retains a rejected Confirm as root cause"
    );
  }

  bool dispatchCommand(
    ControlPadService& service,
    const uint8_t panelMac[6],
    const uint8_t key[16],
    const ControlPadProtocol::Command& input,
    uint32_t delayBeforeTickMs,
    ControlPadProtocol::CommandAckStatus expectedStatus
  ) {
    ControlPadProtocol::Command command = input;
    uint8_t frame[47] = {};
    size_t length = 0;
    if (
      !ControlPadProtocol::signCommand(
        &command, key, service.binding_.panelMac, service.lampMac_, control_pad_host_hmac_sha256
      ) ||
      !ControlPadProtocol::encodeCommand(command, frame, sizeof(frame), &length)
    )
      return false;
    const size_t sendsBefore = control_pad_host_test::sends.size();
    control_pad_host_test::deliver(panelMac, frame, length);
    sim_millis += delayBeforeTickMs;
    service.tick();
    ControlPadProtocol::CommandAck ack = {};
    return control_pad_host_test::sends.size() == sendsBefore + 1 &&
           ControlPadProtocol::decodeCommandAck(
             control_pad_host_test::sends.back().data.data(), control_pad_host_test::sends.back().data.size(), &ack
           ) &&
           ack.status == expectedStatus;
  }

  void testControlPadCommandEventDispatch() {
    constexpr uint8_t panelMac[6] = {0x24, 0x6f, 0x28, 0x70, 0x80, 0x90};
    uint8_t key[16] = {};
    for (uint8_t index = 0; index < sizeof(key); ++index)
      key[index] = static_cast<uint8_t>(0x70 + index);

    resetFixture();
    EepromStore store;
    WifiController wifi;
    expect(
      store.init() && store.writeControlPadBinding(pairedBinding(panelMac, key)),
      "command event fixture initializes bound EEPROM"
    );
    ControlPadService service = makeService(store, wifi);
    service.setCommandHandler(captureCommandEvent, &commandEvents);
    service.init();
    service.tick();
    ControlPadProtocol::Command command = {};
    fillNonce(&command.panelBootNonce, 0x70);
    command.sequence = 2;
    command.code = ControlPadProtocol::CommandCode::AdjustParameter;
    command.target = ControlPadProtocol::ParameterTarget::Speed;
    command.delta = -2;
    expect(
      dispatchCommand(service, panelMac, key, command, 0, ControlPadProtocol::CommandAckStatus::Applied),
      "fresh authenticated command receives Applied acknowledgement"
    );
    expect(
      commandEvents.count == 1 && commandEvents.last.type == ControlPadService::CommandType::AdjustParameter &&
        commandEvents.last.parameter == ControlPadService::ParameterTarget::Speed && commandEvents.last.steps == -2,
      "fresh command emits one translated event with its parameter target and steps"
    );
    expect(
      dispatchCommand(service, panelMac, key, command, 0, ControlPadProtocol::CommandAckStatus::Applied) &&
        commandEvents.count == 1,
      "exact duplicate reuses Applied acknowledgement without another event"
    );

    command.sequence = 1;
    expect(
      dispatchCommand(service, panelMac, key, command, 0, ControlPadProtocol::CommandAckStatus::Stale) &&
        commandEvents.count == 1,
      "stale command emits no event and receives Stale acknowledgement"
    );

    command.sequence = 3;
    expect(
      dispatchCommand(service, panelMac, key, command, 501, ControlPadProtocol::CommandAckStatus::Expired) &&
        commandEvents.count == 1,
      "expired command emits no event and receives Expired acknowledgement"
    );

    command.sequence = 4;
    commandEvents.accept = false;
    expect(
      dispatchCommand(service, panelMac, key, command, 0, ControlPadProtocol::CommandAckStatus::Expired) &&
        commandEvents.count == 2,
      "handler rejection emits one event but never receives Applied acknowledgement"
    );

    command.sequence = 5;
    commandEvents.accept = true;
    service.setCommandHandler(nullptr, nullptr);
    expect(
      dispatchCommand(service, panelMac, key, command, 0, ControlPadProtocol::CommandAckStatus::Expired) &&
        commandEvents.count == 2,
      "missing handler never receives Applied acknowledgement or emits an event"
    );
    service.setCommandHandler(captureCommandEvent, &commandEvents);

    const size_t sendsBeforeInvalid = control_pad_host_test::sends.size();
    const uint8_t malformed[] = {0};
    control_pad_host_test::deliver(panelMac, malformed, sizeof(malformed));
    ++sim_millis;
    service.tick();
    expect(
      commandEvents.count == 2 && control_pad_host_test::sends.size() == sendsBeforeInvalid,
      "malformed command emits no event or acknowledgement"
    );

    command.sequence = 6;
    uint8_t unauthenticated[47] = {};
    size_t unauthenticatedLength = 0;
    expect(
      ControlPadProtocol::signCommand(
        &command, key, service.binding_.panelMac, service.lampMac_, control_pad_host_hmac_sha256
      ) &&
        ControlPadProtocol::encodeCommand(command, unauthenticated, sizeof(unauthenticated), &unauthenticatedLength),
      "unauthenticated command fixture encodes"
    );
    unauthenticated[unauthenticatedLength - 1] ^= 0xff;
    control_pad_host_test::deliver(panelMac, unauthenticated, unauthenticatedLength);
    ++sim_millis;
    service.tick();
    expect(commandEvents.count == 2, "unauthenticated command emits no event");
  }

} // namespace

struct ControlPadHostQueue {
  size_t itemSize;
  uint8_t capacity;
  std::vector<std::vector<uint8_t>> items;
};

namespace control_pad_host_test {

  std::vector<PeerEvent> peerAdds;
  std::vector<PeerEvent> peerRemovals;
  std::vector<SendEvent> sends;
  uint32_t initCalls = 0;
  uint32_t deinitCalls = 0;
  uint8_t failPeerAdds = 0;
  esp_now_recv_cb_t callback = nullptr;
  esp_now_send_cb_t sendCallback = nullptr;
  size_t nextSendToComplete = 0;

  void reset() {
    peerAdds.clear();
    peerRemovals.clear();
    sends.clear();
    initCalls = 0;
    deinitCalls = 0;
    failPeerAdds = 0;
    callback = nullptr;
    sendCallback = nullptr;
    nextSendToComplete = 0;
  }

  void deliver(const uint8_t sourceMac[6], const uint8_t* data, size_t length) {
    if (callback != nullptr) callback(sourceMac, data, static_cast<int>(length));
  }

  void completeNextSend(bool success) {
    if (sendCallback == nullptr || nextSendToComplete >= sends.size()) return;
    sendCallback(sends[nextSendToComplete].destination, success ? ESP_NOW_SEND_SUCCESS : ESP_NOW_SEND_FAIL);
    ++nextSendToComplete;
  }

} // namespace control_pad_host_test

QueueHandle_t xQueueCreate(uint8_t length, size_t itemSize) {
  return new ControlPadHostQueue{itemSize, length, {}};
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, uint32_t) {
  if (queue == nullptr || item == nullptr || queue->items.size() >= queue->capacity) return pdFALSE;
  const auto* bytes = static_cast<const uint8_t*>(item);
  queue->items.emplace_back(bytes, bytes + queue->itemSize);
  return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* item, uint32_t) {
  if (queue == nullptr || item == nullptr || queue->items.empty()) return pdFALSE;
  memcpy(item, queue->items.front().data(), queue->itemSize);
  queue->items.erase(queue->items.begin());
  return pdTRUE;
}

void xQueueReset(QueueHandle_t queue) {
  if (queue != nullptr) queue->items.clear();
}

esp_err_t esp_now_init() {
  ++control_pad_host_test::initCalls;
  return ESP_OK;
}

esp_err_t esp_now_deinit() {
  ++control_pad_host_test::deinitCalls;
  return ESP_OK;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback) {
  control_pad_host_test::callback = callback;
  return ESP_OK;
}

esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback) {
  control_pad_host_test::sendCallback = callback;
  return ESP_OK;
}

esp_err_t esp_now_set_pmk(const uint8_t[16]) {
  return ESP_OK;
}

esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) {
  if (peer == nullptr) return 1;
  if (control_pad_host_test::failPeerAdds != 0) {
    --control_pad_host_test::failPeerAdds;
    return 1;
  }
  control_pad_host_test::PeerEvent event = {};
  memcpy(event.mac, peer->peer_addr, sizeof(event.mac));
  event.encrypted = peer->encrypt;
  control_pad_host_test::peerAdds.push_back(event);
  return ESP_OK;
}

esp_err_t esp_now_del_peer(const uint8_t peerMac[6]) {
  control_pad_host_test::PeerEvent event = {};
  memcpy(event.mac, peerMac, sizeof(event.mac));
  control_pad_host_test::peerRemovals.push_back(event);
  return ESP_OK;
}

esp_err_t esp_now_send(const uint8_t destination[6], const uint8_t* data, size_t length) {
  if (destination == nullptr || data == nullptr) return 1;
  control_pad_host_test::SendEvent event = {};
  memcpy(event.destination, destination, sizeof(event.destination));
  event.data.assign(data, data + length);
  event.commitCountAtSend = EEPROM.commitCount();
  control_pad_host_test::sends.push_back(event);
  return ESP_OK;
}

esp_err_t esp_wifi_get_mac(int, uint8_t mac[6]) {
  constexpr uint8_t lampMac[6] = {0x24, 0x6f, 0x28, 0xaa, 0xbb, 0xcc};
  memcpy(mac, lampMac, sizeof(lampMac));
  return ESP_OK;
}

void esp_fill_random(void* output, size_t length) {
  auto* bytes = static_cast<uint8_t*>(output);
  for (size_t index = 0; index < length; ++index)
    bytes[index] = static_cast<uint8_t>(0xc0 + index);
}

bool control_pad_host_hmac_sha256(
  const uint8_t* key, size_t keyLength, const uint8_t* data, size_t dataLength, uint8_t output[32]
) {
  unsigned int outputLength = 0;
  return key != nullptr && data != nullptr && output != nullptr &&
         HMAC(EVP_sha256(), key, static_cast<int>(keyLength), data, dataLength, output, &outputLength) != nullptr &&
         outputLength == 32;
}

int main() {
  testPairingTranscriptCommitAndDuplicate();
  testCommandExpiryDedupAndStaLifecycle();
  testPairAcceptFailureCommitAndChannelSafety();
  testControlPadCommandEventDispatch();
  if (failures != 0) {
    printf("FAILED: %d control-pad service assertion(s)\n", failures);
    return 1;
  }
  printf("PASS: control-pad fake transport pairing, command, and STA lifecycle fixtures\n");
  return 0;
}
