/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2019 Baldur Karlsson
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

// since we pre-process externally, we can't preserve preprocessor commands through to use them for
// optional extensions. Instead we set defines externally and use those entirely, and require the
// extension whenever they're set.
//
// TEXSAMPLE_CUBE_ARRAY: GL_OES_texture_cube_map_array or GL_EXT_texture_cube_map_array
// TEXSAMPLE_BUFFER: GL_EXT_texture_buffer

// no extension for this one, it just became core in GLES 3.1
#if __VERSION__ >= 310
#define TEXSAMPLE_MULTISAMPLE 1
#endif

#if UINT_TEX

// these bindings are defined based on the RESTYPE_ defines in glsl_ubos.h

// 1d = 1
uniform PRECISION usampler2D texUInt2D;
uniform PRECISION usampler3D texUInt3D;
// cube = 4
// 1d array = 5
uniform PRECISION usampler2DArray texUInt2DArray;
// cube array = 7
// 2d rect = 8
#ifdef TEXSAMPLE_BUFFER
uniform PRECISION usamplerBuffer texUIntBuffer;
#endif
#ifdef TEXSAMPLE_MULTISAMPLE
uniform PRECISION usampler2DMS texUInt2DMS;
uniform PRECISION usampler2DMSArray texUInt2DMSArray;
#endif

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes,
                         uvec4 YUVDownsampleRate, uvec4 YUVAChannels)
{
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

uvec4 SampleTextureUInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  uvec4 col;
  if(type == RESTYPE_TEX2D)
  {
    col = texelFetch(texUInt2D, ivec2(pos * texRes.xy), mipLevel);
  }
  else if(type == RESTYPE_TEXBUFFER)
  {
#ifdef TEXSAMPLE_BUFFER
    col = texelFetch(texUIntBuffer, int(pos.x * texRes.x));
#else
    col = uvec4(0u, 0u, 0u, 0u);
#endif
  }
  else if(type == RESTYPE_TEX2DMS)
  {
#ifdef TEXSAMPLE_MULTISAMPLE
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = texelFetch(texUInt2DMS, ivec2(pos * texRes.xy), sampleIdx);
#else
    col = uvec4(0u, 0u, 0u, 0u);
#endif
  }
  else if(type == RESTYPE_TEX2DMSARRAY)
  {
#ifdef TEXSAMPLE_MULTISAMPLE
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = texelFetch(texUInt2DMSArray, ivec3(pos * texRes.xy, slice), sampleIdx);
#else
    col = uvec4(0u, 0u, 0u, 0u);
#endif
  }
  else if(type == RESTYPE_TEX2DARRAY)
  {
    col = texelFetch(texUInt2DArray, ivec3(pos * texRes.xy, slice), mipLevel);
  }
  else    // if (type == RESTYPE_TEX3D)
  {
    col = texelFetch(texUInt3D, ivec3(pos * texRes.xy, slice + 0.001f), mipLevel);
  }

  return col;
}

ivec4 SampleTextureSInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  return ivec4(0, 0, 0, 0);
}

#elif SINT_TEX

// these bindings are defined based on the RESTYPE_ defines in glsl_ubos.h

// 1d = 1
uniform PRECISION isampler2D texSInt2D;
uniform PRECISION isampler3D texSInt3D;
// cube = 4
// 1d array = 5
uniform PRECISION isampler2DArray texSInt2DArray;
// cube array = 7
// 2d rect = 8
#ifdef TEXSAMPLE_BUFFER
uniform PRECISION isamplerBuffer texSIntBuffer;
#endif
#ifdef TEXSAMPLE_MULTISAMPLE
uniform PRECISION isampler2DMS texSInt2DMS;
uniform PRECISION isampler2DMSArray texSInt2DMSArray;
#endif

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes,
                         uvec4 YUVDownsampleRate, uvec4 YUVAChannels)
{
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

uvec4 SampleTextureUInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  return uvec4(0u, 0u, 0u, 0u);
}

ivec4 SampleTextureSInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  ivec4 col;
  if(type == RESTYPE_TEX2D)
  {
    col = texelFetch(texSInt2D, ivec2(pos * texRes.xy), mipLevel);
  }
  else if(type == RESTYPE_TEXBUFFER)
  {
#ifdef TEXSAMPLE_BUFFER
    col = texelFetch(texSIntBuffer, int(pos.x * texRes.x));
#else
    col = ivec4(0, 0, 0, 0);
#endif
  }
  else if(type == RESTYPE_TEX2DMS)
  {
#ifdef TEXSAMPLE_MULTISAMPLE
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = texelFetch(texSInt2DMS, ivec2(pos * texRes.xy), sampleIdx);
#else
    col = ivec4(0, 0, 0, 0);
#endif
  }
  else if(type == RESTYPE_TEX2DMSARRAY)
  {
#ifdef TEXSAMPLE_MULTISAMPLE
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = texelFetch(texSInt2DMSArray, ivec3(pos * texRes.xy, slice), sampleIdx);
#else
    col = ivec4(0, 0, 0, 0);
#endif
  }
  else if(type == RESTYPE_TEX2DARRAY)
  {
    col = texelFetch(texSInt2DArray, ivec3(pos * texRes.xy, slice), mipLevel);
  }
  else    // if (type == RESTYPE_TEX3D)
  {
    col = texelFetch(texSInt3D, ivec3(pos * texRes.xy, slice + 0.001f), mipLevel);
  }

  return col;
}

#else

// these bindings are defined based on the RESTYPE_ defines in glsl_ubos.h

// 1d = 1
uniform PRECISION sampler2D tex2D;
uniform PRECISION sampler3D tex3D;
uniform PRECISION samplerCube texCube;
// 1d array = 5
uniform PRECISION sampler2DArray tex2DArray;
#ifdef TEXSAMPLE_CUBE_ARRAY
uniform PRECISION samplerCubeArray texCubeArray;
#endif
// 2d rect = 8
#ifdef TEXSAMPLE_BUFFER
uniform PRECISION samplerBuffer texBuffer;
#endif
#ifdef TEXSAMPLE_MULTISAMPLE
uniform PRECISION sampler2DMS tex2DMS;
uniform PRECISION sampler2DMSArray tex2DMSArray;
#endif

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes,
                         uvec4 YUVDownsampleRate, uvec4 YUVAChannels)
{
  vec4 col;
  if(type == RESTYPE_TEX2D)
  {
    col = textureLod(tex2D, pos, float(mipLevel));
  }
  else if(type == RESTYPE_TEXBUFFER)
  {
#ifdef TEXSAMPLE_BUFFER
    col = texelFetch(texBuffer, int(pos.x * texRes.x));
#else
    col = vec4(0.0f, 0.0f, 0.0f, 0.0f);
#endif

#ifdef OPENGL_ES
    // This hack is needed for an Android device to let the compiler optimize out the texBuffer,
    // because otherwise due to some compiler bug the RESTYPE_TEX2D case won't work normally.
    col = vec4(1.0f, 1.0f, 1.0f, 1.0f);
#endif
  }
  else if(type == RESTYPE_TEX2DMS)
  {
#ifdef TEXSAMPLE_MULTISAMPLE
    if(sampleIdx < 0)
    {
      int sampleCount = -sampleIdx;

      // worst resolve you've seen in your life
      // it's manually unrolled because doing it as a dynamic loop on
      // sampleCount seems to produce crazy artifacts on nvidia - probably a compiler bug
      if(sampleCount == 2)
      {
        col += 0.5f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 0);
        col += 0.5f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 1);
      }
      else if(sampleCount == 4)
      {
        col += 0.25f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 0);
        col += 0.25f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 1);
        col += 0.25f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 2);
        col += 0.25f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 3);
      }
      else if(sampleCount == 8)
      {
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 0);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 1);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 2);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 3);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 4);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 5);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 6);
        col += 0.125f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 7);
      }
      else if(sampleCount == 16)
      {
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 0);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 1);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 2);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 3);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 4);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 5);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 6);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 7);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 8);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 9);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 10);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 11);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 12);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 13);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 14);
        col += 0.0625f * texelFetch(tex2DMS, ivec2(pos * texRes.xy), 15);
      }
    }
    else
    {
      col = texelFetch(tex2DMS, ivec2(pos * texRes.xy), sampleIdx);
    }
#else
    col = vec4(0.0f, 0.0f, 0.0f, 0.0f);
#endif
  }
  else if(type == RESTYPE_TEX2DMSARRAY)
  {
#ifdef TEXSAMPLE_MULTISAMPLE
    if(sampleIdx < 0)
    {
      int sampleCount = -sampleIdx;

      // worst resolve you've seen in your life
      // it's manually unrolled because doing it as a dynamic loop on
      // sampleCount seems to produce crazy artifacts on nvidia - probably a compiler bug
      if(sampleCount == 2)
      {
        col += 0.5f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 0);
        col += 0.5f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 1);
      }
      else if(sampleCount == 4)
      {
        col += 0.25f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 0);
        col += 0.25f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 1);
        col += 0.25f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 2);
        col += 0.25f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 3);
      }
      else if(sampleCount == 8)
      {
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 0);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 1);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 2);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 3);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 4);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 5);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 6);
        col += 0.125f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 7);
      }
      else if(sampleCount == 16)
      {
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 0);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 1);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 2);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 3);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 4);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 5);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 6);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 7);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 8);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 9);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 10);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 11);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 12);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 13);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 14);
        col += 0.0625f * texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), 15);
      }
    }
    else
    {
      col = texelFetch(tex2DMSArray, ivec3(pos * texRes.xy, slice), sampleIdx);
    }
#else
    col = vec4(0.0f, 0.0f, 0.0f, 0.0f);
#endif
  }
  else if(type == RESTYPE_TEX2DARRAY)
  {
    col = textureLod(tex2DArray, vec3(pos, slice), float(mipLevel));
  }
  else if(type == RESTYPE_TEX3D)
  {
    col = textureLod(tex3D, vec3(pos, (slice + 0.001f) / texRes.z), float(mipLevel));
  }
  else if(type == RESTYPE_TEXCUBE)
  {
    vec3 cubeCoord = CalcCubeCoord(pos, int(slice));

    col = textureLod(texCube, cubeCoord, float(mipLevel));
  }
  else    // type == RESTYPE_TEXCUBEARRAY
  {
#ifdef TEXSAMPLE_CUBE_ARRAY
    vec3 cubeCoord = CalcCubeCoord(pos, int(slice) % 6);
    vec4 arrayCoord = vec4(cubeCoord, int(slice) / 6);

    col = textureLod(texCubeArray, arrayCoord, float(mipLevel));
#else
    col = vec4(0.0f, 0.0f, 0.0f, 0.0f);
#endif
  }

  return col;
}

uvec4 SampleTextureUInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  return uvec4(0u, 0u, 0u, 0u);
}

ivec4 SampleTextureSInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  return ivec4(0, 0, 0, 0);
}

#endif
