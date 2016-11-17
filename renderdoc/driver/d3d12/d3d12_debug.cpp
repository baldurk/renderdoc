/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "d3d12_debug.h"
#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "serialise/string_utils.h"
#include "stb/stb_truetype.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"

#include "data/hlsl/debugcbuffers.h"

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);

struct D3D12BlobShaderCallbacks
{
  D3D12BlobShaderCallbacks()
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
} ShaderCache12Callbacks;

extern "C" __declspec(dllexport) HRESULT
    __cdecl RENDERDOC_CreateWrappedDXGIFactory1(REFIID riid, void **ppFactory);

D3D12DebugManager::D3D12DebugManager(WrappedID3D12Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D12DebugManager));

  m_Device = wrapper->GetReal();
  m_ResourceManager = wrapper->GetResourceManager();

  m_OutputWindowID = 1;
  m_DSVID = 0;

  m_WrappedDevice = wrapper;
  m_WrappedDevice->InternalRef();

  m_TexResource = NULL;

  m_width = m_height = 1;
  m_BBFmtIdx = BGRA8_BACKBUFFER;

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.0f);

  m_pFactory = NULL;

  HRESULT hr = S_OK;

  hr = RENDERDOC_CreateWrappedDXGIFactory1(__uuidof(IDXGIFactory4), (void **)&m_pFactory);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DXGI factory! 0x%08x", hr);
  }

  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 1;
  desc.NumDescriptors = 1024;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&rtvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create RTV descriptor heap! 0x%08x", hr);
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&dsvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DSV descriptor heap! 0x%08x", hr);
  }

  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  desc.NumDescriptors = 4096;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&cbvsrvuavHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! 0x%08x", hr);
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&samplerHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create sampler descriptor heap! 0x%08x", hr);
  }

  {
    D3D12_RESOURCE_DESC pickPixelDesc = {};
    pickPixelDesc.Alignment = 0;
    pickPixelDesc.DepthOrArraySize = 1;
    pickPixelDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    pickPixelDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    pickPixelDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    pickPixelDesc.Height = 1;
    pickPixelDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    pickPixelDesc.MipLevels = 1;
    pickPixelDesc.SampleDesc.Count = 1;
    pickPixelDesc.SampleDesc.Quality = 0;
    pickPixelDesc.Width = 1;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &pickPixelDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&m_PickPixelTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create rendering texture for pixel picking, HRESULT: 0x%08x", hr);
      return;
    }

    m_PickPixelRTV = GetCPUHandle(PICK_PIXEL_RTV);

    m_WrappedDevice->CreateRenderTargetView(m_PickPixelTex, NULL, m_PickPixelRTV);
  }

  {
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
    readbackDesc.Width = m_ReadbackSize;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
        __uuidof(ID3D12Resource), (void **)&m_ReadbackBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback buffer, HRESULT: 0x%08x", hr);
      return;
    }

    hr = m_WrappedDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void **)&m_ReadbackAlloc);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback command allocator, HRESULT: 0x%08x", hr);
      return;
    }

    hr = m_WrappedDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_ReadbackAlloc,
                                            NULL, __uuidof(ID3D12GraphicsCommandList),
                                            (void **)&m_ReadbackList);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback command list, HRESULT: 0x%08x", hr);
      return;
    }

    if(m_ReadbackList)
      m_ReadbackList->Close();
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.2f);

  // create fixed samplers, point and linear
  D3D12_CPU_DESCRIPTOR_HANDLE samp;
  samp = samplerHeap->GetCPUDescriptorHandleForHeapStart();

  D3D12_SAMPLER_DESC sampDesc;
  RDCEraseEl(sampDesc);

  sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampDesc.MaxAnisotropy = 1;
  sampDesc.MinLOD = 0;
  sampDesc.MaxLOD = FLT_MAX;
  sampDesc.MipLODBias = 0.0f;
  sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

  m_WrappedDevice->CreateSampler(&sampDesc, samp);

  sampDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;

  samp.ptr += sizeof(D3D12Descriptor);
  m_WrappedDevice->CreateSampler(&sampDesc, samp);

  static const UINT64 bufsize = 2048;

  m_GenericVSCbuffer = MakeCBuffer(bufsize);
  m_GenericPSCbuffer = MakeCBuffer(bufsize);

  RDCCOMPILE_ASSERT(sizeof(DebugVertexCBuffer) <= bufsize, "CBuffer isn't large enough");
  RDCCOMPILE_ASSERT(sizeof(DebugPixelCBufferData) <= bufsize, "CBuffer isn't large enough");
  RDCCOMPILE_ASSERT(sizeof(HistogramCBufferData) <= bufsize, "CBuffer isn't large enough");
  RDCCOMPILE_ASSERT(sizeof(overdrawRamp) <= bufsize, "CBuffer isn't large enough");

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.4f);

  bool success = LoadShaderCache("d3d12shaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, ShaderCache12Callbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  m_CacheShaders = true;

  vector<D3D12_ROOT_PARAMETER1> rootSig;

  D3D12_ROOT_PARAMETER1 param = {};

  // m_GenericVSCbuffer
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.ShaderRegister = 0;
  param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  rootSig.push_back(param);

  // m_GenericPSCbuffer
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.Descriptor.ShaderRegister = 1;

  rootSig.push_back(param);

  ID3DBlob *root = MakeRootSig(rootSig);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature), (void **)&m_CBOnlyRootSig);

  SAFE_RELEASE(root);

  D3D12_DESCRIPTOR_RANGE1 srvrange = {};
  srvrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvrange.BaseShaderRegister = 0;
  srvrange.NumDescriptors = 32;
  srvrange.OffsetInDescriptorsFromTableStart = 0;
  srvrange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &srvrange;

  // SRV
  rootSig.push_back(param);

  D3D12_DESCRIPTOR_RANGE1 samplerrange = {};
  samplerrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerrange.BaseShaderRegister = 0;
  samplerrange.NumDescriptors = 2;
  samplerrange.OffsetInDescriptorsFromTableStart = 0;
  samplerrange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &samplerrange;

  // samplers
  rootSig.push_back(param);

  root = MakeRootSig(rootSig);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature),
                                            (void **)&m_TexDisplayRootSig);

  SAFE_RELEASE(root);

  rootSig.clear();

  // 0: CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.ShaderRegister = 0;
  param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  rootSig.push_back(param);

  // 1: Texture SRVs
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &srvrange;

  rootSig.push_back(param);

  // 2: Samplers
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &samplerrange;

  rootSig.push_back(param);

  // 3: UAVs
  D3D12_DESCRIPTOR_RANGE1 uavrange = {};
  uavrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  uavrange.BaseShaderRegister = 0;
  uavrange.NumDescriptors = 3;
  uavrange.OffsetInDescriptorsFromTableStart = 0;
  uavrange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &uavrange;

  rootSig.push_back(param);

  root = MakeRootSig(rootSig);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature),
                                            (void **)&m_HistogramRootSig);

  SAFE_RELEASE(root);

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.6f);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  RDCEraseEl(pipeDesc);

  string displayhlsl = GetEmbeddedResource(debugcbuffers_h);
  displayhlsl += GetEmbeddedResource(debugcommon_hlsl);
  displayhlsl += GetEmbeddedResource(debugdisplay_hlsl);

  ID3DBlob *GenericVS = NULL;
  ID3DBlob *FullscreenVS = NULL;
  ID3DBlob *TexDisplayPS = NULL;
  ID3DBlob *CheckerboardPS = NULL;
  ID3DBlob *OutlinePS = NULL;

  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_DebugVS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0",
                &GenericVS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_FullscreenVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "vs_5_0", &FullscreenVS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_TexDisplayPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &TexDisplayPS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_CheckerboardPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &CheckerboardPS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_OutlinePS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &OutlinePS);

  RDCASSERT(GenericVS);
  RDCASSERT(FullscreenVS);
  RDCASSERT(TexDisplayPS);
  RDCASSERT(CheckerboardPS);
  RDCASSERT(OutlinePS);

  pipeDesc.pRootSignature = m_TexDisplayRootSig;
  pipeDesc.VS.BytecodeLength = GenericVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = GenericVS->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = TexDisplayPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = TexDisplayPS->GetBufferPointer();
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayBlendPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayBlendPipe! 0x%08x", hr);
  }

  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayPipe! 0x%08x", hr);
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayF32Pipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayF32Pipe! 0x%08x", hr);
  }

  pipeDesc.pRootSignature = m_CBOnlyRootSig;

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  pipeDesc.PS.BytecodeLength = CheckerboardPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = CheckerboardPS->GetBufferPointer();

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_CheckerboardPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_CheckerboardPipe! 0x%08x", hr);
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;

  pipeDesc.VS.BytecodeLength = FullscreenVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = FullscreenVS->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = OutlinePS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = OutlinePS->GetBufferPointer();

  pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_OutlinePipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_OutlinePipe! 0x%08x", hr);
  }

  m_OverlayRenderTex = NULL;
  m_OverlayResourceId = ResourceId();

  string histogramhlsl = GetEmbeddedResource(debugcbuffers_h);
  histogramhlsl += GetEmbeddedResource(debugcommon_hlsl);
  histogramhlsl += GetEmbeddedResource(histogram_hlsl);

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.7f);

  D3D12_COMPUTE_PIPELINE_STATE_DESC compPipeDesc;
  RDCEraseEl(compPipeDesc);

  compPipeDesc.pRootSignature = m_HistogramRootSig;

  RDCEraseEl(m_TileMinMaxPipe);
  RDCEraseEl(m_HistogramPipe);
  RDCEraseEl(m_ResultMinMaxPipe);

  for(int t = RESTYPE_TEX1D; t <= RESTYPE_TEX2D_MS; t++)
  {
    // skip unused cube slot
    if(t == 8)
      continue;

    // float, uint, sint
    for(int i = 0; i < 3; i++)
    {
      ID3DBlob *tile = NULL;
      ID3DBlob *result = NULL;
      ID3DBlob *histogram = NULL;

      string hlsl = string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
      hlsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
      hlsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
      hlsl += histogramhlsl;

      GetShaderBlob(hlsl.c_str(), "RENDERDOC_TileMinMaxCS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                    "cs_5_0", &tile);

      compPipeDesc.CS.BytecodeLength = tile->GetBufferSize();
      compPipeDesc.CS.pShaderBytecode = tile->GetBufferPointer();

      hr = m_WrappedDevice->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                       (void **)&m_TileMinMaxPipe[t][i]);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create m_TileMinMaxPipe! 0x%08x", hr);
      }

      GetShaderBlob(hlsl.c_str(), "RENDERDOC_HistogramCS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0",
                    &histogram);

      compPipeDesc.CS.BytecodeLength = histogram->GetBufferSize();
      compPipeDesc.CS.pShaderBytecode = histogram->GetBufferPointer();

      hr = m_WrappedDevice->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                       (void **)&m_HistogramPipe[t][i]);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create m_HistogramPipe! 0x%08x", hr);
      }

      if(t == 1)
      {
        GetShaderBlob(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                      "cs_5_0", &result);

        compPipeDesc.CS.BytecodeLength = result->GetBufferSize();
        compPipeDesc.CS.pShaderBytecode = result->GetBufferPointer();

        hr = m_WrappedDevice->CreateComputePipelineState(
            &compPipeDesc, __uuidof(ID3D12PipelineState), (void **)&m_ResultMinMaxPipe[i]);

        if(FAILED(hr))
        {
          RDCERR("Couldn't create m_HistogramPipe! 0x%08x", hr);
        }
      }

      SAFE_RELEASE(tile);
      SAFE_RELEASE(histogram);
      SAFE_RELEASE(result);
    }
  }

  SAFE_RELEASE(FullscreenVS);
  SAFE_RELEASE(GenericVS);
  SAFE_RELEASE(TexDisplayPS);
  SAFE_RELEASE(OutlinePS);
  SAFE_RELEASE(CheckerboardPS);

  {
    const uint64_t maxTexDim = 16384;
    const uint64_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
    const uint64_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

    D3D12_RESOURCE_DESC minmaxDesc = {};
    minmaxDesc.Alignment = 0;
    minmaxDesc.DepthOrArraySize = 1;
    minmaxDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    minmaxDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    minmaxDesc.Format = DXGI_FORMAT_UNKNOWN;
    minmaxDesc.Height = 1;
    minmaxDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    minmaxDesc.MipLevels = 1;
    minmaxDesc.SampleDesc.Count = 1;
    minmaxDesc.SampleDesc.Quality = 0;
    minmaxDesc.Width =
        2 * sizeof(Vec4f) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &minmaxDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
        __uuidof(ID3D12Resource), (void **)&m_MinMaxTileBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create tile buffer for min/max, HRESULT: 0x%08x", hr);
      return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE uav = GetCPUHandle(MINMAX_TILE_UAVS);

    D3D12_UNORDERED_ACCESS_VIEW_DESC tileDesc = {};
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    tileDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    tileDesc.Buffer.FirstElement = 0;
    tileDesc.Buffer.NumElements = UINT(minmaxDesc.Width / sizeof(Vec4f));

    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxTileBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxTileBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxTileBuffer, NULL, &tileDesc, uav);

    uav = GetCPUHandle(HISTOGRAM_UAV);

    // re-use the tile buffer for histogram
    tileDesc.Format = DXGI_FORMAT_R32_UINT;
    tileDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxTileBuffer, NULL, &tileDesc, uav);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = UINT(minmaxDesc.Width / sizeof(Vec4f));

    D3D12_CPU_DESCRIPTOR_HANDLE srv = GetCPUHandle(MINMAX_TILE_SRVS);

    m_WrappedDevice->CreateShaderResourceView(m_MinMaxTileBuffer, &srvDesc, srv);

    srv.ptr += sizeof(D3D12Descriptor);
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;

    m_WrappedDevice->CreateShaderResourceView(m_MinMaxTileBuffer, &srvDesc, srv);

    srv.ptr += sizeof(D3D12Descriptor);
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;

    m_WrappedDevice->CreateShaderResourceView(m_MinMaxTileBuffer, &srvDesc, srv);

    minmaxDesc.Width = 2 * sizeof(Vec4f);

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &minmaxDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
        __uuidof(ID3D12Resource), (void **)&m_MinMaxResultBuffer);

    if(FAILED(hr))
    {
      RDCERR("Failed to create result buffer for min/max, HRESULT: 0x%08x", hr);
      return;
    }

    uav = GetCPUHandle(MINMAX_RESULT_UAVS);

    tileDesc.Buffer.NumElements = 2;

    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxResultBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxResultBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxResultBuffer, NULL, &tileDesc, uav);
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.8f);

  // font rendering
  {
    D3D12_HEAP_PROPERTIES uploadHeap;
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    D3D12_HEAP_PROPERTIES defaultHeap = uploadHeap;
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    const int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

    D3D12_RESOURCE_DESC bufDesc;
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = width * height;

    ID3D12Resource *uploadBuf = NULL;

    hr = m_WrappedDevice->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&uploadBuf);

    if(FAILED(hr))
      RDCERR("Failed to create uploadBuf %08x", hr);

    D3D12_RESOURCE_DESC texDesc;
    texDesc.Alignment = 0;
    texDesc.DepthOrArraySize = 1;
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texDesc.Format = DXGI_FORMAT_R8_UNORM;
    texDesc.Height = height;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Width = width;

    hr = m_WrappedDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&m_Font.Tex);

    if(FAILED(hr))
      RDCERR("Failed to create m_Font.Tex %08x", hr);

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

    FillBuffer(uploadBuf, buf, width * height);

    delete[] buf;

    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    D3D12_TEXTURE_COPY_LOCATION dst, src;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = m_Font.Tex;
    dst.SubresourceIndex = 0;

    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.pResource = uploadBuf;
    src.PlacedFootprint.Offset = 0;
    src.PlacedFootprint.Footprint.Width = width;
    src.PlacedFootprint.Footprint.Height = height;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
    src.PlacedFootprint.Footprint.RowPitch = width;

    RDCCOMPILE_ASSERT(
        (width / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == width,
        "Width isn't aligned!");

    list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Transition.pResource = m_Font.Tex;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    list->ResourceBarrier(1, &barrier);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();

    SAFE_RELEASE(uploadBuf);

    D3D12_CPU_DESCRIPTOR_HANDLE srv = GetCPUHandle(FONT_SRV);

    m_WrappedDevice->CreateShaderResourceView(m_Font.Tex, NULL, srv);

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

    FillBuffer(m_Font.GlyphData, &glyphData, sizeof(glyphData));

    for(size_t i = 0; i < ARRAY_COUNT(m_Font.Constants); i++)
      m_Font.Constants[i] = MakeCBuffer(sizeof(FontCBuffer));
    m_Font.CharBuffer = MakeCBuffer(FONT_BUFFER_CHARS * sizeof(uint32_t) * 4);

    m_Font.ConstRingIdx = 0;

    rootSig.clear();

    RDCEraseEl(param);

    // m_Font.Constants
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = 0;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

    rootSig.push_back(param);

    // m_Font.GlyphData
    param.Descriptor.ShaderRegister = 1;
    rootSig.push_back(param);

    // CharBuffer
    param.Descriptor.ShaderRegister = 2;
    rootSig.push_back(param);

    RDCEraseEl(srvrange);
    srvrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvrange.BaseShaderRegister = 0;
    srvrange.NumDescriptors = 1;
    srvrange.OffsetInDescriptorsFromTableStart = FONT_SRV;

    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &srvrange;

    // font SRV
    rootSig.push_back(param);

    RDCEraseEl(samplerrange);
    samplerrange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerrange.BaseShaderRegister = 0;
    samplerrange.NumDescriptors = 2;
    samplerrange.OffsetInDescriptorsFromTableStart = 0;

    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &samplerrange;

    // samplers
    rootSig.push_back(param);

    root = MakeRootSig(rootSig);

    RDCASSERT(root);

    hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                              __uuidof(ID3D12RootSignature),
                                              (void **)&m_Font.RootSig);

    SAFE_RELEASE(root);

    string fullhlsl = "";
    {
      string debugShaderCBuf = GetEmbeddedResource(debugcbuffers_h);
      string textShaderHLSL = GetEmbeddedResource(debugtext_hlsl);

      fullhlsl = debugShaderCBuf + textShaderHLSL;
    }

    ID3DBlob *TextVS = NULL;
    ID3DBlob *TextPS = NULL;

    GetShaderBlob(fullhlsl.c_str(), "RENDERDOC_TextVS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0",
                  &TextVS);
    GetShaderBlob(fullhlsl.c_str(), "RENDERDOC_TextPS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0",
                  &TextPS);

    RDCASSERT(TextVS);
    RDCASSERT(TextPS);

    pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;

    pipeDesc.VS.BytecodeLength = TextVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = TextVS->GetBufferPointer();
    pipeDesc.PS.BytecodeLength = TextPS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = TextPS->GetBufferPointer();

    pipeDesc.pRootSignature = m_Font.RootSig;

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[BGRA8_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create BGRA8 m_Font.Pipe! 0x%08x", hr);

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA8_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create RGBA8 m_Font.Pipe! 0x%08x", hr);

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA16_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create RGBA16 m_Font.Pipe! 0x%08x", hr);

    SAFE_RELEASE(TextVS);
    SAFE_RELEASE(TextPS);
  }

  RenderDoc::Inst().SetProgress(DebugManagerInit, 1.0f);

  m_CacheShaders = false;
}

D3D12DebugManager::~D3D12DebugManager()
{
  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("d3d12shaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    ShaderCache12Callbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      ShaderCache12Callbacks.Destroy(it->second);
  }

  SAFE_RELEASE(m_pFactory);

  SAFE_RELEASE(dsvHeap);
  SAFE_RELEASE(rtvHeap);
  SAFE_RELEASE(cbvsrvuavHeap);
  SAFE_RELEASE(samplerHeap);

  SAFE_RELEASE(m_GenericVSCbuffer);
  SAFE_RELEASE(m_GenericPSCbuffer);

  SAFE_RELEASE(m_TexDisplayBlendPipe);
  SAFE_RELEASE(m_TexDisplayPipe);
  SAFE_RELEASE(m_TexDisplayF32Pipe);
  SAFE_RELEASE(m_TexDisplayRootSig);

  SAFE_RELEASE(m_CBOnlyRootSig);
  SAFE_RELEASE(m_CheckerboardPipe);
  SAFE_RELEASE(m_OutlinePipe);

  SAFE_RELEASE(m_PickPixelTex);

  SAFE_RELEASE(m_HistogramRootSig);
  for(int t = RESTYPE_TEX1D; t <= RESTYPE_TEX2D_MS; t++)
  {
    for(int i = 0; i < 3; i++)
    {
      SAFE_RELEASE(m_TileMinMaxPipe[t][i]);
      SAFE_RELEASE(m_HistogramPipe[t][i]);
      if(t == RESTYPE_TEX1D)
        SAFE_RELEASE(m_ResultMinMaxPipe[i]);
    }
  }
  SAFE_RELEASE(m_MinMaxResultBuffer);
  SAFE_RELEASE(m_MinMaxTileBuffer);

  SAFE_RELEASE(m_TexResource);

  if(m_OverlayResourceId != ResourceId())
    SAFE_RELEASE(m_OverlayRenderTex);

  SAFE_RELEASE(m_ReadbackBuffer);
  SAFE_RELEASE(m_ReadbackAlloc);
  SAFE_RELEASE(m_ReadbackList);

  m_WrappedDevice->InternalRelease();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

string D3D12DebugManager::GetShaderBlob(const char *source, const char *entry,
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

D3D12RootSignature D3D12DebugManager::GetRootSig(const void *data, size_t dataSize)
{
  PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER deserializeRootSig =
      (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12CreateVersionedRootSignatureDeserializer");

  if(deserializeRootSig == NULL)
  {
    RDCERR("Can't get D3D12CreateVersionedRootSignatureDeserializer");
    return D3D12RootSignature();
  }

  ID3D12VersionedRootSignatureDeserializer *deser = NULL;
  HRESULT hr = deserializeRootSig(
      data, dataSize, __uuidof(ID3D12VersionedRootSignatureDeserializer), (void **)&deser);

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get deserializer");
    return D3D12RootSignature();
  }

  D3D12RootSignature ret;

  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *verdesc = NULL;
  hr = deser->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &verdesc);
  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get descriptor");
    return D3D12RootSignature();
  }

  const D3D12_ROOT_SIGNATURE_DESC1 *desc = &verdesc->Desc_1_1;

  ret.Flags = desc->Flags;

  ret.params.resize(desc->NumParameters);

  ret.dwordLength = 0;

  for(size_t i = 0; i < ret.params.size(); i++)
  {
    ret.params[i].MakeFrom(desc->pParameters[i], ret.numSpaces);

    // Descriptor tables cost 1 DWORD each.
    // Root constants cost 1 DWORD each, since they are 32-bit values.
    // Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
    if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      ret.dwordLength++;
    else if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      ret.dwordLength += desc->pParameters[i].Constants.Num32BitValues;
    else
      ret.dwordLength += 2;
  }

  if(desc->NumStaticSamplers > 0)
  {
    ret.samplers.assign(desc->pStaticSamplers, desc->pStaticSamplers + desc->NumStaticSamplers);

    for(size_t i = 0; i < ret.samplers.size(); i++)
      ret.numSpaces = RDCMAX(ret.numSpaces, ret.samplers[i].RegisterSpace + 1);
  }

  SAFE_RELEASE(deser);

  return ret;
}

ID3DBlob *D3D12DebugManager::MakeRootSig(const std::vector<D3D12_ROOT_PARAMETER1> params,
                                         D3D12_ROOT_SIGNATURE_FLAGS Flags, UINT NumStaticSamplers,
                                         D3D12_STATIC_SAMPLER_DESC *StaticSamplers)
{
  PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE serializeRootSig =
      (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12SerializeVersionedRootSignature");

  if(serializeRootSig == NULL)
  {
    RDCERR("Can't get D3D12SerializeVersionedRootSignature");
    return NULL;
  }

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC verdesc;
  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

  D3D12_ROOT_SIGNATURE_DESC1 &desc = verdesc.Desc_1_1;
  desc.Flags = Flags;
  desc.NumStaticSamplers = NumStaticSamplers;
  desc.pStaticSamplers = StaticSamplers;
  desc.NumParameters = (UINT)params.size();
  desc.pParameters = &params[0];

  ID3DBlob *ret = NULL;
  ID3DBlob *errBlob = NULL;
  HRESULT hr = serializeRootSig(&verdesc, &ret, &errBlob);

  if(FAILED(hr))
  {
    string errors = (char *)errBlob->GetBufferPointer();

    string logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCERR("Root signature serialize error:\n%s", logerror.c_str());

    SAFE_RELEASE(errBlob);
    SAFE_RELEASE(ret);
    return NULL;
  }

  SAFE_RELEASE(errBlob);

  return ret;
}

ID3DBlob *D3D12DebugManager::MakeFixedColShader(float overlayConsts[4])
{
  ID3DBlob *ret = NULL;
  std::string hlsl =
      StringFormat::Fmt("float4 main() : SV_Target0 { return float4(%f, %f, %f, %f); }\n",
                        overlayConsts[0], overlayConsts[1], overlayConsts[2], overlayConsts[3]);
  GetShaderBlob(hlsl.c_str(), "main", D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &ret);
  return ret;
}

ID3D12Resource *D3D12DebugManager::MakeCBuffer(UINT64 size)
{
  ID3D12Resource *ret;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC cbDesc;
  cbDesc.Alignment = 0;
  cbDesc.DepthOrArraySize = 1;
  cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  cbDesc.Format = DXGI_FORMAT_UNKNOWN;
  cbDesc.Height = 1;
  cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  cbDesc.MipLevels = 1;
  cbDesc.SampleDesc.Count = 1;
  cbDesc.SampleDesc.Quality = 0;
  cbDesc.Width = size;

  HRESULT hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                        __uuidof(ID3D12Resource), (void **)&ret);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create cbuffer size %llu! 0x%08x", size, hr);
    SAFE_RELEASE(ret);
    return NULL;
  }

  return ret;
}

void D3D12DebugManager::OutputWindow::MakeRTV(bool multisampled)
{
  SAFE_RELEASE(col);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  texDesc.SampleDesc.Count = multisampled ? 1 : 1;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                            __uuidof(ID3D12Resource), (void **)&col);

  if(FAILED(hr))
  {
    RDCERR("Failed to create colour texture for window, HRESULT: 0x%08x", hr);
    return;
  }

  dev->CreateRenderTargetView(col, NULL, rtv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RTV for main window, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

void D3D12DebugManager::OutputWindow::MakeDSV()
{
  SAFE_RELEASE(depth);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.SampleDesc.Count = 1;
  texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                            D3D12_RESOURCE_STATE_DEPTH_WRITE, NULL,
                                            __uuidof(ID3D12Resource), (void **)&depth);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV texture for output window, HRESULT: 0x%08x", hr);
    return;
  }

  dev->CreateDepthStencilView(depth, NULL, dsv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create DSV for output window, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

uint64_t D3D12DebugManager::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  RDCASSERT(system == eWindowingSystem_Win32, system);

  OutputWindow outw;
  outw.wnd = (HWND)data;
  outw.dev = m_WrappedDevice;

  DXGI_SWAP_CHAIN_DESC swapDesc;
  RDCEraseEl(swapDesc);

  RECT rect;
  GetClientRect(outw.wnd, &rect);

  swapDesc.BufferCount = 2;
  swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  outw.width = swapDesc.BufferDesc.Width = rect.right - rect.left;
  outw.height = swapDesc.BufferDesc.Height = rect.bottom - rect.top;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.SampleDesc.Count = 1;
  swapDesc.SampleDesc.Quality = 0;
  swapDesc.OutputWindow = outw.wnd;
  swapDesc.Windowed = TRUE;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapDesc.Flags = 0;

  HRESULT hr = S_OK;

  hr = m_pFactory->CreateSwapChain(m_WrappedDevice->GetQueue(), &swapDesc, &outw.swap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create swap chain for HWND, HRESULT: 0x%08x", hr);
    return 0;
  }

  outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb[0]);
  outw.swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&outw.bb[1]);

  outw.bbIdx = 0;

  outw.rtv = GetCPUHandle(FIRST_WIN_RTV);
  outw.rtv.ptr += SIZE_T(m_OutputWindowID) * sizeof(D3D12Descriptor);

  outw.dsv = GetCPUHandle(FIRST_WIN_DSV);
  outw.dsv.ptr += SIZE_T(m_DSVID) * sizeof(D3D12Descriptor);

  outw.col = NULL;
  outw.MakeRTV(depth);
  m_WrappedDevice->CreateRenderTargetView(outw.col, NULL, outw.rtv);

  outw.depth = NULL;
  if(depth)
  {
    outw.MakeDSV();
    m_DSVID++;
  }

  uint64_t id = m_OutputWindowID++;
  m_OutputWindows[id] = outw;
  return id;
}

void D3D12DebugManager::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  SAFE_RELEASE(outw.swap);
  SAFE_RELEASE(outw.bb[0]);
  SAFE_RELEASE(outw.bb[1]);
  SAFE_RELEASE(outw.col);
  SAFE_RELEASE(outw.depth);

  m_OutputWindows.erase(it);
}

bool D3D12DebugManager::CheckResizeOutputWindow(uint64_t id)
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

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists(true);

    if(outw.width > 0 && outw.height > 0)
    {
      SAFE_RELEASE(outw.bb[0]);
      SAFE_RELEASE(outw.bb[1]);

      DXGI_SWAP_CHAIN_DESC desc;
      outw.swap->GetDesc(&desc);

      HRESULT hr = outw.swap->ResizeBuffers(desc.BufferCount, outw.width, outw.height,
                                            desc.BufferDesc.Format, desc.Flags);

      if(FAILED(hr))
      {
        RDCERR("Failed to resize swap chain, HRESULT: 0x%08x", hr);
        return true;
      }

      outw.swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&outw.bb[0]);
      outw.swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&outw.bb[1]);

      outw.bbIdx = 0;

      if(outw.depth)
      {
        outw.MakeRTV(true);
        outw.MakeDSV();
      }
      else
      {
        outw.MakeRTV(false);
      }
    }

    return true;
  }

  return false;
}

void D3D12DebugManager::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  w = m_OutputWindows[id].width;
  h = m_OutputWindows[id].height;
}

void D3D12DebugManager::ClearOutputWindowColour(uint64_t id, float col[4])
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->ClearRenderTargetView(m_OutputWindows[id].rtv, col, 0, NULL);

  list->Close();
}

void D3D12DebugManager::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->ClearDepthStencilView(m_OutputWindows[id].dsv,
                              D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0,
                              NULL);

  list->Close();
}

void D3D12DebugManager::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  m_CurrentOutputWindow = id;

  if(outw.bb[0] == NULL)
    return;

  SetOutputDimensions(outw.width, outw.height, DXGI_FORMAT_UNKNOWN);
}

bool D3D12DebugManager::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

void D3D12DebugManager::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  if(m_OutputWindows[id].bb[0] == NULL)
    return;

  D3D12_RESOURCE_BARRIER barriers[2];
  RDCEraseEl(barriers);

  barriers[0].Transition.pResource = outw.col;
  barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  barriers[1].Transition.pResource = outw.bb[outw.bbIdx];
  barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  // transition colour to copy source, backbuffer to copy test
  list->ResourceBarrier(2, barriers);

  // resolve or copy from colour to backbuffer
  /*
  if(outw.depth)
    list->ResolveSubresource(barriers[1].Transition.pResource, 0, barriers[0].Transition.pResource,
                             0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
  else
  */
  list->CopyResource(barriers[1].Transition.pResource, barriers[0].Transition.pResource);

  std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
  std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);

  // transition colour back to render target, and backbuffer back to present
  list->ResourceBarrier(2, barriers);

  list->Close();

  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();

  outw.swap->Present(0, 0);

  outw.bbIdx++;
  outw.bbIdx %= 2;
}

void D3D12DebugManager::FillBuffer(ID3D12Resource *buf, const void *data, size_t size)
{
  void *ptr = NULL;
  HRESULT hr = buf->Map(0, NULL, &ptr);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer %08x", hr);
  }
  else
  {
    memcpy(ptr, data, size);
    buf->Unmap(0, NULL);
  }
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetCPUHandle(CBVUAVSRVSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = cbvsrvuavHeap->GetCPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetCPUHandle(RTVSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetCPUHandle(DSVSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = dsvHeap->GetCPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetGPUHandle(CBVUAVSRVSlot slot)
{
  D3D12_GPU_DESCRIPTOR_HANDLE ret = cbvsrvuavHeap->GetGPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetGPUHandle(RTVSlot slot)
{
  D3D12_GPU_DESCRIPTOR_HANDLE ret = rtvHeap->GetGPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetGPUHandle(DSVSlot slot)
{
  D3D12_GPU_DESCRIPTOR_HANDLE ret = dsvHeap->GetGPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::AllocRTV()
{
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtv.ptr += SIZE_T(m_OutputWindowID) *
             m_WrappedDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  m_OutputWindowID++;

  return rtv;
}

void D3D12DebugManager::FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  // do nothing for now but could recycle/free-list/etc RTVs
  D3D12NOTIMP("Not freeing RTV's - will run out");
}

void D3D12DebugManager::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                                  uint32_t mip, uint32_t sample, FormatComponentType typeHint,
                                  float pixel[4])
{
  int oldW = GetWidth(), oldH = GetHeight();

  SetOutputDimensions(1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

  {
    TextureDisplay texDisplay;

    texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
    texDisplay.HDRMul = -1.0f;
    texDisplay.linearDisplayAsGamma = true;
    texDisplay.FlipY = false;
    texDisplay.mip = mip;
    texDisplay.sampleIdx = sample;
    texDisplay.CustomShader = ResourceId();
    texDisplay.sliceFace = sliceFace;
    texDisplay.rangemin = 0.0f;
    texDisplay.rangemax = 1.0f;
    texDisplay.scale = 1.0f;
    texDisplay.texid = texture;
    texDisplay.typeHint = typeHint;
    texDisplay.rawoutput = true;
    texDisplay.offx = -float(x);
    texDisplay.offy = -float(y);

    RenderTextureInternal(m_PickPixelRTV, texDisplay, false);
  }

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = m_PickPixelTex;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  list->ResourceBarrier(1, &barrier);

  D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};

  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.pResource = m_PickPixelTex;
  src.SubresourceIndex = 0;

  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.pResource = m_ReadbackBuffer;
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

  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();

  D3D12_RANGE range = {0, sizeof(Vec4f)};

  float *pix = NULL;
  HRESULT hr = m_ReadbackBuffer->Map(0, &range, (void **)&pix);

  if(FAILED(hr))
  {
    RDCERR("Failed to map picking stage tex %08x", hr);
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

  SetOutputDimensions(oldW, oldH, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

  range.End = 0;

  if(SUCCEEDED(hr))
    m_ReadbackBuffer->Unmap(0, &range);
}

void D3D12DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                                             const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars,
                                             const vector<byte> &data)
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
      var.type = eVar_Float;

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
            vr.type = eVar_Float;

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
    VarType type = eVar_Float;
    switch(invars[v].type.descriptor.type)
    {
      case VARTYPE_INT: type = eVar_Int; break;
      case VARTYPE_FLOAT: type = eVar_Float; break;
      case VARTYPE_BOOL:
      case VARTYPE_UINT:
      case VARTYPE_UINT8: type = eVar_UInt; break;
      case VARTYPE_DOUBLE:
        elemByteSize = 8;
        type = eVar_Double;
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

    if(outvars[outIdx].name.count > 0)
    {
      RDCASSERT(flatten);

      RDCASSERT(outvars[vec].rows == 1);
      RDCASSERT(outvars[vec].columns == comp);
      RDCASSERT(rows == 1);

      string combinedName = outvars[outIdx].name.elems;
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

        vector<ShaderVariable> varmembers;
        vector<ShaderVariable> *out = &outvars;
        size_t rowCopy = 1;

        uint32_t registers = rows;
        uint32_t regLen = cols;
        const char *regName = "row";

        string base = outvars[outIdx].name.elems;

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

void D3D12DebugManager::FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                                             vector<ShaderVariable> &outvars, bool flattenVec4s,
                                             const vector<byte> &data)
{
  size_t zero = 0;

  vector<ShaderVariable> v;
  FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

  outvars.reserve(v.size());
  for(size_t i = 0; i < v.size(); i++)
    outvars.push_back(v[i]);
}

void D3D12DebugManager::BuildShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStageType type, ResourceId *id, string *errors)
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
    case eShaderStage_Vertex: profile = "vs_5_0"; break;
    case eShaderStage_Hull: profile = "hs_5_0"; break;
    case eShaderStage_Domain: profile = "ds_5_0"; break;
    case eShaderStage_Geometry: profile = "gs_5_0"; break;
    case eShaderStage_Pixel: profile = "ps_5_0"; break;
    case eShaderStage_Compute: profile = "cs_5_0"; break;
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

  D3D12_SHADER_BYTECODE byteCode;
  byteCode.BytecodeLength = blob->GetBufferSize();
  byteCode.pShaderBytecode = blob->GetBufferPointer();

  WrappedID3D12PipelineState::ShaderEntry *sh =
      WrappedID3D12PipelineState::AddShader(byteCode, m_WrappedDevice, NULL);

  SAFE_RELEASE(blob);

  *id = sh->GetResourceID();
}

void D3D12DebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length,
                                      vector<byte> &retData)
{
  auto it = WrappedID3D12Resource::GetList().find(buff);

  if(it == WrappedID3D12Resource::GetList().end())
  {
    RDCERR("Getting buffer data for unknown buffer %llu!", buff);
    return;
  }

  WrappedID3D12Resource *buffer = it->second;

  RDCASSERT(buffer);

  GetBufferData(buffer, offset, length, retData);
}

void D3D12DebugManager::GetBufferData(ID3D12Resource *buffer, uint64_t offset, uint64_t length,
                                      vector<byte> &ret)
{
  if(buffer == NULL)
    return;

  D3D12_RESOURCE_DESC desc = buffer->GetDesc();
  D3D12_HEAP_PROPERTIES heapProps;
  buffer->GetHeapProperties(&heapProps, NULL);

  if(offset >= desc.Width)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(length == 0)
  {
    length = desc.Width - offset;
  }

  if(length > 0 && offset + length > desc.Width)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%llu)",
            offset, length, desc.Width);
    length = RDCMIN(length, desc.Width - offset);
  }

#if DISABLED(RDOC_X64)
  if(offset + length > 0xfffffff)
  {
    RDCERR("Trying to read back too much data on 32-bit build. Try running on 64-bit.");
    return;
  }
#endif

  uint64_t outOffs = 0;

  ret.resize((size_t)length);

  // directly CPU mappable (and possibly invalid to transition and copy from), so just memcpy
  if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD || heapProps.Type == D3D12_HEAP_TYPE_READBACK)
  {
    D3D12_RANGE range = {(size_t)offset, size_t(offset + length)};

    byte *data = NULL;
    HRESULT hr = buffer->Map(0, &range, (void **)&data);

    if(FAILED(hr))
    {
      RDCERR("Failed to map buffer directly for readback %08x", hr);
      return;
    }

    memcpy(&ret[0], data + offset, (size_t)length);

    range.Begin = range.End = 0;

    buffer->Unmap(0, &range);

    return;
  }

  m_ReadbackList->Reset(m_ReadbackAlloc, NULL);

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = buffer;
  barrier.Transition.StateBefore = m_WrappedDevice->GetSubresourceStates(GetResID(buffer))[0];
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  m_ReadbackList->ResourceBarrier(1, &barrier);

  while(length > 0)
  {
    uint64_t chunkSize = RDCMIN(length, m_ReadbackSize);

    m_ReadbackList->CopyBufferRegion(m_ReadbackBuffer, 0, buffer, offset, chunkSize);

    m_ReadbackList->Close();

    ID3D12CommandList *l = m_ReadbackList;
    m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_WrappedDevice->GPUSync();
    m_ReadbackAlloc->Reset();

    D3D12_RANGE range = {0, (size_t)chunkSize};

    void *data = NULL;
    HRESULT hr = m_ReadbackBuffer->Map(0, &range, &data);

    if(FAILED(hr))
    {
      RDCERR("Failed to map bufferdata buffer %08x", hr);
      return;
    }
    else
    {
      memcpy(&ret[(size_t)outOffs], data, (size_t)chunkSize);

      range.End = 0;

      m_ReadbackBuffer->Unmap(0, &range);
    }

    outOffs += chunkSize;
    length -= chunkSize;
  }
}

void D3D12DebugManager::RenderHighlightBox(float w, float h, float scale)
{
  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

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

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }
}

void D3D12DebugManager::RenderCheckerboard(Vec3f light, Vec3f dark)
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

  pixelData.Channels = Vec4f(light.x, light.y, light.z, 0.0f);
  pixelData.WireframeColour = dark;

  FillBuffer(m_GenericVSCbuffer, &vertexData, sizeof(DebugVertexCBuffer));
  FillBuffer(m_GenericPSCbuffer, &pixelData, sizeof(DebugPixelCBufferData));

  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    list->OMSetRenderTargets(1, &outw.rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, outw.width, outw.height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    list->SetPipelineState(m_CheckerboardPipe);

    list->SetGraphicsRootSignature(m_CBOnlyRootSig);

    list->SetGraphicsRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, m_GenericPSCbuffer->GetGPUVirtualAddress());

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(4, 1, 0, 0);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }
}

void D3D12DebugManager::RenderText(ID3D12GraphicsCommandList *list, float x, float y,
                                   const char *textfmt, ...)
{
  static char tmpBuf[4096];

  va_list args;
  va_start(args, textfmt);
  StringFormat::vsnprintf(tmpBuf, 4095, textfmt, args);
  tmpBuf[4095] = '\0';
  va_end(args);

  RenderTextInternal(list, x, y, tmpBuf);
}

void D3D12DebugManager::RenderTextInternal(ID3D12GraphicsCommandList *list, float x, float y,
                                           const char *text)
{
  if(char *t = strchr((char *)text, '\n'))
  {
    *t = 0;
    RenderTextInternal(list, x, y, text);
    RenderTextInternal(list, x, y + 1.0f, t + 1);
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

  data.CharacterSize.x = 1.0f / float(FONT_TEX_WIDTH);
  data.CharacterSize.y = 1.0f / float(FONT_TEX_HEIGHT);

  FillBuffer(m_Font.Constants[m_Font.ConstRingIdx], &data, sizeof(FontCBuffer));

  size_t chars = strlen(text);

  size_t charOffset = m_Font.CharOffset;

  if(m_Font.CharOffset + chars >= FONT_BUFFER_CHARS)
    charOffset = 0;

  m_Font.CharOffset = charOffset + chars;

  // Is 256 byte alignment on buffer offsets is just fixed, or device-specific?
  m_Font.CharOffset = AlignUp(m_Font.CharOffset, 256 / sizeof(Vec4f));

  unsigned long *texs = NULL;
  HRESULT hr = m_Font.CharBuffer->Map(0, NULL, (void **)&texs);

  if(FAILED(hr) || texs == NULL)
  {
    RDCERR("Failed to map charbuffer %08x", hr);
    return;
  }

  texs += charOffset * 4;

  for(size_t i = 0; i < strlen(text); i++)
    texs[i * 4] = (text[i] - ' ');

  m_Font.CharBuffer->Unmap(0, NULL);

  {
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    D3D12_VIEWPORT view;
    view.TopLeftX = 0;
    view.TopLeftY = 0;
    view.Width = (float)GetWidth();
    view.Height = (float)GetHeight();
    view.MinDepth = 0.0f;
    view.MaxDepth = 1.0f;
    list->RSSetViewports(1, &view);

    D3D12_RECT scissor = {0, 0, GetWidth(), GetHeight()};
    list->RSSetScissorRects(1, &scissor);

    list->SetPipelineState(m_Font.Pipe[m_BBFmtIdx]);
    list->SetGraphicsRootSignature(m_Font.RootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvuavHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    list->SetGraphicsRootConstantBufferView(
        0, m_Font.Constants[m_Font.ConstRingIdx]->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, m_Font.GlyphData->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(
        2, m_Font.CharBuffer->GetGPUVirtualAddress() + charOffset * sizeof(Vec4f));
    list->SetGraphicsRootDescriptorTable(3, cbvsrvuavHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(4, samplerHeap->GetGPUDescriptorHandleForHeapStart());

    list->DrawInstanced(4, (uint32_t)strlen(text), 0, 0);
  }

  m_Font.ConstRingIdx++;
  m_Font.ConstRingIdx %= ARRAY_COUNT(m_Font.Constants);
}

bool D3D12DebugManager::RenderTexture(TextureDisplay cfg, bool blendAlpha)
{
  return RenderTextureInternal(m_OutputWindows[m_CurrentOutputWindow].rtv, cfg, blendAlpha);
}

void D3D12DebugManager::PrepareTextureSampling(ID3D12Resource *resource,
                                               FormatComponentType typeHint, int &resType,
                                               vector<D3D12_RESOURCE_BARRIER> &barriers)
{
  int srvOffset = 0;

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = GetTypedFormat(resourceDesc.Format, typeHint);
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    srvOffset = RESTYPE_TEX3D;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = ~0U;
  }
  else if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
  {
    if(resourceDesc.SampleDesc.Count > 1)
    {
      srvOffset = RESTYPE_TEX2D_MS;
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
      srvDesc.Texture2DMSArray.ArraySize = ~0U;

      if(IsDepthFormat(resourceDesc.Format))
        srvOffset = RESTYPE_DEPTH_MS;
    }
    else
    {
      srvOffset = RESTYPE_TEX2D;
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2D.MipLevels = ~0U;
      srvDesc.Texture2DArray.ArraySize = ~0U;

      if(IsDepthFormat(resourceDesc.Format))
        srvOffset = RESTYPE_DEPTH;
    }
  }
  else if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    srvOffset = RESTYPE_TEX1D;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    srvDesc.Texture1DArray.MipLevels = ~0U;
    srvDesc.Texture1DArray.ArraySize = ~0U;
  }

  resType = srvOffset;

  // if it's a depth and stencil image, increment (as the restype for
  // depth/stencil is one higher than that for depth only).
  if(IsDepthAndStencilFormat(resourceDesc.Format))
    resType++;

  if(IsUIntFormat(resourceDesc.Format))
    srvOffset += 10;
  if(IsIntFormat(resourceDesc.Format))
    srvOffset += 20;

  D3D12_RESOURCE_STATES realResourceState =
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  bool copy = false;

  D3D12_SHADER_RESOURCE_VIEW_DESC stencilSRVDesc = {};

  // for non-typeless depth formats, we need to copy to a typeless resource for read
  if(IsDepthFormat(resourceDesc.Format) &&
     GetTypelessFormat(resourceDesc.Format) != resourceDesc.Format)
  {
    realResourceState = D3D12_RESOURCE_STATE_COPY_SOURCE;
    copy = true;

    switch(GetTypelessFormat(srvDesc.Format))
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilSRVDesc = srvDesc;
        stencilSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      case DXGI_FORMAT_R24G8_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilSRVDesc = srvDesc;
        stencilSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
      case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
      case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_UNORM; break;
      default:
        RDCERR("Unexpected typeless format %d from depth format %d",
               GetTypelessFormat(srvDesc.Format), srvDesc.Format);
        break;
    }
  }

  // even for non-copies, we need to make two SRVs to sample stencil as well
  if(IsDepthAndStencilFormat(resourceDesc.Format) && stencilSRVDesc.Format == DXGI_FORMAT_UNKNOWN)
  {
    switch(GetTypelessFormat(srvDesc.Format))
    {
      case DXGI_FORMAT_R32G8X24_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        stencilSRVDesc = srvDesc;
        stencilSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        break;
      case DXGI_FORMAT_R24G8_TYPELESS:
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        stencilSRVDesc = srvDesc;
        stencilSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        break;
    }
  }

  if(stencilSRVDesc.Format != DXGI_FORMAT_UNKNOWN)
  {
    D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
    formatInfo.Format = srvDesc.Format;
    m_WrappedDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

    if(formatInfo.PlaneCount > 1 && stencilSRVDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      stencilSRVDesc.Texture2DArray.PlaneSlice = 1;
  }

  // transition resource to D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  const vector<D3D12_RESOURCE_STATES> &states =
      m_WrappedDevice->GetSubresourceStates(GetResID(resource));

  barriers.reserve(states.size());
  for(size_t i = 0; i < states.size(); i++)
  {
    D3D12_RESOURCE_BARRIER b;

    // skip unneeded barriers
    if(states[i] & realResourceState)
      continue;

    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = resource;
    b.Transition.Subresource = (UINT)i;
    b.Transition.StateBefore = states[i];
    b.Transition.StateAfter = realResourceState;

    barriers.push_back(b);
  }

  if(copy)
  {
    D3D12_RESOURCE_DESC resDesc = resource->GetDesc();

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Alignment = 0;
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Format = GetTypelessFormat(resDesc.Format);
    texDesc.Width = resDesc.Width;
    texDesc.Height = resDesc.Height;
    texDesc.DepthOrArraySize = resDesc.DepthOrArraySize;
    texDesc.MipLevels = resDesc.MipLevels;
    texDesc.SampleDesc.Count = resDesc.SampleDesc.Count;
    texDesc.SampleDesc.Quality = 0;

    if(texDesc.SampleDesc.Count > 1)
      texDesc.Flags |= IsDepthFormat(texDesc.Format) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                                                     : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // check if the existing resource is similar enough (same typeless format and dimension)
    if(m_TexResource)
    {
      D3D12_RESOURCE_DESC oldDesc = m_TexResource->GetDesc();

      if(oldDesc.Width != texDesc.Width || oldDesc.Height != texDesc.Height ||
         oldDesc.DepthOrArraySize != texDesc.DepthOrArraySize || oldDesc.Format != texDesc.Format ||
         oldDesc.MipLevels != texDesc.MipLevels ||
         oldDesc.SampleDesc.Count != texDesc.SampleDesc.Count)
      {
        SAFE_RELEASE(m_TexResource);
      }
    }

    // create resource if we need it
    if(!m_TexResource)
    {
      HRESULT hr = m_WrappedDevice->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
          NULL, __uuidof(ID3D12Resource), (void **)&m_TexResource);
      RDCASSERTEQUAL(hr, S_OK);
    }

    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    // prepare real resource for copying
    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    D3D12_RESOURCE_BARRIER texResourceBarrier;
    D3D12_RESOURCE_BARRIER &b = texResourceBarrier;

    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource = m_TexResource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    // prepare tex resource for copying
    list->ResourceBarrier(1, &texResourceBarrier);

    list->CopyResource(m_TexResource, resource);

    // tex resource back to readable
    std::swap(texResourceBarrier.Transition.StateBefore, texResourceBarrier.Transition.StateAfter);
    list->ResourceBarrier(1, &texResourceBarrier);

    // real resource back to itself
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    // don't do any barriers outside in the source function
    barriers.clear();

    list->Close();

    resource = m_TexResource;
  }

  // empty all the other SRVs just to mute debug warnings
  D3D12_CPU_DESCRIPTOR_HANDLE srv = GetCPUHandle(FIRST_TEXDISPLAY_SRV);

  D3D12_SHADER_RESOURCE_VIEW_DESC emptyDesc = {};
  emptyDesc.Format = DXGI_FORMAT_R8_UNORM;
  emptyDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  emptyDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  emptyDesc.Texture2D.MipLevels = 1;

  for(size_t i = 0; i < 32; i++)
  {
    m_WrappedDevice->CreateShaderResourceView(NULL, &emptyDesc, srv);
    srv.ptr += sizeof(D3D12Descriptor);
  }

  srv = GetCPUHandle(FIRST_TEXDISPLAY_SRV);
  srv.ptr += srvOffset * sizeof(D3D12Descriptor);

  m_WrappedDevice->CreateShaderResourceView(resource, &srvDesc, srv);
  if(stencilSRVDesc.Format != DXGI_FORMAT_UNKNOWN)
  {
    srv.ptr += sizeof(D3D12Descriptor);
    m_WrappedDevice->CreateShaderResourceView(resource, &stencilSRVDesc, srv);
  }
}

bool D3D12DebugManager::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                                  FormatComponentType typeHint, float *minval, float *maxval)
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

  FillBuffer(m_GenericVSCbuffer, &cdata, sizeof(cdata));

  int blocksX = (int)ceil(cdata.HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata.HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  PrepareTextureSampling(resource, typeHint, resType, barriers);

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->SetPipelineState(m_TileMinMaxPipe[resType][intIdx]);

    list->SetComputeRootSignature(m_HistogramRootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvuavHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE uav = GetGPUHandle(MINMAX_TILE_UAVS);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = GetGPUHandle(FIRST_TEXDISPLAY_SRV);

    list->SetComputeRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
    list->SetComputeRootDescriptorTable(1, srv);
    list->SetComputeRootDescriptorTable(2, samplerHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetComputeRootDescriptorTable(3, uav);

    // discard the whole resource as we will overwrite it
    D3D12_DISCARD_REGION region = {};
    region.NumSubresources = 1;
    list->DiscardResource(m_MinMaxTileBuffer, &region);

    list->Dispatch(blocksX, blocksY, 1);

    D3D12_RESOURCE_BARRIER tileBarriers[2] = {};

    // ensure UAV work is done. Transition to SRV
    tileBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    tileBarriers[0].UAV.pResource = m_MinMaxTileBuffer;
    tileBarriers[1].Transition.pResource = m_MinMaxTileBuffer;
    tileBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    tileBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    list->ResourceBarrier(2, tileBarriers);

    // set up second dispatch
    srv = GetGPUHandle(MINMAX_TILE_SRVS);
    uav = GetGPUHandle(MINMAX_RESULT_UAVS);

    list->SetComputeRootDescriptorTable(1, srv);
    list->SetComputeRootDescriptorTable(3, uav);

    list->SetPipelineState(m_ResultMinMaxPipe[intIdx]);

    list->Dispatch(1, 1, 1);

    // transition back to UAV for next time
    std::swap(tileBarriers[1].Transition.StateBefore, tileBarriers[1].Transition.StateAfter);

    list->ResourceBarrier(1, &tileBarriers[1]);

    // finish the UAV work, and transition to copy.
    tileBarriers[0].UAV.pResource = m_MinMaxResultBuffer;
    tileBarriers[1].Transition.pResource = m_MinMaxResultBuffer;
    tileBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    tileBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(2, tileBarriers);

    // copy to readback
    list->CopyBufferRegion(m_ReadbackBuffer, 0, m_MinMaxResultBuffer, 0, sizeof(Vec4f) * 2);

    // transition back to UAV for next time
    std::swap(tileBarriers[1].Transition.StateBefore, tileBarriers[1].Transition.StateAfter);

    list->ResourceBarrier(1, &tileBarriers[1]);

    // transition image back to where it was
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }

  D3D12_RANGE range = {0, sizeof(Vec4f) * 2};

  void *data = NULL;
  HRESULT hr = m_ReadbackBuffer->Map(0, &range, &data);

  if(FAILED(hr))
  {
    RDCERR("Failed to map bufferdata buffer %08x", hr);
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

    m_ReadbackBuffer->Unmap(0, &range);
  }

  return true;
}

bool D3D12DebugManager::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip,
                                     uint32_t sample, FormatComponentType typeHint, float minval,
                                     float maxval, bool channels[4], vector<uint32_t> &histogram)
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

  FillBuffer(m_GenericVSCbuffer, &cdata, sizeof(cdata));

  int tilesX = (int)ceil(cdata.HistogramTextureResolution.x /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int tilesY = (int)ceil(cdata.HistogramTextureResolution.y /
                         float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  PrepareTextureSampling(resource, typeHint, resType, barriers);

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->SetPipelineState(m_HistogramPipe[resType][intIdx]);

    list->SetComputeRootSignature(m_HistogramRootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvuavHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE uav = GetGPUHandle(HISTOGRAM_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = GetGPUHandle(FIRST_TEXDISPLAY_SRV);
    D3D12_CPU_DESCRIPTOR_HANDLE uavcpu = GetCPUHandle(HISTOGRAM_UAV);

    UINT zeroes[] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(uav, uavcpu, m_MinMaxTileBuffer, zeroes, 0, NULL);

    list->SetComputeRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
    list->SetComputeRootDescriptorTable(1, srv);
    list->SetComputeRootDescriptorTable(2, samplerHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetComputeRootDescriptorTable(3, uav);

    list->Dispatch(tilesX, tilesY, 1);

    D3D12_RESOURCE_BARRIER tileBarriers[2] = {};

    // finish the UAV work, and transition to copy.
    tileBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    tileBarriers[0].UAV.pResource = m_MinMaxTileBuffer;
    tileBarriers[1].Transition.pResource = m_MinMaxTileBuffer;
    tileBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    tileBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(2, tileBarriers);

    // copy to readback
    list->CopyBufferRegion(m_ReadbackBuffer, 0, m_MinMaxTileBuffer, 0,
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

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }

  D3D12_RANGE range = {0, sizeof(uint32_t) * HGRAM_NUM_BUCKETS};

  void *data = NULL;
  HRESULT hr = m_ReadbackBuffer->Map(0, &range, &data);

  histogram.clear();
  histogram.resize(HGRAM_NUM_BUCKETS);

  if(FAILED(hr))
  {
    RDCERR("Failed to map bufferdata buffer %08x", hr);
    return false;
  }
  else
  {
    memcpy(&histogram[0], data, sizeof(uint32_t) * HGRAM_NUM_BUCKETS);

    range.End = 0;

    m_ReadbackBuffer->Unmap(0, &range);
  }

  return true;
}

ResourceId D3D12DebugManager::RenderOverlay(ResourceId texid, FormatComponentType typeHint,
                                            TextureDisplayOverlay overlay, uint32_t eventID,
                                            const vector<uint32_t> &passEvents)
{
  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[texid];

  if(resource == NULL)
    return ResourceId();

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  PrepareTextureSampling(resource, typeHint, resType, barriers);

  D3D12_RESOURCE_DESC overlayTexDesc;
  overlayTexDesc.Alignment = 0;
  overlayTexDesc.DepthOrArraySize = 1;
  overlayTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  overlayTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  overlayTexDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
  overlayTexDesc.Height = resourceDesc.Height;
  overlayTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  overlayTexDesc.MipLevels = 1;
  overlayTexDesc.SampleDesc = resourceDesc.SampleDesc;
  overlayTexDesc.Width = resourceDesc.Width;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC currentOverlayDesc;
  RDCEraseEl(currentOverlayDesc);
  if(m_OverlayRenderTex)
    currentOverlayDesc = m_OverlayRenderTex->GetDesc();

  WrappedID3D12Resource *wrappedCustomRenderTex = (WrappedID3D12Resource *)m_OverlayRenderTex;

  // need to recreate backing custom render tex
  if(overlayTexDesc.Width != currentOverlayDesc.Width ||
     overlayTexDesc.Height != currentOverlayDesc.Height ||
     overlayTexDesc.Format != currentOverlayDesc.Format ||
     overlayTexDesc.SampleDesc.Count != currentOverlayDesc.SampleDesc.Count ||
     overlayTexDesc.SampleDesc.Quality != currentOverlayDesc.SampleDesc.Quality)
  {
    SAFE_RELEASE(m_OverlayRenderTex);
    m_OverlayResourceId = ResourceId();

    ID3D12Resource *customRenderTex = NULL;
    HRESULT hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &overlayTexDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&customRenderTex);
    if(FAILED(hr))
    {
      RDCERR("Failed to create custom render tex %08x", hr);
      return ResourceId();
    }
    wrappedCustomRenderTex = (WrappedID3D12Resource *)customRenderTex;

    m_OverlayRenderTex = wrappedCustomRenderTex;
    m_OverlayResourceId = wrappedCustomRenderTex->GetResourceID();
  }

  D3D12CommandData *cmd = m_WrappedDevice->GetQueue()->GetCommandData();
  D3D12RenderState &rs = cmd->m_RenderState;

  ID3D12Resource *renderDepth = NULL;

  D3D12Descriptor *dsView =
      DescriptorFromPortableHandle(m_WrappedDevice->GetResourceManager(), rs.dsv);

  D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc;
  RDCEraseEl(dsViewDesc);
  if(dsView)
  {
    ID3D12Resource *realDepth = dsView->nonsamp.resource;

    dsViewDesc = dsView->nonsamp.dsv;

    D3D12_RESOURCE_DESC desc = realDepth->GetDesc();
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.Alignment = 0;

    HRESULT hr = S_OK;

    hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&renderDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth %08x", hr);
      return m_OverlayResourceId;
    }

    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    const vector<D3D12_RESOURCE_STATES> &states =
        m_WrappedDevice->GetSubresourceStates(GetResID(realDepth));

    vector<D3D12_RESOURCE_BARRIER> depthBarriers;
    depthBarriers.reserve(states.size());
    for(size_t i = 0; i < states.size(); i++)
    {
      D3D12_RESOURCE_BARRIER b;

      // skip unneeded barriers
      if(states[i] & D3D12_RESOURCE_STATE_COPY_SOURCE)
        continue;

      b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      b.Transition.pResource = realDepth;
      b.Transition.Subresource = (UINT)i;
      b.Transition.StateBefore = states[i];
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

      depthBarriers.push_back(b);
    }

    if(!depthBarriers.empty())
      list->ResourceBarrier((UINT)depthBarriers.size(), &depthBarriers[0]);

    list->CopyResource(renderDepth, realDepth);

    for(size_t i = 0; i < depthBarriers.size(); i++)
      std::swap(depthBarriers[i].Transition.StateBefore, depthBarriers[i].Transition.StateAfter);

    if(!depthBarriers.empty())
      list->ResourceBarrier((UINT)depthBarriers.size(), &depthBarriers[0]);

    D3D12_RESOURCE_BARRIER b = {};

    b.Transition.pResource = renderDepth;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    // prepare tex resource for copying
    list->ResourceBarrier(1, &b);

    list->Close();
  }

  D3D12_RENDER_TARGET_VIEW_DESC rtDesc = {};
  rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
  rtDesc.Texture2D.MipSlice = 0;
  rtDesc.Texture2D.PlaneSlice = 0;

  if(overlayTexDesc.SampleDesc.Count > 1 || overlayTexDesc.SampleDesc.Quality > 0)
    rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCPUHandle(OVERLAY_RTV);

  m_WrappedDevice->CreateRenderTargetView(wrappedCustomRenderTex, &rtDesc, rtv);

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  FLOAT black[] = {0.0f, 0.0f, 0.0f, 0.0f};
  list->ClearRenderTargetView(rtv, black, 0, NULL);

  D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};

  if(renderDepth)
  {
    dsv = GetCPUHandle(OVERLAY_DSV);
    m_WrappedDevice->CreateDepthStencilView(
        renderDepth, dsViewDesc.Format == DXGI_FORMAT_UNKNOWN ? NULL : &dsViewDesc, dsv);
  }

  D3D12_DEPTH_STENCIL_DESC dsDesc;

  dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp =
      dsDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
  dsDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp =
      dsDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
  dsDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  dsDesc.DepthEnable = TRUE;
  dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  dsDesc.StencilEnable = FALSE;
  dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

  WrappedID3D12PipelineState *pipe = NULL;

  if(rs.pipe != ResourceId())
    pipe = m_WrappedDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(overlay == eTexOverlay_NaN || overlay == eTexOverlay_Clipping)
  {
    // just need the basic texture
  }
  else if(overlay == eTexOverlay_Drawcall)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = pipe->GetGraphicsDesc();

      float overlayConsts[4] = {0.8f, 0.1f, 0.8f, 1.0f};
      ID3DBlob *ps = MakeFixedColShader(overlayConsts);

      psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
      psoDesc.PS.BytecodeLength = ps->GetBufferSize();

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      float clearColour[] = {0.0f, 0.0f, 0.0f, 0.5f};
      list->ClearRenderTargetView(rtv, clearColour, 0, NULL);

      list->Close();
      list = NULL;

      ID3D12PipelineState *pso = NULL;
      HRESULT hr = m_WrappedDevice->CreateGraphicsPipelineState(
          &psoDesc, __uuidof(ID3D12PipelineState), (void **)&pso);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso %08x", hr);
        SAFE_RELEASE(ps);
        return m_OverlayResourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(pso);
      rs.rtSingle = true;
      rs.rts.resize(1);
      rs.rts[0] = ToPortableHandle(rtv);
      rs.dsv = PortableHandle();

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      rs = prev;

      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();

      SAFE_RELEASE(pso);
      SAFE_RELEASE(ps);
    }
  }
  else if(overlay == eTexOverlay_BackfaceCull)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = pipe->GetGraphicsDesc();

      D3D12_CULL_MODE origCull = psoDesc.RasterizerState.CullMode;

      float redCol[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      ID3DBlob *red = MakeFixedColShader(redCol);

      float greenCol[4] = {0.0f, 1.0f, 0.0f, 1.0f};
      ID3DBlob *green = MakeFixedColShader(greenCol);

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      list->Close();
      list = NULL;

      ID3D12PipelineState *redPSO = NULL;
      HRESULT hr = m_WrappedDevice->CreateGraphicsPipelineState(
          &psoDesc, __uuidof(ID3D12PipelineState), (void **)&redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso %08x", hr);
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_OverlayResourceId;
      }

      psoDesc.RasterizerState.CullMode = origCull;
      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      ID3D12PipelineState *greenPSO = NULL;
      hr = m_WrappedDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState),
                                                        (void **)&greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso %08x", hr);
        SAFE_RELEASE(red);
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(green);
        return m_OverlayResourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(redPSO);
      rs.rtSingle = true;
      rs.rts.resize(1);
      rs.rts[0] = ToPortableHandle(rtv);
      rs.dsv = PortableHandle();

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      rs.pipe = GetResID(greenPSO);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      rs = prev;

      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(green);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(greenPSO);
    }
  }
  else if(overlay == eTexOverlay_Wireframe)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = pipe->GetGraphicsDesc();

      float overlayConsts[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 1.0f};
      ID3DBlob *ps = MakeFixedColShader(overlayConsts);

      psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
      psoDesc.PS.BytecodeLength = ps->GetBufferSize();

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      overlayConsts[3] = 0.0f;
      list->ClearRenderTargetView(rtv, overlayConsts, 0, NULL);

      list->Close();
      list = NULL;

      ID3D12PipelineState *pso = NULL;
      HRESULT hr = m_WrappedDevice->CreateGraphicsPipelineState(
          &psoDesc, __uuidof(ID3D12PipelineState), (void **)&pso);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso %08x", hr);
        SAFE_RELEASE(ps);
        return m_OverlayResourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(pso);
      rs.rtSingle = true;
      rs.rts.resize(1);
      rs.rts[0] = ToPortableHandle(rtv);
      rs.dsv = ToPortableHandle(dsv);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      rs = prev;

      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();

      SAFE_RELEASE(pso);
      SAFE_RELEASE(ps);
    }
  }
  else if(overlay == eTexOverlay_ClearBeforePass || overlay == eTexOverlay_ClearBeforeDraw)
  {
    vector<uint32_t> events = passEvents;

    if(overlay == eTexOverlay_ClearBeforeDraw)
      events.clear();

    events.push_back(eventID);

    if(!events.empty())
    {
      list->Close();
      list = NULL;

      bool rtSingle = rs.rtSingle;
      vector<PortableHandle> rts = rs.rts;

      if(overlay == eTexOverlay_ClearBeforePass)
        m_WrappedDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      list = m_WrappedDevice->GetNewList();

      for(size_t i = 0; i < rts.size(); i++)
      {
        PortableHandle ph = rtSingle ? rts[0] : rts[i];

        WrappedID3D12DescriptorHeap *heap =
            m_WrappedDevice->GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(ph.heap);

        if(heap)
        {
          D3D12_CPU_DESCRIPTOR_HANDLE clearrtv = heap->GetCPUDescriptorHandleForHeapStart();
          clearrtv.ptr += ph.index * sizeof(D3D12Descriptor);

          if(rtSingle)
            clearrtv.ptr += i * sizeof(D3D12Descriptor);

          list->ClearRenderTargetView(clearrtv, black, 0, NULL);
        }
      }

      list->Close();
      list = NULL;

      for(size_t i = 0; i < events.size(); i++)
      {
        m_WrappedDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == eTexOverlay_ClearBeforePass && i + 1 < events.size())
          m_WrappedDevice->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == eTexOverlay_ViewportScissor)
  {
    if(pipe && pipe->IsGraphics() && !rs.views.empty())
    {
      list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

      D3D12_VIEWPORT viewport = rs.views[0];
      list->RSSetViewports(1, &viewport);

      D3D12_RECT scissor = {0, 0, 16384, 16384};
      list->RSSetScissorRects(1, &scissor);

      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      list->SetPipelineState(m_OutlinePipe);

      list->SetGraphicsRootSignature(m_CBOnlyRootSig);

      DebugPixelCBufferData pixelData = {0};

      // border colour (dark, 2px, opaque)
      pixelData.WireframeColour = Vec3f(0.1f, 0.1f, 0.1f);
      // inner colour (light, transparent)
      pixelData.Channels = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
      pixelData.OutputDisplayFormat = 0;
      pixelData.RangeMinimum = viewport.TopLeftX;
      pixelData.InverseRangeSize = viewport.TopLeftY;
      pixelData.TextureResolutionPS = Vec3f(viewport.Width, viewport.Height, 0.0f);

      FillBuffer(m_GenericVSCbuffer, &pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
      list->SetGraphicsRootConstantBufferView(1, m_GenericVSCbuffer->GetGPUVirtualAddress());

      float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      list->OMSetBlendFactor(factor);

      list->DrawInstanced(3, 1, 0, 0);

      viewport.TopLeftX = (float)rs.scissors[0].left;
      viewport.TopLeftY = (float)rs.scissors[0].top;
      viewport.Width = (float)(rs.scissors[0].right - rs.scissors[0].left);
      viewport.Height = (float)(rs.scissors[0].bottom - rs.scissors[0].top);
      list->RSSetViewports(1, &viewport);

      pixelData.OutputDisplayFormat = 1;
      pixelData.RangeMinimum = viewport.TopLeftX;
      pixelData.InverseRangeSize = viewport.TopLeftY;
      pixelData.TextureResolutionPS = Vec3f(viewport.Width, viewport.Height, 0.0f);

      FillBuffer(m_GenericPSCbuffer, &pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(1, m_GenericPSCbuffer->GetGPUVirtualAddress());

      list->DrawInstanced(3, 1, 0, 0);
    }
  }
  else if(overlay == eTexOverlay_TriangleSizeDraw || overlay == eTexOverlay_TriangleSizePass)
  {
  }
  else if(overlay == eTexOverlay_QuadOverdrawPass || overlay == eTexOverlay_QuadOverdrawDraw)
  {
  }
  else if(overlay == eTexOverlay_Depth || overlay == eTexOverlay_Stencil)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = pipe->GetGraphicsDesc();

      float redCol[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      ID3DBlob *red = MakeFixedColShader(redCol);

      float greenCol[4] = {0.0f, 1.0f, 0.0f, 1.0f};
      ID3DBlob *green = MakeFixedColShader(greenCol);

      // make sure that if a test is disabled, it shows all
      // pixels passing
      if(!psoDesc.DepthStencilState.DepthEnable)
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      if(!psoDesc.DepthStencilState.StencilEnable)
      {
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      }

      if(overlay == eTexOverlay_Depth)
      {
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      }
      else
      {
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      }

      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.MultisampleEnable = FALSE;
      psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;

      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      ID3D12PipelineState *greenPSO = NULL;
      HRESULT hr = m_WrappedDevice->CreateGraphicsPipelineState(
          &psoDesc, __uuidof(ID3D12PipelineState), (void **)&greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso %08x", hr);
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_OverlayResourceId;
      }

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      ID3D12PipelineState *redPSO = NULL;
      hr = m_WrappedDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState),
                                                        (void **)&redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso %08x", hr);
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_OverlayResourceId;
      }

      list->Close();
      list = NULL;

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(redPSO);
      rs.rtSingle = true;
      rs.rts.resize(1);
      rs.rts[0] = ToPortableHandle(rtv);
      rs.dsv = ToPortableHandle(dsv);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      rs.pipe = GetResID(greenPSO);

      m_WrappedDevice->ReplayLog(0, eventID, eReplay_OnlyDraw);

      rs = prev;

      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(green);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(greenPSO);
    }
  }
  else
  {
    RDCERR("Unhandled overlay case!");
  }

  if(list)
    list->Close();

  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();

  SAFE_RELEASE(renderDepth);

  return m_OverlayResourceId;
}

bool D3D12DebugManager::RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                                              bool blendAlpha)
{
  DebugVertexCBuffer vertexData;
  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  float x = cfg.offx;
  float y = cfg.offy;

  vertexData.Position.x = x * (2.0f / float(GetWidth()));
  vertexData.Position.y = -y * (2.0f / float(GetHeight()));

  vertexData.ScreenAspect.x = float(GetHeight()) / float(GetWidth());
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

  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[cfg.texid];
  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  pixelData.SampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, resourceDesc.SampleDesc.Count - 1);

  // hacky resolve
  if(cfg.sampleIdx == ~0U)
    pixelData.SampleIdx = -int(resourceDesc.SampleDesc.Count);

  if(resourceDesc.Format == DXGI_FORMAT_UNKNOWN)
    return false;

  if(resourceDesc.Format == DXGI_FORMAT_A8_UNORM && cfg.scale <= 0.0f)
  {
    pixelData.Channels.x = pixelData.Channels.y = pixelData.Channels.z = 0.0f;
    pixelData.Channels.w = 1.0f;
  }

  float tex_x = float(resourceDesc.Width);
  float tex_y =
      float(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? 100 : resourceDesc.Height);

  vertexData.TextureResolution.x *= tex_x / float(GetWidth());
  vertexData.TextureResolution.y *= tex_y / float(GetHeight());

  pixelData.TextureResolutionPS.x = float(RDCMAX(1U, uint32_t(resourceDesc.Width >> cfg.mip)));
  pixelData.TextureResolutionPS.y = float(RDCMAX(1U, uint32_t(resourceDesc.Height >> cfg.mip)));
  pixelData.TextureResolutionPS.z =
      float(RDCMAX(1U, uint32_t(resourceDesc.DepthOrArraySize >> cfg.mip)));

  if(resourceDesc.DepthOrArraySize > 1 && resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    pixelData.TextureResolutionPS.z = float(resourceDesc.DepthOrArraySize);

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    pixelData.Slice = float(cfg.sliceFace) / float(resourceDesc.DepthOrArraySize);

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

  vertexData.Scale *= 2.0f;    // viewport is -1 -> 1

  pixelData.MipLevel = (float)cfg.mip;
  pixelData.OutputDisplayFormat = RESTYPE_TEX2D;
  pixelData.Slice = float(RDCCLAMP(cfg.sliceFace, 0U, uint32_t(resourceDesc.DepthOrArraySize - 1)));

  vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  PrepareTextureSampling(resource, cfg.typeHint, resType, barriers);

  pixelData.OutputDisplayFormat = resType;

  if(cfg.overlay == eTexOverlay_NaN)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;

  if(cfg.overlay == eTexOverlay_Clipping)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;

  if(IsUIntFormat(resourceDesc.Format))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
  else if(IsIntFormat(resourceDesc.Format))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;

  if(!IsSRGBFormat(resourceDesc.Format) && cfg.linearDisplayAsGamma)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

  FillBuffer(m_GenericVSCbuffer, &vertexData, sizeof(DebugVertexCBuffer));
  FillBuffer(m_GenericPSCbuffer, &pixelData, sizeof(DebugPixelCBufferData));

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)m_width, (float)m_height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, m_width, m_height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    if(cfg.rawoutput || !blendAlpha || cfg.CustomShader != ResourceId())
    {
      if(m_BBFmtIdx == RGBA32_BACKBUFFER)
        list->SetPipelineState(m_TexDisplayF32Pipe);
      else
        list->SetPipelineState(m_TexDisplayPipe);
    }
    else
    {
      list->SetPipelineState(m_TexDisplayBlendPipe);
    }

    list->SetGraphicsRootSignature(m_TexDisplayRootSig);

    // Set the descriptor heap containing the texture srv
    ID3D12DescriptorHeap *heaps[] = {cbvsrvuavHeap, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);

    list->SetGraphicsRootConstantBufferView(0, m_GenericVSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, m_GenericPSCbuffer->GetGPUVirtualAddress());
    list->SetGraphicsRootDescriptorTable(2, cbvsrvuavHeap->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(3, samplerHeap->GetGPUDescriptorHandleForHeapStart());

    float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    list->OMSetBlendFactor(factor);

    list->DrawInstanced(4, 1, 0, 0);

    // transition back to where they were
    for(size_t i = 0; i < barriers.size(); i++)
      std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

    if(!barriers.empty())
      list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    m_WrappedDevice->ExecuteLists();
    m_WrappedDevice->FlushLists();
  }

  return true;
}
