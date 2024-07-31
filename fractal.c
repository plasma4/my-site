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

static inline float powThreeQuarters(float x) {
  float t = sqrtf(x);
  return t * sqrtf(t);
}

static inline uint32_t mixBlack(uint32_t colorStart, uint32_t a) {
  if (!a) return colorStart;
  uint32_t reverse = 0xff - a;
  return ((((colorStart & 0xff) * reverse) >> 8)) ^
         (((((colorStart >> 8) & 0xff) * reverse)) & -0xff) ^
         (((((colorStart >> 16) & 0xff) * reverse) << 8) & -0xffff) ^
         0xff000000;
}

static inline uint32_t toRGB(uint32_t r, uint32_t g, uint32_t b) {
  return r ^ (g << 8) ^ (b << 16) ^ 0xff000000;
}

static inline uint32_t getR(uint32_t color) { return color & 0xff; }

static inline uint32_t getG(uint32_t color) { return (color >> 8) & 0xff; }

static inline uint32_t getB(uint32_t color) { return (color >> 16) & 0xff; }

// Fast mixing (smoothing) of 32-bit colors but with consideration for the
// rendering mode
static inline uint32_t mix2(uint32_t colorStart, uint32_t colorEnd, float a,
                            int renderMode, float darkenAmount) {
  if (renderMode == 0) {
    uint32_t color = mix(colorStart, colorEnd, a * darkenAmount * 255);
  }
  uint32_t color = mix(colorStart, colorEnd, (a * sqrt(a)) * 255);
  return mixBlack(color, 200 * darkenAmount);
}

// Get a smoothed, looped, index of a pallete
uint32_t getPallete(float position, uint32_t *pallete, int length,
                    int renderMode, float darkenAmount) {
  // Pallete used by handlePixels (last element=first element for looping).
  // Interestingly, you can get the hex representations with the middle six
  // letters (#a00a0a for the first one, for example)
  int id = (int)position % length;
  float mod = position - ((int)position / length) * length - (float)id;
  if (renderMode == 1) {
    uint32_t color = mix(pallete[id], pallete[id + 1], mod * sqrtf(mod) * 255);
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
      color =
          mixBlack(color, 200.0f - powThreeQuarters(2.5f * (1.0f - mod)) * 200);
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
    return mixBlack(color, 200 * darkenAmount);
  }
  uint32_t color = mix(pallete[id], pallete[id + 1], mod * 255);
  return mixBlack(color, 200 * darkenAmount);
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

// All the fractal functions are below! The first section has no shading, the
// second section has directional shading, and the third section does some funky
// weird shading that's a little hard to explain.

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
    if (sr + si > 500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.6309297535714575f;
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si))) * 0.5f;
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.43067655807339306f;
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.38685280723454163f;
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.3562071871080222f;
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.6309297535714575f;
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si))) * 0.5f;
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.6309297535714575f;
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si))) * 0.5f;
      return result;
    }
  }
  return -999.0f;
}

// -----

float mandS(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  double dr = 1;
  double di = 0;
  for (int n = 1; n < iterations; n++) {
    double tempdr = 2.0 * (dr * r - di * i) + 1.0;
    di = 2.0 * (dr * i + di * r);
    dr = tempdr;
    i = 2 * r * i + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double sqm = dr * dr + di * di;
      double ur = (r * dr + i * di) / sqm;
      double ui = (i * dr - r * di) / sqm;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5f;
      *ptr = t <= 0 ? 0 : (t * 0.4f);
      return result;
    }
  }
  return -999.0f;
}

float mand3S(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    r = r * (sr - 3.0 * si) + x;
    i = i * (3.0 * sr - si) + y;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.6309297535714575f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mand4S(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = 4.0 * (sr * r * i - r * si * i) + y;
    r = sr * (sr - 6.0 * si) + si * si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si))) * 0.5f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mand5S(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.43067655807339306f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mand6S(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.38685280723454163f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mand7S(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.3562071871080222f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float shipS(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  double dr = 1;
  double di = 0;
  for (int n = 1; n < iterations; n++) {
    i = fabs(2.0 * r * i) + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float ship3S(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    r = fabs(r) * (sr - 3.0 * si) + x;
    i = fabs(i) * (3.0 * sr - si) + y;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.6309297535714575f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float ship4S(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = fabs(4.0 * r * i) * (sr - si) + y;
    r = sr * sr - 6.0 * sr * si + si * si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si))) * 0.5f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float celtS(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = 2.0 * r * i + y;
    r = fabs(sr - si) + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float prmbS(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float buffS(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float tricS(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = -2.0 * r * i + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mbbsS(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mbbs3S(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result =
          (float)n - (secondLog(sqrtf(sr + si))) * 0.6309297535714575f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

float mbbs4S(int iterations, double x, double y, float *ptr) {
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
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si))) * 0.5f;
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

// ----- (there's only one function here for now but whatever...)

float mandS2(int iterations, double x, double y, float *ptr) {
  double r = x;
  double i = y;
  double sr = r * r;
  double si = i * i;
  for (int n = 1; n < iterations; n++) {
    i = 2 * r * i + y;
    r = sr - si + x;
    sr = r * r;
    si = i * i;
    if (sr + si > 2500.0) {
      float result = (float)n - (secondLog(sqrtf(sr + si)));
      double ur = r + i;
      double ui = i - r;
      double norm = sqrt(ur * ur + ui * ui);
      ur /= norm;
      ui /= norm;
      float t = (ur + ui) * 0.7071067811865475f + 1.5;
      *ptr = t <= 0 ? 0 : t * 0.4f;
      return result;
    }
  }
  return -999.0f;
}

extern int run(int type, int w, int h, int pixel, double posX, double posY,
               double zoom, int max, float *iters, uint32_t *colors,
               int iterations, uint32_t *pallete, int palleteLength,
               uint32_t interiorColor, int renderMode, int darkenEffect,
               float speed, float flowAmount) {
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
  float *itersPtr = iters;

  // This uses a do...while rather than a simple while, so it doesn't increment
  // the first time.
  do {
    if (x == W) {
      x = 0;
      y++;
    }
    float t = iters[i];
    float *ptr = itersPtr + limit + i;
    if (t) {
      if (t == -999.0f) {
        colors[i] = interiorColor;
      } else if (t == 1.0f) {
        int index = flowAmount;
        int indexModulo = index % palleteLength;
        float l = *ptr;
        colors[i] = mix2(pallete[indexModulo], pallete[indexModulo + 1],
                         flowAmount - index, renderMode,
                         darkenEffect == 2 ? 1.0f - l : l);
      } else {
        float l = *ptr;
        colors[i] = getPallete(
            flog2(t) * speed1 + (t - 1) * speed2 + flowAmount, pallete,
            palleteLength, renderMode, darkenEffect == 2 ? 1.0f - l : l);
      }
      x++;
      continue;
    }
    double coordinateX = posX + (x++) * zoom;
    double coordinateY = posY + y * zoom;

    float n;
    // Run the function needed and also look at the darken effect
    switch (darkenEffect) {
      case 0:
        switch (type) {
          case 0:
            n = mand(iterations, coordinateX, coordinateY);
            break;
          case 1:
            n = mand3(iterations, coordinateX, coordinateY);
            break;
          case 2:
            n = mand4(iterations, coordinateX, coordinateY);
            break;
          case 3:
            n = mand5(iterations, coordinateX, coordinateY);
            break;
          case 4:
            n = mand6(iterations, coordinateX, coordinateY);
            break;
          case 5:
            n = mand7(iterations, coordinateX, coordinateY);
            break;
          case 6:
            n = ship(iterations, coordinateX, coordinateY);
            break;
          case 7:
            n = ship3(iterations, coordinateX, coordinateY);
            break;
          case 8:
            n = ship4(iterations, coordinateX, coordinateY);
            break;
          case 9:
            n = celt(iterations, coordinateX, coordinateY);
            break;
          case 10:
            n = prmb(iterations, coordinateX, coordinateY);
            break;
          case 11:
            n = buff(iterations, coordinateX, coordinateY);
            break;
          case 12:
            n = tric(iterations, coordinateX, coordinateY);
            break;
          case 13:
            n = mbbs(iterations, coordinateX, coordinateY);
            break;
          case 14:
            n = mbbs3(iterations, coordinateX, coordinateY);
            break;
          case 15:
            n = mbbs4(iterations, coordinateX, coordinateY);
        }
        break;
      case 3:
        switch (type) {
          case 0:
            n = mandS2(iterations, coordinateX, coordinateY, ptr);
            break;
          case 1:
            n = mand3S(
                iterations, coordinateX, coordinateY,
                ptr);  // So, the silly thing is that I haven't got proper
                       // shading working for any other fractals yet. So it
                       // defaults to a wonky alternative to it!
            break;
          case 2:
            n = mand4S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 3:
            n = mand5S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 4:
            n = mand6S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 5:
            n = mand7S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 6:
            n = shipS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 7:
            n = ship3S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 8:
            n = ship4S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 9:
            n = celtS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 10:
            n = prmbS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 11:
            n = buffS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 12:
            n = tricS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 13:
            n = mbbsS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 14:
            n = mbbs3S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 15:
            n = mbbs4S(iterations, coordinateX, coordinateY, ptr);
        }
        break;
      default:
        switch (type) {
          case 0:
            n = mandS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 1:
            n = mand3S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 2:
            n = mand4S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 3:
            n = mand5S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 4:
            n = mand6S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 5:
            n = mand7S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 6:
            n = shipS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 7:
            n = ship3S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 8:
            n = ship4S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 9:
            n = celtS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 10:
            n = prmbS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 11:
            n = buffS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 12:
            n = tricS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 13:
            n = mbbsS(iterations, coordinateX, coordinateY, ptr);
            break;
          case 14:
            n = mbbs3S(iterations, coordinateX, coordinateY, ptr);
            break;
          case 15:
            n = mbbs4S(iterations, coordinateX, coordinateY, ptr);
        }
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
      float l = *ptr;
      colors[i] = mix2(pallete[indexModulo], pallete[indexModulo + 1],
                       flowAmount - index, renderMode,
                       darkenEffect == 2 ? 1.0f - l : l);
      iters[i] = 1.0f;
    } else {
      score += 13 + (int)n;
      float l = *ptr;
      colors[i] = getPallete(flog2(n) * speed1 + (n - 1) * speed2 + flowAmount,
                             pallete, palleteLength, renderMode,
                             darkenEffect == 2 ? 1.0f - l : l);
      iters[i] = n;
    }
    if (score > max) {
      return i;
    }
  } while (++i != limit);
  // Tell the script that it has completed!
  return -1;
}