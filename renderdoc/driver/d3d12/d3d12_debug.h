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

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColor(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void SetOutputDimensions(int w, int h, DXGI_FORMAT fmt)
  {
    m_width = w;
    m_height = h;

    if(fmt == DXGI_FORMAT_B8G8R8A8_UNORM)
      m_BBFmtIdx = BGRA8_BACKBUFFER;
    else if(fmt == DXGI_FORMAT_R16G16B16A16_FLOAT)
      m_BBFmtIdx = RGBA16_BACKBUFFER;
    else if(fmt == DXGI_FORMAT_R32G32B32A32_FLOAT)
      m_BBFmtIdx = RGBA32_BACKBUFFER;
    else
      m_BBFmtIdx = RGBA8_BACKBUFFER;
  }
  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }
  void RenderText(ID3D12GraphicsCommandList *list, float x, float y, const char *textfmt, ...);

  void RenderHighlightBox(float w, float h, float scale);

  void RenderCheckerboard(Vec3f light, Vec3f dark);
  bool RenderTexture(TextureDisplay cfg, bool blendAlpha);
  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);

  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventID, const vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  void FillCBufferVariables(const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, bool flattenVec4s,
                            const vector<byte> &data);

  void InitPostVSBuffers(uint32_t eventID);

  // indicates that EID alias is the same as eventID
  void AliasPostVSBuffers(uint32_t eventID, uint32_t alias) { m_PostVSAlias[alias] = eventID; }
  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t length, vector<byte> &retData);
  void GetBufferData(ID3D12Resource *buff, uint64_t offset, uint64_t length, vector<byte> &retData);

  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void BuildShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                   ResourceId *id, string *errors);

  D3D12_CPU_DESCRIPTOR_HANDLE AllocRTV();
  void FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle);

  static D3D12RootSignature GetRootSig(const void *data, size_t dataSize);
  static ID3DBlob *MakeRootSig(const std::vector<D3D12_ROOT_PARAMETER1> params,
                               D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
                               UINT NumStaticSamplers = 0,
                               const D3D12_STATIC_SAMPLER_DESC *StaticSamplers = NULL);
  static ID3DBlob *MakeRootSig(const D3D12RootSignature &rootsig);

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

  static const int FONT_TEX_WIDTH = 256;
  static const int FONT_TEX_HEIGHT = 128;
  static const int FONT_MAX_CHARS = 256;

  // how much character space is in the ring buffer
  static const int FONT_BUFFER_CHARS = 8192;

  // baked indices in descriptor heaps
  enum CBVUAVSRVSlot
  {
    FIRST_TEXDISPLAY_SRV = 0,
    FONT_SRV = 128,
    MINMAX_TILE_SRVS,

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

  // indices for the pipelines, for the three possible backbuffer formats
  enum BackBufferFormat
  {
    BGRA8_BACKBUFFER = 0,
    RGBA8_BACKBUFFER,
    RGBA8_SRGB_BACKBUFFER,
    RGBA16_BACKBUFFER,
    RGBA32_BACKBUFFER,
    FMTNUM_BACKBUFFER,
  } m_BBFmtIdx;

  struct FontData
  {
    FontData() { RDCEraseMem(this, sizeof(FontData)); }
    ~FontData()
    {
      SAFE_RELEASE(Tex);
      SAFE_RELEASE(RootSig);
      SAFE_RELEASE(GlyphData);
      SAFE_RELEASE(CharBuffer);

      for(size_t i = 0; i < ARRAY_COUNT(Constants); i++)
        SAFE_RELEASE(Constants[i]);
      for(int i = 0; i < ARRAY_COUNT(Pipe); i++)
        SAFE_RELEASE(Pipe[i]);
    }

    ID3D12Resource *Tex;
    ID3D12PipelineState *Pipe[FMTNUM_BACKBUFFER];
    ID3D12RootSignature *RootSig;
    ID3D12Resource *Constants[20];
    ID3D12Resource *GlyphData;
    ID3D12Resource *CharBuffer;

    size_t CharOffset;
    size_t ConstRingIdx;

    float CharAspect;
    float CharSize;
  } m_Font;

  ID3D12DescriptorHeap *cbvsrvuavHeap;
  ID3D12DescriptorHeap *uavClearHeap;
  ID3D12DescriptorHeap *samplerHeap;
  ID3D12DescriptorHeap *rtvHeap;
  ID3D12DescriptorHeap *dsvHeap;

  ID3D12Resource *m_RingConstantBuffer;
  UINT64 m_RingConstantOffset;

  ID3D12PipelineState *m_TexDisplayPipe;
  ID3D12PipelineState *m_TexDisplayLinearPipe;
  ID3D12PipelineState *m_TexDisplayF32Pipe;
  ID3D12PipelineState *m_TexDisplayBlendPipe;
  ID3DBlob *m_GenericVS;

  ID3D12RootSignature *m_TexDisplayRootSig;

  ID3D12RootSignature *m_CBOnlyRootSig;
  ID3D12PipelineState *m_CheckerboardPipe;
  ID3D12PipelineState *m_CheckerboardMSAAPipe;
  ID3D12PipelineState *m_OutlinePipe;

  ID3DBlob *m_QuadOverdrawWritePS;
  ID3D12RootSignature *m_QuadResolveRootSig;
  ID3D12PipelineState *m_QuadResolvePipe;

  ID3D12Resource *m_PickPixelTex;
  D3D12_CPU_DESCRIPTOR_HANDLE m_PickPixelRTV;

  ID3D12RootSignature *m_MeshPickRootSig;
  ID3D12PipelineState *m_MeshPickPipe;

  ID3D12RootSignature *m_HistogramRootSig;
  // one per texture type, one per int/uint/float
  ID3D12PipelineState *m_TileMinMaxPipe[10][3];
  ID3D12PipelineState *m_HistogramPipe[10][3];
  // one per int/uint/float
  ID3D12PipelineState *m_ResultMinMaxPipe[3];
  ID3D12Resource *m_MinMaxResultBuffer;
  ID3D12Resource *m_MinMaxTileBuffer;

  ID3D12GraphicsCommandList *m_DebugList;
  ID3D12CommandAllocator *m_DebugAlloc;
  ID3D12Resource *m_ReadbackBuffer;

  ID3DBlob *m_MeshVS;
  ID3DBlob *m_MeshGS;
  ID3DBlob *m_MeshPS;
  ID3DBlob *m_TriangleSizeGS;
  ID3DBlob *m_TriangleSizePS;

  ID3D12Resource *m_TexResource;

  ID3D12Resource *m_OverlayRenderTex;
  ResourceId m_OverlayResourceId;

  static const uint64_t m_ReadbackSize = 16 * 1024 * 1024;

  static const uint32_t m_ShaderCacheMagic = 0xbaafd1d1;
  static const uint32_t m_ShaderCacheVersion = 1;

  bool m_ShaderCacheDirty, m_CacheShaders;
  map<uint32_t, ID3DBlob *> m_ShaderCache;

  void FillCBufferVariables(const string &prefix, size_t &offset, bool flatten,
                            const vector<DXBC::CBufferVariable> &invars,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  void RenderTextInternal(ID3D12GraphicsCommandList *list, float x, float y, const char *text);
  bool RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg, bool blendAlpha);

  void PrepareTextureSampling(ID3D12Resource *resource, CompType typeHint, int &resType,
                              vector<D3D12_RESOURCE_BARRIER> &barriers);

  string GetShaderBlob(const char *source, const char *entry, const uint32_t compileFlags,
                       const char *profile, ID3DBlob **srcblob);
  ID3DBlob *MakeFixedColShader(float overlayConsts[4]);

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
  ID3D12Resource *m_PickVB;
  uint32_t m_PickSize;
  ID3D12Resource *m_PickResultBuf;

  uint64_t m_SOBufferSize = 128;
  ID3D12Resource *m_SOBuffer = NULL;
  ID3D12Resource *m_SOStagingBuffer = NULL;
  ID3D12Resource *m_SOPatchedIndexBuffer = NULL;
  ID3D12QueryHeap *m_SOQueryHeap = NULL;

  map<uint32_t, D3D12PostVSData> m_PostVSData;
  map<uint32_t, uint32_t> m_PostVSAlias;

  ID3D12Resource *m_CustomShaderTex;
  ResourceId m_CustomShaderResourceId;

  HighlightCache m_HighlightCache;

  int m_width, m_height;

  uint64_t m_OutputWindowID;
  uint64_t m_DSVID;
  uint64_t m_CurrentOutputWindow;
  map<uint64_t, OutputWindow> m_OutputWindows;

  WrappedID3D12Device *m_WrappedDevice;
  ID3D12Device *m_Device;

  IDXGIFactory4 *m_pFactory;

  D3D12ResourceManager *m_ResourceManager;
};
