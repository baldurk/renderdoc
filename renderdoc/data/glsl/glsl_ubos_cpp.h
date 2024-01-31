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

#pragma once

// use some preprocessor hacks to compile the same header in both GLSL and C++ so we can define
// classes that represent a whole cbuffer

#include "common/common.h"
#include "maths/matrix.h"
#include "maths/vec.h"

#define uniform struct

#define vec2 Vec2f
#define vec3 Vec3f
#define vec4 Vec4f

#define mat4 Matrix4f

#define uint uint32_t
#define uvec4 Vec4u

#if !defined(VULKAN) && !defined(OPENGL)
#error Must define VULKAN or OPENGL before including glsl_ubos.h
#endif

#if defined(VULKAN) && defined(OPENGL)
#error Only one of VULKAN and OPENGL must be defined in glsl_ubos.h
#endif

#include "glsl_ubos.h"

struct RD_CustomShader_UBO_Type
{
  uvec4 TexDim;
  uint SelectedMip;
  uint TextureType;
  uint SelectedSliceFace;
  int SelectedSample;
  uvec4 YUVDownsampleRate;
  uvec4 YUVAChannels;
  vec2 SelectedRange;
};

#if defined(VULKAN)

const char HLSL_CUSTOM_PREFIX[] =
    R"EOPREFIX(

#define RD_FLOAT_1D_ARRAY_BINDING t6
#define RD_FLOAT_1D_BINDING t6 // all textures treated as arrays, add macro aliases

#define RD_FLOAT_2D_ARRAY_BINDING t7
#define RD_FLOAT_2D_BINDING t7

#define RD_FLOAT_3D_BINDING t8

#define RD_FLOAT_DEPTH_BINDING t7
#define RD_FLOAT_DEPTH_ARRAY_BINDING t7

#define RD_FLOAT_STENCIL_BINDING t17
#define RD_FLOAT_STENCIL_ARRAY_BINDING t17

#define RD_FLOAT_DEPTHMS_BINDING t9
#define RD_FLOAT_DEPTHMS_ARRAY_BINDING t9

#define RD_FLOAT_STENCILMS_BINDING t19
#define RD_FLOAT_STENCILMS_ARRAY_BINDING t19

#define RD_FLOAT_2DMS_ARRAY_BINDING t9
#define RD_FLOAT_2DMS_BINDING t9

#define RD_FLOAT_YUV_ARRAY_BINDING t10
#define RD_FLOAT_YUV_BINDING t10

#define RD_UINT_1D_ARRAY_BINDING t11
#define RD_UINT_1D_BINDING t11

#define RD_UINT_2D_ARRAY_BINDING t12
#define RD_UINT_2D_BINDING t12

#define RD_UINT_3D_BINDING t13

#define RD_UINT_2DMS_ARRAY_BINDING t14
#define RD_UINT_2DMS_BINDING t14

#define RD_INT_1D_ARRAY_BINDING t16
#define RD_INT_1D_BINDING t16

#define RD_INT_2D_ARRAY_BINDING t17
#define RD_INT_2D_BINDING t17

#define RD_INT_3D_BINDING t18

#define RD_INT_2DMS_ARRAY_BINDING t19
#define RD_INT_2DMS_BINDING t19

#define RD_POINT_SAMPLER_BINDING s50
#define RD_LINEAR_SAMPLER_BINDING s51

#define RD_CONSTANT_BUFFER_BINDING b0

cbuffer RD_CBuffer_Type : register(RD_CONSTANT_BUFFER_BINDING)
{
	struct RD_CBuffer_Struct
	{
		uint4 TexDim;
		uint SelectedMip;
		uint TextureType;
		uint SelectedSliceFace;
		int SelectedSample;
		uint4 YUVDownsampleRate;
		uint4 YUVAChannels;
		float2 SelectedRange;
  } RD_CBuffer_Data;
};

#define RD_TextureType_1D 1
#define RD_TextureType_2D 2
#define RD_TextureType_3D 3
#define RD_TextureType_2DMS 4
#define RD_TextureType_Depth 999
#define RD_TextureType_DepthStencil 999
#define RD_TextureType_DepthMS 999
#define RD_TextureType_DepthStencilMS 999

// for compatibility
#define RD_TextureType_1D_Array 1
#define RD_TextureType_2D_Array 2
#define RD_TextureType_Cube 999
#define RD_TextureType_Cube_Array 999

// possible values (these are only return values from this function, NOT texture binding points):
// RD_TextureType_1D
// RD_TextureType_2D
// RD_TextureType_3D
// RD_TextureType_Depth (D3D only)
// RD_TextureType_DepthStencil (D3D only)
// RD_TextureType_DepthMS (D3D only)
// RD_TextureType_DepthStencilMS (D3D only)
// RD_TextureType_2DMS
uint RD_TextureType()
{
  return RD_CBuffer_Data.TextureType;
}

// selected sample, or -numSamples for resolve
int RD_SelectedSample()
{
  return RD_CBuffer_Data.SelectedSample;
}

uint RD_SelectedSliceFace()
{
  return RD_CBuffer_Data.SelectedSliceFace;
}

uint RD_SelectedMip()
{
  return RD_CBuffer_Data.SelectedMip;
}

// xyz = width, height, depth (or array size). w = # mips
uint4 RD_TexDim()
{
  return RD_CBuffer_Data.TexDim;
}

// x = horizontal downsample rate (1 full rate, 2 half rate)
// y = vertical downsample rate
// z = number of planes in input texture
// w = number of bits per component (8, 10, 16)
uint4 RD_YUVDownsampleRate()
{
  return RD_CBuffer_Data.YUVDownsampleRate;
}

// x = where Y channel comes from
// y = where U channel comes from
// z = where V channel comes from
// w = where A channel comes from
// each index will be [0,1,2,3] for xyzw in first plane,
// [4,5,6,7] for xyzw in second plane texture, etc.
// it will be 0xff = 255 if the channel does not exist.
uint4 RD_YUVAChannels()
{
  return RD_CBuffer_Data.YUVAChannels;
}

// a pair with minimum and maximum selected range values
float2 RD_SelectedRange()
{
  return RD_CBuffer_Data.SelectedRange;
}

)EOPREFIX";

const char GLSL_CUSTOM_PREFIX[] =
    R"EOPREFIX(
#define RD_FLOAT_1D_ARRAY_BINDING 6
#define RD_FLOAT_1D_BINDING 6 // all textures treated as arrays, add macro aliases

#define RD_FLOAT_2D_ARRAY_BINDING 7
#define RD_FLOAT_2D_BINDING 7

// cubemaps can read from the 2D binding
#define RD_FLOAT_CUBE_BINDING 7
#define RD_FLOAT_CUBE_ARRAY_BINDING 7

// these have no equivalent. Define them to something valid so shaders still compile,
// but they will break if used
#define RD_FLOAT_BUFFER_BINDING 3
#define RD_FLOAT_RECT_BINDING 4

#define RD_FLOAT_3D_BINDING 8

#define RD_FLOAT_2DMS_ARRAY_BINDING 9
#define RD_FLOAT_2DMS_BINDING 9

#define RD_FLOAT_YUV_ARRAY_BINDING 10
#define RD_FLOAT_YUV_BINDING 10
#define RD_FLOAT_YUV_ARRAY_SIZE 2

#define RD_UINT_1D_ARRAY_BINDING 11
#define RD_UINT_1D_BINDING 11

#define RD_UINT_2D_ARRAY_BINDING 12
#define RD_UINT_2D_BINDING 12

#define RD_UINT_3D_BINDING 13

#define RD_UINT_2DMS_ARRAY_BINDING 14
#define RD_UINT_2DMS_BINDING 14

#define RD_INT_1D_ARRAY_BINDING 16
#define RD_INT_1D_BINDING 16

#define RD_INT_2D_ARRAY_BINDING 17
#define RD_INT_2D_BINDING 17

#define RD_INT_3D_BINDING 18

#define RD_INT_2DMS_ARRAY_BINDING 19
#define RD_INT_2DMS_BINDING 19

#define RD_POINT_SAMPLER_BINDING 50
#define RD_LINEAR_SAMPLER_BINDING 51

#define RD_CONSTANT_BUFFER_BINDING 0

layout(binding = RD_CONSTANT_BUFFER_BINDING) uniform RD_CBuffer_Type
{
  uvec4 TexDim;
  uint SelectedMip;
  uint TextureType;
  uint SelectedSliceFace;
  int SelectedSample;
  uvec4 YUVDownsampleRate;
  uvec4 YUVAChannels;
  vec2 SelectedRange;
} RD_CBuffer_Data;

#define RD_TextureType_1D 1
#define RD_TextureType_2D 2
#define RD_TextureType_3D 3
#define RD_TextureType_2DMS 4

// for compatibility
#define RD_TextureType_1D_Array 1
#define RD_TextureType_2D_Array 2
#define RD_TextureType_2DMS_Array 4
#define RD_TextureType_Cube 999
#define RD_TextureType_Cube_Array 999
#define RD_TextureType_Rect 999
#define RD_TextureType_Buffer 999
#define RD_TextureType_Depth 999
#define RD_TextureType_DepthStencil 999
#define RD_TextureType_DepthMS 999
#define RD_TextureType_DepthStencilMS 999

// possible values (these are only return values from this function, NOT texture binding points):
// RD_TextureType_1D
// RD_TextureType_2D
// RD_TextureType_3D
// RD_TextureType_Cube (OpenGL only)
// RD_TextureType_1D_Array (OpenGL only)
// RD_TextureType_2D_Array (OpenGL only)
// RD_TextureType_Cube_Array (OpenGL only)
// RD_TextureType_Rect (OpenGL only)
// RD_TextureType_Buffer (OpenGL only)
// RD_TextureType_2DMS
// RD_TextureType_2DMS_Array (OpenGL only)
uint RD_TextureType()
{
  return RD_CBuffer_Data.TextureType;
}

// selected sample, or -numSamples for resolve
int RD_SelectedSample()
{
  return RD_CBuffer_Data.SelectedSample;
}

uint RD_SelectedSliceFace()
{
  return RD_CBuffer_Data.SelectedSliceFace;
}

uint RD_SelectedMip()
{
  return RD_CBuffer_Data.SelectedMip;
}

// xyz = width, height, depth (or array size). w = # mips
uvec4 RD_TexDim()
{
  return RD_CBuffer_Data.TexDim;
}

// x = horizontal downsample rate (1 full rate, 2 half rate)
// y = vertical downsample rate
// z = number of planes in input texture
// w = number of bits per component (8, 10, 16)
uvec4 RD_YUVDownsampleRate()
{
  return RD_CBuffer_Data.YUVDownsampleRate;
}

// x = where Y channel comes from
// y = where U channel comes from
// z = where V channel comes from
// w = where A channel comes from
// each index will be [0,1,2,3] for xyzw in first plane,
// [4,5,6,7] for xyzw in second plane texture, etc.
// it will be 0xff = 255 if the channel does not exist.
uvec4 RD_YUVAChannels()
{
  return RD_CBuffer_Data.YUVAChannels;
}

// a pair with minimum and maximum selected range values
vec2 RD_SelectedRange()
{
  return RD_CBuffer_Data.SelectedRange;
}

)EOPREFIX";

#elif defined(OPENGL)

const char GLSL_CUSTOM_PREFIX[] =
    R"EOPREFIX(
#define RD_FLOAT_1D_BINDING 1
#define RD_FLOAT_2D_BINDING 2
#define RD_FLOAT_3D_BINDING 3
#define RD_FLOAT_CUBE_BINDING 4
#define RD_FLOAT_1D_ARRAY_BINDING 5
#define RD_FLOAT_2D_ARRAY_BINDING 6
#define RD_FLOAT_CUBE_ARRAY_BINDING 7
#define RD_FLOAT_RECT_BINDING 8
#define RD_FLOAT_BUFFER_BINDING 9
#define RD_FLOAT_2DMS_BINDING 10
#define RD_FLOAT_2DMS_ARRAY_BINDING 11

#define RD_INT_1D_BINDING 1
#define RD_INT_2D_BINDING 2
#define RD_INT_3D_BINDING 3
#define RD_INT_CUBE_BINDING 4
#define RD_INT_1D_ARRAY_BINDING 5
#define RD_INT_2D_ARRAY_BINDING 6
#define RD_INT_CUBE_ARRAY_BINDING 7
#define RD_INT_RECT_BINDING 8
#define RD_INT_BUFFER_BINDING 9
#define RD_INT_2DMS_BINDING 10
#define RD_INT_2DMS_ARRAY_BINDING 11

#define RD_UINT_1D_BINDING 1
#define RD_UINT_2D_BINDING 2
#define RD_UINT_3D_BINDING 3
#define RD_UINT_CUBE_BINDING 4
#define RD_UINT_1D_ARRAY_BINDING 5
#define RD_UINT_2D_ARRAY_BINDING 6
#define RD_UINT_CUBE_ARRAY_BINDING 7
#define RD_UINT_RECT_BINDING 8
#define RD_UINT_BUFFER_BINDING 9
#define RD_UINT_2DMS_BINDING 10
#define RD_UINT_2DMS_ARRAY_BINDING 11

#define RD_CONSTANT_BUFFER_BINDING 0

layout(binding = RD_CONSTANT_BUFFER_BINDING) uniform RD_CBuffer_Type
{
  uvec4 TexDim;
  uint SelectedMip;
  uint TextureType;
  uint SelectedSliceFace;
  int SelectedSample;
  uvec4 YUVDownsampleRate;
  uvec4 YUVAChannels;
  vec2 SelectedRange;
} RD_CBuffer_Data;

#define RD_TextureType_1D 1
#define RD_TextureType_2D 2
#define RD_TextureType_3D 3
#define RD_TextureType_Cube 4
#define RD_TextureType_1D_Array 5
#define RD_TextureType_2D_Array 6
#define RD_TextureType_Cube_Array 7
#define RD_TextureType_Rect 8
#define RD_TextureType_Buffer 9
#define RD_TextureType_2DMS 10
#define RD_TextureType_2DMS_Array 11

// for compatibility
#define RD_TextureType_Depth 999
#define RD_TextureType_DepthStencil 999
#define RD_TextureType_DepthMS 999
#define RD_TextureType_DepthStencilMS 999

// possible values (these are only return values from this function, NOT texture binding points):
// RD_TextureType_1D
// RD_TextureType_2D
// RD_TextureType_3D
// RD_TextureType_Cube (OpenGL only)
// RD_TextureType_1D_Array (OpenGL only)
// RD_TextureType_2D_Array (OpenGL only)
// RD_TextureType_Cube_Array (OpenGL only)
// RD_TextureType_Rect (OpenGL only)
// RD_TextureType_Buffer (OpenGL only)
// RD_TextureType_2DMS
// RD_TextureType_2DMS_Array (OpenGL only)
uint RD_TextureType()
{
  return RD_CBuffer_Data.TextureType;
}

// selected sample, or -numSamples for resolve
int RD_SelectedSample()
{
  return RD_CBuffer_Data.SelectedSample;
}

uint RD_SelectedSliceFace()
{
  return RD_CBuffer_Data.SelectedSliceFace;
}

uint RD_SelectedMip()
{
  return RD_CBuffer_Data.SelectedMip;
}

// xyz = width, height, depth (or array size). w = # mips
uvec4 RD_TexDim()
{
  return RD_CBuffer_Data.TexDim;
}

// x = horizontal downsample rate (1 full rate, 2 half rate)
// y = vertical downsample rate
// z = number of planes in input texture
// w = number of bits per component (8, 10, 16)
uvec4 RD_YUVDownsampleRate()
{
  return RD_CBuffer_Data.YUVDownsampleRate;
}

// x = where Y channel comes from
// y = where U channel comes from
// z = where V channel comes from
// w = where A channel comes from
// each index will be [0,1,2,3] for xyzw in first plane,
// [4,5,6,7] for xyzw in second plane texture, etc.
// it will be 0xff = 255 if the channel does not exist.
uvec4 RD_YUVAChannels()
{
  return RD_CBuffer_Data.YUVAChannels;
}

// a pair with minimum and maximum selected range values
vec2 RD_SelectedRange()
{
  return RD_CBuffer_Data.SelectedRange;
}

)EOPREFIX";

#else

#error "OPENGL and VULKAN both not defined"
#endif
