/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#elif ENABLED(RDOC_APPLE)

#define WINDOW_HANDLE_DECL void *wnd;
#define WINDOW_HANDLE_INIT wnd = NULL;

#else

#error "Unknown platform"

#endif

#include <map>
using std::map;

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

class WrappedVulkan;
class VulkanDebugManager;
class VulkanResourceManager;

class VulkanReplay : public IReplayDriver
{
public:
  VulkanReplay();

  void SetProxy(bool p) { m_Proxy = p; }
  bool IsRemoteProxy() { return m_Proxy; }
  void Shutdown();

  void SetDriver(WrappedVulkan *d) { m_pDriver = d; }
  APIProperties GetAPIProperties();

  vector<ResourceId> GetBuffers();
  BufferDescription GetBuffer(ResourceId id);

  vector<ResourceId> GetTextures();
  TextureDescription GetTexture(ResourceId id);

  ShaderReflection *GetShader(ResourceId shader, string entryPoint);

  vector<string> GetDisassemblyTargets();
  string DisassembleShader(const ShaderReflection *refl, const string &target);

  vector<EventUsage> GetUsage(ResourceId id);

  FrameRecord GetFrameRecord();
  vector<DebugMessage> GetDebugMessages();

  void SavePipelineState();
  D3D11Pipe::State GetD3D11PipelineState() { return D3D11Pipe::State(); }
  D3D12Pipe::State GetD3D12PipelineState() { return D3D12Pipe::State(); }
  GLPipe::State GetGLPipelineState() { return GLPipe::State(); }
  VKPipe::State GetVulkanPipelineState() { return m_VulkanPipelineState; }
  void FreeTargetResource(ResourceId id);

  void ReadLogInitialisation();
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);

  vector<uint32_t> GetPassEvents(uint32_t eventID);

  vector<WindowingSystem> GetSupportedWindowSystems();

  uint64_t MakeOutputWindow(WindowingSystem system, void *data, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void ClearOutputWindowColor(uint64_t id, float col[4]);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  void InitPostVSBuffers(uint32_t eventID);
  void InitPostVSBuffers(const vector<uint32_t> &passEvents);

  ResourceId GetLiveID(ResourceId id);

  vector<GPUCounter> EnumerateCounters();
  void DescribeCounter(GPUCounter counterID, CounterDescription &desc);
  vector<CounterResult> FetchCounters(const vector<GPUCounter> &counters);

  bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                 CompType typeHint, float *minval, float *maxval);
  bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                    CompType typeHint, float minval, float maxval, bool channels[4],
                    vector<uint32_t> &histogram);

  MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData);
  byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                       const GetTextureDataParams &params, size_t &dataSize);

  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);

  void RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, const MeshDisplay &cfg);

  void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
  void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStage type,
                         ResourceId *id, string *errors);
  void FreeCustomShader(ResourceId id);

  bool RenderTexture(TextureDisplay cfg);

  void RenderCheckerboard(Vec3f light, Vec3f dark);

  void RenderHighlightBox(float w, float h, float scale);

  void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                            vector<ShaderVariable> &outvars, const vector<byte> &data);

  vector<PixelModification> PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x,
                                         uint32_t y, uint32_t slice, uint32_t mip,
                                         uint32_t sampleIdx, CompType typeHint);
  ShaderDebugTrace DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx,
                               uint32_t instOffset, uint32_t vertOffset);
  ShaderDebugTrace DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                              uint32_t primitive);
  ShaderDebugTrace DebugThread(uint32_t eventID, const uint32_t groupid[3],
                               const uint32_t threadid[3]);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip,
                 uint32_t sample, CompType typeHint, float pixel[4]);
  uint32_t PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y);

  ResourceId RenderOverlay(ResourceId cfg, CompType typeHint, DebugOverlay overlay,
                           uint32_t eventID, const vector<uint32_t> &passEvents);
  ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip, uint32_t arrayIdx,
                               uint32_t sampleIdx, CompType typeHint);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex);
  void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                           size_t dataSize);
  bool IsTextureSupported(const ResourceFormat &format);

  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf);
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

  bool IsRenderOutput(ResourceId id);

  void FileChanged();

  void InitCallstackResolver();
  bool HasCallstacks();
  Callstack::StackResolver *GetCallstackResolver();

  // called before any VkDevice is created, to init any counters
  static void PreDeviceInitCounters();
  // called after the VkDevice is created, to init any counters
  void PostDeviceInitCounters();

  // called after any VkDevice is destroyed, to do corresponding shutdown of counters
  static void PostDeviceShutdownCounters();
  // called before the VkDevice is destroyed, to shutdown any counters
  void PreDeviceShutdownCounters();

  // used for vulkan layer bookkeeping. Ideally this should all be handled by installers/packages,
  // but for developers running builds locally or just in case, we need to be able to update the
  // layer registration ourselves.
  // These functions are defined in vk_<platform>.cpp
  static bool CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                               std::vector<std::string> &otherJSONs);
  static void InstallVulkanLayer(bool systemLevel);

private:
  struct OutputWindow
  {
    OutputWindow();

    void SetCol(VkDeviceMemory mem, VkImage img);
    void SetDS(VkDeviceMemory mem, VkImage img);
    void Create(WrappedVulkan *driver, VkDevice device, bool depth);
    void Destroy(WrappedVulkan *driver, VkDevice device);

    // implemented in vk_replay_platform.cpp
    void CreateSurface(VkInstance inst);
    void SetWindowHandle(WindowingSystem system, void *data);

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

    VkImage dsimg;
    VkDeviceMemory dsmem;
    VkImageView dsview;
    VkImageMemoryBarrier depthBarrier;

    VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
    VulkanResourceManager *m_ResourceManager;
  };

  VKPipe::State m_VulkanPipelineState;

  map<uint64_t, OutputWindow> m_OutputWindows;
  uint64_t m_OutputWinID;
  uint64_t m_ActiveWinID;
  bool m_BindDepth;
  uint32_t m_DebugWidth, m_DebugHeight;

  HighlightCache m_HighlightCache;

  bool m_Proxy;

  WrappedVulkan *m_pDriver;

  enum TexDisplayFlags
  {
    eTexDisplay_F32Render = 0x1,
    eTexDisplay_BlendAlpha = 0x2,
    eTexDisplay_MipShift = 0x4,
  };

  bool RenderTextureInternal(TextureDisplay cfg, VkRenderPassBeginInfo rpbegin, int flags);

  void CreateTexImageView(VkImageAspectFlags aspectFlags, VkImage liveIm,
                          VulkanCreationInfo::Image &iminfo);

  void FillCBufferVariables(rdctype::array<ShaderConstant>, vector<ShaderVariable> &outvars,
                            const vector<byte> &data, size_t baseOffset);

  VulkanDebugManager *GetDebugManager();
  VulkanResourceManager *GetResourceManager();
};
