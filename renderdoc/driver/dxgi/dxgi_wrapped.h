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

#include <dxgi.h>
#include <d3d11.h>

// if you don't have the windows 8.1 SDK, remove this define to exclude the DXGI1.2+ functionality
#define INCLUDE_DXGI_1_2

#include "common/common.h"
#include "common/wrapped_pool.h"

class RefCountDXGIObject : public IDXGIObject
{
	IDXGIObject *m_pReal;
	unsigned int m_iRefcount;
public:
	RefCountDXGIObject(IDXGIObject *real) : m_pReal(real), m_iRefcount(1) {}
	virtual ~RefCountDXGIObject() {}
	
	static bool HandleWrap(REFIID riid, void **ppvObject);
	static HRESULT WrapQueryInterface(IUnknown *real, REFIID riid, void **ppvObject);

	//////////////////////////////
	// implement IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [annotation][iid_is][out] */ 
		__RPC__deref_out  void **ppvObject)
	{ return WrapQueryInterface(m_pReal, riid, ppvObject); }

	ULONG STDMETHODCALLTYPE AddRef() { return ++m_iRefcount; }
	ULONG STDMETHODCALLTYPE Release() { ULONG ret = --m_iRefcount; if(m_iRefcount == 0) delete this; return ret; }

	//////////////////////////////
	// implement IDXGIObject

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData( 
		/* [in] */ REFGUID Name,
		/* [in] */ UINT DataSize,
		/* [in] */ const void *pData)
	{ return m_pReal->SetPrivateData(Name, DataSize, pData); }

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface( 
		/* [in] */ REFGUID Name,
		/* [in] */ const IUnknown *pUnknown)
	{ return m_pReal->SetPrivateDataInterface(Name, pUnknown); }

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData( 
		/* [in] */ REFGUID Name,
		/* [out][in] */ UINT *pDataSize,
		/* [out] */ void *pData)
	{ return m_pReal->GetPrivateData(Name, pDataSize, pData); }

	virtual HRESULT STDMETHODCALLTYPE GetParent( 
		/* [in] */ REFIID riid,
		/* [retval][out] */ void **ppParent);
};

#define IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT \
		ULONG STDMETHODCALLTYPE AddRef() { return RefCountDXGIObject::AddRef(); } \
		ULONG STDMETHODCALLTYPE Release() { return RefCountDXGIObject::Release(); } \
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return RefCountDXGIObject::QueryInterface(riid, ppvObject); } \
		HRESULT STDMETHODCALLTYPE SetPrivateData(REFIID Name, UINT DataSize, const void *pData) { return RefCountDXGIObject::SetPrivateData(Name, DataSize, pData); } \
		HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFIID Name, const IUnknown *pUnknown) { return RefCountDXGIObject::SetPrivateDataInterface(Name, pUnknown); } \
		HRESULT STDMETHODCALLTYPE GetPrivateData(REFIID Name, UINT *pDataSize, void *pData) { return RefCountDXGIObject::GetPrivateData(Name, pDataSize, pData); } \
		HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppvObject) { return RefCountDXGIObject::GetParent(riid, ppvObject); }

#define IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY \
		ULONG STDMETHODCALLTYPE AddRef() { return RefCountDXGIObject::AddRef(); } \
		ULONG STDMETHODCALLTYPE Release() { return RefCountDXGIObject::Release(); } \
		HRESULT STDMETHODCALLTYPE SetPrivateData(REFIID Name, UINT DataSize, const void *pData) { return RefCountDXGIObject::SetPrivateData(Name, DataSize, pData); } \
		HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFIID Name, const IUnknown *pUnknown) { return RefCountDXGIObject::SetPrivateDataInterface(Name, pUnknown); } \
		HRESULT STDMETHODCALLTYPE GetPrivateData(REFIID Name, UINT *pDataSize, void *pData) { return RefCountDXGIObject::GetPrivateData(Name, pDataSize, pData); } \
		HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppvObject) { return RefCountDXGIObject::GetParent(riid, ppvObject); }

class WrappedID3D11Device;
struct ID3D11Resource;

#if defined(INCLUDE_DXGI_1_2)
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#endif

#if defined(INCLUDE_DXGI_1_2)
#define SWAPCHAINPARENT IDXGISwapChain2
#else
#define SWAPCHAINPARENT IDXGISwapChain
#endif

class WrappedIDXGISwapChain2 : public SWAPCHAINPARENT, public RefCountDXGIObject
{
	IDXGISwapChain* m_pReal;
#if defined(INCLUDE_DXGI_1_2)
	IDXGISwapChain1* m_pReal1;
	IDXGISwapChain2* m_pReal2;
#endif
	WrappedID3D11Device *m_pDevice;
	unsigned int m_iRefcount;

	HWND m_Wnd;

  static const int MAX_NUM_BACKBUFFERS = 4;

  ID3D11Resource *m_pBackBuffers[MAX_NUM_BACKBUFFERS];
public:
	WrappedIDXGISwapChain2(IDXGISwapChain* real, HWND wnd, WrappedID3D11Device *device);
	virtual ~WrappedIDXGISwapChain2();
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
	
	//////////////////////////////
	// implement IDXGIDeviceSubObject

	virtual HRESULT STDMETHODCALLTYPE GetDevice( 
		/* [in] */ REFIID riid,
		/* [retval][out] */ void **ppDevice);

	//////////////////////////////
	// implement IDXGISwapChain

	virtual HRESULT STDMETHODCALLTYPE Present( 
		/* [in] */ UINT SyncInterval,
		/* [in] */ UINT Flags);

	virtual HRESULT STDMETHODCALLTYPE GetBuffer( 
		/* [in] */ UINT Buffer,
		/* [in] */ REFIID riid,
		/* [out][in] */ void **ppSurface);

	virtual HRESULT STDMETHODCALLTYPE SetFullscreenState( 
		/* [in] */ BOOL Fullscreen,
		/* [in] */ IDXGIOutput *pTarget);

	virtual HRESULT STDMETHODCALLTYPE GetFullscreenState( 
		/* [out] */ BOOL *pFullscreen,
		/* [out] */ IDXGIOutput **ppTarget);

	virtual HRESULT STDMETHODCALLTYPE GetDesc( 
		/* [out] */ DXGI_SWAP_CHAIN_DESC *pDesc)
	{
		return m_pReal->GetDesc(pDesc);
	}

	virtual HRESULT STDMETHODCALLTYPE ResizeBuffers( 
		/* [in] */ UINT BufferCount,
		/* [in] */ UINT Width,
		/* [in] */ UINT Height,
		/* [in] */ DXGI_FORMAT NewFormat,
		/* [in] */ UINT SwapChainFlags);

	virtual HRESULT STDMETHODCALLTYPE ResizeTarget( 
		/* [in] */ const DXGI_MODE_DESC *pNewTargetParameters)
	{
		return m_pReal->ResizeTarget(pNewTargetParameters);
	}

	virtual HRESULT STDMETHODCALLTYPE GetContainingOutput( 
		IDXGIOutput **ppOutput)
	{
		return m_pReal->GetContainingOutput(ppOutput);
	}

	virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics( 
		/* [out] */ DXGI_FRAME_STATISTICS *pStats)
	{
		return m_pReal->GetFrameStatistics(pStats);
	}

	virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount( 
		/* [out] */ UINT *pLastPresentCount)
	{
		return m_pReal->GetLastPresentCount(pLastPresentCount);
	}

#if defined(INCLUDE_DXGI_1_2)
	//////////////////////////////
	// implement IDXGISwapChain1
	
	virtual HRESULT STDMETHODCALLTYPE GetDesc1( 
		/* [annotation][out] */ 
		_Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc)
	{
		return m_pReal2->GetDesc1(pDesc);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetFullscreenDesc( 
		/* [annotation][out] */ 
		_Out_  DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
	{
		return m_pReal2->GetFullscreenDesc(pDesc);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetHwnd( 
		/* [annotation][out] */ 
		_Out_  HWND *pHwnd)
	{
		return m_pReal2->GetHwnd(pHwnd);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetCoreWindow( 
		/* [annotation][in] */ 
		_In_  REFIID refiid,
		/* [annotation][out] */ 
		_Out_  void **ppUnk)
	{
		return m_pReal2->GetCoreWindow(refiid, ppUnk);
	}
	
	virtual HRESULT STDMETHODCALLTYPE Present1( 
		/* [in] */ UINT SyncInterval,
		/* [in] */ UINT PresentFlags,
		/* [annotation][in] */ 
		_In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters);
	
	virtual BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported( void)
	{
		return m_pReal2->IsTemporaryMonoSupported();
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput( 
		/* [annotation][out] */ 
		_Out_  IDXGIOutput **ppRestrictToOutput)
	{
		return m_pReal2->GetRestrictToOutput(ppRestrictToOutput);
	}
	
	virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor( 
		/* [annotation][in] */ 
		_In_  const DXGI_RGBA *pColor)
	{
		return m_pReal2->SetBackgroundColor(pColor);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor( 
		/* [annotation][out] */ 
		_Out_  DXGI_RGBA *pColor)
	{
		return m_pReal2->GetBackgroundColor(pColor);
	}
	
	virtual HRESULT STDMETHODCALLTYPE SetRotation( 
		/* [annotation][in] */ 
		_In_  DXGI_MODE_ROTATION Rotation)
	{
		return m_pReal2->SetRotation(Rotation);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetRotation( 
		/* [annotation][out] */ 
		_Out_  DXGI_MODE_ROTATION *pRotation)
	{
		return m_pReal2->GetRotation(pRotation);
	}

	//////////////////////////////
	// implement IDXGISwapChain2

	virtual HRESULT STDMETHODCALLTYPE SetSourceSize( 
		UINT Width,
		UINT Height)
	{
		return m_pReal2->SetSourceSize(Width, Height);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetSourceSize( 
		/* [annotation][out] */ 
		_Out_  UINT *pWidth,
		/* [annotation][out] */ 
		_Out_  UINT *pHeight)
	{
		return m_pReal2->GetSourceSize(pWidth, pHeight);
	}
	
	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency( 
		UINT MaxLatency)
	{
		return m_pReal2->SetMaximumFrameLatency(MaxLatency);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency( 
		/* [annotation][out] */ 
		_Out_  UINT *pMaxLatency)
	{
		return m_pReal2->GetMaximumFrameLatency(pMaxLatency);
	}
	
	virtual HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject( void)
	{
		return m_pReal2->GetFrameLatencyWaitableObject();
	}
	
	virtual HRESULT STDMETHODCALLTYPE SetMatrixTransform( 
		const DXGI_MATRIX_3X2_F *pMatrix)
	{
		return m_pReal2->SetMatrixTransform(pMatrix);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetMatrixTransform( 
		/* [annotation][out] */ 
		_Out_  DXGI_MATRIX_3X2_F *pMatrix)
	{
		return m_pReal2->GetMatrixTransform(pMatrix);
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////
// Crap classes we don't really care about, except capturing the swap chain

class WrappedIDXGIAdapter : public IDXGIAdapter, public RefCountDXGIObject
{
	IDXGIAdapter* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIAdapter(IDXGIAdapter* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIAdapter() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;

	//////////////////////////////
	// implement IDXGIAdapter

	virtual HRESULT STDMETHODCALLTYPE EnumOutputs( 
		/* [in] */ UINT Output,
		/* [annotation][out][in] */ 
		__out  IDXGIOutput **ppOutput)
	{
		return m_pReal->EnumOutputs(Output, ppOutput);
	}

	virtual HRESULT STDMETHODCALLTYPE GetDesc( 
		/* [annotation][out] */ 
		__out  DXGI_ADAPTER_DESC *pDesc)
	{
		return m_pReal->GetDesc(pDesc);
	}

	virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport( 
		/* [annotation][in] */ 
		__in  REFGUID InterfaceName,
		/* [annotation][out] */ 
		__out  LARGE_INTEGER *pUMDVersion)
	{
		return m_pReal->CheckInterfaceSupport(InterfaceName, pUMDVersion);
	}
};

class WrappedIDXGIDevice : public IDXGIDevice, public RefCountDXGIObject
{
	IDXGIDevice* m_pReal;
	ID3D11Device* m_pD3DDevice;
public:
	WrappedIDXGIDevice(IDXGIDevice* real, ID3D11Device *d3d) :
	  RefCountDXGIObject(real), m_pReal(real), m_pD3DDevice(d3d) { m_pD3DDevice->AddRef(); }

	virtual ~WrappedIDXGIDevice() { SAFE_RELEASE(m_pReal); SAFE_RELEASE(m_pD3DDevice); }
	
	static const public int AllocPoolCount = 4;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedIDXGIDevice, AllocPoolCount);

	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

	ID3D11Device *GetD3DDevice() { return m_pD3DDevice; }
	
	//////////////////////////////
	// implement IDXGIDevice
	
	virtual HRESULT STDMETHODCALLTYPE GetAdapter( 
		/* [annotation][out] */ 
		__out  IDXGIAdapter **pAdapter)
	{
		HRESULT ret = m_pReal->GetAdapter(pAdapter);
		if(SUCCEEDED(ret)) *pAdapter = new WrappedIDXGIAdapter(*pAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSurface( 
		/* [annotation][in] */ 
		__in  const DXGI_SURFACE_DESC *pDesc,
		/* [in] */ UINT NumSurfaces,
		/* [in] */ DXGI_USAGE Usage,
		/* [annotation][in] */ 
		__in_opt  const DXGI_SHARED_RESOURCE *pSharedResource,
		/* [annotation][out] */ 
		__out  IDXGISurface **ppSurface)
	{
		return m_pReal->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency( 
		/* [annotation][size_is][in] */ 
		__in_ecount(NumResources)  IUnknown *const *ppResources,
		/* [annotation][size_is][out] */ 
		__out_ecount(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
		/* [in] */ UINT NumResources)
	{
		return m_pReal->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	}

	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority( 
		/* [in] */ INT Priority)
	{
		return m_pReal->SetGPUThreadPriority(Priority);
	}

	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority( 
		/* [annotation][retval][out] */ 
		__out  INT *pPriority)
	{
		return m_pReal->GetGPUThreadPriority(pPriority);
	}
};

class WrappedIDXGIFactory : public IDXGIFactory, public RefCountDXGIObject
{
	IDXGIFactory* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIFactory(IDXGIFactory* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIFactory() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;
	
	//////////////////////////////
	// implement IDXGIFactory

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation( 
		HWND WindowHandle,
		UINT Flags)
	{
		return m_pReal->MakeWindowAssociation(WindowHandle, Flags);
	}

	virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation( 
		/* [annotation][out] */ 
		__out  HWND *pWindowHandle)
	{
		return m_pReal->GetWindowAssociation(pWindowHandle);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSwapChain( 
		/* [annotation][in] */ 
		__in  IUnknown *pDevice,
		/* [annotation][in] */ 
		__in  DXGI_SWAP_CHAIN_DESC *pDesc,
		/* [annotation][out] */ 
		__out  IDXGISwapChain **ppSwapChain)
	{
		return WrappedIDXGIFactory::staticCreateSwapChain(m_pReal, pDevice, pDesc, ppSwapChain);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter( 
		/* [in] */ HMODULE Module,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->CreateSoftwareAdapter(Module, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	static HRESULT staticCreateSwapChain(IDXGIFactory *factory,
		IUnknown *pDevice,
		DXGI_SWAP_CHAIN_DESC *pDesc,
		IDXGISwapChain **ppSwapChain);
};

// version 1

class WrappedIDXGIAdapter1 : public IDXGIAdapter1, public RefCountDXGIObject
{
	IDXGIAdapter1* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIAdapter1(IDXGIAdapter1* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIAdapter1() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;
	
	//////////////////////////////
	// implement IDXGIAdapter

	virtual HRESULT STDMETHODCALLTYPE EnumOutputs( 
		/* [in] */ UINT Output,
		/* [annotation][out][in] */ 
		__out  IDXGIOutput **ppOutput)
	{
		return m_pReal->EnumOutputs(Output, ppOutput);
	}

	virtual HRESULT STDMETHODCALLTYPE GetDesc( 
		/* [annotation][out] */ 
		__out  DXGI_ADAPTER_DESC *pDesc)
	{
		return m_pReal->GetDesc(pDesc);
	}

	virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport( 
		/* [annotation][in] */ 
		__in  REFGUID InterfaceName,
		/* [annotation][out] */ 
		__out  LARGE_INTEGER *pUMDVersion)
	{
		return m_pReal->CheckInterfaceSupport(InterfaceName, pUMDVersion);
	}

	//////////////////////////////
	// implement IDXGIAdapter1

	virtual HRESULT STDMETHODCALLTYPE GetDesc1( 
		/* [out] */ DXGI_ADAPTER_DESC1 *pDesc)
	{
		return m_pReal->GetDesc1(pDesc);
	}
};

class WrappedIDXGIDevice1 : public IDXGIDevice1, public RefCountDXGIObject
{
	IDXGIDevice1* m_pReal;
	ID3D11Device* m_pD3DDevice;
public:
	WrappedIDXGIDevice1(IDXGIDevice1* real, ID3D11Device *d3d) :
	  RefCountDXGIObject(real), m_pReal(real), m_pD3DDevice(d3d) { m_pD3DDevice->AddRef(); }
	virtual ~WrappedIDXGIDevice1() { SAFE_RELEASE(m_pReal); SAFE_RELEASE(m_pD3DDevice); }
	
	static const public int AllocPoolCount = 4;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedIDXGIDevice1, AllocPoolCount);

	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

	ID3D11Device *GetD3DDevice() { return m_pD3DDevice; }

	//////////////////////////////
	// implement IDXGIDevice
	
	virtual HRESULT STDMETHODCALLTYPE GetAdapter( 
		/* [annotation][out] */ 
		__out  IDXGIAdapter **pAdapter)
	{
		HRESULT ret = m_pReal->GetAdapter(pAdapter);
		if(SUCCEEDED(ret)) *pAdapter = new WrappedIDXGIAdapter(*pAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSurface( 
		/* [annotation][in] */ 
		__in  const DXGI_SURFACE_DESC *pDesc,
		/* [in] */ UINT NumSurfaces,
		/* [in] */ DXGI_USAGE Usage,
		/* [annotation][in] */ 
		__in_opt  const DXGI_SHARED_RESOURCE *pSharedResource,
		/* [annotation][out] */ 
		__out  IDXGISurface **ppSurface)
	{
		return m_pReal->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency( 
		/* [annotation][size_is][in] */ 
		__in_ecount(NumResources)  IUnknown *const *ppResources,
		/* [annotation][size_is][out] */ 
		__out_ecount(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
		/* [in] */ UINT NumResources)
	{
		return m_pReal->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	}

	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority( 
		/* [in] */ INT Priority)
	{
		return m_pReal->SetGPUThreadPriority(Priority);
	}

	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority( 
		/* [annotation][retval][out] */ 
		__out  INT *pPriority)
	{
		return m_pReal->GetGPUThreadPriority(pPriority);
	}

	//////////////////////////////
	// implement IDXGIDevice1

	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency( 
		/* [in] */ UINT MaxLatency)
	{
		return m_pReal->SetMaximumFrameLatency(MaxLatency);
	}

	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency( 
		/* [annotation][out] */ 
		__out  UINT *pMaxLatency)
	{
		return m_pReal->GetMaximumFrameLatency(pMaxLatency);
	}
};

class WrappedIDXGIFactory1 : public IDXGIFactory1, public RefCountDXGIObject
{
	IDXGIFactory1* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIFactory1(IDXGIFactory1* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIFactory1() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;
	
	//////////////////////////////
	// implement IDXGIFactory

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation( 
		HWND WindowHandle,
		UINT Flags)
	{
		return m_pReal->MakeWindowAssociation(WindowHandle, Flags);
	}

	virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation( 
		/* [annotation][out] */ 
		__out  HWND *pWindowHandle)
	{
		return m_pReal->GetWindowAssociation(pWindowHandle);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSwapChain( 
		/* [annotation][in] */ 
		__in  IUnknown *pDevice,
		/* [annotation][in] */ 
		__in  DXGI_SWAP_CHAIN_DESC *pDesc,
		/* [annotation][out] */ 
		__out  IDXGISwapChain **ppSwapChain)
	{
		return WrappedIDXGIFactory::staticCreateSwapChain(m_pReal, pDevice, pDesc, ppSwapChain);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter( 
		/* [in] */ HMODULE Module,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->CreateSoftwareAdapter(Module, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	//////////////////////////////
	// implement IDXGIFactory1

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters1( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter1 **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters1(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter1(*ppAdapter);
		return ret;
	}

	virtual BOOL STDMETHODCALLTYPE IsCurrent( void)
	{
		return m_pReal->IsCurrent();
	}
};

#if defined(INCLUDE_DXGI_1_2)

class WrappedIDXGIDevice2 : public IDXGIDevice2, public RefCountDXGIObject
{
	IDXGIDevice2* m_pReal;
	ID3D11Device* m_pD3DDevice;
public:
	WrappedIDXGIDevice2(IDXGIDevice2* real, ID3D11Device *d3d) :
	  RefCountDXGIObject(real), m_pReal(real), m_pD3DDevice(d3d) { m_pD3DDevice->AddRef(); }
	virtual ~WrappedIDXGIDevice2() { SAFE_RELEASE(m_pReal); SAFE_RELEASE(m_pD3DDevice); }
	
	static const public int AllocPoolCount = 4;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedIDXGIDevice2, AllocPoolCount);

	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

	ID3D11Device *GetD3DDevice() { return m_pD3DDevice; }

	//////////////////////////////
	// implement IDXGIDevice
	
	virtual HRESULT STDMETHODCALLTYPE GetAdapter( 
		/* [annotation][out] */ 
		__out  IDXGIAdapter **pAdapter)
	{
		HRESULT ret = m_pReal->GetAdapter(pAdapter);
		if(SUCCEEDED(ret)) *pAdapter = new WrappedIDXGIAdapter(*pAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSurface( 
		/* [annotation][in] */ 
		__in  const DXGI_SURFACE_DESC *pDesc,
		/* [in] */ UINT NumSurfaces,
		/* [in] */ DXGI_USAGE Usage,
		/* [annotation][in] */ 
		__in_opt  const DXGI_SHARED_RESOURCE *pSharedResource,
		/* [annotation][out] */ 
		__out  IDXGISurface **ppSurface)
	{
		return m_pReal->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency( 
		/* [annotation][size_is][in] */ 
		__in_ecount(NumResources)  IUnknown *const *ppResources,
		/* [annotation][size_is][out] */ 
		__out_ecount(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
		/* [in] */ UINT NumResources)
	{
		return m_pReal->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	}

	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority( 
		/* [in] */ INT Priority)
	{
		return m_pReal->SetGPUThreadPriority(Priority);
	}

	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority( 
		/* [annotation][retval][out] */ 
		__out  INT *pPriority)
	{
		return m_pReal->GetGPUThreadPriority(pPriority);
	}

	//////////////////////////////
	// implement IDXGIDevice1

	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency( 
		/* [in] */ UINT MaxLatency)
	{
		return m_pReal->SetMaximumFrameLatency(MaxLatency);
	}

	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency( 
		/* [annotation][out] */ 
		__out  UINT *pMaxLatency)
	{
		return m_pReal->GetMaximumFrameLatency(pMaxLatency);
	}
	
	//////////////////////////////
	// implement IDXGIDevice2
	virtual HRESULT STDMETHODCALLTYPE OfferResources( 
		/* [annotation][in] */ 
		_In_  UINT NumResources,
		/* [annotation][size_is][in] */ 
		_In_reads_(NumResources)  IDXGIResource *const *ppResources,
		/* [annotation][in] */ 
		_In_  DXGI_OFFER_RESOURCE_PRIORITY Priority)
	{
		return m_pReal->OfferResources(NumResources, ppResources, Priority);
	}
	
	virtual HRESULT STDMETHODCALLTYPE ReclaimResources( 
		/* [annotation][in] */ 
		_In_  UINT NumResources,
		/* [annotation][size_is][in] */ 
		_In_reads_(NumResources)  IDXGIResource *const *ppResources,
		/* [annotation][size_is][out] */ 
		_Out_writes_all_opt_(NumResources)  BOOL *pDiscarded)
	{
		return m_pReal->ReclaimResources(NumResources, ppResources, pDiscarded);
	}
	
	virtual HRESULT STDMETHODCALLTYPE EnqueueSetEvent( 
		/* [annotation][in] */ 
		_In_  HANDLE hEvent)
	{
		return m_pReal->EnqueueSetEvent(hEvent);
	}
};
class WrappedIDXGIDevice3 : public IDXGIDevice3, public RefCountDXGIObject
{
	IDXGIDevice3* m_pReal;
	ID3D11Device* m_pD3DDevice;
public:
	WrappedIDXGIDevice3(IDXGIDevice3* real, ID3D11Device *d3d) :
	  RefCountDXGIObject(real), m_pReal(real), m_pD3DDevice(d3d) { m_pD3DDevice->AddRef(); }
	virtual ~WrappedIDXGIDevice3() { SAFE_RELEASE(m_pReal); SAFE_RELEASE(m_pD3DDevice); }
	
	static const public int AllocPoolCount = 4;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedIDXGIDevice3, AllocPoolCount);

	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

	ID3D11Device *GetD3DDevice() { return m_pD3DDevice; }

	//////////////////////////////
	// implement IDXGIDevice
	
	virtual HRESULT STDMETHODCALLTYPE GetAdapter( 
		/* [annotation][out] */ 
		__out  IDXGIAdapter **pAdapter)
	{
		HRESULT ret = m_pReal->GetAdapter(pAdapter);
		if(SUCCEEDED(ret)) *pAdapter = new WrappedIDXGIAdapter(*pAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSurface( 
		/* [annotation][in] */ 
		__in  const DXGI_SURFACE_DESC *pDesc,
		/* [in] */ UINT NumSurfaces,
		/* [in] */ DXGI_USAGE Usage,
		/* [annotation][in] */ 
		__in_opt  const DXGI_SHARED_RESOURCE *pSharedResource,
		/* [annotation][out] */ 
		__out  IDXGISurface **ppSurface)
	{
		return m_pReal->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
	}

	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency( 
		/* [annotation][size_is][in] */ 
		__in_ecount(NumResources)  IUnknown *const *ppResources,
		/* [annotation][size_is][out] */ 
		__out_ecount(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
		/* [in] */ UINT NumResources)
	{
		return m_pReal->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	}

	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority( 
		/* [in] */ INT Priority)
	{
		return m_pReal->SetGPUThreadPriority(Priority);
	}

	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority( 
		/* [annotation][retval][out] */ 
		__out  INT *pPriority)
	{
		return m_pReal->GetGPUThreadPriority(pPriority);
	}

	//////////////////////////////
	// implement IDXGIDevice1

	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency( 
		/* [in] */ UINT MaxLatency)
	{
		return m_pReal->SetMaximumFrameLatency(MaxLatency);
	}

	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency( 
		/* [annotation][out] */ 
		__out  UINT *pMaxLatency)
	{
		return m_pReal->GetMaximumFrameLatency(pMaxLatency);
	}
	
	//////////////////////////////
	// implement IDXGIDevice2

	virtual HRESULT STDMETHODCALLTYPE OfferResources( 
		/* [annotation][in] */ 
		_In_  UINT NumResources,
		/* [annotation][size_is][in] */ 
		_In_reads_(NumResources)  IDXGIResource *const *ppResources,
		/* [annotation][in] */ 
		_In_  DXGI_OFFER_RESOURCE_PRIORITY Priority)
	{
		return m_pReal->OfferResources(NumResources, ppResources, Priority);
	}
	
	virtual HRESULT STDMETHODCALLTYPE ReclaimResources( 
		/* [annotation][in] */ 
		_In_  UINT NumResources,
		/* [annotation][size_is][in] */ 
		_In_reads_(NumResources)  IDXGIResource *const *ppResources,
		/* [annotation][size_is][out] */ 
		_Out_writes_all_opt_(NumResources)  BOOL *pDiscarded)
	{
		return m_pReal->ReclaimResources(NumResources, ppResources, pDiscarded);
	}
	
	virtual HRESULT STDMETHODCALLTYPE EnqueueSetEvent( 
		/* [annotation][in] */ 
		_In_  HANDLE hEvent)
	{
		return m_pReal->EnqueueSetEvent(hEvent);
	}
	
	//////////////////////////////
	// implement IDXGIDevice3
	
	virtual void STDMETHODCALLTYPE Trim()
	{
		m_pReal->Trim();
	}
};

class WrappedIDXGIAdapter2 : public IDXGIAdapter2, public RefCountDXGIObject
{
	IDXGIAdapter2* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIAdapter2(IDXGIAdapter2* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIAdapter2() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;
	
	//////////////////////////////
	// implement IDXGIAdapter

	virtual HRESULT STDMETHODCALLTYPE EnumOutputs( 
		/* [in] */ UINT Output,
		/* [annotation][out][in] */ 
		__out  IDXGIOutput **ppOutput)
	{
		return m_pReal->EnumOutputs(Output, ppOutput);
	}

	virtual HRESULT STDMETHODCALLTYPE GetDesc( 
		/* [annotation][out] */ 
		__out  DXGI_ADAPTER_DESC *pDesc)
	{
		return m_pReal->GetDesc(pDesc);
	}

	virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport( 
		/* [annotation][in] */ 
		__in  REFGUID InterfaceName,
		/* [annotation][out] */ 
		__out  LARGE_INTEGER *pUMDVersion)
	{
		return m_pReal->CheckInterfaceSupport(InterfaceName, pUMDVersion);
	}

	//////////////////////////////
	// implement IDXGIAdapter1

	virtual HRESULT STDMETHODCALLTYPE GetDesc1( 
		/* [out] */ DXGI_ADAPTER_DESC1 *pDesc)
	{
		return m_pReal->GetDesc1(pDesc);
	}
	
	//////////////////////////////
	// implement IDXGIAdapter2

	virtual HRESULT STDMETHODCALLTYPE GetDesc2( 
		/* [annotation][out] */ 
		_Out_  DXGI_ADAPTER_DESC2 *pDesc)
	{
		return m_pReal->GetDesc2(pDesc);
	}
};

class WrappedIDXGIFactory2 : public IDXGIFactory2, public RefCountDXGIObject
{
	IDXGIFactory2* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIFactory2(IDXGIFactory2* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIFactory2() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;
	
	//////////////////////////////
	// implement IDXGIFactory

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation( 
		HWND WindowHandle,
		UINT Flags)
	{
		return m_pReal->MakeWindowAssociation(WindowHandle, Flags);
	}

	virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation( 
		/* [annotation][out] */ 
		__out  HWND *pWindowHandle)
	{
		return m_pReal->GetWindowAssociation(pWindowHandle);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSwapChain( 
		/* [annotation][in] */ 
		__in  IUnknown *pDevice,
		/* [annotation][in] */ 
		__in  DXGI_SWAP_CHAIN_DESC *pDesc,
		/* [annotation][out] */ 
		__out  IDXGISwapChain **ppSwapChain)
	{
		return WrappedIDXGIFactory::staticCreateSwapChain(m_pReal, pDevice, pDesc, ppSwapChain);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter( 
		/* [in] */ HMODULE Module,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->CreateSoftwareAdapter(Module, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	//////////////////////////////
	// implement IDXGIFactory1

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters1( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter1 **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters1(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter1(*ppAdapter);
		return ret;
	}

	virtual BOOL STDMETHODCALLTYPE IsCurrent( void)
	{
		return m_pReal->IsCurrent();
	}

	//////////////////////////////
	// implement IDXGIFactory2
	
	virtual BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled( void)
	{
		return m_pReal->IsWindowedStereoEnabled();
	}
	
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd( 
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  HWND hWnd,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Out_  IDXGISwapChain1 **ppSwapChain)
	{
		return WrappedIDXGIFactory2::staticCreateSwapChainForHwnd(m_pReal, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	}
	
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow( 
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  IUnknown *pWindow,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Out_  IDXGISwapChain1 **ppSwapChain)
	{
		return WrappedIDXGIFactory2::staticCreateSwapChainForCoreWindow(m_pReal, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid( 
		/* [annotation] */ 
		_In_  HANDLE hResource,
		/* [annotation] */ 
		_Out_  LUID *pLuid)
	{
		return m_pReal->GetSharedResourceAdapterLuid(hResource, pLuid);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow( 
		/* [annotation][in] */ 
		_In_  HWND WindowHandle,
		/* [annotation][in] */ 
		_In_  UINT wMsg,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent( 
		/* [annotation][in] */ 
		_In_  HANDLE hEvent,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterStereoStatusEvent(hEvent, pdwCookie);
	}
	
	virtual void STDMETHODCALLTYPE UnregisterStereoStatus( 
		/* [annotation][in] */ 
		_In_  DWORD dwCookie)
	{
		return m_pReal->UnregisterStereoStatus(dwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow( 
		/* [annotation][in] */ 
		_In_  HWND WindowHandle,
		/* [annotation][in] */ 
		_In_  UINT wMsg,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent( 
		/* [annotation][in] */ 
		_In_  HANDLE hEvent,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	}
	
	virtual void STDMETHODCALLTYPE UnregisterOcclusionStatus( 
		/* [annotation][in] */ 
		_In_  DWORD dwCookie)
	{
		return m_pReal->UnregisterOcclusionStatus(dwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition( 
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Outptr_  IDXGISwapChain1 **ppSwapChain)
	{
		return WrappedIDXGIFactory2::staticCreateSwapChainForComposition(m_pReal, pDevice, pDesc, pRestrictToOutput, ppSwapChain);
	}
	
	// static functions to share implementation between this and WrappedIDXGIFactory3
	
	static HRESULT staticCreateSwapChainForHwnd( IDXGIFactory2 *factory,
		IUnknown *pDevice,
		HWND hWnd,
		const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
		IDXGIOutput *pRestrictToOutput,
		IDXGISwapChain1 **ppSwapChain);
	
	static HRESULT staticCreateSwapChainForCoreWindow( IDXGIFactory2 *factory,
		IUnknown *pDevice,
		IUnknown *pWindow,
		const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		IDXGIOutput *pRestrictToOutput,
		IDXGISwapChain1 **ppSwapChain);
	
	static HRESULT staticCreateSwapChainForComposition( IDXGIFactory2 *factory,
		IUnknown *pDevice,
		const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		IDXGIOutput *pRestrictToOutput,
		IDXGISwapChain1 **ppSwapChain);
};

class WrappedIDXGIFactory3 : public IDXGIFactory3, public RefCountDXGIObject
{
	IDXGIFactory3* m_pReal;
	unsigned int m_iRefcount;
public:
	WrappedIDXGIFactory3(IDXGIFactory3* real) : RefCountDXGIObject(real), m_pReal(real), m_iRefcount(1) {}
	virtual ~WrappedIDXGIFactory3() { SAFE_RELEASE(m_pReal); }
	
	IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT;
	
	//////////////////////////////
	// implement IDXGIFactory

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation( 
		HWND WindowHandle,
		UINT Flags)
	{
		return m_pReal->MakeWindowAssociation(WindowHandle, Flags);
	}

	virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation( 
		/* [annotation][out] */ 
		__out  HWND *pWindowHandle)
	{
		return m_pReal->GetWindowAssociation(pWindowHandle);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSwapChain( 
		/* [annotation][in] */ 
		__in  IUnknown *pDevice,
		/* [annotation][in] */ 
		__in  DXGI_SWAP_CHAIN_DESC *pDesc,
		/* [annotation][out] */ 
		__out  IDXGISwapChain **ppSwapChain)
	{
		return WrappedIDXGIFactory::staticCreateSwapChain(m_pReal, pDevice, pDesc, ppSwapChain);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter( 
		/* [in] */ HMODULE Module,
		/* [annotation][out] */ 
		__out  IDXGIAdapter **ppAdapter)
	{
		HRESULT ret = m_pReal->CreateSoftwareAdapter(Module, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter(*ppAdapter);
		return ret;
	}

	//////////////////////////////
	// implement IDXGIFactory1

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters1( 
		/* [in] */ UINT Adapter,
		/* [annotation][out] */ 
		__out  IDXGIAdapter1 **ppAdapter)
	{
		HRESULT ret = m_pReal->EnumAdapters1(Adapter, ppAdapter);
		if(SUCCEEDED(ret)) *ppAdapter = new WrappedIDXGIAdapter1(*ppAdapter);
		return ret;
	}

	virtual BOOL STDMETHODCALLTYPE IsCurrent( void)
	{
		return m_pReal->IsCurrent();
	}

	//////////////////////////////
	// implement IDXGIFactory2
	
	virtual BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled( void)
	{
		return m_pReal->IsWindowedStereoEnabled();
	}
	
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd( 
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  HWND hWnd,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Out_  IDXGISwapChain1 **ppSwapChain)
	{
		return WrappedIDXGIFactory2::staticCreateSwapChainForHwnd(m_pReal, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	}
	
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow( 
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  IUnknown *pWindow,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Out_  IDXGISwapChain1 **ppSwapChain)
	{
		return WrappedIDXGIFactory2::staticCreateSwapChainForCoreWindow(m_pReal, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	}
	
	virtual HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid( 
		/* [annotation] */ 
		_In_  HANDLE hResource,
		/* [annotation] */ 
		_Out_  LUID *pLuid)
	{
		return m_pReal->GetSharedResourceAdapterLuid(hResource, pLuid);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow( 
		/* [annotation][in] */ 
		_In_  HWND WindowHandle,
		/* [annotation][in] */ 
		_In_  UINT wMsg,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent( 
		/* [annotation][in] */ 
		_In_  HANDLE hEvent,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterStereoStatusEvent(hEvent, pdwCookie);
	}
	
	virtual void STDMETHODCALLTYPE UnregisterStereoStatus( 
		/* [annotation][in] */ 
		_In_  DWORD dwCookie)
	{
		return m_pReal->UnregisterStereoStatus(dwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow( 
		/* [annotation][in] */ 
		_In_  HWND WindowHandle,
		/* [annotation][in] */ 
		_In_  UINT wMsg,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent( 
		/* [annotation][in] */ 
		_In_  HANDLE hEvent,
		/* [annotation][out] */ 
		_Out_  DWORD *pdwCookie)
	{
		return m_pReal->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	}
	
	virtual void STDMETHODCALLTYPE UnregisterOcclusionStatus( 
		/* [annotation][in] */ 
		_In_  DWORD dwCookie)
	{
		return m_pReal->UnregisterOcclusionStatus(dwCookie);
	}
	
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition( 
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Outptr_  IDXGISwapChain1 **ppSwapChain)
	{
		return WrappedIDXGIFactory2::staticCreateSwapChainForComposition(m_pReal, pDevice, pDesc, pRestrictToOutput, ppSwapChain);
	}
	
	// static functions to share implementation between this and WrappedIDXGIFactory3
	
	static HRESULT staticCreateSwapChainForHwnd( IDXGIFactory2 *factory,
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  HWND hWnd,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Out_  IDXGISwapChain1 **ppSwapChain);
	
	static HRESULT staticCreateSwapChainForCoreWindow( IDXGIFactory2 *factory,
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  IUnknown *pWindow,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Out_  IDXGISwapChain1 **ppSwapChain);
	
	static HRESULT staticCreateSwapChainForComposition( IDXGIFactory2 *factory,
		/* [annotation][in] */ 
		_In_  IUnknown *pDevice,
		/* [annotation][in] */ 
		_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
		/* [annotation][in] */ 
		_In_opt_  IDXGIOutput *pRestrictToOutput,
		/* [annotation][out] */ 
		_Outptr_  IDXGISwapChain1 **ppSwapChain);
	
	//////////////////////////////
	// implement IDXGIFactory3
	
    virtual UINT STDMETHODCALLTYPE GetCreationFlags( void)
	{
		return m_pReal->GetCreationFlags();
	}
};

#endif
