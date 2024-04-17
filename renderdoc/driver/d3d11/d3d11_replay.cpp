/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "d3d11_replay.h"
#include "core/settings.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/intel/intel_counters.h"
#include "driver/ihv/nv/nv_counters.h"
#include "driver/ihv/nv/nv_d3d11_counters.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_hooks.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"
#include "d3d11_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

RDOC_CONFIG(bool, D3D11_HardwareCounters, true,
            "Enable support for IHV-specific hardware counters on D3D11.");

static const char *DXBCDisassemblyTarget = "DXBC";

D3D11Replay::D3D11Replay(WrappedID3D11Device *d)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(D3D11Replay));

  m_pDevice = d;
  m_pImmediateContext = d->GetImmediateContext();

  m_Proxy = false;
  m_WARP = false;

  m_HighlightCache.driver = this;

  RDCEraseEl(m_DriverInfo);
}

D3D11Replay::~D3D11Replay()
{
  RenderDoc::Inst().UnregisterMemoryRegion(this);
}

void D3D11Replay::Shutdown()
{
  for(size_t i = 0; i < m_ProxyResources.size(); i++)
    m_ProxyResources[i]->Release();
  m_ProxyResources.clear();

  m_RealState.state.Clear();

  // explicitly delete the device, as all the replay resources created will be keeping refs on it
  delete m_pDevice;
}

RDResult D3D11Replay::FatalErrorCheck()
{
  return m_pDevice->FatalErrorCheck();
}

IReplayDriver *D3D11Replay::MakeDummyDriver()
{
  // gather up the shaders we've allocated to pass to the dummy driver
  rdcarray<ShaderReflection *> shaders;
  WrappedID3D11Shader<ID3D11ComputeShader>::GetReflections(shaders);

  IReplayDriver *dummy = new DummyDriver(this, shaders, m_pDevice->DetachStructuredFile());

  return dummy;
}

void D3D11Replay::CreateResources(IDXGIFactory *factory)
{
  bool wrapped =
      RefCountDXGIObject::HandleWrap("D3D11Replay", __uuidof(IDXGIFactory), (void **)&factory);
  RDCASSERT(wrapped);
  m_pFactory = factory;

  HRESULT hr = S_OK;

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.0f);

  IDXGIDevice *pDXGIDevice;
  hr = m_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);

  if(FAILED(hr))
  {
    RDCERR("Couldn't get DXGI device from D3D device");
  }
  else
  {
    IDXGIAdapter *pDXGIAdapter;
    hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

    if(FAILED(hr))
    {
      RDCERR("Couldn't get DXGI adapter from DXGI device");
      SAFE_RELEASE(pDXGIDevice);
    }
    else
    {
      DXGI_ADAPTER_DESC desc = {};
      pDXGIAdapter->GetDesc(&desc);

      RDCEraseEl(m_DriverInfo);

      m_DriverInfo.vendor = GPUVendorFromPCIVendor(desc.VendorId);

      rdcstr descString = GetDriverVersion(desc);
      descString.resize(RDCMIN(descString.size(), ARRAY_COUNT(m_DriverInfo.version) - 1));
      memcpy(m_DriverInfo.version, descString.c_str(), descString.size());

      RDCLOG("Running replay on %s / %s", ToStr(m_DriverInfo.vendor).c_str(), m_DriverInfo.version);

      if(m_WARP)
        m_DriverInfo.vendor = GPUVendor::Software;

      SAFE_RELEASE(pDXGIDevice);
      SAFE_RELEASE(pDXGIAdapter);

      if(FAILED(hr))
      {
        RDCERR("Couldn't get DXGI factory from DXGI adapter");
      }
    }
  }

  m_pDevice->GetShaderCache()->SetCaching(true);

  InitStreamOut();

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.1f);

  m_General.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.2f);

  m_TexRender.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.3f);

  m_Overlay.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.4f);

  m_MeshRender.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.5f);

  m_VertexPick.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.6f);

  m_PixelPick.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.65f);

  m_ShaderDebug.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.7f);

  m_Histogram.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.8f);

  m_PixelHistory.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.9f);

  m_pDevice->GetShaderCache()->SetCaching(false);

  if(!m_Proxy && D3D11_HardwareCounters())
  {
    AMDCounters *countersAMD = NULL;
    IntelCounters *countersIntel = NULL;

    ID3D11Device *d3dDevice = m_pDevice->GetReal();

    if(m_DriverInfo.vendor == GPUVendor::AMD)
    {
      RDCLOG("AMD GPU detected - trying to initialise AMD counters");
      countersAMD = new AMDCounters();
    }
    else if(m_DriverInfo.vendor == GPUVendor::nVidia)
    {
      RDCLOG("nVidia GPU detected - trying to initialise nVidia counters");
      m_pNVCounters = NULL;
      m_pNVPerfCounters = NULL;
      // Legacy NVPMAPI counters
      NVCounters *countersNVPMAPI = new NVCounters();
      if(countersNVPMAPI && countersNVPMAPI->Init(d3dDevice))
      {
        m_pNVCounters = countersNVPMAPI;
      }
      else
      {
        delete countersNVPMAPI;

        // Nsight Perf SDK counters
        NVD3D11Counters *countersNvPerf = new NVD3D11Counters();
        if(countersNvPerf && countersNvPerf->Init(m_pDevice))
        {
          m_pNVPerfCounters = countersNvPerf;
        }
        else
        {
          delete countersNvPerf;
        }
      }
    }
    else if(m_DriverInfo.vendor == GPUVendor::Intel)
    {
      RDCLOG("Intel GPU detected - trying to initialize Intel counters");
      countersIntel = new IntelCounters();
    }
    else
    {
      RDCLOG("%s GPU detected - no counters available", ToStr(m_DriverInfo.vendor).c_str());
    }

    if(countersAMD && countersAMD->Init(AMDCounters::ApiType::Dx11, (void *)d3dDevice))
    {
      m_pAMDCounters = countersAMD;
    }
    else
    {
      delete countersAMD;
      m_pAMDCounters = NULL;
    }

    if(countersIntel && countersIntel->Init(d3dDevice))
    {
      m_pIntelCounters = countersIntel;
    }
    else
    {
      delete countersIntel;
      m_pIntelCounters = NULL;
    }
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 1.0f);
}

void D3D11Replay::DestroyResources()
{
  SAFE_RELEASE(m_CustomShaderTex);

  m_General.Release();
  m_TexRender.Release();
  m_Overlay.Release();
  m_MeshRender.Release();
  m_VertexPick.Release();
  m_PixelPick.Release();
  m_ShaderDebug.Release();
  m_Histogram.Release();
  m_PixelHistory.Release();

  SAFE_DELETE(m_pAMDCounters);
  SAFE_DELETE(m_pNVCounters);
  SAFE_DELETE(m_pNVPerfCounters);
  SAFE_DELETE(m_pIntelCounters);

  ShutdownStreamOut();
  ClearPostVSCache();

  SAFE_RELEASE(m_pFactory);
}

rdcarray<ShaderEntryPoint> D3D11Replay::GetShaderEntryPoints(ResourceId shader)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return {};

  ShaderReflection &ret = it->second->GetDetails();

  return {{"main", ret.stage}};
}

ShaderReflection *D3D11Replay::GetShader(ResourceId pipeline, ResourceId shader,
                                         ShaderEntryPoint entry)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return NULL;

  ShaderReflection &ret = it->second->GetDetails();

  return &ret;
}

rdcarray<rdcstr> D3D11Replay::GetDisassemblyTargets(bool withPipeline)
{
  return {DXBCDisassemblyTarget};
}

rdcstr D3D11Replay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const rdcstr &target)
{
  auto it =
      WrappedShader::m_ShaderList.find(m_pDevice->GetResourceManager()->GetLiveID(refl->resourceId));

  if(it == WrappedShader::m_ShaderList.end())
    return "; Invalid Shader Specified";

  DXBC::DXBCContainer *dxbc = it->second->GetDXBC();

  if(target == DXBCDisassemblyTarget || target.empty())
    return dxbc->GetDisassembly(false);

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
}

void D3D11Replay::FreeTargetResource(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D11DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

void D3D11Replay::FreeCustomShader(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D11DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

rdcarray<EventUsage> D3D11Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetImmediateContext()->GetUsage(id);
}

rdcarray<DebugMessage> D3D11Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

rdcarray<GPUDevice> D3D11Replay::GetAvailableGPUs()
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
      dev.apis = {GraphicsAPI::D3D11};

      // don't add duplicate devices even if they get enumerated. Don't add WARP, we'll do that
      // manually since it's inconsistently enumerated
      if(ret.indexOf(dev) == -1 && dev.vendor != GPUVendor::Software)
        ret.push_back(dev);
    }

    SAFE_RELEASE(adapter);
  }

  {
    GPUDevice dev;
    dev.vendor = GPUVendor::Software;
    dev.deviceID = 0;
    dev.driver = "";    // D3D doesn't have multiple drivers per API
    dev.name = "WARP Rasterizer";
    dev.apis = {GraphicsAPI::D3D11};
    ret.push_back(dev);
  }

  return ret;
}

APIProperties D3D11Replay::GetAPIProperties()
{
  APIProperties ret = m_pDevice->APIProps;

  ret.pipelineType = GraphicsAPI::D3D11;
  ret.localRenderer = GraphicsAPI::D3D11;
  ret.vendor = m_DriverInfo.vendor;
  ret.degraded = m_WARP;
  ret.shaderDebugging = true;
  ret.pixelHistory = true;

  return ret;
}

ResourceDescription &D3D11Replay::GetResourceDesc(ResourceId id)
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

rdcarray<ResourceDescription> D3D11Replay::GetResources()
{
  return m_Resources;
}

BufferDescription D3D11Replay::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};
  ret.resourceId = ResourceId();

  auto it = WrappedID3D11Buffer::m_BufferList.find(id);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
    return ret;

  WrappedID3D11Buffer *d3dbuf = it->second.m_Buffer;

  rdcstr str = GetDebugName(d3dbuf);

  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it->first);

  D3D11_BUFFER_DESC desc;
  it->second.m_Buffer->GetDesc(&desc);

  ret.length = desc.ByteWidth;

  ret.creationFlags = BufferCategory::NoFlags;
  if(desc.BindFlags & D3D11_BIND_VERTEX_BUFFER)
    ret.creationFlags |= BufferCategory::Vertex;
  if(desc.BindFlags & D3D11_BIND_INDEX_BUFFER)
    ret.creationFlags |= BufferCategory::Index;
  if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
    ret.creationFlags |= BufferCategory::ReadWrite;
  if(desc.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)
    ret.creationFlags |= BufferCategory::Indirect;

  return ret;
}

TextureDescription D3D11Replay::GetTexture(ResourceId id)
{
  TextureDescription tex = {};
  tex.resourceId = ResourceId();

  auto it1D = WrappedID3D11Texture1D::m_TextureList.find(id);
  if(it1D != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *d3dtex = (WrappedID3D11Texture1D *)it1D->second.m_Texture;

    rdcstr str = GetDebugName(d3dtex);

    D3D11_TEXTURE1D_DESC desc;
    d3dtex->GetDesc(&desc);

    tex.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it1D->first);
    tex.dimension = 1;
    tex.width = desc.Width;
    tex.height = 1;
    tex.depth = 1;
    tex.cubemap = false;
    tex.format = MakeResourceFormat(desc.Format);

    tex.creationFlags = TextureCategory::NoFlags;
    if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.creationFlags |= TextureCategory::ShaderRead;
    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.creationFlags |= TextureCategory::ColorTarget;
    if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.creationFlags |= TextureCategory::DepthTarget;
    if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      tex.creationFlags |= TextureCategory::ShaderReadWrite;

    tex.mips = desc.MipLevels;

    if(desc.MipLevels == 0)
      tex.mips = CalcNumMips(desc.Width, 1, 1);

    tex.arraysize = desc.ArraySize;

    tex.type = tex.arraysize > 1 ? TextureType::Texture1DArray : TextureType::Texture1D;

    tex.msQual = 0;
    tex.msSamp = 1;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.mips * tex.arraysize; s++)
      tex.byteSize += GetByteSize(d3dtex, s);

    return tex;
  }

  auto it2D = WrappedID3D11Texture2D1::m_TextureList.find(id);
  if(it2D != WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *d3dtex = (WrappedID3D11Texture2D1 *)it2D->second.m_Texture;

    rdcstr str = GetDebugName(d3dtex);

    D3D11_TEXTURE2D_DESC desc;
    d3dtex->GetDesc(&desc);

    if(d3dtex->m_RealDescriptor)
      desc.Format = d3dtex->m_RealDescriptor->Format;

    tex.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it2D->first);
    tex.dimension = 2;
    tex.width = desc.Width;
    tex.height = desc.Height;
    tex.depth = 1;
    tex.format = MakeResourceFormat(desc.Format);

    tex.creationFlags = TextureCategory::NoFlags;
    if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.creationFlags |= TextureCategory::ShaderRead;
    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.creationFlags |= TextureCategory::ColorTarget;
    if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.creationFlags |= TextureCategory::DepthTarget;
    if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      tex.creationFlags |= TextureCategory::ShaderReadWrite;
    if(d3dtex->m_RealDescriptor)
      tex.creationFlags |= TextureCategory::SwapBuffer;

    tex.cubemap = false;
    if(desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      tex.cubemap = true;

    tex.mips = desc.MipLevels;

    if(desc.MipLevels == 0)
      tex.mips = CalcNumMips(desc.Width, desc.Height, 1);

    tex.arraysize = desc.ArraySize;

    tex.msQual = desc.SampleDesc.Quality;
    tex.msSamp = RDCMAX(1U, desc.SampleDesc.Count);

    tex.type = tex.arraysize > 1 ? TextureType::Texture2DArray : TextureType::Texture2D;
    if(tex.cubemap)
      tex.type = tex.arraysize > 1 ? TextureType::TextureCubeArray : TextureType::TextureCube;
    if(tex.msSamp > 1)
      tex.type = tex.arraysize > 1 ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.arraysize * tex.mips; s++)
      tex.byteSize += GetByteSize(d3dtex, s);
    tex.byteSize *= tex.msSamp;

    return tex;
  }

  auto it3D = WrappedID3D11Texture3D1::m_TextureList.find(id);
  if(it3D != WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *d3dtex = (WrappedID3D11Texture3D1 *)it3D->second.m_Texture;

    rdcstr str = GetDebugName(d3dtex);

    D3D11_TEXTURE3D_DESC desc;
    d3dtex->GetDesc(&desc);

    tex.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it3D->first);
    tex.dimension = 3;
    tex.width = desc.Width;
    tex.height = desc.Height;
    tex.depth = desc.Depth;
    tex.cubemap = false;
    tex.format = MakeResourceFormat(desc.Format);

    tex.type = TextureType::Texture3D;

    tex.creationFlags = TextureCategory::NoFlags;
    if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.creationFlags |= TextureCategory::ShaderRead;
    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.creationFlags |= TextureCategory::ColorTarget;
    if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.creationFlags |= TextureCategory::DepthTarget;
    if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      tex.creationFlags |= TextureCategory::ShaderReadWrite;

    tex.mips = desc.MipLevels;

    if(desc.MipLevels == 0)
      tex.mips = CalcNumMips(desc.Width, desc.Height, desc.Depth);

    tex.msQual = 0;
    tex.msSamp = 1;

    tex.arraysize = 1;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.arraysize * tex.mips; s++)
      tex.byteSize += GetByteSize(d3dtex, s);

    return tex;
  }

  RDCERR("Unrecognised/unknown texture %s", ToStr(id).c_str());

  tex.byteSize = 0;
  tex.dimension = 2;
  tex.type = TextureType::Texture2D;
  tex.width = 1;
  tex.height = 1;
  tex.depth = 1;
  tex.cubemap = false;
  tex.mips = 1;
  tex.arraysize = 1;
  tex.msQual = 0;
  tex.msSamp = 1;

  return tex;
}

rdcarray<BufferDescription> D3D11Replay::GetBuffers()
{
  rdcarray<BufferDescription> ret;

  ret.reserve(WrappedID3D11Buffer::m_BufferList.size());

  for(auto it = WrappedID3D11Buffer::m_BufferList.begin();
      it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
  {
    // skip buffers that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(GetBuffer(it->first));
  }

  GetDebugManager()->GetCounterBuffers(ret);

  return ret;
}

rdcarray<TextureDescription> D3D11Replay::GetTextures()
{
  rdcarray<TextureDescription> ret;

  ret.reserve(WrappedID3D11Texture1D::m_TextureList.size() +
              WrappedID3D11Texture2D1::m_TextureList.size() +
              WrappedID3D11Texture3D1::m_TextureList.size());

  for(auto it = WrappedID3D11Texture1D::m_TextureList.begin();
      it != WrappedID3D11Texture1D::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(GetTexture(it->first));
  }

  for(auto it = WrappedID3D11Texture2D1::m_TextureList.begin();
      it != WrappedID3D11Texture2D1::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(GetTexture(it->first));
  }

  for(auto it = WrappedID3D11Texture3D1::m_TextureList.begin();
      it != WrappedID3D11Texture3D1::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(GetTexture(it->first));
  }

  return ret;
}

void D3D11Replay::SavePipelineState(uint32_t eventId)
{
  if(!m_D3D11PipelineState)
    return;

  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

  m_RenderStateOM = rs->OM;

  D3D11Pipe::State &ret = *m_D3D11PipelineState;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  ret.descriptorStore = m_pImmediateContext->GetDescriptorsID();
  ret.descriptorCount =
      EncodeD3D11DescriptorIndex({ShaderStage::Compute, D3D11DescriptorMapping::Count, 0});
  ret.descriptorByteSize = 1;

  ret.inputAssembly.bytecode = NULL;
  ret.inputAssembly.resourceId = ResourceId();
  ret.inputAssembly.layouts.clear();

  if(rs->IA.Layout)
  {
    const rdcarray<D3D11_INPUT_ELEMENT_DESC> &vec = m_pDevice->GetLayoutDesc(rs->IA.Layout);

    ResourceId layoutId = GetIDForDeviceChild(rs->IA.Layout);

    ret.inputAssembly.resourceId = rm->GetOriginalID(layoutId);
    ret.inputAssembly.bytecode = GetShader(ResourceId(), layoutId, ShaderEntryPoint());

    ret.inputAssembly.layouts.resize(vec.size());
    for(size_t i = 0; i < vec.size(); i++)
    {
      D3D11Pipe::Layout &l = ret.inputAssembly.layouts[i];

      l.byteOffset = vec[i].AlignedByteOffset;
      l.format = MakeResourceFormat(vec[i].Format);
      l.inputSlot = vec[i].InputSlot;
      l.perInstance = vec[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA;
      l.instanceDataStepRate = vec[i].InstanceDataStepRate;
      l.semanticIndex = vec[i].SemanticIndex;
      l.semanticName = vec[i].SemanticName;
    }
  }

  ret.inputAssembly.vertexBuffers.resize(ARRAY_COUNT(rs->IA.VBs));
  for(size_t i = 0; i < ARRAY_COUNT(rs->IA.VBs); i++)
  {
    D3D11Pipe::VertexBuffer &vb = ret.inputAssembly.vertexBuffers[i];

    vb.resourceId = rm->GetOriginalID(GetIDForDeviceChild(rs->IA.VBs[i]));
    vb.byteOffset = rs->IA.Offsets[i];
    vb.byteStride = rs->IA.Strides[i];
  }

  ret.inputAssembly.indexBuffer.resourceId =
      rm->GetOriginalID(GetIDForDeviceChild(rs->IA.IndexBuffer));
  ret.inputAssembly.indexBuffer.byteOffset = rs->IA.IndexOffset;
  switch(rs->IA.IndexFormat)
  {
    case DXGI_FORMAT_R32_UINT: ret.inputAssembly.indexBuffer.byteStride = 4; break;
    case DXGI_FORMAT_R16_UINT: ret.inputAssembly.indexBuffer.byteStride = 2; break;
    case DXGI_FORMAT_R8_UINT: ret.inputAssembly.indexBuffer.byteStride = 1; break;
    default: ret.inputAssembly.indexBuffer.byteStride = 0; break;
  }

  ret.inputAssembly.topology = MakePrimitiveTopology(rs->IA.Topo);

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  {
    D3D11Pipe::Shader *dstArr[] = {&ret.vertexShader,   &ret.hullShader,  &ret.domainShader,
                                   &ret.geometryShader, &ret.pixelShader, &ret.computeShader};
    const D3D11RenderState::Shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS,
                                                &rs->GS, &rs->PS, &rs->CS};

    for(size_t stage = 0; stage < ARRAY_COUNT(dstArr); stage++)
    {
      D3D11Pipe::Shader &dst = *dstArr[stage];
      const D3D11RenderState::Shader &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      ResourceId id = GetIDForDeviceChild(src.Object);

      WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)src.Object;

      ShaderReflection *refl = NULL;

      if(shad != NULL)
        refl = &shad->GetDetails();

      dst.resourceId = rm->GetUnreplacedOriginalID(id);
      dst.reflection = refl;

      dst.classInstances.reserve(src.NumInstances);
      for(UINT s = 0; s < src.NumInstances; s++)
      {
        D3D11_CLASS_INSTANCE_DESC desc;
        src.Instances[s]->GetDesc(&desc);

        char typeName[256] = {0};
        SIZE_T count = 255;
        src.Instances[s]->GetTypeName(typeName, &count);

        char instName[256] = {0};
        count = 255;
        src.Instances[s]->GetInstanceName(instName, &count);

        dst.classInstances.push_back(instName);
      }
    }
  }

  /////////////////////////////////////////////////
  // Stream Out
  /////////////////////////////////////////////////

  {
    ret.streamOut.outputs.resize(D3D11_SO_BUFFER_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_SO_BUFFER_SLOT_COUNT; s++)
    {
      ret.streamOut.outputs[s].resourceId = rm->GetOriginalID(GetIDForDeviceChild(rs->SO.Buffers[s]));
      ret.streamOut.outputs[s].byteOffset = rs->SO.Offsets[s];
    }

    const SOShaderData &soshader = m_pDevice->GetSOShaderData(GetIDForDeviceChild(rs->GS.Object));

    ret.streamOut.rasterizedStream = soshader.rastStream;
  }

  /////////////////////////////////////////////////
  // Rasterizer
  /////////////////////////////////////////////////

  {
    D3D11_RASTERIZER_DESC desc;

    if(rs->RS.State)
    {
      rs->RS.State->GetDesc(&desc);

      ret.rasterizer.state.antialiasedLines = desc.AntialiasedLineEnable == TRUE;

      ret.rasterizer.state.cullMode = CullMode::NoCull;
      if(desc.CullMode == D3D11_CULL_FRONT)
        ret.rasterizer.state.cullMode = CullMode::Front;
      if(desc.CullMode == D3D11_CULL_BACK)
        ret.rasterizer.state.cullMode = CullMode::Back;

      ret.rasterizer.state.fillMode = FillMode::Solid;
      if(desc.FillMode == D3D11_FILL_WIREFRAME)
        ret.rasterizer.state.fillMode = FillMode::Wireframe;

      ret.rasterizer.state.depthBias = desc.DepthBias;
      ret.rasterizer.state.depthBiasClamp = desc.DepthBiasClamp;
      ret.rasterizer.state.depthClip = desc.DepthClipEnable == TRUE;
      ret.rasterizer.state.frontCCW = desc.FrontCounterClockwise == TRUE;
      ret.rasterizer.state.multisampleEnable = desc.MultisampleEnable == TRUE;
      ret.rasterizer.state.scissorEnable = desc.ScissorEnable == TRUE;
      ret.rasterizer.state.slopeScaledDepthBias = desc.SlopeScaledDepthBias;
      ret.rasterizer.state.forcedSampleCount = 0;

      D3D11_RASTERIZER_DESC1 desc1;
      RDCEraseEl(desc1);

      if(CanQuery<ID3D11RasterizerState1>(rs->RS.State))
      {
        ((ID3D11RasterizerState1 *)rs->RS.State)->GetDesc1(&desc1);
        ret.rasterizer.state.forcedSampleCount = desc1.ForcedSampleCount;
      }

      D3D11_RASTERIZER_DESC2 desc2;
      RDCEraseEl(desc2);

      if(CanQuery<ID3D11RasterizerState2>(rs->RS.State))
      {
        ((ID3D11RasterizerState2 *)rs->RS.State)->GetDesc2(&desc2);

        // D3D only supports overestimate conservative raster (underestimated can be emulated using
        // coverage information in the shader)
        ret.rasterizer.state.conservativeRasterization =
            desc2.ConservativeRaster == D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON
                ? ConservativeRaster::Overestimate
                : ConservativeRaster::Disabled;
      }

      ret.rasterizer.state.resourceId = rm->GetOriginalID(GetIDForDeviceChild(rs->RS.State));
    }
    else
    {
      ret.rasterizer.state.antialiasedLines = FALSE;
      ret.rasterizer.state.cullMode = CullMode::Back;
      ret.rasterizer.state.depthBias = 0;
      ret.rasterizer.state.depthBiasClamp = 0.0f;
      ret.rasterizer.state.depthClip = TRUE;
      ret.rasterizer.state.fillMode = FillMode::Solid;
      ret.rasterizer.state.frontCCW = FALSE;
      ret.rasterizer.state.multisampleEnable = FALSE;
      ret.rasterizer.state.scissorEnable = FALSE;
      ret.rasterizer.state.slopeScaledDepthBias = 0.0f;
      ret.rasterizer.state.forcedSampleCount = 0;
      ret.rasterizer.state.resourceId = ResourceId();
    }

    size_t i = 0;
    ret.rasterizer.scissors.resize(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs->RS.NumScissors; i++)
      ret.rasterizer.scissors[i] = Scissor(rs->RS.Scissors[i].left, rs->RS.Scissors[i].top,
                                           rs->RS.Scissors[i].right - rs->RS.Scissors[i].left,
                                           rs->RS.Scissors[i].bottom - rs->RS.Scissors[i].top,
                                           ret.rasterizer.state.scissorEnable);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.rasterizer.scissors[i] = Scissor(0, 0, 0, 0, ret.rasterizer.state.scissorEnable);

    ret.rasterizer.viewports.resize(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs->RS.NumViews; i++)
      ret.rasterizer.viewports[i] =
          Viewport(rs->RS.Viewports[i].TopLeftX, rs->RS.Viewports[i].TopLeftY,
                   rs->RS.Viewports[i].Width, rs->RS.Viewports[i].Height,
                   rs->RS.Viewports[i].MinDepth, rs->RS.Viewports[i].MaxDepth, true);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.rasterizer.viewports[i] = Viewport(0, 0, 0, 0, 0, 0, false);
  }

  /////////////////////////////////////////////////
  // Output Merger
  /////////////////////////////////////////////////

  {
    ret.outputMerger.renderTargets.resize(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    for(size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
      Descriptor &descriptor = ret.outputMerger.renderTargets[i];

      descriptor.view = rm->GetOriginalID(GetIDForDeviceChild(rs->OM.RenderTargets[i]));

      if(descriptor.view != ResourceId())
      {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        rs->OM.RenderTargets[i]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.RenderTargets[i]->GetResource(&res);

        descriptor.bufferStructCount = 0;
        descriptor.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        descriptor.resource = rm->GetOriginalID(GetIDForDeviceChild(res));

        descriptor.type = DescriptorType::ReadWriteImage;
        descriptor.format = MakeResourceFormat(desc.Format);
        descriptor.textureType = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_RTV_DIMENSION_BUFFER)
        {
          descriptor.byteOffset = desc.Buffer.FirstElement * descriptor.elementByteSize;
          descriptor.byteSize = desc.Buffer.NumElements * descriptor.elementByteSize;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
        {
          descriptor.numSlices = 1;
          descriptor.firstSlice = 0;
          descriptor.firstMip = desc.Texture1D.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
        {
          descriptor.numSlices = desc.Texture1DArray.ArraySize & 0xffff;
          descriptor.firstSlice = desc.Texture1DArray.FirstArraySlice & 0xffff;
          descriptor.firstMip = desc.Texture1DArray.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
        {
          descriptor.numSlices = 1;
          descriptor.firstSlice = 0;
          descriptor.firstMip = desc.Texture2D.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
        {
          descriptor.numSlices = desc.Texture2DArray.ArraySize & 0xffff;
          descriptor.firstSlice = desc.Texture2DArray.FirstArraySlice & 0xffff;
          descriptor.firstMip = desc.Texture2DArray.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
        {
          descriptor.firstMip = 0;
          descriptor.numMips = 1;
          descriptor.firstSlice = 0;
          descriptor.numSlices = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
        {
          descriptor.numSlices = desc.Texture2DMSArray.ArraySize & 0xffff;
          descriptor.firstSlice = desc.Texture2DMSArray.FirstArraySlice & 0xffff;
          descriptor.firstMip = 0;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE3D)
        {
          descriptor.numSlices = desc.Texture3D.WSize & 0xffff;
          descriptor.firstSlice = desc.Texture3D.FirstWSlice & 0xffff;
          descriptor.firstMip = desc.Texture3D.MipSlice & 0xff;
          descriptor.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
      else
      {
        descriptor = Descriptor();
      }
    }

    ret.outputMerger.uavStartSlot = rs->OM.UAVStartSlot;

    {
      Descriptor &descriptor = ret.outputMerger.depthTarget;

      descriptor.view = rm->GetOriginalID(GetIDForDeviceChild(rs->OM.DepthView));

      if(descriptor.view != ResourceId())
      {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        rs->OM.DepthView->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.DepthView->GetResource(&res);

        descriptor.bufferStructCount = 0;
        descriptor.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        ret.outputMerger.depthReadOnly = false;
        ret.outputMerger.stencilReadOnly = false;

        if(desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
          ret.outputMerger.depthReadOnly = true;
        if(desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
          ret.outputMerger.stencilReadOnly = true;

        descriptor.resource = rm->GetOriginalID(GetIDForDeviceChild(res));

        descriptor.type = DescriptorType::ReadWriteImage;
        descriptor.format = MakeResourceFormat(desc.Format);
        descriptor.textureType = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
        {
          descriptor.numSlices = 1;
          descriptor.firstSlice = 0;
          descriptor.firstMip = desc.Texture1D.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
        {
          descriptor.numSlices = desc.Texture1DArray.ArraySize & 0xffff;
          descriptor.firstSlice = desc.Texture1DArray.FirstArraySlice & 0xffff;
          descriptor.firstMip = desc.Texture1DArray.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
        {
          descriptor.numSlices = 1;
          descriptor.firstSlice = 0;
          descriptor.firstMip = desc.Texture2D.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
        {
          descriptor.numSlices = desc.Texture2DArray.ArraySize & 0xffff;
          descriptor.firstSlice = desc.Texture2DArray.FirstArraySlice & 0xffff;
          descriptor.firstMip = desc.Texture2DArray.MipSlice & 0xff;
          descriptor.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS)
        {
          descriptor.firstMip = 0;
          descriptor.numMips = 1;
          descriptor.firstSlice = 0;
          descriptor.numSlices = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
        {
          descriptor.numSlices = desc.Texture2DMSArray.ArraySize & 0xffff;
          descriptor.firstSlice = desc.Texture2DMSArray.FirstArraySlice & 0xffff;
          descriptor.firstMip = 0;
          descriptor.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
      else
      {
        descriptor = Descriptor();
      }
    }

    ret.outputMerger.blendState.sampleMask = rs->OM.SampleMask;

    ret.outputMerger.blendState.blendFactor = rs->OM.BlendFactor;

    if(rs->OM.BlendState)
    {
      D3D11_BLEND_DESC desc;
      rs->OM.BlendState->GetDesc(&desc);

      ret.outputMerger.blendState.resourceId =
          rm->GetOriginalID(GetIDForDeviceChild(rs->OM.BlendState));

      ret.outputMerger.blendState.alphaToCoverage = desc.AlphaToCoverageEnable == TRUE;
      ret.outputMerger.blendState.independentBlend = desc.IndependentBlendEnable == TRUE;

      bool state1 = false;
      D3D11_BLEND_DESC1 desc1;
      RDCEraseEl(desc1);

      if(CanQuery<ID3D11BlendState1>(rs->OM.BlendState))
      {
        ((WrappedID3D11BlendState1 *)rs->OM.BlendState)->GetDesc1(&desc1);

        state1 = true;
      }

      ret.outputMerger.blendState.blends.resize(8);
      for(size_t i = 0; i < 8; i++)
      {
        ColorBlend &blend = ret.outputMerger.blendState.blends[i];

        blend.enabled = desc.RenderTarget[i].BlendEnable == TRUE;

        blend.logicOperationEnabled = state1 && desc1.RenderTarget[i].LogicOpEnable == TRUE;
        blend.logicOperation =
            state1 ? MakeLogicOp(desc1.RenderTarget[i].LogicOp) : LogicOperation::NoOp;

        blend.alphaBlend.source = MakeBlendMultiplier(desc.RenderTarget[i].SrcBlendAlpha, true);
        blend.alphaBlend.destination = MakeBlendMultiplier(desc.RenderTarget[i].DestBlendAlpha, true);
        blend.alphaBlend.operation = MakeBlendOp(desc.RenderTarget[i].BlendOpAlpha);

        blend.colorBlend.source = MakeBlendMultiplier(desc.RenderTarget[i].SrcBlend, false);
        blend.colorBlend.destination = MakeBlendMultiplier(desc.RenderTarget[i].DestBlend, false);
        blend.colorBlend.operation = MakeBlendOp(desc.RenderTarget[i].BlendOp);

        blend.writeMask = desc.RenderTarget[i].RenderTargetWriteMask;
      }
    }
    else
    {
      ret.outputMerger.blendState.resourceId = ResourceId();

      ret.outputMerger.blendState.alphaToCoverage = false;
      ret.outputMerger.blendState.independentBlend = false;

      ColorBlend blend;

      blend.enabled = false;

      blend.alphaBlend.source = BlendMultiplier::One;
      blend.alphaBlend.destination = BlendMultiplier::Zero;
      blend.alphaBlend.operation = BlendOperation::Add;

      blend.colorBlend.source = BlendMultiplier::One;
      blend.colorBlend.destination = BlendMultiplier::Zero;
      blend.colorBlend.operation = BlendOperation::Add;

      blend.logicOperationEnabled = false;
      blend.logicOperation = LogicOperation::NoOp;

      blend.writeMask = D3D11_COLOR_WRITE_ENABLE_ALL;

      ret.outputMerger.blendState.blends.resize(8);
      for(size_t i = 0; i < 8; i++)
        ret.outputMerger.blendState.blends[i] = blend;
    }

    if(rs->OM.DepthStencilState)
    {
      D3D11_DEPTH_STENCIL_DESC desc;
      rs->OM.DepthStencilState->GetDesc(&desc);

      ret.outputMerger.depthStencilState.depthEnable = desc.DepthEnable == TRUE;
      ret.outputMerger.depthStencilState.depthFunction = MakeCompareFunc(desc.DepthFunc);
      ret.outputMerger.depthStencilState.depthWrites =
          desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
      ret.outputMerger.depthStencilState.stencilEnable = desc.StencilEnable == TRUE;
      ret.outputMerger.depthStencilState.resourceId =
          rm->GetOriginalID(GetIDForDeviceChild(rs->OM.DepthStencilState));

      ret.outputMerger.depthStencilState.frontFace.function =
          MakeCompareFunc(desc.FrontFace.StencilFunc);
      ret.outputMerger.depthStencilState.frontFace.depthFailOperation =
          MakeStencilOp(desc.FrontFace.StencilDepthFailOp);
      ret.outputMerger.depthStencilState.frontFace.passOperation =
          MakeStencilOp(desc.FrontFace.StencilPassOp);
      ret.outputMerger.depthStencilState.frontFace.failOperation =
          MakeStencilOp(desc.FrontFace.StencilFailOp);

      ret.outputMerger.depthStencilState.backFace.function =
          MakeCompareFunc(desc.BackFace.StencilFunc);
      ret.outputMerger.depthStencilState.backFace.depthFailOperation =
          MakeStencilOp(desc.BackFace.StencilDepthFailOp);
      ret.outputMerger.depthStencilState.backFace.passOperation =
          MakeStencilOp(desc.BackFace.StencilPassOp);
      ret.outputMerger.depthStencilState.backFace.failOperation =
          MakeStencilOp(desc.BackFace.StencilFailOp);

      // due to shared structs, this is slightly duplicated - D3D doesn't have separate states for
      // front/back.
      ret.outputMerger.depthStencilState.frontFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.frontFace.compareMask = desc.StencilReadMask;
      ret.outputMerger.depthStencilState.frontFace.writeMask = desc.StencilWriteMask;
      ret.outputMerger.depthStencilState.backFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.backFace.compareMask = desc.StencilReadMask;
      ret.outputMerger.depthStencilState.backFace.writeMask = desc.StencilWriteMask;
    }
    else
    {
      ret.outputMerger.depthStencilState.depthEnable = true;
      ret.outputMerger.depthStencilState.depthFunction = CompareFunction::Less;
      ret.outputMerger.depthStencilState.depthWrites = true;
      ret.outputMerger.depthStencilState.stencilEnable = false;
      ret.outputMerger.depthStencilState.resourceId = ResourceId();

      ret.outputMerger.depthStencilState.frontFace.function = CompareFunction::AlwaysTrue;
      ret.outputMerger.depthStencilState.frontFace.depthFailOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.frontFace.passOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.frontFace.failOperation = StencilOperation::Keep;

      ret.outputMerger.depthStencilState.backFace.function = CompareFunction::AlwaysTrue;
      ret.outputMerger.depthStencilState.backFace.depthFailOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.backFace.passOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.backFace.failOperation = StencilOperation::Keep;

      // due to shared structs, this is slightly duplicated - D3D doesn't have separate states for
      // front/back.
      ret.outputMerger.depthStencilState.frontFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.frontFace.compareMask = D3D11_DEFAULT_STENCIL_READ_MASK;
      ret.outputMerger.depthStencilState.frontFace.writeMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
      ret.outputMerger.depthStencilState.backFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.backFace.compareMask = D3D11_DEFAULT_STENCIL_READ_MASK;
      ret.outputMerger.depthStencilState.backFace.writeMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    }
  }

  /////////////////////////////////////////////////
  // Predication
  /////////////////////////////////////////////////

  ret.predication.resourceId = rm->GetOriginalID(GetIDForDeviceChild(rs->Predicate));
  ret.predication.value = rs->PredicateValue == TRUE ? true : false;
  ret.predication.isPassing = rs->PredicationWouldPass();
}

rdcarray<Descriptor> D3D11Replay::GetDescriptors(ResourceId descriptorStore,
                                                 const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<Descriptor> ret;

  if(descriptorStore != m_pImmediateContext->GetDescriptorsID())
  {
    RDCERR(
        "Descriptors query for invalid descriptor descriptorStore on fixed bindings API (D3D11)");
    return ret;
  }

  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();
  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  const D3D11RenderState::Shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS, &rs->GS, &rs->PS, &rs->CS};

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorId = r.offset;

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorId++)
    {
      D3D11DescriptorLocation idx = DecodeD3D11DescriptorIndex(descriptorId);
      const D3D11RenderState::Shader &src = *srcArr[(uint32_t)idx.stage];

      if(idx.type == D3D11DescriptorMapping::CBs)
      {
        ret[dst].type = DescriptorType::ConstantBuffer;

        ret[dst].resource = rm->GetOriginalID(GetIDForDeviceChild(src.ConstantBuffers[idx.idx]));
        ret[dst].byteOffset = src.CBOffsets[idx.idx] * sizeof(Vec4f);
        ret[dst].byteSize = src.CBCounts[idx.idx] * sizeof(Vec4f);
      }
      else if(idx.type == D3D11DescriptorMapping::SRVs)
      {
        ID3D11ShaderResourceView *view = src.SRVs[idx.idx];

        ret[dst].view = rm->GetOriginalID(GetIDForDeviceChild(view));

        ret[dst].type = DescriptorType::Image;
        if(ret[dst].view != ResourceId())
        {
          D3D11_SHADER_RESOURCE_VIEW_DESC desc;
          view->GetDesc(&desc);

          ret[dst].format = MakeResourceFormat(desc.Format);

          ID3D11Resource *res = NULL;
          view->GetResource(&res);

          ret[dst].elementByteSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          ret[dst].resource = rm->GetOriginalID(GetIDForDeviceChild(res));

          ret[dst].textureType = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
          {
            ret[dst].type = DescriptorType::TypedBuffer;

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            if(bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN)
            {
              ret[dst].elementByteSize = bufdesc.StructureByteStride;
              ret[dst].type = DescriptorType::Buffer;
            }

            ret[dst].byteOffset = desc.Buffer.FirstElement * ret[dst].elementByteSize;
            ret[dst].byteSize = desc.Buffer.NumElements * ret[dst].elementByteSize;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
          {
            ret[dst].type = DescriptorType::TypedBuffer;

            ret[dst].flags = DescriptorFlags(desc.BufferEx.Flags);

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            if(bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN)
            {
              ret[dst].elementByteSize = bufdesc.StructureByteStride;
              ret[dst].type = DescriptorType::Buffer;
            }

            ret[dst].byteOffset = desc.BufferEx.FirstElement * ret[dst].elementByteSize;
            ret[dst].byteSize = desc.BufferEx.NumElements * ret[dst].elementByteSize;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
          {
            ret[dst].firstMip = desc.Texture1D.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.Texture1D.MipLevels & 0xff;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
          {
            ret[dst].firstMip = desc.Texture1DArray.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.Texture1DArray.MipLevels & 0xff;
            ret[dst].numSlices = desc.Texture1DArray.ArraySize & 0xffff;
            ret[dst].firstSlice = desc.Texture1DArray.FirstArraySlice & 0xffff;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
          {
            ret[dst].firstMip = desc.Texture2D.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.Texture2D.MipLevels & 0xff;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
          {
            ret[dst].firstMip = desc.Texture2DArray.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.Texture2DArray.MipLevels & 0xff;
            ret[dst].firstSlice = desc.Texture2DArray.FirstArraySlice & 0xffff;
            ret[dst].numSlices = desc.Texture2DArray.ArraySize & 0xffff;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
          {
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
          {
            ret[dst].numSlices = desc.Texture2DMSArray.ArraySize & 0xffff;
            ret[dst].firstSlice = desc.Texture2DMSArray.FirstArraySlice & 0xffff;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D)
          {
            ret[dst].firstMip = desc.Texture3D.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.Texture3D.MipLevels & 0xff;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
          {
            ret[dst].firstMip = desc.TextureCube.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.TextureCube.MipLevels & 0xff;
            ret[dst].numSlices = 6;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
          {
            ret[dst].firstMip = desc.TextureCubeArray.MostDetailedMip & 0xff;
            ret[dst].numMips = desc.TextureCubeArray.MipLevels & 0xff;
            ret[dst].firstSlice = desc.TextureCubeArray.First2DArrayFace & 0xffff;
            ret[dst].numSlices = (desc.TextureCubeArray.NumCubes * 6) & 0xffff;
          }

          SAFE_RELEASE(res);
        }
      }
      else if(idx.type == D3D11DescriptorMapping::UAVs)
      {
        ID3D11UnorderedAccessView *view = NULL;

        if(idx.stage == ShaderStage::Compute)
          view = rs->CSUAVs[idx.idx];
        else if(idx.idx >= rs->OM.UAVStartSlot)
          view = rs->OM.UAVs[idx.idx - rs->OM.UAVStartSlot];

        ret[dst].view = rm->GetOriginalID(GetIDForDeviceChild(view));

        ret[dst].type = DescriptorType::ReadWriteImage;
        if(ret[dst].view != ResourceId())
        {
          D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
          view->GetDesc(&desc);

          ID3D11Resource *res = NULL;
          view->GetResource(&res);

          ret[dst].bufferStructCount = 0;

          ret[dst].elementByteSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          ret[dst].textureType = MakeTextureDim(desc.ViewDimension);

          ret[dst].secondary = ResourceId();

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
             (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)))
          {
            ret[dst].bufferStructCount = GetDebugManager()->GetStructCount(view);

            ret[dst].secondary = GetDebugManager()->GetCounterBufferID(view);
          }

          ret[dst].resource = rm->GetOriginalID(GetIDForDeviceChild(res));

          ret[dst].format = MakeResourceFormat(desc.Format);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
          {
            ret[dst].type = DescriptorType::ReadWriteBuffer;

            if(desc.Format != DXGI_FORMAT_UNKNOWN)
              ret[dst].type = DescriptorType::ReadWriteTypedBuffer;

            ret[dst].flags = DescriptorFlags(desc.Buffer.Flags);

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            if(bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN)
              ret[dst].elementByteSize = bufdesc.StructureByteStride;

            ret[dst].byteOffset = desc.Buffer.FirstElement * ret[dst].elementByteSize;
            ret[dst].byteSize = desc.Buffer.NumElements * ret[dst].elementByteSize;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
          {
            ret[dst].firstMip = desc.Texture1D.MipSlice & 0xff;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
          {
            ret[dst].numSlices = desc.Texture1DArray.ArraySize & 0xffff;
            ret[dst].firstSlice = desc.Texture1DArray.FirstArraySlice & 0xffff;
            ret[dst].firstMip = desc.Texture1DArray.MipSlice & 0xff;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
          {
            ret[dst].firstMip = desc.Texture2D.MipSlice & 0xff;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
          {
            ret[dst].numSlices = desc.Texture2DArray.ArraySize & 0xffff;
            ret[dst].firstSlice = desc.Texture2DArray.FirstArraySlice & 0xffff;
            ret[dst].firstMip = desc.Texture2DArray.MipSlice & 0xff;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
          {
            ret[dst].numSlices = desc.Texture3D.WSize & 0xffff;
            ret[dst].firstSlice = desc.Texture3D.FirstWSlice & 0xffff;
            ret[dst].firstMip = desc.Texture3D.MipSlice & 0xff;
          }

          SAFE_RELEASE(res);
        }
      }
    }
  }

  return ret;
}

rdcarray<SamplerDescriptor> D3D11Replay::GetSamplerDescriptors(ResourceId descriptorStore,
                                                               const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<SamplerDescriptor> ret;

  if(descriptorStore != m_pImmediateContext->GetDescriptorsID())
  {
    RDCERR(
        "Descriptors query for invalid descriptor descriptorStore on fixed bindings API (D3D11)");
    return ret;
  }

  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();
  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  const D3D11RenderState::Shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS, &rs->GS, &rs->PS, &rs->CS};

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorId = r.offset;

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorId++)
    {
      D3D11DescriptorLocation idx = DecodeD3D11DescriptorIndex(descriptorId);

      if(idx.type != D3D11DescriptorMapping::Samplers)
        continue;

      ID3D11SamplerState *samp = srcArr[(uint32_t)idx.stage]->Samplers[idx.idx];

      ret[dst].type = DescriptorType::Sampler;
      ret[dst].object = rm->GetOriginalID(GetIDForDeviceChild(samp));

      if(ret[dst].object != ResourceId())
      {
        D3D11_SAMPLER_DESC desc;
        samp->GetDesc(&desc);

        ret[dst].addressU = MakeAddressMode(desc.AddressU);
        ret[dst].addressV = MakeAddressMode(desc.AddressV);
        ret[dst].addressW = MakeAddressMode(desc.AddressW);

        ret[dst].borderColorValue.floatValue = desc.BorderColor;
        ret[dst].borderColorType = CompType::Float;

        ret[dst].compareFunction = MakeCompareFunc(desc.ComparisonFunc);
        ret[dst].filter = MakeFilter(desc.Filter);
        ret[dst].maxAnisotropy = 0;
        if(ret[dst].filter.mip == FilterMode::Anisotropic)
          ret[dst].maxAnisotropy = (float)desc.MaxAnisotropy;
        ret[dst].maxLOD = desc.MaxLOD;
        ret[dst].minLOD = desc.MinLOD;
        ret[dst].mipBias = desc.MipLODBias;
      }
    }
  }

  return ret;
}

rdcarray<DescriptorAccess> D3D11Replay::GetDescriptorAccess(uint32_t eventId)
{
  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

  rdcarray<DescriptorAccess> ret;

  const D3D11RenderState::Shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS, &rs->GS, &rs->PS, &rs->CS};

  for(size_t stage = 0; stage < ARRAY_COUNT(srcArr); stage++)
  {
    const D3D11RenderState::Shader &src = *srcArr[stage];

    WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)src.Object;

    if(shad)
      ret.append(shad->GetDescriptorAccess());
  }

  return ret;
}

rdcarray<DescriptorLogicalLocation> D3D11Replay::GetDescriptorLocations(
    ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<DescriptorLogicalLocation> ret;

  if(descriptorStore != m_pImmediateContext->GetDescriptorsID())
  {
    RDCERR("Descriptors query for invalid descriptor store on fixed bindings API (D3D11)");
    return ret;
  }

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorByteOffset = r.offset;

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorByteOffset++)
    {
      DescriptorLogicalLocation &dstLoc = ret[dst];
      D3D11DescriptorLocation srcLoc = DecodeD3D11DescriptorIndex(descriptorByteOffset);

      dstLoc.stageMask = MaskForStage(srcLoc.stage);
      char typePrefix = '?';
      switch(srcLoc.type)
      {
        case D3D11DescriptorMapping::CBs:
          typePrefix = 'b';
          dstLoc.category = DescriptorCategory::ConstantBlock;
          break;
        case D3D11DescriptorMapping::Samplers:
          typePrefix = 's';
          dstLoc.category = DescriptorCategory::Sampler;
          break;
        case D3D11DescriptorMapping::SRVs:
          typePrefix = 't';
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case D3D11DescriptorMapping::UAVs:
          typePrefix = 'u';
          dstLoc.category = DescriptorCategory::ReadWriteResource;
          break;
        case D3D11DescriptorMapping::Count:
        case D3D11DescriptorMapping::Invalid: dstLoc.category = DescriptorCategory::Unknown; break;
      }
      dstLoc.fixedBindNumber = srcLoc.idx;

      dstLoc.logicalBindName = StringFormat::Fmt("%c%u", typePrefix, srcLoc.idx);
    }
  }

  return ret;
}

RDResult D3D11Replay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDevice->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

void D3D11Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);

  // if this is a fresh replay from start, update the render state with the bindings at this event
  if(replayType == eReplay_WithoutDraw || replayType == eReplay_Full)
  {
    D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

    m_RenderStateOM = rs->OM;
  }
}

SDFile *D3D11Replay::GetStructuredFile()
{
  return m_pDevice->GetStructuredFile();
}

rdcarray<uint32_t> D3D11Replay::GetPassEvents(uint32_t eventId)
{
  rdcarray<uint32_t> passEvents;

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  const ActionDescription *start = action;
  while(start && start->previous && !(start->previous->flags & ActionFlags::Clear))
  {
    const ActionDescription *prev = start->previous;

    if(start->outputs != prev->outputs || start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  while(start)
  {
    if(start == action)
      break;

    if(start->flags & ActionFlags::Drawcall)
      passEvents.push_back(start->eventId);

    start = start->next;
  }

  return passEvents;
}

ResourceId D3D11Replay::GetLiveID(ResourceId id)
{
  ID3D11UnorderedAccessView *counterUAV = GetDebugManager()->GetCounterBufferUAV(id);
  if(counterUAV)
    return id;
  if(!m_pDevice->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

void D3D11Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                            CompType typeCast, float pixel[4])
{
  D3D11RenderStateTracker tracker(m_pImmediateContext);

  D3D11MarkerRegion marker("PickPixel");

  m_pImmediateContext->OMSetRenderTargets(1, &m_PixelPick.RTV, NULL);

  float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  m_pImmediateContext->ClearRenderTargetView(m_PixelPick.RTV, color);

  D3D11_VIEWPORT viewport;
  RDCEraseEl(viewport);

  SetOutputDimensions(100, 100);

  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = 100;
  viewport.Height = 100;

  m_pImmediateContext->RSSetViewports(1, &viewport);

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

    uint32_t texWidth = 1, texHeight = 1;

    auto it1 = WrappedID3D11Texture1D::m_TextureList.find(texture);
    auto it2 = WrappedID3D11Texture2D1::m_TextureList.find(texture);
    auto it3 = WrappedID3D11Texture3D1::m_TextureList.find(texture);
    if(it1 != WrappedID3D11Texture1D::m_TextureList.end())
    {
      WrappedID3D11Texture1D *wrapTex1D = (WrappedID3D11Texture1D *)it1->second.m_Texture;

      D3D11_TEXTURE1D_DESC desc1d = {0};
      wrapTex1D->GetDesc(&desc1d);

      texWidth = desc1d.Width;
    }
    else if(it2 != WrappedID3D11Texture2D1::m_TextureList.end())
    {
      WrappedID3D11Texture2D1 *wrapTex2D = (WrappedID3D11Texture2D1 *)it2->second.m_Texture;

      D3D11_TEXTURE2D_DESC desc2d = {0};
      wrapTex2D->GetDesc(&desc2d);

      texWidth = desc2d.Width;
      texHeight = desc2d.Height;
    }
    else if(it3 != WrappedID3D11Texture3D1::m_TextureList.end())
    {
      WrappedID3D11Texture3D1 *wrapTex3D = (WrappedID3D11Texture3D1 *)it3->second.m_Texture;

      D3D11_TEXTURE3D_DESC desc3d = {0};
      wrapTex3D->GetDesc(&desc3d);

      texWidth = desc3d.Width;
      texHeight = desc3d.Height;
    }

    uint32_t mipWidth = RDCMAX(1U, texWidth >> sub.mip);
    uint32_t mipHeight = RDCMAX(1U, texHeight >> sub.mip);

    texDisplay.xOffset = -(float(x) / float(mipWidth)) * texWidth;
    texDisplay.yOffset = -(float(y) / float(mipHeight)) * texHeight;

    RenderTextureInternal(texDisplay, eTexDisplay_None);
  }

  D3D11_BOX box;
  box.front = 0;
  box.back = 1;
  box.left = 0;
  box.right = 1;
  box.top = 0;
  box.bottom = 1;

  m_pImmediateContext->CopySubresourceRegion(m_PixelPick.StageTexture, 0, 0, 0, 0,
                                             m_PixelPick.Texture, 0, &box);

  D3D11_MAPPED_SUBRESOURCE mapped;
  mapped.pData = NULL;
  HRESULT hr = m_pImmediateContext->Map(m_PixelPick.StageTexture, 0, D3D11_MAP_READ, 0, &mapped);

  m_pDevice->CheckHRESULT(hr);

  if(FAILED(hr))
  {
    RDCERR("Failed to map stage buff HRESULT: %s", ToStr(hr).c_str());
  }

  float *pix = (float *)mapped.pData;

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

  m_pImmediateContext->Unmap(m_PixelPick.StageTexture, 0);
}

bool D3D11Replay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float *minval, float *maxval)
{
  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(texid, typeCast, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = (float)RDCMAX(details.texWidth >> sub.mip, 1U);
  cdata.HistogramTextureResolution.y = (float)RDCMAX(details.texHeight >> sub.mip, 1U);
  cdata.HistogramTextureResolution.z = (float)RDCMAX(details.texDepth >> sub.mip, 1U);
  if(details.texType == eTexType_3D)
    cdata.HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, (details.texDepth >> sub.mip) - 1);
  else
    cdata.HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, details.texArraySize - 1);
  cdata.HistogramMip = sub.mip;
  cdata.HistogramSample = (int)RDCCLAMP(sub.sample, 0U, details.sampleCount - 1);
  if(sub.sample == ~0U)
    cdata.HistogramSample = -int(details.sampleCount);
  cdata.HistogramMin = 0.0f;
  cdata.HistogramMax = 1.0f;
  cdata.HistogramChannels = 0xf;
  cdata.HistogramFlags = 0;

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(details.texFmt, YUVDownsampleRate, YUVAChannels);

  cdata.HistogramYUVDownsampleRate = YUVDownsampleRate;
  cdata.HistogramYUVAChannels = YUVAChannels;

  int srvOffset = 0;
  int intIdx = 0;

  DXGI_FORMAT fmt = GetTypedFormat(details.texFmt, typeCast);

  if(IsUIntFormat(fmt))
  {
    cdata.HistogramFlags |= TEXDISPLAY_UINT_TEX;
    srvOffset = 10;
    intIdx = 1;
  }
  if(IsIntFormat(fmt))
  {
    cdata.HistogramFlags |= TEXDISPLAY_SINT_TEX;
    srvOffset = 20;
    intIdx = 2;
  }

  ID3D11Buffer *cbuf = GetDebugManager()->MakeCBuffer(&cdata, sizeof(cdata));

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &cbuf);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {NULL};
  const UINT numUAVs =
      m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  uavs[intIdx] = m_Histogram.TileResultUAV[intIdx];
  m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, uavs, NULL);

  m_pImmediateContext->CSSetShaderResources(srvOffset, eTexType_Max, details.srv);

  m_pImmediateContext->CSSetShader(m_Histogram.TileMinMaxCS[details.texType][intIdx], NULL, 0);

  int blocksX = (int)ceil(cdata.HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata.HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  m_pImmediateContext->Dispatch(blocksX, blocksY, 1);

  m_pImmediateContext->CSSetUnorderedAccessViews(intIdx, 1, &m_Histogram.ResultUAV[intIdx], NULL);
  m_pImmediateContext->CSSetShaderResources(intIdx, 1, &m_Histogram.TileResultSRV[intIdx]);

  m_pImmediateContext->CSSetShader(m_Histogram.ResultMinMaxCS[intIdx], NULL, 0);

  m_pImmediateContext->Dispatch(1, 1, 1);

  m_pImmediateContext->CopyResource(m_Histogram.ResultStageBuff, m_Histogram.ResultBuff);

  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->Map(m_Histogram.ResultStageBuff, 0, D3D11_MAP_READ, 0, &mapped);

  m_pDevice->CheckHRESULT(hr);

  if(FAILED(hr))
  {
    RDCERR("Failed to map minmax results buffer HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    Vec4f *minmax = (Vec4f *)mapped.pData;

    minval[0] = minmax[0].x;
    minval[1] = minmax[0].y;
    minval[2] = minmax[0].z;
    minval[3] = minmax[0].w;

    maxval[0] = minmax[1].x;
    maxval[1] = minmax[1].y;
    maxval[2] = minmax[1].z;
    maxval[3] = minmax[1].w;

    m_pImmediateContext->Unmap(m_Histogram.ResultStageBuff, 0);
  }

  return true;
}

bool D3D11Replay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                               float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                               rdcarray<uint32_t> &histogram)
{
  if(minval >= maxval)
    return false;

  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(texid, typeCast, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = (float)RDCMAX(details.texWidth >> sub.mip, 1U);
  cdata.HistogramTextureResolution.y = (float)RDCMAX(details.texHeight >> sub.mip, 1U);
  cdata.HistogramTextureResolution.z = (float)RDCMAX(details.texDepth >> sub.mip, 1U);
  if(details.texType == eTexType_3D)
    cdata.HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, (details.texDepth >> sub.mip) - 1);
  else
    cdata.HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, details.texArraySize - 1);
  cdata.HistogramMip = sub.mip;
  cdata.HistogramSample = (int)RDCCLAMP(sub.sample, 0U, details.sampleCount - 1);
  if(sub.sample == ~0U)
    cdata.HistogramSample = -int(details.sampleCount);
  cdata.HistogramMin = minval;

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

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(details.texFmt, YUVDownsampleRate, YUVAChannels);

  cdata.HistogramYUVDownsampleRate = YUVDownsampleRate;
  cdata.HistogramYUVAChannels = YUVAChannels;

  int srvOffset = 0;
  int intIdx = 0;

  if(IsUIntFormat(details.texFmt))
  {
    cdata.HistogramFlags |= TEXDISPLAY_UINT_TEX;
    srvOffset = 10;
    intIdx = 1;
  }
  if(IsIntFormat(details.texFmt))
  {
    cdata.HistogramFlags |= TEXDISPLAY_SINT_TEX;
    srvOffset = 20;
    intIdx = 2;
  }

  if(details.texType == eTexType_3D)
    cdata.HistogramSlice = float(sub.slice);

  ID3D11Buffer *cbuf = GetDebugManager()->MakeCBuffer(&cdata, sizeof(cdata));

  UINT zeroes[] = {0, 0, 0, 0};
  m_pImmediateContext->ClearUnorderedAccessViewUint(m_Histogram.HistogramUAV, zeroes);

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {0};
  UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&UAV_keepcounts[0], 0xff, sizeof(UAV_keepcounts));

  const UINT numUAVs =
      m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  uavs[0] = m_Histogram.HistogramUAV;
  m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, uavs, UAV_keepcounts);

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &cbuf);

  m_pImmediateContext->CSSetShaderResources(srvOffset, eTexType_Max, details.srv);

  m_pImmediateContext->CSSetShader(m_Histogram.HistogramCS[details.texType][intIdx], NULL, 0);

  int tilesX = (int)ceil(cdata.HistogramTextureResolution.x /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int tilesY = (int)ceil(cdata.HistogramTextureResolution.y /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  m_pImmediateContext->Dispatch(tilesX, tilesY, 1);

  m_pImmediateContext->CopyResource(m_Histogram.ResultStageBuff, m_Histogram.ResultBuff);

  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->Map(m_Histogram.ResultStageBuff, 0, D3D11_MAP_READ, 0, &mapped);

  m_pDevice->CheckHRESULT(hr);

  histogram.clear();
  histogram.resize(HGRAM_NUM_BUCKETS);

  if(FAILED(hr))
  {
    RDCERR("Can't map histogram stage buff HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    memcpy(&histogram[0], mapped.pData, sizeof(uint32_t) * HGRAM_NUM_BUCKETS);

    m_pImmediateContext->Unmap(m_Histogram.ResultStageBuff, 0);
  }

  return true;
}

void D3D11Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, bytebuf &retData)
{
  ID3D11UnorderedAccessView *counterUAV = GetDebugManager()->GetCounterBufferUAV(buff);
  if(counterUAV)
  {
    uint32_t count = GetDebugManager()->GetStructCount(counterUAV);

    // copy the uint first
    retData.resize(4U);
    memcpy(retData.data(), &count, retData.size());

    // remove offset bytes, up to 4
    retData.erase(0, (size_t)RDCMIN(4ULL, offset));
    return;
  }

  auto it = WrappedID3D11Buffer::m_BufferList.find(buff);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
  {
    RDCERR("Getting buffer data for unknown buffer %s!", ToStr(buff).c_str());
    return;
  }

  ID3D11Buffer *buffer = it->second.m_Buffer;

  RDCASSERT(buffer);

  GetDebugManager()->GetBufferData(buffer, offset, length, retData);
}

void D3D11Replay::GetTextureData(ResourceId tex, const Subresource &sub,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11Resource *dummyTex = NULL;

  uint32_t subresource = 0;
  uint32_t mips = 0;

  Subresource s = sub;

  size_t bytesize = 0;

  if(WrappedID3D11Texture1D::m_TextureList.find(tex) != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *wrapTex =
        (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[tex].m_Texture;

    D3D11_TEXTURE1D_DESC desc = {0};
    wrapTex->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture1D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);

    s.mip = RDCMIN(mips - 1, s.mip);
    s.slice = RDCMIN(desc.ArraySize - 1, s.slice);

    if(params.remap != RemapTexture::NoRemap)
    {
      if(params.remap == RemapTexture::RGBA8)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS, BaseRemapType(params));
        if(IsSRGBFormat(desc.Format) && params.typeCast == CompType::Typeless)
          desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      }
      else if(params.remap == RemapTexture::RGBA16)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS, BaseRemapType(params));
      }
      else if(params.remap == RemapTexture::RGBA32)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS, BaseRemapType(params));
      }

      desc.ArraySize = 1;
    }

    subresource = s.slice * mips + s.mip;

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, 1, 1, desc.Format, s.mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      subresource = s.mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture1D *rtTex = NULL;

      hr = m_pDevice->CreateTexture1D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture1D.MipSlice = s.mip;

      ID3D11RenderTargetView *wrappedrtv = NULL;
      hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
      if(FAILED(hr))
      {
        RDCERR("Couldn't create target rtv to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        SAFE_RELEASE(rtTex);
        return;
      }

      ID3D11RenderTargetView *rtv = wrappedrtv;

      m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
      float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      m_pImmediateContext->ClearRenderTargetView(rtv, color);

      D3D11_VIEWPORT viewport = {
          0, 0, (float)desc.Width, 1.0f, 0.0f, 1.0f,
      };
      m_pImmediateContext->RSSetViewports(1, &viewport);
      SetOutputDimensions(desc.Width, 1);

      TexDisplayFlags flags = eTexDisplay_None;

      if(IsUIntFormat(desc.Format))
        flags = eTexDisplay_RemapUInt;
      else if(IsIntFormat(desc.Format))
        flags = eTexDisplay_RemapSInt;
      else
        flags = eTexDisplay_RemapFloat;

      {
        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.subresource = s;
        texDisplay.subresource.sample = 0;
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

        RenderTextureInternal(texDisplay, flags);
      }

      m_pImmediateContext->CopyResource(d, rtTex);
      SAFE_RELEASE(rtTex);

      SAFE_RELEASE(wrappedrtv);
    }
    else
    {
      m_pImmediateContext->CopyResource(d, wrapTex);
    }
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(tex) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *wrapTex =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[tex].m_Texture;

    D3D11_TEXTURE2D_DESC desc = {0};
    wrapTex->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;

    bool wasms = false;

    UINT sampleCount = desc.SampleDesc.Count;

    s.mip = RDCMIN(mips - 1, s.mip);
    s.slice = RDCMIN(desc.ArraySize - 1, s.slice);
    s.sample = RDCMIN(sampleCount - 1, s.sample);

    if(desc.SampleDesc.Count > 1)
    {
      desc.ArraySize *= desc.SampleDesc.Count;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;

      wasms = true;
    }

    ID3D11Texture2D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);

    if(params.remap != RemapTexture::NoRemap)
    {
      if(params.remap == RemapTexture::RGBA8)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS, BaseRemapType(params));
        if((IsSRGBFormat(desc.Format) || wrapTex->m_RealDescriptor) &&
           params.typeCast == CompType::Typeless)
          desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      }
      else if(params.remap == RemapTexture::RGBA16)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS, BaseRemapType(params));
      }
      else if(params.remap == RemapTexture::RGBA32)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS, BaseRemapType(params));
      }

      desc.ArraySize = 1;
    }

    subresource = s.slice * mips + s.mip;

    HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, 1, desc.Format, s.mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      subresource = s.mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture2D *rtTex = NULL;

      hr = m_pDevice->CreateTexture2D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture2D.MipSlice = s.mip;

      ID3D11RenderTargetView *wrappedrtv = NULL;
      hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
      if(FAILED(hr))
      {
        RDCERR("Couldn't create target rtv to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        SAFE_RELEASE(rtTex);
        return;
      }

      ID3D11RenderTargetView *rtv = wrappedrtv;

      m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
      float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      m_pImmediateContext->ClearRenderTargetView(rtv, color);

      D3D11_VIEWPORT viewport = {
          0, 0, (float)desc.Width, (float)desc.Height, 0.0f, 1.0f,
      };

      SetOutputDimensions(desc.Width, desc.Height);
      m_pImmediateContext->RSSetViewports(1, &viewport);

      TexDisplayFlags flags = eTexDisplay_None;

      if(IsUIntFormat(desc.Format))
        flags = eTexDisplay_RemapUInt;
      else if(IsIntFormat(desc.Format))
        flags = eTexDisplay_RemapSInt;
      else
        flags = eTexDisplay_RemapFloat;

      {
        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.subresource.mip = s.mip;
        texDisplay.subresource.slice = s.slice;
        texDisplay.subresource.sample = params.resolve ? ~0U : s.sample;
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

        RenderTextureInternal(texDisplay, flags);
      }

      m_pImmediateContext->CopyResource(d, rtTex);
      SAFE_RELEASE(rtTex);

      SAFE_RELEASE(wrappedrtv);
    }
    else if(wasms && params.resolve)
    {
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.CPUAccessFlags = 0;

      ID3D11Texture2D *resolveTex = NULL;

      hr = m_pDevice->CreateTexture2D(&desc, NULL, &resolveTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to resolve texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
      }

      m_pImmediateContext->ResolveSubresource(resolveTex, s.slice, wrapTex, s.slice, desc.Format);
      m_pImmediateContext->CopyResource(d, resolveTex);

      SAFE_RELEASE(resolveTex);
    }
    else if(wasms)
    {
      GetDebugManager()->CopyTex2DMSToArray(UNWRAP(WrappedID3D11Texture2D1, d), wrapTex->GetReal());

      subresource = (s.slice * sampleCount + s.sample) * mips + s.mip;
    }
    else
    {
      m_pImmediateContext->CopyResource(d, wrapTex);
    }
  }
  else if(WrappedID3D11Texture3D1::m_TextureList.find(tex) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *wrapTex =
        (WrappedID3D11Texture3D1 *)WrappedID3D11Texture3D1::m_TextureList[tex].m_Texture;

    D3D11_TEXTURE3D_DESC desc = {0};
    wrapTex->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture3D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);

    s.mip = RDCMIN(mips - 1, s.mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      if(params.remap == RemapTexture::RGBA8)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R8G8B8A8_TYPELESS, BaseRemapType(params));
        if(IsSRGBFormat(desc.Format) && params.typeCast == CompType::Typeless)
          desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      }
      else if(params.remap == RemapTexture::RGBA16)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R16G16B16A16_TYPELESS, BaseRemapType(params));
      }
      else if(params.remap == RemapTexture::RGBA32)
      {
        desc.Format = GetTypedFormat(DXGI_FORMAT_R32G32B32A32_TYPELESS, BaseRemapType(params));
      }
    }

    subresource = s.mip;

    HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, s.mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      subresource = s.mip;

      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;

      ID3D11Texture3D *rtTex = NULL;

      hr = m_pDevice->CreateTexture3D(&desc, NULL, &rtTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create target texture to downcast texture. HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(d);
        return;
      }

      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
      rtvDesc.Format = desc.Format;
      rtvDesc.Texture3D.MipSlice = s.mip;
      rtvDesc.Texture3D.FirstWSlice = 0;
      rtvDesc.Texture3D.WSize = 1;
      ID3D11RenderTargetView *wrappedrtv = NULL;
      ID3D11RenderTargetView *rtv = NULL;

      D3D11_VIEWPORT viewport = {
          0, 0, (float)desc.Width, (float)desc.Height, 0.0f, 1.0f,
      };

      TexDisplayFlags flags = eTexDisplay_None;

      if(IsUIntFormat(desc.Format))
        flags = eTexDisplay_RemapUInt;
      else if(IsIntFormat(desc.Format))
        flags = eTexDisplay_RemapSInt;
      else
        flags = eTexDisplay_RemapFloat;

      for(UINT i = 0; i < (desc.Depth >> s.mip); i++)
      {
        rtvDesc.Texture3D.FirstWSlice = i;
        hr = m_pDevice->CreateRenderTargetView(rtTex, &rtvDesc, &wrappedrtv);
        if(FAILED(hr))
        {
          RDCERR("Couldn't create target rtv to downcast texture. HRESULT: %s", ToStr(hr).c_str());
          SAFE_RELEASE(d);
          SAFE_RELEASE(rtTex);
          return;
        }

        rtv = wrappedrtv;

        m_pImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
        float color[4] = {0.0f, 0.5f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTargetView(rtv, color);

        SetOutputDimensions(desc.Width, desc.Height);
        m_pImmediateContext->RSSetViewports(1, &viewport);

        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.subresource.mip = s.mip;
        texDisplay.subresource.slice = i;
        texDisplay.subresource.sample = 0;
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

        RenderTextureInternal(texDisplay, flags);

        SAFE_RELEASE(wrappedrtv);
      }

      m_pImmediateContext->CopyResource(d, rtTex);
      SAFE_RELEASE(rtTex);
    }
    else
    {
      m_pImmediateContext->CopyResource(d, wrapTex);
    }
  }
  else
  {
    RDCERR("Trying to get texture data for unknown ID %s!", ToStr(tex).c_str());
    return;
  }

  MapIntercept intercept;

  D3D11_MAPPED_SUBRESOURCE mapped = {0};
  HRESULT hr = m_pImmediateContext->Map(dummyTex, subresource, D3D11_MAP_READ, 0, &mapped);

  if(SUCCEEDED(hr))
  {
    data.resize(bytesize);
    intercept.InitWrappedResource(dummyTex, subresource, data.data());
    intercept.SetD3D(mapped);
    intercept.CopyFromD3D();

    // for 3D textures if we wanted a particular slice (arrayIdx > 0)
    // copy it into the beginning.
    if(intercept.numSlices > 1 && s.slice > 0 && (int)s.slice < intercept.numSlices)
    {
      byte *dst = data.data();
      byte *src = data.data() + intercept.app.DepthPitch * s.slice;

      for(int row = 0; row < intercept.numRows; row++)
      {
        memcpy(dst, src, intercept.app.RowPitch);

        src += intercept.app.RowPitch;
        dst += intercept.app.RowPitch;
      }
    }
  }
  else
  {
    RDCERR("Couldn't map staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
  }

  SAFE_RELEASE(dummyTex);
}

rdcarray<ShaderSourcePrefix> D3D11Replay::GetCustomShaderSourcePrefixes()
{
  return {
      {ShaderEncoding::HLSL, HLSL_CUSTOM_PREFIX},
  };
}

void D3D11Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  auto fromit = WrappedShader::m_ShaderList.find(from);

  if(fromit != WrappedShader::m_ShaderList.end())
  {
    auto toit = WrappedShader::m_ShaderList.find(to);

    // copy the shader ext slot
    toit->second->SetShaderExtSlot(fromit->second->GetShaderExtSlot());
  }

  m_pDevice->GetResourceManager()->ReplaceResource(from, to);
  ClearPostVSCache();
}

void D3D11Replay::RemoveReplacement(ResourceId id)
{
  m_pDevice->GetResourceManager()->RemoveReplacement(id);
  ClearPostVSCache();
}

D3D11DebugManager *D3D11Replay::GetDebugManager()
{
  return m_pDevice->GetDebugManager();
}

void D3D11Replay::BuildShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                              const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                              const rdcarray<rdcstr> &includeDirs, ShaderStage type, ResourceId &id,
                              rdcstr &errors)
{
  bytebuf compiledDXBC;

  const byte *dxbcBytes = source.data();
  size_t dxbcLength = source.size();

  if(sourceEncoding == ShaderEncoding::HLSL)
  {
    uint32_t flags = DXBC::DecodeFlags(compileFlags);
    rdcstr profile = DXBC::GetProfile(compileFlags);

    if(profile.empty())
    {
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
          id = ResourceId();
          return;
      }
    }

    rdcstr hlsl;
    hlsl.assign((const char *)source.data(), source.size());

    ID3DBlob *blob = NULL;

    errors = m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), entry.c_str(), flags,
                                                        includeDirs, profile.c_str(), &blob);

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

  switch(type)
  {
    case ShaderStage::Vertex:
    {
      ID3D11VertexShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateVertexShader(dxbcBytes, dxbcLength, NULL, &sh);

      if(sh != NULL)
      {
        id = ((WrappedID3D11Shader<ID3D11VertexShader> *)sh)->GetResourceID();
      }
      else
      {
        errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        id = ResourceId();
      }
      return;
    }
    case ShaderStage::Hull:
    {
      ID3D11HullShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateHullShader(dxbcBytes, dxbcLength, NULL, &sh);

      if(sh != NULL)
      {
        id = ((WrappedID3D11Shader<ID3D11HullShader> *)sh)->GetResourceID();
      }
      else
      {
        errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        id = ResourceId();
      }
      return;
    }
    case ShaderStage::Domain:
    {
      ID3D11DomainShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateDomainShader(dxbcBytes, dxbcLength, NULL, &sh);

      if(sh != NULL)
      {
        id = ((WrappedID3D11Shader<ID3D11DomainShader> *)sh)->GetResourceID();
      }
      else
      {
        errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        id = ResourceId();
      }
      return;
    }
    case ShaderStage::Geometry:
    {
      ID3D11GeometryShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateGeometryShader(dxbcBytes, dxbcLength, NULL, &sh);

      if(sh != NULL)
      {
        id = ((WrappedID3D11Shader<ID3D11GeometryShader> *)sh)->GetResourceID();
      }
      else
      {
        errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        id = ResourceId();
      }
      return;
    }
    case ShaderStage::Pixel:
    {
      ID3D11PixelShader *sh = NULL;
      HRESULT hr = m_pDevice->CreatePixelShader(dxbcBytes, dxbcLength, NULL, &sh);

      if(sh != NULL)
      {
        id = ((WrappedID3D11Shader<ID3D11PixelShader> *)sh)->GetResourceID();
      }
      else
      {
        errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        id = ResourceId();
      }
      return;
    }
    case ShaderStage::Compute:
    {
      ID3D11ComputeShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateComputeShader(dxbcBytes, dxbcLength, NULL, &sh);

      if(sh != NULL)
      {
        id = ((WrappedID3D11Shader<ID3D11ComputeShader> *)sh)->GetResourceID();
      }
      else
      {
        errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        id = ResourceId();
      }
      return;
    }
    default: break;
  }

  errors = "Unexpected type in BuildShader!";
  id = ResourceId();
}

void D3D11Replay::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  ShaderCompileFlags debugCompileFlags = DXBC::EncodeFlags(
      DXBC::DecodeFlags(compileFlags) | D3DCOMPILE_DEBUG, DXBC::GetProfile(compileFlags));

  BuildShader(sourceEncoding, source, entry, debugCompileFlags, {}, type, id, errors);
}

void D3D11Replay::SetCustomShaderIncludes(const rdcarray<rdcstr> &directories)
{
  m_CustomShaderIncludes = directories;
}

void D3D11Replay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  BuildShader(sourceEncoding, source, entry, compileFlags, m_CustomShaderIncludes, type, id, errors);
}

bool D3D11Replay::RenderTexture(TextureDisplay cfg)
{
  return RenderTextureInternal(cfg, eTexDisplay_BlendAlpha);
}

void D3D11Replay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
  D3D11RenderStateTracker tracker(m_pImmediateContext);

  CheckerboardCBuffer pixelData = {};

  pixelData.PrimaryColor = ConvertSRGBToLinear(dark);
  pixelData.SecondaryColor = ConvertSRGBToLinear(light);
  pixelData.CheckerSquareDimension = 64.0f;

  ID3D11Buffer *psBuf = GetDebugManager()->MakeCBuffer(&pixelData, sizeof(pixelData));

  // can't just clear state because we need to keep things like render targets.
  {
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pImmediateContext->IASetInputLayout(NULL);

    m_pImmediateContext->VSSetShader(m_General.FullscreenVS, NULL, 0);

    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);

    m_pImmediateContext->RSSetState(m_General.RasterState);

    m_pImmediateContext->PSSetShader(m_General.CheckerboardPS, NULL, 0);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &psBuf);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_pImmediateContext->OMSetBlendState(NULL, factor, 0xffffffff);
    m_pImmediateContext->OMSetDepthStencilState(NULL, 0);

    m_pImmediateContext->Draw(4, 0);
  }
}

void D3D11Replay::RenderHighlightBox(float w, float h, float scale)
{
  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11Buffer *pconst = NULL;

  m_pImmediateContext->HSSetShader(NULL, NULL, 0);
  m_pImmediateContext->DSSetShader(NULL, NULL, 0);
  m_pImmediateContext->GSSetShader(NULL, NULL, 0);

  m_pImmediateContext->RSSetState(m_General.RasterScissorState);

  m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_pImmediateContext->IASetInputLayout(NULL);

  m_pImmediateContext->VSSetShader(m_General.FullscreenVS, NULL, 0);
  m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
  m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

  float black[] = {0.0f, 0.0f, 0.0f, 1.0f};
  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};

  // size of box
  LONG sz = LONG(scale);

  // top left, x and y
  LONG tlx = LONG(w / 2.0f + 0.5f);
  LONG tly = LONG(h / 2.0f + 0.5f);

  D3D11_RECT rect[4] = {
      {tlx, tly, tlx + 1, tly + sz},

      {tlx + sz, tly, tlx + sz + 1, tly + sz + 1},

      {tlx, tly, tlx + sz, tly + 1},

      {tlx, tly + sz, tlx + sz, tly + sz + 1},
  };

  // inner
  pconst = GetDebugManager()->MakeCBuffer(white, sizeof(white));

  // render the rects
  for(size_t i = 0; i < ARRAY_COUNT(rect); i++)
  {
    m_pImmediateContext->RSSetScissorRects(1, &rect[i]);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &pconst);
    m_pImmediateContext->Draw(4, 0);
  }

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

  // render the rects
  pconst = GetDebugManager()->MakeCBuffer(black, sizeof(black));

  for(size_t i = 0; i < ARRAY_COUNT(rect); i++)
  {
    m_pImmediateContext->RSSetScissorRects(1, &rect[i]);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &pconst);
    m_pImmediateContext->Draw(4, 0);
  }
}

void D3D11Replay::FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                       rdcstr entryPoint, uint32_t cbufSlot,
                                       rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return;

  const ShaderReflection &refl = it->second->GetDetails();

  if(cbufSlot >= (uint32_t)refl.constantBlocks.count())
  {
    RDCERR("Invalid cbuffer slot");
    return;
  }

  StandardFillCBufferVariables(refl.resourceId, refl.constantBlocks[cbufSlot].variables, outvars,
                               data);
}

uint32_t D3D11Replay::PickVertex(uint32_t eventId, int32_t width, int32_t height,
                                 const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  if(cfg.position.numIndices == 0)
    return ~0U;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

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

  ID3D11Buffer *vb = NULL, *ib = NULL;

  {
    auto it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.vertexResourceId);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      vb = it->second.m_Buffer;

    it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.indexResourceId);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      ib = it->second.m_Buffer;
  }

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers. In the case of VB
  // we also tightly pack and unpack the data. IB is upcast to R32 so it we can apply baseVertex
  // without risking overflow.

  uint32_t minIndex = 0;
  uint32_t maxIndex = cfg.position.numIndices;

  uint32_t idxclamp = 0;
  if(cfg.position.baseVertex < 0)
    idxclamp = uint32_t(-cfg.position.baseVertex);

  if(cfg.position.indexByteStride)
  {
    // resize up on demand
    if(m_VertexPick.PickIBBuf == NULL ||
       m_VertexPick.PickIBSize < cfg.position.numIndices * sizeof(uint32_t))
    {
      SAFE_RELEASE(m_VertexPick.PickIBBuf);
      SAFE_RELEASE(m_VertexPick.PickIBSRV);

      D3D11_BUFFER_DESC desc = {cfg.position.numIndices * sizeof(uint32_t),
                                D3D11_USAGE_DEFAULT,
                                D3D11_BIND_SHADER_RESOURCE,
                                0,
                                0,
                                0};

      m_VertexPick.PickIBSize = cfg.position.numIndices * sizeof(uint32_t);

      hr = m_pDevice->CreateBuffer(&desc, NULL, &m_VertexPick.PickIBBuf);

      if(FAILED(hr))
      {
        RDCERR("Failed to create PickIBBuf HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      sdesc.Format = DXGI_FORMAT_R32_UINT;
      sdesc.Buffer.FirstElement = 0;
      sdesc.Buffer.NumElements = cfg.position.numIndices;

      hr = m_pDevice->CreateShaderResourceView(m_VertexPick.PickIBBuf, &sdesc,
                                               &m_VertexPick.PickIBSRV);

      if(FAILED(hr))
      {
        SAFE_RELEASE(m_VertexPick.PickIBBuf);
        RDCERR("Failed to create PickIBSRV HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }
    }

    RDCASSERT(cfg.position.indexByteOffset < 0xffffffff);

    if(ib)
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

      m_pImmediateContext->UpdateSubresource(m_VertexPick.PickIBBuf, 0, &box, outidxs.data(), 0, 0);
    }
  }

  // unpack and linearise the data
  if(vb)
  {
    bytebuf oldData;
    GetDebugManager()->GetBufferData(vb, cfg.position.vertexByteOffset, 0, oldData);

    // clamp maxIndex to upper bound in case we got invalid indices or primitive restart indices
    maxIndex = RDCMIN(maxIndex, uint32_t(oldData.size() / RDCMAX(1U, cfg.position.vertexByteStride)));

    if(m_VertexPick.PickVBBuf == NULL || m_VertexPick.PickVBSize < (maxIndex + 1) * sizeof(Vec4f))
    {
      SAFE_RELEASE(m_VertexPick.PickVBBuf);
      SAFE_RELEASE(m_VertexPick.PickVBSRV);

      D3D11_BUFFER_DESC desc = {
          (maxIndex + 1) * sizeof(Vec4f), D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, 0};

      m_VertexPick.PickVBSize = (maxIndex + 1) * sizeof(Vec4f);

      hr = m_pDevice->CreateBuffer(&desc, NULL, &m_VertexPick.PickVBBuf);

      if(FAILED(hr))
      {
        RDCERR("Failed to create PickVBBuf HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      sdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      sdesc.Buffer.FirstElement = 0;
      sdesc.Buffer.NumElements = (maxIndex + 1);

      hr = m_pDevice->CreateShaderResourceView(m_VertexPick.PickVBBuf, &sdesc,
                                               &m_VertexPick.PickVBSRV);

      if(FAILED(hr))
      {
        SAFE_RELEASE(m_VertexPick.PickVBBuf);
        RDCERR("Failed to create PickVBSRV HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }
    }

    rdcarray<FloatVector> vbData;
    vbData.resize(maxIndex + 1);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    // the index buffer may refer to vertices past the start of the vertex buffer, so we can't just
    // conver the first N vertices we'll need.
    // Instead we grab min and max above, and convert every vertex in that range. This might
    // slightly over-estimate but not as bad as 0-max or the whole buffer.
    for(uint32_t idx = minIndex; idx <= maxIndex; idx++)
      vbData[idx] = HighlightCache::InterpretVertex(data, idx, cfg.position.vertexByteStride,
                                                    cfg.position.format, dataEnd, valid);

    D3D11_BOX box;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;
    box.left = 0;
    box.right = (maxIndex + 1) * sizeof(Vec4f);

    m_pImmediateContext->UpdateSubresource(m_VertexPick.PickVBBuf, 0, &box, vbData.data(),
                                           sizeof(Vec4f), sizeof(Vec4f));
  }

  ID3D11ShaderResourceView *srvs[2] = {m_VertexPick.PickIBSRV, m_VertexPick.PickVBSRV};

  ID3D11Buffer *buf = GetDebugManager()->MakeCBuffer(&cbuf, sizeof(cbuf));

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &buf);

  m_pImmediateContext->CSSetShaderResources(0, 2, srvs);

  UINT reset = 0;
  m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_VertexPick.PickResultUAV, &reset);

  m_pImmediateContext->CSSetShader(m_VertexPick.MeshPickCS, NULL, 0);

  m_pImmediateContext->Dispatch(cfg.position.numIndices / 1024 + 1, 1, 1);

  uint32_t numResults = GetDebugManager()->GetStructCount(m_VertexPick.PickResultUAV);

  if(numResults > 0)
  {
    bytebuf results;

    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        Vec3f intersectionPoint;
      };

      GetDebugManager()->GetBufferData(m_VertexPick.PickResultBuf, 0, 0, results);

      PickResult *pickResults = (PickResult *)&results[0];

      PickResult *closest = pickResults;

      // distance from raycast hit to nearest worldspace position of the mouse
      float closestPickDistance = (closest->intersectionPoint - rayPos).Length();

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)VertexPicking::MaxMeshPicks, numResults); i++)
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

      GetDebugManager()->GetBufferData(m_VertexPick.PickResultBuf, 0, 0, results);

      PickResult *pickResults = (PickResult *)&results[0];

      PickResult *closest = pickResults;

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)VertexPicking::MaxMeshPicks, numResults); i++)
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

void D3D11Replay::CreateCustomShaderTex(uint32_t w, uint32_t h)
{
  D3D11_TEXTURE2D_DESC texdesc;

  texdesc.ArraySize = 1;
  texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  texdesc.CPUAccessFlags = 0;
  texdesc.MipLevels = CalcNumMips((int)w, (int)h, 1);
  texdesc.MiscFlags = 0;
  texdesc.SampleDesc.Count = 1;
  texdesc.SampleDesc.Quality = 0;
  texdesc.Usage = D3D11_USAGE_DEFAULT;
  texdesc.Width = w;
  texdesc.Height = h;
  texdesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

  if(m_CustomShaderTex)
  {
    D3D11_TEXTURE2D_DESC customTexDesc;
    m_CustomShaderTex->GetDesc(&customTexDesc);

    if(customTexDesc.Width == w && customTexDesc.Height == h)
      return;

    SAFE_RELEASE(m_CustomShaderTex);
  }

  HRESULT hr = m_pDevice->CreateTexture2D(&texdesc, NULL, &m_CustomShaderTex);

  if(FAILED(hr))
  {
    RDCERR("Failed to create custom shader tex HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    m_CustomShaderResourceId = GetIDForDeviceChild(m_CustomShaderTex);
  }
}

ResourceId D3D11Replay::ApplyCustomShader(TextureDisplay &display)
{
  TextureShaderDetails details =
      GetDebugManager()->GetShaderDetails(display.resourceId, display.typeCast, false);

  CreateCustomShaderTex(details.texWidth, details.texHeight);

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11RenderTargetView *customRTV = NULL;

  {
    D3D11_RENDER_TARGET_VIEW_DESC desc;

    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = display.subresource.mip;

    WrappedID3D11Texture2D1 *wrapped = (WrappedID3D11Texture2D1 *)m_CustomShaderTex;
    HRESULT hr = m_pDevice->CreateRenderTargetView(wrapped, &desc, &customRTV);

    if(FAILED(hr))
    {
      RDCERR("Failed to create custom shader rtv HRESULT: %s", ToStr(hr).c_str());
      return m_CustomShaderResourceId;
    }
  }

  m_pImmediateContext->OMSetRenderTargets(1, &customRTV, NULL);

  float clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_pImmediateContext->ClearRenderTargetView(customRTV, clr);

  SAFE_RELEASE(customRTV);

  D3D11_VIEWPORT viewport;
  RDCEraseEl(viewport);

  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = (float)RDCMAX(1U, details.texWidth >> display.subresource.mip);
  viewport.Height = (float)RDCMAX(1U, details.texHeight >> display.subresource.mip);

  m_pImmediateContext->RSSetViewports(1, &viewport);

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = display.customShaderId;
  disp.resourceId = display.resourceId;
  disp.typeCast = display.typeCast;
  disp.backgroundColor = FloatVector(0, 0, 0, 1.0);
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.subresource = display.subresource;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = display.rangeMin;
  disp.rangeMax = display.rangeMax;
  disp.rawOutput = false;
  disp.scale = 1.0f;

  SetOutputDimensions(RDCMAX(1U, details.texWidth), RDCMAX(1U, details.texHeight));

  RenderTextureInternal(disp, eTexDisplay_BlendAlpha);

  return m_CustomShaderResourceId;
}

ResourceId D3D11Replay::CreateProxyTexture(const TextureDescription &templateTex)
{
  ResourceId ret;

  ID3D11Resource *resource = NULL;

  if(templateTex.dimension == 1)
  {
    ID3D11Texture1D *throwaway = NULL;
    D3D11_TEXTURE1D_DESC desc;

    desc.ArraySize = templateTex.arraysize;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.CPUAccessFlags = 0;
    desc.Format = GetTypelessFormat(MakeDXGIFormat(templateTex.format));
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = RDCMAX(1U, templateTex.width);

    if(IsDepthFormat(desc.Format))
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 1D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Texture1D *)throwaway)->GetResourceID();

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      WrappedID3D11Texture1D::m_TextureList[ret].m_Type = TEXDISPLAY_DEPTH_TARGET;
  }
  else if(templateTex.dimension == 2)
  {
    ID3D11Texture2D *throwaway = NULL;
    D3D11_TEXTURE2D_DESC desc;

    desc.ArraySize = templateTex.arraysize;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.CPUAccessFlags = 0;
    desc.Format = GetTypelessFormat(MakeDXGIFormat(templateTex.format));
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = RDCMAX(1U, templateTex.width);
    desc.Height = RDCMAX(1U, templateTex.height);
    desc.SampleDesc.Count = RDCMAX(1U, templateTex.msSamp);
    desc.SampleDesc.Quality = templateTex.msQual;

    if(IsDepthFormat(desc.Format))
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    if(templateTex.cubemap)
      desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

    if(IsBlockFormat(desc.Format))
    {
      desc.Width = AlignUp4(desc.Width);
      desc.Height = AlignUp4(desc.Height);
    }

    HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 2D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Texture2D1 *)throwaway)->GetResourceID();
    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      WrappedID3D11Texture2D1::m_TextureList[ret].m_Type = TEXDISPLAY_DEPTH_TARGET;
  }
  else if(templateTex.dimension == 3)
  {
    ID3D11Texture3D *throwaway = NULL;
    D3D11_TEXTURE3D_DESC desc;

    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.CPUAccessFlags = 0;
    desc.Format = GetTypelessFormat(MakeDXGIFormat(templateTex.format));
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = RDCMAX(1U, templateTex.width);
    desc.Height = RDCMAX(1U, templateTex.height);
    desc.Depth = RDCMAX(1U, templateTex.depth);

    HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 3D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Texture3D1 *)throwaway)->GetResourceID();
  }
  else
  {
    RDCERR("Invalid texture dimension: %d", templateTex.dimension);
  }

  if(resource)
    m_ProxyResources.push_back(resource);

  m_ProxyResourceOrigInfo[ret] = templateTex;

  return ret;
}

void D3D11Replay::SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data,
                                      size_t dataSize)
{
  if(texid == ResourceId())
    return;

  ID3D11DeviceContext *ctx = m_pDevice->GetImmediateContext()->GetReal();

  if(WrappedID3D11Texture1D::m_TextureList.find(texid) != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *tex =
        (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE1D_DESC desc;
    tex->GetDesc(&desc);

    uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);

    uint32_t mip = RDCMIN(sub.mip, mips - 1);
    uint32_t slice = RDCMIN(sub.slice, desc.ArraySize - 1);

    if(dataSize < GetByteSize(desc.Width, 1, 1, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), slice * mips + mip, NULL, data,
                           GetRowPitch(desc.Width, desc.Format, mip),
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip));
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(texid) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *tex =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    UINT width = desc.Width;
    UINT height = desc.Height;

    // for block formats we might have padded up to a multiple of four if the proxy texture wasn't
    // originally a multiple of 4x4 in the top mip. We need to use the original dimensions for
    // calculating expected data sizes and pitches
    auto it = m_ProxyResourceOrigInfo.find(texid);
    if(it == m_ProxyResourceOrigInfo.end())
    {
      RDCERR(
          "Expected proxy resource original info for texture being set with SetProxyTextureData");
    }
    else
    {
      width = it->second.width;
      height = it->second.height;
    }

    uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(width, height, 1);

    UINT sampleCount = RDCMAX(1U, desc.SampleDesc.Count);

    uint32_t mip = RDCMIN(sub.mip, mips - 1);
    uint32_t slice = RDCMIN(sub.slice, desc.ArraySize - 1);
    uint32_t sample = RDCMIN(sub.sample, sampleCount - 1);

    if(dataSize < GetByteSize(width, height, 1, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    if(sampleCount > 1)
    {
      D3D11_TEXTURE2D_DESC uploadDesc = desc;
      uploadDesc.MiscFlags = 0;
      uploadDesc.CPUAccessFlags = 0;
      uploadDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      uploadDesc.Usage = D3D11_USAGE_DEFAULT;
      uploadDesc.SampleDesc.Count = 1;
      uploadDesc.SampleDesc.Quality = 0;
      uploadDesc.ArraySize *= desc.SampleDesc.Count;

      UINT unpackedSlice = slice * desc.SampleDesc.Count + sample;

      // create an unwrapped texture to upload the data into a slice of
      ID3D11Texture2D *uploadTex = NULL;
      m_pDevice->GetReal()->CreateTexture2D(&uploadDesc, NULL, &uploadTex);

      ctx->UpdateSubresource(uploadTex, unpackedSlice, NULL, data,
                             GetRowPitch(width, desc.Format, mip),
                             GetByteSize(width, desc.Height, 1, desc.Format, mip));

      // copy that slice into MSAA sample
      GetDebugManager()->CopyArrayToTex2DMS(tex->GetReal(), uploadTex, unpackedSlice);

      uploadTex->Release();
    }
    else
    {
      ctx->UpdateSubresource(tex->GetReal(), slice * mips + mip, NULL, data,
                             GetRowPitch(width, desc.Format, mip),
                             GetByteSize(width, height, 1, desc.Format, mip));
    }
  }
  else if(WrappedID3D11Texture3D1::m_TextureList.find(texid) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *tex =
        (WrappedID3D11Texture3D1 *)WrappedID3D11Texture3D1::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE3D_DESC desc;
    tex->GetDesc(&desc);

    uint32_t mips =
        desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);

    uint32_t mip = RDCMIN(sub.mip, mips - 1);

    if(dataSize < GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), mip, NULL, data, GetRowPitch(desc.Width, desc.Format, mip),
                           GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
  }
  else
  {
    RDCERR("Invalid texture id passed to SetProxyTextureData");
  }
}

bool D3D11Replay::IsTextureSupported(const TextureDescription &tex)
{
  DXGI_FORMAT f = MakeDXGIFormat(tex.format);

  if(f == DXGI_FORMAT_UNKNOWN)
    return false;

  // if we get a typeless format back for a non-typeless format descriptor then we don't support
  // this component type.
  if(IsTypelessFormat(f) && tex.format.compType != CompType::Typeless)
    return false;

  if(!IsDepthFormat(f))
    f = GetTypelessFormat(f);
  else
    f = GetDepthTypedFormat(f);

  // CheckFormatSupport doesn't like returning MSAA support for typeless formats, if we're thinking
  // about MSAA ensure we query a typed format.
  if(tex.msSamp > 1)
    f = GetTypedFormat(f);

  UINT supp = 0;
  m_pDevice->CheckFormatSupport(f, &supp);

  if(tex.dimension == 1 && (supp & D3D11_FORMAT_SUPPORT_TEXTURE1D) == 0)
    return false;
  if(tex.dimension == 2 && (supp & D3D11_FORMAT_SUPPORT_TEXTURE2D) == 0)
    return false;
  if(tex.dimension == 3 && (supp & D3D11_FORMAT_SUPPORT_TEXTURE3D) == 0)
    return false;
  if(tex.msSamp > 1 && (supp & (D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD |
                                D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET)) == 0)
    return false;

  return true;
}

bool D3D11Replay::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

ResourceId D3D11Replay::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  ResourceId ret;

  ID3D11Resource *resource = NULL;

  {
    ID3D11Buffer *throwaway = NULL;
    D3D11_BUFFER_DESC desc;

    // D3D11_BIND_CONSTANT_BUFFER size must be 16-byte aligned.
    desc.ByteWidth = AlignUp16((UINT)templateBuf.length);
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER;
    desc.StructureByteStride = 0;

    HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create proxy buffer");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Buffer *)throwaway)->GetResourceID();
  }

  m_ProxyResources.push_back(resource);

  return ret;
}

void D3D11Replay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
  if(bufid == ResourceId())
    return;

  ID3D11DeviceContext *ctx = m_pDevice->GetImmediateContext()->GetReal();

  if(WrappedID3D11Buffer::m_BufferList.find(bufid) != WrappedID3D11Buffer::m_BufferList.end())
  {
    WrappedID3D11Buffer *buf =
        (WrappedID3D11Buffer *)WrappedID3D11Buffer::m_BufferList[bufid].m_Buffer;

    D3D11_BUFFER_DESC desc;
    buf->GetDesc(&desc);

    if(AlignUp16(dataSize) < desc.ByteWidth)
    {
      RDCERR("Insufficient data provided to SetProxyBufferData");
      return;
    }

    ctx->UpdateSubresource(buf->GetReal(), 0, NULL, data, (UINT)dataSize, (UINT)dataSize);
  }
  else
  {
    RDCERR("Invalid buffer id passed to SetProxyBufferData");
  }
}

ID3DDevice *GetD3D11DeviceIfAlloc(IUnknown *dev);

RDResult D3D11_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D11 replay device");

  IntelCounters::Load();

  HMODULE lib = NULL;
  lib = LoadLibraryA("d3d11.dll");
  if(lib == NULL)
  {
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Failed to load d3d11.dll");
  }

  PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN CreateDeviceAndSwapChainPtr =
      (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(lib, "D3D11CreateDeviceAndSwapChain");

  RealD3D11CreateFunction CreateDeviceAndSwapChain = CreateDeviceAndSwapChainPtr;

  lib = LoadLibraryA("dxgi.dll");
  if(lib == NULL)
  {
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Failed to load dxgi.dll");
  }

  if(GetD3DCompiler() == NULL)
  {
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Failed to load d3dcompiler_??.dll");
  }

  D3D11InitParams initParams;

  uint64_t ver = D3D11InitParams::CurrentVersion;

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!D3D11InitParams::IsSupportedVersion(ver))
    {
      RETURN_ERROR_RESULT(ResultCode::APIIncompatibleVersion,
                          "D3D11 capture is incompatible version %llu, newest supported by this "
                          "build of RenderDoc is %llu",
                          ver, D3D11InitParams::CurrentVersion);
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    ser.SetVersion(ver);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted,
                          "Expected to get a DriverInit chunk, instead got %u", chunk);
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      return ser.GetError();
    }

    if(initParams.AdapterDesc.Description[0])
      RDCLOG("Capture was created on %s / %ls",
             ToStr(GPUVendorFromPCIVendor(initParams.AdapterDesc.VendorId)).c_str(),
             initParams.AdapterDesc.Description);
  }

  IDXGIFactory *factory = NULL;

  // first try to use DXGI 1.1 as MSDN has vague warnings about trying to 'mix' DXGI 1.0 and 1.1
  {
    typedef HRESULT(WINAPI * PFN_CREATE_DXGI_FACTORY)(REFIID, void **);

    PFN_CREATE_DXGI_FACTORY createFunc =
        (PFN_CREATE_DXGI_FACTORY)GetProcAddress(GetModuleHandleA("dxgi.dll"), "CreateDXGIFactory1");

    if(createFunc)
    {
      IDXGIFactory1 *tmpFactory = NULL;
      HRESULT hr = createFunc(__uuidof(IDXGIFactory1), (void **)&tmpFactory);

      if(SUCCEEDED(hr) && tmpFactory)
      {
        factory = tmpFactory;
      }
      else
      {
        RDCERR("Error creating IDXGIFactory1: %s", ToStr(hr).c_str());
      }
    }
    else
    {
      RDCWARN("Couldn't get CreateDXGIFactory1");
    }
  }

  if(!factory)
  {
    RDCWARN("Couldn't create IDXGIFactory1, falling back to CreateDXGIFactory");

    typedef HRESULT(WINAPI * PFN_CREATE_DXGI_FACTORY)(REFIID, void **);

    PFN_CREATE_DXGI_FACTORY createFunc =
        (PFN_CREATE_DXGI_FACTORY)GetProcAddress(GetModuleHandleA("dxgi.dll"), "CreateDXGIFactory");

    if(createFunc)
    {
      HRESULT hr = createFunc(__uuidof(IDXGIFactory), (void **)&factory);

      if(FAILED(hr) || !factory)
      {
        SAFE_RELEASE(factory);
        RETURN_ERROR_RESULT(ResultCode::APIInitFailed,
                            "Couldn't create DXGI factory from CreateDXGIFactory: %s",
                            ToStr(hr).c_str());
      }
    }
    else
    {
      RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Couldn't find CreateDXGIFactory in dxgi.dll");
    }
  }

  // we won't use the SDKVersion from the init params, so warn if it's different.
  if(initParams.SDKVersion != D3D11_SDK_VERSION)
  {
    RDCWARN(
        "Capture file used a different SDK version %lu from replay app %lu. Results may be "
        "undefined",
        initParams.SDKVersion, D3D11_SDK_VERSION);
  }

  INVAPID3DDevice *nvapiDev = NULL;
  IAGSD3DDevice *agsDev = NULL;

  if(initParams.VendorExtensions == GPUVendor::nVidia)
  {
    nvapiDev = InitialiseNVAPIReplay();

    if(!nvapiDev)
    {
      RETURN_ERROR_RESULT(
          ResultCode::APIHardwareUnsupported,
          "Capture requires nvapi to replay, but it's not available or can't be initialised");
    }
  }
  else if(initParams.VendorExtensions == GPUVendor::AMD)
  {
    agsDev = InitialiseAGSReplay(~0U, initParams.VendorUAV);

    if(!agsDev)
    {
      RETURN_ERROR_RESULT(
          ResultCode::APIHardwareUnsupported,
          "Capture requires ags to replay, but it's not available or can't be initialised");
    }

    CreateDeviceAndSwapChain =
        [agsDev](IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                 CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
                 CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
                 ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
                 ID3D11DeviceContext **ppImmediateContext) {
          return agsDev->CreateD3D11(pAdapter, DriverType, Software, Flags, pFeatureLevels,
                                     FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain,
                                     ppDevice, pFeatureLevel, ppImmediateContext);
        };
  }
  else if(initParams.VendorExtensions != GPUVendor::Unknown)
  {
    RETURN_ERROR_RESULT(
        ResultCode::APIInitFailed,
        "Capture requires vendor extensions by %s to replay, but no support for that is "
        "available.",
        ToStr(initParams.VendorExtensions).c_str());
  }

  IDXGIAdapter *adapter = NULL;
  ID3D11Device *device = NULL;
  HRESULT hr = E_FAIL;

  const bool isProxy = (rdc == NULL);

  // This says whether we're using warp at all
  bool useWarp = false;
  // This says if we've fallen back to warp after failing to find anything better (hence degraded
  // support because using warp was not deliberate)
  bool warpFallback = false;

  if(!isProxy)
    ChooseBestMatchingAdapter(GraphicsAPI::D3D11, factory, initParams.AdapterDesc, opts, &useWarp,
                              &adapter);

  if(useWarp)
    SAFE_RELEASE(adapter);

  DXGI_ADAPTER_DESC chosenAdapter = {};
  if(adapter)
    adapter->GetDesc(&chosenAdapter);

  // check that the adapter supports at least feature level 11_0
  D3D_FEATURE_LEVEL maxFeatureLevel = D3D_FEATURE_LEVEL_9_1;

  // check for feature level 11 support - passing NULL feature level array implicitly checks for
  // 11_0 before others
  {
    D3D_DRIVER_TYPE type = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    if(useWarp)
      type = D3D_DRIVER_TYPE_WARP;

    ID3D11Device *dev = NULL;
    hr = CreateDeviceAndSwapChain(adapter, type, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL,
                                  &dev, &maxFeatureLevel, NULL);
    SAFE_RELEASE(dev);

    // if the device doesn't support 11_0, we can't use it.
    if(SUCCEEDED(hr) && maxFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
      // If we were using a specific adapter try falling back to default selection in case the
      // adapter chosen isn't the default one in the system
      if(adapter)
      {
        RDCWARN(
            "Selected %ls for replay, but it does not support D3D_FEATURE_LEVEL_11_0. "
            "Falling back to NULL adapter",
            chosenAdapter.Description);

        SAFE_RELEASE(adapter);

        hr = CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                                      D3D11_SDK_VERSION, NULL, NULL, &dev, &maxFeatureLevel, NULL);
        SAFE_RELEASE(dev);
      }

      // if it's still not 11_0, assume there is no 11_0 GPU and fall back to WARP.
      if(SUCCEEDED(hr) && maxFeatureLevel < D3D_FEATURE_LEVEL_11_0)
      {
        RDCWARN(
            "Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 "
            "availability - falling back to WARP rasterizer");
        useWarp = warpFallback = true;
      }
    }
  }

  UINT flags = initParams.Flags;

  // we control the debug flag ourselves
  flags &= ~D3D11_CREATE_DEVICE_DEBUG;

#if ENABLED(RDOC_DEVEL)
  // in development builds, always enable debug layer during replay
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#else
  // in release builds, only enable it if forced by replay options
  if(opts.apiValidation)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
  else
    flags &= ~D3D11_CREATE_DEVICE_DEBUG;
#endif

  // we should now be set up to try creating feature level 11 devices either with a selected
  // adapter, a NULL (any) adapter, or WARP.

  // The retry paths are like this:
  //
  // try first with the adapter, then if we were using a specific adapter try without, then last try
  // with WARP
  //      within that, try to create a device at descending feature levels
  //           for each attempt, if the debug layer is enabled try without it

  for(int adapterPass = 0; adapterPass < 3; adapterPass++)
  {
    // if we don't have an adapter we're trying, just skip straight to pass 1
    if(adapterPass == 0 && adapter == NULL)
      continue;

    // if we're already falling back (or deliberately using) WARP, skip to pass 2
    if(adapterPass == 1 && useWarp)
      continue;

    // don't use the adapter except in the first pass
    if(adapterPass > 0)
      SAFE_RELEASE(adapter);

    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_UNKNOWN;

    // if we're on the WARP pass, use the WARP driver type, otherwise use the driver type implied by
    // whether or not we have an adapter specified
    if(adapterPass == 2)
      driverType = D3D_DRIVER_TYPE_WARP;
    else
      driverType = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    // try first to create with the whole array, then with all but the top, then all but the next
    // top, etc
    for(int featureLevelPass = 0; featureLevelPass < ARRAY_COUNT(featureLevels); featureLevelPass++)
    {
      D3D_FEATURE_LEVEL *featureLevelSubset = featureLevels + featureLevelPass;
      UINT numFeatureLevels = ARRAY_COUNT(featureLevels) - featureLevelPass;

      RDCDEBUG(
          "Creating D3D11 replay device, %ls adapter, driver type %s, flags %x, %d feature levels "
          "(first %s)",
          adapter ? chosenAdapter.Description : L"NULL", ToStr(driverType).c_str(), flags,
          numFeatureLevels, ToStr(featureLevelSubset[0]).c_str());

      hr = CreateDeviceAndSwapChain(adapter, driverType, NULL, flags, featureLevelSubset,
                                    numFeatureLevels, D3D11_SDK_VERSION, NULL, NULL, &device, NULL,
                                    NULL);

      if(SUCCEEDED(hr) && device)
        break;

      SAFE_RELEASE(device);

// in release try to fall back to a non-debug device
#if ENABLED(RDOC_RELEASE)
      if(flags & D3D11_CREATE_DEVICE_DEBUG)
      {
        UINT noDebugFlags = flags & ~D3D11_CREATE_DEVICE_DEBUG;

        HRESULT hr2 = CreateDeviceAndSwapChain(adapter, driverType, NULL, noDebugFlags,
                                               featureLevelSubset, numFeatureLevels,
                                               D3D11_SDK_VERSION, NULL, NULL, &device, NULL, NULL);

        // if we can manage to create it without debug active, do that since it's extremely unlikely
        // that any other configuration will have better luck with debug active.
        if(SUCCEEDED(hr2) && device)
        {
          RDCLOG(
              "Device creation failed with validation active - check that you have the "
              "SDK installed or Windows feature enabled to get the D3D debug layers.");

          hr = hr2;

          break;
        }
      }
#endif

      RDCLOG("Device creation failed, %s", ToStr(hr).c_str());
    }

    if(SUCCEEDED(hr) && device)
      break;
  }

  SAFE_RELEASE(adapter);

  if(SUCCEEDED(hr) && device)
  {
    if(nvapiDev)
    {
      BOOL ok = nvapiDev->SetReal(device);
      if(!ok)
      {
        SAFE_RELEASE(device);
        SAFE_RELEASE(nvapiDev);
        SAFE_RELEASE(factory);
        RETURN_ERROR_RESULT(
            ResultCode::APIHardwareUnsupported,
            "This capture needs nvapi extensions to replay, but device selected for replay can't "
            "support nvapi extensions");
      }
    }

    if(agsDev)
    {
      if(!agsDev->ExtensionsSupported())
      {
        SAFE_RELEASE(device);
        SAFE_RELEASE(nvapiDev);
        SAFE_RELEASE(factory);
        RETURN_ERROR_RESULT(
            ResultCode::APIHardwareUnsupported,
            "This capture needs AGS extensions to replay, but device selected for replay can't "
            "support nvapi extensions");
      }
    }

    WrappedID3D11Device *wrappedDev = new WrappedID3D11Device(device, initParams);
    wrappedDev->SetInitParams(initParams, ver, opts, nvapiDev, agsDev);

    if(!isProxy)
      RDCLOG("Created device.");
    D3D11Replay *replay = wrappedDev->GetReplay();

    replay->SetProxy(isProxy, warpFallback);
    replay->CreateResources(factory);
    if(warpFallback)
    {
      wrappedDev->AddDebugMessage(
          MessageCategory::Initialization, MessageSeverity::High, MessageSource::RuntimeWarning,
          "Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 "
          "availability - falling back to WARP rasterizer.\n"
          "Performance and usability will be significantly degraded.");
    }

    *driver = (IReplayDriver *)replay;
    return ResultCode::Succeeded;
  }

  SAFE_RELEASE(factory);

  rdcstr error = "Couldn't create any compatible d3d11 device.";

  if(flags & D3D11_CREATE_DEVICE_DEBUG)
    error +=
        "\n\nDevelopment RenderDoc builds require D3D debug layers available, "
        "ensure you have the windows SDK or windows feature needed.";

  RETURN_ERROR_RESULT(ResultCode::APIHardwareUnsupported, "%s", error.c_str());
}

static DriverRegistration D3D11DriverRegistration(RDCDriver::D3D11, &D3D11_CreateReplayDevice);

RDResult D3D11_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedID3D11Device device(NULL, D3D11InitParams());

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  RDResult result = device.ReadLogInitialisation(rdc, true);

  if(result == ResultCode::Succeeded)
    device.GetStructuredFile()->Swap(output);

  return result;
}

static StructuredProcessRegistration D3D11ProcessRegistration(RDCDriver::D3D11,
                                                              &D3D11_ProcessStructured);
