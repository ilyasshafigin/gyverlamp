#if defined(USE_CONTROL_PAD) && defined(ARDUINO_ARCH_ESP32)

#include "control_pad_service.h"

#if defined(CONTROL_PAD_HOST_TEST)
#include <control_pad_host_test.h>
#else
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/md.h>
#endif

#include <cstring>

#include "../core/power_controller.h"
#include "../core/rotation_controller.h"
#include "../core/state_notifier.h"
#include "../effect/controller.h"
#include "../effect/palette_catalog.h"
#include "../notification/controller.h"
#include "../storage/settings_repository.h"

#include <WifiController.h>

namespace {

  constexpr uint32_t kPairingWindowMs = 60000;
  constexpr uint32_t kCandidateTimeoutMs = 10000;
  constexpr uint32_t kDuplicateConfirmMs = 30000;
  constexpr uint32_t kHelloAgeMs = 1000;
  constexpr uint32_t kConfirmAgeMs = 2000;
  constexpr uint32_t kProbeAgeMs = 500;
  constexpr uint32_t kCommandAgeMs = 500;
  constexpr uint32_t kPairAcceptIntervalMs = 500;
  constexpr uint32_t kPairAcceptTxTimeoutMs = 1000;
  constexpr uint8_t kPairAcceptCandidateMax = 6;
  constexpr uint8_t kPairAcceptWindowMax = 12;
  constexpr uint8_t kRxQueueLength = 8;
  constexpr uint8_t kRxDrainPerTick = 4;
  constexpr uint32_t kRadioBackoffMs[] = {1000, 2000, 4000, 8000, 16000, 30000};
  constexpr uint8_t kBroadcastMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  QueueHandle_t rxQueue = nullptr;
  portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;
  uint32_t rxDropped = 0;
  uint32_t rxQueued = 0;

  bool hmacSha256(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t dataLength, uint8_t out[32]) {
#if defined(CONTROL_PAD_HOST_TEST)
    return control_pad_host_hmac_sha256(key, keyLength, data, dataLength, out);
#else
    if (key == nullptr || data == nullptr || out == nullptr) return false;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return info != nullptr && mbedtls_md_hmac(info, key, keyLength, data, dataLength, out) == 0;
#endif
  }

  bool isBound(const ControlPadProtocol::BindingRecord& binding) {
    return (binding.flags & ControlPadProtocol::kBindingFlagBound) != 0;
  }

  uint8_t clampParameter(int value) {
    if (value < 1) return 1;
    if (value > 255) return 255;
    return static_cast<uint8_t>(value);
  }

  Palettes::Id nextPalette(Palettes::Id current) {
    for (uint8_t index = 0; index < Palettes::kSelectableCount; ++index) {
      if (Palettes::kSelectableOrder[index] != current) continue;
      return Palettes::kSelectableOrder[(index + 1) % Palettes::kSelectableCount];
    }
    return Palettes::kSelectableOrder[0];
  }

} // namespace

ControlPadService* ControlPadService::instance_ = nullptr;

ControlPadService::ControlPadService(
  EepromStore& eeprom,
  WifiController& wifi,
  PowerController& power,
  EffectController& effects,
  RotationController& rotation,
  SettingsRepository& settings,
  NotificationController& notifications,
  StateNotifier& stateNotifier
)
  : eeprom_(eeprom),
    wifi_(wifi),
    power_(power),
    effects_(effects),
    rotation_(rotation),
    settings_(settings),
    notifications_(notifications),
    stateNotifier_(stateNotifier) {
}

void ControlPadService::init() {
  binding_ = eeprom_.readControlPadBinding();
  esp_wifi_get_mac(WIFI_IF_STA, lampMac_.bytes);
  instance_ = this;
  if (rxQueue == nullptr) rxQueue = xQueueCreate(kRxQueueLength, sizeof(RxEnvelope));
}

void ControlPadService::onReceive(const uint8_t* sourceMac, const uint8_t* data, int length) {
  if (
    instance_ == nullptr || sourceMac == nullptr || data == nullptr || length <= 0 ||
    length > static_cast<int>(ControlPadProtocol::kMaxFrameSize) || rxQueue == nullptr
  ) {
    return;
  }
  RxEnvelope envelope = {};
  for (uint8_t index = 0; index < sizeof(envelope.sourceMac); ++index)
    envelope.sourceMac[index] = sourceMac[index];
  envelope.length = static_cast<uint8_t>(length);
  for (uint8_t index = 0; index < envelope.length; ++index)
    envelope.data[index] = data[index];
  envelope.receivedAtMs = millis();
  const BaseType_t queued = xQueueSend(rxQueue, &envelope, 0);
  portENTER_CRITICAL(&telemetryMux);
  if (queued == pdTRUE) ++rxQueued;
  else
    ++rxDropped;
  portEXIT_CRITICAL(&telemetryMux);
}

void ControlPadService::onSend(const uint8_t* destinationMac, esp_now_send_status_t status) {
  if (instance_ == nullptr) return;
  uint8_t difference = 0;
  if (destinationMac != nullptr) {
    for (uint8_t index = 0; index < sizeof(kBroadcastMac); ++index)
      difference = static_cast<uint8_t>(difference | (destinationMac[index] ^ kBroadcastMac[index]));
  } else {
    difference = 0xff;
  }
  portENTER_CRITICAL(&telemetryMux);
  if (instance_->pairAcceptTxPending_) {
    incrementCounter(&instance_->pairingCounters_.pairAcceptSendCallbacks);
    if (difference == 0) {
      incrementCounter(&instance_->pairingCounters_.pairAcceptBroadcastMacMatches);
      incrementCounter(&instance_->pairingCounters_.pairAcceptTxLatched);
      instance_->pairAcceptTxSucceeded_ = status == ESP_NOW_SEND_SUCCESS;
      instance_->pairAcceptTxComplete_ = true;
    }
  }
  portEXIT_CRITICAL(&telemetryMux);
}

void ControlPadService::tick() {
  const uint32_t nowMs = millis();
  processRequests(nowMs);
  ensureRadio(nowMs);
  if (!radioOnline_) return;
  if (pairingOpen_ && elapsed(nowMs, pairingOpenedAtMs_, kPairingWindowMs)) closePairing();
  processPairAcceptTx(nowMs);
  if (candidate_.active && elapsed(nowMs, candidate_.startedAtMs, kCandidateTimeoutMs)) {
    incrementCounter(&pairingCounters_.candidateTimedOut);
    recordFailure(Failure::CandidateTimeout);
    abortCandidate(true);
  }
  if (duplicateConfirmValid_ && elapsed(nowMs, duplicateConfirmAtMs_, kDuplicateConfirmMs)) clearDuplicateConfirm();
  drainQueue(nowMs);
}

ControlPadService::Status ControlPadService::status() const {
  Status result = {};
  const uint32_t nowMs = millis();
  result.bound = isBound(binding_);
  result.enabled = binding_.enabled;
  result.radioOnline = radioOnline_;
  result.pairingOpen = pairingOpen_;
  result.channel = activeChannel_;
  result.failure = terminalFailure_;
  result.pairingPhase = pairingPhase_;
  result.rootFailure = rootFailure_;
  result.terminalFailure = terminalFailure_;
  result.candidatePeerInstalled = candidate_.encryptedPeerInstalled;
  result.candidateChannel = candidate_.active ? candidate_.channel : 0;
  const uint32_t candidateAgeMs = candidate_.active ? nowMs - candidate_.startedAtMs : 0;
  result.candidateAgeMs = static_cast<uint16_t>(candidateAgeMs > 65535 ? 65535 : candidateAgeMs);
  for (uint8_t index = 0; index < sizeof(result.panelMac); ++index)
    result.panelMac[index] = binding_.panelMac.bytes[index];
  portENTER_CRITICAL(&telemetryMux);
  result.rxDropped = static_cast<uint16_t>(rxDropped > 65535 ? 65535 : rxDropped);
  result.pairAcceptTxPending = pairAcceptTxPending_;
  result.pairAcceptTxCompletionLatched = pairAcceptTxComplete_;
  result.pairingCounters = pairingCounters_;
  portEXIT_CRITICAL(&telemetryMux);
  return result;
}

const char* ControlPadService::statusName() const {
  if (pairingOpen_) return candidate_.active ? "Pairing candidate" : "Pairing open";
  if (!radioOnline_) return "Waiting for Wi-Fi";
  if (!isBound(binding_)) return "Unpaired";
  return binding_.enabled ? "Bound" : "Disabled";
}

const char* ControlPadService::failureName(Failure failure) {
  switch (failure) {
    case Failure::None: return "None";
    case Failure::PairAcceptTxFailed: return "PairAccept transmit failed";
    case Failure::PairAcceptTxTimeout: return "PairAccept transmit timeout";
    case Failure::CandidatePeerInstallFailed: return "Candidate peer setup failed";
    case Failure::ConfirmRejected: return "Confirm rejected";
    case Failure::ConfirmAckFailed: return "ConfirmAck failed";
    case Failure::BindingCommitFailed: return "Binding commit failed";
    case Failure::CandidateChannelChanged: return "Candidate channel changed";
    case Failure::CandidateRadioUnavailable: return "Candidate radio unavailable";
    case Failure::CandidateTimeout: return "Candidate timed out";
  }
  return "Unknown";
}

const char* ControlPadService::failureName() const {
  return failureName(terminalFailure_);
}

const char* ControlPadService::rootFailureName() const {
  return failureName(rootFailure_);
}

const char* ControlPadService::terminalFailureName() const {
  return failureName(terminalFailure_);
}

bool ControlPadService::requestOpenPairing(const char* artifact) {
  uint8_t parsed[ControlPadProtocol::kPairKeySize] = {};
  if (artifact == nullptr || !ControlPadProtocol::parseArtifact(artifact, strlen(artifact), parsed)) return false;
  const WifiController::Snapshot snapshot = wifi_.snapshot();
  if (!wifi_.staConnected() || snapshot.channel < 1 || snapshot.channel > 14) {
    zeroBytes(parsed, sizeof(parsed));
    return false;
  }
  for (uint8_t index = 0; index < sizeof(parsed); ++index)
    requestedKey_[index] = parsed[index];
  zeroBytes(parsed, sizeof(parsed));
  openRequested_ = true;
  return true;
}

void ControlPadService::requestClosePairing() {
  closeRequested_ = true;
}

void ControlPadService::requestClear() {
  clearRequested_ = true;
}

void ControlPadService::processRequests(uint32_t nowMs) {
  if (closeRequested_) {
    closeRequested_ = false;
    openRequested_ = false;
    zeroBytes(requestedKey_, sizeof(requestedKey_));
    closePairing();
    resetPairingDiagnostics();
  }
  if (clearRequested_) {
    clearRequested_ = false;
    if (eeprom_.clearControlPadBinding()) {
      binding_ = eeprom_.readControlPadBinding();
      closePairing();
      clearDuplicateConfirm();
      currentNonceValid_ = false;
      zeroBytes(lastCommandFrame_, sizeof(lastCommandFrame_));
      zeroBytes(lastAckFrame_, sizeof(lastAckFrame_));
      for (uint8_t index = 0; index < sizeof(retiredNonces_) / sizeof(retiredNonces_[0]); ++index)
        retiredNonces_[index].valid = false;
      removePeer();
      resetPairingDiagnostics();
    }
  }
  if (!openRequested_) return;
  openRequested_ = false;
  const WifiController::Snapshot snapshot = wifi_.snapshot();
  if (!wifi_.staConnected() || snapshot.channel < 1 || snapshot.channel > 14) {
    zeroBytes(requestedKey_, sizeof(requestedKey_));
    return;
  }
  closePairing();
  for (uint8_t index = 0; index < sizeof(pairingKey_); ++index)
    pairingKey_[index] = requestedKey_[index];
  zeroBytes(requestedKey_, sizeof(requestedKey_));
  pairingOpen_ = true;
  pairingOpenedAtMs_ = nowMs;
  pairAcceptWindowCount_ = 0;
  resetPairingDiagnostics();
}

void ControlPadService::ensureRadio(uint32_t nowMs) {
  const WifiController::Snapshot snapshot = wifi_.snapshot();
  const bool staReady = wifi_.staConnected() && snapshot.channel >= 1 && snapshot.channel <= 14;
  if (!staReady) {
    if (radioOnline_) {
      if (candidate_.active) recordFailure(Failure::CandidateRadioUnavailable);
      teardownRadio();
    }
    return;
  }
  const uint8_t channel = static_cast<uint8_t>(snapshot.channel);
  if (radioOnline_ && activeChannel_ != channel) {
    if (candidate_.active) recordFailure(Failure::CandidateChannelChanged);
    teardownRadio();
  }
  if (radioOnline_) return;
  if (retryAtMs_ != 0 && static_cast<int32_t>(nowMs - retryAtMs_) < 0) return;
  if (esp_now_init() != ESP_OK) {
    const uint8_t index = retryIndex_ >= 5 ? 5 : retryIndex_++;
    retryAtMs_ = nowMs + kRadioBackoffMs[index];
    return;
  }
  if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
    esp_now_deinit();
    const uint8_t index = retryIndex_ >= 5 ? 5 : retryIndex_++;
    retryAtMs_ = nowMs + kRadioBackoffMs[index];
    return;
  }
  if (esp_now_register_send_cb(onSend) != ESP_OK) {
    esp_now_deinit();
    const uint8_t index = retryIndex_ >= 5 ? 5 : retryIndex_++;
    retryAtMs_ = nowMs + kRadioBackoffMs[index];
    return;
  }
  esp_wifi_get_mac(WIFI_IF_STA, lampMac_.bytes);
  activeChannel_ = channel;
  radioOnline_ = true;
  if (rxQueue != nullptr) xQueueReset(rxQueue);
  if (isBound(binding_) && binding_.enabled && !installBoundPeer()) {
    teardownRadio();
    const uint8_t index = retryIndex_ >= 5 ? 5 : retryIndex_++;
    retryAtMs_ = nowMs + kRadioBackoffMs[index];
    return;
  }
  retryIndex_ = 0;
  retryAtMs_ = 0;
}

void ControlPadService::teardownRadio() {
  abortCandidate(false);
  if (rxQueue != nullptr) xQueueReset(rxQueue);
  removePeer();
  if (radioOnline_) esp_now_deinit();
  radioOnline_ = false;
  activeChannel_ = 0;
}

void ControlPadService::processPairAcceptTx(uint32_t nowMs) {
  bool completed = false;
  bool succeeded = false;
  portENTER_CRITICAL(&telemetryMux);
  if (pairAcceptTxPending_ && pairAcceptTxComplete_) {
    completed = true;
    succeeded = pairAcceptTxSucceeded_;
    pairAcceptTxPending_ = false;
    pairAcceptTxComplete_ = false;
  }
  portEXIT_CRITICAL(&telemetryMux);

  if (!completed && pairAcceptTxPending_ && elapsed(nowMs, pairAcceptTxQueuedAtMs_, kPairAcceptTxTimeoutMs)) {
    cancelPairAcceptTx();
    incrementCounter(&pairingCounters_.pairAcceptTxTimedOut);
    recordFailure(Failure::PairAcceptTxTimeout);
    return;
  }
  if (!completed) return;
  incrementCounter(&pairingCounters_.pairAcceptTxProcessed);
  if (!succeeded) {
    incrementCounter(&pairingCounters_.pairAcceptTxFailed);
    recordFailure(Failure::PairAcceptTxFailed);
    return;
  }
  if (!candidate_.active || candidate_.encryptedPeerInstalled) return;
  pairingPhase_ = PairingPhase::PairAcceptTxSucceeded;
  terminalFailure_ = Failure::None;
  incrementCounter(&pairingCounters_.pairAcceptTxSucceeded);
  if (!installCandidatePeer(true)) {
    incrementCounter(&pairingCounters_.candidatePeerInstallFailed);
    recordFailure(Failure::CandidatePeerInstallFailed);
    abortCandidate(true);
    return;
  }
  candidate_.encryptedPeerInstalled = true;
  pairingPhase_ = PairingPhase::CandidatePeerInstalled;
  terminalFailure_ = Failure::None;
  incrementCounter(&pairingCounters_.candidatePeerInstalled);
}

void ControlPadService::drainQueue(uint32_t nowMs) {
  for (uint8_t index = 0; index < kRxDrainPerTick; ++index) {
    RxEnvelope envelope = {};
    if (xQueueReceive(rxQueue, &envelope, 0) != pdTRUE) return;
    handleEnvelope(envelope, nowMs);
  }
}

void ControlPadService::handleEnvelope(const RxEnvelope& envelope, uint32_t nowMs) {
  ControlPadProtocol::WireHeader header = {};
  if (!ControlPadProtocol::decodeHeader(envelope.data, envelope.length, &header)) return;
  uint32_t maxAgeMs = kCommandAgeMs;
  switch (header.type) {
    case ControlPadProtocol::MessageType::PairHello:
    case ControlPadProtocol::MessageType::PairAccept: maxAgeMs = kHelloAgeMs; break;
    case ControlPadProtocol::MessageType::EncryptedConfirm:
    case ControlPadProtocol::MessageType::ConfirmAck: maxAgeMs = kConfirmAgeMs; break;
    case ControlPadProtocol::MessageType::Probe:
    case ControlPadProtocol::MessageType::ProbeAck: maxAgeMs = kProbeAgeMs; break;
    case ControlPadProtocol::MessageType::Command:
    case ControlPadProtocol::MessageType::CommandAck: maxAgeMs = kCommandAgeMs; break;
  }
  if (header.type != ControlPadProtocol::MessageType::Command && elapsed(nowMs, envelope.receivedAtMs, maxAgeMs))
    return;
  switch (header.type) {
    case ControlPadProtocol::MessageType::PairHello: handleHello(envelope, nowMs); break;
    case ControlPadProtocol::MessageType::EncryptedConfirm: handleConfirm(envelope, nowMs); break;
    case ControlPadProtocol::MessageType::Probe: handleProbe(envelope, nowMs); break;
    case ControlPadProtocol::MessageType::Command: handleCommand(envelope, nowMs); break;
    default: break;
  }
}

void ControlPadService::handleHello(const RxEnvelope& envelope, uint32_t nowMs) {
  if (!pairingOpen_ || elapsed(nowMs, pairingOpenedAtMs_, kPairingWindowMs)) return;
  ControlPadProtocol::Message hello = {};
  if (
    !ControlPadProtocol::decodeMessage(envelope.data, envelope.length, &hello) ||
    !sameBytes(envelope.sourceMac, hello.panelMac.bytes, sizeof(envelope.sourceMac)) ||
    !ControlPadProtocol::authenticateMessage(hello, pairingKey_, hmacSha256)
  )
    return;
  if (!candidate_.active) {
    candidate_.active = true;
    pairingPhase_ = PairingPhase::HelloAccepted;
    terminalFailure_ = Failure::None;
    candidate_.panelMac = hello.panelMac;
    candidate_.panelNonce = hello.firstNonce;
    esp_fill_random(candidate_.lampNonce.bytes, sizeof(candidate_.lampNonce.bytes));
    for (uint8_t index = 0; index < sizeof(candidate_.key); ++index)
      candidate_.key[index] = pairingKey_[index];
    candidate_.channel = activeChannel_;
    candidate_.startedAtMs = nowMs;
    candidate_.acceptCount = 0;
    candidate_.helloLength = envelope.length;
    for (uint8_t index = 0; index < envelope.length; ++index)
      candidate_.helloFrame[index] = envelope.data[index];
  } else if (
    candidate_.helloLength != envelope.length || !sameBytes(candidate_.helloFrame, envelope.data, envelope.length)
  ) {
    return;
  }
  if (
    candidate_.acceptCount >= kPairAcceptCandidateMax || pairAcceptWindowCount_ >= kPairAcceptWindowMax ||
    (candidate_.lastAcceptAtMs != 0 && !elapsed(nowMs, candidate_.lastAcceptAtMs, kPairAcceptIntervalMs))
  )
    return;
  if (sendPairAccept(nowMs)) {
    ++candidate_.acceptCount;
    ++pairAcceptWindowCount_;
    candidate_.lastAcceptAtMs = nowMs;
  }
}

void ControlPadService::handleConfirm(const RxEnvelope& envelope, uint32_t nowMs) {
  if (
    duplicateConfirmValid_ && !elapsed(nowMs, duplicateConfirmAtMs_, kDuplicateConfirmMs) &&
    envelope.length == duplicateConfirmLength_ && sameBytes(envelope.sourceMac, binding_.panelMac.bytes, 6) &&
    sameBytes(envelope.data, duplicateConfirmFrame_, envelope.length)
  ) {
    ControlPadProtocol::Message confirm = {};
    if (
      ControlPadProtocol::decodeMessage(envelope.data, envelope.length, &confirm) &&
      ControlPadProtocol::authenticateMessage(confirm, binding_.pairKey, hmacSha256)
    ) {
      sendFrame(binding_.panelMac.bytes, duplicateConfirmAck_, duplicateConfirmAckLength_);
    }
    return;
  }
  if (
    !candidate_.active || !candidate_.encryptedPeerInstalled ||
    !sameBytes(envelope.sourceMac, candidate_.panelMac.bytes, 6)
  )
    return;
  pairingPhase_ = PairingPhase::ConfirmReceived;
  terminalFailure_ = Failure::None;
  incrementCounter(&pairingCounters_.confirmReceived);
  ControlPadProtocol::Message confirm = {};
  if (
    !ControlPadProtocol::decodeMessage(envelope.data, envelope.length, &confirm) ||
    confirm.type != ControlPadProtocol::MessageType::EncryptedConfirm ||
    !sameBytes(confirm.panelMac.bytes, candidate_.panelMac.bytes, 6) ||
    !sameBytes(confirm.lampMac.bytes, lampMac_.bytes, 6) ||
    !sameBytes(confirm.firstNonce.bytes, candidate_.panelNonce.bytes, 16) ||
    !sameBytes(confirm.secondNonce.bytes, candidate_.lampNonce.bytes, 16) || confirm.channel != candidate_.channel ||
    !ControlPadProtocol::authenticateMessage(confirm, candidate_.key, hmacSha256)
  ) {
    incrementCounter(&pairingCounters_.confirmRejected);
    recordFailure(Failure::ConfirmRejected);
    return;
  }
  ControlPadProtocol::BindingRecord next = {};
  next.enabled = true;
  next.protocolVersion = ControlPadProtocol::kProtocolVersion;
  next.panelMac = candidate_.panelMac;
  next.lastChannel = activeChannel_;
  next.flags = ControlPadProtocol::kBindingFlagBound;
  for (uint8_t index = 0; index < sizeof(next.pairKey); ++index)
    next.pairKey[index] = candidate_.key[index];
  uint8_t ack[69] = {};
  uint8_t ackLength = 0;
  if (!sendConfirmAck(candidate_, ack, &ackLength)) {
    recordFailure(Failure::ConfirmAckFailed);
    abortCandidate(true);
    return;
  }
  if (!eeprom_.writeControlPadBinding(next)) {
    recordFailure(Failure::BindingCommitFailed);
    abortCandidate(true);
    return;
  }
  binding_ = next;
  duplicateConfirmValid_ = true;
  duplicateConfirmAtMs_ = nowMs;
  duplicateConfirmLength_ = envelope.length;
  duplicateConfirmAckLength_ = ackLength;
  for (uint8_t index = 0; index < envelope.length; ++index)
    duplicateConfirmFrame_[index] = envelope.data[index];
  for (uint8_t index = 0; index < ackLength; ++index)
    duplicateConfirmAck_[index] = ack[index];
  sendFrame(binding_.panelMac.bytes, ack, ackLength);
  candidate_ = Candidate{};
  cancelPairAcceptTx();
  incrementCounter(&pairingCounters_.confirmAccepted);
  completePairingDiagnostics();
  closePairing();
}

void ControlPadService::handleProbe(const RxEnvelope& envelope, uint32_t) {
  if (!isBound(binding_) || !binding_.enabled || !sameBytes(envelope.sourceMac, binding_.panelMac.bytes, 6)) return;
  ControlPadProtocol::Message probe = {};
  if (
    !ControlPadProtocol::decodeMessage(envelope.data, envelope.length, &probe) ||
    probe.type != ControlPadProtocol::MessageType::Probe ||
    !sameBytes(probe.panelMac.bytes, binding_.panelMac.bytes, 6) ||
    !sameBytes(probe.lampMac.bytes, lampMac_.bytes, 6) ||
    !ControlPadProtocol::authenticateMessage(probe, binding_.pairKey, hmacSha256)
  )
    return;
  sendProbeAck(probe);
}

void ControlPadService::handleCommand(const RxEnvelope& envelope, uint32_t nowMs) {
  if (!isBound(binding_) || !binding_.enabled || !sameBytes(envelope.sourceMac, binding_.panelMac.bytes, 6)) return;
  ControlPadProtocol::Command command = {};
  if (
    !ControlPadProtocol::decodeCommand(envelope.data, envelope.length, &command) ||
    !ControlPadProtocol::authenticateCommand(command, binding_.pairKey, binding_.panelMac, lampMac_, hmacSha256)
  )
    return;
  uint8_t ack[45] = {};
  uint8_t ackLength = 0;
  if (isRetiredNonce(command.panelBootNonce)) {
    if (sendCommandAck(command, ControlPadProtocol::CommandAckStatus::Stale, ack, &ackLength))
      sendFrame(binding_.panelMac.bytes, ack, ackLength);
    return;
  }
  if (!currentNonceValid_ || !sameBytes(command.panelBootNonce.bytes, currentNonce_.bytes, 16))
    switchCommandNonce(command.panelBootNonce);
  if (command.sequence == highestSequence_ && highestSequence_ != 0) {
    if (sameBytes(envelope.data, lastCommandFrame_, envelope.length))
      sendFrame(binding_.panelMac.bytes, lastAckFrame_, lastAckLength_);
    return;
  }
  if (command.sequence < highestSequence_) {
    if (sendCommandAck(command, ControlPadProtocol::CommandAckStatus::Stale, ack, &ackLength))
      sendFrame(binding_.panelMac.bytes, ack, ackLength);
    return;
  }
  const ControlPadProtocol::CommandAckStatus status =
    elapsed(nowMs, envelope.receivedAtMs, kCommandAgeMs)
      ? ControlPadProtocol::CommandAckStatus::Expired
      : (applyCommand(command) ? ControlPadProtocol::CommandAckStatus::Applied
                               : ControlPadProtocol::CommandAckStatus::Expired);
  if (!sendCommandAck(command, status, ack, &ackLength)) return;
  cacheCommand(command.panelBootNonce, command.sequence, envelope.data, ack, ackLength);
  sendFrame(binding_.panelMac.bytes, ack, ackLength);
}

bool ControlPadService::installBoundPeer() {
  return installPeer(binding_.panelMac, binding_.pairKey, true);
}

bool ControlPadService::installCandidatePeer(bool encrypted) {
  return installPeer(candidate_.panelMac, candidate_.key, encrypted);
}

bool ControlPadService::installBroadcastPeer() {
  ControlPadProtocol::Mac broadcast = {};
  for (uint8_t index = 0; index < sizeof(broadcast.bytes); ++index)
    broadcast.bytes[index] = kBroadcastMac[index];
  return installPeer(broadcast, nullptr, false);
}

bool ControlPadService::installPeer(const ControlPadProtocol::Mac& mac, const uint8_t key[16], bool encrypted) {
  if (!radioOnline_) return false;
  removePeer();
  esp_now_peer_info_t peer = {};
  for (uint8_t index = 0; index < 6; ++index)
    peer.peer_addr[index] = mac.bytes[index];
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = encrypted;
  if (encrypted) {
    uint8_t pmk[16] = {};
    if (
      !ControlPadProtocol::derivePmk(key, hmacSha256, pmk) || esp_now_set_pmk(pmk) != ESP_OK ||
      !ControlPadProtocol::deriveLmk(key, mac, lampMac_, hmacSha256, peer.lmk)
    )
      return false;
  }
  if (esp_now_add_peer(&peer) != ESP_OK) return false;
  peerMac_ = mac;
  peerInstalled_ = true;
  return true;
}

void ControlPadService::removePeer() {
  if (!peerInstalled_) return;
  esp_now_del_peer(peerMac_.bytes);
  peerInstalled_ = false;
  zeroBytes(peerMac_.bytes, sizeof(peerMac_.bytes));
}

void ControlPadService::restoreBoundPeer() {
  removePeer();
  if (radioOnline_ && isBound(binding_) && binding_.enabled) installBoundPeer();
}

void ControlPadService::abortCandidate(bool restorePeer) {
  if (!candidate_.active) return;
  cancelPairAcceptTx();
  removePeer();
  zeroBytes(reinterpret_cast<uint8_t*>(&candidate_), sizeof(candidate_));
  if (restorePeer) restoreBoundPeer();
}

void ControlPadService::cancelPairAcceptTx() {
  portENTER_CRITICAL(&telemetryMux);
  pairAcceptTxPending_ = false;
  pairAcceptTxComplete_ = false;
  pairAcceptTxSucceeded_ = false;
  portEXIT_CRITICAL(&telemetryMux);
  pairAcceptTxQueuedAtMs_ = 0;
}

void ControlPadService::closePairing() {
  abortCandidate(true);
  pairingOpen_ = false;
  pairAcceptWindowCount_ = 0;
  zeroBytes(pairingKey_, sizeof(pairingKey_));
}

void ControlPadService::resetPairingDiagnostics() {
  pairingPhase_ = PairingPhase::Idle;
  rootFailure_ = Failure::None;
  terminalFailure_ = Failure::None;
  pairingCounters_ = PairingCounters{};
}

void ControlPadService::completePairingDiagnostics() {
  pairingPhase_ = PairingPhase::ConfirmAccepted;
  rootFailure_ = Failure::None;
  terminalFailure_ = Failure::None;
}

void ControlPadService::clearDuplicateConfirm() {
  duplicateConfirmValid_ = false;
  duplicateConfirmLength_ = 0;
  duplicateConfirmAckLength_ = 0;
  zeroBytes(duplicateConfirmFrame_, sizeof(duplicateConfirmFrame_));
  zeroBytes(duplicateConfirmAck_, sizeof(duplicateConfirmAck_));
}

bool ControlPadService::sendFrame(const uint8_t* destination, const uint8_t* data, uint8_t length) {
  return radioOnline_ && destination != nullptr && data != nullptr && length > 0 &&
         esp_now_send(destination, data, length) == ESP_OK;
}

bool ControlPadService::sendPairAccept(uint32_t nowMs) {
  if (pairAcceptTxPending_) return false;
  if (!installBroadcastPeer()) {
    incrementCounter(&pairingCounters_.pairAcceptTxFailed);
    recordFailure(Failure::PairAcceptTxFailed);
    return false;
  }
  ControlPadProtocol::Message accept = {};
  accept.type = ControlPadProtocol::MessageType::PairAccept;
  accept.panelMac = candidate_.panelMac;
  accept.lampMac = lampMac_;
  accept.firstNonce = candidate_.panelNonce;
  accept.secondNonce = candidate_.lampNonce;
  accept.channel = activeChannel_;
  if (!ControlPadProtocol::signMessage(&accept, candidate_.key, hmacSha256)) return false;
  uint8_t frame[69] = {};
  size_t length = 0;
  if (!ControlPadProtocol::encodeMessage(accept, frame, sizeof(frame), &length)) return false;
  portENTER_CRITICAL(&telemetryMux);
  pairAcceptTxPending_ = true;
  pairAcceptTxComplete_ = false;
  pairAcceptTxSucceeded_ = false;
  portEXIT_CRITICAL(&telemetryMux);
  pairAcceptTxQueuedAtMs_ = nowMs;
  if (sendFrame(kBroadcastMac, frame, static_cast<uint8_t>(length))) {
    pairingPhase_ = PairingPhase::PairAcceptQueued;
    terminalFailure_ = Failure::None;
    incrementCounter(&pairingCounters_.pairAcceptQueued);
    return true;
  }
  cancelPairAcceptTx();
  incrementCounter(&pairingCounters_.pairAcceptTxFailed);
  recordFailure(Failure::PairAcceptTxFailed);
  return false;
}

bool ControlPadService::sendConfirmAck(const Candidate& candidate, uint8_t output[69], uint8_t* outputLength) {
  if (output == nullptr || outputLength == nullptr) return false;
  ControlPadProtocol::Message ack = {};
  ack.type = ControlPadProtocol::MessageType::ConfirmAck;
  ack.panelMac = candidate.panelMac;
  ack.lampMac = lampMac_;
  ack.firstNonce = candidate.panelNonce;
  ack.secondNonce = candidate.lampNonce;
  ack.channel = activeChannel_;
  size_t length = 0;
  if (
    !ControlPadProtocol::signMessage(&ack, candidate.key, hmacSha256) ||
    !ControlPadProtocol::encodeMessage(ack, output, 69, &length)
  )
    return false;
  *outputLength = static_cast<uint8_t>(length);
  return true;
}

bool ControlPadService::sendProbeAck(const ControlPadProtocol::Message& probe) {
  ControlPadProtocol::Message ack = {};
  ack.type = ControlPadProtocol::MessageType::ProbeAck;
  ack.panelMac = binding_.panelMac;
  ack.lampMac = lampMac_;
  ack.firstNonce = probe.firstNonce;
  ack.channel = activeChannel_;
  uint8_t frame[69] = {};
  size_t length = 0;
  return ControlPadProtocol::signMessage(&ack, binding_.pairKey, hmacSha256) &&
         ControlPadProtocol::encodeMessage(ack, frame, sizeof(frame), &length) &&
         sendFrame(binding_.panelMac.bytes, frame, static_cast<uint8_t>(length));
}

bool ControlPadService::sendCommandAck(
  const ControlPadProtocol::Command& command,
  ControlPadProtocol::CommandAckStatus status,
  uint8_t output[45],
  uint8_t* outputLength
) {
  if (output == nullptr || outputLength == nullptr) return false;
  ControlPadProtocol::CommandAck ack = {};
  ack.panelBootNonce = command.panelBootNonce;
  ack.sequence = command.sequence;
  ack.status = status;
  size_t length = 0;
  if (
    !ControlPadProtocol::signCommandAck(&ack, binding_.pairKey, binding_.panelMac, lampMac_, hmacSha256) ||
    !ControlPadProtocol::encodeCommandAck(ack, output, 45, &length)
  )
    return false;
  *outputLength = static_cast<uint8_t>(length);
  return true;
}

bool ControlPadService::applyCommand(const ControlPadProtocol::Command& command) {
  switch (command.code) {
    case ControlPadProtocol::CommandCode::TogglePower: power_.toggle(); return true;
    case ControlPadProtocol::CommandCode::NextEffect:
      rotation_.onManualRotation();
      effects_.setNextEffect();
      stateNotifier_.stateChanged();
      return true;
    case ControlPadProtocol::CommandCode::PreviousEffect:
      rotation_.onManualRotation();
      effects_.setPreviousEffect();
      stateNotifier_.stateChanged();
      return true;
    case ControlPadProtocol::CommandCode::ToggleRotation: rotation_.setEnabled(!rotation_.isActive()); return true;
    case ControlPadProtocol::CommandCode::NextPalette:
      effects_.setPalette(nextPalette(effects_.selectedPalette()));
      stateNotifier_.stateChanged();
      return true;
    case ControlPadProtocol::CommandCode::SetPaletteAuto:
      effects_.setPalette(Palettes::Id::Auto);
      stateNotifier_.stateChanged();
      return true;
    case ControlPadProtocol::CommandCode::ResetCurrentEffectSettings:
      effects_.resetCurrentEffectSettingsToDefaults();
      stateNotifier_.stateChanged();
      return true;
    case ControlPadProtocol::CommandCode::SelectParameter:
      if (command.target == ControlPadProtocol::ParameterTarget::Brightness)
        notifications_.startUserTextNotification("BRI", CRGB::White, 1200);
      else if (command.target == ControlPadProtocol::ParameterTarget::Speed)
        notifications_.startUserTextNotification("SPD", CRGB::White, 1200);
      else if (command.target == ControlPadProtocol::ParameterTarget::Scale)
        notifications_.startUserTextNotification("SCL", CRGB::White, 1200);
      else
        return false;
      stateNotifier_.stateChanged();
      return true;
    case ControlPadProtocol::CommandCode::AdjustParameter: {
      const int delta = static_cast<int>(command.delta) * 15;
      switch (command.target) {
        case ControlPadProtocol::ParameterTarget::Brightness:
          effects_.setGlobalBrightness(clampParameter(static_cast<int>(settings_.globalBrightness()) + delta));
          break;
        case ControlPadProtocol::ParameterTarget::Speed:
          effects_.setEffectSpeed(
            clampParameter(static_cast<int>(settings_.effectSettings(effects_.selectedEffectId()).speed) + delta)
          );
          break;
        case ControlPadProtocol::ParameterTarget::Scale:
          effects_.setEffectScale(
            clampParameter(static_cast<int>(settings_.effectSettings(effects_.selectedEffectId()).scale) + delta)
          );
          break;
        case ControlPadProtocol::ParameterTarget::None: return false;
      }
      stateNotifier_.stateChanged();
      return true;
    }
    case ControlPadProtocol::CommandCode::NoAction: return false;
  }
  return false;
}

void ControlPadService::cacheCommand(
  const ControlPadProtocol::Nonce& nonce,
  uint32_t sequence,
  const uint8_t* commandFrame,
  const uint8_t* ackFrame,
  uint8_t ackLength
) {
  currentNonce_ = nonce;
  currentNonceValid_ = true;
  highestSequence_ = sequence;
  for (uint8_t index = 0; index < sizeof(lastCommandFrame_); ++index)
    lastCommandFrame_[index] = commandFrame[index];
  for (uint8_t index = 0; index < ackLength; ++index)
    lastAckFrame_[index] = ackFrame[index];
  lastAckLength_ = ackLength;
}

bool ControlPadService::isRetiredNonce(const ControlPadProtocol::Nonce& nonce) const {
  for (uint8_t index = 0; index < sizeof(retiredNonces_) / sizeof(retiredNonces_[0]); ++index) {
    if (retiredNonces_[index].valid && sameBytes(retiredNonces_[index].nonce.bytes, nonce.bytes, 16)) return true;
  }
  return false;
}

void ControlPadService::switchCommandNonce(const ControlPadProtocol::Nonce& nonce) {
  if (currentNonceValid_) {
    for (uint8_t index = 3; index > 0; --index)
      retiredNonces_[index] = retiredNonces_[index - 1];
    retiredNonces_[0].valid = true;
    retiredNonces_[0].nonce = currentNonce_;
  }
  currentNonce_ = nonce;
  currentNonceValid_ = true;
  highestSequence_ = 0;
  lastAckLength_ = 0;
  zeroBytes(lastCommandFrame_, sizeof(lastCommandFrame_));
  zeroBytes(lastAckFrame_, sizeof(lastAckFrame_));
}

void ControlPadService::recordFailure(Failure failure) {
  if (rootFailure_ == Failure::None) rootFailure_ = failure;
  terminalFailure_ = failure;
}

void ControlPadService::incrementCounter(uint16_t* counter) {
  if (counter != nullptr && *counter != 65535) ++*counter;
}

bool ControlPadService::elapsed(uint32_t nowMs, uint32_t thenMs, uint32_t durationMs) {
  return static_cast<uint32_t>(nowMs - thenMs) >= durationMs;
}

bool ControlPadService::sameBytes(const uint8_t* left, const uint8_t* right, size_t length) {
  if (left == nullptr || right == nullptr) return false;
  uint8_t difference = 0;
  for (size_t index = 0; index < length; ++index)
    difference = static_cast<uint8_t>(difference | (left[index] ^ right[index]));
  return difference == 0;
}

void ControlPadService::zeroBytes(uint8_t* data, size_t length) {
  for (size_t index = 0; index < length; ++index)
    data[index] = 0;
}

#endif
