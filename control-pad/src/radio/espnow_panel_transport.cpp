#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/md.h>

#include <string.h>

#include <ControlPadProtocol/ControlPadProtocol.h>

#include "panel_credentials.h"
#include "panel_radio_espnow.h"

namespace PanelRadio {
  namespace {

    using namespace ControlPadProtocol;

    constexpr uint8_t kRxQueueCapacity = 8;
    const uint8_t kBroadcastMac[kMacSize] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    QueueHandle_t rxQueue = nullptr;
    portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t dropped = 0;
    volatile uint32_t txSuccessCount = 0;
    volatile uint32_t txFailureCount = 0;

    bool hmacSha256(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t dataLength, uint8_t out[32]) {
      const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
      if (info == nullptr) return false;
      mbedtls_md_context_t context;
      mbedtls_md_init(&context);
      const int setup = mbedtls_md_setup(&context, info, 1);
      const int start = setup == 0 ? mbedtls_md_hmac_starts(&context, key, keyLength) : -1;
      const int update = start == 0 ? mbedtls_md_hmac_update(&context, data, dataLength) : -1;
      const int finish = update == 0 ? mbedtls_md_hmac_finish(&context, out) : -1;
      mbedtls_md_free(&context);
      return finish == 0;
    }

    void receiveCallback(const uint8_t* mac, const uint8_t* data, int length) {
      if (
        rxQueue == nullptr || mac == nullptr || data == nullptr || length <= 0 ||
        length > static_cast<int>(kMaxFrameSize)
      )
        return;
      RxEnvelope envelope = {};
      memcpy(envelope.sourceMac, mac, kMacSize);
      envelope.length = static_cast<uint8_t>(length);
      memcpy(envelope.data, data, envelope.length);
      envelope.receivedAtMs = millis();
      if (xQueueSend(rxQueue, &envelope, 0) != pdTRUE) {
        portENTER_CRITICAL_ISR(&telemetryMux);
        ++dropped;
        portEXIT_CRITICAL_ISR(&telemetryMux);
      }
    }

    void sendCallback(const uint8_t*, esp_now_send_status_t status) {
      portENTER_CRITICAL_ISR(&telemetryMux);
      if (status == ESP_NOW_SEND_SUCCESS) ++txSuccessCount;
      else
        ++txFailureCount;
      portEXIT_CRITICAL_ISR(&telemetryMux);
    }

    Mac makeMac(const uint8_t bytes[kMacSize]) {
      Mac result = {};
      memcpy(result.bytes, bytes, kMacSize);
      return result;
    }

    Nonce makeNonce(const uint8_t bytes[kNonceSize]) {
      Nonce result = {};
      memcpy(result.bytes, bytes, kNonceSize);
      return result;
    }

    bool encodeSigned(Message* message, const uint8_t key[16], uint8_t* frame, size_t* length) {
      return signMessage(message, key, hmacSha256) && encodeMessage(*message, frame, kMaxFrameSize, length);
    }

  } // namespace

  EspNowPanelTransport::EspNowPanelTransport()
    : ready_(false),
      peerActive_(false),
      peerEncrypted_(false),
      stationMacValid_(false),
      radioChannel_(0),
      peerChannel_(0),
      peerMac_{},
      stationMac_{} {
  }

  bool EspNowPanelTransport::begin() {
    if (ready_) return true;
    WiFi.mode(WIFI_STA);
    if (rxQueue == nullptr) rxQueue = xQueueCreate(kRxQueueCapacity, sizeof(RxEnvelope));
    if (rxQueue == nullptr || esp_now_init() != ESP_OK) return false;
    if (esp_wifi_get_mac(WIFI_IF_STA, stationMac_) != ESP_OK) {
      esp_now_deinit();
      return false;
    }
    stationMacValid_ = true;
    if (esp_now_register_recv_cb(receiveCallback) != ESP_OK || esp_now_register_send_cb(sendCallback) != ESP_OK) {
      stationMacValid_ = false;
      esp_now_deinit();
      return false;
    }
    ready_ = true;
    return true;
  }

  bool EspNowPanelTransport::stationMac(uint8_t output[kMacSize]) {
    if (!ready_ || !stationMacValid_ || output == nullptr) return false;
    memcpy(output, stationMac_, kMacSize);
    return true;
  }

  bool EspNowPanelTransport::addPeer(const uint8_t mac[kMacSize], bool encrypted, const uint8_t* lmk, uint8_t channel) {
    if (!ready_) return false;
    if (peerActive_) esp_now_del_peer(peerMac_);
    peerActive_ = false;
    peerEncrypted_ = false;
    peerChannel_ = 0;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, kMacSize);
    peer.channel = encrypted ? 0 : channel;
    peer.encrypt = encrypted;
    if (encrypted && lmk != nullptr) memcpy(peer.lmk, lmk, 16);
    if (esp_now_add_peer(&peer) != ESP_OK) return false;
    memcpy(peerMac_, mac, kMacSize);
    peerActive_ = true;
    peerEncrypted_ = encrypted;
    peerChannel_ = channel;
    return true;
  }

  bool EspNowPanelTransport::tune(uint8_t channel) {
    if (!ready_ || channel == 0 || channel > 14) return false;
    if (peerActive_) esp_now_del_peer(peerMac_);
    peerActive_ = false;
    peerEncrypted_ = false;
    peerChannel_ = 0;
    WiFi.disconnect(false, false);
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
    radioChannel_ = channel;
    return true;
  }

  bool EspNowPanelTransport::addBoundPeer(const Binding& binding) {
    uint8_t pmk[16] = {};
    uint8_t lmk[16] = {};
    if (
      !stationMacValid_ ||
      !derivePmk(PanelCredentials::K_PAIR, hmacSha256, pmk) ||
      !deriveLmk(PanelCredentials::K_PAIR, makeMac(stationMac_), makeMac(binding.lampMac), hmacSha256, lmk) ||
      esp_now_set_pmk(pmk) != ESP_OK
    ) {
      return false;
    }
    return addPeer(binding.lampMac, true, lmk, binding.channel);
  }

  bool EspNowPanelTransport::activateBoundPeer(const Binding& binding) {
    if (!begin() || !stationMacValid_ || !validBinding(binding)) return false;
    if (
      peerActive_ && peerEncrypted_ && peerChannel_ == binding.channel && radioChannel_ == binding.channel &&
      memcmp(peerMac_, binding.lampMac, kMacSize) == 0
    ) {
      return true;
    }
    return tune(binding.channel) && addBoundPeer(binding);
  }

  bool EspNowPanelTransport::tuneForHello(uint8_t channel) {
    return tune(channel) && addPeer(kBroadcastMac, false, nullptr, channel);
  }

  bool EspNowPanelTransport::tuneForProbe(const Binding& binding, uint8_t channel) {
    Binding adjusted = binding;
    adjusted.channel = channel;
    return begin() && validBinding(adjusted) && tune(channel) && addBoundPeer(adjusted);
  }

  bool EspNowPanelTransport::send(const uint8_t* frame, size_t length) {
    return ready_ && peerActive_ && frame != nullptr && length > 0 && length <= kMaxFrameSize &&
           esp_now_send(peerMac_, frame, length) == ESP_OK;
  }

  bool EspNowPanelTransport::next(RxEnvelope* envelope) {
    return envelope != nullptr && rxQueue != nullptr && xQueueReceive(rxQueue, envelope, 0) == pdTRUE;
  }

  bool EspNowPanelTransport::countryChannels(uint8_t* channels, uint8_t* count, uint8_t capacity) {
    if (channels == nullptr || count == nullptr) return false;
    wifi_country_t country = {};
    if (esp_wifi_get_country(&country) != ESP_OK || country.nchan == 0 || country.nchan > capacity) return false;
    for (uint8_t index = 0; index < country.nchan; ++index)
      channels[index] = country.schan + index;
    *count = country.nchan;
    return true;
  }

  uint32_t EspNowPanelTransport::rxDropped() const {
    uint32_t result = 0;
    portENTER_CRITICAL(&telemetryMux);
    result = dropped;
    portEXIT_CRITICAL(&telemetryMux);
    return result;
  }

  uint32_t EspNowPanelTransport::txSucceeded() const {
    uint32_t result = 0;
    portENTER_CRITICAL(&telemetryMux);
    result = txSuccessCount;
    portEXIT_CRITICAL(&telemetryMux);
    return result;
  }

  uint32_t EspNowPanelTransport::txFailed() const {
    uint32_t result = 0;
    portENTER_CRITICAL(&telemetryMux);
    result = txFailureCount;
    portEXIT_CRITICAL(&telemetryMux);
    return result;
  }

  PreferencesBindingStore::PreferencesBindingStore() {
  }

  bool PreferencesBindingStore::load(Binding* output) {
    if (output == nullptr) return false;
    Preferences preferences;
    if (!preferences.begin("glcp", true)) return false;
    uint8_t blob[kBindingBlobSize] = {};
    const bool loaded = preferences.getBytesLength("binding") == sizeof(blob) &&
                        preferences.getBytes("binding", blob, sizeof(blob)) == sizeof(blob);
    preferences.end();
    return loaded && decodeBinding(blob, output);
  }

  bool PreferencesBindingStore::write(const uint8_t blob[kBindingBlobSize]) {
    Preferences preferences;
    if (!preferences.begin("glcp", false)) return false;
    const bool written = preferences.putBytes("binding", blob, kBindingBlobSize) == kBindingBlobSize;
    preferences.end();
    return written;
  }

  void EspRandom::fill(uint8_t* output, size_t length) {
    esp_fill_random(output, length);
  }
  uint32_t MillisClock::nowMs() const {
    return millis();
  }

  FrozenProtocol::FrozenProtocol(const uint8_t pairKey[16]) {
    memcpy(pairKey_, pairKey, sizeof(pairKey_));
  }

  bool FrozenProtocol::makeHello(
    const uint8_t panelMac[kMacSize], const uint8_t nonce[kNonceSize], uint8_t* frame, size_t* length
  ) {
    Message message = {};
    message.type = MessageType::PairHello;
    message.panelMac = makeMac(panelMac);
    message.firstNonce = makeNonce(nonce);
    return encodeSigned(&message, pairKey_, frame, length);
  }

  bool FrozenProtocol::makeConfirm(
    const uint8_t panelMac[kMacSize],
    const uint8_t lampMac[kMacSize],
    const uint8_t panelNonce[kNonceSize],
    const uint8_t lampNonce[kNonceSize],
    uint8_t channel,
    uint8_t* frame,
    size_t* length
  ) {
    Message message = {};
    message.type = MessageType::EncryptedConfirm;
    message.panelMac = makeMac(panelMac);
    message.lampMac = makeMac(lampMac);
    message.firstNonce = makeNonce(panelNonce);
    message.secondNonce = makeNonce(lampNonce);
    message.channel = channel;
    return encodeSigned(&message, pairKey_, frame, length);
  }

  bool FrozenProtocol::makeProbe(
    const uint8_t panelMac[kMacSize],
    const Binding& binding,
    const uint8_t nonce[kNonceSize],
    uint8_t* frame,
    size_t* length
  ) {
    Message message = {};
    message.type = MessageType::Probe;
    message.panelMac = makeMac(panelMac);
    message.lampMac = makeMac(binding.lampMac);
    message.firstNonce = makeNonce(nonce);
    return encodeSigned(&message, pairKey_, frame, length);
  }

  bool FrozenProtocol::makeCommand(
    const uint8_t panelMac[kMacSize],
    const Binding& binding,
    const uint8_t nonce[kNonceSize],
    uint32_t sequence,
    const CommandEvent& event,
    uint8_t* frame,
    size_t* length
  ) {
    Command command = {};
    command.panelBootNonce = makeNonce(nonce);
    command.sequence = sequence;
    command.code = static_cast<ControlPadProtocol::CommandCode>(event.code);
    command.target = static_cast<ControlPadProtocol::ParameterTarget>(event.target);
    command.delta = event.delta;
    return signCommand(&command, pairKey_, makeMac(panelMac), makeMac(binding.lampMac), hmacSha256) &&
           encodeCommand(command, frame, kMaxFrameSize, length);
  }

  DecodeResult FrozenProtocol::decode(
    const RxEnvelope& envelope, const uint8_t panelMac[kMacSize], const Binding& binding, DecodedPacket* packet
  ) {
    if (packet == nullptr) return DecodeResult::Ignored;
    WireHeader header = {};
    if (!decodeHeader(envelope.data, envelope.length, &header)) return DecodeResult::Ignored;
    if (header.type == MessageType::CommandAck) {
      CommandAck ack = {};
      if (
        !decodeCommandAck(envelope.data, envelope.length, &ack) || !validBinding(binding) ||
        !authenticateCommandAck(ack, pairKey_, makeMac(panelMac), makeMac(binding.lampMac), hmacSha256)
      ) {
        return DecodeResult::Ignored;
      }
      if (memcmp(envelope.sourceMac, binding.lampMac, kMacSize) != 0) return DecodeResult::Ignored;
      packet->kind = PacketKind::CommandAck;
      memcpy(packet->lampMac, binding.lampMac, kMacSize);
      memcpy(packet->firstNonce, ack.panelBootNonce.bytes, kNonceSize);
      packet->sequence = ack.sequence;
      packet->ackStatus = static_cast<AckStatus>(ack.status);
      return DecodeResult::Accepted;
    }
    Message message = {};
    if (!decodeMessage(envelope.data, envelope.length, &message)) {
      return header.type == MessageType::PairAccept ? DecodeResult::PairAcceptDecodeFailed : DecodeResult::Ignored;
    }
    if (!authenticateMessage(message, pairKey_, hmacSha256)) {
      return message.type == MessageType::PairAccept ? DecodeResult::PairAcceptAuthFailed : DecodeResult::Ignored;
    }
    memcpy(packet->panelMac, message.panelMac.bytes, kMacSize);
    memcpy(packet->lampMac, message.lampMac.bytes, kMacSize);
    memcpy(packet->firstNonce, message.firstNonce.bytes, kNonceSize);
    memcpy(packet->secondNonce, message.secondNonce.bytes, kNonceSize);
    packet->channel = message.channel;
    if (message.type == MessageType::PairAccept) packet->kind = PacketKind::PairAccept;
    else if (message.type == MessageType::ConfirmAck)
      packet->kind = PacketKind::ConfirmAck;
    else if (message.type == MessageType::ProbeAck)
      packet->kind = PacketKind::ProbeAck;
    else
      return DecodeResult::Ignored;
    return DecodeResult::Accepted;
  }

} // namespace PanelRadio
