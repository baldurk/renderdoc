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

#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_manager.h"
#include "driver/shaders/dxbc/dxbc_inspect.h"
#include <algorithm>

enum ResourceType
{
	Resource_Unknown = 0,
	Resource_InputLayout,
	Resource_Buffer,
	Resource_Texture1D,
	Resource_Texture2D,
	Resource_Texture3D,
	Resource_RasterizerState,
	Resource_RasterizerState1,
	Resource_BlendState,
	Resource_BlendState1,
	Resource_DepthStencilState,
	Resource_SamplerState,
	Resource_RenderTargetView,
	Resource_ShaderResourceView,
	Resource_DepthStencilView,
	Resource_UnorderedAccessView,
	Resource_Shader,
	Resource_Counter,
	Resource_Query,
	Resource_Predicate,
	Resource_ClassInstance,
	Resource_ClassLinkage,

	Resource_DeviceContext,
	Resource_CommandList,
};

ResourceType IdentifyTypeByPtr(IUnknown *ptr);
ResourceId GetIDForResource(ID3D11DeviceChild *ptr);

UINT GetByteSize(int Width, int Height, int Depth, DXGI_FORMAT Format, int mip);
UINT GetByteSize(ID3D11Texture1D *tex, int SubResource);
UINT GetByteSize(ID3D11Texture2D *tex, int SubResource);
UINT GetByteSize(ID3D11Texture3D *tex, int SubResource);

UINT GetMipForSubresource(ID3D11Resource *res, int Subresource);

// returns block size for block-compressed formats
UINT GetFormatBPP(DXGI_FORMAT f);

DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT f);
DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetDepthTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetFloatTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetUnormTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetSnormTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetUIntTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetSIntTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT f);
DXGI_FORMAT GetNonSRGBFormat(DXGI_FORMAT f);
bool IsBlockFormat(DXGI_FORMAT f);
bool IsDepthFormat(DXGI_FORMAT f);

bool IsUIntFormat(DXGI_FORMAT f);
bool IsTypelessFormat(DXGI_FORMAT f);
bool IsIntFormat(DXGI_FORMAT f);
bool IsSRGBFormat(DXGI_FORMAT f);

class TrackedResource
{
	public:
		TrackedResource()
		{
			m_ID = GetNewUniqueID();
		}

		ResourceId GetResourceID() { return m_ID; }

		static void SetReplayResourceIDs()
		{
			globalIDCounter = RDCMAX(uint64_t(globalIDCounter), uint64_t(globalIDCounter|0x1000000000000000ULL));
		}
		
	private:
		TrackedResource(const TrackedResource &);
		TrackedResource &operator =(const TrackedResource &);

		ResourceId GetNewUniqueID()
		{
			uint64_t newID = (uint64_t)InterlockedIncrement64(&globalIDCounter);

			return ResourceId(newID, true); // bool to make explicit
		}
		
		static volatile LONGLONG globalIDCounter;
		ResourceId m_ID;
};

template<typename NestedType>
class WrappedDXGIInterface : public RefCounter, public IDXGIKeyedMutex
#if defined(INCLUDE_D3D_11_1)
	, public IDXGISurface2
	, public IDXGIResource1
#else
	, public IDXGISurface1
	, public IDXGIResource
#endif
{
public:
	WrappedID3D11Device* m_pDevice;
	NestedType* m_pWrapped;

	WrappedDXGIInterface(NestedType* wrapped, WrappedID3D11Device* device)
		: RefCounter(NULL),
			m_pDevice(device),
			m_pWrapped(wrapped)
	{
		m_pWrapped->AddRef();
		m_pDevice->AddRef();
	}
	
	virtual ~WrappedDXGIInterface()
	{
		m_pWrapped->Release();
		m_pDevice->Release();
	}
	
	//////////////////////////////
	// Implement IUnknown
	ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::AddRef(); }
	ULONG STDMETHODCALLTYPE Release() { return RefCounter::Release(); }
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		// ensure the real object has this interface
		void *outObj;
		HRESULT hr = m_pWrapped->QueryInterface(riid, &outObj);

		IUnknown *unk = (IUnknown *)outObj;
		SAFE_RELEASE(unk);

		if(FAILED(hr))
		{
			return hr;
		}

		if(riid == __uuidof(IDXGIObject))
		{
			*ppvObject = (IDXGIObject *)(IDXGIKeyedMutex *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(IDXGIDeviceSubObject))
		{
			*ppvObject = (IDXGIDeviceSubObject *)(IDXGIKeyedMutex *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(IDXGIResource))
		{
			*ppvObject = (IDXGIResource *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(IDXGIKeyedMutex))
		{
			*ppvObject = (IDXGIKeyedMutex *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(IDXGISurface))
		{
			*ppvObject = (IDXGISurface *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(IDXGISurface1))
		{
			*ppvObject = (IDXGISurface1 *)this;
			AddRef();
			return S_OK;
		}
#if defined(INCLUDE_D3D_11_1)
		if(riid == __uuidof(IDXGIResource1))
		{
			*ppvObject = (IDXGIResource1 *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(IDXGISurface2))
		{
			*ppvObject = (IDXGISurface2 *)this;
			AddRef();
			return S_OK;
		}
#endif

		return m_pWrapped->QueryInterface(riid, ppvObject);
	}
	
	//////////////////////////////
	// Implement IDXGIObject
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
	{ return m_pWrapped->SetPrivateData(Name, DataSize, pData); }
        
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
	{ return m_pWrapped->SetPrivateDataInterface(Name, pUnknown); }

	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
	{ return m_pWrapped->GetPrivateData(Name, pDataSize, pData); }
        
	// this should only be called for adapters, devices, factories etc
	// so we pass it onto the device
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent)
	{
		return m_pDevice->QueryInterface(riid, ppParent);
	}
	
	//////////////////////////////
	// Implement IDXGIDeviceSubObject

	// same as GetParent
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice)
	{
		return m_pDevice->QueryInterface(riid, ppDevice);
	}
	
	//////////////////////////////
	// Implement IDXGIKeyedMutex
	HRESULT STDMETHODCALLTYPE AcquireSync(UINT64 Key, DWORD dwMilliseconds)
	{
		// temporarily get the real interface
		IDXGIKeyedMutex *mutex = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&mutex);
		if(FAILED(hr))
		{
			SAFE_RELEASE(mutex);
			return hr;
		}

		hr = mutex->AcquireSync(Key, dwMilliseconds);
		SAFE_RELEASE(mutex);
		return hr;
	}

	HRESULT STDMETHODCALLTYPE ReleaseSync(UINT64 Key)
	{
		// temporarily get the real interface
		IDXGIKeyedMutex *mutex = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&mutex);
		if(FAILED(hr))
		{
			SAFE_RELEASE(mutex);
			return hr;
		}

		hr = mutex->ReleaseSync(Key);
		SAFE_RELEASE(mutex);
		return hr;
	}
	
	//////////////////////////////
	// Implement IDXGIResource
	virtual HRESULT STDMETHODCALLTYPE GetSharedHandle(HANDLE *pSharedHandle)
	{
		// temporarily get the real interface
		IDXGIResource *res = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
		if(FAILED(hr))
		{
			SAFE_RELEASE(res);
			return hr;
		}

		hr = res->GetSharedHandle(pSharedHandle);
		SAFE_RELEASE(res);
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE GetUsage(DXGI_USAGE *pUsage)
	{
		// temporarily get the real interface
		IDXGIResource *res = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
		if(FAILED(hr))
		{
			SAFE_RELEASE(res);
			return hr;
		}

		hr = res->GetUsage(pUsage);
		SAFE_RELEASE(res);
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority)
	{
		// temporarily get the real interface
		IDXGIResource *res = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
		if(FAILED(hr))
		{
			SAFE_RELEASE(res);
			return hr;
		}

		hr = res->SetEvictionPriority(EvictionPriority);
		SAFE_RELEASE(res);
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE GetEvictionPriority(UINT *pEvictionPriority)
	{
		// temporarily get the real interface
		IDXGIResource *res = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
		if(FAILED(hr))
		{
			SAFE_RELEASE(res);
			return hr;
		}

		hr = res->GetEvictionPriority(pEvictionPriority);
		SAFE_RELEASE(res);
		return hr;
	}
	
	//////////////////////////////
	// Implement IDXGISurface
	virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SURFACE_DESC *pDesc)
	{
		// temporarily get the real interface
		IDXGISurface *surf = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
		if(FAILED(hr))
		{
			SAFE_RELEASE(surf);
			return hr;
		}

		hr = surf->GetDesc(pDesc);
		SAFE_RELEASE(surf);
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE Map(DXGI_MAPPED_RECT *pLockedRect, UINT MapFlags)
	{
		// temporarily get the real interface
		IDXGISurface *surf = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
		if(FAILED(hr))
		{
			SAFE_RELEASE(surf);
			return hr;
		}

		hr = surf->Map(pLockedRect, MapFlags);
		SAFE_RELEASE(surf);
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE Unmap( void)
	{
		// temporarily get the real interface
		IDXGISurface *surf = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
		if(FAILED(hr))
		{
			SAFE_RELEASE(surf);
			return hr;
		}

		hr = surf->Unmap();
		SAFE_RELEASE(surf);
		return hr;
	}

	//////////////////////////////
	// Implement IDXGISurface1
	virtual HRESULT STDMETHODCALLTYPE GetDC(BOOL Discard, HDC *phdc)
	{
		// temporarily get the real interface
		IDXGISurface1 *surf = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface1), (void **)&surf);
		if(FAILED(hr))
		{
			SAFE_RELEASE(surf);
			return hr;
		}

		hr = surf->GetDC(Discard, phdc);
		SAFE_RELEASE(surf);
		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE ReleaseDC(RECT *pDirtyRect)
	{
		// temporarily get the real interface
		IDXGISurface1 *surf = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface1), (void **)&surf);
		if(FAILED(hr))
		{
			SAFE_RELEASE(surf);
			return hr;
		}

		hr = surf->ReleaseDC(pDirtyRect);
		SAFE_RELEASE(surf);
		return hr;
	}

#if defined(INCLUDE_D3D_11_1)
	//////////////////////////////
	// Implement IDXGIResource1
	virtual HRESULT STDMETHODCALLTYPE CreateSubresourceSurface(UINT index, IDXGISurface2 **ppSurface)
	{
		if(ppSurface == NULL) return E_INVALIDARG;

		// maybe this will work?!?
		AddRef();
		*ppSurface = (IDXGISurface2 *)this;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *pAttributes, DWORD dwAccess, LPCWSTR lpName, HANDLE *pHandle)
	{
		// temporarily get the real interface
		IDXGIResource1 *res = NULL;
		HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource1), (void **)&res);
		if(FAILED(hr))
		{
			SAFE_RELEASE(res);
			return hr;
		}

		hr = res->CreateSharedHandle(pAttributes, dwAccess, lpName, pHandle);
		SAFE_RELEASE(res);
		return hr;
	}

	//////////////////////////////
	// Implement IDXGISurface2
	virtual HRESULT STDMETHODCALLTYPE GetResource(REFIID riid, void **ppParentResource, UINT *pSubresourceIndex)
	{
		// not really sure how to implement this :(.
		if(pSubresourceIndex) pSubresourceIndex = 0;
		return QueryInterface(riid, ppParentResource);
	}
#endif
};

template<typename NestedType>
class WrappedDeviceChild : public RefCounter, public NestedType, public TrackedResource
{
protected:
	WrappedID3D11Device* m_pDevice;
	NestedType* m_pReal;
	unsigned int m_PipelineRefs;

	WrappedDeviceChild(NestedType* real, WrappedID3D11Device* device)
		:	RefCounter(real),
			m_pDevice(device),
			m_pReal(real),
			m_PipelineRefs(0)
	{
		m_pDevice->SoftRef();

		bool ret = m_pDevice->GetResourceManager()->AddWrapper(this, real);
		if(!ret)
			RDCERR("Error adding wrapper for type %s", ToStr::Get(__uuidof(NestedType)).c_str());

		m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);
	}

	virtual void Shutdown()
	{
		m_pDevice->GetResourceManager()->RemoveWrapper(m_pReal);
		m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());
		m_pDevice->ReleaseResource((NestedType*)this);
		SAFE_RELEASE(m_pReal);
		m_pDevice = NULL;
	}

	virtual ~WrappedDeviceChild()
	{
		// should have already called shutdown (needs to be called from child class to ensure
		// vtables are still in place when we call ReleaseResource)
		RDCASSERT(m_pDevice == NULL && m_pReal == NULL);
	}

public:
	typedef NestedType InnerType;

	NestedType* GetReal() { return m_pReal; }
	
	ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::SoftRef(m_pDevice) - m_PipelineRefs; }
	ULONG STDMETHODCALLTYPE Release()
	{
		unsigned int piperefs = m_PipelineRefs;
		return RefCounter::SoftRelease(m_pDevice) - piperefs;
	}
	
	void PipelineAddRef()
	{
		InterlockedIncrement(&m_PipelineRefs);
	}

	void PipelineRelease()
	{
		InterlockedDecrement(&m_PipelineRefs);
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		if(riid == __uuidof(NestedType))
		{
			*ppvObject = (NestedType *)this;
			AddRef();
			return S_OK;
		}
		if(riid == __uuidof(ID3D11DeviceChild))
		{
			*ppvObject = (ID3D11DeviceChild *)this;
			AddRef();
			return S_OK;
		}

		// for DXGI object queries, just make a new throw-away WrappedDXGIObject
		// and return.
		if(riid == __uuidof(IDXGIObject)
			|| riid == __uuidof(IDXGIDeviceSubObject)
			|| riid == __uuidof(IDXGIResource)
			|| riid == __uuidof(IDXGIKeyedMutex)
			|| riid == __uuidof(IDXGISurface)
			|| riid == __uuidof(IDXGISurface1)
#if defined(INCLUDE_D3D_11_1)
			|| riid == __uuidof(IDXGIResource1)
			|| riid == __uuidof(IDXGISurface2)
#endif
			)
		{
			// ensure the real object has this interface
			void *outObj;
			HRESULT hr = m_pReal->QueryInterface(riid, &outObj);
			
			IUnknown *unk = (IUnknown *)outObj;
			SAFE_RELEASE(unk);

			if(FAILED(hr))
			{
				return hr;
			}

			auto dxgiWrapper = new WrappedDXGIInterface< WrappedDeviceChild<NestedType> >(this, m_pDevice);

			// anything could happen outside of our wrapped ecosystem, so immediately mark dirty
			m_pDevice->GetResourceManager()->MarkDirtyResource(GetResourceID());

			if(riid == __uuidof(IDXGIObject))               *ppvObject = (IDXGIObject *)(IDXGIKeyedMutex *)dxgiWrapper;
			else if(riid == __uuidof(IDXGIDeviceSubObject)) *ppvObject = (IDXGIDeviceSubObject *)(IDXGIKeyedMutex *)dxgiWrapper;
			else if(riid == __uuidof(IDXGIResource))        *ppvObject = (IDXGIResource *)dxgiWrapper;
			else if(riid == __uuidof(IDXGIKeyedMutex))      *ppvObject = (IDXGIKeyedMutex *)dxgiWrapper;
			else if(riid == __uuidof(IDXGISurface))         *ppvObject = (IDXGISurface *)dxgiWrapper;
			else if(riid == __uuidof(IDXGISurface1))        *ppvObject = (IDXGISurface1 *)dxgiWrapper;
#if defined(INCLUDE_D3D_11_1)
			else if(riid == __uuidof(IDXGIResource1))       *ppvObject = (IDXGIResource1 *)dxgiWrapper;
			else if(riid == __uuidof(IDXGISurface2))        *ppvObject = (IDXGISurface2 *)dxgiWrapper;
#endif

			return S_OK;
		}

		return RefCounter::QueryInterface(riid, ppvObject);
	}

	//////////////////////////////
	// implement ID3D11DeviceChild

	void STDMETHODCALLTYPE GetDevice( 
		/* [annotation] */ 
		__out  ID3D11Device **ppDevice)
	{
		if(ppDevice)
		{
			*ppDevice = m_pDevice;
			m_pDevice->AddRef();
		}
	}

	HRESULT STDMETHODCALLTYPE GetPrivateData( 
		/* [annotation] */ 
		__in  REFGUID guid,
		/* [annotation] */ 
		__inout  UINT *pDataSize,
		/* [annotation] */ 
		__out_bcount_opt( *pDataSize )  void *pData)
	{
		return m_pReal->GetPrivateData(guid, pDataSize, pData);
	}

	HRESULT STDMETHODCALLTYPE SetPrivateData( 
		/* [annotation] */ 
		__in  REFGUID guid,
		/* [annotation] */ 
		__in  UINT DataSize,
		/* [annotation] */ 
		__in_bcount_opt( DataSize )  const void *pData)
	{
		if(guid == WKPDID_D3DDebugObjectName)
			m_pDevice->SetResourceName(this, (const char *)pData);
		return m_pReal->SetPrivateData(guid, DataSize, pData);
	}

	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface( 
		/* [annotation] */ 
		__in  REFGUID guid,
		/* [annotation] */ 
		__in_opt  const IUnknown *pData)
	{
		return m_pReal->SetPrivateDataInterface(guid, pData);
	}
};

template<typename NestedType, typename DescType>
class WrappedResource : public WrappedDeviceChild<NestedType>
{
private:
	unsigned int m_ViewRefcount; // refcount from views (invisible to the end-user)

protected:
#if !defined(RELEASE)
	DescType m_Desc;
#endif

	WrappedResource(NestedType* real, WrappedID3D11Device* device)
		:	WrappedDeviceChild(real, device),
			m_ViewRefcount(0)
	{
#if !defined(RELEASE)
		real->GetDesc(&m_Desc);
#endif

		// we'll handle deleting on release, so we can check against m_ViewRefcount
		RefCounter::SetSelfDeleting(false);
	}

	virtual void Shutdown()
	{
		WrappedDeviceChild::Shutdown();
	}

	virtual ~WrappedResource()
	{
	}

public:
	void ViewAddRef()
	{
		InterlockedIncrement(&m_ViewRefcount);

		RefCounter::AddDeviceSoftref(m_pDevice);
	}

	void ViewRelease()
	{
		unsigned int viewRefCount = InterlockedDecrement(&m_ViewRefcount);
		unsigned int extRefCount = RefCounter::GetRefCount();

		WrappedID3D11Device *dev = m_pDevice;

		if(extRefCount == 0 && m_ViewRefcount == 0)
			delete this;

		RefCounter::ReleaseDeviceSoftref(dev);
	}
	
	ULONG STDMETHODCALLTYPE AddRef()
	{
		return RefCounter::SoftRef(m_pDevice) - m_PipelineRefs;
	}

	ULONG STDMETHODCALLTYPE Release()
	{
		unsigned int extRefCount = RefCounter::Release();
		unsigned int pipeRefs = m_PipelineRefs;

		WrappedID3D11Device *dev = m_pDevice;

		if(extRefCount == 0 && m_ViewRefcount == 0)
			delete this;

		RefCounter::ReleaseDeviceSoftref(dev);

		return extRefCount - pipeRefs;
	}
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		if(riid == __uuidof(ID3D11Resource))
		{
			*ppvObject = (ID3D11Resource *)this;
			AddRef();
			return S_OK;
		}

		return WrappedDeviceChild<NestedType>::QueryInterface(riid, ppvObject);
	}

	//////////////////////////////
	// implement ID3D11Resource

	virtual void STDMETHODCALLTYPE GetType( 
		/* [annotation] */ 
		__out  D3D11_RESOURCE_DIMENSION *pResourceDimension)
	{
		m_pReal->GetType(pResourceDimension);
	}

	virtual void STDMETHODCALLTYPE SetEvictionPriority( 
		/* [annotation] */ 
		__in  UINT EvictionPriority)
	{
		m_pReal->SetEvictionPriority(EvictionPriority);
	}

	virtual UINT STDMETHODCALLTYPE GetEvictionPriority( void)
	{
		return m_pReal->GetEvictionPriority();
	}

	//////////////////////////////
	// implement NestedType

	virtual void STDMETHODCALLTYPE GetDesc( 
		/* [annotation] */ 
		__out  DescType *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11Buffer : public WrappedResource<ID3D11Buffer, D3D11_BUFFER_DESC>
{
public:
	struct BufferEntry
	{
		BufferEntry(WrappedID3D11Buffer *b = NULL, uint32_t l = 0) : m_Buffer(b), length(l) { }
		WrappedID3D11Buffer *m_Buffer;
		uint32_t length;
	};

	static map<ResourceId, BufferEntry> m_BufferList;
	
	static const int AllocPoolCount = 128*1024;
	static const int AllocPoolMaxByteSize = 13*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Buffer, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11Buffer(ID3D11Buffer* real, uint32_t byteLength, WrappedID3D11Device* device)
		: WrappedResource(real, device)
	{
		RDCASSERT(m_BufferList.find(GetResourceID()) == m_BufferList.end());
		m_BufferList[GetResourceID()] = BufferEntry(this, byteLength);
	}

	virtual ~WrappedID3D11Buffer()
	{
		if(m_BufferList.find(GetResourceID()) != m_BufferList.end())
			m_BufferList.erase(GetResourceID());

		Shutdown();
	}

	virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
	{
		if(pResourceDimension) *pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
	}
};

template<typename NestedType, typename DescType>
class WrappedTexture : public WrappedResource<NestedType, DescType>
{
public:
	struct TextureEntry
	{
		TextureEntry(NestedType *t = NULL, TextureDisplayType ty = TEXDISPLAY_UNKNOWN) : m_Texture(t), m_Type(ty) {}
		NestedType *m_Texture;
		TextureDisplayType m_Type;
	};

	static map<ResourceId, TextureEntry> m_TextureList;

	WrappedTexture(NestedType* real, WrappedID3D11Device* device, TextureDisplayType type)
		: WrappedResource(real, device)
	{
		if(type != TEXDISPLAY_UNKNOWN)
		{
			RDCASSERT(m_TextureList.find(GetResourceID()) == m_TextureList.end());
			m_TextureList[GetResourceID()] = TextureEntry(this, type);
		}
	}

	virtual ~WrappedTexture()
	{
		if(m_TextureList.find(GetResourceID()) != m_TextureList.end())
			m_TextureList.erase(GetResourceID());

		Shutdown();
	}
};

class WrappedID3D11Texture1D : public WrappedTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Texture1D);

	WrappedID3D11Texture1D(ID3D11Texture1D* real, WrappedID3D11Device* device, TextureDisplayType type = TEXDISPLAY_SRV_COMPATIBLE)
		: WrappedTexture(real, device, type) {}
	virtual ~WrappedID3D11Texture1D() {}

	virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
	{
		if(pResourceDimension) *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
	}
};

class WrappedID3D11Texture2D : public WrappedTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC>
{
public:
	static const int AllocPoolCount = 32768;
	static const int AllocPoolMaxByteSize = 4*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Texture2D, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11Texture2D(ID3D11Texture2D* real, WrappedID3D11Device* device, TextureDisplayType type = TEXDISPLAY_SRV_COMPATIBLE)
		: WrappedTexture(real, device, type)
	{
		m_RealDescriptor = NULL;
	}
	virtual ~WrappedID3D11Texture2D()
	{
		SAFE_DELETE(m_RealDescriptor);
	}

	// for backbuffer textures they behave a little differently from every other texture in D3D11
	// as they can be cast from one type to another, whereas normally you need to declare as typeless
	// and then cast to a type. To simulate this on our fake backbuffer textures I create them as
	// typeless, HOWEVER this means if we try to create a view with a NULL descriptor then we need
	// the real original type.
	D3D11_TEXTURE2D_DESC *m_RealDescriptor;

	virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
	{
		if(pResourceDimension) *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
	}
};

class WrappedID3D11Texture3D : public WrappedTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC>
{
public:
	static const int AllocPoolCount = 16384;
	static const int AllocPoolMaxByteSize = 2*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Texture3D, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11Texture3D(ID3D11Texture3D* real, WrappedID3D11Device* device, TextureDisplayType type = TEXDISPLAY_SRV_COMPATIBLE)
		: WrappedTexture(real, device, type) {}
	virtual ~WrappedID3D11Texture3D() {}

	virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
	{
		if(pResourceDimension) *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
	}
};

class WrappedID3D11InputLayout : public WrappedDeviceChild<ID3D11InputLayout>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11InputLayout);

	WrappedID3D11InputLayout(ID3D11InputLayout* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11InputLayout>(real, device) {}
	virtual ~WrappedID3D11InputLayout() { Shutdown(); }
};

class WrappedID3D11RasterizerState : public WrappedDeviceChild<ID3D11RasterizerState>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11RasterizerState);

	WrappedID3D11RasterizerState(ID3D11RasterizerState* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11RasterizerState>(real, device) {}
	virtual ~WrappedID3D11RasterizerState() { Shutdown(); }

	//////////////////////////////
	// implement ID3D11RasterizerState

	virtual void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11BlendState : public WrappedDeviceChild<ID3D11BlendState>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11BlendState);

	WrappedID3D11BlendState(ID3D11BlendState* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11BlendState>(real, device) {}
	virtual ~WrappedID3D11BlendState() { Shutdown(); }

	//////////////////////////////
	// implement ID3D11BlendState

	virtual void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11DepthStencilState : public WrappedDeviceChild<ID3D11DepthStencilState>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DepthStencilState);

	WrappedID3D11DepthStencilState(ID3D11DepthStencilState* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11DepthStencilState>(real, device) {}
	virtual ~WrappedID3D11DepthStencilState() { Shutdown(); }

	//////////////////////////////
	// implement ID3D11DepthStencilState

	virtual void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11SamplerState : public WrappedDeviceChild<ID3D11SamplerState>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11SamplerState);

	WrappedID3D11SamplerState(ID3D11SamplerState* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11SamplerState>(real, device) {}
	virtual ~WrappedID3D11SamplerState() { Shutdown(); }

	//////////////////////////////
	// implement ID3D11SamplerState

	virtual void STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

template<typename NestedType, typename DescType>
class WrappedView : public WrappedDeviceChild<NestedType>
{
protected:
	ID3D11Resource *m_pResource;
	ResourceId m_ResourceResID;

	WrappedView(NestedType* real, WrappedID3D11Device* device, ID3D11Resource* res)
		:	WrappedDeviceChild(real, device),
			m_pResource(res)
	{
		m_ResourceResID = GetIDForResource(m_pResource);
		// cast is potentially invalid but functions in WrappedResource will be identical across each
		((WrappedID3D11Buffer *)m_pResource)->ViewAddRef();
	}

	virtual void Shutdown()
	{
		WrappedDeviceChild::Shutdown();
		// cast is potentially invalid but functions in WrappedResource will be identical across each
		((WrappedID3D11Buffer *)m_pResource)->ViewRelease();
		m_pResource = NULL;
	}

	virtual ~WrappedView()
	{
	}

public:
	ResourceId GetResourceResID() { return m_ResourceResID; }
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		if(riid == __uuidof(ID3D11View))
		{
			*ppvObject = (ID3D11View *)this;
			AddRef();
			return S_OK;
		}

		return WrappedDeviceChild<NestedType>::QueryInterface(riid, ppvObject);
	}

	//////////////////////////////
	// implement ID3D11View

	void STDMETHODCALLTYPE GetResource( 
		/* [annotation] */ 
		__out  ID3D11Resource **pResource)
	{
		RDCASSERT(m_pResource);
		if(pResource)
			*pResource = m_pResource;
		m_pResource->AddRef();
	}

	//////////////////////////////
	// implement NestedType

	void STDMETHODCALLTYPE GetDesc( 
		/* [annotation] */ 
		__out  DescType *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11RenderTargetView : public WrappedView<ID3D11RenderTargetView, D3D11_RENDER_TARGET_VIEW_DESC>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11RenderTargetView);

	WrappedID3D11RenderTargetView(ID3D11RenderTargetView* real, ID3D11Resource* res, WrappedID3D11Device* device)
		: WrappedView(real, device, res) {}
	virtual ~WrappedID3D11RenderTargetView() { Shutdown(); }
};

class WrappedID3D11ShaderResourceView : public WrappedView<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC>
{
public:
	static const int AllocPoolCount = 65535;
	static const int AllocPoolMaxByteSize = 6*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11ShaderResourceView, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11ShaderResourceView(ID3D11ShaderResourceView* real, ID3D11Resource* res, WrappedID3D11Device* device)
		: WrappedView(real, device, res) {}
	virtual ~WrappedID3D11ShaderResourceView() { Shutdown(); }
};

class WrappedID3D11DepthStencilView : public WrappedView<ID3D11DepthStencilView, D3D11_DEPTH_STENCIL_VIEW_DESC>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DepthStencilView);

	WrappedID3D11DepthStencilView(ID3D11DepthStencilView* real, ID3D11Resource* res, WrappedID3D11Device* device)
		: WrappedView(real, device, res) {}
	virtual ~WrappedID3D11DepthStencilView() { Shutdown(); }
};

class WrappedID3D11UnorderedAccessView : public WrappedView<ID3D11UnorderedAccessView, D3D11_UNORDERED_ACCESS_VIEW_DESC>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11UnorderedAccessView);

	WrappedID3D11UnorderedAccessView(ID3D11UnorderedAccessView* real, ID3D11Resource* res, WrappedID3D11Device* device)
		: WrappedView(real, device, res) {}
	virtual ~WrappedID3D11UnorderedAccessView() { Shutdown(); }
};

class WrappedShader
{
public:
	class ShaderEntry
	{
		public:
			ShaderEntry() : m_DXBCFile(NULL), m_Details(NULL) {}
			ShaderEntry(DXBC::DXBCFile *file)
			{
				m_DXBCFile = file;
				m_Details = MakeShaderReflection(m_DXBCFile);
			}
			~ShaderEntry()
			{
				SAFE_DELETE(m_DXBCFile);
				SAFE_DELETE(m_Details);
			}

			DXBC::DXBCFile *GetDXBC() { return m_DXBCFile; }
			ShaderReflection *GetDetails() { return m_Details; }
		private:
			ShaderEntry(const ShaderEntry &e);
			ShaderEntry &operator =(const ShaderEntry &e);

			DXBC::DXBCFile *m_DXBCFile;
			ShaderReflection *m_Details;
	};

	static map<ResourceId, ShaderEntry*> m_ShaderList;

	WrappedShader(ResourceId id, DXBC::DXBCFile *file) : m_ID(id)
	{
		RDCASSERT(m_ShaderList.find(m_ID) == m_ShaderList.end());
		m_ShaderList[m_ID] = new ShaderEntry(file);
	}
	virtual ~WrappedShader()
	{
		auto it = m_ShaderList.find(m_ID);
		if(it != m_ShaderList.end())
		{
			delete it->second;
			m_ShaderList.erase(it);
		}
	}

	DXBC::DXBCFile *GetDXBC() { return m_ShaderList[m_ID]->GetDXBC(); }
	ShaderReflection *GetDetails() { return m_ShaderList[m_ID]->GetDetails(); }
private:
	ResourceId m_ID;
};

template<class RealShaderType>
class WrappedID3D11Shader : public WrappedDeviceChild<RealShaderType>, public WrappedShader
{
public:
	static const int AllocPoolCount = 32*1024;
	static const int AllocPoolMaxByteSize = 3*1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Shader<RealShaderType>, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11Shader(RealShaderType* real, DXBC::DXBCFile *file, WrappedID3D11Device* device)
		: WrappedDeviceChild<RealShaderType>(real, device), WrappedShader(GetResourceID(), file) {}
	virtual ~WrappedID3D11Shader() { Shutdown(); }
};

class WrappedID3D11Counter : public WrappedDeviceChild<ID3D11Counter>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Counter);

	WrappedID3D11Counter(ID3D11Counter* real, WrappedID3D11Device* device)
		: WrappedDeviceChild(real, device) {}
	virtual ~WrappedID3D11Counter() { Shutdown(); }

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		if(riid == __uuidof(ID3D11Asynchronous))
		{
			*ppvObject = (ID3D11Asynchronous *)this;
			AddRef();
			return S_OK;
		}

		return WrappedDeviceChild<ID3D11Counter>::QueryInterface(riid, ppvObject);
	}

	//////////////////////////////
	// implement ID3D11Asynchronous

	UINT STDMETHODCALLTYPE GetDataSize( void)
	{
		return m_pReal->GetDataSize();
	}

	//////////////////////////////
	// implement ID3D11Counter

	void STDMETHODCALLTYPE GetDesc(__out D3D11_COUNTER_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11Query : public WrappedDeviceChild<ID3D11Query>
{
public:
	static const int AllocPoolCount = 16*1024;
	static const int AllocPoolMaxByteSize = 1024*1024;
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Query, AllocPoolCount, AllocPoolMaxByteSize);

	WrappedID3D11Query(ID3D11Query* real, WrappedID3D11Device* device)
		: WrappedDeviceChild(real, device) {}
	virtual ~WrappedID3D11Query() { Shutdown(); }
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		if(riid == __uuidof(ID3D11Asynchronous))
		{
			*ppvObject = (ID3D11Asynchronous *)this;
			AddRef();
			return S_OK;
		}

		return WrappedDeviceChild<ID3D11Query>::QueryInterface(riid, ppvObject);
	}

	//////////////////////////////
	// implement ID3D11Asynchronous

	UINT STDMETHODCALLTYPE GetDataSize( void)
	{
		return m_pReal->GetDataSize();
	}

	//////////////////////////////
	// implement ID3D11Query

	void STDMETHODCALLTYPE GetDesc(__out D3D11_QUERY_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11Predicate : public WrappedDeviceChild<ID3D11Predicate>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Predicate);

	WrappedID3D11Predicate(ID3D11Predicate* real, WrappedID3D11Device* device)
		: WrappedDeviceChild(real, device) {}
	virtual ~WrappedID3D11Predicate() { Shutdown(); }
	
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
	{
		if(riid == __uuidof(ID3D11Asynchronous))
		{
			*ppvObject = (ID3D11Asynchronous *)this;
			AddRef();
			return S_OK;
		}

		return WrappedDeviceChild<ID3D11Predicate>::QueryInterface(riid, ppvObject);
	}

	//////////////////////////////
	// implement ID3D11Asynchronous

	UINT STDMETHODCALLTYPE GetDataSize( void)
	{
		return m_pReal->GetDataSize();
	}

	//////////////////////////////
	// implement ID3D11Query

	void STDMETHODCALLTYPE GetDesc(__out D3D11_QUERY_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}
};

class WrappedID3D11ClassInstance : public WrappedDeviceChild<ID3D11ClassInstance>
{
private:
	ID3D11ClassLinkage *m_pLinkage;
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11ClassInstance);

	WrappedID3D11ClassInstance(ID3D11ClassInstance* real, ID3D11ClassLinkage *linkage, WrappedID3D11Device* device)
		: WrappedDeviceChild(real, device), m_pLinkage(linkage) { SAFE_ADDREF(m_pLinkage);}
	virtual ~WrappedID3D11ClassInstance() { SAFE_RELEASE(m_pLinkage); Shutdown(); }
	
	//////////////////////////////
	// implement ID3D11ClassInstance

	virtual void STDMETHODCALLTYPE GetClassLinkage( 
		/* [annotation] */ 
		__out  ID3D11ClassLinkage **ppLinkage)
	{ if(ppLinkage) { SAFE_ADDREF(m_pLinkage); *ppLinkage = m_pLinkage; } }

	virtual void STDMETHODCALLTYPE GetDesc( 
		/* [annotation] */ 
		__out  D3D11_CLASS_INSTANCE_DESC *pDesc)
	{ m_pReal->GetDesc(pDesc); }

	virtual void STDMETHODCALLTYPE GetInstanceName( 
		/* [annotation] */ 
		__out_ecount_opt(*pBufferLength)  LPSTR pInstanceName,
		/* [annotation] */ 
		__inout  SIZE_T *pBufferLength)
	{ m_pReal->GetInstanceName(pInstanceName, pBufferLength); }

	virtual void STDMETHODCALLTYPE GetTypeName( 
		/* [annotation] */ 
		__out_ecount_opt(*pBufferLength)  LPSTR pTypeName,
		/* [annotation] */ 
		__inout  SIZE_T *pBufferLength)
	{ m_pReal->GetTypeName(pTypeName, pBufferLength); }
};


class WrappedID3D11ClassLinkage : public WrappedDeviceChild<ID3D11ClassLinkage>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11ClassLinkage);

	WrappedID3D11ClassLinkage(ID3D11ClassLinkage* real, WrappedID3D11Device* device)
		: WrappedDeviceChild(real, device) { }
	virtual ~WrappedID3D11ClassLinkage() { Shutdown(); }
	
	//////////////////////////////
	// implement ID3D11ClassLinkage

	virtual HRESULT STDMETHODCALLTYPE GetClassInstance( 
		/* [annotation] */ 
		__in  LPCSTR pClassInstanceName,
		/* [annotation] */ 
		__in  UINT InstanceIndex,
		/* [annotation] */ 
		__out  ID3D11ClassInstance **ppInstance)
	{
		if(ppInstance == NULL) return E_INVALIDARG;

		ID3D11ClassInstance *real = NULL;
		HRESULT hr = m_pReal->GetClassInstance(pClassInstanceName, InstanceIndex, &real);

		if(SUCCEEDED(hr) && real)
		{
			*ppInstance = m_pDevice->GetClassInstance(pClassInstanceName, InstanceIndex, this, real);
		}
		else
		{
			SAFE_RELEASE(real);
		}

		return hr;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateClassInstance( 
		/* [annotation] */ 
		__in  LPCSTR pClassTypeName,
		/* [annotation] */ 
		__in  UINT ConstantBufferOffset,
		/* [annotation] */ 
		__in  UINT ConstantVectorOffset,
		/* [annotation] */ 
		__in  UINT TextureOffset,
		/* [annotation] */ 
		__in  UINT SamplerOffset,
		/* [annotation] */ 
		__out  ID3D11ClassInstance **ppInstance)
	{
		if(ppInstance == NULL) return E_INVALIDARG;

		ID3D11ClassInstance *real = NULL;
		HRESULT hr = m_pReal->CreateClassInstance(pClassTypeName, ConstantBufferOffset, ConstantVectorOffset, TextureOffset, SamplerOffset, &real);

		if(SUCCEEDED(hr) && real)
		{
			*ppInstance = m_pDevice->CreateClassInstance(pClassTypeName, ConstantBufferOffset, ConstantVectorOffset, TextureOffset, SamplerOffset, this, real);
		}
		else
		{
			SAFE_RELEASE(real);
		}

		return hr;
	}
};

class WrappedID3D11DeviceContext;

class WrappedID3D11CommandList : public WrappedDeviceChild<ID3D11CommandList>
{
	WrappedID3D11DeviceContext* m_pContext;
	bool m_Successful; // indicates whether we have all of the commands serialised for this command list
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11CommandList);

	WrappedID3D11CommandList(ID3D11CommandList* real, WrappedID3D11Device* device,
							 WrappedID3D11DeviceContext* context, bool success)
		: WrappedDeviceChild(real, device), m_pContext(context), m_Successful(success)
	{
		// context isn't defined type at this point.
	}
	virtual ~WrappedID3D11CommandList()
	{
		// context isn't defined type at this point.
		Shutdown();
	}

	WrappedID3D11DeviceContext *GetContext() { return m_pContext; }
	bool IsCaptured() { return m_Successful; }
	
	//////////////////////////////
	// implement ID3D11CommandList
	
	virtual UINT STDMETHODCALLTYPE GetContextFlags( void)
	{
		return m_pReal->GetContextFlags();
	}
};

#if defined(INCLUDE_D3D_11_1)
class WrappedID3D11RasterizerState1 : public WrappedDeviceChild<ID3D11RasterizerState1>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11RasterizerState1);

	WrappedID3D11RasterizerState1(ID3D11RasterizerState1* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11RasterizerState1>(real, device) {}
	virtual ~WrappedID3D11RasterizerState1() { Shutdown(); }

	//////////////////////////////
	// implement ID3D11RasterizerStat1

	virtual void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}

	//////////////////////////////
	// implement ID3D11RasterizerState1

	virtual void STDMETHODCALLTYPE GetDesc1(D3D11_RASTERIZER_DESC1 *pDesc)
	{
		m_pReal->GetDesc1(pDesc);
	}
};

class WrappedID3D11BlendState1 : public WrappedDeviceChild<ID3D11BlendState1>
{
public:
	ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11BlendState1);

	WrappedID3D11BlendState1(ID3D11BlendState1* real, WrappedID3D11Device* device)
		: WrappedDeviceChild<ID3D11BlendState1>(real, device) {}
	virtual ~WrappedID3D11BlendState1() { Shutdown(); }

	//////////////////////////////
	// implement ID3D11BlendState

	virtual void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC *pDesc)
	{
		m_pReal->GetDesc(pDesc);
	}

	//////////////////////////////
	// implement ID3D11BlendState1

	virtual void STDMETHODCALLTYPE GetDesc1(D3D11_BLEND_DESC1 *pDesc)
	{
		m_pReal->GetDesc1(pDesc);
	}
};
#endif
