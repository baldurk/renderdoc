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


#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"

#include <stdio.h>
#include <stddef.h>

WRAPPED_POOL_INST(WrappedIDXGIDevice);
WRAPPED_POOL_INST(WrappedIDXGIDevice1);
#if defined(INCLUDE_DXGI_1_2)
WRAPPED_POOL_INST(WrappedIDXGIDevice2);
WRAPPED_POOL_INST(WrappedIDXGIDevice3);
#endif

HRESULT WrappedIDXGIFactory::staticCreateSwapChain(IDXGIFactory *factory,
	IUnknown *pDevice,
	DXGI_SWAP_CHAIN_DESC *pDesc,
	IDXGISwapChain **ppSwapChain)
{
	if(WrappedID3D11Device::IsAlloc(pDevice) ||
		WrappedIDXGIDevice::IsAlloc(pDevice) ||
		WrappedIDXGIDevice1::IsAlloc(pDevice)
#if defined(INCLUDE_DXGI_1_2)
		|| WrappedIDXGIDevice2::IsAlloc(pDevice)
		|| WrappedIDXGIDevice3::IsAlloc(pDevice)
#endif
		)
	{
		WrappedID3D11Device *wrapDevice = (WrappedID3D11Device *)pDevice;
		
		if(WrappedIDXGIDevice::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice *)(IDXGIDevice *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice1::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice1 *)(IDXGIDevice1 *)pDevice)->GetD3DDevice();
#if defined(INCLUDE_DXGI_1_2)
		if(WrappedIDXGIDevice2::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice2 *)(IDXGIDevice2 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice3::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice3 *)(IDXGIDevice3 *)pDevice)->GetD3DDevice();
#endif

		if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen && pDesc)
		{
			pDesc->Windowed = TRUE;
		}

		HRESULT ret = factory->CreateSwapChain(wrapDevice->GetReal(), pDesc, ppSwapChain);
	
		if(SUCCEEDED(ret))
			*ppSwapChain = new WrappedIDXGISwapChain2(*ppSwapChain, pDesc ? pDesc->OutputWindow : NULL, wrapDevice);

		return ret;
	}
	else
	{
		RDCERR("Creating swap chain with non-hooked device!");
	}

	return factory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
}

#if defined(INCLUDE_DXGI_1_2)

HRESULT WrappedIDXGIFactory2::staticCreateSwapChainForHwnd( IDXGIFactory2 *factory,
	IUnknown *pDevice,
	HWND hWnd,
	const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
	IDXGIOutput *pRestrictToOutput,
	IDXGISwapChain1 **ppSwapChain)
{
	if(WrappedID3D11Device::IsAlloc(pDevice) ||
		WrappedIDXGIDevice::IsAlloc(pDevice) ||
		WrappedIDXGIDevice1::IsAlloc(pDevice) ||
		WrappedIDXGIDevice2::IsAlloc(pDevice) ||
		WrappedIDXGIDevice3::IsAlloc(pDevice)
		)
	{
		WrappedID3D11Device *wrapDevice = (WrappedID3D11Device *)pDevice;
		
		if(WrappedIDXGIDevice::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice *)(IDXGIDevice *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice1::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice1 *)(IDXGIDevice1 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice2::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice2 *)(IDXGIDevice2 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice3::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice3 *)(IDXGIDevice3 *)pDevice)->GetD3DDevice();

		if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen && pFullscreenDesc)
		{
			pFullscreenDesc = NULL;
		}

		HRESULT ret = factory->CreateSwapChainForHwnd(wrapDevice->GetReal(), hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	
		if(SUCCEEDED(ret))
			*ppSwapChain = new WrappedIDXGISwapChain2(*ppSwapChain, hWnd, wrapDevice);

		return ret;
	}
	else
	{
		RDCERR("Creating swap chain with non-hooked device!");
	}
	
	return factory->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
}

HRESULT WrappedIDXGIFactory2::staticCreateSwapChainForCoreWindow( IDXGIFactory2 *factory,
	IUnknown *pDevice,
	IUnknown *pWindow,
	const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	IDXGIOutput *pRestrictToOutput,
	IDXGISwapChain1 **ppSwapChain)
{
	if(WrappedID3D11Device::IsAlloc(pDevice) ||
		WrappedIDXGIDevice::IsAlloc(pDevice) ||
		WrappedIDXGIDevice1::IsAlloc(pDevice) ||
		WrappedIDXGIDevice2::IsAlloc(pDevice) ||
		WrappedIDXGIDevice3::IsAlloc(pDevice)
		)
	{
		WrappedID3D11Device *wrapDevice = (WrappedID3D11Device *)pDevice;
		
		if(WrappedIDXGIDevice::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice *)(IDXGIDevice *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice1::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice1 *)(IDXGIDevice1 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice2::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice2 *)(IDXGIDevice2 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice3::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice3 *)(IDXGIDevice3 *)pDevice)->GetD3DDevice();

		if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
		{
			RDCWARN("Impossible to disallow fullscreen on call to CreateSwapChainForCoreWindow");
		}

		HRESULT ret = factory->CreateSwapChainForCoreWindow(wrapDevice->GetReal(), pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	
		if(SUCCEEDED(ret)) 
		{
			HWND wnd = NULL;
			(*ppSwapChain)->GetHwnd(&wnd);
			*ppSwapChain = new WrappedIDXGISwapChain2(*ppSwapChain, wnd, wrapDevice);
		}

		return ret;
	}
	else
	{
		RDCERR("Creating swap chain with non-hooked device!");
	}

	return factory->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
}

HRESULT WrappedIDXGIFactory2::staticCreateSwapChainForComposition( IDXGIFactory2 *factory,
	IUnknown *pDevice,
	const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	IDXGIOutput *pRestrictToOutput,
	IDXGISwapChain1 **ppSwapChain)
{
	if(WrappedID3D11Device::IsAlloc(pDevice) ||
		WrappedIDXGIDevice::IsAlloc(pDevice) ||
		WrappedIDXGIDevice1::IsAlloc(pDevice) ||
		WrappedIDXGIDevice2::IsAlloc(pDevice) ||
		WrappedIDXGIDevice3::IsAlloc(pDevice)
		)
	{
		WrappedID3D11Device *wrapDevice = (WrappedID3D11Device *)pDevice;
		
		if(WrappedIDXGIDevice::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice *)(IDXGIDevice *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice1::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice1 *)(IDXGIDevice1 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice2::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice2 *)(IDXGIDevice2 *)pDevice)->GetD3DDevice();
		if(WrappedIDXGIDevice3::IsAlloc(pDevice))
			wrapDevice = (WrappedID3D11Device *)((WrappedIDXGIDevice3 *)(IDXGIDevice3 *)pDevice)->GetD3DDevice();

		if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
		{
			RDCWARN("Impossible to disallow fullscreen on call to CreateSwapChainForComposition");
		}

		HRESULT ret = factory->CreateSwapChainForComposition(wrapDevice->GetReal(), pDesc, pRestrictToOutput, ppSwapChain);
	
		if(SUCCEEDED(ret)) 
		{
			HWND wnd = NULL;
			(*ppSwapChain)->GetHwnd(&wnd);
			*ppSwapChain = new WrappedIDXGISwapChain2(*ppSwapChain, wnd, wrapDevice);
		}

		return ret;
	}
	else
	{
		RDCERR("Creating swap chain with non-hooked device!");
	}

	return factory->CreateSwapChainForComposition(pDevice, pDesc, pRestrictToOutput, ppSwapChain);
}

#endif

WrappedIDXGISwapChain2::WrappedIDXGISwapChain2(IDXGISwapChain* real, HWND wnd, WrappedID3D11Device *device)
	: RefCountDXGIObject(real), m_pReal(real), m_pDevice(device), m_iRefcount(1), m_Wnd(wnd)
{
	DXGI_SWAP_CHAIN_DESC desc;
	real->GetDesc(&desc);
	
#if defined(INCLUDE_DXGI_1_2)
	m_pReal1 = NULL;
	real->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&m_pReal1);
	m_pReal2 = NULL;
	real->QueryInterface(__uuidof(IDXGISwapChain2), (void **)&m_pReal2);
#endif

	int bufCount = desc.BufferCount;

	if(desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD)
		bufCount = 1;

	RDCASSERT(bufCount < MAX_NUM_BACKBUFFERS);

	for(int i=0; i < MAX_NUM_BACKBUFFERS; i++)
	{
		m_pBackBuffers[i] = NULL;

		if(i < bufCount)
		{
			GetBuffer(i, __uuidof(ID3D11Texture2D), (void **)&m_pBackBuffers[i]);

			WrappedID3D11Texture2D *wrapped = (WrappedID3D11Texture2D *)m_pBackBuffers[i];

			if(wrapped)
			{
				// keep ref as a 'view' (invisible to user)
				wrapped->ViewAddRef();
				wrapped->Release();
			}
		}
	}

	SAFE_ADDREF(m_pDevice);

	// we do a 'fake' present right at the start, so that we can capture frame 1, by
	// going from this fake present to the first present.
	m_pDevice->FirstFrame(this);
}

WrappedIDXGISwapChain2::~WrappedIDXGISwapChain2()
{
	m_pDevice->ReleaseSwapchainResources(this);

	for(int i=0; i < MAX_NUM_BACKBUFFERS; i++)
	{
		WrappedID3D11Texture2D *wrapped = (WrappedID3D11Texture2D *)m_pBackBuffers[i];
		if(wrapped)
			wrapped->ViewRelease();
		m_pBackBuffers[i] = NULL;
	}
#if defined(INCLUDE_DXGI_1_2)
	SAFE_RELEASE(m_pReal1);
	SAFE_RELEASE(m_pReal2);
#endif
	SAFE_RELEASE(m_pReal);

	SAFE_RELEASE(m_pDevice);
}

HRESULT WrappedIDXGISwapChain2::ResizeBuffers( 
	/* [in] */ UINT BufferCount,
	/* [in] */ UINT Width,
	/* [in] */ UINT Height,
	/* [in] */ DXGI_FORMAT NewFormat,
	/* [in] */ UINT SwapChainFlags)
{
	for(int i=0; i < MAX_NUM_BACKBUFFERS; i++)
	{
		WrappedID3D11Texture2D *wrapped = (WrappedID3D11Texture2D*)m_pBackBuffers[i];
		if(wrapped)
		{
			m_pDevice->GetImmediateContext()->GetCurrentPipelineState()->UnbindIUnknownForWrite(wrapped);
			m_pDevice->GetImmediateContext()->GetCurrentPipelineState()->UnbindForRead(wrapped);

			wrapped->ViewRelease();
		}

		wrapped = NULL;
	}

	m_pDevice->ReleaseSwapchainResources(this);

	HRESULT ret = m_pReal->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
	
	DXGI_SWAP_CHAIN_DESC desc;
	m_pReal->GetDesc(&desc);

	int bufCount = desc.BufferCount;

	if(desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD)
		bufCount = 1;

	RDCASSERT(bufCount < MAX_NUM_BACKBUFFERS);

	for(int i=0; i < MAX_NUM_BACKBUFFERS; i++)
	{
		m_pBackBuffers[i] = NULL;

		if(i < bufCount)
		{
			GetBuffer(i, __uuidof(ID3D11Texture2D), (void **)&m_pBackBuffers[i]);

			WrappedID3D11Texture2D *wrapped = (WrappedID3D11Texture2D *)m_pBackBuffers[i];

			if(wrapped)
			{
				// keep ref as a 'view' (invisible to user)
				wrapped->ViewAddRef();
				wrapped->Release();
			}
		}
	}

	return ret;
}

HRESULT WrappedIDXGISwapChain2::SetFullscreenState( 
	/* [in] */ BOOL Fullscreen,
	/* [in] */ IDXGIOutput *pTarget)
{
	if(RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
		return m_pReal->SetFullscreenState(Fullscreen, pTarget);

	return S_OK;
}

HRESULT WrappedIDXGISwapChain2::GetFullscreenState( 
	/* [out] */ BOOL *pFullscreen,
	/* [out] */ IDXGIOutput **ppTarget)
{
	return m_pReal->GetFullscreenState(pFullscreen, ppTarget);
}

HRESULT WrappedIDXGISwapChain2::GetBuffer( 
	/* [in] */ UINT Buffer,
	/* [in] */ REFIID riid,
	/* [out][in] */ void **ppSurface)
{
	if(ppSurface == NULL) return E_INVALIDARG;

	if(riid == __uuidof(ID3D10Texture2D) || riid == __uuidof(ID3D10Resource))
	{
		RDCERR("Querying swapchain buffers via D3D10 interface UUIDs is not supported");
		return E_NOINTERFACE;
	}
	else if(riid != __uuidof(ID3D11Texture2D) && riid != __uuidof(ID3D11Resource))
	{
		RDCERR("Unsupported or unrecognised UUID passed to IDXGISwapChain::GetBuffer - %s", ToStr::Get(riid).c_str());
		return E_NOINTERFACE;
	}

	RDCASSERT(riid == __uuidof(ID3D11Texture2D) || riid == __uuidof(ID3D11Resource));

	HRESULT ret = m_pReal->GetBuffer(Buffer, riid, ppSurface);
	
	ID3D11Texture2D *realSurface = (ID3D11Texture2D *)*ppSurface;
	ID3D11Texture2D *tex = realSurface;
	if(FAILED(ret))
	{
		RDCERR("Failed to get swapchain backbuffer %d: %08x", Buffer, ret);
		SAFE_RELEASE(realSurface);
		tex = NULL;
	}
	else if(m_pDevice->GetResourceManager()->HasWrapper(realSurface))
	{
		tex = (ID3D11Texture2D *)m_pDevice->GetResourceManager()->GetWrapper(realSurface);
		tex->AddRef();

		realSurface->Release();
	}
	else
	{
		tex = new WrappedID3D11Texture2D(realSurface, m_pDevice, TEXDISPLAY_UNKNOWN);

		DXGI_SWAP_CHAIN_DESC desc;
		m_pReal->GetDesc(&desc);

		m_pDevice->SetSwapChainTexture(this, &desc, Buffer, tex);

		SetDebugName(tex, "Swap Chain Backbuffer");
	}
	
	*ppSurface = tex;

	return ret;
}

HRESULT WrappedIDXGISwapChain2::GetDevice( 
	/* [in] */ REFIID riid,
	/* [retval][out] */ void **ppDevice)
{
	HRESULT ret = m_pReal->GetDevice(riid, ppDevice);

	if(SUCCEEDED(ret))
	{
		// try one of the trivial wraps, we don't mind making a new one of those
		if(riid == __uuidof(ID3D11Device))
		{
			// probably they're asking for the device device.
			*ppDevice = m_pDevice;
			m_pDevice->AddRef();
		}
		else if(riid == __uuidof(IDXGISwapChain))
		{
			// don't think anyone would try this, but what the hell.
			*ppDevice = this;
			AddRef();
		}
		else if(!HandleWrap(riid, ppDevice))
		{
			// can probably get away with returning the real result here,
			// but it worries me a bit.
			RDCUNIMPLEMENTED("Not returning trivial type");
		}
	}

	return ret;
}

HRESULT WrappedIDXGISwapChain2::Present( 
		/* [in] */ UINT SyncInterval,
		/* [in] */ UINT Flags)
{
	if(!RenderDoc::Inst().GetCaptureOptions().AllowVSync)
	{
		SyncInterval = 0;
	}

	m_pDevice->Present(this, SyncInterval, Flags);

	return m_pReal->Present(SyncInterval, Flags);
}

#if defined(INCLUDE_DXGI_1_2)
HRESULT WrappedIDXGISwapChain2::Present1(UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	if(!RenderDoc::Inst().GetCaptureOptions().AllowVSync)
	{
		SyncInterval = 0;
	}

	m_pDevice->Present(this, SyncInterval, Flags);

	return m_pReal1->Present1(SyncInterval, Flags, pPresentParameters);
}
#endif

bool RefCountDXGIObject::HandleWrap(REFIID riid, void **ppvObject)
{
	if(ppvObject == NULL || *ppvObject == NULL)
	{
		RDCWARN("HandleWrap called with NULL ppvObject");
		return false;
	}

	if(riid == __uuidof(IDXGIDevice))
	{
		// should have been handled elsewhere, so we can properly create this device
		RDCERR("Unexpected uuid in RefCountDXGIObject::HandleWrap");
		return false;
	}
	else if(riid == __uuidof(IDXGIAdapter))
	{
		IDXGIAdapter *real = (IDXGIAdapter *)(*ppvObject);
		*ppvObject = new WrappedIDXGIAdapter(real);
		return true;
	}
	else if(riid == __uuidof(IDXGIFactory))
	{
		// yes I know PRECISELY how fucked up this is. Speak to microsoft - after KB2670838 the internal D3D11
		// device creation function will pass in __uuidof(IDXGIFactory) then attempt to call EnumDevices1 (which
		// is in the IDXGIFactory1 vtable). Doing this *should* be safe as using a IDXGIFactory1 like a IDXGIFactory
		// should all just work by definition, but there's no way to know now if someone trying to create a IDXGIFactory
		// really means it or not.
		IDXGIFactory1 *real = (IDXGIFactory1 *)(*ppvObject);
		*ppvObject = new WrappedIDXGIFactory1(real);
		return true;
	}


	else if(riid == __uuidof(IDXGIDevice1))
	{
		// should have been handled elsewhere, so we can properly create this device
		RDCERR("Unexpected uuid in RefCountDXGIObject::HandleWrap");
		return false;
	}
	else if(riid == __uuidof(IDXGIAdapter1))
	{
		IDXGIAdapter1 *real = (IDXGIAdapter1 *)(*ppvObject);
		*ppvObject = new WrappedIDXGIAdapter1(real);
		return true;
	}
	else if(riid == __uuidof(IDXGIFactory1))
	{
		IDXGIFactory1 *real = (IDXGIFactory1 *)(*ppvObject);
		*ppvObject = new WrappedIDXGIFactory1(real);
		return true;
	}
#if defined(INCLUDE_DXGI_1_2)
	else if(riid == __uuidof(IDXGIAdapter2))
	{
		IDXGIAdapter2 *real = (IDXGIAdapter2 *)(*ppvObject);
		*ppvObject = new WrappedIDXGIAdapter2(real);
		return true;
	}
	else if(riid == __uuidof(IDXGIFactory2))
	{
		IDXGIFactory2 *real = (IDXGIFactory2 *)(*ppvObject);
		*ppvObject = new WrappedIDXGIFactory2(real);
		return true;
	}
	else if(riid == __uuidof(IDXGIFactory3))
	{
		IDXGIFactory3 *real = (IDXGIFactory3 *)(*ppvObject);
		*ppvObject = new WrappedIDXGIFactory3(real);
		return true;
	}
#endif
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying IDXGIObject for interface: %s", guid.c_str());
	}
	
	return false;
}

HRESULT STDMETHODCALLTYPE RefCountDXGIObject::GetParent( 
	/* [in] */ REFIID riid,
	/* [retval][out] */ void **ppParent)
{
	HRESULT ret = m_pReal->GetParent(riid, ppParent);

	if(SUCCEEDED(ret))
		HandleWrap(riid, ppParent);

	return ret;
}

HRESULT RefCountDXGIObject::WrapQueryInterface(IUnknown *real, REFIID riid, void **ppvObject)
{
	HRESULT ret = real->QueryInterface(riid, ppvObject);

	if(SUCCEEDED(ret))
		HandleWrap(riid, ppvObject);

	return ret;
}
	
HRESULT STDMETHODCALLTYPE WrappedIDXGISwapChain2::QueryInterface(REFIID riid, void **ppvObject)
{
	if(riid == __uuidof(IDXGISwapChain))
	{
		AddRef();
		*ppvObject = (IDXGISwapChain *)this;
		return S_OK;
	}
#if defined(INCLUDE_DXGI_1_2)
	else if(riid == __uuidof(IDXGISwapChain1))
	{
		if(m_pReal1)
		{
			AddRef();
			*ppvObject = (IDXGISwapChain1 *)this;
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
	else if(riid == __uuidof(IDXGISwapChain2))
	{
		if(m_pReal2)
		{
			AddRef();
			*ppvObject = (IDXGISwapChain2 *)this;
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
#endif
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying IDXGISwapChain for interface: %s", guid.c_str());
	}

	return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice::QueryInterface( REFIID riid, void **ppvObject )
{
	if(riid == __uuidof(ID3D11Device))
	{
		m_pD3DDevice->AddRef();
		*ppvObject = m_pD3DDevice;
		return S_OK;
	}
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying IDXGIDevice for interface: %s", guid.c_str());
	}

	return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice1::QueryInterface( REFIID riid, void **ppvObject )
{
	HRESULT hr = S_OK;

	if(riid == __uuidof(ID3D11Device))
	{
		m_pD3DDevice->AddRef();
		*ppvObject = m_pD3DDevice;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice1))
	{
		AddRef();
		*ppvObject = (IDXGIDevice1 *)this;
		return S_OK;
	}
#if defined(INCLUDE_DXGI_1_2)
	else if(riid == __uuidof(IDXGIDevice2))
	{
		hr = m_pReal->QueryInterface(riid, ppvObject);

		if(SUCCEEDED(hr))
		{
			IDXGIDevice2 *real = (IDXGIDevice2 *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice2(real, m_pD3DDevice);
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
	else if(riid == __uuidof(IDXGIDevice3))
	{
		hr = m_pReal->QueryInterface(riid, ppvObject);

		if(SUCCEEDED(hr))
		{
			IDXGIDevice3 *real = (IDXGIDevice3 *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice3(real, m_pD3DDevice);
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
#endif
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying IDXGIDevice1 for interface: %s", guid.c_str());
	}

	return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

#if defined(INCLUDE_DXGI_1_2)

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice2::QueryInterface( REFIID riid, void **ppvObject )
{
	if(riid == __uuidof(ID3D11Device))
	{
		m_pD3DDevice->AddRef();
		*ppvObject = m_pD3DDevice;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice1))
	{
		AddRef();
		*ppvObject = (IDXGIDevice1 *)this;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice2))
	{
		AddRef();
		*ppvObject = (IDXGIDevice2 *)this;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice3))
	{
		HRESULT hr = m_pReal->QueryInterface(riid, ppvObject);
		
		if(SUCCEEDED(hr))
		{
			IDXGIDevice3 *real = (IDXGIDevice3 *)(*ppvObject);
			*ppvObject = new WrappedIDXGIDevice3(real, m_pD3DDevice);
			return S_OK;
		}
		else
		{
			return E_NOINTERFACE;
		}
	}
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying IDXGIDevice2 for interface: %s", guid.c_str());
	}

	return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice3::QueryInterface( REFIID riid, void **ppvObject )
{
	if(riid == __uuidof(ID3D11Device))
	{
		m_pD3DDevice->AddRef();
		*ppvObject = m_pD3DDevice;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice1))
	{
		AddRef();
		*ppvObject = (IDXGIDevice1 *)this;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice2))
	{
		AddRef();
		*ppvObject = (IDXGIDevice2 *)this;
		return S_OK;
	}
	else if(riid == __uuidof(IDXGIDevice3))
	{
		AddRef();
		*ppvObject = (IDXGIDevice3 *)this;
		return S_OK;
	}
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying IDXGIDevice3 for interface: %s", guid.c_str());
	}

	return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

#endif

