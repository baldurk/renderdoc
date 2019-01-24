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

// this file provides a couple of functions that, given the basic type, will go and
// figure out which resource to sample from and load from it then return the value

SamplerState pointSampler : register(s0);
SamplerState linearSampler : register(s1);

Texture1DArray<float4> texDisplayTex1DArray : register(t1);
Texture2DArray<float4> texDisplayTex2DArray : register(t2);
Texture3D<float4> texDisplayTex3D : register(t3);
Texture2DArray<float2> texDisplayTexDepthArray : register(t4);
Texture2DArray<uint2> texDisplayTexStencilArray : register(t5);
Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(t6);
Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(t7);
Texture2DMSArray<float4> texDisplayTex2DMSArray : register(t9);
Texture2DArray<float4> texDisplayYUVArray : register(t10);

Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);
Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);
Texture3D<uint4> texDisplayUIntTex3D : register(t13);
Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(t19);

Texture1DArray<int4> texDisplayIntTex1DArray : register(t21);
Texture2DArray<int4> texDisplayIntTex2DArray : register(t22);
Texture3D<int4> texDisplayIntTex3D : register(t23);
Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(t29);

uint4 SampleTextureUInt4(uint type, float2 uv, float slice, float mip, int sample, float3 texRes)
{
  uint4 col = 0;

  if(type == RESTYPE_TEX1D)
    col = texDisplayUIntTex1DArray.Load(int3(uv.x * texRes.x, slice, mip));
  else if(type == RESTYPE_TEX3D)
    col = texDisplayUIntTex3D.Load(int4(uv.xy * texRes.xy, slice + 0.001f, mip));
  else if(type == RESTYPE_TEX2D)
    col = texDisplayUIntTex2DArray.Load(int4(uv.xy * texRes.xy, slice, mip));
  else if(type == RESTYPE_TEX2D_MS)
  {
    if(sample < 0)
      sample = 0;
    col = texDisplayUIntTex2DMSArray.Load(int3(uv.xy * texRes.xy, slice), sample);
  }

  return col;
}

int4 SampleTextureInt4(uint type, float2 uv, float slice, float mip, int sample, float3 texRes)
{
  int4 col = 0;

  if(type == RESTYPE_TEX1D)
    col = texDisplayIntTex1DArray.Load(int3(uv.x * texRes.x, slice, mip));
  else if(type == RESTYPE_TEX3D)
    col = texDisplayIntTex3D.Load(int4(uv.xy * texRes.xy, slice + 0.001f, mip));
  else if(type == RESTYPE_TEX2D)
    col = texDisplayIntTex2DArray.Load(int4(uv.xy * texRes.xy, slice, mip));
  else if(type == RESTYPE_TEX2D_MS)
  {
    if(sample < 0)
      sample = 0;
    col = texDisplayIntTex2DMSArray.Load(int3(uv.xy * texRes.xy, slice), sample);
  }

  return col;
}

float4 SampleTextureFloat4(uint type, bool linearSample, float2 uv, float slice, float mip,
                           int sample, float3 texRes, uint4 YUVRate, uint4 YUVASwizzle)
{
  float4 col = 0;

  bool interleaved_luma = false;

  // for interleaved 4:2:2, co-ords are in 2x1 blocks and need special processing
  if(YUVRate.x == 2 && YUVRate.y == 1 && YUVRate.z == 1)
  {
    // set the flag so we can post-process the results of the lookup
    interleaved_luma = true;

    // downsample texture resolution. uv is already normalised but we need to change the effective
    // resolution when multiplying to do a Load()
    texRes.xy /= YUVRate.xy;
  }

  if(type == RESTYPE_TEX1D)
  {
    if(linearSample)
      col = texDisplayTex1DArray.SampleLevel(linearSampler, float2(uv.x, slice), mip);
    else
      col = texDisplayTex1DArray.Load(int3(uv.x * texRes.x, slice, mip));
  }
  else if(type == RESTYPE_TEX3D)
  {
    if(linearSample)
      col = texDisplayTex3D.SampleLevel(linearSampler, float3(uv.xy, (slice + 0.001f) / texRes.z),
                                        mip);
    else
      col = texDisplayTex3D.Load(int4(uv.xy * texRes.xy, slice + 0.001f, mip));
  }
  else if(type == RESTYPE_DEPTH)
  {
    col.r = texDisplayTexDepthArray.Load(int4(uv.xy * texRes.xy, slice, mip)).r;
    col.gba = float3(0, 0, 1);
  }
  else if(type == RESTYPE_DEPTH_STENCIL)
  {
    col.r = texDisplayTexDepthArray.Load(int4(uv.xy * texRes.xy, slice, mip)).r;
    col.g = texDisplayTexStencilArray.Load(int4(uv.xy * texRes.xy, slice, mip)).g / 255.0f;
    col.ba = float2(0, 1);
  }
  else if(type == RESTYPE_DEPTH_MS)
  {
    if(sample < 0)
      sample = 0;

    col.r = texDisplayTexDepthMSArray.Load(int3(uv.xy * texRes.xy, slice), sample).r;
    col.gba = float3(0, 0, 1);
  }
  else if(type == RESTYPE_DEPTH_STENCIL_MS)
  {
    if(sample < 0)
      sample = 0;

    col.r = texDisplayTexDepthMSArray.Load(int3(uv.xy * texRes.xy, slice), sample).r;
    col.g = texDisplayTexStencilMSArray.Load(int3(uv.xy * texRes.xy, slice), sample).g / 255.0f;
    col.ba = float2(0, 1);
  }
  else if(type == RESTYPE_TEX2D_MS)
  {
    if(sample < 0)
    {
      int sampleCount = -sample;

      // worst resolve you've seen in your life
      for(int i = 0; i < sampleCount; i++)
        col += texDisplayTex2DMSArray.Load(int3(uv.xy * texRes.xy, slice), i);

      col /= float(sampleCount);
    }
    else
    {
      col = texDisplayTex2DMSArray.Load(int3(uv.xy * texRes.xy, slice), sample);
    }
  }
  else if(type == RESTYPE_TEX2D)
  {
    if(linearSample)
      col = texDisplayTex2DArray.SampleLevel(linearSampler, float3(uv.xy, slice), mip);
    else
      col = texDisplayTex2DArray.Load(int4(uv.xy * texRes.xy, slice, mip));
  }

  if(YUVRate.w > 0)
  {
    float4 col2 = 0;

    // look up the second YUV plane, in case we need it
    if(linearSample)
      col2 = texDisplayYUVArray.SampleLevel(linearSampler, float3(uv.xy, slice), mip);
    else
      col2 = texDisplayYUVArray.Load(int4(uv.xy * texRes.xy / YUVRate.xy, slice, mip));

    // store the data from both planes for swizzling
    float data[] = {
        col.x, col.y, col.z, col.w, col2.x, col2.y, col2.z, col2.w,
    };

    // use the specified channel swizzle to source the YUVA data
    float Y = data[YUVASwizzle.x];
    float U = data[YUVASwizzle.y];
    float V = data[YUVASwizzle.z];
    float A = 1.0f;

    if(YUVASwizzle.w != 0xff)
      A = data[YUVASwizzle.w];

    // if we have 4:2:2 interleaved luma, this needs special processing
    if(interleaved_luma)
    {
      // in interleaved 4:2:2 we have 2x1 blocks of texels within a single sample.
      // Chroma 'just works' in 4:2:2 because the shared U and V for a block are stored in G and A,
      // and for linear sampling the texture filtering does the right thing by interpolating G0 (U0)
      // with G1 (U1) and A0 (V0) with A1 (V1) for two blocks 0 and 1 of 2x1 texels.
      //
      // However consider 2x1 blocks of luma like this:
      //
      //				left					current				right
      //					v							v						 v
      // -----+--------+-------------------+--------+---
      // . Y1 | Y0	Y1 | (a) Y0 (b) Y1 (c) | Y0	Y1 | ..
      // -----+--------+-------------------+--------+---
      //
      // We define lumalerp = 0.0 to 1.0 across the block. The Y0 and Y1 centres are at 0.25 and
      // 0.75
      // respectively.
      //
      // For nearest sampling it's relatively simple, we just need to bisect by 0.5 and select
      // either
      // our Y0 or Y1. For linear sampling it's more complex:
      //
      // (a) between the left edge of the current block and the Y0 centre we need to do:
      //	 Y = lerp(leftY1, currentY0)
      // (b) between the Y0 and Y1 centres we need to do:
      //	 Y = lerp(currentY0, currentY1)
      // (c) between the Y1 centre and the right edge of the current block we need to do:
      //	 Y = lerp(currentY1, rightY0)

      // calculate the actual X co-ordinate at luma rate (we halved the rate above for loading the
      // texel)
      float x = uv.x * texRes.x * YUVRate.x;

      // calculate the lerp value to know where we are by rounding down to the nearest even X and
      // subtracting that from our X, giving a [0, 2] range, then halving it.
      float lumalerp = (x - (int(x) & ~1)) / 2.0f;

      if(!linearSample)
      {
        // simple nearest - if we're on the left half we use Y0, on the right half we use Y1
        if(lumalerp < 0.5f)
          Y = col.r;
        else
          Y = col.b;
      }
      else
      {
        // round UV x to pixel centre so we get precisely the current 2x1 texel.
        // We keep UV y the same so that we at least get vertical interpolation from the hardware.
        uv.x = floor(uv.x * texRes.x) / texRes.x + 1.0f / (texRes.x * YUVRate.x);

        // sample current texel, this gives us currentY0, currentY1 for our own texture.
        // The previous sample that we did above was fine for chroma, but is nonsense for luma
        col.xy = texDisplayTex2DArray.SampleLevel(linearSampler, float3(uv.xy, slice), mip).rb;

        if(lumalerp < 0.25f)
        {
          // case (a). Fetch the left neighbour block's Y1 and interpolate up to it
          float leftY1 = texDisplayTex2DArray
                             .SampleLevel(linearSampler, float3(uv.xy, slice), mip, int2(-1, 0))
                             .b;

          Y = lerp(leftY1, col.x, lumalerp * 2.0f + 0.5f);
        }
        else if(lumalerp < 0.75f)
        {
          // case (b). Interpolate between Y0 and Y1
          Y = lerp(col.x, col.y, (lumalerp - 0.25f) * 2.0f);
        }
        else
        {
          // case (c). Fetch the right neighbour block's Y0 and interpolate up to it
          float rightY0 =
              texDisplayTex2DArray.SampleLevel(linearSampler, float3(uv.xy, slice), mip, int2(1, 0)).r;

          Y = lerp(col.y, rightY0, (lumalerp - 0.75f) * 2.0f);
        }
      }
    }

    // Y goes in green channel, U in blue, V in red
    col = float4(V, Y, U, A);
  }

  return col;
}
