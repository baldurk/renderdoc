/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"
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
      RDCERR("Couldn't create blob of size %u from shadercache: %s", size, ToStr(ret).c_str());
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

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.0f);

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

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 1.0f);

  if(RenderDoc::Inst().IsReplayApp())
  {
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
  else
  {
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
  ClearPostVSCache();

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
    RDCERR("Couldn't create vertex shader for %s %s", entry, ToStr(ret).c_str());

    SAFE_RELEASE(byteBlob);

    return NULL;
  }

  if(numInputDescs)
  {
    hr = m_pDevice->CreateInputLayout(inputs, numInputDescs, bytecode, bytecodeLen, ret);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create input layout for %s %s", entry, ToStr(ret).c_str());
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
    RDCERR("Couldn't create geometry shader for %s %s", entry, ToStr(hr).c_str());
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
    RDCERR("Couldn't create pixel shader for %s %s", entry, ToStr(hr).c_str());
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
    RDCERR("Couldn't create compute shader for %s %s", entry, ToStr(hr).c_str());
    return NULL;
  }

  return cs;
}

void D3D11DebugManager::BuildShader(string source, string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, string *errors)
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
  *errors = GetShaderBlob(source.c_str(), entry.c_str(), flags, profile, &blob);

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
    RDCERR("Failed to create CBuffer HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
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

    RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.1f);

    for(int t = eTexType_1D; t < eTexType_Max; t++)
    {
      if(t == eTexType_Unused)
        continue;

      // float, uint, sint
      for(int i = 0; i < 3; i++)
      {
        string hlsl = std::string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
        hlsl += std::string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
        hlsl += std::string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
        hlsl += histogramhlsl;

        m_DebugRender.TileMinMaxCS[t][i] =
            MakeCShader(hlsl.c_str(), "RENDERDOC_TileMinMaxCS", "cs_5_0");
        m_DebugRender.HistogramCS[t][i] =
            MakeCShader(hlsl.c_str(), "RENDERDOC_HistogramCS", "cs_5_0");

        if(t == 1)
          m_DebugRender.ResultMinMaxCS[i] =
              MakeCShader(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS", "cs_5_0");

        RenderDoc::Inst().SetProgress(
            LoadProgress::DebugManagerInit,
            (float(i + 3.0f * t) / float(2.0f + 3.0f * (eTexType_Max - 1))) * 0.7f + 0.1f);
      }
    }
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.8f);

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
    RDCERR("Failed to create default blendstate HRESULT: %s", ToStr(hr).c_str());
  }

  blendDesc.RenderTarget[0].BlendEnable = FALSE;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;

  hr = m_pDevice->CreateBlendState(&blendDesc, &m_DebugRender.NopBlendState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create nop blendstate HRESULT: %s", ToStr(hr).c_str());
  }

  D3D11_RASTERIZER_DESC rastDesc;
  RDCEraseEl(rastDesc);

  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.FillMode = D3D11_FILL_SOLID;
  rastDesc.DepthBias = 0;

  hr = m_pDevice->CreateRasterizerState(&rastDesc, &m_DebugRender.RastState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create default rasterizer state HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Failed to create linear sampler state HRESULT: %s", ToStr(hr).c_str());
  }

  sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

  hr = m_pDevice->CreateSamplerState(&sampDesc, &m_DebugRender.PointSampState);

  if(FAILED(hr))
  {
    RDCERR("Failed to create point sampler state HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create no-depth depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.LEqualDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create less-equal depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = TRUE;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.AllPassDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilReadMask = desc.StencilWriteMask = 0;
    desc.StencilEnable = FALSE;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.NopDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create nop depthstencilstate HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create always pass stencil increment depthstencilstate HRESULT: %s",
             ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

    hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.StencIncrEqDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate HRESULT: %s",
             ToStr(hr).c_str());
    }
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.9f);

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
      RDCERR("Failed to create pick tex HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      hr = m_pDevice->CreateRenderTargetView(pickTex, NULL, &m_DebugRender.PickPixelRT);

      if(FAILED(hr))
      {
        RDCERR("Failed to create pick rt HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create pick stage tex HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create tile result buffer HRESULT: %s", ToStr(hr).c_str());
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = bDesc.ByteWidth / sizeof(Vec4f);

    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc,
                                             &m_DebugRender.tileResultSRV[0]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result SRV 0 HRESULT: %s", ToStr(hr).c_str());

    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc,
                                             &m_DebugRender.tileResultSRV[1]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result SRV 1 HRESULT: %s", ToStr(hr).c_str());

    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc,
                                             &m_DebugRender.tileResultSRV[2]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result SRV 2 HRESULT: %s", ToStr(hr).c_str());

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.Flags = 0;
    uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements;

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc,
                                              &m_DebugRender.tileResultUAV[0]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result UAV 0 HRESULT: %s", ToStr(hr).c_str());

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc,
                                              &m_DebugRender.tileResultUAV[1]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result UAV 1 HRESULT: %s", ToStr(hr).c_str());

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc,
                                              &m_DebugRender.tileResultUAV[2]);

    if(FAILED(hr))
      RDCERR("Failed to create tile result UAV 2 HRESULT: %s", ToStr(hr).c_str());

    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    uavDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
    bDesc.ByteWidth = uavDesc.Buffer.NumElements * sizeof(int);

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.histogramBuff);

    if(FAILED(hr))
      RDCERR("Failed to create histogram buff HRESULT: %s", ToStr(hr).c_str());

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.histogramBuff, &uavDesc,
                                              &m_DebugRender.histogramUAV);

    if(FAILED(hr))
      RDCERR("Failed to create histogram UAV HRESULT: %s", ToStr(hr).c_str());

    bDesc.BindFlags = 0;
    bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bDesc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.histogramStageBuff);

    if(FAILED(hr))
      RDCERR("Failed to create histogram stage buff HRESULT: %s", ToStr(hr).c_str());

    bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bDesc.CPUAccessFlags = 0;
    bDesc.ByteWidth = 2 * 4 * sizeof(float);
    bDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.resultBuff);

    if(FAILED(hr))
      RDCERR("Failed to create result buff HRESULT: %s", ToStr(hr).c_str());

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    uavDesc.Buffer.NumElements = 2;

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc,
                                              &m_DebugRender.resultUAV[0]);

    if(FAILED(hr))
      RDCERR("Failed to create result UAV 0 HRESULT: %s", ToStr(hr).c_str());

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc,
                                              &m_DebugRender.resultUAV[1]);

    if(FAILED(hr))
      RDCERR("Failed to create result UAV 1 HRESULT: %s", ToStr(hr).c_str());

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc,
                                              &m_DebugRender.resultUAV[2]);

    if(FAILED(hr))
      RDCERR("Failed to create result UAV 2 HRESULT: %s", ToStr(hr).c_str());

    bDesc.BindFlags = 0;
    bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bDesc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.resultStageBuff);

    if(FAILED(hr))
      RDCERR("Failed to create result stage buff HRESULT: %s", ToStr(hr).c_str());

    bDesc.ByteWidth = sizeof(Vec4f) * DebugRenderData::maxMeshPicks;
    bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bDesc.CPUAccessFlags = 0;
    bDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bDesc.StructureByteStride = sizeof(Vec4f);
    bDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.PickResultBuf);

    if(FAILED(hr))
      RDCERR("Failed to create mesh pick result buff HRESULT: %s", ToStr(hr).c_str());

    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = DebugRenderData::maxMeshPicks;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

    hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.PickResultBuf, &uavDesc,
                                              &m_DebugRender.PickResultUAV);

    if(FAILED(hr))
      RDCERR("Failed to create mesh pick result UAV HRESULT: %s", ToStr(hr).c_str());

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
      RDCERR("Failed to create map staging buffer HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Failed to create m_SOStatsQuery HRESULT: %s", ToStr(hr).c_str());

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
      RDCERR("Failed to create m_WireframeHelpersRS HRESULT: %s", ToStr(hr).c_str());

    desc.FrontCounterClockwise = TRUE;
    desc.CullMode = D3D11_CULL_FRONT;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersCullCCWRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersCullCCWRS HRESULT: %s", ToStr(hr).c_str());

    desc.FrontCounterClockwise = FALSE;
    desc.CullMode = D3D11_CULL_FRONT;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersCullCWRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersCullCCWRS HRESULT: %s", ToStr(hr).c_str());

    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;

    hr = m_pDevice->CreateRasterizerState(&desc, &m_SolidHelpersRS);
    if(FAILED(hr))
      RDCERR("Failed to create m_SolidHelpersRS HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create m_WireframeHelpersRS HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create m_AxisHelper HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create m_FrustumHelper HRESULT: %s", ToStr(hr).c_str());
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
      RDCERR("Failed to create m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Failed to create m_SOBuffer HRESULT: %s", ToStr(hr).c_str());

  bufferDesc.Usage = D3D11_USAGE_STAGING;
  bufferDesc.BindFlags = 0;
  bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  hr = m_pDevice->CreateBuffer(&bufferDesc, NULL, &m_SOStagingBuffer);
  if(FAILED(hr))
    RDCERR("Failed to create m_SOStagingBuffer HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Failed to create debugTex HRESULT: %s", ToStr(hr).c_str());

  delete[] buf;

  hr = m_pDevice->CreateShaderResourceView(debugTex, NULL, &m_Font.Tex);

  if(FAILED(hr))
    RDCERR("Failed to create m_Font.Tex HRESULT: %s", ToStr(hr).c_str());

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

uint32_t D3D11DebugManager::GetStructCount(ID3D11UnorderedAccessView *uav)
{
  m_pImmediateContext->CopyStructureCount(m_DebugRender.StageBuffer, 0, uav);

  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = m_pImmediateContext->Map(m_DebugRender.StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to Map HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Can't map histogram stage buff HRESULT: %s", ToStr(hr).c_str());
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

    m_pImmediateContext->Unmap(m_DebugRender.resultStageBuff, 0);
  }

  return true;
}

void D3D11DebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length,
                                      bytebuf &retData)
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
                                      bytebuf &ret)
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
      RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Failed to map charbuffer HRESULT: %s", ToStr(hr).c_str());
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

void D3D11DebugManager::RenderCheckerboard()
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

  pixelData.Channels = RenderDoc::Inst().LightCheckerboardColor();
  pixelData.WireframeColour = RenderDoc::Inst().DarkCheckerboardColor();

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

void D3D11DebugManager::RenderForPredicate()
{
  // just somehow draw a quad that renders some pixels to fill the predicate with TRUE
  m_WrappedContext->ClearState();
  D3D11_VIEWPORT viewport = {0, 0, 1, 1, 0.0f, 1.0f};
  m_WrappedContext->RSSetViewports(1, &viewport);
  m_WrappedContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
  m_WrappedContext->PSSetShader(m_DebugRender.WireframePS, NULL, 0);
  m_WrappedContext->OMSetRenderTargets(1, &m_DebugRender.PickPixelRT, NULL);
  m_WrappedContext->Draw(3, 0);
}

void D3D11DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                                             const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars, const bytebuf &data)
{
  using namespace DXBC;
  using namespace ShaderDebug;

  size_t o = offset;

  for(size_t v = 0; v < invars.size(); v++)
  {
    size_t vec = o + invars[v].descriptor.offset / 16;
    size_t comp = (invars[v].descriptor.offset - (invars[v].descriptor.offset & ~0xf)) / 4;
    size_t sz = RDCMAX(1U, invars[v].type.descriptor.bytesize / 16);

    offset = vec + sz;

    string basename = prefix + invars[v].name;

    uint32_t rows = invars[v].type.descriptor.rows;
    uint32_t cols = invars[v].type.descriptor.cols;
    uint32_t elems = RDCMAX(1U, invars[v].type.descriptor.elements);

    if(!invars[v].type.members.empty())
    {
      char buf[64] = {0};
      StringFormat::snprintf(buf, 63, "[%d]", elems);

      ShaderVariable var;
      var.name = basename;
      var.rows = var.columns = 0;
      var.type = VarType::Float;

      std::vector<ShaderVariable> varmembers;

      if(elems > 1)
      {
        for(uint32_t i = 0; i < elems; i++)
        {
          StringFormat::snprintf(buf, 63, "[%d]", i);

          if(flatten)
          {
            FillCBufferVariables(basename + buf + ".", vec, flatten, invars[v].type.members,
                                 outvars, data);
          }
          else
          {
            ShaderVariable vr;
            vr.name = basename + buf;
            vr.rows = vr.columns = 0;
            vr.type = VarType::Float;

            std::vector<ShaderVariable> mems;

            FillCBufferVariables("", vec, flatten, invars[v].type.members, mems, data);

            vr.isStruct = true;

            vr.members = mems;

            varmembers.push_back(vr);
          }
        }

        var.isStruct = false;
      }
      else
      {
        var.isStruct = true;

        if(flatten)
          FillCBufferVariables(basename + ".", vec, flatten, invars[v].type.members, outvars, data);
        else
          FillCBufferVariables("", vec, flatten, invars[v].type.members, varmembers, data);
      }

      if(!flatten)
      {
        var.members = varmembers;
        outvars.push_back(var);
      }

      continue;
    }

    if(invars[v].type.descriptor.varClass == CLASS_OBJECT ||
       invars[v].type.descriptor.varClass == CLASS_STRUCT ||
       invars[v].type.descriptor.varClass == CLASS_INTERFACE_CLASS ||
       invars[v].type.descriptor.varClass == CLASS_INTERFACE_POINTER)
    {
      RDCWARN("Unexpected variable '%s' of class '%u' in cbuffer, skipping.",
              invars[v].name.c_str(), invars[v].type.descriptor.type);
      continue;
    }

    size_t elemByteSize = 4;
    VarType type = VarType::Float;
    switch(invars[v].type.descriptor.type)
    {
      case VARTYPE_MIN12INT:
      case VARTYPE_MIN16INT:
      case VARTYPE_INT: type = VarType::Int; break;
      case VARTYPE_MIN8FLOAT:
      case VARTYPE_MIN10FLOAT:
      case VARTYPE_MIN16FLOAT:
      case VARTYPE_FLOAT: type = VarType::Float; break;
      case VARTYPE_BOOL:
      case VARTYPE_UINT:
      case VARTYPE_UINT8:
      case VARTYPE_MIN16UINT: type = VarType::UInt; break;
      case VARTYPE_DOUBLE:
        elemByteSize = 8;
        type = VarType::Double;
        break;
      default:
        RDCERR("Unexpected type %d for variable '%s' in cbuffer", invars[v].type.descriptor.type,
               invars[v].name.c_str());
    }

    bool columnMajor = invars[v].type.descriptor.varClass == CLASS_MATRIX_COLUMNS;

    size_t outIdx = vec;
    if(!flatten)
    {
      outIdx = outvars.size();
      outvars.resize(RDCMAX(outIdx + 1, outvars.size()));
    }
    else
    {
      if(columnMajor)
        outvars.resize(RDCMAX(outIdx + cols * elems, outvars.size()));
      else
        outvars.resize(RDCMAX(outIdx + rows * elems, outvars.size()));
    }

    size_t dataOffset = vec * sizeof(Vec4f) + comp * sizeof(float);

    if(!outvars[outIdx].name.empty())
    {
      RDCASSERT(flatten);

      RDCASSERT(outvars[vec].rows == 1);
      RDCASSERT(outvars[vec].columns == comp);
      RDCASSERT(rows == 1);

      std::string combinedName = outvars[outIdx].name;
      combinedName += ", " + basename;
      outvars[outIdx].name = combinedName;
      outvars[outIdx].rows = 1;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns += cols;

      if(dataOffset < data.size())
      {
        const byte *d = &data[dataOffset];

        memcpy(&outvars[outIdx].value.uv[comp], d,
               RDCMIN(data.size() - dataOffset, elemByteSize * cols));
      }
    }
    else
    {
      outvars[outIdx].name = basename;
      outvars[outIdx].rows = 1;
      outvars[outIdx].type = type;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns = cols;

      ShaderVariable &var = outvars[outIdx];

      bool isArray = invars[v].type.descriptor.elements > 1;

      if(rows * elems == 1)
      {
        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          memcpy(&outvars[outIdx].value.uv[flatten ? comp : 0], d,
                 RDCMIN(data.size() - dataOffset, elemByteSize * cols));
        }
      }
      else if(!isArray && !flatten)
      {
        outvars[outIdx].rows = rows;

        if(dataOffset < data.size())
        {
          const byte *d = &data[dataOffset];

          RDCASSERT(rows <= 4 && rows * cols <= 16);

          if(columnMajor)
          {
            uint32_t tmp[16] = {0};

            // matrices always have 4 columns, for padding reasons (the same reason arrays
            // put every element on a new vec4)
            for(uint32_t c = 0; c < cols; c++)
            {
              size_t srcoffs = 4 * elemByteSize * c;
              size_t dstoffs = rows * elemByteSize * c;
              memcpy((byte *)(tmp) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * rows));
            }

            // transpose
            for(size_t r = 0; r < rows; r++)
              for(size_t c = 0; c < cols; c++)
                outvars[outIdx].value.uv[r * cols + c] = tmp[c * rows + r];
          }
          else    // CLASS_MATRIX_ROWS or other data not to transpose.
          {
            // matrices always have 4 columns, for padding reasons (the same reason arrays
            // put every element on a new vec4)
            for(uint32_t r = 0; r < rows; r++)
            {
              size_t srcoffs = 4 * elemByteSize * r;
              size_t dstoffs = cols * elemByteSize * r;
              memcpy((byte *)(&outvars[outIdx].value.uv[0]) + dstoffs, d + srcoffs,
                     RDCMIN(data.size() - dataOffset + srcoffs, elemByteSize * cols));
            }
          }
        }
      }
      else if(rows * elems > 1)
      {
        char buf[64] = {0};

        var.name = outvars[outIdx].name;

        std::vector<ShaderVariable> varmembers;
        std::vector<ShaderVariable> *out = &outvars;
        size_t rowCopy = 1;

        uint32_t registers = rows;
        uint32_t regLen = cols;
        const char *regName = "row";

        std::string base = outvars[outIdx].name;

        if(!flatten)
        {
          var.rows = 0;
          var.columns = 0;
          outIdx = 0;
          out = &varmembers;
          varmembers.resize(elems);
          rowCopy = rows;
          rows = 1;
          registers = 1;
        }
        else
        {
          if(columnMajor)
          {
            registers = cols;
            regLen = rows;
            regName = "col";
          }
        }

        size_t rowDataOffset = vec * sizeof(Vec4f);

        for(size_t r = 0; r < registers * elems; r++)
        {
          if(isArray && registers > 1)
            StringFormat::snprintf(buf, 63, "[%d].%s%d", r / registers, regName, r % registers);
          else if(registers > 1)
            StringFormat::snprintf(buf, 63, ".%s%d", regName, r);
          else
            StringFormat::snprintf(buf, 63, "[%d]", r);

          (*out)[outIdx + r].name = base + buf;
          (*out)[outIdx + r].rows = (uint32_t)rowCopy;
          (*out)[outIdx + r].type = type;
          (*out)[outIdx + r].isStruct = false;
          (*out)[outIdx + r].columns = regLen;

          size_t totalSize = 0;

          if(flatten)
          {
            totalSize = elemByteSize * regLen;
          }
          else
          {
            // in a matrix, each major element before the last takes up a full
            // vec4 at least
            size_t vecSize = elemByteSize * 4;

            if(columnMajor)
              totalSize = vecSize * (cols - 1) + elemByteSize * rowCopy;
            else
              totalSize = vecSize * (rowCopy - 1) + elemByteSize * cols;
          }

          if((rowDataOffset % sizeof(Vec4f) != 0) &&
             (rowDataOffset / sizeof(Vec4f) != (rowDataOffset + totalSize) / sizeof(Vec4f)))
          {
            rowDataOffset = AlignUp(rowDataOffset, sizeof(Vec4f));
          }

          // arrays are also aligned to the nearest Vec4f for each element
          if(!flatten && isArray)
          {
            rowDataOffset = AlignUp(rowDataOffset, sizeof(Vec4f));
          }

          if(rowDataOffset < data.size())
          {
            const byte *d = &data[rowDataOffset];

            memcpy(&((*out)[outIdx + r].value.uv[0]), d,
                   RDCMIN(data.size() - rowDataOffset, totalSize));

            if(!flatten && columnMajor)
            {
              ShaderVariable tmp = (*out)[outIdx + r];

              size_t transposeRows = rowCopy > 1 ? 4 : 1;

              // transpose
              for(size_t ri = 0; ri < transposeRows; ri++)
                for(size_t ci = 0; ci < cols; ci++)
                  (*out)[outIdx + r].value.uv[ri * cols + ci] = tmp.value.uv[ci * transposeRows + ri];
            }
          }

          if(flatten)
          {
            rowDataOffset += sizeof(Vec4f);
          }
          else
          {
            if(columnMajor)
              rowDataOffset += sizeof(Vec4f) * (cols - 1) + sizeof(float) * rowCopy;
            else
              rowDataOffset += sizeof(Vec4f) * (rowCopy - 1) + sizeof(float) * cols;
          }
        }

        if(!flatten)
        {
          var.isStruct = false;
          var.members = varmembers;
        }
      }
    }
  }
}

void D3D11DebugManager::FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars, bool flattenVec4s,
                                             const bytebuf &data)
{
  size_t zero = 0;

  vector<ShaderVariable> v;
  FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

  outvars.reserve(v.size());
  for(size_t i = 0; i < v.size(); i++)
    outvars.push_back(v[i]);
}

uint32_t D3D11DebugManager::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x,
                                       uint32_t y)
{
  if(cfg.position.numIndices == 0)
    return ~0U;

  D3D11RenderStateTracker tracker(m_WrappedContext);

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
  cbuf.PickViewport = Vec2f((float)GetWidth(), (float)GetHeight());
  cbuf.PickIdx = cfg.position.indexByteStride ? 1 : 0;
  cbuf.PickNumVerts = cfg.position.numIndices;
  cbuf.PickUnproject = cfg.position.unproject ? 1 : 0;

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(GetWidth()) / float(GetHeight()));

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

    float pickX = ((float)x) / ((float)GetWidth());
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)GetHeight());
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

  ID3D11Buffer *vb = NULL, *ib = NULL;
  DXGI_FORMAT ifmt = cfg.position.indexByteStride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

  {
    auto it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.vertexResourceId);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      vb = it->second.m_Buffer;

    it = WrappedID3D11Buffer::m_BufferList.find(cfg.position.indexResourceId);

    if(it != WrappedID3D11Buffer::m_BufferList.end())
      ib = it->second.m_Buffer;
  }

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers.
  // In the case of VB we also tightly pack and unpack the data. IB can just be
  // read as R16 or R32 via the SRV so it is just a straight copy

  if(cfg.position.indexByteStride)
  {
    // resize up on demand
    if(m_DebugRender.PickIBBuf == NULL ||
       m_DebugRender.PickIBSize < cfg.position.numIndices * cfg.position.indexByteStride)
    {
      SAFE_RELEASE(m_DebugRender.PickIBBuf);
      SAFE_RELEASE(m_DebugRender.PickIBSRV);

      D3D11_BUFFER_DESC desc = {cfg.position.numIndices * cfg.position.indexByteStride,
                                D3D11_USAGE_DEFAULT,
                                D3D11_BIND_SHADER_RESOURCE,
                                0,
                                0,
                                0};

      m_DebugRender.PickIBSize = cfg.position.numIndices * cfg.position.indexByteStride;

      hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.PickIBBuf);

      if(FAILED(hr))
      {
        RDCERR("Failed to create PickIBBuf HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
      sdesc.Format = ifmt;
      sdesc.Buffer.FirstElement = 0;
      sdesc.Buffer.NumElements = cfg.position.numIndices;

      hr = m_pDevice->CreateShaderResourceView(m_DebugRender.PickIBBuf, &sdesc,
                                               &m_DebugRender.PickIBSRV);

      if(FAILED(hr))
      {
        SAFE_RELEASE(m_DebugRender.PickIBBuf);
        RDCERR("Failed to create PickIBSRV HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }
    }

    // copy index data as-is, the view format will take care of the rest

    RDCASSERT(cfg.position.indexByteOffset < 0xffffffff);

    if(ib)
    {
      D3D11_BUFFER_DESC ibdesc;
      ib->GetDesc(&ibdesc);

      D3D11_BOX box;
      box.front = 0;
      box.back = 1;
      box.left = (uint32_t)cfg.position.indexByteOffset;
      box.right = (uint32_t)cfg.position.indexByteOffset +
                  cfg.position.numIndices * cfg.position.indexByteStride;
      box.top = 0;
      box.bottom = 1;

      box.right = RDCMIN(box.right, ibdesc.ByteWidth - (uint32_t)cfg.position.indexByteOffset);

      m_pImmediateContext->CopySubresourceRegion(m_DebugRender.PickIBBuf, 0, 0, 0, 0, ib, 0, &box);
    }
  }

  if(m_DebugRender.PickVBBuf == NULL ||
     m_DebugRender.PickVBSize < cfg.position.numIndices * sizeof(Vec4f))
  {
    SAFE_RELEASE(m_DebugRender.PickVBBuf);
    SAFE_RELEASE(m_DebugRender.PickVBSRV);

    D3D11_BUFFER_DESC desc = {cfg.position.numIndices * sizeof(Vec4f),
                              D3D11_USAGE_DEFAULT,
                              D3D11_BIND_SHADER_RESOURCE,
                              0,
                              0,
                              0};

    m_DebugRender.PickVBSize = cfg.position.numIndices * sizeof(Vec4f);

    hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.PickVBBuf);

    if(FAILED(hr))
    {
      RDCERR("Failed to create PickVBBuf HRESULT: %s", ToStr(hr).c_str());
      return ~0U;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
    sdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    sdesc.Buffer.FirstElement = 0;
    sdesc.Buffer.NumElements = cfg.position.numIndices;

    hr = m_pDevice->CreateShaderResourceView(m_DebugRender.PickVBBuf, &sdesc,
                                             &m_DebugRender.PickVBSRV);

    if(FAILED(hr))
    {
      SAFE_RELEASE(m_DebugRender.PickVBBuf);
      RDCERR("Failed to create PickVBSRV HRESULT: %s", ToStr(hr).c_str());
      return ~0U;
    }
  }

  // unpack and linearise the data
  if(vb)
  {
    FloatVector *vbData = new FloatVector[cfg.position.numIndices];

    bytebuf oldData;
    GetBufferData(vb, cfg.position.vertexByteOffset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numIndices; i++)
    {
      uint32_t idx = i;

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(idx < idxclamp)
        idx = 0;
      else if(cfg.position.baseVertex < 0)
        idx -= idxclamp;
      else if(cfg.position.baseVertex > 0)
        idx += cfg.position.baseVertex;

      vbData[i] = HighlightCache::InterpretVertex(data, idx, cfg, dataEnd, valid);
    }

    D3D11_BOX box;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;
    box.left = 0;
    box.right = cfg.position.numIndices * sizeof(Vec4f);

    m_pImmediateContext->UpdateSubresource(m_DebugRender.PickVBBuf, 0, &box, vbData, sizeof(Vec4f),
                                           sizeof(Vec4f));

    delete[] vbData;
  }

  ID3D11ShaderResourceView *srvs[2] = {m_DebugRender.PickIBSRV, m_DebugRender.PickVBSRV};

  ID3D11Buffer *buf = MakeCBuffer(&cbuf, sizeof(cbuf));

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &buf);

  m_pImmediateContext->CSSetShaderResources(0, 2, srvs);

  UINT reset = 0;
  m_pImmediateContext->CSSetUnorderedAccessViews(0, 1, &m_DebugRender.PickResultUAV, &reset);

  m_pImmediateContext->CSSetShader(m_DebugRender.MeshPickCS, NULL, 0);

  m_pImmediateContext->Dispatch(cfg.position.numIndices / 1024 + 1, 1, 1);

  m_pImmediateContext->CopyStructureCount(m_DebugRender.histogramBuff, 0,
                                          m_DebugRender.PickResultUAV);

  bytebuf results;
  GetBufferData(m_DebugRender.histogramBuff, 0, 0, results);

  uint32_t numResults = *(uint32_t *)&results[0];

  if(numResults > 0)
  {
    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        Vec3f intersectionPoint;
      };

      GetBufferData(m_DebugRender.PickResultBuf, 0, 0, results);

      PickResult *pickResults = (PickResult *)&results[0];

      PickResult *closest = pickResults;

      // distance from raycast hit to nearest worldspace position of the mouse
      float closestPickDistance = (closest->intersectionPoint - rayPos).Length();

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)DebugRenderData::maxMeshPicks, numResults); i++)
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

      GetBufferData(m_DebugRender.PickResultBuf, 0, 0, results);

      PickResult *pickResults = (PickResult *)&results[0];

      PickResult *closest = pickResults;

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)DebugRenderData::maxMeshPicks, numResults); i++)
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

void D3D11DebugManager::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                                  uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  D3D11RenderStateTracker tracker(m_WrappedContext);

  D3D11MarkerRegion marker("PickPixel");

  m_pImmediateContext->OMSetRenderTargets(1, &m_DebugRender.PickPixelRT, NULL);

  float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  m_pImmediateContext->ClearRenderTargetView(m_DebugRender.PickPixelRT, color);

  D3D11_VIEWPORT viewport;
  RDCEraseEl(viewport);

  int oldW = GetWidth(), oldH = GetHeight();

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

    RenderTexture(texDisplay, false);
  }

  D3D11_BOX box;
  box.front = 0;
  box.back = 1;
  box.left = 0;
  box.right = 1;
  box.top = 0;
  box.bottom = 1;

  ID3D11Resource *res = NULL;
  m_DebugRender.PickPixelRT->GetResource(&res);

  m_pImmediateContext->CopySubresourceRegion(m_DebugRender.PickPixelStageTex, 0, 0, 0, 0, res, 0,
                                             &box);

  SAFE_RELEASE(res);

  D3D11_MAPPED_SUBRESOURCE mapped;
  mapped.pData = NULL;
  HRESULT hr =
      m_pImmediateContext->Map(m_DebugRender.PickPixelStageTex, 0, D3D11_MAP_READ, 0, &mapped);

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

  SetOutputDimensions(oldW, oldH);

  m_pImmediateContext->Unmap(m_DebugRender.PickPixelStageTex, 0);
}

void D3D11DebugManager::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                       const GetTextureDataParams &params, bytebuf &data)
{
  D3D11RenderStateTracker tracker(m_WrappedContext);

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
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      desc.Format =
          IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
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
      RDCASSERT(params.remap == RemapTexture::RGBA8);

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

      int oldW = GetWidth(), oldH = GetHeight();
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

        RenderTexture(texDisplay, false);
      }

      SetOutputDimensions(oldW, oldH);

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
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      desc.Format = (IsSRGBFormat(desc.Format) || wrapTex->m_RealDescriptor)
                        ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                        : DXGI_FORMAT_R8G8B8A8_UNORM;
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
      RDCASSERT(params.remap == RemapTexture::RGBA8);

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

      int oldW = GetWidth(), oldH = GetHeight();
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
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = params.typeHint;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        RenderTexture(texDisplay, false);
      }

      SetOutputDimensions(oldW, oldH);

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
      CopyTex2DMSToArray(UNWRAP(WrappedID3D11Texture2D1, d), wrapTex->GetReal());
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
      RDCASSERT(params.remap == RemapTexture::RGBA8);

      desc.Format =
          IsSRGBFormat(desc.Format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
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
      RDCASSERT(params.remap == RemapTexture::RGBA8);

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

      int oldW = GetWidth(), oldH = GetHeight();

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

        RenderTexture(texDisplay, false);

        SAFE_RELEASE(wrappedrtv);
      }

      SetOutputDimensions(oldW, oldH);

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

ResourceId D3D11DebugManager::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                                uint32_t arrayIdx, uint32_t sampleIdx,
                                                CompType typeHint)
{
  TextureShaderDetails details = GetShaderDetails(texid, typeHint, false);

  CreateCustomShaderTex(details.texWidth, details.texHeight);

  D3D11RenderStateTracker tracker(m_WrappedContext);

  {
    D3D11_RENDER_TARGET_VIEW_DESC desc;

    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = mip;

    WrappedID3D11Texture2D1 *wrapped = (WrappedID3D11Texture2D1 *)m_CustomShaderTex;
    HRESULT hr = m_pDevice->CreateRenderTargetView(wrapped, &desc, &m_CustomShaderRTV);

    if(FAILED(hr))
    {
      RDCERR("Failed to create custom shader rtv HRESULT: %s", ToStr(hr).c_str());
      return m_CustomShaderResourceId;
    }
  }

  m_pImmediateContext->OMSetRenderTargets(1, &m_CustomShaderRTV, NULL);

  float clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_pImmediateContext->ClearRenderTargetView(m_CustomShaderRTV, clr);

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

  RenderTexture(disp, true);

  return m_CustomShaderResourceId;
}

void D3D11DebugManager::CreateCustomShaderTex(uint32_t w, uint32_t h)
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

    SAFE_RELEASE(m_CustomShaderRTV);
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
