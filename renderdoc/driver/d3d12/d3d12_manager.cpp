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

#include "d3d12_manager.h"
#include "driver/dxgi/dxgi_common.h"
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
  nonsamp.type = D3D12DescriptorType::CBV;
  nonsamp.resource = NULL;
  if(pDesc)
    nonsamp.cbv = *pDesc;
  else
    RDCEraseEl(nonsamp.cbv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc)
{
  nonsamp.type = D3D12DescriptorType::SRV;
  nonsamp.resource = pResource;
  if(pDesc)
    nonsamp.srv = *pDesc;
  else
    RDCEraseEl(nonsamp.srv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, ID3D12Resource *pCounterResource,
                           const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc)
{
  nonsamp.type = D3D12DescriptorType::UAV;
  nonsamp.resource = pResource;
  nonsamp.uav.counterResource = pCounterResource;
  if(pDesc)
    nonsamp.uav.desc.Init(*pDesc);
  else
    RDCEraseEl(nonsamp.uav.desc);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc)
{
  nonsamp.type = D3D12DescriptorType::RTV;
  nonsamp.resource = pResource;
  if(pDesc)
    nonsamp.rtv = *pDesc;
  else
    RDCEraseEl(nonsamp.rtv);
}

void D3D12Descriptor::Init(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc)
{
  nonsamp.type = D3D12DescriptorType::DSV;
  nonsamp.resource = pResource;
  if(pDesc)
    nonsamp.dsv = *pDesc;
  else
    RDCEraseEl(nonsamp.dsv);
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

  switch(type)
  {
    case D3D12DescriptorType::Sampler:
    {
      dev->CreateSampler(&samp.desc, handle);
      break;
    }
    case D3D12DescriptorType::CBV:
    {
      dev->CreateConstantBufferView(&nonsamp.cbv, handle);
      break;
    }
    case D3D12DescriptorType::SRV:
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC *desc = &nonsamp.srv;
      if(desc->ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      {
        desc = nonsamp.resource ? NULL : defaultSRV();

        const map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(nonsamp.resource));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_SHADER_RESOURCE_VIEW_DESC bbDesc = {};
          bbDesc.Format = it->second;
          bbDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
          bbDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
          bbDesc.Texture2D.MipLevels = 1;
          dev->CreateShaderResourceView(nonsamp.resource, &bbDesc, handle);
          return;
        }
      }
      else if(!nonsamp.resource)
      {
        // if we don't have a resource (which is possible if the descriptor is unused), use a
        // default descriptor
        desc = defaultSRV();
      }

      // it's possible to end up with invalid resource and descriptor combinations:
      // 1. descriptor is created for ResID_1234 BC1_TYPELESS and a view BC1_UNORM
      // 2. resource is freed.
      // 3. some time later, new resource is created ResID_5678 BC3_UNORM
      // 4. Key point is - descriptor has a pointer to the resource, and the slot is
      //    re-allocated in 3.
      // 5. We now have a descriptor that is BC3_UNORM resource and BC1_UNORM view.
      //
      // This is unavoidable without recording back-references from resources to the
      // descriptors that use them. Instead, we just detect the invalid case here
      // and since we know the descriptor is unused (since it's invalid to use it
      // after the resource is freed, and it would have to be recreated with a valid
      // format combination) we can just force a null resource.
      //
      // so need to check if
      // a) we have a non-NULL resource (otherwise any descriptor is fine)
      // b) descriptor and resource have a non-UNKNOWN format (buffers have UNKNOWN
      //    type which can be cast arbitrarily by the view).
      // c) when the resource is typed, the view must be identical, when it's typeless
      //    the view format must be castable
      if(nonsamp.resource && desc)
      {
        DXGI_FORMAT resFormat = nonsamp.resource->GetDesc().Format;
        DXGI_FORMAT viewFormat = desc->Format;

        if(resFormat != DXGI_FORMAT_UNKNOWN && viewFormat != DXGI_FORMAT_UNKNOWN)
        {
          if(!IsTypelessFormat(resFormat))
          {
            if(resFormat != viewFormat)
            {
              nonsamp.resource = NULL;
              desc = defaultSRV();
            }
          }
          else
          {
            if(resFormat != GetTypelessFormat(viewFormat))
            {
              nonsamp.resource = NULL;
              desc = defaultSRV();
            }
          }
        }
      }

      D3D12_SHADER_RESOURCE_VIEW_DESC planeDesc;
      // ensure that multi-plane formats have a valid plane slice specified. This shouldn't be
      // possible as it should be the application's responsibility to be valid too, but we fix it up
      // here anyway.
      if(nonsamp.resource && desc)
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

      dev->CreateShaderResourceView(nonsamp.resource, desc, handle);
      break;
    }
    case D3D12DescriptorType::RTV:
    {
      D3D12_RENDER_TARGET_VIEW_DESC *desc = &nonsamp.rtv;
      if(desc->ViewDimension == D3D12_RTV_DIMENSION_UNKNOWN)
      {
        desc = nonsamp.resource ? NULL : defaultRTV();

        const map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(nonsamp.resource));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_RENDER_TARGET_VIEW_DESC bbDesc = {};
          bbDesc.Format = it->second;
          bbDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
          dev->CreateRenderTargetView(nonsamp.resource, &bbDesc, handle);
          return;
        }
      }
      else if(!nonsamp.resource)
      {
        // if we don't have a resource (which is possible if the descriptor is unused), use a
        // default descriptor
        desc = defaultRTV();
      }

      // see comment above in SRV case for what this code is doing
      if(nonsamp.resource && desc)
      {
        DXGI_FORMAT resFormat = nonsamp.resource->GetDesc().Format;
        DXGI_FORMAT viewFormat = desc->Format;

        if(resFormat != DXGI_FORMAT_UNKNOWN && viewFormat != DXGI_FORMAT_UNKNOWN)
        {
          if(!IsTypelessFormat(resFormat))
          {
            if(resFormat != viewFormat)
            {
              nonsamp.resource = NULL;
              desc = defaultRTV();
            }
          }
          else
          {
            if(resFormat != GetTypelessFormat(viewFormat))
            {
              nonsamp.resource = NULL;
              desc = defaultRTV();
            }
          }
        }
      }

      D3D12_RENDER_TARGET_VIEW_DESC planeDesc;
      // ensure that multi-plane formats have a valid plane slice specified. This shouldn't be
      // possible as it should be the application's responsibility to be valid too, but we fix it up
      // here anyway.
      if(nonsamp.resource && desc)
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

      dev->CreateRenderTargetView(nonsamp.resource, desc, handle);
      break;
    }
    case D3D12DescriptorType::DSV:
    {
      D3D12_DEPTH_STENCIL_VIEW_DESC *desc = &nonsamp.dsv;
      if(desc->ViewDimension == D3D12_DSV_DIMENSION_UNKNOWN)
      {
        desc = nonsamp.resource ? NULL : defaultDSV();
      }
      else if(!nonsamp.resource)
      {
        // if we don't have a resource (which is possible if the descriptor is unused), use a
        // default descriptor
        desc = defaultDSV();
      }

      // see comment above in SRV case for what this code is doing
      if(nonsamp.resource && desc)
      {
        DXGI_FORMAT resFormat = nonsamp.resource->GetDesc().Format;
        DXGI_FORMAT viewFormat = desc->Format;

        if(resFormat != DXGI_FORMAT_UNKNOWN && viewFormat != DXGI_FORMAT_UNKNOWN)
        {
          if(!IsTypelessFormat(resFormat))
          {
            if(resFormat != viewFormat)
            {
              nonsamp.resource = NULL;
              desc = defaultDSV();
            }
          }
          else
          {
            if(resFormat != GetTypelessFormat(viewFormat))
            {
              nonsamp.resource = NULL;
              desc = defaultDSV();
            }
          }
        }
      }

      dev->CreateDepthStencilView(nonsamp.resource, desc, handle);
      break;
    }
    case D3D12DescriptorType::UAV:
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = nonsamp.uav.desc.AsDesc();

      D3D12_UNORDERED_ACCESS_VIEW_DESC *desc = &uavdesc;
      if(uavdesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      {
        desc = nonsamp.resource ? NULL : defaultUAV();

        const map<ResourceId, DXGI_FORMAT> &bbs = dev->GetBackbufferFormats();

        auto it = bbs.find(GetResID(nonsamp.resource));

        // fixup for backbuffers
        if(it != bbs.end())
        {
          D3D12_UNORDERED_ACCESS_VIEW_DESC bbDesc = {};
          bbDesc.Format = it->second;
          bbDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
          dev->CreateUnorderedAccessView(nonsamp.resource, NULL, &bbDesc, handle);
          return;
        }
      }
      else if(!nonsamp.resource)
      {
        // if we don't have a resource (which is possible if the descriptor is unused), use a
        // default descriptor
        desc = defaultUAV();
      }

      // don't create a UAV with a counter resource but no main resource. This is fine because
      // if the main resource wasn't present in the capture, this UAV isn't present - the counter
      // must have been included for some other reference.
      ID3D12Resource *counter = nonsamp.uav.counterResource;
      if(nonsamp.resource == NULL)
        counter = NULL;

      if(counter == NULL && desc && desc->ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
        desc->Buffer.CounterOffsetInBytes = 0;

      // see comment above in SRV case for what this code is doing
      if(nonsamp.resource && desc)
      {
        DXGI_FORMAT resFormat = nonsamp.resource->GetDesc().Format;
        DXGI_FORMAT viewFormat = desc->Format;

        if(resFormat != DXGI_FORMAT_UNKNOWN && viewFormat != DXGI_FORMAT_UNKNOWN)
        {
          if(!IsTypelessFormat(resFormat))
          {
            if(resFormat != viewFormat)
            {
              nonsamp.resource = NULL;
              desc = defaultUAV();
            }
          }
          else
          {
            if(resFormat != GetTypelessFormat(viewFormat))
            {
              nonsamp.resource = NULL;
              desc = defaultUAV();
            }
          }
        }
      }

      D3D12_UNORDERED_ACCESS_VIEW_DESC planeDesc;
      // ensure that multi-plane formats have a valid plane slice specified. This shouldn't be
      // possible as it should be the application's responsibility to be valid too, but we fix it up
      // here anyway.
      if(nonsamp.resource && desc)
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

      dev->CreateUnorderedAccessView(nonsamp.resource, counter, desc, handle);
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
  WrappedID3D12DescriptorHeap *heap = samp.heap;
  uint32_t index = samp.idx;

  *this = src;

  samp.heap = heap;
  samp.idx = index;
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
      id = WrappedID3D12Resource::GetResIDFromAddr(nonsamp.cbv.BufferLocation);
      break;
    case D3D12DescriptorType::SRV: id = GetResID(nonsamp.resource); break;
    case D3D12DescriptorType::UAV:
      id2 = GetResID(nonsamp.uav.counterResource);
    // deliberate fall-through
    case D3D12DescriptorType::RTV:
    case D3D12DescriptorType::DSV:
      ref = eFrameRef_Write;
      id = GetResID(nonsamp.resource);
      break;
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE UnwrapCPU(D3D12Descriptor *handle)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = {};
  if(handle == NULL)
    return ret;

  return handle->samp.heap->GetCPU(handle->samp.idx);
}

D3D12_GPU_DESCRIPTOR_HANDLE UnwrapGPU(D3D12Descriptor *handle)
{
  D3D12_GPU_DESCRIPTOR_HANDLE ret = {};
  if(handle == NULL)
    return ret;

  return handle->samp.heap->GetGPU(handle->samp.idx);
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

  return PortableHandle(GetResID(desc->samp.heap), desc->samp.idx);
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

template <typename SerialiserType>
void D3D12ResourceManager::SerialiseResourceStates(SerialiserType &ser,
                                                   std::vector<D3D12_RESOURCE_BARRIER> &barriers,
                                                   std::map<ResourceId, SubresourceStateVector> &states)
{
  SERIALISE_ELEMENT_LOCAL(NumMems, (uint32_t)states.size());

  auto srcit = states.begin();

  for(uint32_t i = 0; i < NumMems; i++)
  {
    SERIALISE_ELEMENT_LOCAL(Resource, srcit->first);
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

bool D3D12ResourceManager::SerialisableResource(ResourceId id, D3D12ResourceRecord *record)
{
  if(record->type == Resource_GraphicsCommandList || record->type == Resource_CommandQueue)
    return false;

  if(m_Device->GetFrameCaptureResourceId() == id)
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
