/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include "maths/formatpacking.h"

#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_context.h"

#include "jpeg-compressor/jpge.h"

const char *D3D11ChunkNames[] =
{
	"ID3D11Device::Initialisation",
	"ID3D11Resource::SetDebugName",
	"ID3D11Resource::Release",
	"IDXGISwapChain::GetBuffer",
	"ID3D11Device::CreateTexture1D",
	"ID3D11Device::CreateTexture2D",
	"ID3D11Device::CreateTexture3D",
	"ID3D11Device::CreateBuffer",
	"ID3D11Device::CreateVertexShader",
	"ID3D11Device::CreateHullShader",
	"ID3D11Device::CreateDomainShader",
	"ID3D11Device::CreateGeometryShader",
	"ID3D11Device::CreateGeometryShaderWithStreamOut",
	"ID3D11Device::CreatePixelShader",
	"ID3D11Device::CreateComputeShader",
	"ID3D11ClassLinkage::GetClassInstance",
	"ID3D11ClassLinkage::CreateClassInstance",
	"ID3D11Device::CreateClassLinkage",
	"ID3D11Device::CreateShaderResourceView",
	"ID3D11Device::CreateRenderTargetView",
	"ID3D11Device::CreateDepthStencilView",
	"ID3D11Device::CreateUnorderedAccessView",
	"ID3D11Device::CreateInputLayout",
	"ID3D11Device::CreateBlendState",
	"ID3D11Device::CreateDepthStencilState",
	"ID3D11Device::CreateRasterizerState",
	"ID3D11Device::CreateSamplerState",
	"ID3D11Device::CreateQuery",
	"ID3D11Device::CreatePredicate",
	"ID3D11Device::CreateCounter",
	"ID3D11Device::CreateDeferredContext",
	"ID3D11Device::SetExceptionMode",
	"ID3D11Device::OpenSharedResource",

	"Capture",

	"ID3D11DeviceContext::IASetInputLayout",
	"ID3D11DeviceContext::IASetVertexBuffers",
	"ID3D11DeviceContext::IASetIndexBuffer",
	"ID3D11DeviceContext::IASetPrimitiveTopology",
	
	"ID3D11DeviceContext::VSSetConstantBuffers",
	"ID3D11DeviceContext::VSSetShaderResources",
	"ID3D11DeviceContext::VSSetSamplers",
	"ID3D11DeviceContext::VSSetShader",
	
	"ID3D11DeviceContext::HSSetConstantBuffers",
	"ID3D11DeviceContext::HSSetShaderResources",
	"ID3D11DeviceContext::HSSetSamplers",
	"ID3D11DeviceContext::HSSetShader",
	
	"ID3D11DeviceContext::DSSetConstantBuffers",
	"ID3D11DeviceContext::DSSetShaderResources",
	"ID3D11DeviceContext::DSSetSamplers",
	"ID3D11DeviceContext::DSSetShader",
	
	"ID3D11DeviceContext::GSSetConstantBuffers",
	"ID3D11DeviceContext::GSSetShaderResources",
	"ID3D11DeviceContext::GSSetSamplers",
	"ID3D11DeviceContext::GSSetShader",
	
	"ID3D11DeviceContext::SOSetTargets",
	
	"ID3D11DeviceContext::PSSetConstantBuffers",
	"ID3D11DeviceContext::PSSetShaderResources",
	"ID3D11DeviceContext::PSSetSamplers",
	"ID3D11DeviceContext::PSSetShader",
	
	"ID3D11DeviceContext::CSSetConstantBuffers",
	"ID3D11DeviceContext::CSSetShaderResources",
	"ID3D11DeviceContext::CSSetUnorderedAccessViews",
	"ID3D11DeviceContext::CSSetSamplers",
	"ID3D11DeviceContext::CSSetShader",

	"ID3D11DeviceContext::RSSetViewports",
	"ID3D11DeviceContext::RSSetScissors",
	"ID3D11DeviceContext::RSSetState",
	
	"ID3D11DeviceContext::OMSetRenderTargets",
	"ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews",
	"ID3D11DeviceContext::OMSetBlendState",
	"ID3D11DeviceContext::OMSetDepthStencilState",
	
	"ID3D11DeviceContext::DrawIndexedInstanced",
	"ID3D11DeviceContext::DrawInstanced",
	"ID3D11DeviceContext::DrawIndexed",
	"ID3D11DeviceContext::Draw",
	"ID3D11DeviceContext::DrawAuto",
	"ID3D11DeviceContext::DrawIndexedInstancedIndirect",
	"ID3D11DeviceContext::DrawInstancedIndirect",

	"ID3D11DeviceContext::Map",
	"ID3D11DeviceContext::Unmap",
	
	"ID3D11DeviceContext::CopySubresourceRegion",
	"ID3D11DeviceContext::CopyResource",
	"ID3D11DeviceContext::UpdateSubresource",
	"ID3D11DeviceContext::CopyStructureCount",
	"ID3D11DeviceContext::ResolveSubresource",
	"ID3D11DeviceContext::GenerateMips",

	"ID3D11DeviceContext::ClearDepthStencilView",
	"ID3D11DeviceContext::ClearRenderTargetView",
	"ID3D11DeviceContext::ClearUnorderedAccessViewInt",
	"ID3D11DeviceContext::ClearUnorderedAccessViewFloat",
	"ID3D11DeviceContext::ClearState",
	
	"ID3D11DeviceContext::ExecuteCommandList",
	"ID3D11DeviceContext::Dispatch",
	"ID3D11DeviceContext::DispatchIndirect",
	"ID3D11DeviceContext::FinishCommandlist",
	"ID3D11DeviceContext::Flush",
	
	"ID3D11DeviceContext::SetPredication",
	"ID3D11DeviceContext::SetResourceMinLOD",

	"ID3D11DeviceContext::Begin",
	"ID3D11DeviceContext::End",
	
	"ID3D11Device1::CreateRasterizerState1",
	"ID3D11Device1::CreateBlendState1",
	
	"ID3D11DeviceContext1::CopySubresourceRegion1",
	"ID3D11DeviceContext1::UpdateSubresource1",
	"ID3D11DeviceContext1::ClearView",
	
	"ID3D11DeviceContext1::VSSetConstantBuffers1",
	"ID3D11DeviceContext1::HSSetConstantBuffers1",
	"ID3D11DeviceContext1::DSSetConstantBuffers1",
	"ID3D11DeviceContext1::GSSetConstantBuffers1",
	"ID3D11DeviceContext1::PSSetConstantBuffers1",
	"ID3D11DeviceContext1::CSSetConstantBuffers1",

	"D3DPERF_PushMarker",
	"D3DPERF_SetMarker",
	"D3DPERF_PopMarker",

	"DebugMessageList",

	"ContextBegin",
	"ContextEnd",

	"SetShaderDebugPath",
};

WRAPPED_POOL_INST(WrappedID3D11Device);

WrappedID3D11Device *WrappedID3D11Device::m_pCurrentWrappedDevice = NULL;

D3D11InitParams::D3D11InitParams()
{
	SerialiseVersion = D3D11_SERIALISE_VERSION;
	DriverType = D3D_DRIVER_TYPE_UNKNOWN;
	Flags = 0;
	SDKVersion = D3D11_SDK_VERSION;
	NumFeatureLevels = 0;
	RDCEraseEl(FeatureLevels);
}

// handling for these versions is scattered throughout the code (as relevant to enable/disable bits of serialisation
// and set some defaults if necessary).
// Here we list which non-current versions we support, and what changed
const uint32_t D3D11InitParams::D3D11_OLD_VERSIONS[D3D11InitParams::D3D11_NUM_SUPPORTED_OLD_VERSIONS] = {
	0x000004, // from 0x4 to 0x5, we added the stream-out hidden counters in the context's Serialise_BeginCaptureFrame
	0x000005, // from 0x5 to 0x6, several new calls were made 'drawcalls', like Copy & GenerateMips, with serialised debug messages
	0x000006, // from 0x6 to 0x7, we added some more padding in some buffer & texture chunks to get larger alignment than 16-byte
	0x000007, // from 0x7 to 0x8, we changed the UAV arrays in the render state to be D3D11.1 sized and separate CS array.
};

ReplayCreateStatus D3D11InitParams::Serialise()
{
	SERIALISE_ELEMENT(uint32_t, ver, D3D11_SERIALISE_VERSION); SerialiseVersion = ver;

	if(ver != D3D11_SERIALISE_VERSION)
	{
		bool oldsupported = false;
		for(uint32_t i=0; i < D3D11_NUM_SUPPORTED_OLD_VERSIONS; i++)
		{
			if(ver == D3D11_OLD_VERSIONS[i])
			{
				oldsupported = true;
				RDCWARN("Old D3D11 serialise version %d, latest is %d. Loading with possibly degraded features/support.", ver, D3D11_SERIALISE_VERSION);
			}
		}

		if(!oldsupported)
		{
			RDCERR("Incompatible D3D11 serialise version, expected %d got %d", D3D11_SERIALISE_VERSION, ver);
			return eReplayCreate_APIIncompatibleVersion;
		}
	}

	SERIALISE_ELEMENT(D3D_DRIVER_TYPE, driverType, DriverType); DriverType = driverType;
	SERIALISE_ELEMENT(uint32_t, flags, Flags); Flags = flags;
	SERIALISE_ELEMENT(uint32_t, sdk, SDKVersion); SDKVersion = sdk;
	SERIALISE_ELEMENT(uint32_t, numlevels, NumFeatureLevels); NumFeatureLevels = numlevels;
	m_pSerialiser->SerialisePODArray<ARRAY_COUNT(FeatureLevels)>("FeatureLevels", FeatureLevels);

	return eReplayCreate_Success;
}

void WrappedID3D11Device::SetLogFile(const char *logfile)
{
#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	m_pSerialiser = new Serialiser(logfile, Serialiser::READING, debugSerialiser);
	m_pSerialiser->SetChunkNameLookup(&GetChunkName);
	m_pImmediateContext->SetSerialiser(m_pSerialiser);

	SAFE_DELETE(m_ResourceManager);
	m_ResourceManager = new D3D11ResourceManager(m_State, m_pSerialiser, this);
}

WrappedID3D11Device::WrappedID3D11Device(ID3D11Device* realDevice, D3D11InitParams *params)
	: m_RefCounter(realDevice, false), m_SoftRefCounter(NULL, false), m_pDevice(realDevice)
{
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D11Device));

	m_pDevice1 = NULL;
	m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&m_pDevice1);

	m_pDevice2 = NULL;
	m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&m_pDevice2);

	m_Replay.SetDevice(this);

	m_DebugManager = NULL;

	// refcounters implicitly construct with one reference, but we don't start with any soft
	// references.
	m_SoftRefCounter.Release();
	m_InternalRefcount = 0;
	m_Alive = true;

	m_DummyInfoQueue.m_pDevice = this;
	m_WrappedDebug.m_pDevice = this;

	m_FrameCounter = 0;
	m_FailedFrame = 0;
	m_FailedReason = CaptureSucceeded;
	m_Failures = 0;

	m_FrameTimer.Restart();

	m_AppControlledCapture = false;

	m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;
	
#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	if(RenderDoc::Inst().IsReplayApp())
	{
		m_State = READING;
		m_pSerialiser = NULL;

		ResourceIDGen::SetReplayResourceIDs();
	}
	else
	{
		m_State = WRITING_IDLE;
		m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
	}
	
	m_ResourceManager = new D3D11ResourceManager(m_State, m_pSerialiser, this);

	if(m_pSerialiser)
		m_pSerialiser->SetChunkNameLookup(&GetChunkName);
	
	// create a temporary and grab its resource ID
	m_ResourceID = ResourceIDGen::GetNewUniqueID();

	m_DeviceRecord = NULL;

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_ResourceID);
		m_DeviceRecord->DataInSerialiser = false;
		m_DeviceRecord->SpecialResource = true;
		m_DeviceRecord->Length = 0;
		m_DeviceRecord->NumSubResources = 0;
		m_DeviceRecord->SubResources = NULL;

		RenderDoc::Inst().AddDeviceFrameCapturer((ID3D11Device *)this, this);
	}
	
	ID3D11DeviceContext *context = NULL;
	realDevice->GetImmediateContext(&context);

	m_pImmediateContext = new WrappedID3D11DeviceContext(this, context, m_pSerialiser);

	realDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&m_pInfoQueue);
	realDevice->QueryInterface(__uuidof(ID3D11Debug), (void **)&m_WrappedDebug.m_pDebug);

	if(m_pInfoQueue)
	{
		if(RenderDoc::Inst().GetCaptureOptions().DebugOutputMute)
			m_pInfoQueue->SetMuteDebugOutput(true);

		UINT size = m_pInfoQueue->GetStorageFilterStackSize();

		while(size > 1)
		{
			m_pInfoQueue->ClearStorageFilter();
			size = m_pInfoQueue->GetStorageFilterStackSize();
		}

		size = m_pInfoQueue->GetRetrievalFilterStackSize();

		while(size > 1)
		{
			m_pInfoQueue->ClearRetrievalFilter();
			size = m_pInfoQueue->GetRetrievalFilterStackSize();
		}

		m_pInfoQueue->ClearStoredMessages();

		if(RenderDoc::Inst().IsReplayApp())
			m_pInfoQueue->SetMuteDebugOutput(false);
	}
	else
	{
		RDCDEBUG("Couldn't get ID3D11InfoQueue.");
	}

	m_InitParams = *params;

	SetContextFilter(ResourceId(), 0, 0);

	// ATI workaround - these dlls can get unloaded and cause a crash.
	
	if(GetModuleHandleA("aticfx32.dll"))
		LoadLibraryA("aticfx32.dll");
	if(GetModuleHandleA("atiuxpag.dll"))
		LoadLibraryA("atiuxpag.dll");
	if(GetModuleHandleA("atidxx32.dll"))
		LoadLibraryA("atidxx32.dll");

	if(GetModuleHandleA("aticfx64.dll"))
		LoadLibraryA("aticfx64.dll");
	if(GetModuleHandleA("atiuxp64.dll"))
		LoadLibraryA("atiuxp64.dll");
	if(GetModuleHandleA("atidxx64.dll"))
		LoadLibraryA("atidxx64.dll");

	// NVIDIA workaround - same as above!

	if(GetModuleHandleA("nvwgf2umx.dll"))
		LoadLibraryA("nvwgf2umx.dll");

	//////////////////////////////////////////////////////////////////////////
	// Compile time asserts

	RDCCOMPILE_ASSERT(ARRAY_COUNT(D3D11ChunkNames) == NUM_D3D11_CHUNKS-FIRST_CHUNK_ID, "Not right number of chunk names");
}

WrappedID3D11Device::~WrappedID3D11Device()
{
	if(m_pCurrentWrappedDevice == this)
		m_pCurrentWrappedDevice = NULL;

	RenderDoc::Inst().RemoveDeviceFrameCapturer((ID3D11Device *)this);

	for(auto it = m_CachedStateObjects.begin(); it != m_CachedStateObjects.end(); ++it)
		if(*it)
			(*it)->Release();

	m_CachedStateObjects.clear();

	SAFE_RELEASE(m_pDevice1);
	SAFE_RELEASE(m_pDevice2);
	
	SAFE_RELEASE(m_pImmediateContext);

	for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
		SAFE_RELEASE(it->second);
	
	SAFE_DELETE(m_DebugManager);
	
	if(m_DeviceRecord)
	{
		RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
		m_DeviceRecord->Delete(GetResourceManager());
	}

	for(auto it = m_LayoutShaders.begin(); it != m_LayoutShaders.end(); ++it)
		SAFE_DELETE(it->second);
	m_LayoutShaders.clear();
	m_LayoutDescs.clear();

	m_ResourceManager->Shutdown();

	SAFE_DELETE(m_ResourceManager);

	SAFE_RELEASE(m_pInfoQueue);
	SAFE_RELEASE(m_WrappedDebug.m_pDebug);
	SAFE_RELEASE(m_pDevice);

	SAFE_DELETE(m_pSerialiser);
	
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void WrappedID3D11Device::CheckForDeath()
{
	if(!m_Alive) return;

	if(m_RefCounter.GetRefCount() == 0)
	{
		RDCASSERT(m_SoftRefCounter.GetRefCount() >= m_InternalRefcount);

		if(m_SoftRefCounter.GetRefCount() <= m_InternalRefcount || m_State < WRITING) // MEGA HACK
		{
			m_Alive = false;
			delete this;
		}
	}
}

ULONG STDMETHODCALLTYPE DummyID3D11InfoQueue::AddRef()
{
	m_pDevice->AddRef();
	return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D11InfoQueue::Release()
{
	m_pDevice->Release();
	return 1;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11Debug::QueryInterface(REFIID riid, void **ppvObject)
{
	if(riid == __uuidof(ID3D11InfoQueue)
		 || riid == __uuidof(ID3D11Debug)
		 || riid == __uuidof(ID3D11Device)
		 || riid == __uuidof(ID3D11Device1)
		 || riid == __uuidof(ID3D11Device2)
		 )
		return m_pDevice->QueryInterface(riid, ppvObject);

	if(riid == __uuidof(IUnknown))
	{
		*ppvObject = (IUnknown *)(ID3D11Debug *)this;
		AddRef();
		return S_OK;
	}

	string guid = ToStr::Get(riid);
	RDCWARN("Querying ID3D11Debug for interface: %s", guid.c_str());

	return m_pDebug->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedID3D11Debug::AddRef()
{
	m_pDevice->AddRef();
	return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D11Debug::Release()
{
	m_pDevice->Release();
	return 1;
}

HRESULT WrappedID3D11Device::QueryInterface(REFIID riid, void **ppvObject)
{
	// DEFINE_GUID(IID_IDirect3DDevice9, 0xd0223b96, 0xbf7a, 0x43fd, 0x92, 0xbd, 0xa4, 0x3b, 0xd, 0x82, 0xb9, 0xeb);
	static const GUID IDirect3DDevice9_uuid = { 0xd0223b96, 0xbf7a, 0x43fd, { 0x92, 0xbd, 0xa4, 0x3b, 0xd, 0x82, 0xb9, 0xeb } };

	// ID3D10Device UUID {9B7E4C0F-342C-4106-A19F-4F2704F689F0}
	static const GUID ID3D10Device_uuid = { 0x9b7e4c0f, 0x342c, 0x4106, { 0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0 } };

	// ID3D11ShaderTraceFactory UUID {1fbad429-66ab-41cc-9617-667ac10e4459}
	static const GUID ID3D11ShaderTraceFactory_uuid = { 0x1fbad429, 0x66ab, 0x41cc, { 0x96, 0x17, 0x66, 0x7a, 0xc1, 0x0e, 0x44, 0x59 } };

	// RenderDoc UUID {A7AA6116-9C8D-4BBA-9083-B4D816B71B78}
	static const GUID IRenderDoc_uuid = { 0xa7aa6116, 0x9c8d, 0x4bba, { 0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78 } };

	HRESULT hr = S_OK;

	if(riid == __uuidof(IUnknown))
	{
		*ppvObject = (IUnknown *)(ID3D11Device2 *)this;
		AddRef();
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice))
	{
		hr = m_pDevice->QueryInterface(riid, ppvObject);

		if(SUCCEEDED(hr))
		{
			IDXGIDevice *real = (IDXGIDevice *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice(real, this);
			return S_OK;
		}
		else
		{
			*ppvObject = NULL;
			return hr;
		}
	}
	else if(riid == __uuidof(IDXGIDevice1))
	{
		hr = m_pDevice->QueryInterface(riid, ppvObject);
		
		if(SUCCEEDED(hr))
		{
			IDXGIDevice1 *real = (IDXGIDevice1 *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice1(real, this);
			return S_OK;
		}
		else
		{
			*ppvObject = NULL;
			return hr;
		}
	}
	else if(riid == __uuidof(IDXGIDevice2))
	{
		hr = m_pDevice->QueryInterface(riid, ppvObject);
		
		if(SUCCEEDED(hr))
		{
			IDXGIDevice2 *real = (IDXGIDevice2 *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice2(real, this);
			return S_OK;
		}
		else
		{
			*ppvObject = NULL;
			return hr;
		}
	}
	else if(riid == __uuidof(IDXGIDevice3))
	{
		hr = m_pDevice->QueryInterface(riid, ppvObject);
		
		if(SUCCEEDED(hr))
		{
			IDXGIDevice3 *real = (IDXGIDevice3 *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice3(real, this);
			return S_OK;
		}
		else
		{
			*ppvObject = NULL;
			return hr;
		}
	}
	else if(riid == __uuidof(ID3D11Device))
	{
		AddRef();
		*ppvObject = (ID3D11Device *)this;
		return S_OK;
	}
	else if(riid == ID3D10Device_uuid)
	{
		RDCWARN("Trying to get ID3D10Device - not supported.");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	else if(riid == IDirect3DDevice9_uuid)
	{
		RDCWARN("Trying to get IDirect3DDevice9 - not supported.");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	else if(riid == __uuidof(ID3D11Device1))
	{
		if(m_pDevice1)
		{
			AddRef();
			*ppvObject = (ID3D11Device1 *)this;
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
	else if(riid == __uuidof(ID3D11Device2))
	{
		if(m_pDevice2)
		{
			AddRef();
			*ppvObject = (ID3D11Device2 *)this;
			RDCWARN("Trying to get ID3D11Device2. DX11.2 tiled resources are not supported at this time.");
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
	else if(riid == ID3D11ShaderTraceFactory_uuid)
	{
		RDCWARN("Trying to get ID3D11ShaderTraceFactory. Not supported at this time.");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	else if(riid == __uuidof(ID3D11InfoQueue))
	{
		RDCWARN("Returning a dummy ID3D11InfoQueue that does nothing. This ID3D11InfoQueue will not work!");
		*ppvObject = (ID3D11InfoQueue *)&m_DummyInfoQueue;
		m_DummyInfoQueue.AddRef();
		return S_OK;
	}
	else if(riid == __uuidof(ID3D11Debug))
	{
		// we queryinterface for this at startup, so if it's present we can
		// return our wrapper
		if(m_WrappedDebug.m_pDebug)
		{
			AddRef();
			*ppvObject = (ID3D11Debug *)&m_WrappedDebug;
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
	else if(riid == IRenderDoc_uuid)
	{
		AddRef();
		*ppvObject = static_cast<IUnknown*>(this);
		return S_OK;
	}
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying ID3D11Device for interface: %s", guid.c_str());
	}

	return m_RefCounter.QueryInterface(riid, ppvObject);
}

const char *WrappedID3D11Device::GetChunkName(uint32_t idx)
{
	if(idx == CREATE_PARAMS) return "Create Params";
	if(idx == THUMBNAIL_DATA) return "Thumbnail Data";
	if(idx == DRIVER_INIT_PARAMS) return "Driver Init Params";
	if(idx == INITIAL_CONTENTS) return "Initial Contents";
	if(idx < FIRST_CHUNK_ID || idx >= NUM_D3D11_CHUNKS)
		return "<unknown>";
	return D3D11ChunkNames[idx-FIRST_CHUNK_ID];
}

template<>
string ToStrHelper<false, D3D11ChunkType>::Get(const D3D11ChunkType &el)
{
	return WrappedID3D11Device::GetChunkName(el);
}

void WrappedID3D11Device::LazyInit()
{
	if(m_DebugManager == NULL)
		m_DebugManager = new D3D11DebugManager(this);
}

void WrappedID3D11Device::AddDebugMessage(DebugMessageCategory c, DebugMessageSeverity sv, DebugMessageSource src, std::string d)
{
	// Only add runtime warnings while executing.
	// While reading, add the messages from the log, and while writing add messages
	// we add (on top of the API debug messages)
	if(m_State != EXECUTING || src == eDbgSource_RuntimeWarning)
	{
		DebugMessage msg;
		msg.eventID = m_State >= WRITING ? 0 : m_pImmediateContext->GetEventID();
		msg.messageID = 0;
		msg.source = src;
		msg.category = c;
		msg.severity = sv;
		msg.description = d;
		m_DebugMessages.push_back(msg);
	}
}

void WrappedID3D11Device::AddDebugMessage(DebugMessage msg)
{
	if(m_State != EXECUTING || msg.source == eDbgSource_RuntimeWarning)
		m_DebugMessages.push_back(msg);
}

vector<DebugMessage> WrappedID3D11Device::GetDebugMessages()
{
	vector<DebugMessage> ret;
	
	// if reading, m_DebugMessages will contain all the messages (we
	// don't try and fetch anything from the API). If writing,
	// m_DebugMessages will contain any manually-added messages.
	ret.swap(m_DebugMessages);
	
	if(m_State < WRITING)
		return ret;

	if(!m_pInfoQueue)
		return ret;

	UINT64 numMessages = m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

	for(UINT64 i=0; i < m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter(); i++)
	{
		SIZE_T len = 0;
		m_pInfoQueue->GetMessage(i, NULL, &len);

		char *msgbuf = new char[len];
		D3D11_MESSAGE *message = (D3D11_MESSAGE *)msgbuf;

		m_pInfoQueue->GetMessage(i, message, &len);

		DebugMessage msg;
		msg.eventID = 0;
		msg.source = eDbgSource_API;
		msg.category = eDbgCategory_Miscellaneous;
		msg.severity = eDbgSeverity_Medium;

		switch(message->Category)
		{
			case D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED:
				msg.category = eDbgCategory_Application_Defined;
				break;
			case D3D11_MESSAGE_CATEGORY_MISCELLANEOUS:
				msg.category = eDbgCategory_Miscellaneous;
				break;
			case D3D11_MESSAGE_CATEGORY_INITIALIZATION:
				msg.category = eDbgCategory_Initialization;
				break;
			case D3D11_MESSAGE_CATEGORY_CLEANUP:
				msg.category = eDbgCategory_Cleanup;
				break;
			case D3D11_MESSAGE_CATEGORY_COMPILATION:
				msg.category = eDbgCategory_Compilation;
				break;
			case D3D11_MESSAGE_CATEGORY_STATE_CREATION:
				msg.category = eDbgCategory_State_Creation;
				break;
			case D3D11_MESSAGE_CATEGORY_STATE_SETTING:
				msg.category = eDbgCategory_State_Setting;
				break;
			case D3D11_MESSAGE_CATEGORY_STATE_GETTING:
				msg.category = eDbgCategory_State_Getting;
				break;
			case D3D11_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
				msg.category = eDbgCategory_Resource_Manipulation;
				break;
			case D3D11_MESSAGE_CATEGORY_EXECUTION:
				msg.category = eDbgCategory_Execution;
				break;
			case D3D11_MESSAGE_CATEGORY_SHADER:
				msg.category = eDbgCategory_Shaders;
				break;
			default:
				RDCWARN("Unexpected message category: %d", message->Category);
				break;
		}

		switch(message->Severity)
		{
			case D3D11_MESSAGE_SEVERITY_CORRUPTION:
				msg.severity = eDbgSeverity_High;
				break;
			case D3D11_MESSAGE_SEVERITY_ERROR:
				msg.severity = eDbgSeverity_Medium;
				break;
			case D3D11_MESSAGE_SEVERITY_WARNING:
				msg.severity = eDbgSeverity_Low;
				break;
			case D3D11_MESSAGE_SEVERITY_INFO:
				msg.severity = eDbgSeverity_Info;
				break;
			case D3D11_MESSAGE_SEVERITY_MESSAGE:
				msg.severity = eDbgSeverity_Info;
				break;
			default:
				RDCWARN("Unexpected message severity: %d", message->Severity);
				break;
		}

		msg.messageID = (uint32_t)message->ID;
		msg.description = string(message->pDescription);

		ret.push_back(msg);

		SAFE_DELETE_ARRAY(msgbuf);
	}

	// Docs are fuzzy on the thread safety of the info queue, but I'm going to assume it should only
	// ever be accessed on one thread since it's tied to the device & immediate context.
	// There doesn't seem to be a way to lock it for access and without that there's no way to know
	// that a new message won't be added between the time you retrieve the last one and clearing the
	// queue. There is also no way to pop a message that I can see, which would presumably be the
	// best way if its member functions are thread safe themselves (if the queue is protected internally).
	RDCASSERT(numMessages == m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter());

	m_pInfoQueue->ClearStoredMessages();

	return ret;
}

void WrappedID3D11Device::ProcessChunk(uint64_t offset, D3D11ChunkType context)
{
	switch(context)
	{
	case DEVICE_INIT:
		{
			SERIALISE_ELEMENT(ResourceId, immContextId, ResourceId());

			// add a reference for the resource manager - normally it takes ownership of the resource on creation and releases it
			// to destruction, but we want to control our immediate context ourselves.
			m_pImmediateContext->AddRef(); 
			m_ResourceManager->AddLiveResource(immContextId, m_pImmediateContext);
			break;
		}
	case SET_RESOURCE_NAME:
		Serialise_SetResourceName(0x0, "");
		break;
	case RELEASE_RESOURCE:
		Serialise_ReleaseResource(0x0);
		break;
	case CREATE_SWAP_BUFFER:
		Serialise_SetSwapChainTexture(0x0, 0x0, 0, 0x0);
		break;
	case CREATE_TEXTURE_1D:
		Serialise_CreateTexture1D(0x0, 0x0, 0x0);
		break;
	case CREATE_TEXTURE_2D:
		Serialise_CreateTexture2D(0x0, 0x0, 0x0);
		break;
	case CREATE_TEXTURE_3D:
		Serialise_CreateTexture3D(0x0, 0x0, 0x0);
		break;
	case CREATE_BUFFER:
		Serialise_CreateBuffer(0x0, 0x0, 0x0);
		break;
	case CREATE_VERTEX_SHADER:
		Serialise_CreateVertexShader(0x0, 0, 0x0, 0x0);
		break;
	case CREATE_HULL_SHADER:
		Serialise_CreateHullShader(0x0, 0, 0x0, 0x0);
		break;
	case CREATE_DOMAIN_SHADER:
		Serialise_CreateDomainShader(0x0, 0, 0x0, 0x0);
		break;
	case CREATE_GEOMETRY_SHADER:
		Serialise_CreateGeometryShader(0x0, 0, 0x0, 0x0);
		break;
	case CREATE_GEOMETRY_SHADER_WITH_SO:
		Serialise_CreateGeometryShaderWithStreamOutput(0x0, 0, 0x0, 0, 0x0, 0, 0, 0x0, 0x0);
		break;
	case CREATE_PIXEL_SHADER:
		Serialise_CreatePixelShader(0x0, 0, 0x0, 0x0);
		break;
	case CREATE_COMPUTE_SHADER:
		Serialise_CreateComputeShader(0x0, 0, 0x0, 0x0);
		break;
	case GET_CLASS_INSTANCE:
		Serialise_GetClassInstance(0x0, 0, 0x0, 0x0);
		break;
	case CREATE_CLASS_INSTANCE:
		Serialise_CreateClassInstance(0x0, 0, 0, 0, 0, 0x0, 0x0);
		break;
	case CREATE_CLASS_LINKAGE:
		Serialise_CreateClassLinkage(0x0);
		break;
	case CREATE_SRV:
		Serialise_CreateShaderResourceView(0x0, 0x0, 0x0);
		break;
	case CREATE_RTV:
		Serialise_CreateRenderTargetView(0x0, 0x0, 0x0);
		break;
	case CREATE_DSV:
		Serialise_CreateDepthStencilView(0x0, 0x0, 0x0);
		break;
	case CREATE_UAV:
		Serialise_CreateUnorderedAccessView(0x0, 0x0, 0x0);
		break;
	case CREATE_INPUT_LAYOUT:
		Serialise_CreateInputLayout(0x0, 0, 0x0, 0, 0x0);
		break;
	case CREATE_BLEND_STATE:
		Serialise_CreateBlendState(0x0, 0x0);
		break;
	case CREATE_DEPTHSTENCIL_STATE:
		Serialise_CreateDepthStencilState(0x0, 0x0);
		break;
	case CREATE_RASTER_STATE:
		Serialise_CreateRasterizerState(0x0, 0x0);
		break;
	case CREATE_BLEND_STATE1:
		Serialise_CreateBlendState1(0x0, 0x0);
		break;
	case CREATE_RASTER_STATE1:
		Serialise_CreateRasterizerState1(0x0, 0x0);
		break;
	case CREATE_SAMPLER_STATE:
		Serialise_CreateSamplerState(0x0, 0x0);
		break;
	case CREATE_QUERY:
		Serialise_CreateQuery(0x0, 0x0);
		break;
	case CREATE_PREDICATE:
		Serialise_CreatePredicate(0x0, 0x0);
		break;
	case CREATE_COUNTER:
		Serialise_CreateCounter(0x0, 0x0);
		break;
	case CREATE_DEFERRED_CONTEXT:
		Serialise_CreateDeferredContext(0, 0x0);
		break;
	case SET_EXCEPTION_MODE:
		Serialise_SetExceptionMode(0);
		break;
	case OPEN_SHARED_RESOURCE:
	{
		IID nul;
		Serialise_OpenSharedResource(0, nul, NULL);
		break;
	}
	case CAPTURE_SCOPE:
		Serialise_CaptureScope(offset);
		break;
	case SET_SHADER_DEBUG_PATH:
		Serialise_SetShaderDebugPath(NULL, NULL);
		break;
	default:
		// ignore system chunks
		if(context == INITIAL_CONTENTS)
			Serialise_InitialState(ResourceId(), NULL);
		else if(context < FIRST_CHUNK_ID)
			m_pSerialiser->SkipCurrentChunk();
		else
			m_pImmediateContext->ProcessChunk(offset, context, true);
		break;
	}
}

void WrappedID3D11Device::Serialise_CaptureScope(uint64_t offset)
{
	SERIALISE_ELEMENT(uint32_t, FrameNumber, m_FrameCounter);

	if(m_State >= WRITING)
	{
		GetResourceManager()->Serialise_InitialContentsNeeded();
	}
	else
	{
		m_FrameRecord.frameInfo.fileOffset = offset;
		m_FrameRecord.frameInfo.firstEvent = m_pImmediateContext->GetEventID();
		m_FrameRecord.frameInfo.frameNumber = FrameNumber;
		m_FrameRecord.frameInfo.immContextId = GetResourceManager()->GetOriginalID(m_pImmediateContext->GetResourceID());

		FetchFrameStatistics& stats = m_FrameRecord.frameInfo.stats;
		RDCEraseEl(stats);

		// #mivance GL/Vulkan don't set this so don't get stats in window
		stats.recorded = 1;

		for(uint32_t stage = eShaderStage_First; stage < eShaderStage_Count; stage++)
		{
			create_array(stats.constants[stage].bindslots, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT + 1);
			create_array(stats.constants[stage].sizes, FetchFrameConstantBindStats::BUCKET_COUNT);

			create_array(stats.samplers[stage].bindslots, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT + 1);

			create_array(stats.resources[stage].types, eResType_Count);
			create_array(stats.resources[stage].bindslots, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT + 1);
		}

		create_array(stats.updates.types, eResType_Count);
		create_array(stats.updates.sizes, FetchFrameUpdateStats::BUCKET_COUNT);

		create_array(stats.draws.counts, FetchFrameDrawStats::BUCKET_COUNT);

		create_array(stats.vertices.bindslots, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT + 1);

		create_array(stats.rasters.viewports, D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1);
		create_array(stats.rasters.rects, D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 1);

		create_array(stats.outputs.bindslots, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + D3D11_1_UAV_SLOT_COUNT + 1);

		GetResourceManager()->CreateInitialContents();
	}
}

void WrappedID3D11Device::ReadLogInitialisation()
{
	uint64_t frameOffset = 0;

	LazyInit();

	m_pSerialiser->SetDebugText(true);

	m_pSerialiser->Rewind();

	int chunkIdx = 0;

	struct chunkinfo
	{
		chunkinfo() : count(0), totalsize(0), total(0.0) {}
		int count;
		uint64_t totalsize;
		double total;
	};

	map<D3D11ChunkType,chunkinfo> chunkInfos;

	SCOPED_TIMER("chunk initialisation");

	for(;;)
	{
		PerformanceTimer timer;

		uint64_t offset = m_pSerialiser->GetOffset();

		D3D11ChunkType context = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
	
		if(context == CAPTURE_SCOPE)
		{
			// immediately read rest of log into memory
			m_pSerialiser->SetPersistentBlock(offset);
		}

		chunkIdx++;

		ProcessChunk(offset, context);

		m_pSerialiser->PopContext(context);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));

		if(context == CAPTURE_SCOPE)
		{
			frameOffset = offset;

			GetResourceManager()->ApplyInitialContents();

			m_pImmediateContext->ReplayLog(READING, 0, 0, false);
		}

		uint64_t offset2 = m_pSerialiser->GetOffset();

		chunkInfos[context].total += timer.GetMilliseconds();
		chunkInfos[context].totalsize += offset2 - offset;
		chunkInfos[context].count++;

		if(context == CAPTURE_SCOPE)
			break;

		if(m_pSerialiser->AtEnd())
			break;
	}

	SetupDrawcallPointers(&m_Drawcalls, m_FrameRecord.frameInfo.immContextId, m_FrameRecord.drawcallList, NULL, NULL);

#if !defined(RELEASE)
	for(auto it=chunkInfos.begin(); it != chunkInfos.end(); ++it)
	{
		double dcount = double(it->second.count);

		RDCDEBUG("% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
				it->second.count,
				it->second.total, it->second.total/dcount,
				double(it->second.totalsize)/(1024.0*1024.0),
				double(it->second.totalsize)/(dcount*1024.0*1024.0),
				GetChunkName(it->first), uint32_t(it->first)
				);
	}
#endif

	m_FrameRecord.frameInfo.fileSize = m_pSerialiser->GetSize();
	m_FrameRecord.frameInfo.persistentSize = m_pSerialiser->GetSize() - frameOffset;
	m_FrameRecord.frameInfo.initDataSize = chunkInfos[(D3D11ChunkType)INITIAL_CONTENTS].totalsize;

	RDCDEBUG("Allocating %llu persistant bytes of memory for the log.", m_pSerialiser->GetSize() - frameOffset);
	
	m_pSerialiser->SetDebugText(false);
}

bool WrappedID3D11Device::Prepare_InitialState(ID3D11DeviceChild *res)
{
	ResourceType type = IdentifyTypeByPtr(res);
	ResourceId Id = GetIDForResource(res);

	RDCASSERT(m_State >= WRITING);
	
	{
		RDCDEBUG("Prepare_InitialState(%llu)", Id);

		if(type == Resource_Buffer)
			RDCDEBUG("    .. buffer");
		else if(type == Resource_UnorderedAccessView)
			RDCDEBUG("    .. UAV");
		else if(type == Resource_Texture1D ||
				type == Resource_Texture2D ||
				type == Resource_Texture3D)
		{
			if(type == Resource_Texture1D)
				RDCDEBUG("    .. tex1d");
			else if(type == Resource_Texture2D)
				RDCDEBUG("    .. tex2d");
			else if(type == Resource_Texture3D)
				RDCDEBUG("    .. tex3d");
		}
		else
			RDCERR("    .. other!");
	}

	if(type == Resource_UnorderedAccessView)
	{
		WrappedID3D11UnorderedAccessView *uav = (WrappedID3D11UnorderedAccessView *)res;

		D3D11_UNORDERED_ACCESS_VIEW_DESC udesc;
		uav->GetDesc(&udesc);
		
		if(udesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
			(udesc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER|D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
		{
			ID3D11Buffer *stage = NULL;

			D3D11_BUFFER_DESC desc;
			desc.BindFlags = 0;
			desc.ByteWidth = 16;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			desc.Usage = D3D11_USAGE_STAGING;
			HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &stage);

			if(FAILED(hr) || stage == NULL)
			{
				RDCERR("Failed to create staging buffer for UAV initial contents %08x", hr);
			}
			else
			{
				m_pImmediateContext->GetReal()->CopyStructureCount(stage, 0, UNWRAP(WrappedID3D11UnorderedAccessView, uav));

				m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(stage, 0, NULL));
			}
		}
	}
	else if(type == Resource_Buffer)
	{
		WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)res;
		D3D11ResourceRecord *record = m_ResourceManager->GetResourceRecord(Id);

		ID3D11Buffer *stage = NULL;

		D3D11_BUFFER_DESC desc;
		desc.BindFlags = 0;
		desc.ByteWidth = (UINT)record->Length;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.Usage = D3D11_USAGE_STAGING;
		HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &stage);

		if(FAILED(hr) || stage == NULL)
		{
			RDCERR("Failed to create staging buffer for buffer initial contents %08x", hr);
		}
		else
		{
			m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Buffer, buf));

			m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(stage, 0, NULL));
		}
	}
	else if(type == Resource_Texture1D)
	{
		WrappedID3D11Texture1D *tex1D = (WrappedID3D11Texture1D *)res;

		D3D11_TEXTURE1D_DESC desc;
		tex1D->GetDesc(&desc);

		D3D11_TEXTURE1D_DESC stageDesc = desc;
		ID3D11Texture1D *stage = NULL;

		stageDesc.MiscFlags = 0;
		stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stageDesc.BindFlags = 0;
		stageDesc.Usage = D3D11_USAGE_STAGING;

		HRESULT hr = m_pDevice->CreateTexture1D(&stageDesc, NULL, &stage);

		if(FAILED(hr))
		{
			RDCERR("Failed to create initial tex1D %08x", hr);
		}
		else
		{
			m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Texture1D, tex1D));

			m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(stage, 0, NULL));
		}
	}
	else if(type == Resource_Texture2D)
	{
		WrappedID3D11Texture2D *tex2D = (WrappedID3D11Texture2D *)res;

		D3D11_TEXTURE2D_DESC desc;
		tex2D->GetDesc(&desc);

		bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

		D3D11_TEXTURE2D_DESC stageDesc = desc;
		ID3D11Texture2D *stage = NULL;

		stageDesc.MiscFlags = 0;
		stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stageDesc.BindFlags = 0;
		stageDesc.Usage = D3D11_USAGE_STAGING;

		// expand out each sample into an array slice. Hope
		// that this doesn't blow over the array size limit
		// (that would be pretty insane)
		if(multisampled)
		{
			stageDesc.SampleDesc.Count = 1;
			stageDesc.SampleDesc.Quality = 0;
			stageDesc.ArraySize *= desc.SampleDesc.Count;
		}

		HRESULT hr = S_OK;

		hr = m_pDevice->CreateTexture2D(&stageDesc, NULL, &stage);

		if(FAILED(hr))
		{
			RDCERR("Failed to create initial tex2D %08x", hr);
		}
		else
		{
			IDXGIKeyedMutex *mutex = NULL;
			
			if(desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
			{
				hr = UNWRAP(WrappedID3D11Texture2D, tex2D)->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&mutex);

				if(SUCCEEDED(hr) && mutex)
				{
					// complete guess but let's try and acquire key 0 so we can cop this texture out.
					mutex->AcquireSync(0, 10);

					// if it failed, give up. Otherwise we can release the sync below
					if(FAILED(hr))
						SAFE_RELEASE(mutex);
				}
				else
				{
					SAFE_RELEASE(mutex);
				}
			}

			if(multisampled)
				m_DebugManager->CopyTex2DMSToArray(stage, UNWRAP(WrappedID3D11Texture2D, tex2D));
			else
				m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Texture2D, tex2D));

			m_pImmediateContext->GetReal()->Flush();
			
			if(mutex)
			{
				mutex->ReleaseSync(0);

				SAFE_RELEASE(mutex);
			}

			m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(stage, 0, NULL));
		}
	}
	else if(type == Resource_Texture3D)
	{
		WrappedID3D11Texture3D *tex3D = (WrappedID3D11Texture3D *)res;

		D3D11_TEXTURE3D_DESC desc;
		tex3D->GetDesc(&desc);

		D3D11_TEXTURE3D_DESC stageDesc = desc;
		ID3D11Texture3D *stage = NULL;

		stageDesc.MiscFlags = 0;
		stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stageDesc.BindFlags = 0;
		stageDesc.Usage = D3D11_USAGE_STAGING;

		HRESULT hr = m_pDevice->CreateTexture3D(&stageDesc, NULL, &stage);

		if(FAILED(hr))
		{
			RDCERR("Failed to create initial tex3D %08x", hr);
		}
		else
		{
			m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Texture3D, tex3D));

			m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(stage, 0, NULL));
		}
	}

	return true;
}

bool WrappedID3D11Device::Serialise_InitialState(ResourceId resid, ID3D11DeviceChild *res)
{
	ResourceType type = Resource_Unknown;
	ResourceId Id = ResourceId();

	if(m_State >= WRITING)
	{
		type = IdentifyTypeByPtr(res);
		Id = GetIDForResource(res);

		if(type != Resource_Buffer)
		{
			m_pSerialiser->Serialise("type", type);
			m_pSerialiser->Serialise("Id", Id);
		}
	}
	else
	{
		m_pSerialiser->Serialise("type", type);
		m_pSerialiser->Serialise("Id", Id);
	}
	
	{
		RDCDEBUG("Serialise_InitialState(%llu)", Id);

		if(type == Resource_Buffer)
			RDCDEBUG("    .. buffer");
		else if(type == Resource_UnorderedAccessView)
			RDCDEBUG("    .. UAV");
		else if(type == Resource_Texture1D ||
				type == Resource_Texture2D ||
				type == Resource_Texture3D)
		{
			if(type == Resource_Texture1D)
				RDCDEBUG("    .. tex1d");
			else if(type == Resource_Texture2D)
				RDCDEBUG("    .. tex2d");
			else if(type == Resource_Texture3D)
				RDCDEBUG("    .. tex3d");
		}
		else
			RDCERR("    .. other!");
	}

	if(type == Resource_UnorderedAccessView)
	{
		WrappedID3D11UnorderedAccessView *uav = (WrappedID3D11UnorderedAccessView *)res;
		if(m_State < WRITING)
		{
			if(m_ResourceManager->HasLiveResource(Id))
			{
				uav = (WrappedID3D11UnorderedAccessView *)m_ResourceManager->GetLiveResource(Id);
			}
			else
			{
				uav = NULL;
				SERIALISE_ELEMENT(uint32_t, initCount, 0);
				return true;
			}
		}
		
		D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
		uav->GetDesc(&desc);

		if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
			(desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER|D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
		{
			if(m_State >= WRITING)
			{
				ID3D11Buffer *stage = (ID3D11Buffer *)m_ResourceManager->GetInitialContents(Id).resource;

				uint32_t countData = 0;

				if(stage != NULL)
				{
					D3D11_MAPPED_SUBRESOURCE mapped;
					HRESULT hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

					if(FAILED(hr))
					{
						RDCERR("Failed to map while getting initial states %08x", hr);
					}
					else
					{
						countData = *((uint32_t *)mapped.pData);

						m_pImmediateContext->GetReal()->Unmap(stage, 0);
					}
				}

				SERIALISE_ELEMENT(uint32_t, count, countData);
			}
			else
			{
				SERIALISE_ELEMENT(uint32_t, initCount, 0);

				m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(NULL, initCount, NULL));
			}
		}
		else
		{
			SERIALISE_ELEMENT(uint32_t, initCount, 0);
		}
	}
	else if(type == Resource_Buffer)
	{
		if(m_State >= WRITING)
		{
			WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)res;
			D3D11ResourceRecord *record = m_ResourceManager->GetResourceRecord(Id);

			D3D11_BUFFER_DESC desc;
			desc.BindFlags = 0;
			desc.ByteWidth = (UINT)record->Length;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			ID3D11Buffer *stage = (ID3D11Buffer *)m_ResourceManager->GetInitialContents(Id).resource;

			D3D11_MAPPED_SUBRESOURCE mapped;
			HRESULT hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

			if(FAILED(hr))
			{
				RDCERR("Failed to map while getting initial states %08x", hr);
			}
			else
			{
				RDCASSERT(record->DataInSerialiser);

				MapIntercept intercept;
				intercept.SetD3D(mapped);
				intercept.Init(buf, record->GetDataPtr());
				intercept.CopyFromD3D();

				m_pImmediateContext->GetReal()->Unmap(stage, 0);
			}
		}
	}
	else if(type == Resource_Texture1D)
	{
		WrappedID3D11Texture1D *tex1D = (WrappedID3D11Texture1D *)res;
		if(m_State < WRITING && m_ResourceManager->HasLiveResource(Id))
			tex1D = (WrappedID3D11Texture1D *)m_ResourceManager->GetLiveResource(Id);
		
		D3D11ResourceRecord *record = NULL;
		if(m_State >= WRITING)
			record = m_ResourceManager->GetResourceRecord(Id);

		D3D11_TEXTURE1D_DESC desc = {0};
		if(tex1D) tex1D->GetDesc(&desc);
		
		SERIALISE_ELEMENT(uint32_t, numSubresources, desc.MipLevels*desc.ArraySize);
		
		{
			if(m_State < WRITING)
			{
				ID3D11Texture1D *contents = (ID3D11Texture1D *)m_ResourceManager->GetInitialContents(Id).resource;

				RDCASSERT(!contents);
			}

			byte *inmemBuffer = NULL;
			D3D11_SUBRESOURCE_DATA *subData = NULL;

			if(m_State >= WRITING)
			{
				inmemBuffer = new byte[GetByteSize(desc.Width, 1, 1, desc.Format, 0)];
			}
			else if(tex1D)
			{
				subData = new D3D11_SUBRESOURCE_DATA[numSubresources];
			}

			ID3D11Texture1D *stage = (ID3D11Texture1D *)m_ResourceManager->GetInitialContents(Id).resource;

			for(UINT sub = 0; sub < numSubresources; sub++)
			{
				UINT mip = tex1D ? GetMipForSubresource(tex1D, sub) : 0;
				
				if(m_State >= WRITING)
				{
					D3D11_MAPPED_SUBRESOURCE mapped;

					HRESULT hr = m_pImmediateContext->GetReal()->Map(stage, sub, D3D11_MAP_READ, 0, &mapped);

					size_t dstPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

					if(FAILED(hr))
					{
						RDCERR("Failed to map in initial states %08x", hr);
					}
					else
					{
						byte *dst = inmemBuffer;
						byte *src = (byte *)mapped.pData;

						memcpy(dst, src, dstPitch);
					}

					size_t len = dstPitch;
					m_pSerialiser->SerialiseBuffer("", inmemBuffer, len);

					if(SUCCEEDED(hr))
						m_pImmediateContext->GetReal()->Unmap(stage, 0);
				}
				else
				{
					byte *data = NULL;
					size_t len = 0;
					m_pSerialiser->SerialiseBuffer("", data, len);

					if(tex1D)
					{
						subData[sub].pSysMem = data;
						subData[sub].SysMemPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
						subData[sub].SysMemSlicePitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
					}
					else
					{
						SAFE_DELETE_ARRAY(data);
					}
				}
			}

			SAFE_DELETE_ARRAY(inmemBuffer);
			
			if(m_State < WRITING && tex1D)
			{
				// We don't need to bind this, but IMMUTABLE requires at least one
				// BindFlags.
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.Usage = D3D11_USAGE_IMMUTABLE;
				desc.MiscFlags = 0;

				ID3D11Texture1D *contents = NULL;
				HRESULT hr = m_pDevice->CreateTexture1D(&desc, subData, &contents);

				if(FAILED(hr) || contents == NULL)
				{
					RDCERR("Failed to create staging resource for Texture1D initial contents %08x", hr);
				}
				else
				{
					m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(contents, eInitialContents_Copy, NULL));
				}

				for(UINT sub = 0; sub < numSubresources; sub++)
					SAFE_DELETE_ARRAY(subData[sub].pSysMem);
				SAFE_DELETE_ARRAY(subData);
			}
		}
	}
	else if(type == Resource_Texture2D)
	{
		WrappedID3D11Texture2D *tex2D = (WrappedID3D11Texture2D *)res;
		if(m_State < WRITING && m_ResourceManager->HasLiveResource(Id))
			tex2D = (WrappedID3D11Texture2D *)m_ResourceManager->GetLiveResource(Id);
		
		D3D11ResourceRecord *record = NULL;
		if(m_State >= WRITING)
			record = m_ResourceManager->GetResourceRecord(Id);

		D3D11_TEXTURE2D_DESC desc = {0};
		if(tex2D) tex2D->GetDesc(&desc);

		SERIALISE_ELEMENT(uint32_t, numSubresources, desc.MipLevels*desc.ArraySize);

		bool bigrt = (
		               (desc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0 ||
		               (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0 ||
		               (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0
		             ) &&
		             (desc.Width > 64 && desc.Height > 64) &&
		             (desc.Width != desc.Height);

		if(bigrt && m_ResourceManager->ReadBeforeWrite(Id))
			bigrt = false;
		
		bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

		if(multisampled)
			numSubresources *= desc.SampleDesc.Count;

		SERIALISE_ELEMENT(bool, omitted, bigrt && !RenderDoc::Inst().GetCaptureOptions().SaveAllInitials);

		if(omitted)
		{
			if(m_State >= WRITING)
			{
				RDCWARN("Not serialising texture 2D initial state. ID %llu", Id);
				if(bigrt)
					RDCWARN("Detected Write before Read of this target - assuming initial contents are unneeded.\n" \
							"Capture again with Save All Initials if this is wrong");
			}
		}
		else
		{
			if(m_State < WRITING)
			{
				ID3D11Texture2D *contents = (ID3D11Texture2D *)m_ResourceManager->GetInitialContents(Id).resource;

				RDCASSERT(!contents);
			}

			byte *inmemBuffer = NULL;
			D3D11_SUBRESOURCE_DATA *subData = NULL;

			if(m_State >= WRITING)
			{
				inmemBuffer = new byte[GetByteSize(desc.Width, desc.Height, 1, desc.Format, 0)];
			}
			else if(tex2D)
			{
				subData = new D3D11_SUBRESOURCE_DATA[numSubresources];
			}

			ID3D11Texture2D *stage = (ID3D11Texture2D *)m_ResourceManager->GetInitialContents(Id).resource;

			for(UINT sub = 0; sub < numSubresources; sub++)
			{
				UINT mip = tex2D ? GetMipForSubresource(tex2D, sub) : 0;
				
				if(m_State >= WRITING)
				{
					D3D11_MAPPED_SUBRESOURCE mapped;

					HRESULT hr = m_pImmediateContext->GetReal()->Map(stage, sub, D3D11_MAP_READ, 0, &mapped);
					
					size_t dstPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
					size_t len = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

					uint32_t rowsPerLine = 1;
					if(IsBlockFormat(desc.Format))
						rowsPerLine = 4;
					
					if(FAILED(hr))
					{
						RDCERR("Failed to map in initial states %08x", hr);
					}
					else
					{
						byte *dst = inmemBuffer;
						byte *src = (byte *)mapped.pData;
						for(uint32_t row=0; row < desc.Height>>mip; row += rowsPerLine)
						{
							memcpy(dst, src, dstPitch);
							dst += dstPitch;
							src += mapped.RowPitch;
						}
					}

					m_pSerialiser->SerialiseBuffer("", inmemBuffer, len);

					m_pImmediateContext->GetReal()->Unmap(stage, sub);
				}
				else
				{
					byte *data = NULL;
					size_t len = 0;
					m_pSerialiser->SerialiseBuffer("", data, len);

					if(tex2D)
					{
						subData[sub].pSysMem = data;
						subData[sub].SysMemPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
						subData[sub].SysMemSlicePitch = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);
					}
					else
					{
						SAFE_DELETE_ARRAY(data);
					}
				}
			}

			SAFE_DELETE_ARRAY(inmemBuffer);
			
			if(m_State < WRITING && tex2D)
			{
				// We don't need to bind this, but IMMUTABLE requires at least one
				// BindFlags.
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				switch(desc.Format)
				{
					case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
						desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
						break;
					case DXGI_FORMAT_D32_FLOAT:
						desc.Format = DXGI_FORMAT_R32_FLOAT;
						break;
					case DXGI_FORMAT_D24_UNORM_S8_UINT:
						desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
						break;
					case DXGI_FORMAT_D16_UNORM:
						desc.Format = DXGI_FORMAT_R16_FLOAT;
						break;
					default:
						break;
				}

				D3D11_TEXTURE2D_DESC initialDesc = desc;
				// if multisampled, need to upload subData into an array with slices for each sample.
				if(multisampled)
				{
					initialDesc.SampleDesc.Count = 1;
					initialDesc.SampleDesc.Quality = 0;
					initialDesc.ArraySize *= desc.SampleDesc.Count;
				}
				
				initialDesc.Usage = D3D11_USAGE_IMMUTABLE;

				HRESULT hr = S_OK;

				ID3D11Texture2D *contents = NULL;
				hr = m_pDevice->CreateTexture2D(&initialDesc, subData, &contents);

				if(FAILED(hr) || contents == NULL)
				{
					RDCERR("Failed to create staging resource for Texture2D initial contents %08x", hr);
				}
				else
				{
					// if multisampled, contents is actually an array with slices for each sample.
					// need to copy back out to a real multisampled resource
					if(multisampled)
					{
						desc.BindFlags = IsDepthFormat(desc.Format) ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;

						if(IsDepthFormat(desc.Format))
							desc.Format = GetDepthTypedFormat(desc.Format);

						ID3D11Texture2D *contentsMS = NULL;
						hr = m_pDevice->CreateTexture2D(&desc, NULL, &contentsMS);
						
						m_DebugManager->CopyArrayToTex2DMS(contentsMS, contents);

						SAFE_RELEASE(contents);
						contents = contentsMS;
					}

					m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(contents, eInitialContents_Copy, NULL));
				}
				
				for(UINT sub = 0; sub < numSubresources; sub++)
					SAFE_DELETE_ARRAY(subData[sub].pSysMem);
				SAFE_DELETE_ARRAY(subData);
			}
		}
	}
	else if(type == Resource_Texture3D)
	{
		WrappedID3D11Texture3D *tex3D = (WrappedID3D11Texture3D *)res;
		if(m_State < WRITING && m_ResourceManager->HasLiveResource(Id))
			tex3D = (WrappedID3D11Texture3D *)m_ResourceManager->GetLiveResource(Id);
		
		D3D11ResourceRecord *record = NULL;
		if(m_State >= WRITING)
			record = m_ResourceManager->GetResourceRecord(Id);

		D3D11_TEXTURE3D_DESC desc = {0};
		if(tex3D) tex3D->GetDesc(&desc);
		
		SERIALISE_ELEMENT(uint32_t, numSubresources, desc.MipLevels);
		
		{
			if(m_State < WRITING)
			{
				ID3D11Texture3D *contents = (ID3D11Texture3D *)m_ResourceManager->GetInitialContents(Id).resource;

				RDCASSERT(!contents);
			}

			byte *inmemBuffer = NULL;
			D3D11_SUBRESOURCE_DATA *subData = NULL;

			if(m_State >= WRITING)
			{
				inmemBuffer = new byte[GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, 0)];
			}
			else if(tex3D)
			{
				subData = new D3D11_SUBRESOURCE_DATA[numSubresources];
			}

			ID3D11Texture3D *stage = (ID3D11Texture3D *)m_ResourceManager->GetInitialContents(Id).resource;

			for(UINT sub = 0; sub < numSubresources; sub++)
			{
				UINT mip = tex3D ? GetMipForSubresource(tex3D, sub) : 0;
				
				if(m_State >= WRITING)
				{
					D3D11_MAPPED_SUBRESOURCE mapped;

					HRESULT hr = m_pImmediateContext->GetReal()->Map(stage, sub, D3D11_MAP_READ, 0, &mapped);
					
					size_t dstPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
					size_t dstSlicePitch = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

					uint32_t rowsPerLine = 1;
					if(IsBlockFormat(desc.Format))
						rowsPerLine = 4;
					
					if(FAILED(hr))
					{
						RDCERR("Failed to map in initial states %08x", hr);
					}
					else
					{
						byte *dst = inmemBuffer;
						byte *src = (byte *)mapped.pData;

						for(uint32_t slice=0; slice < RDCMAX(1U,desc.Depth>>mip); slice++)
						{
							byte *sliceDst = dst;
							byte *sliceSrc = src;

							for(uint32_t row=0; row < RDCMAX(1U,desc.Height>>mip); row += rowsPerLine)
							{
								memcpy(sliceDst, sliceSrc, dstPitch);
								sliceDst += dstPitch;
								sliceSrc += mapped.RowPitch;
							}

							dst += dstSlicePitch;
							src += mapped.DepthPitch;
						}
					}

					size_t len = dstSlicePitch*desc.Depth;
					m_pSerialiser->SerialiseBuffer("", inmemBuffer, len);

					m_pImmediateContext->GetReal()->Unmap(stage, 0);
				}
				else
				{
					byte *data = NULL;
					size_t len = 0;
					m_pSerialiser->SerialiseBuffer("", data, len);

					if(tex3D)
					{
						subData[sub].pSysMem = data;
						subData[sub].SysMemPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
						subData[sub].SysMemSlicePitch = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);
					}
					else
					{
						SAFE_DELETE_ARRAY(data);
					}
				}
			}

			SAFE_DELETE_ARRAY(inmemBuffer);
			
			if(m_State < WRITING && tex3D)
			{
				// We don't need to bind this, but IMMUTABLE requires at least one
				// BindFlags.
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.Usage = D3D11_USAGE_IMMUTABLE;
				desc.MiscFlags = 0;

				ID3D11Texture3D *contents = NULL;
				HRESULT hr = m_pDevice->CreateTexture3D(&desc, subData, &contents);

				if(FAILED(hr) || contents == NULL)
				{
					RDCERR("Failed to create staging resource for Texture3D initial contents %08x", hr);
				}
				else
				{
					m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(contents, eInitialContents_Copy, NULL));
				}
				
				for(UINT sub = 0; sub < numSubresources; sub++)
					SAFE_DELETE_ARRAY(subData[sub].pSysMem);
				SAFE_DELETE_ARRAY(subData);
			}
		}
	}
	else
	{
		RDCERR("Trying to serialise initial state of unsupported resource type");
	}

	return true;
}

void WrappedID3D11Device::Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData)
{
	ResourceType type = IdentifyTypeByPtr(live);
	
	{
		RDCDEBUG("Create_InitialState(%llu)", id);

		if(type == Resource_Buffer)
			RDCDEBUG("    .. buffer");
		else if(type == Resource_UnorderedAccessView)
			RDCDEBUG("    .. UAV");
		else if(type == Resource_Texture1D ||
				type == Resource_Texture2D ||
				type == Resource_Texture3D)
		{
			if(type == Resource_Texture1D)
				RDCDEBUG("    .. tex1d");
			else if(type == Resource_Texture2D)
				RDCDEBUG("    .. tex2d");
			else if(type == Resource_Texture3D)
				RDCDEBUG("    .. tex3d");
		}
		else
			RDCERR("    .. other!");
	}

	if(type == Resource_UnorderedAccessView)
	{
		WrappedID3D11UnorderedAccessView *uav = (WrappedID3D11UnorderedAccessView *)live;

		D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
		uav->GetDesc(&desc);

		if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
			(desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER|D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
		{
			ID3D11Buffer *stage = NULL;

			D3D11_BUFFER_DESC bdesc;
			bdesc.BindFlags = 0;
			bdesc.ByteWidth = 16;
			bdesc.MiscFlags = 0;
			bdesc.StructureByteStride = 0;
			bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			bdesc.Usage = D3D11_USAGE_STAGING;
			HRESULT hr = m_pDevice->CreateBuffer(&bdesc, NULL, &stage);

			if(FAILED(hr) || stage == NULL)
			{
				RDCERR("Failed to create staging resource for UAV initial contents %08x", hr);
			}
			else
			{
				m_pImmediateContext->GetReal()->CopyStructureCount(stage, 0, UNWRAP(WrappedID3D11UnorderedAccessView, uav));

				D3D11_MAPPED_SUBRESOURCE mapped;
				hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

				uint32_t countData = 0;

				if(FAILED(hr))
				{
					RDCERR("Failed to map while creating initial states %08x", hr);
				}
				else
				{
					countData = *((uint32_t *)mapped.pData);

					m_pImmediateContext->GetReal()->Unmap(stage, 0);
				}

				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(NULL, countData, NULL));

				SAFE_RELEASE(stage);
			}
		}
	}
	else if(type == Resource_Texture1D)
	{
		WrappedID3D11Texture1D *tex1D = (WrappedID3D11Texture1D *)live;

		D3D11_TEXTURE1D_DESC desc;
		tex1D->GetDesc(&desc);

		if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_RENDER_TARGET))
		{
			D3D11_RENDER_TARGET_VIEW_DESC rdesc;
			rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
			rdesc.Format = GetTypedFormat(desc.Format);
			rdesc.Texture1D.MipSlice = 0;

			ID3D11RenderTargetView *initContents = NULL;

			HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture1D, tex1D), &rdesc, &initContents);

			if(FAILED(hr))
			{
				RDCERR("Failed to create fast-clear RTV while creating initial states %08x", hr);
			}
			else
			{
				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_ClearRTV, NULL));
			}
		}
		else if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC ddesc;
			ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
			ddesc.Format = GetDepthTypedFormat(desc.Format);
			ddesc.Texture1D.MipSlice = 0;
			ddesc.Flags = 0;

			ID3D11DepthStencilView *initContents = NULL;

			HRESULT hr = m_pDevice->CreateDepthStencilView(UNWRAP(WrappedID3D11Texture1D, tex1D), &ddesc, &initContents);

			if(FAILED(hr))
			{
				RDCERR("Failed to create fast-clear DSV while creating initial states %08x", hr);
			}
			else
			{
				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_ClearDSV, NULL));
			}
		}
		else if(desc.Usage != D3D11_USAGE_IMMUTABLE)
		{
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = 0;
			if(IsDepthFormat(desc.Format))
				desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

			ID3D11Texture1D *initContents = NULL;

			HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &initContents);

			if(FAILED(hr))
			{
				RDCERR("Failed to create tex3D while creating initial states %08x", hr);
			}
			else
			{
				m_pImmediateContext->GetReal()->CopyResource(initContents, UNWRAP(WrappedID3D11Texture1D, tex1D));

				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_Copy, NULL));
			}
		}
	}
	else if(type == Resource_Texture2D)
	{
		WrappedID3D11Texture2D *tex2D = (WrappedID3D11Texture2D *)live;

		D3D11_TEXTURE2D_DESC desc;
		tex2D->GetDesc(&desc);

		bool isMS = (desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0);
		
		if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_RENDER_TARGET))
		{
			D3D11_RENDER_TARGET_VIEW_DESC rdesc;
			rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			rdesc.Format = GetTypedFormat(desc.Format);
			rdesc.Texture2D.MipSlice = 0;

			if(isMS) rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

			ID3D11RenderTargetView *initContents = NULL;

			HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D, tex2D), &rdesc, &initContents);

			if(FAILED(hr))
			{	
				RDCERR("Failed to create fast-clear RTV while creating initial states %08x", hr);
			}
			else
			{
				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_ClearRTV, NULL));
			}
		}
		else if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC ddesc;
			ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			ddesc.Format = GetDepthTypedFormat(desc.Format);
			ddesc.Texture1D.MipSlice = 0;
			ddesc.Flags = 0;

			if(isMS) ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

			ID3D11DepthStencilView *initContents = NULL;

			HRESULT hr = m_pDevice->CreateDepthStencilView(UNWRAP(WrappedID3D11Texture2D, tex2D), &ddesc, &initContents);

			if(FAILED(hr))
			{
				RDCERR("Failed to create fast-clear DSV while creating initial states %08x", hr);
			}
			else
			{
				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_ClearDSV, NULL));
			}
		}
		else if(desc.Usage != D3D11_USAGE_IMMUTABLE)
		{
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = isMS ? D3D11_BIND_SHADER_RESOURCE : 0;
			if(IsDepthFormat(desc.Format))
				desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

			ID3D11Texture2D *initContents = NULL;

			HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &initContents);

			if(FAILED(hr))
			{	
				RDCERR("Failed to create tex2D while creating initial states %08x", hr);
			}
			else
			{
				m_pImmediateContext->GetReal()->CopyResource(initContents, UNWRAP(WrappedID3D11Texture2D, tex2D));

				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_Copy, NULL));
			}
		}
	}
	else if(type == Resource_Texture3D)
	{
		WrappedID3D11Texture3D *tex3D = (WrappedID3D11Texture3D *)live;

		D3D11_TEXTURE3D_DESC desc;
		tex3D->GetDesc(&desc);
			
		if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_RENDER_TARGET))
		{
			D3D11_RENDER_TARGET_VIEW_DESC rdesc;
			rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
			rdesc.Format = GetTypedFormat(desc.Format);
			rdesc.Texture3D.FirstWSlice = 0;
			rdesc.Texture3D.MipSlice = 0;
			rdesc.Texture3D.WSize = desc.Depth;

			ID3D11RenderTargetView *initContents = NULL;

			HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture3D, tex3D), &rdesc, &initContents);

			if(FAILED(hr))
			{
				RDCERR("Failed to create fast-clear RTV while creating initial states %08x", hr);
			}
			else
			{
				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_ClearRTV, NULL));
			}
		}
		else if(!hasData && desc.Usage != D3D11_USAGE_IMMUTABLE)
		{
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = 0;
			desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

			ID3D11Texture3D *initContents = NULL;

			HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &initContents);

			if(FAILED(hr))
			{
				RDCERR("Failed to create tex3D while creating initial states %08x", hr);
			}
			else
			{
				m_pImmediateContext->GetReal()->CopyResource(initContents, UNWRAP(WrappedID3D11Texture3D, tex3D));

				m_ResourceManager->SetInitialContents(id, D3D11ResourceManager::InitialContentData(initContents, eInitialContents_Copy, NULL));
			}
		}
	}
}

void WrappedID3D11Device::Apply_InitialState(ID3D11DeviceChild *live, D3D11ResourceManager::InitialContentData initial)
{
	ResourceType type = IdentifyTypeByPtr(live);

	if(type == Resource_UnorderedAccessView)
	{
		ID3D11UnorderedAccessView *uav = (ID3D11UnorderedAccessView *)live;

		m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &uav, &initial.num);
	}
	else
	{
		if(initial.num == eInitialContents_ClearRTV)
		{
			float emptyCol[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_pImmediateContext->GetReal()->ClearRenderTargetView((ID3D11RenderTargetView *)initial.resource, emptyCol);
		}
		else if(initial.num == eInitialContents_ClearDSV)
		{
			m_pImmediateContext->GetReal()->ClearDepthStencilView((ID3D11DepthStencilView *)initial.resource, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
		}
		else if(initial.num == eInitialContents_Copy)
		{
			ID3D11Resource *liveResource = (ID3D11Resource *)m_ResourceManager->UnwrapResource(live);
			ID3D11Resource *initialResource = (ID3D11Resource *)initial.resource;

			m_pImmediateContext->GetReal()->CopyResource(liveResource, initialResource);
		}
		else
		{
			RDCERR("Unexpected initial contents type");
		}
	}
}

void WrappedID3D11Device::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	m_ReplayDefCtx = id;
	m_FirstDefEv = firstDefEv;
	m_LastDefEv = lastDefEv;
}

void WrappedID3D11Device::ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	uint64_t offs = m_FrameRecord.frameInfo.fileOffset;

	m_pSerialiser->SetOffset(offs);

	bool partial = true;

	if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
	{
		startEventID = m_FrameRecord.frameInfo.firstEvent;
		partial = false;
	}
	
	D3D11ChunkType header = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

	RDCASSERTEQUAL(header, CAPTURE_SCOPE);

	m_pSerialiser->SkipCurrentChunk();

	m_pSerialiser->PopContext(header);
	
	if(!partial)
	{
		GetResourceManager()->ApplyInitialContents();
		GetResourceManager()->ReleaseInFrameResources();
	}

	m_State = EXECUTING;
	
	if(m_ReplayDefCtx == ResourceId())
	{
		if(replayType == eReplay_Full)
			m_pImmediateContext->ReplayLog(EXECUTING, startEventID, endEventID, partial);
		else if(replayType == eReplay_WithoutDraw)
			m_pImmediateContext->ReplayLog(EXECUTING, startEventID, RDCMAX(1U,endEventID)-1, partial);
		else if(replayType == eReplay_OnlyDraw)
			m_pImmediateContext->ReplayLog(EXECUTING, endEventID, endEventID, partial);
		else
			RDCFATAL("Unexpected replay type");
	}
	else
	{
		if(replayType == eReplay_Full || replayType == eReplay_WithoutDraw)
		{
			m_pImmediateContext->ReplayLog(EXECUTING, startEventID, endEventID, partial);
		}

		m_pSerialiser->SetOffset(offs);
		
		header = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
		m_pSerialiser->SkipCurrentChunk();
		m_pSerialiser->PopContext(header);

		m_pImmediateContext->ReplayFakeContext(m_ReplayDefCtx);
		
		if(replayType == eReplay_Full)
		{
			m_pImmediateContext->ClearState();

			m_pImmediateContext->ReplayLog(EXECUTING, m_FirstDefEv, m_LastDefEv, true);
		}
		else if(replayType == eReplay_WithoutDraw && m_LastDefEv-1 >= m_FirstDefEv)
		{
			m_pImmediateContext->ClearState();

			m_pImmediateContext->ReplayLog(EXECUTING, m_FirstDefEv, RDCMAX(m_LastDefEv,1U)-1, true);
		}
		else if(replayType == eReplay_OnlyDraw)
		{
			m_pImmediateContext->ReplayLog(EXECUTING, m_LastDefEv, m_LastDefEv, true);
		}

		m_pImmediateContext->ReplayFakeContext(ResourceId());
	}
}

void WrappedID3D11Device::ReleaseSwapchainResources(IDXGISwapChain *swap)
{
	if(swap)
	{
		DXGI_SWAP_CHAIN_DESC desc;
		swap->GetDesc(&desc);

		Keyboard::RemoveInputWindow(desc.OutputWindow);

		RenderDoc::Inst().RemoveFrameCapturer((ID3D11Device *)this, desc.OutputWindow);
	}

	auto it = m_SwapChains.find(swap);
	if(it != m_SwapChains.end())
	{
		SAFE_RELEASE(it->second);
		m_SwapChains.erase(it);
	}
}

bool WrappedID3D11Device::Serialise_SetSwapChainTexture(IDXGISwapChain *swap, DXGI_SWAP_CHAIN_DESC *swapDesc, UINT buffer, ID3D11Texture2D *pTex)
{
	SERIALISE_ELEMENT(DXGI_FORMAT, swapFormat, swapDesc->BufferDesc.Format);
	SERIALISE_ELEMENT(uint32_t, BuffNum, buffer);
	SERIALISE_ELEMENT(ResourceId, pTexture, GetIDForResource(pTex));

	if(m_State >= WRITING)
	{
		D3D11_TEXTURE2D_DESC desc;

		pTex->GetDesc(&desc);

		SERIALISE_ELEMENT(D3D11_TEXTURE2D_DESC, Descriptor, desc);
	}
	else
	{
		ID3D11Texture2D *fakeBB;

		SERIALISE_ELEMENT(D3D11_TEXTURE2D_DESC, Descriptor, D3D11_TEXTURE2D_DESC());

		D3D11_TEXTURE2D_DESC realDescriptor = Descriptor;

		// DXGI swap chain back buffers can be freely cast as a special-case.
		// translate the format to a typeless format to allow for this.
		// the original type will be stored in the texture below
		Descriptor.Format = GetTypelessFormat(Descriptor.Format);

		HRESULT hr = m_pDevice->CreateTexture2D(&Descriptor, NULL, &fakeBB);

		if(FAILED(hr))
		{
			RDCERR("Failed to create fake back buffer, HRESULT: 0x%08x", hr);
		}
		else
		{
			WrappedID3D11Texture2D *wrapped = new WrappedID3D11Texture2D(fakeBB, this, TEXDISPLAY_INDIRECT_VIEW);
			fakeBB = wrapped;

			wrapped->m_RealDescriptor = new D3D11_TEXTURE2D_DESC(realDescriptor);

			SetDebugName(fakeBB, "Serialised Swap Chain Buffer");

			GetResourceManager()->AddLiveResource(pTexture, fakeBB);
		}
	}

	return true;
}

void WrappedID3D11Device::SetSwapChainTexture(IDXGISwapChain *swap, DXGI_SWAP_CHAIN_DESC *swapDesc, UINT buffer, ID3D11Texture2D *pTex)
{
	D3D11_TEXTURE2D_DESC desc;
	pTex->GetDesc(&desc);
	
	ResourceId id = GetIDForResource(pTex);

	LazyInit();
	
	// there shouldn't be a resource record for this texture as it wasn't created via
	// CreateTexture2D
	RDCASSERT(id != ResourceId() && !GetResourceManager()->HasResourceRecord(id));

	if(m_State >= WRITING)
	{
		D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
		record->DataInSerialiser = false;
		record->SpecialResource = true;
		record->Length = 0;
		record->NumSubResources = 0;
		record->SubResources = NULL;

		SCOPED_LOCK(m_D3DLock);

		SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);

		Serialise_SetSwapChainTexture(swap, swapDesc, buffer, pTex);

		record->AddChunk(scope.Get());
	}
	
	if(buffer == 0 && m_State >= WRITING)
	{
		ID3D11RenderTargetView *rtv = NULL;
		HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D, pTex), NULL, &rtv);

		if(FAILED(hr))
			RDCERR("Couldn't create RTV for swapchain tex %08x", hr);

		m_SwapChains[swap] = rtv;
	}
	
	if(swap)
	{
		DXGI_SWAP_CHAIN_DESC sdesc;
		swap->GetDesc(&sdesc);

		Keyboard::AddInputWindow(sdesc.OutputWindow);

		RenderDoc::Inst().AddFrameCapturer((ID3D11Device *)this, sdesc.OutputWindow, this);
	}
}

void WrappedID3D11Device::SetMarker(uint32_t col, const wchar_t *name)
{
	if(m_pCurrentWrappedDevice == NULL)
		return;

	m_pCurrentWrappedDevice->m_pImmediateContext->ThreadSafe_SetMarker(col, name);
}

int WrappedID3D11Device::BeginEvent(uint32_t col, const wchar_t *name)
{
	if(m_pCurrentWrappedDevice == NULL)
		return 0;

	return m_pCurrentWrappedDevice->m_pImmediateContext->ThreadSafe_BeginEvent(col, name);
}

int WrappedID3D11Device::EndEvent()
{
	if(m_pCurrentWrappedDevice == NULL)
		return 0;

	return m_pCurrentWrappedDevice->m_pImmediateContext->ThreadSafe_EndEvent();
}

void WrappedID3D11Device::StartFrameCapture(void *dev, void *wnd)
{
	if(m_State != WRITING_IDLE) return;

	SCOPED_LOCK(m_D3DLock);

	RenderDoc::Inst().SetCurrentDriver(RDC_D3D11);

	m_State = WRITING_CAPFRAME;

	m_AppControlledCapture = true;

	m_Failures = 0;
	m_FailedFrame = 0;
	m_FailedReason = CaptureSucceeded;
	
	m_FrameCounter = RDCMAX(1+(uint32_t)m_CapturedFrames.size(), m_FrameCounter);
	
	FetchFrameInfo frame;
	frame.frameNumber = m_FrameCounter+1;
	frame.captureTime = Timing::GetUnixTimestamp();
	m_CapturedFrames.push_back(frame);

	m_DebugMessages.clear();

	GetResourceManager()->ClearReferencedResources();

	GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Write);

	m_pImmediateContext->FreeCaptureData();

	m_pImmediateContext->AttemptCapture();
	m_pImmediateContext->BeginCaptureFrame();

	for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
	{
		WrappedID3D11DeviceContext *context = *it;

		if(context)
		{
			context->AttemptCapture();
		}
		else
		{
			RDCERR("NULL deferred context in resource record!");
		}
	}

	GetResourceManager()->PrepareInitialContents();

	if(m_pInfoQueue)
		m_pInfoQueue->ClearStoredMessages();

	RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedID3D11Device::EndFrameCapture(void *dev, void *wnd)
{
	if(m_State != WRITING_CAPFRAME) return true;

	CaptureFailReason reason;

	IDXGISwapChain *swap = NULL;
	
	if(wnd)
	{
		for(auto it=m_SwapChains.begin(); it!=m_SwapChains.end(); ++it)
		{
			DXGI_SWAP_CHAIN_DESC swapDesc;
			it->first->GetDesc(&swapDesc);

			if(swapDesc.OutputWindow == wnd)
			{
				swap = it->first;
				break;
			}
		}

		if(swap == NULL)
		{
			RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
			return false;
		}
	}

	if(m_pImmediateContext->HasSuccessfulCapture(reason))
	{
		SCOPED_LOCK(m_D3DLock);

		RDCLOG("Finished capture, Frame %u", m_FrameCounter);

		m_Failures = 0;
		m_FailedFrame = 0;
		m_FailedReason = CaptureSucceeded;

		m_pImmediateContext->EndCaptureFrame();
		m_pImmediateContext->FinishCapture();

		for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
		{
			WrappedID3D11DeviceContext *context = *it;

			if(context)
			{
				context->FinishCapture();
			}
			else
			{
				RDCERR("NULL deferred context in resource record!");
			}
		}

		const uint32_t maxSize = 1024;

		byte *thpixels = NULL;
		uint32_t thwidth = 0;
		uint32_t thheight = 0;

		if(swap != NULL)
		{
			ID3D11RenderTargetView *rtv = m_SwapChains[swap];

			ID3D11Resource *res = NULL;

			rtv->GetResource(&res); res->Release();

			ID3D11Texture2D *tex = (ID3D11Texture2D *)res;

			D3D11_TEXTURE2D_DESC desc;
			tex->GetDesc(&desc);

			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			desc.MiscFlags = 0;
			desc.Usage = D3D11_USAGE_STAGING;

			bool msaa = (desc.SampleDesc.Count > 1) || (desc.SampleDesc.Quality > 0);

			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;

			ID3D11Texture2D *stagingTex = NULL;

			HRESULT hr = S_OK;

			hr = m_pDevice->CreateTexture2D(&desc, NULL, &stagingTex);

			if(FAILED(hr))
			{
				RDCERR("Couldn't create staging texture to create thumbnail. %08x", hr);
			}
			else
			{
				if(msaa)
				{
					desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					desc.CPUAccessFlags = 0;
					desc.Usage = D3D11_USAGE_DEFAULT;

					ID3D11Texture2D *resolveTex = NULL;

					hr = m_pDevice->CreateTexture2D(&desc, NULL, &resolveTex);

					if(FAILED(hr))
					{
						RDCERR("Couldn't create resolve texture to create thumbnail. %08x", hr);
						tex = NULL;
					}
					else
					{
						m_pImmediateContext->GetReal()->ResolveSubresource(resolveTex, 0, tex, 0, desc.Format);
						m_pImmediateContext->GetReal()->CopyResource(stagingTex, resolveTex);
						resolveTex->Release();
					}
				}
				else
				{
					m_pImmediateContext->GetReal()->CopyResource(stagingTex, tex);
				}

				if(tex)
				{
					ResourceFormat fmt = MakeResourceFormat(desc.Format);

					D3D11_MAPPED_SUBRESOURCE mapped;
					hr = m_pImmediateContext->GetReal()->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mapped);

					if(FAILED(hr))
					{
						RDCERR("Couldn't map staging texture to create thumbnail. %08x", hr);
					}
					else
					{
						byte *data = (byte *)mapped.pData;

						float aspect = float(desc.Width)/float(desc.Height);

						thwidth = RDCMIN(maxSize, desc.Width);
						thwidth &= ~0x7; // align down to multiple of 8
						thheight = uint32_t(float(thwidth)/aspect);

						thpixels = new byte[3*thwidth*thheight];

						float widthf = float(desc.Width);
						float heightf = float(desc.Height);

						uint32_t stride = fmt.compByteWidth*fmt.compCount;

						bool buf1010102 = false;
						bool bufBGRA = (fmt.bgraOrder != false);

						if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
						{
							stride = 4;
							buf1010102 = true;
						}

						byte *dst = thpixels;

						for(uint32_t y=0; y < thheight; y++)
						{
							for(uint32_t x=0; x < thwidth; x++)
							{
								float xf = float(x)/float(thwidth);
								float yf = float(y)/float(thheight);

								byte *src = &data[ stride*uint32_t(xf*widthf) + mapped.RowPitch*uint32_t(yf*heightf) ];

								if(buf1010102)
								{
									uint32_t *src1010102 = (uint32_t *)src;
									Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
									dst[0] = (byte)(unorm.x*255.0f);
									dst[1] = (byte)(unorm.y*255.0f);
									dst[2] = (byte)(unorm.z*255.0f);
								}
								else if(bufBGRA)
								{
									dst[0] = src[2];
									dst[1] = src[1];
									dst[2] = src[0];
								}
								else if(fmt.compByteWidth == 2) // R16G16B16A16 backbuffer
								{
									uint16_t *src16 = (uint16_t *)src;

									float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
									float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
									float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

									if(linearR < 0.0031308f) dst[0] = byte(255.0f*(12.92f * linearR));
									else                     dst[0] = byte(255.0f*(1.055f * powf(linearR, 1.0f/2.4f) - 0.055f));

									if(linearG < 0.0031308f) dst[1] = byte(255.0f*(12.92f * linearG));
									else                     dst[1] = byte(255.0f*(1.055f * powf(linearG, 1.0f/2.4f) - 0.055f));

									if(linearB < 0.0031308f) dst[2] = byte(255.0f*(12.92f * linearB));
									else                     dst[2] = byte(255.0f*(1.055f * powf(linearB, 1.0f/2.4f) - 0.055f));
								}
								else
								{
									dst[0] = src[0];
									dst[1] = src[1];
									dst[2] = src[2];
								}

								dst += 3;
							}
						}

						m_pImmediateContext->GetReal()->Unmap(stagingTex, 0);
					}
				}

				stagingTex->Release();
			}

		}

		byte *jpgbuf = NULL;
		int len = thwidth*thheight;

		if(wnd)
		{
			jpgbuf = new byte[len];

			jpge::params p;

			p.m_quality = 40;

			bool success = jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

			if(!success)
			{
				RDCERR("Failed to compress to jpg");
				SAFE_DELETE_ARRAY(jpgbuf);
				thwidth = 0;
				thheight = 0;
			}
		}

		Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

		SAFE_DELETE_ARRAY(jpgbuf);
		SAFE_DELETE(thpixels);

		{
			SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

			SERIALISE_ELEMENT(ResourceId, immContextId, m_pImmediateContext->GetResourceID());

			m_pFileSerialiser->Insert(scope.Get(true));
		}

		RDCDEBUG("Inserting Resource Serialisers");	

		GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);

		GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

		RDCDEBUG("Creating Capture Scope");	

		{
			SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

			Serialise_CaptureScope(0);

			m_pFileSerialiser->Insert(scope.Get(true));
		}

		{
			RDCDEBUG("Getting Resource Record");	

			D3D11ResourceRecord *record = m_ResourceManager->GetResourceRecord(m_pImmediateContext->GetResourceID());

			RDCDEBUG("Accumulating context resource list");	

			map<int32_t, Chunk *> recordlist;
			record->Insert(recordlist);

			RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());	

			for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
				m_pFileSerialiser->Insert(it->second);

			RDCDEBUG("Done");	
		}

		m_pFileSerialiser->FlushToDisk();

		SAFE_DELETE(m_pFileSerialiser);

		RenderDoc::Inst().SuccessfullyWrittenLog();

		m_State = WRITING_IDLE;

		m_pImmediateContext->CleanupCapture();

		m_pImmediateContext->FreeCaptureData();

		for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
		{
			WrappedID3D11DeviceContext *context = *it;

			if(context)
				context->CleanupCapture();
			else
				RDCERR("NULL deferred context in resource record!");
		}

		GetResourceManager()->MarkUnwrittenResources();

		GetResourceManager()->ClearReferencedResources();

		return true;
	}
	else
	{
		const char *reasonString = "Unknown reason";
		switch(reason)
		{
			case CaptureFailed_UncappedCmdlist: reasonString = "Uncapped command list"; break;
			case CaptureFailed_UncappedUnmap:   reasonString = "Uncapped Map()/Unmap()"; break;
			default: break;
		}

		RDCLOG("Failed to capture, frame %u: %s", m_FrameCounter, reasonString);

		m_Failures++;

		if((RenderDoc::Inst().GetOverlayBits() & eRENDERDOC_Overlay_Enabled) && swap != NULL)
		{
			D3D11RenderState old = *m_pImmediateContext->GetCurrentPipelineState();

			ID3D11RenderTargetView *rtv = m_SwapChains[swap];

			if(rtv)
			{
				m_pImmediateContext->GetReal()->OMSetRenderTargets(1, &rtv, NULL);

				DXGI_SWAP_CHAIN_DESC swapDesc = {0};
				swap->GetDesc(&swapDesc);
				GetDebugManager()->SetOutputDimensions(swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height);
				GetDebugManager()->SetOutputWindow(swapDesc.OutputWindow);

				GetDebugManager()->RenderText(0.0f, 0.0f, "Failed to capture frame %u: %s", m_FrameCounter, reasonString);
			}

			old.ApplyState(m_pImmediateContext);
		}

		m_CapturedFrames.back().frameNumber = m_FrameCounter+1;

		m_pImmediateContext->CleanupCapture();

		for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
		{
			WrappedID3D11DeviceContext *context = *it;

			if(context)
				context->CleanupCapture();
			else
				RDCERR("NULL deferred context in resource record!");
		}

		GetResourceManager()->ClearReferencedResources();

		// if it's a capture triggered from application code, immediately
		// give up as it's not reasonable to expect applications to detect and retry.
		// otherwise we can retry in case the next frame works.
		if(m_Failures > 5 || m_AppControlledCapture)
		{
			m_pImmediateContext->FinishCapture();

			m_CapturedFrames.pop_back();

			for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
			{
				WrappedID3D11DeviceContext *context = *it;

				if(context)
				{
					context->FinishCapture();
				}
				else
				{
					RDCERR("NULL deferred context in resource record!");
				}
			}

			m_pImmediateContext->FreeCaptureData();

			m_FailedFrame = m_FrameCounter;
			m_FailedReason = reason;

			m_State = WRITING_IDLE;

			for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
			{
				WrappedID3D11DeviceContext *context = *it;

				if(context)
					context->CleanupCapture();
				else
					RDCERR("NULL deferred context in resource record!");
			}

			GetResourceManager()->MarkUnwrittenResources();
		}
		else
		{
			GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Write);
			GetResourceManager()->PrepareInitialContents();

			m_pImmediateContext->AttemptCapture();
			m_pImmediateContext->BeginCaptureFrame();

			for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
			{
				WrappedID3D11DeviceContext *context = *it;

				if(context)
				{
					context->AttemptCapture();
				}
				else
				{
					RDCERR("NULL deferred context in resource record!");
				}
			}
		}

		if(m_pInfoQueue)
			m_pInfoQueue->ClearStoredMessages();

		return false;
	}
}

void WrappedID3D11Device::FirstFrame(IDXGISwapChain *swap)
{
	DXGI_SWAP_CHAIN_DESC swapdesc;
	swap->GetDesc(&swapdesc);

	// if we have to capture the first frame, begin capturing immediately
	if(m_State == WRITING_IDLE && RenderDoc::Inst().ShouldTriggerCapture(0))
	{
		RenderDoc::Inst().StartFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);

		m_AppControlledCapture = false;
	}
}

HRESULT WrappedID3D11Device::Present(IDXGISwapChain *swap, UINT SyncInterval, UINT Flags)
{
	if((Flags & DXGI_PRESENT_TEST) != 0)
		return S_OK;
	
	m_pCurrentWrappedDevice = this;
	
	if(m_State == WRITING_IDLE)
		RenderDoc::Inst().Tick();
	
	m_pImmediateContext->EndFrame();

	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame

	m_pImmediateContext->BeginFrame();

	DXGI_SWAP_CHAIN_DESC swapdesc;
	swap->GetDesc(&swapdesc);
	bool activeWindow = RenderDoc::Inst().IsActiveWindow((ID3D11Device *)this, swapdesc.OutputWindow);

	if(m_State == WRITING_IDLE)
	{
		D3D11RenderState old = *m_pImmediateContext->GetCurrentPipelineState();

		m_FrameTimes.push_back(m_FrameTimer.GetMilliseconds());
		m_TotalTime += m_FrameTimes.back();
		m_FrameTimer.Restart();

		// update every second
		if(m_TotalTime > 1000.0)
		{
			m_MinFrametime = 10000.0;
			m_MaxFrametime = 0.0;
			m_AvgFrametime = 0.0;

			m_TotalTime = 0.0;

			for(size_t i=0; i < m_FrameTimes.size(); i++)
			{
				m_AvgFrametime += m_FrameTimes[i];
				if(m_FrameTimes[i] < m_MinFrametime)
					m_MinFrametime = m_FrameTimes[i];
				if(m_FrameTimes[i] > m_MaxFrametime)
					m_MaxFrametime = m_FrameTimes[i];
			}

			m_AvgFrametime /= double(m_FrameTimes.size());

			m_FrameTimes.clear();
		}
		
		uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

		if(overlay & eRENDERDOC_Overlay_Enabled)
		{
			ID3D11RenderTargetView *rtv = m_SwapChains[swap];

			m_pImmediateContext->GetReal()->OMSetRenderTargets(1, &rtv, NULL);

			DXGI_SWAP_CHAIN_DESC swapDesc = {0};
			swap->GetDesc(&swapDesc);
			GetDebugManager()->SetOutputDimensions(swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height);
			GetDebugManager()->SetOutputWindow(swapDesc.OutputWindow);

			if(activeWindow)
			{
				vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetCaptureKeys();

				string overlayText = "D3D11. ";

				for(size_t i=0; i < keys.size(); i++)
				{
					if(i > 0)
						overlayText += ", ";

					overlayText += ToStr::Get(keys[i]);
				}

				if(!keys.empty())
					overlayText += " to capture.";

				if(overlay & eRENDERDOC_Overlay_FrameNumber)
				{
					overlayText += StringFormat::Fmt(" Frame: %d.", m_FrameCounter);
				}
				if(overlay & eRENDERDOC_Overlay_FrameRate)
				{
					overlayText += StringFormat::Fmt(" %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)",
																					m_AvgFrametime, m_MinFrametime, m_MaxFrametime,
																					// max with 0.01ms so that we don't divide by zero
																					1000.0f/RDCMAX(0.01, m_AvgFrametime) );
				}

				float y=0.0f;

				if(!overlayText.empty())
				{
					GetDebugManager()->RenderText(0.0f, y, overlayText.c_str());
					y += 1.0f;
				}

				if(overlay & eRENDERDOC_Overlay_CaptureList)
				{
					GetDebugManager()->RenderText(0.0f, y, "%d Captures saved.\n", (uint32_t)m_CapturedFrames.size());
					y += 1.0f;

					uint64_t now = Timing::GetUnixTimestamp();
					for(size_t i=0; i < m_CapturedFrames.size(); i++)
					{
						if(now - m_CapturedFrames[i].captureTime < 20)
						{
							GetDebugManager()->RenderText(0.0f, y, "Captured frame %d.\n", m_CapturedFrames[i].frameNumber);
							y += 1.0f;
						}
					}
				}

				if(m_FailedFrame > 0)
				{
					const char *reasonString = "Unknown reason";
					switch(m_FailedReason)
					{
					case CaptureFailed_UncappedCmdlist: reasonString = "Uncapped command list"; break;
					case CaptureFailed_UncappedUnmap: reasonString = "Uncapped Map()/Unmap()"; break;
					default: break;
					}

					GetDebugManager()->RenderText(0.0f, y, "Failed capture at frame %d:\n", m_FailedFrame);
					y += 1.0f;
					GetDebugManager()->RenderText(0.0f, y, "    %s\n", reasonString);
					y += 1.0f;
				}

#if !defined(RELEASE)
				GetDebugManager()->RenderText(0.0f, y, "%llu chunks - %.2f MB", Chunk::NumLiveChunks(), float(Chunk::TotalMem())/1024.0f/1024.0f);
				y += 1.0f;
#endif
			}
			else
			{
				vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetFocusKeys();

				string str = "D3D11. Inactive swapchain.";

				for(size_t i=0; i < keys.size(); i++)
				{
					if(i == 0)
						str += " ";
					else
						str += ", ";

					str += ToStr::Get(keys[i]);
				}

				if(!keys.empty())
					str += " to cycle between swapchains";

				GetDebugManager()->RenderText(0.0f, 0.0f, str.c_str());
			}

			old.ApplyState(m_pImmediateContext);
		}
	}

	if(!activeWindow)
		return S_OK;
	
	RenderDoc::Inst().SetCurrentDriver(RDC_D3D11);

	// kill any current capture that isn't application defined
	if(m_State == WRITING_CAPFRAME && !m_AppControlledCapture)
		RenderDoc::Inst().EndFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);

	if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE)
	{
		RenderDoc::Inst().StartFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);

		m_AppControlledCapture = false;
	}

	return S_OK;
}

void WrappedID3D11Device::CachedObjectsGarbageCollect()
{
	// 4000 is a fairly arbitrary number, chosen to make sure this garbage
	// collection kicks in as rarely as possible (4000 is a *lot* of unique
	// state objects to have), while still meaning that we'll never
	// accidentally cause a state object to fail to create because the app
	// expects only N to be alive but we're caching M more causing M+N>4096
	if(m_CachedStateObjects.size() < 4000) return;

	// Now release all purely cached objects that have no external refcounts.
	// This will thrash if we have e.g. 2000 rasterizer state objects, all
	// referenced, and 2000 sampler state objects, all referenced.

	for(auto it=m_CachedStateObjects.begin(); it != m_CachedStateObjects.end();)
	{
		ID3D11DeviceChild *o = *it;

		o->AddRef();
		if(o->Release() == 1)
		{
			auto eraseit = it;
			++it;
			o->Release();
			InternalRelease();
			m_CachedStateObjects.erase(eraseit);
		}
		else
		{
			++it;
		}
	}
}

void WrappedID3D11Device::AddDeferredContext(WrappedID3D11DeviceContext *defctx)
{
	RDCASSERT(m_DeferredContexts.find(defctx) == m_DeferredContexts.end());
	m_DeferredContexts.insert(defctx);
}

void WrappedID3D11Device::RemoveDeferredContext(WrappedID3D11DeviceContext *defctx)
{
	RDCASSERT(m_DeferredContexts.find(defctx) != m_DeferredContexts.end());
	m_DeferredContexts.erase(defctx);
}

bool WrappedID3D11Device::Serialise_SetShaderDebugPath(ID3D11DeviceChild *res, const char *p)
{
	SERIALISE_ELEMENT(ResourceId, resource, GetIDForResource(res));
	string debugPath = p ? p : "";
	m_pSerialiser->Serialise("debugPath", debugPath);

	if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
	{
		auto it = WrappedShader::m_ShaderList.find(GetResourceManager()->GetLiveID(resource));

		if(it != WrappedShader::m_ShaderList.end())
			it->second->SetDebugInfoPath(debugPath);
	}

	return true;
}

HRESULT WrappedID3D11Device::SetShaderDebugPath(ID3D11DeviceChild *res, const char *path)
{
	if(m_State >= WRITING)
	{
		ResourceId idx = GetIDForResource(res);
		D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);

		if(record == NULL)
		{
			RDCERR("Setting shader debug path on object %p of type %d that has no resource record.", res, IdentifyTypeByPtr(res));
			return E_INVALIDARG;
		}

		RDCASSERT(idx != ResourceId());
		
		{
			SCOPED_SERIALISE_CONTEXT(SET_SHADER_DEBUG_PATH);
			Serialise_SetShaderDebugPath(res, path);
			record->AddChunk(scope.Get());
		}

		return S_OK;
	}

	return S_OK;
}

bool WrappedID3D11Device::Serialise_SetResourceName(ID3D11DeviceChild *res, const char *nm)
{
	SERIALISE_ELEMENT(ResourceId, resource, GetIDForResource(res));
	string name = nm ? nm : "";
	m_pSerialiser->Serialise("name", name);

	if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
	{
		ID3D11DeviceChild *r = GetResourceManager()->GetLiveResource(resource);

		SetDebugName(r, name.c_str());
	}

	return true;
}

void WrappedID3D11Device::SetResourceName(ID3D11DeviceChild *res, const char *name)
{
	if(m_State >= WRITING)
	{
		ResourceId idx = GetIDForResource(res);
		D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);

		if(record == NULL)
			record = m_DeviceRecord;

		RDCASSERT(idx != ResourceId());
		
		SCOPED_LOCK(m_D3DLock);
		{
			SCOPED_SERIALISE_CONTEXT(SET_RESOURCE_NAME);

			Serialise_SetResourceName(res, name);
			
			// don't serialise many SetResourceName chunks to the
			// object record, but we can't afford to drop any.
			record->LockChunks();
			while(record->HasChunks())
			{
				Chunk *end = record->GetLastChunk();

				if(end->GetChunkType() == SET_RESOURCE_NAME)
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
	}
}

bool WrappedID3D11Device::Serialise_ReleaseResource(ID3D11DeviceChild *res)
{
	ResourceType resourceType = Resource_Unknown;
	ResourceId resource = GetIDForResource(res);
	
	if(m_State >= WRITING)
	{
		resourceType = IdentifyTypeByPtr(res);
	}

	if(m_State == WRITING_IDLE || m_State < WRITING)
	{
		SERIALISE_ELEMENT(ResourceId, serRes, GetIDForResource(res));
		SERIALISE_ELEMENT(ResourceType, serType, resourceType);

		resourceType = serType;
		resource = serRes;
	}
	
	if(m_State >= WRITING)
	{
		D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(resource);
		if(record)
			record->Delete(m_ResourceManager);
	}
	if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
	{
		res = GetResourceManager()->GetLiveResource(resource);
		GetResourceManager()->EraseLiveResource(resource);
		SAFE_RELEASE(res);
	}

	return true;
}

void WrappedID3D11Device::ReleaseResource(ID3D11DeviceChild *res)
{
	ResourceId idx = GetIDForResource(res);

	// wrapped resources get released all the time, we don't want to
	// try and slerp in a resource release. Just the explicit ones
	if(m_State < WRITING)
	{
		if(GetResourceManager()->HasLiveResource(idx))
			GetResourceManager()->EraseLiveResource(idx);
		return;
	}
	
	SCOPED_LOCK(m_D3DLock);

	ResourceType type = IdentifyTypeByPtr(res);

	D3D11ResourceRecord *record = m_DeviceRecord;

	if(m_State == WRITING_IDLE)
	{
		if(type == Resource_ShaderResourceView ||
			type == Resource_DepthStencilView ||
			type == Resource_UnorderedAccessView ||
			type == Resource_RenderTargetView ||
			type == Resource_Buffer ||
			type == Resource_Texture1D ||
			type == Resource_Texture2D ||
			type == Resource_Texture3D ||
			type == Resource_CommandList)
		{
			record = GetResourceManager()->GetResourceRecord(idx);
			RDCASSERT(record);

			if(record->SpecialResource)
			{
				record = m_DeviceRecord;
			}		
			else if(record->GetRefCount() == 1)
			{
				// we're about to decrement this chunk out of existance!
				// don't hold onto the record to add the chunk.
				record = NULL;
			}
		}
	}

	GetResourceManager()->MarkCleanResource(idx);

	if(type == Resource_DeviceContext)
	{
		RemoveDeferredContext((WrappedID3D11DeviceContext *)res);
	}

	bool serialiseRelease = true;

	WrappedID3D11CommandList *cmdList = (WrappedID3D11CommandList *)res;
	
	// don't serialise releases of counters or queries since we ignore them.
	// Also don't serialise releases of command lists that weren't captured,
	// since their creation won't be in the log either.
	if(type == Resource_Counter || type == Resource_Query ||
	   (type == Resource_CommandList && !cmdList->IsCaptured())
	  )
		serialiseRelease = false;

	if(type == Resource_CommandList && !cmdList->IsCaptured())
	{
		record = GetResourceManager()->GetResourceRecord(idx);
		if(record)
			record->Delete(GetResourceManager());
	}
	
	if(serialiseRelease)
	{
		if(m_State == WRITING_CAPFRAME)
		{
			Serialise_ReleaseResource(res);
		}
		else
		{
			SCOPED_SERIALISE_CONTEXT(RELEASE_RESOURCE);
			Serialise_ReleaseResource(res);

			if(record)
			{
				record->AddChunk(scope.Get());
			}
		}

		if(record == NULL)
		{
			// if record is NULL then we just deleted a reference-less resource.
			// That means it is not used and can be safely discarded, so just
			// throw away the serialiser contents
			m_pSerialiser->Rewind();
		}
	}
}

WrappedID3D11DeviceContext *WrappedID3D11Device::GetDeferredContext( size_t idx )
{
	auto it = m_DeferredContexts.begin();

	for(size_t i=0; i < idx; i++)
	{
		++it;
		if(it == m_DeferredContexts.end())
			return NULL;
	}

	return *it;
}

const FetchDrawcall *WrappedID3D11Device::GetDrawcall(uint32_t eventID)
{
	if(eventID >= m_Drawcalls.size())
		return NULL;

	return m_Drawcalls[eventID];
}