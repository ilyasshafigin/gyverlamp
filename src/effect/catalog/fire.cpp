#include "fire.h"
#include "../shared.h"

// Effect: Fire - Пламя
// Authors: SottNick
// Based on:
//   - Андрей Локтев
//     https://goldenandy.blogspot.com/2021/05/ws2812.html

// характеристики языков пламени
//  x, dx; => trackingObjectPosX, trackingObjectSpeedX;
//  y, dy; => trackingObjectPosY, trackingObjectSpeedY;
//  ttl; => trackingObjectState;
//  uint8_t hue; => float   trackingObjectShift
//  uint8_t saturation; => 255U
//  uint8_t value; => trackingObjectHue;

// характеристики изображения CHSV picture[WIDTH][HEIGHT]
//  uint8_t .hue; => noise3d[0][WIDTH][HEIGHT]
//  uint8_t .sat; => shiftValue[HEIGHT] (не хватило двухмерного массива на насыщенность)
//  uint8_t .val; => noise3d[1][WIDTH][HEIGHT]

#define FLAME_MAX_DY        256 // максимальная вертикальная скорость перемещения языков пламени за кадр.  имеется в виду 256/256 =   1 пиксель за кадр
#define FLAME_MIN_DY        128 // минимальная вертикальная скорость перемещения языков пламени за кадр.   имеется в виду 128/256 = 0.5 пикселя за кадр
#define FLAME_MAX_DX         32 // максимальная горизонтальная скорость перемещения языков пламени за кадр. имеется в виду 32/256 = 0.125 пикселя за кадр
#define FLAME_MIN_DX       (-FLAME_MAX_DX)
#define FLAME_MAX_VALUE     255 // максимальная начальная яркость языка пламени
#define FLAME_MIN_VALUE     176 // минимальная начальная яркость языка пламени

//пришлось изобрести очередную функцию субпиксельной графики. на этот раз бесшовная по ИКСу, работающая в цветовом пространстве HSV и без смешивания цветов
void wu_pixel_maxV(int16_t item) {
  //uint8_t xx = trackingObjectPosX[item] & 0xff, yy = trackingObjectPosY[item] & 0xff, ix = 255 - xx, iy = 255 - yy;
  uint8_t xx = (trackingObjectPosX[item] - (int)trackingObjectPosX[item]) * 255, yy = (trackingObjectPosY[item] - (int)trackingObjectPosY[item]) * 255, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
#define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
  uint8_t wu[4] = { WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy), WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)
  };
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t x1 = (int8_t)(trackingObjectPosX[item] + (i & 1)) % WIDTH; //делаем бесшовный по ИКСу
    uint8_t y1 = (int8_t)(trackingObjectPosY[item] + ((i >> 1) & 1));
    if (y1 < HEIGHT && trackingObjectHue[item] * wu[i] >> 8 >= noise3d[1][x1][y1]) {
      noise3d[0][x1][y1] = trackingObjectShift[item];
      shiftValue[y1] = 255U;//saturation;
      noise3d[1][x1][y1] = trackingObjectHue[item] * wu[i] >> 8;
    }
  }
}


void EffectFire::setup(EffectContext& ctx) {
  int16_t i, j;

  // чистим массив объектов от того, что не похоже на языки пламени
  for (i = 0; i < trackingObjectMaxCount; i++) {
    if (trackingObjectState[i] > 30U || trackingObjectPosY[i] >= HEIGHT || trackingObjectPosX[i] >= WIDTH || trackingObjectPosY[i] <= 0) {
      trackingObjectHue[i] = 0U;
      trackingObjectState[i] = random8(20);
    }
  }
  // заполняем массив изображения из массива leds обратным преобразованием, которое нихрена не работает
  for (i = 0; i < WIDTH; i++) {
    for (j = 0; j < HEIGHT; j++) {
      CHSV tHSV = rgb2hsv_approximate(ctx.led.getPixel(i, j));
      noise3d[0][i][j] = tHSV.hue;
      // такая защита от пересвета более-менее достаточна
      if (tHSV.val > 100U) {
        shiftValue[j] = tHSV.sat;
        // для перехода с очень тусклых эффектов, использующих заливку белым или почти белым светом
        if (tHSV.sat < 100U) {
          noise3d[1][i][j] = tHSV.val / 3U;
        } else {
          noise3d[1][i][j] = tHSV.val - 32U;
        }
      } else {
        noise3d[1][i][j] = 0U;
      }
    }
  }
}

void EffectFire::render(EffectContext& ctx) {
  int16_t i, j;

  enlargedObjectNum = map8(ctx.speed, 1U, trackingObjectMaxCount);
  ff_x = WIDTH * 2.4;
  enlargedObjectNum = (ff_x > enlargedObjectMaxCount) ? enlargedObjectMaxCount : ff_x;

  // минимальная живучесть/высота языка пламени ...ttl
  hue = map8(scaleToWave8(ctx.scale + 3U), 3, 10);
  // максимальная живучесть/высота языка пламени ...ttl
  hue2 = map8(scaleToWave8(ctx.scale + 3U), 6, 31);

  // угасание предыдущего кадра
  for (i = 0; i < WIDTH; i++) {
    for (j = 0; j < HEIGHT; j++) {
      noise3d[1][i][j] = static_cast<uint16_t>(noise3d[1][i][j]) * 237U >> 8;
    }
  }

  // цикл перебора языков пламени
  for (i = 0; i < enlargedObjectNum; i++) {
    // если ещё не закончилась его жизнь
    if (trackingObjectState[i]) {
      wu_pixel_maxV(i);

      j = trackingObjectState[i];
      trackingObjectState[i]--;

      trackingObjectPosX[i] += trackingObjectSpeedX[i];
      trackingObjectPosY[i] += trackingObjectSpeedY[i];

      trackingObjectHue[i] = (trackingObjectState[i] * trackingObjectHue[i] + j / 2) / j;

      // если вышел за верхнюю границу или потух, то и жизнь закончилась
      if (trackingObjectPosY[i] >= HEIGHT || trackingObjectHue[i] < 2U) {
        trackingObjectState[i] = 0;
      }

      // если вылез за край матрицы по горизонтали, перекинем на другую сторону
      if (trackingObjectPosX[i] < 0) {
        trackingObjectPosX[i] += WIDTH;
      } else if (trackingObjectPosX[i] >= WIDTH) {
        trackingObjectPosX[i] -= WIDTH;
      }
    }
    // если жизнь закончилась, перезапускаем
    else {
      trackingObjectState[i] = random8(hue, hue2);
      // 254 - это шаг в обратную сторону от выбранного пользователем оттенка (стартовый оттенок диапазона)
      trackingObjectShift[i] = static_cast<uint8_t>(254U + ctx.scale + random8(20U));
      // 20 - это диапазон из градиента цвета от выбранного пользователем оттенка (диапазон от 254 до 254+20)
      trackingObjectPosX[i] = static_cast<float>(random(WIDTH * 255U)) / 255.0f;
      trackingObjectPosY[i] = -.9;
      trackingObjectSpeedX[i] = static_cast<float>(FLAME_MIN_DX + random8(FLAME_MAX_DX - FLAME_MIN_DX)) / 256.0f;
      trackingObjectSpeedY[i] = static_cast<float>(FLAME_MIN_DY + random8(FLAME_MAX_DY - FLAME_MIN_DY)) / 256.0f;
      trackingObjectHue[i] = FLAME_MIN_VALUE + random8(FLAME_MAX_VALUE - FLAME_MIN_VALUE + 1U);
      //saturation = 255U;
    }
  }

  //выводим кадр на матрицу
  for (i = 0; i < WIDTH; i++) {
    for (j = 0; j < HEIGHT; j++) {
      //hsv2rgb_spectrum(CHSV(noise3d[0][i][j], shiftValue[j], noise3d[1][i][j] * 1.033), leds[XY(i,j)]); // 1.033 - это коэффициент нормализации яркости (чтобы чутка увеличить яркость эффекта в целом)
      if (ctx.palette) {
        ctx.led.getPixel(i, j) = ColorFromPalette(*ctx.palette, noise3d[0][i][j], noise3d[1][i][j]);
      } else {
        hsv2rgb_spectrum(CHSV(noise3d[0][i][j], shiftValue[j], noise3d[1][i][j]), ctx.led.getPixel(i, j));
      }
    }
  }
}
