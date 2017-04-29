/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

// use some preprocessor hacks to compile the same header in both GLSL and C++ so we can define
// classes that represent a whole cbuffer
#if defined(__cplusplus)

// The Android GL ES shader compiler does not supports the " character.
// The includes should be before this file is included!
//#include "common/common.h"
//#include "maths/matrix.h"
//#include "maths/vec.h"

#define uniform struct
#define vec2 Vec2f
#define vec3 Vec3f
#define vec4 Vec4f
#define mat4 Matrix4f
#define uint uint32_t

#define BINDING(b)
#define INST_NAME(name)

struct Vec4u
{
  uint32_t x, y, z, w;
};

#define uvec4 Vec4u

#if !defined(VULKAN) && !defined(OPENGL)
#error Must define VULKAN or OPENGL before including debuguniforms.h
#endif

#if defined(VULKAN) && defined(OPENGL)
#error Only one of VULKAN and OPENGL must be defined in debuguniforms.h
#endif

#else

// we require these extensions to be able to set explicit layout bindings, etc
//#extension_nongles GL_ARB_shading_language_420pack : require
//#extension_nongles GL_ARB_separate_shader_objects : require
//#extension_nongles GL_ARB_explicit_attrib_location : require

#ifdef VULKAN

#define BINDING(b) layout(set = 0, binding = b, std140)
#define VERTEX_ID gl_VertexIndex
#define INSTANCE_ID gl_InstanceIndex

#else

#define OPENGL 1

#ifdef GL_ES
#define OPENGL_ES 1
#endif

#define BINDING(b) layout(binding = b, std140)
#define VERTEX_ID gl_VertexID
#define INSTANCE_ID gl_InstanceID

#endif

#define INST_NAME(name) name

#ifndef OPENGL_ES
#define PRECISION
#else
#define PRECISION mediump
precision PRECISION float;
precision PRECISION int;
#endif

#endif

BINDING(2) uniform HistogramUBOData
{
  uint HistogramChannels;
  float HistogramMin;
  float HistogramMax;
  uint HistogramFlags;

  float HistogramSlice;
  int HistogramMip;
  int HistogramSample;
  int HistogramNumSamples;

  vec3 HistogramTextureResolution;
  float Padding3;
}
INST_NAME(histogram_minmax);

BINDING(0) uniform MeshUBOData
{
  mat4 mvp;
  mat4 invProj;
  vec4 color;
  int displayFormat;
  uint homogenousInput;
  vec2 pointSpriteSize;
  uint rawoutput;
  vec3 padding;
}
INST_NAME(Mesh);

BINDING(0) uniform OutlineUBOData
{
  vec4 Inner_Color;
  vec4 Border_Color;
  vec4 ViewRect;
  uint Scissor;
  vec3 padding;
}
INST_NAME(outline);

BINDING(0) uniform FontUBOData
{
  vec2 TextPosition;
  float txtpadding;
  float TextSize;

  vec2 CharacterSize;
  vec2 FontScreenAspect;
}
INST_NAME(general);

BINDING(0) uniform MeshPickUBOData
{
  vec3 rayPos;
  uint use_indices;

  vec3 rayDir;
  uint numVerts;

  vec2 coords;
  vec2 viewport;

  uint meshMode;    // triangles, triangle strip, fan, etc...
  uint unproject;
  vec2 padding;

  mat4 mvp;
}
INST_NAME(meshpick);

struct FontGlyphData
{
  vec4 posdata;
  vec4 uvdata;
};

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126

BINDING(1) uniform GlyphUBOData
{
  FontGlyphData data[FONT_LAST_CHAR - FONT_FIRST_CHAR + 1];
}
INST_NAME(glyphs);

#define MAX_SINGLE_LINE_LENGTH 256

BINDING(2) uniform StringUBOData
{
  uvec4 chars[MAX_SINGLE_LINE_LENGTH];
}
INST_NAME(str);

BINDING(0) uniform TexDisplayUBOData
{
  vec2 Position;
  float Scale;
  float HDRMul;

  vec4 Channels;

  float RangeMinimum;
  float InverseRangeSize;
  int MipLevel;
  int FlipY;

  vec3 TextureResolutionPS;
  int OutputDisplayFormat;

  vec2 OutputRes;
  int RawOutput;
  float Slice;

  int SampleIdx;
  float MipShift;
  vec2 Padding;
}
INST_NAME(texdisplay);

// some constants available to both C++ and GLSL for configuring display
#define CUBEMAP_FACE_POS_X 0
#define CUBEMAP_FACE_NEG_X 1
#define CUBEMAP_FACE_POS_Y 2
#define CUBEMAP_FACE_NEG_Y 3
#define CUBEMAP_FACE_POS_Z 4
#define CUBEMAP_FACE_NEG_Z 5

#ifdef VULKAN

// we always upload an array (but it might have only one layer),
// so 2D and 2D arrays are the same.
// Cube and cube array textures are treated as 2D arrays.
#define RESTYPE_TEX1D 0x1
#define RESTYPE_TEX2D 0x2
#define RESTYPE_TEX3D 0x3
#define RESTYPE_TEX2DMS 0x4
#define RESTYPE_TEXTYPEMAX 0x5

#else    // OPENGL

#define RESTYPE_TEX1D 0x1
#define RESTYPE_TEX2D 0x2
#define RESTYPE_TEX3D 0x3
#define RESTYPE_TEXCUBE 0x4
#define RESTYPE_TEX1DARRAY 0x5
#define RESTYPE_TEX2DARRAY 0x6
#define RESTYPE_TEXCUBEARRAY 0x7
#define RESTYPE_TEXRECT 0x8
#define RESTYPE_TEXBUFFER 0x9
#define RESTYPE_TEX2DMS 0xA
#define RESTYPE_TEXTYPEMAX 0xA

#endif

#define MESHDISPLAY_SOLID 0x1
#define MESHDISPLAY_FACELIT 0x2
#define MESHDISPLAY_SECONDARY 0x3
#define MESHDISPLAY_SECONDARY_ALPHA 0x4

#define TEXDISPLAY_TYPEMASK 0xF
#define TEXDISPLAY_UINT_TEX 0x10
#define TEXDISPLAY_SINT_TEX 0x20
#define TEXDISPLAY_NANS 0x80
#define TEXDISPLAY_CLIPPING 0x100
#define TEXDISPLAY_GAMMA_CURVE 0x200

#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07f
#endif

// histogram/minmax is calculated in blocks of NxN each with MxM tiles.
// e.g. a tile is 32x32 pixels, then this is arranged in blocks of 32x32 tiles.
// 1 compute thread = 1 tile, 1 compute group = 1 block
//
// NOTE because of this a block can cover more than the texture (think of a 1280x720
// texture covered by 2x1 blocks)
//
// these values are in each dimension
#define HGRAM_PIXELS_PER_TILE 64u
#define HGRAM_TILES_PER_BLOCK 10u

#define HGRAM_NUM_BUCKETS 256u

#define MESH_OTHER 0u    // this covers points and lines, logic is the same
#define MESH_TRIANGLE_LIST 1u
#define MESH_TRIANGLE_STRIP 2u
#define MESH_TRIANGLE_FAN 3u
#define MESH_TRIANGLE_LIST_ADJ 4u
#define MESH_TRIANGLE_STRIP_ADJ 5u

#if !defined(__cplusplus)

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

#endif
