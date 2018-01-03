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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"

class WrappedID3D12Device;
class D3D12ResourceManager;

#define D3D12_MSAA_SAMPLECOUNT 4

class D3D12DebugManager
{
public:
  D3D12DebugManager(WrappedID3D12Device *wrapper);
  ~D3D12DebugManager();

  uint64_t MakeOutputWindow(WindowingData window, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColor(uint64_t id, FloatVector col);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void SetOutputDimensions(int w, int h)
  {
    m_width = w;
    m_height = h;
  }
  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }
  void RenderHighlightBox(float w, float h, float scale);

  void RenderCheckerboard();
  bool RenderTexture(TextureDisplay cfg);
  void RenderMesh(uint32_t eventId, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);

  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventId, const vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  void FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, bool flattenVec4s, const bytebuf &data);

  void InitPostVSBuffers(uint32_t eventId);

  // indicates that EID alias is the same as eventId
  void AliasPostVSBuffers(uint32_t eventId, uint32_t alias) { m_PostVSAlias[alias] = eventId; }
  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage);
  void ClearPostVSCache();

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, bytebuf &retData);
  void GetBufferData(ID3D12Resource *buff, uint64_t offset, uint64_t length, bytebuf &retData);

  void GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                      const GetTextureDataParams &params, bytebuf &data);

  void BuildShader(string source, string entry, const ShaderCompileFlags &compileFlags,
                   ShaderStage type, ResourceId *id, string *errors);

  D3D12_CPU_DESCRIPTOR_HANDLE AllocRTV();
  void FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle);

  ID3DBlob *GetOverdrawWritePS() { return m_QuadOverdrawWritePS; }
private:
  struct OutputWindow
  {
    HWND wnd;
    IDXGISwapChain *swap;
    ID3D12Resource *bb[2];
    uint32_t bbIdx;
    ID3D12Resource *col;
    ID3D12Resource *colResolve;
    ID3D12Resource *depth;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;

    WrappedID3D12Device *dev;

    void MakeRTV(bool multisampled);
    void MakeDSV();

    int width, height;
  };

  ID3D12Resource *MakeCBuffer(UINT64 size);
  void FillBuffer(ID3D12Resource *buf, size_t offset, const void *data, size_t size);
  D3D12_GPU_VIRTUAL_ADDRESS UploadConstants(const void *data, size_t size);

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
  };

  enum RTVSlot
  {
    PICK_PIXEL_RTV,
    CUSTOM_SHADER_RTV,
    OVERLAY_RTV,
    GET_TEX_RTV,
    FIRST_WIN_RTV,
  };

  enum DSVSlot
  {
    OVERLAY_DSV,
    FIRST_WIN_DSV,
  };

  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(CBVUAVSRVSlot slot);
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(RTVSlot slot);
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(DSVSlot slot);

  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(CBVUAVSRVSlot slot);
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(RTVSlot slot);
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(DSVSlot slot);

  D3D12_CPU_DESCRIPTOR_HANDLE GetUAVClearHandle(CBVUAVSRVSlot slot);

  ID3D12DescriptorHeap *cbvsrvuavHeap = NULL;
  ID3D12DescriptorHeap *uavClearHeap = NULL;
  ID3D12DescriptorHeap *samplerHeap = NULL;
  ID3D12DescriptorHeap *rtvHeap = NULL;
  ID3D12DescriptorHeap *dsvHeap = NULL;

  ID3D12Resource *m_RingConstantBuffer = NULL;
  UINT64 m_RingConstantOffset = 0;

  ID3D12PipelineState *m_TexDisplayPipe = NULL;
  ID3D12PipelineState *m_TexDisplayLinearPipe = NULL;
  ID3D12PipelineState *m_TexDisplayF32Pipe = NULL;
  ID3D12PipelineState *m_TexDisplayBlendPipe = NULL;
  ID3DBlob *m_GenericVS = NULL;

  ID3D12RootSignature *m_TexDisplayRootSig = NULL;

  ID3D12RootSignature *m_CBOnlyRootSig = NULL;
  ID3D12PipelineState *m_CheckerboardPipe = NULL;
  ID3D12PipelineState *m_CheckerboardMSAAPipe = NULL;
  ID3D12PipelineState *m_OutlinePipe = NULL;

  ID3DBlob *m_QuadOverdrawWritePS = NULL;
  ID3D12RootSignature *m_QuadResolveRootSig = NULL;
  ID3D12PipelineState *m_QuadResolvePipe = NULL;

  ID3D12Resource *m_PickPixelTex = NULL;
  D3D12_CPU_DESCRIPTOR_HANDLE m_PickPixelRTV = {0};

  ID3D12RootSignature *m_MeshPickRootSig = NULL;
  ID3D12PipelineState *m_MeshPickPipe = NULL;

  ID3D12RootSignature *m_HistogramRootSig = NULL;
  // one per texture type, one per int/uint/float
  ID3D12PipelineState *m_TileMinMaxPipe[10][3] = {};
  ID3D12PipelineState *m_HistogramPipe[10][3] = {};
  // one per int/uint/float
  ID3D12PipelineState *m_ResultMinMaxPipe[3] = {};
  ID3D12Resource *m_MinMaxResultBuffer = NULL;
  ID3D12Resource *m_MinMaxTileBuffer = NULL;

  ID3D12GraphicsCommandList *m_DebugList = NULL;
  ID3D12CommandAllocator *m_DebugAlloc = NULL;
  ID3D12Resource *m_ReadbackBuffer = NULL;

  ID3DBlob *m_MeshVS = NULL;
  ID3DBlob *m_MeshGS = NULL;
  ID3DBlob *m_MeshPS = NULL;
  ID3DBlob *m_TriangleSizeGS = NULL;
  ID3DBlob *m_TriangleSizePS = NULL;

  ID3D12Resource *m_TexResource = NULL;

  ID3D12Resource *m_OverlayRenderTex = NULL;
  ResourceId m_OverlayResourceId;

  static const uint64_t m_ReadbackSize = 16 * 1024 * 1024;

  void FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                            const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, const bytebuf &data);

  enum TexDisplayFlags
  {
    eTexDisplay_None = 0,
    eTexDisplay_LinearRender = 0x1,
    eTexDisplay_F32Render = 0x2,
    eTexDisplay_BlendAlpha = 0x4,
  };

  bool RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                             TexDisplayFlags flags);

  void PrepareTextureSampling(ID3D12Resource *resource, CompType typeHint, int &resType,
                              vector<D3D12_RESOURCE_BARRIER> &barriers);

  void CreateSOBuffers();

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

    ID3D12PipelineState *pipes[ePipe_Count];
  };

  struct D3D12PostVSData
  {
    struct InstData
    {
      uint32_t numVerts = 0;
      uint64_t bufOffset = 0;
    };

    struct StageData
    {
      ID3D12Resource *buf = NULL;
      D3D_PRIMITIVE_TOPOLOGY topo = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

      uint32_t vertStride = 0;

      // simple case - uniform
      uint32_t numVerts = 0;
      uint32_t instStride = 0;

      // complex case - expansion per instance
      std::vector<InstData> instData;

      bool useIndices = false;
      ID3D12Resource *idxBuf = NULL;
      DXGI_FORMAT idxFmt = DXGI_FORMAT_UNKNOWN;

      bool hasPosOut = false;

      float nearPlane = 0.0f;
      float farPlane = 0.0f;
    } vsin, vsout, gsout;

    const StageData &GetStage(MeshDataStage type)
    {
      if(type == MeshDataStage::VSOut)
        return vsout;
      else if(type == MeshDataStage::GSOut)
        return gsout;
      else
        RDCERR("Unexpected mesh data stage!");

      return vsin;
    }
  };

  MeshDisplayPipelines CacheMeshDisplayPipelines(const MeshFormat &primary,
                                                 const MeshFormat &secondary);

  map<uint64_t, MeshDisplayPipelines> m_CachedMeshPipelines;

  static const uint32_t m_MaxMeshPicks = 500;
  ID3D12Resource *m_PickVB = NULL;
  uint32_t m_PickSize = 0;
  ID3D12Resource *m_PickResultBuf = NULL;

  uint64_t m_SOBufferSize = 128;
  ID3D12Resource *m_SOBuffer = NULL;
  ID3D12Resource *m_SOStagingBuffer = NULL;
  ID3D12Resource *m_SOPatchedIndexBuffer = NULL;
  ID3D12QueryHeap *m_SOQueryHeap = NULL;

  map<uint32_t, D3D12PostVSData> m_PostVSData;
  map<uint32_t, uint32_t> m_PostVSAlias;

  ID3D12Resource *m_CustomShaderTex = NULL;
  ResourceId m_CustomShaderResourceId;

  HighlightCache m_HighlightCache;

  int m_width = 1, m_height = 1;

  uint64_t m_OutputWindowID = 0;
  uint64_t m_DSVID = 0;
  uint64_t m_CurrentOutputWindow = 0;
  map<uint64_t, OutputWindow> m_OutputWindows;

  WrappedID3D12Device *m_WrappedDevice = NULL;
  ID3D12Device *m_Device = NULL;

  IDXGIFactory4 *m_pFactory = NULL;

  D3D12ResourceManager *m_ResourceManager = NULL;
};
