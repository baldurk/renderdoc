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

#include "gl_resources.h"
#include "gl_dispatch_table.h"
#include "gl_manager.h"

template <>
rdcstr DoStringise(const GLNamespace &el)
{
  BEGIN_ENUM_STRINGISE(GLNamespace)
  {
    STRINGISE_ENUM_NAMED(eResUnknown, "Unknown");
    STRINGISE_ENUM_NAMED(eResSpecial, "Special Resource");
    STRINGISE_ENUM_NAMED(eResTexture, "Texture");
    STRINGISE_ENUM_NAMED(eResSampler, "Sampler");
    STRINGISE_ENUM_NAMED(eResFramebuffer, "Framebuffer");
    STRINGISE_ENUM_NAMED(eResRenderbuffer, "Renderbuffer");
    STRINGISE_ENUM_NAMED(eResBuffer, "Buffer");
    STRINGISE_ENUM_NAMED(eResVertexArray, "Vertex Array");
    STRINGISE_ENUM_NAMED(eResShader, "Shader");
    STRINGISE_ENUM_NAMED(eResProgram, "Program");
    STRINGISE_ENUM_NAMED(eResProgramPipe, "Program Pipeline");
    STRINGISE_ENUM_NAMED(eResFeedback, "Transform Feedback");
    STRINGISE_ENUM_NAMED(eResQuery, "Query");
    STRINGISE_ENUM_NAMED(eResSync, "Sync");
    STRINGISE_ENUM_NAMED(eResExternalMemory, "External Memory");
    STRINGISE_ENUM_NAMED(eResExternalSemaphore, "External Semaphore");
  }
  END_ENUM_STRINGISE();
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLResource &el)
{
  GLResourceManager *rm = (GLResourceManager *)ser.GetUserData();

  ResourceId id;

  if(ser.IsWriting() && rm)
    id = rm->GetResID(el);

  DoSerialise(ser, id);

  if(ser.IsReading())
  {
    if(id != ResourceId() && rm && rm->HasLiveResource(id))
      el = rm->GetLiveResource(id);
    else
      el = GLResource(MakeNullResource);
  }
}

INSTANTIATE_SERIALISE_TYPE(GLResource);

byte GLResourceRecord::markerValue[32] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0x88, 0x77, 0x66, 0x55, 0x01, 0x23, 0x45, 0x67, 0x98, 0x76, 0x54, 0x32,
};

size_t GetCompressedByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum internalformat)
{
  if(!IsCompressedFormat(internalformat))
  {
    RDCERR("Not compressed format %s", ToStr(internalformat).c_str());
    return GetByteSize(w, h, d, GetBaseFormat(internalformat), GetDataType(internalformat));
  }

  uint32_t astc[3] = {0, 0, 1u};

  size_t numBlockAlignedTexels = size_t(AlignUp4(w)) * size_t(AlignUp4(h)) * size_t(d);

  switch(internalformat)
  {
    // BC1
    case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: return numBlockAlignedTexels / 2;
    // BC2
    case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: return numBlockAlignedTexels;
    // BC3
    case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return numBlockAlignedTexels;
    // BC4
    case eGL_COMPRESSED_RED_RGTC1:
    case eGL_COMPRESSED_SIGNED_RED_RGTC1: return numBlockAlignedTexels / 2;
    // BC5
    case eGL_COMPRESSED_RG_RGTC2:
    case eGL_COMPRESSED_SIGNED_RG_RGTC2: return numBlockAlignedTexels;
    // BC6
    case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB: return numBlockAlignedTexels;
    // BC7
    case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB: return numBlockAlignedTexels;
    // ETC1
    case eGL_ETC1_RGB8_OES:
    // ETC2
    case eGL_COMPRESSED_RGB8_ETC2:
    case eGL_COMPRESSED_SRGB8_ETC2:
    case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: return numBlockAlignedTexels / 2;
    // EAC
    case eGL_COMPRESSED_RGBA8_ETC2_EAC:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: return numBlockAlignedTexels;
    case eGL_COMPRESSED_R11_EAC:
    case eGL_COMPRESSED_SIGNED_R11_EAC: return numBlockAlignedTexels / 2;
    case eGL_COMPRESSED_RG11_EAC:
    case eGL_COMPRESSED_SIGNED_RG11_EAC: return numBlockAlignedTexels;
    // ASTC
    case eGL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
      astc[0] = 4;
      astc[1] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
      astc[0] = 5;
      astc[1] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
      astc[0] = 5;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
      astc[0] = 6;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
      astc[0] = 6;
      astc[1] = 6;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
      astc[0] = 8;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
      astc[0] = 8;
      astc[1] = 6;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
      astc[0] = 8;
      astc[1] = 8;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
      astc[0] = 10;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
      astc[0] = 10;
      astc[1] = 6;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
      astc[0] = 10;
      astc[1] = 8;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
      astc[0] = 10;
      astc[1] = 10;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
      astc[0] = 12;
      astc[1] = 10;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
      astc[0] = 12;
      astc[1] = 12;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_3x3x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES:
      astc[0] = 3;
      astc[1] = 3;
      astc[2] = 3;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_4x3x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES:
      astc[0] = 4;
      astc[1] = 3;
      astc[2] = 3;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_4x4x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES:
      astc[0] = 4;
      astc[1] = 4;
      astc[2] = 3;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_4x4x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES:
      astc[0] = 4;
      astc[1] = 4;
      astc[2] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x4x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES:
      astc[0] = 5;
      astc[1] = 4;
      astc[2] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x5x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES:
      astc[0] = 5;
      astc[1] = 5;
      astc[2] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x5x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES:
      astc[0] = 5;
      astc[1] = 5;
      astc[2] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x5x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES:
      astc[0] = 6;
      astc[1] = 5;
      astc[2] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x6x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES:
      astc[0] = 6;
      astc[1] = 6;
      astc[2] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x6x6_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES:
      astc[0] = 6;
      astc[1] = 6;
      astc[2] = 6;
      break;
    // PVRTC
    case eGL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
      // 4x8 block in 8 bytes = 32 pixels in 8 bytes = 0.25 bytes per pixel
      return (size_t(AlignUp(w, 4)) * size_t(AlignUp(h, 8)) * size_t(d)) / 4;
    case eGL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT:
      // 4x4 block in 8 bytes = 16 pixels in 8 bytes = 0.5 bytes per pixel
      return numBlockAlignedTexels / 2;
    default: break;
  }

  if(astc[0] > 0 && astc[1] > 0 && astc[2] > 0)
  {
    size_t blocks[3] = {(w / astc[0]), (h / astc[1]), (d / astc[2])};

    // how many blocks are needed - including any extra partial blocks
    blocks[0] += (w % astc[0]) ? 1 : 0;
    blocks[1] += (h % astc[1]) ? 1 : 0;
    blocks[2] += (d % astc[2]) ? 1 : 0;

    // ASTC blocks are all 128 bits each
    return blocks[0] * blocks[1] * blocks[2] * 16;
  }

  RDCERR("Unrecognised compressed format %s", ToStr(internalformat).c_str());
  return GetByteSize(w, h, d, GetBaseFormat(internalformat), GetDataType(internalformat));
}

rdcfixedarray<uint32_t, 3> GetCompressedBlockSize(GLenum internalformat)
{
  if(!IsCompressedFormat(internalformat))
  {
    RDCERR("Not compressed format %s", ToStr(internalformat).c_str());
    return {1u, 1u, 1u};
  }

  uint32_t astc[3] = {0, 0, 1u};

  switch(internalformat)
  {
    // BC1
    case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    // BC2
    case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    // BC3
    case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
    // BC4
    case eGL_COMPRESSED_RED_RGTC1:
    case eGL_COMPRESSED_SIGNED_RED_RGTC1:
    // BC5
    case eGL_COMPRESSED_RG_RGTC2:
    case eGL_COMPRESSED_SIGNED_RG_RGTC2:
    // BC6
    case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
    // BC7
    case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
    // ETC1
    case eGL_ETC1_RGB8_OES:
    // ETC2
    case eGL_COMPRESSED_RGB8_ETC2:
    case eGL_COMPRESSED_SRGB8_ETC2:
    case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    // EAC
    case eGL_COMPRESSED_RGBA8_ETC2_EAC:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    case eGL_COMPRESSED_R11_EAC:
    case eGL_COMPRESSED_SIGNED_R11_EAC:
    case eGL_COMPRESSED_RG11_EAC:
    case eGL_COMPRESSED_SIGNED_RG11_EAC: return {4u, 4u, 1u};
    // ASTC
    case eGL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
      astc[0] = 4;
      astc[1] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
      astc[0] = 5;
      astc[1] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
      astc[0] = 5;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
      astc[0] = 6;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
      astc[0] = 6;
      astc[1] = 6;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
      astc[0] = 8;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
      astc[0] = 8;
      astc[1] = 6;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
      astc[0] = 8;
      astc[1] = 8;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
      astc[0] = 10;
      astc[1] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
      astc[0] = 10;
      astc[1] = 6;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
      astc[0] = 10;
      astc[1] = 8;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
      astc[0] = 10;
      astc[1] = 10;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
      astc[0] = 12;
      astc[1] = 10;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
      astc[0] = 12;
      astc[1] = 12;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_3x3x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES:
      astc[0] = 3;
      astc[1] = 3;
      astc[2] = 3;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_4x3x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES:
      astc[0] = 4;
      astc[1] = 3;
      astc[2] = 3;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_4x4x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES:
      astc[0] = 4;
      astc[1] = 4;
      astc[2] = 3;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_4x4x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES:
      astc[0] = 4;
      astc[1] = 4;
      astc[2] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x4x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES:
      astc[0] = 5;
      astc[1] = 4;
      astc[2] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x5x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES:
      astc[0] = 5;
      astc[1] = 5;
      astc[2] = 4;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_5x5x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES:
      astc[0] = 5;
      astc[1] = 5;
      astc[2] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x5x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES:
      astc[0] = 6;
      astc[1] = 5;
      astc[2] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x6x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES:
      astc[0] = 6;
      astc[1] = 6;
      astc[2] = 5;
      break;
    case eGL_COMPRESSED_RGBA_ASTC_6x6x6_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES:
      astc[0] = 6;
      astc[1] = 6;
      astc[2] = 6;
      break;
    // PVRTC
    case eGL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT: return {4u, 4u, 1u};
    default: break;
  }

  if(astc[0] > 0 && astc[1] > 0 && astc[2] > 0)
  {
    return {astc[0], astc[1], astc[2]};
  }

  RDCERR("Unrecognised compressed format %s", ToStr(internalformat).c_str());
  return {1u, 1u, 1u};
}

size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type)
{
  size_t elemSize = 1;

  size_t numTexels = size_t(w) * size_t(h) * size_t(d);

  switch(type)
  {
    case eGL_UNSIGNED_BYTE:
    case eGL_BYTE: elemSize = 1; break;
    case eGL_UNSIGNED_SHORT:
    case eGL_SHORT:
    case eGL_HALF_FLOAT_OES:
    case eGL_HALF_FLOAT: elemSize = 2; break;
    case eGL_UNSIGNED_INT:
    case eGL_INT:
    case eGL_FLOAT: elemSize = 4; break;
    case eGL_DOUBLE: elemSize = 8; break;
    case eGL_UNSIGNED_BYTE_3_3_2:
    case eGL_UNSIGNED_BYTE_2_3_3_REV: return numTexels;
    case eGL_UNSIGNED_SHORT_5_6_5:
    case eGL_UNSIGNED_SHORT_5_6_5_REV:
    case eGL_UNSIGNED_SHORT_4_4_4_4:
    case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
    case eGL_UNSIGNED_SHORT_5_5_5_1:
    case eGL_UNSIGNED_SHORT_1_5_5_5_REV: return numTexels * 2;
    case eGL_UNSIGNED_INT_8_8_8_8:
    case eGL_UNSIGNED_INT_8_8_8_8_REV:
    case eGL_UNSIGNED_INT_10_10_10_2:
    case eGL_UNSIGNED_INT_2_10_10_10_REV:
    case eGL_INT_2_10_10_10_REV:
    case eGL_UNSIGNED_INT_10F_11F_11F_REV:
    case eGL_UNSIGNED_INT_5_9_9_9_REV: return numTexels * 4;
    case eGL_DEPTH_COMPONENT16: return numTexels * 2;
    case eGL_DEPTH_COMPONENT24:
    case eGL_DEPTH24_STENCIL8:
    case eGL_DEPTH_COMPONENT32:
    case eGL_DEPTH_COMPONENT32F:
    case eGL_UNSIGNED_INT_24_8: return numTexels * 4;
    case eGL_DEPTH32F_STENCIL8:
    case eGL_FLOAT_32_UNSIGNED_INT_24_8_REV: return numTexels * 8;
    default: RDCERR("Unhandled Byte Size type %s!", ToStr(type).c_str()); break;
  }

  switch(format)
  {
    case eGL_RED:
    case eGL_RED_INTEGER:
    case eGL_GREEN:
    case eGL_GREEN_INTEGER:
    case eGL_BLUE:
    case eGL_BLUE_INTEGER:
    case eGL_LUMINANCE:
    case eGL_ALPHA:
    case eGL_ALPHA_INTEGER:
    case eGL_DEPTH_COMPONENT:
    case eGL_STENCIL_INDEX:
    case eGL_STENCIL: return numTexels * elemSize;
    case eGL_RG:
    case eGL_RG_INTEGER:
    case eGL_LUMINANCE_ALPHA:
    case eGL_DEPTH_STENCIL: return numTexels * elemSize * 2;
    case eGL_RGB:
    case eGL_RGB_INTEGER:
    case eGL_BGR:
    case eGL_BGR_INTEGER:
    case eGL_SRGB: return numTexels * elemSize * 3;
    case eGL_RGBA:
    case eGL_RGBA_INTEGER:
    case eGL_BGRA:
    case eGL_BGRA_INTEGER:
    case eGL_SRGB_ALPHA: return numTexels * elemSize * 4;
    default: RDCERR("Unhandled Byte Size format %s!", ToStr(format).c_str()); break;
  }

  RDCERR("Unhandled Byte Size case!");

  return 1;
}

GLenum GetBaseFormat(GLenum internalFormat)
{
  switch((int)internalFormat)
  {
    case eGL_R8:
    case eGL_R8_SNORM:
    case eGL_SR8_EXT:
    case eGL_R16:
    case eGL_R16_SNORM:
    case eGL_R16F:
    case eGL_R32F:
    case eGL_RED: return eGL_RED;
    case eGL_ALPHA:
    case eGL_ALPHA8_EXT: return eGL_ALPHA;
    case eGL_LUMINANCE: return eGL_LUMINANCE;
    case eGL_LUMINANCE_ALPHA: return eGL_LUMINANCE_ALPHA;
    case eGL_INTENSITY_EXT: return eGL_INTENSITY_EXT;
    case eGL_R8I:
    case eGL_R16I:
    case eGL_R32I:
    case eGL_R32UI:
    case eGL_R16UI:
    case eGL_R8UI:
    case eGL_RED_INTEGER: return eGL_RED_INTEGER;
    case eGL_RG8:
    case eGL_RG8_SNORM:
    case eGL_SRG8_EXT:
    case eGL_RG16:
    case eGL_RG16_SNORM:
    case eGL_RG16F:
    case eGL_RG32F:
    case eGL_RG: return eGL_RG;
    case eGL_RG8I:
    case eGL_RG8UI:
    case eGL_RG16I:
    case eGL_RG16UI:
    case eGL_RG32I:
    case eGL_RG32UI:
    case eGL_RG_INTEGER: return eGL_RG_INTEGER;
    case eGL_R3_G3_B2:
    case eGL_RGB4:
    case eGL_RGB5:
    case eGL_RGB565:
    case eGL_RGB8:
    case eGL_RGB8_SNORM:
    case eGL_RGB10:
    case eGL_RGB12:
    case eGL_RGB16:
    case eGL_RGB16_SNORM:
    case eGL_SRGB:
    case eGL_SRGB8:
    case eGL_RGB16F:
    case eGL_RGB32F:
    case eGL_R11F_G11F_B10F:
    case eGL_RGB9_E5:
    case eGL_RGB: return eGL_RGB;
    case eGL_RGB8I:
    case eGL_RGB8UI:
    case eGL_RGB16I:
    case eGL_RGB16UI:
    case eGL_RGB32I:
    case eGL_RGB32UI:
    case eGL_RGB_INTEGER: return eGL_RGB_INTEGER;
    case eGL_RGBA2:
    case eGL_RGBA4:
    case eGL_RGB5_A1:
    case eGL_RGBA8:
    case eGL_RGBA8_SNORM:
    case eGL_RGB10_A2:
    case eGL_RGBA12:
    case eGL_RGBA16:
    case eGL_RGBA16_SNORM:
    case eGL_SRGB_ALPHA:
    case eGL_SRGB8_ALPHA8:
    case eGL_RGBA16F:
    case eGL_RGBA32F:
    case eGL_RGBA: return eGL_RGBA;
    case eGL_RGB10_A2UI:
    case eGL_RGBA8I:
    case eGL_RGBA8UI:
    case eGL_RGBA16I:
    case eGL_RGBA16UI:
    case eGL_RGBA32UI:
    case eGL_RGBA32I:
    case eGL_RGBA_INTEGER: return eGL_RGBA_INTEGER;
    case eGL_BGRA8_EXT:
    case eGL_BGRA: return eGL_BGRA;
    case eGL_DEPTH_COMPONENT16:
    case eGL_DEPTH_COMPONENT24:
    case eGL_DEPTH_COMPONENT32:
    case eGL_DEPTH_COMPONENT32F:
    case eGL_DEPTH_COMPONENT: return eGL_DEPTH_COMPONENT;
    case eGL_DEPTH24_STENCIL8:
    case eGL_DEPTH32F_STENCIL8:
    case eGL_DEPTH_STENCIL: return eGL_DEPTH_STENCIL;
    case eGL_STENCIL_INDEX1:
    case eGL_STENCIL_INDEX4:
    case eGL_STENCIL_INDEX8:
    case eGL_STENCIL_INDEX16:
    case eGL_STENCIL: return eGL_STENCIL_INDEX;
    default: break;
  }

  RDCERR("Unhandled Base Format case %s!", ToStr(internalFormat).c_str());

  return eGL_NONE;
}

GLenum GetDataType(GLenum internalFormat)
{
  switch((int)internalFormat)
  {
    case eGL_RGBA8UI:
    case eGL_RG8UI:
    case eGL_R8UI:
    case eGL_RGBA8:
    case eGL_RG8:
    case eGL_R8:
    case eGL_RGB8:
    case eGL_RGB8UI:
    case eGL_BGRA:
    case eGL_BGRA8_EXT:
    case eGL_SRGB8_ALPHA8:
    case eGL_SRGB8:
    case eGL_SRG8_EXT:
    case eGL_SR8_EXT:
    case eGL_SRGB_ALPHA:
    case eGL_SRGB:
    case eGL_RED:
    case eGL_RG:
    case eGL_RGB:
    case eGL_RGBA: return eGL_UNSIGNED_BYTE;
    case eGL_RGBA8I:
    case eGL_RG8I:
    case eGL_R8I:
    case eGL_RGBA8_SNORM:
    case eGL_RG8_SNORM:
    case eGL_R8_SNORM:
    case eGL_RGB8_SNORM:
    case eGL_RGB8I: return eGL_BYTE;
    case eGL_RGBA16UI:
    case eGL_RG16UI:
    case eGL_R16UI:
    case eGL_RGBA16:
    case eGL_RG16:
    case eGL_R16:
    case eGL_RGB16:
    case eGL_RGB16UI:
    case eGL_DEPTH_COMPONENT16: return eGL_UNSIGNED_SHORT;
    case eGL_RGBA16I:
    case eGL_RG16I:
    case eGL_R16I:
    case eGL_RGBA16_SNORM:
    case eGL_RG16_SNORM:
    case eGL_R16_SNORM:
    case eGL_RGB16_SNORM:
    case eGL_RGB16I: return eGL_SHORT;
    case eGL_RGBA32UI:
    case eGL_RG32UI:
    case eGL_R32UI:
    case eGL_RGB32UI:
    case eGL_DEPTH_COMPONENT:
    case eGL_DEPTH_COMPONENT24:
    case eGL_DEPTH_COMPONENT32: return eGL_UNSIGNED_INT;
    case eGL_RGBA32I:
    case eGL_RG32I:
    case eGL_R32I:
    case eGL_RGB32I: return eGL_INT;
    case eGL_RGBA16F:
    case eGL_RG16F:
    case eGL_RGB16F:
    case eGL_R16F: return eGL_HALF_FLOAT;
    case eGL_RGBA32F:
    case eGL_RGB32F:
    case eGL_RG32F:
    case eGL_R32F:
    case eGL_DEPTH_COMPONENT32F: return eGL_FLOAT;
    case eGL_R11F_G11F_B10F: return eGL_UNSIGNED_INT_10F_11F_11F_REV;
    case eGL_RGB10_A2UI: return eGL_UNSIGNED_INT_2_10_10_10_REV;
    case eGL_RGB10_A2: return eGL_UNSIGNED_INT_2_10_10_10_REV;
    case eGL_R3_G3_B2: return eGL_UNSIGNED_BYTE_3_3_2;
    case eGL_RGB4:
    case eGL_RGBA4: return eGL_UNSIGNED_SHORT_4_4_4_4;
    case eGL_RGBA2: return eGL_UNSIGNED_BYTE;
    case eGL_RGB5_A1: return eGL_UNSIGNED_SHORT_5_5_5_1;
    case eGL_RGB565:
    case eGL_RGB5: return eGL_UNSIGNED_SHORT_5_6_5;
    case eGL_RGB10: return eGL_UNSIGNED_INT_10_10_10_2;
    case eGL_RGB9_E5: return eGL_UNSIGNED_INT_5_9_9_9_REV;
    case eGL_DEPTH24_STENCIL8: return eGL_UNSIGNED_INT_24_8;
    case eGL_DEPTH_STENCIL:
    case eGL_DEPTH32F_STENCIL8: return eGL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    case eGL_STENCIL_INDEX:
    case eGL_STENCIL_INDEX8: return eGL_UNSIGNED_BYTE;
    case eGL_ALPHA:
    case eGL_ALPHA8_EXT:
    case eGL_LUMINANCE_ALPHA:
    case eGL_LUMINANCE:
    case eGL_INTENSITY_EXT: return eGL_UNSIGNED_BYTE;
    default: break;
  }

  RDCERR("Unhandled Data Type case %s!", ToStr(internalFormat).c_str());

  return eGL_NONE;
}

int GetNumMips(GLenum target, GLuint tex, GLuint w, GLuint h, GLuint d)
{
  int mips = 1;

  // renderbuffers don't have mips
  if(target == eGL_RENDERBUFFER)
    return 1;

  GLint immut = 0;
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_IMMUTABLE_FORMAT, &immut);

  if(immut)
    GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_IMMUTABLE_LEVELS, (GLint *)&mips);
  else
    mips = CalcNumMips(w, h, d);

  GLint maxLevel = 1000;
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MAX_LEVEL, &maxLevel);
  mips = RDCMIN(mips, maxLevel + 1);

  if(immut == 0)
  {
    // check to see if all mips are set, or clip the number of mips to those that are
    // set.
    if(target == eGL_TEXTURE_CUBE_MAP)
      target = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

    for(int i = 0; i < mips; i++)
    {
      GLint width = 0;
      GL.glGetTextureLevelParameterivEXT(tex, target, i, eGL_TEXTURE_WIDTH, &width);
      if(width == 0)
      {
        mips = i;
        break;
      }
    }
  }

  return RDCMAX(1, mips);
}

void GetFramebufferMipAndLayer(GLuint framebuffer, GLenum attachment, GLint *mip, GLint *layer)
{
  GL.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment,
                                                   eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, mip);

  GLenum face = eGL_NONE;
  GL.glGetNamedFramebufferAttachmentParameterivEXT(
      framebuffer, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

  if(face == 0)
  {
    GL.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, layer);
  }
  else
  {
    *layer = CubeTargetIndex(face);
  }
}

// GL_TEXTURE_SWIZZLE_RGBA is not supported on GLES, so for consistency we use r/g/b/a component
// swizzles for both GL and GLES.
// The same applies to SetTextureSwizzle function.
void GetTextureSwizzle(GLuint tex, GLenum target, GLenum *swizzleRGBA)
{
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_R, (GLint *)&swizzleRGBA[0]);
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_G, (GLint *)&swizzleRGBA[1]);
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_B, (GLint *)&swizzleRGBA[2]);
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_A, (GLint *)&swizzleRGBA[3]);
}

void SetTextureSwizzle(GLuint tex, GLenum target, const GLenum *swizzleRGBA)
{
  GL.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_R, (GLint *)&swizzleRGBA[0]);
  GL.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_G, (GLint *)&swizzleRGBA[1]);
  GL.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_B, (GLint *)&swizzleRGBA[2]);
  GL.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_A, (GLint *)&swizzleRGBA[3]);
}

static rdcstr DimensionString(int dimensions, GLint width, GLint height, GLint depth)
{
  if(dimensions == 1)
    return StringFormat::Fmt("%d", width);
  else if(dimensions == 2)
    return StringFormat::Fmt("%dx%d", width, height);
  else
    return StringFormat::Fmt("%dx%dx%d", width, depth);
}

rdcstr GetTextureCompleteStatus(GLenum target, GLuint tex, GLuint sampler)
{
  // unbound and texture buffers don't need to be checked
  if(tex == 0 || target == eGL_TEXTURE_BUFFER)
    return rdcstr();

  // the completeness rules are fairly complex. The relevant spec is copied here and each rule is
  // annotated with a number for easier reference.
  /*
    For one-, two-, and three-dimensional and one- and two-dimensional array textures, a texture is
    mipmap complete if all of the following conditions hold true:

    * The set of mipmap images levelBase through q (where q is defined in section 8.14.3) were each
      specified with the same internal format. [RULE_1]
    * The dimensions of the images follow the sequence described in section 8.14.3. [RULE_2]
    * level base <= level max [RULE_3]

    [q is the usual definition - natural mip numbering, clamped by either immutable number of mips
    or MAX_LEVEL]

    Image levels k where k < level base or k > q are insignificant to the definition of
    completeness.

    A cube map texture is mipmap complete if each of the six texture images, considered
    individually, is mipmap complete. [RULE_4]

    Additionally, a cube map texture is cube complete if the following conditions all hold true:

    * The level base texture images of each of the six cubemap faces have identical, positive, and
      square dimensions. [RULE_5]
    * The level base images were each specified with the same internal format. [RULE_6]

    A cube map array texture is cube array complete if it is complete when treated as a
    two-dimensional array [RULE_7] and cube complete for every cube map slice within the array
    texture. [RULE_8]

    Using the preceding definitions, a texture is complete unless any of the following conditions
    hold true:

    * Any dimension of the level base image is not positive. For a rectangle or multisample texture,
      level base is always zero. [RULE_9]
    * The texture is a cube map texture, and is not cube complete. [RULE_10]
    * The texture is a cube map array texture, and is not cube array complete. [RULE_11]
    * The minification filter requires a mipmap (is neither NEAREST nor LINEAR), and the texture is
      not mipmap complete. [RULE_12]
    * Any of
        - The internal format of the texture is integer (see table 8.12). [RULE_13]
        - The internal format is STENCIL_INDEX. [RULE_14]
        - The internal format is DEPTH_STENCIL, and the value of DEPTH_STENCIL_TEXTURE_MODE for the
          texture is STENCIL_INDEX. [RULE_15]
      and either the magnification filter is not NEAREST, or the minification filter is neither
      NEAREST nor NEAREST_MIPMAP_NEAREST
  */

  GLint isImmutable = 0, levelBase = 0, levelMax = 1000;
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_IMMUTABLE_FORMAT, &isImmutable);
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_BASE_LEVEL, &levelBase);
  GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MAX_LEVEL, &levelMax);

  bool noMips = false;

  // For a rectangle or multisample texture, level base is always zero.
  if(target == eGL_TEXTURE_RECTANGLE || target == eGL_TEXTURE_2D_MULTISAMPLE ||
     target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
  {
    levelBase = 0;
    noMips = true;
  }

  GLenum targets[] = {
      eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };

  int faceCount = ARRAY_COUNT(targets);

  if(target != eGL_TEXTURE_CUBE_MAP)
  {
    targets[0] = target;
    faceCount = 1;
  }

  // get the properties of levelBase (on POSITIVE_X for cubes)
  GLint levelBaseWidth = 1, levelBaseHeight = 1, levelBaseDepth = 1;

  int dimensions = 2;

  if(target == eGL_TEXTURE_1D || target == eGL_TEXTURE_1D_ARRAY)
  {
    dimensions = 1;
  }
  else if(target == eGL_TEXTURE_3D)
  {
    dimensions = 3;
  }

  GL.glGetTextureLevelParameterivEXT(tex, targets[0], levelBase, eGL_TEXTURE_WIDTH, &levelBaseWidth);
  if(dimensions >= 2)
    GL.glGetTextureLevelParameterivEXT(tex, targets[0], levelBase, eGL_TEXTURE_HEIGHT,
                                       &levelBaseHeight);
  if(dimensions >= 3)
    GL.glGetTextureLevelParameterivEXT(tex, targets[0], levelBase, eGL_TEXTURE_DEPTH,
                                       &levelBaseDepth);

  bool mipmapComplete = true;
  rdcstr mipmapIncompleteness;

  bool cube = (target == eGL_TEXTURE_CUBE_MAP || target == eGL_TEXTURE_CUBE_MAP_ARRAY);

  GLenum levelBaseFormat = eGL_NONE;

  // immutable textures are always mipmap complete
  if(isImmutable)
  {
    mipmapComplete = true;

    // get the format so we can check for integer-ness etc
    GL.glGetTextureLevelParameterivEXT(tex, targets[0], levelBase, eGL_TEXTURE_INTERNAL_FORMAT,
                                       (GLint *)&levelBaseFormat);
  }
  else
  {
    GLint p = 0, q = 0;

    // Otherwise p = floor(log2(maxsize)) + levelBase, and all arrays from levelBase through
    // q = min(p, levelMax) must be defined, as discussed in section 8.17.
    p = CalcNumMips(levelBaseWidth, levelBaseHeight, levelBaseDepth) - 1 + levelBase;
    q = RDCMIN(p, levelMax);

    // this isn't part of the spec, but ensure we process at least levelBase, even if levelMax is
    // less. That's because levelBase <= levelMax is only a mipmap completeness requirement
    // (otherwise if mips aren't used, levelMax is effectively ignored), but we want to check
    // [RULE_9] that levelBase has valid dimensions in this loop since we need to check it per-face
    // for cubemaps
    q = RDCMAX(levelBase, q);

    // [RULE_4] - this just requires the loop over faces, completely independently
    // [RULE_7] [RULE_8] - this mostly is implicit because a single level of a cubemap array can't
    // vary format or dimension, so as long as we enforce cubemap square rules for arrays it works.
    for(int face = 0; face < faceCount; face++)
    {
      GLint expectedWidth = levelBaseWidth;
      GLint expectedHeight = levelBaseHeight;
      GLint expectedDepth = levelBaseDepth;

      GLenum curFaceLevelBaseFormat = eGL_NONE;

      for(GLint level = levelBase; level <= q; level++)
      {
        GLint levelWidth = 1, levelHeight = 1, levelDepth = 1;
        GL.glGetTextureLevelParameterivEXT(tex, targets[face], level, eGL_TEXTURE_WIDTH, &levelWidth);
        if(dimensions >= 2)
          GL.glGetTextureLevelParameterivEXT(tex, targets[face], level, eGL_TEXTURE_HEIGHT,
                                             &levelHeight);
        if(dimensions >= 3)
          GL.glGetTextureLevelParameterivEXT(tex, targets[face], level, eGL_TEXTURE_DEPTH,
                                             &levelDepth);

        GLenum fmt = eGL_NONE;
        GL.glGetTextureLevelParameterivEXT(tex, targets[face], level, eGL_TEXTURE_INTERNAL_FORMAT,
                                           (GLint *)&fmt);

        if(level == levelBase)
        {
          curFaceLevelBaseFormat = fmt;

          if(face == 0)
            levelBaseFormat = fmt;
        }

        rdcstr faceStr;
        rdcstr face0Str;
        if(faceCount > 1)
        {
          face0Str = StringFormat::Fmt(" of %s", ToStr(targets[0]).c_str());
          faceStr = StringFormat::Fmt(" of %s", ToStr(targets[face]).c_str());
        }

        // [RULE_10] [RULE_11] - cubemap completeness issues are fatal, return immediately.

        // [RULE_9]
        // [RULE_5] - by the loop, this also checks that all faces have positive dimensions
        if(level == levelBase && (levelWidth <= 0 || levelHeight <= 0 || levelDepth <= 0))
        {
          return StringFormat::Fmt(
              "BASE_LEVEL %d%s has invalid dimensions: %s", levelBase, faceStr.c_str(),
              DimensionString(dimensions, levelWidth, levelHeight, levelDepth).c_str());
        }

        // [RULE_5] - check the square property here
        // [RULE_8] - checking this applies for cubemap arrays too
        if(cube && level == levelBase && levelWidth != levelHeight)
        {
          return StringFormat::Fmt(
              "BASE_LEVEL %d%s has non-square dimensions: %s "
              "(BASE_LEVEL %d)\n",
              level, faceStr.c_str(),
              DimensionString(dimensions, levelWidth, levelHeight, levelDepth).c_str());
        }

        // [RULE_5] - check that all faces are identical dimensions here
        if(cube && level == levelBase &&
           (levelWidth != levelBaseWidth || levelHeight != levelBaseHeight))
        {
          return StringFormat::Fmt(
              "BASE_LEVEL %d%s has different dimensions: %s to BASE_LEVEL %d%s: %s", levelBase,
              faceStr.c_str(),
              DimensionString(dimensions, levelWidth, levelHeight, levelDepth).c_str(), levelBase,
              face0Str.c_str(),
              DimensionString(dimensions, levelBaseWidth, levelBaseHeight, levelBaseDepth).c_str());
        }

        // [RULE_6]
        if(face > 0 && levelBaseFormat != curFaceLevelBaseFormat)
        {
          return StringFormat::Fmt(
              "BASE_LEVEL %d%s has different format: %s to BASE_LEVEL %d%s: %s", levelBase,
              faceStr.c_str(), ToStr(curFaceLevelBaseFormat).c_str(), levelBase, face0Str.c_str(),
              ToStr(levelBaseFormat).c_str());
        }

        // below here are only mipmap completeness checks, break out if we're already mipmap
        // incomplete
        if(!mipmapComplete)
          break;

        // [RULE_1]
        if(level == levelBase)
        {
          curFaceLevelBaseFormat = fmt;

          // accept any valid format, but if mip 0 isn't defined that's an error. It shouldn't be
          // possible to have a texture with no format but valid dimensions (see above [RULE_9]
          // check), but be safe because GL is GL.
          if(fmt == eGL_NONE)
            return StringFormat::Fmt("BASE_LEVEL %d%s has no format.\n", levelBase, faceStr.c_str());
        }
        else
        {
          if(curFaceLevelBaseFormat != fmt)
          {
            mipmapComplete = false;

            // common case - mip isn't defined at all
            if(fmt == eGL_NONE)
            {
              mipmapIncompleteness +=
                  StringFormat::Fmt("Level %d%s is not defined. (BASE_LEVEL %d, MAX_LEVEL %d)\n",
                                    level, faceStr.c_str(), levelBase, levelMax);
            }
            else
            {
              // uncommon case, mip is defined but with the wrong format
              mipmapIncompleteness += StringFormat::Fmt(
                  "Mip level %d%s has format %s which doesn't match format %s at BASE_LEVEL %d%s "
                  "(MAX_LEVEL is %d)\n",
                  level, faceStr.c_str(), ToStr(fmt).c_str(), ToStr(curFaceLevelBaseFormat).c_str(),
                  levelBase, face0Str.c_str(), levelMax);
            }

            // stop processing, other problems may be the same
            break;
          }
        }

        // [RULE_2]
        // if the level was completely undefined, it would have failed the format check, so this
        // must be badly sized mips.
        // note that for e.g. 2D textures, depth is always 1 so will be trivially as expected.
        if(levelWidth != expectedWidth || levelHeight != expectedHeight || levelDepth != expectedDepth)
        {
          mipmapComplete = false;
          mipmapIncompleteness += StringFormat::Fmt(
              "Mip level %d%s has invalid dimensions: %s, expected: %s "
              "(BASE_LEVEL %d, MAX_LEVEL %d)\n",
              level, faceStr.c_str(),
              DimensionString(dimensions, levelWidth, levelHeight, levelDepth).c_str(),
              DimensionString(dimensions, expectedWidth, expectedHeight, expectedDepth).c_str(),
              levelBase, levelMax);

          break;
        }

        expectedWidth = RDCMAX(1, expectedWidth >> 1);
        expectedHeight = RDCMAX(1, expectedHeight >> 1);
        expectedDepth = RDCMAX(1, expectedDepth >> 1);
      }
    }
  }

  // [RULE_3]
  if(mipmapComplete && !(levelBase <= levelMax))
  {
    mipmapComplete = false;
    mipmapIncompleteness +=
        StringFormat::Fmt("BASE_LEVEL %d must be <= MAX_LEVEL %d\n", levelBase, levelMax);
  }

  // MSAA textures don't have sampler state, so they count as if they are NEAREST. This means they
  // can't fail due to filtering modes, so we can return
  if(target == eGL_TEXTURE_2D_MULTISAMPLE || target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    return rdcstr();

  GLenum minf = eGL_NEAREST;
  GLenum magf = eGL_NEAREST;

  if(sampler != 0)
    GL.glGetSamplerParameteriv(sampler, eGL_TEXTURE_MIN_FILTER, (GLint *)&minf);
  else
    GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MIN_FILTER, (GLint *)&minf);

  if(sampler != 0)
    GL.glGetSamplerParameteriv(sampler, eGL_TEXTURE_MAG_FILTER, (GLint *)&magf);
  else
    GL.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MAG_FILTER, (GLint *)&magf);

  // [RULE_12]
  if(minf != eGL_NEAREST && minf != eGL_LINEAR && !mipmapComplete)
    return StringFormat::Fmt(
        "TEXTURE_MIN_FILTER is %s which requires mipmaps, "
        "but texture is mipmap incomplete:\n%s",
        ToStr(minf).c_str(), mipmapIncompleteness.c_str());

  rdcstr ret;

  // [RULE_13] [RULE_14] [RULE_15] - detect linear filters in either direction
  if(magf != eGL_NEAREST)
    ret = StringFormat::Fmt("TEXTURE_MAG_FILTER is %s", ToStr(magf).c_str());
  else if(minf != eGL_NEAREST && minf != eGL_NEAREST_MIPMAP_NEAREST)
    ret = StringFormat::Fmt("TEXTURE_MIN_FILTER is %s", ToStr(minf).c_str());

  // if we have a linear filter, check for non-filterable formats
  if(!ret.isEmpty())
  {
    // all compressed formats are filterable
    if(IsCompressedFormat(levelBaseFormat))
      return rdcstr();

    // [RULE_13]
    if(IsUIntFormat(levelBaseFormat) || IsSIntFormat(levelBaseFormat))
    {
      return ret + StringFormat::Fmt(" and texture is integer format (%s)",
                                     ToStr(levelBaseFormat).c_str());
    }
    // [RULE_14]
    else if(GetBaseFormat(levelBaseFormat) == eGL_STENCIL_INDEX)
    {
      return ret + StringFormat::Fmt(" and texture is stencil format (%s)",
                                     ToStr(levelBaseFormat).c_str());
    }
    // [RULE_15]
    else if(GetBaseFormat(levelBaseFormat) == eGL_DEPTH_STENCIL)
    {
      GLint depthMode = eGL_DEPTH_COMPONENT;

      if(HasExt[ARB_stencil_texturing])
        GL.glGetTextureParameterivEXT(tex, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &depthMode);

      if(depthMode == eGL_STENCIL_INDEX)
      {
        return ret + StringFormat::Fmt(
                         " and texture is depth/stencil format (%s) "
                         "with DEPTH_STENCIL_TEXTURE_MODE == STENCIL_INDEX",
                         ToStr(levelBaseFormat).c_str());
      }
    }
  }

  // no completeness problems!
  return rdcstr();
}

bool EmulateLuminanceFormat(GLuint tex, GLenum target, GLenum &internalFormat, GLenum &dataFormat)
{
  GLenum swizzle[] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};

  // set swizzles
  switch(internalFormat)
  {
    case eGL_INTENSITY32F_ARB:
    case eGL_INTENSITY16F_ARB:
    case eGL_INTENSITY_EXT:
    case eGL_INTENSITY8_EXT:
    case eGL_INTENSITY16_EXT:
    case eGL_INTENSITY32UI_EXT:
    case eGL_INTENSITY16UI_EXT:
    case eGL_INTENSITY8UI_EXT:
    case eGL_INTENSITY32I_EXT:
    case eGL_INTENSITY16I_EXT:
    case eGL_INTENSITY8I_EXT:
    case eGL_INTENSITY_SNORM:
    case eGL_INTENSITY8_SNORM:
    case eGL_INTENSITY16_SNORM:
      // intensity replicates across all 4 of RGBA
      swizzle[0] = swizzle[1] = swizzle[2] = swizzle[3] = eGL_RED;
      break;
    case eGL_ALPHA:
    case eGL_ALPHA_INTEGER:
    case eGL_ALPHA32F_ARB:
    case eGL_ALPHA16F_ARB:
    case eGL_ALPHA8_EXT:
    case eGL_ALPHA16_EXT:
    case eGL_ALPHA32UI_EXT:
    case eGL_ALPHA16UI_EXT:
    case eGL_ALPHA8UI_EXT:
    case eGL_ALPHA32I_EXT:
    case eGL_ALPHA16I_EXT:
    case eGL_ALPHA8I_EXT:
    case eGL_ALPHA_SNORM:
    case eGL_ALPHA8_SNORM:
    case eGL_ALPHA16_SNORM:
      // single component alpha channel
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_ZERO;
      swizzle[3] = eGL_RED;
      break;
    case eGL_LUMINANCE:
    case eGL_LUMINANCE32F_ARB:
    case eGL_LUMINANCE16F_ARB:
    case eGL_LUMINANCE8_EXT:
    case eGL_LUMINANCE16_EXT:
    case eGL_LUMINANCE32UI_EXT:
    case eGL_LUMINANCE16UI_EXT:
    case eGL_LUMINANCE8UI_EXT:
    case eGL_LUMINANCE32I_EXT:
    case eGL_LUMINANCE16I_EXT:
    case eGL_LUMINANCE8I_EXT:
    case eGL_LUMINANCE_INTEGER_EXT:
    case eGL_LUMINANCE_SNORM:
    case eGL_LUMINANCE8_SNORM:
    case eGL_LUMINANCE16_SNORM:
    case eGL_SLUMINANCE:
    case eGL_SLUMINANCE8:
      // luminance replicates over RGB
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      // alpha explicitly set to 1 in luminance formats
      swizzle[3] = eGL_ONE;
      break;
    case eGL_LUMINANCE_ALPHA:
    case eGL_LUMINANCE_ALPHA32F_ARB:
    case eGL_LUMINANCE_ALPHA16F_ARB:
    case eGL_LUMINANCE8_ALPHA8_EXT:
    case eGL_LUMINANCE16_ALPHA16_EXT:
    case eGL_LUMINANCE_ALPHA32UI_EXT:
    case eGL_LUMINANCE_ALPHA16UI_EXT:
    case eGL_LUMINANCE_ALPHA8UI_EXT:
    case eGL_LUMINANCE_ALPHA32I_EXT:
    case eGL_LUMINANCE_ALPHA16I_EXT:
    case eGL_LUMINANCE_ALPHA8I_EXT:
    case eGL_LUMINANCE_ALPHA_INTEGER_EXT:
    case eGL_LUMINANCE_ALPHA_SNORM:
    case eGL_LUMINANCE8_ALPHA8_SNORM:
    case eGL_LUMINANCE16_ALPHA16_SNORM:
    case eGL_SLUMINANCE_ALPHA:
    case eGL_SLUMINANCE8_ALPHA8:
      // luminance over RGB
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      // alpha in alpha
      swizzle[3] = eGL_GREEN;
      break;
    default: return false;
  }

  // patch the data format
  if(dataFormat == eGL_LUMINANCE || dataFormat == eGL_LUMINANCE_INTEGER_EXT ||
     dataFormat == eGL_LUMINANCE_ALPHA || dataFormat == eGL_LUMINANCE_ALPHA_INTEGER_EXT ||
     dataFormat == eGL_ALPHA || dataFormat == eGL_ALPHA_INTEGER || dataFormat == eGL_INTENSITY_EXT)
  {
    switch(internalFormat)
    {
      case eGL_INTENSITY_EXT:
      case eGL_INTENSITY8_EXT:
      case eGL_INTENSITY16_EXT:
      case eGL_INTENSITY16F_ARB:
      case eGL_INTENSITY32F_ARB:
      case eGL_INTENSITY_SNORM:
      case eGL_INTENSITY8_SNORM:
      case eGL_INTENSITY16_SNORM:
      case eGL_ALPHA:
      case eGL_ALPHA8_EXT:
      case eGL_ALPHA16_EXT:
      case eGL_ALPHA16F_ARB:
      case eGL_ALPHA32F_ARB:
      case eGL_ALPHA_SNORM:
      case eGL_ALPHA8_SNORM:
      case eGL_ALPHA16_SNORM:
      case eGL_LUMINANCE:
      case eGL_LUMINANCE8_EXT:
      case eGL_LUMINANCE16_EXT:
      case eGL_LUMINANCE16F_ARB:
      case eGL_LUMINANCE32F_ARB:
      case eGL_LUMINANCE_SNORM:
      case eGL_LUMINANCE8_SNORM:
      case eGL_LUMINANCE16_SNORM:
      case eGL_SLUMINANCE:
      case eGL_SLUMINANCE8: dataFormat = eGL_RED; break;
      case eGL_INTENSITY8I_EXT:
      case eGL_INTENSITY16I_EXT:
      case eGL_INTENSITY32I_EXT:
      case eGL_INTENSITY8UI_EXT:
      case eGL_INTENSITY16UI_EXT:
      case eGL_INTENSITY32UI_EXT:
      case eGL_ALPHA_INTEGER:
      case eGL_ALPHA8I_EXT:
      case eGL_ALPHA16I_EXT:
      case eGL_ALPHA32I_EXT:
      case eGL_ALPHA8UI_EXT:
      case eGL_ALPHA16UI_EXT:
      case eGL_ALPHA32UI_EXT:
      case eGL_LUMINANCE_INTEGER_EXT:
      case eGL_LUMINANCE8I_EXT:
      case eGL_LUMINANCE16I_EXT:
      case eGL_LUMINANCE32I_EXT:
      case eGL_LUMINANCE8UI_EXT:
      case eGL_LUMINANCE16UI_EXT:
      case eGL_LUMINANCE32UI_EXT: dataFormat = eGL_RED_INTEGER; break;
      case eGL_LUMINANCE_ALPHA:
      case eGL_LUMINANCE8_ALPHA8_EXT:
      case eGL_LUMINANCE16_ALPHA16_EXT:
      case eGL_LUMINANCE_ALPHA16F_ARB:
      case eGL_LUMINANCE_ALPHA32F_ARB:
      case eGL_LUMINANCE_ALPHA_SNORM:
      case eGL_LUMINANCE8_ALPHA8_SNORM:
      case eGL_LUMINANCE16_ALPHA16_SNORM:
      case eGL_SLUMINANCE_ALPHA:
      case eGL_SLUMINANCE8_ALPHA8: dataFormat = eGL_RG; break;
      case eGL_LUMINANCE_ALPHA_INTEGER_EXT:
      case eGL_LUMINANCE_ALPHA8I_EXT:
      case eGL_LUMINANCE_ALPHA16I_EXT:
      case eGL_LUMINANCE_ALPHA32I_EXT:
      case eGL_LUMINANCE_ALPHA8UI_EXT:
      case eGL_LUMINANCE_ALPHA16UI_EXT:
      case eGL_LUMINANCE_ALPHA32UI_EXT: dataFormat = eGL_RG_INTEGER; break;
      default:
        RDCERR("Problem in EnumerateLuminanceFormat - all switches should have same cases");
        break;
    }
  }

  switch(internalFormat)
  {
    case eGL_INTENSITY_EXT:
    case eGL_ALPHA:
    case eGL_INTENSITY8_EXT:
    case eGL_ALPHA8_EXT:
    case eGL_LUMINANCE:
    case eGL_LUMINANCE8_EXT: internalFormat = eGL_R8; break;
    case eGL_INTENSITY16_EXT:
    case eGL_ALPHA16_EXT:
    case eGL_LUMINANCE16_EXT: internalFormat = eGL_R16; break;
    case eGL_INTENSITY16F_ARB:
    case eGL_ALPHA16F_ARB:
    case eGL_LUMINANCE16F_ARB: internalFormat = eGL_R16F; break;
    case eGL_INTENSITY32F_ARB:
    case eGL_ALPHA32F_ARB:
    case eGL_LUMINANCE32F_ARB: internalFormat = eGL_R32F; break;
    case eGL_INTENSITY_SNORM:
    case eGL_INTENSITY8_SNORM:
    case eGL_ALPHA_SNORM:
    case eGL_ALPHA8_SNORM:
    case eGL_LUMINANCE_SNORM:
    case eGL_LUMINANCE8_SNORM: internalFormat = eGL_R8_SNORM; break;
    case eGL_INTENSITY16_SNORM:
    case eGL_ALPHA16_SNORM:
    case eGL_LUMINANCE16_SNORM: internalFormat = eGL_R16_SNORM; break;
    case eGL_INTENSITY8I_EXT:
    case eGL_ALPHA_INTEGER:
    case eGL_ALPHA8I_EXT:
    case eGL_LUMINANCE_INTEGER_EXT:
    case eGL_LUMINANCE8I_EXT: internalFormat = eGL_R8I; break;
    case eGL_INTENSITY16I_EXT:
    case eGL_ALPHA16I_EXT:
    case eGL_LUMINANCE16I_EXT: internalFormat = eGL_R16I; break;
    case eGL_INTENSITY32I_EXT:
    case eGL_ALPHA32I_EXT:
    case eGL_LUMINANCE32I_EXT: internalFormat = eGL_R32I; break;
    case eGL_INTENSITY8UI_EXT:
    case eGL_ALPHA8UI_EXT:
    case eGL_LUMINANCE8UI_EXT: internalFormat = eGL_R8UI; break;
    case eGL_INTENSITY16UI_EXT:
    case eGL_ALPHA16UI_EXT:
    case eGL_LUMINANCE16UI_EXT: internalFormat = eGL_R16UI; break;
    case eGL_INTENSITY32UI_EXT:
    case eGL_ALPHA32UI_EXT:
    case eGL_LUMINANCE32UI_EXT: internalFormat = eGL_R32UI; break;
    case eGL_LUMINANCE_ALPHA:
    case eGL_LUMINANCE8_ALPHA8_EXT: internalFormat = eGL_RG8; break;
    case eGL_LUMINANCE16_ALPHA16_EXT: internalFormat = eGL_RG16; break;
    case eGL_LUMINANCE_ALPHA16F_ARB: internalFormat = eGL_RG16F; break;
    case eGL_LUMINANCE_ALPHA32F_ARB: internalFormat = eGL_RG32F; break;
    case eGL_LUMINANCE_ALPHA_SNORM:
    case eGL_LUMINANCE8_ALPHA8_SNORM: internalFormat = eGL_RG8_SNORM; break;
    case eGL_LUMINANCE16_ALPHA16_SNORM: internalFormat = eGL_RG16_SNORM; break;
    case eGL_LUMINANCE_ALPHA_INTEGER_EXT:
    case eGL_LUMINANCE_ALPHA8I_EXT: internalFormat = eGL_RG8I; break;
    case eGL_LUMINANCE_ALPHA16I_EXT: internalFormat = eGL_RG16I; break;
    case eGL_LUMINANCE_ALPHA32I_EXT: internalFormat = eGL_RG32I; break;
    case eGL_LUMINANCE_ALPHA8UI_EXT: internalFormat = eGL_RG8UI; break;
    case eGL_LUMINANCE_ALPHA16UI_EXT: internalFormat = eGL_RG16UI; break;
    case eGL_LUMINANCE_ALPHA32UI_EXT: internalFormat = eGL_RG32UI; break;
    case eGL_SLUMINANCE:
    case eGL_SLUMINANCE8: internalFormat = eGL_SRGB8; break;
    case eGL_SLUMINANCE_ALPHA:
    case eGL_SLUMINANCE8_ALPHA8: internalFormat = eGL_SRGB8_ALPHA8; break;
    default:
      RDCERR("Problem in EnumerateLuminanceFormat - all switches should have same cases");
      break;
  }

  if(tex)
  {
    if(HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle])
    {
      SetTextureSwizzle(tex, target, swizzle);
    }
    else
    {
      RDCERR("Cannot emulate luminance format without texture swizzle extension");
    }
  }

  return true;
}

GLenum GetSizedFormat(GLenum internalformat)
{
  switch(internalformat)
  {
    case eGL_DEPTH_COMPONENT: internalformat = eGL_DEPTH_COMPONENT32F; break;
    case eGL_DEPTH_STENCIL: internalformat = eGL_DEPTH32F_STENCIL8; break;
    case eGL_STENCIL:
    case eGL_STENCIL_INDEX: internalformat = eGL_STENCIL_INDEX8; break;
    case eGL_RGBA: internalformat = eGL_RGBA8; break;
    case eGL_RGBA_INTEGER: internalformat = eGL_RGBA8I; break;
    case eGL_RGB: internalformat = eGL_RGB8; break;
    case eGL_RGB_INTEGER: internalformat = eGL_RGB8I; break;
    case eGL_RG: internalformat = eGL_RG8; break;
    case eGL_RG_INTEGER: internalformat = eGL_RG8I; break;
    case eGL_RED: internalformat = eGL_R8; break;
    case eGL_RED_INTEGER: internalformat = eGL_R8I; break;
    default: break;
  }

  return internalformat;
}

bool IsCompressedFormat(GLenum internalFormat)
{
  switch(internalFormat)
  {
    // BC1
    case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    // BC2
    case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    // BC3
    case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
    // BC4
    case eGL_COMPRESSED_RED_RGTC1:
    case eGL_COMPRESSED_SIGNED_RED_RGTC1:
    // BC5
    case eGL_COMPRESSED_RG_RGTC2:
    case eGL_COMPRESSED_SIGNED_RG_RGTC2:
    // BC6
    case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
    // BC7
    case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
    // ETC1
    case eGL_ETC1_RGB8_OES:
    // ETC2
    case eGL_COMPRESSED_RGB8_ETC2:
    case eGL_COMPRESSED_SRGB8_ETC2:
    case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    // EAC
    case eGL_COMPRESSED_RGBA8_ETC2_EAC:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    case eGL_COMPRESSED_R11_EAC:
    case eGL_COMPRESSED_SIGNED_R11_EAC:
    case eGL_COMPRESSED_RG11_EAC:
    case eGL_COMPRESSED_SIGNED_RG11_EAC:
    // ASTC
    case eGL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case eGL_COMPRESSED_RGBA_ASTC_3x3x3_OES:
    case eGL_COMPRESSED_RGBA_ASTC_4x3x3_OES:
    case eGL_COMPRESSED_RGBA_ASTC_4x4x3_OES:
    case eGL_COMPRESSED_RGBA_ASTC_4x4x4_OES:
    case eGL_COMPRESSED_RGBA_ASTC_5x4x4_OES:
    case eGL_COMPRESSED_RGBA_ASTC_5x5x4_OES:
    case eGL_COMPRESSED_RGBA_ASTC_5x5x5_OES:
    case eGL_COMPRESSED_RGBA_ASTC_6x5x5_OES:
    case eGL_COMPRESSED_RGBA_ASTC_6x6x5_OES:
    case eGL_COMPRESSED_RGBA_ASTC_6x6x6_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES:
    // PVRTC
    case eGL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT: return true;
    default: break;
  }

  return false;
}

bool IsDepthStencilFormat(GLenum internalFormat)
{
  if(IsCompressedFormat(internalFormat))
    return false;

  GLenum fmt = GetBaseFormat(internalFormat);

  return (fmt == eGL_DEPTH_COMPONENT || fmt == eGL_STENCIL_INDEX || fmt == eGL_DEPTH_STENCIL);
}

bool IsUIntFormat(GLenum internalFormat)
{
  switch(internalFormat)
  {
    case eGL_R8UI:
    case eGL_RG8UI:
    case eGL_RGB8UI:
    case eGL_RGBA8UI:
    case eGL_R16UI:
    case eGL_RG16UI:
    case eGL_RGB16UI:
    case eGL_RGBA16UI:
    case eGL_R32UI:
    case eGL_RG32UI:
    case eGL_RGB32UI:
    case eGL_RGBA32UI:
    case eGL_RGB10_A2UI: return true;
    default: break;
  }

  return false;
}

bool IsSIntFormat(GLenum internalFormat)
{
  switch(internalFormat)
  {
    case eGL_R8I:
    case eGL_RG8I:
    case eGL_RGB8I:
    case eGL_RGBA8I:
    case eGL_R16I:
    case eGL_RG16I:
    case eGL_RGB16I:
    case eGL_RGBA16I:
    case eGL_R32I:
    case eGL_RG32I:
    case eGL_RGB32I:
    case eGL_RGBA32I: return true;
    default: break;
  }

  return false;
}

bool IsSRGBFormat(GLenum internalFormat)
{
  switch(internalFormat)
  {
    case eGL_SRGB8:
    case eGL_SRGB8_ALPHA8:
    case eGL_SRGB:
    case eGL_SRGB_ALPHA:
    case eGL_SLUMINANCE8:
    case eGL_SLUMINANCE8_ALPHA8:
    case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB: return true;
    default: break;
  }

  return false;
}

GLenum GetViewCastedFormat(GLenum internalFormat, CompType typeCast)
{
  if(typeCast == CompType::Typeless)
    return internalFormat;

  switch(internalFormat)
  {
    case eGL_RGBA:
    case eGL_RGBA8:
    case eGL_RGBA8_SNORM:
    case eGL_RGBA8UI:
    case eGL_RGBA8I:
    case eGL_SRGB_ALPHA:
    case eGL_SRGB8_ALPHA8:
      switch(typeCast)
      {
        case CompType::Float:
        case CompType::UNorm: internalFormat = eGL_RGBA8; break;
        case CompType::SNorm: internalFormat = eGL_RGBA8_SNORM; break;
        case CompType::UInt: internalFormat = eGL_RGBA8UI; break;
        case CompType::SInt: internalFormat = eGL_RGBA8I; break;
        case CompType::UNormSRGB: internalFormat = eGL_SRGB8_ALPHA8; break;
        default: break;
      }
      break;

    case eGL_RGB:
    case eGL_RGB8:
    case eGL_RGB8_SNORM:
    case eGL_RGB8UI:
    case eGL_RGB8I:
    case eGL_SRGB:
    case eGL_SRGB8:
      switch(typeCast)
      {
        case CompType::Float:
        case CompType::UNorm: internalFormat = eGL_RGB8; break;
        case CompType::SNorm: internalFormat = eGL_RGB8_SNORM; break;
        case CompType::UInt: internalFormat = eGL_RGB8UI; break;
        case CompType::SInt: internalFormat = eGL_RGB8I; break;
        case CompType::UNormSRGB: internalFormat = eGL_SRGB8; break;
        default: break;
      }
      break;

    case eGL_RG:
    case eGL_RG8:
    case eGL_RG8_SNORM:
    case eGL_RG8UI:
    case eGL_RG8I:
      switch(typeCast)
      {
        case CompType::Float:
        case CompType::UNorm: internalFormat = eGL_RG8; break;
        case CompType::SNorm: internalFormat = eGL_RG8_SNORM; break;
        case CompType::UInt: internalFormat = eGL_RG8UI; break;
        case CompType::SInt: internalFormat = eGL_RG8I; break;
        case CompType::UNormSRGB: internalFormat = eGL_SRG8_EXT; break;
        default: break;
      }
      break;

    case eGL_RED:
    case eGL_R8:
    case eGL_R8_SNORM:
    case eGL_R8UI:
    case eGL_R8I:
      switch(typeCast)
      {
        case CompType::Float:
        case CompType::UNorm: internalFormat = eGL_R8; break;
        case CompType::SNorm: internalFormat = eGL_R8_SNORM; break;
        case CompType::UInt: internalFormat = eGL_R8UI; break;
        case CompType::SInt: internalFormat = eGL_R8I; break;
        case CompType::UNormSRGB: internalFormat = eGL_SR8_EXT; break;
        default: break;
      }
      break;

    case eGL_RGBA16F:
    case eGL_RGBA16:
    case eGL_RGBA16_SNORM:
    case eGL_RGBA16UI:
    case eGL_RGBA16I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_RGBA16F; break;
        case CompType::UNorm: internalFormat = eGL_RGBA16; break;
        case CompType::SNorm: internalFormat = eGL_RGBA16_SNORM; break;
        case CompType::UInt: internalFormat = eGL_RGBA16UI; break;
        case CompType::SInt: internalFormat = eGL_RGBA16I; break;
        default: break;
      }
      break;

    case eGL_RGB16F:
    case eGL_RGB16:
    case eGL_RGB16_SNORM:
    case eGL_RGB16UI:
    case eGL_RGB16I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_RGB16F; break;
        case CompType::UNorm: internalFormat = eGL_RGB16; break;
        case CompType::SNorm: internalFormat = eGL_RGB16_SNORM; break;
        case CompType::UInt: internalFormat = eGL_RGB16UI; break;
        case CompType::SInt: internalFormat = eGL_RGB16I; break;
        default: break;
      }
      break;

    case eGL_RG16F:
    case eGL_RG16:
    case eGL_RG16_SNORM:
    case eGL_RG16UI:
    case eGL_RG16I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_RG16F; break;
        case CompType::UNorm: internalFormat = eGL_RG16; break;
        case CompType::SNorm: internalFormat = eGL_RG16_SNORM; break;
        case CompType::UInt: internalFormat = eGL_RG16UI; break;
        case CompType::SInt: internalFormat = eGL_RG16I; break;
        default: break;
      }
      break;

    case eGL_R16F:
    case eGL_R16:
    case eGL_R16_SNORM:
    case eGL_R16UI:
    case eGL_R16I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_R16F; break;
        case CompType::UNorm: internalFormat = eGL_R16; break;
        case CompType::SNorm: internalFormat = eGL_R16_SNORM; break;
        case CompType::UInt: internalFormat = eGL_R16UI; break;
        case CompType::SInt: internalFormat = eGL_R16I; break;
        default: break;
      }
      break;

    case eGL_RGBA32F:
    case eGL_RGBA32UI:
    case eGL_RGBA32I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_RGBA32F; break;
        case CompType::UInt: internalFormat = eGL_RGBA32UI; break;
        case CompType::SInt: internalFormat = eGL_RGBA32I; break;
        default: break;
      }
      break;

    case eGL_RGB32F:
    case eGL_RGB32UI:
    case eGL_RGB32I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_RGB32F; break;
        case CompType::UInt: internalFormat = eGL_RGB32UI; break;
        case CompType::SInt: internalFormat = eGL_RGB32I; break;
        default: break;
      }
      break;

    case eGL_RG32F:
    case eGL_RG32UI:
    case eGL_RG32I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_RG32F; break;
        case CompType::UInt: internalFormat = eGL_RG32UI; break;
        case CompType::SInt: internalFormat = eGL_RG32I; break;
        default: break;
      }
      break;

    case eGL_R32F:
    case eGL_R32UI:
    case eGL_R32I:
      switch(typeCast)
      {
        case CompType::Float: internalFormat = eGL_R32F; break;
        case CompType::UInt: internalFormat = eGL_R32UI; break;
        case CompType::SInt: internalFormat = eGL_R32I; break;
        default: break;
      }
      break;

    case eGL_RGB10_A2UI:
    case eGL_RGB10_A2:
      switch(typeCast)
      {
        case CompType::Float:
        case CompType::UNorm: internalFormat = eGL_RGB10_A2; break;
        case CompType::UInt: internalFormat = eGL_RGB10_A2UI; break;
        default: break;
      }
      break;

    case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
      internalFormat = (typeCast == CompType::UNormSRGB) ? eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT
                                                         : eGL_COMPRESSED_RGB_S3TC_DXT1_EXT;
      break;

    case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
      internalFormat = (typeCast == CompType::UNormSRGB) ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                                                         : eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      break;

    case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
      internalFormat = (typeCast == CompType::UNormSRGB) ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                                                         : eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
      break;

    case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      internalFormat = (typeCast == CompType::UNormSRGB) ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                                         : eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
      break;

    case eGL_COMPRESSED_RED_RGTC1:
    case eGL_COMPRESSED_SIGNED_RED_RGTC1:
      internalFormat = (typeCast == CompType::SNorm) ? eGL_COMPRESSED_SIGNED_RED_RGTC1
                                                     : eGL_COMPRESSED_RED_RGTC1;
      break;

    case eGL_COMPRESSED_RG_RGTC2:
    case eGL_COMPRESSED_SIGNED_RG_RGTC2:
      internalFormat =
          (typeCast == CompType::SNorm) ? eGL_COMPRESSED_SIGNED_RG_RGTC2 : eGL_COMPRESSED_RG_RGTC2;
      break;

    case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
      internalFormat = (typeCast == CompType::SNorm) ? eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB
                                                     : eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
      break;

    case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
      internalFormat = (typeCast == CompType::UNormSRGB) ? eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB
                                                         : eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
      break;

    case eGL_COMPRESSED_SIGNED_R11_EAC:
    case eGL_COMPRESSED_R11_EAC:
      internalFormat =
          (typeCast == CompType::SNorm) ? eGL_COMPRESSED_SIGNED_R11_EAC : eGL_COMPRESSED_R11_EAC;
      break;

    case eGL_COMPRESSED_SIGNED_RG11_EAC:
    case eGL_COMPRESSED_RG11_EAC:
      internalFormat =
          (typeCast == CompType::SNorm) ? eGL_COMPRESSED_SIGNED_RG11_EAC : eGL_COMPRESSED_RG11_EAC;
      break;

    default: break;
  }

  return internalFormat;
}

GLenum TextureBinding(GLenum target)
{
  switch(target)
  {
    case eGL_TEXTURE_1D: return eGL_TEXTURE_BINDING_1D;
    case eGL_TEXTURE_1D_ARRAY: return eGL_TEXTURE_BINDING_1D_ARRAY;
    case eGL_TEXTURE_2D: return eGL_TEXTURE_BINDING_2D;
    case eGL_TEXTURE_2D_ARRAY: return eGL_TEXTURE_BINDING_2D_ARRAY;
    case eGL_TEXTURE_2D_MULTISAMPLE: return eGL_TEXTURE_BINDING_2D_MULTISAMPLE;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: return eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
    case eGL_TEXTURE_RECTANGLE: return eGL_TEXTURE_BINDING_RECTANGLE;
    case eGL_TEXTURE_3D: return eGL_TEXTURE_BINDING_3D;
    case eGL_TEXTURE_CUBE_MAP:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return eGL_TEXTURE_BINDING_CUBE_MAP;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: return eGL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
    case eGL_TEXTURE_BUFFER: return eGL_TEXTURE_BINDING_BUFFER;
    default: break;
  }

  RDCERR("Unexpected target %s", ToStr(target).c_str());
  return eGL_NONE;
}

GLenum BufferBinding(GLenum target)
{
  switch(target)
  {
    case eGL_ARRAY_BUFFER: return eGL_ARRAY_BUFFER_BINDING;
    case eGL_ATOMIC_COUNTER_BUFFER: return eGL_ATOMIC_COUNTER_BUFFER_BINDING;
    case eGL_COPY_READ_BUFFER: return eGL_COPY_READ_BUFFER_BINDING;
    case eGL_COPY_WRITE_BUFFER: return eGL_COPY_WRITE_BUFFER_BINDING;
    case eGL_DRAW_INDIRECT_BUFFER: return eGL_DRAW_INDIRECT_BUFFER_BINDING;
    case eGL_DISPATCH_INDIRECT_BUFFER: return eGL_DISPATCH_INDIRECT_BUFFER_BINDING;
    case eGL_ELEMENT_ARRAY_BUFFER: return eGL_ELEMENT_ARRAY_BUFFER_BINDING;
    case eGL_PIXEL_PACK_BUFFER: return eGL_PIXEL_PACK_BUFFER_BINDING;
    case eGL_PIXEL_UNPACK_BUFFER: return eGL_PIXEL_UNPACK_BUFFER_BINDING;
    case eGL_QUERY_BUFFER: return eGL_QUERY_BUFFER_BINDING;
    case eGL_SHADER_STORAGE_BUFFER: return eGL_SHADER_STORAGE_BUFFER_BINDING;
    case eGL_TEXTURE_BUFFER: return eGL_TEXTURE_BUFFER_BINDING;
    case eGL_TRANSFORM_FEEDBACK_BUFFER: return eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
    case eGL_UNIFORM_BUFFER: return eGL_UNIFORM_BUFFER_BINDING;
    case eGL_PARAMETER_BUFFER_ARB: return eGL_PARAMETER_BUFFER_BINDING_ARB;
    default: break;
  }

  RDCERR("Unexpected target %s", ToStr(target).c_str());
  return eGL_NONE;
}

GLenum FramebufferBinding(GLenum target)
{
  switch(target)
  {
    case eGL_FRAMEBUFFER: return eGL_FRAMEBUFFER_BINDING;
    case eGL_DRAW_FRAMEBUFFER: return eGL_DRAW_FRAMEBUFFER_BINDING;
    case eGL_READ_FRAMEBUFFER: return eGL_READ_FRAMEBUFFER_BINDING;
    default: break;
  }

  RDCERR("Unexpected target %s", ToStr(target).c_str());
  return eGL_NONE;
}

bool IsCubeFace(GLenum target)
{
  switch(target)
  {
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return true;
    default: break;
  }

  return false;
}

GLint CubeTargetIndex(GLenum face)
{
  switch(face)
  {
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X: return 0;
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X: return 1;
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y: return 2;
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return 3;
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z: return 4;
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return 5;
    default: break;
  }

  return 0;
}

GLenum TextureTarget(GLenum binding)
{
  switch(binding)
  {
    case eGL_TEXTURE_BINDING_1D: return eGL_TEXTURE_1D;
    case eGL_TEXTURE_BINDING_1D_ARRAY: return eGL_TEXTURE_1D_ARRAY;
    case eGL_TEXTURE_BINDING_2D: return eGL_TEXTURE_2D;
    case eGL_TEXTURE_BINDING_2D_ARRAY: return eGL_TEXTURE_2D_ARRAY;
    case eGL_TEXTURE_BINDING_2D_MULTISAMPLE: return eGL_TEXTURE_2D_MULTISAMPLE;
    case eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY: return eGL_TEXTURE_2D_MULTISAMPLE_ARRAY;
    case eGL_TEXTURE_BINDING_RECTANGLE: return eGL_TEXTURE_RECTANGLE;
    case eGL_TEXTURE_BINDING_3D: return eGL_TEXTURE_3D;
    case eGL_TEXTURE_BINDING_CUBE_MAP:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return eGL_TEXTURE_CUBE_MAP;
    case eGL_TEXTURE_BINDING_CUBE_MAP_ARRAY: return eGL_TEXTURE_CUBE_MAP_ARRAY;
    case eGL_TEXTURE_BINDING_BUFFER: return eGL_TEXTURE_BUFFER;
    default: break;
  }

  return binding;
}

bool IsProxyTarget(GLenum target)
{
  switch(target)
  {
    case eGL_PROXY_TEXTURE_1D:
    case eGL_PROXY_TEXTURE_1D_ARRAY:
    case eGL_PROXY_TEXTURE_2D:
    case eGL_PROXY_TEXTURE_2D_ARRAY:
    case eGL_PROXY_TEXTURE_2D_MULTISAMPLE:
    case eGL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
    case eGL_PROXY_TEXTURE_RECTANGLE:
    case eGL_PROXY_TEXTURE_3D:
    case eGL_PROXY_TEXTURE_CUBE_MAP:
    case eGL_PROXY_TEXTURE_CUBE_MAP_ARRAY: return true;
    default: break;
  }

  return false;
}
