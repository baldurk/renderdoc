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
#include "vk_common.h"
#include "vk_info.h"
#include "vk_state.h"

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

#if ENABLED(RDOC_WAYLAND)

#define WINDOW_HANDLE_WAYLAND \
  struct                      \
  {                           \
    wl_display *display;      \
    wl_surface *window;       \
  } wayland;

#else

#define WINDOW_HANDLE_WAYLAND \
  struct                      \
  {                           \
  } wayland;

#endif

#define WINDOW_HANDLE_DECL \
  WINDOW_HANDLE_XLIB       \
  WINDOW_HANDLE_XCB        \
  WINDOW_HANDLE_WAYLAND

#define WINDOW_HANDLE_INIT \
  RDCEraseEl(xlib);        \
  RDCEraseEl(xcb);         \
  RDCEraseEl(wayland);

#elif ENABLED(RDOC_APPLE)

#define WINDOW_HANDLE_DECL \
  struct                   \
  {                        \
    void *view;            \
    void *layer;           \
  } cocoa;

#define WINDOW_HANDLE_INIT RDCEraseEl(cocoa);

#elif ENABLED(RDOC_GGP)

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

#define VULKAN_MESH_VIEW_SAMPLES VK_SAMPLE_COUNT_4_BIT

class AMDCounters;
class WrappedVulkan;
class VulkanDebugManager;
class VulkanResourceManager;
struct VulkanStatePipeline;
struct VulkanAMDActionCallback;

class NVVulkanCounters;

struct VulkanPostVSData
{
  struct InstData
  {
    union
    {
      uint32_t bufOffset;
      uint32_t numIndices;
      uint32_t taskDispatchSizeX;
    };
    union
    {
      uint32_t numVerts;
      struct
      {
        uint16_t y;
        uint16_t z;
      } taskDispatchSizeYZ;
    };
  };

  struct StageData
  {
    VkBuffer buf;
    VkDeviceMemory bufmem;
    Topology topo;

    int32_t baseVertex;

    uint32_t numVerts;
    uint32_t vertStride;
    uint32_t instStride;

    // complex case - expansion per instance,
    // also used for meshlet offsets and sizes
    rdcarray<InstData> instData;

    uint32_t primStride = 0;
    uint64_t primOffset = 0;

    uint32_t numViews;

    bool useIndices;
    VkBuffer idxbuf;
    uint64_t idxOffset = 0;
    VkDeviceMemory idxbufmem;
    VkIndexType idxFmt;

    rdcfixedarray<uint32_t, 3> dispatchSize;

    bool hasPosOut;
    bool flipY;

    float nearPlane;
    float farPlane;

    rdcstr status;
  } vsout, gsout, taskout, meshout;

  VulkanPostVSData()
  {
    RDCEraseEl(vsout);
    RDCEraseEl(gsout);
    RDCEraseEl(taskout);
    RDCEraseEl(meshout);
  }

  const StageData &GetStage(MeshDataStage type)
  {
    if(type == MeshDataStage::VSOut)
      return vsout;
    else if(type == MeshDataStage::GSOut)
      return gsout;
    else if(type == MeshDataStage::TaskOut)
      return taskout;
    else if(type == MeshDataStage::MeshOut)
      return meshout;

    if(type == MeshDataStage::Count)
    {
      if(gsout.buf != VK_NULL_HANDLE)
        return gsout;

      if(vsout.buf != VK_NULL_HANDLE)
        return vsout;

      if(meshout.buf != VK_NULL_HANDLE)
        return meshout;

      return vsout;
    }

    RDCERR("Unexpected mesh data stage!");

    return vsout;
  }
};

struct VKDynamicShaderFeedback
{
  bool compute = false, valid = false;
  rdcarray<DescriptorAccess> access;
  rdcarray<ShaderMessage> messages;
};

enum TexDisplayFlags
{
  eTexDisplay_16Render = 0x1,
  eTexDisplay_32Render = 0x2,
  eTexDisplay_BlendAlpha = 0x4,
  eTexDisplay_MipShift = 0x8,
  eTexDisplay_GreenOnly = 0x10,
  eTexDisplay_RemapFloat = 0x20,
  eTexDisplay_RemapUInt = 0x40,
  eTexDisplay_RemapSInt = 0x80,
  eTexDisplay_RemapSRGB = 0x100,
};

struct ShaderDebugData
{
  void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
  void Destroy(WrappedVulkan *driver);

  VkDescriptorSetLayout DescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout PipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet DescSet = VK_NULL_HANDLE;

  VkPipeline MathPipe[3] = {};

  VkImage Image = VK_NULL_HANDLE;
  VkImageView ImageView = VK_NULL_HANDLE;
  VkDeviceMemory ImageMemory = VK_NULL_HANDLE;
  VkFramebuffer Framebuffer = VK_NULL_HANDLE;
  VkRenderPass RenderPass = VK_NULL_HANDLE;

  VkDescriptorImageInfo DummyImageInfos[4][6] = {};
  VkWriteDescriptorSet DummyWrites[4][7] = {};

  VkShaderModule Module[7] = {};

  std::map<uint32_t, VkPipeline> m_Pipelines;

  GPUBuffer MathResult;
  GPUBuffer ConstantsBuffer;
  GPUBuffer ReadbackBuffer;
};

class VulkanReplay : public IReplayDriver
{
public:
  VulkanReplay(WrappedVulkan *d);
  virtual ~VulkanReplay() {}
  void SetRGP(AMDRGPControl *rgp) { m_RGP = rgp; }
  void SetProxy(bool p) { m_Proxy = p; }
  bool IsRemoteProxy() { return m_Proxy; }
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

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader, ShaderEntryPoint entry);

  rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline);
  rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl, const rdcstr &target);

  rdcarray<EventUsage> GetUsage(ResourceId id);

  ShaderDebugData &GetShaderDebugData() { return m_ShaderDebugData; }
  FrameRecord &WriteFrameRecord() { return m_FrameRecord; }
  FrameRecord GetFrameRecord() { return m_FrameRecord; }
  rdcarray<DebugMessage> GetDebugMessages();

  void SetPipelineStates(D3D11Pipe::State *d3d11, D3D12Pipe::State *d3d12, GLPipe::State *gl,
                         VKPipe::State *vk)
  {
    m_VulkanPipelineState = vk;
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

  RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
  SDFile *GetStructuredFile();

  rdcarray<uint32_t> GetPassEvents(uint32_t eventId);

  rdcarray<WindowingSystem> GetSupportedWindowSystems();

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

  rdcarray<GPUCounter> EnumerateCounters();
  CounterDescription DescribeCounter(GPUCounter counterID);
  rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters);

  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                 CompType typeCast, float pixel[4]);
  bool GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, float *minval,
                 float *maxval);
  bool GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast, float minval,
                    float maxval, const rdcfixedarray<bool, 4> &channels,
                    rdcarray<uint32_t> &histogram);

  VkDescriptorSet GetPixelHistoryDescriptor();
  void ResetPixelHistoryDescriptorPool();
  void UpdatePixelHistoryDescriptor(VkDescriptorSet descSet, VkBuffer buffer, VkImageView imgView1,
                                    VkImageView imgView2);

  void InitPostVSBuffers(uint32_t eventId);
  void InitPostVSBuffers(uint32_t eventId, VulkanRenderState state);
  void InitPostVSBuffers(const rdcarray<uint32_t> &passEvents);

  // indicates that EID alias is the same as eventId
  void AliasPostVSBuffers(uint32_t eventId, uint32_t alias) { m_PostVS.Alias[alias] = eventId; }
  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                              MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData);
  void GetTextureData(ResourceId tex, const Subresource &sub, const GetTextureDataParams &params,
                      bytebuf &data);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  void RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg);

  rdcarray<ShaderEncoding> GetCustomShaderEncodings()
  {
    return {ShaderEncoding::SPIRV, ShaderEncoding::GLSL};
  }
  rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes();
  rdcarray<ShaderEncoding> GetTargetShaderEncodings()
  {
    return {ShaderEncoding::SPIRV, ShaderEncoding::GLSL};
  }
  void BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId &id,
                         rdcstr &errors);
  void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories);
  void BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId &id,
                         rdcstr &errors);
  void FreeCustomShader(ResourceId id);

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
  ResourceId ApplyCustomShader(TextureDisplay &display);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data, size_t dataSize);
  bool IsTextureSupported(const TextureDescription &tex);
  bool NeedRemapForFetch(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  RenderOutputSubresource GetRenderOutputSubresource(ResourceId id);
  bool IsRenderOutput(ResourceId id) { return GetRenderOutputSubresource(id).mip != ~0U; }
  void FileChanged();

  // used for vulkan layer bookkeeping. Ideally this should all be handled by installers/packages,
  // but for developers running builds locally or just in case, we need to be able to update the
  // layer registration ourselves.
  // These functions are defined in vk_<platform>.cpp
  static bool CheckVulkanLayer(VulkanLayerFlags &flags, rdcarray<rdcstr> &myJSONs,
                               rdcarray<rdcstr> &otherJSONs);
  static void InstallVulkanLayer(bool systemLevel);
  void GetInitialDriverVersion();
  void SetDriverInformation(const VkPhysicalDeviceProperties &props,
                            const VkPhysicalDeviceDriverProperties &driverProps);

  AMDCounters *GetAMDCounters() { return m_pAMDCounters; }
  void CopyPixelForPixelHistory(VkCommandBuffer cmd, VkOffset2D offset, uint32_t sample,
                                uint32_t bufferOffset, VkFormat format, VkDescriptorSet descSet);

  bool Depth3DSupported() { return m_TexRender.DummyImages[3][2] != VK_NULL_HANDLE; }
  bool DepthCubeSupported() { return m_TexRender.DepthCubesSupported; }
private:
  bool FetchShaderFeedback(uint32_t eventId);
  void ClearFeedbackCache();

  void FillDescriptor(Descriptor &dstel, const DescriptorSetSlot &srcel);
  void FillSamplerDescriptor(SamplerDescriptor &dstel, const DescriptorSetSlot &srcel);

  void PatchReservedDescriptors(const VulkanStatePipeline &pipe, VkDescriptorPool &descpool,
                                rdcarray<VkDescriptorSetLayout> &setLayouts,
                                rdcarray<VkDescriptorSet> &descSets,
                                VkShaderStageFlagBits patchedBindingStage,
                                const VkDescriptorSetLayoutBinding *newBindings,
                                size_t newBindingsCount);

  void FetchVSOut(uint32_t eventId, VulkanRenderState &state);
  void FetchTessGSOut(uint32_t eventId, VulkanRenderState &state);
  void FetchMeshOut(uint32_t eventId, VulkanRenderState &state);
  void ClearPostVSCache();

  void RefreshDerivedReplacements();

  bool RenderTextureInternal(TextureDisplay cfg, const ImageState &imageState,
                             VkRenderPassBeginInfo rpbegin, int flags);

  bool GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, bool stencil,
                 float *minval, float *maxval);

  void CheckVkResult(VkResult vkr);
  VulkanDebugManager *GetDebugManager();
  VulkanResourceManager *GetResourceManager();

  struct OutputWindow
  {
    OutputWindow();

    void Create(WrappedVulkan *driver, VkDevice device, bool depth);
    void Destroy(WrappedVulkan *driver, VkDevice device);

    // implemented in vk_replay_platform.cpp
    void CreateSurface(WrappedVulkan *driver, VkInstance inst);
    void SetWindowHandle(WindowingData window);

    WindowingSystem m_WindowSystem;

    WINDOW_HANDLE_DECL;

    bool fresh = true;
    bool outofdate = false;

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

  FrameRecord m_FrameRecord;

  WrappedVulkan *m_pDriver = NULL;
  VkDevice m_Device = VK_NULL_HANDLE;

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
    CompType typeCast;
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
    // for each of 8-bit, 16-bit, 32-bit and float, uint, sint, and for normal/green-only
    VkPipeline RemapPipeline[3][3][2] = {};

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

    // images and views are re-used elsewhere in replay, so index them sensibly
    //
    // [float/uint/sint/depth][1D/2D/3D/MS/Cube]
    //
    // the cube image is re-used from the 2D one, so only the view is valid
    VkImage DummyImages[4][5] = {};
    VkImageView DummyImageViews[4][5] = {};
    VkWriteDescriptorSet DummyWrites[14] = {};
    VkDescriptorImageInfo DummyInfos[14] = {};
    VkSampler DummySampler = VK_NULL_HANDLE;
    VkBuffer DummyBuffer = VK_NULL_HANDLE;
    VkBufferView DummyBufferView[4] = {};
    bool DepthCubesSupported = true;

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
    uint32_t MipLevels = 0, ArrayLayers = 0;
    uint32_t MultiViewMask = 0;
    VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
    VkRenderPass NoDepthRP = VK_NULL_HANDLE;

    // the view and framebuffer must be recreated if the mip changes, even if the image doesn't
    uint32_t ViewMip = ~0U, ViewSlice = ~0U, ViewNumSlices = ~0U;
    VkImageView ImageView = VK_NULL_HANDLE;
    VkFramebuffer NoDepthFB = VK_NULL_HANDLE;

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

    VkDescriptorSetLayout m_DepthCopyDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_DepthCopyDescSet = VK_NULL_HANDLE;
    VkPipelineLayout m_DepthCopyPipeLayout = VK_NULL_HANDLE;
    VkPipeline m_DepthCopyPipeline[2][5];

    VkPipelineLayout m_DepthResolvePipeLayout = VK_NULL_HANDLE;
    VkPipeline m_DepthResolvePipeline[2][5];

    GPUBuffer m_DummyMeshletSSBO;
    GPUBuffer m_TriSizeUBO;
    VkDescriptorSetLayout m_TriSizeDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_TriSizeDescSet = VK_NULL_HANDLE;
    VkPipelineLayout m_TriSizePipeLayout = VK_NULL_HANDLE;

    VkSampler m_PointSampler = VK_NULL_HANDLE;
    VkFormat m_DefaultDepthStencilFormat;
  } m_Overlay;

  struct MeshRendering
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    GPUBuffer UBO;
    GPUBuffer MeshletSSBO;
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

  struct PixelHistory
  {
    void Init(WrappedVulkan *driver, VkDescriptorPool descriptorPool);
    void Destroy(WrappedVulkan *driver);

    VkDescriptorSetLayout MSCopyDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool MSCopyDescPool = VK_NULL_HANDLE;
    VkPipeline MSCopyPipe = VK_NULL_HANDLE;
    VkPipeline MSCopyDepthPipe = VK_NULL_HANDLE;
    VkPipelineLayout MSCopyPipeLayout = VK_NULL_HANDLE;

    rdcarray<VkDescriptorSet> allocedSets;
  } m_PixelHistory;

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

    std::map<uint32_t, VKDynamicShaderFeedback> Usage;
  } m_BindlessFeedback;

  ShaderDebugData m_ShaderDebugData;

  rdcarray<ResourceDescription> m_Resources;
  rdcarray<DescriptorStoreDescription> m_DescriptorStores;
  std::map<ResourceId, size_t> m_ResourceIdx;

  VKPipe::State *m_VulkanPipelineState = NULL;

  DriverInformation m_DriverInfo;

  struct PipelineExecutables
  {
    VkShaderStageFlags stages;
    rdcstr name, description;
    uint32_t subgroupSize;
    rdcarray<VkPipelineExecutableStatisticKHR> statistics;
    rdcarray<VkPipelineExecutableInternalRepresentationKHR> representations;

    // internal data, pointed to from representations above
    rdcarray<bytebuf> irbytes;
  };

  std::map<ResourceId, rdcarray<PipelineExecutables>> m_PipelineExecutables;

  void CachePipelineExecutables(ResourceId pipeline);

  void CreateTexImageView(VkImage liveIm, const VulkanCreationInfo::Image &iminfo,
                          CompType typeCast, TextureDisplayViews &views);

  void FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex, rdcarray<uint32_t> *eventIDs);

  rdcarray<CounterResult> FetchCountersAMD(const rdcarray<GPUCounter> &counters);

  AMDCounters *m_pAMDCounters = NULL;
  AMDRGPControl *m_RGP = NULL;

  VulkanAMDActionCallback *m_pAMDActionCallback = NULL;

#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
  NVVulkanCounters *m_pNVCounters = NULL;
#endif

  rdcarray<CounterResult> FetchCountersKHR(const rdcarray<GPUCounter> &counters);

  rdcarray<VkPerformanceCounterKHR> m_KHRCounters;
  rdcarray<VkPerformanceCounterDescriptionKHR> m_KHRCountersDescriptions;

  void convertKhrCounterResult(CounterResult &rdcResult,
                               const VkPerformanceCounterResultKHR &khrResult,
                               VkPerformanceCounterUnitKHR khrUnit,
                               VkPerformanceCounterStorageKHR khrStorage);
};
