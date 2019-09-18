/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "formatpacking.h"
#include <float.h>
#include <math.h>
#include "common/common.h"

Vec3f ConvertFromR11G11B10(uint32_t data)
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

        const uint32_t hiddenBit = 0x40 >> (i == 2 ? 1 : 0);

        // shift until hidden bit is set
        while((mantissas[i] & hiddenBit) == 0)
        {
          mantissas[i] <<= 1;
          exponents[i]--;
        }

        // remove the hidden bit
        mantissas[i] &= ~hiddenBit;

        retu[i] = (exponents[i] + (127 - 15)) << 23 | mantissas[i] << mantissaShift[i];
      }
    }
  }

  return ret;
}

uint32_t ConvertToR11G11B10(Vec3f data)
{
  // convert each component to half first
  uint16_t halves[3] = {
      ConvertToHalf(data.x), ConvertToHalf(data.y), ConvertToHalf(data.z),
  };

  // extract mantissas, exponents, signs
  bool signs[3] = {
      (halves[0] & 0x8000) != 0, (halves[1] & 0x8000) != 0, (halves[2] & 0x8000) != 0,
  };
  uint32_t mantissas[3] = {
      (halves[0] & 0x03FFU), (halves[1] & 0x03FFU), (halves[2] & 0x03FFU),
  };
  uint32_t exponents[3] = {
      (halves[0] & 0x7C00U) >> 10, (halves[1] & 0x7C00U) >> 10, (halves[2] & 0x7C00U) >> 10,
  };

  // normalise NaN/inf values so we can truncate mantissa without converting NaN to inf
  for(int i = 0; i < 3; i++)
  {
    if(exponents[i] == 0x1f)
    {
      if(mantissas[i])
      {
        // if the mantissa is set, it's a NaN, set mantissa fully
        mantissas[i] = 0x3FF;
      }
      else
      {
        // otherwise it's inf. If it's negative inf, clamp to 0
        if(signs[i])
        {
          exponents[i] = 0;
          mantissas[i] = 0;
        }
      }
    }
    else
    {
      // clamp negative finite values to 0
      if(signs[i])
      {
        exponents[i] = 0;
        mantissas[i] = 0;
      }
    }
  }

  // truncate and encode
  return (mantissas[0] >> 4) << 0 | (mantissas[1] >> 4) << 11 | (mantissas[2] >> 5) << 22 |
         uint32_t(exponents[0]) << 6 | uint32_t(exponents[1]) << 17 | uint32_t(exponents[2]) << 27;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Check format conversion", "[format]")
{
  SECTION("Check half conversion is reflexive")
  {
    CHECK(std::isnan(ConvertFromHalf(ConvertToHalf(NAN))));
    CHECK(!std::isnan(ConvertFromHalf(ConvertToHalf(INFINITY))));
    CHECK(!std::isnan(ConvertFromHalf(ConvertToHalf(-INFINITY))));
    CHECK(!std::isfinite(ConvertFromHalf(ConvertToHalf(INFINITY))));
    CHECK(!std::isfinite(ConvertFromHalf(ConvertToHalf(-INFINITY))));

    for(uint16_t i = 0;; i++)
    {
      float f = ConvertFromHalf(i);
      uint16_t i2 = ConvertToHalf(f);

      // NaNs and infinites get mapped together. Just check that the value we get out is still a
      // nan/inf
      if(std::isnan(f))
      {
        float f2 = ConvertFromHalf(i2);
        CHECK(std::isnan(f2));
      }
      else if(!std::isfinite(f))
      {
        float f2 = ConvertFromHalf(i2);
        CHECK(!std::isfinite(f2));
      }
      else if(i == 0x8000)
      {
        // signed 0
        CHECK(i2 == 0);
      }
      else
      {
        CHECK(i == i2);
      }

      if(i == UINT16_MAX)
        break;
    }
  }

  SECTION("Check SRGB <-> Linear conversions are reflexive")
  {
    for(uint16_t i = 0;; i++)
    {
      float a = float(i) / float(UINT16_MAX);
      float b = ConvertLinearToSRGB(a);
      float c = ConvertSRGBToLinear(b);

      INFO(a);
      INFO(c);
      CHECK(abs(a - c) <= abs(a * 3.0f * FLT_EPSILON));

      if(i == UINT16_MAX)
        break;
    }
  };

  SECTION("Check Convert*R10G10B10A2 is reflexive")
  {
    for(uint32_t i = 0;; i++)
    {
      for(uint32_t a = 0; a < 4; a++)
      {
        uint32_t input = i | (a << 30);
        Vec4f vec = ConvertFromR10G10B10A2(input);
        uint32_t output = ConvertToR10G10B10A2(vec);

        CHECK(input == output);
      }

      // to reduce number of iterations we only do enough for red channel
      if(i == 0x400)
        break;
    }
  };

  SECTION("Check Convert*R11G11B11A2 is reflexive")
  {
    for(uint32_t i = 0;; i++)
    {
      // in the 11-bit red channel
      {
        uint32_t input = i << 0;
        Vec3f vec = ConvertFromR11G11B10(input);
        uint32_t output = ConvertToR11G11B10(vec);

        if(std::isnan(vec.x))
        {
          Vec3f vec2 = ConvertFromR11G11B10(output);
          CHECK(std::isnan(vec2.x));
        }
        else if(!std::isfinite(vec.x))
        {
          Vec3f vec2 = ConvertFromR11G11B10(output);
          CHECK(!std::isfinite(vec2.x));
        }
        else
        {
          CHECK(input == output);
        }
      }

      // in the 10-bit blue channel
      if(i < 0x400)
      {
        uint32_t input = i << 22;
        Vec3f vec = ConvertFromR11G11B10(input);
        uint32_t output = ConvertToR11G11B10(vec);

        if(std::isnan(vec.z))
        {
          Vec3f vec2 = ConvertFromR11G11B10(output);
          CHECK(std::isnan(vec2.z));
        }
        else if(!std::isfinite(vec.z))
        {
          Vec3f vec2 = ConvertFromR11G11B10(output);
          CHECK(!std::isfinite(vec2.z));
        }
        else
        {
          CHECK(input == output);
        }
      }

      // to reduce number of iterations we only do enough for each channel
      if(i == 0x800)
        break;
    }
  };

  SECTION("Spot test ConvertFromR11G11B10")
  {
#define R11G11B10(re, rm, ge, gm, be, bm)                                                   \
  ((uint32_t(re & 0x1f) << 6) | (uint32_t(ge & 0x1f) << 17) | (uint32_t(be & 0x1f) << 27) | \
   uint32_t(rm & 0x3f) << 0 | uint32_t(gm & 0x3f) << 11 | uint32_t(bm & 0x1f) << 22)

#define TEST11(e, m, f)                               \
  {                                                   \
    R11G11B10(e, m, 0, 0, 0, 0), Vec3f(f, 0.0f, 0.0f) \
  }
#define TEST10(e, m, f)                               \
  {                                                   \
    R11G11B10(0, 0, 0, 0, e, m), Vec3f(0.0f, 0.0f, f) \
  }

    rdcpair<uint32_t, Vec3f> tests[] = {
        {0x00000000, Vec3f(0.0f, 0.0f, 0.0f)},

        // test 11-bit decoding

        // normal values
        TEST11(0xf, 0, 1.0f),
        TEST11(0xf, 0x20, 1.5f),
        TEST11(0xf, 0x3f, 1.0f + float(0x3f) / float(0x40)),
        TEST11(0x10, 0x20, 3.0f),
        TEST11(0x10, 0, 2.0f),
        TEST11(0x10, 1, 2.0f + 1.0f / float(0x20)),
        TEST11(0xe, 0, 0.5f),
        TEST11(0xe, 1, 0.5f + 0.25f / float(0x20)),

        // maximum value - 0x7f is 0x3f with leading implicit 1. Then shifted by maximum exponent
        // 15, minus the 6 bits in 0x3f to get the fractional bits above 1.
        TEST11(0x1e, 0x3f, float(0x7f << (15 - 6))),

        // minimum normal value
        TEST11(0x1, 0, 1.0f / float(1 << 14)),

        // subnormal values
        TEST11(0, 0x1, float(0x1) / float(1 << (6 + 14))),
        TEST11(0, 0x3f, float(0x3f) / float(1 << (6 + 14))),

        // special values
        TEST11(0x1f, 0x20, NAN),
        TEST11(0x1f, 0x10, NAN),
        TEST11(0x1f, 0x1, NAN),
        TEST11(0x1f, 0, INFINITY),

        // test 10-bit decoding

        // normal values
        TEST10(0xf, 0, 1.0f),
        TEST10(0xf, 0x10, 1.5f),
        TEST10(0x10, 0x10, 3.0f),
        TEST10(0xf, 0x1f, 1.0f + float(0x1f) / float(0x20)),
        TEST10(0x10, 0, 2.0f),
        TEST10(0x10, 1, 2.0f + 1.0f / float(0x10)),
        TEST10(0xe, 0, 0.5f),
        TEST10(0xe, 1, 0.5f + 0.25f / float(0x10)),

        // maximum value - 0x3f is 0x1f with leading implicit 1. Then shifted by maximum exponent
        // 15, minus the 5 bits in 0x1f to get the fractional bits above 1.
        TEST10(0x1e, 0x1f, float(0x3f << (15 - 5))),

        // minimum normal value
        TEST10(0x1, 0, 1.0f / float(1 << 14)),

        // subnormal values
        TEST10(0, 0x1, float(0x1) / float(1 << (5 + 14))),
        TEST10(0, 0x1f, float(0x1f) / float(1 << (5 + 14))),

        // special values
        TEST10(0x1f, 0x10, NAN),
        TEST10(0x1f, 0x8, NAN),
        TEST10(0x1f, 0x1, NAN),
        TEST10(0x1f, 0, INFINITY),
    };

    for(rdcpair<uint32_t, Vec3f> test : tests)
    {
      INFO(test.first << " :: " << test.second.x << "," << test.second.y << "," << test.second.z);

      Vec3f conv = ConvertFromR11G11B10(test.first);

      if(std::isnan(conv.x))
      {
        CHECK(std::isnan(test.second.x));
      }
      else if(!std::isfinite(conv.x))
      {
        CHECK(!std::isfinite(test.second.x));
      }
      else if(std::isnan(conv.z))
      {
        CHECK(std::isnan(test.second.z));
      }
      else if(!std::isfinite(conv.z))
      {
        CHECK(!std::isfinite(test.second.z));
      }
      else
      {
        CHECK(memcmp(&conv, &test.second, sizeof(Vec3f)) == 0);
      }

      // don't need to test the other way as this was already covered in the reflexivity check above
    }
  };

  SECTION("Spot test ConvertToR11G11B10")
  {
#undef TEST11
#define TEST11(f, e, m)                                \
  {                                                    \
    Vec3f(f, 0.0f, 0.0f), R11G11B10(e, m, 0, 0, 0, 0), \
  }

#undef TEST10
#define TEST10(f, e, m)                                \
  {                                                    \
    Vec3f(0.0f, 0.0f, f), R11G11B10(0, 0, 0, 0, e, m), \
  }

    rdcpair<Vec3f, uint32_t> tests[] = {
        // zero is converted to 0
        {Vec3f(0.0f, 0.0f, 0.0f), 0x00000000},

        // test negative values should be clamped to 0
        TEST11(-1.0f, 0, 0),
        TEST11(-2.0f, 0, 0),
        TEST11(-0.5f, 0, 0),
        TEST11(-FLT_MAX, 0, 0),
        TEST11(-1.0e-9f, 0, 0),

        TEST10(-1.0f, 0, 0),
        TEST10(-2.0f, 0, 0),
        TEST10(-0.5f, 0, 0),
        TEST10(-FLT_MAX, 0, 0),
        TEST10(-1.0e-9f, 0, 0),

        // values too small to represent are also clamped to 0
        TEST11(1.0e-9f, 0, 0),
        TEST11(1.0e-7f, 0, 0),
        TEST10(1.0e-9f, 0, 0),
        TEST10(1.0e-7f, 0, 0),

        // minimum value is converted to subnormal, regardless of mantissa
        TEST11(float(0x1) / float(1 << (6 + 14)), 0, 0x1),
        TEST10(float(0x1) / float(1 << (5 + 14)), 0, 0x1),
        TEST11(float(0x3f) / float(1 << (6 + 14)), 0, 0x3f),
        TEST10(float(0x1f) / float(1 << (5 + 14)), 0, 0x1f),

        // first non-subnormal values
        TEST11(1.0f / float(1 << 14), 1, 0),
        TEST10(1.0f / float(1 << 14), 1, 0),

        // normal float values too small to represent get rounded down
        TEST11(0.5f / float(1 << 14), 0, 0x20),
        TEST10(0.5f / float(1 << 14), 0, 0x10),

        // normal values
        TEST11(1.0f, 0xf, 0x0),
        TEST10(1.0f, 0xf, 0x0),
        TEST11(1.5f, 0xf, 0x20),
        TEST10(1.5f, 0xf, 0x10),
    };

    for(rdcpair<Vec3f, uint32_t> test : tests)
    {
      uint32_t conv = ConvertToR11G11B10(test.first);

      CHECK(conv == test.second);
    }
  };
}

#endif
