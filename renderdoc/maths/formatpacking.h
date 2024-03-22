/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <stddef.h>
#include <stdint.h>
#include "api/replay/data_types.h"
#include "half_convert.h"
#include "vec.h"

inline Vec4f ConvertFromR10G10B10A2(uint32_t data)
{
  return Vec4f(float((data >> 0) & 0x3ff) / 1023.0f, float((data >> 10) & 0x3ff) / 1023.0f,
               float((data >> 20) & 0x3ff) / 1023.0f, float((data >> 30) & 0x003) / 3.0f);
}

inline Vec4u ConvertFromR10G10B10A2UInt(uint32_t data)
{
  return Vec4u((data >> 0) & 0x3ff, (data >> 10) & 0x3ff, (data >> 20) & 0x3ff, (data >> 30) & 0x003);
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

inline uint32_t ConvertToR10G10B10A2(Vec4u data)
{
  return (uint32_t(data.x & 0x3ff) << 0) | (uint32_t(data.y & 0x3ff) << 10) |
         (uint32_t(data.z & 0x3ff) << 20) | (uint32_t(data.w & 0x3) << 30);
}

inline uint32_t ConvertToR10G10B10A2SNorm(Vec4f data)
{
  float x = data.x < 1.0f ? (data.x > -1.0f ? data.x : -1.0f) : 1.0f;
  float y = data.y < 1.0f ? (data.y > -1.0f ? data.y : -1.0f) : 1.0f;
  float z = data.z < 1.0f ? (data.z > -1.0f ? data.z : -1.0f) : 1.0f;
  float w = data.w < 1.0f ? (data.w > -1.0f ? data.w : -1.0f) : 1.0f;

  uint32_t xu = x >= 0.0f ? uint32_t(x * 511 + 0.5f) : (1024U + uint32_t(-x * 511 + 0.5f));
  uint32_t yu = y >= 0.0f ? uint32_t(y * 511 + 0.5f) : (1024U + uint32_t(-y * 511 + 0.5f));
  uint32_t zu = z >= 0.0f ? uint32_t(z * 511 + 0.5f) : (1024U + uint32_t(-z * 511 + 0.5f));
  uint32_t wu = w >= 0.0f ? uint32_t(w * 1) : (2U + uint32_t(-z * 1));

  return (uint32_t(xu) << 0) | (uint32_t(yu) << 10) | (uint32_t(zu) << 20) | (uint32_t(wu) << 30);
}

Vec3f ConvertFromR11G11B10(uint32_t data);
uint32_t ConvertToR11G11B10(Vec3f data);

inline Vec4f ConvertFromB5G5R5A1(uint16_t data)
{
  return Vec4f((float)((data >> 10) & 0x1f) / 31.0f, (float)((data >> 5) & 0x1f) / 31.0f,
               (float)((data >> 0) & 0x1f) / 31.0f, ((data & 0x8000) > 0) ? 1.0f : 0.0f);
}

inline uint16_t ConvertToB5G5R5A1(Vec4f data)
{
  float x = data.x < 1.0f ? (data.x > 0.0f ? data.x : 0.0f) : 1.0f;
  float y = data.y < 1.0f ? (data.y > 0.0f ? data.y : 0.0f) : 1.0f;
  float z = data.z < 1.0f ? (data.z > 0.0f ? data.z : 0.0f) : 1.0f;
  float w = data.w < 1.0f ? (data.w > 0.0f ? data.w : 0.0f) : 1.0f;

  return (uint16_t(x * 0x1f + 0.5f) << 10) | (uint16_t(y * 0x1f + 0.5f) << 5) |
         (uint16_t(z * 0x1f + 0.5f) << 0) | (uint16_t(w * 0x1 + 0.5f) << 15);
}

inline Vec3f ConvertFromB5G6R5(uint16_t data)
{
  return Vec3f((float)((data >> 11) & 0x1f) / 31.0f, (float)((data >> 5) & 0x3f) / 63.0f,
               (float)((data >> 0) & 0x1f) / 31.0f);
}

inline uint16_t ConvertToB5G6R5(Vec3f data)
{
  float x = data.x < 1.0f ? (data.x > 0.0f ? data.x : 0.0f) : 1.0f;
  float y = data.y < 1.0f ? (data.y > 0.0f ? data.y : 0.0f) : 1.0f;
  float z = data.z < 1.0f ? (data.z > 0.0f ? data.z : 0.0f) : 1.0f;

  return (uint16_t(x * 0x1f + 0.5f) << 11) | (uint16_t(y * 0x3f + 0.5f) << 5) |
         (uint16_t(z * 0x1f + 0.5f) << 0);
}

inline Vec4f ConvertFromB4G4R4A4(uint16_t data)
{
  return Vec4f((float)((data >> 8) & 0xf) / 15.0f, (float)((data >> 4) & 0xf) / 15.0f,
               (float)((data >> 0) & 0xf) / 15.0f, (float)((data >> 12) & 0xf) / 15.0f);
}

inline uint16_t ConvertToB4G4R4A4(Vec4f data)
{
  float x = data.x < 1.0f ? (data.x > 0.0f ? data.x : 0.0f) : 1.0f;
  float y = data.y < 1.0f ? (data.y > 0.0f ? data.y : 0.0f) : 1.0f;
  float z = data.z < 1.0f ? (data.z > 0.0f ? data.z : 0.0f) : 1.0f;
  float w = data.w < 1.0f ? (data.w > 0.0f ? data.w : 0.0f) : 1.0f;

  return (uint16_t(x * 0xf + 0.5f) << 8) | (uint16_t(y * 0xf + 0.5f) << 4) |
         (uint16_t(z * 0xf + 0.5f) << 0) | (uint16_t(w * 0xf + 0.5f) << 12);
}

inline Vec4f ConvertFromR4G4(uint8_t data)
{
  return Vec4f((float)((data >> 0) & 0xf) / 15.0f, (float)((data >> 4) & 0xf) / 15.0f, 0.0f, 0.0f);
}

inline uint8_t ConvertToR4G4(Vec2f data)
{
  float x = data.x < 1.0f ? (data.x > 0.0f ? data.x : 0.0f) : 1.0f;
  float y = data.y < 1.0f ? (data.y > 0.0f ? data.y : 0.0f) : 1.0f;

  return (uint8_t(x * 0xf + 0.5f) << 0) | (uint8_t(y * 0xf + 0.5f) << 4);
}

Vec3f ConvertFromR9G9B9E5(uint32_t data);
uint32_t ConvertToR9G9B9E5(Vec3f data);

float ConvertFromSRGB8(uint8_t comp);
float ConvertSRGBToLinear(float srgbF);
Vec4f ConvertSRGBToLinear(Vec4f srgbF);
float ConvertLinearToSRGB(float linear);

typedef uint8_t byte;

struct ResourceFormat;
FloatVector DecodeFormattedComponents(const ResourceFormat &fmt, const byte *data,
                                      bool *success = NULL);
void EncodeFormattedComponents(const ResourceFormat &fmt, FloatVector v, byte *data,
                               bool *success = NULL);

void DecodePixelData(const ResourceFormat &srcFmt, const byte *data, PixelValue &out,
                     bool *success = NULL);
