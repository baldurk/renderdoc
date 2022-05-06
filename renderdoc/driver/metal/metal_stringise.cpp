/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_common.h"

#define MTL_STRINGISE_ENUM(a) STRINGISE_ENUM_CLASS_NAMED(a, "MTL" #a)
#define MTL_STRINGISE_BITFIELD_BIT(a) STRINGISE_BITFIELD_CLASS_BIT_NAMED(a, "MTL" #a)
#define MTL_STRINGISE_BITFIELD_VALUE(a) STRINGISE_BITFIELD_CLASS_VALUE_NAMED(a, "MTL" #a)

template <>
rdcstr DoStringise(const MTL::Mutability &el)
{
  BEGIN_ENUM_STRINGISE(MTL::Mutability)
  {
    MTL_STRINGISE_ENUM(MutabilityDefault);
    MTL_STRINGISE_ENUM(MutabilityMutable);
    MTL_STRINGISE_ENUM(MutabilityImmutable);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::PixelFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::PixelFormat)
  {
    MTL_STRINGISE_ENUM(PixelFormatInvalid);
    MTL_STRINGISE_ENUM(PixelFormatA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatR8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Uint);
    MTL_STRINGISE_ENUM(PixelFormatR8Sint);
    MTL_STRINGISE_ENUM(PixelFormatR16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatR16Uint);
    MTL_STRINGISE_ENUM(PixelFormatR16Sint);
    MTL_STRINGISE_ENUM(PixelFormatR16Float);
    MTL_STRINGISE_ENUM(PixelFormatRG8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRG8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatRG8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRG8Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG8Sint);
    MTL_STRINGISE_ENUM(PixelFormatB5G6R5Unorm);
    MTL_STRINGISE_ENUM(PixelFormatA1BGR5Unorm);
    MTL_STRINGISE_ENUM(PixelFormatABGR4Unorm);
    MTL_STRINGISE_ENUM(PixelFormatBGR5A1Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR32Uint);
    MTL_STRINGISE_ENUM(PixelFormatR32Sint);
    MTL_STRINGISE_ENUM(PixelFormatR32Float);
    MTL_STRINGISE_ENUM(PixelFormatRG16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRG16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRG16Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG16Sint);
    MTL_STRINGISE_ENUM(PixelFormatRG16Float);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA8Sint);
    MTL_STRINGISE_ENUM(PixelFormatBGRA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatBGRA8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatRGB10A2Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGB10A2Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG11B10Float);
    MTL_STRINGISE_ENUM(PixelFormatRGB9E5Float);
    MTL_STRINGISE_ENUM(PixelFormatBGR10A2Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRG32Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG32Sint);
    MTL_STRINGISE_ENUM(PixelFormatRG32Float);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Sint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Float);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Sint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Float);
    MTL_STRINGISE_ENUM(PixelFormatBC1_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC1_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC2_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC2_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC3_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC3_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC4_RUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC4_RSnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC5_RGUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC5_RGSnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC6H_RGBFloat);
    MTL_STRINGISE_ENUM(PixelFormatBC6H_RGBUfloat);
    MTL_STRINGISE_ENUM(PixelFormatBC7_RGBAUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC7_RGBAUnorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_2BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_2BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_4BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_4BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_2BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_2BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_4BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_4BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatEAC_R11Unorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_R11Snorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RG11Unorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RG11Snorm);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RGBA8);
    MTL_STRINGISE_ENUM(PixelFormatEAC_RGBA8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8A1);
    MTL_STRINGISE_ENUM(PixelFormatETC2_RGB8A1_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_4x4_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x4_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x6_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x6_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x5_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x6_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x8_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x10_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x10_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x12_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatASTC_4x4_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x4_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x6_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x6_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x8_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x5_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x6_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x8_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x10_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x10_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x12_LDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_4x4_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x4_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_5x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_6x6_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x6_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_8x8_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x5_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x6_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x8_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_10x10_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x10_HDR);
    MTL_STRINGISE_ENUM(PixelFormatASTC_12x12_HDR);
    MTL_STRINGISE_ENUM(PixelFormatGBGR422);
    MTL_STRINGISE_ENUM(PixelFormatBGRG422);
    MTL_STRINGISE_ENUM(PixelFormatDepth16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatDepth32Float);
    MTL_STRINGISE_ENUM(PixelFormatStencil8);
    MTL_STRINGISE_ENUM(PixelFormatDepth24Unorm_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatDepth32Float_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatX32_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatX24_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatBGRA10_XR);
    MTL_STRINGISE_ENUM(PixelFormatBGRA10_XR_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBGR10_XR);
    MTL_STRINGISE_ENUM(PixelFormatBGR10_XR_sRGB);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::VertexFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::VertexFormat)
  {
    MTL_STRINGISE_ENUM(VertexFormatInvalid);
    MTL_STRINGISE_ENUM(VertexFormatUChar2);
    MTL_STRINGISE_ENUM(VertexFormatUChar3);
    MTL_STRINGISE_ENUM(VertexFormatUChar4);
    MTL_STRINGISE_ENUM(VertexFormatChar2);
    MTL_STRINGISE_ENUM(VertexFormatChar3);
    MTL_STRINGISE_ENUM(VertexFormatChar4);
    MTL_STRINGISE_ENUM(VertexFormatUChar2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUChar3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUChar4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatChar2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatChar3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatChar4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort2);
    MTL_STRINGISE_ENUM(VertexFormatUShort3);
    MTL_STRINGISE_ENUM(VertexFormatUShort4);
    MTL_STRINGISE_ENUM(VertexFormatShort2);
    MTL_STRINGISE_ENUM(VertexFormatShort3);
    MTL_STRINGISE_ENUM(VertexFormatShort4);
    MTL_STRINGISE_ENUM(VertexFormatUShort2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatShort2Normalized);
    MTL_STRINGISE_ENUM(VertexFormatShort3Normalized);
    MTL_STRINGISE_ENUM(VertexFormatShort4Normalized);
    MTL_STRINGISE_ENUM(VertexFormatHalf2);
    MTL_STRINGISE_ENUM(VertexFormatHalf3);
    MTL_STRINGISE_ENUM(VertexFormatHalf4);
    MTL_STRINGISE_ENUM(VertexFormatFloat);
    MTL_STRINGISE_ENUM(VertexFormatFloat2);
    MTL_STRINGISE_ENUM(VertexFormatFloat3);
    MTL_STRINGISE_ENUM(VertexFormatFloat4);
    MTL_STRINGISE_ENUM(VertexFormatInt);
    MTL_STRINGISE_ENUM(VertexFormatInt2);
    MTL_STRINGISE_ENUM(VertexFormatInt3);
    MTL_STRINGISE_ENUM(VertexFormatInt4);
    MTL_STRINGISE_ENUM(VertexFormatUInt);
    MTL_STRINGISE_ENUM(VertexFormatUInt2);
    MTL_STRINGISE_ENUM(VertexFormatUInt3);
    MTL_STRINGISE_ENUM(VertexFormatUInt4);
    MTL_STRINGISE_ENUM(VertexFormatInt1010102Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUInt1010102Normalized);
    MTL_STRINGISE_ENUM(VertexFormatUChar4Normalized_BGRA);
    MTL_STRINGISE_ENUM(VertexFormatUChar);
    MTL_STRINGISE_ENUM(VertexFormatChar);
    MTL_STRINGISE_ENUM(VertexFormatUCharNormalized);
    MTL_STRINGISE_ENUM(VertexFormatCharNormalized);
    MTL_STRINGISE_ENUM(VertexFormatUShort);
    MTL_STRINGISE_ENUM(VertexFormatShort);
    MTL_STRINGISE_ENUM(VertexFormatUShortNormalized);
    MTL_STRINGISE_ENUM(VertexFormatShortNormalized);
    MTL_STRINGISE_ENUM(VertexFormatHalf);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::VertexStepFunction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::VertexStepFunction)
  {
    MTL_STRINGISE_ENUM(VertexStepFunctionConstant);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerVertex);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerInstance);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerPatch);
    MTL_STRINGISE_ENUM(VertexStepFunctionPerPatchControlPoint);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::PrimitiveTopologyClass &el)
{
  BEGIN_ENUM_STRINGISE(MTL::PrimitiveTopologyClass)
  {
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassUnspecified);
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassPoint);
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassLine);
    MTL_STRINGISE_ENUM(PrimitiveTopologyClassTriangle);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::Winding &el)
{
  BEGIN_ENUM_STRINGISE(MTL::Winding)
  {
    MTL_STRINGISE_ENUM(WindingClockwise);
    MTL_STRINGISE_ENUM(WindingCounterClockwise);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationFactorFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationFactorFormat)
  {
    MTL_STRINGISE_ENUM(TessellationFactorFormatHalf);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationControlPointIndexType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationControlPointIndexType)
  {
    MTL_STRINGISE_ENUM(TessellationControlPointIndexTypeNone);
    MTL_STRINGISE_ENUM(TessellationControlPointIndexTypeUInt16);
    MTL_STRINGISE_ENUM(TessellationControlPointIndexTypeUInt32);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationFactorStepFunction &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationFactorStepFunction)
  {
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionConstant);
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionPerPatch);
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionPerInstance);
    MTL_STRINGISE_ENUM(TessellationFactorStepFunctionPerPatchAndPerInstance);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TessellationPartitionMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TessellationPartitionMode)
  {
    MTL_STRINGISE_ENUM(TessellationPartitionModePow2);
    MTL_STRINGISE_ENUM(TessellationPartitionModeInteger);
    MTL_STRINGISE_ENUM(TessellationPartitionModeFractionalOdd);
    MTL_STRINGISE_ENUM(TessellationPartitionModeFractionalEven);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::CPUCacheMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::CPUCacheMode)
  {
    MTL_STRINGISE_ENUM(CPUCacheModeDefaultCache);
    MTL_STRINGISE_ENUM(CPUCacheModeWriteCombined);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::StorageMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::StorageMode)
  {
    MTL_STRINGISE_ENUM(StorageModeShared);
    MTL_STRINGISE_ENUM(StorageModeManaged);
    MTL_STRINGISE_ENUM(StorageModePrivate);
    MTL_STRINGISE_ENUM(StorageModeMemoryless);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::HazardTrackingMode &el)
{
  BEGIN_ENUM_STRINGISE(MTL::HazardTrackingMode)
  {
    MTL_STRINGISE_ENUM(HazardTrackingModeDefault);
    MTL_STRINGISE_ENUM(HazardTrackingModeUntracked);
    MTL_STRINGISE_ENUM(HazardTrackingModeTracked);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::ResourceOptions &el)
{
  uint64_t local = (uint64_t)el;
  rdcstr ret;
  // MTL::ResourceOptions is a combined value containing
  // MTL::CPUCacheMode, MTL::StorageMode, MTL::HazardTrackingMode

  // The same value (0) is used for
  // MTLResourceCPUCacheModeDefaultCache
  // MTLResourceStorageModeShared
  // MTLResourceHazardTrackingModeDefault

  if((el & MTL::ResourceCPUCacheModeWriteCombined) == MTL::ResourceCPUCacheModeWriteCombined)
  {
    local &= ~uint64_t(MTL::ResourceCPUCacheModeWriteCombined);
    ret += " | MTLResourceCPUCacheModeWriteCombined";
  }
  else
  {
    ret += " | MTLResourceCPUCacheModeDefaultCache";
  }

  if((el & MTL::ResourceStorageModeManaged) == MTL::ResourceStorageModeManaged)
  {
    local &= ~uint64_t(MTL::ResourceStorageModeManaged);
    ret += " | MTLResourceStorageModeManaged";
  }
  else if((el & MTL::ResourceStorageModePrivate) == MTL::ResourceStorageModePrivate)
  {
    local &= ~uint64_t(MTL::ResourceStorageModePrivate);
    ret += " | MTLResourceStorageModePrivate";
  }
  else if((el & MTL::ResourceStorageModeMemoryless) == MTL::ResourceStorageModeMemoryless)
  {
    local &= ~uint64_t(MTL::ResourceStorageModeMemoryless);
    ret += " | MTLResourceStorageModeMemoryless";
  }
  else
  {
    ret += " | MTLResourceStorageModeShared";
  }

  if((el & MTL::ResourceHazardTrackingModeUntracked) == MTL::ResourceHazardTrackingModeUntracked)
  {
    local &= ~uint64_t(MTL::ResourceHazardTrackingModeUntracked);
    ret += " | MTLResourceHazardTrackingModeUntracked";
  }
  else if((el & MTL::ResourceHazardTrackingModeTracked) == MTL::ResourceHazardTrackingModeTracked)
  {
    local &= ~uint64_t(MTL::ResourceHazardTrackingModeTracked);
    ret += " | MTLResourceHazardTrackingModeTracked";
  }
  else
  {
    ret += " | MTLResourceHazardTrackingModeDefault";
  }

  if(local)
  {
    ret += " | MTLResourceOptions (" + ToStr((uint32_t)local) + ")";
  }

  ret = ret.substr(3);
  return ret;
}

template <>
rdcstr DoStringise(const MTL::TextureType &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TextureType)
  {
    MTL_STRINGISE_ENUM(TextureType1D);
    MTL_STRINGISE_ENUM(TextureType1DArray);
    MTL_STRINGISE_ENUM(TextureType2D);
    MTL_STRINGISE_ENUM(TextureType2DArray);
    MTL_STRINGISE_ENUM(TextureType2DMultisample);
    MTL_STRINGISE_ENUM(TextureTypeCube);
    MTL_STRINGISE_ENUM(TextureTypeCubeArray);
    MTL_STRINGISE_ENUM(TextureType3D);
    MTL_STRINGISE_ENUM(TextureType2DMultisampleArray);
    MTL_STRINGISE_ENUM(TextureTypeTextureBuffer);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TextureUsage &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::TextureUsage)
  {
    MTL_STRINGISE_BITFIELD_VALUE(TextureUsageUnknown);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageShaderRead);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageShaderWrite);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageRenderTarget);
    MTL_STRINGISE_BITFIELD_BIT(TextureUsagePixelFormatView);
  }
  END_BITFIELD_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::TextureSwizzle &el)
{
  BEGIN_ENUM_STRINGISE(MTL::TextureSwizzle)
  {
    MTL_STRINGISE_ENUM(TextureSwizzleZero);
    MTL_STRINGISE_ENUM(TextureSwizzleOne);
    MTL_STRINGISE_ENUM(TextureSwizzleRed);
    MTL_STRINGISE_ENUM(TextureSwizzleGreen);
    MTL_STRINGISE_ENUM(TextureSwizzleBlue);
    MTL_STRINGISE_ENUM(TextureSwizzleAlpha);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::BlendFactor &el)
{
  BEGIN_ENUM_STRINGISE(MTL::BlendFactor)
  {
    MTL_STRINGISE_ENUM(BlendFactorZero);
    MTL_STRINGISE_ENUM(BlendFactorOne);
    MTL_STRINGISE_ENUM(BlendFactorSourceColor);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSourceColor);
    MTL_STRINGISE_ENUM(BlendFactorSourceAlpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSourceAlpha);
    MTL_STRINGISE_ENUM(BlendFactorDestinationColor);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusDestinationColor);
    MTL_STRINGISE_ENUM(BlendFactorDestinationAlpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusDestinationAlpha);
    MTL_STRINGISE_ENUM(BlendFactorSourceAlphaSaturated);
    MTL_STRINGISE_ENUM(BlendFactorBlendColor);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusBlendColor);
    MTL_STRINGISE_ENUM(BlendFactorBlendAlpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusBlendAlpha);
    MTL_STRINGISE_ENUM(BlendFactorSource1Color);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSource1Color);
    MTL_STRINGISE_ENUM(BlendFactorSource1Alpha);
    MTL_STRINGISE_ENUM(BlendFactorOneMinusSource1Alpha);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::BlendOperation &el)
{
  BEGIN_ENUM_STRINGISE(MTL::BlendOperation)
  {
    MTL_STRINGISE_ENUM(BlendOperationAdd);
    MTL_STRINGISE_ENUM(BlendOperationSubtract);
    MTL_STRINGISE_ENUM(BlendOperationReverseSubtract);
    MTL_STRINGISE_ENUM(BlendOperationMin);
    MTL_STRINGISE_ENUM(BlendOperationMax);
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const MTL::ColorWriteMask &el)
{
  BEGIN_BITFIELD_STRINGISE(MTL::ColorWriteMask)
  {
    MTL_STRINGISE_BITFIELD_VALUE(ColorWriteMaskNone);
    MTL_STRINGISE_BITFIELD_VALUE(ColorWriteMaskAll);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskAlpha);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskBlue);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskGreen);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskRed);
  }
  END_BITFIELD_STRINGISE()
}
