/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "hlsl_texsample.h"

struct v2f
{
  float4 pos : SV_Position;
  float4 tex : TEXCOORD0;
};

float4 RENDERDOC_TexRemapFloat(v2f IN) : SV_Target0
{
  float4 ret = SampleTextureFloat4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK,
                                   (ScalePS < 1 && MipLevel == 0), IN.tex.xy, Slice, MipLevel,
                                   SampleIdx, TextureResolutionPS, YUVDownsampleRate, YUVAChannels);

  ret = ((ret - RangeMinimum) * InverseRangeSize);

  return ret;
}

uint4 RENDERDOC_TexRemapUInt(v2f IN) : SV_Target0
{
  return SampleTextureUInt4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK, IN.tex.xy, Slice, MipLevel,
                            SampleIdx, TextureResolutionPS);
}

int4 RENDERDOC_TexRemapSInt(v2f IN) : SV_Target0
{
  return SampleTextureInt4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK, IN.tex.xy, Slice, MipLevel,
                           SampleIdx, TextureResolutionPS);
}
