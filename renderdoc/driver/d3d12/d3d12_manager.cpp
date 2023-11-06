/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include <algorithm>
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

void D3D12Descriptor::Init(const D3D12_SAMPLER_DESC2 *pDesc)
{
  if(pDesc)
    data.samp.desc.Init(*pDesc);
  else
    RDCEraseEl(data.samp.desc);
}

void D3D12Descriptor::Init(const D3D12_SAMPLER_DESC *pDesc)
{
  if(!pDesc)
  {
    RDCEraseEl(data.samp.desc);
    return;
  }

  D3D12_SAMPLER_DESC2 desc;
  desc.Filter = pDesc->Filter;
  desc.Filter = pDesc->Filter;
  desc.AddressU = pDesc->AddressU;
  desc.AddressV = pDesc->AddressV;
  desc.AddressW = pDesc->AddressW;
  desc.ComparisonFunc = pDesc->ComparisonFunc;
  desc.MipLODBias = pDesc->MipLODBias;
  desc.MaxAnisotropy = pDesc->MaxAnisotropy;
  memcpy(desc.UintBorderColor, pDesc->BorderColor, sizeof(desc.UintBorderColor));
  desc.MinLOD = pDesc->MinLOD;
  desc.MaxLOD = pDesc->MaxLOD;
  desc.Flags = D3D12_SAMPLER_FLAG_NONE;

  data.samp.desc.Init(desc);
}

void D3D12Descriptor::Init(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc)
{
  data.nonsamp.type = D3D12DescriptorType::CBV;
  data.nonsamp.resource = ResourceId();
  if(pDesc)
    data.nonsamp.cbv = *pDesc;
  else
    RDCEraseEl(data.nonsamp.cbv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc)
{
  data.nonsamp.type = D3D12DescriptorType::SRV;
  data.nonsamp.resource = GetResID(pResource);
  if(pDesc)
    data.nonsamp.srv.Init(*pDesc);
  else
    RDCEraseEl(data.nonsamp.srv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, ID3D12Resource *pCounterResource,
                           const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc)
{
  data.nonsamp.type = D3D12DescriptorType::UAV;
  data.nonsamp.resource = GetResID(pResource);
  data.nonsamp.counterResource = GetResID(pCounterResource);
  if(pDesc)
    data.nonsamp.uav.Init(*pDesc);
  else
    RDCEraseEl(data.nonsamp.uav);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc)
{
  data.nonsamp.type = D3D12DescriptorType::RTV;
  data.nonsamp.resource = GetResID(pResource);
  if(pDesc)
    data.nonsamp.rtv = *pDesc;
  else
    RDCEraseEl(data.nonsamp.rtv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc)
{
  data.nonsamp.type = D3D12DescriptorType::DSV;
  data.nonsamp.resource = GetResID(pResource);
  if(pDesc)
    data.nonsamp.dsv = *pDesc;
  else
    RDCEraseEl(data.nonsamp.dsv);
}

// these are used to create NULL descriptors where necessary
static D3D12_SHADER_RESOURCE_VIEW_DESC *defaultSRV()
{
  static D3D12_SHADER_RESOURCE_VIEW_DESC ret = {};
  ret.Format = DXGI_FORMAT_R8_UNORM;
  ret.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  ret.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  ret.Texture2D.MipLevels = 1;
  return &ret;
}

static D3D12_RENDER_TARGET_VIEW_DESC *defaultRTV()
{
  static D3D12_RENDER_TARGET_VIEW_DESC ret = {};
  ret.Format = DXGI_FORMAT_R8_UNORM;
  ret.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  return &ret;
}

static D3D12_DEPTH_STENCIL_VIEW_DESC *defaultDSV()
{
  static D3D12_DEPTH_STENCIL_VIEW_DESC ret = {};
  ret.Format = DXGI_FORMAT_D16_UNORM;
  ret.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  return &ret;
}

static D3D12_UNORDERED_ACCESS_VIEW_DESC *defaultUAV()
{
  static D3D12_UNORDERED_ACCESS_VIEW_DESC ret = {};
  ret.Format = DXGI_FORMAT_R8_UNORM;
  ret.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
  return &ret;
}

void D3D12Descriptor::Create(D3D12_DESCRIPTOR_HEAP_TYPE heapType, WrappedID3D12Device *dev,
                             D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  D3D12DescriptorType type = GetType();

  ID3D12Resource *res = NULL;
  ID3D12Resource *countRes = NULL;

  if(type != D3D12DescriptorType::Sampler && type != D3D12DescriptorType::CBV)
    res = dev->GetResourceManager()->GetCurrentAs<ID3D12Resource>(data.nonsamp.resource);

  // don't create a UAV with a counter resource but no main resource. This is fine because
  // if the main resource wasn't present in the capture, this UAV isn't present - the counter
  // must have been included for some other reference.
  if(type == D3D12DescriptorType::UAV && res)
    countRes = dev->GetResourceManager()->GetCurrentAs<ID3D12Resource>(data.nonsamp.counterResource);

  switch(type)
  {
    case D3D12DescriptorType::Sampler:
    {
      D3D12_SAMPLER_DESC2 desc = data.samp.desc.AsDesc();
      if(desc.Flags == D3D12_SAMPLER_FLAG_NONE)
      {
        D3D12_SAMPLER_DESC desc1;
        memcpy(&desc1, &desc, sizeof(desc1));
        dev->CreateSampler(&desc1, handle);
      }
      else
      {
        dev->CreateSampler2(&desc, handle);
      }
      break;
    }
    case D3D12DescriptorType::CBV:
    {
      if(data.nonsamp.cbv.BufferLocation != 0)
        dev->CreateConstantBufferView(&data.nonsamp.cbv, handle);
      else
        dev->CreateShaderResourceView(NULL, defaultSRV(), handle);
      break;
    }
    case D3D12DescriptorType::SRV:
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = data.nonsamp.srv.AsDesc();

      D3D12_SHADER_RESOURCE_VIEW_DESC *desc = &srvdesc;
      if(desc->ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      {
        desc = res ? NULL : defaultSRV();

        const std::map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(res));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_SHADER_RESOURCE_VIEW_DESC bbDesc = {};
          bbDesc.Format = it->second;
          bbDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
          bbDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
          bbDesc.Texture2D.MipLevels = 1;
          dev->CreateShaderResourceView(res, &bbDesc, handle);
          return;
        }
      }
      else if(!res)
      {
        // if we don't have a resource (which is possible if the descriptor is unused or invalidated
        // by referring to a resource that was deleted), use a default descriptor
        desc = defaultSRV();
      }
      else if(desc->Format == DXGI_FORMAT_UNKNOWN)
      {
        const std::map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(res));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_SHADER_RESOURCE_VIEW_DESC bbDesc = *desc;
          bbDesc.Format = it->second;
          dev->CreateShaderResourceView(res, &bbDesc, handle);
          return;
        }
      }

      D3D12_SHADER_RESOURCE_VIEW_DESC planeDesc;
      // ensure that multi-plane formats have a valid plane slice specified. This shouldn't be
      // possible as it should be the application's responsibility to be valid too, but we fix it up
      // here anyway.
      if(res && desc)
      {
        D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
        formatInfo.Format = desc->Format;
        dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

        // if this format is multi-plane
        if(formatInfo.PlaneCount > 1)
        {
          planeDesc = *desc;
          desc = &planeDesc;

          // detect formats that only read plane 1 and set the planeslice to 1
          if(desc->Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
             desc->Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT)
          {
            switch(planeDesc.ViewDimension)
            {
              case D3D12_SRV_DIMENSION_TEXTURE2D: planeDesc.Texture2D.PlaneSlice = 1; break;
              case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                planeDesc.Texture2DArray.PlaneSlice = 1;
                break;
              default: break;
            }
          }
          else
          {
            // otherwise set it to 0
            switch(planeDesc.ViewDimension)
            {
              case D3D12_SRV_DIMENSION_TEXTURE2D: planeDesc.Texture2D.PlaneSlice = 0; break;
              case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                planeDesc.Texture2DArray.PlaneSlice = 0;
                break;
              default: break;
            }
          }
        }
      }

      dev->CreateShaderResourceView(res, desc, handle);
      break;
    }
    case D3D12DescriptorType::RTV:
    {
      D3D12_RENDER_TARGET_VIEW_DESC *desc = &data.nonsamp.rtv;
      if(desc->ViewDimension == D3D12_RTV_DIMENSION_UNKNOWN)
      {
        desc = res ? NULL : defaultRTV();

        const std::map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(res));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_RENDER_TARGET_VIEW_DESC bbDesc = {};
          bbDesc.Format = it->second;
          bbDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
          dev->CreateRenderTargetView(res, &bbDesc, handle);
          return;
        }
      }
      else if(!res)
      {
        // if we don't have a resource (which is possible if the descriptor is unused or invalidated
        // by referring to a resource that was deleted), use a default descriptor
        desc = defaultRTV();
      }
      else if(desc->Format == DXGI_FORMAT_UNKNOWN)
      {
        const std::map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(res));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_RENDER_TARGET_VIEW_DESC bbDesc = *desc;
          bbDesc.Format = it->second;
          dev->CreateRenderTargetView(res, &bbDesc, handle);
          return;
        }
      }

      D3D12_RENDER_TARGET_VIEW_DESC planeDesc;
      // ensure that multi-plane formats have a valid plane slice specified. This shouldn't be
      // possible as it should be the application's responsibility to be valid too, but we fix it up
      // here anyway.
      if(res && desc)
      {
        D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
        formatInfo.Format = desc->Format;
        dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

        // if this format is multi-plane
        if(formatInfo.PlaneCount > 1)
        {
          planeDesc = *desc;
          desc = &planeDesc;

          // detect formats that only read plane 1 and set the planeslice to 1
          if(desc->Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
             desc->Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT)
          {
            switch(planeDesc.ViewDimension)
            {
              case D3D12_RTV_DIMENSION_TEXTURE2D: planeDesc.Texture2D.PlaneSlice = 1; break;
              case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
                planeDesc.Texture2DArray.PlaneSlice = 1;
                break;
              default: break;
            }
          }
          else
          {
            // otherwise set it to 0
            switch(planeDesc.ViewDimension)
            {
              case D3D12_RTV_DIMENSION_TEXTURE2D: planeDesc.Texture2D.PlaneSlice = 0; break;
              case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
                planeDesc.Texture2DArray.PlaneSlice = 0;
                break;
              default: break;
            }
          }
        }
      }

      dev->CreateRenderTargetView(res, desc, handle);
      break;
    }
    case D3D12DescriptorType::DSV:
    {
      D3D12_DEPTH_STENCIL_VIEW_DESC *desc = &data.nonsamp.dsv;
      if(desc->ViewDimension == D3D12_DSV_DIMENSION_UNKNOWN)
      {
        desc = res ? NULL : defaultDSV();
      }
      else if(!res)
      {
        // if we don't have a resource (which is possible if the descriptor is unused or invalidated
        // by referring to a resource that was deleted), use a default descriptor
        desc = defaultDSV();
      }

      dev->CreateDepthStencilView(res, desc, handle);
      break;
    }
    case D3D12DescriptorType::UAV:
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = data.nonsamp.uav.AsDesc();

      D3D12_UNORDERED_ACCESS_VIEW_DESC *desc = &uavdesc;
      if(uavdesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
      {
        desc = res ? NULL : defaultUAV();

        const std::map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(res));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_UNORDERED_ACCESS_VIEW_DESC bbDesc = {};
          bbDesc.Format = it->second;
          bbDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
          dev->CreateUnorderedAccessView(res, NULL, &bbDesc, handle);
          return;
        }
      }
      else if(!res)
      {
        // if we don't have a resource (which is possible if the descriptor is unused), use a
        // default descriptor
        desc = defaultUAV();
      }
      else if(desc->Format == DXGI_FORMAT_UNKNOWN)
      {
        const std::map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(res));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_UNORDERED_ACCESS_VIEW_DESC bbDesc = *desc;
          bbDesc.Format = it->second;
          dev->CreateUnorderedAccessView(res, NULL, &bbDesc, handle);
          return;
        }
      }

      if(countRes == NULL && desc && desc->ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
        desc->Buffer.CounterOffsetInBytes = 0;

      D3D12_UNORDERED_ACCESS_VIEW_DESC planeDesc;
      // ensure that multi-plane formats have a valid plane slice specified. This shouldn't be
      // possible as it should be the application's responsibility to be valid too, but we fix it up
      // here anyway.
      if(res && desc)
      {
        D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
        formatInfo.Format = desc->Format;
        dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

        // if this format is multi-plane
        if(formatInfo.PlaneCount > 1)
        {
          planeDesc = *desc;
          desc = &planeDesc;

          // detect formats that only read plane 1 and set the planeslice to 1
          if(desc->Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
             desc->Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT)
          {
            switch(planeDesc.ViewDimension)
            {
              case D3D12_UAV_DIMENSION_TEXTURE2D: planeDesc.Texture2D.PlaneSlice = 1; break;
              case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
                planeDesc.Texture2DArray.PlaneSlice = 1;
                break;
              default: break;
            }
          }
          else
          {
            // otherwise set it to 0
            switch(planeDesc.ViewDimension)
            {
              case D3D12_UAV_DIMENSION_TEXTURE2D: planeDesc.Texture2D.PlaneSlice = 0; break;
              case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
                planeDesc.Texture2DArray.PlaneSlice = 0;
                break;
              default: break;
            }
          }
        }
      }

      dev->CreateUnorderedAccessView(res, countRes, desc, handle);
      break;
    }
    case D3D12DescriptorType::Undefined:
    {
      // initially descriptors are undefined. This way we just init with
      // a null descriptor so it's valid to copy around etc but is no
      // less undefined for the application to use

      if(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
        dev->CreateShaderResourceView(NULL, defaultSRV(), handle);
      else if(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        dev->CreateDepthStencilView(NULL, defaultDSV(), handle);
      else if(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
        dev->CreateRenderTargetView(NULL, defaultRTV(), handle);

      break;
    }
  }
}

void D3D12Descriptor::CopyFrom(const D3D12Descriptor &src)
{
  // save these so we can do a straight copy then restore them
  WrappedID3D12DescriptorHeap *heap = data.samp.heap;
  uint32_t index = data.samp.idx;

  *this = src;

  data.samp.heap = heap;
  data.samp.idx = index;
}

void D3D12Descriptor::GetRefIDs(ResourceId &id, ResourceId &id2, FrameRefType &ref)
{
  id = ResourceId();
  id2 = ResourceId();
  ref = eFrameRef_Read;

  switch(GetType())
  {
    case D3D12DescriptorType::Undefined:
    case D3D12DescriptorType::Sampler:
      // nothing to do - no resource here
      break;
    case D3D12DescriptorType::CBV:
      id = WrappedID3D12Resource::GetResIDFromAddr(data.nonsamp.cbv.BufferLocation);
      break;
    case D3D12DescriptorType::SRV: id = data.nonsamp.resource; break;
    case D3D12DescriptorType::UAV: id2 = data.nonsamp.counterResource; DELIBERATE_FALLTHROUGH();
    case D3D12DescriptorType::RTV:
    case D3D12DescriptorType::DSV:
      ref = eFrameRef_PartialWrite;
      id = data.nonsamp.resource;
      break;
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Descriptor::GetCPU() const
{
  return data.samp.heap->GetCPU(data.samp.idx);
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12Descriptor::GetGPU() const
{
  return data.samp.heap->GetGPU(data.samp.idx);
}

PortableHandle D3D12Descriptor::GetPortableHandle() const
{
  return PortableHandle(GetResID(data.samp.heap), data.samp.idx);
}

ResourceId D3D12Descriptor::GetHeapResourceId() const
{
  return GetResID(data.samp.heap);
}

ResourceId D3D12Descriptor::GetResResourceId() const
{
  return data.nonsamp.resource;
}

ResourceId D3D12Descriptor::GetCounterResourceId() const
{
  return data.nonsamp.counterResource;
}

D3D12_CPU_DESCRIPTOR_HANDLE UnwrapCPU(D3D12Descriptor *handle)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = {};
  if(handle == NULL)
    return ret;

  return handle->GetCPU();
}

D3D12_GPU_DESCRIPTOR_HANDLE UnwrapGPU(D3D12Descriptor *handle)
{
  D3D12_GPU_DESCRIPTOR_HANDLE ret = {};
  if(handle == NULL)
    return ret;

  return handle->GetGPU();
}

D3D12_CPU_DESCRIPTOR_HANDLE Unwrap(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return handle;

  return UnwrapCPU(GetWrapped(handle));
}

D3D12_GPU_DESCRIPTOR_HANDLE Unwrap(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return handle;

  return UnwrapGPU(GetWrapped(handle));
}

PortableHandle ToPortableHandle(D3D12Descriptor *desc)
{
  if(desc == NULL)
    return PortableHandle(0);

  return desc->GetPortableHandle();
}

PortableHandle ToPortableHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return PortableHandle(0);

  return ToPortableHandle(GetWrapped(handle));
}

PortableHandle ToPortableHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return PortableHandle(0);

  return ToPortableHandle(GetWrapped(handle));
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

D3D12Descriptor *DescriptorFromPortableHandle(D3D12ResourceManager *manager, PortableHandle handle)
{
  if(handle.heap == ResourceId())
    return NULL;

  WrappedID3D12DescriptorHeap *heap =
      manager->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap, true);

  if(heap)
    return heap->GetDescriptors() + handle.index;

  return NULL;
}

// debugging logging for barriers
#if 0
#define BARRIER_DBG RDCLOG
#define BARRIER_ASSERT RDCASSERTMSG
#else
#define BARRIER_DBG(...)
#define BARRIER_ASSERT(...)
#endif

void D3D12ResourceManager::ApplyBarriers(BarrierSet &barriers,
                                         std::map<ResourceId, SubresourceStateVector> &states)
{
  for(size_t b = 0; b < barriers.barriers.size(); b++)
  {
    const D3D12_RESOURCE_TRANSITION_BARRIER &trans = barriers.barriers[b].Transition;
    ResourceId id = GetResID(trans.pResource);

    auto it = states.find(id);
    if(it == states.end())
      return;

    SubresourceStateVector &st = it->second;

    // skip non-transitions, or begin-halves of transitions
    if(barriers.barriers[b].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
       (barriers.barriers[b].Flags & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY))
      continue;

    size_t first = trans.Subresource;
    if(trans.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
      first = 0;

    for(size_t i = first; i < st.size(); i++)
    {
      // layout must either match StateBefore or else be in the common layout
      BARRIER_ASSERT("Mismatching before state",
                     (st[i].IsStates() && st[i].ToStates() == trans.StateBefore) ||
                         (st[i].IsLayout() && st[i].ToLayout() == D3D12_BARRIER_LAYOUT_COMMON),
                     st[i], trans.StateBefore, i);
      st[i] = D3D12ResourceLayout::FromStates(trans.StateAfter);

      if(trans.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        break;
    }
  }

  for(size_t b = 0; b < barriers.newBarriers.size(); b++)
  {
    const D3D12_TEXTURE_BARRIER &trans = barriers.newBarriers[b];
    ResourceId id = GetResID(trans.pResource);

    auto it = states.find(id);
    if(it == states.end())
      return;

    SubresourceStateVector &st = it->second;

    // skip begin-halves of split transitions
    if(trans.SyncBefore == D3D12_BARRIER_SYNC_SPLIT)
      continue;

    // skip non-layout barriers (including UNDEFINED-UNDEFINED)
    if(trans.LayoutBefore == trans.LayoutAfter)
      continue;

    if(trans.Subresources.NumMipLevels == 0)
    {
      size_t first = 0;
      if(trans.Subresources.IndexOrFirstMipLevel == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        first = 0;

      for(size_t sub = first; sub < st.size(); sub++)
      {
        // layout must either match StateBefore, be undefined, or else be in the common state
        BARRIER_ASSERT("Mismatching before state",
                       (st[sub].IsLayout() && st[sub].ToLayout() == trans.LayoutBefore) ||
                           trans.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED ||
                           (st[sub].IsStates() && st[sub].ToStates() == D3D12_RESOURCE_STATE_COMMON),
                       st[sub], trans.LayoutBefore, sub);
        st[sub] = D3D12ResourceLayout::FromLayout(trans.LayoutAfter);

        if(trans.Subresources.IndexOrFirstMipLevel != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
          break;
      }
    }

    D3D12_RESOURCE_DESC desc = trans.pResource->GetDesc();

    UINT arrays = RDCMAX((UINT16)1, desc.DepthOrArraySize);
    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
      arrays = 1;
    UINT mips = RDCMAX((UINT16)1, desc.MipLevels);

    for(UINT p = trans.Subresources.FirstPlane; p < trans.Subresources.NumPlanes; p++)
    {
      for(UINT a = trans.Subresources.FirstArraySlice; a < trans.Subresources.NumArraySlices; a++)
      {
        for(UINT m = trans.Subresources.IndexOrFirstMipLevel; m < trans.Subresources.NumMipLevels; m++)
        {
          UINT sub = ((p * arrays) + a) * mips + m;

          // layout must either match StateBefore, be undefined, or else be in the common state
          BARRIER_ASSERT(
              "Mismatching before state",
              (st[sub].IsLayout() && st[sub].ToLayout() == trans.LayoutBefore) ||
                  trans.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED ||
                  (st[sub].IsStates() && st[sub].ToStates() == D3D12_RESOURCE_STATE_COMMON),
              st[sub], trans.LayoutBefore, sub);
          st[sub] = D3D12ResourceLayout::FromLayout(trans.LayoutAfter);
        }
      }
    }
  }
}

void AddStateResetBarrier(D3D12ResourceLayout srcState, D3D12ResourceLayout dstState,
                          ID3D12Resource *res, UINT subresource, BarrierSet &barriers)
{
  if(srcState.IsStates() && dstState.IsStates())
  {
    D3D12_RESOURCE_BARRIER b;
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = res;
    b.Transition.Subresource = (UINT)subresource;
    b.Transition.StateBefore = srcState.ToStates();
    b.Transition.StateAfter = dstState.ToStates();

    barriers.barriers.push_back(b);
  }
  else if(srcState.IsLayout() && dstState.IsLayout())
  {
    D3D12_TEXTURE_BARRIER b = {};

    b.LayoutBefore = srcState.ToLayout();
    b.AccessBefore = D3D12_BARRIER_ACCESS_COMMON;
    b.SyncBefore = D3D12_BARRIER_SYNC_ALL;
    b.AccessAfter = D3D12_BARRIER_ACCESS_COMMON;
    b.SyncAfter = D3D12_BARRIER_SYNC_ALL;
    b.LayoutAfter = dstState.ToLayout();
    if(b.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED)
      b.AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;
    if(b.LayoutAfter == D3D12_BARRIER_LAYOUT_UNDEFINED)
      b.AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS;
    b.Subresources.IndexOrFirstMipLevel = (UINT)subresource;
    b.pResource = res;

    barriers.newBarriers.push_back(b);
  }
  else
  {
    // difficult case, moving between barrier types and need to go to common in between

    if(srcState.IsStates())
    {
      if(srcState.ToStates() != D3D12_RESOURCE_STATE_COMMON)
      {
        D3D12_RESOURCE_BARRIER b;
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource = res;
        b.Transition.Subresource = (UINT)subresource;
        b.Transition.StateBefore = srcState.ToStates();
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

        barriers.barriers.push_back(b);
      }

      {
        D3D12_TEXTURE_BARRIER b = {};

        b.LayoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
        b.AccessBefore = D3D12_BARRIER_ACCESS_COMMON;
        b.SyncBefore = D3D12_BARRIER_SYNC_ALL;
        b.AccessAfter = D3D12_BARRIER_ACCESS_COMMON;
        b.SyncAfter = D3D12_BARRIER_SYNC_ALL;
        b.LayoutAfter = dstState.ToLayout();
        if(b.LayoutBefore == D3D12_BARRIER_LAYOUT_UNDEFINED)
          b.AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;
        if(b.LayoutAfter == D3D12_BARRIER_LAYOUT_UNDEFINED)
          b.AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS;
        b.Subresources.IndexOrFirstMipLevel = (UINT)subresource;
        b.pResource = res;

        barriers.newBarriers.push_back(b);
      }
    }
    else
    {
      {
        D3D12_TEXTURE_BARRIER b = {};

        b.LayoutBefore = srcState.ToLayout();
        b.AccessBefore = D3D12_BARRIER_ACCESS_COMMON;
        b.SyncBefore = D3D12_BARRIER_SYNC_ALL;
        b.AccessAfter = D3D12_BARRIER_ACCESS_COMMON;
        b.SyncAfter = D3D12_BARRIER_SYNC_ALL;
        b.LayoutAfter = D3D12_BARRIER_LAYOUT_COMMON;
        b.Subresources.IndexOrFirstMipLevel = (UINT)subresource;
        b.pResource = res;

        barriers.newBarriers.push_back(b);
      }

      if(dstState.ToStates() != D3D12_RESOURCE_STATE_COMMON)
      {
        D3D12_RESOURCE_BARRIER b;
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource = res;
        b.Transition.Subresource = (UINT)subresource;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        b.Transition.StateAfter = dstState.ToStates();

        barriers.newToOldBarriers.push_back(b);
      }
    }
  }
}

template <typename SerialiserType>
void D3D12ResourceManager::SerialiseResourceStates(
    SerialiserType &ser, BarrierSet &barriers, std::map<ResourceId, SubresourceStateVector> &states,
    const std::map<ResourceId, SubresourceStateVector> &initialStates)
{
  SERIALISE_ELEMENT_LOCAL(NumMems, (uint32_t)states.size());

  auto srcit = states.begin();

  std::unordered_set<ResourceId> processed;

  for(uint32_t i = 0; i < NumMems; i++)
  {
    SERIALISE_ELEMENT_LOCAL(Resource, srcit->first).TypedAs("ID3D12Resource *"_lit);
    SERIALISE_ELEMENT_LOCAL(States, srcit->second);

    ResourceId liveid;
    if(IsReplayingAndReading() && HasLiveResource(Resource))
      liveid = GetLiveID(Resource);

    if(IsReplayingAndReading() && liveid != ResourceId())
    {
      processed.insert(liveid);

      for(size_t m = 0; m < States.size(); m++)
      {
        const D3D12ResourceLayout srcState = states[liveid][m];
        const D3D12ResourceLayout dstState = States[m];
        if(srcState != dstState)
          AddStateResetBarrier(srcState, dstState, (ID3D12Resource *)GetCurrentResource(liveid),
                               (UINT)m, barriers);
      }
    }

    if(ser.IsWriting())
      srcit++;
  }

  // for any resources that didn't have a recorded state, use the initialStates we're given and
  // restore them if needed
  if(IsReplayingAndReading())
  {
    for(auto it = initialStates.begin(); it != initialStates.end(); ++it)
    {
      // ignore internal resources, we only care about restoring states for captured resources
      if(GetOriginalID(it->first) == it->first)
        continue;

      if(processed.find(it->first) == processed.end())
      {
        for(size_t m = 0; m < it->second.size(); m++)
        {
          const D3D12ResourceLayout srcState = states[it->first][m];
          const D3D12ResourceLayout dstState = it->second[m];
          if(srcState != dstState)
            AddStateResetBarrier(srcState, dstState,
                                 (ID3D12Resource *)GetCurrentResource(it->first), (UINT)m, barriers);
        }
      }
    }
  }

  ApplyBarriers(barriers, states);
}

template void D3D12ResourceManager::SerialiseResourceStates(
    ReadSerialiser &ser, BarrierSet &barriers, std::map<ResourceId, SubresourceStateVector> &states,
    const std::map<ResourceId, SubresourceStateVector> &initialStates);
template void D3D12ResourceManager::SerialiseResourceStates(
    WriteSerialiser &ser, BarrierSet &barriers, std::map<ResourceId, SubresourceStateVector> &states,
    const std::map<ResourceId, SubresourceStateVector> &initialStates);

void D3D12ResourceManager::SetInternalResource(ID3D12DeviceChild *res)
{
  if(!RenderDoc::Inst().IsReplayApp() && res)
  {
    D3D12ResourceRecord *record = GetResourceRecord(GetResID(res));
    if(record)
      record->InternalResource = true;
  }
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

void GPUAddressRangeTracker::AddTo(const GPUAddressRange &range)
{
  SCOPED_WRITELOCK(addressLock);
  auto it = std::lower_bound(addresses.begin(), addresses.end(), range.start);

  addresses.insert(it - addresses.begin(), range);
}

void GPUAddressRangeTracker::RemoveFrom(const GPUAddressRange &range)
{
  {
    SCOPED_WRITELOCK(addressLock);
    size_t i = std::lower_bound(addresses.begin(), addresses.end(), range.start) - addresses.begin();

    // there might be multiple buffers with the same range start, find the exact range for this
    // buffer
    while(i < addresses.size() && addresses[i].start == range.start)
    {
      if(addresses[i].id == range.id)
      {
        addresses.erase(i);
        return;
      }

      ++i;
    }
  }

  RDCERR("Couldn't find matching range to remove for %s", ToStr(range.id).c_str());
}

void GPUAddressRangeTracker::GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr, ResourceId &id,
                                              UINT64 &offs)
{
  id = ResourceId();
  offs = 0;

  if(addr == 0)
    return;

  GPUAddressRange range;

  // this should really be a read-write lock
  {
    SCOPED_READLOCK(addressLock);

    auto it = std::lower_bound(addresses.begin(), addresses.end(), addr);
    if(it == addresses.end())
      return;

    range = *it;
  }

  if(addr < range.start || addr >= range.realEnd)
    return;

  id = range.id;
  offs = addr - range.start;
}

void GPUAddressRangeTracker::GetResIDFromAddrAllowOutOfBounds(D3D12_GPU_VIRTUAL_ADDRESS addr,
                                                              ResourceId &id, UINT64 &offs)
{
  id = ResourceId();
  offs = 0;

  if(addr == 0)
    return;

  GPUAddressRange range;

  // this should really be a read-write lock
  {
    SCOPED_READLOCK(addressLock);

    auto it = std::lower_bound(addresses.begin(), addresses.end(), addr);
    if(it == addresses.end())
      return;

    range = *it;
  }

  if(addr < range.start)
    return;

  // still enforce the OOB end on ranges - which is the remaining range in the backing store.
  // Otherwise we could end up passing through invalid addresses stored in stale descriptors
  if(addr >= range.oobEnd)
    return;

  id = range.id;
  offs = addr - range.start;
}
