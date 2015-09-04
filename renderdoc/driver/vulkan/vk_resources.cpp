/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include "vk_resources.h"

bool IsBlockFormat(VkFormat f)
{
	switch(f)
	{
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC4_SNORM:
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC5_SNORM:
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_BC6H_SFLOAT:
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11_SNORM:
		case VK_FORMAT_EAC_R11G11_UNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
		case VK_FORMAT_ASTC_4x4_UNORM:
		case VK_FORMAT_ASTC_4x4_SRGB:
		case VK_FORMAT_ASTC_5x4_UNORM:
		case VK_FORMAT_ASTC_5x4_SRGB:
		case VK_FORMAT_ASTC_5x5_UNORM:
		case VK_FORMAT_ASTC_5x5_SRGB:
		case VK_FORMAT_ASTC_6x5_UNORM:
		case VK_FORMAT_ASTC_6x5_SRGB:
		case VK_FORMAT_ASTC_6x6_UNORM:
		case VK_FORMAT_ASTC_6x6_SRGB:
		case VK_FORMAT_ASTC_8x5_UNORM:
		case VK_FORMAT_ASTC_8x5_SRGB:
		case VK_FORMAT_ASTC_8x6_UNORM:
		case VK_FORMAT_ASTC_8x6_SRGB:
		case VK_FORMAT_ASTC_8x8_UNORM:
		case VK_FORMAT_ASTC_8x8_SRGB:
		case VK_FORMAT_ASTC_10x5_UNORM:
		case VK_FORMAT_ASTC_10x5_SRGB:
		case VK_FORMAT_ASTC_10x6_UNORM:
		case VK_FORMAT_ASTC_10x6_SRGB:
		case VK_FORMAT_ASTC_10x8_UNORM:
		case VK_FORMAT_ASTC_10x8_SRGB:
		case VK_FORMAT_ASTC_10x10_UNORM:
		case VK_FORMAT_ASTC_10x10_SRGB:
		case VK_FORMAT_ASTC_12x10_UNORM:
		case VK_FORMAT_ASTC_12x10_SRGB:
		case VK_FORMAT_ASTC_12x12_UNORM:
		case VK_FORMAT_ASTC_12x12_SRGB:
			return true;
		default:
			break;
	}

	return false;
}

bool IsDepthStencilFormat(VkFormat f)
{
	switch(f)
	{
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D24_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			break;
	}

	return false;
}

ResourceFormat MakeResourceFormat(VkFormat fmt)
{
	ResourceFormat ret;

	ret.rawType = (uint32_t)fmt;
	ret.special = false;
	ret.specialFormat = eSpecial_Unknown;
	ret.strname = ToStr::Get(fmt).substr(10); // 3 == strlen("VK_FORMAT_")

	
	// VKTODOHIGH generate resource format
	ret.compByteWidth = 1;
	ret.compCount = 4;
	ret.compType = eCompType_UNorm;
	ret.srgbCorrected = false;
	return ret;
}
