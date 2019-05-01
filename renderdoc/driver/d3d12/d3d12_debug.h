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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"

class WrappedID3D12Device;
class D3D12ResourceManager;
struct D3D12Descriptor;

#define D3D12_MSAA_SAMPLECOUNT 4

// baked indices in descriptor heaps
enum CBVUAVSRVSlot
{
  FIRST_TEXDISPLAY_SRV = 0,
  MINMAX_TILE_SRVS = 128,

  MINMAX_TILE_UAVS = MINMAX_TILE_SRVS + 3,
  MINMAX_RESULT_UAVS = MINMAX_TILE_UAVS + 3,
  HISTOGRAM_UAV = MINMAX_RESULT_UAVS + 3,

  OVERDRAW_SRV,
  OVERDRAW_UAV,
  STREAM_OUT_UAV,

  PICK_IB_SRV,
  PICK_VB_SRV,
  PICK_RESULT_UAV,
  PICK_RESULT_CLEAR_UAV,

  TMP_UAV,

  MSAA_SRV2x,
  MSAA_SRV4x,
  MSAA_SRV8x,
  MSAA_SRV16x,
  MSAA_SRV32x,
  STENCIL_MSAA_SRV2x,
  STENCIL_MSAA_SRV4x,
  STENCIL_MSAA_SRV8x,
  STENCIL_MSAA_SRV16x,
  STENCIL_MSAA_SRV32x,

  MAX_SRV_SLOT,
};

enum RTVSlot
{
  PICK_PIXEL_RTV,
  CUSTOM_SHADER_RTV,
  OVERLAY_RTV,
  GET_TEX_RTV,
  MSAA_RTV,
  FIRST_TMP_RTV,
  LAST_TMP_RTV = FIRST_TMP_RTV + 16,
  FIRST_WIN_RTV,
};

enum SamplerSlot
{
  POINT_SAMP,
  FIRST_SAMP = POINT_SAMP,
  LINEAR_SAMP,
};

enum DSVSlot
{
  OVERLAY_DSV,
  MSAA_DSV,
  TMP_DSV,
  FIRST_WIN_DSV,
};

struct MeshDisplayPipelines
{
  enum
  {
    ePipe_Wire = 0,
    ePipe_WireDepth,
    ePipe_Solid,
    ePipe_SolidDepth,
    ePipe_Lit,
    ePipe_Secondary,
    ePipe_Count,
  };

  ID3D12PipelineState *pipes[ePipe_Count] = {};
  ID3D12RootSignature *rootsig = NULL;
};

class D3D12DebugManager
{
public:
  D3D12DebugManager(WrappedID3D12Device *wrapper);
  ~D3D12DebugManager();

  void GetBufferData(ID3D12Resource *buff, uint64_t offset, uint64_t length, bytebuf &retData);

  ID3D12Resource *MakeCBuffer(UINT64 size);
  void FillBuffer(ID3D12Resource *buf, size_t offset, const void *data, size_t size);
  D3D12_GPU_VIRTUAL_ADDRESS UploadConstants(const void *data, size_t size);

  ID3D12RootSignature *GetMeshRootSig() { return m_MeshRootSig; }
  ID3D12GraphicsCommandList4 *ResetDebugList();
  void ResetDebugAlloc();

  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(CBVUAVSRVSlot slot);
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(RTVSlot slot);
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(DSVSlot slot);

  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(CBVUAVSRVSlot slot);
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(RTVSlot slot);
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(DSVSlot slot);
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(SamplerSlot slot);

  D3D12_CPU_DESCRIPTOR_HANDLE GetTempDescriptor(const D3D12Descriptor &desc, size_t idx = 0);

  void SetDescriptorHeaps(ID3D12GraphicsCommandList *list, bool cbvsrvuav, bool samplers);

  D3D12_CPU_DESCRIPTOR_HANDLE GetUAVClearHandle(CBVUAVSRVSlot slot);

  void PrepareTextureSampling(ID3D12Resource *resource, CompType typeHint, int &resType,
                              std::vector<D3D12_RESOURCE_BARRIER> &barriers);

  MeshDisplayPipelines CacheMeshDisplayPipelines(const MeshFormat &primary,
                                                 const MeshFormat &secondary);

  void CopyTex2DMSToArray(ID3D12Resource *destArray, ID3D12Resource *srcMS);
  void CopyArrayToTex2DMS(ID3D12Resource *destMS, ID3D12Resource *srcArray, UINT selectedSlice);

private:
  WrappedID3D12Device *m_pDevice = NULL;

  // heaps
  ID3D12DescriptorHeap *cbvsrvuavHeap = NULL;
  ID3D12DescriptorHeap *uavClearHeap = NULL;
  ID3D12DescriptorHeap *samplerHeap = NULL;
  ID3D12DescriptorHeap *rtvHeap = NULL;
  ID3D12DescriptorHeap *dsvHeap = NULL;

  // PrepareTextureSampling
  ID3D12Resource *m_TexResource = NULL;

  // UploadConstants
  ID3D12Resource *m_RingConstantBuffer = NULL;
  UINT64 m_RingConstantOffset = 0;

  // CacheMeshDisplayPipelines
  ID3DBlob *m_MeshVS = NULL;
  ID3DBlob *m_MeshGS = NULL;
  ID3DBlob *m_MeshPS = NULL;
  ID3D12RootSignature *m_MeshRootSig = NULL;
  std::map<uint64_t, MeshDisplayPipelines> m_CachedMeshPipelines;

  // GetBufferData
  static const uint64_t m_ReadbackSize = 16 * 1024 * 1024;

  ID3D12Resource *m_ReadbackBuffer = NULL;

  // Array <-> MSAA copying
  ID3D12RootSignature *m_ArrayMSAARootSig = NULL;
  ID3DBlob *m_FullscreenVS = NULL;

  ID3DBlob *m_IntMS2Array = NULL;
  ID3DBlob *m_FloatMS2Array = NULL;
  ID3DBlob *m_DepthMS2Array = NULL;

  ID3DBlob *m_IntArray2MS = NULL;
  ID3DBlob *m_FloatArray2MS = NULL;
  ID3DBlob *m_DepthArray2MS = NULL;

  // Debug lists
  ID3D12GraphicsCommandList4 *m_DebugList = NULL;
  ID3D12CommandAllocator *m_DebugAlloc = NULL;
};
