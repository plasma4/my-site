/*
Welcome to my code that helps run my fractal viewer!
It is compiled into WASM, which is supported by JavaScript.


Magic command: emcc fractal.c -O3 -o fractal.wasm -sEXPORT_KEEPALIVE
-sIGNORE_MISSING_MAIN --no-entry -sEXPORTED_FUNCTIONS=_run -sALLOW_MEMORY_GROWTH
*/

#include <math.h>
#include <stdint.h>
// Fast mixing (smoothing) of 32-bit colors
static inline uint32_t mix(uint32_t colorStart, uint32_t colorEnd, uint32_t a) {
  uint32_t reverse = 0xff - a;
  return ((((colorStart & 0xff) * reverse + (colorEnd & 0xff) * a) >> 8)) ^
         (((((colorStart >> 8) & 0xff) * reverse +
            ((colorEnd >> 8) & 0xff) * a)) &
          -0xff) ^
         (((((colorStart >> 16) & 0xff) * reverse +
            ((colorEnd >> 16) & 0xff) * a)
           << 8) &
          -0xffff) ^
         0xff000000;
}

// static inline uint32_t mixWhite(uint32_t colorStart, uint32_t a) {
//   uint32_t reverse = 0xff - a;
//   return ((((colorStart & 0xff) * reverse + 0xff * a) >> 8)) ^
//          (((((colorStart >> 8) & 0xff) * reverse + 0xff * a)) & -0xff) ^
//          (((((colorStart >> 16) & 0xff) * reverse + 0xff * a) << 8) &
//          -0xffff) ^ 0xff000000;
// }

// static inline uint32_t mixBlack(uint32_t colorStart, uint32_t a) {
//   uint32_t reverse = 0xff - a;
//   return ((((colorStart & 0xff) * reverse * a) >> 8)) ^
//          (((((colorStart >> 8) & 0xff) * reverse)) & -0xff) ^
//          ((((colorStart >> 16) & 0xff) * reverse << 8) & -0xffff) ^
//          0xff000000;
// }

static inline uint32_t toRGB(uint32_t r, uint32_t g, uint32_t b) {
  return r ^ (g << 8) ^ (b << 16) ^ 0xff000000;
}

static inline float powThreeQuarters(float x) {
  float t = sqrtf(x);
  return t * sqrtf(t);
}

static inline uint32_t getR(uint32_t color) { return color & 0xff; }

static inline uint32_t getG(uint32_t color) { return (color >> 8) & 0xff; }

static inline uint32_t getB(uint32_t color) { return (color >> 16) & 0xff; }

// Get a smoothed, looped, index of a pallete
uint32_t getPallete(float position, uint32_t *pallete, int length,
                    int renderMode) {
  // Pallete used by handlePixels (last element=first element for looping).
  // Interestingly, you can get the hex representations with the middle six
  // letters (#a00a0a for the first one, for example)
  int id = (int)position;
  float mod = position - (float)id;
  int index = id % length;
  if (renderMode == 1) {
    uint32_t color =
        mix(pallete[index], pallete[index + 1], mod * sqrtf(mod) * 0xff);
    // Incredibly complicated, eh?
    int newColor = toRGB(45.0f + 0.8f * getR(color), 45.0f + 0.8f * getG(color),
                         45.0f + 0.8f * getB(color));
    if (mod < 0.1) {
      if (mod < 0.025) {
        color = mix(color, newColor, mod * 100);
      } else if (mod > 0.075) {
        color = mix(color, newColor, (0.1f - mod) * 100);
      } else {
        color = newColor;
      }
    } else if (mod > 0.6) {
      if (mod < 0.7) {
        if (mod < 0.625) {
          color = mix(color, newColor, (mod - 0.5f) * 100);
        } else if (mod > 0.675) {
          color = mix(color, newColor, (0.6f - mod) * 100);
        } else {
          color = newColor;
        }
      } else if (mod > 0.8 && mod < 0.99) {
        newColor =
            toRGB(64.0f + 0.75f * getR(color), 64.0f + 0.75f * getG(color),
                  64.0f + 0.75f * getB(color));
        if (mod < 0.825) {
          color = mix(color, newColor, (mod - 0.8f) * 100);
        } else if (mod > 0.875) {
          color = mix(color, newColor, (0.9f - mod) * 100);
        } else {
          color = newColor;
        }
      }
      float multiplier = powThreeQuarters(1.0f - (mod - 0.6f) * 2.5f);
      color = toRGB(getR(color) * multiplier, getG(color) * multiplier,
                    getB(color) * multiplier);
    } else if (mod > 0.2 && mod < 0.3) {
      if (mod < 0.225) {
        color = mix(color, newColor, (mod - 0.2f) * 100);
      } else if (mod > 0.275) {
        color = mix(color, newColor, (0.3f - mod) * 100);
      } else {
        color = newColor;
      }
    } else if (mod > 0.4 && mod < 0.5) {
      if (mod < 0.425) {
        color = mix(color, newColor, (mod - 0.4f) * 100);
      } else if (mod > 0.475) {
        color = mix(color, newColor, (0.5f - mod) * 100);
      } else {
        color = newColor;
      }
    }
    return color;
  }
  return mix(pallete[index], pallete[index + 1], mod * 0xff);
}

// Really efficient (wouldn't be used if wasm supported log2; reduces overhead)
static inline float flog2(float n) {
  union {
    float number;
    uint32_t integer;
  } firstUnion = {n};
  union {
    uint32_t integer;
    float number;
  } secondUnion = {(firstUnion.integer & 0x7fffff) | 0x3f000000};
  float y = firstUnion.integer;
  y *= 1.19209289e-7f;

  return y - 124.225517f - 1.4980303f * secondUnion.number -
         1.72588f / (0.35208873f + secondUnion.number);
}

static inline float secondLog(float n) {
  // Simpler to create a function for this, as it's used so much
  return flog2(flog2(n));
}

static inline float cosq(float x) {
  x *= 1.5707963267949f;
  x -= 0.25f + floor(x + 0.25f);
  x *= 16.0f * (fabsf(x) - 0.5f);
  x += 0.225f * x * (fabsf(x) - 1.0f);
  return x;
}

static inline float sinq(float x) {
  x += 1.5707963267949f;
  x *= 0.1591549430919f;
  x -= 0.25f + floor(x + 0.25f);
  x *= 16.0f * (fabsf(x) - 0.5f);
  x += 0.225f * x * (fabsf(x) - 1.0f);
  return x;
}

// All the fractal functions!
float mand(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = 2 * r * i + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float mand3(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    r = r * (sr - 3.0 * si) + x;
    i = i * (3.0 * sr - si) + y;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.6309297535714575;
      return result;
    }
  }
  return -999.0f;
}

float mand4(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = 4.0 * (sr * r * i - r * si * i) + y;
    r = sr * (sr - 6.0 * si) + si * si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.5;
      return result;
    }
  }
  return -999.0f;
}

float mand5(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  double fi = si * si;
  for (int n = 1; n < iterations; n++) {
    i = i * (sr * (5.0 * sr - 10.0 * si) + fi) + y;
    r = r * (sr * (sr - 10.0 * si) + 5.0 * fi) + x;
    sr = r * r;
    si = i * i;
    fi = si * si;
    if (sr + si > 256.0) {
      float result =
          (float)n - (secondLog(sqrt(sr + si))) * 0.43067655807339306;
      return result;
    }
  }
  return -999.0f;
}

float mand6(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  double fr = sr * sr;
  double fi = si * si;
  for (int n = 1; n < iterations; n++) {
    i = r * i * (6.0 * (fr + fi) - 20.0 * sr * si) + y;
    r = sr * (fr + 15.0 * fi) - si * (15.0 * fr + fi) + x;
    sr = r * r;
    si = i * i;
    fr = sr * sr;
    fi = si * si;
    if (sr + si > 256.0) {
      float result =
          (float)n - (secondLog(sqrt(sr + si))) * 0.38685280723454163;
      return result;
    }
  }
  return -999.0f;
}

float mand7(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  double fr = sr * sr;
  double fi = si * si;
  for (int n = 1; n < iterations; n++) {
    r = r * (fr * (sr - 21.0 * si) + fi * (35.0 * sr - 7.0 * si)) + x;
    i = i * (fr * (7.0 * sr - 35.0 * si) + fi * (21.0 * sr - si)) + y;
    sr = r * r;
    si = i * i;
    fr = sr * sr;
    fi = si * si;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.3562071871080222;
      return result;
    }
  }
  return -999.0f;
}

float ship(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = fabs(2.0 * r * i) + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float ship3(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    r = fabs(r) * (sr - 3.0 * si) + x;
    i = fabs(i) * (3.0 * sr - si) + y;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.6309297535714575;
      return result;
    }
  }
  return -999.0f;
}

float ship4(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = fabs(4.0 * r * i) * (sr - si) + y;
    r = sr * sr - 6.0 * sr * si + si * si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.5;
      return result;
    }
  }
  return -999.0f;
}

float celt(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = 2.0 * r * i + y;
    r = fabs(sr - si) + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float prmb(int iterations, double x, double y) {
  double r = fabs(x);
  double i = -y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    double tr = 2.0 * r * i;
    r = fabs(sr - i * i + x);
    i = -tr - y;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float buff(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    r = fabs(r);
    i = fabs(i);
    double tr = 2.0 * r * i;
    r = sr - si - r + x;
    i = tr - i + y;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float tric(int iterations, double x, double y) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = -2.0 * r * i + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float mbbs(int iterations, double x, double y) {
  int exchange = 1;
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    if (exchange++ == 10) {
      exchange = 1;
      i = fabs(2.0 * r * i) + y;
      r = sr - si + x;
    } else {
      i = 2.0 * r * i + y;
      r = sr - si + x;
    }
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si)));
      return result;
    }
  }
  return -999.0f;
}

float mbbs3(int iterations, double x, double y) {
  int exchange = 1;
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    if (exchange++ == 10) {
      exchange = 1;
      r = fabs(r) * (sr - 3.0 * si) + x;
      i = fabs(i) * (3.0 * sr - si) + y;
    } else {
      r = r * (sr - 3.0 * si) + x;
      i = i * (3.0 * sr - si) + y;
    }
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.6309297535714575;
      return result;
    }
  }
  return -999.0f;
}

float mbbs4(int iterations, double x, double y) {
  int exchange = 1;
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    if (exchange++ == 10) {
      exchange = 1;
      i = fabs(4.0 * r * i) * (sr - si) + y;
      r = sr * sr - 6.0 * sr * si + si * si + x;
    } else {
      i = 4.0 * (sr * r * i - r * si * i) + y;
      r = sr * (sr - 6.0 * si) + si * si + x;
    }
    sr = r * r;
    si = i * i;
    if (sr + si > 256.0) {
      float result = (float)n - (secondLog(sqrt(sr + si))) * 0.5;
      return result;
    }
  }
  return -999.0f;
}

extern int run(int type, int w, int h, int pixel, double posX, double posY,
               double zoom, int max, float *iters, uint32_t *colors,
               int iterations, uint32_t *pallete, int palleteLength,
               uint32_t interiorColor, int renderMode, float speed,
               float flowAmount) {
  // The boring stuff is here! We use 32-bit RGBA uint32_t instead of 8-bit
  // numbers for the coloring, because it's simpler and doesn't slow down JS at
  // all (we can access it with Uint8ClampedArray)
  int i = pixel;
  double x = i % w;
  double y = i / w;
  int W = w;
  int score = 0;
  int limit = w * h;
  int biggerIterations = iterations + 2;

  // Pre-calculate speed constants for faster renderings
  float speed1 = sqrtf(sqrtf(speed));
  float speed2 = 0.035f * speed;

  // we use a do...while rather than a simple while, so it doesn't increment the
  // first time.
  do {
    if (x == W) {
      x = 0;
      y++;
    }
    float t = iters[i];
    if (t) {
      if (t == -999.0f) {
        colors[i] = interiorColor;
      } else if (t == 1.0f) {
        int index = flowAmount;
        int indexModulo = index % palleteLength;
        colors[i] = mix(pallete[indexModulo], pallete[indexModulo + 1],
                        (flowAmount - index) * 0xff);
      } else {
        colors[i] =
            getPallete(flog2(t) * speed1 + (t - 1) * speed2 + flowAmount,
                       pallete, palleteLength, renderMode);
      }
      x++;
      continue;
    }
    double coordinateX = posX + (x++) * zoom;
    double coordinateY = posY + y * zoom;

    float n;
    // Run the function for on of the 15 types
    if (type == 0) {
      n = mand(iterations, coordinateX, coordinateY);
    } else if (type == 1) {
      n = mand3(iterations, coordinateX, coordinateY);
    } else if (type == 2) {
      n = mand4(iterations, coordinateX, coordinateY);
    } else if (type == 3) {
      n = mand5(iterations, coordinateX, coordinateY);
    } else if (type == 4) {
      n = mand6(iterations, coordinateX, coordinateY);
    } else if (type == 5) {
      n = mand7(iterations, coordinateX, coordinateY);
    } else if (type == 6) {
      n = ship(iterations, coordinateX, coordinateY);
    } else if (type == 7) {
      n = ship3(iterations, coordinateX, coordinateY);
    } else if (type == 8) {
      n = ship4(iterations, coordinateX, coordinateY);
    } else if (type == 9) {
      n = celt(iterations, coordinateX, coordinateY);
    } else if (type == 10) {
      n = prmb(iterations, coordinateX, coordinateY);
    } else if (type == 11) {
      n = buff(iterations, coordinateX, coordinateY);
    } else if (type == 12) {
      n = tric(iterations, coordinateX, coordinateY);
    } else if (type == 13) {
      n = mbbs(iterations, coordinateX, coordinateY);
    } else if (type == 14) {
      n = mbbs3(iterations, coordinateX, coordinateY);
    } else if (type == 15) {
      n = mbbs4(iterations, coordinateX, coordinateY);
    }
    // Cost increases are pre-computed to be as stable as possible (at least for
    // my computer)
    if (n == -999.0f) {
      score += biggerIterations;
      colors[i] = interiorColor;
      iters[i] = -999.0;
    }
    // What's this number? If flog2() has a value less than this, it gives a
    // negative number, which will cause problems.
    else if (n < 1.000004f) {
      int index = flowAmount;
      int indexModulo = index % palleteLength;
      colors[i] = mix(pallete[indexModulo], pallete[indexModulo + 1],
                      (flowAmount - index) * 0xff);
    } else {
      score += 13 + (int)n;
      colors[i] = getPallete(flog2(n) * speed1 + (n - 1) * speed2 + flowAmount,
                             pallete, palleteLength, renderMode);
      iters[i] = n;
    }
    if (score > max) {
      return i;
    }
  } while (++i != limit);
  // Tell the script that it has completed!
  return -1;
}