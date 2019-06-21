/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

// these bindings are defined based on the RESTYPE_ defines in glsl_ubos.h

// binding = 5 + RESTYPE_x
layout(binding = 6) uniform sampler1DArray tex1DArray;
layout(binding = 7) uniform sampler2DArray tex2DArray;
layout(binding = 8) uniform sampler3D tex3D;

// metal doesn't currently support MSAA arrays
#ifdef METAL_BACKEND

layout(binding = 9) uniform sampler2DMS tex2DMS;
vec4 fetchTex2DMSArray(ivec3 posAndSlice, int sampleIdx)
{
  return texelFetch(tex2DMS, posAndSlice.xy, sampleIdx);
}

#else

layout(binding = 9) uniform sampler2DMSArray tex2DMSArray;
vec4 fetchTex2DMSArray(ivec3 posAndSlice, int sampleIdx)
{
  return texelFetch(tex2DMSArray, posAndSlice, sampleIdx);
}

#endif

layout(binding = 10) uniform sampler2DArray texYUV[2];

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes,
                         uvec4 YUVDownsampleRate, uvec4 YUVAChannels)
{
  vec4 col;

  if(type == RESTYPE_TEX1D)
  {
    col = textureLod(tex1DArray, vec2(pos.x, slice), float(mipLevel));
  }
  else if(type == RESTYPE_TEX2D)
  {
    col = textureLod(tex2DArray, vec3(pos, slice), float(mipLevel));
  }
  else if(type == RESTYPE_TEX3D)
  {
    col = textureLod(tex3D, vec3(pos, (slice + 0.001f) / texRes.z), float(mipLevel));
  }
  else if(type == RESTYPE_TEX2DMS)
  {
    col = vec4(0, 0, 0, 0);

#ifndef NO_TEXEL_FETCH
    if(sampleIdx < 0)
    {
      int sampleCount = -sampleIdx;

      col = vec4(0, 0, 0, 0);

      // worst resolve you've seen in your life
      for(int i = 0; i < sampleCount; i++)
        col += fetchTex2DMSArray(ivec3(pos * texRes.xy, slice), i);

      col /= float(sampleCount);
    }
    else
    {
      col = fetchTex2DMSArray(ivec3(pos * texRes.xy, slice), sampleIdx);
    }
#endif
  }

#ifdef VULKAN
  // Vulkan supports YUV textures, so fetch secondary planes as needed and swizzle to standard
  // layout
  if(YUVDownsampleRate.w > 0)
  {
    vec4 col2 = vec4(0, 0, 0, 0);
    vec4 col3 = vec4(0, 0, 0, 0);

    if(YUVDownsampleRate.z >= 2)
      col2 = textureLod(texYUV[0], vec3(pos, slice), float(mipLevel));

    if(YUVDownsampleRate.z >= 3)
      col3 = textureLod(texYUV[1], vec3(pos, slice), float(mipLevel));

    // for interleaved 4:2:2, co-ords are in 2x1 blocks
    if(YUVDownsampleRate.x == 2 && YUVDownsampleRate.y == 1 && YUVDownsampleRate.z == 1)
    {
      int xcoord = int(pos.x * texRes.x);

      // texels come out as RG, so we need to fetch the adjacent texel to get the other half of the
      // uv data, the y sample is left as-is
      if((xcoord & 1) != 0)
      {
        col.b = textureLodOffset(tex2DArray, vec3(pos, slice), float(mipLevel), ivec2(-1, 0)).g;
      }
      else
      {
        col.b = col.g;
        col.g = textureLodOffset(tex2DArray, vec3(pos, slice), float(mipLevel), ivec2(1, 0)).g;
      }
    }

    float data[] = {
        // plane 0
        col.x, col.y, col.z, col.w,
        // plane 1
        col2.x, col2.y, col2.z, col2.w,
        // plane 3
        col3.x, col3.y, col3.z, col3.w,
    };

    col = vec4(0, 0, 0, 1);

    // luma
    if(YUVAChannels.x != 0xff)
      col.g = data[YUVAChannels.x];

    // chroma (U)
    if(YUVAChannels.y != 0xff)
      col.b = data[YUVAChannels.y];

    // chroma (V)
    if(YUVAChannels.z != 0xff)
      col.r = data[YUVAChannels.z];

    // alpha
    if(YUVAChannels.w != 0xff)
      col.a = data[YUVAChannels.w];
  }
#endif

  return col;
}

// these bindings are defined based on the RESTYPE_ defines in glsl_ubos.h

// binding = 10 + RESTYPE_x
layout(binding = 11) uniform usampler1DArray texUInt1DArray;
layout(binding = 12) uniform usampler2DArray texUInt2DArray;
layout(binding = 13) uniform usampler3D texUInt3D;

// metal doesn't currently support MSAA arrays
#ifdef METAL_BACKEND

layout(binding = 14) uniform usampler2DMS texUInt2DMS;
uvec4 fetchUIntTex2DMSArray(ivec3 posAndSlice, int sampleIdx)
{
  return texelFetch(texUInt2DMS, posAndSlice.xy, sampleIdx);
}

#else

layout(binding = 14) uniform usampler2DMSArray texUInt2DMSArray;
uvec4 fetchUIntTex2DMSArray(ivec3 posAndSlice, int sampleIdx)
{
  return texelFetch(texUInt2DMSArray, posAndSlice, sampleIdx);
}

#endif

uvec4 SampleTextureUInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  uvec4 col = uvec4(0, 0, 0, 0);

#ifndef NO_TEXEL_FETCH
  if(type == RESTYPE_TEX1D)
  {
    col = texelFetch(texUInt1DArray, ivec2(pos.x * texRes.x, slice), mipLevel);
  }
  else if(type == RESTYPE_TEX2D)
  {
    col = texelFetch(texUInt2DArray, ivec3(pos * texRes.xy, slice), mipLevel);
  }
  else if(type == RESTYPE_TEX3D)
  {
    col = texelFetch(texUInt3D, ivec3(pos * texRes.xy, slice + 0.001f), mipLevel);
  }
  else if(type == RESTYPE_TEX2DMS)
  {
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = fetchUIntTex2DMSArray(ivec3(pos * texRes.xy, slice), sampleIdx);
  }
#endif

  return col;
}

// these bindings are defined based on the RESTYPE_ defines in glsl_ubos.h

// binding = 15 + RESTYPE_x
layout(binding = 16) uniform isampler1DArray texSInt1DArray;
layout(binding = 17) uniform isampler2DArray texSInt2DArray;
layout(binding = 18) uniform isampler3D texSInt3D;

// metal doesn't currently support MSAA arrays
#ifdef METAL_BACKEND

layout(binding = 19) uniform isampler2DMS texSInt2DMS;
ivec4 fetchSIntTex2DMSArray(ivec3 posAndSlice, int sampleIdx)
{
  return texelFetch(texSInt2DMS, posAndSlice.xy, sampleIdx);
}

#else

layout(binding = 19) uniform isampler2DMSArray texSInt2DMSArray;
ivec4 fetchSIntTex2DMSArray(ivec3 posAndSlice, int sampleIdx)
{
  return texelFetch(texSInt2DMSArray, posAndSlice, sampleIdx);
}

#endif

ivec4 SampleTextureSInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  ivec4 col = ivec4(0, 0, 0, 0);

#ifndef NO_TEXEL_FETCH
  if(type == RESTYPE_TEX1D)
  {
    col = texelFetch(texSInt1DArray, ivec2(pos.x * texRes.x, slice), mipLevel);
  }
  else if(type == RESTYPE_TEX2D)
  {
    col = texelFetch(texSInt2DArray, ivec3(pos * texRes.xy, slice), mipLevel);
  }
  else if(type == RESTYPE_TEX3D)
  {
    col = texelFetch(texSInt3D, ivec3(pos * texRes.xy, slice + 0.001f), mipLevel);
  }
  else if(type == RESTYPE_TEX2DMS)
  {
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = fetchSIntTex2DMSArray(ivec3(pos * texRes.xy, slice), sampleIdx);
  }
#endif

  return col;
}
