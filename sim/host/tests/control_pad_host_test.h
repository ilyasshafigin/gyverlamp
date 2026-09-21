#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr int WIFI_IF_STA = 0;

struct esp_now_peer_info_t {
  uint8_t peer_addr[6];
  uint8_t lmk[16];
  uint8_t channel;
  int ifidx;
  bool encrypt;
};

using esp_now_recv_cb_t = void (*)(const uint8_t* sourceMac, const uint8_t* data, int length);
enum esp_now_send_status_t : uint8_t { ESP_NOW_SEND_SUCCESS, ESP_NOW_SEND_FAIL };
using esp_now_send_cb_t = void (*)(const uint8_t* destinationMac, esp_now_send_status_t status);

struct ControlPadHostQueue;
using QueueHandle_t = ControlPadHostQueue*;
using BaseType_t = int;
constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFALSE = 0;
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))

QueueHandle_t xQueueCreate(uint8_t length, size_t itemSize);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, uint32_t wait);
BaseType_t xQueueReceive(QueueHandle_t queue, void* item, uint32_t wait);
void xQueueReset(QueueHandle_t queue);

esp_err_t esp_now_init();
esp_err_t esp_now_deinit();
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback);
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback);
esp_err_t esp_now_set_pmk(const uint8_t key[16]);
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
esp_err_t esp_now_del_peer(const uint8_t peerMac[6]);
esp_err_t esp_now_send(const uint8_t destination[6], const uint8_t* data, size_t length);
esp_err_t esp_wifi_get_mac(int interface, uint8_t mac[6]);
void esp_fill_random(void* output, size_t length);

bool control_pad_host_hmac_sha256(
  const uint8_t* key, size_t keyLength, const uint8_t* data, size_t dataLength, uint8_t output[32]
);

namespace control_pad_host_test {

  struct PeerEvent {
    uint8_t mac[6];
    bool encrypted;
  };

  struct SendEvent {
    uint8_t destination[6];
    std::vector<uint8_t> data;
    uint32_t commitCountAtSend;
  };

  extern std::vector<PeerEvent> peerAdds;
  extern std::vector<PeerEvent> peerRemovals;
  extern std::vector<SendEvent> sends;
  extern uint32_t initCalls;
  extern uint32_t deinitCalls;
  extern uint8_t failPeerAdds;

  void reset();
  void deliver(const uint8_t sourceMac[6], const uint8_t* data, size_t length);
  void completeNextSend(bool success);

} // namespace control_pad_host_test
