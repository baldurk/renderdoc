/******************************************************************************
 * The MIT License (MIT)
 * 
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

size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type, int align)
{
	size_t elemSize = 0;

	GLsizei alignMask = ~0x0;
	GLsizei alignAdd = 0;
	switch(align)
	{
		default:
		case 1:
			break;
		case 2:
			alignMask = ~0x1;
			alignAdd = 1;
			break;
		case 4:
			alignMask = ~0x3;
			alignAdd = 3;
			break;
		case 8:
			alignMask = ~0x7;
			alignAdd = 7;
			break;
	}

	switch(type)
	{
		case eGL_UNSIGNED_BYTE:
		case eGL_BYTE:
			elemSize = 1;
			break;
		case eGL_UNSIGNED_SHORT:
		case eGL_SHORT:
		case eGL_HALF_FLOAT:
			elemSize = 2;
			break;
		case eGL_UNSIGNED_INT:
		case eGL_INT:
		case eGL_FLOAT:
			elemSize = 4;
			break;
		case eGL_UNSIGNED_BYTE_3_3_2:
		case eGL_UNSIGNED_BYTE_2_3_3_REV:
			return ((w + alignAdd) & alignMask)*h*d;
		case eGL_UNSIGNED_SHORT_5_6_5:
		case eGL_UNSIGNED_SHORT_5_6_5_REV:
		case eGL_UNSIGNED_SHORT_4_4_4_4:
		case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
		case eGL_UNSIGNED_SHORT_5_5_5_1:
		case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
			return ((w*2 + alignAdd) & alignMask)*h*d;
		case eGL_UNSIGNED_INT_8_8_8_8:
		case eGL_UNSIGNED_INT_8_8_8_8_REV:
		case eGL_UNSIGNED_INT_10_10_10_2:
		case eGL_UNSIGNED_INT_2_10_10_10_REV:
		case eGL_UNSIGNED_INT_10F_11F_11F_REV:
		case eGL_UNSIGNED_INT_5_9_9_9_REV:
			return ((w*4 + alignAdd) & alignMask)*h*d;
		case eGL_DEPTH_COMPONENT16:
			return ((w*2 + alignAdd) & alignMask)*h*d;
		case eGL_DEPTH_COMPONENT24:
		case eGL_DEPTH24_STENCIL8:
		case eGL_DEPTH_COMPONENT32:
		case eGL_DEPTH_COMPONENT32F:
		case eGL_UNSIGNED_INT_24_8:
			return ((w*4 + alignAdd) & alignMask)*h*d;
		case eGL_DEPTH32F_STENCIL8:
		case eGL_FLOAT_32_UNSIGNED_INT_24_8_REV:
			return ((w*5 + alignAdd) & alignMask)*h*d;
		default:
			RDCERR("Unhandled Byte Size type %d!", type);
			break;
	}

	switch(format)
	{
		case eGL_RED:
		case eGL_RED_INTEGER:
		case eGL_GREEN:
		case eGL_GREEN_INTEGER:
		case eGL_BLUE:
		case eGL_BLUE_INTEGER:
			return ((w*elemSize + alignAdd) & alignMask)*h*d;
		case eGL_RG:
		case eGL_RG_INTEGER:
			return ((w*elemSize*2 + alignAdd) & alignMask)*h*d;
		case eGL_RGB:
		case eGL_RGB_INTEGER:
		case eGL_BGR:
		case eGL_BGR_INTEGER:
			return ((w*elemSize*3 + alignAdd) & alignMask)*h*d;
		case eGL_RGBA:
		case eGL_RGBA_INTEGER:
		case eGL_BGRA:
		case eGL_BGRA_INTEGER:
			return ((w*elemSize*4 + alignAdd) & alignMask)*h*d;
		default:
			RDCERR("Unhandled Byte Size format %d!", format);
			break;
	}

	RDCERR("Unhandled Byte Size case!");

	return 0;
}

GLenum GetBaseFormat(GLenum internalFormat)
{
	switch(internalFormat)
	{
		case eGL_R8:
		case eGL_R8_SNORM:
		case eGL_R16:
		case eGL_R16_SNORM:
		case eGL_R16F:
		case eGL_R8I:
		case eGL_R8UI:
		case eGL_R16I:
		case eGL_R16UI:
		case eGL_R32I:
		case eGL_R32UI:
		case eGL_R32F:
			return eGL_RED;
		case eGL_RG8:
		case eGL_RG8_SNORM:
		case eGL_RG16:
		case eGL_RG16_SNORM:
		case eGL_RG16F:
		case eGL_RG32F:
		case eGL_RG8I:
		case eGL_RG8UI:
		case eGL_RG16I:
		case eGL_RG16UI:
		case eGL_RG32I:
		case eGL_RG32UI:
			return eGL_RG;
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
		case eGL_RGB8I:
		case eGL_RGB8UI:
		case eGL_RGB16I:
		case eGL_RGB16UI:
		case eGL_RGB32I:
		case eGL_RGB32UI:
			return eGL_RGB;
		case eGL_RGBA2:
		case eGL_RGBA4:
		case eGL_RGB5_A1:
		case eGL_RGBA8:
		case eGL_RGBA8_SNORM:
		case eGL_RGB10_A2:
		case eGL_RGB10_A2UI:
		case eGL_RGBA12:
		case eGL_RGBA16:
		case eGL_RGBA16_SNORM:
		case eGL_SRGB8_ALPHA8:
		case eGL_RGBA16F:
		case eGL_RGBA32F:
		case eGL_RGBA8I:
		case eGL_RGBA8UI:
		case eGL_RGBA16I:
		case eGL_RGBA16UI:
		case eGL_RGBA32UI:
		case eGL_RGBA32I:
			return eGL_RGBA;
		case eGL_DEPTH_COMPONENT16:
		case eGL_DEPTH_COMPONENT24:
		case eGL_DEPTH_COMPONENT32:
		case eGL_DEPTH_COMPONENT32F:
			return eGL_DEPTH_COMPONENT;
		case eGL_DEPTH24_STENCIL8:
		case eGL_DEPTH32F_STENCIL8:
			return eGL_DEPTH_STENCIL;
		case eGL_STENCIL_INDEX1:
		case eGL_STENCIL_INDEX4:
		case eGL_STENCIL_INDEX8:
		case eGL_STENCIL_INDEX16:
			return eGL_STENCIL;
		default:
			break;
	}

	RDCERR("Unhandled Base Format case!");

	return eGL_NONE;
}

GLenum GetDataType(GLenum internalFormat)
{
	switch(internalFormat)
	{
		case eGL_RGBA8UI:
		case eGL_RG8UI:
		case eGL_R8UI:
		case eGL_RGBA8:
		case eGL_RG8:
		case eGL_R8:
		case eGL_RGB8:
		case eGL_RGB8UI:
			return eGL_UNSIGNED_BYTE;
		case eGL_RGBA8I:
		case eGL_RG8I:
		case eGL_R8I:
		case eGL_RGBA8_SNORM:
		case eGL_RG8_SNORM:
		case eGL_R8_SNORM:
		case eGL_RGB8_SNORM:
		case eGL_SRGB8:
		case eGL_RGB8I:
		case eGL_SRGB8_ALPHA8:
			return eGL_BYTE;
		case eGL_RGBA16UI:
		case eGL_RG16UI:
		case eGL_R16UI:
		case eGL_RGBA16:
		case eGL_RG16:
		case eGL_R16:
		case eGL_RGB16:
		case eGL_RGB16UI:
		case eGL_DEPTH_COMPONENT16:
			return eGL_UNSIGNED_SHORT;
		case eGL_RGBA16I:
		case eGL_RG16I:
		case eGL_R16I:
		case eGL_RGBA16_SNORM:
		case eGL_RG16_SNORM:
		case eGL_R16_SNORM:
		case eGL_RGB16_SNORM:
		case eGL_RGB16I:
			return eGL_SHORT;
		case eGL_RGBA32UI:
		case eGL_RG32UI:
		case eGL_R32UI:
		case eGL_RGB32UI:
		case eGL_DEPTH_COMPONENT24:
		case eGL_DEPTH_COMPONENT32:
			return eGL_UNSIGNED_INT;
		case eGL_RGBA32I:
		case eGL_RG32I:
		case eGL_R32I:
		case eGL_RGB32I:
			return eGL_INT;
		case eGL_RGBA16F:
		case eGL_RG16F:
		case eGL_RGB16F:
		case eGL_R16F:
			return eGL_HALF_FLOAT;
		case eGL_RGBA32F:
		case eGL_RG32F:
		case eGL_R32F:
		case eGL_DEPTH_COMPONENT32F:
			return eGL_FLOAT;
		case eGL_R11F_G11F_B10F:
			return eGL_UNSIGNED_INT_10F_11F_11F_REV;
		case eGL_RGB10_A2UI:
			return eGL_INT_2_10_10_10_REV;
		case eGL_RGB10_A2:
			return eGL_UNSIGNED_INT_2_10_10_10_REV;
		case eGL_R3_G3_B2:
			return eGL_UNSIGNED_BYTE_3_3_2;
		case eGL_RGB4:
		case eGL_RGBA4:
			return eGL_UNSIGNED_SHORT_4_4_4_4;
		case eGL_RGB5:
		case eGL_RGB5_A1:
			return eGL_UNSIGNED_SHORT_5_5_5_1;
		case eGL_RGB565:
			return eGL_UNSIGNED_SHORT_5_6_5;
		case eGL_RGB10:
			return eGL_UNSIGNED_INT_10_10_10_2;
		case eGL_RGB9_E5:
			return eGL_UNSIGNED_INT_5_9_9_9_REV;
		case eGL_DEPTH24_STENCIL8:
			return eGL_UNSIGNED_INT_24_8;
		case eGL_DEPTH32F_STENCIL8:
			return eGL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		case eGL_STENCIL_INDEX8:
			return eGL_UNSIGNED_BYTE;
		default:
			break;
	}

	RDCERR("Unhandled Data Type case!");

	return eGL_NONE;
}

bool IsDepthStencilFormat(GLenum internalFormat)
{
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
		case eGL_RGB10_A2UI:
			return true;
		default:
			break;
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
		case eGL_RGBA32I:
			return true;
		default:
			break;
	}

	return false;
}

GLenum TextureBinding(GLenum target)
{
	switch(target)
	{
		case eGL_TEXTURE_1D:
			return eGL_TEXTURE_BINDING_1D;
		case eGL_TEXTURE_1D_ARRAY:
			return eGL_TEXTURE_BINDING_1D_ARRAY;
		case eGL_TEXTURE_2D:
			return eGL_TEXTURE_BINDING_2D;
		case eGL_TEXTURE_2D_ARRAY:
			return eGL_TEXTURE_BINDING_2D_ARRAY;
		case eGL_TEXTURE_2D_MULTISAMPLE:
			return eGL_TEXTURE_BINDING_2D_MULTISAMPLE;
		case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
			return eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
		case eGL_TEXTURE_RECTANGLE:
			return eGL_TEXTURE_BINDING_RECTANGLE;
		case eGL_TEXTURE_3D:
			return eGL_TEXTURE_BINDING_3D;
		case eGL_TEXTURE_CUBE_MAP:
		case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			return eGL_TEXTURE_BINDING_CUBE_MAP;
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			return eGL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
		case eGL_TEXTURE_BUFFER:
			return eGL_TEXTURE_BINDING_BUFFER;
	}

	RDCERR("Unexpected target %x", target);
	return eGL_NONE;
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
		case eGL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
			return true;
	}

	return false;
}
