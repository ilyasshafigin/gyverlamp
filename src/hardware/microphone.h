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
    : vol_(A0) {
  }
#else
  {
  }
#endif

  void init();
  void tick();

  const AudioFrame& frame() const { return frame_; }

private:
#ifdef USE_ADC
  VolAnalyzer vol_, low_, high_;
  uint32_t lastTickMs_;
#endif
  AudioFrame frame_;
};
