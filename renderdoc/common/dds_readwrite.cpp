/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2019 Baldur Karlsson
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

#include "dds_readwrite.h"
#include <stdint.h>
#include "common/common.h"

static const uint32_t dds_fourcc = MAKE_FOURCC('D', 'D', 'S', ' ');

// from MSDN
struct DDS_PIXELFORMAT
{
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwFourCC;
  uint32_t dwRGBBitCount;
  uint32_t dwRBitMask;
  uint32_t dwGBitMask;
  uint32_t dwBBitMask;
  uint32_t dwABitMask;
};

struct DDS_HEADER
{
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwHeight;
  uint32_t dwWidth;
  uint32_t dwPitchOrLinearSize;
  uint32_t dwDepth;
  uint32_t dwMipMapCount;
  uint32_t dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  uint32_t dwCaps;
  uint32_t dwCaps2;
  uint32_t dwCaps3;
  uint32_t dwCaps4;
  uint32_t dwReserved2;
};

// from d3d10.h
enum D3D10_RESOURCE_DIMENSION
{
  D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
  D3D10_RESOURCE_DIMENSION_BUFFER = 1,
  D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
  D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
  D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
};

// from dxgiformat.h
enum DXGI_FORMAT
{
  DXGI_FORMAT_UNKNOWN = 0,
  DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
  DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
  DXGI_FORMAT_R32G32B32A32_UINT = 3,
  DXGI_FORMAT_R32G32B32A32_SINT = 4,
  DXGI_FORMAT_R32G32B32_TYPELESS = 5,
  DXGI_FORMAT_R32G32B32_FLOAT = 6,
  DXGI_FORMAT_R32G32B32_UINT = 7,
  DXGI_FORMAT_R32G32B32_SINT = 8,
  DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
  DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
  DXGI_FORMAT_R16G16B16A16_UNORM = 11,
  DXGI_FORMAT_R16G16B16A16_UINT = 12,
  DXGI_FORMAT_R16G16B16A16_SNORM = 13,
  DXGI_FORMAT_R16G16B16A16_SINT = 14,
  DXGI_FORMAT_R32G32_TYPELESS = 15,
  DXGI_FORMAT_R32G32_FLOAT = 16,
  DXGI_FORMAT_R32G32_UINT = 17,
  DXGI_FORMAT_R32G32_SINT = 18,
  DXGI_FORMAT_R32G8X24_TYPELESS = 19,
  DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
  DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
  DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
  DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
  DXGI_FORMAT_R10G10B10A2_UNORM = 24,
  DXGI_FORMAT_R10G10B10A2_UINT = 25,
  DXGI_FORMAT_R11G11B10_FLOAT = 26,
  DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
  DXGI_FORMAT_R8G8B8A8_UNORM = 28,
  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
  DXGI_FORMAT_R8G8B8A8_UINT = 30,
  DXGI_FORMAT_R8G8B8A8_SNORM = 31,
  DXGI_FORMAT_R8G8B8A8_SINT = 32,
  DXGI_FORMAT_R16G16_TYPELESS = 33,
  DXGI_FORMAT_R16G16_FLOAT = 34,
  DXGI_FORMAT_R16G16_UNORM = 35,
  DXGI_FORMAT_R16G16_UINT = 36,
  DXGI_FORMAT_R16G16_SNORM = 37,
  DXGI_FORMAT_R16G16_SINT = 38,
  DXGI_FORMAT_R32_TYPELESS = 39,
  DXGI_FORMAT_D32_FLOAT = 40,
  DXGI_FORMAT_R32_FLOAT = 41,
  DXGI_FORMAT_R32_UINT = 42,
  DXGI_FORMAT_R32_SINT = 43,
  DXGI_FORMAT_R24G8_TYPELESS = 44,
  DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
  DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
  DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
  DXGI_FORMAT_R8G8_TYPELESS = 48,
  DXGI_FORMAT_R8G8_UNORM = 49,
  DXGI_FORMAT_R8G8_UINT = 50,
  DXGI_FORMAT_R8G8_SNORM = 51,
  DXGI_FORMAT_R8G8_SINT = 52,
  DXGI_FORMAT_R16_TYPELESS = 53,
  DXGI_FORMAT_R16_FLOAT = 54,
  DXGI_FORMAT_D16_UNORM = 55,
  DXGI_FORMAT_R16_UNORM = 56,
  DXGI_FORMAT_R16_UINT = 57,
  DXGI_FORMAT_R16_SNORM = 58,
  DXGI_FORMAT_R16_SINT = 59,
  DXGI_FORMAT_R8_TYPELESS = 60,
  DXGI_FORMAT_R8_UNORM = 61,
  DXGI_FORMAT_R8_UINT = 62,
  DXGI_FORMAT_R8_SNORM = 63,
  DXGI_FORMAT_R8_SINT = 64,
  DXGI_FORMAT_A8_UNORM = 65,
  DXGI_FORMAT_R1_UNORM = 66,
  DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
  DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
  DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
  DXGI_FORMAT_BC1_TYPELESS = 70,
  DXGI_FORMAT_BC1_UNORM = 71,
  DXGI_FORMAT_BC1_UNORM_SRGB = 72,
  DXGI_FORMAT_BC2_TYPELESS = 73,
  DXGI_FORMAT_BC2_UNORM = 74,
  DXGI_FORMAT_BC2_UNORM_SRGB = 75,
  DXGI_FORMAT_BC3_TYPELESS = 76,
  DXGI_FORMAT_BC3_UNORM = 77,
  DXGI_FORMAT_BC3_UNORM_SRGB = 78,
  DXGI_FORMAT_BC4_TYPELESS = 79,
  DXGI_FORMAT_BC4_UNORM = 80,
  DXGI_FORMAT_BC4_SNORM = 81,
  DXGI_FORMAT_BC5_TYPELESS = 82,
  DXGI_FORMAT_BC5_UNORM = 83,
  DXGI_FORMAT_BC5_SNORM = 84,
  DXGI_FORMAT_B5G6R5_UNORM = 85,
  DXGI_FORMAT_B5G5R5A1_UNORM = 86,
  DXGI_FORMAT_B8G8R8A8_UNORM = 87,
  DXGI_FORMAT_B8G8R8X8_UNORM = 88,
  DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
  DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
  DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
  DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
  DXGI_FORMAT_BC6H_TYPELESS = 94,
  DXGI_FORMAT_BC6H_UF16 = 95,
  DXGI_FORMAT_BC6H_SF16 = 96,
  DXGI_FORMAT_BC7_TYPELESS = 97,
  DXGI_FORMAT_BC7_UNORM = 98,
  DXGI_FORMAT_BC7_UNORM_SRGB = 99,
  DXGI_FORMAT_AYUV = 100,
  DXGI_FORMAT_Y410 = 101,
  DXGI_FORMAT_Y416 = 102,
  DXGI_FORMAT_NV12 = 103,
  DXGI_FORMAT_P010 = 104,
  DXGI_FORMAT_P016 = 105,
  DXGI_FORMAT_420_OPAQUE = 106,
  DXGI_FORMAT_YUY2 = 107,
  DXGI_FORMAT_Y210 = 108,
  DXGI_FORMAT_Y216 = 109,
  DXGI_FORMAT_NV11 = 110,
  DXGI_FORMAT_AI44 = 111,
  DXGI_FORMAT_IA44 = 112,
  DXGI_FORMAT_P8 = 113,
  DXGI_FORMAT_A8P8 = 114,
  DXGI_FORMAT_B4G4R4A4_UNORM = 115,
  DXGI_FORMAT_FORCE_UINT = 0xffffffff
};

struct DDS_HEADER_DXT10
{
  DXGI_FORMAT dxgiFormat;
  D3D10_RESOURCE_DIMENSION resourceDimension;
  uint32_t miscFlag;
  uint32_t arraySize;
  uint32_t reserved;
};

#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

#define DDSCAPS_COMPLEX 0x8
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_TEXTURE 0x1000

#define DDSCAPS2_CUBEMAP 0x0200    // d3d10+ requires all cubemap faces
#define DDSCAPS2_CUBEMAP_ALLFACES 0xfc00
#define DDSCAPS2_VOLUME 0x200000

#define DDS_RESOURCE_MISC_TEXTURECUBE 0x4

#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000
#define DDPF_RGBA (DDPF_RGB | DDPF_ALPHAPIXELS)

ResourceFormat DXGIFormat2ResourceFormat(DXGI_FORMAT format)
{
  ResourceFormat special;
  ResourceFormat fmt32, fmt16, fmt8;

  fmt32.compByteWidth = 4;
  fmt32.compCount = 1;
  fmt32.compType = CompType::Float;
  fmt32.type = ResourceFormatType::Regular;

  fmt16.compByteWidth = 2;
  fmt16.compCount = 1;
  fmt16.compType = CompType::Float;
  fmt16.type = ResourceFormatType::Regular;

  fmt8.compByteWidth = 1;
  fmt8.compCount = 1;
  fmt8.compType = CompType::UNorm;
  fmt8.type = ResourceFormatType::Regular;

  switch(format)
  {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
      special.type = ResourceFormatType::BC1;
      special.compType =
          (format == DXGI_FORMAT_BC1_UNORM_SRGB) ? CompType::UNormSRGB : CompType::UNorm;
      return special;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
      special.type = ResourceFormatType::BC2;
      special.compType =
          (format == DXGI_FORMAT_BC2_UNORM_SRGB) ? CompType::UNormSRGB : CompType::UNorm;
      return special;
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
      special.type = ResourceFormatType::BC3;
      special.compType =
          (format == DXGI_FORMAT_BC3_UNORM_SRGB) ? CompType::UNormSRGB : CompType::UNorm;
      return special;
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
      special.type = ResourceFormatType::BC4;
      special.compType = (format == DXGI_FORMAT_BC4_UNORM ? CompType::UNorm : CompType::SNorm);
      return special;
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
      special.type = ResourceFormatType::BC5;
      special.compType = (format == DXGI_FORMAT_BC5_UNORM ? CompType::UNorm : CompType::SNorm);
      return special;
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
      special.type = ResourceFormatType::BC6;
      special.compType = (format == DXGI_FORMAT_BC6H_UF16 ? CompType::UNorm : CompType::SNorm);
      return special;
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      special.type = ResourceFormatType::BC7;
      special.compType =
          (format == DXGI_FORMAT_BC7_UNORM_SRGB) ? CompType::UNormSRGB : CompType::UNorm;
      return special;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
      special.type = ResourceFormatType::R10G10B10A2;
      special.compType = (format == DXGI_FORMAT_R10G10B10A2_UNORM ? CompType::UNorm : CompType::UInt);
      return special;
    case DXGI_FORMAT_R11G11B10_FLOAT: special.type = ResourceFormatType::R11G11B10; return special;
    case DXGI_FORMAT_B5G6R5_UNORM:
      fmt8.SetBGRAOrder(true);
      special.type = ResourceFormatType::R5G6B5;
      return special;
    case DXGI_FORMAT_B5G5R5A1_UNORM:
      fmt8.SetBGRAOrder(true);
      special.type = ResourceFormatType::R5G5B5A1;
      return special;
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
      special.type = ResourceFormatType::R9G9B9E5;
      return special;
    case DXGI_FORMAT_B4G4R4A4_UNORM:
      fmt8.SetBGRAOrder(true);
      special.type = ResourceFormatType::R4G4B4A4;
      return special;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: special.type = ResourceFormatType::D24S8; return special;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: special.type = ResourceFormatType::D32S8; return special;

    case DXGI_FORMAT_R32G32B32A32_UINT:
      fmt32.compType = CompType::UInt;
      fmt32.compCount = 4;
      return fmt32;
    case DXGI_FORMAT_R32G32B32A32_SINT:
      fmt32.compType = CompType::SInt;
      fmt32.compCount = 4;
      return fmt32;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: fmt32.compCount = 4; return fmt32;

    case DXGI_FORMAT_R32G32B32_UINT:
      fmt32.compType = CompType::UInt;
      fmt32.compCount = 3;
      return fmt32;
    case DXGI_FORMAT_R32G32B32_SINT:
      fmt32.compType = CompType::SInt;
      fmt32.compCount = 3;
      return fmt32;
    case DXGI_FORMAT_R32G32B32_FLOAT: fmt32.compCount = 3; return fmt32;

    case DXGI_FORMAT_R32G32_UINT:
      fmt32.compType = CompType::UInt;
      fmt32.compCount = 2;
      return fmt32;
    case DXGI_FORMAT_R32G32_SINT:
      fmt32.compType = CompType::SInt;
      fmt32.compCount = 2;
      return fmt32;
    case DXGI_FORMAT_R32G32_FLOAT: fmt32.compCount = 2; return fmt32;

    case DXGI_FORMAT_R32_UINT: fmt32.compType = CompType::UInt; return fmt32;
    case DXGI_FORMAT_R32_SINT: fmt32.compType = CompType::SInt; return fmt32;
    case DXGI_FORMAT_R32_FLOAT: return fmt32;

    case DXGI_FORMAT_R16G16B16A16_UINT:
      fmt16.compType = CompType::UInt;
      fmt16.compCount = 4;
      return fmt16;
    case DXGI_FORMAT_R16G16B16A16_SINT:
      fmt16.compType = CompType::SInt;
      fmt16.compCount = 4;
      return fmt16;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
      fmt16.compType = CompType::UNorm;
      fmt16.compCount = 4;
      return fmt16;
    case DXGI_FORMAT_R16G16B16A16_SNORM:
      fmt16.compType = CompType::SNorm;
      fmt16.compCount = 4;
      return fmt16;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: fmt16.compCount = 4; return fmt16;

    case DXGI_FORMAT_R16G16_UINT:
      fmt16.compType = CompType::UInt;
      fmt16.compCount = 2;
      return fmt16;
    case DXGI_FORMAT_R16G16_SINT:
      fmt16.compType = CompType::SInt;
      fmt16.compCount = 2;
      return fmt16;
    case DXGI_FORMAT_R16G16_UNORM:
      fmt16.compType = CompType::UNorm;
      fmt16.compCount = 2;
      return fmt16;
    case DXGI_FORMAT_R16G16_SNORM:
      fmt16.compType = CompType::SNorm;
      fmt16.compCount = 2;
      return fmt16;
    case DXGI_FORMAT_R16G16_FLOAT: fmt16.compCount = 2; return fmt16;

    case DXGI_FORMAT_R16_UINT: fmt16.compType = CompType::UInt; return fmt16;
    case DXGI_FORMAT_R16_SINT: fmt16.compType = CompType::SInt; return fmt16;
    case DXGI_FORMAT_R16_UNORM: fmt16.compType = CompType::UNorm; return fmt16;
    case DXGI_FORMAT_R16_SNORM: fmt16.compType = CompType::SNorm; return fmt16;
    case DXGI_FORMAT_R16_FLOAT: return fmt16;

    case DXGI_FORMAT_R8G8B8A8_UINT:
      fmt8.compType = CompType::UInt;
      fmt8.compCount = 4;
      return fmt8;
    case DXGI_FORMAT_R8G8B8A8_SINT:
      fmt8.compType = CompType::SInt;
      fmt8.compCount = 4;
      return fmt8;
    case DXGI_FORMAT_R8G8B8A8_SNORM:
      fmt8.compType = CompType::SNorm;
      fmt8.compCount = 4;
      return fmt8;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      fmt8.compType = CompType::UNormSRGB;
      fmt8.compCount = 4;
      return fmt8;
    case DXGI_FORMAT_R8G8B8A8_UNORM: fmt8.compCount = 4; return fmt8;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      fmt8.compCount = 4;
      fmt8.SetBGRAOrder(true);
      fmt8.compType =
          (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) ? CompType::UNormSRGB : CompType::UNorm;
      return fmt8;

    case DXGI_FORMAT_R8G8_UINT:
      fmt8.compType = CompType::UInt;
      fmt8.compCount = 2;
      return fmt8;
    case DXGI_FORMAT_R8G8_SINT:
      fmt8.compType = CompType::SInt;
      fmt8.compCount = 2;
      return fmt8;
    case DXGI_FORMAT_R8G8_SNORM:
      fmt8.compType = CompType::SNorm;
      fmt8.compCount = 2;
      return fmt8;
    case DXGI_FORMAT_R8G8_UNORM: fmt8.compCount = 2; return fmt8;

    case DXGI_FORMAT_R8_UINT: fmt8.compType = CompType::UInt; return fmt8;
    case DXGI_FORMAT_R8_SINT: fmt8.compType = CompType::SInt; return fmt8;
    case DXGI_FORMAT_R8_SNORM: fmt8.compType = CompType::SNorm; return fmt8;
    case DXGI_FORMAT_R8_UNORM: return fmt8;

    default: RDCWARN("Unsupported DXGI_FORMAT: %u", (uint32_t)format);
  }

  return ResourceFormat();
}

DXGI_FORMAT ResourceFormat2DXGIFormat(ResourceFormat format)
{
  if(format.Special())
  {
    switch(format.type)
    {
      case ResourceFormatType::BC1:
        return format.SRGBCorrected() ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
      case ResourceFormatType::BC2:
        return format.SRGBCorrected() ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM;
      case ResourceFormatType::BC3:
        return format.SRGBCorrected() ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
      case ResourceFormatType::BC4:
        return format.compType == CompType::UNorm ? DXGI_FORMAT_BC4_UNORM : DXGI_FORMAT_BC4_SNORM;
      case ResourceFormatType::BC5:
        return format.compType == CompType::UNorm ? DXGI_FORMAT_BC5_UNORM : DXGI_FORMAT_BC5_SNORM;
      case ResourceFormatType::BC6:
        return format.compType == CompType::UNorm ? DXGI_FORMAT_BC6H_UF16 : DXGI_FORMAT_BC6H_SF16;
      case ResourceFormatType::BC7:
        return format.SRGBCorrected() ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
      case ResourceFormatType::R10G10B10A2:
        return format.compType == CompType::UNorm ? DXGI_FORMAT_R10G10B10A2_UNORM
                                                  : DXGI_FORMAT_R10G10B10A2_UINT;
      case ResourceFormatType::R11G11B10: return DXGI_FORMAT_R11G11B10_FLOAT;
      case ResourceFormatType::R5G6B5:
        RDCASSERT(format.BGRAOrder());
        return DXGI_FORMAT_B5G6R5_UNORM;
      case ResourceFormatType::R5G5B5A1:
        RDCASSERT(format.BGRAOrder());
        return DXGI_FORMAT_B5G5R5A1_UNORM;
      case ResourceFormatType::R9G9B9E5: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
      case ResourceFormatType::R4G4B4A4:
        RDCASSERT(format.BGRAOrder());
        return DXGI_FORMAT_B4G4R4A4_UNORM;
      case ResourceFormatType::D24S8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
      case ResourceFormatType::D32S8: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
      case ResourceFormatType::S8: return DXGI_FORMAT_R8_UINT;
      default:
      case ResourceFormatType::R4G4:
      case ResourceFormatType::D16S8:
      case ResourceFormatType::ETC2:
      case ResourceFormatType::EAC:
      case ResourceFormatType::ASTC:
      case ResourceFormatType::YUV8:
      case ResourceFormatType::YUV10:
      case ResourceFormatType::YUV12:
      case ResourceFormatType::YUV16:
        RDCERR("Unsupported writing format %u", format.type);
        return DXGI_FORMAT_UNKNOWN;
    }
  }

  if(format.compCount == 4)
  {
    if(format.compByteWidth == 4)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R32G32B32A32_UINT;
        case CompType::SInt: return DXGI_FORMAT_R32G32B32A32_SINT;
        default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
      }
    }
    else if(format.compByteWidth == 2)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R16G16B16A16_UINT;
        case CompType::SInt: return DXGI_FORMAT_R16G16B16A16_SINT;
        case CompType::UNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case CompType::SNorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
        default: return DXGI_FORMAT_R16G16B16A16_FLOAT;
      }
    }
    else if(format.compByteWidth == 1)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R8G8B8A8_UINT;
        case CompType::SInt: return DXGI_FORMAT_R8G8B8A8_SINT;
        case CompType::SNorm: return DXGI_FORMAT_R8G8B8A8_SNORM;
        default:
        case CompType::UNorm:
        case CompType::UNormSRGB:
          if(format.SRGBCorrected())
          {
            if(format.BGRAOrder())
              return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            else
              return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
          }
          else
          {
            if(format.BGRAOrder())
              return DXGI_FORMAT_B8G8R8A8_UNORM;
            else
              return DXGI_FORMAT_R8G8B8A8_UNORM;
          }
      }
    }
    RDCERR("Unexpected component byte width %u for 4-component type", format.compByteWidth);
    return DXGI_FORMAT_UNKNOWN;
  }
  else if(format.compCount == 3)
  {
    if(format.compByteWidth == 4)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R32G32B32_UINT;
        case CompType::SInt: return DXGI_FORMAT_R32G32B32_SINT;
        default: return DXGI_FORMAT_R32G32B32_FLOAT;
      }
    }
    RDCERR("Unexpected component byte width %u for 3-component type", format.compByteWidth);
    return DXGI_FORMAT_UNKNOWN;
  }
  else if(format.compCount == 2)
  {
    if(format.compByteWidth == 4)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R32G32_UINT;
        case CompType::SInt: return DXGI_FORMAT_R32G32_SINT;
        default: return DXGI_FORMAT_R32G32_FLOAT;
      }
    }
    else if(format.compByteWidth == 2)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R16G16_UINT;
        case CompType::SInt: return DXGI_FORMAT_R16G16_SINT;
        case CompType::UNorm: return DXGI_FORMAT_R16G16_UNORM;
        case CompType::SNorm: return DXGI_FORMAT_R16G16_SNORM;
        default: return DXGI_FORMAT_R16G16_FLOAT;
      }
    }
    else if(format.compByteWidth == 1)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R8G8_UINT;
        case CompType::SInt: return DXGI_FORMAT_R8G8_SINT;
        case CompType::SNorm: return DXGI_FORMAT_R8G8_SNORM;
        default: return DXGI_FORMAT_R8G8_UNORM;
      }
    }
    RDCERR("Unexpected component byte width %u for 2-component type", format.compByteWidth);
    return DXGI_FORMAT_UNKNOWN;
  }
  else if(format.compCount == 1)
  {
    if(format.compByteWidth == 4)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R32_UINT;
        case CompType::SInt: return DXGI_FORMAT_R32_SINT;
        default: return DXGI_FORMAT_R32_FLOAT;
      }
    }
    else if(format.compByteWidth == 2)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R16_UINT;
        case CompType::SInt: return DXGI_FORMAT_R16_SINT;
        case CompType::UNorm: return DXGI_FORMAT_R16_UNORM;
        case CompType::SNorm: return DXGI_FORMAT_R16_SNORM;
        default: return DXGI_FORMAT_R16_FLOAT;
      }
    }
    else if(format.compByteWidth == 1)
    {
      switch(format.compType)
      {
        case CompType::UInt: return DXGI_FORMAT_R8_UINT;
        case CompType::SInt: return DXGI_FORMAT_R8_SINT;
        case CompType::SNorm: return DXGI_FORMAT_R8_SNORM;
        default: return DXGI_FORMAT_R8_UNORM;
      }
    }
    RDCERR("Unexpected component byte width %u for 1-component type", format.compByteWidth);
    return DXGI_FORMAT_UNKNOWN;
  }

  RDCERR("Unexpected component count %u", format.compCount);
  return DXGI_FORMAT_UNKNOWN;
}

bool write_dds_to_file(FILE *f, const dds_data &data)
{
  if(!f)
    return false;

  uint32_t magic = dds_fourcc;
  DDS_HEADER header;
  DDS_HEADER_DXT10 headerDXT10;
  RDCEraseEl(header);
  RDCEraseEl(headerDXT10);

  header.dwSize = sizeof(DDS_HEADER);

  header.ddspf.dwSize = sizeof(DDS_PIXELFORMAT);

  header.dwWidth = data.width;
  header.dwHeight = data.height;
  header.dwDepth = data.depth;
  header.dwMipMapCount = data.mips;

  header.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
  if(data.mips > 1)
    header.dwFlags |= DDSD_MIPMAPCOUNT;
  if(data.depth > 1)
    header.dwFlags |= DDSD_DEPTH;

  bool blockFormat = false;

  if(data.format.Special())
  {
    switch(data.format.type)
    {
      case ResourceFormatType::BC1:
      case ResourceFormatType::BC2:
      case ResourceFormatType::BC3:
      case ResourceFormatType::BC4:
      case ResourceFormatType::BC5:
      case ResourceFormatType::BC6:
      case ResourceFormatType::BC7: blockFormat = true; break;
      case ResourceFormatType::ETC2:
      case ResourceFormatType::EAC:
      case ResourceFormatType::ASTC:
      case ResourceFormatType::YUV8:
      case ResourceFormatType::YUV10:
      case ResourceFormatType::YUV12:
      case ResourceFormatType::YUV16:
        RDCERR("Unsupported file format, %u", data.format.type);
        return false;
      default: break;
    }
  }

  if(blockFormat)
    header.dwFlags |= DDSD_LINEARSIZE;
  else
    header.dwFlags |= DDSD_PITCH;

  header.dwCaps = DDSCAPS_TEXTURE;
  if(data.mips > 1)
    header.dwCaps |= DDSCAPS_MIPMAP;
  if(data.mips > 1 || data.slices > 1 || data.depth > 1)
    header.dwCaps |= DDSCAPS_COMPLEX;

  header.dwCaps2 = data.depth > 1 ? DDSCAPS2_VOLUME : 0;

  bool dx10Header = false;

  headerDXT10.dxgiFormat = ResourceFormat2DXGIFormat(data.format);
  headerDXT10.resourceDimension =
      data.depth > 1 ? D3D10_RESOURCE_DIMENSION_TEXTURE3D : D3D10_RESOURCE_DIMENSION_TEXTURE2D;
  headerDXT10.miscFlag = 0;
  headerDXT10.arraySize = data.slices;

  if(headerDXT10.dxgiFormat == DXGI_FORMAT_UNKNOWN)
  {
    RDCERR("Couldn't convert resource format to DXGI format");
    return false;
  }

  if(data.cubemap)
  {
    header.dwCaps2 = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES;
    headerDXT10.miscFlag |= DDS_RESOURCE_MISC_TEXTURECUBE;
    headerDXT10.arraySize /= 6;
  }

  if(headerDXT10.arraySize > 1)
    dx10Header = true;    // need to specify dx10 header to give array size

  uint32_t bytesPerPixel = 1;

  if(blockFormat)
  {
    int blockSize =
        (data.format.type == ResourceFormatType::BC1 || data.format.type == ResourceFormatType::BC4)
            ? 8
            : 16;
    header.dwPitchOrLinearSize = RDCMAX(1U, ((header.dwWidth + 3) / 4)) * blockSize;
  }
  else
  {
    switch(data.format.type)
    {
      case ResourceFormatType::S8: bytesPerPixel = 1; break;
      case ResourceFormatType::R10G10B10A2:
      case ResourceFormatType::R9G9B9E5:
      case ResourceFormatType::R11G11B10:
      case ResourceFormatType::D24S8: bytesPerPixel = 4; break;
      case ResourceFormatType::R5G6B5:
      case ResourceFormatType::R5G5B5A1:
      case ResourceFormatType::R4G4B4A4: bytesPerPixel = 2; break;
      case ResourceFormatType::D32S8: bytesPerPixel = 8; break;
      case ResourceFormatType::D16S8:
      case ResourceFormatType::YUV8:
      case ResourceFormatType::YUV10:
      case ResourceFormatType::YUV12:
      case ResourceFormatType::YUV16:
      case ResourceFormatType::R4G4:
        RDCERR("Unsupported file format %u", data.format.type);
        return false;
      default: bytesPerPixel = data.format.compCount * data.format.compByteWidth;
    }

    header.dwPitchOrLinearSize = header.dwWidth * bytesPerPixel;
  }

  // special case a couple of formats to write out non-DX10 style, for
  // backwards compatibility
  if(data.format.compByteWidth == 1 && data.format.compCount == 4 &&
     (data.format.compType == CompType::UNorm || data.format.compType == CompType::UNormSRGB))
  {
    header.ddspf.dwFlags = DDPF_RGBA;
    header.ddspf.dwRGBBitCount = 32;
    header.ddspf.dwRBitMask = 0x000000ff;
    header.ddspf.dwGBitMask = 0x0000ff00;
    header.ddspf.dwBBitMask = 0x00ff0000;
    header.ddspf.dwABitMask = 0xff000000;

    if(data.format.BGRAOrder())
      std::swap(header.ddspf.dwRBitMask, header.ddspf.dwBBitMask);
  }
  else if(data.format.type == ResourceFormatType::BC1)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', 'T', '1');
  }
  else if(data.format.type == ResourceFormatType::BC2)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', 'T', '3');
  }
  else if(data.format.type == ResourceFormatType::BC3)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', 'T', '5');
  }
  else if(data.format.type == ResourceFormatType::BC4 && data.format.compType == CompType::UNorm)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('B', 'C', '4', 'U');
  }
  else if(data.format.type == ResourceFormatType::BC4 && data.format.compType == CompType::SNorm)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('B', 'C', '4', 'S');
  }
  else if(data.format.type == ResourceFormatType::BC5 && data.format.compType == CompType::UNorm)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('A', 'T', 'I', '2');
  }
  else if(data.format.type == ResourceFormatType::BC5 && data.format.compType == CompType::SNorm)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('B', 'C', '5', 'S');
  }
  else
  {
    // just write out DX10 header
    dx10Header = true;
  }

  if(dx10Header)
  {
    header.ddspf.dwFlags = DDPF_FOURCC;
    header.ddspf.dwFourCC = MAKE_FOURCC('D', 'X', '1', '0');
  }

  {
    FileIO::fwrite(&magic, sizeof(magic), 1, f);
    FileIO::fwrite(&header, sizeof(header), 1, f);
    if(dx10Header)
      FileIO::fwrite(&headerDXT10, sizeof(headerDXT10), 1, f);

    int i = 0;
    for(int slice = 0; slice < RDCMAX(1, data.slices); slice++)
    {
      for(int mip = 0; mip < RDCMAX(1, data.mips); mip++)
      {
        int numdepths = RDCMAX(1, data.depth >> mip);
        for(int d = 0; d < numdepths; d++)
        {
          byte *bytedata = data.subdata[i];

          int rowlen = RDCMAX(1, data.width >> mip);
          int numRows = RDCMAX(1, data.height >> mip);
          int pitch = RDCMAX(1U, rowlen * bytesPerPixel);

          // pitch/rows are in blocks, not pixels, for block formats.
          if(blockFormat)
          {
            numRows = RDCMAX(1, numRows / 4);

            int blockSize = (data.format.type == ResourceFormatType::BC1 ||
                             data.format.type == ResourceFormatType::BC4)
                                ? 8
                                : 16;

            pitch = RDCMAX(blockSize, (((rowlen + 3) / 4)) * blockSize);
          }

          for(int row = 0; row < numRows; row++)
          {
            FileIO::fwrite(bytedata, 1, pitch, f);

            bytedata += pitch;
          }

          i++;
        }
      }
    }
  }

  return true;
}

bool is_dds_file(FILE *f)
{
  FileIO::fseek64(f, 0, SEEK_SET);

  uint32_t magic = 0;
  FileIO::fread(&magic, sizeof(magic), 1, f);

  FileIO::fseek64(f, 0, SEEK_SET);

  return magic == dds_fourcc;
}

dds_data load_dds_from_file(FILE *f)
{
  dds_data ret = {};
  dds_data error = {};

  FileIO::fseek64(f, 0, SEEK_SET);

  uint32_t magic = 0;
  FileIO::fread(&magic, sizeof(magic), 1, f);

  DDS_HEADER header = {};
  FileIO::fread(&header, sizeof(header), 1, f);

  bool dx10Header = false;
  DDS_HEADER_DXT10 headerDXT10 = {};

  if(header.ddspf.dwFlags == DDPF_FOURCC && header.ddspf.dwFourCC == MAKE_FOURCC('D', 'X', '1', '0'))
  {
    FileIO::fread(&headerDXT10, sizeof(headerDXT10), 1, f);
    dx10Header = true;
  }

  ret.width = RDCMAX(1U, header.dwWidth);
  ret.height = RDCMAX(1U, header.dwHeight);
  ret.depth = RDCMAX(1U, header.dwDepth);
  ret.slices = dx10Header ? RDCMAX(1U, headerDXT10.arraySize) : 1;
  ret.mips = RDCMAX(1U, header.dwMipMapCount);

  uint32_t cubeFlags = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES;

  if((header.dwCaps2 & cubeFlags) == cubeFlags && header.dwCaps & DDSCAPS_COMPLEX)
    ret.cubemap = true;

  if(dx10Header && headerDXT10.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE)
    ret.cubemap = true;

  if(ret.cubemap)
    ret.slices *= 6;

  if(dx10Header)
  {
    ret.format = DXGIFormat2ResourceFormat(headerDXT10.dxgiFormat);
    if(ret.format.type == ResourceFormatType::Undefined)
    {
      RDCWARN("Unsupported DXGI_FORMAT: %u", (uint32_t)headerDXT10.dxgiFormat);
      return error;
    }
  }
  else if(header.ddspf.dwFlags & DDPF_FOURCC)
  {
    switch(header.ddspf.dwFourCC)
    {
      case MAKE_FOURCC('D', 'X', 'T', '1'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC1_UNORM);
        break;
      case MAKE_FOURCC('D', 'X', 'T', '3'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC2_UNORM);
        break;
      case MAKE_FOURCC('D', 'X', 'T', '5'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC3_UNORM);
        break;
      case MAKE_FOURCC('A', 'T', 'I', '1'):
      case MAKE_FOURCC('B', 'C', '4', 'U'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC4_UNORM);
        break;
      case MAKE_FOURCC('B', 'C', '4', 'S'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC4_SNORM);
        break;
      case MAKE_FOURCC('A', 'T', 'I', '2'):
      case MAKE_FOURCC('B', 'C', '5', 'U'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC5_UNORM);
        break;
      case MAKE_FOURCC('B', 'C', '5', 'S'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_BC5_SNORM);
        break;
      case MAKE_FOURCC('R', 'G', 'B', 'G'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R8G8_B8G8_UNORM);
        break;
      case MAKE_FOURCC('G', 'R', 'G', 'B'):
        ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_G8R8_G8B8_UNORM);
        break;
      case 36: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R16G16B16A16_UNORM); break;
      case 110: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R16G16B16A16_SNORM); break;
      case 111: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R16_FLOAT); break;
      case 112: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R16G16_FLOAT); break;
      case 113: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R16G16B16A16_FLOAT); break;
      case 114: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R32_FLOAT); break;
      case 115: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R32G32_FLOAT); break;
      case 116: ret.format = DXGIFormat2ResourceFormat(DXGI_FORMAT_R32G32B32A32_FLOAT); break;
      default: RDCWARN("Unsupported FourCC: %08x", header.ddspf.dwFourCC); return error;
    }
  }
  else
  {
    if(header.ddspf.dwRGBBitCount != 32 && header.ddspf.dwRGBBitCount != 24 &&
       header.ddspf.dwRGBBitCount != 16 && header.ddspf.dwRGBBitCount != 8)
    {
      RDCWARN("Unsupported RGB bit count: %u", header.ddspf.dwRGBBitCount);
      return error;
    }

    ret.format.compByteWidth = 1;
    ret.format.compCount = uint8_t(header.ddspf.dwRGBBitCount / 8);
    ret.format.compType = CompType::UNorm;
    ret.format.type = ResourceFormatType::Regular;

    if(header.ddspf.dwBBitMask < header.ddspf.dwRBitMask)
      ret.format.SetBGRAOrder(true);
  }

  uint32_t bytesPerPixel = 1;
  switch(ret.format.type)
  {
    case ResourceFormatType::S8: bytesPerPixel = 1; break;
    case ResourceFormatType::R10G10B10A2:
    case ResourceFormatType::R9G9B9E5:
    case ResourceFormatType::R11G11B10:
    case ResourceFormatType::D24S8: bytesPerPixel = 4; break;
    case ResourceFormatType::R5G6B5:
    case ResourceFormatType::R5G5B5A1:
    case ResourceFormatType::R4G4B4A4: bytesPerPixel = 2; break;
    case ResourceFormatType::D32S8: bytesPerPixel = 8; break;
    case ResourceFormatType::D16S8:
    case ResourceFormatType::YUV8:
    case ResourceFormatType::YUV10:
    case ResourceFormatType::YUV12:
    case ResourceFormatType::YUV16:
    case ResourceFormatType::R4G4:
      RDCERR("Unsupported file format %u", ret.format.type);
      return error;
    default: bytesPerPixel = ret.format.compCount * ret.format.compByteWidth;
  }

  bool blockFormat = false;

  if(ret.format.Special())
  {
    switch(ret.format.type)
    {
      case ResourceFormatType::BC1:
      case ResourceFormatType::BC2:
      case ResourceFormatType::BC3:
      case ResourceFormatType::BC4:
      case ResourceFormatType::BC5:
      case ResourceFormatType::BC6:
      case ResourceFormatType::BC7: blockFormat = true; break;
      case ResourceFormatType::ETC2:
      case ResourceFormatType::EAC:
      case ResourceFormatType::ASTC:
      case ResourceFormatType::YUV8:
      case ResourceFormatType::YUV10:
      case ResourceFormatType::YUV12:
      case ResourceFormatType::YUV16:
        RDCERR("Unsupported file format, %u", ret.format.type);
        return error;
      default: break;
    }
  }

  ret.subsizes = new uint32_t[ret.slices * ret.mips];
  ret.subdata = new byte *[ret.slices * ret.mips];

  int i = 0;
  for(int slice = 0; slice < ret.slices; slice++)
  {
    for(int mip = 0; mip < ret.mips; mip++)
    {
      int rowlen = RDCMAX(1, ret.width >> mip);
      int numRows = RDCMAX(1, ret.height >> mip);
      int numdepths = RDCMAX(1, ret.depth >> mip);
      int pitch = RDCMAX(1U, rowlen * bytesPerPixel);

      // pitch/rows are in blocks, not pixels, for block formats.
      if(blockFormat)
      {
        numRows = RDCMAX(1, numRows / 4);

        int blockSize = (ret.format.type == ResourceFormatType::BC1 ||
                         ret.format.type == ResourceFormatType::BC4)
                            ? 8
                            : 16;

        pitch = RDCMAX(blockSize, (((rowlen + 3) / 4)) * blockSize);
      }

      ret.subsizes[i] = numdepths * numRows * pitch;

      byte *bytedata = ret.subdata[i] = new byte[ret.subsizes[i]];

      for(int d = 0; d < numdepths; d++)
      {
        for(int row = 0; row < numRows; row++)
        {
          FileIO::fread(bytedata, 1, pitch, f);

          bytedata += pitch;
        }
      }

      i++;
    }
  }

  return ret;
}
