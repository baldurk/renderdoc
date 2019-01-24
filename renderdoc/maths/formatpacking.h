/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <stdint.h>
#include "vec.h"

inline Vec4f ConvertFromR10G10B10A2(uint32_t data)
{
  return Vec4f(float((data >> 0) & 0x3ff) / 1023.0f, float((data >> 10) & 0x3ff) / 1023.0f,
               float((data >> 20) & 0x3ff) / 1023.0f, float((data >> 30) & 0x003) / 3.0f);
}

inline Vec4f ConvertFromR10G10B10A2SNorm(uint32_t data)
{
  int r = int(data >> 0) & 0x3ff;
  int g = int(data >> 10) & 0x3ff;
  int b = int(data >> 20) & 0x3ff;
  int a = int(data >> 30) & 3;

  if(r >= 512)
    r -= 1024;
  if(g >= 512)
    g -= 1024;
  if(b >= 512)
    b -= 1024;
  if(a >= 2)
    a -= 4;

  if(r == -512)
    r = -511;
  if(g == -512)
    g = -511;
  if(b == -512)
    b = -511;
  if(a == -2)
    a = -1;

  return Vec4f(float(r) / 511.0f, float(g) / 511.0f, float(b) / 511.0f, float(a) / 1.0f);
}

inline uint32_t ConvertToR10G10B10A2(Vec4f data)
{
  float x = data.x < 1.0f ? (data.x > 0.0f ? data.x : 0.0f) : 1.0f;
  float y = data.y < 1.0f ? (data.y > 0.0f ? data.y : 0.0f) : 1.0f;
  float z = data.z < 1.0f ? (data.z > 0.0f ? data.z : 0.0f) : 1.0f;
  float w = data.w < 1.0f ? (data.w > 0.0f ? data.w : 0.0f) : 1.0f;

  return (uint32_t(x * 1023) << 0) | (uint32_t(y * 1023) << 10) | (uint32_t(z * 1023) << 20) |
         (uint32_t(w * 3) << 30);
}

inline Vec3f ConvertFromR11G11B10(uint32_t data)
{
  uint32_t mantissas[3] = {
      (data >> 0) & 0x3f, (data >> 11) & 0x3f, (data >> 22) & 0x1f,
  };
  int32_t exponents[3] = {
      int32_t(data >> 6) & 0x1f, int32_t(data >> 17) & 0x1f, int32_t(data >> 27) & 0x1f,
  };

  Vec3f ret;
  uint32_t *retu = (uint32_t *)&ret.x;

  // floats have 23 bit mantissa, 8bit exponent
  // R11G11B10 has 6/6/5 bit mantissas, 5bit exponents

  const int mantissaShift[] = {23 - 6, 23 - 6, 23 - 5};

  for(int i = 0; i < 3; i++)
  {
    if(mantissas[i] == 0 && exponents[i] == 0)
    {
      retu[i] = 0;
    }
    else
    {
      if(exponents[i] == 0x1f)
      {
        // infinity or nan
        retu[i] = 0x7f800000 | mantissas[i] << mantissaShift[i];
      }
      else if(exponents[i] != 0)
      {
        // shift exponent and mantissa to the right range for 32bit floats
        retu[i] = (exponents[i] + (127 - 15)) << 23 | mantissas[i] << mantissaShift[i];
      }
      else if(exponents[i] == 0)
      {
        // we know xMantissa isn't 0 also, or it would have been caught above

        exponents[i] = 1;

        // shift until hidden bit is set
        while((mantissas[i] & 0x40) == 0)
        {
          mantissas[i] <<= 1;
          exponents[i]--;
        }

        // remove the hidden bit
        mantissas[i] &= ~0x40;

        retu[i] = (exponents[i] + (127 - 15)) << 23 | mantissas[i] << mantissaShift[i];
      }
    }
  }

  return ret;
}

/*
inline uint32_t ConvertToR11G11B10(Vec3f data)
{
  return 0;
}
*/

inline Vec4f ConvertFromB5G5R5A1(uint16_t data)
{
  return Vec4f((float)((data >> 0) & 0x1f) / 31.0f, (float)((data >> 5) & 0x1f) / 31.0f,
               (float)((data >> 10) & 0x1f) / 31.0f, ((data & 0x8000) > 0) ? 1.0f : 0.0f);
}

inline Vec3f ConvertFromB5G6R5(uint16_t data)
{
  return Vec3f((float)((data >> 0) & 0x1f) / 31.0f, (float)((data >> 5) & 0x3f) / 63.0f,
               (float)((data >> 11) & 0x1f) / 31.0f);
}

inline Vec4f ConvertFromB4G4R4A4(uint16_t data)
{
  return Vec4f((float)((data >> 0) & 0xf) / 15.0f, (float)((data >> 4) & 0xf) / 15.0f,
               (float)((data >> 8) & 0xf) / 15.0f, (float)((data >> 12) & 0xf) / 15.0f);
}

extern float SRGB8_lookuptable[256];

inline float ConvertFromSRGB8(uint8_t comp)
{
  return SRGB8_lookuptable[comp];
}

inline float ConvertSRGBToLinear(float srgbF)
{
  if(srgbF <= 0.04045f)
    return srgbF / 12.92f;

  if(srgbF < 0.0f)
    srgbF = 0.0f;
  else if(srgbF > 1.0f)
    srgbF = 1.0f;

  return powf((0.055f + srgbF) / 1.055f, 2.4f);
}

inline Vec4f ConvertSRGBToLinear(Vec4f srgbF)
{
  return Vec4f(ConvertSRGBToLinear(srgbF.x), ConvertSRGBToLinear(srgbF.y),
               ConvertSRGBToLinear(srgbF.z), srgbF.w);
}

struct ResourceFormat;
float ConvertComponent(const ResourceFormat &fmt, const byte *data);

#include "half_convert.h"
