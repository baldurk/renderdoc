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

#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

bool D3D12ResourceManager::Prepare_InitialState(ID3D12DeviceChild *res)
{
  ResourceId id = GetResID(res);
  D3D12ResourceType type = IdentifyTypeByPtr(res);

  if(type == Resource_DescriptorHeap)
  {
    WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)res;

    UINT numElems = heap->GetDesc().NumDescriptors;

    D3D12Descriptor *descs = new D3D12Descriptor[numElems];
    memcpy(descs, heap->GetDescriptors(), sizeof(D3D12Descriptor) * numElems);

    SetInitialContents(heap->GetResourceID(), D3D12InitialContents(descs, numElems));
    return true;
  }
  else if(type == Resource_Resource)
  {
    WrappedID3D12Resource1 *r = (WrappedID3D12Resource1 *)res;
    ID3D12Pageable *pageable = r;

    bool nonresident = false;
    if(!r->Resident())
      nonresident = true;

    D3D12_RESOURCE_DESC desc = r->GetDesc();

    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      D3D12_HEAP_PROPERTIES heapProps;
      r->GetHeapProperties(&heapProps, NULL);

      HRESULT hr = S_OK;

      if(heapProps.Type == D3D12_HEAP_TYPE_READBACK)
      {
        // readback resources can't be copied by the GPU but are always immediately CPU readable, so
        // copy to a buffer now
        size_t size = size_t(desc.Width);
        byte *buffer = AllocAlignedBuffer(RDCMAX(desc.Width, 64ULL));

        byte *bufData = NULL;
        hr = r->GetReal()->Map(0, NULL, (void **)&bufData);

        if(SUCCEEDED(hr))
        {
          memcpy(buffer, bufData, size);

          D3D12_RANGE range = {};
          r->GetReal()->Unmap(0, &range);
        }
        else
        {
          RDCERR("Couldn't map directly readback buffer: HRESULT: %s", ToStr(hr).c_str());
        }

        SetInitialContents(GetResID(r), D3D12InitialContents(buffer, size));
        return true;
      }

      heapProps.Type = D3D12_HEAP_TYPE_READBACK;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      desc.Flags = D3D12_RESOURCE_FLAG_NONE;

      ID3D12Resource *copyDst = NULL;
      hr = m_Device->GetReal()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                        D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                        __uuidof(ID3D12Resource), (void **)&copyDst);

      if(nonresident)
        m_Device->MakeResident(1, &pageable);

      const std::vector<D3D12_RESOURCE_STATES> &states =
          m_Device->GetSubresourceStates(GetResID(res));
      RDCASSERT(states.size() == 1);

      D3D12_RESOURCE_BARRIER barrier;
      const bool isUploadHeap = (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD);
      const bool needsTransition =
          !isUploadHeap && ((states[0] & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0);

      if(needsTransition)
      {
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = r->GetReal();
        barrier.Transition.Subresource = (UINT)0;
        barrier.Transition.StateBefore = states[0];
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
      }

      ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

      // transition to copy source
      if(needsTransition)
        list->ResourceBarrier(1, &barrier);

      if(SUCCEEDED(hr))
      {
        list->CopyResource(copyDst, r->GetReal());
      }
      else
      {
        RDCERR("Couldn't create readback buffer: HRESULT: %s", ToStr(hr).c_str());
      }

      // transition back to whatever it was before
      if(needsTransition)
      {
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        list->ResourceBarrier(1, &barrier);
      }

      if(nonresident)
      {
        m_Device->CloseInitialStateList();

        m_Device->ExecuteLists(NULL, true);
        m_Device->FlushLists();

        m_Device->Evict(1, &pageable);
      }

      SetInitialContents(GetResID(r), D3D12InitialContents(copyDst));
      return true;
    }
    else
    {
      if(nonresident)
        m_Device->MakeResident(1, &pageable);

      ID3D12Resource *arrayTexture = NULL;
      D3D12_RESOURCE_STATES destState = D3D12_RESOURCE_STATE_COPY_SOURCE;
      ID3D12Resource *unwrappedCopySource = r->GetReal();

      bool isDepth =
          IsDepthFormat(desc.Format) || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;

      if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.SampleDesc.Count > 1)
      {
        desc.Alignment = 0;
        desc.DepthOrArraySize *= (UINT16)desc.SampleDesc.Count;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        if(isDepth)
          desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        else
          desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_PROPERTIES defaultHeap;
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeap.CreationNodeMask = 1;
        defaultHeap.VisibleNodeMask = 1;

        // we don't want to serialise this resource's creation, so wrap it manually
        HRESULT hr = m_Device->GetReal()->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
            isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
            __uuidof(ID3D12Resource), (void **)&arrayTexture);
        RDCASSERTEQUAL(hr, S_OK);

        destState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
      }

      ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

      std::vector<D3D12_RESOURCE_BARRIER> barriers;

      {
        const std::vector<D3D12_RESOURCE_STATES> &states =
            m_Device->GetSubresourceStates(GetResID(r));

        barriers.reserve(states.size());

        for(size_t i = 0; i < states.size(); i++)
        {
          if(states[i] & destState)
            continue;

          D3D12_RESOURCE_BARRIER barrier;
          barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
          barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
          barrier.Transition.pResource = r->GetReal();
          barrier.Transition.Subresource = (UINT)i;
          barrier.Transition.StateBefore = states[i];
          barrier.Transition.StateAfter = destState;

          barriers.push_back(barrier);
        }

        // transition to copy dest
        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);
      }

      if(arrayTexture)
      {
        // execute the above barriers
        m_Device->CloseInitialStateList();

        m_Device->ExecuteLists(NULL, true);
        m_Device->FlushLists();

        // expand multisamples out to array
        m_Device->GetDebugManager()->CopyTex2DMSToArray(arrayTexture, r->GetReal());

        // open the initial state list again for the remainder of the work
        list = Unwrap(m_Device->GetInitialStateList());

        D3D12_RESOURCE_BARRIER b = {};
        b.Transition.pResource = arrayTexture;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore =
            isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        list->ResourceBarrier(1, &b);

        unwrappedCopySource = arrayTexture;
      }

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

      // account for multiple planes (i.e. depth and stencil)
      {
        D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
        formatInfo.Format = desc.Format;
        m_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

        UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

        numSubresources *= planes;
      }

      D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts =
          new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numSubresources];

      m_Device->GetCopyableFootprints(&desc, 0, numSubresources, 0, layouts, NULL, NULL,
                                      &bufDesc.Width);

      ID3D12Resource *copyDst = NULL;
      HRESULT hr = m_Device->GetReal()->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
          __uuidof(ID3D12Resource), (void **)&copyDst);

      if(SUCCEEDED(hr))
      {
        for(UINT i = 0; i < numSubresources; i++)
        {
          D3D12_TEXTURE_COPY_LOCATION dst, src;

          src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
          src.pResource = unwrappedCopySource;
          src.SubresourceIndex = i;

          dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          dst.pResource = copyDst;
          dst.PlacedFootprint = layouts[i];

          list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
        }
      }
      else
      {
        RDCERR("Couldn't create readback buffer: HRESULT: %s", ToStr(hr).c_str());
      }

      // transition back
      for(size_t i = 0; i < barriers.size(); i++)
        std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

      if(!barriers.empty())
        list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

      if(nonresident || arrayTexture)
      {
        m_Device->CloseInitialStateList();

        m_Device->ExecuteLists(NULL, true);
        m_Device->FlushLists();

        if(nonresident)
          m_Device->Evict(1, &pageable);
      }

      SAFE_RELEASE(arrayTexture);
      SAFE_DELETE_ARRAY(layouts);

      SetInitialContents(GetResID(r), D3D12InitialContents(copyDst));
      return true;
    }
  }
  else
  {
    RDCERR("Unexpected type needing an initial state prepared: %d", type);
  }

  return false;
}

uint64_t D3D12ResourceManager::GetSize_InitialState(ResourceId id, const D3D12InitialContents &data)
{
  if(data.resourceType == Resource_DescriptorHeap)
  {
    // the initial contents are just the descriptors. Estimate the serialise size here
    const uint64_t descriptorSerSize = 40 + sizeof(D3D12_SAMPLER_DESC);

    // add a little extra room for fixed overhead
    return 64 + data.numDescriptors * descriptorSerSize;
  }
  else if(data.resourceType == Resource_Resource)
  {
    ID3D12Resource *buf = (ID3D12Resource *)data.resource;

    // readback heaps have already been copied to a buffer, so use that length
    if(data.tag == D3D12InitialContents::MapDirect)
      return WriteSerialiser::GetChunkAlignment() + 16 + uint64_t(data.dataSize);

    return WriteSerialiser::GetChunkAlignment() + 16 + uint64_t(buf ? buf->GetDesc().Width : 0);
  }
  else
  {
    RDCERR("Unexpected type needing an initial state serialised: %d", data.resourceType);
  }

  return 16;
}

template <typename SerialiserType>
bool D3D12ResourceManager::Serialise_InitialState(SerialiserType &ser, ResourceId id,
                                                  D3D12ResourceRecord *record,
                                                  const D3D12InitialContents *initial)
{
  m_State = m_Device->GetState();

  bool ret = true;

  SERIALISE_ELEMENT(id).TypedAs("ID3D12DeviceChild *"_lit);
  SERIALISE_ELEMENT_LOCAL(type, record->type);

  if(IsReplayingAndReading())
  {
    m_Device->AddResourceCurChunk(id);
  }

  if(type == Resource_DescriptorHeap)
  {
    D3D12Descriptor *Descriptors = initial ? initial->descriptors : NULL;
    uint32_t numElems = initial ? initial->numDescriptors : 0;

    SERIALISE_ELEMENT_ARRAY(Descriptors, numElems);
    SERIALISE_ELEMENT(numElems);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)GetLiveResource(id);

      D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();

      // this heap doesn't have to be shader visible, we just use it to copy from
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

      ID3D12DescriptorHeap *copyheap = NULL;
      HRESULT hr = m_Device->GetReal()->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                                             (void **)&copyheap);

      if(FAILED(hr))
      {
        RDCERR("Failed to create CPU descriptor heap for initial state: HRESULT: %s",
               ToStr(hr).c_str());
        return false;
      }

      if(Descriptors == NULL)
      {
        RDCERR("Failed to correctly serialise descriptor heap initial state");
        return false;
      }

      copyheap = new WrappedID3D12DescriptorHeap(copyheap, m_Device, desc);

      D3D12_CPU_DESCRIPTOR_HANDLE handle = copyheap->GetCPUDescriptorHandleForHeapStart();

      UINT increment = m_Device->GetDescriptorHandleIncrementSize(desc.Type);

      for(uint32_t i = 0; i < RDCMIN(numElems, desc.NumDescriptors); i++)
      {
        Descriptors[i].Create(desc.Type, m_Device, handle);

        handle.ptr += increment;
      }

      SetInitialContents(id, D3D12InitialContents(copyheap));
    }
  }
  else if(type == Resource_Resource)
  {
    byte *ResourceContents = NULL;
    uint64_t ContentsLength = 0;
    byte *dummy = NULL;
    ID3D12Resource *mappedBuffer = NULL;

    ID3D12Resource *liveRes = NULL;

    if(IsReplayingAndReading())
    {
      liveRes = (ID3D12Resource *)GetLiveResource(id);
    }

    if(ser.IsWriting())
    {
      m_Device->ExecuteLists(NULL, true);
      m_Device->FlushLists();

      RDCASSERT(initial);

      mappedBuffer = (ID3D12Resource *)initial->resource;

      if(initial->tag == D3D12InitialContents::MapDirect)
      {
        // this was a readback heap, so we did the readback in Prepare already to a buffer
        ResourceContents = initial->srcData;
        ContentsLength = uint64_t(initial->dataSize);
        mappedBuffer = NULL;
      }
      else if(mappedBuffer)
      {
        HRESULT hr = mappedBuffer->Map(0, NULL, (void **)&ResourceContents);
        ContentsLength = mappedBuffer->GetDesc().Width;

        if(FAILED(hr) || ResourceContents == NULL)
        {
          ContentsLength = 0;
          ResourceContents = NULL;
          mappedBuffer = NULL;

          RDCERR("Failed to map buffer for readback! %s", ToStr(hr).c_str());
          ret = false;
        }
      }
    }

    // serialise the size separately so we can recreate on replay
    SERIALISE_ELEMENT(ContentsLength);

    // only map on replay if we haven't encountered any errors so far
    if(IsReplayingAndReading() && !ser.IsErrored())
    {
      D3D12_RESOURCE_DESC resDesc = liveRes->GetDesc();

      D3D12_HEAP_PROPERTIES heapProps = {};
      liveRes->GetHeapProperties(&heapProps, NULL);

      if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
      {
        // if destination is on the upload heap, it's impossible to copy via the device,
        // so we have to CPU copy. To save time and make a more optimal copy, we just keep the data
        // CPU-side
        mappedBuffer = NULL;

        D3D12InitialContents initContents(D3D12InitialContents::Copy, type);
        ResourceContents = initContents.srcData = AllocAlignedBuffer(RDCMAX(ContentsLength, 64ULL));
        initContents.resourceType = Resource_Resource;
        SetInitialContents(id, initContents);
      }
      else
      {
        // create an upload buffer to contain the contents
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
        desc.Width = RDCMAX(ContentsLength, 64ULL);

        ID3D12Resource *copySrc = NULL;
        HRESULT hr = m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                       __uuidof(ID3D12Resource), (void **)&copySrc);

        if(SUCCEEDED(hr))
        {
          mappedBuffer = copySrc;

          // map the upload buffer to serialise into
          hr = copySrc->Map(0, NULL, (void **)&ResourceContents);

          if(FAILED(hr))
          {
            RDCERR("Created but couldn't map upload buffer: %s", ToStr(hr).c_str());
            ret = false;
            SAFE_RELEASE(copySrc);
            mappedBuffer = NULL;
            ResourceContents = NULL;
          }
        }
        else
        {
          RDCERR("Couldn't create upload buffer: %s", ToStr(hr).c_str());
          ret = false;
          mappedBuffer = NULL;
          ResourceContents = NULL;
        }
      }

      // need to create a dummy buffer to serialise into if anything went wrong
      if(ResourceContents == NULL && ContentsLength > 0)
        ResourceContents = dummy = new byte[(size_t)ContentsLength];
    }

    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("ResourceContents"_lit, ResourceContents, ContentsLength, SerialiserFlags::NoFlags);

    if(mappedBuffer)
      mappedBuffer->Unmap(0, NULL);

    SAFE_DELETE_ARRAY(dummy);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading() && mappedBuffer)
    {
      D3D12InitialContents initContents(D3D12InitialContents::Copy, type);
      initContents.resourceType = Resource_Resource;
      initContents.resource = mappedBuffer;

      D3D12_RESOURCE_DESC resDesc = liveRes->GetDesc();

      // for MSAA textures we upload to an MSAA texture here so we're ready to copy the image in
      // Apply_InitState
      if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resDesc.SampleDesc.Count > 1)
      {
        if(ContentsLength == 0)
        {
          // backwards compatibility - older captures will have no data for MSAA textures.
          initContents.resource = NULL;
          SAFE_RELEASE(mappedBuffer);
        }
        else
        {
          D3D12_HEAP_PROPERTIES heapProps = {};
          liveRes->GetHeapProperties(&heapProps, NULL);

          ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

          D3D12_RESOURCE_DESC arrayDesc = resDesc;
          arrayDesc.Alignment = 0;
          arrayDesc.DepthOrArraySize *= (UINT16)arrayDesc.SampleDesc.Count;
          arrayDesc.SampleDesc.Count = 1;
          arrayDesc.SampleDesc.Quality = 0;
          arrayDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

          bool isDepth = IsDepthFormat(resDesc.Format) ||
                         (resDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;

          D3D12_RESOURCE_DESC msaaDesc = resDesc;
          arrayDesc.Alignment = 0;
          arrayDesc.Flags = isDepth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                                    : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

          ID3D12Resource *arrayTex = NULL;
          HRESULT hr = m_Device->CreateCommittedResource(
              &heapProps, D3D12_HEAP_FLAG_NONE, &arrayDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
              __uuidof(ID3D12Resource), (void **)&arrayTex);
          if(FAILED(hr))
          {
            RDCERR("Couldn't create temporary array texture: %s", ToStr(hr).c_str());
            ret = false;
          }

          ID3D12Resource *msaaTex = NULL;
          hr = m_Device->CreateCommittedResource(
              &heapProps, D3D12_HEAP_FLAG_NONE, &msaaDesc,
              isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
              __uuidof(ID3D12Resource), (void **)&msaaTex);
          RDCASSERTEQUAL(hr, S_OK);
          if(FAILED(hr))
          {
            RDCERR("Couldn't create init state MSAA texture: %s", ToStr(hr).c_str());
            ret = false;
          }

          // copy buffer to array texture
          if(arrayTex)
          {
            uint32_t numSubresources = arrayDesc.DepthOrArraySize;

            {
              D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
              formatInfo.Format = arrayDesc.Format;
              m_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo,
                                            sizeof(formatInfo));

              UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

              numSubresources *= planes;
            }

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts =
                new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numSubresources];

            m_Device->GetCopyableFootprints(&arrayDesc, 0, numSubresources, 0, layouts, NULL, NULL,
                                            NULL);

            for(UINT i = 0; i < numSubresources; i++)
            {
              D3D12_TEXTURE_COPY_LOCATION dst, src;

              dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
              dst.pResource = Unwrap(arrayTex);
              dst.SubresourceIndex = i;

              src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
              src.pResource = Unwrap(mappedBuffer);
              src.PlacedFootprint = layouts[i];

              // copy buffer into this array slice
              list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

              // this slice now needs to be in shader-read to copy to the MSAA texture
              D3D12_RESOURCE_BARRIER b = {};
              b.Transition.pResource = Unwrap(arrayTex);
              b.Transition.Subresource = i;
              b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
              b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
              list->ResourceBarrier(1, &b);
            }

            delete[] layouts;
          }

          m_Device->CloseInitialStateList();
          m_Device->ExecuteLists(NULL, true);
          m_Device->FlushLists(true);

          // compact array into MSAA texture
          if(msaaTex && arrayTex)
            m_Device->GetDebugManager()->CopyArrayToTex2DMS(msaaTex, arrayTex, ~0U);

          // move MSAA texture permanently to copy source state
          if(msaaTex)
          {
            list = Unwrap(m_Device->GetInitialStateList());

            D3D12_RESOURCE_BARRIER b = {};
            b.Transition.pResource = Unwrap(msaaTex);
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore =
                isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            list->ResourceBarrier(1, &b);

            m_Device->CloseInitialStateList();
            m_Device->ExecuteLists(NULL, true);
            m_Device->FlushLists(true);
          }

          // subsequent copy comes from msaa texture
          initContents.resource = msaaTex;

          // we can release the buffer now, and the temporary array texture
          SAFE_RELEASE(mappedBuffer);
          SAFE_RELEASE(arrayTex);
        }
      }

      if(initContents.resource)
        SetInitialContents(id, initContents);
    }
  }
  else
  {
    RDCERR("Unexpected type needing an initial state serialised: %d", type);
    return false;
  }

  return ret;
}

template bool D3D12ResourceManager::Serialise_InitialState(ReadSerialiser &ser, ResourceId id,
                                                           D3D12ResourceRecord *record,
                                                           const D3D12InitialContents *initial);
template bool D3D12ResourceManager::Serialise_InitialState(WriteSerialiser &ser, ResourceId id,
                                                           D3D12ResourceRecord *record,
                                                           const D3D12InitialContents *initial);

void D3D12ResourceManager::Create_InitialState(ResourceId id, ID3D12DeviceChild *live, bool hasData)
{
  D3D12ResourceType type = IdentifyTypeByPtr(live);

  if(type == Resource_DescriptorHeap)
  {
    // set a NULL heap, if there are no initial contents for a descriptor heap we just leave
    // it all entirely undefined.
    SetInitialContents(id, D3D12InitialContents((ID3D12DescriptorHeap *)NULL));
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

void D3D12ResourceManager::Apply_InitialState(ID3D12DeviceChild *live,
                                              const D3D12InitialContents &data)
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
    if(data.tag == D3D12InitialContents::Copy)
    {
      ID3D12Resource *copyDst = Unwrap((ID3D12Resource *)live);

      if(!copyDst)
      {
        RDCERR("Missing copy destination in initial state apply (%p)", copyDst);
        return;
      }

      D3D12_HEAP_PROPERTIES heapProps = {};
      copyDst->GetHeapProperties(&heapProps, NULL);

      // if destination is on the upload heap, it's impossible to copy via the device,
      // so we have to CPU copy. We assume that we detected this case above and never uploaded a
      // device copy in the first place, and just kept the data CPU-side to source from.
      if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
      {
        byte *src = data.srcData, *dst = NULL;

        if(!src)
        {
          RDCERR("Doing CPU-side copy, don't have source data");
          return;
        }

        HRESULT hr = S_OK;

        if(copyDst->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
          hr = copyDst->Map(0, NULL, (void **)&dst);

          if(FAILED(hr))
          {
            RDCERR("Doing CPU-side copy, couldn't map destination: HRESULT: %s", ToStr(hr).c_str());
            dst = NULL;
          }

          if(src && dst)
            memcpy(dst, src, (size_t)copyDst->GetDesc().Width);

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
              RDCERR("Doing CPU-side copy, couldn't map source: HRESULT: %s", ToStr(hr).c_str());
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
              copyDst->Unmap(i, NULL);
          }

          delete[] layouts;
          delete[] numrows;
          delete[] rowsizes;
        }
      }
      else
      {
        ID3D12Resource *copySrc = (ID3D12Resource *)data.resource;

        if(!copySrc)
        {
          RDCERR("Missing copy source in initial state apply (%p)", copySrc);
          return;
        }

        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetInitialStateList());

        std::vector<D3D12_RESOURCE_BARRIER> barriers;

        const std::vector<D3D12_RESOURCE_STATES> &states =
            m_Device->GetSubresourceStates(GetResID(live));

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
          D3D12_RESOURCE_DESC srcDesc = copySrc->GetDesc();
          D3D12_RESOURCE_DESC dstDesc = copyDst->GetDesc();

          list->CopyBufferRegion(copyDst, 0, Unwrap(copySrc), 0,
                                 RDCMIN(srcDesc.Width, dstDesc.Width));
        }
        else if(copyDst->GetDesc().SampleDesc.Count > 1)
        {
          // MSAA texture was pre-uploaded and decoded, just copy the texture
          list->CopyResource(copyDst, Unwrap(copySrc));
        }
        else
        {
          D3D12_RESOURCE_DESC desc = copyDst->GetDesc();

          UINT numSubresources = desc.MipLevels;
          if(desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
            numSubresources *= desc.DepthOrArraySize;

          // we only accounted for planes in version 0x6, before then we only copied the first plane
          // so the buffer won't have enough data
          if(m_Device->GetLogVersion() >= 0x6)
          {
            D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
            formatInfo.Format = desc.Format;
            m_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

            UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

            numSubresources *= planes;
          }

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
            src.pResource = Unwrap(copySrc);
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
        m_Device->ExecuteLists(NULL, true);
        m_Device->FlushLists(true);
#endif
      }
    }
    else
    {
      RDCERR("Unexpected tag: %u", data.tag);
    }
  }
  else
  {
    RDCERR("Unexpected type needing an initial state created: %d", type);
  }
}
