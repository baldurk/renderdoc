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

#include "d3d12_debug.h"
#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_bytecode.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_replay.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

inline static D3D12_ROOT_PARAMETER1 cbvParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg)
{
  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  ret.Descriptor.RegisterSpace = space;
  ret.Descriptor.ShaderRegister = reg;
  ret.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  return ret;
}

inline static D3D12_ROOT_PARAMETER1 srvParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg)
{
  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
  ret.Descriptor.RegisterSpace = space;
  ret.Descriptor.ShaderRegister = reg;
  ret.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  return ret;
}

inline static D3D12_ROOT_PARAMETER1 uavParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg)
{
  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  ret.Descriptor.RegisterSpace = space;
  ret.Descriptor.ShaderRegister = reg;
  ret.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  return ret;
}

inline static D3D12_ROOT_PARAMETER1 constParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg,
                                               UINT num)
{
  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  ret.Constants.RegisterSpace = space;
  ret.Constants.ShaderRegister = reg;
  ret.Constants.Num32BitValues = num;

  return ret;
}

inline static D3D12_ROOT_PARAMETER1 tableParam(D3D12_SHADER_VISIBILITY vis,
                                               D3D12_DESCRIPTOR_RANGE_TYPE type, UINT space,
                                               UINT basereg, UINT numreg)
{
  // this is a super hack but avoids the need to be clumsy with allocation of these structs
  static D3D12_DESCRIPTOR_RANGE1 ranges[32] = {};
  static int rangeIdx = 0;

  D3D12_DESCRIPTOR_RANGE1 &range = ranges[rangeIdx];
  rangeIdx = (rangeIdx + 1) % ARRAY_COUNT(ranges);

  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  ret.DescriptorTable.NumDescriptorRanges = 1;
  ret.DescriptorTable.pDescriptorRanges = &range;

  RDCEraseEl(range);

  range.RangeType = type;
  range.RegisterSpace = space;
  range.BaseShaderRegister = basereg;
  range.NumDescriptors = numreg;
  range.OffsetInDescriptorsFromTableStart = 0;
  if(type != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
  else
    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

  return ret;
}

D3D12DebugManager::D3D12DebugManager(WrappedID3D12Device *wrapper)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(D3D12DebugManager));

  m_pDevice = wrapper;

  D3D12ResourceManager *rm = wrapper->GetResourceManager();

  HRESULT hr = S_OK;

  const uint32_t rtvCount = 1024;

  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 1;
  desc.NumDescriptors = rtvCount;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

  RDCCOMPILE_ASSERT(LAST_WIN_RTV < rtvCount, "Increase size of RTV heap");

  hr = m_pDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&rtvHeap);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create RTV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(rtvHeap);

  const uint32_t dsvCount = 80;

  desc.NumDescriptors = dsvCount;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

  RDCCOMPILE_ASSERT(LAST_WIN_DSV < dsvCount, "Increase size of DSV heap");

  hr = m_pDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&dsvHeap);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create DSV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(dsvHeap);

  desc.NumDescriptors = 4096;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

  RDCCOMPILE_ASSERT(MAX_SRV_SLOT < 4096, "Increase size of CBV/SRV/UAV heap");

  hr = m_pDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&uavClearHeap);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(uavClearHeap);

  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  hr = m_pDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                       (void **)&cbvsrvuavHeap);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create CBV/SRV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(cbvsrvuavHeap);

  desc.NumDescriptors = 16;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

  hr = m_pDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&samplerHeap);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create sampler descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(samplerHeap);

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
  sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

  m_pDevice->CreateSampler(&sampDesc, samp);

  sampDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;

  samp.ptr += sizeof(D3D12Descriptor);
  m_pDevice->CreateSampler(&sampDesc, samp);

  static const UINT64 bufsize = 2 * 1024 * 1024;

  m_RingConstantBuffer = MakeCBuffer(bufsize);
  m_RingConstantOffset = 0;
  m_pDevice->InternalRef();

  D3D12ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  shaderCache->SetCaching(true);

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        // cbuffer
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
        // normal SRVs (2x, 4x, 8x, 16x, 32x)
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, 5),
        // stencil SRVs (2x, 4x, 8x, 16x, 32x)
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 11, 5),
    });

    RDCASSERT(root);

    hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                        __uuidof(ID3D12RootSignature), (void **)&m_ArrayMSAARootSig);
    m_pDevice->InternalRef();

    SAFE_RELEASE(root);

    rm->SetInternalResource(m_ArrayMSAARootSig);
  }

  {
    ID3DBlob *root = shaderCache->MakeRootSig(
        {
            cbvParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0),
            cbvParam(D3D12_SHADER_VISIBILITY_GEOMETRY, 0, 0),
            // 'push constant' CBV
            constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 4),
            // meshlet sizes SRV
            srvParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    RDCASSERT(root);

    hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                        __uuidof(ID3D12RootSignature), (void **)&m_MeshRootSig);
    m_pDevice->InternalRef();

    SAFE_RELEASE(root);

    rm->SetInternalResource(m_MeshRootSig);
  }

  if(!CreateShaderDebugResources())
  {
    RDCERR("Failed to create resources for shader debugging math intrinsics");
    SAFE_RELEASE(m_ShaderDebugRootSig);
    SAFE_RELEASE(m_MathIntrinsicsPso);
    SAFE_RELEASE(m_DXILMathIntrinsicsPso);
    SAFE_RELEASE(m_ShaderDebugResultBuffer);
  }

  {
    rdcstr meshhlsl = GetEmbeddedResource(mesh_hlsl);

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               {}, "vs_5_0", &m_MeshVS);
    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshGS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               {}, "gs_5_0", &m_MeshGS);
    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               {}, "ps_5_0", &m_MeshPS);
  }

  {
    D3D12_RESOURCE_DESC meshletSizeBuf = {};
    meshletSizeBuf.Alignment = 0;
    meshletSizeBuf.DepthOrArraySize = 1;
    meshletSizeBuf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    meshletSizeBuf.Flags = D3D12_RESOURCE_FLAG_NONE;
    meshletSizeBuf.Format = DXGI_FORMAT_UNKNOWN;
    meshletSizeBuf.Height = 1;
    meshletSizeBuf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    meshletSizeBuf.MipLevels = 1;
    meshletSizeBuf.SampleDesc.Count = 1;
    meshletSizeBuf.SampleDesc.Quality = 0;
    meshletSizeBuf.Width = sizeof(uint32_t) * (4 + MAX_NUM_MESHLETS);

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &meshletSizeBuf,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                            __uuidof(ID3D12Resource), (void **)&m_MeshletBuf);
    m_pDevice->InternalRef();

    if(FAILED(hr))
    {
      RDCERR("Failed to create meshlet size buffer, HRESULT: %s", ToStr(hr).c_str());
    }

    if(m_MeshletBuf)
      m_MeshletBuf->SetName(L"m_MeshletBuf");
  }

  {
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0", &m_FullscreenVS);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DiscardFloatPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_DiscardFloatPS);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DiscardIntPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_DiscardIntPS);
  }

  {
    rdcstr multisamplehlsl = GetEmbeddedResource(multisample_hlsl);

    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_CopyMSToArray",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_IntMS2Array);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyMSToArray",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_FloatMS2Array);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyMSToArray",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_DepthMS2Array);

    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_CopyArrayToMS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_IntArray2MS);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyArrayToMS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_FloatArray2MS);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyArrayToMS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &m_DepthArray2MS);
  }

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        cbvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 5),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 10),
    });

    RDCASSERT(root);

    hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                        __uuidof(ID3D12RootSignature),
                                        (void **)&m_PixelHistoryCopySig);
    m_pDevice->InternalRef();

    SAFE_RELEASE(root);

    rm->SetInternalResource(m_PixelHistoryCopySig);

    rdcstr hlsl = GetEmbeddedResource(d3d12_pixelhistory_hlsl);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PixelHistoryCopyPixel",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &m_PixelHistoryCopyCS);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.CS.pShaderBytecode = m_PixelHistoryCopyCS->GetBufferPointer();
    pipeDesc.CS.BytecodeLength = m_PixelHistoryCopyCS->GetBufferSize();
    pipeDesc.pRootSignature = m_PixelHistoryCopySig;
    hr = m_pDevice->CreateComputePipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                               (void **)&m_PixelHistoryCopyPso);
    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for pixel history HRESULT: %s", ToStr(hr).c_str());
      return;
    }
    m_pDevice->GetResourceManager()->SetInternalResource(m_PixelHistoryCopyPso);
  }

  shaderCache->SetCaching(false);

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

  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_ReadbackBuffer);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback buffer, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  m_ReadbackBuffer->SetName(L"m_ReadbackBuffer");

  rm->SetInternalResource(m_ReadbackBuffer);

  hr = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         __uuidof(ID3D12CommandAllocator), (void **)&m_DebugAlloc);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback command allocator, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  rm->SetInternalResource(m_DebugAlloc);

  m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_DebugFence);
  m_pDevice->InternalRef();

  rm->SetInternalResource(m_DebugFence);

  ID3D12GraphicsCommandList *list = NULL;

  hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DebugAlloc, NULL,
                                    __uuidof(ID3D12GraphicsCommandList), (void **)&list);
  m_pDevice->InternalRef();

  // safe to upcast - this is a wrapped object
  m_DebugList = (ID3D12GraphicsCommandListX *)list;

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback command list, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  rm->SetInternalResource(m_DebugList);

  if(m_DebugList)
    m_DebugList->Close();

  {
    ResourceFormat fmt;
    fmt.type = ResourceFormatType::Regular;
    fmt.compType = CompType::Float;
    fmt.compByteWidth = 4;
    fmt.compCount = 1;
    bytebuf pattern = GetDiscardPattern(DiscardType::DiscardCall, fmt);
    fmt.compType = CompType::UInt;
    pattern.append(GetDiscardPattern(DiscardType::DiscardCall, fmt));

    m_DiscardConstantsDiscard = MakeCBuffer(pattern.size());
    m_pDevice->InternalRef();
    FillBuffer(m_DiscardConstantsDiscard, 0, pattern.data(), pattern.size());

    fmt.compType = CompType::Float;
    pattern = GetDiscardPattern(DiscardType::UndefinedTransition, fmt);
    fmt.compType = CompType::UInt;
    pattern.append(GetDiscardPattern(DiscardType::UndefinedTransition, fmt));

    m_DiscardConstantsUndefined = MakeCBuffer(pattern.size());
    m_pDevice->InternalRef();
    FillBuffer(m_DiscardConstantsUndefined, 0, pattern.data(), pattern.size());

    ID3DBlob *root = shaderCache->MakeRootSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 1, 1),
    });

    RDCASSERT(root);

    hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                        __uuidof(ID3D12RootSignature), (void **)&m_DiscardRootSig);
    m_pDevice->InternalRef();

    SAFE_RELEASE(root);
  }
}

D3D12DebugManager::~D3D12DebugManager()
{
  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(size_t p = 0; p < MeshDisplayPipelines::ePipe_Count; p++)
      SAFE_RELEASE(it->second.pipes[p]);

  for(auto it = m_MS2ArrayPSOCache.begin(); it != m_MS2ArrayPSOCache.end(); ++it)
  {
    SAFE_RELEASE(it->second.first);
    SAFE_RELEASE(it->second.second);
  }

  SAFE_RELEASE(dsvHeap);
  SAFE_RELEASE(rtvHeap);
  SAFE_RELEASE(cbvsrvuavHeap);
  SAFE_RELEASE(uavClearHeap);
  SAFE_RELEASE(samplerHeap);

  SAFE_RELEASE(m_MeshVS);
  SAFE_RELEASE(m_MeshGS);
  SAFE_RELEASE(m_MeshPS);
  SAFE_RELEASE(m_MeshRootSig);
  SAFE_RELEASE(m_MeshletBuf);

  SAFE_RELEASE(m_ShaderDebugRootSig);
  SAFE_RELEASE(m_MathIntrinsicsPso);
  SAFE_RELEASE(m_DXILMathIntrinsicsPso);
  SAFE_RELEASE(m_ShaderDebugResultBuffer);
  SAFE_RELEASE(m_TexSamplePso);
  SAFE_RELEASE(m_DXILTexSamplePso);
  for(auto it = m_OffsetTexSamplePso.begin(); it != m_OffsetTexSamplePso.end(); ++it)
    it->second->Release();
  for(auto it = m_DXILOffsetTexSamplePso.begin(); it != m_DXILOffsetTexSamplePso.end(); ++it)
    it->second->Release();

  SAFE_RELEASE(m_ArrayMSAARootSig);
  SAFE_RELEASE(m_FullscreenVS);

  SAFE_RELEASE(m_IntMS2Array);
  SAFE_RELEASE(m_FloatMS2Array);
  SAFE_RELEASE(m_DepthMS2Array);

  SAFE_RELEASE(m_IntArray2MS);
  SAFE_RELEASE(m_FloatArray2MS);
  SAFE_RELEASE(m_DepthArray2MS);

  SAFE_RELEASE(m_PixelHistoryCopyCS);
  SAFE_RELEASE(m_PixelHistoryCopySig);
  SAFE_RELEASE(m_PixelHistoryCopyPso);

  SAFE_RELEASE(m_ReadbackBuffer);

  SAFE_RELEASE(m_RingConstantBuffer);

  SAFE_RELEASE(m_TexResource);

  SAFE_RELEASE(m_DiscardConstantsDiscard);
  SAFE_RELEASE(m_DiscardConstantsUndefined);
  SAFE_RELEASE(m_DiscardRootSig);
  SAFE_RELEASE(m_DiscardFloatPS);
  SAFE_RELEASE(m_DiscardIntPS);

  SAFE_RELEASE(m_EIPatchRootSig);
  SAFE_RELEASE(m_EIPatchBufferData);
  SAFE_RELEASE(m_EIPatchPso);
  SAFE_RELEASE(m_EIPatchScratchBuffer);

  SAFE_RELEASE(m_DebugAlloc);
  SAFE_RELEASE(m_DebugList);
  SAFE_RELEASE(m_DebugFence);

  for(auto it = m_DiscardPipes.begin(); it != m_DiscardPipes.end(); it++)
    if(it->second)
      it->second->Release();

  for(auto it = m_DiscardPatterns.begin(); it != m_DiscardPatterns.end(); it++)
    if(it->second)
      it->second->Release();

  for(size_t i = 0; i < m_DiscardBuffers.size(); i++)
    m_DiscardBuffers[i]->Release();

  RenderDoc::Inst().UnregisterMemoryRegion(this);
}

bool D3D12DebugManager::CreateShaderDebugResources()
{
  rdcstr hlsl = GetEmbeddedResource(shaderdebug_hlsl);

  D3D12RootSignature rootSig;

  ID3DBlob *csBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugMathOp",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0",
                                                &csBlob) != "")
  {
    RDCERR("Failed to create shader to calculate math intrinsic");
    return false;
  }

  D3D12RootSignatureParameter constantParam;
  constantParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  constantParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  constantParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
  constantParam.Descriptor.ShaderRegister = 0;
  constantParam.Descriptor.RegisterSpace = 0;
  rootSig.Parameters.push_back(constantParam);

  D3D12RootSignatureParameter uavParam;
  uavParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  uavParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  uavParam.Descriptor.ShaderRegister = 1;
  uavParam.Descriptor.RegisterSpace = 0;
  uavParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
  rootSig.Parameters.push_back(uavParam);

  D3D12_DESCRIPTOR_RANGE1 range = {};
  range.BaseShaderRegister = 0;
  range.RegisterSpace = 0;
  range.NumDescriptors = 25;
  range.OffsetInDescriptorsFromTableStart = 0;
  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

  D3D12RootSignatureParameter srv;
  srv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  srv.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  srv.ranges.push_back(range);
  rootSig.Parameters.push_back(srv);

  range.NumDescriptors = 2;
  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

  D3D12RootSignatureParameter sampParam;
  sampParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  sampParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  sampParam.ranges.push_back(range);
  rootSig.Parameters.push_back(sampParam);

  ID3DBlob *root = m_pDevice->GetShaderCache()->MakeRootSig(rootSig);
  if(root == NULL)
  {
    RDCERR("Failed to create root signature for shader debugging");
    SAFE_RELEASE(csBlob);
    return false;
  }

  HRESULT hr =
      m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature), (void **)&m_ShaderDebugRootSig);
  m_pDevice->InternalRef();
  SAFE_RELEASE(root);
  if(FAILED(hr))
  {
    RDCERR("Failed to create root signature for shader debugging HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(csBlob);
    return false;
  }
  m_pDevice->GetResourceManager()->SetInternalResource(m_ShaderDebugRootSig);

  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc;
  psoDesc.pRootSignature = m_ShaderDebugRootSig;
  psoDesc.CS.BytecodeLength = csBlob->GetBufferSize();
  psoDesc.CS.pShaderBytecode = csBlob->GetBufferPointer();
  psoDesc.NodeMask = 0;
  psoDesc.CachedPSO.pCachedBlob = NULL;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  hr = m_pDevice->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&m_MathIntrinsicsPso);
  m_pDevice->InternalRef();
  SAFE_RELEASE(csBlob);
  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  m_pDevice->GetResourceManager()->SetInternalResource(m_MathIntrinsicsPso);

  // Create buffer to store computed result
  D3D12_RESOURCE_DESC rdesc;
  ZeroMemory(&rdesc, sizeof(D3D12_RESOURCE_DESC));
  rdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  rdesc.Width = sizeof(Vec4f) * 6;
  rdesc.Height = 1;
  rdesc.DepthOrArraySize = 1;
  rdesc.MipLevels = 1;
  rdesc.Format = DXGI_FORMAT_UNKNOWN;
  rdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  rdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  rdesc.SampleDesc.Count = 1;
  rdesc.SampleDesc.Quality = 0;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  hr = m_pDevice->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &rdesc, D3D12_RESOURCE_STATE_COMMON, NULL,
      __uuidof(ID3D12Resource), (void **)&m_ShaderDebugResultBuffer);
  m_pDevice->InternalRef();
  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer for pixel shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  m_pDevice->GetResourceManager()->SetInternalResource(m_ShaderDebugResultBuffer);

  // Create UAV to store the computed results
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
  ZeroMemory(&uavDesc, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = 1;
  uavDesc.Buffer.StructureByteStride = sizeof(Vec4f) * 6;

  D3D12_CPU_DESCRIPTOR_HANDLE uav = GetCPUHandle(SHADER_DEBUG_UAV);
  m_pDevice->CreateUnorderedAccessView(m_ShaderDebugResultBuffer, NULL, &uavDesc, uav);

  ID3DBlob *vsBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSampleVS",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0",
                                                &vsBlob) != "")
  {
    RDCERR("Failed to create shader to do texture sample");
    SAFE_RELEASE(vsBlob);
    return false;
  }

  ID3DBlob *psBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSamplePS",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0",
                                                &psBlob) != "")
  {
    RDCERR("Failed to create shader to do texture sample");
    SAFE_RELEASE(vsBlob);
    SAFE_RELEASE(psBlob);
    return false;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

  pipeDesc.pRootSignature = m_ShaderDebugRootSig;
  pipeDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

  hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                              (void **)&m_TexSamplePso);
  m_pDevice->InternalRef();
  SAFE_RELEASE(vsBlob);
  SAFE_RELEASE(psBlob);
  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  m_pDevice->GetResourceManager()->SetInternalResource(m_TexSamplePso);

  if(m_pDevice->UsedDXIL())
  {
    ID3DBlob *dxilCsBlob = NULL;
    if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugMathOp",
                                                  D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_6_0",
                                                  &dxilCsBlob) != "")
    {
      RDCASSERT(!dxilCsBlob);
      RDCERR("Failed to compile DXIL compute shader to calculate math intrinsic");
      return false;
    }

    psoDesc.pRootSignature = m_ShaderDebugRootSig;
    psoDesc.CS.BytecodeLength = dxilCsBlob->GetBufferSize();
    psoDesc.CS.pShaderBytecode = dxilCsBlob->GetBufferPointer();
    psoDesc.NodeMask = 0;
    psoDesc.CachedPSO.pCachedBlob = NULL;
    psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    hr = m_pDevice->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState),
                                               (void **)&m_DXILMathIntrinsicsPso);
    m_pDevice->InternalRef();
    SAFE_RELEASE(dxilCsBlob);
    if(FAILED(hr))
    {
      RDCERR("Failed to create math instrinsics PSO for DXIL shader debugging HRESULT: %s",
             ToStr(hr).c_str());
      return false;
    }
    m_pDevice->GetResourceManager()->SetInternalResource(m_DXILMathIntrinsicsPso);
  }

  if(m_pDevice->UsedDXIL())
  {
    ID3DBlob *dxilVsBlob = NULL;
    if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSampleVS",
                                                  D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_6_0",
                                                  &dxilVsBlob) != "")
    {
      RDCASSERT(!dxilVsBlob);
      RDCERR("Failed to compile DXIL vertex shader for shader debugging");
      return false;
    }
    ID3DBlob *dxilPsBlob = NULL;
    if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSamplePS",
                                                  D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_6_0",
                                                  &dxilPsBlob) != "")
    {
      RDCASSERT(!dxilPsBlob);
      RDCERR("Failed to compile DXIL pixel shader for shader debugging");
      return false;
    }

    pipeDesc.VS.BytecodeLength = dxilVsBlob->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = dxilVsBlob->GetBufferPointer();
    pipeDesc.PS.BytecodeLength = dxilPsBlob->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = dxilPsBlob->GetBufferPointer();

    hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                (void **)&m_DXILTexSamplePso);
    m_pDevice->InternalRef();
    SAFE_RELEASE(dxilVsBlob);
    SAFE_RELEASE(dxilPsBlob);
    if(FAILED(hr))
    {
      RDCERR("Failed to create texture sampling PSO for DXIL shader debugging HRESULT: %s",
             ToStr(hr).c_str());
      return false;
    }
    m_pDevice->GetResourceManager()->SetInternalResource(m_DXILTexSamplePso);
  }

  return true;
}

ID3D12PipelineState *D3D12DebugManager::GetTexSamplePso(const int8_t offsets[3])
{
  uint32_t offsKey = offsets[0] | (offsets[1] << 8) | (offsets[2] << 16);
  if(offsKey == 0)
    return m_TexSamplePso;

  ID3D12PipelineState *ps = m_OffsetTexSamplePso[offsKey];
  if(ps)
    return ps;

  D3D12ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  shaderCache->SetCaching(true);

  rdcstr hlsl = GetEmbeddedResource(shaderdebug_hlsl);

  hlsl = StringFormat::Fmt("#define debugSampleOffsets int4(%d,%d,%d,0)\n\n%s", offsets[0],
                           offsets[1], offsets[2], hlsl.c_str());

  ID3DBlob *vsBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSampleVS",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0",
                                                &vsBlob) != "")
  {
    RDCERR("Failed to create shader to do texture sample");
    SAFE_RELEASE(vsBlob);
    return false;
  }

  ID3DBlob *psBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSamplePS",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0",
                                                &psBlob) != "")
  {
    RDCERR("Failed to create shader to do texture sample");
    SAFE_RELEASE(vsBlob);
    SAFE_RELEASE(psBlob);
    return false;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

  pipeDesc.pRootSignature = m_ShaderDebugRootSig;
  pipeDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

  ID3D12PipelineState *pso = NULL;
  HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&pso);
  m_pDevice->InternalRef();
  SAFE_RELEASE(vsBlob);
  SAFE_RELEASE(psBlob);
  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  m_pDevice->GetResourceManager()->SetInternalResource(pso);

  m_OffsetTexSamplePso[offsKey] = pso;

  shaderCache->SetCaching(false);

  return pso;
}

ID3D12PipelineState *D3D12DebugManager::GetDXILTexSamplePso(const int8_t offsets[3])
{
  uint32_t offsKey = offsets[0] | (offsets[1] << 8) | (offsets[2] << 16);
  if(offsKey == 0)
    return m_DXILTexSamplePso;

  ID3D12PipelineState *ps = m_DXILOffsetTexSamplePso[offsKey];
  if(ps)
    return ps;

  D3D12ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  shaderCache->SetCaching(true);

  rdcstr hlsl = GetEmbeddedResource(shaderdebug_hlsl);

  hlsl = StringFormat::Fmt("#define debugSampleOffsets int4(%d,%d,%d,0)\n\n%s", offsets[0],
                           offsets[1], offsets[2], hlsl.c_str());

  ID3DBlob *vsBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSampleVS",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_6_0",
                                                &vsBlob) != "")
  {
    // TODO : need some kind of fallback
    RDCERR("Failed to create DXIL shader debugger vertex shader to do texture sample");
    SAFE_RELEASE(vsBlob);
    return false;
  }

  ID3DBlob *psBlob = NULL;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DebugSamplePS",
                                                D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_6_0",
                                                &psBlob) != "")
  {
    // TODO : need some kind of fallback
    RDCERR("Failed to create DXIL shader debugger pixel shader to do texture sample");
    SAFE_RELEASE(vsBlob);
    SAFE_RELEASE(psBlob);
    return false;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

  pipeDesc.pRootSignature = m_ShaderDebugRootSig;
  pipeDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.SampleMask = 0xFFFFFFFF;
  pipeDesc.SampleDesc.Count = 1;
  pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
  pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

  ID3D12PipelineState *pso = NULL;
  HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&pso);
  m_pDevice->InternalRef();
  SAFE_RELEASE(vsBlob);
  SAFE_RELEASE(psBlob);
  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }
  m_pDevice->GetResourceManager()->SetInternalResource(pso);

  m_DXILOffsetTexSamplePso[offsKey] = pso;

  shaderCache->SetCaching(false);

  return pso;
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

  HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
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
  m_pDevice->CheckHRESULT(hr);

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

  // passing the unwrapped object here is immaterial as all we do is Map/Unmap, but it means we can
  // call this function while capturing without worrying about serialising the map or deadlocking.
  FillBuffer(Unwrap(m_RingConstantBuffer), (size_t)m_RingConstantOffset, data, size);

  m_RingConstantOffset += size;
  m_RingConstantOffset =
      AlignUp(m_RingConstantOffset, (UINT64)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

  return ret;
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12DebugManager::UploadMeshletSizes(uint32_t meshletIndexOffset,
                                                                const rdcarray<MeshletSize> &sizes)
{
  D3D12_GPU_VIRTUAL_ADDRESS ret = m_MeshletBuf->GetGPUVirtualAddress();

  if(sizes.empty())
    return ret;

  rdcarray<uint32_t> data;
  data.resize(sizes.size());
  uint32_t prefixCount = meshletIndexOffset;
  for(size_t i = 0; i < data.size(); i++)
  {
    prefixCount += sizes[i].numVertices;
    data[i] = prefixCount;
  }

  if(m_CurMeshletOffset + data.byteSize() > m_MeshletBuf->GetDesc().Width)
    m_CurMeshletOffset = 0;

  ret += m_CurMeshletOffset;

  // passing the unwrapped object here is immaterial as all we do is Map/Unmap, but it means we can
  // call this function while capturing without worrying about serialising the map or deadlocking.
  FillBuffer(Unwrap(m_MeshletBuf), (size_t)m_CurMeshletOffset, data.data(), data.byteSize());

  m_CurMeshletOffset += data.byteSize();
  m_CurMeshletOffset =
      AlignUp(m_CurMeshletOffset, (UINT64)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

  return ret;
}

ID3D12GraphicsCommandListX *D3D12DebugManager::ResetDebugList()
{
  m_DebugList->Reset(m_DebugAlloc, NULL);

  return m_DebugList;
}

void D3D12DebugManager::ResetDebugAlloc()
{
  m_DebugAlloc->Reset();
}

rdcpair<ID3D12Resource *, UINT64> D3D12DebugManager::PatchExecuteIndirect(
    ID3D12GraphicsCommandListX *cmd, const D3D12RenderState &state, ID3D12CommandSignature *comSig,
    ID3D12Resource *argBuf, UINT64 argBufOffset, D3D12_GPU_VIRTUAL_ADDRESS countBufAddr,
    UINT maxCount)
{
  rdcarray<uint32_t> argOffsets;

  WrappedID3D12CommandSignature *wrappedComSig = (WrappedID3D12CommandSignature *)comSig;
  uint32_t offset = 0;
  for(const D3D12_INDIRECT_ARGUMENT_DESC &arg : wrappedComSig->sig.arguments)
  {
    switch(arg.Type)
    {
      case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
      {
        offset += sizeof(D3D12_DRAW_ARGUMENTS);
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
      {
        offset += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
      {
        offset += sizeof(D3D12_DISPATCH_ARGUMENTS);
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
      {
        offset += sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:
      {
        argOffsets.push_back(
            offset + offsetof(D3D12_DISPATCH_RAYS_DESC, RayGenerationShaderRecord.StartAddress));
        argOffsets.push_back(offset +
                             offsetof(D3D12_DISPATCH_RAYS_DESC, MissShaderTable.StartAddress));
        argOffsets.push_back(offset + offsetof(D3D12_DISPATCH_RAYS_DESC, HitGroupTable.StartAddress));
        argOffsets.push_back(offset +
                             offsetof(D3D12_DISPATCH_RAYS_DESC, CallableShaderTable.StartAddress));
        offset += sizeof(D3D12_DISPATCH_RAYS_DESC);
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
      {
        offset += sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
      {
        argOffsets.push_back(offset);
        offset += sizeof(D3D12_VERTEX_BUFFER_VIEW);

        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
      {
        argOffsets.push_back(offset);
        offset += sizeof(D3D12_INDEX_BUFFER_VIEW);
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
      case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
      case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
      {
        argOffsets.push_back(offset);
        offset += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
        break;
      }
      default: RDCERR("Unexpected argument type! %d", arg.Type); break;
    }
  }

  // early out if the command signature doesn't reference anything with addresses
  if(argOffsets.empty())
    return {argBuf, argBufOffset};

  // only handle patching 128 address based arguments...
  RDCASSERT(argOffsets.size() <= 128);

  D3D12MarkerRegion marker(cmd, "Patch execute indirect");

  argOffsets.insert(0, (uint32_t)argOffsets.size());
  argOffsets.insert(1, m_EIPatchBufferCount);
  argOffsets.insert(2, wrappedComSig->sig.ByteStride);
  argOffsets.insert(3, 0);    // padding
  argOffsets.resize(128 + 3);
  // argOffsets is now the executepatchdata cbuffer

  const UINT64 argDataSize =
      wrappedComSig->sig.ByteStride * (maxCount - 1) + wrappedComSig->sig.PackedByteSize;

  if(m_EIPatchScratchOffset + argDataSize > m_EIPatchScratchBuffer->GetDesc().Width)
    m_EIPatchScratchOffset = 0;

  RDCASSERT(m_EIPatchScratchOffset + argDataSize < m_EIPatchScratchBuffer->GetDesc().Width,
            wrappedComSig->sig.ByteStride, wrappedComSig->sig.PackedByteSize, maxCount);

  rdcpair<ID3D12Resource *, UINT64> ret = {m_EIPatchScratchBuffer, m_EIPatchScratchOffset};

  D3D12_RESOURCE_BARRIER b = {};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = m_EIPatchScratchBuffer;
  b.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

  cmd->ResourceBarrier(1, &b);

  cmd->CopyBufferRegion(m_EIPatchScratchBuffer, m_EIPatchScratchOffset, argBuf, argBufOffset,
                        argDataSize);

  b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  b.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  cmd->ResourceBarrier(1, &b);

  cmd->SetPipelineState(m_EIPatchPso);
  cmd->SetComputeRootSignature(m_EIPatchRootSig);
  cmd->SetComputeRootConstantBufferView(0, UploadConstants(argOffsets.data(), argOffsets.byteSize()));
  if(countBufAddr == 0)
    cmd->SetComputeRootConstantBufferView(1, UploadConstants(&maxCount, sizeof(uint32_t)));
  else
    cmd->SetComputeRootConstantBufferView(1, countBufAddr);
  cmd->SetComputeRoot32BitConstant(2, maxCount, 0);
  cmd->SetComputeRootShaderResourceView(3, m_EIPatchBufferData->GetGPUVirtualAddress());
  cmd->SetComputeRootUnorderedAccessView(4, ret.first->GetGPUVirtualAddress() + ret.second);
  cmd->Dispatch(1, 1, 1);

  b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  b.Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  cmd->ResourceBarrier(1, &b);

  b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  b.UAV.pResource = m_EIPatchScratchBuffer;
  cmd->ResourceBarrier(1, &b);

  state.ApplyState(m_pDevice, cmd);

  m_EIPatchScratchOffset += wrappedComSig->sig.ByteStride * maxCount;

  return ret;
}

void D3D12DebugManager::FillWithDiscardPattern(ID3D12GraphicsCommandListX *cmd,
                                               const D3D12RenderState &state, DiscardType type,
                                               ID3D12Resource *res,
                                               const D3D12_DISCARD_REGION *region,
                                               D3D12_BARRIER_LAYOUT LayoutAfter)
{
  RDCASSERT(type == DiscardType::DiscardCall || type == DiscardType::UndefinedTransition);

  D3D12MarkerRegion marker(
      cmd, StringFormat::Fmt("FillWithDiscardPattern %s", ToStr(GetResID(res)).c_str()));

  D3D12_RESOURCE_DESC desc = res->GetDesc();

  rdcarray<D3D12_RECT> rects;

  if(region && region->NumRects > 0)
    rects.assign(region->pRects, region->NumRects);
  else
    rects = {{0, 0, (LONG)desc.Width, (LONG)desc.Height}};

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    // ignore rects, they are only allowed with 2D resources
    size_t size = (size_t)desc.Width;

    ID3D12Resource *patternBuf = NULL;

    // if we have discard buffers, try the last one, it's the biggest we have
    if(!m_DiscardBuffers.empty())
    {
      patternBuf = m_DiscardBuffers.back();

      // if it's not big enough, don't use it
      if(patternBuf->GetDesc().Width < size)
        patternBuf = NULL;
    }

    // if we don't have a buffer, make one that's big enough and use that
    if(patternBuf == NULL)
    {
      bytebuf pattern;
      // make at least 1K at a time to prevent too much incremental updates if we encounter buffers
      // of different sizes
      pattern.resize(AlignUp<size_t>(size, 1024U));

      uint32_t value = 0xD15CAD3D;

      for(size_t i = 0; i < pattern.size(); i += 4)
        memcpy(&pattern[i], &value, sizeof(uint32_t));

      patternBuf = MakeCBuffer(pattern.size());

      m_DiscardBuffers.push_back(patternBuf);

      FillBuffer(patternBuf, 0, pattern.data(), size);
    }

    // fill the destination with a copy from the pattern buffer
    cmd->CopyBufferRegion(res, 0, patternBuf, 0, size);

    return;
  }

  UINT firstSub = region ? region->FirstSubresource : 0;
  UINT numSubs = region ? region->NumSubresources : GetNumSubresources(m_pDevice, &desc);

  if(desc.SampleDesc.Count > 1)
  {
    // we can't do discard patterns for MSAA on compute comand lists
    if(cmd->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE)
      return;

    bool depth = false;
    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
      depth = true;

    DXGI_FORMAT fmt = desc.Format;

    if(depth)
      fmt = GetDepthTypedFormat(fmt);
    else
      fmt = GetTypedFormat(fmt, CompType::Float);

    rdcpair<DXGI_FORMAT, UINT> key = {fmt, desc.SampleDesc.Count};
    rdcpair<DXGI_FORMAT, UINT> stencilKey = {DXGI_FORMAT_UNKNOWN, desc.SampleDesc.Count};

    ID3D12PipelineState *pipe = m_DiscardPipes[key];
    ID3D12PipelineState *stencilpipe = pipe;

    if(fmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
    {
      stencilKey.first = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
      stencilpipe = m_DiscardPipes[stencilKey];
    }
    else if(fmt == DXGI_FORMAT_D24_UNORM_S8_UINT)
    {
      stencilKey.first = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      stencilpipe = m_DiscardPipes[stencilKey];
    }

    bool intFormat = !depth && (IsIntFormat(fmt) || IsUIntFormat(fmt));

    if(pipe == NULL)
    {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

      pipeDesc.pRootSignature = m_DiscardRootSig;
      pipeDesc.VS.BytecodeLength = m_FullscreenVS->GetBufferSize();
      pipeDesc.VS.pShaderBytecode = m_FullscreenVS->GetBufferPointer();
      pipeDesc.PS.BytecodeLength =
          intFormat ? m_DiscardIntPS->GetBufferSize() : m_DiscardFloatPS->GetBufferSize();
      pipeDesc.PS.pShaderBytecode =
          intFormat ? m_DiscardIntPS->GetBufferPointer() : m_DiscardFloatPS->GetBufferPointer();
      pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      pipeDesc.SampleMask = 0xFFFFFFFF;
      pipeDesc.SampleDesc.Count = desc.SampleDesc.Count;
      pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
      pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

      pipeDesc.DepthStencilState.StencilReadMask = 0xFF;
      pipeDesc.DepthStencilState.StencilWriteMask = 0xFF;
      pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
      pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
      pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
      pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;

      pipeDesc.DepthStencilState.DepthEnable = FALSE;
      pipeDesc.DepthStencilState.StencilEnable = FALSE;

      if(depth)
      {
        pipeDesc.DSVFormat = fmt;
        pipeDesc.DepthStencilState.DepthEnable = TRUE;
      }
      else
      {
        pipeDesc.NumRenderTargets = 1;
        pipeDesc.RTVFormats[0] = fmt;
      }

      HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                          (void **)&pipe);

      if(FAILED(hr))
        RDCERR("Couldn't create MSAA discard pattern pipe! HRESULT: %s", ToStr(hr).c_str());

      m_DiscardPipes[key] = pipe;

      if(stencilKey.first != DXGI_FORMAT_UNKNOWN)
      {
        pipeDesc.DepthStencilState.DepthEnable = FALSE;
        pipeDesc.DepthStencilState.StencilEnable = TRUE;

        hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                    (void **)&stencilpipe);

        if(FAILED(hr))
          RDCERR("Couldn't create MSAA discard pattern pipe! HRESULT: %s", ToStr(hr).c_str());

        m_DiscardPipes[stencilKey] = stencilpipe;
      }
    }

    if(!pipe)
      return;

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetPipelineState(pipe);
    cmd->SetGraphicsRootSignature(m_DiscardRootSig);
    cmd->SetGraphicsRootConstantBufferView(
        0, type == DiscardType::DiscardCall ? m_DiscardConstantsDiscard->GetGPUVirtualAddress()
                                            : m_DiscardConstantsUndefined->GetGPUVirtualAddress());
    D3D12_VIEWPORT viewport = {0, 0, (float)desc.Width, (float)desc.Height, 0.0f, 1.0f};
    cmd->RSSetViewports(1, &viewport);

    if(m_pDevice->GetOpts3().ViewInstancingTier != D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED)
      cmd->SetViewInstanceMask(0);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
    rtvDesc.Format = fmt;
    rtvDesc.Texture2DMSArray.ArraySize = 1;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
    dsvDesc.Format = fmt;
    dsvDesc.Texture2DMSArray.ArraySize = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCPUHandle(MSAA_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetCPUHandle(MSAA_DSV);

    for(UINT sub = 0; sub < numSubs; sub++)
    {
      UINT subresource = firstSub + sub;
      if(depth)
      {
        dsvDesc.Texture2DMSArray.FirstArraySlice = GetSliceForSubresource(res, subresource);
        m_pDevice->CreateDepthStencilView(res, &dsvDesc, dsv);
        cmd->OMSetRenderTargets(0, NULL, FALSE, &dsv);
      }
      else
      {
        rtvDesc.Texture2DMSArray.FirstArraySlice = GetSliceForSubresource(res, subresource);
        m_pDevice->CreateRenderTargetView(res, &rtvDesc, rtv);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, NULL);
      }

      UINT mip = GetMipForSubresource(res, subresource);
      UINT plane = GetPlaneForSubresource(res, subresource);

      for(D3D12_RECT r : rects)
      {
        r.right = RDCMIN(LONG(RDCMAX(1U, (UINT)desc.Width >> mip)), r.right);
        r.bottom = RDCMIN(LONG(RDCMAX(1U, (UINT)desc.Height >> mip)), r.bottom);

        cmd->RSSetScissorRects(1, &r);

        if(depth)
        {
          if(plane == 0)
          {
            cmd->SetPipelineState(pipe);
            cmd->SetGraphicsRoot32BitConstant(1, 0, 0);
            cmd->DrawInstanced(3, 1, 0, 0);
          }
          else
          {
            cmd->SetPipelineState(stencilpipe);
            cmd->SetGraphicsRoot32BitConstant(1, 1, 0);
            cmd->OMSetStencilRef(0x00);
            cmd->DrawInstanced(3, 1, 0, 0);

            cmd->SetGraphicsRoot32BitConstant(1, 2, 0);
            cmd->OMSetStencilRef(0xff);
            cmd->DrawInstanced(3, 1, 0, 0);
          }
        }
        else
        {
          cmd->SetGraphicsRoot32BitConstant(1, 0, 0);
          cmd->DrawInstanced(3, 1, 0, 0);
        }
      }
    }

    state.ApplyState(m_pDevice, cmd);

    return;
  }

  static const uint32_t PatternBatchWidth = 256;
  static const uint32_t PatternBatchHeight = 256;

  // see if we already have a buffer with texels in the desired format, if not then create it
  ID3D12Resource *buf = m_DiscardPatterns[{type, desc.Format}];

  if(buf == NULL)
  {
    bytebuf pattern = GetDiscardPattern(type, MakeResourceFormat(desc.Format));

    buf = MakeCBuffer(pattern.size() * (PatternBatchWidth / DiscardPatternWidth) *
                      (PatternBatchHeight / DiscardPatternHeight));

    D3D12_RANGE range = {0, 0};
    byte *ptr = NULL;
    HRESULT hr = buf->Map(0, &range, (void **)&ptr);
    m_pDevice->CheckHRESULT(hr);

    if(ptr)
    {
      DXGI_FORMAT fmt = desc.Format;
      int passes = 1;

      if(IsDepthAndStencilFormat(fmt))
      {
        fmt = DXGI_FORMAT_R32_FLOAT;
        passes = 2;
      }

      byte *dst = ptr;
      size_t srcOffset = 0;

      // row pitch is the same for depth and stencil
      const uint32_t srcRowPitch = GetByteSize(DiscardPatternWidth, 1, 1, fmt, 0);

      for(int pass = 0; pass < passes; pass++)
      {
        const uint32_t numHorizBatches = PatternBatchWidth / DiscardPatternWidth;
        const uint32_t srcRowByteLength = GetByteSize(DiscardPatternWidth, 1, 1, fmt, 0);
        const uint32_t dstRowByteLength = GetByteSize(PatternBatchWidth, 1, 1, fmt, 0);
        uint32_t numDstRows = PatternBatchHeight;
        uint32_t numSrcRows = DiscardPatternHeight;
        if(IsBlockFormat(fmt))
        {
          numDstRows /= 4;
          numSrcRows /= 4;
        }

        for(uint32_t row = 0; row < numDstRows; row++)
        {
          byte *src = pattern.data() + srcOffset + (row % numSrcRows) * srcRowPitch;
          for(uint32_t x = 0; x < numHorizBatches; x++)
            memcpy(dst + srcRowByteLength * x, src, srcRowByteLength);

          dst += dstRowByteLength;
        }

        if(passes == 2)
        {
          fmt = DXGI_FORMAT_R8_UINT;
          srcOffset =
              GetByteSize(DiscardPatternWidth, DiscardPatternHeight, 1, DXGI_FORMAT_R32_FLOAT, 0);
        }
      }

      buf->Unmap(0, NULL);
    }

    m_DiscardPatterns[{type, desc.Format}] = buf;
  }

  for(UINT sub = firstSub; sub < firstSub + numSubs; sub++)
  {
    D3D12_RESOURCE_BARRIER b = {};
    b.Transition.pResource = res;
    b.Transition.Subresource = sub;

    // TODO can we do better than an educated guess as to what the previous state was?
    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    else
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;

    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_TEXTURE_BARRIER tex = {};
    D3D12_BARRIER_GROUP group = {};

    if(m_pDevice->GetOpts12().EnhancedBarriersSupported)
    {
      // with new barriers we can explicitly discard here instead of guessing StateBefore, though we
      // still need to guess a LayoutAfter once we're done for DiscardResource() calls

      tex.LayoutBefore = D3D12_BARRIER_LAYOUT_UNDEFINED;
      tex.AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;
      tex.SyncBefore = D3D12_BARRIER_SYNC_ALL;
      tex.LayoutAfter = D3D12_BARRIER_LAYOUT_COPY_DEST;
      tex.AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST;
      tex.SyncAfter = D3D12_BARRIER_SYNC_COPY;

      tex.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
      tex.Subresources.IndexOrFirstMipLevel = (UINT)sub;
      tex.pResource = res;

      group.NumBarriers = 1;
      group.Type = D3D12_BARRIER_TYPE_TEXTURE;
      group.pTextureBarriers = &tex;

      cmd->Barrier(1, &group);
    }
    else
    {
      cmd->ResourceBarrier(1, &b);
    }

    D3D12_TEXTURE_COPY_LOCATION dst, src;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = res;
    dst.SubresourceIndex = sub;

    UINT mip = GetMipForSubresource(res, sub);

    DXGI_FORMAT fmt = desc.Format;
    UINT bufOffset = 0;

    // if this is a depth/stencil format it comes in multiple planes - figure out which format we're
    // copying and the appropriate buffer offset
    if(IsDepthAndStencilFormat(fmt))
    {
      UINT planeSlice = GetPlaneForSubresource(res, sub);

      if(planeSlice == 0)
      {
        fmt = DXGI_FORMAT_R32_TYPELESS;
      }
      else
      {
        fmt = DXGI_FORMAT_R8_TYPELESS;
        bufOffset += GetByteSize(PatternBatchWidth, PatternBatchHeight, 1, DXGI_FORMAT_R32_FLOAT, 0);
      }
    }

    // the user isn't allowed to specify rects for 3D textures, so in that case we'll have our own
    // default 0,0->64k,64k one. Similarly we also discard all z slices
    uint32_t depth =
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1U;
    for(uint32_t z = 0; z < RDCMAX(1U, depth >> mip); z++)
    {
      for(D3D12_RECT r : rects)
      {
        int32_t rectWidth = RDCMIN(LONG(RDCMAX(1U, (UINT)desc.Width >> mip)), r.right);
        int32_t rectHeight = RDCMIN(LONG(RDCMAX(1U, (UINT)desc.Height >> mip)), r.bottom);

        for(int32_t y = r.top; y < rectHeight; y += PatternBatchHeight)
        {
          for(int32_t x = r.left; x < rectWidth; x += PatternBatchWidth)
          {
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.pResource = buf;
            src.PlacedFootprint.Offset = bufOffset;
            src.PlacedFootprint.Footprint.Format = fmt;
            src.PlacedFootprint.Footprint.RowPitch =
                AlignUp(GetRowPitch(PatternBatchWidth, fmt, 0), 256U);
            src.PlacedFootprint.Footprint.Width = RDCMIN(PatternBatchWidth, uint32_t(rectWidth - x));
            src.PlacedFootprint.Footprint.Height =
                RDCMIN(PatternBatchHeight, uint32_t(rectHeight - y));
            src.PlacedFootprint.Footprint.Depth = 1;

            cmd->CopyTextureRegion(&dst, x, y, z, &src, NULL);
          }
        }
      }
    }

    if(group.NumBarriers > 0)
    {
      tex.LayoutBefore = D3D12_BARRIER_LAYOUT_COPY_DEST;
      tex.AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST;
      tex.SyncBefore = D3D12_BARRIER_SYNC_COPY;
      tex.LayoutAfter = LayoutAfter;
      tex.AccessAfter = D3D12_BARRIER_ACCESS_COMMON;
      tex.SyncAfter = D3D12_BARRIER_SYNC_ALL;
      tex.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;

      if(LayoutAfter == D3D12_BARRIER_LAYOUT_UNDEFINED)
      {
        // still need to guess, oops.
        // since we don't know if the user will use old or new barriers we transition to common so
        // that it's hopefully as suitable as possible for possible interactions. There is no single
        // legal layout we can transition to, but we err on the side of assuming that calls to
        // DiscardResource (where an undefined LayoutAfter would happen) will be used with old
        // states, since new layouts have a built-in discard function with undefined transitions. So
        // transitioning to common makes it more compatible with an old barrier after that
        tex.LayoutAfter = D3D12_BARRIER_LAYOUT_COMMON;
      }

      cmd->Barrier(1, &group);
    }
    else
    {
      std::swap(b.Transition.StateBefore, b.Transition.StateAfter);

      cmd->ResourceBarrier(1, &b);
    }

    // workaround possible nvidia driver bug? without this depth-stencil discards of only one region
    // (smaller than 256x256) will get corrupted if both depth and stencil are copied to.
    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
      b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      b.UAV.pResource = NULL;

      cmd->ResourceBarrier(1, &b);
    }
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

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetCPUHandle(SamplerSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = samplerHeap->GetCPUDescriptorHandleForHeapStart();
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

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetGPUHandle(SamplerSlot slot)
{
  D3D12_GPU_DESCRIPTOR_HANDLE ret = samplerHeap->GetGPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetTempDescriptor(const D3D12Descriptor &desc,
                                                                 size_t idx)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = {};

  ID3D12Resource *res =
      m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(desc.GetResResourceId());

  if(desc.GetType() == D3D12DescriptorType::RTV)
  {
    ret = GetCPUHandle(FIRST_TMP_RTV);
    ret.ptr += idx * sizeof(D3D12Descriptor);

    const D3D12_RENDER_TARGET_VIEW_DESC *rtvdesc = &desc.GetRTV();
    if(rtvdesc->ViewDimension == D3D12_RTV_DIMENSION_UNKNOWN)
      rtvdesc = NULL;

    if(rtvdesc == NULL || rtvdesc->Format == DXGI_FORMAT_UNKNOWN)
    {
      const std::map<ResourceId, DXGI_FORMAT> &bbs = m_pDevice->GetBackbufferFormats();

      auto it = bbs.find(GetResID(res));

      // fixup for backbuffers
      if(it != bbs.end())
      {
        D3D12_RENDER_TARGET_VIEW_DESC bbDesc = {};
        bbDesc.Format = it->second;
        bbDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_pDevice->CreateRenderTargetView(res, &bbDesc, ret);
        return ret;
      }
    }

    m_pDevice->CreateRenderTargetView(res, rtvdesc, ret);
  }
  else if(desc.GetType() == D3D12DescriptorType::DSV)
  {
    ret = GetCPUHandle(TMP_DSV);

    const D3D12_DEPTH_STENCIL_VIEW_DESC *dsvdesc = &desc.GetDSV();
    if(dsvdesc->ViewDimension == D3D12_DSV_DIMENSION_UNKNOWN)
      dsvdesc = NULL;

    m_pDevice->CreateDepthStencilView(res, dsvdesc, ret);
  }
  else if(desc.GetType() == D3D12DescriptorType::UAV)
  {
    // need a non-shader visible heap for this one
    ret = GetUAVClearHandle(TMP_UAV);

    ID3D12Resource *counterRes =
        m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(desc.GetCounterResourceId());

    D3D12_UNORDERED_ACCESS_VIEW_DESC unpacked = desc.GetUAV();

    const D3D12_UNORDERED_ACCESS_VIEW_DESC *uavdesc = &unpacked;
    if(uavdesc->ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
      uavdesc = NULL;

    if(uavdesc == NULL || uavdesc->Format == DXGI_FORMAT_UNKNOWN)
    {
      const std::map<ResourceId, DXGI_FORMAT> &bbs = m_pDevice->GetBackbufferFormats();

      auto it = bbs.find(GetResID(res));

      // fixup for backbuffers
      if(it != bbs.end())
      {
        D3D12_UNORDERED_ACCESS_VIEW_DESC bbDesc = {};
        bbDesc.Format = it->second;
        bbDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        m_pDevice->CreateUnorderedAccessView(res, NULL, &bbDesc, ret);
        return ret;
      }
    }

    m_pDevice->CreateUnorderedAccessView(res, counterRes, uavdesc, ret);
  }
  else
  {
    RDCERR("Unexpected descriptor type %s for temp descriptor!", ToStr(desc.GetType()).c_str());
  }

  return ret;
}

void D3D12DebugManager::SetDescriptorHeaps(ID3D12GraphicsCommandList *list, bool cbvsrvuav,
                                           bool samplers)
{
  ID3D12DescriptorHeap *heaps[] = {cbvsrvuavHeap, samplerHeap};

  if(cbvsrvuav && samplers)
  {
    list->SetDescriptorHeaps(2, heaps);
  }
  else if(cbvsrvuav)
  {
    list->SetDescriptorHeaps(1, &heaps[0]);
  }
  else if(samplers)
  {
    list->SetDescriptorHeaps(1, &heaps[1]);
  }
}

void D3D12DebugManager::SetDescriptorHeaps(rdcarray<ResourceId> &heaps, bool cbvsrvuav, bool samplers)
{
  heaps.clear();
  if(cbvsrvuav)
    heaps.push_back(GetResID(cbvsrvuavHeap));
  if(samplers)
    heaps.push_back(GetResID(samplerHeap));
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetUAVClearHandle(CBVUAVSRVSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = uavClearHeap->GetCPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
}

void D3D12DebugManager::PrepareExecuteIndirectPatching(const GPUAddressRangeTracker &origAddresses)
{
  D3D12ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  shaderCache->SetCaching(true);

  HRESULT hr = S_OK;

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        cbvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        cbvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 1),
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 2, 1),
        srvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
    });

    RDCASSERT(root);

    hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                        __uuidof(ID3D12RootSignature), (void **)&m_EIPatchRootSig);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create execute indirect patching RootSig! HRESULT: %s", ToStr(hr).c_str());
    }

    SAFE_RELEASE(root);
  }

  {
    rdcstr mischlsl = GetEmbeddedResource(misc_hlsl);

    ID3DBlob *eiPatchCS;

    shaderCache->GetShaderBlob(mischlsl.c_str(), "RENDERDOC_ExecuteIndirectPatchCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &eiPatchCS);

    RDCASSERT(eiPatchCS);

    D3D12_COMPUTE_PIPELINE_STATE_DESC compPipeDesc = {};
    compPipeDesc.pRootSignature = m_EIPatchRootSig;
    compPipeDesc.CS.BytecodeLength = eiPatchCS->GetBufferSize();
    compPipeDesc.CS.pShaderBytecode = eiPatchCS->GetBufferPointer();

    hr = m_pDevice->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                               (void **)&m_EIPatchPso);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_MeshPickPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    SAFE_RELEASE(eiPatchCS);
  }

  shaderCache->SetCaching(false);

  struct buffermapping
  {
    uint64_t origBase;
    uint64_t origEnd;
    uint64_t newBase;
    uint64_t pad;
  };
  rdcarray<buffermapping> buffers;

  for(const GPUAddressRange &addr : origAddresses.addresses)
  {
    buffermapping b = {};
    b.origBase = addr.start;
    b.origEnd = addr.realEnd;
    b.newBase =
        m_pDevice->GetResourceManager()->GetLiveAs<ID3D12Resource>(addr.id)->GetGPUVirtualAddress();
    buffers.push_back(b);
  }

  m_EIPatchBufferCount = (uint32_t)buffers.size();

  if(!buffers.empty())
  {
    m_EIPatchBufferData = MakeCBuffer(buffers.byteSize());
    FillBuffer(m_EIPatchBufferData, 0, buffers.data(), buffers.byteSize());
  }

  // estimated sizing for scratch buffers:
  // 65536 maxcount
  // 128 bytes command signature
  // = 8MB per EI
  // 64MB = ring for 8 such executes (or many more smaller)
  {
    D3D12_RESOURCE_DESC desc;
    desc.Alignment = 0;
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Height = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Width = 64 * 1024 * 1024;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, NULL,
        __uuidof(ID3D12Resource), (void **)&m_EIPatchScratchBuffer);

    m_EIPatchScratchBuffer->SetName(L"m_EIPatchScratchBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create scratch buffer, HRESULT: %s", ToStr(hr).c_str());
      return;
    }
  }
}

void D3D12DebugManager::GetBufferData(ID3D12Resource *buffer, uint64_t offset, uint64_t length,
                                      bytebuf &ret)
{
  if(buffer == NULL)
    return;

  m_pDevice->GPUSyncAllQueues();

  D3D12_RESOURCE_DESC desc = buffer->GetDesc();
  D3D12_HEAP_PROPERTIES heapProps = {};
  // can't call GetHeapProperties on sparse resources
  if(!m_pDevice->IsSparseResource(GetResID(buffer)))
    buffer->GetHeapProperties(&heapProps, NULL);

  if(offset >= desc.Width)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(length == 0 || length > desc.Width)
  {
    length = desc.Width - offset;
  }

  if(offset + length > desc.Width)
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
    m_pDevice->CheckHRESULT(hr);

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

  D3D12ResourceLayout layout = m_pDevice->GetSubresourceStates(GetResID(buffer))[0];

  barrier.Transition.pResource = buffer;
  barrier.Transition.StateBefore = layout.ToStates();
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  // new layouts guarantee that buffers are safe to use in a new list like this with no barrier, so
  // only barrier for old states
  if(layout.IsStates() && (barrier.Transition.StateBefore & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0)
    m_DebugList->ResourceBarrier(1, &barrier);

  while(length > 0)
  {
    uint64_t chunkSize = RDCMIN(length, m_ReadbackSize);

    m_DebugList->CopyBufferRegion(m_ReadbackBuffer, 0, buffer, offset + outOffs, chunkSize);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();
    m_DebugAlloc->Reset();

    D3D12_RANGE range = {0, (size_t)chunkSize};

    void *data = NULL;
    HRESULT hr = m_ReadbackBuffer->Map(0, &range, &data);
    m_pDevice->CheckHRESULT(hr);

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

  if(layout.IsStates() && (barrier.Transition.StateBefore & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0)
  {
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

    m_DebugList->ResourceBarrier(1, &barrier);
  }

  m_DebugList->Close();

  ID3D12CommandList *l = m_DebugList;
  m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
  m_pDevice->GPUSync();
  m_DebugAlloc->Reset();
}

void D3D12Replay::GeneralMisc::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

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
    readbackDesc.Width = 4096;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                         __uuidof(ID3D12Resource), (void **)&ResultReadbackBuffer);

    ResultReadbackBuffer->SetName(L"m_ResultReadbackBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create readback buffer, HRESULT: %s", ToStr(hr).c_str());
      return;
    }
  }

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
    });

    RDCASSERT(root);

    hr = device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature), (void **)&CheckerboardRootSig);

    SAFE_RELEASE(root);
  }

  {
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

    ID3DBlob *FullscreenVS = NULL;
    ID3DBlob *CheckerboardPS = NULL;

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0", &FullscreenVS);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_CheckerboardPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &CheckerboardPS);

    RDCASSERT(CheckerboardPS);
    RDCASSERT(FullscreenVS);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};

    pipeDesc.pRootSignature = CheckerboardRootSig;
    pipeDesc.VS.BytecodeLength = FullscreenVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = FullscreenVS->GetBufferPointer();
    pipeDesc.PS.BytecodeLength = CheckerboardPS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = CheckerboardPS->GetBufferPointer();
    pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeDesc.SampleMask = 0xFFFFFFFF;
    pipeDesc.SampleDesc.Count = 1;
    pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeDesc.NumRenderTargets = 1;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&CheckerboardPipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_CheckerboardPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    pipeDesc.SampleDesc.Count = D3D12_MSAA_SAMPLECOUNT;

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&CheckerboardMSAAPipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_CheckerboardMSAAPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipeDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;

    for(size_t i = 0; i < ARRAY_COUNT(CheckerboardF16Pipe); i++)
    {
      pipeDesc.SampleDesc.Count = UINT(1 << i);

      D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS check = {};
      check.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      check.SampleCount = pipeDesc.SampleDesc.Count;
      device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &check, sizeof(check));

      if(check.NumQualityLevels == 0)
        continue;

      hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                               (void **)&CheckerboardF16Pipe[i]);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create CheckerboardF16Pipe[%zu]! HRESULT: %s", i, ToStr(hr).c_str());
      }
    }

    SAFE_RELEASE(CheckerboardPS);
    SAFE_RELEASE(FullscreenVS);
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::GeneralMisc::Release()
{
  SAFE_RELEASE(ResultReadbackBuffer);
  SAFE_RELEASE(CheckerboardRootSig);
  SAFE_RELEASE(CheckerboardPipe);
  SAFE_RELEASE(CheckerboardMSAAPipe);
  for(size_t i = 0; i < ARRAY_COUNT(CheckerboardF16Pipe); i++)
    SAFE_RELEASE(CheckerboardF16Pipe[i]);
}

void D3D12Replay::TextureRendering::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        // VS cbuffer
        cbvParam(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0),
        // normal FS cbuffer
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
        // heatmap cbuffer
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 1),
        // display SRVs
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 32),
        // samplers
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 0, 2),
    });

    RDCASSERT(root);

    hr = device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature), (void **)&RootSig);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create tex display RootSig! HRESULT: %s", ToStr(hr).c_str());
    }

    SAFE_RELEASE(root);
  }

  {
    rdcstr hlsl = GetEmbeddedResource(texdisplay_hlsl);

    ID3DBlob *TexDisplayPS = NULL;

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexDisplayVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0", &VS);
    RDCASSERT(VS);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexDisplayPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &TexDisplayPS);
    RDCASSERT(TexDisplayPS);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.pRootSignature = RootSig;
    pipeDesc.VS.BytecodeLength = VS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = VS->GetBufferPointer();
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

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&BlendPipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_TexDisplayBlendPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&SRGBPipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_TexDisplayPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&LinearPipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_TexDisplayPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&F32Pipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_TexDisplayF32Pipe! HRESULT: %s", ToStr(hr).c_str());
    }

    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&F16Pipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_TexDisplayF16Pipe! HRESULT: %s", ToStr(hr).c_str());
    }

    SAFE_RELEASE(TexDisplayPS);

    hlsl = GetEmbeddedResource(texremap_hlsl);

    ID3DBlob *TexRemap[3] = {};
    DXGI_FORMAT formats[3] = {
        DXGI_FORMAT_R8G8B8A8_TYPELESS,
        DXGI_FORMAT_R16G16B16A16_TYPELESS,
        DXGI_FORMAT_R32G32B32A32_TYPELESS,
    };

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexRemapFloat",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &TexRemap[0]);
    RDCASSERT(TexRemap[0]);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexRemapUInt",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &TexRemap[1]);
    RDCASSERT(TexRemap[1]);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexRemapSInt",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &TexRemap[2]);
    RDCASSERT(TexRemap[2]);

    for(int f = 0; f < 3; f++)
    {
      for(int i = 0; i < 3; i++)
      {
        pipeDesc.PS.BytecodeLength = TexRemap[i]->GetBufferSize();
        pipeDesc.PS.pShaderBytecode = TexRemap[i]->GetBufferPointer();

        if(i == 0)
          pipeDesc.RTVFormats[0] = GetFloatTypedFormat(formats[f]);
        else if(i == 1)
          pipeDesc.RTVFormats[0] = GetUIntTypedFormat(formats[f]);
        else
          pipeDesc.RTVFormats[0] = GetSIntTypedFormat(formats[f]);

        hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                 (void **)&m_TexRemapPipe[f][i]);

        if(FAILED(hr))
        {
          RDCERR("Couldn't create m_TexRemapPipe for %s! HRESULT: %s",
                 ToStr(pipeDesc.RTVFormats[0]).c_str(), ToStr(hr).c_str());
        }
      }
    }

    for(int i = 0; i < 3; i++)
      SAFE_RELEASE(TexRemap[i]);
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::TextureRendering::Release()
{
  SAFE_RELEASE(BlendPipe);
  SAFE_RELEASE(SRGBPipe);
  SAFE_RELEASE(LinearPipe);
  SAFE_RELEASE(F16Pipe);
  SAFE_RELEASE(F32Pipe);
  SAFE_RELEASE(RootSig);
  SAFE_RELEASE(VS);
  for(int f = 0; f < 3; f++)
  {
    for(int i = 0; i < 3; i++)
    {
      SAFE_RELEASE(m_TexRemapPipe[f][i]);
    }
  }
}

void D3D12Replay::OverlayRendering::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

  {
    rdcstr meshhlsl = GetEmbeddedResource(mesh_hlsl);

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_TriangleSizeGS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "gs_5_0", &TriangleSizeGS);
    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_TriangleSizePS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &TriangleSizePS);

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               {}, "vs_5_0", &MeshVS);

    rdcstr hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    hlsl = "#define D3D12 1\n\n" + hlsl;

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_1", &QuadOverdrawWritePS);

    // only create DXIL shaders if DXIL was used by the application, since dxc/dxcompiler is really
    // flakey.
    if(device->UsedDXIL())
    {
      shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS",
                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_6_0",
                                 &QuadOverdrawWriteDXILPS);

      if(QuadOverdrawWriteDXILPS == NULL)
      {
        RDCWARN(
            "Couldn't compile DXIL overlay shader at runtime, falling back to baked DXIL shader");

        QuadOverdrawWriteDXILPS = shaderCache->GetQuadShaderDXILBlob();

        if(!QuadOverdrawWriteDXILPS)
        {
          RDCWARN("No fallback DXIL shader available!");
        }
      }
    }
  }

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        // quad overdraw results SRV
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1),
    });

    RDCASSERT(root);

    hr = device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature), (void **)&QuadResolveRootSig);

    SAFE_RELEASE(root);
  }

  {
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);

    ID3DBlob *FullscreenVS = NULL;
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0", &FullscreenVS);
    RDCASSERT(FullscreenVS);

    hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    ID3DBlob *QOResolvePS = NULL;
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QOResolvePS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &QOResolvePS);
    RDCASSERT(QOResolvePS);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.pRootSignature = QuadResolveRootSig;
    pipeDesc.VS.BytecodeLength = FullscreenVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = FullscreenVS->GetBufferPointer();
    pipeDesc.PS.BytecodeLength = QOResolvePS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = QOResolvePS->GetBufferPointer();
    pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeDesc.SampleMask = 0xFFFFFFFF;
    pipeDesc.SampleDesc.Count = 1;
    pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeDesc.NumRenderTargets = 1;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pipeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    for(size_t i = 0; i < ARRAY_COUNT(QuadResolvePipe); i++)
    {
      pipeDesc.SampleDesc.Count = UINT(1 << i);

      D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS check = {};
      check.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      check.SampleCount = pipeDesc.SampleDesc.Count;
      device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &check, sizeof(check));

      if(check.NumQualityLevels == 0)
        continue;

      hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                               (void **)&QuadResolvePipe[i]);

      if(FAILED(hr))
        RDCERR("Couldn't create QuadResolvePipe[%zu]! HRESULT: %s", i, ToStr(hr).c_str());
    }

    SAFE_RELEASE(FullscreenVS);
    SAFE_RELEASE(QOResolvePS);
  }

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        // depth copy SRV
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1),
    });

    RDCASSERT(root);
    hr = device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature),
                                     (void **)&DepthCopyResolveRootSig);
    SAFE_RELEASE(root);
  }

  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.pRootSignature = DepthCopyResolveRootSig;

    ID3DBlob *FullscreenVS = NULL;
    rdcstr hlsl = GetEmbeddedResource(misc_hlsl);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0", &FullscreenVS);
    pipeDesc.VS.BytecodeLength = FullscreenVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = FullscreenVS->GetBufferPointer();

    ID3DBlob *FixedColPS = shaderCache->MakeFixedColShader(D3D12ShaderCache::GREEN);
    pipeDesc.PS.BytecodeLength = FixedColPS->GetBufferSize();
    pipeDesc.PS.pShaderBytecode = FixedColPS->GetBufferPointer();

    pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeDesc.SampleMask = 0xFFFFFFFF;
    pipeDesc.SampleDesc.Count = 1;
    pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeDesc.NumRenderTargets = 1;
    pipeDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pipeDesc.DepthStencilState.DepthEnable = FALSE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeDesc.DepthStencilState.StencilEnable = TRUE;
    pipeDesc.DepthStencilState.StencilReadMask = 0xff;
    pipeDesc.DepthStencilState.StencilWriteMask = 0x0;
    pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
    pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;

    for(size_t f = 0; f < ARRAY_COUNT(DepthResolvePipe); ++f)
    {
      DXGI_FORMAT fmt = (f == 0) ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
      for(size_t i = 0; i < ARRAY_COUNT(DepthResolvePipe[f]); ++i)
      {
        DepthResolvePipe[f][i] = NULL;
        pipeDesc.SampleDesc.Count = UINT(1 << i);

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS check = {};
        check.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        check.SampleCount = pipeDesc.SampleDesc.Count;
        device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &check, sizeof(check));

        if(check.NumQualityLevels == 0)
          continue;

        check.Format = fmt;
        check.SampleCount = pipeDesc.SampleDesc.Count;
        device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &check, sizeof(check));

        if(check.NumQualityLevels == 0)
          continue;

        pipeDesc.DSVFormat = fmt;
        hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                 (void **)&DepthResolvePipe[f][i]);
        if(FAILED(hr))
          RDCERR("Failed to create depth resolve pass overlay pso HRESULT: %s", ToStr(hr).c_str());
      }
    }

    SAFE_RELEASE(FullscreenVS);
    SAFE_RELEASE(FixedColPS);
  }

  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.pRootSignature = DepthCopyResolveRootSig;

    ID3DBlob *FullscreenVS = NULL;
    {
      rdcstr hlsl = GetEmbeddedResource(misc_hlsl);
      shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "vs_5_0", &FullscreenVS);
    }
    pipeDesc.VS.BytecodeLength = FullscreenVS->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = FullscreenVS->GetBufferPointer();

    ID3DBlob *DepthCopyPS = NULL;
    ID3DBlob *DepthCopyMSPS = NULL;
    {
      rdcstr hlsl = GetEmbeddedResource(depth_copy_hlsl);
      shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DepthCopyPS",
                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &DepthCopyPS);
      shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_DepthCopyMSPS",
                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &DepthCopyMSPS);
    }

    pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeDesc.SampleMask = 0xFFFFFFFF;
    pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeDesc.NumRenderTargets = 0;

    // Clear stencil to 0 during the copy
    pipeDesc.DepthStencilState.DepthEnable = TRUE;
    pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeDesc.DepthStencilState.StencilEnable = TRUE;
    pipeDesc.DepthStencilState.StencilReadMask = 0x0;
    pipeDesc.DepthStencilState.StencilWriteMask = 0xff;
    pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_ZERO;
    pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_ZERO;
    pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_ZERO;
    pipeDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeDesc.DepthStencilState.BackFace = pipeDesc.DepthStencilState.FrontFace;
    pipeDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    RDCCOMPILE_ASSERT(ARRAY_COUNT(DepthCopyPipe) == ARRAY_COUNT(DepthResolvePipe),
                      "DepthCopyPipe size must match DepthResolvePipe");
    for(size_t f = 0; f < ARRAY_COUNT(DepthCopyPipe); ++f)
    {
      DXGI_FORMAT fmt = (f == 0) ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
      for(size_t i = 0; i < ARRAY_COUNT(DepthCopyPipe[f]); ++i)
      {
        DepthCopyPipe[f][i] = NULL;
        pipeDesc.SampleDesc.Count = UINT(1 << i);

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS check = {};
        check.Format = fmt;
        check.SampleCount = pipeDesc.SampleDesc.Count;
        device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &check, sizeof(check));

        if(check.NumQualityLevels == 0)
          continue;

        pipeDesc.DSVFormat = fmt;
        if(i == 0)
        {
          pipeDesc.PS.BytecodeLength = DepthCopyPS->GetBufferSize();
          pipeDesc.PS.pShaderBytecode = DepthCopyPS->GetBufferPointer();
        }
        else
        {
          pipeDesc.PS.BytecodeLength = DepthCopyMSPS->GetBufferSize();
          pipeDesc.PS.pShaderBytecode = DepthCopyMSPS->GetBufferPointer();
        }

        hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                 (void **)&DepthCopyPipe[f][i]);
        if(FAILED(hr))
          RDCERR("Failed to create depth overlay depth copy pso HRESULT: %s", ToStr(hr).c_str());
      }
    }
    SAFE_RELEASE(DepthCopyMSPS);
    SAFE_RELEASE(DepthCopyPS);
    SAFE_RELEASE(FullscreenVS);
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::OverlayRendering::Release()
{
  SAFE_RELEASE(MeshVS);
  SAFE_RELEASE(TriangleSizeGS);
  SAFE_RELEASE(TriangleSizePS);
  SAFE_RELEASE(QuadOverdrawWritePS);
  SAFE_RELEASE(QuadOverdrawWriteDXILPS);
  SAFE_RELEASE(QuadResolveRootSig);
  for(size_t i = 0; i < ARRAY_COUNT(QuadResolvePipe); i++)
    SAFE_RELEASE(QuadResolvePipe[i]);

  SAFE_RELEASE(DepthCopyResolveRootSig);
  for(size_t f = 0; f < ARRAY_COUNT(DepthCopyPipe); ++f)
  {
    for(size_t i = 0; i < ARRAY_COUNT(DepthCopyPipe[f]); ++i)
    {
      SAFE_RELEASE(DepthCopyPipe[f][i]);
      SAFE_RELEASE(DepthResolvePipe[f][i]);
    }
  }

  SAFE_RELEASE(Texture);
}

void D3D12Replay::VertexPicking::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

  VB = NULL;
  VBSize = 0;

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        cbvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 2),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1),
    });

    RDCASSERT(root);

    hr = device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature), (void **)&RootSig);

    SAFE_RELEASE(root);
  }

  {
    rdcstr meshhlsl = GetEmbeddedResource(mesh_hlsl);

    ID3DBlob *meshPickCS;

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshPickCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &meshPickCS);

    RDCASSERT(meshPickCS);

    D3D12_COMPUTE_PIPELINE_STATE_DESC compPipeDesc = {};
    compPipeDesc.pRootSignature = RootSig;
    compPipeDesc.CS.BytecodeLength = meshPickCS->GetBufferSize();
    compPipeDesc.CS.pShaderBytecode = meshPickCS->GetBufferPointer();

    hr = device->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                            (void **)&Pipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_MeshPickPipe! HRESULT: %s", ToStr(hr).c_str());
    }

    SAFE_RELEASE(meshPickCS);
  }

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
    pickResultDesc.Width = MaxMeshPicks * sizeof(Vec4f) + 64;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &pickResultDesc,
                                         D3D12_RESOURCE_STATE_COMMON, NULL,
                                         __uuidof(ID3D12Resource), (void **)&ResultBuf);

    ResultBuf->SetName(L"m_PickResultBuf");

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
    uavDesc.Buffer.NumElements = MaxMeshPicks;
    uavDesc.Buffer.StructureByteStride = sizeof(Vec4f);

    device->CreateUnorderedAccessView(ResultBuf, ResultBuf, &uavDesc,
                                      debug->GetCPUHandle(PICK_RESULT_UAV));
    device->CreateUnorderedAccessView(ResultBuf, ResultBuf, &uavDesc,
                                      debug->GetUAVClearHandle(PICK_RESULT_UAV));

    // this UAV is used for clearing everything back to 0

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = MaxMeshPicks + 64 / sizeof(Vec4f);
    uavDesc.Buffer.StructureByteStride = 0;

    device->CreateUnorderedAccessView(ResultBuf, NULL, &uavDesc,
                                      debug->GetCPUHandle(PICK_RESULT_CLEAR_UAV));
    device->CreateUnorderedAccessView(ResultBuf, NULL, &uavDesc,
                                      debug->GetUAVClearHandle(PICK_RESULT_CLEAR_UAV));
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::VertexPicking::Release()
{
  SAFE_RELEASE(IB);
  SAFE_RELEASE(VB);
  SAFE_RELEASE(ResultBuf);
  SAFE_RELEASE(RootSig);
  SAFE_RELEASE(Pipe);
}

void D3D12Replay::PixelPicking::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

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

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &pickPixelDesc,
                                         D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
                                         __uuidof(ID3D12Resource), (void **)&Texture);

    Texture->SetName(L"m_PickPixelTex");

    if(FAILED(hr))
    {
      RDCERR("Failed to create rendering texture for pixel picking, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = debug->GetCPUHandle(PICK_PIXEL_RTV);

    device->CreateRenderTargetView(Texture, NULL, rtv);
  }
}

void D3D12Replay::PixelPicking::Release()
{
  SAFE_RELEASE(Texture);
}

void D3D12Replay::PixelHistory::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

  rdcstr hlsl = GetEmbeddedResource(d3d12_pixelhistory_hlsl);

  shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PrimitiveIDPS",
                             D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &PrimitiveIDPS);

  for(int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
  {
    rdcstr hlsl_variant = "#define RT " + ToStr(i) + "\n" + hlsl;
    shaderCache->GetShaderBlob(hlsl_variant.c_str(), "RENDERDOC_PixelHistoryFixedColPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_0", &FixedColorPS[i]);
  }

  // only create DXIL shaders if DXIL was used by the application to reduce the chance of failure
  if(device->UsedDXIL())
  {
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_PrimitiveIDPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_6_0", &PrimitiveIDPSDxil);

    if(PrimitiveIDPSDxil == NULL)
    {
      RDCWARN(
          "Couldn't compile DXIL Pixel History Primitive ID shader at runtime, falling back to "
          "baked DXIL shader");

      PrimitiveIDPSDxil = shaderCache->GetPrimitiveIDShaderDXILBlob();
      if(!PrimitiveIDPSDxil)
      {
        RDCWARN("No fallback DXIL shader available!");
      }
    }
    for(int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
      rdcstr hlsl_variant = "#define RT " + ToStr(i) + "\n" + hlsl;
      shaderCache->GetShaderBlob(hlsl_variant.c_str(), "RENDERDOC_PixelHistoryFixedColPS",
                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_6_0", &FixedColorPSDxil[i]);
      if(FixedColorPSDxil[i] == NULL)
      {
        RDCWARN(
            "Couldn't compile DXIL Pixel History Fixed Color %d shader at runtime, falling back to "
            "baked DXIL shader",
            i);

        FixedColorPSDxil[i] = shaderCache->GetFixedColorShaderDXILBlob(i);
        if(!FixedColorPSDxil[i])
        {
          RDCWARN("No fallback DXIL shader available!");
        }
      }
    }
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::PixelHistory::Release()
{
  SAFE_RELEASE(PrimitiveIDPS);
  SAFE_RELEASE(PrimitiveIDPSDxil);
  for(int i = 0; i < ARRAY_COUNT(FixedColorPS); ++i)
    SAFE_RELEASE(FixedColorPS[i]);
  for(int i = 0; i < ARRAY_COUNT(FixedColorPSDxil); ++i)
    SAFE_RELEASE(FixedColorPSDxil[i]);
}

void D3D12Replay::HistogramMinMax::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

  {
    ID3DBlob *root = shaderCache->MakeRootSig({
        cbvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        // texture SRVs
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 32),
        // samplers
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 0, 2),
        // UAVs
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 3),
    });

    RDCASSERT(root);

    hr = device->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                     __uuidof(ID3D12RootSignature), (void **)&HistogramRootSig);

    SAFE_RELEASE(root);
  }

  {
    rdcstr histogramhlsl = GetEmbeddedResource(histogram_hlsl);

    D3D12_COMPUTE_PIPELINE_STATE_DESC compPipeDesc = {};

    compPipeDesc.pRootSignature = HistogramRootSig;

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

        rdcstr hlsl = rdcstr("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
        hlsl += rdcstr("#define SHADER_BASETYPE ") + ToStr(i) + "\n";
        hlsl += histogramhlsl;

        shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TileMinMaxCS",
                                   D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &tile);

        compPipeDesc.CS.BytecodeLength = tile->GetBufferSize();
        compPipeDesc.CS.pShaderBytecode = tile->GetBufferPointer();

        hr = device->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                (void **)&TileMinMaxPipe[t][i]);

        if(FAILED(hr))
        {
          RDCERR("Couldn't create m_TileMinMaxPipe! HRESULT: %s", ToStr(hr).c_str());
        }

        shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_HistogramCS",
                                   D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &histogram);

        compPipeDesc.CS.BytecodeLength = histogram->GetBufferSize();
        compPipeDesc.CS.pShaderBytecode = histogram->GetBufferPointer();

        hr = device->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                (void **)&HistogramPipe[t][i]);

        if(FAILED(hr))
        {
          RDCERR("Couldn't create m_HistogramPipe! HRESULT: %s", ToStr(hr).c_str());
        }

        if(t == 1)
        {
          shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS",
                                     D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "cs_5_0", &result);

          compPipeDesc.CS.BytecodeLength = result->GetBufferSize();
          compPipeDesc.CS.pShaderBytecode = result->GetBufferPointer();

          hr = device->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                  (void **)&ResultMinMaxPipe[i]);

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

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &minmaxDesc,
                                         D3D12_RESOURCE_STATE_COMMON, NULL,
                                         __uuidof(ID3D12Resource), (void **)&MinMaxTileBuffer);

    MinMaxTileBuffer->SetName(L"m_MinMaxTileBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create tile buffer for min/max, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE uav = debug->GetCPUHandle(MINMAX_TILE_UAVS);

    D3D12_UNORDERED_ACCESS_VIEW_DESC tileDesc = {};
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    tileDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    tileDesc.Buffer.FirstElement = 0;
    tileDesc.Buffer.NumElements = UINT(minmaxDesc.Width / sizeof(Vec4f));

    device->CreateUnorderedAccessView(MinMaxTileBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    device->CreateUnorderedAccessView(MinMaxTileBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    device->CreateUnorderedAccessView(MinMaxTileBuffer, NULL, &tileDesc, uav);

    uav = debug->GetCPUHandle(HISTOGRAM_UAV);

    // re-use the tile buffer for histogram
    tileDesc.Format = DXGI_FORMAT_R32_UINT;
    tileDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
    device->CreateUnorderedAccessView(MinMaxTileBuffer, NULL, &tileDesc, uav);
    device->CreateUnorderedAccessView(MinMaxTileBuffer, NULL, &tileDesc,
                                      debug->GetUAVClearHandle(HISTOGRAM_UAV));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = UINT(minmaxDesc.Width / sizeof(Vec4f));

    D3D12_CPU_DESCRIPTOR_HANDLE srv = debug->GetCPUHandle(MINMAX_TILE_SRVS);

    device->CreateShaderResourceView(MinMaxTileBuffer, &srvDesc, srv);

    srv.ptr += sizeof(D3D12Descriptor);
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;

    device->CreateShaderResourceView(MinMaxTileBuffer, &srvDesc, srv);

    srv.ptr += sizeof(D3D12Descriptor);
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;

    device->CreateShaderResourceView(MinMaxTileBuffer, &srvDesc, srv);

    minmaxDesc.Width = 2 * sizeof(Vec4f);

    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &minmaxDesc,
                                         D3D12_RESOURCE_STATE_COMMON, NULL,
                                         __uuidof(ID3D12Resource), (void **)&MinMaxResultBuffer);

    MinMaxResultBuffer->SetName(L"m_MinMaxResultBuffer");

    if(FAILED(hr))
    {
      RDCERR("Failed to create result buffer for min/max, HRESULT: %s", ToStr(hr).c_str());
      return;
    }

    uav = debug->GetCPUHandle(MINMAX_RESULT_UAVS);

    tileDesc.Buffer.NumElements = 2;

    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    device->CreateUnorderedAccessView(MinMaxResultBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    device->CreateUnorderedAccessView(MinMaxResultBuffer, NULL, &tileDesc, uav);

    uav.ptr += sizeof(D3D12Descriptor);
    tileDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    device->CreateUnorderedAccessView(MinMaxResultBuffer, NULL, &tileDesc, uav);
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::HistogramMinMax::Release()
{
  SAFE_RELEASE(HistogramRootSig);
  for(int t = RESTYPE_TEX1D; t <= RESTYPE_TEX2D_MS; t++)
  {
    for(int i = 0; i < 3; i++)
    {
      SAFE_RELEASE(TileMinMaxPipe[t][i]);
      SAFE_RELEASE(HistogramPipe[t][i]);
      if(t == RESTYPE_TEX1D)
        SAFE_RELEASE(ResultMinMaxPipe[i]);
    }
  }
  SAFE_RELEASE(MinMaxResultBuffer);
  SAFE_RELEASE(MinMaxTileBuffer);
}

uint32_t GetFreeRegSpace(const D3D12RootSignature &sig, const uint32_t registerSpace,
                         D3D12DescriptorType type, D3D12_SHADER_VISIBILITY visibility)
{
  // This function is used when root signature elements need to be added to a specific register
  // space, such as for debug overlays. We can't remove elements from the root signature entirely
  // because then then the root signature indices wouldn't match up as expected. Instead we see if a
  // given desired register space is unused (which can be referenced in pre-compiled shaders), and
  // if not return an unused space for use instead.
  uint32_t maxSpace = 0;
  bool usedDesiredSpace = false;

  size_t numParams = sig.Parameters.size();
  for(size_t i = 0; i < numParams; i++)
  {
    if(sig.Parameters[i].ShaderVisibility == visibility ||
       sig.Parameters[i].ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
    {
      D3D12_ROOT_PARAMETER_TYPE rootType = sig.Parameters[i].ParameterType;
      if(rootType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        size_t numRanges = sig.Parameters[i].ranges.size();
        for(size_t r = 0; r < numRanges; r++)
        {
          D3D12_DESCRIPTOR_RANGE_TYPE rangeType = sig.Parameters[i].ranges[r].RangeType;
          if(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV && type == D3D12DescriptorType::CBV)
          {
            maxSpace = RDCMAX(maxSpace, sig.Parameters[i].ranges[r].RegisterSpace);
            usedDesiredSpace |= (sig.Parameters[i].ranges[r].RegisterSpace == registerSpace);
          }
          else if(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV && type == D3D12DescriptorType::SRV)
          {
            maxSpace = RDCMAX(maxSpace, sig.Parameters[i].ranges[r].RegisterSpace);
            usedDesiredSpace |= (sig.Parameters[i].ranges[r].RegisterSpace == registerSpace);
          }
          else if(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV && type == D3D12DescriptorType::UAV)
          {
            maxSpace = RDCMAX(maxSpace, sig.Parameters[i].ranges[r].RegisterSpace);
            usedDesiredSpace |= (sig.Parameters[i].ranges[r].RegisterSpace == registerSpace);
          }
        }
      }
      else if(rootType == D3D12_ROOT_PARAMETER_TYPE_CBV && type == D3D12DescriptorType::CBV)
      {
        maxSpace = RDCMAX(maxSpace, sig.Parameters[i].Descriptor.RegisterSpace);
        usedDesiredSpace |= (sig.Parameters[i].Descriptor.RegisterSpace == registerSpace);
      }
      else if(rootType == D3D12_ROOT_PARAMETER_TYPE_SRV && type == D3D12DescriptorType::SRV)
      {
        maxSpace = RDCMAX(maxSpace, sig.Parameters[i].Descriptor.RegisterSpace);
        usedDesiredSpace |= (sig.Parameters[i].Descriptor.RegisterSpace == registerSpace);
      }
      else if(rootType == D3D12_ROOT_PARAMETER_TYPE_UAV && type == D3D12DescriptorType::UAV)
      {
        maxSpace = RDCMAX(maxSpace, sig.Parameters[i].Descriptor.RegisterSpace);
        usedDesiredSpace |= (sig.Parameters[i].Descriptor.RegisterSpace == registerSpace);
      }
    }
  }

  if(usedDesiredSpace)
    return maxSpace + 1;

  return registerSpace;
}

void AddDebugDescriptorsToRenderState(WrappedID3D12Device *pDevice, D3D12RenderState &rs,
                                      const rdcarray<PortableHandle> &handles,
                                      D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t sigElem,
                                      std::set<ResourceId> &copiedHeaps)
{
  if(rs.graphics.sigelems.size() <= sigElem)
    rs.graphics.sigelems.resize(sigElem + 1);

  PortableHandle newHandle = handles[0];

  // If a CBV_SRV_UAV heap is already set and hasn't had a debug descriptor copied in,
  // copy the desired descriptor in and add the heap to the set of heaps that have had
  // a debug descriptor set. If there's no available heapOtherwise we can set our own heap.

  // It is the responsibility of the caller to keep track of the set of copied heaps to
  // avoid overwriting another debug descriptor that may be needed.

  for(size_t i = 0; i < rs.heaps.size(); i++)
  {
    WrappedID3D12DescriptorHeap *h =
        pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(rs.heaps[i]);
    if(h->GetDesc().Type == heapType)
    {
      // use the last descriptors
      D3D12_CPU_DESCRIPTOR_HANDLE dst = h->GetCPUDescriptorHandleForHeapStart();
      dst.ptr += (h->GetDesc().NumDescriptors - handles.size()) * sizeof(D3D12Descriptor);

      newHandle = ToPortableHandle(dst);

      if(copiedHeaps.find(rs.heaps[i]) == copiedHeaps.end())
      {
        for(size_t j = 0; j < handles.size(); ++j)
        {
          WrappedID3D12DescriptorHeap *h2 =
              pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(
                  handles[j].heap);
          D3D12_CPU_DESCRIPTOR_HANDLE src = h2->GetCPUDescriptorHandleForHeapStart();
          src.ptr += handles[j].index * sizeof(D3D12Descriptor);

          // can't do a copy because the src heap is CPU write-only (shader visible). So instead,
          // create directly
          D3D12Descriptor *srcDesc = (D3D12Descriptor *)src.ptr;
          srcDesc->Create(heapType, pDevice, dst);
          dst.ptr += sizeof(D3D12Descriptor);
        }

        copiedHeaps.insert(rs.heaps[i]);
      }

      break;
    }
  }

  if(newHandle.heap == handles[0].heap)
    rs.heaps.push_back(handles[0].heap);

  rs.graphics.sigelems[sigElem] =
      D3D12RenderState::SignatureElement(eRootTable, newHandle.heap, newHandle.index);
}
