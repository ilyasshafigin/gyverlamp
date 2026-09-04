#pragma once

#include <stdint.h>

enum class MqttState : uint8_t {
  Disabled,
  WaitingForWifi,
  DisconnectBarrier,
  RetryWait,
  ConnectPrepare,
  Connecting,
  Online,
  ConfigError,
};
