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


#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_resources.h"

D3D11RenderState::D3D11RenderState(Serialiser *ser)
{
	RDCEraseEl(IA);
	RDCEraseEl(VS);
	RDCEraseEl(HS);
	RDCEraseEl(DS);
	RDCEraseEl(GS);
	RDCEraseEl(SO);
	RDCEraseEl(RS);
	RDCEraseEl(PS);
	RDCEraseEl(OM);
	RDCEraseEl(CS);
	Clear();
	m_pSerialiser = ser;

	m_ImmediatePipeline = false;
	m_pDevice = NULL;
}

D3D11RenderState::D3D11RenderState(const D3D11RenderState &other)
{
	RDCEraseEl(IA);
	RDCEraseEl(VS);
	RDCEraseEl(HS);
	RDCEraseEl(DS);
	RDCEraseEl(GS);
	RDCEraseEl(SO);
	RDCEraseEl(RS);
	RDCEraseEl(PS);
	RDCEraseEl(OM);
	RDCEraseEl(CS);
	*this = other;

	m_ImmediatePipeline = false;
	m_pDevice = NULL;
}

D3D11RenderState &D3D11RenderState::operator =(const D3D11RenderState &other)
{
	ReleaseRefs();

	memcpy(&IA, &other.IA, sizeof(IA));
	memcpy(&VS, &other.VS, sizeof(VS));
	memcpy(&HS, &other.HS, sizeof(HS));
	memcpy(&DS, &other.DS, sizeof(DS));
	memcpy(&GS, &other.GS, sizeof(GS));
	memcpy(&SO, &other.SO, sizeof(SO));
	memcpy(&RS, &other.RS, sizeof(RS));
	memcpy(&PS, &other.PS, sizeof(PS));
	memcpy(&OM, &other.OM, sizeof(OM));
	memcpy(&CS, &other.CS, sizeof(CS));

	m_ImmediatePipeline = false;
	m_pDevice = NULL;

	AddRefs();

	return *this;
}

D3D11RenderState::~D3D11RenderState()
{
	ReleaseRefs();
}

void D3D11RenderState::ReleaseRefs()
{
	ReleaseRef(IA.IndexBuffer);
	ReleaseRef(IA.Layout);
	
	for(UINT i=0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
		ReleaseRef(IA.VBs[i]);

	shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		ReleaseRef(sh->Shader);

		for(UINT i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
			ReleaseRef(sh->ConstantBuffers[i]);
		
		for(UINT i=0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
			ReleaseRef(sh->Samplers[i]);
		
		for(UINT i=0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
			ReleaseRef(sh->SRVs[i]);
		
		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
			ReleaseRef(sh->UAVs[i]);
		
		for(UINT i=0; i < D3D11_SHADER_MAX_INTERFACES; i++)
			ReleaseRef(sh->Instances[i]);
		
		sh++;
	}
	
	for(UINT i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
		ReleaseRef(SO.Buffers[i]);

	ReleaseRef(RS.State);

	ReleaseRef(OM.BlendState);
	ReleaseRef(OM.DepthStencilState);

	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		ReleaseRef(OM.RenderTargets[i]);

	for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		ReleaseRef(OM.UAVs[i]);

	ReleaseRef(OM.DepthView);
	
	RDCEraseEl(IA);
	RDCEraseEl(VS);
	RDCEraseEl(HS);
	RDCEraseEl(DS);
	RDCEraseEl(GS);
	RDCEraseEl(SO);
	RDCEraseEl(RS);
	RDCEraseEl(PS);
	RDCEraseEl(OM);
	RDCEraseEl(CS);
}

void D3D11RenderState::MarkDirty(D3D11ResourceManager *manager) const
{
	const shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		{
			ID3D11Resource *res = NULL;
			if(sh->UAVs[i])
			{
				sh->UAVs[i]->GetResource(&res);
				manager->MarkDirtyResource(GetIDForResource(res));
				SAFE_RELEASE(res);
			}
		}

		sh++;
	}
		
	for(UINT i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
		manager->MarkDirtyResource(GetIDForResource(SO.Buffers[i]));

	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		ID3D11Resource *res = NULL;
		if(OM.RenderTargets[i])
		{
			OM.RenderTargets[i]->GetResource(&res);
			manager->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}
	}

	for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		ID3D11Resource *res = NULL;
		if(OM.UAVs[i])
		{
			OM.UAVs[i]->GetResource(&res);
			manager->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}
	}

	{
		ID3D11Resource *res = NULL;
		if(OM.DepthView)
		{
			OM.DepthView->GetResource(&res);
			manager->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}
	}
}

void D3D11RenderState::MarkReferenced(WrappedID3D11DeviceContext *ctx, bool initial) const
{
	ctx->MarkResourceReferenced(GetIDForResource(IA.IndexBuffer), initial ? eFrameRef_Unknown : eFrameRef_Read);
	
	for(UINT i=0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
		ctx->MarkResourceReferenced(GetIDForResource(IA.VBs[i]), initial ? eFrameRef_Unknown : eFrameRef_Read);

	const shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		ctx->MarkResourceReferenced(GetIDForResource(sh->Shader), initial ? eFrameRef_Unknown : eFrameRef_Read);

		for(UINT i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
			ctx->MarkResourceReferenced(GetIDForResource(sh->ConstantBuffers[i]), initial ? eFrameRef_Unknown : eFrameRef_Read);

		for(UINT i=0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
		{
			ID3D11Resource *res = NULL;
			if(sh->SRVs[i])
			{
				sh->SRVs[i]->GetResource(&res);
				ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Read);
				SAFE_RELEASE(res);
			}
		}

		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		{
			ID3D11Resource *res = NULL;
			if(sh->UAVs[i])
			{
				sh->UAVs[i]->GetResource(&res);
				ctx->m_MissingTracks.insert(GetIDForResource(res));
				ctx->m_MissingTracks.insert(GetIDForResource(sh->UAVs[i]));
				// UAVs we always assume to be partial updates
				ctx->MarkResourceReferenced(GetIDForResource(sh->UAVs[i]), initial ? eFrameRef_Unknown : eFrameRef_Read);
				ctx->MarkResourceReferenced(GetIDForResource(sh->UAVs[i]), initial ? eFrameRef_Unknown : eFrameRef_Write);
				ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Read);
				ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Write);
				SAFE_RELEASE(res);
			}
		}

		sh++;
	}
		
	for(UINT i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
		ctx->MarkResourceReferenced(GetIDForResource(SO.Buffers[i]), initial ? eFrameRef_Unknown : eFrameRef_Write);

	// tracks the min region of the enabled viewports plus scissors, to see if we could potentially
	// partially-update a render target (ie. we know for sure that we are only
	// writing to a region in one of the viewports). In this case we mark the
	// RT/DSV as read-write instead of just write, for initial state tracking.
	D3D11_RECT viewportScissorMin = { 0, 0, 0xfffffff, 0xfffffff };

	D3D11_RASTERIZER_DESC rsdesc;
	RDCEraseEl(rsdesc);
	rsdesc.ScissorEnable = FALSE;
	if(RS.State)
		RS.State->GetDesc(&rsdesc);

	for(UINT v=0; v < RS.NumViews; v++)
	{
		D3D11_RECT scissor = { (LONG)RS.Viewports[v].TopLeftX,
								(LONG)RS.Viewports[v].TopLeftY,
								(LONG)RS.Viewports[v].Width,
								(LONG)RS.Viewports[v].Height };

		// scissor (if set) is relative to matching viewport)
		if(v < RS.NumScissors && rsdesc.ScissorEnable)
		{
			scissor.left += RS.Scissors[v].left;
			scissor.top += RS.Scissors[v].top;
			scissor.right = RDCMIN(scissor.right, RS.Scissors[v].right-RS.Scissors[v].left);
			scissor.bottom = RDCMIN(scissor.bottom, RS.Scissors[v].bottom-RS.Scissors[v].top);
		}

		viewportScissorMin.left = RDCMAX(viewportScissorMin.left, scissor.left);
		viewportScissorMin.top = RDCMAX(viewportScissorMin.top, scissor.top);
		viewportScissorMin.right = RDCMIN(viewportScissorMin.right, scissor.right);
		viewportScissorMin.bottom = RDCMIN(viewportScissorMin.bottom, scissor.bottom);
	}

	bool viewportScissorPartial = false;

	if(viewportScissorMin.left > 0 || viewportScissorMin.top > 0)
	{
		viewportScissorPartial = true;
	}
	else
	{
		ID3D11Resource *res = NULL;
		if(OM.RenderTargets[0])
			OM.RenderTargets[0]->GetResource(&res);
		else if(OM.DepthView)
			OM.DepthView->GetResource(&res);

		if(res)
		{
			D3D11_RESOURCE_DIMENSION dim;
			res->GetType(&dim);

			if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
			{
				// assume partial
				viewportScissorPartial = true;
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
			{
				D3D11_TEXTURE1D_DESC desc;
				((ID3D11Texture1D *)res)->GetDesc(&desc);

				if(viewportScissorMin.right < (LONG)desc.Width)
					viewportScissorPartial = true;
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
			{
				D3D11_TEXTURE2D_DESC desc;
				((ID3D11Texture2D *)res)->GetDesc(&desc);

				if(viewportScissorMin.right < (LONG)desc.Width || viewportScissorMin.bottom < (LONG)desc.Height)
					viewportScissorPartial = true;
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
			{
				D3D11_TEXTURE3D_DESC desc;
				((ID3D11Texture3D *)res)->GetDesc(&desc);

				if(viewportScissorMin.right < (LONG)desc.Width || viewportScissorMin.bottom < (LONG)desc.Height)
					viewportScissorPartial = true;
			}
		}

		SAFE_RELEASE(res);
	}

	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		ID3D11Resource *res = NULL;
		if(OM.RenderTargets[i])
		{
			OM.RenderTargets[i]->GetResource(&res);
			ctx->m_MissingTracks.insert(GetIDForResource(res));
			if(viewportScissorPartial)
				ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Read);
			ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Write);
			SAFE_RELEASE(res);
		}
	}

	for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		ID3D11Resource *res = NULL;
		if(OM.UAVs[i])
		{
			OM.UAVs[i]->GetResource(&res);
			ctx->m_MissingTracks.insert(GetIDForResource(OM.UAVs[i]));
			ctx->m_MissingTracks.insert(GetIDForResource(res));
			// UAVs we always assume to be partial updates
			ctx->MarkResourceReferenced(GetIDForResource(OM.UAVs[i]), initial ? eFrameRef_Unknown : eFrameRef_Read);
			ctx->MarkResourceReferenced(GetIDForResource(OM.UAVs[i]), initial ? eFrameRef_Unknown : eFrameRef_Write);
			ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Read);
			ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Write);
			SAFE_RELEASE(res);
		}
	}

	{
		ID3D11Resource *res = NULL;
		if(OM.DepthView)
		{
			OM.DepthView->GetResource(&res);
			ctx->m_MissingTracks.insert(GetIDForResource(res));
			if(viewportScissorPartial)
				ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Read);
			ctx->MarkResourceReferenced(GetIDForResource(res), initial ? eFrameRef_Unknown : eFrameRef_Write);
			SAFE_RELEASE(res);
		}
	}
}

void D3D11RenderState::AddRefs()
{
	TakeRef(IA.IndexBuffer);
	TakeRef(IA.Layout);
	
	for(UINT i=0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
		TakeRef(IA.VBs[i]);

	shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		TakeRef(sh->Shader);

		for(UINT i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
			TakeRef(sh->ConstantBuffers[i]);
		
		for(UINT i=0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
			TakeRef(sh->Samplers[i]);
		
		for(UINT i=0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
			TakeRef(sh->SRVs[i]);
		
		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
			TakeRef(sh->UAVs[i]);
		
		for(UINT i=0; i < D3D11_SHADER_MAX_INTERFACES; i++)
			TakeRef(sh->Instances[i]);
		
		sh++;
	}
		
	for(UINT i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
		TakeRef(SO.Buffers[i]);

	TakeRef(RS.State);

	TakeRef(OM.BlendState);
	TakeRef(OM.DepthStencilState);

	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		TakeRef(OM.RenderTargets[i]);

	for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		TakeRef(OM.UAVs[i]);

	TakeRef(OM.DepthView);
}

void D3D11RenderState::Serialise(LogState m_State, WrappedID3D11Device *device)
{
	SERIALISE_ELEMENT(ResourceId, IALayout, GetIDForResource(IA.Layout));
	if(m_State < WRITING)
	{
		if(device->GetResourceManager()->HasLiveResource(IALayout))
			IA.Layout = (ID3D11InputLayout *)device->GetResourceManager()->GetLiveResource(IALayout);
		else
			IA.Layout = NULL;
	}

	m_pSerialiser->Serialise("IA.Topo", IA.Topo);

	SERIALISE_ELEMENT(ResourceId, IAIndexBuffer, GetIDForResource(IA.IndexBuffer));
	if(m_State < WRITING)
	{
		if(device->GetResourceManager()->HasLiveResource(IAIndexBuffer))
			IA.IndexBuffer = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(IAIndexBuffer);
		else
			IA.IndexBuffer = NULL;
	}

	for(int i=0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		ResourceId VB;
		if(m_State >= WRITING) VB = GetIDForResource(IA.VBs[i]);
		m_pSerialiser->Serialise("IA.VBs", VB);
		if(m_State < WRITING)
		{
			if(device->GetResourceManager()->HasLiveResource(VB))
				IA.VBs[i] = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(VB);
			else
				IA.VBs[i] = NULL;
		}
	}

	m_pSerialiser->Serialise<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT>("IA.Strides", IA.Strides);
	m_pSerialiser->Serialise<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT>("IA.Offsets", IA.Offsets);
	m_pSerialiser->Serialise("IA.indexFormat", IA.IndexFormat);
	m_pSerialiser->Serialise("IA.indexOffset", IA.IndexOffset);

	const char *names[] = { "VS", "DS", "HS", "GS", "CS", "PS" };
	shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(sh->Shader));
		if(m_State < WRITING)
		{
			if(device->GetResourceManager()->HasLiveResource(Shader))
				sh->Shader = (ID3D11DeviceChild *)device->GetResourceManager()->GetLiveResource(Shader);
			else
				sh->Shader = NULL;
		}

		for(int i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		{
			ResourceId id;
			if(m_State >= WRITING) id = GetIDForResource(sh->ConstantBuffers[i]);
			m_pSerialiser->Serialise((string(names[s]) + ".ConstantBuffers").c_str(), id);
			if(m_State < WRITING)
			{
				if(device->GetResourceManager()->HasLiveResource(id))
					sh->ConstantBuffers[i] = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(id);
				else
					sh->ConstantBuffers[i] = NULL;
			}
			
			m_pSerialiser->Serialise((string(names[s]) + ".CBOffsets").c_str(), sh->CBOffsets[i]);
			m_pSerialiser->Serialise((string(names[s]) + ".CBCounts").c_str(), sh->CBCounts[i]);
		}

		for(int i=0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++)
		{
			ResourceId id;
			if(m_State >= WRITING) id = GetIDForResource(sh->Samplers[i]);
			m_pSerialiser->Serialise((string(names[s]) + ".Samplers").c_str(), id);
			if(m_State < WRITING)
			{
				if(device->GetResourceManager()->HasLiveResource(id)) 
					sh->Samplers[i] = (ID3D11SamplerState *)device->GetResourceManager()->GetLiveResource(id);
				else
					sh->Samplers[i] = NULL;
			}
		}

		for(int i=0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
		{
			ResourceId id;
			if(m_State >= WRITING) id = GetIDForResource(sh->SRVs[i]);
			m_pSerialiser->Serialise((string(names[s]) + ".SRVs").c_str(), id);
			if(m_State < WRITING)
			{
				if(device->GetResourceManager()->HasLiveResource(id)) 
					sh->SRVs[i] = (ID3D11ShaderResourceView *)device->GetResourceManager()->GetLiveResource(id);
				else
					sh->SRVs[i] = NULL;
			}
		}

		for(int i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		{
			ResourceId id;
			if(m_State >= WRITING) id = GetIDForResource(sh->UAVs[i]);
			m_pSerialiser->Serialise((string(names[s]) + ".UAVs").c_str(), id);
			if(m_State < WRITING)
			{
				if(device->GetResourceManager()->HasLiveResource(id)) 
					sh->UAVs[i] = (ID3D11UnorderedAccessView *)device->GetResourceManager()->GetLiveResource(id);
				else
					sh->UAVs[i] = NULL;
			}
		}
		
		for(int i=0; i < D3D11_SHADER_MAX_INTERFACES; i++)
		{
			ResourceId id;
			if(m_State >= WRITING) id = GetIDForResource(sh->Instances[i]);
			m_pSerialiser->Serialise((string(names[s]) + ".Instances").c_str(), id);
			if(m_State < WRITING)
			{
				if(device->GetResourceManager()->HasLiveResource(id)) 
					sh->Instances[i] = (ID3D11ClassInstance *)device->GetResourceManager()->GetLiveResource(id);
				else
					sh->Instances[i] = NULL;
			}
		}

		sh++;
	}

	for(int i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
	{
		ResourceId id;
		if(m_State >= WRITING) id = GetIDForResource(SO.Buffers[i]);
		m_pSerialiser->Serialise("SO.Buffers", id);
		if(m_State < WRITING)
		{
			if(device->GetResourceManager()->HasLiveResource(id)) 
				SO.Buffers[i] = (ID3D11Buffer *)device->GetResourceManager()->GetLiveResource(id);
			else
				SO.Buffers[i] = NULL;
		}

		m_pSerialiser->Serialise("SO.Offsets", SO.Offsets[i]);
	}

	SERIALISE_ELEMENT(ResourceId, RSState, GetIDForResource(RS.State));
	if(m_State < WRITING)
	{
		if(device->GetResourceManager()->HasLiveResource(RSState)) 
			RS.State = (ID3D11RasterizerState *)device->GetResourceManager()->GetLiveResource(RSState);
		else
			RS.State = NULL;
	}

	m_pSerialiser->Serialise("RS.NumViews", RS.NumViews);
	m_pSerialiser->Serialise("RS.NumScissors", RS.NumScissors);
	m_pSerialiser->Serialise<D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>("RS.Viewports", RS.Viewports);
	m_pSerialiser->Serialise<D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>("RS.Scissors", RS.Scissors);
	
	SERIALISE_ELEMENT(ResourceId, OMDepthStencilState, GetIDForResource(OM.DepthStencilState));
	if(m_State < WRITING)
	{
		if(device->GetResourceManager()->HasLiveResource(OMDepthStencilState)) 
			OM.DepthStencilState = (ID3D11DepthStencilState *)device->GetResourceManager()->GetLiveResource(OMDepthStencilState);
		else
			OM.DepthStencilState = NULL;
	}

	m_pSerialiser->Serialise("OM.StencRef", OM.StencRef);

	SERIALISE_ELEMENT(ResourceId, OMBlendState, GetIDForResource(OM.BlendState));
	if(m_State < WRITING)
	{
		if(device->GetResourceManager()->HasLiveResource(OMBlendState)) 
			OM.BlendState = (ID3D11BlendState *)device->GetResourceManager()->GetLiveResource(OMBlendState);
		else
			OM.BlendState = NULL;
	}

	m_pSerialiser->Serialise<4>("OM.BlendFactor", OM.BlendFactor);
	m_pSerialiser->Serialise("OM.SampleMask", OM.SampleMask);

	SERIALISE_ELEMENT(ResourceId, OMDepthView, GetIDForResource(OM.DepthView));
	if(m_State < WRITING)
	{
		if(device->GetResourceManager()->HasLiveResource(OMDepthView)) 
			OM.DepthView = (ID3D11DepthStencilView *)device->GetResourceManager()->GetLiveResource(OMDepthView);
		else
			OM.DepthView = NULL;
	}

	m_pSerialiser->Serialise("OM.UAVStartSlot", OM.UAVStartSlot);

	for(int i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		ResourceId UAV;
		if(m_State >= WRITING) UAV = GetIDForResource(OM.UAVs[i]);
		m_pSerialiser->Serialise("OM.UAVs", UAV);
		if(m_State < WRITING)
		{
			if(device->GetResourceManager()->HasLiveResource(UAV)) 
				OM.UAVs[i] = (ID3D11UnorderedAccessView *)device->GetResourceManager()->GetLiveResource(UAV);
			else
				OM.UAVs[i] = NULL;
		}
	}

	for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		ResourceId RTV;
		if(m_State >= WRITING) RTV = GetIDForResource(OM.RenderTargets[i]);
		m_pSerialiser->Serialise("OM.RenderTargets", RTV);
		if(m_State < WRITING)
		{
			if(device->GetResourceManager()->HasLiveResource(RTV)) 
				OM.RenderTargets[i] = (ID3D11RenderTargetView *)device->GetResourceManager()->GetLiveResource(RTV);
			else
				OM.RenderTargets[i] = NULL;
		}
	}

	if(m_State < WRITING)
		AddRefs();
}

D3D11RenderState::D3D11RenderState(WrappedID3D11DeviceContext *context)
{
	RDCEraseMem(this, sizeof(D3D11RenderState));
	m_pSerialiser = context->GetSerialiser();

	// IA
	context->IAGetInputLayout(&IA.Layout);
	context->IAGetPrimitiveTopology(&IA.Topo);
	context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, IA.VBs, (UINT*)IA.Strides, (UINT*)IA.Offsets);
	context->IAGetIndexBuffer(&IA.IndexBuffer, &IA.IndexFormat, &IA.IndexOffset);

	// VS
	context->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, VS.SRVs);
	context->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, VS.Samplers);
	context->VSGetShader((ID3D11VertexShader **)&VS.Shader, VS.Instances, &VS.NumInstances);

	// DS
	context->DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, DS.SRVs);
	context->DSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, DS.Samplers);
	context->DSGetShader((ID3D11DomainShader **)&DS.Shader, DS.Instances, &DS.NumInstances);

	// HS
	context->HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, HS.SRVs);
	context->HSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, HS.Samplers);
	context->HSGetShader((ID3D11HullShader **)&HS.Shader, HS.Instances, &HS.NumInstances);

	// GS
	context->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, GS.SRVs);
	context->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, GS.Samplers);
	context->GSGetShader((ID3D11GeometryShader **)&GS.Shader, GS.Instances, &GS.NumInstances);

	context->SOGetTargets(D3D11_SO_BUFFER_SLOT_COUNT, SO.Buffers);

	// RS
	context->RSGetState(&RS.State);
	RDCEraseEl(RS.Viewports);
	RS.NumViews = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	context->RSGetViewports(&RS.NumViews, RS.Viewports);
	RDCEraseEl(RS.Scissors);
	RS.NumScissors = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	context->RSGetScissorRects(&RS.NumScissors, RS.Scissors);

	// CS
	context->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, CS.SRVs);
	context->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, CS.UAVs);
	context->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, CS.Samplers);
	context->CSGetShader((ID3D11ComputeShader **)&CS.Shader, CS.Instances, &CS.NumInstances);

	// PS
	context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, PS.SRVs);
	context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, PS.Samplers);
	context->PSGetShader((ID3D11PixelShader **)&PS.Shader, PS.Instances, &PS.NumInstances);

#if defined(INCLUDE_D3D_11_1)
	context->VSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, VS.ConstantBuffers, VS.CBOffsets, VS.CBCounts);
	context->DSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, DS.ConstantBuffers, DS.CBOffsets, DS.CBCounts);
	context->HSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, HS.ConstantBuffers, HS.CBOffsets, HS.CBCounts);
	context->GSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, GS.ConstantBuffers, GS.CBOffsets, GS.CBCounts);
	context->CSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, CS.ConstantBuffers, CS.CBOffsets, CS.CBCounts);
	context->PSGetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, PS.ConstantBuffers, PS.CBOffsets, PS.CBCounts);
#else
	context->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, VS.ConstantBuffers);
	context->DSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, DS.ConstantBuffers);
	context->HSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, HS.ConstantBuffers);
	context->GSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, GS.ConstantBuffers);
	context->CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, CS.ConstantBuffers);
	context->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, PS.ConstantBuffers);

	RDCEraseEl(VS.CBOffsets);
	RDCEraseEl(DS.CBOffsets);
	RDCEraseEl(HS.CBOffsets);
	RDCEraseEl(GS.CBOffsets);
	RDCEraseEl(CS.CBOffsets);
	RDCEraseEl(PS.CBOffsets);
	
	RDCEraseEl(VS.CBCounts);
	RDCEraseEl(DS.CBCounts);
	RDCEraseEl(HS.CBCounts);
	RDCEraseEl(GS.CBCounts);
	RDCEraseEl(CS.CBCounts);
	RDCEraseEl(PS.CBCounts);
#endif

	// OM
	context->OMGetBlendState(&OM.BlendState, OM.BlendFactor, &OM.SampleMask);
	context->OMGetDepthStencilState(&OM.DepthStencilState, &OM.StencRef);

	ID3D11RenderTargetView* tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews, NULL);

	OM.UAVStartSlot = 0;
	for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		if(tmpViews[i] != NULL)
		{
			OM.UAVStartSlot = i+1;
			SAFE_RELEASE(tmpViews[i]);
		}
	}

	context->OMGetRenderTargetsAndUnorderedAccessViews(OM.UAVStartSlot, OM.RenderTargets,
														&OM.DepthView,
														OM.UAVStartSlot, D3D11_PS_CS_UAV_REGISTER_COUNT-OM.UAVStartSlot, OM.UAVs);
}

void D3D11RenderState::Clear()
{
	ReleaseRefs();
	OM.BlendFactor[0] = OM.BlendFactor[1] = OM.BlendFactor[2] = OM.BlendFactor[3] = 1.0f;
	OM.SampleMask = 0xffffffff;
}

void D3D11RenderState::ApplyState(WrappedID3D11DeviceContext *context)
{
	context->ClearState();

	// IA
	context->IASetInputLayout(IA.Layout);
	context->IASetPrimitiveTopology(IA.Topo);
	context->IASetIndexBuffer(IA.IndexBuffer, IA.IndexFormat, IA.IndexOffset);
	context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, IA.VBs, IA.Strides, IA.Offsets);

	// VS
	context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, VS.SRVs);
	context->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, VS.Samplers);
	context->VSSetShader((ID3D11VertexShader *)VS.Shader, VS.Instances, VS.NumInstances);

	// DS
	context->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, DS.SRVs);
	context->DSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, DS.Samplers);
	context->DSSetShader((ID3D11DomainShader *)DS.Shader, DS.Instances, DS.NumInstances);

	// HS
	context->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, HS.SRVs);
	context->HSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, HS.Samplers);
	context->HSSetShader((ID3D11HullShader *)HS.Shader, HS.Instances, HS.NumInstances);

	// GS
	context->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, GS.SRVs);
	context->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, GS.Samplers);
	context->GSSetShader((ID3D11GeometryShader *)GS.Shader, GS.Instances, GS.NumInstances);

	context->SOSetTargets(D3D11_SO_BUFFER_SLOT_COUNT, SO.Buffers, SO.Offsets);

	// RS
	context->RSSetState(RS.State);
	context->RSSetViewports(RS.NumViews, RS.Viewports);
	context->RSSetScissorRects(RS.NumScissors, RS.Scissors);

	UINT UAV_keepcounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = { (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1 };

	// CS
	context->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, CS.SRVs);
	context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, CS.UAVs, UAV_keepcounts);
	context->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, CS.Samplers);
	context->CSSetShader((ID3D11ComputeShader *)CS.Shader, CS.Instances, CS.NumInstances);

	// PS
	context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, PS.SRVs);
	context->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, PS.Samplers);
	context->PSSetShader((ID3D11PixelShader *)PS.Shader, PS.Instances, PS.NumInstances);
	
#if defined(INCLUDE_D3D_11_1)
	context->VSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, VS.ConstantBuffers, VS.CBOffsets, VS.CBCounts);
	context->DSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, DS.ConstantBuffers, DS.CBOffsets, DS.CBCounts);
	context->HSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, HS.ConstantBuffers, HS.CBOffsets, HS.CBCounts);
	context->GSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, GS.ConstantBuffers, GS.CBOffsets, GS.CBCounts);
	context->CSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, CS.ConstantBuffers, CS.CBOffsets, CS.CBCounts);
	context->PSSetConstantBuffers1(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, PS.ConstantBuffers, PS.CBOffsets, PS.CBCounts);
#else
	context->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, VS.ConstantBuffers);
	context->DSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, DS.ConstantBuffers);
	context->HSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, HS.ConstantBuffers);
	context->GSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, GS.ConstantBuffers);
	context->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, CS.ConstantBuffers);
	context->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, PS.ConstantBuffers);
#endif

	// OM
	context->OMSetBlendState(OM.BlendState, OM.BlendFactor, OM.SampleMask);
	context->OMSetDepthStencilState(OM.DepthStencilState, OM.StencRef);

	context->OMSetRenderTargetsAndUnorderedAccessViews(OM.UAVStartSlot, OM.RenderTargets,
														OM.DepthView,
														OM.UAVStartSlot, D3D11_PS_CS_UAV_REGISTER_COUNT-OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
}

void D3D11RenderState::TakeRef(IUnknown *p)
{
	if(p)
	{
		p->AddRef();
		if(m_ImmediatePipeline)
		{
			if(WrappedID3D11RenderTargetView::IsAlloc(p) ||
				WrappedID3D11ShaderResourceView::IsAlloc(p) ||
				WrappedID3D11DepthStencilView::IsAlloc(p) ||
				WrappedID3D11UnorderedAccessView::IsAlloc(p))
				m_pDevice->InternalRef();

			m_pDevice->InternalRef();
		}
	}
}

void D3D11RenderState::ReleaseRef(IUnknown *p)
{
	if(p)
	{
		p->Release();
		if(m_ImmediatePipeline)
		{
			if(WrappedID3D11RenderTargetView::IsAlloc(p) ||
				WrappedID3D11ShaderResourceView::IsAlloc(p) ||
				WrappedID3D11DepthStencilView::IsAlloc(p) ||
				WrappedID3D11UnorderedAccessView::IsAlloc(p))
				m_pDevice->InternalRelease();

			m_pDevice->InternalRelease();
		}
	}
}

bool D3D11RenderState::IsBoundIUnknownForWrite(IUnknown *resource, bool readDepthOnly, bool readStencilOnly)
{
	const char *names[] = { "VS", "DS", "HS", "GS", "CS", "PS" };
	shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		{
			bool found = false;
			
			ID3D11Resource *res = NULL;
			if(sh->UAVs[i])
			{
				sh->UAVs[i]->GetResource(&res);
				if(resource == (IUnknown *)res)
					found = true;
				SAFE_RELEASE(res);
			}
			
			if(found || resource == (IUnknown *)sh->UAVs[i])
			{
				//RDCDEBUG("Resource was bound on %s UAV %u", names[s], i);
				return true;
			}
		}
		
		sh++;
	}
	
	for(UINT i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
	{
		if(resource == (IUnknown *)SO.Buffers[i])
		{
			//RDCDEBUG("Resource was bound on SO buffer %u", i);
			return true;
		}
	}

	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		bool found = false;

		ID3D11Resource *res = NULL;
		if(OM.RenderTargets[i])
		{
			OM.RenderTargets[i]->GetResource(&res);
			if(resource == (IUnknown *)res)
				found = true;
			SAFE_RELEASE(res);
		}

		if(found || resource == (IUnknown *)OM.RenderTargets[i])
		{
			//RDCDEBUG("Resource was bound on RTV %u", i);
			return true;
		}
	}

	for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		bool found = false;

		ID3D11Resource *res = NULL;
		if(OM.UAVs[i])
		{
			OM.UAVs[i]->GetResource(&res);
			if(resource == (IUnknown *)res)
				found = true;
			SAFE_RELEASE(res);
		}

		if(found || resource == (IUnknown *)OM.UAVs[i])
		{
			//RDCDEBUG("Resource was bound on OM UAV %d", i);
			return true;
		}
	}
	
	{
		bool found = false;

		UINT depthFlags = 0;

		ID3D11Resource *res = NULL;
		if(OM.DepthView)
		{
			OM.DepthView->GetResource(&res);
			if(resource == (IUnknown *)res)
				found = true;
			SAFE_RELEASE(res);

			D3D11_DEPTH_STENCIL_VIEW_DESC d;
			OM.DepthView->GetDesc(&d);

			depthFlags = d.Flags;
		}

		if(found || resource == (IUnknown *)OM.DepthView)
		{
			//RDCDEBUG("Resource was bound on OM DSV");

			if(depthFlags == (D3D11_DSV_READ_ONLY_DEPTH|D3D11_DSV_READ_ONLY_STENCIL))
			{
				//RDCDEBUG("but it's a readonly DSV, so that's fine");
			}
			else if(depthFlags == D3D11_DSV_READ_ONLY_DEPTH && readDepthOnly)
			{
				//RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
			}
			else if(depthFlags == D3D11_DSV_READ_ONLY_STENCIL && readStencilOnly)
			{
				//RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

void D3D11RenderState::UnbindIUnknownForWrite(IUnknown *resource)
{
	shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		{
			bool found = false;
			
			ID3D11Resource *res = NULL;
			if(sh->UAVs[i])
			{
				sh->UAVs[i]->GetResource(&res);
				if(resource == (IUnknown *)res)
					found = true;
				SAFE_RELEASE(res);
			}
			
			if(found || resource == (IUnknown *)sh->UAVs[i])
			{
				ReleaseRef(sh->UAVs[i]);
				sh->UAVs[i] = NULL;
			}
		}
		
		sh++;
	}
	
	for(UINT i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
	{
		if(resource == (IUnknown *)SO.Buffers[i])
		{
			ReleaseRef(SO.Buffers[i]);
			SO.Buffers[i] = NULL;
		}
	}

	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		bool found = false;

		ID3D11Resource *res = NULL;
		if(OM.RenderTargets[i])
		{
			OM.RenderTargets[i]->GetResource(&res);
			if(resource == (IUnknown *)res)
				found = true;
			SAFE_RELEASE(res);
		}

		if(found || resource == (IUnknown *)OM.RenderTargets[i])
		{
			ReleaseRef(OM.RenderTargets[i]);
			OM.RenderTargets[i] = NULL;
		}
	}

	for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		bool found = false;

		ID3D11Resource *res = NULL;
		if(OM.UAVs[i])
		{
			OM.UAVs[i]->GetResource(&res);
			if(resource == (IUnknown *)res)
				found = true;
			SAFE_RELEASE(res);
		}

		if(found || resource == (IUnknown *)OM.UAVs[i])
		{
			ReleaseRef(OM.UAVs[i]);
			OM.UAVs[i] = NULL;
		}
	}
	
	{
		bool found = false;

		UINT depthFlags = 0;

		ID3D11Resource *res = NULL;
		if(OM.DepthView)
		{
			OM.DepthView->GetResource(&res);
			if(resource == (IUnknown *)res)
				found = true;
			SAFE_RELEASE(res);

			D3D11_DEPTH_STENCIL_VIEW_DESC d;
			OM.DepthView->GetDesc(&d);

			depthFlags = d.Flags;
		}

		if(found || resource == (IUnknown *)OM.DepthView)
		{
			ReleaseRef(OM.DepthView);
			OM.DepthView = NULL;
		}
	}
}

void D3D11RenderState::UnbindIUnknownForRead(IUnknown *resource, bool allowDepthOnly, bool allowStencilOnly)
{
	for(int i=0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		if(resource == (IUnknown *)IA.VBs[i])
		{
			//RDCDEBUG("Resource was bound on IA VB %u", i);
			ReleaseRef(IA.VBs[i]);
			IA.VBs[i] = NULL;
		}
	}

	if(resource == (IUnknown *)IA.IndexBuffer)
	{
		//RDCDEBUG("Resource was bound on IA IB");
		ReleaseRef(IA.IndexBuffer);
		IA.IndexBuffer = NULL;
	}
	
	const char *names[] = { "VS", "DS", "HS", "GS", "PS", "CS" };
	shader *sh = &VS;
	for(int s=0; s < 6; s++)
	{
		for(UINT i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		{
			if(resource == (IUnknown *)sh->ConstantBuffers[i])
			{
				//RDCDEBUG("Resource was bound on %s CB %u", names[s], i);
				ReleaseRef(sh->ConstantBuffers[i]);
				sh->ConstantBuffers[i] = NULL;
			}
		}

		for(UINT i=0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
		{
			bool found = false;
			
			bool readDepthOnly = false;
			bool readStencilOnly = false;
			
			D3D11_RESOURCE_DIMENSION dim;
			DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

			ID3D11Resource *res = NULL;
			if(sh->SRVs[i])
			{
				sh->SRVs[i]->GetResource(&res);
				if(resource == (IUnknown *)res)
					found = true;

				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				sh->SRVs[i]->GetDesc(&srvDesc);

				fmt = srvDesc.Format;

				res->GetType(&dim);
				
				if(fmt == DXGI_FORMAT_UNKNOWN)
				{
					if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
					{
						D3D11_TEXTURE1D_DESC d;
						((ID3D11Texture1D *)res)->GetDesc(&d);

						fmt = d.Format;
					}
					else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
					{
						D3D11_TEXTURE2D_DESC d;
						((ID3D11Texture2D *)res)->GetDesc(&d);

						fmt = d.Format;
					}
				}

				if(fmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
					fmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
				{
					readStencilOnly = true;
				}
				if(fmt == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
					fmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
				{
					readDepthOnly = true;
				}
				
				SAFE_RELEASE(res);
			}
			
			if(found || resource == (IUnknown *)sh->SRVs[i])
			{
				//RDCDEBUG("Resource was bound on %s SRV %u", names[s], i);

				if(allowDepthOnly && readDepthOnly)
				{
					//RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
				}
				else if(allowStencilOnly && readStencilOnly)
				{
					//RDCDEBUG("but it's a depth readonly DSV and we're only reading depth, so that's fine");
				}
				else
				{
					//RDCDEBUG("Unbinding.");
					ReleaseRef(sh->SRVs[i]);
					sh->SRVs[i] = NULL;
				}
			}
		}
		
		sh++;
	}
}

bool D3D11RenderState::ValidOutputMerger(ID3D11RenderTargetView **RTs, ID3D11DepthStencilView *depth)
{
	D3D11_RENDER_TARGET_VIEW_DESC RTDescs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	D3D11_DEPTH_STENCIL_VIEW_DESC DepthDesc;

	RDCEraseEl(RTDescs);
	RDCEraseEl(DepthDesc);

	ID3D11Resource *Resources[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
	ID3D11Resource *DepthResource = NULL;

	D3D11_RESOURCE_DIMENSION renderdim[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {D3D11_RESOURCE_DIMENSION_UNKNOWN};
	D3D11_RESOURCE_DIMENSION depthdim = D3D11_RESOURCE_DIMENSION_UNKNOWN;

	for(int i=0; RTs && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		if(RTs[i])
		{
			RTs[i]->GetDesc(&RTDescs[i]);
			RTs[i]->GetResource(&Resources[i]);
			Resources[i]->GetType(&renderdim[i]);
		}
	}

	if(depth)
	{
		depth->GetDesc(&DepthDesc);
		depth->GetResource(&DepthResource);
		DepthResource->GetType(&depthdim);
	}

	bool valid = true;

	//////////////////////////////////////////////////////////////////////////
	// Resource dimensions of all views must be the same

	D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	
	for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		if(renderdim[i] == D3D11_RESOURCE_DIMENSION_UNKNOWN) continue;
		if(dim == D3D11_RESOURCE_DIMENSION_UNKNOWN) dim = renderdim[i];

		if(renderdim[i] != dim)
		{
			valid = false;
			m_pDevice->AddDebugMessage(eDbgCategory_State_Setting, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
				"Invalid output merger - Render targets of different type");
			break;
		}
	}

	if(depthdim != D3D11_RESOURCE_DIMENSION_UNKNOWN &&
		dim != D3D11_RESOURCE_DIMENSION_UNKNOWN &&
		depthdim != dim)
	{
		m_pDevice->AddDebugMessage(eDbgCategory_State_Setting, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
			"Invalid output merger - Render target(s) and depth target of different type");
		valid = false;
	}

	if(!valid)
	{
		//RDCDEBUG("Resource dimensions don't match between render targets and/or depth stencil");
	}
	else
	{
		// pretend all resources are 3D descs just to make the code simpler
		// * put arraysize for 1D/2D into the depth for 3D
		// * use sampledesc from 2d as it will be identical for 1D/3D
		
		D3D11_TEXTURE3D_DESC desc = {0};
		D3D11_TEXTURE2D_DESC desc2 = {0};

		for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		{
			if(Resources[i] == NULL) continue;
			
			D3D11_TEXTURE1D_DESC d1 = {0};
			D3D11_TEXTURE2D_DESC d2 = {0};
			D3D11_TEXTURE3D_DESC d3 = {0};
			
			if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
			{
			}
			if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
			{
				((ID3D11Texture1D *)Resources[i])->GetDesc(&d1);
				d3.Width = RDCMAX(1U, d1.Width >> RTDescs[i].Texture1D.MipSlice);

				if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
					d3.Depth = 1;
				else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
					d3.Depth = RDCMIN(d1.ArraySize, RTDescs[i].Texture1DArray.ArraySize);
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
			{
				((ID3D11Texture2D *)Resources[i])->GetDesc(&d2);

				if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
				{
					d3.Width = RDCMAX(1U, d2.Width >> RTDescs[i].Texture2D.MipSlice);
					d3.Height = RDCMAX(1U, d2.Height >> RTDescs[i].Texture2D.MipSlice);
					d3.Depth = 1;
				}
				else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
				{
					d3.Width = d2.Width;
					d3.Height = d2.Height;
					d3.Depth = 1;
				}
				else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
				{
					d3.Width = RDCMAX(1U, d2.Width >> RTDescs[i].Texture2DArray.MipSlice);
					d3.Height = RDCMAX(1U, d2.Height >> RTDescs[i].Texture2DArray.MipSlice);
					d3.Depth = RDCMIN(d2.ArraySize, RTDescs[i].Texture2DArray.ArraySize);
				}
				else if(RTDescs[i].ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
				{
					d3.Width = d2.Width;
					d3.Height = d2.Height;
					d3.Depth = RDCMIN(d2.ArraySize, RTDescs[i].Texture2DMSArray.ArraySize);
				}
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
			{
				((ID3D11Texture3D *)Resources[i])->GetDesc(&d3);
				d3.Width = RDCMAX(1U, d3.Width >> RTDescs[i].Texture3D.MipSlice);
				d3.Height = RDCMAX(1U, d3.Height >> RTDescs[i].Texture3D.MipSlice);
				d3.Depth = RDCMAX(1U, d3.Depth >> RTDescs[i].Texture3D.MipSlice);
				d3.Depth = RDCMIN(d3.Depth, RTDescs[i].Texture3D.WSize);
			}

			if(desc.Width == 0)
			{
				desc = d3;
				desc2 = d2;
				continue;
			}

			if(desc.Width != d3.Width ||
				desc.Height != d3.Height ||
				desc.Depth != d3.Depth ||
				desc2.SampleDesc.Count != d2.SampleDesc.Count ||
				desc2.SampleDesc.Quality != d2.SampleDesc.Quality)
			{
				m_pDevice->AddDebugMessage(eDbgCategory_State_Setting, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
					"Invalid output merger - Render targets are different dimensions");
				valid = false;
				break;
			}
		}

		if(DepthResource && valid)
		{
			D3D11_TEXTURE1D_DESC d1 = {0};
			D3D11_TEXTURE2D_DESC d2 = {0};
			D3D11_TEXTURE3D_DESC d3 = {0};

			if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
			{
				((ID3D11Texture1D *)DepthResource)->GetDesc(&d1);
				d3.Width = RDCMAX(1U, d1.Width >> DepthDesc.Texture1D.MipSlice);

				if(DepthDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
					d3.Depth = 1;
				else if(DepthDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
					d3.Depth = RDCMIN(d1.ArraySize, DepthDesc.Texture1DArray.ArraySize);
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
			{
				((ID3D11Texture2D *)DepthResource)->GetDesc(&d2);

				if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
				{
					d3.Width = RDCMAX(1U, d2.Width >> DepthDesc.Texture2D.MipSlice);
					d3.Height = RDCMAX(1U, d2.Height >> DepthDesc.Texture2D.MipSlice);
					d3.Depth = 1;
				}
				else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
				{
					d3.Width = RDCMAX(1U, d2.Width >> DepthDesc.Texture2DArray.MipSlice);
					d3.Height = RDCMAX(1U, d2.Height >> DepthDesc.Texture2DArray.MipSlice);
					d3.Depth = RDCMIN(d2.ArraySize, DepthDesc.Texture2DArray.ArraySize);
				}
				else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS)
				{
					d3.Width = d2.Width;
					d3.Height = d2.Height;
					d3.Depth = 1;
				}
				else if(DepthDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
				{
					d3.Width = d2.Width;
					d3.Height = d2.Height;
					d3.Depth = RDCMIN(d2.ArraySize, DepthDesc.Texture2DMSArray.ArraySize);
				}
			}
			else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D ||
					dim == D3D11_RESOURCE_DIMENSION_BUFFER)
			{
				m_pDevice->AddDebugMessage(eDbgCategory_State_Setting, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
					"Invalid output merger - Depth target is Texture3D or Buffer (shouldn't be possible! How did you create this view?!)");
				valid = false;
			}

			if(desc.Width != 0 && valid)
			{
				if(desc.Width != d3.Width ||
					desc.Height != d3.Height ||
					desc.Depth != d3.Depth ||
					desc2.SampleDesc.Count != d2.SampleDesc.Count ||
					desc2.SampleDesc.Quality != d2.SampleDesc.Quality)
				{
					valid = false;

					// explicitly allow over-sized depth targets
					if(desc.Width <= d3.Width &&
						desc.Height <= d3.Height &&
						desc.Depth <= d3.Depth &&
						desc2.SampleDesc.Count == d2.SampleDesc.Count &&
						desc2.SampleDesc.Quality == d2.SampleDesc.Quality)
					{
						valid = true;
						m_pDevice->AddDebugMessage(eDbgCategory_State_Setting, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
							"Valid but unusual output merger - Depth target is larger than render target(s)");
					}
					else
					{
						m_pDevice->AddDebugMessage(eDbgCategory_State_Setting, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
							"Invalid output merger - Depth target is different size or MS count to render target(s)");
					}
				}
			}
		}
	}

	for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		SAFE_RELEASE(Resources[i]);

	SAFE_RELEASE(DepthResource);

	return valid;
}

bool D3D11RenderState::inputassembler::Used_VB(WrappedID3D11Device *device, uint32_t slot) const
{
	if(Layout == NULL)
		return false;

	const vector<D3D11_INPUT_ELEMENT_DESC> &vec = device->GetLayoutDesc(Layout);

	for(size_t i=0; i < vec.size(); i++)
		if(vec[i].InputSlot == slot)
			return true;

	return false;
}

bool D3D11RenderState::shader::Used_CB(uint32_t slot) const
{
	if(ConstantBuffers[slot] == NULL)
		return false;

	WrappedShader *shad = (WrappedShader*)(WrappedID3D11Shader<ID3D11VertexShader> *)Shader;

	if(shad == NULL)
		return false;

	DXBC::DXBCFile *dxbc = shad->GetDXBC();

	// have to assume it's used if there's no DXBC
	if(dxbc == NULL)
		return true;

	if(slot >= dxbc->m_CBuffers.size())
		return false;

	if(dxbc->m_CBuffers[slot].variables.empty())
		return false;

	return true;
}

bool D3D11RenderState::shader::Used_SRV(uint32_t slot) const
{
	if(SRVs[slot] == NULL)
		return false;

	WrappedShader *shad = (WrappedShader*)(WrappedID3D11Shader<ID3D11VertexShader> *)Shader;
	
	if(shad == NULL)
		return false;

	DXBC::DXBCFile *dxbc = shad->GetDXBC();

	// have to assume it's used if there's no DXBC
	if(dxbc == NULL)
		return true;

	for(size_t i=0; i < dxbc->m_Resources.size(); i++)
	{
		if(dxbc->m_Resources[i].bindPoint == slot &&
			(dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_TEXTURE ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_STRUCTURED ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_TBUFFER ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_BYTEADDRESS)
		  )
		{
			return true;
		}
	}

	return false;
}

bool D3D11RenderState::shader::Used_UAV(uint32_t slot) const
{
	WrappedShader *shad = (WrappedShader*)(WrappedID3D11Shader<ID3D11VertexShader> *)Shader;
	
	if(shad == NULL)
		return false;

	DXBC::DXBCFile *dxbc = shad->GetDXBC();

	// have to assume it's used if there's no DXBC
	if(dxbc == NULL)
		return true;

	for(size_t i=0; i < dxbc->m_Resources.size(); i++)
	{
		if(dxbc->m_Resources[i].bindPoint == slot &&
			(dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_APPEND_STRUCTURED ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_CONSUME_STRUCTURED ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER ||
			 dxbc->m_Resources[i].type == DXBC::ShaderInputBind::TYPE_UAV_RWTYPED)
		  )
		{
			return true;
		}
	}

	return false;
}

D3D11RenderStateTracker::D3D11RenderStateTracker(WrappedID3D11DeviceContext *ctx)
	: m_RS(*ctx->GetCurrentPipelineState())
{
	m_pContext = ctx;
}

D3D11RenderStateTracker::~D3D11RenderStateTracker()
{
	m_RS.ApplyState(m_pContext);
}
