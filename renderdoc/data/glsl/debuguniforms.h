/******************************************************************************
 * The MIT License (MIT)
 * 
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

// use some preprocessor hacks to compile the same header in both GLSL and C++ so we can define
// classes that represent a whole cbuffer
#if defined(__cplusplus)

#define uniform struct
#define vec2 Vec2f
#define vec3 Vec3f
#define vec4 Vec4f

#define BINDING(b)

#else

#version 420 core

#define BINDING(b) layout (binding = b, std140) 

#endif

BINDING(0) uniform texdisplay
{
  vec2  Position;
  float Scale;
  float HDRMul;

  vec4  Channels;

  float RangeMinimum;
  float InverseRangeSize;
  float MipLevel;
  int   FlipY;

  vec3  TextureResolutionPS;
  int   OutputDisplayFormat;

  vec2  OutputRes;
  int   RawOutput;
  float Slice;
};

BINDING(0) uniform FontUniforms
{
	vec2  TextPosition;
	float txtpadding;
	float TextSize;

	vec2  CharacterSize;
	vec2  FontScreenAspect;
};

// some constants available to both C++ and GLSL for configuring display
#define CUBEMAP_FACE_POS_X 0
#define CUBEMAP_FACE_NEG_X 1
#define CUBEMAP_FACE_POS_Y 2
#define CUBEMAP_FACE_NEG_Y 3
#define CUBEMAP_FACE_POS_Z 4
#define CUBEMAP_FACE_NEG_Z 5

#define RESTYPE_TEX1D         0x1
#define RESTYPE_TEX2D         0x2
#define RESTYPE_TEX3D         0x3
#define RESTYPE_TEXCUBE       0x4
#define RESTYPE_TEX1DARRAY    0x5
#define RESTYPE_TEX2DARRAY    0x6
#define RESTYPE_TEXCUBEARRAY  0x7

#define TEXDISPLAY_TYPEMASK   0x7
#define TEXDISPLAY_UINT_TEX   0x8
#define TEXDISPLAY_SINT_TEX   0x10

