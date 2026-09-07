#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>

#include "../../detail/wifi_platform.h"

namespace {
  constexpr uint8_t kMailboxCapacity = 8;
  constexpr uint32_t kServiceRetryIntervalMs = 5000;

  wifi_controller::detail::PlatformEvent mailbox[kMailboxCapacity]{};
  uint8_t mailboxReadIndex = 0;
  uint8_t mailboxWriteIndex = 0;
  uint8_t mailboxCount = 0;
  bool initialized = false;
  bool apRunning = false;
  bool mdnsRunning = false;
  bool mdnsAttempted = false;
  bool captiveDnsRunning = false;
  bool captiveDnsAttempted = false;
  uint32_t mdnsLastAttemptAt = 0;
  uint32_t captiveDnsLastAttemptAt = 0;
  IPAddress captiveDnsIp;
  DNSServer captiveDns;
  WiFiEventHandler stationDisconnectedEventHandler;

  void pushEvent(uint16_t disconnectReason) {
    if (mailboxCount == kMailboxCapacity) {
      mailboxReadIndex = (mailboxReadIndex + 1) % kMailboxCapacity;
      --mailboxCount;
    }
    mailbox[mailboxWriteIndex] = {disconnectReason};
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
    return WifiController::Ipv4Address(address[0], address[1], address[2], address[3]);
  }

  bool isZeroIpAddress(const IPAddress& address) {
    return address[0] == 0 && address[1] == 0 && address[2] == 0 && address[3] == 0;
  }

  bool sameIpAddress(const IPAddress& first, const IPAddress& second) {
    return first[0] == second[0] && first[1] == second[1] && first[2] == second[2] && first[3] == second[3];
  }

  bool retryDue(bool attempted, uint32_t now, uint32_t lastAttemptAt) {
    return !attempted || now - lastAttemptAt >= kServiceRetryIntervalMs;
  }

#ifdef DEBUG
  const __FlashStringHelper* modeName(wifi_controller::detail::Mode mode) {
    switch (mode) {
      case wifi_controller::detail::Mode::Off: return F("OFF");
      case wifi_controller::detail::Mode::Sta: return F("STA");
      case wifi_controller::detail::Mode::Ap: return F("AP");
      case wifi_controller::detail::Mode::ApSta: return F("AP+STA");
    }
    return F("unknown");
  }

  void logIp(const __FlashStringHelper* label, const IPAddress& address) {
    Serial.print(F("[WIFI] "));
    Serial.print(label);
    Serial.print(address[0]);
    Serial.print('.');
    Serial.print(address[1]);
    Serial.print('.');
    Serial.print(address[2]);
    Serial.print('.');
    Serial.println(address[3]);
  }
#endif

  void stopCaptiveDns() {
    if (captiveDnsRunning) {
#ifdef DEBUG
      Serial.println(F("[WIFI] captive DNS stop"));
#endif
      captiveDns.stop();
    }
    captiveDnsRunning = false;
    captiveDnsAttempted = false;
  }

  void tickCaptiveDns() {
    if (!apRunning) return;

    const IPAddress apIp = WiFi.softAPIP();
    if (isZeroIpAddress(apIp)) {
      if (captiveDnsRunning) stopCaptiveDns();
      return;
    }

    if (captiveDnsRunning && sameIpAddress(captiveDnsIp, apIp)) {
      captiveDns.processNextRequest();
      return;
    }

    if (captiveDnsRunning) {
#ifdef DEBUG
      Serial.println(F("[WIFI] captive DNS rebind"));
#endif
      stopCaptiveDns();
    }

    const uint32_t now = millis();
    if (!retryDue(captiveDnsAttempted, now, captiveDnsLastAttemptAt)) return;

#ifdef DEBUG
    if (captiveDnsAttempted) Serial.println(F("[WIFI] captive DNS retry"));
#endif
    captiveDnsAttempted = true;
    captiveDnsLastAttemptAt = now;
    if (!captiveDns.start(53, "*", apIp)) {
      captiveDns.stop();
#ifdef DEBUG
      Serial.println(F("[WIFI] captive DNS start failed"));
#endif
      return;
    }

    captiveDnsIp = apIp;
    captiveDnsRunning = true;
#ifdef DEBUG
    logIp(F("captive DNS started IP="), apIp);
#endif
  }

  void tickMdns(const char* hostname) {
    if (hostname == nullptr || hostname[0] == '\0') return;

    if (!mdnsRunning) {
      const IPAddress staIp = WiFi.localIP();
      const IPAddress apIp = WiFi.softAPIP();
      const bool staReady = WiFi.status() == WL_CONNECTED && !isZeroIpAddress(staIp);
      const bool apReady = apRunning && !isZeroIpAddress(apIp);
      if (!staReady && !apReady) return;

      const uint32_t now = millis();
      if (retryDue(mdnsAttempted, now, mdnsLastAttemptAt)) {
#ifdef DEBUG
        if (mdnsAttempted) Serial.println(F("[WIFI] mDNS retry"));
        logIp(staReady ? F("mDNS start STA IP=") : F("mDNS start AP IP="), staReady ? staIp : apIp);
#endif
        mdnsAttempted = true;
        mdnsLastAttemptAt = now;
        mdnsRunning = MDNS.begin(hostname);
        if (!mdnsRunning) {
          MDNS.close();
#ifdef DEBUG
          Serial.println(F("[WIFI] mDNS start failed"));
#endif
        }
      }
    }

    if (mdnsRunning) MDNS.update();
  }
} // namespace

namespace wifi_controller {
  namespace detail {
    void platformInitialize() {
      if (initialized) return;

      stationDisconnectedEventHandler =
        WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
          pushEvent(static_cast<uint16_t>(event.reason));
        });
      initialized = true;
    }

    void platformSetAutoReconnect(bool enabled) {
      WiFi.setAutoReconnect(enabled);
    }

    void platformSetMode(Mode mode) {
#ifdef DEBUG
      Serial.print(F("[WIFI] mode "));
      Serial.println(modeName(mode));
#endif
      WiFi.mode(toEspMode(mode));
    }

    bool platformSetStaHostname(const char* hostname) {
      (void)hostname;
      return true;
    }

    bool platformBeginSta(const char* ssid, const char* password, const char* hostname) {
      const bool hostnameSet = hostname == nullptr || hostname[0] == '\0' || WiFi.hostname(hostname);
      WiFi.begin(ssid, password);
      return hostnameSet;
    }

    void platformDisconnectSta() {
      WiFi.disconnect(false, false);
    }

    StaLinkStatus platformStaLinkStatus() {
      if (WiFi.status() == WL_CONNECTED) return StaLinkStatus::Connected;
      if (WiFi.status() == WL_IDLE_STATUS) return StaLinkStatus::Connecting;
      return StaLinkStatus::Disconnected;
    }

    void platformTickNetworkServices(const char* hostname) {
      tickCaptiveDns();
      tickMdns(hostname);
    }

    bool platformStartAp(const char* ssid, const char* password, const WifiController::Ipv4Address& ipAddress) {
      const IPAddress address = toIpAddress(ipAddress);
      if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0))) {
#ifdef DEBUG
        Serial.println(F("[WIFI] AP config failed"));
#endif
        return false;
      }
      apRunning = WiFi.softAP(ssid, password);
#ifdef DEBUG
      if (apRunning) {
        logIp(F("AP started IP="), WiFi.softAPIP());
      } else {
        Serial.println(F("[WIFI] AP start failed"));
      }
#endif
      return apRunning;
    }

    void platformStopAp() {
      stopCaptiveDns();
      WiFi.softAPdisconnect(true);
      apRunning = false;
    }

    bool platformSetApHostname(const char* hostname) {
      (void)hostname;
      return true;
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

#endif
