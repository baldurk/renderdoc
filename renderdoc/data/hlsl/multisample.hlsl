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

// when serialising out a multisampled texture to disk from the app we're capturing
// (or serialising it back in, for that matter), we copy each sample to a slice in a
// 2DArray texture, and vice-versa. These shaders do that copy as appropriate.

cbuffer msarraycopyconsts : register(b0)
{
  uint sampleCount;
  uint currentStencil;
  uint currentSample;
  uint currentSlice;
};

Texture2DMSArray<uint4, 2> sourceMS2 : register(t1);
Texture2DMSArray<uint4, 4> sourceMS4 : register(t2);
Texture2DMSArray<uint4, 8> sourceMS8 : register(t3);
Texture2DMSArray<uint4, 16> sourceMS16 : register(t4);
Texture2DMSArray<uint4, 32> sourceMS32 : register(t5);

uint4 RENDERDOC_CopyMSToArray(float4 pos : SV_Position) : SV_Target0
{
  uint3 srcCoord = uint3(pos.x, pos.y, currentSlice);

  if(sampleCount == 2)
  {
    return sourceMS2.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 4)
  {
    return sourceMS4.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 8)
  {
    return sourceMS8.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 16)
  {
    return sourceMS16.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 32)
  {
    return sourceMS32.Load(srcCoord, currentSample);
  }

  return 0;
}

Texture2DMSArray<float4, 2> sourceFloatMS2 : register(t1);
Texture2DMSArray<float4, 4> sourceFloatMS4 : register(t2);
Texture2DMSArray<float4, 8> sourceFloatMS8 : register(t3);
Texture2DMSArray<float4, 16> sourceFloatMS16 : register(t4);
Texture2DMSArray<float4, 32> sourceFloatMS32 : register(t5);

float4 RENDERDOC_FloatCopyMSToArray(float4 pos : SV_Position) : SV_Target0
{
  uint3 srcCoord = uint3(pos.x, pos.y, currentSlice);

  if(sampleCount == 2)
  {
    return sourceFloatMS2.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 4)
  {
    return sourceFloatMS4.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 8)
  {
    return sourceFloatMS8.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 16)
  {
    return sourceFloatMS16.Load(srcCoord, currentSample);
  }
  else if(sampleCount == 32)
  {
    return sourceFloatMS32.Load(srcCoord, currentSample);
  }

  return 0;
}

Texture2DMSArray<float2, 2> sourceDepthMS2 : register(t1);
Texture2DMSArray<float2, 4> sourceDepthMS4 : register(t2);
Texture2DMSArray<float2, 8> sourceDepthMS8 : register(t3);
Texture2DMSArray<float2, 16> sourceDepthMS16 : register(t4);
Texture2DMSArray<float2, 32> sourceDepthMS32 : register(t5);

Texture2DMSArray<uint2, 2> sourceStencilMS2 : register(t11);
Texture2DMSArray<uint2, 4> sourceStencilMS4 : register(t12);
Texture2DMSArray<uint2, 8> sourceStencilMS8 : register(t13);
Texture2DMSArray<uint2, 16> sourceStencilMS16 : register(t14);
Texture2DMSArray<uint2, 32> sourceStencilMS32 : register(t15);

float RENDERDOC_DepthCopyMSToArray(float4 pos : SV_Position) : SV_Depth
{
  uint3 srcCoord = uint3(pos.x, pos.y, currentSlice);

  if(currentStencil < 256)
  {
    if(sampleCount == 2)
    {
      if(sourceStencilMS2.Load(srcCoord, currentSample).y != currentStencil)
        discard;
    }
    else if(sampleCount == 4)
    {
      if(sourceStencilMS4.Load(srcCoord, currentSample).y != currentStencil)
        discard;
    }
    else if(sampleCount == 8)
    {
      if(sourceStencilMS8.Load(srcCoord, currentSample).y != currentStencil)
        discard;
    }
    else if(sampleCount == 16)
    {
      if(sourceStencilMS16.Load(srcCoord, currentSample).y != currentStencil)
        discard;
    }
    else if(sampleCount == 32)
    {
      if(sourceStencilMS32.Load(srcCoord, currentSample).y != currentStencil)
        discard;
    }
  }

  if(sampleCount == 2)
  {
    return sourceDepthMS2.Load(srcCoord, currentSample).x;
  }
  else if(sampleCount == 4)
  {
    return sourceDepthMS4.Load(srcCoord, currentSample).x;
  }
  else if(sampleCount == 8)
  {
    return sourceDepthMS8.Load(srcCoord, currentSample).x;
  }
  else if(sampleCount == 16)
  {
    return sourceDepthMS16.Load(srcCoord, currentSample).x;
  }
  else if(sampleCount == 32)
  {
    return sourceDepthMS32.Load(srcCoord, currentSample).x;
  }

  return 0;
}

Texture2DArray<uint4> sourceArray : register(t1);

uint4 RENDERDOC_CopyArrayToMS(float4 pos
                              : SV_Position, uint curSample
                              : SV_SampleIndex)
    : SV_Target0
{
  uint4 srcCoord = uint4(pos.x, pos.y, currentSlice * sampleCount + curSample, 0);

  return sourceArray.Load(srcCoord);
}

Texture2DArray<float4> sourceFloatArray : register(t1);

float4 RENDERDOC_FloatCopyArrayToMS(float4 pos
                                    : SV_Position, uint curSample
                                    : SV_SampleIndex)
    : SV_Target0
{
  uint4 srcCoord = uint4(pos.x, pos.y, currentSlice * sampleCount + curSample, 0);

  return sourceFloatArray.Load(srcCoord);
}

Texture2DArray<float2> sourceDepthArray : register(t1);
Texture2DArray<uint2> sourceStencilArray : register(t11);

float RENDERDOC_DepthCopyArrayToMS(float4 pos
                                   : SV_Position, uint curSample
                                   : SV_SampleIndex)
    : SV_Depth
{
  uint4 srcCoord = uint4(pos.x, pos.y, currentSlice * sampleCount + curSample, 0);

  if(currentStencil < 1000)
  {
    if(sourceStencilArray.Load(srcCoord).y != currentStencil)
      discard;
  }

  return sourceDepthArray.Load(srcCoord).x;
}
