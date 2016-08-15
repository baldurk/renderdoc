/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "d3d12_manager.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

void D3D12Descriptor::Init(const D3D12_SAMPLER_DESC *pDesc)
{
  if(pDesc)
    samp.desc = *pDesc;
  else
    RDCEraseEl(samp.desc);
}

void D3D12Descriptor::Init(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc)
{
  nonsamp.type = TypeCBV;
  nonsamp.resource = NULL;
  if(pDesc)
    nonsamp.cbv = *pDesc;
  else
    RDCEraseEl(nonsamp.cbv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc)
{
  nonsamp.type = TypeSRV;
  nonsamp.resource = pResource;
  if(pDesc)
    nonsamp.srv = *pDesc;
  else
    RDCEraseEl(nonsamp.srv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, ID3D12Resource *pCounterResource,
                           const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc)
{
  nonsamp.type = TypeUAV;
  nonsamp.resource = pResource;
  nonsamp.uav.counterResource = pCounterResource;
  if(pDesc)
    nonsamp.uav.desc.Init(*pDesc);
  else
    RDCEraseEl(nonsamp.uav.desc);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc)
{
  nonsamp.type = TypeRTV;
  nonsamp.resource = pResource;
  if(pDesc)
    nonsamp.rtv = *pDesc;
  else
    RDCEraseEl(nonsamp.rtv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc)
{
  nonsamp.type = TypeDSV;
  nonsamp.resource = pResource;
  if(pDesc)
    nonsamp.dsv = *pDesc;
  else
    RDCEraseEl(nonsamp.dsv);
}

void D3D12Descriptor::Create(ID3D12Device *dev, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  D3D12Descriptor::DescriptorType type = GetType();

  switch(type)
  {
    case D3D12Descriptor::TypeSampler:
    {
      dev->CreateSampler(&samp.desc, handle);
      break;
    }
    case D3D12Descriptor::TypeCBV:
    {
      dev->CreateConstantBufferView(&nonsamp.cbv, handle);
      break;
    }
    case D3D12Descriptor::TypeSRV:
    {
      dev->CreateShaderResourceView(nonsamp.resource, &nonsamp.srv, handle);
      break;
    }
    case D3D12Descriptor::TypeRTV:
    {
      dev->CreateRenderTargetView(nonsamp.resource, &nonsamp.rtv, handle);
      break;
    }
    case D3D12Descriptor::TypeDSV:
    {
      dev->CreateDepthStencilView(nonsamp.resource, &nonsamp.dsv, handle);
      break;
    }
    case D3D12Descriptor::TypeUAV:
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = nonsamp.uav.desc.AsDesc();
      dev->CreateUnorderedAccessView(nonsamp.resource, nonsamp.uav.counterResource, &desc, handle);
      break;
    }
    case D3D12Descriptor::TypeUndefined:
    {
      // initially descriptors are undefined. This way we just init with
      // a null SRV descriptor so it's valid to copy around etc but is no
      // less undefined for the application to use
      D3D12_SHADER_RESOURCE_VIEW_DESC desc;
      desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.Texture2D.MipLevels = 1;
      desc.Texture2D.MostDetailedMip = 0;
      desc.Texture2D.PlaneSlice = 0;
      desc.Texture2D.ResourceMinLODClamp = 0.0f;

      dev->CreateShaderResourceView(NULL, &desc, handle);
      break;
    }
  }
}

void D3D12Descriptor::CopyFrom(const D3D12Descriptor &src)
{
  // save these so we can do a straight copy then restore them
  WrappedID3D12DescriptorHeap *heap = samp.heap;
  uint32_t index = samp.idx;

  *this = src;

  samp.heap = heap;
  samp.idx = index;
}

D3D12_CPU_DESCRIPTOR_HANDLE Unwrap(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return handle;

  D3D12Descriptor *desc = GetWrapped(handle);
  return desc->samp.heap->GetCPU(desc->samp.idx);
}

D3D12_GPU_DESCRIPTOR_HANDLE Unwrap(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return handle;

  D3D12Descriptor *desc = GetWrapped(handle);
  return desc->samp.heap->GetGPU(desc->samp.idx);
}

PortableHandle ToPortableHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return PortableHandle(0);

  D3D12Descriptor *desc = GetWrapped(handle);

  return PortableHandle(GetResID(desc->samp.heap), desc->samp.idx);
}

PortableHandle ToPortableHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return PortableHandle(0);

  D3D12Descriptor *desc = GetWrapped(handle);

  return PortableHandle(GetResID(desc->samp.heap), desc->samp.idx);
}

D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromPortableHandle(D3D12ResourceManager *manager,
                                                        PortableHandle handle)
{
  if(handle.heap == ResourceId())
    return D3D12_CPU_DESCRIPTOR_HANDLE();

  WrappedID3D12DescriptorHeap *heap = manager->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap);

  if(heap)
    return heap->GetCPU(handle.index);

  return D3D12_CPU_DESCRIPTOR_HANDLE();
}

D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromPortableHandle(D3D12ResourceManager *manager,
                                                        PortableHandle handle)
{
  if(handle.heap == ResourceId())
    return D3D12_GPU_DESCRIPTOR_HANDLE();

  WrappedID3D12DescriptorHeap *heap = manager->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap);

  if(heap)
    return heap->GetGPU(handle.index);

  return D3D12_GPU_DESCRIPTOR_HANDLE();
}

// debugging logging for barriers
#if 1
#define BARRIER_DBG RDCLOG
#define BARRIER_ASSERT RDCASSERTMSG
#else
#define BARRIER_DBG(...)
#define BARRIER_ASSERT(...)
#endif

void D3D12ResourceManager::ApplyBarriers(vector<D3D12_RESOURCE_BARRIER> &barriers,
                                         map<ResourceId, SubresourceStateVector> &states)
{
  for(size_t b = 0; b < barriers.size(); b++)
  {
    D3D12_RESOURCE_TRANSITION_BARRIER &trans = barriers[b].Transition;
    ResourceId id = GetResID(trans.pResource);
    SubresourceStateVector &st = states[id];

    // skip non-transitions, or begin-halves of transitions
    if(barriers[b].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
       (barriers[b].Flags & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY))
      continue;

    if(trans.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
      for(size_t i = 0; i < st.size(); i++)
      {
        BARRIER_ASSERT("Mismatching before state", st[i] == trans.StateBefore, st[i],
                       trans.StateBefore, i);
        st[i] = trans.StateAfter;
      }
    }
    else
    {
      BARRIER_ASSERT("Mismatching before state", st[trans.Subresource] == trans.StateBefore,
                     st[trans.Subresource], trans.StateBefore, trans.Subresource);
      st[trans.Subresource] = trans.StateAfter;
    }
  }
}

void D3D12ResourceManager::SerialiseResourceStates(vector<D3D12_RESOURCE_BARRIER> &barriers,
                                                   map<ResourceId, SubresourceStateVector> &states)
{
  SERIALISE_ELEMENT(uint32_t, NumMems, (uint32_t)states.size());

  auto srcit = states.begin();

  for(uint32_t i = 0; i < NumMems; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id, srcit->first);
    SERIALISE_ELEMENT(uint32_t, NumStates, (uint32_t)srcit->second.size());

    ResourceId liveid;
    if(m_State < WRITING && HasLiveResource(id))
      liveid = GetLiveID(id);

    for(uint32_t m = 0; m < NumStates; m++)
    {
      SERIALISE_ELEMENT(D3D12_RESOURCE_STATES, state, srcit->second[m]);

      if(m_State < WRITING && liveid != ResourceId() && srcit != states.end())
      {
        D3D12_RESOURCE_BARRIER b;
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource = (ID3D12Resource *)GetCurrentResource(liveid);
        b.Transition.Subresource = m;
        b.Transition.StateBefore = states[liveid][m];
        b.Transition.StateAfter = state;

        barriers.push_back(b);
      }
    }

    if(m_State >= WRITING)
      srcit++;
  }

  // erase any do-nothing barriers
  for(auto it = barriers.begin(); it != barriers.end();)
  {
    if(it->Transition.StateBefore == it->Transition.StateAfter)
      it = barriers.erase(it);
    else
      ++it;
  }

  ApplyBarriers(barriers, states);
}

bool D3D12ResourceManager::SerialisableResource(ResourceId id, D3D12ResourceRecord *record)
{
  if(record->SpecialResource)
    return false;

  return true;
}

ResourceId D3D12ResourceManager::GetID(ID3D12DeviceChild *res)
{
  return GetResID(res);
}

bool D3D12ResourceManager::ResourceTypeRelease(ID3D12DeviceChild *res)
{
  if(res)
    res->Release();

  return true;
}

bool D3D12ResourceManager::Force_InitialState(ID3D12DeviceChild *res)
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
        (D3D12Descriptor *)Serialiser::AllocAlignedBuffer(sizeof(D3D12Descriptor) * numElems);

    memcpy(descs, heap->GetDescriptors(), sizeof(D3D12Descriptor) * numElems);

    SetInitialContents(heap->GetResourceID(),
                       D3D12ResourceManager::InitialContentData(NULL, numElems, (byte *)descs));
    return true;
  }
  else if(type == Resource_Resource)
  {
    ID3D12Resource *r = (ID3D12Resource *)res;

    D3D12_RESOURCE_DESC desc = r->GetDesc();

    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      D3D12_HEAP_PROPERTIES heapProps =
          m_Device->GetReal()->GetCustomHeapProperties(1, D3D12_HEAP_TYPE_READBACK);

      ID3D12Resource *copyDst = NULL;
      HRESULT hr = m_Device->GetReal()->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
          __uuidof(ID3D12Resource), (void **)&copyDst);

      if(SUCCEEDED(hr))
      {
        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetNewList());

        list->CopyResource(copyDst, Unwrap(r));

        list->Close();
      }
      else
      {
        RDCERR("Couldn't create readback buffer: 0x%08x", hr);
      }

      SetInitialContents(GetResID(r), D3D12ResourceManager::InitialContentData(copyDst, 0, NULL));
      return true;
    }
    else
    {
      D3D12_HEAP_PROPERTIES heapProps =
          m_Device->GetReal()->GetCustomHeapProperties(1, D3D12_HEAP_TYPE_READBACK);

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

      if(SUCCEEDED(hr))
      {
        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetNewList());

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
          barrier.Transition.pResource = Unwrap(r);
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
          src.pResource = Unwrap(r);
          src.SubresourceIndex = i;

          dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          dst.pResource = copyDst;
          dst.PlacedFootprint = layouts[i];

          list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }

        // transition back
        for(size_t i = 0; i < barriers.size(); i++)
          std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

        list->Close();
      }
      else
      {
        RDCERR("Couldn't create readback buffer: 0x%08x", hr);
      }

      SAFE_DELETE_ARRAY(layouts);

      SetInitialContents(GetResID(r), D3D12ResourceManager::InitialContentData(copyDst, 0, NULL));
      return true;
    }
  }

  RDCUNIMPLEMENTED("init states");
  return false;
}

bool D3D12ResourceManager::Serialise_InitialState(ResourceId resid, ID3D12DeviceChild *)
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

      byte dummy[4] = {};
      byte *ptr = NULL;
      size_t size = 0;

      HRESULT hr = E_NOINTERFACE;

      if(copiedBuffer)
      {
        hr = copiedBuffer->Map(0, NULL, (void **)&ptr);
        size = (size_t)copiedBuffer->GetDesc().Width;
      }

      if(FAILED(hr) || ptr == NULL)
      {
        size = 4;
        ptr = dummy;

        RDCERR("Failed to map buffer for readback! 0x%08x", hr);
      }

      m_pSerialiser->Serialise("NumBytes", size);
      m_pSerialiser->SerialiseBuffer("BufferData", ptr, size);

      if(SUCCEEDED(hr) && ptr)
        copiedBuffer->Unmap(0, NULL);

      return true;
    }
    else
    {
      RDCUNIMPLEMENTED("init states");
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

      copyheap = new WrappedID3D12DescriptorHeap(copyheap, m_Device, desc);

      if(FAILED(hr))
      {
        RDCERR("Failed to create CPU descriptor heap for initial state: 0x%08x", hr);
        return false;
      }

      D3D12_CPU_DESCRIPTOR_HANDLE handle = copyheap->GetCPUDescriptorHandleForHeapStart();

      UINT increment = m_Device->GetDescriptorHandleIncrementSize(desc.Type);

      for(uint32_t i = 0; i < numElems; i++)
      {
        descs[i].Create(m_Device, handle);

        handle.ptr += increment;
      }

      SAFE_DELETE_ARRAY(descs);

      SetInitialContents(id, D3D12ResourceManager::InitialContentData(copyheap, 0, NULL));
    }
    else if(type == Resource_Resource)
    {
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

      SetInitialContents(id, D3D12ResourceManager::InitialContentData(copySrc, 1, NULL));

      return true;
    }
    else
    {
      RDCUNIMPLEMENTED("init states");
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
    SetInitialContents(id, D3D12ResourceManager::InitialContentData(NULL, 1, NULL));
  }
  else if(type == Resource_Resource)
  {
    D3D12NOTIMP("resource init states");

    // not handling any missing states at the moment
  }
  else
  {
    RDCUNIMPLEMENTED("init states");
  }
}

void D3D12ResourceManager::Apply_InitialState(ID3D12DeviceChild *live, InitialContentData data)
{
  D3D12ResourceType type = IdentifyTypeByPtr(live);

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
        ID3D12GraphicsCommandList *list = Unwrap(m_Device->GetNewList());

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
          barrier.Transition.Subresource = 0;
          barrier.Transition.StateBefore = m_Device->GetSubresourceStates(GetResID(live))[0];
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

            list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
          }

          delete[] layouts;
        }

        // transition back to whatever it was before
        for(size_t i = 0; i < barriers.size(); i++)
          std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

        if(!barriers.empty())
          list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

        list->Close();
      }
    }
    else
    {
      D3D12NOTIMP("resource init states");
    }
  }
  else
  {
    RDCUNIMPLEMENTED("init states");
  }
}
