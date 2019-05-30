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

#include "vk_core.h"
#include "driver/ihv/amd/amd_rgp.h"
#include "driver/shaders/spirv/spirv_compile.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "vk_debug.h"

#include "stb/stb_image_write.h"

uint64_t VkInitParams::GetSerialiseSize()
{
  // misc bytes and fixed integer members
  size_t ret = 128;

  ret += AppName.size() + EngineName.size();

  for(const std::string &s : Layers)
    ret += 8 + s.size();

  for(const std::string &s : Extensions)
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

WrappedVulkan::WrappedVulkan() : m_RenderState(this, &m_CreationInfo)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedVulkan));

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

  m_StructuredFile = &m_StoredStructuredData;

  m_SectionVersion = VkInitParams::CurrentVersion;

  InitSPIRVCompiler();
  RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);

  m_Replay.SetDriver(this);

  m_FrameCounter = 0;

  m_AppControlledCapture = false;

  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();
  tempMemoryTLSSlot = Threading::AllocateTLSSlot();
  debugMessageSinkTLSSlot = Threading::AllocateTLSSlot();

  m_RootEventID = 1;
  m_RootDrawcallID = 1;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_DrawcallCallback = NULL;

  m_CurChunkOffset = 0;
  m_AddedDrawcall = false;

  m_LastCmdBufferID = ResourceId();

  m_DrawcallStack.push_back(&m_ParentDrawcall);

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

  // in case the application leaked some objects, avoid crashing trying
  // to release them ourselves by clearing the resource manager.
  // In a well-behaved application, this should be a no-op.
  m_ResourceManager->ClearWithoutReleasing();
  SAFE_DELETE(m_ResourceManager);

  SAFE_DELETE(m_FrameReader);

  for(size_t i = 0; i < m_MemIdxMaps.size(); i++)
    delete[] m_MemIdxMaps[i];

  for(size_t i = 0; i < m_ThreadSerialisers.size(); i++)
    delete m_ThreadSerialisers[i];

  for(size_t i = 0; i < m_ThreadTempMem.size(); i++)
  {
    delete[] m_ThreadTempMem[i]->memory;
    delete m_ThreadTempMem[i];
  }
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

    if(m_SetDeviceLoaderData)
      m_SetDeviceLoaderData(m_Device, ret);
    else
      SetDispatchTableOverMagicNumber(m_Device, ret);

    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(m_Device), ret);
  }

  m_InternalCmds.pendingcmds.push_back(ret);

  return ret;
}

void WrappedVulkan::RemovePendingCommandBuffer(VkCommandBuffer cmd)
{
  for(auto it = m_InternalCmds.pendingcmds.begin(); it != m_InternalCmds.pendingcmds.end(); ++it)
  {
    if(*it == cmd)
    {
      m_InternalCmds.pendingcmds.erase(it);
      break;
    }
  }
}

void WrappedVulkan::AddPendingCommandBuffer(VkCommandBuffer cmd)
{
  m_InternalCmds.pendingcmds.push_back(cmd);
}

void WrappedVulkan::SubmitCmds(VkSemaphore *unwrappedWaitSemaphores,
                               VkPipelineStageFlags *waitStageMask, uint32_t waitSemaphoreCount)
{
  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  std::vector<VkCommandBuffer> cmds = m_InternalCmds.pendingcmds;
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = Unwrap(cmds[i]);

  VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      NULL,
      waitSemaphoreCount,
      unwrappedWaitSemaphores,
      waitStageMask,
      (uint32_t)cmds.size(),
      &cmds[0],    // command buffers
      0,
      NULL,    // signal semaphores
  };

  // we might have work to do (e.g. debug manager creation command buffer) but
  // no queue, if the device is destroyed immediately. In this case we can just
  // skip the submit
  if(m_Queue != VK_NULL_HANDLE)
  {
    VkResult vkr = ObjDisp(m_Queue)->QueueSubmit(Unwrap(m_Queue), 1, &submitInfo, VK_NULL_HANDLE);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  FlushQ();
#endif

  m_InternalCmds.submittedcmds.insert(m_InternalCmds.submittedcmds.end(),
                                      m_InternalCmds.pendingcmds.begin(),
                                      m_InternalCmds.pendingcmds.end());
  m_InternalCmds.pendingcmds.clear();
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
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
  m_InternalCmds.submittedsems.insert(m_InternalCmds.submittedsems.end(),
                                      m_InternalCmds.pendingsems.begin(),
                                      m_InternalCmds.pendingsems.end());
  m_InternalCmds.pendingsems.clear();
}

void WrappedVulkan::FlushQ()
{
  // VKTODOLOW could do away with the need for this function by keeping
  // commands until N presents later, or something, or checking on fences.
  // If we do so, then check each use for FlushQ to see if it needs a
  // CPU-GPU sync or whether it is just looking to recycle command buffers
  // (Particularly the one in vkQueuePresentKHR drawing the overlay)

  // see comment in SubmitQ()
  if(m_Queue != VK_NULL_HANDLE)
  {
    VkResult vkr = ObjDisp(m_Queue)->QueueWaitIdle(Unwrap(m_Queue));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  {
    ObjDisp(m_Queue)->DeviceWaitIdle(Unwrap(m_Device));
    VkResult vkr = ObjDisp(m_Queue)->DeviceWaitIdle(Unwrap(m_Device));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
#endif

  if(!m_InternalCmds.submittedcmds.empty())
  {
    m_InternalCmds.freecmds.insert(m_InternalCmds.freecmds.end(),
                                   m_InternalCmds.submittedcmds.begin(),
                                   m_InternalCmds.submittedcmds.end());
    m_InternalCmds.submittedcmds.clear();
  }
}

VkCommandBuffer WrappedVulkan::GetExtQueueCmd(uint32_t queueFamilyIdx)
{
  if(queueFamilyIdx >= m_ExternalQueues.size())
  {
    RDCERR("Unsupported queue family %u", queueFamilyIdx);
    return VK_NULL_HANDLE;
  }

  VkCommandBuffer buf = m_ExternalQueues[queueFamilyIdx].buffer;

  ObjDisp(buf)->ResetCommandBuffer(Unwrap(buf), 0);

  return buf;
}

void WrappedVulkan::SubmitAndFlushExtQueue(uint32_t queueFamilyIdx)
{
  if(queueFamilyIdx >= m_ExternalQueues.size())
  {
    RDCERR("Unsupported queue family %u", queueFamilyIdx);
    return;
  }

  VkCommandBuffer buf = Unwrap(m_ExternalQueues[queueFamilyIdx].buffer);

  VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      NULL,
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
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ObjDisp(q)->QueueWaitIdle(Unwrap(q));
}

uint32_t WrappedVulkan::HandlePreCallback(VkCommandBuffer commandBuffer, DrawFlags type,
                                          uint32_t multiDrawOffset)
{
  if(!m_DrawcallCallback)
    return 0;

  // look up the EID this drawcall came from
  DrawcallUse use(m_CurChunkOffset, 0);
  auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

  if(it == m_DrawcallUses.end())
  {
    RDCERR("Couldn't find drawcall use entry for %llu", m_CurChunkOffset);
    return 0;
  }

  uint32_t eventId = it->eventId;

  RDCASSERT(eventId != 0);

  // handle all aliases of this drawcall as long as it's not a multidraw
  const DrawcallDescription *draw = GetDrawcall(eventId);

  if(draw == NULL || !(draw->flags & DrawFlags::MultiDraw))
  {
    ++it;
    while(it != m_DrawcallUses.end() && it->fileOffset == m_CurChunkOffset)
    {
      m_DrawcallCallback->AliasEvent(eventId, it->eventId);
      ++it;
    }
  }

  eventId += multiDrawOffset;

  if(type == DrawFlags::Drawcall)
    m_DrawcallCallback->PreDraw(eventId, commandBuffer);
  else if(type == DrawFlags::Dispatch)
    m_DrawcallCallback->PreDispatch(eventId, commandBuffer);
  else
    m_DrawcallCallback->PreMisc(eventId, type, commandBuffer);

  return eventId;
}

std::string WrappedVulkan::GetChunkName(uint32_t idx)
{
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

byte *WrappedVulkan::GetTempMemory(size_t s)
{
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
#ifdef VK_EXT_acquire_xlib_display
    {
        VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME, VK_EXT_ACQUIRE_XLIB_DISPLAY_SPEC_VERSION,
    },
#endif
    {
        VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME, VK_EXT_ASTC_DECODE_MODE_SPEC_VERSION,
    },
    {
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, VK_EXT_CALIBRATED_TIMESTAMPS_SPEC_VERSION,
    },
    {
        VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME, VK_EXT_CONDITIONAL_RENDERING_SPEC_VERSION,
    },
    {
        VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
        VK_EXT_CONSERVATIVE_RASTERIZATION_SPEC_VERSION,
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
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_SPEC_VERSION,
    },
    {
        VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, VK_EXT_FRAGMENT_DENSITY_MAP_SPEC_VERSION,
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
        VK_EXT_HDR_METADATA_EXTENSION_NAME, VK_EXT_HDR_METADATA_SPEC_VERSION,
    },
    {
        VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, VK_EXT_HOST_QUERY_RESET_SPEC_VERSION,
    },
    {
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, VK_EXT_MEMORY_BUDGET_SPEC_VERSION,
    },
    {
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, VK_EXT_MEMORY_PRIORITY_SPEC_VERSION,
    },
    {
        VK_EXT_PCI_BUS_INFO_EXTENSION_NAME, VK_EXT_PCI_BUS_INFO_SPEC_VERSION,
    },
    {
        VK_EXT_PCI_BUS_INFO_EXTENSION_NAME, VK_EXT_PCI_BUS_INFO_SPEC_VERSION,
    },
    {
        VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
        VK_EXT_PIPELINE_CREATION_FEEDBACK_SPEC_VERSION,
    },
    {
        VK_EXT_POST_DEPTH_COVERAGE_EXTENSION_NAME, VK_EXT_POST_DEPTH_COVERAGE_SPEC_VERSION,
    },
    {
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, VK_EXT_QUEUE_FAMILY_FOREIGN_SPEC_VERSION,
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
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, VK_EXT_SWAPCHAIN_COLOR_SPACE_SPEC_VERSION,
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
        VK_GOOGLE_HLSL_FUNCTIONALITY1_EXTENSION_NAME, VK_GOOGLE_HLSL_FUNCTIONALITY1_SPEC_VERSION,
    },
#ifdef VK_IMG_format_pvrtc
    {
        VK_IMG_FORMAT_PVRTC_EXTENSION_NAME, VK_IMG_FORMAT_PVRTC_SPEC_VERSION,
    },
#endif
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
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, VK_KHR_IMAGE_FORMAT_LIST_SPEC_VERSION,
    },
    {
        VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME, VK_KHR_INCREMENTAL_PRESENT_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE1_EXTENSION_NAME, VK_KHR_MAINTENANCE1_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE2_EXTENSION_NAME, VK_KHR_MAINTENANCE2_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE3_EXTENSION_NAME, VK_KHR_MAINTENANCE3_SPEC_VERSION,
    },
    {
        VK_KHR_MULTIVIEW_EXTENSION_NAME, VK_KHR_MULTIVIEW_SPEC_VERSION,
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
        VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME, VK_KHR_SHADER_ATOMIC_INT64_SPEC_VERSION,
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
        VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME, VK_KHR_SHARED_PRESENTABLE_IMAGE_SPEC_VERSION,
    },
    {
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
        VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_SPEC_VERSION,
    },
    {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION,
    },
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_SPEC_VERSION,
    },
    {
        VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME, VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_SPEC_VERSION,
    },
    {
        VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME, VK_KHR_VARIABLE_POINTERS_SPEC_VERSION,
    },
    {
        VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, VK_KHR_VULKAN_MEMORY_MODEL_SPEC_VERSION,
    },
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
};

// this is the list of extensions we provide - regardless of whether the ICD supports them
static const VkExtensionProperties renderdocProvidedDeviceExtensions[] = {
    {VK_EXT_DEBUG_MARKER_EXTENSION_NAME, VK_EXT_DEBUG_MARKER_SPEC_VERSION},
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

void WrappedVulkan::FilterToSupportedExtensions(std::vector<VkExtensionProperties> &exts,
                                                std::vector<VkExtensionProperties> &filtered)
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
      // warn on spec version mismatch, but allow it.
      if(supportedExtensions[i].specVersion != it->specVersion)
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

  std::vector<VkExtensionProperties> exts(numExts);
  vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), pLayerName, &numExts,
                                                             &exts[0]);

  if(vkr != VK_SUCCESS)
    return vkr;

  // filter the list of extensions to only the ones we support.

  // sort the reported extensions
  std::sort(exts.begin(), exts.end());

  std::vector<VkExtensionProperties> filtered;
  filtered.reserve(exts.size());
  FilterToSupportedExtensions(exts, filtered);

  if(pLayerName == NULL)
  {
    InstanceDeviceInfo *instDevInfo = GetRecord(m_Instance)->instDevInfo;

    // extensions with conditional support
    for(auto it = filtered.begin(); it != filtered.end();)
    {
      if(!strcmp(it->extensionName, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME))
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
            // supported
            ++it;
            continue;
          }
          else
          {
            RDCWARN(
                "VkPhysicalDeviceFragmentDensityMapFeaturesEXT."
                "fragmentDensityMapNonSubsampledImages is "
                "false, can't support capture of VK_EXT_fragment_density_map");
          }
        }

        // if it wasn't supported, remove the extension
        it = filtered.erase(it);
        continue;
      }

      if(!strcmp(it->extensionName, VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
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
            // supported
            ++it;
            continue;
          }
          else
          {
            RDCWARN(
                "VkPhysicalDeviceBufferDeviceAddressFeaturesEXT.bufferDeviceAddressCaptureReplay "
                "is false, can't support capture of VK_EXT_buffer_device_address");
          }
        }

        // if it wasn't supported, remove the extension
        it = filtered.erase(it);
        continue;
      }

      ++it;
    }

    // now we can add extensions that we provide ourselves (note this isn't sorted, but we
    // don't have to sort the results, the sorting was just so we could filter optimally).
    filtered.insert(
        filtered.end(), &renderdocProvidedDeviceExtensions[0],
        &renderdocProvidedDeviceExtensions[0] + ARRAY_COUNT(renderdocProvidedDeviceExtensions));
  }

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

  std::vector<VkExtensionProperties> exts(numExts);
  vkr = pChain->CallDown(pLayerName, &numExts, &exts[0]);

  if(vkr != VK_SUCCESS)
    return vkr;

  // filter the list of extensions to only the ones we support.

  // sort the reported extensions
  std::sort(exts.begin(), exts.end());

  std::vector<VkExtensionProperties> filtered;
  filtered.reserve(exts.size());

  FilterToSupportedExtensions(exts, filtered);

  if(pLayerName == NULL)
  {
    // now we can add extensions that we provide ourselves (note this isn't sorted, but we
    // don't have to sort the results, the sorting was just so we could filter optimally).
    filtered.insert(
        filtered.end(), &renderdocProvidedInstanceExtensions[0],
        &renderdocProvidedInstanceExtensions[0] + ARRAY_COUNT(renderdocProvidedInstanceExtensions));
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
  SERIALISE_ELEMENT(m_FrameCounter);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_FrameRecord.frameInfo.frameNumber = m_FrameCounter;
    RDCEraseEl(m_FrameRecord.frameInfo.stats);
  }

  return true;
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
  CACHE_THREAD_SERIALISER();
  ser.SetDrawChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  SERIALISE_ELEMENT_LOCAL(PresentedImage, GetResID(presentImage)).TypedAs("VkImage"_lit);

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedVulkan::FirstFrame()
{
  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(LayerDisp(m_Instance), NULL);

    m_AppControlledCapture = false;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  std::vector<VkImageMemoryBarrier> imgBarriers;

  {
    SCOPED_LOCK(m_ImageLayoutsLock);    // not needed on replay, but harmless also
    GetResourceManager()->SerialiseImageStates(ser, m_ImageLayouts, imgBarriers);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if(IsLoading(m_State))
    {
      // for the first load, promote any PREINITIALIZED images to GENERAL here since we treat
      // PREINIT as if it was GENERAL.
      for(auto it = m_ImageLayouts.begin(); it != m_ImageLayouts.end(); ++it)
      {
        for(auto stit = it->second.subresourceStates.begin();
            stit != it->second.subresourceStates.end(); ++stit)
        {
          if(stit->newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED &&
             GetResourceManager()->HasCurrentResource(it->first))
          {
            VkImage img = GetResourceManager()->GetCurrentHandle<VkImage>(it->first);

            {
              VkImageMemoryBarrier barrier = {};

              barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
              barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
              barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
              barrier.srcQueueFamilyIndex = m_QueueFamilyIdx;
              barrier.dstQueueFamilyIndex = m_QueueFamilyIdx;
              barrier.image = Unwrap(img);
              barrier.subresourceRange = stit->subresourceRange;

              imgBarriers.push_back(barrier);
            }
          }
        }
      }
    }

    if(!imgBarriers.empty())
    {
      for(size_t i = 0; i < imgBarriers.size(); i++)
      {
        // sanitise the layouts before passing to Vulkan
        if(!IsLoading(m_State))
          SanitiseOldImageLayout(imgBarriers[i].oldLayout);
        SanitiseNewImageLayout(imgBarriers[i].newLayout);

        imgBarriers[i].srcAccessMask = MakeAccessMask(imgBarriers[i].oldLayout);
        imgBarriers[i].dstAccessMask = MakeAccessMask(imgBarriers[i].newLayout);
      }

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      for(size_t i = 0; i < imgBarriers.size(); i++)
      {
        VkCommandBuffer cmd = GetNextCmd();

        VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

        ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), src_stages, dest_stages, false, 0, NULL, 0,
                                         NULL, 1, &imgBarriers[i]);

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        SubmitCmds();
      }
#else
      VkCommandBuffer cmd = GetNextCmd();

      VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

      ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), src_stages, dest_stages, false, 0, NULL, 0,
                                       NULL, (uint32_t)imgBarriers.size(), &imgBarriers[0]);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SubmitCmds();
#endif
    }
    // don't need to flush here
  }

  return true;
}

void WrappedVulkan::StartFrameCapture(void *dev, void *wnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  m_AppControlledCapture = true;

  m_SubmitCounter = 0;

  m_FrameCounter = RDCMAX((uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FrameDescription frame;
  frame.frameNumber = m_FrameCounter;
  frame.captureTime = Timing::GetUnixTimestamp();
  RDCEraseEl(frame.stats);
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->ClearReferencedMemory();

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Instance), eFrameRef_Read);
  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Device), eFrameRef_Read);
  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Queue), eFrameRef_Read);

  std::map<ResourceId, FrameRefType> forced = GetForcedReferences();

  // Note we force read-before-write because this resource is implicitly untracked so we have no
  // way of knowing how it's used
  for(auto it = forced.begin(); it != forced.end(); ++it)
  {
    GetResourceManager()->MarkResourceFrameReferenced(it->first, eFrameRef_Read);
    if(it->second != eFrameRef_Read)
      GetResourceManager()->MarkResourceFrameReferenced(it->first, it->second);
  }

  // need to do all this atomically so that no other commands
  // will check to see if they need to markdirty or markpendingdirty
  // and go into the frame record.
  {
    SCOPED_LOCK(m_CapTransitionLock);

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
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      DoPipelineBarrier(cmd, 1, &memBarrier);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureBegin);

      Serialise_BeginCaptureFrame(ser);

      // need to hold onto this as it must come right after the capture chunk,
      // before any command buffers
      m_HeaderChunk = scope.Get();
    }

    m_State = CaptureState::ActiveCapturing;
  }

  RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedVulkan::EndFrameCapture(void *dev, void *wnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  VkSwapchainKHR swap = VK_NULL_HANDLE;

  if(wnd)
  {
    {
      SCOPED_LOCK(m_SwapLookupLock);
      auto it = m_SwapLookup.find(wnd);
      if(it != m_SwapLookup.end())
        swap = it->second;
    }

    if(swap == VK_NULL_HANDLE)
    {
      RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
      return false;
    }
  }

  RDCLOG("Finished capture, Frame %u", m_FrameCounter);

  VkImage backbuffer = VK_NULL_HANDLE;
  VkResourceRecord *swaprecord = NULL;

  if(swap != VK_NULL_HANDLE)
  {
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(swap), eFrameRef_Read);

    swaprecord = GetRecord(swap);
    RDCASSERT(swaprecord->swapInfo);

    const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

    backbuffer = swapInfo.images[swapInfo.lastPresent].im;

    // mark all images referenced as well
    for(size_t i = 0; i < swapInfo.images.size(); i++)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(swapInfo.images[i].im),
                                                        eFrameRef_Read);
  }
  else
  {
    // if a swapchain wasn't specified or found, use the last one presented
    swaprecord = GetResourceManager()->GetResourceRecord(m_LastSwap);

    if(swaprecord)
    {
      GetResourceManager()->MarkResourceFrameReferenced(swaprecord->GetResourceID(), eFrameRef_Read);
      RDCASSERT(swaprecord->swapInfo);

      const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

      backbuffer = swapInfo.images[swapInfo.lastPresent].im;

      // mark all images referenced as well
      for(size_t i = 0; i < swapInfo.images.size(); i++)
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(swapInfo.images[i].im),
                                                          eFrameRef_Read);
    }
  }

  // transition back to IDLE atomically
  {
    SCOPED_LOCK(m_CapTransitionLock);
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
  }

  // gather backbuffer screenshot
  const uint32_t maxSize = 2048;
  RenderDoc::FramePixels fp;

  if(swaprecord != NULL)
  {
    VkDevice device = GetDev();
    VkCommandBuffer cmd = GetNextCmd();

    const VkLayerDispatchTable *vt = ObjDisp(device);

    vt->DeviceWaitIdle(Unwrap(device));

    const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

    // since this happens during capture, we don't want to start serialising extra buffer creates,
    // so we manually create & then just wrap.
    VkBuffer readbackBuf = VK_NULL_HANDLE;

    VkResult vkr = VK_SUCCESS;

    // create readback buffer
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        GetByteSize(swapInfo.extent.width, swapInfo.extent.height, 1, swapInfo.format, 0),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };
    vt->CreateBuffer(Unwrap(device), &bufInfo, NULL, &readbackBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(device), readbackBuf);

    MemoryAllocation readbackMem =
        AllocateMemoryForResource(readbackBuf, MemoryScope::InitialContents, MemoryType::Readback);

    vkr = vt->BindBufferMemory(Unwrap(device), Unwrap(readbackBuf), Unwrap(readbackMem.mem),
                               readbackMem.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    // do image copy
    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    uint32_t rowPitch = GetByteSize(swapInfo.extent.width, 1, 1, swapInfo.format, 0);

    VkBufferImageCopy cpy = {
        0,
        0,
        0,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {
            0, 0, 0,
        },
        {swapInfo.extent.width, swapInfo.extent.height, 1},
    };

    uint32_t swapQueueIndex = m_ImageLayouts[GetResID(backbuffer)].queueFamilyIndex;

    VkImageMemoryBarrier bbBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapQueueIndex,
        m_QueueFamilyIdx,
        Unwrap(backbuffer),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    if(swapInfo.shared)
      bbBarrier.oldLayout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;

    DoPipelineBarrier(cmd, 1, &bbBarrier);

    if(swapQueueIndex != m_QueueFamilyIdx)
    {
      VkCommandBuffer extQCmd = GetExtQueueCmd(swapQueueIndex);

      vkr = vt->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    SubmitCmds();
    FlushQ();    // need to wait so we can readback

    if(swapQueueIndex != m_QueueFamilyIdx)
    {
      VkCommandBuffer extQCmd = GetExtQueueCmd(swapQueueIndex);

      vkr = vt->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      DoPipelineBarrier(extQCmd, 1, &bbBarrier);

      ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));

      SubmitAndFlushExtQueue(swapQueueIndex);
    }

    // map memory and readback
    byte *pData = NULL;
    vkr = vt->MapMemory(Unwrap(device), Unwrap(readbackMem.mem), readbackMem.offs, readbackMem.size,
                        0, (void **)&pData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
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
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vt->UnmapMemory(Unwrap(device), Unwrap(readbackMem.mem));

    // delete all
    vt->DestroyBuffer(Unwrap(device), Unwrap(readbackBuf), NULL);
    GetResourceManager()->ReleaseWrappedResource(readbackBuf);

    ResourceFormat fmt = MakeResourceFormat(swapInfo.format);
    fp.width = swapInfo.extent.width;
    fp.height = swapInfo.extent.height;
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
    GetResourceManager()->InsertImageRefs(ser);

    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);

      Serialise_CaptureScope(ser);
    }

    m_HeaderChunk->Write(ser);

    // don't need to lock access to m_CmdBufferRecords as we are no longer
    // in capframe (the transition is thread-protected) so nothing will be
    // pushed to the vector

    {
      RDCDEBUG("Flushing %u command buffer records to file serialiser",
               (uint32_t)m_CmdBufferRecords.size());

      std::map<int32_t, Chunk *> recordlist;

      // ensure all command buffer records within the frame evne if recorded before, but
      // otherwise order must be preserved (vs. queue submits and desc set updates)
      for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
      {
        m_CmdBufferRecords[i]->Insert(recordlist);

        RDCDEBUG("Adding %u chunks to file serialiser from command buffer %llu",
                 (uint32_t)recordlist.size(), m_CmdBufferRecords[i]->GetResourceID());
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

      RDCDEBUG("Done");
    }
  }

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  SAFE_DELETE(m_HeaderChunk);

  m_State = CaptureState::BackgroundCapturing;

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedMemory();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  FreeAllMemory(MemoryScope::InitialContents);

  return true;
}

bool WrappedVulkan::DiscardFrameCapture(void *dev, void *wnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  // transition back to IDLE atomically
  {
    SCOPED_LOCK(m_CapTransitionLock);

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

  SAFE_DELETE(m_HeaderChunk);

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  FreeAllMemory(MemoryScope::InitialContents);

  return true;
}

void WrappedVulkan::AdvanceFrame()
{
  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame
}

void WrappedVulkan::Present(void *dev, void *wnd)
{
  bool activeWindow = wnd == NULL || RenderDoc::Inst().IsActiveWindow(dev, wnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Vulkan, true);
  if(!activeWindow)
    return;

  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(dev, wnd);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(dev, wnd);

    m_AppControlledCapture = false;
  }
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

ReplayStatus WrappedVulkan::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  GetResourceManager()->SetState(m_State);

  if(sectionIdx < 0)
    return ReplayStatus::FileCorrupted;

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(reader->IsErrored())
  {
    delete reader;
    return ReplayStatus::FileIOFailed;
  }

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData.version = m_StructuredFile->version = m_SectionVersion;

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

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offsetStart = reader->GetOffset();

    VulkanChunk context = ser.ReadChunk<VulkanChunk>();

    chunkIdx++;

    if(reader->IsErrored())
      return ReplayStatus::APIDataCorrupted;

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(reader->IsErrored())
      return ReplayStatus::APIDataCorrupted;

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
      return m_FailedReplayStatus;

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
      m_FrameRecord.frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      m_FrameReader = new StreamReader(reader, frameDataSize);

      ReplayStatus status = ContextReplayLog(m_State, 0, 0, false);

      if(status != ReplayStatus::Succeeded)
        return status;
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if((SystemChunk)context == SystemChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
      break;
  }

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
  m_StructuredFile->Swap(m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = &m_StoredStructuredData;

  m_FrameRecord.frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  m_FrameRecord.frameInfo.compressedFileSize = rdc->GetSectionProperties(sectionIdx).compressedSize;
  m_FrameRecord.frameInfo.persistentSize = frameDataSize;
  m_FrameRecord.frameInfo.initDataSize =
      chunkInfos[(VulkanChunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_FrameRecord.frameInfo.persistentSize);

  // ensure the capture at least created a device and fetched a queue.
  if(!IsStructuredExporting(m_State))
  {
    RDCASSERT(m_Device != VK_NULL_HANDLE && m_Queue != VK_NULL_HANDLE &&
              m_InternalCmds.cmdpool != VK_NULL_HANDLE);

    // create indirect draw buffer
    m_IndirectBufferSize = AlignUp(m_IndirectBufferSize + 63, (size_t)64);

    m_IndirectBuffer.Create(this, GetDev(), m_IndirectBufferSize, 1, 0);

    m_IndirectCommandBuffer = GetNextCmd();

    // steal the command buffer out of the pending commands - we'll manage its lifetime ourselves
    m_InternalCmds.pendingcmds.pop_back();
  }

  FreeAllMemory(MemoryScope::IndirectReadback);

  return ReplayStatus::Succeeded;
}

ReplayStatus WrappedVulkan::ContextReplayLog(CaptureState readType, uint32_t startEventID,
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
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State));

    ser.GetStructuredFile().Swap(*m_StructuredFile);

    m_StructuredFile = &ser.GetStructuredFile();
  }

  SystemChunk header = ser.ReadChunk<SystemChunk>();
  RDCASSERTEQUAL(header, SystemChunk::CaptureBegin);

  if(partial)
    ser.SkipCurrentChunk();
  else
    Serialise_BeginCaptureFrame(ser);

  ser.EndChunk();

  if(!IsStructuredExporting(m_State))
    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

  // apply initial contents here so that images are in the right layout
  // (not undefined)
  if(IsLoading(m_State))
  {
    ApplyInitialContents();

    SubmitCmds();
    FlushQ();
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
    m_RootDrawcallID = 1;
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
      return ReplayStatus::APIDataCorrupted;

    m_ChunkMetadata = ser.ChunkMetadata();

    m_LastCmdBufferID = ResourceId();

    bool success = ContextProcessChunk(ser, chunktype);

    ser.EndChunk();

    if(ser.GetReader()->IsErrored())
      return ReplayStatus::APIDataCorrupted;

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
      return m_FailedReplayStatus;

    RenderDoc::Inst().SetProgress(
        LoadProgress::FrameEventsRead,
        float(m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if((SystemChunk)chunktype == SystemChunk::CaptureEnd)
      break;

    // break out if we were only executing one event
    if(IsActiveReplaying(m_State) && startEventID == endEventID)
      break;

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

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().Swap(*prevFile);

  m_StructuredFile = prevFile;

  if(IsLoading(m_State))
  {
    GetFrameRecord().drawcallList = m_ParentDrawcall.Bake();

    SetupDrawcallPointers(m_Drawcalls, GetFrameRecord().drawcallList);

    m_ParentDrawcall.children.clear();
  }

  if(!IsStructuredExporting(m_State))
  {
    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

    // destroy any events we created for waiting on
    for(size_t i = 0; i < m_CleanupEvents.size(); i++)
      ObjDisp(GetDev())->DestroyEvent(Unwrap(GetDev()), m_CleanupEvents[i], NULL);

    for(const rdcpair<VkCommandPool, VkCommandBuffer> &rerecord : m_RerecordCmdList)
      vkFreeCommandBuffers(GetDev(), rerecord.first, 1, &rerecord.second);
  }

  // submit the indirect preparation command buffer, if we need to
  if(m_IndirectDraw)
  {
    VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        NULL,
        0,
        NULL,
        NULL,    // wait semaphores
        1,
        UnwrapPtr(m_IndirectCommandBuffer),    // command buffers
        0,
        NULL,    // signal semaphores
    };

    VkResult vkr = ObjDisp(m_Queue)->QueueSubmit(Unwrap(m_Queue), 1, &submitInfo, VK_NULL_HANDLE);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  m_IndirectDraw = false;

  m_CleanupEvents.clear();

  m_RerecordCmds.clear();
  m_RerecordCmdList.clear();

  return ReplayStatus::Succeeded;
}

void WrappedVulkan::ApplyInitialContents()
{
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

  VkResult vkr = VK_SUCCESS;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  DoPipelineBarrier(cmd, 1, &memBarrier);

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // sync all GPU work so we can also apply descriptor set initial contents
  SubmitCmds();
  FlushQ();

  // actually apply the initial contents here
  GetResourceManager()->ApplyInitialContents();

  // likewise again to make sure the initial states are all applied
  cmd = GetNextCmd();

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  DoPipelineBarrier(cmd, 1, &memBarrier);

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  SubmitCmds();
#endif
}

bool WrappedVulkan::ContextProcessChunk(ReadSerialiser &ser, VulkanChunk chunk)
{
  m_AddedDrawcall = false;

  bool success = false;

#if ENABLED(DISPLAY_RUNTIME_DEBUG_MESSAGES)
  // see the definition of DISPLAY_RUNTIME_DEBUG_MESSAGES for more information. During replay, add a
  // debug sink to catch any replay-time messages
  {
    ScopedDebugMessageSink sink(this);

    success = ProcessChunk(ser, chunk);

    if(IsActiveReplaying(m_State))
    {
      std::vector<DebugMessage> DebugMessages;
      DebugMessages.swap(sink.msgs);

      for(const DebugMessage &msg : DebugMessages)
        AddDebugMessage(msg);
    }
  }
#else
  success = ProcessChunk(ser, chunk);
#endif

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
      // also ignore, this just pops the drawcall stack
    }
    else
    {
      if(!m_AddedDrawcall)
        AddEvent();
    }
  }

  m_AddedDrawcall = false;

  return true;
}

bool WrappedVulkan::ProcessChunk(ReadSerialiser &ser, VulkanChunk chunk)
{
  switch(chunk)
  {
    case VulkanChunk::vkEnumeratePhysicalDevices:
      return Serialise_vkEnumeratePhysicalDevices(ser, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateDevice:
      return Serialise_vkCreateDevice(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetDeviceQueue:
      return Serialise_vkGetDeviceQueue(ser, VK_NULL_HANDLE, 0, 0, NULL);
      break;

    case VulkanChunk::vkAllocateMemory:
      return Serialise_vkAllocateMemory(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkUnmapMemory:
      return Serialise_vkUnmapMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkFlushMappedMemoryRanges:
      return Serialise_vkFlushMappedMemoryRanges(ser, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCreateCommandPool:
      return Serialise_vkCreateCommandPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkAllocateCommandBuffers:
      return Serialise_vkAllocateCommandBuffers(ser, VK_NULL_HANDLE, NULL, NULL);
      break;
    case VulkanChunk::vkCreateFramebuffer:
      return Serialise_vkCreateFramebuffer(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateRenderPass:
      return Serialise_vkCreateRenderPass(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateDescriptorPool:
      return Serialise_vkCreateDescriptorPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateDescriptorSetLayout:
      return Serialise_vkCreateDescriptorSetLayout(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateBuffer:
      return Serialise_vkCreateBuffer(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateBufferView:
      return Serialise_vkCreateBufferView(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateImage:
      return Serialise_vkCreateImage(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateImageView:
      return Serialise_vkCreateImageView(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateSampler:
      return Serialise_vkCreateSampler(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateShaderModule:
      return Serialise_vkCreateShaderModule(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreatePipelineLayout:
      return Serialise_vkCreatePipelineLayout(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreatePipelineCache:
      return Serialise_vkCreatePipelineCache(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateGraphicsPipelines:
      return Serialise_vkCreateGraphicsPipelines(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL,
                                                 NULL);
      break;
    case VulkanChunk::vkCreateComputePipelines:
      return Serialise_vkCreateComputePipelines(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL,
                                                NULL);
      break;
    case VulkanChunk::vkGetSwapchainImagesKHR:
      return Serialise_vkGetSwapchainImagesKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, NULL);
      break;

    case VulkanChunk::vkCreateSemaphore:
      return Serialise_vkCreateSemaphore(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateFence:
    // these chunks re-use serialisation from vkCreateFence, but have separate chunks for user
    // identification
    case VulkanChunk::vkRegisterDeviceEventEXT:
    case VulkanChunk::vkRegisterDisplayEventEXT:
      return Serialise_vkCreateFence(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetFenceStatus:
      return Serialise_vkGetFenceStatus(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkResetFences:
      return Serialise_vkResetFences(ser, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkWaitForFences:
      return Serialise_vkWaitForFences(ser, VK_NULL_HANDLE, 0, NULL, VK_FALSE, 0);
      break;

    case VulkanChunk::vkCreateEvent:
      return Serialise_vkCreateEvent(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetEventStatus:
      return Serialise_vkGetEventStatus(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkSetEvent:
      return Serialise_vkSetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkResetEvent:
      return Serialise_vkResetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkCreateQueryPool:
      return Serialise_vkCreateQueryPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;

    case VulkanChunk::vkAllocateDescriptorSets:
      return Serialise_vkAllocateDescriptorSets(ser, VK_NULL_HANDLE, NULL, NULL);
      break;
    case VulkanChunk::vkUpdateDescriptorSets:
      return Serialise_vkUpdateDescriptorSets(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);
      break;

    case VulkanChunk::vkBeginCommandBuffer:
      return Serialise_vkBeginCommandBuffer(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkEndCommandBuffer:
      return Serialise_vkEndCommandBuffer(ser, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkQueueWaitIdle: return Serialise_vkQueueWaitIdle(ser, VK_NULL_HANDLE); break;
    case VulkanChunk::vkDeviceWaitIdle:
      return Serialise_vkDeviceWaitIdle(ser, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkQueueSubmit:
      return Serialise_vkQueueSubmit(ser, VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkBindBufferMemory:
      return Serialise_vkBindBufferMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkBindImageMemory:
      return Serialise_vkBindImageMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;

    case VulkanChunk::vkQueueBindSparse:
      return Serialise_vkQueueBindSparse(ser, VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkCmdBeginRenderPass:
      return Serialise_vkCmdBeginRenderPass(ser, VK_NULL_HANDLE, NULL, VK_SUBPASS_CONTENTS_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdNextSubpass:
      return Serialise_vkCmdNextSubpass(ser, VK_NULL_HANDLE, VK_SUBPASS_CONTENTS_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdExecuteCommands:
      return Serialise_vkCmdExecuteCommands(ser, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCmdEndRenderPass:
      return Serialise_vkCmdEndRenderPass(ser, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkCmdBindPipeline:
      return Serialise_vkCmdBindPipeline(ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM,
                                         VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkCmdSetViewport:
      return Serialise_vkCmdSetViewport(ser, VK_NULL_HANDLE, 0, 0, NULL);
      break;
    case VulkanChunk::vkCmdSetScissor:
      return Serialise_vkCmdSetScissor(ser, VK_NULL_HANDLE, 0, 0, NULL);
      break;
    case VulkanChunk::vkCmdSetLineWidth:
      return Serialise_vkCmdSetLineWidth(ser, VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkCmdSetDepthBias:
      return Serialise_vkCmdSetDepthBias(ser, VK_NULL_HANDLE, 0.0f, 0.0f, 0.0f);
      break;
    case VulkanChunk::vkCmdSetBlendConstants:
      return Serialise_vkCmdSetBlendConstants(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdSetDepthBounds:
      return Serialise_vkCmdSetDepthBounds(ser, VK_NULL_HANDLE, 0.0f, 0.0f);
      break;
    case VulkanChunk::vkCmdSetStencilCompareMask:
      return Serialise_vkCmdSetStencilCompareMask(ser, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdSetStencilWriteMask:
      return Serialise_vkCmdSetStencilWriteMask(ser, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdSetStencilReference:
      return Serialise_vkCmdSetStencilReference(ser, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdBindDescriptorSets:
      return Serialise_vkCmdBindDescriptorSets(ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM,
                                               VK_NULL_HANDLE, 0, 0, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdBindIndexBuffer:
      return Serialise_vkCmdBindIndexBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0,
                                            VK_INDEX_TYPE_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdBindVertexBuffers:
      return Serialise_vkCmdBindVertexBuffers(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL);
      break;
    case VulkanChunk::vkCmdCopyBufferToImage:
      return Serialise_vkCmdCopyBufferToImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
      break;
    case VulkanChunk::vkCmdCopyImageToBuffer:
      return Serialise_vkCmdCopyImageToBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCmdCopyImage:
      return Serialise_vkCmdCopyImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                                      VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
      break;
    case VulkanChunk::vkCmdBlitImage:
      return Serialise_vkCmdBlitImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                                      VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL,
                                      VK_FILTER_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdResolveImage:
      return Serialise_vkCmdResolveImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                         VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE,
                                         VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
      break;
    case VulkanChunk::vkCmdCopyBuffer:
      return Serialise_vkCmdCopyBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCmdUpdateBuffer:
      return Serialise_vkCmdUpdateBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, NULL);
      break;
    case VulkanChunk::vkCmdFillBuffer:
      return Serialise_vkCmdFillBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdPushConstants:
      return Serialise_vkCmdPushConstants(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SHADER_STAGE_ALL,
                                          0, 0, NULL);
      break;
    case VulkanChunk::vkCmdClearColorImage:
      return Serialise_vkCmdClearColorImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdClearDepthStencilImage:
      return Serialise_vkCmdClearDepthStencilImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                   VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdClearAttachments:
      return Serialise_vkCmdClearAttachments(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdPipelineBarrier:
      return Serialise_vkCmdPipelineBarrier(ser, VK_NULL_HANDLE, 0, 0, VK_FALSE, 0, NULL, 0, NULL,
                                            0, NULL);
      break;
    case VulkanChunk::vkCmdWriteTimestamp:
      return Serialise_vkCmdWriteTimestamp(ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                           VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkCmdCopyQueryPoolResults:
      return Serialise_vkCmdCopyQueryPoolResults(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0,
                                                 VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdBeginQuery:
      return Serialise_vkCmdBeginQuery(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdEndQuery:
      return Serialise_vkCmdEndQuery(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkCmdResetQueryPool:
      return Serialise_vkCmdResetQueryPool(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
      break;

    case VulkanChunk::vkCmdSetEvent:
      return Serialise_vkCmdSetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
      break;
    case VulkanChunk::vkCmdResetEvent:
      return Serialise_vkCmdResetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
      break;
    case VulkanChunk::vkCmdWaitEvents:
      return Serialise_vkCmdWaitEvents(
          ser, VK_NULL_HANDLE, 0, NULL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, NULL, 0, NULL, 0, NULL);
      break;

    case VulkanChunk::vkCmdDraw: return Serialise_vkCmdDraw(ser, VK_NULL_HANDLE, 0, 0, 0, 0); break;
    case VulkanChunk::vkCmdDrawIndirect:
      return Serialise_vkCmdDrawIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDrawIndexed:
      return Serialise_vkCmdDrawIndexed(ser, VK_NULL_HANDLE, 0, 0, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDrawIndexedIndirect:
      return Serialise_vkCmdDrawIndexedIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDispatch:
      return Serialise_vkCmdDispatch(ser, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDispatchIndirect:
      return Serialise_vkCmdDispatchIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;

    case VulkanChunk::vkCmdDebugMarkerBeginEXT:
      return Serialise_vkCmdDebugMarkerBeginEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdDebugMarkerInsertEXT:
      return Serialise_vkCmdDebugMarkerInsertEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdDebugMarkerEndEXT:
      return Serialise_vkCmdDebugMarkerEndEXT(ser, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkDebugMarkerSetObjectNameEXT:
      return Serialise_vkDebugMarkerSetObjectNameEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::SetShaderDebugPath:
      return Serialise_SetShaderDebugPath(ser, VK_NULL_HANDLE, NULL);
      break;

    case VulkanChunk::vkCreateSwapchainKHR:
      return Serialise_vkCreateSwapchainKHR(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;

    case VulkanChunk::vkCmdIndirectSubCommand:
      // this is a fake chunk generated at runtime as part of indirect draws.
      // Just in case it gets exported and imported, completely ignore it.
      return true;
      break;

    case VulkanChunk::vkCmdPushDescriptorSetKHR:
      return Serialise_vkCmdPushDescriptorSetKHR(
          ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_GRAPHICS, VK_NULL_HANDLE, 0, 0, NULL);
      break;

    case VulkanChunk::vkCmdPushDescriptorSetWithTemplateKHR:
      return Serialise_vkCmdPushDescriptorSetWithTemplateKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                             VK_NULL_HANDLE, 0, NULL);
      break;

    case VulkanChunk::vkCreateDescriptorUpdateTemplate:
      return Serialise_vkCreateDescriptorUpdateTemplate(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkUpdateDescriptorSetWithTemplate:
      return Serialise_vkUpdateDescriptorSetWithTemplate(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                         VK_NULL_HANDLE, NULL);
      break;

    case VulkanChunk::vkBindBufferMemory2:
      return Serialise_vkBindBufferMemory2(ser, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkBindImageMemory2:
      return Serialise_vkBindImageMemory2(ser, VK_NULL_HANDLE, 0, NULL);
      break;

    case VulkanChunk::vkCmdWriteBufferMarkerAMD:
      return Serialise_vkCmdWriteBufferMarkerAMD(
          ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_NULL_HANDLE, 0, 0);
      break;

    case VulkanChunk::vkSetDebugUtilsObjectNameEXT:
      return Serialise_vkSetDebugUtilsObjectNameEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkQueueBeginDebugUtilsLabelEXT:
      return Serialise_vkQueueBeginDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkQueueEndDebugUtilsLabelEXT:
      return Serialise_vkQueueEndDebugUtilsLabelEXT(ser, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkQueueInsertDebugUtilsLabelEXT:
      return Serialise_vkQueueInsertDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdBeginDebugUtilsLabelEXT:
      return Serialise_vkCmdBeginDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdEndDebugUtilsLabelEXT:
      return Serialise_vkCmdEndDebugUtilsLabelEXT(ser, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkCmdInsertDebugUtilsLabelEXT:
      return Serialise_vkCmdInsertDebugUtilsLabelEXT(ser, VK_NULL_HANDLE, NULL);
      break;

    case VulkanChunk::vkCreateSamplerYcbcrConversion:
      return Serialise_vkCreateSamplerYcbcrConversion(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;

    case VulkanChunk::vkCmdSetDeviceMask:
      return Serialise_vkCmdSetDeviceMask(ser, VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkCmdDispatchBase:
      return Serialise_vkCmdDispatchBase(ser, VK_NULL_HANDLE, 0, 0, 0, 0, 0, 0);
      break;

    case VulkanChunk::vkGetDeviceQueue2:
      return Serialise_vkGetDeviceQueue2(ser, VK_NULL_HANDLE, NULL, NULL);
      break;

    case VulkanChunk::vkCmdDrawIndirectCountKHR:
      return Serialise_vkCmdDrawIndirectCountKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0,
                                                 VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDrawIndexedIndirectCountKHR:
      return Serialise_vkCmdDrawIndexedIndirectCountKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0,
                                                        VK_NULL_HANDLE, 0, 0, 0);
      break;

    case VulkanChunk::vkCreateRenderPass2KHR:
      return Serialise_vkCreateRenderPass2KHR(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
    case VulkanChunk::vkCmdBeginRenderPass2KHR:
      return Serialise_vkCmdBeginRenderPass2KHR(ser, VK_NULL_HANDLE, NULL, NULL);
    case VulkanChunk::vkCmdNextSubpass2KHR:
      return Serialise_vkCmdNextSubpass2KHR(ser, VK_NULL_HANDLE, NULL, NULL);
    case VulkanChunk::vkCmdEndRenderPass2KHR:
      return Serialise_vkCmdEndRenderPass2KHR(ser, VK_NULL_HANDLE, NULL);

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
      std::vector<MemRefInterval> data;
      return GetResourceManager()->Serialise_DeviceMemoryRefs(ser, data);
    }
    case VulkanChunk::vkResetQueryPoolEXT:
      return Serialise_vkResetQueryPoolEXT(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::ImageRefs:
    {
      std::vector<ImgRefsPair> data;
      return GetResourceManager()->Serialise_ImageRefs(ser, data);
    }
    default:
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

        if(IsLoading(m_State))
        {
          AddEvent();

          DrawcallDescription draw;
          draw.name = "vkQueuePresentKHR()";
          draw.flags |= DrawFlags::Present;

          draw.copyDestination = PresentedImage;

          AddDrawcall(draw, true);
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
  }

  return true;
}

void WrappedVulkan::AddFrameTerminator(uint64_t queueMarkerTag)
{
  VkCommandBuffer cmdBuffer = GetNextCmd();
  VkResult vkr = VK_SUCCESS;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmdBuffer)->BeginCommandBuffer(Unwrap(cmdBuffer), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

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

    SubmitCmds();
    FlushQ();
  }

  m_State = CaptureState::ActiveReplaying;

  VkMarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal: RenderDoc Replay %d (%d): %u->%u",
                                        (int)replayType, (int)partial, startEventID, endEventID));

  {
    if(!partial)
    {
      m_Partial[Primary].Reset();
      m_Partial[Secondary].Reset();
      m_RenderState = VulkanRenderState(this, &m_CreationInfo);
    }

    VkResult vkr = VK_SUCCESS;

    bool rpWasActive = false;
    // these are the image barriers to take the images from their current state to whatever is
    // needed for the loadRP. This is because when creating the loadRP we set initial = final =
    // attachment layout, to ensure it's in a known layout (and not transitioned from UNDEFINED or
    // something). We do a 'safe' transition from current layout to what's expected in the
    // attachment, which should always be a nop or overriding an UNDEFINED transition. Then we put
    // it back again afterwards.
    std::vector<VkImageMemoryBarrier> loadRPImgBarriers;

    m_RenderState.rpBarriers.clear();

    // we'll need our own command buffer if we're replaying just a subsection
    // of events within a single command buffer record - always if it's only
    // one drawcall, or if start event ID is > 0 we assume the outside code
    // has chosen a subsection that lies within a command buffer
    if(partial)
    {
      VkCommandBuffer cmd = m_OutsideCmdBuffer = GetNextCmd();

      // we'll explicitly submit this when we're ready
      RemovePendingCommandBuffer(cmd);

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      rpWasActive = m_Partial[Primary].renderPassActive;

      if(m_Partial[Primary].renderPassActive)
      {
        const DrawcallDescription *draw = GetDrawcall(endEventID);

        bool rpUnneeded = false;

        // if we're only replaying a draw, and it's not a drawcall or dispatch, don't try and bind
        // all the replay state as we don't know if it will be valid.
        if(replayType == eReplay_OnlyDraw)
        {
          if(!draw)
          {
            rpUnneeded = true;
          }
          else if(!(draw->flags & (DrawFlags::Drawcall | DrawFlags::Dispatch)))
          {
            rpUnneeded = true;
          }
        }

        // if a render pass was active, begin it and set up the partial replay state
        m_RenderState.BeginRenderPassAndApplyState(
            cmd, rpUnneeded ? VulkanRenderState::BindNone : VulkanRenderState::BindGraphics);
      }
      else
      {
        // even outside of render passes, we need to restore the state
        m_RenderState.BindPipeline(cmd, VulkanRenderState::BindCompute, false);
        m_RenderState.BindPipeline(cmd, VulkanRenderState::BindGraphics, false);
      }
    }

    ReplayStatus status = ReplayStatus::Succeeded;

    if(replayType == eReplay_Full)
      status = ContextReplayLog(m_State, startEventID, endEventID, partial);
    else if(replayType == eReplay_WithoutDraw)
      status = ContextReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    else if(replayType == eReplay_OnlyDraw)
      status = ContextReplayLog(m_State, endEventID, endEventID, partial);
    else
      RDCFATAL("Unexpected replay type");

    RDCASSERTEQUAL(status, ReplayStatus::Succeeded);

    if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    {
      VkCommandBuffer cmd = m_OutsideCmdBuffer;

      // end any active XFB
      if(!m_RenderState.xfbcounters.empty())
        m_RenderState.EndTransformFeedback(cmd);

      // end any active conditional rendering
      if(m_RenderState.IsConditionalRenderingEnabled())
        m_RenderState.EndConditionalRendering(cmd);

      // check if the render pass is active - it could have become active
      // even if it wasn't before (if the above event was a CmdBeginRenderPass).
      // If we began our own custom single-draw loadrp, and it was ended by a CmdEndRenderPass,
      // we need to reverse the virtual transitions we did above, as it won't happen otherwise
      if(m_Partial[Primary].renderPassActive)
        m_RenderState.EndRenderPass(cmd);

      // we might have replayed a CmdBeginRenderPass or CmdEndRenderPass,
      // but we want to keep the partial replay data state intact, so restore
      // whether or not a render pass was active.
      m_Partial[Primary].renderPassActive = rpWasActive;

      ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

      AddPendingCommandBuffer(cmd);

      SubmitCmds();

      m_OutsideCmdBuffer = VK_NULL_HANDLE;
    }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitCmds();
#endif
  }

  VkMarkerRegion::Set("!!!!RenderDoc Internal: Done replay");
}

template <typename SerialiserType>
void WrappedVulkan::Serialise_DebugMessages(SerialiserType &ser)
{
  std::vector<DebugMessage> DebugMessages;

  if(ser.IsWriting())
  {
    ScopedDebugMessageSink *sink = GetDebugMessageSink();
    if(sink)
      DebugMessages.swap(sink->msgs);

    // if we have the unique objects layer we can assume all objects have a unique ID, and replace
    // any text that looks like an object reference (0xHEX[NAME]).
    if(m_LayersEnabled[VkCheckLayer_unique_objects])
    {
      for(DebugMessage &msg : DebugMessages)
      {
        if(strstr(msg.description.c_str(), "0x"))
        {
          std::string desc = msg.description;

          size_t offs = desc.find("0x");
          while(offs != std::string::npos)
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

                // unique objects layer implies this is a unique search so we don't have to worry
                // about type aliases
                ResourceId id = GetResourceManager()->GetFirstIDForHandle(val);

                if(id != ResourceId())
                {
                  std::string idstr = ToStr(id);

                  desc.erase(offs, end - offs);

                  desc.insert(offs, idstr.c_str());

                  offs = desc.find("0x", offs + idstr.length());
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
  }

  SERIALISE_ELEMENT(DebugMessages);

  // hide empty sets of messages.
  if(ser.IsReading() && DebugMessages.empty())
    ser.Hidden();

  if(ser.IsReading() && IsLoading(m_State))
  {
    for(const DebugMessage &msg : DebugMessages)
      AddDebugMessage(msg);
  }
}

template void WrappedVulkan::Serialise_DebugMessages(WriteSerialiser &ser);
template void WrappedVulkan::Serialise_DebugMessages(ReadSerialiser &ser);

std::vector<DebugMessage> WrappedVulkan::GetDebugMessages()
{
  std::vector<DebugMessage> ret;
  ret.swap(m_DebugMessages);
  return ret;
}

void WrappedVulkan::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                    std::string d)
{
  DebugMessage msg;
  msg.eventId = 0;
  if(IsActiveReplaying(m_State))
  {
    // look up the EID this drawcall came from
    DrawcallUse use(m_CurChunkOffset, 0);
    auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

    if(it != m_DrawcallUses.end())
      msg.eventId = it->eventId;
    else
      RDCERR("Couldn't locate drawcall use for current chunk offset %llu", m_CurChunkOffset);
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
    m_EventMessages.push_back(msg);
  else
    m_DebugMessages.push_back(msg);
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
        // look up the EID this drawcall came from
        DrawcallUse use(m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

        if(it != m_DrawcallUses.end())
          msg.eventId = it->eventId;
      }

      sink->msgs.push_back(msg);
    }
  }

  {
    // ignore perf warnings
    if(category == MessageCategory::Performance)
      return false;

    // Non-linear image is aliased with linear buffer
    // Not an error, the validation layers complain at our whole-mem bufs
    if(strstr(pMessageId, "InvalidAliasing") || strstr(pMessage, "InvalidAliasing"))
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

  std::string msgid;

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

bool WrappedVulkan::HasNonMarkerEvents(ResourceId cmdBuffer)
{
  for(const APIEvent &ev : m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents)
  {
    VulkanChunk chunk = (VulkanChunk)m_StructuredFile->chunks[ev.chunkIndex]->metadata.chunkID;
    if(chunk != VulkanChunk::vkCmdDebugMarkerBeginEXT &&
       chunk != VulkanChunk::vkCmdDebugMarkerEndEXT &&
       chunk != VulkanChunk::vkCmdBeginDebugUtilsLabelEXT &&
       chunk != VulkanChunk::vkCmdEndDebugUtilsLabelEXT)
      return true;
  }

  return false;
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
      return m_BakedCmdBufferInfo[m_Partial[p].partialParent].curEventID <=
             m_LastEventID - m_Partial[p].baseEvent;
    }
  }

  // otherwise just check if we have a re-record command buffer for this, as then we're doing a full
  // re-record and replay
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

VkCommandBuffer WrappedVulkan::RerecordCmdBuf(ResourceId cmdid, PartialReplayIndex partialType)
{
  if(m_OutsideCmdBuffer != VK_NULL_HANDLE)
    return m_OutsideCmdBuffer;

  auto it = m_RerecordCmds.find(cmdid);

  if(it == m_RerecordCmds.end())
  {
    RDCERR("Didn't generate re-record command for %llu", cmdid);
    return NULL;
  }

  return it->second;
}

void WrappedVulkan::AddDrawcall(const DrawcallDescription &d, bool hasEvents)
{
  m_AddedDrawcall = true;

  DrawcallDescription draw = d;
  draw.eventId = m_LastCmdBufferID != ResourceId()
                     ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID
                     : m_RootEventID;
  draw.drawcallId = m_LastCmdBufferID != ResourceId()
                        ? m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount
                        : m_RootDrawcallID;

  for(int i = 0; i < 8; i++)
    draw.outputs[i] = ResourceId();

  draw.depthOut = ResourceId();

  draw.indexByteWidth = 0;
  draw.topology = Topology::Unknown;

  if(m_LastCmdBufferID != ResourceId())
  {
    ResourceId pipe = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.pipeline;
    if(pipe != ResourceId())
      draw.topology = MakePrimitiveTopology(m_CreationInfo.m_Pipeline[pipe].topology,
                                            m_CreationInfo.m_Pipeline[pipe].patchControlPoints);

    draw.indexByteWidth = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.idxWidth;

    ResourceId fb = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer;
    ResourceId rp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass;
    uint32_t sp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass;

    if(fb != ResourceId() && rp != ResourceId())
    {
      std::vector<VulkanCreationInfo::Framebuffer::Attachment> &atts =
          m_CreationInfo.m_Framebuffer[fb].attachments;

      RDCASSERT(sp < m_CreationInfo.m_RenderPass[rp].subpasses.size());

      std::vector<uint32_t> &colAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].colorAttachments;
      int32_t dsAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].depthstencilAttachment;

      RDCASSERT(colAtt.size() <= ARRAY_COUNT(draw.outputs));

      for(size_t i = 0; i < ARRAY_COUNT(draw.outputs) && i < colAtt.size(); i++)
      {
        if(colAtt[i] == VK_ATTACHMENT_UNUSED)
          continue;

        RDCASSERT(colAtt[i] < atts.size());
        draw.outputs[i] = GetResourceManager()->GetOriginalID(
            m_CreationInfo.m_ImageView[atts[colAtt[i]].view].image);
      }

      if(dsAtt != -1)
      {
        RDCASSERT(dsAtt < (int32_t)atts.size());
        draw.depthOut =
            GetResourceManager()->GetOriginalID(m_CreationInfo.m_ImageView[atts[dsAtt].view].image);
      }
    }
  }

  // markers don't increment drawcall ID
  DrawFlags MarkerMask = DrawFlags::SetMarker | DrawFlags::PushMarker | DrawFlags::PassBoundary;
  if(!(draw.flags & MarkerMask))
  {
    if(m_LastCmdBufferID != ResourceId())
      m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount++;
    else
      m_RootDrawcallID++;
  }

  if(hasEvents)
  {
    std::vector<APIEvent> &srcEvents = m_LastCmdBufferID != ResourceId()
                                           ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents
                                           : m_RootEvents;

    draw.events = srcEvents;
    srcEvents.clear();
  }

  // should have at least the root drawcall here, push this drawcall
  // onto the back's children list.
  if(!GetDrawcallStack().empty())
  {
    VulkanDrawcallTreeNode node(draw);

    node.resourceUsage.swap(m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage);

    if(m_LastCmdBufferID != ResourceId())
      AddUsage(node, m_BakedCmdBufferInfo[m_LastCmdBufferID].debugMessages);

    node.children.insert(node.children.begin(), draw.children.begin(), draw.children.end());
    GetDrawcallStack().back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost drawcall stack!");
}

void WrappedVulkan::AddUsage(VulkanDrawcallTreeNode &drawNode,
                             std::vector<DebugMessage> &debugMessages)
{
  DrawcallDescription &d = drawNode.draw;

  const BakedCmdBufferInfo::CmdBufferState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;
  VulkanCreationInfo &c = m_CreationInfo;
  uint32_t e = d.eventId;

  DrawFlags DrawMask = DrawFlags::Drawcall | DrawFlags::Dispatch;
  if(!(d.flags & DrawMask))
    return;

  //////////////////////////////
  // Vertex input

  if(d.flags & DrawFlags::Indexed && state.ibuffer != ResourceId())
    drawNode.resourceUsage.push_back(
        make_rdcpair(state.ibuffer, EventUsage(e, ResourceUsage::IndexBuffer)));

  for(size_t i = 0; i < state.vbuffers.size(); i++)
  {
    if(state.vbuffers[i] != ResourceId())
    {
      drawNode.resourceUsage.push_back(
          make_rdcpair(state.vbuffers[i], EventUsage(e, ResourceUsage::VertexBuffer)));
    }
  }

  for(uint32_t i = state.xfbfirst;
      i < state.xfbfirst + state.xfbcount && i < state.xfbbuffers.size(); i++)
  {
    if(state.xfbbuffers[i] != ResourceId())
    {
      drawNode.resourceUsage.push_back(
          make_rdcpair(state.xfbbuffers[i], EventUsage(e, ResourceUsage::StreamOut)));
    }
  }

  //////////////////////////////
  // Shaders

  for(int shad = 0; shad < 6; shad++)
  {
    VulkanCreationInfo::Pipeline::Shader &sh = c.m_Pipeline[state.pipeline].shaders[shad];
    if(sh.module == ResourceId())
      continue;

    ResourceId origPipe = GetResourceManager()->GetOriginalID(state.pipeline);
    ResourceId origShad = GetResourceManager()->GetOriginalID(sh.module);

    // 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)
    const std::vector<BakedCmdBufferInfo::CmdBufferState::DescriptorAndOffsets> &descSets =
        (shad == 5 ? state.computeDescSets : state.graphicsDescSets);

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
    msg.eventId = e;
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

        if(bindset >= (int32_t)descSets.size())
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

        // handled as part of the framebuffer attachments
        if(layout.bindings[bind].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
          continue;

        // we don't mark samplers with usage
        if(layout.bindings[bind].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
          continue;

        ResourceUsage usage = ResourceUsage(uint32_t(types[t].usage) + shad);

        if(bind >= (int32_t)descset.currentBindings.size())
        {
          msg.description = StringFormat::Fmt(
              "Shader referenced a bind %i in descriptor set %i that does not exist. Mismatched "
              "descriptor set?",
              bind, bindset);
          debugMessages.push_back(msg);
          continue;
        }

        for(uint32_t a = 0; a < layout.bindings[bind].descriptorCount; a++)
        {
          DescriptorSetBindingElement &slot = descset.currentBindings[bind][a];

          ResourceId id;

          switch(layout.bindings[bind].descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
              if(slot.imageInfo.imageView != VK_NULL_HANDLE)
                id = c.m_ImageView[GetResID(slot.imageInfo.imageView)].image;
              break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
              if(slot.texelBufferView != VK_NULL_HANDLE)
                id = c.m_BufferView[GetResID(slot.texelBufferView)].buffer;
              break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
              if(slot.bufferInfo.buffer != VK_NULL_HANDLE)
                id = GetResID(slot.bufferInfo.buffer);
              break;
            default: RDCERR("Unexpected type %d", layout.bindings[bind].descriptorType); break;
          }

          if(id != ResourceId())
            drawNode.resourceUsage.push_back(make_rdcpair(id, EventUsage(e, usage)));
        }
      }
    }
  }

  //////////////////////////////
  // Framebuffer/renderpass

  AddFramebufferUsage(drawNode, state.renderPass, state.framebuffer, state.subpass);
}

void WrappedVulkan::AddFramebufferUsage(VulkanDrawcallTreeNode &drawNode, ResourceId renderPass,
                                        ResourceId framebuffer, uint32_t subpass)
{
  VulkanCreationInfo &c = m_CreationInfo;
  uint32_t e = drawNode.draw.eventId;

  if(renderPass != ResourceId() && framebuffer != ResourceId())
  {
    const VulkanCreationInfo::RenderPass &rp = c.m_RenderPass[renderPass];
    const VulkanCreationInfo::Framebuffer &fb = c.m_Framebuffer[framebuffer];

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
        drawNode.resourceUsage.push_back(
            make_rdcpair(c.m_ImageView[fb.attachments[att].view].image,
                         EventUsage(e, ResourceUsage::InputTarget, fb.attachments[att].view)));
      }

      for(size_t i = 0; i < sub.colorAttachments.size(); i++)
      {
        uint32_t att = sub.colorAttachments[i];
        if(att == VK_ATTACHMENT_UNUSED)
          continue;
        drawNode.resourceUsage.push_back(
            make_rdcpair(c.m_ImageView[fb.attachments[att].view].image,
                         EventUsage(e, ResourceUsage::ColorTarget, fb.attachments[att].view)));
      }

      if(sub.depthstencilAttachment >= 0)
      {
        int32_t att = sub.depthstencilAttachment;
        drawNode.resourceUsage.push_back(make_rdcpair(
            c.m_ImageView[fb.attachments[att].view].image,
            EventUsage(e, ResourceUsage::DepthStencilTarget, fb.attachments[att].view)));
      }
    }
  }
}

void WrappedVulkan::AddFramebufferUsageAllChildren(VulkanDrawcallTreeNode &drawNode,
                                                   ResourceId renderPass, ResourceId framebuffer,
                                                   uint32_t subpass)
{
  for(VulkanDrawcallTreeNode &c : drawNode.children)
    AddFramebufferUsageAllChildren(c, renderPass, framebuffer, subpass);

  AddFramebufferUsage(drawNode, renderPass, framebuffer, subpass);
}

void WrappedVulkan::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventId = m_LastCmdBufferID != ResourceId()
                         ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID
                         : m_RootEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  apievent.callstack = m_ChunkMetadata.callstack;

  for(size_t i = 0; i < m_EventMessages.size(); i++)
    m_EventMessages[i].eventId = apievent.eventId;

  if(m_LastCmdBufferID != ResourceId())
  {
    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.push_back(apievent);

    std::vector<DebugMessage> &msgs = m_BakedCmdBufferInfo[m_LastCmdBufferID].debugMessages;

    msgs.insert(msgs.end(), m_EventMessages.begin(), m_EventMessages.end());
  }
  else
  {
    m_RootEvents.push_back(apievent);
    m_Events.resize(apievent.eventId + 1);
    m_Events[apievent.eventId] = apievent;

    m_DebugMessages.insert(m_DebugMessages.end(), m_EventMessages.begin(), m_EventMessages.end());
  }

  m_EventMessages.clear();
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

const DrawcallDescription *WrappedVulkan::GetDrawcall(uint32_t eventId)
{
  if(eventId >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventId];
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Validate supported extensions list", "[vulkan]")
{
  std::vector<VkExtensionProperties> unsorted;
  unsorted.insert(unsorted.begin(), &supportedExtensions[0],
                  &supportedExtensions[ARRAY_COUNT(supportedExtensions)]);

  std::vector<VkExtensionProperties> sorted = unsorted;

  std::sort(sorted.begin(), sorted.end());

  for(size_t i = 0; i < unsorted.size(); i++)
  {
    CHECK(std::string(unsorted[i].extensionName) == std::string(sorted[i].extensionName));
  }
}

#endif
