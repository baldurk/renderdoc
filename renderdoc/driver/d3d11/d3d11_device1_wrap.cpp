/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device1 interface

void WrappedID3D11Device::GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext)
{
  if(m_pDevice1 == NULL)
    return;

  if(ppImmediateContext)
  {
    m_pImmediateContext->AddRef();
    *ppImmediateContext = (ID3D11DeviceContext1 *)m_pImmediateContext;
  }
}

HRESULT WrappedID3D11Device::CreateDeferredContext1(UINT ContextFlags,
                                                    ID3D11DeviceContext1 **ppDeferredContext)
{
  if(m_pDevice1 == NULL)
    return E_NOINTERFACE;
  if(ppDeferredContext == NULL)
    return m_pDevice1->CreateDeferredContext1(ContextFlags, NULL);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateBlendState1(SerialiserType &ser,
                                                      const D3D11_BLEND_DESC1 *pBlendStateDesc,
                                                      ID3D11BlendState1 **ppBlendState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pBlendStateDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppBlendState))
      .TypedAs("ID3D11BlendState1 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11BlendState1 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice1)
      hr = m_pDevice1->CreateBlendState1(&Descriptor, &ret);
    else
      RDCERR("Replaying a D3D11.1 device without D3D11.1 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11BlendState1 *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(pState, ret);
      }
      else
      {
        ret = new WrappedID3D11BlendState1(ret, this);

        GetResourceManager()->AddLiveResource(pState, ret);
      }
    }

    AddResource(pState, ResourceType::StateObject, "Blend State");
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
                                               ID3D11BlendState1 **ppBlendState)
{
  if(m_pDevice1 == NULL)
    return E_NOINTERFACE;
  if(ppBlendState == NULL)
    return m_pDevice1->CreateBlendState1(pBlendStateDesc, NULL);

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

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateBlendState1);
      Serialise_CreateBlendState1(GET_SERIALISER, pBlendStateDesc, &wrapped);

      WrappedID3D11BlendState1 *st = (WrappedID3D11BlendState1 *)wrapped;
      ResourceId id = st->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppBlendState = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateRasterizerState1(
    SerialiserType &ser, const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
    ID3D11RasterizerState1 **ppRasterizerState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pRasterizerDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppRasterizerState))
      .TypedAs("ID3D11RasterizerState1 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11RasterizerState1 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice1)
      hr = m_pDevice1->CreateRasterizerState1(&Descriptor, &ret);
    else
      RDCERR("Replaying a D3D11.1 device without D3D11.1 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11RasterizerState1 *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(pState, ret);
      }
      else
      {
        ret = new WrappedID3D11RasterizerState2(ret, this);

        GetResourceManager()->AddLiveResource(pState, ret);
      }
    }

    AddResource(pState, ResourceType::StateObject, "Rasterizer State");
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
                                                    ID3D11RasterizerState1 **ppRasterizerState)
{
  if(m_pDevice1 == NULL)
    return E_NOINTERFACE;
  if(ppRasterizerState == NULL)
    return m_pDevice1->CreateRasterizerState1(pRasterizerDesc, NULL);

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

    ID3D11RasterizerState1 *wrapped = new WrappedID3D11RasterizerState2(real, this);

    CachedObjectsGarbageCollect();

    {
      RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
      wrapped->AddRef();
      InternalRef();
      m_CachedStateObjects.insert(wrapped);
    }

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateRasterizerState1);
      Serialise_CreateRasterizerState1(GET_SERIALISER, pRasterizerDesc, &wrapped);

      WrappedID3D11RasterizerState2 *st = (WrappedID3D11RasterizerState2 *)wrapped;
      ResourceId id = st->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppRasterizerState = wrapped;
  }

  return ret;
}

HRESULT WrappedID3D11Device::CreateDeviceContextState(UINT Flags,
                                                      const D3D_FEATURE_LEVEL *pFeatureLevels,
                                                      UINT FeatureLevels, UINT SDKVersion,
                                                      REFIID EmulatedInterface,
                                                      D3D_FEATURE_LEVEL *pChosenFeatureLevel,
                                                      ID3DDeviceContextState **ppContextState)
{
  if(m_pDevice1 == NULL)
    return E_NOINTERFACE;

  if(ppContextState == NULL)
    return m_pDevice1->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                                                EmulatedInterface, pChosenFeatureLevel, NULL);

  ID3DDeviceContextState *real = NULL;
  HRESULT ret = m_pDevice1->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                                                     EmulatedInterface, pChosenFeatureLevel, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3DDeviceContextState *wrapped = new WrappedID3DDeviceContextState(real, this);

    wrapped->state->CopyState(*m_pImmediateContext->GetCurrentPipelineState());

    *ppContextState = wrapped;
  }

  return ret;
}

HRESULT WrappedID3D11Device::OpenSharedResource1(HANDLE hResource, REFIID returnedInterface,
                                                 void **ppResource)
{
  if(m_pDevice1 == NULL)
    return E_NOINTERFACE;
  RDCUNIMPLEMENTED("Not wrapping OpenSharedResource1");
  return m_pDevice1->OpenSharedResource1(hResource, returnedInterface, ppResource);
}

HRESULT WrappedID3D11Device::OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess,
                                                      REFIID returnedInterface, void **ppResource)
{
  if(m_pDevice1 == NULL)
    return E_NOINTERFACE;
  RDCUNIMPLEMENTED("Not wrapping OpenSharedResourceByName");
  return m_pDevice1->OpenSharedResourceByName(lpName, dwDesiredAccess, returnedInterface, ppResource);
}

#undef IMPLEMENT_FUNCTION_SERIALISED
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...)                                            \
  template bool WrappedID3D11Device::CONCAT(Serialise_, func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool WrappedID3D11Device::CONCAT(Serialise_, func(WriteSerialiser &ser, __VA_ARGS__));

SERIALISED_ID3D11DEVICE1_FUNCTIONS();