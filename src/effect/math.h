#pragma once

#include <Arduino.h>
#include <FastLED.h>

uint8_t mapsin8(uint8_t theta, uint8_t lowest = 0, uint8_t highest = 255);
uint8_t mapcos8(uint8_t theta, uint8_t lowest = 0, uint8_t highest = 255);
uint8_t beatcos8(accum88 beats_per_minute, uint8_t lowest, uint8_t highest, uint32_t timebase = 0, uint8_t phase_offset = 0);

// Преобразует фазу 0..7 в значение волны 0..255 по типу синусоиды.
// Период повторяется каждые 8 значений.
uint8_t scaleToWave8(uint8_t x); // aka myScale8

// неточный, зато более быстрый квадратный корень
static inline float sqrt3(const float x) {
  uint32_t i;
  std::memcpy(&i, &x, sizeof(i));
  i = (1UL << 29) + (i >> 1) - (1UL << 22);
  float result;
  std::memcpy(&result, &i, sizeof(result));
  return result;
}

//аналог ардуино функции map(), но только для float
static inline float fmap(
  const float x,
  const float in_min,
  const float in_max,
  const float out_min,
  const float out_max
) {
  return (out_max - out_min) * (x - in_min) / (in_max - in_min) + out_min;
}

static inline float mapcurve(
  const float x,
  const float in_min,
  const float in_max,
  const float out_min,
  const float out_max,
  float (*curve)(float, float, float, float)
) {
  if (x <= in_min) return out_min;
  if (x >= in_max) return out_max;
  return curve((x - in_min), out_min, (out_max - out_min), (in_max - in_min));
}

static inline float InQuad(float t, float b, float c, float d) {
  t /= d;
  return c * t * t + b;
}

static inline float OutQuart(float t, float b, float c, float d) {
  t = t / d - 1;
  return -c * (t * t * t * t - 1) + b;
}

static inline float InOutQuad(float t, float b, float c, float d) {
  t /= d / 2;
  if (t < 1) return c / 2 * t * t + b;
  --t;
  return -c / 2 * (t * (t - 2) - 1) + b;
}

// template <class T>
// class Vector2 {
// public:
//   T x, y;

//   Vector2() : x(0), y(0) {}
//   Vector2(T x, T y) : x(x), y(y) {}
//   Vector2(const Vector2& v) : x(v.x), y(v.y) {}

//   Vector2& operator=(const Vector2& v) { x = v.x; y = v.y; return *this; }

//   bool isEmpty() { return x == 0 && y == 0; }

//   bool operator==(Vector2& v) { return x == v.x && y == v.y; }
//   bool operator!=(Vector2& v) { return !(x == y); }

//   Vector2 operator+(Vector2& v) { return Vector2(x + v.x, y + v.y); }
//   Vector2 operator-(Vector2& v) { return Vector2(x - v.x, y - v.y); }

//   Vector2& operator+=(Vector2& v) { x += v.x; y += v.y; return *this; }
//   Vector2& operator-=(Vector2& v) { x -= v.x; y -= v.y; return *this; }

//   Vector2 operator+(double s) { return Vector2(x + s, y + s); }
//   Vector2 operator-(double s) { return Vector2(x - s, y - s); }
//   Vector2 operator*(double s) { return Vector2(x * s, y * s); }
//   Vector2 operator/(double s) { return Vector2(x / s, y / s); }

//   Vector2& operator+=(double s) { x += s; y += s; return *this; }
//   Vector2& operator-=(double s) { x -= s; y -= s; return *this; }
//   Vector2& operator*=(double s) { x *= s; y *= s; return *this; }
//   Vector2& operator/=(double s) { x /= s; y /= s; return *this; }

//   void set(T x, T y) {
//     this->x = x;
//     this->y = y;
//   }

//   void rotate(double deg) {
//     double theta = deg / 180.0 * M_PI;
//     double c = cos(theta);
//     double s = sin(theta);
//     double tx = x * c - y * s;
//     double ty = x * s + y * c;
//     x = tx;
//     y = ty;
//   }

//   Vector2& normalize() {
//     if (length() == 0) return *this;
//     *this *= (1.0 / length());
//     return *this;
//   }

//   float dist(Vector2 v) const {
//     Vector2 d(v.x - x, v.y - y);
//     return d.length();
//   }
//   float length() const { return sqrt(x * x + y * y); }
//   float mag() const { return length(); }
//   float magSq() { return (x * x + y * y); }

//   void truncate(double length) {
//     double angle = atan2f(y, x);
//     x = length * cos(angle);
//     y = length * sin(angle);
//   }

//   Vector2 ortho() const { return Vector2(y, -x); }

//   static float dot(Vector2 v1, Vector2 v2) { return v1.x * v2.x + v1.y * v2.y; }
//   static float cross(Vector2 v1, Vector2 v2) { return (v1.x * v2.y) - (v1.y * v2.x); }

//   void limit(float max) {
//     if (magSq() > max * max) {
//       normalize();
//       *this *= max;
//     }
//   }
// };

// typedef Vector2<float> PVector;
