/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "driver/dx/official/d3dcompiler.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/intel/intel_counters.h"
#include "driver/ihv/nv/nv_counters.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"
#include "d3d11_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

static const char *DXBCDisassemblyTarget = "DXBC";

D3D11Replay::D3D11Replay()
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D11Replay));

  m_pDevice = NULL;
  m_Proxy = false;
  m_WARP = false;

  m_HighlightCache.driver = this;

  RDCEraseEl(m_DriverInfo);
}

D3D11Replay::~D3D11Replay()
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void D3D11Replay::Shutdown()
{
  for(size_t i = 0; i < m_ProxyResources.size(); i++)
    m_ProxyResources[i]->Release();
  m_ProxyResources.clear();

  m_pDevice->Release();
}

void D3D11Replay::SetDevice(WrappedID3D11Device *d)
{
  m_pDevice = d;
  m_pImmediateContext = d->GetImmediateContext();
}

void D3D11Replay::CreateResources()
{
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

      std::string descString = GetDriverVersion(desc);
      descString.resize(RDCMIN(descString.size(), ARRAY_COUNT(m_DriverInfo.version) - 1));
      memcpy(m_DriverInfo.version, descString.c_str(), descString.size());

      RDCLOG("Running replay on %s / %s", ToStr(m_DriverInfo.vendor).c_str(), m_DriverInfo.version);

      if(m_WARP)
        m_DriverInfo.vendor = GPUVendor::Software;

      hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&m_pFactory);

      SAFE_RELEASE(pDXGIDevice);
      SAFE_RELEASE(pDXGIAdapter);

      if(FAILED(hr))
      {
        RDCERR("Couldn't get DXGI factory from DXGI adapter");
      }
    }
  }

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

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.7f);

  m_Histogram.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.8f);

  m_PixelHistory.Init(m_pDevice);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.9f);

  AMDCounters *countersAMD = NULL;
  NVCounters *countersNV = NULL;
  IntelCounters *countersIntel = NULL;

  if(m_DriverInfo.vendor == GPUVendor::AMD)
  {
    RDCLOG("AMD GPU detected - trying to initialise AMD counters");
    countersAMD = new AMDCounters();
  }
  else if(m_DriverInfo.vendor == GPUVendor::nVidia)
  {
    RDCLOG("nVidia GPU detected - trying to initialise nVidia counters");
    countersNV = new NVCounters();
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

  ID3D11Device *d3dDevice = m_pDevice->GetReal();

  if(countersAMD && countersAMD->Init(AMDCounters::ApiType::Dx11, (void *)d3dDevice))
  {
    m_pAMDCounters = countersAMD;
  }
  else
  {
    delete countersAMD;
    m_pAMDCounters = NULL;
  }

  if(countersNV && countersNV->Init(d3dDevice))
  {
    m_pNVCounters = countersNV;
  }
  else
  {
    delete countersNV;
    m_pNVCounters = NULL;
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
  m_Histogram.Release();
  m_PixelHistory.Release();

  SAFE_DELETE(m_pAMDCounters);
  SAFE_DELETE(m_pNVCounters);
  SAFE_DELETE(m_pIntelCounters);

  ShutdownStreamOut();
  ClearPostVSCache();

  SAFE_RELEASE(m_pFactory);
}

TextureDescription D3D11Replay::GetTexture(ResourceId id)
{
  TextureDescription tex = {};
  tex.resourceId = ResourceId();

  auto it1D = WrappedID3D11Texture1D::m_TextureList.find(id);
  if(it1D != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *d3dtex = (WrappedID3D11Texture1D *)it1D->second.m_Texture;

    std::string str = GetDebugName(d3dtex);

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

    std::string str = GetDebugName(d3dtex);

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

    return tex;
  }

  auto it3D = WrappedID3D11Texture3D1::m_TextureList.find(id);
  if(it3D != WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *d3dtex = (WrappedID3D11Texture3D1 *)it3D->second.m_Texture;

    std::string str = GetDebugName(d3dtex);

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

  RDCERR("Unrecognised/unknown texture %llu", id);

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

rdcarray<ShaderEntryPoint> D3D11Replay::GetShaderEntryPoints(ResourceId shader)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return {};

  ShaderReflection &ret = it->second->GetDetails();

  return {{"main", ret.stage}};
}

ShaderReflection *D3D11Replay::GetShader(ResourceId shader, ShaderEntryPoint entry)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return NULL;

  ShaderReflection &ret = it->second->GetDetails();

  return &ret;
}

std::vector<std::string> D3D11Replay::GetDisassemblyTargets()
{
  std::vector<std::string> ret;

  // DXBC is always first
  ret.insert(ret.begin(), DXBCDisassemblyTarget);

  return ret;
}

std::string D3D11Replay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                           const std::string &target)
{
  auto it =
      WrappedShader::m_ShaderList.find(m_pDevice->GetResourceManager()->GetLiveID(refl->resourceId));

  if(it == WrappedShader::m_ShaderList.end())
    return "; Invalid Shader Specified";

  DXBC::DXBCFile *dxbc = it->second->GetDXBC();

  if(target == DXBCDisassemblyTarget || target.empty())
    return dxbc->GetDisassembly();

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

FrameRecord D3D11Replay::GetFrameRecord()
{
  return m_pDevice->GetFrameRecord();
}

std::vector<EventUsage> D3D11Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetImmediateContext()->GetUsage(id);
}

std::vector<DebugMessage> D3D11Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

APIProperties D3D11Replay::GetAPIProperties()
{
  APIProperties ret = m_pDevice->APIProps;

  ret.pipelineType = GraphicsAPI::D3D11;
  ret.localRenderer = GraphicsAPI::D3D11;
  ret.vendor = m_DriverInfo.vendor;
  ret.degraded = m_WARP;
  ret.shadersMutable = false;

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

const std::vector<ResourceDescription> &D3D11Replay::GetResources()
{
  return m_Resources;
}

std::vector<ResourceId> D3D11Replay::GetBuffers()
{
  std::vector<ResourceId> ret;

  ret.reserve(WrappedID3D11Buffer::m_BufferList.size());

  for(auto it = WrappedID3D11Buffer::m_BufferList.begin();
      it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
  {
    // skip buffers that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  return ret;
}

BufferDescription D3D11Replay::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};
  ret.resourceId = ResourceId();

  auto it = WrappedID3D11Buffer::m_BufferList.find(id);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
    return ret;

  WrappedID3D11Buffer *d3dbuf = it->second.m_Buffer;

  std::string str = GetDebugName(d3dbuf);

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

std::vector<ResourceId> D3D11Replay::GetTextures()
{
  std::vector<ResourceId> ret;

  ret.reserve(WrappedID3D11Texture1D::m_TextureList.size() +
              WrappedID3D11Texture2D1::m_TextureList.size() +
              WrappedID3D11Texture3D1::m_TextureList.size());

  for(auto it = WrappedID3D11Texture1D::m_TextureList.begin();
      it != WrappedID3D11Texture1D::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  for(auto it = WrappedID3D11Texture2D1::m_TextureList.begin();
      it != WrappedID3D11Texture2D1::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  for(auto it = WrappedID3D11Texture3D1::m_TextureList.begin();
      it != WrappedID3D11Texture3D1::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  return ret;
}

void D3D11Replay::SavePipelineState(uint32_t eventId)
{
  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

  D3D11Pipe::State &ret = m_CurPipelineState;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  ret.inputAssembly.bytecode = NULL;

  if(rs->IA.Layout)
  {
    const std::vector<D3D11_INPUT_ELEMENT_DESC> &vec = m_pDevice->GetLayoutDesc(rs->IA.Layout);

    ResourceId layoutId = GetIDForResource(rs->IA.Layout);

    ret.inputAssembly.resourceId = rm->GetOriginalID(layoutId);
    ret.inputAssembly.bytecode = GetShader(layoutId, ShaderEntryPoint());

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

    vb.resourceId = rm->GetOriginalID(GetIDForResource(rs->IA.VBs[i]));
    vb.byteOffset = rs->IA.Offsets[i];
    vb.byteStride = rs->IA.Strides[i];
  }

  ret.inputAssembly.indexBuffer.resourceId = rm->GetOriginalID(GetIDForResource(rs->IA.IndexBuffer));
  ret.inputAssembly.indexBuffer.byteOffset = rs->IA.IndexOffset;

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  {
    D3D11Pipe::Shader *dstArr[] = {&ret.vertexShader,   &ret.hullShader,  &ret.domainShader,
                                   &ret.geometryShader, &ret.pixelShader, &ret.computeShader};
    const D3D11RenderState::Shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS,
                                                &rs->GS, &rs->PS, &rs->CS};

    const char *stageNames[] = {"Vertex", "Hull", "Domain", "Geometry", "Pixel", "Compute"};

    for(size_t stage = 0; stage < 6; stage++)
    {
      D3D11Pipe::Shader &dst = *dstArr[stage];
      const D3D11RenderState::Shader &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      ResourceId id = GetIDForResource(src.Object);

      WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)src.Object;

      ShaderReflection *refl = NULL;

      if(shad != NULL)
      {
        refl = &shad->GetDetails();
        dst.bindpointMapping = shad->GetMapping();
      }
      else
      {
        dst.bindpointMapping = ShaderBindpointMapping();
      }

      dst.resourceId = rm->GetOriginalID(id);
      dst.reflection = refl;

      dst.constantBuffers.resize(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; s++)
      {
        dst.constantBuffers[s].resourceId =
            rm->GetOriginalID(GetIDForResource(src.ConstantBuffers[s]));
        dst.constantBuffers[s].vecOffset = src.CBOffsets[s];
        dst.constantBuffers[s].vecCount = src.CBCounts[s];
      }

      dst.samplers.resize(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; s++)
      {
        D3D11Pipe::Sampler &samp = dst.samplers[s];

        samp.resourceId = rm->GetOriginalID(GetIDForResource(src.Samplers[s]));

        if(samp.resourceId != ResourceId())
        {
          D3D11_SAMPLER_DESC desc;
          src.Samplers[s]->GetDesc(&desc);

          samp.addressU = MakeAddressMode(desc.AddressU);
          samp.addressV = MakeAddressMode(desc.AddressV);
          samp.addressW = MakeAddressMode(desc.AddressW);

          memcpy(samp.borderColor, desc.BorderColor, sizeof(FLOAT) * 4);

          samp.compareFunction = MakeCompareFunc(desc.ComparisonFunc);
          samp.filter = MakeFilter(desc.Filter);
          samp.maxAnisotropy = 0;
          if(samp.filter.mip == FilterMode::Anisotropic)
            samp.maxAnisotropy = desc.MaxAnisotropy;
          samp.maxLOD = desc.MaxLOD;
          samp.minLOD = desc.MinLOD;
          samp.mipLODBias = desc.MipLODBias;
        }
      }

      dst.srvs.resize(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
      {
        D3D11Pipe::View &view = dst.srvs[s];

        view.viewResourceId = rm->GetOriginalID(GetIDForResource(src.SRVs[s]));

        if(view.viewResourceId != ResourceId())
        {
          D3D11_SHADER_RESOURCE_VIEW_DESC desc;
          src.SRVs[s]->GetDesc(&desc);

          view.viewFormat = MakeResourceFormat(desc.Format);

          ID3D11Resource *res = NULL;
          src.SRVs[s]->GetResource(&res);

          view.structured = false;
          view.bufferStructCount = 0;

          view.elementByteSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

          view.type = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
          {
            view.firstElement = desc.Buffer.FirstElement;
            view.numElements = desc.Buffer.NumElements;

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

            if(view.structured)
              view.elementByteSize = bufdesc.StructureByteStride;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
          {
            view.firstElement = desc.BufferEx.FirstElement;
            view.numElements = desc.BufferEx.NumElements;
            view.bufferFlags = D3DBufferViewFlags(desc.BufferEx.Flags);

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

            if(view.structured)
              view.elementByteSize = bufdesc.StructureByteStride;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
          {
            view.firstMip = desc.Texture1D.MostDetailedMip;
            view.numMips = desc.Texture1D.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
          {
            view.numSlices = desc.Texture1DArray.ArraySize;
            view.firstSlice = desc.Texture1DArray.FirstArraySlice;
            view.firstMip = desc.Texture1DArray.MostDetailedMip;
            view.numMips = desc.Texture1DArray.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
          {
            view.firstMip = desc.Texture2D.MostDetailedMip;
            view.numMips = desc.Texture2D.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
          {
            view.numSlices = desc.Texture2DArray.ArraySize;
            view.firstSlice = desc.Texture2DArray.FirstArraySlice;
            view.firstMip = desc.Texture2DArray.MostDetailedMip;
            view.numMips = desc.Texture2DArray.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
          {
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
          {
            view.numSlices = desc.Texture2DArray.ArraySize;
            view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D)
          {
            view.firstMip = desc.Texture3D.MostDetailedMip;
            view.numMips = desc.Texture3D.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
          {
            view.numSlices = 6;
            view.firstMip = desc.TextureCube.MostDetailedMip;
            view.numMips = desc.TextureCube.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
          {
            view.numSlices = desc.TextureCubeArray.NumCubes * 6;
            view.firstSlice = desc.TextureCubeArray.First2DArrayFace;
            view.firstMip = desc.TextureCubeArray.MostDetailedMip;
            view.numMips = desc.TextureCubeArray.MipLevels;
          }

          SAFE_RELEASE(res);
        }
        else
        {
          view.resourceResourceId = ResourceId();
        }
      }

      dst.uavs.resize(D3D11_1_UAV_SLOT_COUNT);
      for(size_t s = 0; dst.stage == ShaderStage::Compute && s < D3D11_1_UAV_SLOT_COUNT; s++)
      {
        D3D11Pipe::View &view = dst.uavs[s];

        view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->CSUAVs[s]));

        if(view.viewResourceId != ResourceId())
        {
          D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
          rs->CSUAVs[s]->GetDesc(&desc);

          ID3D11Resource *res = NULL;
          rs->CSUAVs[s]->GetResource(&res);

          view.structured = false;
          view.bufferStructCount = 0;

          view.elementByteSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
             (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)))
          {
            view.bufferStructCount = GetDebugManager()->GetStructCount(rs->CSUAVs[s]);
          }

          view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

          view.viewFormat = MakeResourceFormat(desc.Format);
          view.type = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
          {
            view.firstElement = desc.Buffer.FirstElement;
            view.numElements = desc.Buffer.NumElements;
            view.bufferFlags = D3DBufferViewFlags(desc.Buffer.Flags);

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

            if(view.structured)
              view.elementByteSize = bufdesc.StructureByteStride;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
          {
            view.firstMip = desc.Texture1D.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
          {
            view.numSlices = desc.Texture1DArray.ArraySize;
            view.firstSlice = desc.Texture1DArray.FirstArraySlice;
            view.firstMip = desc.Texture1DArray.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
          {
            view.firstMip = desc.Texture2D.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
          {
            view.numSlices = desc.Texture2DArray.ArraySize;
            view.firstSlice = desc.Texture2DArray.FirstArraySlice;
            view.firstMip = desc.Texture2DArray.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
          {
            view.numSlices = desc.Texture3D.WSize;
            view.firstSlice = desc.Texture3D.FirstWSlice;
            view.firstMip = desc.Texture3D.MipSlice;
            view.numMips = 1;
          }

          SAFE_RELEASE(res);
        }
        else
        {
          view.resourceResourceId = ResourceId();
        }
      }

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
      ret.streamOut.outputs[s].resourceId = rm->GetOriginalID(GetIDForResource(rs->SO.Buffers[s]));
      ret.streamOut.outputs[s].byteOffset = rs->SO.Offsets[s];
    }
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

      ret.rasterizer.state.resourceId = rm->GetOriginalID(GetIDForResource(rs->RS.State));
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
                                           rs->RS.Scissors[i].bottom - rs->RS.Scissors[i].top, true);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.rasterizer.scissors[i] = Scissor(0, 0, 0, 0, false);

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
      D3D11Pipe::View &view = ret.outputMerger.renderTargets[i];

      view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->OM.RenderTargets[i]));

      if(view.viewResourceId != ResourceId())
      {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        rs->OM.RenderTargets[i]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.RenderTargets[i]->GetResource(&res);

        view.structured = false;
        view.bufferStructCount = 0;
        view.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

        view.viewFormat = MakeResourceFormat(desc.Format);
        view.type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_RTV_DIMENSION_BUFFER)
        {
          view.firstElement = desc.Buffer.FirstElement;
          view.numElements = desc.Buffer.NumElements;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
        {
          view.firstMip = desc.Texture1D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
        {
          view.numSlices = desc.Texture1DArray.ArraySize;
          view.firstSlice = desc.Texture1DArray.FirstArraySlice;
          view.firstMip = desc.Texture1DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
        {
          view.firstMip = desc.Texture2D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
        {
          view.numSlices = desc.Texture2DArray.ArraySize;
          view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          view.firstMip = desc.Texture2DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE3D)
        {
          view.numSlices = desc.Texture3D.WSize;
          view.firstSlice = desc.Texture3D.FirstWSlice;
          view.firstMip = desc.Texture3D.MipSlice;
          view.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
      else
      {
        view.resourceResourceId = ResourceId();
      }
    }

    ret.outputMerger.uavStartSlot = rs->OM.UAVStartSlot;

    ret.outputMerger.uavs.resize(D3D11_1_UAV_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_1_UAV_SLOT_COUNT; s++)
    {
      D3D11Pipe::View view;

      view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->OM.UAVs[s]));

      if(view.viewResourceId != ResourceId())
      {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
        rs->OM.UAVs[s]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.UAVs[s]->GetResource(&res);

        view.structured = false;
        view.bufferStructCount = 0;
        view.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
           (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)))
        {
          view.bufferStructCount = GetDebugManager()->GetStructCount(rs->OM.UAVs[s]);
        }

        view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

        view.viewFormat = MakeResourceFormat(desc.Format);
        view.type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
        {
          view.firstElement = desc.Buffer.FirstElement;
          view.numElements = desc.Buffer.NumElements;
          view.bufferFlags = D3DBufferViewFlags(desc.Buffer.Flags);

          D3D11_BUFFER_DESC bufdesc;
          ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

          view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

          if(view.structured)
            view.elementByteSize = bufdesc.StructureByteStride;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
        {
          view.firstMip = desc.Texture1D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
        {
          view.numSlices = desc.Texture1DArray.ArraySize;
          view.firstSlice = desc.Texture1DArray.FirstArraySlice;
          view.firstMip = desc.Texture1DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
        {
          view.firstMip = desc.Texture2D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
        {
          view.numSlices = desc.Texture2DArray.ArraySize;
          view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          view.firstMip = desc.Texture2DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
        {
          view.numSlices = desc.Texture3D.WSize;
          view.firstSlice = desc.Texture3D.FirstWSlice;
          view.firstMip = desc.Texture3D.MipSlice;
          view.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
      else
      {
        view.resourceResourceId = ResourceId();
      }

      ret.outputMerger.uavs[s] = view;
    }

    {
      D3D11Pipe::View &view = ret.outputMerger.depthTarget;

      view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->OM.DepthView));

      if(view.viewResourceId != ResourceId())
      {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        rs->OM.DepthView->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.DepthView->GetResource(&res);

        view.structured = false;
        view.bufferStructCount = 0;
        view.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        ret.outputMerger.depthReadOnly = false;
        ret.outputMerger.stencilReadOnly = false;

        if(desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
          ret.outputMerger.depthReadOnly = true;
        if(desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
          ret.outputMerger.stencilReadOnly = true;

        view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

        view.viewFormat = MakeResourceFormat(desc.Format);
        view.type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
        {
          view.firstMip = desc.Texture1D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
        {
          view.numSlices = desc.Texture1DArray.ArraySize;
          view.firstSlice = desc.Texture1DArray.FirstArraySlice;
          view.firstMip = desc.Texture1DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
        {
          view.firstMip = desc.Texture2D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
        {
          view.numSlices = desc.Texture2DArray.ArraySize;
          view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          view.firstMip = desc.Texture2DArray.MipSlice;
          view.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
      else
      {
        view.resourceResourceId = ResourceId();
      }
    }

    ret.outputMerger.blendState.sampleMask = rs->OM.SampleMask;

    memcpy(ret.outputMerger.blendState.blendFactor, rs->OM.BlendFactor, sizeof(FLOAT) * 4);

    if(rs->OM.BlendState)
    {
      D3D11_BLEND_DESC desc;
      rs->OM.BlendState->GetDesc(&desc);

      ret.outputMerger.blendState.resourceId = rm->GetOriginalID(GetIDForResource(rs->OM.BlendState));

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
          rm->GetOriginalID(GetIDForResource(rs->OM.DepthStencilState));

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

  ret.predication.resourceId = rm->GetOriginalID(GetIDForResource(rs->Predicate));
  ret.predication.value = rs->PredicateValue == TRUE ? true : false;
  ret.predication.isPassing = rs->PredicationWouldPass();
}

ReplayStatus D3D11Replay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDevice->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

void D3D11Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

const SDFile &D3D11Replay::GetStructuredFile()
{
  return m_pDevice->GetStructuredFile();
}

std::vector<uint32_t> D3D11Replay::GetPassEvents(uint32_t eventId)
{
  std::vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

  const DrawcallDescription *start = draw;
  while(start && start->previous && !(start->previous->flags & DrawFlags::Clear))
  {
    const DrawcallDescription *prev = start->previous;

    if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) ||
       start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  while(start)
  {
    if(start == draw)
      break;

    if(start->flags & DrawFlags::Drawcall)
      passEvents.push_back(start->eventId);

    start = start->next;
  }

  return passEvents;
}

ResourceId D3D11Replay::GetLiveID(ResourceId id)
{
  if(!m_pDevice->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

bool D3D11Replay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                               CompType typeHint, float minval, float maxval, bool channels[4],
                               std::vector<uint32_t> &histogram)
{
  if(minval >= maxval)
    return false;

  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(texid, typeHint, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = (float)RDCMAX(details.texWidth >> mip, 1U);
  cdata.HistogramTextureResolution.y = (float)RDCMAX(details.texHeight >> mip, 1U);
  cdata.HistogramTextureResolution.z = (float)RDCMAX(details.texDepth >> mip, 1U);
  cdata.HistogramSlice = (float)sliceFace;
  cdata.HistogramMip = mip;
  cdata.HistogramSample = (int)RDCCLAMP(sample, 0U, details.sampleCount - 1);
  if(sample == ~0U)
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
    cdata.HistogramSlice = float(sliceFace);

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

bool D3D11Replay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float *minval, float *maxval)
{
  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(texid, typeHint, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  HistogramCBufferData cdata;
  cdata.HistogramTextureResolution.x = (float)RDCMAX(details.texWidth >> mip, 1U);
  cdata.HistogramTextureResolution.y = (float)RDCMAX(details.texHeight >> mip, 1U);
  cdata.HistogramTextureResolution.z = (float)RDCMAX(details.texDepth >> mip, 1U);
  cdata.HistogramSlice = (float)sliceFace;
  cdata.HistogramMip = mip;
  cdata.HistogramSample = (int)RDCCLAMP(sample, 0U, details.sampleCount - 1);
  if(sample == ~0U)
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

  DXGI_FORMAT fmt = GetTypedFormat(details.texFmt);

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

  if(details.texType == eTexType_3D)
    cdata.HistogramSlice = float(sliceFace);

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

void D3D11Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, bytebuf &retData)
{
  auto it = WrappedID3D11Buffer::m_BufferList.find(buff);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
  {
    RDCERR("Getting buffer data for unknown buffer %llu!", buff);
    return;
  }

  ID3D11Buffer *buffer = it->second.m_Buffer;

  RDCASSERT(buffer);

  GetDebugManager()->GetBufferData(buffer, offset, length, retData);
}

void D3D11Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11Resource *dummyTex = NULL;

  uint32_t subresource = 0;
  uint32_t mips = 0;

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

    if(mip >= mips || arrayIdx >= desc.ArraySize)
      return;

    if(params.remap != RemapTexture::NoRemap)
    {
      if(params.remap == RemapTexture::RGBA8)
      {
        desc.Format = IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                                : DXGI_FORMAT_R8G8B8A8_UNORM;
      }
      else if(params.remap == RemapTexture::RGBA16)
      {
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      }
      else if(params.remap == RemapTexture::RGBA32)
      {
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      }

      desc.ArraySize = 1;
    }

    subresource = arrayIdx * mips + mip;

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, 1, 1, desc.Format, mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      subresource = mip;

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
      rtvDesc.Texture1D.MipSlice = mip;

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

      D3D11_VIEWPORT viewport = {0, 0, (float)(desc.Width >> mip), 1.0f, 0.0f, 1.0f};

      SetOutputDimensions(desc.Width, 1);
      m_pImmediateContext->RSSetViewports(1, &viewport);

      {
        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = 0;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        RenderTextureInternal(texDisplay, false);
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

    if(desc.SampleDesc.Count > 1)
    {
      desc.ArraySize *= desc.SampleDesc.Count;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;

      wasms = true;
    }

    ID3D11Texture2D *d = NULL;

    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);

    if(mip >= mips || arrayIdx >= desc.ArraySize)
      return;

    if(params.remap != RemapTexture::NoRemap)
    {
      if(params.remap == RemapTexture::RGBA8)
      {
        desc.Format = (IsSRGBFormat(desc.Format) || wrapTex->m_RealDescriptor)
                          ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                          : DXGI_FORMAT_R8G8B8A8_UNORM;
      }
      else if(params.remap == RemapTexture::RGBA16)
      {
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      }
      else if(params.remap == RemapTexture::RGBA32)
      {
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      }

      desc.ArraySize = 1;
    }

    subresource = arrayIdx * mips + mip;

    HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      subresource = mip;

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
      rtvDesc.Texture2D.MipSlice = mip;

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

      D3D11_VIEWPORT viewport = {0,    0,   (float)(desc.Width >> mip), (float)(desc.Height >> mip),
                                 0.0f, 1.0f};

      SetOutputDimensions(desc.Width, desc.Height);
      m_pImmediateContext->RSSetViewports(1, &viewport);

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
        if(sampleCount > 1)
          texDisplay.sliceFace /= sampleCount;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        RenderTextureInternal(texDisplay, false);
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

      m_pImmediateContext->ResolveSubresource(resolveTex, arrayIdx, wrapTex, arrayIdx, desc.Format);
      m_pImmediateContext->CopyResource(d, resolveTex);

      SAFE_RELEASE(resolveTex);
    }
    else if(wasms)
    {
      GetDebugManager()->CopyTex2DMSToArray(UNWRAP(WrappedID3D11Texture2D1, d), wrapTex->GetReal());
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

    if(mip >= mips)
      return;

    if(params.remap != RemapTexture::NoRemap)
    {
      if(params.remap == RemapTexture::RGBA8)
      {
        desc.Format = IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                                : DXGI_FORMAT_R8G8B8A8_UNORM;
      }
      else if(params.remap == RemapTexture::RGBA16)
      {
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      }
      else if(params.remap == RemapTexture::RGBA32)
      {
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      }
    }

    subresource = mip;

    HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &d);

    dummyTex = d;

    if(FAILED(hr))
    {
      RDCERR("Couldn't create staging texture to retrieve data. HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    bytesize = GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip);

    if(params.remap != RemapTexture::NoRemap)
    {
      subresource = mip;

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
      rtvDesc.Texture3D.MipSlice = mip;
      rtvDesc.Texture3D.FirstWSlice = 0;
      rtvDesc.Texture3D.WSize = 1;
      ID3D11RenderTargetView *wrappedrtv = NULL;
      ID3D11RenderTargetView *rtv = NULL;

      D3D11_VIEWPORT viewport = {0,    0,   (float)(desc.Width >> mip), (float)(desc.Height >> mip),
                                 0.0f, 1.0f};

      for(UINT i = 0; i < (desc.Depth >> mip); i++)
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
        texDisplay.mip = mip;
        texDisplay.sampleIdx = 0;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = i << mip;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        RenderTextureInternal(texDisplay, false);

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
    RDCERR("Trying to get texture data for unknown ID %llu!", tex);
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
    if(intercept.numSlices > 1 && arrayIdx > 0 && (int)arrayIdx < intercept.numSlices)
    {
      byte *dst = data.data();
      byte *src = data.data() + intercept.app.DepthPitch * arrayIdx;

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

void D3D11Replay::ReplaceResource(ResourceId from, ResourceId to)
{
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

void D3D11Replay::BuildShader(ShaderEncoding sourceEncoding, bytebuf source,
                              const std::string &entry, const ShaderCompileFlags &compileFlags,
                              ShaderStage type, ResourceId *id, std::string *errors)
{
  if(id == NULL || errors == NULL)
  {
    if(id)
      *id = ResourceId();
    return;
  }

  if(sourceEncoding == ShaderEncoding::HLSL)
  {
    uint32_t flags = DXBC::DecodeFlags(compileFlags);

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

    std::string hlsl;
    hlsl.assign((const char *)source.data(), source.size());

    ID3DBlob *blob = NULL;

    *errors = m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), entry.c_str(), flags,
                                                         profile, &blob);

    if(blob == NULL)
    {
      *id = ResourceId();
      return;
    }

    source.clear();
    source.assign((byte *)blob->GetBufferPointer(), blob->GetBufferSize());

    SAFE_RELEASE(blob);
  }

  switch(type)
  {
    case ShaderStage::Vertex:
    {
      ID3D11VertexShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateVertexShader(source.data(), source.size(), NULL, &sh);

      if(sh != NULL)
      {
        *id = ((WrappedID3D11Shader<ID3D11VertexShader> *)sh)->GetResourceID();
      }
      else
      {
        *errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        *id = ResourceId();
      }
      return;
    }
    case ShaderStage::Hull:
    {
      ID3D11HullShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateHullShader(source.data(), source.size(), NULL, &sh);

      if(sh != NULL)
      {
        *id = ((WrappedID3D11Shader<ID3D11HullShader> *)sh)->GetResourceID();
      }
      else
      {
        *errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        *id = ResourceId();
      }
      return;
    }
    case ShaderStage::Domain:
    {
      ID3D11DomainShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateDomainShader(source.data(), source.size(), NULL, &sh);

      if(sh != NULL)
      {
        *id = ((WrappedID3D11Shader<ID3D11DomainShader> *)sh)->GetResourceID();
      }
      else
      {
        *errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        *id = ResourceId();
      }
      return;
    }
    case ShaderStage::Geometry:
    {
      ID3D11GeometryShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateGeometryShader(source.data(), source.size(), NULL, &sh);

      if(sh != NULL)
      {
        *id = ((WrappedID3D11Shader<ID3D11GeometryShader> *)sh)->GetResourceID();
      }
      else
      {
        *errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        *id = ResourceId();
      }
      return;
    }
    case ShaderStage::Pixel:
    {
      ID3D11PixelShader *sh = NULL;
      HRESULT hr = m_pDevice->CreatePixelShader(source.data(), source.size(), NULL, &sh);

      if(sh != NULL)
      {
        *id = ((WrappedID3D11Shader<ID3D11PixelShader> *)sh)->GetResourceID();
      }
      else
      {
        *errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        *id = ResourceId();
      }
      return;
    }
    case ShaderStage::Compute:
    {
      ID3D11ComputeShader *sh = NULL;
      HRESULT hr = m_pDevice->CreateComputeShader(source.data(), source.size(), NULL, &sh);

      if(sh != NULL)
      {
        *id = ((WrappedID3D11Shader<ID3D11ComputeShader> *)sh)->GetResourceID();
      }
      else
      {
        *errors = StringFormat::Fmt("Failed to create shader: %s", ToStr(hr).c_str());
        *id = ResourceId();
      }
      return;
    }
    default: break;
  }

  RDCERR("Unexpected type in BuildShader!");
  *id = ResourceId();
}

void D3D11Replay::BuildTargetShader(ShaderEncoding sourceEncoding, bytebuf source,
                                    const std::string &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId *id, std::string *errors)
{
  ShaderCompileFlags debugCompileFlags =
      DXBC::EncodeFlags(DXBC::DecodeFlags(compileFlags) | D3DCOMPILE_DEBUG);

  BuildShader(sourceEncoding, source, entry, debugCompileFlags, type, id, errors);
}

void D3D11Replay::BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source,
                                    const std::string &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId *id, std::string *errors)
{
  BuildTargetShader(sourceEncoding, source, entry, compileFlags, type, id, errors);
}

bool D3D11Replay::RenderTexture(TextureDisplay cfg)
{
  return RenderTextureInternal(cfg, true);
}

void D3D11Replay::RenderCheckerboard()
{
  D3D11RenderStateTracker tracker(m_pImmediateContext);

  CheckerboardCBuffer pixelData = {};

  pixelData.PrimaryColor = ConvertSRGBToLinear(RenderDoc::Inst().DarkCheckerboardColor());
  pixelData.SecondaryColor = ConvertSRGBToLinear(RenderDoc::Inst().LightCheckerboardColor());
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

void D3D11Replay::FillCBufferVariables(ResourceId shader, std::string entryPoint, uint32_t cbufSlot,
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

  StandardFillCBufferVariables(refl.constantBlocks[cbufSlot].variables, outvars, data);
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

  cbuf.PickRayPos = rayPos;
  cbuf.PickRayDir = rayDir;

  cbuf.PickMVP = cfg.position.unproject ? pickMVPProj : pickMVP;

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

      m_pImmediateContext->UpdateSubresource(m_VertexPick.PickIBBuf, 0, &box, outidxs.data(), 0, 0);
    }
  }

  // unpack and linearise the data
  if(vb)
  {
    bytebuf oldData;
    GetDebugManager()->GetBufferData(vb, cfg.position.vertexByteOffset, 0, oldData);

    // clamp maxIndex to upper bound in case we got invalid indices or primitive restart indices
    maxIndex = RDCMIN(maxIndex, uint32_t(oldData.size() / cfg.position.vertexByteStride));

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

    std::vector<FloatVector> vbData;
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

void D3D11Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
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

    RenderTextureInternal(texDisplay, false);
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
    m_CustomShaderResourceId = GetIDForResource(m_CustomShaderTex);
  }
}

ResourceId D3D11Replay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                          uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint)
{
  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(texid, typeHint, false);

  CreateCustomShaderTex(details.texWidth, details.texHeight);

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11RenderTargetView *customRTV = NULL;

  {
    D3D11_RENDER_TARGET_VIEW_DESC desc;

    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = mip;

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
  viewport.Width = (float)RDCMAX(1U, details.texWidth >> mip);
  viewport.Height = (float)RDCMAX(1U, details.texHeight >> mip);

  m_pImmediateContext->RSSetViewports(1, &viewport);

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = shader;
  disp.resourceId = texid;
  disp.typeHint = typeHint;
  disp.backgroundColor = FloatVector(0, 0, 0, 1.0);
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

  SetOutputDimensions(RDCMAX(1U, details.texWidth >> mip), RDCMAX(1U, details.texHeight >> mip));

  RenderTextureInternal(disp, true);

  return m_CustomShaderResourceId;
}

bool D3D11Replay::IsRenderOutput(ResourceId id)
{
  for(size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(m_CurPipelineState.outputMerger.renderTargets[i].viewResourceId == id ||
       m_CurPipelineState.outputMerger.renderTargets[i].resourceResourceId == id)
      return true;
  }

  if(m_CurPipelineState.outputMerger.depthTarget.viewResourceId == id ||
     m_CurPipelineState.outputMerger.depthTarget.resourceResourceId == id)
    return true;

  return false;
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

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    desc.CPUAccessFlags = 0;
    desc.Format = MakeDXGIFormat(templateTex.format);
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = RDCMAX(1U, templateTex.width);

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 1D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      desc.Format = GetTypelessFormat(desc.Format);

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
    desc.Format = MakeDXGIFormat(templateTex.format);
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = RDCMAX(1U, templateTex.width);
    desc.Height = RDCMAX(1U, templateTex.height);
    desc.SampleDesc.Count = RDCMAX(1U, templateTex.msSamp);
    desc.SampleDesc.Quality = templateTex.msQual;

    if(templateTex.creationFlags & TextureCategory::DepthTarget || IsDepthFormat(desc.Format))
    {
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
      desc.Format = GetTypelessFormat(desc.Format);
    }

    if(templateTex.cubemap)
      desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

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

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    desc.CPUAccessFlags = 0;
    desc.Format = MakeDXGIFormat(templateTex.format);
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

  m_ProxyResources.push_back(resource);

  return ret;
}

void D3D11Replay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
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

    if(mip >= mips || arrayIdx >= desc.ArraySize)
    {
      RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
      return;
    }

    uint32_t sub = arrayIdx * mips + mip;

    if(dataSize < GetByteSize(desc.Width, 1, 1, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), sub, NULL, data,
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip));
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(texid) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *tex =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);

    UINT sampleCount = RDCMAX(1U, desc.SampleDesc.Count);

    if(mip >= mips || arrayIdx >= desc.ArraySize * sampleCount)
    {
      RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
      return;
    }

    if(dataSize < GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip))
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

      // create an unwrapped texture to upload the data into a slice of
      ID3D11Texture2D *uploadTex = NULL;
      m_pDevice->GetReal()->CreateTexture2D(&uploadDesc, NULL, &uploadTex);

      ctx->UpdateSubresource(uploadTex, arrayIdx, NULL, data,
                             GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                             GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));

      // copy that slice into MSAA sample
      GetDebugManager()->CopyArrayToTex2DMS(tex->GetReal(), uploadTex, arrayIdx);

      uploadTex->Release();
    }
    else
    {
      uint32_t sub = arrayIdx * mips + mip;

      ctx->UpdateSubresource(tex->GetReal(), sub, NULL, data,
                             GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                             GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
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

    if(mip >= mips)
    {
      RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
      return;
    }

    if(dataSize < GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), mip, NULL, data,
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                           GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
  }
  else
  {
    RDCERR("Invalid texture id passed to SetProxyTextureData");
  }
}

bool D3D11Replay::IsTextureSupported(const ResourceFormat &format)
{
  return MakeDXGIFormat(format) != DXGI_FORMAT_UNKNOWN;
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

ReplayStatus D3D11_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D11 replay device");

  IntelCounters::Load();

  HMODULE lib = NULL;
  lib = LoadLibraryA("d3d11.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d11.dll");
    return ReplayStatus::APIInitFailed;
  }

  PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN CreateDeviceAndSwapChain =
      (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(lib, "D3D11CreateDeviceAndSwapChain");

  lib = LoadLibraryA("d3d9.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d9.dll");
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

  D3D11InitParams initParams;

  uint64_t ver = D3D11InitParams::CurrentVersion;

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!D3D11InitParams::IsSupportedVersion(ver))
    {
      RDCERR("Incompatible D3D11 serialise version %llu", ver);
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

  ID3D11Device *device = NULL;

  if(initParams.SDKVersion != D3D11_SDK_VERSION)
  {
    RDCWARN(
        "Capture file used a different SDK version %lu from replay app %lu. Results may be "
        "undefined",
        initParams.SDKVersion, D3D11_SDK_VERSION);
  }

  if(initParams.DriverType == D3D_DRIVER_TYPE_UNKNOWN)
    initParams.DriverType = D3D_DRIVER_TYPE_HARDWARE;

  int i = -2;

  // force using our feature levels as we require >= 11_0 for analysis
  D3D_FEATURE_LEVEL featureLevelArray11_1[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  UINT numFeatureLevels11_1 = ARRAY_COUNT(featureLevelArray11_1);

  D3D_FEATURE_LEVEL featureLevelArray11_0[] = {D3D_FEATURE_LEVEL_11_0};
  UINT numFeatureLevels11_0 = ARRAY_COUNT(featureLevelArray11_0);

  D3D_DRIVER_TYPE driverTypes[] = {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP,
                                   D3D_DRIVER_TYPE_REFERENCE};
  int numDrivers = ARRAY_COUNT(driverTypes);

  D3D_FEATURE_LEVEL *featureLevelArray = featureLevelArray11_1;
  UINT numFeatureLevels = numFeatureLevels11_1;
  D3D_DRIVER_TYPE driverType = initParams.DriverType;
  UINT flags = initParams.Flags;

  HRESULT hr = E_FAIL;

  D3D_FEATURE_LEVEL maxFeatureLevel = D3D_FEATURE_LEVEL_9_1;

  // check for feature level 11 support - passing NULL feature level array implicitly checks for
  // 11_0 before others
  ID3D11Device *dev = NULL;
  hr = CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
                                NULL, NULL, &dev, &maxFeatureLevel, NULL);
  SAFE_RELEASE(dev);

  bool warpFallback = false;

  if(SUCCEEDED(hr) && maxFeatureLevel < D3D_FEATURE_LEVEL_11_0)
  {
    RDCWARN(
        "Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 "
        "availability - falling back to WARP rasterizer");
    driverTypes[0] = driverType = D3D_DRIVER_TYPE_WARP;
    warpFallback = true;
  }

  hr = E_FAIL;
  for(;;)
  {
#if ENABLED(RDOC_DEVEL)
    // in development builds, always enable debug layer during replay
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#else
    // in release builds, never enable it
    flags &= ~D3D11_CREATE_DEVICE_DEBUG;
#endif

    RDCLOG(
        "Creating D3D11 replay device, driver type %s, flags %x, %d feature levels (first %s)",
        ToStr(driverType).c_str(), flags, numFeatureLevels,
        (numFeatureLevels > 0 && featureLevelArray) ? ToStr(featureLevelArray[0]).c_str() : "NULL");

    hr = CreateDeviceAndSwapChain(
        /*pAdapter=*/NULL, driverType, /*Software=*/NULL, flags,
        /*pFeatureLevels=*/featureLevelArray, /*nFeatureLevels=*/numFeatureLevels, D3D11_SDK_VERSION,
        /*pSwapChainDesc=*/NULL, (IDXGISwapChain **)NULL, (ID3D11Device **)&device,
        (D3D_FEATURE_LEVEL *)NULL, (ID3D11DeviceContext **)NULL);

    if(SUCCEEDED(hr))
    {
      WrappedID3D11Device *wrappedDev = new WrappedID3D11Device(device, initParams);
      wrappedDev->SetInitParams(initParams, ver);

      RDCLOG("Created device.");
      D3D11Replay *replay = wrappedDev->GetReplay();

      replay->SetProxy(rdc == NULL, warpFallback);
      replay->CreateResources();
      if(warpFallback)
      {
        wrappedDev->AddDebugMessage(
            MessageCategory::Initialization, MessageSeverity::High, MessageSource::RuntimeWarning,
            "Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 "
            "availability - falling back to WARP rasterizer.\n"
            "Performance and usability will be significantly degraded.");
      }

      *driver = (IReplayDriver *)replay;
      return ReplayStatus::Succeeded;
    }

    RDCLOG("Device creation failed, %s", ToStr(hr).c_str());

    if(i == -1)
    {
      RDCWARN("Couldn't create device with similar settings to capture.");
    }

    SAFE_RELEASE(device);

    i++;

    if(i >= numDrivers * 2)
      break;

    if(i >= 0)
      driverType = driverTypes[i / 2];

    if(i % 2 == 0)
    {
      featureLevelArray = featureLevelArray11_1;
      numFeatureLevels = numFeatureLevels11_1;
    }
    else
    {
      featureLevelArray = featureLevelArray11_0;
      numFeatureLevels = numFeatureLevels11_0;
    }
  }

  RDCERR("Couldn't create any compatible d3d11 device.");

  if(flags & D3D11_CREATE_DEVICE_DEBUG)
    RDCLOG(
        "Development RenderDoc builds require D3D debug layers available, "
        "ensure you have the windows SDK or windows feature needed.");

  return ReplayStatus::APIHardwareUnsupported;
}

static DriverRegistration D3D11DriverRegistration(RDCDriver::D3D11, &D3D11_CreateReplayDevice);

void D3D11_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedID3D11Device device(NULL, D3D11InitParams());

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = device.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    device.GetStructuredFile().Swap(output);
}

static StructuredProcessRegistration D3D11ProcessRegistration(RDCDriver::D3D11,
                                                              &D3D11_ProcessStructured);