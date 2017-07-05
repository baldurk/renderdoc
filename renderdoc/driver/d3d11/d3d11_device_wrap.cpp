/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_resources.h"

bool WrappedID3D11Device::Serialise_CreateBuffer(const D3D11_BUFFER_DESC *pDesc,
                                                 const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                 ID3D11Buffer **ppBuffer)
{
  D3D11_SUBRESOURCE_DATA fakeData;
  RDCEraseEl(fakeData);

  SERIALISE_ELEMENT_PTR(D3D11_BUFFER_DESC, Descriptor, pDesc);
  if(pInitialData == NULL && m_State >= WRITING)
  {
    fakeData.pSysMem = new char[Descriptor.ByteWidth];
    fakeData.SysMemPitch = fakeData.SysMemSlicePitch = Descriptor.ByteWidth;
    memset((void *)fakeData.pSysMem, 0xfe, Descriptor.ByteWidth);
    pInitialData = &fakeData;
  }

  // this is a bit of a hack, but to maintain backwards compatibility we have a
  // separate function here that aligns the next serialised buffer to a 32-byte
  // boundary in memory while writing (just skips the padding on read).
  if(m_State >= WRITING || GetLogVersion() >= 0x000007)
    m_pSerialiser->AlignNextBuffer(32);

  // work around an nvidia driver bug, if a buffer is created as IMMUTABLE then it
  // can't be CopySubresourceRegion'd with a box offset, the data that's read is
  // wrong.
  if(m_State < WRITING && Descriptor.Usage == D3D11_USAGE_IMMUTABLE)
  {
    Descriptor.Usage = D3D11_USAGE_DEFAULT;
    // paranoid - I don't know what requirements might change, so set some sane default
    if(Descriptor.BindFlags == 0)
      Descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  }

  SERIALISE_ELEMENT_BUF(byte *, InitialData, pInitialData->pSysMem, Descriptor.ByteWidth);

  uint64_t offs = m_pSerialiser->GetOffset() - Descriptor.ByteWidth;

  RDCASSERT((offs % 16) == 0);

  SERIALISE_ELEMENT(uint32_t, MemPitch, pInitialData->SysMemPitch);
  SERIALISE_ELEMENT(uint32_t, MemSlicePitch, pInitialData->SysMemSlicePitch);
  SERIALISE_ELEMENT(ResourceId, pBuffer, GetIDForResource(*ppBuffer));

  if(m_State >= WRITING)
  {
    RDCASSERT(GetResourceManager()->GetResourceRecord(pBuffer) == NULL);

    D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(pBuffer);
    record->SetDataOffset(offs);
    record->DataInSerialiser = true;
    record->Length = Descriptor.ByteWidth;
  }

  if(m_State == READING)
  {
    ID3D11Buffer *ret;

    HRESULT hr = S_OK;

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = InitialData;
    data.SysMemPitch = MemPitch;
    data.SysMemSlicePitch = MemSlicePitch;
    hr = m_pDevice->CreateBuffer(&Descriptor, &data, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Buffer(ret, Descriptor.ByteWidth, this);

      GetResourceManager()->AddLiveResource(pBuffer, ret);
    }

    if(Descriptor.Usage != D3D11_USAGE_IMMUTABLE)
    {
      ID3D11Buffer *stage = NULL;

      D3D11_BUFFER_DESC desc;
      desc.ByteWidth = Descriptor.ByteWidth;
      desc.MiscFlags = 0;
      desc.StructureByteStride = 0;
      // We don't need to bind this, but IMMUTABLE requires at least one
      // BindFlags.
      desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_IMMUTABLE;

      data.SysMemPitch = Descriptor.ByteWidth;
      data.SysMemSlicePitch = Descriptor.ByteWidth;
      hr = m_pDevice->CreateBuffer(&desc, &data, &stage);

      if(FAILED(hr) || stage == NULL)
      {
        RDCERR("Failed to create staging buffer for buffer initial contents %08x", hr);
      }
      else
      {
        m_ResourceManager->SetInitialContents(
            pBuffer, D3D11ResourceManager::InitialContentData(stage, eInitialContents_Copy, NULL));
      }
    }

    SAFE_DELETE_ARRAY(InitialData);
  }

  char *arr = (char *)fakeData.pSysMem;
  SAFE_DELETE_ARRAY(arr);

  return true;
}

HRESULT WrappedID3D11Device::CreateBuffer(const D3D11_BUFFER_DESC *pDesc,
                                          const D3D11_SUBRESOURCE_DATA *pInitialData,
                                          ID3D11Buffer **ppBuffer)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppBuffer == NULL)
    return m_pDevice->CreateBuffer(pDesc, pInitialData, NULL);

  ID3D11Buffer *real = NULL;
  ID3D11Buffer *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateBuffer(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Buffer(real, pDesc ? pDesc->ByteWidth : 0, this);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER);
        Serialise_CreateBuffer(pDesc, pInitialData, &wrapped);

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
      WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppBuffer = wrapped;
  }

  return ret;
}

vector<D3D11_SUBRESOURCE_DATA> WrappedID3D11Device::Serialise_CreateTextureData(
    ID3D11Resource *tex, ResourceId id, const D3D11_SUBRESOURCE_DATA *data, UINT w, UINT h, UINT d,
    DXGI_FORMAT fmt, UINT mips, UINT arr, bool HasData)
{
  UINT numSubresources = mips;
  UINT numMips = mips;

  if(mips == 0)
    numSubresources = numMips = CalcNumMips(w, h, d);

  numSubresources *= arr;

  vector<D3D11_SUBRESOURCE_DATA> descs;
  if(m_State == READING && HasData)
  {
    descs.resize(numSubresources);
  }

  byte *scratch = NULL;

  for(UINT i = 0; i < numSubresources; i++)
  {
    int mip = i % numMips;

    UINT subresourceSize = GetByteSize(w, h, d, fmt, mip);

    RDCASSERT(subresourceSize > 0);

    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(id);

    if(m_State >= WRITING)
    {
      if(i == 0)
      {
        RDCASSERT(record == NULL);

        record = GetResourceManager()->AddResourceRecord(id);
        record->Length = 1;

        if(HasData)
          record->DataInSerialiser = true;

        record->NumSubResources = numSubresources;
        record->SubResources = new ResourceRecord *[record->NumSubResources];
        for(UINT s = 0; s < numSubresources; s++)
        {
          record->SubResources[s] = new D3D11ResourceRecord(ResourceId());
          record->SubResources[s]->DataInSerialiser = HasData;
        }
      }

      RDCASSERT(record != NULL);

      record->SubResources[i]->Length = subresourceSize;
    }

    if(!HasData)
      continue;

    if(scratch == NULL && m_State >= WRITING)
      scratch = new byte[subresourceSize];

    if(m_State >= WRITING)
    {
      MapIntercept intercept;
      intercept.SetD3D(data[i]);

      D3D11_RESOURCE_DIMENSION dim;
      tex->GetType(&dim);

      if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
        intercept.Init((ID3D11Texture1D *)tex, i, scratch);
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        intercept.Init((ID3D11Texture2D *)tex, i, scratch);
      else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
        intercept.Init((ID3D11Texture3D *)tex, i, scratch);
      else
        RDCERR("Unexpected resource type!");

      intercept.CopyFromD3D();
    }

    // this is a bit of a hack, but to maintain backwards compatibility we have a
    // separate function here that aligns the next serialised buffer to a 32-byte
    // boundary in memory while writing (just skips the padding on read).
    if(m_State >= WRITING || GetLogVersion() >= 0x000007)
      m_pSerialiser->AlignNextBuffer(32);

    SERIALISE_ELEMENT_BUF(byte *, buf, scratch, subresourceSize);

    if(m_State >= WRITING)
    {
      RDCASSERT(record);

      record->SubResources[i]->SetDataOffset(m_pSerialiser->GetOffset() - subresourceSize);
    }

    if(m_State == READING)
    {
      descs[i].pSysMem = buf;
      descs[i].SysMemPitch = GetByteSize(w, 1, 1, fmt, mip);
      descs[i].SysMemSlicePitch = GetByteSize(w, h, 1, fmt, mip);
    }
  }

  if(scratch)
    SAFE_DELETE_ARRAY(scratch);

  return descs;
}

bool WrappedID3D11Device::Serialise_CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
                                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                    ID3D11Texture1D **ppTexture1D)
{
  SERIALISE_ELEMENT_PTR(D3D11_TEXTURE1D_DESC, Descriptor, pDesc);
  SERIALISE_ELEMENT(ResourceId, pTexture, GetIDForResource(*ppTexture1D));

  SERIALISE_ELEMENT(bool, HasInitialData, pInitialData != NULL);

  vector<D3D11_SUBRESOURCE_DATA> descs = Serialise_CreateTextureData(
      ppTexture1D ? *ppTexture1D : NULL, pTexture, pInitialData, Descriptor.Width, 1, 1,
      Descriptor.Format, Descriptor.MipLevels, Descriptor.ArraySize, HasInitialData);

  if(m_State == READING)
  {
    ID3D11Texture1D *ret;
    HRESULT hr = S_OK;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(HasInitialData)
      hr = m_pDevice->CreateTexture1D(&Descriptor, &descs[0], &ret);
    else
      hr = m_pDevice->CreateTexture1D(&Descriptor, NULL, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Texture1D(ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }
  }

  for(size_t i = 0; i < descs.size(); i++)
    SAFE_DELETE_ARRAY(descs[i].pSysMem);

  return true;
}

HRESULT WrappedID3D11Device::CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture1D **ppTexture1D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture1D == NULL)
    return m_pDevice->CreateTexture1D(pDesc, pInitialData, NULL);

  ID3D11Texture1D *real = NULL;
  ID3D11Texture1D *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateTexture1D(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture1D(real, this);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_TEXTURE_1D);
        Serialise_CreateTexture1D(pDesc, pInitialData, &wrapped);

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
      WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppTexture1D = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                    ID3D11Texture2D **ppTexture2D)
{
  SERIALISE_ELEMENT_PTR(D3D11_TEXTURE2D_DESC, Descriptor, pDesc);
  SERIALISE_ELEMENT(ResourceId, pTexture, GetIDForResource(*ppTexture2D));

  SERIALISE_ELEMENT(bool, HasInitialData, pInitialData != NULL);

  vector<D3D11_SUBRESOURCE_DATA> descs = Serialise_CreateTextureData(
      ppTexture2D ? *ppTexture2D : NULL, pTexture, pInitialData, Descriptor.Width, Descriptor.Height,
      1, Descriptor.Format, Descriptor.MipLevels, Descriptor.ArraySize, HasInitialData);

  if(m_State == READING)
  {
    ID3D11Texture2D *ret;
    HRESULT hr = S_OK;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(HasInitialData)
      hr = m_pDevice->CreateTexture2D(&Descriptor, &descs[0], &ret);
    else
      hr = m_pDevice->CreateTexture2D(&Descriptor, NULL, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Texture2D1(ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }
  }

  for(size_t i = 0; i < descs.size(); i++)
    SAFE_DELETE_ARRAY(descs[i].pSysMem);

  return true;
}

HRESULT WrappedID3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture2D **ppTexture2D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture2D == NULL)
    return m_pDevice->CreateTexture2D(pDesc, pInitialData, NULL);

  ID3D11Texture2D *real = NULL;
  ID3D11Texture2D *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateTexture2D(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture2D1(real, this);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_TEXTURE_2D);
        Serialise_CreateTexture2D(pDesc, pInitialData, &wrapped);

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

bool WrappedID3D11Device::Serialise_CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                    ID3D11Texture3D **ppTexture3D)
{
  SERIALISE_ELEMENT_PTR(D3D11_TEXTURE3D_DESC, Descriptor, pDesc);
  SERIALISE_ELEMENT(ResourceId, pTexture, GetIDForResource(*ppTexture3D));

  SERIALISE_ELEMENT(bool, HasInitialData, pInitialData != NULL);

  vector<D3D11_SUBRESOURCE_DATA> descs = Serialise_CreateTextureData(
      ppTexture3D ? *ppTexture3D : NULL, pTexture, pInitialData, Descriptor.Width, Descriptor.Height,
      Descriptor.Depth, Descriptor.Format, Descriptor.MipLevels, 1, HasInitialData);

  if(m_State == READING)
  {
    ID3D11Texture3D *ret;
    HRESULT hr = S_OK;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(HasInitialData)
      hr = m_pDevice->CreateTexture3D(&Descriptor, &descs[0], &ret);
    else
      hr = m_pDevice->CreateTexture3D(&Descriptor, NULL, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Texture3D1(ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }
  }

  for(size_t i = 0; i < descs.size(); i++)
    SAFE_DELETE_ARRAY(descs[i].pSysMem);

  return true;
}

HRESULT WrappedID3D11Device::CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture3D **ppTexture3D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture3D == NULL)
    return m_pDevice->CreateTexture3D(pDesc, pInitialData, NULL);

  ID3D11Texture3D *real = NULL;
  ID3D11Texture3D *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateTexture3D(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture3D1(real, this);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_TEXTURE_3D);
        Serialise_CreateTexture3D(pDesc, pInitialData, &wrapped);

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

bool WrappedID3D11Device::Serialise_CreateShaderResourceView(ID3D11Resource *pResource,
                                                             const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                             ID3D11ShaderResourceView **ppSRView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_SHADER_RESOURCE_VIEW_DESC, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppSRView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11ShaderResourceView *ret;

    D3D11_SHADER_RESOURCE_VIEW_DESC *pSRVDesc = NULL;
    if(HasDesc)
      pSRVDesc = &Descriptor;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)live;

    D3D11_SHADER_RESOURCE_VIEW_DESC backbufferTypedDesc;

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

    HRESULT hr = m_pDevice->CreateShaderResourceView(GetResourceManager()->UnwrapResource(live),
                                                     pSRVDesc, &ret);

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

HRESULT WrappedID3D11Device::CreateShaderResourceView(ID3D11Resource *pResource,
                                                      const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                      ID3D11ShaderResourceView **ppSRView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppSRView == NULL)
    return m_pDevice->CreateShaderResourceView(GetResourceManager()->UnwrapResource(pResource),
                                               pDesc, NULL);

  ID3D11ShaderResourceView *real = NULL;
  ID3D11ShaderResourceView *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateShaderResourceView(GetResourceManager()->UnwrapResource(pResource),
                                                    pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ShaderResourceView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_SRV);
      Serialise_CreateShaderResourceView(pResource, pDesc, &wrapped);

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

bool WrappedID3D11Device::Serialise_CreateUnorderedAccessView(
    ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
    ID3D11UnorderedAccessView **ppUAView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_UNORDERED_ACCESS_VIEW_DESC, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppUAView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11UnorderedAccessView *ret;

    D3D11_UNORDERED_ACCESS_VIEW_DESC *pUAVDesc = NULL;
    if(HasDesc)
      pUAVDesc = &Descriptor;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    HRESULT hr = m_pDevice->CreateUnorderedAccessView(GetResourceManager()->UnwrapResource(live),
                                                      pUAVDesc, &ret);

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

HRESULT WrappedID3D11Device::CreateUnorderedAccessView(ID3D11Resource *pResource,
                                                       const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                                       ID3D11UnorderedAccessView **ppUAView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppUAView == NULL)
    return m_pDevice->CreateUnorderedAccessView(GetResourceManager()->UnwrapResource(pResource),
                                                pDesc, NULL);

  ID3D11UnorderedAccessView *real = NULL;
  ID3D11UnorderedAccessView *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateUnorderedAccessView(
      GetResourceManager()->UnwrapResource(pResource), pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11UnorderedAccessView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_UAV);
      Serialise_CreateUnorderedAccessView(pResource, pDesc, &wrapped);

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

bool WrappedID3D11Device::Serialise_CreateRenderTargetView(ID3D11Resource *pResource,
                                                           const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                                           ID3D11RenderTargetView **ppRTView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_RENDER_TARGET_VIEW_DESC, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppRTView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11RenderTargetView *ret;

    D3D11_RENDER_TARGET_VIEW_DESC *pRTVDesc = NULL;
    if(HasDesc)
      pRTVDesc = &Descriptor;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)live;

    D3D11_RENDER_TARGET_VIEW_DESC backbufferTypedDesc;

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

    HRESULT hr = m_pDevice->CreateRenderTargetView(GetResourceManager()->UnwrapResource(live),
                                                   pRTVDesc, &ret);

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

HRESULT WrappedID3D11Device::CreateRenderTargetView(ID3D11Resource *pResource,
                                                    const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                                    ID3D11RenderTargetView **ppRTView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppRTView == NULL)
    return m_pDevice->CreateRenderTargetView(GetResourceManager()->UnwrapResource(pResource), pDesc,
                                             NULL);

  ID3D11RenderTargetView *real = NULL;
  ID3D11RenderTargetView *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateRenderTargetView(GetResourceManager()->UnwrapResource(pResource),
                                                  pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11RenderTargetView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_RTV);
      Serialise_CreateRenderTargetView(pResource, pDesc, &wrapped);

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

bool WrappedID3D11Device::Serialise_CreateDepthStencilView(ID3D11Resource *pResource,
                                                           const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                           ID3D11DepthStencilView **ppDepthStencilView)
{
  SERIALISE_ELEMENT(ResourceId, Resource, GetIDForResource(pResource));
  SERIALISE_ELEMENT(bool, HasDesc, pDesc != NULL);
  SERIALISE_ELEMENT_PTR_OPT(D3D11_DEPTH_STENCIL_VIEW_DESC, Descriptor, pDesc, HasDesc);
  SERIALISE_ELEMENT(ResourceId, pView, GetIDForResource(*ppDepthStencilView));

  if(m_State == READING && GetResourceManager()->HasLiveResource(Resource))
  {
    ID3D11DepthStencilView *ret;

    ID3D11Resource *live = (ID3D11Resource *)GetResourceManager()->GetLiveResource(Resource);

    pDesc = NULL;
    if(HasDesc)
      pDesc = &Descriptor;

    HRESULT hr =
        m_pDevice->CreateDepthStencilView(GetResourceManager()->UnwrapResource(live), pDesc, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11DepthStencilView(ret, live, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateDepthStencilView(ID3D11Resource *pResource,
                                                    const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                    ID3D11DepthStencilView **ppDepthStencilView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDepthStencilView == NULL)
    return m_pDevice->CreateDepthStencilView(GetResourceManager()->UnwrapResource(pResource), pDesc,
                                             NULL);

  ID3D11DepthStencilView *real = NULL;
  ID3D11DepthStencilView *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateDepthStencilView(GetResourceManager()->UnwrapResource(pResource),
                                                  pDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11DepthStencilView(real, pResource, this);

    Chunk *chunk = NULL;

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_DSV);
      Serialise_CreateDepthStencilView(pResource, pDesc, &wrapped);

      chunk = scope.Get();

      if(WrappedID3D11Texture1D::IsAlloc(pResource) || WrappedID3D11Texture2D1::IsAlloc(pResource) ||
         WrappedID3D11Texture3D1::IsAlloc(pResource) || WrappedID3D11Buffer::IsAlloc(pResource))
      {
        D3D11ResourceRecord *parent =
            GetResourceManager()->GetResourceRecord(GetIDForResource(pResource));

        RDCASSERT(parent);

        WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)wrapped;
        ResourceId id = view->GetResourceID();

        RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

        D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
        record->Length = 0;

        record->AddParent(parent);

        record->AddChunk(chunk);
      }
      else
      {
        RDCERR("Unexpected resource type in DSV creation");

        m_DeviceRecord->AddChunk(chunk);
      }
    }

    *ppDepthStencilView = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
    const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
    ID3D11InputLayout **ppInputLayout)
{
  SERIALISE_ELEMENT(uint32_t, NumElems, NumElements);

  D3D11_INPUT_ELEMENT_DESC *layouts = new D3D11_INPUT_ELEMENT_DESC[NumElems];

  for(UINT i = 0; i < NumElems; i++)
  {
    SERIALISE_ELEMENT(D3D11_INPUT_ELEMENT_DESC, layout, pInputElementDescs[i]);

    layouts[i] = layout;
  }

  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecodeWithInputSignature, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLayout, GetIDForResource(*ppInputLayout));

  ID3D11InputLayout *ret = NULL;
  if(m_State >= WRITING)
  {
    ret = *ppInputLayout;
  }
  else if(m_State == READING)
  {
    HRESULT hr =
        m_pDevice->CreateInputLayout(layouts, NumElems, ShaderBytecode, (size_t)BytecodeLen, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11InputLayout(ret, this);

      GetResourceManager()->AddLiveResource(pLayout, ret);
    }

    vector<D3D11_INPUT_ELEMENT_DESC> descvec(layouts, layouts + NumElems);

    m_LayoutDescs[ret] = descvec;
    if(BytecodeLen > 0 && ShaderBytecode)
      m_LayoutShaders[ret] =
          new WrappedShader(this, pLayout, GetIDForResource(ret), ShaderBytecode, BytecodeLen);

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  SAFE_DELETE_ARRAY(layouts);

  return true;
}

HRESULT WrappedID3D11Device::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
                                               UINT NumElements,
                                               const void *pShaderBytecodeWithInputSignature,
                                               SIZE_T BytecodeLength,
                                               ID3D11InputLayout **ppInputLayout)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppInputLayout == NULL)
    return m_pDevice->CreateInputLayout(pInputElementDescs, NumElements,
                                        pShaderBytecodeWithInputSignature, BytecodeLength, NULL);

  ID3D11InputLayout *real = NULL;
  ID3D11InputLayout *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateInputLayout(
      pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11InputLayout(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_INPUT_LAYOUT);
      Serialise_CreateInputLayout(pInputElementDescs, NumElements,
                                  pShaderBytecodeWithInputSignature, BytecodeLength, &wrapped);

      WrappedID3D11InputLayout *lay = (WrappedID3D11InputLayout *)wrapped;
      ResourceId id = lay->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
  }

  *ppInputLayout = wrapped;

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateVertexShader(const void *pShaderBytecode,
                                                       SIZE_T BytecodeLength,
                                                       ID3D11ClassLinkage *pClassLinkage,
                                                       ID3D11VertexShader **ppVertexShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, (void *&)pShaderBytecode, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppVertexShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11VertexShader *ret;
    HRESULT hr = m_pDevice->CreateVertexShader(ShaderBytecode, (size_t)BytecodeLen, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11VertexShader>(ret, pShader, ShaderBytecode,
                                                        (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                ID3D11ClassLinkage *pClassLinkage,
                                                ID3D11VertexShader **ppVertexShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppVertexShader == NULL)
    return m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength,
                                         UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11VertexShader *real = NULL;
  ID3D11VertexShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateVertexShader(
      pShaderBytecode, BytecodeLength, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11VertexShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_VERTEX_SHADER);
      Serialise_CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11VertexShader> *sh =
          (WrappedID3D11Shader<ID3D11VertexShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11VertexShader> *w = (WrappedID3D11Shader<ID3D11VertexShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppVertexShader = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateGeometryShader(const void *pShaderBytecode,
                                                         SIZE_T BytecodeLength,
                                                         ID3D11ClassLinkage *pClassLinkage,
                                                         ID3D11GeometryShader **ppGeometryShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecode, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppGeometryShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11GeometryShader *ret;
    HRESULT hr = m_pDevice->CreateGeometryShader(ShaderBytecode, (size_t)BytecodeLen, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11GeometryShader>(ret, pShader, ShaderBytecode,
                                                          (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                  ID3D11ClassLinkage *pClassLinkage,
                                                  ID3D11GeometryShader **ppGeometryShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppGeometryShader == NULL)
    return m_pDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength,
                                           UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11GeometryShader *real = NULL;
  ID3D11GeometryShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateGeometryShader(
      pShaderBytecode, BytecodeLength, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11GeometryShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_GEOMETRY_SHADER);
      Serialise_CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11GeometryShader> *sh =
          (WrappedID3D11Shader<ID3D11GeometryShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11GeometryShader> *w =
          (WrappedID3D11Shader<ID3D11GeometryShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppGeometryShader = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateGeometryShaderWithStreamOutput(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides,
    UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage,
    ID3D11GeometryShader **ppGeometryShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecode, BytecodeLength);

  SERIALISE_ELEMENT(uint32_t, numEntries, NumEntries);
  SERIALISE_ELEMENT_ARR(D3D11_SO_DECLARATION_ENTRY, SODecl, pSODeclaration, numEntries);

  SERIALISE_ELEMENT(uint32_t, numStrides, NumStrides);
  SERIALISE_ELEMENT_ARR(uint32_t, BufStrides, pBufferStrides, numStrides);

  SERIALISE_ELEMENT(uint32_t, RastStream, RasterizedStream);

  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppGeometryShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11GeometryShader *ret;
    HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
        ShaderBytecode, (size_t)BytecodeLen, SODecl, numEntries,
        numStrides == 0 ? NULL : BufStrides, numStrides, RastStream, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11GeometryShader>(ret, pShader, ShaderBytecode,
                                                          (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  SAFE_DELETE_ARRAY(SODecl);
  SAFE_DELETE_ARRAY(BufStrides);

  return true;
}

HRESULT WrappedID3D11Device::CreateGeometryShaderWithStreamOutput(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides,
    UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage,
    ID3D11GeometryShader **ppGeometryShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppGeometryShader == NULL)
    return m_pDevice->CreateGeometryShaderWithStreamOutput(
        pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides,
        RasterizedStream, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11GeometryShader *real = NULL;
  ID3D11GeometryShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateGeometryShaderWithStreamOutput(
      pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides,
      RasterizedStream, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11GeometryShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_GEOMETRY_SHADER_WITH_SO);
      Serialise_CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
                                                     NumEntries, pBufferStrides, NumStrides,
                                                     RasterizedStream, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11GeometryShader> *sh =
          (WrappedID3D11Shader<ID3D11GeometryShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11GeometryShader> *w =
          (WrappedID3D11Shader<ID3D11GeometryShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppGeometryShader = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreatePixelShader(const void *pShaderBytecode,
                                                      SIZE_T BytecodeLength,
                                                      ID3D11ClassLinkage *pClassLinkage,
                                                      ID3D11PixelShader **ppPixelShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecode, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppPixelShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11PixelShader *ret;
    HRESULT hr = m_pDevice->CreatePixelShader(ShaderBytecode, (size_t)BytecodeLen, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11PixelShader>(ret, pShader, ShaderBytecode,
                                                       (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  return true;
}

HRESULT WrappedID3D11Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                               ID3D11ClassLinkage *pClassLinkage,
                                               ID3D11PixelShader **ppPixelShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppPixelShader == NULL)
    return m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength,
                                        UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11PixelShader *real = NULL;
  ID3D11PixelShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreatePixelShader(
      pShaderBytecode, BytecodeLength, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11PixelShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_PIXEL_SHADER);
      Serialise_CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11PixelShader> *sh = (WrappedID3D11Shader<ID3D11PixelShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11PixelShader> *w = (WrappedID3D11Shader<ID3D11PixelShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppPixelShader = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateHullShader(const void *pShaderBytecode,
                                                     SIZE_T BytecodeLength,
                                                     ID3D11ClassLinkage *pClassLinkage,
                                                     ID3D11HullShader **ppHullShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecode, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppHullShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11HullShader *ret;
    HRESULT hr = m_pDevice->CreateHullShader(ShaderBytecode, (size_t)BytecodeLen, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11HullShader>(ret, pShader, ShaderBytecode,
                                                      (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                              ID3D11ClassLinkage *pClassLinkage,
                                              ID3D11HullShader **ppHullShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppHullShader == NULL)
    return m_pDevice->CreateHullShader(pShaderBytecode, BytecodeLength,
                                       UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11HullShader *real = NULL;
  ID3D11HullShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateHullShader(
      pShaderBytecode, BytecodeLength, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11HullShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_HULL_SHADER);
      Serialise_CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11HullShader> *sh = (WrappedID3D11Shader<ID3D11HullShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11HullShader> *w = (WrappedID3D11Shader<ID3D11HullShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppHullShader = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateDomainShader(const void *pShaderBytecode,
                                                       SIZE_T BytecodeLength,
                                                       ID3D11ClassLinkage *pClassLinkage,
                                                       ID3D11DomainShader **ppDomainShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecode, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppDomainShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11DomainShader *ret;
    HRESULT hr = m_pDevice->CreateDomainShader(ShaderBytecode, (size_t)BytecodeLen, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11DomainShader>(ret, pShader, ShaderBytecode,
                                                        (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                ID3D11ClassLinkage *pClassLinkage,
                                                ID3D11DomainShader **ppDomainShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDomainShader == NULL)
    return m_pDevice->CreateDomainShader(pShaderBytecode, BytecodeLength,
                                         UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11DomainShader *real = NULL;
  ID3D11DomainShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateDomainShader(
      pShaderBytecode, BytecodeLength, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11DomainShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_DOMAIN_SHADER);
      Serialise_CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11DomainShader> *sh =
          (WrappedID3D11Shader<ID3D11DomainShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11DomainShader> *w = (WrappedID3D11Shader<ID3D11DomainShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppDomainShader = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateComputeShader(const void *pShaderBytecode,
                                                        SIZE_T BytecodeLength,
                                                        ID3D11ClassLinkage *pClassLinkage,
                                                        ID3D11ComputeShader **ppComputeShader)
{
  SERIALISE_ELEMENT(uint32_t, BytecodeLen, (uint32_t)BytecodeLength);
  SERIALISE_ELEMENT_BUF(byte *, ShaderBytecode, pShaderBytecode, BytecodeLength);
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(pClassLinkage));
  SERIALISE_ELEMENT(ResourceId, pShader, GetIDForResource(*ppComputeShader));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *linkage = NULL;
    if(GetResourceManager()->HasLiveResource(pLinkage))
      linkage = UNWRAP(WrappedID3D11ClassLinkage,
                       (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage));

    ID3D11ComputeShader *ret;
    HRESULT hr = m_pDevice->CreateComputeShader(ShaderBytecode, (size_t)BytecodeLen, linkage, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11ComputeShader>(ret, pShader, ShaderBytecode,
                                                         (size_t)BytecodeLen, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    SAFE_DELETE_ARRAY(ShaderBytecode);
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                 ID3D11ClassLinkage *pClassLinkage,
                                                 ID3D11ComputeShader **ppComputeShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppComputeShader == NULL)
    return m_pDevice->CreateComputeShader(pShaderBytecode, BytecodeLength,
                                          UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), NULL);

  ID3D11ComputeShader *real = NULL;
  ID3D11ComputeShader *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateComputeShader(
      pShaderBytecode, BytecodeLength, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11ComputeShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_COMPUTE_SHADER);
      Serialise_CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11ComputeShader> *sh =
          (WrappedID3D11Shader<ID3D11ComputeShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
    else
    {
      WrappedID3D11Shader<ID3D11ComputeShader> *w =
          (WrappedID3D11Shader<ID3D11ComputeShader> *)wrapped;

      GetResourceManager()->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppComputeShader = wrapped;
  }

  return ret;
}

// Class Linkage 'fake' interfaces
bool WrappedID3D11Device::Serialise_CreateClassInstance(
    LPCSTR pClassTypeName, UINT ConstantBufferOffset, UINT ConstantVectorOffset, UINT TextureOffset,
    UINT SamplerOffset, WrappedID3D11ClassLinkage *linkage, ID3D11ClassInstance *inst)
{
  string name = pClassTypeName ? pClassTypeName : "";
  m_pSerialiser->Serialise("name", name);

  SERIALISE_ELEMENT(UINT, cbOffset, ConstantBufferOffset);
  SERIALISE_ELEMENT(UINT, cvOffset, ConstantVectorOffset);
  SERIALISE_ELEMENT(UINT, texOffset, TextureOffset);
  SERIALISE_ELEMENT(UINT, sampOffset, SamplerOffset);
  SERIALISE_ELEMENT(ResourceId, pLinkage, linkage->GetResourceID());
  SERIALISE_ELEMENT(ResourceId, instance, GetIDForResource(inst));

  if(m_State == READING && GetResourceManager()->HasLiveResource(pLinkage))
  {
    ID3D11ClassLinkage *wrappedLink =
        (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage);
    ID3D11ClassLinkage *realLink = UNWRAP(WrappedID3D11ClassLinkage, wrappedLink);

    ID3D11ClassInstance *real = NULL;
    ID3D11ClassInstance *wrapped = NULL;
    HRESULT hr = realLink->CreateClassInstance(name.c_str(), cbOffset, cvOffset, texOffset,
                                               sampOffset, &real);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      wrapped = new WrappedID3D11ClassInstance(real, wrappedLink, this);

      GetResourceManager()->AddLiveResource(instance, wrapped);
    }
  }

  return true;
}

ID3D11ClassInstance *WrappedID3D11Device::CreateClassInstance(
    LPCSTR pClassTypeName, UINT ConstantBufferOffset, UINT ConstantVectorOffset, UINT TextureOffset,
    UINT SamplerOffset, WrappedID3D11ClassLinkage *linkage, ID3D11ClassInstance *inst)
{
  ID3D11ClassInstance *wrapped = NULL;

  if(m_State >= WRITING)
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ClassInstance(inst, linkage, this);

    {
      SCOPED_SERIALISE_CONTEXT(CREATE_CLASS_INSTANCE);
      Serialise_CreateClassInstance(pClassTypeName, ConstantBufferOffset, ConstantVectorOffset,
                                    TextureOffset, SamplerOffset, linkage, wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    return wrapped;
  }

  return inst;
}

bool WrappedID3D11Device::Serialise_GetClassInstance(LPCSTR pClassInstanceName, UINT InstanceIndex,
                                                     WrappedID3D11ClassLinkage *linkage,
                                                     ID3D11ClassInstance *inst)
{
  string name = pClassInstanceName ? pClassInstanceName : "";
  m_pSerialiser->Serialise("name", name);

  SERIALISE_ELEMENT(UINT, idx, InstanceIndex);
  SERIALISE_ELEMENT(ResourceId, pLinkage, linkage->GetResourceID());
  SERIALISE_ELEMENT(ResourceId, instance, GetIDForResource(inst));

  if(m_State == READING && GetResourceManager()->HasLiveResource(pLinkage))
  {
    ID3D11ClassLinkage *wrappedLink =
        (ID3D11ClassLinkage *)GetResourceManager()->GetLiveResource(pLinkage);
    ID3D11ClassLinkage *realLink = UNWRAP(WrappedID3D11ClassLinkage, wrappedLink);

    ID3D11ClassInstance *real = NULL;
    ID3D11ClassInstance *wrapped = NULL;
    HRESULT hr = realLink->GetClassInstance(name.c_str(), idx, &real);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      wrapped = new WrappedID3D11ClassInstance(real, wrappedLink, this);

      GetResourceManager()->AddLiveResource(instance, wrapped);
    }
  }

  return true;
}

ID3D11ClassInstance *WrappedID3D11Device::GetClassInstance(LPCSTR pClassInstanceName,
                                                           UINT InstanceIndex,
                                                           WrappedID3D11ClassLinkage *linkage,
                                                           ID3D11ClassInstance *inst)
{
  ID3D11ClassInstance *wrapped = NULL;

  if(m_State >= WRITING)
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ClassInstance(inst, linkage, this);

    {
      SCOPED_SERIALISE_CONTEXT(GET_CLASS_INSTANCE);
      Serialise_GetClassInstance(pClassInstanceName, InstanceIndex, linkage, wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    return wrapped;
  }

  return inst;
}

bool WrappedID3D11Device::Serialise_CreateClassLinkage(ID3D11ClassLinkage **ppLinkage)
{
  SERIALISE_ELEMENT(ResourceId, pLinkage, GetIDForResource(*ppLinkage));

  if(m_State == READING)
  {
    ID3D11ClassLinkage *ret;
    HRESULT hr = m_pDevice->CreateClassLinkage(&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11ClassLinkage(ret, this);

      GetResourceManager()->AddLiveResource(pLinkage, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateClassLinkage(ID3D11ClassLinkage **ppLinkage)
{
  // get 'real' return value for NULL parameter
  if(ppLinkage == NULL)
    return m_pDevice->CreateClassLinkage(NULL);

  ID3D11ClassLinkage *real = NULL;
  ID3D11ClassLinkage *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateClassLinkage(&real);

  if(SUCCEEDED(ret) && m_State >= WRITING)
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ClassLinkage(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_CLASS_LINKAGE);
      Serialise_CreateClassLinkage(&wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppLinkage = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                                                     ID3D11BlendState **ppBlendState)
{
  SERIALISE_ELEMENT_PTR(D3D11_BLEND_DESC, Descriptor, pBlendStateDesc);
  SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppBlendState));

  if(m_State == READING)
  {
    ID3D11BlendState *ret;
    HRESULT hr = m_pDevice->CreateBlendState(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11BlendState *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(State, ret);
      }
      else
      {
        ret = new WrappedID3D11BlendState1(ret, this);

        GetResourceManager()->AddLiveResource(State, ret);
      }
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                                              ID3D11BlendState **ppBlendState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppBlendState == NULL)
    return m_pDevice->CreateBlendState(pBlendStateDesc, NULL);

  ID3D11BlendState *real = NULL;
  HRESULT ret = m_pDevice->CreateBlendState(pBlendStateDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    // duplicate states can be returned, if Create is called with a previous descriptor
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      *ppBlendState = (ID3D11BlendState *)GetResourceManager()->GetWrapper(real);
      (*ppBlendState)->AddRef();
      return ret;
    }

    ID3D11BlendState *wrapped = new WrappedID3D11BlendState1(real, this);

    CachedObjectsGarbageCollect();

    {
      RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
      wrapped->AddRef();
      InternalRef();
      m_CachedStateObjects.insert(wrapped);
    }

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_BLEND_STATE);
      Serialise_CreateBlendState(pBlendStateDesc, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppBlendState = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState)
{
  SERIALISE_ELEMENT_PTR(D3D11_DEPTH_STENCIL_DESC, Descriptor, pDepthStencilDesc);
  SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppDepthStencilState));

  if(m_State == READING)
  {
    ID3D11DepthStencilState *ret;
    HRESULT hr = m_pDevice->CreateDepthStencilState(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11DepthStencilState *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(State, ret);
      }
      else
      {
        ret = new WrappedID3D11DepthStencilState(ret, this);

        GetResourceManager()->AddLiveResource(State, ret);
      }
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
                                                     ID3D11DepthStencilState **ppDepthStencilState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDepthStencilState == NULL)
    return m_pDevice->CreateDepthStencilState(pDepthStencilDesc, NULL);

  ID3D11DepthStencilState *real = NULL;
  HRESULT ret = m_pDevice->CreateDepthStencilState(pDepthStencilDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    // duplicate states can be returned, if Create is called with a previous descriptor
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      *ppDepthStencilState = (ID3D11DepthStencilState *)GetResourceManager()->GetWrapper(real);
      (*ppDepthStencilState)->AddRef();
      return ret;
    }

    ID3D11DepthStencilState *wrapped = new WrappedID3D11DepthStencilState(real, this);

    CachedObjectsGarbageCollect();

    {
      RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
      wrapped->AddRef();
      InternalRef();
      m_CachedStateObjects.insert(wrapped);
    }

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_DEPTHSTENCIL_STATE);
      Serialise_CreateDepthStencilState(pDepthStencilDesc, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppDepthStencilState = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                                                          ID3D11RasterizerState **ppRasterizerState)
{
  SERIALISE_ELEMENT_PTR(D3D11_RASTERIZER_DESC, Descriptor, pRasterizerDesc);
  SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppRasterizerState));

  if(m_State == READING)
  {
    ID3D11RasterizerState *ret;
    HRESULT hr = m_pDevice->CreateRasterizerState(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11RasterizerState *)GetResourceManager()->GetWrapper(ret);
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

HRESULT WrappedID3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                                                   ID3D11RasterizerState **ppRasterizerState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppRasterizerState == NULL)
    return m_pDevice->CreateRasterizerState(pRasterizerDesc, NULL);

  ID3D11RasterizerState *real = NULL;
  HRESULT ret = m_pDevice->CreateRasterizerState(pRasterizerDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    // duplicate states can be returned, if Create is called with a previous descriptor
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      *ppRasterizerState = (ID3D11RasterizerState *)GetResourceManager()->GetWrapper(real);
      (*ppRasterizerState)->AddRef();
      return ret;
    }

    ID3D11RasterizerState *wrapped = new WrappedID3D11RasterizerState2(real, this);

    CachedObjectsGarbageCollect();

    {
      RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
      wrapped->AddRef();
      InternalRef();
      m_CachedStateObjects.insert(wrapped);
    }

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE);
      Serialise_CreateRasterizerState(pRasterizerDesc, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppRasterizerState = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                                                       ID3D11SamplerState **ppSamplerState)
{
  SERIALISE_ELEMENT_PTR(D3D11_SAMPLER_DESC, Descriptor, pSamplerDesc);
  SERIALISE_ELEMENT(ResourceId, State, GetIDForResource(*ppSamplerState));

  if(m_State == READING)
  {
    ID3D11SamplerState *ret;
    HRESULT hr = m_pDevice->CreateSamplerState(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      if(GetResourceManager()->HasWrapper(ret))
      {
        ret->Release();
        ret = (ID3D11SamplerState *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(State, ret);
      }
      else
      {
        ret = new WrappedID3D11SamplerState(ret, this);

        GetResourceManager()->AddLiveResource(State, ret);
      }
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                                                ID3D11SamplerState **ppSamplerState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppSamplerState == NULL)
    return m_pDevice->CreateSamplerState(pSamplerDesc, NULL);

  ID3D11SamplerState *real = NULL;
  HRESULT ret = m_pDevice->CreateSamplerState(pSamplerDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    // duplicate states can be returned, if Create is called with a previous descriptor
    if(GetResourceManager()->HasWrapper(real))
    {
      real->Release();
      *ppSamplerState = (ID3D11SamplerState *)GetResourceManager()->GetWrapper(real);
      (*ppSamplerState)->AddRef();
      return ret;
    }

    ID3D11SamplerState *wrapped = new WrappedID3D11SamplerState(real, this);

    CachedObjectsGarbageCollect();

    {
      RDCASSERT(m_CachedStateObjects.find(wrapped) == m_CachedStateObjects.end());
      wrapped->AddRef();
      InternalRef();
      m_CachedStateObjects.insert(wrapped);
    }

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLER_STATE);
      Serialise_CreateSamplerState(pSamplerDesc, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppSamplerState = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateQuery(const D3D11_QUERY_DESC *pQueryDesc,
                                                ID3D11Query **ppQuery)
{
  SERIALISE_ELEMENT_PTR(D3D11_QUERY_DESC, Descriptor, pQueryDesc);
  SERIALISE_ELEMENT(ResourceId, Query, GetIDForResource(*ppQuery));

  if(m_State == READING)
  {
    ID3D11Query *ret;
    HRESULT hr = m_pDevice->CreateQuery(&Descriptor, &ret);

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

HRESULT WrappedID3D11Device::CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppQuery == NULL)
    return m_pDevice->CreateQuery(pQueryDesc, NULL);

  ID3D11Query *real = NULL;
  HRESULT ret = m_pDevice->CreateQuery(pQueryDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    *ppQuery = new WrappedID3D11Query1(real, this);
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
                                                    ID3D11Predicate **ppPredicate)
{
  SERIALISE_ELEMENT_PTR(D3D11_QUERY_DESC, Descriptor, pPredicateDesc);
  SERIALISE_ELEMENT(ResourceId, Predicate, GetIDForResource(*ppPredicate));

  if(m_State == READING)
  {
    ID3D11Predicate *ret;
    HRESULT hr = m_pDevice->CreatePredicate(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Predicate(ret, this);

      GetResourceManager()->AddLiveResource(Predicate, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
                                             ID3D11Predicate **ppPredicate)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppPredicate == NULL)
    return m_pDevice->CreatePredicate(pPredicateDesc, NULL);

  ID3D11Predicate *real = NULL;
  ID3D11Predicate *wrapped = NULL;
  HRESULT ret = m_pDevice->CreatePredicate(pPredicateDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Predicate(real, this);

    if(m_State >= WRITING)
    {
      SCOPED_SERIALISE_CONTEXT(CREATE_PREDICATE);
      Serialise_CreatePredicate(pPredicateDesc, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppPredicate = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc,
                                                  ID3D11Counter **ppCounter)
{
  SERIALISE_ELEMENT_PTR(D3D11_COUNTER_DESC, Descriptor, pCounterDesc);
  SERIALISE_ELEMENT(ResourceId, Counter, GetIDForResource(*ppCounter));

  if(m_State == READING)
  {
    ID3D11Counter *ret;
    HRESULT hr = m_pDevice->CreateCounter(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11Counter(ret, this);

      GetResourceManager()->AddLiveResource(Counter, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc,
                                           ID3D11Counter **ppCounter)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppCounter == NULL)
    return m_pDevice->CreateCounter(pCounterDesc, NULL);

  ID3D11Counter *real = NULL;
  HRESULT ret = m_pDevice->CreateCounter(pCounterDesc, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    *ppCounter = new WrappedID3D11Counter(real, this);
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_CreateDeferredContext(const UINT ContextFlags,
                                                          ID3D11DeviceContext **ppDeferredContext)
{
  SERIALISE_ELEMENT(uint32_t, Flags, ContextFlags);
  SERIALISE_ELEMENT(ResourceId, Context, GetIDForResource(*ppDeferredContext));

  if(m_State == READING)
  {
    ID3D11DeviceContext *ret;
    HRESULT hr = m_pDevice->CreateDeferredContext(Flags, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
    }
    else
    {
      ret = new WrappedID3D11DeviceContext(this, ret, m_pSerialiser);

      AddDeferredContext((WrappedID3D11DeviceContext *)ret);

      GetResourceManager()->AddLiveResource(Context, ret);
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::CreateDeferredContext(UINT ContextFlags,
                                                   ID3D11DeviceContext **ppDeferredContext)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDeferredContext == NULL)
    return m_pDevice->CreateDeferredContext(ContextFlags, NULL);

  ID3D11DeviceContext *real = NULL;
  ID3D11DeviceContext *wrapped = NULL;
  HRESULT ret = m_pDevice->CreateDeferredContext(ContextFlags, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D11DeviceContext *w = new WrappedID3D11DeviceContext(this, real, m_pSerialiser);

    wrapped = w;

    if(m_State == WRITING_CAPFRAME)
      w->AttemptCapture();

    if(m_State >= WRITING)
    {
      AddDeferredContext(w);

      SCOPED_SERIALISE_CONTEXT(CREATE_DEFERRED_CONTEXT);
      Serialise_CreateDeferredContext(ContextFlags, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppDeferredContext = wrapped;
  }

  return ret;
}

bool WrappedID3D11Device::Serialise_OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
                                                       void **ppResource)
{
  SERIALISE_ELEMENT(ResourceType, type, IdentifyTypeByPtr((IUnknown *)*ppResource));
  SERIALISE_ELEMENT(ResourceId, pResource, GetIDForResource((ID3D11DeviceChild *)*ppResource));

  if(type == Resource_Buffer)
  {
    D3D11_BUFFER_DESC desc;
    RDCEraseEl(desc);

    if(m_State >= WRITING)
    {
      ID3D11Buffer *buf = (ID3D11Buffer *)*ppResource;
      buf->GetDesc(&desc);
    }

    SERIALISE_ELEMENT(D3D11_BUFFER_DESC, Descriptor, desc);

    char *dummy = new char[Descriptor.ByteWidth];
    SERIALISE_ELEMENT_BUF(byte *, InitialData, dummy, Descriptor.ByteWidth);
    delete[] dummy;

    uint64_t offs = m_pSerialiser->GetOffset() - Descriptor.ByteWidth;

    RDCASSERT((offs % 16) == 0);

    if(m_State >= WRITING)
    {
      RDCASSERT(GetResourceManager()->GetResourceRecord(pResource) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(pResource);
      record->SetDataOffset(offs);
      record->DataInSerialiser = true;
      record->Length = Descriptor.ByteWidth;
    }

    if(m_State == READING)
    {
      ID3D11Buffer *ret;

      HRESULT hr = S_OK;

      // unset flags that are unimportant/problematic in replay
      Descriptor.MiscFlags &=
          ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
            D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

      D3D11_SUBRESOURCE_DATA data;
      data.pSysMem = InitialData;
      data.SysMemPitch = Descriptor.ByteWidth;
      data.SysMemSlicePitch = Descriptor.ByteWidth;
      hr = m_pDevice->CreateBuffer(&Descriptor, &data, &ret);

      if(FAILED(hr))
      {
        RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
      }
      else
      {
        ret = new WrappedID3D11Buffer(ret, Descriptor.ByteWidth, this);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }

      if(Descriptor.Usage != D3D11_USAGE_IMMUTABLE)
      {
        ID3D11Buffer *stage = NULL;

        RDCEraseEl(desc);
        desc.ByteWidth = Descriptor.ByteWidth;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;
        // We don't need to bind this, but IMMUTABLE requires at least one
        // BindFlags.
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;

        hr = m_pDevice->CreateBuffer(&desc, &data, &stage);

        if(FAILED(hr) || stage == NULL)
        {
          RDCERR("Failed to create staging buffer for buffer initial contents %08x", hr);
        }
        else
        {
          m_ResourceManager->SetInitialContents(pResource, D3D11ResourceManager::InitialContentData(
                                                               stage, eInitialContents_Copy, NULL));
        }
      }

      SAFE_DELETE_ARRAY(InitialData);
    }
  }
  else if(type == Resource_Texture1D)
  {
    D3D11_TEXTURE1D_DESC desc;
    RDCEraseEl(desc);

    if(m_State >= WRITING)
    {
      ID3D11Texture1D *tex = (ID3D11Texture1D *)*ppResource;
      tex->GetDesc(&desc);
    }

    SERIALISE_ELEMENT(D3D11_TEXTURE1D_DESC, Descriptor, desc);

    Serialise_CreateTextureData(ppResource ? (ID3D11Texture1D *)*ppResource : NULL, pResource, NULL,
                                Descriptor.Width, 1, 1, Descriptor.Format, Descriptor.MipLevels,
                                Descriptor.ArraySize, false);

    if(m_State == READING)
    {
      ID3D11Texture1D *ret;
      HRESULT hr = S_OK;

      TextureDisplayType dispType = DispTypeForTexture(Descriptor);

      // unset flags that are unimportant/problematic in replay
      Descriptor.MiscFlags &=
          ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
            D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

      hr = m_pDevice->CreateTexture1D(&Descriptor, NULL, &ret);

      if(FAILED(hr))
      {
        RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
      }
      else
      {
        ret = new WrappedID3D11Texture1D(ret, this, dispType);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }
    }
  }
  else if(type == Resource_Texture2D)
  {
    D3D11_TEXTURE2D_DESC desc;
    RDCEraseEl(desc);

    if(m_State >= WRITING)
    {
      ID3D11Texture2D *tex = (ID3D11Texture2D *)*ppResource;
      tex->GetDesc(&desc);
    }

    SERIALISE_ELEMENT(D3D11_TEXTURE2D_DESC, Descriptor, desc);

    Serialise_CreateTextureData(ppResource ? (ID3D11Texture2D *)*ppResource : NULL, pResource, NULL,
                                Descriptor.Width, Descriptor.Height, 1, Descriptor.Format,
                                Descriptor.MipLevels, Descriptor.ArraySize, false);

    if(m_State == READING)
    {
      ID3D11Texture2D *ret;
      HRESULT hr = S_OK;

      TextureDisplayType dispType = DispTypeForTexture(Descriptor);

      // unset flags that are unimportant/problematic in replay
      Descriptor.MiscFlags &=
          ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
            D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

      hr = m_pDevice->CreateTexture2D(&Descriptor, NULL, &ret);

      if(FAILED(hr))
      {
        RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
      }
      else
      {
        ret = new WrappedID3D11Texture2D1(ret, this, dispType);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }
    }
  }
  else if(type == Resource_Texture3D)
  {
    D3D11_TEXTURE3D_DESC desc;
    RDCEraseEl(desc);

    if(m_State >= WRITING)
    {
      ID3D11Texture3D *tex = (ID3D11Texture3D *)*ppResource;
      tex->GetDesc(&desc);
    }

    SERIALISE_ELEMENT(D3D11_TEXTURE3D_DESC, Descriptor, desc);

    Serialise_CreateTextureData(ppResource ? (ID3D11Texture3D *)*ppResource : NULL, pResource, NULL,
                                Descriptor.Width, Descriptor.Height, Descriptor.Depth,
                                Descriptor.Format, Descriptor.MipLevels, 1, false);

    if(m_State == READING)
    {
      ID3D11Texture3D *ret;
      HRESULT hr = S_OK;

      TextureDisplayType dispType = DispTypeForTexture(Descriptor);

      // unset flags that are unimportant/problematic in replay
      Descriptor.MiscFlags &=
          ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
            D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

      hr = m_pDevice->CreateTexture3D(&Descriptor, NULL, &ret);

      if(FAILED(hr))
      {
        RDCERR("Failed on resource serialise-creation, HRESULT: 0x%08x", hr);
      }
      else
      {
        ret = new WrappedID3D11Texture3D1(ret, this, dispType);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }
    }
  }

  return true;
}

HRESULT WrappedID3D11Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
                                                void **ppResource)
{
  if(m_State < WRITING || ppResource == NULL)
    return E_INVALIDARG;

  bool isDXGIRes = (ReturnedInterface == __uuidof(IDXGIResource) ? true : false);
  bool isRes = (ReturnedInterface == __uuidof(ID3D11Resource) ? true : false);
  bool isBuf = (ReturnedInterface == __uuidof(ID3D11Buffer) ? true : false);
  bool isTex1D = (ReturnedInterface == __uuidof(ID3D11Texture1D) ? true : false);
  bool isTex2D = (ReturnedInterface == __uuidof(ID3D11Texture2D) ? true : false);
  bool isTex3D = (ReturnedInterface == __uuidof(ID3D11Texture3D) ? true : false);

  if(isDXGIRes || isRes || isBuf || isTex1D || isTex2D || isTex3D)
  {
    void *res = NULL;
    HRESULT hr = m_pDevice->OpenSharedResource(hResource, ReturnedInterface, &res);

    if(FAILED(hr))
    {
      IUnknown *unk = (IUnknown *)res;
      SAFE_RELEASE(unk);
      return hr;
    }
    else
    {
      if(isDXGIRes)
      {
        IDXGIResource *dxgiRes = (IDXGIResource *)res;

        ID3D11Resource *d3d11Res = NULL;
        hr = dxgiRes->QueryInterface(__uuidof(ID3D11Resource), (void **)&d3d11Res);

        // if we can't get a d3d11Res then we can't properly wrap this resource,
        // whatever it is.
        if(FAILED(hr) || d3d11Res == NULL)
        {
          SAFE_RELEASE(d3d11Res);
          SAFE_RELEASE(dxgiRes);
          return E_NOINTERFACE;
        }

        // release this interface
        SAFE_RELEASE(dxgiRes);

        // and use this one, so it'll be casted back below
        res = (void *)d3d11Res;
        isRes = true;
      }

      SCOPED_LOCK(m_D3DLock);

      ResourceId wrappedID;

      if(isRes)
      {
        ID3D11Resource *resource = (ID3D11Resource *)res;
        D3D11_RESOURCE_DIMENSION dim;
        resource->GetType(&dim);

        if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
        {
          res = (ID3D11Buffer *)(ID3D11Resource *)res;
          isBuf = true;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
        {
          res = (ID3D11Texture1D *)(ID3D11Resource *)res;
          isTex1D = true;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
          res = (ID3D11Texture2D *)(ID3D11Resource *)res;
          isTex2D = true;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
        {
          res = (ID3D11Texture3D *)(ID3D11Resource *)res;
          isTex3D = true;
        }
      }

      ID3D11Resource *realRes = NULL;

      if(isBuf)
      {
        WrappedID3D11Buffer *w = new WrappedID3D11Buffer((ID3D11Buffer *)res, 0, this);
        wrappedID = w->GetResourceID();

        realRes = w->GetReal();

        *ppResource = (ID3D11Buffer *)w;
      }
      else if(isTex1D)
      {
        WrappedID3D11Texture1D *w = new WrappedID3D11Texture1D((ID3D11Texture1D *)res, this);
        wrappedID = w->GetResourceID();

        realRes = w->GetReal();

        *ppResource = (ID3D11Texture1D *)w;
      }
      else if(isTex2D)
      {
        WrappedID3D11Texture2D1 *w = new WrappedID3D11Texture2D1((ID3D11Texture2D *)res, this);
        wrappedID = w->GetResourceID();

        realRes = w->GetReal();

        *ppResource = (ID3D11Texture2D *)w;
      }
      else if(isTex3D)
      {
        WrappedID3D11Texture3D1 *w = new WrappedID3D11Texture3D1((ID3D11Texture3D *)res, this);
        wrappedID = w->GetResourceID();

        realRes = w->GetReal();

        *ppResource = (ID3D11Texture3D *)w;
      }

      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(OPEN_SHARED_RESOURCE);
        Serialise_OpenSharedResource(hResource, ReturnedInterface, ppResource);

        chunk = scope.Get();
      }

      // don't know where this came from or who might modify it at any point.
      GetResourceManager()->MarkDirtyResource(wrappedID);

      D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(wrappedID);
      RDCASSERT(record);

      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
    }

    return S_OK;
  }

  return E_NOINTERFACE;
}

HRESULT WrappedID3D11Device::CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport)
{
  return m_pDevice->CheckFormatSupport(Format, pFormatSupport);
}

HRESULT WrappedID3D11Device::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount,
                                                           UINT *pNumQualityLevels)
{
  return m_pDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}

void WrappedID3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo)
{
  m_pDevice->CheckCounterInfo(pCounterInfo);
}

HRESULT WrappedID3D11Device::CheckCounter(const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType,
                                          UINT *pActiveCounters, LPSTR szName, UINT *pNameLength,
                                          LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription,
                                          UINT *pDescriptionLength)
{
  return m_pDevice->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits,
                                 pUnitsLength, szDescription, pDescriptionLength);
}

HRESULT WrappedID3D11Device::CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData,
                                                 UINT FeatureSupportDataSize)
{
  return m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

HRESULT WrappedID3D11Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
  return m_pDevice->GetPrivateData(guid, pDataSize, pData);
}

HRESULT WrappedID3D11Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
  return m_pDevice->SetPrivateData(guid, DataSize, pData);
}

HRESULT WrappedID3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
  return m_pDevice->SetPrivateDataInterface(guid, pData);
}

D3D_FEATURE_LEVEL WrappedID3D11Device::GetFeatureLevel()
{
  return m_pDevice->GetFeatureLevel();
}

UINT WrappedID3D11Device::GetCreationFlags()
{
  return m_pDevice->GetCreationFlags();
}

HRESULT WrappedID3D11Device::GetDeviceRemovedReason()
{
  return m_pDevice->GetDeviceRemovedReason();
}

void WrappedID3D11Device::GetImmediateContext(ID3D11DeviceContext **ppImmediateContext)
{
  if(ppImmediateContext)
  {
    *ppImmediateContext = (ID3D11DeviceContext *)m_pImmediateContext;
    m_pImmediateContext->AddRef();
  }
}

bool WrappedID3D11Device::Serialise_SetExceptionMode(UINT RaiseFlags)
{
  SERIALISE_ELEMENT(uint32_t, Flags, RaiseFlags);

  if(m_State == READING)
  {
    m_pDevice->SetExceptionMode(Flags);
  }

  return true;
}

HRESULT WrappedID3D11Device::SetExceptionMode(UINT RaiseFlags)
{
  HRESULT ret = m_pDevice->SetExceptionMode(RaiseFlags);

  if(SUCCEEDED(ret) && m_State >= WRITING)
  {
    SCOPED_LOCK(m_D3DLock);

    SCOPED_SERIALISE_CONTEXT(SET_EXCEPTION_MODE);
    Serialise_SetExceptionMode(RaiseFlags);

    m_DeviceRecord->AddChunk(scope.Get());
  }

  return ret;
}

UINT WrappedID3D11Device::GetExceptionMode()
{
  return m_pDevice->GetExceptionMode();
}
