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

#include "vk_common.h"
#include "vk_manager.h"
#include "vk_resources.h"

ResourceFormat MakeResourceFormat(VkFormat fmt)
{
	ResourceFormat ret;

	ret.rawType = (uint32_t)fmt;
	ret.special = false;
	ret.specialFormat = eSpecial_Unknown;
	ret.strname = ToStr::Get(fmt).substr(10); // 3 == strlen("VK_FORMAT_")
	ret.compByteWidth = 0;
	ret.compCount = 0;
	ret.compType = eCompType_None;
	ret.srgbCorrected = false;

	if(fmt == VK_FORMAT_UNDEFINED)
	{
		return ret;
	}

	// VKTODOMED R4G4, R4G4B4A4, and refactor BGRA formats

	switch(fmt)
	{
		case VK_FORMAT_B4G4R4A4_UNORM:
			ret.special = true;
			ret.specialFormat = eSpecial_B4G4R4A4;
			break;
		case VK_FORMAT_R10G10B10A2_UNORM:
		case VK_FORMAT_R10G10B10A2_SNORM:
		case VK_FORMAT_R10G10B10A2_USCALED:
		case VK_FORMAT_R10G10B10A2_SSCALED:
		case VK_FORMAT_R10G10B10A2_UINT:
		case VK_FORMAT_R10G10B10A2_SINT:
			ret.special = true;
			ret.specialFormat = eSpecial_R10G10B10A2;
			break;
		case VK_FORMAT_R11G11B10_UFLOAT:
			ret.special = true;
			ret.specialFormat = eSpecial_R11G11B10;
			break;
		case VK_FORMAT_R9G9B9E5_UFLOAT:
			ret.special = true;
			ret.specialFormat = eSpecial_R9G9B9E5;
			break;
		case VK_FORMAT_D16_UNORM_S8_UINT:
			ret.special = true;
			ret.specialFormat = eSpecial_D16S8;
			break;
		case VK_FORMAT_D24_UNORM_S8_UINT:
			ret.special = true;
			ret.specialFormat = eSpecial_D24S8;
			break;
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			ret.special = true;
			ret.specialFormat = eSpecial_D32S8;
			break;
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
			ret.special = true;
			ret.specialFormat = eSpecial_BC1;
			break;
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
			ret.special = true;
			ret.specialFormat = eSpecial_BC2;
			break;
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
			ret.special = true;
			ret.specialFormat = eSpecial_BC3;
			break;
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC4_SNORM:
			ret.special = true;
			ret.specialFormat = eSpecial_BC4;
			break;
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC5_SNORM:
			ret.special = true;
			ret.specialFormat = eSpecial_BC5;
			break;
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_BC6H_SFLOAT:
			ret.special = true;
			ret.specialFormat = eSpecial_BC6;
			break;
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
			ret.special = true;
			ret.specialFormat = eSpecial_BC7;
			break;
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
			ret.special = true;
			ret.specialFormat = eSpecial_ETC2;
			break;
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11_SNORM:
		case VK_FORMAT_EAC_R11G11_UNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
			ret.special = true;
			ret.specialFormat = eSpecial_EAC;
			break;
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
			ret.special = true;
			ret.specialFormat = eSpecial_ASTC;
			break;
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
			ret.special = true;
			ret.specialFormat = eSpecial_B8G8R8A8;
			break;
		default:
			break;
	}

	switch(fmt)
	{
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_SRGB:
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM_X8:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_S8_UINT:
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC4_SNORM:
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11_SNORM:
			ret.compCount = 1;
			break;
		case VK_FORMAT_R4G4_UNORM:
		case VK_FORMAT_R4G4_USCALED:
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_SRGB:
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC5_SNORM:
		case VK_FORMAT_EAC_R11G11_UNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
			ret.compCount = 2;
			break;
		case VK_FORMAT_R5G6B5_UNORM:
		case VK_FORMAT_R5G6B5_USCALED:
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R11G11B10_UFLOAT:
		case VK_FORMAT_R9G9B9E5_UFLOAT:
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_BC6H_SFLOAT:
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_B5G6R5_UNORM:
		case VK_FORMAT_B5G6R5_USCALED:
		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8_SRGB:
			ret.compCount = 3;
			break;
		case VK_FORMAT_R4G4B4A4_UNORM:
		case VK_FORMAT_R4G4B4A4_USCALED:
		case VK_FORMAT_R5G5B5A1_UNORM:
		case VK_FORMAT_R5G5B5A1_USCALED:
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R10G10B10A2_UNORM:
		case VK_FORMAT_R10G10B10A2_SNORM:
		case VK_FORMAT_R10G10B10A2_USCALED:
		case VK_FORMAT_R10G10B10A2_SSCALED:
		case VK_FORMAT_R10G10B10A2_UINT:
		case VK_FORMAT_R10G10B10A2_SINT:
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_B4G4R4A4_UNORM:
		case VK_FORMAT_B5G5R5A1_UNORM:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B10G10R10A2_UNORM:
		case VK_FORMAT_B10G10R10A2_SNORM:
		case VK_FORMAT_B10G10R10A2_USCALED:
		case VK_FORMAT_B10G10R10A2_SSCALED:
		case VK_FORMAT_B10G10R10A2_UINT:
		case VK_FORMAT_B10G10R10A2_SINT:
			ret.compCount = 4;
			break;
		default:
			break;
	}

	switch(fmt)
	{
		case VK_FORMAT_R8_SRGB:
		case VK_FORMAT_R8G8_SRGB:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_ASTC_4x4_SRGB:
		case VK_FORMAT_ASTC_5x4_SRGB:
		case VK_FORMAT_ASTC_5x5_SRGB:
		case VK_FORMAT_ASTC_6x5_SRGB:
		case VK_FORMAT_ASTC_6x6_SRGB:
		case VK_FORMAT_ASTC_8x5_SRGB:
		case VK_FORMAT_ASTC_8x6_SRGB:
		case VK_FORMAT_ASTC_8x8_SRGB:
		case VK_FORMAT_ASTC_10x5_SRGB:
		case VK_FORMAT_ASTC_10x6_SRGB:
		case VK_FORMAT_ASTC_10x8_SRGB:
		case VK_FORMAT_ASTC_10x10_SRGB:
		case VK_FORMAT_ASTC_12x10_SRGB:
		case VK_FORMAT_ASTC_12x12_SRGB:
		case VK_FORMAT_B8G8R8_SRGB:
		case VK_FORMAT_B8G8R8A8_SRGB:
			ret.srgbCorrected = true;
			break;
		default:
			break;
	}

	switch(fmt)
	{
		case VK_FORMAT_R4G4_UNORM:
		case VK_FORMAT_R4G4B4A4_UNORM:
		case VK_FORMAT_R5G6B5_UNORM:
		case VK_FORMAT_R5G5B5A1_UNORM:
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SRGB:
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SRGB:
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_R10G10B10A2_UNORM:
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11G11_UNORM:
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
		case VK_FORMAT_B4G4R4A4_UNORM:
		case VK_FORMAT_B5G5R5A1_UNORM:
		case VK_FORMAT_B5G6R5_UNORM:
		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B10G10R10A2_UNORM:
			ret.compType = eCompType_UNorm;
			break;
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R10G10B10A2_SNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_BC4_SNORM:
		case VK_FORMAT_BC5_SNORM:
		case VK_FORMAT_BC6H_SFLOAT:
		case VK_FORMAT_EAC_R11_SNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B10G10R10A2_SNORM:
			ret.compType = eCompType_SNorm;
			break;
		case VK_FORMAT_R4G4_USCALED:
		case VK_FORMAT_R4G4B4A4_USCALED:
		case VK_FORMAT_R5G6B5_USCALED:
		case VK_FORMAT_R5G5B5A1_USCALED:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R10G10B10A2_USCALED:
		case VK_FORMAT_B5G6R5_USCALED:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B10G10R10A2_USCALED:
			ret.compType = eCompType_UScaled;
			break;
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R10G10B10A2_SSCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B10G10R10A2_SSCALED:
			ret.compType = eCompType_SScaled;
			break;
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R10G10B10A2_UINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			// Maybe S8 should be identified by something else?
		case VK_FORMAT_S8_UINT:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B10G10R10A2_UINT:
			ret.compType = eCompType_UInt;
			break;
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R10G10B10A2_SINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B10G10R10A2_SINT:
			ret.compType = eCompType_SInt;
			break;
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R11G11B10_UFLOAT:
		case VK_FORMAT_R9G9B9E5_UFLOAT:
			ret.compType = eCompType_Float;
			break;
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			ret.compType = eCompType_Double;
			break;
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM_X8:
		case VK_FORMAT_D32_SFLOAT:
			ret.compType = eCompType_Depth;
			break;
		default:
			break;
	}

	switch(fmt)
	{
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_SRGB:
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_SRGB:
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_S8_UINT:
		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
			ret.compByteWidth = 1;
			break;
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_D16_UNORM:
			ret.compByteWidth = 2;
			break;
		case VK_FORMAT_D24_UNORM_X8:
			ret.compByteWidth = 3;
			break;
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT:
			ret.compByteWidth = 4;
			break;
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			ret.compByteWidth = 8;
			break;
		default:
			break;
	}

	return ret;
}

PrimitiveTopology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints)
{
	switch(Topo)
	{
		default:
			break;
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			return eTopology_PointList;
			break;
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
			return eTopology_LineList;
			break;
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			return eTopology_LineStrip;
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			return eTopology_TriangleList;
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			return eTopology_TriangleStrip;
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			return eTopology_TriangleFan;
			break;
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_ADJ:
			return eTopology_LineList_Adj;
			break;
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_ADJ:
			return eTopology_LineStrip_Adj;
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_ADJ:
			return eTopology_TriangleList_Adj;
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_ADJ:
			return eTopology_TriangleStrip_Adj;
			break;
		case VK_PRIMITIVE_TOPOLOGY_PATCH:
			return PrimitiveTopology(eTopology_PatchList_1CPs + patchControlPoints);
			break;
	}

	return eTopology_Unknown;
}

template<>
string ToStrHelper<false, VkResourceType>::Get(const VkResourceType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(eResUnknown)
		TOSTR_CASE_STRINGIZE(eResPhysicalDevice)
		TOSTR_CASE_STRINGIZE(eResInstance)
		TOSTR_CASE_STRINGIZE(eResDevice)
		TOSTR_CASE_STRINGIZE(eResQueue)
		TOSTR_CASE_STRINGIZE(eResDeviceMemory)
		TOSTR_CASE_STRINGIZE(eResBuffer)
		TOSTR_CASE_STRINGIZE(eResBufferView)
		TOSTR_CASE_STRINGIZE(eResImage)
		TOSTR_CASE_STRINGIZE(eResImageView)
		TOSTR_CASE_STRINGIZE(eResFramebuffer)
		TOSTR_CASE_STRINGIZE(eResRenderPass)
		TOSTR_CASE_STRINGIZE(eResShaderModule)
		TOSTR_CASE_STRINGIZE(eResShader)
		TOSTR_CASE_STRINGIZE(eResPipelineCache)
		TOSTR_CASE_STRINGIZE(eResPipelineLayout)
		TOSTR_CASE_STRINGIZE(eResPipeline)
		TOSTR_CASE_STRINGIZE(eResSampler)
		TOSTR_CASE_STRINGIZE(eResDescriptorPool)
		TOSTR_CASE_STRINGIZE(eResDescriptorSetLayout)
		TOSTR_CASE_STRINGIZE(eResDescriptorSet)
		TOSTR_CASE_STRINGIZE(eResCmdPool)
		TOSTR_CASE_STRINGIZE(eResCmdBuffer)
		TOSTR_CASE_STRINGIZE(eResFence)
		TOSTR_CASE_STRINGIZE(eResEvent)
		TOSTR_CASE_STRINGIZE(eResQueryPool)
		TOSTR_CASE_STRINGIZE(eResSemaphore)
		TOSTR_CASE_STRINGIZE(eResSwapchain)
		default: break;
	}
	
	return StringFormat::Fmt("VkResourceType<%d>", el);
}

template<>
string ToStrHelper<false, VkQueueFlagBits>::Get(const VkQueueFlagBits &el)
{
	string ret;

	if(el & VK_QUEUE_GRAPHICS_BIT)      ret += " | VK_QUEUE_GRAPHICS_BIT";
	if(el & VK_QUEUE_COMPUTE_BIT)       ret += " | VK_QUEUE_COMPUTE_BIT";
	if(el & VK_QUEUE_DMA_BIT)           ret += " | VK_QUEUE_DMA_BIT";
	if(el & VK_QUEUE_SPARSE_MEMMGR_BIT) ret += " | VK_QUEUE_SPARSE_MEMMGR_BIT";
	if(el & VK_QUEUE_EXTENDED_BIT)      ret += " | VK_QUEUE_EXTENDED_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkPipelineCreateFlagBits>::Get(const VkPipelineCreateFlagBits &el)
{
	string ret;

	if(el & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT) ret += " | VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT";
	if(el & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT)    ret += " | VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT";
	if(el & VK_PIPELINE_CREATE_DERIVATIVE_BIT)           ret += " | VK_PIPELINE_CREATE_DERIVATIVE_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkPipelineStageFlagBits>::Get(const VkPipelineStageFlagBits &el)
{
	string ret;

	if(el == VK_PIPELINE_STAGE_ALL_GRAPHICS)
		return "VK_PIPELINE_STAGE_ALL_GRAPHICS";

	if(el == VK_PIPELINE_STAGE_ALL_GPU_COMMANDS)
		return "VK_PIPELINE_STAGE_ALL_GPU_COMMANDS";

	if(el & VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)                 ret += " | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT";
	if(el & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT)               ret += " | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT";
	if(el & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)                ret += " | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT";
	if(el & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT)               ret += " | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT";
	if(el & VK_PIPELINE_STAGE_TESS_CONTROL_SHADER_BIT)         ret += " | VK_PIPELINE_STAGE_TESS_CONTROL_SHADER_BIT";
	if(el & VK_PIPELINE_STAGE_TESS_EVALUATION_SHADER_BIT)      ret += " | VK_PIPELINE_STAGE_TESS_EVALUATION_SHADER_BIT";
	if(el & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)             ret += " | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT";
	if(el & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)             ret += " | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT";
	if(el & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)        ret += " | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT";
	if(el & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)         ret += " | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT";
	if(el & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)     ret += " | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT";
	if(el & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)              ret += " | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT";
	if(el & VK_PIPELINE_STAGE_TRANSFER_BIT)                    ret += " | VK_PIPELINE_STAGE_TRANSFER_BIT";
	if(el & VK_PIPELINE_STAGE_HOST_BIT)                        ret += " | VK_PIPELINE_STAGE_HOST_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkBufferUsageFlagBits>::Get(const VkBufferUsageFlagBits &el)
{
	string ret = "";

	if(el & VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT)       ret += " | VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT";
	if(el & VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT)  ret += " | VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT";
	if(el & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)  ret += " | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT";
	if(el & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)  ret += " | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT";
	if(el & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)        ret += " | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT";
	if(el & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)        ret += " | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT";
	if(el & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)          ret += " | VK_BUFFER_USAGE_INDEX_BUFFER_BIT";
	if(el & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)         ret += " | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT";
	if(el & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)       ret += " | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkImageUsageFlagBits>::Get(const VkImageUsageFlagBits &el)
{
	string ret = "";

	if(el & VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT)          ret += " | VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT";
	if(el & VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT)     ret += " | VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT";
	if(el & VK_IMAGE_USAGE_SAMPLED_BIT)                  ret += " | VK_IMAGE_USAGE_SAMPLED_BIT";
	if(el & VK_IMAGE_USAGE_STORAGE_BIT)                  ret += " | VK_IMAGE_USAGE_STORAGE_BIT";
	if(el & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)         ret += " | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT";
	if(el & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ret += " | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT";
	if(el & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)     ret += " | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT";
	if(el & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)         ret += " | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkBufferCreateFlagBits>::Get(const VkBufferCreateFlagBits &el)
{
	string ret;

	if(el & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)   ret += " | VK_BUFFER_CREATE_SPARSE_BINDING_BIT";
	if(el & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) ret += " | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT";
	if(el & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)   ret += " | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkImageCreateFlagBits>::Get(const VkImageCreateFlagBits &el)
{
	string ret;

	if(el & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)   ret += " | VK_IMAGE_CREATE_SPARSE_BINDING_BIT";
	if(el & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) ret += " | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT";
	if(el & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)   ret += " | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT";
	if(el & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)   ret += " | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT";
	if(el & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)  ret += " | VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkImageViewCreateFlagBits>::Get(const VkImageViewCreateFlagBits &el)
{
	string ret;

	if(el & VK_IMAGE_VIEW_CREATE_READ_ONLY_DEPTH_BIT)   ret += " | VK_IMAGE_VIEW_CREATE_READ_ONLY_DEPTH_BIT";
	if(el & VK_IMAGE_VIEW_CREATE_READ_ONLY_STENCIL_BIT) ret += " | VK_IMAGE_VIEW_CREATE_READ_ONLY_STENCIL_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkCmdPoolCreateFlagBits>::Get(const VkCmdPoolCreateFlagBits &el)
{
	string ret;

	if(el & VK_CMD_POOL_CREATE_TRANSIENT_BIT)            ret += " | VK_CMD_POOL_CREATE_TRANSIENT_BIT";
	if(el & VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) ret += " | VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkCmdPoolResetFlagBits>::Get(const VkCmdPoolResetFlagBits &el)
{
	string ret;

	if(el & VK_CMD_POOL_RESET_RELEASE_RESOURCES_BIT)          ret += " | VK_CMD_POOL_RESET_RELEASE_RESOURCES_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkCmdBufferOptimizeFlagBits>::Get(const VkCmdBufferOptimizeFlagBits &el)
{
	string ret;

	if(el & VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT)           ret += " | VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT";
	if(el & VK_CMD_BUFFER_OPTIMIZE_PIPELINE_SWITCH_BIT)       ret += " | VK_CMD_BUFFER_OPTIMIZE_PIPELINE_SWITCH_BIT";
	if(el & VK_CMD_BUFFER_OPTIMIZE_PIPELINE_SWITCH_BIT)       ret += " | VK_CMD_BUFFER_OPTIMIZE_PIPELINE_SWITCH_BIT";
	if(el & VK_CMD_BUFFER_OPTIMIZE_DESCRIPTOR_SET_SWITCH_BIT) ret += " | VK_CMD_BUFFER_OPTIMIZE_DESCRIPTOR_SET_SWITCH_BIT";
	if(el & VK_CMD_BUFFER_OPTIMIZE_NO_SIMULTANEOUS_USE_BIT)   ret += " | VK_CMD_BUFFER_OPTIMIZE_NO_SIMULTANEOUS_USE_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkFenceCreateFlagBits>::Get(const VkFenceCreateFlagBits &el)
{
	string ret;

	if(el & VK_FENCE_CREATE_SIGNALED_BIT) ret += " | VK_FENCE_CREATE_SIGNALED_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkQueryPipelineStatisticFlagBits>::Get(const VkQueryPipelineStatisticFlagBits &el)
{
	string ret;

	if(el & VK_QUERY_PIPELINE_STATISTIC_IA_VERTICES_BIT)     ret += " | VK_QUERY_PIPELINE_STATISTIC_IA_VERTICES_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_IA_PRIMITIVES_BIT)   ret += " | VK_QUERY_PIPELINE_STATISTIC_IA_PRIMITIVES_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_VS_INVOCATIONS_BIT)  ret += " | VK_QUERY_PIPELINE_STATISTIC_VS_INVOCATIONS_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_GS_INVOCATIONS_BIT)  ret += " | VK_QUERY_PIPELINE_STATISTIC_GS_INVOCATIONS_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_GS_PRIMITIVES_BIT)   ret += " | VK_QUERY_PIPELINE_STATISTIC_GS_PRIMITIVES_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_C_INVOCATIONS_BIT)   ret += " | VK_QUERY_PIPELINE_STATISTIC_C_INVOCATIONS_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_C_PRIMITIVES_BIT)    ret += " | VK_QUERY_PIPELINE_STATISTIC_C_PRIMITIVES_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_FS_INVOCATIONS_BIT)  ret += " | VK_QUERY_PIPELINE_STATISTIC_FS_INVOCATIONS_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_TCS_PATCHES_BIT)     ret += " | VK_QUERY_PIPELINE_STATISTIC_TCS_PATCHES_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_TES_INVOCATIONS_BIT) ret += " | VK_QUERY_PIPELINE_STATISTIC_TES_INVOCATIONS_BIT";
	if(el & VK_QUERY_PIPELINE_STATISTIC_CS_INVOCATIONS_BIT)  ret += " | VK_QUERY_PIPELINE_STATISTIC_CS_INVOCATIONS_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkQueryControlFlagBits>::Get(const VkQueryControlFlagBits &el)
{
	string ret;

	if(el & VK_QUERY_CONTROL_CONSERVATIVE_BIT) ret += " | VK_QUERY_CONTROL_CONSERVATIVE_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkQueryResultFlagBits>::Get(const VkQueryResultFlagBits &el)
{
	string ret;

	if(el == VK_QUERY_RESULT_DEFAULT)
		return "VK_QUERY_RESULT_DEFAULT";

	if(el & VK_QUERY_RESULT_64_BIT)                    ret += " | VK_QUERY_RESULT_64_BIT";
	if(el & VK_QUERY_RESULT_WAIT_BIT)                  ret += " | VK_QUERY_RESULT_WAIT_BIT";
	if(el & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)     ret += " | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT";
	if(el & VK_QUERY_RESULT_PARTIAL_BIT)               ret += " | VK_QUERY_RESULT_PARTIAL_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkShaderStageFlagBits>::Get(const VkShaderStageFlagBits &el)
{
	string ret;

	if(el == VK_SHADER_STAGE_ALL)
		return "VK_SHADER_STAGE_ALL";

	if(el & VK_SHADER_STAGE_VERTEX_BIT)              ret += " | VK_SHADER_STAGE_VERTEX_BIT";
	if(el & VK_SHADER_STAGE_TESS_CONTROL_BIT)        ret += " | VK_SHADER_STAGE_TESS_CONTROL_BIT";
	if(el & VK_SHADER_STAGE_TESS_EVALUATION_BIT)     ret += " | VK_SHADER_STAGE_TESS_EVALUATION_BIT";
	if(el & VK_SHADER_STAGE_GEOMETRY_BIT)            ret += " | VK_SHADER_STAGE_GEOMETRY_BIT";
	if(el & VK_SHADER_STAGE_FRAGMENT_BIT)            ret += " | VK_SHADER_STAGE_FRAGMENT_BIT";
	if(el & VK_SHADER_STAGE_COMPUTE_BIT)             ret += " | VK_SHADER_STAGE_COMPUTE_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkSubpassDescriptionFlagBits>::Get(const VkSubpassDescriptionFlagBits &el)
{
	string ret;

	if(el & VK_SUBPASS_DESCRIPTION_NO_OVERDRAW_BIT)              ret += " | VK_SUBPASS_DESCRIPTION_NO_OVERDRAW_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkAttachmentDescriptionFlagBits>::Get(const VkAttachmentDescriptionFlagBits &el)
{
	string ret;

	if(el & VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT)  ret += " | VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkImageAspectFlagBits>::Get(const VkImageAspectFlagBits &el)
{
	string ret;

	if(el & VK_IMAGE_ASPECT_COLOR_BIT)    ret += " | VK_IMAGE_ASPECT_COLOR_BIT";
	if(el & VK_IMAGE_ASPECT_DEPTH_BIT)    ret += " | VK_IMAGE_ASPECT_DEPTH_BIT";
	if(el & VK_IMAGE_ASPECT_STENCIL_BIT)  ret += " | VK_IMAGE_ASPECT_STENCIL_BIT";
	if(el & VK_IMAGE_ASPECT_METADATA_BIT) ret += " | VK_IMAGE_ASPECT_METADATA_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkStencilFaceFlagBits>::Get(const VkStencilFaceFlagBits &el)
{
	string ret;

	if(el & VK_STENCIL_FACE_FRONT_BIT)    ret += " | VK_STENCIL_FACE_FRONT_BIT";
	if(el & VK_STENCIL_FACE_BACK_BIT)     ret += " | VK_STENCIL_FACE_BACK_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);
	if(ret.empty())
		ret = "VK_STENCIL_FACE_NONE";

	return ret;
}

template<>
string ToStrHelper<false, VkPipelineBindPoint>::Get(const VkPipelineBindPoint &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_PIPELINE_BIND_POINT_COMPUTE)
		TOSTR_CASE_STRINGIZE(VK_PIPELINE_BIND_POINT_GRAPHICS)
		default: break;
	}
	
	return StringFormat::Fmt("VkPipelineBindPoint<%d>", el);
}

template<>
string ToStrHelper<false, VkIndexType>::Get(const VkIndexType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_INDEX_TYPE_UINT16)
		TOSTR_CASE_STRINGIZE(VK_INDEX_TYPE_UINT32)
		default: break;
	}
	
	return StringFormat::Fmt("VkIndexType<%d>", el);
}

template<>
string ToStrHelper<false, VkTimestampType>::Get(const VkTimestampType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_TIMESTAMP_TYPE_TOP)
		TOSTR_CASE_STRINGIZE(VK_TIMESTAMP_TYPE_BOTTOM)
		default: break;
	}
	
	return StringFormat::Fmt("VkTimestampType<%d>", el);
}

template<>
string ToStrHelper<false, VkImageType>::Get(const VkImageType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_IMAGE_TYPE_1D)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_TYPE_2D)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_TYPE_3D)
		default: break;
	}
	
	return StringFormat::Fmt("VkImageType<%d>", el);
}

template<>
string ToStrHelper<false, VkImageTiling>::Get(const VkImageTiling &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_IMAGE_TILING_LINEAR)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_TILING_OPTIMAL)
		default: break;
	}
	
	return StringFormat::Fmt("VkImageTiling<%d>", el);
}

template<>
string ToStrHelper<false, VkImageViewType>::Get(const VkImageViewType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_1D)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_2D)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_3D)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_CUBE)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_1D_ARRAY)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		default: break;
	}
	
	return StringFormat::Fmt("VkImageViewType<%d>", el);
}

template<>
string ToStrHelper<false, VkVertexInputStepRate>::Get(const VkVertexInputStepRate &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_VERTEX_INPUT_STEP_RATE_VERTEX)
		TOSTR_CASE_STRINGIZE(VK_VERTEX_INPUT_STEP_RATE_INSTANCE)
		default: break;
	}
	
	return StringFormat::Fmt("VkVertexInputStepRate<%d>", el);
}

template<>
string ToStrHelper<false, VkFillMode>::Get(const VkFillMode &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_FILL_MODE_POINTS)
		TOSTR_CASE_STRINGIZE(VK_FILL_MODE_WIREFRAME)
		TOSTR_CASE_STRINGIZE(VK_FILL_MODE_SOLID)
		default: break;
	}
	
	return StringFormat::Fmt("VkFillMode<%d>", el);
}

template<>
string ToStrHelper<false, VkCullMode>::Get(const VkCullMode &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_CULL_MODE_NONE)
		TOSTR_CASE_STRINGIZE(VK_CULL_MODE_FRONT)
		TOSTR_CASE_STRINGIZE(VK_CULL_MODE_BACK)
		TOSTR_CASE_STRINGIZE(VK_CULL_MODE_FRONT_AND_BACK)
		default: break;
	}
	
	return StringFormat::Fmt("VkCullMode<%d>", el);
}

template<>
string ToStrHelper<false, VkFrontFace>::Get(const VkFrontFace &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_FRONT_FACE_CCW)
		TOSTR_CASE_STRINGIZE(VK_FRONT_FACE_CW)
		default: break;
	}
	
	return StringFormat::Fmt("VkFrontFace<%d>", el);
}

template<>
string ToStrHelper<false, VkBlend>::Get(const VkBlend &el)
{
	switch(el)
	{
		case VK_BLEND_ZERO:                       return "ZERO";
		case VK_BLEND_ONE:                        return "ONE";
		case VK_BLEND_SRC_COLOR:                  return "SRC_COLOR";
		case VK_BLEND_ONE_MINUS_SRC_COLOR:        return "INV_SRC_COLOR";
		case VK_BLEND_DEST_COLOR:                 return "DEST_COLOR";
		case VK_BLEND_ONE_MINUS_DEST_COLOR:       return "INV_DEST_COLOR";
		case VK_BLEND_SRC_ALPHA:                  return "SRC_ALPHA";
		case VK_BLEND_ONE_MINUS_SRC_ALPHA:        return "INV_SRC_ALPHA";
		case VK_BLEND_DEST_ALPHA:                 return "DEST_ALPHA";
		case VK_BLEND_ONE_MINUS_DEST_ALPHA:       return "INV_DEST_ALPHA";
		case VK_BLEND_CONSTANT_COLOR:             return "CONST_COLOR";
		case VK_BLEND_ONE_MINUS_CONSTANT_COLOR:   return "INV_CONST_COLOR";
		case VK_BLEND_CONSTANT_ALPHA:             return "CONST_ALPHA";
		case VK_BLEND_ONE_MINUS_CONSTANT_ALPHA:   return "INV_CONST_ALPHA";
		case VK_BLEND_SRC_ALPHA_SATURATE:         return "SRC_ALPHA_SAT";
		case VK_BLEND_SRC1_COLOR:                 return "SRC1_COLOR";
		case VK_BLEND_ONE_MINUS_SRC1_COLOR:       return "INV_SRC1_COLOR";
		case VK_BLEND_SRC1_ALPHA:                 return "SRC1_ALPHA";
		case VK_BLEND_ONE_MINUS_SRC1_ALPHA:       return "INV_SRC1_ALPHA";
		default: break;
	}
	
	return StringFormat::Fmt("VK_BLEND<%d>", el);
}

template<>
string ToStrHelper<false, VkDynamicState>::Get(const VkDynamicState &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_VIEWPORT)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_SCISSOR)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_LINE_WIDTH)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_DEPTH_BIAS)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_BLEND_CONSTANTS)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_DEPTH_BOUNDS)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)
		TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_STENCIL_REFERENCE)
		default: break;
	}
	
	return StringFormat::Fmt("VkDynamicState<%d>", el);
}


template<>
string ToStrHelper<false, VkAttachmentLoadOp>::Get(const VkAttachmentLoadOp &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_ATTACHMENT_LOAD_OP_LOAD)
		TOSTR_CASE_STRINGIZE(VK_ATTACHMENT_LOAD_OP_CLEAR)
		TOSTR_CASE_STRINGIZE(VK_ATTACHMENT_LOAD_OP_DONT_CARE)
		default: break;
	}
	
	return StringFormat::Fmt("VkAttachmentLoadOp<%d>", el);
}

template<>
string ToStrHelper<false, VkAttachmentStoreOp>::Get(const VkAttachmentStoreOp &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_ATTACHMENT_STORE_OP_STORE)
		TOSTR_CASE_STRINGIZE(VK_ATTACHMENT_STORE_OP_DONT_CARE)
		default: break;
	}
	
	return StringFormat::Fmt("VkAttachmentLoadOp<%d>", el);
}

template<>
string ToStrHelper<false, VkBlendOp>::Get(const VkBlendOp &el)
{
	switch(el)
	{
		case VK_BLEND_OP_ADD:                 return "ADD";
		case VK_BLEND_OP_SUBTRACT:            return "SUB";
		case VK_BLEND_OP_REVERSE_SUBTRACT:    return "REV_SUB";
		case VK_BLEND_OP_MIN:                 return "MIN";
		case VK_BLEND_OP_MAX:                 return "MAX";
		default: break;
	}
	
	return StringFormat::Fmt("VkBlendOp<%d>", el);
}

template<>
string ToStrHelper<false, VkStencilOp>::Get(const VkStencilOp &el)
{
	switch(el)
	{
		case VK_STENCIL_OP_KEEP:           return "KEEP";
		case VK_STENCIL_OP_ZERO:           return "ZERO";
		case VK_STENCIL_OP_REPLACE:        return "REPLACE";
		case VK_STENCIL_OP_INC_CLAMP:      return "INC_SAT";
		case VK_STENCIL_OP_DEC_CLAMP:      return "DEC_SAT";
		case VK_STENCIL_OP_INVERT:         return "INVERT";
		case VK_STENCIL_OP_INC_WRAP:       return "INC_WRAP";
		case VK_STENCIL_OP_DEC_WRAP:       return "DEC_WRAP";
		default: break;
	}
	
	return StringFormat::Fmt("VkStencilOp<%d>", el);
}

template<>
string ToStrHelper<false, VkLogicOp>::Get(const VkLogicOp &el)
{
	switch(el)
	{
		case VK_LOGIC_OP_CLEAR:          return "CLEAR";
		case VK_LOGIC_OP_AND:            return "AND";
		case VK_LOGIC_OP_AND_REVERSE:    return "AND_REV";
		case VK_LOGIC_OP_COPY:           return "COPY";
		case VK_LOGIC_OP_AND_INVERTED:   return "AND_INV";
		case VK_LOGIC_OP_NOOP:           return "NOOP";
		case VK_LOGIC_OP_XOR:            return "XOR";
		case VK_LOGIC_OP_OR:             return "OR";
		case VK_LOGIC_OP_NOR:            return "NOR";
		case VK_LOGIC_OP_EQUIV:          return "EQUIV";
		case VK_LOGIC_OP_INVERT:         return "INVERT";
		case VK_LOGIC_OP_OR_REVERSE:     return "OR_REV";
		case VK_LOGIC_OP_COPY_INVERTED:  return "COPY_INV";
		case VK_LOGIC_OP_OR_INVERTED:    return "OR_INV";
		case VK_LOGIC_OP_NAND:           return "NAND";
		case VK_LOGIC_OP_SET:            return "SET";
		default: break;
	}
	
	return StringFormat::Fmt("VkLogicOp<%d>", el);
}

template<>
string ToStrHelper<false, VkCompareOp>::Get(const VkCompareOp &el)
{
	switch(el)
	{
		case VK_COMPARE_OP_NEVER:           return "NEVER";
		case VK_COMPARE_OP_LESS:            return "LESS";
		case VK_COMPARE_OP_EQUAL:           return "EQUAL";
		case VK_COMPARE_OP_LESS_EQUAL:      return "LESS_EQUAL";
		case VK_COMPARE_OP_GREATER:         return "GREATER";
		case VK_COMPARE_OP_NOT_EQUAL:       return "NOT_EQUAL";
		case VK_COMPARE_OP_GREATER_EQUAL:   return "GREATER_EQUAL";
		case VK_COMPARE_OP_ALWAYS:          return "ALWAYS";
		default: break;
	}
	
	return StringFormat::Fmt("VkCompareOp<%d>", el);
}

template<>
string ToStrHelper<false, VkTexFilter>::Get(const VkTexFilter &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_TEX_FILTER_NEAREST)
		TOSTR_CASE_STRINGIZE(VK_TEX_FILTER_LINEAR)
		default: break;		 
	}
	
	return StringFormat::Fmt("VkTexFilter<%d>", el);
}

template<>
string ToStrHelper<false, VkTexMipmapMode>::Get(const VkTexMipmapMode &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_TEX_MIPMAP_MODE_BASE)
		TOSTR_CASE_STRINGIZE(VK_TEX_MIPMAP_MODE_NEAREST)
		TOSTR_CASE_STRINGIZE(VK_TEX_MIPMAP_MODE_LINEAR)
		default: break;		 
	}
	
	return StringFormat::Fmt("VkTexMipmapMode<%d>", el);
}

template<>
string ToStrHelper<false, VkTexAddressMode>::Get(const VkTexAddressMode &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_TEX_ADDRESS_MODE_WRAP)
		TOSTR_CASE_STRINGIZE(VK_TEX_ADDRESS_MODE_MIRROR)
		TOSTR_CASE_STRINGIZE(VK_TEX_ADDRESS_MODE_CLAMP)
		TOSTR_CASE_STRINGIZE(VK_TEX_ADDRESS_MODE_MIRROR_ONCE)
		TOSTR_CASE_STRINGIZE(VK_TEX_ADDRESS_MODE_CLAMP_BORDER)
		default: break;		 
	}
	
	return StringFormat::Fmt("VkTexAddressMode<%d>", el);
}

template<>
string ToStrHelper<false, VkBorderColor>::Get(const VkBorderColor &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
		TOSTR_CASE_STRINGIZE(VK_BORDER_COLOR_INT_TRANSPARENT_BLACK)
		TOSTR_CASE_STRINGIZE(VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK)
		TOSTR_CASE_STRINGIZE(VK_BORDER_COLOR_INT_OPAQUE_BLACK)
		TOSTR_CASE_STRINGIZE(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
		TOSTR_CASE_STRINGIZE(VK_BORDER_COLOR_INT_OPAQUE_WHITE)
		default: break;		 
	}
	
	return StringFormat::Fmt("VkBorderColor<%d>", el);
}

template<>
string ToStrHelper<false, VkImageAspect>::Get(const VkImageAspect &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_IMAGE_ASPECT_COLOR)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_ASPECT_DEPTH)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_ASPECT_STENCIL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_ASPECT_METADATA)
		default: break;		 
	}
	
	return StringFormat::Fmt("VkImageAspect<%d>", el);
}

template<>
string ToStrHelper<false, VkPrimitiveTopology>::Get(const VkPrimitiveTopology &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_LIST_ADJ)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_ADJ)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_ADJ)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_ADJ)
		TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_PATCH)
		default: break;
	}
	
	return StringFormat::Fmt("VkPrimitiveTopology<%d>", el);
}

template<>
string ToStrHelper<false, VkDescriptorType>::Get(const VkDescriptorType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_SAMPLER)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
		default: break;
	}
	
	return StringFormat::Fmt("VkDescriptorType<%d>", el);
}

template<>
string ToStrHelper<false, VkDescriptorPoolUsage>::Get(const VkDescriptorPoolUsage &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_POOL_USAGE_DYNAMIC)
		default: break;
	}
	
	return StringFormat::Fmt("VkDescriptorPoolUsage<%d>", el);
}

template<>
string ToStrHelper<false, VkDescriptorSetUsage>::Get(const VkDescriptorSetUsage &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_SET_USAGE_ONE_SHOT)
		TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_SET_USAGE_STATIC)
		default: break;
	}
	
	return StringFormat::Fmt("VkDescriptorSetUsage<%d>", el);
}

template<>
string ToStrHelper<false, VkQueryType>::Get(const VkQueryType &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_QUERY_TYPE_OCCLUSION)
		TOSTR_CASE_STRINGIZE(VK_QUERY_TYPE_PIPELINE_STATISTICS)
		default: break;
	}
	
	return StringFormat::Fmt("VkQueryType<%d>", el);
}

template<>
string ToStrHelper<false, VkMemoryHeapFlagBits>::Get(const VkMemoryHeapFlagBits &el)
{
	string ret;

	if(el & VK_MEMORY_HEAP_HOST_LOCAL_BIT)          ret += " | VK_MEMORY_HEAP_HOST_LOCAL_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkMemoryPropertyFlagBits>::Get(const VkMemoryPropertyFlagBits &el)
{
	string ret;

	if(el & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)          ret += " | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT";
	if(el & VK_MEMORY_PROPERTY_HOST_NON_COHERENT_BIT)     ret += " | VK_MEMORY_PROPERTY_HOST_NON_COHERENT_BIT";
	if(el & VK_MEMORY_PROPERTY_HOST_UNCACHED_BIT)         ret += " | VK_MEMORY_PROPERTY_HOST_UNCACHED_BIT";
	if(el & VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT)   ret += " | VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT";
	if(el & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)      ret += " | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);
	else
		ret = "VK_MEMORY_PROPERTY_DEVICE_ONLY";

	return ret;
}

template<>
string ToStrHelper<false, VkMemoryInputFlagBits>::Get(const VkMemoryInputFlagBits &el)
{
	string ret;

	if(el & VK_MEMORY_INPUT_HOST_READ_BIT)                     ret += " | VK_MEMORY_INPUT_HOST_READ_BIT";
	if(el & VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT)              ret += " | VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT";
	if(el & VK_MEMORY_INPUT_INDEX_FETCH_BIT)                   ret += " | VK_MEMORY_INPUT_INDEX_FETCH_BIT";
	if(el & VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT)        ret += " | VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT";
	if(el & VK_MEMORY_INPUT_UNIFORM_READ_BIT)                  ret += " | VK_MEMORY_INPUT_UNIFORM_READ_BIT";
	if(el & VK_MEMORY_INPUT_SHADER_READ_BIT)                   ret += " | VK_MEMORY_INPUT_SHADER_READ_BIT";
	if(el & VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT)              ret += " | VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT";
	if(el & VK_MEMORY_INPUT_DEPTH_STENCIL_ATTACHMENT_BIT)      ret += " | VK_MEMORY_INPUT_DEPTH_STENCIL_ATTACHMENT_BIT";
	if(el & VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT)              ret += " | VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT";
	if(el & VK_MEMORY_INPUT_TRANSFER_BIT)                      ret += " | VK_MEMORY_INPUT_TRANSFER_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkMemoryOutputFlagBits>::Get(const VkMemoryOutputFlagBits &el)
{
	string ret;

	if(el & VK_MEMORY_OUTPUT_HOST_WRITE_BIT)                    ret += " | VK_MEMORY_OUTPUT_HOST_WRITE_BIT";
	if(el & VK_MEMORY_OUTPUT_SHADER_WRITE_BIT)                  ret += " | VK_MEMORY_OUTPUT_SHADER_WRITE_BIT";
	if(el & VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT)              ret += " | VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT";
	if(el & VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT)      ret += " | VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT";
	if(el & VK_MEMORY_OUTPUT_TRANSFER_BIT)                      ret += " | VK_MEMORY_OUTPUT_TRANSFER_BIT";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkSharingMode>::Get(const VkSharingMode &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_SHARING_MODE_EXCLUSIVE)
		TOSTR_CASE_STRINGIZE(VK_SHARING_MODE_CONCURRENT)
		default: break;
	}
	
	return StringFormat::Fmt("VkSharingMode<%d>", el);
}

template<>
string ToStrHelper<false, VkCmdBufferLevel>::Get(const VkCmdBufferLevel &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_CMD_BUFFER_LEVEL_PRIMARY)
		TOSTR_CASE_STRINGIZE(VK_CMD_BUFFER_LEVEL_SECONDARY)
		default: break;
	}
	
	return StringFormat::Fmt("VkCmdBufferLevel<%d>", el);
}

template<>
string ToStrHelper<false, VkRenderPassContents>::Get(const VkRenderPassContents &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_RENDER_PASS_CONTENTS_INLINE)
		TOSTR_CASE_STRINGIZE(VK_RENDER_PASS_CONTENTS_SECONDARY_CMD_BUFFERS)
		default: break;
	}
	
	return StringFormat::Fmt("VkRenderPassContents<%d>", el);
}

template<>
string ToStrHelper<false, VkImageLayout>::Get(const VkImageLayout &el)
{
	// cast to int for WSI enum
	switch((int)el)
	{
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_UNDEFINED)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_GENERAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_PREINITIALIZED)
		TOSTR_CASE_STRINGIZE(VK_IMAGE_LAYOUT_PRESENT_SOURCE_KHR)
		default: break;
	}
	
	return StringFormat::Fmt("VkImageLayout<%d>", el);
}

template<>
string ToStrHelper<false, VkStructureType>::Get(const VkStructureType &el)
{
	// cast to int for WSI enum
	switch((int)el)
	{
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_APPLICATION_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SHADER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MEMORY_BARRIER)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO)
		TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR)
		default: break;
	}

	return StringFormat::Fmt("VkStructureType<%d>", el);
}

template<>
string ToStrHelper<false, VkChannelSwizzle>::Get(const VkChannelSwizzle &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_CHANNEL_SWIZZLE_ZERO)
		TOSTR_CASE_STRINGIZE(VK_CHANNEL_SWIZZLE_ONE)
		TOSTR_CASE_STRINGIZE(VK_CHANNEL_SWIZZLE_R)
		TOSTR_CASE_STRINGIZE(VK_CHANNEL_SWIZZLE_G)
		TOSTR_CASE_STRINGIZE(VK_CHANNEL_SWIZZLE_B)
		TOSTR_CASE_STRINGIZE(VK_CHANNEL_SWIZZLE_A)
		default: break;
	}

	return StringFormat::Fmt("VkChannelSwizzle<%d>", el);
}

template<>
string ToStrHelper<false, VkShaderStage>::Get(const VkShaderStage &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_SHADER_STAGE_VERTEX)
		TOSTR_CASE_STRINGIZE(VK_SHADER_STAGE_TESS_CONTROL)
		TOSTR_CASE_STRINGIZE(VK_SHADER_STAGE_TESS_EVALUATION)
		TOSTR_CASE_STRINGIZE(VK_SHADER_STAGE_GEOMETRY)
		TOSTR_CASE_STRINGIZE(VK_SHADER_STAGE_FRAGMENT)
		TOSTR_CASE_STRINGIZE(VK_SHADER_STAGE_COMPUTE)
		default: break;
	}

	return StringFormat::Fmt("VkShaderStage<%d>", el);
}

template<>
string ToStrHelper<false, VkFormat>::Get(const VkFormat &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_FORMAT_UNDEFINED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R4G4_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R4G4_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R4G4B4A4_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R4G4B4A4_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R5G6B5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R5G6B5_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R5G5B5A1_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R5G5B5A1_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R10G10B10A2_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R10G10B10A2_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R10G10B10A2_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R10G10B10A2_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R10G10B10A2_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R10G10B10A2_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32A32_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32A32_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32A32_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R64_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64A64_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R11G11B10_UFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_R9G9B9E5_UFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_D16_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_D24_UNORM_X8)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_D32_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_S8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_D16_UNORM_S8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_D24_UNORM_S8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_D32_SFLOAT_S8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGB_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGB_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGBA_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGBA_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC2_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC2_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC3_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC3_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC4_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC4_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC5_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC6H_UFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC6H_SFLOAT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC7_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_BC7_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A1_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A1_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11G11_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11G11_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_4x4_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_4x4_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x4_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x4_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x5_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x5_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x6_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x6_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x5_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x6_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x6_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x5_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x6_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x6_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x10_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x10_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x10_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x10_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x12_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x12_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B4G4R4A4_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B5G5R5A1_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B5G6R5_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B5G6R5_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SRGB)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G10R10A2_UNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G10R10A2_SNORM)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G10R10A2_USCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G10R10A2_SSCALED)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G10R10A2_UINT)
		TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G10R10A2_SINT)
		default: break;
	}
	
	return StringFormat::Fmt("VkFormat<%d>", el);
}

template<>
string ToStrHelper<false, VkRect2D>::Get(const VkRect2D &el)
{
	return StringFormat::Fmt("VkRect2D<%dx%d+%d+%d>", el.extent.width, el.extent.height, el.offset.x, el.offset.y);
}

template<>
string ToStrHelper<false, VkExtent2D>::Get(const VkExtent2D &el)
{
	return StringFormat::Fmt("VkExtent<%d,%d>", el.width, el.height);
}

template<>
string ToStrHelper<false, VkExtent3D>::Get(const VkExtent3D &el)
{
	return StringFormat::Fmt("VkExtent<%d,%d,%d>", el.width, el.height, el.depth);
}

template<>
string ToStrHelper<false, VkOffset2D>::Get(const VkOffset2D &el)
{
	return StringFormat::Fmt("VkOffset<%d,%d>", el.x, el.y);
}

template<>
string ToStrHelper<false, VkOffset3D>::Get(const VkOffset3D &el)
{
	return StringFormat::Fmt("VkOffset<%d,%d,%d>", el.x, el.y, el.z);
}

template<>
string ToStrHelper<false, VkViewport>::Get(const VkViewport &el)
{
	return StringFormat::Fmt("VkViewport<%f,%f, %fx%f, %f-%f>", el.originX, el.originY, el.width, el.height, el.minDepth, el.maxDepth);
}

template<>
string ToStrHelper<false, VkClearColorValue>::Get(const VkClearColorValue &el)
{
	return StringFormat::Fmt("VkClearColorValue<%f,%f,%f,%f>"
			, el.float32[0], el.float32[1], el.float32[2], el.float32[3]);
}
template<>
string ToStrHelper<false, VkClearDepthStencilValue>::Get(const VkClearDepthStencilValue &el)
{
	return StringFormat::Fmt("VkClearDepthStencilValue<%f %u>"
			, el.depth, el.stencil);
}
template<>
string ToStrHelper<false, VkClearValue>::Get(const VkClearValue &el)
{
	return StringFormat::Fmt("VkClearValue[ col:<%f,%f,%f,%f> / d:%f s:%u ]"
			, el.color.float32[0], el.color.float32[1], el.color.float32[2], el.color.float32[3]
			, el.depthStencil.depth, el.depthStencil.stencil);
}

template<>
string ToStrHelper<false, VkAttachmentReference>::Get(const VkAttachmentReference &el)
{
	return StringFormat::Fmt("VkAttachmentReference<%u, %s>", el.attachment, ToStr::Get(el.layout).c_str());
}

template<>
string ToStrHelper<false, VkSurfaceTransformKHR>::Get(const VkSurfaceTransformKHR &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_NONE_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_ROT90_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_ROT180_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_ROT270_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_HMIRROR_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_HMIRROR_ROT90_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_HMIRROR_ROT180_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_HMIRROR_ROT270_KHR)
		TOSTR_CASE_STRINGIZE(VK_SURFACE_TRANSFORM_INHERIT_KHR)
		default: break;
	}

	return StringFormat::Fmt("VkSurfaceTransformKHR<%d>", el);
}

template<>
string ToStrHelper<false, VkSurfaceTransformFlagBitsKHR>::Get(const VkSurfaceTransformFlagBitsKHR &el)
{
	string ret;

	if(el & VK_SURFACE_TRANSFORM_ROT90_BIT_KHR)           ret += " | VK_SURFACE_TRANSFORM_ROT90_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_ROT180_BIT_KHR)          ret += " | VK_SURFACE_TRANSFORM_ROT180_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_ROT270_BIT_KHR)          ret += " | VK_SURFACE_TRANSFORM_ROT270_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_HMIRROR_BIT_KHR)         ret += " | VK_SURFACE_TRANSFORM_HMIRROR_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_HMIRROR_ROT90_BIT_KHR)   ret += " | VK_SURFACE_TRANSFORM_HMIRROR_ROT90_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_HMIRROR_ROT180_BIT_KHR)  ret += " | VK_SURFACE_TRANSFORM_HMIRROR_ROT180_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_HMIRROR_ROT270_BIT_KHR)  ret += " | VK_SURFACE_TRANSFORM_HMIRROR_ROT270_BIT_KHR";
	if(el & VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR)         ret += " | VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

template<>
string ToStrHelper<false, VkColorSpaceKHR>::Get(const VkColorSpaceKHR &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_COLORSPACE_SRGB_NONLINEAR_KHR)
		default: break;
	}

	return StringFormat::Fmt("VkColorSpaceKHR<%d>", el);
}

template<>
string ToStrHelper<false, VkPresentModeKHR>::Get(const VkPresentModeKHR &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_IMMEDIATE_KHR)
		TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_MAILBOX_KHR)
		TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_FIFO_KHR)
		default: break;
	}

	return StringFormat::Fmt("VkPresentModeKHR<%d>", el);
}

// we know the object will be a non-dispatchable object type
#define SerialiseObject(type, name, obj) \
			{ \
				VulkanResourceManager *rm = (VulkanResourceManager *)GetUserData(); \
				ResourceId id; \
				if(m_Mode >= WRITING) id = GetResID(obj); \
				Serialise(name, id); \
				if(m_Mode < WRITING) obj = (id == ResourceId() || !rm->HasLiveResource(id)) ? VK_NULL_HANDLE : Unwrap(rm->GetLiveHandle<type>(id)); \
			}

static void SerialiseNext(Serialiser *ser, const void *&pNext)
{
	// VKTODOLOW serialise out whether there is a next structure, its type, and contents
	if(ser->IsReading())
		pNext = NULL;
	//else
		//RDCASSERT(pNext == NULL);
}

template<typename T>
void SerialiseOptionalObject(Serialiser *ser, const char *name, T *&el)
{
	bool present;

	present = el != NULL;
	ser->Serialise((string(name) + "Present").c_str(), present);
	if(present)
	{
		if(ser->IsReading())
			el = new T;
		ser->Serialise(name, *el);
	}
	else if(ser->IsReading())
	{
		el = NULL;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkGenericStruct &el)
{
	ScopedContext scope(this, name, "NextStructure", 0, true);
}

template<>
void Serialiser::Serialise(const char *name, VkDeviceQueueCreateInfo &el)
{
	ScopedContext scope(this, name, "VkDeviceQueueCreateInfo", 0, true);
	
	//RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("queueFamilyIndex", el.queueFamilyIndex);
	Serialise("queueCount", el.queueCount);
}

// technically this doesn't need a serialise function as it's POD,
// but we give it one just for ease of printing etc.
template<>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceFeatures &el)
{
	ScopedContext scope(this, name, "VkPhysicalDeviceFeatures", 0, true);

	Serialise("robustBufferAccess", el.robustBufferAccess);
	Serialise("fullDrawIndexUint32", el.fullDrawIndexUint32);
	Serialise("imageCubeArray", el.imageCubeArray);
	Serialise("independentBlend", el.independentBlend);
	Serialise("geometryShader", el.geometryShader);
	Serialise("tessellationShader", el.tessellationShader);
	Serialise("sampleRateShading", el.sampleRateShading);
	Serialise("dualSourceBlend", el.dualSourceBlend);
	Serialise("logicOp", el.logicOp);
	Serialise("multiDrawIndirect", el.multiDrawIndirect);
	Serialise("depthClip", el.depthClip);
	Serialise("depthBiasClamp", el.depthBiasClamp);
	Serialise("fillModeNonSolid", el.fillModeNonSolid);
	Serialise("depthBounds", el.depthBounds);
	Serialise("wideLines", el.wideLines);
	Serialise("largePoints", el.largePoints);
	Serialise("textureCompressionETC2", el.textureCompressionETC2);
	Serialise("textureCompressionASTC_LDR", el.textureCompressionASTC_LDR);
	Serialise("textureCompressionBC", el.textureCompressionBC);
	Serialise("pipelineStatisticsQuery", el.pipelineStatisticsQuery);
	Serialise("vertexSideEffects", el.vertexSideEffects);
	Serialise("tessellationSideEffects", el.tessellationSideEffects);
	Serialise("geometrySideEffects", el.geometrySideEffects);
	Serialise("fragmentSideEffects", el.fragmentSideEffects);
	Serialise("shaderTessellationPointSize", el.shaderTessellationPointSize);
	Serialise("shaderGeometryPointSize", el.shaderGeometryPointSize);
	Serialise("shaderTextureGatherExtended", el.shaderImageGatherExtended);
	Serialise("shaderStorageImageExtendedFormats", el.shaderStorageImageExtendedFormats);
	Serialise("shaderStorageImageMultisample", el.shaderStorageImageMultisample);
	Serialise("shaderUniformBufferArrayDynamicIndexing", el.shaderUniformBufferArrayDynamicIndexing);
	Serialise("shaderSampledImageArrayDynamicIndexing", el.shaderSampledImageArrayDynamicIndexing);
	Serialise("shaderStorageBufferArrayDynamicIndexing", el.shaderStorageBufferArrayDynamicIndexing);
	Serialise("shaderStorageImageArrayDynamicIndexing", el.shaderStorageImageArrayDynamicIndexing);
	Serialise("shaderClipDistance", el.shaderClipDistance);
	Serialise("shaderCullDistance", el.shaderCullDistance);
	Serialise("shaderFloat64", el.shaderFloat64);
	Serialise("shaderInt64", el.shaderInt64);
	Serialise("shaderInt16", el.shaderInt16);
	Serialise("shaderResourceResidency", el.shaderResourceResidency);
	Serialise("shaderResourceMinLOD", el.shaderResourceMinLOD);
	Serialise("alphaToOne", el.alphaToOne);
	Serialise("sparseBinding", el.sparseBinding);
	Serialise("sparseResidencyBuffer", el.sparseResidencyBuffer);
	Serialise("sparseResidencyImage2D", el.sparseResidencyImage2D);
	Serialise("sparseResidencyImage3D", el.sparseResidencyImage3D);
	Serialise("sparseResidency2Samples", el.sparseResidency2Samples);
	Serialise("sparseResidency4Samples", el.sparseResidency4Samples);
	Serialise("sparseResidency8Samples", el.sparseResidency8Samples);
	Serialise("sparseResidency16Samples", el.sparseResidency16Samples);
	Serialise("sparseResidencyAliased", el.sparseResidencyAliased);
}

template<>
void Serialiser::Serialise(const char *name, VkDeviceCreateInfo &el)
{
	ScopedContext scope(this, name, "VkDeviceCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	SerialiseComplexArray("pRequestedQueues", (VkDeviceQueueCreateInfo *&)el.pRequestedQueues, el.queueRecordCount);

	// need to do this by hand to use string DB
	Serialise("extensionCount", el.extensionCount);
	
	if(m_Mode == READING)
		el.ppEnabledExtensionNames = el.extensionCount ? new char*[el.extensionCount] : NULL;
	
	// cast away const on array so we can assign to it on reading
	const char **exts = (const char **)el.ppEnabledExtensionNames;
	for(uint32_t i=0; i < el.extensionCount; i++)
	{
		string s = "";
		if(m_Mode == WRITING && exts[i] != NULL)
			s = exts[i];

		Serialise("ppEnabledExtensionNames", s);

		if(m_Mode == READING)
		{
			m_StringDB.insert(s);
			exts[i] = m_StringDB.find(s)->c_str();
		}
	}

	// need to do this by hand to use string DB
	Serialise("layerCount", el.layerCount);
	
	if(m_Mode == READING)
		el.ppEnabledLayerNames = el.layerCount ? new char*[el.layerCount] : NULL;
	
	// cast away const on array so we can assign to it on reading
	const char **layers = (const char **)el.ppEnabledLayerNames;
	for(uint32_t i=0; i < el.layerCount; i++)
	{
		string s = "";
		if(m_Mode == WRITING && layers[i] != NULL)
			s = layers[i];

		Serialise("ppEnabledLayerNames", s);

		if(m_Mode == READING)
		{
			m_StringDB.insert(s);
			layers[i] = m_StringDB.find(s)->c_str();
		}
	}

	SerialiseOptionalObject(this, "pEnabledFeatures", (VkPhysicalDeviceFeatures *&)el.pEnabledFeatures);
}

//template <> class Serialiser::Deserialise<VkDeviceCreateInfo>;

template<>
void Serialiser::Deserialise(const VkDeviceCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pRequestedQueues;
		delete el->ppEnabledExtensionNames;
		delete el->ppEnabledLayerNames;
		delete el->pEnabledFeatures;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkBufferCreateInfo &el)
{
	ScopedContext scope(this, name, "VkBufferCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("size", el.size);
	Serialise("usage", (VkBufferUsageFlagBits &)el.usage);
	Serialise("flags", (VkBufferCreateFlagBits &)el.flags);
	Serialise("sharingMode", el.sharingMode);
	if(m_Mode == READING) el.pQueueFamilyIndices = NULL;
	SerialisePODArray("pQueueFamilyIndices", (uint32_t *&)el.pQueueFamilyIndices, el.queueFamilyCount);
}

template<>
void Serialiser::Deserialise(const VkBufferCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pQueueFamilyIndices;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkBufferViewCreateInfo &el)
{
	ScopedContext scope(this, name, "VkBufferViewCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	SerialiseObject(VkBuffer, "buffer", el.buffer);
	Serialise("format", el.format);
	Serialise("offset", el.offset);
	Serialise("range", el.range);
}

template<>
void Serialiser::Serialise(const char *name, VkImageCreateInfo &el)
{
	ScopedContext scope(this, name, "VkImageCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("imageType", el.imageType);
	Serialise("format", el.format);
	Serialise("extent", el.extent);
	Serialise("mipLevels", el.mipLevels);
	Serialise("arraySize", el.arraySize);
	Serialise("samples", el.samples);
	Serialise("tiling", el.tiling);
	Serialise("usage", (VkImageUsageFlagBits &)el.usage);
	Serialise("flags", (VkImageCreateFlagBits &)el.flags);
	Serialise("sharingMode", el.sharingMode);
	if(m_Mode == READING) el.pQueueFamilyIndices = NULL;
	SerialisePODArray("pQueueFamilyIndices", (uint32_t *&)el.pQueueFamilyIndices, el.queueFamilyCount);
	Serialise("initialLayout", el.initialLayout);
}

template<>
void Serialiser::Deserialise(const VkImageCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pQueueFamilyIndices;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkImageViewCreateInfo &el)
{
	ScopedContext scope(this, name, "VkImageViewCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseObject(VkImage, "image", el.image);
	Serialise("viewType", el.viewType);
	Serialise("format", el.format);
	Serialise("channels", el.channels);
	Serialise("subresourceRange", el.subresourceRange);
	Serialise("flags", (VkImageViewCreateFlagBits &)el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkFramebufferCreateInfo &el)
{
	ScopedContext scope(this, name, "VkFramebufferCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
	Serialise("width", el.width);
	Serialise("height", el.height);
	Serialise("layers", el.layers);

	// do this one by hand because it's an array of objects that aren't Serialise
	// overloaded
	Serialise("attachmentCount", el.attachmentCount);

	if(m_Mode == READING)
		el.pAttachments = el.attachmentCount ? new VkImageView[el.attachmentCount] : NULL;

	VkImageView *attaches = (VkImageView *)el.pAttachments;
	for(uint32_t i=0; i < el.attachmentCount; i++)
		SerialiseObject(VkImageView, "pAttachments", attaches[i]);
}

template<>
void Serialiser::Deserialise(const VkFramebufferCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pAttachments;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkAttachmentDescription &el)
{
	ScopedContext scope(this, name, "VkAttachmentDescription", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("format", el.format);
	Serialise("samples", el.samples);
	Serialise("loadOp", el.loadOp);
	Serialise("storeOp", el.storeOp);
	Serialise("stencilLoadOp", el.stencilLoadOp);
	Serialise("stencilStoreOp", el.stencilStoreOp);
	Serialise("initialLayout", el.initialLayout);
	Serialise("finalLayout", el.finalLayout);
	Serialise("flags", (VkAttachmentDescriptionFlagBits &)el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkSubpassDescription &el)
{
	ScopedContext scope(this, name, "VkSubpassDescription", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("pipelineBindPoint", el.pipelineBindPoint);
	Serialise("flags", (VkSubpassDescriptionFlagBits &)el.flags);
	Serialise("depthStencilAttachment", el.depthStencilAttachment);

	if(m_Mode == READING)
	{
		el.pInputAttachments = NULL;
		el.pColorAttachments = NULL;
		el.pResolveAttachments = NULL;
		el.pPreserveAttachments = NULL;
	}
	
	SerialisePODArray("inputAttachments", (VkAttachmentReference *&)el.pInputAttachments, el.inputCount);
	SerialisePODArray("colorAttachments", (VkAttachmentReference *&)el.pColorAttachments, el.colorCount);

	bool hasResolves = (el.pResolveAttachments != NULL);
	Serialise("hasResolves", hasResolves);

	if(hasResolves)
		SerialisePODArray("resolveAttachments", (VkAttachmentReference *&)el.pResolveAttachments, el.colorCount);
	
	SerialisePODArray("preserveAttachments", (VkAttachmentReference *&)el.pPreserveAttachments, el.preserveCount);
}

template<>
void Serialiser::Serialise(const char *name, VkSubpassDependency &el)
{
	ScopedContext scope(this, name, "VkSubpassDependency", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("srcSubpass", el.srcSubpass);
	Serialise("destSubpass", el.destSubpass);
	Serialise("srcStageMask", el.srcStageMask);
	Serialise("destStageMask", el.destStageMask);
	Serialise("outputMask", el.outputMask);
	Serialise("inputMask", el.inputMask);
	Serialise("byRegion", el.byRegion);
}

template<>
void Serialiser::Serialise(const char *name, VkRenderPassCreateInfo &el)
{
	ScopedContext scope(this, name, "VkRenderPassCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	SerialiseComplexArray("pAttachments", (VkAttachmentDescription *&)el.pAttachments, el.attachmentCount);
	SerialiseComplexArray("pSubpasses", (VkSubpassDescription *&)el.pSubpasses, el.subpassCount);
	SerialiseComplexArray("pDependencies", (VkSubpassDependency *&)el.pDependencies, el.dependencyCount);
}

template<>
void Serialiser::Deserialise(const VkRenderPassCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		for (uint32_t i=0; i<el->attachmentCount; i++)
		{
			RDCASSERT(el->pAttachments[i].pNext == NULL); // otherwise delete
		}
		delete [] el->pAttachments;
		for (uint32_t i=0; i<el->subpassCount; i++)
		{
			RDCASSERT(el->pSubpasses[i].pNext == NULL); // otherwise delete
			delete el->pSubpasses[i].pInputAttachments;
			delete el->pSubpasses[i].pColorAttachments;
			delete el->pSubpasses[i].pResolveAttachments;
			delete el->pSubpasses[i].pPreserveAttachments;
		}
		delete [] el->pSubpasses;
		for (uint32_t i=0; i<el->dependencyCount; i++)
		{
			RDCASSERT(el->pDependencies[i].pNext == NULL); // otherwise delete
		}
		delete [] el->pDependencies;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkRenderPassBeginInfo &el)
{
	ScopedContext scope(this, name, "VkRenderPassBeginInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
	SerialiseObject(VkFramebuffer, "framebuffer", el.framebuffer);
	Serialise("renderArea", el.renderArea);

	if(m_Mode == READING)
		el.pClearValues = NULL;
	SerialisePODArray("pClearValues", (VkClearValue *&)el.pClearValues, el.clearValueCount);
}

template<>
void Serialiser::Deserialise(const VkRenderPassBeginInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pClearValues;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkVertexInputBindingDescription &el)
{
	ScopedContext scope(this, name, "VkVertexInputBindingDescription", 0, true);
	
	Serialise("binding", el.binding);
	Serialise("strideInBytes", el.strideInBytes);
	Serialise("stepRate", el.stepRate);
}

template<>
void Serialiser::Serialise(const char *name, VkVertexInputAttributeDescription &el)
{
	ScopedContext scope(this, name, "VkVertexInputAttributeDescription", 0, true);
	
	Serialise("location", el.location);
	Serialise("binding", el.binding);
	Serialise("format", el.format);
	Serialise("offsetInBytes", el.offsetInBytes);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineVertexInputStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineVertexInputStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseComplexArray("pVertexBindingDescriptions", (VkVertexInputBindingDescription *&)el.pVertexBindingDescriptions, el.bindingCount);
	SerialiseComplexArray("pVertexAttributeDescriptions", (VkVertexInputAttributeDescription *&)el.pVertexAttributeDescriptions, el.attributeCount);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineInputAssemblyStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineInputAssemblyStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("topology", el.topology);
	Serialise("primitiveRestartEnable", el.primitiveRestartEnable);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineTessellationStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineTessStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("patchControlPoints", el.patchControlPoints);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineViewportStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineViewportStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	if(m_Mode == READING)
	{
		el.pViewports = NULL;
		el.pScissors = NULL;
	}

	// need to handle these arrays potentially being NULL if they're dynamic
	bool hasViews = (el.pViewports != NULL);
	bool hasScissors = (el.pScissors != NULL);
	
	Serialise("hasViews", hasViews);
	Serialise("hasScissors", hasScissors);

	if(hasViews)
		SerialisePODArray("viewports", (VkViewport *&)el.pViewports, el.viewportCount);
	else
		Serialise("viewportCount", el.viewportCount);

	if(hasScissors)
		SerialisePODArray("scissors", (VkRect2D *&)el.pScissors, el.scissorCount);
	else
		Serialise("scissorCount", el.scissorCount);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineRasterStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineRasterStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("depthClipEnable", el.depthClipEnable);
	Serialise("rasterizerDiscardEnable", el.rasterizerDiscardEnable);
	Serialise("fillMode", el.fillMode);
	Serialise("cullMode", el.cullMode);
	Serialise("frontFace", el.frontFace);
	Serialise("depthBiasEnable", el.depthBiasEnable);
	Serialise("depthBias", el.depthBias);
	Serialise("depthBiasClamp", el.depthBiasClamp);
	Serialise("slopeScaledDepthBias", el.slopeScaledDepthBias);
	Serialise("lineWidth", el.lineWidth);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineMultisampleStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineMultisampleStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("rasterSamples", el.rasterSamples);
	RDCASSERT(el.rasterSamples <= 32);
	Serialise("sampleShadingEnable", el.sampleShadingEnable);
	Serialise("minSampleShading", el.minSampleShading);
	SerialiseOptionalObject(this, "sampleMask", (VkSampleMask *&)el.pSampleMask);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineColorBlendAttachmentState &el)
{
	ScopedContext scope(this, name, "VkPipelineColorBlendAttachmentState", 0, true);

	Serialise("blendEnable", el.blendEnable);
	Serialise("srcBlendColor", el.srcBlendColor);
	Serialise("destBlendColor", el.destBlendColor);
	Serialise("blendOpColor", el.blendOpColor);
	Serialise("srcBlendAlpha", el.srcBlendAlpha);
	Serialise("destBlendAlpha", el.destBlendAlpha);
	Serialise("blendOpAlpha", el.blendOpAlpha);
	Serialise("channelWriteMask", el.channelWriteMask);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineColorBlendStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineColorBlendStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("alphaToCoverageEnable", el.alphaToCoverageEnable);
	Serialise("alphaToOneEnable", el.alphaToOneEnable);
	Serialise("logicOpEnable", el.logicOpEnable);
	Serialise("logicOp", el.logicOp);

	Serialise("attachmentCount", el.attachmentCount);
	
	SerialiseComplexArray("pAttachments", (VkPipelineColorBlendAttachmentState*&)el.pAttachments, el.attachmentCount);

	SerialisePODArray<4>("blendConst", el.blendConst);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineDepthStencilStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineDepthStencilStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("depthTestEnable", el.depthTestEnable);
	Serialise("depthWriteEnable", el.depthWriteEnable);
	Serialise("depthCompareOp", el.depthCompareOp);
	Serialise("depthBoundsTestEnable", el.depthBoundsTestEnable);
	Serialise("stencilEnable", el.stencilTestEnable);
	Serialise("front", el.front);
	Serialise("back", el.back);
	Serialise("minDepthBounds", el.minDepthBounds);
	Serialise("maxDepthBounds", el.maxDepthBounds);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineDynamicStateCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineDynamicStateCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	if(m_Mode == READING)
		el.pDynamicStates = NULL;
	SerialisePODArray("dynamicStates", (VkDynamicState *&)el.pDynamicStates, el.dynamicStateCount);
}

template<>
void Serialiser::Serialise(const char *name, VkCmdPoolCreateInfo &el)
{
	ScopedContext scope(this, name, "VkCmdPoolCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("queueFamilyIndex", el.queueFamilyIndex);
	Serialise("flags", (VkCmdPoolCreateFlagBits &)el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkCmdBufferCreateInfo &el)
{
	ScopedContext scope(this, name, "VkCmdBufferCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseObject(VkCmdPool, "cmdPool", el.cmdPool);
	Serialise("level", el.level);
	// VKTODOLOW if this enum gets any bits, cast to Vk*FlagBits
	// for strongly typed serialising
	Serialise("flags", el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkCmdBufferBeginInfo &el)
{
	ScopedContext scope(this, name, "VkCmdBufferBeginInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("flags", (VkCmdBufferOptimizeFlagBits &)el.flags);
	SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
	Serialise("subpass", el.subpass);
	SerialiseObject(VkFramebuffer, "framebuffer", el.framebuffer);
}

template<>
void Serialiser::Serialise(const char *name, VkStencilOpState &el)
{
	ScopedContext scope(this, name, "VkStencilOpState", 0, true);
	
	Serialise("stencilFailOp", el.stencilFailOp);
	Serialise("stencilPassOp", el.stencilPassOp);
	Serialise("stencilDepthFailOp", el.stencilDepthFailOp);
	Serialise("stencilCompareOp", el.stencilCompareOp);
	Serialise("stencilCompareMask", el.stencilCompareMask);
	Serialise("stencilWriteMask", el.stencilWriteMask);
	Serialise("stencilReference", el.stencilReference);
}

template<>
void Serialiser::Serialise(const char *name, VkQueryPoolCreateInfo &el)
{
	ScopedContext scope(this, name, "VkQueryPoolCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("queryType", el.queryType);
	Serialise("slots", el.slots);
	Serialise("pipelineStatistics", (VkQueryPipelineStatisticFlagBits &)el.pipelineStatistics);
}

template<>
void Serialiser::Serialise(const char *name, VkSemaphoreCreateInfo &el)
{
	ScopedContext scope(this, name, "VkSemaphoreCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	// VKTODOLOW if this enum gets any bits, cast to Vk*FlagBits
	// for strongly typed serialising
	Serialise("flags", el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkEventCreateInfo &el)
{
	ScopedContext scope(this, name, "VkEventCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	// VKTODOLOW if this enum gets any bits, cast to Vk*FlagBits
	// for strongly typed serialising
	Serialise("flags", el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkFenceCreateInfo &el)
{
	ScopedContext scope(this, name, "VkFenceCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("flags", (VkFenceCreateFlagBits &)el.flags);
}

template<>
void Serialiser::Serialise(const char *name, VkSamplerCreateInfo &el)
{
	ScopedContext scope(this, name, "VkSamplerCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("minFilter", el.minFilter);
	Serialise("magFilter", el.magFilter);
	Serialise("mipMode", el.mipMode);
	Serialise("addressModeU", el.addressModeU);
	Serialise("addressModeV", el.addressModeV);
	Serialise("addressModeW", el.addressModeW);
	Serialise("mipLodBias", el.mipLodBias);
	Serialise("maxAnisotropy", el.maxAnisotropy);
	Serialise("compareEnable", el.compareEnable);
	Serialise("compareOp", el.compareOp);
	Serialise("minLod", el.minLod);
	Serialise("maxLod", el.maxLod);
	Serialise("borderColor", el.borderColor);
	Serialise("unnormalizedCoordinates", el.unnormalizedCoordinates);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineShaderStageCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineShaderStageCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("stage", el.stage);
	SerialiseObject(VkShader, "shader", el.shader);

	SerialiseOptionalObject(this, "el.pSpecializationInfo", (VkSpecializationInfo *&)el.pSpecializationInfo);
}

template<>
void Serialiser::Serialise(const char *name, VkSpecializationMapEntry &el)
{
	ScopedContext scope(this, name, "VkSpecializationMapEntry", 0, true);

	Serialise("constantId", el.constantId);
	uint64_t size = el.size;
	Serialise("size", size);
	if(m_Mode == READING) el.size = (size_t)size;
	Serialise("offset", el.offset);
}

template<>
void Serialiser::Serialise(const char *name, VkSpecializationInfo &el)
{
	ScopedContext scope(this, name, "VkSpecializationInfo", 0, true);

	uint64_t dataSize = el.dataSize;
	Serialise("dataSize", el.dataSize);
	size_t sz = (size_t)dataSize;
	if(m_Mode == READING) el.pData = NULL;
	SerialiseBuffer("pData", (byte *&)el.pData, sz);

	SerialiseComplexArray("pMap", (VkSpecializationMapEntry *&)el.pMap, el.mapEntryCount);
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineCacheCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineCacheCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	uint64_t initialSize = el.initialSize;
	Serialise("codeSize", initialSize);
	el.initialSize = (size_t)initialSize;

	size_t sz = (size_t)initialSize;
	if(m_Mode == READING) el.initialData = NULL;
	SerialiseBuffer("initialData", (byte *&)el.initialData, sz);

	uint64_t maxSize = el.maxSize;
	Serialise("maxSize", maxSize);
	el.maxSize = (size_t)maxSize;
}

template<>
void Serialiser::Deserialise(const VkPipelineCacheCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] (byte *)(el->initialData);
	}
}

template<>
void Serialiser::Serialise(const char *name, VkPipelineLayoutCreateInfo &el)
{
	ScopedContext scope(this, name, "VkPipelineLayoutCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	// need to do this one by hand since it's just an array of objects that don't themselves have
	// a Serialise overload
	Serialise("descriptorSetCount", el.descriptorSetCount);

	if(m_Mode == READING)
		el.pSetLayouts = el.descriptorSetCount ? new VkDescriptorSetLayout[el.descriptorSetCount] : NULL;

	// cast away const on array so we can assign to it on reading
	VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)el.pSetLayouts;
	for(uint32_t i=0; i < el.descriptorSetCount; i++)
		SerialiseObject(VkDescriptorSetLayout, "layout", layouts[i]);

	SerialiseComplexArray("pPushConstantRanges", (VkPushConstantRange*&)el.pPushConstantRanges, el.pushConstantRangeCount);
}

template<>
void Serialiser::Deserialise(const VkPipelineLayoutCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pSetLayouts;
		delete [] el->pPushConstantRanges;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkShaderModuleCreateInfo &el)
{
	ScopedContext scope(this, name, "VkShaderModuleCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	uint64_t codeSize = el.codeSize;
	Serialise("codeSize", codeSize);
	el.codeSize = (size_t)codeSize;

	size_t sz = (size_t)codeSize;
	if(m_Mode == READING) el.pCode = NULL;
	SerialiseBuffer("pCode", (byte *&)el.pCode, sz);
	// VKTODOLOW if this enum gets any bits, cast to Vk*FlagBits
	// for strongly typed serialising
	Serialise("flags", el.flags);
}

template<>
void Serialiser::Deserialise(const VkShaderModuleCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] (byte *)(el->pCode);
	}
}

template<>
void Serialiser::Serialise(const char *name, VkShaderCreateInfo &el)
{
	ScopedContext scope(this, name, "VkShaderCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SHADER_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	string s = "";
	if(m_Mode >= WRITING && el.pName != NULL)
		s = el.pName;

	Serialise("pName", s);

	if(m_Mode == READING)
	{
		if(s == "")
		{
			el.pName = "";
		}
		else
		{
			string str = (char *)m_BufferHead-s.length();
			m_StringDB.insert(str);
			el.pName = m_StringDB.find(str)->c_str();
		}
	}

	// VKTODOLOW if this enum gets any bits, cast to Vk*FlagBits
	// for strongly typed serialising
	Serialise("flags", el.flags);
	Serialise("stage", el.stage);
	SerialiseObject(VkShaderModule, "module", el.module);
}

template<>
void Serialiser::Serialise(const char *name, VkImageSubresourceRange &el)
{
	ScopedContext scope(this, name, "VkImageSubresourceRange", 0, true);

	Serialise("aspectMask", (VkImageAspectFlagBits &)el.aspectMask);
	Serialise("baseMipLevel", el.baseMipLevel);
	Serialise("mipLevels", el.mipLevels);
	Serialise("baseArrayLayer", el.baseArrayLayer);
	Serialise("arraySize", el.arraySize);
}

template<>
void Serialiser::Serialise(const char *name, VkImageSubresourceCopy &el)
{
	ScopedContext scope(this, name, "VkImageSubresourceCopy", 0, true);

	Serialise("aspect", el.aspect);
	Serialise("mipLevel", el.mipLevel);
	Serialise("arrayLayer", el.arrayLayer);
	Serialise("arraySize", el.arraySize);
}

template<>
void Serialiser::Serialise(const char *name, VkImageSubresource &el)
{
	ScopedContext scope(this, name, "VkImageSubresource", 0, true);
	
	Serialise("aspect", el.aspect);
	Serialise("mipLevel", el.mipLevel);
	Serialise("arrayLayer", el.arrayLayer);
}

template<>
void Serialiser::Serialise(const char *name, VkMemoryAllocInfo &el)
{
	ScopedContext scope(this, name, "VkMemoryAllocInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("allocationSize", el.allocationSize);
	Serialise("memoryTypeIndex", el.memoryTypeIndex);
}

template<>
void Serialiser::Serialise(const char *name, VkMemoryBarrier &el)
{
	ScopedContext scope(this, name, "VkMemoryBarrier", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_MEMORY_BARRIER);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("outputMask", el.outputMask);
	Serialise("inputMask", el.inputMask);
}

template<>
void Serialiser::Serialise(const char *name, VkBufferMemoryBarrier &el)
{
	ScopedContext scope(this, name, "VkBufferMemoryBarrier", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("outputMask", el.outputMask);
	Serialise("inputMask", el.inputMask);
	Serialise("srcQueueFamilyIndex", el.srcQueueFamilyIndex);
	Serialise("destQueueFamilyIndex", el.destQueueFamilyIndex);
	SerialiseObject(VkBuffer, "buffer", el.buffer);
	Serialise("offset", el.offset);
	Serialise("size", el.size);
}

template<>
void Serialiser::Serialise(const char *name, VkImageMemoryBarrier &el)
{
	ScopedContext scope(this, name, "VkImageMemoryBarrier", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("outputMask", el.outputMask);
	Serialise("inputMask", el.inputMask);
	Serialise("oldLayout", el.oldLayout);
	Serialise("newLayout", el.newLayout);
	Serialise("srcQueueFamilyIndex", el.srcQueueFamilyIndex);
	Serialise("destQueueFamilyIndex", el.destQueueFamilyIndex);
	SerialiseObject(VkImage, "image", el.image);
	Serialise("subresourceRange", el.subresourceRange);
}

template<>
void Serialiser::Serialise(const char *name, VkGraphicsPipelineCreateInfo &el)
{
	ScopedContext scope(this, name, "VkGraphicsPipelineCreateInfo", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("flags", (VkPipelineCreateFlagBits &)el.flags);
	SerialiseObject(VkPipelineLayout, "layout", el.layout);
	SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
	Serialise("subpass", el.subpass);
	SerialiseObject(VkPipeline, "basePipelineHandle", el.basePipelineHandle);
	Serialise("basePipelineIndex", el.basePipelineIndex);

	SerialiseOptionalObject(this, "pVertexInputState", (VkPipelineVertexInputStateCreateInfo *&)el.pVertexInputState);
	SerialiseOptionalObject(this, "pInputAssemblyState", (VkPipelineInputAssemblyStateCreateInfo *&)el.pInputAssemblyState);
	SerialiseOptionalObject(this, "pTessellationState", (VkPipelineTessellationStateCreateInfo *&)el.pTessellationState);
	SerialiseOptionalObject(this, "pViewportState", (VkPipelineViewportStateCreateInfo *&)el.pViewportState);
	SerialiseOptionalObject(this, "pRasterState", (VkPipelineRasterStateCreateInfo *&)el.pRasterState);
	SerialiseOptionalObject(this, "pMultisampleState", (VkPipelineMultisampleStateCreateInfo *&)el.pMultisampleState);
	SerialiseOptionalObject(this, "pDepthStencilState", (VkPipelineDepthStencilStateCreateInfo *&)el.pDepthStencilState);
	SerialiseOptionalObject(this, "pColorBlendState", (VkPipelineColorBlendStateCreateInfo *&)el.pColorBlendState);
	SerialiseOptionalObject(this, "pDynamicState", (VkPipelineDynamicStateCreateInfo *&)el.pDynamicState);

	SerialiseComplexArray("pStages", (VkPipelineShaderStageCreateInfo *&)el.pStages, el.stageCount);
}

template<>
void Serialiser::Deserialise(const VkGraphicsPipelineCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		if (el->pVertexInputState)
		{
			RDCASSERT(el->pVertexInputState->pNext == NULL); // otherwise delete
			delete el->pVertexInputState->pVertexBindingDescriptions;
			delete el->pVertexInputState->pVertexAttributeDescriptions;
			delete el->pVertexInputState;
		}
		if (el->pInputAssemblyState)
		{
			RDCASSERT(el->pInputAssemblyState->pNext == NULL); // otherwise delete
			delete el->pInputAssemblyState;
		}
		if (el->pTessellationState)
		{
			RDCASSERT(el->pTessellationState->pNext == NULL); // otherwise delete
			delete el->pTessellationState;
		}
		if (el->pViewportState)
		{
			RDCASSERT(el->pViewportState->pNext == NULL); // otherwise delete
			delete [] el->pViewportState->pViewports;
			delete [] el->pViewportState->pScissors;
			delete el->pViewportState;
		}
		if (el->pRasterState)
		{
			RDCASSERT(el->pRasterState->pNext == NULL); // otherwise delete
			delete el->pRasterState;
		}
		if (el->pMultisampleState)
		{
			RDCASSERT(el->pMultisampleState->pNext == NULL); // otherwise delete
			delete el->pMultisampleState->pSampleMask;
			delete el->pMultisampleState;
		}
		if (el->pDepthStencilState)
		{
			RDCASSERT(el->pDepthStencilState->pNext == NULL); // otherwise delete
			delete el->pDepthStencilState;
		}
		if (el->pColorBlendState)
		{
			RDCASSERT(el->pColorBlendState->pNext == NULL); // otherwise delete
			delete [] el->pColorBlendState->pAttachments;
			delete el->pColorBlendState;
		}
		if (el->pDynamicState)
		{
			RDCASSERT(el->pDynamicState->pNext == NULL); // otherwise delete
			delete [] el->pDynamicState->pDynamicStates;
			delete el->pDynamicState;
		}
		for (uint32_t i=0; i<el->stageCount; i++)
		{
			RDCASSERT(el->pStages[i].pNext == NULL); // otherwise delete
			if (el->pStages[i].pSpecializationInfo)
			{
				delete [] (byte *)(el->pStages[i].pSpecializationInfo->pData);
				delete [] el->pStages[i].pSpecializationInfo->pMap;
				delete el->pStages[i].pSpecializationInfo;
			}
		}
		delete [] el->pStages;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkComputePipelineCreateInfo &el)
{
	ScopedContext scope(this, name, "VkComputePipelineCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);

	Serialise("stage", el.stage);
	Serialise("flags", (VkPipelineCreateFlagBits &)el.flags);
	SerialiseObject(VkPipelineLayout, "layout", el.layout);
	SerialiseObject(VkPipeline, "basePipelineHandle", el.basePipelineHandle);
	Serialise("basePipelineIndex", el.basePipelineIndex);
}

template<>
void Serialiser::Serialise(const char *name, VkDescriptorTypeCount &el)
{
	ScopedContext scope(this, name, "VkDescriptorTypeCount", 0, true);

	Serialise("type", el.type);
	Serialise("count", el.count);
}

template<>
void Serialiser::Serialise(const char *name, VkDescriptorPoolCreateInfo &el)
{
	ScopedContext scope(this, name, "VkDescriptorPoolCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	Serialise("poolUsage", el.poolUsage);
	Serialise("maxSets", el.maxSets);
	SerialiseComplexArray("pTypeCount", (VkDescriptorTypeCount*&)el.pTypeCount, el.count);
}

template<>
void Serialiser::Deserialise(const VkDescriptorPoolCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pTypeCount;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkDescriptorInfo &el)
{
	ScopedContext scope(this, name, "VkDescriptorInfo", 0, true);
	
	SerialiseObject(VkBufferView, "bufferView", el.bufferView);
	SerialiseObject(VkSampler, "sampler", el.sampler);
	SerialiseObject(VkImageView, "imageView", el.imageView);
	Serialise("imageLayout", el.imageLayout);
	SerialiseObject(VkBuffer, "bufferInfo.buffer", el.bufferInfo.buffer);
	Serialise("bufferInfo.offset", el.bufferInfo.offset);
	Serialise("bufferInfo.range", el.bufferInfo.range);
}

template<>
void Serialiser::Serialise(const char *name, VkWriteDescriptorSet &el)
{
	ScopedContext scope(this, name, "VkWriteDescriptorSet", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseObject(VkDescriptorSet, "destSet", el.destSet);
	Serialise("destBinding", el.destBinding);
	Serialise("destArrayElement", el.destArrayElement);
	Serialise("descriptorType", el.descriptorType);

	SerialiseComplexArray("pDescriptors", (VkDescriptorInfo*&)el.pDescriptors, el.count);
}

template<>
void Serialiser::Deserialise(const VkWriteDescriptorSet* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		delete [] el->pDescriptors;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkCopyDescriptorSet &el)
{
	ScopedContext scope(this, name, "VkCopyDescriptorSet", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseObject(VkDescriptorSet, "srcSet", el.srcSet);
	Serialise("srcBinding", el.srcBinding);
	Serialise("srcArrayElement", el.srcArrayElement);
	SerialiseObject(VkDescriptorSet, "destSet", el.destSet);
	Serialise("destBinding", el.destBinding);
	Serialise("destArrayElement", el.destArrayElement);

	Serialise("count", el.count);
}

template<>
void Serialiser::Serialise(const char *name, VkPushConstantRange &el)
{
	ScopedContext scope(this, name, "VkPushConstantRange", 0, true);

	Serialise("stageFlags", (VkShaderStageFlagBits &)el.stageFlags);
	Serialise("start", el.start);
	Serialise("length", el.length);
}

template<>
void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutBinding &el)
{
	ScopedContext scope(this, name, "VkDescriptorSetLayoutBinding", 0, true);

	Serialise("descriptorType", el.descriptorType);
	Serialise("arraySize", el.arraySize);
	Serialise("stageFlags", (VkShaderStageFlagBits &)el.stageFlags);

	bool hasSamplers = el.pImmutableSamplers != NULL;
	Serialise("hasSamplers", hasSamplers);

	// do this one by hand because it's an array of objects that aren't Serialise
	// overloaded
	if(m_Mode == READING)
	{
		if(hasSamplers)
			el.pImmutableSamplers = el.arraySize ? new VkSampler[el.arraySize] : NULL;
		else
			el.pImmutableSamplers = NULL;
	}

	VkSampler *samplers = (VkSampler *)el.pImmutableSamplers;

	for(uint32_t i=0; hasSamplers && i < el.arraySize; i++)
	{
		SerialiseObject(VkSampler, "pImmutableSampler", samplers[i]);
	}
}

template<>
void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutCreateInfo &el)
{
	ScopedContext scope(this, name, "VkDescriptorSetLayoutCreateInfo", 0, true);

	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	SerialiseComplexArray("pBinding", (VkDescriptorSetLayoutBinding *&)el.pBinding, el.count);
}

template<>
void Serialiser::Deserialise(const VkDescriptorSetLayoutCreateInfo* const el) const
{
	if(m_Mode == READING)
	{
		RDCASSERT(el->pNext == NULL); // otherwise delete
		for (uint32_t i=0; i<el->count; i++)
		{
			delete [] el->pBinding[i].pImmutableSamplers;
		}
		delete [] el->pBinding;
	}
}

template<>
void Serialiser::Serialise(const char *name, VkChannelMapping &el)
{
	ScopedContext scope(this, name, "VkChannelMapping", 0, true);

	Serialise("r", el.r);
	Serialise("g", el.g);
	Serialise("b", el.b);
	Serialise("a", el.a);
}

template<>
void Serialiser::Serialise(const char *name, VkBufferImageCopy &el)
{
	ScopedContext scope(this, name, "VkBufferImageCopy", 0, true);
	
	Serialise("memOffset", el.bufferOffset);
	Serialise("bufferRowLength", el.bufferRowLength);
	Serialise("bufferImageHeight", el.bufferImageHeight);
	Serialise("imageSubresource", el.imageSubresource);
	Serialise("imageOffset", el.imageOffset);
	Serialise("imageExtent", el.imageExtent);
}

template<>
void Serialiser::Serialise(const char *name, VkBufferCopy &el)
{
	ScopedContext scope(this, name, "VkBufferCopy", 0, true);
	
	Serialise("srcOffset", el.srcOffset);
	Serialise("destOffset", el.destOffset);
	Serialise("copySize", el.copySize);
}

template<>
void Serialiser::Serialise(const char *name, VkImageCopy &el)
{
	ScopedContext scope(this, name, "VkImageCopy", 0, true);

	Serialise("srcSubresource", el.srcSubresource);
	Serialise("srcOffset", el.srcOffset);
	Serialise("destSubresource", el.destSubresource);
	Serialise("destOffset", el.destOffset);
	Serialise("extent", el.extent);
}

template<>
void Serialiser::Serialise(const char *name, VkImageBlit &el)
{
	ScopedContext scope(this, name, "VkImageBlit", 0, true);

	Serialise("srcSubresource", el.srcSubresource);
	Serialise("srcOffset", el.srcOffset);
	Serialise("srcExtent", el.srcExtent);
	Serialise("destSubresource", el.destSubresource);
	Serialise("destOffset", el.destOffset);
	Serialise("destExtent", el.destExtent);
}

template<>
void Serialiser::Serialise(const char *name, VkImageResolve &el)
{
	ScopedContext scope(this, name, "VkImageResolve", 0, true);
	
	Serialise("srcSubresource", el.srcSubresource);
	Serialise("srcOffset", el.srcOffset);
	Serialise("destSubresource", el.destSubresource);
	Serialise("destOffset", el.destOffset);
	Serialise("extent", el.extent);
}

template<>
void Serialiser::Serialise(const char *name, VkRect2D &el)
{
	ScopedContext scope(this, name, "VkRect2D", 0, true);

	Serialise("offset", el.offset);
	Serialise("extent", el.extent);
}

template<>
void Serialiser::Serialise(const char *name, VkRect3D &el)
{
	ScopedContext scope(this, name, "VkRect3D", 0, true);

	Serialise("offset", el.offset);
	Serialise("extent", el.extent);
}

template<>
void Serialiser::Serialise(const char *name, VkSwapchainCreateInfoKHR &el)
{
	ScopedContext scope(this, name, "VkSwapchainCreateInfoKHR", 0, true);
	
	RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
	Serialise("sType", el.sType);
	SerialiseNext(this, el.pNext);
	
	// VKTODOLOW: don't need any of the info in here, I think
	//Serialise("pSurfaceDescription", *el.pSurfaceDescription);
	
	Serialise("minImageCount", el.minImageCount);
	Serialise("imageFormat", el.imageFormat);
	Serialise("imageColorSpace", el.imageColorSpace);
	Serialise("imageExtent", el.imageExtent);
	Serialise("imageUsageFlags", el.imageUsageFlags);
	Serialise("preTransform", el.preTransform);
	Serialise("imageArraySize", el.imageArraySize);

	// VKTODOLOW: do we need sharingMode, queueFamilyCount, pQueueFamilyIndices?
	
	Serialise("presentMode", el.presentMode);

	// VKTODOLOW: don't think we need the old swap chain
	//Serialise("oldSwapChain", el.oldSwapChain);

	Serialise("clipped", el.clipped);
}
