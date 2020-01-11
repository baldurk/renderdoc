/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "api/replay/data_types.h"
#include "api/replay/rdcpair.h"
#include "common/common.h"
#include "os/os_specific.h"

//	for(int i=0; i < 256; i++)
//	{
//		uint8_t comp = i&0xff;
//		float srgbF = float(comp)/255.0f;
//
//		if(srgbF <= 0.04045f)
//		  SRGB8_lookuptable[comp] = srgbF/12.92f;
//		else
//		  SRGB8_lookuptable[comp] = powf((0.055f + srgbF) / 1.055f, 2.4f);
//	}

float SRGB8_lookuptable[256] = {
    0.000000f, 0.000304f, 0.000607f, 0.000911f, 0.001214f, 0.001518f, 0.001821f, 0.002125f,
    0.002428f, 0.002732f, 0.003035f, 0.003347f, 0.003677f, 0.004025f, 0.004391f, 0.004777f,
    0.005182f, 0.005605f, 0.006049f, 0.006512f, 0.006995f, 0.007499f, 0.008023f, 0.008568f,
    0.009134f, 0.009721f, 0.010330f, 0.010960f, 0.011612f, 0.012286f, 0.012983f, 0.013702f,
    0.014444f, 0.015209f, 0.015996f, 0.016807f, 0.017642f, 0.018500f, 0.019382f, 0.020289f,
    0.021219f, 0.022174f, 0.023153f, 0.024158f, 0.025187f, 0.026241f, 0.027321f, 0.028426f,
    0.029557f, 0.030713f, 0.031896f, 0.033105f, 0.034340f, 0.035601f, 0.036889f, 0.038204f,
    0.039546f, 0.040915f, 0.042311f, 0.043735f, 0.045186f, 0.046665f, 0.048172f, 0.049707f,
    0.051269f, 0.052861f, 0.054480f, 0.056128f, 0.057805f, 0.059511f, 0.061246f, 0.063010f,
    0.064803f, 0.066626f, 0.068478f, 0.070360f, 0.072272f, 0.074214f, 0.076185f, 0.078187f,
    0.080220f, 0.082283f, 0.084376f, 0.086500f, 0.088656f, 0.090842f, 0.093059f, 0.095307f,
    0.097587f, 0.099899f, 0.102242f, 0.104616f, 0.107023f, 0.109462f, 0.111932f, 0.114435f,
    0.116971f, 0.119538f, 0.122139f, 0.124772f, 0.127438f, 0.130136f, 0.132868f, 0.135633f,
    0.138432f, 0.141263f, 0.144128f, 0.147027f, 0.149960f, 0.152926f, 0.155926f, 0.158961f,
    0.162029f, 0.165132f, 0.168269f, 0.171441f, 0.174647f, 0.177888f, 0.181164f, 0.184475f,
    0.187821f, 0.191202f, 0.194618f, 0.198069f, 0.201556f, 0.205079f, 0.208637f, 0.212231f,
    0.215861f, 0.219526f, 0.223228f, 0.226966f, 0.230740f, 0.234551f, 0.238398f, 0.242281f,
    0.246201f, 0.250158f, 0.254152f, 0.258183f, 0.262251f, 0.266356f, 0.270498f, 0.274677f,
    0.278894f, 0.283149f, 0.287441f, 0.291771f, 0.296138f, 0.300544f, 0.304987f, 0.309469f,
    0.313989f, 0.318547f, 0.323143f, 0.327778f, 0.332452f, 0.337164f, 0.341914f, 0.346704f,
    0.351533f, 0.356400f, 0.361307f, 0.366253f, 0.371238f, 0.376262f, 0.381326f, 0.386430f,
    0.391573f, 0.396755f, 0.401978f, 0.407240f, 0.412543f, 0.417885f, 0.423268f, 0.428691f,
    0.434154f, 0.439657f, 0.445201f, 0.450786f, 0.456411f, 0.462077f, 0.467784f, 0.473532f,
    0.479320f, 0.485150f, 0.491021f, 0.496933f, 0.502887f, 0.508881f, 0.514918f, 0.520996f,
    0.527115f, 0.533276f, 0.539480f, 0.545725f, 0.552011f, 0.558340f, 0.564712f, 0.571125f,
    0.577581f, 0.584078f, 0.590619f, 0.597202f, 0.603827f, 0.610496f, 0.617207f, 0.623960f,
    0.630757f, 0.637597f, 0.644480f, 0.651406f, 0.658375f, 0.665387f, 0.672443f, 0.679543f,
    0.686685f, 0.693872f, 0.701102f, 0.708376f, 0.715694f, 0.723055f, 0.730461f, 0.737911f,
    0.745404f, 0.752942f, 0.760525f, 0.768151f, 0.775822f, 0.783538f, 0.791298f, 0.799103f,
    0.806952f, 0.814847f, 0.822786f, 0.830770f, 0.838799f, 0.846873f, 0.854993f, 0.863157f,
    0.871367f, 0.879622f, 0.887923f, 0.896269f, 0.904661f, 0.913099f, 0.921582f, 0.930111f,
    0.938686f, 0.947307f, 0.955974f, 0.964686f, 0.973445f, 0.982251f, 0.991102f, 1.000000f,
};

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

float ConvertFromSRGB8(uint8_t comp)
{
  return SRGB8_lookuptable[comp];
}

float ConvertSRGBToLinear(float srgbF)
{
  if(srgbF <= 0.04045f)
    return srgbF / 12.92f;

  if(srgbF < 0.0f)
    srgbF = 0.0f;
  else if(srgbF > 1.0f)
    srgbF = 1.0f;

  return powf((0.055f + srgbF) / 1.055f, 2.4f);
}

Vec4f ConvertSRGBToLinear(Vec4f srgbF)
{
  return Vec4f(ConvertSRGBToLinear(srgbF.x), ConvertSRGBToLinear(srgbF.y),
               ConvertSRGBToLinear(srgbF.z), srgbF.w);
}

float ConvertLinearToSRGB(float linear)
{
  if(linear <= 0.0031308f)
    return 12.92f * linear;

  if(linear < 0.0f)
    linear = 0.0f;
  else if(linear > 1.0f)
    linear = 1.0f;

  return 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
}

float ConvertComponent(const ResourceFormat &fmt, const byte *data)
{
  if(fmt.compByteWidth == 8)
  {
    // we just downcast
    const uint64_t *u64 = (const uint64_t *)data;
    const int64_t *i64 = (const int64_t *)data;

    if(fmt.compType == CompType::Double || fmt.compType == CompType::Float)
    {
      return float(*(const double *)u64);
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u64);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i64);
    }
  }
  else if(fmt.compByteWidth == 4)
  {
    const uint32_t *u32 = (const uint32_t *)data;
    const int32_t *i32 = (const int32_t *)data;

    if(fmt.compType == CompType::Float || fmt.compType == CompType::Depth)
    {
      return *(const float *)u32;
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u32);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i32);
    }
  }
  else if(fmt.compByteWidth == 3 && fmt.compType == CompType::Depth)
  {
    // 24-bit depth is a weird edge case we need to assemble it by hand
    const uint8_t *u8 = (const uint8_t *)data;

    uint32_t depth = 0;
    depth |= uint32_t(u8[1]);
    depth |= uint32_t(u8[2]) << 8;
    depth |= uint32_t(u8[3]) << 16;

    return float(depth) / float(16777215.0f);
  }
  else if(fmt.compByteWidth == 2)
  {
    const uint16_t *u16 = (const uint16_t *)data;
    const int16_t *i16 = (const int16_t *)data;

    if(fmt.compType == CompType::Float)
    {
      return ConvertFromHalf(*u16);
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u16);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i16);
    }
    // 16-bit depth is UNORM
    else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::Depth)
    {
      return float(*u16) / 65535.0f;
    }
    else if(fmt.compType == CompType::SNorm)
    {
      float f = -1.0f;

      if(*i16 == -32768)
        f = -1.0f;
      else
        f = ((float)*i16) / 32767.0f;

      return f;
    }
  }
  else if(fmt.compByteWidth == 1)
  {
    const uint8_t *u8 = (const uint8_t *)data;
    const int8_t *i8 = (const int8_t *)data;

    if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u8);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i8);
    }
    else if(fmt.compType == CompType::UNormSRGB)
    {
      return SRGB8_lookuptable[*u8];
    }
    else if(fmt.compType == CompType::UNorm)
    {
      return float(*u8) / 255.0f;
    }
    else if(fmt.compType == CompType::SNorm)
    {
      float f = -1.0f;

      if(*i8 == -128)
        f = -1.0f;
      else
        f = ((float)*i8) / 127.0f;

      return f;
    }
  }

  RDCERR("Unexpected format to convert from %u %u", fmt.compByteWidth, fmt.compType);

  return 0.0f;
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
