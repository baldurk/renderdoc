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

template <>
rdcstr DoStringise(const MTL::PixelFormat &el)
{
  BEGIN_ENUM_STRINGISE(MTL::PixelFormat)
  {
    MTL_STRINGISE_ENUM(PixelFormatInvalid);

    /* Normal 8 bit formats */
    MTL_STRINGISE_ENUM(PixelFormatA8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Unorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Unorm_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatR8Snorm);
    MTL_STRINGISE_ENUM(PixelFormatR8Uint);
    MTL_STRINGISE_ENUM(PixelFormatR8Sint);

    /* Normal 16 bit formats */
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

    /* Packed 16 bit formats */

    MTL_STRINGISE_ENUM(PixelFormatB5G6R5Unorm);
    MTL_STRINGISE_ENUM(PixelFormatA1BGR5Unorm);
    MTL_STRINGISE_ENUM(PixelFormatABGR4Unorm);
    MTL_STRINGISE_ENUM(PixelFormatBGR5A1Unorm);

    /* Normal 32 bit formats */

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

    /* Packed 32 bit formats */

    MTL_STRINGISE_ENUM(PixelFormatRGB10A2Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGB10A2Uint);

    MTL_STRINGISE_ENUM(PixelFormatRG11B10Float);
    MTL_STRINGISE_ENUM(PixelFormatRGB9E5Float);

    MTL_STRINGISE_ENUM(PixelFormatBGR10A2Unorm);

    MTL_STRINGISE_ENUM(PixelFormatBGR10_XR);
    MTL_STRINGISE_ENUM(PixelFormatBGR10_XR_sRGB);

    /* Normal 64 bit formats */

    MTL_STRINGISE_ENUM(PixelFormatRG32Uint);
    MTL_STRINGISE_ENUM(PixelFormatRG32Sint);
    MTL_STRINGISE_ENUM(PixelFormatRG32Float);

    MTL_STRINGISE_ENUM(PixelFormatRGBA16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Snorm);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Sint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA16Float);

    MTL_STRINGISE_ENUM(PixelFormatBGRA10_XR);
    MTL_STRINGISE_ENUM(PixelFormatBGRA10_XR_sRGB);

    /* Normal 128 bit formats */

    MTL_STRINGISE_ENUM(PixelFormatRGBA32Uint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Sint);
    MTL_STRINGISE_ENUM(PixelFormatRGBA32Float);

    /* Compressed formats. */

    /* S3TC/DXT */
    MTL_STRINGISE_ENUM(PixelFormatBC1_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC1_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC2_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC2_RGBA_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatBC3_RGBA);
    MTL_STRINGISE_ENUM(PixelFormatBC3_RGBA_sRGB);

    /* RGTC */
    MTL_STRINGISE_ENUM(PixelFormatBC4_RUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC4_RSnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC5_RGUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC5_RGSnorm);

    /* BPTC */
    MTL_STRINGISE_ENUM(PixelFormatBC6H_RGBFloat);
    MTL_STRINGISE_ENUM(PixelFormatBC6H_RGBUfloat);
    MTL_STRINGISE_ENUM(PixelFormatBC7_RGBAUnorm);
    MTL_STRINGISE_ENUM(PixelFormatBC7_RGBAUnorm_sRGB);

    /* PVRTC */
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_2BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_2BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_4BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGB_4BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_2BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_2BPP_sRGB);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_4BPP);
    MTL_STRINGISE_ENUM(PixelFormatPVRTC_RGBA_4BPP_sRGB);

    /* ETC2 */
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

    /* ASTC */
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

    // ASTC HDR (High Dynamic Range) Formats
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

    /* Depth */

    MTL_STRINGISE_ENUM(PixelFormatDepth16Unorm);
    MTL_STRINGISE_ENUM(PixelFormatDepth32Float);

    /* Stencil */

    MTL_STRINGISE_ENUM(PixelFormatStencil8);

    /* Depth Stencil */

    MTL_STRINGISE_ENUM(PixelFormatDepth24Unorm_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatDepth32Float_Stencil8);

    MTL_STRINGISE_ENUM(PixelFormatX32_Stencil8);
    MTL_STRINGISE_ENUM(PixelFormatX24_Stencil8);
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
  BEGIN_BITFIELD_STRINGISE(MTL::ResourceOptions)
  {
    MTL_STRINGISE_BITFIELD_BIT(ResourceCPUCacheModeDefaultCache);
    MTL_STRINGISE_BITFIELD_BIT(ResourceCPUCacheModeWriteCombined);

    MTL_STRINGISE_BITFIELD_BIT(ResourceStorageModeShared);
    MTL_STRINGISE_BITFIELD_BIT(ResourceStorageModeManaged);
    MTL_STRINGISE_BITFIELD_BIT(ResourceStorageModePrivate);
    MTL_STRINGISE_BITFIELD_BIT(ResourceStorageModeMemoryless);

    MTL_STRINGISE_BITFIELD_BIT(ResourceHazardTrackingModeDefault);
    MTL_STRINGISE_BITFIELD_BIT(ResourceHazardTrackingModeUntracked);
    MTL_STRINGISE_BITFIELD_BIT(ResourceHazardTrackingModeTracked);

    MTL_STRINGISE_BITFIELD_BIT(ResourceOptionCPUCacheModeDefault);
    MTL_STRINGISE_BITFIELD_BIT(ResourceOptionCPUCacheModeWriteCombined);
  }
  END_BITFIELD_STRINGISE()
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
    MTL_STRINGISE_BITFIELD_BIT(TextureUsageUnknown);
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
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskNone);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskRed);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskGreen);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskBlue);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskAlpha);
    MTL_STRINGISE_BITFIELD_BIT(ColorWriteMaskAll);
  }
  END_BITFIELD_STRINGISE()
}
