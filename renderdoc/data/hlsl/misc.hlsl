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

#include "hlsl_cbuffers.h"

struct MultipleOutput
{
  float4 col0 : SV_Target0;
  float4 col1 : SV_Target1;
  float4 col2 : SV_Target2;
  float4 col3 : SV_Target3;
  float4 col4 : SV_Target4;
  float4 col5 : SV_Target5;
  float4 col6 : SV_Target6;
  float4 col7 : SV_Target7;
};

float4 RENDERDOC_FullscreenVS(uint id : SV_VertexID) : SV_Position
{
  float4 pos[] = {float4(-1.0f, 1.0f, 0.0f, 1.0f), float4(3.0f, 1.0f, 0.0f, 1.0f),
                  float4(-1.0f, -3.0f, 0.0f, 1.0f)};

  return pos[id];
}

cbuffer overlayconsts : register(b0)
{
  float4 overlaycol;
};

MultipleOutput RENDERDOC_FixedColPS()
{
  MultipleOutput OUT = (MultipleOutput)0;

  OUT.col0 = OUT.col1 = OUT.col2 = OUT.col3 = OUT.col4 = OUT.col5 = OUT.col6 = OUT.col7 = overlaycol;

  return OUT;
}

float4 RENDERDOC_CheckerboardPS(float4 pos : SV_Position) : SV_Target0
{
  float2 RectRelativePos = pos.xy - RectPosition;

  // if we have a border, and our pos is inside the border, return inner color
  if(BorderWidth >= 0.0f)
  {
    if(RectRelativePos.x >= BorderWidth && RectRelativePos.x <= RectSize.x - BorderWidth &&
       RectRelativePos.y >= BorderWidth && RectRelativePos.y <= RectSize.y - BorderWidth)
    {
      return InnerColor;
    }
  }

  float2 ab = fmod(RectRelativePos.xy, (CheckerSquareDimension * 2.0f).xx);

  bool checkerVariant = ((ab.x < CheckerSquareDimension && ab.y < CheckerSquareDimension) ||
                         (ab.x > CheckerSquareDimension && ab.y > CheckerSquareDimension));

  // otherwise return checker pattern
  return checkerVariant ? PrimaryColor : SecondaryColor;
}

#define PATTERN_WIDTH 64
#define PATTERN_HEIGHT 8

cbuffer discarddata : register(b0)
{
  float4 floatpattern[(PATTERN_WIDTH * PATTERN_HEIGHT) / 4];
  uint4 intpattern[(PATTERN_WIDTH * PATTERN_HEIGHT) / 4];
};

cbuffer discardopts : register(b1)
{
  uint discardPass;
};

float4 RENDERDOC_DiscardFloatPS(float4 pos : SV_Position, out float depth : SV_Depth) : SV_Target0
{
  uint x = uint(pos.x) % PATTERN_WIDTH;
  uint y = uint(pos.y) % PATTERN_HEIGHT;

  uint idx = ((y * 64) + x);

  float val = floatpattern[idx / 4][idx % 4];

  if(discardPass == 1 && val >= 0.5f)
    clip(-1);
  else if(discardPass == 2 && val < 0.5f)
    clip(-1);

  depth = saturate(val);

  return val.xxxx;
}

uint4 RENDERDOC_DiscardIntPS(float4 pos : SV_Position, out float depth : SV_Depth) : SV_Target0
{
  uint x = uint(pos.x) % PATTERN_WIDTH;
  uint y = uint(pos.y) % PATTERN_HEIGHT;

  uint idx = ((y * 64) + x);

  uint val = intpattern[idx / 4][idx % 4];

  if(discardPass == 1 && val > 0)
    clip(-1);
  else if(discardPass == 2 && val == 0)
    clip(-1);

  depth = saturate(float(val));

  return val.xxxx;
}

cbuffer executepatchdata : register(b0)
{
  uint argCount;
  uint bufCount;
  uint argStride;
  uint4 argOffsets[32];
};

cbuffer countbuffer : register(b1)
{
  uint numExecutes;
};

cbuffer countbuffer : register(b2)
{
  uint maxNumExecutes;
};

struct buffermapping
{
  // {.x = LSB, .y = MSB} to match uint64 order
  uint2 origBase;
  uint2 origEnd;
  uint2 newBase;
  uint2 pad;
};

StructuredBuffer<buffermapping> buffers : register(t0);
RWByteAddressBuffer arguments : register(u0);

bool uint64LessThan(uint2 a, uint2 b)
{
  // either MSB is less, or MSB is equal and LSB is less-equal
  return a.y < b.y || (a.y == b.y && a.x < b.x);
}

bool uint64LessEqual(uint2 a, uint2 b)
{
  return uint64LessThan(a, b) || (a.y == b.y && a.x == b.x);
}

uint2 uint64Add(uint2 a, uint2 b)
{
  uint msb = 0, lsb = 0;
  if(b.x > 0 && a.x > 0xffffffff - b.x)
  {
    uint x = max(a.x, b.x) - 0x80000000;
    uint y = min(a.x, b.x);

    uint sum = x + y;

    msb = a.y + b.y + 1;
    lsb = sum - 0x80000000;
  }
  else
  {
    msb = a.y + b.y;
    lsb = a.x + b.x;
  }

  return uint2(lsb, msb);
}

uint2 uint64Sub(uint2 a, uint2 b)
{
  uint msb = 0, lsb = 0;
  if(a.x < b.x)
  {
    uint diff = b.x - a.x;

    msb = a.y - b.y - 1;
    lsb = 0xffffffff - (diff - 1);
  }
  else
  {
    msb = a.y - b.y;
    lsb = a.x - b.x;
  }

  return uint2(lsb, msb);
}

uint2 PatchAddress(uint2 addr)
{
  for(uint i = 0; i < bufCount; i++)
  {
    buffermapping b = buffers[i];

    if(uint64LessEqual(b.origBase, addr) && uint64LessThan(addr, b.origEnd))
    {
      return uint64Add(b.newBase, uint64Sub(addr, b.origBase));
    }
  }

  return addr;
}

[numthreads(128, 1, 1)] void RENDERDOC_ExecuteIndirectPatchCS(uint idx
                                                              : SV_GroupIndex) {
  if(idx < argCount)
  {
    for(uint i = 0; i < min(numExecutes, maxNumExecutes); i++)
    {
      uint offs = argStride * i + argOffsets[idx / 4][idx % 4];

      arguments.Store2(offs, PatchAddress(arguments.Load2(offs)));
    }
  }
}
