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

// use some preprocessor hacks to compile the same header in both hlsl and C++ so we can define
// classes that represent a whole cbuffer
#if defined(__cplusplus)

#define cbuffer struct
#define float2 Vec2f
#define float3 Vec3f
#define uint4 Vec4u
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
  float2 TextureResolution;

  float2 ScreenAspect;
  float Scale;
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
  float2 Padding;

  row_major float4x4 PickMVP;
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

#define MESHDISPLAY_SOLID 0x1
#define MESHDISPLAY_FACELIT 0x2
#define MESHDISPLAY_SECONDARY 0x3
#define MESHDISPLAY_SECONDARY_ALPHA 0x4

#define TEXDISPLAY_TYPEMASK 0xF
#define TEXDISPLAY_NANS 0x0100
#define TEXDISPLAY_CLIPPING 0x0200
#define TEXDISPLAY_UINT_TEX 0x0400
#define TEXDISPLAY_SINT_TEX 0x0800
#define TEXDISPLAY_GAMMA_CURVE 0x1000

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
#define HGRAM_PIXELS_PER_TILE 64
#define HGRAM_TILES_PER_BLOCK 10

#define HGRAM_NUM_BUCKETS 256

#define MESH_OTHER 0    // this covers points and lines, logic is the same
#define MESH_TRIANGLE_LIST 1
#define MESH_TRIANGLE_STRIP 2
#define MESH_TRIANGLE_LIST_ADJ 3
#define MESH_TRIANGLE_STRIP_ADJ 4