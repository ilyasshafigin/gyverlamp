#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include "sim_audio_input.h"

using byte = uint8_t;
using word = uint16_t;
using boolean = bool;

extern uint32_t sim_millis;

// The host-emulated millis symbol is defined in sim/common/sim_time.cpp and
// shared by the WASM simulator runtime.
#define PROGMEM
#define PGM_P const char*
#define PSTR(x) (x)
#define pgm_read_byte(addr) (*(reinterpret_cast<const uint8_t*>(addr)))
#define pgm_read_word(addr) (*(reinterpret_cast<const uint16_t*>(addr)))
#define pgm_read_dword(addr) (*(reinterpret_cast<const uint32_t*>(addr)))
#define F(x) (x)

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559

inline uint32_t millis() {
  return sim_millis;
}
inline uint32_t micros() {
  return sim_millis * 1000;
}
inline void delay(uint32_t) {
}
inline void delayMicroseconds(uint32_t) {
}

constexpr uint8_t A0 = 0;
constexpr uint8_t INPUT = 0;

inline void pinMode(uint8_t, uint8_t) {
}
inline int analogRead(uint8_t pin) {
  return sim::audioReadAdc(pin);
}

// ---------------------------------------------------------------------------
// String
// ---------------------------------------------------------------------------

class String {
public:
  String() = default;
  String(const char* s)
    : data_(s ? s : "") {}
  String(const String&) = default;
  String& operator=(const String&) = default;

  bool equals(const String& s) const { return data_ == s.data_; }
  bool operator==(const String& s) const { return data_ == s.data_; }
  bool operator!=(const String& s) const { return data_ != s.data_; }

  char operator[](size_t i) const { return i < data_.size() ? data_[i] : '\0'; }
  size_t length() const { return data_.size(); }
  const char* c_str() const { return data_.c_str(); }
  bool isEmpty() const { return data_.empty(); }

  String& operator+=(const String& s) {
    data_ += s.data_;
    return *this;
  }
  String& operator+=(const char* s) {
    data_ += s;
    return *this;
  }
  String& operator+=(char c) {
    data_ += c;
    return *this;
  }

private:
  std::string data_;
};

inline String operator+(const String& a, const String& b) {
  String r = a;
  r += b;
  return r;
}

// ---------------------------------------------------------------------------
// Math / utility
// ---------------------------------------------------------------------------

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

template <typename T, typename U> inline auto min(const T& a, const U& b) -> decltype(a < b ? a : b) {
  return (a < b) ? a : b;
}

template <typename T, typename U> inline auto max(const T& a, const U& b) -> decltype(a > b ? a : b) {
  return (a > b) ? a : b;
}

#define highByte(x) static_cast<uint8_t>((x) >> 8)
#define lowByte(x) static_cast<uint8_t>((x) & 0xFF)

#define B00 0
#define B01 1
#define B10 2
#define B11 3

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------

namespace {
  inline uint32_t& rng_state() {
    static uint32_t state = 123456789;
    return state;
  }
} // namespace

inline void randomSeed(uint32_t seed) {
  rng_state() = seed ? seed : 123456789;
}

inline uint32_t random_uint32() {
  uint32_t x = rng_state();
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state() = x;
  return x;
}

inline int32_t random_int() {
  return static_cast<int32_t>(random_uint32());
}

inline long random(long max) {
  if (max <= 0) return 0;
  return static_cast<long>(random_uint32() % static_cast<uint32_t>(max));
}

inline long random(long min, long max) {
  if (max <= min) return min;
  return min + static_cast<long>(random_uint32() % static_cast<uint32_t>(max - min));
}

inline uint8_t random8() {
  return static_cast<uint8_t>(random_uint32());
}
inline uint8_t random8(uint8_t lim) {
  return lim ? static_cast<uint8_t>(random_uint32() % lim) : 0;
}
inline uint8_t random8(uint8_t min, uint8_t lim) {
  return (lim > min) ? static_cast<uint8_t>(min + (random_uint32() % (lim - min))) : min;
}
inline uint16_t random16() {
  return static_cast<uint16_t>(random_uint32());
}
inline uint16_t random16(uint16_t lim) {
  return lim ? static_cast<uint16_t>(random_uint32() % lim) : 0;
}

// ---------------------------------------------------------------------------
// Serial placeholder
// ---------------------------------------------------------------------------

class FakeSerial {
public:
  template <typename T> FakeSerial& print(T) { return *this; }
  template <typename T> FakeSerial& println(T) { return *this; }
  FakeSerial& println() { return *this; }
};
extern FakeSerial Serial;
inline FakeSerial Serial;
