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

#include <memory>
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"
#include "d3d12_state.h"

class AMDCounters;
struct D3D12AMDDrawCallback;
class WrappedID3D12Device;

class D3D12DebugManager;

struct PortableHandle;

enum TexDisplayFlags
{
  eTexDisplay_None = 0,
  eTexDisplay_LinearRender = 0x1,
  eTexDisplay_F16Render = 0x2,
  eTexDisplay_F32Render = 0x4,
  eTexDisplay_BlendAlpha = 0x8,
};

class D3D12Replay : public IReplayDriver
{
public:
  D3D12Replay();

  D3D12DebugManager *GetDebugManager() { return m_DebugManager; }
  void SetRGP(AMDRGPControl *rgp) { m_RGP = rgp; }
  void SetProxy(bool proxy) { m_Proxy = proxy; }
  bool IsRemoteProxy() { return m_Proxy; }
  void Initialise();
  void Shutdown();

  void SetDevice(WrappedID3D12Device *d) { m_pDevice = d; }
  void CreateResources();
  void DestroyResources();
  DriverInformation GetDriverInfo() { return m_DriverInfo; }
  APIProperties GetAPIProperties();

  ResourceDescription &GetResourceDesc(ResourceId id);
  const std::vector<ResourceDescription> &GetResources();

  std::vector<ResourceId> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  std::vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  std::vector<DebugMessage> GetDebugMessages();

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  ShaderReflection *GetShader(ResourceId shader, ShaderEntryPoint entry);

  std::vector<std::string> GetDisassemblyTargets();
  std::string DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                const std::string &target);

  std::vector<EventUsage> GetUsage(ResourceId id);

  FrameRecord GetFrameRecord();

  void SavePipelineState(uint32_t eventId);
  const D3D11Pipe::State *GetD3D11PipelineState() { return NULL; }
  const D3D12Pipe::State *GetD3D12PipelineState() { return &m_PipelineState; }
  const GLPipe::State *GetGLPipelineState() { return NULL; }
  const VKPipe::State *GetVulkanPipelineState() { return NULL; }
  void FreeTargetResource(ResourceId id);
  void FreeCustomShader(ResourceId id);

  ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool readStructuredBuffers);
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
  const SDFile &GetStructuredFile();

  std::vector<uint32_t> GetPassEvents(uint32_t eventId);

  std::vector<WindowingSystem> GetSupportedWindowSystems()
  {
    std::vector<WindowingSystem> ret;
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
  void InitPostVSBuffers(const std::vector<uint32_t> &passEvents);

  // indicates that EID alias is the same as eventId
  void AliasPostVSBuffers(uint32_t eventId, uint32_t alias) { m_PostVSAlias[alias] = eventId; }
  ResourceId GetLiveID(ResourceId id);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    std::vector<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                              MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData);
  void GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                      const GetTextureDataParams &params, bytebuf &data);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings()
  {
    return {ShaderEncoding::DXBC, ShaderEncoding::HLSL};
  }
  rdcarray<ShaderEncoding> GetTargetShaderEncodings()
  {
    return {ShaderEncoding::DXBC, ShaderEncoding::HLSL};
  }
  void BuildTargetShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors);
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  std::vector<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  std::vector<CounterResult> FetchCounters(const std::vector<GPUCounter> &counters);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);
  bool NeedRemapForFetch(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  void RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg);

  bool RenderTexture(TextureDisplay cfg);

  void RenderCheckerboard();

  void RenderHighlightBox(float w, float h, float scale);

  void FillCBufferVariables(ResourceId shader, std::string entryPoint, uint32_t cbufSlot,
                            rdcarray<ShaderVariable> &outvars, const bytebuf &data);

  std::vector<PixelModification> PixelHistory(std::vector<EventUsage> events, ResourceId target,
                                              uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
                                              uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventId, const uint32_t groupid[3],
                               const uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height, const MeshDisplay &cfg,
                      uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventId, const std::vector<uint32_t> &passEvents);

  void BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  bool IsRenderOutput(ResourceId id);

  void FileChanged() {}
  AMDCounters *GetAMDCounters() { return m_pAMDCounters; }
private:
  void FillRegisterSpaces(const D3D12RenderState::RootSignature &rootSig,
                          const ShaderBindpointMapping &mapping,
                          rdcarray<D3D12Pipe::RegisterSpace> &spaces,
                          D3D12_SHADER_VISIBILITY visibility);
  void FillResourceView(D3D12Pipe::View &view, const D3D12Descriptor *desc);

  void ClearPostVSCache();

  void CreateSOBuffers();

  void BuildShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                   const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                   std::string *errors);

  bool RenderTextureInternal(D3D12_CPU_DESCRIPTOR_HANDLE rtv, TextureDisplay cfg,
                             TexDisplayFlags flags);

  void SetOutputDimensions(int w, int h)
  {
    m_OutputWidth = (float)w;
    m_OutputHeight = (float)h;
  }

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

  std::map<uint32_t, D3D12PostVSData> m_PostVSData;
  std::map<uint32_t, uint32_t> m_PostVSAlias;

  uint64_t m_SOBufferSize = 128;
  ID3D12Resource *m_SOBuffer = NULL;
  ID3D12Resource *m_SOStagingBuffer = NULL;
  ID3D12Resource *m_SOPatchedIndexBuffer = NULL;
  ID3D12QueryHeap *m_SOQueryHeap = NULL;

  bool m_Proxy;

  std::vector<ID3D12Resource *> m_ProxyResources;

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
    bool multisampled;
  };

  float m_OutputWidth = 1.0f;
  float m_OutputHeight = 1.0f;

  uint64_t m_OutputWindowID = 1;
  uint64_t m_DSVID = 0;
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
  } m_TexRender;

  struct OverlayRendering
  {
    void Init(WrappedID3D12Device *device, D3D12DebugManager *debug);
    void Release();

    ID3DBlob *MeshVS = NULL;
    ID3DBlob *TriangleSizeGS = NULL;
    ID3DBlob *TriangleSizePS = NULL;
    ID3DBlob *QuadOverdrawWritePS = NULL;
    ID3D12RootSignature *QuadResolveRootSig = NULL;
    ID3D12PipelineState *QuadResolvePipe = NULL;

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

  std::vector<ResourceDescription> m_Resources;
  std::map<ResourceId, size_t> m_ResourceIdx;

  bool m_ISAChecked = false;
  bool m_ISAAvailable = false;

  D3D12Pipe::State m_PipelineState;

  WrappedID3D12Device *m_pDevice = NULL;

  D3D12DebugManager *m_DebugManager = NULL;

  IDXGIFactory4 *m_pFactory = NULL;

  AMDCounters *m_pAMDCounters = NULL;
  AMDRGPControl *m_RGP = NULL;

  DriverInformation m_DriverInfo;

  D3D12AMDDrawCallback *m_pAMDDrawCallback = NULL;

  void FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex, std::vector<uint32_t> *eventIDs);

  std::vector<CounterResult> FetchCountersAMD(const std::vector<GPUCounter> &counters);
};
