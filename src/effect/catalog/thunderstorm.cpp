#include "thunderstorm.h"
#include "../shared.h"

#include <cstring>

// Effect: Thunderstorm - ЭФФЕКТЫ ОСАДКИ / ТУЧКА В БАНКЕ / ГРОЗА В БАНКЕ
// Authors: marcmerlin
// Based on:
//   - marcmerlin/FastLED_NeoMatrix_SmartMatrix_LEDMatrix_GFX_Demos
//     https://github.com/marcmerlin/FastLED_NeoMatrix_SmartMatrix_LEDMatrix_GFX_Demos/blob/master/FastLED/Sublime_Demos/Sublime_Demos.ino
//   - Whilser/GyverLamp
//     https://github.com/Whilser/GyverLamp/blob/e7545c18f75911df6fc017d3e18c52cdd994cfc9/firmware/GyverLamp_v1.6_MQTT/effects.ino#L472
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L4420
//   - vvip-68/LedPanelWiFi
//     https://github.com/vvip-68/LedPanelWiFi/blob/2f86a32a925a995e8fe13c3a52b8f1ac0ec94a96/firmware/LedPanelWiFi_v1.14/effects.ino#L2372

CRGB solidRainColor = CRGB(60, 80, 90);

static constexpr uint8_t CLOUD_HEIGHT = (HEIGHT * 2U) / 5U + 1U;
static uint8_t cloudNoise[WIDTH * CLOUD_HEIGHT];

static uint8_t wrapX(int8_t x) {
  return (x + WIDTH) % WIDTH;
}

static uint8_t wrapY(int8_t y) {
  return (y + HEIGHT) % HEIGHT;
}

static void rain(
  Led& led,
  uint8_t backgroundDepth,
  uint8_t maxBrightness,
  uint8_t spawnFreq,
  uint8_t tailLength,
  bool splashes,
  bool clouds,
  bool storm,
  const CRGB& lightningColor,
  const CRGBPalette16& rainPalette,
  const CRGBPalette16& cloudsPalette
) {
  ff_x = random16();
  ff_y = random16();
  ff_z = random16();

  nscale8(led.getLedsBuff(), NUM_LEDS, tailLength);

  // Loop for each column individually
  for (uint8_t x = 0; x < WIDTH; x++) {
    // Step 1.  Move each dot down one cell
    for (uint8_t y = 0; y < HEIGHT; y++) {
      if (noise3d[0][x][y] >= backgroundDepth) { // Don't move empty cells
        if (y > 0) noise3d[0][x][wrapY(y - 1)] = noise3d[0][x][y];
        noise3d[0][x][y] = 0;
      }
    }

    // Step 2.  Randomly spawn new dots at top
    if (random8() < spawnFreq) {
      noise3d[0][x][HEIGHT - 1] = random(backgroundDepth, maxBrightness);
    }

    // Step 3. Map from tempMatrix cells to LED colors
    for (uint8_t y = 0; y < HEIGHT; y++) {
      if (noise3d[0][x][y] >= backgroundDepth) { // Don't write out empty cells
        led.setLedBuff(led.getPixelNumber(x, y), ColorFromPalette(rainPalette, noise3d[0][x][y]));
      }
    }

    // Step 4. Add splash if called for
    if (splashes) {
      // FIXME, this is broken
      uint8_t j = line[x];
      uint8_t v = noise3d[0][x][0];

      if (j >= backgroundDepth) {
        led.setLedBuff(led.getPixelNumber(wrapX(x - 2), 0), ColorFromPalette(rainPalette, j / 3));
        led.setLedBuff(led.getPixelNumber(wrapX(x + 2), 0), ColorFromPalette(rainPalette, j / 3));
        line[x] = 0; // Reset splash
      }

      if (v >= backgroundDepth) {
        led.setLedBuff(led.getPixelNumber(wrapX(x - 1), 1), ColorFromPalette(rainPalette, v / 2));
        led.setLedBuff(led.getPixelNumber(wrapX(x + 1), 1), ColorFromPalette(rainPalette, v / 2));
        line[x] = v; // Prep splash for next frame
      }
    }

    // Step 5. Add lightning if called for
    if (storm) {
      static uint8_t lightning[WIDTH * HEIGHT];
      memset(lightning, 0, sizeof(lightning));

      if (random16() < 72) {                                                  // Odds of a lightning bolt
        lightning[scale8(random8(), WIDTH - 1) + (HEIGHT - 1) * WIDTH] = 255; // Random starting location
        for (uint8_t ly = HEIGHT - 1; ly > 1; ly--) {
          for (uint8_t lx = 1; lx < WIDTH - 1; lx++) {
            if (lightning[lx + ly * WIDTH] == 255) {
              lightning[lx + ly * WIDTH] = 0;
              uint8_t dir = random8(4);
              switch (dir) {
                case 0:
                  led.setLedBuff(led.getPixelNumber(lx + 1, ly - 1), lightningColor);
                  lightning[(lx + 1) + (ly - 1) * WIDTH] = 255; // move down and right
                  break;
                case 1:
                  led.setLedBuff(
                    led.getPixelNumber(lx, ly - 1), CRGB(128, 128, 128)
                  ); // я без понятия, почему у верхней молнии один оттенок, а у остальных - другой
                  lightning[lx + (ly - 1) * WIDTH] = 255; // move down
                  break;
                case 2:
                  led.setLedBuff(led.getPixelNumber(lx - 1, ly - 1), CRGB(128, 128, 128));
                  lightning[(lx - 1) + (ly - 1) * WIDTH] = 255; // move down and left
                  break;
                case 3:
                  led.setLedBuff(led.getPixelNumber(lx - 1, ly - 1), CRGB(128, 128, 128));
                  lightning[(lx - 1) + (ly - 1) * WIDTH] = 255; // fork down and left
                  led.setLedBuff(led.getPixelNumber(lx - 1, ly - 1), CRGB(128, 128, 128));
                  lightning[(lx + 1) + (ly - 1) * WIDTH] = 255; // fork down and right
                  break;
              }
            }
          }
        }
      }
    }

    // Step 6. Add clouds if called for
    if (clouds) {
      uint16_t noiseScale =
        250; // A value of 1 will be so zoomed in, you'll mostly see solid colors. A value of 4011 will be very zoomed out and shimmery
      int xoffset = noiseScale * x + hue;

      for (uint8_t z = 0; z < CLOUD_HEIGHT; z++) {
        int yoffset = noiseScale * z - hue;
        uint8_t dataSmoothing = 192;
        uint8_t noiseData = qsub8(inoise8(ff_x + xoffset, ff_y + yoffset, ff_z), 16);
        noiseData = qadd8(noiseData, scale8(noiseData, 39));
        const uint16_t noiseIndex = x * CLOUD_HEIGHT + z;
        cloudNoise[noiseIndex] = scale8(cloudNoise[noiseIndex], dataSmoothing) + scale8(noiseData, 256 - dataSmoothing);
        nblend(
          led.getLedBuff(led.getPixelNumber(x, HEIGHT - z - 1)),
          ColorFromPalette(cloudsPalette, cloudNoise[noiseIndex]),
          (CLOUD_HEIGHT - z) * (250 / CLOUD_HEIGHT)
        );
      }
      ff_z++;
    }
  }
}

void EffectThunderstorm::setup(EffectContext& ctx) {
  ctx.led.clearLedsBuff();
  memset(noise3d[0], 0, sizeof(noise3d[0]));
  memset(line, 0, sizeof(line));
  memset(cloudNoise, 0, sizeof(cloudNoise));
  resetEveryMs(step, speedToIntervalMs(ctx.speed, 80U, 20U), ctx.nowMs);
}

void EffectThunderstorm::render(EffectContext& ctx) {
  // ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds, ligthening )
  //rain(0, 90, map8(intensity,0,150)+60, 10, solidRainColor, true, true, true);
  if (everyMs(step, speedToIntervalMs(ctx.speed, 80U, 20U), ctx.nowMs)) {
    CRGB lightningColor(72, 72, 80);
    CRGBPalette16 rainPalette = ctx.palette
                                  ? CRGBPalette16(CRGB::Black, ColorFromPalette(*ctx.palette, ctx.scale, 255U))
                                  : CRGBPalette16(CRGB::Black, solidRainColor);

    CRGBPalette16 cloudsPalette = ctx.palette
                                    ? CRGBPalette16(
                                        CRGB::Black,
                                        ColorFromPalette(*ctx.palette, ctx.scale + 32U, 80U),
                                        ColorFromPalette(*ctx.palette, ctx.scale + 64U, 45U),
                                        CRGB::Black
                                      )
                                    : CRGBPalette16(CRGB::Black, CRGB(15, 24, 24), CRGB(9, 15, 15), CRGB::Black);

    rain(ctx.led, 60, 160, ctx.scale, 30, true, true, true, lightningColor, rainPalette, cloudsPalette);
  }

  ctx.led.copyLedsBuffToLeds();
}
