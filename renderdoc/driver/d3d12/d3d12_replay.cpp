/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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

#include "d3d12_replay.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/amd_counters.h"
#include "maths/camera.h"
#include "maths/matrix.h"
#include "serialise/rdcfile.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/debugcbuffers.h"

extern "C" __declspec(dllexport) HRESULT
    __cdecl RENDERDOC_CreateWrappedDXGIFactory1(REFIID riid, void **ppFactory);
extern "C" __declspec(dllexport) HRESULT
    __cdecl RENDERDOC_CreateWrappedD3D12Device(IUnknown *pAdapter,
                                               D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                               void **ppDevice);
ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev);

static const char *DXBCDisassemblyTarget = "DXBC";

template <class T>
T &resize_and_add(rdcarray<T> &vec, size_t idx)
{
  if(idx >= vec.size())
    vec.resize(idx + 1);

  return vec[idx];
}

D3D12Replay::D3D12Replay()
{
  m_pDevice = NULL;
  m_Proxy = false;

  m_HighlightCache.driver = this;
}

void D3D12Replay::Shutdown()
{
  for(size_t i = 0; i < m_ProxyResources.size(); i++)
    m_ProxyResources[i]->Release();
  m_ProxyResources.clear();

  m_pDevice->Release();
}

void D3D12Replay::CreateResources()
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    HRESULT hr = RENDERDOC_CreateWrappedDXGIFactory1(__uuidof(IDXGIFactory4), (void **)&m_pFactory);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create DXGI factory! HRESULT: %s", ToStr(hr).c_str());
    }

    if(m_pFactory)
    {
      LUID luid = m_pDevice->GetAdapterLuid();

      IDXGIAdapter *pDXGIAdapter;
      hr = m_pFactory->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

      if(FAILED(hr))
      {
        RDCERR("Couldn't get DXGI adapter by LUID from D3D device");
      }
      else
      {
        DXGI_ADAPTER_DESC desc = {};
        pDXGIAdapter->GetDesc(&desc);

        m_Vendor = GPUVendorFromPCIVendor(desc.VendorId);
      }
    }

    m_DebugManager = new D3D12DebugManager(m_pDevice);

    CreateSOBuffers();

    m_General.Init(m_pDevice, m_DebugManager);
    m_TexRender.Init(m_pDevice, m_DebugManager);
    m_Overlay.Init(m_pDevice, m_DebugManager);
    m_VertexPick.Init(m_pDevice, m_DebugManager);
    m_PixelPick.Init(m_pDevice, m_DebugManager);
    m_Histogram.Init(m_pDevice, m_DebugManager);
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    AMDCounters *counters = new AMDCounters();

    ID3D12Device *d3dDevice = m_pDevice->GetReal();

    if(counters->Init(AMDCounters::ApiType::Dx12, (void *)d3dDevice))
    {
      m_pAMDCounters = counters;
    }
    else
    {
      delete counters;
      m_pAMDCounters = NULL;
    }
  }
  else
  {
    m_pAMDCounters = NULL;
  }
}

void D3D12Replay::DestroyResources()
{
  ClearPostVSCache();

  m_General.Release();
  m_TexRender.Release();
  m_Overlay.Release();
  m_VertexPick.Release();
  m_PixelPick.Release();
  m_Histogram.Release();

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);

  SAFE_RELEASE(m_pFactory);

  SAFE_RELEASE(m_CustomShaderTex);

  SAFE_DELETE(m_DebugManager);

  SAFE_DELETE(m_pAMDCounters);
}

ReplayStatus D3D12Replay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDevice->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

APIProperties D3D12Replay::GetAPIProperties()
{
  APIProperties ret = m_pDevice->APIProps;

  ret.pipelineType = GraphicsAPI::D3D12;
  ret.localRenderer = GraphicsAPI::D3D12;
  ret.vendor = m_Vendor;
  ret.degraded = false;
  ret.shadersMutable = false;

  return ret;
}

void D3D12Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

const SDFile &D3D12Replay::GetStructuredFile()
{
  return m_pDevice->GetStructuredFile();
}

ResourceDescription &D3D12Replay::GetResourceDesc(ResourceId id)
{
  auto it = m_ResourceIdx.find(id);
  if(it == m_ResourceIdx.end())
  {
    m_ResourceIdx[id] = m_Resources.size();
    m_Resources.push_back(ResourceDescription());
    m_Resources.back().resourceId = id;
    return m_Resources.back();
  }

  return m_Resources[it->second];
}

const std::vector<ResourceDescription> &D3D12Replay::GetResources()
{
  return m_Resources;
}

std::vector<ResourceId> D3D12Replay::GetBuffers()
{
  std::vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::GetList().begin();
      it != WrappedID3D12Resource::GetList().end(); it++)
    if(it->second->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      ret.push_back(it->first);

  return ret;
}

std::vector<ResourceId> D3D12Replay::GetTextures()
{
  std::vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::GetList().begin();
      it != WrappedID3D12Resource::GetList().end(); it++)
  {
    if(it->second->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
       m_pDevice->GetResourceManager()->GetOriginalID(it->first) != it->first)
      ret.push_back(it->first);
  }

  return ret;
}

BufferDescription D3D12Replay::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};
  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::GetList().find(id);

  if(it == WrappedID3D12Resource::GetList().end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

  ret.length = desc.Width;

  ret.creationFlags = BufferCategory::NoFlags;

  const std::vector<EventUsage> &usage = m_pDevice->GetQueue()->GetUsage(id);

  for(size_t i = 0; i < usage.size(); i++)
  {
    if(usage[i].usage == ResourceUsage::VS_RWResource ||
       usage[i].usage == ResourceUsage::HS_RWResource ||
       usage[i].usage == ResourceUsage::DS_RWResource ||
       usage[i].usage == ResourceUsage::GS_RWResource ||
       usage[i].usage == ResourceUsage::PS_RWResource ||
       usage[i].usage == ResourceUsage::CS_RWResource)
      ret.creationFlags |= BufferCategory::ReadWrite;
    else if(usage[i].usage == ResourceUsage::VertexBuffer)
      ret.creationFlags |= BufferCategory::Vertex;
    else if(usage[i].usage == ResourceUsage::IndexBuffer)
      ret.creationFlags |= BufferCategory::Index;
    else if(usage[i].usage == ResourceUsage::Indirect)
      ret.creationFlags |= BufferCategory::Indirect;
  }

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= BufferCategory::ReadWrite;

  return ret;
}

TextureDescription D3D12Replay::GetTexture(ResourceId id)
{
  TextureDescription ret = {};
  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::GetList().find(id);

  if(it == WrappedID3D12Resource::GetList().end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

  ret.format = MakeResourceFormat(desc.Format);
  ret.dimension = desc.Dimension - D3D12_RESOURCE_DIMENSION_BUFFER;

  ret.width = (uint32_t)desc.Width;
  ret.height = desc.Height;
  ret.depth = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1;
  ret.arraysize = desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1;
  ret.mips = desc.MipLevels;
  ret.msQual = desc.SampleDesc.Quality;
  ret.msSamp = RDCMAX(1U, desc.SampleDesc.Count);
  ret.byteSize = 0;
  for(uint32_t i = 0; i < ret.mips; i++)
    ret.byteSize += GetByteSize(ret.width, ret.height, ret.depth, desc.Format, i);
  ret.byteSize *= ret.arraysize;

  switch(ret.dimension)
  {
    case 1:
      ret.type = ret.arraysize > 1 ? TextureType::Texture1DArray : TextureType::Texture1D;
      break;
    case 2:
      if(ret.msSamp > 1)
        ret.type = ret.arraysize > 1 ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;
      else
        ret.type = ret.arraysize > 1 ? TextureType::Texture2DArray : TextureType::Texture2D;
      break;
    case 3: ret.type = TextureType::Texture3D; break;
  }

  ret.cubemap = m_pDevice->IsCubemap(id);

  ret.creationFlags = TextureCategory::ShaderRead;

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret.creationFlags |= TextureCategory::ColorTarget;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret.creationFlags |= TextureCategory::DepthTarget;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= TextureCategory::ShaderReadWrite;

  if(ret.resourceId == m_pDevice->GetQueue()->GetBackbufferResourceID())
  {
    ret.format = MakeResourceFormat(GetTypedFormat(desc.Format, CompType::UNorm));
    ret.creationFlags |= TextureCategory::SwapBuffer;
  }

  return ret;
}

rdcarray<ShaderEntryPoint> D3D12Replay::GetShaderEntryPoints(ResourceId shader)
{
  ID3D12DeviceChild *res = m_pDevice->GetResourceManager()->GetCurrentResource(shader);

  if(!res || !WrappedID3D12Shader::IsAlloc(res))
    return {};

  WrappedID3D12Shader *sh = (WrappedID3D12Shader *)res;

  ShaderReflection &ret = sh->GetDetails();

  return {{"main", ret.stage}};
}

ShaderReflection *D3D12Replay::GetShader(ResourceId shader, ShaderEntryPoint entry)
{
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(shader);

  if(sh)
    return &sh->GetDetails();

  return NULL;
}

vector<string> D3D12Replay::GetDisassemblyTargets()
{
  vector<string> ret;

  // DXBC is always first
  ret.insert(ret.begin(), DXBCDisassemblyTarget);

  return ret;
}

string D3D12Replay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const string &target)
{
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetLiveAs<WrappedID3D12Shader>(refl->resourceId);

  if(!sh)
    return "; Invalid Shader Specified";

  DXBC::DXBCFile *dxbc = sh->GetDXBC();

  if(target == DXBCDisassemblyTarget || target.empty())
    return dxbc->GetDisassembly();

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
}

void D3D12Replay::FreeTargetResource(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D12DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

void D3D12Replay::FreeCustomShader(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D12DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

FrameRecord D3D12Replay::GetFrameRecord()
{
  return m_pDevice->GetFrameRecord();
}

ResourceId D3D12Replay::GetLiveID(ResourceId id)
{
  if(!m_pDevice->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

vector<EventUsage> D3D12Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetQueue()->GetUsage(id);
}

void D3D12Replay::FillResourceView(D3D12Pipe::View &view, D3D12Descriptor *desc)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(desc->GetType() == D3D12DescriptorType::Sampler || desc->GetType() == D3D12DescriptorType::CBV)
  {
    return;
  }

  view.resourceId = rm->GetOriginalID(GetResID(desc->nonsamp.resource));

  if(view.resourceId == ResourceId())
    return;

  D3D12_RESOURCE_DESC res = desc->nonsamp.resource->GetDesc();

  {
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    if(desc->GetType() == D3D12DescriptorType::RTV)
      fmt = desc->nonsamp.rtv.Format;
    else if(desc->GetType() == D3D12DescriptorType::SRV)
      fmt = desc->nonsamp.srv.Format;
    else if(desc->GetType() == D3D12DescriptorType::UAV)
      fmt = (DXGI_FORMAT)desc->nonsamp.uav.desc.Format;

    if(fmt == DXGI_FORMAT_UNKNOWN)
      fmt = res.Format;

    view.elementByteSize = fmt == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, fmt, 0);

    view.viewFormat = MakeResourceFormat(fmt);

    if(desc->GetType() == D3D12DescriptorType::RTV)
    {
      view.type = MakeTextureDim(desc->nonsamp.rtv.ViewDimension);

      if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_BUFFER)
      {
        view.firstElement = desc->nonsamp.rtv.Buffer.FirstElement;
        view.numElements = desc->nonsamp.rtv.Buffer.NumElements;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = desc->nonsamp.rtv.Texture1D.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = desc->nonsamp.rtv.Texture1DArray.ArraySize;
        view.firstSlice = desc->nonsamp.rtv.Texture1DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.rtv.Texture1DArray.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = desc->nonsamp.rtv.Texture2D.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = desc->nonsamp.rtv.Texture2DArray.ArraySize;
        view.firstSlice = desc->nonsamp.rtv.Texture2DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.rtv.Texture2DArray.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = desc->nonsamp.rtv.Texture2DMSArray.ArraySize;
        view.firstSlice = desc->nonsamp.rtv.Texture2DArray.FirstArraySlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE3D)
      {
        view.numSlices = desc->nonsamp.rtv.Texture3D.WSize;
        view.firstSlice = desc->nonsamp.rtv.Texture3D.FirstWSlice;
        view.firstMip = desc->nonsamp.rtv.Texture3D.MipSlice;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::DSV)
    {
      view.type = MakeTextureDim(desc->nonsamp.dsv.ViewDimension);

      if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = desc->nonsamp.dsv.Texture1D.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = desc->nonsamp.dsv.Texture1DArray.ArraySize;
        view.firstSlice = desc->nonsamp.dsv.Texture1DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.dsv.Texture1DArray.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = desc->nonsamp.dsv.Texture2D.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = desc->nonsamp.dsv.Texture2DArray.ArraySize;
        view.firstSlice = desc->nonsamp.dsv.Texture2DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.dsv.Texture2DArray.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = desc->nonsamp.dsv.Texture2DMSArray.ArraySize;
        view.firstSlice = desc->nonsamp.dsv.Texture2DArray.FirstArraySlice;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::SRV)
    {
      view.type = MakeTextureDim(desc->nonsamp.srv.ViewDimension);

      view.swizzle[0] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          0, desc->nonsamp.srv.Shader4ComponentMapping);
      view.swizzle[1] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          1, desc->nonsamp.srv.Shader4ComponentMapping);
      view.swizzle[2] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          2, desc->nonsamp.srv.Shader4ComponentMapping);
      view.swizzle[3] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          3, desc->nonsamp.srv.Shader4ComponentMapping);

      if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
      {
        view.firstElement = desc->nonsamp.srv.Buffer.FirstElement;
        view.numElements = desc->nonsamp.srv.Buffer.NumElements;

        view.bufferFlags = MakeBufferFlags(desc->nonsamp.srv.Buffer.Flags);
        if(desc->nonsamp.srv.Buffer.StructureByteStride > 0)
          view.elementByteSize = desc->nonsamp.srv.Buffer.StructureByteStride;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = desc->nonsamp.srv.Texture1D.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture1D.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture1D.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = desc->nonsamp.srv.Texture1DArray.ArraySize;
        view.firstSlice = desc->nonsamp.srv.Texture1DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.srv.Texture1DArray.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture1DArray.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture1DArray.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = desc->nonsamp.srv.Texture2D.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture2D.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture2D.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = desc->nonsamp.srv.Texture2DArray.ArraySize;
        view.firstSlice = desc->nonsamp.srv.Texture2DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.srv.Texture2DArray.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture2DArray.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture2DArray.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = desc->nonsamp.srv.Texture2DMSArray.ArraySize;
        view.firstSlice = desc->nonsamp.srv.Texture2DMSArray.FirstArraySlice;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D)
      {
        view.firstMip = desc->nonsamp.srv.Texture3D.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture3D.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture3D.ResourceMinLODClamp;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::UAV)
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uav = desc->nonsamp.uav.desc.AsDesc();

      view.counterResourceId = rm->GetOriginalID(GetResID(desc->nonsamp.uav.counterResource));

      view.type = MakeTextureDim(uav.ViewDimension);

      if(uav.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
      {
        view.firstElement = uav.Buffer.FirstElement;
        view.numElements = uav.Buffer.NumElements;

        view.bufferFlags = MakeBufferFlags(uav.Buffer.Flags);
        if(uav.Buffer.StructureByteStride > 0)
          view.elementByteSize = uav.Buffer.StructureByteStride;

        view.counterByteOffset = uav.Buffer.CounterOffsetInBytes;

        if(view.counterResourceId != ResourceId())
        {
          bytebuf counterVal;
          GetDebugManager()->GetBufferData(desc->nonsamp.uav.counterResource,
                                           view.counterByteOffset, 4, counterVal);
          uint32_t *val = (uint32_t *)&counterVal[0];
          view.bufferStructCount = *val;
        }
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = uav.Texture1D.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = uav.Texture1DArray.ArraySize;
        view.firstSlice = uav.Texture1DArray.FirstArraySlice;
        view.firstMip = uav.Texture1DArray.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = uav.Texture2D.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = uav.Texture2DArray.ArraySize;
        view.firstSlice = uav.Texture2DArray.FirstArraySlice;
        view.firstMip = uav.Texture2DArray.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D)
      {
        view.numSlices = uav.Texture3D.WSize;
        view.firstSlice = uav.Texture3D.FirstWSlice;
        view.firstMip = uav.Texture3D.MipSlice;
      }
    }
  }
}

void D3D12Replay::FillRegisterSpaces(const D3D12RenderState::RootSignature &rootSig,
                                     rdcarray<D3D12Pipe::RegisterSpace> &dstSpaces,
                                     D3D12_SHADER_VISIBILITY visibility)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootSig.rootsig);

  // clear first to ensure the spaces are default-initialised
  dstSpaces.clear();
  dstSpaces.resize(sig->sig.numSpaces);
  D3D12Pipe::RegisterSpace *spaces = dstSpaces.data();

  for(size_t rootEl = 0; rootEl < sig->sig.params.size(); rootEl++)
  {
    const D3D12RootSignatureParameter &p = sig->sig.params[rootEl];

    if(p.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL && p.ShaderVisibility != visibility)
      continue;

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      D3D12Pipe::ConstantBuffer &cb = resize_and_add(
          spaces[p.Constants.RegisterSpace].constantBuffers, p.Constants.ShaderRegister);
      cb.immediate = true;
      cb.rootElement = (uint32_t)rootEl;
      cb.byteSize = uint32_t(sizeof(uint32_t) * p.Constants.Num32BitValues);

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootConst)
          cb.rootValues.assign(e.constants.data(),
                               RDCMIN(e.constants.size(), (size_t)p.Constants.Num32BitValues));
      }

      if(cb.rootValues.empty())
        cb.rootValues.resize(p.Constants.Num32BitValues);
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
    {
      D3D12Pipe::ConstantBuffer &cb = resize_and_add(
          spaces[p.Descriptor.RegisterSpace].constantBuffers, p.Descriptor.ShaderRegister);
      cb.immediate = true;
      cb.rootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootCBV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          cb.resourceId = rm->GetOriginalID(e.id);
          cb.byteOffset = e.offset;
          cb.byteSize = uint32_t(res->GetDesc().Width - cb.byteOffset);
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV)
    {
      D3D12Pipe::View &view =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].srvs, p.Descriptor.ShaderRegister);
      view.immediate = true;
      view.rootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootSRV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          // parameters from resource/view
          view.resourceId = rm->GetOriginalID(e.id);
          view.type = TextureType::Buffer;
          view.viewFormat = MakeResourceFormat(DXGI_FORMAT_R32_UINT);

          view.elementByteSize = sizeof(uint32_t);
          view.firstElement = e.offset / sizeof(uint32_t);
          view.numElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
    {
      D3D12Pipe::View &view =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].uavs, p.Descriptor.ShaderRegister);
      view.immediate = true;
      view.rootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootUAV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          // parameters from resource/view
          view.resourceId = rm->GetOriginalID(e.id);
          view.type = TextureType::Buffer;
          view.viewFormat = MakeResourceFormat(DXGI_FORMAT_R32_UINT);

          view.elementByteSize = sizeof(uint32_t);
          view.firstElement = e.offset / sizeof(uint32_t);
          view.numElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
      const D3D12RenderState::SignatureElement *e = NULL;
      WrappedID3D12DescriptorHeap *heap = NULL;

      if(rootEl < rootSig.sigelems.size() && rootSig.sigelems[rootEl].type == eRootTable)
      {
        e = &rootSig.sigelems[rootEl];

        heap = rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(e->id);
      }

      UINT prevTableOffset = 0;

      for(size_t r = 0; r < p.ranges.size(); r++)
      {
        const D3D12_DESCRIPTOR_RANGE1 &range = p.ranges[r];

        UINT shaderReg = range.BaseShaderRegister;
        UINT regSpace = range.RegisterSpace;

        D3D12Descriptor *desc = NULL;

        UINT offset = range.OffsetInDescriptorsFromTableStart;

        if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
          offset = prevTableOffset;

        UINT num = range.NumDescriptors;

        if(heap)
        {
          desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
          desc += e->offset;
          desc += offset;

          if(num == UINT_MAX)
          {
            // find out how many descriptors are left after
            num = heap->GetNumDescriptors() - offset - UINT(e->offset);
          }
        }
        else if(num == UINT_MAX)
        {
          RDCWARN(
              "Heap not available on replay with unbounded descriptor range, clamping to 1 "
              "descriptor.");
          num = 1;
        }

        prevTableOffset = offset + num;

        if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].samplers.size())
            spaces[regSpace].samplers.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::Sampler &samp = spaces[regSpace].samplers[shaderReg];
            samp.immediate = false;
            samp.rootElement = (uint32_t)rootEl;
            samp.tableIndex = offset + i;

            if(desc)
            {
              D3D12_SAMPLER_DESC &sampDesc = desc->samp.desc;

              samp.addressU = MakeAddressMode(sampDesc.AddressU);
              samp.addressV = MakeAddressMode(sampDesc.AddressV);
              samp.addressW = MakeAddressMode(sampDesc.AddressW);

              memcpy(samp.borderColor, sampDesc.BorderColor, sizeof(FLOAT) * 4);

              samp.compareFunction = MakeCompareFunc(sampDesc.ComparisonFunc);
              samp.filter = MakeFilter(sampDesc.Filter);
              samp.maxAnisotropy = 0;
              if(samp.filter.minify == FilterMode::Anisotropic)
                samp.maxAnisotropy = sampDesc.MaxAnisotropy;
              samp.maxLOD = sampDesc.MaxLOD;
              samp.minLOD = sampDesc.MinLOD;
              samp.mipLODBias = sampDesc.MipLODBias;

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].constantBuffers.size())
            spaces[regSpace].constantBuffers.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::ConstantBuffer &cb = spaces[regSpace].constantBuffers[shaderReg];
            cb.immediate = false;
            cb.rootElement = (uint32_t)rootEl;
            cb.tableIndex = offset + i;

            if(desc)
            {
              WrappedID3D12Resource::GetResIDFromAddr(desc->nonsamp.cbv.BufferLocation,
                                                      cb.resourceId, cb.byteOffset);
              cb.resourceId = rm->GetOriginalID(cb.resourceId);
              cb.byteSize = desc->nonsamp.cbv.SizeInBytes;

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].srvs.size())
            spaces[regSpace].srvs.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::View &view = spaces[regSpace].srvs[shaderReg];
            view.immediate = false;
            view.rootElement = (uint32_t)rootEl;
            view.tableIndex = offset + i;

            if(desc)
            {
              FillResourceView(view, desc);

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].uavs.size())
            spaces[regSpace].uavs.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::View &view = spaces[regSpace].uavs[shaderReg];
            view.immediate = false;
            view.rootElement = (uint32_t)rootEl;
            view.tableIndex = offset + i;

            if(desc)
            {
              FillResourceView(view, desc);

              desc++;
            }
          }
        }
      }
    }
  }

  for(size_t i = 0; i < sig->sig.samplers.size(); i++)
  {
    D3D12_STATIC_SAMPLER_DESC &sampDesc = sig->sig.samplers[i];

    if(sampDesc.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL &&
       sampDesc.ShaderVisibility != visibility)
      continue;

    D3D12Pipe::Sampler &samp =
        resize_and_add(spaces[sampDesc.RegisterSpace].samplers, sampDesc.ShaderRegister);
    samp.immediate = true;
    samp.rootElement = (uint32_t)i;

    samp.addressU = MakeAddressMode(sampDesc.AddressU);
    samp.addressV = MakeAddressMode(sampDesc.AddressV);
    samp.addressW = MakeAddressMode(sampDesc.AddressW);

    if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK)
    {
      samp.borderColor[0] = 0.0f;
      samp.borderColor[1] = 0.0f;
      samp.borderColor[2] = 0.0f;
      samp.borderColor[3] = 0.0f;
    }
    else if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK)
    {
      samp.borderColor[0] = 0.0f;
      samp.borderColor[1] = 0.0f;
      samp.borderColor[2] = 0.0f;
      samp.borderColor[3] = 1.0f;
    }
    else if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)
    {
      samp.borderColor[0] = 1.0f;
      samp.borderColor[1] = 1.0f;
      samp.borderColor[2] = 1.0f;
      samp.borderColor[3] = 1.0f;
    }
    else
    {
      RDCERR("Unexpected static border colour: %u", sampDesc.BorderColor);
    }

    samp.compareFunction = MakeCompareFunc(sampDesc.ComparisonFunc);
    samp.filter = MakeFilter(sampDesc.Filter);
    samp.maxAnisotropy = 0;
    if(samp.filter.minify == FilterMode::Anisotropic)
      samp.maxAnisotropy = sampDesc.MaxAnisotropy;
    samp.maxLOD = sampDesc.MaxLOD;
    samp.minLOD = sampDesc.MinLOD;
    samp.mipLODBias = sampDesc.MipLODBias;
  }
}

void D3D12Replay::SavePipelineState()
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12Pipe::State &state = m_PipelineState;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  state.pipelineResourceId = rm->GetOriginalID(rs.pipe);

  WrappedID3D12PipelineState *pipe = NULL;

  if(rs.pipe != ResourceId())
    pipe = rm->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(pipe && pipe->IsGraphics())
  {
    const D3D12_INPUT_ELEMENT_DESC *inputEl = pipe->graphics->InputLayout.pInputElementDescs;
    UINT numInput = pipe->graphics->InputLayout.NumElements;

    state.inputAssembly.layouts.resize(numInput);
    for(UINT i = 0; i < numInput; i++)
    {
      D3D12Pipe::Layout &l = state.inputAssembly.layouts[i];

      l.byteOffset = inputEl[i].AlignedByteOffset;
      l.format = MakeResourceFormat(inputEl[i].Format);
      l.inputSlot = inputEl[i].InputSlot;
      l.perInstance = inputEl[i].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
      l.instanceDataStepRate = inputEl[i].InstanceDataStepRate;
      l.semanticIndex = inputEl[i].SemanticIndex;
      l.semanticName = inputEl[i].SemanticName;
    }

    state.inputAssembly.indexStripCutValue = 0;
    switch(pipe->graphics->IBStripCutValue)
    {
      case D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF:
        state.inputAssembly.indexStripCutValue = 0xFFFF;
        break;
      case D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF:
        state.inputAssembly.indexStripCutValue = 0xFFFFFFFF;
        break;
      default: break;
    }

    state.inputAssembly.vertexBuffers.resize(rs.vbuffers.size());
    for(size_t i = 0; i < rs.vbuffers.size(); i++)
    {
      D3D12Pipe::VertexBuffer &vb = state.inputAssembly.vertexBuffers[i];

      vb.resourceId = rm->GetOriginalID(rs.vbuffers[i].buf);
      vb.byteOffset = rs.vbuffers[i].offs;
      vb.byteSize = rs.vbuffers[i].size;
      vb.byteStride = rs.vbuffers[i].stride;
    }

    state.inputAssembly.indexBuffer.resourceId = rm->GetOriginalID(rs.ibuffer.buf);
    state.inputAssembly.indexBuffer.byteOffset = rs.ibuffer.offs;
    state.inputAssembly.indexBuffer.byteSize = rs.ibuffer.size;
  }

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  if(pipe && pipe->IsCompute())
  {
    WrappedID3D12Shader *sh = (WrappedID3D12Shader *)pipe->compute->CS.pShaderBytecode;

    state.computeShader.resourceId = sh->GetResourceID();
    state.computeShader.stage = ShaderStage::Compute;
    state.computeShader.reflection = &sh->GetDetails();
    state.computeShader.bindpointMapping = sh->GetMapping();

    state.rootSignatureResourceId = rm->GetOriginalID(rs.compute.rootsig);

    if(rs.compute.rootsig != ResourceId())
      FillRegisterSpaces(rs.compute, state.computeShader.spaces, D3D12_SHADER_VISIBILITY_ALL);
  }
  else if(pipe)
  {
    D3D12Pipe::Shader *dstArr[] = {&state.vertexShader, &state.hullShader, &state.domainShader,
                                   &state.geometryShader, &state.pixelShader};

    D3D12_SHADER_BYTECODE *srcArr[] = {&pipe->graphics->VS, &pipe->graphics->HS, &pipe->graphics->DS,
                                       &pipe->graphics->GS, &pipe->graphics->PS};

    D3D12_SHADER_VISIBILITY visibility[] = {
        D3D12_SHADER_VISIBILITY_VERTEX, D3D12_SHADER_VISIBILITY_HULL, D3D12_SHADER_VISIBILITY_DOMAIN,
        D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_SHADER_VISIBILITY_PIXEL};

    for(size_t stage = 0; stage < 5; stage++)
    {
      D3D12Pipe::Shader &dst = *dstArr[stage];
      D3D12_SHADER_BYTECODE &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)src.pShaderBytecode;

      if(sh)
      {
        dst.resourceId = sh->GetResourceID();
        dst.bindpointMapping = sh->GetMapping();
        dst.reflection = &sh->GetDetails();
      }
      else
      {
        dst.resourceId = ResourceId();
        dst.bindpointMapping = ShaderBindpointMapping();
        dst.reflection = NULL;
      }

      if(rs.graphics.rootsig != ResourceId())
        FillRegisterSpaces(rs.graphics, dst.spaces, visibility[stage]);
    }

    state.rootSignatureResourceId = rm->GetOriginalID(rs.graphics.rootsig);
  }

  if(pipe && pipe->IsGraphics())
  {
    /////////////////////////////////////////////////
    // Stream Out
    /////////////////////////////////////////////////

    state.streamOut.outputs.resize(rs.streamouts.size());
    for(size_t s = 0; s < rs.streamouts.size(); s++)
    {
      state.streamOut.outputs[s].resourceId = rm->GetOriginalID(rs.streamouts[s].buf);
      state.streamOut.outputs[s].byteOffset = rs.streamouts[s].offs;
      state.streamOut.outputs[s].byteSize = rs.streamouts[s].size;

      state.streamOut.outputs[s].writtenCountResourceId =
          rm->GetOriginalID(rs.streamouts[s].countbuf);
      state.streamOut.outputs[s].writtenCountByteOffset = rs.streamouts[s].countoffs;
    }

    /////////////////////////////////////////////////
    // Rasterizer
    /////////////////////////////////////////////////

    state.rasterizer.sampleMask = pipe->graphics->SampleMask;

    {
      D3D12Pipe::RasterizerState &dst = state.rasterizer.state;
      D3D12_RASTERIZER_DESC &src = pipe->graphics->RasterizerState;

      dst.antialiasedLines = src.AntialiasedLineEnable == TRUE;

      dst.cullMode = CullMode::NoCull;
      if(src.CullMode == D3D12_CULL_MODE_FRONT)
        dst.cullMode = CullMode::Front;
      if(src.CullMode == D3D12_CULL_MODE_BACK)
        dst.cullMode = CullMode::Back;

      dst.fillMode = FillMode::Solid;
      if(src.FillMode == D3D12_FILL_MODE_WIREFRAME)
        dst.fillMode = FillMode::Wireframe;

      dst.depthBias = src.DepthBias;
      dst.depthBiasClamp = src.DepthBiasClamp;
      dst.depthClip = src.DepthClipEnable == TRUE;
      dst.frontCCW = src.FrontCounterClockwise == TRUE;
      dst.multisampleEnable = src.MultisampleEnable == TRUE;
      dst.slopeScaledDepthBias = src.SlopeScaledDepthBias;
      dst.forcedSampleCount = src.ForcedSampleCount;
      dst.conservativeRasterization =
          src.ConservativeRaster == D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
    }

    state.rasterizer.scissors.resize(rs.scissors.size());
    for(size_t i = 0; i < rs.scissors.size(); i++)
      state.rasterizer.scissors[i] = Scissor(rs.scissors[i].left, rs.scissors[i].top,
                                             rs.scissors[i].right - rs.scissors[i].left,
                                             rs.scissors[i].bottom - rs.scissors[i].top);

    state.rasterizer.viewports.resize(rs.views.size());
    for(size_t i = 0; i < rs.views.size(); i++)
      state.rasterizer.viewports[i] =
          Viewport(rs.views[i].TopLeftX, rs.views[i].TopLeftY, rs.views[i].Width,
                   rs.views[i].Height, rs.views[i].MinDepth, rs.views[i].MaxDepth);

    /////////////////////////////////////////////////
    // Output Merger
    /////////////////////////////////////////////////

    state.outputMerger.renderTargets.resize(rs.rts.size());
    for(size_t i = 0; i < rs.rts.size(); i++)
    {
      D3D12Pipe::View &view = state.outputMerger.renderTargets[i];

      D3D12Descriptor *desc = rs.rtSingle ? GetWrapped(rs.rts[0]) : GetWrapped(rs.rts[i]);

      if(desc)
      {
        if(rs.rtSingle)
          desc += i;

        view.rootElement = (uint32_t)i;
        view.immediate = false;

        FillResourceView(view, desc);
      }
      else
      {
        view = D3D12Pipe::View();
      }
    }

    {
      D3D12Pipe::View &view = state.outputMerger.depthTarget;

      if(rs.dsv.ptr)
      {
        D3D12Descriptor *desc = GetWrapped(rs.dsv);

        view.rootElement = 0;
        view.immediate = false;

        FillResourceView(view, desc);
      }
      else
      {
        view = D3D12Pipe::View();
      }
    }

    memcpy(state.outputMerger.blendState.blendFactor, rs.blendFactor, sizeof(FLOAT) * 4);

    {
      D3D12_BLEND_DESC &src = pipe->graphics->BlendState;

      state.outputMerger.blendState.alphaToCoverage = src.AlphaToCoverageEnable == TRUE;
      state.outputMerger.blendState.independentBlend = src.IndependentBlendEnable == TRUE;

      state.outputMerger.blendState.blends.resize(8);
      for(size_t i = 0; i < 8; i++)
      {
        ColorBlend &blend = state.outputMerger.blendState.blends[i];

        blend.enabled = src.RenderTarget[i].BlendEnable == TRUE;

        blend.logicOperationEnabled = src.RenderTarget[i].LogicOpEnable == TRUE;
        blend.logicOperation = MakeLogicOp(src.RenderTarget[i].LogicOp);

        blend.alphaBlend.source = MakeBlendMultiplier(src.RenderTarget[i].SrcBlendAlpha, true);
        blend.alphaBlend.destination = MakeBlendMultiplier(src.RenderTarget[i].DestBlendAlpha, true);
        blend.alphaBlend.operation = MakeBlendOp(src.RenderTarget[i].BlendOpAlpha);

        blend.colorBlend.source = MakeBlendMultiplier(src.RenderTarget[i].SrcBlend, false);
        blend.colorBlend.destination = MakeBlendMultiplier(src.RenderTarget[i].DestBlend, false);
        blend.colorBlend.operation = MakeBlendOp(src.RenderTarget[i].BlendOp);

        blend.writeMask = src.RenderTarget[i].RenderTargetWriteMask;
      }
    }

    {
      D3D12_DEPTH_STENCIL_DESC &src = pipe->graphics->DepthStencilState;

      state.outputMerger.depthStencilState.depthEnable = src.DepthEnable == TRUE;
      state.outputMerger.depthStencilState.depthFunction = MakeCompareFunc(src.DepthFunc);
      state.outputMerger.depthStencilState.depthWrites =
          src.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
      state.outputMerger.depthStencilState.stencilEnable = src.StencilEnable == TRUE;

      state.outputMerger.depthStencilState.frontFace.function =
          MakeCompareFunc(src.FrontFace.StencilFunc);
      state.outputMerger.depthStencilState.frontFace.depthFailOperation =
          MakeStencilOp(src.FrontFace.StencilDepthFailOp);
      state.outputMerger.depthStencilState.frontFace.passOperation =
          MakeStencilOp(src.FrontFace.StencilPassOp);
      state.outputMerger.depthStencilState.frontFace.failOperation =
          MakeStencilOp(src.FrontFace.StencilFailOp);

      state.outputMerger.depthStencilState.backFace.function =
          MakeCompareFunc(src.BackFace.StencilFunc);
      state.outputMerger.depthStencilState.backFace.depthFailOperation =
          MakeStencilOp(src.BackFace.StencilDepthFailOp);
      state.outputMerger.depthStencilState.backFace.passOperation =
          MakeStencilOp(src.BackFace.StencilPassOp);
      state.outputMerger.depthStencilState.backFace.failOperation =
          MakeStencilOp(src.BackFace.StencilFailOp);

      // due to shared structs, this is slightly duplicated - D3D doesn't have separate states for
      // front/back.
      state.outputMerger.depthStencilState.frontFace.reference = rs.stencilRef;
      state.outputMerger.depthStencilState.frontFace.compareMask = src.StencilReadMask;
      state.outputMerger.depthStencilState.frontFace.writeMask = src.StencilWriteMask;
      state.outputMerger.depthStencilState.backFace.reference = rs.stencilRef;
      state.outputMerger.depthStencilState.backFace.compareMask = src.StencilReadMask;
      state.outputMerger.depthStencilState.backFace.writeMask = src.StencilWriteMask;
    }
  }

  // resource states
  {
    const std::map<ResourceId, SubresourceStateVector> &states = m_pDevice->GetSubresourceStates();
    state.resourceStates.resize(states.size());
    size_t i = 0;
    for(auto it = states.begin(); it != states.end(); ++it)
    {
      D3D12Pipe::ResourceData &res = state.resourceStates[i];

      res.resourceId = rm->GetOriginalID(it->first);

      res.states.resize(it->second.size());
      for(size_t l = 0; l < it->second.size(); l++)
        res.states[l].name = ToStr(it->second[l]);

      i++;
    }
  }
}

void D3D12Replay::RenderHighlightBox(float w, float h, float scale)
{
  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    float black[] = {0.0f, 0.0f, 0.0f, 1.0f};
    float white[] = {1.0f, 1.0f, 1.0f, 1.0f};

    // size of box
    LONG sz = LONG(scale);

    // top left, x and y
    LONG tlx = LONG(w / 2.0f + 0.5f);
    LONG tly = LONG(h / 2.0f + 0.5f);

    D3D12_RECT rect[4] = {
        {tlx, tly, tlx + 1, tly + sz},

        {tlx + sz, tly, tlx + sz + 1, tly + sz + 1},

        {tlx, tly, tlx + sz, tly + 1},

        {tlx, tly + sz, tlx + sz, tly + sz + 1},
    };

    // inner
    list->ClearRenderTargetView(outw.rtv, white, 4, rect);

    // shift both sides to just translate the rect without changing its size
    rect[0].left--;
    rect[0].right--;
    rect[1].left++;
    rect[1].right++;
    rect[2].left--;
    rect[2].right--;
    rect[3].left--;
    rect[3].right--;

    rect[0].top--;
    rect[0].bottom--;
    rect[1].top--;
    rect[1].bottom--;
    rect[2].top--;
    rect[2].bottom--;
    rect[3].top++;
    rect[3].bottom++;

    // now increase the 'size' of the rects
    rect[0].bottom += 2;
    rect[1].bottom += 2;
    rect[2].right += 2;
    rect[3].right += 2;

    // outer
    list->ClearRenderTargetView(outw.rtv, black, 4, rect);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }
}

void D3D12Replay::RenderCheckerboard()
{
  DebugVertexCBuffer vertexData;

  vertexData.Scale = 2.0f;
  vertexData.Position.x = vertexData.Position.y = 0;

  vertexData.ScreenAspect.x = 1.0f;
  vertexData.ScreenAspect.y = 1.0f;

  vertexData.TextureResolution.x = 1.0f;
  vertexData.TextureResolution.y = 1.0f;

  vertexData.LineStrip = 0;

  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  pixelData.Channels = RenderDoc::Inst().LightCheckerboardColor();
  pixelData.WireframeColour = RenderDoc::Inst().DarkCheckerboardColor();

  D3D12_GPU_VIRTUAL_ADDRESS vs =
      GetDebugManager()->UploadConstants(&vertexData, sizeof(DebugVertexCBuffer));
  D3D12_GPU_VIRTUAL_ADDRESS ps = GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));

  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    list->OMSetRenderTargets(1, &outw.rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, outw.width, outw.height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    list->SetPipelineState(outw.depth ? m_General.CheckerboardMSAAPipe : m_General.CheckerboardPipe);

    list->SetGraphicsRootSignature(m_General.ConstOnlyRootSig);

    list->SetGraphicsRootConstantBufferView(0, vs);
    list->SetGraphicsRootConstantBufferView(1, ps);
    list->SetGraphicsRootConstantBufferView(2, vs);

    Vec4f dummy;
    list->SetGraphicsRoot32BitConstants(3, 4, &dummy.x, 0);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(3, 1, 0, 0);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }
}

void D3D12Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  SetOutputDimensions(1, 1);

  {
    TextureDisplay texDisplay;

    texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
    texDisplay.hdrMultiplier = -1.0f;
    texDisplay.linearDisplayAsGamma = true;
    texDisplay.flipY = false;
    texDisplay.mip = mip;
    texDisplay.sampleIdx = sample;
    texDisplay.customShaderId = ResourceId();
    texDisplay.sliceFace = sliceFace;
    texDisplay.rangeMin = 0.0f;
    texDisplay.rangeMax = 1.0f;
    texDisplay.scale = 1.0f;
    texDisplay.resourceId = texture;
    texDisplay.typeHint = typeHint;
    texDisplay.rawOutput = true;
    texDisplay.xOffset = -float(x);
    texDisplay.yOffset = -float(y);

    RenderTextureInternal(GetDebugManager()->GetCPUHandle(PICK_PIXEL_RTV), texDisplay,
                          eTexDisplay_F32Render);
  }

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = m_PixelPick.Texture;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  list->ResourceBarrier(1, &barrier);

  D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};

  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.pResource = m_PixelPick.Texture;
  src.SubresourceIndex = 0;

  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.pResource = m_General.ResultReadbackBuffer;
  dst.PlacedFootprint.Offset = 0;
  dst.PlacedFootprint.Footprint.Width = sizeof(Vec4f);
  dst.PlacedFootprint.Footprint.Height = 1;
  dst.PlacedFootprint.Footprint.Depth = 1;
  dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  dst.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

  list->ResourceBarrier(1, &barrier);

  list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  D3D12_RANGE range = {0, sizeof(Vec4f)};

  float *pix = NULL;
  HRESULT hr = m_General.ResultReadbackBuffer->Map(0, &range, (void **)&pix);

  if(FAILED(hr))
  {
    RDCERR("Failed to map picking stage tex HRESULT: %s", ToStr(hr).c_str());
  }

  if(pix == NULL)
  {
    RDCERR("Failed to map pick-pixel staging texture.");
  }
  else
  {
    pixel[0] = pix[0];
    pixel[1] = pix[1];
    pixel[2] = pix[2];
    pixel[3] = pix[3];
  }

  range.End = 0;

  if(SUCCEEDED(hr))
    m_General.ResultReadbackBuffer->Unmap(0, &range);
}

uint32_t D3D12Replay::PickVertex(uint32_t eventId, int32_t width, int32_t height,
                                 const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  if(cfg.position.numIndices == 0)
    return ~0U;

  struct MeshPickData
  {
    Vec3f RayPos;
    uint32_t PickIdx;

    Vec3f RayDir;
    uint32_t PickNumVerts;

    Vec2f PickCoords;
    Vec2f PickViewport;

    uint32_t MeshMode;
    uint32_t PickUnproject;
    Vec2f Padding;

    Matrix4f PickMVP;

  } cbuf;

  cbuf.PickCoords = Vec2f((float)x, (float)y);
  cbuf.PickViewport = Vec2f((float)width, (float)height);
  cbuf.PickIdx = cfg.position.indexByteStride ? 1 : 0;
  cbuf.PickNumVerts = cfg.position.numIndices;
  cbuf.PickUnproject = cfg.position.unproject ? 1 : 0;

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(width) / float(height));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f pickMVP = projMat.Mul(camMat);

  Matrix4f pickMVPProj;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    pickMVPProj = projMat.Mul(camMat.Mul(guessProj.Inverse()));
  }

  Vec3f rayPos;
  Vec3f rayDir;
  // convert mouse pos to world space ray
  {
    Matrix4f inversePickMVP = pickMVP.Inverse();

    float pickX = ((float)x) / ((float)width);
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)height);
    // flip the Y axis
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    Vec3f cameraToWorldNearPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

    Vec3f cameraToWorldFarPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

    Vec3f testDir = (cameraToWorldFarPosition - cameraToWorldNearPosition);
    testDir.Normalise();

    // Calculate the ray direction first in the regular way (above), so we can use the
    // the output for testing if the ray we are picking is negative or not. This is similar
    // to checking against the forward direction of the camera, but more robust
    if(cfg.position.unproject)
    {
      Matrix4f inversePickMVPGuess = pickMVPProj.Inverse();

      Vec3f nearPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

      Vec3f farPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else
    {
      rayDir = testDir;
      rayPos = cameraToWorldNearPosition;
    }
  }

  cbuf.RayPos = rayPos;
  cbuf.RayDir = rayDir;

  cbuf.PickMVP = cfg.position.unproject ? pickMVPProj : pickMVP;

  bool isTriangleMesh = true;
  switch(cfg.position.topology)
  {
    case Topology::TriangleList:
    {
      cbuf.MeshMode = MESH_TRIANGLE_LIST;
      break;
    }
    case Topology::TriangleStrip:
    {
      cbuf.MeshMode = MESH_TRIANGLE_STRIP;
      break;
    }
    case Topology::TriangleList_Adj:
    {
      cbuf.MeshMode = MESH_TRIANGLE_LIST_ADJ;
      break;
    }
    case Topology::TriangleStrip_Adj:
    {
      cbuf.MeshMode = MESH_TRIANGLE_STRIP_ADJ;
      break;
    }
    default:    // points, lines, patchlists, unknown
    {
      cbuf.MeshMode = MESH_OTHER;
      isTriangleMesh = false;
    }
  }

  ID3D12Resource *vb = NULL, *ib = NULL;

  if(cfg.position.vertexResourceId != ResourceId())
    vb = m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.vertexResourceId);

  if(cfg.position.indexResourceId != ResourceId())
    ib = m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.indexResourceId);

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers. In the case of VB
  // we also tightly pack and unpack the data. IB is upcast to R32 so it we can apply baseVertex
  // without risking overflow.

  uint32_t minIndex = 0;
  uint32_t maxIndex = cfg.position.numIndices;

  uint32_t idxclamp = 0;
  if(cfg.position.baseVertex < 0)
    idxclamp = uint32_t(-cfg.position.baseVertex);

  D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {};
  sdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  sdesc.Format = DXGI_FORMAT_R32_UINT;

  if(cfg.position.indexByteStride && ib)
  {
    // resize up on demand
    if(m_VertexPick.IB == NULL || m_VertexPick.IBSize < cfg.position.numIndices * sizeof(uint32_t))
    {
      SAFE_RELEASE(m_VertexPick.IB);

      m_VertexPick.IBSize = cfg.position.numIndices * sizeof(uint32_t);

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      D3D12_RESOURCE_DESC ibDesc;
      ibDesc.Alignment = 0;
      ibDesc.DepthOrArraySize = 1;
      ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      ibDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      ibDesc.Format = DXGI_FORMAT_UNKNOWN;
      ibDesc.Height = 1;
      ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      ibDesc.MipLevels = 1;
      ibDesc.SampleDesc.Count = 1;
      ibDesc.SampleDesc.Quality = 0;
      ibDesc.Width = m_VertexPick.IBSize;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &ibDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                              __uuidof(ID3D12Resource), (void **)&m_VertexPick.IB);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create pick index buffer: HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }

      m_VertexPick.IB->SetName(L"m_PickIB");

      sdesc.Buffer.FirstElement = 0;
      sdesc.Buffer.NumElements = cfg.position.numIndices;
      m_pDevice->CreateShaderResourceView(m_VertexPick.IB, &sdesc,
                                          GetDebugManager()->GetCPUHandle(PICK_IB_SRV));
    }

    RDCASSERT(cfg.position.indexByteOffset < 0xffffffff);

    if(m_VertexPick.IB)
    {
      bytebuf idxs;
      GetBufferData(cfg.position.indexResourceId, cfg.position.indexByteOffset, 0, idxs);

      std::vector<uint32_t> outidxs;
      outidxs.resize(cfg.position.numIndices);

      uint16_t *idxs16 = (uint16_t *)&idxs[0];
      uint32_t *idxs32 = (uint32_t *)&idxs[0];

      if(cfg.position.indexByteStride == 2)
      {
        size_t bufsize = idxs.size() / 2;

        for(uint32_t i = 0; i < bufsize && i < cfg.position.numIndices; i++)
        {
          uint32_t idx = idxs16[i];

          if(idx < idxclamp)
            idx = 0;
          else if(cfg.position.baseVertex < 0)
            idx -= idxclamp;
          else if(cfg.position.baseVertex > 0)
            idx += cfg.position.baseVertex;

          if(i == 0)
          {
            minIndex = maxIndex = idx;
          }
          else
          {
            minIndex = RDCMIN(idx, minIndex);
            maxIndex = RDCMAX(idx, maxIndex);
          }

          outidxs[i] = idx;
        }
      }
      else
      {
        uint32_t bufsize = uint32_t(idxs.size() / 4);

        minIndex = maxIndex = idxs32[0];

        for(uint32_t i = 0; i < RDCMIN(bufsize, cfg.position.numIndices); i++)
        {
          uint32_t idx = idxs32[i];

          if(idx < idxclamp)
            idx = 0;
          else if(cfg.position.baseVertex < 0)
            idx -= idxclamp;
          else if(cfg.position.baseVertex > 0)
            idx += cfg.position.baseVertex;

          minIndex = RDCMIN(idx, minIndex);
          maxIndex = RDCMAX(idx, maxIndex);

          outidxs[i] = idx;
        }
      }

      D3D11_BOX box;
      box.top = 0;
      box.bottom = 1;
      box.front = 0;
      box.back = 1;
      box.left = 0;
      box.right = UINT(outidxs.size() * sizeof(uint32_t));

      GetDebugManager()->FillBuffer(m_VertexPick.IB, 0, outidxs.data(),
                                    sizeof(uint32_t) * outidxs.size());
    }
  }
  else
  {
    sdesc.Buffer.NumElements = 4;
    m_pDevice->CreateShaderResourceView(NULL, &sdesc, GetDebugManager()->GetCPUHandle(PICK_IB_SRV));
  }

  sdesc.Buffer.FirstElement = 0;
  sdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

  // unpack and linearise the data
  {
    bytebuf oldData;
    GetDebugManager()->GetBufferData(vb, cfg.position.vertexByteOffset, 0, oldData);

    // clamp maxIndex to upper bound in case we got invalid indices or primitive restart indices
    maxIndex = RDCMIN(maxIndex, uint32_t(oldData.size() / cfg.position.vertexByteStride));

    if(vb)
    {
      if(m_VertexPick.VB == NULL || m_VertexPick.VBSize < (maxIndex + 1) * sizeof(Vec4f))
      {
        SAFE_RELEASE(m_VertexPick.VB);

        m_VertexPick.VBSize = (maxIndex + 1) * sizeof(Vec4f);

        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC vbDesc;
        vbDesc.Alignment = 0;
        vbDesc.DepthOrArraySize = 1;
        vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        vbDesc.Format = DXGI_FORMAT_UNKNOWN;
        vbDesc.Height = 1;
        vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        vbDesc.MipLevels = 1;
        vbDesc.SampleDesc.Count = 1;
        vbDesc.SampleDesc.Quality = 0;
        vbDesc.Width = m_VertexPick.VBSize;

        hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                __uuidof(ID3D12Resource), (void **)&m_VertexPick.VB);

        if(FAILED(hr))
        {
          RDCERR("Couldn't create pick vertex buffer: HRESULT: %s", ToStr(hr).c_str());
          return ~0U;
        }

        m_VertexPick.VB->SetName(L"m_PickVB");

        sdesc.Buffer.NumElements = (maxIndex + 1);
        m_pDevice->CreateShaderResourceView(m_VertexPick.VB, &sdesc,
                                            GetDebugManager()->GetCPUHandle(PICK_VB_SRV));
      }
    }
    else
    {
      sdesc.Buffer.NumElements = 4;
      m_pDevice->CreateShaderResourceView(NULL, &sdesc, GetDebugManager()->GetCPUHandle(PICK_VB_SRV));
    }

    std::vector<FloatVector> vbData;
    vbData.resize(maxIndex + 1);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

    // the index buffer may refer to vertices past the start of the vertex buffer, so we can't just
    // conver the first N vertices we'll need.
    // Instead we grab min and max above, and convert every vertex in that range. This might
    // slightly over-estimate but not as bad as 0-max or the whole buffer.
    for(uint32_t idx = minIndex; idx <= maxIndex; idx++)
      vbData[idx] = HighlightCache::InterpretVertex(data, idx, cfg, dataEnd, valid);

    GetDebugManager()->FillBuffer(m_VertexPick.VB, 0, vbData.data(), sizeof(Vec4f) * (maxIndex + 1));
  }

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  list->SetPipelineState(m_VertexPick.Pipe);

  list->SetComputeRootSignature(m_VertexPick.RootSig);

  GetDebugManager()->SetDescriptorHeaps(list, true, false);

  list->SetComputeRootConstantBufferView(0, GetDebugManager()->UploadConstants(&cbuf, sizeof(cbuf)));
  list->SetComputeRootDescriptorTable(1, GetDebugManager()->GetGPUHandle(PICK_IB_SRV));
  list->SetComputeRootDescriptorTable(2, GetDebugManager()->GetGPUHandle(PICK_RESULT_UAV));

  list->Dispatch(cfg.position.numIndices / 1024 + 1, 1, 1);

  list->Close();
  m_pDevice->ExecuteLists();

  bytebuf results;
  GetDebugManager()->GetBufferData(m_VertexPick.ResultBuf, 0, 0, results);

  list = m_pDevice->GetNewList();

  UINT zeroes[4] = {0, 0, 0, 0};
  list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(PICK_RESULT_CLEAR_UAV),
                                     GetDebugManager()->GetUAVClearHandle(PICK_RESULT_CLEAR_UAV),
                                     m_VertexPick.ResultBuf, zeroes, 0, NULL);

  list->Close();

  byte *data = &results[0];

  uint32_t numResults = *(uint32_t *)data;

  if(numResults > 0)
  {
    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        Vec3f intersectionPoint;
      };

      PickResult *pickResults = (PickResult *)(data + 64);

      PickResult *closest = pickResults;

      // distance from raycast hit to nearest worldspace position of the mouse
      float closestPickDistance = (closest->intersectionPoint - rayPos).Length();

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN(VertexPicking::MaxMeshPicks, numResults); i++)
      {
        float pickDistance = (pickResults[i].intersectionPoint - rayPos).Length();
        if(pickDistance < closestPickDistance)
        {
          closest = pickResults + i;
        }
      }

      return closest->vertid;
    }
    else
    {
      struct PickResult
      {
        uint32_t vertid;
        uint32_t idx;
        float len;
        float depth;
      };

      PickResult *pickResults = (PickResult *)(data + 64);

      PickResult *closest = pickResults;

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN(VertexPicking::MaxMeshPicks, numResults); i++)
      {
        // We need to keep the picking order consistent in the face
        // of random buffer appends, when multiple vertices have the
        // identical position (e.g. if UVs or normals are different).
        //
        // We could do something to try and disambiguate, but it's
        // never going to be intuitive, it's just going to flicker
        // confusingly.
        if(pickResults[i].len < closest->len ||
           (pickResults[i].len == closest->len && pickResults[i].depth < closest->depth) ||
           (pickResults[i].len == closest->len && pickResults[i].depth == closest->depth &&
            pickResults[i].vertid < closest->vertid))
          closest = pickResults + i;
      }

      return closest->vertid;
    }
  }

  return ~0U;
}

bool D3D12Replay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float *minval, float *maxval)
{
  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[texid];

  if(resource == NULL)
    return false;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = float(RDCMAX(1U, uint32_t(resourceDesc.Width >> mip)));
  cdata.HistogramTextureResolution.y = float(RDCMAX(1U, uint32_t(resourceDesc.Height >> mip)));
  cdata.HistogramTextureResolution.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramTextureResolution.z = float(resourceDesc.DepthOrArraySize);

  cdata.HistogramSlice = float(RDCCLAMP(sliceFace, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramSlice = float(sliceFace) / float(resourceDesc.DepthOrArraySize);

  cdata.HistogramMip = mip;
  cdata.HistogramSample = (int)RDCCLAMP(sample, 0U, resourceDesc.SampleDesc.Count - 1);
  if(sample == ~0U)
    cdata.HistogramSample = -int(resourceDesc.SampleDesc.Count);
  cdata.HistogramMin = 0.0f;
  cdata.HistogramMax = 1.0f;
  cdata.HistogramChannels = 0xf;
  cdata.HistogramFlags = 0;

  int intIdx = 0;

  DXGI_FORMAT fmt = GetTypedFormat(resourceDesc.Format, typeHint);

  if(IsUIntFormat(fmt))
    intIdx = 1;
  else if(IsIntFormat(fmt))
    intIdx = 2;

  int blocksX = (int)ceil(cdata.HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata.HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, typeHint, resType, barriers);

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->SetPipelineState(m_Histogram.TileMinMaxPipe[resType][intIdx]);

    list->SetComputeRootSignature(m_Histogram.HistogramRootSig);

    GetDebugManager()->SetDescriptorHeaps(list, true, true);

    D3D12_GPU_DESCRIPTOR_HANDLE uav = GetDebugManager()->GetGPUHandle(MINMAX_TILE_UAVS);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = GetDebugManager()->GetGPUHandle(FIRST_TEXDISPLAY_SRV);

    list->SetComputeRootConstantBufferView(
        0, GetDebugManager()->UploadConstants(&cdata, sizeof(cdata)));
    list->SetComputeRootDescriptorTable(1, srv);
    list->SetComputeRootDescriptorTable(2, GetDebugManager()->GetGPUHandle(FIRST_SAMP));
    list->SetComputeRootDescriptorTable(3, uav);

    // discard the whole resource as we will overwrite it
    D3D12_DISCARD_REGION region = {};
    region.NumSubresources = 1;
    list->DiscardResource(m_Histogram.MinMaxTileBuffer, &region);

    list->Dispatch(blocksX, blocksY, 1);

    D3D12_RESOURCE_BARRIER tileBarriers[2] = {};

    // ensure UAV work is done. Transition to SRV
    tileBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    tileBarriers[0].UAV.pResource = m_Histogram.MinMaxTileBuffer;
    tileBarriers[1].Transition.pResource = m_Histogram.MinMaxTileBuffer;
    tileBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    tileBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    list->ResourceBarrier(2, tileBarriers);

    // set up second dispatch
    srv = GetDebugManager()->GetGPUHandle(MINMAX_TILE_SRVS);
    uav = GetDebugManager()->GetGPUHandle(MINMAX_RESULT_UAVS);

    list->SetComputeRootDescriptorTable(1, srv);
    list->SetComputeRootDescriptorTable(3, uav);

    list->SetPipelineState(m_Histogram.ResultMinMaxPipe[intIdx]);

    list->Dispatch(1, 1, 1);

    // transition back to UAV for next time
    std::swap(tileBarriers[1].Transition.StateBefore, tileBarriers[1].Transition.StateAfter);

    list->ResourceBarrier(1, &tileBarriers[1]);

    // finish the UAV work, and transition to copy.
    tileBarriers[0].UAV.pResource = m_Histogram.MinMaxResultBuffer;
    tileBarriers[1].Transition.pResource = m_Histogram.MinMaxResultBuffer;
    tileBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    tileBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(2, tileBarriers);

    // copy to readback
    list->CopyBufferRegion(m_General.ResultReadbackBuffer, 0, m_Histogram.MinMaxResultBuffer, 0,
                           sizeof(Vec4f) * 2);

    // transition back to UAV for next time
    std::swap(tileBarriers[1].Transition.StateBefore, tileBarriers[1].Transition.StateAfter);

    list->ResourceBarrier(1, &tileBarriers[1]);

    // transition image back to where it was
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }

  D3D12_RANGE range = {0, sizeof(Vec4f) * 2};

  void *data = NULL;
  HRESULT hr = m_General.ResultReadbackBuffer->Map(0, &range, &data);

  if(FAILED(hr))
  {
    RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  else
  {
    Vec4f *minmax = (Vec4f *)data;

    minval[0] = minmax[0].x;
    minval[1] = minmax[0].y;
    minval[2] = minmax[0].z;
    minval[3] = minmax[0].w;

    maxval[0] = minmax[1].x;
    maxval[1] = minmax[1].y;
    maxval[2] = minmax[1].z;
    maxval[3] = minmax[1].w;

    range.End = 0;

    m_General.ResultReadbackBuffer->Unmap(0, &range);
  }

  return true;
}

bool D3D12Replay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                               CompType typeHint, float minval, float maxval, bool channels[4],
                               vector<uint32_t> &histogram)
{
  if(minval >= maxval)
    return false;

  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[texid];

  if(resource == NULL)
    return false;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = float(RDCMAX(1U, uint32_t(resourceDesc.Width >> mip)));
  cdata.HistogramTextureResolution.y = float(RDCMAX(1U, uint32_t(resourceDesc.Height >> mip)));
  cdata.HistogramTextureResolution.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramTextureResolution.z = float(resourceDesc.DepthOrArraySize);

  cdata.HistogramSlice = float(RDCCLAMP(sliceFace, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramSlice = float(sliceFace) / float(resourceDesc.DepthOrArraySize);

  cdata.HistogramMip = mip;
  cdata.HistogramSample = (int)RDCCLAMP(sample, 0U, resourceDesc.SampleDesc.Count - 1);
  if(sample == ~0U)
    cdata.HistogramSample = -int(resourceDesc.SampleDesc.Count);
  cdata.HistogramMin = minval;
  cdata.HistogramFlags = 0;

  // The calculation in the shader normalises each value between min and max, then multiplies by the
  // number of buckets.
  // But any value equal to HistogramMax must go into NUM_BUCKETS-1, so add a small delta.
  cdata.HistogramMax = maxval + maxval * 1e-6f;

  cdata.HistogramChannels = 0;
  if(channels[0])
    cdata.HistogramChannels |= 0x1;
  if(channels[1])
    cdata.HistogramChannels |= 0x2;
  if(channels[2])
    cdata.HistogramChannels |= 0x4;
  if(channels[3])
    cdata.HistogramChannels |= 0x8;
  cdata.HistogramFlags = 0;

  int intIdx = 0;

  DXGI_FORMAT fmt = GetTypedFormat(resourceDesc.Format, typeHint);

  if(IsUIntFormat(fmt))
    intIdx = 1;
  else if(IsIntFormat(fmt))
    intIdx = 2;

  int tilesX = (int)ceil(cdata.HistogramTextureResolution.x /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int tilesY = (int)ceil(cdata.HistogramTextureResolution.y /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, typeHint, resType, barriers);

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->SetPipelineState(m_Histogram.HistogramPipe[resType][intIdx]);

    list->SetComputeRootSignature(m_Histogram.HistogramRootSig);

    GetDebugManager()->SetDescriptorHeaps(list, true, true);

    D3D12_GPU_DESCRIPTOR_HANDLE uav = GetDebugManager()->GetGPUHandle(HISTOGRAM_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = GetDebugManager()->GetGPUHandle(FIRST_TEXDISPLAY_SRV);
    D3D12_CPU_DESCRIPTOR_HANDLE uavcpu = GetDebugManager()->GetUAVClearHandle(HISTOGRAM_UAV);

    UINT zeroes[] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(uav, uavcpu, m_Histogram.MinMaxTileBuffer, zeroes, 0, NULL);

    list->SetComputeRootConstantBufferView(
        0, GetDebugManager()->UploadConstants(&cdata, sizeof(cdata)));
    list->SetComputeRootDescriptorTable(1, srv);
    list->SetComputeRootDescriptorTable(2, GetDebugManager()->GetGPUHandle(FIRST_SAMP));
    list->SetComputeRootDescriptorTable(3, uav);

    list->Dispatch(tilesX, tilesY, 1);

    D3D12_RESOURCE_BARRIER tileBarriers[2] = {};

    // finish the UAV work, and transition to copy.
    tileBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    tileBarriers[0].UAV.pResource = m_Histogram.MinMaxTileBuffer;
    tileBarriers[1].Transition.pResource = m_Histogram.MinMaxTileBuffer;
    tileBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    tileBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(2, tileBarriers);

    // copy to readback
    list->CopyBufferRegion(m_General.ResultReadbackBuffer, 0, m_Histogram.MinMaxTileBuffer, 0,
                           sizeof(uint32_t) * HGRAM_NUM_BUCKETS);

    // transition back to UAV for next time
    std::swap(tileBarriers[1].Transition.StateBefore, tileBarriers[1].Transition.StateAfter);

    list->ResourceBarrier(1, &tileBarriers[1]);

    // transition image back to where it was
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }

  D3D12_RANGE range = {0, sizeof(uint32_t) * HGRAM_NUM_BUCKETS};

  void *data = NULL;
  HRESULT hr = m_General.ResultReadbackBuffer->Map(0, &range, &data);

  histogram.clear();
  histogram.resize(HGRAM_NUM_BUCKETS);

  if(FAILED(hr))
  {
    RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  else
  {
    memcpy(&histogram[0], data, sizeof(uint32_t) * HGRAM_NUM_BUCKETS);

    range.End = 0;

    m_General.ResultReadbackBuffer->Unmap(0, &range);
  }

  return true;
}

bool D3D12Replay::IsRenderOutput(ResourceId id)
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  id = m_pDevice->GetResourceManager()->GetLiveID(id);

  for(size_t i = 0; i < rs.rts.size(); i++)
  {
    D3D12Descriptor *desc = rs.rtSingle ? GetWrapped(rs.rts[0]) : GetWrapped(rs.rts[i]);

    if(desc)
    {
      if(rs.rtSingle)
        desc += i;

      if(id == GetResID(desc->nonsamp.resource))
        return true;
    }
  }

  if(rs.dsv.ptr)
  {
    D3D12Descriptor *desc = GetWrapped(rs.dsv);

    if(id == GetResID(desc->nonsamp.resource))
      return true;
  }

  return false;
}

vector<uint32_t> D3D12Replay::GetPassEvents(uint32_t eventId)
{
  vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

  if(!draw)
    return passEvents;

  // for D3D12 a pass == everything writing to the same RTs in a command list.
  const DrawcallDescription *start = draw;
  while(start)
  {
    // if we've come to the beginning of a list, break out of the loop, we've
    // found the start.
    if(start->flags & DrawFlags::BeginPass)
      break;

    // if we come to the END of a list, since we were iterating backwards that
    // means we started outside of a list, so return empty set.
    if(start->flags & DrawFlags::EndPass)
      return passEvents;

    // if we've come to the start of the log we were outside of a list
    // to start with
    if(start->previous == 0)
      return passEvents;

    // step back
    const DrawcallDescription *prev = m_pDevice->GetDrawcall((uint32_t)start->previous);

    // something went wrong, start->previous was non-zero but we didn't
    // get a draw. Abort
    if(!prev)
      return passEvents;

    // if the outputs changed, we're done
    if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) ||
       start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  // store all the draw eventIDs up to the one specified at the start
  while(start)
  {
    if(start == draw)
      break;

    // include pass boundaries, these will be filtered out later
    // so we don't actually do anything (init postvs/draw overlay)
    // but it's useful to have the first part of the pass as part
    // of the list
    if(start->flags & (DrawFlags::Drawcall | DrawFlags::PassBoundary))
      passEvents.push_back(start->eventId);

    start = m_pDevice->GetDrawcall((uint32_t)start->next);
  }

  return passEvents;
}

bool D3D12Replay::IsTextureSupported(const ResourceFormat &format)
{
  return MakeDXGIFormat(format) != DXGI_FORMAT_UNKNOWN;
}

bool D3D12Replay::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

void D3D12Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, bytebuf &retData)
{
  auto it = WrappedID3D12Resource::GetList().find(buff);

  if(it == WrappedID3D12Resource::GetList().end())
  {
    RDCERR("Getting buffer data for unknown buffer %llu!", buff);
    return;
  }

  WrappedID3D12Resource *buffer = it->second;

  RDCASSERT(buffer);

  GetDebugManager()->GetBufferData(buffer, offset, length, retData);
}

void D3D12Replay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const bytebuf &data)
{
  if(shader == ResourceId())
    return;

  ID3D12DeviceChild *res = m_pDevice->GetResourceManager()->GetCurrentResource(shader);

  if(!WrappedID3D12Shader::IsAlloc(res))
  {
    RDCERR("Shader ID %llu does not correspond to a known fake shader", shader);
    return;
  }

  WrappedID3D12Shader *sh = (WrappedID3D12Shader *)res;

  DXBC::DXBCFile *dxbc = sh->GetDXBC();
  const ShaderBindpointMapping &bindMap = sh->GetMapping();

  RDCASSERT(dxbc);

  DXBC::CBuffer *cb = NULL;

  uint32_t idx = 0;
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    if(dxbc->m_CBuffers[i].descriptor.type != DXBC::CBuffer::Descriptor::TYPE_CBUFFER)
      continue;

    if(idx == cbufSlot)
      cb = &dxbc->m_CBuffers[i];

    idx++;
  }

  if(cb && cbufSlot < (uint32_t)bindMap.constantBlocks.count())
  {
    // check if the data actually comes from root constants

    const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
    Bindpoint bind = bindMap.constantBlocks[cbufSlot];

    WrappedID3D12RootSignature *sig = NULL;
    const vector<D3D12RenderState::SignatureElement> *sigElems = NULL;

    if(dxbc->m_Type == D3D11_ShaderType_Compute && rs.compute.rootsig != ResourceId())
    {
      sig = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
          rs.compute.rootsig);
      sigElems = &rs.compute.sigelems;
    }
    else if(dxbc->m_Type != D3D11_ShaderType_Compute && rs.graphics.rootsig != ResourceId())
    {
      sig = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
          rs.graphics.rootsig);
      sigElems = &rs.graphics.sigelems;
    }

    bytebuf rootData;

    for(size_t i = 0; sig && i < sig->sig.params.size(); i++)
    {
      const D3D12RootSignatureParameter &p = sig->sig.params[i];

      if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
         p.Constants.RegisterSpace == (UINT)bind.bindset &&
         p.Constants.ShaderRegister == (UINT)bind.bind)
      {
        size_t dstSize = sig->sig.params[i].Constants.Num32BitValues * sizeof(uint32_t);
        rootData.resize(dstSize);

        if(i < sigElems->size())
        {
          const D3D12RenderState::SignatureElement &el = (*sigElems)[i];

          if(el.type == eRootConst)
          {
            byte *dst = &rootData[0];
            const UINT *src = &el.constants[0];
            size_t srcSize = el.constants.size() * sizeof(uint32_t);

            memcpy(dst, src, RDCMIN(srcSize, dstSize));
          }
        }
      }
    }

    size_t zero = 0;
    GetDebugManager()->FillCBufferVariables("", zero, false, cb->variables, outvars,
                                            rootData.empty() ? data : rootData);
  }
}

vector<DebugMessage> D3D12Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

void D3D12Replay::BuildShader(std::string source, std::string entry,
                              const ShaderCompileFlags &compileFlags, ShaderStage type,
                              ResourceId *id, std::string *errors)
{
  uint32_t flags = DXBC::DecodeFlags(compileFlags);

  if(id == NULL || errors == NULL)
  {
    if(id)
      *id = ResourceId();
    return;
  }

  char *profile = NULL;

  switch(type)
  {
    case ShaderStage::Vertex: profile = "vs_5_0"; break;
    case ShaderStage::Hull: profile = "hs_5_0"; break;
    case ShaderStage::Domain: profile = "ds_5_0"; break;
    case ShaderStage::Geometry: profile = "gs_5_0"; break;
    case ShaderStage::Pixel: profile = "ps_5_0"; break;
    case ShaderStage::Compute: profile = "cs_5_0"; break;
    default:
      RDCERR("Unexpected type in BuildShader!");
      *id = ResourceId();
      return;
  }

  ID3DBlob *blob = NULL;
  *errors = m_pDevice->GetShaderCache()->GetShaderBlob(source.c_str(), entry.c_str(), flags,
                                                       profile, &blob);

  if(blob == NULL)
  {
    *id = ResourceId();
    return;
  }

  D3D12_SHADER_BYTECODE byteCode;
  byteCode.BytecodeLength = blob->GetBufferSize();
  byteCode.pShaderBytecode = blob->GetBufferPointer();

  WrappedID3D12Shader *sh = WrappedID3D12Shader::AddShader(byteCode, m_pDevice, NULL);

  SAFE_RELEASE(blob);

  *id = sh->GetResourceID();
}

void D3D12Replay::BuildTargetShader(std::string source, std::string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, std::string *errors)
{
  ShaderCompileFlags debugCompileFlags =
      DXBC::EncodeFlags(DXBC::DecodeFlags(compileFlags) | D3DCOMPILE_DEBUG);

  BuildShader(source, entry, debugCompileFlags, type, id, errors);
}

void D3D12Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // remove any previous replacement
  RemoveReplacement(from);

  if(rm->HasLiveResource(from))
  {
    ID3D12DeviceChild *resource = rm->GetLiveResource(from);

    if(WrappedID3D12Shader::IsAlloc(resource))
    {
      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)resource;

      for(size_t i = 0; i < sh->m_Pipes.size(); i++)
      {
        WrappedID3D12PipelineState *pipe = sh->m_Pipes[i];

        ResourceId id = rm->GetOriginalID(pipe->GetResourceID());

        ID3D12PipelineState *replpipe = NULL;

        D3D12_SHADER_BYTECODE shDesc = rm->GetLiveAs<WrappedID3D12Shader>(to)->GetDesc();

        if(pipe->graphics)
        {
          D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = *pipe->graphics;

          D3D12_SHADER_BYTECODE *shaders[] = {
              &desc.VS, &desc.HS, &desc.DS, &desc.GS, &desc.PS,
          };

          for(size_t s = 0; s < ARRAY_COUNT(shaders); s++)
          {
            if(shaders[s]->BytecodeLength > 0)
            {
              WrappedID3D12Shader *stage = (WrappedID3D12Shader *)shaders[s]->pShaderBytecode;

              if(stage->GetResourceID() == from)
                *shaders[s] = shDesc;
              else
                *shaders[s] = stage->GetDesc();
            }
          }

          m_pDevice->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState),
                                                 (void **)&replpipe);
        }
        else
        {
          D3D12_COMPUTE_PIPELINE_STATE_DESC desc = *pipe->compute;

          // replace the shader
          desc.CS = shDesc;

          m_pDevice->CreateComputePipelineState(&desc, __uuidof(ID3D12PipelineState),
                                                (void **)&replpipe);
        }

        rm->ReplaceResource(id, GetResID(replpipe));
      }
    }

    rm->ReplaceResource(from, to);
  }

  ClearPostVSCache();
}

void D3D12Replay::RemoveReplacement(ResourceId id)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  rm->RemoveReplacement(id);

  if(rm->HasLiveResource(id))
  {
    ID3D12DeviceChild *resource = rm->GetLiveResource(id);

    if(WrappedID3D12Shader::IsAlloc(resource))
    {
      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)resource;

      for(size_t i = 0; i < sh->m_Pipes.size(); i++)
      {
        WrappedID3D12PipelineState *pipe = sh->m_Pipes[i];

        ResourceId pipeid = rm->GetOriginalID(pipe->GetResourceID());

        if(rm->HasReplacement(pipeid))
        {
          // if there was an active replacement, remove the dependent replaced pipelines.
          ID3D12DeviceChild *replpipe = rm->GetLiveResource(pipeid);

          rm->RemoveReplacement(pipeid);

          SAFE_RELEASE(replpipe);
        }
      }
    }
  }

  ClearPostVSCache();
}

void D3D12Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  bool wasms = false;

  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[tex];

  if(resource == NULL)
  {
    RDCERR("Trying to get texture data for unknown ID %llu!", tex);
    return;
  }

  HRESULT hr = S_OK;

  D3D12_RESOURCE_DESC resDesc = resource->GetDesc();

  D3D12_RESOURCE_DESC copyDesc = resDesc;
  copyDesc.Alignment = 0;
  copyDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  copyDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

  D3D12_HEAP_PROPERTIES defaultHeap;
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
  defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  defaultHeap.CreationNodeMask = 1;
  defaultHeap.VisibleNodeMask = 1;

  bool isDepth = IsDepthFormat(resDesc.Format);
  bool isStencil = IsDepthAndStencilFormat(resDesc.Format);

  if(copyDesc.SampleDesc.Count > 1)
  {
    // make image n-array instead of n-samples
    copyDesc.DepthOrArraySize *= (UINT16)copyDesc.SampleDesc.Count;
    copyDesc.SampleDesc.Count = 1;
    copyDesc.SampleDesc.Quality = 0;

    wasms = true;
  }

  ID3D12Resource *srcTexture = resource;
  ID3D12Resource *tmpTexture = NULL;

  ID3D12GraphicsCommandList *list = NULL;

  if(params.remap != RemapTexture::NoRemap)
  {
    RDCASSERT(params.remap == RemapTexture::RGBA8);

    // force readback texture to RGBA8 unorm
    copyDesc.Format = IsSRGBFormat(copyDesc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                                    : DXGI_FORMAT_R8G8B8A8_UNORM;
    // force to 1 array slice, 1 mip
    copyDesc.DepthOrArraySize = 1;
    copyDesc.MipLevels = 1;
    // force to 2D
    copyDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    copyDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    copyDesc.Width = RDCMAX(1ULL, copyDesc.Width >> mip);
    copyDesc.Height = RDCMAX(1U, copyDesc.Height >> mip);

    ID3D12Resource *remapTexture;
    hr = m_pDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                            __uuidof(ID3D12Resource), (void **)&remapTexture);
    RDCASSERTEQUAL(hr, S_OK);

    SetOutputDimensions(uint32_t(copyDesc.Width), copyDesc.Height);

    TexDisplayFlags flags =
        IsSRGBFormat(copyDesc.Format) ? eTexDisplay_None : eTexDisplay_LinearRender;

    m_pDevice->CreateRenderTargetView(remapTexture, NULL,
                                      GetDebugManager()->GetCPUHandle(GET_TEX_RTV));

    {
      TextureDisplay texDisplay;

      texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
      texDisplay.hdrMultiplier = -1.0f;
      texDisplay.linearDisplayAsGamma = false;
      texDisplay.overlay = DebugOverlay::NoOverlay;
      texDisplay.flipY = false;
      texDisplay.mip = mip;
      texDisplay.sampleIdx = params.resolve ? ~0U : arrayIdx;
      texDisplay.customShaderId = ResourceId();
      texDisplay.sliceFace = arrayIdx;
      texDisplay.rangeMin = params.blackPoint;
      texDisplay.rangeMax = params.whitePoint;
      texDisplay.scale = 1.0f;
      texDisplay.resourceId = tex;
      texDisplay.typeHint = CompType::Typeless;
      texDisplay.rawOutput = false;
      texDisplay.xOffset = 0;
      texDisplay.yOffset = 0;

      RenderTextureInternal(GetDebugManager()->GetCPUHandle(GET_TEX_RTV), texDisplay, flags);
    }

    tmpTexture = srcTexture = remapTexture;

    list = m_pDevice->GetNewList();

    D3D12_RESOURCE_BARRIER b = {};
    b.Transition.pResource = remapTexture;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &b);

    // these have already been selected, don't need to fetch that subresource
    // when copying back to readback buffer
    arrayIdx = 0;
    mip = 0;

    // no longer depth, if it was
    isDepth = false;
    isStencil = false;
  }
  else if(wasms && params.resolve)
  {
    // force to 1 array slice, 1 mip
    copyDesc.DepthOrArraySize = 1;
    copyDesc.MipLevels = 1;

    copyDesc.Width = RDCMAX(1ULL, copyDesc.Width >> mip);
    copyDesc.Height = RDCMAX(1U, copyDesc.Height >> mip);

    ID3D12Resource *resolveTexture;
    hr = m_pDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc,
                                            D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL,
                                            __uuidof(ID3D12Resource), (void **)&resolveTexture);
    RDCASSERTEQUAL(hr, S_OK);

    RDCASSERT(!isDepth && !isStencil);

    list = m_pDevice->GetNewList();

    // put source texture into resolve source state
    const vector<D3D12_RESOURCE_STATES> &states = m_pDevice->GetSubresourceStates(tex);

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(states.size());
    for(size_t i = 0; i < states.size(); i++)
    {
      D3D12_RESOURCE_BARRIER b;

      // skip unneeded barriers
      if(states[i] & D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
        continue;

      b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      b.Transition.pResource = resource;
      b.Transition.Subresource = (UINT)i;
      b.Transition.StateBefore = states[i];
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

      barriers.push_back(b);
    }

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->ResolveSubresource(resolveTexture, 0, srcTexture,
                             arrayIdx * resDesc.DepthOrArraySize + mip, resDesc.Format);

    // real resource back to normal
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    D3D12_RESOURCE_BARRIER b = {};
    b.Transition.pResource = resolveTexture;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &b);

    tmpTexture = srcTexture = resolveTexture;

    // these have already been selected, don't need to fetch that subresource
    // when copying back to readback buffer
    arrayIdx = 0;
    mip = 0;
  }
  else if(wasms)
  {
    // copy/expand multisampled live texture to array readback texture
    RDCUNIMPLEMENTED("CopyTex2DMSToArray on D3D12");
  }

  if(list == NULL)
    list = m_pDevice->GetNewList();

  std::vector<D3D12_RESOURCE_BARRIER> barriers;

  // if we have no tmpImage, we're copying directly from the real image
  if(tmpTexture == NULL)
  {
    const vector<D3D12_RESOURCE_STATES> &states = m_pDevice->GetSubresourceStates(tex);
    barriers.reserve(states.size());
    for(size_t i = 0; i < states.size(); i++)
    {
      D3D12_RESOURCE_BARRIER b;

      // skip unneeded barriers
      if(states[i] & D3D12_RESOURCE_STATE_COPY_SOURCE)
        continue;

      b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      b.Transition.pResource = resource;
      b.Transition.Subresource = (UINT)i;
      b.Transition.StateBefore = states[i];
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

      barriers.push_back(b);
    }

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);
  }

  D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
  formatInfo.Format = copyDesc.Format;
  m_pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

  UINT planes = RDCMAX((UINT8)1, formatInfo.PlaneCount);

  UINT numSubresources = copyDesc.MipLevels;
  if(copyDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    numSubresources *= copyDesc.DepthOrArraySize;

  numSubresources *= planes;

  D3D12_RESOURCE_DESC readbackDesc;
  readbackDesc.Alignment = 0;
  readbackDesc.DepthOrArraySize = 1;
  readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
  readbackDesc.Height = 1;
  readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  readbackDesc.MipLevels = 1;
  readbackDesc.SampleDesc.Count = 1;
  readbackDesc.SampleDesc.Quality = 0;
  readbackDesc.Width = 0;

  // we only actually want to copy the specified array index/mip.
  // But we do need to copy all planes
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[planes];
  UINT *rowcounts = new UINT[planes];

  UINT arrayStride = copyDesc.MipLevels;
  UINT planeStride = copyDesc.DepthOrArraySize * copyDesc.MipLevels;

  for(UINT i = 0; i < planes; i++)
  {
    readbackDesc.Width = AlignUp(readbackDesc.Width, 512ULL);

    UINT sub = mip + arrayIdx * arrayStride + i * planeStride;

    UINT64 subSize = 0;
    m_pDevice->GetCopyableFootprints(&copyDesc, sub, 1, readbackDesc.Width, layouts + i,
                                     rowcounts + i, NULL, &subSize);
    readbackDesc.Width += subSize;
  }

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  ID3D12Resource *readbackBuf = NULL;
  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                          __uuidof(ID3D12Resource), (void **)&readbackBuf);
  RDCASSERTEQUAL(hr, S_OK);

  for(UINT i = 0; i < planes; i++)
  {
    D3D12_TEXTURE_COPY_LOCATION dst, src;

    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = srcTexture;
    src.SubresourceIndex = mip + arrayIdx * arrayStride + i * planeStride;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.pResource = readbackBuf;
    dst.PlacedFootprint = layouts[i];

    list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
  }

  // if we have no tmpImage, we're copying directly from the real image
  if(tmpTexture == NULL)
  {
    // real resource back to normal
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);
  }

  list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  // map the buffer and copy to return buffer
  byte *pData = NULL;
  hr = readbackBuf->Map(0, NULL, (void **)&pData);
  RDCASSERTEQUAL(hr, S_OK);

  RDCASSERT(pData != NULL);

  data.resize(GetByteSize(layouts[0].Footprint.Width, layouts[0].Footprint.Height,
                          layouts[0].Footprint.Depth, copyDesc.Format, 0));

  // for depth-stencil need to merge the planes pixel-wise
  if(isDepth && isStencil)
  {
    UINT dstRowPitch = GetByteSize(layouts[0].Footprint.Width, 1, 1, copyDesc.Format, 0);

    if(copyDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
       copyDesc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
       copyDesc.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
    {
      for(UINT s = 0; s < layouts[0].Footprint.Depth; s++)
      {
        for(UINT r = 0; r < layouts[0].Footprint.Height; r++)
        {
          UINT row = r + s * layouts[0].Footprint.Height;

          uint32_t *dSrc = (uint32_t *)(pData + layouts[0].Footprint.RowPitch * row);
          uint8_t *sSrc =
              (uint8_t *)(pData + layouts[1].Offset + layouts[1].Footprint.RowPitch * row);

          uint32_t *dDst = (uint32_t *)(data.data() + dstRowPitch * row);
          uint32_t *sDst = dDst + 1;    // interleaved, next pixel

          for(UINT i = 0; i < layouts[0].Footprint.Width; i++)
          {
            *dDst = *dSrc;
            *sDst = *sSrc;

            // increment source pointers by 1 since they're separate, and dest pointers by 2 since
            // they're interleaved
            dDst += 2;
            sDst += 2;

            sSrc++;
            dSrc++;
          }
        }
      }
    }
    else    // D24_S8
    {
      for(UINT s = 0; s < layouts[0].Footprint.Depth; s++)
      {
        for(UINT r = 0; r < rowcounts[0]; r++)
        {
          UINT row = r + s * rowcounts[0];

          // we can copy the depth from D24 as a 32-bit integer, since the remaining bits are
          // garbage
          // and we overwrite them with stencil
          uint32_t *dSrc = (uint32_t *)(pData + layouts[0].Footprint.RowPitch * row);
          uint8_t *sSrc =
              (uint8_t *)(pData + layouts[1].Offset + layouts[1].Footprint.RowPitch * row);

          uint32_t *dst = (uint32_t *)(data.data() + dstRowPitch * row);

          for(UINT i = 0; i < layouts[0].Footprint.Width; i++)
          {
            // pack the data together again, stencil in top bits
            *dst = (*dSrc & 0x00ffffff) | (uint32_t(*sSrc) << 24);

            dst++;
            sSrc++;
            dSrc++;
          }
        }
      }
    }
  }
  else
  {
    UINT dstRowPitch = GetByteSize(layouts[0].Footprint.Width, 1, 1, copyDesc.Format, 0);

    // copy row by row
    for(UINT s = 0; s < layouts[0].Footprint.Depth; s++)
    {
      for(UINT r = 0; r < rowcounts[0]; r++)
      {
        UINT row = r + s * rowcounts[0];

        byte *src = pData + layouts[0].Footprint.RowPitch * row;
        byte *dst = data.data() + dstRowPitch * row;

        memcpy(dst, src, dstRowPitch);
      }
    }
  }

  SAFE_DELETE_ARRAY(layouts);
  SAFE_DELETE_ARRAY(rowcounts);

  D3D12_RANGE range = {0, 0};
  readbackBuf->Unmap(0, &range);

  // clean up temporary objects
  SAFE_RELEASE(readbackBuf);
  SAFE_RELEASE(tmpTexture);
}

void D3D12Replay::BuildCustomShader(std::string source, std::string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, std::string *errors)
{
  BuildShader(source, entry, compileFlags, type, id, errors);
}

ResourceId D3D12Replay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                          uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint)
{
  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[texid];

  if(resource == NULL)
    return ResourceId();

  D3D12_RESOURCE_DESC resDesc = resource->GetDesc();

  resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  resDesc.Alignment = 0;
  resDesc.DepthOrArraySize = 1;
  resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  resDesc.MipLevels = (UINT16)CalcNumMips((int)resDesc.Width, (int)resDesc.Height, 1);
  resDesc.SampleDesc.Count = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

  D3D12_RESOURCE_DESC customTexDesc = {};

  if(m_CustomShaderTex)
    customTexDesc = m_CustomShaderTex->GetDesc();

  if(customTexDesc.Width != resDesc.Width || customTexDesc.Height != resDesc.Height)
  {
    SAFE_RELEASE(m_CustomShaderTex);

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    HRESULT hr = m_pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&m_CustomShaderTex);
    RDCASSERTEQUAL(hr, S_OK);

    if(m_CustomShaderTex)
    {
      m_CustomShaderTex->SetName(L"m_CustomShaderTex");
      m_CustomShaderResourceId = GetResID(m_CustomShaderTex);
    }
    else
    {
      m_CustomShaderResourceId = ResourceId();
    }
  }

  if(m_CustomShaderResourceId == ResourceId())
    return m_CustomShaderResourceId;

  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  rtvDesc.Texture2D.MipSlice = mip;

  m_pDevice->CreateRenderTargetView(m_CustomShaderTex, &rtvDesc,
                                    GetDebugManager()->GetCPUHandle(CUSTOM_SHADER_RTV));

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();

  float clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
  list->ClearRenderTargetView(GetDebugManager()->GetCPUHandle(CUSTOM_SHADER_RTV), clr, 0, NULL);

  list->Close();

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = shader;
  disp.resourceId = texid;
  disp.typeHint = typeHint;
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.mip = mip;
  disp.sampleIdx = sampleIdx;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = 0.0f;
  disp.rangeMax = 1.0f;
  disp.rawOutput = false;
  disp.scale = 1.0f;
  disp.sliceFace = arrayIdx;

  SetOutputDimensions(RDCMAX(1U, (UINT)resDesc.Width >> mip), RDCMAX(1U, resDesc.Height >> mip));

  RenderTextureInternal(GetDebugManager()->GetCPUHandle(CUSTOM_SHADER_RTV), disp,
                        eTexDisplay_BlendAlpha);

  return m_CustomShaderResourceId;
}

#pragma region not yet implemented

vector<PixelModification> D3D12Replay::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                    uint32_t x, uint32_t y, uint32_t slice,
                                                    uint32_t mip, uint32_t sampleIdx,
                                                    CompType typeHint)
{
  return vector<PixelModification>();
}

ShaderDebugTrace D3D12Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  return ShaderDebugTrace();
}

ShaderDebugTrace D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  return ShaderDebugTrace();
}

ShaderDebugTrace D3D12Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  return ShaderDebugTrace();
}

ResourceId D3D12Replay::CreateProxyTexture(const TextureDescription &templateTex)
{
  return ResourceId();
}

void D3D12Replay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                      size_t dataSize)
{
}

ResourceId D3D12Replay::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  return ResourceId();
}

void D3D12Replay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
}

#pragma endregion

ReplayStatus D3D12_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D12 replay device");

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D12DeviceIfAlloc);

  HMODULE lib = NULL;
  lib = LoadLibraryA("d3d12.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d12.dll");
    return ReplayStatus::APIInitFailed;
  }

  lib = LoadLibraryA("dxgi.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load dxgi.dll");
    return ReplayStatus::APIInitFailed;
  }

  if(GetD3DCompiler() == NULL)
  {
    RDCERR("Failed to load d3dcompiler_??.dll");
    return ReplayStatus::APIInitFailed;
  }

  D3D12InitParams initParams;

  uint64_t ver = D3D12InitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!D3D12InitParams::IsSupportedVersion(ver))
    {
      RDCERR("Incompatible D3D12 serialise version %llu", ver);
      return ReplayStatus::APIIncompatibleVersion;
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RDCERR("Expected to get a DriverInit chunk, instead got %u", chunk);
      return ReplayStatus::FileCorrupted;
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      RDCERR("Failed reading driver init params.");
      return ReplayStatus::FileIOFailed;
    }
  }

  if(initParams.MinimumFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    initParams.MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  ID3D12Device *dev = NULL;
  HRESULT hr = RENDERDOC_CreateWrappedD3D12Device(NULL, initParams.MinimumFeatureLevel,
                                                  __uuidof(ID3D12Device), (void **)&dev);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create a d3d12 device :(.");

    return ReplayStatus::APIHardwareUnsupported;
  }

  WrappedID3D12Device *wrappedDev = (WrappedID3D12Device *)dev;
  wrappedDev->SetInitParams(initParams, ver);

  RDCLOG("Created device.");
  D3D12Replay *replay = wrappedDev->GetReplay();

  replay->SetProxy(rdc == NULL);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}

static DriverRegistration D3D12DriverRegistration(RDCDriver::D3D12, &D3D12_CreateReplayDevice);

void D3D12_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedID3D12Device device(NULL, NULL);

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = device.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    device.GetStructuredFile().Swap(output);
}

static StructuredProcessRegistration D3D12ProcessRegistration(RDCDriver::D3D12,
                                                              &D3D12_ProcessStructured);
