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


#include "d3d11_device.h"
#include "d3d11_context.h"
#include "d3d11_resources.h"
#include "d3d11_renderstate.h"
#include "d3d11_replay.h"

#include <d3dcompiler.h>

#include "shaders/dxbc_debug.h"

#include "common/string_utils.h"

D3D11Replay::D3D11Replay()
{
	m_pDevice = NULL;
	m_Proxy = false;
}

void D3D11Replay::Shutdown()
{
	m_pDevice->Release();
}

FetchTexture D3D11Replay::GetTexture(ResourceId id)
{
	FetchTexture tex;
	tex.ID = ResourceId();

	auto it1D = WrappedID3D11Texture1D::m_TextureList.find(id);
	if(it1D != WrappedID3D11Texture1D::m_TextureList.end())
	{
		WrappedID3D11Texture1D *d3dtex = (WrappedID3D11Texture1D *)it1D->second.m_Texture;

		string str = GetDebugName(d3dtex);
		
		D3D11_TEXTURE1D_DESC desc;
		d3dtex->GetDesc(&desc);

		tex.ID = m_pDevice->GetResourceManager()->GetOriginalID(it1D->first);
		tex.dimension = 1;
		tex.width = desc.Width;
		tex.height = 1;
		tex.depth = 1;
		tex.cubemap = false;
		tex.format = MakeResourceFormat(desc.Format);
		tex.customName = true;

		if(str == "")
		{
			tex.customName = false;
			str = StringFormat::Fmt("Texture1D %llu", tex.ID);
		}
		
		tex.name = widen(str);

		tex.numSubresources = desc.MipLevels;

		tex.creationFlags = 0;
		if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
			tex.creationFlags |= eTextureCreate_SRV;
		if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
			tex.creationFlags |= eTextureCreate_RTV;
		if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
			tex.creationFlags |= eTextureCreate_DSV;
		if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
			tex.creationFlags |= eTextureCreate_UAV;

		if(desc.MipLevels == 0)
			tex.numSubresources = CalcNumMips(desc.Width, 1, 1);

		tex.mips = tex.numSubresources;
		tex.arraysize = desc.ArraySize;
		
		tex.msQual = 0; tex.msSamp = 1;

		tex.numSubresources *= desc.ArraySize;
		
		tex.byteSize = 0;
		for(uint32_t s=0; s < tex.numSubresources; s++)
			tex.byteSize += GetByteSize(d3dtex, s);
		
		return tex;
	}

	auto it2D = WrappedID3D11Texture2D::m_TextureList.find(id);
	if(it2D != WrappedID3D11Texture2D::m_TextureList.end())
	{
		WrappedID3D11Texture2D *d3dtex = (WrappedID3D11Texture2D *)it2D->second.m_Texture;

		string str = GetDebugName(d3dtex);
		
		D3D11_TEXTURE2D_DESC desc;
		d3dtex->GetDesc(&desc);

		if(d3dtex->m_RealDescriptor)
			desc.Format = d3dtex->m_RealDescriptor->Format;

		tex.ID = m_pDevice->GetResourceManager()->GetOriginalID(it2D->first);
		tex.dimension = 2;
		tex.width = desc.Width;
		tex.height = desc.Height;
		tex.depth = 1;
		tex.format = MakeResourceFormat(desc.Format);
		tex.customName = true;
		
		if(str == "")
		{
			tex.customName = false;
			str = StringFormat::Fmt("Texture2D %llu", tex.ID);
		}
		
		tex.name = widen(str);

		tex.numSubresources = desc.MipLevels;

		tex.creationFlags = 0;
		if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
			tex.creationFlags |= eTextureCreate_SRV;
		if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
			tex.creationFlags |= eTextureCreate_RTV;
		if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
			tex.creationFlags |= eTextureCreate_DSV;
		if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
			tex.creationFlags |= eTextureCreate_UAV;
		if(d3dtex->m_RealDescriptor)
			tex.creationFlags |= eTextureCreate_SwapBuffer;

		tex.cubemap = false;
		if(desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
			tex.cubemap = true;
		
		if(desc.MipLevels == 0)
			tex.numSubresources = CalcNumMips(desc.Width, desc.Height, 1);

		tex.mips = tex.numSubresources;
		tex.arraysize = desc.ArraySize;

		tex.msQual = desc.SampleDesc.Quality; tex.msSamp = desc.SampleDesc.Count;

		tex.numSubresources *= desc.ArraySize;
		
		tex.byteSize = 0;
		for(uint32_t s=0; s < tex.numSubresources; s++)
			tex.byteSize += GetByteSize(d3dtex, s);
		
		return tex;
	}

	auto it3D = WrappedID3D11Texture3D::m_TextureList.find(id);
	if(it3D != WrappedID3D11Texture3D::m_TextureList.end())
	{
		WrappedID3D11Texture3D *d3dtex = (WrappedID3D11Texture3D *)it3D->second.m_Texture;

		string str = GetDebugName(d3dtex);
		
		D3D11_TEXTURE3D_DESC desc;
		d3dtex->GetDesc(&desc);

		tex.ID = m_pDevice->GetResourceManager()->GetOriginalID(it3D->first);
		tex.dimension = 3;
		tex.width = desc.Width;
		tex.height = desc.Height;
		tex.depth = desc.Depth;
		tex.cubemap = false;
		tex.format = MakeResourceFormat(desc.Format);
		tex.customName = true;
		
		if(str == "")
		{
			tex.customName = false;
			str = StringFormat::Fmt("Texture3D %llu", tex.ID);
		}
		
		tex.name = widen(str);

		tex.numSubresources = desc.MipLevels;

		tex.creationFlags = 0;
		if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
			tex.creationFlags |= eTextureCreate_SRV;
		if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
			tex.creationFlags |= eTextureCreate_RTV;
		if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
			tex.creationFlags |= eTextureCreate_DSV;
		if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
			tex.creationFlags |= eTextureCreate_UAV;
		
		if(desc.MipLevels == 0)
			tex.numSubresources = CalcNumMips(desc.Width, desc.Height, desc.Depth);

		tex.msQual = 0; tex.msSamp = 1;

		tex.mips = tex.numSubresources;
		tex.arraysize = 1;
		
		tex.byteSize = 0;
		for(uint32_t s=0; s < tex.numSubresources; s++)
			tex.byteSize += GetByteSize(d3dtex, s);
		
		return tex;
	}

	return tex;
}

ShaderReflection *D3D11Replay::GetShader(ResourceId id)
{
	auto it = WrappedShader::m_ShaderList.find(id);

	if(it == WrappedShader::m_ShaderList.end())
		return NULL;

	RDCASSERT(it->second.m_Details);

	return it->second.m_Details;
}

void D3D11Replay::FreeTargetResource(ResourceId id)
{
	ID3D11DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

	SAFE_RELEASE(resource);
}

void D3D11Replay::FreeCustomShader(ResourceId id)
{
	ID3D11DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

	SAFE_RELEASE(resource);
}

vector<FetchFrameRecord> D3D11Replay::GetFrameRecord()
{
	return m_pDevice->GetFrameRecord();
}

vector<EventUsage> D3D11Replay::GetUsage(ResourceId id)
{
	return m_pDevice->GetImmediateContext()->GetUsage(id);
}

APIProperties D3D11Replay::GetAPIProperties()
{
	APIProperties ret;

	ret.pipelineType = ePipelineState_D3D11;

	return ret;
}

vector<ResourceId> D3D11Replay::GetBuffers()
{
	vector<ResourceId> ret;

	ret.reserve(WrappedID3D11Buffer::m_BufferList.size());

	for(auto it = WrappedID3D11Buffer::m_BufferList.begin(); it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
		ret.push_back(it->first);

	return ret;
}

FetchBuffer D3D11Replay::GetBuffer(ResourceId id)
{
	FetchBuffer ret;
	ret.ID = ResourceId();

	auto it = WrappedID3D11Buffer::m_BufferList.find(id);

	if(it == WrappedID3D11Buffer::m_BufferList.end())
		return ret;

	WrappedID3D11Buffer *d3dbuf = it->second.m_Buffer;

	string str = GetDebugName(d3dbuf);

	ret.ID = m_pDevice->GetResourceManager()->GetOriginalID(it->first);
	ret.customName = true;

	if(str == "")
	{
		ret.customName = false;
		str = StringFormat::Fmt("Buffer %llu", ret.ID);
	}

	D3D11_BUFFER_DESC desc;
	it->second.m_Buffer->GetDesc(&desc);

	ret.name = widen(str);
	ret.length = it->second.length;
	ret.structureSize = desc.StructureByteStride;
	ret.byteSize = desc.ByteWidth;

	ret.creationFlags = 0;
	if(desc.BindFlags & D3D11_BIND_VERTEX_BUFFER)
		ret.creationFlags |= eBufferCreate_VB;
	if(desc.BindFlags & D3D11_BIND_INDEX_BUFFER)
		ret.creationFlags |= eBufferCreate_IB;

	return ret;
}

vector<ResourceId> D3D11Replay::GetTextures()
{
	vector<ResourceId> ret;

	ret.reserve(WrappedID3D11Texture1D::m_TextureList.size() +
				WrappedID3D11Texture2D::m_TextureList.size() +
				WrappedID3D11Texture3D::m_TextureList.size());

	for(auto it = WrappedID3D11Texture1D::m_TextureList.begin(); it != WrappedID3D11Texture1D::m_TextureList.end(); ++it)
		ret.push_back(it->first);

	for(auto it = WrappedID3D11Texture2D::m_TextureList.begin(); it != WrappedID3D11Texture2D::m_TextureList.end(); ++it)
		ret.push_back(it->first);

	for(auto it = WrappedID3D11Texture3D::m_TextureList.begin(); it != WrappedID3D11Texture3D::m_TextureList.end(); ++it)
		ret.push_back(it->first);

	return ret;
}

D3D11PipelineState D3D11Replay::MakePipelineState()
{
	D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

	D3D11PipelineState ret;

	/////////////////////////////////////////////////
	// Input Assembler
	/////////////////////////////////////////////////

	switch(rs->IA.Topo)
	{
		default:
		case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
			ret.m_IA.Topology = eTopology_Unknown;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
			ret.m_IA.Topology = eTopology_PointList;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			ret.m_IA.Topology = eTopology_LineList;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
			ret.m_IA.Topology = eTopology_LineStrip;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			ret.m_IA.Topology = eTopology_TriangleList;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			ret.m_IA.Topology = eTopology_TriangleStrip;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
			ret.m_IA.Topology = eTopology_LineList_Adj;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
			ret.m_IA.Topology = eTopology_LineStrip_Adj;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
			ret.m_IA.Topology = eTopology_TriangleList_Adj;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
			ret.m_IA.Topology = eTopology_TriangleStrip_Adj;
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
			ret.m_IA.Topology = PrimitiveTopology(eTopology_PatchList_1CPs + (rs->IA.Topo - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) );
			break;
	}

	D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

	ret.m_IA.Bytecode = NULL;
	
	if(rs->IA.Layout)
	{
		const vector<D3D11_INPUT_ELEMENT_DESC> &vec = m_pDevice->GetLayoutDesc(rs->IA.Layout);

		ret.m_IA.layout = rm->GetOriginalID(GetIDForResource(rs->IA.Layout));
		ret.m_IA.Bytecode = m_pDevice->GetLayoutDXBC(rs->IA.Layout);

		create_array_uninit(ret.m_IA.layouts, vec.size());

		for(size_t i=0; i < vec.size(); i++)
		{
			D3D11PipelineState::InputAssembler::LayoutInput &l = ret.m_IA.layouts[i];
			
			l.ByteOffset = vec[i].AlignedByteOffset;
			l.Format = MakeResourceFormat(vec[i].Format);
			l.InputSlot = vec[i].InputSlot;
			l.PerInstance = vec[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA;
			l.InstanceDataStepRate = vec[i].InstanceDataStepRate;
			l.SemanticIndex = vec[i].SemanticIndex;
			l.SemanticName = widen(vec[i].SemanticName);
		}
	}
	
	create_array_uninit(ret.m_IA.vbuffers, ARRAY_COUNT(rs->IA.VBs));

	for(size_t i=0; i < ARRAY_COUNT(rs->IA.VBs); i++)
	{
		D3D11PipelineState::InputAssembler::VertexBuffer &vb = ret.m_IA.vbuffers[i];

		vb.Buffer = rm->GetOriginalID(GetIDForResource(rs->IA.VBs[i]));
		vb.Offset = rs->IA.Offsets[i];
		vb.Stride = rs->IA.Strides[i];
	}

	ret.m_IA.ibuffer.Buffer = rm->GetOriginalID(GetIDForResource(rs->IA.IndexBuffer));
	ret.m_IA.ibuffer.Format = MakeResourceFormat(rs->IA.IndexFormat);
	ret.m_IA.ibuffer.Offset = rs->IA.IndexOffset;

	/////////////////////////////////////////////////
	// Shaders
	/////////////////////////////////////////////////

	{
		D3D11PipelineState::ShaderStage *dstArr = &ret.m_VS;
		const D3D11RenderState::shader *srcArr = &rs->VS;

		for(size_t i=0; i < 6; i++)
		{
			D3D11PipelineState::ShaderStage &dst = dstArr[i];
			const D3D11RenderState::shader &src = srcArr[i];

			dst.stage = (ShaderStageType)i;
			
			ResourceId id = GetIDForResource(src.Shader);

			dst.Shader = rm->GetOriginalID(id);
			dst.ShaderDetails = NULL;

			// create identity bindpoint mapping
			create_array_uninit(dst.BindpointMapping.ConstantBlocks, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
			for(int s=0; s < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; s++)
			{
				dst.BindpointMapping.ConstantBlocks[s].bind = s;
				dst.BindpointMapping.ConstantBlocks[s].used = true;
			}

			create_array_uninit(dst.BindpointMapping.Resources, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
			for(int32_t s=0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
			{
				dst.BindpointMapping.Resources[s].bind = s;
				dst.BindpointMapping.Resources[s].used = true;
			}

			create_array_uninit(dst.ConstantBuffers, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
			for(size_t s=0; s < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; s++)
			{
				dst.ConstantBuffers[s].Buffer = rm->GetOriginalID(GetIDForResource(src.ConstantBuffers[s]));
				dst.ConstantBuffers[s].VecOffset = src.CBOffsets[s];
				dst.ConstantBuffers[s].VecCount = src.CBCounts[s];
			}

			create_array_uninit(dst.Samplers, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
			for(size_t s=0; s < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; s++)
			{
				D3D11PipelineState::ShaderStage::Sampler &samp = dst.Samplers[s];

				samp.Samp = rm->GetOriginalID(GetIDForResource(src.Samplers[s]));

				if(samp.Samp != ResourceId())
				{
					D3D11_SAMPLER_DESC desc;
					src.Samplers[s]->GetDesc(&desc);

					samp.AddressU = widen(ToStr::Get(desc.AddressU));
					samp.AddressV = widen(ToStr::Get(desc.AddressV));
					samp.AddressW = widen(ToStr::Get(desc.AddressW));

					memcpy(samp.BorderColor, desc.BorderColor, sizeof(FLOAT)*4);

					samp.Comparison = widen(ToStr::Get(desc.ComparisonFunc));
					samp.Filter = widen(ToStr::Get(desc.Filter));
					samp.MaxAniso = desc.MaxAnisotropy;
					samp.MaxLOD = desc.MaxLOD;
					samp.MinLOD = desc.MinLOD;
					samp.MipLODBias = desc.MipLODBias;
				}
			}

			create_array_uninit(dst.SRVs, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
			for(size_t s=0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
			{
				D3D11PipelineState::ShaderStage::ResourceView &view = dst.SRVs[s];

				view.View = rm->GetOriginalID(GetIDForResource(src.SRVs[s]));

				if(view.View != ResourceId())
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC desc;
					src.SRVs[s]->GetDesc(&desc);

					view.Format = MakeResourceFormat(desc.Format);

					ID3D11Resource *res = NULL;
					src.SRVs[s]->GetResource(&res);

					view.Structured = false;
					view.BufferStructCount = 0;

					view.Resource = rm->GetOriginalID(GetIDForResource(res));

					view.Type = widen(ToStr::Get(desc.ViewDimension));

					if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
					{
						view.FirstElement = desc.Buffer.FirstElement;
						view.NumElements = desc.Buffer.NumElements;
						view.ElementOffset = desc.Buffer.ElementOffset;
						view.ElementWidth = desc.Buffer.ElementWidth;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
					{
						view.FirstElement = desc.BufferEx.FirstElement;
						view.NumElements = desc.BufferEx.NumElements;
						view.Flags = desc.BufferEx.Flags;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
					{
						view.HighestMip = desc.Texture1D.MostDetailedMip;
						view.NumMipLevels = desc.Texture1D.MipLevels;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
					{
						view.ArraySize = desc.Texture1DArray.ArraySize;
						view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
						view.HighestMip = desc.Texture1DArray.MostDetailedMip;
						view.NumMipLevels = desc.Texture1DArray.MipLevels;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
					{
						view.HighestMip = desc.Texture2D.MostDetailedMip;
						view.NumMipLevels = desc.Texture2D.MipLevels;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
					{
						view.ArraySize = desc.Texture2DArray.ArraySize;
						view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
						view.HighestMip = desc.Texture2DArray.MostDetailedMip;
						view.NumMipLevels = desc.Texture2DArray.MipLevels;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
					{
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
					{
						view.ArraySize = desc.Texture2DArray.ArraySize;
						view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D)
					{
						view.HighestMip = desc.Texture3D.MostDetailedMip;
						view.NumMipLevels = desc.Texture3D.MipLevels;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
					{
						view.HighestMip = desc.TextureCube.MostDetailedMip;
						view.NumMipLevels = desc.TextureCube.MipLevels;
					}
					else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
					{
						view.ArraySize = desc.TextureCubeArray.NumCubes;
						view.FirstArraySlice = desc.TextureCubeArray.First2DArrayFace;
						view.HighestMip = desc.TextureCubeArray.MostDetailedMip;
						view.NumMipLevels = desc.TextureCubeArray.MipLevels;
					}

					SAFE_RELEASE(res);
				}
			}

			create_array_uninit(dst.UAVs, D3D11_PS_CS_UAV_REGISTER_COUNT);
			for(size_t s=0; s < D3D11_PS_CS_UAV_REGISTER_COUNT; s++)
			{
				D3D11PipelineState::ShaderStage::ResourceView &view = dst.UAVs[s];

				view.View = rm->GetOriginalID(GetIDForResource(src.UAVs[s]));

				if(view.View != ResourceId())
				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
					src.UAVs[s]->GetDesc(&desc);

					ID3D11Resource *res = NULL;
					src.UAVs[s]->GetResource(&res);
					
					view.Structured = false;
					view.BufferStructCount = 0;

					if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
						(desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND|D3D11_BUFFER_UAV_FLAG_COUNTER)))
					{
						view.Structured = true;
						view.BufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(src.UAVs[s]);
					}

					view.Resource = rm->GetOriginalID(GetIDForResource(res));

					view.Format = MakeResourceFormat(desc.Format);
					view.Type = widen(ToStr::Get(desc.ViewDimension));

					if(desc.ViewDimension == D3D11_RTV_DIMENSION_BUFFER)
					{
						view.FirstElement = desc.Buffer.FirstElement;
						view.NumElements = desc.Buffer.NumElements;
					}
					else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
					{
						view.MipSlice = desc.Texture1D.MipSlice;
					}
					else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
					{
						view.ArraySize = desc.Texture1DArray.ArraySize;
						view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
						view.MipSlice = desc.Texture1DArray.MipSlice;
					}
					else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
					{
						view.MipSlice = desc.Texture2D.MipSlice;
					}
					else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
					{
						view.ArraySize = desc.Texture2DArray.ArraySize;
						view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
						view.MipSlice = desc.Texture2DArray.MipSlice;
					}
					else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
					{
						view.ArraySize = desc.Texture3D.WSize;
						view.FirstArraySlice = desc.Texture3D.FirstWSlice;
						view.MipSlice = desc.Texture3D.MipSlice;
					}

					SAFE_RELEASE(res);
				}
			}

			create_array_uninit(dst.ClassInstances, src.NumInstances);
			for(UINT s=0; s < src.NumInstances; s++)
			{
				D3D11_CLASS_INSTANCE_DESC desc;
				src.Instances[s]->GetDesc(&desc);

				char typeName[256] = {0};
				SIZE_T count = 255;
				src.Instances[s]->GetTypeName(typeName, &count);

				char instName[256] = {0};
				count = 255;
				src.Instances[s]->GetInstanceName(instName, &count);

				dst.ClassInstances[s] = widen(instName);
			}
		}
	}

	/////////////////////////////////////////////////
	// Stream Out
	/////////////////////////////////////////////////

	{
		create_array_uninit(ret.m_SO.Outputs, D3D11_SO_BUFFER_SLOT_COUNT);
		for(size_t s=0; s < D3D11_SO_BUFFER_SLOT_COUNT; s++)
		{
			ret.m_SO.Outputs[s].Buffer = rm->GetOriginalID(GetIDForResource(rs->SO.Buffers[s]));
			ret.m_SO.Outputs[s].Offset = rs->SO.Offsets[s];
		}
	}

	/////////////////////////////////////////////////
	// Rasterizer
	/////////////////////////////////////////////////

	{
		D3D11_RASTERIZER_DESC desc;

		if(rs->RS.State)
		{
			rs->RS.State->GetDesc(&desc);

			ret.m_RS.m_State.AntialiasedLineEnable = desc.AntialiasedLineEnable == TRUE;

			ret.m_RS.m_State.CullMode = eCull_None;
			if(desc.CullMode == D3D11_CULL_FRONT) ret.m_RS.m_State.CullMode = eCull_Front;
			if(desc.CullMode == D3D11_CULL_BACK) ret.m_RS.m_State.CullMode = eCull_Back;
			
			ret.m_RS.m_State.FillMode = eFill_Solid;
			if(desc.FillMode == D3D11_FILL_WIREFRAME) ret.m_RS.m_State.FillMode = eFill_Wireframe;

			ret.m_RS.m_State.DepthBias = desc.DepthBias;
			ret.m_RS.m_State.DepthBiasClamp = desc.DepthBiasClamp;
			ret.m_RS.m_State.DepthClip = desc.DepthClipEnable == TRUE;
			ret.m_RS.m_State.FrontCCW = desc.FrontCounterClockwise == TRUE;
			ret.m_RS.m_State.MultisampleEnable = desc.MultisampleEnable == TRUE;
			ret.m_RS.m_State.ScissorEnable = desc.ScissorEnable == TRUE;
			ret.m_RS.m_State.SlopeScaledDepthBias = desc.SlopeScaledDepthBias;
			ret.m_RS.m_State.ForcedSampleCount = 0;

#if defined(INCLUDE_D3D_11_1)
			D3D11_RASTERIZER_DESC1 desc1;
			RDCEraseEl(desc1);

			if(WrappedID3D11RasterizerState1::IsAlloc(rs->RS.State))
			{
				((ID3D11RasterizerState1 *)rs->RS.State)->GetDesc1(&desc1);
				ret.m_RS.m_State.ForcedSampleCount = desc1.ForcedSampleCount;
			}
#endif

			ret.m_RS.m_State.State = rm->GetOriginalID(GetIDForResource(rs->RS.State));
		}
		else
		{
			ret.m_RS.m_State.AntialiasedLineEnable = FALSE;
			ret.m_RS.m_State.CullMode = eCull_Back;
			ret.m_RS.m_State.DepthBias = 0;
			ret.m_RS.m_State.DepthBiasClamp = 0.0f;
			ret.m_RS.m_State.DepthClip = TRUE;
			ret.m_RS.m_State.FillMode = eFill_Solid;
			ret.m_RS.m_State.FrontCCW = FALSE;
			ret.m_RS.m_State.MultisampleEnable = FALSE;
			ret.m_RS.m_State.ScissorEnable = FALSE;
			ret.m_RS.m_State.SlopeScaledDepthBias = 0.0f;
			ret.m_RS.m_State.ForcedSampleCount = 0;
			ret.m_RS.m_State.State = ResourceId();
		}

		size_t i=0;
		create_array_uninit(ret.m_RS.Scissors, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
		for(i=0; i < rs->RS.NumScissors; i++)
			ret.m_RS.Scissors[i] = D3D11PipelineState::Rasterizer::Scissor(rs->RS.Scissors[i].left, rs->RS.Scissors[i].top,
																			rs->RS.Scissors[i].right, rs->RS.Scissors[i].bottom); 

		for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
			ret.m_RS.Scissors[i] = D3D11PipelineState::Rasterizer::Scissor(0, 0, 0, 0);
		
		create_array_uninit(ret.m_RS.Viewports, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
		for(i=0; i < rs->RS.NumViews; i++)
			ret.m_RS.Viewports[i] = D3D11PipelineState::Rasterizer::Viewport(rs->RS.Viewports[i].TopLeftX, rs->RS.Viewports[i].TopLeftY, 
																				rs->RS.Viewports[i].Width, rs->RS.Viewports[i].Height, 
																				rs->RS.Viewports[i].MinDepth, rs->RS.Viewports[i].MaxDepth);

		for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
			ret.m_RS.Viewports[i] = D3D11PipelineState::Rasterizer::Viewport(0, 0, 0, 0, 0, 0);
	}

	/////////////////////////////////////////////////
	// Output Merger
	/////////////////////////////////////////////////

	{
		create_array_uninit(ret.m_OM.RenderTargets, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
		for(size_t i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			D3D11PipelineState::ShaderStage::ResourceView &view = ret.m_OM.RenderTargets[i];

			view.View = rm->GetOriginalID(GetIDForResource(rs->OM.RenderTargets[i]));

			if(view.View != ResourceId())
			{
				D3D11_RENDER_TARGET_VIEW_DESC desc;
				rs->OM.RenderTargets[i]->GetDesc(&desc);

				ID3D11Resource *res = NULL;
				rs->OM.RenderTargets[i]->GetResource(&res);

				view.Structured = false;
				view.BufferStructCount = 0;

				view.Resource = rm->GetOriginalID(GetIDForResource(res));

				view.Format = MakeResourceFormat(desc.Format);
				view.Type = widen(ToStr::Get(desc.ViewDimension));

				if(desc.ViewDimension == D3D11_RTV_DIMENSION_BUFFER)
				{
					view.FirstElement = desc.Buffer.FirstElement;
					view.NumElements = desc.Buffer.NumElements;
				}
				else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
				{
					view.MipSlice = desc.Texture1D.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
				{
					view.ArraySize = desc.Texture1DArray.ArraySize;
					view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
					view.MipSlice = desc.Texture1DArray.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
				{
					view.MipSlice = desc.Texture2D.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
				{
					view.ArraySize = desc.Texture2DArray.ArraySize;
					view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
					view.MipSlice = desc.Texture2DArray.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE3D)
				{
					view.ArraySize = desc.Texture3D.WSize;
					view.FirstArraySlice = desc.Texture3D.FirstWSlice;
					view.MipSlice = desc.Texture3D.MipSlice;
				}

				SAFE_RELEASE(res);
			}
		}

		ret.m_OM.UAVStartSlot = rs->OM.UAVStartSlot;
		
		create_array_uninit(ret.m_OM.UAVs, D3D11_PS_CS_UAV_REGISTER_COUNT);
		for(size_t s=0; s < D3D11_PS_CS_UAV_REGISTER_COUNT; s++)
		{
			D3D11PipelineState::ShaderStage::ResourceView view;

			view.View = rm->GetOriginalID(GetIDForResource(rs->OM.UAVs[s]));

			if(view.View != ResourceId())
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
				rs->OM.UAVs[s]->GetDesc(&desc);

				ID3D11Resource *res = NULL;
				rs->OM.UAVs[s]->GetResource(&res);

				view.Structured = false;
				view.BufferStructCount = 0;

				if(desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND|D3D11_BUFFER_UAV_FLAG_COUNTER))
				{
					view.Structured = true;
					view.BufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(rs->OM.UAVs[s]);
				}

				view.Resource = rm->GetOriginalID(GetIDForResource(res));

				view.Format = MakeResourceFormat(desc.Format);
				view.Type = widen(ToStr::Get(desc.ViewDimension));

				if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
				{
					view.FirstElement = desc.Buffer.FirstElement;
					view.NumElements = desc.Buffer.NumElements;
					view.Flags = desc.Buffer.Flags;
				}
				else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
				{
					view.MipSlice = desc.Texture1D.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
				{
					view.ArraySize = desc.Texture1DArray.ArraySize;
					view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
					view.MipSlice = desc.Texture1DArray.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
				{
					view.MipSlice = desc.Texture2D.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
				{
					view.ArraySize = desc.Texture2DArray.ArraySize;
					view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
					view.MipSlice = desc.Texture2DArray.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
				{
					view.ArraySize = desc.Texture3D.WSize;
					view.FirstArraySlice = desc.Texture3D.FirstWSlice;
					view.MipSlice = desc.Texture3D.MipSlice;
				}

				SAFE_RELEASE(res);
			}

			ret.m_OM.UAVs[s] = view;
		}
		
		{
			D3D11PipelineState::ShaderStage::ResourceView &view = ret.m_OM.DepthTarget;

			view.View = rm->GetOriginalID(GetIDForResource(rs->OM.DepthView));

			if(view.View != ResourceId())
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC desc;
				rs->OM.DepthView->GetDesc(&desc);

				ID3D11Resource *res = NULL;
				rs->OM.DepthView->GetResource(&res);

				view.Structured = false;
				view.BufferStructCount = 0;
				
				ret.m_OM.DepthReadOnly = false;
				ret.m_OM.StencilReadOnly = false;

				if(desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
					ret.m_OM.DepthReadOnly = true;
				if(desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
					ret.m_OM.StencilReadOnly = true;

				view.Resource = rm->GetOriginalID(GetIDForResource(res));

				view.Format = MakeResourceFormat(desc.Format);
				view.Type = widen(ToStr::Get(desc.ViewDimension));

				if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
				{
					view.MipSlice = desc.Texture1D.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
				{
					view.ArraySize = desc.Texture1DArray.ArraySize;
					view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
					view.MipSlice = desc.Texture1DArray.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
				{
					view.MipSlice = desc.Texture2D.MipSlice;
				}
				else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
				{
					view.ArraySize = desc.Texture2DArray.ArraySize;
					view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
					view.MipSlice = desc.Texture2DArray.MipSlice;
				}

				SAFE_RELEASE(res);
			}
		}
		
		if(rs->OM.BlendState)
		{
			D3D11_BLEND_DESC desc;
			rs->OM.BlendState->GetDesc(&desc);
			
			ret.m_OM.m_BlendState.State = GetIDForResource(rs->OM.BlendState);

			ret.m_OM.m_BlendState.SampleMask = rs->OM.SampleMask;
			memcpy(ret.m_OM.m_BlendState.BlendFactor, rs->OM.BlendFactor, sizeof(FLOAT)*4);
			ret.m_OM.m_BlendState.AlphaToCoverage = desc.AlphaToCoverageEnable == TRUE;
			ret.m_OM.m_BlendState.IndependentBlend = desc.IndependentBlendEnable == TRUE;
			
#if defined(INCLUDE_D3D_11_1)
			bool state1 = false;
			D3D11_BLEND_DESC1 desc1;
			RDCEraseEl(desc1);

			if(WrappedID3D11BlendState1::IsAlloc(rs->OM.BlendState))
			{
				((ID3D11BlendState1 *)rs->OM.BlendState)->GetDesc1(&desc1);

				state1 = true;
			}
#endif

			create_array_uninit(ret.m_OM.m_BlendState.Blends, 8);
			for(size_t i=0; i < 8; i++)
			{
				D3D11PipelineState::OutputMerger::BlendState::RTBlend &blend = ret.m_OM.m_BlendState.Blends[i];

				blend.Enabled = desc.RenderTarget[i].BlendEnable == TRUE;

#if defined(INCLUDE_D3D_11_1)
				blend.LogicEnabled = state1 && desc1.RenderTarget[i].LogicOpEnable == TRUE;
				blend.LogicOp = state1 ? widen(ToStr::Get(desc1.RenderTarget[i].LogicOp)) : widen(ToStr::Get(D3D11_LOGIC_OP_NOOP));
#else
				blend.LogicEnabled = false;
				blend.LogicOp = L"NOOP";
#endif

				blend.m_AlphaBlend.Source = widen(ToStr::Get(desc.RenderTarget[i].SrcBlendAlpha));
				blend.m_AlphaBlend.Destination = widen(ToStr::Get(desc.RenderTarget[i].DestBlendAlpha));
				blend.m_AlphaBlend.Operation = widen(ToStr::Get(desc.RenderTarget[i].BlendOpAlpha));

				blend.m_Blend.Source = widen(ToStr::Get(desc.RenderTarget[i].SrcBlend));
				blend.m_Blend.Destination = widen(ToStr::Get(desc.RenderTarget[i].DestBlend));
				blend.m_Blend.Operation = widen(ToStr::Get(desc.RenderTarget[i].BlendOp));

				blend.WriteMask = desc.RenderTarget[i].RenderTargetWriteMask;
			}
		}
		else
		{
			ret.m_OM.m_BlendState.State = ResourceId();

			ret.m_OM.m_BlendState.SampleMask = ~0U;
			ret.m_OM.m_BlendState.BlendFactor[0] = ret.m_OM.m_BlendState.BlendFactor[1] =
				ret.m_OM.m_BlendState.BlendFactor[2] = ret.m_OM.m_BlendState.BlendFactor[3] = 1.0f;
			ret.m_OM.m_BlendState.AlphaToCoverage = false;
			ret.m_OM.m_BlendState.IndependentBlend = false;
			
			D3D11PipelineState::OutputMerger::BlendState::RTBlend blend;
			
			blend.Enabled = false;

			blend.m_AlphaBlend.Source = widen(ToStr::Get(D3D11_BLEND_ONE));
			blend.m_AlphaBlend.Destination = widen(ToStr::Get(D3D11_BLEND_ZERO));
			blend.m_AlphaBlend.Operation = widen(ToStr::Get(D3D11_BLEND_OP_ADD));

			blend.m_Blend.Source = widen(ToStr::Get(D3D11_BLEND_ONE));
			blend.m_Blend.Destination = widen(ToStr::Get(D3D11_BLEND_ZERO));
			blend.m_Blend.Operation = widen(ToStr::Get(D3D11_BLEND_OP_ADD));

#if defined(INCLUDE_D3D_11_1)
			blend.LogicEnabled = false;
			blend.LogicOp = widen(ToStr::Get(D3D11_LOGIC_OP_NOOP));
#else
			blend.LogicEnabled = false;
			blend.LogicOp = L"NOOP";
#endif

			blend.WriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				
			create_array_uninit(ret.m_OM.m_BlendState.Blends, 8);
			for(size_t i=0; i < 8; i++)
				ret.m_OM.m_BlendState.Blends[i] = blend;
		}

		if(rs->OM.DepthStencilState)
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			rs->OM.DepthStencilState->GetDesc(&desc);

			ret.m_OM.m_State.DepthEnable = desc.DepthEnable == TRUE;
			ret.m_OM.m_State.DepthFunc = widen(ToStr::Get(desc.DepthFunc));
			ret.m_OM.m_State.DepthWrites = desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
			ret.m_OM.m_State.StencilEnable = desc.StencilEnable == TRUE;
			ret.m_OM.m_State.StencilRef = rs->OM.StencRef;
			ret.m_OM.m_State.StencilReadMask = desc.StencilReadMask;
			ret.m_OM.m_State.StencilWriteMask = desc.StencilWriteMask;
			ret.m_OM.m_State.State = rm->GetOriginalID(GetIDForResource(rs->OM.DepthStencilState));

			ret.m_OM.m_State.m_FrontFace.Func = widen(ToStr::Get(desc.FrontFace.StencilFunc));
			ret.m_OM.m_State.m_FrontFace.DepthFailOp = widen(ToStr::Get(desc.FrontFace.StencilDepthFailOp));
			ret.m_OM.m_State.m_FrontFace.PassOp = widen(ToStr::Get(desc.FrontFace.StencilPassOp));
			ret.m_OM.m_State.m_FrontFace.FailOp = widen(ToStr::Get(desc.FrontFace.StencilFailOp));

			ret.m_OM.m_State.m_BackFace.Func = widen(ToStr::Get(desc.BackFace.StencilFunc));
			ret.m_OM.m_State.m_BackFace.DepthFailOp = widen(ToStr::Get(desc.BackFace.StencilDepthFailOp));
			ret.m_OM.m_State.m_BackFace.PassOp = widen(ToStr::Get(desc.BackFace.StencilPassOp));
			ret.m_OM.m_State.m_BackFace.FailOp = widen(ToStr::Get(desc.BackFace.StencilFailOp));
		}
		else
		{

			ret.m_OM.m_State.DepthEnable = true;
			ret.m_OM.m_State.DepthFunc = widen(ToStr::Get(D3D11_COMPARISON_LESS));
			ret.m_OM.m_State.DepthWrites = true;
			ret.m_OM.m_State.StencilEnable = false;
			ret.m_OM.m_State.StencilRef = rs->OM.StencRef;
			ret.m_OM.m_State.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
			ret.m_OM.m_State.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
			ret.m_OM.m_State.State = ResourceId();

			ret.m_OM.m_State.m_FrontFace.Func = widen(ToStr::Get(D3D11_COMPARISON_ALWAYS));
			ret.m_OM.m_State.m_FrontFace.DepthFailOp = widen(ToStr::Get(D3D11_STENCIL_OP_KEEP));
			ret.m_OM.m_State.m_FrontFace.PassOp = widen(ToStr::Get(D3D11_STENCIL_OP_KEEP));
			ret.m_OM.m_State.m_FrontFace.FailOp = widen(ToStr::Get(D3D11_STENCIL_OP_KEEP));

			ret.m_OM.m_State.m_BackFace.Func = widen(ToStr::Get(D3D11_COMPARISON_ALWAYS));
			ret.m_OM.m_State.m_BackFace.DepthFailOp = widen(ToStr::Get(D3D11_STENCIL_OP_KEEP));
			ret.m_OM.m_State.m_BackFace.PassOp = widen(ToStr::Get(D3D11_STENCIL_OP_KEEP));
			ret.m_OM.m_State.m_BackFace.FailOp = widen(ToStr::Get(D3D11_STENCIL_OP_KEEP));
		}
	}

	return ret;
}

void D3D11Replay::ReadLogInitialisation()
{
	m_pDevice->ReadLogInitialisation();
}

void D3D11Replay::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	m_pDevice->SetContextFilter(id, firstDefEv, lastDefEv);
}

void D3D11Replay::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	m_pDevice->ReplayLog(frameID, startEventID, endEventID, replayType);
}


uint64_t D3D11Replay::MakeOutputWindow(void *w, bool depth)
{
	return m_pDevice->GetDebugManager()->MakeOutputWindow(w, depth);
}

void D3D11Replay::DestroyOutputWindow(uint64_t id)
{
	m_pDevice->GetDebugManager()->DestroyOutputWindow(id);
}

bool D3D11Replay::CheckResizeOutputWindow(uint64_t id)
{
	return m_pDevice->GetDebugManager()->CheckResizeOutputWindow(id);
}

void D3D11Replay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
	m_pDevice->GetDebugManager()->GetOutputWindowDimensions(id, w, h);
}

void D3D11Replay::ClearOutputWindowColour(uint64_t id, float col[4])
{
	m_pDevice->GetDebugManager()->ClearOutputWindowColour(id, col);
}

void D3D11Replay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	m_pDevice->GetDebugManager()->ClearOutputWindowDepth(id, depth, stencil);
}

void D3D11Replay::BindOutputWindow(uint64_t id, bool depth)
{
	m_pDevice->GetDebugManager()->BindOutputWindow(id, depth);
}

bool D3D11Replay::IsOutputWindowVisible(uint64_t id)
{
	return m_pDevice->GetDebugManager()->IsOutputWindowVisible(id);
}

void D3D11Replay::FlipOutputWindow(uint64_t id)
{
	m_pDevice->GetDebugManager()->FlipOutputWindow(id);
}

void D3D11Replay::InitPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	m_pDevice->GetDebugManager()->InitPostVSBuffers(frameID, eventID);
}

ResourceId D3D11Replay::GetLiveID(ResourceId id)
{
	return m_pDevice->GetResourceManager()->GetLiveID(id);
}
		
bool D3D11Replay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, float *minval, float *maxval)
{
	return m_pDevice->GetDebugManager()->GetMinMax(texid, sliceFace, mip, minval, maxval);
}

bool D3D11Replay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
{
	return m_pDevice->GetDebugManager()->GetHistogram(texid, sliceFace, mip, minval, maxval, channels, histogram);
}

PostVSMeshData D3D11Replay::GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage)
{
	return m_pDevice->GetDebugManager()->GetPostVSBuffers(frameID, eventID, stage);
}

vector<byte> D3D11Replay::GetBufferData(ResourceId buff, uint32_t offset, uint32_t len)
{
	return m_pDevice->GetDebugManager()->GetBufferData(buff, offset, len);
}

byte *D3D11Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, size_t &dataSize)
{
	return m_pDevice->GetDebugManager()->GetTextureData(tex, arrayIdx, mip, dataSize);
}

void D3D11Replay::ReplaceResource(ResourceId from, ResourceId to)
{
	m_pDevice->GetResourceManager()->ReplaceResource(from, to);
}

void D3D11Replay::RemoveReplacement(ResourceId id)
{
	m_pDevice->GetResourceManager()->RemoveReplacement(id);
}

void D3D11Replay::TimeDrawcalls(rdctype::array<FetchDrawcall> &arr)
{
	return m_pDevice->GetDebugManager()->TimeDrawcalls(arr);
}

bool D3D11Replay::SaveTexture(ResourceId tex, uint32_t saveMip, wstring path)
{
	return m_pDevice->GetDebugManager()->SaveTexture(tex, saveMip, path);
}

void D3D11Replay::RenderMesh(int frameID, vector<int> eventID, MeshDisplay cfg)
{
	return m_pDevice->GetDebugManager()->RenderMesh(frameID, eventID, cfg);
}

void D3D11Replay::BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	m_pDevice->GetDebugManager()->BuildShader(source, entry, D3DCOMPILE_DEBUG|compileFlags, type, id, errors);
}

void D3D11Replay::BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	m_pDevice->GetDebugManager()->BuildShader(source, entry, compileFlags, type, id, errors);
}

bool D3D11Replay::RenderTexture(TextureDisplay cfg)
{
	return m_pDevice->GetDebugManager()->RenderTexture(cfg);
}

void D3D11Replay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
	m_pDevice->GetDebugManager()->RenderCheckerboard(light, dark);
}

void D3D11Replay::RenderHighlightBox(float w, float h, float scale)
{
	m_pDevice->GetDebugManager()->RenderHighlightBox(w, h, scale);
}

void D3D11Replay::FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	auto it = WrappedShader::m_ShaderList.find(shader);

	if(it == WrappedShader::m_ShaderList.end())
		return;

	RDCASSERT(it->second.m_DXBCFile);

	DXBC::DXBCFile *dxbc = it->second.m_DXBCFile;

	if(cbufSlot < dxbc->m_CBuffers.size())
		m_pDevice->GetDebugManager()->FillCBufferVariables(dxbc->m_CBuffers[cbufSlot].variables, outvars, false, data);
	return;
}

vector<PixelModification> D3D11Replay::PixelHistory(uint32_t frameID, vector<uint32_t> events, ResourceId target, uint32_t x, uint32_t y)
{
	return m_pDevice->GetDebugManager()->PixelHistory(frameID, events, target, x, y);
}

ShaderDebugTrace D3D11Replay::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	return m_pDevice->GetDebugManager()->DebugVertex(frameID, eventID, vertid, instid, idx, instOffset, vertOffset);
}

ShaderDebugTrace D3D11Replay::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y)
{
	return m_pDevice->GetDebugManager()->DebugPixel(frameID, eventID, x, y);
}

ShaderDebugTrace D3D11Replay::DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	return m_pDevice->GetDebugManager()->DebugThread(frameID, eventID, groupid, threadid);
}

void D3D11Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, float pixel[4])
{
	m_pDevice->GetDebugManager()->PickPixel(texture, x, y, sliceFace, mip, pixel);
}

ResourceId D3D11Replay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID)
{
	return m_pDevice->GetDebugManager()->RenderOverlay(texid, overlay, frameID, eventID);
}

ResourceId D3D11Replay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
{
	return m_pDevice->GetDebugManager()->ApplyCustomShader(shader, texid, mip);
}

bool D3D11Replay::IsRenderOutput(ResourceId id)
{
	for(size_t i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		if(m_CurPipelineState.m_OM.RenderTargets[i].View == id ||
			 m_CurPipelineState.m_OM.RenderTargets[i].Resource == id)
				return true;
	}
	
	if(m_CurPipelineState.m_OM.DepthTarget.View == id ||
		 m_CurPipelineState.m_OM.DepthTarget.Resource == id)
			return true;

	return false;
}

void D3D11Replay::InitCallstackResolver()
{
	m_pDevice->GetSerialiser()->InitCallstackResolver();
}

bool D3D11Replay::HasCallstacks()
{
	return m_pDevice->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *D3D11Replay::GetCallstackResolver()
{
	return m_pDevice->GetSerialiser()->GetCallstackResolver();
}

ResourceId D3D11Replay::CreateProxyTexture(FetchTexture templateTex)
{
	ResourceId ret;

	if(templateTex.dimension == 1)
	{
		ID3D11Texture1D *throwaway = NULL;
		D3D11_TEXTURE1D_DESC desc;

		desc.ArraySize = templateTex.arraysize;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		if(templateTex.creationFlags & eTextureCreate_DSV)
			desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

		desc.CPUAccessFlags = 0;
		desc.Format = MakeDXGIFormat(templateTex.format);
		desc.MipLevels = templateTex.mips;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Width = templateTex.width;

		HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &throwaway);
		if(FAILED(hr))
		{
			RDCERR("Failed to create 1D proxy texture");
			return ResourceId();
		}
		
		if(templateTex.creationFlags & eTextureCreate_DSV)
			desc.Format = GetTypelessFormat(desc.Format);

		ret = ((WrappedID3D11Texture1D *)throwaway)->GetResourceID();

		if(templateTex.creationFlags & eTextureCreate_DSV)
			WrappedID3D11Texture1D::m_TextureList[ret].m_Type = TEXDISPLAY_DEPTH_TARGET;
	}
	else if(templateTex.dimension == 2)
	{
		ID3D11Texture2D *throwaway = NULL;
		D3D11_TEXTURE2D_DESC desc;
		
		desc.ArraySize = templateTex.arraysize;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		desc.CPUAccessFlags = 0;
		desc.Format = MakeDXGIFormat(templateTex.format);
		desc.MipLevels = templateTex.mips;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Width = templateTex.width;
		desc.Height = templateTex.height;
		desc.SampleDesc.Count = templateTex.msSamp;
		desc.SampleDesc.Quality = templateTex.msQual;
		
		if(templateTex.creationFlags & eTextureCreate_DSV)
		{
			desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
			desc.Format = GetTypelessFormat(desc.Format);
		}

		HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &throwaway);
		if(FAILED(hr))
		{
			RDCERR("Failed to create 2D proxy texture");
			return ResourceId();
		}

		ret = ((WrappedID3D11Texture2D *)throwaway)->GetResourceID();
		if(templateTex.creationFlags & eTextureCreate_DSV)
			WrappedID3D11Texture2D::m_TextureList[ret].m_Type = TEXDISPLAY_DEPTH_TARGET;
	}
	else if(templateTex.dimension == 3)
	{
		ID3D11Texture3D *throwaway = NULL;
		D3D11_TEXTURE3D_DESC desc;
		
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		if(templateTex.creationFlags & eTextureCreate_DSV)
			desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

		desc.CPUAccessFlags = 0;
		desc.Format = MakeDXGIFormat(templateTex.format);
		desc.MipLevels = templateTex.mips;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Width = templateTex.width;
		desc.Height = templateTex.height;
		desc.Depth = templateTex.depth;

		HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &throwaway);
		if(FAILED(hr))
		{
			RDCERR("Failed to create 3D proxy texture");
			return ResourceId();
		}

		ret = ((WrappedID3D11Texture3D *)throwaway)->GetResourceID();
	}
	else
	{
		RDCERR("Invalid texture dimension: %d", templateTex.dimension);
	}

	return ret;
}

void D3D11Replay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
{
	if(texid == ResourceId()) return;

	ID3D11DeviceContext *ctx = m_pDevice->GetImmediateContext()->GetReal();

	if(WrappedID3D11Texture1D::m_TextureList.find(texid) != WrappedID3D11Texture1D::m_TextureList.end())
	{
		WrappedID3D11Texture1D *tex = (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[texid].m_Texture;

		D3D11_TEXTURE1D_DESC desc;
		tex->GetDesc(&desc);
		
		uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);
		
		if(mip >= mips || arrayIdx >= desc.ArraySize)
		{
			RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
			return;
		}

		uint32_t sub = arrayIdx*mips + mip;

		if(dataSize < GetByteSize(desc.Width, 1, 1, desc.Format, mip))
		{
			RDCERR("Insufficient data provided to SetProxyTextureData");
			return;
		}

		ctx->UpdateSubresource(tex->GetReal(), sub, NULL, data,
													 GetByteSize(desc.Width, 1, 1, desc.Format, mip),
													 GetByteSize(desc.Width, 1, 1, desc.Format, mip));
	}
	else if(WrappedID3D11Texture2D::m_TextureList.find(texid) != WrappedID3D11Texture2D::m_TextureList.end())
	{
		WrappedID3D11Texture2D *tex = (WrappedID3D11Texture2D *)WrappedID3D11Texture2D::m_TextureList[texid].m_Texture;

		D3D11_TEXTURE2D_DESC desc;
		tex->GetDesc(&desc);
		
		uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);
		
		if(mip >= mips || arrayIdx >= desc.ArraySize)
		{
			RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
			return;
		}

		uint32_t sub = arrayIdx*mips + mip;

		if(dataSize < GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip))
		{
			RDCERR("Insufficient data provided to SetProxyTextureData");
			return;
		}

		ctx->UpdateSubresource(tex->GetReal(), sub, NULL, data,
													 GetByteSize(desc.Width, 1, 1, desc.Format, mip),
													 GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
	}
	else if(WrappedID3D11Texture3D::m_TextureList.find(texid) != WrappedID3D11Texture3D::m_TextureList.end())
	{
		WrappedID3D11Texture3D *tex = (WrappedID3D11Texture3D *)WrappedID3D11Texture3D::m_TextureList[texid].m_Texture;

		D3D11_TEXTURE3D_DESC desc;
		tex->GetDesc(&desc);
		
		uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);
		
		if(mip >= mips)
		{
			RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
			return;
		}

		if(dataSize < GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip))
		{
			RDCERR("Insufficient data provided to SetProxyTextureData");
			return;
		}
		
		ctx->UpdateSubresource(tex->GetReal(), mip, NULL, data,
													 GetByteSize(desc.Width, 1, 1, desc.Format, mip),
													 GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
	}
	else
	{
		RDCERR("Invalid texture id passed to SetProxyTextureData");
	}
}

ReplayCreateStatus D3D11_CreateReplayDevice(const wchar_t *logfile, IReplayDriver **driver)
{
	RDCDEBUG("Creating a D3D11 replay device");

	HMODULE lib = NULL;
	lib = LoadLibraryA("d3d11.dll");
	if(lib == NULL)
	{
		RDCERR("Failed to load d3d11.dll");
		return eReplayCreate_APIInitFailed;
	}

	lib = LoadLibraryA("d3d9.dll");
	if(lib == NULL)
	{
		RDCERR("Failed to load d3d9.dll");
		return eReplayCreate_APIInitFailed;
	}

	lib = LoadLibraryA("dxgi.dll");
	if(lib == NULL)
	{
		RDCERR("Failed to load dxgi.dll");
		return eReplayCreate_APIInitFailed;
	}

	if(GetD3DCompiler() == NULL)
	{
		RDCERR("Failed to load d3dcompiler_??.dll");
		return eReplayCreate_APIInitFailed;
	}

	typedef HRESULT (__cdecl* PFN_RENDERDOC_CREATE_DEVICE_AND_SWAP_CHAIN)( __in_opt IDXGIAdapter*, 
		D3D_DRIVER_TYPE, HMODULE, UINT, 
		__in_ecount_opt( FeatureLevels ) CONST D3D_FEATURE_LEVEL*, 
		UINT FeatureLevels, UINT, __in_opt CONST DXGI_SWAP_CHAIN_DESC*, 
		__out_opt IDXGISwapChain**, __out_opt ID3D11Device**, 
		__out_opt D3D_FEATURE_LEVEL*, __out_opt ID3D11DeviceContext** );

	PFN_RENDERDOC_CREATE_DEVICE_AND_SWAP_CHAIN createDevice = (PFN_RENDERDOC_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(GetModuleHandleA("renderdoc.dll"), "RENDERDOC_CreateWrappedD3D11DeviceAndSwapChain");

	RDCASSERT(createDevice);

	ID3D11Device *device = NULL;

	D3D11InitParams initParams;
	RDCDriver driverFileType = RDC_D3D11;
	wstring driverName = L"D3D11";
	if(logfile)
	{
		auto status = RenderDoc::Inst().FillInitParams(logfile, driverFileType, driverName, (RDCInitParams *)&initParams);
		if(status != eReplayCreate_Success)
			return status;
	}

	if(initParams.SerialiseVersion != D3D11InitParams::D3D11_SERIALISE_VERSION)
	{
		RDCERR("Incompatible D3D11 serialise version, expected %d got %d", D3D11InitParams::D3D11_SERIALISE_VERSION, initParams.SerialiseVersion);
		return eReplayCreate_APIIncompatibleVersion;
	}

	if(initParams.SDKVersion != D3D11_SDK_VERSION)
	{
		RDCWARN("Capture file used a different SDK version %lu from replay app %lu. Results may be undefined", initParams.SDKVersion, D3D11_SDK_VERSION);
	}
	
	if(initParams.DriverType == D3D_DRIVER_TYPE_UNKNOWN)
		initParams.DriverType = D3D_DRIVER_TYPE_HARDWARE;

	int i=-2;

	// force using our feature levels as we require >= 11_0 for analysis
#if defined(INCLUDE_D3D_11_1)
	D3D_FEATURE_LEVEL featureLevelArray11_1[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
#else
	D3D_FEATURE_LEVEL featureLevelArray11_1[] = { D3D_FEATURE_LEVEL_11_0 };
#endif
	UINT numFeatureLevels11_1 = ARRAY_COUNT(featureLevelArray11_1);

	D3D_FEATURE_LEVEL featureLevelArray11_0[] = { D3D_FEATURE_LEVEL_11_0 };
	UINT numFeatureLevels11_0 = ARRAY_COUNT(featureLevelArray11_0);

	D3D_DRIVER_TYPE driverTypes[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE };
	int numDrivers = ARRAY_COUNT(driverTypes);

	D3D_FEATURE_LEVEL *featureLevelArray = featureLevelArray11_1;
	UINT numFeatureLevels = numFeatureLevels11_1;
	D3D_DRIVER_TYPE driverType = initParams.DriverType;
	UINT flags = initParams.Flags;

	HRESULT hr = E_FAIL;

	D3D_FEATURE_LEVEL maxFeatureLevel = D3D_FEATURE_LEVEL_9_1;
	
	// check for feature level 11 support - passing NULL feature level array implicitly checks for 11_0 before others
	hr = createDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL, NULL, &maxFeatureLevel, NULL);

	if(SUCCEEDED(hr) && maxFeatureLevel < D3D_FEATURE_LEVEL_11_0)
	{
		RDCERR("Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 availability");
		return eReplayCreate_APIHardwareUnsupported;
	}

	hr = E_FAIL;
	while(1)
	{
		hr = createDevice(
			/*pAdapter=*/NULL, driverType, /*Software=*/NULL, flags,
			/*pFeatureLevels=*/featureLevelArray, /*nFeatureLevels=*/numFeatureLevels, D3D11_SDK_VERSION,
			/*pSwapChainDesc=*/NULL, (IDXGISwapChain **)NULL, (ID3D11Device **)&device, (D3D_FEATURE_LEVEL*)NULL, (ID3D11DeviceContext **)NULL);

		if(SUCCEEDED(hr))
		{
			if(logfile)	((WrappedID3D11Device *)device)->SetLogFile(logfile);

			RDCLOG("Created device.");
			D3D11Replay *replay = ((WrappedID3D11Device *)device)->GetReplay();

			replay->SetProxy(logfile == NULL);

			*driver = (IReplayDriver *)replay;
			return eReplayCreate_Success;
		}

		if(i == -1)
		{
			RDCWARN("Couldn't create device with similar settings to capture.");
		}

		SAFE_RELEASE(device);

		i++;

		if(i >= numDrivers*2)
			break;

		if(i >= 0)
			initParams.DriverType = driverTypes[i/2];

		if(i % 2 == 0)
		{
			featureLevelArray = featureLevelArray11_1;
			numFeatureLevels = numFeatureLevels11_1;
		}
		else
		{
			featureLevelArray = featureLevelArray11_0;
			numFeatureLevels = numFeatureLevels11_0;
		}
	}

	RDCERR("Couldn't create any compatible d3d11 device :(.");

	return eReplayCreate_APIHardwareUnsupported;
}

static DriverRegistration D3D11DriverRegistration(RDC_D3D11, L"D3D11", &D3D11_CreateReplayDevice);
