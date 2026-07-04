#pragma once

#include <Arduino.h>
#include "../audio/audio_frame.h"

#ifdef USE_ADC
#include "../util/fast_filter.h"
#include "../util/vol_analyzer.h"
#endif

class Microphone {
public:
  explicit Microphone()
#ifdef USE_ADC
    : _vol(A0) {
  }
#else
  {
  }
#endif

  void init();
  void tick();

  const AudioFrame& frame() const { return _frame; }

private:
#ifdef USE_ADC
  VolAnalyzer _vol, _low, _high;
  uint32_t _lastTickMs;
#endif
  AudioFrame _frame;
};
