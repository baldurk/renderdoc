/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "vk_common.h"
#include "vk_info.h"

#if ENABLED(RDOC_WIN32)

#include <windows.h>
#define WINDOW_HANDLE_DECL HWND wnd;
#define WINDOW_HANDLE_INIT wnd = NULL;

#elif ENABLED(RDOC_ANDROID)

#define WINDOW_HANDLE_DECL ANativeWindow *wnd;
#define WINDOW_HANDLE_INIT wnd = NULL;

#elif ENABLED(RDOC_LINUX)

#if ENABLED(RDOC_XLIB)

#define WINDOW_HANDLE_XLIB \
  struct                   \
  {                        \
    Display *display;      \
    Drawable window;       \
  } xlib;

#else

#define WINDOW_HANDLE_XLIB \
  struct                   \
  {                        \
  } xlib;

#endif

#if ENABLED(RDOC_XCB)

#define WINDOW_HANDLE_XCB         \
  struct                          \
  {                               \
    xcb_connection_t *connection; \
    xcb_window_t window;          \
  } xcb;

#else

#define WINDOW_HANDLE_XCB \
  struct                  \
  {                       \
  } xcb;

#endif

#define WINDOW_HANDLE_DECL \
  WINDOW_HANDLE_XLIB       \
  WINDOW_HANDLE_XCB

#define WINDOW_HANDLE_INIT \
  RDCEraseEl(xlib);        \
  RDCEraseEl(xcb);

#elif ENABLED(RDOC_APPLE) || ENABLED(RDOC_GGP)

#define WINDOW_HANDLE_DECL void *wnd;
#define WINDOW_HANDLE_INIT wnd = NULL;

#else

#error "Unknown platform"

#endif

#include <map>

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define VULKANNOTIMP(...)                                \
  do                                                     \
  {                                                      \
    static bool msgprinted = false;                      \
    if(!msgprinted)                                      \
      RDCDEBUG("Vulkan not implemented - " __VA_ARGS__); \
    msgprinted = true;                                   \
  } while((void)0, 0)

#define MSAA_MESH_VIEW OPTION_ON

#if ENABLED(MSAA_MESH_VIEW)
#define VULKAN_MESH_VIEW_SAMPLES VK_SAMPLE_COUNT_4_BIT
#else
#define VULKAN_MESH_VIEW_SAMPLES VK_SAMPLE_COUNT_1_BIT
#endif

class AMDCounters;
class WrappedVulkan;
class VulkanDebugManager;
class VulkanResourceManager;
struct VulkanStatePipeline;
struct VulkanAMDDrawCallback;

struct VulkanPostVSData
{
  struct InstData
  {
    uint32_t numVerts = 0;
    uint32_t bufOffset = 0;
  };

  struct StageData
  {
    VkBuffer buf;
    VkDeviceMemory bufmem;
    VkPrimitiveTopology topo;

    int32_t baseVertex;

    uint32_t numVerts;
    uint32_t vertStride;
    uint32_t instStride;

    // complex case - expansion per instance
    std::vector<InstData> instData;

    uint32_t numViews;

    bool useIndices;
    VkBuffer idxbuf;
    VkDeviceMemory idxbufmem;
    VkIndexType idxFmt;

    bool hasPosOut;

    float nearPlane;
    float farPlane;
  } vsin, vsout, gsout;

  VulkanPostVSData()
  {
    RDCEraseEl(vsin);
    RDCEraseEl(vsout);
    RDCEraseEl(gsout);
  }

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

struct BindIdx
{
  uint32_t set, bind, arrayidx;

  bool operator<(const BindIdx &o) const
  {
    if(set != o.set)
      return set < o.set;
    else if(bind != o.bind)
      return bind < o.bind;
    return arrayidx < o.arrayidx;
  }

  bool operator>(const BindIdx &o) const
  {
    if(set != o.set)
      return set > o.set;
    else if(bind != o.bind)
      return bind > o.bind;
    return arrayidx > o.arrayidx;
  }

  bool operator==(const BindIdx &o) const
  {
    return set == o.set && bind == o.bind && arrayidx == o.arrayidx;
  }
};

struct DynamicUsedBinds
{
  bool compute = false, valid = false;
  std::vector<BindIdx> used;
};

class VulkanReplay : public IReplayDriver
{
public:
  VulkanReplay();

  void SetRGP(AMDRGPControl *rgp) { m_RGP = rgp; }
  void SetProxy(bool p) { m_Proxy = p; }
  bool IsRemoteProxy() { return m_Proxy; }
  void Shutdown();

  void CreateResources();
  void DestroyResources();

  void SetDriver(WrappedVulkan *d) { m_pDriver = d; }
  DriverInformation GetDriverInfo() { return m_DriverInfo; }
  APIProperties GetAPIProperties();

  ResourceDescription &GetResourceDesc(ResourceId id);
  const std::vector<ResourceDescription> &GetResources();

  std::vector<ResourceId> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  std::vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  ShaderReflection *GetShader(ResourceId shader, ShaderEntryPoint entry);

  std::vector<std::string> GetDisassemblyTargets();
  std::string DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                const std::string &target);

  std::vector<EventUsage> GetUsage(ResourceId id);

  FrameRecord GetFrameRecord();
  std::vector<DebugMessage> GetDebugMessages();

  void SavePipelineState(uint32_t eventId);
  const D3D11Pipe::State *GetD3D11PipelineState() { return NULL; }
  const D3D12Pipe::State *GetD3D12PipelineState() { return NULL; }
  const GLPipe::State *GetGLPipelineState() { return NULL; }
  const VKPipe::State *GetVulkanPipelineState() { return &m_VulkanPipelineState; }
  void FreeTargetResource(ResourceId id);

  ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
  const SDFile &GetStructuredFile();

  std::vector<uint32_t> GetPassEvents(uint32_t eventId);

  std::vector<WindowingSystem> GetSupportedWindowSystems();

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

  ResourceId GetLiveID(ResourceId id);

  std::vector<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  std::vector<CounterResult> FetchCounters(const std::vector<GPUCounter> &counters);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    std::vector<uint32_t> &histogram);

  void InitPostVSBuffers(uint32_t eventId);
  void InitPostVSBuffers(const std::vector<uint32_t> &passEvents);

  // indicates that EID alias is the same as eventId
  void AliasPostVSBuffers(uint32_t eventId, uint32_t alias) { m_PostVS.Alias[alias] = eventId; }
  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                              MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData);
  void GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                      const GetTextureDataParams &params, bytebuf &data);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  void RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings()
  {
    return {ShaderEncoding::SPIRV, ShaderEncoding::GLSL};
  }
  rdcarray<ShaderEncoding> GetTargetShaderEncodings()
  {
    return {ShaderEncoding::SPIRV, ShaderEncoding::GLSL};
  }
  void BuildTargetShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors);
  void BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source, const std::string &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId *id,
                         std::string *errors);
  void FreeCustomShader(ResourceId id);

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

  ResourceId RenderOverlay(ResourceId cfg, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventId, const std::vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);
  bool NeedRemapForFetch(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  bool IsRenderOutput(ResourceId id);

  void FileChanged();

  void InitCallstackResolver();
  bool HasCallstacks();
  Callstack::StackResolver *GetCallstackResolver();

  // used for vulkan layer bookkeeping. Ideally this should all be handled by installers/packages,
  // but for developers running builds locally or just in case, we need to be able to update the
  // layer registration ourselves.
  // These functions are defined in vk_<platform>.cpp
  static bool CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                               std::vector<std::string> &otherJSONs);
  static void InstallVulkanLayer(bool systemLevel);
  void GetInitialDriverVersion();
  void SetDriverInformation(const VkPhysicalDeviceProperties &props);

  AMDCounters *GetAMDCounters() { return m_pAMDCounters; }
private:
  void FetchShaderFeedback(uint32_t eventId);
  void ClearFeedbackCache();

  void PatchReservedDescriptors(const VulkanStatePipeline &pipe, VkDescriptorPool &descpool,
                                std::vector<VkDescriptorSetLayout> &setLayouts,
                                std::vector<VkDescriptorSet> &descSets,
                                VkShaderStageFlagBits patchedBindingStage,
                                const VkDescriptorSetLayoutBinding *newBindings,
                                size_t newBindingsCount);

  void FetchVSOut(uint32_t eventId);
  void FetchTessGSOut(uint32_t eventId);
  void ClearPostVSCache();

  bool RenderTextureInternal(TextureDisplay cfg, VkRenderPassBeginInfo rpbegin, int flags);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, bool stencil, float *minval, float *maxval);

  VulkanDebugManager *GetDebugManager();
  VulkanResourceManager *GetResourceManager();

  struct OutputWindow
  {
    OutputWindow();

    void Create(WrappedVulkan *driver, VkDevice device, bool depth);
    void Destroy(WrappedVulkan *driver, VkDevice device);

    // implemented in vk_replay_platform.cpp
    void CreateSurface(VkInstance inst);
    void SetWindowHandle(WindowingData window);

    WindowingSystem m_WindowSystem;

    WINDOW_HANDLE_DECL;

    bool fresh;

    uint32_t width, height;

    bool hasDepth;

    int failures;
    int recreatePause;

    VkSurfaceKHR surface;
    VkSwapchainKHR swap;
    uint32_t numImgs;
    VkImage colimg[8];
    VkImageMemoryBarrier colBarrier[8];

    VkImage bb;
    VkImageView bbview;
    VkDeviceMemory bbmem;
    VkImageMemoryBarrier bbBarrier;
    VkFramebuffer fb, fbdepth;
    VkRenderPass rp, rpdepth;
    uint32_t curidx;

    VkImage resolveimg;
    VkDeviceMemory resolvemem;

    VkImage dsimg;
    VkDeviceMemory dsmem;
    VkImageView dsview;
    VkImageMemoryBarrier depthBarrier;

    VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
    VulkanResourceManager *m_ResourceManager;
  };

  std::map<uint64_t, OutputWindow> m_OutputWindows;
  uint64_t m_OutputWinID;
  uint64_t m_ActiveWinID;
  bool m_BindDepth;
  uint32_t m_DebugWidth, m_DebugHeight;

  HighlightCache m_HighlightCache;

  bool m_Proxy;

  WrappedVulkan *m_pDriver = NULL;
  VkDevice m_Device = VK_NULL_HANDLE;

  enum TexDisplayFlags
  {
    eTexDisplay_F16Render = 0x1,
    eTexDisplay_F32Render = 0x2,
    eTexDisplay_BlendAlpha = 0x4,
    eTexDisplay_MipShift = 0x8,
    eTexDisplay_GreenOnly = 0x10,
  };

  // General use/misc items that are used in many places
  struct GeneralMisc
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkSampler PointSampler = VK_NULL_HANDLE;
  } m_General;

  struct TextureDisplayViews
  {
    CompType typeHint;
    VkFormat castedFormat;    // the format after applying the above type hint

    // for a color/depth-only textures, views[0] is the view and views[1] and views[2] are NULL
    // for stencil-only textures similarly, views[0] is the view and views[1] and views[2] are NULL
    // for a depth-stencil texture, views[0] is depth, views[1] is stencil and views[2] is NULL
    // for a YUV texture, each views[i] is a plane, and the remainder are NULL
    VkImageView views[3];
  };

  struct TextureRendering
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    VkDescriptorSetLayout DescSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout PipeLayout = VK_NULL_HANDLE;
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipeline BlendPipeline = VK_NULL_HANDLE;
    VkPipeline F16Pipeline = VK_NULL_HANDLE;
    VkPipeline F32Pipeline = VK_NULL_HANDLE;

    VkPipeline PipelineGreenOnly = VK_NULL_HANDLE;
    VkPipeline F16PipelineGreenOnly = VK_NULL_HANDLE;
    VkPipeline F32PipelineGreenOnly = VK_NULL_HANDLE;
    GPUBuffer UBO;
    GPUBuffer HeatmapUBO;

    // ring buffered to allow multiple texture renders between flushes
    VkDescriptorSet DescSet[16] = {VK_NULL_HANDLE};
    uint32_t NextSet = 0;

    VkSampler PointSampler = VK_NULL_HANDLE;
    VkSampler LinearSampler = VK_NULL_HANDLE;

    // descriptors must be valid even if they're skipped dynamically in the shader, so we create
    // tiny (but valid) dummy images to fill in the rest of the descriptors
    VkImage DummyImages[14] = {VK_NULL_HANDLE};
    VkImageView DummyImageViews[14] = {VK_NULL_HANDLE};
    VkWriteDescriptorSet DummyWrites[14] = {};
    VkDescriptorImageInfo DummyInfos[14] = {};
    VkDeviceMemory DummyMemory = VK_NULL_HANDLE;
    VkSampler DummySampler = VK_NULL_HANDLE;

    std::map<ResourceId, TextureDisplayViews> TextureViews;

    VkDescriptorSet GetDescSet()
    {
      NextSet = (NextSet + 1) % ARRAY_COUNT(DescSet);
      return DescSet[NextSet];
    }
  } m_TexRender;

  struct OverlayRendering
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    VkDeviceMemory ImageMem = VK_NULL_HANDLE;
    VkDeviceSize ImageMemSize = 0;
    VkImage Image = VK_NULL_HANDLE;
    VkExtent2D ImageDim = {0, 0};
    VkImageView ImageView = VK_NULL_HANDLE;
    VkFramebuffer NoDepthFB = VK_NULL_HANDLE;
    VkRenderPass NoDepthRP = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_CheckerDescSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_CheckerPipeLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_CheckerDescSet = VK_NULL_HANDLE;
    VkPipeline m_CheckerPipeline = VK_NULL_HANDLE;
    VkPipeline m_CheckerMSAAPipeline = VK_NULL_HANDLE;
    VkPipeline m_CheckerF16Pipeline[8] = {VK_NULL_HANDLE};
    GPUBuffer m_CheckerUBO;

    VkDescriptorSetLayout m_QuadDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_QuadDescSet = VK_NULL_HANDLE;
    VkPipelineLayout m_QuadResolvePipeLayout = VK_NULL_HANDLE;
    VkPipeline m_QuadResolvePipeline[8] = {VK_NULL_HANDLE};

    GPUBuffer m_TriSizeUBO;
    VkDescriptorSetLayout m_TriSizeDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_TriSizeDescSet = VK_NULL_HANDLE;
    VkPipelineLayout m_TriSizePipeLayout = VK_NULL_HANDLE;
  } m_Overlay;

  struct MeshRendering
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    GPUBuffer UBO;
    GPUBuffer BBoxVB;
    GPUBuffer AxisFrustumVB;

    VkDescriptorSetLayout DescSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout PipeLayout = VK_NULL_HANDLE;
    VkDescriptorSet DescSet = VK_NULL_HANDLE;
  } m_MeshRender;

  struct VertexPicking
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    static constexpr int MaxMeshPicks = 500;

    GPUBuffer UBO;
    GPUBuffer IB;
    GPUBuffer IBUpload;
    GPUBuffer VB;
    GPUBuffer VBUpload;
    uint32_t IBSize = 0, VBSize = 0;
    GPUBuffer Result, ResultReadback;
    VkDescriptorSetLayout DescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet DescSet = VK_NULL_HANDLE;
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkPipeline Pipeline = VK_NULL_HANDLE;
  } m_VertexPick;

  struct PixelPicking
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    VkDeviceMemory ImageMem = VK_NULL_HANDLE;
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    GPUBuffer ReadbackBuffer;
    VkFramebuffer FB = VK_NULL_HANDLE;
    VkRenderPass RP = VK_NULL_HANDLE;
  } m_PixelPick;

  struct HistogramMinMax
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    // tile result buffer
    GPUBuffer m_MinMaxTileResult;
    // Vec4f[2] final result buffer
    GPUBuffer m_MinMaxResult;
    GPUBuffer m_MinMaxReadback;
    // uint32_t * num buckets buffer
    GPUBuffer m_HistogramBuf;
    GPUBuffer m_HistogramReadback;
    VkDescriptorSetLayout m_HistogramDescSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_HistogramPipeLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_HistogramDescSet[2] = {VK_NULL_HANDLE};
    GPUBuffer m_HistogramUBO;
    // float, uint, sint for each of 1D, 2D, 3D, 2DMS
    VkPipeline m_HistogramPipe[5][3] = {{VK_NULL_HANDLE}};
    VkPipeline m_MinMaxTilePipe[5][3] = {{VK_NULL_HANDLE}};
    // float, uint, sint
    VkPipeline m_MinMaxResultPipe[3] = {VK_NULL_HANDLE};
  } m_Histogram;

  struct PostVS
  {
    void Destroy(WrappedVulkan *driver);

    VkQueryPool XFBQueryPool = VK_NULL_HANDLE;
    uint32_t XFBQueryPoolSize = 0;

    std::map<uint32_t, VulkanPostVSData> Data;
    std::map<uint32_t, uint32_t> Alias;
  } m_PostVS;

  struct Feedback
  {
    void Destroy(WrappedVulkan *driver);

    GPUBuffer FeedbackBuffer;

    std::map<uint32_t, DynamicUsedBinds> Usage;
  } m_BindlessFeedback;

  std::vector<ResourceDescription> m_Resources;
  std::map<ResourceId, size_t> m_ResourceIdx;

  VKPipe::State m_VulkanPipelineState;

  DriverInformation m_DriverInfo;

  void CreateTexImageView(VkImage liveIm, const VulkanCreationInfo::Image &iminfo,
                          CompType typeHint, TextureDisplayViews &views);

  void FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex, std::vector<uint32_t> *eventIDs);

  std::vector<CounterResult> FetchCountersAMD(const std::vector<GPUCounter> &counters);

  AMDCounters *m_pAMDCounters = NULL;
  AMDRGPControl *m_RGP = NULL;

  VulkanAMDDrawCallback *m_pAMDDrawCallback = NULL;
};
