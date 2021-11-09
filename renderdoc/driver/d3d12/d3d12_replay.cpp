/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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
#include "core/plugins.h"
#include "core/settings.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/amd/amd_rgp.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_hooks.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

RDOC_CONFIG(bool, D3D12_HardwareCounters, true,
            "Enable support for IHV-specific hardware counters on D3D12.");

// this is global so we can free it even after D3D12Replay is destroyed
static HMODULE D3D12Lib = NULL;

static const char *LiveDriverDisassemblyTarget = "Live driver disassembly";

ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev);

static const char *DXBCDXILDisassemblyTarget = "DXBC/DXIL";

D3D12Replay::D3D12Replay(WrappedID3D12Device *d)
{
  m_pDevice = d;
  m_HighlightCache.driver = this;
}

void D3D12Replay::Shutdown()
{
  for(size_t i = 0; i < m_ProxyResources.size(); i++)
    m_ProxyResources[i]->Release();
  m_ProxyResources.clear();

  SAFE_DELETE(m_RGP);

  m_pDevice->Release();

  // the this pointer is free'd after this point

  FreeLibrary(D3D12Lib);
  D3D12_CleanupReplaySDK();
}

void D3D12Replay::Initialise(IDXGIFactory1 *factory)
{
  m_pFactory = factory;

  RDCEraseEl(m_DriverInfo);

  if(m_pFactory)
  {
    RefCountDXGIObject::HandleWrap("D3D12Replay", __uuidof(IDXGIFactory1), (void **)&m_pFactory);

    LUID luid = m_pDevice->GetAdapterLuid();

    IDXGIAdapter *pDXGIAdapter = NULL;
    HRESULT hr = EnumAdapterByLuid(m_pFactory, luid, &pDXGIAdapter);

    if(FAILED(hr))
    {
      RDCERR("Couldn't get DXGI adapter by LUID from D3D device");
    }
    else
    {
      DXGI_ADAPTER_DESC desc = {};
      pDXGIAdapter->GetDesc(&desc);

      m_DriverInfo.vendor = GPUVendorFromPCIVendor(desc.VendorId);

      rdcstr descString = GetDriverVersion(desc);
      descString.resize(RDCMIN(descString.size(), ARRAY_COUNT(m_DriverInfo.version) - 1));
      memcpy(m_DriverInfo.version, descString.c_str(), descString.size());

      RDCLOG("Running replay on %s / %s", ToStr(m_DriverInfo.vendor).c_str(), m_DriverInfo.version);

      SAFE_RELEASE(pDXGIAdapter);
    }
  }
}

ReplayStatus D3D12Replay::FatalErrorCheck()
{
  return m_pDevice->FatalErrorCheck();
}

IReplayDriver *D3D12Replay::MakeDummyDriver()
{
  // gather up the shaders we've allocated to pass to the dummy driver
  rdcarray<ShaderReflection *> shaders;
  WrappedID3D12Shader::GetReflections(shaders);

  IReplayDriver *dummy = new DummyDriver(this, shaders, m_pDevice->DetachStructuredFile());

  return dummy;
}

void D3D12Replay::CreateResources()
{
  m_DebugManager = new D3D12DebugManager(m_pDevice);

  if(RenderDoc::Inst().IsReplayApp())
  {
    CreateSOBuffers();

    if(m_pDevice->UsedDXIL())
      RDCLOG("Replaying with DXIL enabled");

    m_General.Init(m_pDevice, m_DebugManager);
    m_TexRender.Init(m_pDevice, m_DebugManager);
    m_Overlay.Init(m_pDevice, m_DebugManager);
    m_VertexPick.Init(m_pDevice, m_DebugManager);
    m_PixelPick.Init(m_pDevice, m_DebugManager);
    m_Histogram.Init(m_pDevice, m_DebugManager);

    if(!m_Proxy && D3D12_HardwareCounters())
    {
      AMDCounters *counters = NULL;

      if(m_DriverInfo.vendor == GPUVendor::AMD)
      {
        RDCLOG("AMD GPU detected - trying to initialise AMD counters");
        counters = new AMDCounters(m_pDevice->IsDebugLayerEnabled());
      }
      else
      {
        RDCLOG("%s GPU detected - no counters available", ToStr(m_DriverInfo.vendor).c_str());
      }

      ID3D12Device *d3dDevice = m_pDevice->GetReal();

      if(counters && counters->Init(AMDCounters::ApiType::Dx12, (void *)d3dDevice))
      {
        m_pAMDCounters = counters;
      }
      else
      {
        delete counters;
        m_pAMDCounters = NULL;
      }
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
  ClearFeedbackCache();

  m_General.Release();
  m_TexRender.Release();
  m_Overlay.Release();
  m_VertexPick.Release();
  m_PixelPick.Release();
  m_Histogram.Release();

  SAFE_RELEASE(m_BindlessFeedback.FeedbackBuffer);

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);
  SAFE_RELEASE(m_SOPatchedIndexBuffer);
  SAFE_RELEASE(m_SOQueryHeap);

  SAFE_RELEASE(m_pFactory);

  SAFE_RELEASE(m_CustomShaderTex);

  SAFE_DELETE(m_DebugManager);

  SAFE_DELETE(m_pAMDCounters);
}

ReplayStatus D3D12Replay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDevice->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

rdcarray<GPUDevice> D3D12Replay::GetAvailableGPUs()
{
  rdcarray<GPUDevice> ret;

  for(UINT i = 0; i < 10; i++)
  {
    IDXGIAdapter *adapter = NULL;

    HRESULT hr = m_pFactory->EnumAdapters(i, &adapter);

    if(SUCCEEDED(hr) && adapter)
    {
      DXGI_ADAPTER_DESC desc;
      adapter->GetDesc(&desc);

      GPUDevice dev;
      dev.vendor = GPUVendorFromPCIVendor(desc.VendorId);
      dev.deviceID = desc.DeviceId;
      dev.driver = "";    // D3D doesn't have multiple drivers per API
      dev.name = StringFormat::Wide2UTF8(desc.Description);
      dev.apis = {GraphicsAPI::D3D12};

      // don't add duplicate devices even if they get enumerated. Don't add WARP, we'll do that
      // manually since it's inconsistently enumerated
      if(ret.indexOf(dev) == -1 && dev.vendor != GPUVendor::Software)
        ret.push_back(dev);
    }

    SAFE_RELEASE(adapter);
  }

  // add WARP as long as we're not 12On7 (where we can't use WARP)
  if(!m_D3D12On7)
  {
    GPUDevice dev;
    dev.vendor = GPUVendor::Software;
    dev.deviceID = 0;
    dev.driver = "";    // D3D doesn't have multiple drivers per API
    dev.name = "WARP Rasterizer";
    dev.apis = {GraphicsAPI::D3D12};
    ret.push_back(dev);
  }

  return ret;
}

APIProperties D3D12Replay::GetAPIProperties()
{
  APIProperties ret = m_pDevice->APIProps;

  ret.pipelineType = GraphicsAPI::D3D12;
  ret.localRenderer = GraphicsAPI::D3D12;
  ret.vendor = m_DriverInfo.vendor;
  ret.degraded = false;
  ret.shadersMutable = false;
  ret.rgpCapture =
      m_DriverInfo.vendor == GPUVendor::AMD && m_RGP != NULL && m_RGP->DriverSupportsInterop();
  ret.shaderDebugging = true;

  return ret;
}

void D3D12Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

SDFile *D3D12Replay::GetStructuredFile()
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

rdcarray<ResourceDescription> D3D12Replay::GetResources()
{
  return m_Resources;
}

rdcarray<BufferDescription> D3D12Replay::GetBuffers()
{
  rdcarray<BufferDescription> ret;

  for(auto it = m_pDevice->GetResourceList().begin(); it != m_pDevice->GetResourceList().end(); it++)
    if(it->second->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      ret.push_back(GetBuffer(it->first));

  return ret;
}

rdcarray<TextureDescription> D3D12Replay::GetTextures()
{
  rdcarray<TextureDescription> ret;

  for(auto it = m_pDevice->GetResourceList().begin(); it != m_pDevice->GetResourceList().end(); it++)
  {
    if(it->second->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
       m_pDevice->GetResourceManager()->GetOriginalID(it->first) != it->first)
      ret.push_back(GetTexture(it->first));
  }

  return ret;
}

BufferDescription D3D12Replay::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};
  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = m_pDevice->GetResourceList().find(id);

  if(it == m_pDevice->GetResourceList().end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

  ret.length = desc.Width;

  ret.creationFlags = BufferCategory::NoFlags;

  const rdcarray<EventUsage> &usage = m_pDevice->GetQueue()->GetUsage(id);

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

  auto it = m_pDevice->GetResourceList().find(id);

  if(it == m_pDevice->GetResourceList().end())
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
  ret.byteSize *= ret.msSamp;

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

  {
    auto resit = m_ResourceIdx.find(ret.resourceId);
    if(resit != m_ResourceIdx.end() && m_Resources[resit->second].type == ResourceType::SwapchainImage)
    {
      ret.format = MakeResourceFormat(GetTypedFormat(desc.Format, CompType::UNorm));
      ret.creationFlags |= TextureCategory::SwapBuffer;
    }
  }

  return ret;
}

rdcarray<ShaderEntryPoint> D3D12Replay::GetShaderEntryPoints(ResourceId shader)
{
  if(!WrappedID3D12Shader::IsShader(shader))
    return {};

  ID3D12DeviceChild *res = m_pDevice->GetResourceManager()->GetCurrentResource(shader);

  if(!res)
    return {};

  WrappedID3D12Shader *sh = (WrappedID3D12Shader *)res;

  ShaderReflection &ret = sh->GetDetails();

  return {{"main", ret.stage}};
}

ShaderReflection *D3D12Replay::GetShader(ResourceId pipeline, ResourceId shader,
                                         ShaderEntryPoint entry)
{
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(shader);

  if(sh)
    return &sh->GetDetails();

  return NULL;
}

rdcarray<rdcstr> D3D12Replay::GetDisassemblyTargets(bool withPipeline)
{
  rdcarray<rdcstr> ret;

  // DXBC is always first
  ret.push_back(DXBCDXILDisassemblyTarget);

  if(!m_ISAChecked && m_TexRender.BlendPipe)
  {
    m_ISAChecked = true;

    UINT size = 0;
    m_TexRender.BlendPipe->GetPrivateData(WKPDID_CommentStringW, &size, NULL);

    if(size > 0)
    {
      byte *isa = new byte[size + 1];
      m_TexRender.BlendPipe->GetPrivateData(WKPDID_CommentStringW, &size, isa);

      if(strstr((const char *)isa, "disassembly requires a participating driver") == NULL &&
         wcsstr((const wchar_t *)isa, L"disassembly requires a participating driver") == NULL)
      {
        m_ISAAvailable =
            strstr((const char *)isa, "<shader") || wcsstr((const wchar_t *)isa, L"<shader");
      }

      delete[] isa;
    }
  }

  if(m_ISAAvailable)
    ret.push_back(LiveDriverDisassemblyTarget);

  return ret;
}

rdcstr D3D12Replay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const rdcstr &target)
{
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetLiveAs<WrappedID3D12Shader>(refl->resourceId);

  if(!sh)
    return "; Invalid Shader Specified";

  DXBC::DXBCContainer *dxbc = sh->GetDXBC();

  if(target == DXBCDXILDisassemblyTarget || target.empty())
    return dxbc->GetDisassembly();

  if(target == LiveDriverDisassemblyTarget)
  {
    if(pipeline == ResourceId())
    {
      return "; No pipeline specified, live driver disassembly is not available\n"
             "; Shader must be disassembled with a specific pipeline to get live driver assembly.";
    }

    WrappedID3D12PipelineState *pipe =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(pipeline);

    UINT size = 0;
    pipe->GetPrivateData(WKPDID_CommentStringW, &size, NULL);

    if(size == 0)
      return "; Unknown error fetching disassembly, empty string returned\n";

    byte *data = new byte[size + 1];
    memset(data, 0, size);
    pipe->GetPrivateData(WKPDID_CommentStringW, &size, data);

    byte *iter = data;

    // need to handle wide and ascii strings to some extent. We assume that it doesn't change from
    // character to character, we just handle the initial <comments> being either one, then assume
    // the xml within is all consistent.
    if(!strncmp((char *)iter, "<comments", 9))
    {
      iter += 9;

      // find the end of the tag, advance past it
      iter = (byte *)strchr((char *)iter, '>') + 1;

      if(*(char *)iter == '\n')
        iter++;
    }
    else if(!wcsncmp((wchar_t *)iter, L"<comments", 9))
    {
      iter += 9 * sizeof(wchar_t);

      // find the end of the tag
      iter = (byte *)wcschr((wchar_t *)iter, L'>') + sizeof(wchar_t);

      if(*(wchar_t *)iter == L'\n')
        iter += sizeof(wchar_t);
    }
    else
      return "; Unknown error fetching disassembly, invalid string returned\n\n\n" +
             rdcstr((char *)data, size);

    rdcstr contents;

    // decode the <shader><comment> tags
    if(!strncmp((char *)iter, "<shader", 7))
      contents = (char *)iter;
    else if(!wcsncmp((wchar_t *)iter, L"<shader", 7))
      contents = StringFormat::Wide2UTF8((wchar_t *)iter);

    delete[] data;

    const char *search = "stage=\"?S\"";

    switch(refl->stage)
    {
      case ShaderStage::Vertex: search = "stage=\"VS\""; break;
      case ShaderStage::Hull: search = "stage=\"HS\""; break;
      case ShaderStage::Domain: search = "stage=\"DS\""; break;
      case ShaderStage::Geometry: search = "stage=\"GS\""; break;
      case ShaderStage::Pixel: search = "stage=\"PS\""; break;
      case ShaderStage::Compute: search = "stage=\"CS\""; break;
      default: return "; Unknown shader stage in shader reflection\n";
    }

    int32_t idx = contents.find(search);

    if(idx < 0)
      return "; Couldn't find disassembly for given shader stage in returned string\n\n\n" + contents;

    idx += 11;    // stage=".S">

    if(strncmp(contents.c_str() + idx, "<comment>", 9) != 0)
      return "; Unknown error fetching disassembly, invalid string returned\n\n\n" + contents;

    idx += 9;    // <comment>

    if(strncmp(contents.c_str() + idx, "<![CDATA[\n", 10) != 0)
      return "; Unknown error fetching disassembly, invalid string returned\n\n\n" + contents;

    idx += 10;    // <![CDATA[\n

    int32_t end = contents.find("]]></comment>", idx);

    if(end < 0)
      return "; Unknown error fetching disassembly, invalid string returned\n\n\n" + contents;

    return contents.substr(idx, end - idx);
  }

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

ResourceId D3D12Replay::GetLiveID(ResourceId id)
{
  if(!m_pDevice->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

rdcarray<EventUsage> D3D12Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetQueue()->GetUsage(id);
}

void D3D12Replay::FillResourceView(D3D12Pipe::View &view, const D3D12Descriptor *desc)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(desc->GetType() == D3D12DescriptorType::Sampler || desc->GetType() == D3D12DescriptorType::CBV)
  {
    return;
  }

  view.resourceId = rm->GetOriginalID(desc->GetResResourceId());

  if(view.resourceId == ResourceId())
    return;

  D3D12_RESOURCE_DESC res;

  {
    ID3D12Resource *r = rm->GetCurrentAs<ID3D12Resource>(desc->GetResResourceId());
    res = r->GetDesc();
  }

  {
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    if(desc->GetType() == D3D12DescriptorType::RTV)
    {
      const D3D12_RENDER_TARGET_VIEW_DESC &rtv = desc->GetRTV();

      fmt = rtv.Format;

      view.type = MakeTextureDim(rtv.ViewDimension);

      if(rtv.ViewDimension == D3D12_RTV_DIMENSION_BUFFER)
      {
        view.firstElement = rtv.Buffer.FirstElement;
        view.numElements = rtv.Buffer.NumElements;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = rtv.Texture1D.MipSlice;
        view.numMips = 1;
        view.firstSlice = 0;
        view.numSlices = 1;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1DARRAY)
      {
        view.firstMip = rtv.Texture1DArray.MipSlice;
        view.numMips = 1;
        view.numSlices = rtv.Texture1DArray.ArraySize;
        view.firstSlice = rtv.Texture1DArray.FirstArraySlice;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = rtv.Texture2D.MipSlice;
        view.numMips = 1;
        view.firstSlice = 0;
        view.numSlices = 1;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = rtv.Texture2DArray.ArraySize;
        view.firstSlice = rtv.Texture2DArray.FirstArraySlice;
        view.firstMip = rtv.Texture2DArray.MipSlice;
        view.numMips = 1;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMS)
      {
        view.firstMip = 0;
        view.numMips = 1;
        view.firstSlice = 0;
        view.numSlices = 1;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.firstMip = 0;
        view.numMips = 1;
        view.numSlices = rtv.Texture2DMSArray.ArraySize;
        view.firstSlice = rtv.Texture2DMSArray.FirstArraySlice;
      }
      else if(rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE3D)
      {
        view.numSlices = rtv.Texture3D.WSize;
        view.firstSlice = rtv.Texture3D.FirstWSlice;
        view.firstMip = rtv.Texture3D.MipSlice;
        view.numMips = 1;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::DSV)
    {
      const D3D12_DEPTH_STENCIL_VIEW_DESC &dsv = desc->GetDSV();

      fmt = dsv.Format;

      view.type = MakeTextureDim(dsv.ViewDimension);

      if(dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = dsv.Texture1D.MipSlice;
      }
      else if(dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = dsv.Texture1DArray.ArraySize;
        view.firstSlice = dsv.Texture1DArray.FirstArraySlice;
        view.firstMip = dsv.Texture1DArray.MipSlice;
      }
      else if(dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = dsv.Texture2D.MipSlice;
      }
      else if(dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = dsv.Texture2DArray.ArraySize;
        view.firstSlice = dsv.Texture2DArray.FirstArraySlice;
        view.firstMip = dsv.Texture2DArray.MipSlice;
      }
      else if(dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = dsv.Texture2DMSArray.ArraySize;
        view.firstSlice = dsv.Texture2DMSArray.FirstArraySlice;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::SRV)
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srv = desc->GetSRV();

      if(srv.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
        srv = MakeSRVDesc(res);

      fmt = srv.Format;

      view.type = MakeTextureDim(srv.ViewDimension);

      view.swizzle.red =
          (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(0, srv.Shader4ComponentMapping);
      view.swizzle.green =
          (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(1, srv.Shader4ComponentMapping);
      view.swizzle.blue =
          (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(2, srv.Shader4ComponentMapping);
      view.swizzle.alpha =
          (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(3, srv.Shader4ComponentMapping);

      if(srv.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
      {
        view.firstElement = srv.Buffer.FirstElement;
        view.numElements = srv.Buffer.NumElements;

        view.bufferFlags = MakeBufferFlags(srv.Buffer.Flags);
        if(srv.Buffer.StructureByteStride > 0)
          view.elementByteSize = srv.Buffer.StructureByteStride;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = srv.Texture1D.MostDetailedMip;
        view.numMips = srv.Texture1D.MipLevels;
        view.minLODClamp = srv.Texture1D.ResourceMinLODClamp;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = srv.Texture1DArray.ArraySize;
        view.firstSlice = srv.Texture1DArray.FirstArraySlice;
        view.firstMip = srv.Texture1DArray.MostDetailedMip;
        view.numMips = srv.Texture1DArray.MipLevels;
        view.minLODClamp = srv.Texture1DArray.ResourceMinLODClamp;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = srv.Texture2D.MostDetailedMip;
        view.numMips = srv.Texture2D.MipLevels;
        view.minLODClamp = srv.Texture2D.ResourceMinLODClamp;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = srv.Texture2DArray.ArraySize;
        view.firstSlice = srv.Texture2DArray.FirstArraySlice;
        view.firstMip = srv.Texture2DArray.MostDetailedMip;
        view.numMips = srv.Texture2DArray.MipLevels;
        view.minLODClamp = srv.Texture2DArray.ResourceMinLODClamp;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = srv.Texture2DMSArray.ArraySize;
        view.firstSlice = srv.Texture2DMSArray.FirstArraySlice;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D)
      {
        view.firstMip = srv.Texture3D.MostDetailedMip;
        view.numMips = srv.Texture3D.MipLevels;
        view.minLODClamp = srv.Texture3D.ResourceMinLODClamp;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
      {
        view.numSlices = 6;
        view.firstMip = srv.TextureCube.MostDetailedMip;
        view.numMips = srv.TextureCube.MipLevels;
        view.minLODClamp = srv.TextureCube.ResourceMinLODClamp;
      }
      else if(srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY)
      {
        view.numSlices = srv.TextureCubeArray.NumCubes * 6;
        view.firstSlice = srv.TextureCubeArray.First2DArrayFace;
        view.firstMip = srv.TextureCubeArray.MostDetailedMip;
        view.numMips = srv.TextureCubeArray.MipLevels;
        view.minLODClamp = srv.TextureCube.ResourceMinLODClamp;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::UAV)
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uav = desc->GetUAV();

      if(uav.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
        uav = MakeUAVDesc(res);

      fmt = uav.Format;

      view.counterResourceId = rm->GetOriginalID(desc->GetCounterResourceId());

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
          GetDebugManager()->GetBufferData(
              rm->GetCurrentAs<ID3D12Resource>(desc->GetCounterResourceId()),
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

    if(fmt == DXGI_FORMAT_UNKNOWN)
      fmt = res.Format;

    if(view.elementByteSize == 0)
      view.elementByteSize = fmt == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, fmt, 0);

    view.viewFormat = MakeResourceFormat(fmt);
  }
}

ShaderStageMask ToShaderStageMask(D3D12_SHADER_VISIBILITY vis)
{
  switch(vis)
  {
    case D3D12_SHADER_VISIBILITY_ALL: return ShaderStageMask::All;
    case D3D12_SHADER_VISIBILITY_VERTEX: return ShaderStageMask::Vertex;
    case D3D12_SHADER_VISIBILITY_HULL: return ShaderStageMask::Hull;
    case D3D12_SHADER_VISIBILITY_DOMAIN: return ShaderStageMask::Domain;
    case D3D12_SHADER_VISIBILITY_GEOMETRY: return ShaderStageMask::Geometry;
    case D3D12_SHADER_VISIBILITY_PIXEL: return ShaderStageMask::Pixel;
    default: return ShaderStageMask::Unknown;
  }
}

void D3D12Replay::FillRootElements(uint32_t eventId, const D3D12RenderState::RootSignature &rootSig,
                                   const ShaderBindpointMapping *mappings[(uint32_t)ShaderStage::Count],
                                   rdcarray<D3D12Pipe::RootSignatureRange> &rootElements)
{
  if(rootSig.rootsig == ResourceId())
    return;

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  const D3D12DynamicShaderFeedback &usage = m_BindlessFeedback.Usage[eventId];

  const D3D12FeedbackBindIdentifier *curUsage = usage.used.begin();
  const D3D12FeedbackBindIdentifier *lastUsage = usage.used.end();
  D3D12FeedbackBindIdentifier curIdentifier;

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootSig.rootsig);

  rootElements.reserve(sig->sig.Parameters.size() + sig->sig.StaticSamplers.size());

  size_t ridx = 0;

  for(size_t rootEl = 0; rootEl < sig->sig.Parameters.size(); rootEl++)
  {
    curIdentifier.rootEl = rootEl;

    const D3D12RootSignatureParameter &p = sig->sig.Parameters[rootEl];

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      rootElements.resize_for_index(ridx);
      D3D12Pipe::RootSignatureRange &element = rootElements[ridx++];
      element.immediate = true;
      element.rootSignatureIndex = (uint32_t)rootEl;
      element.type = BindType::ConstantBuffer;
      element.registerSpace = p.Constants.RegisterSpace;
      element.visibility = ToShaderStageMask(p.ShaderVisibility);

      element.samplers.clear();
      element.constantBuffers.clear();
      element.views.clear();

      element.constantBuffers.push_back(D3D12Pipe::ConstantBuffer(p.Constants.ShaderRegister));
      D3D12Pipe::ConstantBuffer &cb = element.constantBuffers.back();
      cb.resourceId = ResourceId();
      cb.byteOffset = 0;
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
      rootElements.resize_for_index(ridx);
      D3D12Pipe::RootSignatureRange &element = rootElements[ridx++];
      element.immediate = true;
      element.rootSignatureIndex = (uint32_t)rootEl;
      element.type = BindType::ConstantBuffer;
      element.registerSpace = p.Descriptor.RegisterSpace;
      element.visibility = ToShaderStageMask(p.ShaderVisibility);

      element.samplers.clear();
      element.constantBuffers.clear();
      element.views.clear();

      element.constantBuffers.push_back(D3D12Pipe::ConstantBuffer(p.Descriptor.ShaderRegister));
      D3D12Pipe::ConstantBuffer &cb = element.constantBuffers.back();

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootCBV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          cb.resourceId = rm->GetOriginalID(e.id);
          cb.byteOffset = e.offset;
          if(res)
            cb.byteSize = uint32_t(res->GetDesc().Width - cb.byteOffset);
          else
            cb.byteSize = 0;
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV)
    {
      rootElements.resize_for_index(ridx);
      D3D12Pipe::RootSignatureRange &element = rootElements[ridx++];
      element.immediate = true;
      element.rootSignatureIndex = (uint32_t)rootEl;
      element.type = BindType::ReadOnlyResource;
      element.registerSpace = p.Descriptor.RegisterSpace;
      element.visibility = ToShaderStageMask(p.ShaderVisibility);

      element.samplers.clear();
      element.constantBuffers.clear();
      element.views.clear();

      element.views.push_back(D3D12Pipe::View(p.Descriptor.ShaderRegister));
      D3D12Pipe::View &view = element.views.back();

      view.dynamicallyUsed = true;

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
          if(res)
            view.numElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
          else
            view.numElements = 0;
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
    {
      rootElements.resize_for_index(ridx);
      D3D12Pipe::RootSignatureRange &element = rootElements[ridx++];
      element.immediate = true;
      element.rootSignatureIndex = (uint32_t)rootEl;
      element.type = BindType::ReadWriteResource;
      element.registerSpace = p.Descriptor.RegisterSpace;
      element.visibility = ToShaderStageMask(p.ShaderVisibility);

      element.samplers.clear();
      element.constantBuffers.clear();
      element.views.clear();

      element.views.push_back(D3D12Pipe::View(p.Descriptor.ShaderRegister));
      D3D12Pipe::View &view = element.views.back();

      view.dynamicallyUsed = true;

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
          if(res)
            view.numElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
          else
            view.numElements = 0;
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

        curIdentifier.rangeIndex = r;

        // Here we diverge slightly from how root signatures store data. A descriptor table can
        // contain multiple ranges which can each contain different types. D3D12Pipe treats
        // each range as a separate RootElement
        rootElements.resize_for_index(ridx);
        D3D12Pipe::RootSignatureRange &element = rootElements[ridx++];

        element.immediate = false;
        element.rootSignatureIndex = (uint32_t)rootEl;
        element.registerSpace = range.RegisterSpace;
        element.visibility = ToShaderStageMask(p.ShaderVisibility);

        element.samplers.clear();
        element.constantBuffers.clear();
        element.views.clear();

        UINT shaderReg = range.BaseShaderRegister;

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

          if(num >= heap->GetNumDescriptors())
          {
            UINT availDescriptors = heap->GetNumDescriptors() - offset - UINT(e->offset);

            const Bindpoint *highestBind = NULL;
            UINT maxBindReg = shaderReg + availDescriptors;

            // Find shader binds that map any or all of this range to see if we can trim it
            for(ShaderStage stage = ShaderStage::Vertex; stage < ShaderStage::Count; ++stage)
            {
              const ShaderBindpointMapping *mapping = mappings[(uint32_t)stage];
              if((element.visibility & MaskForStage(stage)) != ShaderStageMask::Unknown)
              {
                // This range is visible to this shader stage, check its mappings
                const rdcarray<Bindpoint> &bps = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV
                                                     ? mapping->readWriteResources
                                                     : mapping->readOnlyResources;
                for(size_t b = 0; b < bps.size(); ++b)
                {
                  if(bps[b].bindset == (int32_t)element.registerSpace &&
                     bps[b].bind >= (int32_t)shaderReg)
                  {
                    UINT bindEnd = bps[b].arraySize == ~0U ? ~0U : bps[b].bind + bps[b].arraySize;
                    if(highestBind == NULL || bindEnd > maxBindReg)
                    {
                      highestBind = &bps[b];
                      maxBindReg = bindEnd;
                    }
                  }
                }
              }
            }

            // If we didn't find a bindpoint, this will evaluate to simply availDescriptors - the
            // number of remaining entries in the heap. If we did find a bind in this range, it
            // will trim to the max register used by bindings in the range.
            num = RDCMIN(availDescriptors, maxBindReg - shaderReg);
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
          element.type = BindType::Sampler;

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            element.samplers.push_back(D3D12Pipe::Sampler(shaderReg));
            D3D12Pipe::Sampler &samp = element.samplers.back();
            samp.tableIndex = offset + i;

            if(desc)
            {
              const D3D12_SAMPLER_DESC &sampDesc = desc->GetSampler();

              samp.addressU = MakeAddressMode(sampDesc.AddressU);
              samp.addressV = MakeAddressMode(sampDesc.AddressV);
              samp.addressW = MakeAddressMode(sampDesc.AddressW);

              samp.borderColor = sampDesc.BorderColor;

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
          element.type = BindType::ConstantBuffer;

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            element.constantBuffers.push_back(D3D12Pipe::ConstantBuffer(shaderReg));
            D3D12Pipe::ConstantBuffer &cb = element.constantBuffers.back();
            cb.tableIndex = offset + i;

            if(desc)
            {
              const D3D12_CONSTANT_BUFFER_VIEW_DESC &cbv = desc->GetCBV();
              WrappedID3D12Resource::GetResIDFromAddr(cbv.BufferLocation, cb.resourceId,
                                                      cb.byteOffset);
              cb.resourceId = rm->GetOriginalID(cb.resourceId);
              cb.byteSize = cbv.SizeInBytes;

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ||
                range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
        {
          if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
            element.type = BindType::ReadOnlyResource;
          else
            element.type = BindType::ReadWriteResource;

          if(usage.valid)
            element.dynamicallyUsedCount = 0;

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            element.views.push_back(D3D12Pipe::View(shaderReg));
            D3D12Pipe::View &view = element.views.back();
            view.tableIndex = offset + i;

            if(usage.valid)
            {
              curIdentifier.descIndex = i;

              // if the usage data is valid we expect entries for all descriptors that are used.
              // Note that because of the messed up root signature/binding split, some of these
              // entries will just be statically inserted and not actually from dynamic feedback
              //
              // usage data is only valid when at least one array exists to have feedback
              if(curUsage >= lastUsage)
              {
                // if we exhausted the list, all other elements are unused
                view.dynamicallyUsed = false;
              }
              else
              {
                // we never saw the current value of curUsage (which is odd, we should have
                // when iterating over all descriptors. This could only happen if there's some
                // layout mismatch or a feedback bug that lead to an invalid entry in the list).
                // Keep advancing until we get to one that is >= our current bind
                while(*curUsage < curIdentifier && curUsage < lastUsage)
                {
                  curUsage++;
                }

                // the next used bind is equal to this one. Mark it as dynamically used, and consume
                if(curUsage < lastUsage && *curUsage == curIdentifier)
                {
                  view.dynamicallyUsed = true;
                  curUsage++;
                }
                // the next used bind is after the current one, this is not used.
                else if(curUsage < lastUsage && curIdentifier < *curUsage)
                {
                  view.dynamicallyUsed = false;
                }
              }
            }
            else
            {
              view.dynamicallyUsed = true;
            }

            if(view.dynamicallyUsed)
            {
              element.dynamicallyUsedCount++;
              // we iterate in forward order, so we can unconditinoally set the last bind to the
              // current one, and only set the first bind if we haven't encountered one before
              element.lastUsedIndex = i;

              if(element.firstUsedIndex < 0)
                element.firstUsedIndex = i;
            }

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

  // Each static sampler gets its own RootElement
  for(size_t i = 0; i < sig->sig.StaticSamplers.size(); i++)
  {
    D3D12_STATIC_SAMPLER_DESC &sampDesc = sig->sig.StaticSamplers[i];

    rootElements.resize_for_index(ridx);
    D3D12Pipe::RootSignatureRange &element = rootElements[ridx++];
    element.immediate = true;
    element.rootSignatureIndex = (uint32_t)i;
    element.type = BindType::Sampler;
    element.registerSpace = sampDesc.RegisterSpace;
    element.visibility = ToShaderStageMask(sampDesc.ShaderVisibility);

    element.samplers.clear();
    element.constantBuffers.clear();
    element.views.clear();

    element.samplers.push_back(D3D12Pipe::Sampler(sampDesc.ShaderRegister));
    D3D12Pipe::Sampler &samp = element.samplers[0];

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

  rootElements.resize(ridx);
}

void D3D12Replay::SavePipelineState(uint32_t eventId)
{
  if(!m_D3D12PipelineState)
    return;

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12MarkerRegion::Begin(m_pDevice->GetQueue(),
                           StringFormat::Fmt("FetchShaderFeedback for %u", eventId));

  FetchShaderFeedback(eventId);

  D3D12MarkerRegion::End(m_pDevice->GetQueue());

  D3D12Pipe::State &state = *m_D3D12PipelineState;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  state.pipelineResourceId = rm->GetUnreplacedOriginalID(rs.pipe);

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
    state.inputAssembly.indexBuffer.byteStride = rs.ibuffer.bytewidth;

    state.inputAssembly.topology = MakePrimitiveTopology(rs.topo);
  }

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  if(pipe && pipe->IsCompute())
  {
    WrappedID3D12Shader *sh = (WrappedID3D12Shader *)pipe->compute->CS.pShaderBytecode;

    state.computeShader.resourceId = rm->GetUnreplacedOriginalID(sh->GetResourceID());
    state.computeShader.stage = ShaderStage::Compute;
    state.computeShader.reflection = &sh->GetDetails();
    state.computeShader.bindpointMapping = sh->GetMapping();
  }
  else if(pipe)
  {
    D3D12Pipe::Shader *dstArr[] = {&state.vertexShader, &state.hullShader, &state.domainShader,
                                   &state.geometryShader, &state.pixelShader};

    D3D12_SHADER_BYTECODE *srcArr[] = {&pipe->graphics->VS, &pipe->graphics->HS, &pipe->graphics->DS,
                                       &pipe->graphics->GS, &pipe->graphics->PS};

    for(size_t stage = 0; stage < 5; stage++)
    {
      D3D12Pipe::Shader &dst = *dstArr[stage];
      D3D12_SHADER_BYTECODE &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)src.pShaderBytecode;

      if(sh)
      {
        dst.resourceId = rm->GetUnreplacedOriginalID(sh->GetResourceID());
        dst.bindpointMapping = sh->GetMapping();
        dst.reflection = &sh->GetDetails();
      }
      else
      {
        dst.resourceId = ResourceId();
        dst.bindpointMapping = ShaderBindpointMapping();
        dst.reflection = NULL;
      }
    }
  }

  /////////////////////////////////////////////////
  // Root Signature
  /////////////////////////////////////////////////
  {
    const ShaderBindpointMapping *mappings[(uint32_t)ShaderStage::Count];
    mappings[(uint32_t)ShaderStage::Vertex] = &state.vertexShader.bindpointMapping;
    mappings[(uint32_t)ShaderStage::Hull] = &state.hullShader.bindpointMapping;
    mappings[(uint32_t)ShaderStage::Domain] = &state.domainShader.bindpointMapping;
    mappings[(uint32_t)ShaderStage::Geometry] = &state.geometryShader.bindpointMapping;
    mappings[(uint32_t)ShaderStage::Pixel] = &state.pixelShader.bindpointMapping;
    mappings[(uint32_t)ShaderStage::Compute] = &state.computeShader.bindpointMapping;

    if(pipe && pipe->IsCompute())
    {
      FillRootElements(eventId, rs.compute, mappings, state.rootElements);
      state.rootSignatureResourceId = rm->GetOriginalID(rs.compute.rootsig);
    }
    else if(pipe)
    {
      FillRootElements(eventId, rs.graphics, mappings, state.rootElements);
      state.rootSignatureResourceId = rm->GetOriginalID(rs.graphics.rootsig);
    }
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

      // D3D only supports overestimate conservative raster (underestimated can be emulated using
      // coverage information in the shader)
      dst.conservativeRasterization =
          src.ConservativeRaster == D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON
              ? ConservativeRaster::Overestimate
              : ConservativeRaster::Disabled;

      switch(rs.shadingRate)
      {
        default:
        case D3D12_SHADING_RATE_1X1: dst.baseShadingRate = {1, 1}; break;
        case D3D12_SHADING_RATE_1X2: dst.baseShadingRate = {1, 2}; break;
        case D3D12_SHADING_RATE_2X1: dst.baseShadingRate = {2, 1}; break;
        case D3D12_SHADING_RATE_2X2: dst.baseShadingRate = {2, 2}; break;
        case D3D12_SHADING_RATE_2X4: dst.baseShadingRate = {2, 4}; break;
        case D3D12_SHADING_RATE_4X2: dst.baseShadingRate = {4, 2}; break;
        case D3D12_SHADING_RATE_4X4: dst.baseShadingRate = {4, 4}; break;
      }

      ShadingRateCombiner combiners[2];

      for(int i = 0; i < 2; i++)
      {
        switch(rs.shadingRateCombiners[i])
        {
          default:
          case D3D12_SHADING_RATE_COMBINER_PASSTHROUGH:
            combiners[i] = ShadingRateCombiner::Passthrough;
            break;
          case D3D12_SHADING_RATE_COMBINER_OVERRIDE:
            combiners[i] = ShadingRateCombiner::Override;
            break;
          case D3D12_SHADING_RATE_COMBINER_MIN: combiners[i] = ShadingRateCombiner::Min; break;
          case D3D12_SHADING_RATE_COMBINER_MAX: combiners[i] = ShadingRateCombiner::Max; break;
          case D3D12_SHADING_RATE_COMBINER_SUM: combiners[i] = ShadingRateCombiner::Multiply; break;
        }
      }

      dst.shadingRateCombiners = {combiners[0], combiners[1]};

      dst.shadingRateImage = rm->GetOriginalID(rs.shadingRateImage);
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

    state.outputMerger.renderTargets.reserve(rs.rts.size());
    state.outputMerger.renderTargets.clear();
    for(size_t i = 0; i < rs.rts.size(); i++)
    {
      const D3D12Descriptor &desc = rs.rts[i];

      if(desc.GetResResourceId() != ResourceId())
      {
        state.outputMerger.renderTargets.push_back(D3D12Pipe::View((uint32_t)i));
        D3D12Pipe::View &view = state.outputMerger.renderTargets.back();

        FillResourceView(view, &desc);
      }
    }

    if(rs.dsv.GetResResourceId() != ResourceId())
    {
      FillResourceView(state.outputMerger.depthTarget, &rs.dsv);

      state.outputMerger.depthReadOnly = false;
      state.outputMerger.stencilReadOnly = false;

      if(rs.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH)
        state.outputMerger.depthReadOnly = true;
      if(rs.dsv.GetDSV().Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL)
        state.outputMerger.stencilReadOnly = true;
    }

    state.outputMerger.blendState.blendFactor = rs.blendFactor;

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
      D3D12_DEPTH_STENCIL_DESC1 &src = pipe->graphics->DepthStencilState;

      state.outputMerger.depthStencilState.depthEnable = src.DepthEnable == TRUE;
      state.outputMerger.depthStencilState.depthFunction = MakeCompareFunc(src.DepthFunc);
      state.outputMerger.depthStencilState.depthWrites =
          src.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
      state.outputMerger.depthStencilState.stencilEnable = src.StencilEnable == TRUE;
      state.outputMerger.depthStencilState.depthBoundsEnable = src.DepthBoundsTestEnable == TRUE;

      state.outputMerger.depthStencilState.minDepthBounds = rs.depthBoundsMin;
      state.outputMerger.depthStencilState.maxDepthBounds = rs.depthBoundsMax;

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

      if(res.states.empty())
        res.states.push_back({"Unknown"});

      i++;
    }
  }
}

void D3D12Replay::RenderHighlightBox(float w, float h, float scale)
{
  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
    if(!list)
      return;

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

void D3D12Replay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
  CheckerboardCBuffer pixelData = {};

  pixelData.PrimaryColor = ConvertSRGBToLinear(dark);
  pixelData.SecondaryColor = ConvertSRGBToLinear(light);
  pixelData.CheckerSquareDimension = 64.0f;

  D3D12_GPU_VIRTUAL_ADDRESS ps = GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));

  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
    if(!list)
      return;

    list->OMSetRenderTargets(1, &outw.rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, outw.width, outw.height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    list->SetPipelineState(outw.colResolve ? m_General.CheckerboardMSAAPipe
                                           : m_General.CheckerboardPipe);

    list->SetGraphicsRootSignature(m_General.CheckerboardRootSig);

    list->SetGraphicsRootConstantBufferView(0, ps);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(3, 1, 0, 0);

    list->Close();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }
}

uint32_t D3D12Replay::PickVertex(uint32_t eventId, int32_t width, int32_t height,
                                 const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  if(cfg.position.numIndices == 0)
    return ~0U;

  MeshPickData cbuf = {};

  cbuf.PickCoords = Vec2f((float)x, (float)y);
  cbuf.PickViewport = Vec2f((float)width, (float)height);
  cbuf.PickIdx = cfg.position.indexByteStride ? 1 : 0;
  cbuf.PickNumVerts = cfg.position.numIndices;
  cbuf.PickUnproject = cfg.position.unproject ? 1 : 0;
  cbuf.PickFlipY = cfg.position.flipY;
  cbuf.PickOrtho = cfg.ortho;

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(width) / float(height));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f pickMVP = projMat.Mul(camMat);
  if(!cfg.position.unproject)
  {
    pickMVP = pickMVP.Mul(Matrix4f(cfg.axisMapping));
  }

  bool reverseProjection = false;
  Matrix4f guessProj;
  Matrix4f guessProjInverse;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    if(cfg.position.farPlane != FLT_MAX)
    {
      guessProj =
          Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect);
    }
    else
    {
      reverseProjection = true;
      guessProj = Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);
    }

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    if(cfg.position.flipY)
      guessProj[5] *= -1.0f;

    guessProjInverse = guessProj.Inverse();
  }

  Vec3f rayPos;
  Vec3f rayDir;
  // convert mouse pos to world space ray
  {
    float pickX = ((float)x) / ((float)width);
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)height);
    // flip the Y axis by default for Y-up
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    if(cfg.position.flipY && !cfg.ortho)
      pickYCanonical = -pickYCanonical;

    // x/y is inside the window. Since we're not using the window projection we need to correct
    // for the aspect ratio here.
    if(cfg.position.unproject && !cfg.ortho)
      pickXCanonical *= (float(width) / float(height)) / cfg.aspect;

    // set up the NDC near/far pos
    Vec3f nearPosNDC = Vec3f(pickXCanonical, pickYCanonical, 0);
    Vec3f farPosNDC = Vec3f(pickXCanonical, pickYCanonical, 1);

    if(cfg.position.unproject && cfg.ortho)
    {
      // orthographic projections we raycast in NDC space
      Matrix4f inversePickMVP = pickMVP.Inverse();

      // transform from the desired NDC co-ordinates into camera space
      Vec3f nearPosCamera = inversePickMVP.Transform(nearPosNDC, 1);
      Vec3f farPosCamera = inversePickMVP.Transform(farPosNDC, 1);

      Vec3f testDir = (farPosCamera - nearPosCamera);
      testDir.Normalise();

      Matrix4f pickMVPguessProjInverse = guessProj.Mul(inversePickMVP);

      Vec3f nearPosProj = pickMVPguessProjInverse.Transform(nearPosNDC, 1);
      Vec3f farPosProj = pickMVPguessProjInverse.Transform(farPosNDC, 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      // Calculate the ray direction first in the regular way (above), so we can use the
      // the output for testing if the ray we are picking is negative or not. This is similar
      // to checking against the forward direction of the camera, but more robust
      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else if(cfg.position.unproject)
    {
      // projected data we pick in world-space to avoid problems with handling unusual transforms

      if(reverseProjection)
      {
        farPosNDC.z = 1e-6f;
        nearPosNDC.z = 1e+6f;
      }

      // invert the guessed projection matrix to get the near/far pos in camera space
      Vec3f nearPosCamera = guessProjInverse.Transform(nearPosNDC, 1.0f);
      Vec3f farPosCamera = guessProjInverse.Transform(farPosNDC, 1.0f);

      // normalise and generate the ray
      rayDir = (farPosCamera - nearPosCamera);
      rayDir.Normalise();

      farPosCamera = nearPosCamera + rayDir;

      // invert the camera transform to transform the ray as camera-relative into world space
      Matrix4f inverseCamera = camMat.Inverse();

      Vec3f nearPosWorld = inverseCamera.Transform(nearPosCamera, 1);
      Vec3f farPosWorld = inverseCamera.Transform(farPosCamera, 1);

      // again normalise our final ray
      rayDir = (farPosWorld - nearPosWorld);
      rayDir.Normalise();

      rayPos = nearPosWorld;
    }
    else
    {
      Matrix4f inversePickMVP = pickMVP.Inverse();

      // transform from the desired NDC co-ordinates into model space
      Vec3f nearPosCamera = inversePickMVP.Transform(nearPosNDC, 1);
      Vec3f farPosCamera = inversePickMVP.Transform(farPosNDC, 1);

      rayDir = (farPosCamera - nearPosCamera);
      rayDir.Normalise();
      rayPos = nearPosCamera;
    }
  }

  cbuf.PickRayPos = rayPos;
  cbuf.PickRayDir = rayDir;

  bool isTriangleMesh = true;
  switch(cfg.position.topology)
  {
    case Topology::TriangleList:
    {
      cbuf.PickMeshMode = MESH_TRIANGLE_LIST;
      break;
    }
    case Topology::TriangleStrip:
    {
      cbuf.PickMeshMode = MESH_TRIANGLE_STRIP;
      break;
    }
    case Topology::TriangleList_Adj:
    {
      cbuf.PickMeshMode = MESH_TRIANGLE_LIST_ADJ;
      break;
    }
    case Topology::TriangleStrip_Adj:
    {
      cbuf.PickMeshMode = MESH_TRIANGLE_STRIP_ADJ;
      break;
    }
    default:    // points, lines, patchlists, unknown
    {
      cbuf.PickMeshMode = MESH_OTHER;
      isTriangleMesh = false;
    }
  }

  if(cfg.position.unproject && isTriangleMesh)
  {
    // projected triangle meshes we transform the vertices into world space, and ray-cast against
    // that
    //
    // NOTE: for ortho, this matrix is not used and we just do the perspective W division on model
    // vertices. The ray is cast in NDC
    if(cfg.ortho)
      cbuf.PickTransformMat = Matrix4f::Identity();
    else
      cbuf.PickTransformMat = guessProjInverse;
  }
  else if(cfg.position.unproject)
  {
    // projected non-triangles are just point clouds, so we transform the vertices into world space
    // then project them back onto the output and compare that against the picking 2D co-ordinates
    cbuf.PickTransformMat = pickMVP.Mul(guessProjInverse);
  }
  else
  {
    // plain meshes of either type, we just transform from model space to the output, and raycast or
    // co-ordinate check
    cbuf.PickTransformMat = pickMVP;
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

      rdcarray<uint32_t> outidxs;
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
    maxIndex = RDCMIN(maxIndex, uint32_t(oldData.size() / RDCMAX(1U, cfg.position.vertexByteStride)));

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

    rdcarray<FloatVector> vbData;
    vbData.resize(maxIndex + 1);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

    // the index buffer may refer to vertices past the start of the vertex buffer, so we can't just
    // conver the first N vertices we'll need.
    // Instead we grab min and max above, and convert every vertex in that range. This might
    // slightly over-estimate but not as bad as 0-max or the whole buffer.
    for(uint32_t idx = minIndex; idx <= maxIndex; idx++)
      vbData[idx] = HighlightCache::InterpretVertex(data, idx, cfg.position.vertexByteStride,
                                                    cfg.position.format, dataEnd, valid);

    GetDebugManager()->FillBuffer(m_VertexPick.VB, 0, vbData.data(), sizeof(Vec4f) * (maxIndex + 1));
  }

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
  if(!list)
    return ~0U;

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
  if(!list)
    return ~0U;

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

void D3D12Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                            CompType typeCast, float pixel[4])
{
  SetOutputDimensions(1, 1);

  {
    TextureDisplay texDisplay;

    texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
    texDisplay.hdrMultiplier = -1.0f;
    texDisplay.linearDisplayAsGamma = true;
    texDisplay.flipY = false;
    texDisplay.subresource = sub;
    texDisplay.customShaderId = ResourceId();
    texDisplay.rangeMin = 0.0f;
    texDisplay.rangeMax = 1.0f;
    texDisplay.scale = 1.0f;
    texDisplay.resourceId = texture;
    texDisplay.typeCast = typeCast;
    texDisplay.rawOutput = true;
    texDisplay.xOffset = -float(x << sub.mip);
    texDisplay.yOffset = -float(y << sub.mip);

    m_OutputViewport = {0, 0, 1, 1, 0.0f, 1.0f};
    RenderTextureInternal(GetDebugManager()->GetCPUHandle(PICK_PIXEL_RTV), texDisplay,
                          eTexDisplay_32Render);
  }

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
  if(!list)
    return;

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
  m_pDevice->CheckHRESULT(hr);

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

bool D3D12Replay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float *minval, float *maxval)
{
  ID3D12Resource *resource = m_pDevice->GetResourceList()[texid];

  if(resource == NULL)
    return false;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = float(RDCMAX(1U, uint32_t(resourceDesc.Width >> sub.mip)));
  cdata.HistogramTextureResolution.y = float(RDCMAX(1U, uint32_t(resourceDesc.Height >> sub.mip)));
  cdata.HistogramTextureResolution.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> sub.mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramTextureResolution.z = float(resourceDesc.DepthOrArraySize);

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramSlice =
        float(RDCCLAMP(sub.slice, 0U, uint32_t((resourceDesc.DepthOrArraySize >> sub.mip) - 1)));
  else
    cdata.HistogramSlice =
        float(RDCCLAMP(sub.slice, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  cdata.HistogramMip = sub.mip;
  cdata.HistogramSample = (int)RDCCLAMP(sub.sample, 0U, resourceDesc.SampleDesc.Count - 1);
  if(sub.sample == ~0U)
    cdata.HistogramSample = -int(resourceDesc.SampleDesc.Count);
  cdata.HistogramMin = 0.0f;
  cdata.HistogramMax = 1.0f;
  cdata.HistogramChannels = 0xf;
  cdata.HistogramFlags = 0;

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(resourceDesc.Format, YUVDownsampleRate, YUVAChannels);

  cdata.HistogramYUVDownsampleRate = YUVDownsampleRate;
  cdata.HistogramYUVAChannels = YUVAChannels;

  int intIdx = 0;

  DXGI_FORMAT fmt = GetTypedFormat(resourceDesc.Format, typeCast);

  if(IsUIntFormat(fmt))
    intIdx = 1;
  else if(IsIntFormat(fmt))
    intIdx = 2;

  int blocksX = (int)ceil(cdata.HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata.HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  rdcarray<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, typeCast, resType, barriers);

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
    if(!list)
      return false;

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
  m_pDevice->CheckHRESULT(hr);

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

bool D3D12Replay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                               float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                               rdcarray<uint32_t> &histogram)
{
  if(minval >= maxval)
    return false;

  ID3D12Resource *resource = m_pDevice->GetResourceList()[texid];

  if(resource == NULL)
    return false;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = float(RDCMAX(1U, uint32_t(resourceDesc.Width >> sub.mip)));
  cdata.HistogramTextureResolution.y = float(RDCMAX(1U, uint32_t(resourceDesc.Height >> sub.mip)));
  cdata.HistogramTextureResolution.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> sub.mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramTextureResolution.z = float(resourceDesc.DepthOrArraySize);

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    cdata.HistogramSlice =
        float(RDCCLAMP(sub.slice, 0U, uint32_t((resourceDesc.DepthOrArraySize >> sub.mip) - 1)));
  else
    cdata.HistogramSlice =
        float(RDCCLAMP(sub.slice, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  cdata.HistogramMip = sub.mip;
  cdata.HistogramSample = (int)RDCCLAMP(sub.sample, 0U, resourceDesc.SampleDesc.Count - 1);
  if(sub.sample == ~0U)
    cdata.HistogramSample = -int(resourceDesc.SampleDesc.Count);
  cdata.HistogramMin = minval;
  cdata.HistogramFlags = 0;

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(resourceDesc.Format, YUVDownsampleRate, YUVAChannels);

  cdata.HistogramYUVDownsampleRate = YUVDownsampleRate;
  cdata.HistogramYUVAChannels = YUVAChannels;

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

  DXGI_FORMAT fmt = GetTypedFormat(resourceDesc.Format, typeCast);

  if(IsUIntFormat(fmt))
    intIdx = 1;
  else if(IsIntFormat(fmt))
    intIdx = 2;

  int tilesX = (int)ceil(cdata.HistogramTextureResolution.x /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int tilesY = (int)ceil(cdata.HistogramTextureResolution.y /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  rdcarray<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  GetDebugManager()->PrepareTextureSampling(resource, typeCast, resType, barriers);

  {
    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
    if(!list)
      return false;

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
  m_pDevice->CheckHRESULT(hr);

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

rdcarray<uint32_t> D3D12Replay::GetPassEvents(uint32_t eventId)
{
  rdcarray<uint32_t> passEvents;

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  if(!action)
    return passEvents;

  // for D3D12 a pass == everything writing to the same RTs in a command list.
  const ActionDescription *start = action;
  while(start)
  {
    // if we've come to the beginning of a list, break out of the loop, we've
    // found the start.
    if(start->flags & ActionFlags::BeginPass)
      break;

    // if we come to the END of a list, since we were iterating backwards that
    // means we started outside of a list, so return empty set.
    if(start->flags & ActionFlags::EndPass)
      return passEvents;

    // if we've come to the start of the log we were outside of a list
    // to start with
    if(start->previous == NULL)
      return passEvents;

    // step back
    const ActionDescription *prev = start->previous;

    // if the previous is a clear, we're done
    if(prev->flags & ActionFlags::Clear)
      break;

    // if the outputs changed, we're done
    if(start->outputs != prev->outputs || start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  // store all the action eventIDs up to the one specified at the start
  while(start)
  {
    if(start->eventId >= action->eventId)
      break;

    // include pass boundaries, these will be filtered out later
    // so we don't actually do anything (init postvs/action overlay)
    // but it's useful to have the first part of the pass as part
    // of the list
    if(start->flags & (ActionFlags::Drawcall | ActionFlags::PassBoundary))
      passEvents.push_back(start->eventId);

    start = start->next;
  }

  return passEvents;
}

bool D3D12Replay::IsTextureSupported(const TextureDescription &tex)
{
  return MakeDXGIFormat(tex.format) != DXGI_FORMAT_UNKNOWN;
}

bool D3D12Replay::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

void D3D12Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, bytebuf &retData)
{
  auto it = m_pDevice->GetResourceList().find(buff);

  if(it == m_pDevice->GetResourceList().end())
  {
    RDCERR("Getting buffer data for unknown buffer %s!",
           ToStr(m_pDevice->GetResourceManager()->GetLiveID(buff)).c_str());
    return;
  }

  WrappedID3D12Resource *buffer = it->second;

  if(buffer->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    RDCERR("Getting buffer data for non-buffer %s!",
           ToStr(m_pDevice->GetResourceManager()->GetLiveID(buff)).c_str());
    return;
  }

  RDCASSERT(buffer);

  GetDebugManager()->GetBufferData(buffer, offset, length, retData);
}

void D3D12Replay::FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                       rdcstr entryPoint, uint32_t cbufSlot,
                                       rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  if(shader == ResourceId())
    return;

  ID3D12DeviceChild *res = m_pDevice->GetResourceManager()->GetCurrentResource(shader);

  WrappedID3D12Shader *sh = (WrappedID3D12Shader *)res;

  const ShaderReflection &refl = sh->GetDetails();
  const ShaderBindpointMapping &mapping = sh->GetMapping();

  if(cbufSlot >= (uint32_t)mapping.constantBlocks.count())
  {
    RDCERR("Invalid cbuffer slot");
    return;
  }

  const ConstantBlock &c = refl.constantBlocks[cbufSlot];
  const Bindpoint &bind = mapping.constantBlocks[c.bindPoint];

  // check if the data actually comes from root constants
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12RootSignature *sig = NULL;
  const rdcarray<D3D12RenderState::SignatureElement> *sigElems = NULL;

  if(refl.stage == ShaderStage::Compute && rs.compute.rootsig != ResourceId())
  {
    sig =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rs.compute.rootsig);
    sigElems = &rs.compute.sigelems;
  }
  else if(refl.stage != ShaderStage::Compute && rs.graphics.rootsig != ResourceId())
  {
    sig = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
        rs.graphics.rootsig);
    sigElems = &rs.graphics.sigelems;
  }

  bytebuf rootData;

  ShaderStageMask reflMask = MaskForStage(refl.stage);

  for(size_t i = 0; sig && i < sig->sig.Parameters.size(); i++)
  {
    const D3D12RootSignatureParameter &p = sig->sig.Parameters[i];

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
       (ToShaderStageMask(p.ShaderVisibility) & reflMask) &&
       p.Constants.RegisterSpace == (UINT)bind.bindset &&
       p.Constants.ShaderRegister == (UINT)bind.bind)
    {
      size_t dstSize = sig->sig.Parameters[i].Constants.Num32BitValues * sizeof(uint32_t);
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

  StandardFillCBufferVariables(refl.resourceId, c.variables, outvars,
                               rootData.empty() ? data : rootData);
}

rdcarray<DebugMessage> D3D12Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

void D3D12Replay::BuildShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                              const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                              const rdcarray<rdcstr> &includeDirs, ShaderStage type, ResourceId &id,
                              rdcstr &errors)
{
  bytebuf compiledDXBC;

  const byte *dxbcBytes = source.data();
  size_t dxbcLength = source.size();

  if(sourceEncoding == ShaderEncoding::HLSL)
  {
    rdcstr profile = DXBC::GetProfile(compileFlags);

    if(profile.empty())
    {
      switch(type)
      {
        case ShaderStage::Vertex: profile = "vs_5_1"; break;
        case ShaderStage::Hull: profile = "hs_5_1"; break;
        case ShaderStage::Domain: profile = "ds_5_1"; break;
        case ShaderStage::Geometry: profile = "gs_5_1"; break;
        case ShaderStage::Pixel: profile = "ps_5_1"; break;
        case ShaderStage::Compute: profile = "cs_5_1"; break;
        default:
          RDCERR("Unexpected type in BuildShader!");
          id = ResourceId();
          return;
      }
    }

    rdcstr hlsl;
    hlsl.assign((const char *)source.data(), source.size());

    ID3DBlob *blob = NULL;
    errors = m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), entry.c_str(), compileFlags,
                                                        includeDirs, profile.c_str(), &blob);

    if(m_D3D12On7 && blob == NULL && errors.contains("unrecognized compiler target"))
    {
      profile.back() = '0';
      errors = m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), entry.c_str(), compileFlags,
                                                          includeDirs, profile.c_str(), &blob);
    }

    if(blob == NULL)
    {
      id = ResourceId();
      return;
    }

    compiledDXBC.assign((byte *)blob->GetBufferPointer(), blob->GetBufferSize());

    dxbcBytes = compiledDXBC.data();
    dxbcLength = compiledDXBC.size();

    SAFE_RELEASE(blob);
  }

  D3D12_SHADER_BYTECODE byteCode;
  byteCode.BytecodeLength = dxbcLength;
  byteCode.pShaderBytecode = dxbcBytes;

  WrappedID3D12Shader *sh = WrappedID3D12Shader::AddShader(byteCode, m_pDevice);

  sh->AddRef();

  id = sh->GetResourceID();
}

void D3D12Replay::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  ShaderCompileFlags debugCompileFlags = DXBC::EncodeFlags(
      DXBC::DecodeFlags(compileFlags) | D3DCOMPILE_DEBUG, DXBC::GetProfile(compileFlags));

  BuildShader(sourceEncoding, source, entry, debugCompileFlags, {}, type, id, errors);
}

void D3D12Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  if(WrappedID3D12Shader::IsShader(from))
  {
    WrappedID3D12Shader *fromsh =
        (WrappedID3D12Shader *)m_pDevice->GetResourceManager()->GetCurrentResource(from);
    WrappedID3D12Shader *tosh =
        (WrappedID3D12Shader *)m_pDevice->GetResourceManager()->GetCurrentResource(to);

    if(fromsh && tosh)
    {
      uint32_t slot = ~0U, space = ~0U;
      // copy the shader ext slot
      fromsh->GetShaderExtSlot(slot, space);
      tosh->SetShaderExtSlot(slot, space);
    }
  }

  // replace the shader module
  m_pDevice->GetResourceManager()->ReplaceResource(from, to);

  // now update any derived resources
  RefreshDerivedReplacements();

  ClearPostVSCache();
  ClearFeedbackCache();
}

void D3D12Replay::RemoveReplacement(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasReplacement(id))
  {
    m_pDevice->GetResourceManager()->RemoveReplacement(id);

    RefreshDerivedReplacements();

    ClearPostVSCache();
    ClearFeedbackCache();
  }
}

void D3D12Replay::RefreshDerivedReplacements()
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // we defer deletes of old replaced resources since it will invalidate elements in the vector
  // we're iterating
  rdcarray<ID3D12PipelineState *> deletequeue;

  for(WrappedID3D12PipelineState *pipe : m_pDevice->GetPipelineList())
  {
    ResourceId pipesrcid = pipe->GetResourceID();
    ResourceId origsrcid = rm->GetOriginalID(pipesrcid);

    // only look at pipelines from the capture, no replay-time programs.
    if(origsrcid == pipesrcid)
      continue;

    // if this pipeline has a replacement, remove it and delete the program generated for it
    if(rm->HasReplacement(origsrcid))
    {
      deletequeue.push_back(rm->GetLiveAs<ID3D12PipelineState>(origsrcid));

      rm->RemoveReplacement(origsrcid);
    }

    bool usesReplacedShader = false;

    if(pipe->IsGraphics())
    {
      ResourceId shaders[5];

      if(pipe->VS())
        shaders[0] = rm->GetOriginalID(pipe->VS()->GetResourceID());
      if(pipe->HS())
        shaders[1] = rm->GetOriginalID(pipe->HS()->GetResourceID());
      if(pipe->DS())
        shaders[2] = rm->GetOriginalID(pipe->DS()->GetResourceID());
      if(pipe->GS())
        shaders[3] = rm->GetOriginalID(pipe->GS()->GetResourceID());
      if(pipe->PS())
        shaders[4] = rm->GetOriginalID(pipe->PS()->GetResourceID());

      for(size_t i = 0; i < ARRAY_COUNT(shaders); i++)
      {
        usesReplacedShader = rm->HasReplacement(shaders[i]);
        if(usesReplacedShader)
          break;
      }
    }
    else
    {
      if(rm->HasReplacement(rm->GetOriginalID(pipe->CS()->GetResourceID())))
      {
        usesReplacedShader = true;
      }
    }

    // if there are replaced shaders in use, create a new pipeline with any/all replaced shaders.
    if(usesReplacedShader)
    {
      ID3D12PipelineState *newpipe = NULL;

      if(pipe->IsGraphics())
      {
        D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC desc = *pipe->graphics;

        D3D12_SHADER_BYTECODE *shaders[] = {
            &desc.VS, &desc.HS, &desc.DS, &desc.GS, &desc.PS,
        };

        for(size_t s = 0; s < ARRAY_COUNT(shaders); s++)
        {
          if(shaders[s]->BytecodeLength > 0)
          {
            WrappedID3D12Shader *stage = (WrappedID3D12Shader *)shaders[s]->pShaderBytecode;

            // remap through the original ID to pick up any replacements
            stage = rm->GetLiveAs<WrappedID3D12Shader>(rm->GetOriginalID(stage->GetResourceID()));

            *shaders[s] = stage->GetDesc();
          }
        }

        m_pDevice->CreatePipeState(desc, &newpipe);
      }
      else
      {
        D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC desc = *pipe->compute;

        WrappedID3D12Shader *stage = pipe->CS();

        // remap through the original ID to pick up any replacements
        stage = rm->GetLiveAs<WrappedID3D12Shader>(rm->GetOriginalID(stage->GetResourceID()));

        desc.CS = stage->GetDesc();

        m_pDevice->CreatePipeState(desc, &newpipe);
      }

      rm->ReplaceResource(origsrcid, GetResID(newpipe));
    }
  }

  m_pDevice->GPUSync();

  for(ID3D12PipelineState *pipe : deletequeue)
  {
    SAFE_RELEASE(pipe);
  }
}

void D3D12Replay::GetTextureData(ResourceId tex, const Subresource &sub,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  bool wasms = false;
  bool resolve = params.resolve;

  ID3D12Resource *resource = m_pDevice->GetResourceList()[tex];

  if(resource == NULL)
  {
    RDCERR("Trying to get texture data for unknown ID %s!",
           ToStr(m_pDevice->GetResourceManager()->GetLiveID(tex)).c_str());
    return;
  }

  if(resource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    RDCERR("Getting texture data for buffer %s!",
           ToStr(m_pDevice->GetResourceManager()->GetLiveID(tex)).c_str());
    return;
  }

  D3D12MarkerRegion region(m_pDevice->GetQueue(),
                           StringFormat::Fmt("GetTextureData(%u, %u, %u, remap=%d)", sub.mip,
                                             sub.slice, sub.sample, params.remap));

  Subresource s = sub;

  HRESULT hr = S_OK;

  D3D12_RESOURCE_DESC resDesc = resource->GetDesc();

  s.mip = RDCMIN(uint32_t(resDesc.MipLevels - 1), s.mip);
  s.sample = RDCMIN(resDesc.SampleDesc.Count - 1, s.sample);
  if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    s.slice = 0;
  else
    s.slice = RDCMIN(uint32_t(resDesc.DepthOrArraySize - 1), s.slice);

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

  bool isDepth = IsDepthFormat(resDesc.Format) ||
                 (resDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
  bool isStencil = IsDepthAndStencilFormat(resDesc.Format);

  UINT sampleCount = copyDesc.SampleDesc.Count;

  if(copyDesc.SampleDesc.Count > 1)
  {
    // make image n-array instead of n-samples
    copyDesc.DepthOrArraySize *= (UINT16)copyDesc.SampleDesc.Count;
    copyDesc.SampleDesc.Count = 1;
    copyDesc.SampleDesc.Quality = 0;
    copyDesc.MipLevels = 1;

    wasms = true;
  }

  if(wasms && (isDepth || isStencil))
    resolve = false;

  uint32_t slice3DCopy = 0;

  // arrayIdx isn't used for anything except copying the slice out at the end, so save the index we
  // want to copy and then set arrayIdx to 0 to simplify subresource calculations
  if(copyDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    slice3DCopy = s.slice;
    s.slice = 0;
  }

  ID3D12Resource *srcTexture = resource;
  ID3D12Resource *tmpTexture = NULL;

  ID3D12GraphicsCommandList *list = NULL;

  if(params.remap != RemapTexture::NoRemap)
  {
    if(params.remap == RemapTexture::RGBA8)
    {
      copyDesc.Format = GetTypedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS, BaseRemapType(params.typeCast));
      if(IsSRGBFormat(copyDesc.Format) && params.typeCast == CompType::Typeless)
        copyDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
    else if(params.remap == RemapTexture::RGBA16)
    {
      copyDesc.Format =
          GetTypedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS, BaseRemapType(params.typeCast));
    }
    else if(params.remap == RemapTexture::RGBA32)
    {
      copyDesc.Format =
          GetTypedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS, BaseRemapType(params.typeCast));
    }

    // force to 1 mip
    copyDesc.MipLevels = 1;
    copyDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // Keep 3D texture depth, set array size to 1.
    if(copyDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
      copyDesc.DepthOrArraySize = 1;

    SetOutputDimensions(uint32_t(copyDesc.Width), copyDesc.Height);
    m_OutputViewport = {0, 0, (float)copyDesc.Width, (float)copyDesc.Height, 0.0f, 1.0f};

    copyDesc.Width = RDCMAX(1ULL, copyDesc.Width >> s.mip);
    copyDesc.Height = RDCMAX(1U, copyDesc.Height >> s.mip);

    ID3D12Resource *remapTexture;
    hr = m_pDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                            __uuidof(ID3D12Resource), (void **)&remapTexture);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create remap texture: %s", ToStr(hr).c_str());
      return;
    }

    TexDisplayFlags flags =
        IsSRGBFormat(copyDesc.Format) ? eTexDisplay_None : eTexDisplay_LinearRender;

    if(GetTypelessFormat(copyDesc.Format) == DXGI_FORMAT_R16G16B16A16_TYPELESS)
      flags = eTexDisplay_16Render;
    else if(GetTypelessFormat(copyDesc.Format) == DXGI_FORMAT_R32G32B32A32_TYPELESS)
      flags = eTexDisplay_32Render;

    if(IsUIntFormat(copyDesc.Format))
      flags = TexDisplayFlags(flags | eTexDisplay_RemapUInt);
    else if(IsIntFormat(copyDesc.Format))
      flags = TexDisplayFlags(flags | eTexDisplay_RemapSInt);
    else
      flags = TexDisplayFlags(flags | eTexDisplay_RemapFloat);

    uint32_t loopCount = 1;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = copyDesc.Format;
    if(copyDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    {
      rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
    }
    else if(copyDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
      rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    }
    else if(copyDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
      rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
      rtvDesc.Texture3D.WSize = 1;
      loopCount = RDCMAX(0U, uint32_t(copyDesc.DepthOrArraySize >> s.mip));
    }

    // we only loop for 3D slices, other types we just do one remap for the desired mip/slice
    for(uint32_t loop = 0; loop < loopCount; loop++)
    {
      if(copyDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        rtvDesc.Texture3D.FirstWSlice = loop;

      m_pDevice->CreateRenderTargetView(remapTexture, &rtvDesc,
                                        GetDebugManager()->GetCPUHandle(GET_TEX_RTV));

      TextureDisplay texDisplay;

      texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
      texDisplay.hdrMultiplier = -1.0f;
      texDisplay.linearDisplayAsGamma = false;
      texDisplay.overlay = DebugOverlay::NoOverlay;
      texDisplay.flipY = false;
      texDisplay.subresource.mip = s.mip;
      texDisplay.subresource.slice = s.slice;
      if(copyDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        texDisplay.subresource.slice = loop;
      texDisplay.subresource.sample = resolve ? ~0U : s.sample;
      texDisplay.customShaderId = ResourceId();
      texDisplay.rangeMin = params.blackPoint;
      texDisplay.rangeMax = params.whitePoint;
      texDisplay.resourceId = tex;
      texDisplay.typeCast = params.typeCast;
      texDisplay.rawOutput = false;
      texDisplay.xOffset = 0;
      texDisplay.yOffset = 0;

      // we scale our texture rendering by output dimension. To counteract that, add a manual
      // scale here
      texDisplay.scale = 1.0f / float(1 << s.mip);

      RenderTextureInternal(GetDebugManager()->GetCPUHandle(GET_TEX_RTV), texDisplay, flags);
    }

    tmpTexture = srcTexture = remapTexture;

    list = m_pDevice->GetNewList();
    if(!list)
      return;

    D3D12_RESOURCE_BARRIER b = {};
    b.Transition.pResource = remapTexture;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &b);

    // these have already been selected, don't need to fetch that subresource
    // when copying back to readback buffer
    s.slice = 0;
    s.mip = 0;

    // no longer depth, if it was
    isDepth = false;
    isStencil = false;
  }
  else if(wasms && resolve)
  {
    // force to 1 array slice, 1 mip
    copyDesc.DepthOrArraySize = 1;
    copyDesc.MipLevels = 1;

    copyDesc.Width = RDCMAX(1ULL, copyDesc.Width >> s.mip);
    copyDesc.Height = RDCMAX(1U, copyDesc.Height >> s.mip);

    ID3D12Resource *resolveTexture;
    hr = m_pDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc,
                                            D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL,
                                            __uuidof(ID3D12Resource), (void **)&resolveTexture);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create resolve texture: %s", ToStr(hr).c_str());
      return;
    }

    RDCASSERT(!isDepth && !isStencil);

    list = m_pDevice->GetNewList();
    if(!list)
      return;

    // put source texture into resolve source state
    const rdcarray<D3D12_RESOURCE_STATES> &states = m_pDevice->GetSubresourceStates(tex);

    rdcarray<D3D12_RESOURCE_BARRIER> barriers;
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
                             s.slice * resDesc.DepthOrArraySize + s.mip, resDesc.Format);

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
    s.slice = 0;
    s.mip = 0;
  }
  else if(wasms)
  {
    // copy/expand multisampled live texture to array readback texture
    if(isDepth)
      copyDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    else
      copyDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    copyDesc.Format = GetTypelessFormat(copyDesc.Format);

    ID3D12Resource *arrayTexture;
    hr = m_pDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc,
        isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&arrayTexture);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create array texture: %s", ToStr(hr).c_str());
      return;
    }

    list = m_pDevice->GetNewList();
    if(!list)
      return;

    // put source texture into shader read state
    const rdcarray<D3D12_RESOURCE_STATES> &states = m_pDevice->GetSubresourceStates(tex);

    rdcarray<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(states.size());
    for(size_t i = 0; i < states.size(); i++)
    {
      D3D12_RESOURCE_BARRIER b;

      // skip unneeded barriers
      if(states[i] & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        continue;

      b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      b.Transition.pResource = resource;
      b.Transition.Subresource = (UINT)i;
      b.Transition.StateBefore = states[i];
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

      barriers.push_back(b);
    }

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();
    list = NULL;

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();

    // expand multisamples out to array
    GetDebugManager()->CopyTex2DMSToArray(NULL, Unwrap(arrayTexture), Unwrap(srcTexture));

    tmpTexture = srcTexture = arrayTexture;

    list = m_pDevice->GetNewList();
    if(!list)
      return;

    // real resource back to normal
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    D3D12_RESOURCE_BARRIER b = {};
    b.Transition.pResource = arrayTexture;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore =
        isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &b);

    s.slice = s.slice * sampleCount + s.sample;
    s.sample = 0;
  }

  if(list == NULL)
    list = m_pDevice->GetNewList();
  if(!list)
    return;

  rdcarray<D3D12_RESOURCE_BARRIER> barriers;

  // if we have no tmpImage, we're copying directly from the real image
  if(tmpTexture == NULL)
  {
    const rdcarray<D3D12_RESOURCE_STATES> &states = m_pDevice->GetSubresourceStates(tex);
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

  for(UINT p = 0; p < planes; p++)
  {
    readbackDesc.Width = AlignUp(readbackDesc.Width, 512ULL);

    UINT64 subSize = 0;
    m_pDevice->GetCopyableFootprints(&copyDesc, s.mip + s.slice * arrayStride + p * planeStride, 1,
                                     readbackDesc.Width, layouts + p, rowcounts + p, NULL, &subSize);
    readbackDesc.Width += subSize;
  }

  UINT rowcount = rowcounts[0];

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
  if(FAILED(hr))
  {
    RDCERR("Couldn't create readback buffer: %s", ToStr(hr).c_str());
    return;
  }

  for(UINT p = 0; p < planes; p++)
  {
    D3D12_TEXTURE_COPY_LOCATION dst, src;

    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = srcTexture;
    src.SubresourceIndex = s.mip + s.slice * arrayStride + p * planeStride;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.pResource = readbackBuf;
    dst.PlacedFootprint = layouts[p];

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
  m_pDevice->CheckHRESULT(hr);
  if(FAILED(hr))
  {
    RDCERR("Couldn't map readback buffer: %s", ToStr(hr).c_str());
    readbackBuf->Unmap(0, NULL);
    return;
  }
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
      for(UINT z = 0; z < layouts[0].Footprint.Depth; z++)
      {
        for(UINT y = 0; y < layouts[0].Footprint.Height; y++)
        {
          UINT row = y + z * layouts[0].Footprint.Height;

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
      for(UINT z = 0; z < layouts[0].Footprint.Depth; z++)
      {
        for(UINT y = 0; y < rowcount; y++)
        {
          UINT row = y + z * rowcount;

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
    for(UINT z = 0; z < layouts[0].Footprint.Depth; z++)
    {
      for(UINT y = 0; y < rowcount; y++)
      {
        UINT row = y + z * rowcount;

        byte *src = pData + layouts[0].Footprint.RowPitch * row;
        byte *dst = data.data() + dstRowPitch * row;

        memcpy(dst, src, dstRowPitch);
      }
    }

    // for 3D textures if we wanted a particular slice (slice3DCopy > 0) copy it into the beginning.
    if(layouts[0].Footprint.Depth > 1 && slice3DCopy > 0 &&
       (int)slice3DCopy < layouts[0].Footprint.Depth)
    {
      for(UINT y = 0; y < rowcount; y++)
      {
        UINT srcrow = y + slice3DCopy * rowcount;
        UINT dstrow = y;

        byte *src = pData + layouts[0].Footprint.RowPitch * srcrow;
        byte *dst = data.data() + dstRowPitch * dstrow;

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

void D3D12Replay::SetCustomShaderIncludes(const rdcarray<rdcstr> &directories)
{
  m_CustomShaderIncludes = directories;
}

void D3D12Replay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  BuildShader(sourceEncoding, source, entry, compileFlags, m_CustomShaderIncludes, type, id, errors);
}

ResourceId D3D12Replay::ApplyCustomShader(TextureDisplay &display)
{
  ID3D12Resource *resource = m_pDevice->GetResourceList()[display.resourceId];

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
    if(FAILED(hr))
    {
      RDCERR("Couldn't create custom shader texture: %s", ToStr(hr).c_str());
      return ResourceId();
    }

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
  rtvDesc.Texture2D.MipSlice = display.subresource.mip;

  m_pDevice->CreateRenderTargetView(m_CustomShaderTex, &rtvDesc,
                                    GetDebugManager()->GetCPUHandle(CUSTOM_SHADER_RTV));

  ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
  if(!list)
    return ResourceId();

  float clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
  list->ClearRenderTargetView(GetDebugManager()->GetCPUHandle(CUSTOM_SHADER_RTV), clr, 0, NULL);

  list->Close();

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = display.customShaderId;
  disp.resourceId = display.resourceId;
  disp.typeCast = display.typeCast;
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.subresource = display.subresource;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = 0.0f;
  disp.rangeMax = 1.0f;
  disp.rawOutput = false;
  disp.scale = 1.0f;

  SetOutputDimensions(RDCMAX(1U, (UINT)resDesc.Width), RDCMAX(1U, resDesc.Height));

  m_OutputViewport = {
      0,
      0,
      (float)RDCMAX(1ULL, resDesc.Width >> display.subresource.mip),
      (float)RDCMAX(1U, resDesc.Height >> display.subresource.mip),
      0.0f,
      1.0f,
  };
  RenderTextureInternal(GetDebugManager()->GetCPUHandle(CUSTOM_SHADER_RTV), disp,
                        eTexDisplay_BlendAlpha);

  return m_CustomShaderResourceId;
}

#pragma region not yet implemented

rdcarray<PixelModification> D3D12Replay::PixelHistory(rdcarray<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      const Subresource &sub, CompType typeCast)
{
  return {};
}

ResourceId D3D12Replay::CreateProxyTexture(const TextureDescription &templateTex)
{
  return ResourceId();
}

void D3D12Replay::SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data,
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

ReplayStatus D3D12_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D12 replay device");

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D12DeviceIfAlloc);

  // this needs to be static, because we don't unload d3d12.dll so the plain LoadLibraryA will
  // succeed on subsequent capture loads.
  static bool d3d12on7 = false;

  D3D12Lib = LoadLibraryA("d3d12.dll");
  if(D3D12Lib == NULL)
  {
    // if it fails try to find D3D12On7 DLLs
    d3d12on7 = true;

    // if it fails, try in the plugin directory
    D3D12Lib = (HMODULE)Process::LoadModule(LocatePluginFile("d3d12", "d3d12.dll"));

    // if that succeeded, also load dxilconv7.dll from there
    if(D3D12Lib)
    {
      HMODULE dxilconv = (HMODULE)Process::LoadModule(LocatePluginFile("d3d12", "dxilconv7.dll"));

      if(!dxilconv)
      {
        RDCERR("Found d3d12.dll in plugin path, but couldn't load dxilconv7.dll");
        return ReplayStatus::APIInitFailed;
      }
    }
    else
    {
      // if it failed, try one more time in MS's subfolder convention
      D3D12Lib = LoadLibraryA("12on7/d3d12.dll");

      if(D3D12Lib)
      {
        RDCWARN(
            "Loaded d3d12.dll from 12on7 subfolder."
            "Please use RenderDoc's plugins/d3d12/ subfolder instead");
      }
      else
      {
        if(rdc)
          RDCERR("Failed to load d3d12.dll");
        else
          RDCWARN("Failed to load d3d12.dll");
        return ReplayStatus::APIInitFailed;
      }
    }
  }

  PFN_D3D12_CREATE_DEVICE createDevicePtr =
      (PFN_D3D12_CREATE_DEVICE)GetProcAddress(D3D12Lib, "D3D12CreateDevice");

  RealD3D12CreateFunction createDevice = createDevicePtr;

  HMODULE dxgilib = LoadLibraryA("dxgi.dll");
  if(dxgilib == NULL)
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
  bytebuf D3D12Core, D3D12SDKLayers;

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

    ser.SetVersion(ver);

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

    if(initParams.AdapterDesc.DeviceId != 0)
      RDCLOG("Capture was created on %s / %ls",
             ToStr(GPUVendorFromPCIVendor(initParams.AdapterDesc.VendorId)).c_str(),
             initParams.AdapterDesc.Description);

    sectionIdx = rdc->SectionIndex(SectionType::D3D12Core);

    if(sectionIdx >= 0)
    {
      SectionProperties props = rdc->GetSectionProperties(sectionIdx);
      reader = rdc->ReadSection(sectionIdx);

      D3D12Core.resize((size_t)props.uncompressedSize);
      reader->Read(D3D12Core.data(), props.uncompressedSize);

      RDCLOG("Got D3D12Core.dll of size %llu (decompressed from %llu)", props.uncompressedSize,
             props.compressedSize);

      RDCASSERT(reader->AtEnd());
      delete reader;
    }

    sectionIdx = rdc->SectionIndex(SectionType::D3D12SDKLayers);

    if(sectionIdx >= 0)
    {
      SectionProperties props = rdc->GetSectionProperties(sectionIdx);
      reader = rdc->ReadSection(sectionIdx);

      D3D12SDKLayers.resize((size_t)props.uncompressedSize);
      reader->Read(D3D12SDKLayers.data(), props.uncompressedSize);

      RDCLOG("Got D3D12SDKLayers.dll of size %llu (decompressed from %llu)", props.uncompressedSize,
             props.compressedSize);

      RDCASSERT(reader->AtEnd());
      delete reader;
    }
  }

  if(initParams.MinimumFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    initParams.MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  D3D12_PrepareReplaySDKVersion(initParams.SDKVersion, D3D12Core, D3D12SDKLayers, D3D12Lib);

  const bool isProxy = (rdc == NULL);

  AMDRGPControl *rgp = NULL;

  if(!isProxy)
  {
    rgp = new AMDRGPControl();

    if(!rgp->Initialised())
      SAFE_DELETE(rgp);
  }

  typedef HRESULT(WINAPI * PFN_CREATE_DXGI_FACTORY)(REFIID, void **);

  PFN_CREATE_DXGI_FACTORY createFunc =
      (PFN_CREATE_DXGI_FACTORY)GetProcAddress(GetModuleHandleA("dxgi.dll"), "CreateDXGIFactory1");

  IDXGIFactory1 *factory = NULL;
  HRESULT hr = createFunc(__uuidof(IDXGIFactory1), (void **)&factory);

  if(FAILED(hr))
    RDCERR("Couldn't create DXGI factory! HRESULT: %s", ToStr(hr).c_str());

  IDXGIAdapter *adapter = NULL;

  if(!isProxy)
    ChooseBestMatchingAdapter(GraphicsAPI::D3D12, factory, initParams.AdapterDesc, opts, NULL,
                              &adapter);

  INVAPID3DDevice *nvapiDev = NULL;
  IAGSD3DDevice *agsDev = NULL;

  if(initParams.VendorExtensions == GPUVendor::nVidia)
  {
    nvapiDev = InitialiseNVAPIReplay();

    if(!nvapiDev)
    {
      RDCERR("Capture requires nvapi to replay, but it's not available or can't be initialised");
      return ReplayStatus::APIHardwareUnsupported;
    }
  }
  else if(initParams.VendorExtensions == GPUVendor::AMD)
  {
    agsDev = InitialiseAGSReplay(initParams.VendorUAVSpace, initParams.VendorUAV);

    if(!agsDev)
    {
      RDCERR("Capture requires ags to replay, but it's not available or can't be initialised");
      return ReplayStatus::APIHardwareUnsupported;
    }

    createDevice = [agsDev](IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                            void **ppDevice) {
      return agsDev->CreateD3D12(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    };
  }
  else if(initParams.VendorExtensions != GPUVendor::Unknown)
  {
    RDCERR(
        "Capture requires vendor extensions by %s to replay, but no support for that is "
        "available.",
        ToStr(initParams.VendorExtensions).c_str());
    return ReplayStatus::APIInitFailed;
  }

  bool debugLayerEnabled = false;

  if(!isProxy)
    RDCLOG("Creating D3D12 replay device, minimum feature level %s",
           ToStr(initParams.MinimumFeatureLevel).c_str());

  bool shouldEnableDebugLayer = opts.apiValidation;

  if(shouldEnableDebugLayer && !D3D12Core.empty() && D3D12SDKLayers.empty())
  {
    RDCWARN(
        "Not enabling D3D debug layers because we captured a D3D12Core.dll but not a matching "
        "D3D12SDKLayers.dll, so this may crash");
    shouldEnableDebugLayer = false;
  }

  if(shouldEnableDebugLayer)
  {
    debugLayerEnabled = EnableD3D12DebugLayer();

    if(!debugLayerEnabled && !isProxy)
    {
      RDCLOG(
          "Enabling the D3D debug layers failed, "
          "ensure you have the windows SDK or windows feature needed.");
    }
  }

  ID3D12Device *dev = NULL;
  hr = createDevice(adapter, initParams.MinimumFeatureLevel, __uuidof(ID3D12Device), (void **)&dev);

  if((FAILED(hr) || !dev) && adapter)
  {
    SAFE_RELEASE(dev);

    RDCWARN("Couldn't replay on selected adapter, falling back to default adapter");

    SAFE_RELEASE(adapter);
    hr = createDevice(adapter, initParams.MinimumFeatureLevel, __uuidof(ID3D12Device), (void **)&dev);
  }

  SAFE_RELEASE(adapter);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create a d3d12 device.");

    SAFE_DELETE(rgp);

    return ReplayStatus::APIHardwareUnsupported;
  }

  if(nvapiDev)
  {
    BOOL ok = nvapiDev->SetReal(dev);
    if(!ok)
    {
      RDCERR(
          "This capture needs nvapi extensions to replay, but device selected for replay can't "
          "support nvapi extensions");
      SAFE_RELEASE(dev);
      SAFE_RELEASE(nvapiDev);
      SAFE_RELEASE(factory);
      SAFE_DELETE(rgp);
      return ReplayStatus::APIHardwareUnsupported;
    }
  }

  if(agsDev)
  {
    if(!agsDev->ExtensionsSupported())
    {
      RDCERR(
          "This capture needs AGS extensions to replay, but device selected for replay can't "
          "support AGS extensions");
      SAFE_RELEASE(dev);
      SAFE_RELEASE(nvapiDev);
      SAFE_RELEASE(factory);
      SAFE_DELETE(rgp);
      return ReplayStatus::APIHardwareUnsupported;
    }
  }

  WrappedID3D12Device *wrappedDev = new WrappedID3D12Device(dev, initParams, debugLayerEnabled);
  wrappedDev->SetInitParams(initParams, ver, opts, nvapiDev, agsDev);

  if(!isProxy)
    RDCLOG("Created device.");
  D3D12Replay *replay = wrappedDev->GetReplay();

  replay->Set12On7(d3d12on7);
  replay->SetProxy(isProxy);
  replay->SetRGP(rgp);

  replay->Initialise(factory);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}

static DriverRegistration D3D12DriverRegistration(RDCDriver::D3D12, &D3D12_CreateReplayDevice);

void D3D12_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedID3D12Device device(NULL, D3D12InitParams(), false);

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = device.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    device.GetStructuredFile()->Swap(output);
}

static StructuredProcessRegistration D3D12ProcessRegistration(RDCDriver::D3D12,
                                                              &D3D12_ProcessStructured);
