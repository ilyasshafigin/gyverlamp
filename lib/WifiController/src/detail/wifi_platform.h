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

    enum class PlatformEventType : uint8_t {
      StaConnected,
      StaDisconnected,
      StaGotIp,
      StaDhcpTimeout,
    };

    struct PlatformEvent {
      PlatformEventType type;
      uint16_t disconnectReason;
    };

    void platformInitialize();
    void platformSetAutoReconnect(bool enabled);
    void platformSetMode(Mode mode);
    void platformBeginSta(const char* ssid, const char* password);
    void platformDisconnectSta();
    StaLinkStatus platformStaLinkStatus();
    bool platformStartAp(const char* ssid, const char* password, const WifiController::Ipv4Address& ipAddress);
    void platformStopAp();
    uint8_t platformApClientCount();
    void platformSnapshot(WifiController::Snapshot& snapshot);
    uint32_t platformMillis();
    bool platformNextEvent(PlatformEvent& event);
  } // namespace detail
} // namespace wifi_controller
