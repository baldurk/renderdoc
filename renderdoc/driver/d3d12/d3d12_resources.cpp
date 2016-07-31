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

#include "d3d12_resources.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"

std::vector<WrappedID3D12Resource::AddressRange> WrappedID3D12Resource::m_Addresses;
std::map<ResourceId, WrappedID3D12Resource *> WrappedID3D12Resource::m_List;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface) WRAPPED_POOL_INST(CONCAT(Wrapped, iface));

ALL_D3D12_TYPES;

D3D12ResourceType IdentifyTypeByPtr(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return Resource_Unknown;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return UnwrapHelper<iface>::GetTypeEnum();

  ALL_D3D12_TYPES;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return Resource_GraphicsCommandList;
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return Resource_CommandQueue;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return Resource_Unknown;
}

TrackedResource *GetTracked(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (TrackedResource *)GetWrapped((iface *)ptr);

  ALL_D3D12_TYPES;

  return NULL;
}

template <>
ID3D12DeviceChild *Unwrap(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return (ID3D12DeviceChild *)GetWrapped((iface *)ptr)->GetReal();

  ALL_D3D12_TYPES;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return (ID3D12DeviceChild *)(((WrappedID3D12GraphicsCommandList *)ptr)->GetReal());
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return (ID3D12DeviceChild *)(((WrappedID3D12CommandQueue *)ptr)->GetReal());

  RDCERR("Unknown type of ptr 0x%p", ptr);

  return NULL;
}

template <>
ResourceId GetResID(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return ResourceId();

  TrackedResource *res = GetTracked(ptr);

  if(res == NULL)
  {
    if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
      return ((WrappedID3D12GraphicsCommandList *)ptr)->GetResourceID();
    if(WrappedID3D12CommandQueue::IsAlloc(ptr))
      return ((WrappedID3D12CommandQueue *)ptr)->GetResourceID();

    RDCERR("Unknown type of ptr 0x%p", ptr);

    return ResourceId();
  }

  return res->GetResourceID();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

  TrackedResource *res = GetTracked(ptr);

  if(res == NULL)
  {
    if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
      return ((WrappedID3D12GraphicsCommandList *)ptr)->GetResourceRecord();
    if(WrappedID3D12CommandQueue::IsAlloc(ptr))
      return ((WrappedID3D12CommandQueue *)ptr)->GetResourceRecord();

    RDCERR("Unknown type of ptr 0x%p", ptr);

    return NULL;
  }

  return res->GetResourceRecord();
}

WrappedID3D12DescriptorHeap::WrappedID3D12DescriptorHeap(ID3D12DescriptorHeap *real,
                                                         WrappedID3D12Device *device,
                                                         const D3D12_DESCRIPTOR_HEAP_DESC &desc)
    : WrappedDeviceChild12(real, device)
{
  realCPUBase = real->GetCPUDescriptorHandleForHeapStart();
  realGPUBase = real->GetGPUDescriptorHandleForHeapStart();

  increment = device->GetUnwrappedDescriptorIncrement(desc.Type);
  numDescriptors = desc.NumDescriptors;

  descriptors = new D3D12Descriptor[numDescriptors];

  RDCEraseMem(descriptors, sizeof(D3D12Descriptor) * numDescriptors);
  for(UINT i = 0; i < numDescriptors; i++)
  {
    // only need to set this once, it's aliased between samp and nonsamp
    descriptors[i].samp.heap = this;
    descriptors[i].samp.idx = i;

    // initially descriptors are undefined. This way we just fill them with
    // some null SRV descriptor so it's safe to copy around etc but is no
    // less undefined for the application to use
    descriptors[i].nonsamp.type = D3D12Descriptor::TypeUndefined;
  }
}

WrappedID3D12DescriptorHeap::~WrappedID3D12DescriptorHeap()
{
  Shutdown();
  SAFE_DELETE_ARRAY(descriptors);
}
