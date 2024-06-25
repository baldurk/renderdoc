/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

// use some preprocessor hacks to compile the same header in both hlsl and C++ so we can define
// classes that represent a whole cbuffer
#if defined(__cplusplus)

#include "maths/matrix.h"
#include "maths/vec.h"

#define cbuffer struct
#define float2 Vec2f
#define float3 Vec3f
#define uint4 Vec4u
#define uint3 Vec3u
#define uint2 Vec2u
#define int4 Vec4i
#define float4 Vec4f
#define float4x4 Matrix4f
#define uint uint32_t
#define row_major

#define REG(r)

#else

#define REG(r) : register(r)

#endif

cbuffer FontCBuffer REG(b0)
{
  float2 TextPosition;
  float txtpadding;
  float TextSize;

  float2 CharacterSize;
  float2 FontScreenAspect;
};

cbuffer TexDisplayVSCBuffer REG(b0)
{
  float2 Position;
  float2 VertexScale;
};

cbuffer TexDisplayPSCBuffer REG(b0)
{
  float4 Channels;

  float RangeMinimum;
  float InverseRangeSize;
  float MipLevel;
  int FlipY;

  float3 WireframeColour;
  int OutputDisplayFormat;

  float Slice;
  float ScalePS;
  int SampleIdx;
  float AlwaysZero;

  int RawOutput;
  float3 TextureResolutionPS;

  uint4 YUVDownsampleRate;
  uint4 YUVAChannels;
};

cbuffer CheckerboardCBuffer REG(b0)
{
  float2 RectPosition;
  float2 RectSize;

  float4 PrimaryColor;
  float4 SecondaryColor;
  float4 InnerColor;

  float CheckerSquareDimension;
  float BorderWidth;
};

cbuffer MeshVertexCBuffer REG(b0)
{
  row_major float4x4 ModelViewProj;

  float2 SpriteSize;
  uint homogenousInput;
  float vtxExploderSNorm;

  float3 exploderCentre;
  float exploderScale;    // Non-zero values imply use of the exploder visualisation.

  uint vertMeshDisplayFormat;
  uint meshletOffset;
  uint meshletCount;
  uint padding1;

  uint4 meshletColours[12];
};

cbuffer MeshGeometryCBuffer REG(b0)
{
  row_major float4x4 InvProj;
};

cbuffer MeshPixelCBuffer REG(b0)
{
  float3 MeshColour;
  uint MeshDisplayFormat;
};

cbuffer MeshPickData REG(b0)
{
  float3 PickRayPos;
  uint PickIdx;

  float3 PickRayDir;
  uint PickNumVerts;

  float2 PickCoords;
  float2 PickViewport;

  uint PickMeshMode;
  uint PickUnproject;
  uint PickFlipY;
  uint PickOrtho;

  row_major float4x4 PickTransformMat;
};

#define HEATMAP_DISABLED 0
#define HEATMAP_LINEAR 1
#define HEATMAP_TRISIZE 2

#define HEATMAP_RAMPSIZE 22

cbuffer HeatmapData REG(b1)
{
  int HeatmapMode;
  float3 HeatmapPadding;

  // must match size of colorRamp on C++ side
  float4 ColorRamp[HEATMAP_RAMPSIZE];
};

cbuffer HistogramCBufferData REG(b0)
{
  uint HistogramChannels;
  float HistogramMin;
  float HistogramMax;
  uint HistogramFlags;

  float HistogramSlice;
  uint HistogramMip;
  int HistogramSample;
  uint Padding2;

  float3 HistogramTextureResolution;
  float Padding3;

  uint4 HistogramYUVDownsampleRate;
  uint4 HistogramYUVAChannels;
};

cbuffer DebugMathOperation REG(b0)
{
  float4 mathInVal;
  int mathOp;
};

cbuffer AccStructPatchInfo REG(b0)
{
  uint addressCount;
};

  // INCLUDE_GPUADDRESS_HELPERS should only be set for unit tests to check these functions below,
  // otherwise it pollutes the interface

#if defined(__cplusplus) && !defined(INCLUDE_GPUADDRESS_HELPERS)
// on the GPU this will be uint2 {.x = LSB, .y = MSB} to match uint64 order
typedef uint64_t GPUAddress;
#else
typedef uint2 GPUAddress;
#endif

// don't define the helpers in C++ by default, unless we're using them for unit tests
#if !defined(__cplusplus) || defined(INCLUDE_GPUADDRESS_HELPERS)

#if defined(__cplusplus)
#define max RDCMAX
#define min RDCMIN
#endif

bool lessThan(GPUAddress a, GPUAddress b)
{
  // either MSB is less, or MSB is equal and LSB is less-equal
  return a.y < b.y || (a.y == b.y && a.x < b.x);
}

bool lessEqual(GPUAddress a, GPUAddress b)
{
  return lessThan(a, b) || (a.y == b.y && a.x == b.x);
}

GPUAddress add(GPUAddress a, GPUAddress b)
{
  uint msb = 0, lsb = 0;
  if(b.x > 0 && a.x > 0xffffffff - b.x)
  {
    uint x = max(a.x, b.x) - 0x80000000;
    uint y = min(a.x, b.x);

    uint sum = x + y;

    msb = a.y + b.y + 1;
    lsb = sum - 0x80000000;
  }
  else
  {
    msb = a.y + b.y;
    lsb = a.x + b.x;
  }

  return GPUAddress(lsb, msb);
}

GPUAddress sub(GPUAddress a, GPUAddress b)
{
  uint msb = 0, lsb = 0;
  if(a.x < b.x)
  {
    uint diff = b.x - a.x;

    msb = a.y - b.y - 1;
    lsb = 0xffffffff - (diff - 1);
  }
  else
  {
    msb = a.y - b.y;
    lsb = a.x - b.x;
  }

  return GPUAddress(lsb, msb);
}
#endif

struct BlasAddressRange
{
  GPUAddress start;
  GPUAddress end;
};

struct BlasAddressPair
{
  BlasAddressRange oldAddress;
  BlasAddressRange newAddress;
};

// This corresponds to D3D12_RAYTRACING_INSTANCE_DESC structure
struct InstanceDesc
{
  uint2 padding[7];
  GPUAddress blasAddress;
};

cbuffer RayDispatchPatchCB REG(b0)
{
  // declare GPUAddresses first to avoid padding/alignment issues
  GPUAddress wrapped_sampHeapBase;
  GPUAddress wrapped_srvHeapBase;

  GPUAddress unwrapped_sampHeapBase;
  GPUAddress unwrapped_srvHeapBase;

  uint wrapped_sampHeapSize;
  uint wrapped_srvHeapSize;
  uint unwrapped_heapStrides;    // LSB = sampler, MSB = srv

  uint numPatchingAddrs;
};

cbuffer RayDispatchShaderRecordCB REG(b1)
{
  uint shaderrecord_stride;
  uint shaderrecord_count;
};

struct StateObjectLookup
{
  uint2 id;    // ResourceId
  uint offset;

  uint pad;
};

struct ShaderRecordData
{
  uint4 identifier[2];    // 32-byte real identifier
  uint rootSigIndex;      // only lower 16-bits are valid
};

#define RECORD_PATCH_THREADS 32

#define MAX_LOCALSIG_PARAMS 31

struct LocalRootSigData
{
  uint numParams;
  uint paramOffsets[MAX_LOCALSIG_PARAMS];
};

#define WRAPPED_DESCRIPTOR_STRIDE 64

cbuffer RayIndirectDispatchCB REG(b0)
{
  GPUAddress scratchBuffer;

  uint commandSigDispatchOffset;
  uint commandSigStride;
  uint commandSigSize;
  uint maxCommandCount;    // MaxCommandCount to clamp to. We also set the top bit if there is no count buffer
};

struct PatchingExecute
{
  // D3D12PatchRayDispatchParam::RecordCB
  uint shaderrecord_stride;
  uint shaderrecord_count;
  // D3D12PatchRayDispatchParam::SourceBuffer
  GPUAddress sourceData;
  // D3D12PatchRayDispatchParam::DestBuffer
  GPUAddress destData;
  // Dispatch itself
  uint3 dispatchDim;
  uint padding1;

  uint2 padding2;
};

cbuffer DebugSampleOperation REG(b0)
{
  float4 debugSampleUV;
  float4 debugSampleDDX;
  float4 debugSampleDDY;
  int4 debugSampleUVInt;
  int debugSampleTexDim;
  int debugSampleRetType;
  int debugSampleGatherChannel;
  int debugSampleSampleIndex;
  int debugSampleOperation;
  float debugSampleLodCompare;
};

#define DEBUG_SAMPLE_MATH_DXBC_RCP 1000
#define DEBUG_SAMPLE_MATH_DXBC_RSQ 1001
#define DEBUG_SAMPLE_MATH_DXBC_EXP 1002
#define DEBUG_SAMPLE_MATH_DXBC_LOG 1003
#define DEBUG_SAMPLE_MATH_DXBC_SINCOS 1004

#define DEBUG_SAMPLE_TEX_SAMPLE 100
#define DEBUG_SAMPLE_TEX_SAMPLE_LEVEL 101
#define DEBUG_SAMPLE_TEX_SAMPLE_BIAS 102
#define DEBUG_SAMPLE_TEX_SAMPLE_GRAD 103
#define DEBUG_SAMPLE_TEX_SAMPLE_CMP 104
#define DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO 105
#define DEBUG_SAMPLE_TEX_GATHER4 106
#define DEBUG_SAMPLE_TEX_GATHER4_CMP 107
#define DEBUG_SAMPLE_TEX_GATHER4_PO 108
#define DEBUG_SAMPLE_TEX_GATHER4_PO_CMP 109
#define DEBUG_SAMPLE_TEX_LOD 110
#define DEBUG_SAMPLE_TEX_LOAD 111
#define DEBUG_SAMPLE_TEX_LOAD_MS 112

#define DEBUG_SAMPLE_TEX1D 1
#define DEBUG_SAMPLE_TEX2D 2
#define DEBUG_SAMPLE_TEX3D 3
#define DEBUG_SAMPLE_TEXMS 4
#define DEBUG_SAMPLE_TEXCUBE 5

#define DEBUG_SAMPLE_UNORM 1
#define DEBUG_SAMPLE_SNORM 2
#define DEBUG_SAMPLE_INT 3
#define DEBUG_SAMPLE_UINT 4
#define DEBUG_SAMPLE_FLOAT 5

#define DEBUG_SAMPLE_MATH_DXIL_COS 10000
#define DEBUG_SAMPLE_MATH_DXIL_SIN 10001
#define DEBUG_SAMPLE_MATH_DXIL_TAN 10002
#define DEBUG_SAMPLE_MATH_DXIL_ACOS 10003
#define DEBUG_SAMPLE_MATH_DXIL_ASIN 10004
#define DEBUG_SAMPLE_MATH_DXIL_ATAN 10005
#define DEBUG_SAMPLE_MATH_DXIL_HCOS 10006
#define DEBUG_SAMPLE_MATH_DXIL_HSIN 10007
#define DEBUG_SAMPLE_MATH_DXIL_HTAN 10008
#define DEBUG_SAMPLE_MATH_DXIL_EXP 10009
#define DEBUG_SAMPLE_MATH_DXIL_LOG 10010
#define DEBUG_SAMPLE_MATH_DXIL_SQRT 10011
#define DEBUG_SAMPLE_MATH_DXIL_RSQRT 10012

// some constants available to both C++ and HLSL for configuring display
#define CUBEMAP_FACE_RIGHT 0
#define CUBEMAP_FACE_LEFT 1
#define CUBEMAP_FACE_UP 2
#define CUBEMAP_FACE_DOWN 3
#define CUBEMAP_FACE_FRONT 4
#define CUBEMAP_FACE_BACK 5

#define RESTYPE_TEX1D 0x1
#define RESTYPE_TEX2D 0x2
#define RESTYPE_TEX3D 0x3
#define RESTYPE_DEPTH 0x4
#define RESTYPE_DEPTH_STENCIL 0x5
#define RESTYPE_DEPTH_MS 0x6
#define RESTYPE_DEPTH_STENCIL_MS 0x7
#define RESTYPE_TEX2D_MS 0x9

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
#define TEXDISPLAY_NANS 0x0100
#define TEXDISPLAY_CLIPPING 0x0200
#define TEXDISPLAY_UINT_TEX 0x0400
#define TEXDISPLAY_SINT_TEX 0x0800
#define TEXDISPLAY_GAMMA_CURVE 0x1000

#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07f
#endif

// we pick a space that hopefully no-one else will use
// must match the define in quadoverdraw.hlsl
#define QUADOVERDRAW_UAV_SPACE 105202922

// histogram/minmax is calculated in blocks of NxN each with MxM tiles.
// e.g. a tile is 32x32 pixels, then this is arranged in blocks of 32x32 tiles.
// 1 compute thread = 1 tile, 1 compute group = 1 block
//
// NOTE because of this a block can cover more than the texture (think of a 1280x720
// texture covered by 2x1 blocks)
//
// these values are in each dimension
#define HGRAM_PIXELS_PER_TILE 64
#define HGRAM_TILES_PER_BLOCK 10

#define HGRAM_NUM_BUCKETS 256

#define MESH_OTHER 0    // this covers points and lines, logic is the same
#define MESH_TRIANGLE_LIST 1
#define MESH_TRIANGLE_STRIP 2
#define MESH_TRIANGLE_LIST_ADJ 3
#define MESH_TRIANGLE_STRIP_ADJ 4

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

#if defined(__cplusplus)

struct RD_CustomShader_CBuffer_Type
{
  uint4 TexDim;
  uint SelectedMip;
  uint TextureType;
  uint SelectedSliceFace;
  int SelectedSample;
  uint4 YUVDownsampleRate;
  uint4 YUVAChannels;
  float2 SelectedRange;
};

// move to an #include since fxc barfs on it
#include "hlsl_custom_prefix.h"

#endif
