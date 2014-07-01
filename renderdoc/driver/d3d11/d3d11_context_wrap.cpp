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


#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/d3d11/d3d11_renderstate.h"

#include "common/string_utils.h"

uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

#pragma region D3DPERF

bool WrappedID3D11DeviceContext::Serialise_SetMarker(uint32_t col, const wchar_t *name_)
{
	SERIALISE_ELEMENT(uint32_t, colour, col);

	wstring name = name_ ? name_ : L"";

	m_pSerialiser->Serialise("Name", name);

	if(m_State == READING)
	{
		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_SetMarker;

		AddDrawcall(draw, false);
	}

	return true;
}

bool WrappedID3D11DeviceContext::Serialise_BeginEvent(uint32_t col, const wchar_t *name_)
{
	SERIALISE_ELEMENT(uint32_t, colour, col);

	wstring name = name_ ? name_ : L"";

	m_pSerialiser->Serialise("Name", name);

	if(m_State == READING)
	{
		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, false);
	}

	return true;
}

bool WrappedID3D11DeviceContext::Serialise_EndEvent()
{
	if(m_State == READING && !m_CurEvents.empty())
	{
		FetchDrawcall draw;
		draw.name = L"API Calls";
		draw.flags |= eDraw_SetMarker;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::SetMarker(uint32_t col, const wchar_t *name)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_MARKER);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_SetMarker(col, name);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

int WrappedID3D11DeviceContext::BeginEvent(uint32_t col, const wchar_t *name)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_BeginEvent(col, name);

		m_ContextRecord->AddChunk(scope.Get());
	}
	
	return m_MarkerIndentLevel++;
}

int WrappedID3D11DeviceContext::EndEvent()
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(END_EVENT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_EndEvent();

		m_ContextRecord->AddChunk(scope.Get());
	}
	
	return --m_MarkerIndentLevel;
}

void WrappedID3D11DeviceContext::ThreadSafe_SetMarker(uint32_t col, const wchar_t *name)
{
	Annotation annot;
	annot.m_Type = Annotation::ANNOT_SETMARKER;
	annot.m_Col = col;
	annot.m_Name = name;

	{
		SCOPED_LOCK(m_AnnotLock);
		m_AnnotationQueue.push_back(annot);
	}
}

int WrappedID3D11DeviceContext::ThreadSafe_BeginEvent(uint32_t col, const wchar_t *name)
{
	Annotation annot;
	annot.m_Type = Annotation::ANNOT_BEGINEVENT;
	annot.m_Col = col;
	annot.m_Name = name;

	{
		SCOPED_LOCK(m_AnnotLock);
		m_AnnotationQueue.push_back(annot);
	}

	// not thread safe but I don't want to lock over access to this - if people use D3DPERF + MT
	// they shouldn't rely on this return value anyway :).
	return m_MarkerIndentLevel;
}

int WrappedID3D11DeviceContext::ThreadSafe_EndEvent()
{
	Annotation annot;
	annot.m_Type = Annotation::ANNOT_ENDEVENT;

	{
		SCOPED_LOCK(m_AnnotLock);
		m_AnnotationQueue.push_back(annot);
	}

	// not thread safe but I don't want to lock over access to this - if people use D3DPERF + MT
	// they shouldn't rely on this return value anyway :).
	return m_MarkerIndentLevel-1;
}

void WrappedID3D11DeviceContext::DrainAnnotationQueue()
{
	if(m_State != WRITING_CAPFRAME)
		return;

	m_AnnotLock.Lock();

	// fastest possible early-out
	if(m_AnnotationQueue.empty())
	{
		m_AnnotLock.Unlock();
		return;
	}

	vector<Annotation> annotations;
	annotations.swap(m_AnnotationQueue);
	
	m_AnnotLock.Unlock();

	for(size_t i=0; i < annotations.size(); i++)
	{
		const Annotation &a = annotations[i];

		switch(a.m_Type)
		{
			case Annotation::ANNOT_SETMARKER:
				SetMarker(a.m_Col, a.m_Name.c_str());
				break;
			case Annotation::ANNOT_BEGINEVENT:
				BeginEvent(a.m_Col, a.m_Name.c_str());
				break;
			case Annotation::ANNOT_ENDEVENT:
				EndEvent();
				break;
		}
	}
}

#pragma endregion D3DPERF

#pragma region Input Assembly

void WrappedID3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout **ppInputLayout)
{
	if(ppInputLayout)
	{
		ID3D11InputLayout *real = NULL;
		m_pRealContext->IAGetInputLayout(&real);
	
		SAFE_RELEASE_NOCLEAR(real);
		*ppInputLayout = (ID3D11InputLayout *)m_pDevice->GetResourceManager()->GetWrapper(real);
		SAFE_ADDREF(*ppInputLayout);

		RDCASSERT(*ppInputLayout == m_CurrentPipelineState->IA.Layout);
	}
}

void WrappedID3D11DeviceContext::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers,
													ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets)
{
	ID3D11Buffer *real[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {0};
	m_pRealContext->IAGetVertexBuffers(StartSlot, NumBuffers, real, pStrides, pOffsets);

	for(UINT i=0; i < NumBuffers; i++)
	{
		SAFE_RELEASE_NOCLEAR(real[i]);
		if(ppVertexBuffers)
		{
			ppVertexBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppVertexBuffers[i]);

			RDCASSERT(ppVertexBuffers[i] == m_CurrentPipelineState->IA.VBs[i+StartSlot]);
		}

		// D3D11 really inconsistently tracks these.
		//RDCASSERT(pStrides[i] == m_CurrentPipelineState->IA.Strides[i+StartSlot]);
		//RDCASSERT(pOffsets[i] == m_CurrentPipelineState->IA.Offsets[i+StartSlot]);
	}
}

void WrappedID3D11DeviceContext::IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset)
{
	if(pIndexBuffer)
	{
		ID3D11Buffer *real = NULL;
		m_pRealContext->IAGetIndexBuffer(&real, Format, Offset);

		SAFE_RELEASE_NOCLEAR(real);
		*pIndexBuffer = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real);
		SAFE_ADDREF(*pIndexBuffer);

		RDCASSERT(*pIndexBuffer == m_CurrentPipelineState->IA.IndexBuffer);
	}
	if(Format)
		RDCASSERT(*Format == m_CurrentPipelineState->IA.IndexFormat);
	if(Offset)
		RDCASSERT(*Offset == m_CurrentPipelineState->IA.IndexOffset);
}

void WrappedID3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	m_pRealContext->IAGetPrimitiveTopology(pTopology);
	if(pTopology)
		RDCASSERT(*pTopology == m_CurrentPipelineState->IA.Topo);
}

bool WrappedID3D11DeviceContext::Serialise_IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology_)
{
	SERIALISE_ELEMENT(D3D11_PRIMITIVE_TOPOLOGY, Topology, Topology_);

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Topo, Topology);
		m_pRealContext->IASetPrimitiveTopology(Topology);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_TOPOLOGY);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_IASetPrimitiveTopology(Topology);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Topo, Topology);
	m_pRealContext->IASetPrimitiveTopology(Topology);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_IASetInputLayout(ID3D11InputLayout *pInputLayout)
{
	SERIALISE_ELEMENT(ResourceId, InputLayout, GetIDForResource(pInputLayout));

	if(m_State <= EXECUTING)
	{
		pInputLayout = (ID3D11InputLayout *)m_pDevice->GetResourceManager()->GetLiveResource(InputLayout);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.Layout, pInputLayout);
		m_pRealContext->IASetInputLayout(UNWRAP(WrappedID3D11InputLayout, pInputLayout));
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::IASetInputLayout(ID3D11InputLayout *pInputLayout)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_INPUT_LAYOUT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_IASetInputLayout(pInputLayout);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.Layout, pInputLayout);
	m_pRealContext->IASetInputLayout(UNWRAP(WrappedID3D11InputLayout, pInputLayout));
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_IASetVertexBuffers(UINT StartSlot_, UINT NumBuffers_,
															ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	SERIALISE_ELEMENT_ARR(uint32_t, Strides, pStrides, NumBuffers);
	SERIALISE_ELEMENT_ARR(uint32_t, Offsets, pOffsets, NumBuffers);
	
	ID3D11Buffer *Buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppVertexBuffers[i]));
		
		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.VBs, Buffers, StartSlot, NumBuffers);
	}
	
	for(UINT i=0; i < NumBuffers; i++)
		if(m_State <= EXECUTING)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Strides, Strides, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Offsets, Offsets, StartSlot, NumBuffers);
		m_pRealContext->IASetVertexBuffers(StartSlot, NumBuffers, Buffers, Strides, Offsets);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Strides);
	SAFE_DELETE_ARRAY(Offsets);

	return true;
}

void WrappedID3D11DeviceContext::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers,
													ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_VBUFFER);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.VBs, ppVertexBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Strides, pStrides, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.Offsets, pOffsets, StartSlot, NumBuffers);
	
	ID3D11Buffer *bufs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppVertexBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppVertexBuffers[i]), eFrameRef_Read);
		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppVertexBuffers[i]);
	}
	
	m_pRealContext->IASetVertexBuffers(StartSlot, NumBuffers, bufs, pStrides, pOffsets);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format_, UINT Offset_)
{
	SERIALISE_ELEMENT(ResourceId, Buffer, GetIDForResource(pIndexBuffer));
	SERIALISE_ELEMENT(DXGI_FORMAT, Format, Format_);
	SERIALISE_ELEMENT(uint32_t, Offset, Offset_);
	
	if(m_State <= EXECUTING)
	{
		pIndexBuffer = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(Buffer);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.IndexBuffer, pIndexBuffer);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexFormat, Format);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexOffset, Offset);
		m_pRealContext->IASetIndexBuffer(UNWRAP(WrappedID3D11Buffer, pIndexBuffer), Format, Offset);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_IBUFFER);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_IASetIndexBuffer(pIndexBuffer, Format, Offset);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	if(pIndexBuffer && m_State >= WRITING_CAPFRAME)
		MarkResourceReferenced(GetIDForResource(pIndexBuffer), eFrameRef_Read);
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->IA.IndexBuffer, pIndexBuffer);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexFormat, Format);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->IA.IndexOffset, Offset);
	m_pRealContext->IASetIndexBuffer(UNWRAP(WrappedID3D11Buffer, pIndexBuffer), Format, Offset);
	VerifyState();
}

#pragma endregion Input Assembly

#pragma region Vertex Shader

void WrappedID3D11DeviceContext::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	if(ppConstantBuffers)
	{
		ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
		m_pRealContext->VSGetConstantBuffers(StartSlot, NumBuffers, real);

		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppConstantBuffers[i]);

			RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->VS.ConstantBuffers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	if(ppShaderResourceViews)
	{
		ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
		m_pRealContext->VSGetShaderResources(StartSlot, NumViews, real);

		for(UINT i=0; i < NumViews; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppShaderResourceViews[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppShaderResourceViews[i]);

			RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->VS.SRVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	if(ppSamplers)
	{
		ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
		m_pRealContext->VSGetSamplers(StartSlot, NumSamplers, real);

		for(UINT i=0; i < NumSamplers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSamplers[i]);

			RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->VS.Samplers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::VSGetShader(ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances,
												UINT *pNumClassInstances)
{
	if(ppVertexShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
		return;
	
	ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
	UINT numInsts = 0;
	ID3D11VertexShader *realShader = NULL;
	m_pRealContext->VSGetShader(&realShader, realInsts, &numInsts);
	
	SAFE_RELEASE_NOCLEAR(realShader);
	for(UINT i=0; i < numInsts; i++)
		SAFE_RELEASE_NOCLEAR(realInsts[i]);

	if(ppVertexShader)
	{
		*ppVertexShader = (ID3D11VertexShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
		SAFE_ADDREF(*ppVertexShader);

		RDCASSERT(*ppVertexShader == m_CurrentPipelineState->VS.Shader);
	}

	if(ppClassInstances)
	{
		for(UINT i=0; i < numInsts; i++)
		{
			ppClassInstances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
			SAFE_ADDREF(ppClassInstances[i]);

			RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->VS.Instances[i]);
		}
	}

	if(pNumClassInstances)
	{
		*pNumClassInstances = numInsts;
	}
}

bool WrappedID3D11DeviceContext::Serialise_VSSetConstantBuffers(UINT StartSlot_, UINT NumBuffers_,
												ID3D11Buffer *const *ppConstantBuffers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppConstantBuffers[i]));
		
		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers, Buffers, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

		for(UINT i=0; i < NumBuffers; i++)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

		m_pRealContext->VSSetConstantBuffers(StartSlot, NumBuffers, Buffers);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_VS_CBUFFERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.ConstantBuffers, ppConstantBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

	ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppConstantBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
	}
	
	m_pRealContext->VSSetConstantBuffers(StartSlot, NumBuffers, bufs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_VSSetShaderResources(UINT StartSlot_, UINT NumViews_, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);

	ID3D11ShaderResourceView **Views = new ID3D11ShaderResourceView *[NumViews];

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppShaderResourceViews[i]));

		if(m_State <= EXECUTING)
			Views[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.SRVs, Views, StartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			Views[i] = UNWRAP(WrappedID3D11ShaderResourceView, Views[i]);

		m_pRealContext->VSSetShaderResources(StartSlot, NumViews, Views);
		VerifyState();
	}
	
	SAFE_DELETE_ARRAY(Views);

	return true;
}

void WrappedID3D11DeviceContext::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_VS_RESOURCES);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.SRVs, ppShaderResourceViews, StartSlot, NumViews);
	
	ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppShaderResourceViews[i] && m_State >= WRITING_CAPFRAME)
		{
			ID3D11Resource *res = NULL;
			ppShaderResourceViews[i]->GetResource(&res);
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
			SAFE_RELEASE(res);
		}

		SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView, ppShaderResourceViews[i]);
	}

	m_pRealContext->VSSetShaderResources(StartSlot, NumViews, SRVs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_VSSetSamplers(UINT StartSlot_, UINT NumSamplers_, ID3D11SamplerState *const *ppSamplers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumSamplers, NumSamplers_);

	ID3D11SamplerState **Sampler = new ID3D11SamplerState *[NumSamplers];

	for(UINT i=0; i < NumSamplers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSamplers[i]));

		if(m_State <= EXECUTING)
			Sampler[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Samplers, Sampler, StartSlot, NumSamplers);

		for(UINT i=0; i < NumSamplers; i++)
			Sampler[i] = UNWRAP(WrappedID3D11SamplerState, Sampler[i]);

		m_pRealContext->VSSetSamplers(StartSlot, NumSamplers, Sampler);
		VerifyState();
	}
		
	SAFE_DELETE_ARRAY(Sampler);

	return true;
}

void WrappedID3D11DeviceContext::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_VS_SAMPLERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Samplers, ppSamplers, StartSlot, NumSamplers);
	
	ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for(UINT i=0; i < NumSamplers; i++)
		samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

	m_pRealContext->VSSetSamplers(StartSlot, NumSamplers, samps);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_VSSetShader(ID3D11VertexShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
														UINT NumClassInstances_)
{
	SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(pShader));
	SERIALISE_ELEMENT(uint32_t, NumClassInstances, NumClassInstances_);
	
	ID3D11ClassInstance **Instances = new ID3D11ClassInstance *[NumClassInstances];

	for(UINT i=0; i < NumClassInstances; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppClassInstances[i]));

		if(m_State <= EXECUTING)
			Instances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Instances, Instances, 0, NumClassInstances);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.NumInstances, NumClassInstances);

		for(UINT i=0; i < NumClassInstances; i++)
			Instances[i] = UNWRAP(WrappedID3D11ClassInstance, Instances[i]);

		ID3D11DeviceChild *pShader = (ID3D11DeviceChild *)m_pDevice->GetResourceManager()->GetLiveResource(Shader);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Shader, pShader);
		m_pRealContext->VSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, pShader), Instances, NumClassInstances);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::VSSetShader(ID3D11VertexShader *pVertexShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_VS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
		
		MarkResourceReferenced(GetIDForResource(pVertexShader), eFrameRef_Read);

		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Shader, (ID3D11DeviceChild *)pVertexShader);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->VS.NumInstances, NumClassInstances);
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->VS.Instances, ppClassInstances, 0, NumClassInstances);
	
	ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
	if(ppClassInstances && NumClassInstances > 0)
		for(UINT i=0; i < NumClassInstances; i++)
			insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);
	
	m_pRealContext->VSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, pVertexShader), insts, NumClassInstances);
	VerifyState();
}

#pragma endregion Vertex Shader

#pragma region Hull Shader

void WrappedID3D11DeviceContext::HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	if(ppConstantBuffers)
	{
		ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
		m_pRealContext->HSGetConstantBuffers(StartSlot, NumBuffers, real);
	
		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppConstantBuffers[i]);

			RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->HS.ConstantBuffers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	if(ppShaderResourceViews)
	{
		ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
		m_pRealContext->HSGetShaderResources(StartSlot, NumViews, real);

		for(UINT i=0; i < NumViews; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppShaderResourceViews[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppShaderResourceViews[i]);

			RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->HS.SRVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	if(ppSamplers)
	{
		ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
		m_pRealContext->HSGetSamplers(StartSlot, NumSamplers, real);

		for(UINT i=0; i < NumSamplers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSamplers[i]);

			RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->HS.Samplers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::HSGetShader(ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	if(ppHullShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
		return;
	
	ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
	UINT numInsts = 0;
	ID3D11HullShader *realShader = NULL;
	m_pRealContext->HSGetShader(&realShader, realInsts, &numInsts);
	
	SAFE_RELEASE_NOCLEAR(realShader);
	for(UINT i=0; i < numInsts; i++)
		SAFE_RELEASE_NOCLEAR(realInsts[i]);

	if(ppHullShader)
	{
		*ppHullShader = (ID3D11HullShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
		SAFE_ADDREF(*ppHullShader);

		RDCASSERT(*ppHullShader == m_CurrentPipelineState->HS.Shader);
	}

	if(ppClassInstances)
	{
		for(UINT i=0; i < numInsts; i++)
		{
			ppClassInstances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
			SAFE_ADDREF(ppClassInstances[i]);

			RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->HS.Instances[i]);
		}
	}

	if(pNumClassInstances)
	{
		*pNumClassInstances = numInsts;
	}
}

bool WrappedID3D11DeviceContext::Serialise_HSSetConstantBuffers(UINT StartSlot_, UINT NumBuffers_,
												ID3D11Buffer *const *ppConstantBuffers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	ID3D11Buffer **Buffers = new ID3D11Buffer *[NumBuffers];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppConstantBuffers[i]));

		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers, Buffers, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

		for(UINT i=0; i < NumBuffers; i++)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

		if(m_State <= EXECUTING)
			m_pRealContext->HSSetConstantBuffers(StartSlot, NumBuffers, Buffers);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Buffers);

	return true;
}

void WrappedID3D11DeviceContext::HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_HS_CBUFFERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.ConstantBuffers, ppConstantBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

	ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppConstantBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
	}
	
	m_pRealContext->HSSetConstantBuffers(StartSlot, NumBuffers, bufs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_HSSetShaderResources(UINT StartSlot_, UINT NumViews_, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);

	ID3D11ShaderResourceView **Views = new ID3D11ShaderResourceView *[NumViews];

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppShaderResourceViews[i]));

		if(m_State <= EXECUTING)
			Views[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.SRVs, Views, StartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			Views[i] = UNWRAP(WrappedID3D11ShaderResourceView, Views[i]);

		m_pRealContext->HSSetShaderResources(StartSlot, NumViews, Views);
		VerifyState();
	}
	
	SAFE_DELETE_ARRAY(Views);

	return true;
}

void WrappedID3D11DeviceContext::HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_HS_RESOURCES);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.SRVs, ppShaderResourceViews, StartSlot, NumViews);
	
	ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppShaderResourceViews[i] && m_State >= WRITING_CAPFRAME)
		{
			ID3D11Resource *res = NULL;
			ppShaderResourceViews[i]->GetResource(&res);
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
			SAFE_RELEASE(res);
		}

		SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView, ppShaderResourceViews[i]);
	}

	m_pRealContext->HSSetShaderResources(StartSlot, NumViews, SRVs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_HSSetSamplers(UINT StartSlot_, UINT NumSamplers_, ID3D11SamplerState *const *ppSamplers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumSamplers, NumSamplers_);

	ID3D11SamplerState **Sampler = new ID3D11SamplerState *[NumSamplers];

	for(UINT i=0; i < NumSamplers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSamplers[i]));

		if(m_State <= EXECUTING)
			Sampler[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Samplers, Sampler, StartSlot, NumSamplers);

		for(UINT i=0; i < NumSamplers; i++)
			Sampler[i] = UNWRAP(WrappedID3D11SamplerState, Sampler[i]);

		m_pRealContext->HSSetSamplers(StartSlot, NumSamplers, Sampler);
		VerifyState();
	}
		
	SAFE_DELETE_ARRAY(Sampler);

	return true;
}

void WrappedID3D11DeviceContext::HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_HS_SAMPLERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Samplers, ppSamplers, StartSlot, NumSamplers);
	
	ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for(UINT i=0; i < NumSamplers; i++)
		samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

	m_pRealContext->HSSetSamplers(StartSlot, NumSamplers, samps);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_HSSetShader(ID3D11HullShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
														UINT NumClassInstances_)
{
	SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(pShader));
	SERIALISE_ELEMENT(uint32_t, NumClassInstances, NumClassInstances_);
	
	ID3D11ClassInstance **Instances = new ID3D11ClassInstance *[NumClassInstances];

	for(UINT i=0; i < NumClassInstances; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppClassInstances[i]));

		if(m_State <= EXECUTING)
			Instances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Instances, Instances, 0, NumClassInstances);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.NumInstances, NumClassInstances);

		for(UINT i=0; i < NumClassInstances; i++)
			Instances[i] = UNWRAP(WrappedID3D11ClassInstance, Instances[i]);

		ID3D11DeviceChild *pShader = (ID3D11DeviceChild *)m_pDevice->GetResourceManager()->GetLiveResource(Shader);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Shader, pShader);
		m_pRealContext->HSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11HullShader>, pShader), Instances, NumClassInstances);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::HSSetShader(ID3D11HullShader *pHullShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_HS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
		
		MarkResourceReferenced(GetIDForResource(pHullShader), eFrameRef_Read);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Shader, (ID3D11DeviceChild *)pHullShader);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->HS.NumInstances, NumClassInstances);
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->HS.Instances, ppClassInstances, 0, NumClassInstances);
	
	ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
	if(ppClassInstances && NumClassInstances > 0)
		for(UINT i=0; i < NumClassInstances; i++)
			insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);
	
	m_pRealContext->HSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11HullShader>, pHullShader), insts, NumClassInstances);
	VerifyState();
}

#pragma endregion Hull Shader

#pragma region Domain Shader

void WrappedID3D11DeviceContext::DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	if(ppConstantBuffers)
	{
		ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
		m_pRealContext->DSGetConstantBuffers(StartSlot, NumBuffers, real);

		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppConstantBuffers[i]);

			RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->DS.ConstantBuffers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	if(ppShaderResourceViews)
	{
		ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
		m_pRealContext->DSGetShaderResources(StartSlot, NumViews, real);

		for(UINT i=0; i < NumViews; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppShaderResourceViews[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppShaderResourceViews[i]);

			RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->DS.SRVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	if(ppSamplers)
	{
		ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
		m_pRealContext->DSGetSamplers(StartSlot, NumSamplers, real);

		for(UINT i=0; i < NumSamplers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSamplers[i]);

			RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->DS.Samplers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::DSGetShader(ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	if(ppDomainShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
		return;
	
	ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
	UINT numInsts = 0;
	ID3D11DomainShader *realShader = NULL;
	m_pRealContext->DSGetShader(&realShader, realInsts, &numInsts);
	
	SAFE_RELEASE_NOCLEAR(realShader);
	for(UINT i=0; i < numInsts; i++)
		SAFE_RELEASE_NOCLEAR(realInsts[i]);

	if(ppDomainShader)
	{
		*ppDomainShader = (ID3D11DomainShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
		SAFE_ADDREF(*ppDomainShader);

		RDCASSERT(*ppDomainShader == m_CurrentPipelineState->DS.Shader);
	}

	if(ppClassInstances)
	{
		for(UINT i=0; i < numInsts; i++)
		{
			ppClassInstances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
			SAFE_ADDREF(ppClassInstances[i]);

			RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->DS.Instances[i]);
		}
	}

	if(pNumClassInstances)
	{
		*pNumClassInstances = numInsts;
	}
}

bool WrappedID3D11DeviceContext::Serialise_DSSetConstantBuffers(UINT StartSlot_, UINT NumBuffers_,
												ID3D11Buffer *const *ppConstantBuffers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	ID3D11Buffer **Buffers = new ID3D11Buffer *[NumBuffers];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppConstantBuffers[i]));

		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers, Buffers, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

		for(UINT i=0; i < NumBuffers; i++)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

		m_pRealContext->DSSetConstantBuffers(StartSlot, NumBuffers, Buffers);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Buffers);

	return true;
}

void WrappedID3D11DeviceContext::DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_DS_CBUFFERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.ConstantBuffers, ppConstantBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

	ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppConstantBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
	}

	m_pRealContext->DSSetConstantBuffers(StartSlot, NumBuffers, bufs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_DSSetShaderResources(UINT StartSlot_, UINT NumViews_, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);

	ID3D11ShaderResourceView **Views = new ID3D11ShaderResourceView *[NumViews];

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppShaderResourceViews[i]));

		if(m_State <= EXECUTING)
			Views[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.SRVs, Views, StartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			Views[i] = UNWRAP(WrappedID3D11ShaderResourceView, Views[i]);

		m_pRealContext->DSSetShaderResources(StartSlot, NumViews, Views);
		VerifyState();
	}
	
	SAFE_DELETE_ARRAY(Views);

	return true;
}

void WrappedID3D11DeviceContext::DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_DS_RESOURCES);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.SRVs, ppShaderResourceViews, StartSlot, NumViews);
	
	ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppShaderResourceViews[i] && m_State >= WRITING_CAPFRAME)
		{
			ID3D11Resource *res = NULL;
			ppShaderResourceViews[i]->GetResource(&res);
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
			SAFE_RELEASE(res);
		}

		SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView, ppShaderResourceViews[i]);
	}

	m_pRealContext->DSSetShaderResources(StartSlot, NumViews, SRVs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_DSSetSamplers(UINT StartSlot_, UINT NumSamplers_, ID3D11SamplerState *const *ppSamplers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumSamplers, NumSamplers_);

	ID3D11SamplerState **Sampler = new ID3D11SamplerState *[NumSamplers];

	for(UINT i=0; i < NumSamplers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSamplers[i]));

		if(m_State <= EXECUTING)
			Sampler[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Samplers, Sampler, StartSlot, NumSamplers);

		for(UINT i=0; i < NumSamplers; i++)
			Sampler[i] = UNWRAP(WrappedID3D11SamplerState, Sampler[i]);

		m_pRealContext->DSSetSamplers(StartSlot, NumSamplers, Sampler);
		VerifyState();
	}
		
	SAFE_DELETE_ARRAY(Sampler);

	return true;
}

void WrappedID3D11DeviceContext::DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_DS_SAMPLERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Samplers, ppSamplers, StartSlot, NumSamplers);
	
	ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for(UINT i=0; i < NumSamplers; i++)
		samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

	m_pRealContext->DSSetSamplers(StartSlot, NumSamplers, samps);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_DSSetShader(ID3D11DomainShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
														UINT NumClassInstances_)
{
	SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(pShader));
	SERIALISE_ELEMENT(uint32_t, NumClassInstances, NumClassInstances_);
	
	ID3D11ClassInstance **Instances = new ID3D11ClassInstance *[NumClassInstances];

	for(UINT i=0; i < NumClassInstances; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppClassInstances[i]));

		if(m_State <= EXECUTING)
			Instances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Instances, Instances, 0, NumClassInstances);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.NumInstances, NumClassInstances);

		for(UINT i=0; i < NumClassInstances; i++)
			Instances[i] = UNWRAP(WrappedID3D11ClassInstance, Instances[i]);

		ID3D11DeviceChild *pShader = (ID3D11DeviceChild *)m_pDevice->GetResourceManager()->GetLiveResource(Shader);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Shader, pShader);
		m_pRealContext->DSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11DomainShader>, pShader), Instances, NumClassInstances);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::DSSetShader(ID3D11DomainShader *pDomainShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_DS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
		
		MarkResourceReferenced(GetIDForResource(pDomainShader), eFrameRef_Read);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Shader, (ID3D11DeviceChild *)pDomainShader);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->DS.NumInstances, NumClassInstances);
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->DS.Instances, ppClassInstances, 0, NumClassInstances);
	
	ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
	if(ppClassInstances && NumClassInstances > 0)
		for(UINT i=0; i < NumClassInstances; i++)
			insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);
	
	m_pRealContext->DSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11DomainShader>, pDomainShader), insts, NumClassInstances);
	VerifyState();
}

#pragma endregion Domain Shader

#pragma region Geometry Shader

void WrappedID3D11DeviceContext::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	if(ppConstantBuffers)
	{
		ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
		m_pRealContext->GSGetConstantBuffers(StartSlot, NumBuffers, real);
	
		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppConstantBuffers[i]);

			RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->GS.ConstantBuffers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	if(ppShaderResourceViews)
	{
		ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
		m_pRealContext->GSGetShaderResources(StartSlot, NumViews, real);

		for(UINT i=0; i < NumViews; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppShaderResourceViews[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppShaderResourceViews[i]);

			RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->GS.SRVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	if(ppSamplers)
	{
		ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
		m_pRealContext->GSGetSamplers(StartSlot, NumSamplers, real);

		for(UINT i=0; i < NumSamplers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSamplers[i]);

			RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->GS.Samplers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::GSGetShader(ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	if(ppGeometryShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
		return;
	
	ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
	UINT numInsts = 0;
	ID3D11GeometryShader *realShader = NULL;
	m_pRealContext->GSGetShader(&realShader, realInsts, &numInsts);
	
	SAFE_RELEASE_NOCLEAR(realShader);
	for(UINT i=0; i < numInsts; i++)
		SAFE_RELEASE_NOCLEAR(realInsts[i]);

	if(ppGeometryShader)
	{
		*ppGeometryShader = (ID3D11GeometryShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
		SAFE_ADDREF(*ppGeometryShader);

		RDCASSERT(*ppGeometryShader == m_CurrentPipelineState->GS.Shader);
	}

	if(ppClassInstances)
	{
		for(UINT i=0; i < numInsts; i++)
		{
			ppClassInstances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
			SAFE_ADDREF(ppClassInstances[i]);

			RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->GS.Instances[i]);
		}
	}

	if(pNumClassInstances)
	{
		*pNumClassInstances = numInsts;
	}
}

bool WrappedID3D11DeviceContext::Serialise_GSSetConstantBuffers(UINT StartSlot_, UINT NumBuffers_,
												ID3D11Buffer *const *ppConstantBuffers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	ID3D11Buffer **Buffers = new ID3D11Buffer *[NumBuffers];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppConstantBuffers[i]));

		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers, Buffers, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

		for(UINT i=0; i < NumBuffers; i++)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

		m_pRealContext->GSSetConstantBuffers(StartSlot, NumBuffers, Buffers);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Buffers);

	return true;
}

void WrappedID3D11DeviceContext::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_GS_CBUFFERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.ConstantBuffers, ppConstantBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

	ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppConstantBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
	}

	m_pRealContext->GSSetConstantBuffers(StartSlot, NumBuffers, bufs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_GSSetShaderResources(UINT StartSlot_, UINT NumViews_, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);

	ID3D11ShaderResourceView **Views = new ID3D11ShaderResourceView *[NumViews];

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppShaderResourceViews[i]));

		if(m_State <= EXECUTING)
			Views[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.SRVs, Views, StartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			Views[i] = UNWRAP(WrappedID3D11ShaderResourceView, Views[i]);

		m_pRealContext->GSSetShaderResources(StartSlot, NumViews, Views);
		VerifyState();
	}
	
	SAFE_DELETE_ARRAY(Views);

	return true;
}

void WrappedID3D11DeviceContext::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_GS_RESOURCES);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.SRVs, ppShaderResourceViews, StartSlot, NumViews);
	
	ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppShaderResourceViews[i] && m_State >= WRITING_CAPFRAME)
		{
			ID3D11Resource *res = NULL;
			ppShaderResourceViews[i]->GetResource(&res);
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
			SAFE_RELEASE(res);
		}

		SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView, ppShaderResourceViews[i]);
	}

	m_pRealContext->GSSetShaderResources(StartSlot, NumViews, SRVs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_GSSetSamplers(UINT StartSlot_, UINT NumSamplers_, ID3D11SamplerState *const *ppSamplers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumSamplers, NumSamplers_);

	ID3D11SamplerState **Sampler = new ID3D11SamplerState *[NumSamplers];

	for(UINT i=0; i < NumSamplers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSamplers[i]));

		if(m_State <= EXECUTING)
			Sampler[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Samplers, Sampler, StartSlot, NumSamplers);

		for(UINT i=0; i < NumSamplers; i++)
			Sampler[i] = UNWRAP(WrappedID3D11SamplerState, Sampler[i]);

		m_pRealContext->GSSetSamplers(StartSlot, NumSamplers, Sampler);
		VerifyState();
	}
		
	SAFE_DELETE_ARRAY(Sampler);

	return true;
}

void WrappedID3D11DeviceContext::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_GS_SAMPLERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Samplers, ppSamplers, StartSlot, NumSamplers);
	
	ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for(UINT i=0; i < NumSamplers; i++)
		samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

	m_pRealContext->GSSetSamplers(StartSlot, NumSamplers, samps);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
														UINT NumClassInstances_)
{
	SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(pShader));
	SERIALISE_ELEMENT(uint32_t, NumClassInstances, NumClassInstances_);
	
	ID3D11ClassInstance **Instances = new ID3D11ClassInstance *[NumClassInstances];

	for(UINT i=0; i < NumClassInstances; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppClassInstances[i]));

		if(m_State <= EXECUTING)
			Instances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Instances, Instances, 0, NumClassInstances);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.NumInstances, NumClassInstances);

		for(UINT i=0; i < NumClassInstances; i++)
			Instances[i] = UNWRAP(WrappedID3D11ClassInstance, Instances[i]);

		ID3D11DeviceChild *pShader = (ID3D11DeviceChild *)m_pDevice->GetResourceManager()->GetLiveResource(Shader);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Shader, pShader);
		m_pRealContext->GSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11GeometryShader>, pShader), Instances, NumClassInstances);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::GSSetShader( ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_GS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_GSSetShader(pShader, ppClassInstances, NumClassInstances);
		
		MarkResourceReferenced(GetIDForResource(pShader), eFrameRef_Read);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Shader, (ID3D11DeviceChild *)pShader);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->GS.NumInstances, NumClassInstances);
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->GS.Instances, ppClassInstances, 0, NumClassInstances);
	
	ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
	if(ppClassInstances && NumClassInstances > 0)
		for(UINT i=0; i < NumClassInstances; i++)
			insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);
	
	m_pRealContext->GSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11GeometryShader>, pShader), insts, NumClassInstances);
	VerifyState();
}

#pragma endregion Geometry Shader

#pragma region Stream Out

void WrappedID3D11DeviceContext::SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets)
{
	if(ppSOTargets)
	{
		ID3D11Buffer *real[D3D11_SO_BUFFER_SLOT_COUNT] = {0};
		m_pRealContext->SOGetTargets(NumBuffers, real);

		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSOTargets[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSOTargets[i]);

			RDCASSERT(ppSOTargets[i] == m_CurrentPipelineState->SO.Buffers[i]);
		}
	}
}

bool WrappedID3D11DeviceContext::Serialise_SOSetTargets(UINT NumBuffers_, ID3D11Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	SERIALISE_ELEMENT_ARR(uint32_t, Offsets, pOffsets, NumBuffers);

	ID3D11Buffer **Buffers = new ID3D11Buffer *[NumBuffers];
	
	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSOTargets[i]));

		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->SO.Buffers, Buffers, 0, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->SO.Offsets, Offsets, 0, NumBuffers);
	}
	
	for(UINT i=0; i < NumBuffers; i++)
		if(m_State <= EXECUTING)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);
	
	if(m_State <= EXECUTING)
	{
		m_pRealContext->SOSetTargets(NumBuffers, Buffers, Offsets);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Buffers);
	SAFE_DELETE_ARRAY(Offsets);

	return true;
}

void WrappedID3D11DeviceContext::SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_SO_TARGETS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->SO.Buffers, ppSOTargets, 0, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->SO.Offsets, pOffsets, 0, NumBuffers);
	
	ID3D11Buffer *bufs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		// technically this isn't dirty until the draw call, but let's be conservative
		// to avoid having to track "possibly" dirty resources.
		// Besides, it's unlikely an application will set an output then not draw to it
		if(ppSOTargets[i] && m_State >= WRITING_CAPFRAME)
		{
			MarkResourceReferenced(GetIDForResource(ppSOTargets[i]), eFrameRef_Write);
			
			if(m_State == WRITING_CAPFRAME)
				m_MissingTracks.insert(GetIDForResource(ppSOTargets[i]));
			if(m_State == WRITING_IDLE)
				m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(ppSOTargets[i]));
		}
		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppSOTargets[i]);
	}

	m_pRealContext->SOSetTargets(NumBuffers, bufs, pOffsets);
	VerifyState();
}

#pragma endregion Stream Out

#pragma region Rasterizer

void WrappedID3D11DeviceContext::RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports)
{
	m_pRealContext->RSGetViewports(pNumViewports, pViewports);

	if(pViewports)
		RDCASSERT(memcmp(pViewports, m_CurrentPipelineState->RS.Viewports, sizeof(D3D11_VIEWPORT)* (*pNumViewports)) == 0);
}

void WrappedID3D11DeviceContext::RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects)
{
	m_pRealContext->RSGetScissorRects(pNumRects, pRects);

	if(pRects)
		RDCASSERT(memcmp(pRects, m_CurrentPipelineState->RS.Scissors, sizeof(D3D11_RECT)* (*pNumRects)) == 0);
}

void WrappedID3D11DeviceContext::RSGetState(ID3D11RasterizerState **ppRasterizerState)
{
	if(ppRasterizerState)
	{
		ID3D11RasterizerState *real = NULL;
		m_pRealContext->RSGetState(&real);

		if(real != NULL)
		{
			real->Release();
			ID3D11DeviceChild *state = m_pDevice->GetResourceManager()->GetWrapper(real);
#if defined(INCLUDE_D3D_11_1)
			if(WrappedID3D11RasterizerState1::IsAlloc(state))
			{
				*ppRasterizerState = (ID3D11RasterizerState *)(ID3D11RasterizerState1 *)state;
				(*ppRasterizerState)->AddRef();
			}
			else
#endif
			{
				*ppRasterizerState = (ID3D11RasterizerState *)state;
				(*ppRasterizerState)->AddRef();
			}
		}
		else
		{
			*ppRasterizerState = NULL;
		}

		RDCASSERT(*ppRasterizerState == m_CurrentPipelineState->RS.State);
	}
}

bool WrappedID3D11DeviceContext::Serialise_RSSetViewports(UINT NumViewports_, const D3D11_VIEWPORT *pViewports)
{
	SERIALISE_ELEMENT(uint32_t, NumViewports, NumViewports_);

	D3D11_VIEWPORT views[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

	for(UINT i=0; i < NumViewports; i++)
	{
		D3D11_VIEWPORT view;

		if(pViewports)
			view = pViewports[i];

		m_pSerialiser->Serialise<6>((string("Viewport[") + ToStr::Get(i) + "]").c_str(), (FLOAT *)&view);

		views[i] = view;
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Viewports, views, 0, NumViewports);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumViews, NumViewports);
		m_pRealContext->RSSetViewports(NumViewports, views);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_VIEWPORTS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_RSSetViewports(NumViewports, pViewports);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Viewports, pViewports, 0, NumViewports);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumViews, NumViewports);
	m_pRealContext->RSSetViewports(NumViewports, pViewports);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_RSSetScissorRects(UINT NumRects_, const D3D11_RECT *pRects_)
{
	SERIALISE_ELEMENT(uint32_t, NumRects, NumRects_);
	SERIALISE_ELEMENT_ARR(D3D11_RECT, Rects, pRects_, NumRects);
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Scissors, Rects, 0, NumRects);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumScissors, NumRects);
		RSSetScissorRects(NumRects, Rects);
		VerifyState();
	}

	SAFE_DELETE(Rects);

	return true;
}

void WrappedID3D11DeviceContext::RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_SCISSORS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_RSSetScissorRects(NumRects, pRects);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.Scissors, pRects, 0, NumRects);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->RS.NumScissors, NumRects);
	m_pRealContext->RSSetScissorRects(NumRects, pRects);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_RSSetState(ID3D11RasterizerState *pRasterizerState)
{
	SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(pRasterizerState));
	
	if(m_State <= EXECUTING)
	{
		ID3D11DeviceChild *live = m_pDevice->GetResourceManager()->GetLiveResource(id);
#if defined(INCLUDE_D3D_11_1)
		if(WrappedID3D11RasterizerState1::IsAlloc(live))
		{
			ID3D11RasterizerState1 *state = (ID3D11RasterizerState1 *)live;
			m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->RS.State, (ID3D11RasterizerState *)state);
			m_pRealContext->RSSetState((ID3D11RasterizerState *)UNWRAP(WrappedID3D11RasterizerState1, state));
		}
		else
#endif
		{
			ID3D11RasterizerState *state = (ID3D11RasterizerState *)live;
			m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->RS.State, state);
			m_pRealContext->RSSetState(UNWRAP(WrappedID3D11RasterizerState, state));
		}
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::RSSetState(ID3D11RasterizerState *pRasterizerState)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_RASTER);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_RSSetState(pRasterizerState);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

#if defined(INCLUDE_D3D_11_1)
	RDCASSERT(!pRasterizerState || WrappedID3D11RasterizerState::IsAlloc(pRasterizerState) || WrappedID3D11RasterizerState1::IsAlloc(pRasterizerState));
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->RS.State, pRasterizerState);
	if(!pRasterizerState || WrappedID3D11RasterizerState::IsAlloc(pRasterizerState))
		m_pRealContext->RSSetState(UNWRAP(WrappedID3D11RasterizerState, pRasterizerState));
	else
		m_pRealContext->RSSetState((ID3D11RasterizerState *)UNWRAP(WrappedID3D11RasterizerState1, pRasterizerState));
#else
	RDCASSERT(!pRasterizerState || WrappedID3D11RasterizerState::IsAlloc(pRasterizerState));
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->RS.State, pRasterizerState);
	m_pRealContext->RSSetState(UNWRAP(WrappedID3D11RasterizerState, pRasterizerState));
#endif

	VerifyState();
}

#pragma endregion Rasterizer

#pragma region Pixel Shader

void WrappedID3D11DeviceContext::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	if(ppConstantBuffers)
	{
		ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
		m_pRealContext->PSGetConstantBuffers(StartSlot, NumBuffers, real);
	
		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppConstantBuffers[i]);

			RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->PS.ConstantBuffers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	if(ppShaderResourceViews)
	{
		ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
		m_pRealContext->PSGetShaderResources(StartSlot, NumViews, real);

		for(UINT i=0; i < NumViews; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppShaderResourceViews[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppShaderResourceViews[i]);

			RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->PS.SRVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	if(ppSamplers)
	{
		ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
		m_pRealContext->PSGetSamplers(StartSlot, NumSamplers, real);

		for(UINT i=0; i < NumSamplers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSamplers[i]);

			RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->PS.Samplers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::PSGetShader(ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	if(ppPixelShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
		return;
	
	ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
	UINT numInsts = 0;
	ID3D11PixelShader *realShader = NULL;
	m_pRealContext->PSGetShader(&realShader, realInsts, &numInsts);
	
	SAFE_RELEASE_NOCLEAR(realShader);
	for(UINT i=0; i < numInsts; i++)
		SAFE_RELEASE_NOCLEAR(realInsts[i]);

	if(ppPixelShader)
	{
		*ppPixelShader = (ID3D11PixelShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
		SAFE_ADDREF(*ppPixelShader);

		RDCASSERT(*ppPixelShader == m_CurrentPipelineState->PS.Shader);
	}

	if(ppClassInstances)
	{
		for(UINT i=0; i < numInsts; i++)
		{
			ppClassInstances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
			SAFE_ADDREF(ppClassInstances[i]);

			RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->PS.Instances[i]);
		}
	}

	if(pNumClassInstances)
	{
		*pNumClassInstances = numInsts;
	}
}

bool WrappedID3D11DeviceContext::Serialise_PSSetConstantBuffers(UINT StartSlot_, UINT NumBuffers_, ID3D11Buffer *const *ppConstantBuffers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	ID3D11Buffer *Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppConstantBuffers[i]));

		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers, Buffers, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

		for(UINT i=0; i < NumBuffers; i++)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);

		m_pRealContext->PSSetConstantBuffers(StartSlot, NumBuffers, Buffers);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_PS_CBUFFERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.ConstantBuffers, ppConstantBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

	ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppConstantBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
	}

	m_pRealContext->PSSetConstantBuffers(StartSlot, NumBuffers, bufs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_PSSetShaderResources(UINT StartSlot_, UINT NumViews_, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);
	
	ID3D11ShaderResourceView *Views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppShaderResourceViews[i]));

		if(m_State <= EXECUTING)
			Views[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.SRVs, Views, StartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			Views[i] = UNWRAP(WrappedID3D11ShaderResourceView, Views[i]);

		m_pRealContext->PSSetShaderResources(StartSlot, NumViews, Views);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_PS_RESOURCES);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.SRVs, ppShaderResourceViews, StartSlot, NumViews);
	
	ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppShaderResourceViews[i] && m_State >= WRITING_CAPFRAME)
		{
			ID3D11Resource *res = NULL;
			ppShaderResourceViews[i]->GetResource(&res);
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
			SAFE_RELEASE(res);
		}

		SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView, ppShaderResourceViews[i]);
	}

	m_pRealContext->PSSetShaderResources(StartSlot, NumViews, SRVs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_PSSetSamplers(UINT StartSlot_, UINT NumSamplers_, ID3D11SamplerState *const *ppSamplers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumSamplers, NumSamplers_);

	ID3D11SamplerState *Sampler[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

	for(UINT i=0; i < NumSamplers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSamplers[i]));

		if(m_State <= EXECUTING)
			Sampler[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Samplers, Sampler, StartSlot, NumSamplers);

		for(UINT i=0; i < NumSamplers; i++)
			Sampler[i] = UNWRAP(WrappedID3D11SamplerState, Sampler[i]);

		m_pRealContext->PSSetSamplers(StartSlot, NumSamplers, Sampler);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_PS_SAMPLERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Samplers, ppSamplers, StartSlot, NumSamplers);

	ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for(UINT i=0; i < NumSamplers; i++)
		samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

	m_pRealContext->PSSetSamplers(StartSlot, NumSamplers, samps);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_PSSetShader(ID3D11PixelShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances_)
{
	SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(pShader));
	SERIALISE_ELEMENT(uint32_t, NumClassInstances, NumClassInstances_);
	
	ID3D11ClassInstance **Instances = new ID3D11ClassInstance *[NumClassInstances];

	for(UINT i=0; i < NumClassInstances; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppClassInstances[i]));

		if(m_State <= EXECUTING)
			Instances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Instances, Instances, 0, NumClassInstances);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.NumInstances, NumClassInstances);

		for(UINT i=0; i < NumClassInstances; i++)
			Instances[i] = UNWRAP(WrappedID3D11ClassInstance, Instances[i]);

		ID3D11DeviceChild *pShader = (ID3D11DeviceChild *)m_pDevice->GetResourceManager()->GetLiveResource(Shader);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Shader, pShader);
		m_pRealContext->PSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, pShader), Instances, NumClassInstances);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::PSSetShader(ID3D11PixelShader *pPixelShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_PS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
		
		MarkResourceReferenced(GetIDForResource(pPixelShader), eFrameRef_Read);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Shader, (ID3D11DeviceChild *)pPixelShader);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->PS.NumInstances, NumClassInstances);
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->PS.Instances, ppClassInstances, 0, NumClassInstances);
	
	ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
	if(ppClassInstances && NumClassInstances > 0)
		for(UINT i=0; i < NumClassInstances; i++)
			insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);
	
	m_pRealContext->PSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, pPixelShader), insts, NumClassInstances);
	VerifyState();
}

#pragma endregion Pixel Shader

#pragma region Output Merger

void WrappedID3D11DeviceContext::OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView)
{
	if(ppRenderTargetViews == NULL && ppDepthStencilView == NULL)
		return;

	ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
	ID3D11DepthStencilView *dsv = NULL;
	m_pRealContext->OMGetRenderTargets(NumViews, rtv, &dsv);
	
	for(UINT i=0; i < NumViews; i++)
		SAFE_RELEASE_NOCLEAR(rtv[i]);

	SAFE_RELEASE_NOCLEAR(dsv);

	if(ppRenderTargetViews)
	{
		for(UINT i=0; i < NumViews; i++)
		{
			ppRenderTargetViews[i] = (ID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetWrapper(rtv[i]);
			SAFE_ADDREF(ppRenderTargetViews[i]);

			RDCASSERT(ppRenderTargetViews[i] == m_CurrentPipelineState->OM.RenderTargets[i]);
		}
	}

	if(ppDepthStencilView)
	{
		*ppDepthStencilView = (ID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetWrapper(dsv);
		SAFE_ADDREF(*ppDepthStencilView);

		RDCASSERT(*ppDepthStencilView == m_CurrentPipelineState->OM.DepthView);
	}
}

void WrappedID3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews,
												ID3D11DepthStencilView **ppDepthStencilView,
												UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	if(ppRenderTargetViews == NULL && ppDepthStencilView == NULL && ppUnorderedAccessViews == NULL)
		return;
	
	ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
	ID3D11UnorderedAccessView *uav[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
	ID3D11DepthStencilView *dsv = NULL;
	m_pRealContext->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, rtv, &dsv, UAVStartSlot, NumUAVs, uav);
	
	for(UINT i=0; i < NumRTVs; i++)
		SAFE_RELEASE_NOCLEAR(rtv[i]);

	SAFE_RELEASE_NOCLEAR(dsv);
	
	for(UINT i=0; i < NumUAVs; i++)
		SAFE_RELEASE_NOCLEAR(uav[i]);

	if(ppRenderTargetViews)
	{
		for(UINT i=0; i < NumRTVs; i++)
		{
			ppRenderTargetViews[i] = (ID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetWrapper(rtv[i]);
			SAFE_ADDREF(ppRenderTargetViews[i]);

			RDCASSERT(ppRenderTargetViews[i] == m_CurrentPipelineState->OM.RenderTargets[i]);
		}
	}

	if(ppDepthStencilView)
	{
		*ppDepthStencilView = (ID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetWrapper(dsv);
		SAFE_ADDREF(*ppDepthStencilView);

		RDCASSERT(*ppDepthStencilView == m_CurrentPipelineState->OM.DepthView);
	}
	
	if(ppUnorderedAccessViews)
	{
		for(UINT i=0; i < NumUAVs; i++)
		{
			ppUnorderedAccessViews[i] = (ID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetWrapper(uav[i]);
			SAFE_ADDREF(ppUnorderedAccessViews[i]);

			RDCASSERT(ppUnorderedAccessViews[i] == m_CurrentPipelineState->OM.UAVs[i]);
		}
	}
}

void WrappedID3D11DeviceContext::OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask)
{
	ID3D11BlendState *real = NULL;
	m_pRealContext->OMGetBlendState(&real, BlendFactor, pSampleMask);
	
	SAFE_RELEASE_NOCLEAR(real);

	if(ppBlendState)
	{
		if(real != NULL)
		{
			ID3D11DeviceChild *state = m_pDevice->GetResourceManager()->GetWrapper(real);
#if defined(INCLUDE_D3D_11_1)
			if(WrappedID3D11BlendState1::IsAlloc(state))
			{
				*ppBlendState = (ID3D11BlendState *)(ID3D11BlendState1 *)state;
				(*ppBlendState)->AddRef();
			}
			else
#endif
			{
				*ppBlendState = (ID3D11BlendState *)state;
				(*ppBlendState)->AddRef();
			}
		}
		else
		{
			*ppBlendState = NULL;
		}

		RDCASSERT(*ppBlendState == m_CurrentPipelineState->OM.BlendState);
	}
	if(BlendFactor)
		RDCASSERT(memcmp(BlendFactor, m_CurrentPipelineState->OM.BlendFactor, sizeof(float)*4) == 0);
	if(pSampleMask)
		RDCASSERT(*pSampleMask == m_CurrentPipelineState->OM.SampleMask);
}

void WrappedID3D11DeviceContext::OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef)
{
	ID3D11DepthStencilState *real = NULL;
	m_pRealContext->OMGetDepthStencilState(&real, pStencilRef);

	SAFE_RELEASE_NOCLEAR(real);

	if(ppDepthStencilState)
	{
		if(real != NULL)
		{
			*ppDepthStencilState = (ID3D11DepthStencilState *)m_pDevice->GetResourceManager()->GetWrapper(real);
			SAFE_ADDREF(*ppDepthStencilState);
		}
		else
		{
			*ppDepthStencilState = NULL;
		}

		RDCASSERT(*ppDepthStencilState == m_CurrentPipelineState->OM.DepthStencilState);
	}
	if(pStencilRef)
		RDCASSERT(*pStencilRef == m_CurrentPipelineState->OM.StencRef);
}

bool WrappedID3D11DeviceContext::Serialise_OMSetRenderTargets(UINT NumViews_, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView)
{
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);
	SERIALISE_ELEMENT(ResourceId, DepthStencilView, GetIDForResource(pDepthStencilView));

	ID3D11RenderTargetView **RenderTargetViews = new ID3D11RenderTargetView *[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	
	for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		RenderTargetViews[i] = NULL;

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppRenderTargetViews[i]));

		if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(id))
			RenderTargetViews[i] = (ID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		pDepthStencilView = NULL;
		if(m_pDevice->GetResourceManager()->HasLiveResource(DepthStencilView))
			pDepthStencilView = (ID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetLiveResource(DepthStencilView);

		if(m_CurrentPipelineState->ValidOutputMerger(RenderTargetViews, pDepthStencilView))
		{
			m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RenderTargetViews, 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
			m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
		}

		ID3D11UnorderedAccessView *UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
		m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0, D3D11_PS_CS_UAV_REGISTER_COUNT);
	
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			RenderTargetViews[i] = UNWRAP(WrappedID3D11RenderTargetView, RenderTargetViews[i]);

		m_pRealContext->OMSetRenderTargets(NumViews, RenderTargetViews,
												UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView));
		VerifyState();
	}
	
	SAFE_DELETE_ARRAY(RenderTargetViews);

	return true;
}

void WrappedID3D11DeviceContext::OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_SMALL_CONTEXT(SET_RTARGET);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	ID3D11RenderTargetView *RTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
	for(UINT i=0; i < NumViews && ppRenderTargetViews; i++)
		RTs[i] = ppRenderTargetViews[i];

	// this function always sets all render targets
	if(m_CurrentPipelineState->ValidOutputMerger(RTs, pDepthStencilView))
	{
		m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RTs, 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
		m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, NumViews);
	}

	ID3D11UnorderedAccessView *UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
	m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0, D3D11_PS_CS_UAV_REGISTER_COUNT);
	
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppRenderTargetViews[i] && m_State >= WRITING)
		{
			ID3D11Resource *res = NULL;
			ppRenderTargetViews[i]->GetResource(&res);
			// technically this isn't dirty until the draw call, but let's be conservative
			// to avoid having to track "possibly" dirty resources.
			// Besides, it's unlikely an application will set an output then not draw to it
			if(m_State == WRITING_IDLE)
				m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}

		RTs[i] = UNWRAP(WrappedID3D11RenderTargetView, ppRenderTargetViews[i]);
	}

	if(pDepthStencilView && m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pDepthStencilView->GetResource(&res);

		if(m_State == WRITING_IDLE)
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(res));
		SAFE_RELEASE(res);
	}

	m_pRealContext->OMSetRenderTargets(NumViews, RTs, UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView));
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_OMSetRenderTargetsAndUnorderedAccessViews(
												UINT NumRTVs_, ID3D11RenderTargetView *const *ppRenderTargetViews,
												ID3D11DepthStencilView *pDepthStencilView,
												UINT UAVStartSlot_, UINT NumUAVs_, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
												const UINT *pUAVInitialCounts)
{
	SERIALISE_ELEMENT(ResourceId, DepthStencilView, GetIDForResource(pDepthStencilView));

	SERIALISE_ELEMENT(uint32_t, NumRTVs, NumRTVs_);

	SERIALISE_ELEMENT(uint32_t, UAVStartSlot, UAVStartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumUAVs, NumUAVs_);

	ID3D11RenderTargetView **RenderTargetViews = NULL;
	ID3D11UnorderedAccessView **UnorderedAccessViews = NULL;

	if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
	{
		RenderTargetViews = new ID3D11RenderTargetView *[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for(UINT i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
			RenderTargetViews[i] = NULL;
	} 

	if(NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
	{
		UnorderedAccessViews = new ID3D11UnorderedAccessView *[D3D11_PS_CS_UAV_REGISTER_COUNT];
		for(UINT i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
			UnorderedAccessViews[i] = NULL;
	}

	SERIALISE_ELEMENT(uint8_t, HasInitialCounts, pUAVInitialCounts != NULL);

	SERIALISE_ELEMENT_ARR_OPT(uint32_t, UAVInitialCounts, pUAVInitialCounts, NumUAVs, HasInitialCounts && NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS);

	RDCASSERT(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL || NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS);

	for(UINT i=0; i < NumRTVs && NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppRenderTargetViews[i]));

		if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(id))
			RenderTargetViews[i] = (ID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	for(UINT i=0; i < NumUAVs && NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppUnorderedAccessViews[i]));

		if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(id))
			UnorderedAccessViews[i] = (ID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{	
		pDepthStencilView = NULL;
		if(m_pDevice->GetResourceManager()->HasLiveResource(DepthStencilView))
			pDepthStencilView = (ID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetLiveResource(DepthStencilView);

		if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
		{
			if(m_CurrentPipelineState->ValidOutputMerger(RenderTargetViews, pDepthStencilView))
			{
				m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RenderTargetViews, 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
				m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
			}
		}

		if(NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
		{
			m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UnorderedAccessViews, 0, D3D11_PS_CS_UAV_REGISTER_COUNT);
			m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, UAVStartSlot);
		}

		for(UINT i=0; i < NumRTVs && NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL; i++)
			RenderTargetViews[i] = UNWRAP(WrappedID3D11RenderTargetView, RenderTargetViews[i]);

		for(UINT i=0; i < NumUAVs && NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS; i++)
			UnorderedAccessViews[i] = UNWRAP(WrappedID3D11UnorderedAccessView, UnorderedAccessViews[i]);

		if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
			pDepthStencilView = UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView);
		else
			pDepthStencilView = NULL;

		m_pRealContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, RenderTargetViews,
												pDepthStencilView,
												UAVStartSlot, NumUAVs, UnorderedAccessViews, UAVInitialCounts);
		VerifyState();
	}
	
	SAFE_DELETE_ARRAY(RenderTargetViews);
	SAFE_DELETE_ARRAY(UnorderedAccessViews);
	SAFE_DELETE_ARRAY(UAVInitialCounts);

	return true;
}

void WrappedID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
												ID3D11DepthStencilView *pDepthStencilView,
												UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
												const UINT *pUAVInitialCounts)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_RTARGET_AND_UAVS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	ID3D11RenderTargetView *RTs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
	ID3D11UnorderedAccessView *UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
	
	for(UINT i=0; ppRenderTargetViews && i < NumRTVs; i++)
		RTs[i] = ppRenderTargetViews[i];
	
	for(UINT i=0; ppUnorderedAccessViews && i < NumUAVs; i++)
		UAVs[i] = ppUnorderedAccessViews[i];

	if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
	{
		if(m_CurrentPipelineState->ValidOutputMerger(RTs, pDepthStencilView))
		{
			m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.RenderTargets, RTs, 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
			m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.DepthView, pDepthStencilView);
		}
	}

	if(NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
	{
		m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->OM.UAVs, UAVs, 0, D3D11_PS_CS_UAV_REGISTER_COUNT);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.UAVStartSlot, UAVStartSlot);
	}

	for(UINT i=0; ppRenderTargetViews && i < NumRTVs; i++)
	{
		if(ppRenderTargetViews[i] && m_State >= WRITING)
		{
			ID3D11Resource *res = NULL;
			ppRenderTargetViews[i]->GetResource(&res);
			// technically this isn't dirty until the draw call, but let's be conservative
			// to avoid having to track "possibly" dirty resources.
			// Besides, it's unlikely an application will set an output then not draw to it
			if(m_State == WRITING_IDLE)
				m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}

		RTs[i] = UNWRAP(WrappedID3D11RenderTargetView, ppRenderTargetViews[i]);
	}
	
	for(UINT i=0; ppUnorderedAccessViews && i < NumUAVs; i++)
	{
		if(ppUnorderedAccessViews[i] && m_State >= WRITING)
		{
			ID3D11Resource *res = NULL;
			ppUnorderedAccessViews[i]->GetResource(&res);
			if(m_State == WRITING_IDLE)
				m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}

		UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView, ppUnorderedAccessViews[i]);
	}

	if(pDepthStencilView && m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pDepthStencilView->GetResource(&res);

		if(m_State == WRITING_IDLE)
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(res));
		SAFE_RELEASE(res);
	}

	m_pRealContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews ? RTs : NULL, UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView),
																	UAVStartSlot, NumUAVs, ppUnorderedAccessViews ? UAVs : NULL, pUAVInitialCounts);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor_[4], UINT SampleMask_)
{
	SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(pBlendState));
	
	float BlendFactor[4] = {0};

	if(m_State >= WRITING)
	{
		if((const FLOAT *)BlendFactor_ == NULL)
		{
			BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 1.0f;
		}
		else
		{
			memcpy(BlendFactor, BlendFactor_, sizeof(float)*4);
		}
	}

	m_pSerialiser->Serialise<4>("BlendFactor", BlendFactor);

	SERIALISE_ELEMENT(uint32_t, SampleMask, SampleMask_);

	if(m_State <= EXECUTING)
	{
		ID3D11DeviceChild *live = m_pDevice->GetResourceManager()->GetLiveResource(State);
#if defined(INCLUDE_D3D_11_1)
		if(WrappedID3D11BlendState1::IsAlloc(live))
		{
			ID3D11BlendState1 *state = (ID3D11BlendState1 *)live;
			m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.BlendState, (ID3D11BlendState *)state);
			m_pRealContext->OMSetBlendState((ID3D11BlendState *)UNWRAP(WrappedID3D11BlendState1, state), BlendFactor, SampleMask);
		}
		else
#endif
		{
			ID3D11BlendState *state = (ID3D11BlendState *)live;
			m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.BlendState, state);
			m_pRealContext->OMSetBlendState(UNWRAP(WrappedID3D11BlendState, state), BlendFactor, SampleMask);
		}
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.BlendFactor, BlendFactor, 0, 4);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.SampleMask, SampleMask);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_BLEND);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_OMSetBlendState(pBlendState, BlendFactor, SampleMask);

		m_ContextRecord->AddChunk(scope.Get());
	}

	FLOAT DefaultBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.BlendState, pBlendState);
	if(BlendFactor != NULL)
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.BlendFactor, BlendFactor, 0, 4);
	else
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.BlendFactor, DefaultBlendFactor, 0, 4);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.SampleMask, SampleMask);

#if defined(INCLUDE_D3D_11_1)
	if(!pBlendState || WrappedID3D11BlendState::IsAlloc(pBlendState))
		m_pRealContext->OMSetBlendState(UNWRAP(WrappedID3D11BlendState, pBlendState), BlendFactor, SampleMask);
	else
		m_pRealContext->OMSetBlendState((ID3D11BlendState *)UNWRAP(WrappedID3D11BlendState1, pBlendState), BlendFactor, SampleMask);
#else
	m_pRealContext->OMSetBlendState(UNWRAP(WrappedID3D11BlendState, pBlendState), BlendFactor, SampleMask);
#endif

	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef_)
{
	SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(pDepthStencilState));
	SERIALISE_ELEMENT(uint32_t, StencilRef, StencilRef_&0xff);

	if(m_State <= EXECUTING)
	{
		pDepthStencilState = (ID3D11DepthStencilState *)m_pDevice->GetResourceManager()->GetLiveResource(State);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.DepthStencilState, pDepthStencilState);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.StencRef, StencilRef&0xff);
		m_pRealContext->OMSetDepthStencilState(UNWRAP(WrappedID3D11DepthStencilState, pDepthStencilState), StencilRef);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_DEPTHSTENCIL);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_OMSetDepthStencilState(pDepthStencilState, StencilRef);

		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->OM.DepthStencilState, pDepthStencilState);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->OM.StencRef, StencilRef&0xff);
	m_pRealContext->OMSetDepthStencilState(UNWRAP(WrappedID3D11DepthStencilState, pDepthStencilState), StencilRef);
	VerifyState();
}

#pragma endregion Output Merger

#pragma region Draw

vector<DebugMessage> WrappedID3D11DeviceContext::Serialise_DebugMessages()
{
	SCOPED_SERIALISE_CONTEXT(DEBUG_MESSAGES);
	
	vector<DebugMessage> debugMessages;

	m_EmptyCommandList = false;

	// only grab debug messages for the immediate context, without serialising all
	// API use there's no way to find out which messages come from which context :(.
	if(m_State == WRITING_CAPFRAME && GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
	{
		debugMessages = m_pDevice->GetDebugMessages();
	}

	SERIALISE_ELEMENT(bool, HasCallstack, RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws != 0);

	if(HasCallstack)
	{
		if(m_State >= WRITING)
		{
			Callstack::Stackwalk *call = Callstack::Collect();

			RDCASSERT(call->NumLevels() < 0xff);

			size_t numLevels = call->NumLevels();
			uint64_t *stack = call->GetAddrs();

			m_pSerialiser->Serialise("callstack", stack, numLevels);

			delete call;
		}
		else
		{
			size_t numLevels = 0;
			uint64_t *stack = NULL;

			m_pSerialiser->Serialise("callstack", stack, numLevels);

			m_pSerialiser->SetCallstack(stack, numLevels);

			SAFE_DELETE_ARRAY(stack);
		}
	}

	SERIALISE_ELEMENT(uint32_t, NumMessages, (uint32_t)debugMessages.size());

	for(uint32_t i=0; i < NumMessages; i++)
	{
		ScopedContext scope(m_pSerialiser, m_pDebugSerialiser, "DebugMessage", "DebugMessage", 0, false);

		string desc;
		if(m_State >= WRITING)
			desc = debugMessages[i].description.elems;

		SERIALISE_ELEMENT(uint32_t, Category, debugMessages[i].category);
		SERIALISE_ELEMENT(uint32_t, Severity, debugMessages[i].severity);
		SERIALISE_ELEMENT(uint32_t, ID, debugMessages[i].messageID);
		SERIALISE_ELEMENT(string, Description, desc);

		if(m_State == READING)
		{
			DebugMessage msg;
			msg.category = (DebugMessageCategory)Category;
			msg.severity = (DebugMessageSeverity)Severity;
			msg.messageID = ID;
			msg.description = Description;

			debugMessages.push_back(msg);
		}
	}

	return debugMessages;
}


bool WrappedID3D11DeviceContext::Serialise_DrawIndexedInstanced(UINT IndexCountPerInstance_, UINT InstanceCount_, UINT StartIndexLocation_,
																INT BaseVertexLocation_, UINT StartInstanceLocation_)
{
	SERIALISE_ELEMENT(uint32_t, IndexCountPerInstance, IndexCountPerInstance_);
	SERIALISE_ELEMENT(uint32_t, InstanceCount, InstanceCount_);
	SERIALISE_ELEMENT(uint32_t, StartIndexLocation, StartIndexLocation_);
	SERIALISE_ELEMENT(int32_t, BaseVertexLocation, BaseVertexLocation_);
	SERIALISE_ELEMENT(uint32_t, StartInstanceLocation, StartInstanceLocation_);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_INDEXED_INST, desc);
		string name = "DrawIndexedInstanced(" + ToStr::Get(IndexCountPerInstance)
						+ ", " + ToStr::Get(InstanceCount) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = IndexCountPerInstance;
		draw.numInstances = InstanceCount;
		draw.indexOffset = StartIndexLocation;
		draw.vertexOffset = BaseVertexLocation;
		draw.instanceOffset = StartInstanceLocation;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced|eDraw_UseIBuffer;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
												INT BaseVertexLocation, UINT StartInstanceLocation)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INST);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}
}

bool WrappedID3D11DeviceContext::Serialise_DrawInstanced(UINT VertexCountPerInstance_, UINT InstanceCount_, UINT StartVertexLocation_, UINT StartInstanceLocation_)
{
	SERIALISE_ELEMENT(uint32_t, VertexCountPerInstance, VertexCountPerInstance_);
	SERIALISE_ELEMENT(uint32_t, InstanceCount, InstanceCount_);
	SERIALISE_ELEMENT(uint32_t, StartVertexLocation, StartVertexLocation_);
	SERIALISE_ELEMENT(uint32_t, StartInstanceLocation, StartInstanceLocation_);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_INST, desc);
		string name = "DrawInstanced(" + ToStr::Get(VertexCountPerInstance)
						+ ", " + ToStr::Get(InstanceCount) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = VertexCountPerInstance;
		draw.numInstances = InstanceCount;
		draw.vertexOffset = StartVertexLocation;
		draw.instanceOffset = StartInstanceLocation;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_INST);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}
}

bool WrappedID3D11DeviceContext::Serialise_DrawIndexed(UINT IndexCount_, UINT StartIndexLocation_, INT BaseVertexLocation_)
{
	SERIALISE_ELEMENT(uint32_t, IndexCount, IndexCount_);
	SERIALISE_ELEMENT(uint32_t, StartIndexLocation, StartIndexLocation_);
	SERIALISE_ELEMENT(int32_t, BaseVertexLocation, BaseVertexLocation_);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_INDEXED, desc);
		string name = "DrawIndexed(" + ToStr::Get(IndexCount) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = IndexCount;
		draw.vertexOffset = BaseVertexLocation;
		draw.indexOffset = StartIndexLocation;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_SMALL_CONTEXT(DRAW_INDEXED);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}
}

bool WrappedID3D11DeviceContext::Serialise_Draw(UINT VertexCount_, UINT StartVertexLocation_)
{
	SERIALISE_ELEMENT(uint32_t, VertexCount, VertexCount_);
	SERIALISE_ELEMENT(uint32_t, StartVertexLocation, StartVertexLocation_);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->Draw(VertexCount, StartVertexLocation);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW, desc);
		string name = "Draw(" + ToStr::Get(VertexCount) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = VertexCount;
		draw.vertexOffset = StartVertexLocation;

		draw.flags |= eDraw_Drawcall;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::Draw(UINT VertexCount, UINT StartVertexLocation)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->Draw(VertexCount, StartVertexLocation);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_Draw(VertexCount, StartVertexLocation);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}
}

bool WrappedID3D11DeviceContext::Serialise_DrawAuto()
{
	if(m_State <= EXECUTING)
	{
		m_pRealContext->DrawAuto();
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_AUTO, desc);
		string name = "DrawAuto()";

		// Not implemented. Need to D3D11_QUERY_SO_STATISTICS to find out the
		// index count etc to fill out FetchDrawcall
		RDCUNIMPLEMENTED("Not fetching draw data for DrawAuto() display");

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Drawcall|eDraw_Auto;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DrawAuto()
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DrawAuto();

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_AUTO);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DrawAuto();
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}
}

bool WrappedID3D11DeviceContext::Serialise_DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs_)
{
	SERIALISE_ELEMENT(ResourceId, BufferForArgs, GetIDForResource(pBufferForArgs));
	SERIALISE_ELEMENT(uint32_t, AlignedByteOffsetForArgs, AlignedByteOffsetForArgs_);
	
	if(m_State <= EXECUTING)
	{
		m_pRealContext->DrawIndexedInstancedIndirect(UNWRAP(WrappedID3D11Buffer, m_pDevice->GetResourceManager()->GetLiveResource(BufferForArgs)), AlignedByteOffsetForArgs);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW, desc);
		
		ID3D11Buffer *argBuffer = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(BufferForArgs);

		vector<byte> args = m_pDevice->GetDebugManager()->GetBufferData(argBuffer, AlignedByteOffsetForArgs, 5*sizeof(uint32_t));
		uint32_t *uargs = (uint32_t *)&args[0];

		string name = "DrawIndexedInstancedIndirect(<" + ToStr::Get(uargs[0])
						+ ", " + ToStr::Get(uargs[1]) + ">)";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = uargs[0];
		draw.numInstances = uargs[1];
		draw.indexOffset = uargs[2];
		draw.vertexOffset = uargs[3];
		draw.instanceOffset = uargs[4];

		draw.flags |= eDraw_Drawcall|eDraw_Instanced|eDraw_UseIBuffer|eDraw_Indirect;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DrawIndexedInstancedIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs), AlignedByteOffsetForArgs);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INST_INDIRECT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}

	if(pBufferForArgs && m_State >= WRITING_CAPFRAME)
		MarkResourceReferenced(GetIDForResource(pBufferForArgs), eFrameRef_Read);
}

bool WrappedID3D11DeviceContext::Serialise_DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs_)
{
	SERIALISE_ELEMENT(ResourceId, BufferForArgs, GetIDForResource(pBufferForArgs));
	SERIALISE_ELEMENT(uint32_t, AlignedByteOffsetForArgs, AlignedByteOffsetForArgs_);
	
	if(m_State <= EXECUTING)
	{
		m_pRealContext->DrawInstancedIndirect(UNWRAP(WrappedID3D11Buffer, m_pDevice->GetResourceManager()->GetLiveResource(BufferForArgs)), AlignedByteOffsetForArgs);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW, desc);
		
		ID3D11Buffer *argBuffer = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(BufferForArgs);

		vector<byte> args = m_pDevice->GetDebugManager()->GetBufferData(argBuffer, AlignedByteOffsetForArgs, 4*sizeof(uint32_t));
		uint32_t *uargs = (uint32_t *)&args[0];

		string name = "DrawInstancedIndirect(<" + ToStr::Get(uargs[0])
						+ ", " + ToStr::Get(uargs[1]) + ">)";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = uargs[0];
		draw.numInstances = uargs[1];
		draw.vertexOffset = uargs[2];
		draw.instanceOffset = uargs[3];

		draw.flags |= eDraw_Drawcall|eDraw_Instanced|eDraw_Indirect;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DrawInstancedIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs), AlignedByteOffsetForArgs);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_INST_INDIRECT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}

	if(pBufferForArgs && m_State >= WRITING_CAPFRAME)
		MarkResourceReferenced(GetIDForResource(pBufferForArgs), eFrameRef_Read);
}

#pragma endregion Draw

#pragma region Compute Shader

void WrappedID3D11DeviceContext::CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	if(ppConstantBuffers)
	{
		ID3D11Buffer *real[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {0};
		m_pRealContext->CSGetConstantBuffers(StartSlot, NumBuffers, real);
	
		for(UINT i=0; i < NumBuffers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppConstantBuffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppConstantBuffers[i]);

			RDCASSERT(ppConstantBuffers[i] == m_CurrentPipelineState->CS.ConstantBuffers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	if(ppShaderResourceViews)
	{
		ID3D11ShaderResourceView *real[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {0};
		m_pRealContext->CSGetShaderResources(StartSlot, NumViews, real);

		for(UINT i=0; i < NumViews; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppShaderResourceViews[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppShaderResourceViews[i]);

			RDCASSERT(ppShaderResourceViews[i] == m_CurrentPipelineState->CS.SRVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	if(ppUnorderedAccessViews)
	{
		ID3D11UnorderedAccessView *real[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
		m_pRealContext->CSGetUnorderedAccessViews(StartSlot, NumUAVs, real);

		for(UINT i=0; i < NumUAVs; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppUnorderedAccessViews[i] = (ID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppUnorderedAccessViews[i]);

			RDCASSERT(ppUnorderedAccessViews[i] == m_CurrentPipelineState->CS.UAVs[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	if(ppSamplers)
	{
		ID3D11SamplerState *real[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {0};
		m_pRealContext->CSGetSamplers(StartSlot, NumSamplers, real);

		for(UINT i=0; i < NumSamplers; i++)
		{
			SAFE_RELEASE_NOCLEAR(real[i]);
			ppSamplers[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetWrapper(real[i]);
			SAFE_ADDREF(ppSamplers[i]);

			RDCASSERT(ppSamplers[i] == m_CurrentPipelineState->CS.Samplers[i+StartSlot]);
		}
	}
}

void WrappedID3D11DeviceContext::CSGetShader(ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	if(ppComputeShader == NULL && ppClassInstances == NULL && pNumClassInstances == NULL)
		return;
	
	ID3D11ClassInstance *realInsts[D3D11_SHADER_MAX_INTERFACES] = {0};
	UINT numInsts = 0;
	ID3D11ComputeShader *realShader = NULL;
	m_pRealContext->CSGetShader(&realShader, realInsts, &numInsts);
	
	SAFE_RELEASE_NOCLEAR(realShader);
	for(UINT i=0; i < numInsts; i++)
		SAFE_RELEASE_NOCLEAR(realInsts[i]);

	if(ppComputeShader)
	{
		*ppComputeShader = (ID3D11ComputeShader *)m_pDevice->GetResourceManager()->GetWrapper(realShader);
		SAFE_ADDREF(*ppComputeShader);

		RDCASSERT(*ppComputeShader == m_CurrentPipelineState->CS.Shader);
	}

	if(ppClassInstances)
	{
		for(UINT i=0; i < numInsts; i++)
		{
			ppClassInstances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetWrapper(realInsts[i]);
			SAFE_ADDREF(ppClassInstances[i]);

			RDCASSERT(ppClassInstances[i] == m_CurrentPipelineState->CS.Instances[i]);
		}
	}

	if(pNumClassInstances)
	{
		*pNumClassInstances = numInsts;
	}
}

bool WrappedID3D11DeviceContext::Serialise_CSSetConstantBuffers(UINT StartSlot_, UINT NumBuffers_, ID3D11Buffer *const *ppConstantBuffers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumBuffers, NumBuffers_);

	ID3D11Buffer **Buffers = new ID3D11Buffer *[NumBuffers];

	for(UINT i=0; i < NumBuffers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppConstantBuffers[i]));

		if(m_State <= EXECUTING)
			Buffers[i] = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}
	
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers, Buffers, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, NullCBCounts, StartSlot, NumBuffers);
	
		for(UINT i=0; i < NumBuffers; i++)
			Buffers[i] = UNWRAP(WrappedID3D11Buffer, Buffers[i]);
	
		m_pRealContext->CSSetConstantBuffers(StartSlot, NumBuffers, Buffers);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Buffers);

	return true;
}

void WrappedID3D11DeviceContext::CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_CS_CBUFFERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.ConstantBuffers, ppConstantBuffers, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBOffsets, NullCBOffsets, StartSlot, NumBuffers);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.CBCounts, NullCBCounts, StartSlot, NumBuffers);

	ID3D11Buffer *bufs[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for(UINT i=0; i < NumBuffers; i++)
	{
		if(ppConstantBuffers[i] && m_State >= WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(ppConstantBuffers[i]), eFrameRef_Read);

		bufs[i] = UNWRAP(WrappedID3D11Buffer, ppConstantBuffers[i]);
	}

	m_pRealContext->CSSetConstantBuffers(StartSlot, NumBuffers, bufs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_CSSetShaderResources(UINT StartSlot_, UINT NumViews_, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumViews, NumViews_);

	ID3D11ShaderResourceView **Views = new ID3D11ShaderResourceView *[NumViews];

	for(UINT i=0; i < NumViews; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppShaderResourceViews[i]));

		if(m_State <= EXECUTING)
			Views[i] = (ID3D11ShaderResourceView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.SRVs, Views, StartSlot, NumViews);

		for(UINT i=0; i < NumViews; i++)
			Views[i] = UNWRAP(WrappedID3D11ShaderResourceView, Views[i]);

		m_pRealContext->CSSetShaderResources(StartSlot, NumViews, Views);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(Views);

	return true;
}

void WrappedID3D11DeviceContext::CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_CS_RESOURCES);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.SRVs, ppShaderResourceViews, StartSlot, NumViews);
	
	ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumViews; i++)
	{
		if(ppShaderResourceViews[i] && m_State >= WRITING_CAPFRAME)
		{
			ID3D11Resource *res = NULL;
			ppShaderResourceViews[i]->GetResource(&res);
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
			SAFE_RELEASE(res);
		}

		SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView, ppShaderResourceViews[i]);
	}

	m_pRealContext->CSSetShaderResources(StartSlot, NumViews, SRVs);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_CSSetUnorderedAccessViews(UINT StartSlot_, UINT NumUAVs_,
														ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumUAVs, NumUAVs_);
	SERIALISE_ELEMENT(uint8_t, HasInitialCounts, pUAVInitialCounts != NULL);
	SERIALISE_ELEMENT_ARR_OPT(uint32_t, UAVInitialCounts, pUAVInitialCounts, NumUAVs, HasInitialCounts);

	ID3D11UnorderedAccessView **UAVs = new ID3D11UnorderedAccessView *[NumUAVs];
	
	for(UINT i=0; i < NumUAVs; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppUnorderedAccessViews[i]));

		UAVs[i] = NULL;
	
		if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(id))
			UAVs[i] = (WrappedID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->CS.UAVs, UAVs, StartSlot, NumUAVs);

		for(UINT i=0; i < NumUAVs; i++)
			UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView, UAVs[i]);

		m_pRealContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, UAVs, UAVInitialCounts);
		VerifyState();
	}

	SAFE_DELETE_ARRAY(UAVs);
	SAFE_DELETE_ARRAY(UAVInitialCounts);

	return true;
}

void WrappedID3D11DeviceContext::CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
												 const UINT *pUAVInitialCounts)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_CS_UAVS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	m_CurrentPipelineState->ChangeRefWrite(m_CurrentPipelineState->CS.UAVs, ppUnorderedAccessViews, StartSlot, NumUAVs);
	
	ID3D11UnorderedAccessView *UAVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for(UINT i=0; i < NumUAVs; i++)
	{
		if(ppUnorderedAccessViews[i] && m_State >= WRITING)
		{
			ID3D11Resource *res = NULL;
			ppUnorderedAccessViews[i]->GetResource(&res);
			
			if(m_State == WRITING_IDLE)
				m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(res));
			SAFE_RELEASE(res);
		}

		UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView, ppUnorderedAccessViews[i]);
	}

	m_pRealContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, UAVs, pUAVInitialCounts);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_CSSetSamplers(UINT StartSlot_, UINT NumSamplers_, ID3D11SamplerState *const *ppSamplers)
{
	SERIALISE_ELEMENT(uint32_t, StartSlot, StartSlot_);
	SERIALISE_ELEMENT(uint32_t, NumSamplers, NumSamplers_);

	ID3D11SamplerState **Sampler = new ID3D11SamplerState *[NumSamplers];

	for(UINT i=0; i < NumSamplers; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppSamplers[i]));

		if(m_State <= EXECUTING)
			Sampler[i] = (ID3D11SamplerState *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Samplers, Sampler, StartSlot, NumSamplers);

		for(UINT i=0; i < NumSamplers; i++)
			Sampler[i] = UNWRAP(WrappedID3D11SamplerState, Sampler[i]);

		m_pRealContext->CSSetSamplers(StartSlot, NumSamplers, Sampler);
		VerifyState();
	}
		
	SAFE_DELETE_ARRAY(Sampler);

	return true;
}

void WrappedID3D11DeviceContext::CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_CS_SAMPLERS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Samplers, ppSamplers, StartSlot, NumSamplers);
	
	ID3D11SamplerState *samps[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for(UINT i=0; i < NumSamplers; i++)
		samps[i] = UNWRAP(WrappedID3D11SamplerState, ppSamplers[i]);

	m_pRealContext->CSSetSamplers(StartSlot, NumSamplers, samps);
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_CSSetShader(ID3D11ComputeShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances_)
{
	SERIALISE_ELEMENT(ResourceId, Shader, GetIDForResource(pShader));
	SERIALISE_ELEMENT(uint32_t, NumClassInstances, NumClassInstances_);
	
	ID3D11ClassInstance **Instances = new ID3D11ClassInstance *[NumClassInstances];

	for(UINT i=0; i < NumClassInstances; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetIDForResource(ppClassInstances[i]));

		if(m_State <= EXECUTING)
			Instances[i] = (ID3D11ClassInstance *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	}

	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Instances, Instances, 0, NumClassInstances);
		m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.NumInstances, NumClassInstances);

		for(UINT i=0; i < NumClassInstances; i++)
			Instances[i] = UNWRAP(WrappedID3D11ClassInstance, Instances[i]);

		ID3D11DeviceChild *pShader = (ID3D11DeviceChild *)m_pDevice->GetResourceManager()->GetLiveResource(Shader);
		m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Shader, pShader);
		m_pRealContext->CSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11ComputeShader>, pShader), Instances, NumClassInstances);
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::CSSetShader(ID3D11ComputeShader *pComputeShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_CS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
		
		MarkResourceReferenced(GetIDForResource(pComputeShader), eFrameRef_Read);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Shader, (ID3D11DeviceChild *)pComputeShader);
	m_CurrentPipelineState->Change(m_CurrentPipelineState->CS.NumInstances, NumClassInstances);
	m_CurrentPipelineState->ChangeRefRead(m_CurrentPipelineState->CS.Instances, ppClassInstances, 0, NumClassInstances);
	
	ID3D11ClassInstance *insts[D3D11_SHADER_MAX_INTERFACES] = {0};
	if(ppClassInstances && NumClassInstances > 0)
		for(UINT i=0; i < NumClassInstances; i++)
			insts[i] = UNWRAP(WrappedID3D11ClassInstance, ppClassInstances[i]);
	
	m_pRealContext->CSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11ComputeShader>, pComputeShader), insts, NumClassInstances);
	VerifyState();
}

#pragma endregion Compute Shader

#pragma region Execute

bool WrappedID3D11DeviceContext::Serialise_ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState_)
{
	SERIALISE_ELEMENT(uint8_t, RestoreContextState, RestoreContextState_ == TRUE);
	SERIALISE_ELEMENT(ResourceId, cmdList, GetIDForResource(pCommandList));

	RDCASSERT(GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);
	
	if(m_State <= EXECUTING)
	{
		if(m_pDevice->GetResourceManager()->HasLiveResource(cmdList))
			m_pRealContext->ExecuteCommandList(UNWRAP(WrappedID3D11CommandList, m_pDevice->GetResourceManager()->GetLiveResource(cmdList)), RestoreContextState);
		else
			RDCERR("Don't have command list serialised for %llu", cmdList);

		if(!RestoreContextState)
			m_CurrentPipelineState->Clear();

		VerifyState();
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		string name = "ExecuteCommandList(" + ToStr::Get(cmdList) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_CmdList|eDraw_PushMarker;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);

		auto cmdDrawChildren = m_CmdLists.find(cmdList);

		if(!m_DrawcallStack.empty() && !m_DrawcallStack.back()->children.empty() &&
			 cmdDrawChildren != m_CmdLists.end())
		{
			m_DrawcallStack.back()->children.back().children = cmdDrawChildren->second.children;

			// assign new drawcall IDs so that we don't get duplicates if this commandlist is executed again
			RefreshDrawcallIDs(cmdDrawChildren->second);
		}
	}

	return true;
}

void WrappedID3D11DeviceContext::ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;
	
	RDCASSERT(GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	m_pRealContext->ExecuteCommandList(UNWRAP(WrappedID3D11CommandList, pCommandList), RestoreContextState);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(EXECUTE_CMD_LIST);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ExecuteCommandList(pCommandList, RestoreContextState);
		
		m_ContextRecord->AddChunk(scope.Get());

		WrappedID3D11CommandList *wrapped = (WrappedID3D11CommandList *)pCommandList;

		if(!wrapped->IsCaptured())
		{
			// we don't have this command list captured. This frame is no longer successful
			RDCWARN("Don't have command list %llu captured! This frame is unsuccessful.", wrapped->GetResourceID());
			m_SuccessfulCapture = false;
			m_FailureReason = CaptureFailed_UncappedCmdlist;
		}
		else
		{
			RDCDEBUG("Executed successful command list %llu", wrapped->GetResourceID());
			ResourceId contextId = wrapped->GetResourceID();

			D3D11ResourceRecord *cmdListRecord = m_pDevice->GetResourceManager()->GetResourceRecord(contextId);

			if(m_DeferredRecords.find(cmdListRecord) == m_DeferredRecords.end())
			{
				m_DeferredRecords.insert(cmdListRecord);
				cmdListRecord->AddRef();
			}

			cmdListRecord->AddResourceReferences(m_pDevice->GetResourceManager());
		}

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}

	if(!RestoreContextState)
		m_CurrentPipelineState->Clear();

	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_Dispatch(UINT ThreadGroupCountX_, UINT ThreadGroupCountY_, UINT ThreadGroupCountZ_)
{
	SERIALISE_ELEMENT(uint32_t, ThreadGroupCountX, ThreadGroupCountX_);
	SERIALISE_ELEMENT(uint32_t, ThreadGroupCountY, ThreadGroupCountY_);
	SERIALISE_ELEMENT(uint32_t, ThreadGroupCountZ, ThreadGroupCountZ_);
	
	if(m_State <= EXECUTING)
	{
		m_pRealContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DISPATCH, desc);
		string name = "Dispatch("
						+ ToStr::Get(ThreadGroupCountX) + ", "
						+ ToStr::Get(ThreadGroupCountY) + ", "
						+ ToStr::Get(ThreadGroupCountZ) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Dispatch;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISPATCH);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}
}

bool WrappedID3D11DeviceContext::Serialise_DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs_)
{
	SERIALISE_ELEMENT(ResourceId, BufferForArgs, GetIDForResource(pBufferForArgs));
	SERIALISE_ELEMENT(uint32_t, AlignedByteOffsetForArgs, AlignedByteOffsetForArgs_);
	
	if(m_State <= EXECUTING)
	{
		m_pRealContext->DispatchIndirect(UNWRAP(WrappedID3D11Buffer, m_pDevice->GetResourceManager()->GetLiveResource(BufferForArgs)), AlignedByteOffsetForArgs);
	}
		
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DISPATCH_INDIRECT, desc);
		
		ID3D11Buffer *argBuffer = (ID3D11Buffer *)m_pDevice->GetResourceManager()->GetLiveResource(BufferForArgs);

		vector<byte> args = m_pDevice->GetDebugManager()->GetBufferData(argBuffer, AlignedByteOffsetForArgs, 5*sizeof(uint32_t));
		uint32_t *uargs = (uint32_t *)&args[0];

		string name = "DispatchIndirect(<" + ToStr::Get(uargs[0])
						+ ", " + ToStr::Get(uargs[1]) + 
						+ ", " + ToStr::Get(uargs[2]) + ">)";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Dispatch|eDraw_Indirect;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedID3D11DeviceContext::DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	m_pRealContext->DispatchIndirect(UNWRAP(WrappedID3D11Buffer, pBufferForArgs), AlignedByteOffsetForArgs);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISPATCH_INDIRECT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}

	if(pBufferForArgs && m_State >= WRITING_CAPFRAME)
		MarkResourceReferenced(GetIDForResource(pBufferForArgs), eFrameRef_Read);
}

bool WrappedID3D11DeviceContext::Serialise_FinishCommandList(BOOL RestoreDeferredContextState_, ID3D11CommandList **ppCommandList)
{
	SERIALISE_ELEMENT(uint8_t, RestoreDeferredContextState, RestoreDeferredContextState_ == TRUE);
	SERIALISE_ELEMENT(ResourceId, cmdList, GetIDForResource(*ppCommandList));

	if(m_State <= EXECUTING && GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
	{
		ID3D11CommandList *ret = NULL;
		HRESULT hr = m_pRealContext->FinishCommandList(RestoreDeferredContextState, &ret);

		if(!RestoreDeferredContextState)
			m_CurrentPipelineState->Clear();

		VerifyState();

		if(FAILED(hr)) RDCERR("Failed on finishing command list, HRESULT: 0x%08x", hr);

		RDCASSERT(SUCCEEDED(hr) && ret);
		
		ret = new WrappedID3D11CommandList(ret, m_pDevice, this, true);

		if(ret)
		{
			m_pDevice->GetResourceManager()->AddLiveResource(cmdList, ret);
		}
	}

	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(FINISH_CMD_LIST, desc);
		string name = "FinishCommandList() -> " + ToStr::Get(cmdList);

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_CmdList;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);

		m_pDevice->GetImmediateContext()->m_CmdLists[cmdList] = m_ParentDrawcall;
		m_ParentDrawcall.children.clear();
	}

	return true;
}

HRESULT WrappedID3D11DeviceContext::FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList)
{
	DrainAnnotationQueue();

	ID3D11CommandList *real = NULL;
	HRESULT hr = m_pRealContext->FinishCommandList(RestoreDeferredContextState, &real);

	RDCASSERT(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);

	bool cmdListSuccessful = m_SuccessfulCapture;

	if(m_State != WRITING_CAPFRAME && !m_EmptyCommandList)
		cmdListSuccessful = false;

	WrappedID3D11CommandList *wrapped = new WrappedID3D11CommandList(real, m_pDevice, this, cmdListSuccessful);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_pDevice->GetResourceManager()->GetResourceRecord(wrapped->GetResourceID()) == NULL);

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
		record->Length = 0;
		record->ignoreSerialise = true;
	}

	// if we got here and m_SuccessfulCapture is on, we have captured everything in this command list
	if(m_State == WRITING_CAPFRAME && m_SuccessfulCapture)
	{
		RDCDEBUG("Deferred Context %llu Finish()'d successfully! Got successful command list %llu", GetResourceID(), wrapped->GetResourceID());

		RDCASSERT(wrapped->IsCaptured());
		
		ID3D11CommandList *w = wrapped;

		SCOPED_SERIALISE_CONTEXT(FINISH_CMD_LIST);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_FinishCommandList(RestoreDeferredContextState, &w);
		
		m_ContextRecord->AddChunk(scope.Get());

		D3D11ResourceRecord *r = m_pDevice->GetResourceManager()->GetResourceRecord(wrapped->GetResourceID());
		RDCASSERT(r);

		m_ContextRecord->SwapChunks(r);
	}
	else if(m_State == WRITING_CAPFRAME && !m_SuccessfulCapture)
	{
		m_SuccessfulCapture = true;

		RDCDEBUG("Deferred Context %llu wasn't successful, but now we've Finish()'d so it is! Produced unsuccessful command list %llu.",
					GetResourceID(), wrapped->GetResourceID());

		RDCASSERT(!wrapped->IsCaptured());

		// need to clear out anything we had serialised before
		m_ContextRecord->LockChunks();
		while(m_ContextRecord->HasChunks())
		{
			Chunk *chunk = m_ContextRecord->GetLastChunk();
			SAFE_DELETE(chunk);
			m_ContextRecord->PopChunk();
		}
		m_ContextRecord->UnlockChunks();
	}
	else if(m_State >= WRITING)
	{
		// mark that this command list is empty so that if we immediately try and capture
		// we pick up on that.
		m_EmptyCommandList = true;
		
		RDCDEBUG("Deferred Context %llu not capturing at the moment, Produced unsuccessful command list %llu.",
					GetResourceID(), wrapped->GetResourceID());
	}

	if(!RestoreDeferredContextState)
		m_CurrentPipelineState->Clear();
	VerifyState();

	*ppCommandList = wrapped;

	return hr;
}

bool WrappedID3D11DeviceContext::Serialise_Flush()
{
	if(m_State <= EXECUTING)
	{
		m_pRealContext->Flush();
	}

	return true;
}

void WrappedID3D11DeviceContext::Flush()
{
	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(FLUSH);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_Flush();
		
		m_ContextRecord->AddChunk(scope.Get());

		m_CurrentPipelineState->MarkReferenced(this, false);
	}
	else if(m_State == WRITING_IDLE)
	{
		m_CurrentPipelineState->MarkDirty(m_pDevice->GetResourceManager());
	}

	m_pRealContext->Flush();
}

#pragma endregion Execute

#pragma region Copy

bool WrappedID3D11DeviceContext::Serialise_CopySubresourceRegion( ID3D11Resource *pDstResource, UINT DstSubresource,
												UINT DstX, UINT DstY, UINT DstZ,
												ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox)
{
	SERIALISE_ELEMENT(ResourceId, Destination, GetIDForResource(pDstResource));
	SERIALISE_ELEMENT(uint32_t, DestSubresource, DstSubresource);
	SERIALISE_ELEMENT(uint32_t, DestX, DstX);
	SERIALISE_ELEMENT(uint32_t, DestY, DstY);
	SERIALISE_ELEMENT(uint32_t, DestZ, DstZ);
	SERIALISE_ELEMENT(ResourceId, Source, GetIDForResource(pSrcResource));
	SERIALISE_ELEMENT(uint32_t, SourceSubresource, SrcSubresource);
	SERIALISE_ELEMENT(uint8_t, HasSourceBox, pSrcBox != NULL);
	SERIALISE_ELEMENT_OPT(D3D11_BOX, SourceBox, *pSrcBox, HasSourceBox);

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(Destination))
	{
		D3D11_BOX *box = &SourceBox;
		if(!HasSourceBox)
			box = NULL;

		m_pRealContext->CopySubresourceRegion(m_pDevice->GetResourceManager()->UnwrapResource((ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(Destination)),
													DestSubresource, DestX, DestY, DestZ,
													m_pDevice->GetResourceManager()->UnwrapResource((ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(Source)),
													SourceSubresource, box);
	}

	return true;
}

void WrappedID3D11DeviceContext::CopySubresourceRegion( ID3D11Resource *pDstResource, UINT DstSubresource,
												UINT DstX, UINT DstY, UINT DstZ,
												ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(COPY_SUBRESOURCE_REGION);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
		
		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
		RDCASSERT(record);
		D3D11ResourceRecord *srcRecord = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
		RDCASSERT(srcRecord);
		record->AddParent(srcRecord);
		
		m_ContextRecord->AddChunk(scope.Get());
		
		m_MissingTracks.insert(GetIDForResource(pDstResource));
		// assume partial update
		MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Read);
		MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Write);
		MarkResourceReferenced(GetIDForResource(pSrcResource), eFrameRef_Read);
	}
	else if(m_State >= WRITING)
	{
		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
		RDCASSERT(record);

		D3D11ResourceRecord *srcRecord = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
		RDCASSERT(srcRecord);

		if(m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pSrcResource)))
		{
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pDstResource));
		}
		else if(WrappedID3D11Buffer::IsAlloc(pDstResource) && WrappedID3D11Buffer::IsAlloc(pSrcResource))
		{
			// perform copy manually (since we have buffer contents locally)

			RDCASSERT(record->DataInSerialiser);
			RDCASSERT(srcRecord->DataInSerialiser);

			byte *from = srcRecord->GetDataPtr();
			byte *to = record->GetDataPtr();

			to += DstX;

			size_t length = record->Length;

			if(pSrcBox)
			{
				from += pSrcBox->left;
				length = pSrcBox->right - pSrcBox->left;
			}

			if(length > 0)
			{
				memcpy(to, from, length);
			}
		}
		else
		{
			// GPU dirty. Just let initial state handle this.
			
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pDstResource));

			RDCASSERT(
				(WrappedID3D11Texture1D::IsAlloc(pDstResource) && WrappedID3D11Texture1D::IsAlloc(pSrcResource)) ||
				(WrappedID3D11Texture2D::IsAlloc(pDstResource) && WrappedID3D11Texture2D::IsAlloc(pSrcResource)) ||
				(WrappedID3D11Texture3D::IsAlloc(pDstResource) && WrappedID3D11Texture3D::IsAlloc(pSrcResource))
				);
		}
	}

	m_pRealContext->CopySubresourceRegion(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, DstX, DstY, DstZ,
												m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, pSrcBox);
}

bool WrappedID3D11DeviceContext::Serialise_CopyResource(ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource)
{
	SERIALISE_ELEMENT(ResourceId, Destination, GetIDForResource(pDstResource));
	SERIALISE_ELEMENT(ResourceId, Source, GetIDForResource(pSrcResource));

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(Destination))
	{
		m_pRealContext->CopyResource(m_pDevice->GetResourceManager()->UnwrapResource((ID3D11Resource*)m_pDevice->GetResourceManager()->GetLiveResource(Destination)),
											m_pDevice->GetResourceManager()->UnwrapResource((ID3D11Resource*)m_pDevice->GetResourceManager()->GetLiveResource(Source)));
	}

	return true;
}

void WrappedID3D11DeviceContext::CopyResource(ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(COPY_RESOURCE);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CopyResource(pDstResource, pSrcResource);
		
		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
		RDCASSERT(record);
		D3D11ResourceRecord *srcRecord = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
		RDCASSERT(srcRecord);
		record->AddParent(srcRecord);

		m_ContextRecord->AddChunk(scope.Get());
		
		m_MissingTracks.insert(GetIDForResource(pDstResource));
		MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Write);
		MarkResourceReferenced(GetIDForResource(pSrcResource), eFrameRef_Read);
	}
	else if(m_State >= WRITING)
	{
		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
		RDCASSERT(record);

		D3D11ResourceRecord *srcRecord = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
		RDCASSERT(srcRecord);
		
		if(m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pSrcResource)))
		{
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pDstResource));
		}
		else if(WrappedID3D11Buffer::IsAlloc(pDstResource) && WrappedID3D11Buffer::IsAlloc(pSrcResource))
		{
			// perform copy manually (since we have buffer contents locally)

			RDCASSERT(record->DataInSerialiser);
			RDCASSERT(srcRecord->DataInSerialiser);
			
			byte *from = srcRecord->GetDataPtr();
			byte *to = record->GetDataPtr();

			memcpy(to, from, record->Length);
		}
		else if(
			(WrappedID3D11Texture1D::IsAlloc(pDstResource) && WrappedID3D11Texture1D::IsAlloc(pSrcResource)) ||
			(WrappedID3D11Texture2D::IsAlloc(pDstResource) && WrappedID3D11Texture2D::IsAlloc(pSrcResource)) ||
			(WrappedID3D11Texture3D::IsAlloc(pDstResource) && WrappedID3D11Texture3D::IsAlloc(pSrcResource))
		  )
		{
			bool copied = false;

			// can't copy without data allocated
			if(!record->DataInSerialiser || !srcRecord->DataInSerialiser)
			{
				SCOPED_SERIALISE_CONTEXT(COPY_RESOURCE);
				m_pSerialiser->Serialise("context", m_ResourceID);	
				Serialise_CopyResource(pDstResource, pSrcResource);

				record->LockChunks();
				while(true)
				{
					Chunk *end = record->GetLastChunk();

					if(end->GetChunkType() == CLEAR_RTV ||
						end->GetChunkType() == CLEAR_DSV ||
						end->GetChunkType() == CLEAR_UAV_FLOAT ||
						end->GetChunkType() == CLEAR_UAV_INT ||
						end->GetChunkType() == RESOLVE_SUBRESOURCE ||
						end->GetChunkType() == COPY_RESOURCE)
					{
						SAFE_DELETE(end);

						record->PopChunk();

						continue;
					}

					break;
				}
				record->UnlockChunks();

				record->AddChunk(scope.Get());
				record->AddParent(srcRecord);
			}
			else
			{
				RDCASSERT(record->NumSubResources == srcRecord->NumSubResources);

				for(int i=0; i < record->NumSubResources; i++)
				{
					byte *from = srcRecord->SubResources[i]->GetDataPtr();
					byte *to = record->SubResources[i]->GetDataPtr();

					memcpy(to, from, record->SubResources[i]->Length);
				}
			}
		}
		else
		{
			RDCERR("Unexpected resource type");
		}
	}

	m_pRealContext->CopyResource(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource));
}

bool WrappedID3D11DeviceContext::Serialise_UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox,
																const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	return Serialise_UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, 0);
}

void WrappedID3D11DeviceContext::UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox,
												const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UPDATE_SUBRESOURCE);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_UpdateSubresource(pDstResource, DstSubresource, pDstBox,
									pSrcData, SrcRowPitch, SrcDepthPitch);
		
		m_MissingTracks.insert(GetIDForResource(pDstResource));

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State >= WRITING)
	{
		ResourceId idx = GetIDForResource(pDstResource);
		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(idx);
		RDCASSERT(record);

		// buffers MUST update the whole resource, and don't have any subresources,
		// so this effectively becomes just a map/unmap pair.
		if(WrappedID3D11Buffer::IsAlloc(pDstResource))
		{
			RDCASSERT(record->NumSubResources == 0);

			size_t offs = 0;
			size_t length = record->Length;
			if(pDstBox)
			{
				offs += pDstBox->left;
				length = RDCMIN((uint32_t)length, pDstBox->right - pDstBox->left);
			}

			RDCASSERT(record->DataInSerialiser);
			
			void *ptr = record->GetDataPtr()+offs;

			memcpy(ptr, pSrcData, length);
		}
		else if(WrappedID3D11Texture1D::IsAlloc(pDstResource) ||
				WrappedID3D11Texture2D::IsAlloc(pDstResource) ||
				WrappedID3D11Texture3D::IsAlloc(pDstResource))
		{
			RDCASSERT(record->Length == 1 && record->NumSubResources > 0);

			if(DstSubresource >= (UINT)record->NumSubResources)
			{
				RDCERR("DstSubresource %u >= %u (num subresources)", DstSubresource, record->NumSubResources);
				return;
			}
			
			// this record isn't in the log already, write out a chunk that we can update after.
			if(!record->SubResources[DstSubresource]->DataInSerialiser)
			{
				SCOPED_SERIALISE_CONTEXT(UPDATE_SUBRESOURCE);
				m_pSerialiser->Serialise("context", m_ResourceID);	
				
				Serialise_UpdateSubresource(pDstResource, DstSubresource, pDstBox,
											pSrcData, SrcRowPitch, SrcDepthPitch);

				Chunk *chunk = scope.Get();

				record->AddChunk(chunk);
				record->SubResources[DstSubresource]->SetDataPtr(chunk->GetData());
				
				record->SubResources[DstSubresource]->DataInSerialiser = true;
			}

			{
				RDCASSERT(record->SubResources[DstSubresource]->DataInSerialiser);

				void *ptr = record->SubResources[DstSubresource]->GetDataPtr();
				
				// if the box is empty, we don't have to do anything! hooray!
				if(pDstBox &&
					(pDstBox->back == pDstBox->front ||
					pDstBox->left == pDstBox->right ||
					pDstBox->top == pDstBox->bottom)
					)
				{
					// empty, do nothing.
				}
				else
				{
					WrappedID3D11Texture1D *tex1 = WrappedID3D11Texture1D::IsAlloc(pDstResource) ? (WrappedID3D11Texture1D *)pDstResource : NULL;
					WrappedID3D11Texture2D *tex2 = WrappedID3D11Texture2D::IsAlloc(pDstResource) ? (WrappedID3D11Texture2D *)pDstResource : NULL;
					WrappedID3D11Texture3D *tex3 = WrappedID3D11Texture3D::IsAlloc(pDstResource) ? (WrappedID3D11Texture3D *)pDstResource : NULL;

					RDCASSERT(tex1 || tex2 || tex3);

					DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
					UINT subWidth = 1;
					UINT subHeight = 1;
					UINT subDepth = 1;

					UINT mipLevel = GetMipForSubresource(pDstResource, DstSubresource);
					
					if(tex1)
					{
						D3D11_TEXTURE1D_DESC desc = {0};
						tex1->GetDesc(&desc);
						fmt = desc.Format;
						subWidth = RDCMAX(1U, desc.Width>>mipLevel);
					}
					else if(tex2)
					{
						D3D11_TEXTURE2D_DESC desc = {0};
						tex2->GetDesc(&desc);
						fmt = desc.Format;
						subWidth = RDCMAX(1U, desc.Width>>mipLevel);
						subHeight = RDCMAX(1U, desc.Height>>mipLevel);
					}
					else if(tex3)
					{
						D3D11_TEXTURE3D_DESC desc = {0};
						tex3->GetDesc(&desc);
						fmt = desc.Format;
						subWidth = RDCMAX(1U, desc.Width>>mipLevel);
						subHeight = RDCMAX(1U, desc.Height>>mipLevel);
						subDepth = RDCMAX(1U, desc.Depth>>mipLevel);
					}
					
					UINT boxWidth = pDstBox ? pDstBox->right - pDstBox->left : subWidth;
					UINT boxHeight = pDstBox ? pDstBox->bottom - pDstBox->top : subHeight;
					UINT boxDepth = pDstBox ? pDstBox->back - pDstBox->front : subDepth;

					UINT boxTop = pDstBox ? pDstBox->top : 0;
					
					UINT DstRowPitch = GetByteSize(subWidth, 1, 1, fmt, 0);
					UINT DstBoxRowPitch = GetByteSize(boxWidth, 1, 1, fmt, 0);
					UINT DstSlicePitch = GetByteSize(subWidth, subHeight, 1, fmt, 0);

					// for block formats, rows are in blocks (so height is squished essentially)
					if(IsBlockFormat(fmt))
					{
						subWidth = AlignUp4(subWidth);
						subHeight = AlignUp4(RDCMAX(1U, subHeight/4));
						boxHeight = RDCMAX(1U, boxHeight/4);
						boxTop = RDCMAX(0U, boxTop/4);
					}

					RDCASSERT(boxWidth <= subWidth &&
							 boxHeight <= subHeight &&
							 boxDepth <= subDepth);

					bool totalUpdate = false;
					
					// if there is no box, it's a totalUpdate (boxwidth/height are equal by inspection from the initialisation above)
					// if the box describes the whole subresource, it's a totalUpdate
					if(boxWidth == subWidth && boxHeight == subHeight && boxDepth == subDepth)
						totalUpdate = true;
					
					// fast path for a total update from a source of the same size
					if(totalUpdate &&
						(
							(tex1 && (UINT)record->SubResources[DstSubresource]->Length == SrcRowPitch) || 
							(tex2 && (UINT)record->SubResources[DstSubresource]->Length == SrcRowPitch*subHeight) || 
							(tex3 && (UINT)record->SubResources[DstSubresource]->Length == SrcDepthPitch*subDepth)
					    )
					  )
					{
						memcpy(ptr, pSrcData, record->SubResources[DstSubresource]->Length);
					}
					else
					{
						// need to fall back to copying row by row from the source
						byte *dstBuf = (byte *)ptr;
						byte *src = (byte *)pSrcData;
						
						// if we have a box, skip to the front of it
						if(pDstBox)
							dstBuf += DstSlicePitch*pDstBox->front;

						for(UINT slice=0; slice < boxDepth; slice++)
						{
							byte *slicedst = dstBuf;
							byte *slicesrc = src;

							// if we have a box, skip to the top of it
							if(pDstBox)
								slicedst += DstRowPitch*boxTop;

							for(UINT row=0; row < boxHeight; row++)
							{
								byte *rowdst = slicedst;

								// if we have a box, skip to the left of it
								if(pDstBox && pDstBox->left > 0)
									rowdst += GetByteSize(pDstBox->left, 1, 1, fmt, 0);

								memcpy(rowdst, slicesrc, DstBoxRowPitch);

								slicedst += DstRowPitch;
								slicesrc += SrcRowPitch;
							}

							dstBuf += DstSlicePitch;
							src += SrcDepthPitch;
						}
					}
				}
			}
		}
	}

	m_pRealContext->UpdateSubresource(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource, pDstBox,
											pSrcData, SrcRowPitch, SrcDepthPitch);
}

bool WrappedID3D11DeviceContext::Serialise_CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView)
{
	SERIALISE_ELEMENT(ResourceId, DestBuffer, GetIDForResource(pDstBuffer));
	SERIALISE_ELEMENT(uint32_t, DestAlignedByteOffset, DstAlignedByteOffset);
	SERIALISE_ELEMENT(ResourceId, SourceView, GetIDForResource(pSrcView));

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(DestBuffer))
	{
		m_pRealContext->CopyStructureCount(UNWRAP(WrappedID3D11Buffer, m_pDevice->GetResourceManager()->GetLiveResource(DestBuffer)),
											DestAlignedByteOffset,
											UNWRAP(WrappedID3D11UnorderedAccessView, m_pDevice->GetResourceManager()->GetLiveResource(SourceView)));
	}

	return true;
}

void WrappedID3D11DeviceContext::CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(COPY_STRUCTURE_COUNT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
		
		m_ContextRecord->AddChunk(scope.Get());
		
		m_MissingTracks.insert(GetIDForResource(pDstBuffer));
		MarkResourceReferenced(GetIDForResource(pDstBuffer), eFrameRef_Read);
		MarkResourceReferenced(GetIDForResource(pDstBuffer), eFrameRef_Write);

		ID3D11Resource *res = NULL;
		pSrcView->GetResource(&res);
		MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
		SAFE_RELEASE(res);
	}
	else if(m_State >= WRITING)
	{
		// needs to go into device serialiser

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstBuffer));
		RDCASSERT(record);

		D3D11ResourceRecord *srcRecord = ((WrappedID3D11UnorderedAccessView *)pSrcView)->GetResourceRecord();
		RDCASSERT(srcRecord);

		record->AddParent(srcRecord);

		ID3D11Resource *res = NULL;
		pSrcView->GetResource(&res);

		m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pDstBuffer));

		SAFE_RELEASE(res);
	}

	m_pRealContext->CopyStructureCount(UNWRAP(WrappedID3D11Buffer, pDstBuffer), DstAlignedByteOffset, UNWRAP(WrappedID3D11UnorderedAccessView, pSrcView));
}

bool WrappedID3D11DeviceContext::Serialise_ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
																ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format_)
{
	SERIALISE_ELEMENT(ResourceId, DestResource, GetIDForResource(pDstResource));
	SERIALISE_ELEMENT(uint32_t, DestSubresource, DstSubresource);
	SERIALISE_ELEMENT(ResourceId, SourceResource, GetIDForResource(pSrcResource));
	SERIALISE_ELEMENT(uint32_t, SourceSubresource, SrcSubresource);
	SERIALISE_ELEMENT(DXGI_FORMAT, Format, Format_);

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(DestResource))
	{
		m_pRealContext->ResolveSubresource(m_pDevice->GetResourceManager()->UnwrapResource((ID3D11Resource*)m_pDevice->GetResourceManager()->GetLiveResource(DestResource)),
												DestSubresource,
												m_pDevice->GetResourceManager()->UnwrapResource((ID3D11Resource*)m_pDevice->GetResourceManager()->GetLiveResource(SourceResource)),
												SourceSubresource, Format);
	}

	return true;
}

void WrappedID3D11DeviceContext::ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
												ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(RESOLVE_SUBRESOURCE);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
		
		m_ContextRecord->AddChunk(scope.Get());
		
		m_MissingTracks.insert(GetIDForResource(pDstResource));
		MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Read);
		MarkResourceReferenced(GetIDForResource(pDstResource), eFrameRef_Write);
		MarkResourceReferenced(GetIDForResource(pSrcResource), eFrameRef_Read);
	}
	else if(m_State >= WRITING)
	{
		// needs to go into device serialiser

		RDCASSERT(WrappedID3D11Texture2D::IsAlloc(pDstResource) && WrappedID3D11Texture2D::IsAlloc(pSrcResource));

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pDstResource));
		RDCASSERT(record);

		D3D11ResourceRecord *srcRecord = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pSrcResource));
		RDCASSERT(srcRecord);

		record->AddParent(srcRecord);

		if(m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pSrcResource)))
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pDstResource));

		SCOPED_SERIALISE_CONTEXT(RESOLVE_SUBRESOURCE);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);

		// resolve subresource only really 'clears' if it's the only subresource.
		// This is usually the case for render target textures though.
		if(record->NumSubResources == 1)
		{
			record->LockChunks();
			while(true)
			{
				Chunk *end = record->GetLastChunk();

				if(end->GetChunkType() == CLEAR_RTV ||
					end->GetChunkType() == CLEAR_DSV ||
					end->GetChunkType() == CLEAR_UAV_FLOAT ||
					end->GetChunkType() == CLEAR_UAV_INT ||
					end->GetChunkType() == RESOLVE_SUBRESOURCE ||
					end->GetChunkType() == COPY_RESOURCE)
				{
					SAFE_DELETE(end);

					record->PopChunk();

					continue;
				}

				break;
			}
			record->UnlockChunks();
		}

		record->AddChunk(scope.Get());
	}

	m_pRealContext->ResolveSubresource(m_pDevice->GetResourceManager()->UnwrapResource(pDstResource), DstSubresource,
											m_pDevice->GetResourceManager()->UnwrapResource(pSrcResource), SrcSubresource, Format);
}

bool WrappedID3D11DeviceContext::Serialise_GenerateMips(ID3D11ShaderResourceView *pShaderResourceView)
{
	SERIALISE_ELEMENT(ResourceId, ShaderResourceView, GetIDForResource(pShaderResourceView));

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(ShaderResourceView))
	{
		m_pRealContext->GenerateMips(UNWRAP(WrappedID3D11ShaderResourceView, m_pDevice->GetResourceManager()->GetLiveResource(ShaderResourceView)));
	}

	return true;
}

void WrappedID3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView *pShaderResourceView)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(GENERATE_MIPS);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_GenerateMips(pShaderResourceView);
		
		m_ContextRecord->AddChunk(scope.Get());

		ID3D11Resource *res = NULL;
		pShaderResourceView->GetResource(&res);

		m_MissingTracks.insert(GetIDForResource(res));
		m_MissingTracks.insert(GetIDForResource(pShaderResourceView));
		
		MarkResourceReferenced(GetIDForResource(res), eFrameRef_Read);
		MarkResourceReferenced(GetIDForResource(res), eFrameRef_Write);
		SAFE_RELEASE(res);
	}
	else if(m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pShaderResourceView->GetResource(&res);
		ResourceId id = GetIDForResource(res);
		m_pDevice->GetResourceManager()->MarkDirtyResource(id);
		SAFE_RELEASE(res);
	}

	m_pRealContext->GenerateMips(UNWRAP(WrappedID3D11ShaderResourceView, pShaderResourceView));
}

#pragma endregion Copy

#pragma region Clear

bool WrappedID3D11DeviceContext::Serialise_ClearState()
{
	if(m_State <= EXECUTING)
	{
		m_CurrentPipelineState->Clear();
		m_pRealContext->ClearState();
		VerifyState();
	}

	return true;
}

void WrappedID3D11DeviceContext::ClearState()
{
	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_STATE);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearState();

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_CurrentPipelineState->Clear();
	m_pRealContext->ClearState();
	VerifyState();
}

bool WrappedID3D11DeviceContext::Serialise_ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
	SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pRenderTargetView));
	
	float Color[4] = {0};

	if(m_State >= WRITING)
		memcpy(Color, ColorRGBA, sizeof(float)*4);

	m_pSerialiser->Serialise<4>("ColorRGBA", Color);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->ClearRenderTargetView(UNWRAP(WrappedID3D11RenderTargetView, m_pDevice->GetResourceManager()->GetLiveResource(View)), Color);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEAR_RTV, desc);
		string name = "ClearRenderTargetView(" +
							ToStr::Get(Color[0]) + ", " +
							ToStr::Get(Color[1]) + ", " +
							ToStr::Get(Color[2]) + ", " +
							ToStr::Get(Color[3]) +
							")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);

		m_ResourceUses[((WrappedID3D11RenderTargetView *)m_pDevice->GetResourceManager()->GetLiveResource(View))->GetResourceResID()]
			.push_back(EventUsage(m_CurEventID, eUsage_Clear));
	}

	return true;
}

void WrappedID3D11DeviceContext::ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
	DrainAnnotationQueue();

	if(pRenderTargetView == NULL) return;

	m_EmptyCommandList = false;

	m_pRealContext->ClearRenderTargetView(UNWRAP(WrappedID3D11RenderTargetView, pRenderTargetView), ColorRGBA);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_RTV);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearRenderTargetView(pRenderTargetView, ColorRGBA);
		
		ID3D11Resource *res = NULL;
		pRenderTargetView->GetResource(&res);

		m_MissingTracks.insert(GetIDForResource(res));
		m_MissingTracks.insert(GetIDForResource(pRenderTargetView));

		SAFE_RELEASE(res);

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_RTV);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearRenderTargetView(pRenderTargetView, ColorRGBA);
		
		ID3D11Resource *viewRes = NULL;
		pRenderTargetView->GetResource(&viewRes);
		ResourceId id = GetIDForResource(viewRes);
		SAFE_RELEASE(viewRes);

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(id);
		RDCASSERT(record);

		record->LockChunks();
		while(true)
		{
			Chunk *end = record->GetLastChunk();

			if(end->GetChunkType() == CLEAR_RTV ||
				end->GetChunkType() == CLEAR_DSV ||
				end->GetChunkType() == CLEAR_UAV_FLOAT ||
				end->GetChunkType() == CLEAR_UAV_INT ||
				end->GetChunkType() == RESOLVE_SUBRESOURCE ||
				end->GetChunkType() == COPY_RESOURCE)
			{
				SAFE_DELETE(end);

				record->PopChunk();

				continue;
			}

			break;
		}
		record->UnlockChunks();

		record->AddChunk(scope.Get());
	}

	if(pRenderTargetView && m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pRenderTargetView->GetResource(&res);

		if(m_State == WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Write);
		
		if(m_State == WRITING_IDLE)
			m_pDevice->GetResourceManager()->MarkCleanResource(GetIDForResource(res));
		SAFE_RELEASE(res);
	}
}

bool WrappedID3D11DeviceContext::Serialise_ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values_[4])
{
	SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pUnorderedAccessView));
	
	UINT Values[4] = {0};

	if(m_State >= WRITING)
		memcpy(Values, Values_, sizeof(UINT)*4);

	m_pSerialiser->Serialise<4>("Values", Values);

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(View))
	{
		m_pRealContext->ClearUnorderedAccessViewUint(UNWRAP(WrappedID3D11UnorderedAccessView, m_pDevice->GetResourceManager()->GetLiveResource(View)), Values);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	if(m_State == READING)
	{
		AddEvent(CLEAR_UAV_INT, desc);
		string name = "ClearUnorderedAccessViewUint(" +
							ToStr::Get(Values[0]) + ", " +
							ToStr::Get(Values[1]) + ", " +
							ToStr::Get(Values[2]) + ", " +
							ToStr::Get(Values[3]) + ", " +
							")";

		FetchDrawcall draw;
		draw.name = widen(name);

		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);

		m_ResourceUses[((WrappedID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetLiveResource(View))->GetResourceResID()]
			.push_back(EventUsage(m_CurEventID, eUsage_Clear));
	}

	return true;
}

void WrappedID3D11DeviceContext::ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4])
{
	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_UAV_INT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
		
		ID3D11Resource *res = NULL;
		pUnorderedAccessView->GetResource(&res);

		m_MissingTracks.insert(GetIDForResource(res));
		m_MissingTracks.insert(GetIDForResource(pUnorderedAccessView));

		SAFE_RELEASE(res);

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_UAV_INT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);

		ID3D11Resource *viewRes = NULL;
		pUnorderedAccessView->GetResource(&viewRes);
		ResourceId id = GetIDForResource(viewRes);
		SAFE_RELEASE(viewRes);

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(id);
		RDCASSERT(record);

		record->LockChunks();
		while(true)
		{
			Chunk *end = record->GetLastChunk();

			if(end->GetChunkType() == CLEAR_RTV ||
				end->GetChunkType() == CLEAR_DSV ||
				end->GetChunkType() == CLEAR_UAV_FLOAT ||
				end->GetChunkType() == CLEAR_UAV_INT ||
				end->GetChunkType() == RESOLVE_SUBRESOURCE ||
				end->GetChunkType() == COPY_RESOURCE)
			{
				SAFE_DELETE(end);

				record->PopChunk();

				continue;
			}

			break;
		}
		record->UnlockChunks();

		record->AddChunk(scope.Get());
	}
	
	if(pUnorderedAccessView && m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pUnorderedAccessView->GetResource(&res);
		
		if(m_State == WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Write);
		
		if(m_State == WRITING_IDLE)
			m_pDevice->GetResourceManager()->MarkCleanResource(GetIDForResource(res));
		SAFE_RELEASE(res);
	}

	m_pRealContext->ClearUnorderedAccessViewUint(UNWRAP(WrappedID3D11UnorderedAccessView, pUnorderedAccessView), Values);
}

bool WrappedID3D11DeviceContext::Serialise_ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values_[4])
{
	SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pUnorderedAccessView));
	
	FLOAT Values[4] = {0};

	if(m_State >= WRITING)
		memcpy(Values, Values_, sizeof(FLOAT)*4);

	m_pSerialiser->Serialise<4>("Values", Values);

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(View))
	{
		m_pRealContext->ClearUnorderedAccessViewFloat(UNWRAP(WrappedID3D11UnorderedAccessView, m_pDevice->GetResourceManager()->GetLiveResource(View)), Values);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	if(m_State == READING)
	{
		AddEvent(CLEAR_UAV_FLOAT, desc);
		string name = "ClearUnorderedAccessViewFloat(" +
							ToStr::Get(Values[0]) + ", " +
							ToStr::Get(Values[1]) + ", " +
							ToStr::Get(Values[2]) + ", " +
							ToStr::Get(Values[3]) + ", " +
							")";

		FetchDrawcall draw;
		draw.name = (widen(name));
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);

		m_ResourceUses[((WrappedID3D11UnorderedAccessView *)m_pDevice->GetResourceManager()->GetLiveResource(View))->GetResourceResID()]
			.push_back(EventUsage(m_CurEventID, eUsage_Clear));
	}

	return true;
}

void WrappedID3D11DeviceContext::ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4])
{
	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_UAV_FLOAT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
		
		ID3D11Resource *res = NULL;
		pUnorderedAccessView->GetResource(&res);

		m_MissingTracks.insert(GetIDForResource(res));
		m_MissingTracks.insert(GetIDForResource(pUnorderedAccessView));

		SAFE_RELEASE(res);

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_UAV_FLOAT);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);

		ID3D11Resource *viewRes = NULL;
		pUnorderedAccessView->GetResource(&viewRes);
		ResourceId id = GetIDForResource(viewRes);
		SAFE_RELEASE(viewRes);

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(id);
		RDCASSERT(record);

		record->LockChunks();
		while(true)
		{
			Chunk *end = record->GetLastChunk();

			if(end->GetChunkType() == CLEAR_RTV ||
				end->GetChunkType() == CLEAR_DSV ||
				end->GetChunkType() == CLEAR_UAV_FLOAT ||
				end->GetChunkType() == CLEAR_UAV_INT ||
				end->GetChunkType() == RESOLVE_SUBRESOURCE ||
				end->GetChunkType() == COPY_RESOURCE)
			{
				SAFE_DELETE(end);

				record->PopChunk();

				continue;
			}

			break;
		}
		record->UnlockChunks();

		record->AddChunk(scope.Get());
	}
	
	if(pUnorderedAccessView && m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pUnorderedAccessView->GetResource(&res);
		
		if(m_State == WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Write);
		
		if(m_State == WRITING_IDLE)
			m_pDevice->GetResourceManager()->MarkCleanResource(GetIDForResource(res));
		SAFE_RELEASE(res);
	}

	m_pRealContext->ClearUnorderedAccessViewFloat(UNWRAP(WrappedID3D11UnorderedAccessView, pUnorderedAccessView), Values);
}

bool WrappedID3D11DeviceContext::Serialise_ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags_, FLOAT Depth_, UINT8 Stencil_)
{
	SERIALISE_ELEMENT(ResourceId, View, GetIDForResource(pDepthStencilView));
	SERIALISE_ELEMENT(uint32_t, ClearFlags, ClearFlags_);
	SERIALISE_ELEMENT(float, Depth, Depth_);
	SERIALISE_ELEMENT(uint8_t, Stencil, Stencil_);

	if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(View))
	{
		m_pRealContext->ClearDepthStencilView(UNWRAP(WrappedID3D11DepthStencilView, m_pDevice->GetResourceManager()->GetLiveResource(View)), ClearFlags, Depth, Stencil);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	vector<DebugMessage> debugMessages = Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEAR_DSV, desc);
		string name = "ClearDepthStencilView(" +
							ToStr::Get(Depth) + ", " +
							ToStr::Get(Stencil) +
							")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		draw.debugMessages = debugMessages;

		AddDrawcall(draw, true);

		m_ResourceUses[((WrappedID3D11DepthStencilView *)m_pDevice->GetResourceManager()->GetLiveResource(View))->GetResourceResID()]
			.push_back(EventUsage(m_CurEventID, eUsage_Clear));
	}

	return true;
}

void WrappedID3D11DeviceContext::ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	DrainAnnotationQueue();

	if(pDepthStencilView == NULL) return;

	m_EmptyCommandList = false;

	m_pRealContext->ClearDepthStencilView(UNWRAP(WrappedID3D11DepthStencilView, pDepthStencilView), ClearFlags, Depth, Stencil);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_DSV);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
		
		ID3D11Resource *res = NULL;
		pDepthStencilView->GetResource(&res);

		m_MissingTracks.insert(GetIDForResource(res));
		m_MissingTracks.insert(GetIDForResource(pDepthStencilView));

		SAFE_RELEASE(res);

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_DSV);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);

		ID3D11Resource *viewRes = NULL;
		pDepthStencilView->GetResource(&viewRes);
		ResourceId id = GetIDForResource(viewRes);
		SAFE_RELEASE(viewRes);

		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(id);
		RDCASSERT(record);

		record->LockChunks();
		while(true)
		{
			Chunk *end = record->GetLastChunk();

			if(end->GetChunkType() == CLEAR_RTV ||
				end->GetChunkType() == CLEAR_DSV ||
				end->GetChunkType() == CLEAR_UAV_FLOAT ||
				end->GetChunkType() == CLEAR_UAV_INT ||
				end->GetChunkType() == RESOLVE_SUBRESOURCE ||
				end->GetChunkType() == COPY_RESOURCE)
			{
				SAFE_DELETE(end);

				record->PopChunk();

				continue;
			}

			break;
		}
		record->UnlockChunks();

		record->AddChunk(scope.Get());
	}
	
	if(pDepthStencilView && m_State >= WRITING)
	{
		ID3D11Resource *res = NULL;
		pDepthStencilView->GetResource(&res);
		
		if(m_State == WRITING_CAPFRAME)
			MarkResourceReferenced(GetIDForResource(res), eFrameRef_Write);

		if(m_State == WRITING_IDLE)
			m_pDevice->GetResourceManager()->MarkCleanResource(GetIDForResource(res));
		SAFE_RELEASE(res);
	}
}

#pragma endregion Clear

#pragma region Misc

bool WrappedID3D11DeviceContext::Serialise_Begin(ID3D11Asynchronous *pAsync)
{
	SERIALISE_ELEMENT(ResourceId, Async, GetIDForResource(pAsync));

	SERIALISE_ELEMENT(bool, IsQuery, WrappedID3D11Query::IsAlloc(pAsync));

	if(IsQuery)
	{
		D3D11_QUERY qt = D3D11_QUERY_EVENT;

		if(m_State >= WRITING)
		{
			D3D11_QUERY_DESC desc;
			ID3D11Query *q = (ID3D11Query *)pAsync;
			q->GetDesc(&desc);

			qt = desc.Query;
		}

		SERIALISE_ELEMENT(D3D11_QUERY, QueryType, qt);
	}

	if(m_State <= EXECUTING)
	{
		//m_pImmediateContext->Begin((ID3D11Asynchronous *)m_pDevice->GetResourceManager()->GetLiveResource(Async));
	}

	return true;
}

void WrappedID3D11DeviceContext::Begin(ID3D11Asynchronous *pAsync)
{
	ID3D11Asynchronous *unwrapped = NULL;

	if(WrappedID3D11Query::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Query, pAsync);
	else if(WrappedID3D11Predicate::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);
	else if(WrappedID3D11Counter::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Counter, pAsync);
	else
		RDCERR("Unexpected ID3D11Asynchronous");
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BEGIN);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_Begin(pAsync);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_pRealContext->Begin(unwrapped);
}

bool WrappedID3D11DeviceContext::Serialise_End(ID3D11Asynchronous *pAsync)
{
	SERIALISE_ELEMENT(ResourceId, Async, GetIDForResource(pAsync));

	SERIALISE_ELEMENT(bool, IsQuery, WrappedID3D11Query::IsAlloc(pAsync));

	if(IsQuery)
	{
		D3D11_QUERY qt = D3D11_QUERY_EVENT;

		if(m_State >= WRITING)
		{
			D3D11_QUERY_DESC desc;
			ID3D11Query *q = (ID3D11Query *)pAsync;
			q->GetDesc(&desc);

			qt = desc.Query;
		}

		SERIALISE_ELEMENT(D3D11_QUERY, QueryType, qt);
	}

	if(m_State <= EXECUTING)
	{
		//m_pImmediateContext->End((ID3D11Asynchronous *)m_pDevice->GetResourceManager()->GetLiveResource(Async));
	}

	return true;
}

void WrappedID3D11DeviceContext::End(ID3D11Asynchronous *pAsync)
{
	ID3D11Asynchronous *unwrapped = NULL;

	if(WrappedID3D11Query::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Query, pAsync);
	else if(WrappedID3D11Predicate::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);
	else if(WrappedID3D11Counter::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Counter, pAsync);
	else
		RDCERR("Unexpected ID3D11Asynchronous");
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BEGIN);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_End(pAsync);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_pRealContext->End(unwrapped);
}

HRESULT WrappedID3D11DeviceContext::GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags)
{
	ID3D11Asynchronous *unwrapped = NULL;

	if(WrappedID3D11Query::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Query, pAsync);
	else if(WrappedID3D11Predicate::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Predicate, pAsync);
	else if(WrappedID3D11Counter::IsAlloc(pAsync))
		unwrapped = UNWRAP(WrappedID3D11Counter, pAsync);
	else
		RDCERR("Unexpected ID3D11Asynchronous");

	return m_pRealContext->GetData(unwrapped, pData, DataSize, GetDataFlags);
}

bool WrappedID3D11DeviceContext::Serialise_SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue_)
{
	SERIALISE_ELEMENT(ResourceId, Predicate, GetIDForResource(pPredicate));
	SERIALISE_ELEMENT(uint8_t, PredicateValue, PredicateValue_ == TRUE);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->SetPredication(UNWRAP(WrappedID3D11Predicate, m_pDevice->GetResourceManager()->GetLiveResource(Predicate)), PredicateValue);
	}

	return true;
}

void WrappedID3D11DeviceContext::SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue)
{
	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_PREDICATION);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_SetPredication(pPredicate, PredicateValue);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_pRealContext->SetPredication(UNWRAP(WrappedID3D11Predicate, pPredicate), PredicateValue);
}

FLOAT WrappedID3D11DeviceContext::GetResourceMinLOD(ID3D11Resource *pResource)
{
	return m_pRealContext->GetResourceMinLOD(m_pDevice->GetResourceManager()->UnwrapResource(pResource));
}

bool WrappedID3D11DeviceContext::Serialise_SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD_)
{
	SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
	SERIALISE_ELEMENT(float, MinLOD, MinLOD_);

	if(m_State <= EXECUTING)
	{
		m_pRealContext->SetResourceMinLOD((ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(Resource), MinLOD);
	}

	return true;
}

void WrappedID3D11DeviceContext::SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD)
{
	m_EmptyCommandList = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SET_RESOURCE_MINLOD);
		m_pSerialiser->Serialise("context", m_ResourceID);	
		Serialise_SetResourceMinLOD(pResource, MinLOD);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_pRealContext->SetResourceMinLOD(m_pDevice->GetResourceManager()->UnwrapResource(pResource), MinLOD);
}

void WrappedID3D11DeviceContext::GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue)
{
	ID3D11Predicate *real = NULL;
	m_pRealContext->GetPredication(&real, pPredicateValue);
	SAFE_RELEASE_NOCLEAR(real);

	if(ppPredicate)
	{
		if(real)
			*ppPredicate = UNWRAP(WrappedID3D11Predicate, real);
		else
			*ppPredicate = NULL;
	}
}

D3D11_DEVICE_CONTEXT_TYPE WrappedID3D11DeviceContext::GetType()
{
	return m_pRealContext->GetType();
}

UINT WrappedID3D11DeviceContext::GetContextFlags()
{
	return m_pRealContext->GetContextFlags();
}

#pragma endregion Misc

#pragma region Map

void MapIntercept::SetAppMemory(void *appMemory)
{
	app.pData = appMemory;
}

void MapIntercept::SetD3D(D3D11_SUBRESOURCE_DATA d3dData)
{
	D3D11_MAPPED_SUBRESOURCE d3dMap;
	d3dMap.pData = (void *)d3dData.pSysMem;
	d3dMap.RowPitch = d3dData.SysMemPitch;
	d3dMap.DepthPitch = d3dData.SysMemSlicePitch;

	d3d = d3dMap;

	RDCASSERT(d3d.pData);
}

void MapIntercept::SetD3D(D3D11_MAPPED_SUBRESOURCE d3dMap)
{
	d3d = d3dMap;

	RDCASSERT(d3d.pData);
}
	
void MapIntercept::InitWrappedResource(ID3D11Resource *res, UINT sub, void *appMemory)
{
	if(WrappedID3D11Buffer::IsAlloc(res))
		Init((ID3D11Buffer*)res, appMemory);
	else if(WrappedID3D11Texture1D::IsAlloc(res))
		Init((ID3D11Texture1D*)res, sub, appMemory);
	else if(WrappedID3D11Texture2D::IsAlloc(res))
		Init((ID3D11Texture2D*)res, sub, appMemory);
	else if(WrappedID3D11Texture3D::IsAlloc(res))
		Init((ID3D11Texture3D*)res, sub, appMemory);
	else
		RDCERR("Unexpected resource type");
}

void MapIntercept::Init(ID3D11Buffer *buf, void *appMemory)
{
	app.pData = appMemory;

	if(buf == NULL)
		return;

	D3D11_BUFFER_DESC desc;
	buf->GetDesc(&desc);
	
	app.RowPitch = app.DepthPitch = desc.ByteWidth;

	if(d3d.RowPitch == 0) 
		d3d.RowPitch = desc.ByteWidth;
	if(d3d.DepthPitch == 0) 
		d3d.DepthPitch = desc.ByteWidth;
}

void MapIntercept::Init(ID3D11Texture1D *tex, UINT sub, void *appMemory)
{
	app.pData = appMemory;

	if(tex == NULL)
		return;

	D3D11_TEXTURE1D_DESC desc;
	tex->GetDesc(&desc);

	int width = desc.Width;
	int height = 1;
	int depth = 1;
	DXGI_FORMAT fmt = desc.Format;

	int mip = GetMipForSubresource(tex, sub);

	// a row in block formats is a row of 4x4 blocks.
	if(IsBlockFormat(fmt))
		numRows /= 4;

	numRows = RDCMAX(1, numRows>>mip);
	numSlices = RDCMAX(1, numSlices>>mip);

	app.RowPitch = GetByteSize(width, 1, 1, fmt, mip);
	app.DepthPitch = GetByteSize(width, height, 1, fmt, mip);
	
	if(d3d.DepthPitch == 0) 
		d3d.DepthPitch = app.RowPitch;
	if(d3d.DepthPitch == 0) 
		d3d.DepthPitch = app.DepthPitch;
}

void MapIntercept::Init(ID3D11Texture2D *tex, UINT sub, void *appMemory)
{
	app.pData = appMemory;

	if(tex == NULL)
		return;

	D3D11_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);

	int width = desc.Width;
	int height = numRows = desc.Height;
	int depth = 1;
	DXGI_FORMAT fmt = desc.Format;

	int mip = GetMipForSubresource(tex, sub);

	// a row in block formats is a row of 4x4 blocks.
	if(IsBlockFormat(fmt))
		numRows /= 4;

	numRows = RDCMAX(1, numRows>>mip);
	numSlices = RDCMAX(1, numSlices>>mip);

	app.RowPitch = GetByteSize(width, 1, 1, fmt, mip);
	app.DepthPitch = GetByteSize(width, height, 1, fmt, mip);
	
	if(d3d.DepthPitch == 0) 
		d3d.DepthPitch = app.DepthPitch;
}

void MapIntercept::Init(ID3D11Texture3D *tex, UINT sub, void *appMemory)
{
	app.pData = appMemory;

	if(tex == NULL)
		return;

	D3D11_TEXTURE3D_DESC desc;
	tex->GetDesc(&desc);

	int width = desc.Width;
	int height = numRows = desc.Height;
	int depth = numSlices = desc.Depth;
	DXGI_FORMAT fmt = desc.Format;

	int mip = GetMipForSubresource(tex, sub);

	// a row in block formats is a row of 4x4 blocks.
	if(IsBlockFormat(fmt))
		numRows /= 4;

	numRows = RDCMAX(1, numRows>>mip);
	numSlices = RDCMAX(1, numSlices>>mip);

	app.RowPitch = GetByteSize(width, 1, 1, fmt, mip);
	app.DepthPitch = GetByteSize(width, height, 1, fmt, mip);
}

void MapIntercept::CopyFromD3D()
{
	byte *sliceSrc = (byte *)d3d.pData;
	byte *sliceDst = (byte *)app.pData;

	RDCASSERT(numSlices > 0 && numRows > 0 &&
			 (numRows == 1 || (app.RowPitch > 0 && d3d.RowPitch > 0)) &&
			 (numSlices == 1 || (app.DepthPitch > 0 && d3d.DepthPitch > 0)));

	for(int slice=0; slice < numSlices; slice++)
	{
		byte *rowSrc = sliceSrc;
		byte *rowDst = sliceDst;

		for(int row=0; row < numRows; row++)
		{
			memcpy(rowDst, rowSrc, app.RowPitch);

			rowSrc += d3d.RowPitch;
			rowDst += app.RowPitch;
		}

		sliceSrc += d3d.DepthPitch;
		sliceDst += app.DepthPitch;
	}
}

void MapIntercept::CopyToD3D(size_t RangeStart, size_t RangeEnd)
{
	byte *sliceSrc = (byte *)app.pData;
	byte *sliceDst = (byte *)d3d.pData + RangeStart;

	RDCASSERT(numSlices > 0 && numRows > 0 &&
			 app.RowPitch > 0 && d3d.RowPitch > 0 &&
			 app.DepthPitch > 0 && d3d.DepthPitch > 0);

	for(int slice=0; slice < numSlices; slice++)
	{
		byte *rowSrc = sliceSrc;
		byte *rowDst = sliceDst;

		for(int row=0; row < numRows; row++)
		{
			size_t len = app.RowPitch;

			if(RangeEnd > 0)
			{
				if(rowSrc + len > (byte *)app.pData + (RangeEnd-RangeStart))
					len = (byte *)app.pData + (RangeEnd-RangeStart) - rowSrc;
			}

			memcpy(rowDst, rowSrc, len);

			rowSrc += app.RowPitch;
			rowDst += d3d.RowPitch;

			if(RangeEnd > 0 && rowSrc > (byte *)app.pData + (RangeEnd-RangeStart))
				return;
		}

		sliceSrc += app.DepthPitch;
		sliceDst += d3d.DepthPitch;
	}
}

bool WrappedID3D11DeviceContext::Serialise_Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource = D3D11_MAPPED_SUBRESOURCE();

	if(pMappedResource)
		mappedResource = *pMappedResource;

	D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));

	// we only serialise out unmap - should never hit this on read.
	RDCASSERT(m_State >= WRITING);

	RDCASSERT(record);

	if(record->NumSubResources > (int)Subresource)
		record = (D3D11ResourceRecord *)record->SubResources[Subresource];

	MapIntercept intercept;	

	size_t mapLength = record->Length;

	if(m_State == WRITING_CAPFRAME || (record && !record->DataInSerialiser))
	{
		ResourceId Resource = GetIDForResource(pResource);

		RDCASSERT(m_OpenMaps.find(MappedResource(Resource, Subresource)) == m_OpenMaps.end());

		ID3D11Resource *resMap = pResource;
		if(m_pDevice->GetResourceManager()->HasLiveResource(Resource))
			resMap = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(Resource);

		RDCASSERT(resMap);
		
		int ctxMapID = 0;

		if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
		{
			if(m_MapResourceRecordAllocs[Resource] == 0)
				m_MapResourceRecordAllocs[Resource] = record->GetContextID();

			ctxMapID = m_MapResourceRecordAllocs[Resource];

			RDCASSERT(ctxMapID != 0);
		}

		void *appMem = record->GetShadowPtr(ctxMapID, 0);

		if(appMem == NULL)
		{
			record->AllocShadowStorage(ctxMapID, mapLength);
			appMem = record->GetShadowPtr(ctxMapID, 0);

			if(MapType != D3D11_MAP_WRITE_DISCARD)
			{
				if(m_pDevice->GetResourceManager()->IsResourceDirty(Resource))
				{
					ID3D11DeviceChild *initial = m_pDevice->GetResourceManager()->GetInitialContents(Resource);

					if(WrappedID3D11Buffer::IsAlloc(pResource))
					{
						RDCASSERT(initial);

						ID3D11Buffer *stage = (ID3D11Buffer *)initial;

						D3D11_MAPPED_SUBRESOURCE mapped;
						HRESULT hr = m_pRealContext->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

						if(FAILED(hr))
						{
							RDCERR("Failed to map while getting initial states %08x", hr);
						}
						else
						{
							MapIntercept intercept;
							intercept.SetD3D(mapped);
							intercept.Init((ID3D11Buffer *)pResource, record->GetDataPtr());
							intercept.CopyFromD3D();

							RDCASSERT(mapLength == (size_t)record->Length);

							memcpy(appMem, record->GetDataPtr(), mapLength);

							m_pRealContext->Unmap(stage, 0);
						}
					}
					else
					{
						RDCUNIMPLEMENTED("Not getting initial contents for non-buffer GPU dirty map"); // need to get initial contents out
						RDCERR("CORRUPTION - Invalid/inaccurate initial data for Map() - non-buffer GPU dirty data mapped");
					}
				}
				else if(record->DataInSerialiser)
				{
					RDCASSERT(mapLength == (size_t)record->Length);
					memcpy(appMem, record->GetDataPtr(), mapLength);
				}
				else
				{
					memset(appMem, 0, mapLength);
				}
			}

			memcpy(record->GetShadowPtr(ctxMapID, 1), appMem, mapLength);
		}

		if(MapType == D3D11_MAP_WRITE_DISCARD)
		{
			memset(appMem, 0xcc, mapLength);
			memcpy(record->GetShadowPtr(ctxMapID, 1), appMem, mapLength);
		}

		intercept = MapIntercept();
		intercept.SetD3D(mappedResource);
		intercept.InitWrappedResource(resMap, Subresource, appMem);
		intercept.MapType = MapType;
		intercept.MapFlags = MapFlags;

		RDCASSERT(pMappedResource);
		*pMappedResource = intercept.app;

		m_OpenMaps[MappedResource(Resource, Subresource)] = intercept;
	}
	else if(m_State == WRITING_IDLE)
	{
		RDCASSERT(record->DataInSerialiser);

		mapLength = record->Length;
		
		intercept = MapIntercept();
		intercept.SetD3D(mappedResource);
		intercept.InitWrappedResource(pResource, Subresource, record->GetDataPtr());
		intercept.MapType = MapType;
		intercept.MapFlags = MapFlags;

		*pMappedResource = intercept.app;

		m_OpenMaps[MappedResource(GetIDForResource(pResource), Subresource)] = intercept;
	}
	else
	{
		RDCERR("Unexpected and unhandled case");
		RDCEraseEl(intercept);
	}

	// for read write fill out the buffer with what's on the mapped resource already
	if(MapType == D3D11_MAP_READ_WRITE || MapType == D3D11_MAP_READ)
	{
		intercept.CopyFromD3D();
	}
	else if(MapType == D3D11_MAP_WRITE_DISCARD)
	{
		// the easy case!
	}
	else if(MapType == D3D11_MAP_WRITE || MapType == D3D11_MAP_WRITE_NO_OVERWRITE)
	{
		// For now we'll just assume that the buffer contents are perfectly accurate
		// (which they are if no gpu writes to the buffer happens).

		// could take the performance hit and just copy anyway, spec doesn't see if the
		// data will be invalid but it will certainly be slow.
	}

	return true;
}

HRESULT WrappedID3D11DeviceContext::Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	bool straightUp = false;
	if(m_HighTrafficResources.find(pResource) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
		straightUp = true;

	if(m_pDevice->GetResourceManager()->IsResourceDirty(GetIDForResource(pResource)) && m_State != WRITING_CAPFRAME)
		straightUp = true;

	if((!straightUp && MapType == D3D11_MAP_WRITE_NO_OVERWRITE && m_State != WRITING_CAPFRAME) ||
		m_pRealContext->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
	{
		straightUp = true;
		m_HighTrafficResources.insert(pResource);
		if(m_State != WRITING_CAPFRAME)
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pResource));
	}

	if(straightUp && m_State == WRITING_IDLE)
	{
		return m_pRealContext->Map(m_pDevice->GetResourceManager()->UnwrapResource(pResource), Subresource,
										MapType, MapFlags, pMappedResource);
	}

	// can't promise no-overwrite as we're going to blat the whole buffer!
	HRESULT ret = m_pRealContext->Map(m_pDevice->GetResourceManager()->UnwrapResource(pResource), Subresource,
											MapType == D3D11_MAP_WRITE_NO_OVERWRITE ? D3D11_MAP_WRITE_DISCARD : MapType,
											MapFlags, pMappedResource);

	if(SUCCEEDED(ret))
	{
		if(m_State == WRITING_CAPFRAME)
		{
			if(MapType == D3D11_MAP_READ)
			{
				MapIntercept intercept;
				intercept.MapType = MapType;
				intercept.MapFlags = MapFlags;

				m_OpenMaps[MappedResource(GetIDForResource(pResource), Subresource)] = intercept;
			}
			else
			{
				m_MissingTracks.insert(GetIDForResource(pResource));

				Serialise_Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
			}
		}
		else if(m_State >= WRITING)
		{
			RDCASSERT(WrappedID3D11Buffer::IsAlloc(pResource) ||
				WrappedID3D11Texture1D::IsAlloc(pResource) ||
				WrappedID3D11Texture2D::IsAlloc(pResource) ||
				WrappedID3D11Texture3D::IsAlloc(pResource));

			ResourceId Id = GetIDForResource(pResource);

			D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(Id);
			RDCASSERT(record);

			D3D11ResourceRecord *parent = record;

			if(record->NumSubResources > (int)Subresource)
				record = (D3D11ResourceRecord *)record->SubResources[Subresource];

			record->UpdateCount++;

			if(record->UpdateCount > 60)
			{
				m_HighTrafficResources.insert(pResource);
				m_pDevice->GetResourceManager()->MarkDirtyResource(Id);

				return ret;
			}

			Serialise_Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
		}
	}

	return ret;
}

bool WrappedID3D11DeviceContext::Serialise_Unmap(ID3D11Resource *pResource, UINT Subresource_)
{
	MappedResource mapIdx;
	
	D3D11ResourceRecord *record = NULL;

	if(m_State >= WRITING)
	{
		record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));
		RDCASSERT(record);

		if(record->NumSubResources > (int)Subresource_)
			record = (D3D11ResourceRecord *)record->SubResources[Subresource_];
	}

	if(m_State < WRITING || m_State == WRITING_CAPFRAME || !record->DataInSerialiser)
	{
		SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
		SERIALISE_ELEMENT(uint32_t, Subresource, Subresource_);
		
		mapIdx = MappedResource(Resource, Subresource);
	}
	else if(m_State == WRITING_IDLE)
	{
		mapIdx = MappedResource(GetIDForResource(pResource), Subresource_);
	}

	MapIntercept intercept;
	
	if(m_State >= WRITING)
	{
		auto it = m_OpenMaps.find(mapIdx);

		RDCASSERT(it != m_OpenMaps.end());

		intercept = it->second;

		m_OpenMaps.erase(it);
	}

	if(m_State < WRITING || m_State == WRITING_CAPFRAME)
	{
		size_t len = record ? record->Length : 0;

		byte *appWritePtr = NULL;

		appWritePtr = (byte *)intercept.app.pData;

		size_t diffStart = 0;
		size_t diffEnd = len;
		
		int ctxMapID = 0;

		if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED && m_State == WRITING_CAPFRAME)
		{
			ctxMapID = m_MapResourceRecordAllocs[mapIdx.resource];

			RDCASSERT(ctxMapID != 0);
		}

		if(m_State == WRITING_CAPFRAME && len > 512 && intercept.MapType != D3D11_MAP_WRITE_DISCARD)
		{
			bool found = FindDiffRange(appWritePtr, record->GetShadowPtr(ctxMapID, 1), len, diffStart, diffEnd);
			if(found)
			{
				static size_t saved = 0;

				saved += len - (diffEnd-diffStart);

				RDCDEBUG("Mapped resource size %u, difference: %u -> %u. Total bytes saved so far: %u",
								(uint32_t)len, (uint32_t)diffStart, (uint32_t)diffEnd, (uint32_t)saved);

				len = diffEnd-diffStart;
			}
			else
			{
				diffStart = 0;
				diffEnd = 0;

				len = 1;
			}
		}

		appWritePtr += diffStart;
		if(m_State == WRITING_CAPFRAME && record->GetShadowPtr(ctxMapID, 1))
		{
			memcpy(record->GetShadowPtr(ctxMapID, 1)+diffStart, appWritePtr, diffEnd-diffStart);
		}
		
		SERIALISE_ELEMENT(D3D11_MAP, MapType, intercept.MapType);
		SERIALISE_ELEMENT(uint32_t, MapFlags, intercept.MapFlags);

		SERIALISE_ELEMENT(uint32_t, DiffStart, (uint32_t)diffStart);
		SERIALISE_ELEMENT(uint32_t, DiffEnd, (uint32_t)diffEnd);

		m_pSerialiser->SerialiseBuffer("MapData", appWritePtr, len);

		if(m_State <= EXECUTING && m_pDevice->GetResourceManager()->HasLiveResource(mapIdx.resource))
		{
			intercept.app.pData = appWritePtr;
			
			ID3D11Resource *res = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(mapIdx.resource);

			if(DiffStart >= DiffEnd)
			{
				// do nothing
			}
			else if(MapType == D3D11_MAP_WRITE_NO_OVERWRITE)
			{
				RDCASSERT(WrappedID3D11Buffer::IsAlloc(res));
				ID3D11Buffer *mapContents = NULL;

				D3D11_BUFFER_DESC bdesc;
				bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				bdesc.ByteWidth = DiffEnd-DiffStart;
				bdesc.CPUAccessFlags = 0;
				bdesc.MiscFlags = 0;
				bdesc.StructureByteStride = 0;
				bdesc.Usage = D3D11_USAGE_IMMUTABLE;

				D3D11_SUBRESOURCE_DATA data;
				data.pSysMem = appWritePtr;
				data.SysMemPitch = bdesc.ByteWidth;
				data.SysMemSlicePitch = bdesc.ByteWidth;

				HRESULT hr = m_pDevice->GetReal()->CreateBuffer(&bdesc, &data, &mapContents);

				if(FAILED(hr))
				{
					RDCERR("Failed to create temp Unmap() buffer %08x", hr);
				}
				else
				{
					m_pRealContext->CopySubresourceRegion(m_pDevice->GetResourceManager()->UnwrapResource(res), mapIdx.subresource,
															DiffStart, 0, 0,
															mapContents, 0, NULL);

					SAFE_RELEASE(mapContents);
				}
			}
			else
			{
				D3D11_MAPPED_SUBRESOURCE mappedResource;

				HRESULT hr = m_pRealContext->Map(m_pDevice->GetResourceManager()->UnwrapResource(res), mapIdx.subresource,
					MapType, MapFlags, &mappedResource);

				RDCASSERT(mappedResource.pData);

				if(FAILED(hr))
				{
					RDCERR("Failed to map resource, HRESULT: 0x%08x", hr);
				}
				else
				{
					intercept.SetD3D(mappedResource);
					intercept.InitWrappedResource(res, mapIdx.subresource, appWritePtr);

					intercept.CopyToD3D(DiffStart, DiffEnd);

					m_pRealContext->Unmap(m_pDevice->GetResourceManager()->UnwrapResource(res), mapIdx.subresource);
				}
			}

			SAFE_DELETE_ARRAY(appWritePtr);
		}
		else if(m_State == WRITING_CAPFRAME)
		{
			intercept.CopyToD3D();
		}
	}
	else if(m_State == WRITING_IDLE)
	{
		size_t len = record->Length;

		intercept.CopyToD3D();

		if(!record->DataInSerialiser)
		{
			uint32_t diffStart = 0;
			uint32_t diffEnd = (uint32_t)len;

			m_pSerialiser->Serialise("MapType", intercept.MapType);
			m_pSerialiser->Serialise("MapFlags", intercept.MapFlags);

			m_pSerialiser->Serialise("DiffStart", diffStart);
			m_pSerialiser->Serialise("DiffEnd", diffEnd);

			byte *buf = (byte *)intercept.app.pData;
			m_pSerialiser->SerialiseBuffer("MapData", buf, len);

			intercept.app.pData = buf;

			record->DataInSerialiser = true;
			record->SetDataOffset(m_pSerialiser->GetOffset()-record->Length);

			if(m_State < WRITING)
				SAFE_DELETE_ARRAY(buf);
		}
	}

	return true;
}

void WrappedID3D11DeviceContext::Unmap(ID3D11Resource *pResource, UINT Subresource)
{
	DrainAnnotationQueue();

	m_EmptyCommandList = false;

	auto it = m_OpenMaps.find(MappedResource(GetIDForResource(pResource), Subresource));

	if(m_State == WRITING_IDLE && m_HighTrafficResources.find(pResource) != m_HighTrafficResources.end())
	{
		// we intercepted this, even though we now don't need to serialise it. Time to finish what we started!
		if(it != m_OpenMaps.end() && it->second.MapType != D3D11_MAP_READ)
		{
			it->second.CopyToD3D();
			
			D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(it->first.resource);
			if(record) record->FreeShadowStorage();

			m_OpenMaps.erase(it);
		}
		else if(it != m_OpenMaps.end())
		{
			m_OpenMaps.erase(it);
		}
	}
	else if(m_State >= WRITING)
	{
		if(it == m_OpenMaps.end() && m_State == WRITING_CAPFRAME)
		{
			RDCWARN("Saw an Unmap that we didn't capture the corresponding Map for - this frame is unsuccessful");
			m_SuccessfulCapture = false;
			m_FailureReason = CaptureFailed_UncappedUnmap;
		}

		if(it != m_OpenMaps.end())
		{
			if(it->second.MapType == D3D11_MAP_READ)
			{
				m_OpenMaps.erase(it);
			}
			else if(m_State == WRITING_CAPFRAME)
			{
				MarkResourceReferenced(it->first.resource, eFrameRef_Read);
				MarkResourceReferenced(it->first.resource, eFrameRef_Write);

				SCOPED_SERIALISE_CONTEXT(UNMAP);
				m_pSerialiser->Serialise("context", m_ResourceID);	
				Serialise_Unmap(pResource, Subresource);

				m_ContextRecord->AddChunk(scope.Get());
			}
			else if(m_State >= WRITING)
			{
				RDCASSERT(WrappedID3D11Buffer::IsAlloc(pResource) ||
					WrappedID3D11Texture1D::IsAlloc(pResource) ||
					WrappedID3D11Texture2D::IsAlloc(pResource) ||
					WrappedID3D11Texture3D::IsAlloc(pResource));
				
				D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));
				RDCASSERT(record);

				D3D11ResourceRecord *parent = record;

				if(record->NumSubResources > (int)Subresource)
					record = (D3D11ResourceRecord *)record->SubResources[Subresource];

				if(record->DataInSerialiser)
				{
					Serialise_Unmap(pResource, Subresource);
				}
				else
				{
					SCOPED_SERIALISE_CONTEXT(UNMAP);
					m_pSerialiser->Serialise("context", m_ResourceID);	
					Serialise_Unmap(pResource, Subresource);

					Chunk *chunk = scope.Get();

					record->AddChunk(chunk);
					record->SetDataPtr(chunk->GetData());

					record->DataInSerialiser = true;
				}
			}
		}
	}
	
	m_pRealContext->Unmap(m_pDevice->GetResourceManager()->UnwrapResource(pResource), Subresource);
}

#pragma endregion Map

