#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)

#include <ESP8266WiFi.h>

#include "../../detail/wifi_platform.h"

namespace {
  constexpr uint8_t kMailboxCapacity = 8;
  wifi_controller::detail::PlatformEvent mailbox[kMailboxCapacity]{};
  uint8_t mailboxReadIndex = 0;
  uint8_t mailboxWriteIndex = 0;
  uint8_t mailboxCount = 0;
  bool initialized = false;
  WiFiEventHandler stationConnectedEventHandler;
  WiFiEventHandler stationDisconnectedEventHandler;
  WiFiEventHandler stationGotIpEventHandler;
  WiFiEventHandler stationDhcpTimeoutEventHandler;

  void pushEvent(wifi_controller::detail::PlatformEventType type, uint16_t disconnectReason = 0) {
    if (mailboxCount == kMailboxCapacity) {
      mailboxReadIndex = (mailboxReadIndex + 1) % kMailboxCapacity;
      --mailboxCount;
    }
    mailbox[mailboxWriteIndex] = {type, disconnectReason};
    mailboxWriteIndex = (mailboxWriteIndex + 1) % kMailboxCapacity;
    ++mailboxCount;
  }

  WiFiMode_t toEspMode(wifi_controller::detail::Mode mode) {
    switch (mode) {
      case wifi_controller::detail::Mode::Off: return WIFI_OFF;
      case wifi_controller::detail::Mode::Sta: return WIFI_STA;
      case wifi_controller::detail::Mode::Ap: return WIFI_AP;
      case wifi_controller::detail::Mode::ApSta: return WIFI_AP_STA;
    }
    return WIFI_OFF;
  }

  IPAddress toIpAddress(const WifiController::Ipv4Address& address) {
    return IPAddress(address.octets[0], address.octets[1], address.octets[2], address.octets[3]);
  }

  WifiController::Ipv4Address fromIpAddress(const IPAddress& address) {
    return {{address[0], address[1], address[2], address[3]}};
  }
} // namespace

namespace wifi_controller {
  namespace detail {
    void platformInitialize() {
      if (initialized) return;

      stationConnectedEventHandler = WiFi.onStationModeConnected([](const WiFiEventStationModeConnected&) {
        pushEvent(PlatformEventType::StaConnected);
      });
      stationDisconnectedEventHandler =
        WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
          pushEvent(PlatformEventType::StaDisconnected, static_cast<uint16_t>(event.reason));
        });
      stationGotIpEventHandler =
        WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&) { pushEvent(PlatformEventType::StaGotIp); });
      stationDhcpTimeoutEventHandler =
        WiFi.onStationModeDHCPTimeout([]() { pushEvent(PlatformEventType::StaDhcpTimeout); });
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
      if (mailboxCount == 0) return false;
      event = mailbox[mailboxReadIndex];
      mailboxReadIndex = (mailboxReadIndex + 1) % kMailboxCapacity;
      --mailboxCount;
      return true;
    }
  } // namespace detail
} // namespace wifi_controller

#else
#error WifiController: unsupported platform
#endif
