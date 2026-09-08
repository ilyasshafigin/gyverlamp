#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fastled_parity {

inline std::string base64(const std::vector<uint8_t>& data) {
  static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;
  encoded.reserve((data.size() + 2) / 3 * 4);
  for (size_t i = 0; i < data.size(); i += 3) {
    const uint32_t group = static_cast<uint32_t>(data[i]) << 16 |
                           (i + 1 < data.size() ? static_cast<uint32_t>(data[i + 1]) << 8 : 0) |
                           (i + 2 < data.size() ? data[i + 2] : 0);
    encoded += kAlphabet[(group >> 18) & 0x3f];
    encoded += kAlphabet[(group >> 12) & 0x3f];
    encoded += i + 1 < data.size() ? kAlphabet[(group >> 6) & 0x3f] : '=';
    encoded += i + 2 < data.size() ? kAlphabet[group & 0x3f] : '=';
  }
  return encoded;
}

inline uint64_t fnv1a(uint64_t hash, uint8_t value) {
  return (hash ^ value) * 1099511628211ULL;
}

inline std::string hex64(uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}

inline void append(std::vector<uint8_t>& out, const CRGB& color) {
  out.push_back(color.r);
  out.push_back(color.g);
  out.push_back(color.b);
}

inline void append16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value));
  out.push_back(static_cast<uint8_t>(value >> 8));
}

void fastled_parity_set_millis(uint32_t value);
void fastled_parity_blur1d(CRGB* leds, uint16_t count, uint8_t amount);
using ParityXYFunction = uint16_t (*)(uint16_t, uint16_t, uint16_t, uint16_t);
void fastled_parity_blur2d(CRGB* leds, uint8_t width, uint8_t height, uint8_t amount, ParityXYFunction map);
uint16_t fastled_parity_map_to_index(uint8_t width, uint8_t height, ParityXYFunction map, uint16_t x, uint16_t y);

inline uint16_t paritySerpentine(uint16_t x, uint16_t y, uint16_t width, uint16_t) {
  return static_cast<uint16_t>(y * width + ((y & 1) ? width - 1 - x : x));
}

inline uint16_t parityRectangular(uint16_t x, uint16_t y, uint16_t width, uint16_t) {
  return static_cast<uint16_t>(y * width + x);
}

inline int run() {
  std::vector<uint8_t> scale;
  scale.reserve(256 * 256 * 2);
  for (uint16_t input = 0; input < 256; ++input) {
    for (uint16_t factor = 0; factor < 256; ++factor) {
      scale.push_back(scale8(static_cast<uint8_t>(input), static_cast<uint8_t>(factor)));
      scale.push_back(scale8_video(static_cast<uint8_t>(input), static_cast<uint8_t>(factor)));
    }
  }

  std::vector<uint8_t> trig;
  trig.reserve(256 * 3);
  for (uint16_t theta = 0; theta < 256; ++theta) {
    trig.push_back(sin8(static_cast<uint8_t>(theta)));
    trig.push_back(cos8(static_cast<uint8_t>(theta)));
    trig.push_back(ease8InOutApprox(static_cast<uint8_t>(theta)));
  }

  std::vector<uint8_t> trig16;
  trig16.reserve(65536 * 4);
  for (uint32_t theta = 0; theta < 65536; ++theta) {
    append16(trig16, static_cast<uint16_t>(sin16(static_cast<uint16_t>(theta))));
    append16(trig16, static_cast<uint16_t>(cos16(static_cast<uint16_t>(theta))));
  }

  const uint16_t scale16Factors[] = {0, 1, 2, 255, 256, 257, 0x7fff, 0x8000, 0xff00, 0xfffe, 0xffff};
  uint64_t scale16Digest = 14695981039346656037ULL;
  for (uint32_t input = 0; input < 65536; ++input) {
    for (uint16_t factor : scale16Factors) {
      const uint16_t value = scale16(static_cast<uint16_t>(input), factor);
      scale16Digest = fnv1a(scale16Digest, static_cast<uint8_t>(value));
      scale16Digest = fnv1a(scale16Digest, static_cast<uint8_t>(value >> 8));
    }
  }

  const uint16_t trig16BoundaryInputs[] = {0, 1, 7, 8, 0x1fff, 0x2000, 0x3fff, 0x4000, 0x7fff, 0x8000, 0xbfff, 0xc000, 0xffff};
  std::vector<uint8_t> trig16Boundaries;
  for (uint16_t theta : trig16BoundaryInputs) {
    append16(trig16Boundaries, static_cast<uint16_t>(sin16(theta)));
    append16(trig16Boundaries, static_cast<uint16_t>(cos16(theta)));
  }

  const uint16_t scale16BoundaryInputs[] = {0, 1, 255, 256, 0x7fff, 0x8000, 0xfffe, 0xffff};
  std::vector<uint8_t> scale16Boundaries;
  for (uint16_t input : scale16BoundaryInputs) {
    for (uint16_t factor : scale16Factors) append16(scale16Boundaries, scale16(input, factor));
  }

  struct BeatCase {
    accum88 bpm;
    uint32_t timebase;
    uint16_t lowest16;
    uint16_t highest16;
    uint16_t phase16;
    uint8_t lowest8;
    uint8_t highest8;
    uint8_t phase8;
  };
  const BeatCase beatCases[] = {
    {1, 0, 0, 65535, 0, 0, 255, 0}, {60, 0, 100, 900, 0x1234, 10, 200, 0x12},
    {120, 1, 0, 1, 0x4000, 0, 1, 0x40}, {255, 999, 1234, 5678, 0x8000, 25, 26, 0x80},
    {256, 0xffffff00U, 0, 65535, 0xffff, 0, 255, 0xff}, {0x0180, 0x80000000U, 40000, 50000, 0x0020, 100, 200, 0x20},
    {0x1234, 0xffffffffU, 32767, 32768, 0xc000, 127, 128, 0xc0}, {0xffff, 60000, 65534, 65535, 0x7fff, 254, 255, 0x7f},
  };
  const uint32_t beatTimes[] = {0, 1, 999, 1000, 59999, 60000, 0x7fffffffU, 0xffffff00U, 0xfffffffeU, 0xffffffffU};
  std::vector<uint8_t> beatVectors;
  for (uint32_t now : beatTimes) {
    fastled_parity_set_millis(now);
    for (const BeatCase& test : beatCases) {
      append16(beatVectors, beat88(test.bpm, test.timebase));
      append16(beatVectors, beat16(test.bpm, test.timebase));
      beatVectors.push_back(beat8(test.bpm, test.timebase));
      append16(beatVectors, beatsin88(test.bpm, test.lowest16, test.highest16, test.timebase, test.phase16));
      append16(beatVectors, beatsin16(test.bpm, test.lowest16, test.highest16, test.timebase, test.phase16));
      beatVectors.push_back(beatsin8(test.bpm, test.lowest8, test.highest8, test.timebase, test.phase8));
    }
  }

  uint64_t blendDigest = 14695981039346656037ULL;
  for (uint16_t a = 0; a < 256; ++a) {
    for (uint16_t b = 0; b < 256; ++b) {
      for (uint16_t amount = 0; amount < 256; ++amount) {
        CRGB color(static_cast<uint8_t>(a), static_cast<uint8_t>(b), static_cast<uint8_t>(a ^ b));
        nblend(color, CRGB(static_cast<uint8_t>(b), static_cast<uint8_t>(a), static_cast<uint8_t>(a + b)), amount);
        blendDigest = fnv1a(blendDigest, color.r);
        blendDigest = fnv1a(blendDigest, color.g);
        blendDigest = fnv1a(blendDigest, color.b);
      }
    }
  }

  const uint8_t amounts[] = {0, 1, 127, 128, 254, 255};
  const CRGB starts[] = {CRGB(0, 0, 0), CRGB(255, 255, 255), CRGB(1, 254, 17), CRGB(254, 1, 238)};
  const CRGB overlays[] = {CRGB(255, 255, 255), CRGB(0, 0, 0), CRGB(254, 1, 238), CRGB(1, 254, 17)};
  std::vector<uint8_t> blendBoundaries;
  for (const CRGB& start : starts) {
    for (const CRGB& overlay : overlays) {
      for (uint8_t amount : amounts) {
        CRGB color(start);
        nblend(color, overlay, amount);
        append(blendBoundaries, color);
      }
    }
  }

  CRGB entries[16];
  for (uint8_t i = 0; i < 16; ++i) {
    entries[i] = CRGB(static_cast<uint8_t>(i * 17), static_cast<uint8_t>(255 - i * 13), static_cast<uint8_t>(i * 29));
  }
  const CRGBPalette16 palette(entries);
  const uint8_t brightnesses[] = {0, 1, 26, 128, 210, 254, 255};
  const TBlendType blends[] = {NOBLEND, LINEARBLEND, LINEARBLEND_NOWRAP};
  std::vector<uint8_t> paletteVectors;
  paletteVectors.reserve(256 * 7 * 3 * 3);
  for (uint16_t index = 0; index < 256; ++index) {
    for (TBlendType blendType : blends) {
      for (uint8_t brightness : brightnesses) {
        append(paletteVectors, ColorFromPalette(palette, static_cast<uint8_t>(index), brightness, blendType));
      }
    }
  }

  const CHSV hsvInputs[] = {CHSV(0, 0, 0), CHSV(0, 0, 255), CHSV(0, 255, 0), CHSV(0, 255, 255)};
  std::vector<uint8_t> hsv;
  hsv.reserve(sizeof(hsvInputs) / sizeof(hsvInputs[0]) * 3);
  for (const CHSV& input : hsvInputs) {
    append(hsv, hsv2rgb_rainbow(input));
  }

  uint64_t rainbowDigest = 14695981039346656037ULL;
  uint64_t spectrumDigest = 14695981039346656037ULL;
  for (uint16_t hue = 0; hue < 256; ++hue) {
    for (uint16_t saturation = 0; saturation < 256; ++saturation) {
      for (uint16_t value = 0; value < 256; ++value) {
        const CHSV input(static_cast<uint8_t>(hue), static_cast<uint8_t>(saturation), static_cast<uint8_t>(value));
        const CRGB rainbow = hsv2rgb_rainbow(input);
        const CRGB spectrum = hsv2rgb_spectrum(input);
        rainbowDigest = fnv1a(fnv1a(fnv1a(rainbowDigest, rainbow.r), rainbow.g), rainbow.b);
        spectrumDigest = fnv1a(fnv1a(fnv1a(spectrumDigest, spectrum.r), spectrum.g), spectrum.b);
      }
    }
  }

  const uint8_t hsvBoundaryHues[] = {0, 1, 31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128, 129, 159, 160, 161, 191, 192, 193, 223, 224, 225, 254, 255};
  const uint8_t hsvBoundaryLevels[] = {0, 1, 254, 255};
  std::vector<uint8_t> hsvBoundaries;
  for (uint8_t hue : hsvBoundaryHues) {
    for (uint8_t saturation : hsvBoundaryLevels) {
      for (uint8_t value : hsvBoundaryLevels) {
        const CHSV input(hue, saturation, value);
        append(hsvBoundaries, hsv2rgb_rainbow(input));
        append(hsvBoundaries, hsv2rgb_spectrum(input));
      }
    }
  }

  uint64_t reverseDigest = 14695981039346656037ULL;
  for (uint16_t r = 0; r < 256; r += 17) {
    for (uint16_t g = 0; g < 256; g += 17) {
      for (uint16_t b = 0; b < 256; b += 17) {
        const CHSV converted = rgb2hsv_approximate(CRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)));
        reverseDigest = fnv1a(fnv1a(fnv1a(reverseDigest, converted.h), converted.s), converted.v);
      }
    }
  }
  uint32_t reverseState = 0x9e3779b9U;
  for (uint32_t index = 0; index < 65536; ++index) {
    reverseState = reverseState * 1664525U + 1013904223U;
    const CRGB input(static_cast<uint8_t>(reverseState), static_cast<uint8_t>(reverseState >> 8), static_cast<uint8_t>(reverseState >> 16));
    const CHSV converted = rgb2hsv_approximate(input);
    reverseDigest = fnv1a(fnv1a(fnv1a(reverseDigest, converted.h), converted.s), converted.v);
  }

  const CRGB reverseEdges[] = {
    CRGB(0, 0, 0), CRGB(1, 1, 1), CRGB(255, 255, 255), CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255),
    CRGB(255, 153, 0), CRGB(171, 255, 0), CRGB(0, 255, 85), CRGB(0, 171, 255), CRGB(85, 0, 255), CRGB(255, 0, 85),
    CRGB(252, 0, 126), CRGB(252, 252, 0), CRGB(252, 252, 126), CRGB(192, 64, 64), CRGB(224, 32, 32), CRGB(127, 128, 127),
  };
  std::vector<uint8_t> reverseBoundaries;
  for (const CRGB& input : reverseEdges) {
    const CHSV converted = rgb2hsv_approximate(input);
    reverseBoundaries.push_back(converted.h);
    reverseBoundaries.push_back(converted.s);
    reverseBoundaries.push_back(converted.v);
  }

  const uint8_t blurAmounts[] = {0, 1, 64, 172, 255};
  std::vector<uint8_t> blur1dVectors;
  for (uint16_t length = 1; length <= 8; ++length) {
    for (uint8_t amount : blurAmounts) {
      std::vector<CRGB> leds(length);
      for (uint16_t index = 0; index < length; ++index) leds[index] = CRGB(index * 37 + 1, 255 - index * 29, index * 53 + 7);
      fastled_parity_blur1d(leds.data(), length, amount);
      for (const CRGB& pixel : leds) append(blur1dVectors, pixel);
      fastled_parity_blur1d(leds.data(), length, amount);
      for (const CRGB& pixel : leds) append(blur1dVectors, pixel);
    }
  }

  struct BlurGridCase { uint8_t width; uint8_t height; uint8_t amount; uint8_t passes; bool serpentine; };
  const BlurGridCase blurGridCases[] = {
    {1, 1, 0, 1, false}, {1, 4, 172, 1, false}, {4, 1, 255, 2, false}, {2, 2, 64, 1, false},
    {3, 2, 172, 2, false}, {3, 3, 255, 1, false}, {4, 3, 64, 2, true}, {4, 3, 172, 1, true},
  };
  std::vector<uint8_t> blur2dVectors;
  for (const BlurGridCase& test : blurGridCases) {
    const ParityXYFunction map = test.serpentine ? paritySerpentine : parityRectangular;
    std::vector<CRGB> leds(test.width * test.height);
    for (uint8_t y = 0; y < test.height; ++y) {
      for (uint8_t x = 0; x < test.width; ++x) {
        leds[fastled_parity_map_to_index(test.width, test.height, map, x, y)] = CRGB(x * 71 + y * 11 + 3, x * 13 + y * 83 + 5, x * 47 + y * 31 + 7);
      }
    }
    for (uint8_t pass = 0; pass < test.passes; ++pass) fastled_parity_blur2d(leds.data(), test.width, test.height, test.amount, map);
    for (const CRGB& pixel : leds) append(blur2dVectors, pixel);
  }

  const uint16_t noiseBoundaries[] = {0, 1, 127, 128, 255, 256, 257, 0x7fff, 0x8000, 0xff00, 0xfffe, 0xffff};
  std::vector<uint8_t> noiseBoundaryVectors;
  for (uint16_t x : noiseBoundaries) {
    for (uint16_t y : noiseBoundaries) noiseBoundaryVectors.push_back(inoise8(x, y));
  }
  for (uint16_t x : noiseBoundaries) {
    for (uint16_t y : noiseBoundaries) {
      for (uint16_t z : noiseBoundaries) noiseBoundaryVectors.push_back(inoise8(x, y, z));
    }
  }
  uint64_t noiseDigest = 14695981039346656037ULL;
  uint32_t noiseState = 0x6d2b79f5U;
  for (uint32_t index = 0; index < 100000; ++index) {
    noiseState = noiseState * 1664525U + 1013904223U;
    const uint16_t x = noiseState;
    noiseState = noiseState * 1664525U + 1013904223U;
    const uint16_t y = noiseState;
    noiseDigest = fnv1a(noiseDigest, inoise8(x, y));
  }
  for (uint32_t index = 0; index < 100000; ++index) {
    const uint16_t x = static_cast<uint16_t>(index * 19U + 0x0101U);
    const uint16_t y = static_cast<uint16_t>(index * 37U + 0x2020U);
    const uint16_t z = static_cast<uint16_t>(index * 53U + 0x4040U);
    noiseDigest = fnv1a(noiseDigest, inoise8(x, y, z));
  }

  fastled_parity_set_millis(60000);
  std::cout << "{\"schema\":2,\"vectors\":{"
            << "\"scale\":\"" << base64(scale) << "\","
            << "\"trig\":\"" << base64(trig) << "\","
            << "\"trig16\":\"" << base64(trig16) << "\","
            << "\"trig16Boundaries\":\"" << base64(trig16Boundaries) << "\","
            << "\"scale16Digest\":\"" << hex64(scale16Digest) << "\","
            << "\"scale16Boundaries\":\"" << base64(scale16Boundaries) << "\","
            << "\"beat\":\"" << base64(beatVectors) << "\","
            << "\"blendDigest\":\"" << hex64(blendDigest) << "\","
            << "\"blendBoundaries\":\"" << base64(blendBoundaries) << "\","
            << "\"palette\":\"" << base64(paletteVectors) << "\","
            << "\"hsv\":\"" << base64(hsv) << "\","
            << "\"rainbowDigest\":\"" << hex64(rainbowDigest) << "\","
            << "\"spectrumDigest\":\"" << hex64(spectrumDigest) << "\","
            << "\"hsvBoundaries\":\"" << base64(hsvBoundaries) << "\","
            << "\"reverseDigest\":\"" << hex64(reverseDigest) << "\","
            << "\"reverseBoundaries\":\"" << base64(reverseBoundaries) << "\","
            << "\"blur1d\":\"" << base64(blur1dVectors) << "\","
            << "\"blur2d\":\"" << base64(blur2dVectors) << "\","
            << "\"noiseBoundaries\":\"" << base64(noiseBoundaryVectors) << "\","
            << "\"noiseDigest\":\"" << hex64(noiseDigest) << "\"},"
            << "\"sentinels\":{\"sin16_0\":" << sin16(0)
            << ",\"sin16_4000\":" << sin16(0x4000)
            << ",\"scale16_ffff_ffff\":" << scale16(0xffff, 0xffff)
            << ",\"beat88_q88_120_at_60000\":" << beat88(120 * 256, 0)
            << ",\"scale8_255_255\":" << unsigned(scale8(255, 255))
            << ",\"scale8_video_255_255\":" << unsigned(scale8_video(255, 255))
            << ",\"blend_black_white_128\":[" << unsigned(blend(CRGB::Black, CRGB::White, 128).r) << ","
            << unsigned(blend(CRGB::Black, CRGB::White, 128).g) << ","
            << unsigned(blend(CRGB::Black, CRGB::White, 128).b) << "]"
            << ",\"hsv_rainbow_h0_s0_v255\":[" << unsigned(hsv[3]) << "," << unsigned(hsv[4]) << ","
            << unsigned(hsv[5]) << "]}}\n";
  return 0;
}

} // namespace fastled_parity
