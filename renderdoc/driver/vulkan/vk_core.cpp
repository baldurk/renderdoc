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

#include "vk_core.h"
#include <ctype.h>
#include <algorithm>
#include "core/settings.h"
#include "driver/ihv/amd/amd_rgp.h"
#include "driver/shaders/spirv/spirv_compile.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "vk_debug.h"
#include "vk_replay.h"

#include "stb/stb_image_write.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_VerboseCommandRecording);

RDOC_DEBUG_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing, false,
                  "Every command buffer is submitted and fully flushed to the GPU, to narrow down "
                  "the source of problems.");

uint64_t VkInitParams::GetSerialiseSize()
{
  // misc bytes and fixed integer members
  size_t ret = 128;

  ret += AppName.size() + EngineName.size();

  for(const rdcstr &s : Layers)
    ret += 8 + s.size();

  for(const rdcstr &s : Extensions)
    ret += 8 + s.size();

  return (uint64_t)ret;
}

void VkInitParams::Set(const VkInstanceCreateInfo *pCreateInfo, ResourceId inst)
{
  RDCASSERT(pCreateInfo);

  if(pCreateInfo->pApplicationInfo)
  {
    // we don't support any extensions on appinfo structure
    RDCASSERT(pCreateInfo->pApplicationInfo->pNext == NULL);

    AppName = pCreateInfo->pApplicationInfo->pApplicationName
                  ? pCreateInfo->pApplicationInfo->pApplicationName
                  : "";
    EngineName =
        pCreateInfo->pApplicationInfo->pEngineName ? pCreateInfo->pApplicationInfo->pEngineName : "";

    AppVersion = pCreateInfo->pApplicationInfo->applicationVersion;
    EngineVersion = pCreateInfo->pApplicationInfo->engineVersion;
    APIVersion = pCreateInfo->pApplicationInfo->apiVersion;
  }
  else
  {
    AppName = "";
    EngineName = "";

    AppVersion = 0;
    EngineVersion = 0;
    APIVersion = 0;
  }

  Layers.resize(pCreateInfo->enabledLayerCount);
  Extensions.resize(pCreateInfo->enabledExtensionCount);

  for(uint32_t i = 0; i < pCreateInfo->enabledLayerCount; i++)
    Layers[i] = pCreateInfo->ppEnabledLayerNames[i];

  for(uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++)
    Extensions[i] = pCreateInfo->ppEnabledExtensionNames[i];

  InstanceID = inst;
}

WrappedVulkan::WrappedVulkan()
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(WrappedVulkan));

  if(RenderDoc::Inst().IsReplayApp())
  {
    if(VkMarkerRegion::vk == NULL)
      VkMarkerRegion::vk = this;

    m_State = CaptureState::LoadingReplaying;
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

  m_StructuredFile = m_StoredStructuredData = new SDFile;

  m_SectionVersion = VkInitParams::CurrentVersion;

  rdcspv::Init();
  RenderDoc::Inst().RegisterShutdownFunction(&rdcspv::Shutdown);

  m_Replay = new VulkanReplay(this);

  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();
  tempMemoryTLSSlot = Threading::AllocateTLSSlot();
  debugMessageSinkTLSSlot = Threading::AllocateTLSSlot();

  m_RootEventID = 1;
  m_RootActionID = 1;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_ActionCallback = NULL;
  m_SubmitChain = NULL;

  m_CurChunkOffset = 0;
  m_AddedAction = false;

  m_LastCmdBufferID = ResourceId();

  m_ActionStack.push_back(&m_ParentAction);

  m_SetDeviceLoaderData = NULL;

  m_ResourceManager = new VulkanResourceManager(m_State, this);

  m_Instance = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_Queue = VK_NULL_HANDLE;
  m_QueueFamilyIdx = 0;
  m_DbgReportCallback = VK_NULL_HANDLE;

  m_HeaderChunk = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_FrameCaptureRecord->DataInSerialiser = false;
    m_FrameCaptureRecord->Length = 0;
    m_FrameCaptureRecord->InternalResource = true;
  }
  else
  {
    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();
  }
}

WrappedVulkan::~WrappedVulkan()
{
  // records must be deleted before resource manager shutdown
  if(m_FrameCaptureRecord)
  {
    RDCASSERT(m_FrameCaptureRecord->GetRefCount() == 1);
    m_FrameCaptureRecord->Delete(GetResourceManager());
    m_FrameCaptureRecord = NULL;
  }

  if(VkMarkerRegion::vk == this)
    VkMarkerRegion::vk = NULL;

  SAFE_DELETE(m_StoredStructuredData);

  // in case the application leaked some objects, avoid crashing trying
  // to release them ourselves by clearing the resource manager.
  // In a well-behaved application, this should be a no-op.
  m_ResourceManager->ClearWithoutReleasing();
  SAFE_DELETE(m_ResourceManager);

  SAFE_DELETE(m_FrameReader);

  for(size_t i = 0; i < m_ThreadSerialisers.size(); i++)
    delete m_ThreadSerialisers[i];

  for(size_t i = 0; i < m_ThreadTempMem.size(); i++)
  {
    delete[] m_ThreadTempMem[i]->memory;
    delete m_ThreadTempMem[i];
  }

  delete m_Replay;
}

VkCommandBuffer WrappedVulkan::GetInitStateCmd()
{
  if(initStateCurBatch >= initialStateMaxBatch)
  {
    CloseInitStateCmd();
  }

  if(initStateCurCmd == VK_NULL_HANDLE)
  {
    initStateCurCmd = GetNextCmd();

    if(initStateCurCmd == VK_NULL_HANDLE)
      return VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    VkResult vkr = ObjDisp(initStateCurCmd)->BeginCommandBuffer(Unwrap(initStateCurCmd), &beginInfo);
    CheckVkResult(vkr);

    if(IsReplayMode(m_State))
    {
      VkMarkerRegion::Begin("!!!!RenderDoc Internal: ApplyInitialContents batched list",
                            initStateCurCmd);
    }
    else
    {
      VkMarkerRegion::Begin("!!!!RenderDoc Internal: PrepareInitialContents batched list",
                            initStateCurCmd);
    }
  }

  initStateCurBatch++;

  return initStateCurCmd;
}

void WrappedVulkan::CloseInitStateCmd()
{
  if(initStateCurCmd == VK_NULL_HANDLE)
    return;

  VkMarkerRegion::End(initStateCurCmd);

  VkResult vkr = ObjDisp(initStateCurCmd)->EndCommandBuffer(Unwrap(initStateCurCmd));
  CheckVkResult(vkr);

  initStateCurCmd = VK_NULL_HANDLE;
  initStateCurBatch = 0;
}

VkCommandBuffer WrappedVulkan::GetNextCmd()
{
  VkCommandBuffer ret;

  if(!m_InternalCmds.freecmds.empty())
  {
    ret = m_InternalCmds.freecmds.back();
    m_InternalCmds.freecmds.pop_back();

    ObjDisp(ret)->ResetCommandBuffer(Unwrap(ret), 0);
  }
  else
  {
    VkCommandBufferAllocateInfo cmdInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        NULL,
        Unwrap(m_InternalCmds.cmdpool),
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1,
    };
    VkResult vkr = ObjDisp(m_Device)->AllocateCommandBuffers(Unwrap(m_Device), &cmdInfo, &ret);

    CheckVkResult(vkr);
    if(vkr == VK_SUCCESS)
    {
      if(m_SetDeviceLoaderData)
        m_SetDeviceLoaderData(m_Device, ret);
      else
        SetDispatchTableOverMagicNumber(m_Device, ret);

      GetResourceManager()->WrapResource(Unwrap(m_Device), ret);
    }
    else
    {
      ret = VK_NULL_HANDLE;
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIInitFailed,
                       "Failed to create command buffer: %s", ToStr(vkr).c_str());
    }
  }

  m_InternalCmds.pendingcmds.push_back(ret);

  return ret;
}

void WrappedVulkan::RemovePendingCommandBuffer(VkCommandBuffer cmd)
{
  m_InternalCmds.pendingcmds.removeOne(cmd);
}

void WrappedVulkan::AddPendingCommandBuffer(VkCommandBuffer cmd)
{
  m_InternalCmds.pendingcmds.push_back(cmd);
}

void WrappedVulkan::AddFreeCommandBuffer(VkCommandBuffer cmd)
{
  m_InternalCmds.freecmds.push_back(cmd);
}

void WrappedVulkan::SubmitCmds(VkSemaphore *unwrappedWaitSemaphores,
                               VkPipelineStageFlags *waitStageMask, uint32_t waitSemaphoreCount)
{
  RENDERDOC_PROFILEFUNCTION();
  if(HasFatalError())
    return;

  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  rdcarray<VkCommandBuffer> cmds = m_InternalCmds.pendingcmds;
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = Unwrap(cmds[i]);

  VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      m_SubmitChain,
      waitSemaphoreCount,
      unwrappedWaitSemaphores,
      waitStageMask,
      (uint32_t)cmds.size(),
      &cmds[0],    // command buffers
      0,
      NULL,    // signal semaphores
  };

  // we might have work to do (e.g. debug manager creation command buffer) but no queue, if the
  // device is destroyed immediately. In this case we can just skip the submit. We don't mark these
  // command buffers as submitted in case we're capturing an early frame - we can't lose these so we
  // just defer them until later.
  if(m_Queue == VK_NULL_HANDLE)
    return;

  {
    VkResult vkr = ObjDisp(m_Queue)->QueueSubmit(Unwrap(m_Queue), 1, &submitInfo, VK_NULL_HANDLE);
    CheckVkResult(vkr);
  }

  m_InternalCmds.submittedcmds.append(m_InternalCmds.pendingcmds);
  m_InternalCmds.pendingcmds.clear();

  if(Vulkan_Debug_SingleSubmitFlushing())
    FlushQ();
}

VkSemaphore WrappedVulkan::GetNextSemaphore()
{
  VkSemaphore ret;

  if(!m_InternalCmds.freesems.empty())
  {
    ret = m_InternalCmds.freesems.back();
    m_InternalCmds.freesems.pop_back();

    // assume semaphore is back to unsignaled state after being waited on
  }
  else
  {
    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkResult vkr = ObjDisp(m_Device)->CreateSemaphore(Unwrap(m_Device), &semInfo, NULL, &ret);
    CheckVkResult(vkr);

    GetResourceManager()->WrapResource(Unwrap(m_Device), ret);
  }

  m_InternalCmds.pendingsems.push_back(ret);

  return ret;
}

void WrappedVulkan::SubmitSemaphores()
{
  // nothing to do
  if(m_InternalCmds.pendingsems.empty())
    return;

  // no actual submission, just mark them as 'done with' so they will be
  // recycled on next flush
  m_InternalCmds.submittedsems.append(m_InternalCmds.pendingsems);
  m_InternalCmds.pendingsems.clear();
}

void WrappedVulkan::FlushQ()
{
  RENDERDOC_PROFILEFUNCTION();

  if(HasFatalError())
    return;

  // VKTODOLOW could do away with the need for this function by keeping
  // commands until N presents later, or something, or checking on fences.
  // If we do so, then check each use for FlushQ to see if it needs a
  // CPU-GPU sync or whether it is just looking to recycle command buffers
  // (Particularly the one in vkQueuePresentKHR drawing the overlay)

  // see comment in SubmitQ()
  if(m_Queue != VK_NULL_HANDLE)
  {
    VkResult vkr = ObjDisp(m_Queue)->QueueWaitIdle(Unwrap(m_Queue));
    CheckVkResult(vkr);
  }

  if(Vulkan_Debug_SingleSubmitFlushing() && m_Device != VK_NULL_HANDLE)
  {
    ObjDisp(m_Device)->DeviceWaitIdle(Unwrap(m_Device));
    VkResult vkr = ObjDisp(m_Device)->DeviceWaitIdle(Unwrap(m_Device));
    CheckVkResult(vkr);
  }

  for(std::function<void()> cleanup : m_PendingCleanups)
    cleanup();
  m_PendingCleanups.clear();

  if(!m_InternalCmds.submittedcmds.empty())
  {
    m_InternalCmds.freecmds.append(m_InternalCmds.submittedcmds);
    m_InternalCmds.submittedcmds.clear();
  }

  if(!m_InternalCmds.submittedsems.empty())
  {
    m_InternalCmds.freesems.append(m_InternalCmds.submittedsems);
    m_InternalCmds.submittedsems.clear();
  }
}

VkCommandBuffer WrappedVulkan::GetExtQueueCmd(uint32_t queueFamilyIdx) const
{
  if(queueFamilyIdx >= m_ExternalQueues.size())
  {
    RDCERR("Unsupported queue family %u", queueFamilyIdx);
    return VK_NULL_HANDLE;
  }

  VkCommandBuffer buf = m_ExternalQueues[queueFamilyIdx].ring[0].acquire;

  ObjDisp(buf)->ResetCommandBuffer(Unwrap(buf), 0);

  return buf;
}

void WrappedVulkan::SubmitAndFlushExtQueue(uint32_t queueFamilyIdx)
{
  if(HasFatalError())
    return;

  if(queueFamilyIdx >= m_ExternalQueues.size())
  {
    RDCERR("Unsupported queue family %u", queueFamilyIdx);
    return;
  }

  VkCommandBuffer buf = Unwrap(m_ExternalQueues[queueFamilyIdx].ring[0].acquire);

  VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      m_SubmitChain,
      0,
      NULL,
      NULL,    // wait semaphores
      1,
      &buf,    // command buffers
      0,
      NULL,    // signal semaphores
  };

  VkQueue q = m_ExternalQueues[queueFamilyIdx].queue;

  VkResult vkr = ObjDisp(q)->QueueSubmit(Unwrap(q), 1, &submitInfo, VK_NULL_HANDLE);
  CheckVkResult(vkr);

  ObjDisp(q)->QueueWaitIdle(Unwrap(q));
}

void WrappedVulkan::SubmitAndFlushImageStateBarriers(ImageBarrierSequence &barriers)
{
  if(HasFatalError())
    return;

  if(barriers.empty())
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  rdcarray<VkFence> queueFamilyFences;
  rdcarray<VkFence> submittedFences;
  rdcarray<VkImageMemoryBarrier> batch;

  VkResult vkr;
  for(uint32_t batchIndex = 0; batchIndex < ImageBarrierSequence::MAX_BATCH_COUNT; ++batchIndex)
  {
    for(uint32_t queueFamilyIndex = 0;
        queueFamilyIndex < ImageBarrierSequence::GetMaxQueueFamilyIndex(); ++queueFamilyIndex)
    {
      barriers.ExtractUnwrappedBatch(batchIndex, queueFamilyIndex, batch);
      if(batch.empty())
        continue;

      VkCommandBuffer cmd = GetExtQueueCmd(queueFamilyIndex);
      VkQueue queue = m_ExternalQueues[queueFamilyIndex].queue;

      VkCommandBuffer unwrappedCmd = Unwrap(cmd);

      VkSubmitInfo submitInfo = {
          VK_STRUCTURE_TYPE_SUBMIT_INFO,
          NULL,
          0,
          NULL,
          NULL,    // wait semaphores
          1,
          &unwrappedCmd,    // command buffers
          0,
          NULL,    // signal semaphores
      };

      if(Vulkan_Debug_SingleSubmitFlushing())
      {
        for(auto it = batch.begin(); it != batch.end(); ++it)
        {
          vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);

          DoPipelineBarrier(cmd, 1, it);
          vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);

          vkr = ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
          CheckVkResult(vkr);

          vkr = ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
          CheckVkResult(vkr);
        }
      }
      else
      {
        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        CheckVkResult(vkr);

        DoPipelineBarrier(cmd, (uint32_t)batch.size(), batch.data());

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);

        queueFamilyFences.resize_for_index(queueFamilyIndex);
        VkFence &fence = queueFamilyFences[queueFamilyIndex];
        if(fence == VK_NULL_HANDLE)
        {
          VkFenceCreateInfo fenceInfo = {
              /* sType = */ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
              /* pNext = */ NULL,
              /* flags = */ 0,
          };
          vkr = ObjDisp(m_Device)->CreateFence(Unwrap(m_Device), &fenceInfo, NULL, &fence);
          CheckVkResult(vkr);
        }

        vkr = ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, fence);
        CheckVkResult(vkr);
        submittedFences.push_back(fence);
      }

      batch.clear();
    }
    if(!submittedFences.empty())
    {
      vkr = ObjDisp(m_Device)->WaitForFences(Unwrap(m_Device), (uint32_t)submittedFences.size(),
                                             submittedFences.data(), VK_TRUE, 1000000000);
      CheckVkResult(vkr);
      vkr = ObjDisp(m_Device)->ResetFences(Unwrap(m_Device), (uint32_t)submittedFences.size(),
                                           submittedFences.data());
      CheckVkResult(vkr);
      submittedFences.clear();
    }
  }

  for(VkFence fence : queueFamilyFences)
    ObjDisp(m_Device)->DestroyFence(Unwrap(m_Device), fence, NULL);
}

void WrappedVulkan::InlineSetupImageBarriers(VkCommandBuffer cmd, ImageBarrierSequence &barriers)
{
  rdcarray<VkImageMemoryBarrier> batch;
  barriers.ExtractLastUnwrappedBatchForQueue(m_QueueFamilyIdx, batch);
  if(!batch.empty())
    DoPipelineBarrier(cmd, (uint32_t)batch.size(), batch.data());
}

void WrappedVulkan::InlineCleanupImageBarriers(VkCommandBuffer cmd, ImageBarrierSequence &barriers)
{
  rdcarray<VkImageMemoryBarrier> batch;
  barriers.ExtractFirstUnwrappedBatchForQueue(m_QueueFamilyIdx, batch);
  if(!batch.empty())
    DoPipelineBarrier(cmd, (uint32_t)batch.size(), batch.data());
}

uint32_t WrappedVulkan::HandlePreCallback(VkCommandBuffer commandBuffer, ActionFlags type,
                                          uint32_t multiDrawOffset)
{
  if(!m_ActionCallback)
    return 0;

  // look up the EID this action came from
  ActionUse use(m_CurChunkOffset, 0);
  auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

  if(it == m_ActionUses.end())
  {
    RDCERR("Couldn't find action use entry for %llu", m_CurChunkOffset);
    return 0;
  }

  uint32_t eventId = it->eventId;

  RDCASSERT(eventId != 0);

  // handle all aliases of this action as long as it's not a multidraw
  const ActionDescription *action = GetAction(eventId);

  if(action == NULL || !(action->flags & ActionFlags::MultiAction))
  {
    ++it;
    while(it != m_ActionUses.end() && it->fileOffset == m_CurChunkOffset)
    {
      m_ActionCallback->AliasEvent(eventId, it->eventId);
      ++it;
    }
  }

  eventId += multiDrawOffset;

  if(type == ActionFlags::Drawcall)
    m_ActionCallback->PreDraw(eventId, commandBuffer);
  else if(type == ActionFlags::Dispatch)
    m_ActionCallback->PreDispatch(eventId, commandBuffer);
  else
    m_ActionCallback->PreMisc(eventId, type, commandBuffer);

  return eventId;
}

rdcstr WrappedVulkan::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx == SystemChunk::DriverInit)
    return "vkCreateInstance"_lit;

  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((VulkanChunk)idx);
}

WrappedVulkan::ScopedDebugMessageSink::ScopedDebugMessageSink(WrappedVulkan *driver)
{
  driver->SetDebugMessageSink(this);
  m_pDriver = driver;
}

WrappedVulkan::ScopedDebugMessageSink::~ScopedDebugMessageSink()
{
  m_pDriver->SetDebugMessageSink(NULL);
}

WrappedVulkan::ScopedDebugMessageSink *WrappedVulkan::GetDebugMessageSink()
{
  return (WrappedVulkan::ScopedDebugMessageSink *)Threading::GetTLSValue(debugMessageSinkTLSSlot);
}

void WrappedVulkan::SetDebugMessageSink(WrappedVulkan::ScopedDebugMessageSink *sink)
{
  Threading::SetTLSValue(debugMessageSinkTLSSlot, (void *)sink);
}

byte *WrappedVulkan::GetRingTempMemory(size_t s)
{
  TempMem *mem = (TempMem *)Threading::GetTLSValue(tempMemoryTLSSlot);

  if(!mem || mem->size < s)
  {
    if(mem && mem->size < s)
      RDCWARN("More than %zu bytes needed to unwrap!", mem->size);

    mem = new TempMem();
    mem->size = AlignUp(s, size_t(4 * 1024 * 1024));
    mem->memory = mem->cur = new byte[mem->size];

    SCOPED_LOCK(m_ThreadTempMemLock);
    m_ThreadTempMem.push_back(mem);

    Threading::SetTLSValue(tempMemoryTLSSlot, (void *)mem);
  }

  // if we'd wrap, go back to the start
  if(mem->cur + s >= mem->memory + mem->size)
    mem->cur = mem->memory;

  // save the return value and update the cur pointer
  byte *ret = mem->cur;
  mem->cur = AlignUpPtr(mem->cur + s, 16);
  return ret;
}

byte *WrappedVulkan::GetTempMemory(size_t s)
{
  if(IsReplayMode(m_State))
    return GetRingTempMemory(s);

  TempMem *mem = (TempMem *)Threading::GetTLSValue(tempMemoryTLSSlot);
  if(mem && mem->size >= s)
    return mem->memory;

  // alloc or grow alloc
  TempMem *newmem = mem;

  if(!newmem)
    newmem = new TempMem();

  // free old memory, don't need to keep contents
  if(newmem->memory)
    delete[] newmem->memory;

  // alloc new memory
  newmem->size = s;
  newmem->memory = new byte[s];

  Threading::SetTLSValue(tempMemoryTLSSlot, (void *)newmem);

  // if this is entirely new, save it for deletion on shutdown
  if(!mem)
  {
    SCOPED_LOCK(m_ThreadTempMemLock);
    m_ThreadTempMem.push_back(newmem);
  }

  return newmem->memory;
}

WriteSerialiser &WrappedVulkan::GetThreadSerialiser()
{
  WriteSerialiser *ser = (WriteSerialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
  if(ser)
    return *ser;

  // slow path, but rare
  ser = new WriteSerialiser(new StreamWriter(1024), Ownership::Stream);

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());
  ser->SetVersion(VkInitParams::CurrentVersion);

  Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

  {
    SCOPED_LOCK(m_ThreadSerialisersLock);
    m_ThreadSerialisers.push_back(ser);
  }

  return *ser;
}

static VkResult FillPropertyCountAndList(const VkExtensionProperties *src, uint32_t numExts,
                                         uint32_t *dstCount, VkExtensionProperties *dstProps)
{
  if(dstCount && !dstProps)
  {
    // just returning the number of extensions
    *dstCount = numExts;
    return VK_SUCCESS;
  }
  else if(dstCount && dstProps)
  {
    uint32_t dstSpace = *dstCount;

    // return the number of extensions.
    *dstCount = RDCMIN(numExts, dstSpace);

    // copy as much as there's space for, up to how many there are
    if(src)
      memcpy(dstProps, src, sizeof(VkExtensionProperties) * RDCMIN(numExts, dstSpace));

    // if there was enough space, return success, else incomplete
    if(dstSpace >= numExts)
      return VK_SUCCESS;
    else
      return VK_INCOMPLETE;
  }

  // both parameters were NULL, return incomplete
  return VK_INCOMPLETE;
}

bool operator<(const VkExtensionProperties &a, const VkExtensionProperties &b)
{
  // assume a given extension name is unique, ie. an implementation won't report the
  // same extension with two different spec versions.
  return strcmp(a.extensionName, b.extensionName) < 0;
}

// This list must be kept sorted according to the above sort operator!
static const VkExtensionProperties supportedExtensions[] = {
    {
        VK_AMD_BUFFER_MARKER_EXTENSION_NAME, VK_AMD_BUFFER_MARKER_SPEC_VERSION,
    },
    {
        VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME, VK_AMD_DEVICE_COHERENT_MEMORY_SPEC_VERSION,
    },
    {
        VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME, VK_AMD_DISPLAY_NATIVE_HDR_SPEC_VERSION,
    },
    {
        VK_AMD_GCN_SHADER_EXTENSION_NAME, VK_AMD_GCN_SHADER_SPEC_VERSION,
    },
    {
        VK_AMD_GPU_SHADER_HALF_FLOAT_EXTENSION_NAME, VK_AMD_GPU_SHADER_HALF_FLOAT_SPEC_VERSION,
    },
    {
        VK_AMD_GPU_SHADER_INT16_EXTENSION_NAME, VK_AMD_GPU_SHADER_INT16_SPEC_VERSION,
    },
    {
        VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NAME,
        VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_SPEC_VERSION,
    },
    {
        VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME, VK_AMD_MIXED_ATTACHMENT_SAMPLES_SPEC_VERSION,
    },
    {
        VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_EXTENSION_NAME, VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_SPEC_VERSION,
    },
    {
        VK_AMD_SHADER_BALLOT_EXTENSION_NAME, VK_AMD_SHADER_BALLOT_SPEC_VERSION,
    },
    {
        VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME, VK_AMD_SHADER_CORE_PROPERTIES_SPEC_VERSION,
    },
    {
        VK_AMD_SHADER_EXPLICIT_VERTEX_PARAMETER_EXTENSION_NAME,
        VK_AMD_SHADER_EXPLICIT_VERTEX_PARAMETER_SPEC_VERSION,
    },
    {
        VK_AMD_SHADER_FRAGMENT_MASK_EXTENSION_NAME, VK_AMD_SHADER_FRAGMENT_MASK_SPEC_VERSION,
    },
    {
        VK_AMD_SHADER_IMAGE_LOAD_STORE_LOD_EXTENSION_NAME,
        VK_AMD_SHADER_IMAGE_LOAD_STORE_LOD_SPEC_VERSION,
    },
    {
        VK_AMD_SHADER_TRINARY_MINMAX_EXTENSION_NAME, VK_AMD_SHADER_TRINARY_MINMAX_SPEC_VERSION,
    },
    {
        VK_AMD_TEXTURE_GATHER_BIAS_LOD_EXTENSION_NAME, VK_AMD_TEXTURE_GATHER_BIAS_LOD_SPEC_VERSION,
    },
#ifdef VK_ANDROID_external_memory_android_hardware_buffer
    {
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_SPEC_VERSION,
    },
#endif
    {
        VK_EXT_4444_FORMATS_EXTENSION_NAME, VK_EXT_4444_FORMATS_SPEC_VERSION,
    },
    {
        VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME, VK_EXT_ACQUIRE_DRM_DISPLAY_SPEC_VERSION,
    },
#ifdef VK_EXT_acquire_xlib_display
    {
        VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME, VK_EXT_ACQUIRE_XLIB_DISPLAY_SPEC_VERSION,
    },
#endif
    {
        VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME, VK_EXT_ASTC_DECODE_MODE_SPEC_VERSION,
    },
    {
        VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME,
        VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_SPEC_VERSION,
    },
    {
        VK_EXT_BORDER_COLOR_SWIZZLE_EXTENSION_NAME, VK_EXT_BORDER_COLOR_SWIZZLE_SPEC_VERSION,
    },
    {
        VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_EXT_BUFFER_DEVICE_ADDRESS_SPEC_VERSION,
    },
    {
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, VK_EXT_CALIBRATED_TIMESTAMPS_SPEC_VERSION,
    },
    {
        VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME, VK_EXT_COLOR_WRITE_ENABLE_SPEC_VERSION,
    },
    {
        VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME, VK_EXT_CONDITIONAL_RENDERING_SPEC_VERSION,
    },
    {
        VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
        VK_EXT_CONSERVATIVE_RASTERIZATION_SPEC_VERSION,
    },
    {
        VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME, VK_EXT_CUSTOM_BORDER_COLOR_SPEC_VERSION,
    },
    {
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME, VK_EXT_DEBUG_MARKER_SPEC_VERSION,
    },
    {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_SPEC_VERSION,
    },
    {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_SPEC_VERSION,
    },
    {
        VK_EXT_DEPTH_CLAMP_ZERO_ONE_EXTENSION_NAME, VK_EXT_DEPTH_CLAMP_ZERO_ONE_SPEC_VERSION,
    },
    {
        VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME, VK_EXT_DEPTH_CLIP_CONTROL_SPEC_VERSION,
    },
    {
        VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME, VK_EXT_DEPTH_CLIP_ENABLE_SPEC_VERSION,
    },
    {
        VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME, VK_EXT_DEPTH_RANGE_UNRESTRICTED_SPEC_VERSION,
    },
    {
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_SPEC_VERSION,
    },
    {
        VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME, VK_EXT_DIRECT_MODE_DISPLAY_SPEC_VERSION,
    },
    {
        VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME, VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION,
    },
    {
        VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME, VK_EXT_DISPLAY_CONTROL_SPEC_VERSION,
    },
    {
        VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME, VK_EXT_DISPLAY_SURFACE_COUNTER_SPEC_VERSION,
    },
    {
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, VK_EXT_EXTENDED_DYNAMIC_STATE_SPEC_VERSION,
    },
    {
        VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME, VK_EXT_EXTENDED_DYNAMIC_STATE_2_SPEC_VERSION,
    },
    {
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_SPEC_VERSION,
    },
    {
        VK_EXT_FILTER_CUBIC_EXTENSION_NAME, VK_EXT_FILTER_CUBIC_SPEC_VERSION,
    },
    {
        VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, VK_EXT_FRAGMENT_DENSITY_MAP_SPEC_VERSION,
    },
    {
        VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME, VK_EXT_FRAGMENT_DENSITY_MAP_2_SPEC_VERSION,
    },
    {
        VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME, VK_EXT_FRAGMENT_SHADER_INTERLOCK_SPEC_VERSION,
    },
#ifdef VK_EXT_full_screen_exclusive
    {
        VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME, VK_EXT_FULL_SCREEN_EXCLUSIVE_SPEC_VERSION,
    },
#endif
    {
        VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME, VK_EXT_GLOBAL_PRIORITY_SPEC_VERSION,
    },
    {
        VK_EXT_GLOBAL_PRIORITY_QUERY_EXTENSION_NAME, VK_EXT_GLOBAL_PRIORITY_QUERY_SPEC_VERSION,
    },
    {
        VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, VK_EXT_GRAPHICS_PIPELINE_LIBRARY_SPEC_VERSION,
    },
    {
        VK_EXT_HDR_METADATA_EXTENSION_NAME, VK_EXT_HDR_METADATA_SPEC_VERSION,
    },
    {
        VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, VK_EXT_HEADLESS_SURFACE_SPEC_VERSION,
    },
    {
        VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, VK_EXT_HOST_QUERY_RESET_SPEC_VERSION,
    },
    {
        VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME, VK_EXT_IMAGE_ROBUSTNESS_SPEC_VERSION,
    },
    {
        VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME, VK_EXT_IMAGE_VIEW_MIN_LOD_SPEC_VERSION,
    },
    {
        VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, VK_EXT_INDEX_TYPE_UINT8_SPEC_VERSION,
    },
    {
        VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME, VK_EXT_INLINE_UNIFORM_BLOCK_SPEC_VERSION,
    },
    {
        VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME, VK_EXT_LINE_RASTERIZATION_SPEC_VERSION,
    },
    {
        VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME, VK_EXT_LOAD_STORE_OP_NONE_SPEC_VERSION,
    },
    {
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, VK_EXT_MEMORY_BUDGET_SPEC_VERSION,
    },
    {
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, VK_EXT_MEMORY_PRIORITY_SPEC_VERSION,
    },
#ifdef VK_EXT_metal_surface
    {
        VK_EXT_METAL_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME,
        VK_EXT_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_SPEC_VERSION,
    },
    {
        VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME, VK_EXT_MUTABLE_DESCRIPTOR_TYPE_SPEC_VERSION,
    },
    {
        VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME, VK_EXT_NON_SEAMLESS_CUBE_MAP_SPEC_VERSION,
    },
    {
        VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,
        VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_SPEC_VERSION,
    },
    {
        VK_EXT_PCI_BUS_INFO_EXTENSION_NAME, VK_EXT_PCI_BUS_INFO_SPEC_VERSION,
    },
    {
        VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
        VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_SPEC_VERSION,
    },
    {
        VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
        VK_EXT_PIPELINE_CREATION_FEEDBACK_SPEC_VERSION,
    },
    {
        VK_EXT_POST_DEPTH_COVERAGE_EXTENSION_NAME, VK_EXT_POST_DEPTH_COVERAGE_SPEC_VERSION,
    },
    {
        VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME,
        VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_SPEC_VERSION,
    },
    {
        VK_EXT_PRIMITIVES_GENERATED_QUERY_EXTENSION_NAME,
        VK_EXT_PRIMITIVES_GENERATED_QUERY_SPEC_VERSION,
    },
    {
        VK_EXT_PRIVATE_DATA_EXTENSION_NAME, VK_EXT_PRIVATE_DATA_SPEC_VERSION,
    },
    {
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, VK_EXT_QUEUE_FAMILY_FOREIGN_SPEC_VERSION,
    },
    {
        VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME,
        VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_SPEC_VERSION,
    },
    {
        VK_EXT_RGBA10X6_FORMATS_EXTENSION_NAME, VK_EXT_RGBA10X6_FORMATS_SPEC_VERSION,
    },
    {
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_SPEC_VERSION,
    },
    {
        VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME, VK_EXT_SAMPLE_LOCATIONS_SPEC_VERSION,
    },
    {
        VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME, VK_EXT_SAMPLER_FILTER_MINMAX_SPEC_VERSION,
    },
    {
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, VK_EXT_SCALAR_BLOCK_LAYOUT_SPEC_VERSION,
    },
    {
        VK_EXT_SEPARATE_STENCIL_USAGE_EXTENSION_NAME, VK_EXT_SEPARATE_STENCIL_USAGE_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME, VK_EXT_SHADER_ATOMIC_FLOAT_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_ATOMIC_FLOAT_2_EXTENSION_NAME, VK_EXT_SHADER_ATOMIC_FLOAT_2_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
        VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, VK_EXT_SHADER_IMAGE_ATOMIC_INT64_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME, VK_EXT_SHADER_STENCIL_EXPORT_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME, VK_EXT_SHADER_SUBGROUP_BALLOT_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME, VK_EXT_SHADER_SUBGROUP_VOTE_SPEC_VERSION,
    },
    {
        VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
        VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_SPEC_VERSION,
    },
    {
        VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, VK_EXT_SUBGROUP_SIZE_CONTROL_SPEC_VERSION,
    },
    {
        VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME, VK_EXT_SURFACE_MAINTENANCE_1_SPEC_VERSION,
    },
    {
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, VK_EXT_SWAPCHAIN_COLOR_SPACE_SPEC_VERSION,
    },
    {
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME, VK_EXT_SWAPCHAIN_MAINTENANCE_1_SPEC_VERSION,
    },
    {
        VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME, VK_EXT_TEXEL_BUFFER_ALIGNMENT_SPEC_VERSION,
    },
    {
        VK_EXT_TOOLING_INFO_EXTENSION_NAME, VK_EXT_TOOLING_INFO_SPEC_VERSION,
    },
    {
        VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME, VK_EXT_TRANSFORM_FEEDBACK_SPEC_VERSION,
    },
    {
        VK_EXT_VALIDATION_CACHE_EXTENSION_NAME, VK_EXT_VALIDATION_CACHE_SPEC_VERSION,
    },
    {
        VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, VK_EXT_VALIDATION_FEATURES_SPEC_VERSION,
    },
    {
        VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME, VK_EXT_VALIDATION_FLAGS_SPEC_VERSION,
    },
    {
        VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME, VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_SPEC_VERSION,
    },
    {
        VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
        VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_SPEC_VERSION,
    },
    {
        VK_EXT_YCBCR_2PLANE_444_FORMATS_EXTENSION_NAME, VK_EXT_YCBCR_2PLANE_444_FORMATS_SPEC_VERSION,
    },
    {
        VK_EXT_YCBCR_IMAGE_ARRAYS_EXTENSION_NAME, VK_EXT_YCBCR_IMAGE_ARRAYS_SPEC_VERSION,
    },
#ifdef VK_GGP_frame_token
    {
        VK_GGP_FRAME_TOKEN_EXTENSION_NAME, VK_GGP_FRAME_TOKEN_SPEC_VERSION,
    },
#endif
#ifdef VK_GGP_stream_descriptor_surface
    {
        VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME, VK_GGP_STREAM_DESCRIPTOR_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_GOOGLE_DECORATE_STRING_EXTENSION_NAME, VK_GOOGLE_DECORATE_STRING_SPEC_VERSION,
    },
    {
        VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME, VK_GOOGLE_DISPLAY_TIMING_SPEC_VERSION,
    },
    {
        VK_GOOGLE_HLSL_FUNCTIONALITY_1_EXTENSION_NAME, VK_GOOGLE_HLSL_FUNCTIONALITY_1_SPEC_VERSION,
    },
    {
        VK_GOOGLE_SURFACELESS_QUERY_EXTENSION_NAME, VK_GOOGLE_SURFACELESS_QUERY_SPEC_VERSION,
    },
    {
        VK_GOOGLE_USER_TYPE_EXTENSION_NAME, VK_GOOGLE_USER_TYPE_SPEC_VERSION,
    },
    {
        VK_IMG_FILTER_CUBIC_EXTENSION_NAME, VK_IMG_FILTER_CUBIC_SPEC_VERSION,
    },
    {
        VK_IMG_FORMAT_PVRTC_EXTENSION_NAME, VK_IMG_FORMAT_PVRTC_SPEC_VERSION,
    },
    {
        VK_KHR_16BIT_STORAGE_EXTENSION_NAME, VK_KHR_16BIT_STORAGE_SPEC_VERSION,
    },
    {
        VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_SPEC_VERSION,
    },
#ifdef VK_KHR_android_surface
    {
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, VK_KHR_BIND_MEMORY_2_SPEC_VERSION,
    },
    {
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_SPEC_VERSION,
    },
    {
        VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME, VK_KHR_COPY_COMMANDS_2_SPEC_VERSION,
    },
    {
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_SPEC_VERSION,
    },
    {
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, VK_KHR_DEDICATED_ALLOCATION_SPEC_VERSION,
    },
    {
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_SPEC_VERSION,
    },
    {
        VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
        VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_SPEC_VERSION,
    },
    {
        VK_KHR_DEVICE_GROUP_EXTENSION_NAME, VK_KHR_DEVICE_GROUP_SPEC_VERSION,
    },
    {
        VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME, VK_KHR_DEVICE_GROUP_CREATION_SPEC_VERSION,
    },
#ifdef VK_KHR_display
    {
        VK_KHR_DISPLAY_EXTENSION_NAME, VK_KHR_DISPLAY_SPEC_VERSION,
    },
#endif
#ifdef VK_KHR_display_swapchain
    {
        VK_KHR_DISPLAY_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DISPLAY_SWAPCHAIN_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME, VK_KHR_DRAW_INDIRECT_COUNT_SPEC_VERSION,
    },
    {
        VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, VK_KHR_DRIVER_PROPERTIES_SPEC_VERSION,
    },
    {
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME, VK_KHR_EXTERNAL_FENCE_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_CAPABILITIES_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_FENCE_FD_SPEC_VERSION,
    },
#ifdef VK_KHR_external_fence_win32
    {
        VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME, VK_KHR_EXTERNAL_FENCE_WIN32_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_SPEC_VERSION,
    },
#ifdef VK_KHR_external_memory_win32
    {
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_SPEC_VERSION,
    },
    {
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_SPEC_VERSION,
    },
#ifdef VK_KHR_external_semaphore_win32
    {
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_WIN32_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME, VK_KHR_FORMAT_FEATURE_FLAGS_2_SPEC_VERSION,
    },
    {
        VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
        VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_SPEC_VERSION,
    },
    {
        VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, VK_KHR_FRAGMENT_SHADING_RATE_SPEC_VERSION,
    },
    {
        VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME, VK_KHR_GET_DISPLAY_PROPERTIES_2_SPEC_VERSION,
    },
    {
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_GET_MEMORY_REQUIREMENTS_2_SPEC_VERSION,
    },
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION,
    },
    {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_GET_SURFACE_CAPABILITIES_2_SPEC_VERSION,
    },
    {
        VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME, VK_KHR_GLOBAL_PRIORITY_SPEC_VERSION,
    },
    {
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, VK_KHR_IMAGE_FORMAT_LIST_SPEC_VERSION,
    },
    {
        VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME, VK_KHR_IMAGELESS_FRAMEBUFFER_SPEC_VERSION,
    },
    {
        VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME, VK_KHR_INCREMENTAL_PRESENT_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME, VK_KHR_MAINTENANCE_1_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE_2_EXTENSION_NAME, VK_KHR_MAINTENANCE_2_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE_3_EXTENSION_NAME, VK_KHR_MAINTENANCE_3_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_SPEC_VERSION,
    },
    {
        VK_KHR_MULTIVIEW_EXTENSION_NAME, VK_KHR_MULTIVIEW_SPEC_VERSION,
    },
    {
        VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, VK_KHR_PERFORMANCE_QUERY_SPEC_VERSION,
    },
    {
        VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME,
        VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_SPEC_VERSION,
    },
    {
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME, VK_KHR_PIPELINE_LIBRARY_SPEC_VERSION,
    },
    {
        VK_KHR_PRESENT_ID_EXTENSION_NAME, VK_KHR_PRESENT_ID_SPEC_VERSION,
    },
    {
        VK_KHR_PRESENT_WAIT_EXTENSION_NAME, VK_KHR_PRESENT_WAIT_SPEC_VERSION,
    },
    {
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_SPEC_VERSION,
    },
    {
        VK_KHR_RELAXED_BLOCK_LAYOUT_EXTENSION_NAME, VK_KHR_RELAXED_BLOCK_LAYOUT_SPEC_VERSION,
    },
    {
        VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
        VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_SPEC_VERSION,
    },
    {
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, VK_KHR_SAMPLER_YCBCR_CONVERSION_SPEC_VERSION,
    },
    {
        VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
        VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME, VK_KHR_SHADER_ATOMIC_INT64_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_CLOCK_EXTENSION_NAME, VK_KHR_SHADER_CLOCK_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, VK_KHR_SHADER_FLOAT16_INT8_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_INTEGER_DOT_PRODUCT_EXTENSION_NAME,
        VK_KHR_SHADER_INTEGER_DOT_PRODUCT_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, VK_KHR_SHADER_NON_SEMANTIC_INFO_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
        VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_EXTENSION_NAME,
        VK_KHR_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_SPEC_VERSION,
    },
    {
        VK_KHR_SHADER_TERMINATE_INVOCATION_EXTENSION_NAME,
        VK_KHR_SHADER_TERMINATE_INVOCATION_SPEC_VERSION,
    },
    {
        VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME, VK_KHR_SHARED_PRESENTABLE_IMAGE_SPEC_VERSION,
    },
    {
        VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SPIRV_1_4_SPEC_VERSION,
    },
    {
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_SPEC_VERSION,
    },
    {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION,
    },
    {
        VK_KHR_SURFACE_PROTECTED_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_SURFACE_PROTECTED_CAPABILITIES_SPEC_VERSION,
    },
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_SPEC_VERSION,
    },
    {
        VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_SPEC_VERSION,
    },
    {
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_SPEC_VERSION,
    },
    {
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_TIMELINE_SEMAPHORE_SPEC_VERSION,
    },
    {
        VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME,
        VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_SPEC_VERSION,
    },
    {
        VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME, VK_KHR_VARIABLE_POINTERS_SPEC_VERSION,
    },
    {
        VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, VK_KHR_VULKAN_MEMORY_MODEL_SPEC_VERSION,
    },
#ifdef VK_KHR_wayland_surface
    {
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_SPEC_VERSION,
    },
#endif
#ifdef VK_KHR_win32_keyed_mutex
    {
        VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME, VK_KHR_WIN32_KEYED_MUTEX_SPEC_VERSION,
    },
#endif
#ifdef VK_KHR_win32_surface
    {
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_EXTENSION_NAME,
        VK_KHR_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_SPEC_VERSION,
    },
#ifdef VK_KHR_xcb_surface
    {
        VK_KHR_XCB_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_SPEC_VERSION,
    },
#endif
#ifdef VK_KHR_xlib_surface
    {
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME,
        VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_SPEC_VERSION,
    },
#ifdef VK_MVK_macos_surface
    {
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME, VK_MVK_MACOS_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_NV_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME, VK_NV_COMPUTE_SHADER_DERIVATIVES_SPEC_VERSION,
    },
    {
        VK_NV_DEDICATED_ALLOCATION_EXTENSION_NAME, VK_NV_DEDICATED_ALLOCATION_SPEC_VERSION,
    },
    {
        VK_NV_EXTERNAL_MEMORY_EXTENSION_NAME, VK_NV_EXTERNAL_MEMORY_SPEC_VERSION,
    },
    {
        VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_NV_EXTERNAL_MEMORY_CAPABILITIES_SPEC_VERSION,
    },
#ifdef VK_NV_external_memory_win32
    {
        VK_NV_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, VK_NV_EXTERNAL_MEMORY_WIN32_SPEC_VERSION,
    },
#endif
    {
        VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
        VK_NV_FRAGMENT_SHADER_BARYCENTRIC_SPEC_VERSION,
    },
    {
        VK_NV_GEOMETRY_SHADER_PASSTHROUGH_EXTENSION_NAME,
        VK_NV_GEOMETRY_SHADER_PASSTHROUGH_SPEC_VERSION,
    },
    {
        VK_NV_SAMPLE_MASK_OVERRIDE_COVERAGE_EXTENSION_NAME,
        VK_NV_SAMPLE_MASK_OVERRIDE_COVERAGE_SPEC_VERSION,
    },
    {
        VK_NV_SHADER_IMAGE_FOOTPRINT_EXTENSION_NAME, VK_NV_SHADER_IMAGE_FOOTPRINT_SPEC_VERSION,
    },
    {
        VK_NV_SHADER_SUBGROUP_PARTITIONED_EXTENSION_NAME,
        VK_NV_SHADER_SUBGROUP_PARTITIONED_SPEC_VERSION,
    },
    {
        VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME, VK_NV_VIEWPORT_ARRAY2_SPEC_VERSION,
    },
#ifdef VK_NV_win32_keyed_mutex
    {
        VK_NV_WIN32_KEYED_MUTEX_EXTENSION_NAME, VK_NV_WIN32_KEYED_MUTEX_SPEC_VERSION,
    },
#endif
    {
        VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME,
        VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_SPEC_VERSION,
    },
    {
        VK_QCOM_RENDER_PASS_SHADER_RESOLVE_EXTENSION_NAME,
        VK_QCOM_RENDER_PASS_SHADER_RESOLVE_SPEC_VERSION,
    },
    {
        VK_QCOM_RENDER_PASS_STORE_OPS_EXTENSION_NAME, VK_QCOM_RENDER_PASS_STORE_OPS_SPEC_VERSION,
    },
    {
        VK_VALVE_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME, VK_VALVE_MUTABLE_DESCRIPTOR_TYPE_SPEC_VERSION,
    },
};

// this is the list of extensions we provide - regardless of whether the ICD supports them
static const VkExtensionProperties renderdocProvidedDeviceExtensions[] = {
    {VK_EXT_DEBUG_MARKER_EXTENSION_NAME, VK_EXT_DEBUG_MARKER_SPEC_VERSION},
    {VK_EXT_TOOLING_INFO_EXTENSION_NAME, VK_EXT_TOOLING_INFO_SPEC_VERSION},
};

static const VkExtensionProperties renderdocProvidedInstanceExtensions[] = {
    {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_SPEC_VERSION},
};

bool WrappedVulkan::IsSupportedExtension(const char *extName)
{
  for(size_t i = 0; i < ARRAY_COUNT(supportedExtensions); i++)
    if(!strcmp(supportedExtensions[i].extensionName, extName))
      return true;

  return false;
}

void WrappedVulkan::FilterToSupportedExtensions(rdcarray<VkExtensionProperties> &exts,
                                                rdcarray<VkExtensionProperties> &filtered)
{
  // now we can step through both lists with two pointers,
  // instead of doing an O(N*M) lookup searching through each
  // supported extension for each reported extension.
  size_t i = 0;
  for(auto it = exts.begin(); it != exts.end() && i < ARRAY_COUNT(supportedExtensions);)
  {
    int nameCompare = strcmp(it->extensionName, supportedExtensions[i].extensionName);
    // if neither is less than the other, the extensions are equal
    if(nameCompare == 0)
    {
      // warn on spec version mismatch if it's newer than ours, but allow it.
      if(supportedExtensions[i].specVersion < it->specVersion)
        RDCWARN(
            "Spec versions of %s are different between supported extension (%d) and reported (%d)!",
            it->extensionName, supportedExtensions[i].specVersion, it->specVersion);

      filtered.push_back(*it);
      ++it;
      ++i;
    }
    else if(nameCompare < 0)
    {
      // reported extension was less. It's not supported - skip past it and continue
      ++it;
    }
    else if(nameCompare > 0)
    {
      // supported extension was less. Check the next supported extension
      ++i;
    }
  }
}

static bool filterWarned = false;

VkResult WrappedVulkan::FilterDeviceExtensionProperties(VkPhysicalDevice physDev,
                                                        const char *pLayerName,
                                                        uint32_t *pPropertyCount,
                                                        VkExtensionProperties *pProperties)
{
  VkResult vkr;

  // first fetch the list of extensions ourselves
  uint32_t numExts;
  vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), pLayerName, &numExts,
                                                             NULL);

  if(vkr != VK_SUCCESS)
    return vkr;

  rdcarray<VkExtensionProperties> exts;
  exts.resize(numExts);
  vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), pLayerName, &numExts,
                                                             &exts[0]);

  if(vkr != VK_SUCCESS)
    return vkr;

  // filter the list of extensions to only the ones we support.

  // sort the reported extensions
  std::sort(exts.begin(), exts.end());

  rdcarray<VkExtensionProperties> filtered;
  filtered.reserve(exts.size());
  FilterToSupportedExtensions(exts, filtered);

  if(pLayerName == NULL)
  {
    InstanceDeviceInfo *instDevInfo = GetRecord(m_Instance)->instDevInfo;

    // extensions with conditional support
    filtered.removeIf([instDevInfo, physDev](const VkExtensionProperties &ext) {
      if(!strcmp(ext.extensionName, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME))
      {
        // require GPDP2
        if(instDevInfo->ext_KHR_get_physical_device_properties2)
        {
          VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityFeatures = {
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};
          VkPhysicalDeviceFeatures2 base = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
          base.pNext = &fragmentDensityFeatures;
          ObjDisp(physDev)->GetPhysicalDeviceFeatures2(Unwrap(physDev), &base);

          if(fragmentDensityFeatures.fragmentDensityMapNonSubsampledImages)
          {
            // supported, don't remove
            return false;
          }
          else if(!filterWarned)
          {
            RDCWARN(
                "VkPhysicalDeviceFragmentDensityMapFeaturesEXT."
                "fragmentDensityMapNonSubsampledImages is "
                "false, can't support capture of VK_EXT_fragment_density_map");
          }
        }

        // if it wasn't supported, remove the extension
        return true;
      }

      if(!strcmp(ext.extensionName, VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
      {
        // require GPDP2
        if(instDevInfo->ext_KHR_get_physical_device_properties2)
        {
          VkPhysicalDeviceBufferDeviceAddressFeaturesEXT bufaddr = {
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT};
          VkPhysicalDeviceFeatures2 base = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
          base.pNext = &bufaddr;
          ObjDisp(physDev)->GetPhysicalDeviceFeatures2(Unwrap(physDev), &base);

          if(bufaddr.bufferDeviceAddressCaptureReplay)
          {
            // supported, don't remove
            return false;
          }
          else if(!filterWarned)
          {
            RDCWARN(
                "VkPhysicalDeviceBufferDeviceAddressFeaturesEXT.bufferDeviceAddressCaptureReplay "
                "is false, can't support capture of VK_EXT_buffer_device_address");
          }
        }

        // if it wasn't supported, remove the extension
        return true;
      }

      if(!strcmp(ext.extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
      {
        // require GPDP2
        if(instDevInfo->ext_KHR_get_physical_device_properties2)
        {
          VkPhysicalDeviceBufferDeviceAddressFeatures bufaddr = {
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
          VkPhysicalDeviceFeatures2 base = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
          base.pNext = &bufaddr;
          ObjDisp(physDev)->GetPhysicalDeviceFeatures2(Unwrap(physDev), &base);

          if(bufaddr.bufferDeviceAddressCaptureReplay)
          {
            // supported, don't remove
            return false;
          }
          else if(!filterWarned)
          {
            RDCWARN(
                "VkPhysicalDeviceBufferDeviceAddressFeaturesKHR.bufferDeviceAddressCaptureReplay "
                "is false, can't support capture of VK_KHR_buffer_device_address");
          }
        }

        // if it wasn't supported, remove the extension
        return true;
      }

      // not an extension with conditional support, don't remove
      return false;
    });

    // now we can add extensions that we provide ourselves (note this isn't sorted, but we
    // don't have to sort the results, the sorting was just so we could filter optimally).
    filtered.append(&renderdocProvidedDeviceExtensions[0],
                    ARRAY_COUNT(renderdocProvidedDeviceExtensions));
  }

  filterWarned = true;

  return FillPropertyCountAndList(&filtered[0], (uint32_t)filtered.size(), pPropertyCount,
                                  pProperties);
}

VkResult WrappedVulkan::FilterInstanceExtensionProperties(
    const VkEnumerateInstanceExtensionPropertiesChain *pChain, const char *pLayerName,
    uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
  VkResult vkr;

  // first fetch the list of extensions ourselves
  uint32_t numExts;
  vkr = pChain->CallDown(pLayerName, &numExts, NULL);

  if(vkr != VK_SUCCESS)
    return vkr;

  rdcarray<VkExtensionProperties> exts;
  exts.resize(numExts);
  vkr = pChain->CallDown(pLayerName, &numExts, &exts[0]);

  if(vkr != VK_SUCCESS)
    return vkr;

  // filter the list of extensions to only the ones we support.

  // sort the reported extensions
  std::sort(exts.begin(), exts.end());

  rdcarray<VkExtensionProperties> filtered;
  filtered.reserve(exts.size());

  FilterToSupportedExtensions(exts, filtered);

  if(pLayerName == NULL)
  {
    // now we can add extensions that we provide ourselves (note this isn't sorted, but we
    // don't have to sort the results, the sorting was just so we could filter optimally).
    filtered.append(&renderdocProvidedInstanceExtensions[0],
                    ARRAY_COUNT(renderdocProvidedInstanceExtensions));
  }

  return FillPropertyCountAndList(&filtered[0], (uint32_t)filtered.size(), pPropertyCount,
                                  pProperties);
}

VkResult WrappedVulkan::GetProvidedDeviceExtensionProperties(uint32_t *pPropertyCount,
                                                             VkExtensionProperties *pProperties)
{
  return FillPropertyCountAndList(renderdocProvidedDeviceExtensions,
                                  (uint32_t)ARRAY_COUNT(renderdocProvidedDeviceExtensions),
                                  pPropertyCount, pProperties);
}

VkResult WrappedVulkan::GetProvidedInstanceExtensionProperties(uint32_t *pPropertyCount,
                                                               VkExtensionProperties *pProperties)
{
  return FillPropertyCountAndList(NULL, 0, pPropertyCount, pProperties);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(frameNumber, m_CapturedFrames.back().frameNumber);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GetReplay()->WriteFrameRecord().frameInfo.frameNumber = frameNumber;
    RDCEraseEl(GetReplay()->WriteFrameRecord().frameInfo.stats);
  }

  return true;
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
  CACHE_THREAD_SERIALISER();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  SERIALISE_ELEMENT_LOCAL(PresentedImage, GetResID(presentImage)).TypedAs("VkImage"_lit);

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedVulkan::FirstFrame()
{
  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));

    m_FirstFrameCapture = true;

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  SCOPED_LOCK(m_ImageStatesLock);

  for(auto it = m_ImageStates.begin(); it != m_ImageStates.end(); ++it)
  {
    it->second.LockWrite()->FixupStorageReferences();
  }

  GetResourceManager()->SerialiseImageStates(ser, m_ImageStates);

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

void WrappedVulkan::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  m_CaptureFailure = false;

  RDCLOG("Starting capture");

  if(m_Queue == VK_NULL_HANDLE && m_QueueFamilyIdx != ~0U)
  {
    RDCLOG("Creating desired queue as none was obtained by the application");

    VkQueue q = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_Device, m_QueueFamilyIdx, 0, &q);
  }

  Atomic::Dec32(&m_ReuseEnabled);

  m_CaptureTimer.Restart();

  GetResourceManager()->ResetCaptureStartTime();

  m_AppControlledCapture = true;

  m_SubmitCounter = 0;

  FrameDescription frame;
  frame.frameNumber = ~0U;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  m_DebugMessages.clear();

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->ClearReferencedMemory();

  // need to do all this atomically so that no other commands
  // will check to see if they need to markdirty or markpendingdirty
  // and go into the frame record.
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    // wait for all work to finish and apply a memory barrier to ensure all memory is visible
    for(size_t i = 0; i < m_QueueFamilies.size(); i++)
    {
      for(uint32_t q = 0; q < m_QueueFamilyCounts[i]; q++)
      {
        if(m_QueueFamilies[i][q] != VK_NULL_HANDLE)
          ObjDisp(m_QueueFamilies[i][q])->QueueWaitIdle(Unwrap(m_QueueFamilies[i][q]));
      }
    }

    {
      VkMemoryBarrier memBarrier = {
          VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_ALL_WRITE_BITS, VK_ACCESS_ALL_READ_BITS,
      };

      VkCommandBuffer cmd = GetNextCmd();

      VkResult vkr = VK_SUCCESS;

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      DoPipelineBarrier(cmd, 1, &memBarrier);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);
    }

    m_PreparedNotSerialisedInitStates.clear();
    GetResourceManager()->PrepareInitialContents();

    {
      SCOPED_LOCK(m_CapDescriptorsLock);
      for(const rdcpair<ResourceId, VkResourceRecord *> &it : m_CapDescriptors)
        it.second->Delete(GetResourceManager());
      m_CapDescriptors.clear();
    }

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();
    {
      SCOPED_LOCK(m_ImageStatesLock);
      for(auto it = m_ImageStates.begin(); it != m_ImageStates.end(); ++it)
      {
        it->second.LockWrite()->BeginCapture();
      }
    }

    m_State = CaptureState::ActiveCapturing;
  }

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Instance), eFrameRef_Read);
  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Device), eFrameRef_Read);
  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Queue), eFrameRef_Read);

  rdcarray<VkResourceRecord *> forced = GetForcedReferences();

  // Note we force read-before-write because this resource is implicitly untracked so we have no
  // way of knowing how it's used
  for(auto it = forced.begin(); it != forced.end(); ++it)
  {
    // reference the buffer
    GetResourceManager()->MarkResourceFrameReferenced((*it)->GetResourceID(), eFrameRef_Read);
    // and its backing memory
    GetResourceManager()->MarkMemoryFrameReferenced((*it)->baseResource, (*it)->memOffset,
                                                    (*it)->memSize, eFrameRef_ReadBeforeWrite);
  }
}

bool WrappedVulkan::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  if(m_CaptureFailure)
  {
    m_LastCaptureFailed = Timing::GetUnixTimestamp();
    return DiscardFrameCapture(devWnd);
  }

  m_CaptureFailure = false;

  VkSwapchainKHR swap = VK_NULL_HANDLE;

  if(devWnd.windowHandle)
  {
    {
      SCOPED_LOCK(m_SwapLookupLock);
      auto it = m_SwapLookup.find(devWnd.windowHandle);
      if(it != m_SwapLookup.end())
        swap = it->second;
    }

    if(swap == VK_NULL_HANDLE)
    {
      RDCERR("Output window %p provided for frame capture corresponds with no known swap chain",
             devWnd.windowHandle);
      return false;
    }
  }

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  VkImage backbuffer = VK_NULL_HANDLE;
  const ImageInfo *swapImageInfo = NULL;
  uint32_t swapQueueIndex = 0;
  VkImageLayout swapLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if(swap != VK_NULL_HANDLE)
  {
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(swap), eFrameRef_Read);

    VkResourceRecord *swaprecord = GetRecord(swap);
    RDCASSERT(swaprecord->swapInfo);

    const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

    backbuffer = swapInfo.images[swapInfo.lastPresent.imageIndex].im;
    swapImageInfo = &swapInfo.imageInfo;
    swapQueueIndex = GetRecord(swapInfo.lastPresent.presentQueue)->queueFamilyIndex;
    swapLayout =
        swapInfo.shared ? VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // mark all images referenced as well
    for(size_t i = 0; i < swapInfo.images.size(); i++)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(swapInfo.images[i].im),
                                                        eFrameRef_Read);
  }
  else
  {
    // if a swapchain wasn't specified or found, use the last one presented
    VkResourceRecord *swaprecord = GetResourceManager()->GetResourceRecord(m_LastSwap);
    VkResourceRecord *VRBackbufferRecord =
        GetResourceManager()->GetResourceRecord(m_CurrentVRBackbuffer);

    if(swaprecord)
    {
      GetResourceManager()->MarkResourceFrameReferenced(swaprecord->GetResourceID(), eFrameRef_Read);
      RDCASSERT(swaprecord->swapInfo);

      const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

      backbuffer = swapInfo.images[swapInfo.lastPresent.imageIndex].im;
      swapImageInfo = &swapInfo.imageInfo;
      swapQueueIndex = GetRecord(swapInfo.lastPresent.presentQueue)->queueFamilyIndex;
      swapLayout =
          swapInfo.shared ? VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

      // mark all images referenced as well
      for(size_t i = 0; i < swapInfo.images.size(); i++)
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(swapInfo.images[i].im),
                                                          eFrameRef_Read);
    }
    else if(VRBackbufferRecord)
    {
      RDCASSERT(VRBackbufferRecord->resInfo);
      backbuffer = GetResourceManager()->GetCurrentHandle<VkImage>(m_CurrentVRBackbuffer);
      swapImageInfo = &VRBackbufferRecord->resInfo->imageInfo;
      swapQueueIndex = m_QueueFamilyIdx;
      swapLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      GetResourceManager()->MarkResourceFrameReferenced(m_CurrentVRBackbuffer, eFrameRef_Read);
    }
  }

  rdcarray<VkDeviceMemory> DeadMemories;
  rdcarray<VkBuffer> DeadBuffers;

  // transition back to IDLE atomically
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    EndCaptureFrame(backbuffer);

    m_State = CaptureState::BackgroundCapturing;

    // m_SuccessfulCapture = false;

    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

    {
      SCOPED_LOCK(m_CoherentMapsLock);
      for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
      {
        FreeAlignedBuffer((*it)->memMapState->refData);
        (*it)->memMapState->refData = NULL;
        (*it)->memMapState->needRefData = false;
      }
    }

    DeadMemories.swap(m_DeviceAddressResources.DeadMemories);
    DeadBuffers.swap(m_DeviceAddressResources.DeadBuffers);
  }

  for(VkDeviceMemory m : DeadMemories)
    vkFreeMemory(m_Device, m, NULL);

  for(VkBuffer b : DeadBuffers)
    vkDestroyBuffer(m_Device, b, NULL);

  // gather backbuffer screenshot
  const uint32_t maxSize = 2048;
  RenderDoc::FramePixels fp;

  if(backbuffer != VK_NULL_HANDLE)
  {
    VkDevice device = GetDev();
    VkCommandBuffer cmd = GetNextCmd();

    const VkDevDispatchTable *vt = ObjDisp(device);

    vt->DeviceWaitIdle(Unwrap(device));

    const ImageInfo &imageInfo = *swapImageInfo;

    // since this happens during capture, we don't want to start serialising extra buffer creates,
    // so we manually create & then just wrap.
    VkBuffer readbackBuf = VK_NULL_HANDLE;

    VkResult vkr = VK_SUCCESS;

    // create readback buffer
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        GetByteSize(imageInfo.extent.width, imageInfo.extent.height, 1, imageInfo.format, 0),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };
    vt->CreateBuffer(Unwrap(device), &bufInfo, NULL, &readbackBuf);
    CheckVkResult(vkr);

    GetResourceManager()->WrapResource(Unwrap(device), readbackBuf);

    MemoryAllocation readbackMem =
        AllocateMemoryForResource(readbackBuf, MemoryScope::InitialContents, MemoryType::Readback);

    vkr = vt->BindBufferMemory(Unwrap(device), Unwrap(readbackBuf), Unwrap(readbackMem.mem),
                               readbackMem.offs);
    CheckVkResult(vkr);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    // do image copy
    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    uint32_t rowPitch = GetByteSize(imageInfo.extent.width, 1, 1, imageInfo.format, 0);

    VkBufferImageCopy cpy = {
        0,
        0,
        0,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {
            0, 0, 0,
        },
        {imageInfo.extent.width, imageInfo.extent.height, 1},
    };

    VkImageMemoryBarrier bbBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        swapLayout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapQueueIndex,
        m_QueueFamilyIdx,
        Unwrap(backbuffer),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    DoPipelineBarrier(cmd, 1, &bbBarrier);

    if(swapQueueIndex != m_QueueFamilyIdx)
    {
      VkCommandBuffer extQCmd = GetExtQueueCmd(swapQueueIndex);

      vkr = vt->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      CheckVkResult(vkr);

      DoPipelineBarrier(extQCmd, 1, &bbBarrier);

      ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));

      SubmitAndFlushExtQueue(swapQueueIndex);
    }

    vt->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(backbuffer), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             Unwrap(readbackBuf), 1, &cpy);

    // barrier to switch backbuffer back to present layout
    std::swap(bbBarrier.oldLayout, bbBarrier.newLayout);
    std::swap(bbBarrier.srcAccessMask, bbBarrier.dstAccessMask);
    std::swap(bbBarrier.srcQueueFamilyIndex, bbBarrier.dstQueueFamilyIndex);

    VkBufferMemoryBarrier bufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_HOST_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(readbackBuf),
        0,
        bufInfo.size,
    };

    DoPipelineBarrier(cmd, 1, &bbBarrier);
    DoPipelineBarrier(cmd, 1, &bufBarrier);

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    SubmitCmds();
    FlushQ();    // need to wait so we can readback

    if(swapQueueIndex != m_QueueFamilyIdx)
    {
      VkCommandBuffer extQCmd = GetExtQueueCmd(swapQueueIndex);

      vkr = vt->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      CheckVkResult(vkr);

      DoPipelineBarrier(extQCmd, 1, &bbBarrier);

      ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));

      SubmitAndFlushExtQueue(swapQueueIndex);
    }

    // map memory and readback
    byte *pData = NULL;
    vkr = vt->MapMemory(Unwrap(device), Unwrap(readbackMem.mem), readbackMem.offs, readbackMem.size,
                        0, (void **)&pData);
    CheckVkResult(vkr);
    RDCASSERT(pData != NULL);

    fp.len = (uint32_t)readbackMem.size;
    fp.data = new uint8_t[fp.len];
    memcpy(fp.data, pData, fp.len);

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        NULL,
        Unwrap(readbackMem.mem),
        readbackMem.offs,
        readbackMem.size,
    };

    vkr = vt->InvalidateMappedMemoryRanges(Unwrap(device), 1, &range);
    CheckVkResult(vkr);

    vt->UnmapMemory(Unwrap(device), Unwrap(readbackMem.mem));

    // delete all
    vt->DestroyBuffer(Unwrap(device), Unwrap(readbackBuf), NULL);
    GetResourceManager()->ReleaseWrappedResource(readbackBuf);

    ResourceFormat fmt = MakeResourceFormat(imageInfo.format);
    fp.width = imageInfo.extent.width;
    fp.height = imageInfo.extent.height;
    fp.pitch = rowPitch;
    fp.stride = fmt.compByteWidth * fmt.compCount;
    fp.bpc = fmt.compByteWidth;
    fp.bgra = fmt.BGRAOrder();
    fp.max_width = maxSize;
    fp.pitch_requirement = 8;
    switch(fmt.type)
    {
      case ResourceFormatType::R10G10B10A2:
        fp.stride = 4;
        fp.buf1010102 = true;
        break;
      case ResourceFormatType::R5G6B5:
        fp.stride = 2;
        fp.buf565 = true;
        break;
      case ResourceFormatType::R5G5B5A1:
        fp.stride = 2;
        fp.buf5551 = true;
        break;
      default: break;
    }
  }

  RDCFile *rdc =
      RenderDoc::Inst().CreateRDC(RDCDriver::Vulkan, m_CapturedFrames.back().frameNumber, fp);

  StreamWriter *captureWriter = NULL;

  if(rdc)
  {
    SectionProperties props;

    // Compress with LZ4 so that it's fast
    props.flags = SectionFlags::LZ4Compressed;
    props.version = m_SectionVersion;
    props.type = SectionType::FrameCapture;

    captureWriter = rdc->WriteSection(props);
  }
  else
  {
    captureWriter = new StreamWriter(StreamWriter::InvalidStream);
  }

  uint64_t captureSectionSize = 0;

  {
    WriteSerialiser ser(captureWriter, Ownership::Stream);

    ser.SetChunkMetadataRecording(GetThreadSerialiser().GetChunkMetadataRecording());

    ser.SetUserData(GetResourceManager());

    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, m_InitParams.GetSerialiseSize());

      SERIALISE_ELEMENT(m_InitParams);
    }

    RDCDEBUG("Inserting Resource Serialisers");

    GetResourceManager()->InsertReferencedChunks(ser);

    GetResourceManager()->InsertInitialContentsChunks(ser);

    RDCDEBUG("Creating Capture Scope");

    GetResourceManager()->Serialise_InitialContentsNeeded(ser);
    GetResourceManager()->InsertDeviceMemoryRefs(ser);

    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);

      Serialise_CaptureScope(ser);
    }

    {
      WriteSerialiser &captureBeginSer = GetThreadSerialiser();
      ScopedChunk scope(captureBeginSer, SystemChunk::CaptureBegin);

      Serialise_BeginCaptureFrame(captureBeginSer);

      m_HeaderChunk = scope.Get();
    }
    m_HeaderChunk->Write(ser);

    // don't need to lock access to m_CmdBufferRecords as we are no longer
    // in capframe (the transition is thread-protected) so nothing will be
    // pushed to the vector

    {
      RDCDEBUG("Flushing %u command buffer records to file serialiser",
               (uint32_t)m_CmdBufferRecords.size());

      std::map<int64_t, Chunk *> recordlist;

      // ensure all command buffer records within the frame evne if recorded before, but
      // otherwise order must be preserved (vs. queue submits and desc set updates)
      for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
      {
        if(Vulkan_Debug_VerboseCommandRecording())
        {
          RDCLOG("Adding chunks from command buffer %s",
                 ToStr(m_CmdBufferRecords[i]->GetResourceID()).c_str());
        }
        else
        {
          RDCDEBUG("Adding chunks from command buffer %s",
                   ToStr(m_CmdBufferRecords[i]->GetResourceID()).c_str());
        }

        size_t prevSize = recordlist.size();
        (void)prevSize;

        m_CmdBufferRecords[i]->Insert(recordlist);

        RDCDEBUG("Added %zu chunks to file serialiser", recordlist.size() - prevSize);
      }

      m_FrameCaptureRecord->Insert(recordlist);

      RDCDEBUG("Flushing %u chunks to file serialiser from context record",
               (uint32_t)recordlist.size());

      float num = float(recordlist.size());
      float idx = 0.0f;

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      {
        RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
        idx += 1.0f;
        it->second->Write(ser);
      }

      m_FrameCaptureRecord->DeleteChunks();

      RDCDEBUG("Done");
    }

    captureSectionSize = captureWriter->GetOffset();
  }

  RDCLOG("Captured Vulkan frame with %f MB capture section in %f seconds",
         double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  m_State = CaptureState::BackgroundCapturing;

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  Atomic::Inc32(&m_ReuseEnabled);

  GetResourceManager()->ResetLastWriteTimes();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedMemory();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  FreeAllMemory(MemoryScope::InitialContents);
  for(rdcstr &fn : m_InitTempFiles)
    FileIO::Delete(fn);
  m_InitTempFiles.clear();

  return true;
}

bool WrappedVulkan::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  m_CaptureFailure = false;

  RDCLOG("Discarding frame capture.");

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  // transition back to IDLE atomically
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    m_State = CaptureState::BackgroundCapturing;

    // m_SuccessfulCapture = false;

    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

    {
      SCOPED_LOCK(m_CoherentMapsLock);
      for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
      {
        FreeAlignedBuffer((*it)->memMapState->refData);
        (*it)->memMapState->refData = NULL;
        (*it)->memMapState->needRefData = false;
      }
    }
  }

  Atomic::Inc32(&m_ReuseEnabled);

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  FreeAllMemory(MemoryScope::InitialContents);
  for(rdcstr &fn : m_InitTempFiles)
    FileIO::Delete(fn);
  m_InitTempFiles.clear();

  return true;
}

void WrappedVulkan::AdvanceFrame()
{
  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame
}

void WrappedVulkan::Present(DeviceOwnedWindow devWnd)
{
  bool activeWindow = devWnd.windowHandle == NULL || RenderDoc::Inst().IsActiveWindow(devWnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Vulkan, true);

  if(!activeWindow)
  {
    // first present to *any* window, even inactive, terminates frame 0
    if(m_FirstFrameCapture && IsActiveCapturing(m_State))
    {
      RenderDoc::Inst().EndFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
      m_FirstFrameCapture = false;
    }

    return;
  }

  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(devWnd);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

void WrappedVulkan::HandleFrameMarkers(const char *marker, VkCommandBuffer commandBuffer)
{
  if(!marker)
    return;

  if(strstr(marker, "vr-marker,frame_end,type,application") != NULL)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);
    record->bakedCommands->cmdInfo->present = true;
  }
  if(strstr(marker, "capture-marker,begin_capture") != NULL)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);
    record->bakedCommands->cmdInfo->beginCapture = true;
  }
  if(strstr(marker, "capture-marker,end_capture") != NULL)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);
    record->bakedCommands->cmdInfo->endCapture = true;
  }
}

void WrappedVulkan::HandleFrameMarkers(const char *marker, VkQueue queue)
{
  if(!marker)
    return;

  if(strstr(marker, "capture-marker,begin_capture") != NULL)
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }
  if(strstr(marker, "capture-marker,end_capture") != NULL)
  {
    RenderDoc::Inst().EndFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }
}

ResourceDescription &WrappedVulkan::GetResourceDesc(ResourceId id)
{
  return GetReplay()->GetResourceDesc(id);
}

void WrappedVulkan::AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix)
{
  ResourceDescription &descr = GetReplay()->GetResourceDesc(id);

  uint64_t num;
  memcpy(&num, &id, sizeof(uint64_t));
  descr.name = defaultNamePrefix + (" " + ToStr(num));
  descr.autogeneratedName = true;
  descr.type = type;
  AddResourceCurChunk(descr);
}

void WrappedVulkan::DerivedResource(ResourceId parentLive, ResourceId child)
{
  ResourceId parentId = GetResourceManager()->GetOriginalID(parentLive);

  if(GetReplay()->GetResourceDesc(parentId).derivedResources.contains(child))
    return;

  GetReplay()->GetResourceDesc(parentId).derivedResources.push_back(child);
  GetReplay()->GetResourceDesc(child).parentResources.push_back(parentId);
}

void WrappedVulkan::AddResourceCurChunk(ResourceDescription &descr)
{
  descr.initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 1);
}

void WrappedVulkan::AddResourceCurChunk(ResourceId id)
{
  AddResourceCurChunk(GetReplay()->GetResourceDesc(id));
}

RDResult WrappedVulkan::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  GetResourceManager()->SetState(m_State);

  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(IsStructuredExporting(m_State))
  {
    // when structured exporting don't do any timebase conversion
    m_TimeBase = 0;
    m_TimeFrequency = 1.0;
  }
  else
  {
    m_TimeBase = rdc->GetTimestampBase();
    m_TimeFrequency = rdc->GetTimestampFrequency();
  }

  if(reader->IsErrored())
  {
    RDResult result = reader->GetError();
    delete reader;
    return result;
  }

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers, m_TimeBase, m_TimeFrequency);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData->version = m_StructuredFile->version = m_SectionVersion;

  ser.SetVersion(m_SectionVersion);

  int chunkIdx = 0;

  struct chunkinfo
  {
    chunkinfo() : count(0), totalsize(0), total(0.0) {}
    int count;
    uint64_t totalsize;
    double total;
  };

  std::map<VulkanChunk, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  uint64_t frameDataSize = 0;

  ScopedDebugMessageSink *sink = NULL;
  if(m_ReplayOptions.apiValidation)
    sink = new ScopedDebugMessageSink(this);

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offsetStart = reader->GetOffset();

    VulkanChunk context = ser.ReadChunk<VulkanChunk>();

    chunkIdx++;

    if(reader->IsErrored())
    {
      SAFE_DELETE(sink);
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);
    }

    size_t firstMessage = 0;
    if(sink)
      firstMessage = sink->msgs.size();

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(reader->IsErrored())
    {
      SAFE_DELETE(sink);
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);
    }

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
    {
      rdcstr extra;

      if(sink)
      {
        extra += "\n";

        for(size_t i = firstMessage; i < sink->msgs.size(); i++)
        {
          extra += "\n";
          extra += sink->msgs[i].description;
        }
      }
      else
      {
        extra +=
            "\n\nMore debugging information may be available by enabling API validation on replay";
      }

      SAFE_DELETE(sink);
      m_FailedReplayResult.message = rdcstr(m_FailedReplayResult.message) + extra;
      return m_FailedReplayResult;
    }

    if(m_FatalError != ResultCode::Succeeded)
      return m_FatalError;

    uint64_t offsetEnd = reader->GetOffset();

    // only set progress after we've initialised the debug manager, to prevent progress jumping
    // backwards.
    if(m_DebugManager || IsStructuredExporting(m_State))
    {
      RenderDoc::Inst().SetProgress(LoadProgress::FileInitialRead,
                                    float(offsetEnd) / float(reader->GetSize()));
    }

    if((SystemChunk)context == SystemChunk::CaptureScope)
    {
      GetReplay()->WriteFrameRecord().frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      if(m_Queue == VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE && m_QueueFamilyIdx != ~0U)
      {
        if(m_ExternalQueues[m_QueueFamilyIdx].queue != VK_NULL_HANDLE)
        {
          m_Queue = m_ExternalQueues[m_QueueFamilyIdx].queue;
        }
        else
        {
          ObjDisp(m_Device)->GetDeviceQueue(Unwrap(m_Device), m_QueueFamilyIdx, 0, &m_Queue);

          GetResourceManager()->WrapResource(Unwrap(m_Device), m_Queue);
          GetResourceManager()->AddLiveResource(ResourceIDGen::GetNewUniqueID(), m_Queue);

          m_ExternalQueues[m_QueueFamilyIdx].queue = m_Queue;
        }
      }

      m_FrameReader = new StreamReader(reader, frameDataSize);

      for(auto it = m_CreationInfo.m_Memory.begin(); it != m_CreationInfo.m_Memory.end(); ++it)
        it->second.SimplifyBindings();

      RDResult status = ContextReplayLog(m_State, 0, 0, false);

      if(status != ResultCode::Succeeded)
      {
        SAFE_DELETE(sink);
        return status;
      }
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if((SystemChunk)context == SystemChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
      break;
  }

  SAFE_DELETE(sink);

#if ENABLED(RDOC_DEVEL)
  for(auto it = chunkInfos.begin(); it != chunkInfos.end(); ++it)
  {
    double dcount = double(it->second.count);

    RDCDEBUG(
        "% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
        it->second.count, it->second.total, it->second.total / dcount,
        double(it->second.totalsize) / (1024.0 * 1024.0),
        double(it->second.totalsize) / (dcount * 1024.0 * 1024.0),
        GetChunkName((uint32_t)it->first).c_str(), uint32_t(it->first));
  }
#endif

  // steal the structured data for ourselves
  m_StructuredFile->Swap(*m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = m_StoredStructuredData;

  GetReplay()->WriteFrameRecord().frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.compressedFileSize =
      rdc->GetSectionProperties(sectionIdx).compressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.persistentSize = frameDataSize;
  GetReplay()->WriteFrameRecord().frameInfo.initDataSize =
      chunkInfos[(VulkanChunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           GetReplay()->WriteFrameRecord().frameInfo.persistentSize);

  // ensure the capture at least created a device and fetched a queue.
  if(!IsStructuredExporting(m_State))
  {
    RDCASSERT(m_Device != VK_NULL_HANDLE && m_Queue != VK_NULL_HANDLE &&
              m_InternalCmds.cmdpool != VK_NULL_HANDLE);

    // create indirect action buffer
    m_IndirectBufferSize = AlignUp(m_IndirectBufferSize + 63, (size_t)64);

    m_IndirectBuffer.Create(this, GetDev(), m_IndirectBufferSize * 2, 1,
                            GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferIndirectBuffer);

    m_IndirectCommandBuffer = GetNextCmd();

    // steal the command buffer out of the pending commands - we'll manage its lifetime ourselves
    m_InternalCmds.pendingcmds.pop_back();
  }

  FreeAllMemory(MemoryScope::IndirectReadback);

  return ResultCode::Succeeded;
}

RDResult WrappedVulkan::ContextReplayLog(CaptureState readType, uint32_t startEventID,
                                         uint32_t endEventID, bool partial)
{
  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());
  ser.SetVersion(m_SectionVersion);

  SDFile *prevFile = m_StructuredFile;

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State), m_TimeBase,
                                  m_TimeFrequency);

    ser.GetStructuredFile().Swap(*m_StructuredFile);

    m_StructuredFile = &ser.GetStructuredFile();
  }

  SystemChunk header = ser.ReadChunk<SystemChunk>();
  RDCASSERTEQUAL(header, SystemChunk::CaptureBegin);

  if(partial)
  {
    ser.SkipCurrentChunk();
  }
  else
  {
#if ENABLED(RDOC_RELEASE)
    if(IsLoading(m_State))
      Serialise_BeginCaptureFrame(ser);
    else
      ser.SkipCurrentChunk();
#else
    Serialise_BeginCaptureFrame(ser);

    if(IsLoading(m_State))
    {
      AddResourceCurChunk(m_InitParams.InstanceID);
    }
#endif
  }

  ser.EndChunk();

  if(!IsStructuredExporting(m_State))
    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

  // apply initial contents here so that images are in the right layout
  // (not undefined)
  if(IsLoading(m_State))
  {
    // temporarily disable the debug message sink, to ignore messages from initial contents apply
    ScopedDebugMessageSink *sink = GetDebugMessageSink();
    SetDebugMessageSink(NULL);

    ApplyInitialContents();

    SetDebugMessageSink(sink);
  }

  m_RootEvents.clear();

  if(IsActiveReplaying(m_State))
  {
    APIEvent ev = GetEvent(startEventID);
    m_RootEventID = ev.eventId;

    // if not partial, we need to be sure to replay
    // past the command buffer records, so can't
    // skip to the file offset of the first event
    if(partial)
      ser.GetReader()->SetOffset(ev.fileOffset);

    m_FirstEventID = startEventID;
    m_LastEventID = endEventID;

    // when selecting a marker we can get into an inconsistent state -
    // make sure that we make things consistent again here, replay the event
    // that we ended up selecting (the one that was closest)
    if(startEventID == endEventID && m_RootEventID != m_FirstEventID)
      m_FirstEventID = m_LastEventID = m_RootEventID;
  }
  else
  {
    m_RootEventID = 1;
    m_RootActionID = 1;
    m_FirstEventID = 0;
    m_LastEventID = ~0U;
  }

  if(!partial && !IsStructuredExporting(m_State))
    AddFrameTerminator(AMDRGPControl::GetBeginTag());

  uint64_t startOffset = ser.GetReader()->GetOffset();

  for(;;)
  {
    if(IsActiveReplaying(m_State) && m_RootEventID > endEventID)
    {
      // we can just break out if we've done all the events desired.
      // note that the command buffer events aren't 'real' and we just blaze through them
      break;
    }

    m_CurChunkOffset = ser.GetReader()->GetOffset();

    VulkanChunk chunktype = ser.ReadChunk<VulkanChunk>();

    if(ser.GetReader()->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    m_ChunkMetadata = ser.ChunkMetadata();

    m_LastCmdBufferID = ResourceId();

    bool success = ContextProcessChunk(ser, chunktype);

    ser.EndChunk();

    if(ser.GetReader()->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
    {
      rdcstr extra;

      ScopedDebugMessageSink *sink = GetDebugMessageSink();

      if(sink)
      {
        extra += "\n";

        for(size_t i = 0; i < sink->msgs.size(); i++)
        {
          extra += "\n";
          extra += sink->msgs[i].description;
        }
      }
      else
      {
        extra +=
            "\n\nMore debugging information may be available by enabling API validation on replay";
      }

      m_FailedReplayResult.message = rdcstr(m_FailedReplayResult.message) + extra;
      return m_FailedReplayResult;
    }

    if(m_FatalError != ResultCode::Succeeded)
      return m_FatalError;

    RenderDoc::Inst().SetProgress(
        LoadProgress::FrameEventsRead,
        float(m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if((SystemChunk)chunktype == SystemChunk::CaptureEnd || ser.GetReader()->AtEnd())
      break;

    // break out if we were only executing one event
    if(IsActiveReplaying(m_State) && startEventID == endEventID)
      break;

    m_LastChunk = chunktype;

    // increment root event ID either if we didn't just replay a cmd
    // buffer event, OR if we are doing a frame sub-section replay,
    // in which case it's up to the calling code to make sure we only
    // replay inside a command buffer (if we crossed command buffer
    // boundaries, the event IDs would no longer match up).
    if(m_LastCmdBufferID == ResourceId() || startEventID > 1)
    {
      m_RootEventID++;

      if(startEventID > 1)
        ser.GetReader()->SetOffset(GetEvent(m_RootEventID).fileOffset);
    }
    else
    {
      // these events are completely omitted, so don't increment the curEventID
      if(chunktype != VulkanChunk::vkBeginCommandBuffer &&
         chunktype != VulkanChunk::vkEndCommandBuffer)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
    }
  }

  if(!partial && !IsStructuredExporting(m_State))
    AddFrameTerminator(AMDRGPControl::GetEndTag());

  // Save the current render state in the partial command buffer.
  m_RenderState = m_BakedCmdBufferInfo[GetPartialCommandBuffer()].state;

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().Swap(*prevFile);

  m_StructuredFile = prevFile;

  if(IsLoading(m_State))
  {
    GetReplay()->WriteFrameRecord().actionList = m_ParentAction.Bake();

    SetupActionPointers(m_Actions, GetReplay()->WriteFrameRecord().actionList);

    m_ParentAction.children.clear();
  }

  // submit the indirect preparation command buffer, if we need to
  if(m_IndirectDraw)
  {
    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        m_SubmitChain,
        0,
        NULL,
        NULL,    // wait semaphores
        1,
        UnwrapPtr(m_IndirectCommandBuffer),    // command buffers
        0,
        NULL,    // signal semaphores
    };

    VkResult vkr = ObjDisp(m_Queue)->QueueSubmit(Unwrap(m_Queue), 1, &submitInfo, VK_NULL_HANDLE);
    CheckVkResult(vkr);
  }

  m_IndirectDraw = false;

  m_RerecordCmds.clear();

  return ResultCode::Succeeded;
}

void WrappedVulkan::ApplyInitialContents()
{
  RENDERDOC_PROFILEFUNCTION();
  if(HasFatalError())
    return;

  VkMarkerRegion region("ApplyInitialContents");

  initStateCurBatch = 0;
  initStateCurCmd = VK_NULL_HANDLE;

  // check that we have all external queues necessary
  for(size_t i = 0; i < m_ExternalQueues.size(); i++)
  {
    // if we created a pool (so this is a queue family we're using) but
    // didn't get a queue at all, fetch our own queue for this family
    if(m_ExternalQueues[i].queue != VK_NULL_HANDLE || m_ExternalQueues[i].pool == VK_NULL_HANDLE)
      continue;

    VkQueue queue;

    ObjDisp(m_Device)->GetDeviceQueue(Unwrap(m_Device), (uint32_t)i, 0, &queue);

    GetResourceManager()->WrapResource(Unwrap(m_Device), queue);
    GetResourceManager()->AddLiveResource(ResourceIDGen::GetNewUniqueID(), queue);

    m_ExternalQueues[i].queue = queue;
  }

  // add a global memory barrier to ensure all writes have finished and are synchronised
  // add memory barrier to ensure this copy completes before any subsequent work
  // this is a very blunt instrument but it ensures we don't get random artifacts around
  // frame restart where we may be skipping a lot of important synchronisation
  VkMemoryBarrier memBarrier = {
      VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_ALL_WRITE_BITS, VK_ACCESS_ALL_READ_BITS,
  };

  VkCommandBuffer cmd = GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return;

  VkResult vkr = VK_SUCCESS;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  DoPipelineBarrier(cmd, 1, &memBarrier);

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  CheckVkResult(vkr);

  // sync all GPU work so we can also apply descriptor set initial contents
  SubmitCmds();
  FlushQ();

  // actually apply the initial contents here
  GetResourceManager()->ApplyInitialContents();

  // close the final command buffer
  if(initStateCurCmd != VK_NULL_HANDLE)
  {
    CloseInitStateCmd();
  }

  initStateCurBatch = 0;
  initStateCurCmd = VK_NULL_HANDLE;

  for(auto it = m_ImageStates.begin(); it != m_ImageStates.end(); ++it)
  {
    if(GetResourceManager()->HasCurrentResource(it->first))
    {
      it->second.LockWrite()->ResetToOldState(m_cleanupImageBarriers, GetImageTransitionInfo());
    }
    else
    {
      it = m_ImageStates.erase(it);
      --it;
    }
  }

  // likewise again to make sure the initial states are all applied
  cmd = GetNextCmd();

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  DoPipelineBarrier(cmd, 1, &memBarrier);

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  CheckVkResult(vkr);

  SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
  SubmitCmds();
  FlushQ();
  SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);

  // reset any queries to a valid copy-able state if they need to be copied.
  if(!m_ResetQueries.empty())
  {
    // sort all pools together
    std::sort(m_ResetQueries.begin(), m_ResetQueries.end(),
              [](const ResetQuery &a, const ResetQuery &b) { return a.pool < b.pool; });

    cmd = GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    uint32_t i = 0;
    for(const ResetQuery &r : m_ResetQueries)
    {
      ObjDisp(cmd)->CmdResetQueryPool(Unwrap(cmd), Unwrap(r.pool), r.firstQuery, r.queryCount);

      for(uint32_t q = 0; q < r.queryCount; q++)
      {
        // Timestamps are easy - we can do these without needing to render
        if(m_CreationInfo.m_QueryPool[GetResID(r.pool)].queryType == VK_QUERY_TYPE_TIMESTAMP)
        {
          ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          Unwrap(r.pool), r.firstQuery + q);
        }
        else
        {
          ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), Unwrap(r.pool), r.firstQuery + q, 0);
          ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), Unwrap(r.pool), r.firstQuery + q);
        }

        i++;

        // split the command buffer and flush if the number of queries is massive
        if(i > 0 && (i % (128 * 1024)) == 0)
        {
          vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);

          SubmitCmds();
          FlushQ();

          cmd = GetNextCmd();

          if(cmd == VK_NULL_HANDLE)
            return;

          vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);
        }
      }
    }

    vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    m_ResetQueries.clear();

    SubmitCmds();
    FlushQ();
  }
}

bool WrappedVulkan::ContextProcessChunk(ReadSerialiser &ser, VulkanChunk chunk)
{
  m_AddedAction = false;

  bool success = ProcessChunk(ser, chunk);

  if(!success)
    return false;

  if(IsLoading(m_State))
  {
    if(chunk == VulkanChunk::vkBeginCommandBuffer || chunk == VulkanChunk::vkEndCommandBuffer)
    {
      // don't add these events - they will be handled when inserted in-line into queue submit
    }
    else if(chunk == VulkanChunk::vkQueueEndDebugUtilsLabelEXT)
    {
      // also ignore, this just pops the action stack
    }
    else
    {
      if(!m_AddedAction)
        AddEvent();
    }
  }

  m_AddedAction = false;

  return true;
}

bool WrappedVulkan::ProcessChunk(ReadSerialiser &ser, VulkanChunk chunk)
{
  switch(chunk)
  {
    case VulkanChunk::vkEnumeratePhysicalDevices:
      return Serialise_vkEnumeratePhysicalDevices(ser, NULL, NULL, NULL);
    case VulkanChunk::vkCreateDevice:
      return Serialise_vkCreateDevice(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkGetDeviceQueue:
      return Serialise_vkGetDeviceQueue(ser, VK_NULL_HANDLE, 0, 0, NULL);

    case VulkanChunk::vkAllocateMemory:
      return Serialise_vkAllocateMemory(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkUnmapMemory:
      return Serialise_vkUnmapMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
    case VulkanChunk::vkFlushMappedMemoryRanges:
    case VulkanChunk::CoherentMapWrite:
      return Serialise_vkFlushMappedMemoryRanges(ser, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkCreateCommandPool:
      return Serialise_vkCreateCommandPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkAllocateCommandBuffers:
      return Serialise_vkAllocateCommandBuffers(ser, VK_NULL_HANDLE, NULL, NULL);
    case VulkanChunk::vkCreateFramebuffer:
      return Serialise_vkCreateFramebuffer(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateRenderPass:
      return Serialise_vkCreateRenderPass(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateDescriptorPool:
      return Serialise_vkCreateDescriptorPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateDescriptorSetLayout:
      return Serialise_vkCreateDescriptorSetLayout(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateBuffer:
      return Serialise_vkCreateBuffer(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateBufferView:
      return Serialise_vkCreateBufferView(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateImage:
      return Serialise_vkCreateImage(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateImageView:
      return Serialise_vkCreateImageView(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateSampler:
      return Serialise_vkCreateSampler(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateShaderModule:
      return Serialise_vkCreateShaderModule(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreatePipelineLayout:
      return Serialise_vkCreatePipelineLayout(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreatePipelineCache:
      return Serialise_vkCreatePipelineCache(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateGraphicsPipelines:
      return Serialise_vkCreateGraphicsPipelines(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL,
                                                 NULL);
    case VulkanChunk::vkCreateComputePipelines:
      return Serialise_vkCreateComputePipelines(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL,
                                                NULL);
    case VulkanChunk::vkGetSwapchainImagesKHR:
      return Serialise_vkGetSwapchainImagesKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, NULL);

    case VulkanChunk::vkCreateSemaphore:
      return Serialise_vkCreateSemaphore(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCreateFence:
    // these chunks re-use serialisation from vkCreateFence, but have separate chunks for user
    // identification
    case VulkanChunk::vkRegisterDeviceEventEXT:
    case VulkanChunk::vkRegisterDisplayEventEXT:
      return Serialise_vkCreateFence(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkGetFenceStatus:
      return Serialise_vkGetFenceStatus(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
    case VulkanChunk::vkResetFences: return Serialise_vkResetFences(ser, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkWaitForFences:
      return Serialise_vkWaitForFences(ser, VK_NULL_HANDLE, 0, NULL, VK_FALSE, 0);

    case VulkanChunk::vkCreateEvent:
      return Serialise_vkCreateEvent(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkGetEventStatus:
      return Serialise_vkGetEventStatus(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
    case VulkanChunk::vkSetEvent: return Serialise_vkSetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
    case VulkanChunk::vkResetEvent:
      return Serialise_vkResetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);

    case VulkanChunk::vkCreateQueryPool:
      return Serialise_vkCreateQueryPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);

    case VulkanChunk::vkAllocateDescriptorSets:
      return Serialise_vkAllocateDescriptorSets(ser, VK_NULL_HANDLE, NULL, NULL);
    case VulkanChunk::vkUpdateDescriptorSets:
      return Serialise_vkUpdateDescriptorSets(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);

    case VulkanChunk::vkBeginCommandBuffer:
      return Serialise_vkBeginCommandBuffer(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkEndCommandBuffer: return Serialise_vkEndCommandBuffer(ser, VK_NULL_HANDLE);

    case VulkanChunk::vkQueueWaitIdle: return Serialise_vkQueueWaitIdle(ser, VK_NULL_HANDLE);
    case VulkanChunk::vkDeviceWaitIdle: return Serialise_vkDeviceWaitIdle(ser, VK_NULL_HANDLE);

    case VulkanChunk::vkQueueSubmit:
      return Serialise_vkQueueSubmit(ser, VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
    case VulkanChunk::vkBindBufferMemory:
      return Serialise_vkBindBufferMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
    case VulkanChunk::vkBindImageMemory:
      return Serialise_vkBindImageMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);

    case VulkanChunk::vkQueueBindSparse:
      return Serialise_vkQueueBindSparse(ser, VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);

    case VulkanChunk::vkCmdBeginRenderPass:
      return Serialise_vkCmdBeginRenderPass(ser, VK_NULL_HANDLE, NULL, VK_SUBPASS_CONTENTS_MAX_ENUM);
    case VulkanChunk::vkCmdNextSubpass:
      return Serialise_vkCmdNextSubpass(ser, VK_NULL_HANDLE, VK_SUBPASS_CONTENTS_MAX_ENUM);
    case VulkanChunk::vkCmdExecuteCommands:
      return Serialise_vkCmdExecuteCommands(ser, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkCmdEndRenderPass: return Serialise_vkCmdEndRenderPass(ser, VK_NULL_HANDLE);

    case VulkanChunk::vkCmdBindPipeline:
      return Serialise_vkCmdBindPipeline(ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM,
                                         VK_NULL_HANDLE);
    case VulkanChunk::vkCmdSetViewport:
      return Serialise_vkCmdSetViewport(ser, VK_NULL_HANDLE, 0, 0, NULL);
    case VulkanChunk::vkCmdSetScissor:
      return Serialise_vkCmdSetScissor(ser, VK_NULL_HANDLE, 0, 0, NULL);
    case VulkanChunk::vkCmdSetLineWidth: return Serialise_vkCmdSetLineWidth(ser, VK_NULL_HANDLE, 0);
    case VulkanChunk::vkCmdSetDepthBias:
      return Serialise_vkCmdSetDepthBias(ser, VK_NULL_HANDLE, 0.0f, 0.0f, 0.0f);
    case VulkanChunk::vkCmdSetBlendConstants:
      return Serialise_vkCmdSetBlendConstants(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdSetDepthBounds:
      return Serialise_vkCmdSetDepthBounds(ser, VK_NULL_HANDLE, 0.0f, 0.0f);
    case VulkanChunk::vkCmdSetStencilCompareMask:
      return Serialise_vkCmdSetStencilCompareMask(ser, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdSetStencilWriteMask:
      return Serialise_vkCmdSetStencilWriteMask(ser, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdSetStencilReference:
      return Serialise_vkCmdSetStencilReference(ser, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdBindDescriptorSets:
      return Serialise_vkCmdBindDescriptorSets(ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM,
                                               VK_NULL_HANDLE, 0, 0, NULL, 0, NULL);
    case VulkanChunk::vkCmdBindIndexBuffer:
      return Serialise_vkCmdBindIndexBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0,
                                            VK_INDEX_TYPE_MAX_ENUM);
    case VulkanChunk::vkCmdBindVertexBuffers:
      return Serialise_vkCmdBindVertexBuffers(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL);
    case VulkanChunk::vkCmdCopyBufferToImage:
      return Serialise_vkCmdCopyBufferToImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
    case VulkanChunk::vkCmdCopyImageToBuffer:
      return Serialise_vkCmdCopyImageToBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkCmdCopyImage:
      return Serialise_vkCmdCopyImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                                      VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
    case VulkanChunk::vkCmdBlitImage:
      return Serialise_vkCmdBlitImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                                      VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL,
                                      VK_FILTER_MAX_ENUM);
    case VulkanChunk::vkCmdResolveImage:
      return Serialise_vkCmdResolveImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                         VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE,
                                         VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
    case VulkanChunk::vkCmdCopyBuffer:
      return Serialise_vkCmdCopyBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkCmdUpdateBuffer:
      return Serialise_vkCmdUpdateBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, NULL);
    case VulkanChunk::vkCmdFillBuffer:
      return Serialise_vkCmdFillBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdPushConstants:
      return Serialise_vkCmdPushConstants(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SHADER_STAGE_ALL,
                                          0, 0, NULL);
    case VulkanChunk::vkCmdClearColorImage:
      return Serialise_vkCmdClearColorImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
    case VulkanChunk::vkCmdClearDepthStencilImage:
      return Serialise_vkCmdClearDepthStencilImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                   VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
    case VulkanChunk::vkCmdClearAttachments:
      return Serialise_vkCmdClearAttachments(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);
    case VulkanChunk::vkCmdPipelineBarrier:
      return Serialise_vkCmdPipelineBarrier(ser, VK_NULL_HANDLE, 0, 0, VK_FALSE, 0, NULL, 0, NULL,
                                            0, NULL);
    case VulkanChunk::vkCmdWriteTimestamp:
      return Serialise_vkCmdWriteTimestamp(ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                           VK_NULL_HANDLE, 0);
    case VulkanChunk::vkCmdCopyQueryPoolResults:
      return Serialise_vkCmdCopyQueryPoolResults(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0,
                                                 VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdBeginQuery:
      return Serialise_vkCmdBeginQuery(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdEndQuery:
      return Serialise_vkCmdEndQuery(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
    case VulkanChunk::vkCmdResetQueryPool:
      return Serialise_vkCmdResetQueryPool(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);

    case VulkanChunk::vkCmdSetEvent:
      return Serialise_vkCmdSetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    case VulkanChunk::vkCmdResetEvent:
      return Serialise_vkCmdResetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    case VulkanChunk::vkCmdWaitEvents:
      return Serialise_vkCmdWaitEvents(
          ser, VK_NULL_HANDLE, 0, NULL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, NULL, 0, NULL, 0, NULL);

    case VulkanChunk::vkCmdDraw: return Serialise_vkCmdDraw(ser, VK_NULL_HANDLE, 0, 0, 0, 0);
    case VulkanChunk::vkCmdDrawIndirect:
      return Serialise_vkCmdDrawIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdDrawIndexed:
      return Serialise_vkCmdDrawIndexed(ser, VK_NULL_HANDLE, 0, 0, 0, 0, 0);
    case VulkanChunk::vkCmdDrawIndexedIndirect:
      return Serialise_vkCmdDrawIndexedIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdDispatch: return Serialise_vkCmdDispatch(ser, VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdDispatchIndirect:
      return Serialise_vkCmdDispatchIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);

    case VulkanChunk::vkCmdDebugMarkerBeginEXT:
      return Serialise_vkCmdDebugMarkerBeginEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdDebugMarkerInsertEXT:
      return Serialise_vkCmdDebugMarkerInsertEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdDebugMarkerEndEXT:
      return Serialise_vkCmdDebugMarkerEndEXT(ser, VK_NULL_HANDLE);
    case VulkanChunk::vkDebugMarkerSetObjectNameEXT:
      return Serialise_vkDebugMarkerSetObjectNameEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::SetShaderDebugPath:
      return Serialise_SetShaderDebugPath(ser, VK_NULL_HANDLE, rdcstr());

    case VulkanChunk::vkCreateSwapchainKHR:
      return Serialise_vkCreateSwapchainKHR(ser, VK_NULL_HANDLE, NULL, NULL, NULL);

    case VulkanChunk::vkCmdIndirectSubCommand:
      // this is a fake chunk generated at runtime as part of indirect draws.
      // Just in case it gets exported and imported, completely ignore it.
      return true;

    case VulkanChunk::vkCmdPushDescriptorSetKHR:
      return Serialise_vkCmdPushDescriptorSetKHR(
          ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_GRAPHICS, VK_NULL_HANDLE, 0, 0, NULL);

    case VulkanChunk::vkCmdPushDescriptorSetWithTemplateKHR:
      return Serialise_vkCmdPushDescriptorSetWithTemplateKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                             VK_NULL_HANDLE, 0, NULL);

    case VulkanChunk::vkCreateDescriptorUpdateTemplate:
      return Serialise_vkCreateDescriptorUpdateTemplate(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkUpdateDescriptorSetWithTemplate:
      return Serialise_vkUpdateDescriptorSetWithTemplate(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                         VK_NULL_HANDLE, NULL);

    case VulkanChunk::vkBindBufferMemory2:
      return Serialise_vkBindBufferMemory2(ser, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkBindImageMemory2:
      return Serialise_vkBindImageMemory2(ser, VK_NULL_HANDLE, 0, NULL);

    case VulkanChunk::vkCmdWriteBufferMarkerAMD:
      return Serialise_vkCmdWriteBufferMarkerAMD(
          ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_NULL_HANDLE, 0, 0);

    case VulkanChunk::vkSetDebugUtilsObjectNameEXT:
      return Serialise_vkSetDebugUtilsObjectNameEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkQueueBeginDebugUtilsLabelEXT:
      return Serialise_vkQueueBeginDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkQueueEndDebugUtilsLabelEXT:
      return Serialise_vkQueueEndDebugUtilsLabelEXT(ser, VK_NULL_HANDLE);
    case VulkanChunk::vkQueueInsertDebugUtilsLabelEXT:
      return Serialise_vkQueueInsertDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdBeginDebugUtilsLabelEXT:
      return Serialise_vkCmdBeginDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdEndDebugUtilsLabelEXT:
      return Serialise_vkCmdEndDebugUtilsLabelEXT(ser, VK_NULL_HANDLE);
    case VulkanChunk::vkCmdInsertDebugUtilsLabelEXT:
      return Serialise_vkCmdInsertDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);

    case VulkanChunk::vkCreateSamplerYcbcrConversion:
      return Serialise_vkCreateSamplerYcbcrConversion(ser, VK_NULL_HANDLE, NULL, NULL, NULL);

    case VulkanChunk::vkCmdSetDeviceMask:
      return Serialise_vkCmdSetDeviceMask(ser, VK_NULL_HANDLE, 0);
    case VulkanChunk::vkCmdDispatchBase:
      return Serialise_vkCmdDispatchBase(ser, VK_NULL_HANDLE, 0, 0, 0, 0, 0, 0);

    case VulkanChunk::vkGetDeviceQueue2:
      return Serialise_vkGetDeviceQueue2(ser, VK_NULL_HANDLE, NULL, NULL);

    case VulkanChunk::vkCmdDrawIndirectCount:
      return Serialise_vkCmdDrawIndirectCount(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0,
                                              VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdDrawIndexedIndirectCount:
      return Serialise_vkCmdDrawIndexedIndirectCount(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0,
                                                     VK_NULL_HANDLE, 0, 0, 0);

    case VulkanChunk::vkCreateRenderPass2:
      return Serialise_vkCreateRenderPass2(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCmdBeginRenderPass2:
      return Serialise_vkCmdBeginRenderPass2(ser, VK_NULL_HANDLE, NULL, NULL);
    case VulkanChunk::vkCmdNextSubpass2:
      return Serialise_vkCmdNextSubpass2(ser, VK_NULL_HANDLE, NULL, NULL);
    case VulkanChunk::vkCmdEndRenderPass2:
      return Serialise_vkCmdEndRenderPass2(ser, VK_NULL_HANDLE, NULL);

    case VulkanChunk::vkCmdBindTransformFeedbackBuffersEXT:
      return Serialise_vkCmdBindTransformFeedbackBuffersEXT(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL,
                                                            NULL);
    case VulkanChunk::vkCmdBeginTransformFeedbackEXT:
      return Serialise_vkCmdBeginTransformFeedbackEXT(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL);
    case VulkanChunk::vkCmdEndTransformFeedbackEXT:
      return Serialise_vkCmdEndTransformFeedbackEXT(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL);
    case VulkanChunk::vkCmdBeginQueryIndexedEXT:
      return Serialise_vkCmdBeginQueryIndexedEXT(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
    case VulkanChunk::vkCmdEndQueryIndexedEXT:
      return Serialise_vkCmdEndQueryIndexedEXT(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdDrawIndirectByteCountEXT:
      return Serialise_vkCmdDrawIndirectByteCountEXT(ser, VK_NULL_HANDLE, 0, 0, VK_NULL_HANDLE, 0,
                                                     0, 0);
    case VulkanChunk::vkCmdBeginConditionalRenderingEXT:
      return Serialise_vkCmdBeginConditionalRenderingEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdEndConditionalRenderingEXT:
      return Serialise_vkCmdEndConditionalRenderingEXT(ser, VK_NULL_HANDLE);
    case VulkanChunk::vkCmdSetSampleLocationsEXT:
      return Serialise_vkCmdSetSampleLocationsEXT(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdSetDiscardRectangleEXT:
      return Serialise_vkCmdSetDiscardRectangleEXT(ser, VK_NULL_HANDLE, 0, 0, NULL);
    case VulkanChunk::DeviceMemoryRefs:
    {
      rdcarray<MemRefInterval> data;
      return GetResourceManager()->Serialise_DeviceMemoryRefs(ser, data);
    }
    case VulkanChunk::vkResetQueryPool:
      return Serialise_vkResetQueryPool(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdSetLineStippleEXT:
      return Serialise_vkCmdSetLineStippleEXT(ser, VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::ImageRefs:
    {
      SCOPED_LOCK(m_ImageStatesLock);
      return GetResourceManager()->Serialise_ImageRefs(ser, m_ImageStates);
    }
    case VulkanChunk::vkGetSemaphoreCounterValue:
      return Serialise_vkGetSemaphoreCounterValue(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkWaitSemaphores:
      return Serialise_vkWaitSemaphores(ser, VK_NULL_HANDLE, NULL, 0);
    case VulkanChunk::vkSignalSemaphore:
      return Serialise_vkSignalSemaphore(ser, VK_NULL_HANDLE, NULL);

    case VulkanChunk::vkQueuePresentKHR:
      return Serialise_vkQueuePresentKHR(ser, VK_NULL_HANDLE, NULL);

    case VulkanChunk::vkCmdSetCullMode:
      return Serialise_vkCmdSetCullMode(ser, VK_NULL_HANDLE, VK_CULL_MODE_FLAG_BITS_MAX_ENUM);
    case VulkanChunk::vkCmdSetFrontFace:
      return Serialise_vkCmdSetFrontFace(ser, VK_NULL_HANDLE, VK_FRONT_FACE_MAX_ENUM);
    case VulkanChunk::vkCmdSetPrimitiveTopology:
      return Serialise_vkCmdSetPrimitiveTopology(ser, VK_NULL_HANDLE, VK_PRIMITIVE_TOPOLOGY_MAX_ENUM);
    case VulkanChunk::vkCmdSetViewportWithCount:
      return Serialise_vkCmdSetViewportWithCount(ser, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkCmdSetScissorWithCount:
      return Serialise_vkCmdSetScissorWithCount(ser, VK_NULL_HANDLE, 0, NULL);
    case VulkanChunk::vkCmdBindVertexBuffers2:
      return Serialise_vkCmdBindVertexBuffers2(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL, NULL, NULL);
    case VulkanChunk::vkCmdSetDepthTestEnable:
      return Serialise_vkCmdSetDepthTestEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetDepthWriteEnable:
      return Serialise_vkCmdSetDepthWriteEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetDepthCompareOp:
      return Serialise_vkCmdSetDepthCompareOp(ser, VK_NULL_HANDLE, VK_COMPARE_OP_MAX_ENUM);
    case VulkanChunk::vkCmdSetDepthBoundsTestEnable:
      return Serialise_vkCmdSetDepthBoundsTestEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetStencilTestEnable:
      return Serialise_vkCmdSetStencilTestEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetStencilOp:
      return Serialise_vkCmdSetStencilOp(ser, VK_NULL_HANDLE, VK_STENCIL_FACE_FLAG_BITS_MAX_ENUM,
                                         VK_STENCIL_OP_MAX_ENUM, VK_STENCIL_OP_MAX_ENUM,
                                         VK_STENCIL_OP_MAX_ENUM, VK_COMPARE_OP_MAX_ENUM);

    case VulkanChunk::vkCmdCopyBuffer2:
      return Serialise_vkCmdCopyBuffer2(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdCopyImage2: return Serialise_vkCmdCopyImage2(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdCopyBufferToImage2:
      return Serialise_vkCmdCopyBufferToImage2(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdCopyImageToBuffer2:
      return Serialise_vkCmdCopyImageToBuffer2(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdBlitImage2: return Serialise_vkCmdBlitImage2(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdResolveImage2:
      return Serialise_vkCmdResolveImage2(ser, VK_NULL_HANDLE, NULL);

    case VulkanChunk::vkCmdSetEvent2:
      return Serialise_vkCmdSetEvent2(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdResetEvent2:
      return Serialise_vkCmdResetEvent2(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                        VK_PIPELINE_STAGE_2_NONE);
    case VulkanChunk::vkCmdWaitEvents2:
      return Serialise_vkCmdWaitEvents2(ser, VK_NULL_HANDLE, 0, NULL, NULL);
    case VulkanChunk::vkCmdPipelineBarrier2:
      return Serialise_vkCmdPipelineBarrier2(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdWriteTimestamp2:
      return Serialise_vkCmdWriteTimestamp2(ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_NONE,
                                            VK_NULL_HANDLE, 0);
    case VulkanChunk::vkQueueSubmit2:
      return Serialise_vkQueueSubmit2(ser, VK_NULL_HANDLE, 1, NULL, VK_NULL_HANDLE);
    case VulkanChunk::vkCmdWriteBufferMarker2AMD:
      return Serialise_vkCmdWriteBufferMarker2AMD(ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_NONE,
                                                  VK_NULL_HANDLE, 0, 0);
    case VulkanChunk::vkCmdSetColorWriteEnableEXT:
      return Serialise_vkCmdSetColorWriteEnableEXT(ser, VK_NULL_HANDLE, 0, NULL);

    case VulkanChunk::vkCmdSetDepthBiasEnable:
      return Serialise_vkCmdSetDepthBiasEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetLogicOpEXT:
      return Serialise_vkCmdSetLogicOpEXT(ser, VK_NULL_HANDLE, VK_LOGIC_OP_MAX_ENUM);
    case VulkanChunk::vkCmdSetPatchControlPointsEXT:
      return Serialise_vkCmdSetPatchControlPointsEXT(ser, VK_NULL_HANDLE, 0);
    case VulkanChunk::vkCmdSetPrimitiveRestartEnable:
      return Serialise_vkCmdSetPrimitiveRestartEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetRasterizerDiscardEnable:
      return Serialise_vkCmdSetRasterizerDiscardEnable(ser, VK_NULL_HANDLE, VK_FALSE);
    case VulkanChunk::vkCmdSetVertexInputEXT:
      return Serialise_vkCmdSetVertexInputEXT(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);

    case VulkanChunk::vkCmdBeginRendering:
      return Serialise_vkCmdBeginRendering(ser, VK_NULL_HANDLE, NULL);
    case VulkanChunk::vkCmdEndRendering: return Serialise_vkCmdEndRendering(ser, VK_NULL_HANDLE);

    case VulkanChunk::vkCmdSetFragmentShadingRateKHR:
      return Serialise_vkCmdSetFragmentShadingRateKHR(ser, VK_NULL_HANDLE, NULL, NULL);

    case VulkanChunk::vkSetDeviceMemoryPriorityEXT:
      return Serialise_vkSetDeviceMemoryPriorityEXT(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0.0f);

    // chunks that are reserved but not yet serialised
    case VulkanChunk::vkResetCommandPool:
    case VulkanChunk::vkCreateDepthTargetView:
      RDCERR("Unexpected Chunk type %s", ToStr(chunk).c_str());

    // no explicit default so that we have compiler warnings if a chunk isn't explicitly handled.
    case VulkanChunk::Max: break;
  }

  {
    SystemChunk system = (SystemChunk)chunk;
    if(system == SystemChunk::DriverInit)
    {
      VkInitParams InitParams;
      SERIALISE_ELEMENT(InitParams);

      SERIALISE_CHECK_READ_ERRORS();

      AddResourceCurChunk(InitParams.InstanceID);
    }
    else if(system == SystemChunk::InitialContentsList)
    {
      GetResourceManager()->CreateInitialContents(ser);

      if(initStateCurCmd != VK_NULL_HANDLE)
      {
        CloseInitStateCmd();
        SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
        SubmitCmds();
        FlushQ();
        SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
      }

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContents)
    {
      return Serialise_InitialState(ser, ResourceId(), NULL, NULL);
    }
    else if(system == SystemChunk::CaptureScope)
    {
      return Serialise_CaptureScope(ser);
    }
    else if(system == SystemChunk::CaptureEnd)
    {
      SERIALISE_ELEMENT_LOCAL(PresentedImage, ResourceId()).TypedAs("VkImage"_lit);

      SERIALISE_CHECK_READ_ERRORS();

      if(PresentedImage != ResourceId())
        m_LastPresentedImage = PresentedImage;

      if(IsLoading(m_State) && m_LastChunk != VulkanChunk::vkQueuePresentKHR)
      {
        AddEvent();

        ActionDescription action;
        action.customName = "End of Capture";
        action.flags |= ActionFlags::Present;

        action.copyDestination = m_LastPresentedImage;

        AddAction(action);
      }

      return true;
    }
    else if(system < SystemChunk::FirstDriverChunk)
    {
      RDCERR("Unexpected system chunk in capture data: %u", system);
      ser.SkipCurrentChunk();

      SERIALISE_CHECK_READ_ERRORS();
    }
    else
    {
      RDCERR("Unrecognised Chunk type %d", chunk);
      return false;
    }
  }

  return true;
}

void WrappedVulkan::AddFrameTerminator(uint64_t queueMarkerTag)
{
  if(HasFatalError())
    return;

  VkCommandBuffer cmdBuffer = GetNextCmd();
  VkResult vkr = VK_SUCCESS;

  if(cmdBuffer == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmdBuffer)->BeginCommandBuffer(Unwrap(cmdBuffer), &beginInfo);
  CheckVkResult(vkr);

  vkr = ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));
  CheckVkResult(vkr);

  VkDebugMarkerObjectTagInfoEXT tagInfo = {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT, NULL};
  tagInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT;
  tagInfo.object = uint64_t(Unwrap(cmdBuffer));
  tagInfo.tagName = queueMarkerTag;
  tagInfo.tagSize = 0;
  tagInfo.pTag = NULL;

  // check for presence of the queue marker extension
  if(ObjDisp(m_Device)->DebugMarkerSetObjectTagEXT)
  {
    vkr = ObjDisp(m_Device)->DebugMarkerSetObjectTagEXT(Unwrap(m_Device), &tagInfo);
  }

  SubmitCmds();
}

VkResourceRecord *WrappedVulkan::RegisterSurface(WindowingSystem system, void *handle)
{
  Keyboard::AddInputWindow(system, handle);

  RDCLOG("RegisterSurface() window %p", handle);

  RenderDoc::Inst().AddFrameCapturer(DeviceOwnedWindow(LayerDisp(m_Instance), handle), this);

  return (VkResourceRecord *)new PackedWindowHandle(system, handle);
}

void WrappedVulkan::ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;
  }

  if(!partial)
  {
    VkMarkerRegion::Begin("!!!!RenderDoc Internal: ApplyInitialContents");
    ApplyInitialContents();
    VkMarkerRegion::End();
  }

  m_State = CaptureState::ActiveReplaying;

  VkMarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal: RenderDoc Replay %d (%d): %u->%u",
                                        (int)replayType, (int)partial, startEventID, endEventID));

  {
    if(!partial)
    {
      m_Partial[Primary].Reset();
      m_Partial[Secondary].Reset();
      m_RenderState = VulkanRenderState();
      for(auto it = m_BakedCmdBufferInfo.begin(); it != m_BakedCmdBufferInfo.end(); it++)
        it->second.state = VulkanRenderState();
    }
    else
    {
      // Copy the state in case m_RenderState was modified externally for the partial replay.
      m_BakedCmdBufferInfo[GetPartialCommandBuffer()].state = m_RenderState;
    }

    VkResult vkr = VK_SUCCESS;

    bool rpWasActive[2] = {};

    // we'll need our own command buffer if we're replaying just a subsection
    // of events within a single command buffer record - always if it's only
    // one action, or if start event ID is > 0 we assume the outside code
    // has chosen a subsection that lies within a command buffer
    if(partial)
    {
      VkCommandBuffer cmd = m_OutsideCmdBuffer = GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return;

      // we'll explicitly submit this when we're ready
      RemovePendingCommandBuffer(cmd);

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      // we're replaying a single item inline, even if it was previously in a secondary command
      // buffer execution.
      VkSubpassContents subpassContents = m_RenderState.subpassContents;
      VkRenderingFlags dynamicFlags = m_RenderState.dynamicRendering.flags;
      m_RenderState.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
      m_RenderState.dynamicRendering.flags &= ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

      rpWasActive[Primary] = m_Partial[Primary].renderPassActive;
      rpWasActive[Secondary] = m_Partial[Secondary].renderPassActive;

      if(rpWasActive[Primary] || rpWasActive[Secondary])
      {
        const ActionDescription *action = GetAction(endEventID);

        bool rpUnneeded = false;

        // if we're only replaying an action, and it's not an draw or dispatch, don't try and bind
        // all the replay state as we don't know if it will be valid.
        if(replayType == eReplay_OnlyDraw)
        {
          if(!action)
          {
            rpUnneeded = true;
          }
          else if(!(action->flags & (ActionFlags::Drawcall | ActionFlags::Dispatch)))
          {
            rpUnneeded = true;
          }
        }

        // if we have an indirect action with one action, the subcommand will have an event which
        // isn't a ActionDescription and selecting it will still replay that indirect action. We
        // need to detect this case and ensure we prepare the RP. This doesn't happen for
        // multi-action indirects because there each subcommand has an actual ActionDescription
        if(rpUnneeded)
        {
          APIEvent ev = GetEvent(endEventID);
          if(m_StructuredFile->chunks[ev.chunkIndex]->metadata.chunkID ==
             (uint32_t)VulkanChunk::vkCmdIndirectSubCommand)
            rpUnneeded = false;
        }

        // if a render pass was active, begin it and set up the partial replay state
        m_RenderState.BeginRenderPassAndApplyState(
            this, cmd, rpUnneeded ? VulkanRenderState::BindNone : VulkanRenderState::BindGraphics,
            false);
      }
      else
      {
        // even outside of render passes, we need to restore the state
        m_RenderState.BindPipeline(this, cmd, VulkanRenderState::BindInitial, false);
      }

      m_RenderState.subpassContents = subpassContents;
      m_RenderState.dynamicRendering.flags = dynamicFlags;
    }

    RDResult status = ResultCode::Succeeded;

    if(replayType == eReplay_Full)
      status = ContextReplayLog(m_State, startEventID, endEventID, partial);
    else if(replayType == eReplay_WithoutDraw)
      status = ContextReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    else if(replayType == eReplay_OnlyDraw)
      status = ContextReplayLog(m_State, endEventID, endEventID, partial);
    else
      RDCFATAL("Unexpected replay type");

    RDCASSERTEQUAL(status.code, ResultCode::Succeeded);

    if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    {
      if(replayType == eReplay_OnlyDraw)
        UpdateImageStates(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates);

      VkCommandBuffer cmd = m_OutsideCmdBuffer;

      // end any active XFB
      if(!m_RenderState.xfbcounters.empty())
        m_RenderState.EndTransformFeedback(this, cmd);

      // end any active conditional rendering
      if(m_RenderState.IsConditionalRenderingEnabled())
        m_RenderState.EndConditionalRendering(cmd);

      // check if the render pass is active - it could have become active
      // even if it wasn't before (if the above event was a CmdBeginRenderPass).
      // If we began our own custom single-action loadrp, and it was ended by a CmdEndRenderPass,
      // we need to reverse the virtual transitions we did above, as it won't happen otherwise
      if(m_Partial[Primary].renderPassActive || m_Partial[Secondary].renderPassActive)
        m_RenderState.EndRenderPass(cmd);

      // we might have replayed a CmdBeginRenderPass or CmdEndRenderPass,
      // but we want to keep the partial replay data state intact, so restore
      // whether or not a render pass was active.
      m_Partial[Primary].renderPassActive = rpWasActive[Primary];
      m_Partial[Secondary].renderPassActive = rpWasActive[Secondary];

      ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

      AddPendingCommandBuffer(cmd);

      SubmitCmds();

      m_OutsideCmdBuffer = VK_NULL_HANDLE;
    }

    if(Vulkan_Debug_SingleSubmitFlushing())
    {
      SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
      SubmitCmds();
      FlushQ();
      SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
    }
  }

  if(!IsStructuredExporting(m_State))
  {
    AddPendingObjectCleanup([this]() {
      // destroy any events we created for waiting on
      for(size_t i = 0; i < m_CleanupEvents.size(); i++)
        ObjDisp(GetDev())->DestroyEvent(Unwrap(GetDev()), m_CleanupEvents[i], NULL);

      m_CleanupEvents.clear();

      for(const rdcpair<VkCommandPool, VkCommandBuffer> &rerecord : m_RerecordCmdList)
        vkFreeCommandBuffers(GetDev(), rerecord.first, 1, &rerecord.second);

      m_RerecordCmdList.clear();
    });
  }

  VkMarkerRegion::Set("!!!!RenderDoc Internal: Done replay");
}

template <typename SerialiserType>
void WrappedVulkan::Serialise_DebugMessages(SerialiserType &ser)
{
  rdcarray<DebugMessage> DebugMessages;

  if(ser.IsWriting())
  {
    ScopedDebugMessageSink *sink = GetDebugMessageSink();
    if(sink)
      DebugMessages.swap(sink->msgs);

    for(DebugMessage &msg : DebugMessages)
      ProcessDebugMessage(msg);
  }

  SERIALISE_ELEMENT(DebugMessages).Hidden();

  // if we're using debug messages from replay, discard any from the capture
  if(ser.IsReading() && IsLoading(m_State) && m_ReplayOptions.apiValidation)
    DebugMessages.clear();

  if(ser.IsReading() && IsLoading(m_State))
  {
    for(const DebugMessage &msg : DebugMessages)
      AddDebugMessage(msg);
  }
}

template void WrappedVulkan::Serialise_DebugMessages(WriteSerialiser &ser);
template void WrappedVulkan::Serialise_DebugMessages(ReadSerialiser &ser);

void WrappedVulkan::ProcessDebugMessage(DebugMessage &msg)
{
  // if we have the unique objects layer we can assume all objects have a unique ID, and replace
  // any text that looks like an object reference (0xHEX[NAME]).
  if(m_LayersEnabled[VkCheckLayer_unique_objects])
  {
    if(strstr(msg.description.c_str(), "0x"))
    {
      rdcstr desc = msg.description;

      int32_t offs = desc.find("0x");
      while(offs >= 0)
      {
        // if we're on a word boundary
        if(offs == 0 || !isalnum(desc[offs - 1]))
        {
          size_t end = offs + 2;

          uint64_t val = 0;

          // consume all hex chars
          while(end < desc.length())
          {
            if(desc[end] >= '0' && desc[end] <= '9')
            {
              val <<= 4;
              val += (desc[end] - '0');
              end++;
            }
            else if(desc[end] >= 'A' && desc[end] <= 'F')
            {
              val <<= 4;
              val += (desc[end] - 'A') + 0xA;
              end++;
            }
            else if(desc[end] >= 'a' && desc[end] <= 'f')
            {
              val <<= 4;
              val += (desc[end] - 'a') + 0xA;
              end++;
            }
            else
            {
              break;
            }
          }

          bool do_replace = false;

          // we now expect a [NAME]. Look for matched set of []s
          if(desc[end] == '[')
          {
            int depth = 1;
            end++;

            while(end < desc.length() && depth)
            {
              if(desc[end] == '[')
                depth++;
              else if(desc[end] == ']')
                depth--;

              end++;
            }

            do_replace = true;
          }
          // if we didn't see a trailing [], look for a preceeding handle =
          else if(offs >= 9 && desc.substr(offs - 9, 9) == "handle = ")
          {
            do_replace = true;
          }

          if(do_replace)
          {
            // unique objects layer implies this is a unique search so we don't have to worry
            // about type aliases
            ResourceId id = GetResourceManager()->GetFirstIDForHandle(val);

            if(id != ResourceId())
            {
              rdcstr idstr = ToStr(id);

              desc.erase(offs, end - offs);

              desc.insert(offs, idstr.c_str());

              offs = desc.find("0x", offs + idstr.count());
              continue;
            }
          }
        }

        offs = desc.find("0x", offs + 1);
      }

      msg.description = desc;
    }
  }
}

rdcarray<DebugMessage> WrappedVulkan::GetDebugMessages()
{
  rdcarray<DebugMessage> ret;
  ret.swap(m_DebugMessages);
  return ret;
}

void WrappedVulkan::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d)
{
  DebugMessage msg;
  msg.eventId = 0;
  if(IsActiveReplaying(m_State))
  {
    // look up the EID this action came from
    ActionUse use(m_CurChunkOffset, 0);
    auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

    if(it != m_ActionUses.end())
      msg.eventId = it->eventId;
    else
      RDCERR("Couldn't locate action use for current chunk offset %llu", m_CurChunkOffset);
  }
  msg.messageID = 0;
  msg.source = src;
  msg.category = c;
  msg.severity = sv;
  msg.description = d;
  AddDebugMessage(msg);
}

void WrappedVulkan::AddDebugMessage(DebugMessage msg)
{
  if(IsLoading(m_State))
  {
    m_EventMessages.push_back(msg);
  }
  else
  {
    m_DebugMessages.push_back(msg);
  }
}

rdcstr WrappedVulkan::GetPhysDeviceCompatString(bool externalResource, bool origInvalid)
{
  const VkDriverInfo &capture = m_OrigPhysicalDeviceData.driverInfo;
  const VkDriverInfo &replay = m_PhysicalDeviceData.driverInfo;

  if(origInvalid)
  {
    return StringFormat::Fmt(
        "This was invalid at capture time.\n"
        "You must use API validation, as RenderDoc does not handle invalid API use like this.\n\n"
        "Captured on device: %s %s, %u.%u.%u",
        ToStr(capture.Vendor()).c_str(), m_OrigPhysicalDeviceData.props.deviceName, capture.Major(),
        capture.Minor(), capture.Patch());
  }

  rdcstr ret;

  if(externalResource)
  {
    ret =
        "This resource was externally imported, which cannot happen at replay time.\n"
        "Some drivers do not allow externally-imported resources to be bound to non-external "
        "memory, meaning that captures using resources like this can't be replayed.\n\n";
  }

  if(capture == replay)
  {
    ret += StringFormat::Fmt("Captured and replayed on the same device: %s %s, %u.%u.%u",
                             ToStr(capture.Vendor()).c_str(),
                             m_OrigPhysicalDeviceData.props.deviceName, capture.Major(),
                             capture.Minor(), capture.Patch());
  }
  else
  {
    ret += StringFormat::Fmt(
        "Capture was made on: %s %s, %u.%u.%u\n"
        "Replayed on: %s %s, %u.%u.%u\n",

        // capture device
        ToStr(capture.Vendor()).c_str(), m_OrigPhysicalDeviceData.props.deviceName, capture.Major(),
        capture.Minor(), capture.Patch(),

        // replay device
        ToStr(replay.Vendor()).c_str(), m_PhysicalDeviceData.props.deviceName, replay.Major(),
        replay.Minor(), replay.Patch());

    if(capture.Vendor() != replay.Vendor())
    {
      ret += "Captures are not commonly portable between GPUs from different vendors.";
    }
    else if(strcmp(m_OrigPhysicalDeviceData.props.deviceName, m_PhysicalDeviceData.props.deviceName))
    {
      ret += "Captures are sometimes not portable between different GPUs from a vendor.";
    }
    else
    {
      ret += "Driver changes can sometimes cause captures to no longer work.";
    }
  }

  return ret;
}

void WrappedVulkan::CheckErrorVkResult(VkResult vkr)
{
  if(vkr == VK_SUCCESS || HasFatalError() || IsCaptureMode(m_State))
    return;

  if(vkr == VK_ERROR_INITIALIZATION_FAILED || vkr == VK_ERROR_DEVICE_LOST || vkr == VK_ERROR_UNKNOWN)
  {
    SET_ERROR_RESULT(m_FatalError, ResultCode::DeviceLost, "Logging device lost fatal error for %s",
                     ToStr(vkr).c_str());
    m_FailedReplayResult = m_FatalError;
  }
  else if(vkr == VK_ERROR_OUT_OF_HOST_MEMORY || vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY)
  {
    if(m_OOMHandler)
    {
      RDCLOG("Ignoring out of memory error that will be handled");
    }
    else
    {
      SET_ERROR_RESULT(m_FatalError, ResultCode::OutOfMemory,
                       "Logging out of memory fatal error for %s", ToStr(vkr).c_str());
      m_FailedReplayResult = m_FatalError;
    }
  }
  else
  {
    RDCLOG("Ignoring return code %s", ToStr(vkr).c_str());
  }
}

VkBool32 WrappedVulkan::DebugCallback(MessageSeverity severity, MessageCategory category,
                                      int messageCode, const char *pMessageId, const char *pMessage)
{
  {
    ScopedDebugMessageSink *sink = GetDebugMessageSink();

    if(sink)
    {
      DebugMessage msg;

      msg.eventId = 0;
      msg.category = category;
      msg.description = pMessage;
      msg.severity = severity;
      msg.messageID = messageCode;
      msg.source = MessageSource::API;

      // during replay we can get an eventId to correspond to this message.
      if(IsActiveReplaying(m_State))
      {
        // look up the EID this action came from
        ActionUse use(m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

        if(it != m_ActionUses.end())
          msg.eventId = it->eventId;
      }

      // function calls are replayed after the call to Serialise_DebugMessages() so we don't have a
      // sync point to gather together all the messages from the sink. But instead we can just push
      // them directly into the list since we're linearised
      if(IsLoading(m_State))
      {
        ProcessDebugMessage(msg);
        AddDebugMessage(msg);
      }
      else
      {
        sink->msgs.push_back(msg);
      }
    }
  }

  {
    // ignore perf warnings
    if(category == MessageCategory::Performance)
      return false;

    // "fragment shader writes to output location X with no matching attachment"
    // Not an error, this is defined as with all APIs to drop the output.
    if(strstr(pMessageId, "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed"))
      return false;
    // "Attachment X not written by fragment shader; undefined values will be written to attachment"
    // Not strictly an error, though more of a problem than the above. However we occasionally do
    // this on purpose in the pixel history when running history on depth targets, and it's safe to
    // silence unless we see undefined values.
    if(strstr(pMessageId, "UNASSIGNED-CoreValidation-Shader-InputNotProduced"))
      return false;

    // "Non-linear image is aliased with linear buffer"
    // Not an error, the validation layers complain at our whole-mem bufs
    if(strstr(pMessageId, "InvalidAliasing") || strstr(pMessage, "InvalidAliasing"))
      return false;

    // "vkCreateSwapchainKHR() called with imageExtent, which is outside the bounds returned by
    // vkGetPhysicalDeviceSurfaceCapabilitiesKHR(): currentExtent"
    // This is quite racey, the currentExtent can change in between us checking it and the valiation
    // layers checking it. We handle out of date, so this is likely fine.
    if(strstr(pMessageId, "VUID-VkSwapchainCreateInfoKHR-imageExtent"))
      return false;

    // "Missing extension required by the device extension VK_KHR_driver_properties:
    // VK_KHR_get_physical_device_properties2. The Vulkan spec states: All required extensions for
    // each extension in the VkDeviceCreateInfo::ppEnabledExtensionNames list must also be present
    // in that list."
    // During capture we can't enable instance extensions so it's impossible for us to enable gpdp2,
    // but we still want to use driver properties and in practice it's safe.
    if(strstr(pMessage, "VK_KHR_get_physical_device_properties2") &&
       strstr(pMessage, "VK_KHR_driver_properties"))
      return false;

    RDCWARN("[%s] %s", pMessageId, pMessage);
  }

  return false;
}

VkBool32 VKAPI_PTR WrappedVulkan::DebugReportCallbackStatic(VkDebugReportFlagsEXT flags,
                                                            VkDebugReportObjectTypeEXT objectType,
                                                            uint64_t object, size_t location,
                                                            int32_t messageCode,
                                                            const char *pLayerPrefix,
                                                            const char *pMessage, void *pUserData)
{
  MessageSeverity severity = MessageSeverity::Low;

  if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    severity = MessageSeverity::High;
  else if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    severity = MessageSeverity::Medium;
  else if(flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    severity = MessageSeverity::Low;
  else if(flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    severity = MessageSeverity::Info;

  MessageCategory category = MessageCategory::Miscellaneous;

  if(flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    category = MessageCategory::Performance;

  return ((WrappedVulkan *)pUserData)
      ->DebugCallback(severity, category, messageCode, pLayerPrefix, pMessage);
}

VkBool32 VKAPI_PTR WrappedVulkan::DebugUtilsCallbackStatic(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
  MessageSeverity severity = MessageSeverity::Low;

  if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    severity = MessageSeverity::High;
  else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    severity = MessageSeverity::Medium;
  else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    severity = MessageSeverity::Low;
  else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    severity = MessageSeverity::Info;

  MessageCategory category = MessageCategory::Miscellaneous;

  if(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    category = MessageCategory::Performance;

  rdcstr msgid;

  const char *pMessageId = pCallbackData->pMessageIdName;
  int messageCode = pCallbackData->messageIdNumber;

  if(messageCode == 0 && pMessageId && !strncmp(pMessageId, "VUID", 4))
  {
    const char *c = pMessageId + strlen(pMessageId) - 1;
    int mult = 1;

    while(c > pMessageId && *c >= '0' && *c <= '9')
    {
      messageCode += mult * int(*c - '0');
      mult *= 10;
      c--;
    }
  }

  if(!pMessageId)
  {
    msgid = StringFormat::Fmt("%d", pCallbackData->messageIdNumber);
    pMessageId = msgid.c_str();
  }

  return ((WrappedVulkan *)pUserData)
      ->DebugCallback(severity, category, messageCode, pMessageId, pCallbackData->pMessage);
}

const VkFormatProperties &WrappedVulkan::GetFormatProperties(VkFormat f)
{
  if(m_PhysicalDeviceData.fmtProps.find(f) == m_PhysicalDeviceData.fmtProps.end())
  {
    ObjDisp(m_PhysicalDevice)
        ->GetPhysicalDeviceFormatProperties(Unwrap(m_PhysicalDevice), f,
                                            &m_PhysicalDeviceData.fmtProps[f]);
  }
  return m_PhysicalDeviceData.fmtProps[f];
}

bool WrappedVulkan::InRerecordRange(ResourceId cmdid)
{
  // if we have an outside command buffer, assume the range is valid and we're replaying all events
  // onto it.
  if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    return true;

  // if not, check if we're one of the actual partial command buffers and check to see if we're in
  // the range for their partial replay.
  for(int p = 0; p < ePartialNum; p++)
  {
    if(cmdid == m_Partial[p].partialParent)
    {
      return m_BakedCmdBufferInfo[m_Partial[p].partialParent].curEventID + m_Partial[p].baseEvent <=
             m_LastEventID;
    }
  }

  // otherwise just check if we have a re-record command buffer for this, as then we're doing a full
  // re-record and replay of the command buffer
  return m_RerecordCmds.find(cmdid) != m_RerecordCmds.end();
}

bool WrappedVulkan::HasRerecordCmdBuf(ResourceId cmdid)
{
  if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    return true;

  return m_RerecordCmds.find(cmdid) != m_RerecordCmds.end();
}

bool WrappedVulkan::ShouldUpdateRenderState(ResourceId cmdid, bool forcePrimary)
{
  if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    return true;

  // if forcePrimary is set we're tracking renderpass activity that only happens in the primary
  // command buffer. So even if a secondary is partial, we still want to check it.
  if(forcePrimary)
    return m_Partial[Primary].partialParent == cmdid;

  // otherwise, if a secondary command buffer is partial we want to *ignore* any state setting
  // happening in the primary buffer as fortunately no state is inherited (so we don't need to
  // worry about any state before the execute) and any state setting recorded afterwards would
  // incorrectly override what we have.
  if(m_Partial[Secondary].partialParent != ResourceId())
    return cmdid == m_Partial[Secondary].partialParent;

  return cmdid == m_Partial[Primary].partialParent;
}

bool WrappedVulkan::IsRenderpassOpen(ResourceId cmdid)
{
  if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    return true;

  // if not, check if we're one of the actual partial command buffers and check to see if we're in
  // the range for their partial replay.
  for(int p = 0; p < ePartialNum; p++)
  {
    if(cmdid == m_Partial[p].partialParent)
    {
      return m_BakedCmdBufferInfo[cmdid].renderPassOpen;
    }
  }

  return false;
}

VkCommandBuffer WrappedVulkan::RerecordCmdBuf(ResourceId cmdid, PartialReplayIndex partialType)
{
  if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    return m_OutsideCmdBuffer;

  auto it = m_RerecordCmds.find(cmdid);

  if(it == m_RerecordCmds.end())
  {
    RDCERR("Didn't generate re-record command for %s", ToStr(cmdid).c_str());
    return NULL;
  }

  return it->second;
}

ResourceId WrappedVulkan::GetPartialCommandBuffer()
{
  if(m_Partial[Secondary].partialParent != ResourceId())
    return m_Partial[Secondary].partialParent;
  return m_Partial[Primary].partialParent;
}

void WrappedVulkan::AddAction(const ActionDescription &a)
{
  m_AddedAction = true;

  ActionDescription action = a;
  action.eventId = m_LastCmdBufferID != ResourceId()
                       ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID
                       : m_RootEventID;
  action.actionId = m_LastCmdBufferID != ResourceId()
                        ? m_BakedCmdBufferInfo[m_LastCmdBufferID].actionCount
                        : m_RootActionID;

  for(int i = 0; i < 8; i++)
    action.outputs[i] = ResourceId();

  action.depthOut = ResourceId();

  if(m_LastCmdBufferID != ResourceId())
  {
    const VulkanRenderState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

    ResourceId fb = state.GetFramebuffer();
    ResourceId rp = state.GetRenderPass();
    uint32_t sp = state.subpass;

    if(fb != ResourceId() && rp != ResourceId())
    {
      const rdcarray<ResourceId> &atts = state.GetFramebufferAttachments();

      RDCASSERT(sp < m_CreationInfo.m_RenderPass[rp].subpasses.size());

      rdcarray<uint32_t> &colAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].colorAttachments;
      int32_t dsAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].depthstencilAttachment;

      RDCASSERT(colAtt.size() <= ARRAY_COUNT(action.outputs));

      for(size_t i = 0; i < ARRAY_COUNT(action.outputs) && i < colAtt.size(); i++)
      {
        if(colAtt[i] == VK_ATTACHMENT_UNUSED)
          continue;

        RDCASSERT(colAtt[i] < atts.size());
        action.outputs[i] =
            GetResourceManager()->GetOriginalID(m_CreationInfo.m_ImageView[atts[colAtt[i]]].image);
      }

      if(dsAtt != -1)
      {
        RDCASSERT(dsAtt < (int32_t)atts.size());
        action.depthOut =
            GetResourceManager()->GetOriginalID(m_CreationInfo.m_ImageView[atts[dsAtt]].image);
      }
    }
    else if(state.dynamicRendering.active)
    {
      const VulkanRenderState::DynamicRendering &dyn = state.dynamicRendering;

      for(size_t i = 0; i < ARRAY_COUNT(action.outputs) && i < dyn.color.size(); i++)
      {
        if(dyn.color[i].imageView == VK_NULL_HANDLE)
          continue;

        action.outputs[i] = GetResourceManager()->GetOriginalID(
            m_CreationInfo.m_ImageView[GetResID(dyn.color[i].imageView)].image);
      }

      if(dyn.depth.imageView != VK_NULL_HANDLE)
      {
        action.depthOut = GetResourceManager()->GetOriginalID(
            m_CreationInfo.m_ImageView[GetResID(dyn.depth.imageView)].image);
      }
    }
  }

  // markers don't increment action ID
  ActionFlags MarkerMask = ActionFlags::SetMarker | ActionFlags::PushMarker |
                           ActionFlags::PopMarker | ActionFlags::PassBoundary;
  if(!(action.flags & MarkerMask))
  {
    if(m_LastCmdBufferID != ResourceId())
      m_BakedCmdBufferInfo[m_LastCmdBufferID].actionCount++;
    else
      m_RootActionID++;
  }

  action.events.swap(m_LastCmdBufferID != ResourceId()
                         ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents
                         : m_RootEvents);

  // should have at least the root action here, push this action
  // onto the back's children list.
  if(!GetActionStack().empty())
  {
    VulkanActionTreeNode node(action);

    node.resourceUsage.swap(m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage);

    if(m_LastCmdBufferID != ResourceId())
      AddUsage(node, m_BakedCmdBufferInfo[m_LastCmdBufferID].debugMessages);

    node.children.reserve(action.children.size());
    for(const ActionDescription &child : action.children)
      node.children.push_back(VulkanActionTreeNode(child));
    GetActionStack().back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost action stack!");
}

void WrappedVulkan::AddUsage(VulkanActionTreeNode &actionNode, rdcarray<DebugMessage> &debugMessages)
{
  ActionDescription &action = actionNode.action;

  const VulkanRenderState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;
  VulkanCreationInfo &c = m_CreationInfo;
  uint32_t eid = action.eventId;

  ActionFlags DrawMask = ActionFlags::Drawcall | ActionFlags::Dispatch;
  if(!(action.flags & DrawMask))
    return;

  //////////////////////////////
  // Vertex input

  if(action.flags & ActionFlags::Indexed && state.ibuffer.buf != ResourceId())
    actionNode.resourceUsage.push_back(
        make_rdcpair(state.ibuffer.buf, EventUsage(eid, ResourceUsage::IndexBuffer)));

  for(size_t i = 0; i < state.vbuffers.size(); i++)
  {
    if(state.vbuffers[i].buf != ResourceId())
    {
      actionNode.resourceUsage.push_back(
          make_rdcpair(state.vbuffers[i].buf, EventUsage(eid, ResourceUsage::VertexBuffer)));
    }
  }

  for(uint32_t i = state.firstxfbcounter;
      i < state.firstxfbcounter + state.xfbcounters.size() && i < state.xfbbuffers.size(); i++)
  {
    if(state.xfbbuffers[i].buf != ResourceId())
    {
      actionNode.resourceUsage.push_back(
          make_rdcpair(state.xfbbuffers[i].buf, EventUsage(eid, ResourceUsage::StreamOut)));
    }
  }

  //////////////////////////////
  // Shaders

  static bool hugeRangeWarned = false;

  int shaderStart = 0;
  int shaderEnd = 0;
  if(action.flags & ActionFlags::Dispatch)
  {
    shaderStart = 5;
    shaderEnd = 6;
  }
  else if(action.flags & ActionFlags::Drawcall)
  {
    shaderStart = 0;
    shaderEnd = 5;
  }

  for(int shad = shaderStart; shad < shaderEnd; shad++)
  {
    bool compute = (shad == 5);
    ResourceId pipe = (compute ? state.compute.pipeline : state.graphics.pipeline);
    VulkanCreationInfo::Pipeline::Shader &sh = c.m_Pipeline[pipe].shaders[shad];
    if(sh.module == ResourceId())
      continue;

    ResourceId origPipe = GetResourceManager()->GetOriginalID(pipe);
    ResourceId origShad = GetResourceManager()->GetOriginalID(sh.module);

    // 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)
    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descSets =
        (compute ? state.compute.descSets : state.graphics.descSets);

    RDCASSERT(sh.mapping);

    struct ResUsageType
    {
      ResUsageType(rdcarray<Bindpoint> &a, ResourceUsage u) : bindmap(a), usage(u) {}
      rdcarray<Bindpoint> &bindmap;
      ResourceUsage usage;
    };

    ResUsageType types[] = {
        ResUsageType(sh.mapping->readOnlyResources, ResourceUsage::VS_Resource),
        ResUsageType(sh.mapping->readWriteResources, ResourceUsage::VS_RWResource),
        ResUsageType(sh.mapping->constantBlocks, ResourceUsage::VS_Constants),
    };

    DebugMessage msg;
    msg.eventId = eid;
    msg.category = MessageCategory::Execution;
    msg.messageID = 0;
    msg.source = MessageSource::IncorrectAPIUse;
    msg.severity = MessageSeverity::High;

    for(size_t t = 0; t < ARRAY_COUNT(types); t++)
    {
      for(size_t i = 0; i < types[t].bindmap.size(); i++)
      {
        if(!types[t].bindmap[i].used)
          continue;

        // ignore push constants
        if(t == 2 && !sh.refl->constantBlocks[i].bufferBacked)
          continue;

        int32_t bindset = types[t].bindmap[i].bindset;
        int32_t bind = types[t].bindmap[i].bind;

        if(bindset >= (int32_t)descSets.size() || descSets[bindset].descSet == ResourceId())
        {
          msg.description =
              StringFormat::Fmt("Shader referenced a descriptor set %i that was not bound", bindset);
          debugMessages.push_back(msg);
          continue;
        }

        DescriptorSetInfo &descset = m_DescriptorSetState[descSets[bindset].descSet];
        DescSetLayout &layout = c.m_DescSetLayout[descset.layout];

        ResourceId layoutId = GetResourceManager()->GetOriginalID(descset.layout);

        if(layout.bindings.empty())
        {
          msg.description =
              StringFormat::Fmt("Shader referenced a descriptor set %i that was not bound", bindset);
          debugMessages.push_back(msg);
          continue;
        }

        if(bind >= (int32_t)layout.bindings.size())
        {
          msg.description = StringFormat::Fmt(
              "Shader referenced a bind %i in descriptor set %i that does not exist. Mismatched "
              "descriptor set?",
              bind, bindset);
          debugMessages.push_back(msg);
          continue;
        }

        // no object to mark for usage with inline blocks
        if(layout.bindings[bind].layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
          continue;

        ResourceUsage usage = ResourceUsage(uint32_t(types[t].usage) + shad);

        if(bind >= (int32_t)descset.data.binds.size())
        {
          msg.description = StringFormat::Fmt(
              "Shader referenced a bind %i in descriptor set %i that does not exist. Mismatched "
              "descriptor set?",
              bind, bindset);
          debugMessages.push_back(msg);
          continue;
        }

        uint32_t descriptorCount = layout.bindings[bind].descriptorCount;
        if(layout.bindings[bind].variableSize)
          descriptorCount = descset.data.variableDescriptorCount;

        if(descriptorCount > 1000)
        {
          if(!hugeRangeWarned)
            RDCWARN("Skipping large, most likely 'bindless', descriptor range");
          hugeRangeWarned = true;
          continue;
        }

        for(uint32_t a = 0; a < descriptorCount; a++)
        {
          if(!descset.data.binds[bind])
            continue;

          DescriptorSetSlot &slot = descset.data.binds[bind][a];

          // handled as part of the framebuffer attachments
          if(slot.type == DescriptorSlotType::InputAttachment)
            continue;

          // ignore unwritten descriptors
          if(slot.type == DescriptorSlotType::Unwritten)
            continue;

          // we don't mark samplers with usage
          if(slot.type == DescriptorSlotType::Sampler)
            continue;

          ResourceId id;

          switch(slot.type)
          {
            case DescriptorSlotType::CombinedImageSampler:
            case DescriptorSlotType::SampledImage:
            case DescriptorSlotType::StorageImage:
              if(slot.resource != ResourceId())
                id = c.m_ImageView[slot.resource].image;
              break;
            case DescriptorSlotType::UniformTexelBuffer:
            case DescriptorSlotType::StorageTexelBuffer:
              if(slot.resource != ResourceId())
                id = c.m_BufferView[slot.resource].buffer;
              break;
            case DescriptorSlotType::UniformBuffer:
            case DescriptorSlotType::UniformBufferDynamic:
            case DescriptorSlotType::StorageBuffer:
            case DescriptorSlotType::StorageBufferDynamic:
              if(slot.resource != ResourceId())
                id = slot.resource;
              break;
            default: RDCERR("Unexpected type %d", slot.type); break;
          }

          if(id != ResourceId())
            actionNode.resourceUsage.push_back(make_rdcpair(id, EventUsage(eid, usage)));
        }
      }
    }
  }

  //////////////////////////////
  // Framebuffer/renderpass

  AddFramebufferUsage(actionNode, state);
}

void WrappedVulkan::AddFramebufferUsage(VulkanActionTreeNode &actionNode,
                                        const VulkanRenderState &renderState)
{
  ResourceId renderPass = renderState.GetRenderPass();
  ResourceId framebuffer = renderState.GetFramebuffer();

  uint32_t subpass = renderState.subpass;
  const rdcarray<ResourceId> &fbattachments = renderState.GetFramebufferAttachments();

  VulkanCreationInfo &c = m_CreationInfo;
  uint32_t e = actionNode.action.eventId;

  if(renderPass != ResourceId() && framebuffer != ResourceId())
  {
    const VulkanCreationInfo::RenderPass &rp = c.m_RenderPass[renderPass];

    if(subpass >= rp.subpasses.size())
    {
      RDCERR("Invalid subpass index %u, only %u subpasses exist in this renderpass", subpass,
             (uint32_t)rp.subpasses.size());
    }
    else
    {
      const VulkanCreationInfo::RenderPass::Subpass &sub = rp.subpasses[subpass];

      for(size_t i = 0; i < sub.inputAttachments.size(); i++)
      {
        uint32_t att = sub.inputAttachments[i];
        if(att == VK_ATTACHMENT_UNUSED)
          continue;
        actionNode.resourceUsage.push_back(
            make_rdcpair(c.m_ImageView[fbattachments[att]].image,
                         EventUsage(e, ResourceUsage::InputTarget, fbattachments[att])));
      }

      for(size_t i = 0; i < sub.colorAttachments.size(); i++)
      {
        uint32_t att = sub.colorAttachments[i];
        if(att == VK_ATTACHMENT_UNUSED)
          continue;
        actionNode.resourceUsage.push_back(
            make_rdcpair(c.m_ImageView[fbattachments[att]].image,
                         EventUsage(e, ResourceUsage::ColorTarget, fbattachments[att])));
      }

      if(sub.depthstencilAttachment >= 0)
      {
        int32_t att = sub.depthstencilAttachment;
        actionNode.resourceUsage.push_back(
            make_rdcpair(c.m_ImageView[fbattachments[att]].image,
                         EventUsage(e, ResourceUsage::DepthStencilTarget, fbattachments[att])));
      }
    }
  }
  else if(renderState.dynamicRendering.active)
  {
    const VulkanRenderState::DynamicRendering &dyn = renderState.dynamicRendering;

    for(size_t i = 0; i < dyn.color.size(); i++)
    {
      if(dyn.color[i].imageView == VK_NULL_HANDLE)
        continue;

      actionNode.resourceUsage.push_back(make_rdcpair(
          c.m_ImageView[GetResID(dyn.color[i].imageView)].image,
          EventUsage(e, ResourceUsage::ColorTarget, GetResID(dyn.color[i].imageView))));
    }

    if(dyn.depth.imageView != VK_NULL_HANDLE)
    {
      actionNode.resourceUsage.push_back(make_rdcpair(
          c.m_ImageView[GetResID(dyn.depth.imageView)].image,
          EventUsage(e, ResourceUsage::DepthStencilTarget, GetResID(dyn.depth.imageView))));
    }

    if(dyn.stencil.imageView != VK_NULL_HANDLE && dyn.depth.imageView != dyn.stencil.imageView)
    {
      actionNode.resourceUsage.push_back(make_rdcpair(
          c.m_ImageView[GetResID(dyn.stencil.imageView)].image,
          EventUsage(e, ResourceUsage::DepthStencilTarget, GetResID(dyn.stencil.imageView))));
    }
  }
}

void WrappedVulkan::AddFramebufferUsageAllChildren(VulkanActionTreeNode &actionNode,
                                                   const VulkanRenderState &renderState)
{
  for(VulkanActionTreeNode &c : actionNode.children)
    AddFramebufferUsageAllChildren(c, renderState);

  AddFramebufferUsage(actionNode, renderState);
}

void WrappedVulkan::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventId = m_LastCmdBufferID != ResourceId()
                         ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID
                         : m_RootEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  for(DebugMessage &msg : m_EventMessages)
    msg.eventId = apievent.eventId;

  if(m_LastCmdBufferID != ResourceId())
  {
    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.push_back(apievent);

    m_BakedCmdBufferInfo[m_LastCmdBufferID].debugMessages.append(m_EventMessages);
    m_EventMessages.clear();
  }
  else
  {
    m_RootEvents.push_back(apievent);
    m_Events.resize(apievent.eventId + 1);
    m_Events[apievent.eventId] = apievent;

    m_DebugMessages.append(m_EventMessages);
    m_EventMessages.clear();
  }
}

const APIEvent &WrappedVulkan::GetEvent(uint32_t eventId)
{
  // start at where the requested eventId would be
  size_t idx = eventId;

  // find the next valid event (some may be skipped)
  while(idx < m_Events.size() - 1 && m_Events[idx].eventId == 0)
    idx++;

  return m_Events[RDCMIN(idx, m_Events.size() - 1)];
}

const ActionDescription *WrappedVulkan::GetAction(uint32_t eventId)
{
  if(eventId >= m_Actions.size())
    return NULL;

  return m_Actions[eventId];
}

uint32_t WrappedVulkan::FindCommandQueueFamily(ResourceId cmdId)
{
  auto it = m_commandQueueFamilies.find(cmdId);
  if(it == m_commandQueueFamilies.end())
  {
    RDCERR("Unknown queue family for %s", ToStr(cmdId).c_str());
    return m_QueueFamilyIdx;
  }
  return it->second;
}

void WrappedVulkan::InsertCommandQueueFamily(ResourceId cmdId, uint32_t queueFamilyIndex)
{
  m_commandQueueFamilies[cmdId] = queueFamilyIndex;
}
LockedImageStateRef WrappedVulkan::FindImageState(ResourceId id)
{
  SCOPED_LOCK(m_ImageStatesLock);
  auto it = m_ImageStates.find(id);
  if(it != m_ImageStates.end())
    return it->second.LockWrite();
  else
    return LockedImageStateRef();
}

LockedConstImageStateRef WrappedVulkan::FindConstImageState(ResourceId id)
{
  SCOPED_LOCK(m_ImageStatesLock);
  auto it = m_ImageStates.find(id);
  if(it != m_ImageStates.end())
    return it->second.LockRead();
  else
    return LockedConstImageStateRef();
}

LockedImageStateRef WrappedVulkan::InsertImageState(VkImage wrappedHandle, ResourceId id,
                                                    const ImageInfo &info, FrameRefType refType,
                                                    bool *inserted)
{
  SCOPED_LOCK(m_ImageStatesLock);
  auto it = m_ImageStates.find(id);
  if(it != m_ImageStates.end())
  {
    if(inserted != NULL)
      *inserted = false;
    return it->second.LockWrite();
  }
  else
  {
    if(inserted != NULL)
      *inserted = true;
    it = m_ImageStates.insert({id, LockingImageState(wrappedHandle, info, refType)}).first;
    return it->second.LockWrite();
  }
}

VkQueueFlags WrappedVulkan::GetCommandType(ResourceId cmdId)
{
  auto it = m_commandQueueFamilies.find(cmdId);
  if(it == m_commandQueueFamilies.end())
  {
    RDCERR("Unknown queue family for %s", ToStr(cmdId).c_str());
    return VkQueueFlags(0);
  }
  return m_PhysicalDeviceData.queueProps[it->second].queueFlags;
}

bool WrappedVulkan::EraseImageState(ResourceId id)
{
  SCOPED_LOCK(m_ImageStatesLock);
  auto it = m_ImageStates.find(id);
  if(it != m_ImageStates.end())
  {
    m_ImageStates.erase(it);
    return true;
  }
  return false;
}

void WrappedVulkan::UpdateImageStates(const rdcflatmap<ResourceId, ImageState> &dstStates)
{
  // this function expects the number of updates to be orders of magnitude fewer than the number of
  // existing images. If there are a small number of images in total then it doesn't matter much,
  // and if there are a large number of images then it's better to do repeated map lookups rather
  // than spend time iterating linearly across the map for a sparse set of updates.
  SCOPED_LOCK(m_ImageStatesLock);
  auto dstIt = dstStates.begin();
  ImageTransitionInfo info = GetImageTransitionInfo();
  while(dstIt != dstStates.end())
  {
    // find the entry. This is expected because images are only not in the map if we've never seen
    // them before, a rare case.
    auto it = m_ImageStates.find(dstIt->first);

    // insert the initial state if needed.
    if(it == m_ImageStates.end())
    {
      it = m_ImageStates
               .insert({dstIt->first,
                        LockingImageState(dstIt->second.wrappedHandle, dstIt->second.GetImageInfo(),
                                          info.GetDefaultRefType())})
               .first;
      dstIt->second.InitialState(*it->second.LockWrite());
    }

    // merge in the info into the entry.
    it->second.LockWrite()->Merge(dstIt->second, info);
    ++dstIt;
  }
}

void WrappedVulkan::ReplayDraw(VkCommandBuffer cmd, const ActionDescription &action)
{
  // if this isn't a multidraw (or it's the first action in a multidraw, it's fairly easy
  if(action.drawIndex == 0)
  {
    if(action.flags & ActionFlags::Indexed)
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), action.numIndices, action.numInstances,
                                   action.indexOffset, action.baseVertex, action.instanceOffset);
    else
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), action.numIndices, action.numInstances,
                            action.vertexOffset, action.instanceOffset);
  }
  else
  {
    // otherwise it's a bit more complex, we need to set up a multidraw with the first N draws nop'd
    // out and the parameters added into the last one

    VkMarkerRegion::Begin(StringFormat::Fmt("ReplayDraw(drawIndex=%u)", action.drawIndex), cmd);

    bytebuf params;

    if(action.flags & ActionFlags::Indexed)
    {
      VkDrawIndexedIndirectCommand drawParams;
      drawParams.indexCount = action.numIndices;
      drawParams.instanceCount = action.numInstances;
      drawParams.firstIndex = action.indexOffset;
      drawParams.vertexOffset = action.baseVertex;
      drawParams.firstInstance = action.instanceOffset;

      params.resize(sizeof(drawParams));
      memcpy(params.data(), &drawParams, sizeof(drawParams));
    }
    else
    {
      VkDrawIndirectCommand drawParams;

      drawParams.vertexCount = action.numIndices;
      drawParams.instanceCount = action.numInstances;
      drawParams.firstVertex = action.vertexOffset;
      drawParams.firstInstance = action.instanceOffset;

      params.resize(sizeof(drawParams));
      memcpy(params.data(), &drawParams, sizeof(drawParams));
    }

    // ensure the custom buffer is large enough
    VkDeviceSize bufLength = params.size() * (action.drawIndex + 1);

    RDCASSERT(bufLength <= m_IndirectBufferSize, bufLength, m_IndirectBufferSize);

    VkBufferMemoryBarrier bufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_IndirectBuffer.buf),
        m_IndirectBufferSize,
        m_IndirectBufferSize,
    };

    // wait for any previous indirect draws to complete before filling/transferring
    DoPipelineBarrier(cmd, 1, &bufBarrier);

    // initialise to 0 so all other draws don't draw anything
    ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(m_IndirectBuffer.buf), m_IndirectBufferSize,
                                m_IndirectBufferSize, 0);

    // wait for fill to complete before update
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    DoPipelineBarrier(cmd, 1, &bufBarrier);

    // upload the parameters for the draw we want
    ObjDisp(cmd)->CmdUpdateBuffer(Unwrap(cmd), Unwrap(m_IndirectBuffer.buf),
                                  m_IndirectBufferSize + params.size() * action.drawIndex,
                                  params.size(), params.data());

    // finally wait for copy to complete before drawing from it
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    DoPipelineBarrier(cmd, 1, &bufBarrier);

    if(action.flags & ActionFlags::Indexed)
      ObjDisp(cmd)->CmdDrawIndexedIndirect(Unwrap(cmd), Unwrap(m_IndirectBuffer.buf),
                                           m_IndirectBufferSize, action.drawIndex + 1,
                                           (uint32_t)params.size());
    else
      ObjDisp(cmd)->CmdDrawIndirect(Unwrap(cmd), Unwrap(m_IndirectBuffer.buf), m_IndirectBufferSize,
                                    action.drawIndex + 1, (uint32_t)params.size());

    VkMarkerRegion::End(cmd);
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None
#undef Always

#include "catch/catch.hpp"

TEST_CASE("Validate supported extensions list", "[vulkan]")
{
  rdcarray<VkExtensionProperties> unsorted(&supportedExtensions[0], ARRAY_COUNT(supportedExtensions));
  rdcarray<VkExtensionProperties> sorted = unsorted;

  std::sort(sorted.begin(), sorted.end());

  for(size_t i = 0; i < unsorted.size(); i++)
  {
    CHECK(rdcstr(unsorted[i].extensionName) == rdcstr(sorted[i].extensionName));
  }
}

#endif
