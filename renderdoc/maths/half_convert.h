///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#pragma once

inline uint16_t ConvertToHalf(float comp)
{
  int *alias = (int *)&comp;
  int i = *alias;

  int sign = (i >> 16) & 0x00008000;
  int exponent = ((i >> 23) & 0x000000ff) - (127 - 15);
  int mantissa = i & 0x007fffff;

  if(exponent <= 0)
  {
    if(exponent < -10)
      return sign & 0xffff;

    mantissa |= 0x00800000;

    int t = 14 - exponent;
    int a = (1 << (t - 1)) - 1;
    int b = (mantissa >> t) & 1;

    mantissa = (mantissa + a + b) >> t;

    return (sign | mantissa) & 0xffff;
  }
  else if(exponent == 0xff - (127 - 15))
  {
    if(mantissa == 0)
      return (sign | 0x7c00) & 0xffff;

    mantissa >>= 13;
    return (sign | 0x7c00 | mantissa | (mantissa == 0)) & 0xffff;
  }
  else
  {
    mantissa = mantissa + 0x00000fff + ((mantissa >> 13) & 1);

    if(mantissa & 0x00800000)
    {
      mantissa = 0;
      exponent += 1;
    }

    if(exponent > 30)
    {
      return (sign | 0x7c00) & 0xffff;
    }

    return (sign | (exponent << 10) | (mantissa >> 13)) & 0xffff;
  }
}

inline float ConvertFromHalf(uint16_t comp)
{
  bool sign = (comp & 0x8000) != 0;
  int exponent = (comp & 0x7C00) >> 10;
  int mantissa = comp & 0x03FF;

  if(exponent == 0x00)
  {
    if(mantissa == 0)
      return 0.0f;

    // subnormal
    float ret = (float)mantissa;
    int *alias = (int *)&ret;

    // set sign bit and set exponent to 2^-24
    // (2^-14 from spec for subnormals * 2^-10 to convert (float)mantissa to 0.mantissa)
    *alias = (sign ? 0x80000000 : 0) | (*alias - (24 << 23));

    return ret;
  }
  else if(exponent < 0x1f)
  {
    exponent -= 15;

    float ret = 0.0f;
    int *alias = (int *)&ret;

    // convert to float. Put sign bit in the right place, convert exponent to be
    // [-128,127] and put in the right place, then shift mantissa up.
    *alias = (sign ? 0x80000000 : 0) | (exponent + 127) << 23 | (mantissa << 13);

    return ret;
  }
  else    // if(exponent = 0x1f)
  {
    union
    {
      int i;
      float f;
    } nan;
    nan.i = 0x7F800001;
    return nan.f;
  }
}
