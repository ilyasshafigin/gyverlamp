#pragma once

#include <stdint.h>

#include "../WifiController.h"

namespace wifi_controller {
  namespace detail {
    enum class Mode : uint8_t {
      Off,
      Sta,
      Ap,
      ApSta,
    };

    enum class StaLinkStatus : uint8_t {
      Disconnected,
      Connecting,
      Connected,
    };

    struct PlatformEvent {
      uint16_t disconnectReason;
    };

    // Platform callbacks are registered once and remain active for the
    // process lifetime. Events contain only STA disconnects used by controller.

    void platformInitialize();
    void platformSetAutoReconnect(bool enabled);
    void platformSetMode(Mode mode);
    bool platformSetStaHostname(const char* hostname);
    bool platformBeginSta(const char* ssid, const char* password, const char* hostname);
    void platformDisconnectSta();
    StaLinkStatus platformStaLinkStatus();
    void platformTickNetworkServices(const char* hostname);
    bool platformStartAp(const char* ssid, const char* password, const WifiController::Ipv4Address& ipAddress);
    bool platformSetApHostname(const char* hostname);
    void platformStopAp();
    uint8_t platformApClientCount();
    void platformSnapshot(WifiController::Snapshot& snapshot);
    uint32_t platformMillis();
    bool platformNextEvent(PlatformEvent& event);
  } // namespace detail
} // namespace wifi_controller
