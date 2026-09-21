#include "panel_radio_runtime.h"

#include <string.h>

#ifdef DEBUG
#include <Arduino.h>
#endif

namespace PanelRadio {
  namespace {

    constexpr uint32_t kPairCampaignMs = 30000;
    constexpr uint32_t kPairDwellMs = 180;
    constexpr uint32_t kRecoveryDwellMs = 150;
    constexpr uint32_t kCommandAckMs = 150;
    constexpr uint32_t kCommandExpiryMs = 500;
    constexpr uint32_t kConfirmOffsets[] = {0, 250, 750, 1750};
    constexpr uint32_t kConfirmEnqueueRetryMs = 100;
    constexpr uint32_t kPairBackoffMs[] = {500, 1000, 2000, 4000};
    constexpr uint32_t kRecoveryBackoffMs[] = {1000, 2000, 4000, 8000, 16000, 30000};
    constexpr uint32_t kRadioBackoffMs[] = {1000, 2000, 4000, 8000, 16000, 30000};

    bool due(uint32_t now, uint32_t deadline) {
      return static_cast<int32_t>(now - deadline) >= 0;
    }

  } // namespace

  PanelRadioRuntime::PanelRadioRuntime(
    Clock& clock, Random& random, Transport& transport, BindingStore& store, Protocol& protocol
  )
    : clock_(clock),
      random_(random),
      transport_(transport),
      store_(store),
      protocol_(protocol),
      panelMac_{},
      bindingLoaded_(false),
      initialized_(false),
      binding_{},
      oldBinding_{},
      persistBinding_{},
      persistSuccessState_(State::Unpaired),
      resetCommandSession_(false),
      state_(State::Unpaired),
      campaignStartedMs_(0),
      confirmStartedMs_(0),
      nextConfirmAttemptMs_(0),
      confirmAttempt_(0),
      scanCount_(0),
      scanIndex_(0),
      nextScanMs_(0),
      currentChannel_(0),
      scanBackoffMs_(0),
      nextRadioAttemptMs_(0),
      radioBackoffIndex_(0),
      persistAttempts_(0),
      nextPersistMs_(0),
      eventHead_(0),
      eventSize_(0),
      inFlight_{},
      nextSequence_(1),
      selectedTarget_(ParameterTarget::Brightness),
      pendingDelta_(0),
      lastDirection_(0),
      lastDetentMs_(0),
      lastFlushMs_(0),
      commandDropped_(0),
      pairAcceptDecoded_(0),
      pairAcceptDecodeFailed_(0),
      pairAcceptAuthFailed_(0),
      pairAcceptGuardRejected_(0),
      encryptedPeerFailed_(0),
      confirmEncodeFailed_(0),
      confirmEnqueueAttempts_(0),
      confirmEnqueueFailed_(0) {
    memset(panelMac_, 0, sizeof(panelMac_));
    memset(panelNonce_, 0, sizeof(panelNonce_));
    memset(lampNonce_, 0, sizeof(lampNonce_));
    memset(probeNonce_, 0, sizeof(probeNonce_));
  }

  void PanelRadioRuntime::begin() {
    if (!bindingLoaded_) {
      Binding loaded = {};
      if (store_.load(&loaded) && validBinding(loaded)) binding_ = loaded;
      state_ = binding_.bound ? State::Normal : State::Unpaired;
      bindingLoaded_ = true;
    }
    ensureTransport(clock_.nowMs());
  }

  bool PanelRadioRuntime::queueEvent(const CommandEvent& event) {
    if (eventSize_ == kCommandQueueCapacity) {
      ++commandDropped_;
      return false;
    }
    events_[(eventHead_ + eventSize_) % kCommandQueueCapacity] = event;
    ++eventSize_;
    return true;
  }

  bool PanelRadioRuntime::popEvent(CommandEvent* event) {
    if (event == nullptr || eventSize_ == 0) return false;
    *event = events_[eventHead_];
    eventHead_ = (eventHead_ + 1) % kCommandQueueCapacity;
    --eventSize_;
    return true;
  }

  void PanelRadioRuntime::clearCommands() {
    eventHead_ = 0;
    eventSize_ = 0;
    inFlight_.active = false;
  }

  void PanelRadioRuntime::clearAggregation() {
    pendingDelta_ = 0;
    lastDirection_ = 0;
    lastDetentMs_ = 0;
    lastFlushMs_ = 0;
  }

  bool PanelRadioRuntime::enqueue(CommandCode code, ParameterTarget target, int8_t delta) {
    if (state_ != State::Normal || !binding_.bound) {
#ifdef DEBUG
      Serial.printf(
        "ACT CMD queue=reject code=%u target=%u delta=%d state=%u\n",
        static_cast<unsigned>(code),
        static_cast<unsigned>(target),
        static_cast<int>(delta),
        static_cast<unsigned>(state_)
      );
#endif
      return false;
    }
    const bool queued = queueEvent({code, target, delta, clock_.nowMs()});
#ifdef DEBUG
    Serial.printf(
      "ACT CMD queue=%s code=%u target=%u delta=%d\n",
      queued ? "ok" : "full",
      static_cast<unsigned>(code),
      static_cast<unsigned>(target),
      static_cast<int>(delta)
    );
#endif
    return queued;
  }

  bool PanelRadioRuntime::selectParameter(ParameterTarget target) {
    if (target == ParameterTarget::None) return false;
    selectedTarget_ = target;
    return enqueue(CommandCode::SelectParameter, target, 0);
  }

  void PanelRadioRuntime::onEncoderRawTurn(int8_t direction) {
    if (state_ != State::Normal || !binding_.bound || direction == 0) return;
    const uint32_t now = clock_.nowMs();
    if (lastDirection_ != 0 && direction != lastDirection_) {
      if (pendingDelta_ != 0) {
        const int8_t delta = clampPacketDelta(pendingDelta_);
        if (enqueue(CommandCode::AdjustParameter, selectedTarget_, delta)) pendingDelta_ -= delta;
        lastFlushMs_ = now;
      }
    }
    const int8_t multiplier = lastDetentMs_ == 0 ? 1 : turnMultiplier(now - lastDetentMs_);
    pendingDelta_ += static_cast<int16_t>(direction) * multiplier;
    lastDetentMs_ = now;
#ifdef DEBUG
    Serial.printf("ACT ENC detent=%d aggregate=%d\n", static_cast<int>(direction), static_cast<int>(pendingDelta_));
#endif
    lastDirection_ = direction;
  }

  bool PanelRadioRuntime::buildScan(uint8_t storedChannel) {
    uint8_t legal[14] = {};
    uint8_t count = 0;
    scanCount_ = 0;
    scanIndex_ = 0;
    if (!transport_.countryChannels(legal, &count, sizeof(legal))) return false;
    for (uint8_t index = 0; index < count; ++index) {
      if (legal[index] == storedChannel) scanChannels_[scanCount_++] = storedChannel;
    }
    for (uint8_t index = 0; index < count; ++index) {
      if (legal[index] != storedChannel) scanChannels_[scanCount_++] = legal[index];
    }
    return scanCount_ != 0;
  }

  bool PanelRadioRuntime::ensureTransport(uint32_t now) {
    if (initialized_) return true;
    if (!due(now, nextRadioAttemptMs_)) return false;
    if (transport_.begin() && transport_.stationMac(panelMac_)) {
      random_.fill(panelNonce_, sizeof(panelNonce_));
      initialized_ = true;
      radioBackoffIndex_ = 0;
      nextRadioAttemptMs_ = now;
      return true;
    }
    scheduleTransportRetry(now);
    return false;
  }

  void PanelRadioRuntime::scheduleTransportRetry(uint32_t now) {
    const uint8_t index = radioBackoffIndex_ < 5 ? radioBackoffIndex_++ : 5;
    nextRadioAttemptMs_ = now + kRadioBackoffMs[index];
  }

  void PanelRadioRuntime::startPairing() {
    if (!initialized_) return;
    const uint32_t now = clock_.nowMs();
    oldBinding_ = binding_;
    clearCommands();
    clearAggregation();
    random_.fill(panelNonce_, sizeof(panelNonce_));
    memset(lampNonce_, 0, sizeof(lampNonce_));
    campaignStartedMs_ = now;
    nextScanMs_ = now;
    scanBackoffMs_ = 0;
    scanCount_ = 0;
    state_ = State::PairScan;
  }

  bool PanelRadioRuntime::scanTick(uint32_t now, bool pairing) {
    if (!ensureTransport(now) || !due(now, nextScanMs_)) return false;
    if (scanCount_ == 0) {
      if (!due(now, nextRadioAttemptMs_)) return false;
      if (!buildScan(pairing ? oldBinding_.channel : binding_.channel)) {
        scheduleTransportRetry(now);
        return false;
      }
    }
    if (scanIndex_ == scanCount_) {
      scanIndex_ = 0;
      const uint32_t* backoff = pairing ? kPairBackoffMs : kRecoveryBackoffMs;
      const uint8_t max = pairing ? 3 : 5;
      const uint8_t index = scanBackoffMs_ < max ? static_cast<uint8_t>(scanBackoffMs_++) : max;
      nextScanMs_ = now + backoff[index];
      return false;
    }
    const uint8_t channel = scanChannels_[scanIndex_++];
    currentChannel_ = channel;
    uint8_t frame[kMaxFrameSize] = {};
    size_t length = 0;
    bool sent = false;
    if (pairing) {
      sent = transport_.tuneForHello(channel) && protocol_.makeHello(panelMac_, panelNonce_, frame, &length);
    } else {
      sent = transport_.tuneForProbe(binding_, channel) &&
             protocol_.makeProbe(panelMac_, binding_, probeNonce_, frame, &length);
    }
    if (sent) {
#ifdef DEBUG
      const bool queued = transport_.send(frame, length);
      Serial.printf("ACT TX kind=%s queue=%s\n", pairing ? "hello" : "probe", queued ? "ok" : "fail");
#else
      transport_.send(frame, length);
#endif
    }
    nextScanMs_ = now + (pairing ? kPairDwellMs : kRecoveryDwellMs);
    return sent;
  }

  void PanelRadioRuntime::beginRecovery() {
    clearCommands();
    clearAggregation();
    random_.fill(probeNonce_, sizeof(probeNonce_));
    scanCount_ = 0;
    nextScanMs_ = clock_.nowMs();
    scanBackoffMs_ = 0;
    state_ = State::Recovery;
  }

  void PanelRadioRuntime::beginPersist(const Binding& candidate, State successState, bool resetCommandSession) {
    persistBinding_ = candidate;
    persistSuccessState_ = successState;
    resetCommandSession_ = resetCommandSession;
    persistAttempts_ = 0;
    nextPersistMs_ = clock_.nowMs();
    state_ = State::PersistConfirm;
  }

  void PanelRadioRuntime::persistTick(uint32_t now) {
    if (!due(now, nextPersistMs_)) return;
    uint8_t blob[kBindingBlobSize] = {};
    encodeBinding(persistBinding_, blob);
    ++persistAttempts_;
    if (store_.write(blob)) {
      binding_ = persistBinding_;
      if (resetCommandSession_) {
        random_.fill(panelNonce_, sizeof(panelNonce_));
        nextSequence_ = 1;
      }
      state_ = persistSuccessState_;
      return;
    }
    if (persistAttempts_ == 3) {
      binding_ = oldBinding_;
      state_ = oldBinding_.bound ? State::Normal : State::Unpaired;
      if (oldBinding_.bound) transport_.activateBoundPeer(oldBinding_);
      return;
    }
    nextPersistMs_ = now + 100;
  }

  bool PanelRadioRuntime::matchingMac(const uint8_t left[kMacSize], const uint8_t right[kMacSize]) const {
    return memcmp(left, right, kMacSize) == 0;
  }

  bool PanelRadioRuntime::matchingNonce(const uint8_t left[kNonceSize], const uint8_t right[kNonceSize]) const {
    return memcmp(left, right, kNonceSize) == 0;
  }

  void PanelRadioRuntime::handlePacket(const DecodedPacket& packet) {
    if (packet.kind == PacketKind::PairAccept) {
      if (
        state_ != State::PairScan || !matchingMac(packet.sourceMac, packet.lampMac) ||
        !matchingMac(packet.panelMac, panelMac_) || !matchingNonce(packet.firstNonce, panelNonce_) ||
        packet.channel != currentChannel_
      ) {
        ++pairAcceptGuardRejected_;
        return;
      }
      ++pairAcceptDecoded_;
      memcpy(lampNonce_, packet.secondNonce, kNonceSize);
      Binding candidate = {true, {}, packet.channel};
      memcpy(candidate.lampMac, packet.lampMac, kMacSize);
      if (!validBinding(candidate) || !transport_.activateBoundPeer(candidate)) {
        ++encryptedPeerFailed_;
        return;
      }
      persistBinding_ = candidate;
      confirmStartedMs_ = clock_.nowMs();
      nextConfirmAttemptMs_ = confirmStartedMs_;
      confirmAttempt_ = 0;
      state_ = State::Confirm;
      return;
    }
    if (
      packet.kind == PacketKind::ConfirmAck && state_ == State::Confirm && matchingMac(packet.panelMac, panelMac_) &&
      matchingMac(packet.sourceMac, packet.lampMac) && matchingMac(packet.lampMac, persistBinding_.lampMac) &&
      matchingNonce(packet.firstNonce, panelNonce_) && matchingNonce(packet.secondNonce, lampNonce_) &&
      packet.channel == persistBinding_.channel
    ) {
      beginPersist(persistBinding_, State::Normal, true);
      return;
    }
    if (
      packet.kind == PacketKind::ProbeAck && state_ == State::Recovery && matchingMac(packet.panelMac, panelMac_) &&
      matchingMac(packet.sourceMac, packet.lampMac) && matchingMac(packet.lampMac, binding_.lampMac) &&
      matchingNonce(packet.firstNonce, probeNonce_) && packet.channel == currentChannel_
    ) {
      Binding changed = binding_;
      changed.channel = packet.channel;
      if (!validBinding(changed)) return;
      oldBinding_ = binding_;
      if (changed.channel == binding_.channel) {
        state_ = State::Normal;
        transport_.activateBoundPeer(binding_);
      } else {
        beginPersist(changed, State::Normal, false);
      }
      return;
    }
    if (
      packet.kind == PacketKind::CommandAck && state_ == State::Normal && inFlight_.active &&
      matchingMac(packet.sourceMac, binding_.lampMac) && matchingMac(packet.lampMac, binding_.lampMac) &&
      matchingNonce(packet.firstNonce, panelNonce_) && packet.sequence == inFlight_.sequence
    ) {
      inFlight_.active = false;
#ifdef DEBUG
      Serial.printf(
        "ACT ACK seq=%lu status=%u\n",
        static_cast<unsigned long>(packet.sequence),
        static_cast<unsigned>(packet.ackStatus)
      );
#endif
    }
  }

  void PanelRadioRuntime::drainRx(uint32_t now) {
    for (uint8_t index = 0; index < 4; ++index) {
      RxEnvelope envelope = {};
      if (!transport_.next(&envelope)) return;
      if (envelope.length == 0 || envelope.length > kMaxFrameSize) continue;
      DecodedPacket packet = {};
      const DecodeResult decoded = protocol_.decode(envelope, panelMac_, binding_, &packet);
      if (decoded == DecodeResult::PairAcceptDecodeFailed) {
        ++pairAcceptDecodeFailed_;
        continue;
      }
      if (decoded == DecodeResult::PairAcceptAuthFailed) {
        ++pairAcceptAuthFailed_;
        continue;
      }
      if (decoded != DecodeResult::Accepted) continue;
      memcpy(packet.sourceMac, envelope.sourceMac, kMacSize);
      const uint32_t age = packet.kind == PacketKind::PairAccept   ? 1000
                           : packet.kind == PacketKind::ConfirmAck ? 2000
                                                                   : 500;
      if (elapsed(now, envelope.receivedAtMs, age)) {
        if (packet.kind == PacketKind::PairAccept) ++pairAcceptGuardRejected_;
        continue;
      }
      handlePacket(packet);
    }
  }

  void PanelRadioRuntime::aggregationTick(uint32_t now) {
    if (pendingDelta_ == 0 || (!elapsed(now, lastDetentMs_, 40) && !elapsed(now, lastFlushMs_, 80))) return;
    const int8_t delta = clampPacketDelta(pendingDelta_);
#ifdef DEBUG
    Serial.printf("ACT ENC aggregate=%d target=%u\n", static_cast<int>(delta), static_cast<unsigned>(selectedTarget_));
#endif
    if (enqueue(CommandCode::AdjustParameter, selectedTarget_, delta)) pendingDelta_ -= delta;
    lastFlushMs_ = now;
  }

  void PanelRadioRuntime::commandTick(uint32_t now) {
    if (state_ != State::Normal || !binding_.bound) return;
    if (!ensureTransport(now) || !transport_.activateBoundPeer(binding_)) return;
    if (inFlight_.active) {
      if (elapsed(now, inFlight_.sentAtMs, kCommandExpiryMs)) {
        beginRecovery();
      } else if (!inFlight_.retried && elapsed(now, inFlight_.sentAtMs, kCommandAckMs)) {
#ifdef DEBUG
        const bool queued = transport_.send(inFlight_.frame, inFlight_.length);
        Serial.printf(
          "ACT TX kind=command-retry seq=%lu queue=%s\n",
          static_cast<unsigned long>(inFlight_.sequence),
          queued ? "ok" : "fail"
        );
#else
        transport_.send(inFlight_.frame, inFlight_.length);
#endif
        inFlight_.retried = true;
      }
      return;
    }
    CommandEvent event = {};
    if (!popEvent(&event)) return;
    if (event.code == CommandCode::AdjustParameter && elapsed(now, event.createdAtMs, kCommandExpiryMs)) return;
    if (nextSequence_ == 0) {
      random_.fill(panelNonce_, sizeof(panelNonce_));
      nextSequence_ = 1;
    }
    size_t length = 0;
    const uint32_t sequence = nextSequence_++;
    if (!protocol_.makeCommand(panelMac_, binding_, panelNonce_, sequence, event, inFlight_.frame, &length)) return;
    inFlight_.active = true;
    inFlight_.retried = false;
    inFlight_.sequence = sequence;
    inFlight_.length = length;
    inFlight_.sentAtMs = now;
#ifdef DEBUG
    const bool queued = transport_.send(inFlight_.frame, inFlight_.length);
    Serial.printf(
      "ACT TX kind=command seq=%lu code=%u target=%u delta=%d queue=%s\n",
      static_cast<unsigned long>(sequence),
      static_cast<unsigned>(event.code),
      static_cast<unsigned>(event.target),
      static_cast<int>(event.delta),
      queued ? "ok" : "fail"
    );
#else
    transport_.send(inFlight_.frame, inFlight_.length);
#endif
  }

  void PanelRadioRuntime::tick() {
    if (!initialized_) {
      begin();
      return;
    }
    uint32_t now = clock_.nowMs();
    drainRx(now);
    now = clock_.nowMs();
    if (state_ == State::PairScan) {
      if (elapsed(now, campaignStartedMs_, kPairCampaignMs)) {
        binding_ = oldBinding_;
        state_ = binding_.bound ? State::Normal : State::Unpaired;
      } else {
        scanTick(now, true);
      }
    } else if (state_ == State::Confirm) {
      if (confirmAttempt_ < 4 && due(now, nextConfirmAttemptMs_)) {
        uint8_t frame[kMaxFrameSize] = {};
        size_t length = 0;
        if (
          protocol_.makeConfirm(
            panelMac_, persistBinding_.lampMac, panelNonce_, lampNonce_, persistBinding_.channel, frame, &length
          )
        ) {
          ++confirmEnqueueAttempts_;
          if (transport_.send(frame, length)) {
#ifdef DEBUG
            Serial.println("ACT TX kind=confirm queue=ok");
#endif
            ++confirmAttempt_;
            nextConfirmAttemptMs_ =
              confirmAttempt_ < 4 ? confirmStartedMs_ + kConfirmOffsets[confirmAttempt_] : confirmStartedMs_ + 2500;
          } else {
#ifdef DEBUG
            Serial.println("ACT TX kind=confirm queue=fail");
#endif
            ++confirmEnqueueFailed_;
            nextConfirmAttemptMs_ = now + kConfirmEnqueueRetryMs;
          }
        } else {
          ++confirmEncodeFailed_;
          nextConfirmAttemptMs_ = now + kConfirmEnqueueRetryMs;
        }
      }
      if (elapsed(now, confirmStartedMs_, 2500)) {
        state_ = State::PairScan;
        scanCount_ = 0;
        nextScanMs_ = now;
      }
    } else if (state_ == State::PersistConfirm) {
      persistTick(now);
    } else if (state_ == State::Recovery) {
      scanTick(now, false);
    }
    aggregationTick(now);
    commandTick(now);
  }

  State PanelRadioRuntime::state() const {
    return state_;
  }
  const Binding& PanelRadioRuntime::binding() const {
    return binding_;
  }
  Telemetry PanelRadioRuntime::telemetry() const {
    return {
      state_,
      commandDropped_,
      transport_.rxDropped(),
      pairAcceptDecoded_,
      pairAcceptDecodeFailed_,
      pairAcceptAuthFailed_,
      pairAcceptGuardRejected_,
      encryptedPeerFailed_,
      confirmEncodeFailed_,
      confirmEnqueueAttempts_,
      confirmEnqueueFailed_,
      transport_.txSucceeded(),
      transport_.txFailed()
    };
  }

} // namespace PanelRadio
