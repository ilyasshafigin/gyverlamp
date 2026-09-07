#pragma once

#include <Arduino.h>

namespace Device {

  struct Metric {
    uint32_t value = 0;
    bool available = false;
  };

  struct Diagnostics {
    String chipId;
    String resetReason;
    String coreVersion;
    Metric vccMillivolts;
    Metric cpuFrequencyMhz;
    Metric sketchSizeBytes;
    Metric flashSizeBytes;
    Metric freeSketchSpaceBytes;
    Metric freeHeapBytes;
    Metric maxFreeBlockBytes;
    Metric heapFragmentationPercent;
  };

  constexpr const char* kUnavailable = "unavailable";

  Diagnostics diagnostics();
  String metricText(const Metric& metric);
  void restart();

} // namespace Device
