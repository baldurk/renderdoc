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


#include "core/core.h"

#include "serialise/string_utils.h"

#include "serialise/serialiser.h"

#include "d3d11_common.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/d3d11/d3d11_renderstate.h"

// gives us an address to identify this dll with
static int dllLocator=0;

HMODULE GetD3DCompiler()
{
	static HMODULE ret = NULL;
	if(ret != NULL) return ret;

	// dlls to try in priority order
	const char *dlls[] = {
		"d3dcompiler_47.dll",
		"d3dcompiler_46.dll",
		"d3dcompiler_45.dll",
		"d3dcompiler_44.dll",
		"d3dcompiler_43.dll",
	};

	for(int i=0; i < 2; i++)
	{
		for(int d=0; d < ARRAY_COUNT(dlls); d++)
		{
			if(i == 0)
				ret = GetModuleHandleA(dlls[d]);
			else
				ret = LoadLibraryA(dlls[d]);

			if(ret != NULL) return ret;
		}
	}

	// all else failed, couldn't find d3dcompiler loaded,
	// and couldn't even loadlibrary any version!
	// we'll have to loadlibrary the version that ships with
	// RenderDoc.
	
	HMODULE hModule = NULL;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)&dllLocator,
		&hModule);
	wchar_t curFile[512] = {0};
	GetModuleFileNameW(hModule, curFile, 511);

	wstring path = wstring(curFile);
	path = dirname(path);
	wstring dll = path + L"/d3dcompiler_47.dll";
	
	ret = LoadLibraryW(dll.c_str());

	return ret;
}

D3D11_PRIMITIVE_TOPOLOGY MakeD3D11PrimitiveTopology(PrimitiveTopology Topo)
{
	switch(Topo)
	{
		case eTopology_LineLoop:
		case eTopology_TriangleFan:
			RDCWARN("Unsupported primitive topology on D3D11: %x", Topo);
			break;
		default:
		case eTopology_Unknown:
			return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		case eTopology_PointList:
			return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		case eTopology_LineList:
			return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case eTopology_LineStrip:
			return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case eTopology_TriangleList:
			return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case eTopology_TriangleStrip:
			return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case eTopology_LineList_Adj:
			return D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
		case eTopology_LineStrip_Adj:
			return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
		case eTopology_TriangleList_Adj:
			return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
		case eTopology_TriangleStrip_Adj:
			return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
		case eTopology_PatchList_1CPs:
		case eTopology_PatchList_2CPs:
		case eTopology_PatchList_3CPs:
		case eTopology_PatchList_4CPs:
		case eTopology_PatchList_5CPs:
		case eTopology_PatchList_6CPs:
		case eTopology_PatchList_7CPs:
		case eTopology_PatchList_8CPs:
		case eTopology_PatchList_9CPs:
		case eTopology_PatchList_10CPs:
		case eTopology_PatchList_11CPs:
		case eTopology_PatchList_12CPs:
		case eTopology_PatchList_13CPs:
		case eTopology_PatchList_14CPs:
		case eTopology_PatchList_15CPs:
		case eTopology_PatchList_16CPs:
		case eTopology_PatchList_17CPs:
		case eTopology_PatchList_18CPs:
		case eTopology_PatchList_19CPs:
		case eTopology_PatchList_20CPs:
		case eTopology_PatchList_21CPs:
		case eTopology_PatchList_22CPs:
		case eTopology_PatchList_23CPs:
		case eTopology_PatchList_24CPs:
		case eTopology_PatchList_25CPs:
		case eTopology_PatchList_26CPs:
		case eTopology_PatchList_27CPs:
		case eTopology_PatchList_28CPs:
		case eTopology_PatchList_29CPs:
		case eTopology_PatchList_30CPs:
		case eTopology_PatchList_31CPs:
		case eTopology_PatchList_32CPs:
			return D3D11_PRIMITIVE_TOPOLOGY(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (Topo - eTopology_PatchList_1CPs));
	}

	return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

PrimitiveTopology MakePrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topo)
{
	switch(Topo)
	{
		default:
		case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
			break;
		case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
			return eTopology_PointList;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			return eTopology_LineList;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
			return eTopology_LineStrip;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			return eTopology_TriangleList;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			return eTopology_TriangleStrip;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
			return eTopology_LineList_Adj;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
			return eTopology_LineStrip_Adj;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
			return eTopology_TriangleList_Adj;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
			return eTopology_TriangleStrip_Adj;
			break;
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
			return PrimitiveTopology(eTopology_PatchList_1CPs + (Topo - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) );
			break;
	}

	return eTopology_Unknown;
}

DXGI_FORMAT MakeDXGIFormat(ResourceFormat fmt)
{
	DXGI_FORMAT ret = DXGI_FORMAT_UNKNOWN;

	if(fmt.special)
	{
		switch(fmt.specialFormat)
		{
			case eSpecial_BC1:
				ret = DXGI_FORMAT_BC1_UNORM;
				break;
			case eSpecial_BC2:
				ret = DXGI_FORMAT_BC2_UNORM;
				break;
			case eSpecial_BC3:
				ret = DXGI_FORMAT_BC3_UNORM;
				break;
			case eSpecial_BC4:
				ret = DXGI_FORMAT_BC4_UNORM;
				break;
			case eSpecial_BC5:
				ret = DXGI_FORMAT_BC5_UNORM;
				break;
			case eSpecial_BC6:
				ret = DXGI_FORMAT_BC6H_UF16;
				break;
			case eSpecial_BC7:
				ret = DXGI_FORMAT_BC7_UNORM;
				break;
			case eSpecial_R10G10B10A2:
				if(fmt.compType == eCompType_UNorm)
					ret = DXGI_FORMAT_R10G10B10A2_UNORM;
				else
					ret = DXGI_FORMAT_R10G10B10A2_UINT;
				break;
			case eSpecial_R11G11B10:
				ret = DXGI_FORMAT_R11G11B10_FLOAT;
				break;
			case eSpecial_R5G6B5:
				RDCASSERT(fmt.bgraOrder);
				ret = DXGI_FORMAT_B5G6R5_UNORM;
				break;
			case eSpecial_R5G5B5A1:
				RDCASSERT(fmt.bgraOrder);
				ret = DXGI_FORMAT_B5G5R5A1_UNORM;
				break;
			case eSpecial_R9G9B9E5:
				ret = DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
				break;
#if defined(INCLUDE_D3D_11_1)
			case eSpecial_R4G4B4A4:
				RDCASSERT(fmt.bgraOrder);
				ret = DXGI_FORMAT_B4G4R4A4_UNORM;
				break;
#endif
			case eSpecial_D24S8:
				ret = DXGI_FORMAT_R24G8_TYPELESS;
				break;
			case eSpecial_D32S8:
				ret = DXGI_FORMAT_R32G8X24_TYPELESS;
				break;
#if defined(INCLUDE_D3D_11_1)
			case eSpecial_YUV:
				RDCERR("Video format not unambiguously encoded");
				ret = DXGI_FORMAT_AYUV;
				break;
#endif
			case eSpecial_S8:
				RDCERR("D3D11 has no stencil-only format");
				break;
			case eSpecial_D16S8:
				RDCERR("D3D11 has no D16S8 format");
				break;
			default:
				RDCERR("Unrecognised/unsupported special format %u", fmt.specialFormat);
				break;
		}
	}
	else if(fmt.compCount == 4)
	{
		     if(fmt.compByteWidth == 4) ret = DXGI_FORMAT_R32G32B32A32_TYPELESS;
		else if(fmt.compByteWidth == 2) ret = DXGI_FORMAT_R16G16B16A16_TYPELESS;
		else if(fmt.compByteWidth == 1) ret = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		else		RDCERR("Unrecognised 4-component byte width: %d", fmt.compByteWidth);

		if(fmt.bgraOrder)
			ret = DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	else if(fmt.compCount == 3)
	{
		     if(fmt.compByteWidth == 4) ret = DXGI_FORMAT_R32G32B32_TYPELESS;
		//else if(fmt.compByteWidth == 2) ret = DXGI_FORMAT_R16G16B16_TYPELESS; // format doesn't exist
		//else if(fmt.compByteWidth == 1) ret = DXGI_FORMAT_R8G8B8_TYPELESS; // format doesn't exist
		else		RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
	}
	else if(fmt.compCount == 2)
	{
		     if(fmt.compByteWidth == 4) ret = DXGI_FORMAT_R32G32_TYPELESS;
		else if(fmt.compByteWidth == 2) ret = DXGI_FORMAT_R16G16_TYPELESS;
		else if(fmt.compByteWidth == 1) ret = DXGI_FORMAT_R8G8_TYPELESS;
		else		RDCERR("Unrecognised 2-component byte width: %d", fmt.compByteWidth);
	}
	else if(fmt.compCount == 1)
	{
		     if(fmt.compByteWidth == 4) ret = DXGI_FORMAT_R32_TYPELESS;
		else if(fmt.compByteWidth == 2) ret = DXGI_FORMAT_R16_TYPELESS;
		else if(fmt.compByteWidth == 1) ret = DXGI_FORMAT_R8_TYPELESS;
		else		RDCERR("Unrecognised 1-component byte width: %d", fmt.compByteWidth);
	}
	else
	{
		RDCERR("Unrecognised component count: %d", fmt.compCount);
	}

	if(fmt.compType == eCompType_None)
		ret = GetTypelessFormat(ret);
	else if(fmt.compType == eCompType_Float)
		ret = GetFloatTypedFormat(ret);
	else if(fmt.compType == eCompType_Depth)
		ret = GetDepthTypedFormat(ret);
	else if(fmt.compType == eCompType_UNorm)
		ret = GetUnormTypedFormat(ret);
	else if(fmt.compType == eCompType_SNorm)
		ret = GetSnormTypedFormat(ret);
	else if(fmt.compType == eCompType_UInt)
		ret = GetUIntTypedFormat(ret);
	else if(fmt.compType == eCompType_SInt)
		ret = GetSIntTypedFormat(ret);
	else
		RDCERR("Unrecognised component type");

	if(fmt.srgbCorrected)
		ret = GetSRGBFormat(ret);

	if(ret == DXGI_FORMAT_UNKNOWN)
		RDCERR("No known DXGI_FORMAT corresponding to resource format!");

	return ret;
}

ResourceFormat MakeResourceFormat(DXGI_FORMAT fmt)
{
	ResourceFormat ret;

	ret.rawType = fmt;
	ret.special = false;
	ret.strname = ToStr::Get(fmt).substr(12); // 12 == strlen("DXGI_FORMAT_")

	ret.compCount = ret.compByteWidth = 0;
	ret.compType = eCompType_Float;

	ret.srgbCorrected = IsSRGBFormat(fmt);

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
			ret.compCount = 4;
			break;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			ret.compCount = 3;
			break;
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
		case DXGI_FORMAT_R8G8_SINT:
			ret.compCount = 2;
			break;
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
		case DXGI_FORMAT_A8_UNORM:
			ret.compCount = 1;
			break;

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
		case DXGI_FORMAT_BC5_SNORM:
			ret.compCount = 2;
			break;

		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_B5G6R5_UNORM:

		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			ret.compCount = 3;
			break;

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
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			ret.compCount = 4;
			break;

		case DXGI_FORMAT_R1_UNORM:

		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			ret.compCount = 1;
			break;

		case DXGI_FORMAT_UNKNOWN:
			ret.compCount = 0;

		default:
			ret.special = true;
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
		case DXGI_FORMAT_R32_SINT:
			ret.compByteWidth = 4;
			break;
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
		case DXGI_FORMAT_R16_SINT:
			ret.compByteWidth = 2;
			break;
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
		case DXGI_FORMAT_A8_UNORM:
			ret.compByteWidth = 1;
			break;

		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			ret.compByteWidth = 1;
			break;

		case DXGI_FORMAT_UNKNOWN:
			ret.compByteWidth = 0;

		default:
			ret.special = true;
	}

	// fetch component type from typedFormat so that we don't set eCompType_None.
	// This is a bit of a hack but it's more consistent overall.
	DXGI_FORMAT typedFormat = GetTypedFormat(fmt);

	switch(typedFormat)
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
		case DXGI_FORMAT_R8_TYPELESS:
			RDCERR("We should have converted format to typed! %d", typedFormat);
			ret.compType = eCompType_None;
			break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R16_FLOAT:
			ret.compType = eCompType_Float;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_A8_UNORM:
			ret.compType = eCompType_UNorm;
			break;
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R8_SNORM:
			ret.compType = eCompType_SNorm;
			break;
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R8_UINT:
			ret.compType = eCompType_UInt;
			break;
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R8_SINT:
			ret.compType = eCompType_SInt;
			break;

		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			ret.compType = eCompType_UInt;
			break;

		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R11G11B10_FLOAT:
			ret.compType = eCompType_Float;
			break;

		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC6H_TYPELESS:
			ret.compType = eCompType_SNorm;
			break;

		case DXGI_FORMAT_R24G8_TYPELESS:
			RDCERR("We should have converted format to typed! %d", typedFormat);
			ret.compType = eCompType_None;
			break;
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_D16_UNORM:
			ret.compType = eCompType_Depth;
			break;

		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC7_TYPELESS:
			RDCERR("We should have converted format to typed! %d", typedFormat);
			ret.compType = eCompType_None;
			break;
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_R1_UNORM:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			ret.compType = eCompType_UNorm;
			break;

		case DXGI_FORMAT_UNKNOWN:
			ret.compType = eCompType_None;

		default:
			ret.special = true;
	}

	switch(fmt)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			ret.bgraOrder = true;
			break;
	}

	ret.specialFormat = eSpecial_Unknown;

	switch(fmt)
	{
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
			ret.specialFormat = eSpecial_D24S8;
			break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			ret.specialFormat = eSpecial_D32S8;
			break;
			
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM:
			ret.specialFormat = eSpecial_BC1;
			break;
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM:
			ret.specialFormat = eSpecial_BC2;
			break;
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
			ret.specialFormat = eSpecial_BC3;
			break;
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			ret.specialFormat = eSpecial_BC4;
			break;
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			ret.specialFormat = eSpecial_BC5;
			break;
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC6H_TYPELESS:
			ret.specialFormat = eSpecial_BC6;
			break;
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM:
			ret.specialFormat = eSpecial_BC7;
			break;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			ret.specialFormat = eSpecial_R10G10B10A2;
			break;
		case DXGI_FORMAT_R11G11B10_FLOAT:
			ret.specialFormat = eSpecial_R11G11B10;
			break;
		case DXGI_FORMAT_B5G6R5_UNORM:
			ret.specialFormat = eSpecial_R5G6B5;
			ret.bgraOrder = true;
			break;
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			ret.specialFormat = eSpecial_R5G5B5A1;
			ret.bgraOrder = true;
			break;
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			ret.specialFormat = eSpecial_R9G9B9E5;
			break;

#if defined(INCLUDE_D3D_11_1)
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
			ret.specialFormat = eSpecial_YUV;
			break;

		case DXGI_FORMAT_B4G4R4A4_UNORM:
			ret.specialFormat = eSpecial_R4G4B4A4;
			ret.bgraOrder = true;
			break;
#endif

		default:
			break;
	}

	if(ret.specialFormat != eSpecial_Unknown)
	{
		ret.special = true;
	}

	return ret;
}

ShaderConstant MakeConstantBufferVariable(DXBC::CBufferVariable var, uint32_t &offset);

ShaderVariableType MakeShaderVariableType(DXBC::CBufferVariableType type, uint32_t &offset)
{
	ShaderVariableType ret;

	switch(type.descriptor.type)
	{
		case DXBC::VARTYPE_INT:
			ret.descriptor.type = eVar_Int;
			break;
		case DXBC::VARTYPE_BOOL:
		case DXBC::VARTYPE_UINT:
			ret.descriptor.type = eVar_UInt;
			break;
		case DXBC::VARTYPE_DOUBLE:
			ret.descriptor.type = eVar_Double;
			break;
		case DXBC::VARTYPE_FLOAT:
		default:
			ret.descriptor.type = eVar_Float;
			break;
	}
	ret.descriptor.rows = type.descriptor.rows;
	ret.descriptor.cols = type.descriptor.cols;
	ret.descriptor.elements = type.descriptor.elements;
	ret.descriptor.name = type.descriptor.name;
	ret.descriptor.rowMajorStorage = (type.descriptor.varClass == DXBC::CLASS_MATRIX_ROWS);
	
	uint32_t o = offset;
	
	create_array_uninit(ret.members, type.members.size());
	for(size_t i=0; i < type.members.size(); i++)
	{
		offset = o;
		ret.members[i] = MakeConstantBufferVariable(type.members[i], offset);
	}

	if(ret.members.count > 0)
	{
		ret.descriptor.rows = 0;
		ret.descriptor.cols = 0;
		ret.descriptor.elements = 0;
	}

	return ret;
}

ShaderConstant MakeConstantBufferVariable(DXBC::CBufferVariable var, uint32_t &offset)
{
	ShaderConstant ret;

	ret.name = var.name;
	ret.reg.vec = offset + var.descriptor.offset/16;
	ret.reg.comp = (var.descriptor.offset - (var.descriptor.offset&~0xf))/4;

	offset = ret.reg.vec;

	ret.type = MakeShaderVariableType(var.type, offset);
	
	offset = ret.reg.vec + RDCMAX(1U, var.type.descriptor.bytesize/16);

	return ret;
}

ShaderReflection *MakeShaderReflection(DXBC::DXBCFile *dxbc)
{
	if(dxbc == NULL || !RenderDoc::Inst().IsReplayApp())
		return NULL;

	ShaderReflection *ret = new ShaderReflection();

	if(dxbc->m_DebugInfo)
	{
		ret->DebugInfo.entryFunc = dxbc->m_DebugInfo->GetEntryFunction();
		ret->DebugInfo.compileFlags = dxbc->m_DebugInfo->GetShaderCompileFlags();

		ret->DebugInfo.entryFile = -1;

		create_array_uninit(ret->DebugInfo.files, dxbc->m_DebugInfo->Files.size());
		for(size_t i=0; i < dxbc->m_DebugInfo->Files.size(); i++)
		{
			ret->DebugInfo.files[i].first = dxbc->m_DebugInfo->Files[i].first;
			ret->DebugInfo.files[i].second = dxbc->m_DebugInfo->Files[i].second;

			if(ret->DebugInfo.entryFile == -1 &&
				 strstr(ret->DebugInfo.files[i].second.elems, ret->DebugInfo.entryFunc.elems))
			{
				ret->DebugInfo.entryFile = (int32_t)i;
			}
		}
	}

	ret->Disassembly = dxbc->GetDisassembly();

	ret->DispatchThreadsDimension[0] = dxbc->DispatchThreadsDimension[0];
	ret->DispatchThreadsDimension[1] = dxbc->DispatchThreadsDimension[1];
	ret->DispatchThreadsDimension[2] = dxbc->DispatchThreadsDimension[2];

	ret->InputSig = dxbc->m_InputSig;
	ret->OutputSig = dxbc->m_OutputSig;

	create_array_uninit(ret->ConstantBlocks, dxbc->m_CBuffers.size());
	for(size_t i=0; i < dxbc->m_CBuffers.size(); i++)
	{
		ConstantBlock &cb = ret->ConstantBlocks[i];
		cb.name = dxbc->m_CBuffers[i].name;
		cb.bufferBacked = true;
		cb.bindPoint = (uint32_t)i;

		create_array_uninit(cb.variables, dxbc->m_CBuffers[i].variables.size());
		for(size_t v=0; v < dxbc->m_CBuffers[i].variables.size(); v++)
		{
			uint32_t vecOffset = 0;
			cb.variables[v] = MakeConstantBufferVariable(dxbc->m_CBuffers[i].variables[v], vecOffset);
		}
	}

	int numResources = 0;
	for(size_t i=0; i < dxbc->m_Resources.size(); i++)
		if(dxbc->m_Resources[i].type != DXBC::ShaderInputBind::TYPE_CBUFFER)
			numResources++;

	create_array_uninit(ret->Resources, numResources);
	int32_t idx=0;
	for(size_t i=0; i < dxbc->m_Resources.size(); i++)
	{
		const auto &r = dxbc->m_Resources[i];
		
		if(r.type == DXBC::ShaderInputBind::TYPE_CBUFFER)
			continue;

		ShaderResource res;
		res.bindPoint = r.bindPoint;
		res.name = r.name;

		res.IsSampler = (r.type == DXBC::ShaderInputBind::TYPE_SAMPLER);
		res.IsTexture = (r.type == DXBC::ShaderInputBind::TYPE_TEXTURE &&
						 r.dimension != DXBC::ShaderInputBind::DIM_UNKNOWN &&
						 r.dimension != DXBC::ShaderInputBind::DIM_BUFFER &&
						 r.dimension != DXBC::ShaderInputBind::DIM_BUFFEREX);
		res.IsSRV = (r.type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
		             r.type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
		             r.type == DXBC::ShaderInputBind::TYPE_STRUCTURED ||
		             r.type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS);
		res.IsReadWrite = (r.type == DXBC::ShaderInputBind::TYPE_UAV_RWTYPED ||
		                   r.type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED ||
		                   r.type == DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS ||
		                   r.type == DXBC::ShaderInputBind::TYPE_UAV_APPEND_STRUCTURED ||
		                   r.type == DXBC::ShaderInputBind::TYPE_UAV_CONSUME_STRUCTURED ||
		                   r.type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER);
		
		switch(r.dimension)
		{
			default:
			case DXBC::ShaderInputBind::DIM_UNKNOWN:
				res.resType = eResType_None;
				break;
			case DXBC::ShaderInputBind::DIM_BUFFER:
			case DXBC::ShaderInputBind::DIM_BUFFEREX:
				res.resType = eResType_Buffer;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE1D:
				res.resType = eResType_Texture1D;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY:
				res.resType = eResType_Texture1DArray;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE2D:
				res.resType = eResType_Texture2D;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY:
				res.resType = eResType_Texture2DArray;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE2DMS:
				res.resType = eResType_Texture2DMS;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY:
				res.resType = eResType_Texture2DMSArray;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURE3D:
				res.resType = eResType_Texture3D;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURECUBE:
				res.resType = eResType_TextureCube;
				break;
			case DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY:
				res.resType = eResType_TextureCubeArray;
				break;
		}
		
		if(r.retType != DXBC::ShaderInputBind::RETTYPE_UNKNOWN &&
			 r.retType != DXBC::ShaderInputBind::RETTYPE_MIXED &&
			 r.retType != DXBC::ShaderInputBind::RETTYPE_CONTINUED)
		{
			res.variableType.descriptor.rows = 1;
			res.variableType.descriptor.cols = r.numSamples;
			res.variableType.descriptor.elements = 1;

			string name;

			switch(r.retType)
			{
				case DXBC::ShaderInputBind::RETTYPE_UNORM: name = "unorm float"; break;
				case DXBC::ShaderInputBind::RETTYPE_SNORM: name = "snorm float"; break;
				case DXBC::ShaderInputBind::RETTYPE_SINT: name = "int"; break;
				case DXBC::ShaderInputBind::RETTYPE_UINT: name = "uint"; break;
				case DXBC::ShaderInputBind::RETTYPE_FLOAT: name = "float"; break;
				case DXBC::ShaderInputBind::RETTYPE_DOUBLE: name = "double"; break;
				default: name = "unknown"; break;
			}

			name += ToStr::Get(r.numSamples);

			res.variableType.descriptor.name = name;
		}
		else
		{
			if(dxbc->m_ResourceBinds.find(r.name) != dxbc->m_ResourceBinds.end())
			{
				uint32_t vecOffset = 0;
				res.variableType = MakeShaderVariableType(dxbc->m_ResourceBinds[r.name], vecOffset);
			}
			else
			{
				res.variableType.descriptor.rows = 0;
				res.variableType.descriptor.cols = 0;
				res.variableType.descriptor.elements = 0;
				res.variableType.descriptor.name = "";
			}
		}

		ret->Resources[idx++] = res;
	}

	uint32_t numInterfaces = 0;
	for(size_t i=0; i < dxbc->m_Interfaces.variables.size(); i++)
		numInterfaces = RDCMAX(dxbc->m_Interfaces.variables[i].descriptor.offset+1, numInterfaces);

	create_array(ret->Interfaces, numInterfaces);
	for(size_t i=0; i < dxbc->m_Interfaces.variables.size(); i++)
		ret->Interfaces[dxbc->m_Interfaces.variables[i].descriptor.offset] = dxbc->m_Interfaces.variables[i].name;

	return ret;
}

/////////////////////////////////////////////////////////////
// Structures/descriptors. Serialise members separately
// instead of ToStrInternal separately. Mostly for convenience of
// debugging the output

template<>
void Serialiser::Serialise(const char *name, D3D11_BUFFER_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_BUFFER_DESC", 0, true);
	Serialise("ByteWidth", el.ByteWidth);
	Serialise("Usage", el.Usage);
	Serialise("BindFlags", (D3D11_BIND_FLAG&)el.BindFlags);
	Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG&)el.CPUAccessFlags);
	Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG&)el.MiscFlags);
	Serialise("StructureByteStride", el.StructureByteStride);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE1D_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_TEXTURE1D_DESC", 0, true);
    Serialise("Width", el.Width);
    Serialise("MipLevels", el.MipLevels);
    Serialise("ArraySize", el.ArraySize);
    Serialise("Format", el.Format);
    Serialise("Usage", el.Usage);
    Serialise("BindFlags", (D3D11_BIND_FLAG&)el.BindFlags);
    Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG&)el.CPUAccessFlags);
    Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG&)el.MiscFlags);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE2D_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_TEXTURE2D_DESC", 0, true);
    Serialise("Width", el.Width);
    Serialise("Height", el.Height);
    Serialise("MipLevels", el.MipLevels);
    Serialise("ArraySize", el.ArraySize);
    Serialise("Format", el.Format);
    Serialise("SampleDesc", el.SampleDesc);
    Serialise("Usage", el.Usage);
    Serialise("BindFlags", (D3D11_BIND_FLAG&)el.BindFlags);
    Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG&)el.CPUAccessFlags);
    Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG&)el.MiscFlags);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_TEXTURE3D_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_TEXTURE3D_DESC", 0, true);
    Serialise("Width", el.Width);
    Serialise("Height", el.Height);
    Serialise("Depth", el.Depth);
    Serialise("MipLevels", el.MipLevels);
    Serialise("Format", el.Format);
    Serialise("Usage", el.Usage);
    Serialise("BindFlags", (D3D11_BIND_FLAG&)el.BindFlags);
    Serialise("CPUAccessFlags", (D3D11_CPU_ACCESS_FLAG&)el.CPUAccessFlags);
    Serialise("MiscFlags", (D3D11_RESOURCE_MISC_FLAG&)el.MiscFlags);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_SHADER_RESOURCE_VIEW_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_SHADER_RESOURCE_VIEW_DESC", 0, true);
    Serialise("Format", el.Format);
    Serialise("ViewDimension", el.ViewDimension);

	switch(el.ViewDimension)
	{
		case D3D11_SRV_DIMENSION_BUFFER:
			Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
			Serialise("Buffer.NumElements", el.Buffer.NumElements);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE1D:
			Serialise("Texture1D.MipLevels", el.Texture1D.MipLevels);
			Serialise("Texture1D.MostDetailedMip", el.Texture1D.MostDetailedMip);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
			Serialise("Texture1DArray.MipLevels", el.Texture1DArray.MipLevels);
			Serialise("Texture1DArray.MostDetailedMip", el.Texture1DArray.MostDetailedMip);
			Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
			Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2D:
			Serialise("Texture2D.MipLevels", el.Texture2D.MipLevels);
			Serialise("Texture2D.MostDetailedMip", el.Texture2D.MostDetailedMip);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
			Serialise("Texture2DArray.MipLevels", el.Texture2DArray.MipLevels);
			Serialise("Texture2DArray.MostDetailedMip", el.Texture2DArray.MostDetailedMip);
			Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
			Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DMS:
			// el.Texture2DMS.UnusedField_NothingToDefine
			break;
		case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
			Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
			Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
			break;
		case D3D11_SRV_DIMENSION_TEXTURE3D:
			Serialise("Texture3D.MipLevels", el.Texture3D.MipLevels);
			Serialise("Texture3D.MostDetailedMip", el.Texture3D.MostDetailedMip);
			break;
		case D3D11_SRV_DIMENSION_TEXTURECUBE:
			Serialise("TextureCube.MipLevels", el.TextureCube.MipLevels);
			Serialise("TextureCube.MostDetailedMip", el.TextureCube.MostDetailedMip);
			break;
		case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
			Serialise("TextureCubeArray.MipLevels", el.TextureCubeArray.MipLevels);
			Serialise("TextureCubeArray.MostDetailedMip", el.TextureCubeArray.MostDetailedMip);
			Serialise("TextureCubeArray.NumCubes", el.TextureCubeArray.NumCubes);
			Serialise("TextureCubeArray.First2DArrayFace", el.TextureCubeArray.First2DArrayFace);
			break;
		case D3D11_SRV_DIMENSION_BUFFEREX:
			Serialise("Buffer.FirstElement", el.BufferEx.FirstElement);
			Serialise("Buffer.NumElements", el.BufferEx.NumElements);
			Serialise("Buffer.Flags", el.BufferEx.Flags);
			break;
		default:
			RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension);
			break;
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_RENDER_TARGET_VIEW_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_RENDER_TARGET_VIEW_DESC", 0, true);
    Serialise("Format", el.Format);
    Serialise("ViewDimension", el.ViewDimension);

	switch(el.ViewDimension)
	{
		case D3D11_RTV_DIMENSION_BUFFER:
			Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
			Serialise("Buffer.NumElements", el.Buffer.NumElements);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE1D:
			Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
			Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
			Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
			Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2D:
			Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
			Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
			Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
			Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DMS:
			// el.Texture2DMS.UnusedField_NothingToDefine
			break;
		case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
			Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
			Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
			break;
		case D3D11_RTV_DIMENSION_TEXTURE3D:
			Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
			Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
			Serialise("Texture3D.WSize", el.Texture3D.WSize);
			break;
		default:
			RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension);
			break;
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_UNORDERED_ACCESS_VIEW_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_UNORDERED_ACCESS_VIEW_DESC", 0, true);
    Serialise("Format", el.Format);
    Serialise("ViewDimension", el.ViewDimension);

	switch(el.ViewDimension)
	{
		case D3D11_UAV_DIMENSION_BUFFER:
			Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
			Serialise("Buffer.NumElements", el.Buffer.NumElements);
			Serialise("Buffer.Flags", el.Buffer.Flags);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE1D:
			Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
			Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
			Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
			Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2D:
			Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
			Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
			Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
			Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
			break;
		case D3D11_UAV_DIMENSION_TEXTURE3D:
			Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
			Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
			Serialise("Texture3D.WSize", el.Texture3D.WSize);
			break;
		default:
			RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension);
			break;
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_DEPTH_STENCIL_VIEW_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_DEPTH_STENCIL_VIEW_DESC", 0, true);
    Serialise("Format", el.Format);
    Serialise("Flags", el.Flags);
    Serialise("ViewDimension", el.ViewDimension);

	switch(el.ViewDimension)
	{
		case D3D11_DSV_DIMENSION_TEXTURE1D:
			Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
			Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
			Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
			Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2D:
			Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
			Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
			Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
			Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2DMS:
			// el.Texture2DMS.UnusedField_NothingToDefine
			break;
		case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
			Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
			Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
			break;
		default:
			RDCERR("Unrecognised DSV Dimension %d", el.ViewDimension);
			break;
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_BLEND_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_BLEND_DESC", 0, true);

	Serialise("AlphaToCoverageEnable", el.AlphaToCoverageEnable);
	Serialise("IndependentBlendEnable", el.IndependentBlendEnable);
	for(int i=0; i < 8; i++)
	{
		ScopedContext targetscope(this, this, name, "D3D11_RENDER_TARGET_BLEND_DESC", 0, true);

		bool enable = el.RenderTarget[i].BlendEnable == TRUE;
		Serialise("BlendEnable", enable);
		el.RenderTarget[i].BlendEnable = enable;

		{
			Serialise("SrcBlend", el.RenderTarget[i].SrcBlend);
			Serialise("DestBlend", el.RenderTarget[i].DestBlend);
			Serialise("BlendOp", el.RenderTarget[i].BlendOp);
			Serialise("SrcBlendAlpha", el.RenderTarget[i].SrcBlendAlpha);
			Serialise("DestBlendAlpha", el.RenderTarget[i].DestBlendAlpha);
			Serialise("BlendOpAlpha", el.RenderTarget[i].BlendOpAlpha);
		}

		Serialise("RenderTargetWriteMask", el.RenderTarget[i].RenderTargetWriteMask);
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_DEPTH_STENCIL_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_DEPTH_STENCIL_DESC", 0, true);

	Serialise("DepthEnable", el.DepthEnable);
	Serialise("DepthWriteMask", el.DepthWriteMask);
	Serialise("DepthFunc", el.DepthFunc);
	Serialise("StencilEnable", el.StencilEnable);
	Serialise("StencilReadMask", el.StencilReadMask);
	Serialise("StencilWriteMask", el.StencilWriteMask);

	{
		ScopedContext opscope(this, this, name, "D3D11_DEPTH_STENCILOP_DESC", 0, true);
		Serialise("FrontFace.StencilFailOp", el.FrontFace.StencilFailOp);
		Serialise("FrontFace.StencilDepthFailOp", el.FrontFace.StencilDepthFailOp);
		Serialise("FrontFace.StencilPassOp", el.FrontFace.StencilPassOp);
		Serialise("FrontFace.StencilFunc", el.FrontFace.StencilFunc);
	}
	{
		ScopedContext opscope(this, this, name, "D3D11_DEPTH_STENCILOP_DESC", 0, true);
		Serialise("BackFace.StencilFailOp", el.BackFace.StencilFailOp);
		Serialise("BackFace.StencilDepthFailOp", el.BackFace.StencilDepthFailOp);
		Serialise("BackFace.StencilPassOp", el.BackFace.StencilPassOp);
		Serialise("BackFace.StencilFunc", el.BackFace.StencilFunc);
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_RASTERIZER_DESC", 0, true);

	Serialise("FillMode", el.FillMode);
	Serialise("CullMode", el.CullMode);
	Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
	Serialise("DepthBias", el.DepthBias);
	Serialise("DepthBiasClamp", el.DepthBiasClamp);
	Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
	Serialise("DepthClipEnable", el.DepthClipEnable);
	Serialise("ScissorEnable", el.ScissorEnable);
	Serialise("MultisampleEnable", el.MultisampleEnable);
	Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_QUERY_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_QUERY_DESC", 0, true);

	Serialise("MiscFlags", el.MiscFlags);
	Serialise("Query", el.Query);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_COUNTER_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_COUNTER_DESC", 0, true);

	Serialise("MiscFlags", el.MiscFlags);
	Serialise("Counter", el.Counter);
}

template<>
void Serialiser::Serialise(const char *name, D3D11_SAMPLER_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_SAMPLER_DESC", 0, true);

	Serialise("Filter", el.Filter);
	Serialise("AddressU", el.AddressU);
	Serialise("AddressV", el.AddressV);
	Serialise("AddressW", el.AddressW);
	Serialise("MipLODBias", el.MipLODBias);
	Serialise("MaxAnisotropy", el.MaxAnisotropy);
	Serialise("ComparisonFunc", el.ComparisonFunc);
	SerialisePODArray<4>("BorderColor", el.BorderColor);
	Serialise("MinLOD", el.MinLOD);
	Serialise("MaxLOD", el.MaxLOD);
}

template<> void Serialiser::Serialise(const char *name, D3D11_SO_DECLARATION_ENTRY &el)
{
	ScopedContext scope(this, this, name, "D3D11_SO_DECLARATION_ENTRY", 0, true);
	
	string s = "";
	if(m_Mode >= WRITING && el.SemanticName != NULL)
		s = el.SemanticName;
	
	Serialise("SemanticName", s);

	if(m_Mode == READING)
	{
		if(s == "")
		{
			el.SemanticName = NULL;
		}
		else
		{
			string str = (char *)m_BufferHead-s.length();
			m_StringDB.insert(str);
			el.SemanticName = m_StringDB.find(str)->c_str();
		}
	}

	// so we can just take a char* into the buffer above for the semantic name,
	// ensure we serialise a null terminator (slightly redundant because the above
	// serialise of the string wrote out the length, but not the end of the world).
	char nullterminator = 0;
	Serialise(NULL, nullterminator);
	
	Serialise("SemanticIndex", el.SemanticIndex);
	Serialise("Stream", el.Stream);
	Serialise("StartComponent", el.StartComponent);
	Serialise("ComponentCount", el.ComponentCount);
	Serialise("OutputSlot", el.OutputSlot);
}

template<> void Serialiser::Serialise(const char *name, D3D11_INPUT_ELEMENT_DESC &el)
{
	ScopedContext scope(this, this, name, "D3D11_INPUT_ELEMENT_DESC", 0, true);
	
	string s;
	if(m_Mode >= WRITING)
		s = el.SemanticName;
	
	Serialise("SemanticName", s);

	if(m_Mode == READING)
	{
		string str = (char *)m_BufferHead-s.length();
		m_StringDB.insert(str);
		el.SemanticName = m_StringDB.find(str)->c_str();
	}

	// so we can just take a char* into the buffer above for the semantic name,
	// ensure we serialise a null terminator (slightly redundant because the above
	// serialise of the string wrote out the length, but not the end of the world).
	char nullterminator = 0;
	Serialise(NULL, nullterminator);
	
	Serialise("SemanticIndex", el.SemanticIndex);
	Serialise("Format", el.Format);
	Serialise("InputSlot", el.InputSlot);
	Serialise("AlignedByteOffset", el.AlignedByteOffset);
	Serialise("InputSlotClass", el.InputSlotClass);
	Serialise("InstanceDataStepRate", el.InstanceDataStepRate);
}

template<> void Serialiser::Serialise(const char *name, D3D11_SUBRESOURCE_DATA &el)
{
	ScopedContext scope(this, this, name, "D3D11_SUBRESOURCE_DATA", 0, true);
	
	// el.pSysMem
	Serialise("SysMemPitch", el.SysMemPitch);
	Serialise("SysMemSlicePitch", el.SysMemSlicePitch);
}

#if defined(INCLUDE_D3D_11_1)
template<>
void Serialiser::Serialise(const char *name, D3D11_BLEND_DESC1 &el)
{
	ScopedContext scope(this, this, name, "D3D11_BLEND_DESC1", 0, true);

	Serialise("AlphaToCoverageEnable", el.AlphaToCoverageEnable);
	Serialise("IndependentBlendEnable", el.IndependentBlendEnable);
	for(int i=0; i < 8; i++)
	{
		ScopedContext targetscope(this, this, name, "D3D11_RENDER_TARGET_BLEND_DESC1", 0, true);

		bool enable = el.RenderTarget[i].BlendEnable == TRUE;
		Serialise("BlendEnable", enable);
		el.RenderTarget[i].BlendEnable = enable;
		
		enable = el.RenderTarget[i].LogicOpEnable == TRUE;
		Serialise("LogicOpEnable", enable);
		el.RenderTarget[i].LogicOpEnable = enable;

		{
			Serialise("SrcBlend", el.RenderTarget[i].SrcBlend);
			Serialise("DestBlend", el.RenderTarget[i].DestBlend);
			Serialise("BlendOp", el.RenderTarget[i].BlendOp);
			Serialise("SrcBlendAlpha", el.RenderTarget[i].SrcBlendAlpha);
			Serialise("DestBlendAlpha", el.RenderTarget[i].DestBlendAlpha);
			Serialise("BlendOpAlpha", el.RenderTarget[i].BlendOpAlpha);
			Serialise("LogicOp", el.RenderTarget[i].LogicOp);
		}

		Serialise("RenderTargetWriteMask", el.RenderTarget[i].RenderTargetWriteMask);
	}
}

template<>
void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC1 &el)
{
	ScopedContext scope(this, this, name, "D3D11_RASTERIZER_DESC1", 0, true);

	Serialise("FillMode", el.FillMode);
	Serialise("CullMode", el.CullMode);
	Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
	Serialise("DepthBias", el.DepthBias);
	Serialise("DepthBiasClamp", el.DepthBiasClamp);
	Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
	Serialise("DepthClipEnable", el.DepthClipEnable);
	Serialise("ScissorEnable", el.ScissorEnable);
	Serialise("MultisampleEnable", el.MultisampleEnable);
	Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
	Serialise("ForcedSampleCount", el.ForcedSampleCount);
}
#endif

/////////////////////////////////////////////////////////////
// Trivial structures

string ToStrHelper<false, IID>::Get(const IID &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "GUID {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
				el.Data1, (unsigned int)el.Data2, (unsigned int)el.Data3,
				el.Data4[0], el.Data4[1], el.Data4[2], el.Data4[3],
				el.Data4[4], el.Data4[5], el.Data4[6], el.Data4[7]);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_VIEWPORT>::Get(const D3D11_VIEWPORT &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "Viewport<%.0fx%.0f+%.0f+%.0f>", el.Width, el.Height, el.TopLeftX, el.TopLeftY);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_RECT>::Get(const D3D11_RECT &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "RECT<%d,%d,%d,%d>", el.left, el.right, el.top, el.bottom);

	return tostrBuf;
}


string ToStrHelper<false, D3D11_BOX>::Get(const D3D11_BOX &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "BOX<%d,%d,%d,%d,%d,%d>", el.left, el.right, el.top, el.bottom, el.front, el.back);

	return tostrBuf;
}

string ToStrHelper<false, DXGI_SAMPLE_DESC>::Get(const DXGI_SAMPLE_DESC &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "DXGI_SAMPLE_DESC<%d,%d>", el.Count, el.Quality);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_BIND_FLAG>::Get(const D3D11_BIND_FLAG &el)
{
	string ret;

	if(el & D3D11_BIND_VERTEX_BUFFER)     ret += " | D3D11_BIND_VERTEX_BUFFER";
	if(el & D3D11_BIND_INDEX_BUFFER)      ret += " | D3D11_BIND_INDEX_BUFFER";
	if(el & D3D11_BIND_CONSTANT_BUFFER)   ret += " | D3D11_BIND_CONSTANT_BUFFER";
	if(el & D3D11_BIND_SHADER_RESOURCE)   ret += " | D3D11_BIND_SHADER_RESOURCE";
	if(el & D3D11_BIND_STREAM_OUTPUT)     ret += " | D3D11_BIND_STREAM_OUTPUT";
	if(el & D3D11_BIND_RENDER_TARGET)     ret += " | D3D11_BIND_RENDER_TARGET";
	if(el & D3D11_BIND_DEPTH_STENCIL)     ret += " | D3D11_BIND_DEPTH_STENCIL";
	if(el & D3D11_BIND_UNORDERED_ACCESS)  ret += " | D3D11_BIND_UNORDERED_ACCESS";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

string ToStrHelper<false, D3D11_CPU_ACCESS_FLAG>::Get(const D3D11_CPU_ACCESS_FLAG &el)
{
	string ret;
	
	if(el & D3D11_CPU_ACCESS_READ)      ret += " | D3D11_CPU_ACCESS_READ";
	if(el & D3D11_CPU_ACCESS_WRITE)     ret += " | D3D11_CPU_ACCESS_WRITE";

	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

string ToStrHelper<false, D3D11_RESOURCE_MISC_FLAG>::Get(const D3D11_RESOURCE_MISC_FLAG &el)
{
	string ret;

	if(el & D3D11_RESOURCE_MISC_GENERATE_MIPS)           ret + " | D3D11_RESOURCE_MISC_GENERATE_MIPS";
	if(el & D3D11_RESOURCE_MISC_SHARED)                  ret + " | D3D11_RESOURCE_MISC_SHARED";
	if(el & D3D11_RESOURCE_MISC_TEXTURECUBE)             ret + " | D3D11_RESOURCE_MISC_TEXTURECUBE";
	if(el & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)       ret + " | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS";
	if(el & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)  ret + " | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS";
	if(el & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)       ret + " | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED";
	if(el & D3D11_RESOURCE_MISC_RESOURCE_CLAMP)          ret + " | D3D11_RESOURCE_MISC_RESOURCE_CLAMP";
	if(el & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)       ret + " | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX";
	if(el & D3D11_RESOURCE_MISC_GDI_COMPATIBLE)          ret + " | D3D11_RESOURCE_MISC_GDI_COMPATIBLE";
	
	if(!ret.empty())
		ret = ret.substr(3);

	return ret;
}

/////////////////////////////////////////////////////////////
// Enums and lists

string ToStrHelper<false, D3D11_DEPTH_WRITE_MASK>::Get(const D3D11_DEPTH_WRITE_MASK &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_DEPTH_WRITE_MASK_ZERO)
		TOSTR_CASE_STRINGIZE(D3D11_DEPTH_WRITE_MASK_ALL)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_DEPTH_WRITE_MASK<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_COMPARISON_FUNC>::Get(const D3D11_COMPARISON_FUNC &el)
{
	switch(el)
	{
		case D3D11_COMPARISON_NEVER:           return "NEVER";
		case D3D11_COMPARISON_LESS:            return "LESS";
		case D3D11_COMPARISON_EQUAL:           return "EQUAL";
		case D3D11_COMPARISON_LESS_EQUAL:      return "LESS_EQUAL";
		case D3D11_COMPARISON_GREATER:         return "GREATER";
		case D3D11_COMPARISON_NOT_EQUAL:       return "NOT_EQUAL";
		case D3D11_COMPARISON_GREATER_EQUAL:   return "GREATER_EQUAL";
		case D3D11_COMPARISON_ALWAYS:          return "ALWAYS";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_COMPARISON_FUNC<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_STENCIL_OP>::Get(const D3D11_STENCIL_OP &el)
{
	switch(el)
	{
		case D3D11_STENCIL_OP_KEEP:			return "KEEP";
		case D3D11_STENCIL_OP_ZERO:			return "ZERO";
		case D3D11_STENCIL_OP_REPLACE:		return "REPLACE";
		case D3D11_STENCIL_OP_INCR_SAT:		return "INCR_SAT";
		case D3D11_STENCIL_OP_DECR_SAT:		return "DECR_SAT";
		case D3D11_STENCIL_OP_INVERT:		return "INVERT";
		case D3D11_STENCIL_OP_INCR:			return "INCR";
		case D3D11_STENCIL_OP_DECR:			return "DECR";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_STENCIL_OP<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_BLEND>::Get(const D3D11_BLEND &el)
{
	switch(el)
	{
		case D3D11_BLEND_ZERO:				return "ZERO";
		case D3D11_BLEND_ONE:				return "ONE";
		case D3D11_BLEND_SRC_COLOR:			return "SRC_COLOR";
		case D3D11_BLEND_INV_SRC_COLOR:		return "INV_SRC_COLOR";
		case D3D11_BLEND_SRC_ALPHA:			return "SRC_ALPHA";
		case D3D11_BLEND_INV_SRC_ALPHA:		return "INV_SRC_ALPHA";
		case D3D11_BLEND_DEST_ALPHA:		return "DEST_ALPHA";
		case D3D11_BLEND_INV_DEST_ALPHA:	return "INV_DEST_ALPHA";
		case D3D11_BLEND_DEST_COLOR:		return "DEST_COLOR";
		case D3D11_BLEND_INV_DEST_COLOR:	return "INV_DEST_COLOR";
		case D3D11_BLEND_SRC_ALPHA_SAT:		return "SRC_ALPHA_SAT";
		case D3D11_BLEND_BLEND_FACTOR:		return "BLEND_FACTOR";
		case D3D11_BLEND_INV_BLEND_FACTOR:	return "INV_BLEND_FACTOR";
		case D3D11_BLEND_SRC1_COLOR:		return "SRC1_COLOR";
		case D3D11_BLEND_INV_SRC1_COLOR:	return "INV_SRC1_COLOR";
		case D3D11_BLEND_SRC1_ALPHA:		return "SRC1_ALPHA";
		case D3D11_BLEND_INV_SRC1_ALPHA:	return "INV_SRC1_ALPHA";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_BLEND<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_BLEND_OP>::Get(const D3D11_BLEND_OP &el)
{
	switch(el)
	{
		case D3D11_BLEND_OP_ADD:			return "ADD";
		case D3D11_BLEND_OP_SUBTRACT:		return "SUBTRACT";
		case D3D11_BLEND_OP_REV_SUBTRACT:	return "REV_SUBTRACT";
		case D3D11_BLEND_OP_MIN:			return "MIN";
		case D3D11_BLEND_OP_MAX:			return "MAX";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_BLEND_OP<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_CULL_MODE>::Get(const D3D11_CULL_MODE &el)
{
	switch(el)
	{
		case D3D11_CULL_NONE:  return "NONE";
		case D3D11_CULL_FRONT: return "FRONT";
		case D3D11_CULL_BACK:  return "BACK";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_CULL_MODE<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_FILL_MODE>::Get(const D3D11_FILL_MODE &el)
{
	switch(el)
	{
		case D3D11_FILL_WIREFRAME:	return "WIREFRAME";
		case D3D11_FILL_SOLID:		return "SOLID";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_FILL_MODE<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_TEXTURE_ADDRESS_MODE>::Get(const D3D11_TEXTURE_ADDRESS_MODE &el)
{
	switch(el)
	{
		case D3D11_TEXTURE_ADDRESS_WRAP:		return "WRAP";
		case D3D11_TEXTURE_ADDRESS_MIRROR:		return "MIRROR";
		case D3D11_TEXTURE_ADDRESS_CLAMP:		return "CLAMP";
		case D3D11_TEXTURE_ADDRESS_BORDER:		return "BORDER";
		case D3D11_TEXTURE_ADDRESS_MIRROR_ONCE:	return "MIRROR_ONCE";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_TEXTURE_ADDRESS_MODE<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_FILTER>::Get(const D3D11_FILTER &el)
{
	switch(el)
	{
		case D3D11_FILTER_MIN_MAG_MIP_POINT:							return "MIN_MAG_MIP_POINT";
		case D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR:						return "MIN_MAG_POINT_MIP_LINEAR";
		case D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:				return "MIN_POINT_MAG_LINEAR_MIP_POINT";
		case D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR:						return "MIN_POINT_MAG_MIP_LINEAR";
		case D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT:						return "MIN_LINEAR_MAG_MIP_POINT";
		case D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:				return "MIN_LINEAR_MAG_POINT_MIP_LINEAR";
		case D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT:						return "MIN_MAG_LINEAR_MIP_POINT";
		case D3D11_FILTER_MIN_MAG_MIP_LINEAR:							return "MIN_MAG_MIP_LINEAR";
		case D3D11_FILTER_ANISOTROPIC:									return "ANISOTROPIC";
		case D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT:					return "CMP_MIN_MAG_MIP_POINT";
		case D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR:			return "CMP_MIN_MAG_POINT_MIP_LINEAR";
		case D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return "CMP_MIN_POINT_MAG_LINEAR_MIP_POINT";
		case D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR:			return "CMP_MIN_POINT_MAG_MIP_LINEAR";
		case D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT:			return "CMP_MIN_LINEAR_MAG_MIP_POINT";
		case D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return "CMP_MIN_LINEAR_MAG_POINT_MIP_LINEAR";
		case D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT:			return "CMP_MIN_MAG_LINEAR_MIP_POINT";
		case D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR:				return "CMP_MIN_MAG_MIP_LINEAR";
		case D3D11_FILTER_COMPARISON_ANISOTROPIC:						return "CMP_ANISOTROPIC";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_FILTER<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_SRV_DIMENSION>::Get(const D3D11_SRV_DIMENSION &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_BUFFER)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE1D)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2D)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2DMS)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURE3D)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURECUBE)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_SRV_DIMENSION_BUFFEREX)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_SRV_DIMENSION<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_RTV_DIMENSION>::Get(const D3D11_RTV_DIMENSION &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_BUFFER)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE1D)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2D)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2DMS)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_RTV_DIMENSION_TEXTURE3D)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_RTV_DIMENSION<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_UAV_DIMENSION>::Get(const D3D11_UAV_DIMENSION &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_BUFFER)
		TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE1D)
		TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE2D)
		TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_UAV_DIMENSION_TEXTURE3D)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_UAV_DIMENSION<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_DSV_DIMENSION>::Get(const D3D11_DSV_DIMENSION &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE1D)
		TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2D)
		TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
		TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2DMS)
		TOSTR_CASE_STRINGIZE(D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_DSV_DIMENSION<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D_FEATURE_LEVEL>::Get(const D3D_FEATURE_LEVEL &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_9_1)
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_9_2)
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_9_3)
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_10_0)
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_10_1)
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_11_0)
#if defined(INCLUDE_D3D_11_1)
		TOSTR_CASE_STRINGIZE(D3D_FEATURE_LEVEL_11_1)
#endif
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D_FEATURE_LEVEL<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D_DRIVER_TYPE>::Get(const D3D_DRIVER_TYPE &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D_DRIVER_TYPE_HARDWARE)
		TOSTR_CASE_STRINGIZE(D3D_DRIVER_TYPE_REFERENCE)
		TOSTR_CASE_STRINGIZE(D3D_DRIVER_TYPE_NULL)
		TOSTR_CASE_STRINGIZE(D3D_DRIVER_TYPE_SOFTWARE)
		TOSTR_CASE_STRINGIZE(D3D_DRIVER_TYPE_WARP)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D_DRIVER_TYPE<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_QUERY>::Get(const D3D11_QUERY &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_EVENT)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_OCCLUSION)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_TIMESTAMP)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_TIMESTAMP_DISJOINT)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_PIPELINE_STATISTICS)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_OCCLUSION_PREDICATE)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM0)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM1)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM2)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_STATISTICS_STREAM3)
		TOSTR_CASE_STRINGIZE(D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_QUERY<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_COUNTER>::Get(const D3D11_COUNTER &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_COUNTER_DEVICE_DEPENDENT_0)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_COUNTER<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_MAP>::Get(const D3D11_MAP &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_MAP_READ)
		TOSTR_CASE_STRINGIZE(D3D11_MAP_WRITE)
		TOSTR_CASE_STRINGIZE(D3D11_MAP_READ_WRITE)
		TOSTR_CASE_STRINGIZE(D3D11_MAP_WRITE_DISCARD)
		TOSTR_CASE_STRINGIZE(D3D11_MAP_WRITE_NO_OVERWRITE)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_MAP<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_PRIMITIVE_TOPOLOGY>::Get(const D3D11_PRIMITIVE_TOPOLOGY &el)
{
	switch(el)
	{
		case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:			return "PointList";
		case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:				return "LineList";
		case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:			return "LineStrip";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:			return "TriangleList";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:		return "TriangleStrip";
		case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:			return "LineListAdj";
		case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:		return "LineStripAdj";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:		return "TriangleListAdj";
		case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:	return "TriangleStripAdj";
		default: break;
	}

	char tostrBuf[256] = {0};

	if(el >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST &&
		el <= D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST)
	{
		StringFormat::snprintf(tostrBuf, 255, "Patchlist_%dCPs", el-D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST+1);
	}
	else
	{	
		StringFormat::snprintf(tostrBuf, 255, "D3D11_PRIMITIVE_TOPOLOGY<%d>", el);
	}

	return tostrBuf;
}

string ToStrHelper<false, D3D11_USAGE>::Get(const D3D11_USAGE &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_USAGE_DEFAULT)
		TOSTR_CASE_STRINGIZE(D3D11_USAGE_IMMUTABLE)
		TOSTR_CASE_STRINGIZE(D3D11_USAGE_DYNAMIC)
		TOSTR_CASE_STRINGIZE(D3D11_USAGE_STAGING)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_USAGE<%d>", el);

	return tostrBuf;
}

string ToStrHelper<false, D3D11_INPUT_CLASSIFICATION>::Get(const D3D11_INPUT_CLASSIFICATION &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(D3D11_INPUT_PER_VERTEX_DATA)
		TOSTR_CASE_STRINGIZE(D3D11_INPUT_PER_INSTANCE_DATA)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_INPUT_CLASSIFICATION<%d>", el);

	return tostrBuf;
}

#if defined(INCLUDE_D3D_11_1)
string ToStrHelper<false, D3D11_LOGIC_OP>::Get(const D3D11_LOGIC_OP &el)
{
	switch(el)
	{
        case D3D11_LOGIC_OP_CLEAR:			return "CLEAR";
        case D3D11_LOGIC_OP_SET:			return "SET";
        case D3D11_LOGIC_OP_COPY:			return "COPY";
        case D3D11_LOGIC_OP_COPY_INVERTED:	return "COPY_INVERTED";
        case D3D11_LOGIC_OP_NOOP:			return "NOOP";
        case D3D11_LOGIC_OP_INVERT:			return "INVERT";
        case D3D11_LOGIC_OP_AND:			return "AND";
        case D3D11_LOGIC_OP_NAND:			return "NAND";
        case D3D11_LOGIC_OP_OR:				return "OR";
        case D3D11_LOGIC_OP_NOR:			return "NOR";
        case D3D11_LOGIC_OP_XOR:			return "XOR";
        case D3D11_LOGIC_OP_EQUIV:			return "EQUIV";
        case D3D11_LOGIC_OP_AND_REVERSE:	return "AND_REVERSE";
        case D3D11_LOGIC_OP_AND_INVERTED:	return "AND_INVERTED";
        case D3D11_LOGIC_OP_OR_REVERSE:		return "OR_REVERSE";
        case D3D11_LOGIC_OP_OR_INVERTED:	return "OR_INVERTED";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "D3D11_LOGIC_OP<%d>", el);

	return tostrBuf;
}
#endif

string ToStrHelper<false, DXGI_FORMAT>::Get(const DXGI_FORMAT &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32A32_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32A32_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32A32_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32A32_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32B32_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16B16A16_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16B16A16_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16B16A16_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16B16A16_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16B16A16_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16B16A16_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G32_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32G8X24_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R10G10B10A2_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R10G10B10A2_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R10G10B10A2_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R11G11B10_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8B8A8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8B8A8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8B8A8_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8B8A8_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8B8A8_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16G16_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_D32_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R32_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R24G8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_D24_UNORM_S8_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_X24_TYPELESS_G8_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16_FLOAT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_D16_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R16_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8_UINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8_SINT)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_A8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R1_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R9G9B9E5_SHAREDEXP)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R8G8_B8G8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_G8R8_G8B8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC1_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC1_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC1_UNORM_SRGB)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC2_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC2_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC2_UNORM_SRGB)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC3_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC3_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC3_UNORM_SRGB)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC4_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC4_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC4_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC5_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC5_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC5_SNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B5G6R5_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B5G5R5A1_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B8G8R8A8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B8G8R8X8_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B8G8R8A8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B8G8R8X8_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC6H_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC6H_UF16)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC6H_SF16)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC7_TYPELESS)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC7_UNORM)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_BC7_UNORM_SRGB)
		
#if defined(INCLUDE_D3D_11_1)
		// D3D11.1 formats
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_AYUV)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_Y410)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_Y416)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_NV12)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_P010)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_P016)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_420_OPAQUE)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_YUY2)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_Y210)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_Y216)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_NV11)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_AI44)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_IA44)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_P8)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_A8P8)
		TOSTR_CASE_STRINGIZE(DXGI_FORMAT_B4G4R4A4_UNORM)
#endif
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "DXGI_FORMAT<%d>", el);

	return tostrBuf;
}
