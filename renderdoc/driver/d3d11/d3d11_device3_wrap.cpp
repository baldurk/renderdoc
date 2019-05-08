/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateTexture2D1(SerialiserType &ser,
                                                     const D3D11_TEXTURE2D_DESC1 *pDesc,
                                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                     ID3D11Texture2D1 **ppTexture2D)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc);

  // unused, just for the sake of the user
  {
    UINT numSubresources = Descriptor.MipLevels
                               ? Descriptor.MipLevels
                               : CalcNumMips(Descriptor.Width, Descriptor.Height, 1);
    numSubresources *= Descriptor.ArraySize;

    SERIALISE_ELEMENT_ARRAY(pInitialData, pInitialData ? numSubresources : 0);
  }

  SERIALISE_ELEMENT_LOCAL(pTexture, GetIDForResource(*ppTexture2D))
      .TypedAs("ID3D11Texture2D *"_lit);

  std::vector<D3D11_SUBRESOURCE_DATA> descs =
      Serialise_CreateTextureData(ser, ppTexture2D ? *ppTexture2D : NULL, pTexture, pInitialData,
                                  Descriptor.Width, Descriptor.Height, 1, Descriptor.Format,
                                  Descriptor.MipLevels, Descriptor.ArraySize, pInitialData != NULL);

  if(IsReplayingAndReading() && ser.IsErrored())
  {
    // need to manually free the serialised buffers we stole in Serialise_CreateTextureData here,
    // before the SERIALISE_CHECK_READ_ERRORS macro below returns out of the function
    for(size_t i = 0; i < descs.size(); i++)
      FreeAlignedBuffer((byte *)descs[i].pSysMem);
  }

  if(IsReplayingAndReading())
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
      if(pInitialData != NULL)
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Texture2D1((ID3D11Texture2D1 *)ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }

    const char *prefix = Descriptor.ArraySize > 1 ? "2D TextureArray" : "2D Texture";

    if(Descriptor.BindFlags & D3D11_BIND_RENDER_TARGET)
      prefix = "2D Render Target";
    else if(Descriptor.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      prefix = "2D Depth Target";

    AddResource(pTexture, ResourceType::Texture, prefix);

    // free the serialised buffers we stole in Serialise_CreateTextureData
    for(size_t i = 0; i < descs.size(); i++)
      FreeAlignedBuffer((byte *)descs[i].pSysMem);
  }

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateTexture2D1(pDesc1, pInitialData, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture2D1((ID3D11Texture2D *)real, this);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateTexture2D1);
        Serialise_CreateTexture2D1(GET_SERIALISER, pDesc1, pInitialData, &wrapped);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateTexture3D1(SerialiserType &ser,
                                                     const D3D11_TEXTURE3D_DESC1 *pDesc,
                                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                     ID3D11Texture3D1 **ppTexture3D)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc);

  // unused, just for the sake of the user
  {
    UINT numSubresources = Descriptor.MipLevels
                               ? Descriptor.MipLevels
                               : CalcNumMips(Descriptor.Width, Descriptor.Height, Descriptor.Depth);

    SERIALISE_ELEMENT_ARRAY(pInitialData, pInitialData ? numSubresources : 0);
  }

  SERIALISE_ELEMENT_LOCAL(pTexture, GetIDForResource(*ppTexture3D))
      .TypedAs("ID3D11Texture3D *"_lit);

  std::vector<D3D11_SUBRESOURCE_DATA> descs =
      Serialise_CreateTextureData(ser, ppTexture3D ? *ppTexture3D : NULL, pTexture, pInitialData,
                                  Descriptor.Width, Descriptor.Height, Descriptor.Depth,
                                  Descriptor.Format, Descriptor.MipLevels, 1, pInitialData != NULL);

  if(IsReplayingAndReading() && ser.IsErrored())
  {
    // need to manually free the serialised buffers we stole in Serialise_CreateTextureData here,
    // before the SERIALISE_CHECK_READ_ERRORS macro below returns out of the function
    for(size_t i = 0; i < descs.size(); i++)
      FreeAlignedBuffer((byte *)descs[i].pSysMem);
  }

  if(IsReplayingAndReading())
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
      if(pInitialData != NULL)
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Texture3D1((ID3D11Texture3D1 *)ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }

    const char *prefix = "3D Texture";

    if(Descriptor.BindFlags & D3D11_BIND_RENDER_TARGET)
      prefix = "3D Render Target";
    else if(Descriptor.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      prefix = "3D Depth Target";

    AddResource(pTexture, ResourceType::Texture, prefix);

    // free the serialised buffers we stole in Serialise_CreateTextureData
    for(size_t i = 0; i < descs.size(); i++)
      FreeAlignedBuffer((byte *)descs[i].pSysMem);
  }

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateTexture3D1(pDesc1, pInitialData, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture3D1((ID3D11Texture3D *)real, this);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateTexture3D1);
        Serialise_CreateTexture3D1(GET_SERIALISER, pDesc1, pInitialData, &wrapped);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateShaderResourceView1(
    SerialiserType &ser, ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
    ID3D11ShaderResourceView1 **ppSRView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppSRView))
      .TypedAs("ID3D11ShaderResourceView1 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11ShaderResourceView1 *ret = NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC1 *pSRVDesc = (D3D11_SHADER_RESOURCE_VIEW_DESC1 *)pDesc;

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)pResource;

    D3D11_SHADER_RESOURCE_VIEW_DESC1 backbufferTypedDesc = {};

    // need to fixup typeless backbuffer fudging, if a descriptor isn't specified then
    // we need to make one to give the correct type
    if(!pDesc && WrappedID3D11Texture2D1::IsAlloc(pResource) && tex2d->m_RealDescriptor)
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
       WrappedID3D11Texture2D1::IsAlloc(pResource) && tex2d->m_RealDescriptor)
    {
      pSRVDesc->Format = tex2d->m_RealDescriptor->Format;
    }

    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
    {
      hr = m_pDevice3->CreateShaderResourceView1(GetResourceManager()->UnwrapResource(pResource),
                                                 pSRVDesc, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11ShaderResourceView1(ret, pResource, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }

    AddResource(pView, ResourceType::View, "Shader Resource View");
    DerivedResource(pResource, pView);
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateShaderResourceView1(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ShaderResourceView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateShaderResourceView1);
      Serialise_CreateShaderResourceView1(GET_SERIALISER, pResource, pDesc, &wrapped);

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
        record->ResType = IdentifyTypeByPtr(wrapped);
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
template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateRenderTargetView1(SerialiserType &ser,
                                                            ID3D11Resource *pResource,
                                                            const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                                                            ID3D11RenderTargetView1 **ppRTView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppRTView))
      .TypedAs("ID3D11RenderTargetView1 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11RenderTargetView1 *ret = NULL;

    D3D11_RENDER_TARGET_VIEW_DESC1 *pRTVDesc = (D3D11_RENDER_TARGET_VIEW_DESC1 *)pDesc;

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)pResource;

    D3D11_RENDER_TARGET_VIEW_DESC1 backbufferTypedDesc = {};

    // need to fixup typeless backbuffer fudging, if a descriptor isn't specified then
    // we need to make one to give the correct type
    if(!pDesc && WrappedID3D11Texture2D1::IsAlloc(pResource) && tex2d->m_RealDescriptor)
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
       WrappedID3D11Texture2D1::IsAlloc(pResource) && tex2d->m_RealDescriptor)
    {
      pRTVDesc->Format = tex2d->m_RealDescriptor->Format;
    }

    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
    {
      hr = m_pDevice3->CreateRenderTargetView1(GetResourceManager()->UnwrapResource(pResource),
                                               pRTVDesc, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11RenderTargetView1(ret, pResource, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }

    AddResource(pView, ResourceType::View, "Render Target View");
    DerivedResource(pResource, pView);
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateRenderTargetView1(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11RenderTargetView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateRenderTargetView1);
      Serialise_CreateRenderTargetView1(GET_SERIALISER, pResource, pDesc, &wrapped);

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
        record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateUnorderedAccessView1(
    SerialiserType &ser, ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
    ID3D11UnorderedAccessView1 **ppUAView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppUAView))
      .TypedAs("ID3D11UnorderedAccessView1 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11UnorderedAccessView1 *ret = NULL;

    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
    {
      hr = m_pDevice3->CreateUnorderedAccessView1(GetResourceManager()->UnwrapResource(pResource),
                                                  pDesc, &ret);
    }
    else
    {
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11UnorderedAccessView1(ret, pResource, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }

    AddResource(pView, ResourceType::View, "Unordered Access View");
    DerivedResource(pResource, pView);
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateUnorderedAccessView1(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11UnorderedAccessView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateUnorderedAccessView1);
      Serialise_CreateUnorderedAccessView1(GET_SERIALISER, pResource, pDesc, &wrapped);

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
        record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateRasterizerState2(
    SerialiserType &ser, const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,
    ID3D11RasterizerState2 **ppRasterizerState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pRasterizerDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppRasterizerState))
      .TypedAs("ID3D11RasterizerState2 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11RasterizerState2 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
      hr = m_pDevice3->CreateRasterizerState2(&Descriptor, &ret);
    else
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");

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
        ret = (ID3D11RasterizerState2 *)GetResourceManager()->GetWrapper(ret);
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

HRESULT WrappedID3D11Device::CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,
                                                    ID3D11RasterizerState2 **ppRasterizerState)
{
  if(m_pDevice3 == NULL)
    return E_NOINTERFACE;

  if(ppRasterizerState == NULL)
    return m_pDevice3->CreateRasterizerState2(pRasterizerDesc, NULL);

  ID3D11RasterizerState2 *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateRasterizerState2(pRasterizerDesc, &real));

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

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateRasterizerState2);
      Serialise_CreateRasterizerState2(GET_SERIALISER, pRasterizerDesc, &wrapped);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateQuery1(SerialiserType &ser,
                                                 const D3D11_QUERY_DESC1 *pQueryDesc,
                                                 ID3D11Query1 **ppQuery)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pQueryDesc);
  SERIALISE_ELEMENT_LOCAL(pQuery, GetIDForResource(*ppQuery)).TypedAs("ID3D11Query1 *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11Query1 *ret = NULL;
    HRESULT hr = E_NOINTERFACE;

    if(m_pDevice3)
      hr = m_pDevice3->CreateQuery1(&Descriptor, &ret);
    else
      RDCERR("Replaying a D3D11.3 device without D3D11.3 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Query1(ret, this);

      GetResourceManager()->AddLiveResource(pQuery, ret);
    }

    AddResource(pQuery, ResourceType::Query, "Query");
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
  ID3D11Query1 *wrapped = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->CreateQuery1(pQueryDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Query1(real, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateQuery1);
      Serialise_CreateQuery1(GET_SERIALISER, pQueryDesc, &wrapped);

      WrappedID3D11Query1 *q = (WrappedID3D11Query1 *)wrapped;
      ResourceId id = q->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppQuery = wrapped;
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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device5 interface

HRESULT WrappedID3D11Device::CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID riid,
                                         void **ppFence)
{
  if(m_pDevice5 == NULL)
    return E_NOINTERFACE;

  if(ppFence == NULL)
    return E_INVALIDARG;

  if(riid != __uuidof(ID3D11Fence))
  {
    RDCERR("Unsupported UUID '%s' in WrappedID3D11Device::CreateFence", ToStr(riid).c_str());
    return E_NOINTERFACE;
  }

  ID3D11Fence *ret = NULL;
  HRESULT hr = m_pDevice5->CreateFence(InitialValue, Flags, riid, (void **)&ret);

  if(FAILED(hr) || ret == NULL)
    return hr;

  WrappedID3D11Fence *wrapped = new WrappedID3D11Fence(ret, this);

  *ppFence = (ID3D11Fence *)wrapped;

  return S_OK;
}

HRESULT WrappedID3D11Device::OpenSharedFence(HANDLE hFence, REFIID riid, void **ppFence)
{
  if(m_pDevice5 == NULL)
    return E_NOINTERFACE;

  if(ppFence == NULL)
    return E_INVALIDARG;

  if(riid != __uuidof(ID3D11Fence))
  {
    RDCERR("Unsupported UUID '%s' in WrappedID3D11Device::OpenSharedFence", ToStr(riid).c_str());
    return E_NOINTERFACE;
  }

  ID3D11Fence *ret = NULL;
  HRESULT hr = m_pDevice5->OpenSharedFence(hFence, riid, (void **)&ret);

  if(FAILED(hr) || ret == NULL)
    return hr;

  WrappedID3D11Fence *wrapped = new WrappedID3D11Fence(ret, this);

  *ppFence = (ID3D11Fence *)wrapped;

  return S_OK;
}

#undef IMPLEMENT_FUNCTION_SERIALISED
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...)                                            \
  template bool WrappedID3D11Device::CONCAT(Serialise_, func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool WrappedID3D11Device::CONCAT(Serialise_, func(WriteSerialiser &ser, __VA_ARGS__));

SERIALISED_ID3D11DEVICE3_FUNCTIONS();