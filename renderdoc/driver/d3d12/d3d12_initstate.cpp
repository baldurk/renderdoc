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

#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

bool D3D12ResourceManager::Force_InitialState(ID3D12DeviceChild *res, bool prepare)
{
  return false;
}

bool D3D12ResourceManager::Need_InitialStateChunk(ID3D12DeviceChild *res)
{
  return true;
}

bool D3D12ResourceManager::Prepare_InitialState(ID3D12DeviceChild *res)
{
  ResourceId id = GetResID(res);
  D3D12ResourceType type = IdentifyTypeByPtr(res);

  if(type == Resource_DescriptorHeap)
  {
    WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)res;

    UINT numElems = heap->GetDesc().NumDescriptors;

    D3D12Descriptor *descs =
        (D3D12Descriptor *)AllocAlignedBuffer(sizeof(D3D12Descriptor) * numElems);

    memcpy(descs, heap->GetDescriptors(), sizeof(D3D12Descriptor) * numElems);

    SetInitialContents(heap->GetResourceID(), D3D12ResourceManager::InitialContentData(
                                                  type, NULL, numElems, (byte *)descs));
    return true;
  }
  else if(type == Resource_Resource)
  {
    WrappedID3D12Resource *r = (WrappedID3D12Resource *)res;
    ID3D12Pageable *pageable = r;

    bool nonresident = false;
    if(!r->Resident())
      nonresident = true;

    D3D12_RESOURCE_DESC desc = r->GetDesc();

    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.SampleDesc.Count > 1)
    {
      D3D12NOTIMP("Multisampled initial contents");

      SetInitialContents(GetResID(r), D3D12ResourceManager::InitialContentData(type, NULL, 2, NULL));
      return true;
    }
    else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      D3D12_HEAP_PROPERTIES heapProps;
      r->GetHeapProperties(&heapProps, NULL);

      if(heapProps.Type == D3D12_HEAP_TYPE_READBACK)
      {
        // already on readback heap, just mark that we can map it directly and continue
        SetInitialContents(GetResID(r),
                           D3D12ResourceManager::InitialContentData(type, NULL, 1, NULL));
        return true;
      }

      heapProps.Type = D3D12_HEAP_TYPE_READBACK;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      desc.Flags = D3D12_RESOURCE_FLAG_NONE;

      ID3D12Resource *copyDst = NULL;
      HRESULT hr = m_Device->GetReal()->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
          __uuidof(ID3D12Resource), (void **)&copyDst);

      if(nonresident)
        m_Device->MakeResident(1, &pageable);

      if(SUCCEEDED(hr))
      {
        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

        list->CopyResource(copyDst, r->GetReal());
      }
      else
      {
        RDCERR("Couldn't create readback buffer: 0x%08x", hr);
      }

      if(nonresident)
      {
        m_Device->CloseInitialStateList();

        m_Device->ExecuteLists();
        m_Device->FlushLists();

        m_Device->Evict(1, &pageable);
      }

      SetInitialContents(GetResID(r),
                         D3D12ResourceManager::InitialContentData(type, copyDst, 0, NULL));
      return true;
    }
    else
    {
      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_READBACK;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      D3D12_RESOURCE_DESC bufDesc;

      bufDesc.Alignment = 0;
      bufDesc.DepthOrArraySize = 1;
      bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      bufDesc.Format = DXGI_FORMAT_UNKNOWN;
      bufDesc.Height = 1;
      bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      bufDesc.MipLevels = 1;
      bufDesc.SampleDesc.Count = 1;
      bufDesc.SampleDesc.Quality = 0;
      bufDesc.Width = 1;

      UINT numSubresources = desc.MipLevels;
      if(desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        numSubresources *= desc.DepthOrArraySize;

      D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts =
          new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numSubresources];

      m_Device->GetCopyableFootprints(&desc, 0, numSubresources, 0, layouts, NULL, NULL,
                                      &bufDesc.Width);

      ID3D12Resource *copyDst = NULL;
      HRESULT hr = m_Device->GetReal()->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
          __uuidof(ID3D12Resource), (void **)&copyDst);

      if(nonresident)
        m_Device->MakeResident(1, &pageable);

      if(SUCCEEDED(hr))
      {
        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

        vector<D3D12_RESOURCE_BARRIER> barriers;

        const vector<D3D12_RESOURCE_STATES> &states = m_Device->GetSubresourceStates(GetResID(r));

        barriers.reserve(states.size());

        for(size_t i = 0; i < states.size(); i++)
        {
          if(states[i] & D3D12_RESOURCE_STATE_COPY_SOURCE)
            continue;

          D3D12_RESOURCE_BARRIER barrier;
          barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
          barrier.Transition.pResource = r->GetReal();
          barrier.Transition.Subresource = (UINT)i;
          barrier.Transition.StateBefore = states[i];
          barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

          barriers.push_back(barrier);
        }

        // transition to copy dest
        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

        for(UINT i = 0; i < numSubresources; i++)
        {
          D3D12_TEXTURE_COPY_LOCATION dst, src;

          src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
          src.pResource = r->GetReal();
          src.SubresourceIndex = i;

          dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          dst.pResource = copyDst;
          dst.PlacedFootprint = layouts[i];

          list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
        }

        // transition back
        for(size_t i = 0; i < barriers.size(); i++)
          std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);
      }
      else
      {
        RDCERR("Couldn't create readback buffer: 0x%08x", hr);
      }

      if(nonresident)
      {
        m_Device->CloseInitialStateList();

        m_Device->ExecuteLists();
        m_Device->FlushLists();

        m_Device->Evict(1, &pageable);
      }

      SAFE_DELETE_ARRAY(layouts);

      SetInitialContents(GetResID(r),
                         D3D12ResourceManager::InitialContentData(type, copyDst, 0, NULL));
      return true;
    }
  }
  else
  {
    RDCERR("Unexpected type needing an initial state prepared: %d", type);
  }

  return false;
}

bool D3D12ResourceManager::Serialise_InitialState(ResourceId resid, ID3D12DeviceChild *liveRes)
{
  D3D12ResourceRecord *record = NULL;
  if(m_State >= WRITING)
    record = GetResourceRecord(resid);

  SERIALISE_ELEMENT(ResourceId, id, resid);
  SERIALISE_ELEMENT(D3D12ResourceType, type, record->type);

  if(m_State >= WRITING)
  {
    D3D12ResourceManager::InitialContentData initContents = GetInitialContents(id);

    if(type == Resource_DescriptorHeap)
    {
      D3D12Descriptor *descs = (D3D12Descriptor *)initContents.blob;
      uint32_t numElems = initContents.num;

      m_pSerialiser->SerialiseComplexArray("Descriptors", descs, numElems);
    }
    else if(type == Resource_Resource)
    {
      m_Device->ExecuteLists();
      m_Device->FlushLists();

      ID3D12Resource *copiedBuffer = (ID3D12Resource *)initContents.resource;

      if(initContents.num == 1)
      {
        copiedBuffer = (ID3D12Resource *)liveRes;
      }

      if(initContents.num == 2)
      {
        D3D12NOTIMP("Multisampled initial contents");
        return true;
      }

      byte dummy[4] = {};
      byte *ptr = NULL;
      uint64_t size = 0;

      HRESULT hr = E_NOINTERFACE;

      if(copiedBuffer)
      {
        hr = copiedBuffer->Map(0, NULL, (void **)&ptr);
        size = (uint64_t)copiedBuffer->GetDesc().Width;
      }

      if(FAILED(hr) || ptr == NULL)
      {
        size = 4;
        ptr = dummy;

        RDCERR("Failed to map buffer for readback! 0x%08x", hr);
      }

      m_pSerialiser->Serialise("NumBytes", size);
      size_t sz = (size_t)size;
      m_pSerialiser->SerialiseBuffer("BufferData", ptr, sz);

      if(SUCCEEDED(hr) && ptr)
        copiedBuffer->Unmap(0, NULL);

      return true;
    }
    else
    {
      RDCERR("Unexpected type needing an initial state serialised out: %d", type);
      return false;
    }
  }
  else
  {
    ID3D12DeviceChild *res = GetLiveResource(id);

    RDCASSERT(res != NULL);

    ResourceId liveid = GetLiveID(id);

    if(type == Resource_DescriptorHeap)
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)res;

      uint32_t numElems = 0;
      D3D12Descriptor *descs = NULL;

      m_pSerialiser->SerialiseComplexArray("Descriptors", descs, numElems);

      D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();

      // this heap doesn't have to be shader visible, we just use it to copy from
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

      ID3D12DescriptorHeap *copyheap = NULL;
      HRESULT hr = m_Device->GetReal()->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                                             (void **)&copyheap);

      if(FAILED(hr))
      {
        RDCERR("Failed to create CPU descriptor heap for initial state: 0x%08x", hr);
        return false;
      }

      copyheap = new WrappedID3D12DescriptorHeap(copyheap, m_Device, desc);

      D3D12_CPU_DESCRIPTOR_HANDLE handle = copyheap->GetCPUDescriptorHandleForHeapStart();

      UINT increment = m_Device->GetDescriptorHandleIncrementSize(desc.Type);

      for(uint32_t i = 0; i < numElems; i++)
      {
        descs[i].Create(desc.Type, m_Device, handle);

        handle.ptr += increment;
      }

      SAFE_DELETE_ARRAY(descs);

      SetInitialContents(id, D3D12ResourceManager::InitialContentData(type, copyheap, 0, NULL));
    }
    else if(type == Resource_Resource)
    {
      D3D12_RESOURCE_DESC resDesc = ((ID3D12Resource *)res)->GetDesc();

      if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resDesc.SampleDesc.Count > 1)
      {
        D3D12NOTIMP("Multisampled initial contents");
        return true;
      }

      uint64_t size = 0;

      m_pSerialiser->Serialise("NumBytes", size);

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      D3D12_RESOURCE_DESC desc;
      desc.Alignment = 0;
      desc.DepthOrArraySize = 1;
      desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Flags = D3D12_RESOURCE_FLAG_NONE;
      desc.Format = DXGI_FORMAT_UNKNOWN;
      desc.Height = 1;
      desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      desc.MipLevels = 1;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;
      desc.Width = size;

      ID3D12Resource *copySrc = NULL;
      HRESULT hr = m_Device->GetReal()->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
          __uuidof(ID3D12Resource), (void **)&copySrc);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create upload buffer: 0x%08x", hr);
        return false;
      }

      byte *ptr = NULL;
      hr = copySrc->Map(0, NULL, (void **)&ptr);

      if(FAILED(hr))
      {
        RDCERR("Couldn't map upload buffer: 0x%08x", hr);
        ptr = NULL;
      }

      size_t sz = (size_t)size;

      m_pSerialiser->SerialiseBuffer("BufferData", ptr, sz);

      if(SUCCEEDED(hr))
        copySrc->Unmap(0, NULL);
      else
        SAFE_DELETE_ARRAY(ptr);

      SetInitialContents(id, D3D12ResourceManager::InitialContentData(type, copySrc, 1, NULL));

      return true;
    }
    else
    {
      RDCERR("Unexpected type needing an initial state serialised in: %d", type);
      return false;
    }
  }

  return true;
}

void D3D12ResourceManager::Create_InitialState(ResourceId id, ID3D12DeviceChild *live, bool hasData)
{
  D3D12ResourceType type = IdentifyTypeByPtr(live);

  if(type == Resource_DescriptorHeap)
  {
    // set a NULL heap, if there are no initial contents for a descriptor heap we just leave
    // it all entirely undefined.
    SetInitialContents(id, D3D12ResourceManager::InitialContentData(type, NULL, 1, NULL));
  }
  else if(type == Resource_Resource)
  {
    D3D12NOTIMP("Creating init states for resources");

    // not handling any missing states at the moment
  }
  else
  {
    RDCERR("Unexpected type needing an initial state created: %d", type);
  }
}

void D3D12ResourceManager::Apply_InitialState(ID3D12DeviceChild *live, InitialContentData data)
{
  D3D12ResourceType type = (D3D12ResourceType)data.resourceType;

  if(type == Resource_DescriptorHeap)
  {
    ID3D12DescriptorHeap *dstheap = (ID3D12DescriptorHeap *)live;
    ID3D12DescriptorHeap *srcheap = (ID3D12DescriptorHeap *)data.resource;

    if(srcheap)
    {
      // copy the whole heap
      m_Device->CopyDescriptorsSimple(
          srcheap->GetDesc().NumDescriptors, dstheap->GetCPUDescriptorHandleForHeapStart(),
          srcheap->GetCPUDescriptorHandleForHeapStart(), srcheap->GetDesc().Type);
    }
  }
  else if(type == Resource_Resource)
  {
    if(data.num == 1 && data.resource)
    {
      ID3D12Resource *copyDst = Unwrap((ID3D12Resource *)live);
      ID3D12Resource *copySrc = (ID3D12Resource *)data.resource;

      D3D12_HEAP_PROPERTIES heapProps = {};
      copyDst->GetHeapProperties(&heapProps, NULL);

      // if destination is on the upload heap, it's impossible to copy via the device,
      // so we have to map both sides and CPU copy.
      if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
      {
        byte *src = NULL, *dst = NULL;

        HRESULT hr = S_OK;

        hr = copySrc->Map(0, NULL, (void **)&src);

        if(FAILED(hr))
        {
          RDCERR("Doing CPU-side copy, couldn't map source: 0x%08x", hr);
          src = NULL;
        }

        if(copyDst->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
          hr = copyDst->Map(0, NULL, (void **)&dst);

          if(FAILED(hr))
          {
            RDCERR("Doing CPU-side copy, couldn't map source: 0x%08x", hr);
            dst = NULL;
          }

          if(src && dst)
          {
            memcpy(dst, src, (size_t)copySrc->GetDesc().Width);
          }

          if(dst)
            copyDst->Unmap(0, NULL);
        }
        else
        {
          D3D12_RESOURCE_DESC desc = copyDst->GetDesc();

          UINT numSubresources = desc.MipLevels;
          if(desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
            numSubresources *= desc.DepthOrArraySize;

          D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts =
              new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numSubresources];
          UINT *numrows = new UINT[numSubresources];
          UINT64 *rowsizes = new UINT64[numSubresources];

          m_Device->GetCopyableFootprints(&desc, 0, numSubresources, 0, layouts, numrows, rowsizes,
                                          NULL);

          for(UINT i = 0; i < numSubresources; i++)
          {
            hr = copyDst->Map(i, NULL, (void **)&dst);

            if(FAILED(hr))
            {
              RDCERR("Doing CPU-side copy, couldn't map source: 0x%08x", hr);
              dst = NULL;
            }

            if(src && dst)
            {
              byte *bufPtr = src + layouts[i].Offset;
              byte *texPtr = dst;

              for(UINT d = 0; d < layouts[i].Footprint.Depth; d++)
              {
                for(UINT r = 0; r < numrows[i]; r++)
                {
                  memcpy(bufPtr, texPtr, (size_t)rowsizes[i]);

                  bufPtr += layouts[i].Footprint.RowPitch;
                  texPtr += rowsizes[i];
                }
              }
            }

            if(dst)
              copyDst->Unmap(0, NULL);
          }

          delete[] layouts;
          delete[] numrows;
          delete[] rowsizes;
        }

        if(src)
          copySrc->Unmap(0, NULL);
      }
      else
      {
        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

        vector<D3D12_RESOURCE_BARRIER> barriers;

        const vector<D3D12_RESOURCE_STATES> &states = m_Device->GetSubresourceStates(GetResID(live));

        barriers.reserve(states.size());

        for(size_t i = 0; i < states.size(); i++)
        {
          if(states[i] & D3D12_RESOURCE_STATE_COPY_DEST)
            continue;

          D3D12_RESOURCE_BARRIER barrier;
          barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
          barrier.Transition.pResource = copyDst;
          barrier.Transition.Subresource = (UINT)i;
          barrier.Transition.StateBefore = states[i];
          barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

          barriers.push_back(barrier);
        }

        // transition to copy dest
        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

        if(copyDst->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
          list->CopyBufferRegion(copyDst, 0, copySrc, 0, copySrc->GetDesc().Width);
        }
        else
        {
          D3D12_RESOURCE_DESC desc = copyDst->GetDesc();

          UINT numSubresources = desc.MipLevels;
          if(desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
            numSubresources *= desc.DepthOrArraySize;

          D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts =
              new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numSubresources];

          m_Device->GetCopyableFootprints(&desc, 0, numSubresources, 0, layouts, NULL, NULL, NULL);

          for(UINT i = 0; i < numSubresources; i++)
          {
            D3D12_TEXTURE_COPY_LOCATION dst, src;

            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = copyDst;
            dst.SubresourceIndex = i;

            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.pResource = copySrc;
            src.PlacedFootprint = layouts[i];

            list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
          }

          delete[] layouts;
        }

        // transition back to whatever it was before
        for(size_t i = 0; i < barriers.size(); i++)
          std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        m_Device->CloseInitialStateList();
        m_Device->ExecuteLists();
        m_Device->FlushLists(true);
#endif
      }
    }
    else
    {
      RDCERR("Unexpected num or NULL resource: %d, %p", data.num, data.resource);
    }
  }
  else
  {
    RDCERR("Unexpected type needing an initial state created: %d", type);
  }
}
