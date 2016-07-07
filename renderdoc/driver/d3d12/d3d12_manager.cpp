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

D3D12_CPU_DESCRIPTOR_HANDLE FromPortableHandle(D3D12ResourceManager *manager, PortableHandle handle)
{
  if(handle.heap == ResourceId())
    return D3D12_CPU_DESCRIPTOR_HANDLE();

  WrappedID3D12DescriptorHeap *heap = manager->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap);

  if(heap)
    return heap->GetCPU(handle.index);

  return D3D12_CPU_DESCRIPTOR_HANDLE();
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

    (void)desc;

    D3D12NOTIMP("resource init states");
    return true;
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
      ID3D12Resource *r = (ID3D12Resource *)GetCurrentResource(id);

      D3D12_RESOURCE_DESC desc = r->GetDesc();

      (void)desc;

      D3D12NOTIMP("resource init states");
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

      if(FAILED(hr))
      {
        RDCERR("Failed to create CPU descriptor heap for initial state: 0x%08x", hr);
        return false;
      }

      D3D12_CPU_DESCRIPTOR_HANDLE handle = copyheap->GetCPUDescriptorHandleForHeapStart();

      UINT increment = m_Device->GetDescriptorHandleIncrementSize(desc.Type);

      for(uint32_t i = 0; i < numElems; i++)
      {
        descs[i].Create(m_Device->GetReal(), handle);

        handle.ptr += increment;
      }

      SAFE_DELETE_ARRAY(descs);

      SetInitialContents(id, D3D12ResourceManager::InitialContentData(copyheap, 0, NULL));
    }
    else if(type == Resource_Resource)
    {
      D3D12NOTIMP("resource init states");
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
    ID3D12DescriptorHeap *dstheap = Unwrap((ID3D12DescriptorHeap *)live);
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
    D3D12NOTIMP("resource init states");
  }
  else
  {
    RDCUNIMPLEMENTED("init states");
  }
}
