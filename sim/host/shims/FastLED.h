#pragma once

#include "Arduino.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// Forward declarations
struct CRGB;
struct CHSV;
struct CRGBPalette16;

using fract8 = uint8_t;
using fract16 = uint16_t;
using accum88 = uint16_t;
using TProgmemRGBPalette16 = const uint8_t;

namespace fl {

  class XYMap {
  public:
    using Func = uint16_t (*)(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

    static XYMap constructWithUserFunction(uint16_t width, uint16_t height, Func func) {
      XYMap m;
      m.w_ = width;
      m.h_ = height;
      m.func_ = func;
      return m;
    }

    uint16_t operator()(uint16_t x, uint16_t y) const {
      return func_ ? func_(x, y, w_, h_) : static_cast<uint16_t>(y * w_ + x);
    }

    uint16_t width() const { return w_; }
    uint16_t height() const { return h_; }

  private:
    Func func_ = nullptr;
    uint16_t w_ = 0;
    uint16_t h_ = 0;
  };

} // namespace fl

// ---------------------------------------------------------------------------
// Low-level math helpers (needed by CRGB)
// ---------------------------------------------------------------------------

inline uint8_t scale8(uint8_t i, uint8_t scale) {
  return static_cast<uint8_t>((static_cast<uint16_t>(i) * (1U + static_cast<uint16_t>(scale))) >> 8);
}

inline uint8_t scale8_video(uint8_t i, uint8_t scale) {
  return static_cast<uint8_t>(((static_cast<uint16_t>(i) * scale) >> 8) + (i && scale ? 1 : 0));
}

inline uint8_t qadd8(uint8_t a, uint8_t b) {
  uint16_t r = static_cast<uint16_t>(a) + b;
  return r > 255 ? 255 : static_cast<uint8_t>(r);
}

inline uint8_t qsub8(uint8_t a, uint8_t b) {
  return a > b ? a - b : 0;
}

inline uint16_t scale16(uint16_t i, uint16_t scale) {
  return static_cast<uint16_t>((static_cast<uint32_t>(i) * (1U + static_cast<uint32_t>(scale))) >> 16);
}

inline uint8_t sqrt16(uint16_t x);

// ---------------------------------------------------------------------------
// CRGB
// ---------------------------------------------------------------------------

struct CRGB {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  CRGB() = default;
  constexpr CRGB(uint8_t r_, uint8_t g_, uint8_t b_)
    : r(r_),
      g(g_),
      b(b_) {}
  constexpr CRGB(uint32_t colorcode)
    : r(static_cast<uint8_t>((colorcode >> 16) & 0xFF)),
      g(static_cast<uint8_t>((colorcode >> 8) & 0xFF)),
      b(static_cast<uint8_t>(colorcode & 0xFF)) {}

  static const CRGB Black;
  static const CRGB White;
  static const CRGB Red;
  static const CRGB Green;
  static const CRGB Blue;

  bool operator==(const CRGB& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const CRGB& o) const { return !(*this == o); }

  bool operator==(uint32_t c) const { return pack() == (c & 0xFFFFFFU); }
  bool operator!=(uint32_t c) const { return !(*this == c); }
  bool operator<(const CRGB& o) const { return pack() < o.pack(); }
  bool operator>(const CRGB& o) const { return pack() > o.pack(); }
  bool operator<(uint32_t c) const { return pack() < (c & 0xFFFFFFU); }
  bool operator>(uint32_t c) const { return pack() > (c & 0xFFFFFFU); }

  CRGB& operator+=(const CRGB& o) {
    r = qadd8(r, o.r);
    g = qadd8(g, o.g);
    b = qadd8(b, o.b);
    return *this;
  }
  CRGB operator+(const CRGB& o) const {
    CRGB t = *this;
    return t += o;
  }

  CRGB& operator-=(const CRGB& o) {
    r = qsub8(r, o.r);
    g = qsub8(g, o.g);
    b = qsub8(b, o.b);
    return *this;
  }
  CRGB operator-(const CRGB& o) const {
    CRGB t = *this;
    return t -= o;
  }

  CRGB& operator-=(uint32_t c) {
    r = qsub8(r, static_cast<uint8_t>((c >> 16) & 0xFF));
    g = qsub8(g, static_cast<uint8_t>((c >> 8) & 0xFF));
    b = qsub8(b, static_cast<uint8_t>(c & 0xFF));
    return *this;
  }
  CRGB operator-(uint32_t c) const {
    CRGB t = *this;
    return t -= c;
  }

  CRGB operator*(uint8_t s) const { return CRGB(scale8(r, s), scale8(g, s), scale8(b, s)); }
  CRGB operator*(int s) const { return operator*(static_cast<uint8_t>(s)); }
  CRGB operator*(bool s) const { return s ? *this : Black; }

  CRGB& operator*=(uint8_t s) {
    r = scale8(r, s);
    g = scale8(g, s);
    b = scale8(b, s);
    return *this;
  }

  CRGB& nscale8(uint8_t s) {
    r = scale8(r, s);
    g = scale8(g, s);
    b = scale8(b, s);
    return *this;
  }

  void fadeToBlackBy(uint8_t step) {
    r = r > step ? r - step : 0;
    g = g > step ? g - step : 0;
    b = b > step ? b - step : 0;
  }

  uint8_t getAverageLight() const { return static_cast<uint8_t>((static_cast<uint16_t>(r) + g + b) / 3); }

  constexpr uint32_t pack() const {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
  }
};

inline const CRGB CRGB::Black = CRGB(0, 0, 0);
inline const CRGB CRGB::White = CRGB(255, 255, 255);
inline const CRGB CRGB::Red = CRGB(255, 0, 0);
inline const CRGB CRGB::Green = CRGB(0, 255, 0);
inline const CRGB CRGB::Blue = CRGB(0, 0, 255);

// ---------------------------------------------------------------------------
// HSV
// ---------------------------------------------------------------------------

CRGB hsv2rgb_rainbow(const CHSV& hsv);
CHSV rgb2hsv_approximate(const CRGB& rgb);
CRGB hsv2rgb_spectrum(const CHSV& hsv);

struct CHSV {
  union {
    uint8_t h = 0;
    uint8_t hue;
  };
  union {
    uint8_t s = 255;
    uint8_t sat;
  };
  union {
    uint8_t v = 255;
    uint8_t val;
  };
  CHSV() = default;
  constexpr CHSV(uint8_t h_, uint8_t s_, uint8_t v_)
    : h(h_),
      s(s_),
      v(v_) {}
  operator CRGB() const { return hsv2rgb_rainbow(*this); }
};

inline CRGB hsv2rgb_rainbow(const CHSV& hsv) {
  const uint8_t hue = hsv.h;
  const uint8_t sat = hsv.s;
  uint8_t val = hsv.v;
  const uint8_t offset8 = static_cast<uint8_t>((hue & 0x1f) << 3);
  const uint8_t third = scale8(offset8, 256 / 3);
  uint8_t r, g, b;

  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 255 - third; g = third; b = 0; }
      else { r = 171; g = 85 + third; b = 0; }
    } else {
      if (!(hue & 0x20)) { r = 171 - scale8(offset8, (256 * 2) / 3); g = 170 + third; b = 0; }
      else { r = 0; g = 255 - third; b = third; }
    }
  } else if (!(hue & 0x40)) {
    if (!(hue & 0x20)) { r = 0; g = 171 - scale8(offset8, (256 * 2) / 3); b = 85 + scale8(offset8, (256 * 2) / 3); }
    else { r = third; g = 0; b = 255 - third; }
  } else if (!(hue & 0x20)) {
    r = 85 + third; g = 0; b = 171 - third;
  } else {
    r = 170 + third; g = 0; b = 85 - third;
  }

  if (sat != 255) {
    if (sat == 0) {
      r = 255; g = 255; b = 255;
    } else {
      uint8_t desat = scale8_video(255 - sat, 255 - sat);
      const uint8_t satscale = 255 - desat;
      r = scale8(r, satscale);
      g = scale8(g, satscale);
      b = scale8(b, satscale);
      r += desat; g += desat; b += desat;
    }
  }
  if (val != 255) {
    val = scale8_video(val, val);
    if (val == 0) r = g = b = 0;
    else { r = scale8(r, val); g = scale8(g, val); b = scale8(b, val); }
  }
  return CRGB(r, g, b);
}

inline CRGB hsv2rgb_spectrum(const CHSV& hsv) {
  const uint8_t hue = scale8(hsv.h, 191);
  const uint8_t invsat = 255 - hsv.s;
  const uint8_t brightnessFloor = static_cast<uint8_t>((static_cast<uint16_t>(hsv.v) * invsat) / 256);
  const uint8_t amplitude = hsv.v - brightnessFloor;
  const uint8_t offset = hue & 0x3f;
  const uint8_t rampup = static_cast<uint8_t>((static_cast<uint16_t>(offset) * amplitude) / 64);
  const uint8_t rampdown = static_cast<uint8_t>((static_cast<uint16_t>(63 - offset) * amplitude) / 64);
  if (hue / 0x40 == 0) return CRGB(rampdown + brightnessFloor, rampup + brightnessFloor, brightnessFloor);
  if (hue / 0x40 == 1) return CRGB(brightnessFloor, rampdown + brightnessFloor, rampup + brightnessFloor);
  return CRGB(rampup + brightnessFloor, brightnessFloor, rampdown + brightnessFloor);
}

inline void hsv2rgb_spectrum(const CHSV& hsv, CRGB& rgb) {
  rgb = hsv2rgb_spectrum(hsv);
}

inline CHSV rgb2hsv_approximate(const CRGB& rgb) {
  uint8_t r = rgb.r, g = rgb.g, b = rgb.b;
  uint8_t desat = std::min(r, std::min(g, b));
  r -= desat; g -= desat; b -= desat;
  uint8_t s = 255 - desat;
  if (s != 255) s = 255 - sqrt16(static_cast<uint16_t>((255 - s) * 256));
  if ((static_cast<uint16_t>(r) + g + b) == 0) return CHSV(0, 0, 255 - s);
  if (s < 255) {
    if (s == 0) s = 1;
    const uint32_t scaleup = 65535 / s;
    r = static_cast<uint8_t>((static_cast<uint32_t>(r) * scaleup) / 256);
    g = static_cast<uint8_t>((static_cast<uint32_t>(g) * scaleup) / 256);
    b = static_cast<uint8_t>((static_cast<uint32_t>(b) * scaleup) / 256);
  }
  const uint16_t total = r + g + b;
  if (total < 255) {
    const uint32_t scaleup = 65535 / (total ? total : 1);
    r = static_cast<uint8_t>((static_cast<uint32_t>(r) * scaleup) / 256);
    g = static_cast<uint8_t>((static_cast<uint32_t>(g) * scaleup) / 256);
    b = static_cast<uint8_t>((static_cast<uint32_t>(b) * scaleup) / 256);
  }
  uint8_t v = total > 255 ? 255 : qadd8(desat, total);
  if (v != 255) v = sqrt16(static_cast<uint16_t>(v * 256));
  uint8_t h;
  const uint8_t highest = std::max(r, std::max(g, b));
  if (highest == r) {
    if (g == 0) h = 208 + scale8(qsub8(r, 128), (48 * 256) / 128);
    else if ((r - g) > g) h = scale8(g, (32 * 256) / 85);
    else h = 32 + scale8(qsub8((g - 85) + (255 - r), 4), (32 * 256) / 170);
  } else if (highest == g) {
    if (b == 0) h = 64 + (scale8(qsub8(171, r), 47) + scale8(qsub8(g, 171), 96)) / 2;
    else if ((g - b) > b) h = 96 + scale8(b, (32 * 256) / 85);
    else h = 128 + scale8(qsub8(b, 85), (8 * 256) / 42);
  } else {
    if (r == 0) h = 136 + scale8(qsub8(b, 128), (24 * 256) / 128);
    else if ((b - r) > r) h = 160 + scale8(r, (32 * 256) / 85);
    else h = 192 + scale8(qsub8(r, 85), (32 * 256) / 85);
  }
  return CHSV(static_cast<uint8_t>(h + 1), s, v);
}

// ---------------------------------------------------------------------------
// Blend
// ---------------------------------------------------------------------------

inline uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
  uint16_t partial = static_cast<uint16_t>(a) << 8;
  partial += static_cast<uint16_t>(b) * amountOfB;
  partial -= static_cast<uint16_t>(a) * amountOfB;
  partial += 0x80;
  return static_cast<uint8_t>(partial >> 8);
}

inline CRGB& nblend(CRGB& a, const CRGB& b, uint8_t amount) {
  if (amount == 0) return a;
  if (amount == 255) {
    a = b;
    return a;
  }

  a.r = blend8(a.r, b.r, amount);
  a.g = blend8(a.g, b.g, amount);
  a.b = blend8(a.b, b.b, amount);
  return a;
}

inline CRGB blend(const CRGB& a, const CRGB& b, uint8_t amount) {
  CRGB result(a);
  nblend(result, b, amount);
  return result;
}

// ---------------------------------------------------------------------------
// CRGBPalette16
// ---------------------------------------------------------------------------

struct CRGBPalette16 {
  CRGB entries[16];

  CRGBPalette16() = default;
  CRGBPalette16(const CRGBPalette16&) = default;
  CRGBPalette16& operator=(const CRGBPalette16&) = default;

  explicit CRGBPalette16(const CRGB* src) {
    for (int i = 0; i < 16; ++i)
      entries[i] = src[i];
  }

  explicit CRGBPalette16(const uint8_t* gradient);

  CRGBPalette16(const CRGB& c1, const CRGB& c2) {
    for (int i = 0; i < 16; ++i) {
      uint8_t t = static_cast<uint8_t>((i * 255) / 15);
      entries[i] = blend(c1, c2, t);
    }
  }

  CRGBPalette16(const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4) {
    for (int i = 0; i < 16; ++i) {
      uint8_t t = static_cast<uint8_t>((i * 255) / 15);
      if (t < 85) {
        entries[i] = blend(c1, c2, static_cast<uint8_t>(t * 3));
      } else if (t < 170) {
        entries[i] = blend(c2, c3, static_cast<uint8_t>((t - 85) * 3));
      } else {
        entries[i] = blend(c3, c4, static_cast<uint8_t>((t - 170) * 3));
      }
    }
  }

  CRGBPalette16(const CHSV& c1, const CHSV& c2)
    : CRGBPalette16(hsv2rgb_rainbow(c1), hsv2rgb_rainbow(c2)) {}
};

inline CRGBPalette16::CRGBPalette16(const uint8_t* gradient) {
  for (int i = 0; i < 16; ++i)
    entries[i] = CRGB::Black;
  if (!gradient) return;

  uint8_t last_index = 0;
  CRGB last_color = CRGB::Black;
  bool first = true;

  for (;;) {
    uint8_t index = pgm_read_byte(gradient++);
    uint8_t r = pgm_read_byte(gradient++);
    uint8_t g = pgm_read_byte(gradient++);
    uint8_t b = pgm_read_byte(gradient++);
    CRGB color(r, g, b);

    if (!first) {
      uint8_t dist = (index >= last_index) ? (index - last_index) : 0;
      if (dist == 0) {
        entries[(last_index >> 4) & 0x0F] = color;
      } else {
        for (uint16_t v = last_index; v <= index; ++v) {
          uint8_t slot = static_cast<uint8_t>(v >> 4) & 0x0F;
          uint8_t amount = static_cast<uint8_t>(((v - last_index) * 255) / dist);
          entries[slot] = blend(last_color, color, amount);
        }
      }
    } else {
      entries[0] = color;
      first = false;
    }

    last_index = index;
    last_color = color;
    if (index == 255) break;
  }
}

enum TBlendType { NOBLEND = 0, LINEARBLEND = 1, LINEARBLEND_NOWRAP = 2 };

inline uint8_t map8(uint8_t x, uint8_t out_min, uint8_t out_max);

inline CRGB
ColorFromPalette(const CRGBPalette16& pal, uint8_t index, uint8_t brightness = 255, uint8_t blendType = LINEARBLEND) {
  if (blendType == LINEARBLEND_NOWRAP) {
    index = map8(index, 0, 239);
  }

  const uint8_t hi4 = index >> 4;
  const uint8_t lo4 = index & 0x0F;
  const CRGB& a = pal.entries[hi4];
  CRGB e = a;
  if (blendType != 0 && lo4 != 0) {
    const CRGB& b = pal.entries[hi4 == 15 ? 0 : hi4 + 1];
    const uint8_t f2 = lo4 << 4;
    const uint8_t f1 = 255 - f2;
    e.r = static_cast<uint8_t>(scale8(a.r, f1) + scale8(b.r, f2));
    e.g = static_cast<uint8_t>(scale8(a.g, f1) + scale8(b.g, f2));
    e.b = static_cast<uint8_t>(scale8(a.b, f1) + scale8(b.b, f2));
  }
  if (brightness == 255) return e;
  if (brightness == 0) return CRGB::Black;
  ++brightness;
  e.r = scale8(e.r, brightness);
  e.g = scale8(e.g, brightness);
  e.b = scale8(e.b, brightness);
  return e;
}

// ---------------------------------------------------------------------------
// Fill helpers
// ---------------------------------------------------------------------------

enum TGradientDirectionCode { FORWARD_HUES, BACKWARD_HUES, SHORTEST_HUES, LONGEST_HUES };

inline void fill_solid(CRGB* leds, int numLeds, const CRGB& color) {
  for (int i = 0; i < numLeds; ++i)
    leds[i] = color;
}

inline void fill_gradient_RGB(CRGB* leds, uint16_t startpos, CRGB startcolor, uint16_t endpos, CRGB endcolor) {
  if (endpos < startpos) {
    uint16_t t = endpos;
    CRGB tc = endcolor;
    endcolor = startcolor;
    endpos = startpos;
    startpos = t;
    startcolor = tc;
  }

  int16_t rdistance87;
  int16_t gdistance87;
  int16_t bdistance87;

  rdistance87 = (endcolor.r - startcolor.r) << 7;
  gdistance87 = (endcolor.g - startcolor.g) << 7;
  bdistance87 = (endcolor.b - startcolor.b) << 7;

  uint16_t pixeldistance = endpos - startpos;
  int16_t divisor = pixeldistance ? pixeldistance : 1;

  int16_t rdelta87 = rdistance87 / divisor;
  int16_t gdelta87 = gdistance87 / divisor;
  int16_t bdelta87 = bdistance87 / divisor;

  rdelta87 *= 2;
  gdelta87 *= 2;
  bdelta87 *= 2;

  accum88 r88 = startcolor.r << 8;
  accum88 g88 = startcolor.g << 8;
  accum88 b88 = startcolor.b << 8;
  for (uint8_t i = startpos; i <= endpos; ++i) {
    leds[i] = CRGB(r88 >> 8, g88 >> 8, b88 >> 8);
    r88 += rdelta87;
    g88 += gdelta87;
    b88 += bdelta87;
  }
}

inline void fill_gradient(
  CRGB* leds,
  int index1,
  const CHSV& c1,
  int index2,
  const CHSV& c2,
  TGradientDirectionCode /*direction*/ = SHORTEST_HUES
) {
  fill_gradient_RGB(leds, index1, hsv2rgb_rainbow(c1), index2, hsv2rgb_rainbow(c2));
}

inline void fill_rainbow(CRGB* leds, int numLeds, uint8_t initialHue, uint8_t deltaHue = 5) {
  for (int i = 0; i < numLeds; ++i) {
    leds[i] = hsv2rgb_rainbow(CHSV(initialHue, 255, 255));
    initialHue += deltaHue;
  }
}

// ---------------------------------------------------------------------------
// More math helpers
// ---------------------------------------------------------------------------

inline uint8_t ease8InOutQuad(uint8_t t) {
  uint8_t j = t;
  if (j & 0x80) j = 255 - j;
  uint8_t jj = scale8(j, j);
  uint8_t jj2 = jj << 1;
  if (t & 0x80) jj2 = 255 - jj2;
  return jj2;
}

inline uint8_t ease8InOutApprox(uint8_t t) {
  if (t < 64) {
    t /= 2;
  } else if (t > (255 - 64)) {
    t = 255 - t;
    t /= 2;
    t = 255 - t;
  } else {
    t -= 64;
    t += t / 2;
    t += 32;
  }
  return t;
}

inline uint8_t lerp8by8(uint8_t a, uint8_t b, uint8_t amount) {
  return a + (((static_cast<int16_t>(b) - a) * amount) >> 8);
}

inline uint16_t lerp16by16(uint16_t a, uint16_t b, uint16_t amount) {
  return a + ((static_cast<uint32_t>(b - a) * amount) >> 16);
}

inline uint8_t dim8_raw(uint8_t x) {
  return x >> 1;
}
inline uint8_t brighten8_raw(uint8_t x) {
  uint16_t r = x + (x >> 1);
  return r > 255 ? 255 : static_cast<uint8_t>(r);
}

inline uint8_t sqrt16(uint16_t x) {
  if (x <= 1) return static_cast<uint8_t>(x);
  uint8_t low = 0;
  uint8_t hi = x > 7904 ? 255 : static_cast<uint8_t>((x >> 5) + 8);
  do {
    const uint8_t mid = static_cast<uint8_t>((low + hi) >> 1);
    if (static_cast<uint16_t>(mid * mid) > x) hi = mid - 1;
    else {
      if (mid == 255) return 255;
      low = mid + 1;
    }
  } while (hi >= low);
  return low - 1;
}

inline uint8_t square8(uint8_t x) {
  return static_cast<uint8_t>((static_cast<uint16_t>(x) * x) >> 8);
}

inline uint8_t triwave8(uint8_t x) {
  uint8_t y = x << 1;
  if (y < 255) return y;
  return 510 - y;
}

inline uint8_t cubicwave8(uint8_t x) {
  return scale8(triwave8(x), triwave8(x + 64));
}

inline uint8_t quadwave8(uint8_t x) {
  return scale8(cubicwave8(x), triwave8(x + 64));
}

inline void nscale8(CRGB* leds, int numLeds, uint8_t scale) {
  for (int i = 0; i < numLeds; ++i) {
    leds[i].r = scale8(leds[i].r, scale);
    leds[i].g = scale8(leds[i].g, scale);
    leds[i].b = scale8(leds[i].b, scale);
  }
}

inline void nscale8_video(CRGB* leds, int numLeds, uint8_t scale) {
  for (int i = 0; i < numLeds; ++i) {
    leds[i].r = scale8_video(leds[i].r, scale);
    leds[i].g = scale8_video(leds[i].g, scale);
    leds[i].b = scale8_video(leds[i].b, scale);
  }
}

inline void fadeToBlackBy(CRGB* leds, int numLeds, uint8_t step) {
  for (int i = 0; i < numLeds; ++i)
    leds[i].fadeToBlackBy(step);
}

inline void fadeLightBy(CRGB* leds, int numLeds, uint8_t step) {
  fadeToBlackBy(leds, numLeds, step);
}

// ---------------------------------------------------------------------------
// Trig / beat helpers
// ---------------------------------------------------------------------------

inline uint8_t sin8(uint8_t theta) {
  static constexpr uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
  uint8_t offset = theta;
  if (theta & 0x40) {
    offset = static_cast<uint8_t>(255 - offset);
  }
  offset &= 0x3F;

  uint8_t secoffset = offset & 0x0F;
  if (theta & 0x40) {
    ++secoffset;
  }

  const uint8_t section = offset >> 4;
  const uint8_t b = b_m16_interleave[section * 2];
  const uint8_t m16 = b_m16_interleave[section * 2 + 1];
  const uint8_t mx = static_cast<uint8_t>((m16 * secoffset) >> 4);
  int8_t y = static_cast<int8_t>(mx + b);
  if (theta & 0x80) {
    y = -y;
  }
  return static_cast<uint8_t>(y + 128);
}

inline uint8_t cos8(uint8_t theta) {
  return sin8(theta + 64);
}

inline int16_t sin16(uint16_t theta) {
  static constexpr uint16_t base[] = {0, 6393, 12539, 18204, 23170, 27245, 30273, 32137};
  static constexpr uint8_t slope[] = {49, 48, 44, 38, 31, 23, 14, 4};

  uint16_t offset = (theta & 0x3FFFU) >> 3;
  if (theta & 0x4000U) {
    offset = 2047 - offset;
  }

  const uint8_t section = offset / 256;
  const uint8_t secoffset8 = static_cast<uint8_t>(offset) / 2;
  const uint16_t mx = static_cast<uint16_t>(slope[section]) * secoffset8;
  int16_t y = static_cast<int16_t>(mx + base[section]);
  if (theta & 0x8000U) {
    y = -y;
  }
  return y;
}

inline int16_t cos16(uint16_t theta) {
  return sin16(theta + 16384);
}

inline uint16_t beat88(accum88 beats_per_minute_88, uint32_t timebase = 0) {
  return static_cast<uint16_t>(((sim_millis - timebase) * static_cast<uint32_t>(beats_per_minute_88) * 280U) >> 16);
}

inline uint16_t beat16(accum88 beats_per_minute, uint32_t timebase = 0) {
  if (beats_per_minute < 256) {
    beats_per_minute <<= 8;
  }
  return beat88(beats_per_minute, timebase);
}

inline uint8_t beat8(accum88 beats_per_minute, uint32_t timebase = 0) {
  return static_cast<uint8_t>(beat16(beats_per_minute, timebase) >> 8);
}

inline uint8_t beatsin8(
  accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255, uint32_t timebase = 0, uint8_t phase_offset = 0
) {
  uint8_t beat = beat8(beats_per_minute, timebase);
  const uint8_t sine = sin8(beat + phase_offset);
  const uint8_t range = highest - lowest;
  uint8_t result = lowest + scale8(sine, range);
  if (result > highest) {
    result = highest;
  }
  return result;
}

inline uint16_t beatsin16(
  accum88 beats_per_minute,
  uint16_t lowest = 0,
  uint16_t highest = 65535,
  uint32_t timebase = 0,
  uint16_t phase_offset = 0
) {
  const uint16_t beat = beat16(beats_per_minute, timebase);
  const uint16_t sine = static_cast<uint16_t>(static_cast<int32_t>(sin16(beat + phase_offset)) + 32768);
  const uint16_t range = highest - lowest;
  uint16_t result = lowest + scale16(sine, range);
  if (result > highest) {
    result = highest;
  }
  return result;
}

inline uint16_t beatsin88(
  accum88 beats_per_minute,
  uint16_t lowest = 0,
  uint16_t highest = 65535,
  uint32_t timebase = 0,
  uint16_t phase_offset = 0
) {
  const uint16_t beat = beat88(beats_per_minute, timebase);
  const uint16_t sine = static_cast<uint16_t>(static_cast<int32_t>(sin16(beat + phase_offset)) + 32768);
  const uint16_t range = highest - lowest;
  uint16_t result = lowest + scale16(sine, range);
  if (result > highest) {
    result = highest;
  }
  return result;
}

inline uint8_t map8(uint8_t x, uint8_t out_min, uint8_t out_max) {
  return out_min + scale8(x, out_max - out_min);
}

// ---------------------------------------------------------------------------
// Noise
// ---------------------------------------------------------------------------

namespace detail {

  inline uint8_t noise_hash(uint8_t x, uint8_t y, uint8_t z) {
    static const uint8_t perm[256] = {
      151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142,
      8,   99,  37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203,
      117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165,
      71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,
      55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,
      18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250,
      124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189,
      28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,
      129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,
      242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,
      181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114,
      67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180
    };
    return perm[(perm[(perm[x] + y) & 0xFF] + z) & 0xFF];
  }

  inline uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t) {
    return a + (((static_cast<int16_t>(b) - a) * t) >> 8);
  }

  inline uint8_t noise_p(uint8_t index) {
    static constexpr uint8_t p[] = {
      151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
      247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
      74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
      65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,
      217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,
      248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,
      246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,
      84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
    return p[index];
  }

  inline int8_t avg7(int8_t a, int8_t b) {
    return static_cast<int8_t>((a >> 1) + (b >> 1) + (a & 1));
  }

  inline int8_t noise_grad8(uint8_t hash, int8_t x, int8_t y, int8_t z) {
    switch (hash & 0x0f) {
      case 0: return avg7(x, y); case 1: return avg7(-x, y); case 2: return avg7(x, -y); case 3: return avg7(-x, -y);
      case 4: return avg7(x, z); case 5: return avg7(-x, z); case 6: return avg7(x, -z); case 7: return avg7(-x, -z);
      case 8: return avg7(y, z); case 9: return avg7(-y, z); case 10: return avg7(y, -z); case 11: return avg7(-y, -z);
      case 12: return avg7(x, y); case 13: return avg7(-x, y); case 14: return avg7(x, -y); default: return avg7(-x, -y);
    }
  }

  inline int8_t noise_grad8(uint8_t hash, int8_t x, int8_t y) {
    int8_t u = (hash & 4) ? y : x;
    int8_t v = (hash & 4) ? x : y;
    if (hash & 1) u = -u;
    if (hash & 2) v = -v;
    return avg7(u, v);
  }

  inline int8_t noise_lerp7by8(int8_t a, int8_t b, uint8_t fraction) {
    if (b > a) return static_cast<int8_t>(a + scale8(static_cast<uint8_t>(b - a), fraction));
    return static_cast<int8_t>(a - scale8(static_cast<uint8_t>(a - b), fraction));
  }

} // namespace detail

inline uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z) {
  const uint8_t X = x >> 8, Y = y >> 8, Z = z >> 8;
  const uint8_t A = detail::noise_p(X) + Y, AA = detail::noise_p(A) + Z, AB = detail::noise_p(A + 1) + Z;
  const uint8_t B = detail::noise_p(X + 1) + Y, BA = detail::noise_p(B) + Z, BB = detail::noise_p(B + 1) + Z;
  uint8_t u = ease8InOutQuad(static_cast<uint8_t>(x)), v = ease8InOutQuad(static_cast<uint8_t>(y)), w = ease8InOutQuad(static_cast<uint8_t>(z));
  const int8_t xx = (static_cast<uint8_t>(x) >> 1) & 0x7f, yy = (static_cast<uint8_t>(y) >> 1) & 0x7f, zz = (static_cast<uint8_t>(z) >> 1) & 0x7f;
  constexpr uint8_t N = 0x80;
  const int8_t X1 = detail::noise_lerp7by8(detail::noise_grad8(detail::noise_p(AA), xx, yy, zz), detail::noise_grad8(detail::noise_p(BA), xx - N, yy, zz), u);
  const int8_t X2 = detail::noise_lerp7by8(detail::noise_grad8(detail::noise_p(AB), xx, yy - N, zz), detail::noise_grad8(detail::noise_p(BB), xx - N, yy - N, zz), u);
  const int8_t X3 = detail::noise_lerp7by8(detail::noise_grad8(detail::noise_p(AA + 1), xx, yy, zz - N), detail::noise_grad8(detail::noise_p(BA + 1), xx - N, yy, zz - N), u);
  const int8_t X4 = detail::noise_lerp7by8(detail::noise_grad8(detail::noise_p(AB + 1), xx, yy - N, zz - N), detail::noise_grad8(detail::noise_p(BB + 1), xx - N, yy - N, zz - N), u);
  const int8_t result = detail::noise_lerp7by8(detail::noise_lerp7by8(X1, X2, v), detail::noise_lerp7by8(X3, X4, v), w);
  return qadd8(static_cast<uint8_t>(result + 64), static_cast<uint8_t>(result + 64));
}

inline uint8_t inoise8(uint16_t x, uint16_t y) {
  const uint8_t X = x >> 8, Y = y >> 8;
  const uint8_t A = detail::noise_p(X) + Y, AA = detail::noise_p(A), AB = detail::noise_p(A + 1);
  const uint8_t B = detail::noise_p(X + 1) + Y, BA = detail::noise_p(B), BB = detail::noise_p(B + 1);
  const uint8_t u = ease8InOutQuad(static_cast<uint8_t>(x)), v = ease8InOutQuad(static_cast<uint8_t>(y));
  const int8_t xx = (static_cast<uint8_t>(x) >> 1) & 0x7f, yy = (static_cast<uint8_t>(y) >> 1) & 0x7f;
  constexpr uint8_t N = 0x80;
  const int8_t X1 = detail::noise_lerp7by8(detail::noise_grad8(detail::noise_p(AA), xx, yy), detail::noise_grad8(detail::noise_p(BA), xx - N, yy), u);
  const int8_t X2 = detail::noise_lerp7by8(detail::noise_grad8(detail::noise_p(AB), xx, yy - N), detail::noise_grad8(detail::noise_p(BB), xx - N, yy - N), u);
  const int8_t result = detail::noise_lerp7by8(X1, X2, v);
  return qadd8(static_cast<uint8_t>(result + 64), static_cast<uint8_t>(result + 64));
}

// Known non-parity placeholder. Phase 4A covers only inoise8 2D/3D overloads.
inline uint16_t inoise16(uint16_t x, uint16_t y = 0, uint16_t z = 0) {
  return static_cast<uint16_t>(inoise8(x, y, z)) * 257U;
}

// ---------------------------------------------------------------------------
// Blur
// ---------------------------------------------------------------------------

inline void blur1d(CRGB* leds, int numLeds, fract8 amount) {
  const uint8_t keep = 255 - amount;
  const uint8_t seep = amount >> 1;
  CRGB carryover = CRGB::Black;
  for (int i = 0; i < numLeds; ++i) {
    CRGB cur = leds[i];
    CRGB part = cur;
    part.nscale8(seep);
    cur.nscale8(keep);
    cur += carryover;
    if (i) leds[i - 1] += part;
    leds[i] = cur;
    carryover = part;
  }
}

inline void blur2d(CRGB* leds, uint8_t width, uint8_t height, fract8 amount, const fl::XYMap& xyMap) {
  const uint8_t keep = 255 - amount;
  const uint8_t seep = amount >> 1;
  for (uint8_t row = 0; row < height; ++row) {
    CRGB carryover = CRGB::Black;
    for (uint8_t col = 0; col < width; ++col) {
      const uint16_t index = xyMap(col, row);
      CRGB cur = leds[index];
      CRGB part = cur;
      part.nscale8(seep);
      cur.nscale8(keep);
      cur += carryover;
      if (col) leds[xyMap(col - 1, row)] += part;
      leds[index] = cur;
      carryover = part;
    }
  }
  for (uint8_t col = 0; col < width; ++col) {
    CRGB carryover = CRGB::Black;
    for (uint8_t row = 0; row < height; ++row) {
      const uint16_t index = xyMap(col, row);
      CRGB cur = leds[index];
      CRGB part = cur;
      part.nscale8(seep);
      cur.nscale8(keep);
      cur += carryover;
      if (row) leds[xyMap(col, row - 1)] += part;
      leds[index] = cur;
      carryover = part;
    }
  }
}

// ---------------------------------------------------------------------------
// Color order enum placeholder
// ---------------------------------------------------------------------------

enum EOrder { RGB = 0, RBG = 1, GRB = 2, GBR = 3, BRG = 4, BGR = 5 };
// ---------------------------------------------------------------------------
// FastLED singleton stubs
// ---------------------------------------------------------------------------

class CFastLED {
public:
  template <typename LED_TYPE, uint8_t DATA_PIN, EOrder RGB_ORDER = RGB> struct CLEDController {};

  template <typename LED_TYPE, uint8_t DATA_PIN, EOrder RGB_ORDER = RGB>
  CLEDController<LED_TYPE, DATA_PIN, RGB_ORDER>& addLeds(CRGB* /*data*/, int /*nLeds*/) {
    static CLEDController<LED_TYPE, DATA_PIN, RGB_ORDER> controller;
    return controller;
  }

  static void show(uint8_t = 255) {}
  static void clear(bool = true) {}
  static void setBrightness(uint8_t) {}
  static void setMaxPowerInVoltsAndMilliamps(uint8_t, uint16_t) {}
};

extern CFastLED FastLED;
inline CFastLED FastLED;

// ---------------------------------------------------------------------------
// Built-in palettes
// ---------------------------------------------------------------------------

namespace {

  inline CRGBPalette16 make_rainbow_palette() {
    // FastLED's built-in RainbowColors_p values. Do not derive this from the
    // host HSV shim: Rainbow effects use this palette directly, so any HSV
    // approximation error would distort the baseline simulator colors.
    CRGB entries[16] = {
      CRGB(0xFF0000),
      CRGB(0xD52A00),
      CRGB(0xAB5500),
      CRGB(0xAB7F00),
      CRGB(0xABAB00),
      CRGB(0x56D500),
      CRGB(0x00FF00),
      CRGB(0x00D52A),
      CRGB(0x00AB55),
      CRGB(0x0056AA),
      CRGB(0x0000FF),
      CRGB(0x2A00D5),
      CRGB(0x5500AB),
      CRGB(0x7F0081),
      CRGB(0xAB0055),
      CRGB(0xD5002B)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_party_palette() {
    CRGB entries[16] = {
      CRGB(0x5500AB),
      CRGB(0x84007C),
      CRGB(0xB5004B),
      CRGB(0xE4001B),
      CRGB(0xF91700),
      CRGB(0xF96400),
      CRGB(0xF9B200),
      CRGB(0xF9FF00),
      CRGB(0xB2FF00),
      CRGB(0x64FF00),
      CRGB(0x17FF00),
      CRGB(0x00FF1B),
      CRGB(0x00FF68),
      CRGB(0x00FFB6),
      CRGB(0x00FFFF),
      CRGB(0x00ABFF)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_ocean_palette() {
    CRGB entries[16] = {
      CRGB(0x000050),
      CRGB(0x000060),
      CRGB(0x000070),
      CRGB(0x000080),
      CRGB(0x000090),
      CRGB(0x0000A0),
      CRGB(0x0000B0),
      CRGB(0x0000C0),
      CRGB(0x0000D0),
      CRGB(0x0000E0),
      CRGB(0x0000F0),
      CRGB(0x0000FF),
      CRGB(0x0030FF),
      CRGB(0x0060FF),
      CRGB(0x0090FF),
      CRGB(0x00C0FF)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_forest_palette() {
    CRGB entries[16] = {
      CRGB(0x001100),
      CRGB(0x002200),
      CRGB(0x003300),
      CRGB(0x004400),
      CRGB(0x005500),
      CRGB(0x006600),
      CRGB(0x007700),
      CRGB(0x008800),
      CRGB(0x009900),
      CRGB(0x22AA00),
      CRGB(0x44BB00),
      CRGB(0x66CC00),
      CRGB(0x88DD00),
      CRGB(0xAAEE00),
      CRGB(0xCCFF00),
      CRGB(0xEEFF44)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_clouds_palette() {
    CRGB entries[16] = {
      CRGB(0x444444),
      CRGB(0x555555),
      CRGB(0x666666),
      CRGB(0x777777),
      CRGB(0x888888),
      CRGB(0x999999),
      CRGB(0xAAAAAA),
      CRGB(0xBBBBBB),
      CRGB(0xCCCCCC),
      CRGB(0xDDDDDD),
      CRGB(0xDDEEEE),
      CRGB(0xDDFFFF),
      CRGB(0xEEEEFF),
      CRGB(0xFFFFFF),
      CRGB(0xFFFFFF),
      CRGB(0xFFFFFF)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_lava_palette() {
    CRGB entries[16] = {
      CRGB(0x000000),
      CRGB(0x1F0000),
      CRGB(0x3F0000),
      CRGB(0x5F0000),
      CRGB(0x7F0000),
      CRGB(0x9F0000),
      CRGB(0xBF0000),
      CRGB(0xDF0000),
      CRGB(0xFF0000),
      CRGB(0xFF1F00),
      CRGB(0xFF3F00),
      CRGB(0xFF5F00),
      CRGB(0xFF7F00),
      CRGB(0xFF9F00),
      CRGB(0xFFBF00),
      CRGB(0xFFDF00)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_heat_palette() {
    CRGB entries[16] = {
      CRGB(0x000000),
      CRGB(0x330000),
      CRGB(0x660000),
      CRGB(0x990000),
      CRGB(0xCC0000),
      CRGB(0xFF0000),
      CRGB(0xFF3300),
      CRGB(0xFF6600),
      CRGB(0xFF9900),
      CRGB(0xFFCC00),
      CRGB(0xFFFF00),
      CRGB(0xFFFF33),
      CRGB(0xFFFF66),
      CRGB(0xFFFF99),
      CRGB(0xFFFFCC),
      CRGB(0xFFFFFF)
    };
    return CRGBPalette16(entries);
  }

  inline CRGBPalette16 make_rainbow_stripe_palette() {
    CRGB entries[16];
    for (int i = 0; i < 16; ++i) {
      uint8_t h = static_cast<uint8_t>(i * 16);
      entries[i] = (i % 2 == 0) ? hsv2rgb_rainbow(CHSV(h, 255, 255)) : CRGB::Black;
    }
    return CRGBPalette16(entries);
  }

} // namespace

#define RainbowColors_p (make_rainbow_palette())
#define PartyColors_p (make_party_palette())
#define OceanColors_p (make_ocean_palette())
#define ForestColors_p (make_forest_palette())
#define CloudColors_p (make_clouds_palette())
#define LavaColors_p (make_lava_palette())
#define HeatColors_p (make_heat_palette())
#define RainbowStripeColors_p (make_rainbow_stripe_palette())

// ---------------------------------------------------------------------------
// Gradient palette macros
// ---------------------------------------------------------------------------

#define DECLARE_GRADIENT_PALETTE(name) extern const uint8_t name[] PROGMEM
#define DEFINE_GRADIENT_PALETTE(name) const uint8_t name[] PROGMEM =
