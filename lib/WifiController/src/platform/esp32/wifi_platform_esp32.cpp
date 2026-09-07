#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../../detail/wifi_platform.h"

namespace {
  constexpr UBaseType_t kEventQueueDepth = 8;

  QueueHandle_t eventQueue = nullptr;
  std::atomic<bool> resyncPending{false};
  std::atomic<uint16_t> latestDisconnectReason{0};
  bool initialized = false;

  WiFiMode_t toEspMode(wifi_controller::detail::Mode mode) {
    switch (mode) {
      case wifi_controller::detail::Mode::Off: return WIFI_MODE_NULL;
      case wifi_controller::detail::Mode::Sta: return WIFI_MODE_STA;
      case wifi_controller::detail::Mode::Ap: return WIFI_MODE_AP;
      case wifi_controller::detail::Mode::ApSta: return WIFI_MODE_APSTA;
    }
    return WIFI_MODE_NULL;
  }

  IPAddress toIpAddress(const WifiController::Ipv4Address& address) {
    return IPAddress(address.octets[0], address.octets[1], address.octets[2], address.octets[3]);
  }

  WifiController::Ipv4Address fromIpAddress(const IPAddress& address) {
    return WifiController::Ipv4Address(address[0], address[1], address[2], address[3]);
  }

  void queueEvent(const wifi_controller::detail::PlatformEvent& event) {
    if (eventQueue != nullptr && xQueueSend(eventQueue, &event, 0) == pdPASS) return;
    resyncPending.store(true, std::memory_order_release);
  }

  void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    using wifi_controller::detail::PlatformEvent;

    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
        const uint16_t reason = static_cast<uint16_t>(info.wifi_sta_disconnected.reason);
        latestDisconnectReason.store(reason, std::memory_order_release);
        queueEvent({reason});
        break;
      }

      default: break;
    }
  }
} // namespace

namespace wifi_controller {
  namespace detail {
    void platformInitialize() {
      if (initialized) return;

      eventQueue = xQueueCreate(kEventQueueDepth, sizeof(PlatformEvent));
      WiFi.onEvent(onWifiEvent);
      initialized = true;
    }

    void platformSetAutoReconnect(bool enabled) {
      WiFi.setAutoReconnect(enabled);
    }

    void platformSetMode(Mode mode) {
      WiFi.mode(toEspMode(mode));
    }

    void platformBeginSta(const char* ssid, const char* password) {
      WiFi.begin(ssid, password);
    }

    void platformDisconnectSta() {
      WiFi.disconnect(false, false);
    }

    StaLinkStatus platformStaLinkStatus() {
      if (WiFi.status() == WL_CONNECTED) return StaLinkStatus::Connected;
      if (WiFi.status() == WL_IDLE_STATUS) return StaLinkStatus::Connecting;
      return StaLinkStatus::Disconnected;
    }

    bool platformStartAp(const char* ssid, const char* password, const WifiController::Ipv4Address& ipAddress) {
      const IPAddress address = toIpAddress(ipAddress);
      if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0))) return false;
      return WiFi.softAP(ssid, password);
    }

    void platformStopAp() {
      WiFi.softAPdisconnect(true);
    }

    uint8_t platformApClientCount() {
      return WiFi.softAPgetStationNum();
    }

    void platformSnapshot(WifiController::Snapshot& snapshot) {
      strlcpy(snapshot.wifiSsid, WiFi.SSID().c_str(), sizeof(snapshot.wifiSsid));
      snapshot.localIp = fromIpAddress(WiFi.localIP());
      snapshot.gateway = fromIpAddress(WiFi.gatewayIP());
      strlcpy(snapshot.mac, WiFi.macAddress().c_str(), sizeof(snapshot.mac));
      snapshot.rssi = WiFi.RSSI();
      snapshot.channel = WiFi.channel();
    }

    uint32_t platformMillis() {
      return millis();
    }

    bool platformNextEvent(PlatformEvent& event) {
      if (resyncPending.exchange(false, std::memory_order_acq_rel)) {
        // Queue contents predate overflow and may no longer describe current link.
        if (eventQueue != nullptr) xQueueReset(eventQueue);
        if (platformStaLinkStatus() != StaLinkStatus::Disconnected) return false;
        event = {latestDisconnectReason.load(std::memory_order_acquire)};
        return true;
      }

      return eventQueue != nullptr && xQueueReceive(eventQueue, &event, 0) == pdPASS;
    }
  } // namespace detail
} // namespace wifi_controller

#endif
