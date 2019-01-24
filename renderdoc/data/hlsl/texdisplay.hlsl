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

#include "hlsl_cbuffers.h"
#include "hlsl_texsample.h"

float ConvertSRGBToLinear(float srgb)
{
  if(srgb <= 0.04045f)
    return srgb / 12.92f;
  else
    return pow((saturate(srgb) + 0.055f) / 1.055f, 2.4f);
}

struct v2f
{
  float4 pos : SV_Position;
  float4 tex : TEXCOORD0;
};

v2f RENDERDOC_TexDisplayVS(uint vertID : SV_VertexID)
{
  v2f OUT = (v2f)0;

  float2 positions[] = {
      float2(0.0f, 0.0f),  float2(0.0f, -1.0f), float2(1.0f, 0.0f),
      float2(1.0f, -1.0f), float2(0.0f, 0.0f),
  };

  float2 pos = positions[vertID];

  OUT.pos = float4(Position.xy + pos.xy * TextureResolution.xy * Scale * ScreenAspect.xy, 0, 1) -
            float4(1.0, -1.0, 0, 0);
  OUT.tex.xy = float2(pos.x, -pos.y);
  return OUT;
}

// main texture display shader, used for the texture viewer. It samples the right resource
// for the type and applies things like the range check and channel masking.
// It also does a couple of overlays that we can get 'free' like NaN/inf checks
// or range clipping
float4 RENDERDOC_TexDisplayPS(v2f IN) : SV_Target0
{
  bool uintTex = OutputDisplayFormat & TEXDISPLAY_UINT_TEX;
  bool sintTex = OutputDisplayFormat & TEXDISPLAY_SINT_TEX;

  float4 col = 0;
  uint4 ucol = 0;
  int4 scol = 0;

  float2 uvTex = IN.tex.xy;

  if(FlipY)
    uvTex.y = 1.0f - uvTex.y;

  if(uintTex)
  {
    ucol = SampleTextureUInt4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK, uvTex, Slice, MipLevel,
                              SampleIdx, TextureResolutionPS);
  }
  else if(sintTex)
  {
    scol = SampleTextureInt4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK, uvTex, Slice, MipLevel,
                             SampleIdx, TextureResolutionPS);
  }
  else
  {
    col = SampleTextureFloat4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK,
                              (ScalePS < 1 && MipLevel == 0), uvTex, Slice, MipLevel, SampleIdx,
                              TextureResolutionPS, YUVDownsampleRate, YUVAChannels);
  }

  if(RawOutput)
  {
    if(uintTex)
      return asfloat(ucol);
    else if(sintTex)
      return asfloat(scol);
    else
      return col;
  }

  if(WireframeColour.y > 0.0f)
  {
    // assume col is now YUVA, perform a default conversion to RGBA
    const float Kr = 0.2126f;
    const float Kb = 0.0722f;

    float L = col.g;
    float Pb = col.b - 0.5f;
    float Pr = col.r - 0.5f;

    col.b = L + (Pb / 0.5f) * (1 - Kb);
    col.r = L + (Pr / 0.5f) * (1 - Kr);
    col.g = (L - Kr * col.r - Kb * col.b) / (1.0f - Kr - Kb);
  }

  if(HeatmapMode)
  {
    if(HeatmapMode == HEATMAP_LINEAR)
    {
      // cast the float value to an integer with safe rounding, then return the
      int bucket = int(floor(col.x + 0.25f));

      bucket = max(bucket, 0);
      bucket = min(bucket, HEATMAP_RAMPSIZE - 1);

      return ColorRamp[bucket];
    }
    else if(HeatmapMode == HEATMAP_TRISIZE)
    {
      // uninitialised regions have alpha=0
      if(col.w < 0.5f)
        return ColorRamp[0];

      float area = max(col.x, 0.001f);

      int bucket = 2 + int(floor(20.0f - 20.1f * (1.0f - exp(-0.4f * area))));

      return ColorRamp[bucket];
    }
    else
    {
      // error! invalid heatmap mode
    }
  }

  // RGBM encoding
  if(WireframeColour.x > 0.0f)
  {
    if(uintTex)
      ucol = float4(ucol.rgb * ucol.a * (uint)(WireframeColour.x), 1.0f);
    else if(sintTex)
      scol = float4(scol.rgb * scol.a * (int)(WireframeColour.x), 1.0f);
    else
      col = float4(col.rgb * col.a * WireframeColour.x, 1.0f);
  }

  if(uintTex)
    col = (float4)(ucol);
  else if(sintTex)
    col = (float4)(scol);

  float4 pre_range_col = col;

  col = ((col - RangeMinimum) * InverseRangeSize);

  // workaround for D3DCompiler bug. For some reason it assumes texture samples can
  // never come back as NaN, so involving a cbuffer value like this here ensures
  // the below isnan()s don't get optimised out.
  if(Channels.x < 0.5f)
    col.x = pre_range_col.x = AlwaysZero;
  if(Channels.y < 0.5f)
    col.y = pre_range_col.y = AlwaysZero;
  if(Channels.z < 0.5f)
    col.z = pre_range_col.z = AlwaysZero;
  if(Channels.w < 0.5f)
    col.w = pre_range_col.w = 1.0f - AlwaysZero;

  // show nans, infs and negatives
  if(OutputDisplayFormat & TEXDISPLAY_NANS)
  {
    if(isnan(pre_range_col.r) || isnan(pre_range_col.g) || isnan(pre_range_col.b) ||
       isnan(pre_range_col.a))
      return float4(1, 0, 0, 1);

    if(isinf(pre_range_col.r) || isinf(pre_range_col.g) || isinf(pre_range_col.b) ||
       isinf(pre_range_col.a))
      return float4(0, 1, 0, 1);

    if(pre_range_col.r < 0 || pre_range_col.g < 0 || pre_range_col.b < 0 || pre_range_col.a < 0)
      return float4(0, 0, 1, 1);

    col = float4(dot(col.xyz, float3(0.2126, 0.7152, 0.0722)).xxx, 1);
  }
  else if(OutputDisplayFormat & TEXDISPLAY_CLIPPING)
  {
    if(col.r < 0 || col.g < 0 || col.b < 0 || col.a < 0)
      return float4(1, 0, 0, 1);

    if(col.r > (1 + FLT_EPSILON) || col.g > (1 + FLT_EPSILON) || col.b > (1 + FLT_EPSILON) ||
       col.a > (1 + FLT_EPSILON))
      return float4(0, 1, 0, 1);

    col = float4(dot(col.xyz, float3(0.2126, 0.7152, 0.0722)).xxx, 1);
  }
  else
  {
    // if only one channel is selected
    if(dot(Channels, 1) == 1)
    {
      // if it's alpha, just move it into rgb
      // otherwise, select the channel that's on and replicate it across all channels
      if(Channels.a == 1)
        col = float4(col.aaa, 1);
      else
        col = float4(dot(col.rgb, 1).xxx, 1);
    }
  }

  if(OutputDisplayFormat & TEXDISPLAY_GAMMA_CURVE)
  {
    col.rgb =
        float3(ConvertSRGBToLinear(col.r), ConvertSRGBToLinear(col.g), ConvertSRGBToLinear(col.b));
  }

  return col;
}
