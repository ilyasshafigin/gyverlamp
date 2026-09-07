#include "device.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_arduino_version.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

namespace {

  Device::Metric available(uint32_t value) {
    return {value, true};
  }

#if defined(ARDUINO_ARCH_ESP32)
  const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
      case ESP_RST_POWERON: return "Power-on reset";
      case ESP_RST_EXT: return "External reset";
      case ESP_RST_SW: return "Software reset";
      case ESP_RST_PANIC: return "Panic reset";
      case ESP_RST_INT_WDT: return "Interrupt watchdog";
      case ESP_RST_TASK_WDT: return "Task watchdog";
      case ESP_RST_WDT: return "Other watchdog";
      case ESP_RST_DEEPSLEEP: return "Deep sleep reset";
      case ESP_RST_BROWNOUT: return "Brownout reset";
      case ESP_RST_SDIO: return "SDIO reset";
      case ESP_RST_UNKNOWN: return "Unknown reset";
      default: return "Unknown reset";
    }
  }
#endif

} // namespace

namespace Device {

  Diagnostics diagnostics() {
    Diagnostics result;

#if defined(ARDUINO_ARCH_ESP8266)
    result.chipId = String(ESP.getChipId(), HEX);
    result.resetReason = ESP.getResetReason();
    result.coreVersion = ESP.getCoreVersion();
    const uint16_t vccMillivolts = ESP.getVcc();
    if (vccMillivolts >= 2500 && vccMillivolts <= 3700) result.vccMillivolts = available(vccMillivolts);
    result.cpuFrequencyMhz = available(ESP.getCpuFreqMHz());
    result.sketchSizeBytes = available(ESP.getSketchSize());
    result.flashSizeBytes = available(ESP.getFlashChipSize());
    result.freeSketchSpaceBytes = available(ESP.getFreeSketchSpace());
    result.freeHeapBytes = available(ESP.getFreeHeap());
    result.maxFreeBlockBytes = available(ESP.getMaxFreeBlockSize());
    result.heapFragmentationPercent = available(ESP.getHeapFragmentation());
#elif defined(ARDUINO_ARCH_ESP32)
    const uint64_t chipId = ESP.getEfuseMac();
    char chipIdBuffer[13];
    snprintf(
      chipIdBuffer, sizeof(chipIdBuffer), "%04X%08X", static_cast<uint16_t>(chipId >> 32), static_cast<uint32_t>(chipId)
    );
    result.chipId = chipIdBuffer;
    result.resetReason = resetReasonName(esp_reset_reason());
    result.coreVersion = String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." +
                         String(ESP_ARDUINO_VERSION_PATCH);
    result.cpuFrequencyMhz = available(ESP.getCpuFreqMHz());
    result.sketchSizeBytes = available(ESP.getSketchSize());
    result.flashSizeBytes = available(ESP.getFlashChipSize());
    result.freeSketchSpaceBytes = available(ESP.getFreeSketchSpace());
    result.freeHeapBytes = available(ESP.getFreeHeap());
    result.maxFreeBlockBytes = available(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif

    return result;
  }

  String metricText(const Metric& metric) {
    return metric.available ? String(metric.value) : String(kUnavailable);
  }

  void restart() {
    ESP.restart();
  }

} // namespace Device
