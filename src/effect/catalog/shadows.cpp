#include "shadows.h"

// Effect: Shadows
// Authors: vvip-68
// Based on:
//   - vvip-68/LedPanelWiFi
//     https://github.com/vvip-68/LedPanelWiFi/blob/2f86a32a925a995e8fe13c3a52b8f1ac0ec94a96/firmware/LedPanelWiFi_v1.14/effects.ino#L1927
//   - MishanyaTS/FieryLedLamp
//     https://github.com/MishanyaTS/FieryLedLamp/blob/56130dcd9b0059355016c0f476891b68c73f3298/FieryLedLamp/FieryLedLamp/effects.ino#L5870
//   - Whilser/GyverLamp
//     https://github.com/Whilser/GyverLamp/blob/e7545c18f75911df6fc017d3e18c52cdd994cfc9/firmware/GyverLamp_v1.6_MQTT/pride.ino#L1

static uint16_t sPseudotime = 0;
static uint16_t sHue16 = 0;

void EffectShadows::render(EffectContext& ctx) {
  uint8_t sat8 = beatsin88(87, 220, 250);
  uint8_t brightdepth = beatsin88(341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
  uint8_t msmultiplier =
    beatsin88(map(ctx.speed, 1, 255, 100, 255), 32, map(ctx.speed, 1, 255, 60, 255)); //beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16; //gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  sPseudotime += ctx.deltaMs * msmultiplier;
  sHue16 += ctx.deltaMs * beatsin88(400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16 += brightnessthetainc16;
    uint16_t b16 = sin16(brightnesstheta16) + 32768;

    uint16_t bri16 = static_cast<uint32_t>(static_cast<uint32_t>(b16) * static_cast<uint32_t>(b16)) / 65536;
    uint8_t bri8 = static_cast<uint32_t>(static_cast<uint32_t>(bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor;
    if (ctx.palette) {
      newcolor = ColorFromPalette(
        *ctx.palette, hue8, map8(bri8, map(ctx.scale, 32, 255, 32, 125), map(ctx.scale, 32, 255, 125, 250))
      );
    } else {
      newcolor = CHSV(hue8, sat8, map8(bri8, map(ctx.scale, 32, 255, 32, 125), map(ctx.scale, 32, 255, 125, 250)));
    }

    uint8_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend(ctx.led.getLed(pixelnumber), newcolor, 64);
  }
}
