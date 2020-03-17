#ifndef _COMMON_DEFINITIONS_H
#define _COMMON_DEFINITIONS_H

//===============================================================================
// Copyright (c) 2007-2019 Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2004-2006 ATI Technologies Inc.
//===============================================================================
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//
//  File Name:   Common_Def.h
//  Description: common definitions used for CPU/HPC/GPU
//
//////////////////////////////////////////////////////////////////////////////

// Features
#ifdef _WIN32
//#define USE_ASPM_CODE
#endif

// Proxy ISPC compiler (Warning! Not all ASPM features will be available : expect build errors for
// specialized ASPM code!
#ifdef ISPC
#define ASPM
#endif

// Using OpenCL Compiler
#ifdef __OPENCL_VERSION__
#define ASPM_GPU
#endif

#ifdef _LINUX
#undef ASPM_GPU
#include <stdio.h>
#include <cmath>
#include <cstring>
#include "cmp_math_vec4.h"
#endif

#ifndef CMP_MAX
#define CMP_MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef CMP_MIN
#define CMP_MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define CMP_SET_BC13_DECODER_RGBA    //  Sets mapping BC1, BC2 & BC3 to decode Red,Green,Blue and
                                     //  Alpha
                                     //       RGBA to channels [0,1,2,3] else BGRA maps to [0,1,2,3]
                                     //  BC4 alpha always maps as AAAA to channels [0,1,2,3]
//  BC5 decoded (Red&Green) maps R,G,B=0,A=255 to [0,1,2,3] else  maps [B=0,G,R,A=255] to [0,1,2,3]

//#define USE_BLOCK_LINEAR

#define CMP_FLOAT_MAX 3.402823466e+38F    // max value used to detect an Error in processing
#define CMP_FLOAT_MAX_EXP 38
#define USE_PROCESS_SEPERATE_ALPHA    // Enable this to use higher quality code using
                                      // CompressDualIndexBlock
#define COMPRESSED_BLOCK_SIZE 16      // Size of a compressed block in bytes
#define MAX_DIMENSION_BIG 4           // Max number of channels  (RGBA)
#define MAX_SUBSETS 3                 // Maximum number of possible subsets
#define MAX_SUBSET_SIZE 16            // Largest possible size for an individual subset
#define BLOCK_SIZE_4X4X4 64
#define BLOCK_SIZE_4X4 16
#define BlockX 4
#define BlockY 4
//#define USE_BLOCK_LINEAR    // Source Data is organized in linear form for each block :
// Experimental Code not fully developed
//#define USE_DOUBLE          // Default is to use float, enable to use double data types only for
// float definitions

typedef enum {
  CGU_CORE_OK = 0,            // No errors, call was successfull
  CGU_CORE_ERR_UNKOWN,        // An unknown error occurred
  CGU_CORE_ERR_NEWMEM,        // New Memory Allocation Failed
  CGU_CORE_ERR_INVALIDPTR,    // The pointer value used is invalid or null
  CGU_CORE_ERR_RANGERED,      // values for Red   Channel is out of range (too high or too low)
  CGU_CORE_ERR_RANGEGREEN,    // values for Green Channel is out of range (too high or too low)
  CGU_CORE_ERR_RANGEBLUE,     // values for Blue  Channel is out of range (too high or too low)
} CGU_ERROR_CODES;

//---------------------------------------------
// Predefinitions for GPU and CPU compiled code
//---------------------------------------------

#ifdef ASPM_GPU    // GPU Based code
// ==== Vectors ====
typedef float2 CGU_Vec2f;
typedef float2 CGV_Vec2f;
typedef float3 CMP_Vec3f;
typedef float3 CGU_Vec3f;
typedef float3 CGV_Vec3f;
typedef uchar3 CGU_Vec3uc;
typedef uchar3 CGV_Vec3uc;
typedef uchar4 CMP_Vec4uc;
typedef uchar4 CGU_Vec4uc;
typedef uchar4 CGV_Vec4uc;

#define USE_BC7_SP_ERR_IDX
#define ASPM_PRINT(args) printf args
#define BC7_ENCODECLASS

#define CMP_EXPORT
#define INLINE
#define uniform
#define varying
#define CMP_GLOBAL __global
#define CMP_KERNEL __kernel
#define CMP_CONSTANT __constant
#define CMP_STATIC

typedef unsigned int CGU_DWORD;    // 32bits
typedef int CGU_INT;               // 32bits
typedef int CGU_BOOL;
typedef unsigned short CGU_SHORT;    // 16bits
typedef float CGU_FLOAT;
typedef unsigned int uint32;    // need to remove this def

typedef int CGV_INT;
typedef unsigned int CGU_UINT;
typedef int CGUV_INT;
typedef int CGV_BOOL;

typedef char CGU_INT8;
typedef unsigned char CGU_UINT8;
typedef short CGU_INT16;
typedef unsigned short CGU_UINT16;
typedef int CGU_INT32;
typedef unsigned int CGU_UINT32;
typedef unsigned long CGU_UINT64;

typedef char CGV_INT8;
typedef unsigned char CGV_UINT8;
typedef short CGV_INT16;
typedef unsigned short CGV_UINT16;
typedef int CGV_INT32;
typedef unsigned int CGV_UINT32;
typedef unsigned long CGV_UINT64;

typedef float CGV_FLOAT;

#define TRUE 1
#define FALSE 0
#define CMP_CDECL

#else
// CPU & ASPM definitions

#ifdef ASPM    // SPMD ,SIMD CPU code
// using hybrid (CPU/GPU) aspm compiler
#define ASPM_PRINT(args) print args
#define CMP_USE_FOREACH_ASPM
#define __ASPM__
#define BC7_ENCODECLASS

#define USE_BC7_SP_ERR_IDX
//#define USE_BC7_RAMP

#define CMP_EXPORT export
#define TRUE true
#define FALSE false
typedef uniform bool CGU_BOOL;
typedef bool CGV_BOOL;

typedef unsigned int8 uint8;
typedef unsigned int16 uint16;
typedef unsigned int32 uint32;
typedef unsigned int64 uint64;
typedef uniform float CGU_FLOAT;
typedef varying float CGV_FLOAT;
typedef uniform uint8 CGU_UINT8;
typedef varying uint8 CGV_UINT8;

typedef CGV_UINT8<4> CGV_Vec4uc;
typedef CGU_UINT8<4> CGU_Vec4uc;

typedef CGU_FLOAT<3> CGU_Vec3f;
typedef CGV_FLOAT<3> CGV_Vec3f;

typedef CGU_FLOAT<2> CGU_Vec2f;
typedef CGV_FLOAT<2> CGV_Vec2f;

#define CMP_CDECL

#else    // standard CPU code
#include <stdio.h>
#include <string>
#include "cmp_math_vec4.h"

// using CPU compiler
#define ASPM_PRINT(args) printf args
#define USE_BC7_RAMP
#define USE_BC7_SP_ERR_IDX

#define CMP_EXPORT
#define BC7_ENCODECLASS BC7_EncodeClass::
#define TRUE 1
#define FALSE 0
#define uniform
#define varying

typedef char int8;
typedef short int16;
typedef int int32;
typedef long int64;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef int8 CGV_BOOL;
typedef int8 CGU_BOOL;
typedef int16 CGU_WORD;
typedef uint8 CGU_SHORT;
typedef int64 CGU_LONG;
typedef uint64 CGU_ULONG;

typedef uniform float CGU_FLOAT;
typedef varying float CGV_FLOAT;
typedef uniform uint8 CGU_UINT8;
typedef varying uint8 CGV_UINT8;
#if defined(WIN32) || defined(_WIN64)
#define CMP_CDECL __cdecl
#else
#define CMP_CDECL
#endif
#endif

// Common CPU & ASPM definitions
#define CMP_ASSERT(arg)

#define CMP_GLOBAL

#define CMP_KERNEL
#define __local const
#define __constant const
#define CMP_CONSTANT const
#define INLINE inline
#define CMP_STATIC static

typedef uniform int32 CGU_DWORD;
typedef uniform uint8 CGU_UBYTE;
typedef uniform int CGU_INT;
typedef uniform int8 CGU_INT8;

typedef uniform int16 CGU_INT16;
typedef uniform uint16 CGU_UINT16;
typedef uniform int32 CGU_INT32;
typedef uniform uint32 CGU_UINT32;
typedef uniform uint64 CGU_UINT64;

typedef int CGV_INT;
typedef int8 CGV_INT8;
typedef int16 CGV_INT16;
typedef int32 CGV_INT32;
typedef uint16 CGV_UINT16;
typedef uint32 CGV_UINT32;
typedef uint64 CGV_UINT64;
#endif    // ASPM_GPU

typedef struct
{
  CGU_UINT32 m_src_width;
  CGU_UINT32 m_src_height;
  CGU_UINT32 m_width_in_blocks;
  CGU_UINT32 m_height_in_blocks;
  CGU_FLOAT m_fquality;
} Source_Info;

// Ref Compute_CPU_HPC
struct texture_surface
{
  CGU_UINT8 *ptr;
  CGU_INT width, height, stride;
  CGU_INT channels;
};

#endif
