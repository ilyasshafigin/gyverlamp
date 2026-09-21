#pragma once

#include <stdint.h>

class WifiController {
public:
  struct Snapshot {
    int channel;
  };

  static inline bool connected = true;
  static inline int activeChannel = 6;

  bool staConnected() const { return connected; }
  Snapshot snapshot() const { return {activeChannel}; }
};
