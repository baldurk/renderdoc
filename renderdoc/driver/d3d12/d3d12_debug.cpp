/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
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
  range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

  return ret;
}

D3D12DebugManager::D3D12DebugManager(WrappedID3D12Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D12DebugManager));

  m_pDevice = wrapper;

  D3D12ResourceManager *rm = wrapper->GetResourceManager();

  HRESULT hr = S_OK;

  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 1;
  desc.NumDescriptors = 1024;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

  RDCCOMPILE_ASSERT(FIRST_WIN_RTV + 256 < 1024, "Increase size of RTV heap");

  hr = m_pDevice->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&rtvHeap);
  m_pDevice->InternalRef();

  if(FAILED(hr))
  {
    RDCERR("Couldn't create RTV descriptor heap! HRESULT: %s", ToStr(hr).c_str());
  }

  rm->SetInternalResource(rtvHeap);

  desc.NumDescriptors = 64;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

  RDCCOMPILE_ASSERT(FIRST_WIN_DSV + 32 < 64, "Increase size of DSV heap");

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
  sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

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
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    RDCASSERT(root);

    hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                        __uuidof(ID3D12RootSignature), (void **)&m_MeshRootSig);
    m_pDevice->InternalRef();

    SAFE_RELEASE(root);

    rm->SetInternalResource(m_MeshRootSig);
  }

  {
    std::string meshhlsl = GetEmbeddedResource(mesh_hlsl);

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               "vs_5_0", &m_MeshVS);
    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshGS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               "gs_5_0", &m_MeshGS);
    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshPS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               "ps_5_0", &m_MeshPS);
  }

  {
    std::string hlsl = GetEmbeddedResource(misc_hlsl);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0", &m_FullscreenVS);
  }

  {
    std::string multisamplehlsl = GetEmbeddedResource(multisample_hlsl);

    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_CopyMSToArray",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &m_IntMS2Array);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyMSToArray",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &m_FloatMS2Array);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyMSToArray",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &m_DepthMS2Array);

    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_CopyArrayToMS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &m_IntArray2MS);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyArrayToMS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &m_FloatArray2MS);
    shaderCache->GetShaderBlob(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyArrayToMS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &m_DepthArray2MS);
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

  ID3D12GraphicsCommandList *list = NULL;

  hr = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DebugAlloc, NULL,
                                    __uuidof(ID3D12GraphicsCommandList), (void **)&list);
  m_pDevice->InternalRef();

  // safe to upcast - this is a wrapped object
  m_DebugList = (ID3D12GraphicsCommandList4 *)list;

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback command list, HRESULT: %s", ToStr(hr).c_str());
    return;
  }

  rm->SetInternalResource(m_DebugList);

  if(m_DebugList)
    m_DebugList->Close();
}

D3D12DebugManager::~D3D12DebugManager()
{
  for(auto it = m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
    for(size_t p = 0; p < MeshDisplayPipelines::ePipe_Count; p++)
      SAFE_RELEASE(it->second.pipes[p]);

  SAFE_RELEASE(dsvHeap);
  SAFE_RELEASE(rtvHeap);
  SAFE_RELEASE(cbvsrvuavHeap);
  SAFE_RELEASE(uavClearHeap);
  SAFE_RELEASE(samplerHeap);

  SAFE_RELEASE(m_MeshVS);
  SAFE_RELEASE(m_MeshGS);
  SAFE_RELEASE(m_MeshPS);
  SAFE_RELEASE(m_MeshRootSig);

  SAFE_RELEASE(m_ArrayMSAARootSig);
  SAFE_RELEASE(m_FullscreenVS);

  SAFE_RELEASE(m_IntMS2Array);
  SAFE_RELEASE(m_FloatMS2Array);
  SAFE_RELEASE(m_DepthMS2Array);

  SAFE_RELEASE(m_IntArray2MS);
  SAFE_RELEASE(m_FloatArray2MS);
  SAFE_RELEASE(m_DepthArray2MS);

  SAFE_RELEASE(m_ReadbackBuffer);

  SAFE_RELEASE(m_RingConstantBuffer);

  SAFE_RELEASE(m_TexResource);

  SAFE_RELEASE(m_DebugAlloc);
  SAFE_RELEASE(m_DebugList);

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
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

ID3D12GraphicsCommandList4 *D3D12DebugManager::ResetDebugList()
{
  m_DebugList->Reset(m_DebugAlloc, NULL);

  return m_DebugList;
}

void D3D12DebugManager::ResetDebugAlloc()
{
  m_DebugAlloc->Reset();
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
    if(dsvdesc->ViewDimension == D3D12_RTV_DIMENSION_UNKNOWN)
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

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DebugManager::GetUAVClearHandle(CBVUAVSRVSlot slot)
{
  D3D12_CPU_DESCRIPTOR_HANDLE ret = uavClearHeap->GetCPUDescriptorHandleForHeapStart();
  ret.ptr += slot * sizeof(D3D12Descriptor);
  return ret;
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
  barrier.Transition.StateBefore = m_pDevice->GetSubresourceStates(GetResID(buffer))[0];
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

  if(barrier.Transition.StateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE)
    m_DebugList->ResourceBarrier(1, &barrier);

  while(length > 0)
  {
    uint64_t chunkSize = RDCMIN(length, m_ReadbackSize);

    m_DebugList->CopyBufferRegion(m_ReadbackBuffer, 0, buffer, offset, chunkSize);

    m_DebugList->Close();

    ID3D12CommandList *l = m_DebugList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();
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
    std::string hlsl = GetEmbeddedResource(misc_hlsl);

    ID3DBlob *FullscreenVS = NULL;
    ID3DBlob *CheckerboardPS = NULL;

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0", &FullscreenVS);
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_CheckerboardPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &CheckerboardPS);

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
    std::string hlsl = GetEmbeddedResource(texdisplay_hlsl);

    ID3DBlob *TexDisplayPS = NULL;

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexDisplayVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0", &VS);
    RDCASSERT(VS);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TexDisplayPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &TexDisplayPS);
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
}

void D3D12Replay::OverlayRendering::Init(WrappedID3D12Device *device, D3D12DebugManager *debug)
{
  HRESULT hr = S_OK;

  D3D12ShaderCache *shaderCache = device->GetShaderCache();

  shaderCache->SetCaching(true);

  {
    std::string meshhlsl = GetEmbeddedResource(mesh_hlsl);

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_TriangleSizeGS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "gs_5_0", &TriangleSizeGS);
    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_TriangleSizePS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &TriangleSizePS);

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshVS", D3DCOMPILE_WARNINGS_ARE_ERRORS,
                               "vs_5_0", &MeshVS);

    std::string hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &QuadOverdrawWritePS);
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
    std::string hlsl = GetEmbeddedResource(misc_hlsl);

    ID3DBlob *FullscreenVS = NULL;
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_FullscreenVS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "vs_5_0", &FullscreenVS);
    RDCASSERT(FullscreenVS);

    hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    ID3DBlob *QOResolvePS = NULL;
    shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QOResolvePS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &QOResolvePS);
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

    hr = device->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                             (void **)&QuadResolvePipe);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create m_QuadResolvePipe! HRESULT: %s", ToStr(hr).c_str());
    }

    SAFE_RELEASE(FullscreenVS);
    SAFE_RELEASE(QOResolvePS);
  }

  shaderCache->SetCaching(false);
}

void D3D12Replay::OverlayRendering::Release()
{
  SAFE_RELEASE(MeshVS);
  SAFE_RELEASE(TriangleSizeGS);
  SAFE_RELEASE(TriangleSizePS);
  SAFE_RELEASE(QuadOverdrawWritePS);
  SAFE_RELEASE(QuadResolveRootSig);
  SAFE_RELEASE(QuadResolvePipe);

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
    std::string meshhlsl = GetEmbeddedResource(mesh_hlsl);

    ID3DBlob *meshPickCS;

    shaderCache->GetShaderBlob(meshhlsl.c_str(), "RENDERDOC_MeshPickCS",
                               D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0", &meshPickCS);

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
                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
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
    std::string histogramhlsl = GetEmbeddedResource(histogram_hlsl);

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

        std::string hlsl = std::string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
        hlsl += std::string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
        hlsl += std::string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
        hlsl += histogramhlsl;

        shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_TileMinMaxCS",
                                   D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0", &tile);

        compPipeDesc.CS.BytecodeLength = tile->GetBufferSize();
        compPipeDesc.CS.pShaderBytecode = tile->GetBufferPointer();

        hr = device->CreateComputePipelineState(&compPipeDesc, __uuidof(ID3D12PipelineState),
                                                (void **)&TileMinMaxPipe[t][i]);

        if(FAILED(hr))
        {
          RDCERR("Couldn't create m_TileMinMaxPipe! HRESULT: %s", ToStr(hr).c_str());
        }

        shaderCache->GetShaderBlob(hlsl.c_str(), "RENDERDOC_HistogramCS",
                                   D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0", &histogram);

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
                                     D3DCOMPILE_WARNINGS_ARE_ERRORS, "cs_5_0", &result);

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
                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
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
                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
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
