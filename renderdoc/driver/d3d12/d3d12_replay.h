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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"
#include "d3d12_state.h"

class AMDCounters;
struct D3D12AMDActionCallback;
class WrappedID3D12Device;

class NVD3D12Counters;

class D3D12DebugManager;

struct PortableHandle;

enum TexDisplayFlags
{
  eTexDisplay_None = 0,
  eTexDisplay_LinearRender = 0x1,
  eTexDisplay_16Render = 0x2,
  eTexDisplay_32Render = 0x4,
  eTexDisplay_BlendAlpha = 0x8,
  eTexDisplay_RemapFloat = 0x10,
  eTexDisplay_RemapUInt = 0x20,
  eTexDisplay_RemapSInt = 0x40,
};

struct D3D12FeedbackBindIdentifier
{
  size_t rootEl;
  size_t rangeIndex;
  UINT descIndex;
  ShaderStage shaderStage;    // Only used for direct access views
  BindType bindType;          // Only used for direct access views
  bool directAccess;

  bool operator<(const D3D12FeedbackBindIdentifier &o) const
  {
    if(directAccess != o.directAccess)
      return directAccess < o.directAccess;
    if(!directAccess)
    {
      if(rootEl != o.rootEl)
        return rootEl < o.rootEl;
      if(rangeIndex != o.rangeIndex)
        return rangeIndex < o.rangeIndex;
    }
    else
    {
      if(shaderStage != o.shaderStage)
        return shaderStage < o.shaderStage;
      if(bindType != o.bindType)
        return bindType < o.bindType;
    }
    return descIndex < o.descIndex;
  }

  bool operator==(const D3D12FeedbackBindIdentifier &o) const
  {
    return rootEl == o.rootEl && rangeIndex == o.rangeIndex && descIndex == o.descIndex &&
           directAccess == o.directAccess && shaderStage == o.shaderStage && bindType == o.bindType;
  }
};

class D3D12Replay : public IReplayDriver
{
public:
  D3D12Replay(WrappedID3D12Device *d);

  D3D12DevConfiguration *GetDevConfiguration() { return m_DevConfig; }
  void SetDevConfiguration(D3D12DevConfiguration *config) { m_DevConfig = config; }

  D3D12DebugManager *GetDebugManager() { return m_DebugManager; }
  void SetRGP(AMDRGPControl *rgp) { m_RGP = rgp; }
  void Set12On7(bool d3d12on7) { m_D3D12On7 = d3d12on7; }
  void SetProxy(bool proxy) { m_Proxy = proxy; }
  bool IsRemoteProxy() { return m_Proxy; }
  void Initialise(IDXGIFactory1 *factory, D3D12DevConfiguration *config);
  void Shutdown();

  RDResult FatalErrorCheck();
  IReplayDriver *MakeDummyDriver();

  void CreateResources();
  void DestroyResources();
  DriverInformation GetDriverInfo() { return m_DriverInfo; }
  rdcarray<GPUDevice> GetAvailableGPUs();
  APIProperties GetAPIProperties();

  ResourceDescription &GetResourceDesc(ResourceId id);
  rdcarray<ResourceDescription> GetResources();

  rdcarray<DescriptorStoreDescription> GetDescriptorStores();
  void RegisterDescriptorStore(const DescriptorStoreDescription &desc);

  rdcarray<BufferDescription> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  rdcarray<TextureDescription> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  rdcarray<DebugMessage> GetDebugMessages();

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader, ShaderEntryPoint entry);

  rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline);
  rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl, const rdcstr &target);

  rdcarray<EventUsage> GetUsage(ResourceId id);

  FrameRecord &WriteFrameRecord() { return m_FrameRecord; }
  FrameRecord GetFrameRecord() { return m_FrameRecord; }
  void SetPipelineStates(D3D11Pipe::State *d3d11, D3D12Pipe::State *d3d12, GLPipe::State *gl,
                         VKPipe::State *vk)
  {
    m_D3D12PipelineState = d3d12;
  }
  void SavePipelineState(uint32_t eventId);
  rdcarray<Descriptor> GetDescriptors(ResourceId descriptorStore,
                                      const rdcarray<DescriptorRange> &ranges);
  rdcarray<SamplerDescriptor> GetSamplerDescriptors(ResourceId descriptorStore,
                                                    const rdcarray<DescriptorRange> &ranges);
  rdcarray<DescriptorAccess> GetDescriptorAccess(uint32_t eventId);
  rdcarray<DescriptorLogicalLocation> GetDescriptorLocations(ResourceId descriptorStore,
                                                             const rdcarray<DescriptorRange> &ranges);
  void FreeTargetResource(ResourceId id);
  void FreeCustomShader(ResourceId id);

  RDResult ReadLogInitialisation(RDCFile *rdc, bool readStructuredBuffers);
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
  SDFile *GetStructuredFile();

  rdcarray<uint32_t> GetPassEvents(uint32_t eventId);

  rdcarray<WindowingSystem> GetSupportedWindowSystems()
  {
    rdcarray<WindowingSystem> ret;
    ret.push_back(WindowingSystem::Win32);
    return ret;
  }

  AMDRGPControl *GetRGPControl() { return m_RGP; }
  uint64_t MakeOutputWindow(WindowingData window, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void SetOutputWindowDimensions(uint64_t id, int32_t w, int32_t h);
  void GetOutputWindowData(uint64_t id, bytebuf &retData);
  void ClearOutputWindowColor(uint64_t id, FloatVector col);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void InitPostVSBuffers(uint32_t eventId);
  void InitPostMSBuffers(uint32_t eventId);
  void InitPostVSBuffers(const rdcarray<uint32_t> &passEvents);

  // indicates that EID alias is the same as eventId
  void AliasPostVSBuffers(uint32_t eventId, uint32_t alias) { m_PostVSAlias[alias] = eventId; }
  ResourceId GetLiveID(ResourceId id);

  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                 CompType typeCast, float pixel[4]);
  bool GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, float *minval,
                 float *maxval);
  bool GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast, float minval,
                    float maxval, const rdcfixedarray<bool, 4> &channels,
                    rdcarray<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                              MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData);
  void GetTextureData(ResourceId tex, const Subresource &sub, const GetTextureDataParams &params,
                      bytebuf &data);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings()
  {
    return {ShaderEncoding::DXBC, ShaderEncoding::DXIL, ShaderEncoding::HLSL};
  }
  rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes();
  rdcarray<ShaderEncoding> GetTargetShaderEncodings()
  {
    return {ShaderEncoding::DXBC, ShaderEncoding::DXIL, ShaderEncoding::HLSL};
  }
  void BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId &id,
                         rdcstr &errors);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  rdcarray<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data, size_t dataSize);
  bool IsTextureSupported(const TextureDescription &tex);
  bool NeedRemapForFetch(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  void RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg);

  bool RenderTexture(TextureDisplay cfg);

  void RenderCheckerboard(FloatVector dark, FloatVector light);

  void RenderHighlightBox(float w, float h, float scale);

  void FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                            rdcstr entryPoint, uint32_t cbufSlot, rdcarray<ShaderVariable> &outvars,
                            const bytebuf &data);

  rdcarray<PixelModification> PixelHistory(rdcarray<EventUsage> events, ResourceId target, uint32_t x,
                                           uint32_t y, const Subresource &sub, CompType typeCast);
  ShaderDebugTrace *DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid, uint32_t idx,
                                uint32_t view);
  ShaderDebugTrace *DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                               const DebugPixelInputs &inputs);
  ShaderDebugTrace *DebugThread(uint32_t eventId, const rdcfixedarray<uint32_t, 3> &groupid,
                                const rdcfixedarray<uint32_t, 3> &threadid);
  rdcarray<ShaderDebugState> ContinueDebug(ShaderDebugger *debugger);
  void FreeDebugger(ShaderDebugger *debugger);

  uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height, const MeshDisplay &cfg,
                      uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                           uint32_t eventId, const rdcarray<uint32_t> &passEvents);

  void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories);
  void BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId &id,
                         rdcstr &errors);
  ResourceId ApplyCustomShader(TextureDisplay &display);

  RenderOutputSubresource GetRenderOutputSubresource(ResourceId id);
  bool IsRenderOutput(ResourceId id) { return GetRenderOutputSubresource(id).mip != ~0U; }
  void FileChanged() {}
  AMDCounters *GetAMDCounters() { return m_pAMDCounters; }
  void PatchQuadWritePS(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &pipeDesc, uint32_t regSpace,
                        bool dxil);

private:
  void FillDescriptor(Descriptor &dst, const D3D12Descriptor *src);
  void FillRootDescriptor(Descriptor &dst, const D3D12RenderState::SignatureElement &src);
  void FillSamplerDescriptor(SamplerDescriptor &dst, const D3D12_SAMPLER_DESC2 &src);

  bool CreateSOBuffers();
  void ClearPostVSCache();

  bool FetchShaderFeedback(uint32_t eventId);
  void ClearFeedbackCache();

  void RefreshDerivedReplacements();

  void BuildShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                   const ShaderCompileFlags &compileFlags, const rdcarray<rdcstr> &includeDirs,
                   ShaderStage type, ResourceId &id, rdcstr &errors);

  bool RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                             TexDisplayFlags flags);

  void SetOutputDimensions(int w, int h)
  {
    m_OutputWidth = (float)w;
    m_OutputHeight = (float)h;
  }

  struct D3D12DynamicShaderFeedback
  {
    bool compute = false, valid = false;
    rdcarray<DescriptorAccess> access;
  };

  struct Feedback
  {
    ID3D12Resource *FeedbackBuffer = NULL;
    ID3D12QueryHeap *PipeStatsHeap = NULL;

    std::unordered_map<uint32_t, D3D12DynamicShaderFeedback> Usage;
  } m_BindlessFeedback;

  struct D3D12PostVSData
  {
    struct InstData
    {
      union
      {
        uint64_t bufOffset;
        uint32_t numIndices;
        uint32_t ampDispatchSizeX;
      };
      union
      {
        uint32_t numVerts;
        struct
        {
          uint16_t y;
          uint16_t z;
        } ampDispatchSizeYZ;
      };
    };

    struct StageData
    {
      ID3D12Resource *buf = NULL;
      uint64_t bufSize = ~0ULL;
      Topology topo = Topology::Unknown;

      uint32_t vertStride = 0;

      // simple case - uniform
      uint32_t numVerts = 0;
      uint32_t instStride = 0;

      // complex case - expansion per instance,
      // also used for meshlet offsets and sizes
      rdcarray<InstData> instData;

      uint32_t primStride = 0;
      uint64_t primOffset = 0;

      bool useIndices = false;
      ID3D12Resource *idxBuf = NULL;
      uint64_t idxBufSize = ~0ULL;
      uint64_t idxOffset = 0;
      DXGI_FORMAT idxFmt = DXGI_FORMAT_UNKNOWN;

      rdcfixedarray<uint32_t, 3> dispatchSize;

      bool hasPosOut = false;

      float nearPlane = 0.0f;
      float farPlane = 0.0f;

      rdcstr status;
    } vsout, gsout, ampout, meshout;

    const StageData &GetStage(MeshDataStage type)
    {
      if(type == MeshDataStage::VSOut)
        return vsout;
      else if(type == MeshDataStage::GSOut)
        return gsout;
      else if(type == MeshDataStage::TaskOut)
        return ampout;
      else if(type == MeshDataStage::MeshOut)
        return meshout;

      if(type == MeshDataStage::Count)
      {
        if(gsout.buf)
          return gsout;

        if(vsout.buf)
          return vsout;

        if(meshout.buf)
          return meshout;

        return vsout;
      }

      RDCERR("Unexpected mesh data stage!");

      return vsout;
    }
  };

  std::map<uint32_t, D3D12PostVSData> m_PostVSData;
  std::map<uint32_t, uint32_t> m_PostVSAlias;

  uint64_t m_SOBufferSize = 128;
  ID3D12Resource *m_SOBuffer = NULL;
  ID3D12Resource *m_SOStagingBuffer = NULL;
  ID3D12Resource *m_SOPatchedIndexBuffer = NULL;
  ID3D12QueryHeap *m_SOQueryHeap = NULL;

  bool m_Proxy, m_D3D12On7;

  rdcarray<ID3D12Resource *> m_ProxyResources;

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
    D3D12_RESOURCE_DESC bbDesc;

    uint64_t rtvId, dsvId;

    WrappedID3D12Device *dev;

    void MakeRTV(bool multisampled);
    void MakeDSV();

    int width, height;
  };

  float m_OutputWidth = 1.0f;
  float m_OutputHeight = 1.0f;
  D3D12_VIEWPORT m_OutputViewport;

  rdcarray<uint64_t> m_OutputWindowIDs;
  rdcarray<uint64_t> m_DSVIDs;
  uint64_t m_CurrentOutputWindow = 0;
  std::map<uint64_t, OutputWindow> m_OutputWindows;

  HighlightCache m_HighlightCache;

  ID3D12Resource *m_CustomShaderTex = NULL;
  ResourceId m_CustomShaderResourceId;

  // General use/misc items that are used in many places
  struct GeneralMisc
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3D12Resource *ResultReadbackBuffer = NULL;
    ID3D12RootSignature *CheckerboardRootSig = NULL;
    ID3D12PipelineState *CheckerboardPipe = NULL;
    ID3D12PipelineState *CheckerboardMSAAPipe = NULL;
    ID3D12PipelineState *CheckerboardF16Pipe[8] = {NULL};
  } m_General;

  struct TextureRendering
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3DBlob *VS = NULL;
    ID3D12RootSignature *RootSig = NULL;
    ID3D12PipelineState *SRGBPipe = NULL;
    ID3D12PipelineState *LinearPipe = NULL;
    ID3D12PipelineState *F16Pipe = NULL;
    ID3D12PipelineState *F32Pipe = NULL;
    ID3D12PipelineState *BlendPipe = NULL;
    // for each of 8-bit, 16-bit, 32-bit and float, uint, sint
    ID3D12PipelineState *m_TexRemapPipe[3][3] = {};
  } m_TexRender;

  struct OverlayRendering
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3DBlob *MeshVS = NULL;
    ID3DBlob *TriangleSizeGS = NULL;
    ID3DBlob *TriangleSizePS = NULL;
    ID3DBlob *QuadOverdrawWritePS = NULL;
    ID3DBlob *QuadOverdrawWriteDXILPS = NULL;
    ID3D12RootSignature *QuadResolveRootSig = NULL;
    ID3D12PipelineState *QuadResolvePipe[8] = {NULL};
    ID3D12RootSignature *DepthCopyResolveRootSig = NULL;
    ID3D12PipelineState *DepthResolvePipe[2][5] = {};
    ID3D12PipelineState *DepthCopyPipe[2][5] = {};

    ID3D12Resource *Texture = NULL;
    ResourceId resourceId;
  } m_Overlay;

  struct VertexPicking
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    static const uint32_t MaxMeshPicks = 500;
    ID3D12Resource *VB = NULL;
    ID3D12Resource *IB = NULL;
    uint32_t VBSize = 0;
    uint32_t IBSize = 0;
    ID3D12Resource *ResultBuf = NULL;
    ID3D12RootSignature *RootSig = NULL;
    ID3D12PipelineState *Pipe = NULL;
  } m_VertexPick;

  struct PixelPicking
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3D12Resource *Texture = NULL;
  } m_PixelPick;

  struct PixelHistory
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3DBlob *PrimitiveIDPS = NULL;
    ID3DBlob *PrimitiveIDPSDxil = NULL;

    ID3DBlob *FixedColorPS[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {NULL};
    ID3DBlob *FixedColorPSDxil[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {NULL};
  } m_PixelHistory;

  struct HistogramMinMax
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3D12RootSignature *HistogramRootSig = NULL;
    // one per texture type, one per int/uint/float
    ID3D12PipelineState *TileMinMaxPipe[10][3] = {};
    ID3D12PipelineState *HistogramPipe[10][3] = {};
    // one per int/uint/float
    ID3D12PipelineState *ResultMinMaxPipe[3] = {};
    ID3D12Resource *MinMaxResultBuffer = NULL;
    ID3D12Resource *MinMaxTileBuffer = NULL;
  } m_Histogram;

  rdcarray<ResourceDescription> m_Resources;
  rdcarray<DescriptorStoreDescription> m_DescriptorStores;
  std::map<ResourceId, size_t> m_ResourceIdx;

  bool m_ISAChecked = false;
  bool m_ISAAvailable = false;

  D3D12Pipe::State *m_D3D12PipelineState = NULL;

  FrameRecord m_FrameRecord;

  WrappedID3D12Device *m_pDevice = NULL;

  D3D12DebugManager *m_DebugManager = NULL;

  D3D12DevConfiguration *m_DevConfig = NULL;

  IDXGIFactory1 *m_pFactory = NULL;
  HMODULE m_D3D12Lib = NULL;

  AMDCounters *m_pAMDCounters = NULL;
  AMDRGPControl *m_RGP = NULL;

  DriverInformation m_DriverInfo;

  D3D12AMDActionCallback *m_pAMDActionCallback = NULL;

  rdcarray<rdcstr> m_CustomShaderIncludes;

  std::map<rdcfixedarray<uint32_t, 4>, bytebuf> m_PatchedPSCache;

  void FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex, rdcarray<uint32_t> *eventIDs);
  rdcarray<CounterResult> FetchCountersAMD(const rdcarray<GPUCounter> &counters);

  NVD3D12Counters *m_pNVCounters = NULL;
};
