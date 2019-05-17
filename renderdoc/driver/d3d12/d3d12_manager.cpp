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

#include "d3d12_manager.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

void D3D12Descriptor::Init(const D3D12_SAMPLER_DESC *pDesc)
{
  if(pDesc)
    data.samp.desc = *pDesc;
  else
    RDCEraseEl(data.samp.desc);
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
      dev->CreateSampler(&data.samp.desc, handle);
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
      id = WrappedID3D12Resource1::GetResIDFromAddr(data.nonsamp.cbv.BufferLocation);
      break;
    case D3D12DescriptorType::SRV: id = data.nonsamp.resource; break;
    case D3D12DescriptorType::UAV:
      id2 = data.nonsamp.counterResource;
    // deliberate fall-through
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

  if(!manager->HasLiveResource(handle.heap))
    return NULL;

  WrappedID3D12DescriptorHeap *heap = manager->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap);

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

void D3D12ResourceManager::ApplyBarriers(std::vector<D3D12_RESOURCE_BARRIER> &barriers,
                                         std::map<ResourceId, SubresourceStateVector> &states)
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

template <typename SerialiserType>
void D3D12ResourceManager::SerialiseResourceStates(SerialiserType &ser,
                                                   std::vector<D3D12_RESOURCE_BARRIER> &barriers,
                                                   std::map<ResourceId, SubresourceStateVector> &states)
{
  SERIALISE_ELEMENT_LOCAL(NumMems, (uint32_t)states.size());

  auto srcit = states.begin();

  for(uint32_t i = 0; i < NumMems; i++)
  {
    SERIALISE_ELEMENT_LOCAL(Resource, srcit->first).TypedAs("ID3D12Resource *"_lit);
    SERIALISE_ELEMENT_LOCAL(States, srcit->second);

    ResourceId liveid;
    if(IsReplayingAndReading() && HasLiveResource(Resource))
      liveid = GetLiveID(Resource);

    if(IsReplayingAndReading() && liveid != ResourceId())
    {
      for(size_t m = 0; m < States.size(); m++)
      {
        D3D12_RESOURCE_BARRIER b;
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource = (ID3D12Resource *)GetCurrentResource(liveid);
        b.Transition.Subresource = (UINT)m;
        b.Transition.StateBefore = states[liveid][m];
        b.Transition.StateAfter = States[m];

        barriers.push_back(b);
      }
    }

    if(ser.IsWriting())
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

template void D3D12ResourceManager::SerialiseResourceStates(
    ReadSerialiser &ser, std::vector<D3D12_RESOURCE_BARRIER> &barriers,
    std::map<ResourceId, SubresourceStateVector> &states);
template void D3D12ResourceManager::SerialiseResourceStates(
    WriteSerialiser &ser, std::vector<D3D12_RESOURCE_BARRIER> &barriers,
    std::map<ResourceId, SubresourceStateVector> &states);

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
