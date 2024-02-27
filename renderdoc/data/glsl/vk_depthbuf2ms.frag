/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "glsl_globals.h"

layout(binding = 2, std430) readonly buffer srcBuf
{
  uint srcData[];
};

layout(push_constant) uniform multisamplePush
{
  int numMultiSamples;
  int format;
  int currentSlice;
  uint currentStencil;
  int textureWidth;
  int textureHeight;
}
mscopy;

#define MAX_D16 ((1 << 16) - 1)
#define MAX_D24 ((1 << 24) - 1)

#define D16ToFloat(depth) (float(depth) / float(MAX_D16))
#define D24ToFloat(depth) (float(depth) / float(MAX_D24))

#define numMultiSamples (mscopy.numMultiSamples)
#define format (mscopy.format)
#define currentSlice (mscopy.currentSlice)
#define currentStencil (mscopy.currentStencil)
#define textureWidth (mscopy.textureWidth)
#define textureHeight (mscopy.textureHeight)

void main()
{
  ivec3 srcCoord =
      ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), currentSlice * numMultiSamples + gl_SampleID);
  uint idx = srcCoord.x + textureWidth * (srcCoord.y + (textureHeight * srcCoord.z));

  float depth = 0;
  uint stencil = 0;
  if(format == SHADER_D16_UNORM)
  {
    uint data = srcData[idx / 2];
    if(idx % 2 == 0)
    {
      depth = D16ToFloat(data & 0xFFFF);
    }
    else
    {
      depth = D16ToFloat((data >> 16) & 0xFFFF);
    }
  }
  else if(format == SHADER_D16_UNORM_S8_UINT)
  {
    uint data = srcData[idx];
    depth = D16ToFloat(data & 0xFFFF);
    stencil = (data >> 16) & 0xFF;
  }
  else if(format == SHADER_X8_D24_UNORM_PACK32)
  {
    uint data = srcData[idx];
    depth = D24ToFloat(data & 0xFFFFFF);
  }
  else if(format == SHADER_D24_UNORM_S8_UINT)
  {
    uint data = srcData[idx];
    depth = D24ToFloat(data & 0xFFFFFF);
    stencil = (data >> 24) & 0xFF;
  }
  else if(format == SHADER_D32_SFLOAT)
  {
    uint data = srcData[idx];
    depth = uintBitsToFloat(data);
  }
  else if(format == SHADER_D32_SFLOAT_S8_UINT)
  {
    uint data = srcData[idx * 2];
    depth = uintBitsToFloat(data);
    uint stencilData = srcData[idx * 2 + 1];
    stencil = stencilData & 0xFF;
  }
  else if(format == SHADER_S8_UINT)
  {
    uint stencilData = srcData[idx / 4];
    if((idx % 4) == 0)
    {
      stencil = stencilData & 0x000000FF;
    }
    else if((idx % 4) == 1)
    {
      stencil = (stencilData & 0x0000FF00) >> 8;
    }
    else if((idx % 4) == 2)
    {
      stencil = (stencilData & 0x00FF0000) >> 16;
    }
    else
    {
      stencil = (stencilData & 0xFF000000) >> 24;
    }
  }

  if(currentStencil < 256u)
  {
    if(stencil != currentStencil)
      discard;
  }

  gl_FragDepth = depth;
}
