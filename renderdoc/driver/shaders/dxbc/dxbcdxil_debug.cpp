/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include "dxbcdxil_debug.h"

namespace DXBCDXILDebug
{
// "NaN has special handling. If one source operand is NaN, then the other source operand is
// returned. If both are NaN, any NaN representation is returned."

float dxbc_min(float a, float b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a < b ? a : b;
}

double dxbc_min(double a, double b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a < b ? a : b;
}

float dxbc_max(float a, float b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a >= b ? a : b;
}

double dxbc_max(double a, double b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a >= b ? a : b;
}

float round_ne(float x)
{
  if(!RDCISFINITE(x))
    return x;

  float rem = remainderf(x, 1.0f);

  return x - rem;
}

float flush_denorm(const float f)
{
  uint32_t x;
  memcpy(&x, &f, sizeof(f));

  // if any bit is set in the exponent, it's not denormal
  if(x & 0x7F800000)
    return f;

  // keep only the sign bit
  x &= 0x80000000;
  float ret;
  memcpy(&ret, &x, sizeof(ret));
  return ret;
}

void get_sample_position(uint32_t sampleIndex, uint32_t sampleCount, float *position)
{
  // assume standard sample pattern - this might not hold in all cases
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx

  if(sampleIndex >= sampleCount)
  {
    // Per HLSL docs, if sampleIndex is out of bounds a zero vector is returned
    RDCWARN("sample index %u is out of bounds on resource bound to sample_pos (%u samples)",
            sampleIndex, sampleCount);
    position[0] = 0.0f;
    position[1] = 0.0f;
    position[2] = 0.0f;
    position[3] = 0.0f;
  }
  else
  {
    const float *sample_pattern = NULL;

// co-ordinates are given as (i,j) in 16ths of a pixel
#define _SMP(c) ((c) / 16.0f)

    if(sampleCount == 1)
    {
      sample_pattern = NULL;
    }
    else if(sampleCount == 2)
    {
      static const float pattern_2x[] = {
          _SMP(4.0f),
          _SMP(4.0f),
          _SMP(-4.0f),
          _SMP(-4.0f),
      };

      sample_pattern = &pattern_2x[0];
    }
    else if(sampleCount == 4)
    {
      static const float pattern_4x[] = {
          _SMP(-2.0f), _SMP(-6.0f), _SMP(6.0f), _SMP(-2.0f),
          _SMP(-6.0f), _SMP(2.0f),  _SMP(2.0f), _SMP(6.0f),
      };

      sample_pattern = &pattern_4x[0];
    }
    else if(sampleCount == 8)
    {
      static const float pattern_8x[] = {
          _SMP(1.0f),  _SMP(-3.0f), _SMP(-1.0f), _SMP(3.0f),  _SMP(5.0f),  _SMP(1.0f),
          _SMP(-3.0f), _SMP(-5.0f), _SMP(-5.0f), _SMP(5.0f),  _SMP(-7.0f), _SMP(-1.0f),
          _SMP(3.0f),  _SMP(7.0f),  _SMP(7.0f),  _SMP(-7.0f),
      };

      sample_pattern = &pattern_8x[0];
    }
    else if(sampleCount == 16)
    {
      static const float pattern_16x[] = {
          _SMP(1.0f),  _SMP(1.0f),  _SMP(-1.0f), _SMP(-3.0f), _SMP(-3.0f), _SMP(2.0f),  _SMP(4.0f),
          _SMP(-1.0f), _SMP(-5.0f), _SMP(-2.0f), _SMP(2.0f),  _SMP(5.0f),  _SMP(5.0f),  _SMP(3.0f),
          _SMP(3.0f),  _SMP(-5.0f), _SMP(-2.0f), _SMP(6.0f),  _SMP(0.0f),  _SMP(-7.0f), _SMP(-4.0f),
          _SMP(-6.0f), _SMP(-6.0f), _SMP(4.0f),  _SMP(-8.0f), _SMP(0.0f),  _SMP(7.0f),  _SMP(-4.0f),
          _SMP(6.0f),  _SMP(7.0f),  _SMP(-7.0f), _SMP(-8.0f),
      };

      sample_pattern = &pattern_16x[0];
    }
    else    // unsupported sample count
    {
      RDCERR("Unsupported sample count on resource for sample_pos: %u", sampleCount);
      sample_pattern = NULL;
    }

    if(sample_pattern == NULL)
    {
      position[0] = 0.0f;
      position[1] = 0.0f;
    }
    else
    {
      position[0] = sample_pattern[sampleIndex * 2 + 0];
      position[1] = sample_pattern[sampleIndex * 2 + 1];
    }
  }
#undef _SMP
}

};    // namespace DXBCDXILDebug

#if ENABLED(ENABLE_UNIT_TESTS)

#include <limits>
#include "catch/catch.hpp"

using namespace DXBCDXILDebug;

TEST_CASE("DXBCDXIL debugging helpers", "[program]")
{
  const float posinf = std::numeric_limits<float>::infinity();
  const float neginf = -std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float a = 1.0f;
  const float b = 2.0f;

  SECTION("dxbc_min")
  {
    CHECK(dxbc_min(neginf, neginf) == neginf);
    CHECK(dxbc_min(neginf, a) == neginf);
    CHECK(dxbc_min(neginf, posinf) == neginf);
    CHECK(dxbc_min(neginf, nan) == neginf);
    CHECK(dxbc_min(a, neginf) == neginf);
    CHECK(dxbc_min(a, b) == a);
    CHECK(dxbc_min(a, posinf) == a);
    CHECK(dxbc_min(a, nan) == a);
    CHECK(dxbc_min(posinf, neginf) == neginf);
    CHECK(dxbc_min(posinf, a) == a);
    CHECK(dxbc_min(posinf, posinf) == posinf);
    CHECK(dxbc_min(posinf, nan) == posinf);
    CHECK(dxbc_min(nan, neginf) == neginf);
    CHECK(dxbc_min(nan, a) == a);
    CHECK(dxbc_min(nan, posinf) == posinf);
    CHECK(RDCISNAN(dxbc_min(nan, nan)));
  };

  SECTION("dxbc_max")
  {
    CHECK(dxbc_max(neginf, neginf) == neginf);
    CHECK(dxbc_max(neginf, a) == a);
    CHECK(dxbc_max(neginf, posinf) == posinf);
    CHECK(dxbc_max(neginf, nan) == neginf);
    CHECK(dxbc_max(a, neginf) == a);
    CHECK(dxbc_max(a, b) == b);
    CHECK(dxbc_max(a, posinf) == posinf);
    CHECK(dxbc_max(a, nan) == a);
    CHECK(dxbc_max(posinf, neginf) == posinf);
    CHECK(dxbc_max(posinf, a) == posinf);
    CHECK(dxbc_max(posinf, posinf) == posinf);
    CHECK(dxbc_max(posinf, nan) == posinf);
    CHECK(dxbc_max(nan, neginf) == neginf);
    CHECK(dxbc_max(nan, a) == a);
    CHECK(dxbc_max(nan, posinf) == posinf);
    CHECK(RDCISNAN(dxbc_max(nan, nan)));
  };

  SECTION("test denorm flushing")
  {
    float foo = 3.141f;

    // check normal values
    CHECK(flush_denorm(0.0f) == 0.0f);
    CHECK(flush_denorm(foo) == foo);
    CHECK(flush_denorm(-foo) == -foo);

    // check NaN/inf values
    CHECK(RDCISNAN(flush_denorm(nan)));
    CHECK(flush_denorm(neginf) == neginf);
    CHECK(flush_denorm(posinf) == posinf);

    // check zero sign bit - bit more complex
    uint32_t negzero = 0x80000000U;
    float negzerof;
    memcpy(&negzerof, &negzero, sizeof(negzero));

    float flushed = flush_denorm(negzerof);
    CHECK(memcmp(&flushed, &negzerof, sizeof(negzerof)) == 0);

    // check that denormal values are flushed, preserving sign
    foo = 1.12104e-44f;
    CHECK(flush_denorm(foo) != foo);
    CHECK(flush_denorm(-foo) != -foo);
    CHECK(flush_denorm(foo) == 0.0f);
    flushed = flush_denorm(-foo);
    CHECK(memcmp(&flushed, &negzerof, sizeof(negzerof)) == 0);
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
