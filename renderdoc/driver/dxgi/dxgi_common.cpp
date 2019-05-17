/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include "dxgi_common.h"
#include "common/common.h"
#include "common/threading.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"

// for GetDriverVersion()
#include <devpkey.h>
#include <setupapi.h>

UINT GetByteSize(int Width, int Height, int Depth, DXGI_FORMAT Format, int mip)
{
  UINT ret = RDCMAX(Width >> mip, 1) * RDCMAX(Height >> mip, 1) * RDCMAX(Depth >> mip, 1);

  switch(Format)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT: ret *= 16; break;
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT: ret *= 12; break;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: ret *= 8; break;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: ret *= 4; break;
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM: ret *= 2; break;
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: ret *= 1; break;
    case DXGI_FORMAT_R1_UNORM: ret = RDCMAX(ret / 8, 1U); break;
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
      ret = AlignUp4(RDCMAX(Width >> mip, 1)) * AlignUp4(RDCMAX(Height >> mip, 1)) *
            RDCMAX(Depth >> mip, 1);
      ret /= 2;
      break;
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      ret = AlignUp4(RDCMAX(Width >> mip, 1)) * AlignUp4(RDCMAX(Height >> mip, 1)) *
            RDCMAX(Depth >> mip, 1);
      ret *= 1;
      break;
    case DXGI_FORMAT_B4G4R4A4_UNORM:
      ret *= 2;    // 4 channels, half a byte each
      break;
    /*
     * YUV planar/packed subsampled textures.
     *
     * In each diagram we indicate (maybe part) of the data for a 4x4 texture:
     *
     * +---+---+---+---+
     * | 0 | 1 | 2 | 3 |
     * +---+---+---+---+
     * | 4 | 5 | 6 | 7 |
     * +---+---+---+---+
     * | 8 | 9 | A | B |
     * +---+---+---+---+
     * | C | D | E | F |
     * +---+---+---+---+
     *
     *
     * FOURCC decoding:
     *  - char 0: 'Y' = packed, 'P' = planar
     *  - char 1: '4' = 4:4:4, '2' = 4:2:2, '1' = 4:2:1, '0' = 4:2:0
     *  - char 2+3: '16' = 16-bit, '10' = 10-bit, '08' = 8-bit
     *
     * planar = Y is first, all together, then UV comes second.
     * packed = YUV is interleaved
     *
     * ======================= 4:4:4 lossless packed =========================
     *
     * Equivalent to uncompressed formats, just YUV instead of RGB. For 8-bit:
     *
     * pixel:      0            1            2            3
     * byte:  0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F
     *        Y0 U0 V0 A0  Y1 U1 V1 A1  Y2 U2 V2 A2  Y3 U3 V3 A3
     *
     * 16-bit is similar with two bytes per sample, 10-bit for uncompressed is
     * equivalent to R10G10B10A2 but with RGB=>YUV
     *
     * ============================ 4:2:2 packed =============================
     *
     * 50% horizontal subsampling packed, two Y samples for each U/V sample pair. For 8-bit:
     *
     * pixel:   0  |  1      2  |  3      4  |  5      6  |  7
     * byte:  0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F
     *        Y0 U0 Y1 V0  Y2 U1 Y3 V1  Y4 U2 Y5 V2  Y6 U3 Y7 V3
     *
     * 16-bit is similar with two bytes per sample, 10-bit is stored identically to 16-bit but in
     * the most significant bits:
     *
     * bit:    FEDCBA9876543210
     * 16-bit: XXXXXXXXXXXXXXXX
     * 10-bit: XXXXXXXXXX000000
     *
     * Since the data is unorm this just spaces out valid values.
     *
     * ============================ 4:2:0 planar =============================
     *
     * 50% horizontal and vertical subsampled planar, four Y samples for each U/V sample pair.
     * For 8-bit:
     *
     *
     * pixel: 0  1  2  3   4  5  6  7
     * byte:  0  1  2  3   4  5  6  7
     *        Y0 Y1 Y2 Y3  Y4 Y5 Y6 Y7
     *
     * pixel: 8  9  A  B   C  D  E  F
     * byte:  8  9  A  B   C  D  E  F
     *        Y8 Y9 Ya Yb  Yc Yd Ye Yf
     *
     *        ... all of the rest of Y luma ...
     *
     * pixel:  T&4 | 1&5    2&6 | 3&7
     * byte:  0  1  2  3   4  5  6  7
     *        U0 V0 U1 V1  U2 V2 U3 V3
     *
     * pixel:  8&C | 9&D    A&E | B&F
     * byte:  8  9  A  B   C  D  E  F
     *        U4 V4 U5 V5  U6 V6 U7 V7
     */
    case DXGI_FORMAT_AYUV:
      // 4:4:4 lossless packed, 8-bit. Equivalent size to R8G8B8A8
      ret *= 4;
      break;
    case DXGI_FORMAT_Y410:
      // 4:4:4 lossless packed. Equivalent size to R10G10B10A2, unlike most 10-bit/16-bit formats is
      // not equivalent to the 16-bit format.
      ret *= 4;
      break;
    case DXGI_FORMAT_Y416:
      // 4:4:4 lossless packed. Equivalent size to R16G16B16A16
      ret *= 8;
      break;
    case DXGI_FORMAT_NV12:
      // 4:2:0 planar. Since we can assume even width and height, resulting size is 1 byte per pixel
      // for luma, plus 1 byte per 2 pixels for chroma
      ret = ret + ret / 2;
      break;
    case DXGI_FORMAT_P010:
    // 10-bit formats are stored identically to 16-bit formats
    // deliberate fallthrough
    case DXGI_FORMAT_P016:
      // 4:2:0 planar but 16-bit, so pixelCount*2 + (pixelCount*2) / 2
      ret *= 2;
      ret = ret + ret / 2;
      break;
    case DXGI_FORMAT_420_OPAQUE:
      // same size as NV12 - planar 4:2:0 but opaque layout
      ret = ret + ret / 2;
      break;
    case DXGI_FORMAT_YUY2:
      // 4:2:2 packed 8-bit, so 1 byte per pixel for luma and 1 byte per pixel for chroma (2 chroma
      // samples, with 50% subsampling = 1 byte per pixel)
      ret *= 2;
      break;
    case DXGI_FORMAT_Y210:
    // 10-bit formats are stored identically to 16-bit formats
    // deliberate fallthrough
    case DXGI_FORMAT_Y216:
      // 4:2:2 packed 16-bit
      ret *= 4;
      break;
    case DXGI_FORMAT_NV11:
      // similar to NV11 - planar 4:1:1 4 horizontal downsampling but no vertical downsampling. For
      // size calculation amounts to the same result.
      ret = ret + ret / 2;
      break;
    case DXGI_FORMAT_AI44:
    // special format, 1 byte per pixel, palletised values in 4 most significant bits, alpha in 4
    // least significant bits.
    // deliberate fallthrough
    case DXGI_FORMAT_IA44:
      // same as above but swapped MSB/LSB
      break;
    case DXGI_FORMAT_P8:
      // 8 bits of palletised data
      break;
    case DXGI_FORMAT_A8P8:
      // 8 bits palletised data, 8 bits alpha data. Seems to be packed (no indication in docs of
      // planar)
      ret *= 2;
      break;
    case DXGI_FORMAT_P208:
      // 4:2:2 planar 8-bit. 1 byte per pixel of luma, then separately 1 byte per pixel of chroma.
      // Identical size to packed 4:2:2, just different layout
      ret *= 2;
      break;
    case DXGI_FORMAT_V208:
      // unclear, seems to be packed 4:4:0 8-bit. Thus 1 byte per pixel for luma, 2 chroma samples
      // every 2 rows = 1 byte per pixel for chroma
      ret *= 2;
      break;
    case DXGI_FORMAT_V408:
      // unclear, seems to be packed 4:4:4 8-bit
      ret *= 4;
      break;

    case DXGI_FORMAT_UNKNOWN:
      RDCERR("Getting byte size of unknown DXGI format");
      ret = 0;
      break;
    default: RDCERR("Unrecognised DXGI Format: %d", Format); break;
  }

  return ret;
}

UINT GetRowPitch(int Width, DXGI_FORMAT Format, int mip)
{
  // only YUV formats can have different rowpitch to their actual width
  if(!IsYUVFormat(Format))
    return GetByteSize(Width, 1, 1, Format, mip);

  UINT ret = RDCMAX(Width >> mip, 1);

  switch(Format)
  {
    case DXGI_FORMAT_AYUV:
      // 4:4:4 lossless packed, 8-bit. Equivalent size to R8G8B8A8
      ret *= 4;
      break;
    case DXGI_FORMAT_Y410:
      // 4:4:4 lossless packed. Equivalent size to R10G10B10A2, unlike most 10-bit/16-bit formats is
      // not equivalent to the 16-bit format.
      ret *= 4;
      break;
    case DXGI_FORMAT_Y416:
      // 4:4:4 lossless packed. Equivalent size to R16G16B16A16
      ret *= 8;
      break;
    case DXGI_FORMAT_NV12:
      // 4:2:0 planar. Since we can assume even width and height, resulting row pitch is 1 byte per
      // pixel - 1 byte luma each, and half subsampled chroma U/V in 1 byte total per pixel.
      break;
    case DXGI_FORMAT_P010:
    // 10-bit formats are stored identically to 16-bit formats
    // deliberate fallthrough
    case DXGI_FORMAT_P016:
      // Similar to NV12 but 16-bit elements
      ret *= 2;
      break;
    case DXGI_FORMAT_420_OPAQUE:
      // same size as NV12 - planar 4:2:0 but opaque layout
      break;
    case DXGI_FORMAT_YUY2:
      // 4:2:2 packed 8-bit, so 1 byte per pixel for luma and 1 byte per pixel for chroma (2 chroma
      // samples, with 50% subsampling = 1 byte per pixel)
      ret *= 2;
      break;
    case DXGI_FORMAT_Y210:
    // 10-bit formats are stored identically to 16-bit formats
    // deliberate fallthrough
    case DXGI_FORMAT_Y216:
      // 4:2:2 packed 16-bit
      ret *= 4;
      break;
    case DXGI_FORMAT_NV11:
      // similar to NV12 - planar 4:1:1 4 horizontal downsampling but no vertical downsampling. For
      // row pitch calculation amounts to the same result.
      ret = ret;
      break;
    case DXGI_FORMAT_AI44:
    // special format, 1 byte per pixel, palletised values in 4 most significant bits, alpha in 4
    // least significant bits.
    // deliberate fallthrough
    case DXGI_FORMAT_IA44:
      // same as above but swapped MSB/LSB
      break;
    case DXGI_FORMAT_P8:
      // 8 bits of palletised data
      break;
    case DXGI_FORMAT_A8P8:
      // 8 bits palletised data, 8 bits alpha data. Seems to be packed (no indication in docs of
      // planar)
      ret *= 2;
      break;
    case DXGI_FORMAT_P208:
      // 4:2:2 planar 8-bit. 1 byte per pixel of luma, then separately 1 byte per pixel of chroma.
      // Identical pitch to 4:2:0, just more rows in second plane
      break;
    case DXGI_FORMAT_V208:
      // unclear, seems to be packed 4:4:0 8-bit. Thus 1 byte per pixel for luma, 2 chroma samples
      // every 2 rows = 1 byte per pixel for chroma
      ret *= 2;
      break;
    case DXGI_FORMAT_V408:
      // unclear, seems to be packed 4:4:4 8-bit
      ret *= 4;
      break;

    case DXGI_FORMAT_UNKNOWN:
      RDCERR("Getting row pitch of unknown DXGI format");
      ret = 0;
      break;
    default: RDCERR("Unrecognised DXGI Format: %d", Format); break;
  }

  return ret;
}

bool IsBlockFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB: return true;
    default: break;
  }

  return false;
}

bool IsDepthFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:

    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS:

    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_D16_UNORM: return true;
  }

  return false;
}

bool IsDepthAndStencilFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:

    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS: return true;
  }

  return false;
}

bool IsTypelessFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC7_TYPELESS: return true;
  }

  return false;
}

bool IsUIntFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R8_UINT: return true;
  }

  return false;
}

bool IsIntFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R8_SINT: return true;
  }

  return false;
}

bool IsSRGBFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return true;

    default: break;
  }

  return false;
}

bool IsYUVFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408: return true;
    default: break;
  }

  return false;
}

bool IsYUVPlanarFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208: return true;
    default: break;
  }

  return false;
}

UINT GetYUVNumRows(DXGI_FORMAT f, UINT height)
{
  switch(f)
  {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
      // all of these are 4:2:0, so number of rows is equal to height + height/2
      return height + height / 2;
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208:
      // 4:1:1 and 4:2:2 have the same number of rows for chroma and luma planes, so we have
      // height * 2 rows
      return height * 2;
    default: break;
  }

  return height;
}

DXGI_FORMAT GetDepthTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;

    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;

    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_D16_UNORM;

    default: break;
  }
  return f;
}

DXGI_FORMAT GetNonSRGBFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM: return DXGI_FORMAT_BC1_UNORM_SRGB;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM: return DXGI_FORMAT_BC2_UNORM_SRGB;

    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM: return DXGI_FORMAT_BC3_UNORM_SRGB;

    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM: return DXGI_FORMAT_BC7_UNORM_SRGB;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM: return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

    default: break;
  }
  return f;
}

DXGI_FORMAT GetUnormTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_UNORM;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UINT: return DXGI_FORMAT_R10G10B10A2_UNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_UNORM;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_UNORM;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_UNORM;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT: return DXGI_FORMAT_R8_UNORM;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_UNORM;

    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM;

    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_SNORM: return DXGI_FORMAT_BC4_UNORM;

    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_SNORM: return DXGI_FORMAT_BC5_UNORM;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;

    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_SF16: return DXGI_FORMAT_BC6H_UF16;

    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetSnormTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_SNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_SNORM;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_SNORM;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_SNORM;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_SNORM;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: return DXGI_FORMAT_R8_SNORM;

    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM: return DXGI_FORMAT_BC4_SNORM;

    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM: return DXGI_FORMAT_BC5_SNORM;

    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16: return DXGI_FORMAT_BC6H_SF16;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetUIntTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_UINT;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_UINT;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_UINT;

    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_SINT: return DXGI_FORMAT_R32G32_UINT;

    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM: return DXGI_FORMAT_R10G10B10A2_UINT;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_UINT;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_UINT;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_SINT: return DXGI_FORMAT_R32_UINT;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_UINT;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_UINT;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: return DXGI_FORMAT_R8_UINT;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetSIntTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_SINT;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT: return DXGI_FORMAT_R32G32B32_SINT;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SINT;

    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT: return DXGI_FORMAT_R32G32_SINT;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SINT;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM: return DXGI_FORMAT_R16G16_SINT;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT: return DXGI_FORMAT_R32_SINT;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM: return DXGI_FORMAT_R8G8_SINT;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM: return DXGI_FORMAT_R16_SINT;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM: return DXGI_FORMAT_R8_SINT;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetFloatTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R32G32B32_UINT: return DXGI_FORMAT_R32G32B32_FLOAT;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R16G16B16A16_UINT: return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G32_UINT: return DXGI_FORMAT_R32G32_FLOAT;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UINT: return DXGI_FORMAT_R10G10B10A2_UNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM: return DXGI_FORMAT_R16G16_FLOAT;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT: return DXGI_FORMAT_R32_FLOAT;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM: return DXGI_FORMAT_R8G8_UNORM;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM: return DXGI_FORMAT_R16_FLOAT;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM: return DXGI_FORMAT_R8_UNORM;
  }

  return GetTypedFormat(f);
}

DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case DXGI_FORMAT_R32G32B32_TYPELESS: return DXGI_FORMAT_R32G32B32_FLOAT;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case DXGI_FORMAT_R32G32_TYPELESS: return DXGI_FORMAT_R32G32_FLOAT;

    case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_R32G8X24_TYPELESS;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_R16G16_TYPELESS: return DXGI_FORMAT_R16G16_FLOAT;

    case DXGI_FORMAT_R32_TYPELESS:
      return DXGI_FORMAT_R32_FLOAT;

    // maybe not valid casts?
    case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;

    case DXGI_FORMAT_R8G8_TYPELESS: return DXGI_FORMAT_R8G8_UNORM;

    case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;

    case DXGI_FORMAT_R8_TYPELESS: return DXGI_FORMAT_R8_UNORM;

    case DXGI_FORMAT_BC1_TYPELESS: return DXGI_FORMAT_BC1_UNORM;

    case DXGI_FORMAT_BC4_TYPELESS: return DXGI_FORMAT_BC4_UNORM;

    case DXGI_FORMAT_BC2_TYPELESS: return DXGI_FORMAT_BC2_UNORM;

    case DXGI_FORMAT_BC3_TYPELESS: return DXGI_FORMAT_BC3_UNORM;

    case DXGI_FORMAT_BC5_TYPELESS: return DXGI_FORMAT_BC5_UNORM;

    case DXGI_FORMAT_BC6H_TYPELESS: return DXGI_FORMAT_BC6H_UF16;

    case DXGI_FORMAT_BC7_TYPELESS: return DXGI_FORMAT_BC7_UNORM;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f, CompType typeHint)
{
  switch(f)
  {
    // these formats have multiple typed formats - use the hint to decide which to use

    case DXGI_FORMAT_R8_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R8_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R8_SINT;
      if(typeHint == CompType::SNorm)
        return DXGI_FORMAT_R8_SNORM;
      return DXGI_FORMAT_R8_UNORM;
    }

    case DXGI_FORMAT_R8G8_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R8G8_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R8G8_SINT;
      if(typeHint == CompType::SNorm)
        return DXGI_FORMAT_R8G8_SNORM;
      return DXGI_FORMAT_R8G8_UNORM;
    }

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R8G8B8A8_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R8G8B8A8_SINT;
      if(typeHint == CompType::SNorm)
        return DXGI_FORMAT_R8G8B8A8_SNORM;
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    case DXGI_FORMAT_R16_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R16_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R16_SINT;
      if(typeHint == CompType::SNorm)
        return DXGI_FORMAT_R16_SNORM;
      if(typeHint == CompType::Float)
        return DXGI_FORMAT_R16_FLOAT;
      if(typeHint == CompType::Depth)
        return DXGI_FORMAT_D16_UNORM;
      return DXGI_FORMAT_R16_UNORM;
    }

    case DXGI_FORMAT_R16G16_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R16G16_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R16G16_SINT;
      if(typeHint == CompType::SNorm)
        return DXGI_FORMAT_R16G16_SNORM;
      if(typeHint == CompType::Float)
        return DXGI_FORMAT_R16G16_FLOAT;
      return DXGI_FORMAT_R16G16_UNORM;
    }

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R16G16B16A16_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R16G16B16A16_SINT;
      if(typeHint == CompType::SNorm)
        return DXGI_FORMAT_R16G16B16A16_SNORM;
      if(typeHint == CompType::Float)
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
      return DXGI_FORMAT_R16G16B16A16_UNORM;
    }

    case DXGI_FORMAT_R32_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R32_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R32_SINT;
      if(typeHint == CompType::Depth)
        return DXGI_FORMAT_D32_FLOAT;
      return DXGI_FORMAT_R32_FLOAT;
    }

    case DXGI_FORMAT_R32G32_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R32G32_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R32G32_SINT;
      return DXGI_FORMAT_R32G32_FLOAT;
    }

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R32G32B32_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R32G32B32_SINT;
      return DXGI_FORMAT_R32G32B32_FLOAT;
    }

    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R32G32B32A32_UINT;
      if(typeHint == CompType::SInt)
        return DXGI_FORMAT_R32G32B32A32_SINT;
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }

    case DXGI_FORMAT_R32G8X24_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
      if(typeHint == CompType::Depth)
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
      return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    }

    case DXGI_FORMAT_R24G8_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
      if(typeHint == CompType::Depth)
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
      return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    }

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    {
      if(typeHint == CompType::UInt)
        return DXGI_FORMAT_R10G10B10A2_UINT;
      return DXGI_FORMAT_R10G10B10A2_UNORM;
    }

    case DXGI_FORMAT_BC4_TYPELESS:
      return typeHint == CompType::SNorm ? DXGI_FORMAT_BC4_SNORM : DXGI_FORMAT_BC4_UNORM;

    case DXGI_FORMAT_BC5_TYPELESS:
      return typeHint == CompType::SNorm ? DXGI_FORMAT_BC5_SNORM : DXGI_FORMAT_BC5_UNORM;

    case DXGI_FORMAT_BC6H_TYPELESS:
      return typeHint == CompType::SNorm ? DXGI_FORMAT_BC6H_SF16 : DXGI_FORMAT_BC6H_UF16;

    // these formats have only one valid non-typeless format (ignoring SRGB)
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_BC1_TYPELESS: return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS: return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS: return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS: return DXGI_FORMAT_BC7_UNORM;

    default: break;
  }

  return f;
}

DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_TYPELESS;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_TYPELESS;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_TYPELESS;

    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT: return DXGI_FORMAT_R32G32_TYPELESS;

    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:    // maybe not valid cast?
      return DXGI_FORMAT_R10G10B10A2_TYPELESS;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_TYPELESS;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_TYPELESS;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:    // maybe not valid cast?
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
      return DXGI_FORMAT_R32_TYPELESS;

    // maybe not valid casts?
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:    // maybe not valid cast?
    case DXGI_FORMAT_G8R8_G8B8_UNORM:    // maybe not valid cast?
      return DXGI_FORMAT_B8G8R8A8_TYPELESS;

    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_TYPELESS;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_TYPELESS;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_TYPELESS;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: return DXGI_FORMAT_R8_TYPELESS;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_TYPELESS;

    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM: return DXGI_FORMAT_BC4_TYPELESS;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_TYPELESS;

    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_TYPELESS;

    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM: return DXGI_FORMAT_BC5_TYPELESS;

    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16: return DXGI_FORMAT_BC6H_TYPELESS;

    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_TYPELESS;

    case DXGI_FORMAT_R1_UNORM:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408:
    case DXGI_FORMAT_B4G4R4A4_UNORM: return f;

    case DXGI_FORMAT_UNKNOWN: return DXGI_FORMAT_UNKNOWN;

    default: RDCERR("Unrecognised DXGI Format: %d", f); return DXGI_FORMAT_UNKNOWN;
  }
}

DXGI_FORMAT GetYUVViewPlane0Format(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_AYUV: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_Y410: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_Y416: return DXGI_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P208: return DXGI_FORMAT_R8_UNORM;
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016: return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_YUY2: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216: return DXGI_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44: return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_P8: return DXGI_FORMAT_R8_UNORM;
    case DXGI_FORMAT_A8P8: return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_420_OPAQUE: return DXGI_FORMAT_UNKNOWN;
    default: break;
  }

  return f;
}

DXGI_FORMAT GetYUVViewPlane1Format(DXGI_FORMAT f)
{
  switch(f)
  {
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P208: return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016: return DXGI_FORMAT_R16G16_UNORM;
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408: return DXGI_FORMAT_UNKNOWN;
    default: break;
  }

  return f;
}

void GetYUVShaderParameters(DXGI_FORMAT f, Vec4u &YUVDownsampleRate, Vec4u &YUVAChannels)
{
  YUVDownsampleRate = {};
  YUVAChannels = {};

// YUVDownsampleRate = { horizontal downsampling, vertical downsampling, # planes, # bits }
#define PACKED_444(bits) \
  {                      \
    1, 1, 1, bits        \
  }
#define PACKED_422(bits) \
  {                      \
    2, 1, 1, bits        \
  }
#define PACKED_420(bits) \
  {                      \
    2, 2, 1, bits        \
  }
#define PLANAR_422(bits) \
  {                      \
    2, 1, 2, bits        \
  }
#define PLANAR_420(bits) \
  {                      \
    2, 2, 2, bits        \
  }

  // YUVAChannels = { Y index, U index, V index, A index }
  // where index is 0,1,2,3 for rgba in first texture, 0,1,2,3 for rgba in second texture
  // 0xff for alpha means not available

  switch(f)
  {
    case DXGI_FORMAT_AYUV:
      YUVDownsampleRate = PACKED_444(8);
      YUVAChannels = {2, 1, 0, 3};
      break;
    case DXGI_FORMAT_Y410:
      YUVDownsampleRate = PACKED_444(10);
      YUVAChannels = {1, 0, 2, 3};
      break;
    case DXGI_FORMAT_Y416:
      YUVDownsampleRate = PACKED_444(16);
      YUVAChannels = {1, 0, 2, 3};
      break;
    case DXGI_FORMAT_NV12:
      YUVDownsampleRate = PLANAR_420(8);
      YUVAChannels = {0, 4, 5, 0xff};
      break;
    case DXGI_FORMAT_P010:
      YUVDownsampleRate = PLANAR_420(10);
      YUVAChannels = {0, 4, 5, 0xff};
      break;
    case DXGI_FORMAT_P016:
      YUVDownsampleRate = PLANAR_420(16);
      YUVAChannels = {0, 4, 5, 0xff};
      break;
    case DXGI_FORMAT_YUY2:
      YUVDownsampleRate = PACKED_422(8);
      YUVAChannels = {0, 1, 3, 0xff};
      break;
    case DXGI_FORMAT_Y210:
      YUVDownsampleRate = PACKED_422(10);
      YUVAChannels = {0, 1, 3, 0xff};
      break;
    case DXGI_FORMAT_Y216:
      YUVDownsampleRate = PACKED_422(16);
      YUVAChannels = {0, 1, 3, 0xff};
      break;
    case DXGI_FORMAT_P208:
      YUVDownsampleRate = PLANAR_422(8);
      YUVAChannels = {0, 4, 5, 0xff};
      break;
    default: break;
  }
}

D3D_PRIMITIVE_TOPOLOGY MakeD3DPrimitiveTopology(Topology Topo)
{
  switch(Topo)
  {
    case Topology::LineLoop:
    case Topology::TriangleFan: RDCWARN("Unsupported primitive topology on D3D: %x", Topo); break;
    default:
    case Topology::Unknown: return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    case Topology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case Topology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case Topology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case Topology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case Topology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case Topology::LineList_Adj: return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
    case Topology::LineStrip_Adj: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
    case Topology::TriangleList_Adj: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    case Topology::TriangleStrip_Adj: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
    case Topology::PatchList_1CPs:
    case Topology::PatchList_2CPs:
    case Topology::PatchList_3CPs:
    case Topology::PatchList_4CPs:
    case Topology::PatchList_5CPs:
    case Topology::PatchList_6CPs:
    case Topology::PatchList_7CPs:
    case Topology::PatchList_8CPs:
    case Topology::PatchList_9CPs:
    case Topology::PatchList_10CPs:
    case Topology::PatchList_11CPs:
    case Topology::PatchList_12CPs:
    case Topology::PatchList_13CPs:
    case Topology::PatchList_14CPs:
    case Topology::PatchList_15CPs:
    case Topology::PatchList_16CPs:
    case Topology::PatchList_17CPs:
    case Topology::PatchList_18CPs:
    case Topology::PatchList_19CPs:
    case Topology::PatchList_20CPs:
    case Topology::PatchList_21CPs:
    case Topology::PatchList_22CPs:
    case Topology::PatchList_23CPs:
    case Topology::PatchList_24CPs:
    case Topology::PatchList_25CPs:
    case Topology::PatchList_26CPs:
    case Topology::PatchList_27CPs:
    case Topology::PatchList_28CPs:
    case Topology::PatchList_29CPs:
    case Topology::PatchList_30CPs:
    case Topology::PatchList_31CPs:
    case Topology::PatchList_32CPs:
      return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST +
                                    PatchList_Count(Topo) - 1);
  }

  return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

void WarnUnknownGUID(const char *name, REFIID riid)
{
  static Threading::CriticalSection lock;
  // we use a vector here, because the number of *distinct* unknown GUIDs encountered is likely to
  // be low (e.g. less than 10).
  static std::vector<rdcpair<IID, int> > warned;

  {
    SCOPED_LOCK(lock);

    for(rdcpair<IID, int> &w : warned)
    {
      if(w.first == riid)
      {
        w.second++;
        if(w.second > 5)
          return;

        RDCWARN("Querying %s for interface: %s", name, ToStr(riid).c_str());
        return;
      }
    }

    RDCWARN("Querying %s for interface: %s", name, ToStr(riid).c_str());
    warned.push_back(make_rdcpair(riid, 1));
  }
}

static std::string GetDeviceProperty(HDEVINFO devs, PSP_DEVINFO_DATA data, const DEVPROPKEY *key)
{
  DEVPROPTYPE type = {};
  DWORD bufSize = 0;

  // this ALWAYS fails, we need to check if the er ror was just an insufficient buffer.
  SetupDiGetDevicePropertyW(devs, data, key, &type, NULL, 0, &bufSize, 0);

  if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return "";

  RDCASSERTEQUAL((uint32_t)type, DEVPROP_TYPE_STRING);

  std::wstring string;
  string.resize(bufSize);
  BOOL success =
      SetupDiGetDevicePropertyW(devs, data, key, &type, (PBYTE)string.data(), bufSize, &bufSize, 0);

  if(!success)
    return "";

  return StringFormat::Wide2UTF8(string);
}

static uint32_t HexToInt(char hex)
{
  if(hex >= 'a' && hex <= 'f')
    return (hex - 'a') + 0xa;
  else if(hex >= 'A' && hex <= 'F')
    return (hex - 'A') + 0xa;
  else if(hex >= '0' && hex <= '9')
    return hex - '0';

  return 0;
}

std::string GetDriverVersion(DXGI_ADAPTER_DESC &desc)
{
  std::string device = StringFormat::Wide2UTF8(desc.Description);

  // fixed GUID for graphics drivers, from
  // https://msdn.microsoft.com/en-us/library/windows/hardware/ff553426%28v=vs.85%29.aspx
  GUID display_class = {0x4d36e968, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

  HDEVINFO devs = SetupDiGetClassDevs(&display_class, NULL, NULL, DIGCF_PRESENT);

  if(devs == NULL)
  {
    RDCERR("Couldn't enumerate graphics adapters: %d", GetLastError());
    return device;
  }

  std::string driverVersion = "";

  DWORD idx = 0;
  SP_DEVINFO_DATA data = {};
  data.cbSize = sizeof(data);
  while(SetupDiEnumDeviceInfo(devs, idx, &data))
  {
    std::string version = GetDeviceProperty(devs, &data, &DEVPKEY_Device_DriverVersion);

    if(version.empty())
    {
      SetupDiDestroyDeviceInfoList(devs);
      return device;
    }

    // if we got a version, and didn't have one yet, set it
    if(driverVersion.empty())
      driverVersion = version;

    std::string pciid = GetDeviceProperty(devs, &data, &DEVPKEY_Device_MatchingDeviceId);

    if(pciid.empty())
    {
      SetupDiDestroyDeviceInfoList(devs);
      return device;
    }

    pciid = strlower(pciid);

    UINT VendorId = 0, DeviceId = 0;

    char *end = &(*pciid.end());

    const char *ven = strstr(pciid.c_str(), "ven_");
    if(ven && ven + 8 <= end)
    {
      ven += 4;
      VendorId =
          HexToInt(ven[0]) << 12 | HexToInt(ven[1]) << 8 | HexToInt(ven[2]) << 4 | HexToInt(ven[3]);
    }

    const char *dev = strstr(pciid.c_str(), "dev_");
    if(dev && dev + 8 <= end)
    {
      dev += 4;
      DeviceId =
          HexToInt(dev[0]) << 12 | HexToInt(dev[1]) << 8 | HexToInt(dev[2]) << 4 | HexToInt(dev[3]);
    }

    // if the PCI id matches, take it
    if(VendorId == desc.VendorId && DeviceId == desc.DeviceId)
      driverVersion = version;

    // move to the next device
    RDCEraseEl(data);
    data.cbSize = sizeof(data);
    idx++;
  }

  SetupDiDestroyDeviceInfoList(devs);
  return device + " " + driverVersion;
}

Topology MakePrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY Topo)
{
  switch(Topo)
  {
    default:
    case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED: break;
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: return Topology::PointList;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST: return Topology::LineList;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: return Topology::LineStrip;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST: return Topology::TriangleList;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return Topology::TriangleStrip;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: return Topology::LineList_Adj;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ: return Topology::LineStrip_Adj;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ: return Topology::TriangleList_Adj;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ: return Topology::TriangleStrip_Adj;
    case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
      return PatchList_Topology(Topo - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1);
  }

  return Topology::Unknown;
}

DXGI_FORMAT MakeDXGIFormat(ResourceFormat fmt)
{
  DXGI_FORMAT ret = DXGI_FORMAT_UNKNOWN;

  if(fmt.Special())
  {
    switch(fmt.type)
    {
      case ResourceFormatType::BC1: ret = DXGI_FORMAT_BC1_UNORM; break;
      case ResourceFormatType::BC2: ret = DXGI_FORMAT_BC2_UNORM; break;
      case ResourceFormatType::BC3: ret = DXGI_FORMAT_BC3_UNORM; break;
      case ResourceFormatType::BC4: ret = DXGI_FORMAT_BC4_UNORM; break;
      case ResourceFormatType::BC5: ret = DXGI_FORMAT_BC5_UNORM; break;
      case ResourceFormatType::BC6: ret = DXGI_FORMAT_BC6H_UF16; break;
      case ResourceFormatType::BC7: ret = DXGI_FORMAT_BC7_UNORM; break;
      case ResourceFormatType::R10G10B10A2:
        if(fmt.compType == CompType::UNorm)
          ret = DXGI_FORMAT_R10G10B10A2_UNORM;
        else if(fmt.compType == CompType::Float)
          ret = DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
        else
          ret = DXGI_FORMAT_R10G10B10A2_UINT;
        break;
      case ResourceFormatType::R11G11B10: ret = DXGI_FORMAT_R11G11B10_FLOAT; break;
      case ResourceFormatType::R5G6B5:
        // only support bgra order
        if(!fmt.BGRAOrder())
          return DXGI_FORMAT_UNKNOWN;
        ret = DXGI_FORMAT_B5G6R5_UNORM;
        break;
      case ResourceFormatType::R5G5B5A1:
        // only support bgra order
        if(!fmt.BGRAOrder())
          return DXGI_FORMAT_UNKNOWN;
        ret = DXGI_FORMAT_B5G5R5A1_UNORM;
        break;
      case ResourceFormatType::R9G9B9E5: ret = DXGI_FORMAT_R9G9B9E5_SHAREDEXP; break;
      case ResourceFormatType::R4G4B4A4:
        // only support bgra order
        if(!fmt.BGRAOrder())
          return DXGI_FORMAT_UNKNOWN;
        ret = DXGI_FORMAT_B4G4R4A4_UNORM;
        break;
      case ResourceFormatType::D24S8: ret = DXGI_FORMAT_R24G8_TYPELESS; break;
      case ResourceFormatType::D32S8: ret = DXGI_FORMAT_R32G8X24_TYPELESS; break;
      case ResourceFormatType::YUV8:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();
        if(subsampling == 444)
        {
          // only support AYUV - 4 components
          if(fmt.compCount != 4)
            return DXGI_FORMAT_UNKNOWN;

          // only support packed 4:4:4
          return planeCount == 1 ? DXGI_FORMAT_AYUV : DXGI_FORMAT_UNKNOWN;
        }
        else if(subsampling == 422)
        {
          // only support 3 components
          if(fmt.compCount != 3)
            return DXGI_FORMAT_UNKNOWN;

          // packed 4:2:2
          if(planeCount == 1)
            return DXGI_FORMAT_YUY2;
          // planar 4:2:2
          else if(planeCount == 2)
            return DXGI_FORMAT_P208;

          return DXGI_FORMAT_UNKNOWN;
        }
        else if(subsampling == 420)
        {
          // only support 3 components
          if(fmt.compCount != 3)
            return DXGI_FORMAT_UNKNOWN;

          // only support 2-planar 4:2:0
          return planeCount == 2 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_UNKNOWN;
        }
        break;
      }
      case ResourceFormatType::YUV10:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();
        if(subsampling == 444)
        {
          // only support Y410 - 4 components
          if(fmt.compCount != 4)
            return DXGI_FORMAT_UNKNOWN;

          // only support packed 4:4:4
          return planeCount == 1 ? DXGI_FORMAT_Y410 : DXGI_FORMAT_UNKNOWN;
        }
        else if(subsampling == 422)
        {
          // only support 3 components
          if(fmt.compCount != 3)
            return DXGI_FORMAT_UNKNOWN;

          // only support packed 4:2:2
          return planeCount == 1 ? DXGI_FORMAT_Y210 : DXGI_FORMAT_UNKNOWN;
        }
        else if(subsampling == 420)
        {
          // only support 3 components
          if(fmt.compCount != 3)
            return DXGI_FORMAT_UNKNOWN;

          // only support 2-planar 4:2:0
          return planeCount == 2 ? DXGI_FORMAT_P010 : DXGI_FORMAT_UNKNOWN;
        }
        break;
      }
      case ResourceFormatType::YUV12:
      {
        // no 12-bit YUV format support
        return DXGI_FORMAT_UNKNOWN;
      }
      case ResourceFormatType::YUV16:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();
        if(subsampling == 444)
        {
          // only support Y416 - 4 components
          if(fmt.compCount != 4)
            return DXGI_FORMAT_UNKNOWN;

          // only support packed 4:4:4
          return planeCount == 1 ? DXGI_FORMAT_Y416 : DXGI_FORMAT_UNKNOWN;
        }
        else if(subsampling == 422)
        {
          // only support 3 components
          if(fmt.compCount != 3)
            return DXGI_FORMAT_UNKNOWN;

          // only support packed 4:2:2
          return planeCount == 1 ? DXGI_FORMAT_Y216 : DXGI_FORMAT_UNKNOWN;
        }
        else if(subsampling == 420)
        {
          // only support 3 components
          if(fmt.compCount != 3)
            return DXGI_FORMAT_UNKNOWN;

          // only support 2-planar 4:2:0
          return planeCount == 2 ? DXGI_FORMAT_P016 : DXGI_FORMAT_UNKNOWN;
        }
        break;
      }
      case ResourceFormatType::S8:       // D3D has no stencil-only format
      case ResourceFormatType::D16S8:    // D3D has no D16S8 format
      default: return DXGI_FORMAT_UNKNOWN;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.compByteWidth == 4)
      ret = DXGI_FORMAT_R32G32B32A32_TYPELESS;
    else if(fmt.compByteWidth == 2)
      ret = DXGI_FORMAT_R16G16B16A16_TYPELESS;
    else if(fmt.compByteWidth == 1)
      ret = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    else
      return DXGI_FORMAT_UNKNOWN;

    if(fmt.BGRAOrder())
      ret = DXGI_FORMAT_B8G8R8A8_UNORM;
  }
  else if(fmt.compCount == 3)
  {
    if(fmt.compByteWidth == 4)
      ret = DXGI_FORMAT_R32G32B32_TYPELESS;
    else
      return DXGI_FORMAT_UNKNOWN;
  }
  else if(fmt.compCount == 2)
  {
    if(fmt.compByteWidth == 4)
      ret = DXGI_FORMAT_R32G32_TYPELESS;
    else if(fmt.compByteWidth == 2)
      ret = DXGI_FORMAT_R16G16_TYPELESS;
    else if(fmt.compByteWidth == 1)
      ret = DXGI_FORMAT_R8G8_TYPELESS;
    else
      return DXGI_FORMAT_UNKNOWN;
  }
  else if(fmt.compCount == 1)
  {
    if(fmt.compByteWidth == 4)
      ret = DXGI_FORMAT_R32_TYPELESS;
    else if(fmt.compByteWidth == 2)
      ret = DXGI_FORMAT_R16_TYPELESS;
    else if(fmt.compByteWidth == 1)
      ret = DXGI_FORMAT_R8_TYPELESS;
    else
      return DXGI_FORMAT_UNKNOWN;
  }
  else
  {
    return DXGI_FORMAT_UNKNOWN;
  }

  if(fmt.compType == CompType::Typeless)
  {
    ret = GetTypelessFormat(ret);
  }
  else if(fmt.compType == CompType::Float)
  {
    ret = GetFloatTypedFormat(ret);
  }
  else if(fmt.compType == CompType::Depth)
  {
    ret = GetDepthTypedFormat(ret);
  }
  else if(fmt.compType == CompType::UNorm)
  {
    ret = GetUnormTypedFormat(ret);
  }
  else if(fmt.compType == CompType::SNorm)
  {
    ret = GetSnormTypedFormat(ret);
  }
  else if(fmt.compType == CompType::UInt)
  {
    ret = GetUIntTypedFormat(ret);
  }
  else if(fmt.compType == CompType::SInt)
  {
    ret = GetSIntTypedFormat(ret);
  }
  else if(fmt.compType == CompType::UNormSRGB)
  {
    ret = GetSRGBFormat(ret);
  }
  else
  {
    RDCERR("Unexpected component type %x", fmt.compType);
    return DXGI_FORMAT_UNKNOWN;
  }

  return ret;
}

ResourceFormat MakeResourceFormat(DXGI_FORMAT fmt)
{
  ResourceFormat ret;

  ret.compCount = ret.compByteWidth = 0;
  ret.compType = CompType::Float;

  switch(fmt)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_B4G4R4A4_UNORM: ret.compCount = 4; break;
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT: ret.compCount = 3; break;
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT: ret.compCount = 2; break;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: ret.compCount = 1; break;

    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:

    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM: ret.compCount = 2; break;

    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:

    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16: ret.compCount = 3; break;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB: ret.compCount = 4; break;

    case DXGI_FORMAT_R1_UNORM:

    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM: ret.compCount = 1; break;

    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416: ret.compCount = 4; break;
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408: ret.compCount = 3; break;
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8: ret.compCount = 2; break;

    case DXGI_FORMAT_UNKNOWN:
    case DXGI_FORMAT_FORCE_UINT: ret.compCount = 0; break;
  }

  switch(fmt)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT: ret.compByteWidth = 4; break;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT: ret.compByteWidth = 2; break;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: ret.compByteWidth = 1; break;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: ret.compByteWidth = 1; break;

    default: ret.compByteWidth = 0; break;
  }

  switch(fmt)
  {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8_TYPELESS: ret.compType = CompType::Typeless; break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R16_FLOAT: ret.compType = CompType::Float; break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_A8_UNORM: ret.compType = CompType::UNorm; break;
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R8_SNORM: ret.compType = CompType::SNorm; break;
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R8_UINT: ret.compType = CompType::UInt; break;
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R8_SINT: ret.compType = CompType::SInt; break;

    case DXGI_FORMAT_R10G10B10A2_UINT: ret.compType = CompType::UInt; break;
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: ret.compType = CompType::Float; break;
    case DXGI_FORMAT_R10G10B10A2_UNORM: ret.compType = CompType::UNorm; break;

    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R11G11B10_FLOAT: ret.compType = CompType::Float; break;

    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_SF16: ret.compType = CompType::SNorm; break;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS: ret.compType = CompType::Typeless; break;
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_D16_UNORM: ret.compType = CompType::Depth; break;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC7_TYPELESS: ret.compType = CompType::Typeless; break;
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R1_UNORM:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC7_UNORM: ret.compType = CompType::UNorm; break;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB: ret.compType = CompType::UNormSRGB; break;

    case DXGI_FORMAT_UNKNOWN:
    case DXGI_FORMAT_FORCE_UINT: ret.compType = CompType::Typeless; break;

    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408: ret.compType = CompType::UNorm; break;
  }

  switch(fmt)
  {
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: ret.SetBGRAOrder(true); break;
    default: break;
  }

  ret.type = ResourceFormatType::Regular;

  switch(fmt)
  {
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS: ret.type = ResourceFormatType::D24S8; break;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS: ret.type = ResourceFormatType::D32S8; break;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM: ret.type = ResourceFormatType::BC1; break;
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM: ret.type = ResourceFormatType::BC2; break;
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM: ret.type = ResourceFormatType::BC3; break;
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM: ret.type = ResourceFormatType::BC4; break;
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM: ret.type = ResourceFormatType::BC5; break;
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC6H_TYPELESS: ret.type = ResourceFormatType::BC6; break;
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM: ret.type = ResourceFormatType::BC7; break;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: ret.type = ResourceFormatType::R10G10B10A2; break;
    case DXGI_FORMAT_R11G11B10_FLOAT: ret.type = ResourceFormatType::R11G11B10; break;
    case DXGI_FORMAT_B5G6R5_UNORM:
      ret.type = ResourceFormatType::R5G6B5;
      ret.SetBGRAOrder(true);
      break;
    case DXGI_FORMAT_B5G5R5A1_UNORM:
      ret.type = ResourceFormatType::R5G5B5A1;
      ret.SetBGRAOrder(true);
      break;
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: ret.type = ResourceFormatType::R9G9B9E5; break;

    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_P208:
    {
      ret.type = ResourceFormatType::YUV8;

      switch(fmt)
      {
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y410: ret.type = ResourceFormatType::YUV10;
        default: break;
      }

      switch(fmt)
      {
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_Y416: ret.type = ResourceFormatType::YUV16;
        default: break;
      }

      switch(fmt)
      {
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416: ret.SetYUVSubsampling(444);
        default: break;
      }

      switch(fmt)
      {
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_P208: ret.SetYUVSubsampling(422);
        default: break;
      }

      switch(fmt)
      {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016: ret.SetYUVSubsampling(420);
        default: break;
      }

      switch(fmt)
      {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_P208: ret.SetYUVPlaneCount(2);
      }

      break;
    }
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_V208:
    case DXGI_FORMAT_V408:
    case DXGI_FORMAT_420_OPAQUE:
      RDCERR("Unsupported YUV format %s", ToStr(fmt).c_str());
      ret.type = ResourceFormatType::Undefined;
      break;

    case DXGI_FORMAT_B4G4R4A4_UNORM:
      ret.type = ResourceFormatType::R4G4B4A4;
      ret.SetBGRAOrder(true);
      break;

    case DXGI_FORMAT_UNKNOWN: ret.type = ResourceFormatType::Undefined; break;

    default: break;
  }

  return ret;
}

// shared between D3D11 and D3D12 as D3Dxx_RECT
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, RECT &el)
{
  // avoid serialising 'long' directly as we pretend it's only used for HRESULT
  SERIALISE_MEMBER_TYPED(int32_t, left);
  SERIALISE_MEMBER_TYPED(int32_t, top);
  SERIALISE_MEMBER_TYPED(int32_t, right);
  SERIALISE_MEMBER_TYPED(int32_t, bottom);
}

INSTANTIATE_SERIALISE_TYPE(RECT);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, IID &el)
{
  SERIALISE_MEMBER_TYPED(uint32_t, Data1);
  SERIALISE_MEMBER_TYPED(uint16_t, Data2);
  SERIALISE_MEMBER_TYPED(uint16_t, Data3);
  SERIALISE_MEMBER(Data4);
}

INSTANTIATE_SERIALISE_TYPE(IID);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, DXGI_SAMPLE_DESC &el)
{
  SERIALISE_MEMBER(Count);
  SERIALISE_MEMBER(Quality);
}

INSTANTIATE_SERIALISE_TYPE(DXGI_SAMPLE_DESC);

#if ENABLED(ENABLE_UNIT_TESTS)
#include "3rdparty/catch/catch.hpp"

TEST_CASE("DXGI formats", "[format][d3d]")
{
  // must be updated by hand
  DXGI_FORMAT maxFormat = DXGI_FORMAT_V408;

  // we want to skip formats that we deliberately don't represent or handle.
  auto isUnsupportedFormat = [](DXGI_FORMAT f) {
    // gap in DXGI_FORMAT enum
    if(f > DXGI_FORMAT_B4G4R4A4_UNORM && f < DXGI_FORMAT_P208)
      return true;
    return (f == DXGI_FORMAT_R1_UNORM || f == DXGI_FORMAT_A8_UNORM ||
            f == DXGI_FORMAT_R8G8_B8G8_UNORM || f == DXGI_FORMAT_G8R8_G8B8_UNORM ||
            f == DXGI_FORMAT_B8G8R8X8_TYPELESS || f == DXGI_FORMAT_B8G8R8X8_UNORM ||
            f == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB || f == DXGI_FORMAT_NV11 ||
            f == DXGI_FORMAT_AI44 || f == DXGI_FORMAT_IA44 || f == DXGI_FORMAT_P8 ||
            f == DXGI_FORMAT_A8P8 || f == DXGI_FORMAT_P208 || f == DXGI_FORMAT_V208 ||
            f == DXGI_FORMAT_V408 || f == DXGI_FORMAT_420_OPAQUE);
  };

  SECTION("Only DXGI_FORMAT_UNKNOWN is ResourceFormatType::Undefined")
  {
    for(DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; f <= maxFormat; f = DXGI_FORMAT(f + 1))
    {
      if(isUnsupportedFormat(f))
        continue;

      ResourceFormat fmt = MakeResourceFormat(f);

      if(f == DXGI_FORMAT_UNKNOWN)
        CHECK(fmt.type == ResourceFormatType::Undefined);
      else
        CHECK(fmt.type != ResourceFormatType::Undefined);
    }
  };

  SECTION("MakeDXGIFormat is reflexive with MakeResourceFormat")
  {
    for(DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; f <= maxFormat; f = DXGI_FORMAT(f + 1))
    {
      if(isUnsupportedFormat(f))
        continue;

      ResourceFormat fmt = MakeResourceFormat(f);

      DXGI_FORMAT original = f;
      DXGI_FORMAT reconstructed = MakeDXGIFormat(fmt);

      // we are OK with remapping these formats to a single value instead of preserving the view
      // type.
      if(f == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS || f == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT)
      {
        CHECK(reconstructed == DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
      }
      else if(f == DXGI_FORMAT_R24_UNORM_X8_TYPELESS || f == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
      {
        CHECK(reconstructed == DXGI_FORMAT_D24_UNORM_S8_UINT);
      }
      else
      {
        CHECK(reconstructed == original);
      }
    }
  };

  SECTION("MakeResourceFormat concurs with helpers")
  {
    for(DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; f <= maxFormat; f = DXGI_FORMAT(f + 1))
    {
      if(isUnsupportedFormat(f))
        continue;

      ResourceFormat fmt = MakeResourceFormat(f);

      INFO("Format is " << ToStr(f));

      if(IsBlockFormat(f))
      {
        CHECK(fmt.type >= ResourceFormatType::BC1);
        CHECK(fmt.type <= ResourceFormatType::BC7);
      }

      if(IsDepthAndStencilFormat(f))
      {
        // manually check these
        switch(f)
        {
          case DXGI_FORMAT_R32G8X24_TYPELESS: CHECK(fmt.compType == CompType::Typeless); break;
          case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: CHECK(fmt.compType == CompType::Depth); break;
          case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: CHECK(fmt.compType == CompType::Depth); break;
          case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: CHECK(fmt.compType == CompType::Depth); break;

          case DXGI_FORMAT_D24_UNORM_S8_UINT: CHECK(fmt.compType == CompType::Depth); break;
          case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: CHECK(fmt.compType == CompType::Depth); break;
          case DXGI_FORMAT_X24_TYPELESS_G8_UINT: CHECK(fmt.compType == CompType::Depth); break;
          case DXGI_FORMAT_R24G8_TYPELESS: CHECK(fmt.compType == CompType::Typeless); break;
          default: break;
        }
      }
      else if(IsTypelessFormat(f))
      {
        CHECK(fmt.compType == CompType::Typeless);
      }
      else if(IsDepthFormat(f))
      {
        CHECK(fmt.compType == CompType::Depth);
      }
      else if(IsUIntFormat(f))
      {
        CHECK(fmt.compType == CompType::UInt);
      }
      else if(IsIntFormat(f))
      {
        CHECK(fmt.compType == CompType::SInt);
      }

      if(IsSRGBFormat(f))
      {
        CHECK(fmt.SRGBCorrected());
      }
    }
  };

  SECTION("Get*Format helpers match MakeResourceFormat")
  {
    for(DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; f <= maxFormat; f = DXGI_FORMAT(f + 1))
    {
      if(isUnsupportedFormat(f))
        continue;

      ResourceFormat fmt = MakeResourceFormat(f);

      INFO("Format is " << ToStr(f));

      if(IsSRGBFormat(f))
      {
        DXGI_FORMAT conv = GetNonSRGBFormat(f);
        INFO(ToStr(conv));

        ResourceFormat convfmt = MakeResourceFormat(conv);

        CHECK(!convfmt.SRGBCorrected());
      }

      if(fmt.type == ResourceFormatType::BC1 || fmt.type == ResourceFormatType::BC2 ||
         fmt.type == ResourceFormatType::BC3 || fmt.type == ResourceFormatType::BC7 ||

         (fmt.type == ResourceFormatType::Regular && fmt.compByteWidth == 1 && fmt.compCount == 4 &&
          fmt.compType != CompType::UInt && fmt.compType != CompType::SInt &&
          fmt.compType != CompType::SNorm))
      {
        DXGI_FORMAT conv = GetSRGBFormat(f);
        INFO(ToStr(conv));

        ResourceFormat convfmt = MakeResourceFormat(conv);

        CHECK(convfmt.SRGBCorrected());
      }

      if(f == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)
      {
        // this format has special handling, so we skip it from the below Typeless/Typed check

        CompType typeHint = fmt.compType;

        DXGI_FORMAT typeless = GetTypelessFormat(f);
        DXGI_FORMAT typed = GetTypedFormat(typeless, typeHint);

        CHECK(typed == DXGI_FORMAT_R10G10B10A2_UNORM);

        continue;
      }

      if(!IsTypelessFormat(f))
      {
        CompType typeHint = fmt.compType;

        DXGI_FORMAT original = f;
        DXGI_FORMAT typeless = GetTypelessFormat(f);
        DXGI_FORMAT typed = GetTypedFormat(typeless, typeHint);

        if(fmt.SRGBCorrected())
          typed = GetSRGBFormat(typed);

        CHECK(original == typed);
      }
    }
  };

  SECTION("GetByteSize returns expected values for regular formats")
  {
    for(DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; f <= maxFormat; f = DXGI_FORMAT(f + 1))
    {
      if(isUnsupportedFormat(f))
        continue;

      ResourceFormat fmt = MakeResourceFormat(f);

      if(fmt.type != ResourceFormatType::Regular)
        continue;

      INFO("Format is " << ToStr(f));

      uint32_t size = fmt.compCount * fmt.compByteWidth * 123 * 456;

      CHECK(size == GetByteSize(123, 456, 1, f, 0));
    }
  };

  SECTION("GetByteSize for BCn formats")
  {
    const uint32_t width = 24, height = 24;

    // reference: 24x24 = 576, 576/2 = 288

    const uint32_t bcnsizes[] = {
        288,    // DXGI_FORMAT_BC1_TYPELESS
        288,    // DXGI_FORMAT_BC1_UNORM
        288,    // DXGI_FORMAT_BC1_UNORM_SRGB = 0.5 byte/px
        576,    // DXGI_FORMAT_BC2_TYPELESS
        576,    // DXGI_FORMAT_BC2_UNORM
        576,    // DXGI_FORMAT_BC2_UNORM_SRGB = 1 byte/px
        576,    // DXGI_FORMAT_BC3_TYPELESS
        576,    // DXGI_FORMAT_BC3_UNORM
        576,    // DXGI_FORMAT_BC3_UNORM_SRGB = 1 byte/px
        288,    // DXGI_FORMAT_BC4_TYPELESS
        288,    // DXGI_FORMAT_BC4_UNORM
        288,    // DXGI_FORMAT_BC4_SNORM = 0.5 byte/px
        576,    // DXGI_FORMAT_BC5_TYPELESS
        576,    // DXGI_FORMAT_BC5_UNORM
        576,    // DXGI_FORMAT_BC5_SNORM = 1 byte/px
        576,    // DXGI_FORMAT_BC6H_TYPELESS
        576,    // DXGI_FORMAT_BC6H_UF16
        576,    // DXGI_FORMAT_BC6H_SF16 = 1 byte/px
        576,    // DXGI_FORMAT_BC7_TYPELESS
        576,    // DXGI_FORMAT_BC7_UNORM
        576,    // DXGI_FORMAT_BC7_UNORM_SRGB = 1 byte/px
    };

    int i = 0;
    for(DXGI_FORMAT f : {
            DXGI_FORMAT_BC1_TYPELESS,  DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM_SRGB,
            DXGI_FORMAT_BC2_TYPELESS,  DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM_SRGB,
            DXGI_FORMAT_BC3_TYPELESS,  DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM_SRGB,
            DXGI_FORMAT_BC4_TYPELESS,  DXGI_FORMAT_BC4_UNORM, DXGI_FORMAT_BC4_SNORM,
            DXGI_FORMAT_BC5_TYPELESS,  DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC5_SNORM,
            DXGI_FORMAT_BC6H_TYPELESS, DXGI_FORMAT_BC6H_UF16, DXGI_FORMAT_BC6H_SF16,
            DXGI_FORMAT_BC7_TYPELESS,  DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_UNORM_SRGB,
        })
    {
      if(isUnsupportedFormat(f))
        continue;

      INFO("Format is " << ToStr(f));

      CHECK(bcnsizes[i++] == GetByteSize(width, height, 1, f, 0));
    }
  };

  SECTION("GetByteSize for YUV formats")
  {
    const uint32_t width = 24, height = 24;

    // reference: 24x24 = 576

    const uint32_t yuvsizes[] = {
        2304,    // DXGI_FORMAT_AYUV (4:4:4 8-bit packed)
        2304,    // DXGI_FORMAT_Y410 (4:4:4 10-bit packed)
        4608,    // DXGI_FORMAT_Y416 (4:4:4 16-bit packed)
        864,     // DXGI_FORMAT_NV12 (4:2:0 8-bit planar)
        1728,    // DXGI_FORMAT_P010 (4:2:0 10-bit planar)
        1728,    // DXGI_FORMAT_P016 (4:2:0 16-bit planar)
        1152,    // DXGI_FORMAT_YUY2 (4:2:2 8-bit packed)
        2304,    // DXGI_FORMAT_Y210 (4:2:2 10-bit packed)
        2304,    // DXGI_FORMAT_Y216 (4:2:2 16-bit packed)
        1152,    // DXGI_FORMAT_P208 (4:2:2 8-bit planar)
    };

    int i = 0;
    for(DXGI_FORMAT f : {
            DXGI_FORMAT_AYUV, DXGI_FORMAT_Y410, DXGI_FORMAT_Y416, DXGI_FORMAT_NV12, DXGI_FORMAT_P010,
            DXGI_FORMAT_P016, DXGI_FORMAT_YUY2, DXGI_FORMAT_Y210, DXGI_FORMAT_Y216, DXGI_FORMAT_P208,
        })
    {
      if(isUnsupportedFormat(f))
        continue;

      INFO("Format is " << ToStr(f));

      CHECK(yuvsizes[i++] == GetByteSize(width, height, 1, f, 0));
    }
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
