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
#include "d3d11_resources.h"
#include "d3d11_context.h"

#if defined(INCLUDE_D3D_11_1)
///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device1 interface

void WrappedID3D11Device::GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext)
{
	if(m_pDevice1 == NULL) return;

	if(ppImmediateContext)
	{
		m_pImmediateContext->AddRef();
		*ppImmediateContext = (ID3D11DeviceContext1 *)m_pImmediateContext;
	}
}

HRESULT WrappedID3D11Device::CreateDeferredContext1(UINT ContextFlags, ID3D11DeviceContext1 **ppDeferredContext)
{
	if(m_pDevice1 == NULL) return E_NOINTERFACE;
	if(ppDeferredContext == NULL) return m_pDevice1->CreateDeferredContext1(ContextFlags, NULL);

	ID3D11DeviceContext *defCtx = NULL;
	HRESULT ret = CreateDeferredContext(ContextFlags, &defCtx);

	if(SUCCEEDED(ret))
	{
		WrappedID3D11DeviceContext *wrapped = (WrappedID3D11DeviceContext *)defCtx;
		*ppDeferredContext = (ID3D11DeviceContext1 *)wrapped;
	}
	else
	{
		SAFE_RELEASE(defCtx);
	}

	return ret;
}

bool WrappedID3D11Device::Serialise_CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc, ID3D11BlendState1 **ppBlendState)
{
	SERIALISE_ELEMENT_PTR(D3D11_BLEND_DESC1, Descriptor, pBlendStateDesc);
	SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppBlendState));
	
	if(m_State == READING)
	{
		ID3D11BlendState1 *ret = NULL;
		HRESULT hr = E_NOINTERFACE;
		
		if(m_pDevice1)
			m_pDevice1->CreateBlendState1(&Descriptor, &ret);
		else
			RDCERR("Replaying a D3D11.1 device without D3D11.1 available");

		if(FAILED(hr))
		{
			RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
		}
		else
		{
			ret = new WrappedID3D11BlendState1(ret, this);

			GetResourceManager()->AddLiveResource(State, ret);
		}
	}

	return true;
}

HRESULT WrappedID3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc, ID3D11BlendState1 **ppBlendState)
{
	if(m_pDevice1 == NULL) return E_NOINTERFACE;
	if(ppBlendState == NULL) return m_pDevice1->CreateBlendState1(pBlendStateDesc, NULL);

	ID3D11BlendState1 *real = NULL;
	HRESULT ret = m_pDevice1->CreateBlendState1(pBlendStateDesc, &real);
	
	if(SUCCEEDED(ret))
	{
		SCOPED_LOCK(m_D3DLock);
		
		// duplicate states can be returned, if Create is called with a previous descriptor
		if(GetResourceManager()->HasWrapper(real))
		{
			real->Release();
			*ppBlendState = (ID3D11BlendState1 *)GetResourceManager()->GetWrapper(real);
			(*ppBlendState)->AddRef();
			return ret;
		}
		
		ID3D11BlendState1 *wrapped = new WrappedID3D11BlendState1(real, this);

		CachedObjectsGarbageCollect();

		{
			RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
			wrapped->AddRef();
			InternalRef();
			m_CachedStateObjects.insert(wrapped);
		}

		if(m_State >= WRITING)
		{
			SCOPED_SERIALISE_CONTEXT(CREATE_BLEND_STATE1);
			Serialise_CreateBlendState1(pBlendStateDesc, &wrapped);

			m_DeviceRecord->AddChunk(scope.Get());
		}

		*ppBlendState = wrapped;
	}

	return ret;
}

bool WrappedID3D11Device::Serialise_CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc, ID3D11RasterizerState1 **ppRasterizerState)
{
	SERIALISE_ELEMENT_PTR(D3D11_RASTERIZER_DESC1, Descriptor, pRasterizerDesc);
	SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppRasterizerState));
	
	if(m_State == READING)
	{
		ID3D11RasterizerState1 *ret = NULL;
		HRESULT hr = E_NOINTERFACE;
		
		if(m_pDevice1)
			m_pDevice1->CreateRasterizerState1(&Descriptor, &ret);
		else
			RDCERR("Replaying a D3D11.1 device without D3D11.1 available");

		if(FAILED(hr))
		{
			RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
		}
		else
		{		
			ret = new WrappedID3D11RasterizerState1(ret, this);

			GetResourceManager()->AddLiveResource(State, ret);
		}
	}

	return true;
}

HRESULT WrappedID3D11Device::CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc, ID3D11RasterizerState1 **ppRasterizerState)
{
	if(m_pDevice1 == NULL) return E_NOINTERFACE;
	if(ppRasterizerState == NULL) return m_pDevice1->CreateRasterizerState1(pRasterizerDesc, NULL);

	ID3D11RasterizerState1 *real = NULL;
	HRESULT ret = m_pDevice1->CreateRasterizerState1(pRasterizerDesc, &real);

	if(SUCCEEDED(ret))
	{
		SCOPED_LOCK(m_D3DLock);
		
		// duplicate states can be returned, if Create is called with a previous descriptor
		if(GetResourceManager()->HasWrapper(real))
		{
			real->Release();
			*ppRasterizerState = (ID3D11RasterizerState1 *)GetResourceManager()->GetWrapper(real);
			(*ppRasterizerState)->AddRef();
			return ret;
		}
		
		ID3D11RasterizerState1 *wrapped = new WrappedID3D11RasterizerState1(real, this);

		CachedObjectsGarbageCollect();

		{
			RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
			wrapped->AddRef();
			InternalRef();
			m_CachedStateObjects.insert(wrapped);
		}

		if(m_State >= WRITING)
		{
			SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE1);
			Serialise_CreateRasterizerState1(pRasterizerDesc, &wrapped);

			m_DeviceRecord->AddChunk(scope.Get());
		}

		*ppRasterizerState = wrapped;
	}

	return ret;
}

HRESULT WrappedID3D11Device::CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, REFIID EmulatedInterface,
														D3D_FEATURE_LEVEL *pChosenFeatureLevel, ID3DDeviceContextState **ppContextState)
{
	if(m_pDevice1 == NULL) return E_NOINTERFACE;
	RDCUNIMPLEMENTED("Not wrapping CreateDeviceContextState");
	return m_pDevice1->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion, EmulatedInterface, pChosenFeatureLevel, ppContextState);
}

HRESULT WrappedID3D11Device::OpenSharedResource1(HANDLE hResource, REFIID returnedInterface, void **ppResource)
{
	if(m_pDevice1 == NULL) return E_NOINTERFACE;
	RDCUNIMPLEMENTED("Not wrapping OpenSharedResource1");
	return m_pDevice1->OpenSharedResource1(hResource, returnedInterface, ppResource);
}

HRESULT WrappedID3D11Device::OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface, void **ppResource)
{
	if(m_pDevice1 == NULL) return E_NOINTERFACE;
	RDCUNIMPLEMENTED("Not wrapping OpenSharedResourceByName");
	return m_pDevice1->OpenSharedResourceByName(lpName, dwDesiredAccess, returnedInterface, ppResource);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device2 interface

void WrappedID3D11Device::GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext)
{
	if(m_pDevice2 == NULL) return;

	if(ppImmediateContext)
	{
		m_pImmediateContext->AddRef();
		*ppImmediateContext = (ID3D11DeviceContext2 *)m_pImmediateContext;
	}
}

HRESULT WrappedID3D11Device::CreateDeferredContext2(UINT ContextFlags, ID3D11DeviceContext2 **ppDeferredContext)
{
	if(m_pDevice2 == NULL) return E_NOINTERFACE;
	if(ppDeferredContext == NULL) return m_pDevice2->CreateDeferredContext2(ContextFlags, NULL);

	ID3D11DeviceContext *defCtx = NULL;
	HRESULT ret = CreateDeferredContext(ContextFlags, &defCtx);

	if(SUCCEEDED(ret))
	{
		WrappedID3D11DeviceContext *wrapped = (WrappedID3D11DeviceContext *)defCtx;
		*ppDeferredContext = (ID3D11DeviceContext2 *)wrapped;
	}
	else
	{
		SAFE_RELEASE(defCtx);
	}

	return ret;
}
	
void WrappedID3D11Device::GetResourceTiling( 
		ID3D11Resource *pTiledResource,
		UINT *pNumTilesForEntireResource,
		D3D11_PACKED_MIP_DESC *pPackedMipDesc,
		D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
		UINT *pNumSubresourceTilings,
		UINT FirstSubresourceTilingToGet,
		D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
	if(m_pDevice2 == NULL) return;

	m_pDevice2->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips,
	                              pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}

HRESULT WrappedID3D11Device::CheckMultisampleQualityLevels1( 
		DXGI_FORMAT Format,
		UINT SampleCount,
		UINT Flags,
		UINT *pNumQualityLevels)
{
	if(m_pDevice2 == NULL) return E_NOINTERFACE;

	return m_pDevice2->CheckMultisampleQualityLevels1(Format, SampleCount, Flags, pNumQualityLevels);
}

#endif