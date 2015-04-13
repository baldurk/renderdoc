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

#include <stdint.h>

#include "common/threading.h"
#include "common/timing.h"

#include "core/core.h"
#include "api/replay/renderdoc_replay.h"

#include "d3d11_common.h"

#if defined(INCLUDE_D3D_11_1)
#include <d3d11_1.h>
#include <d3d11_2.h>
#endif

#include "d3d11_manager.h"
#include "d3d11_replay.h"
#include "d3d11_debug.h"

#include <map>
using std::map;

enum TextureDisplayType
{
	TEXDISPLAY_UNKNOWN = 0,
	TEXDISPLAY_SRV_COMPATIBLE,
	TEXDISPLAY_DEPTH_TARGET,
	TEXDISPLAY_INDIRECT_VIEW,
};

struct D3D11InitParams : public RDCInitParams
{
	D3D11InitParams();
	ReplayCreateStatus Serialise();

	D3D_DRIVER_TYPE DriverType;
	UINT Flags;
	UINT SDKVersion;
	UINT NumFeatureLevels;
	D3D_FEATURE_LEVEL FeatureLevels[16];
	
	static const uint32_t D3D11_SERIALISE_VERSION = 0x0000006;

	// backwards compatibility for old logs described at the declaration of this array
	static const uint32_t D3D11_NUM_SUPPORTED_OLD_VERSIONS = 2;
	static const uint32_t D3D11_OLD_VERSIONS[D3D11_NUM_SUPPORTED_OLD_VERSIONS];

	// version number internal to d3d11 stream
	uint32_t SerialiseVersion;
};

class WrappedID3D11Device;

// We can pass through all calls to ID3D11Debug without intercepting, this
// struct isonly here so that we can intercept QueryInterface calls to return
// ID3D11InfoQueue
struct WrappedID3D11Debug : public ID3D11Debug
{
	WrappedID3D11Device *m_pDevice;
	ID3D11Debug *m_pDebug;

	WrappedID3D11Debug() { }

	//////////////////////////////
	// implement IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	//////////////////////////////
	// implement ID3D11Debug
	virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(UINT Mask)
	{ return m_pDebug->SetFeatureMask(Mask); }
	virtual UINT STDMETHODCALLTYPE GetFeatureMask()
	{ return m_pDebug->GetFeatureMask(); }
	virtual HRESULT STDMETHODCALLTYPE SetPresentPerRenderOpDelay(UINT Milliseconds)
	{ return m_pDebug->SetPresentPerRenderOpDelay(Milliseconds); }
	virtual UINT STDMETHODCALLTYPE GetPresentPerRenderOpDelay()
	{ return m_pDebug->GetPresentPerRenderOpDelay(); }
	virtual HRESULT STDMETHODCALLTYPE SetSwapChain(IDXGISwapChain *pSwapChain)
	{ return m_pDebug->SetSwapChain(pSwapChain); }
	virtual HRESULT STDMETHODCALLTYPE GetSwapChain(IDXGISwapChain **ppSwapChain)
	{ return m_pDebug->GetSwapChain(ppSwapChain); }
	virtual HRESULT STDMETHODCALLTYPE ValidateContext(ID3D11DeviceContext *pContext)
	{ return m_pDebug->ValidateContext(pContext); }
	virtual HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(D3D11_RLDO_FLAGS Flags)
	{ return m_pDebug->ReportLiveDeviceObjects(Flags); }
	virtual HRESULT STDMETHODCALLTYPE ValidateContextForDispatch(ID3D11DeviceContext *pContext)
	{ return m_pDebug->ValidateContextForDispatch(pContext); }
};

// give every impression of working but do nothing.
// Just allow the user to call functions so that they don't
// have to check for E_NOINTERFACE when they expect an infoqueue to be there
struct DummyID3D11InfoQueue : public ID3D11InfoQueue
{
	WrappedID3D11Device *m_pDevice;

	DummyID3D11InfoQueue() {}

	//////////////////////////////
	// implement IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return E_NOINTERFACE; }
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	//////////////////////////////
	// implement ID3D11InfoQueue
	virtual HRESULT STDMETHODCALLTYPE SetMessageCountLimit(UINT64 MessageCountLimit) { return S_OK; }
	virtual void STDMETHODCALLTYPE ClearStoredMessages() { }
	virtual HRESULT STDMETHODCALLTYPE GetMessage(UINT64 MessageIndex, D3D11_MESSAGE *pMessage, SIZE_T *pMessageByteLength) { return S_OK; }
	virtual UINT64 STDMETHODCALLTYPE GetNumMessagesAllowedByStorageFilter() { return 0; }
	virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDeniedByStorageFilter() { return 0; }
	virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessages() { return 0; }
	virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessagesAllowedByRetrievalFilter() { return 0; }
	virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDiscardedByMessageCountLimit() { return 0; }
	virtual UINT64 STDMETHODCALLTYPE GetMessageCountLimit() { return 0; }
	virtual HRESULT STDMETHODCALLTYPE AddStorageFilterEntries(D3D11_INFO_QUEUE_FILTER *pFilter) { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE GetStorageFilter(D3D11_INFO_QUEUE_FILTER *pFilter, SIZE_T *pFilterByteLength) { return S_OK; }
	virtual void STDMETHODCALLTYPE ClearStorageFilter() { }
	virtual HRESULT STDMETHODCALLTYPE PushEmptyStorageFilter() { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE PushCopyOfStorageFilter() { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE PushStorageFilter(D3D11_INFO_QUEUE_FILTER *pFilter) { return S_OK; }
	virtual void STDMETHODCALLTYPE PopStorageFilter() { }
	virtual UINT STDMETHODCALLTYPE GetStorageFilterStackSize() { return 0; }
	virtual HRESULT STDMETHODCALLTYPE AddRetrievalFilterEntries(D3D11_INFO_QUEUE_FILTER *pFilter) { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE GetRetrievalFilter(D3D11_INFO_QUEUE_FILTER *pFilter, SIZE_T *pFilterByteLength) { return S_OK; }
	virtual void STDMETHODCALLTYPE ClearRetrievalFilter() { }
	virtual HRESULT STDMETHODCALLTYPE PushEmptyRetrievalFilter() { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE PushCopyOfRetrievalFilter() { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE PushRetrievalFilter(D3D11_INFO_QUEUE_FILTER *pFilter) { return S_OK; }
	virtual void STDMETHODCALLTYPE PopRetrievalFilter() { }
	virtual UINT STDMETHODCALLTYPE GetRetrievalFilterStackSize() { return 0; }
	virtual HRESULT STDMETHODCALLTYPE AddMessage(D3D11_MESSAGE_CATEGORY Category, D3D11_MESSAGE_SEVERITY Severity, D3D11_MESSAGE_ID ID, LPCSTR pDescription) { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE AddApplicationMessage(D3D11_MESSAGE_SEVERITY Severity, LPCSTR pDescription) { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE SetBreakOnCategory(D3D11_MESSAGE_CATEGORY Category, BOOL bEnable) { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY Severity, BOOL bEnable) { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE SetBreakOnID(D3D11_MESSAGE_ID ID, BOOL bEnable) { return S_OK; }
	virtual BOOL STDMETHODCALLTYPE GetBreakOnCategory(D3D11_MESSAGE_CATEGORY Category) { return FALSE; }
	virtual BOOL STDMETHODCALLTYPE GetBreakOnSeverity(D3D11_MESSAGE_SEVERITY Severity) { return FALSE; }
	virtual BOOL STDMETHODCALLTYPE GetBreakOnID(D3D11_MESSAGE_ID ID) { return FALSE; }
	virtual void STDMETHODCALLTYPE SetMuteDebugOutput(BOOL bMute) { }
	virtual BOOL STDMETHODCALLTYPE GetMuteDebugOutput() { return TRUE; }
};

class WrappedID3D11ClassLinkage;
enum CaptureFailReason;

#if defined(INCLUDE_D3D_11_1)
#define D3DDEVICEPARENT ID3D11Device2
#else
#define D3DDEVICEPARENT ID3D11Device
#endif

class WrappedID3D11Device : public IFrameCapturer, public D3DDEVICEPARENT
{
private:
	// since enumeration creates a lot of devices, save
	// large-scale init until some point that we know we're the real device
	void LazyInit();

	enum {
		eInitialContents_Copy = 0,
		eInitialContents_ClearRTV = 1,
		eInitialContents_ClearDSV = 2,
	};
		
	D3D11Replay m_Replay;

	DummyID3D11InfoQueue m_DummyInfoQueue;
	WrappedID3D11Debug m_WrappedDebug;

	unsigned int m_InternalRefcount;
	RefCounter m_RefCounter;
	RefCounter m_SoftRefCounter;
	bool m_Alive;

	D3D11DebugManager *m_DebugManager;
	D3D11ResourceManager *m_ResourceManager;
	
	D3D11InitParams m_InitParams;

	ID3D11Device* m_pDevice;
#if defined(INCLUDE_D3D_11_1)
	ID3D11Device1* m_pDevice1;
	ID3D11Device2* m_pDevice2;
#endif
	ID3D11InfoQueue *m_pInfoQueue;
	WrappedID3D11DeviceContext* m_pImmediateContext;

	// ensure all calls in via the D3D wrapped interface are thread safe
	// protects wrapped resource creation and serialiser access
	Threading::CriticalSection m_D3DLock;

	ResourceId m_ResourceID;
	D3D11ResourceRecord *m_DeviceRecord;

	Serialiser *m_pSerialiser;
	Serialiser *m_pDebugSerialiser;
	LogState m_State;
	bool m_AppControlledCapture;
	
	set<ID3D11DeviceChild *> m_CachedStateObjects;
	set<WrappedID3D11DeviceContext *> m_DeferredContexts;
	map<ID3D11InputLayout *, vector<D3D11_INPUT_ELEMENT_DESC> > m_LayoutDescs;
	map<ID3D11InputLayout *, ShaderReflection *> m_LayoutDXBC;

	ResourceId m_ReplayDefCtx;
	uint32_t m_FirstDefEv;
	uint32_t m_LastDefEv;

	static WrappedID3D11Device *m_pCurrentWrappedDevice;

	map<IDXGISwapChain*, ID3D11RenderTargetView*> m_SwapChains;

	uint32_t m_FrameCounter;
	uint32_t m_FailedFrame;
	CaptureFailReason m_FailedReason;
	uint32_t m_Failures;

	uint64_t m_CurFileSize;

	PerformanceTimer m_FrameTimer;
	vector<double> m_FrameTimes;
	double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;

	vector<DebugMessage> m_DebugMessages;

	vector<FetchFrameRecord> m_FrameRecord;
	const FetchDrawcall *GetDrawcall(const FetchDrawcall *draw, uint32_t eventID);
public:
	static const int AllocPoolCount = 4;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Device, AllocPoolCount);

	WrappedID3D11Device(ID3D11Device* realDevice, D3D11InitParams *params);
	void SetLogFile(const char *logfile);
	void SetLogVersion(uint32_t fileversion) { LazyInit(); m_InitParams.SerialiseVersion = fileversion; }
	uint32_t GetLogVersion() { return m_InitParams.SerialiseVersion; }
	virtual ~WrappedID3D11Device();

	////////////////////////////////////////////////////////////////
	// non wrapping interface

	ID3D11Device *GetReal() { return m_pDevice; }
	static const char *GetChunkName(uint32_t idx);
	D3D11DebugManager *GetDebugManager() { return m_DebugManager; }
	D3D11ResourceManager *GetResourceManager() { return m_ResourceManager; }

	D3D11Replay *GetReplay() { return &m_Replay; }

	WrappedID3D11DeviceContext *GetImmediateContext() { return m_pImmediateContext; }
	size_t GetNumDeferredContexts() { return m_DeferredContexts.size(); }
	void AddDeferredContext(WrappedID3D11DeviceContext *defctx);
	void RemoveDeferredContext(WrappedID3D11DeviceContext *defctx);
	WrappedID3D11DeviceContext *GetDeferredContext(size_t idx);

	Serialiser *GetSerialiser() { return m_pSerialiser; }

	ResourceId GetResourceID() { return m_ResourceID; }

	vector<FetchFrameRecord> &GetFrameRecord() { return m_FrameRecord; }

	const FetchDrawcall *GetDrawcall(uint32_t frameID, uint32_t eventID);

	vector<DebugMessage> GetDebugMessages();
	void AddDebugMessage(DebugMessage msg) { if(m_State < WRITING) m_DebugMessages.push_back(msg); }
	void AddDebugMessage(DebugMessageCategory c, DebugMessageSeverity sv, DebugMessageSource src, std::string d);
	const vector<D3D11_INPUT_ELEMENT_DESC> &GetLayoutDesc(ID3D11InputLayout *layout) { return m_LayoutDescs[layout]; }
	ShaderReflection *GetLayoutDXBC(ID3D11InputLayout *layout) { return m_LayoutDXBC[layout]; }

	void ReleaseSwapchainResources(IDXGISwapChain *swap);
	
	void Serialise_CaptureScope(uint64_t offset);

	void StartFrameCapture(void *dev, void *wnd);
	bool EndFrameCapture(void *dev, void *wnd);

	////////////////////////////////////////////////////////////////
	// log replaying
	
	bool Prepare_InitialState(ID3D11DeviceChild *res);
	bool Serialise_InitialState(ID3D11DeviceChild *res);
	void Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData);
	void Apply_InitialState(ID3D11DeviceChild *live, D3D11ResourceManager::InitialContentData initial);

	void ReadLogInitialisation();
	void ProcessChunk(uint64_t offset, D3D11ChunkType context);
	void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
	void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
	
	////////////////////////////////////////////////////////////////
	// 'fake' interfaces

	// Resource
	IMPLEMENT_FUNCTION_SERIALISED(void, SetResourceName(ID3D11DeviceChild *res, const char *name));
	IMPLEMENT_FUNCTION_SERIALISED(void, ReleaseResource(ID3D11DeviceChild *res));

	// Class Linkage
	IMPLEMENT_FUNCTION_SERIALISED(ID3D11ClassInstance*, CreateClassInstance(LPCSTR pClassTypeName,
																			UINT ConstantBufferOffset, UINT ConstantVectorOffset,
																			UINT TextureOffset, UINT SamplerOffset,
																			WrappedID3D11ClassLinkage *linkage, ID3D11ClassInstance *inst));

	IMPLEMENT_FUNCTION_SERIALISED(ID3D11ClassInstance*, GetClassInstance(LPCSTR pClassInstanceName, UINT InstanceIndex,
																		 WrappedID3D11ClassLinkage *linkage, ID3D11ClassInstance *inst));

	// Swap Chain
	IMPLEMENT_FUNCTION_SERIALISED(void, SetSwapChainTexture(IDXGISwapChain *swap, DXGI_SWAP_CHAIN_DESC *desc, UINT buffer, ID3D11Texture2D *pTex));
	HRESULT Present(IDXGISwapChain *swap, UINT SyncInterval, UINT Flags);
	
	void InternalRef() { InterlockedIncrement(&m_InternalRefcount); }
	void InternalRelease() { InterlockedDecrement(&m_InternalRefcount); }

	void SoftRef() { m_SoftRefCounter.AddRef(); }
	void SoftRelease() { m_SoftRefCounter.Release(); CheckForDeath(); }
	void CheckForDeath();

	////////////////////////////////////////////////////////////////
	// Functions for D3D9 hooks to call into (D3DPERF api)

	static void SetMarker(uint32_t col, const wchar_t *name);
	static int BeginEvent(uint32_t col, const wchar_t *name);
	static int EndEvent();

	//////////////////////////////
	// implement IUnknown
	ULONG STDMETHODCALLTYPE AddRef() { return m_RefCounter.AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { unsigned int ret = m_RefCounter.Release(); CheckForDeath(); return ret; }
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
	

	//////////////////////////////
	// implement ID3D11Device
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateBuffer( 
		const D3D11_BUFFER_DESC *pDesc,
		const D3D11_SUBRESOURCE_DATA *pInitialData,
		ID3D11Buffer **ppBuffer));
	
	template<typename TexDesc>
	TextureDisplayType DispTypeForTexture(TexDesc &Descriptor);

	vector<D3D11_SUBRESOURCE_DATA> Serialise_CreateTextureData(ID3D11Resource *tex, ResourceId id, const D3D11_SUBRESOURCE_DATA *data,
															   UINT w, UINT h, UINT d, DXGI_FORMAT fmt, UINT mips, UINT arr, bool HasData);

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateTexture1D( 
		const D3D11_TEXTURE1D_DESC *pDesc,
		const D3D11_SUBRESOURCE_DATA *pInitialData,
		ID3D11Texture1D **ppTexture1D));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateTexture2D( 
		const D3D11_TEXTURE2D_DESC *pDesc,
		const D3D11_SUBRESOURCE_DATA *pInitialData,
		ID3D11Texture2D **ppTexture2D));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateTexture3D( 
		const D3D11_TEXTURE3D_DESC *pDesc,
		const D3D11_SUBRESOURCE_DATA *pInitialData,
		ID3D11Texture3D **ppTexture3D));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateShaderResourceView( 
		ID3D11Resource *pResource,
		const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
		ID3D11ShaderResourceView **ppSRView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateUnorderedAccessView( 
		ID3D11Resource *pResource,
		const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
		ID3D11UnorderedAccessView **ppUAView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRenderTargetView( 
		ID3D11Resource *pResource,
		const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
		ID3D11RenderTargetView **ppRTView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDepthStencilView( 
		ID3D11Resource *pResource,
		const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
		ID3D11DepthStencilView **ppDepthStencilView));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateInputLayout( 
		const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
		UINT NumElements,
		const void *pShaderBytecodeWithInputSignature,
		SIZE_T BytecodeLength,
		ID3D11InputLayout **ppInputLayout));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateVertexShader( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11VertexShader **ppVertexShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateGeometryShader( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11GeometryShader **ppGeometryShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateGeometryShaderWithStreamOutput( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
		UINT NumEntries,
		const UINT *pBufferStrides,
		UINT NumStrides,
		UINT RasterizedStream,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11GeometryShader **ppGeometryShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePixelShader( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11PixelShader **ppPixelShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateHullShader( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11HullShader **ppHullShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDomainShader( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11DomainShader **ppDomainShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateComputeShader( 
		const void *pShaderBytecode,
		SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage,
		ID3D11ComputeShader **ppComputeShader));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateClassLinkage( 
		ID3D11ClassLinkage **ppLinkage));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateBlendState( 
		const D3D11_BLEND_DESC *pBlendStateDesc,
		ID3D11BlendState **ppBlendState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDepthStencilState( 
		const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
		ID3D11DepthStencilState **ppDepthStencilState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRasterizerState( 
		const D3D11_RASTERIZER_DESC *pRasterizerDesc,
		ID3D11RasterizerState **ppRasterizerState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateSamplerState( 
		const D3D11_SAMPLER_DESC *pSamplerDesc,
		ID3D11SamplerState **ppSamplerState));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateQuery( 
		const D3D11_QUERY_DESC *pQueryDesc,
		ID3D11Query **ppQuery));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePredicate( 
		const D3D11_QUERY_DESC *pPredicateDesc,
		ID3D11Predicate **ppPredicate));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCounter( 
		const D3D11_COUNTER_DESC *pCounterDesc,
		ID3D11Counter **ppCounter));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDeferredContext( 
		UINT ContextFlags,
		ID3D11DeviceContext **ppDeferredContext));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedResource( 
		HANDLE hResource,
		REFIID ReturnedInterface,
		void **ppResource));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckFormatSupport( 
		DXGI_FORMAT Format,
		UINT *pFormatSupport));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckMultisampleQualityLevels( 
		DXGI_FORMAT Format,
		UINT SampleCount,
		UINT *pNumQualityLevels));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CheckCounterInfo( 
		D3D11_COUNTER_INFO *pCounterInfo));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckCounter( 
		const D3D11_COUNTER_DESC *pDesc,
		D3D11_COUNTER_TYPE *pType,
		UINT *pActiveCounters,
		LPSTR szName,
		UINT *pNameLength,
		LPSTR szUnits,
		UINT *pUnitsLength,
		LPSTR szDescription,
		UINT *pDescriptionLength));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckFeatureSupport( 
		D3D11_FEATURE Feature,
		void *pFeatureSupportData,
		UINT FeatureSupportDataSize));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetPrivateData( 
		REFGUID guid,
		UINT *pDataSize,
		void *pData));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetPrivateData( 
		REFGUID guid,
		UINT DataSize,
		const void *pData));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetPrivateDataInterface( 
		REFGUID guid,
		const IUnknown *pData));

	IMPLEMENT_FUNCTION_SERIALISED(virtual D3D_FEATURE_LEVEL STDMETHODCALLTYPE, GetFeatureLevel( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetCreationFlags( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetDeviceRemovedReason( void));

	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GetImmediateContext( 
		ID3D11DeviceContext **ppImmediateContext));

	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetExceptionMode( 
		UINT RaiseFlags));

	IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetExceptionMode( void));

#if defined(INCLUDE_D3D_11_1)
	//////////////////////////////
	// implement ID3D11Device1
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext));
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDeferredContext1( 
			UINT ContextFlags,
			ID3D11DeviceContext1 **ppDeferredContext));
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateBlendState1( 
			const D3D11_BLEND_DESC1 *pBlendStateDesc,
			ID3D11BlendState1 **ppBlendState));
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRasterizerState1( 
			const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
			ID3D11RasterizerState1 **ppRasterizerState));
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDeviceContextState( 
			UINT Flags,
			const D3D_FEATURE_LEVEL *pFeatureLevels,
			UINT FeatureLevels,
			UINT SDKVersion,
			REFIID EmulatedInterface,
			D3D_FEATURE_LEVEL *pChosenFeatureLevel,
			ID3DDeviceContextState **ppContextState));
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedResource1( 
			HANDLE hResource,
			REFIID returnedInterface,
			void **ppResource));
	
	IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedResourceByName( 
			LPCWSTR lpName,
			DWORD dwDesiredAccess,
			REFIID returnedInterface,
			void **ppResource));
	
	//////////////////////////////
	// implement ID3D11Device2

	virtual void STDMETHODCALLTYPE GetImmediateContext2( 
			ID3D11DeviceContext2 **ppImmediateContext);
	
	virtual HRESULT STDMETHODCALLTYPE CreateDeferredContext2( 
			UINT ContextFlags,
			ID3D11DeviceContext2 **ppDeferredContext);
	
	virtual void STDMETHODCALLTYPE GetResourceTiling( 
			ID3D11Resource *pTiledResource,
			UINT *pNumTilesForEntireResource,
			D3D11_PACKED_MIP_DESC *pPackedMipDesc,
			D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
			UINT *pNumSubresourceTilings,
			UINT FirstSubresourceTilingToGet,
			D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips);
	
	virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels1( 
			DXGI_FORMAT Format,
			UINT SampleCount,
			UINT Flags,
			UINT *pNumQualityLevels);
#endif
};
