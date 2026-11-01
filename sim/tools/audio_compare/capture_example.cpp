#include "effect/effect.h"
#include "effect/catalog/living_membrane.h"
#include "time/time_service.h"
#include "sim_random.h"
#include "audio_fixture.h"

#include <cstdio>
#include <cstring>

// This intentionally exercises only public production setup/render and the
// existing public simulator LED reader. Each lane runs in a fresh process.
int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--info") == 0) {
    std::printf("{\"width\":%u,\"height\":%u}", unsigned(WIDTH), unsigned(HEIGHT));
    return 0;
  }
  if (argc != 3 || (std::strcmp(argv[1], "off") && std::strcmp(argv[1], "on"))) {
    std::fprintf(stderr, "usage: capture off|on <output.rgb>\n");
    return 2;
  }
  FILE* output = std::fopen(argv[2], "wb");
  if (!output) {
    std::perror("open capture output");
    return 2;
  }
  sim_random_seed(12345);
  Led led;
  TimeService time;
  EffectLivingMembrane effect;
  CRGB colors[16];
  for (int i = 0; i < 16; i++)
    colors[i] = CHSV(i * 16, 240, 255);
  const CRGBPalette16 palette(colors);
  const AudioConfig config{std::strcmp(argv[1], "on") == 0 ? AudioMode::Effect : AudioMode::Off, AudioBand::Bass, 255};
  for (unsigned frame = 0; frame < kAudioFrameCount; frame++) {
    EffectContext ctx(
      255,
      128,
      160,
      0,
      0,
      0,
      frame * kAudioFrameMs,
      frame ? kAudioFrameMs : 0,
      &palette,
      kAudioFrames[frame],
      config,
      led,
      time
    );
    if (frame == 0) effect.setup(ctx);
    effect.render(ctx);
    // Top of the RGB image is the highest physical Y, matching prior captures.
    for (int y = HEIGHT - 1; y >= 0; y--) {
      for (int x = 0; x < WIDTH; x++) {
        const uint32_t color = led.pixelColor(x, y);
        const uint8_t pixel[] = {uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color)};
        if (std::fwrite(pixel, 1, sizeof(pixel), output) != sizeof(pixel)) {
          std::perror("write capture");
          std::fclose(output);
          return 2;
        }
      }
    }
  }
  if (std::fclose(output)) {
    std::perror("close capture");
    return 2;
  }
  return 0;
}
