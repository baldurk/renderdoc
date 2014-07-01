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

size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type, int level, int align)
{
	size_t elemSize = 0;

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
			return w*h*d;
		case eGL_UNSIGNED_SHORT_5_6_5:
		case eGL_UNSIGNED_SHORT_5_6_5_REV:
		case eGL_UNSIGNED_SHORT_4_4_4_4:
		case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
		case eGL_UNSIGNED_SHORT_5_5_5_1:
		case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
			return w*h*d*2;
		case eGL_UNSIGNED_INT_8_8_8_8:
		case eGL_UNSIGNED_INT_8_8_8_8_REV:
		case eGL_UNSIGNED_INT_10_10_10_2:
		case eGL_UNSIGNED_INT_2_10_10_10_REV:
			return w*h*d*4;
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
			return w*h*d*elemSize;
		case eGL_RG:
		case eGL_RG_INTEGER:
			return w*h*d*elemSize*2;
		case eGL_RGB:
		case eGL_RGB_INTEGER:
		case eGL_BGR:
		case eGL_BGR_INTEGER:
			return w*h*d*elemSize*3;
		case eGL_RGBA:
		case eGL_RGBA_INTEGER:
		case eGL_BGRA:
		case eGL_BGRA_INTEGER:
			return w*h*d*elemSize*4;
		default:
			RDCERR("Unhandled Byte Size format %d!", format);
			break;
	}

	RDCERR("Unhandled Byte Size case!");

	return 0;
}
