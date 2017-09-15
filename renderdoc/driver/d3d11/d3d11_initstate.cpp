/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include "d3d11_device.h"
#include "d3d11_resources.h"

bool WrappedID3D11Device::Prepare_InitialState(ID3D11DeviceChild *res)
{
  ResourceType type = IdentifyTypeByPtr(res);
  ResourceId Id = GetIDForResource(res);

  RDCASSERT(m_State >= WRITING);

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
      ID3D11Buffer *stage = NULL;

      D3D11_BUFFER_DESC desc;
      desc.BindFlags = 0;
      desc.ByteWidth = 16;
      desc.MiscFlags = 0;
      desc.StructureByteStride = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.Usage = D3D11_USAGE_STAGING;
      HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &stage);

      if(FAILED(hr) || stage == NULL)
      {
        RDCERR("Failed to create staging buffer for UAV initial contents %08x", hr);
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyStructureCount(
            stage, 0, UNWRAP(WrappedID3D11UnorderedAccessView1, uav));

        m_ResourceManager->SetInitialContents(
            Id, D3D11ResourceManager::InitialContentData(type, stage, 0, NULL));
      }
    }
  }
  else if(type == Resource_Buffer)
  {
    WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)res;
    D3D11ResourceRecord *record = m_ResourceManager->GetResourceRecord(Id);

    ID3D11Buffer *stage = NULL;

    D3D11_BUFFER_DESC desc;
    desc.BindFlags = 0;
    desc.ByteWidth = (UINT)record->Length;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &stage);

    if(FAILED(hr) || stage == NULL)
    {
      RDCERR("Failed to create staging buffer for buffer initial contents %08x", hr);
    }
    else
    {
      m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Buffer, buf));

      m_ResourceManager->SetInitialContents(
          Id, D3D11ResourceManager::InitialContentData(type, stage, 0, NULL));
    }
  }
  else if(type == Resource_Texture1D)
  {
    WrappedID3D11Texture1D *tex1D = (WrappedID3D11Texture1D *)res;

    D3D11_TEXTURE1D_DESC desc;
    tex1D->GetDesc(&desc);

    D3D11_TEXTURE1D_DESC stageDesc = desc;
    ID3D11Texture1D *stage = NULL;

    stageDesc.MiscFlags = 0;
    stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stageDesc.BindFlags = 0;
    stageDesc.Usage = D3D11_USAGE_STAGING;

    HRESULT hr = m_pDevice->CreateTexture1D(&stageDesc, NULL, &stage);

    if(FAILED(hr))
    {
      RDCERR("Failed to create initial tex1D %08x", hr);
    }
    else
    {
      m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Texture1D, tex1D));

      m_ResourceManager->SetInitialContents(
          Id, D3D11ResourceManager::InitialContentData(type, stage, 0, NULL));
    }
  }
  else if(type == Resource_Texture2D)
  {
    WrappedID3D11Texture2D1 *tex2D = (WrappedID3D11Texture2D1 *)res;

    D3D11_TEXTURE2D_DESC desc;
    tex2D->GetDesc(&desc);

    bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

    D3D11_TEXTURE2D_DESC stageDesc = desc;
    ID3D11Texture2D *stage = NULL;

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

    hr = m_pDevice->CreateTexture2D(&stageDesc, NULL, &stage);

    if(FAILED(hr))
    {
      RDCERR("Failed to create initial tex2D %08x", hr);
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
        m_DebugManager->CopyTex2DMSToArray(stage, UNWRAP(WrappedID3D11Texture2D1, tex2D));
      else
        m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Texture2D1, tex2D));

      m_pImmediateContext->GetReal()->Flush();

      if(mutex)
      {
        mutex->ReleaseSync(0);

        SAFE_RELEASE(mutex);
      }

      m_ResourceManager->SetInitialContents(
          Id, D3D11ResourceManager::InitialContentData(type, stage, 0, NULL));
    }
  }
  else if(type == Resource_Texture3D)
  {
    WrappedID3D11Texture3D1 *tex3D = (WrappedID3D11Texture3D1 *)res;

    D3D11_TEXTURE3D_DESC desc;
    tex3D->GetDesc(&desc);

    D3D11_TEXTURE3D_DESC stageDesc = desc;
    ID3D11Texture3D *stage = NULL;

    stageDesc.MiscFlags = 0;
    stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stageDesc.BindFlags = 0;
    stageDesc.Usage = D3D11_USAGE_STAGING;

    HRESULT hr = m_pDevice->CreateTexture3D(&stageDesc, NULL, &stage);

    if(FAILED(hr))
    {
      RDCERR("Failed to create initial tex3D %08x", hr);
    }
    else
    {
      m_pImmediateContext->GetReal()->CopyResource(stage, UNWRAP(WrappedID3D11Texture3D1, tex3D));

      m_ResourceManager->SetInitialContents(
          Id, D3D11ResourceManager::InitialContentData(type, stage, 0, NULL));
    }
  }

  return true;
}

bool WrappedID3D11Device::Serialise_InitialState(ResourceId resid, ID3D11DeviceChild *res)
{
  ResourceType type = Resource_Unknown;
  ResourceId Id = ResourceId();

  if(m_State >= WRITING)
  {
    type = IdentifyTypeByPtr(res);
    Id = GetIDForResource(res);

    if(type != Resource_Buffer)
    {
      m_pSerialiser->Serialise("type", type);
      m_pSerialiser->Serialise("Id", Id);
    }
  }
  else
  {
    m_pSerialiser->Serialise("type", type);
    m_pSerialiser->Serialise("Id", Id);
  }

  {
    RDCDEBUG("Serialise_InitialState(%llu)", Id);

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
    if(m_State < WRITING)
    {
      if(m_ResourceManager->HasLiveResource(Id))
      {
        uav = (WrappedID3D11UnorderedAccessView1 *)m_ResourceManager->GetLiveResource(Id);
      }
      else
      {
        uav = NULL;
        SERIALISE_ELEMENT(uint32_t, initCount, 0);
        return true;
      }
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    uav->GetDesc(&desc);

    if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
       (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_COUNTER | D3D11_BUFFER_UAV_FLAG_APPEND)) != 0)
    {
      if(m_State >= WRITING)
      {
        ID3D11Buffer *stage = (ID3D11Buffer *)m_ResourceManager->GetInitialContents(Id).resource;

        uint32_t countData = 0;

        if(stage != NULL)
        {
          D3D11_MAPPED_SUBRESOURCE mapped = {};
          HRESULT hr = E_INVALIDARG;

          if(stage)
            hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);
          else
            RDCERR(
                "Didn't have stage resource for %llu when serialising initial state! "
                "Dirty tracking is incorrect",
                Id);

          if(FAILED(hr))
          {
            RDCERR("Failed to map while getting initial states %08x", hr);
          }
          else
          {
            countData = *((uint32_t *)mapped.pData);

            m_pImmediateContext->GetReal()->Unmap(stage, 0);
          }
        }

        SERIALISE_ELEMENT(uint32_t, count, countData);
      }
      else
      {
        SERIALISE_ELEMENT(uint32_t, initCount, 0);

        m_ResourceManager->SetInitialContents(
            Id, D3D11ResourceManager::InitialContentData(type, NULL, initCount, NULL));
      }
    }
    else
    {
      SERIALISE_ELEMENT(uint32_t, initCount, 0);
    }
  }
  else if(type == Resource_Buffer)
  {
    if(m_State >= WRITING)
    {
      WrappedID3D11Buffer *buf = (WrappedID3D11Buffer *)res;
      D3D11ResourceRecord *record = m_ResourceManager->GetResourceRecord(Id);

      D3D11_BUFFER_DESC desc;
      desc.BindFlags = 0;
      desc.ByteWidth = (UINT)record->Length;
      desc.MiscFlags = 0;
      desc.StructureByteStride = 0;

      ID3D11Buffer *stage = (ID3D11Buffer *)m_ResourceManager->GetInitialContents(Id).resource;

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      HRESULT hr = E_INVALIDARG;

      if(stage)
        hr = m_pImmediateContext->GetReal()->Map(stage, 0, D3D11_MAP_READ, 0, &mapped);
      else
        RDCERR(
            "Didn't have stage resource for %llu when serialising initial state! "
            "Dirty tracking is incorrect",
            Id);

      if(FAILED(hr))
      {
        RDCERR("Failed to map while getting initial states %08x", hr);
      }
      else
      {
        RDCASSERT(record->DataInSerialiser);

        MapIntercept intercept;
        intercept.SetD3D(mapped);
        intercept.Init(buf, record->GetDataPtr());
        intercept.CopyFromD3D();

        m_pImmediateContext->GetReal()->Unmap(stage, 0);
      }
    }
  }
  else if(type == Resource_Texture1D)
  {
    WrappedID3D11Texture1D *tex1D = (WrappedID3D11Texture1D *)res;
    if(m_State < WRITING && m_ResourceManager->HasLiveResource(Id))
      tex1D = (WrappedID3D11Texture1D *)m_ResourceManager->GetLiveResource(Id);

    D3D11ResourceRecord *record = NULL;
    if(m_State >= WRITING)
      record = m_ResourceManager->GetResourceRecord(Id);

    D3D11_TEXTURE1D_DESC desc = {0};
    if(tex1D)
      tex1D->GetDesc(&desc);

    SERIALISE_ELEMENT(uint32_t, numSubresources, desc.MipLevels * desc.ArraySize);

    {
      if(m_State < WRITING)
      {
        ID3D11Texture1D *contents =
            (ID3D11Texture1D *)m_ResourceManager->GetInitialContents(Id).resource;

        RDCASSERT(!contents);
      }

      byte *inmemBuffer = NULL;
      D3D11_SUBRESOURCE_DATA *subData = NULL;

      if(m_State >= WRITING)
      {
        inmemBuffer = new byte[GetByteSize(desc.Width, 1, 1, desc.Format, 0)];
      }
      else if(tex1D)
      {
        subData = new D3D11_SUBRESOURCE_DATA[numSubresources];
      }

      ID3D11Texture1D *stage = (ID3D11Texture1D *)m_ResourceManager->GetInitialContents(Id).resource;

      for(UINT sub = 0; sub < numSubresources; sub++)
      {
        UINT mip = tex1D ? GetMipForSubresource(tex1D, sub) : 0;

        if(m_State >= WRITING)
        {
          D3D11_MAPPED_SUBRESOURCE mapped = {};
          HRESULT hr = E_INVALIDARG;

          if(stage)
            hr = m_pImmediateContext->GetReal()->Map(stage, sub, D3D11_MAP_READ, 0, &mapped);
          else
            RDCERR(
                "Didn't have stage resource for %llu when serialising initial state! "
                "Dirty tracking is incorrect",
                Id);

          size_t dstPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

          if(FAILED(hr))
          {
            RDCERR("Failed to map in initial states %08x", hr);
          }
          else
          {
            byte *dst = inmemBuffer;
            byte *src = (byte *)mapped.pData;

            memcpy(dst, src, dstPitch);
          }

          size_t len = dstPitch;
          m_pSerialiser->SerialiseBuffer("", inmemBuffer, len);

          if(SUCCEEDED(hr))
            m_pImmediateContext->GetReal()->Unmap(stage, 0);
        }
        else
        {
          byte *data = NULL;
          size_t len = 0;
          m_pSerialiser->SerialiseBuffer("", data, len);

          if(tex1D)
          {
            subData[sub].pSysMem = data;
            subData[sub].SysMemPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
            subData[sub].SysMemSlicePitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
          }
          else
          {
            SAFE_DELETE_ARRAY(data);
          }
        }
      }

      SAFE_DELETE_ARRAY(inmemBuffer);

      if(m_State < WRITING && tex1D)
      {
        // We don't need to bind this, but IMMUTABLE requires at least one
        // BindFlags.
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.MiscFlags = 0;

        ID3D11Texture1D *contents = NULL;
        HRESULT hr = m_pDevice->CreateTexture1D(&desc, subData, &contents);

        if(FAILED(hr) || contents == NULL)
        {
          RDCERR("Failed to create staging resource for Texture1D initial contents %08x", hr);
        }
        else
        {
          m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(
                                                        type, contents, eInitialContents_Copy, NULL));
        }

        for(UINT sub = 0; sub < numSubresources; sub++)
          SAFE_DELETE_ARRAY(subData[sub].pSysMem);
        SAFE_DELETE_ARRAY(subData);
      }
    }
  }
  else if(type == Resource_Texture2D)
  {
    WrappedID3D11Texture2D1 *tex2D = (WrappedID3D11Texture2D1 *)res;
    if(m_State < WRITING && m_ResourceManager->HasLiveResource(Id))
      tex2D = (WrappedID3D11Texture2D1 *)m_ResourceManager->GetLiveResource(Id);

    D3D11ResourceRecord *record = NULL;
    if(m_State >= WRITING)
      record = m_ResourceManager->GetResourceRecord(Id);

    D3D11_TEXTURE2D_DESC desc = {0};
    if(tex2D)
      tex2D->GetDesc(&desc);

    SERIALISE_ELEMENT(uint32_t, numSubresources, desc.MipLevels * desc.ArraySize);

    bool bigrt = ((desc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0 ||
                  (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0 ||
                  (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0) &&
                 (desc.Width > 64 && desc.Height > 64) && (desc.Width != desc.Height);

    if(bigrt && m_ResourceManager->ReadBeforeWrite(Id))
      bigrt = false;

    bool multisampled = desc.SampleDesc.Count > 1 || desc.SampleDesc.Quality > 0;

    if(multisampled)
      numSubresources *= desc.SampleDesc.Count;

    SERIALISE_ELEMENT(bool, omitted, bigrt && !RenderDoc::Inst().GetCaptureOptions().SaveAllInitials);

    if(omitted)
    {
      if(m_State >= WRITING)
      {
        RDCWARN("Not serialising texture 2D initial state. ID %llu", Id);
        if(bigrt)
          RDCWARN(
              "Detected Write before Read of this target - assuming initial contents are "
              "unneeded.\n"
              "Capture again with Save All Initials if this is wrong");
      }
    }
    else
    {
      if(m_State < WRITING)
      {
        ID3D11Texture2D *contents =
            (ID3D11Texture2D *)m_ResourceManager->GetInitialContents(Id).resource;

        RDCASSERT(!contents);
      }

      byte *inmemBuffer = NULL;
      D3D11_SUBRESOURCE_DATA *subData = NULL;

      if(m_State >= WRITING)
      {
        inmemBuffer = new byte[GetByteSize(desc.Width, desc.Height, 1, desc.Format, 0)];
      }
      else if(tex2D)
      {
        subData = new D3D11_SUBRESOURCE_DATA[numSubresources];
      }

      ID3D11Texture2D *stage = (ID3D11Texture2D *)m_ResourceManager->GetInitialContents(Id).resource;

      for(UINT sub = 0; sub < numSubresources; sub++)
      {
        UINT mip = tex2D ? GetMipForSubresource(tex2D, sub) : 0;

        if(m_State >= WRITING)
        {
          D3D11_MAPPED_SUBRESOURCE mapped = {};
          HRESULT hr = E_INVALIDARG;

          if(stage)
            hr = m_pImmediateContext->GetReal()->Map(stage, sub, D3D11_MAP_READ, 0, &mapped);
          else
            RDCERR(
                "Didn't have stage resource for %llu when serialising initial state! "
                "Dirty tracking is incorrect",
                Id);

          size_t dstPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
          size_t len = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

          uint32_t rowsPerLine = 1;
          if(IsBlockFormat(desc.Format))
            rowsPerLine = 4;

          if(FAILED(hr))
          {
            RDCERR("Failed to map in initial states %08x", hr);
          }
          else
          {
            byte *dst = inmemBuffer;
            byte *src = (byte *)mapped.pData;
            for(uint32_t row = 0; row<desc.Height>> mip; row += rowsPerLine)
            {
              memcpy(dst, src, dstPitch);
              dst += dstPitch;
              src += mapped.RowPitch;
            }
          }

          m_pSerialiser->SerialiseBuffer("", inmemBuffer, len);

          if(SUCCEEDED(hr))
            m_pImmediateContext->GetReal()->Unmap(stage, sub);
        }
        else
        {
          byte *data = NULL;
          size_t len = 0;
          m_pSerialiser->SerialiseBuffer("", data, len);

          if(tex2D)
          {
            subData[sub].pSysMem = data;
            subData[sub].SysMemPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
            subData[sub].SysMemSlicePitch = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);
          }
          else
          {
            SAFE_DELETE_ARRAY(data);
          }
        }
      }

      SAFE_DELETE_ARRAY(inmemBuffer);

      if(m_State < WRITING && tex2D)
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

        ID3D11Texture2D *contents = NULL;
        hr = m_pDevice->CreateTexture2D(&initialDesc, subData, &contents);

        if(FAILED(hr) || contents == NULL)
        {
          RDCERR("Failed to create staging resource for Texture2D initial contents %08x", hr);
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

            m_DebugManager->CopyArrayToTex2DMS(contentsMS, contents);

            SAFE_RELEASE(contents);
            contents = contentsMS;
          }

          m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(
                                                        type, contents, eInitialContents_Copy, NULL));
        }

        for(UINT sub = 0; sub < numSubresources; sub++)
          SAFE_DELETE_ARRAY(subData[sub].pSysMem);
        SAFE_DELETE_ARRAY(subData);
      }
    }
  }
  else if(type == Resource_Texture3D)
  {
    WrappedID3D11Texture3D1 *tex3D = (WrappedID3D11Texture3D1 *)res;
    if(m_State < WRITING && m_ResourceManager->HasLiveResource(Id))
      tex3D = (WrappedID3D11Texture3D1 *)m_ResourceManager->GetLiveResource(Id);

    D3D11ResourceRecord *record = NULL;
    if(m_State >= WRITING)
      record = m_ResourceManager->GetResourceRecord(Id);

    D3D11_TEXTURE3D_DESC desc = {0};
    if(tex3D)
      tex3D->GetDesc(&desc);

    SERIALISE_ELEMENT(uint32_t, numSubresources, desc.MipLevels);

    {
      if(m_State < WRITING)
      {
        ID3D11Texture3D *contents =
            (ID3D11Texture3D *)m_ResourceManager->GetInitialContents(Id).resource;

        RDCASSERT(!contents);
      }

      byte *inmemBuffer = NULL;
      D3D11_SUBRESOURCE_DATA *subData = NULL;

      if(m_State >= WRITING)
      {
        inmemBuffer = new byte[GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, 0)];
      }
      else if(tex3D)
      {
        subData = new D3D11_SUBRESOURCE_DATA[numSubresources];
      }

      ID3D11Texture3D *stage = (ID3D11Texture3D *)m_ResourceManager->GetInitialContents(Id).resource;

      for(UINT sub = 0; sub < numSubresources; sub++)
      {
        UINT mip = tex3D ? GetMipForSubresource(tex3D, sub) : 0;

        if(m_State >= WRITING)
        {
          D3D11_MAPPED_SUBRESOURCE mapped = {};
          HRESULT hr = E_INVALIDARG;

          if(stage)
            hr = m_pImmediateContext->GetReal()->Map(stage, sub, D3D11_MAP_READ, 0, &mapped);
          else
            RDCERR(
                "Didn't have stage resource for %llu when serialising initial state! "
                "Dirty tracking is incorrect",
                Id);

          size_t dstPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
          size_t dstSlicePitch = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

          uint32_t rowsPerLine = 1;
          if(IsBlockFormat(desc.Format))
            rowsPerLine = 4;

          if(FAILED(hr))
          {
            RDCERR("Failed to map in initial states %08x", hr);
          }
          else
          {
            byte *dst = inmemBuffer;
            byte *src = (byte *)mapped.pData;

            for(uint32_t slice = 0; slice < RDCMAX(1U, desc.Depth >> mip); slice++)
            {
              byte *sliceDst = dst;
              byte *sliceSrc = src;

              for(uint32_t row = 0; row < RDCMAX(1U, desc.Height >> mip); row += rowsPerLine)
              {
                memcpy(sliceDst, sliceSrc, dstPitch);
                sliceDst += dstPitch;
                sliceSrc += mapped.RowPitch;
              }

              dst += dstSlicePitch;
              src += mapped.DepthPitch;
            }
          }

          size_t len = dstSlicePitch * desc.Depth;
          m_pSerialiser->SerialiseBuffer("", inmemBuffer, len);

          if(SUCCEEDED(hr))
            m_pImmediateContext->GetReal()->Unmap(stage, sub);
        }
        else
        {
          byte *data = NULL;
          size_t len = 0;
          m_pSerialiser->SerialiseBuffer("", data, len);

          if(tex3D)
          {
            subData[sub].pSysMem = data;
            subData[sub].SysMemPitch = GetByteSize(desc.Width, 1, 1, desc.Format, mip);
            subData[sub].SysMemSlicePitch = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);
          }
          else
          {
            SAFE_DELETE_ARRAY(data);
          }
        }
      }

      SAFE_DELETE_ARRAY(inmemBuffer);

      if(m_State < WRITING && tex3D)
      {
        // We don't need to bind this, but IMMUTABLE requires at least one
        // BindFlags.
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.MiscFlags = 0;

        ID3D11Texture3D *contents = NULL;
        HRESULT hr = m_pDevice->CreateTexture3D(&desc, subData, &contents);

        if(FAILED(hr) || contents == NULL)
        {
          RDCERR("Failed to create staging resource for Texture3D initial contents %08x", hr);
        }
        else
        {
          m_ResourceManager->SetInitialContents(Id, D3D11ResourceManager::InitialContentData(
                                                        type, contents, eInitialContents_Copy, NULL));
        }

        for(UINT sub = 0; sub < numSubresources; sub++)
          SAFE_DELETE_ARRAY(subData[sub].pSysMem);
        SAFE_DELETE_ARRAY(subData);
      }
    }
  }
  else
  {
    RDCERR("Trying to serialise initial state of unsupported resource type");
  }

  return true;
}

void WrappedID3D11Device::Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData)
{
  ResourceType type = IdentifyTypeByPtr(live);

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
        RDCERR("Failed to create staging resource for UAV initial contents %08x", hr);
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
          RDCERR("Failed to map while creating initial states %08x", hr);
        }
        else
        {
          countData = *((uint32_t *)mapped.pData);

          m_pImmediateContext->GetReal()->Unmap(stage, 0);
        }

        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, NULL, countData, NULL));

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

      ID3D11RenderTargetView *initContents = NULL;

      HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture1D, tex1D), &rdesc,
                                                     &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear RTV while creating initial states %08x", hr);
      }
      else
      {
        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents,
                                                         eInitialContents_ClearRTV, NULL));
      }
    }
    else if(!hasData && desc.MipLevels == 1 && (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
    {
      D3D11_DEPTH_STENCIL_VIEW_DESC ddesc;
      ddesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
      ddesc.Format = GetDepthTypedFormat(desc.Format);
      ddesc.Texture1D.MipSlice = 0;
      ddesc.Flags = 0;

      ID3D11DepthStencilView *initContents = NULL;

      HRESULT hr = m_pDevice->CreateDepthStencilView(UNWRAP(WrappedID3D11Texture1D, tex1D), &ddesc,
                                                     &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear DSV while creating initial states %08x", hr);
      }
      else
      {
        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents,
                                                         eInitialContents_ClearDSV, NULL));
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

      ID3D11Texture1D *initContents = NULL;

      HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create tex3D while creating initial states %08x", hr);
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyResource(initContents,
                                                     UNWRAP(WrappedID3D11Texture1D, tex1D));

        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents, eInitialContents_Copy,
                                                         NULL));
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

      ID3D11RenderTargetView *initContents = NULL;

      HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D1, tex2D), &rdesc,
                                                     &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear RTV while creating initial states %08x", hr);
      }
      else
      {
        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents,
                                                         eInitialContents_ClearRTV, NULL));
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

      ID3D11DepthStencilView *initContents = NULL;

      HRESULT hr = m_pDevice->CreateDepthStencilView(UNWRAP(WrappedID3D11Texture2D1, tex2D), &ddesc,
                                                     &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear DSV while creating initial states %08x", hr);
      }
      else
      {
        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents,
                                                         eInitialContents_ClearDSV, NULL));
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

      ID3D11Texture2D *initContents = NULL;

      HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create tex2D while creating initial states %08x", hr);
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyResource(initContents,
                                                     UNWRAP(WrappedID3D11Texture2D1, tex2D));

        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents, eInitialContents_Copy,
                                                         NULL));
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

      ID3D11RenderTargetView *initContents = NULL;

      HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture3D1, tex3D), &rdesc,
                                                     &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create fast-clear RTV while creating initial states %08x", hr);
      }
      else
      {
        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents,
                                                         eInitialContents_ClearRTV, NULL));
      }
    }
    else if(!hasData && desc.Usage != D3D11_USAGE_IMMUTABLE)
    {
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = 0;
      desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

      ID3D11Texture3D *initContents = NULL;

      HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &initContents);

      if(FAILED(hr))
      {
        RDCERR("Failed to create tex3D while creating initial states %08x", hr);
      }
      else
      {
        m_pImmediateContext->GetReal()->CopyResource(initContents,
                                                     UNWRAP(WrappedID3D11Texture3D1, tex3D));

        m_ResourceManager->SetInitialContents(
            id, D3D11ResourceManager::InitialContentData(type, initContents, eInitialContents_Copy,
                                                         NULL));
      }
    }
  }
}

void WrappedID3D11Device::Apply_InitialState(ID3D11DeviceChild *live,
                                             D3D11ResourceManager::InitialContentData initial)
{
  if(initial.resourceType == Resource_UnorderedAccessView)
  {
    ID3D11UnorderedAccessView *uav = (ID3D11UnorderedAccessView *)live;

    m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &uav, &initial.num);
  }
  else
  {
    if(initial.num == eInitialContents_ClearRTV)
    {
      float emptyCol[] = {0.0f, 0.0f, 0.0f, 0.0f};
      m_pImmediateContext->GetReal()->ClearRenderTargetView(
          (ID3D11RenderTargetView *)initial.resource, emptyCol);
    }
    else if(initial.num == eInitialContents_ClearDSV)
    {
      m_pImmediateContext->GetReal()->ClearDepthStencilView(
          (ID3D11DepthStencilView *)initial.resource, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,
          0);
    }
    else if(initial.num == eInitialContents_Copy)
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
