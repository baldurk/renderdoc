/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "core/settings.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

RDOC_CONFIG(uint32_t, D3D12_Debug_RTIndirectEstimateOverride, 0,
            "Override how many bytes are reserved for shader tables in each indirect ray dispatch");

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
      else if(!res && desc->ViewDimension != D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE)
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

D3D12RaytracingResourceAndUtilHandler::D3D12RaytracingResourceAndUtilHandler(
    WrappedID3D12Device *device, D3D12GpuBufferAllocator &gpuBufferAllocator)
    : m_wrappedDevice(device),
      m_cmdList(NULL),
      m_cmdAlloc(NULL),
      m_cmdQueue(NULL),
      m_gpuFence(NULL),
      m_gpuSyncHandle(NULL),
      m_gpuSyncCounter(0u),
      m_GPUBufferAllocator(gpuBufferAllocator)
{
}

void D3D12RaytracingResourceAndUtilHandler::CreateInternalResources()
{
  if(m_wrappedDevice)
  {
    ID3D12Device *realDevice = m_wrappedDevice->GetReal();

    if(realDevice)
    {
      HRESULT result = realDevice->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void **)&m_cmdAlloc);

      if(!SUCCEEDED(result))
        RDCERR("D3D12 Command allocator creation failed with error %s", ToStr(result).c_str());

      if(m_cmdAlloc != NULL)
      {
        ID3D12GraphicsCommandList *cmd = NULL;
        result = realDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc, NULL,
                                               __uuidof(ID3D12GraphicsCommandList), (void **)(&cmd));

        if(!SUCCEEDED(result))
          RDCERR("D3D12 Command list creation failed with error %s", ToStr(result).c_str());

        m_cmdList = (ID3D12GraphicsCommandListX *)cmd;
        m_cmdList->Close();
      }

      D3D12_COMMAND_QUEUE_DESC cmdQueueDesc;
      cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
      cmdQueueDesc.NodeMask = 0;
      cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
      cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
      result = realDevice->CreateCommandQueue(&cmdQueueDesc, __uuidof(ID3D12CommandQueue),
                                              (void **)&m_cmdQueue);

      if(!SUCCEEDED(result))
        RDCERR("D3D12 Command queue creation failed with error %s", ToStr(result).c_str());

      result = realDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                       (void **)&m_gpuFence);

      if(!SUCCEEDED(result))
        RDCERR("D3D12 fence creation failed with error %s", ToStr(result).c_str());

      m_gpuSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::CustomHeapWithUavCpuAccess,
                               D3D12GpuBufferHeapMemoryFlag::Default, 16, 256, &ASQueryBuffer);
  }
}

void D3D12RaytracingResourceAndUtilHandler::SyncGpuForRtWork()
{
  m_gpuSyncCounter++;

  HRESULT hr = m_cmdQueue->Signal(m_gpuFence, m_gpuSyncCounter);
  if(!SUCCEEDED(hr))
    RDCERR("Command queue fence signaling failed with error %s ", ToStr(hr).c_str());

  hr = m_gpuFence->SetEventOnCompletion(m_gpuSyncCounter, m_gpuSyncHandle);
  if(!SUCCEEDED(hr))
    RDCERR("Fence completion event signaling failed with error %s ", ToStr(hr).c_str());

  WaitForSingleObject(m_gpuSyncHandle, 10000);
}

void D3D12RaytracingResourceAndUtilHandler::InitInternalResources()
{
  if(IsReplayMode(m_wrappedDevice->GetState()))
  {
    InitReplayBlasPatchingResources();
  }
  InitRayDispatchPatchingResources();
}

void D3D12RaytracingResourceAndUtilHandler::ResizeSerialisationBuffer(UINT64 size)
{
  if(!ASSerialiseBuffer || size > ASSerialiseBuffer->Size())
  {
    SAFE_RELEASE(ASSerialiseBuffer);

    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                               D3D12GpuBufferHeapMemoryFlag::Default, size, 256, &ASSerialiseBuffer);
  }
}

void D3D12RaytracingResourceAndUtilHandler::AddPendingASBuilds(
    ID3D12Fence *fence, UINT64 waitValue, const rdcarray<std::function<bool()>> &callbacks)
{
  SCOPED_LOCK(m_PendingASBuildsLock);
  for(const std::function<bool()> &cb : callbacks)
  {
    fence->AddRef();
    m_PendingASBuilds.push_back({fence, waitValue, cb});
  }
}

void D3D12RaytracingResourceAndUtilHandler::CheckPendingASBuilds()
{
  std::map<ID3D12Fence *, UINT64> fenceValues;
  SCOPED_LOCK(m_PendingASBuildsLock);

  if(m_PendingASBuilds.empty())
    return;

  for(PendingASBuild &build : m_PendingASBuilds)
  {
    // first time we see each fence, get the completed value
    if(fenceValues[build.fence] == 0)
      fenceValues[build.fence] = build.fence->GetCompletedValue();

    // if this fence has been satisfied, release our ref (so it will also get removed below) and call the callback
    if(fenceValues[build.fence] >= build.fenceValue)
    {
      SAFE_RELEASE(build.fence);
      build.callback();
    }
  }

  // remove any builds that completed
  m_PendingASBuilds.removeIf([](const PendingASBuild &build) { return build.fence == NULL; });
}

PatchedRayDispatch D3D12RaytracingResourceAndUtilHandler::PatchRayDispatch(
    ID3D12GraphicsCommandList4 *unwrappedCmd, rdcarray<ResourceId> heaps,
    const D3D12_DISPATCH_RAYS_DESC &desc)
{
  PatchedRayDispatch ret = {};

  ret.desc = desc;

  D3D12MarkerRegion region(unwrappedCmd, "PatchRayDispatch");

  PrepareRayDispatchBuffer(NULL);

  D3D12GpuBuffer *scratchBuffer = NULL;

  RayDispatchPatchCB cbufferData = {};

  uint32_t patchDataSize = 0;

  const uint32_t raygenOffs = patchDataSize;
  patchDataSize = (uint32_t)desc.RayGenerationShaderRecord.SizeInBytes;
  patchDataSize = AlignUp(patchDataSize, (uint32_t)D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

  const uint32_t missOffs = patchDataSize;
  patchDataSize += (uint32_t)desc.MissShaderTable.SizeInBytes;
  patchDataSize = AlignUp(patchDataSize, (uint32_t)D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

  const uint32_t hitOffs = patchDataSize;
  patchDataSize += (uint32_t)desc.HitGroupTable.SizeInBytes;
  patchDataSize = AlignUp(patchDataSize, (uint32_t)D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

  const uint32_t callOffs = patchDataSize;
  patchDataSize += (uint32_t)desc.CallableShaderTable.SizeInBytes;

  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                             D3D12GpuBufferHeapMemoryFlag::Default, patchDataSize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &scratchBuffer);

  RDCCOMPILE_ASSERT(WRAPPED_DESCRIPTOR_STRIDE == sizeof(D3D12Descriptor),
                    "Shader descriptor stride is wrong");

  for(ResourceId heapId : heaps)
  {
    WrappedID3D12DescriptorHeap *heap =
        (WrappedID3D12DescriptorHeap *)m_wrappedDevice->GetResourceManager()
            ->GetCurrentAs<ID3D12DescriptorHeap>(heapId);

    if(heap->GetDescriptors()->GetType() == D3D12DescriptorType::Sampler)
    {
      cbufferData.wrapped_sampHeapBase = heap->GetOriginalGPUBase();
      cbufferData.unwrapped_sampHeapBase = heap->GetGPU(0).ptr;
      cbufferData.wrapped_sampHeapSize = heap->GetNumDescriptors() * sizeof(D3D12Descriptor);
      cbufferData.unwrapped_heapStrides |= uint16_t(heap->GetUnwrappedIncrement());
    }
    else
    {
      cbufferData.wrapped_srvHeapBase = heap->GetOriginalGPUBase();
      cbufferData.unwrapped_srvHeapBase = heap->GetGPU(0).ptr;
      cbufferData.wrapped_srvHeapSize = heap->GetNumDescriptors() * sizeof(D3D12Descriptor);
      cbufferData.unwrapped_heapStrides |= uint32_t(heap->GetUnwrappedIncrement()) << 16;
    }
  }

  cbufferData.numPatchingAddrs = m_NumPatchingAddrs;

  RayDispatchShaderRecordCB recordInfo;

  // set up general patching data - lookup buffers and so on

  unwrappedCmd->SetPipelineState(m_RayPatchingData.descPatchPipe);
  unwrappedCmd->SetComputeRootSignature(m_RayPatchingData.descPatchRootSig);
  unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::GeneralCB,
                                             sizeof(cbufferData) / sizeof(uint32_t), &cbufferData, 0);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::StateObjectData,
                                                 m_LookupAddrs[0]);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::RecordData,
                                                 m_LookupAddrs[1]);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::RootSigData,
                                                 m_LookupAddrs[2]);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::AddrPatchData,
                                                 m_LookupAddrs[3]);

  // dispatch per shader table

  // raygen - required
  {
    recordInfo.shaderrecord_count = 1;
    recordInfo.shaderrecord_stride = uint32_t(ret.desc.RayGenerationShaderRecord.SizeInBytes);

    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::RecordCB,
                                               sizeof(recordInfo) / sizeof(uint32_t), &recordInfo, 0);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::SourceBuffer,
                                                   ret.desc.RayGenerationShaderRecord.StartAddress);
    ret.desc.RayGenerationShaderRecord.StartAddress = scratchBuffer->Address() + raygenOffs;
    unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12PatchRayDispatchParam::DestBuffer,
                                                    ret.desc.RayGenerationShaderRecord.StartAddress);
    unwrappedCmd->Dispatch(1, 1, 1);
  }

  // miss - optional
  if(ret.desc.MissShaderTable.SizeInBytes > 0)
  {
    recordInfo.shaderrecord_count = uint32_t(ret.desc.MissShaderTable.SizeInBytes /
                                             RDCMAX(1ULL, ret.desc.MissShaderTable.StrideInBytes));
    recordInfo.shaderrecord_stride = uint32_t(ret.desc.MissShaderTable.StrideInBytes);

    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::RecordCB,
                                               sizeof(recordInfo) / sizeof(uint32_t), &recordInfo, 0);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::SourceBuffer,
                                                   ret.desc.MissShaderTable.StartAddress);
    ret.desc.MissShaderTable.StartAddress = scratchBuffer->Address() + missOffs;
    unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12PatchRayDispatchParam::DestBuffer,
                                                    ret.desc.MissShaderTable.StartAddress);
    unwrappedCmd->Dispatch(AlignUp(recordInfo.shaderrecord_count, (uint32_t)RECORD_PATCH_THREADS) /
                               RECORD_PATCH_THREADS,
                           1, 1);
  }

  // hitgroups - optional
  if(desc.HitGroupTable.SizeInBytes > 0)
  {
    recordInfo.shaderrecord_count = uint32_t(ret.desc.HitGroupTable.SizeInBytes /
                                             RDCMAX(1ULL, ret.desc.HitGroupTable.StrideInBytes));
    recordInfo.shaderrecord_stride = uint32_t(ret.desc.HitGroupTable.StrideInBytes);

    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::RecordCB,
                                               sizeof(recordInfo) / sizeof(uint32_t), &recordInfo, 0);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::SourceBuffer,
                                                   ret.desc.HitGroupTable.StartAddress);
    ret.desc.HitGroupTable.StartAddress = scratchBuffer->Address() + hitOffs;
    unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12PatchRayDispatchParam::DestBuffer,
                                                    ret.desc.HitGroupTable.StartAddress);
    unwrappedCmd->Dispatch(AlignUp(recordInfo.shaderrecord_count, (uint32_t)RECORD_PATCH_THREADS) /
                               RECORD_PATCH_THREADS,
                           1, 1);
  }

  // callables - optional
  if(desc.CallableShaderTable.SizeInBytes > 0)
  {
    recordInfo.shaderrecord_count =
        uint32_t(ret.desc.CallableShaderTable.SizeInBytes /
                 RDCMAX(1ULL, ret.desc.CallableShaderTable.StrideInBytes));
    recordInfo.shaderrecord_stride = uint32_t(ret.desc.CallableShaderTable.StrideInBytes);

    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::RecordCB,
                                               sizeof(recordInfo) / sizeof(uint32_t), &recordInfo, 0);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::SourceBuffer,
                                                   ret.desc.CallableShaderTable.StartAddress);
    ret.desc.CallableShaderTable.StartAddress = scratchBuffer->Address() + callOffs;
    unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12PatchRayDispatchParam::DestBuffer,
                                                    ret.desc.CallableShaderTable.StartAddress);
    unwrappedCmd->Dispatch(AlignUp(recordInfo.shaderrecord_count, (uint32_t)RECORD_PATCH_THREADS) /
                               RECORD_PATCH_THREADS,
                           1, 1);
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = scratchBuffer->Resource();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  // we have our own ref, the patch data has its ref too that will be held while the list is
  // submittable. Each submission will also get a ref to keep this referenced lookup buffer alive until then
  m_LookupBuffer->AddRef();
  ret.resources.lookupBuffer = m_LookupBuffer;

  // the patch buffer is not owned by us, so the refcounting is the same as above but it takes the
  // ref we had when we created it.
  ret.resources.patchScratchBuffer = scratchBuffer;

  ret.resources.argumentBuffer = NULL;

  return ret;
}

PatchedRayDispatch D3D12RaytracingResourceAndUtilHandler::PatchIndirectRayDispatch(
    ID3D12GraphicsCommandList *unwrappedCmd, rdcarray<ResourceId> heaps,
    ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer,
    UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset)
{
  PatchedRayDispatch ret = {};

  D3D12MarkerRegion region(unwrappedCmd, "PatchIndirectRayDispatch");

  PrepareRayDispatchBuffer(NULL);

  D3D12GpuBuffer *scratchBuffer = NULL;

  // :( some games have fixed sizes, so any other estimate could fail.
  // games without fixed sizes would be reasonably served by 3 or 4 multiplied by the number of BLAS
  // in the largest TLAS, multiplied by the largest local root signature size.
  uint32_t patchDataSize = 20 * 1024 * 1024 * MaxCommandCount;

  if(D3D12_Debug_RTIndirectEstimateOverride() > 0)
    patchDataSize = RDCMAX(patchDataSize, D3D12_Debug_RTIndirectEstimateOverride());

  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                             D3D12GpuBufferHeapMemoryFlag::Default, patchDataSize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &scratchBuffer);

  WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)pCommandSignature;

  D3D12GpuBuffer *argsBuffer = NULL;

  // the args buffer contains both patched arguments for the application as well as patched
  // arguments & count for our indirect patching

  uint64_t applicationArgsSize = AlignUp16(MaxCommandCount * comSig->sig.ByteStride);
  uint64_t patchingArgsSize = AlignUp16(MaxCommandCount * 4 * AlignUp16(sizeof(PatchingExecute)));

  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                             D3D12GpuBufferHeapMemoryFlag::Default,
                             applicationArgsSize + patchingArgsSize + 4,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &argsBuffer);

  RDCCOMPILE_ASSERT(WRAPPED_DESCRIPTOR_STRIDE == sizeof(D3D12Descriptor),
                    "Shader descriptor stride is wrong");

  RayDispatchPatchCB cbufferData = {};

  for(ResourceId heapId : heaps)
  {
    WrappedID3D12DescriptorHeap *heap =
        (WrappedID3D12DescriptorHeap *)m_wrappedDevice->GetResourceManager()
            ->GetCurrentAs<ID3D12DescriptorHeap>(heapId);

    if(heap->GetDescriptors()->GetType() == D3D12DescriptorType::Sampler)
    {
      cbufferData.wrapped_sampHeapBase = heap->GetOriginalGPUBase();
      cbufferData.unwrapped_sampHeapBase = heap->GetGPU(0).ptr;
      cbufferData.wrapped_sampHeapSize = heap->GetNumDescriptors() * sizeof(D3D12Descriptor);
      cbufferData.unwrapped_heapStrides |= uint16_t(heap->GetUnwrappedIncrement());
    }
    else
    {
      cbufferData.wrapped_srvHeapBase = heap->GetOriginalGPUBase();
      cbufferData.unwrapped_srvHeapBase = heap->GetGPU(0).ptr;
      cbufferData.wrapped_srvHeapSize = heap->GetNumDescriptors() * sizeof(D3D12Descriptor);
      cbufferData.unwrapped_heapStrides |= uint32_t(heap->GetUnwrappedIncrement()) << 16;
    }
  }

  cbufferData.numPatchingAddrs = m_NumPatchingAddrs;

  RayIndirectDispatchCB prepInfo = {};

  prepInfo.commandSigDispatchOffset = comSig->sig.PackedByteSize - sizeof(D3D12_DISPATCH_RAYS_DESC);
  prepInfo.commandSigSize = comSig->sig.PackedByteSize;
  prepInfo.commandSigStride = comSig->sig.ByteStride;
  prepInfo.maxCommandCount = MaxCommandCount;
  prepInfo.scratchBuffer = scratchBuffer->Address();

  // set up general patching data - lookup buffers and so on

  unwrappedCmd->SetPipelineState(m_RayPatchingData.indirectPrepPipe);
  unwrappedCmd->SetComputeRootSignature(m_RayPatchingData.indirectPrepRootSig);
  unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12IndirectPrepParam::GeneralCB,
                                             sizeof(prepInfo) / sizeof(uint32_t), &prepInfo, 0);
  unwrappedCmd->SetComputeRootShaderResourceView(
      (UINT)D3D12IndirectPrepParam::AppExecuteArgs,
      pArgumentBuffer->GetGPUVirtualAddress() + ArgumentBufferOffset);
  unwrappedCmd->SetComputeRootShaderResourceView(
      (UINT)D3D12IndirectPrepParam::AppCount,
      pCountBuffer ? pCountBuffer->GetGPUVirtualAddress() + CountBufferOffset
                   : pArgumentBuffer->GetGPUVirtualAddress());
  unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12IndirectPrepParam::PatchedExecuteArgs,
                                                  argsBuffer->Address());
  unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12IndirectPrepParam::InternalExecuteArgs,
                                                  argsBuffer->Address() + applicationArgsSize);
  unwrappedCmd->SetComputeRootUnorderedAccessView(
      (UINT)D3D12IndirectPrepParam::InternalExecuteCount,
      argsBuffer->Address() + applicationArgsSize + patchingArgsSize);

  // prepare our actual indirect patching by setting up destination space locations as well as
  // patching the actual arguments buffer we'll return
  unwrappedCmd->Dispatch(1, 1, 1);

  // this is ready for the application to use (once we patch the things it refers to), and for us to use below
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = argsBuffer->Resource();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  unwrappedCmd->SetPipelineState(m_RayPatchingData.descPatchPipe);
  unwrappedCmd->SetComputeRootSignature(m_RayPatchingData.descPatchRootSig);
  unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::GeneralCB,
                                             sizeof(cbufferData) / sizeof(uint32_t), &cbufferData, 0);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::StateObjectData,
                                                 m_LookupAddrs[0]);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::RecordData,
                                                 m_LookupAddrs[1]);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::RootSigData,
                                                 m_LookupAddrs[2]);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::AddrPatchData,
                                                 m_LookupAddrs[3]);

  RayDispatchShaderRecordCB recordInfo = {};
  // these will be overwritten by the execute indirect, but set them to something to be safe
  unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::RecordCB,
                                             sizeof(recordInfo) / sizeof(uint32_t), &recordInfo, 0);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::SourceBuffer,
                                                 m_LookupAddrs[0]);
  unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12PatchRayDispatchParam::DestBuffer,
                                                  scratchBuffer->Address());
  // execute the indirect arguments buffer that we prepared in the dispatch above to do the actual
  // shader record patching/unwrapping
  unwrappedCmd->ExecuteIndirect(m_RayPatchingData.indirectComSig, MaxCommandCount * 4,
                                argsBuffer->Resource(), argsBuffer->Offset() + applicationArgsSize,
                                argsBuffer->Resource(),
                                argsBuffer->Offset() + applicationArgsSize + patchingArgsSize);

  // scratch buffer has now been patched and is ready to use as well
  barrier.Transition.pResource = scratchBuffer->Resource();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  // we have our own ref, the patch data has its ref too that will be held while the list is
  // submittable. Each submission will also get a ref to keep this referenced lookup buffer alive until then
  m_LookupBuffer->AddRef();
  ret.resources.lookupBuffer = m_LookupBuffer;

  // the patch and arguments buffers are not owned by us, so the refcounting is the same as above
  // but it takes the ref we had when we created it.
  ret.resources.patchScratchBuffer = scratchBuffer;
  ret.resources.argumentBuffer = argsBuffer;

  return ret;
}

void D3D12RaytracingResourceAndUtilHandler::PrepareRayDispatchBuffer(
    const GPUAddressRangeTracker *origAddresses)
{
  SCOPED_LOCK(m_LookupBufferLock);
  if(m_LookupBufferDirty || origAddresses)
  {
    m_LookupBufferDirty = false;
    SAFE_RELEASE(m_LookupBuffer);

    bytebuf lookupData;

    const size_t ObjectLookupStride = sizeof(StateObjectLookup);
    const size_t RecordDataStride = sizeof(D3D12ShaderExportDatabase::ExportedIdentifier);
    const size_t RootSigStride = sizeof(LocalRootSigData);

    RDCCOMPILE_ASSERT(int(ObjectLookupStride / 8) * 8 == ObjectLookupStride, "Not aligned");
    RDCCOMPILE_ASSERT(RecordDataStride == sizeof(ShaderRecordData), "Not identically sized");

    size_t numExports = 0;
    for(size_t i = 0; i < m_ExportDatabases.size(); i++)
      numExports += m_ExportDatabases[i]->ownExports.size();

    const size_t ObjectLookupOffset = lookupData.size();
    // we include one extra export database as a NULL terminator
    lookupData.resize(lookupData.size() + (m_ExportDatabases.size() + 1) * ObjectLookupStride);
    lookupData.resize(AlignUp(lookupData.size(), (size_t)256U));

    const size_t RecordDataOffset = lookupData.size();
    lookupData.resize(lookupData.size() + numExports * RecordDataStride);
    lookupData.resize(AlignUp(lookupData.size(), (size_t)256U));

    const size_t RootSigOffset = lookupData.size();
    lookupData.resize(lookupData.size() + m_UniqueLocalRootSigs.size() * RootSigStride);

    const size_t PatchAddrOffset = lookupData.size();
    if(origAddresses)
    {
      lookupData.resize(lookupData.size() + sizeof(BlasAddressPair) * origAddresses->addresses.size());
    }
    else
    {
      lookupData.resize(lookupData.size() + sizeof(BlasAddressPair));
    }

    uint32_t exportIndex = 0;
    for(size_t i = 0; i < m_ExportDatabases.size(); i++)
    {
      ResourceId id = m_ExportDatabases[i]->GetResourceId();
      memcpy(lookupData.data() + ObjectLookupOffset + i * ObjectLookupStride, &id, sizeof(id));
      memcpy(lookupData.data() + ObjectLookupOffset + i * ObjectLookupStride + sizeof(ResourceId),
             &exportIndex, sizeof(exportIndex));

      memcpy(lookupData.data() + RecordDataOffset + RecordDataStride * exportIndex,
             m_ExportDatabases[i]->ownExports.data(), m_ExportDatabases[i]->ownExports.byteSize());

      exportIndex += (uint32_t)m_ExportDatabases[i]->ownExports.size();
    }

    for(size_t i = 0; i < m_UniqueLocalRootSigs.size(); i++)
    {
      uint32_t *rootSigData = (uint32_t *)(lookupData.data() + RootSigOffset + RootSigStride * i);

      rootSigData[0] = (uint32_t)m_UniqueLocalRootSigs[i].size();
      memcpy(&rootSigData[1], m_UniqueLocalRootSigs[i].data(), m_UniqueLocalRootSigs[i].byteSize());
    }

    m_NumPatchingAddrs = 0;

    for(size_t i = 0; origAddresses && i < origAddresses->addresses.size(); i++)
    {
      GPUAddressRange addressRange = origAddresses->addresses[i];
      ResourceId resId = addressRange.id;
      if(m_wrappedDevice->GetResourceManager()->HasLiveResource(resId))
      {
        WrappedID3D12Resource *wrappedRes =
            (WrappedID3D12Resource *)m_wrappedDevice->GetResourceManager()->GetLiveResource(resId);

        BlasAddressPair addressPair;
        addressPair.oldAddress.start = addressRange.start;
        addressPair.oldAddress.end = addressRange.realEnd;

        addressPair.newAddress.start = wrappedRes->GetGPUVirtualAddress();
        addressPair.newAddress.end = addressPair.newAddress.start + wrappedRes->GetDesc().Width;
        memcpy(lookupData.data() + PatchAddrOffset + sizeof(BlasAddressPair) * m_NumPatchingAddrs,
               &addressPair, sizeof(addressPair));
        m_NumPatchingAddrs++;
      }
    }

    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::UploadHeap,
                               D3D12GpuBufferHeapMemoryFlag::Default, lookupData.size(), 256,
                               &m_LookupBuffer);

    memcpy(m_LookupBuffer->Map(), lookupData.data(), lookupData.size());
    m_LookupBuffer->Unmap();

    D3D12_GPU_VIRTUAL_ADDRESS baseAddr = m_LookupBuffer->Address();
    m_LookupAddrs[0] = baseAddr + ObjectLookupOffset;
    m_LookupAddrs[1] = baseAddr + RecordDataOffset;
    m_LookupAddrs[2] = baseAddr + RootSigOffset;
    m_LookupAddrs[3] = baseAddr + PatchAddrOffset;
  }
}

void D3D12RaytracingResourceAndUtilHandler::InitRayDispatchPatchingResources()
{
  D3D12ShaderCache *shaderCache = m_wrappedDevice->GetShaderCache();

  if(shaderCache == NULL)
  {
    RDCERR("Shadercache not available");
    return;
  }

  // need 5x 2-DWORD root buffers, the rest we can have for constants.
  // this could be made another buffer to track but it fits in push constants so we'll use them
  RDCCOMPILE_ASSERT(
      ((sizeof(RayDispatchPatchCB) + sizeof(RayDispatchShaderRecordCB)) / sizeof(uint32_t)) +
              (uint32_t(D3D12PatchRayDispatchParam::Count) - 2) * 2 <
          64,
      "Root signature constants are too large");

  // Root Signature
  rdcarray<D3D12_ROOT_PARAMETER1> rootParameters;
  rootParameters.reserve((uint16_t)D3D12PatchRayDispatchParam::Count);

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues = sizeof(RayDispatchPatchCB) / sizeof(uint32_t);
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Constants.ShaderRegister = 1;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues = sizeof(RayDispatchShaderRecordCB) / sizeof(uint32_t);
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 1;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 2;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 3;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 4;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  RDCASSERT(rootParameters.size() == uint32_t(D3D12PatchRayDispatchParam::Count));

  ID3DBlob *rootSig = shaderCache->MakeRootSig(rootParameters, D3D12_ROOT_SIGNATURE_FLAG_NONE);

  if(rootSig)
  {
    HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
        0, rootSig->GetBufferPointer(), rootSig->GetBufferSize(), __uuidof(ID3D12RootSignature),
        (void **)&m_RayPatchingData.descPatchRootSig);

    if(!SUCCEEDED(result))
      RDCERR("Unable to create root signature for dispatch patching");

    // PipelineState
    ID3DBlob *shader = NULL;
    rdcstr hlsl = GetEmbeddedResource(raytracing_hlsl);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PatchRayDispatchCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

    if(shader)
    {
      D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
      pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      pipeline.NodeMask = 0;
      pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
      pipeline.CachedPSO = {NULL, 0};
      pipeline.pRootSignature = m_RayPatchingData.descPatchRootSig;

      result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
          &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_RayPatchingData.descPatchPipe);

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for dispatch patching");
    }
    else
    {
      RDCERR("Failed to get shader for dispatch patching");
    }

    SAFE_RELEASE(rootSig);
  }

  // need 5x 2-DWORD root buffers, the rest we can have for constants.
  // this could be made another buffer to track but it fits in push constants so we'll use them
  RDCCOMPILE_ASSERT((sizeof(RayIndirectDispatchCB) / sizeof(uint32_t)) +
                            (uint32_t(D3D12IndirectPrepParam::Count) - 2) * 2 <
                        64,
                    "Root signature constants are too large");

  rootParameters.clear();
  rootParameters.reserve((uint16_t)D3D12IndirectPrepParam::Count);

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues = sizeof(RayIndirectDispatchCB) / sizeof(uint32_t);
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 1;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 1;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 2;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  RDCASSERT(rootParameters.size() == uint32_t(D3D12IndirectPrepParam::Count));

  rootSig = shaderCache->MakeRootSig(rootParameters, D3D12_ROOT_SIGNATURE_FLAG_NONE);

  if(rootSig)
  {
    HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
        0, rootSig->GetBufferPointer(), rootSig->GetBufferSize(), __uuidof(ID3D12RootSignature),
        (void **)&m_RayPatchingData.indirectPrepRootSig);

    if(!SUCCEEDED(result))
      RDCERR("Unable to create root signature for indirect execute patching");

    // PipelineState
    ID3DBlob *shader = NULL;
    rdcstr hlsl = GetEmbeddedResource(raytracing_hlsl);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PrepareRayIndirectExecuteCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

    if(shader)
    {
      D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
      pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      pipeline.NodeMask = 0;
      pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
      pipeline.CachedPSO = {NULL, 0};
      pipeline.pRootSignature = m_RayPatchingData.indirectPrepRootSig;

      result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
          &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_RayPatchingData.indirectPrepPipe);

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for indirect execute patching");
    }
    else
    {
      RDCERR("Failed to get shader for indirect execute patching");
    }

    SAFE_RELEASE(rootSig);
  }

  {
    D3D12_INDIRECT_ARGUMENT_DESC args[] = {
        {D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT},
        {D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW},
        {D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW},
        {D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH},
    };

    args[0].Constant.DestOffsetIn32BitValues = 0;
    args[0].Constant.Num32BitValuesToSet = sizeof(RayDispatchShaderRecordCB) / sizeof(uint32_t);
    args[0].Constant.RootParameterIndex = (uint32_t)D3D12PatchRayDispatchParam::RecordCB;

    RDCCOMPILE_ASSERT(sizeof(RayDispatchShaderRecordCB) == offsetof(PatchingExecute, sourceData),
                      "Start of PatchingExecute is not one RayDispatchShaderRecordCB");

    args[1].ShaderResourceView.RootParameterIndex =
        (uint32_t)D3D12PatchRayDispatchParam::SourceBuffer;
    args[2].UnorderedAccessView.RootParameterIndex = (uint32_t)D3D12PatchRayDispatchParam::DestBuffer;

    RDCCOMPILE_ASSERT(sizeof(PatchingExecute) == sizeof(GPUAddress) * 2 +
                                                     sizeof(RayDispatchShaderRecordCB) +
                                                     sizeof(Vec4u) + sizeof(uint32_t) * 2,
                      "PatchingExecute has changed size");
    RDCCOMPILE_ASSERT(sizeof(PatchingExecute) % 16 == 0,
                      "PatchingExecute is not 16-byte aligned, add explicit padding");

    D3D12_COMMAND_SIGNATURE_DESC desc = {};
    desc.ByteStride = (UINT)AlignUp16(sizeof(PatchingExecute));
    desc.NumArgumentDescs = ARRAY_COUNT(args);
    desc.pArgumentDescs = args;

    HRESULT hr = m_wrappedDevice->GetReal()->CreateCommandSignature(
        &desc, m_RayPatchingData.descPatchRootSig, __uuidof(ID3D12CommandSignature),
        (void **)&m_RayPatchingData.indirectComSig);

    if(!SUCCEEDED(hr))
      RDCERR("Unable to create command signature for indirect execute patching");
  }
}

void D3D12RaytracingResourceAndUtilHandler::InitReplayBlasPatchingResources()
{
  // Root Signature
  rdcarray<D3D12_ROOT_PARAMETER1> rootParameters;
  rootParameters.reserve((uint16_t)D3D12PatchTLASBuildParam::Count);

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues = 1;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  {
    D3D12_ROOT_PARAMETER1 rootParam;
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  D3D12ShaderCache *shaderCache = m_wrappedDevice->GetShaderCache();

  if(shaderCache != NULL)
  {
    ID3DBlob *rootSig = shaderCache->MakeRootSig(rootParameters, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    if(rootSig)
    {
      HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
          0, rootSig->GetBufferPointer(), rootSig->GetBufferSize(), __uuidof(ID3D12RootSignature),
          (void **)&m_accStructPatchInfo.m_rootSignature);

      if(!SUCCEEDED(result))
        RDCERR("Unable to create root signature for patching the BLAS");

      // PipelineState
      ID3DBlob *shader = NULL;
      rdcstr hlsl = GetEmbeddedResource(raytracing_hlsl);
      shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PatchAccStructAddressCS",
                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

      if(shader)
      {
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
        pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        pipeline.NodeMask = 0;
        pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
        pipeline.CachedPSO = {NULL, 0};
        pipeline.pRootSignature = m_accStructPatchInfo.m_rootSignature;

        result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
            &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_accStructPatchInfo.m_pipeline);

        if(!SUCCEEDED(result))
          RDCERR("Unable to create pipeline for patching the BLAS");
      }

      SAFE_RELEASE(rootSig);
    }
  }
  else
  {
    RDCERR("Shadercache not available");
  }
}

uint32_t D3D12RaytracingResourceAndUtilHandler::RegisterLocalRootSig(const D3D12RootSignature &sig)
{
  rdcarray<uint32_t> patchOffsets;
  uint32_t offset = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
  for(uint32_t i = 0; i < sig.Parameters.size(); i++)
  {
    // constants are 4-byte aligned, everything else is 8-byte
    if(sig.Parameters[i].ParameterType != D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      offset = AlignUp(offset, 8U);

    if(sig.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      patchOffsets.push_back(offset);

    if(sig.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
       sig.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
       sig.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
      patchOffsets.push_back(0x80000000U | offset);

    if(sig.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      offset += sig.Parameters[i].Constants.Num32BitValues * sizeof(uint32_t);
    else
      offset += sizeof(uint64_t);
  }

  if(patchOffsets.size() > MAX_LOCALSIG_PARAMS)
    RDCERR("Local root signature uses more than %zu patchable parameters, will fail to patch",
           patchOffsets.size());

  // no patching needed if no tables
  if(patchOffsets.empty())
    return ~0U;

  SCOPED_LOCK(m_LookupBufferLock);

  int idx = m_UniqueLocalRootSigs.indexOf(patchOffsets);
  if(idx < 0)
  {
    idx = m_UniqueLocalRootSigs.count();
    m_UniqueLocalRootSigs.push_back(patchOffsets);
    m_LookupBufferDirty = true;
  }

  return idx;
}

void D3D12RaytracingResourceAndUtilHandler::RegisterExportDatabase(D3D12ShaderExportDatabase *db)
{
  SCOPED_LOCK(m_LookupBufferLock);
  m_ExportDatabases.push_back(db);

  m_LookupBufferDirty = true;
}

void D3D12RaytracingResourceAndUtilHandler::UnregisterExportDatabase(D3D12ShaderExportDatabase *db)
{
  SCOPED_LOCK(m_LookupBufferLock);
  m_ExportDatabases.removeOne(db);
  // don't dirty the lookup buffer here, there's not much value in recreating it just to reduce
  // memory use - next time we need to add data we'll reclaim that.
}

bool D3D12GpuBufferAllocator::D3D12GpuBufferResource::CreateCommittedResourceBuffer(
    ID3D12Device *device, const D3D12_HEAP_PROPERTIES &heapProperty, D3D12_RESOURCE_STATES initState,
    uint64_t size, bool allowUav, D3D12GpuBufferResource **bufferResource)
{
  if(device && bufferResource)
  {
    ID3D12Resource *newBufferResource = NULL;

    D3D12_RESOURCE_DESC bufferResDesc;
    bufferResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferResDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    bufferResDesc.DepthOrArraySize = 1u;
    bufferResDesc.MipLevels = 1u;
    bufferResDesc.Height = 1u;
    bufferResDesc.Flags =
        allowUav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    bufferResDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferResDesc.SampleDesc = {1, 0};
    bufferResDesc.Width = size;

    D3D12GpuBufferResource *retBufferRes = NULL;

    // Create committed resource
    HRESULT opResult = device->CreateCommittedResource(
        &heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResDesc, initState, NULL,
        __uuidof(ID3D12Resource), (void **)&newBufferResource);

    if(SUCCEEDED(opResult) && newBufferResource != NULL)
    {
      retBufferRes = new D3D12GpuBufferResource(newBufferResource, heapProperty.Type);
    }
    else
    {
      RDCERR("Allocation failed with result code %s", ToStr(opResult).c_str());
    }

    if(retBufferRes)
    {
      *bufferResource = retBufferRes;
      return true;
    }
  }

  return false;
}

bool D3D12GpuBufferAllocator::D3D12GpuBufferResource::ReleaseGpuBufferResource(
    D3D12GpuBufferResource *bufferResource)
{
  delete bufferResource;
  bufferResource = NULL;
  return true;
}

D3D12GpuBufferAllocator::D3D12GpuBufferResource::D3D12GpuBufferResource(ID3D12Resource *resource,
                                                                        D3D12_HEAP_TYPE heapType)
    : m_resource(resource), m_heapType(heapType)

{
  if(m_resource)
  {
    m_resDesc = m_resource->GetDesc();
    m_resourceGpuAddressRange.start = resource->GetGPUVirtualAddress();
    m_resourceGpuAddressRange.realEnd = m_resourceGpuAddressRange.start + m_resDesc.Width;
  }
}

bool D3D12GpuBufferAllocator::D3D12GpuBufferPool::Alloc(WrappedID3D12Device *wrappedDevice,
                                                        D3D12GpuBufferHeapMemoryFlag heapMem,
                                                        uint64_t size, uint64_t alignment,
                                                        D3D12GpuBuffer **gpuBuffer)
{
  D3D12GpuBufferAllocator &allocator = wrappedDevice->GetResourceManager()->GetGPUBufferAllocator();
  if(heapMem == D3D12GpuBufferHeapMemoryFlag::Default)
  {
    if(size > m_bufferInitSize)
    {
      m_bufferInitSize = size;
    }

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    for(D3D12GpuBufferResource *bufferRes : m_bufferResourceList)
    {
      if(bufferRes->SubAlloc(size, alignment, gpuAddress))
      {
        *gpuBuffer = new D3D12GpuBuffer(allocator, m_bufferPoolHeapType,
                                        D3D12GpuBufferHeapMemoryFlag::Default, size, alignment,
                                        gpuAddress, bufferRes->Resource());
        return true;
      }
    }

    D3D12GpuBufferResource *newBufferResource = NULL;
    if(D3D12GpuBufferAllocator::D3D12GpuBufferResource::CreateBufferResource(
           wrappedDevice, m_bufferPoolHeapType, m_bufferInitSize, &newBufferResource))
    {
      m_bufferResourceList.push_back(newBufferResource);
      if(newBufferResource->SubAlloc(size, alignment, gpuAddress))
      {
        *gpuBuffer = new D3D12GpuBuffer(allocator, m_bufferPoolHeapType,
                                        D3D12GpuBufferHeapMemoryFlag::Default, size, alignment,
                                        gpuAddress, newBufferResource->Resource());
        return true;
      }
    }
  }
  else
  {
    D3D12GpuBufferResource *newBufferResource = NULL;
    if(D3D12GpuBufferAllocator::D3D12GpuBufferResource::CreateBufferResource(
           wrappedDevice, m_bufferPoolHeapType, size, &newBufferResource))
    {
      m_bufferResourceList.push_back(newBufferResource);
      *gpuBuffer = new D3D12GpuBuffer(
          allocator, m_bufferPoolHeapType, D3D12GpuBufferHeapMemoryFlag::Dedicated, size,
          D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
          newBufferResource->Resource()->GetGPUVirtualAddress(), newBufferResource->Resource());
      return true;
    }
  }

  RDCERR("Unable to allocate GPU memory");
  return false;
}

void D3D12GpuBufferAllocator::D3D12GpuBufferPool::Free(const D3D12GpuBuffer &gpuBuffer)
{
  if(gpuBuffer.Resource() == NULL)
  {
    RDCERR("Freeing invalid GPU buffer");
    return;
  }

  for(D3D12GpuBufferResource *bufferRes : m_bufferResourceList)
  {
    if(bufferRes->Resource() == gpuBuffer.Resource())
    {
      D3D12GpuBufferHeapMemoryFlag heapMem = gpuBuffer.HeapMemory();
      if(heapMem == D3D12GpuBufferHeapMemoryFlag::Default)
      {
        if(bufferRes->SubAllocationInRange(gpuBuffer.Address()))
        {
          if(!bufferRes->Free(gpuBuffer.Address()))
          {
            RDCERR("Invalid address when freeing buffer");
          }
          return;
        }
      }
      else if(heapMem == D3D12GpuBufferHeapMemoryFlag::Dedicated)
      {
        if(D3D12GpuBufferResource::ReleaseGpuBufferResource(bufferRes))
        {
          m_bufferResourceList.removeOne(bufferRes);
          return;
        }
      }
    }
  }
}

bool D3D12GpuBufferAllocator::Alloc(D3D12GpuBufferHeapType heapType,
                                    D3D12GpuBufferHeapMemoryFlag heapMem, uint64_t size,
                                    uint64_t alignment, D3D12GpuBuffer **gpuBuffer)
{
  SCOPED_LOCK(m_bufferAllocLock);
  bool success = false;
  if(heapType < D3D12GpuBufferHeapType::Count && heapType != D3D12GpuBufferHeapType::UnInitialized)
  {
    size_t heap = (size_t)heapType;
    if(m_bufferPoolList[heap] == NULL)
    {
      uint64_t bufferPoolInitSize = D3D12GpuBufferPool::kDefaultWithUavSizeBufferInitSize;
      if(heapType == D3D12GpuBufferHeapType::AccStructDefaultHeap)
      {
        bufferPoolInitSize = D3D12GpuBufferPool::kAccStructBufferPoolInitSize;
      }

      m_bufferPoolList[heap] = new D3D12GpuBufferPool(heapType, bufferPoolInitSize);
    }

    if(m_bufferPoolList[heap] != NULL)
    {
      success = m_bufferPoolList[heap]->Alloc(m_wrappedDevice, heapMem, size, alignment, gpuBuffer);
    }
  }

  if(success)
  {
    m_totalAllocatedMemoryInUse += size;
  }

  return success;
}

void D3D12GpuBufferAllocator::Release(const D3D12GpuBuffer &gpuBuffer)
{
  SCOPED_LOCK(m_bufferAllocLock);
  size_t heap = (size_t)gpuBuffer.HeapType();
  if(gpuBuffer.HeapType() < D3D12GpuBufferHeapType::Count && m_bufferPoolList[heap] != NULL)
  {
    m_bufferPoolList[heap]->Free(gpuBuffer);
    return;
  }

  RDCERR("Couldn't identify buffer heap type %zu", heap);
}

bool D3D12GpuBufferAllocator::D3D12GpuBufferResource::CreateBufferResource(
    WrappedID3D12Device *wrappedDevice, D3D12GpuBufferHeapType heapType, uint64_t size,
    D3D12GpuBufferResource **bufferResource)
{
  D3D12_HEAP_PROPERTIES heapProperty;
  heapProperty.CreationNodeMask = 0;
  heapProperty.VisibleNodeMask = 0;
  heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON;
  bool allowUav = false;
  switch(heapType)
  {
    case D3D12GpuBufferHeapType::AccStructDefaultHeap:
    case D3D12GpuBufferHeapType::DefaultHeap:
    case D3D12GpuBufferHeapType::DefaultHeapWithUav:
    {
      heapProperty.Type = D3D12_HEAP_TYPE_DEFAULT;
      if(heapType == D3D12GpuBufferHeapType::AccStructDefaultHeap)
      {
        initState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        allowUav = true;
      }

      if(heapType == D3D12GpuBufferHeapType::DefaultHeapWithUav)
      {
        allowUav = true;
      }

      break;
    }

    case D3D12GpuBufferHeapType::ReadBackHeap:
    {
      heapProperty.Type = D3D12_HEAP_TYPE_READBACK;
      initState = D3D12_RESOURCE_STATE_COPY_DEST;
      break;
    }

    case D3D12GpuBufferHeapType::UploadHeap:
    {
      heapProperty.Type = D3D12_HEAP_TYPE_UPLOAD;
      initState = D3D12_RESOURCE_STATE_GENERIC_READ;
      break;
    }

    case D3D12GpuBufferHeapType::CustomHeapWithUavCpuAccess:
    {
      heapProperty.Type = D3D12_HEAP_TYPE_CUSTOM;
      heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
      heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
      allowUav = true;
      break;
    }
    default: RDCLOG("Unhandled buffer pool");
  }

  D3D12GpuBufferResource *newBufferResource = NULL;
  if(D3D12GpuBufferResource::CreateCommittedResourceBuffer(
         wrappedDevice->GetReal(), heapProperty, initState, size, allowUav, &newBufferResource))
  {
    *bufferResource = newBufferResource;
    return true;
  }

  return false;
}

void D3D12ResourceManager::ApplyBarriers(BarrierSet &barriers,
                                         std::map<ResourceId, SubresourceStateVector> &states)
{
  for(size_t b = 0; b < barriers.barriers.size(); b++)
  {
    const D3D12_RESOURCE_TRANSITION_BARRIER &trans = barriers.barriers[b].Transition;
    ResourceId id = GetResID(trans.pResource);

    auto it = states.find(id);
    if(it == states.end())
      continue;

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
      continue;

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
        D3D12ResourceLayout srcState = states[liveid][m];
        D3D12ResourceLayout dstState = States[m];

        // because of some extreme ugliness on the D3D12 side, resources can be created in new
        // layouts without the new barriers actually being supported. If that's the case, we just
        // pretend they're in old COMMON to avoid doing the new barrier
        if(!m_Device->GetOpts12().EnhancedBarriersSupported && srcState.IsLayout())
        {
          RDCASSERT(srcState.ToLayout() == D3D12_BARRIER_LAYOUT_COMMON, srcState.ToLayout());
          srcState = D3D12ResourceLayout::FromStates(D3D12_RESOURCE_STATE_COMMON);
        }

        if(!m_Device->GetOpts12().EnhancedBarriersSupported && dstState.IsLayout())
        {
          RDCASSERT(dstState.ToLayout() == D3D12_BARRIER_LAYOUT_COMMON, dstState.ToLayout());
          dstState = D3D12ResourceLayout::FromStates(D3D12_RESOURCE_STATE_COMMON);
        }

        if(srcState != dstState)
        {
          AddStateResetBarrier(srcState, dstState, (ID3D12Resource *)GetCurrentResource(liveid),
                               (UINT)m, barriers);
        }
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

    // find the largest resource containing this address - not perfect but helps with trivially bad
    // aliases where a tiny resource and a large resource are co-situated and the larger resource
    // needs to be used for validity
    while((it + 1)->start <= addr && (it + 1)->realEnd > range.realEnd)
    {
      it++;
      range = *it;
    }
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

    // find the largest resource containing this address - not perfect but helps with trivially bad
    // aliases where a tiny resource and a large resource are co-situated and the larger resource
    // needs to be used for validity
    while((it + 1)->start <= addr && (it + 1)->realEnd > range.realEnd)
    {
      it++;
      range = *it;
    }
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

void D3D12GpuBuffer::AddRef()
{
  InterlockedIncrement(&m_RefCount);
}

void D3D12GpuBuffer::Release()
{
  unsigned int ret = InterlockedDecrement(&m_RefCount);
  if(ret == 0)
  {
    m_Allocator.Release(*this);

    delete this;
  }
}
