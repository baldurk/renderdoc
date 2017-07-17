/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

  wrapper->GetReplay()->PostDeviceInitCounters();

  m_HighlightCache.driver = wrapper->GetReplay();

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

  desc.NumDescriptors = 4096;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

  hr = m_WrappedDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                             (void **)&uavClearHeap);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! 0x%08x", hr);
  }

  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

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

    m_PickPixelTex->SetName(L"m_PickPixelTex");

    if(FAILED(hr))
    {
      RDCERR("Failed to create rendering texture for pixel picking, HRESULT: 0x%08x", hr);
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
      RDCERR("Failed to create readback buffer, HRESULT: 0x%08x", hr);
      return;
    }

    hr = m_WrappedDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void **)&m_DebugAlloc);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback command allocator, HRESULT: 0x%08x", hr);
      return;
    }

    hr = m_WrappedDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DebugAlloc, NULL,
                                            __uuidof(ID3D12GraphicsCommandList),
                                            (void **)&m_DebugList);

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback command list, HRESULT: 0x%08x", hr);
      return;
    }

    if(m_DebugList)
      m_DebugList->Close();
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

  static const UINT64 bufsize = 2 * 1024 * 1024;

  m_RingConstantBuffer = MakeCBuffer(bufsize);
  m_RingConstantOffset = 0;

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.4f);

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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.6f);

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
    RDCERR("Couldn't create m_TexDisplayBlendPipe! 0x%08x", hr);
  }

  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_TexDisplayPipe! 0x%08x", hr);
  }

  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_TexDisplayLinearPipe);

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

  pipeDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&m_CheckerboardMSAAPipe);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create m_CheckerboardMSAAPipe! 0x%08x", hr);
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
    RDCERR("Couldn't create m_OutlinePipe! 0x%08x", hr);
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
    RDCERR("Couldn't create m_QuadResolvePipe! 0x%08x", hr);
  }

  m_OverlayRenderTex = NULL;
  m_OverlayResourceId = ResourceId();

  string histogramhlsl = GetEmbeddedResource(debugcbuffers_h);
  histogramhlsl += GetEmbeddedResource(debugcommon_hlsl);
  histogramhlsl += GetEmbeddedResource(histogram_hlsl);

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.7f);

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
    RDCERR("Couldn't create m_MeshPickPipe! 0x%08x", hr);
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
      RDCERR("Failed to create tile buffer for min/max, HRESULT: 0x%08x", hr);
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

    m_Font.Tex->SetName(L"m_Font.Tex");

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
      RDCERR("Couldn't create BGRA8 m_Font.Pipe! 0x%08x", hr);

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    hr = m_WrappedDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&m_Font.Pipe[RGBA8_SRGB_BACKBUFFER]);

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

  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(size_t p = 0; p < MeshDisplayPipelines::ePipe_Count; p++)
      SAFE_RELEASE(it->second.pipes[p]);

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    SAFE_RELEASE(it->second.vsout.buf);
    SAFE_RELEASE(it->second.vsout.idxBuf);
    SAFE_RELEASE(it->second.gsout.buf);
    SAFE_RELEASE(it->second.gsout.idxBuf);
  }

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
    RDCERR("Failed to create SO output buffer, HRESULT: 0x%08x", hr);
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
    RDCERR("Failed to create readback buffer, HRESULT: 0x%08x", hr);
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
    RDCERR("Failed to create SO index buffer, HRESULT: 0x%08x", hr);
    return;
  }

  D3D12_QUERY_HEAP_DESC queryDesc;
  queryDesc.Count = 16;
  queryDesc.NodeMask = 1;
  queryDesc.Type = D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
  hr = m_WrappedDevice->CreateQueryHeap(&queryDesc, __uuidof(m_SOQueryHeap), (void **)&m_SOQueryHeap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO query heap, HRESULT: 0x%08x", hr);
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

ID3DBlob *D3D12DebugManager::MakeRootSig(const std::vector<D3D12_ROOT_PARAMETER1> params,
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
    RDCERR("Couldn't create cbuffer size %llu! 0x%08x", size, hr);
    SAFE_RELEASE(ret);
    return NULL;
  }

  return ret;
}

void D3D12DebugManager::OutputWindow::MakeRTV(bool multisampled)
{
  SAFE_RELEASE(col);
  SAFE_RELEASE(colResolve);

  D3D12_RESOURCE_DESC texDesc = bb[0]->GetDesc();

  texDesc.Alignment = 0;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  texDesc.SampleDesc.Count = multisampled ? D3D12_MSAA_SAMPLECOUNT : 1;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = S_OK;

  hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                    __uuidof(ID3D12Resource), (void **)&col);

  col->SetName(L"Output Window RTV");

  if(FAILED(hr))
  {
    RDCERR("Failed to create colour texture for window, HRESULT: 0x%08x", hr);
    return;
  }

  colResolve = NULL;

  if(multisampled)
  {
    texDesc.SampleDesc.Count = 1;

    hr = dev->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                      __uuidof(ID3D12Resource), (void **)&colResolve);

    col->SetName(L"Output Window Resolve");

    if(FAILED(hr))
    {
      RDCERR("Failed to create resolve texture for window, HRESULT: 0x%08x", hr);
      return;
    }
  }

  dev->CreateRenderTargetView(col, NULL, rtv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RTV for main window, HRESULT: 0x%08x", hr);
    SAFE_RELEASE(swap);
    SAFE_RELEASE(col);
    SAFE_RELEASE(colResolve);
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

  texDesc.Alignment = 0;
  texDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;
  texDesc.Format = DXGI_FORMAT_D32_FLOAT;
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

  col->SetName(L"Output Window Depth");

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
    SAFE_RELEASE(colResolve);
    SAFE_RELEASE(depth);
    SAFE_RELEASE(bb[0]);
    SAFE_RELEASE(bb[1]);
    return;
  }
}

uint64_t D3D12DebugManager::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
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
  outw.colResolve = NULL;
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
  SAFE_RELEASE(outw.colResolve);
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

void D3D12DebugManager::ClearOutputWindowColor(uint64_t id, float col[4])
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

  D3D12_RESOURCE_BARRIER barriers[3];
  RDCEraseEl(barriers);

  barriers[0].Transition.pResource = outw.col;
  barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barriers[0].Transition.StateAfter =
      outw.depth ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;

  barriers[1].Transition.pResource = outw.bb[outw.bbIdx];
  barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

  barriers[2].Transition.pResource = outw.colResolve;
  barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  // resolve or copy from colour to backbuffer
  if(outw.depth)
  {
    // transition colour to resolve source, resolve target to resolve dest, backbuffer to copy dest
    list->ResourceBarrier(3, barriers);

    // resolve then copy, as the resolve can't go from SRGB to non-SRGB target
    list->ResolveSubresource(barriers[2].Transition.pResource, 0, barriers[0].Transition.pResource,
                             0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    std::swap(barriers[2].Transition.StateBefore, barriers[2].Transition.StateAfter);

    // now move the resolve target into copy source
    list->ResourceBarrier(1, &barriers[2]);

    list->CopyResource(barriers[1].Transition.pResource, barriers[2].Transition.pResource);
  }
  else
  {
    // transition colour to copy source, backbuffer to copy dest
    list->ResourceBarrier(2, barriers);

    list->CopyResource(barriers[1].Transition.pResource, barriers[0].Transition.pResource);
  }

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

void D3D12DebugManager::FillBuffer(ID3D12Resource *buf, size_t offset, const void *data, size_t size)
{
  D3D12_RANGE range = {offset, offset + size};
  byte *ptr = NULL;
  HRESULT hr = buf->Map(0, &range, (void **)&ptr);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer %08x", hr);
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

uint32_t D3D12DebugManager::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x,
                                       uint32_t y)
{
  if(cfg.position.numVerts == 0)
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
  cbuf.PickIdx = cfg.position.idxByteWidth ? 1 : 0;
  cbuf.PickNumVerts = cfg.position.numVerts;
  cbuf.PickUnproject = cfg.position.unproject ? 1 : 0;

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(GetWidth()) / float(GetHeight()));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f pickMVP = projMat.Mul(camMat);

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
  switch(cfg.position.topo)
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
  DXGI_FORMAT ifmt = cfg.position.idxByteWidth == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

  if(cfg.position.buf != ResourceId())
    vb = m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.buf);

  if(cfg.position.idxbuf != ResourceId())
    ib = m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.idxbuf);

  HRESULT hr = S_OK;

  // most IB/VBs will not be available as SRVs. So, we copy into our own buffers.
  // In the case of VB we also tightly pack and unpack the data. IB can just be
  // read as R16 or R32 via the SRV so it is just a straight copy

  D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {};
  sdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  sdesc.Format = ifmt;

  if(cfg.position.idxByteWidth && ib)
  {
    sdesc.Buffer.FirstElement = cfg.position.idxoffs / (cfg.position.idxByteWidth);
    sdesc.Buffer.NumElements = cfg.position.numVerts;
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
    if(m_PickVB == NULL || m_PickSize < cfg.position.numVerts)
    {
      SAFE_RELEASE(m_PickVB);

      m_PickSize = cfg.position.numVerts;

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
      vbDesc.Width = sizeof(Vec4f) * cfg.position.numVerts;

      hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vbDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&m_PickVB);

      m_PickVB->SetName(L"m_PickVB");

      if(FAILED(hr))
      {
        RDCERR("Couldn't create pick vertex buffer: %08x", hr);
        return ~0U;
      }

      sdesc.Buffer.NumElements = cfg.position.numVerts;
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
    FloatVector *vbData = new FloatVector[cfg.position.numVerts];

    vector<byte> oldData;
    GetBufferData(vb, cfg.position.offset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid = true;

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numVerts; i++)
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

    FillBuffer(m_PickVB, 0, vbData, sizeof(Vec4f) * cfg.position.numVerts);

    delete[] vbData;
  }

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->SetPipelineState(m_MeshPickPipe);

  list->SetComputeRootSignature(m_MeshPickRootSig);

  list->SetDescriptorHeaps(1, &cbvsrvuavHeap);

  list->SetComputeRootConstantBufferView(0, UploadConstants(&cbuf, sizeof(cbuf)));
  list->SetComputeRootDescriptorTable(1, GetGPUHandle(PICK_IB_SRV));
  list->SetComputeRootDescriptorTable(2, GetGPUHandle(PICK_RESULT_UAV));

  list->Dispatch(cfg.position.numVerts / 1024 + 1, 1, 1);

  list->Close();
  m_WrappedDevice->ExecuteLists();

  vector<byte> results;
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
      case VARTYPE_INT: type = VarType::Int; break;
      case VARTYPE_FLOAT: type = VarType::Float; break;
      case VARTYPE_BOOL:
      case VARTYPE_UINT:
      case VARTYPE_UINT8: type = VarType::UInt; break;
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

  D3D12_SHADER_BYTECODE byteCode;
  byteCode.BytecodeLength = blob->GetBufferSize();
  byteCode.pShaderBytecode = blob->GetBufferPointer();

  WrappedID3D12Shader *sh = WrappedID3D12Shader::AddShader(byteCode, m_WrappedDevice, NULL);

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

byte *D3D12DebugManager::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                        const GetTextureDataParams &params, size_t &dataSize)
{
  bool wasms = false;

  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[tex];

  if(resource == NULL)
  {
    RDCERR("Trying to get texture data for unknown ID %llu!", tex);
    dataSize = 0;
    return new byte[0];
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

  if(params.remap)
  {
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

      texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
      texDisplay.HDRMul = -1.0f;
      texDisplay.linearDisplayAsGamma = false;
      texDisplay.overlay = DebugOverlay::NoOverlay;
      texDisplay.FlipY = false;
      texDisplay.mip = mip;
      texDisplay.sampleIdx = params.resolve ? ~0U : arrayIdx;
      texDisplay.CustomShader = ResourceId();
      texDisplay.sliceFace = arrayIdx;
      texDisplay.rangemin = params.blackPoint;
      texDisplay.rangemax = params.whitePoint;
      texDisplay.scale = 1.0f;
      texDisplay.texid = tex;
      texDisplay.typeHint = CompType::Typeless;
      texDisplay.rawoutput = false;
      texDisplay.offx = 0;
      texDisplay.offy = 0;

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
  D3D12_RANGE range = {0, dataSize};
  byte *pData = NULL;
  hr = readbackBuf->Map(0, &range, (void **)&pData);
  RDCASSERTEQUAL(hr, S_OK);

  RDCASSERT(pData != NULL);

  dataSize = GetByteSize(layouts[0].Footprint.Width, layouts[0].Footprint.Height,
                         layouts[0].Footprint.Depth, copyDesc.Format, 0);
  byte *ret = new byte[dataSize];

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

          uint32_t *dDst = (uint32_t *)(ret + dstRowPitch * row);
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

          uint32_t *dst = (uint32_t *)(ret + dstRowPitch * row);

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
        byte *dst = ret + dstRowPitch * row;

        memcpy(dst, src, dstRowPitch);
      }
    }
  }

  SAFE_DELETE_ARRAY(layouts);
  SAFE_DELETE_ARRAY(rowcounts);

  range.End = 0;
  readbackBuf->Unmap(0, &range);

  // clean up temporary objects
  SAFE_RELEASE(readbackBuf);
  SAFE_RELEASE(tmpTexture);

  return ret;
}

void D3D12DebugManager::InitPostVSBuffers(uint32_t eventID)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventID) != m_PostVSAlias.end())
    eventID = m_PostVSAlias[eventID];

  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    return;

  D3D12CommandData *cmd = m_WrappedDevice->GetQueue()->GetCommandData();
  const D3D12RenderState &rs = cmd->m_RenderState;

  if(rs.pipe == ResourceId())
    return;

  WrappedID3D12PipelineState *origPSO =
      m_WrappedDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(!origPSO->IsGraphics())
    return;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = origPSO->GetGraphicsDesc();

  if(psoDesc.VS.BytecodeLength == 0)
    return;

  WrappedID3D12Shader *vs = origPSO->VS();

  D3D_PRIMITIVE_TOPOLOGY topo = rs.topo;

  const DrawcallDescription *drawcall = m_WrappedDevice->GetDrawcall(eventID);

  if(drawcall->numIndices == 0)
    return;

  DXBC::DXBCFile *dxbcVS = vs->GetDXBC();

  RDCASSERT(dxbcVS);

  DXBC::DXBCFile *dxbcGS = NULL;

  WrappedID3D12Shader *gs = origPSO->GS();

  if(gs)
  {
    dxbcGS = gs->GetDXBC();

    RDCASSERT(dxbcGS);
  }

  DXBC::DXBCFile *dxbcDS = NULL;

  WrappedID3D12Shader *ds = origPSO->DS();

  if(ds)
  {
    dxbcDS = ds->GetDXBC();

    RDCASSERT(dxbcDS);
  }

  ID3D12RootSignature *soSig = NULL;

  HRESULT hr = S_OK;

  {
    WrappedID3D12RootSignature *sig =
        m_WrappedDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
            rs.graphics.rootsig);

    D3D12RootSignature rootsig = sig->sig;

    // create a root signature that allows stream out, if necessary
    if((rootsig.Flags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT) == 0)
    {
      rootsig.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

      ID3DBlob *blob = MakeRootSig(rootsig);

      hr = m_WrappedDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                                __uuidof(ID3D12RootSignature), (void **)&soSig);
      if(FAILED(hr))
      {
        RDCERR("Couldn't enable stream-out in root signature: 0x%08x", hr);
        return;
      }

      SAFE_RELEASE(blob);
    }
  }

  vector<D3D12_SO_DECLARATION_ENTRY> sodecls;

  UINT stride = 0;
  int posidx = -1;
  int numPosComponents = 0;

  if(!dxbcVS->m_OutputSig.empty())
  {
    for(size_t i = 0; i < dxbcVS->m_OutputSig.size(); i++)
    {
      SigParameter &sign = dxbcVS->m_OutputSig[i];

      D3D12_SO_DECLARATION_ENTRY decl;

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
      D3D12_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(sodecls.begin() + posidx);
      sodecls.insert(sodecls.begin(), pos);
    }

    // set up stream output entries and buffers
    psoDesc.StreamOutput.NumEntries = (UINT)sodecls.size();
    psoDesc.StreamOutput.pSODeclaration = &sodecls[0];
    psoDesc.StreamOutput.NumStrides = 1;
    psoDesc.StreamOutput.pBufferStrides = &stride;
    psoDesc.StreamOutput.RasterizedStream = D3D12_SO_NO_RASTERIZED_STREAM;

    // disable all other shader stages
    psoDesc.HS.BytecodeLength = 0;
    psoDesc.HS.pShaderBytecode = NULL;
    psoDesc.DS.BytecodeLength = 0;
    psoDesc.DS.pShaderBytecode = NULL;
    psoDesc.GS.BytecodeLength = 0;
    psoDesc.GS.pShaderBytecode = NULL;
    psoDesc.PS.BytecodeLength = 0;
    psoDesc.PS.pShaderBytecode = NULL;

    // disable any rasterization/use of output targets
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    if(soSig)
      psoDesc.pRootSignature = soSig;

    // render as points
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

    // disable outputs
    psoDesc.NumRenderTargets = 0;
    RDCEraseEl(psoDesc.RTVFormats);
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    ID3D12PipelineState *pipe = NULL;
    hr = m_WrappedDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&pipe);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create patched graphics pipeline: 0x%08x", hr);
      SAFE_RELEASE(soSig);
      return;
    }

    ID3D12Resource *idxBuf = NULL;

    bool recreate = false;
    uint64_t outputSize = stride * drawcall->numIndices * drawcall->numInstances;

    if(m_SOBufferSize < outputSize)
    {
      uint64_t oldSize = m_SOBufferSize;
      while(m_SOBufferSize < outputSize)
        m_SOBufferSize *= 2;
      RDCWARN("Resizing stream-out buffer from %llu to %llu for output data", oldSize,
              m_SOBufferSize);
      recreate = true;
    }

    if(!(drawcall->flags & DrawFlags::UseIBuffer))
    {
      if(recreate)
      {
        m_WrappedDevice->GPUSync();

        CreateSOBuffers();
      }

      m_DebugList->Reset(m_DebugAlloc, NULL);

      rs.ApplyState(m_DebugList);

      m_DebugList->SetPipelineState(pipe);

      if(soSig)
      {
        m_DebugList->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(m_DebugList);
      }

      D3D12_STREAM_OUTPUT_BUFFER_VIEW view;
      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize;
      m_DebugList->SOSetTargets(0, 1, &view);

      m_DebugList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      m_DebugList->DrawInstanced(drawcall->numIndices, drawcall->numInstances,
                                 drawcall->vertexOffset, drawcall->instanceOffset);
    }
    else    // drawcall is indexed
    {
      vector<byte> idxdata;
      GetBufferData(rs.ibuffer.buf, rs.ibuffer.offs + drawcall->indexOffset * rs.ibuffer.bytewidth,
                    RDCMIN(drawcall->numIndices * rs.ibuffer.bytewidth, rs.ibuffer.size), idxdata);

      vector<uint32_t> indices;

      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      // only read as many indices as were available in the buffer
      uint32_t numIndices =
          RDCMIN(uint32_t(idxdata.size() / rs.ibuffer.bytewidth), drawcall->numIndices);

      uint32_t idxclamp = 0;
      if(drawcall->baseVertex < 0)
        idxclamp = uint32_t(-drawcall->baseVertex);

      // grab all unique vertex indices referenced
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = rs.ibuffer.bytewidth == 2 ? uint32_t(idx16[i]) : idx32[i];

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

      if(m_SOBufferSize / sizeof(Vec4f) < indices.size() * sizeof(uint32_t))
      {
        uint64_t oldSize = m_SOBufferSize;
        while(m_SOBufferSize / sizeof(Vec4f) < indices.size() * sizeof(uint32_t))
          m_SOBufferSize *= 2;
        RDCWARN("Resizing stream-out buffer from %llu to %llu for indices", oldSize, m_SOBufferSize);
        recreate = true;
      }

      if(recreate)
      {
        m_WrappedDevice->GPUSync();

        CreateSOBuffers();
      }

      FillBuffer(m_SOPatchedIndexBuffer, 0, &indices[0], indices.size() * sizeof(uint32_t));

      D3D12_INDEX_BUFFER_VIEW patchedIB;

      patchedIB.BufferLocation = m_SOPatchedIndexBuffer->GetGPUVirtualAddress();
      patchedIB.Format = DXGI_FORMAT_R32_UINT;
      patchedIB.SizeInBytes = UINT(indices.size() * sizeof(uint32_t));

      m_DebugList->Reset(m_DebugAlloc, NULL);

      rs.ApplyState(m_DebugList);

      m_DebugList->SetPipelineState(pipe);

      m_DebugList->IASetIndexBuffer(&patchedIB);

      if(soSig)
      {
        m_DebugList->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(m_DebugList);
      }

      D3D12_STREAM_OUTPUT_BUFFER_VIEW view;
      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize;
      m_DebugList->SOSetTargets(0, 1, &view);

      m_DebugList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

      m_DebugList->DrawIndexedInstanced((UINT)indices.size(), drawcall->numInstances, 0, 0,
                                        drawcall->instanceOffset);

      uint32_t stripCutValue = 0;
      if(psoDesc.IBStripCutValue == D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF)
        stripCutValue = 0xffff;
      else if(psoDesc.IBStripCutValue == D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF)
        stripCutValue = 0xffffffff;

      // rebase existing index buffer to point to the right elements in our stream-out'd
      // vertex buffer
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = rs.ibuffer.bytewidth == 2 ? uint32_t(idx16[i]) : idx32[i];

        // preserve primitive restart indices
        if(stripCutValue && i32 == stripCutValue)
          continue;

        // apply baseVertex but clamp to 0 (don't allow index to become negative)
        if(i32 < idxclamp)
          i32 = 0;
        else if(drawcall->baseVertex < 0)
          i32 -= idxclamp;
        else if(drawcall->baseVertex > 0)
          i32 += drawcall->baseVertex;

        if(rs.ibuffer.bytewidth == 2)
          idx16[i] = uint16_t(indexRemap[i32]);
        else
          idx32[i] = uint32_t(indexRemap[i32]);
      }

      idxBuf = NULL;

      if(!idxdata.empty())
      {
        D3D12_RESOURCE_DESC idxBufDesc;
        idxBufDesc.Alignment = 0;
        idxBufDesc.DepthOrArraySize = 1;
        idxBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        idxBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        idxBufDesc.Format = DXGI_FORMAT_UNKNOWN;
        idxBufDesc.Height = 1;
        idxBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        idxBufDesc.MipLevels = 1;
        idxBufDesc.SampleDesc.Count = 1;
        idxBufDesc.SampleDesc.Quality = 0;
        idxBufDesc.Width = idxdata.size();

        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &idxBufDesc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                      __uuidof(ID3D12Resource), (void **)&idxBuf);
        RDCASSERTEQUAL(hr, S_OK);

        SetObjName(idxBuf, StringFormat::Fmt("PostVS idxBuf for %u", eventID));

        FillBuffer(idxBuf, 0, &idxdata[0], idxdata.size());
      }
    }

    D3D12_RESOURCE_BARRIER sobarr = {};
    sobarr.Transition.pResource = m_SOBuffer;
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    m_DebugList->ResourceBarrier(1, &sobarr);

    m_DebugList->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    // we're done with this after the copy, so we can discard it and reset
    // the counter for the next stream-out
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    m_DebugList->DiscardResource(m_SOBuffer, NULL);
    m_DebugList->ResourceBarrier(1, &sobarr);

    UINT zeroes[4] = {0, 0, 0, 0};
    m_DebugList->ClearUnorderedAccessViewUint(
        GetGPUHandle(STREAM_OUT_UAV), GetUAVClearHandle(STREAM_OUT_UAV), m_SOBuffer, zeroes, 0, NULL);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_WrappedDevice->GPUSync();
    m_DebugAlloc->Reset();

    SAFE_RELEASE(pipe);

    byte *byteData = NULL;
    D3D12_RANGE range = {0, (SIZE_T)m_SOBufferSize};
    hr = m_SOStagingBuffer->Map(0, &range, (void **)&byteData);
    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer %08x", hr);
      SAFE_RELEASE(idxBuf);
      SAFE_RELEASE(soSig);
      return;
    }

    range.End = 0;

    uint64_t numBytesWritten = *(uint64_t *)byteData;

    if(numBytesWritten == 0)
    {
      m_PostVSData[eventID] = D3D12PostVSData();
      SAFE_RELEASE(idxBuf);
      SAFE_RELEASE(soSig);
      return;
    }

    // skip past the counter
    byteData += 64;

    uint64_t numPrims = numBytesWritten / stride;

    ID3D12Resource *vsoutBuffer = NULL;

    {
      D3D12_RESOURCE_DESC vertBufDesc;
      vertBufDesc.Alignment = 0;
      vertBufDesc.DepthOrArraySize = 1;
      vertBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      vertBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      vertBufDesc.Format = DXGI_FORMAT_UNKNOWN;
      vertBufDesc.Height = 1;
      vertBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      vertBufDesc.MipLevels = 1;
      vertBufDesc.SampleDesc.Count = 1;
      vertBufDesc.SampleDesc.Quality = 0;
      vertBufDesc.Width = numBytesWritten;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertBufDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&vsoutBuffer);
      RDCASSERTEQUAL(hr, S_OK);

      if(vsoutBuffer)
      {
        SetObjName(vsoutBuffer, StringFormat::Fmt("PostVS vsoutBuffer for %u", eventID));
        FillBuffer(vsoutBuffer, 0, byteData, (size_t)numBytesWritten);
      }
    }

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(uint64_t i = 1; numPosComponents == 4 && i < numPrims; i++)
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

    m_SOStagingBuffer->Unmap(0, &range);

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
          uint32_t(numBytesWritten / RDCMAX(1U, drawcall->numInstances));

    m_PostVSData[eventID].vsout.idxBuf = NULL;
    if(m_PostVSData[eventID].vsout.useIndices && idxBuf)
    {
      m_PostVSData[eventID].vsout.idxBuf = idxBuf;
      m_PostVSData[eventID].vsout.idxFmt =
          rs.ibuffer.bytewidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
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

      D3D12_SO_DECLARATION_ENTRY decl;

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
      D3D12_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(sodecls.begin() + posidx);
      sodecls.insert(sodecls.begin(), pos);
    }

    // enable the other shader stages again
    if(origPSO->DS())
      psoDesc.DS = origPSO->DS()->GetDesc();
    if(origPSO->HS())
      psoDesc.HS = origPSO->HS()->GetDesc();
    if(origPSO->GS())
      psoDesc.GS = origPSO->GS()->GetDesc();

    // configure new SO declarations
    psoDesc.StreamOutput.NumEntries = (UINT)sodecls.size();
    psoDesc.StreamOutput.pSODeclaration = &sodecls[0];
    psoDesc.StreamOutput.NumStrides = 1;
    psoDesc.StreamOutput.pBufferStrides = &stride;

    // we're using the same topology this time
    psoDesc.PrimitiveTopologyType = origPSO->graphics->PrimitiveTopologyType;

    ID3D12PipelineState *pipe = NULL;
    hr = m_WrappedDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&pipe);
    if(FAILED(hr))
    {
      RDCERR("Couldn't create patched graphics pipeline: 0x%08x", hr);
      SAFE_RELEASE(soSig);
      return;
    }

    D3D12_STREAM_OUTPUT_BUFFER_VIEW view;

    view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
    view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
    view.SizeInBytes = m_SOBufferSize;
    // draws with multiple instances must be replayed one at a time so we can record the number of
    // primitives from each drawcall, as due to expansion this can vary per-instance.
    if(drawcall->numInstances > 1)
    {
      m_DebugList->Reset(m_DebugAlloc, NULL);

      rs.ApplyState(m_DebugList);

      m_DebugList->SetPipelineState(pipe);

      if(soSig)
      {
        m_DebugList->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(m_DebugList);
      }

      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize;

      // do a dummy draw to make sure we have enough space in the output buffer
      m_DebugList->SOSetTargets(0, 1, &view);

      m_DebugList->BeginQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

      // because the result is expanded we don't have to remap index buffers or anything
      if(drawcall->flags & DrawFlags::UseIBuffer)
      {
        m_DebugList->DrawIndexedInstanced(drawcall->numIndices, drawcall->numInstances,
                                          drawcall->indexOffset, drawcall->baseVertex,
                                          drawcall->instanceOffset);
      }
      else
      {
        m_DebugList->DrawInstanced(drawcall->numIndices, drawcall->numInstances,
                                   drawcall->vertexOffset, drawcall->instanceOffset);
      }

      m_DebugList->EndQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

      m_DebugList->ResolveQueryData(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1,
                                    m_SOStagingBuffer, 0);

      m_DebugList->Close();

      ID3D12CommandList *l = m_DebugList;
      m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_WrappedDevice->GPUSync();

      // check that things are OK, and resize up if needed
      D3D12_RANGE range;
      range.Begin = 0;
      range.End = (SIZE_T)sizeof(D3D12_QUERY_DATA_SO_STATISTICS);

      D3D12_QUERY_DATA_SO_STATISTICS *data;
      hr = m_SOStagingBuffer->Map(0, &range, (void **)&data);

      D3D12_QUERY_DATA_SO_STATISTICS result = *data;

      range.End = 0;
      m_SOStagingBuffer->Unmap(0, &range);

      if(m_SOBufferSize < data->PrimitivesStorageNeeded * 3 * stride)
      {
        uint64_t oldSize = m_SOBufferSize;
        while(m_SOBufferSize < data->PrimitivesStorageNeeded * 3 * stride)
          m_SOBufferSize *= 2;
        RDCWARN("Resizing stream-out buffer from %llu to %llu for output", oldSize, m_SOBufferSize);
        CreateSOBuffers();
      }

      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize;

      m_DebugAlloc->Reset();

      // now do the actual stream out
      m_DebugList->Reset(m_DebugAlloc, NULL);

      // first need to reset the counter byte values which may have either been written to above, or
      // are newly created
      {
        D3D12_RESOURCE_BARRIER sobarr = {};
        sobarr.Transition.pResource = m_SOBuffer;
        sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
        sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        m_DebugList->ResourceBarrier(1, &sobarr);

        D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc = {};
        counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        counterDesc.Format = DXGI_FORMAT_R32_UINT;
        counterDesc.Buffer.FirstElement = 0;
        counterDesc.Buffer.NumElements = 4;

        UINT zeroes[4] = {0, 0, 0, 0};
        m_DebugList->ClearUnorderedAccessViewUint(GetGPUHandle(STREAM_OUT_UAV),
                                                  GetUAVClearHandle(STREAM_OUT_UAV), m_SOBuffer,
                                                  zeroes, 0, NULL);

        std::swap(sobarr.Transition.StateBefore, sobarr.Transition.StateAfter);
        m_DebugList->ResourceBarrier(1, &sobarr);
      }

      rs.ApplyState(m_DebugList);

      m_DebugList->SetPipelineState(pipe);

      if(soSig)
      {
        m_DebugList->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(m_DebugList);
      }

      // reserve space for enough 'buffer filled size' locations
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() +
                            AlignUp(uint64_t(drawcall->numInstances * sizeof(UINT64)), 64ULL);

      // do incremental draws to get the output size. We have to do this O(N^2) style because
      // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N instances
      // and count the total number of verts each time, then we can see from the difference how much
      // each instance wrote.
      for(uint32_t inst = 1; inst <= drawcall->numInstances; inst++)
      {
        if(drawcall->flags & DrawFlags::UseIBuffer)
        {
          view.BufferFilledSizeLocation =
              m_SOBuffer->GetGPUVirtualAddress() + (inst - 1) * sizeof(UINT64);
          m_DebugList->SOSetTargets(0, 1, &view);
          m_DebugList->DrawIndexedInstanced(drawcall->numIndices, inst, drawcall->indexOffset,
                                            drawcall->baseVertex, drawcall->instanceOffset);
        }
        else
        {
          view.BufferFilledSizeLocation =
              m_SOBuffer->GetGPUVirtualAddress() + (inst - 1) * sizeof(UINT64);
          m_DebugList->SOSetTargets(0, 1, &view);
          m_DebugList->DrawInstanced(drawcall->numIndices, inst, drawcall->vertexOffset,
                                     drawcall->instanceOffset);
        }
      }

      m_DebugList->Close();

      l = m_DebugList;
      m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_WrappedDevice->GPUSync();

      // the last draw will have written the actual data we want into the buffer
    }
    else
    {
      // this only loops if we find from a query that we need to resize up
      while(true)
      {
        m_DebugList->Reset(m_DebugAlloc, NULL);

        rs.ApplyState(m_DebugList);

        m_DebugList->SetPipelineState(pipe);

        if(soSig)
        {
          m_DebugList->SetGraphicsRootSignature(soSig);
          rs.ApplyGraphicsRootElements(m_DebugList);
        }

        view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
        view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
        view.SizeInBytes = m_SOBufferSize;

        m_DebugList->SOSetTargets(0, 1, &view);

        m_DebugList->BeginQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

        // because the result is expanded we don't have to remap index buffers or anything
        if(drawcall->flags & DrawFlags::UseIBuffer)
        {
          m_DebugList->DrawIndexedInstanced(drawcall->numIndices, drawcall->numInstances,
                                            drawcall->indexOffset, drawcall->baseVertex,
                                            drawcall->instanceOffset);
        }
        else
        {
          m_DebugList->DrawInstanced(drawcall->numIndices, drawcall->numInstances,
                                     drawcall->vertexOffset, drawcall->instanceOffset);
        }

        m_DebugList->EndQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

        m_DebugList->ResolveQueryData(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1,
                                      m_SOStagingBuffer, 0);

        m_DebugList->Close();

        ID3D12CommandList *l = m_DebugList;
        m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
        m_WrappedDevice->GPUSync();

        // check that things are OK, and resize up if needed
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = (SIZE_T)sizeof(D3D12_QUERY_DATA_SO_STATISTICS);

        D3D12_QUERY_DATA_SO_STATISTICS *data;
        hr = m_SOStagingBuffer->Map(0, &range, (void **)&data);

        if(m_SOBufferSize < data->PrimitivesStorageNeeded * 3 * stride)
        {
          uint64_t oldSize = m_SOBufferSize;
          while(m_SOBufferSize < data->PrimitivesStorageNeeded * 3 * stride)
            m_SOBufferSize *= 2;
          RDCWARN("Resizing stream-out buffer from %llu to %llu for output", oldSize, m_SOBufferSize);
          CreateSOBuffers();

          continue;
        }

        range.End = 0;
        m_SOStagingBuffer->Unmap(0, &range);

        m_DebugAlloc->Reset();

        break;
      }
    }

    m_DebugList->Reset(m_DebugAlloc, NULL);

    D3D12_RESOURCE_BARRIER sobarr = {};
    sobarr.Transition.pResource = m_SOBuffer;
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    m_DebugList->ResourceBarrier(1, &sobarr);

    m_DebugList->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    // we're done with this after the copy, so we can discard it and reset
    // the counter for the next stream-out
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    m_DebugList->DiscardResource(m_SOBuffer, NULL);
    m_DebugList->ResourceBarrier(1, &sobarr);

    D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc = {};
    counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    counterDesc.Format = DXGI_FORMAT_R32_UINT;
    counterDesc.Buffer.FirstElement = 0;
    counterDesc.Buffer.NumElements = 4;

    UINT zeroes[4] = {0, 0, 0, 0};
    m_DebugList->ClearUnorderedAccessViewUint(
        GetGPUHandle(STREAM_OUT_UAV), GetUAVClearHandle(STREAM_OUT_UAV), m_SOBuffer, zeroes, 0, NULL);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    m_WrappedDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_WrappedDevice->GPUSync();
    m_DebugAlloc->Reset();

    SAFE_RELEASE(pipe);

    byte *byteData = NULL;
    D3D12_RANGE range = {0, (SIZE_T)m_SOBufferSize};
    hr = m_SOStagingBuffer->Map(0, &range, (void **)&byteData);
    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer %08x", hr);
      SAFE_RELEASE(soSig);
      return;
    }

    range.End = 0;

    uint64_t *counters = (uint64_t *)byteData;

    uint64_t numBytesWritten = 0;
    std::vector<D3D12PostVSData::InstData> instData;
    if(drawcall->numInstances > 1)
    {
      uint64_t prevByteCount = 0;

      for(uint32_t inst = 0; inst < drawcall->numInstances; inst++)
      {
        uint64_t byteCount = counters[inst];

        D3D12PostVSData::InstData d;
        d.numVerts = uint32_t((byteCount - prevByteCount) / stride);
        d.bufOffset = prevByteCount;
        prevByteCount = byteCount;

        instData.push_back(d);
      }

      numBytesWritten = prevByteCount;
    }
    else
    {
      numBytesWritten = counters[0];
    }

    if(numBytesWritten == 0)
    {
      SAFE_RELEASE(soSig);
      return;
    }

    // skip past the counter(s)
    byteData += (view.BufferLocation - m_SOBuffer->GetGPUVirtualAddress());

    uint64_t numVerts = numBytesWritten / stride;

    ID3D12Resource *gsoutBuffer = NULL;

    {
      D3D12_RESOURCE_DESC vertBufDesc;
      vertBufDesc.Alignment = 0;
      vertBufDesc.DepthOrArraySize = 1;
      vertBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      vertBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      vertBufDesc.Format = DXGI_FORMAT_UNKNOWN;
      vertBufDesc.Height = 1;
      vertBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      vertBufDesc.MipLevels = 1;
      vertBufDesc.SampleDesc.Count = 1;
      vertBufDesc.SampleDesc.Quality = 0;
      vertBufDesc.Width = numBytesWritten;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertBufDesc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&gsoutBuffer);
      RDCASSERTEQUAL(hr, S_OK);

      if(gsoutBuffer)
      {
        SetObjName(gsoutBuffer, StringFormat::Fmt("PostVS gsoutBuffer for %u", eventID));
        FillBuffer(gsoutBuffer, 0, byteData, (size_t)numBytesWritten);
      }
    }

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numVerts; i++)
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

    m_SOStagingBuffer->Unmap(0, &range);

    m_PostVSData[eventID].gsout.buf = gsoutBuffer;
    m_PostVSData[eventID].gsout.instStride = 0;
    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventID].gsout.instStride =
          uint32_t(numBytesWritten / RDCMAX(1U, drawcall->numInstances));
    m_PostVSData[eventID].gsout.vertStride = stride;
    m_PostVSData[eventID].gsout.nearPlane = nearp;
    m_PostVSData[eventID].gsout.farPlane = farp;
    m_PostVSData[eventID].gsout.useIndices = false;
    m_PostVSData[eventID].gsout.hasPosOut = posidx >= 0;
    m_PostVSData[eventID].gsout.idxBuf = NULL;

    topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if(lastShader == dxbcGS)
    {
      for(size_t i = 0; i < dxbcGS->GetNumDeclarations(); i++)
      {
        const DXBC::ASMDecl &decl = dxbcGS->GetDeclaration(i);

        if(decl.declaration == DXBC::OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
        {
          topo = (D3D_PRIMITIVE_TOPOLOGY) int(decl.outTopology);    // enums match
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
            topo = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
          else
            topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
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

    m_PostVSData[eventID].gsout.numVerts = (uint32_t)numVerts;

    if(drawcall->flags & DrawFlags::Instanced)
      m_PostVSData[eventID].gsout.numVerts /= RDCMAX(1U, drawcall->numInstances);

    m_PostVSData[eventID].gsout.instData = instData;
  }

  SAFE_RELEASE(soSig);
}

MeshFormat D3D12DebugManager::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventID) != m_PostVSAlias.end())
    eventID = m_PostVSAlias[eventID];

  D3D12PostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    postvs = m_PostVSData[eventID];

  const D3D12PostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf != NULL)
  {
    ret.idxbuf = GetResID(s.idxBuf);
    ret.idxByteWidth = s.idxFmt == DXGI_FORMAT_R16_UINT ? 2 : 4;
  }
  else
  {
    ret.idxbuf = ResourceId();
    ret.idxByteWidth = 0;
  }
  ret.idxoffs = 0;
  ret.baseVertex = 0;

  if(s.buf != NULL)
    ret.buf = GetResID(s.buf);
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
    D3D12PostVSData::InstData inst = s.instData[instID];

    ret.offset = inst.bufOffset;
    ret.numVerts = inst.numVerts;
  }

  return ret;
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

D3D12DebugManager::MeshDisplayPipelines D3D12DebugManager::CacheMeshDisplayPipelines(
    const MeshFormat &primary, const MeshFormat &secondary)
{
  // generate a key to look up the map
  uint64_t key = 0;

  uint64_t bit = 0;

  if(primary.idxByteWidth == 4)
    key |= 1ULL << bit;
  bit++;

  RDCASSERT((uint32_t)primary.topo < 64);
  key |= uint64_t((uint32_t)primary.topo & 0x3f) << bit;
  bit += 6;

  ResourceFormat fmt;
  fmt.special = primary.specialFormat != SpecialFormat::Unknown;
  fmt.specialFormat = primary.specialFormat;
  fmt.compByteWidth = primary.compByteWidth;
  fmt.compCount = primary.compCount;
  fmt.compType = primary.compType;

  DXGI_FORMAT primaryFmt = MakeDXGIFormat(fmt);

  fmt.special = secondary.specialFormat != SpecialFormat::Unknown;
  fmt.specialFormat = secondary.specialFormat;
  fmt.compByteWidth = secondary.compByteWidth;
  fmt.compCount = secondary.compCount;
  fmt.compType = secondary.compType;

  DXGI_FORMAT secondaryFmt =
      secondary.buf == ResourceId() ? DXGI_FORMAT_UNKNOWN : MakeDXGIFormat(fmt);

  key |= uint64_t((uint32_t)primaryFmt & 0xff) << bit;
  bit += 8;

  key |= uint64_t((uint32_t)secondaryFmt & 0xff) << bit;
  bit += 8;

  RDCASSERT(primary.stride <= 0xffff);
  key |= uint64_t((uint32_t)primary.stride & 0xffff) << bit;
  bit += 16;

  if(secondary.buf != ResourceId())
  {
    RDCASSERT(secondary.stride <= 0xffff);
    key |= uint64_t((uint32_t)secondary.stride & 0xffff) << bit;
  }
  bit += 16;

  MeshDisplayPipelines &cache = m_CachedMeshPipelines[key];

  if(cache.pipes[(uint32_t)SolidShade::NoSolid] != NULL)
    return cache;

  // should we try and evict old pipelines from the cache here?
  // or just keep them forever

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  RDCEraseEl(pipeDesc);
  pipeDesc.pRootSignature = m_CBOnlyRootSig;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  D3D_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(primary.topo);

  if(topo == D3D_PRIMITIVE_TOPOLOGY_POINTLIST ||
     topo >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
  else if(topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP || topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST ||
          topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ || topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  else
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  pipeDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  D3D12_INPUT_ELEMENT_DESC ia[2] = {};
  ia[0].SemanticName = "pos";
  ia[0].Format = primaryFmt;
  ia[1].SemanticName = "sec";
  ia[1].InputSlot = 1;
  ia[1].Format = secondaryFmt == DXGI_FORMAT_UNKNOWN ? primaryFmt : secondaryFmt;
  ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

  pipeDesc.InputLayout.NumElements = 2;
  pipeDesc.InputLayout.pInputElementDescs = ia;

  RDCASSERT(primaryFmt != DXGI_FORMAT_UNKNOWN);

  // wireframe pipeline
  pipeDesc.VS.BytecodeLength = m_MeshVS->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = m_MeshVS->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = m_MeshPS->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = m_MeshPS->GetBufferPointer();

  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
  pipeDesc.DepthStencilState.DepthEnable = FALSE;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  HRESULT hr = S_OK;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Wire]);
  RDCASSERTEQUAL(hr, S_OK);

  pipeDesc.DepthStencilState.DepthEnable = TRUE;
  pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
  RDCASSERTEQUAL(hr, S_OK);

  // solid shading pipeline
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.DepthStencilState.DepthEnable = FALSE;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Solid]);
  RDCASSERTEQUAL(hr, S_OK);

  pipeDesc.DepthStencilState.DepthEnable = TRUE;
  pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

  hr = m_WrappedDevice->CreateGraphicsPipelineState(
      &pipeDesc, __uuidof(ID3D12PipelineState),
      (void **)&cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]);
  RDCASSERTEQUAL(hr, S_OK);

  if(secondary.buf != ResourceId())
  {
    // pull secondary information from second vertex buffer
    ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    RDCASSERT(secondaryFmt != DXGI_FORMAT_UNKNOWN);

    hr = m_WrappedDevice->CreateGraphicsPipelineState(
        &pipeDesc, __uuidof(ID3D12PipelineState),
        (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Secondary]);
    RDCASSERTEQUAL(hr, S_OK);
  }

  if(pipeDesc.PrimitiveTopologyType == D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
  {
    ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

    // flat lit pipeline, needs geometry shader to calculate face normals
    pipeDesc.GS.BytecodeLength = m_MeshGS->GetBufferSize();
    pipeDesc.GS.pShaderBytecode = m_MeshGS->GetBufferPointer();

    hr = m_WrappedDevice->CreateGraphicsPipelineState(
        &pipeDesc, __uuidof(ID3D12PipelineState),
        (void **)&cache.pipes[MeshDisplayPipelines::ePipe_Lit]);
    RDCASSERTEQUAL(hr, S_OK);
  }

  return cache;
}

void D3D12DebugManager::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                                   const MeshDisplay &cfg)
{
  if(cfg.position.buf == ResourceId() || cfg.position.numVerts == 0)
    return;

  auto it = m_OutputWindows.find(m_CurrentOutputWindow);
  if(m_CurrentOutputWindow == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  ID3D12GraphicsCommandList *list = m_WrappedDevice->GetNewList();

  list->OMSetRenderTargets(1, &outw.rtv, TRUE, &outw.dsv);

  D3D12_VIEWPORT viewport = {0, 0, (float)outw.width, (float)outw.height, 0.0f, 1.0f};
  list->RSSetViewports(1, &viewport);

  D3D12_RECT scissor = {0, 0, outw.width, outw.height};
  list->RSSetScissorRects(1, &scissor);

  DebugVertexCBuffer vertexData;

  vertexData.LineStrip = 0;

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, viewport.Width / viewport.Height);
  Matrix4f InvProj = projMat.Inverse();

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f guessProjInv;

  vertexData.ModelViewProj = projMat.Mul(camMat);
  vertexData.SpriteSize = Vec2f();

  DebugPixelCBufferData pixelData;

  pixelData.AlwaysZero = 0.0f;

  pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;
  pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 0.0f);

  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
    {
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
    }

    guessProjInv = guessProj.Inverse();

    vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
  }

  D3D12_GPU_VIRTUAL_ADDRESS vsCB = UploadConstants(&vertexData, sizeof(vertexData));

  if(!secondaryDraws.empty())
  {
    list->SetGraphicsRootSignature(m_CBOnlyRootSig);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, UploadConstants(&pixelData, sizeof(pixelData)));
    list->SetGraphicsRootConstantBufferView(2, vsCB);

    for(size_t i = 0; i < secondaryDraws.size(); i++)
    {
      const MeshFormat &fmt = secondaryDraws[i];

      DebugVertexCBuffer vdata;

      if(fmt.buf != ResourceId())
      {
        list->SetGraphicsRoot32BitConstants(3, 4, &fmt.meshColor.x, 0);

        MeshDisplayPipelines secondaryCache =
            CacheMeshDisplayPipelines(secondaryDraws[i], secondaryDraws[i]);

        list->SetPipelineState(secondaryCache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);

        ID3D12Resource *vb =
            m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.buf);

        UINT64 offs = fmt.offset;
        D3D12_VERTEX_BUFFER_VIEW view;
        view.BufferLocation = vb->GetGPUVirtualAddress() + offs;
        view.StrideInBytes = fmt.stride;
        view.SizeInBytes = UINT(vb->GetDesc().Width - offs);
        list->IASetVertexBuffers(0, 1, &view);

        // set it to the secondary buffer too just as dummy info
        list->IASetVertexBuffers(1, 1, &view);

        list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(fmt.topo));

        if(PatchList_Count(fmt.topo) > 0)
          list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

        if(fmt.idxByteWidth && fmt.idxbuf != ResourceId())
        {
          ID3D12Resource *ib =
              m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.idxbuf);

          D3D12_INDEX_BUFFER_VIEW iview;
          iview.BufferLocation = ib->GetGPUVirtualAddress() + fmt.idxoffs;
          iview.SizeInBytes = UINT(ib->GetDesc().Width - fmt.idxoffs);
          iview.Format = fmt.idxByteWidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
          list->IASetIndexBuffer(&iview);

          list->DrawIndexedInstanced(fmt.numVerts, 1, 0, fmt.baseVertex, 0);
        }
        else
        {
          list->DrawInstanced(fmt.numVerts, 1, 0, 0);
        }
      }
    }
  }

  MeshDisplayPipelines cache = CacheMeshDisplayPipelines(cfg.position, cfg.second);

  if(cfg.position.buf != ResourceId())
  {
    ID3D12Resource *vb =
        m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.buf);

    UINT64 offs = cfg.position.offset;
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vb->GetGPUVirtualAddress() + offs;
    view.StrideInBytes = cfg.position.stride;
    view.SizeInBytes = UINT(vb->GetDesc().Width - offs);
    list->IASetVertexBuffers(0, 1, &view);

    // set it to the secondary buffer too just as dummy info
    list->IASetVertexBuffers(1, 1, &view);

    list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(cfg.position.topo));

    if(PatchList_Count(cfg.position.topo) > 0)
      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
  }

  SolidShade solidShadeMode = cfg.solidShadeMode;

  // can't support secondary shading without a buffer - no pipeline will have been created
  if(solidShadeMode == SolidShade::Secondary && cfg.second.buf == ResourceId())
    solidShadeMode = SolidShade::NoSolid;

  if(solidShadeMode == SolidShade::Secondary)
  {
    ID3D12Resource *vb =
        m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.buf);

    UINT64 offs = cfg.second.offset;
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vb->GetGPUVirtualAddress() + offs;
    view.StrideInBytes = cfg.second.stride;
    view.SizeInBytes = UINT(vb->GetDesc().Width - offs);
    list->IASetVertexBuffers(1, 1, &view);
  }

  // solid render
  if(solidShadeMode != SolidShade::NoSolid && cfg.position.topo < Topology::PatchList)
  {
    ID3D12PipelineState *pipe = NULL;
    switch(solidShadeMode)
    {
      default:
      case SolidShade::Solid: pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]; break;
      case SolidShade::Lit: pipe = cache.pipes[MeshDisplayPipelines::ePipe_Lit]; break;
      case SolidShade::Secondary: pipe = cache.pipes[MeshDisplayPipelines::ePipe_Secondary]; break;
    }

    pixelData.OutputDisplayFormat = (int)cfg.solidShadeMode;
    if(cfg.solidShadeMode == SolidShade::Secondary && cfg.second.showAlpha)
      pixelData.OutputDisplayFormat = MESHDISPLAY_SECONDARY_ALPHA;
    pixelData.WireframeColour = Vec3f(0.8f, 0.8f, 0.0f);

    list->SetPipelineState(pipe);
    list->SetGraphicsRootSignature(m_CBOnlyRootSig);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, UploadConstants(&pixelData, sizeof(pixelData)));

    if(solidShadeMode == SolidShade::Lit)
    {
      DebugGeometryCBuffer geomData;
      geomData.InvProj = projMat.Inverse();

      list->SetGraphicsRootConstantBufferView(2, UploadConstants(&geomData, sizeof(geomData)));
    }
    else
    {
      list->SetGraphicsRootConstantBufferView(2, vsCB);
    }

    Vec4f colour(0.8f, 0.8f, 0.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

    if(cfg.position.idxByteWidth && cfg.position.idxbuf != ResourceId())
    {
      ID3D12Resource *ib =
          m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.idxbuf);

      D3D12_INDEX_BUFFER_VIEW view;
      view.BufferLocation = ib->GetGPUVirtualAddress() + cfg.position.idxoffs;
      view.SizeInBytes = UINT(ib->GetDesc().Width - cfg.position.idxoffs);
      view.Format = cfg.position.idxByteWidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
      list->IASetIndexBuffer(&view);

      list->DrawIndexedInstanced(cfg.position.numVerts, 1, 0, cfg.position.baseVertex, 0);
    }
    else
    {
      list->DrawInstanced(cfg.position.numVerts, 1, 0, 0);
    }
  }

  // wireframe render
  if(solidShadeMode == SolidShade::NoSolid || cfg.wireframeDraw ||
     cfg.position.topo >= Topology::PatchList)
  {
    Vec4f wireCol =
        Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y, cfg.position.meshColor.z, 1.0f);

    pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
    list->SetGraphicsRootSignature(m_CBOnlyRootSig);

    list->SetGraphicsRootConstantBufferView(0, vsCB);
    list->SetGraphicsRootConstantBufferView(1, UploadConstants(&pixelData, sizeof(pixelData)));
    list->SetGraphicsRootConstantBufferView(2, vsCB);

    list->SetGraphicsRoot32BitConstants(3, 4, &cfg.position.meshColor.x, 0);

    if(cfg.position.idxByteWidth && cfg.position.idxbuf != ResourceId())
    {
      ID3D12Resource *ib =
          m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(cfg.position.idxbuf);

      D3D12_INDEX_BUFFER_VIEW view;
      view.BufferLocation = ib->GetGPUVirtualAddress() + cfg.position.idxoffs;
      view.SizeInBytes = UINT(ib->GetDesc().Width - cfg.position.idxoffs);
      view.Format = cfg.position.idxByteWidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
      list->IASetIndexBuffer(&view);

      list->DrawIndexedInstanced(cfg.position.numVerts, 1, 0, cfg.position.baseVertex, 0);
    }
    else
    {
      list->DrawInstanced(cfg.position.numVerts, 1, 0, 0);
    }
  }

  MeshFormat helper;
  helper.idxByteWidth = 2;
  helper.topo = Topology::LineList;

  helper.specialFormat = SpecialFormat::Unknown;
  helper.compByteWidth = 4;
  helper.compCount = 4;
  helper.compType = CompType::Float;

  helper.stride = sizeof(Vec4f);

  pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;

  list->SetGraphicsRootConstantBufferView(1, UploadConstants(&pixelData, sizeof(pixelData)));

  // cache pipelines for use in drawing wireframe helpers
  cache = CacheMeshDisplayPipelines(helper, helper);

  if(cfg.showBBox)
  {
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

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = UploadConstants(bbox, sizeof(bbox));
    view.SizeInBytes = sizeof(bbox);
    view.StrideInBytes = sizeof(Vec4f);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    list->IASetVertexBuffers(0, 1, &view);

    Vec4f colour(0.2f, 0.2f, 1.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);

    list->DrawInstanced(24, 1, 0, 0);
  }

  // draw axis helpers
  if(!cfg.position.unproject)
  {
    Vec4f axismarker[6] = {
        Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
        Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f),
    };

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = UploadConstants(axismarker, sizeof(axismarker));
    view.SizeInBytes = sizeof(axismarker);
    view.StrideInBytes = sizeof(Vec4f);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    list->IASetVertexBuffers(0, 1, &view);

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Wire]);

    Vec4f colour(1.0f, 0.0f, 0.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);
    list->DrawInstanced(2, 1, 0, 0);

    colour = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);
    list->DrawInstanced(2, 1, 2, 0);

    colour = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);
    list->DrawInstanced(2, 1, 4, 0);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
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
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = UploadConstants(bbox, sizeof(bbox));
    view.SizeInBytes = sizeof(bbox);
    view.StrideInBytes = sizeof(Vec4f);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    list->IASetVertexBuffers(0, 1, &view);

    Vec4f colour(1.0f, 1.0f, 1.0f, 1.0f);
    list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

    list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Wire]);

    list->DrawInstanced(24, 1, 0, 0);
  }

  // show highlighted vertex
  if(cfg.highlightVert != ~0U)
  {
    m_HighlightCache.CacheHighlightingData(eventID, cfg);

    Topology meshtopo = cfg.position.topo;

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

    helper.topo = Topology::TriangleList;
    uint32_t primSize = 3;    // number of verts per primitive

    if(meshtopo == Topology::LineList || meshtopo == Topology::LineStrip ||
       meshtopo == Topology::LineList_Adj || meshtopo == Topology::LineStrip_Adj)
    {
      primSize = 2;
      helper.topo = Topology::LineList;
    }
    else
    {
      // update the cache, as it's currently linelist
      helper.topo = Topology::TriangleList;
      cache = CacheMeshDisplayPipelines(helper, helper);
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

      list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(helper.topo));

      if(PatchList_Count(helper.topo) > 0)
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

      list->SetGraphicsRootConstantBufferView(0, UploadConstants(&vertexData, sizeof(vertexData)));

      list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Solid]);

      ////////////////////////////////////////////////////////////////
      // render primitives

      // Draw active primitive (red)
      Vec4f colour(1.0f, 0.0f, 0.0f, 1.0f);
      list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

      D3D12_VERTEX_BUFFER_VIEW view = {};
      view.StrideInBytes = sizeof(Vec4f);

      if(activePrim.size() >= primSize)
      {
        view.BufferLocation = UploadConstants(&activePrim[0], sizeof(Vec4f) * primSize);
        view.SizeInBytes = sizeof(Vec4f) * primSize;

        list->IASetVertexBuffers(0, 1, &view);

        list->DrawInstanced(primSize, 1, 0, 0);
      }

      // Draw adjacent primitives (green)
      colour = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        view.BufferLocation =
            UploadConstants(&activePrim[0], sizeof(Vec4f) * adjacentPrimVertices.size());
        view.SizeInBytes = UINT(sizeof(Vec4f) * adjacentPrimVertices.size());

        list->IASetVertexBuffers(0, 1, &view);

        list->DrawInstanced((UINT)adjacentPrimVertices.size(), 1, 0, 0);
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots

      float scale = 800.0f / viewport.Height;
      float asp = viewport.Width / viewport.Height;

      vertexData.SpriteSize = Vec2f(scale / asp, scale);

      list->SetGraphicsRootConstantBufferView(0, UploadConstants(&vertexData, sizeof(vertexData)));

      // Draw active vertex (blue)
      colour = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

      // vertices are drawn with tri strips
      helper.topo = Topology::TriangleStrip;
      cache = CacheMeshDisplayPipelines(helper, helper);

      FloatVector vertSprite[4] = {
          activeVertex, activeVertex, activeVertex, activeVertex,
      };

      list->IASetPrimitiveTopology(MakeD3DPrimitiveTopology(helper.topo));

      if(PatchList_Count(helper.topo) > 0)
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

      list->SetPipelineState(cache.pipes[MeshDisplayPipelines::ePipe_Solid]);

      {
        view.BufferLocation = UploadConstants(&vertSprite[0], sizeof(vertSprite));
        view.SizeInBytes = sizeof(vertSprite);

        list->IASetVertexBuffers(0, 1, &view);

        list->DrawInstanced(4, 1, 0, 0);
      }

      // Draw inactive vertices (green)
      colour = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      list->SetGraphicsRoot32BitConstants(3, 4, &colour.x, 0);

      if(!inactiveVertices.empty())
      {
        std::vector<FloatVector> inactiveVB;
        inactiveVB.reserve(inactiveVertices.size() * 4);

        for(size_t i = 0; i < inactiveVertices.size(); i++)
        {
          inactiveVB.push_back(inactiveVertices[i]);
          inactiveVB.push_back(inactiveVertices[i]);
          inactiveVB.push_back(inactiveVertices[i]);
          inactiveVB.push_back(inactiveVertices[i]);
        }

        view.BufferLocation =
            UploadConstants(&inactiveVB[0], sizeof(vertSprite) * inactiveVertices.size());
        view.SizeInBytes = UINT(sizeof(vertSprite) * inactiveVertices.size());

        for(size_t i = 0; i < inactiveVertices.size(); i++)
        {
          list->IASetVertexBuffers(0, 1, &view);

          list->DrawInstanced(4, 1, 0, 0);

          view.BufferLocation += sizeof(FloatVector) * 4;
        }
      }
    }
  }

  list->Close();

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_WrappedDevice->ExecuteLists();
  m_WrappedDevice->FlushLists();
#endif
}

void D3D12DebugManager::PrepareTextureSampling(ID3D12Resource *resource, CompType typeHint,
                                               int &resType, vector<D3D12_RESOURCE_BARRIER> &barriers)
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
    if((states[i] & realResourceState) == realResourceState)
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

      m_TexResource->SetName(L"m_TexResource");
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

struct D3D12QuadOverdrawCallback : public D3D12DrawcallCallback
{
  D3D12QuadOverdrawCallback(WrappedID3D12Device *dev, const vector<uint32_t> &events,
                            PortableHandle uav)
      : m_pDevice(dev), m_pDebug(dev->GetDebugManager()), m_Events(events), m_UAV(uav)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = this;
  }
  ~D3D12QuadOverdrawCallback()
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = NULL;
  }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return;

    // we customise the pipeline to disable framebuffer writes, but perform normal testing
    // and substitute our quad calculation fragment shader that writes to a storage image
    // that is bound in a new root signature element.

    D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
    m_PrevState = rs;

    // check cache first
    CachedPipeline cache = m_PipelineCache[rs.pipe];

    // if we don't get a hit, create a modified pipeline
    if(cache.pipe == NULL)
    {
      HRESULT hr = S_OK;

      WrappedID3D12RootSignature *sig =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
              rs.graphics.rootsig);

      // need to be able to add a descriptor table with our UAV without hitting the 64 DWORD limit
      RDCASSERT(sig->sig.dwordLength < 64);

      D3D12RootSignature modsig = sig->sig;

      // make sure no other UAV tables overlap. We can't remove elements entirely because then the
      // root signature indices wouldn't match up as expected.
      // Instead move them into an unused space.
      for(size_t i = 0; i < modsig.params.size(); i++)
      {
        if(modsig.params[i].ShaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL)
        {
          if(modsig.params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
          {
            modsig.params[i].Descriptor.RegisterSpace = modsig.numSpaces;
          }
          else if(modsig.params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
          {
            for(size_t r = 0; r < modsig.params[i].ranges.size(); r++)
            {
              modsig.params[i].ranges[r].RegisterSpace = modsig.numSpaces;
            }
          }
        }
      }

      D3D12_DESCRIPTOR_RANGE1 range;
      range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      range.NumDescriptors = 1;
      range.BaseShaderRegister = 0;
      range.RegisterSpace = 0;
      range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
      range.OffsetInDescriptorsFromTableStart = 0;

      modsig.params.push_back(D3D12RootSignatureParameter());
      D3D12RootSignatureParameter &param = modsig.params.back();
      param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
      param.DescriptorTable.NumDescriptorRanges = 1;
      param.DescriptorTable.pDescriptorRanges = &range;

      cache.sigElem = uint32_t(modsig.params.size() - 1);

      std::vector<D3D12_ROOT_PARAMETER1> params;
      params.resize(modsig.params.size());
      for(size_t i = 0; i < params.size(); i++)
        params[i] = modsig.params[i];

      ID3DBlob *root = m_pDebug->MakeRootSig(modsig);

      hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                          __uuidof(ID3D12RootSignature), (void **)&cache.sig);
      RDCASSERTEQUAL(hr, S_OK);

      SAFE_RELEASE(root);

      WrappedID3D12PipelineState *origPSO =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

      RDCASSERT(origPSO->IsGraphics());

      D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = origPSO->GetGraphicsDesc();

      for(size_t i = 0; i < ARRAY_COUNT(pipeDesc.BlendState.RenderTarget); i++)
        pipeDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;

      // disable depth/stencil writes
      pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pipeDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pipeDesc.DepthStencilState.StencilWriteMask = 0;

      pipeDesc.PS.BytecodeLength = m_pDebug->GetOverdrawWritePS()->GetBufferSize();
      pipeDesc.PS.pShaderBytecode = m_pDebug->GetOverdrawWritePS()->GetBufferPointer();

      pipeDesc.pRootSignature = cache.sig;

      hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                  (void **)&cache.pipe);
      RDCASSERTEQUAL(hr, S_OK);

      m_PipelineCache[rs.pipe] = cache;
    }

    // modify state for first draw call
    rs.pipe = GetResID(cache.pipe);
    rs.graphics.rootsig = GetResID(cache.sig);

    if(rs.graphics.sigelems.size() <= cache.sigElem)
      rs.graphics.sigelems.resize(cache.sigElem + 1);

    PortableHandle uav = m_UAV;

    // if a CBV_SRV_UAV heap is already set, we need to copy our descriptor in
    // if we haven't already. Otherwise we can set our own heap.
    for(size_t i = 0; i < rs.heaps.size(); i++)
    {
      WrappedID3D12DescriptorHeap *h =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(rs.heaps[i]);
      if(h->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
      {
        // use the last descriptor
        D3D12_CPU_DESCRIPTOR_HANDLE dst = h->GetCPUDescriptorHandleForHeapStart();
        dst.ptr += (h->GetDesc().NumDescriptors - 1) * sizeof(D3D12Descriptor);

        if(m_CopiedHeaps.find(rs.heaps[i]) == m_CopiedHeaps.end())
        {
          WrappedID3D12DescriptorHeap *h2 =
              m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(m_UAV.heap);
          D3D12_CPU_DESCRIPTOR_HANDLE src = h2->GetCPUDescriptorHandleForHeapStart();
          src.ptr += m_UAV.index * sizeof(D3D12Descriptor);

          // can't do a copy because the src heap is CPU write-only (shader visible). So instead,
          // create directly
          D3D12Descriptor *srcDesc = (D3D12Descriptor *)src.ptr;
          srcDesc->Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_pDevice, dst);

          m_CopiedHeaps.insert(rs.heaps[i]);
        }

        uav = ToPortableHandle(dst);

        break;
      }
    }

    if(uav.heap == m_UAV.heap)
      rs.heaps.push_back(m_UAV.heap);

    rs.graphics.sigelems[cache.sigElem] =
        D3D12RenderState::SignatureElement(eRootTable, uav.heap, uav.index);

    // as we're changing the root signature, we need to reapply all elements,
    // so just apply all state
    if(cmd)
      rs.ApplyState(cmd);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return false;

    // restore the render state and go ahead with the real draw
    m_pDevice->GetQueue()->GetCommandData()->m_RenderState = m_PrevState;

    RDCASSERT(cmd);
    m_pDevice->GetQueue()->GetCommandData()->m_RenderState.ApplyState(cmd);

    return true;
  }

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    // nothing to do
  }

  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) {}
  bool RecordAllCmds() { return false; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // don't care
  }

  WrappedID3D12Device *m_pDevice;
  D3D12DebugManager *m_pDebug;
  const vector<uint32_t> &m_Events;
  PortableHandle m_UAV;

  // cache modified pipelines
  struct CachedPipeline
  {
    ID3D12RootSignature *sig;
    uint32_t sigElem;
    ID3D12PipelineState *pipe;
  };
  map<ResourceId, CachedPipeline> m_PipelineCache;
  std::set<ResourceId> m_CopiedHeaps;
  D3D12RenderState m_PrevState;
};

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
  disp.Red = disp.Green = disp.Blue = disp.Alpha = true;
  disp.FlipY = false;
  disp.offx = 0.0f;
  disp.offy = 0.0f;
  disp.CustomShader = shader;
  disp.texid = texid;
  disp.typeHint = typeHint;
  disp.lightBackgroundColor = disp.darkBackgroundColor = FloatVector(0, 0, 0, 0);
  disp.HDRMul = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.mip = mip;
  disp.sampleIdx = sampleIdx;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangemin = 0.0f;
  disp.rangemax = 1.0f;
  disp.rawoutput = false;
  disp.scale = 1.0f;
  disp.sliceFace = arrayIdx;

  SetOutputDimensions(RDCMAX(1U, (UINT)resDesc.Width >> mip), RDCMAX(1U, resDesc.Height >> mip),
                      resDesc.Format);

  RenderTextureInternal(GetCPUHandle(CUSTOM_SHADER_RTV), disp, true);

  return m_CustomShaderResourceId;
}

ResourceId D3D12DebugManager::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                            uint32_t eventID, const vector<uint32_t> &passEvents)
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

    customRenderTex->SetName(L"customRenderTex");

    m_OverlayRenderTex = wrappedCustomRenderTex;
    m_OverlayResourceId = wrappedCustomRenderTex->GetResourceID();
  }

  D3D12RenderState &rs = m_WrappedDevice->GetQueue()->GetCommandData()->m_RenderState;

  ID3D12Resource *renderDepth = NULL;

  D3D12Descriptor *dsView =
      DescriptorFromPortableHandle(m_WrappedDevice->GetResourceManager(), rs.dsv);

  D3D12_RESOURCE_DESC depthTexDesc = {};
  D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc = {};
  if(dsView)
  {
    ID3D12Resource *realDepth = dsView->nonsamp.resource;

    dsViewDesc = dsView->nonsamp.dsv;

    depthTexDesc = realDepth->GetDesc();
    depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    depthTexDesc.Alignment = 0;

    HRESULT hr = S_OK;

    hr = m_WrappedDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthTexDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                  __uuidof(ID3D12Resource), (void **)&renderDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth %08x", hr);
      return m_OverlayResourceId;
    }

    renderDepth->SetName(L"Overlay renderDepth");

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

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
  }
  else if(overlay == DebugOverlay::Drawcall)
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
      RDCEraseEl(psoDesc.RTVFormats);
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
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
  else if(overlay == DebugOverlay::BackfaceCull)
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
      RDCEraseEl(psoDesc.RTVFormats);
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
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
  else if(overlay == DebugOverlay::Wireframe)
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
      RDCEraseEl(psoDesc.RTVFormats);
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
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
  else if(overlay == DebugOverlay::ClearBeforePass || overlay == DebugOverlay::ClearBeforeDraw)
  {
    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventID);

    if(!events.empty())
    {
      list->Close();
      list = NULL;

      bool rtSingle = rs.rtSingle;
      vector<PortableHandle> rts = rs.rts;

      if(overlay == DebugOverlay::ClearBeforePass)
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

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_WrappedDevice->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == DebugOverlay::ViewportScissor)
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

      D3D12_GPU_VIRTUAL_ADDRESS viewCB = UploadConstants(&pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(0, viewCB);
      list->SetGraphicsRootConstantBufferView(1, viewCB);
      list->SetGraphicsRootConstantBufferView(2, viewCB);

      Vec4f dummy;
      list->SetGraphicsRoot32BitConstants(3, 4, &dummy.x, 0);

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

      D3D12_GPU_VIRTUAL_ADDRESS scissorCB = UploadConstants(&pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(1, scissorCB);

      list->DrawInstanced(3, 1, 0, 0);
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    if(pipe && pipe->IsGraphics())
    {
      SCOPED_TIMER("Triangle size");

      vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::TriangleSizeDraw)
        events.clear();

      while(!events.empty())
      {
        const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(events[0]);

        // remove any non-drawcalls, like the pass boundary.
        if(!(draw->flags & DrawFlags::Drawcall))
          events.erase(events.begin());
        else
          break;
      }

      events.push_back(eventID);

      D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = pipe->GetGraphicsDesc();
      pipeDesc.pRootSignature = m_CBOnlyRootSig;
      pipeDesc.SampleMask = 0xFFFFFFFF;
      pipeDesc.SampleDesc.Count = 1;
      pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

      pipeDesc.NumRenderTargets = 1;
      RDCEraseEl(pipeDesc.RTVFormats);
      pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      D3D12_INPUT_ELEMENT_DESC ia[2] = {};
      ia[0].SemanticName = "pos";
      ia[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      ia[1].SemanticName = "sec";
      ia[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      ia[1].InputSlot = 1;
      ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

      pipeDesc.InputLayout.NumElements = 2;
      pipeDesc.InputLayout.pInputElementDescs = ia;

      pipeDesc.VS.BytecodeLength = m_MeshVS->GetBufferSize();
      pipeDesc.VS.pShaderBytecode = m_MeshVS->GetBufferPointer();
      RDCEraseEl(pipeDesc.HS);
      RDCEraseEl(pipeDesc.DS);
      pipeDesc.GS.BytecodeLength = m_TriangleSizeGS->GetBufferSize();
      pipeDesc.GS.pShaderBytecode = m_TriangleSizeGS->GetBufferPointer();
      pipeDesc.PS.BytecodeLength = m_TriangleSizePS->GetBufferSize();
      pipeDesc.PS.pShaderBytecode = m_TriangleSizePS->GetBufferPointer();

      pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

      if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_GREATER)
        pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
      if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_LESS)
        pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

      // enough for all primitive topology types
      ID3D12PipelineState *pipes[D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH + 1] = {};

      DebugVertexCBuffer vertexData = {};
      vertexData.LineStrip = 0;
      vertexData.ModelViewProj = Matrix4f::Identity();
      vertexData.SpriteSize = Vec2f();

      Vec4f viewport(rs.views[0].Width, rs.views[0].Height);

      if(rs.dsv.heap != ResourceId())
      {
        WrappedID3D12DescriptorHeap *realDSVHeap =
            m_WrappedDevice->GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(rs.dsv.heap);

        D3D12_CPU_DESCRIPTOR_HANDLE realDSV = realDSVHeap->GetCPUDescriptorHandleForHeapStart();
        realDSV.ptr += sizeof(D3D12Descriptor) * rs.dsv.index;

        list->OMSetRenderTargets(1, &rtv, TRUE, &realDSV);
      }

      list->RSSetViewports(1, &rs.views[0]);

      D3D12_RECT scissor = {0, 0, 16384, 16384};
      list->RSSetScissorRects(1, &scissor);

      list->SetGraphicsRootSignature(m_CBOnlyRootSig);

      list->SetGraphicsRootConstantBufferView(0, UploadConstants(&vertexData, sizeof(vertexData)));
      list->SetGraphicsRootConstantBufferView(
          1, UploadConstants(&overdrawRamp[0].x, sizeof(overdrawRamp)));
      list->SetGraphicsRootConstantBufferView(2, UploadConstants(&viewport, sizeof(viewport)));
      list->SetGraphicsRoot32BitConstants(3, 4, &viewport.x, 0);

      for(size_t i = 0; i < events.size(); i++)
      {
        const DrawcallDescription *draw = m_WrappedDevice->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::GSOut);
          if(fmt.buf == ResourceId())
            fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::VSOut);

          if(fmt.buf != ResourceId())
          {
            D3D_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(fmt.topo);

            if(topo == D3D_PRIMITIVE_TOPOLOGY_POINTLIST ||
               topo >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            else if(topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            else
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            list->IASetPrimitiveTopology(topo);

            if(pipes[pipeDesc.PrimitiveTopologyType] == NULL)
            {
              HRESULT hr = m_WrappedDevice->CreateGraphicsPipelineState(
                  &pipeDesc, __uuidof(ID3D12PipelineState),
                  (void **)&pipes[pipeDesc.PrimitiveTopologyType]);
              RDCASSERTEQUAL(hr, S_OK);
            }

            ID3D12Resource *vb =
                m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.buf);

            D3D12_VERTEX_BUFFER_VIEW vbView = {};
            vbView.BufferLocation = vb->GetGPUVirtualAddress() + fmt.offset;
            vbView.StrideInBytes = fmt.stride;
            vbView.SizeInBytes = UINT(vb->GetDesc().Width - fmt.offset);

            // second bind is just a dummy, so we don't have to make a shader
            // that doesn't accept the secondary stream
            list->IASetVertexBuffers(0, 1, &vbView);
            list->IASetVertexBuffers(1, 1, &vbView);

            list->SetPipelineState(pipes[pipeDesc.PrimitiveTopologyType]);

            if(fmt.idxByteWidth && fmt.idxbuf != ResourceId())
            {
              ID3D12Resource *ib =
                  m_WrappedDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.idxbuf);

              D3D12_INDEX_BUFFER_VIEW view;
              view.BufferLocation = ib->GetGPUVirtualAddress() + fmt.idxoffs;
              view.SizeInBytes = UINT(ib->GetDesc().Width - fmt.idxoffs);
              view.Format = fmt.idxByteWidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
              list->IASetIndexBuffer(&view);

              list->DrawIndexedInstanced(fmt.numVerts, 1, 0, fmt.baseVertex, 0);
            }
            else
            {
              list->DrawInstanced(fmt.numVerts, 1, 0, 0);
            }
          }
        }
      }

      list->Close();
      list = NULL;

      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();

      for(size_t i = 0; i < ARRAY_COUNT(pipes); i++)
        SAFE_RELEASE(pipes[i]);
    }

    // restore back to normal
    m_WrappedDevice->ReplayLog(0, eventID, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    SCOPED_TIMER("Quad Overdraw");

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::QuadOverdrawDraw)
      events.clear();

    events.push_back(eventID);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::QuadOverdrawPass)
      {
        list->Close();
        m_WrappedDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);
        list = m_WrappedDevice->GetNewList();
      }

      uint32_t width = uint32_t(resourceDesc.Width >> 1);
      uint32_t height = resourceDesc.Height >> 1;

      width = RDCMAX(1U, width);
      height = RDCMAX(1U, height);

      D3D12_RESOURCE_DESC uavTexDesc = {};
      uavTexDesc.Alignment = 0;
      uavTexDesc.DepthOrArraySize = 4;
      uavTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      uavTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      uavTexDesc.Format = DXGI_FORMAT_R32_UINT;
      uavTexDesc.Height = height;
      uavTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      uavTexDesc.MipLevels = 1;
      uavTexDesc.SampleDesc.Count = 1;
      uavTexDesc.SampleDesc.Quality = 0;
      uavTexDesc.Width = width;

      ID3D12Resource *overdrawTex = NULL;
      HRESULT hr = m_WrappedDevice->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &uavTexDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
          NULL, __uuidof(ID3D12Resource), (void **)&overdrawTex);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overdrawTex %08x", hr);
        list->Close();
        list = NULL;
        return m_OverlayResourceId;
      }

      m_WrappedDevice->CreateShaderResourceView(overdrawTex, NULL, GetCPUHandle(OVERDRAW_SRV));
      m_WrappedDevice->CreateUnorderedAccessView(overdrawTex, NULL, NULL, GetCPUHandle(OVERDRAW_UAV));
      m_WrappedDevice->CreateUnorderedAccessView(overdrawTex, NULL, NULL,
                                                 GetUAVClearHandle(OVERDRAW_UAV));

      UINT zeroes[4] = {0, 0, 0, 0};
      list->ClearUnorderedAccessViewUint(
          GetGPUHandle(OVERDRAW_UAV), GetUAVClearHandle(OVERDRAW_UAV), overdrawTex, zeroes, 0, NULL);
      list->Close();
      list = NULL;

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();
#endif

      m_WrappedDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      // declare callback struct here
      D3D12QuadOverdrawCallback cb(m_WrappedDevice, events,
                                   ToPortableHandle(GetCPUHandle(OVERDRAW_UAV)));

      m_WrappedDevice->ReplayLog(events.front(), events.back(), eReplay_Full);

      // resolve pass
      {
        list = m_WrappedDevice->GetNewList();

        D3D12_RESOURCE_BARRIER overdrawBarriers[2] = {};

        // make sure UAV work is done then prepare for reading in PS
        overdrawBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        overdrawBarriers[0].UAV.pResource = overdrawTex;
        overdrawBarriers[1].Transition.pResource = overdrawTex;
        overdrawBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        overdrawBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        overdrawBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // prepare tex resource for copying
        list->ResourceBarrier(2, overdrawBarriers);

        list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

        list->RSSetViewports(1, &rs.views[0]);

        D3D12_RECT scissor = {0, 0, 16384, 16384};
        list->RSSetScissorRects(1, &scissor);

        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        list->SetPipelineState(m_QuadResolvePipe);

        list->SetGraphicsRootSignature(m_QuadResolveRootSig);

        list->SetDescriptorHeaps(1, &cbvsrvuavHeap);

        list->SetGraphicsRootConstantBufferView(
            0, UploadConstants(&overdrawRamp[0].x, sizeof(overdrawRamp)));
        list->SetGraphicsRootDescriptorTable(1, GetGPUHandle(OVERDRAW_SRV));

        list->DrawInstanced(3, 1, 0, 0);

        list->Close();
        list = NULL;
      }

      m_WrappedDevice->ExecuteLists();
      m_WrappedDevice->FlushLists();

      for(auto it = cb.m_PipelineCache.begin(); it != cb.m_PipelineCache.end(); ++it)
      {
        SAFE_RELEASE(it->second.pipe);
        SAFE_RELEASE(it->second.sig);
      }

      SAFE_RELEASE(overdrawTex);
    }

    if(overlay == DebugOverlay::QuadOverdrawPass)
      m_WrappedDevice->ReplayLog(0, eventID, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
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

      if(overlay == DebugOverlay::Depth)
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

      RDCEraseEl(psoDesc.RTVFormats);
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_UNORM;
      psoDesc.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
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

      list->Close();
      list = NULL;

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
  ID3D12Resource *resource = WrappedID3D12Resource::GetList()[cfg.texid];

  if(resource == NULL)
    return false;

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

  if(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    pixelData.Slice = float(cfg.sliceFace);

  vector<D3D12_RESOURCE_BARRIER> barriers;
  int resType = 0;
  PrepareTextureSampling(resource, cfg.typeHint, resType, barriers);

  pixelData.OutputDisplayFormat = resType;

  if(cfg.overlay == DebugOverlay::NaN)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;

  if(cfg.overlay == DebugOverlay::Clipping)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;

  if(IsUIntFormat(resourceDesc.Format))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
  else if(IsIntFormat(resourceDesc.Format))
    pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;

  if(!IsSRGBFormat(resourceDesc.Format) && cfg.linearDisplayAsGamma)
    pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

  ID3D12PipelineState *customPSO = NULL;

  D3D12_GPU_VIRTUAL_ADDRESS psCBuf = 0;

  if(cfg.CustomShader != ResourceId())
  {
    WrappedID3D12Shader *shader =
        m_WrappedDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(cfg.CustomShader);

    if(shader == NULL)
      return false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.pRootSignature = m_TexDisplayRootSig;
    pipeDesc.VS.BytecodeLength = m_GenericVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = m_GenericVS->GetBufferPointer();
    pipeDesc.PS = shader->GetDesc();
    pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeDesc.SampleMask = 0xFFFFFFFF;
    pipeDesc.SampleDesc.Count = 1;
    pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeDesc.NumRenderTargets = 1;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = m_WrappedDevice->CreateGraphicsPipelineState(
        &pipeDesc, __uuidof(ID3D12PipelineState), (void **)&customPSO);
    if(FAILED(hr))
      return false;

    DXBC::DXBCFile *dxbc = shader->GetDXBC();

    RDCASSERT(dxbc);
    RDCASSERT(dxbc->m_Type == D3D11_ShaderType_Pixel);

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

              d[0] = (uint32_t)resourceDesc.Width;
              d[1] = resourceDesc.Height;
              d[2] = resourceDesc.DepthOrArraySize;
              d[3] = resourceDesc.MipLevels;
              if(resourceDesc.MipLevels == 0)
                d[3] = CalcNumMips(
                    d[1], d[2],
                    resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? d[3] : 1);
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

              d[0] = resType;
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

        psCBuf = UploadConstants(cbufData, cbuf.descriptor.byteSize);

        SAFE_DELETE_ARRAY(cbufData);
      }
    }
  }
  else
  {
    psCBuf = UploadConstants(&pixelData, sizeof(pixelData));
  }

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

    if(customPSO)
    {
      list->SetPipelineState(customPSO);
    }
    else if(cfg.rawoutput || !blendAlpha || cfg.CustomShader != ResourceId())
    {
      if(m_BBFmtIdx == RGBA32_BACKBUFFER)
        list->SetPipelineState(m_TexDisplayF32Pipe);
      else if(m_BBFmtIdx == RGBA8_BACKBUFFER)
        list->SetPipelineState(m_TexDisplayLinearPipe);
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

    list->SetGraphicsRootConstantBufferView(0, UploadConstants(&vertexData, sizeof(vertexData)));
    list->SetGraphicsRootConstantBufferView(1, psCBuf);
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

    SAFE_RELEASE(customPSO);
  }

  return true;
}
