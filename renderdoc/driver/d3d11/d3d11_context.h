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


#pragma once

#include "core/core.h"
#include "replay/renderdoc.h"

#if defined(INCLUDE_D3D_11_1)
#include <d3d11_1.h>
#endif

#include "d3d11_manager.h"

#include <map>
#include <list>
using std::map;
using std::list;

struct MapIntercept
{
	MapIntercept()
	{
		RDCEraseEl(app); RDCEraseEl(d3d);
		numRows = numSlices = 1;
	}

	void SetAppMemory(void *appMemory);
	void SetD3D(D3D11_MAPPED_SUBRESOURCE d3dMap);
	void SetD3D(D3D11_SUBRESOURCE_DATA d3dMap);
	
	void InitWrappedResource(ID3D11Resource *res, UINT sub, void *appMemory);

	void Init(ID3D11Buffer *buf, void *appMemory);
	void Init(ID3D11Texture1D *tex, UINT sub, void *appMemory);
	void Init(ID3D11Texture2D *tex, UINT sub, void *appMemory);
	void Init(ID3D11Texture3D *tex, UINT sub, void *appMemory);

	D3D11_MAPPED_SUBRESOURCE app, d3d;
	int numRows, numSlices;

	D3D11_MAP MapType;
	UINT MapFlags;

	void CopyFromD3D();
	void CopyToD3D(size_t RangeStart = 0, size_t RangeEnd = 0);
};

class WrappedID3D11DeviceContext;

#if defined(INCLUDE_D3D_11_1)
// ID3DUserDefinedAnnotation
class WrappedID3DUserDefinedAnnotation : public RefCounter, public ID3DUserDefinedAnnotation
{
	public:
		WrappedID3DUserDefinedAnnotation() : RefCounter(NULL), m_Context(NULL) {}

		void SetContext(WrappedID3D11DeviceContext *ctx)
		{ m_Context = ctx; }

		// doesn't need to soft-ref the device, for once!
		IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER_CUSTOMQUERY;
	
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

		virtual INT STDMETHODCALLTYPE BeginEvent(LPCWSTR Name);
		virtual INT STDMETHODCALLTYPE EndEvent();
		virtual void STDMETHODCALLTYPE SetMarker(LPCWSTR Name);
		virtual BOOL STDMETHODCALLTYPE GetStatus() { return TRUE; }
		
	private:
		WrappedID3D11DeviceContext *m_Context;
};
#endif

enum CaptureFailReason
{
	CaptureSucceeded = 0,
	CaptureFailed_UncappedUnmap,
	CaptureFailed_UncappedCmdlist,
};

struct DrawcallTreeNode
{
	DrawcallTreeNode() {}
	explicit DrawcallTreeNode(FetchDrawcall d) : draw(d) {}
	FetchDrawcall draw;
	vector<DrawcallTreeNode> children;

	rdctype::array<FetchDrawcall> Bake()
	{
		rdctype::array<FetchDrawcall> ret;
		if(children.empty()) return ret;

		create_array_uninit(ret, children.size());
		for(size_t i=0; i < children.size(); i++)
		{
			ret.elems[i] = children[i].draw;
			ret.elems[i].children = children[i].Bake();
		}

		return ret;
	}
};

class WrappedID3D11DeviceContext : public RefCounter,
#if defined(INCLUDE_D3D_11_1)
	public ID3D11DeviceContext1
#else
	public ID3D11DeviceContext
#endif
{
private:
	friend class WrappedID3D11DeviceContext;
	friend class WrappedID3DUserDefinedAnnotation;
	friend struct D3D11RenderState;

	struct MappedResource
	{
		MappedResource(ResourceId res = ResourceId(), UINT sub = 0) : resource(res), subresource(sub) {}
		ResourceId resource;
		UINT subresource;

		bool operator <(const MappedResource &o) const
		{
			if(resource != o.resource)
				return resource < o.resource;

			return subresource < o.subresource;
		}
	};

	set<IUnknown*> m_HighTrafficResources;
	map<MappedResource, MapIntercept> m_OpenMaps;

	map<ResourceId, vector<EventUsage> > m_ResourceUses;

	WrappedID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pRealContext;
#if defined(INCLUDE_D3D_11_1)
	ID3D11DeviceContext1* m_pRealContext1;
	bool m_SetCBuffer1;
#endif

	set<D3D11ResourceRecord *> m_DeferredRecords;
	map<ResourceId, int> m_MapResourceRecordAllocs;

	set<ResourceId> m_MissingTracks;

	ResourceId m_ResourceID;
	D3D11ResourceRecord *m_ContextRecord;

	Serialiser *m_pSerialiser;
	Serialiser *m_pDebugSerialiser;
	LogState m_State;
	CaptureFailReason m_FailureReason;
	bool m_SuccessfulCapture;
	bool m_EmptyCommandList;

	ResourceId m_FakeContext;

	bool m_DoStateVerify;
	D3D11RenderState *m_CurrentPipelineState;

	vector<FetchAPIEvent> m_CurEvents, m_Events;
	bool m_AddedDrawcall;
	
#if defined(INCLUDE_D3D_11_1)
	WrappedID3DUserDefinedAnnotation m_UserAnnotation;
#endif
	int32_t m_MarkerIndentLevel;

	struct Annotation
	{
		enum { ANNOT_SETMARKER, ANNOT_BEGINEVENT, ANNOT_ENDEVENT } m_Type;
		uint32_t m_Col;
		wstring m_Name;
	};
	vector<Annotation> m_AnnotationQueue;
	Threading::CriticalSection m_AnnotLock;

	uint64_t m_CurChunkOffset;
	uint32_t m_CurEventID, m_CurDrawcallID;

	DrawcallTreeNode m_ParentDrawcall;
	map<ResourceId,DrawcallTreeNode> m_CmdLists;

	list<DrawcallTreeNode *> m_DrawcallStack;

	const char *GetChunkName(D3D11ChunkType idx);
	
	vector<DebugMessage> Serialise_DebugMessages();

	void DrainAnnotationQueue();

	void AddUsage(FetchDrawcall draw);

	void AddEvent(D3D11ChunkType type, string description, ResourceId ctx = ResourceId());
	void AddDrawcall(FetchDrawcall draw, bool hasEvents);

	////////////////////////////////////////////////////////////////
	// implement InterceptorSystem privately, since it is not thread safe (like all other context functions)
	IMPLEMENT_FUNCTION_SERIALISED(void, SetMarker(uint32_t col, const wchar_t *name));
	IMPLEMENT_FUNCTION_SERIALISED(int, BeginEvent(uint32_t col, const wchar_t *name));
	IMPLEMENT_FUNCTION_SERIALISED(int, EndEvent());
public:
	static const int AllocPoolCount = 2048;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DeviceContext, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11DeviceContext(WrappedID3D11Device* realDevice, ID3D11DeviceContext* context, Serialiser *ser, Serialiser *debugser);
	void SetSerialiser(Serialiser *ser) { m_pSerialiser = ser; }
	virtual ~WrappedID3D11DeviceContext();

	void VerifyState();

	void BeginFrame();
	void EndFrame();

	bool Serialise_BeginCaptureFrame(bool applyInitialState);
	void BeginCaptureFrame();
	void EndCaptureFrame();
	
	void CleanupCapture();
	void FreeCaptureData();

	bool HasSuccessfulCapture(CaptureFailReason &reason) { reason = m_FailureReason; return m_SuccessfulCapture && m_ContextRecord->NumChunks() > 3; }

	void AttemptCapture();
	void FinishCapture();
	
	D3D11RenderState *GetCurrentPipelineState() { return m_CurrentPipelineState; }
	Serialiser *GetSerialiser() { return m_pSerialiser; }
	ResourceId GetResourceID() { return m_ResourceID; }
	ID3D11DeviceContext* GetReal() { return m_pRealContext; }

	void ProcessChunk(uint64_t offset, D3D11ChunkType chunk, bool forceExecute);
	void ReplayFakeContext(ResourceId id);
	void ReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
	
	void MarkResourceReferenced(ResourceId id, FrameRefType refType);

	vector<EventUsage> GetUsage(ResourceId id) { return m_ResourceUses[id]; }

	void ClearMaps();

	uint32_t GetEventID() { return m_CurEventID; }
	FetchAPIEvent GetEvent(uint32_t eventID);
	
	void ThreadSafe_SetMarker(uint32_t col, const wchar_t *name);
	int ThreadSafe_BeginEvent(uint32_t col, const wchar_t *name);
	int ThreadSafe_EndEvent();
	
	//////////////////////////////
	// implement IUnknown
	ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::SoftRef(m_pDevice); }
	ULONG STDMETHODCALLTYPE Release() { return RefCounter::SoftRelease(m_pDevice); }
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

	//////////////////////////////
	// implement IDXGIDeviceChild

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData( 
		/* [in] */ REFGUID Name,
		/* [in] */ UINT DataSize,
		/* [in] */ const void *pData)
	{ return m_pRealContext->SetPrivateData(Name, DataSize, pData); }

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface( 
		/* [in] */ REFGUID Name,
		/* [in] */ const IUnknown *pUnknown)
	{ return m_pRealContext->SetPrivateDataInterface(Name, pUnknown); }

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData( 
		/* [in] */ REFGUID Name,
		/* [out][in] */ UINT *pDataSize,
		/* [out] */ void *pData)
	{ return m_pRealContext->GetPrivateData(Name, pDataSize, pData); }

	virtual void STDMETHODCALLTYPE GetDevice( 
		/* [retval][out] */ ID3D11Device **ppDevice)
	{ *ppDevice = (ID3D11Device *)m_pDevice; (*ppDevice)->AddRef(); }
	
	//////////////////////////////
	// implement ID3D11DeviceContext

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D11PixelShader *pPixelShader,
		/* [annotation] */ 
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D11VertexShader *pVertexShader,
		/* [annotation] */ 
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawIndexed( 
		/* [annotation] */ 
		__in  UINT IndexCount,
		/* [annotation] */ 
		__in  UINT StartIndexLocation,
		/* [annotation] */ 
		__in  INT BaseVertexLocation));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Draw( 
		/* [annotation] */ 
		__in  UINT VertexCount,
		/* [annotation] */ 
		__in  UINT StartVertexLocation));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Map( 
		/* [annotation] */ 
		__in  ID3D11Resource *pResource,
		/* [annotation] */ 
		__in  UINT Subresource,
		/* [annotation] */ 
		__in  D3D11_MAP MapType,
		/* [annotation] */ 
		__in  UINT MapFlags,
		/* [annotation] */ 
		__out  D3D11_MAPPED_SUBRESOURCE *pMappedResource));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Unmap( 
		/* [annotation] */ 
		__in  ID3D11Resource *pResource,
		/* [annotation] */ 
		__in  UINT Subresource));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetInputLayout( 
		/* [annotation] */ 
		__in_opt  ID3D11InputLayout *pInputLayout));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetVertexBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  const UINT *pStrides,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  const UINT *pOffsets));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetIndexBuffer( 
		/* [annotation] */ 
		__in_opt  ID3D11Buffer *pIndexBuffer,
		/* [annotation] */ 
		__in  DXGI_FORMAT Format,
		/* [annotation] */ 
		__in  UINT Offset));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawIndexedInstanced( 
		/* [annotation] */ 
		__in  UINT IndexCountPerInstance,
		/* [annotation] */ 
		__in  UINT InstanceCount,
		/* [annotation] */ 
		__in  UINT StartIndexLocation,
		/* [annotation] */ 
		__in  INT BaseVertexLocation,
		/* [annotation] */ 
		__in  UINT StartInstanceLocation));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawInstanced( 
		/* [annotation] */ 
		__in  UINT VertexCountPerInstance,
		/* [annotation] */ 
		__in  UINT InstanceCount,
		/* [annotation] */ 
		__in  UINT StartVertexLocation,
		/* [annotation] */ 
		__in  UINT StartInstanceLocation));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D11GeometryShader *pShader,
		/* [annotation] */ 
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetPrimitiveTopology( 
		/* [annotation] */ 
		__in  D3D11_PRIMITIVE_TOPOLOGY Topology));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Begin( 
		/* [annotation] */ 
		__in  ID3D11Asynchronous *pAsync));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, End( 
		/* [annotation] */ 
		__in  ID3D11Asynchronous *pAsync));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetData( 
		/* [annotation] */ 
		__in  ID3D11Asynchronous *pAsync,
		/* [annotation] */ 
		__out_bcount_opt( DataSize )  void *pData,
		/* [annotation] */ 
		__in  UINT DataSize,
		/* [annotation] */ 
		__in  UINT GetDataFlags));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetPredication( 
		/* [annotation] */ 
		__in_opt  ID3D11Predicate *pPredicate,
		/* [annotation] */ 
		__in  BOOL PredicateValue));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetRenderTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount_opt(NumViews)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */ 
		__in_opt  ID3D11DepthStencilView *pDepthStencilView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetRenderTargetsAndUnorderedAccessViews( 
		/* [annotation] */ 
		__in  UINT NumRTVs,
		/* [annotation] */ 
		__in_ecount_opt(NumRTVs)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		/* [annotation] */ 
		__in_opt  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
		/* [annotation] */ 
		__in  UINT NumUAVs,
		/* [annotation] */ 
		__in_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */ 
		__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetBlendState( 
		/* [annotation] */ 
		__in_opt  ID3D11BlendState *pBlendState,
		/* [annotation] */ 
		__in_opt  const FLOAT BlendFactor[ 4 ],
		/* [annotation] */ 
		__in  UINT SampleMask));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetDepthStencilState( 
		/* [annotation] */ 
		__in_opt  ID3D11DepthStencilState *pDepthStencilState,
		/* [annotation] */ 
		__in  UINT StencilRef));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SOSetTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
		/* [annotation] */ 
		__in_ecount_opt(NumBuffers)  const UINT *pOffsets));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawAuto( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawIndexedInstancedIndirect( 
		/* [annotation] */ 
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */ 
		__in  UINT AlignedByteOffsetForArgs));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawInstancedIndirect( 
		/* [annotation] */ 
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */ 
		__in  UINT AlignedByteOffsetForArgs));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Dispatch( 
		/* [annotation] */ 
		__in  UINT ThreadGroupCountX,
		/* [annotation] */ 
		__in  UINT ThreadGroupCountY,
		/* [annotation] */ 
		__in  UINT ThreadGroupCountZ));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DispatchIndirect( 
		/* [annotation] */ 
		__in  ID3D11Buffer *pBufferForArgs,
		/* [annotation] */ 
		__in  UINT AlignedByteOffsetForArgs));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetState( 
		/* [annotation] */ 
		__in_opt  ID3D11RasterizerState *pRasterizerState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetViewports( 
		/* [annotation] */ 
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		/* [annotation] */ 
		__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetScissorRects( 
		/* [annotation] */ 
		__in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		/* [annotation] */ 
		__in_ecount_opt(NumRects)  const D3D11_RECT *pRects));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopySubresourceRegion( 
		/* [annotation] */ 
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */ 
		__in  UINT DstSubresource,
		/* [annotation] */ 
		__in  UINT DstX,
		/* [annotation] */ 
		__in  UINT DstY,
		/* [annotation] */ 
		__in  UINT DstZ,
		/* [annotation] */ 
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */ 
		__in  UINT SrcSubresource,
		/* [annotation] */ 
		__in_opt  const D3D11_BOX *pSrcBox));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyResource( 
		/* [annotation] */ 
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */ 
		__in  ID3D11Resource *pSrcResource));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, UpdateSubresource( 
		/* [annotation] */ 
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */ 
		__in  UINT DstSubresource,
		/* [annotation] */ 
		__in_opt  const D3D11_BOX *pDstBox,
		/* [annotation] */ 
		__in  const void *pSrcData,
		/* [annotation] */ 
		__in  UINT SrcRowPitch,
		/* [annotation] */ 
		__in  UINT SrcDepthPitch));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyStructureCount( 
		/* [annotation] */ 
		__in  ID3D11Buffer *pDstBuffer,
		/* [annotation] */ 
		__in  UINT DstAlignedByteOffset,
		/* [annotation] */ 
		__in  ID3D11UnorderedAccessView *pSrcView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearRenderTargetView( 
		/* [annotation] */ 
		__in  ID3D11RenderTargetView *pRenderTargetView,
		/* [annotation] */ 
		__in  const FLOAT ColorRGBA[ 4 ]));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearUnorderedAccessViewUint( 
		/* [annotation] */ 
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */ 
		__in  const UINT Values[ 4 ]));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearUnorderedAccessViewFloat( 
		/* [annotation] */ 
		__in  ID3D11UnorderedAccessView *pUnorderedAccessView,
		/* [annotation] */ 
		__in  const FLOAT Values[ 4 ]));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearDepthStencilView( 
		/* [annotation] */ 
		__in  ID3D11DepthStencilView *pDepthStencilView,
		/* [annotation] */ 
		__in  UINT ClearFlags,
		/* [annotation] */ 
		__in  FLOAT Depth,
		/* [annotation] */ 
		__in  UINT8 Stencil));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GenerateMips( 
		/* [annotation] */ 
		__in  ID3D11ShaderResourceView *pShaderResourceView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetResourceMinLOD( 
		/* [annotation] */ 
		__in  ID3D11Resource *pResource,
		FLOAT MinLOD));

	IMPLEMENT_FUNCTION_SERIALISED(virtual FLOAT STDMETHODCALLTYPE, GetResourceMinLOD( 
		/* [annotation] */ 
		__in  ID3D11Resource *pResource));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ResolveSubresource( 
		/* [annotation] */ 
		__in  ID3D11Resource *pDstResource,
		/* [annotation] */ 
		__in  UINT DstSubresource,
		/* [annotation] */ 
		__in  ID3D11Resource *pSrcResource,
		/* [annotation] */ 
		__in  UINT SrcSubresource,
		/* [annotation] */ 
		__in  DXGI_FORMAT Format));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ExecuteCommandList( 
		/* [annotation] */ 
		__in  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D11HullShader *pHullShader,
		/* [annotation] */ 
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D11DomainShader *pDomainShader,
		/* [annotation] */ 
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSSetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSSetUnorderedAccessViews( 
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
		/* [annotation] */ 
		__in_ecount(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		/* [annotation] */ 
		__in_ecount(NumUAVs)  const UINT *pUAVInitialCounts));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSSetShader( 
		/* [annotation] */ 
		__in_opt  ID3D11ComputeShader *pComputeShader,
		/* [annotation] */ 
		__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSSetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSSetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSGetShader( 
		/* [annotation] */ 
		__out  ID3D11PixelShader **ppPixelShader,
		/* [annotation] */ 
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */ 
		__inout_opt  UINT *pNumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSGetShader( 
		/* [annotation] */ 
		__out  ID3D11VertexShader **ppVertexShader,
		/* [annotation] */ 
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */ 
		__inout_opt  UINT *pNumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IAGetInputLayout( 
		/* [annotation] */ 
		__out  ID3D11InputLayout **ppInputLayout));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IAGetVertexBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  UINT *pStrides,
		/* [annotation] */ 
		__out_ecount_opt(NumBuffers)  UINT *pOffsets));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IAGetIndexBuffer( 
		/* [annotation] */ 
		__out_opt  ID3D11Buffer **pIndexBuffer,
		/* [annotation] */ 
		__out_opt  DXGI_FORMAT *Format,
		/* [annotation] */ 
		__out_opt  UINT *Offset));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSGetShader( 
		/* [annotation] */ 
		__out  ID3D11GeometryShader **ppGeometryShader,
		/* [annotation] */ 
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */ 
		__inout_opt  UINT *pNumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IAGetPrimitiveTopology( 
		/* [annotation] */ 
		__out  D3D11_PRIMITIVE_TOPOLOGY *pTopology));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GetPredication( 
		/* [annotation] */ 
		__out_opt  ID3D11Predicate **ppPredicate,
		/* [annotation] */ 
		__out_opt  BOOL *pPredicateValue));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMGetRenderTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */ 
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMGetRenderTargetsAndUnorderedAccessViews( 
		/* [annotation] */ 
		__in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumRTVs,
		/* [annotation] */ 
		__out_ecount_opt(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
		/* [annotation] */ 
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView,
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot )  UINT NumUAVs,
		/* [annotation] */ 
		__out_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMGetBlendState( 
		/* [annotation] */ 
		__out_opt  ID3D11BlendState **ppBlendState,
		/* [annotation] */ 
		__out_opt  FLOAT BlendFactor[ 4 ],
		/* [annotation] */ 
		__out_opt  UINT *pSampleMask));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMGetDepthStencilState( 
		/* [annotation] */ 
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
		/* [annotation] */ 
		__out_opt  UINT *pStencilRef));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SOGetTargets( 
		/* [annotation] */ 
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSGetState( 
		/* [annotation] */ 
		__out  ID3D11RasterizerState **ppRasterizerState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSGetViewports( 
		/* [annotation] */ 
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		/* [annotation] */ 
		__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSGetScissorRects( 
		/* [annotation] */ 
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		/* [annotation] */ 
		__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSGetShader( 
		/* [annotation] */ 
		__out  ID3D11HullShader **ppHullShader,
		/* [annotation] */ 
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */ 
		__inout_opt  UINT *pNumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSGetShader( 
		/* [annotation] */ 
		__out  ID3D11DomainShader **ppDomainShader,
		/* [annotation] */ 
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */ 
		__inout_opt  UINT *pNumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSGetShaderResources( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
		/* [annotation] */ 
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSGetUnorderedAccessViews( 
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
		/* [annotation] */ 
		__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSGetShader( 
		/* [annotation] */ 
		__out  ID3D11ComputeShader **ppComputeShader,
		/* [annotation] */ 
		__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		/* [annotation] */ 
		__inout_opt  UINT *pNumClassInstances));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSGetSamplers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
		/* [annotation] */ 
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSGetConstantBuffers( 
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
		/* [annotation] */ 
		__in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
		/* [annotation] */ 
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearState( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Flush( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE, GetType( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetContextFlags( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, FinishCommandList( 
		BOOL RestoreDeferredContextState,
		/* [annotation] */ 
		__out_opt  ID3D11CommandList **ppCommandList));
	
	//////////////////////////////
	// implement ID3D11DeviceContext1
	
	// outside the define as it doesn't depend on any 11_1 definitions, and it's just an unused
	// virtual function. We re-use the Serialise_UpdateSubresource1 function for Serialise_UpdateSubresource
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, UpdateSubresource1( 
        /* [annotation] */ 
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */ 
        _In_  UINT DstSubresource,
        /* [annotation] */ 
        _In_opt_  const D3D11_BOX *pDstBox,
        /* [annotation] */ 
        _In_  const void *pSrcData,
        /* [annotation] */ 
        _In_  UINT SrcRowPitch,
        /* [annotation] */ 
        _In_  UINT SrcDepthPitch,
        /* [annotation] */ 
        _In_  UINT CopyFlags));
    
#if defined(INCLUDE_D3D_11_1)
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopySubresourceRegion1( 
        /* [annotation] */ 
        _In_  ID3D11Resource *pDstResource,
        /* [annotation] */ 
        _In_  UINT DstSubresource,
        /* [annotation] */ 
        _In_  UINT DstX,
        /* [annotation] */ 
        _In_  UINT DstY,
        /* [annotation] */ 
        _In_  UINT DstZ,
        /* [annotation] */ 
        _In_  ID3D11Resource *pSrcResource,
        /* [annotation] */ 
        _In_  UINT SrcSubresource,
        /* [annotation] */ 
        _In_opt_  const D3D11_BOX *pSrcBox,
        /* [annotation] */ 
        _In_  UINT CopyFlags));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DiscardResource( 
        /* [annotation] */ 
        _In_  ID3D11Resource *pResource));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DiscardView( 
        /* [annotation] */ 
        _In_  ID3D11View *pResourceView));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSSetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSSetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSSetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSSetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSSetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSSetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
        /* [annotation] */ 
        _In_reads_opt_(NumBuffers)  const UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, VSGetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, HSGetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DSGetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GSGetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, PSGetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CSGetConstantBuffers1( 
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        _In_range_( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
        /* [annotation] */ 
        _Out_writes_opt_(NumBuffers)  UINT *pNumConstants));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SwapDeviceContextState( 
        /* [annotation] */ 
        _In_  ID3DDeviceContextState *pState,
        /* [annotation] */ 
        _Out_opt_  ID3DDeviceContextState **ppPreviousState));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearView( 
        /* [annotation] */ 
        _In_  ID3D11View *pView,
        /* [annotation] */ 
        _In_  const FLOAT Color[ 4 ],
        /* [annotation] */ 
        _In_reads_opt_(NumRects)  const D3D11_RECT *pRect,
        UINT NumRects));
    
    IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DiscardView1( 
        /* [annotation] */ 
        _In_  ID3D11View *pResourceView,
        /* [annotation] */ 
        _In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
        UINT NumRects));
#endif
};
