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

#include "gl_resources.h"
#include "gl_hookset.h"

byte GLResourceRecord::markerValue[32] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0x88, 0x77, 0x66, 0x55, 0x01, 0x23, 0x45, 0x67, 0x98, 0x76, 0x54, 0x32,
};

size_t GetCompressedByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum internalformat)
{
  if(!IsCompressedFormat(internalformat))
  {
    RDCERR("Not compressed format %s", ToStr::Get(internalformat).c_str());
    return GetByteSize(w, h, d, GetBaseFormat(internalformat), GetDataType(internalformat));
  }

  uint32_t astc[2] = {0, 0};

  switch(internalformat)
  {
    // BC1
    case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
      return (AlignUp4(w) * AlignUp4(h) * d) / 2;
    // BC2
    case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
      return (AlignUp4(w) * AlignUp4(h) * d);
    // BC3
    case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      return (AlignUp4(w) * AlignUp4(h) * d);
    // BC4
    case eGL_COMPRESSED_RED_RGTC1:
    case eGL_COMPRESSED_SIGNED_RED_RGTC1:
      return (AlignUp4(w) * AlignUp4(h) * d) / 2;
    // BC5
    case eGL_COMPRESSED_RG_RGTC2:
    case eGL_COMPRESSED_SIGNED_RG_RGTC2:
      return (AlignUp4(w) * AlignUp4(h) * d);
    // BC6
    case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
    case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
      return (AlignUp4(w) * AlignUp4(h) * d);
    // BC7
    case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
    case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
      return (AlignUp4(w) * AlignUp4(h) * d);
    // ETC1
    case eGL_ETC1_RGB8_OES:
    // ETC2
    case eGL_COMPRESSED_RGB8_ETC2:
    case eGL_COMPRESSED_SRGB8_ETC2:
    case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      return (AlignUp4(w) * AlignUp4(h) * d) / 2;
    // EAC
    case eGL_COMPRESSED_RGBA8_ETC2_EAC:
    case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: return (AlignUp4(w) * AlignUp4(h) * d);
    case eGL_COMPRESSED_R11_EAC:
    case eGL_COMPRESSED_SIGNED_R11_EAC: return (AlignUp4(w) * AlignUp4(h) * d) / 2;
    case eGL_COMPRESSED_RG11_EAC:
    case eGL_COMPRESSED_SIGNED_RG11_EAC: return (AlignUp4(w) * AlignUp4(h) * d);
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
      astc[0] = 4;
      astc[1] = 4;
      break;
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
      astc[0] = 5;
      astc[1] = 4;
      break;
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
      astc[0] = 5;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
      astc[0] = 6;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
      astc[0] = 6;
      astc[1] = 6;
      break;
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
      astc[0] = 8;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
      astc[0] = 8;
      astc[1] = 6;
      break;
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
      astc[0] = 8;
      astc[1] = 8;
      break;
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
      astc[0] = 10;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
      astc[0] = 10;
      astc[1] = 6;
      break;
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
      astc[0] = 10;
      astc[1] = 8;
      break;
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
      astc[0] = 10;
      astc[1] = 10;
      break;
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
      astc[0] = 12;
      astc[1] = 10;
      break;
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
      astc[0] = 12;
      astc[1] = 12;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
      astc[0] = 4;
      astc[1] = 4;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
      astc[0] = 5;
      astc[1] = 4;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
      astc[0] = 5;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
      astc[0] = 6;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
      astc[0] = 6;
      astc[1] = 6;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
      astc[0] = 8;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
      astc[0] = 8;
      astc[1] = 6;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
      astc[0] = 8;
      astc[1] = 8;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
      astc[0] = 10;
      astc[1] = 5;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
      astc[0] = 10;
      astc[1] = 6;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
      astc[0] = 10;
      astc[1] = 8;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
      astc[0] = 10;
      astc[1] = 10;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
      astc[0] = 12;
      astc[1] = 10;
      break;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
      astc[0] = 12;
      astc[1] = 12;
      break;
    default: break;
  }

  if(astc[0] > 0 && astc[1] > 0)
  {
    uint32_t blocks[2] = {(w / astc[0]), (h / astc[1])};

    // how many blocks are needed - including any extra partial blocks
    blocks[0] += (w % astc[0]) ? 1 : 0;
    blocks[1] += (h % astc[1]) ? 1 : 0;

    // ASTC blocks are all 128 bits each
    return blocks[0] * blocks[1] * 16 * d;
  }

  RDCERR("Unrecognised compressed format %s", ToStr::Get(internalformat).c_str());
  return GetByteSize(w, h, d, GetBaseFormat(internalformat), GetDataType(internalformat));
}

size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type)
{
  size_t elemSize = 1;

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
    case eGL_UNSIGNED_BYTE_2_3_3_REV: return w * h * d;
    case eGL_UNSIGNED_SHORT_5_6_5:
    case eGL_UNSIGNED_SHORT_5_6_5_REV:
    case eGL_UNSIGNED_SHORT_4_4_4_4:
    case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
    case eGL_UNSIGNED_SHORT_5_5_5_1:
    case eGL_UNSIGNED_SHORT_1_5_5_5_REV: return w * h * d * 2;
    case eGL_UNSIGNED_INT_8_8_8_8:
    case eGL_UNSIGNED_INT_8_8_8_8_REV:
    case eGL_UNSIGNED_INT_10_10_10_2:
    case eGL_UNSIGNED_INT_2_10_10_10_REV:
    case eGL_INT_2_10_10_10_REV:
    case eGL_UNSIGNED_INT_10F_11F_11F_REV:
    case eGL_UNSIGNED_INT_5_9_9_9_REV: return w * h * d * 4;
    case eGL_DEPTH_COMPONENT16: return w * h * d * 2;
    case eGL_DEPTH_COMPONENT24:
    case eGL_DEPTH24_STENCIL8:
    case eGL_DEPTH_COMPONENT32:
    case eGL_DEPTH_COMPONENT32F:
    case eGL_UNSIGNED_INT_24_8: return w * h * d * 4;
    case eGL_DEPTH32F_STENCIL8:
    case eGL_FLOAT_32_UNSIGNED_INT_24_8_REV: return w * h * d * 8;
    default: RDCERR("Unhandled Byte Size type %s!", ToStr::Get(type).c_str()); break;
  }

  switch((int)format)
  {
    case eGL_RED:
    case eGL_RED_INTEGER:
    case eGL_GREEN:
    case eGL_GREEN_INTEGER:
    case eGL_BLUE:
    case eGL_BLUE_INTEGER:
    case eGL_LUMINANCE:
    case eGL_ALPHA:
    case eGL_DEPTH_COMPONENT:
    case eGL_STENCIL_INDEX:
    case eGL_STENCIL: return w * h * d * elemSize;
    case eGL_RG:
    case eGL_RG_INTEGER:
    case eGL_LUMINANCE_ALPHA:
    case eGL_DEPTH_STENCIL: return w * h * d * elemSize * 2;
    case eGL_RGB:
    case eGL_RGB_INTEGER:
    case eGL_BGR:
    case eGL_BGR_INTEGER: return w * h * d * elemSize * 3;
    case eGL_RGBA:
    case eGL_RGBA_INTEGER:
    case eGL_BGRA:
    case eGL_BGRA_INTEGER: return w * h * d * elemSize * 4;
    default: RDCERR("Unhandled Byte Size format %s!", ToStr::Get(type).c_str()); break;
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
    case eGL_R16:
    case eGL_R16_SNORM:
    case eGL_R16F:
    case eGL_R32F:
    case eGL_RED: return eGL_RED;
    case eGL_ALPHA:
    case eGL_ALPHA8_EXT: return eGL_ALPHA;
    case eGL_LUMINANCE: return eGL_LUMINANCE;
    case eGL_LUMINANCE_ALPHA: return eGL_LUMINANCE_ALPHA;
    case eGL_INTENSITY: return eGL_INTENSITY;
    case eGL_R8I:
    case eGL_R16I:
    case eGL_R32I:
    case eGL_R32UI:
    case eGL_R16UI:
    case eGL_R8UI: return eGL_RED_INTEGER;
    case eGL_RG8:
    case eGL_RG8_SNORM:
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
    case eGL_RG32UI: return eGL_RG_INTEGER;
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
    case eGL_RGB32UI: return eGL_RGB_INTEGER;
    case eGL_RGBA2:
    case eGL_RGBA4:
    case eGL_RGB5_A1:
    case eGL_RGBA8:
    case eGL_RGBA8_SNORM:
    case eGL_RGB10_A2:
    case eGL_RGBA12:
    case eGL_RGBA16:
    case eGL_RGBA16_SNORM:
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
    case eGL_RGBA32I: return eGL_RGBA_INTEGER;
    case eGL_BGRA:
    case eGL_BGRA8_EXT: return eGL_BGRA;
    case eGL_DEPTH_COMPONENT16:
    case eGL_DEPTH_COMPONENT24:
    case eGL_DEPTH_COMPONENT32:
    case eGL_DEPTH_COMPONENT32F: return eGL_DEPTH_COMPONENT;
    case eGL_DEPTH24_STENCIL8:
    case eGL_DEPTH32F_STENCIL8: return eGL_DEPTH_STENCIL;
    case eGL_STENCIL_INDEX1:
    case eGL_STENCIL_INDEX4:
    case eGL_STENCIL_INDEX8:
    case eGL_STENCIL_INDEX16: return eGL_STENCIL;
    default: break;
  }

  RDCERR("Unhandled Base Format case %s!", ToStr::Get(internalFormat).c_str());

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
    case eGL_RGB10_A2UI: return eGL_INT_2_10_10_10_REV;
    case eGL_RGB10_A2: return eGL_UNSIGNED_INT_2_10_10_10_REV;
    case eGL_R3_G3_B2: return eGL_UNSIGNED_BYTE_3_3_2;
    case eGL_RGB4:
    case eGL_RGBA4: return eGL_UNSIGNED_SHORT_4_4_4_4;
    case eGL_RGB5_A1: return eGL_UNSIGNED_SHORT_5_5_5_1;
    case eGL_RGB565:
    case eGL_RGB5: return eGL_UNSIGNED_SHORT_5_6_5;
    case eGL_RGB10: return eGL_UNSIGNED_INT_10_10_10_2;
    case eGL_RGB9_E5: return eGL_UNSIGNED_INT_5_9_9_9_REV;
    case eGL_DEPTH24_STENCIL8: return eGL_UNSIGNED_INT_24_8;
    case eGL_DEPTH32F_STENCIL8: return eGL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    case eGL_STENCIL_INDEX8: return eGL_UNSIGNED_BYTE;
    case eGL_ALPHA:
    case eGL_ALPHA8_EXT:
    case eGL_LUMINANCE_ALPHA:
    case eGL_LUMINANCE:
    case eGL_INTENSITY: return eGL_UNSIGNED_BYTE;
    default: break;
  }

  RDCERR("Unhandled Data Type case %s!", ToStr::Get(internalFormat).c_str());

  return eGL_NONE;
}

int GetNumMips(const GLHookSet &gl, GLenum target, GLuint tex, GLuint w, GLuint h, GLuint d)
{
  int mips = 1;

  GLint immut = 0;
  gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_IMMUTABLE_FORMAT, &immut);

  if(immut)
    gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_IMMUTABLE_LEVELS, (GLint *)&mips);
  else
    mips = CalcNumMips(w, h, d);

  GLint maxLevel = 1000;
  gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MAX_LEVEL, &maxLevel);
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
      gl.glGetTextureLevelParameterivEXT(tex, target, i, eGL_TEXTURE_WIDTH, &width);
      if(width == 0)
      {
        mips = i;
        break;
      }
    }
  }

  return RDCMAX(1, mips);
}

GLenum GetSizedFormat(const GLHookSet &gl, GLenum target, GLenum internalFormat, GLenum type)
{
  switch(type)
  {
    // some types imply a sized internalFormat
    case eGL_UNSIGNED_SHORT_5_6_5: return eGL_RGB565;
    case eGL_UNSIGNED_SHORT_4_4_4_4: return eGL_RGBA4;
    case eGL_UNSIGNED_SHORT_5_5_5_1: return eGL_RGB5_A1;
    default: break;
  }

  switch(internalFormat)
  {
    // pick sized format ourselves for generic formats
    case eGL_COMPRESSED_RED: return eGL_COMPRESSED_RED_RGTC1;
    case eGL_COMPRESSED_RG: return eGL_COMPRESSED_RG_RGTC2;
    case eGL_COMPRESSED_RGB: return eGL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case eGL_COMPRESSED_RGBA: return eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case eGL_COMPRESSED_SRGB: return eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
    case eGL_COMPRESSED_SRGB_ALPHA:
      return eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;

    // only one sized format for SRGB
    case eGL_SRGB: return eGL_SRGB8;
    case eGL_SRGB_ALPHA: return eGL_SRGB8_ALPHA8;

    case eGL_RED:
    case eGL_RG:
    case eGL_RGB:
    case eGL_RGBA:
    case eGL_DEPTH_COMPONENT:
    case eGL_STENCIL:
    case eGL_STENCIL_INDEX:
    case eGL_DEPTH_STENCIL: break;
    default:
      return internalFormat;    // already explicitly sized
  }

  switch(target)
  {
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: target = eGL_TEXTURE_CUBE_MAP;
    default: break;
  }

  GLint red, depth, stencil;
  if(HasExt[ARB_internalformat_query2] && gl.glGetInternalformativ)
  {
    gl.glGetInternalformativ(target, internalFormat, eGL_INTERNALFORMAT_RED_SIZE, sizeof(GLint),
                             &red);
    gl.glGetInternalformativ(target, internalFormat, eGL_INTERNALFORMAT_DEPTH_SIZE, sizeof(GLint),
                             &depth);
    gl.glGetInternalformativ(target, internalFormat, eGL_INTERNALFORMAT_STENCIL_SIZE, sizeof(GLint),
                             &stencil);
  }
  else
  {
    // without the query function, just default to sensible defaults
    red = 8;
    if(type == eGL_FLOAT)
      depth = 32;
    else if(type == eGL_UNSIGNED_SHORT)
      depth = 16;
    else
      depth = 24;
    stencil = 8;
  }

  switch(internalFormat)
  {
    case eGL_RED:
      if(red == 32)
        return eGL_R32F;
      else if(red == 16)
        return eGL_R16;
      else
        return eGL_R8;
    case eGL_RG:
      if(red == 32)
        return eGL_RG32F;
      else if(red == 16)
        return eGL_RG16;
      else
        return eGL_RG8;
    case eGL_RGB:
      if(red == 32)
        return eGL_RGB32F;
      else if(red == 16)
        return eGL_RGB16;
      else
        return eGL_RGB8;
    case eGL_RGBA:
      if(red == 32)
        return eGL_RGBA32F;
      else if(red == 16)
        return eGL_RGBA16;
      else
        return eGL_RGBA8;
    case eGL_STENCIL:
    case eGL_STENCIL_INDEX:
      if(stencil == 16)
        return eGL_STENCIL_INDEX16;
      else
        return eGL_STENCIL_INDEX8;
    case eGL_DEPTH_COMPONENT:
      if(depth == 32)
        return eGL_DEPTH_COMPONENT32F;
      else if(depth == 16)
        return eGL_DEPTH_COMPONENT16;
      else
        return eGL_DEPTH_COMPONENT24;
    case eGL_DEPTH_STENCIL:
      if(depth == 32)
        return eGL_DEPTH32F_STENCIL8;
      else
        return eGL_DEPTH24_STENCIL8;
    default: break;
  }

  return internalFormat;
}

void GetFramebufferMipAndLayer(const GLHookSet &gl, GLenum framebuffer, GLenum attachment,
                               GLint *mip, GLint *layer)
{
  gl.glGetFramebufferAttachmentParameteriv(framebuffer, attachment,
                                           eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, mip);

  GLenum face = eGL_NONE;
  gl.glGetFramebufferAttachmentParameteriv(
      framebuffer, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

  if(face == 0)
  {
    gl.glGetFramebufferAttachmentParameteriv(framebuffer, attachment,
                                             eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, layer);
  }
  else
  {
    *layer = CubeTargetIndex(face);
  }
}

// GL_TEXTURE_SWIZZLE_RGBA is not supported on GLES, so for consistency we use r/g/b/a component
// swizzles for both GL and GLES.
// The same applies to SetTextureSwizzle function.
void GetTextureSwizzle(const GLHookSet &gl, GLuint tex, GLenum target, GLenum *swizzleRGBA)
{
  gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_R, (GLint *)&swizzleRGBA[0]);
  gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_G, (GLint *)&swizzleRGBA[1]);
  gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_B, (GLint *)&swizzleRGBA[2]);
  gl.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_A, (GLint *)&swizzleRGBA[3]);
}

void SetTextureSwizzle(const GLHookSet &gl, GLuint tex, GLenum target, GLenum *swizzleRGBA)
{
  gl.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_R, (GLint *)&swizzleRGBA[0]);
  gl.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_G, (GLint *)&swizzleRGBA[1]);
  gl.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_B, (GLint *)&swizzleRGBA[2]);
  gl.glTextureParameterivEXT(tex, target, eGL_TEXTURE_SWIZZLE_A, (GLint *)&swizzleRGBA[3]);
}

bool EmulateLuminanceFormat(const GLHookSet &gl, GLuint tex, GLenum target, GLenum &internalFormat,
                            GLenum &dataFormat)
{
  GLenum swizzle[] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};

  bool dataFormatLum = (dataFormat == eGL_LUMINANCE || dataFormat == eGL_LUMINANCE_ALPHA ||
                        dataFormat == eGL_ALPHA || dataFormat == eGL_INTENSITY);

  switch((int)internalFormat)
  {
    case eGL_INTENSITY:
    case eGL_INTENSITY8_EXT:
      internalFormat = eGL_R8;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = swizzle[3] =
          eGL_RED;    // intensity replicates across all 4 of RGBA
      break;
    case eGL_INTENSITY16_EXT:
      internalFormat = eGL_R16;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = swizzle[3] =
          eGL_RED;    // intensity replicates across all 4 of RGBA
      break;
    case eGL_ALPHA:
    case eGL_ALPHA8_EXT:
      internalFormat = eGL_R8;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_NONE;
      swizzle[3] = eGL_RED;    // single component alpha channel
      break;
    case eGL_LUMINANCE:
    case eGL_LUMINANCE8_EXT:
      internalFormat = eGL_R8;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = (GLenum)1;    // alpha explicitly set to 1 in luminance formats
      break;
    case eGL_SLUMINANCE8_EXT:
      internalFormat = eGL_SRGB8;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = (GLenum)1;    // alpha explicitly set to 1 in luminance formats
      break;
    case eGL_LUMINANCE16_EXT:
      internalFormat = eGL_R16;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = (GLenum)1;    // alpha explicitly set to 1 in luminance formats
      break;
    case eGL_LUMINANCE32F_ARB:
      internalFormat = eGL_R32F;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = (GLenum)1;    // alpha explicitly set to 1 in luminance formats
      break;
    case eGL_LUMINANCE32I_EXT:
      internalFormat = eGL_R32I;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = (GLenum)1;    // alpha explicitly set to 1 in luminance formats
      break;
    case eGL_LUMINANCE32UI_EXT:
      internalFormat = eGL_R32UI;
      if(dataFormatLum)
        dataFormat = eGL_RED;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = (GLenum)1;    // alpha explicitly set to 1 in luminance formats
      break;
    case eGL_LUMINANCE_ALPHA:
    case eGL_LUMINANCE8_ALPHA8_EXT:
      internalFormat = eGL_RG8;
      if(dataFormatLum)
        dataFormat = eGL_RG;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = eGL_GREEN;
      break;
    case eGL_SLUMINANCE8_ALPHA8_EXT:
      internalFormat = eGL_SRGB8_ALPHA8;
      if(dataFormatLum)
        dataFormat = eGL_RG;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = eGL_GREEN;
      break;
    case eGL_LUMINANCE16_ALPHA16_EXT:
      internalFormat = eGL_RG16;
      if(dataFormatLum)
        dataFormat = eGL_RG;
      swizzle[0] = swizzle[1] = swizzle[2] = eGL_RED;
      swizzle[3] = eGL_GREEN;
      break;
    default: return false;
  }

  if(tex)
  {
    if(HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle])
    {
      SetTextureSwizzle(gl, tex, target, swizzle);
    }
    else
    {
      RDCERR("Cannot emulate luminance format without texture swizzle extension");
    }
  }

  return true;
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
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: return true;
    default: break;
  }

  return false;
}

bool IsDepthStencilFormat(GLenum internalFormat)
{
  if(IsCompressedFormat(internalFormat))
    return false;

  GLenum fmt = GetBaseFormat(internalFormat);

  return (fmt == eGL_DEPTH_COMPONENT || fmt == eGL_STENCIL || fmt == eGL_DEPTH_STENCIL);
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

  RDCERR("Unexpected target %s", ToStr::Get(target).c_str());
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

  RDCERR("Unexpected target %s", ToStr::Get(target).c_str());
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

  RDCERR("Unexpected target %s", ToStr::Get(target).c_str());
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

GLenum TextureTarget(GLenum target)
{
  switch(target)
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

  return target;
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
