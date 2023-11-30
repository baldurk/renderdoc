/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

Texture2D<float2> srcDepth : register(t0);
cbuffer ViewInput : register(b0)
{
  uint4 viewData;    // viewIndex, ???, ???, ???
};

void RENDERDOC_DepthCopyPS(float4 pos : SV_Position, out float depth : SV_Depth)
{
  int2 srcCoord = int2(pos.xy);
  depth = srcDepth.Load(int3(srcCoord, 0)).r;
}

Texture2DArray<float2> srcDepthArray : register(t0);

void RENDERDOC_DepthCopyArrayPS(float4 pos : SV_Position, out float depth : SV_Depth)
{
  int2 srcCoord = int2(pos.xy);
  depth = srcDepthArray.Load(int4(srcCoord, viewData.x, 0)).r;
}

Texture2DMS<float2> srcDepthMS : register(t0);

void RENDERDOC_DepthCopyMSPS(float4 pos
                             : SV_Position, uint sample
                             : SV_SampleIndex, out float depth
                             : SV_Depth)
{
  int2 srcCoord = int2(pos.xy);
  depth = srcDepthMS.Load(srcCoord, sample).r;
}

Texture2DMSArray<float2> srcDepthMSArray : register(t0);

void RENDERDOC_DepthCopyMSArrayPS(float4 pos
                                  : SV_Position, uint sample
                                  : SV_SampleIndex, out float depth
                                  : SV_Depth)
{
  int2 srcCoord = int2(pos.xy);
  depth = srcDepthMSArray.Load(int3(srcCoord, viewData.x), sample).r;
}
