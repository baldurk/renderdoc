/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"

bool WrappedID3D11Device::Prepare_InitialState(ID3D11DeviceChild *res)
{
  D3D11ResourceType type = IdentifyTypeByPtr(res);
  ResourceId Id = GetIDForResource(res);

  RDCASSERT(IsCaptureMode(m_State));

  {
    RDCDEBUG("Prepare_InitialState(%llu)", Id);

    if(type == Resource_Buffer)
      RDCDEBUG("    .. buffer");
    else if(type == Resource_UnorderedAccessView)
      RDCDEBUG("    .. UAV");
    else if(type == Resource_Texture1D || type == Resource_Texture2D || type == Resource_Texture3D)
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
    WrappedID3D11UnorderedAccessView1 *uav = (WrappedID3D11UnorderedAccessView1 *)res;

    D3D11_UNORDERED_ACCESS_VIEW_DESC udesc;
    uav->GetDesc(&udesc);

    if(udesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
       (udesc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER | D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
    {
      ID3D11Buffer *stagingCountBuf = NULL;

      D3D11_BUFFER_DESC desc;
      desc.BindFlags = 0;
      desc.ByteWidth = 16;
      desc.MiscFlags = 0;
      desc.StructureByteStride = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.Usage = D3D11_USAGE_STAGING;
      HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &stagingCountBuf);

      if(FAILED(hr) || stagingCountBuf == NULL)
      {
        RDCERR("Failed to create staging buffer for UAV initial contents HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyStructureCount(
            stagingCountBuf, 0, UNWRAP(WrappedID3D11UnorderedAccessView1, uav));

        m_ResourceManager->SetInitialContents(Id, D3D11InitialContents(type, stagingCountBuf));
      }
    }
  }
  else if(type == Resource_Buffer)
  {
    WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)res;
    D3D11ResourceRecord *record = m_ResourceManager->GetResourceRecord(Id);

    RDCASSERT(record);

    ID3D11Buffer *stagingBuf = NULL;

    D3D11_BUFFER_DESC desc;
    desc.BindFlags = 0;
    desc.ByteWidth = (UINT)record->Length;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &stagingBuf);

    if(FAILED(hr) || stagingBuf == NULL)
    {
      RDCERR("Failed to create staging buffer for buffer initial contents HRESULT: %s",
             ToStr(hr).c_str());
    }
    else
    {
      m_pImmediateContext->GetReal()->CopyResource(stagingBuf, UNWRAP(WrappedID3D11Buffer, buf));

      m_ResourceManager->SetInitialContents(Id, D3D11InitialContents(type, stagingBuf));
    }
  }
  else if(type == Resource_Texture1D)
  {
    WrappedID3D11Texture1D *tex1D = (WrappedID3D11Texture1D *)res;

    D3D11_TEXTURE1D_DESC desc;
    tex1D->GetDesc(&desc);

    D3D11_TEXTURE1D_DESC stageDesc = desc;
    ID3D11Texture1D *stagingTex = NULL;

    stageDesc.MiscFlags = 0;
    stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stageDesc.BindFlags = 0;
    stageDesc.Usage = D3D11_USAGE_STAGING;

    HRESULT hr = m_pDevice->CreateTexture1D(&stageDesc, NULL, &stagingTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create initial tex1D HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      m_pImmediateContext->GetReal()->CopyResource(stagingTex, UNWRAP(WrappedID3D11Texture1D, tex1D));

      m_ResourceManager->SetInitialContents(Id, D3D11InitialContents(type, stagingTex));
    }
  }
  else if(type == Resource_Texture2D)
  {
    WrappedID3D11Texture2D1 *tex2D = (WrappedID3D11Texture2D1 *)res;

    D3D11_TEXTURE2D_DESC desc;
    tex2D->GetDesc(&desc);

    bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

    D3D11_TEXTURE2D_DESC stageDesc = desc;
    ID3D11Texture2D *stagingTex = NULL;

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

    hr = m_pDevice->CreateTexture2D(&stageDesc, NULL, &stagingTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create initial tex2D HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      IDXGIKeyedMutex *mutex = NULL;

      if(desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
      {
        hr = UNWRAP(WrappedID3D11Texture2D1, tex2D)
                 ->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&mutex);

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
        m_DebugManager->CopyTex2DMSToArray(stagingTex, UNWRAP(WrappedID3D11Texture2D1, tex2D));
      else
        m_pImmediateContext->GetReal()->CopyResource(stagingTex,
                                                     UNWRAP(WrappedID3D11Texture2D1, tex2D));

      m_pImmediateContext->GetReal()->Flush();

      if(mutex)
      {
        mutex->ReleaseSync(0);

        SAFE_RELEASE(mutex);
      }

      m_ResourceManager->SetInitialContents(Id, D3D11InitialContents(type, stagingTex));
    }
  }
  else if(type == Resource_Texture3D)
  {
    WrappedID3D11Texture3D1 *tex3D = (WrappedID3D11Texture3D1 *)res;

    D3D11_TEXTURE3D_DESC desc;
    tex3D->GetDesc(&desc);

    D3D11_TEXTURE3D_DESC stageDesc = desc;
    ID3D11Texture3D *stagingTex = NULL;

    stageDesc.MiscFlags = 0;
    stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stageDesc.BindFlags = 0;
    stageDesc.Usage = D3D11_USAGE_STAGING;

    HRESULT hr = m_pDevice->CreateTexture3D(&stageDesc, NULL, &stagingTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create initial tex3D HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      m_pImmediateContext->GetReal()->CopyResource(stagingTex,
                                                   UNWRAP(WrappedID3D11Texture3D1, tex3D));

      m_ResourceManager->SetInitialContents(Id, D3D11InitialContents(type, stagingTex));
    }
  }

  return true;
}

uint64_t WrappedID3D11Device::GetSize_InitialState(ResourceId id, const D3D11InitialContents &initial)
{
  // This function provides an upper bound on how much data Serialise_InitialState will write, so
  // that the chunk can be pre-allocated and not require seeking to fix-up the length.
  // It can be an over-estimate as long as it's not *too* far over.

  uint64_t ret = 128;    // type, Id, plus breathing room

  ResourcePitch pitch = {};

  if(initial.resourceType == Resource_UnorderedAccessView)
  {
    // no data stored, just a counter.
    ret += 8;
  }
  else if(initial.resourceType == Resource_Buffer)
  {
    WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)initial.resource;

    D3D11_BUFFER_DESC desc = {};
    buf->GetDesc(&desc);

    // buffer width plus alignment
    ret += desc.ByteWidth;
    ret += WriteSerialiser::GetChunkAlignment();
  }
  else if(initial.resourceType == Resource_Texture1D)
  {
    WrappedID3D11Texture1D *tex = (WrappedID3D11Texture1D *)initial.resource;

    D3D11_TEXTURE1D_DESC desc = {};
    tex->GetDesc(&desc);

    uint32_t NumSubresources = desc.MipLevels * desc.ArraySize;

    ret += 4;    // number of subresources

    // Subresource contents:
    for(UINT sub = 0; sub < NumSubresources; sub++)
    {
      UINT mip = GetMipForSubresource(tex, sub);

      const UINT RowPitch = GetRowPitch(desc.Width, desc.Format, mip);

      ret += RowPitch;
      ret += WriteSerialiser::GetChunkAlignment();
    }
  }
  else if(initial.resourceType == Resource_Texture2D)
  {
    WrappedID3D11Texture2D1 *tex = (WrappedID3D11Texture2D1 *)initial.resource;

    D3D11_TEXTURE2D_DESC desc = {};
    tex->GetDesc(&desc);

    uint32_t NumSubresources = desc.MipLevels * desc.ArraySize;

    bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

    if(multisampled)
      NumSubresources *= desc.SampleDesc.Count;

    {
      ret += 4;                      // number of subresources
      ret += 4 * NumSubresources;    // RowPitch for each subresource

      // Subresource contents:
      for(UINT sub = 0; sub < NumSubresources; sub++)
      {
        UINT mip = GetMipForSubresource(tex, sub);

        uint32_t numRows = RDCMAX(1U, desc.Height >> mip);
        if(IsBlockFormat(desc.Format))
          numRows = AlignUp4(numRows) / 4;
        else if(IsYUVPlanarFormat(desc.Format))
          numRows = GetYUVNumRows(desc.Format, numRows);

        pitch = GetResourcePitchForSubresource(m_pImmediateContext->GetReal(), tex, sub);
        ret += pitch.m_RowPitch * numRows;

        ret += WriteSerialiser::GetChunkAlignment();
      }
    }
  }
  else if(initial.resourceType == Resource_Texture3D)
  {
    WrappedID3D11Texture3D1 *tex = (WrappedID3D11Texture3D1 *)initial.resource;

    D3D11_TEXTURE3D_DESC desc = {};
    tex->GetDesc(&desc);

    uint32_t NumSubresources = desc.MipLevels;

    ret += 4;                      // number of subresources
    ret += 8 * NumSubresources;    // RowPitch and DepthPitch for each subresource

    // Subresource contents:
    for(UINT sub = 0; sub < NumSubresources; sub++)
    {
      UINT mip = GetMipForSubresource(tex, sub);

      pitch = GetResourcePitchForSubresource(m_pImmediateContext->GetReal(), tex, sub);
      ret += pitch.m_DepthPitch * RDCMAX(1U, desc.Depth >> mip);

      ret += WriteSerialiser::GetChunkAlignment();
    }
  }
  else
  {
    RDCERR("Trying to serialise initial state of unsupported resource type %s",
           ToStr(initial.resourceType).c_str());
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_InitialState(SerialiserType &ser, ResourceId id,
                                                 D3D11ResourceRecord *record,
                                                 const D3D11InitialContents *initial)
{
  D3D11ResourceType type = Resource_Unknown;

  if(IsCaptureMode(m_State))
    type = record->ResType;

  bool ret = true;

  if(type != Resource_Buffer)
  {
    SERIALISE_ELEMENT(type);
    SERIALISE_ELEMENT(id).TypedAs("ID3D11DeviceChild *"_lit);
  }

  if(IsReplayingAndReading())
  {
    AddResourceCurChunk(id);
  }

  {
    RDCDEBUG("Serialise_InitialState(%llu)", id);

    if(type == Resource_Buffer)
      RDCDEBUG("    .. buffer");
    else if(type == Resource_UnorderedAccessView)
      RDCDEBUG("    .. UAV");
    else if(type == Resource_Texture1D || type == Resource_Texture2D || type == Resource_Texture3D)
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
    uint32_t InitialHiddenCount = 0;

    if(ser.IsWriting())
    {
      ID3D11Buffer *stage = initial ? (ID3D11Buffer *)initial->resource : NULL;

      if(stage)
      {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failed to map while getting initial states HRESULT: %s", ToStr(hr).c_str());
        }
        else
        {
          InitialHiddenCount = *((uint32_t *)mapped.pData);

          m_pImmediateContext->GetReal()->Unmap(stage, 0);
        }
      }
    }

    SERIALISE_ELEMENT(InitialHiddenCount);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, InitialHiddenCount));
    }
  }
  else if(type == Resource_Buffer)
  {
    if(ser.IsWriting())
    {
      RDCASSERT(record);

      D3D11_BUFFER_DESC desc;
      desc.BindFlags = 0;
      desc.ByteWidth = (UINT)record->Length;
      desc.MiscFlags = 0;
      desc.StructureByteStride = 0;

      ID3D11Buffer *stage = initial ? (ID3D11Buffer *)initial->resource : NULL;

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      HRESULT hr = E_INVALIDARG;

      if(stage)
        hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);
      else
        RDCERR(
            "Didn't have stage resource for %llu when serialising initial state! "
            "Dirty tracking is incorrect",
            id);

      if(FAILED(hr))
      {
        RDCERR("Failed to map while getting initial states HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        RDCASSERT(record->DataInSerialiser);

        MapIntercept intercept;
        intercept.SetD3D(mapped);
        intercept.Init(stage, record->GetDataPtr());
        intercept.CopyFromD3D();

        m_pImmediateContext->GetReal()->Unmap(stage, 0);
      }
    }
  }
  else if(type == Resource_Texture1D)
  {
    ID3D11Texture1D *prepared = initial ? (ID3D11Texture1D *)initial->resource : NULL;

    ID3D11Texture1D *tex = NULL;
    D3D11_TEXTURE1D_DESC desc = {0};

    if(ser.IsWriting())
    {
      tex = prepared;
      tex->GetDesc(&desc);
    }
    else if(IsReplayingAndReading() && m_ResourceManager->HasLiveResource(id))
    {
      tex = (WrappedID3D11Texture1D *)m_ResourceManager->GetLiveResource(id);
      tex->GetDesc(&desc);
    }

    uint32_t NumSubresources = desc.MipLevels * desc.ArraySize;
    SERIALISE_ELEMENT(NumSubresources);

    D3D11_SUBRESOURCE_DATA *subData = NULL;

    if(IsReplayingAndReading() && tex)
      subData = new D3D11_SUBRESOURCE_DATA[NumSubresources];

    for(UINT sub = 0; sub < NumSubresources; sub++)
    {
      UINT mip = tex ? GetMipForSubresource(tex, sub) : 0;
      HRESULT hr = E_INVALIDARG;

      void *SubresourceContents = NULL;
      uint32_t ContentsLength = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

      if(ser.IsWriting())
      {
        D3D11_MAPPED_SUBRESOURCE mapped = {};

        if(prepared)
          hr = m_pImmediateContext->GetReal()->Map(prepared, sub, D3D11_MAP_READ, 0, &mapped);
        else
          RDCERR(
              "Didn't have stage resource for %llu when serialising initial state! "
              "Dirty tracking is incorrect",
              id);

        if(FAILED(hr))
          RDCERR("Failed to map in initial states %s", ToStr(hr).c_str());
        else
          SubresourceContents = mapped.pData;
      }

      SERIALISE_ELEMENT_ARRAY(SubresourceContents, ContentsLength);
      SERIALISE_ELEMENT(ContentsLength);

      if(ser.IsWriting() && SUCCEEDED(hr))
        m_pImmediateContext->GetReal()->Unmap(prepared, sub);

      if(IsReplayingAndReading() && tex)
      {
        // we 'steal' the SubresourceContents buffer so it doesn't get de-serialised and free'd
        subData[sub].pSysMem = SubresourceContents;
        SubresourceContents = NULL;

        subData[sub].SysMemPitch = ContentsLength;
        subData[sub].SysMemSlicePitch = ContentsLength;
      }
    }

    // need to manually do cleanup here if we're about to bail on errors
    if(IsReplayingAndReading() && tex && ser.IsErrored())
    {
      // free the buffers we stole
      for(UINT sub = 0; sub < NumSubresources; sub++)
        FreeAlignedBuffer((byte *)subData[sub].pSysMem);

      SAFE_DELETE_ARRAY(subData);
    }

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading() && tex)
    {
      // We don't need to bind this, but IMMUTABLE requires at least one
      // BindFlags.
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_IMMUTABLE;
      desc.MiscFlags = 0;

      ID3D11Texture1D *dataTex = NULL;
      HRESULT hr = m_pDevice->CreateTexture1D(&desc, subData, &dataTex);

      if(FAILED(hr) || dataTex == NULL)
      {
        RDCERR("Failed to create staging resource for Texture1D initial contents HRESULT: %s",
               ToStr(hr).c_str());
        ret = false;
      }
      else
      {
        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, dataTex));
      }

      // free the buffers we stole
      for(UINT sub = 0; sub < NumSubresources; sub++)
        FreeAlignedBuffer((byte *)subData[sub].pSysMem);
    }

    SAFE_DELETE_ARRAY(subData);
  }
  else if(type == Resource_Texture2D)
  {
    ID3D11Texture2D *prepared = initial ? (ID3D11Texture2D *)initial->resource : NULL;

    ID3D11Texture2D *tex = NULL;
    D3D11_TEXTURE2D_DESC desc = {0};

    if(ser.IsWriting())
    {
      tex = prepared;
      tex->GetDesc(&desc);
    }
    else if(IsReplayingAndReading() && m_ResourceManager->HasLiveResource(id))
    {
      tex = (WrappedID3D11Texture2D1 *)m_ResourceManager->GetLiveResource(id);
      tex->GetDesc(&desc);
    }

    uint32_t NumSubresources = desc.MipLevels * desc.ArraySize;
    bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

    // in version 0xF and before, we mistakenly multiplied on the sample count to the number of
    // subresources after serialisation - meaning the loop below breaks for pure structured data
    // serialisation.
    // After version 0x10 we pre-multiply before serialising, because the result is the same either
    // way and we don't need the un-multiplied value.
    if(ser.VersionAtLeast(0x10))
    {
      if(multisampled)
        NumSubresources *= desc.SampleDesc.Count;

      SERIALISE_ELEMENT(NumSubresources);
    }
    else
    {
      SERIALISE_ELEMENT(NumSubresources);

      if(multisampled)
        NumSubresources *= desc.SampleDesc.Count;
    }

    // this value is serialised here for compatibility with pre-v1.1 captures. In prior versions the
    // 'save all initials' option, if disabled, meant a heuristic was used to determine if this
    // initial state should be saved or skipped. We now always save all initial states.
    SERIALISE_ELEMENT_LOCAL(OmittedContents, false);

    if(OmittedContents)
    {
      // need to handle this case for legacy captures with omitted resources. Just skip
    }
    else
    {
      D3D11_SUBRESOURCE_DATA *subData = NULL;

      if(IsReplayingAndReading() && tex)
        subData = new D3D11_SUBRESOURCE_DATA[NumSubresources];

      for(UINT sub = 0; sub < NumSubresources; sub++)
      {
        UINT mip = tex ? GetMipForSubresource(tex, sub) : 0;
        HRESULT hr = E_INVALIDARG;

        uint32_t numRows = RDCMAX(1U, desc.Height >> mip);
        if(IsBlockFormat(desc.Format))
          numRows = AlignUp4(numRows) / 4;
        else if(IsYUVPlanarFormat(desc.Format))
          numRows = GetYUVNumRows(desc.Format, numRows);

        void *SubresourceContents = NULL;
        uint32_t ContentsLength = 0;
        uint32_t RowPitch = 0;

        if(ser.IsWriting())
        {
          D3D11_MAPPED_SUBRESOURCE mapped = {};

          if(prepared)
            hr = m_pImmediateContext->GetReal()->Map(prepared, sub, D3D11_MAP_READ, 0, &mapped);
          else
            RDCERR(
                "Didn't have stage resource for %llu when serialising initial state! "
                "Dirty tracking is incorrect",
                id);

          if(FAILED(hr))
          {
            RDCERR("Failed to map in initial states HRESULT: %s", ToStr(hr).c_str());
          }
          else
          {
            SubresourceContents = mapped.pData;
            RowPitch = mapped.RowPitch;
            ContentsLength = RowPitch * numRows;
          }
        }

        SERIALISE_ELEMENT(RowPitch);
        SERIALISE_ELEMENT_ARRAY(SubresourceContents, ContentsLength);

        if(ser.IsWriting() && SUCCEEDED(hr))
          m_pImmediateContext->GetReal()->Unmap(prepared, sub);

        if(IsReplayingAndReading() && tex)
        {
          // we 'steal' the SubresourceContents buffer so it doesn't get de-serialised and free'd
          subData[sub].pSysMem = SubresourceContents;
          SubresourceContents = NULL;

          // use the RowPitch provided when we mapped in the first place, since we read the whole
          // buffer including padding
          subData[sub].SysMemPitch = RowPitch;
          subData[sub].SysMemSlicePitch = RowPitch * numRows;
        }
      }

      // need to manually do cleanup here if we're about to bail on errors
      if(IsReplayingAndReading() && tex && ser.IsErrored())
      {
        // free the buffers we stole
        for(UINT sub = 0; sub < NumSubresources; sub++)
          FreeAlignedBuffer((byte *)subData[sub].pSysMem);

        SAFE_DELETE_ARRAY(subData);
      }

      SERIALISE_CHECK_READ_ERRORS();

      if(IsReplayingAndReading() && tex)
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
          case DXGI_FORMAT_D32_FLOAT: desc.Format = DXGI_FORMAT_R32_FLOAT; break;
          case DXGI_FORMAT_D24_UNORM_S8_UINT:
            desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
          case DXGI_FORMAT_D16_UNORM: desc.Format = DXGI_FORMAT_R16_FLOAT; break;
          default: break;
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

        ID3D11Texture2D *dataTex = NULL;
        hr = m_pDevice->CreateTexture2D(&initialDesc, subData, &dataTex);

        if(FAILED(hr) || dataTex == NULL)
        {
          RDCERR("Failed to create staging resource for Texture2D initial contents HRESULT: %s",
                 ToStr(hr).c_str());
          ret = false;
        }
        else
        {
          // if multisampled, contents is actually an array with slices for each sample.
          // need to copy back out to a real multisampled resource
          if(multisampled)
          {
            desc.BindFlags =
                IsDepthFormat(desc.Format) ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;

            if(IsDepthFormat(desc.Format))
              desc.Format = GetDepthTypedFormat(desc.Format);

            ID3D11Texture2D *contentsMS = NULL;
            hr = m_pDevice->CreateTexture2D(&desc, NULL, &contentsMS);

            m_DebugManager->CopyArrayToTex2DMS(contentsMS, dataTex, ~0U);

            SAFE_RELEASE(dataTex);
            dataTex = contentsMS;
          }

          m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, dataTex));
        }

        // free the buffers we stole
        for(UINT sub = 0; sub < NumSubresources; sub++)
          FreeAlignedBuffer((byte *)subData[sub].pSysMem);
      }

      SAFE_DELETE_ARRAY(subData);
    }
  }
  else if(type == Resource_Texture3D)
  {
    ID3D11Texture3D *prepared = initial ? (ID3D11Texture3D *)initial->resource : NULL;

    ID3D11Texture3D *tex = NULL;
    D3D11_TEXTURE3D_DESC desc = {0};

    if(ser.IsWriting())
    {
      tex = prepared;
      tex->GetDesc(&desc);
    }
    else if(IsReplayingAndReading() && m_ResourceManager->HasLiveResource(id))
    {
      tex = (WrappedID3D11Texture3D1 *)m_ResourceManager->GetLiveResource(id);
      tex->GetDesc(&desc);
    }

    uint32_t NumSubresources = desc.MipLevels;
    SERIALISE_ELEMENT(NumSubresources);

    D3D11_SUBRESOURCE_DATA *subData = NULL;

    if(IsReplayingAndReading() && tex)
      subData = new D3D11_SUBRESOURCE_DATA[NumSubresources];

    for(UINT sub = 0; sub < NumSubresources; sub++)
    {
      UINT mip = tex ? GetMipForSubresource(tex, sub) : 0;
      HRESULT hr = E_INVALIDARG;

      uint32_t numRows = RDCMAX(1U, desc.Height >> mip);
      if(IsBlockFormat(desc.Format))
        numRows = AlignUp4(numRows) / 4;
      else if(IsYUVPlanarFormat(desc.Format))
        numRows = GetYUVNumRows(desc.Format, numRows);

      void *SubresourceContents = NULL;
      uint32_t ContentsLength = 0;
      uint32_t RowPitch = 0;
      uint32_t DepthPitch = 0;

      if(ser.IsWriting())
      {
        D3D11_MAPPED_SUBRESOURCE mapped = {};

        if(prepared)
          hr = m_pImmediateContext->GetReal()->Map(prepared, sub, D3D11_MAP_READ, 0, &mapped);
        else
          RDCERR(
              "Didn't have stage resource for %llu when serialising initial state! "
              "Dirty tracking is incorrect",
              id);

        if(FAILED(hr))
        {
          RDCERR("Failed to map in initial states HRESULT: %s", ToStr(hr).c_str());
        }
        else
        {
          SubresourceContents = mapped.pData;
          RowPitch = mapped.RowPitch;
          DepthPitch = mapped.DepthPitch;
          RDCASSERT(DepthPitch >= RowPitch * numRows);
          ContentsLength = DepthPitch * RDCMAX(1U, desc.Depth >> mip);
        }
      }

      SERIALISE_ELEMENT(RowPitch);
      SERIALISE_ELEMENT(DepthPitch);
      SERIALISE_ELEMENT_ARRAY(SubresourceContents, ContentsLength);
      SERIALISE_ELEMENT(ContentsLength);

      if(ser.IsWriting() && SUCCEEDED(hr))
        m_pImmediateContext->GetReal()->Unmap(prepared, sub);

      if(IsReplayingAndReading() && tex)
      {
        // we 'steal' the SubresourceContents buffer so it doesn't get de-serialised and free'd
        subData[sub].pSysMem = SubresourceContents;
        SubresourceContents = NULL;

        // use the Row/DepthPitch provided when we mapped in the first place, since we read the
        // whole buffer including padding
        subData[sub].SysMemPitch = RowPitch;
        subData[sub].SysMemSlicePitch = DepthPitch;
      }
    }

    // need to manually do cleanup here if we're about to bail on errors
    if(IsReplayingAndReading() && tex && ser.IsErrored())
    {
      // free the buffers we stole
      for(UINT sub = 0; sub < NumSubresources; sub++)
        FreeAlignedBuffer((byte *)subData[sub].pSysMem);

      SAFE_DELETE_ARRAY(subData);
    }

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading() && tex)
    {
      // We don't need to bind this, but IMMUTABLE requires at least one
      // BindFlags.
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_IMMUTABLE;
      desc.MiscFlags = 0;

      ID3D11Texture3D *dataTex = NULL;
      HRESULT hr = m_pDevice->CreateTexture3D(&desc, subData, &dataTex);

      if(FAILED(hr) || dataTex == NULL)
      {
        RDCERR("Failed to create staging resource for Texture3D initial contents HRESULT: %s",
               ToStr(hr).c_str());
        ret = false;
      }
      else
      {
        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, dataTex));
      }

      // free the buffers we stole
      for(UINT sub = 0; sub < NumSubresources; sub++)
        FreeAlignedBuffer((byte *)subData[sub].pSysMem);
    }

    SAFE_DELETE_ARRAY(subData);
  }
  else
  {
    RDCERR("Trying to serialise initial state of unsupported resource type");
  }

  return ret;
}

void WrappedID3D11Device::Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData)
{
  if(IsStructuredExporting(m_State))
    return;

  D3D11ResourceType type = IdentifyTypeByPtr(live);

  {
    RDCDEBUG("Create_InitialState(%llu)", id);

    if(type == Resource_Buffer)
      RDCDEBUG("    .. buffer");
    else if(type == Resource_UnorderedAccessView)
      RDCDEBUG("    .. UAV");
    else if(type == Resource_Texture1D || type == Resource_Texture2D || type == Resource_Texture3D)
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
    WrappedID3D11UnorderedAccessView1 *uav = (WrappedID3D11UnorderedAccessView1 *)live;

    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    uav->GetDesc(&desc);

    if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
       (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER | D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
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
        RDCERR("Failed to create staging resource for UAV initial contents HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyStructureCount(
            stage, 0, UNWRAP(WrappedID3D11UnorderedAccessView1, uav));

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);

        uint32_t countData = 0;

        if(FAILED(hr))
        {
          RDCERR("Failed to map while creating initial states HRESULT: %s", ToStr(hr).c_str());
        }
        else
        {
          countData = *((uint32_t *)mapped.pData);

          m_pImmediateContext->GetReal()->Unmap(stage, 0);
        }

        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, countData));

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

      ID3D11RenderTargetView *clearRTV = NULL;

      HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture1D, tex1D), &rdesc,
                                                     &clearRTV);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear RTV while creating initial states HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, clearRTV));
      }
    }
    else if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
    {
      D3D11_DEPTH_STENCIL_VIEW_DESC ddesc;
      ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
      ddesc.Format = GetDepthTypedFormat(desc.Format);
      ddesc.Texture1D.MipSlice = 0;
      ddesc.Flags = 0;

      ID3D11DepthStencilView *clearDSV = NULL;

      HRESULT hr = m_pDevice->CreateDepthStencilView(UNWRAP(WrappedID3D11Texture1D, tex1D), &ddesc,
                                                     &clearDSV);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear DSV while creating initial states HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, clearDSV));
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

      ID3D11Texture1D *dataTex = NULL;

      HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &dataTex);

      if(FAILED(hr))
      {
        RDCERR("Failed to create tex3D while creating initial states HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyResource(dataTex, UNWRAP(WrappedID3D11Texture1D, tex1D));

        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, dataTex));
      }
    }
  }
  else if(type == Resource_Texture2D)
  {
    WrappedID3D11Texture2D1 *tex2D = (WrappedID3D11Texture2D1 *)live;

    D3D11_TEXTURE2D_DESC desc;
    tex2D->GetDesc(&desc);

    bool isMS = (desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0);

    if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_RENDER_TARGET))
    {
      D3D11_RENDER_TARGET_VIEW_DESC rdesc;
      rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      rdesc.Format = GetTypedFormat(desc.Format);
      rdesc.Texture2D.MipSlice = 0;

      if(isMS)
        rdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

      ID3D11RenderTargetView *clearRTV = NULL, *clear2RTV = NULL;

      if(IsYUVFormat(desc.Format))
      {
        rdesc.Format = GetYUVViewPlane0Format(desc.Format);

        HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D1, tex2D),
                                                       &rdesc, &clearRTV);

        if(SUCCEEDED(hr))
        {
          rdesc.Format = GetYUVViewPlane1Format(desc.Format);

          if(rdesc.Format != DXGI_FORMAT_UNKNOWN)
            hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D1, tex2D), &rdesc,
                                                   &clear2RTV);
        }

        if(FAILED(hr))
        {
          RDCERR(
              "Failed to create fast-clear RTVs while creating initial states for YUV texture %s "
              "HRESULT: %s",
              ToStr(desc.Format).c_str(), ToStr(hr).c_str());
        }
        else
        {
          m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, clearRTV, clear2RTV));
        }
      }
      else
      {
        HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D1, tex2D),
                                                       &rdesc, &clearRTV);

        if(FAILED(hr))
        {
          RDCERR("Failed to create fast-clear RTV while creating initial states HRESULT: %s",
                 ToStr(hr).c_str());
        }
        else
        {
          m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, clearRTV));
        }
      }
    }
    else if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
    {
      D3D11_DEPTH_STENCIL_VIEW_DESC ddesc;
      ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
      ddesc.Format = GetDepthTypedFormat(desc.Format);
      ddesc.Texture1D.MipSlice = 0;
      ddesc.Flags = 0;

      if(isMS)
        ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

      ID3D11DepthStencilView *clearDSV = NULL;

      HRESULT hr = m_pDevice->CreateDepthStencilView(UNWRAP(WrappedID3D11Texture2D1, tex2D), &ddesc,
                                                     &clearDSV);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear DSV while creating initial states HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, clearDSV));
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

      ID3D11Texture2D *dataTex = NULL;

      HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &dataTex);

      if(FAILED(hr))
      {
        RDCERR("Failed to create tex2D while creating initial states HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyResource(dataTex, UNWRAP(WrappedID3D11Texture2D1, tex2D));

        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, dataTex));
      }
    }
  }
  else if(type == Resource_Texture3D)
  {
    WrappedID3D11Texture3D1 *tex3D = (WrappedID3D11Texture3D1 *)live;

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

      ID3D11RenderTargetView *clearRTV = NULL;

      HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture3D1, tex3D), &rdesc,
                                                     &clearRTV);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear RTV while creating initial states HRESULT: %s",
               ToStr(hr).c_str());
      }
      else
      {
        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, clearRTV));
      }
    }
    else if(!hasData && desc.Usage != D3D11_USAGE_IMMUTABLE)
    {
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = 0;
      desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

      ID3D11Texture3D *dataTex = NULL;

      HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &dataTex);

      if(FAILED(hr))
      {
        RDCERR("Failed to create tex3D while creating initial states HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyResource(dataTex, UNWRAP(WrappedID3D11Texture3D1, tex3D));

        m_ResourceManager->SetInitialContents(id, D3D11InitialContents(type, dataTex));
      }
    }
  }
}

void WrappedID3D11Device::Apply_InitialState(ID3D11DeviceChild *live,
                                             const D3D11InitialContents &initial)
{
  if(initial.resourceType == Resource_UnorderedAccessView)
  {
    ID3D11UnorderedAccessView *uav = (ID3D11UnorderedAccessView *)live;

    m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &uav, &initial.uavCount);
  }
  else
  {
    if(initial.tag == D3D11InitialContents::ClearRTV)
    {
      float emptyCol[] = {0.0f, 0.0f, 0.0f, 0.0f};
      m_pImmediateContext->GetReal()->ClearRenderTargetView(
          (ID3D11RenderTargetView *)initial.resource, emptyCol);

      if(initial.resource2)
        m_pImmediateContext->GetReal()->ClearRenderTargetView(
            (ID3D11RenderTargetView *)initial.resource2, emptyCol);
    }
    else if(initial.tag == D3D11InitialContents::ClearDSV)
    {
      m_pImmediateContext->GetReal()->ClearDepthStencilView(
          (ID3D11DepthStencilView *)initial.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,
          0);
    }
    else if(initial.tag == D3D11InitialContents::Copy)
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

template bool WrappedID3D11Device::Serialise_InitialState(ReadSerialiser &ser, ResourceId id,
                                                          D3D11ResourceRecord *record,
                                                          const D3D11InitialContents *initial);
template bool WrappedID3D11Device::Serialise_InitialState(WriteSerialiser &ser, ResourceId id,
                                                          D3D11ResourceRecord *record,
                                                          const D3D11InitialContents *initial);