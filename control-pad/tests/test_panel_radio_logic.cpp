#include <assert.h>
#include <string.h>

#include <vector>

#include "panel_radio_binding.h"
#include "panel_radio_runtime.h"

using namespace PanelRadio;

namespace {

  const uint8_t kPanel[kMacSize] = {2, 0, 0, 0, 0, 1};
  const uint8_t kLamp[kMacSize] = {2, 0, 0, 0, 0, 2};

  struct FakeClock : Clock {
    uint32_t now = 0;
    uint32_t nowMs() const override { return now; }
  };

  struct FakeRandom : Random {
    uint8_t next = 1;
    void fill(uint8_t* output, size_t length) override {
      for (size_t index = 0; index < length; ++index)
        output[index] = next++;
    }
  };

  struct FakeStore : BindingStore {
    Binding loaded = {};
    bool readable = true;
    std::vector<bool> writes;
    std::vector<std::vector<uint8_t>> blobs;
    bool load(Binding* output) override {
      if (!readable || !validBinding(loaded)) return false;
      *output = loaded;
      return true;
    }
    bool write(const uint8_t blob[kBindingBlobSize]) override {
      blobs.push_back(std::vector<uint8_t>(blob, blob + kBindingBlobSize));
      const size_t index = blobs.size() - 1;
      return index < writes.size() ? writes[index] : true;
    }
  };

  struct FakeTransport : Transport {
    std::vector<uint8_t> legal;
    std::vector<std::vector<uint8_t>> sent;
    std::vector<RxEnvelope> rx;
    std::vector<uint8_t> helloChannels;
    std::vector<uint8_t> probeChannels;
    std::vector<Binding> probeBindings;
    std::vector<bool> beginResults;
    Binding activated = {};
    uint8_t activeChannel = 0;
    bool peerEncrypted = false;
    uint32_t dropped = 0;
    uint32_t beginCalls = 0;
    uint32_t stationMacCalls = 0;
    uint32_t countryCalls = 0;
    bool begun = false;
    bool stationMacAvailable = true;
    bool countryAvailable = true;
    bool activateAvailable = true;
    bool advanceClockOnActivate = false;
    FakeClock* clockToAdvance = nullptr;
    uint32_t activateCalls = 0;
    std::vector<bool> sendResults;
    uint32_t txOk = 0;
    uint32_t txError = 0;
    bool begin() override {
      ++beginCalls;
      const size_t index = beginCalls - 1;
      begun = index < beginResults.size() ? beginResults[index] : true;
      return begun;
    }
    bool stationMac(uint8_t output[kMacSize]) override {
      ++stationMacCalls;
      if (!begun || !stationMacAvailable) return false;
      memcpy(output, kPanel, kMacSize);
      return true;
    }
    bool activateBoundPeer(const Binding& binding) override {
      ++activateCalls;
      if (!begun || !activateAvailable) return false;
      activated = binding;
      activeChannel = binding.channel;
      peerEncrypted = true;
      if (advanceClockOnActivate && clockToAdvance != nullptr) ++clockToAdvance->now;
      return true;
    }
    bool tuneForHello(uint8_t channel) override {
      if (!begun) return false;
      helloChannels.push_back(channel);
      activeChannel = channel;
      peerEncrypted = false;
      return true;
    }
    bool tuneForProbe(const Binding& binding, uint8_t channel) override {
      if (!begun) return false;
      probeChannels.push_back(channel);
      Binding adjusted = binding;
      adjusted.channel = channel;
      probeBindings.push_back(adjusted);
      activated = adjusted;
      activeChannel = channel;
      peerEncrypted = true;
      return true;
    }
    bool send(const uint8_t* frame, size_t length) override {
      sent.push_back(std::vector<uint8_t>(frame, frame + length));
      const size_t index = sent.size() - 1;
      const bool result = index < sendResults.size() ? sendResults[index] : true;
      if (result) ++txOk;
      else
        ++txError;
      return result;
    }
    bool next(RxEnvelope* envelope) override {
      if (rx.empty()) return false;
      *envelope = rx.front();
      rx.erase(rx.begin());
      return true;
    }
    bool countryChannels(uint8_t* channels, uint8_t* count, uint8_t capacity) override {
      ++countryCalls;
      if (!begun || !countryAvailable) return false;
      if (legal.size() > capacity) return false;
      for (size_t index = 0; index < legal.size(); ++index)
        channels[index] = legal[index];
      *count = static_cast<uint8_t>(legal.size());
      return !legal.empty();
    }
    uint32_t rxDropped() const override { return dropped; }
    uint32_t txSucceeded() const override { return txOk; }
    uint32_t txFailed() const override { return txError; }
  };

  struct FakeProtocol : Protocol {
    std::vector<DecodedPacket> packets;
    std::vector<DecodeResult> decodeResults;
    std::vector<CommandEvent> commands;
    bool makeHello(const uint8_t[], const uint8_t[], uint8_t* frame, size_t* length) override {
      frame[0] = 'H';
      *length = 1;
      return true;
    }
    bool makeConfirm(
      const uint8_t[], const uint8_t[], const uint8_t[], const uint8_t[], uint8_t, uint8_t* frame, size_t* length
    ) override {
      frame[0] = 'F';
      *length = 1;
      return true;
    }
    bool makeProbe(const uint8_t[], const Binding&, const uint8_t[], uint8_t* frame, size_t* length) override {
      frame[0] = 'P';
      *length = 1;
      return true;
    }
    bool makeCommand(
      const uint8_t[],
      const Binding&,
      const uint8_t[],
      uint32_t sequence,
      const CommandEvent& event,
      uint8_t* frame,
      size_t* length
    ) override {
      commands.push_back(event);
      frame[0] = 'C';
      frame[1] = static_cast<uint8_t>(sequence);
      frame[2] = static_cast<uint8_t>(event.delta);
      *length = 3;
      return true;
    }
    DecodeResult decode(const RxEnvelope&, const uint8_t[], const Binding&, DecodedPacket* packet) override {
      if (!decodeResults.empty()) {
        const DecodeResult result = decodeResults.front();
        decodeResults.erase(decodeResults.begin());
        if (result != DecodeResult::Accepted) return result;
      }
      if (packets.empty()) return DecodeResult::Ignored;
      *packet = packets.front();
      packets.erase(packets.begin());
      return DecodeResult::Accepted;
    }
  };

  Binding bound(uint8_t channel = 6) {
    Binding result = {true, {}, channel};
    memcpy(result.lampMac, kLamp, kMacSize);
    return result;
  }

  DecodedPacket packet(PacketKind kind, uint8_t channel = 6, uint32_t sequence = 0) {
    DecodedPacket result = {};
    result.kind = kind;
    memcpy(result.sourceMac, kLamp, kMacSize);
    memcpy(result.panelMac, kPanel, kMacSize);
    memcpy(result.lampMac, kLamp, kMacSize);
    result.channel = channel;
    result.sequence = sequence;
    return result;
  }

  RxEnvelope envelope(uint32_t receivedAt = 0) {
    RxEnvelope result = {};
    memcpy(result.sourceMac, kLamp, kMacSize);
    result.length = 1;
    result.receivedAtMs = receivedAt;
    return result;
  }

  void matchingPairNonce(DecodedPacket* packet) {
    memcpy(packet->firstNonce, "\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\040", kNonceSize);
  }

  void testBinding() {
    const uint8_t expected[kBindingBlobSize] = {0xC2, 1, 1, 1, 2, 0, 0, 0, 0, 2, 6, 0, 0xC8, 0x54, 0x86, 0xE9};
    uint8_t blob[kBindingBlobSize] = {};
    encodeBinding(bound(), blob);
    assert(memcmp(blob, expected, sizeof(blob)) == 0);
    Binding decoded = {};
    assert(decodeBinding(blob, &decoded));
    assert(decoded.channel == 6 && memcmp(decoded.lampMac, kLamp, kMacSize) == 0);
    blob[15] ^= 1;
    assert(!decodeBinding(blob, &decoded));
  }

  void testPairingCombo() {
    PairingCombo combo;
    assert(!combo.tick(0, true, false));
    assert(!combo.tick(100, true, true));
    assert(!combo.tick(2599, true, true));
    assert(combo.tick(2600, true, true));
    assert(combo.locked(2600));
    assert(!combo.tick(2700, false, false));
    assert(!combo.tick(3000, true, true));
    assert(!combo.tick(5500, true, true));
    assert(combo.tick(8000, true, true));
  }

  void testColdStartBackoff() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    transport.beginResults = {false, true};
    transport.legal = {6};
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    assert(transport.beginCalls == 1 && transport.stationMacCalls == 0 && transport.countryCalls == 0);
    runtime.startPairing();
    assert(transport.countryCalls == 0);
    clock.now = 999;
    runtime.tick();
    assert(transport.beginCalls == 1 && transport.stationMacCalls == 0);
    clock.now = 1000;
    runtime.tick();
    assert(transport.beginCalls == 2 && transport.stationMacCalls == 1 && transport.countryCalls == 0);
    runtime.startPairing();
    runtime.tick();
    assert(transport.countryCalls == 1 && transport.helloChannels.size() == 1);
  }

  void testCountryScanBackoff() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    transport.legal = {6};
    transport.countryAvailable = false;
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    runtime.startPairing();
    runtime.tick();
    assert(transport.countryCalls == 1 && transport.helloChannels.empty());
    clock.now = 999;
    runtime.tick();
    assert(transport.countryCalls == 1);
    clock.now = 1000;
    transport.countryAvailable = true;
    runtime.tick();
    assert(transport.countryCalls == 2 && transport.helloChannels.size() == 1);
  }

  void testPairingAndPersistence() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    transport.legal = {1, 6, 11};
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    runtime.startPairing();
    runtime.tick();
    assert(transport.helloChannels.size() == 1 && transport.helloChannels[0] == 1);
    clock.now = 180;
    runtime.tick();
    assert(transport.helloChannels[1] == 6);

    DecodedPacket accept = packet(PacketKind::PairAccept, 6);
    memcpy(accept.firstNonce, "\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\040", kNonceSize);
    memcpy(accept.secondNonce, "abcdefghijklmnop", kNonceSize);
    protocol.packets.push_back(accept);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    assert(runtime.state() == State::Confirm);
    assert(transport.sent.back()[0] == 'F');
    clock.now += 250;
    runtime.tick();
    assert(transport.sent.back()[0] == 'F');

    DecodedPacket ack = packet(PacketKind::ConfirmAck, 6);
    memcpy(ack.firstNonce, accept.firstNonce, kNonceSize);
    memcpy(ack.secondNonce, accept.secondNonce, kNonceSize);
    protocol.packets.push_back(ack);
    transport.rx.push_back(envelope(clock.now));
    store.writes = {false, false, true};
    runtime.tick();
    assert(runtime.state() == State::PersistConfirm);
    clock.now += 100;
    runtime.tick();
    clock.now += 100;
    runtime.tick();
    assert(runtime.state() == State::Normal && store.blobs.size() == 3);

    store.blobs.clear();
    store.writes = {false, false, false};
    runtime.startPairing();
    runtime.tick();
    DecodedPacket secondAccept = packet(PacketKind::PairAccept, 6);
    memcpy(secondAccept.firstNonce, "123456789:;<=>?@", kNonceSize);
    memcpy(secondAccept.secondNonce, "abcdefghijklmnop", kNonceSize);
    protocol.packets.push_back(secondAccept);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    DecodedPacket secondAck = packet(PacketKind::ConfirmAck, 6);
    memcpy(secondAck.firstNonce, secondAccept.firstNonce, kNonceSize);
    memcpy(secondAck.secondNonce, secondAccept.secondNonce, kNonceSize);
    protocol.packets.push_back(secondAck);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    clock.now += 100;
    runtime.tick();
    clock.now += 100;
    runtime.tick();
    assert(runtime.state() == State::Normal && runtime.binding().channel == 6 && store.blobs.size() == 3);
  }

  void testPairingFailuresAndTelemetry() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    transport.legal = {6};
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    runtime.startPairing();
    runtime.tick();

    protocol.decodeResults.push_back(DecodeResult::PairAcceptDecodeFailed);
    transport.rx.push_back(envelope(clock.now));
    protocol.decodeResults.push_back(DecodeResult::PairAcceptAuthFailed);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    assert(runtime.telemetry().pairAcceptDecodeFailed == 1 && runtime.telemetry().pairAcceptAuthFailed == 1);

    DecodedPacket rejected = packet(PacketKind::PairAccept, 1);
    protocol.packets.push_back(rejected);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    assert(runtime.state() == State::PairScan && runtime.telemetry().pairAcceptGuardRejected == 1);

    transport.activateAvailable = false;
    DecodedPacket peerFailure = packet(PacketKind::PairAccept, 6);
    matchingPairNonce(&peerFailure);
    protocol.packets.push_back(peerFailure);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    assert(runtime.state() == State::PairScan && runtime.telemetry().encryptedPeerFailed == 1);

    transport.activateAvailable = true;
    transport.sendResults = {true, false, true};
    DecodedPacket accepted = packet(PacketKind::PairAccept, 6);
    matchingPairNonce(&accepted);
    protocol.packets.push_back(accepted);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    assert(runtime.state() == State::Confirm && runtime.telemetry().pairAcceptDecoded == 2);
    assert(runtime.telemetry().confirmEnqueueAttempts == 1 && runtime.telemetry().confirmEnqueueFailed == 1);
    const size_t sentAfterFailure = transport.sent.size();
    clock.now = 99;
    runtime.tick();
    assert(transport.sent.size() == sentAfterFailure);
    clock.now = 100;
    runtime.tick();
    assert(runtime.telemetry().confirmEnqueueAttempts == 2 && runtime.telemetry().confirmEnqueueFailed == 1);
    assert(runtime.telemetry().txSucceeded == 2 && runtime.telemetry().txFailed == 1);
  }

  void testPairAcceptActivationClockRefreshesConfirmSchedule() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    transport.legal = {6};
    transport.advanceClockOnActivate = true;
    transport.clockToAdvance = &clock;
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    runtime.startPairing();
    runtime.tick();

    DecodedPacket accepted = packet(PacketKind::PairAccept, 6);
    matchingPairNonce(&accepted);
    protocol.packets.push_back(accepted);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();

    assert(runtime.state() == State::Confirm);
    assert(runtime.telemetry().state == State::Confirm && runtime.telemetry().pairAcceptDecoded == 1);
    assert(runtime.telemetry().encryptedPeerFailed == 0 && runtime.telemetry().confirmEnqueueAttempts == 1);
    assert(transport.activateCalls == 1 && transport.helloChannels.size() == 1);
    assert(transport.sent.size() == 2 && transport.sent[1][0] == 'F');
  }

  void testMissingStationMacDoesNotStartPairing() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    transport.legal = {6};
    transport.stationMacAvailable = false;
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    runtime.startPairing();
    clock.now = 1000;
    runtime.tick();
    assert(transport.helloChannels.empty() && runtime.state() == State::Unpaired);
  }

  void testStoredFirstScanAndCampaignTimeout() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    store.loaded = bound(6);
    transport.legal = {1, 6, 11};
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    runtime.startPairing();
    runtime.tick();
    assert(transport.helloChannels[0] == 6);
    clock.now = 180;
    runtime.tick();
    assert(transport.helloChannels[1] == 1);
    clock.now = 360;
    runtime.tick();
    assert(transport.helloChannels[2] == 11);
    clock.now = 540;
    runtime.tick();
    clock.now = 1039;
    runtime.tick();
    assert(transport.helloChannels.size() == 3);
    clock.now = 1040;
    runtime.tick();
    assert(transport.helloChannels[3] == 6);
    clock.now = 30000;
    runtime.tick();
    assert(runtime.state() == State::Normal && runtime.binding().channel == 6);
  }

  void testCommandRecoveryAndAggregation() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    store.loaded = bound();
    transport.legal = {6, 7};
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();
    assert(runtime.enqueue(CommandCode::TogglePower, ParameterTarget::None, 0));
    runtime.tick();
    const std::vector<uint8_t> first = transport.sent.back();
    clock.now = 150;
    runtime.tick();
    assert(transport.sent.back() == first);
    clock.now = 500;
    runtime.tick();
    assert(runtime.state() == State::Recovery);
    runtime.tick();
    assert(transport.probeChannels[0] == 6);
    assert(transport.activeChannel == 6 && transport.peerEncrypted && transport.probeBindings[0].channel == 6);
    assert(!runtime.enqueue(CommandCode::NextEffect, ParameterTarget::None, 0));

    clock.now = 650;
    runtime.tick();
    assert(transport.probeChannels[1] == 7);
    assert(transport.activeChannel == 7 && transport.peerEncrypted && transport.probeBindings[1].channel == 7);

    DecodedPacket probeAck = packet(PacketKind::ProbeAck, 7);
    memcpy(probeAck.firstNonce, "\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\040", kNonceSize);
    protocol.packets.push_back(probeAck);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    Binding persisted = {};
    assert(
      runtime.state() == State::Normal && runtime.binding().channel == 7 && transport.activeChannel == 7 &&
      transport.peerEncrypted && decodeBinding(store.blobs.back().data(), &persisted) && persisted.channel == 7
    );

    for (uint8_t index = 0; index < kCommandQueueCapacity + 1; ++index)
      runtime.enqueue(CommandCode::NextEffect, ParameterTarget::None, 0);
    assert(runtime.telemetry().commandDropped > 0);
  }

  void testEncoderDetentAggregationAndCap() {
    FakeClock clock;
    FakeRandom random;
    FakeTransport transport;
    FakeStore store;
    FakeProtocol protocol;
    store.loaded = bound();
    PanelRadioRuntime runtime(clock, random, transport, store, protocol);
    runtime.begin();

    assert(runtime.selectParameter(ParameterTarget::Speed));
    runtime.onEncoderRawTurn(-1);
    clock.now = 40;
    runtime.tick();
    assert(protocol.commands.back().code == CommandCode::SelectParameter);

    DecodedPacket selectAck = packet(PacketKind::CommandAck, 6, 1);
    memcpy(selectAck.firstNonce, "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020", kNonceSize);
    protocol.packets.push_back(selectAck);
    transport.rx.push_back(envelope(clock.now));
    runtime.tick();
    assert(protocol.commands.back().code == CommandCode::AdjustParameter);
    assert(protocol.commands.back().target == ParameterTarget::Speed && protocol.commands.back().delta == -1);

    DecodedPacket adjustAck = packet(PacketKind::CommandAck, 6, 2);
    memcpy(adjustAck.firstNonce, selectAck.firstNonce, kNonceSize);
    protocol.packets.push_back(adjustAck);
    transport.rx.push_back(envelope(clock.now));
    ++clock.now;
    runtime.tick();

    for (uint8_t index = 0; index < 4; ++index) {
      clock.now += 10;
      runtime.onEncoderRawTurn(1);
    }
    clock.now += 40;
    runtime.tick();
    assert(protocol.commands.back().code == CommandCode::AdjustParameter);
    assert(protocol.commands.back().target == ParameterTarget::Speed && protocol.commands.back().delta == 8);
  }

} // namespace

int main() {
  testBinding();
  testPairingCombo();
  testColdStartBackoff();
  testCountryScanBackoff();
  testPairingAndPersistence();
  testPairingFailuresAndTelemetry();
  testPairAcceptActivationClockRefreshesConfirmSchedule();
  testMissingStationMacDoesNotStartPairing();
  testStoredFirstScanAndCampaignTimeout();
  testCommandRecoveryAndAggregation();
  testEncoderDetentAggregationAndCap();
  return 0;
}
