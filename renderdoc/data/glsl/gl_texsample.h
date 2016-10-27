/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2016 Baldur Karlsson
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

vec3 CalcCubeCoord(vec2 uv, int face)
{
  // From table 8.19 in GL4.5 spec
  // Map UVs to [-0.5, 0.5] and rotate
  uv -= vec2(0.5);
  vec3 coord;
  if(face == CUBEMAP_FACE_POS_X)
    coord = vec3(0.5, -uv.y, -uv.x);
  else if(face == CUBEMAP_FACE_NEG_X)
    coord = vec3(-0.5, -uv.y, uv.x);
  else if(face == CUBEMAP_FACE_POS_Y)
    coord = vec3(uv.x, 0.5, uv.y);
  else if(face == CUBEMAP_FACE_NEG_Y)
    coord = vec3(uv.x, -0.5, -uv.y);
  else if(face == CUBEMAP_FACE_POS_Z)
    coord = vec3(uv.x, -uv.y, 0.5);
  else    // face == CUBEMAP_FACE_NEG_Z
    coord = vec3(-uv.x, -uv.y, -0.5);
  return coord;
}

#if UINT_TEX

// these bindings are defined based on the RESTYPE_ defines in debuguniforms.h

#ifndef OPENGL_ES
layout(binding = 1) uniform usampler1D texUInt1D;
#endif
layout(binding = 2) uniform lowp usampler2D texUInt2D;
layout(binding = 3) uniform lowp usampler3D texUInt3D;
// cube = 4
#ifndef OPENGL_ES
layout(binding = 5) uniform usampler1DArray texUInt1DArray;
#endif
layout(binding = 6) uniform lowp usampler2DArray texUInt2DArray;
// cube array = 7
#ifndef OPENGL_ES
layout(binding = 8) uniform usampler2DRect texUInt2DRect;
#endif
layout(binding = 9) uniform lowp usamplerBuffer texUIntBuffer;
layout(binding = 10) uniform lowp usampler2DMS texUInt2DMS;

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

uvec4 SampleTextureUInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  uvec4 col;
#ifndef OPENGL_ES
  if(type == RESTYPE_TEX1D)
  {
    col = texelFetch(texUInt1D, int(pos.x * texRes.x), mipLevel);
  }
  else if(type == RESTYPE_TEX1DARRAY)
  {
    col = texelFetch(texUInt1DArray, ivec2(pos.x * texRes.x, slice), mipLevel);
  }
  else
#endif
  if(type == RESTYPE_TEX2D)
  {
    col = texelFetch(texUInt2D, ivec2(pos * texRes.xy), mipLevel);
  }
#ifndef OPENGL_ES
  else if(type == RESTYPE_TEXRECT)
  {
    col = texelFetch(texUInt2DRect, ivec2(pos * texRes.xy));
  }
#endif
  else if(type == RESTYPE_TEXBUFFER)
  {
    col = texelFetch(texUIntBuffer, int(pos.x * texRes.x));
  }
  else if(type == RESTYPE_TEX2DMS)
  {
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = texelFetch(texUInt2DMS, ivec2(pos * texRes.xy), sampleIdx);
  }
  else if(type == RESTYPE_TEX2DARRAY)
  {
    col = texelFetch(texUInt2DArray, ivec3(pos * texRes.xy, slice), mipLevel);
  }
  else if (type == RESTYPE_TEX3D)
  {
    col = texelFetch(texUInt3D, ivec3(pos * texRes.xy, slice), mipLevel);
  }

  return col;
}

ivec4 SampleTextureSInt4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  return ivec4(0, 0, 0, 0);
}

#elif SINT_TEX

// these bindings are defined based on the RESTYPE_ defines in debuguniforms.h

#ifndef OPENGL_ES
layout(binding = 1) uniform isampler1D texSInt1D;
#endif
layout(binding = 2) uniform lowp isampler2D texSInt2D;
layout(binding = 3) uniform lowp isampler3D texSInt3D;
// cube = 4
#ifndef OPENGL_ES
layout(binding = 5) uniform isampler1DArray texSInt1DArray;
#endif
layout(binding = 6) uniform lowp isampler2DArray texSInt2DArray;
// cube array = 7
#ifndef OPENGL_ES
layout(binding = 8) uniform isampler2DRect texSInt2DRect;
#endif
layout(binding = 9) uniform lowp isamplerBuffer texSIntBuffer;
layout(binding = 10) uniform lowp isampler2DMS texSInt2DMS;

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
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
#ifndef OPENGL_ES
  if(type == RESTYPE_TEX1D)
  {
    col = texelFetch(texSInt1D, int(pos.x * texRes.x), mipLevel);
  }
  else if(type == RESTYPE_TEX1DARRAY)
  {
    col = texelFetch(texSInt1DArray, ivec2(pos.x * texRes.x, slice), mipLevel);
  }
  else
#endif
  if(type == RESTYPE_TEX2D)
  {
    col = texelFetch(texSInt2D, ivec2(pos * texRes.xy), mipLevel);
  }
#ifndef OPENGL_ES
  else if(type == RESTYPE_TEXRECT)
  {
    col = texelFetch(texSInt2DRect, ivec2(pos * texRes.xy));
  }
#endif
  else if(type == RESTYPE_TEXBUFFER)
  {
    col = texelFetch(texSIntBuffer, int(pos.x * texRes.x));
  }
  else if(type == RESTYPE_TEX2DMS)
  {
    if(sampleIdx < 0)
      sampleIdx = 0;

    col = texelFetch(texSInt2DMS, ivec2(pos * texRes.xy), sampleIdx);
  }
  else if(type == RESTYPE_TEX2DARRAY)
  {
    col = texelFetch(texSInt2DArray, ivec3(pos * texRes.xy, slice), mipLevel);
  }
  else if (type == RESTYPE_TEX3D)
  {
    col = texelFetch(texSInt3D, ivec3(pos * texRes.xy, slice), mipLevel);
  }

  return col;
}

#else

// these bindings are defined based on the RESTYPE_ defines in debuguniforms.h

#ifndef OPENGL_ES
layout(binding = 1) uniform sampler1D tex1D;
#endif
layout(binding = 2) uniform lowp sampler2D tex2D;
layout(binding = 3) uniform lowp sampler3D tex3D;
layout(binding = 4) uniform lowp samplerCube texCube;
#ifndef OPENGL_ES
layout(binding = 5) uniform sampler1DArray tex1DArray;
#endif
layout(binding = 6) uniform lowp sampler2DArray tex2DArray;
layout(binding = 7) uniform lowp samplerCubeArray texCubeArray;
#ifndef OPENGL_ES
layout(binding = 8) uniform sampler2DRect tex2DRect;
#endif
layout(binding = 9) uniform lowp samplerBuffer texBuffer;
layout(binding = 10) uniform lowp sampler2DMS tex2DMS;

vec4 SampleTextureFloat4(int type, vec2 pos, float slice, int mipLevel, int sampleIdx, vec3 texRes)
{
  vec4 col;
#ifndef OPENGL_ES
  if(type == RESTYPE_TEX1D)
  {
    col = textureLod(tex1D, pos.x, float(mipLevel));
  }
  else if(type == RESTYPE_TEX1DARRAY)
  {
    col = textureLod(tex1DArray, vec2(pos.x, slice), float(mipLevel));
  }
  else
#endif
  if(type == RESTYPE_TEX2D)
  {
    col = textureLod(tex2D, pos, float(mipLevel));
  }
#ifndef OPENGL_ES
  else if(type == RESTYPE_TEXRECT)
  {
    col = texelFetch(tex2DRect, ivec2(pos * texRes.xy));
  }
#endif
  else if(type == RESTYPE_TEXBUFFER)
  {
    col = texelFetch(texBuffer, int(pos.x * texRes.x));
  }
  else if(type == RESTYPE_TEX2DMS)
  {
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
  }
  else if(type == RESTYPE_TEX2DARRAY)
  {
    col = textureLod(tex2DArray, vec3(pos, slice), float(mipLevel));
  }
  else if(type == RESTYPE_TEX3D)
  {
    col = textureLod(tex3D, vec3(pos, slice / texRes.z), float(mipLevel));
  }
  else if(type == RESTYPE_TEXCUBE)
  {
    vec3 cubeCoord = CalcCubeCoord(pos, int(slice));

    col = textureLod(texCube, cubeCoord, float(mipLevel));
  }
  else if(type == RESTYPE_TEXCUBEARRAY)
  {
    vec3 cubeCoord = CalcCubeCoord(pos, int(slice) % 6);
    vec4 arrayCoord = vec4(cubeCoord, int(slice) / 6);

    col = textureLod(texCubeArray, arrayCoord, float(mipLevel));
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
