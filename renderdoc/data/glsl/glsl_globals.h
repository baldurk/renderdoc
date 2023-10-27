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

#if defined(__cplusplus)

#define BINDING(b)
#define INST_NAME(name)

#else    // defined(__cplusplus)

#ifdef VULKAN

#define BINDING(b) layout(set = 0, binding = b, std140)
#define IO_LOCATION(l) layout(location = l)
#define VERTEX_ID gl_VertexIndex
#define INSTANCE_ID gl_InstanceIndex

#else    // ifdef VULKAN

// GL SPIR-V compilation uses a mixture
#ifdef GL_SPIRV

#define BINDING(b) layout(binding = b, std140)
#define IO_LOCATION(l) layout(location = l)
#define VERTEX_ID gl_VertexID
#define INSTANCE_ID gl_InstanceID

#else

// drop I/O location specifiers and bindings on GL, we don't use separate programs so I/O variables
// can be matched by name, and we don't want to require GL_ARB_shading_language_420pack so we can't
// specify bindings in shaders.
//
// however due to obtuse GL rules, variables with identical name and declaration do NOT match if
// only one of them has an explicit location. This is only used in custom shaders, but means we need
// to match it as while most drivers will handle the fallback as you'd normally expect, not all do
// and the spec doesn't require it. This does mean custom shaders won't work if they drop the
// explicit location, but there's no feasible way to support both and explicit location has been
// standard since this was added.

#ifdef FORCE_IO_LOCATION

#define IO_LOCATION(l) layout(location = l)

#else

#define IO_LOCATION(l)

#endif

#define BINDING(b) layout(std140)
#define VERTEX_ID gl_VertexID
#define INSTANCE_ID gl_InstanceID

#endif

#endif    // ifdef VULKAN

#define INST_NAME(name) name

#endif    // defined(__cplusplus)

#ifndef OPENGL_ES
#define PRECISION
#else
#define PRECISION highp
precision highp float;
precision highp int;
#endif

#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07f
#endif

// some constants available to both C++ and GLSL for configuring display
#define CUBEMAP_FACE_POS_X 0
#define CUBEMAP_FACE_NEG_X 1
#define CUBEMAP_FACE_POS_Y 2
#define CUBEMAP_FACE_NEG_Y 3
#define CUBEMAP_FACE_POS_Z 4
#define CUBEMAP_FACE_NEG_Z 5

// used in depthms2buffer.comp to define enum-format mapping
#define SHADER_D16_UNORM 0
#define SHADER_D16_UNORM_S8_UINT 1
#define SHADER_X8_D24_UNORM_PACK32 2
#define SHADER_D24_UNORM_S8_UINT 3
#define SHADER_D32_SFLOAT 4
#define SHADER_D32_SFLOAT_S8_UINT 5
#define SHADER_S8_UINT 6

// divide MS<->buffer workgroups by 64
#define MS_DISPATCH_LOCAL_SIZE 64

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

#define HEATMAP_DISABLED 0
#define HEATMAP_LINEAR 1
#define HEATMAP_TRISIZE 2

#define HEATMAP_RAMPSIZE 22

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
#define RESTYPE_TEX2DMSARRAY 0xB
#define RESTYPE_TEXTYPEMAX 0xB

#endif

// first few match Visualisation enum
#define MESHDISPLAY_SOLID 0x1
#define MESHDISPLAY_FACELIT 0x2
#define MESHDISPLAY_SECONDARY 0x3
#define MESHDISPLAY_EXPLODE 0x4
#define MESHDISPLAY_MESHLET 0x5

// extra values below
#define MESHDISPLAY_SECONDARY_ALPHA 0x6

#define MAX_NUM_MESHLETS (512 * 1024)

#define TEXDISPLAY_TYPEMASK 0xF
#define TEXDISPLAY_UINT_TEX 0x10
#define TEXDISPLAY_SINT_TEX 0x20
#define TEXDISPLAY_NANS 0x80
#define TEXDISPLAY_CLIPPING 0x100
#define TEXDISPLAY_GAMMA_CURVE 0x200

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

#if defined(SHADER_BASETYPE) && SHADER_BASETYPE == 0

#define FLOAT_TEX 1
#define UINT_TEX 0
#define SINT_TEX 0

#elif defined(SHADER_BASETYPE) && SHADER_BASETYPE == 1

#define FLOAT_TEX 0
#define UINT_TEX 1
#define SINT_TEX 0

#elif defined(SHADER_BASETYPE) && SHADER_BASETYPE == 2

#define FLOAT_TEX 0
#define UINT_TEX 0
#define SINT_TEX 1

#else

#define FLOAT_TEX 1
#define UINT_TEX 0
#define SINT_TEX 0

#endif
