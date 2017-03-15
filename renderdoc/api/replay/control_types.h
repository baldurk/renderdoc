/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#pragma once

#include "data_types.h"
#include "replay_enums.h"

struct MeshFormat
{
  MeshFormat()
  {
    idxoffs = 0;
    idxByteWidth = 0;
    baseVertex = 0;
    offset = 0;
    stride = 0;
    compCount = 0;
    compByteWidth = 0;
    compType = CompType::Typeless;
    bgraOrder = false;
    specialFormat = SpecialFormat::Unknown;
    showAlpha = false;
    topo = Topology::Unknown;
    numVerts = 0;
    unproject = false;
    nearPlane = farPlane = 0.0f;
  }

  ResourceId idxbuf;
  uint64_t idxoffs;
  uint32_t idxByteWidth;
  int32_t baseVertex;

  ResourceId buf;
  uint64_t offset;
  uint32_t stride;

  uint32_t compCount;
  uint32_t compByteWidth;
  CompType compType;
  bool32 bgraOrder;
  SpecialFormat specialFormat;

  FloatVector meshColour;

  bool showAlpha;

  Topology topo;
  uint32_t numVerts;

  bool32 unproject;
  float nearPlane;
  float farPlane;
};

class Camera;

struct MeshDisplay
{
  MeshDataStage type;

  Camera *cam;

  bool32 ortho;
  float fov, aspect;

  bool32 showPrevInstances;
  bool32 showAllInstances;
  bool32 showWholePass;
  uint32_t curInstance;

  uint32_t highlightVert;
  MeshFormat position;
  MeshFormat second;

  FloatVector minBounds;
  FloatVector maxBounds;
  bool32 showBBox;

  SolidShade solidShadeMode;
  bool32 wireframeDraw;
};

struct TextureDisplay
{
  ResourceId texid;
  CompType typeHint;
  float rangemin;
  float rangemax;
  float scale;
  bool32 Red, Green, Blue, Alpha;
  bool32 FlipY;
  float HDRMul;
  bool32 linearDisplayAsGamma;
  ResourceId CustomShader;
  uint32_t mip;
  uint32_t sliceFace;
  uint32_t sampleIdx;
  bool32 rawoutput;

  float offx, offy;

  FloatVector lightBackgroundColour;
  FloatVector darkBackgroundColour;

  DebugOverlay overlay;
};

// some dependent structs for TextureSave
struct TextureComponentMapping
{
  float blackPoint;
  float whitePoint;
};

struct TextureSampleMapping
{
  // if true, texture acts like an array, each slice being
  // the corresponding sample, and below sample index is ignored.
  // Later options for handling slices/faces then control how
  // a texture array is mapped to the file.
  bool32 mapToArray;

  // if the above mapToArray is false, this selects the sample
  // index to treat as a normal 2D image. If this is ~0U a default
  // unweighted average resolve is performed instead.
  // resolve only available for uncompressed simple formats.
  uint32_t sampleIndex;
};

struct TextureSliceMapping
{
  // select the (depth/array) slice to save.
  // If this is -1, writes out all slices as detailed below
  // this is only supported in formats that don't support
  // slices natively, and will be done in RGBA8 space.
  int32_t sliceIndex;

  // write out the slices as a 2D grid, with the below
  // width. Any empty slices are writted as (0,0,0,0)
  bool32 slicesAsGrid;

  int32_t sliceGridWidth;

  // write out 6 slices in the cruciform:
  /*
         +---+
         |+y |
         |   |
     +---+---+---+---+
     |-x |+z |+x |-z |
     |   |   |   |   |
     +---+---+---+---+
         |-y |
         |   |
         +---+
  */
  // with the gaps filled with (0,0,0,0)
  bool32 cubeCruciform;

  // if sliceIndex is -1, cubeCruciform == slicesAsGrid == false
  // and file format doesn't support saving all slices, only
  // slice 0 is saved
};

struct TextureSave
{
  ResourceId id;

  CompType typeHint;

  FileType destType;

  // mip == -1 writes out all mips where allowed by file format
  // or writes mip 0 otherwise
  int32_t mip;

  // for output formats that are 8bit unorm srgb, values are mapped using
  // the following black/white points.
  TextureComponentMapping comp;

  // what to do for multisampled textures (ignored otherwise)
  TextureSampleMapping sample;

  // how to select/save depth/array slices or cubemap faces
  // if invalid options are specified, slice index 0 is written
  // alone
  TextureSliceMapping slice;

  int channelExtract;

  // for formats without an alpha channel, define how it should be
  // mapped. Only available for uncompressed simple formats, done
  // in RGBA8 space.
  AlphaMapping alpha;
  FloatVector alphaCol;
  FloatVector alphaColSecondary;

  int jpegQuality;
};

// dependent structs for TargetControlMessage
struct NewCaptureData
{
  uint32_t ID;
  uint64_t timestamp;
  rdctype::array<byte> thumbnail;
  rdctype::str path;
  bool32 local;
};

struct RegisterAPIData
{
  rdctype::str APIName;
};

struct BusyData
{
  rdctype::str ClientName;
};

struct NewChildData
{
  uint32_t PID;
  uint32_t ident;
};

struct TargetControlMessage
{
  TargetControlMessage() {}
  TargetControlMessageType Type;

  NewCaptureData NewCapture;
  RegisterAPIData RegisterAPI;
  BusyData Busy;
  NewChildData NewChild;
};
