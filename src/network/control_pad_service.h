#pragma once

#if defined(USE_CONTROL_PAD) && defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>

#if defined(CONTROL_PAD_HOST_TEST)
#include <control_pad_host_test.h>
#else
#include <esp_now.h>
#endif

#include <ControlPadProtocol/ControlPadProtocol.h>

#include "../storage/eeprom_store.h"

class EffectController;
class NotificationController;
class PowerController;
class RotationController;
class SettingsRepository;
class StateNotifier;
class WifiController;

class ControlPadService {
public:
  enum class Failure : uint8_t {
    None,
    PairAcceptTxFailed,
    PairAcceptTxTimeout,
    CandidatePeerInstallFailed,
    ConfirmRejected,
    ConfirmAckFailed,
    BindingCommitFailed,
    CandidateChannelChanged,
    CandidateRadioUnavailable,
    CandidateTimeout,
  };

  enum class PairingPhase : uint8_t {
    Idle,
    HelloAccepted,
    PairAcceptQueued,
    PairAcceptTxSucceeded,
    CandidatePeerInstalled,
    ConfirmReceived,
    ConfirmAccepted,
  };

  struct PairingCounters {
    uint16_t pairAcceptQueued;
    uint16_t pairAcceptSendCallbacks;
    uint16_t pairAcceptBroadcastMacMatches;
    uint16_t pairAcceptTxLatched;
    uint16_t pairAcceptTxProcessed;
    uint16_t pairAcceptTxSucceeded;
    uint16_t pairAcceptTxFailed;
    uint16_t pairAcceptTxTimedOut;
    uint16_t candidatePeerInstalled;
    uint16_t candidatePeerInstallFailed;
    uint16_t confirmReceived;
    uint16_t confirmRejected;
    uint16_t confirmAccepted;
    uint16_t candidateTimedOut;
  };

  struct Status {
    bool bound;
    bool enabled;
    bool radioOnline;
    bool pairingOpen;
    uint8_t panelMac[6];
    uint8_t channel;
    uint16_t rxDropped;
    Failure failure;
    PairingPhase pairingPhase;
    Failure rootFailure;
    Failure terminalFailure;
    bool pairAcceptTxPending;
    bool pairAcceptTxCompletionLatched;
    bool candidatePeerInstalled;
    uint8_t candidateChannel;
    uint16_t candidateAgeMs;
    PairingCounters pairingCounters;
  };

  ControlPadService(
    EepromStore& eeprom,
    WifiController& wifi,
    PowerController& power,
    EffectController& effects,
    RotationController& rotation,
    SettingsRepository& settings,
    NotificationController& notifications,
    StateNotifier& stateNotifier
  );

  void init();
  void tick();

  Status status() const;
  const char* statusName() const;
  const char* failureName() const;
  const char* rootFailureName() const;
  const char* terminalFailureName() const;
  bool requestOpenPairing(const char* artifact);
  void requestClosePairing();
  void requestClear();

private:
  struct RxEnvelope {
    uint8_t sourceMac[6];
    uint8_t length;
    uint8_t data[ControlPadProtocol::kMaxFrameSize];
    uint32_t receivedAtMs;
  };

  struct Candidate {
    bool active;
    ControlPadProtocol::Mac panelMac;
    ControlPadProtocol::Nonce panelNonce;
    ControlPadProtocol::Nonce lampNonce;
    uint8_t key[ControlPadProtocol::kPairKeySize];
    uint8_t channel;
    uint8_t helloFrame[ControlPadProtocol::kMaxFrameSize];
    uint8_t helloLength;
    uint32_t startedAtMs;
    uint32_t lastAcceptAtMs;
    uint8_t acceptCount;
    bool encryptedPeerInstalled;
  };

  struct RetiredNonce {
    bool valid;
    ControlPadProtocol::Nonce nonce;
  };

  EepromStore& eeprom_;
  WifiController& wifi_;
  PowerController& power_;
  EffectController& effects_;
  RotationController& rotation_;
  SettingsRepository& settings_;
  NotificationController& notifications_;
  StateNotifier& stateNotifier_;

  ControlPadProtocol::BindingRecord binding_ = {};
  ControlPadProtocol::Mac lampMac_ = {};
  bool radioOnline_ = false;
  bool peerInstalled_ = false;
  ControlPadProtocol::Mac peerMac_ = {};
  uint8_t activeChannel_ = 0;
  uint8_t retryIndex_ = 0;
  uint32_t retryAtMs_ = 0;

  bool pairingOpen_ = false;
  uint8_t pairingKey_[ControlPadProtocol::kPairKeySize] = {};
  uint32_t pairingOpenedAtMs_ = 0;
  uint8_t pairAcceptWindowCount_ = 0;
  volatile bool pairAcceptTxPending_ = false;
  volatile bool pairAcceptTxComplete_ = false;
  volatile bool pairAcceptTxSucceeded_ = false;
  uint32_t pairAcceptTxQueuedAtMs_ = 0;
  PairingPhase pairingPhase_ = PairingPhase::Idle;
  Failure rootFailure_ = Failure::None;
  Failure terminalFailure_ = Failure::None;
  PairingCounters pairingCounters_ = {};
  bool openRequested_ = false;
  bool closeRequested_ = false;
  bool clearRequested_ = false;
  uint8_t requestedKey_[ControlPadProtocol::kPairKeySize] = {};
  Candidate candidate_ = {};

  bool duplicateConfirmValid_ = false;
  uint8_t duplicateConfirmFrame_[69] = {};
  uint8_t duplicateConfirmLength_ = 0;
  uint8_t duplicateConfirmAck_[69] = {};
  uint8_t duplicateConfirmAckLength_ = 0;
  uint32_t duplicateConfirmAtMs_ = 0;

  bool currentNonceValid_ = false;
  ControlPadProtocol::Nonce currentNonce_ = {};
  uint32_t highestSequence_ = 0;
  uint8_t lastCommandFrame_[47] = {};
  uint8_t lastAckFrame_[45] = {};
  uint8_t lastAckLength_ = 0;
  RetiredNonce retiredNonces_[4] = {};
  static ControlPadService* instance_;
  static void onReceive(const uint8_t* sourceMac, const uint8_t* data, int length);
  static void onSend(const uint8_t* destinationMac, esp_now_send_status_t status);

  void processRequests(uint32_t nowMs);
  void ensureRadio(uint32_t nowMs);
  void teardownRadio();
  void processPairAcceptTx(uint32_t nowMs);
  void drainQueue(uint32_t nowMs);
  void handleEnvelope(const RxEnvelope& envelope, uint32_t nowMs);
  void handleHello(const RxEnvelope& envelope, uint32_t nowMs);
  void handleConfirm(const RxEnvelope& envelope, uint32_t nowMs);
  void handleProbe(const RxEnvelope& envelope, uint32_t nowMs);
  void handleCommand(const RxEnvelope& envelope, uint32_t nowMs);

  bool installBoundPeer();
  bool installCandidatePeer(bool encrypted);
  bool installBroadcastPeer();
  bool installPeer(const ControlPadProtocol::Mac& mac, const uint8_t key[16], bool encrypted);
  void removePeer();
  void restoreBoundPeer();
  void abortCandidate(bool restorePeer);
  void cancelPairAcceptTx();
  void closePairing();
  void resetPairingDiagnostics();
  void completePairingDiagnostics();
  void clearDuplicateConfirm();
  bool sendFrame(const uint8_t* destination, const uint8_t* data, uint8_t length);
  bool sendPairAccept(uint32_t nowMs);
  bool sendConfirmAck(const Candidate& candidate, uint8_t output[69], uint8_t* outputLength);
  bool sendProbeAck(const ControlPadProtocol::Message& probe);
  bool sendCommandAck(
    const ControlPadProtocol::Command& command,
    ControlPadProtocol::CommandAckStatus status,
    uint8_t output[45],
    uint8_t* outputLength
  );
  bool applyCommand(const ControlPadProtocol::Command& command);
  void cacheCommand(
    const ControlPadProtocol::Nonce& nonce,
    uint32_t sequence,
    const uint8_t* commandFrame,
    const uint8_t* ackFrame,
    uint8_t ackLength
  );
  bool isRetiredNonce(const ControlPadProtocol::Nonce& nonce) const;
  void switchCommandNonce(const ControlPadProtocol::Nonce& nonce);
  void recordFailure(Failure failure);
  static const char* failureName(Failure failure);
  static void incrementCounter(uint16_t* counter);
  static bool elapsed(uint32_t nowMs, uint32_t thenMs, uint32_t durationMs);
  static bool sameBytes(const uint8_t* left, const uint8_t* right, size_t length);
  static void zeroBytes(uint8_t* data, size_t length);
};

#endif
