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
#include "d3d11_debug.h"
#include "d3d11_resources.h"

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateBuffer(SerialiserType &ser, const D3D11_BUFFER_DESC *pDesc,
                                                 const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                 ID3D11Buffer **ppBuffer)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
  // unused, just for the sake of the user
  SERIALISE_ELEMENT_OPT(pInitialData);
  SERIALISE_ELEMENT_LOCAL(pBuffer, GetIDForResource(*ppBuffer)).TypedAs("ID3D11Buffer *"_lit);

  D3D11_SUBRESOURCE_DATA fakeData;
  RDCEraseEl(fakeData);
  // we always want buffers to have data, so we create some to serialise
  if(pInitialData == NULL && ser.IsWriting())
  {
    fakeData.pSysMem = new byte[Descriptor.ByteWidth];
    fakeData.SysMemPitch = fakeData.SysMemSlicePitch = Descriptor.ByteWidth;
    // fill with 0xdddddddd to indicate that the data is uninitialised, if that option is enabled
    memset((void *)fakeData.pSysMem,
           RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 0xdd : 0x0,
           Descriptor.ByteWidth);
    pInitialData = &fakeData;
  }

  // work around an nvidia driver bug, if a buffer is created as IMMUTABLE then it
  // can't be CopySubresourceRegion'd with a box offset, the data that's read is
  // wrong.
  if(IsReplayingAndReading() && Descriptor.Usage == D3D11_USAGE_IMMUTABLE)
  {
    Descriptor.Usage = D3D11_USAGE_DEFAULT;
    // paranoid - I don't know what requirements might change, so set some sane default
    if(Descriptor.BindFlags == 0)
      Descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  }

  const void *InitialData = NULL;
  uint64_t InitialDataLength = 0;

  if(ser.IsWriting())
  {
    InitialData = pInitialData->pSysMem;
    InitialDataLength = Descriptor.ByteWidth;
  }

  SERIALISE_ELEMENT_ARRAY(InitialData, InitialDataLength);

  if(ser.IsWriting())
  {
    uint64_t offs = ser.GetWriter()->GetOffset() - InitialDataLength;

    RDCASSERT((offs % 64) == 0);

    RDCASSERT(GetResourceManager()->GetResourceRecord(pBuffer) == NULL);

    D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(pBuffer);
    record->ResType = Resource_Buffer;
    record->SetDataOffset(offs);
    record->DataInSerialiser = true;
    record->Length = Descriptor.ByteWidth;
  }

  SERIALISE_ELEMENT(InitialDataLength);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
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
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Buffer(ret, Descriptor.ByteWidth, this);

      GetResourceManager()->AddLiveResource(pBuffer, ret);
    }

    AddResource(pBuffer, ResourceType::Buffer, "Buffer");

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
        RDCERR("Failed to create staging buffer for buffer initial contents HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_ResourceManager->SetInitialContents(pBuffer, D3D11InitialContents(Resource_Buffer, stage));
      }
    }
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

  bool intelExtensionMagic = false;
  byte intelExtensionData[28] = {0};

  // snoop to disable the absurdly implemented intel DX11 extensions.
  if(pDesc->ByteWidth == sizeof(intelExtensionData) && pDesc->Usage == D3D11_USAGE_STAGING &&
     pInitialData && pDesc->BindFlags == 0)
  {
    byte *data = (byte *)pInitialData->pSysMem;

    if(!memcmp(data, "INTCEXTN", 8))
    {
      RDCLOG("Intercepting and preventing attempt to initialise intel extensions.");

      intelExtensionMagic = true;

      // back-up the data from the user
      memcpy(intelExtensionData, data, sizeof(intelExtensionData));

      // overwrite the initial data, so the driver doesn't see the request (it just sees an empty
      // buffer). Just in case passing along the real data does something.
      memset(data, 0, sizeof(intelExtensionData));
    }
  }

  ID3D11Buffer *real = NULL;
  ID3D11Buffer *wrapped = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateBuffer(pDesc, pInitialData, &real));

  if(intelExtensionMagic)
  {
    byte *data = (byte *)pInitialData->pSysMem;

    // restore the user's data unmodified, which is the expected behaviour when the extensions
    // aren't supported.
    memcpy(data, intelExtensionData, sizeof(intelExtensionData));
  }

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Buffer(real, pDesc->ByteWidth, this);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateBuffer);
        Serialise_CreateBuffer(GET_SERIALISER, pDesc, pInitialData, &wrapped);
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

template <typename SerialiserType>
std::vector<D3D11_SUBRESOURCE_DATA> WrappedID3D11Device::Serialise_CreateTextureData(
    SerialiserType &ser, ID3D11Resource *tex, ResourceId id, const D3D11_SUBRESOURCE_DATA *data,
    UINT w, UINT h, UINT d, DXGI_FORMAT fmt, UINT mips, UINT arr, bool HasData)
{
  UINT numSubresources = mips;
  UINT numMips = mips;

  if(mips == 0)
    numSubresources = numMips = CalcNumMips(w, h, d);

  numSubresources *= arr;

  std::vector<D3D11_SUBRESOURCE_DATA> descs;
  if(IsReplayingAndReading() && HasData)
    descs.resize(numSubresources);

  // scratch memory. Used to linearise & tightly pack incoming data for serialising, so that we know
  // the data is in a fixed and predictable layout for
  byte *scratch = NULL;

  for(UINT i = 0; i < numSubresources; i++)
  {
    int mip = i % numMips;

    byte *SubresourceContents = NULL;
    UINT SubresourceContentsLength = GetByteSize(w, h, d, fmt, mip);

    RDCASSERT(SubresourceContentsLength > 0);

    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(id);

    if(ser.IsWriting())
    {
      // on the first iteration, set up the resource record
      if(i == 0)
      {
        RDCASSERT(record == NULL);

        record = GetResourceManager()->AddResourceRecord(id);
        record->ResType = IdentifyTypeByPtr(tex);
        record->Length = 1;

        if(HasData)
          record->DataInSerialiser = true;

        record->NumSubResources = numSubresources;
        record->SubResources = new D3D11ResourceRecord *[record->NumSubResources];
        for(UINT s = 0; s < numSubresources; s++)
        {
          record->SubResources[s] = new D3D11ResourceRecord(ResourceId());
          record->SubResources[s]->ResType = record->ResType;
          record->SubResources[s]->DataInSerialiser = HasData;
        }
      }

      RDCASSERT(record);

      record->SubResources[i]->Length = SubresourceContentsLength;
    }

    // skip the remaining if there's no data to serialise
    if(!HasData)
      continue;

    // we linearise the data so that future functions know the format of the data in the record
    if(ser.IsWriting())
    {
      // since we iterate from largest subresource down, we only need to allocate once - the rest of
      // the subresources use progressively less of the allocation at a time
      if(scratch == NULL)
        scratch = new byte[SubresourceContentsLength];

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

      SubresourceContents = scratch;
    }

    SERIALISE_ELEMENT_ARRAY(SubresourceContents, SubresourceContentsLength);

    if(ser.IsWriting())
    {
      RDCASSERT(record);

      record->SubResources[i]->SetDataOffset(ser.GetWriter()->GetOffset() - SubresourceContentsLength);
    }

    SERIALISE_ELEMENT(SubresourceContentsLength);

    if(IsReplayingAndReading())
    {
      // we 'steal' the SubresourceContents buffer so it doesn't get de-serialised and free'd
      descs[i].pSysMem = SubresourceContents;
      SubresourceContents = NULL;

      // calculate tightly packed pitches
      descs[i].SysMemPitch = GetRowPitch(w, fmt, mip);
      descs[i].SysMemSlicePitch = GetByteSize(w, h, 1, fmt, mip);
    }
  }

  // delete our scratch memory
  SAFE_DELETE_ARRAY(scratch);

  return descs;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateTexture1D(SerialiserType &ser,
                                                    const D3D11_TEXTURE1D_DESC *pDesc,
                                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                    ID3D11Texture1D **ppTexture1D)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc);

  // unused, just for the sake of the user
  {
    UINT numSubresources =
        Descriptor.MipLevels ? Descriptor.MipLevels : CalcNumMips(Descriptor.Width, 1, 1);
    numSubresources *= Descriptor.ArraySize;

    SERIALISE_ELEMENT_ARRAY(pInitialData, pInitialData ? numSubresources : 0);
  }

  SERIALISE_ELEMENT_LOCAL(pTexture, GetIDForResource(*ppTexture1D))
      .TypedAs("ID3D11Texture1D *"_lit);

  std::vector<D3D11_SUBRESOURCE_DATA> descs = Serialise_CreateTextureData(
      ser, ppTexture1D ? *ppTexture1D : NULL, pTexture, pInitialData, Descriptor.Width, 1, 1,
      Descriptor.Format, Descriptor.MipLevels, Descriptor.ArraySize, pInitialData != NULL);

  if(IsReplayingAndReading() && ser.IsErrored())
  {
    // need to manually free the serialised buffers we stole in Serialise_CreateTextureData here,
    // before the SERIALISE_CHECK_READ_ERRORS macro below returns out of the function
    for(size_t i = 0; i < descs.size(); i++)
      FreeAlignedBuffer((byte *)descs[i].pSysMem);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11Texture1D *ret;
    HRESULT hr = S_OK;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(pInitialData != NULL)
      hr = m_pDevice->CreateTexture1D(&Descriptor, &descs[0], &ret);
    else
      hr = m_pDevice->CreateTexture1D(&Descriptor, NULL, &ret);

    APIProps.YUVTextures |= IsYUVFormat(Descriptor.Format);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Texture1D(ret, this, dispType);

      GetResourceManager()->AddLiveResource(pTexture, ret);
    }

    const char *prefix = Descriptor.ArraySize > 1 ? "1D TextureArray" : "1D Texture";

    if(Descriptor.BindFlags & D3D11_BIND_RENDER_TARGET)
      prefix = "1D Render Target";
    else if(Descriptor.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      prefix = "1D Depth Target";

    AddResource(pTexture, ResourceType::Texture, prefix);

    // free the serialised buffers we stole in Serialise_CreateTextureData
    for(size_t i = 0; i < descs.size(); i++)
      FreeAlignedBuffer((byte *)descs[i].pSysMem);
  }

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateTexture1D(pDesc, pInitialData, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture1D(real, this);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateTexture1D);
        Serialise_CreateTexture1D(GET_SERIALISER, pDesc, pInitialData, &wrapped);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateTexture2D(SerialiserType &ser,
                                                    const D3D11_TEXTURE2D_DESC *pDesc,
                                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                    ID3D11Texture2D **ppTexture2D)
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11Texture2D *ret;
    HRESULT hr = S_OK;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    APIProps.YUVTextures |= IsYUVFormat(Descriptor.Format);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(pInitialData != NULL)
      hr = m_pDevice->CreateTexture2D(&Descriptor, &descs[0], &ret);
    else
      hr = m_pDevice->CreateTexture2D(&Descriptor, NULL, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Texture2D1(ret, this, dispType);

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

HRESULT WrappedID3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture2D **ppTexture2D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture2D == NULL)
    return m_pDevice->CreateTexture2D(pDesc, pInitialData, NULL);

  ID3D11Texture2D *real = NULL;
  ID3D11Texture2D *wrapped = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateTexture2D(pDesc, pInitialData, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture2D1(real, this);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateTexture2D);
        Serialise_CreateTexture2D(GET_SERIALISER, pDesc, pInitialData, &wrapped);

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
bool WrappedID3D11Device::Serialise_CreateTexture3D(SerialiserType &ser,
                                                    const D3D11_TEXTURE3D_DESC *pDesc,
                                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                    ID3D11Texture3D **ppTexture3D)
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11Texture3D *ret;
    HRESULT hr = S_OK;

    TextureDisplayType dispType = DispTypeForTexture(Descriptor);

    APIProps.YUVTextures |= IsYUVFormat(Descriptor.Format);

    // unset flags that are unimportant/problematic in replay
    Descriptor.MiscFlags &=
        ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
          D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

    if(pInitialData != NULL)
      hr = m_pDevice->CreateTexture3D(&Descriptor, &descs[0], &ret);
    else
      hr = m_pDevice->CreateTexture3D(&Descriptor, NULL, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Texture3D1(ret, this, dispType);

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

HRESULT WrappedID3D11Device::CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture3D **ppTexture3D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture3D == NULL)
    return m_pDevice->CreateTexture3D(pDesc, pInitialData, NULL);

  ID3D11Texture3D *real = NULL;
  ID3D11Texture3D *wrapped = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateTexture3D(pDesc, pInitialData, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Texture3D1(real, this);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateTexture3D);
        Serialise_CreateTexture3D(GET_SERIALISER, pDesc, pInitialData, &wrapped);

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
bool WrappedID3D11Device::Serialise_CreateShaderResourceView(
    SerialiserType &ser, ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
    ID3D11ShaderResourceView **ppSRView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppSRView))
      .TypedAs("ID3D11ShaderResourceView *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11ShaderResourceView *ret;

    D3D11_SHADER_RESOURCE_VIEW_DESC *pSRVDesc = (D3D11_SHADER_RESOURCE_VIEW_DESC *)pDesc;

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)pResource;

    D3D11_SHADER_RESOURCE_VIEW_DESC backbufferTypedDesc;

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

    if(pSRVDesc)
      APIProps.YUVTextures |= IsYUVFormat(pSRVDesc->Format);

    HRESULT hr = m_pDevice->CreateShaderResourceView(
        GetResourceManager()->UnwrapResource(pResource), pSRVDesc, &ret);

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateShaderResourceView(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ShaderResourceView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateShaderResourceView);
      Serialise_CreateShaderResourceView(GET_SERIALISER, pResource, pDesc, &wrapped);

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
bool WrappedID3D11Device::Serialise_CreateUnorderedAccessView(
    SerialiserType &ser, ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
    ID3D11UnorderedAccessView **ppUAView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppUAView))
      .TypedAs("ID3D11UnorderedAccessView *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11UnorderedAccessView *ret;

    D3D11_UNORDERED_ACCESS_VIEW_DESC *pUAVDesc = (D3D11_UNORDERED_ACCESS_VIEW_DESC *)pDesc;

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)pResource;

    D3D11_UNORDERED_ACCESS_VIEW_DESC backbufferTypedDesc;

    // need to fixup typeless backbuffer fudging, if a descriptor isn't specified then
    // we need to make one to give the correct type
    if(!pDesc && WrappedID3D11Texture2D1::IsAlloc(pResource) && tex2d->m_RealDescriptor)
    {
      // MSAA UAVs aren't supported so this must be non-MSAA
      backbufferTypedDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
      backbufferTypedDesc.Format = tex2d->m_RealDescriptor->Format;
      backbufferTypedDesc.Texture2D.MipSlice = 0;
      pUAVDesc = &backbufferTypedDesc;
    }

    // if we have a descriptor but it specifies DXGI_FORMAT_UNKNOWN format, that means use
    // the texture's format. But as above, we fudge around the typeless backbuffer so we
    // have to set the correct typed format
    //
    // This behaviour is documented only for render targets, but seems to be used & work for
    // UAVs, so apply it here too.
    if(pUAVDesc && pUAVDesc->Format == DXGI_FORMAT_UNKNOWN &&
       WrappedID3D11Texture2D1::IsAlloc(pResource) && tex2d->m_RealDescriptor)
    {
      pUAVDesc->Format = tex2d->m_RealDescriptor->Format;
    }

    HRESULT hr = m_pDevice->CreateUnorderedAccessView(
        GetResourceManager()->UnwrapResource(pResource), pUAVDesc, &ret);

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateUnorderedAccessView(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11UnorderedAccessView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateUnorderedAccessView);
      Serialise_CreateUnorderedAccessView(GET_SERIALISER, pResource, pDesc, &wrapped);

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

        // if this UAV has a hidden counter, immediately mark it as dirty so we force initial
        // contents to fetch its counter
        if(pDesc->ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
           (pDesc->Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER | D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
        {
          GetResourceManager()->MarkDirtyResource(id);
        }
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
bool WrappedID3D11Device::Serialise_CreateRenderTargetView(SerialiserType &ser,
                                                           ID3D11Resource *pResource,
                                                           const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                                           ID3D11RenderTargetView **ppRTView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppRTView))
      .TypedAs("ID3D11RenderTargetView *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11RenderTargetView *ret;

    D3D11_RENDER_TARGET_VIEW_DESC *pRTVDesc = (D3D11_RENDER_TARGET_VIEW_DESC *)pDesc;

    WrappedID3D11Texture2D1 *tex2d = (WrappedID3D11Texture2D1 *)pResource;

    D3D11_RENDER_TARGET_VIEW_DESC backbufferTypedDesc;

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

    HRESULT hr = m_pDevice->CreateRenderTargetView(GetResourceManager()->UnwrapResource(pResource),
                                                   pRTVDesc, &ret);

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateRenderTargetView(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11RenderTargetView1(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateRenderTargetView);
      Serialise_CreateRenderTargetView(GET_SERIALISER, pResource, pDesc, &wrapped);

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
bool WrappedID3D11Device::Serialise_CreateDepthStencilView(
    SerialiserType &ser, ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
    ID3D11DepthStencilView **ppDepthStencilView)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pDesc);
  SERIALISE_ELEMENT_LOCAL(pView, GetIDForResource(*ppDepthStencilView))
      .TypedAs("ID3D11DepthStencilView *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ID3D11DepthStencilView *ret;

    HRESULT hr = m_pDevice->CreateDepthStencilView(GetResourceManager()->UnwrapResource(pResource),
                                                   pDesc, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11DepthStencilView(ret, pResource, this);

      GetResourceManager()->AddLiveResource(pView, ret);
    }

    AddResource(pView, ResourceType::View, "Depth Stencil View");
    DerivedResource(pResource, pView);
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateDepthStencilView(
                          GetResourceManager()->UnwrapResource(pResource), pDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11DepthStencilView(real, pResource, this);

    Chunk *chunk = NULL;

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateDepthStencilView);
      Serialise_CreateDepthStencilView(GET_SERIALISER, pResource, pDesc, &wrapped);

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
        record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateInputLayout(
    SerialiserType &ser, const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
    const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength_,
    ID3D11InputLayout **ppInputLayout)
{
  SERIALISE_ELEMENT_ARRAY(pInputElementDescs, NumElements);
  SERIALISE_ELEMENT(NumElements);
  SERIALISE_ELEMENT_ARRAY(pShaderBytecodeWithInputSignature, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT_LOCAL(pInputLayout, GetIDForResource(*ppInputLayout))
      .TypedAs("ID3D11InputLayout *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // for no explicable reason, CreateInputLayout requires a descriptor pointer even if
    // NumElements==0.
    D3D11_INPUT_ELEMENT_DESC dummy = {};
    const D3D11_INPUT_ELEMENT_DESC *inputDescs = pInputElementDescs ? pInputElementDescs : &dummy;

    ID3D11InputLayout *ret = NULL;
    HRESULT hr = m_pDevice->CreateInputLayout(
        inputDescs, NumElements, pShaderBytecodeWithInputSignature, (SIZE_T)BytecodeLength, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11InputLayout(ret, this);

      GetResourceManager()->AddLiveResource(pInputLayout, ret);
    }

    AddResource(pInputLayout, ResourceType::StateObject, "Input Layout");

    if(NumElements > 0)
      m_LayoutDescs[ret] = std::vector<D3D11_INPUT_ELEMENT_DESC>(pInputElementDescs,
                                                                 pInputElementDescs + NumElements);

    if(BytecodeLength > 0 && pShaderBytecodeWithInputSignature)
      m_LayoutShaders[ret] = new WrappedShader(this, pInputLayout, GetIDForResource(ret),
                                               (const byte *)pShaderBytecodeWithInputSignature,
                                               (size_t)BytecodeLength);
  }

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateInputLayout(pInputElementDescs, NumElements,
                                                         pShaderBytecodeWithInputSignature,
                                                         BytecodeLength, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11InputLayout(real, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateInputLayout);
      Serialise_CreateInputLayout(GET_SERIALISER, pInputElementDescs, NumElements,
                                  pShaderBytecodeWithInputSignature, BytecodeLength, &wrapped);

      WrappedID3D11InputLayout *lay = (WrappedID3D11InputLayout *)wrapped;
      ResourceId id = lay->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }
  }

  *ppInputLayout = wrapped;

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateVertexShader(SerialiserType &ser,
                                                       const void *pShaderBytecode,
                                                       SIZE_T BytecodeLength_,
                                                       ID3D11ClassLinkage *pClassLinkage,
                                                       ID3D11VertexShader **ppVertexShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppVertexShader))
      .TypedAs("ID3D11VertexShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11VertexShader *ret;
    HRESULT hr =
        m_pDevice->CreateVertexShader(pShaderBytecode, (SIZE_T)BytecodeLength,
                                      UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11VertexShader>(ret, pShader, (const byte *)pShaderBytecode,
                                                        (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Vertex Shader");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength,
                                          UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11VertexShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateVertexShader);
      Serialise_CreateVertexShader(GET_SERIALISER, pShaderBytecode, BytecodeLength, pClassLinkage,
                                   &wrapped);

      WrappedID3D11Shader<ID3D11VertexShader> *sh =
          (WrappedID3D11Shader<ID3D11VertexShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateGeometryShader(SerialiserType &ser,
                                                         const void *pShaderBytecode,
                                                         SIZE_T BytecodeLength_,
                                                         ID3D11ClassLinkage *pClassLinkage,
                                                         ID3D11GeometryShader **ppGeometryShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppGeometryShader))
      .TypedAs("ID3D11GeometryShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11GeometryShader *ret;
    HRESULT hr =
        m_pDevice->CreateGeometryShader(pShaderBytecode, (SIZE_T)BytecodeLength,
                                        UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11GeometryShader>(
          ret, pShader, (const byte *)pShaderBytecode, (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Geometry Shader");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateGeometryShader(
                          pShaderBytecode, BytecodeLength,
                          UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11GeometryShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateGeometryShader);
      Serialise_CreateGeometryShader(GET_SERIALISER, pShaderBytecode, BytecodeLength, pClassLinkage,
                                     &wrapped);

      WrappedID3D11Shader<ID3D11GeometryShader> *sh =
          (WrappedID3D11Shader<ID3D11GeometryShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateGeometryShaderWithStreamOutput(
    SerialiserType &ser, const void *pShaderBytecode, SIZE_T BytecodeLength_,
    const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides,
    UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage,
    ID3D11GeometryShader **ppGeometryShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT_ARRAY(pSODeclaration, NumEntries);
  SERIALISE_ELEMENT(NumEntries);
  SERIALISE_ELEMENT_ARRAY(pBufferStrides, NumStrides);
  SERIALISE_ELEMENT(NumStrides);
  SERIALISE_ELEMENT(RasterizedStream);
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppGeometryShader))
      .TypedAs("ID3D11GeometryShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11GeometryShader *ret;
    HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
        pShaderBytecode, (SIZE_T)BytecodeLength, pSODeclaration, NumEntries, pBufferStrides,
        NumStrides, RasterizedStream, UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11GeometryShader>(
          ret, pShader, (const byte *)pShaderBytecode, (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Geometry Shader");
  }

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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateGeometryShaderWithStreamOutput(
                          pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries,
                          pBufferStrides, NumStrides, RasterizedStream,
                          UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11GeometryShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateGeometryShaderWithStreamOutput);
      Serialise_CreateGeometryShaderWithStreamOutput(
          GET_SERIALISER, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries,
          pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, &wrapped);

      WrappedID3D11Shader<ID3D11GeometryShader> *sh =
          (WrappedID3D11Shader<ID3D11GeometryShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreatePixelShader(SerialiserType &ser,
                                                      const void *pShaderBytecode,
                                                      SIZE_T BytecodeLength_,
                                                      ID3D11ClassLinkage *pClassLinkage,
                                                      ID3D11PixelShader **ppPixelShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppPixelShader))
      .TypedAs("ID3D11PixelShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11PixelShader *ret;
    HRESULT hr = m_pDevice->CreatePixelShader(pShaderBytecode, (SIZE_T)BytecodeLength,
                                              UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11PixelShader>(ret, pShader, (const byte *)pShaderBytecode,
                                                       (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Pixel Shader");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength,
                                         UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11PixelShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreatePixelShader);
      Serialise_CreatePixelShader(GET_SERIALISER, pShaderBytecode, BytecodeLength, pClassLinkage,
                                  &wrapped);

      WrappedID3D11Shader<ID3D11PixelShader> *sh = (WrappedID3D11Shader<ID3D11PixelShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateHullShader(SerialiserType &ser, const void *pShaderBytecode,
                                                     SIZE_T BytecodeLength_,
                                                     ID3D11ClassLinkage *pClassLinkage,
                                                     ID3D11HullShader **ppHullShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppHullShader))
      .TypedAs("ID3D11HullShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11HullShader *ret;
    HRESULT hr = m_pDevice->CreateHullShader(pShaderBytecode, (SIZE_T)BytecodeLength,
                                             UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11HullShader>(ret, pShader, (const byte *)pShaderBytecode,
                                                      (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Hull Shader");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateHullShader(pShaderBytecode, BytecodeLength,
                                        UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11HullShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateHullShader);
      Serialise_CreateHullShader(GET_SERIALISER, pShaderBytecode, BytecodeLength, pClassLinkage,
                                 &wrapped);

      WrappedID3D11Shader<ID3D11HullShader> *sh = (WrappedID3D11Shader<ID3D11HullShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateDomainShader(SerialiserType &ser,
                                                       const void *pShaderBytecode,
                                                       SIZE_T BytecodeLength_,
                                                       ID3D11ClassLinkage *pClassLinkage,
                                                       ID3D11DomainShader **ppDomainShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppDomainShader))
      .TypedAs("ID3D11DomainShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11DomainShader *ret;
    HRESULT hr =
        m_pDevice->CreateDomainShader(pShaderBytecode, (SIZE_T)BytecodeLength,
                                      UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11DomainShader>(ret, pShader, (const byte *)pShaderBytecode,
                                                        (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Domain Shader");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice->CreateDomainShader(pShaderBytecode, BytecodeLength,
                                          UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11DomainShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateDomainShader);
      Serialise_CreateDomainShader(GET_SERIALISER, pShaderBytecode, BytecodeLength, pClassLinkage,
                                   &wrapped);

      WrappedID3D11Shader<ID3D11DomainShader> *sh =
          (WrappedID3D11Shader<ID3D11DomainShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateComputeShader(SerialiserType &ser,
                                                        const void *pShaderBytecode,
                                                        SIZE_T BytecodeLength_,
                                                        ID3D11ClassLinkage *pClassLinkage,
                                                        ID3D11ComputeShader **ppComputeShader)
{
  SERIALISE_ELEMENT_ARRAY(pShaderBytecode, BytecodeLength_);
  SERIALISE_ELEMENT_LOCAL(BytecodeLength, uint64_t(BytecodeLength_));
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pShader, GetIDForResource(*ppComputeShader))
      .TypedAs("ID3D11ComputeShader *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11ComputeShader *ret;
    HRESULT hr =
        m_pDevice->CreateComputeShader(pShaderBytecode, (SIZE_T)BytecodeLength,
                                       UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Shader<ID3D11ComputeShader>(
          ret, pShader, (const byte *)pShaderBytecode, (size_t)BytecodeLength, this);

      GetResourceManager()->AddLiveResource(pShader, ret);
    }

    AddResource(pShader, ResourceType::Shader, "Compute Shader");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateComputeShader(
                          pShaderBytecode, BytecodeLength,
                          UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage), &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Shader<ID3D11ComputeShader>(
        real, ResourceId(), (const byte *)pShaderBytecode, BytecodeLength, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateComputeShader);
      Serialise_CreateComputeShader(GET_SERIALISER, pShaderBytecode, BytecodeLength, pClassLinkage,
                                    &wrapped);

      WrappedID3D11Shader<ID3D11ComputeShader> *sh =
          (WrappedID3D11Shader<ID3D11ComputeShader> *)wrapped;
      ResourceId id = sh->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
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
template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateClassInstance(SerialiserType &ser, LPCSTR pClassTypeName,
                                                        UINT ConstantBufferOffset,
                                                        UINT ConstantVectorOffset,
                                                        UINT TextureOffset, UINT SamplerOffset,
                                                        ID3D11ClassLinkage *pClassLinkage,
                                                        ID3D11ClassInstance **ppInstance)
{
  SERIALISE_ELEMENT(pClassTypeName);
  SERIALISE_ELEMENT(ConstantBufferOffset);
  SERIALISE_ELEMENT(ConstantVectorOffset);
  SERIALISE_ELEMENT(TextureOffset);
  SERIALISE_ELEMENT(SamplerOffset);
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pInstance, GetIDForResource(*ppInstance))
      .TypedAs("ID3D11ClassInstance *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pClassLinkage)
  {
    ID3D11ClassInstance *real = NULL;
    ID3D11ClassInstance *wrapped = NULL;
    HRESULT hr = UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage)
                     ->CreateClassInstance(pClassTypeName, ConstantBufferOffset,
                                           ConstantVectorOffset, TextureOffset, SamplerOffset, &real);

    APIProps.ShaderLinkage = true;

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      wrapped = new WrappedID3D11ClassInstance(real, pClassLinkage, this);

      GetResourceManager()->AddLiveResource(pInstance, wrapped);
    }

    AddResource(pInstance, ResourceType::ShaderBinding, "Class Instance");
    DerivedResource(pClassLinkage, pInstance);
  }

  return true;
}

ID3D11ClassInstance *WrappedID3D11Device::CreateClassInstance(
    LPCSTR pClassTypeName, UINT ConstantBufferOffset, UINT ConstantVectorOffset, UINT TextureOffset,
    UINT SamplerOffset, ID3D11ClassLinkage *pClassLinkage, ID3D11ClassInstance **ppInstance)
{
  ID3D11ClassInstance *wrapped = NULL;

  if(IsCaptureMode(m_State))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ClassInstance(*ppInstance, pClassLinkage, this);

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateClassInstance);
      Serialise_CreateClassInstance(GET_SERIALISER, pClassTypeName, ConstantBufferOffset,
                                    ConstantVectorOffset, TextureOffset, SamplerOffset,
                                    pClassLinkage, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    return wrapped;
  }

  RDCERR("Creating class instances while not in capture mode!");
  return NULL;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_GetClassInstance(SerialiserType &ser, LPCSTR pClassInstanceName,
                                                     UINT InstanceIndex,
                                                     ID3D11ClassLinkage *pClassLinkage,
                                                     ID3D11ClassInstance **ppInstance)
{
  SERIALISE_ELEMENT(pClassInstanceName);
  SERIALISE_ELEMENT(InstanceIndex);
  SERIALISE_ELEMENT(pClassLinkage);
  SERIALISE_ELEMENT_LOCAL(pInstance, GetIDForResource(*ppInstance))
      .TypedAs("ID3D11ClassInstance *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pClassLinkage)
  {
    ID3D11ClassInstance *real = NULL;
    ID3D11ClassInstance *wrapped = NULL;
    HRESULT hr = UNWRAP(WrappedID3D11ClassLinkage, pClassLinkage)
                     ->GetClassInstance(pClassInstanceName, InstanceIndex, &real);

    APIProps.ShaderLinkage = true;

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      wrapped = new WrappedID3D11ClassInstance(real, pClassLinkage, this);

      GetResourceManager()->AddLiveResource(pInstance, wrapped);
    }

    AddResource(pInstance, ResourceType::ShaderBinding, "Class Instance");
    DerivedResource(pClassLinkage, pInstance);
  }

  return true;
}

ID3D11ClassInstance *WrappedID3D11Device::GetClassInstance(LPCSTR pClassInstanceName,
                                                           UINT InstanceIndex,
                                                           ID3D11ClassLinkage *pClassLinkage,
                                                           ID3D11ClassInstance **ppInstance)
{
  ID3D11ClassInstance *wrapped = NULL;

  if(IsCaptureMode(m_State))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ClassInstance(*ppInstance, pClassLinkage, this);

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::GetClassInstance);
      Serialise_GetClassInstance(GET_SERIALISER, pClassInstanceName, InstanceIndex, pClassLinkage,
                                 &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    return wrapped;
  }

  RDCERR("Creating class instances while not in capture mode!");
  return NULL;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateClassLinkage(SerialiserType &ser,
                                                       ID3D11ClassLinkage **ppLinkage)
{
  SERIALISE_ELEMENT_LOCAL(pLinkage, GetIDForResource(*ppLinkage))
      .TypedAs("ID3D11ClassLinkage *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11ClassLinkage *ret;
    HRESULT hr = m_pDevice->CreateClassLinkage(&ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11ClassLinkage(ret, this);

      GetResourceManager()->AddLiveResource(pLinkage, ret);
    }

    AddResource(pLinkage, ResourceType::ShaderBinding, "Class Linkage");
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

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11ClassLinkage(real, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateClassLinkage);
      Serialise_CreateClassLinkage(GET_SERIALISER, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppLinkage = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateBlendState(SerialiserType &ser,
                                                     const D3D11_BLEND_DESC *pBlendStateDesc,
                                                     ID3D11BlendState **ppBlendState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pBlendStateDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppBlendState))
      .TypedAs("ID3D11BlendState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11BlendState *ret;
    HRESULT hr = m_pDevice->CreateBlendState(&Descriptor, &ret);

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
        ret = (ID3D11BlendState *)GetResourceManager()->GetWrapper(ret);
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

HRESULT WrappedID3D11Device::CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                                              ID3D11BlendState **ppBlendState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppBlendState == NULL)
    return m_pDevice->CreateBlendState(pBlendStateDesc, NULL);

  ID3D11BlendState *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateBlendState(pBlendStateDesc, &real));

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

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateBlendState);
      Serialise_CreateBlendState(GET_SERIALISER, pBlendStateDesc, &wrapped);

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
bool WrappedID3D11Device::Serialise_CreateDepthStencilState(
    SerialiserType &ser, const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
    ID3D11DepthStencilState **ppDepthStencilState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDepthStencilDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppDepthStencilState))
      .TypedAs("ID3D11DepthStencilState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11DepthStencilState *ret;
    HRESULT hr = m_pDevice->CreateDepthStencilState(&Descriptor, &ret);

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
        ret = (ID3D11DepthStencilState *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(pState, ret);
      }
      else
      {
        ret = new WrappedID3D11DepthStencilState(ret, this);

        GetResourceManager()->AddLiveResource(pState, ret);
      }
    }

    AddResource(pState, ResourceType::StateObject, "Depth-Stencil State");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateDepthStencilState(pDepthStencilDesc, &real));

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

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateDepthStencilState);
      Serialise_CreateDepthStencilState(GET_SERIALISER, pDepthStencilDesc, &wrapped);

      WrappedID3D11DepthStencilState *st = (WrappedID3D11DepthStencilState *)wrapped;
      ResourceId id = st->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppDepthStencilState = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateRasterizerState(SerialiserType &ser,
                                                          const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                                                          ID3D11RasterizerState **ppRasterizerState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pRasterizerDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppRasterizerState))
      .TypedAs("ID3D11RasterizerState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11RasterizerState *ret;
    HRESULT hr = m_pDevice->CreateRasterizerState(&Descriptor, &ret);

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
        ret = (ID3D11RasterizerState *)GetResourceManager()->GetWrapper(ret);
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

HRESULT WrappedID3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                                                   ID3D11RasterizerState **ppRasterizerState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppRasterizerState == NULL)
    return m_pDevice->CreateRasterizerState(pRasterizerDesc, NULL);

  ID3D11RasterizerState *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateRasterizerState(pRasterizerDesc, &real));

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

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateRasterizerState);
      Serialise_CreateRasterizerState(GET_SERIALISER, pRasterizerDesc, &wrapped);

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
bool WrappedID3D11Device::Serialise_CreateSamplerState(SerialiserType &ser,
                                                       const D3D11_SAMPLER_DESC *pSamplerDesc,
                                                       ID3D11SamplerState **ppSamplerState)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pSamplerDesc);
  SERIALISE_ELEMENT_LOCAL(pState, GetIDForResource(*ppSamplerState))
      .TypedAs("ID3D11SamplerState *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11SamplerState *ret;
    HRESULT hr = m_pDevice->CreateSamplerState(&Descriptor, &ret);

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
        ret = (ID3D11SamplerState *)GetResourceManager()->GetWrapper(ret);
        ret->AddRef();

        GetResourceManager()->AddLiveResource(pState, ret);
      }
      else
      {
        ret = new WrappedID3D11SamplerState(ret, this);

        GetResourceManager()->AddLiveResource(pState, ret);
      }
    }

    AddResource(pState, ResourceType::Sampler, "Sampler State");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateSamplerState(pSamplerDesc, &real));

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

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateSamplerState);
      Serialise_CreateSamplerState(GET_SERIALISER, pSamplerDesc, &wrapped);

      WrappedID3D11SamplerState *st = (WrappedID3D11SamplerState *)wrapped;
      ResourceId id = st->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppSamplerState = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateQuery(SerialiserType &ser,
                                                const D3D11_QUERY_DESC *pQueryDesc,
                                                ID3D11Query **ppQuery)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pQueryDesc);
  SERIALISE_ELEMENT_LOCAL(pQuery, GetIDForResource(*ppQuery)).TypedAs("ID3D11Query *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11Query *ret;
    HRESULT hr = m_pDevice->CreateQuery(&Descriptor, &ret);

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

HRESULT WrappedID3D11Device::CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppQuery == NULL)
    return m_pDevice->CreateQuery(pQueryDesc, NULL);

  ID3D11Query *real = NULL;
  ID3D11Query *wrapped = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateQuery(pQueryDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Query1(real, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateQuery);
      Serialise_CreateQuery(GET_SERIALISER, pQueryDesc, &wrapped);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreatePredicate(SerialiserType &ser,
                                                    const D3D11_QUERY_DESC *pPredicateDesc,
                                                    ID3D11Predicate **ppPredicate)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pPredicateDesc);
  SERIALISE_ELEMENT_LOCAL(pPredicate, GetIDForResource(*ppPredicate))
      .TypedAs("ID3D11Predicate *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // We don't use predicates directly, so removing this flag doesn't make any direct difference.
    // It means we can query the result via GetData which we can't if the flag is set.
    Descriptor.MiscFlags &= ~D3D11_QUERY_MISC_PREDICATEHINT;

    ID3D11Predicate *ret;
    HRESULT hr = m_pDevice->CreatePredicate(&Descriptor, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Predicate(ret, this);

      GetResourceManager()->AddLiveResource(pPredicate, ret);
    }

    AddResource(pPredicate, ResourceType::Query, "Predicate");

    // prime the predicate with a true result, so that if we end up referencing a predicate that was
    // filled in a previous frame that we don't have the data for, we default to passing.
    m_pImmediateContext->Begin(ret);
    m_DebugManager->RenderForPredicate();
    m_pImmediateContext->End(ret);
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreatePredicate(pPredicateDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Predicate(real, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreatePredicate);
      Serialise_CreatePredicate(GET_SERIALISER, pPredicateDesc, &wrapped);

      WrappedID3D11Predicate *p = (WrappedID3D11Predicate *)wrapped;
      ResourceId id = p->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppPredicate = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateCounter(SerialiserType &ser,
                                                  const D3D11_COUNTER_DESC *pCounterDesc,
                                                  ID3D11Counter **ppCounter)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pCounterDesc);
  SERIALISE_ELEMENT_LOCAL(pCounter, GetIDForResource(*ppCounter)).TypedAs("ID3D11Counter *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // counters may not always be available on the replay, so instead we create a timestamp query as
    // a dummy. We never replay any counter fetch since it doesn't affect rendering, so the
    // difference doesn't matter.
    D3D11_QUERY_DESC dummyQueryDesc;
    dummyQueryDesc.Query = D3D11_QUERY_TIMESTAMP;
    dummyQueryDesc.MiscFlags = 0;
    ID3D11Query *ret;
    HRESULT hr = m_pDevice->CreateQuery(&dummyQueryDesc, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11Query1(ret, this);

      GetResourceManager()->AddLiveResource(pCounter, ret);
    }

    AddResource(pCounter, ResourceType::Query, "Counter");
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
  ID3D11Counter *wrapped = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateCounter(pCounterDesc, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedID3D11Counter(real, this);

    if(IsCaptureMode(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateCounter);
      Serialise_CreateCounter(GET_SERIALISER, pCounterDesc, &wrapped);

      WrappedID3D11Counter *c = (WrappedID3D11Counter *)wrapped;
      ResourceId id = c->GetResourceID();

      RDCASSERT(GetResourceManager()->GetResourceRecord(id) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->ResType = IdentifyTypeByPtr(wrapped);
      record->Length = 0;

      record->AddChunk(scope.Get());
    }

    *ppCounter = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CreateDeferredContext(SerialiserType &ser,
                                                          const UINT ContextFlags,
                                                          ID3D11DeviceContext **ppDeferredContext)
{
  SERIALISE_ELEMENT(ContextFlags);
  SERIALISE_ELEMENT_LOCAL(pDeferredContext, GetIDForDeviceChild(*ppDeferredContext))
      .TypedAs("ID3D11DeviceContext *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11DeviceContext *ret;
    HRESULT hr = m_pDevice->CreateDeferredContext(ContextFlags, &ret);

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D11DeviceContext(this, ret);

      AddDeferredContext((WrappedID3D11DeviceContext *)ret);

      GetResourceManager()->AddLiveResource(pDeferredContext, ret);
    }

    AddResource(pDeferredContext, ResourceType::CommandBuffer, "Deferred Context");
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
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->CreateDeferredContext(ContextFlags, &real));

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    WrappedID3D11DeviceContext *w = new WrappedID3D11DeviceContext(this, real);

    w->GetScratchSerialiser().SetChunkMetadataRecording(
        m_ScratchSerialiser.GetChunkMetadataRecording());

    wrapped = w;

    if(IsActiveCapturing(m_State))
      w->AttemptCapture();

    if(IsCaptureMode(m_State))
    {
      AddDeferredContext(w);

      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateDeferredContext);
      Serialise_CreateDeferredContext(GET_SERIALISER, ContextFlags, &wrapped);

      m_DeviceRecord->AddChunk(scope.Get());
    }

    *ppDeferredContext = wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_OpenSharedResource(SerialiserType &ser, HANDLE hResource,
                                                       REFIID ReturnedInterface, void **ppResource)
{
  ID3D11DeviceChild *res = ser.IsWriting() ? (ID3D11DeviceChild *)*ppResource : NULL;

  SERIALISE_ELEMENT_LOCAL(Type, IdentifyTypeByPtr(res));
  SERIALISE_ELEMENT_LOCAL(pResource, GetIDForResource(res)).TypedAs("ID3D11DeviceChild *"_lit);

  if(Type == Resource_Buffer)
  {
    D3D11_BUFFER_DESC Descriptor;
    RDCEraseEl(Descriptor);

    byte *BufferContents = NULL;

    if(ser.IsWriting())
    {
      ID3D11Buffer *buf = (ID3D11Buffer *)res;
      buf->GetDesc(&Descriptor);

      // need to allocate record storage for this buffer
      BufferContents = new byte[Descriptor.ByteWidth];
    }

    SERIALISE_ELEMENT(Descriptor);
    SERIALISE_ELEMENT_ARRAY(BufferContents, Descriptor.ByteWidth);

    if(ser.IsWriting())
    {
      SAFE_DELETE_ARRAY(BufferContents);

      uint64_t offs = ser.GetWriter()->GetOffset() - Descriptor.ByteWidth;

      RDCASSERT((offs % 64) == 0);

      RDCASSERT(GetResourceManager()->GetResourceRecord(pResource) == NULL);

      D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(pResource);
      record->ResType = Type;
      record->SetDataOffset(offs);
      record->DataInSerialiser = true;
      record->Length = Descriptor.ByteWidth;
    }

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      ID3D11Buffer *ret;

      HRESULT hr = S_OK;

      // unset flags that are unimportant/problematic in replay
      Descriptor.MiscFlags &=
          ~(D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
            D3D11_RESOURCE_MISC_GDI_COMPATIBLE | D3D11_RESOURCE_MISC_SHARED_NTHANDLE);

      D3D11_SUBRESOURCE_DATA data;
      data.pSysMem = BufferContents;
      data.SysMemPitch = Descriptor.ByteWidth;
      data.SysMemSlicePitch = Descriptor.ByteWidth;
      hr = m_pDevice->CreateBuffer(&Descriptor, &data, &ret);

      if(FAILED(hr))
      {
        RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        ret = new WrappedID3D11Buffer(ret, Descriptor.ByteWidth, this);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }

      AddResource(pResource, ResourceType::Buffer, "Shared Buffer");

      if(Descriptor.Usage != D3D11_USAGE_IMMUTABLE)
      {
        ID3D11Buffer *stage = NULL;

        UINT byteSize = Descriptor.ByteWidth;

        RDCEraseEl(Descriptor);
        Descriptor.ByteWidth = byteSize;
        Descriptor.MiscFlags = 0;
        Descriptor.StructureByteStride = 0;
        // We don't need to bind this, but IMMUTABLE requires at least one
        // BindFlags.
        Descriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        Descriptor.CPUAccessFlags = 0;
        Descriptor.Usage = D3D11_USAGE_IMMUTABLE;

        hr = m_pDevice->CreateBuffer(&Descriptor, &data, &stage);

        if(FAILED(hr) || stage == NULL)
        {
          RDCERR("Failed to create staging buffer for buffer initial contents HRESULT: %s",
                 ToStr(hr).c_str());
        }
        else
        {
          m_ResourceManager->SetInitialContents(pResource,
                                                D3D11InitialContents(Resource_Buffer, stage));
        }
      }
    }
  }
  else if(Type == Resource_Texture1D)
  {
    D3D11_TEXTURE1D_DESC Descriptor;
    RDCEraseEl(Descriptor);

    ID3D11Texture1D *tex = (ID3D11Texture1D *)res;

    if(ser.IsWriting())
      tex->GetDesc(&Descriptor);

    SERIALISE_ELEMENT(Descriptor);

    Serialise_CreateTextureData(ser, tex, pResource, NULL, Descriptor.Width, 1, 1, Descriptor.Format,
                                Descriptor.MipLevels, Descriptor.ArraySize, false);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
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
        RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        ret = new WrappedID3D11Texture1D(ret, this, dispType);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }

      AddResource(pResource, ResourceType::Texture, "Shared 1D Texture");
    }
  }
  else if(Type == Resource_Texture2D)
  {
    D3D11_TEXTURE2D_DESC Descriptor;
    RDCEraseEl(Descriptor);

    ID3D11Texture2D *tex = (ID3D11Texture2D *)res;

    if(ser.IsWriting())
      tex->GetDesc(&Descriptor);

    SERIALISE_ELEMENT(Descriptor);

    Serialise_CreateTextureData(ser, tex, pResource, NULL, Descriptor.Width, Descriptor.Height, 1,
                                Descriptor.Format, Descriptor.MipLevels, Descriptor.ArraySize, false);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
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
        RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        ret = new WrappedID3D11Texture2D1(ret, this, dispType);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }

      AddResource(pResource, ResourceType::Texture, "Shared 2D Texture");
    }
  }
  else if(Type == Resource_Texture3D)
  {
    D3D11_TEXTURE3D_DESC Descriptor;
    RDCEraseEl(Descriptor);

    ID3D11Texture3D *tex = (ID3D11Texture3D *)res;

    if(ser.IsWriting())
      tex->GetDesc(&Descriptor);

    SERIALISE_ELEMENT(Descriptor);

    Serialise_CreateTextureData(ser, tex, pResource, NULL, Descriptor.Width, Descriptor.Height,
                                Descriptor.Depth, Descriptor.Format, Descriptor.MipLevels, 1, false);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
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
        RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        ret = new WrappedID3D11Texture3D1(ret, this, dispType);

        GetResourceManager()->AddLiveResource(pResource, ret);
      }

      AddResource(pResource, ResourceType::Texture, "Shared 3D Texture");
    }
  }
  else
  {
    RDCERR("Unknown type of resource being shared");
  }

  return true;
}

HRESULT WrappedID3D11Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
                                                void **ppResource)
{
  if(IsReplayMode(m_State))
  {
    RDCERR("Don't support opening shared resources during replay.");
    return E_NOTIMPL;
  }

  if(ppResource == NULL)
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
    HRESULT hr;
    SERIALISE_TIME_CALL(hr = m_pDevice->OpenSharedResource(hResource, ReturnedInterface, &res));

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
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::OpenSharedResource);
        Serialise_OpenSharedResource(GET_SERIALISER, hResource, ReturnedInterface, ppResource);

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

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_SetExceptionMode(SerialiserType &ser, UINT RaiseFlags)
{
  SERIALISE_ELEMENT(RaiseFlags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    m_pDevice->SetExceptionMode(RaiseFlags);

  return true;
}

HRESULT WrappedID3D11Device::SetExceptionMode(UINT RaiseFlags)
{
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice->SetExceptionMode(RaiseFlags));

  if(SUCCEEDED(ret) && IsCaptureMode(m_State))
  {
    SCOPED_LOCK(m_D3DLock);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D11Chunk::SetExceptionMode);
    Serialise_SetExceptionMode(GET_SERIALISER, RaiseFlags);

    m_DeviceRecord->AddChunk(scope.Get());
  }

  return ret;
}

UINT WrappedID3D11Device::GetExceptionMode()
{
  return m_pDevice->GetExceptionMode();
}

#undef IMPLEMENT_FUNCTION_SERIALISED
#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...)                                            \
  template bool WrappedID3D11Device::CONCAT(Serialise_, func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool WrappedID3D11Device::CONCAT(Serialise_, func(WriteSerialiser &ser, __VA_ARGS__));

SERIALISED_ID3D11DEVICE_FUNCTIONS();
SERIALISED_ID3D11DEVICE_FAKE_FUNCTIONS();