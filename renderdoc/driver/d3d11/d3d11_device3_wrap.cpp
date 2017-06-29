/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "d3d11_resources.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device3 interface

bool WrappedID3D11Device::Serialise_CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *pDesc,
                                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                     ID3D11Texture2D1 **ppTexture2D)
{
  SERIALISE_ELEMENT_PTR(D3D11_TEXTURE2D_DESC1, Descriptor, pDesc);
  SERIALISE_ELEMENT(ResourceId, pTexture, GetIDForResource(*ppTexture2D));

  SERIALISE_ELEMENT(bool, HasInitialData, pInitialData != NULL);

  vector<D3D11_SUBRESOURCE_DATA> descs = Serialise_CreateTextureData(
      ppTexture2D ? *ppTexture2D : NULL, pTexture, pInitialData, Descriptor.Width, Descriptor.Height,
      1, Descriptor.Format, Descriptor.MipLevels, Descriptor.ArraySize, HasInitialData);

  if(m_State == READING)
  {
    ID3D11Texture2D1 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(m_pDevice3)
    {
      if(HasInitialData)
        hr = m_pDevice3->CreateTexture2D1(&Descriptor, &descs[0], &ret);
      else
        hr = m_pDevice3->CreateTexture2D1(&Descriptor, NULL, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Texture2D1((ID3D11Texture2D1 *)ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }
  }

  for(size_t i = 0; i < descs.size(); i++)
    SAFE_DELETE_ARRAY(descs[i].pSysMem);

  return true;
}

HRESULT WrappedID3D11Device::CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *pDesc1,
                                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                                              ID3D11Texture2D1 **ppTexture2D)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture2D == NULL)
    return m_pDevice3->CreateTexture2D1(pDesc1, pInitialData, NULL);

  ID3D11Texture2D1 *real = NULL;
  ID3D11Texture2D1 *wrapped = NULL;
  HRESULT ret = m_pDevice3->CreateTexture2D1(pDesc1, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture2D1((ID3D11Texture2D *)real, this);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_TEXTURE_2D1);
        Serialise_CreateTexture2D1(pDesc1, pInitialData, &wrapped);

        chunk = scope.Get();
      }

      D3D11ResourceRecord *record =
          GetResourceManager()->GetResourceRecord(GetIDForResource(wrapped));
      RDCASSERT(record);

      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
    }
    else
    {
      WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppTexture2D = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc,
                                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                     ID3D11Texture3D1 **ppTexture3D)
{
  SERIALISE_ELEMENT_PTR(D3D11_TEXTURE3D_DESC1, Descriptor, pDesc);
  SERIALISE_ELEMENT(ResourceId, pTexture, GetIDForResource(*ppTexture3D));

  SERIALISE_ELEMENT(bool, HasInitialData, pInitialData != NULL);

  vector<D3D11_SUBRESOURCE_DATA> descs = Serialise_CreateTextureData(
      ppTexture3D ? *ppTexture3D : NULL, pTexture, pInitialData, Descriptor.Width, Descriptor.Height,
      Descriptor.Depth, Descriptor.Format, Descriptor.MipLevels, 1, HasInitialData);

  if(m_State == READING)
  {
    ID3D11Texture3D1 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(m_pDevice3)
    {
      if(HasInitialData)
        hr = m_pDevice3->CreateTexture3D1(&Descriptor, &descs[0], &ret);
      else
        hr = m_pDevice3->CreateTexture3D1(&Descriptor, NULL, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Texture3D1((ID3D11Texture3D1 *)ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }
  }

  for(size_t i = 0; i < descs.size(); i++)
    SAFE_DELETE_ARRAY(descs[i].pSysMem);

  return true;
}

HRESULT WrappedID3D11Device::CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc1,
                                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                                              ID3D11Texture3D1 **ppTexture3D)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture3D == NULL)
    return m_pDevice3->CreateTexture3D1(pDesc1, pInitialData, NULL);

  ID3D11Texture3D1 *real = NULL;
  ID3D11Texture3D1 *wrapped = NULL;
  HRESULT ret = m_pDevice3->CreateTexture3D1(pDesc1, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture3D1((ID3D11Texture3D *)real, this);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_TEXTURE_3D1);
        Serialise_CreateTexture3D1(pDesc1, pInitialData, &wrapped);

        chunk = scope.Get();
      }

      D3D11ResourceRecord *record =
          GetResourceManager()->GetResourceRecord(GetIDForResource(wrapped));
      RDCASSERT(record);

      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
    }
    else
    {
      WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppTexture3D = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateShaderResourceView1(
    ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
    ID3D11ShaderResourceView1 **ppSRView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_SHADER_RESOURCE_VIEW_DESC1, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppSRView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11ShaderResourceView1 *ret = NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC1 *pSRVDesc = NULL;
    if(HasDesc)
      pSRVDesc = &Descriptor;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)live;

    D3D11_SHADER_RESOURCE_VIEW_DESC1 backbufferTypedDesc = {};

    // need to fixup typeless backbuffer fudging, if a descriptor isn't specified then
    // we need to make one to give the correct type
    if(!HasDesc && WrappedID3D11Texture2D1::IsAlloc(live) && tex2d->m_RealDescriptor)
    {
      backbufferTypedDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

      if(tex2d->m_RealDescriptor->SampleDesc.Quality > 0 ||
         tex2d->m_RealDescriptor->SampleDesc.Count > 1)
        backbufferTypedDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

      backbufferTypedDesc.Format = tex2d->m_RealDescriptor->Format;
      backbufferTypedDesc.Texture2D.MipLevels = 1;
      backbufferTypedDesc.Texture2D.MostDetailedMip = 0;
      backbufferTypedDesc.Texture2D.PlaneSlice = 0;
      pSRVDesc = &backbufferTypedDesc;
    }

    // if we have a descriptor but it specifies DXGI_FORMAT_UNKNOWN format, that means use
    // the texture's format. But as above, we fudge around the typeless backbuffer so we
    // have to set the correct typed format
    //
    // This behaviour is documented only for render targets, but seems to be used & work for
    // SRVs, so apply it here too.
    if(pSRVDesc && pSRVDesc->Format == DXGI_FORMAT_UNKNOWN &&
       WrappedID3D11Texture2D1::IsAlloc(live) && tex2d->m_RealDescriptor)
    {
      pSRVDesc->Format = tex2d->m_RealDescriptor->Format;
    }

    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
    {
      hr = m_pDevice3->CreateShaderResourceView1(GetResourceManager()->UnwrapResource(live),
                                                 pSRVDesc, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11ShaderResourceView1(ret, live, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateShaderResourceView1(ID3D11Resource *pResource,
                                                       const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                                                       ID3D11ShaderResourceView1 **ppSRView)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppSRView == NULL)
    return m_pDevice3->CreateShaderResourceView1(GetResourceManager()->UnwrapResource(pResource),
                                                 pDesc, NULL);

  ID3D11ShaderResourceView1 *real = NULL;
  ID3D11ShaderResourceView1 *wrapped = NULL;
  HRESULT ret = m_pDevice3->CreateShaderResourceView1(
      GetResourceManager()->UnwrapResource(pResource), pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ShaderResourceView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_SRV1);
      Serialise_CreateShaderResourceView1(pResource, pDesc, &wrapped);

      chunk = scope.Get();

      if(WrappedID3D11Texture1D::IsAlloc(pResource) || WrappedID3D11Texture2D1::IsAlloc(pResource) ||
         WrappedID3D11Texture3D1::IsAlloc(pResource) || WrappedID3D11Buffer::IsAlloc(pResource))
      {
        D3D11ResourceRecord *parent =
            GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));

        RDCASSERT(parent);

        WrappedID3D11ShaderResourceView1 *view = (WrappedID3D11ShaderResourceView1 *)wrapped;
        ResourceId id = view->GetResourceID();

        RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

        D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
        record->Length = 0;

        record->AddParent(parent);

        record->AddChunk(chunk);
      }
      else
      {
        RDCERR("Unexpected resource type in SRV creation");

        m_DeviceRecord->AddChunk(chunk);
      }
    }

    *ppSRView = wrapped;
  }

  return ret;
}
bool WrappedID3D11Device::Serialise_CreateRenderTargetView1(ID3D11Resource *pResource,
                                                            const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                                                            ID3D11RenderTargetView1 **ppRTView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_RENDER_TARGET_VIEW_DESC1, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppRTView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11RenderTargetView1 *ret = NULL;

    D3D11_RENDER_TARGET_VIEW_DESC1 *pRTVDesc = NULL;
    if(HasDesc)
      pRTVDesc = &Descriptor;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)live;

    D3D11_RENDER_TARGET_VIEW_DESC1 backbufferTypedDesc = {};

    // need to fixup typeless backbuffer fudging, if a descriptor isn't specified then
    // we need to make one to give the correct type
    if(!HasDesc && WrappedID3D11Texture2D1::IsAlloc(live) && tex2d->m_RealDescriptor)
    {
      backbufferTypedDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

      if(tex2d->m_RealDescriptor->SampleDesc.Quality > 0 ||
         tex2d->m_RealDescriptor->SampleDesc.Count > 1)
        backbufferTypedDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

      backbufferTypedDesc.Format = tex2d->m_RealDescriptor->Format;
      backbufferTypedDesc.Texture2D.MipSlice = 0;
      backbufferTypedDesc.Texture2D.PlaneSlice = 0;
      pRTVDesc = &backbufferTypedDesc;
    }

    // if we have a descriptor but it specifies DXGI_FORMAT_UNKNOWN format, that means use
    // the texture's format. But as above, we fudge around the typeless backbuffer so we
    // have to set the correct typed format
    if(pRTVDesc && pRTVDesc->Format == DXGI_FORMAT_UNKNOWN &&
       WrappedID3D11Texture2D1::IsAlloc(live) && tex2d->m_RealDescriptor)
    {
      pRTVDesc->Format = tex2d->m_RealDescriptor->Format;
    }

    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
    {
      hr = m_pDevice3->CreateRenderTargetView1(GetResourceManager()->UnwrapResource(live), pRTVDesc,
                                               &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11RenderTargetView1(ret, live, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateRenderTargetView1(ID3D11Resource *pResource,
                                                     const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                                                     ID3D11RenderTargetView1 **ppRTView)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppRTView == NULL)
    return m_pDevice3->CreateRenderTargetView1(GetResourceManager()->UnwrapResource(pResource),
                                               pDesc, NULL);

  ID3D11RenderTargetView1 *real = NULL;
  ID3D11RenderTargetView1 *wrapped = NULL;
  HRESULT ret = m_pDevice3->CreateRenderTargetView1(GetResourceManager()->UnwrapResource(pResource),
                                                    pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11RenderTargetView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_RTV1);
      Serialise_CreateRenderTargetView1(pResource, pDesc, &wrapped);

      chunk = scope.Get();

      if(WrappedID3D11Texture1D::IsAlloc(pResource) || WrappedID3D11Texture2D1::IsAlloc(pResource) ||
         WrappedID3D11Texture3D1::IsAlloc(pResource) || WrappedID3D11Buffer::IsAlloc(pResource))
      {
        D3D11ResourceRecord *parent =
            GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));

        RDCASSERT(parent);

        WrappedID3D11RenderTargetView1 *view = (WrappedID3D11RenderTargetView1 *)wrapped;
        ResourceId id = view->GetResourceID();

        RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

        D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
        record->Length = 0;

        record->AddParent(parent);

        record->AddChunk(chunk);
      }
      else
      {
        RDCERR("Unexpected resource type in RTV creation");

        m_DeviceRecord->AddChunk(chunk);
      }
    }

    *ppRTView = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateUnorderedAccessView1(
    ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
    ID3D11UnorderedAccessView1 **ppUAView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_UNORDERED_ACCESS_VIEW_DESC1, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppUAView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11UnorderedAccessView1 *ret = NULL;

    D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pUAVDesc = NULL;
    if(HasDesc)
      pUAVDesc = &Descriptor;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
    {
      hr = m_pDevice3->CreateUnorderedAccessView1(GetResourceManager()->UnwrapResource(live),
                                                  pUAVDesc, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11UnorderedAccessView1(ret, live, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateUnorderedAccessView1(ID3D11Resource *pResource,
                                                        const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
                                                        ID3D11UnorderedAccessView1 **ppUAView)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppUAView == NULL)
    return m_pDevice3->CreateUnorderedAccessView1(GetResourceManager()->UnwrapResource(pResource),
                                                  pDesc, NULL);

  ID3D11UnorderedAccessView1 *real = NULL;
  ID3D11UnorderedAccessView1 *wrapped = NULL;
  HRESULT ret = m_pDevice3->CreateUnorderedAccessView1(
      GetResourceManager()->UnwrapResource(pResource), pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11UnorderedAccessView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_UAV1);
      Serialise_CreateUnorderedAccessView1(pResource, pDesc, &wrapped);

      chunk = scope.Get();

      if(WrappedID3D11Texture1D::IsAlloc(pResource) || WrappedID3D11Texture2D1::IsAlloc(pResource) ||
         WrappedID3D11Texture3D1::IsAlloc(pResource) || WrappedID3D11Buffer::IsAlloc(pResource))
      {
        D3D11ResourceRecord *parent =
            GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));

        RDCASSERT(parent);

        WrappedID3D11UnorderedAccessView1 *view = (WrappedID3D11UnorderedAccessView1 *)wrapped;
        ResourceId id = view->GetResourceID();

        RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

        D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
        record->Length = 0;

        record->AddParent(parent);

        record->AddChunk(chunk);
      }
      else
      {
        RDCERR("Unexpected resource type in UAV creation");

        m_DeviceRecord->AddChunk(chunk);
      }
    }

    *ppUAView = wrapped;
  }
  return ret;
}

bool WrappedID3D11Device::Serialise_CreateRasterizerState2(
    const D3D11_RASTERIZER_DESC2 *pRasterizerDesc, ID3D11RasterizerState2 **ppRasterizerState)
{
  SERIALISE_ELEMENT_PTR(D3D11_RASTERIZER_DESC2, Descriptor, pRasterizerDesc);
  SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppRasterizerState));

  if(m_State == READING)
  {
    ID3D11RasterizerState2 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
      hr = m_pDevice3->CreateRasterizerState2(&Descriptor, &ret);
    else
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11RasterizerState2 *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(State, ret);
      }
      else
      {
        ret = new WrappedID3D11RasterizerState2(ret, this);

        GetResourceManager()->AddLiveResource(State, ret);
      }
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,
                                                    ID3D11RasterizerState2 **ppRasterizerState)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  if(ppRasterizerState == NULL)
    return m_pDevice3->CreateRasterizerState2(pRasterizerDesc, NULL);

  ID3D11RasterizerState2 *real = NULL;
  HRESULT ret = m_pDevice3->CreateRasterizerState2(pRasterizerDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    // duplicate states can be returned, if Create is called with a previous descriptor
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      *ppRasterizerState = (ID3D11RasterizerState2 *)GetResourceManager()->GetWrapper(real);
      (*ppRasterizerState)->AddRef();
      return ret;
    }

    ID3D11RasterizerState2 *wrapped = new WrappedID3D11RasterizerState2(real, this);

    CachedObjectsGarbageCollect();

    {
      RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
      wrapped->AddRef();
      InternalRef();
      m_CachedStateObjects.insert(wrapped);
    }

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE2);
      Serialise_CreateRasterizerState2(pRasterizerDesc, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppRasterizerState = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc,
                                                 ID3D11Query1 **ppQuery)
{
  SERIALISE_ELEMENT_PTR(D3D11_QUERY_DESC1, Descriptor, pQueryDesc);
  SERIALISE_ELEMENT(ResourceId, Query, GetIDForResource(*ppQuery));

  if(m_State == READING)
  {
    ID3D11Query1 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
      hr = m_pDevice3->CreateQuery1(&Descriptor, &ret);
    else
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Query1(ret, this);

      GetResourceManager()->AddLiveResource(Query, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc, ID3D11Query1 **ppQuery)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppQuery == NULL)
    return m_pDevice3->CreateQuery1(pQueryDesc, NULL);

  ID3D11Query1 *real = NULL;
  HRESULT ret = m_pDevice3->CreateQuery1(pQueryDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    *ppQuery = new WrappedID3D11Query1(real, this);
  }

  return ret;
}

void WrappedID3D11Device::GetImmediateContext3(ID3D11DeviceContext3 **ppImmediateContext)
{
  if(m_pDevice3 == NULL)
    return;

  if(ppImmediateContext)
  {
    m_pImmediateContext->AddRef();
    *ppImmediateContext = (ID3D11DeviceContext3 *)m_pImmediateContext;
  }
}

HRESULT WrappedID3D11Device::CreateDeferredContext3(UINT ContextFlags,
                                                    ID3D11DeviceContext3 **ppDeferredContext)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  if(ppDeferredContext == NULL)
    return m_pDevice3->CreateDeferredContext3(ContextFlags, NULL);

  ID3D11DeviceContext *defCtx = NULL;
  HRESULT ret = CreateDeferredContext(ContextFlags, &defCtx);

  if(SUCCEEDED(ret))
  {
    WrappedID3D11DeviceContext *wrapped = (WrappedID3D11DeviceContext *)defCtx;
    *ppDeferredContext = (ID3D11DeviceContext3 *)wrapped;
  }
  else
  {
    SAFE_RELEASE(defCtx);
  }

  return ret;
}

void WrappedID3D11Device::WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                                             const D3D11_BOX *pDstBox, const void *pSrcData,
                                             UINT SrcRowPitch, UINT SrcDepthPitch)
{
  if(m_pDevice3 == NULL)
    return;

  RDCUNIMPLEMENTED(
      "WriteToSubresource is not supported. Please contact me if you have a working example! "
      "https://github.com/baldurk/renderdoc/issues");

  m_pDevice3->WriteToSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
                                 SrcDepthPitch);

  return;
}

void WrappedID3D11Device::ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                                              ID3D11Resource *pSrcResource, UINT SrcSubresource,
                                              const D3D11_BOX *pSrcBox)
{
  if(m_pDevice3 == NULL)
    return;

  RDCUNIMPLEMENTED(
      "ReadFromSubresource is not supported. Please contact me if you have a working example! "
      "https://github.com/baldurk/renderdoc/issues");

  m_pDevice3->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, pSrcResource,
                                  SrcSubresource, pSrcBox);

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device4 interface

HRESULT WrappedID3D11Device::RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie)
{
  if(m_pDevice4 == NULL)
    return E_NOINTERFACE;

  return m_pDevice4->RegisterDeviceRemovedEvent(hEvent, pdwCookie);
}

void WrappedID3D11Device::UnregisterDeviceRemoved(DWORD dwCookie)
{
  if(m_pDevice4 == NULL)
    return;

  return m_pDevice4->UnregisterDeviceRemoved(dwCookie);
}