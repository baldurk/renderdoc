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

namespace DXIL
{
enum class ResourceClass
{
  SRV = 0,
  UAV,
  CBuffer,
  Sampler,
  Invalid
};

enum class ComponentType
{
  Invalid = 0,
  I1,
  I16,
  U16,
  I32,
  U32,
  I64,
  U64,
  F16,
  F32,
  F64,
  SNormF16,
  UNormF16,
  SNormF32,
  UNormF32,
  SNormF64,
  UNormF64,
};

VarType VarTypeForComponentType(ComponentType compType);

enum class ResourceKind
{
  Unknown = 0,
  Texture1D,
  Texture2D,
  Texture2DMS,
  Texture3D,
  TextureCube,
  Texture1DArray,
  Texture2DArray,
  Texture2DMSArray,
  TextureCubeArray,
  TypedBuffer,
  RawBuffer,
  StructuredBuffer,
  CBuffer,
  Sampler,
  TBuffer,
  RTAccelerationStructure,
  FeedbackTexture2D,
  FeedbackTexture2DArray,
  StructuredBufferWithCounter,
  SamplerComparison,
};

enum class SamplerKind
{
  Default = 0,
  Comparison,
  Mono,
  Invalid,
};

enum class ShaderEntryTag
{
  ShaderFlags = 0,
  Geometry = 1,
  Domain = 2,
  Hull = 3,
  Compute = 4,
  Mesh = 9,
  Amplification = 10,
};

enum class ResField
{
  ID = 0,
  VarDecl = 1,
  Name = 2,
  Space = 3,
  RegBase = 4,
  RegCount = 5,

  // SRV
  SRVShape = 6,
  SRVSampleCount = 7,
  SRVTags = 8,

  // UAV
  UAVShape = 6,
  UAVGloballyCoherent = 7,
  UAVHiddenCounter = 8,
  UAVRasterOrder = 9,
  UAVTags = 10,

  // CBuffer
  CBufferByteSize = 6,
  CBufferTags = 7,

  // Sampler
  SamplerType = 6,
  SamplerTags = 7,
};

enum class MatrixOrientation
{
  Undefined = 0,
  RowMajor,
  ColumnMajor,
  LastEntry
};

enum class SamplerFeedbackType : uint8_t
{
  MinMip = 0,
  MipRegionUsed = 1,
  LastEntry = 2
};
};    // namespace DXIL

DECLARE_STRINGISE_TYPE(DXIL::ComponentType);
DECLARE_STRINGISE_TYPE(DXIL::ResourceClass);
