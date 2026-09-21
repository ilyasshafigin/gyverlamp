#pragma once

#include "panel_radio_runtime.h"

namespace PanelRadio {

class EspNowPanelTransport : public Transport {
 public:
  EspNowPanelTransport();
  bool begin() override;
  bool stationMac(uint8_t output[kMacSize]) override;
  bool activateBoundPeer(const Binding& binding) override;
  bool tuneForHello(uint8_t channel) override;
  bool tuneForProbe(const Binding& binding, uint8_t channel) override;
  bool send(const uint8_t* frame, size_t length) override;
  bool next(RxEnvelope* envelope) override;
  bool countryChannels(uint8_t* channels, uint8_t* count, uint8_t capacity) override;
  uint32_t rxDropped() const override;
  uint32_t txSucceeded() const override;
  uint32_t txFailed() const override;
 private:
  bool tune(uint8_t channel);
  bool addBoundPeer(const Binding& binding);
  bool addPeer(const uint8_t mac[kMacSize], bool encrypted, const uint8_t* lmk, uint8_t channel);
  bool ready_;
  bool peerActive_;
  bool peerEncrypted_;
  bool stationMacValid_;
  uint8_t radioChannel_;
  uint8_t peerChannel_;
  uint8_t peerMac_[kMacSize];
  uint8_t stationMac_[kMacSize];
};

class PreferencesBindingStore : public BindingStore {
 public:
  PreferencesBindingStore();
  bool load(Binding* output) override;
  bool write(const uint8_t blob[kBindingBlobSize]) override;
};

class EspRandom : public Random {
 public:
  void fill(uint8_t* output, size_t length) override;
};

class MillisClock : public Clock {
 public:
  uint32_t nowMs() const override;
};

class FrozenProtocol : public Protocol {
 public:
  FrozenProtocol(const uint8_t pairKey[16]);
  bool makeHello(const uint8_t panelMac[kMacSize], const uint8_t nonce[kNonceSize], uint8_t* frame, size_t* length) override;
  bool makeConfirm(const uint8_t panelMac[kMacSize], const uint8_t lampMac[kMacSize], const uint8_t panelNonce[kNonceSize],
                   const uint8_t lampNonce[kNonceSize], uint8_t channel, uint8_t* frame, size_t* length) override;
  bool makeProbe(const uint8_t panelMac[kMacSize], const Binding& binding, const uint8_t nonce[kNonceSize], uint8_t* frame,
                 size_t* length) override;
  bool makeCommand(const uint8_t panelMac[kMacSize], const Binding& binding, const uint8_t nonce[kNonceSize], uint32_t sequence,
                   const CommandEvent& event, uint8_t* frame, size_t* length) override;
  DecodeResult decode(const RxEnvelope& envelope, const uint8_t panelMac[kMacSize], const Binding& binding,
                      DecodedPacket* packet) override;
 private:
  uint8_t pairKey_[16];
};

} // namespace PanelRadio
