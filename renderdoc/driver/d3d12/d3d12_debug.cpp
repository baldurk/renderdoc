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

#include "d3d12_debug.h"
#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
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
      RDCERR("Couldn't create blob of size %u from shadercache: HRESULT: %s", size,
             ToStr(hr).c_str());
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

  wrapper->GetReplay()->PostDeviceInitCounters();

  m_HighlightCache.driver = wrapper->GetReplay();

  m_OutputWindowID = 1;
  m_DSVID = 0;

  m_WrappedDevice = wrapper;
  m_WrappedDevice->InternalRef();

  m_TexResource = NULL;

  m_width = m_height = 1;
  m_BBFmtIdx = BGRA8_BACKBUFFER;

  RDCEraseEl(m_TileMinMaxPipe);
  RDCEraseEl(m_HistogramPipe);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.0f);

  m_pFactory = NULL;

  HRESULT hr = S_OK;

  hr = RENDERDOC_CreateWrappedDXGIFactory1(__uuidof(IDXGIFactory4), (void **)&m_pFactory);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DXGI factory! HRESULT: %s", ToStr(hr).c_str());
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
    RDCERR("Couldn't create RTV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&dsvHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DSV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  desc.NumDescriptors = 4096;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&uavClearHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&cbvsrvuavHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&samplerHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create sampler descriptor heap! HRESULT: %s", ToStr(hr).c_str());
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

    m_PickPixelTex->SetName(L"m_PickPixelTex");

    if(FAILED(hr))
    {
      RDCERR("Failed to create rendering texture for pixel picking, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    m_PickPixelRTV = GetCPUHandle(PICK_PIXEL_RTV);

    m_WrappedDevice->CreateRenderTargetView(m_PickPixelTex, NULL, m_PickPixelRTV);
  }

  m_PickVB = NULL;
  m_PickSize = 0;

  m_CustomShaderTex = NULL;

  CreateSOBuffers();

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

    m_ReadbackBuffer->SetName(L"m_ReadbackBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback buffer, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    hr = m_WrappedDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void **)&m_DebugAlloc);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback command allocator, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    hr = m_WrappedDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DebugAlloc, NULL,
                                            __uuidof(ID3D12GraphicsCommandList),
                                            (void **)&m_DebugList);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback command list, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    if(m_DebugList)
      m_DebugList->Close();
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.2f);

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

  static const UINT64 bufsize = 2 * 1024 * 1024;

  m_RingConstantBuffer = MakeCBuffer(bufsize);
  m_RingConstantOffset = 0;

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.4f);

  bool success = LoadShaderCache("d3d12shaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, ShaderCache12Callbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  m_CacheShaders = true;

  vector<D3D12_ROOT_PARAMETER1> rootSig;

  D3D12_ROOT_PARAMETER1 param = {};

  // VS CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.RegisterSpace = 0;
  param.Descriptor.ShaderRegister = 0;
  param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  rootSig.push_back(param);

  // PS CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.Descriptor.ShaderRegister = 0;

  rootSig.push_back(param);

  // GS CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
  param.Descriptor.ShaderRegister = 0;

  rootSig.push_back(param);

  // push constant CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  param.Constants.Num32BitValues = 4;
  param.Constants.RegisterSpace = 0;
  param.Constants.ShaderRegister = 2;

  rootSig.push_back(param);

  ID3DBlob *root = MakeRootSig(rootSig, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature), (void **)&m_CBOnlyRootSig);

  SAFE_RELEASE(root);

  // remove GS cbuffer and push constant
  rootSig.pop_back();
  rootSig.pop_back();

  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
  param.Descriptor.RegisterSpace = 0;
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

  rootSig.clear();

  // 0: CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.RegisterSpace = 0;
  param.Descriptor.ShaderRegister = 0;
  param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  rootSig.push_back(param);

  srvrange.NumDescriptors = 1;

  // 1: SRV
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &srvrange;

  rootSig.push_back(param);

  root = MakeRootSig(rootSig);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature),
                                            (void **)&m_QuadResolveRootSig);

  SAFE_RELEASE(root);

  rootSig.clear();

  // 0: CBV
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.Descriptor.RegisterSpace = 0;
  param.Descriptor.ShaderRegister = 0;
  param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  rootSig.push_back(param);

  // 0: SRVs
  srvrange.NumDescriptors = 2;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.pDescriptorRanges = &srvrange;
  param.DescriptorTable.NumDescriptorRanges = 1;

  rootSig.push_back(param);

  // 0: UAV
  uavrange.NumDescriptors = 1;

  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.pDescriptorRanges = &uavrange;
  param.DescriptorTable.NumDescriptorRanges = 1;

  rootSig.push_back(param);

  root = MakeRootSig(rootSig);

  RDCASSERT(root);

  hr = m_WrappedDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                            __uuidof(ID3D12RootSignature),
                                            (void **)&m_MeshPickRootSig);

  SAFE_RELEASE(root);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.6f);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  RDCEraseEl(pipeDesc);

  string displayhlsl = GetEmbeddedResource(debugcbuffers_h);
  displayhlsl += GetEmbeddedResource(debugcommon_hlsl);
  displayhlsl += GetEmbeddedResource(debugdisplay_hlsl);

  ID3DBlob *FullscreenVS = NULL;
  ID3DBlob *TexDisplayPS = NULL;
  ID3DBlob *CheckerboardPS = NULL;
  ID3DBlob *OutlinePS = NULL;
  ID3DBlob *QOResolvePS = NULL;

  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_DebugVS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0",
                &m_GenericVS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_FullscreenVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "vs_5_0", &FullscreenVS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_TexDisplayPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &TexDisplayPS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_CheckerboardPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &CheckerboardPS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_OutlinePS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &OutlinePS);
  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_QOResolvePS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &QOResolvePS);

  RDCASSERT(m_GenericVS);
  RDCASSERT(FullscreenVS);
  RDCASSERT(TexDisplayPS);
  RDCASSERT(CheckerboardPS);
  RDCASSERT(OutlinePS);
  RDCASSERT(QOResolvePS);

  pipeDesc.pRootSignature = m_TexDisplayRootSig;
  pipeDesc.VS.BytecodeLength = m_GenericVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = m_GenericVS->GetBufferPointer();
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
    RDCERR("Couldn't create m_TexDisplayBlendPipe! HRESULT: %s", ToStr(hr).c_str());
  }

  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayPipe! HRESULT: %s", ToStr(hr).c_str());
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayLinearPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayPipe! HRESULT: %s", ToStr(hr).c_str());
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayF32Pipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayF32Pipe! HRESULT: %s", ToStr(hr).c_str());
  }

  pipeDesc.pRootSignature = m_CBOnlyRootSig;

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  pipeDesc.PS.BytecodeLength = CheckerboardPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = CheckerboardPS->GetBufferPointer();

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_CheckerboardPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_CheckerboardPipe! HRESULT: %s", ToStr(hr).c_str());
  }

  pipeDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_CheckerboardMSAAPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_CheckerboardMSAAPipe! HRESULT: %s", ToStr(hr).c_str());
  }

  pipeDesc.SampleDesc.Count = 1;

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
    RDCERR("Couldn't create m_OutlinePipe! HRESULT: %s", ToStr(hr).c_str());
  }

  GetShaderBlob(displayhlsl.c_str(), "RENDERDOC_QuadOverdrawPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &m_QuadOverdrawWritePS);

  string meshhlsl = GetEmbeddedResource(debugcbuffers_h) + GetEmbeddedResource(mesh_hlsl);

  GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshVS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0",
                &m_MeshVS);
  GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshGS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "gs_5_0",
                &m_MeshGS);
  GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshPS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0",
                &m_MeshPS);
  GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_TriangleSizeGS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "gs_5_0", &m_TriangleSizeGS);
  GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_TriangleSizePS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                "ps_5_0", &m_TriangleSizePS);

  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

  pipeDesc.pRootSignature = m_QuadResolveRootSig;

  pipeDesc.PS.BytecodeLength = QOResolvePS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = QOResolvePS->GetBufferPointer();

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_QuadResolvePipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_QuadResolvePipe! HRESULT: %s", ToStr(hr).c_str());
  }

  m_OverlayRenderTex = NULL;
  m_OverlayResourceId = ResourceId();

  string histogramhlsl = GetEmbeddedResource(debugcbuffers_h);
  histogramhlsl += GetEmbeddedResource(debugcommon_hlsl);
  histogramhlsl += GetEmbeddedResource(histogram_hlsl);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.7f);

  D3D12_COMPUTE_PIPELINE_STATE_DESC compPipeDesc;
  RDCEraseEl(compPipeDesc);

  compPipeDesc.pRootSignature = m_MeshPickRootSig;

  ID3DBlob *meshPickCS;

  GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshPickCS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0",
                &meshPickCS);

  RDCASSERT(meshPickCS);

  compPipeDesc.CS.BytecodeLength = meshPickCS->GetBufferSize();
  compPipeDesc.CS.pShaderBytecode = meshPickCS->GetBufferPointer();

  hr = m_WrappedDevice->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                   (void **)&m_MeshPickPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_MeshPickPipe! HRESULT: %s", ToStr(hr).c_str());
  }

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

      string hlsl = string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
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
        RDCERR("Couldn't create m_TileMinMaxPipe! HRESULT: %s", ToStr(hr).c_str());
      }

      GetShaderBlob(hlsl.c_str(), "RENDERDOC_HistogramCS", D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0",
                    &histogram);

      compPipeDesc.CS.BytecodeLength = histogram->GetBufferSize();
      compPipeDesc.CS.pShaderBytecode = histogram->GetBufferPointer();

      hr = m_WrappedDevice->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                       (void **)&m_HistogramPipe[t][i]);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create m_HistogramPipe! HRESULT: %s", ToStr(hr).c_str());
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
          RDCERR("Couldn't create m_HistogramPipe! HRESULT: %s", ToStr(hr).c_str());
        }
      }

      SAFE_RELEASE(tile);
      SAFE_RELEASE(histogram);
      SAFE_RELEASE(result);
    }
  }

  SAFE_RELEASE(FullscreenVS);
  SAFE_RELEASE(TexDisplayPS);
  SAFE_RELEASE(OutlinePS);
  SAFE_RELEASE(QOResolvePS);
  SAFE_RELEASE(CheckerboardPS);

  {
    D3D12_RESOURCE_DESC pickResultDesc = {};
    pickResultDesc.Alignment = 0;
    pickResultDesc.DepthOrArraySize = 1;
    pickResultDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    pickResultDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    pickResultDesc.Format = DXGI_FORMAT_UNKNOWN;
    pickResultDesc.Height = 1;
    pickResultDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    pickResultDesc.MipLevels = 1;
    pickResultDesc.SampleDesc.Count = 1;
    pickResultDesc.SampleDesc.Quality = 0;
    // add an extra 64 bytes for the counter at the start
    pickResultDesc.Width = m_MaxMeshPicks * sizeof(Vec4f) + 64;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_WrappedDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &pickResultDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        NULL, __uuidof(ID3D12Resource), (void **)&m_PickResultBuf);

    m_PickResultBuf->SetName(L"m_PickResultBuf");

    if(FAILED(hr))
    {
      RDCERR("Failed to create tile buffer for min/max, HRESULT: %s", ToStr(hr).c_str());
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    // start with elements after the counter
    uavDesc.Buffer.FirstElement = 64 / sizeof(Vec4f);
    uavDesc.Buffer.NumElements = m_MaxMeshPicks;
    uavDesc.Buffer.StructureByteStride = sizeof(Vec4f);

    m_WrappedDevice->CreateUnorderedAccessView(m_PickResultBuf, m_PickResultBuf, &uavDesc,
                                               GetCPUHandle(PICK_RESULT_UAV));
    m_WrappedDevice->CreateUnorderedAccessView(m_PickResultBuf, m_PickResultBuf, &uavDesc,
                                               GetUAVClearHandle(PICK_RESULT_UAV));

    // this UAV is used for clearing everything back to 0

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = m_MaxMeshPicks + 64 / sizeof(Vec4f);
    uavDesc.Buffer.StructureByteStride = 0;

    m_WrappedDevice->CreateUnorderedAccessView(m_PickResultBuf, NULL, &uavDesc,
                                               GetCPUHandle(PICK_RESULT_CLEAR_UAV));
    m_WrappedDevice->CreateUnorderedAccessView(m_PickResultBuf, NULL, &uavDesc,
                                               GetUAVClearHandle(PICK_RESULT_CLEAR_UAV));
  }

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

    m_MinMaxTileBuffer->SetName(L"m_MinMaxTileBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create tile buffer for min/max, HRESULT: %s", ToStr(hr).c_str());
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
    m_WrappedDevice->CreateUnorderedAccessView(m_MinMaxTileBuffer, NULL, &tileDesc,
                                               GetUAVClearHandle(HISTOGRAM_UAV));

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

    m_MinMaxResultBuffer->SetName(L"m_MinMaxResultBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create result buffer for min/max, HRESULT: %s", ToStr(hr).c_str());
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

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.8f);

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
      RDCERR("Failed to create uploadBuf HRESULT: %s", ToStr(hr).c_str());

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

    m_Font.Tex->SetName(L"m_Font.Tex");

    if(FAILED(hr))
      RDCERR("Failed to create m_Font.Tex HRESULT: %s", ToStr(hr).c_str());

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

    FillBuffer(uploadBuf, 0, buf, width * height);

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

    FillBuffer(m_Font.GlyphData, 0, &glyphData, sizeof(glyphData));

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
      RDCERR("Couldn't create BGRA8 m_Font.Pipe! HRESULT: %s", ToStr(hr).c_str());

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA8_SRGB_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create BGRA8 m_Font.Pipe! HRESULT: %s", ToStr(hr).c_str());

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA8_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create RGBA8 m_Font.Pipe! HRESULT: %s", ToStr(hr).c_str());

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA16_BACKBUFFER]);

    if(FAILED(hr))
      RDCERR("Couldn't create RGBA16 m_Font.Pipe! HRESULT: %s", ToStr(hr).c_str());

    SAFE_RELEASE(TextVS);
    SAFE_RELEASE(TextPS);
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 1.0f);

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

  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(size_t p = 0; p < MeshDisplayPipelines::ePipe_Count; p++)
      SAFE_RELEASE(it->second.pipes[p]);

  SAFE_RELEASE(m_pFactory);

  SAFE_RELEASE(dsvHeap);
  SAFE_RELEASE(rtvHeap);
  SAFE_RELEASE(cbvsrvuavHeap);
  SAFE_RELEASE(uavClearHeap);
  SAFE_RELEASE(samplerHeap);

  SAFE_RELEASE(m_RingConstantBuffer);

  SAFE_RELEASE(m_TexDisplayBlendPipe);
  SAFE_RELEASE(m_TexDisplayPipe);
  SAFE_RELEASE(m_TexDisplayLinearPipe);
  SAFE_RELEASE(m_TexDisplayF32Pipe);
  SAFE_RELEASE(m_TexDisplayRootSig);
  SAFE_RELEASE(m_GenericVS);

  SAFE_RELEASE(m_CBOnlyRootSig);
  SAFE_RELEASE(m_CheckerboardPipe);
  SAFE_RELEASE(m_CheckerboardMSAAPipe);
  SAFE_RELEASE(m_OutlinePipe);

  SAFE_RELEASE(m_QuadOverdrawWritePS);
  SAFE_RELEASE(m_QuadResolveRootSig);
  SAFE_RELEASE(m_QuadResolvePipe);

  SAFE_RELEASE(m_PickPixelTex);

  SAFE_RELEASE(m_MeshPickRootSig);
  SAFE_RELEASE(m_MeshPickPipe);
  SAFE_RELEASE(m_PickResultBuf);
  SAFE_RELEASE(m_PickVB);

  SAFE_RELEASE(m_CustomShaderTex);

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);

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

  SAFE_RELEASE(m_MeshVS);
  SAFE_RELEASE(m_MeshGS);
  SAFE_RELEASE(m_MeshPS);
  SAFE_RELEASE(m_TriangleSizeGS);
  SAFE_RELEASE(m_TriangleSizePS);

  SAFE_RELEASE(m_TexResource);

  if(m_OverlayResourceId != ResourceId())
    SAFE_RELEASE(m_OverlayRenderTex);

  SAFE_RELEASE(m_ReadbackBuffer);
  SAFE_RELEASE(m_DebugAlloc);
  SAFE_RELEASE(m_DebugList);

  m_WrappedDevice->InternalRelease();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void D3D12DebugManager::CreateSOBuffers()
{
  HRESULT hr = S_OK;

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);
  SAFE_RELEASE(m_SOPatchedIndexBuffer);
  SAFE_RELEASE(m_SOQueryHeap);

  D3D12_RESOURCE_DESC soBufDesc;
  soBufDesc.Alignment = 0;
  soBufDesc.DepthOrArraySize = 1;
  soBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  // need to allow UAV access to reset the counter each time
  soBufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  soBufDesc.Format = DXGI_FORMAT_UNKNOWN;
  soBufDesc.Height = 1;
  soBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  soBufDesc.MipLevels = 1;
  soBufDesc.SampleDesc.Count = 1;
  soBufDesc.SampleDesc.Quality = 0;
  // add 64 bytes for the counter at the start
  soBufDesc.Width = m_SOBufferSize + 64;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc,
                                                D3D12_RESOURCE_STATE_STREAM_OUT, NULL,
                                                __uuidof(ID3D12Resource), (void **)&m_SOBuffer);

  m_SOBuffer->SetName(L"m_SOBuffer");

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO output buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  soBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;

  hr = m_WrappedDevice->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
      __uuidof(ID3D12Resource), (void **)&m_SOStagingBuffer);

  m_SOStagingBuffer->SetName(L"m_SOStagingBuffer");

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  // this is a buffer of unique indices, so it allows for
  // the worst case - float4 per vertex, all unique indices.
  soBufDesc.Width = m_SOBufferSize / sizeof(Vec4f);
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  hr = m_WrappedDevice->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
      __uuidof(ID3D12Resource), (void **)&m_SOPatchedIndexBuffer);

  m_SOPatchedIndexBuffer->SetName(L"m_SOPatchedIndexBuffer");

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO index buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  D3D12_QUERY_HEAP_DESC queryDesc;
  queryDesc.Count = 16;
  queryDesc.NodeMask = 1;
  queryDesc.Type = D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
  hr = m_WrappedDevice->CreateQueryHeap(&queryDesc, __uuidof(m_SOQueryHeap), (void **)&m_SOQueryHeap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO query heap, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc = {};
  counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  counterDesc.Format = DXGI_FORMAT_R32_UINT;
  counterDesc.Buffer.FirstElement = 0;
  counterDesc.Buffer.NumElements = 4;

  m_WrappedDevice->CreateUnorderedAccessView(m_SOBuffer, NULL, &counterDesc,
                                             GetCPUHandle(STREAM_OUT_UAV));

  m_WrappedDevice->CreateUnorderedAccessView(m_SOBuffer, NULL, &counterDesc,
                                             GetUAVClearHandle(STREAM_OUT_UAV));
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

  PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER deserializeRootSigOld =
      (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12CreateRootSignatureDeserializer");

  if(deserializeRootSig == NULL)
  {
    RDCWARN("Can't get D3D12CreateVersionedRootSignatureDeserializer - old version of windows?");

    if(deserializeRootSigOld == NULL)
    {
      RDCERR("Can't get D3D12CreateRootSignatureDeserializer!");
      return D3D12RootSignature();
    }

    ID3D12RootSignatureDeserializer *deser = NULL;
    HRESULT hr = deserializeRootSigOld(data, dataSize, __uuidof(ID3D12RootSignatureDeserializer),
                                       (void **)&deser);

    if(FAILED(hr))
    {
      SAFE_RELEASE(deser);
      RDCERR("Can't get deserializer");
      return D3D12RootSignature();
    }

    D3D12RootSignature ret;

    const D3D12_ROOT_SIGNATURE_DESC *desc = deser->GetRootSignatureDesc();
    if(FAILED(hr))
    {
      SAFE_RELEASE(deser);
      RDCERR("Can't get descriptor");
      return D3D12RootSignature();
    }

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

ID3DBlob *D3D12DebugManager::MakeRootSig(const std::vector<D3D12_ROOT_PARAMETER1> &params,
                                         D3D12_ROOT_SIGNATURE_FLAGS Flags, UINT NumStaticSamplers,
                                         const D3D12_STATIC_SAMPLER_DESC *StaticSamplers)
{
  PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE serializeRootSig =
      (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12SerializeVersionedRootSignature");

  PFN_D3D12_SERIALIZE_ROOT_SIGNATURE serializeRootSigOld =
      (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(GetModuleHandleA("d3d12.dll"),
                                                         "D3D12SerializeRootSignature");

  if(serializeRootSig == NULL)
  {
    RDCWARN("Can't get D3D12SerializeVersionedRootSignature - old version of windows?");

    if(serializeRootSigOld == NULL)
    {
      RDCERR("Can't get D3D12SerializeRootSignature!");
      return NULL;
    }

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.Flags = Flags;
    desc.NumStaticSamplers = NumStaticSamplers;
    desc.pStaticSamplers = StaticSamplers;
    desc.NumParameters = (UINT)params.size();

    std::vector<D3D12_ROOT_PARAMETER> params_1_0;
    params_1_0.resize(params.size());
    for(size_t i = 0; i < params.size(); i++)
    {
      params_1_0[i].ShaderVisibility = params[i].ShaderVisibility;
      params_1_0[i].ParameterType = params[i].ParameterType;

      if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      {
        params_1_0[i].Constants = params[i].Constants;
      }
      else if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        params_1_0[i].DescriptorTable.NumDescriptorRanges =
            params[i].DescriptorTable.NumDescriptorRanges;

        D3D12_DESCRIPTOR_RANGE *dst =
            new D3D12_DESCRIPTOR_RANGE[params[i].DescriptorTable.NumDescriptorRanges];
        params_1_0[i].DescriptorTable.pDescriptorRanges = dst;

        for(UINT r = 0; r < params[i].DescriptorTable.NumDescriptorRanges; r++)
        {
          dst[r].BaseShaderRegister =
              params[i].DescriptorTable.pDescriptorRanges[r].BaseShaderRegister;
          dst[r].NumDescriptors = params[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
          dst[r].OffsetInDescriptorsFromTableStart =
              params[i].DescriptorTable.pDescriptorRanges[r].OffsetInDescriptorsFromTableStart;
          dst[r].RangeType = params[i].DescriptorTable.pDescriptorRanges[r].RangeType;
          dst[r].RegisterSpace = params[i].DescriptorTable.pDescriptorRanges[r].RegisterSpace;

          if(params[i].DescriptorTable.pDescriptorRanges[r].Flags !=
             (D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE))
            RDCWARN("Losing information when reducing down to 1.0 root signature");
        }
      }
      else
      {
        params_1_0[i].Descriptor.RegisterSpace = params[i].Descriptor.RegisterSpace;
        params_1_0[i].Descriptor.ShaderRegister = params[i].Descriptor.ShaderRegister;

        if(params[i].Descriptor.Flags != D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE)
          RDCWARN("Losing information when reducing down to 1.0 root signature");
      }
    }

    desc.pParameters = &params_1_0[0];

    ID3DBlob *ret = NULL;
    ID3DBlob *errBlob = NULL;
    HRESULT hr = serializeRootSigOld(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &ret, &errBlob);

    for(size_t i = 0; i < params_1_0.size(); i++)
      if(params_1_0[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        delete[] params_1_0[i].DescriptorTable.pDescriptorRanges;

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

ID3DBlob *D3D12DebugManager::MakeRootSig(const D3D12RootSignature &rootsig)
{
  std::vector<D3D12_ROOT_PARAMETER1> params;
  params.resize(rootsig.params.size());
  for(size_t i = 0; i < params.size(); i++)
    params[i] = rootsig.params[i];

  return MakeRootSig(params, rootsig.Flags, (UINT)rootsig.samplers.size(),
                     rootsig.samplers.empty() ? NULL : &rootsig.samplers[0]);
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
    RDCERR("Couldn't create cbuffer size %llu! %s", size, ToStr(hr).c_str());
    SAFE_RELEASE(ret);
    return NULL;
  }

  return ret;
}

void D3D12DebugManager::FillBuffer(ID3D12Resource *buf, size_t offset, const void *data, size_t size)
{
  D3D12_RANGE range = {offset, offset + size};
  byte *ptr = NULL;
  HRESULT hr = buf->Map(0, &range, (void **)&ptr);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    memcpy(ptr + offset, data, size);
    buf->Unmap(0, &range);
  }
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12DebugManager::UploadConstants(const void *data, size_t size)
{
  D3D12_GPU_VIRTUAL_ADDRESS ret = m_RingConstantBuffer->GetGPUVirtualAddress();

  if(m_RingConstantOffset + size > m_RingConstantBuffer->GetDesc().Width)
    m_RingConstantOffset = 0;

  ret += m_RingConstantOffset;

  FillBuffer(m_RingConstantBuffer, (size_t)m_RingConstantOffset, data, size);

  m_RingConstantOffset += size;
  m_RingConstantOffset =
      AlignUp(m_RingConstantOffset, (UINT64)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

  return ret;
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

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetUAVClearHandle(CBVUAVSRVSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = uavClearHeap->GetCPUDescriptorHandleForHeapStart();
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
                                  uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  int oldW = GetWidth(), oldH = GetHeight();

  SetOutputDimensions(1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);

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

  SetOutputDimensions(oldW, oldH, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

  range.End = 0;

  if(SUCCEEDED(hr))
    m_ReadbackBuffer->Unmap(0, &range);
}

uint32_t D3D12DebugManager::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x,
                                       uint32_t y)
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

  ID3D12Resource *vb = NULL, *ib = NULL;
  DXGI_FORMAT ifmt = cfg.position.indexByteStride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

  if(cfg.position.vertexResourceId != ResourceId())
    vb = m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(
        cfg.position.vertexResourceId);

  if(cfg.position.indexResourceId != ResourceId())
    ib = m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(
        cfg.position.indexResourceId);

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers.
  // In the case of VB we also tightly pack and unpack the data. IB can just be
  // read as R16 or R32 via the SRV so it is just a straight copy

  D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {};
  sdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  sdesc.Format = ifmt;

  if(cfg.position.indexByteStride && ib)
  {
    sdesc.Buffer.FirstElement = cfg.position.indexByteOffset / (cfg.position.indexByteStride);
    sdesc.Buffer.NumElements = cfg.position.numIndices;
    m_WrappedDevice->CreateShaderResourceView(ib, &sdesc, GetCPUHandle(PICK_IB_SRV));
  }
  else
  {
    sdesc.Buffer.NumElements = 4;
    m_WrappedDevice->CreateShaderResourceView(NULL, &sdesc, GetCPUHandle(PICK_IB_SRV));
  }

  sdesc.Buffer.FirstElement = 0;
  sdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

  if(vb)
  {
    if(m_PickVB == NULL || m_PickSize < cfg.position.numIndices)
    {
      SAFE_RELEASE(m_PickVB);

      m_PickSize = cfg.position.numIndices;

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
      vbDesc.Width = sizeof(Vec4f) * cfg.position.numIndices;

      hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&m_PickVB);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create pick vertex buffer: HRESULT: %s", ToStr(hr).c_str());
        return ~0U;
      }

      m_PickVB->SetName(L"m_PickVB");

      sdesc.Buffer.NumElements = cfg.position.numIndices;
      m_WrappedDevice->CreateShaderResourceView(m_PickVB, &sdesc, GetCPUHandle(PICK_VB_SRV));
    }
  }
  else
  {
    sdesc.Buffer.NumElements = 4;
    m_WrappedDevice->CreateShaderResourceView(NULL, &sdesc, GetCPUHandle(PICK_VB_SRV));
  }

  // unpack and linearise the data
  {
    FloatVector *vbData = new FloatVector[cfg.position.numIndices];

    bytebuf oldData;
    GetBufferData(vb, cfg.position.vertexByteOffset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

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

    FillBuffer(m_PickVB, 0, vbData, sizeof(Vec4f) * cfg.position.numIndices);

    delete[] vbData;
  }

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->SetPipelineState(m_MeshPickPipe);

  list->SetComputeRootSignature(m_MeshPickRootSig);

  list->SetDescriptorHeaps(1, &cbvsrvuavHeap);

  list->SetComputeRootConstantBufferView(0, UploadConstants(&cbuf, sizeof(cbuf)));
  list->SetComputeRootDescriptorTable(1, GetGPUHandle(PICK_IB_SRV));
  list->SetComputeRootDescriptorTable(2, GetGPUHandle(PICK_RESULT_UAV));

  list->Dispatch(cfg.position.numIndices / 1024 + 1, 1, 1);

  list->Close();
  m_WrappedDevice->ExecuteLists();

  bytebuf results;
  GetBufferData(m_PickResultBuf, 0, 0, results);

  list = m_WrappedDevice->GetNewList();

  UINT zeroes[4] = {0, 0, 0, 0};
  list->ClearUnorderedAccessViewUint(GetGPUHandle(PICK_RESULT_CLEAR_UAV),
                                     GetUAVClearHandle(PICK_RESULT_CLEAR_UAV), m_PickResultBuf,
                                     zeroes, 0, NULL);

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
      for(uint32_t i = 1; i < RDCMIN(m_MaxMeshPicks, numResults); i++)
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
      for(uint32_t i = 1; i < RDCMIN(m_MaxMeshPicks, numResults); i++)
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

void D3D12DebugManager::FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
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

      std::string combinedName = outvars[outIdx].name.c_str();
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

        std::string base = outvars[outIdx].name.c_str();

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
                                             const bytebuf &data)
{
  size_t zero = 0;

  vector<ShaderVariable> v;
  FillCBufferVariables("", zero, flattenVec4s, invars, v, data);

  outvars.reserve(v.size());
  for(size_t i = 0; i < v.size(); i++)
    outvars.push_back(v[i]);
}

void D3D12DebugManager::BuildShader(string source, string entry,
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

  D3D12_SHADER_BYTECODE byteCode;
  byteCode.BytecodeLength = blob->GetBufferSize();
  byteCode.pShaderBytecode = blob->GetBufferPointer();

  WrappedID3D12Shader *sh = WrappedID3D12Shader::AddShader(byteCode, m_WrappedDevice, NULL);

  SAFE_RELEASE(blob);

  *id = sh->GetResourceID();
}

void D3D12DebugManager::GetBufferData(ResourceId buff, uint64_t offset, uint64_t length,
                                      bytebuf &retData)
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
                                      bytebuf &ret)
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
      RDCERR("Failed to map buffer directly for readback HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    memcpy(&ret[0], data + offset, (size_t)length);

    range.Begin = range.End = 0;

    buffer->Unmap(0, &range);

    return;
  }

  m_DebugList->Reset(m_DebugAlloc, NULL);

  D3D12_RESOURCE_BARRIER barrier = {};

  barrier.Transition.pResource = buffer;
  barrier.Transition.StateBefore = m_WrappedDevice->GetSubresourceStates(GetResID(buffer))[0];
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  if(barrier.Transition.StateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE)
    m_DebugList->ResourceBarrier(1, &barrier);

  while(length > 0)
  {
    uint64_t chunkSize = RDCMIN(length, m_ReadbackSize);

    m_DebugList->CopyBufferRegion(m_ReadbackBuffer, 0, buffer, offset, chunkSize);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_WrappedDevice->GPUSync();
    m_DebugAlloc->Reset();

    D3D12_RANGE range = {0, (size_t)chunkSize};

    void *data = NULL;
    HRESULT hr = m_ReadbackBuffer->Map(0, &range, &data);

    if(FAILED(hr))
    {
      RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
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

    m_DebugList->Reset(m_DebugAlloc, NULL);
  }

  if(barrier.Transition.StateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE)
  {
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

    m_DebugList->ResourceBarrier(1, &barrier);
  }

  m_DebugList->Close();

  ID3D12CommandList *l = m_DebugList;
  m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
  m_WrappedDevice->GPUSync();
  m_DebugAlloc->Reset();
}

void D3D12DebugManager::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
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
    hr = m_WrappedDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc,
                                                  D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&remapTexture);
    RDCASSERTEQUAL(hr, S_OK);

    int oldW = m_width, oldH = m_height;
    BackBufferFormat idx = m_BBFmtIdx;

    m_width = uint32_t(copyDesc.Width);
    m_height = copyDesc.Height;
    m_BBFmtIdx = IsSRGBFormat(copyDesc.Format) ? RGBA8_SRGB_BACKBUFFER : RGBA8_BACKBUFFER;

    m_WrappedDevice->CreateRenderTargetView(remapTexture, NULL, GetCPUHandle(GET_TEX_RTV));

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

      RenderTextureInternal(GetCPUHandle(GET_TEX_RTV), texDisplay, false);
    }

    m_width = oldW;
    m_height = oldH;
    m_BBFmtIdx = idx;

    tmpTexture = srcTexture = remapTexture;

    list = m_WrappedDevice->GetNewList();

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
    hr = m_WrappedDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &copyDesc, D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL,
        __uuidof(ID3D12Resource), (void **)&resolveTexture);
    RDCASSERTEQUAL(hr, S_OK);

    RDCASSERT(!isDepth && !isStencil);

    list = m_WrappedDevice->GetNewList();

    // put source texture into resolve source state
    const vector<D3D12_RESOURCE_STATES> &states = m_WrappedDevice->GetSubresourceStates(tex);

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
    list = m_WrappedDevice->GetNewList();

  std::vector<D3D12_RESOURCE_BARRIER> barriers;

  // if we have no tmpImage, we're copying directly from the real image
  if(tmpTexture == NULL)
  {
    const vector<D3D12_RESOURCE_STATES> &states = m_WrappedDevice->GetSubresourceStates(tex);
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
  m_WrappedDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo));

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
    m_WrappedDevice->GetCopyableFootprints(&copyDesc, sub, 1, readbackDesc.Width, layouts + i,
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
  hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
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

  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();

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

void D3D12DebugManager::RenderCheckerboard()
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

  D3D12_GPU_VIRTUAL_ADDRESS vs = UploadConstants(&vertexData, sizeof(DebugVertexCBuffer));
  D3D12_GPU_VIRTUAL_ADDRESS ps = UploadConstants(&pixelData, sizeof(pixelData));

  OutputWindow &outw = m_OutputWindows[m_CurrentOutputWindow];

  {
    ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

    list->OMSetRenderTargets(1, &outw.rtv, TRUE, NULL);

    D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
    list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, outw.width, outw.height};
    list->RSSetScissorRects(1, &scissor);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    list->SetPipelineState(outw.depth ? m_CheckerboardMSAAPipe : m_CheckerboardPipe);

    list->SetGraphicsRootSignature(m_CBOnlyRootSig);

    list->SetGraphicsRootConstantBufferView(0, vs);
    list->SetGraphicsRootConstantBufferView(1, ps);
    list->SetGraphicsRootConstantBufferView(2, vs);

    Vec4f dummy;
    list->SetGraphicsRoot32BitConstants(3, 4, &dummy.x, 0);

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

  FillBuffer(m_Font.Constants[m_Font.ConstRingIdx], 0, &data, sizeof(FontCBuffer));

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
    RDCERR("Failed to map charbuffer HRESULT: %s", ToStr(hr).c_str());
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

bool D3D12DebugManager::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip,
                                  uint32_t sample, CompType typeHint, float *minval, float *maxval)
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

    list->SetComputeRootConstantBufferView(0, UploadConstants(&cdata, sizeof(cdata)));
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

    m_ReadbackBuffer->Unmap(0, &range);
  }

  return true;
}

bool D3D12DebugManager::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip,
                                     uint32_t sample, CompType typeHint, float minval, float maxval,
                                     bool channels[4], vector<uint32_t> &histogram)
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
    D3D12_CPU_DESCRIPTOR_HANDLE uavcpu = GetUAVClearHandle(HISTOGRAM_UAV);

    UINT zeroes[] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(uav, uavcpu, m_MinMaxTileBuffer, zeroes, 0, NULL);

    list->SetComputeRootConstantBufferView(0, UploadConstants(&cdata, sizeof(cdata)));
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
    RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
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

ResourceId D3D12DebugManager::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                                uint32_t arrayIdx, uint32_t sampleIdx,
                                                CompType typeHint)
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

    HRESULT hr = m_WrappedDevice->CreateCommittedResource(
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

  m_WrappedDevice->CreateRenderTargetView(m_CustomShaderTex, &rtvDesc,
                                          GetCPUHandle(CUSTOM_SHADER_RTV));

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  float clr[] = {0.0f, 0.0f, 0.0f, 0.0f};
  list->ClearRenderTargetView(GetCPUHandle(CUSTOM_SHADER_RTV), clr, 0, NULL);

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

  SetOutputDimensions(RDCMAX(1U, (UINT)resDesc.Width >> mip), RDCMAX(1U, resDesc.Height >> mip),
                      resDesc.Format);

  RenderTextureInternal(GetCPUHandle(CUSTOM_SHADER_RTV), disp, true);

  return m_CustomShaderResourceId;
}
