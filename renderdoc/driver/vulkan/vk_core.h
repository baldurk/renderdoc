/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "common/timing.h"
#include "serialise/serialiser.h"
#include "vk_common.h"
#include "vk_info.h"
#include "vk_manager.h"
#include "vk_state.h"

class VulkanShaderCache;
class VulkanDebugManager;
class VulkanResourceManager;
class VulkanTextRenderer;
class VulkanReplay;

struct VkInitParams
{
  void Set(const VkInstanceCreateInfo *pCreateInfo, ResourceId inst);

  rdcstr AppName, EngineName;
  uint32_t AppVersion = 0, EngineVersion = 0;
  VkPackedVersion APIVersion;

  rdcarray<rdcstr> Layers;
  rdcarray<rdcstr> Extensions;
  ResourceId InstanceID;

  // remember to update this function if you add more members
  uint64_t GetSerialiseSize();

  // check if a frame capture section version is supported
  static const uint64_t CurrentVersion = 0x15;
  static bool IsSupportedVersion(uint64_t ver);
};

DECLARE_REFLECTION_STRUCT(VkInitParams);

enum class VkIndirectPatchType
{
  NoPatch,
  DispatchIndirect,
  DrawIndirect,
  DrawIndexedIndirect,
  DrawIndirectCount,
  DrawIndexedIndirectCount,
  DrawIndirectByteCount,
};

enum class EventFlags : uint32_t
{
  NoFlags = 0x0,
  VertexRWUsage = 1 << uint32_t(ShaderStage::Vertex),
  TessControlRWUsage = 1 << uint32_t(ShaderStage::Tess_Control),
  TessEvalRWUsage = 1 << uint32_t(ShaderStage::Tess_Eval),
  GeometryRWUsage = 1 << uint32_t(ShaderStage::Geometry),
};

BITMASK_OPERATORS(EventFlags);

constexpr inline EventFlags PipeStageRWEventFlags(ShaderStage stage)
{
  return EventFlags(1 << uint32_t(stage));
}

inline EventFlags PipeRWUsageEventFlags(ResourceUsage usage)
{
  if(usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::GS_RWResource)
    return PipeStageRWEventFlags(
        ShaderStage(uint32_t(usage) - uint32_t(ResourceUsage::VS_RWResource)));
  return EventFlags::NoFlags;
}

struct VkIndirectRecordData
{
  VkBufferMemoryBarrier paramsBarrier, countBarrier;

  struct
  {
    VkBuffer src, dst;
    VkBufferCopy copy;
  } paramsCopy, countCopy;
};

struct VkIndirectPatchData
{
  VkIndirectPatchType type = VkIndirectPatchType::NoPatch;
  MemoryAllocation alloc;
  VkBuffer buf;
  uint32_t count;
  uint32_t stride;
  uint32_t vertexoffset;

  ResourceId commandBuffer;
};

struct VulkanActionTreeNode
{
  VulkanActionTreeNode() {}
  explicit VulkanActionTreeNode(const ActionDescription &a) : action(a) {}
  ActionDescription action;
  rdcarray<VulkanActionTreeNode> children;

  VkIndirectPatchData indirectPatch;

  rdcarray<rdcpair<ResourceId, EventUsage>> resourceUsage;

  rdcarray<ResourceId> executedCmds;

  VulkanActionTreeNode &operator=(const ActionDescription &a)
  {
    *this = VulkanActionTreeNode(a);
    return *this;
  }

  void InsertAndUpdateIDs(const VulkanActionTreeNode &child, uint32_t baseEventID, uint32_t baseDrawID)
  {
    resourceUsage.reserve(child.resourceUsage.size());
    for(size_t i = 0; i < child.resourceUsage.size(); i++)
    {
      resourceUsage.push_back(child.resourceUsage[i]);
      resourceUsage.back().second.eventId += baseEventID;
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
    action.eventId += baseEventID;
    action.actionId += baseDrawID;

    for(APIEvent &ev : action.events)
      ev.eventId += baseEventID;

    for(size_t i = 0; i < resourceUsage.size(); i++)
      resourceUsage[i].second.eventId += baseEventID;

    for(size_t i = 0; i < children.size(); i++)
      children[i].UpdateIDs(baseEventID, baseDrawID);
  }

  rdcarray<ActionDescription> Bake()
  {
    rdcarray<ActionDescription> ret;
    if(children.empty())
      return ret;

    ret.resize(children.size());
    for(size_t i = 0; i < children.size(); i++)
    {
      ret[i] = children[i].action;
      ret[i].children = children[i].Bake();
    }

    return ret;
  }
};

#define SERIALISE_TIME_CALL(...)                                                                \
  {                                                                                             \
    WriteSerialiser &ser = GetThreadSerialiser();                                               \
    ser.ChunkMetadata().timestampMicro = Timing::GetTick();                                     \
    __VA_ARGS__;                                                                                \
    ser.ChunkMetadata().durationMicro = Timing::GetTick() - ser.ChunkMetadata().timestampMicro; \
  }

// must be at the start of any function that serialises
#define CACHE_THREAD_SERIALISER() WriteSerialiser &ser = GetThreadSerialiser();

struct VulkanActionCallback
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
  virtual void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) = 0;
  virtual bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) = 0;
  virtual void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) = 0;

  // called immediately before the command buffer is ended
  virtual void PreEndCommandBuffer(VkCommandBuffer cmd) = 0;

  // if a command buffer is recorded once and submitted N > 1 times, then the same
  // drawcall will have several EIDs that refer to it. We'll only do the full
  // callbacks above for the first EID, then call this function for the others
  // to indicate that they are the same.
  virtual void AliasEvent(uint32_t primary, uint32_t alias) = 0;

  // Returns true if vkCmdExecuteCommands should be called separately for every
  // command buffer in pCommandBuffers.
  virtual bool SplitSecondary() = 0;

  // Returns true if secondary command buffer inheritance info should be modified so
  // it uses the load FB/RP instead of the original FB/RP. This is mostly used when a callback is
  // starting/stopping RPs around every execute, so it resumes with the load RP which must
  // match.
  virtual bool ForceLoadRPs() = 0;

  // called before vkCmdExecuteCommands with a range for the draws inside the
  // secondary command buffer.
  virtual void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                             VkCommandBuffer cmd) = 0;

  // called after vkCmdExecuteCommands with a range for the action inside the
  // seocndary command buffer.
  virtual void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                              VkCommandBuffer cmd) = 0;
};

struct UserDebugReportCallbackData
{
  VkInstance wrappedInstance;
  VkDebugReportCallbackCreateInfoEXT createInfo;
  bool muteWarned;

  VkDebugReportCallbackEXT realObject;
};

struct UserDebugUtilsCallbackData
{
  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  bool muteWarned;

  VkDebugUtilsMessengerEXT realObject;
};

class WrappedVulkan : public IFrameCapturer
{
private:
  friend class VulkanReplay;
  friend class VulkanDebugManager;
  friend struct VulkanRenderState;
  friend class VulkanShaderCache;

  struct ScopedDebugMessageSink
  {
    ScopedDebugMessageSink(WrappedVulkan *driver);
    ~ScopedDebugMessageSink();

    rdcarray<DebugMessage> msgs;
    WrappedVulkan *m_pDriver;
  };

  friend struct ScopedDebugMessageSink;

#define SCOPED_DBG_SINK() ScopedDebugMessageSink debug_message_sink(this);

  uint64_t debugMessageSinkTLSSlot;
  ScopedDebugMessageSink *GetDebugMessageSink();
  void SetDebugMessageSink(ScopedDebugMessageSink *sink);

  // the messages retrieved for the current event (filled in Serialise_vk...() and read in
  // AddEvent())
  rdcarray<DebugMessage> m_EventMessages;

  // list of all debug messages by EID in the frame
  rdcarray<DebugMessage> m_DebugMessages;
  template <typename SerialiserType>
  void Serialise_DebugMessages(SerialiserType &ser);

  void ProcessDebugMessage(DebugMessage &DebugMessages);
  rdcarray<DebugMessage> GetDebugMessages();
  void AddDebugMessage(DebugMessage msg);

  bool m_CaptureFailure = false;
  uint64_t m_LastCaptureFailed = 0;
  RDResult m_LastCaptureError = ResultCode::Succeeded;
  int m_OOMHandler = 0;
  RDResult m_FatalError = ResultCode::Succeeded;
  CaptureState m_State;
  bool m_AppControlledCapture = false;
  bool m_FirstFrameCapture = false;

  int32_t m_ReuseEnabled = 1;

  PerformanceTimer m_CaptureTimer;

  bool m_MarkedActive = false;
  uint32_t m_SubmitCounter = 0;

  uint64_t threadSerialiserTLSSlot;

  Threading::CriticalSection m_ThreadSerialisersLock;
  rdcarray<WriteSerialiser *> m_ThreadSerialisers;

  Threading::CriticalSection m_CallbacksLock;
  rdcarray<UserDebugReportCallbackData *> m_ReportCallbacks;
  rdcarray<UserDebugUtilsCallbackData *> m_UtilsCallbacks;
  void SendUserDebugMessage(const rdcstr &msg);

  uint64_t tempMemoryTLSSlot;
  struct TempMem
  {
    TempMem() : memory(NULL), cur(NULL), size(0) {}
    byte *memory;
    byte *cur;
    size_t size;
  };
  Threading::CriticalSection m_ThreadTempMemLock;
  rdcarray<TempMem *> m_ThreadTempMem;

  VulkanReplay *m_Replay;
  ReplayOptions m_ReplayOptions;

  VkInitParams m_InitParams;
  uint64_t m_SectionVersion;

  StreamReader *m_FrameReader = NULL;

  std::set<rdcstr> m_StringDB;

  Threading::CriticalSection m_CapDescriptorsLock;
  std::set<rdcpair<ResourceId, VkResourceRecord *>> m_CapDescriptors;

  VkResourceRecord *m_FrameCaptureRecord;
  Chunk *m_HeaderChunk;

  // we record the command buffer records so we can insert them
  // individually, that means even if they were recorded locklessly
  // in parallel, on replay they are disjoint and it makes things
  // much easier to process (we will enforce/display ordering
  // by queue submit order anyway, so it's OK to lose the record
  // order).
  Threading::CriticalSection m_CmdBufferRecordsLock;
  rdcarray<VkResourceRecord *> m_CmdBufferRecords;

  VulkanResourceManager *m_ResourceManager = NULL;
  VulkanDebugManager *m_DebugManager = NULL;
  VulkanShaderCache *m_ShaderCache = NULL;
  VulkanTextRenderer *m_TextRenderer = NULL;

  Threading::RWLock m_CapTransitionLock;

  VulkanActionCallback *m_ActionCallback;
  void *m_SubmitChain;

  uint64_t m_TimeBase = 0;
  double m_TimeFrequency = 1.0f;
  SDFile *m_StructuredFile;
  SDFile *m_StoredStructuredData;

  void AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix);
  void DerivedResource(ResourceId parentLive, ResourceId child);
  template <typename VulkanType>
  void DerivedResource(VulkanType parent, ResourceId child)
  {
    DerivedResource(GetResID(parent), child);
  }
  void AddResourceCurChunk(ResourceDescription &descr);
  void AddResourceCurChunk(ResourceId id);

  // util function to handle fetching the right eventId, calling any
  // aliases then calling PreDraw/PreDispatch.
  uint32_t HandlePreCallback(VkCommandBuffer commandBuffer,
                             ActionFlags type = ActionFlags::Drawcall, uint32_t multiDrawOffset = 0);

  rdcarray<WindowingSystem> m_SupportedWindowSystems;

  uint32_t m_FrameCounter = 0;

  rdcarray<FrameDescription> m_CapturedFrames;
  rdcarray<ActionDescription *> m_Actions;

  struct PhysicalDeviceData
  {
    uint32_t GetMemoryIndex(uint32_t resourceCompatibleBitmask, uint32_t allocRequiredProps,
                            uint32_t allocPreferredProps);

    // store the three most common memory indices:
    //  - memory for copying into and reading back from the GPU
    //  - memory for copying into and uploading to the GPU
    //  - memory for sitting on the GPU and never being CPU accessed
    uint32_t readbackMemIndex = 0;
    uint32_t uploadMemIndex = 0;
    uint32_t GPULocalMemIndex = 0;

    VkPhysicalDeviceFeatures availFeatures = {};
    VkPhysicalDeviceFeatures enabledFeatures = {};
    VkPhysicalDeviceProperties props = {};
    VkPhysicalDeviceDriverProperties driverProps = {};
    VkPhysicalDeviceMemoryProperties memProps = {};
    std::map<VkFormat, VkFormatProperties> fmtProps = {};
    VkDriverInfo driverInfo = VkDriverInfo(false);

    VkPhysicalDevicePerformanceQueryFeaturesKHR performanceQueryFeatures = {};

    uint32_t queueCount = 0;
    VkQueueFamilyProperties queueProps[16] = {};
  };

  bool m_SeparateDepthStencil = false;
  bool m_NULLDescriptorsAllowed = false;
  bool m_ExtendedDynState = false;
  bool m_ExtendedDynState2 = false;
  bool m_ExtendedDynState2Logic = false;
  bool m_ExtendedDynState2CPs = false;
  bool m_FragmentShadingRate = false;
  bool m_DynColorWrite = false;
  bool m_DynVertexInput = false;

  PFN_vkSetDeviceLoaderData m_SetDeviceLoaderData;

  InstanceDeviceInfo m_EnabledExtensions;

  // the instance corresponding to this WrappedVulkan
  VkInstance m_Instance;
  // the instance's dbg msg callback handle
  VkDebugReportCallbackEXT m_DbgReportCallback;
  VkDebugUtilsMessengerEXT m_DbgUtilsCallback;
  // the physical device we created m_Device with
  VkPhysicalDevice m_PhysicalDevice;
  // the device used for our own command buffer work
  VkDevice m_Device;
  // the data about the physical device used for the above device
  PhysicalDeviceData m_PhysicalDeviceData;
  // the driver info for the original device
  PhysicalDeviceData m_OrigPhysicalDeviceData;
  // the family index that we've selected in CreateDevice for our queue. During replay, this is an
  // index in the replay-time queues, not the capture-time queues (i.e. after remapping)
  uint32_t m_QueueFamilyIdx;
  // the queue used for our own command buffer work
  VkQueue m_Queue;
  // the last queue that submitted something during replay, to allow correct sync between
  // submissions
  VkQueue m_PrevQueue;

  // the physical devices. At capture time this is trivial, just the enumerated devices.
  // At replay time this is re-ordered from the real list to try and match
  rdcarray<VkPhysicalDevice> m_PhysicalDevices;

  // replay only, information we need for remapping. The original vector keeps information about the
  // physical devices used at capture time, and the replay vector contains the real unmodified list
  // of physical devices at replay time.
  rdcarray<PhysicalDeviceData> m_OriginalPhysicalDevices;
  rdcarray<VkPhysicalDevice> m_ReplayPhysicalDevices;
  rdcarray<bool> m_ReplayPhysicalDevicesUsed;

  // the queue families (an array of count for each) for the created device
  rdcarray<VkQueue *> m_QueueFamilies;
  rdcarray<uint32_t> m_QueueFamilyCounts;
  rdcarray<uint32_t> m_QueueFamilyIndices;

  ImageBarrierSequence m_setupImageBarriers;
  ImageBarrierSequence m_cleanupImageBarriers;

  // a small amount of helper code during capture for handling resources on different queues in init
  // states
  struct ExternalQueue
  {
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;

    struct
    {
      VkCommandBuffer acquire = VK_NULL_HANDLE;
      VkCommandBuffer release = VK_NULL_HANDLE;
      VkSemaphore fromext = VK_NULL_HANDLE;
      VkSemaphore toext = VK_NULL_HANDLE;
      VkFence fence = VK_NULL_HANDLE;
    } ring[4];
    uint32_t ringIndex = 0;
    uint32_t GetNextIdx()
    {
      uint32_t ret = ringIndex;
      ringIndex = (ringIndex + 1) % ARRAY_COUNT(ring);
      return ret;
    }
  };
  rdcarray<ExternalQueue> m_ExternalQueues;

  VkCommandBuffer GetExtQueueCmd(uint32_t queueFamilyIdx) const;
  void SubmitAndFlushExtQueue(uint32_t queueFamilyIdx);

  void SubmitAndFlushImageStateBarriers(ImageBarrierSequence &barriers);
  void InlineSetupImageBarriers(VkCommandBuffer cmd, ImageBarrierSequence &batches);
  void InlineCleanupImageBarriers(VkCommandBuffer cmd, ImageBarrierSequence &batches);

  struct QueueRemap
  {
    uint32_t family;
    uint32_t index;
  };

  // for each queue family in the original captured physical device, we have a remapping vector.
  // Each element in the vector is an available queue in that family, and the uint64 is packed as
  // (targetQueueFamily << 32) | (targetQueueIndex)
  rdcarray<QueueRemap> m_QueueRemapping[16];

  void WrapAndProcessCreatedSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                      VkSwapchainKHR *pSwapChain);

  GPUBuffer m_IndirectBuffer;
  size_t m_IndirectBufferSize = 0;
  VkCommandBuffer m_IndirectCommandBuffer = VK_NULL_HANDLE;
  bool m_IndirectDraw = false;

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

    rdcarray<VkCommandBuffer> freecmds;
    // -> GetNextCmd() ->
    rdcarray<VkCommandBuffer> pendingcmds;
    // -> SubmitCmds() ->
    rdcarray<VkCommandBuffer> submittedcmds;
    // -> FlushQ() ------back to freecmds------^

    rdcarray<VkSemaphore> freesems;
    // -> GetNextSemaphore() ->
    rdcarray<VkSemaphore> pendingsems;
    // -> SubmitSemaphores() ->
    rdcarray<VkSemaphore> submittedsems;
    // -> FlushQ() ----back to freesems-------^
  } m_InternalCmds;

  static const int initialStateMaxBatch = 100;
  int initStateCurBatch = 0;

  bool m_PrepareInitStateBatching = false;
  // list of resources which have been prepared but haven't been serialised
  rdcarray<ResourceId> m_PreparedNotSerialisedInitStates;

  rdcarray<rdcstr> m_InitTempFiles;
  VkCommandBuffer initStateCurCmd = VK_NULL_HANDLE;
  rdcarray<std::function<void()>> m_PendingCleanups;

  // Internal lumped/pooled memory allocations

  // Each memory scope gets a separate vector of allocation objects. The vector contains the list of
  // all 'base' allocations. The offset is used to indicate the current offset, and the size is the
  // total size, thus the free space can be determined with size - offset.
  rdcarray<MemoryAllocation> m_MemoryBlocks[arraydim<MemoryScope>()];
  Threading::ThreadHandle m_MemoryFreeThread = 0;

  // Per memory scope, the size of the next allocation. This allows us to balance number of memory
  // allocation objects with size by incrementally allocating larger blocks.
  VkDeviceSize m_MemoryBlockSize[arraydim<MemoryScope>()] = {};

  void FreeAllMemory(MemoryScope scope);
  uint64_t CurMemoryUsage(MemoryScope scope);
  void ResetMemoryBlocks(MemoryScope scope);
  void FreeMemoryAllocation(MemoryAllocation alloc);

  // internal implementation - call one of the functions above
  MemoryAllocation AllocateMemoryForResource(bool buffer, VkMemoryRequirements mrq,
                                             MemoryScope scope, MemoryType type);

  rdcarray<VkEvent> m_CleanupEvents;

  // reset queries must be restored to a valid state on next replay to ensure any query pool
  // results copies that refer to previous frames have valid data. We initialised all of the
  // query pool at creation time, but any queries that are reset might not be recorded within
  // the frame - certainly not if we're doing partial replays. So we record all queries that are
  // reset to re-initialise them.
  struct ResetQuery
  {
    VkQueryPool pool;
    uint32_t firstQuery;
    uint32_t queryCount;
  };
  rdcarray<ResetQuery> m_ResetQueries;

  const VkFormatProperties &GetFormatProperties(VkFormat f);

  struct BakedCmdBufferInfo
  {
    BakedCmdBufferInfo()
        : action(NULL),
          eventCount(0),
          curEventID(0),
          actionCount(0),
          level(VK_COMMAND_BUFFER_LEVEL_PRIMARY),
          beginFlags(0),
          markerCount(0)

    {
    }
    ~BakedCmdBufferInfo() { SAFE_DELETE(action); }
    rdcarray<APIEvent> curEvents;
    rdcarray<DebugMessage> debugMessages;
    rdcarray<VulkanActionTreeNode *> actionStack;

    rdcarray<VkIndirectRecordData> indirectCopies;

    uint32_t beginChunk = 0;
    uint32_t endChunk = 0;

    VkCommandBufferLevel level;
    VkCommandBufferUsageFlags beginFlags;

    bool inheritConditionalRendering = false;

    int markerCount;

    rdcarray<rdcpair<ResourceId, EventUsage>> resourceUsage;

    VulkanRenderState state;

    rdcflatmap<ResourceId, ImageState> imageStates;

    // whether the renderdoc commandbuffer execution has a renderpass currently open and replaying
    // and expects nextSubpass/endRPass/endRendering commands to be executed even if partial
    bool renderPassOpen = false;

    // barriers executed by nextSubpass/endRP executions after the last active subpass, to revert
    // after executing said commands and keep the target-EID layout
    rdcarray<VkImageMemoryBarrier> endBarriers;

    // subpass currently active in the commandbuffer's renderpass. The subpass counter in
    // VulkanRenderState stops
    // after the selected drawcall in the case of a partial replay, but this one increments with
    // every call to
    // vkCmdNextSubpass for valid barrier counting.
    int activeSubpass = 0;

    ResourceId pushDescriptorID[2][64];

    VulkanActionTreeNode *action;    // the root action to copy from when submitting
    uint32_t eventCount;             // how many events are in this cmd buffer, for quick skipping
    uint32_t curEventID;             // current event ID while reading or executing
    uint32_t actionCount;            // similar to above
  };

  // on replay, the current command buffer for the last chunk we
  // handled.
  ResourceId m_LastCmdBufferID;

  // this is a list of uint64_t file offset -> uint32_t EIDs of where each
  // action is used. E.g. the action at offset 873954 is EID 50. If a
  // command buffer is submitted more than once, there may be more than
  // one entry here - the action will be aliased among several EIDs, with
  // the first one being the 'primary'
  struct ActionUse
  {
    ActionUse(uint64_t offs, uint32_t eid) : fileOffset(offs), eventId(eid) {}
    uint64_t fileOffset;
    uint32_t eventId;
    bool operator<(const ActionUse &o) const
    {
      if(fileOffset != o.fileOffset)
        return fileOffset < o.fileOffset;
      return eventId < o.eventId;
    }
  };
  rdcarray<ActionUse> m_ActionUses;

  enum PartialReplayIndex
  {
    Primary,
    Secondary,
    ePartialNum
  };

  struct Submission
  {
    Submission(uint32_t eid) : baseEvent(eid), rebased(false) {}
    uint32_t baseEvent = 0;
    bool rebased = false;
  };

  // by definition, when replaying we must have N completely submitted command buffers, and at most
  // two partially-submitted command buffers. One primary, that we're part-way through, and then
  // if we're part-way through a vkCmdExecuteCommandBuffers inside that primary then there's one
  // secondary.
  struct PartialReplayData
  {
    PartialReplayData() { Reset(); }
    void Reset()
    {
      partialParent = ResourceId();
      baseEvent = 0;
      renderPassActive = false;
    }

    // this records where in the frame a command buffer was submitted, so that we know if our replay
    // range ends in one of these ranges we need to construct a partial command buffer for future
    // replaying. Note that we always have the complete command buffer around - it's the bakeID
    // itself.
    // Since we only ever record a bakeID once the key is unique - note that the same command buffer
    // could be recorded multiple times a frame, so the parent command buffer ID (the one recorded
    // in vkCmd chunks) is NOT unique.
    // However, a single baked command list can be submitted multiple times - so we have to have a
    // list of base events
    // Note in the case of secondary command buffers we mark when these are rebased to 'absolute'
    // event IDs, since they could be submitted multiple times in the frame and we don't want to
    // rebase all of them each time.
    // Map from bakeID -> vector<Submission>
    std::map<ResourceId, rdcarray<Submission>> cmdBufferSubmits;

    // identifies the baked ID of the command buffer that's actually partial at each level.
    ResourceId partialParent;

    // the base even of the submission that's partial, as defined above in partialParent
    uint32_t baseEvent;

    // whether a renderpass is currently active in the partial recording - as with baseEvent, only
    // valid for the command buffer referred to by partialParent.
    bool renderPassActive;
  } m_Partial[ePartialNum];

  // if we're replaying just a single action or a particular command
  // buffer subsection of command events, we don't go through the
  // whole original command buffers to set up the partial replay,
  // so we just set this command buffer
  VkCommandBuffer m_OutsideCmdBuffer = VK_NULL_HANDLE;

  // stores the currently re-recording command buffer for any original command buffer ID (not bake
  // ID). This allows a quick check to see if an original command should be recorded, and also to
  // fetch the command buffer to record into.
  std::map<ResourceId, VkCommandBuffer> m_RerecordCmds;

  // we store the list here, since we need to keep all command buffers until the whole replay is
  // finished, but if a command buffer is re-recorded multiple times it would be overwritten in the
  // above map
  rdcarray<rdcpair<VkCommandPool, VkCommandBuffer>> m_RerecordCmdList;

  // There is only a state while currently partially replaying, it's
  // undefined/empty otherwise.
  // All IDs are original IDs, not live.
  VulkanRenderState m_RenderState;

  bool InRerecordRange(ResourceId cmdid);
  bool HasRerecordCmdBuf(ResourceId cmdid);
  bool ShouldUpdateRenderState(ResourceId cmdid, bool forcePrimary = false);
  bool IsRenderpassOpen(ResourceId cmdid);
  VkCommandBuffer RerecordCmdBuf(ResourceId cmdid, PartialReplayIndex partialType = ePartialNum);

  ResourceId GetPartialCommandBuffer();

  // this info is stored in the record on capture, but we
  // need it on replay too
  struct DescriptorSetInfo
  {
    DescriptorSetInfo(bool p = false) : push(p) {}
    DescriptorSetInfo(const DescriptorSetInfo &) = delete;
    DescriptorSetInfo &operator=(const DescriptorSetInfo &) = delete;
    ~DescriptorSetInfo() { clear(); }
    ResourceId layout;
    BindingStorage data;
    bool push;

    void clear()
    {
      layout = ResourceId();
      data.clear();
    }
  };

  // capture-side data

  ResourceId m_LastSwap;

  // When capturing in VR mode (no conventional present), resource of the vkImage that the VR
  // runtime
  // specifies as last backbuffer through the VR backbuffer tag
  ResourceId m_CurrentVRBackbuffer;

  // hold onto device address resources (buffers and memories) so that if one is destroyed
  // mid-capture we can hold onto it until the capture is complete.
  struct
  {
    rdcarray<VkDeviceMemory> DeadMemories;
    rdcarray<VkBuffer> DeadBuffers;
    rdcarray<ResourceId> IDs;
  } m_DeviceAddressResources;

  // holds the current list of coherent mapped memory. Locked against concurrent use
  rdcarray<VkResourceRecord *> m_CoherentMaps;
  Threading::CriticalSection m_CoherentMapsLock;

  rdcarray<VkResourceRecord *> m_ForcedReferences;
  Threading::CriticalSection m_ForcedReferencesLock;

  int64_t m_QueueCounter = 0;

  rdcarray<VkResourceRecord *> GetForcedReferences()
  {
    rdcarray<VkResourceRecord *> ret;

    {
      SCOPED_LOCK(m_ForcedReferencesLock);
      ret = m_ForcedReferences;
    }

    return ret;
  }

  bool IsForcedReference(VkResourceRecord *record)
  {
    bool ret = false;

    {
      SCOPED_LOCK(m_ForcedReferencesLock);
      ret = (m_ForcedReferences.indexOf(record) != -1);
    }

    return ret;
  }

  void AddForcedReference(VkResourceRecord *record)
  {
    SCOPED_LOCK(m_ForcedReferencesLock);
    m_ForcedReferences.push_back(record);
  }

  // used on replay side to track the queue family of command buffers and pools
  std::map<ResourceId, uint32_t> m_commandQueueFamilies;

  // used both on capture and replay side to track image state. Only locked
  // in capture
  std::map<ResourceId, LockingImageState> m_ImageStates;
  Threading::CriticalSection m_ImageStatesLock;

  // find swapchain for an image
  std::map<RENDERDOC_WindowHandle, VkSwapchainKHR> m_SwapLookup;
  Threading::CriticalSection m_SwapLookupLock;

  // below are replay-side data only, doesn't have to be thread protected

  // current descriptor set contents
  std::map<ResourceId, DescriptorSetInfo> m_DescriptorSetState;
  // data for a baked command buffer - its actions and events, ready to submit
  std::map<ResourceId, BakedCmdBufferInfo> m_BakedCmdBufferInfo;
  // immutable creation data
  VulkanCreationInfo m_CreationInfo;

  std::map<ResourceId, rdcarray<EventUsage>> m_ResourceUses;
  std::map<uint32_t, EventFlags> m_EventFlags;
  rdcarray<ResourceId> m_FeedbackRPs;

  bytebuf m_MaskedMapData;

  // on replay we may need to allocate several bits of temporary memory, so the single-region
  // doesn't work as well. We're not quite as performance-sensitive so we allocate 4MB per thread
  // and use it in a ring-buffer fashion. This allows multiple allocations to live at once as long
  // as we don't need it all in one stack.
  byte *GetRingTempMemory(size_t s);

  // returns thread-local temporary memory
  byte *GetTempMemory(size_t s);
  template <class T>
  T *GetTempArray(uint32_t arraycount)
  {
    return (T *)GetTempMemory(sizeof(T) * arraycount);
  }

  template <class T>
  T *UnwrapArray(const T *wrapped, uint32_t count)
  {
    T *ret = GetTempArray<T>(count);
    for(uint32_t i = 0; i < count; i++)
      ret[i] = wrapped ? Unwrap(wrapped[i]) : VK_NULL_HANDLE;
    return ret;
  }

  // specialised for each info structure we want to unwrap, where it's used
  template <class T>
  T UnwrapInfo(const T *info);
  template <class T>
  T *UnwrapInfos(CaptureState state, const T *infos, uint32_t count);

  void PatchAttachment(VkFramebufferAttachmentImageInfo *att, VkFormat imgFormat,
                       VkSampleCountFlagBits samples);
  void PatchImageViewUsage(VkImageViewUsageCreateInfo *usage, VkFormat imgFormat,
                           VkSampleCountFlagBits samples);

  VkIndirectPatchData FetchIndirectData(VkIndirectPatchType type, VkCommandBuffer commandBuffer,
                                        VkBuffer dataBuffer, VkDeviceSize dataOffset, uint32_t count,
                                        uint32_t stride = 0, VkBuffer counterBuffer = VK_NULL_HANDLE,
                                        VkDeviceSize counterOffset = 0);
  void ExecuteIndirectReadback(VkCommandBuffer commandBuffer,
                               const VkIndirectRecordData &indirectcopy);

  WriteSerialiser &GetThreadSerialiser();
  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);
  bool HasSuccessfulCapture();

  template <typename SerialiserType>
  bool Serialise_BeginCaptureFrame(SerialiserType &ser);
  void EndCaptureFrame(VkImage presentImage);

  void FirstFrame();

  rdcstr GetPhysDeviceCompatString(bool external, bool origInvalid);
  bool CheckMemoryRequirements(const char *resourceName, ResourceId memId,
                               VkDeviceSize memoryOffset, const VkMemoryRequirements &mrq,
                               bool external, const VkMemoryRequirements &origMrq);

  void AddImplicitResolveResourceUsage(uint32_t subpass = 0);
  rdcarray<VkImageMemoryBarrier> GetImplicitRenderPassBarriers(uint32_t subpass = 0);
  rdcstr MakeRenderPassOpString(bool store);
  void ApplyRPLoadDiscards(VkCommandBuffer commandBuffer, VkRect2D renderArea);

  RDCDriver GetFrameCaptureDriver() { return RDCDriver::Vulkan; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);

  void AdvanceFrame();
  void Present(DeviceOwnedWindow devWnd);

  void HandleFrameMarkers(const char *marker, VkCommandBuffer commandBuffer);
  void HandleFrameMarkers(const char *marker, VkQueue queue);

  template <typename SerialiserType>
  bool Serialise_SetShaderDebugPath(SerialiserType &ser, VkShaderModule ShaderObject,
                                    rdcstr DebugPath);

  // replay

  ResourceDescription &GetResourceDesc(ResourceId id);

  void ApplyInitialContents();

  rdcarray<APIEvent> m_RootEvents, m_Events;
  bool m_AddedAction;

  uint64_t m_CurChunkOffset;
  SDChunkMetaData m_ChunkMetadata;
  uint32_t m_RootEventID, m_RootActionID;
  uint32_t m_FirstEventID, m_LastEventID;
  VulkanChunk m_LastChunk;

  ResourceId m_LastPresentedImage;

  std::set<ResourceId> m_SparseBindResources;

  RDResult m_FailedReplayResult = ResultCode::APIReplayFailed;

  VulkanActionTreeNode m_ParentAction;

  bool m_LayersEnabled[VkCheckLayer_Max] = {};

  // in vk_<platform>.cpp
  void AddRequiredExtensions(bool instance, rdcarray<rdcstr> &extensionList,
                             const std::set<rdcstr> &supportedExtensions);

  bool PatchIndirectDraw(size_t drawIndex, uint32_t paramStride, VkIndirectPatchType type,
                         ActionDescription &action, byte *&argptr, byte *argend);
  void InsertActionsAndRefreshIDs(BakedCmdBufferInfo &cmdBufInfo);
  void CaptureQueueSubmit(VkQueue queue, const rdcarray<VkCommandBuffer> &commandBuffers,
                          VkFence fence);
  void ReplayQueueSubmit(VkQueue queue, VkSubmitInfo2 submitInfo, rdcstr basename);
  void InternalFlushMemoryRange(VkDevice device, const VkMappedMemoryRange &memRange,
                                bool internalFlush, bool capframe);

  void DoSubmit(VkQueue queue, VkSubmitInfo2 submitInfo);

  rdcarray<VulkanActionTreeNode *> m_ActionStack;

  rdcarray<VulkanActionTreeNode *> &GetActionStack()
  {
    if(m_LastCmdBufferID != ResourceId())
      return m_BakedCmdBufferInfo[m_LastCmdBufferID].actionStack;

    return m_ActionStack;
  }

  bool ProcessChunk(ReadSerialiser &ser, VulkanChunk chunk);
  RDResult ContextReplayLog(CaptureState readType, uint32_t startEventID, uint32_t endEventID,
                            bool partial);
  bool ContextProcessChunk(ReadSerialiser &ser, VulkanChunk chunk);
  void AddAction(const ActionDescription &a);
  void AddEvent();

  void AddUsage(VulkanActionTreeNode &actionNode, rdcarray<DebugMessage> &debugMessages);
  void AddFramebufferUsage(VulkanActionTreeNode &actionNode, const VulkanRenderState &renderState);
  void AddFramebufferUsageAllChildren(VulkanActionTreeNode &actionNode,
                                      const VulkanRenderState &renderState);

  // no copy semantics
  WrappedVulkan(const WrappedVulkan &);
  WrappedVulkan &operator=(const WrappedVulkan &);

  VkBool32 DebugCallback(MessageSeverity severity, MessageCategory category, int messageCode,
                         const char *pMessageId, const char *pMessage);

  static VkBool32 VKAPI_PTR DebugUtilsCallbackStatic(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageTypes,
      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

  static VkBool32 VKAPI_PTR DebugReportCallbackStatic(VkDebugReportFlagsEXT flags,
                                                      VkDebugReportObjectTypeEXT objectType,
                                                      uint64_t object, size_t location,
                                                      int32_t messageCode, const char *pLayerPrefix,
                                                      const char *pMessage, void *pUserData);
  void AddFrameTerminator(uint64_t queueMarkerTag);

  VkResourceRecord *RegisterSurface(WindowingSystem system, void *handle);

public:
  WrappedVulkan();
  virtual ~WrappedVulkan();

  APIProperties APIProps;

  const InstanceDeviceInfo &GetExtensions(VkResourceRecord *record) const
  {
    return record ? *record->instDevInfo : m_EnabledExtensions;
  }

  bool IsCmdPrimary()
  {
    RDCASSERT(m_LastCmdBufferID != ResourceId());
    auto it = m_BakedCmdBufferInfo.find(m_LastCmdBufferID);
    RDCASSERT(it != m_BakedCmdBufferInfo.end());
    return it->second.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  }

  VulkanRenderState &GetCmdRenderState()
  {
    RDCASSERT(m_LastCmdBufferID != ResourceId());
    auto it = m_BakedCmdBufferInfo.find(m_LastCmdBufferID);
    RDCASSERT(it != m_BakedCmdBufferInfo.end());
    return it->second.state;
  }

  uint32_t RemapQueue(uint32_t q)
  {
    if(q >= ARRAY_COUNT(m_QueueRemapping) || m_QueueRemapping[q].empty())
      return q;
    return m_QueueRemapping[q][0].family;
  }

  static rdcstr GetChunkName(uint32_t idx);
  VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
  VulkanDebugManager *GetDebugManager() { return m_DebugManager; }
  VulkanShaderCache *GetShaderCache() { return m_ShaderCache; }
  CaptureState GetState() { return m_State; }
  VulkanReplay *GetReplay() { return m_Replay; }
  // replay interface
  void Begin_PrepareInitialBatch();
  void End_PrepareInitialBatch();
  bool Prepare_InitialState(WrappedVkRes *res);
  uint64_t GetSize_InitialState(ResourceId id, const VkInitialContents &initial);
  template <typename SerialiserType>
  bool Serialise_InitialState(SerialiserType &ser, ResourceId id, VkResourceRecord *record,
                              const VkInitialContents *initial);
  void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
  void Apply_InitialState(WrappedVkRes *live, const VkInitialContents &initial);

  void RemapQueueFamilyIndices(uint32_t &srcQueueFamily, uint32_t &dstQueueFamily);
  uint32_t GetQueueFamilyIndex() const { return m_QueueFamilyIdx; }
  bool ReleaseResource(WrappedVkRes *res);

  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d);

  RDResult Initialise(VkInitParams &params, uint64_t sectionVersion, const ReplayOptions &opts);
  uint64_t GetLogVersion() { return m_SectionVersion; }
  void SetStructuredExport(uint64_t sectionVersion)
  {
    m_SectionVersion = sectionVersion;
    m_State = CaptureState::StructuredExport;
  }
  void Shutdown();
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
  void ReplayDraw(VkCommandBuffer cmd, const ActionDescription &action);
  RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);

  SDFile *GetStructuredFile() { return m_StructuredFile; }
  SDFile *DetachStructuredFile()
  {
    SDFile *ret = m_StoredStructuredData;
    m_StoredStructuredData = m_StructuredFile = NULL;
    return ret;
  }
  const APIEvent &GetEvent(uint32_t eventId);
  uint32_t GetMaxEID() { return m_Events.back().eventId; }
  const ActionDescription *GetAction(uint32_t eventId);

  ResourceId GetDescLayoutForDescSet(ResourceId descSet)
  {
    return m_DescriptorSetState[descSet].layout;
  }
  const BindingStorage &GetCurrentDescSetBindingStorage(ResourceId descSet)
  {
    return m_DescriptorSetState[descSet].data;
  }

  uint32_t GetReadbackMemoryIndex(uint32_t resourceCompatibleBitmask);
  uint32_t GetUploadMemoryIndex(uint32_t resourceCompatibleBitmask);
  uint32_t GetGPULocalMemoryIndex(uint32_t resourceCompatibleBitmask);

  MemoryAllocation AllocateMemoryForResource(VkImage im, MemoryScope scope, MemoryType type);
  MemoryAllocation AllocateMemoryForResource(VkBuffer buf, MemoryScope scope, MemoryType type);

  void ChooseMemoryIndices();

  EventFlags GetEventFlags(uint32_t eid) { return m_EventFlags[eid]; }
  rdcarray<EventUsage> GetUsage(ResourceId id) { return m_ResourceUses[id]; }
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
  VkCommandBuffer GetInitStateCmd();

  void AddPendingObjectCleanup(std::function<void()> &&cleanup)
  {
    m_PendingCleanups.push_back(cleanup);
  }
  void CloseInitStateCmd();
  void RemovePendingCommandBuffer(VkCommandBuffer cmd);
  void AddPendingCommandBuffer(VkCommandBuffer cmd);
  void AddFreeCommandBuffer(VkCommandBuffer cmd);
  void SubmitCmds(VkSemaphore *unwrappedWaitSemaphores = NULL,
                  VkPipelineStageFlags *waitStageMask = NULL, uint32_t waitSemaphoreCount = 0);
  VkSemaphore GetNextSemaphore();
  void SubmitSemaphores();
  void FlushQ();

  bool SelectGraphicsComputeQueue(const rdcarray<VkQueueFamilyProperties> &queueProps,
                                  VkDeviceCreateInfo &createInfo, uint32_t &queueFamilyIndex);

  void HandleOOM(bool handle)
  {
    if(handle)
      m_OOMHandler++;
    else
      m_OOMHandler--;
  }
  RDResult FatalErrorCheck() { return m_FatalError; }
  bool HasFatalError() { return m_FatalError != ResultCode::Succeeded; }
  inline void CheckVkResult(VkResult vkr)
  {
    if(vkr == VK_SUCCESS)
      return;
    CheckErrorVkResult(vkr);
  }
  void CheckErrorVkResult(VkResult vkr);

  bool SeparateDepthStencil() const { return m_SeparateDepthStencil; }
  bool NULLDescriptorsAllowed() const { return m_NULLDescriptorsAllowed; }
  bool ExtendedDynamicState() const { return m_ExtendedDynState; }
  bool ExtendedDynamicState2() const { return m_ExtendedDynState2; }
  bool ExtendedDynamicState2Logic() const { return m_ExtendedDynState2Logic; }
  bool ExtendedDynamicState2CPs() const { return m_ExtendedDynState2CPs; }
  bool FragmentShadingRate() const { return m_FragmentShadingRate; }
  bool DynamicColorWrite() const { return m_DynColorWrite; }
  bool DynamicVertexInput() const { return m_DynVertexInput; }
  VulkanRenderState &GetRenderState() { return m_RenderState; }
  void SetActionCB(VulkanActionCallback *cb) { m_ActionCallback = cb; }
  void SetSubmitChain(void *submitChain) { m_SubmitChain = submitChain; }
  static bool IsSupportedExtension(const char *extName);
  static void FilterToSupportedExtensions(rdcarray<VkExtensionProperties> &exts,
                                          rdcarray<VkExtensionProperties> &filtered);
  VkResult FilterDeviceExtensionProperties(VkPhysicalDevice physDev, const char *pLayerName,
                                           uint32_t *pPropertyCount,
                                           VkExtensionProperties *pProperties);
  static VkResult FilterInstanceExtensionProperties(
      const VkEnumerateInstanceExtensionPropertiesChain *pChain, const char *pLayerName,
      uint32_t *pPropertyCount, VkExtensionProperties *pProperties);
  static VkResult GetProvidedDeviceExtensionProperties(uint32_t *pPropertyCount,
                                                       VkExtensionProperties *pProperties);
  static VkResult GetProvidedInstanceExtensionProperties(uint32_t *pPropertyCount,
                                                         VkExtensionProperties *pProperties);

  const VkPhysicalDeviceFeatures &GetDeviceEnabledFeatures()
  {
    return m_PhysicalDeviceData.enabledFeatures;
  }
  const VkPhysicalDeviceFeatures &GetDeviceAvailableFeatures()
  {
    return m_PhysicalDeviceData.availFeatures;
  }
  const VkPhysicalDeviceProperties &GetDeviceProps() { return m_PhysicalDeviceData.props; }
  const VkPhysicalDevicePerformanceQueryFeaturesKHR &GetPhysicalDevicePerformanceQueryFeatures()
  {
    return m_PhysicalDeviceData.performanceQueryFeatures;
  }
  const VkDriverInfo &GetDriverInfo() const { return m_PhysicalDeviceData.driverInfo; }
  uint32_t FindCommandQueueFamily(ResourceId cmdId);
  void InsertCommandQueueFamily(ResourceId cmdId, uint32_t queueFamilyIndex);
  VkQueueFlags GetCommandType(ResourceId cmdId);
  VkQueueFlags GetCommandType() { return GetCommandType(m_LastCmdBufferID); }
  LockedImageStateRef FindImageState(ResourceId id);
  LockedConstImageStateRef FindConstImageState(ResourceId id);
  LockedImageStateRef InsertImageState(VkImage wrappedHandle, ResourceId id, const ImageInfo &info,
                                       FrameRefType refType, bool *inserted = NULL);
  bool EraseImageState(ResourceId id);
  void UpdateImageStates(const rdcflatmap<ResourceId, ImageState> &dstStates);

  inline ImageTransitionInfo GetImageTransitionInfo() const
  {
    return ImageTransitionInfo(m_State, m_QueueFamilyIdx, SeparateDepthStencil());
  }

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

  void ReplayDescriptorSetWrite(VkDevice device, const VkWriteDescriptorSet &writeDesc);
  void ReplayDescriptorSetCopy(VkDevice device, const VkCopyDescriptorSet &copyDesc);

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
                                const VkDebugMarkerObjectTagInfoEXT *pTagInfo);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDebugMarkerSetObjectNameEXT, VkDevice device,
                                const VkDebugMarkerObjectNameInfoEXT *pNameInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDebugMarkerBeginEXT, VkCommandBuffer commandBuffer,
                                const VkDebugMarkerMarkerInfoEXT *pMarker);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDebugMarkerEndEXT, VkCommandBuffer commandBuffer);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDebugMarkerInsertEXT, VkCommandBuffer commandBuffer,
                                const VkDebugMarkerMarkerInfoEXT *pMarker);

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

  void HandlePresent(VkQueue queue, const VkPresentInfoKHR *pPresentInfo,
                     rdcarray<VkSemaphore> &unwrappedWaitSems);

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

  // VK_KHR_external_memory_win32
  VkResult vkGetMemoryWin32HandleKHR(VkDevice device,
                                     const VkMemoryGetWin32HandleInfoKHR *pGetWin32HandleInfo,
                                     HANDLE *pHandle);
  VkResult vkGetMemoryWin32HandlePropertiesKHR(
      VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, HANDLE handle,
      VkMemoryWin32HandlePropertiesKHR *pMemoryWin32HandleProperties);

  // VK_KHR_external_semaphore_win32
  VkResult vkImportSemaphoreWin32HandleKHR(
      VkDevice device, const VkImportSemaphoreWin32HandleInfoKHR *pImportSemaphoreWin32HandleInfo);
  VkResult vkGetSemaphoreWin32HandleKHR(VkDevice device,
                                        const VkSemaphoreGetWin32HandleInfoKHR *pGetWin32HandleInfo,
                                        HANDLE *pHandle);

  // VK_KHR_external_fence_win32
  VkResult vkImportFenceWin32HandleKHR(
      VkDevice device, const VkImportFenceWin32HandleInfoKHR *pImportFenceWin32HandleInfo);
  VkResult vkGetFenceWin32HandleKHR(VkDevice device,
                                    const VkFenceGetWin32HandleInfoKHR *pGetWin32HandleInfo,
                                    HANDLE *pHandle);
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
  // VK_KHR_android_surface
  VkResult vkCreateAndroidSurfaceKHR(VkInstance instance,
                                     const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);

  // VK_ANDROID_external_memory_android_hardware_buffer
  VkResult vkGetAndroidHardwareBufferPropertiesANDROID(
      VkDevice device, const struct AHardwareBuffer *buffer,
      VkAndroidHardwareBufferPropertiesANDROID *pProperties);

  // VK_ANDROID_external_memory_android_hardware_buffer
  VkResult vkGetMemoryAndroidHardwareBufferANDROID(
      VkDevice device, const VkMemoryGetAndroidHardwareBufferInfoANDROID *pInfo,
      struct AHardwareBuffer **pBuffer);
#endif

#if defined(VK_USE_PLATFORM_MACOS_MVK)
  // VK_MVK_macos_surface
  VkResult vkCreateMacOSSurfaceMVK(VkInstance instance,
                                   const VkMacOSSurfaceCreateInfoMVK *pCreateInfo,
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

#if defined(VK_USE_PLATFORM_GGP)
  VkResult vkCreateStreamDescriptorSurfaceGGP(VkInstance instance,
                                              const VkStreamDescriptorSurfaceCreateInfoGGP *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkSurfaceKHR *pSurface);
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
  // VK_KHR_wayland_surface
  VkResult vkCreateWaylandSurfaceKHR(VkInstance instance,
                                     const VkWaylandSurfaceCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);

  VkBool32 vkGetPhysicalDeviceWaylandPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                            uint32_t queueFamilyIndex,
                                                            struct wl_display *display);
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
  void vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags);

  // VK_KHR_get_physical_device_properties2
  void vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                    VkPhysicalDeviceFeatures2 *pFeatures);
  void vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                      VkPhysicalDeviceProperties2 *pProperties);
  void vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format,
                                            VkFormatProperties2 *pFormatProperties);
  VkResult vkGetPhysicalDeviceImageFormatProperties2(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo,
      VkImageFormatProperties2 *pImageFormatProperties);
  void vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t *pCount,
                                                 VkQueueFamilyProperties2 *pQueueFamilyProperties);
  void vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                            VkPhysicalDeviceMemoryProperties2 *pMemoryProperties);
  void vkGetPhysicalDeviceSparseImageFormatProperties2(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
      uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties);

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

  // VK_KHR_external_memory_capabilities
  void vkGetPhysicalDeviceExternalBufferProperties(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
      VkExternalBufferProperties *pExternalBufferProperties);

  // VK_KHR_external_memory_fd
  VkResult vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR *pGetFdInfo, int *pFd);
  VkResult vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType,
                                      int fd, VkMemoryFdPropertiesKHR *pMemoryFdProperties);

  // VK_KHR_external_semaphore_capabilities
  void vkGetPhysicalDeviceExternalSemaphoreProperties(
      VkPhysicalDevice physicalDevice,
      const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
      VkExternalSemaphoreProperties *pExternalSemaphoreProperties);

  // VK_KHR_external_semaphore_fd
  VkResult vkImportSemaphoreFdKHR(VkDevice device,
                                  const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo);
  VkResult vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR *pGetFdInfo, int *pFd);

  // VK_KHR_external_fence_capabilities
  void vkGetPhysicalDeviceExternalFenceProperties(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
      VkExternalFenceProperties *pExternalFenceProperties);

  // VK_KHR_external_fence_fd
  VkResult vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR *pImportFenceFdInfo);
  VkResult vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR *pGetFdInfo, int *pFd);

  // VK_KHR_get_memory_requirements2
  void vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                     VkMemoryRequirements2 *pMemoryRequirements);
  void vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2 *pInfo,
                                      VkMemoryRequirements2 *pMemoryRequirements);
  void vkGetImageSparseMemoryRequirements2(VkDevice device,
                                           const VkImageSparseMemoryRequirementsInfo2 *pInfo,
                                           uint32_t *pSparseMemoryRequirementCount,
                                           VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements);

  // VK_AMD_shader_info
  VkResult vkGetShaderInfoAMD(VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage,
                              VkShaderInfoTypeAMD infoType, size_t *pInfoSize, void *pInfo);

  // VK_KHR_push_descriptor
  void ApplyPushDescriptorWrites(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                                 uint32_t set, uint32_t descriptorWriteCount,
                                 const VkWriteDescriptorSet *pDescriptorWrites);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPushDescriptorSetKHR, VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                                uint32_t set, uint32_t descriptorWriteCount,
                                const VkWriteDescriptorSet *pDescriptorWrites);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPushDescriptorSetWithTemplateKHR,
                                VkCommandBuffer commandBuffer,
                                VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                VkPipelineLayout layout, uint32_t set, const void *pData);

  // VK_KHR_descriptor_update_template
  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorUpdateTemplate, VkDevice device,
                                const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDescriptorUpdateTemplate, VkDevice device,
                                VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkUpdateDescriptorSetWithTemplate, VkDevice device,
                                VkDescriptorSet descriptorSet,
                                VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                const void *pData);

  // VK_KHR_bind_memory2
  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory2, VkDevice device,
                                uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindImageMemory2, VkDevice device,
                                uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos);

  // VK_KHR_maintenance3
  void vkGetDescriptorSetLayoutSupport(VkDevice device,
                                       const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                       VkDescriptorSetLayoutSupport *pSupport);

  // VK_AMD_buffer_marker
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWriteBufferMarkerAMD, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer,
                                VkDeviceSize dstOffset, uint32_t marker);
  // VK_KHR_synchronization2 interaction
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWriteBufferMarker2AMD, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlags2 stage, VkBuffer dstBuffer,
                                VkDeviceSize dstOffset, uint32_t marker);

  // VK_EXT_debug_utils
  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkSetDebugUtilsObjectNameEXT, VkDevice device,
                                const VkDebugUtilsObjectNameInfoEXT *pNameInfo);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkSetDebugUtilsObjectTagEXT, VkDevice device,
                                const VkDebugUtilsObjectTagInfoEXT *pTagInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkQueueBeginDebugUtilsLabelEXT, VkQueue queue,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkQueueEndDebugUtilsLabelEXT, VkQueue queue);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkQueueInsertDebugUtilsLabelEXT, VkQueue queue,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginDebugUtilsLabelEXT, VkCommandBuffer commandBuffer,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndDebugUtilsLabelEXT, VkCommandBuffer commandBuffer);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdInsertDebugUtilsLabelEXT, VkCommandBuffer commandBuffer,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDebugUtilsMessengerEXT, VkInstance instance,
                                const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDebugUtilsMessengerEXT *pMessenger);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDebugUtilsMessengerEXT, VkInstance instance,
                                VkDebugUtilsMessengerEXT messenger,
                                const VkAllocationCallbacks *pAllocator);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkSubmitDebugUtilsMessageEXT, VkInstance instance,
                                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData);

  // VK_KHR_sampler_ycbcr_conversion
  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSamplerYcbcrConversion, VkDevice device,
                                const VkSamplerYcbcrConversionCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkSamplerYcbcrConversion *pYcbcrConversion);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroySamplerYcbcrConversion, VkDevice device,
                                VkSamplerYcbcrConversion ycbcrConversion,
                                const VkAllocationCallbacks *pAllocator);

  // VK_KHR_device_group_creation
  VkResult vkEnumeratePhysicalDeviceGroups(
      VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
      VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties);

  // VK_KHR_device_group
  void vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex,
                                          uint32_t localDeviceIndex, uint32_t remoteDeviceIndex,
                                          VkPeerMemoryFeatureFlags *pPeerMemoryFeatures);
  VkResult vkGetDeviceGroupPresentCapabilitiesKHR(
      VkDevice device, VkDeviceGroupPresentCapabilitiesKHR *pDeviceGroupPresentCapabilities);
  VkResult vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface,
                                                  VkDeviceGroupPresentModeFlagsKHR *pModes);
  VkResult vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice,
                                                   VkSurfaceKHR surface, uint32_t *pRectCount,
                                                   VkRect2D *pRects);
  VkResult vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR *pAcquireInfo,
                                  uint32_t *pImageIndex);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDeviceMask, VkCommandBuffer commandBuffer,
                                uint32_t deviceMask);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatchBase, VkCommandBuffer commandBuffer,
                                uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ,
                                uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

  // Vulkan 1.1

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetDeviceQueue2, VkDevice device,
                                const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue);

  // VK_KHR_get_surface_capabilities2

  VkResult vkGetPhysicalDeviceSurfaceCapabilities2KHR(
      VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
      VkSurfaceCapabilities2KHR *pSurfaceCapabilities);

  VkResult vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice,
                                                 const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                                 uint32_t *pSurfaceFormatCount,
                                                 VkSurfaceFormat2KHR *pSurfaceFormats);

  // VK_KHR_get_display_properties2
  VkResult vkGetPhysicalDeviceDisplayProperties2KHR(VkPhysicalDevice physicalDevice,
                                                    uint32_t *pPropertyCount,
                                                    VkDisplayProperties2KHR *pProperties);

  VkResult vkGetPhysicalDeviceDisplayPlaneProperties2KHR(VkPhysicalDevice physicalDevice,
                                                         uint32_t *pPropertyCount,
                                                         VkDisplayPlaneProperties2KHR *pProperties);

  VkResult vkGetDisplayModeProperties2KHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display,
                                          uint32_t *pPropertyCount,
                                          VkDisplayModeProperties2KHR *pProperties);

  VkResult vkGetDisplayPlaneCapabilities2KHR(VkPhysicalDevice physicalDevice,
                                             const VkDisplayPlaneInfo2KHR *pDisplayPlaneInfo,
                                             VkDisplayPlaneCapabilities2KHR *pCapabilities);

  // VK_KHR_draw_indirect_count
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndirectCount, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                uint32_t stride);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirectCount, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                uint32_t stride);

  // VK_EXT_validation_cache
  VkResult vkCreateValidationCacheEXT(VkDevice device,
                                      const VkValidationCacheCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkValidationCacheEXT *pValidationCache);

  void vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache,
                                   const VkAllocationCallbacks *pAllocator);

  VkResult vkMergeValidationCachesEXT(VkDevice device, VkValidationCacheEXT dstCache,
                                      uint32_t srcCacheCount, const VkValidationCacheEXT *pSrcCaches);

  VkResult vkGetValidationCacheDataEXT(VkDevice device, VkValidationCacheEXT validationCache,
                                       size_t *pDataSize, void *pData);

  // VK_KHR_shared_presentable_image
  VkResult vkGetSwapchainStatusKHR(VkDevice device, VkSwapchainKHR swapchain);

  // VK_KHR_create_renderpass2
  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateRenderPass2, VkDevice device,
                                const VkRenderPassCreateInfo2 *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass2, VkCommandBuffer commandBuffer,
                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                const VkSubpassBeginInfo *pSubpassBeginInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdNextSubpass2, VkCommandBuffer commandBuffer,
                                const VkSubpassBeginInfo *pSubpassBeginInfo,
                                const VkSubpassEndInfo *pSubpassEndInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndRenderPass2, VkCommandBuffer commandBuffer,
                                const VkSubpassEndInfo *pSubpassEndInfo);

  // VK_EXT_transform_feedback

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindTransformFeedbackBuffersEXT,
                                VkCommandBuffer commandBuffer, uint32_t firstBinding,
                                uint32_t bindingCount, const VkBuffer *pBuffers,
                                const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginTransformFeedbackEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstBuffer, uint32_t bufferCount,
                                const VkBuffer *pCounterBuffers,
                                const VkDeviceSize *pCounterBufferOffsets);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndTransformFeedbackEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstBuffer, uint32_t bufferCount,
                                const VkBuffer *pCounterBuffers,
                                const VkDeviceSize *pCounterBufferOffsets);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginQueryIndexedEXT, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags,
                                uint32_t index);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndQueryIndexedEXT, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query, uint32_t index);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndirectByteCountEXT, VkCommandBuffer commandBuffer,
                                uint32_t instanceCount, uint32_t firstInstance,
                                VkBuffer counterBuffer, VkDeviceSize counterBufferOffset,
                                uint32_t counterOffset, uint32_t vertexStride);

  // VK_EXT_conditional_rendering
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginConditionalRenderingEXT,
                                VkCommandBuffer commandBuffer,
                                const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndConditionalRenderingEXT, VkCommandBuffer commandBuffer);

  // VK_EXT_sample_locations

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetSampleLocationsEXT, VkCommandBuffer commandBuffer,
                                const VkSampleLocationsInfoEXT *pSampleLocationsInfo);
  void vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice,
                                                   VkSampleCountFlagBits samples,
                                                   VkMultisamplePropertiesEXT *pMultisampleProperties);

  // VK_EXT_discard_rectangles

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDiscardRectangleEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstDiscardRectangle, uint32_t discardRectangleCount,
                                const VkRect2D *pDiscardRectangles);

  // VK_EXT_calibrated_timestamps

  VkResult vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice,
                                                          uint32_t *pTimeDomainCount,
                                                          VkTimeDomainEXT *pTimeDomains);
  VkResult vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount,
                                        const VkCalibratedTimestampInfoEXT *pTimestampInfos,
                                        uint64_t *pTimestamps, uint64_t *pMaxDeviation);

  // VK_EXT_host_query_reset

  IMPLEMENT_FUNCTION_SERIALISED(void, vkResetQueryPool, VkDevice device, VkQueryPool queryPool,
                                uint32_t firstQuery, uint32_t queryCount);

  // VK_EXT_buffer_device_address
  VkDeviceAddress vkGetBufferDeviceAddressEXT(VkDevice device,
                                              const VkBufferDeviceAddressInfoEXT *pInfo);

  // VK_EXT_hdr_metadata

  IMPLEMENT_FUNCTION_SERIALISED(void, vkSetHdrMetadataEXT, VkDevice device, uint32_t swapchainCount,
                                const VkSwapchainKHR *pSwapchains, const VkHdrMetadataEXT *pMetadata);

  // VK_AMD_display_native_hdr

  IMPLEMENT_FUNCTION_SERIALISED(void, vkSetLocalDimmingAMD, VkDevice device,
                                VkSwapchainKHR swapChain, VkBool32 localDimmingEnable);

  // VK_EXT_full_screen_exclusive

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetDeviceGroupSurfacePresentModes2EXT, VkDevice device,
                                const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                VkDeviceGroupPresentModeFlagsKHR *pModes);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfacePresentModes2EXT,
                                VkPhysicalDevice physicalDevice,
                                const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAcquireFullScreenExclusiveModeEXT, VkDevice device,
                                VkSwapchainKHR swapchain);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkReleaseFullScreenExclusiveModeEXT, VkDevice device,
                                VkSwapchainKHR swapchain);

  // VK_EXT_headless_surface

  VkResult vkCreateHeadlessSurfaceEXT(VkInstance instance,
                                      const VkHeadlessSurfaceCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkSurfaceKHR *pSurface);

  // VK_KHR_pipeline_executable_properties

  VkResult vkGetPipelineExecutablePropertiesKHR(VkDevice device,
                                                const VkPipelineInfoKHR *pPipelineInfo,
                                                uint32_t *pExecutableCount,
                                                VkPipelineExecutablePropertiesKHR *pProperties);
  VkResult vkGetPipelineExecutableStatisticsKHR(VkDevice device,
                                                const VkPipelineExecutableInfoKHR *pExecutableInfo,
                                                uint32_t *pStatisticCount,
                                                VkPipelineExecutableStatisticKHR *pStatistics);
  VkResult vkGetPipelineExecutableInternalRepresentationsKHR(
      VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo,
      uint32_t *pInternalRepresentationCount,
      VkPipelineExecutableInternalRepresentationKHR *pInternalRepresentations);

  // VK_EXT_line_rasterization

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetLineStippleEXT, VkCommandBuffer commandBuffer,
                                uint32_t lineStippleFactor, uint16_t lineStipplePattern);

  // VK_GOOGLE_display_timing

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetRefreshCycleDurationGOOGLE, VkDevice device,
                                VkSwapchainKHR swapchain,
                                VkRefreshCycleDurationGOOGLE *pDisplayTimingProperties);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPastPresentationTimingGOOGLE, VkDevice device,
                                VkSwapchainKHR swapchain, uint32_t *pPresentationTimingCount,
                                VkPastPresentationTimingGOOGLE *pPresentationTimings);

#if defined(VK_USE_PLATFORM_METAL_EXT)
  // VK_EXT_metal_surface
  VkResult vkCreateMetalSurfaceEXT(VkInstance instance,
                                   const VkMetalSurfaceCreateInfoEXT *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface);
#endif

  // VK_KHR_timeline_semaphore

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSemaphoreCounterValue, VkDevice device,
                                VkSemaphore semaphore, uint64_t *pValue);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkWaitSemaphores, VkDevice device,
                                const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkSignalSemaphore, VkDevice device,
                                const VkSemaphoreSignalInfo *pSignalInfo);

  // VK_KHR_performance_query

  VkResult vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
      VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t *pCounterCount,
      VkPerformanceCounterKHR *pCounters, VkPerformanceCounterDescriptionKHR *pCounterDescriptions);
  void vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(
      VkPhysicalDevice physicalDevice,
      const VkQueryPoolPerformanceCreateInfoKHR *pPerformanceQueryCreateInfo, uint32_t *pNumPasses);
  VkResult vkAcquireProfilingLockKHR(VkDevice device, const VkAcquireProfilingLockInfoKHR *pInfo);
  void vkReleaseProfilingLockKHR(VkDevice device);

  // VK_KHR_buffer_device_address

  VkDeviceAddress vkGetBufferDeviceAddress(VkDevice device, VkBufferDeviceAddressInfo *pInfo);
  uint64_t vkGetBufferOpaqueCaptureAddress(VkDevice device, VkBufferDeviceAddressInfo *pInfo);
  uint64_t vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device,
                                                 VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo);

  // VK_EXT_tooling_info

  VkResult vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount,
                                             VkPhysicalDeviceToolProperties *pToolProperties);

  // VK_EXT_private_data

  VkResult vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator,
                                   VkPrivateDataSlot *pPrivateDataSlot);

  void vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot,
                                const VkAllocationCallbacks *pAllocator);

  VkResult vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle,
                            VkPrivateDataSlot privateDataSlot, uint64_t data);

  void vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle,
                        VkPrivateDataSlot privateDataSlot, uint64_t *pData);

  // VK_EXT_extended_dynamic_state
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetCullMode, VkCommandBuffer commandBuffer,
                                VkCullModeFlags cullMode);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetFrontFace, VkCommandBuffer commandBuffer,
                                VkFrontFace frontFace);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetPrimitiveTopology, VkCommandBuffer commandBuffer,
                                VkPrimitiveTopology primitiveTopology);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetViewportWithCount, VkCommandBuffer commandBuffer,
                                uint32_t viewportCount, const VkViewport *pViewports);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetScissorWithCount, VkCommandBuffer commandBuffer,
                                uint32_t scissorCount, const VkRect2D *pScissors);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindVertexBuffers2, VkCommandBuffer commandBuffer,
                                uint32_t firstBinding, uint32_t bindingCount,
                                const VkBuffer *pBuffers, const VkDeviceSize *pOffsets,
                                const VkDeviceSize *pSizes, const VkDeviceSize *pStrides);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthTestEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthTestEnable);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthWriteEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthWriteEnable);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthCompareOp, VkCommandBuffer commandBuffer,
                                VkCompareOp depthCompareOp);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthBoundsTestEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthBoundsTestEnable);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilTestEnable, VkCommandBuffer commandBuffer,
                                VkBool32 stencilTestEnable);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilOp, VkCommandBuffer commandBuffer,
                                VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp,
                                VkStencilOp depthFailOp, VkCompareOp compareOp);

  // VK_KHR_copy_commands2
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBuffer2, VkCommandBuffer commandBuffer,
                                const VkCopyBufferInfo2 *pCopyBufferInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImage2, VkCommandBuffer commandBuffer,
                                const VkCopyImageInfo2 *pCopyImageInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBufferToImage2, VkCommandBuffer commandBuffer,
                                const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImageToBuffer2, VkCommandBuffer commandBuffer,
                                const VkCopyImageToBufferInfo2 *pCopyImageToBufferInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBlitImage2, VkCommandBuffer commandBuffer,
                                const VkBlitImageInfo2 *pBlitImageInfo);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResolveImage2, VkCommandBuffer commandBuffer,
                                const VkResolveImageInfo2 *pResolveImageInfo);

  // VK_KHR_synchronization2
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetEvent2, VkCommandBuffer commandBuffer, VkEvent event,
                                const VkDependencyInfo *pDependencyInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResetEvent2, VkCommandBuffer commandBuffer,
                                VkEvent event, VkPipelineStageFlags2 stageMask);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWaitEvents2, VkCommandBuffer commandBuffer,
                                uint32_t eventCount, const VkEvent *pEvents,
                                const VkDependencyInfo *pDependencyInfos);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPipelineBarrier2, VkCommandBuffer commandBuffer,
                                const VkDependencyInfo *pDependencyInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWriteTimestamp2, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query);

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueSubmit2, VkQueue queue, uint32_t submitCount,
                                const VkSubmitInfo2 *pSubmits, VkFence fence);

  // VK_KHR_present_wait

  IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkWaitForPresentKHR, VkDevice device,
                                VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout);

  // VK_KHR_maintenance4
  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetDeviceBufferMemoryRequirements, VkDevice device,
                                const VkDeviceBufferMemoryRequirements *pInfo,
                                VkMemoryRequirements2 *pMemoryRequirements);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetDeviceImageMemoryRequirements, VkDevice device,
                                const VkDeviceImageMemoryRequirements *pInfo,
                                VkMemoryRequirements2 *pMemoryRequirements);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkGetDeviceImageSparseMemoryRequirements, VkDevice device,
                                const VkDeviceImageMemoryRequirements *pInfo,
                                uint32_t *pSparseMemoryRequirementCount,
                                VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements);

  // VK_EXT_color_write_enable

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetColorWriteEnableEXT, VkCommandBuffer commandBuffer,
                                uint32_t attachmentCount, const VkBool32 *pColorWriteEnables);

  // VK_EXT_extended_dynamic_state2

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthBiasEnable, VkCommandBuffer commandBuffer,
                                VkBool32 depthBiasEnable);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetLogicOpEXT, VkCommandBuffer commandBuffer,
                                VkLogicOp logicOp);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetPatchControlPointsEXT, VkCommandBuffer commandBuffer,
                                uint32_t patchControlPoints);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetPrimitiveRestartEnable, VkCommandBuffer commandBuffer,
                                VkBool32 primitiveRestartEnable);
  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetRasterizerDiscardEnable,
                                VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable);

  // VK_EXT_vertex_input_dynamic_state

  IMPLEMENT_FUNCTION_SERIALISED(
      void, vkCmdSetVertexInputEXT, VkCommandBuffer commandBuffer,
      uint32_t vertexBindingDescriptionCount,
      const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
      uint32_t vertexAttributeDescriptionCount,
      const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions);

  // VK_KHR_dynamic_rendering

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginRendering, VkCommandBuffer commandBuffer,
                                const VkRenderingInfo *pRenderingInfo);

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndRendering, VkCommandBuffer commandBuffer);

  // VK_KHR_fragment_shading_rate

  IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetFragmentShadingRateKHR, VkCommandBuffer commandBuffer,
                                const VkExtent2D *pFragmentSize,
                                const VkFragmentShadingRateCombinerOpKHR combinerOps[2]);
  VkResult vkGetPhysicalDeviceFragmentShadingRatesKHR(
      VkPhysicalDevice physicalDevice, uint32_t *pFragmentShadingRateCount,
      VkPhysicalDeviceFragmentShadingRateKHR *pFragmentShadingRates);

  // VK_EXT_pageable_device_local_memory

  IMPLEMENT_FUNCTION_SERIALISED(void, vkSetDeviceMemoryPriorityEXT, VkDevice device,
                                VkDeviceMemory memory, float priority);

  // VK_EXT_acquire_drm_display
  VkResult vkAcquireDrmDisplayEXT(VkPhysicalDevice physicalDevice, int32_t drmFd,
                                  VkDisplayKHR display);

  VkResult vkGetDrmDisplayEXT(VkPhysicalDevice physicalDevice, int32_t drmFd, uint32_t connectorId,
                              VkDisplayKHR *display);

  // VK_EXT_swapchain_maintenance1
  VkResult vkReleaseSwapchainImagesEXT(VkDevice device,
                                       const VkReleaseSwapchainImagesInfoEXT *pReleaseInfo);
};
