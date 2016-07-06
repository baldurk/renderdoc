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

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface) WRAPPED_POOL_INST(CONCAT(Wrapped, iface));

ALL_D3D12_TYPES;

ResourceType IdentifyTypeByPtr(ID3D12DeviceChild *ptr)
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

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return (TrackedResource *)(WrappedID3D12GraphicsCommandList *)ptr;
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return (TrackedResource *)(WrappedID3D12CommandQueue *)ptr;

  RDCERR("Unknown type of ptr 0x%p", ptr);

  return NULL;
}

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
    return ResourceId();

  return res->GetResourceID();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr)
{
  if(ptr == NULL)
    return NULL;

  TrackedResource *res = GetTracked(ptr);

  if(res == NULL)
    return NULL;

  return res->GetResourceRecord();
}
