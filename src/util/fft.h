#pragma once

#include <stdint.h>

// Based on https://github.com/AlexGyver/GyverLamp2/blob/main/firmware/GyverLamp2/FFT_C.h
// (c) AlexGyver

// размер выборки, степень 2
#define FFT_SIZE 64

// Q15 twiddle table: cos/sin для 64-point FFT, k = 0..31
static const int16_t FFT_COS_Q15[FFT_SIZE / 2] = {
  32767, 32610, 32138, 31357, 30273, 28898, 27245, 25329,
  23170, 20787, 18204, 15447, 12539,  9512,  6393,  3212,
      0, -3212, -6393, -9512,-12539,-15447,-18204,-20787,
 -23170,-25329,-27245,-28898,-30273,-31357,-32138,-32610
};

static const int16_t FFT_SIN_Q15[FFT_SIZE / 2] = {
      0, -3212, -6393, -9512,-12539,-15447,-18204,-20787,
 -23170,-25329,-27245,-28898,-30273,-31357,-32138,-32610,
 -32767,-32610,-32138,-31357,-30273,-28898,-27245,-25329,
 -23170,-20787,-18204,-15447,-12539, -9512, -6393, -3212
};

static inline int32_t mulQ15(int32_t a, int16_t b) {
  return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 15);
}

static inline void FFT(int32_t* AVal, uint32_t* FTvl) {
  int32_t real[FFT_SIZE];
  int32_t imag[FFT_SIZE];

  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    real[i] = AVal[i];
    imag[i] = 0;
  }

  // bit-reversal
  uint16_t j = 0;
  for (uint16_t i = 1; i < FFT_SIZE; i++) {
    uint16_t bit = FFT_SIZE >> 1;
    while (j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;

    if (i < j) {
      int32_t tr = real[i];
      real[i] = real[j];
      real[j] = tr;

      int32_t ti = imag[i];
      imag[i] = imag[j];
      imag[j] = ti;
    }
  }

  // radix-2 FFT
  for (uint16_t len = 2; len <= FFT_SIZE; len <<= 1) {
    const uint16_t halfLen = len >> 1;
    const uint16_t twStep = FFT_SIZE / len;

    for (uint16_t i = 0; i < FFT_SIZE; i += len) {
      for (uint16_t k = 0; k < halfLen; k++) {
        const uint16_t tw = k * twStep;
        const int16_t wr = FFT_COS_Q15[tw];
        const int16_t wi = FFT_SIN_Q15[tw];

        const uint16_t even = i + k;
        const uint16_t odd = even + halfLen;

        const int32_t oddR = real[odd];
        const int32_t oddI = imag[odd];

        const int32_t tr = mulQ15(oddR, wr) - mulQ15(oddI, wi);
        const int32_t ti = mulQ15(oddR, wi) + mulQ15(oddI, wr);

        const int32_t ur = real[even];
        const int32_t ui = imag[even];

        real[odd] = ur - tr;
        imag[odd] = ui - ti;
        real[even] = ur + tr;
        imag[even] = ui + ti;
      }
    }
  }

  // magnitude^2 + scale.
  // Старый код делал >> 18.
  // Для fixed-point масштаб может отличаться, поэтому это главный tuning knob.
  for (uint16_t i = 0; i < FFT_SIZE; i++) {
    const int32_t r = real[i];
    const int32_t im = imag[i];

    const uint64_t magSq = static_cast<uint64_t>(r) * r + static_cast<uint64_t>(im) * im;
    FTvl[i] = static_cast<uint32_t>(magSq >> 18);
  }
}
