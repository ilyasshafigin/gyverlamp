#pragma once

#include <stddef.h>
#include <stdint.h>

#include "panel_radio_binding.h"
#include "panel_radio_logic.h"

namespace PanelRadio {

constexpr size_t kMacSize = 6;
constexpr size_t kNonceSize = 16;
constexpr size_t kMaxFrameSize = 69;

enum class State : uint8_t { Unpaired, Normal, PairScan, Confirm, PersistConfirm, Recovery };
enum class PacketKind : uint8_t { None, PairAccept, ConfirmAck, ProbeAck, CommandAck };
enum class DecodeResult : uint8_t { Ignored, Accepted, PairAcceptDecodeFailed, PairAcceptAuthFailed };
enum class AckStatus : uint8_t { Applied = 1, Expired = 2, Stale = 3 };
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
enum class ParameterTarget : uint8_t { None = 0, Brightness = 1, Speed = 2, Scale = 3 };

struct CommandEvent {
  CommandCode code;
  ParameterTarget target;
  int8_t delta;
  uint32_t createdAtMs;
};

struct RxEnvelope {
  uint8_t sourceMac[kMacSize];
  uint8_t length;
  uint8_t data[kMaxFrameSize];
  uint32_t receivedAtMs;
};

struct DecodedPacket {
  PacketKind kind;
  uint8_t sourceMac[kMacSize];
  uint8_t panelMac[kMacSize];
  uint8_t lampMac[kMacSize];
  uint8_t firstNonce[kNonceSize];
  uint8_t secondNonce[kNonceSize];
  uint8_t channel;
  uint32_t sequence;
  AckStatus ackStatus;
};

class Clock {
 public:
  virtual ~Clock() {}
  virtual uint32_t nowMs() const = 0;
};

class Random {
 public:
  virtual ~Random() {}
  virtual void fill(uint8_t* output, size_t length) = 0;
};

class Transport {
 public:
  virtual ~Transport() {}
  virtual bool begin() = 0;
  virtual bool stationMac(uint8_t output[kMacSize]) = 0;
  virtual bool activateBoundPeer(const Binding& binding) = 0;
  virtual bool tuneForHello(uint8_t channel) = 0;
  virtual bool tuneForProbe(const Binding& binding, uint8_t channel) = 0;
  virtual bool send(const uint8_t* frame, size_t length) = 0;
  virtual bool next(RxEnvelope* envelope) = 0;
  virtual bool countryChannels(uint8_t* channels, uint8_t* count, uint8_t capacity) = 0;
  virtual uint32_t rxDropped() const = 0;
  virtual uint32_t txSucceeded() const = 0;
  virtual uint32_t txFailed() const = 0;
};

class Protocol {
 public:
  virtual ~Protocol() {}
  virtual bool makeHello(const uint8_t panelMac[kMacSize], const uint8_t nonce[kNonceSize], uint8_t* frame, size_t* length) = 0;
  virtual bool makeConfirm(const uint8_t panelMac[kMacSize], const uint8_t lampMac[kMacSize],
                           const uint8_t panelNonce[kNonceSize], const uint8_t lampNonce[kNonceSize], uint8_t channel,
                           uint8_t* frame, size_t* length) = 0;
  virtual bool makeProbe(const uint8_t panelMac[kMacSize], const Binding& binding, const uint8_t nonce[kNonceSize],
                         uint8_t* frame, size_t* length) = 0;
  virtual bool makeCommand(const uint8_t panelMac[kMacSize], const Binding& binding, const uint8_t nonce[kNonceSize],
                           uint32_t sequence, const CommandEvent& event, uint8_t* frame, size_t* length) = 0;
  virtual DecodeResult decode(const RxEnvelope& envelope, const uint8_t panelMac[kMacSize], const Binding& binding,
                              DecodedPacket* packet) = 0;
};

struct Telemetry {
  State state;
  uint32_t commandDropped;
  uint32_t rxDropped;
  uint32_t pairAcceptDecoded;
  uint32_t pairAcceptDecodeFailed;
  uint32_t pairAcceptAuthFailed;
  uint32_t pairAcceptGuardRejected;
  uint32_t encryptedPeerFailed;
  uint32_t confirmEncodeFailed;
  uint32_t confirmEnqueueAttempts;
  uint32_t confirmEnqueueFailed;
  uint32_t txSucceeded;
  uint32_t txFailed;
};

class PanelRadioRuntime {
 public:
  PanelRadioRuntime(Clock& clock, Random& random, Transport& transport, BindingStore& store, Protocol& protocol);

  void begin();
  void tick();
  void startPairing();
  bool enqueue(CommandCode code, ParameterTarget target, int8_t delta);
  bool selectParameter(ParameterTarget target);
  void onEncoderRawTurn(int8_t direction);
  State state() const;
  const Binding& binding() const;
  Telemetry telemetry() const;

 private:
  struct InFlight {
    bool active;
    bool retried;
    uint32_t sequence;
    uint32_t sentAtMs;
    uint8_t frame[kMaxFrameSize];
    size_t length;
  };

  bool queueEvent(const CommandEvent& event);
  bool popEvent(CommandEvent* event);
  void clearCommands();
  void clearAggregation();
  bool buildScan(uint8_t storedChannel);
  bool ensureTransport(uint32_t now);
  void scheduleTransportRetry(uint32_t now);
  bool scanTick(uint32_t now, bool pairing);
  void beginRecovery();
  void beginPersist(const Binding& candidate, State successState, bool resetCommandSession);
  void persistTick(uint32_t now);
  void drainRx(uint32_t now);
  void handlePacket(const DecodedPacket& packet);
  void commandTick(uint32_t now);
  void aggregationTick(uint32_t now);
  bool matchingMac(const uint8_t left[kMacSize], const uint8_t right[kMacSize]) const;
  bool matchingNonce(const uint8_t left[kNonceSize], const uint8_t right[kNonceSize]) const;

  Clock& clock_;
  Random& random_;
  Transport& transport_;
  BindingStore& store_;
  Protocol& protocol_;
  uint8_t panelMac_[kMacSize];
  bool bindingLoaded_;
  bool initialized_;
  Binding binding_;
  Binding oldBinding_;
  Binding persistBinding_;
  State persistSuccessState_;
  bool resetCommandSession_;
  State state_;
  uint8_t panelNonce_[kNonceSize];
  uint8_t lampNonce_[kNonceSize];
  uint8_t probeNonce_[kNonceSize];
  uint32_t campaignStartedMs_;
  uint32_t confirmStartedMs_;
  uint32_t nextConfirmAttemptMs_;
  uint8_t confirmAttempt_;
  uint8_t scanChannels_[14];
  uint8_t scanCount_;
  uint8_t scanIndex_;
  uint32_t nextScanMs_;
  uint8_t currentChannel_;
  uint32_t scanBackoffMs_;
  uint32_t nextRadioAttemptMs_;
  uint8_t radioBackoffIndex_;
  uint8_t persistAttempts_;
  uint32_t nextPersistMs_;
  CommandEvent events_[kCommandQueueCapacity];
  uint8_t eventHead_;
  uint8_t eventSize_;
  InFlight inFlight_;
  uint32_t nextSequence_;
  ParameterTarget selectedTarget_;
  int16_t pendingDelta_;
  int8_t lastDirection_;
  uint32_t lastDetentMs_;
  uint32_t lastFlushMs_;
  uint32_t commandDropped_;
  uint32_t pairAcceptDecoded_;
  uint32_t pairAcceptDecodeFailed_;
  uint32_t pairAcceptAuthFailed_;
  uint32_t pairAcceptGuardRejected_;
  uint32_t encryptedPeerFailed_;
  uint32_t confirmEncodeFailed_;
  uint32_t confirmEnqueueAttempts_;
  uint32_t confirmEnqueueFailed_;
};

} // namespace PanelRadio
