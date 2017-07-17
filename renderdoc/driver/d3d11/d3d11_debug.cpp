/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "d3d11_debug.h"
#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "serialise/string_utils.h"
#include "stb/stb_truetype.h"
#include "d3d11_context.h"
#include "d3d11_manager.h"
#include "d3d11_renderstate.h"

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);

struct D3DBlobShaderCallbacks
{
  D3DBlobShaderCallbacks()
  {
    HMODULE d3dcompiler = GetD3DCompiler();

    if(d3dcompiler == NULL)
      RDCFATAL("Can't get handle to d3dcompiler_??.dll");

    m_BlobCreate = (pD3DCreateBlob)GetProcAddress(d3dcompiler, "D3DCreateBlob");

    if(m_BlobCreate == NULL)
      RDCFATAL("d3dcompiler.dll doesn't contain D3DCreateBlob");
  }

  bool Create(uint32_t size, byte *data, ID3DBlob **ret) const
  {
    RDCASSERT(ret);

    *ret = NULL;
    HRESULT hr = m_BlobCreate((SIZE_T)size, ret);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create blob of size %u from shadercache: %08x", size, hr);
      return false;
    }

    memcpy((*ret)->GetBufferPointer(), data, size);

    return true;
  }

  void Destroy(ID3DBlob *blob) const { blob->Release(); }
  uint32_t GetSize(ID3DBlob *blob) const { return (uint32_t)blob->GetBufferSize(); }
  byte *GetData(ID3DBlob *blob) const { return (byte *)blob->GetBufferPointer(); }
  pD3DCreateBlob m_BlobCreate;
} ShaderCacheCallbacks;

D3D11DebugManager::D3D11DebugManager(WrappedID3D11Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D11DebugManager));

  m_WrappedDevice = wrapper;
  m_pDevice = wrapper;
  m_pDevice->GetImmediateContext(&m_pImmediateContext);
  m_ResourceManager = wrapper->GetResourceManager();

  m_WrappedContext = wrapper->GetImmediateContext();

  m_HighlightCache.driver = wrapper->GetReplay();

  m_OutputWindowID = 1;

  m_supersamplingX = 1.0f;
  m_supersamplingY = 1.0f;

  m_width = m_height = 1;

  wrapper->InternalRef();

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.0f);

  m_pFactory = NULL;

  HRESULT hr = S_OK;

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
      hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&m_pFactory);

      SAFE_RELEASE(pDXGIDevice);
      SAFE_RELEASE(pDXGIAdapter);

      if(FAILED(hr))
      {
        RDCERR("Couldn't get DXGI factory from DXGI adapter");
      }
    }
  }

  bool success = LoadShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, ShaderCacheCallbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  m_CacheShaders = true;

  InitStreamOut();
  InitDebugRendering();
  InitFontRendering();

  m_CacheShaders = false;

  PostDeviceInitCounters();

  RenderDoc::Inst().SetProgress(DebugManagerInit, 1.0f);

  AMDCounters *counters = new AMDCounters();
  if(counters->Init((void *)m_pDevice))
  {
    m_pAMDCounters = counters;
  }
  else
  {
    delete counters;
    m_pAMDCounters = NULL;
  }
}

D3D11DebugManager::~D3D11DebugManager()
{
  SAFE_DELETE(m_pAMDCounters);

  PreDeviceShutdownCounters();

  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    ShaderCacheCallbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      ShaderCacheCallbacks.Destroy(it->second);
  }

  ShutdownFontRendering();
  ShutdownStreamOut();

  if(m_OverlayResourceId != ResourceId())
    SAFE_RELEASE(m_OverlayRenderTex);

  SAFE_RELEASE(m_CustomShaderRTV);

  if(m_CustomShaderResourceId != ResourceId())
    SAFE_RELEASE(m_CustomShaderTex);

  SAFE_RELEASE(m_pFactory);

  while(!m_ShaderItemCache.empty())
  {
    CacheElem &elem = m_ShaderItemCache.back();
    elem.Release();
    m_ShaderItemCache.pop_back();
  }

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    SAFE_RELEASE(it->second.vsout.buf);
    SAFE_RELEASE(it->second.vsout.idxBuf);
    SAFE_RELEASE(it->second.gsout.buf);
    SAFE_RELEASE(it->second.gsout.idxBuf);
  }

  m_PostVSData.clear();

  SAFE_RELEASE(m_pImmediateContext);
  m_WrappedDevice->InternalRelease();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

//////////////////////////////////////////////////////
// debug/replay functions

string D3D11DebugManager::GetShaderBlob(const char *source, const char *entry,
                                        const uint32_t compileFlags, const char *profile,
                                        ID3DBlob **srcblob)
{
  uint32_t hash = strhash(source);
  hash = strhash(entry, hash);
  hash = strhash(profile, hash);
  hash ^= compileFlags;

  if(m_ShaderCache.find(hash) != m_ShaderCache.end())
  {
    *srcblob = m_ShaderCache[hash];
    (*srcblob)->AddRef();
    return "";
  }

  HRESULT hr = S_OK;

  ID3DBlob *byteBlob = NULL;
  ID3DBlob *errBlob = NULL;

  HMODULE d3dcompiler = GetD3DCompiler();

  if(d3dcompiler == NULL)
  {
    RDCFATAL("Can't get handle to d3dcompiler_??.dll");
  }

  pD3DCompile compileFunc = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");

  if(compileFunc == NULL)
  {
    RDCFATAL("Can't get D3DCompile from d3dcompiler_??.dll");
  }

  uint32_t flags = compileFlags & ~D3DCOMPILE_NO_PRESHADER;

  hr = compileFunc(source, strlen(source), entry, NULL, NULL, entry, profile, flags, 0, &byteBlob,
                   &errBlob);

  string errors = "";

  if(errBlob)
  {
    errors = (char *)errBlob->GetBufferPointer();

    string logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCWARN("Shader compile error in '%s':\n%s", entry, logerror.c_str());

    SAFE_RELEASE(errBlob);

    if(FAILED(hr))
    {
      SAFE_RELEASE(byteBlob);
      return errors;
    }
  }

  if(m_CacheShaders)
  {
    m_ShaderCache[hash] = byteBlob;
    byteBlob->AddRef();
    m_ShaderCacheDirty = true;
  }

  SAFE_RELEASE(errBlob);

  *srcblob = byteBlob;
  return errors;
}

ID3D11VertexShader *D3D11DebugManager::MakeVShader(const char *source, const char *entry,
                                                   const char *profile, int numInputDescs,
                                                   D3D11_INPUT_ELEMENT_DESC *inputs,
                                                   ID3D11InputLayout **ret, vector<byte> *blob)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
  {
    RDCERR("Couldn't get shader blob for %s", entry);
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11VertexShader *ps = NULL;

  HRESULT hr = m_pDevice->CreateVertexShader(bytecode, bytecodeLen, NULL, &ps);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create vertex shader for %s %08x", entry, hr);

    SAFE_RELEASE(byteBlob);

    return NULL;
  }

  if(numInputDescs)
  {
    hr = m_pDevice->CreateInputLayout(inputs, numInputDescs, bytecode, bytecodeLen, ret);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create input layout for %s %08x", entry, hr);
    }
  }

  if(blob)
  {
    blob->resize(bytecodeLen);
    memcpy(&(*blob)[0], bytecode, bytecodeLen);
  }

  SAFE_RELEASE(byteBlob);

  return ps;
}

ID3D11GeometryShader *D3D11DebugManager::MakeGShader(const char *source, const char *entry,
                                                     const char *profile)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
  {
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11GeometryShader *gs = NULL;

  HRESULT hr = m_pDevice->CreateGeometryShader(bytecode, bytecodeLen, NULL, &gs);

  SAFE_RELEASE(byteBlob);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create geometry shader for %s %08x", entry, hr);
    return NULL;
  }

  return gs;
}

ID3D11PixelShader *D3D11DebugManager::MakePShader(const char *source, const char *entry,
                                                  const char *profile)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
  {
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11PixelShader *ps = NULL;

  HRESULT hr = m_pDevice->CreatePixelShader(bytecode, bytecodeLen, NULL, &ps);

  SAFE_RELEASE(byteBlob);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create pixel shader for %s %08x", entry, hr);
    return NULL;
  }

  return ps;
}

ID3D11ComputeShader *D3D11DebugManager::MakeCShader(const char *source, const char *entry,
                                                    const char *profile)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
  {
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11ComputeShader *cs = NULL;

  HRESULT hr = m_pDevice->CreateComputeShader(bytecode, bytecodeLen, NULL, &cs);

  SAFE_RELEASE(byteBlob);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create compute shader for %s %08x", entry, hr);
    return NULL;
  }

  return cs;
}

void D3D11DebugManager::BuildShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStage type, ResourceId *id, string *errors)
{
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
  *errors = GetShaderBlob(source.c_str(), entry.c_str(), compileFlags, profile, &blob);

  if(blob == NULL)
  {
    *id = ResourceId();
    return;
  }

  switch(type)
  {
    case ShaderStage::Vertex:
    {
      ID3D11VertexShader *sh = NULL;
      m_pDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

      SAFE_RELEASE(blob);

      if(sh != NULL)
        *id = ((WrappedID3D11Shader<ID3D11VertexShader> *)sh)->GetResourceID();
      else
        *id = ResourceId();
      return;
    }
    case ShaderStage::Hull:
    {
      ID3D11HullShader *sh = NULL;
      m_pDevice->CreateHullShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

      SAFE_RELEASE(blob);

      if(sh != NULL)
        *id = ((WrappedID3D11Shader<ID3D11HullShader> *)sh)->GetResourceID();
      else
        *id = ResourceId();
      return;
    }
    case ShaderStage::Domain:
    {
      ID3D11DomainShader *sh = NULL;
      m_pDevice->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

      SAFE_RELEASE(blob);

      if(sh != NULL)
        *id = ((WrappedID3D11Shader<ID3D11DomainShader> *)sh)->GetResourceID();
      else
        *id = ResourceId();
      return;
    }
    case ShaderStage::Geometry:
    {
      ID3D11GeometryShader *sh = NULL;
      m_pDevice->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

      SAFE_RELEASE(blob);

      if(sh != NULL)
        *id = ((WrappedID3D11Shader<ID3D11GeometryShader> *)sh)->GetResourceID();
      else
        *id = ResourceId();
      return;
    }
    case ShaderStage::Pixel:
    {
      ID3D11PixelShader *sh = NULL;
      m_pDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

      SAFE_RELEASE(blob);

      if(sh != NULL)
        *id = ((WrappedID3D11Shader<ID3D11PixelShader> *)sh)->GetResourceID();
      else
        *id = ResourceId();
      return;
    }
    case ShaderStage::Compute:
    {
      ID3D11ComputeShader *sh = NULL;
      m_pDevice->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

      SAFE_RELEASE(blob);

      if(sh != NULL)
        *id = ((WrappedID3D11Shader<ID3D11ComputeShader> *)sh)->GetResourceID();
      else
        *id = ResourceId();
      return;
    }
    default: break;
  }

  SAFE_RELEASE(blob);

  RDCERR("Unexpected type in BuildShader!");
  *id = ResourceId();
}

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(UINT size)
{
  D3D11_BUFFER_DESC bufDesc;

  bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bufDesc.Usage = D3D11_USAGE_DYNAMIC;
  bufDesc.ByteWidth = size;
  bufDesc.StructureByteStride = 0;
  bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bufDesc.MiscFlags = 0;

  ID3D11Buffer *ret = NULL;

  HRESULT hr = m_pDevice->CreateBuffer(&bufDesc, NULL, &ret);

  if(FAILED(hr))
  {
    RDCERR("Failed to create CBuffer %08x", hr);
    return NULL;
  }

  return ret;
}

void D3D11DebugManager::FillCBuffer(ID3D11Buffer *buf, const void *data, size_t size)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer %08x", hr);
  }
  else
  {
    memcpy(mapped.pData, data, size);
    m_pImmediateContext->Unmap(buf, 0);
  }
}

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(const void *data, size_t size)
{
  int idx = m_DebugRender.publicCBufIdx;

  FillCBuffer(m_DebugRender.PublicCBuffers[idx], data, size);

  m_DebugRender.publicCBufIdx =
      (m_DebugRender.publicCBufIdx + 1) % ARRAY_COUNT(m_DebugRender.PublicCBuffers);

  return m_DebugRender.PublicCBuffers[idx];
}

#include "data/hlsl/debugcbuffers.h"

bool D3D11DebugManager::InitDebugRendering()
{
  HRESULT hr = S_OK;

  m_CustomShaderTex = NULL;
  m_CustomShaderRTV = NULL;
  m_CustomShaderResourceId = ResourceId();

  m_OverlayRenderTex = NULL;
  m_OverlayResourceId = ResourceId();

  m_DebugRender.GenericVSCBuffer = MakeCBuffer(sizeof(DebugVertexCBuffer));
  m_DebugRender.GenericGSCBuffer = MakeCBuffer(sizeof(DebugGeometryCBuffer));
  m_DebugRender.GenericPSCBuffer = MakeCBuffer(sizeof(DebugPixelCBufferData));

  for(int i = 0; i < ARRAY_COUNT(m_DebugRender.PublicCBuffers); i++)
    m_DebugRender.PublicCBuffers[i] = MakeCBuffer(sizeof(float) * 4 * 100);

  m_DebugRender.publicCBufIdx = 0;

  string multisamplehlsl = GetEmbeddedResource(multisample_hlsl);

  m_DebugRender.CopyMSToArrayPS =
      MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyMSToArray", "ps_5_0");
  m_DebugRender.CopyArrayToMSPS =
      MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyArrayToMS", "ps_5_0");
  m_DebugRender.FloatCopyMSToArrayPS =
      MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyMSToArray", "ps_5_0");
  m_DebugRender.FloatCopyArrayToMSPS =
      MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyArrayToMS", "ps_5_0");
  m_DebugRender.DepthCopyMSToArrayPS =
      MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyMSToArray", "ps_5_0");
  m_DebugRender.DepthCopyArrayToMSPS =
      MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyArrayToMS", "ps_5_0");

  string displayhlsl = GetEmbeddedResource(debugcbuffers_h);
  displayhlsl += GetEmbeddedResource(debugcommon_hlsl);
  displayhlsl += GetEmbeddedResource(debugdisplay_hlsl);

  string meshhlsl = GetEmbeddedResource(debugcbuffers_h) + GetEmbeddedResource(mesh_hlsl);

  m_DebugRender.FullscreenVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");

  if(RenderDoc::Inst().IsReplayApp())
  {
    D3D11_INPUT_ELEMENT_DESC inputDescSecondary[2];

    inputDescSecondary[0].SemanticName = "pos";
    inputDescSecondary[0].SemanticIndex = 0;
    inputDescSecondary[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputDescSecondary[0].InputSlot = 0;
    inputDescSecondary[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    inputDescSecondary[0].AlignedByteOffset = 0;
    inputDescSecondary[0].InstanceDataStepRate = 0;

    inputDescSecondary[1].SemanticName = "sec";
    inputDescSecondary[1].SemanticIndex = 0;
    inputDescSecondary[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    inputDescSecondary[1].InputSlot = 0;
    inputDescSecondary[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    inputDescSecondary[1].AlignedByteOffset = 0;
    inputDescSecondary[1].InstanceDataStepRate = 0;

    vector<byte> bytecode;

    m_DebugRender.GenericVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_DebugVS", "vs_4_0");
    m_DebugRender.TexDisplayPS =
        MakePShader(displayhlsl.c_str(), "RENDERDOC_TexDisplayPS", "ps_5_0");
    m_DebugRender.MeshVS = MakeVShader(meshhlsl.c_str(), "RENDERDOC_MeshVS", "vs_4_0", 2,
                                       inputDescSecondary, &m_DebugRender.GenericLayout, &bytecode);
    m_DebugRender.MeshGS = MakeGShader(meshhlsl.c_str(), "RENDERDOC_MeshGS", "gs_4_0");
    m_DebugRender.MeshPS = MakePShader(meshhlsl.c_str(), "RENDERDOC_MeshPS", "ps_4_0");

    m_DebugRender.TriangleSizeGS =
        MakeGShader(meshhlsl.c_str(), "RENDERDOC_TriangleSizeGS", "gs_4_0");
    m_DebugRender.TriangleSizePS =
        MakePShader(meshhlsl.c_str(), "RENDERDOC_TriangleSizePS", "ps_4_0");

    m_DebugRender.MeshVSBytecode = new byte[bytecode.size()];
    m_DebugRender.MeshVSBytelen = (uint32_t)bytecode.size();
    memcpy(m_DebugRender.MeshVSBytecode, &bytecode[0], bytecode.size());

    m_DebugRender.WireframePS = MakePShader(displayhlsl.c_str(), "RENDERDOC_WireframePS", "ps_4_0");
    m_DebugRender.OverlayPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_OverlayPS", "ps_4_0");
    m_DebugRender.CheckerboardPS =
        MakePShader(displayhlsl.c_str(), "RENDERDOC_CheckerboardPS", "ps_4_0");
    m_DebugRender.OutlinePS = MakePShader(displayhlsl.c_str(), "RENDERDOC_OutlinePS", "ps_4_0");

    m_DebugRender.QuadOverdrawPS =
        MakePShader(displayhlsl.c_str(), "RENDERDOC_QuadOverdrawPS", "ps_5_0");
    m_DebugRender.QOResolvePS = MakePShader(displayhlsl.c_str(), "RENDERDOC_QOResolvePS", "ps_5_0");

    m_DebugRender.PixelHistoryUnusedCS =
        MakeCShader(displayhlsl.c_str(), "RENDERDOC_PixelHistoryUnused", "cs_5_0");
    m_DebugRender.PixelHistoryCopyCS =
        MakeCShader(displayhlsl.c_str(), "RENDERDOC_PixelHistoryCopyPixel", "cs_5_0");
    m_DebugRender.PrimitiveIDPS =
        MakePShader(displayhlsl.c_str(), "RENDERDOC_PrimitiveIDPS", "ps_5_0");

    m_DebugRender.MeshPickCS = MakeCShader(meshhlsl.c_str(), "RENDERDOC_MeshPickCS", "cs_5_0");

    string histogramhlsl = GetEmbeddedResource(debugcbuffers_h);
    histogramhlsl += GetEmbeddedResource(debugcommon_hlsl);
    histogramhlsl += GetEmbeddedResource(histogram_hlsl);

    RenderDoc::Inst().SetProgress(DebugManagerInit, 0.1f);

    for(int t = eTexType_1D; t < eTexType_Max; t++)
    {
      if(t == eTexType_Unused)
        continue;

      // float, uint, sint
      for(int i = 0; i < 3; i++)
      {
        string hlsl = string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
        hlsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
        hlsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
        hlsl += histogramhlsl;

        m_DebugRender.TileMinMaxCS[t][i] =
            MakeCShader(hlsl.c_str(), "RENDERDOC_TileMinMaxCS", "cs_5_0");
        m_DebugRender.HistogramCS[t][i] =
            MakeCShader(hlsl.c_str(), "RENDERDOC_HistogramCS", "cs_5_0");

        if(t == 1)
          m_DebugRender.ResultMinMaxCS[i] =
              MakeCShader(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS", "cs_5_0");

        RenderDoc::Inst().SetProgress(
            DebugManagerInit,
            (float(i + 3.0f * t) / float(2.0f + 3.0f * (eTexType_Max - 1))) * 0.7f + 0.1f);
      }
    }
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.8f);

  RDCCOMPILE_ASSERT(eTexType_1D == RESTYPE_TEX1D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_2D == RESTYPE_TEX2D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_3D == RESTYPE_TEX3D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_Depth == RESTYPE_DEPTH, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_Stencil == RESTYPE_DEPTH_STENCIL,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_DepthMS == RESTYPE_DEPTH_MS,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_StencilMS == RESTYPE_DEPTH_STENCIL_MS,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_2DMS == RESTYPE_TEX2D_MS,
                    "Tex type enum doesn't match shader defines");

  D3D11_BLEND_DESC blendDesc;
  RDCEraseEl(blendDesc);

  blendDesc.AlphaToCoverageEnable = FALSE;
  blendDesc.IndependentBlendEnable = FALSE;
  blendDesc.RenderTarget[0].BlendEnable = TRUE;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  hr = m_pDevice->CreateBlendState(&blendDesc, &m_DebugRender.BlendState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create default blendstate %08x", hr);
  }

  blendDesc.RenderTarget[0].BlendEnable = FALSE;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;

  hr = m_pDevice->CreateBlendState(&blendDesc, &m_DebugRender.NopBlendState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create nop blendstate %08x", hr);
  }

  D3D11_RASTERIZER_DESC rastDesc;
  RDCEraseEl(rastDesc);

  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.FillMode = D3D11_FILL_SOLID;
  rastDesc.DepthBias = 0;

  hr = m_pDevice->CreateRasterizerState(&rastDesc, &m_DebugRender.RastState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create default rasterizer state %08x", hr);
  }

  D3D11_SAMPLER_DESC sampDesc;
  RDCEraseEl(sampDesc);

  sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  sampDesc.MaxAnisotropy = 1;
  sampDesc.MinLOD = 0;
  sampDesc.MaxLOD = FLT_MAX;
  sampDesc.MipLODBias = 0.0f;

  hr = m_pDevice->CreateSamplerState(&sampDesc, &m_DebugRender.LinearSampState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create linear sampler state %08x", hr);
  }

  sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

  hr = m_pDevice->CreateSamplerState(&sampDesc, &m_DebugRender.PointSampState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create point sampler state %08x", hr);
  }

  {
    D3D11_DEPTH_STENCIL_DESC desc;

    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_KEEP;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = desc.StencilWriteMask = 0xff;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.NoDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create no-depth depthstencilstate %08x", hr);
    }

    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.LEqualDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create less-equal depthstencilstate %08x", hr);
    }

    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = TRUE;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.AllPassDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass depthstencilstate %08x", hr);
    }

    desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilReadMask = desc.StencilWriteMask = 0;
    desc.StencilEnable = FALSE;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.NopDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create nop depthstencilstate %08x", hr);
    }

    desc.StencilReadMask = desc.StencilWriteMask = 0xff;
    desc.StencilEnable = TRUE;
    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_INCR_SAT;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.AllPassIncrDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate %08x", hr);
    }

    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.StencIncrEqDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate %08x", hr);
    }
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.9f);

  if(RenderDoc::Inst().IsReplayApp())
  {
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D *pickTex = NULL;

    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Width = 100;
    desc.Height = 100;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = 0;

    hr = m_pDevice->CreateTexture2D(&desc, NULL, &pickTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create pick tex %08x", hr);
    }
    else
    {
      hr = m_pDevice->CreateRenderTargetView(pickTex, NULL, &m_DebugRender.PickPixelRT);

      if(FAILED(hr))
      {
        RDCERR("Failed to create pick rt %08x", hr);
      }

      SAFE_RELEASE(pickTex);
    }
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    D3D11_TEXTURE2D_DESC desc;
    RDCEraseEl(desc);
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Width = 1;
    desc.Height = 1;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = m_pDevice->CreateTexture2D(&desc, NULL, &m_DebugRender.PickPixelStageTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create pick stage tex %08x", hr);
    }
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    D3D11_BUFFER_DESC bDesc;

    const uint32_t maxTexDim = 16384;
    const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
    const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

    bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bDesc.ByteWidth =
        2 * 4 * sizeof(float) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;
    bDesc.CPUAccessFlags = 0;
    bDesc.MiscFlags = 0;
    bDesc.StructureByteStride = 0;
    bDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.tileResultBuff);

    if(FAILED(hr))
    {
      RDCERR("Failed to create tile result buffer %08x", hr);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = bDesc.ByteWidth / sizeof(Vec4f);

    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc,
                                             &m_DebugRender.tileResultSRV[0]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result SRV 0 %08x", hr);

    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc,
                                             &m_DebugRender.tileResultSRV[1]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result SRV 1 %08x", hr);

    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc,
                                             &m_DebugRender.tileResultSRV[2]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result SRV 2 %08x", hr);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.Flags = 0;
    uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements;

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc,
                                              &m_DebugRender.tileResultUAV[0]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result UAV 0 %08x", hr);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc,
                                              &m_DebugRender.tileResultUAV[1]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result UAV 1 %08x", hr);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc,
                                              &m_DebugRender.tileResultUAV[2]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result UAV 2 %08x", hr);

    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    uavDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
    bDesc.ByteWidth = uavDesc.Buffer.NumElements * sizeof(int);

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.histogramBuff);

    if(FAILED(hr))
      RDCERR("Failed to create histogram buff %08x", hr);

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.histogramBuff, &uavDesc,
                                              &m_DebugRender.histogramUAV);

    if(FAILED(hr))
      RDCERR("Failed to create histogram UAV %08x", hr);

    bDesc.BindFlags = 0;
    bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bDesc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.histogramStageBuff);

    if(FAILED(hr))
      RDCERR("Failed to create histogram stage buff %08x", hr);

    bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bDesc.CPUAccessFlags = 0;
    bDesc.ByteWidth = 2 * 4 * sizeof(float);
    bDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.resultBuff);

    if(FAILED(hr))
      RDCERR("Failed to create result buff %08x", hr);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    uavDesc.Buffer.NumElements = 2;

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc,
                                              &m_DebugRender.resultUAV[0]);

    if(FAILED(hr))
      RDCERR("Failed to create result UAV 0 %08x", hr);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc,
                                              &m_DebugRender.resultUAV[1]);

    if(FAILED(hr))
      RDCERR("Failed to create result UAV 1 %08x", hr);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc,
                                              &m_DebugRender.resultUAV[2]);

    if(FAILED(hr))
      RDCERR("Failed to create result UAV 2 %08x", hr);

    bDesc.BindFlags = 0;
    bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bDesc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.resultStageBuff);

    if(FAILED(hr))
      RDCERR("Failed to create result stage buff %08x", hr);

    bDesc.ByteWidth = sizeof(Vec4f) * DebugRenderData::maxMeshPicks;
    bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bDesc.CPUAccessFlags = 0;
    bDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bDesc.StructureByteStride = sizeof(Vec4f);
    bDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.PickResultBuf);

    if(FAILED(hr))
      RDCERR("Failed to create mesh pick result buff %08x", hr);

    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = DebugRenderData::maxMeshPicks;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.PickResultBuf, &uavDesc,
                                              &m_DebugRender.PickResultUAV);

    if(FAILED(hr))
      RDCERR("Failed to create mesh pick result UAV %08x", hr);

    // created/sized on demand
    m_DebugRender.PickIBBuf = m_DebugRender.PickVBBuf = NULL;
    m_DebugRender.PickIBSRV = m_DebugRender.PickVBSRV = NULL;
    m_DebugRender.PickIBSize = m_DebugRender.PickVBSize = 0;
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    D3D11_BUFFER_DESC desc;

    desc.StructureByteStride = 0;
    desc.ByteWidth = STAGE_BUFFER_BYTE_SIZE;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.StageBuffer);

    if(FAILED(hr))
      RDCERR("Failed to create map staging buffer %08x", hr);
  }

  return true;
}

void D3D11DebugManager::ShutdownFontRendering()
{
}

void D3D11DebugManager::ShutdownStreamOut()
{
  SAFE_RELEASE(m_SOBuffer);
  for(ID3D11Query *q : m_SOStatsQueries)
    SAFE_RELEASE(q);
  SAFE_RELEASE(m_SOStagingBuffer);

  SAFE_RELEASE(m_WireframeHelpersRS);
  SAFE_RELEASE(m_WireframeHelpersCullCCWRS);
  SAFE_RELEASE(m_WireframeHelpersCullCWRS);
  SAFE_RELEASE(m_WireframeHelpersBS);
  SAFE_RELEASE(m_SolidHelpersRS);

  SAFE_RELEASE(m_MeshDisplayLayout);

  SAFE_RELEASE(m_FrustumHelper);
  SAFE_RELEASE(m_AxisHelper);
  SAFE_RELEASE(m_TriHighlightHelper);
}

bool D3D11DebugManager::InitStreamOut()
{
  CreateSOBuffers();

  m_MeshDisplayLayout = NULL;

  HRESULT hr = S_OK;

  D3D11_QUERY_DESC qdesc;
  qdesc.MiscFlags = 0;
  qdesc.Query = D3D11_QUERY_SO_STATISTICS;

  m_SOStatsQueries.push_back(NULL);
  hr = m_pDevice->CreateQuery(&qdesc, &m_SOStatsQueries[0]);
  if(FAILED(hr))
    RDCERR("Failed to create m_SOStatsQuery %08x", hr);

  {
    D3D11_RASTERIZER_DESC desc;
    desc.AntialiasedLineEnable = TRUE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.DepthClipEnable = FALSE;
    desc.FrontCounterClockwise = FALSE;
    desc.MultisampleEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.FillMode = D3D11_FILL_WIREFRAME;
    desc.CullMode = D3D11_CULL_NONE;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersRS %08x", hr);

    desc.FrontCounterClockwise = TRUE;
    desc.CullMode = D3D11_CULL_FRONT;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersCullCCWRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersCullCCWRS %08x", hr);

    desc.FrontCounterClockwise = FALSE;
    desc.CullMode = D3D11_CULL_FRONT;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersCullCWRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersCullCCWRS %08x", hr);

    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_SolidHelpersRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_SolidHelpersRS %08x", hr);
  }

  {
    D3D11_BLEND_DESC desc;
    RDCEraseEl(desc);

    desc.AlphaToCoverageEnable = TRUE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].RenderTargetWriteMask = 0xf;

    hr = m_pDevice->CreateBlendState(&desc, &m_WireframeHelpersBS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersRS %08x", hr);
  }

  {
    Vec4f axisVB[6] = {
        Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
        Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f),
    };

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = axisVB;
    data.SysMemPitch = data.SysMemSlicePitch = 0;

    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.ByteWidth = sizeof(axisVB);
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_IMMUTABLE;

    hr = m_pDevice->CreateBuffer(&bdesc, &data, &m_AxisHelper);
    if(FAILED(hr))
      RDCERR("Failed to create m_AxisHelper %08x", hr);
  }

  {
    Vec4f TLN = Vec4f(-1.0f, 1.0f, 0.0f, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
    Vec4f BLN = Vec4f(-1.0f, -1.0f, 0.0f, 1.0f);
    Vec4f BRN = Vec4f(1.0f, -1.0f, 0.0f, 1.0f);

    Vec4f TLF = Vec4f(-1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f TRF = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f BLF = Vec4f(-1.0f, -1.0f, 1.0f, 1.0f);
    Vec4f BRF = Vec4f(1.0f, -1.0f, 1.0f, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f axisVB[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = axisVB;
    data.SysMemPitch = data.SysMemSlicePitch = 0;

    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.ByteWidth = sizeof(axisVB);
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_IMMUTABLE;

    hr = m_pDevice->CreateBuffer(&bdesc, &data, &m_FrustumHelper);

    if(FAILED(hr))
      RDCERR("Failed to create m_FrustumHelper %08x", hr);
  }

  {
    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bdesc.ByteWidth = sizeof(Vec4f) * 24;
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_DYNAMIC;

    hr = m_pDevice->CreateBuffer(&bdesc, NULL, &m_TriHighlightHelper);

    if(FAILED(hr))
      RDCERR("Failed to create m_TriHighlightHelper %08x", hr);
  }

  return true;
}

void D3D11DebugManager::CreateSOBuffers()
{
  HRESULT hr = S_OK;

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);

  D3D11_BUFFER_DESC bufferDesc = {
      m_SOBufferSize, D3D11_USAGE_DEFAULT, D3D11_BIND_STREAM_OUTPUT, 0, 0, 0};

  hr = m_pDevice->CreateBuffer(&bufferDesc, NULL, &m_SOBuffer);

  if(FAILED(hr))
    RDCERR("Failed to create m_SOBuffer %08x", hr);

  bufferDesc.Usage = D3D11_USAGE_STAGING;
  bufferDesc.BindFlags = 0;
  bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  hr = m_pDevice->CreateBuffer(&bufferDesc, NULL, &m_SOStagingBuffer);
  if(FAILED(hr))
    RDCERR("Failed to create m_SOStagingBuffer %08x", hr);
}

bool D3D11DebugManager::InitFontRendering()
{
  HRESULT hr = S_OK;

  D3D11_TEXTURE2D_DESC desc;
  RDCEraseEl(desc);

  int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

  desc.ArraySize = 1;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = 0;
  desc.Format = DXGI_FORMAT_R8_UNORM;
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.MiscFlags = 0;
  desc.SampleDesc.Quality = 0;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;

  D3D11_SUBRESOURCE_DATA initialData;

  string font = GetEmbeddedResource(sourcecodepro_ttf);
  byte *ttfdata = (byte *)font.c_str();

  const int firstChar = int(' ') + 1;
  const int lastChar = 127;
  const int numChars = lastChar - firstChar;

  byte *buf = new byte[width * height];

  const float pixelHeight = 20.0f;

  stbtt_bakedchar chardata[numChars];
  stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars, chardata);

  m_Font.CharSize = pixelHeight;
  m_Font.CharAspect = chardata->xadvance / pixelHeight;

  stbtt_fontinfo f = {0};
  stbtt_InitFont(&f, ttfdata, 0);

  int ascent = 0;
  stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

  float maxheight = float(ascent) * stbtt_ScaleForPixelHeight(&f, pixelHeight);

  initialData.pSysMem = buf;
  initialData.SysMemPitch = width;
  initialData.SysMemSlicePitch = width * height;

  ID3D11Texture2D *debugTex = NULL;

  hr = m_pDevice->CreateTexture2D(&desc, &initialData, &debugTex);

  if(FAILED(hr))
    RDCERR("Failed to create debugTex %08x", hr);

  delete[] buf;

  hr = m_pDevice->CreateShaderResourceView(debugTex, NULL, &m_Font.Tex);

  if(FAILED(hr))
    RDCERR("Failed to create m_Font.Tex %08x", hr);

  SAFE_RELEASE(debugTex);

  Vec4f glyphData[2 * (numChars + 1)];

  m_Font.GlyphData = MakeCBuffer(sizeof(glyphData));

  for(int i = 0; i < numChars; i++)
  {
    stbtt_bakedchar *b = chardata + i;

    float x = b->xoff;
    float y = b->yoff + maxheight;

    glyphData[(i + 1) * 2 + 0] =
        Vec4f(x / b->xadvance, y / pixelHeight, b->xadvance / float(b->x1 - b->x0),
              pixelHeight / float(b->y1 - b->y0));
    glyphData[(i + 1) * 2 + 1] = Vec4f(b->x0, b->y0, b->x1, b->y1);
  }

  FillCBuffer(m_Font.GlyphData, &glyphData, sizeof(glyphData));

  m_Font.CBuffer = MakeCBuffer(sizeof(FontCBuffer));
  m_Font.CharBuffer = MakeCBuffer((2 + FONT_MAX_CHARS) * sizeof(uint32_t) * 4);

  string fullhlsl = "";
  {
    string debugShaderCBuf = GetEmbeddedResource(debugcbuffers_h);
    string textShaderHLSL = GetEmbeddedResource(debugtext_hlsl);

    fullhlsl = debugShaderCBuf + textShaderHLSL;
  }

  m_Font.VS = MakeVShader(fullhlsl.c_str(), "RENDERDOC_TextVS", "vs_4_0");
  m_Font.PS = MakePShader(fullhlsl.c_str(), "RENDERDOC_TextPS", "ps_4_0");

  return true;
}

void D3D11DebugManager::SetOutputWindow(HWND w)
{
  RECT rect = {0, 0, 0, 0};
  GetClientRect(w, &rect);
  if(rect.right == rect.left || rect.bottom == rect.top)
  {
    m_supersamplingX = 1.0f;
    m_supersamplingY = 1.0f;
  }
  else
  {
    m_supersamplingX = float(m_width) / float(rect.right - rect.left);
    m_supersamplingY = float(m_height) / float(rect.bottom - rect.top);
  }
}

void D3D11DebugManager::OutputWindow::MakeRTV()
{
  ID3D11Texture2D *texture = NULL;
  HRESULT hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to get swap chain buffer, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(texture);
    return;
  }

  hr = dev->CreateRenderTargetView(texture, NULL, &rtv);

  SAFE_RELEASE(texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RTV for swap chain buffer, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    return;
  }
}

void D3D11DebugManager::OutputWindow::MakeDSV()
{
  ID3D11Texture2D *texture = NULL;
  HRESULT hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to get swap chain buffer, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(texture);
    return;
  }

  D3D11_TEXTURE2D_DESC texDesc;
  texture->GetDesc(&texDesc);

  SAFE_RELEASE(texture);

  texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

  hr = dev->CreateTexture2D(&texDesc, NULL, &texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV texture for main output, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(rtv);
    return;
  }

  hr = dev->CreateDepthStencilView(texture, NULL, &dsv);

  SAFE_RELEASE(texture);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV for main output, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(rtv);
    return;
  }
}

uint64_t D3D11DebugManager::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  RDCASSERT(system == WindowingSystem::Win32, system);

  OutputWindow outw;
  outw.wnd = (HWND)data;
  outw.dev = m_WrappedDevice;

  DXGI_SWAP_CHAIN_DESC swapDesc;
  RDCEraseEl(swapDesc);

  RECT rect;
  GetClientRect(outw.wnd, &rect);

  swapDesc.BufferCount = 2;
  swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  outw.width = swapDesc.BufferDesc.Width = rect.right - rect.left;
  outw.height = swapDesc.BufferDesc.Height = rect.bottom - rect.top;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.SampleDesc.Count = depth ? 4 : 1;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.OutputWindow = outw.wnd;
  swapDesc.Windowed = TRUE;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  swapDesc.Flags = 0;

  HRESULT hr = S_OK;

  hr = m_pFactory->CreateSwapChain(m_pDevice, &swapDesc, &outw.swap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create swap chain for HWND, HRESULT: 0x%08x", hr);
    return 0;
  }

  outw.MakeRTV();

  outw.dsv = NULL;
  if(depth)
    outw.MakeDSV();

  uint64_t id = m_OutputWindowID++;
  m_OutputWindows[id] = outw;
  return id;
}

void D3D11DebugManager::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  SAFE_RELEASE(outw.swap);
  SAFE_RELEASE(outw.rtv);
  SAFE_RELEASE(outw.dsv);

  m_OutputWindows.erase(it);
}

bool D3D11DebugManager::CheckResizeOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.wnd == NULL || outw.swap == NULL)
    return false;

  RECT rect;
  GetClientRect(outw.wnd, &rect);
  long w = rect.right - rect.left;
  long h = rect.bottom - rect.top;

  if(w != outw.width || h != outw.height)
  {
    outw.width = w;
    outw.height = h;

    D3D11RenderStateTracker tracker(m_WrappedContext);

    m_pImmediateContext->OMSetRenderTargets(0, 0, 0);

    if(outw.width > 0 && outw.height > 0)
    {
      SAFE_RELEASE(outw.rtv);
      SAFE_RELEASE(outw.dsv);

      DXGI_SWAP_CHAIN_DESC desc;
      outw.swap->GetDesc(&desc);

      HRESULT hr = outw.swap->ResizeBuffers(desc.BufferCount, outw.width, outw.height,
                                            desc.BufferDesc.Format, desc.Flags);

      if(FAILED(hr))
      {
        RDCERR("Failed to resize swap chain, HRESULT: 0x%08x", hr);
        return true;
      }

      outw.MakeRTV();
      outw.MakeDSV();
    }

    return true;
  }

  return false;
}

void D3D11DebugManager::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  w = m_OutputWindows[id].width;
  h = m_OutputWindows[id].height;
}

void D3D11DebugManager::ClearOutputWindowColor(uint64_t id, float col[4])
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  m_pImmediateContext->ClearRenderTargetView(m_OutputWindows[id].rtv, col);
}

void D3D11DebugManager::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_OutputWindows[id].dsv)
    m_pImmediateContext->ClearDepthStencilView(
        m_OutputWindows[id].dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void D3D11DebugManager::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_RealState.active)
    RDCERR("Trashing RealState! Mismatched use of BindOutputWindow / FlipOutputWindow");

  m_RealState.active = true;
  m_RealState.state.CopyState(*m_WrappedContext->GetCurrentPipelineState());

  m_pImmediateContext->OMSetRenderTargets(
      1, &m_OutputWindows[id].rtv, depth && m_OutputWindows[id].dsv ? m_OutputWindows[id].dsv : NULL);

  D3D11_VIEWPORT viewport = {
      0, 0, (float)m_OutputWindows[id].width, (float)m_OutputWindows[id].height, 0.0f, 1.0f};
  m_pImmediateContext->RSSetViewports(1, &viewport);

  SetOutputDimensions(m_OutputWindows[id].width, m_OutputWindows[id].height);
}

bool D3D11DebugManager::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

void D3D11DebugManager::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  if(m_OutputWindows[id].swap)
    m_OutputWindows[id].swap->Present(0, 0);

  if(m_RealState.active)
  {
    m_RealState.active = false;
    m_RealState.state.ApplyState(m_WrappedContext);
    m_RealState.state.Clear();
  }
  else
  {
    RDCERR("RealState wasn't active! Mismatched use of BindOutputWindow / FlipOutputWindow");
  }
}

uint32_t D3D11DebugManager::GetStructCount(ID3D11UnorderedAccessView *uav)
{
  m_pImmediateContext->CopyStructureCount(m_DebugRender.StageBuffer, 0, uav);

  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = m_pImmediateContext->Map(m_DebugRender.StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to Map %08x", hr);
    return ~0U;
  }

  uint32_t ret = *((uint32_t *)mapped.pData);

  m_pImmediateContext->Unmap(m_DebugRender.StageBuffer, 0);

  return ret;
}

bool D3D11DebugManager::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip,
                                     uint32_t sample, CompType typeHint, float minval, float maxval,
                                     bool channels[4], vector<uint32_t> &histogram)
{
  if(minval >= maxval)
    return false;

  TextureShaderDetails details = GetShaderDetails(texid, typeHint, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_WrappedContext);

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

  ID3D11Buffer *cbuf = MakeCBuffer(&cdata, sizeof(cdata));

  UINT zeroes[] = {0, 0, 0, 0};
  m_pImmediateContext->ClearUnorderedAccessViewUint(m_DebugRender.histogramUAV, zeroes);

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {0};
  UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&UAV_keepcounts[0], 0xff, sizeof(UAV_keepcounts));

  const UINT numUAVs =
      m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  uavs[0] = m_DebugRender.histogramUAV;
  m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, uavs, UAV_keepcounts);

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &cbuf);

  m_pImmediateContext->CSSetShaderResources(srvOffset, eTexType_Max, details.srv);

  ID3D11SamplerState *samps[] = {m_DebugRender.PointSampState, m_DebugRender.LinearSampState};
  m_pImmediateContext->CSSetSamplers(0, 2, samps);

  m_pImmediateContext->CSSetShader(m_DebugRender.HistogramCS[details.texType][intIdx], NULL, 0);

  int tilesX = (int)ceil(cdata.HistogramTextureResolution.x /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int tilesY = (int)ceil(cdata.HistogramTextureResolution.y /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  m_pImmediateContext->Dispatch(tilesX, tilesY, 1);

  m_pImmediateContext->CopyResource(m_DebugRender.histogramStageBuff, m_DebugRender.histogramBuff);

  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr =
      m_pImmediateContext->Map(m_DebugRender.histogramStageBuff, 0, D3D11_MAP_READ, 0, &mapped);

  histogram.clear();
  histogram.resize(HGRAM_NUM_BUCKETS);

  if(FAILED(hr))
  {
    RDCERR("Can't map histogram stage buff %08x", hr);
  }
  else
  {
    memcpy(&histogram[0], mapped.pData, sizeof(uint32_t) * HGRAM_NUM_BUCKETS);

    m_pImmediateContext->Unmap(m_DebugRender.histogramStageBuff, 0);
  }

  return true;
}

bool D3D11DebugManager::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip,
                                  uint32_t sample, CompType typeHint, float *minval, float *maxval)
{
  TextureShaderDetails details = GetShaderDetails(texid, typeHint, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_WrappedContext);

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

  ID3D11Buffer *cbuf = MakeCBuffer(&cdata, sizeof(cdata));

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &cbuf);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {NULL};
  const UINT numUAVs =
      m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  uavs[intIdx] = m_DebugRender.tileResultUAV[intIdx];
  m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, uavs, NULL);

  m_pImmediateContext->CSSetShaderResources(srvOffset, eTexType_Max, details.srv);

  ID3D11SamplerState *samps[] = {m_DebugRender.PointSampState, m_DebugRender.LinearSampState};
  m_pImmediateContext->CSSetSamplers(0, 2, samps);

  m_pImmediateContext->CSSetShader(m_DebugRender.TileMinMaxCS[details.texType][intIdx], NULL, 0);

  int blocksX = (int)ceil(cdata.HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata.HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  m_pImmediateContext->Dispatch(blocksX, blocksY, 1);

  m_pImmediateContext->CSSetUnorderedAccessViews(intIdx, 1, &m_DebugRender.resultUAV[intIdx], NULL);
  m_pImmediateContext->CSSetShaderResources(intIdx, 1, &m_DebugRender.tileResultSRV[intIdx]);

  m_pImmediateContext->CSSetShader(m_DebugRender.ResultMinMaxCS[intIdx], NULL, 0);

  m_pImmediateContext->Dispatch(1, 1, 1);

  m_pImmediateContext->CopyResource(m_DebugRender.resultStageBuff, m_DebugRender.resultBuff);

  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->Map(m_DebugRender.resultStageBuff, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map minmax results buffer %08x", hr);
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

    m_pImmediateContext->Unmap(m_DebugRender.resultStageBuff, 0);
  }

  return true;
}

void D3D11DebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length,
                                      vector<byte> &retData)
{
  auto it = WrappedID3D11Buffer::m_BufferList.find(buff);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
  {
    RDCERR("Getting buffer data for unknown buffer %llu!", buff);
    return;
  }

  ID3D11Buffer *buffer = it->second.m_Buffer;

  RDCASSERT(buffer);

  GetBufferData(buffer, offset, length, retData);
}

void D3D11DebugManager::GetBufferData(ID3D11Buffer *buffer, uint64_t offset, uint64_t length,
                                      vector<byte> &ret)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  if(buffer == NULL)
    return;

  RDCASSERT(offset < 0xffffffff);
  RDCASSERT(length <= 0xffffffff);

  uint32_t offs = (uint32_t)offset;
  uint32_t len = (uint32_t)length;

  D3D11_BUFFER_DESC desc;
  buffer->GetDesc(&desc);

  if(offs >= desc.ByteWidth)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(len == 0)
  {
    len = desc.ByteWidth - offs;
  }

  if(len > 0 && offs + len > desc.ByteWidth)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%u)",
            offset, length, desc.ByteWidth);
    len = RDCMIN(len, desc.ByteWidth - offs);
  }

  uint32_t outOffs = 0;

  ret.resize(len);

  D3D11_BOX box;
  box.top = 0;
  box.bottom = 1;
  box.front = 0;
  box.back = 1;

  while(len > 0)
  {
    uint32_t chunkSize = RDCMIN(len, STAGE_BUFFER_BYTE_SIZE);

    if(desc.StructureByteStride > 0)
      chunkSize -= (chunkSize % desc.StructureByteStride);

    box.left = RDCMIN(offs + outOffs, desc.ByteWidth);
    box.right = RDCMIN(offs + outOffs + chunkSize, desc.ByteWidth);

    if(box.right - box.left == 0)
      break;

    m_pImmediateContext->CopySubresourceRegion(m_DebugRender.StageBuffer, 0, 0, 0, 0, buffer, 0,
                                               &box);

    HRESULT hr = m_pImmediateContext->Map(m_DebugRender.StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map bufferdata buffer %08x", hr);
      return;
    }
    else
    {
      memcpy(&ret[outOffs], mapped.pData, RDCMIN(len, STAGE_BUFFER_BYTE_SIZE));

      m_pImmediateContext->Unmap(m_DebugRender.StageBuffer, 0);
    }

    outOffs += chunkSize;
    len -= chunkSize;
  }
}

void D3D11DebugManager::CopyArrayToTex2DMS(ID3D11Texture2D *destMS, ID3D11Texture2D *srcArray)
{
  // unlike CopyTex2DMSToArray we can use the wrapped context here, but for consistency
  // we accept unwrapped parameters.

  D3D11RenderStateTracker tracker(m_WrappedContext);

  // copy to textures with right bind flags for operation
  D3D11_TEXTURE2D_DESC descArr;
  srcArray->GetDesc(&descArr);

  D3D11_TEXTURE2D_DESC descMS;
  destMS->GetDesc(&descMS);

  bool depth = IsDepthFormat(descMS.Format);

  ID3D11Texture2D *rtvResource = NULL;
  ID3D11Texture2D *srvResource = NULL;

  D3D11_TEXTURE2D_DESC rtvResDesc = descMS;
  D3D11_TEXTURE2D_DESC srvResDesc = descArr;

  rtvResDesc.BindFlags = depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
  srvResDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  if(depth)
  {
    rtvResDesc.Format = GetTypelessFormat(rtvResDesc.Format);
    srvResDesc.Format = GetTypelessFormat(srvResDesc.Format);
  }

  rtvResDesc.Usage = D3D11_USAGE_DEFAULT;
  srvResDesc.Usage = D3D11_USAGE_DEFAULT;

  rtvResDesc.CPUAccessFlags = 0;
  srvResDesc.CPUAccessFlags = 0;

  HRESULT hr = S_OK;

  hr = m_pDevice->CreateTexture2D(&rtvResDesc, NULL, &rtvResource);
  if(FAILED(hr))
  {
    RDCERR("0x%08x", hr);
    return;
  }

  hr = m_pDevice->CreateTexture2D(&srvResDesc, NULL, &srvResource);
  if(FAILED(hr))
  {
    RDCERR("0x%08x", hr);
    return;
  }

  m_WrappedContext->GetReal()->CopyResource(UNWRAP(WrappedID3D11Texture2D1, srvResource), srcArray);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {NULL};
  const UINT numUAVs =
      m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  UINT uavCounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&uavCounts[0], 0xff, sizeof(uavCounts));

  m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, uavs, uavCounts);

  m_pImmediateContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
  m_pImmediateContext->PSSetShader(
      depth ? m_DebugRender.DepthCopyArrayToMSPS : m_DebugRender.CopyArrayToMSPS, NULL, 0);

  m_pImmediateContext->HSSetShader(NULL, NULL, 0);
  m_pImmediateContext->DSSetShader(NULL, NULL, 0);
  m_pImmediateContext->GSSetShader(NULL, NULL, 0);

  D3D11_VIEWPORT view = {0.0f, 0.0f, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f};

  m_pImmediateContext->RSSetState(m_DebugRender.RastState);
  m_pImmediateContext->RSSetViewports(1, &view);

  m_pImmediateContext->IASetInputLayout(NULL);
  m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  float blendFactor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  m_pImmediateContext->OMSetBlendState(NULL, blendFactor, ~0U);

  if(depth)
  {
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.StencilEnable = FALSE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);
    m_pImmediateContext->OMSetDepthStencilState(dsState, 0);
    SAFE_RELEASE(dsState);
  }
  else
  {
    m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, 0);
  }

  ID3D11DepthStencilView *dsvMS = NULL;
  ID3D11RenderTargetView *rtvMS = NULL;
  ID3D11ShaderResourceView *srvArray = NULL;

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
  rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
  rtvDesc.Format =
      depth ? GetUIntTypedFormat(descMS.Format) : GetTypedFormat(descMS.Format, CompType::UInt);
  rtvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
  rtvDesc.Texture2DMSArray.FirstArraySlice = 0;

  D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
  dsvDesc.Flags = 0;
  dsvDesc.Format = GetDepthTypedFormat(descMS.Format);
  dsvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
  dsvDesc.Texture2DMSArray.FirstArraySlice = 0;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc.Format =
      depth ? GetUIntTypedFormat(descArr.Format) : GetTypedFormat(descArr.Format, CompType::UInt);
  srvDesc.Texture2DArray.ArraySize = descArr.ArraySize;
  srvDesc.Texture2DArray.FirstArraySlice = 0;
  srvDesc.Texture2DArray.MipLevels = descArr.MipLevels;
  srvDesc.Texture2DArray.MostDetailedMip = 0;

  bool stencil = false;
  DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

  if(depth)
  {
    switch(descArr.Format)
    {
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_FLOAT; break;
    }
  }

  hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvArray);
  if(FAILED(hr))
  {
    RDCERR("0x%08x", hr);
    return;
  }

  ID3D11ShaderResourceView *srvs[8] = {NULL};
  srvs[0] = srvArray;

  m_pImmediateContext->PSSetShaderResources(0, 8, srvs);

  // loop over every array slice in MS texture
  for(UINT slice = 0; slice < descMS.ArraySize; slice++)
  {
    uint32_t cdata[4] = {descMS.SampleDesc.Count, 1000, 0, slice};

    ID3D11Buffer *cbuf = MakeCBuffer(cdata, sizeof(cdata));

    m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

    rtvDesc.Texture2DMSArray.FirstArraySlice = slice;
    rtvDesc.Texture2DMSArray.ArraySize = 1;
    dsvDesc.Texture2DMSArray.FirstArraySlice = slice;
    dsvDesc.Texture2DMSArray.ArraySize = 1;

    if(depth)
      hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvMS);
    else
      hr = m_pDevice->CreateRenderTargetView(rtvResource, &rtvDesc, &rtvMS);
    if(FAILED(hr))
    {
      SAFE_RELEASE(srvArray);
      RDCERR("0x%08x", hr);
      return;
    }

    if(depth)
      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvMS, 0, 0, NULL,
                                                                     NULL);
    else
      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtvMS, NULL, 0, 0, NULL,
                                                                     NULL);

    m_pImmediateContext->Draw(3, 0);

    SAFE_RELEASE(rtvMS);
    SAFE_RELEASE(dsvMS);
  }

  SAFE_RELEASE(srvArray);

  if(stencil)
  {
    srvDesc.Format = stencilFormat;

    hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvArray);
    if(FAILED(hr))
    {
      RDCERR("0x%08x", hr);
      return;
    }

    m_pImmediateContext->PSSetShaderResources(1, 1, &srvArray);

    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.StencilEnable = TRUE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
    dsvDesc.Texture2DArray.ArraySize = 1;

    m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);

    // loop over every array slice in MS texture
    for(UINT slice = 0; slice < descMS.ArraySize; slice++)
    {
      dsvDesc.Texture2DMSArray.FirstArraySlice = slice;

      hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvMS);
      if(FAILED(hr))
      {
        SAFE_RELEASE(srvArray);
        SAFE_RELEASE(dsState);
        RDCERR("0x%08x", hr);
        return;
      }

      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvMS, 0, 0, NULL,
                                                                     NULL);

      // loop over every stencil value (zzzzzz, no shader stencil read/write)
      for(UINT stencilval = 0; stencilval < 256; stencilval++)
      {
        uint32_t cdata[4] = {descMS.SampleDesc.Count, stencilval, 0, slice};

        ID3D11Buffer *cbuf = MakeCBuffer(cdata, sizeof(cdata));

        m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

        m_pImmediateContext->OMSetDepthStencilState(dsState, stencilval);

        m_pImmediateContext->Draw(3, 0);
      }

      SAFE_RELEASE(dsvMS);
    }

    SAFE_RELEASE(srvArray);
    SAFE_RELEASE(dsState);
  }

  m_WrappedContext->GetReal()->CopyResource(destMS, UNWRAP(WrappedID3D11Texture2D1, rtvResource));

  SAFE_RELEASE(rtvResource);
  SAFE_RELEASE(srvResource);
}

struct Tex2DMSToArrayStateTracker
{
  Tex2DMSToArrayStateTracker(WrappedID3D11DeviceContext *wrappedContext)
  {
    m_WrappedContext = wrappedContext;
    D3D11RenderState *rs = wrappedContext->GetCurrentPipelineState();

    // first copy the properties. We don't need to keep refs as the objects won't be deleted by
    // being unbound and we won't do anything with them
    Layout = rs->IA.Layout;
    memcpy(&VS, &rs->VS, sizeof(VS));
    memcpy(&PS, &rs->PS, sizeof(PS));

    memcpy(CSUAVs, rs->CSUAVs, sizeof(CSUAVs));

    memcpy(&RS, &rs->RS, sizeof(RS));
    memcpy(&OM, &rs->OM, sizeof(OM));

    RDCCOMPILE_ASSERT(sizeof(VS) == sizeof(rs->VS), "Struct sizes have changed, ensure full copy");
    RDCCOMPILE_ASSERT(sizeof(RS) == sizeof(rs->RS), "Struct sizes have changed, ensure full copy");
    RDCCOMPILE_ASSERT(sizeof(OM) == sizeof(rs->OM), "Struct sizes have changed, ensure full copy");

    // now unwrap everything in place.
    Layout = UNWRAP(WrappedID3D11InputLayout, Layout);
    VS.Shader = UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, VS.Shader);
    PS.Shader = UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, PS.Shader);

    // only need to save/restore constant buffer 0
    PS.ConstantBuffers[0] = UNWRAP(WrappedID3D11Buffer, PS.ConstantBuffers[0]);

    // same for the first 8 SRVs
    for(int i = 0; i < 8; i++)
      PS.SRVs[i] = UNWRAP(WrappedID3D11ShaderResourceView1, PS.SRVs[i]);

    for(int i = 0; i < D3D11_SHADER_MAX_INTERFACES; i++)
    {
      VS.Instances[i] = UNWRAP(WrappedID3D11ClassInstance, VS.Instances[i]);
      PS.Instances[i] = UNWRAP(WrappedID3D11ClassInstance, PS.Instances[i]);
    }

    for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
      CSUAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, CSUAVs[i]);

    RS.State = UNWRAP(WrappedID3D11RasterizerState2, RS.State);
    OM.DepthStencilState = UNWRAP(WrappedID3D11DepthStencilState, OM.DepthStencilState);
    OM.BlendState = UNWRAP(WrappedID3D11BlendState1, OM.BlendState);
    OM.DepthView = UNWRAP(WrappedID3D11DepthStencilView, OM.DepthView);

    for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      OM.RenderTargets[i] = UNWRAP(WrappedID3D11RenderTargetView1, OM.RenderTargets[i]);

    for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
      OM.UAVs[i] = UNWRAP(WrappedID3D11UnorderedAccessView1, OM.UAVs[i]);
  }
  ~Tex2DMSToArrayStateTracker()
  {
    ID3D11DeviceContext *context = m_WrappedContext->GetReal();
    ID3D11DeviceContext1 *context1 = m_WrappedContext->GetReal1();

    context->IASetInputLayout(Layout);
    context->VSSetShader((ID3D11VertexShader *)VS.Shader, VS.Instances, VS.NumInstances);

    context->PSSetShaderResources(0, 8, PS.SRVs);
    context->PSSetShader((ID3D11PixelShader *)PS.Shader, PS.Instances, PS.NumInstances);

    if(m_WrappedContext->IsFL11_1())
      context1->PSSetConstantBuffers1(0, 1, PS.ConstantBuffers, PS.CBOffsets, PS.CBCounts);
    else
      context->PSSetConstantBuffers(0, 1, PS.ConstantBuffers);

    UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1,
                                                   (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};

    if(m_WrappedContext->IsFL11_1())
      context->CSSetUnorderedAccessViews(0, D3D11_1_UAV_SLOT_COUNT, CSUAVs, UAV_keepcounts);
    else
      context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, CSUAVs, UAV_keepcounts);

    context->RSSetState(RS.State);
    context->RSSetViewports(RS.NumViews, RS.Viewports);

    context->OMSetBlendState(OM.BlendState, OM.BlendFactor, OM.SampleMask);
    context->OMSetDepthStencilState(OM.DepthStencilState, OM.StencRef);

    if(m_WrappedContext->IsFL11_1())
      context->OMSetRenderTargetsAndUnorderedAccessViews(
          OM.UAVStartSlot, OM.RenderTargets, OM.DepthView, OM.UAVStartSlot,
          D3D11_1_UAV_SLOT_COUNT - OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
    else
      context->OMSetRenderTargetsAndUnorderedAccessViews(
          OM.UAVStartSlot, OM.RenderTargets, OM.DepthView, OM.UAVStartSlot,
          D3D11_PS_CS_UAV_REGISTER_COUNT - OM.UAVStartSlot, OM.UAVs, UAV_keepcounts);
  }

  WrappedID3D11DeviceContext *m_WrappedContext;

  ID3D11InputLayout *Layout;

  struct shader
  {
    ID3D11DeviceChild *Shader;
    ID3D11Buffer *ConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    UINT CBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    UINT CBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    ID3D11SamplerState *Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11ClassInstance *Instances[D3D11_SHADER_MAX_INTERFACES];
    UINT NumInstances;

    bool Used_CB(uint32_t slot) const;
    bool Used_SRV(uint32_t slot) const;
    bool Used_UAV(uint32_t slot) const;
  } VS, PS;

  ID3D11UnorderedAccessView *CSUAVs[D3D11_1_UAV_SLOT_COUNT];

  struct rasterizer
  {
    UINT NumViews, NumScissors;
    D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    ID3D11RasterizerState *State;
  } RS;

  struct outmerger
  {
    ID3D11DepthStencilState *DepthStencilState;
    UINT StencRef;

    ID3D11BlendState *BlendState;
    FLOAT BlendFactor[4];
    UINT SampleMask;

    ID3D11DepthStencilView *DepthView;

    ID3D11RenderTargetView *RenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

    UINT UAVStartSlot;
    ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT];
  } OM;
};

void D3D11DebugManager::CopyTex2DMSToArray(ID3D11Texture2D *destArray, ID3D11Texture2D *srcMS)
{
  // we have to use exclusively the unwrapped context here as this might be happening during
  // capture and we don't want to serialise any of this work, and the parameters might not exist
  // as wrapped objects for that reason

  // use the wrapped context's state tracked to avoid needing our own tracking, just restore it to
  // the unwrapped context
  Tex2DMSToArrayStateTracker tracker(m_WrappedContext);

  ID3D11Device *dev = m_WrappedDevice->GetReal();
  ID3D11DeviceContext *ctx = m_WrappedContext->GetReal();

  // copy to textures with right bind flags for operation
  D3D11_TEXTURE2D_DESC descMS;
  srcMS->GetDesc(&descMS);

  D3D11_TEXTURE2D_DESC descArr;
  destArray->GetDesc(&descArr);

  ID3D11Texture2D *rtvResource = NULL;
  ID3D11Texture2D *srvResource = NULL;

  D3D11_TEXTURE2D_DESC rtvResDesc = descArr;
  D3D11_TEXTURE2D_DESC srvResDesc = descMS;

  bool depth = IsDepthFormat(descMS.Format);

  rtvResDesc.BindFlags = depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
  srvResDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  if(depth)
  {
    rtvResDesc.Format = GetTypelessFormat(rtvResDesc.Format);
    srvResDesc.Format = GetTypelessFormat(srvResDesc.Format);
  }

  rtvResDesc.Usage = D3D11_USAGE_DEFAULT;
  srvResDesc.Usage = D3D11_USAGE_DEFAULT;

  rtvResDesc.CPUAccessFlags = 0;
  srvResDesc.CPUAccessFlags = 0;

  HRESULT hr = S_OK;

  hr = dev->CreateTexture2D(&rtvResDesc, NULL, &rtvResource);
  if(FAILED(hr))
  {
    RDCERR("0x%08x", hr);
    return;
  }

  hr = dev->CreateTexture2D(&srvResDesc, NULL, &srvResource);
  if(FAILED(hr))
  {
    RDCERR("0x%08x", hr);
    return;
  }

  ctx->CopyResource(srvResource, srcMS);

  ID3D11UnorderedAccessView *uavs[D3D11_1_UAV_SLOT_COUNT] = {NULL};
  UINT uavCounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&uavCounts[0], 0xff, sizeof(uavCounts));
  const UINT numUAVs =
      m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;

  ctx->CSSetUnorderedAccessViews(0, numUAVs, uavs, uavCounts);

  ctx->VSSetShader(UNWRAP(WrappedID3D11Shader<ID3D11VertexShader>, m_DebugRender.FullscreenVS),
                   NULL, 0);
  ctx->PSSetShader(
      depth ? UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, m_DebugRender.DepthCopyMSToArrayPS)
            : UNWRAP(WrappedID3D11Shader<ID3D11PixelShader>, m_DebugRender.CopyMSToArrayPS),
      NULL, 0);

  D3D11_VIEWPORT view = {0.0f, 0.0f, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f};

  ctx->RSSetState(UNWRAP(WrappedID3D11RasterizerState2, m_DebugRender.RastState));
  ctx->RSSetViewports(1, &view);

  ctx->IASetInputLayout(NULL);
  float blendFactor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  ctx->OMSetBlendState(NULL, blendFactor, ~0U);

  if(depth)
  {
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.StencilEnable = FALSE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    dev->CreateDepthStencilState(&dsDesc, &dsState);
    ctx->OMSetDepthStencilState(dsState, 0);
    SAFE_RELEASE(dsState);
  }
  else
  {
    ctx->OMSetDepthStencilState(
        UNWRAP(WrappedID3D11DepthStencilState, m_DebugRender.AllPassDepthState), 0);
  }

  ID3D11RenderTargetView *rtvArray = NULL;
  ID3D11DepthStencilView *dsvArray = NULL;
  ID3D11ShaderResourceView *srvMS = NULL;

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
  rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
  rtvDesc.Format =
      depth ? GetUIntTypedFormat(descArr.Format) : GetTypedFormat(descArr.Format, CompType::UInt);
  rtvDesc.Texture2DArray.FirstArraySlice = 0;
  rtvDesc.Texture2DArray.ArraySize = 1;
  rtvDesc.Texture2DArray.MipSlice = 0;

  D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
  dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
  dsvDesc.Format = GetDepthTypedFormat(descArr.Format);
  dsvDesc.Flags = 0;
  dsvDesc.Texture2DArray.FirstArraySlice = 0;
  dsvDesc.Texture2DArray.ArraySize = 1;
  dsvDesc.Texture2DArray.MipSlice = 0;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
  srvDesc.Format =
      depth ? GetUIntTypedFormat(descMS.Format) : GetTypedFormat(descMS.Format, CompType::UInt);
  srvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
  srvDesc.Texture2DMSArray.FirstArraySlice = 0;

  bool stencil = false;
  DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

  if(depth)
  {
    switch(descMS.Format)
    {
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;

      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        stencil = true;
        break;

      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_FLOAT; break;
    }
  }

  hr = dev->CreateShaderResourceView(srvResource, &srvDesc, &srvMS);
  if(FAILED(hr))
  {
    RDCERR("0x%08x", hr);
    return;
  }

  ID3D11ShaderResourceView *srvs[8] = {NULL};

  int srvIndex = 0;

  for(int i = 0; i < 8; i++)
    if(descMS.SampleDesc.Count == UINT(1 << i))
      srvIndex = i;

  srvs[srvIndex] = srvMS;

  ctx->PSSetShaderResources(0, 8, srvs);

  // loop over every array slice in MS texture
  for(UINT slice = 0; slice < descMS.ArraySize; slice++)
  {
    // loop over every multi sample
    for(UINT sample = 0; sample < descMS.SampleDesc.Count; sample++)
    {
      uint32_t cdata[4] = {descMS.SampleDesc.Count, 1000, sample, slice};

      ID3D11Buffer *cbuf = UNWRAP(WrappedID3D11Buffer, MakeCBuffer(cdata, sizeof(cdata)));

      ctx->PSSetConstantBuffers(0, 1, &cbuf);

      rtvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;
      dsvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;

      if(depth)
        hr = dev->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvArray);
      else
        hr = dev->CreateRenderTargetView(rtvResource, &rtvDesc, &rtvArray);

      if(FAILED(hr))
      {
        SAFE_RELEASE(rtvArray);
        SAFE_RELEASE(dsvArray);
        RDCERR("0x%08x", hr);
        return;
      }

      if(depth)
        ctx->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvArray, 0, 0, NULL, NULL);
      else
        ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtvArray, NULL, 0, 0, NULL, NULL);

      ctx->Draw(3, 0);

      SAFE_RELEASE(rtvArray);
      SAFE_RELEASE(dsvArray);
    }
  }

  SAFE_RELEASE(srvMS);

  if(stencil)
  {
    srvDesc.Format = stencilFormat;

    hr = dev->CreateShaderResourceView(srvResource, &srvDesc, &srvMS);
    if(FAILED(hr))
    {
      RDCERR("0x%08x", hr);
      return;
    }

    ctx->PSSetShaderResources(10 + srvIndex, 1, &srvMS);

    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ID3D11DepthStencilState *dsState = NULL;
    RDCEraseEl(dsDesc);

    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.StencilEnable = TRUE;

    dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
        dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

    dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
    dsvDesc.Texture2DArray.ArraySize = 1;

    dev->CreateDepthStencilState(&dsDesc, &dsState);

    // loop over every array slice in MS texture
    for(UINT slice = 0; slice < descMS.ArraySize; slice++)
    {
      // loop over every multi sample
      for(UINT sample = 0; sample < descMS.SampleDesc.Count; sample++)
      {
        dsvDesc.Texture2DArray.FirstArraySlice = slice * descMS.SampleDesc.Count + sample;

        hr = dev->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvArray);
        if(FAILED(hr))
        {
          SAFE_RELEASE(dsState);
          SAFE_RELEASE(srvMS);
          RDCERR("0x%08x", hr);
          return;
        }

        ctx->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvArray, 0, 0, NULL, NULL);

        // loop over every stencil value (zzzzzz, no shader stencil read/write)
        for(UINT stencilval = 0; stencilval < 256; stencilval++)
        {
          uint32_t cdata[4] = {descMS.SampleDesc.Count, stencilval, sample, slice};

          ID3D11Buffer *cbuf = UNWRAP(WrappedID3D11Buffer, MakeCBuffer(cdata, sizeof(cdata)));

          ctx->PSSetConstantBuffers(0, 1, &cbuf);

          ctx->OMSetDepthStencilState(dsState, stencilval);

          ctx->Draw(3, 0);
        }

        SAFE_RELEASE(dsvArray);
      }
    }

    SAFE_RELEASE(dsState);
    SAFE_RELEASE(srvMS);
  }

  ctx->CopyResource(destArray, rtvResource);

  SAFE_RELEASE(rtvResource);
  SAFE_RELEASE(srvResource);
}

D3D11DebugManager::CacheElem &D3D11DebugManager::GetCachedElem(ResourceId id, CompType typeHint,
                                                               bool raw)
{
  for(auto it = m_ShaderItemCache.begin(); it != m_ShaderItemCache.end(); ++it)
  {
    if(it->id == id && it->typeHint == typeHint && it->raw == raw)
      return *it;
  }

  if(m_ShaderItemCache.size() >= NUM_CACHED_SRVS)
  {
    CacheElem &elem = m_ShaderItemCache.back();
    elem.Release();
    m_ShaderItemCache.pop_back();
  }

  m_ShaderItemCache.push_front(CacheElem(id, typeHint, raw));
  return m_ShaderItemCache.front();
}

D3D11DebugManager::TextureShaderDetails D3D11DebugManager::GetShaderDetails(ResourceId id,
                                                                            CompType typeHint,
                                                                            bool rawOutput)
{
  TextureShaderDetails details;
  HRESULT hr = S_OK;

  bool foundResource = false;

  CacheElem &cache = GetCachedElem(id, typeHint, rawOutput);

  bool msaaDepth = false;

  DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

  if(WrappedID3D11Texture1D::m_TextureList.find(id) != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *wrapTex1D =
        (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[id].m_Texture;
    TextureDisplayType mode = WrappedID3D11Texture1D::m_TextureList[id].m_Type;

    foundResource = true;

    details.texType = eTexType_1D;

    if(mode == TEXDISPLAY_DEPTH_TARGET)
      details.texType = eTexType_Depth;

    D3D11_TEXTURE1D_DESC desc1d = {0};
    wrapTex1D->GetDesc(&desc1d);

    details.texFmt = desc1d.Format;
    details.texWidth = desc1d.Width;
    details.texHeight = 1;
    details.texDepth = 1;
    details.texArraySize = desc1d.ArraySize;
    details.texMips = desc1d.MipLevels;

    srvFormat = GetTypedFormat(details.texFmt, typeHint);

    details.srvResource = wrapTex1D;

    if(mode == TEXDISPLAY_INDIRECT_VIEW || mode == TEXDISPLAY_DEPTH_TARGET)
    {
      D3D11_TEXTURE1D_DESC desc = desc1d;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      if(mode == TEXDISPLAY_DEPTH_TARGET)
        desc.Format = GetTypelessFormat(desc.Format);

      if(!cache.created)
      {
        ID3D11Texture1D *tmp = NULL;
        hr = m_pDevice->CreateTexture1D(&desc, NULL, &tmp);

        if(FAILED(hr))
        {
          RDCERR("Failed to create temporary Texture1D %08x", hr);
        }

        cache.srvResource = tmp;
      }

      details.previewCopy = cache.srvResource;

      m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

      details.srvResource = details.previewCopy;
    }
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(id) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *wrapTex2D =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[id].m_Texture;
    TextureDisplayType mode = WrappedID3D11Texture2D1::m_TextureList[id].m_Type;

    foundResource = true;

    details.texType = eTexType_2D;

    D3D11_TEXTURE2D_DESC desc2d = {0};
    wrapTex2D->GetDesc(&desc2d);

    details.texFmt = desc2d.Format;
    details.texWidth = desc2d.Width;
    details.texHeight = desc2d.Height;
    details.texDepth = 1;
    details.texArraySize = desc2d.ArraySize;
    details.texMips = desc2d.MipLevels;
    details.sampleCount = RDCMAX(1U, desc2d.SampleDesc.Count);
    details.sampleQuality = desc2d.SampleDesc.Quality;

    if(desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0)
    {
      details.texType = eTexType_2DMS;
    }

    if(mode == TEXDISPLAY_DEPTH_TARGET || IsDepthFormat(details.texFmt))
    {
      details.texType = eTexType_Depth;
      details.texFmt = GetTypedFormat(details.texFmt, typeHint);
    }

    // backbuffer is always interpreted as SRGB data regardless of format specified:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh972627(v=vs.85).aspx
    //
    // "The app must always place sRGB data into back buffers with integer-valued formats
    // to present the sRGB data to the screen, even if the data doesn't have this format
    // modifier in its format name."
    //
    // This essentially corrects for us always declaring an SRGB render target for our
    // output displays, as any app with a non-SRGB backbuffer would be incorrectly converted
    // unless we read out SRGB here.
    //
    // However when picking a pixel we want the actual value stored, not the corrected perceptual
    // value so for raw output we don't do this. This does my head in, it really does.
    if(wrapTex2D->m_RealDescriptor)
    {
      if(rawOutput)
        details.texFmt = wrapTex2D->m_RealDescriptor->Format;
      else
        details.texFmt = GetSRGBFormat(wrapTex2D->m_RealDescriptor->Format);
    }

    srvFormat = GetTypedFormat(details.texFmt, typeHint);

    details.srvResource = wrapTex2D;

    if(mode == TEXDISPLAY_INDIRECT_VIEW || mode == TEXDISPLAY_DEPTH_TARGET ||
       desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0)
    {
      D3D11_TEXTURE2D_DESC desc = desc2d;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      if(mode == TEXDISPLAY_DEPTH_TARGET)
      {
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.Format = GetTypelessFormat(desc.Format);
      }
      else
      {
        desc.Format = srvFormat;
      }

      if(!cache.created)
      {
        ID3D11Texture2D *tmp = NULL;
        hr = m_pDevice->CreateTexture2D(&desc, NULL, &tmp);

        if(FAILED(hr))
        {
          RDCERR("Failed to create temporary Texture2D %08x", hr);
        }

        cache.srvResource = tmp;
      }

      details.previewCopy = cache.srvResource;

      if((desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0) &&
         mode == TEXDISPLAY_DEPTH_TARGET)
        msaaDepth = true;

      m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

      details.srvResource = details.previewCopy;
    }
  }
  else if(WrappedID3D11Texture3D1::m_TextureList.find(id) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *wrapTex3D =
        (WrappedID3D11Texture3D1 *)WrappedID3D11Texture3D1::m_TextureList[id].m_Texture;
    TextureDisplayType mode = WrappedID3D11Texture3D1::m_TextureList[id].m_Type;

    foundResource = true;

    details.texType = eTexType_3D;

    D3D11_TEXTURE3D_DESC desc3d = {0};
    wrapTex3D->GetDesc(&desc3d);

    details.texFmt = desc3d.Format;
    details.texWidth = desc3d.Width;
    details.texHeight = desc3d.Height;
    details.texDepth = desc3d.Depth;
    details.texArraySize = 1;
    details.texMips = desc3d.MipLevels;

    srvFormat = GetTypedFormat(details.texFmt, typeHint);

    details.srvResource = wrapTex3D;

    if(mode == TEXDISPLAY_INDIRECT_VIEW)
    {
      D3D11_TEXTURE3D_DESC desc = desc3d;
      desc.CPUAccessFlags = 0;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      if(IsUIntFormat(srvFormat) || IsIntFormat(srvFormat))
        desc.Format = GetTypelessFormat(desc.Format);

      if(!cache.created)
      {
        ID3D11Texture3D *tmp = NULL;
        hr = m_pDevice->CreateTexture3D(&desc, NULL, &tmp);

        if(FAILED(hr))
        {
          RDCERR("Failed to create temporary Texture3D %08x", hr);
        }

        cache.srvResource = tmp;
      }

      details.previewCopy = cache.srvResource;

      m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

      details.srvResource = details.previewCopy;
    }
  }

  if(!foundResource)
  {
    RDCERR("bad texture trying to be displayed");
    return TextureShaderDetails();
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc[eTexType_Max];

  srvDesc[eTexType_1D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
  srvDesc[eTexType_1D].Texture1DArray.ArraySize = details.texArraySize;
  srvDesc[eTexType_1D].Texture1DArray.FirstArraySlice = 0;
  srvDesc[eTexType_1D].Texture1DArray.MipLevels = details.texMips;
  srvDesc[eTexType_1D].Texture1DArray.MostDetailedMip = 0;

  srvDesc[eTexType_2D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  srvDesc[eTexType_2D].Texture2DArray.ArraySize = details.texArraySize;
  srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice = 0;
  srvDesc[eTexType_2D].Texture2DArray.MipLevels = details.texMips;
  srvDesc[eTexType_2D].Texture2DArray.MostDetailedMip = 0;

  srvDesc[eTexType_2DMS].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
  srvDesc[eTexType_2DMS].Texture2DMSArray.ArraySize = details.texArraySize;
  srvDesc[eTexType_2DMS].Texture2DMSArray.FirstArraySlice = 0;

  srvDesc[eTexType_Stencil] = srvDesc[eTexType_Depth] = srvDesc[eTexType_2D];

  srvDesc[eTexType_3D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
  srvDesc[eTexType_3D].Texture3D.MipLevels = details.texMips;
  srvDesc[eTexType_3D].Texture3D.MostDetailedMip = 0;

  for(int i = 0; i < eTexType_Max; i++)
    srvDesc[i].Format = srvFormat;

  if(details.texType == eTexType_Depth)
  {
    switch(details.texFmt)
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      }
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_UNKNOWN;
        break;
      }
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
      }
      case DXGI_FORMAT_R16_FLOAT:
      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      {
        srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R16_UNORM;
        srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_UNKNOWN;
        break;
      }
      default: break;
    }
  }

  if(msaaDepth)
  {
    srvDesc[eTexType_Stencil].ViewDimension = srvDesc[eTexType_Depth].ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;

    srvDesc[eTexType_Depth].Texture2DMSArray.ArraySize =
        srvDesc[eTexType_2D].Texture2DArray.ArraySize;
    srvDesc[eTexType_Stencil].Texture2DMSArray.ArraySize =
        srvDesc[eTexType_2D].Texture2DArray.ArraySize;
    srvDesc[eTexType_Depth].Texture2DMSArray.FirstArraySlice =
        srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice;
    srvDesc[eTexType_Stencil].Texture2DMSArray.FirstArraySlice =
        srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice;
  }

  if(!cache.created)
  {
    hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[details.texType],
                                             &cache.srv[0]);

    if(FAILED(hr))
      RDCERR("Failed to create cache SRV 0, type %d %08x", details.texType, hr);
  }

  details.srv[details.texType] = cache.srv[0];

  if(details.texType == eTexType_Depth && srvDesc[eTexType_Stencil].Format != DXGI_FORMAT_UNKNOWN)
  {
    if(!cache.created)
    {
      hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[eTexType_Stencil],
                                               &cache.srv[1]);

      if(FAILED(hr))
        RDCERR("Failed to create cache SRV 1, type %d %08x", details.texType, hr);
    }

    details.srv[eTexType_Stencil] = cache.srv[1];

    details.texType = eTexType_Stencil;
  }

  if(msaaDepth)
  {
    if(details.texType == eTexType_Depth)
      details.texType = eTexType_DepthMS;
    if(details.texType == eTexType_Stencil)
      details.texType = eTexType_StencilMS;

    details.srv[eTexType_Depth] = NULL;
    details.srv[eTexType_Stencil] = NULL;
    details.srv[eTexType_DepthMS] = cache.srv[0];
    details.srv[eTexType_StencilMS] = cache.srv[1];
  }

  cache.created = true;

  return details;
}

void D3D11DebugManager::RenderText(float x, float y, const char *textfmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, textfmt);
  StringFormat::vsnprintf(tmpBuf, 4095, textfmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderTextInternal(x, y, tmpBuf);
}

void D3D11DebugManager::RenderTextInternal(float x, float y, const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderTextInternal(x, y, text);
    RenderTextInternal(x, y + 1.0f, t + 1);
    *t = '\n';
    return;
  }

  if(strlen(text) == 0)
    return;

  RDCASSERT(strlen(text) < FONT_MAX_CHARS);

  FontCBuffer data;

  data.TextPosition.x = x;
  data.TextPosition.y = y;

  data.FontScreenAspect.x = 1.0f / float(GetWidth());
  data.FontScreenAspect.y = 1.0f / float(GetHeight());

  data.TextSize = m_Font.CharSize;
  data.FontScreenAspect.x *= m_Font.CharAspect;

  data.FontScreenAspect.x *= m_supersamplingX;
  data.FontScreenAspect.y *= m_supersamplingY;

  data.CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
  data.CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

  D3D11_MAPPED_SUBRESOURCE mapped;

  FillCBuffer(m_Font.CBuffer, &data, sizeof(FontCBuffer));

  HRESULT hr = m_pImmediateContext->Map(m_Font.CharBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map charbuffer %08x", hr);
    return;
  }

  unsigned long *texs = (unsigned long *)mapped.pData;

  for(size_t i = 0; i < strlen(text); i++)
    texs[i * 4] = (text[i] - ' ');

  m_pImmediateContext->Unmap(m_Font.CharBuffer, 0);

  // can't just clear state because we need to keep things like render targets.
  {
    m_pImmediateContext->IASetInputLayout(NULL);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    m_pImmediateContext->VSSetShader(m_Font.VS, NULL, 0);
    m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_Font.CBuffer);
    m_pImmediateContext->VSSetConstantBuffers(1, 1, &m_Font.GlyphData);
    m_pImmediateContext->VSSetConstantBuffers(2, 1, &m_Font.CharBuffer);

    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);

    m_pImmediateContext->RSSetState(m_DebugRender.RastState);

    D3D11_VIEWPORT view;
    view.TopLeftX = 0;
    view.TopLeftY = 0;
    view.Width = (float)GetWidth();
    view.Height = (float)GetHeight();
    view.MinDepth = 0.0f;
    view.MaxDepth = 1.0f;
    m_pImmediateContext->RSSetViewports(1, &view);

    m_pImmediateContext->PSSetShader(m_Font.PS, NULL, 0);
    m_pImmediateContext->PSSetShaderResources(0, 1, &m_Font.Tex);

    ID3D11SamplerState *samps[] = {m_DebugRender.PointSampState, m_DebugRender.LinearSampState};
    m_pImmediateContext->PSSetSamplers(0, 2, samps);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_pImmediateContext->OMSetBlendState(m_DebugRender.BlendState, factor, 0xffffffff);

    m_pImmediateContext->DrawInstanced(4, (uint32_t)strlen(text), 0, 0);
  }
}

bool D3D11DebugManager::RenderTexture(TextureDisplay cfg, bool blendAlpha)
{
  DebugVertexCBuffer vertexData;
  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  float x = cfg.offx;
  float y = cfg.offy;

  vertexData.Position.x = x * (2.0f / float(GetWidth()));
  vertexData.Position.y = -y * (2.0f / float(GetHeight()));

  vertexData.ScreenAspect.x =
      (float(GetHeight()) / float(GetWidth()));    // 0.5 = character width / character height
  vertexData.ScreenAspect.y = 1.0f;

  vertexData.TextureResolution.x = 1.0f / vertexData.ScreenAspect.x;
  vertexData.TextureResolution.y = 1.0f;

  vertexData.LineStrip = 0;

  if(cfg.rangemax <= cfg.rangemin)
    cfg.rangemax += 0.00001f;

  pixelData.Channels.x = cfg.Red ? 1.0f : 0.0f;
  pixelData.Channels.y = cfg.Green ? 1.0f : 0.0f;
  pixelData.Channels.z = cfg.Blue ? 1.0f : 0.0f;
  pixelData.Channels.w = cfg.Alpha ? 1.0f : 0.0f;

  pixelData.RangeMinimum = cfg.rangemin;
  pixelData.InverseRangeSize = 1.0f / (cfg.rangemax - cfg.rangemin);

  if(_isnan(pixelData.InverseRangeSize) || !_finite(pixelData.InverseRangeSize))
  {
    pixelData.InverseRangeSize = FLT_MAX;
  }

  pixelData.WireframeColour.x = cfg.HDRMul;

  pixelData.RawOutput = cfg.rawoutput ? 1 : 0;

  pixelData.FlipY = cfg.FlipY ? 1 : 0;

  TextureShaderDetails details =
      GetShaderDetails(cfg.texid, cfg.typeHint, cfg.rawoutput ? true : false);

  int sampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, details.sampleCount - 1);

  // hacky resolve
  if(cfg.sampleIdx == ~0U)
    sampleIdx = -int(details.sampleCount);

  pixelData.SampleIdx = sampleIdx;

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return false;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  if(details.texFmt == DXGI_FORMAT_A8_UNORM && cfg.scale <= 0.0f)
  {
    pixelData.Channels.x = pixelData.Channels.y = pixelData.Channels.z = 0.0f;
    pixelData.Channels.w = 1.0f;
  }

  float tex_x = float(details.texWidth);
  float tex_y = float(details.texType == eTexType_1D ? 100 : details.texHeight);

  vertexData.TextureResolution.x *= tex_x / float(GetWidth());
  vertexData.TextureResolution.y *= tex_y / float(GetHeight());

  pixelData.TextureResolutionPS.x = float(RDCMAX(1U, details.texWidth >> cfg.mip));
  pixelData.TextureResolutionPS.y = float(RDCMAX(1U, details.texHeight >> cfg.mip));
  pixelData.TextureResolutionPS.z = float(RDCMAX(1U, details.texDepth >> cfg.mip));

  if(details.texArraySize > 1 && details.texType != eTexType_3D)
    pixelData.TextureResolutionPS.z = float(details.texArraySize);

  vertexData.Scale = cfg.scale;
  pixelData.ScalePS = cfg.scale;

  if(cfg.scale <= 0.0f)
  {
    float xscale = float(GetWidth()) / tex_x;
    float yscale = float(GetHeight()) / tex_y;

    vertexData.Scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      vertexData.Position.x = 0;
      vertexData.Position.y = tex_y * vertexData.Scale / float(GetHeight()) - 1.0f;
    }
    else
    {
      vertexData.Position.y = 0;
      vertexData.Position.x = 1.0f - tex_x * vertexData.Scale / float(GetWidth());
    }
  }

  ID3D11PixelShader *customPS = NULL;
  ID3D11Buffer *customBuff = NULL;

  if(cfg.CustomShader != ResourceId())
  {
    auto it = WrappedShader::m_ShaderList.find(cfg.CustomShader);

    if(it != WrappedShader::m_ShaderList.end())
    {
      auto dxbc = it->second->GetDXBC();

      RDCASSERT(dxbc);
      RDCASSERT(dxbc->m_Type == D3D11_ShaderType_Pixel);

      if(m_WrappedDevice->GetResourceManager()->HasLiveResource(cfg.CustomShader))
      {
        WrappedID3D11Shader<ID3D11PixelShader> *wrapped =
            (WrappedID3D11Shader<ID3D11PixelShader> *)m_WrappedDevice->GetResourceManager()
                ->GetLiveResource(cfg.CustomShader);

        customPS = wrapped;

        for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
        {
          const DXBC::CBuffer &cbuf = dxbc->m_CBuffers[i];
          if(cbuf.name == "$Globals")
          {
            float *cbufData = new float[cbuf.descriptor.byteSize / sizeof(float) + 1];
            byte *byteData = (byte *)cbufData;

            for(size_t v = 0; v < cbuf.variables.size(); v++)
            {
              const DXBC::CBufferVariable &var = cbuf.variables[v];

              if(var.name == "RENDERDOC_TexDim")
              {
                if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 4 &&
                   var.type.descriptor.type == DXBC::VARTYPE_UINT)
                {
                  uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

                  d[0] = details.texWidth;
                  d[1] = details.texHeight;
                  d[2] = details.texType == D3D11DebugManager::eTexType_3D ? details.texDepth
                                                                           : details.texArraySize;
                  d[3] = details.texMips;
                }
                else
                {
                  RDCWARN("Custom shader: Variable recognised but type wrong, expected uint4: %s",
                          var.name.c_str());
                }
              }
              else if(var.name == "RENDERDOC_SelectedMip")
              {
                if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
                   var.type.descriptor.type == DXBC::VARTYPE_UINT)
                {
                  uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

                  d[0] = cfg.mip;
                }
                else
                {
                  RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                          var.name.c_str());
                }
              }
              else if(var.name == "RENDERDOC_SelectedSliceFace")
              {
                if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
                   var.type.descriptor.type == DXBC::VARTYPE_UINT)
                {
                  uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

                  d[0] = cfg.sliceFace;
                }
                else
                {
                  RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                          var.name.c_str());
                }
              }
              else if(var.name == "RENDERDOC_SelectedSample")
              {
                if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
                   var.type.descriptor.type == DXBC::VARTYPE_INT)
                {
                  int32_t *d = (int32_t *)(byteData + var.descriptor.offset);

                  d[0] = cfg.sampleIdx;
                }
                else
                {
                  RDCWARN("Custom shader: Variable recognised but type wrong, expected int: %s",
                          var.name.c_str());
                }
              }
              else if(var.name == "RENDERDOC_TextureType")
              {
                if(var.type.descriptor.rows == 1 && var.type.descriptor.cols == 1 &&
                   var.type.descriptor.type == DXBC::VARTYPE_UINT)
                {
                  uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

                  d[0] = details.texType;
                }
                else
                {
                  RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %s",
                          var.name.c_str());
                }
              }
              else
              {
                RDCWARN("Custom shader: Variable not recognised: %s", var.name.c_str());
              }
            }

            customBuff = MakeCBuffer(cbufData, cbuf.descriptor.byteSize);

            SAFE_DELETE_ARRAY(cbufData);
          }
        }
      }
    }
  }

  vertexData.Scale *= 2.0f;    // viewport is -1 -> 1

  pixelData.MipLevel = (float)cfg.mip;
  pixelData.OutputDisplayFormat = RESTYPE_TEX2D;
  pixelData.Slice = float(RDCCLAMP(cfg.sliceFace, 0U, details.texArraySize - 1));

  if(details.texType == eTexType_3D)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX3D;
    pixelData.Slice = float(cfg.sliceFace);
  }
  else if(details.texType == eTexType_1D)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX1D;
  }
  else if(details.texType == eTexType_Depth)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH;
  }
  else if(details.texType == eTexType_Stencil)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH_STENCIL;
  }
  else if(details.texType == eTexType_DepthMS)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH_MS;
  }
  else if(details.texType == eTexType_StencilMS)
  {
    pixelData.OutputDisplayFormat = RESTYPE_DEPTH_STENCIL_MS;
  }
  else if(details.texType == eTexType_2DMS)
  {
    pixelData.OutputDisplayFormat = RESTYPE_TEX2D_MS;
  }

  if(cfg.overlay == DebugOverlay::NaN)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;
  }

  if(cfg.overlay == DebugOverlay::Clipping)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;
  }

  int srvOffset = 0;

  if(IsUIntFormat(details.texFmt) ||
     (IsTypelessFormat(details.texFmt) && cfg.typeHint == CompType::UInt))
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
    srvOffset = 10;
  }
  if(IsIntFormat(details.texFmt) ||
     (IsTypelessFormat(details.texFmt) && cfg.typeHint == CompType::SInt))
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;
    srvOffset = 20;
  }
  if(!IsSRGBFormat(details.texFmt) && cfg.linearDisplayAsGamma)
  {
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;
  }

  FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));
  FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

  // can't just clear state because we need to keep things like render targets.
  {
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    m_pImmediateContext->VSSetShader(m_DebugRender.GenericVS, NULL, 0);
    m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);

    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);

    m_pImmediateContext->RSSetState(m_DebugRender.RastState);

    if(customPS == NULL)
    {
      m_pImmediateContext->PSSetShader(m_DebugRender.TexDisplayPS, NULL, 0);
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
    }
    else
    {
      m_pImmediateContext->PSSetShader(customPS, NULL, 0);
      m_pImmediateContext->PSSetConstantBuffers(0, 1, &customBuff);
    }

    ID3D11UnorderedAccessView *NullUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
    UINT UAV_keepcounts[D3D11_1_UAV_SLOT_COUNT];
    memset(&UAV_keepcounts[0], 0xff, sizeof(UAV_keepcounts));
    const UINT numUAVs =
        m_WrappedContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;

    m_pImmediateContext->CSSetUnorderedAccessViews(0, numUAVs, NullUAVs, UAV_keepcounts);

    m_pImmediateContext->PSSetShaderResources(srvOffset, eTexType_Max, details.srv);

    ID3D11SamplerState *samps[] = {m_DebugRender.PointSampState, m_DebugRender.LinearSampState};
    m_pImmediateContext->PSSetSamplers(0, 2, samps);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if(cfg.rawoutput || !blendAlpha || cfg.CustomShader != ResourceId())
      m_pImmediateContext->OMSetBlendState(NULL, factor, 0xffffffff);
    else
      m_pImmediateContext->OMSetBlendState(m_DebugRender.BlendState, factor, 0xffffffff);

    m_pImmediateContext->Draw(4, 0);
  }

  return true;
}

void D3D11DebugManager::RenderHighlightBox(float w, float h, float scale)
{
  D3D11RenderStateTracker tracker(m_WrappedContext);

  float overlayConsts[] = {1.0f, 1.0f, 1.0f, 1.0f};

  ID3D11Buffer *vconst = NULL;
  ID3D11Buffer *pconst = NULL;

  pconst = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

  const float xpixdim = 2.0f / w;
  const float ypixdim = 2.0f / h;

  const float xdim = scale * xpixdim;
  const float ydim = scale * ypixdim;

  DebugVertexCBuffer vertCBuffer;
  RDCEraseEl(vertCBuffer);
  vertCBuffer.Scale = 1.0f;
  vertCBuffer.ScreenAspect.x = vertCBuffer.ScreenAspect.y = 1.0f;

  vertCBuffer.Position.x = 1.0f;
  vertCBuffer.Position.y = -1.0f;
  vertCBuffer.TextureResolution.x = xdim;
  vertCBuffer.TextureResolution.y = ydim;

  vertCBuffer.LineStrip = 1;

  vconst = MakeCBuffer(&vertCBuffer, sizeof(vertCBuffer));

  m_pImmediateContext->HSSetShader(NULL, NULL, 0);
  m_pImmediateContext->DSSetShader(NULL, NULL, 0);
  m_pImmediateContext->GSSetShader(NULL, NULL, 0);

  m_pImmediateContext->RSSetState(m_DebugRender.RastState);

  m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
  m_pImmediateContext->IASetInputLayout(NULL);

  m_pImmediateContext->VSSetShader(m_DebugRender.GenericVS, NULL, 0);
  m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
  m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);

  m_pImmediateContext->PSSetConstantBuffers(0, 1, &pconst);
  m_pImmediateContext->VSSetConstantBuffers(0, 1, &vconst);

  m_pImmediateContext->Draw(5, 0);

  vertCBuffer.Position.x = 1.0f - xpixdim;
  vertCBuffer.Position.y = -1.0f + ypixdim;
  vertCBuffer.TextureResolution.x = xdim + xpixdim * 2;
  vertCBuffer.TextureResolution.y = ydim + ypixdim * 2;

  overlayConsts[0] = overlayConsts[1] = overlayConsts[2] = 0.0f;

  vconst = MakeCBuffer(&vertCBuffer, sizeof(vertCBuffer));
  pconst = MakeCBuffer(overlayConsts, sizeof(overlayConsts));

  m_pImmediateContext->VSSetConstantBuffers(0, 1, &vconst);
  m_pImmediateContext->PSSetConstantBuffers(0, 1, &pconst);
  m_pImmediateContext->Draw(5, 0);
}

void D3D11DebugManager::RenderCheckerboard(Vec3f light, Vec3f dark)
{
  DebugVertexCBuffer vertexData;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  vertexData.Scale = 2.0f;
  vertexData.Position.x = vertexData.Position.y = 0;

  vertexData.ScreenAspect.x = 1.0f;
  vertexData.ScreenAspect.y = 1.0f;

  vertexData.TextureResolution.x = 1.0f;
  vertexData.TextureResolution.y = 1.0f;

  vertexData.LineStrip = 0;

  FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  pixelData.Channels = Vec4f(light.x, light.y, light.z, 0.0f);
  pixelData.WireframeColour = dark;

  FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

  // can't just clear state because we need to keep things like render targets.
  {
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pImmediateContext->IASetInputLayout(NULL);

    m_pImmediateContext->VSSetShader(m_DebugRender.GenericVS, NULL, 0);
    m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);

    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);
    m_pImmediateContext->GSSetShader(NULL, NULL, 0);

    m_pImmediateContext->RSSetState(m_DebugRender.RastState);

    m_pImmediateContext->PSSetShader(m_DebugRender.CheckerboardPS, NULL, 0);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_pImmediateContext->OMSetBlendState(NULL, factor, 0xffffffff);
    m_pImmediateContext->OMSetDepthStencilState(NULL, 0);

    m_pImmediateContext->Draw(4, 0);
  }
}

MeshFormat D3D11DebugManager::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  D3D11PostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    postvs = m_PostVSData[eventID];

  const D3D11PostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf)
    ret.idxbuf = ((WrappedID3D11Buffer *)s.idxBuf)->GetResourceID();
  else
    ret.idxbuf = ResourceId();
  ret.idxoffs = 0;
  ret.idxByteWidth = s.idxFmt == DXGI_FORMAT_R16_UINT ? 2 : 4;
  ret.baseVertex = 0;

  if(s.buf)
    ret.buf = ((WrappedID3D11Buffer *)s.buf)->GetResourceID();
  else
    ret.buf = ResourceId();

  ret.offset = s.instStride * instID;
  ret.stride = s.vertStride;

  ret.compCount = 4;
  ret.compByteWidth = 4;
  ret.compType = CompType::Float;
  ret.specialFormat = SpecialFormat::Unknown;

  ret.showAlpha = false;
  ret.bgraOrder = false;

  ret.topo = MakePrimitiveTopology(s.topo);
  ret.numVerts = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  if(instID < s.instData.size())
  {
    D3D11PostVSData::InstData inst = s.instData[instID];

    ret.offset = inst.bufOffset;
    ret.numVerts = inst.numVerts;
  }

  return ret;
}

void D3D11DebugManager::InitPostVSBuffers(uint32_t eventID)
{
  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    return;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  ID3D11VertexShader *vs = NULL;
  m_pImmediateContext->VSGetShader(&vs, NULL, NULL);

  ID3D11GeometryShader *gs = NULL;
  m_pImmediateContext->GSGetShader(&gs, NULL, NULL);

  ID3D11HullShader *hs = NULL;
  m_pImmediateContext->HSGetShader(&hs, NULL, NULL);

  ID3D11DomainShader *ds = NULL;
  m_pImmediateContext->DSGetShader(&ds, NULL, NULL);

  if(vs)
    vs->Release();
  if(gs)
    gs->Release();
  if(hs)
    hs->Release();
  if(ds)
    ds->Release();

  if(!vs)
    return;

  D3D11_PRIMITIVE_TOPOLOGY topo;
  m_pImmediateContext->IAGetPrimitiveTopology(&topo);

  WrappedID3D11Shader<ID3D11VertexShader> *wrappedVS = (WrappedID3D11Shader<ID3D11VertexShader> *)vs;

  if(!wrappedVS)
  {
    RDCERR("Couldn't find wrapped vertex shader!");
    return;
  }

  const DrawcallDescription *drawcall = m_WrappedDevice->GetDrawcall(eventID);

  if(drawcall->numIndices == 0)
    return;

  DXBC::DXBCFile *dxbcVS = wrappedVS->GetDXBC();

  RDCASSERT(dxbcVS);

  DXBC::DXBCFile *dxbcGS = NULL;

  if(gs)
  {
    WrappedID3D11Shader<ID3D11GeometryShader> *wrappedGS =
        (WrappedID3D11Shader<ID3D11GeometryShader> *)gs;

    if(!wrappedGS)
    {
      RDCERR("Couldn't find wrapped geometry shader!");
      return;
    }

    dxbcGS = wrappedGS->GetDXBC();

    RDCASSERT(dxbcGS);
  }

  DXBC::DXBCFile *dxbcDS = NULL;

  if(ds)
  {
    WrappedID3D11Shader<ID3D11DomainShader> *wrappedDS =
        (WrappedID3D11Shader<ID3D11DomainShader> *)ds;

    if(!wrappedDS)
    {
      RDCERR("Couldn't find wrapped domain shader!");
      return;
    }

    dxbcDS = wrappedDS->GetDXBC();

    RDCASSERT(dxbcDS);
  }

  vector<D3D11_SO_DECLARATION_ENTRY> sodecls;

  UINT stride = 0;
  int posidx = -1;
  int numPosComponents = 0;

  ID3D11GeometryShader *streamoutGS = NULL;

  if(!dxbcVS->m_OutputSig.empty())
  {
    for(size_t i = 0; i < dxbcVS->m_OutputSig.size(); i++)
    {
      SigParameter &sign = dxbcVS->m_OutputSig[i];

      D3D11_SO_DECLARATION_ENTRY decl;

      decl.Stream = 0;
      decl.OutputSlot = 0;

      decl.SemanticName = sign.semanticName.elems;
      decl.SemanticIndex = sign.semanticIndex;
      decl.StartComponent = 0;
      decl.ComponentCount = sign.compCount & 0xff;

      if(sign.systemValue == ShaderBuiltin::Position)
      {
        posidx = (int)sodecls.size();
        numPosComponents = decl.ComponentCount = 4;
      }

      stride += decl.ComponentCount * sizeof(float);
      sodecls.push_back(decl);
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D11_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(sodecls.begin() + posidx);
      sodecls.insert(sodecls.begin(), pos);
    }

    HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
        (void *)&dxbcVS->m_ShaderBlob[0], dxbcVS->m_ShaderBlob.size(), &sodecls[0],
        (UINT)sodecls.size(), &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &streamoutGS);

    if(FAILED(hr))
    {
      RDCERR("Failed to create Geometry Shader + SO %08x", hr);
      return;
    }

    m_pImmediateContext->GSSetShader(streamoutGS, NULL, 0);
    m_pImmediateContext->HSSetShader(NULL, NULL, 0);
    m_pImmediateContext->DSSetShader(NULL, NULL, 0);

    SAFE_RELEASE(streamoutGS);

    UINT offset = 0;
    ID3D11Buffer *idxBuf = NULL;
    DXGI_FORMAT idxFmt = DXGI_FORMAT_UNKNOWN;
    UINT idxOffs = 0;

    m_pImmediateContext->IAGetIndexBuffer(&idxBuf, &idxFmt, &idxOffs);

    ID3D11Buffer *origBuf = idxBuf;

    if(!(drawcall->flags & DrawFlags::UseIBuffer))
    {
      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

      SAFE_RELEASE(idxBuf);

      uint32_t outputSize = stride * drawcall->numIndices;
      if(drawcall->flags & DrawFlags::Instanced)
        outputSize *= drawcall->numInstances;

      if(m_SOBufferSize < outputSize)
      {
        int oldSize = m_SOBufferSize;
        while(m_SOBufferSize < outputSize)
          m_SOBufferSize *= 2;
        RDCWARN("Resizing stream-out buffer from %d to %d", oldSize, m_SOBufferSize);
        CreateSOBuffers();
      }

      m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);

      m_pImmediateContext->Begin(m_SOStatsQueries[0]);

      if(drawcall->flags & DrawFlags::Instanced)
        m_pImmediateContext->DrawInstanced(drawcall->numIndices, drawcall->numInstances,
                                           drawcall->vertexOffset, drawcall->instanceOffset);
      else
        m_pImmediateContext->Draw(drawcall->numIndices, drawcall->vertexOffset);

      m_pImmediateContext->End(m_SOStatsQueries[0]);
    }
    else    // drawcall is indexed
    {
      bool index16 = (idxFmt == DXGI_FORMAT_R16_UINT);
      UINT bytesize = index16 ? 2 : 4;

      vector<byte> idxdata;
      GetBufferData(idxBuf, idxOffs + drawcall->indexOffset * bytesize,
                    drawcall->numIndices * bytesize, idxdata);

      SAFE_RELEASE(idxBuf);

      vector<uint32_t> indices;

      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      // only read as many indices as were available in the buffer
      uint32_t numIndices =
          RDCMIN(uint32_t(index16 ? idxdata.size() / 2 : idxdata.size() / 4), drawcall->numIndices);

      uint32_t idxclamp = 0;
      if(drawcall->baseVertex < 0)
        idxclamp = uint32_t(-drawcall->baseVertex);

      // grab all unique vertex indices referenced
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

        // apply baseVertex but clamp to 0 (don't allow index to become negative)
        if(i32 < idxclamp)
          i32 = 0;
        else if(drawcall->baseVertex < 0)
          i32 -= idxclamp;
        else if(drawcall->baseVertex > 0)
          i32 += drawcall->baseVertex;

        auto it = std::lower_bound(indices.begin(), indices.end(), i32);

        if(it != indices.end() && *it == i32)
          continue;

        indices.insert(it, i32);
      }

      // if we read out of bounds, we'll also have a 0 index being referenced
      // (as 0 is read). Don't insert 0 if we already have 0 though
      if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
        indices.insert(indices.begin(), 0);

      // An index buffer could be something like: 500, 501, 502, 501, 503, 502
      // in which case we can't use the existing index buffer without filling 499 slots of vertex
      // data with padding. Instead we rebase the indices based on the smallest vertex so it becomes
      // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
      //
      // Note that there could also be gaps, like: 500, 501, 502, 510, 511, 512
      // which would become 0, 1, 2, 3, 4, 5 and so the old index buffer would no longer be valid.
      // We just stream-out a tightly packed list of unique indices, and then remap the index buffer
      // so that what did point to 500 points to 0 (accounting for rebasing), and what did point
      // to 510 now points to 3 (accounting for the unique sort).

      // we use a map here since the indices may be sparse. Especially considering if an index
      // is 'invalid' like 0xcccccccc then we don't want an array of 3.4 billion entries.
      map<uint32_t, size_t> indexRemap;
      for(size_t i = 0; i < indices.size(); i++)
      {
        // by definition, this index will only appear once in indices[]
        indexRemap[indices[i]] = i;
      }

      D3D11_BUFFER_DESC desc = {UINT(sizeof(uint32_t) * indices.size()),
                                D3D11_USAGE_IMMUTABLE,
                                D3D11_BIND_INDEX_BUFFER,
                                0,
                                0,
                                0};
      D3D11_SUBRESOURCE_DATA initData = {&indices[0], desc.ByteWidth, desc.ByteWidth};

      if(!indices.empty())
        m_pDevice->CreateBuffer(&desc, &initData, &idxBuf);
      else
        idxBuf = NULL;

      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      m_pImmediateContext->IASetIndexBuffer(idxBuf, DXGI_FORMAT_R32_UINT, 0);
      SAFE_RELEASE(idxBuf);

      uint32_t outputSize = stride * (uint32_t)indices.size();
      if(drawcall->flags & DrawFlags::Instanced)
        outputSize *= drawcall->numInstances;

      if(m_SOBufferSize < outputSize)
      {
        int oldSize = m_SOBufferSize;
        while(m_SOBufferSize < outputSize)
          m_SOBufferSize *= 2;
        RDCWARN("Resizing stream-out buffer from %d to %d", oldSize, m_SOBufferSize);
        CreateSOBuffers();
      }

      m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);

      m_pImmediateContext->Begin(m_SOStatsQueries[0]);

      if(drawcall->flags & DrawFlags::Instanced)
        m_pImmediateContext->DrawIndexedInstanced((UINT)indices.size(), drawcall->numInstances, 0,
                                                  0, drawcall->instanceOffset);
      else
        m_pImmediateContext->DrawIndexed((UINT)indices.size(), 0, 0);

      m_pImmediateContext->End(m_SOStatsQueries[0]);

      // rebase existing index buffer to point to the right elements in our stream-out'd
      // vertex buffer
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

        // preserve primitive restart indices
        if(i32 == (index16 ? 0xffff : 0xffffffff))
          continue;

        // apply baseVertex but clamp to 0 (don't allow index to become negative)
        if(i32 < idxclamp)
          i32 = 0;
        else if(drawcall->baseVertex < 0)
          i32 -= idxclamp;
        else if(drawcall->baseVertex > 0)
          i32 += drawcall->baseVertex;

        if(index16)
          idx16[i] = uint16_t(indexRemap[i32]);
        else
          idx32[i] = uint32_t(indexRemap[i32]);
      }

      desc.ByteWidth = (UINT)idxdata.size();
      initData.pSysMem = &idxdata[0];
      initData.SysMemPitch = initData.SysMemSlicePitch = desc.ByteWidth;

      if(desc.ByteWidth > 0)
        m_pDevice->CreateBuffer(&desc, &initData, &idxBuf);
      else
        idxBuf = NULL;
    }

    m_pImmediateContext->IASetPrimitiveTopology(topo);
    m_pImmediateContext->IASetIndexBuffer(origBuf, idxFmt, idxOffs);

    m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    m_pImmediateContext->SOSetTargets(0, NULL, NULL);

    D3D11_QUERY_DATA_SO_STATISTICS numPrims;

    m_pImmediateContext->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    do
    {
      hr = m_pImmediateContext->GetData(m_SOStatsQueries[0], &numPrims,
                                        sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
    } while(hr == S_FALSE);

    if(numPrims.NumPrimitivesWritten == 0)
    {
      m_PostVSData[eventID] = D3D11PostVSData();
      SAFE_RELEASE(idxBuf);
      return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_pImmediateContext->Map(m_SOStagingBuffer, 0, D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer %08x", hr);
      SAFE_RELEASE(idxBuf);
      return;
    }

    D3D11_BUFFER_DESC bufferDesc = {stride * (uint32_t)numPrims.NumPrimitivesWritten,
                                    D3D11_USAGE_IMMUTABLE,
                                    D3D11_BIND_VERTEX_BUFFER,
                                    0,
                                    0,
                                    0};

    ID3D11Buffer *vsoutBuffer = NULL;

    // we need to map this data into memory for read anyway, might as well make this VB
    // immutable while we're at it.
    D3D11_SUBRESOURCE_DATA initialData;
    initialData.pSysMem = mapped.pData;
    initialData.SysMemPitch = bufferDesc.ByteWidth;
    initialData.SysMemSlicePitch = bufferDesc.ByteWidth;

    hr = m_pDevice->CreateBuffer(&bufferDesc, &initialData, &vsoutBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create postvs pos buffer %08x", hr);

      m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
      SAFE_RELEASE(idxBuf);
      return;
    }

    byte *byteData = (byte *)mapped.pData;

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numPrims.NumPrimitivesWritten; i++)
    {
      //////////////////////////////////////////////////////////////////////////////////
      // derive near/far, assuming a standard perspective matrix
      //
      // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
      // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
      // and we know Wpost = Zpre from the perspective matrix.
      // we can then see from the perspective matrix that
      // m = F/(F-N)
      // c = -(F*N)/(F-N)
      //
      // with re-arranging and substitution, we then get:
      // N = -c/m
      // F = c/(1-m)
      //
      // so if we can derive m and c then we can determine N and F. We can do this with
      // two points, and we pick them reasonably distinct on z to reduce floating-point
      // error

      Vec4f *pos = (Vec4f *)(byteData + i * stride);

      if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
      {
        Vec2f A(pos0->w, pos0->z);
        Vec2f B(pos->w, pos->z);

        float m = (B.y - A.y) / (B.x - A.x);
        float c = B.y - B.x * m;

        if(m == 1.0f)
          continue;

        nearp = -c / m;
        farp = c / (1 - m);

        found = true;

        break;
      }
    }

    // if we didn't find anything, all z's and w's were identical.
    // If the z is positive and w greater for the first element then
    // we detect this projection as reversed z with infinite far plane
    if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
    {
      nearp = pos0->z;
      farp = FLT_MAX;
    }

    m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);

    m_PostVSData[eventID].vsin.topo = topo;
    m_PostVSData[eventID].vsout.buf = vsoutBuffer;
    m_PostVSData[eventID].vsout.vertStride = stride;
    m_PostVSData[eventID].vsout.nearPlane = nearp;
    m_PostVSData[eventID].vsout.farPlane = farp;

    m_PostVSData[eventID].vsout.useIndices = bool(drawcall->flags & DrawFlags::UseIBuffer);
    m_PostVSData[eventID].vsout.numVerts = drawcall->numIndices;

    m_PostVSData[eventID].vsout.instStride = 0;
    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventID].vsout.instStride =
          bufferDesc.ByteWidth / RDCMAX(1U, drawcall->numInstances);

    m_PostVSData[eventID].vsout.idxBuf = NULL;
    if(m_PostVSData[eventID].vsout.useIndices && idxBuf)
    {
      m_PostVSData[eventID].vsout.idxBuf = idxBuf;
      m_PostVSData[eventID].vsout.idxFmt = idxFmt;
    }

    m_PostVSData[eventID].vsout.hasPosOut = posidx >= 0;

    m_PostVSData[eventID].vsout.topo = topo;
  }
  else
  {
    // empty vertex output signature
    m_PostVSData[eventID].vsin.topo = topo;
    m_PostVSData[eventID].vsout.buf = NULL;
    m_PostVSData[eventID].vsout.instStride = 0;
    m_PostVSData[eventID].vsout.vertStride = 0;
    m_PostVSData[eventID].vsout.nearPlane = 0.0f;
    m_PostVSData[eventID].vsout.farPlane = 0.0f;
    m_PostVSData[eventID].vsout.useIndices = false;
    m_PostVSData[eventID].vsout.hasPosOut = false;
    m_PostVSData[eventID].vsout.idxBuf = NULL;

    m_PostVSData[eventID].vsout.topo = topo;
  }

  if(dxbcGS || dxbcDS)
  {
    stride = 0;
    posidx = -1;
    numPosComponents = 0;

    DXBC::DXBCFile *lastShader = dxbcGS;
    if(dxbcDS)
      lastShader = dxbcDS;

    sodecls.clear();
    for(size_t i = 0; i < lastShader->m_OutputSig.size(); i++)
    {
      SigParameter &sign = lastShader->m_OutputSig[i];

      D3D11_SO_DECLARATION_ENTRY decl;

      // for now, skip streams that aren't stream 0
      if(sign.stream != 0)
        continue;

      decl.Stream = 0;
      decl.OutputSlot = 0;

      decl.SemanticName = sign.semanticName.elems;
      decl.SemanticIndex = sign.semanticIndex;
      decl.StartComponent = 0;
      decl.ComponentCount = sign.compCount & 0xff;

      if(sign.systemValue == ShaderBuiltin::Position)
      {
        posidx = (int)sodecls.size();
        numPosComponents = decl.ComponentCount = 4;
      }

      stride += decl.ComponentCount * sizeof(float);
      sodecls.push_back(decl);
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D11_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(sodecls.begin() + posidx);
      sodecls.insert(sodecls.begin(), pos);
    }

    streamoutGS = NULL;

    HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
        (void *)&lastShader->m_ShaderBlob[0], lastShader->m_ShaderBlob.size(), &sodecls[0],
        (UINT)sodecls.size(), &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &streamoutGS);

    if(FAILED(hr))
    {
      RDCERR("Failed to create Geometry Shader + SO %08x", hr);
      return;
    }

    m_pImmediateContext->GSSetShader(streamoutGS, NULL, 0);
    m_pImmediateContext->HSSetShader(hs, NULL, 0);
    m_pImmediateContext->DSSetShader(ds, NULL, 0);

    SAFE_RELEASE(streamoutGS);

    UINT offset = 0;

    D3D11_QUERY_DATA_SO_STATISTICS numPrims = {0};

    // do the whole draw, and if our output buffer isn't large enough then loop around.
    while(true)
    {
      m_pImmediateContext->Begin(m_SOStatsQueries[0]);

      m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);

      if(drawcall->flags & DrawFlags::Instanced)
      {
        if(drawcall->flags & DrawFlags::UseIBuffer)
        {
          m_pImmediateContext->DrawIndexedInstanced(drawcall->numIndices, drawcall->numInstances,
                                                    drawcall->indexOffset, drawcall->baseVertex,
                                                    drawcall->instanceOffset);
        }
        else
        {
          m_pImmediateContext->DrawInstanced(drawcall->numIndices, drawcall->numInstances,
                                             drawcall->vertexOffset, drawcall->instanceOffset);
        }
      }
      else
      {
        // trying to stream out a stream-out-auto based drawcall would be bad!
        // instead just draw the number of verts we pre-calculated
        if(drawcall->flags & DrawFlags::Auto)
        {
          m_pImmediateContext->Draw(drawcall->numIndices, 0);
        }
        else
        {
          if(drawcall->flags & DrawFlags::UseIBuffer)
          {
            m_pImmediateContext->DrawIndexed(drawcall->numIndices, drawcall->indexOffset,
                                             drawcall->baseVertex);
          }
          else
          {
            m_pImmediateContext->Draw(drawcall->numIndices, drawcall->vertexOffset);
          }
        }
      }

      m_pImmediateContext->End(m_SOStatsQueries[0]);

      do
      {
        hr = m_pImmediateContext->GetData(m_SOStatsQueries[0], &numPrims,
                                          sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
      } while(hr == S_FALSE);

      if(m_SOBufferSize < stride * (uint32_t)numPrims.PrimitivesStorageNeeded * 3)
      {
        int oldSize = m_SOBufferSize;
        while(m_SOBufferSize < stride * (uint32_t)numPrims.PrimitivesStorageNeeded * 3)
          m_SOBufferSize *= 2;
        RDCWARN("Resizing stream-out buffer from %d to %d", oldSize, m_SOBufferSize);
        CreateSOBuffers();
        continue;
      }

      break;
    }

    // instanced draws must be replayed one at a time so we can record the number of primitives from
    // each drawcall, as due to expansion this can vary per-instance.
    if(drawcall->flags & DrawFlags::Instanced && drawcall->numInstances > 1)
    {
      // ensure we have enough queries
      while(m_SOStatsQueries.size() < drawcall->numInstances)
      {
        D3D11_QUERY_DESC qdesc;
        qdesc.MiscFlags = 0;
        qdesc.Query = D3D11_QUERY_SO_STATISTICS;

        ID3D11Query *q = NULL;
        hr = m_pDevice->CreateQuery(&qdesc, &q);
        if(FAILED(hr))
          RDCERR("Failed to create m_SOStatsQuery %08x", hr);

        m_SOStatsQueries.push_back(q);
      }

      // do incremental draws to get the output size. We have to do this O(N^2) style because
      // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
      // instances and count the total number of verts each time, then we can see from the
      // difference how much each instance wrote.
      for(uint32_t inst = 1; inst <= drawcall->numInstances; inst++)
      {
        if(drawcall->flags & DrawFlags::UseIBuffer)
        {
          m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);
          m_pImmediateContext->Begin(m_SOStatsQueries[inst - 1]);
          m_pImmediateContext->DrawIndexedInstanced(drawcall->numIndices, inst, drawcall->indexOffset,
                                                    drawcall->baseVertex, drawcall->instanceOffset);
          m_pImmediateContext->End(m_SOStatsQueries[inst - 1]);
        }
        else
        {
          m_pImmediateContext->SOSetTargets(1, &m_SOBuffer, &offset);
          m_pImmediateContext->Begin(m_SOStatsQueries[inst - 1]);
          m_pImmediateContext->DrawInstanced(drawcall->numIndices, inst, drawcall->vertexOffset,
                                             drawcall->instanceOffset);
          m_pImmediateContext->End(m_SOStatsQueries[inst - 1]);
        }
      }
    }

    m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    m_pImmediateContext->SOSetTargets(0, NULL, NULL);

    m_pImmediateContext->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    std::vector<D3D11PostVSData::InstData> instData;

    if((drawcall->flags & DrawFlags::Instanced) && drawcall->numInstances > 1)
    {
      uint64_t prevVertCount = 0;

      for(uint32_t inst = 0; inst < drawcall->numInstances; inst++)
      {
        do
        {
          hr = m_pImmediateContext->GetData(m_SOStatsQueries[inst], &numPrims,
                                            sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
        } while(hr == S_FALSE);

        uint64_t vertCount = 3 * numPrims.NumPrimitivesWritten;

        D3D11PostVSData::InstData d;
        d.numVerts = uint32_t(vertCount - prevVertCount);
        d.bufOffset = uint32_t(stride * prevVertCount);
        prevVertCount = vertCount;

        instData.push_back(d);
      }
    }
    else
    {
      do
      {
        hr = m_pImmediateContext->GetData(m_SOStatsQueries[0], &numPrims,
                                          sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
      } while(hr == S_FALSE);
    }

    if(numPrims.NumPrimitivesWritten == 0)
    {
      return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_pImmediateContext->Map(m_SOStagingBuffer, 0, D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer %08x", hr);
      return;
    }

    D3D11_BUFFER_DESC bufferDesc = {stride * (uint32_t)numPrims.NumPrimitivesWritten * 3,
                                    D3D11_USAGE_IMMUTABLE,
                                    D3D11_BIND_VERTEX_BUFFER,
                                    0,
                                    0,
                                    0};

    if(bufferDesc.ByteWidth >= m_SOBufferSize)
    {
      RDCERR("Generated output data too large: %08x", bufferDesc.ByteWidth);

      m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
      return;
    }

    ID3D11Buffer *gsoutBuffer = NULL;

    // we need to map this data into memory for read anyway, might as well make this VB
    // immutable while we're at it.
    D3D11_SUBRESOURCE_DATA initialData;
    initialData.pSysMem = mapped.pData;
    initialData.SysMemPitch = bufferDesc.ByteWidth;
    initialData.SysMemSlicePitch = bufferDesc.ByteWidth;

    hr = m_pDevice->CreateBuffer(&bufferDesc, &initialData, &gsoutBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create postvs pos buffer %08x", hr);

      m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
      return;
    }

    byte *byteData = (byte *)mapped.pData;

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numPrims.NumPrimitivesWritten; i++)
    {
      //////////////////////////////////////////////////////////////////////////////////
      // derive near/far, assuming a standard perspective matrix
      //
      // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
      // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
      // and we know Wpost = Zpre from the perspective matrix.
      // we can then see from the perspective matrix that
      // m = F/(F-N)
      // c = -(F*N)/(F-N)
      //
      // with re-arranging and substitution, we then get:
      // N = -c/m
      // F = c/(1-m)
      //
      // so if we can derive m and c then we can determine N and F. We can do this with
      // two points, and we pick them reasonably distinct on z to reduce floating-point
      // error

      Vec4f *pos = (Vec4f *)(byteData + i * stride);

      if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
      {
        Vec2f A(pos0->w, pos0->z);
        Vec2f B(pos->w, pos->z);

        float m = (B.y - A.y) / (B.x - A.x);
        float c = B.y - B.x * m;

        if(m == 1.0f)
          continue;

        nearp = -c / m;
        farp = c / (1 - m);

        found = true;

        break;
      }
    }

    // if we didn't find anything, all z's and w's were identical.
    // If the z is positive and w greater for the first element then
    // we detect this projection as reversed z with infinite far plane
    if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
    {
      nearp = pos0->z;
      farp = FLT_MAX;
    }

    m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);

    m_PostVSData[eventID].gsout.buf = gsoutBuffer;
    m_PostVSData[eventID].gsout.instStride = 0;
    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventID].gsout.instStride =
          bufferDesc.ByteWidth / RDCMAX(1U, drawcall->numInstances);
    m_PostVSData[eventID].gsout.vertStride = stride;
    m_PostVSData[eventID].gsout.nearPlane = nearp;
    m_PostVSData[eventID].gsout.farPlane = farp;
    m_PostVSData[eventID].gsout.useIndices = false;
    m_PostVSData[eventID].gsout.hasPosOut = posidx >= 0;
    m_PostVSData[eventID].gsout.idxBuf = NULL;

    topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if(lastShader == dxbcGS)
    {
      for(size_t i = 0; i < dxbcGS->GetNumDeclarations(); i++)
      {
        const DXBC::ASMDecl &decl = dxbcGS->GetDeclaration(i);

        if(decl.declaration == DXBC::OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
        {
          topo = (D3D11_PRIMITIVE_TOPOLOGY) int(decl.outTopology);    // enums match
          break;
        }
      }
    }
    else if(lastShader == dxbcDS)
    {
      for(size_t i = 0; i < dxbcDS->GetNumDeclarations(); i++)
      {
        const DXBC::ASMDecl &decl = dxbcDS->GetDeclaration(i);

        if(decl.declaration == DXBC::OPCODE_DCL_TESS_DOMAIN)
        {
          if(decl.domain == DXBC::DOMAIN_ISOLINE)
            topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
          else
            topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
          break;
        }
      }
    }

    m_PostVSData[eventID].gsout.topo = topo;

    // streamout expands strips unfortunately
    if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
      m_PostVSData[eventID].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP)
      m_PostVSData[eventID].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
      m_PostVSData[eventID].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
      m_PostVSData[eventID].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

    switch(m_PostVSData[eventID].gsout.topo)
    {
      case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
        m_PostVSData[eventID].gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten;
        break;
      case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
      case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
        m_PostVSData[eventID].gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten * 2;
        break;
      default:
      case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
      case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
        m_PostVSData[eventID].gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten * 3;
        break;
    }

    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventID].gsout.numVerts /= RDCMAX(1U, drawcall->numInstances);

    m_PostVSData[eventID].gsout.instData = instData;
  }
}

void D3D11DebugManager::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                                   const MeshDisplay &cfg)
{
  if(cfg.position.buf == ResourceId() || cfg.position.numVerts == 0)
    return;

  DebugVertexCBuffer vertexData;

  D3D11RenderStateTracker tracker(m_WrappedContext);

  vertexData.LineStrip = 0;

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(GetWidth()) / float(GetHeight()));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f guessProjInv;

  vertexData.ModelViewProj = projMat.Mul(camMat);
  vertexData.SpriteSize = Vec2f();

  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;
  pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 0.0f);
  FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

  m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
  m_pImmediateContext->PSSetShader(m_DebugRender.WireframePS, NULL, 0);

  m_pImmediateContext->HSSetShader(NULL, NULL, 0);
  m_pImmediateContext->DSSetShader(NULL, NULL, 0);
  m_pImmediateContext->GSSetShader(NULL, NULL, 0);

  m_pImmediateContext->OMSetDepthStencilState(NULL, 0);
  m_pImmediateContext->OMSetBlendState(m_WireframeHelpersBS, NULL, 0xffffffff);

  // don't cull in wireframe mesh display
  m_pImmediateContext->RSSetState(m_WireframeHelpersRS);

  ResourceFormat resFmt;
  resFmt.compByteWidth = cfg.position.compByteWidth;
  resFmt.compCount = cfg.position.compCount;
  resFmt.compType = cfg.position.compType;
  resFmt.special = false;
  if(cfg.position.specialFormat != SpecialFormat::Unknown)
  {
    resFmt.special = true;
    resFmt.specialFormat = cfg.position.specialFormat;
  }

  ResourceFormat resFmt2;
  resFmt2.compByteWidth = cfg.second.compByteWidth;
  resFmt2.compCount = cfg.second.compCount;
  resFmt2.compType = cfg.second.compType;
  resFmt2.special = false;
  if(cfg.second.specialFormat != SpecialFormat::Unknown)
  {
    resFmt2.special = true;
    resFmt2.specialFormat = cfg.second.specialFormat;
  }

  if(m_PrevMeshFmt != resFmt || m_PrevMeshFmt2 != resFmt2)
  {
    SAFE_RELEASE(m_MeshDisplayLayout);

    D3D11_INPUT_ELEMENT_DESC layoutdesc[2];

    layoutdesc[0].SemanticName = "pos";
    layoutdesc[0].SemanticIndex = 0;
    layoutdesc[0].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if(cfg.position.buf != ResourceId() &&
       (cfg.position.specialFormat != SpecialFormat::Unknown || cfg.position.compCount > 0))
      layoutdesc[0].Format = MakeDXGIFormat(resFmt);
    layoutdesc[0].AlignedByteOffset = 0;    // offset will be handled by vertex buffer offset
    layoutdesc[0].InputSlot = 0;
    layoutdesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    layoutdesc[0].InstanceDataStepRate = 0;

    layoutdesc[1].SemanticName = "sec";
    layoutdesc[1].SemanticIndex = 0;
    layoutdesc[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if(cfg.second.buf != ResourceId() &&
       (cfg.second.specialFormat != SpecialFormat::Unknown || cfg.second.compCount > 0))
      layoutdesc[1].Format = MakeDXGIFormat(resFmt2);
    layoutdesc[1].AlignedByteOffset = 0;
    layoutdesc[1].InputSlot = 1;
    layoutdesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    layoutdesc[1].InstanceDataStepRate = 0;

    HRESULT hr = m_pDevice->CreateInputLayout(layoutdesc, 2, m_DebugRender.MeshVSBytecode,
                                              m_DebugRender.MeshVSBytelen, &m_MeshDisplayLayout);

    if(FAILED(hr))
    {
      RDCERR("Failed to create m_MeshDisplayLayout %08x", hr);
      m_MeshDisplayLayout = NULL;
    }
  }

  m_PrevMeshFmt = resFmt;
  m_PrevMeshFmt2 = resFmt2;

  RDCASSERT(cfg.position.idxoffs < 0xffffffff);

  ID3D11Buffer *ibuf = NULL;
  DXGI_FORMAT ifmt = DXGI_FORMAT_R16_UINT;
  UINT ioffs = (UINT)cfg.position.idxoffs;

  D3D11_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(cfg.position.topo);

  // render the mesh itself (solid, then wireframe)
  {
    if(cfg.position.unproject)
    {
      // the derivation of the projection matrix might not be right (hell, it could be an
      // orthographic projection). But it'll be close enough likely.
      Matrix4f guessProj =
          cfg.position.farPlane != FLT_MAX
              ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane,
                                      cfg.aspect)
              : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

      if(cfg.ortho)
      {
        guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
      }

      guessProjInv = guessProj.Inverse();

      vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
    }

    FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

    m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

    Vec4f meshColour;

    ID3D11Buffer *meshColourBuf = MakeCBuffer(&meshColour, sizeof(Vec4f));

    m_pImmediateContext->VSSetShader(m_DebugRender.MeshVS, NULL, 0);
    m_pImmediateContext->PSSetShader(m_DebugRender.MeshPS, NULL, 0);

    // secondary draws - this is the "draw since last clear" feature. We don't have
    // full flexibility, it only draws wireframe, and only the final rasterized position.
    if(secondaryDraws.size() > 0)
    {
      m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);

      pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      for(size_t i = 0; i < secondaryDraws.size(); i++)
      {
        const MeshFormat &fmt = secondaryDraws[i];

        if(fmt.buf != ResourceId())
        {
          meshColour = Vec4f(fmt.meshColor.x, fmt.meshColor.y, fmt.meshColor.z, 1.0f);
          FillCBuffer(meshColourBuf, &meshColour, sizeof(meshColour));
          m_pImmediateContext->PSSetConstantBuffers(2, 1, &meshColourBuf);

          m_pImmediateContext->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(fmt.topo));

          auto it = WrappedID3D11Buffer::m_BufferList.find(fmt.buf);

          ID3D11Buffer *buf = it->second.m_Buffer;
          m_pImmediateContext->IASetVertexBuffers(0, 1, &buf, (UINT *)&fmt.stride,
                                                  (UINT *)&fmt.offset);
          if(fmt.idxbuf != ResourceId())
          {
            RDCASSERT(fmt.idxoffs < 0xffffffff);

            it = WrappedID3D11Buffer::m_BufferList.find(fmt.idxbuf);
            buf = it->second.m_Buffer;
            m_pImmediateContext->IASetIndexBuffer(
                buf, fmt.idxByteWidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
                (UINT)fmt.idxoffs);

            m_pImmediateContext->DrawIndexed(fmt.numVerts, 0, fmt.baseVertex);
          }
          else
          {
            m_pImmediateContext->Draw(fmt.numVerts, 0);
          }
        }
      }
    }

    ID3D11InputLayout *layout = m_MeshDisplayLayout;

    if(layout == NULL)
    {
      RDCWARN("Couldn't get a mesh display layout");
      return;
    }

    m_pImmediateContext->IASetInputLayout(layout);

    RDCASSERT(cfg.position.offset < 0xffffffff && cfg.second.offset < 0xffffffff);

    ID3D11Buffer *vbs[2] = {NULL, NULL};
    UINT str[] = {cfg.position.stride, cfg.second.stride};
    UINT offs[] = {(UINT)cfg.position.offset, (UINT)cfg.second.offset};

    {
      auto it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.buf);

      if(it != WrappedID3D11Buffer::m_BufferList.end())
        vbs[0] = it->second.m_Buffer;

      it = WrappedID3D11Buffer::m_BufferList.find(cfg.second.buf);

      if(it != WrappedID3D11Buffer::m_BufferList.end())
        vbs[1] = it->second.m_Buffer;

      it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.idxbuf);

      if(it != WrappedID3D11Buffer::m_BufferList.end())
        ibuf = it->second.m_Buffer;

      if(cfg.position.idxByteWidth == 4)
        ifmt = DXGI_FORMAT_R32_UINT;
    }

    m_pImmediateContext->IASetVertexBuffers(0, 2, vbs, str, offs);
    if(ibuf)
      m_pImmediateContext->IASetIndexBuffer(ibuf, ifmt, ioffs);
    else
      m_pImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, NULL);

    // draw solid shaded mode
    if(cfg.solidShadeMode != SolidShade::NoSolid && cfg.position.topo < Topology::PatchList_1CPs)
    {
      m_pImmediateContext->RSSetState(m_DebugRender.RastState);

      m_pImmediateContext->IASetPrimitiveTopology(topo);

      pixelData.OutputDisplayFormat = (int)cfg.solidShadeMode;
      if(cfg.solidShadeMode == SolidShade::Secondary && cfg.second.showAlpha)
        pixelData.OutputDisplayFormat = MESHDISPLAY_SECONDARY_ALPHA;
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      meshColour = Vec4f(0.8f, 0.8f, 0.0f, 1.0f);
      FillCBuffer(meshColourBuf, &meshColour, sizeof(meshColour));
      m_pImmediateContext->PSSetConstantBuffers(2, 1, &meshColourBuf);

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

      if(cfg.solidShadeMode == SolidShade::Lit)
      {
        DebugGeometryCBuffer geomData;

        geomData.InvProj = projMat.Inverse();

        FillCBuffer(m_DebugRender.GenericGSCBuffer, &geomData, sizeof(DebugGeometryCBuffer));
        m_pImmediateContext->GSSetConstantBuffers(0, 1, &m_DebugRender.GenericGSCBuffer);

        m_pImmediateContext->GSSetShader(m_DebugRender.MeshGS, NULL, 0);
      }

      if(cfg.position.idxByteWidth)
        m_pImmediateContext->DrawIndexed(cfg.position.numVerts, 0, cfg.position.baseVertex);
      else
        m_pImmediateContext->Draw(cfg.position.numVerts, 0);

      if(cfg.solidShadeMode == SolidShade::Lit)
        m_pImmediateContext->GSSetShader(NULL, NULL, 0);
    }

    // draw wireframe mode
    if(cfg.solidShadeMode == SolidShade::NoSolid || cfg.wireframeDraw ||
       cfg.position.topo >= Topology::PatchList_1CPs)
    {
      m_pImmediateContext->RSSetState(m_WireframeHelpersRS);

      m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.LEqualDepthState, 0);

      pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      meshColour =
          Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y, cfg.position.meshColor.z, 1.0f);
      FillCBuffer(meshColourBuf, &meshColour, sizeof(meshColour));
      m_pImmediateContext->PSSetConstantBuffers(2, 1, &meshColourBuf);

      m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

      if(cfg.position.topo >= Topology::PatchList_1CPs)
        m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      else
        m_pImmediateContext->IASetPrimitiveTopology(topo);

      if(cfg.position.idxByteWidth)
        m_pImmediateContext->DrawIndexed(cfg.position.numVerts, 0, cfg.position.baseVertex);
      else
        m_pImmediateContext->Draw(cfg.position.numVerts, 0);
    }
  }

  m_pImmediateContext->RSSetState(m_WireframeHelpersRS);

  // set up state for drawing helpers
  {
    vertexData.ModelViewProj = projMat.Mul(camMat);
    FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

    m_pImmediateContext->RSSetState(m_SolidHelpersRS);

    m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.NoDepthState, 0);

    m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);
    m_pImmediateContext->VSSetShader(m_DebugRender.MeshVS, NULL, 0);
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
    m_pImmediateContext->PSSetShader(m_DebugRender.WireframePS, NULL, 0);
  }

  // axis markers
  if(!cfg.position.unproject)
  {
    m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

    UINT strides[] = {sizeof(Vec4f)};
    UINT offsets[] = {0};

    m_pImmediateContext->IASetVertexBuffers(0, 1, &m_AxisHelper, strides, offsets);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);

    pixelData.WireframeColour = Vec3f(1.0f, 0.0f, 0.0f);
    FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));
    m_pImmediateContext->Draw(2, 0);

    pixelData.WireframeColour = Vec3f(0.0f, 1.0f, 0.0f);
    FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));
    m_pImmediateContext->Draw(2, 2);

    pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 1.0f);
    FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));
    m_pImmediateContext->Draw(2, 4);
  }

  if(cfg.highlightVert != ~0U)
  {
    m_HighlightCache.CacheHighlightingData(eventID, cfg);

    D3D11_PRIMITIVE_TOPOLOGY meshtopo = topo;

    ///////////////////////////////////////////////////////////////
    // vectors to be set from buffers, depending on topology

    // this vert (blue dot, required)
    FloatVector activeVertex;

    // primitive this vert is a part of (red prim, optional)
    vector<FloatVector> activePrim;

    // for patch lists, to show other verts in patch (green dots, optional)
    // for non-patch lists, we use the activePrim and adjacentPrimVertices
    // to show what other verts are related
    vector<FloatVector> inactiveVertices;

    // adjacency (line or tri, strips or lists) (green prims, optional)
    // will be N*M long, N adjacent prims of M verts each. M = primSize below
    vector<FloatVector> adjacentPrimVertices;

    D3D11_PRIMITIVE_TOPOLOGY primTopo =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;    // tri or line list
    uint32_t primSize = 3;                        // number of verts per primitive

    if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST ||
       meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ ||
       meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP ||
       meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
    {
      primSize = 2;
      primTopo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    }

    bool valid = m_HighlightCache.FetchHighlightPositions(cfg, activeVertex, activePrim,
                                                          adjacentPrimVertices, inactiveVertices);

    if(valid)
    {
      ////////////////////////////////////////////////////////////////
      // prepare rendering (for both vertices & primitives)

      // if data is from post transform, it will be in clipspace
      if(cfg.position.unproject)
        vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
      else
        vertexData.ModelViewProj = projMat.Mul(camMat);

      m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);

      FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

      D3D11_MAPPED_SUBRESOURCE mapped;
      HRESULT hr = S_OK;
      UINT strides[] = {sizeof(Vec4f)};
      UINT offsets[] = {0};
      m_pImmediateContext->IASetVertexBuffers(0, 1, &m_TriHighlightHelper, (UINT *)&strides,
                                              (UINT *)&offsets);

      ////////////////////////////////////////////////////////////////
      // render primitives

      m_pImmediateContext->IASetPrimitiveTopology(primTopo);

      // Draw active primitive (red)
      pixelData.WireframeColour = Vec3f(1.0f, 0.0f, 0.0f);
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      if(activePrim.size() >= primSize)
      {
        hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
          return;
        }

        memcpy(mapped.pData, &activePrim[0], sizeof(Vec4f) * primSize);
        m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

        m_pImmediateContext->Draw(primSize, 0);
      }

      // Draw adjacent primitives (green)
      pixelData.WireframeColour = Vec3f(0.0f, 1.0f, 0.0f);
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
          return;
        }

        memcpy(mapped.pData, &adjacentPrimVertices[0], sizeof(Vec4f) * adjacentPrimVertices.size());
        m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

        m_pImmediateContext->Draw((UINT)adjacentPrimVertices.size(), 0);
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots (set new VS params and topology)
      float scale = 800.0f / float(GetHeight());
      float asp = float(GetWidth()) / float(GetHeight());

      vertexData.SpriteSize = Vec2f(scale / asp, scale);
      FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

      // Draw active vertex (blue)
      pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 1.0f);
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      FloatVector vertSprite[4] = {
          activeVertex, activeVertex, activeVertex, activeVertex,
      };

      hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

      if(FAILED(hr))
      {
        RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
        return;
      }

      memcpy(mapped.pData, vertSprite, sizeof(vertSprite));
      m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

      m_pImmediateContext->Draw(4, 0);

      // Draw inactive vertices (green)
      pixelData.WireframeColour = Vec3f(0.0f, 1.0f, 0.0f);
      FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

      for(size_t i = 0; i < inactiveVertices.size(); i++)
      {
        vertSprite[0] = vertSprite[1] = vertSprite[2] = vertSprite[3] = inactiveVertices[i];

        hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

        if(FAILED(hr))
        {
          RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
          return;
        }

        memcpy(mapped.pData, vertSprite, sizeof(vertSprite));
        m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

        m_pImmediateContext->Draw(4, 0);
      }
    }

    if(cfg.position.unproject)
      m_pImmediateContext->VSSetShader(m_DebugRender.MeshVS, NULL, 0);
  }

  // bounding box
  if(cfg.showBBox)
  {
    UINT strides[] = {sizeof(Vec4f)};
    UINT offsets[] = {0};
    D3D11_MAPPED_SUBRESOURCE mapped;

    vertexData.SpriteSize = Vec2f();
    vertexData.ModelViewProj = projMat.Mul(camMat);
    FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

    HRESULT hr =
        m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    RDCASSERTEQUAL(hr, S_OK);

    Vec4f a = Vec4f(cfg.minBounds.x, cfg.minBounds.y, cfg.minBounds.z, cfg.minBounds.w);
    Vec4f b = Vec4f(cfg.maxBounds.x, cfg.maxBounds.y, cfg.maxBounds.z, cfg.maxBounds.w);

    Vec4f TLN = Vec4f(a.x, b.y, a.z, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(b.x, b.y, a.z, 1.0f);
    Vec4f BLN = Vec4f(a.x, a.y, a.z, 1.0f);
    Vec4f BRN = Vec4f(b.x, a.y, a.z, 1.0f);

    Vec4f TLF = Vec4f(a.x, b.y, b.z, 1.0f);
    Vec4f TRF = Vec4f(b.x, b.y, b.z, 1.0f);
    Vec4f BLF = Vec4f(a.x, a.y, b.z, 1.0f);
    Vec4f BRF = Vec4f(b.x, a.y, b.z, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    memcpy(mapped.pData, bbox, sizeof(bbox));

    m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

    // we want this to clip
    m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.LEqualDepthState, 0);

    m_pImmediateContext->IASetVertexBuffers(0, 1, &m_TriHighlightHelper, (UINT *)&strides,
                                            (UINT *)&offsets);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);

    pixelData.WireframeColour = Vec3f(0.2f, 0.2f, 1.0f);
    FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

    m_pImmediateContext->Draw(24, 0);

    m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.NoDepthState, 0);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
  {
    UINT strides[] = {sizeof(Vec4f)};
    UINT offsets[] = {0};

    vertexData.SpriteSize = Vec2f();
    vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
    FillCBuffer(m_DebugRender.GenericVSCBuffer, &vertexData, sizeof(DebugVertexCBuffer));

    m_pImmediateContext->IASetVertexBuffers(0, 1, &m_FrustumHelper, (UINT *)&strides,
                                            (UINT *)&offsets);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);

    pixelData.WireframeColour = Vec3f(1.0f, 1.0f, 1.0f);
    FillCBuffer(m_DebugRender.GenericPSCBuffer, &pixelData, sizeof(DebugPixelCBufferData));

    m_pImmediateContext->Draw(24, 0);
  }
}
