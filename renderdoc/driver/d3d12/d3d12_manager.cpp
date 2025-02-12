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
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"
#include "d3d12_rootsig.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

RDOC_CONFIG(uint32_t, D3D12_Debug_RT_IndirectEstimateOverride, 0,
            "Override how many bytes are reserved for shader tables in each indirect ray dispatch "
            "(will be multiplied by MaxCommandCount)");
RDOC_CONFIG(uint32_t, D3D12_Debug_RT_MaxVertexIncrement, 1000,
            "Amount to add to the API-provided max vertex when building a BLAS with an index "
            "buffer, to account for incorrectly set values by application.");
RDOC_CONFIG(
    uint32_t, D3D12_Debug_RT_MaxVertexPercentIncrease, 10,
    "Percentage increase for the API-provided max vertex when building a BLAS with an index "
    "buffer, to account for incorrectly set values by application.");
RDOC_CONFIG(uint32_t, D3D12_Debug_RT_ASCacheThreshold, 5000,
            "How many milliseconds to wait before caching an AS to disk if it has been unmodified "
            "for that long");
RDOC_CONFIG(bool, D3D12_Debug_RT_Auditing, false, "Audit RT work during capture and replay.");

// batch 50 at a time, if we have one check per frame this would cache 5000 BLASs in 100 frames
// which is a reasonable background pace
RDOC_CONFIG(uint32_t, D3D12_Debug_RT_ASCacheBatchSize, 50,
            "The maximum number of ASs to cache to disk in a single batch (batch processing "
            "happens at indeterminate intervals but no more than once per submission");

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

static uint32_t GuessTablePatchRequirements(UINT MaxCommandCount)
{
  uint32_t patchDataSize = RDCMAX(64U, 20 * 1024 * 1024 * MaxCommandCount);

  if(D3D12_Debug_RT_IndirectEstimateOverride() > 0)
    patchDataSize = D3D12_Debug_RT_IndirectEstimateOverride() * MaxCommandCount;

  return patchDataSize;
}

static UINT64 GetTableCount(D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE table)
{
  return table.StrideInBytes == 0 ? 1ULL : table.SizeInBytes / table.StrideInBytes;
}

// debugging logging for barriers
#if 0
#define BARRIER_DBG RDCLOG
#define BARRIER_ASSERT RDCASSERTMSG
#else
#define BARRIER_DBG(...)
#define BARRIER_ASSERT(...)
#endif

D3D12RTManager::D3D12RTManager(WrappedID3D12Device *device,
                               D3D12GpuBufferAllocator &gpuBufferAllocator)
    : m_wrappedDevice(device), m_GPUBufferAllocator(gpuBufferAllocator)
{
}

D3D12RTManager::~D3D12RTManager()
{
  SAFE_RELEASE(ASSerialiseBuffer);
  SAFE_RELEASE(m_accStructPatchInfo.m_rootSignature);
  SAFE_RELEASE(m_accStructPatchInfo.m_pipeline);
  SAFE_RELEASE(m_TLASCopyingData.ArgsBuffer);
  SAFE_RELEASE(m_TLASCopyingData.PreparePipe);
  SAFE_RELEASE(m_TLASCopyingData.CopyPipe);
  SAFE_RELEASE(m_TLASCopyingData.RootSig);
  SAFE_RELEASE(m_TLASCopyingData.IndirectSig);
  SAFE_RELEASE(m_RayPatchingData.shaderTablePatchRootSig);
  SAFE_RELEASE(m_RayPatchingData.shaderTablePatchPipe);
  SAFE_RELEASE(m_RayPatchingData.shaderTableCopyPipe);
  SAFE_RELEASE(m_RayPatchingData.indirectComSig);
  SAFE_RELEASE(m_RayPatchingData.indirectPrepPipe);
  SAFE_RELEASE(m_RayPatchingData.indirectPrepRootSig);
  SAFE_RELEASE(m_TimerQueryHeap);

  if(m_ASCacheThread)
  {
    Atomic::Dec32(&m_ASCacheThreadRunning);
    // wake the thread so it can notice that the running flag is zero'd
    m_ASCacheThreadSemaphore->Wake(1);
    Threading::JoinThread(m_ASCacheThread);
    Threading::CloseThread(m_ASCacheThread);
    m_ASCacheThreadSemaphore->Destroy();
  }
}

void D3D12RTManager::InitInternalResources()
{
  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::CustomHeapWithUavCpuAccess,
                             D3D12GpuBufferHeapMemoryFlag::Default, 16, 256, &ASQueryBuffer);

  if(D3D12_Debug_RT_Auditing())
  {
    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::CustomHeapWithUavCpuAccess,
                               D3D12GpuBufferHeapMemoryFlag::Default, 256, 256,
                               &PostbuildReadbackBuffer);
  }

  if(IsReplayMode(m_wrappedDevice->GetState()))
  {
    InitReplayBlasPatchingResources();
  }
  InitTLASInstanceCopyingResources();
  InitRayDispatchPatchingResources();
}

void D3D12RTManager::ResizeSerialisationBuffer(UINT64 size)
{
  if(!ASSerialiseBuffer || size > ASSerialiseBuffer->Size())
  {
    SAFE_RELEASE(ASSerialiseBuffer);

    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                               D3D12GpuBufferHeapMemoryFlag::Default, size, 256, &ASSerialiseBuffer);
  }
}

void D3D12RTManager::Verify(PatchedRayDispatch &r)
{
  if(!r.resources.readbackBuffer)
    return;

  WrappedID3D12DescriptorHeap *sampHeap = NULL, *resHeap = NULL;
  for(ResourceId heapId : r.heaps)
  {
    WrappedID3D12DescriptorHeap *heap =
        (WrappedID3D12DescriptorHeap *)m_wrappedDevice->GetResourceManager()
            ->GetCurrentAs<ID3D12DescriptorHeap>(heapId);

    if(heap->GetDescriptors()->GetType() == D3D12DescriptorType::Sampler)
      sampHeap = heap;
    else
      resHeap = heap;
  }

  byte *data = (byte *)r.resources.readbackBuffer->Map();

  // simple case, single non-indirect dispatch
  if(r.comSig == NULL)
  {
    VerifyDispatch(r.desc, data, data + r.resources.patchScratchBuffer->Size(), resHeap, sampHeap);
    return;
  }

  // indirect dispatches need extra work (and extra verification!)

  const uint32_t indirectPatchReservationSize = GuessTablePatchRequirements(r.MaxCommands);
  const uint64_t applicationArgsSize = AlignUp16(r.MaxCommands * r.comSig->sig.ByteStride);
  const uint64_t patchingArgsSize = AlignUp16(r.MaxCommands * 4 * AlignUp16(sizeof(PatchingExecute)));

  byte *wrappedExecArgs = data + indirectPatchReservationSize * 2;
  UINT *sourceCount = (UINT *)(wrappedExecArgs + applicationArgsSize);
  byte *unwrappedExecArgs = ((byte *)sourceCount) + 4;
  PatchingExecute *internalExecs = (PatchingExecute *)(unwrappedExecArgs + applicationArgsSize);
  UINT *patchExecCount = (UINT *)(((byte *)internalExecs) + patchingArgsSize);

  UINT cmdCount = *sourceCount;
  RDCASSERT(cmdCount < r.MaxCommands, cmdCount, r.MaxCommands);
  // if we didn't have a dynamic count this should be 0, revert to the fixed CPU-side count
  if(!r.HasDynamicCount)
  {
    RDCASSERTEQUAL(cmdCount, 0);
    cmdCount = r.MaxCommands;
  }

  byte *wrappedRecords = data;
  byte *unwrappedRecords = wrappedRecords + indirectPatchReservationSize;

  UINT internalExecsRequired = 0;

  // this loop emulates/verifies RENDERDOC_PrepareRayIndirectExecuteCS
  for(UINT cmd = 0; cmd < cmdCount; cmd++)
  {
    D3D12_DISPATCH_RAYS_DESC *wrappedDisp =
        (D3D12_DISPATCH_RAYS_DESC *)(wrappedExecArgs + cmd * r.comSig->sig.ByteStride +
                                     r.comSig->sig.PackedByteSize - sizeof(D3D12_DISPATCH_RAYS_DESC));
    D3D12_DISPATCH_RAYS_DESC *unwrappedDisp =
        (D3D12_DISPATCH_RAYS_DESC *)(unwrappedExecArgs + cmd * r.comSig->sig.ByteStride +
                                     r.comSig->sig.PackedByteSize - sizeof(D3D12_DISPATCH_RAYS_DESC));

    // sizes should have been propagated trivially
    RDCASSERTEQUAL(wrappedDisp->Width, unwrappedDisp->Width);
    RDCASSERTEQUAL(wrappedDisp->Height, unwrappedDisp->Height);
    RDCASSERTEQUAL(wrappedDisp->Depth, unwrappedDisp->Depth);

    uint64_t offset = 0;

    // raygen is required. Check it w as placed correctly
    RDCASSERT(wrappedDisp->RayGenerationShaderRecord.SizeInBytes);
    RDCASSERTEQUAL(unwrappedDisp->RayGenerationShaderRecord.StartAddress,
                   r.resources.patchScratchBuffer->Address() + offset);
    RDCASSERTEQUAL(wrappedDisp->RayGenerationShaderRecord.SizeInBytes,
                   unwrappedDisp->RayGenerationShaderRecord.SizeInBytes);
    offset += AlignUp(wrappedDisp->RayGenerationShaderRecord.SizeInBytes, 256ULL);

    // also check the internal indirect patch was properly configured
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].sourceData,
                   wrappedDisp->RayGenerationShaderRecord.StartAddress);
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].destData,
                   unwrappedDisp->RayGenerationShaderRecord.StartAddress);
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_count, 1);
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_stride,
                   unwrappedDisp->RayGenerationShaderRecord.SizeInBytes);
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.x, 1);
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.y, 1);
    RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.z, 1);

    internalExecsRequired++;

    // for each optional table check its placement as well and the corresponding indirect patch

    if(wrappedDisp->MissShaderTable.SizeInBytes)
    {
      if(unwrappedDisp->HitGroupTable.StrideInBytes == 0)
        unwrappedDisp->HitGroupTable.StrideInBytes = unwrappedDisp->HitGroupTable.SizeInBytes;

      RDCASSERTEQUAL(unwrappedDisp->MissShaderTable.StartAddress,
                     r.resources.patchScratchBuffer->Address() + offset);
      RDCASSERTEQUAL(wrappedDisp->MissShaderTable.SizeInBytes,
                     unwrappedDisp->MissShaderTable.SizeInBytes);
      RDCASSERTEQUAL(wrappedDisp->MissShaderTable.StrideInBytes,
                     unwrappedDisp->MissShaderTable.StrideInBytes);
      offset += AlignUp(wrappedDisp->MissShaderTable.SizeInBytes, 256ULL);

      RDCASSERTEQUAL(internalExecs[internalExecsRequired].sourceData,
                     wrappedDisp->MissShaderTable.StartAddress);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].destData,
                     unwrappedDisp->MissShaderTable.StartAddress);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_count,
                     GetTableCount(wrappedDisp->MissShaderTable));
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_stride,
                     unwrappedDisp->MissShaderTable.StrideInBytes);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.x,
                     AlignUp(internalExecs[internalExecsRequired].shaderrecord_count,
                             (uint32_t)RECORD_PATCH_THREADS) /
                         RECORD_PATCH_THREADS);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.y, 1);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.z, 1);

      internalExecsRequired++;
    }

    if(wrappedDisp->HitGroupTable.SizeInBytes)
    {
      if(wrappedDisp->HitGroupTable.StrideInBytes == 0)
        wrappedDisp->HitGroupTable.StrideInBytes = wrappedDisp->HitGroupTable.SizeInBytes;

      RDCASSERTEQUAL(unwrappedDisp->HitGroupTable.StartAddress,
                     r.resources.patchScratchBuffer->Address() + offset);
      RDCASSERTEQUAL(wrappedDisp->HitGroupTable.SizeInBytes,
                     unwrappedDisp->HitGroupTable.SizeInBytes);
      RDCASSERTEQUAL(wrappedDisp->HitGroupTable.StrideInBytes,
                     unwrappedDisp->HitGroupTable.StrideInBytes);
      offset += AlignUp(wrappedDisp->HitGroupTable.SizeInBytes, 256ULL);

      RDCASSERTEQUAL(internalExecs[internalExecsRequired].sourceData,
                     wrappedDisp->HitGroupTable.StartAddress);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].destData,
                     unwrappedDisp->HitGroupTable.StartAddress);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_count,
                     GetTableCount(wrappedDisp->HitGroupTable));
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_stride,
                     unwrappedDisp->HitGroupTable.StrideInBytes);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.x,
                     AlignUp(internalExecs[internalExecsRequired].shaderrecord_count,
                             (uint32_t)RECORD_PATCH_THREADS) /
                         RECORD_PATCH_THREADS);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.y, 1);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.z, 1);

      internalExecsRequired++;
    }

    if(wrappedDisp->CallableShaderTable.SizeInBytes)
    {
      if(wrappedDisp->CallableShaderTable.StrideInBytes == 0)
        wrappedDisp->CallableShaderTable.StrideInBytes = wrappedDisp->CallableShaderTable.SizeInBytes;

      RDCASSERTEQUAL(unwrappedDisp->CallableShaderTable.StartAddress,
                     r.resources.patchScratchBuffer->Address() + offset);
      RDCASSERTEQUAL(wrappedDisp->CallableShaderTable.SizeInBytes,
                     unwrappedDisp->CallableShaderTable.SizeInBytes);
      RDCASSERTEQUAL(wrappedDisp->CallableShaderTable.StrideInBytes,
                     unwrappedDisp->CallableShaderTable.StrideInBytes);
      offset += AlignUp(wrappedDisp->CallableShaderTable.SizeInBytes, 256ULL);

      RDCASSERTEQUAL(internalExecs[internalExecsRequired].sourceData,
                     wrappedDisp->CallableShaderTable.StartAddress);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].destData,
                     unwrappedDisp->CallableShaderTable.StartAddress);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_count,
                     GetTableCount(wrappedDisp->CallableShaderTable));
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].shaderrecord_stride,
                     unwrappedDisp->CallableShaderTable.StrideInBytes);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.x,
                     AlignUp(internalExecs[internalExecsRequired].shaderrecord_count,
                             (uint32_t)RECORD_PATCH_THREADS) /
                         RECORD_PATCH_THREADS);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.y, 1);
      RDCASSERTEQUAL(internalExecs[internalExecsRequired].dispatchDim.z, 1);

      internalExecsRequired++;
    }

    RDCASSERTMSG("Indirect reservation size too small", offset < indirectPatchReservationSize,
                 offset, indirectPatchReservationSize);

    VerifyDispatch(*unwrappedDisp, wrappedRecords, unwrappedRecords, resHeap, sampHeap);

    wrappedRecords += offset;
    unwrappedRecords += offset;
  }

  RDCASSERTEQUAL(internalExecsRequired, *patchExecCount);

  r.resources.readbackBuffer->Unmap();
}

void D3D12RTManager::VerifyDispatch(D3D12_DISPATCH_RAYS_DESC desc, byte *wrappedRecords,
                                    byte *unwrappedRecords, WrappedID3D12DescriptorHeap *resHeap,
                                    WrappedID3D12DescriptorHeap *sampHeap)
{
  // raygen is not optional
  RDCASSERT(desc.RayGenerationShaderRecord.SizeInBytes >= D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
  RDCASSERT(desc.RayGenerationShaderRecord.StartAddress);
  if(desc.RayGenerationShaderRecord.StartAddress)
  {
    VerifyRecord(desc.RayGenerationShaderRecord.SizeInBytes, wrappedRecords, unwrappedRecords,
                 resHeap, sampHeap);
  }

  wrappedRecords += AlignUp(desc.RayGenerationShaderRecord.SizeInBytes, 256ULL);
  unwrappedRecords += AlignUp(desc.RayGenerationShaderRecord.SizeInBytes, 256ULL);

  if(desc.MissShaderTable.StartAddress)
  {
    if(desc.MissShaderTable.StrideInBytes == 0)
      desc.MissShaderTable.StrideInBytes = desc.MissShaderTable.SizeInBytes;
    for(UINT64 i = 0; i < desc.MissShaderTable.SizeInBytes / desc.MissShaderTable.StrideInBytes; i++)
      VerifyRecord(desc.MissShaderTable.StrideInBytes,
                   wrappedRecords + desc.MissShaderTable.StrideInBytes * i,
                   unwrappedRecords + desc.MissShaderTable.StrideInBytes * i, resHeap, sampHeap);
  }

  wrappedRecords += AlignUp(desc.MissShaderTable.SizeInBytes, 256ULL);
  unwrappedRecords += AlignUp(desc.MissShaderTable.SizeInBytes, 256ULL);

  if(desc.HitGroupTable.StartAddress)
  {
    if(desc.HitGroupTable.StrideInBytes == 0)
      desc.HitGroupTable.StrideInBytes = desc.HitGroupTable.SizeInBytes;
    for(UINT64 i = 0; i < desc.HitGroupTable.SizeInBytes / desc.HitGroupTable.StrideInBytes; i++)
      VerifyRecord(desc.HitGroupTable.StrideInBytes,
                   wrappedRecords + desc.HitGroupTable.StrideInBytes * i,
                   unwrappedRecords + desc.HitGroupTable.StrideInBytes * i, resHeap, sampHeap);
  }

  wrappedRecords += AlignUp(desc.HitGroupTable.SizeInBytes, 256ULL);
  unwrappedRecords += AlignUp(desc.HitGroupTable.SizeInBytes, 256ULL);

  if(desc.CallableShaderTable.StartAddress)
  {
    if(desc.CallableShaderTable.StrideInBytes == 0)
      desc.CallableShaderTable.StrideInBytes = desc.CallableShaderTable.SizeInBytes;
    for(UINT64 i = 0;
        i < desc.CallableShaderTable.SizeInBytes / desc.CallableShaderTable.StrideInBytes; i++)
      VerifyRecord(desc.CallableShaderTable.StrideInBytes,
                   wrappedRecords + desc.CallableShaderTable.StrideInBytes * i,
                   unwrappedRecords + desc.CallableShaderTable.StrideInBytes * i, resHeap, sampHeap);
  }

  wrappedRecords += AlignUp(desc.CallableShaderTable.SizeInBytes, 256ULL);
  unwrappedRecords += AlignUp(desc.CallableShaderTable.SizeInBytes, 256ULL);
}

void D3D12RTManager::VerifyRecord(const uint64_t recordSize, byte *wrappedRecord,
                                  byte *unwrappedRef, WrappedID3D12DescriptorHeap *resHeap,
                                  WrappedID3D12DescriptorHeap *sampHeap)
{
  bytebuf record;

  record.resize((size_t)recordSize);
  memcpy(record.data(), wrappedRecord, (size_t)recordSize);

  struct ShaderIdentifier
  {
    ResourceId id;      // the object which has the actual identifier in its ownExports array
    uint32_t index;     // the index in the object's ownExports array
    uint32_t pad[5];    // padding up to D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
  };

  ShaderIdentifier *ident = (ShaderIdentifier *)wrappedRecord;

  WrappedID3D12StateObject *obj =
      m_wrappedDevice->GetResourceManager()->GetLiveAs<WrappedID3D12StateObject>(ident->id);

  uint16_t localIdx = 0xffff;

  if(obj)
  {
    memcpy(record.data(), obj->exports->ownExports[ident->index].real,
           D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    localIdx = obj->exports->ownExports[ident->index].localRootSigIndex;
  }
  else
  {
    // we should not get NULL objects back for records with data
    RDCASSERT(ident->id == ResourceId());
    memset(record.data(), 0, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
  }

  // copy remaining data - it will be overwritten below by an VA/handle unwraps, otherwise it could
  // be constant data or just padding
  memcpy(record.data() + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
         wrappedRecord + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
         size_t(recordSize - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));

  if(localIdx != 0xffff)
  {
    rdcarray<uint32_t> &rootConfig = m_UniqueLocalRootSigs[localIdx];

    for(uint32_t offs : rootConfig)
    {
      bool isVA = (offs & 0x80000000U) != 0;
      offs &= ~0x80000000U;

      if(isVA)
      {
        D3D12_GPU_VIRTUAL_ADDRESS *va = (uint64_t *)(wrappedRecord + offs);

        ResourceId id;
        uint64_t resoffs;
        m_wrappedDevice->GetResIDFromOrigAddr(*va, id, resoffs);

        D3D12_GPU_VIRTUAL_ADDRESS unwrappedVA = 0;
        if(*va == 0)
        {
        }
        else if(id == ResourceId())
        {
          RDCWARN("Invalid VA %llx, setting to 0", *va);
        }
        else
        {
          ID3D12Resource *res = m_wrappedDevice->GetResourceManager()->GetLiveAs<ID3D12Resource>(id);

          unwrappedVA = res->GetGPUVirtualAddress() + resoffs;
        }

        memcpy(record.data() + offs, &unwrappedVA, sizeof(unwrappedVA));
      }
      else
      {
        uint64_t wrappedHandle = *(uint64_t *)(wrappedRecord + offs);

        D3D12_GPU_DESCRIPTOR_HANDLE unwrappedHandle = {};

        if(resHeap && wrappedHandle >= resHeap->GetOriginalGPUBase() &&
           wrappedHandle < resHeap->GetOriginalGPUBase() +
                               resHeap->GetNumDescriptors() * sizeof(D3D12Descriptor))
        {
          uint32_t idx =
              uint32_t((wrappedHandle - resHeap->GetOriginalGPUBase()) / sizeof(D3D12Descriptor));
          unwrappedHandle = resHeap->GetGPU(idx);
        }
        else if(sampHeap && wrappedHandle >= sampHeap->GetOriginalGPUBase() &&
                wrappedHandle < +sampHeap->GetOriginalGPUBase() +
                                    sampHeap->GetNumDescriptors() * sizeof(D3D12Descriptor))
        {
          uint32_t idx =
              uint32_t((wrappedHandle - sampHeap->GetOriginalGPUBase()) / sizeof(D3D12Descriptor));
          unwrappedHandle = sampHeap->GetGPU(idx);
        }
        else
        {
          RDCWARN("Invalid descriptor, setting to 0");
        }

        memcpy(record.data() + offs, &unwrappedHandle, sizeof(unwrappedHandle));
      }
    }
  }

  RDCASSERT(memcmp(record.data(), unwrappedRef, record.size()) == 0);
}

uint32_t D3D12RTManager::GetFreeQuery()
{
  SCOPED_LOCK(m_TimerStatsLock);
  if(m_TimerQueryHeap == NULL)
  {
    D3D12_QUERY_HEAP_DESC timerQueryDesc;
    // allow for up to 50 dispatches per frame, 500 AS builds, and assume 5 frames before we see the results
    timerQueryDesc.Count = (50 + 500) * 5 * 2;
    timerQueryDesc.NodeMask = 1;
    timerQueryDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    HRESULT hr = m_wrappedDevice->GetReal()->CreateQueryHeap(
        &timerQueryDesc, __uuidof(ID3D12QueryHeap), (void **)&m_TimerQueryHeap);
    CHECK_HR(m_wrappedDevice, hr);
    if(FAILED(hr))
      RDCERR("Failed to create timer query heap HRESULT: %s", ToStr(hr).c_str());

    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::ReadBackHeap,
                               D3D12GpuBufferHeapMemoryFlag::Default,
                               timerQueryDesc.Count * sizeof(UINT64), 64, &m_TimerReadbackBuffer);

    if(m_TimerReadbackBuffer && m_TimerQueryHeap)
    {
      m_Timestamps = (uint64_t *)m_TimerReadbackBuffer->Map();
      for(uint32_t i = 0; i < timerQueryDesc.Count; i += 2)
        m_FreeQueries.push_back(i);
    }

    m_wrappedDevice->GetQueue()->GetTimestampFrequency(&m_TimerFrequency);
  }

  if(!m_FreeQueries.empty())
    return m_FreeQueries.takeAt(m_FreeQueries.size() - 1);
  return ~0U;
}

void D3D12RTManager::AddDispatchTimer(uint32_t q)
{
  // could track this maybe, for now drop it on the floor
  if(q == ~0U)
    return;

  uint64_t *timestamps = m_Timestamps + q;

  {
    SCOPED_LOCK(m_TimerStatsLock);
    m_AccumulatedStats.dispatches++;
    m_AccumulatedStats.totalDispatchesMS +=
        ((timestamps[1] - timestamps[0]) / double(m_TimerFrequency)) * 1024.0;
    m_FreeQueries.push_back(q);
  }
}

void D3D12RTManager::AddBuildTimer(uint32_t q, uint64_t size)
{
  if(q == ~0U)
    return;

  uint64_t *timestamps = m_Timestamps + q;

  {
    SCOPED_LOCK(m_TimerStatsLock);
    m_AccumulatedStats.builds++;
    m_AccumulatedStats.buildBytes += size;
    m_AccumulatedStats.totalBuildMS +=
        ((timestamps[1] - timestamps[0]) / double(m_TimerFrequency)) * 1024.0;
    m_FreeQueries.push_back(q);
  }
}

void D3D12RTManager::AddPendingASBuilds(ID3D12Fence *fence, UINT64 waitValue,
                                        const rdcarray<std::function<bool()>> &callbacks)
{
  SCOPED_LOCK(m_PendingASBuildsLock);
  for(const std::function<bool()> &cb : callbacks)
  {
    fence->AddRef();
    m_PendingASBuilds.push_back({fence, waitValue, cb});
  }
}

void D3D12RTManager::TickASManagement()
{
  CheckPendingASBuilds();
  CheckASCaching();
}

FILE *OpenCacheFile()
{
  rdcstr filename = StringFormat::Fmt(
      "%s/rdoc_as_%llu_%llu.bin", get_dirname(RenderDoc::Inst().GetCaptureFileTemplate()).c_str(),
      Timing::GetTick(), Threading::GetCurrentID());
  FILE *file = FileIO::OpenTransientFileHandle(filename, FileIO::OverwriteBinary);
  if(!file)
  {
    FileIO::CreateParentDirectory(filename);
    file = FileIO::OpenTransientFileHandle(filename, FileIO::OverwriteBinary);
  }

  return file;
}

DiskCachedAS D3D12RTManager::AllocDiskCache(uint64_t byteSize)
{
  DiskCachedAS ret;
  ret.size = byteSize;

  uint64_t blocksNeeded = AlignUp(byteSize, DiskCacheFile::blockSize) / DiskCacheFile::blockSize;

  if(blocksNeeded >= DiskCacheFile::blocksInFile)
    RDCWARN("disk cache sized insufficiently for allocation %llu, allocating dedicated file",
            byteSize);

  if(blocksNeeded < DiskCacheFile::blocksInFile)
  {
    SCOPED_LOCK(m_DiskCacheLock);

    for(size_t i = 0; i < m_DiskCache.size(); i++)
    {
      DiskCacheFile &diskCache = m_DiskCache[i];

      uint64_t firstBlock = ~0U;
      uint64_t blocksFree = 0;
      for(uint64_t b = 0; b < (uint64_t)ARRAY_COUNT(DiskCacheFile::blocksUsed); b++)
      {
        if(b + blocksNeeded > ARRAY_COUNT(DiskCacheFile::blocksUsed))
          break;

        if(diskCache.blocksUsed[b])
        {
          blocksFree = 0;
          firstBlock = ~0U;
        }
        else
        {
          blocksFree++;
          if(firstBlock == ~0U)
            firstBlock = b;

          if(blocksFree == blocksNeeded)
            break;
        }
      }

      if(blocksFree == blocksNeeded)
      {
        ret.fileIndex = i;
        ret.offset = firstBlock * DiskCacheFile::blockSize;
        memset(&m_DiskCache[i].blocksUsed[firstBlock], 1, size_t(blocksNeeded * sizeof(bool)));
        return ret;
      }
    }
  }

  // if this was oversized it will allow silent writing off the end by the user
  {
    SCOPED_LOCK(m_DiskCacheLock);
    FILE *f = OpenCacheFile();
    DiskCacheFile cache;
    cache.file = f;

    // if this was an outsized file, only reset the first N blocks for our tracking. The file will
    // be larger forever, but that's fine
    if(blocksNeeded > DiskCacheFile::blocksInFile)
      blocksNeeded = DiskCacheFile::blocksInFile;

    memset(cache.blocksUsed, 1, size_t(blocksNeeded * sizeof(bool)));

    {
      SCOPED_LOCK(m_DiskCacheLock);
      ret.fileIndex = m_DiskCache.size();
      m_DiskCache.push_back(cache);
    }

    return ret;
  }
}

void D3D12RTManager::ReleaseDiskCache(DiskCachedAS diskCache)
{
  uint64_t blocksNeeded =
      AlignUp(diskCache.size, DiskCacheFile::blockSize) / DiskCacheFile::blockSize;
  uint64_t blockOffset = diskCache.offset / DiskCacheFile::blockSize;

  // if this was an outsized file, only reset the first N blocks for our tracking. The file will be
  // larger forever, but that's fine
  if(blocksNeeded > DiskCacheFile::blocksInFile)
    blocksNeeded = DiskCacheFile::blocksInFile;

  if(diskCache.fileIndex >= m_DiskCache.size())
  {
    RDCERR("Invalid disk cache file %zu vs %zu", diskCache.fileIndex, m_DiskCache.size());
    return;
  }

  memset(&m_DiskCache[diskCache.fileIndex].blocksUsed[blockOffset], 0,
         size_t(blocksNeeded * sizeof(bool)));
}

void D3D12RTManager::FillDiskCache(DiskCachedAS diskCache, void *data)
{
  FileIO::fseek64(m_DiskCache[diskCache.fileIndex].file, diskCache.offset, SEEK_SET);

  StreamWriter writer(m_DiskCache[diskCache.fileIndex].file, Ownership::Nothing);
  // de-interleave positions in geoms here if their stride is greater than vertex format to save space?
  writer.Write(data, diskCache.size);
}

void D3D12RTManager::PushDiskCacheTask(std::function<void()> task)
{
  {
    SCOPED_LOCK(m_ASCacheThreadLock);

    // create thread lazily if we need it and it's not already here
    if(!m_ASCacheThread)
    {
      m_ASCacheThreadRunning = 1;
      m_ASCacheThreadSemaphore = Threading::Semaphore::Create();

      m_ASCacheThread = Threading::CreateThread([this]() {
        // loop until we shut down
        while(Atomic::CmpExch32(&m_ASCacheThreadRunning, 1, 1) == 1)
        {
          // wait until there's a work item (may be multiple, we process one per loop and may not
          // actually block here if there's more work waiting)
          m_ASCacheThreadSemaphore->WaitForWake();

          // grab a task
          std::function<void()> task;
          {
            SCOPED_LOCK(m_ASCacheThreadLock);
            if(!m_ASCacheTasks.empty())
            {
              task = m_ASCacheTasks.front();
              m_ASCacheTasks.erase(0);

              // consider ourselves active as soon as we have the task before releasing the lock so
              // there is no period where both the queue is empty and we're not active, but a task is in flight
              Atomic::Inc32(&m_ASCacheThreadActive);
            }
          }

          // run it if we got one
          if(task)
          {
            task();
            Atomic::Dec32(&m_ASCacheThreadActive);
          }
        }
      });
    }

    // push a task
    m_ASCacheTasks.push_back(task);
  }

  // wake the thread to do one bit of work
  m_ASCacheThreadSemaphore->Wake(1);
}

void D3D12RTManager::FlushDiskCacheThread()
{
  if(!m_ASCacheThread)
    return;

  m_ASCacheThreadSemaphore->Wake(1);

  // just spin until the thread is done
  while(true)
  {
    SCOPED_LOCK(m_ASCacheThreadLock);
    // there may be a task running on the thread even if the queue is empty, ensure the active flag is not set
    if(m_ASCacheTasks.empty() && Atomic::CmpExch32(&m_ASCacheThreadActive, 0, 0) == 0)
      return;
  }
}

void D3D12RTManager::CheckASCaching()
{
  double now = m_Timestamp.GetMilliseconds();

  SCOPED_LOCK(m_ASBuildDataLock);

  const uint32_t ageThreshold = D3D12_Debug_RT_ASCacheThreshold();
  const size_t maxCacheBatch = D3D12_Debug_RT_ASCacheBatchSize();

  // see if any AS builds are finished and old enough that we should flush them to disk.
  // to avoid doing too much work at a time we do these in batches of up to N. They're pushed in
  // order so the first one is oldest. We don't care too much about completion (there may be a
  // slight gap between record/create and submission, but that will be dominated by the time
  // between submission and it being old enough) but we don't want an AS which is built but never
  // submitted or destroyed and stays potential forever to block caching, so we skip over any such
  // ASs and start from the first old-enough AS.
  size_t first = ~0U;
  for(size_t i = 0; i < m_InMemASBuildDatas.size(); i++)
  {
    ASBuildData *buildData = m_InMemASBuildDatas[i];

    uint32_t age = uint32_t(now - buildData->timestamp);

    // if we encounter one that is too young, bail out as all later ones will be too young as well
    if(age < ageThreshold)
      break;

    // skip any that are somehow old enough but not complete
    if(!buildData->IsWorkComplete())
      continue;

    // this build is both complete and old enough, store
    first = i;
  }

  // if we didn't find one at all, stop now.
  if(first == ~0U)
    return;

  // the build data at [first] is both old enough to be cached and complete! we take a few more -
  // up to a small batch at a time.
  size_t last;
  for(last = first; last < m_InMemASBuildDatas.size() && last < first + maxCacheBatch; last++)
  {
    ASBuildData *buildData = m_InMemASBuildDatas[last];

    uint32_t age = uint32_t(now - buildData->timestamp);

    // as soon as we find a build which is either too new or not complete, we're finished.
    if(age < ageThreshold || !buildData->IsWorkComplete())
    {
      // decrement last now so that it is inclusive of the range. We know [first] will have passed
      // because it can only have gotten older
      last--;
      break;
    }
  }

  // if the whole list was old then last could be pointing off the end
  if(last == m_InMemASBuildDatas.size())
    last--;

  // whether there were more to batch or not, last is the last element (and may be equal to first)

  for(size_t i = first; i <= last; i++)
  {
    ASBuildData *buildData = m_InMemASBuildDatas[i];

    // grab parameters for the task
    D3D12GpuBuffer *buf = buildData->buffer;

    // immediately update the build data as if the cache has completed. We flush the thread
    DiskCachedAS diskCache = buildData->diskCache = AllocDiskCache(buildData->buffer->Size());
    buildData->buffer = NULL;

    if(D3D12_Debug_RT_Auditing())
    {
      RDCDEBUG("Flushing AS build data of size %llu to disk", buf->Size());
    }

    PushDiskCacheTask([this, diskCache, buf]() {
      // de-interleave positions in geoms here if their stride is greater than vertex format to save space?

      FillDiskCache(diskCache, buf->Map());

      buf->Unmap();
      buf->Release();
    });

    m_DiskCachedASBuildDatas.push_back(buildData);
  }

  // remove the build datas that we've processed
  m_InMemASBuildDatas.erase(first, last - first + 1);
}

void D3D12RTManager::CheckPendingASBuilds()
{
  std::map<ID3D12Fence *, UINT64> fenceValues;
  SCOPED_LOCK(m_PendingASBuildsLock);

  if(m_PendingASBuilds.empty())
    return;

  for(size_t i = 0; i < m_PendingASBuilds.size(); i++)
  {
    PendingASBuild &build = m_PendingASBuilds[i];

    // first time we see each fence, get the completed value
    if(fenceValues[build.fence] == 0)
      fenceValues[build.fence] = build.fence->GetCompletedValue();

    // if this fence has been satisfied, release our ref (so it will also get removed below) and call the callback
    if(fenceValues[build.fence] >= build.fenceValue)
    {
      SAFE_RELEASE(build.fence);
      build.callback();

      // swap with last if this isn't already last - we don't need to keep order
      if(i < m_PendingASBuilds.size() - 1)
        std::swap(build, m_PendingASBuilds.back());
      m_PendingASBuilds.pop_back();
    }
  }
}

void D3D12RTManager::GatherRTStatistics(ASStats &blasAges, ASStats &tlasAges,
                                        RTGPUPatchingStats &gpuStats)
{
  double now = m_Timestamp.GetMilliseconds();

  gpuStats = m_AccumulatedStats;
  m_AccumulatedStats = {};

  SCOPED_LOCK(m_ASBuildDataLock);

  blasAges.bucket[0].msThreshold = tlasAges.bucket[0].msThreshold = 50;
  blasAges.bucket[1].msThreshold = tlasAges.bucket[1].msThreshold = 250;
  blasAges.bucket[2].msThreshold = tlasAges.bucket[2].msThreshold = 2000;
  blasAges.bucket[3].msThreshold = tlasAges.bucket[3].msThreshold = ~0U;

  for(ASBuildData *buildData : m_DiskCachedASBuildDatas)
  {
    if(buildData && buildData->diskCache.Valid())
    {
      ASStats &ages = buildData->Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL
                          ? tlasAges
                          : blasAges;

      ages.diskBytes += buildData->diskCache.size;
      ages.diskCached++;
    }
  }

  for(ASBuildData *buildData : m_InMemASBuildDatas)
  {
    if(buildData)
    {
      uint32_t age = uint32_t(now - buildData->timestamp);

      ASStats &ages = buildData->Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL
                          ? tlasAges
                          : blasAges;

      // should never encounter this
      if(buildData->diskCache.Valid())
        continue;

      uint64_t size = buildData->buffer ? buildData->buffer->Size() : 0;

      ages.overheadBytes += buildData->bytesOverhead;

      for(size_t i = 0; i < ARRAY_COUNT(tlasAges.bucket); i++)
      {
        if(age <= ages.bucket[i].msThreshold)
        {
          ages.bucket[i].count++;
          ages.bucket[i].bytes += size;
          break;
        }
      }
    }
  }
}

PatchedRayDispatch D3D12RTManager::PatchRayDispatch(ID3D12GraphicsCommandList4 *unwrappedCmd,
                                                    rdcarray<ResourceId> heaps,
                                                    const D3D12_DISPATCH_RAYS_DESC &desc)
{
  PatchedRayDispatch ret = {};

  ret.desc = desc;
  ret.heaps = heaps;

  D3D12MarkerRegion region(unwrappedCmd, "PatchRayDispatch");

  ret.resources.query = GetFreeQuery();

  if(ret.resources.query != ~0U)
  {
    unwrappedCmd->EndQuery(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, ret.resources.query);
  }

  PrepareRayDispatchBuffer(NULL);

  D3D12GpuBuffer *scratchBuffer = NULL;

  RayDispatchPatchCB cbufferData = {};

  uint32_t patchDataSize = 0;

  const uint32_t raygenOffs = patchDataSize;
  patchDataSize = (uint32_t)desc.RayGenerationShaderRecord.SizeInBytes;
  patchDataSize = AlignUp(patchDataSize, 256U);

  const uint32_t missOffs = patchDataSize;
  patchDataSize += (uint32_t)desc.MissShaderTable.SizeInBytes;
  patchDataSize = AlignUp(patchDataSize, 256U);

  const uint32_t hitOffs = patchDataSize;
  patchDataSize += (uint32_t)desc.HitGroupTable.SizeInBytes;
  patchDataSize = AlignUp(patchDataSize, 256U);

  // use 256 to match the indirect patching in raytracing.hlsl
  RDCCOMPILE_ASSERT(256U >= D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                    "expect 256 to satisfy shader table alignment");

  const uint32_t callOffs = patchDataSize;
  patchDataSize += (uint32_t)desc.CallableShaderTable.SizeInBytes;

  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                             D3D12GpuBufferHeapMemoryFlag::Default, patchDataSize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &scratchBuffer);

  ret.resources.readbackBuffer = NULL;

  if(IsReplayMode(m_wrappedDevice->GetState()) && D3D12_Debug_RT_Auditing())
  {
    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::ReadBackHeap,
                               D3D12GpuBufferHeapMemoryFlag::Default, patchDataSize * 2,
                               D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                               &ret.resources.readbackBuffer);
  }

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

  unwrappedCmd->SetPipelineState(m_RayPatchingData.shaderTablePatchPipe);
  unwrappedCmd->SetComputeRootSignature(m_RayPatchingData.shaderTablePatchRootSig);
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

    if(ret.resources.readbackBuffer)
    {
      CopyFromVA(unwrappedCmd, ret.resources.readbackBuffer->Resource(),
                 ret.resources.readbackBuffer->Offset() + raygenOffs,
                 ret.desc.RayGenerationShaderRecord.StartAddress,
                 desc.RayGenerationShaderRecord.SizeInBytes);
    }

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

    if(ret.resources.readbackBuffer)
    {
      CopyFromVA(unwrappedCmd, ret.resources.readbackBuffer->Resource(),
                 ret.resources.readbackBuffer->Offset() + missOffs,
                 ret.desc.MissShaderTable.StartAddress, desc.MissShaderTable.SizeInBytes);
    }

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

    if(ret.resources.readbackBuffer)
    {
      CopyFromVA(unwrappedCmd, ret.resources.readbackBuffer->Resource(),
                 ret.resources.readbackBuffer->Offset() + hitOffs,
                 ret.desc.HitGroupTable.StartAddress, desc.HitGroupTable.SizeInBytes);
    }

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

    if(ret.resources.readbackBuffer)
    {
      CopyFromVA(unwrappedCmd, ret.resources.readbackBuffer->Resource(),
                 ret.resources.readbackBuffer->Offset() + callOffs,
                 ret.desc.CallableShaderTable.StartAddress, desc.CallableShaderTable.SizeInBytes);
    }

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
  barrier.Transition.StateAfter =
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_COPY_SOURCE;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  if(ret.resources.readbackBuffer)
  {
    unwrappedCmd->CopyBufferRegion(ret.resources.readbackBuffer->Resource(),
                                   ret.resources.readbackBuffer->Offset() + patchDataSize,
                                   scratchBuffer->Resource(), scratchBuffer->Offset(), patchDataSize);
  }

  // we have our own ref, the patch data has its ref too that will be held while the list is
  // submittable. Each submission will also get a ref to keep this referenced lookup buffer alive until then
  m_LookupBuffer->AddRef();
  ret.resources.lookupBuffer = m_LookupBuffer;

  // the patch buffer is not owned by us, so the refcounting is the same as above but it takes the
  // ref we had when we created it.
  ret.resources.patchScratchBuffer = scratchBuffer;

  ret.resources.argumentBuffer = NULL;

  if(ret.resources.query != ~0U)
  {
    unwrappedCmd->EndQuery(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, ret.resources.query + 1);
    unwrappedCmd->ResolveQueryData(
        m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, ret.resources.query, 2,
        m_TimerReadbackBuffer->Resource(),
        m_TimerReadbackBuffer->Offset() + sizeof(uint64_t) * ret.resources.query);
  }

  return ret;
}

PatchedRayDispatch D3D12RTManager::PatchIndirectRayDispatch(
    ID3D12GraphicsCommandList *unwrappedCmd, rdcarray<ResourceId> heaps,
    ID3D12CommandSignature *pCommandSignature, UINT &MaxCommandCount, ID3D12Resource *pArgumentBuffer,
    UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset)
{
  PatchedRayDispatch ret = {};

  D3D12MarkerRegion region(unwrappedCmd, "PatchIndirectRayDispatch");

  PrepareRayDispatchBuffer(NULL);

  D3D12GpuBuffer *scratchBuffer = NULL;

  // :( some games have fixed sizes, so any other estimate could fail.
  // games without fixed sizes would be reasonably served by 3 or 4 multiplied by the number of BLAS
  // in the largest TLAS, multiplied by the largest local root signature size.
  //
  // minimum of 64 bytes to account for MaxCommandCount so that we don't allocate 0-sized buffers
  uint32_t patchDataSize = GuessTablePatchRequirements(MaxCommandCount);

  WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)pCommandSignature;

  D3D12GpuBuffer *argsBuffer = NULL;

  // the args buffer contains both patched arguments for the application as well as patched
  // arguments & count for our indirect patching

  uint64_t applicationArgsSize = AlignUp16(MaxCommandCount * comSig->sig.ByteStride);
  uint64_t patchingArgsSize = AlignUp16(MaxCommandCount * 4 * AlignUp16(sizeof(PatchingExecute)));

  ret.MaxCommands = MaxCommandCount;
  ret.comSig = comSig;
  ret.HasDynamicCount = pCountBuffer != NULL;

  uint64_t totalArgsSize =
      // unwrapped exec args
      applicationArgsSize +
      // unwrapping internal exec
      patchingArgsSize +
      // count
      4;
  uint64_t totalScratchSize = patchDataSize;

  uint64_t auditCopyArgsOffset = totalArgsSize;

  RDCCOMPILE_ASSERT(WRAPPED_DESCRIPTOR_STRIDE == sizeof(D3D12Descriptor),
                    "Shader descriptor stride is wrong");

  ret.resources.readbackBuffer = NULL;

  if(IsReplayMode(m_wrappedDevice->GetState()) && D3D12_Debug_RT_Auditing())
  {
    m_GPUBufferAllocator.Alloc(
        D3D12GpuBufferHeapType::ReadBackHeap, D3D12GpuBufferHeapMemoryFlag::Default,
        // unpatched records
        patchDataSize +
            // patched records
            patchDataSize +
            // app exec args + optional count
            (applicationArgsSize + 4) +
            // patched exec args + internal args + count
            applicationArgsSize + patchingArgsSize + 4,
        D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &ret.resources.readbackBuffer);

    // double the args we need internally to make room for a second internal indirect dispatch to
    // copy the unwrapped arguments
    totalArgsSize *= 2;

    // we can't write directly to our readback buffer with our indirect copy, need to copy into scratch first
    totalScratchSize *= 2;
  }

  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                             D3D12GpuBufferHeapMemoryFlag::Default, totalScratchSize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &scratchBuffer);

  m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                             D3D12GpuBufferHeapMemoryFlag::Default, totalArgsSize,
                             D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, &argsBuffer);

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
  prepInfo.destBuffer = scratchBuffer->Address();
  prepInfo.destBufferEnd = scratchBuffer->Address() + patchDataSize;

  if(pCountBuffer == NULL)
    prepInfo.maxCommandCount |= 0x80000000U;

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = Unwrap(pArgumentBuffer);
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  unwrappedCmd->ResourceBarrier(1, &barrier);

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

  // if we're reading back, do another dispatch to write args which just purely copy
  if(ret.resources.readbackBuffer)
  {
    prepInfo.destBuffer = scratchBuffer->Address() + patchDataSize;
    prepInfo.destBufferEnd = scratchBuffer->Address() + patchDataSize * 2;
    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12IndirectPrepParam::GeneralCB,
                                               sizeof(prepInfo) / sizeof(uint32_t), &prepInfo, 0);

    // unused, we don't need this but it keeps the shader the same
    unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12IndirectPrepParam::PatchedExecuteArgs,
                                                    argsBuffer->Address() + auditCopyArgsOffset);
    // this will be the arguments for our internal copy-dispatch
    unwrappedCmd->SetComputeRootUnorderedAccessView(
        (UINT)D3D12IndirectPrepParam::InternalExecuteArgs,
        argsBuffer->Address() + auditCopyArgsOffset + applicationArgsSize);
    unwrappedCmd->SetComputeRootUnorderedAccessView(
        (UINT)D3D12IndirectPrepParam::InternalExecuteCount,
        argsBuffer->Address() + auditCopyArgsOffset + applicationArgsSize + patchingArgsSize);

    unwrappedCmd->Dispatch(1, 1, 1);
  }

  barrier.Transition.pResource = Unwrap(pArgumentBuffer);
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  // this is ready for the application to use (once we patch the things it refers to), and for us to use below
  barrier.Transition.pResource = argsBuffer->Resource();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  unwrappedCmd->SetPipelineState(m_RayPatchingData.shaderTablePatchPipe);
  unwrappedCmd->SetComputeRootSignature(m_RayPatchingData.shaderTablePatchRootSig);
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
  barrier.Transition.StateAfter =
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_COPY_SOURCE;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  if(ret.resources.readbackBuffer)
  {
    barrier.Transition.pResource = Unwrap(pArgumentBuffer);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    unwrappedCmd->ResourceBarrier(1, &barrier);

    uint64_t offset = 0;

    unwrappedCmd->SetPipelineState(m_RayPatchingData.shaderTableCopyPipe);
    unwrappedCmd->SetComputeRootSignature(m_RayPatchingData.shaderTablePatchRootSig);
    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::GeneralCB,
                                               sizeof(cbufferData) / sizeof(uint32_t), &cbufferData,
                                               0);
    unwrappedCmd->SetComputeRootShaderResourceView(
        (UINT)D3D12PatchRayDispatchParam::StateObjectData, m_LookupAddrs[0]);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::RecordData,
                                                   m_LookupAddrs[1]);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::RootSigData,
                                                   m_LookupAddrs[2]);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::AddrPatchData,
                                                   m_LookupAddrs[3]);

    // these will be overwritten by the execute indirect, but set them to something to be safe
    unwrappedCmd->SetComputeRoot32BitConstants((UINT)D3D12PatchRayDispatchParam::RecordCB,
                                               sizeof(recordInfo) / sizeof(uint32_t), &recordInfo, 0);
    unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12PatchRayDispatchParam::SourceBuffer,
                                                   m_LookupAddrs[0]);
    unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12PatchRayDispatchParam::DestBuffer,
                                                    scratchBuffer->Address());

    unwrappedCmd->ExecuteIndirect(
        m_RayPatchingData.indirectComSig, MaxCommandCount * 4, argsBuffer->Resource(),
        argsBuffer->Offset() + auditCopyArgsOffset + applicationArgsSize, argsBuffer->Resource(),
        argsBuffer->Offset() + auditCopyArgsOffset + applicationArgsSize + patchingArgsSize);

    // unpatched data
    unwrappedCmd->CopyBufferRegion(
        ret.resources.readbackBuffer->Resource(), ret.resources.readbackBuffer->Offset() + offset,
        scratchBuffer->Resource(), scratchBuffer->Offset() + patchDataSize, patchDataSize);
    offset += patchDataSize;

    // patched data
    unwrappedCmd->CopyBufferRegion(
        ret.resources.readbackBuffer->Resource(), ret.resources.readbackBuffer->Offset() + offset,
        scratchBuffer->Resource(), scratchBuffer->Offset(), patchDataSize);
    offset += patchDataSize;

    // unpatched execute args
    unwrappedCmd->CopyBufferRegion(ret.resources.readbackBuffer->Resource(),
                                   ret.resources.readbackBuffer->Offset() + patchDataSize * 2,
                                   Unwrap(pArgumentBuffer), ArgumentBufferOffset,
                                   applicationArgsSize);
    offset += applicationArgsSize;

    // source count
    if(pCountBuffer)
    {
      unwrappedCmd->CopyBufferRegion(ret.resources.readbackBuffer->Resource(),
                                     ret.resources.readbackBuffer->Offset() + offset,
                                     Unwrap(pCountBuffer), CountBufferOffset, applicationArgsSize);
    }
    offset += 4;

    barrier.Transition.pResource = argsBuffer->Resource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    unwrappedCmd->ResourceBarrier(1, &barrier);

    // patched execute args, our internal executes, count
    unwrappedCmd->CopyBufferRegion(
        ret.resources.readbackBuffer->Resource(), ret.resources.readbackBuffer->Offset() + offset,
        argsBuffer->Resource(), argsBuffer->Offset(), applicationArgsSize + patchingArgsSize + 4);
    offset += applicationArgsSize + patchingArgsSize + 4;

    RDCASSERTEQUAL(offset, ret.resources.readbackBuffer->Size());

    barrier.Transition.pResource = argsBuffer->Resource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    unwrappedCmd->ResourceBarrier(1, &barrier);

    barrier.Transition.pResource = Unwrap(pArgumentBuffer);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    unwrappedCmd->ResourceBarrier(1, &barrier);
  }

  // we have our own ref, the patch data has its ref too that will be held while the list is
  // submittable. Each submission will also get a ref to keep this referenced lookup buffer alive until then
  m_LookupBuffer->AddRef();
  ret.resources.lookupBuffer = m_LookupBuffer;

  // the patch and arguments buffers are not owned by us, so the refcounting is the same as above
  // but it takes the ref we had when we created it.
  ret.resources.patchScratchBuffer = scratchBuffer;
  ret.resources.argumentBuffer = argsBuffer;

  // TODO: Verify for indirect executes

  if(IsReplayMode(m_wrappedDevice->GetState()) && D3D12_Debug_RT_Auditing())
  {
    // only one execute is allowed per command signature, if it's ray tracing and we're auditing then turn it off
    MaxCommandCount = 0;
  }

  return ret;
}

void D3D12RTManager::PrepareRayDispatchBuffer(GPUAddressRangeTracker *origAddresses)
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

    rdcarray<GPUAddressRange> addresses;

    const size_t PatchAddrOffset = lookupData.size();
    if(origAddresses)
    {
      addresses = origAddresses->GetAddresses();
      lookupData.resize(lookupData.size() + sizeof(BlasAddressPair) * addresses.size());
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

    for(size_t i = 0; origAddresses && i < addresses.size(); i++)
    {
      GPUAddressRange addressRange = addresses[i];
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

ASBuildData *D3D12RTManager::CopyBuildInputs(
    ID3D12GraphicsCommandList4 *unwrappedCmd,
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &inputs)
{
  ASBuildData *ret = new ASBuildData;
  ret->Type = inputs.Type;
  ret->Flags = inputs.Flags;
  ret->timestamp = m_Timestamp.GetMilliseconds();
  ret->rtManager = this;

  ret->query = GetFreeQuery();

  if(ret->query != ~0U)
  {
    unwrappedCmd->EndQuery(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, ret->query);
  }

  if(inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
  {
    ret->NumBLAS = inputs.NumDescs;

    if(ret->NumBLAS > 0)
    {
      uint64_t byteSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * inputs.NumDescs;
      m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::ReadBackHeap,
                                 D3D12GpuBufferHeapMemoryFlag::Default, byteSize, 256, &ret->buffer);

      if(inputs.DescsLayout == D3D12_ELEMENTS_LAYOUT_ARRAY)
      {
        // easy case, one copy of instances

        ResourceId sourceBufferId;
        D3D12BufferOffset sourceOffset;

        WrappedID3D12Resource::GetResIDFromAddr(inputs.InstanceDescs, sourceBufferId, sourceOffset);
        ID3D12Resource *sourceBuffer = Unwrap(
            m_wrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(sourceBufferId));

        unwrappedCmd->CopyBufferRegion(ret->buffer->Resource(), ret->buffer->Offset(), sourceBuffer,
                                       sourceOffset, byteSize);
      }
      else
      {
        // hard case :( instance pointers, need to do indirected copy with compute
        // this is extra hard because all the indirect pointers are inaccessible from shaders so we
        // need to shunt through an ExecuteIndirect to use them as root SRVs. That also means that
        // we need to first do a pre-pass to prepare the indirect argument buffer from
        // {BLASDescAddr, BLASDescAddr, BlasDescAddr...} to
        // {BLASDescAddr, dispatch(1,1,1), BLASDescAddr, dispatch(1,1,1), ...}

        const uint64_t unpackedLayoutSize = byteSize;

        if(m_TLASCopyingData.ScratchBuffer == NULL ||
           m_TLASCopyingData.ScratchBuffer->Size() < unpackedLayoutSize)
        {
          m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                                     D3D12GpuBufferHeapMemoryFlag::Default, unpackedLayoutSize,
                                     D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                                     &m_TLASCopyingData.ScratchBuffer);
        }

        UnrollBLASInstancesList(unwrappedCmd, inputs, 0, 0, m_TLASCopyingData.ScratchBuffer);

        // copy to readback buffer (can't write to it directly)
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Transition.pResource = m_TLASCopyingData.ScratchBuffer->Resource();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        unwrappedCmd->ResourceBarrier(1, &barrier);

        unwrappedCmd->CopyBufferRegion(ret->buffer->Resource(), ret->buffer->Offset(),
                                       m_TLASCopyingData.ScratchBuffer->Resource(),
                                       m_TLASCopyingData.ScratchBuffer->Offset(), unpackedLayoutSize);

        // keep these buffer around until the parent cmd executes even if we reallocate soon
        m_TLASCopyingData.ArgsBuffer->AddRef();
        m_TLASCopyingData.ScratchBuffer->AddRef();
        ret->cleanupCallback = [this]() {
          m_TLASCopyingData.ArgsBuffer->Release();
          m_TLASCopyingData.ScratchBuffer->Release();
          return true;
        };
      }
    }
  }
  else
  {
    ret->NumBLAS = 0;

    if(inputs.DescsLayout == D3D12_ELEMENTS_LAYOUT_ARRAY)
    {
      ret->geoms.assign((ASBuildData::RTGeometryDesc *)inputs.pGeometryDescs, inputs.NumDescs);
    }
    else
    {
      ret->geoms.reserve(inputs.NumDescs);
      for(UINT i = 0; i < inputs.NumDescs; i++)
        ret->geoms.push_back(*inputs.ppGeometryDescs[i]);
    }

    // calculate how much data is needed. Add 256 bytes padding
    uint64_t byteSize = 0;
    uint64_t bytesOverhead = 0;
    for(const ASBuildData::RTGeometryDesc &desc : ret->geoms)
    {
      if(desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
      {
        if(desc.AABBs.AABBCount > 0)
        {
          byteSize += (desc.AABBs.AABBCount - 1) * desc.AABBs.AABBs.StrideInBytes;
          byteSize += sizeof(D3D12_RAYTRACING_AABB);
          byteSize = AlignUp16(byteSize);

          if(desc.AABBs.AABBs.StrideInBytes > sizeof(D3D12_RAYTRACING_AABB))
            bytesOverhead += (desc.AABBs.AABBCount - 1) *
                             (desc.AABBs.AABBs.StrideInBytes - sizeof(D3D12_RAYTRACING_AABB));
        }
      }
      else
      {
        if(desc.Triangles.Transform3x4)
        {
          byteSize += sizeof(float) * 3 * 4;
          byteSize = AlignUp16(byteSize);
        }

        if(desc.Triangles.IndexBuffer)
        {
          UINT isize = 2;
          if(desc.Triangles.IndexFormat == DXGI_FORMAT_R32_UINT)
            isize = 4;
          byteSize += isize * desc.Triangles.IndexCount;
          byteSize = AlignUp16(byteSize);

          ResourceId vbId;
          uint64_t srcOffs = 0;
          WrappedID3D12Resource::GetResIDFromAddr(desc.Triangles.VertexBuffer.RVA, vbId, srcOffs);
          ID3D12Resource *sourceBuffer =
              m_wrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(vbId);

          uint64_t vbSize = sourceBuffer->GetDesc().Width - srcOffs;

          uint32_t untrustedVertexCount = desc.Triangles.VertexCount;
          uint32_t estimatedVertexCount =
              untrustedVertexCount +
              (untrustedVertexCount / 100) * D3D12_Debug_RT_MaxVertexPercentIncrease() +
              D3D12_Debug_RT_MaxVertexIncrement();

          RDCASSERT(vbSize >= desc.Triangles.VertexBuffer.StrideInBytes * untrustedVertexCount);

          vbSize = RDCMIN(vbSize, (desc.Triangles.VertexBuffer.StrideInBytes *
                                   (RDCMAX(1U, estimatedVertexCount) - 1)) +
                                      GetByteSize(1, 1, 1, desc.Triangles.VertexFormat, 0));

          byteSize += vbSize;
          byteSize = AlignUp16(byteSize);

          uint64_t tightStride = GetByteSize(1, 1, 1, desc.Triangles.VertexFormat, 0);

          if(desc.Triangles.VertexBuffer.StrideInBytes > tightStride)
          {
            bytesOverhead += vbSize - (tightStride * untrustedVertexCount);
          }
          else if(vbSize > desc.Triangles.VertexBuffer.StrideInBytes * untrustedVertexCount)
          {
            bytesOverhead +=
                vbSize - (desc.Triangles.VertexBuffer.StrideInBytes * untrustedVertexCount);
          }
        }
        else
        {
          if(desc.Triangles.VertexCount > 0)
          {
            byteSize += (desc.Triangles.VertexCount - 1) * desc.Triangles.VertexBuffer.StrideInBytes;

            uint64_t tightStride = GetByteSize(1, 1, 1, desc.Triangles.VertexFormat, 0);

            if(desc.Triangles.VertexBuffer.StrideInBytes > tightStride)
              bytesOverhead += (desc.Triangles.VertexCount - 1) *
                               (desc.Triangles.VertexBuffer.StrideInBytes - tightStride);

            byteSize += tightStride;
            byteSize = AlignUp16(byteSize);
          }
        }
      }
    }

    ret->bytesOverhead = bytesOverhead;

    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::ReadBackHeap,
                               D3D12GpuBufferHeapMemoryFlag::Default, byteSize, 256, &ret->buffer);

    if(!ret->buffer)
    {
      RDCERR("Failed to allocate shadow storage for AS");
      ret = {};
      return ret;
    }

    const uint64_t allocedByteSize = byteSize;

    ID3D12Resource *dstRes = ret->buffer->Resource();
    uint64_t dstOffset = ret->buffer->Offset();
    uint64_t baseOffset = dstOffset;

    for(ASBuildData::RTGeometryDesc &desc : ret->geoms)
    {
      if(desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
      {
        if(desc.AABBs.AABBCount > 0)
        {
          byteSize = (desc.AABBs.AABBCount - 1) * desc.AABBs.AABBs.StrideInBytes;
          byteSize += sizeof(D3D12_RAYTRACING_AABB);

          CopyFromVA(unwrappedCmd, dstRes, dstOffset, desc.AABBs.AABBs.RVA, byteSize);

          desc.AABBs.AABBs.RVA = dstOffset - baseOffset;
          RDCASSERT(desc.AABBs.AABBs.RVA + byteSize <= allocedByteSize, desc.AABBs.AABBs.RVA,
                    byteSize, allocedByteSize);

          dstOffset = AlignUp16(dstOffset + byteSize);
        }
        else
        {
          // set a NULL marker so that we don't confuse an offset of 0 with an intended NULL
          desc.AABBs.AABBs.RVA = ASBuildData::NULLVA;
        }
      }
      else
      {
        if(desc.Triangles.Transform3x4)
        {
          byteSize = sizeof(float) * 3 * 4;

          CopyFromVA(unwrappedCmd, dstRes, dstOffset, desc.Triangles.Transform3x4, byteSize);

          desc.Triangles.Transform3x4 = dstOffset - baseOffset;
          RDCASSERT(desc.Triangles.Transform3x4 + byteSize <= allocedByteSize,
                    desc.Triangles.Transform3x4, byteSize, allocedByteSize);

          dstOffset = AlignUp16(dstOffset + byteSize);
        }
        else
        {
          desc.Triangles.Transform3x4 = ASBuildData::NULLVA;
        }

        if(desc.Triangles.IndexBuffer)
        {
          UINT isize = 2;
          if(desc.Triangles.IndexFormat == DXGI_FORMAT_R32_UINT)
            isize = 4;
          byteSize = isize * desc.Triangles.IndexCount;

          CopyFromVA(unwrappedCmd, dstRes, dstOffset, desc.Triangles.IndexBuffer, byteSize);

          desc.Triangles.IndexBuffer = dstOffset - baseOffset;
          RDCASSERT(desc.Triangles.IndexBuffer + byteSize <= allocedByteSize,
                    desc.Triangles.IndexBuffer, byteSize, allocedByteSize);

          dstOffset = AlignUp16(dstOffset + byteSize);

          ResourceId vbId;
          uint64_t srcOffs = 0;
          WrappedID3D12Resource::GetResIDFromAddr(desc.Triangles.VertexBuffer.RVA, vbId, srcOffs);
          ID3D12Resource *sourceBuffer =
              m_wrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(vbId);

          uint64_t vbSize = sourceBuffer->GetDesc().Width - srcOffs;

          uint32_t untrustedVertexCount = desc.Triangles.VertexCount;
          uint32_t estimatedVertexCount =
              untrustedVertexCount +
              (untrustedVertexCount / 100) * D3D12_Debug_RT_MaxVertexPercentIncrease() +
              D3D12_Debug_RT_MaxVertexIncrement();

          vbSize = RDCMIN(vbSize, (desc.Triangles.VertexBuffer.StrideInBytes *
                                   (RDCMAX(1U, estimatedVertexCount) - 1)) +
                                      GetByteSize(1, 1, 1, desc.Triangles.VertexFormat, 0));

          unwrappedCmd->CopyBufferRegion(dstRes, dstOffset, Unwrap(sourceBuffer), srcOffs, vbSize);

          desc.Triangles.VertexBuffer.RVA = dstOffset - baseOffset;
          RDCASSERT(desc.Triangles.VertexBuffer.RVA + vbSize <= allocedByteSize,
                    desc.Triangles.VertexBuffer.RVA, vbSize, allocedByteSize);

          dstOffset = AlignUp16(dstOffset + vbSize);
        }
        else
        {
          desc.Triangles.IndexBuffer = ASBuildData::NULLVA;
          if(desc.Triangles.VertexCount > 0)
          {
            byteSize = (desc.Triangles.VertexCount - 1) * desc.Triangles.VertexBuffer.StrideInBytes;

            byteSize += GetByteSize(1, 1, 1, desc.Triangles.VertexFormat, 0);

            CopyFromVA(unwrappedCmd, dstRes, dstOffset, desc.Triangles.VertexBuffer.RVA, byteSize);

            desc.Triangles.VertexBuffer.RVA = dstOffset - baseOffset;
            RDCASSERT(desc.Triangles.VertexBuffer.RVA + byteSize <= allocedByteSize);

            dstOffset = AlignUp16(dstOffset + byteSize);
          }
        }
      }
    }
  }

  // ensure the copy finishes before anything changes in the input buffer
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  unwrappedCmd->ResourceBarrier(1, &barrier);

  // only bother tracking build data with a buffer attached, as without the buffer there is nothing
  // to cache and we don't care too much about missing stats for empty/degenerate ASs
  if(ret->buffer)
  {
    SCOPED_LOCK(m_ASBuildDataLock);
    m_InMemASBuildDatas.push_back(ret);
  }

  if(ret->query != ~0U)
  {
    unwrappedCmd->EndQuery(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, ret->query + 1);
    unwrappedCmd->ResolveQueryData(m_TimerQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, ret->query, 2,
                                   m_TimerReadbackBuffer->Resource(),
                                   m_TimerReadbackBuffer->Offset() + sizeof(uint64_t) * ret->query);
  }

  return ret;
}

D3D12GpuBuffer *D3D12RTManager::UnrollBLASInstancesList(
    ID3D12GraphicsCommandList4 *unwrappedCmd,
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &inputs,
    D3D12_GPU_VIRTUAL_ADDRESS addressPairResAddress, uint64_t addressCount,
    D3D12GpuBuffer *copyDestUAV)
{
  const uint64_t indirectArgSize =
      AlignUp(sizeof(TLASCopyExecute) * (uint64_t)inputs.NumDescs, 256ULL);

  if(m_TLASCopyingData.ArgsBuffer == NULL || m_TLASCopyingData.ArgsBuffer->Size() < indirectArgSize)
  {
    // needs to be dedicated so we can sure it's not shared with anything when we transition it...
    m_GPUBufferAllocator.Alloc(D3D12GpuBufferHeapType::DefaultHeapWithUav,
                               D3D12GpuBufferHeapMemoryFlag::Dedicated, indirectArgSize,
                               D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
                               &m_TLASCopyingData.ArgsBuffer);
  }

  // do a normal dispatch to set up the EI argument buffer in temporary scratch memory
  unwrappedCmd->SetPipelineState(m_TLASCopyingData.PreparePipe);
  unwrappedCmd->SetComputeRootSignature(m_TLASCopyingData.RootSig);
  unwrappedCmd->SetComputeRoot32BitConstant((UINT)D3D12TLASInstanceCopyParam::RootCB,
                                            (UINT)addressCount, 0);
  unwrappedCmd->SetComputeRootShaderResourceView((UINT)D3D12TLASInstanceCopyParam::SourceSRV,
                                                 inputs.InstanceDescs);
  unwrappedCmd->SetComputeRootShaderResourceView(
      (UINT)D3D12TLASInstanceCopyParam::RootAddressPairSrv,
      addressPairResAddress ? addressPairResAddress : inputs.InstanceDescs);
  unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12TLASInstanceCopyParam::DestUAV,
                                                  m_TLASCopyingData.ArgsBuffer->Address());
  unwrappedCmd->Dispatch(inputs.NumDescs, 1, 1);

  // make sure the argument buffer is ready
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_TLASCopyingData.ArgsBuffer->Resource();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

  unwrappedCmd->ResourceBarrier(1, &barrier);

  unwrappedCmd->SetPipelineState(m_TLASCopyingData.CopyPipe);
  unwrappedCmd->SetComputeRootSignature(m_TLASCopyingData.RootSig);
  // dummy, will be set by the EI argument
  unwrappedCmd->SetComputeRoot32BitConstant((UINT)D3D12TLASInstanceCopyParam::RootCB, 0, 0);
  unwrappedCmd->SetComputeRootShaderResourceView(
      (UINT)D3D12TLASInstanceCopyParam::RootAddressPairSrv,
      addressPairResAddress ? addressPairResAddress : inputs.InstanceDescs);
  unwrappedCmd->SetComputeRootUnorderedAccessView((UINT)D3D12TLASInstanceCopyParam::DestUAV,
                                                  copyDestUAV->Address());
  // the EI takes care of both setting the source SRV and the index constant
  unwrappedCmd->ExecuteIndirect(m_TLASCopyingData.IndirectSig, inputs.NumDescs,
                                m_TLASCopyingData.ArgsBuffer->Resource(),
                                m_TLASCopyingData.ArgsBuffer->Offset(), NULL, 0);

  return m_TLASCopyingData.ArgsBuffer;
}

void D3D12RTManager::CopyFromVA(ID3D12GraphicsCommandList4 *unwrappedCmd, ID3D12Resource *dstRes,
                                uint64_t dstOffset, D3D12_GPU_VIRTUAL_ADDRESS sourceVA,
                                uint64_t byteSize)
{
  ResourceId srcId;
  uint64_t srcOffs = 0;
  WrappedID3D12Resource::GetResIDFromAddr(sourceVA, srcId, srcOffs);
  ID3D12Resource *srcBuf = m_wrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(srcId);

  unwrappedCmd->CopyBufferRegion(dstRes, dstOffset, Unwrap(srcBuf), srcOffs, byteSize);
}

void D3D12RTManager::InitRayDispatchPatchingResources()
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

  bytebuf rootSig = EncodeRootSig(m_wrappedDevice->RootSigVersion(), rootParameters,
                                  D3D12_ROOT_SIGNATURE_FLAG_NONE);

  if(!rootSig.empty())
  {
    HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
        0, rootSig.data(), rootSig.size(), __uuidof(ID3D12RootSignature),
        (void **)&m_RayPatchingData.shaderTablePatchRootSig);

    if(!SUCCEEDED(result))
      RDCERR("Unable to create root signature for dispatch patching");

    // PipelineState
    ID3DBlob *shader = NULL;
    rdcstr hlsl = GetEmbeddedResource(raytracing_hlsl);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PatchShaderTableCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

    if(shader)
    {
      D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
      pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      pipeline.NodeMask = 0;
      pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
      pipeline.CachedPSO = {NULL, 0};
      pipeline.pRootSignature = m_RayPatchingData.shaderTablePatchRootSig;

      result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
          &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_RayPatchingData.shaderTablePatchPipe);

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for dispatch patching");

      if(m_RayPatchingData.shaderTablePatchPipe)
        m_RayPatchingData.shaderTablePatchPipe->SetName(L"RENDERDOC_PatchShaderTableCS");
    }
    else
    {
      RDCERR("Failed to get shader for record patching in dispatch");
    }

    SAFE_RELEASE(shader);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_CopyShaderTableCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

    if(shader)
    {
      D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
      pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      pipeline.NodeMask = 0;
      pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
      pipeline.CachedPSO = {NULL, 0};
      pipeline.pRootSignature = m_RayPatchingData.shaderTablePatchRootSig;

      result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
          &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_RayPatchingData.shaderTableCopyPipe);

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for dispatch patching");

      if(m_RayPatchingData.shaderTableCopyPipe)
        m_RayPatchingData.shaderTableCopyPipe->SetName(L"RENDERDOC_CopyShaderTableCS");
    }
    else
    {
      RDCERR("Failed to get shader for record copying in indirect auditing");
    }

    SAFE_RELEASE(shader);
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

  rootSig = EncodeRootSig(m_wrappedDevice->RootSigVersion(), rootParameters,
                          D3D12_ROOT_SIGNATURE_FLAG_NONE);

  if(!rootSig.empty())
  {
    HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
        0, rootSig.data(), rootSig.size(), __uuidof(ID3D12RootSignature),
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

      if(m_RayPatchingData.indirectPrepPipe)
        m_RayPatchingData.indirectPrepPipe->SetName(L"RENDERDOC_PrepareRayIndirectExecuteCS");

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for indirect execute patching");
    }
    else
    {
      RDCERR("Failed to get shader for indirect execute patching");
    }

    SAFE_RELEASE(shader);
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
        &desc, m_RayPatchingData.shaderTablePatchRootSig, __uuidof(ID3D12CommandSignature),
        (void **)&m_RayPatchingData.indirectComSig);

    if(!SUCCEEDED(hr))
      RDCERR("Unable to create command signature for indirect execute patching");
  }
}

void D3D12RTManager::InitTLASInstanceCopyingResources()
{
  D3D12ShaderCache *shaderCache = m_wrappedDevice->GetShaderCache();

  if(shaderCache == NULL)
  {
    RDCERR("Shadercache not available");
    return;
  }

  // Root Signature
  rdcarray<D3D12_ROOT_PARAMETER1> rootParameters;
  rootParameters.reserve((uint16_t)D3D12TLASInstanceCopyParam::Count);

  // used as an index in the EI, and as an address count in the prepare step
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
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    rootParameters.push_back(rootParam);
  }

  RDCASSERT(rootParameters.size() == uint32_t(D3D12TLASInstanceCopyParam::Count));

  bytebuf rootSig = EncodeRootSig(m_wrappedDevice->RootSigVersion(), rootParameters,
                                  D3D12_ROOT_SIGNATURE_FLAG_NONE);

  if(!rootSig.empty())
  {
    HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
        0, rootSig.data(), rootSig.size(), __uuidof(ID3D12RootSignature),
        (void **)&m_TLASCopyingData.RootSig);

    if(!SUCCEEDED(result))
      RDCERR("Unable to create root signature for TLAS instance copying");

    // PipelineState
    ID3DBlob *shader = NULL;
    rdcstr hlsl = GetEmbeddedResource(raytracing_hlsl);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PrepareTLASCopyIndirectExecuteCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

    if(shader)
    {
      D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
      pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      pipeline.NodeMask = 0;
      pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
      pipeline.CachedPSO = {NULL, 0};
      pipeline.pRootSignature = m_TLASCopyingData.RootSig;

      result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
          &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_TLASCopyingData.PreparePipe);

      if(m_TLASCopyingData.PreparePipe)
        m_TLASCopyingData.PreparePipe->SetName(L"RENDERDOC_PrepareTLASCopyIndirectExecuteCS");

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for TLAS instance copying");
    }
    else
    {
      RDCERR("Failed to get shader for TLAS instance copying");
    }

    SAFE_RELEASE(shader);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_CopyBLASInstanceCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &shader);

    if(shader)
    {
      D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
      pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
      pipeline.NodeMask = 0;
      pipeline.CS = {(void *)shader->GetBufferPointer(), shader->GetBufferSize()};
      pipeline.CachedPSO = {NULL, 0};
      pipeline.pRootSignature = m_TLASCopyingData.RootSig;

      result = m_wrappedDevice->GetReal()->CreateComputePipelineState(
          &pipeline, __uuidof(ID3D12PipelineState), (void **)&m_TLASCopyingData.CopyPipe);

      if(m_TLASCopyingData.CopyPipe)
        m_TLASCopyingData.CopyPipe->SetName(L"RENDERDOC_CopyBLASInstanceCS");

      if(!SUCCEEDED(result))
        RDCERR("Unable to create pipeline for TLAS instance copying");
    }
    else
    {
      RDCERR("Failed to get shader for TLAS instance copying");
    }
  }

  {
    D3D12_INDIRECT_ARGUMENT_DESC args[] = {
        {D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT},
        {D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW},
        {D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH},
    };

    args[0].Constant.DestOffsetIn32BitValues = 0;
    args[0].Constant.Num32BitValuesToSet = 1;
    args[0].Constant.RootParameterIndex = (uint32_t)D3D12TLASInstanceCopyParam::RootCB;

    args[1].ShaderResourceView.RootParameterIndex = (uint32_t)D3D12TLASInstanceCopyParam::SourceSRV;

    D3D12_COMMAND_SIGNATURE_DESC desc = {};
    desc.ByteStride = (UINT)AlignUp16(sizeof(TLASCopyExecute));
    desc.NumArgumentDescs = ARRAY_COUNT(args);
    desc.pArgumentDescs = args;

    HRESULT hr = m_wrappedDevice->GetReal()->CreateCommandSignature(
        &desc, m_TLASCopyingData.RootSig, __uuidof(ID3D12CommandSignature),
        (void **)&m_TLASCopyingData.IndirectSig);

    if(!SUCCEEDED(hr))
      RDCERR("Unable to create command signature for TLAS instance copying");
  }
}

void D3D12RTManager::InitReplayBlasPatchingResources()
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
    bytebuf rootSig = EncodeRootSig(m_wrappedDevice->RootSigVersion(), rootParameters,
                                    D3D12_ROOT_SIGNATURE_FLAG_NONE);

    if(!rootSig.empty())
    {
      HRESULT result = m_wrappedDevice->GetReal()->CreateRootSignature(
          0, rootSig.data(), rootSig.size(), __uuidof(ID3D12RootSignature),
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

        if(m_accStructPatchInfo.m_pipeline)
          m_accStructPatchInfo.m_pipeline->SetName(L"RENDERDOC_PatchAccStructAddressCS");

        if(!SUCCEEDED(result))
          RDCERR("Unable to create pipeline for patching the BLAS");
      }

      SAFE_RELEASE(shader);
    }
  }
  else
  {
    RDCERR("Shadercache not available");
  }
}

uint32_t D3D12RTManager::RegisterLocalRootSig(const D3D12RootSignature &sig)
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

void D3D12RTManager::RegisterExportDatabase(D3D12ShaderExportDatabase *db)
{
  SCOPED_LOCK(m_LookupBufferLock);
  m_ExportDatabases.push_back(db);

  m_LookupBufferDirty = true;
}

void D3D12RTManager::UnregisterExportDatabase(D3D12ShaderExportDatabase *db)
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
    : m_resource(resource), m_heapType(heapType), m_subRanges(), m_lastFree(m_subRanges.begin())

{
  if(m_resource)
  {
    m_resDesc = m_resource->GetDesc();
    m_resourceGpuAddressRange.start = resource->GetGPUVirtualAddress();
    m_resourceGpuAddressRange.realEnd = m_resourceGpuAddressRange.start + m_resDesc.Width;
    m_bytesFree = m_resDesc.Width;
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
      if(size > bufferRes->m_bytesFree)
        continue;

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
      D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
      if(newBufferResource->SubAlloc(size, alignment, gpuAddress))
      {
        *gpuBuffer = new D3D12GpuBuffer(
            allocator, m_bufferPoolHeapType, D3D12GpuBufferHeapMemoryFlag::Dedicated, size,
            D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, gpuAddress, newBufferResource->Resource());
      }
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
          if(!bufferRes->Free(gpuBuffer.Address(), gpuBuffer.Size(), gpuBuffer.Alignment()))
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
  if(size == 0)
  {
    RDCERR("Can't allocate 0-byte buffer");
    *gpuBuffer = NULL;
    return false;
  }

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

void D3D12ResourceManager::ResolveDeferredWrappers()
{
  rdcarray<ID3D12DeviceChild *> wrappers;
  for(auto it = m_WrapperMap.begin(); it != m_WrapperMap.end();)
  {
    if((uint64_t)it->first >= m_DummyHandle)
    {
      wrappers.push_back(it->second);
      it = m_WrapperMap.erase(it);
      continue;
    }

    ++it;
  }

  for(ID3D12DeviceChild *wrapper : wrappers)
    AddWrapper(wrapper, Unwrap(wrapper));
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

rdcarray<ResourceId> D3D12ResourceManager::InitialContentResources()
{
  rdcarray<ResourceId> resources =
      ResourceManager<D3D12ResourceManagerConfiguration>::InitialContentResources();
  std::sort(resources.begin(), resources.end(), [this](ResourceId a, ResourceId b) {
    const InitialContentData &aData = m_InitialContents[a].data;
    const InitialContentData &bData = m_InitialContents[b].data;

    // Always sort BLASs before TLASs, as a TLAS holds device addresses for it's BLASs
    // and we make sure those addresses are built first
    if(aData.buildData && bData.buildData)
      return aData.buildData->Type > bData.buildData->Type;

    // serialise ASs first to allow reallocation.
    // since the enum is serialised we couldn't change the order so we do this by hand
    int aASSort = aData.resourceType == Resource_AccelerationStructure ? 0 : 1;
    int bASSort = bData.resourceType == Resource_AccelerationStructure ? 0 : 1;
    if(aASSort != bASSort)
      return aASSort < bASSort;

    return aData.resourceType < bData.resourceType;
  });
  return resources;
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

void ASBuildData::MarkWorkComplete()
{
  complete = true;
  rtManager->AddBuildTimer(query, buffer ? buffer->Size() : 0);
}

void ASBuildData::AddRef()
{
  InterlockedIncrement(&m_RefCount);
}

void ASBuildData::Release()
{
  unsigned int ret = InterlockedDecrement(&m_RefCount);
  if(ret == 0)
  {
    if(rtManager)
      rtManager->RemoveASBuildData(this);

    SAFE_RELEASE(buffer);

    if(diskCache.size && rtManager)
    {
      rtManager->ReleaseDiskCache(diskCache);
    }

    delete this;
  }
}
