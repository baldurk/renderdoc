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

#include <vector>
#include "common/timing.h"
#include "replay/replay_driver.h"
#include "serialise/serialiser.h"
#include "vk_common.h"
#include "vk_info.h"
#include "vk_manager.h"
#include "vk_replay.h"
#include "vk_state.h"

using std::vector;
using std::list;

struct VkInitParams : public RDCInitParams
{
  VkInitParams();
  ReplayStatus Serialise();

  void Set(const VkInstanceCreateInfo *pCreateInfo, ResourceId inst);

  static const uint32_t VK_SERIALISE_VERSION = 0x0000006;

  // backwards compatibility for old logs described at the declaration of this array
  static const uint32_t VK_NUM_SUPPORTED_OLD_VERSIONS = 1;
  static const uint32_t VK_OLD_VERSIONS[VK_NUM_SUPPORTED_OLD_VERSIONS];

  // version number internal to vulkan stream
  uint32_t SerialiseVersion;

  string AppName, EngineName;
  uint32_t AppVersion, EngineVersion, APIVersion;

  vector<string> Layers;
  vector<string> Extensions;
  ResourceId InstanceID;
};

struct VulkanDrawcallTreeNode
{
  VulkanDrawcallTreeNode() {}
  explicit VulkanDrawcallTreeNode(const DrawcallDescription &d) : draw(d) {}
  DrawcallDescription draw;
  vector<VulkanDrawcallTreeNode> children;

  vector<pair<ResourceId, EventUsage> > resourceUsage;

  vector<ResourceId> executedCmds;

  VulkanDrawcallTreeNode &operator=(const DrawcallDescription &d)
  {
    *this = VulkanDrawcallTreeNode(d);
    return *this;
  }

  void InsertAndUpdateIDs(const VulkanDrawcallTreeNode &child, uint32_t baseEventID,
                          uint32_t baseDrawID)
  {
    resourceUsage.reserve(child.resourceUsage.size());
    for(size_t i = 0; i < child.resourceUsage.size(); i++)
    {
      resourceUsage.push_back(child.resourceUsage[i]);
      resourceUsage.back().second.eventID += baseEventID;
    }

    children.reserve(child.children.size());
    for(size_t i = 0; i < child.children.size(); i++)
    {
      children.push_back(child.children[i]);
      children.back().UpdateIDs(baseEventID, baseDrawID);
    }
  }

  void UpdateIDs(uint32_t baseEventID, uint32_t baseDrawID)
  {
    draw.eventID += baseEventID;
    draw.drawcallID += baseDrawID;

    for(int32_t i = 0; i < draw.events.count; i++)
      draw.events[i].eventID += baseEventID;

    for(size_t i = 0; i < resourceUsage.size(); i++)
      resourceUsage[i].second.eventID += baseEventID;

    for(size_t i = 0; i < children.size(); i++)
      children[i].UpdateIDs(baseEventID, baseDrawID);
  }

  vector<DrawcallDescription> Bake()
  {
    vector<DrawcallDescription> ret;
    if(children.empty())
      return ret;

    ret.resize(children.size());
    for(size_t i = 0; i < children.size(); i++)
    {
      ret[i] = children[i].draw;
      ret[i].children = children[i].Bake();
    }

    return ret;
  }
};

// use locally cached serialiser, per-thread
#undef GET_SERIALISER
#define GET_SERIALISER localSerialiser

// must be at the start of any function that serialises
#define CACHE_THREAD_SERIALISER() Serialiser *localSerialiser = GetThreadSerialiser();

struct VulkanDrawcallCallback
{
  // the three callbacks are used to allow the callback implementor to either
  // do a modified draw before or after the real thing.
  //
  // PreDraw()
  // do draw call as specified by the log
  // PostDraw()
  // if PostDraw() returns true:
  //   do draw call again
  //   PostRedraw()
  //
  // So either the modification happens in PreDraw, the modified draw happens,
  // then in PostDraw() the implementation can elect to undo the modifications
  // and do the real draw by returning true. OR they can do nothing in PreDraw,
  // do the real draw, then in PostDraw return true to apply the modifications
  // which are then undone in PostRedraw.
  virtual void PreDraw(uint32_t eid, VkCommandBuffer cmd) = 0;
  virtual bool PostDraw(uint32_t eid, VkCommandBuffer cmd) = 0;
  virtual void PostRedraw(uint32_t eid, VkCommandBuffer cmd) = 0;

  // same principle as above, but for dispatch calls
  virtual void PreDispatch(uint32_t eid, VkCommandBuffer cmd) = 0;
  virtual bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) = 0;
  virtual void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) = 0;

  // finally, these are for copy/blit/resolve/clear/etc
  virtual void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) = 0;
  virtual bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) = 0;
  virtual void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) = 0;

  // should we re-record all command buffers? this needs to be true if the range
  // being replayed is larger than one command buffer (which usually means the
  // whole frame).
  virtual bool RecordAllCmds() = 0;

  // if a command buffer is recorded once and submitted N > 1 times, then the same
  // drawcall will have several EIDs that refer to it. We'll only do the full
  // callbacks above for the first EID, then call this function for the others
  // to indicate that they are the same.
  virtual void AliasEvent(uint32_t primary, uint32_t alias) = 0;
};

class WrappedVulkan : public IFrameCapturer
{
private:
  friend class VulkanReplay;
  friend class VulkanDebugManager;

  struct ScopedDebugMessageSink
  {
    ScopedDebugMessageSink(WrappedVulkan *driver);
    ~ScopedDebugMessageSink();

    vector<DebugMessage> msgs;
    WrappedVulkan *m_pDriver;
  };

  friend struct ScopedDebugMessageSink;

#define SCOPED_DBG_SINK() ScopedDebugMessageSink debug_message_sink(this);

  uint64_t debugMessageSinkTLSSlot;
  ScopedDebugMessageSink *GetDebugMessageSink();
  void SetDebugMessageSink(ScopedDebugMessageSink *sink);

  // the messages retrieved for the current event (filled in Serialise_vk...() and read in
  // AddEvent())
  vector<DebugMessage> m_EventMessages;

  // list of all debug messages by EID in the frame
  vector<DebugMessage> m_DebugMessages;
  void Serialise_DebugMessages(Serialiser *localSerialiser, bool isDrawcall);
  vector<DebugMessage> GetDebugMessages();
  void AddDebugMessage(DebugMessage msg);
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, std::string d);

  enum
  {
    eInitialContents_ClearColorImage = 1,
    eInitialContents_ClearDepthStencilImage,
    eInitialContents_Sparse,
  };

  Serialiser *m_pSerialiser;
  LogState m_State;
  bool m_AppControlledCapture;

  uint64_t threadSerialiserTLSSlot;

  Threading::CriticalSection m_ThreadSerialisersLock;
  vector<Serialiser *> m_ThreadSerialisers;

  uint64_t tempMemoryTLSSlot;
  struct TempMem
  {
    TempMem() : memory(NULL), size(0) {}
    byte *memory;
    size_t size;
  };
  Threading::CriticalSection m_ThreadTempMemLock;
  vector<TempMem *> m_ThreadTempMem;

  VulkanReplay m_Replay;

  VkInitParams m_InitParams;

  VkResourceRecord *m_FrameCaptureRecord;
  Chunk *m_HeaderChunk;

  // we record the command buffer records so we can insert them
  // individually, that means even if they were recorded locklessly
  // in parallel, on replay they are disjoint and it makes things
  // much easier to process (we will enforce/display ordering
  // by queue submit order anyway, so it's OK to lose the record
  // order).
  Threading::CriticalSection m_CmdBufferRecordsLock;
  vector<VkResourceRecord *> m_CmdBufferRecords;

  VulkanResourceManager *m_ResourceManager;
  VulkanDebugManager *m_DebugManager;

  Threading::CriticalSection m_CapTransitionLock;

  VulkanDrawcallCallback *m_DrawcallCallback;

  // util function to handle fetching the right eventID, calling any
  // aliases then calling PreDraw/PreDispatch.
  uint32_t HandlePreCallback(VkCommandBuffer commandBuffer, DrawFlags type = DrawFlags::Drawcall,
                             uint32_t multiDrawOffset = 0);

  vector<WindowingSystem> m_SupportedWindowSystems;

  uint32_t m_FrameCounter;

  vector<FrameDescription> m_CapturedFrames;
  FrameRecord m_FrameRecord;
  vector<DrawcallDescription *> m_Drawcalls;

  struct PhysicalDeviceData
  {
    PhysicalDeviceData() : readbackMemIndex(0), uploadMemIndex(0), GPULocalMemIndex(0)
    {
      fakeMemProps = NULL;
      memIdxMap = NULL;
      RDCEraseEl(features);
      RDCEraseEl(props);
      RDCEraseEl(memProps);
      RDCEraseEl(fmtprops);
    }

    uint32_t GetMemoryIndex(uint32_t resourceRequiredBitmask, uint32_t allocRequiredProps,
                            uint32_t allocUndesiredProps);

    // store the three most common memory indices:
    //  - memory for copying into and reading back from the GPU
    //  - memory for copying into and uploading to the GPU
    //  - memory for sitting on the GPU and never being CPU accessed
    uint32_t readbackMemIndex;
    uint32_t uploadMemIndex;
    uint32_t GPULocalMemIndex;

    VkPhysicalDeviceMemoryProperties *fakeMemProps;
    uint32_t *memIdxMap;

    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memProps;
    VkFormatProperties fmtprops[VK_FORMAT_RANGE_SIZE];
  };

  PFN_vkSetDeviceLoaderData m_SetDeviceLoaderData;

  VkInstance m_Instance;                        // the instance corresponding to this WrappedVulkan
  VkDebugReportCallbackEXT m_DbgMsgCallback;    // the instance's dbg msg callback handle
  VkPhysicalDevice m_PhysicalDevice;            // the physical device we created m_Device with
  VkDevice m_Device;                            // the device used for our own command buffer work
  PhysicalDeviceData
      m_PhysicalDeviceData;    // the data about the physical device used for the above device;
  uint32_t
      m_QueueFamilyIdx;    // the family index that we've selected in CreateDevice for our queue
  VkQueue m_Queue;         // the queue used for our own command buffer work

  // the physical devices. At capture time this is trivial, just the enumerated devices.
  // At replay time this is re-ordered from the real list to try and match
  vector<VkPhysicalDevice> m_PhysicalDevices;

  // replay only, information we need for remapping. The original vector keeps information about the
  // physical devices used at capture time, and the replay vector contains the real unmodified list
  // of physical devices at replay time.
  vector<PhysicalDeviceData> m_OriginalPhysicalDevices;
  vector<VkPhysicalDevice> m_ReplayPhysicalDevices;
  vector<bool> m_ReplayPhysicalDevicesUsed;

  // the single queue family supported for each physical device
  vector<pair<uint32_t, VkQueueFamilyProperties> > m_SupportedQueueFamilies;

  // the supported queue family for the created device
  uint32_t m_SupportedQueueFamily;

  // the queue families (an array of count for each) for the created device
  vector<VkQueue *> m_QueueFamilies;

  vector<uint32_t *> m_MemIdxMaps;
  void RemapMemoryIndices(VkPhysicalDeviceMemoryProperties *memProps, uint32_t **memIdxMap);

  void WrapAndProcessCreatedSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                      VkSwapchainKHR *pSwapChain);

  struct
  {
    void Reset()
    {
      cmdpool = VK_NULL_HANDLE;
      freecmds.clear();
      pendingcmds.clear();
      submittedcmds.clear();

      freesems.clear();
      pendingsems.clear();
      submittedsems.clear();
    }

    VkCommandPool cmdpool;    // the command pool used for allocating our own command buffers

    vector<VkCommandBuffer> freecmds;
    // -> GetNextCmd() ->
    vector<VkCommandBuffer> pendingcmds;
    // -> SubmitCmds() ->
    vector<VkCommandBuffer> submittedcmds;
    // -> FlushQ() ------back to freecmds------^

    vector<VkSemaphore> freesems;
    // -> GetNextSemaphore() ->
    vector<VkSemaphore> pendingsems;
    // -> SubmitSemaphores() ->
    vector<VkSemaphore> submittedsems;
    // -> FlushQ() ----back to freesems-------^
  } m_InternalCmds;

  vector<VkDeviceMemory> m_CleanupMems;
  vector<VkEvent> m_CleanupEvents;
  vector<VkEvent> m_PersistentEvents;

  const VkPhysicalDeviceProperties &GetDeviceProps() { return m_PhysicalDeviceData.props; }
  VkDriverInfo GetDriverVersion() { return VkDriverInfo(m_PhysicalDeviceData.props); }
  const VkFormatProperties &GetFormatProperties(VkFormat f)
  {
    return m_PhysicalDeviceData.fmtprops[f];
  }

  uint32_t GetReadbackMemoryIndex(uint32_t resourceRequiredBitmask);
  uint32_t GetUploadMemoryIndex(uint32_t resourceRequiredBitmask);
  uint32_t GetGPULocalMemoryIndex(uint32_t resourceRequiredBitmask);

  struct BakedCmdBufferInfo
  {
    BakedCmdBufferInfo()
        : draw(NULL),
          eventCount(0),
          curEventID(0),
          drawCount(0),
          level(VK_COMMAND_BUFFER_LEVEL_PRIMARY),
          beginFlags(0)
    {
    }
    ~BakedCmdBufferInfo() { SAFE_DELETE(draw); }
    vector<APIEvent> curEvents;
    vector<DebugMessage> debugMessages;
    list<VulkanDrawcallTreeNode *> drawStack;

    VkCommandBufferLevel level;
    VkCommandBufferUsageFlags beginFlags;

    vector<pair<ResourceId, EventUsage> > resourceUsage;

    struct CmdBufferState
    {
      CmdBufferState() : idxWidth(0), subpass(0) {}
      ResourceId pipeline;

      struct DescriptorAndOffsets
      {
        ResourceId descSet;
        vector<uint32_t> offsets;
      };
      vector<DescriptorAndOffsets> graphicsDescSets, computeDescSets;

      uint32_t idxWidth;
      ResourceId ibuffer;
      vector<ResourceId> vbuffers;

      ResourceId renderPass;
      ResourceId framebuffer;
      uint32_t subpass;
    } state;

    vector<pair<ResourceId, ImageRegionState> > imgbarriers;

    VulkanDrawcallTreeNode *draw;    // the root draw to copy from when submitting
    uint32_t eventCount;             // how many events are in this cmd buffer, for quick skipping
    uint32_t curEventID;             // current event ID while reading or executing
    uint32_t drawCount;              // similar to above
  };

  // on replay, the current command buffer for the last chunk we
  // handled.
  ResourceId m_LastCmdBufferID;

  // this is a list of uint64_t file offset -> uint32_t EIDs of where each
  // drawcall is used. E.g. the drawcall at offset 873954 is EID 50. If a
  // command buffer is submitted more than once, there may be more than
  // one entry here - the drawcall will be aliased among several EIDs, with
  // the first one being the 'primary'
  struct DrawcallUse
  {
    DrawcallUse(uint64_t offs, uint32_t eid) : fileOffset(offs), eventID(eid) {}
    uint64_t fileOffset;
    uint32_t eventID;
    bool operator<(const DrawcallUse &o) const
    {
      if(fileOffset != o.fileOffset)
        return fileOffset < o.fileOffset;
      return eventID < o.eventID;
    }
  };
  vector<DrawcallUse> m_DrawcallUses;

  enum PartialReplayIndex
  {
    Primary,
    Secondary,
    ePartialNum
  };

  struct PartialReplayData
  {
    PartialReplayData() { Reset(); }
    void Reset()
    {
      resultPartialCmdPool = VK_NULL_HANDLE;
      resultPartialCmdBuffer = VK_NULL_HANDLE;
      partialDevice = VK_NULL_HANDLE;
      outsideCmdBuffer = VK_NULL_HANDLE;
      partialParent = ResourceId();
      baseEvent = 0;
      renderPassActive = false;
    }

    // if we're doing a partial replay, by definition only one command
    // buffer will be partial at any one time. While replaying through
    // the command buffer chunks, the partial command buffer will be
    // created as a temporary new command buffer and when it comes to
    // the queue that should submit it, it can submit this instead.
    VkCommandPool resultPartialCmdPool;
    VkCommandBuffer resultPartialCmdBuffer;
    VkDevice partialDevice;    // device for above cmd buffer

    // if we're replaying just a single draw or a particular command
    // buffer subsection of command events, we don't go through the
    // whole original command buffers to set up the partial replay,
    // so we just set this command buffer
    VkCommandBuffer outsideCmdBuffer;

    // this records where in the frame a command buffer was submitted,
    // so that we know if our replay range ends in one of these ranges
    // we need to construct a partial command buffer for future
    // replaying. Note that we always have the complete command buffer
    // around - it's the bakeID itself.
    // Since we only ever record a bakeID once the key is unique - note
    // that the same command buffer could be recorded multiple times
    // a frame, so the parent command buffer ID (the one recorded in
    // vkCmd chunks) is NOT unique.
    // However, a single baked command list can be submitted multiple
    // times - so we have to have a list of base events
    // Map from bakeID -> vector<baseEventID>
    map<ResourceId, vector<uint32_t> > cmdBufferSubmits;

    // This is just the ResourceId of the original parent command buffer
    // and it's baked id.
    // If we are in the middle of a partial replay - allows fast checking
    // in all vkCmd chunks, with the iteration through the above list
    // only in vkBegin.
    // partialParent gets reset to ResourceId() in the vkEnd so that
    // other baked command buffers from the same parent don't pick it up
    // Also reset each overall replay
    ResourceId partialParent;

    // If a partial replay is detected, this records the base of the
    // range. This both allows easily and uniquely identifying it in the
    // queuesubmit, but also allows the recording to 'rebase' the last
    // event ID by subtracting this, to know how far to record
    uint32_t baseEvent;

    // If we're doing a partial record this bool tells us when we
    // reach the vkEndCommandBuffer that we also need to end a render
    // pass.
    bool renderPassActive;
  } m_Partial[ePartialNum];

  map<ResourceId, VkCommandBuffer> m_RerecordCmds;

  // There is only a state while currently partially replaying, it's
  // undefined/empty otherwise.
  // All IDs are original IDs, not live.
  VulkanRenderState m_RenderState;

  bool ShouldRerecordCmd(ResourceId cmdid);
  bool InRerecordRange(ResourceId cmdid);
  VkCommandBuffer RerecordCmdBuf(ResourceId cmdid, PartialReplayIndex partialType = ePartialNum);

  // this info is stored in the record on capture, but we
  // need it on replay too
  struct DescriptorSetInfo
  {
    ~DescriptorSetInfo()
    {
      for(size_t i = 0; i < currentBindings.size(); i++)
        delete[] currentBindings[i];
      currentBindings.clear();
    }
    ResourceId layout;
    vector<DescriptorSetSlot *> currentBindings;
  };

  // capture-side data

  ResourceId m_LastSwap;

  // holds the current list of coherent mapped memory. Locked against concurrent use
  vector<VkResourceRecord *> m_CoherentMaps;
  Threading::CriticalSection m_CoherentMapsLock;

  // used both on capture and replay side to track image layouts. Only locked
  // in capture
  map<ResourceId, ImageLayouts> m_ImageLayouts;
  Threading::CriticalSection m_ImageLayoutsLock;

  // find swapchain for an image
  map<RENDERDOC_WindowHandle, VkSwapchainKHR> m_SwapLookup;
  Threading::CriticalSection m_SwapLookupLock;

  // below are replay-side data only, doesn't have to be thread protected

  // current descriptor set contents
  map<ResourceId, DescriptorSetInfo> m_DescriptorSetState;
  // data for a baked command buffer - its drawcalls and events, ready to submit
  map<ResourceId, BakedCmdBufferInfo> m_BakedCmdBufferInfo;
  // immutable creation data
  VulkanCreationInfo m_CreationInfo;

  map<ResourceId, vector<EventUsage> > m_ResourceUses;

  // returns thread-local temporary memory
  byte *GetTempMemory(size_t s);
  template <class T>
  T *GetTempArray(uint32_t arraycount)
  {
    return (T *)GetTempMemory(sizeof(T) * arraycount);
  }

  Serialiser *GetThreadSerialiser();
  Serialiser *GetMainSerialiser() { return m_pSerialiser; }
  void Serialise_CaptureScope(uint64_t offset);
  bool HasSuccessfulCapture();
  bool Serialise_BeginCaptureFrame(bool applyInitialState);
  void EndCaptureFrame(VkImage presentImage);

  void FirstFrame(VkSwapchainKHR swap);

  std::vector<VkImageMemoryBarrier> GetImplicitRenderPassBarriers(uint32_t subpass = 0);
  string MakeRenderPassOpString(bool store);
  void MakeSubpassLoadRP(VkRenderPassCreateInfo &info, const VkRenderPassCreateInfo *origInfo,
                         uint32_t s);

  bool IsDrawInRenderPass();

  void StartFrameCapture(void *dev, void *wnd);
  bool EndFrameCapture(void *dev, void *wnd);

  bool Serialise_SetShaderDebugPath(Serialiser *localSerialiser, VkDevice device,
                                    VkDebugMarkerObjectTagInfoEXT *pTagInfo);

  // replay

  bool Prepare_SparseInitialState(WrappedVkBuffer *buf);
  bool Prepare_SparseInitialState(WrappedVkImage *im);
  bool Serialise_SparseBufferInitialState(ResourceId id,
                                          VulkanResourceManager::InitialContentData contents);
  bool Serialise_SparseImageInitialState(ResourceId id,
                                         VulkanResourceManager::InitialContentData contents);
  bool Apply_SparseInitialState(WrappedVkBuffer *buf,
                                VulkanResourceManager::InitialContentData contents);
  bool Apply_SparseInitialState(WrappedVkImage *im,
                                VulkanResourceManager::InitialContentData contents);

  void ApplyInitialContents();

  vector<APIEvent> m_RootEvents, m_Events;
  bool m_AddedDrawcall;

  uint64_t m_CurChunkOffset;
  uint32_t m_RootEventID, m_RootDrawcallID;
  uint32_t m_FirstEventID, m_LastEventID;

  VulkanDrawcallTreeNode m_ParentDrawcall;

  bool m_ExtensionsEnabled[VkCheckExt_Max];

  // in vk_<platform>.cpp
  bool AddRequiredExtensions(bool instance, vector<string> &extensionList,
                             const std::set<string> &supportedExtensions);

  void InsertDrawsAndRefreshIDs(vector<VulkanDrawcallTreeNode> &cmdBufNodes);

  list<VulkanDrawcallTreeNode *> m_DrawcallStack;

  list<VulkanDrawcallTreeNode *> &GetDrawcallStack()
  {
    if(m_LastCmdBufferID != ResourceId())
      return m_BakedCmdBufferInfo[m_LastCmdBufferID].drawStack;

    return m_DrawcallStack;
  }

  void ProcessChunk(uint64_t offset, VulkanChunkType context);
  void ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
  void ContextProcessChunk(uint64_t offset, VulkanChunkType chunk);
  void AddDrawcall(const DrawcallDescription &d, bool hasEvents);
  void AddEvent(string description);

  void AddUsage(VulkanDrawcallTreeNode &drawNode, vector<DebugMessage> &debugMessages);

  // no copy semantics
  WrappedVulkan(const WrappedVulkan &);
  WrappedVulkan &operator=(const WrappedVulkan &);

  VkBool32 DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                         uint64_t object, size_t location, int32_t messageCode,
                         const char *pLayerPrefix, const char *pMessage);

  static VkBool32 VKAPI_PTR DebugCallbackStatic(VkDebugReportFlagsEXT flags,
                                                VkDebugReportObjectTypeEXT objectType,
                                                uint64_t object, size_t location,
                                                int32_t messageCode, const char *pLayerPrefix,
                                                const char *pMessage, void *pUserData)
  {
    return ((WrappedVulkan *)pUserData)
        ->DebugCallback(flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
  }

public:
  WrappedVulkan(const char *logFilename);
  virtual ~WrappedVulkan();

  ResourceId GetContextResourceID() { return m_FrameCaptureRecord->GetResourceID(); }
  static const char *GetChunkName(uint32_t idx);
  VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
  VulkanDebugManager *GetDebugManager() { return m_DebugManager; }
  LogState GetState() { return m_State; }
  VulkanReplay *GetReplay() { return &m_Replay; }
  // replay interface
  bool Prepare_InitialState(WrappedVkRes *res);
  bool Serialise_InitialState(ResourceId resid, WrappedVkRes *res);
  void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
  void Apply_InitialState(WrappedVkRes *live, VulkanResourceManager::InitialContentData initial);

  bool ReleaseResource(WrappedVkRes *res);

  ReplayStatus Initialise(VkInitParams &params);
  uint32_t GetLogVersion() { return m_InitParams.SerialiseVersion; }
  void Shutdown();
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
  void ReadLogInitialisation();

  FrameRecord &GetFrameRecord() { return m_FrameRecord; }
  APIEvent GetEvent(uint32_t eventID);
  uint32_t GetMaxEID() { return m_Events.back().eventID; }
  const DrawcallDescription *GetDrawcall(uint32_t eventID);

  ResourceId GetDescLayoutForDescSet(ResourceId descSet)
  {
    return m_DescriptorSetState[descSet].layout;
  }

  vector<EventUsage> GetUsage(ResourceId id) { return m_ResourceUses[id]; }
  // return the pre-selected device and queue
  VkDevice GetDev()
  {
    RDCASSERT(m_Device != VK_NULL_HANDLE);
    return m_Device;
  }
  uint32_t GetQFamilyIdx() { return m_QueueFamilyIdx; }
  VkQueue GetQ()
  {
    RDCASSERT(m_Device != VK_NULL_HANDLE);
    return m_Queue;
  }
  VkInstance GetInstance()
  {
    RDCASSERT(m_Instance != VK_NULL_HANDLE);
    return m_Instance;
  }
  VkPhysicalDevice GetPhysDev()
  {
    RDCASSERT(m_PhysicalDevice != VK_NULL_HANDLE);
    return m_PhysicalDevice;
  }
  VkCommandBuffer GetNextCmd();
  void SubmitCmds();
  VkSemaphore GetNextSemaphore();
  void SubmitSemaphores();
  void FlushQ();

  VulkanRenderState &GetRenderState() { return m_RenderState; }
  void SetDrawcallCB(VulkanDrawcallCallback *cb) { m_DrawcallCallback = cb; }
  bool IsSupportedExtension(const char *extName);
  VkResult FilterDeviceExtensionProperties(VkPhysicalDevice physDev, uint32_t *pPropertyCount,
                                           VkExtensionProperties *pProperties);
  static VkResult GetProvidedExtensionProperties(uint32_t *pPropertyCount,
                                                 VkExtensionProperties *pProperties);

  const VkPhysicalDeviceFeatures &GetDeviceFeatures() { return m_PhysicalDeviceData.features; }
  // Device initialization

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateInstance, const VkInstanceCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyInstance, VkInstance instance,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkEnumeratePhysicalDevices, VkInstance instance,
                                uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetPhysicalDeviceFeatures, VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceFeatures *pFeatures);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetPhysicalDeviceFormatProperties,
                                VkPhysicalDevice physicalDevice, VkFormat format,
                                VkFormatProperties *pFormatProperties);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceImageFormatProperties,
                                VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
                                VkImageTiling tiling, VkImageUsageFlags usage,
                                VkImageCreateFlags flags,
                                VkImageFormatProperties *pImageFormatProperties);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetPhysicalDeviceSparseImageFormatProperties,
                                VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
                                VkSampleCountFlagBits samples, VkImageUsageFlags usage,
                                VkImageTiling tiling, uint32_t *pPropertyCount,
                                VkSparseImageFormatProperties *pProperties);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetPhysicalDeviceProperties, VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceProperties *pProperties);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetPhysicalDeviceQueueFamilyProperties,
                                VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
                                VkQueueFamilyProperties *pQueueFamilyProperties);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetPhysicalDeviceMemoryProperties,
                                VkPhysicalDevice physicalDevice,
                                VkPhysicalDeviceMemoryProperties *pMemoryProperties);

  // Device functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDevice, VkPhysicalDevice physicalDevice,
                                const VkDeviceCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDevice, VkDevice device,
                                const VkAllocationCallbacks *pAllocator);

  // Queue functions

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetDeviceQueue, VkDevice device, uint32_t queueFamilyIndex,
                                uint32_t queueIndex, VkQueue *pQueue);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueSubmit, VkQueue queue, uint32_t submitCount,
                                const VkSubmitInfo *pSubmits, VkFence fence);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueWaitIdle, VkQueue queue);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDeviceWaitIdle, VkDevice device);

  // Query pool functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateQueryPool, VkDevice device,
                                const VkQueryPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyQueryPool, VkDevice device, VkQueryPool queryPool,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetQueryPoolResults, VkDevice device,
                                VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
                                size_t dataSize, void *pData, VkDeviceSize stride,
                                VkQueryResultFlags flags);

  // Semaphore functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSemaphore, VkDevice device,
                                const VkSemaphoreCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroySemaphore, VkDevice device, VkSemaphore semaphore,
                                const VkAllocationCallbacks *pAllocator);

  // Fence functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateFence, VkDevice device,
                                const VkFenceCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkFence *pFence);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyFence, VkDevice device, VkFence fence,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetFences, VkDevice device, uint32_t fenceCount,
                                const VkFence *pFences);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetFenceStatus, VkDevice device, VkFence fence);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkWaitForFences, VkDevice device, uint32_t fenceCount,
                                const VkFence *pFences, VkBool32 waitAll, uint64_t timeout);

  // Event functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateEvent, VkDevice device,
                                const VkEventCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkEvent *pEvent);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyEvent, VkDevice device, VkEvent event,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetEventStatus, VkDevice device, VkEvent event);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkSetEvent, VkDevice device, VkEvent event);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetEvent, VkDevice device, VkEvent event);

  // Memory functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocateMemory, VkDevice device,
                                const VkMemoryAllocateInfo *pAllocateInfo,
                                const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkFreeMemory, VkDevice device, VkDeviceMemory memory,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkMapMemory, VkDevice device, VkDeviceMemory memory,
                                VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                                void **ppData);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkUnmapMemory, VkDevice device, VkDeviceMemory memory);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkFlushMappedMemoryRanges, VkDevice device,
                                uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkInvalidateMappedMemoryRanges, VkDevice device,
                                uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetDeviceMemoryCommitment, VkDevice device,
                                VkDeviceMemory memory, VkDeviceSize *pCommittedMemoryInBytes);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetBufferMemoryRequirements, VkDevice device,
                                VkBuffer buffer, VkMemoryRequirements *pMemoryRequirements);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetImageMemoryRequirements, VkDevice device, VkImage image,
                                VkMemoryRequirements *pMemoryRequirements);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetImageSparseMemoryRequirements, VkDevice device,
                                VkImage image, uint32_t *pSparseMemoryRequirementCount,
                                VkSparseImageMemoryRequirements *pSparseMemoryRequirements);

  // Memory management API functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory, VkDevice device, VkBuffer buffer,
                                VkDeviceMemory memory, VkDeviceSize memoryOffset);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindImageMemory, VkDevice device, VkImage image,
                                VkDeviceMemory memory, VkDeviceSize memoryOffset);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueBindSparse, VkQueue queue, uint32_t bindInfoCount,
                                const VkBindSparseInfo *pBindInfo, VkFence fence);

  // Buffer functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateBuffer, VkDevice device,
                                const VkBufferCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyBuffer, VkDevice device, VkBuffer buffer,
                                const VkAllocationCallbacks *pAllocator);

  // Buffer view functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateBufferView, VkDevice device,
                                const VkBufferViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkBufferView *pView);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyBufferView, VkDevice device, VkBufferView bufferView,
                                const VkAllocationCallbacks *pAllocator);

  // Image functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateImage, VkDevice device,
                                const VkImageCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkImage *pImage);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyImage, VkDevice device, VkImage image,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetImageSubresourceLayout, VkDevice device, VkImage image,
                                const VkImageSubresource *pSubresource, VkSubresourceLayout *pLayout);

  // Image view functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateImageView, VkDevice device,
                                const VkImageViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkImageView *pView);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyImageView, VkDevice device, VkImageView imageView,
                                const VkAllocationCallbacks *pAllocator);

  // Shader functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateShaderModule, VkDevice device,
                                const VkShaderModuleCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkShaderModule *pShaderModule);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyShaderModule, VkDevice device,
                                VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator);

  // Pipeline functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreatePipelineCache, VkDevice device,
                                const VkPipelineCacheCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkPipelineCache *pPipelineCache);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyPipelineCache, VkDevice device,
                                VkPipelineCache pipelineCache,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPipelineCacheData, VkDevice device,
                                VkPipelineCache pipelineCache, size_t *pDataSize, void *pData);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkMergePipelineCaches, VkDevice device,
                                VkPipelineCache dstCache, uint32_t srcCacheCount,
                                const VkPipelineCache *pSrcCaches);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateGraphicsPipelines, VkDevice device,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateComputePipelines, VkDevice device,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkComputePipelineCreateInfo *pCreateInfos,
                                const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyPipeline, VkDevice device, VkPipeline pipeline,
                                const VkAllocationCallbacks *pAllocator);

  // Pipeline layout functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreatePipelineLayout, VkDevice device,
                                const VkPipelineLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkPipelineLayout *pPipelineLayout);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyPipelineLayout, VkDevice device,
                                VkPipelineLayout pipelineLayout,
                                const VkAllocationCallbacks *pAllocator);

  // Sampler functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSampler, VkDevice device,
                                const VkSamplerCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkSampler *pSampler);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroySampler, VkDevice device, VkSampler sampler,
                                const VkAllocationCallbacks *pAllocator);

  // Descriptor set functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorSetLayout, VkDevice device,
                                const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDescriptorSetLayout *pSetLayout);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDescriptorSetLayout, VkDevice device,
                                VkDescriptorSetLayout descriptorSetLayout,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorPool, VkDevice device,
                                const VkDescriptorPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDescriptorPool *pDescriptorPool);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDescriptorPool, VkDevice device,
                                VkDescriptorPool descriptorPool,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetDescriptorPool, VkDevice device,
                                VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocateDescriptorSets, VkDevice device,
                                const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                VkDescriptorSet *pDescriptorSets);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkFreeDescriptorSets, VkDevice device,
                                VkDescriptorPool descriptorPool, uint32_t descriptorSetCount,
                                const VkDescriptorSet *pDescriptorSets);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkUpdateDescriptorSets, VkDevice device,
                                uint32_t descriptorWriteCount,
                                const VkWriteDescriptorSet *pDescriptorWrites,
                                uint32_t descriptorCopyCount,
                                const VkCopyDescriptorSet *pDescriptorCopies);

  // Command pool functions

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetRenderAreaGranularity, VkDevice device,
                                VkRenderPass renderPass, VkExtent2D *pGranularity);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateCommandPool, VkDevice device,
                                const VkCommandPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyCommandPool, VkDevice device,
                                VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetCommandPool, VkDevice device,
                                VkCommandPool commandPool, VkCommandPoolResetFlags flags);

  // Command buffer functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocateCommandBuffers, VkDevice device,
                                const VkCommandBufferAllocateInfo *pAllocateInfo,
                                VkCommandBuffer *pCommandBuffers);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkFreeCommandBuffers, VkDevice device,
                                VkCommandPool commandPool, uint32_t commandBufferCount,
                                const VkCommandBuffer *pCommandBuffers);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBeginCommandBuffer, VkCommandBuffer commandBuffer,
                                const VkCommandBufferBeginInfo *pBeginInfo);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkEndCommandBuffer, VkCommandBuffer commandBuffer);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetCommandBuffer, VkCommandBuffer commandBuffer,
                                VkCommandBufferResetFlags flags);

  // Command buffer building functions

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindPipeline, VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetViewport, VkCommandBuffer commandBuffer,
                                uint32_t firstViewport, uint32_t viewportCount,
                                const VkViewport *pViewports);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetScissor, VkCommandBuffer commandBuffer,
                                uint32_t firstScissor, uint32_t scissorCount,
                                const VkRect2D *pScissors);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetLineWidth, VkCommandBuffer commandBuffer,
                                float lineWidth);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthBias, VkCommandBuffer commandBuffer,
                                float depthBiasConstantFactor, float depthBiasClamp,
                                float depthBiasSlopeFactor);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetBlendConstants, VkCommandBuffer commandBuffer,
                                const float blendConstants[4]);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthBounds, VkCommandBuffer commandBuffer,
                                float minDepthBounds, float maxDepthBounds);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilCompareMask, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, uint32_t compareMask);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilWriteMask, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, uint32_t writeMask);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilReference, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, uint32_t reference);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDescriptorSets, VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                                uint32_t firstSet, uint32_t setCount,
                                const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
                                const uint32_t *pDynamicOffsets);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindIndexBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindVertexBuffers, VkCommandBuffer commandBuffer,
                                uint32_t firstBinding, uint32_t bindingCount,
                                const VkBuffer *pBuffers, const VkDeviceSize *pOffsets);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDraw, VkCommandBuffer commandBuffer, uint32_t vertexCount,
                                uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexed, VkCommandBuffer commandBuffer,
                                uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                                int32_t vertexOffset, uint32_t firstInstance);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndirect, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                                uint32_t stride);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirect, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                                uint32_t stride);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatch, VkCommandBuffer commandBuffer, uint32_t x,
                                uint32_t y, uint32_t z);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatchIndirect, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                                const VkBufferCopy *pRegions);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImage, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
                                VkImageLayout dstImageLayout, uint32_t regionCount,
                                const VkImageCopy *pRegions);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBlitImage, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
                                VkImageLayout dstImageLayout, uint32_t regionCount,
                                const VkImageBlit *pRegions, VkFilter filter);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBufferToImage, VkCommandBuffer commandBuffer,
                                VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout,
                                uint32_t regionCount, const VkBufferImageCopy *pRegions);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImageToBuffer, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer,
                                uint32_t regionCount, const VkBufferImageCopy *pRegions);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdUpdateBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize,
                                const uint32_t *pData);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdFillBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize fillSize,
                                uint32_t data);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearColorImage, VkCommandBuffer commandBuffer,
                                VkImage image, VkImageLayout imageLayout,
                                const VkClearColorValue *pColor, uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearDepthStencilImage, VkCommandBuffer commandBuffer,
                                VkImage image, VkImageLayout imageLayout,
                                const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearAttachments, VkCommandBuffer commandBuffer,
                                uint32_t attachmentCount, const VkClearAttachment *pAttachments,
                                uint32_t rectCount, const VkClearRect *pRects);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResolveImage, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
                                VkImageLayout dstImageLayout, uint32_t regionCount,
                                const VkImageResolve *pRegions);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetEvent, VkCommandBuffer commandBuffer, VkEvent event,
                                VkPipelineStageFlags stageMask);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResetEvent, VkCommandBuffer commandBuffer, VkEvent event,
                                VkPipelineStageFlags stageMask);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWaitEvents, VkCommandBuffer commandBuffer,
                                uint32_t eventCount, const VkEvent *pEvents,
                                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                                uint32_t bufferMemoryBarrierCount,
                                const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                uint32_t imageMemoryBarrierCount,
                                const VkImageMemoryBarrier *pImageMemoryBarriers);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPipelineBarrier, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount,
                                const VkMemoryBarrier *pMemoryBarriers,
                                uint32_t bufferMemoryBarrierCount,
                                const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                uint32_t imageMemoryBarrierCount,
                                const VkImageMemoryBarrier *pImageMemoryBarriers);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPushConstants, VkCommandBuffer commandBuffer,
                                VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                                uint32_t offset, uint32_t size, const void *pValues);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass, VkCommandBuffer commandBuffer,
                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                VkSubpassContents contents);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdNextSubpass, VkCommandBuffer commandBuffer,
                                VkSubpassContents contents);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndRenderPass, VkCommandBuffer commandBuffer);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdExecuteCommands, VkCommandBuffer commandBuffer,
                                uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWriteTimestamp, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool,
                                uint32_t query);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyQueryPoolResults, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride,
                                VkQueryResultFlags flags);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginQuery, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndQuery, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResetQueryPool, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateFramebuffer, VkDevice device,
                                const VkFramebufferCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyFramebuffer, VkDevice device,
                                VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateRenderPass, VkDevice device,
                                const VkRenderPassCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyRenderPass, VkDevice device, VkRenderPass renderPass,
                                const VkAllocationCallbacks *pAllocator);

  // VK_EXT_debug_report functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDebugReportCallbackEXT, VkInstance instance,
                                const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDebugReportCallbackEXT *pCallback);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDebugReportCallbackEXT, VkInstance instance,
                                VkDebugReportCallbackEXT callback,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDebugReportMessageEXT, VkInstance instance,
                                VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                uint64_t object, size_t location, int32_t messageCode,
                                const char *pLayerPrefix, const char *pMessage);

  // VK_EXT_debug_marker functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDebugMarkerSetObjectTagEXT, VkDevice device,
                                VkDebugMarkerObjectTagInfoEXT *pTagInfo);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDebugMarkerSetObjectNameEXT, VkDevice device,
                                VkDebugMarkerObjectNameInfoEXT *pNameInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDebugMarkerBeginEXT, VkCommandBuffer commandBuffer,
                                VkDebugMarkerMarkerInfoEXT *pMarker);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDebugMarkerEndEXT, VkCommandBuffer commandBuffer);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDebugMarkerInsertEXT, VkCommandBuffer commandBuffer,
                                VkDebugMarkerMarkerInfoEXT *pMarker);

  // Windowing extension functions

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfaceSupportKHR,
                                VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
                                VkSurfaceKHR surface, VkBool32 *pSupported);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
                                VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                VkSurfaceCapabilitiesKHR *pSurfaceCapabilities);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfaceFormatsKHR,
                                VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfacePresentModesKHR,
                                VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSwapchainKHR, VkDevice device,
                                const VkSwapchainCreateInfoKHR *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroySwapchainKHR, VkDevice device,
                                VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSwapchainImagesKHR, VkDevice device,
                                VkSwapchainKHR swapchain, uint32_t *pSwapchainImageCount,
                                VkImage *pSwapchainImages);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAcquireNextImageKHR, VkDevice device,
                                VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
                                VkFence fence, uint32_t *pImageIndex);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueuePresentKHR, VkQueue queue,
                                const VkPresentInfoKHR *pPresentInfo);

  // these functions are non-serialised as they're only used for windowing
  // setup during capture, but they must be intercepted so we can unwrap
  // properly
  void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                           const VkAllocationCallbacks *pAllocator);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
  // VK_KHR_win32_surface
  VkResult vkCreateWin32SurfaceKHR(VkInstance instance,
                                   const VkWin32SurfaceCreateInfoKHR *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);

  VkBool32 vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                          uint32_t queueFamilyIndex);

  // VK_NV_external_memory_win32
  VkResult vkGetMemoryWin32HandleNV(VkDevice device, VkDeviceMemory memory,
                                    VkExternalMemoryHandleTypeFlagsNV handleType, HANDLE *pHandle);

  // VK_KHX_external_memory_win32
  VkResult vkGetMemoryWin32HandleKHX(VkDevice device, VkDeviceMemory memory,
                                     VkExternalMemoryHandleTypeFlagBitsKHX handleType,
                                     HANDLE *pHandle);
  VkResult vkGetMemoryWin32HandlePropertiesKHX(
      VkDevice device, VkExternalMemoryHandleTypeFlagBitsKHX handleType, HANDLE handle,
      VkMemoryWin32HandlePropertiesKHX *pMemoryWin32HandleProperties);

  // VK_KHX_external_semaphore_win32
  VkResult vkImportSemaphoreWin32HandleKHX(
      VkDevice device, const VkImportSemaphoreWin32HandleInfoKHX *pImportSemaphoreWin32HandleInfo);
  VkResult vkGetSemaphoreWin32HandleKHX(VkDevice device, VkSemaphore semaphore,
                                        VkExternalSemaphoreHandleTypeFlagBitsKHX handleType,
                                        HANDLE *pHandle);
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  // VK_KHR_android_surface
  VkResult vkCreateAndroidSurfaceKHR(VkInstance instance,
                                     const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
  // VK_KHR_xcb_surface
  VkResult vkCreateXcbSurfaceKHR(VkInstance instance, const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);

  VkBool32 vkGetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                        uint32_t queueFamilyIndex,
                                                        xcb_connection_t *connection,
                                                        xcb_visualid_t visual_id);
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
  // VK_KHR_xlib_surface
  VkResult vkCreateXlibSurfaceKHR(VkInstance instance, const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);

  VkBool32 vkGetPhysicalDeviceXlibPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                         uint32_t queueFamilyIndex, Display *dpy,
                                                         VisualID visualID);

  // VK_EXT_acquire_xlib_display
  VkResult vkAcquireXlibDisplayEXT(VkPhysicalDevice physicalDevice, Display *dpy,
                                   VkDisplayKHR display);
  VkResult vkGetRandROutputDisplayEXT(VkPhysicalDevice physicalDevice, Display *dpy,
                                      RROutput rrOutput, VkDisplayKHR *pDisplay);

#endif

  // VK_KHR_display and VK_KHR_display_swapchain. These have no library or include dependencies so
  // wecan just compile them in on all platforms to reduce platform-specific code. They are mostly
  // only actually used though on *nix.
  VkResult vkGetPhysicalDeviceDisplayPropertiesKHR(VkPhysicalDevice physicalDevice,
                                                   uint32_t *pPropertyCount,
                                                   VkDisplayPropertiesKHR *pProperties);

  VkResult vkGetPhysicalDeviceDisplayPlanePropertiesKHR(VkPhysicalDevice physicalDevice,
                                                        uint32_t *pPropertyCount,
                                                        VkDisplayPlanePropertiesKHR *pProperties);

  VkResult vkGetDisplayPlaneSupportedDisplaysKHR(VkPhysicalDevice physicalDevice, uint32_t planeIndex,
                                                 uint32_t *pDisplayCount, VkDisplayKHR *pDisplays);

  VkResult vkGetDisplayModePropertiesKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display,
                                         uint32_t *pPropertyCount,
                                         VkDisplayModePropertiesKHR *pProperties);

  VkResult vkCreateDisplayModeKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display,
                                  const VkDisplayModeCreateInfoKHR *pCreateInfo,
                                  const VkAllocationCallbacks *pAllocator, VkDisplayModeKHR *pMode);

  VkResult vkGetDisplayPlaneCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkDisplayModeKHR mode,
                                            uint32_t planeIndex,
                                            VkDisplayPlaneCapabilitiesKHR *pCapabilities);

  VkResult vkCreateDisplayPlaneSurfaceKHR(VkInstance instance,
                                          const VkDisplaySurfaceCreateInfoKHR *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkSurfaceKHR *pSurface);

  VkResult vkCreateSharedSwapchainsKHR(VkDevice device, uint32_t swapchainCount,
                                       const VkSwapchainCreateInfoKHR *pCreateInfos,
                                       const VkAllocationCallbacks *pAllocator,
                                       VkSwapchainKHR *pSwapchains);

  // VK_NV_external_memory_capabilities
  VkResult vkGetPhysicalDeviceExternalImageFormatPropertiesNV(
      VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling,
      VkImageUsageFlags usage, VkImageCreateFlags flags,
      VkExternalMemoryHandleTypeFlagsNV externalHandleType,
      VkExternalImageFormatPropertiesNV *pExternalImageFormatProperties);

  // VK_KHR_maintenance1
  void vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool,
                            VkCommandPoolTrimFlagsKHR flags);

  // VK_KHR_get_physical_device_properties2
  void vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice,
                                       VkPhysicalDeviceFeatures2KHR *pFeatures);
  void vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice physicalDevice,
                                         VkPhysicalDeviceProperties2KHR *pProperties);
  void vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format,
                                               VkFormatProperties2KHR *pFormatProperties);
  VkResult vkGetPhysicalDeviceImageFormatProperties2KHR(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2KHR *pImageFormatInfo,
      VkImageFormatProperties2KHR *pImageFormatProperties);
  void vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t *pCount,
                                                    VkQueueFamilyProperties2KHR *pQueueFamilyProperties);
  void vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice,
                                               VkPhysicalDeviceMemoryProperties2KHR *pMemoryProperties);
  void vkGetPhysicalDeviceSparseImageFormatProperties2KHR(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2KHR *pFormatInfo,
      uint32_t *pPropertyCount, VkSparseImageFormatProperties2KHR *pProperties);

  // VK_EXT_display_surface_counter
  VkResult vkGetPhysicalDeviceSurfaceCapabilities2EXT(VkPhysicalDevice physicalDevice,
                                                      VkSurfaceKHR surface,
                                                      VkSurfaceCapabilities2EXT *pSurfaceCapabilities);

  // VK_EXT_display_control
  VkResult vkDisplayPowerControlEXT(VkDevice device, VkDisplayKHR display,
                                    const VkDisplayPowerInfoEXT *pDisplayPowerInfo);
  VkResult vkRegisterDeviceEventEXT(VkDevice device, const VkDeviceEventInfoEXT *pDeviceEventInfo,
                                    const VkAllocationCallbacks *pAllocator, VkFence *pFence);
  VkResult vkRegisterDisplayEventEXT(VkDevice device, VkDisplayKHR display,
                                     const VkDisplayEventInfoEXT *pDisplayEventInfo,
                                     const VkAllocationCallbacks *pAllocator, VkFence *pFence);
  VkResult vkGetSwapchainCounterEXT(VkDevice device, VkSwapchainKHR swapchain,
                                    VkSurfaceCounterFlagBitsEXT counter, uint64_t *pCounterValue);

  // VK_EXT_direct_mode_display
  VkResult vkReleaseDisplayEXT(VkPhysicalDevice physicalDevice, VkDisplayKHR display);

  // VK_KHX_external_memory_capabilities
  void vkGetPhysicalDeviceExternalBufferPropertiesKHX(
      VkPhysicalDevice physicalDevice,
      const VkPhysicalDeviceExternalBufferInfoKHX *pExternalBufferInfo,
      VkExternalBufferPropertiesKHX *pExternalBufferProperties);

  // VK_KHX_external_memory_fd
  VkResult vkGetMemoryFdKHX(VkDevice device, VkDeviceMemory memory,
                            VkExternalMemoryHandleTypeFlagBitsKHX handleType, int *pFd);
  VkResult vkGetMemoryFdPropertiesKHX(VkDevice device,
                                      VkExternalMemoryHandleTypeFlagBitsKHX handleType, int fd,
                                      VkMemoryFdPropertiesKHX *pMemoryFdProperties);

  // VK_KHX_external_semaphore_capabilities
  void vkGetPhysicalDeviceExternalSemaphorePropertiesKHX(
      VkPhysicalDevice physicalDevice,
      const VkPhysicalDeviceExternalSemaphoreInfoKHX *pExternalSemaphoreInfo,
      VkExternalSemaphorePropertiesKHX *pExternalSemaphoreProperties);

  // VK_KHX_external_semaphore_fd
  VkResult vkImportSemaphoreFdKHX(VkDevice device,
                                  const VkImportSemaphoreFdInfoKHX *pImportSemaphoreFdInfo);
  VkResult vkGetSemaphoreFdKHX(VkDevice device, VkSemaphore semaphore,
                               VkExternalSemaphoreHandleTypeFlagBitsKHX handleType, int *pFd);
};
