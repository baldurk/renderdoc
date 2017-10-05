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

#include "vk_core.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "vk_debug.h"

uint32_t VkInitParams::GetSerialiseSize()
{
  // misc bytes and fixed integer members
  size_t ret = 128;

  ret += AppName.size() + EngineName.size();

  for(const std::string &s : Layers)
    ret += 8 + s.size();

  for(const std::string &s : Extensions)
    ret += 8 + s.size();

  return (uint32_t)ret;
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
  if(RenderDoc::Inst().IsReplayApp())
  {
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

  m_DebugManager = NULL;

  m_Instance = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_Queue = VK_NULL_HANDLE;
  m_QueueFamilyIdx = 0;
  m_SupportedQueueFamily = 0;
  m_DbgMsgCallback = VK_NULL_HANDLE;

  m_HeaderChunk = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_FrameCaptureRecord->DataInSerialiser = false;
    m_FrameCaptureRecord->Length = 0;
    m_FrameCaptureRecord->SpecialResource = true;
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
    VkCommandBufferAllocateInfo cmdInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
                                           Unwrap(m_InternalCmds.cmdpool),
                                           VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
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

void WrappedVulkan::SubmitCmds()
{
  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  vector<VkCommandBuffer> cmds = m_InternalCmds.pendingcmds;
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = Unwrap(cmds[i]);

  VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      NULL,
      0,
      NULL,
      NULL,    // wait semaphores
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
    ObjDisp(m_Queue)->QueueWaitIdle(Unwrap(m_Queue));
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

uint32_t WrappedVulkan::HandlePreCallback(VkCommandBuffer commandBuffer, DrawFlags type,
                                          uint32_t multiDrawOffset)
{
  if(!m_DrawcallCallback)
    return 0;

  // look up the EID this drawcall came from
  DrawcallUse use(m_CurChunkOffset, 0);
  auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
  RDCASSERT(it != m_DrawcallUses.end());

  uint32_t eventID = it->eventID;

  RDCASSERT(eventID != 0);

  // handle all aliases of this drawcall as long as it's not a multidraw
  const DrawcallDescription *draw = GetDrawcall(eventID);

  if(draw == NULL || !(draw->flags & DrawFlags::MultiDraw))
  {
    ++it;
    while(it != m_DrawcallUses.end() && it->fileOffset == m_CurChunkOffset)
    {
      m_DrawcallCallback->AliasEvent(eventID, it->eventID);
      ++it;
    }
  }

  eventID += multiDrawOffset;

  if(type == DrawFlags::Drawcall)
    m_DrawcallCallback->PreDraw(eventID, commandBuffer);
  else if(type == DrawFlags::Dispatch)
    m_DrawcallCallback->PreDispatch(eventID, commandBuffer);
  else
    m_DrawcallCallback->PreMisc(eventID, type, commandBuffer);

  return eventID;
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

  uint32_t flags = 0;

  if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());

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
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_AMD_GCN_SHADER_EXTENSION_NAME, VK_AMD_GCN_SHADER_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_AMD_GPU_SHADER_HALF_FLOAT_EXTENSION_NAME, VK_AMD_GPU_SHADER_HALF_FLOAT_SPEC_VERSION,
    },
    {
        VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_EXTENSION_NAME, VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_AMD_SHADER_BALLOT_EXTENSION_NAME, VK_AMD_SHADER_BALLOT_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_AMD_SHADER_EXPLICIT_VERTEX_PARAMETER_EXTENSION_NAME,
        VK_AMD_SHADER_EXPLICIT_VERTEX_PARAMETER_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_AMD_SHADER_TRINARY_MINMAX_EXTENSION_NAME, VK_AMD_SHADER_TRINARY_MINMAX_SPEC_VERSION,
    },
#ifdef VK_EXT_acquire_xlib_display
    {
        VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME, VK_EXT_ACQUIRE_XLIB_DISPLAY_SPEC_VERSION,
    },
#endif
    {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_SPEC_VERSION,
    },
    {
        VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME, VK_EXT_DIRECT_MODE_DISPLAY_SPEC_VERSION,
    },
    {
        VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME, VK_EXT_DISPLAY_CONTROL_SPEC_VERSION,
    },
    {
        VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME, VK_EXT_DISPLAY_SURFACE_COUNTER_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME, VK_EXT_SHADER_SUBGROUP_BALLOT_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME, VK_EXT_SHADER_SUBGROUP_BALLOT_SPEC_VERSION,
    },
    {
        VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME, VK_EXT_VALIDATION_FLAGS_SPEC_VERSION,
    },
#ifdef VK_IMG_format_pvrtc
    {
        VK_IMG_FORMAT_PVRTC_EXTENSION_NAME, VK_IMG_FORMAT_PVRTC_SPEC_VERSION,
    },
#endif
#ifdef VK_KHR_android_surface
    {
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_SPEC_VERSION,
    },
#endif
    {
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, VK_KHR_DEDICATED_ALLOCATION_SPEC_VERSION,
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
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_GET_MEMORY_REQUIREMENTS_2_SPEC_VERSION,
    },
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION,
    },
    {
        VK_KHR_MAINTENANCE1_EXTENSION_NAME, VK_KHR_MAINTENANCE1_SPEC_VERSION,
    },
    {
        VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
        VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_SPEC_VERSION,
    },
    // this extension is 'free' - it just marks SPIR-V extension availability
    {
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_SPEC_VERSION,
    },
    {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION,
    },
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_SPEC_VERSION,
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
#ifdef VK_NV_win32_keyed_mutex
    {
        VK_NV_WIN32_KEYED_MUTEX_EXTENSION_NAME, VK_NV_WIN32_KEYED_MUTEX_SPEC_VERSION,
    },
#endif
};

// this is the list of extensions we provide - regardless of whether the ICD supports them
static const VkExtensionProperties renderdocProvidedExtensions[] = {
    {VK_EXT_DEBUG_MARKER_EXTENSION_NAME, VK_EXT_DEBUG_MARKER_SPEC_VERSION},
};

bool WrappedVulkan::IsSupportedExtension(const char *extName)
{
  for(size_t i = 0; i < ARRAY_COUNT(supportedExtensions); i++)
    if(!strcmp(supportedExtensions[i].extensionName, extName))
      return true;

  return false;
}

VkResult WrappedVulkan::FilterDeviceExtensionProperties(VkPhysicalDevice physDev,
                                                        uint32_t *pPropertyCount,
                                                        VkExtensionProperties *pProperties)
{
  VkResult vkr;

  // first fetch the list of extensions ourselves
  uint32_t numExts;
  vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), NULL, &numExts, NULL);

  if(vkr != VK_SUCCESS)
    return vkr;

  vector<VkExtensionProperties> exts(numExts);
  vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), NULL, &numExts,
                                                             &exts[0]);

  if(vkr != VK_SUCCESS)
    return vkr;

  // filter the list of extensions to only the ones we support.

  // sort the reported extensions
  std::sort(exts.begin(), exts.end());

  std::vector<VkExtensionProperties> filtered;
  filtered.reserve(exts.size());

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

  // now we can add extensions that we provide ourselves (note this isn't sorted, but we
  // don't have to sort the results, the sorting was just so we could filter optimally).
  filtered.insert(filtered.end(), &renderdocProvidedExtensions[0],
                  &renderdocProvidedExtensions[0] + ARRAY_COUNT(renderdocProvidedExtensions));

  return FillPropertyCountAndList(&filtered[0], (uint32_t)filtered.size(), pPropertyCount,
                                  pProperties);
}

VkResult WrappedVulkan::GetProvidedExtensionProperties(uint32_t *pPropertyCount,
                                                       VkExtensionProperties *pProperties)
{
  return FillPropertyCountAndList(renderdocProvidedExtensions,
                                  (uint32_t)ARRAY_COUNT(renderdocProvidedExtensions),
                                  pPropertyCount, pProperties);
}
template <typename SerialiserType>
void WrappedVulkan::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT(m_FrameCounter);

  if(IsReplayingAndReading())
  {
    m_FrameRecord.frameInfo.frameNumber = m_FrameCounter;
    RDCEraseEl(m_FrameRecord.frameInfo.stats);
  }
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
  CACHE_THREAD_SERIALISER();
  ser.SetDrawChunk();
  SCOPED_SERIALISE_CHUNK(VulkanChunk::CaptureEnd);

  SERIALISE_ELEMENT_LOCAL(PresentedImage, GetResID(presentImage));

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedVulkan::FirstFrame(VkSwapchainKHR swap)
{
  SwapchainInfo *swapdesc = GetRecord(swap)->swapInfo;

  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(LayerDisp(m_Instance), swapdesc ? swapdesc->wndHandle : NULL);

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

  if(IsReplayingAndReading() && !imgBarriers.empty())
  {
    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if(!imgBarriers.empty())
    {
      for(size_t i = 0; i < imgBarriers.size(); i++)
      {
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

  RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);

  m_AppControlledCapture = true;

  m_FrameCounter = RDCMAX(1 + (uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FrameDescription frame;
  frame.frameNumber = m_FrameCounter + 1;
  frame.captureTime = Timing::GetUnixTimestamp();
  RDCEraseEl(frame.stats);
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Instance), eFrameRef_Read);
  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Device), eFrameRef_Read);
  GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Queue), eFrameRef_Read);

  // need to do all this atomically so that no other commands
  // will check to see if they need to markdirty or markpendingdirty
  // and go into the frame record.
  {
    SCOPED_LOCK(m_CapTransitionLock);
    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::CaptureBegin);

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

  byte *thpixels = NULL;
  uint16_t thwidth = 0;
  uint16_t thheight = 0;

  // gather backbuffer screenshot
  const uint32_t maxSize = 2048;

  if(swap != VK_NULL_HANDLE)
  {
    VkDevice device = GetDev();
    VkCommandBuffer cmd = GetNextCmd();

    const VkLayerDispatchTable *vt = ObjDisp(device);

    vt->DeviceWaitIdle(Unwrap(device));

    const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

    // since these objects are very short lived (only this scope), we
    // don't wrap them.
    VkImage readbackIm = VK_NULL_HANDLE;
    VkDeviceMemory readbackMem = VK_NULL_HANDLE;

    VkResult vkr = VK_SUCCESS;

    // create identical image
    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        swapInfo.format,
        {swapInfo.extent.width, swapInfo.extent.height, 1},
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_LINEAR,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    vt->CreateImage(Unwrap(device), &imInfo, NULL, &readbackIm);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    vt->GetImageMemoryRequirements(Unwrap(device), readbackIm, &mrq);

    VkImageSubresource subr = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout = {0};
    vt->GetImageSubresourceLayout(Unwrap(device), readbackIm, &subr, &layout);

    // allocate readback memory
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = vt->AllocateMemory(Unwrap(device), &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    vkr = vt->BindImageMemory(Unwrap(device), readbackIm, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    // do image copy
    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageCopy cpy = {
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},           {0, 0, 0},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},           {0, 0, 0},
        {imInfo.extent.width, imInfo.extent.height, 1},
    };

    VkImageMemoryBarrier bbBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        0,
        0,    // MULTIDEVICE - need to actually pick the right queue family here maybe?
        Unwrap(backbuffer),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    VkImageMemoryBarrier readBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        NULL,
                                        0,
                                        VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        readbackIm,    // was never wrapped
                                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &bbBarrier);
    DoPipelineBarrier(cmd, 1, &readBarrier);

    vt->CmdCopyImage(Unwrap(cmd), Unwrap(backbuffer), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     readbackIm, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

    // barrier to switch backbuffer back to present layout
    std::swap(bbBarrier.oldLayout, bbBarrier.newLayout);
    std::swap(bbBarrier.srcAccessMask, bbBarrier.dstAccessMask);

    readBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    readBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    readBarrier.oldLayout = readBarrier.newLayout;
    readBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    DoPipelineBarrier(cmd, 1, &bbBarrier);
    DoPipelineBarrier(cmd, 1, &readBarrier);

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    SubmitCmds();
    FlushQ();    // need to wait so we can readback

    // map memory and readback
    byte *pData = NULL;
    vkr = vt->MapMemory(Unwrap(device), readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&pData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    RDCASSERT(pData != NULL);

    // point sample info into raw buffer
    {
      ResourceFormat fmt = MakeResourceFormat(imInfo.format);

      byte *data = (byte *)pData;

      data += layout.offset;

      float widthf = float(imInfo.extent.width);
      float heightf = float(imInfo.extent.height);

      float aspect = widthf / heightf;

      thwidth = (uint16_t)RDCMIN(maxSize, imInfo.extent.width);
      thwidth &= ~0x7;    // align down to multiple of 8
      thheight = uint16_t(float(thwidth) / aspect);

      thpixels = new byte[3 * thwidth * thheight];

      uint32_t stride = fmt.compByteWidth * fmt.compCount;

      bool buf1010102 = false;
      bool buf565 = false, buf5551 = false;
      bool bufBGRA = (fmt.bgraOrder != false);

      switch(fmt.type)
      {
        case ResourceFormatType::R10G10B10A2:
          stride = 4;
          buf1010102 = true;
          break;
        case ResourceFormatType::R5G6B5:
          stride = 2;
          buf565 = true;
          break;
        case ResourceFormatType::R5G5B5A1:
          stride = 2;
          buf5551 = true;
          break;
        default: break;
      }

      byte *dst = thpixels;

      for(uint32_t y = 0; y < thheight; y++)
      {
        for(uint32_t x = 0; x < thwidth; x++)
        {
          float xf = float(x) / float(thwidth);
          float yf = float(y) / float(thheight);

          byte *src =
              &data[stride * uint32_t(xf * widthf) + layout.rowPitch * uint32_t(yf * heightf)];

          if(buf1010102)
          {
            uint32_t *src1010102 = (uint32_t *)src;
            Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
            dst[0] = (byte)(unorm.x * 255.0f);
            dst[1] = (byte)(unorm.y * 255.0f);
            dst[2] = (byte)(unorm.z * 255.0f);
          }
          else if(buf565)
          {
            uint16_t *src565 = (uint16_t *)src;
            Vec3f unorm = ConvertFromB5G6R5(*src565);
            dst[0] = (byte)(unorm.z * 255.0f);
            dst[1] = (byte)(unorm.y * 255.0f);
            dst[2] = (byte)(unorm.x * 255.0f);
          }
          else if(buf5551)
          {
            uint16_t *src5551 = (uint16_t *)src;
            Vec4f unorm = ConvertFromB5G5R5A1(*src5551);
            dst[0] = (byte)(unorm.z * 255.0f);
            dst[1] = (byte)(unorm.y * 255.0f);
            dst[2] = (byte)(unorm.x * 255.0f);
          }
          else if(bufBGRA)
          {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
          }
          else if(fmt.compByteWidth == 2)    // R16G16B16A16 backbuffer
          {
            uint16_t *src16 = (uint16_t *)src;

            float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
            float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
            float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

            if(linearR < 0.0031308f)
              dst[0] = byte(255.0f * (12.92f * linearR));
            else
              dst[0] = byte(255.0f * (1.055f * powf(linearR, 1.0f / 2.4f) - 0.055f));

            if(linearG < 0.0031308f)
              dst[1] = byte(255.0f * (12.92f * linearG));
            else
              dst[1] = byte(255.0f * (1.055f * powf(linearG, 1.0f / 2.4f) - 0.055f));

            if(linearB < 0.0031308f)
              dst[2] = byte(255.0f * (12.92f * linearB));
            else
              dst[2] = byte(255.0f * (1.055f * powf(linearB, 1.0f / 2.4f) - 0.055f));
          }
          else
          {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
          }

          dst += 3;
        }
      }
    }

    vt->UnmapMemory(Unwrap(device), readbackMem);

    // delete all
    vt->DestroyImage(Unwrap(device), readbackIm, NULL);
    vt->FreeMemory(Unwrap(device), readbackMem, NULL);
  }

  byte *jpgbuf = NULL;
  int len = thwidth * thheight;

  if(wnd)
  {
    jpgbuf = new byte[len];

    jpge::params p;
    p.m_quality = 80;

    bool success =
        jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

    if(!success)
    {
      RDCERR("Failed to compress to jpg");
      SAFE_DELETE_ARRAY(jpgbuf);
      thwidth = 0;
      thheight = 0;
    }
  }

  RDCFile *rdc = RenderDoc::Inst().CreateRDC(m_FrameCounter, jpgbuf, len, thwidth, thheight);

  SAFE_DELETE_ARRAY(jpgbuf);
  SAFE_DELETE_ARRAY(thpixels);

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

    {
      SCOPED_SERIALISE_CHUNK(VulkanChunk::CaptureScope, 16);

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

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
        it->second->Write(ser);

      RDCDEBUG("Done");
    }
  }

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_FrameCounter);

  SAFE_DELETE(m_HeaderChunk);

  m_State = CaptureState::BackgroundCapturing;

  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdBufferRecords.size(); i++)
    m_CmdBufferRecords[i]->Delete(GetResourceManager());

  m_CmdBufferRecords.clear();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  GetResourceManager()->FlushPendingDirty();

  return true;
}

void WrappedVulkan::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(reader->IsErrored())
    return;

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData.version = m_StructuredFile->version = m_SectionVersion;

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

    ProcessChunk(ser, context);

    ser.EndChunk();

    uint64_t offsetEnd = reader->GetOffset();

    RenderDoc::Inst().SetProgress(FileInitialRead, float(offsetEnd) / float(reader->GetSize()));

    if(context == VulkanChunk::CaptureScope)
    {
      m_FrameRecord.frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      m_FrameReader = new StreamReader(reader, frameDataSize);

      ContextReplayLog(m_State, 0, 0, false);
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if(context == VulkanChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
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
  m_StructuredFile->swap(m_StoredStructuredData);

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
  }
}

void WrappedVulkan::ContextReplayLog(CaptureState readType, uint32_t startEventID,
                                     uint32_t endEventID, bool partial)
{
  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  SDFile *prevFile = m_StructuredFile;

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, false);

    ser.GetStructuredFile().swap(*m_StructuredFile);

    m_StructuredFile = &ser.GetStructuredFile();
  }

  VulkanChunk header = ser.ReadChunk<VulkanChunk>();
  RDCASSERTEQUAL(header, VulkanChunk::CaptureBegin);

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
    m_RootEventID = ev.eventID;

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

    m_ChunkMetadata = ser.ChunkMetadata();

    m_LastCmdBufferID = ResourceId();

    ContextProcessChunk(ser, chunktype);

    ser.EndChunk();

    RenderDoc::Inst().SetProgress(
        FileInitialRead, float(m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if(chunktype == VulkanChunk::CaptureEnd)
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

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().swap(*prevFile);

  m_StructuredFile = prevFile;

  if(IsLoading(m_State))
  {
    GetFrameRecord().drawcallList = m_ParentDrawcall.Bake();

    DrawcallDescription *previous = NULL;
    SetupDrawcallPointers(&m_Drawcalls, GetFrameRecord().drawcallList, NULL, previous);

    struct SortEID
    {
      bool operator()(const APIEvent &a, const APIEvent &b) { return a.eventID < b.eventID; }
    };

    std::sort(m_Events.begin(), m_Events.end(), SortEID());
    m_ParentDrawcall.children.clear();
  }

  if(!IsStructuredExporting(m_State))
  {
    ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

    // destroy any events we created for waiting on
    for(size_t i = 0; i < m_CleanupEvents.size(); i++)
      ObjDisp(GetDev())->DestroyEvent(Unwrap(GetDev()), m_CleanupEvents[i], NULL);
  }

  m_CleanupEvents.clear();

  for(int p = 0; p < ePartialNum; p++)
  {
    if(m_Partial[p].resultPartialCmdBuffer != VK_NULL_HANDLE)
    {
      // deliberately call our own function, so this is destroyed as a wrapped object
      vkFreeCommandBuffers(m_Partial[p].partialDevice, m_Partial[p].resultPartialCmdPool, 1,
                           &m_Partial[p].resultPartialCmdBuffer);
      m_Partial[p].resultPartialCmdBuffer = VK_NULL_HANDLE;
    }
  }

  for(auto it = m_RerecordCmds.begin(); it != m_RerecordCmds.end(); ++it)
  {
    VkCommandBuffer cmd = it->second;

    // same as above (these are created in an identical way)
    vkFreeCommandBuffers(GetDev(), m_InternalCmds.cmdpool, 1, &cmd);
  }

  m_RerecordCmds.clear();
}

void WrappedVulkan::ApplyInitialContents()
{
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

void WrappedVulkan::ContextProcessChunk(ReadSerialiser &ser, VulkanChunk chunk)
{
  m_AddedDrawcall = false;

  ProcessChunk(ser, chunk);

  if(IsLoading(m_State))
  {
    if(chunk == VulkanChunk::vkCmdDebugMarkerInsertEXT)
    {
      // no push/pop necessary
    }
    else if(chunk == VulkanChunk::vkBeginCommandBuffer || chunk == VulkanChunk::vkEndCommandBuffer ||
            chunk == VulkanChunk::vkCmdDebugMarkerBeginEXT ||
            chunk == VulkanChunk::vkCmdDebugMarkerEndEXT)
    {
      // don't add these events - they will be handled when inserted in-line into queue submit
    }
    else
    {
      if(!m_AddedDrawcall)
        AddEvent();
    }
  }

  m_AddedDrawcall = false;
}

void WrappedVulkan::ProcessChunk(ReadSerialiser &ser, VulkanChunk chunk)
{
  switch(chunk)
  {
    case VulkanChunk::vkEnumeratePhysicalDevices:
      Serialise_vkEnumeratePhysicalDevices(ser, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateDevice:
      Serialise_vkCreateDevice(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetDeviceQueue:
      Serialise_vkGetDeviceQueue(ser, VK_NULL_HANDLE, 0, 0, NULL);
      break;

    case VulkanChunk::vkAllocateMemory:
      Serialise_vkAllocateMemory(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkUnmapMemory:
      Serialise_vkUnmapMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkFlushMappedMemoryRanges:
      Serialise_vkFlushMappedMemoryRanges(ser, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCreateCommandPool:
      Serialise_vkCreateCommandPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateFramebuffer:
      Serialise_vkCreateFramebuffer(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateRenderPass:
      Serialise_vkCreateRenderPass(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateDescriptorPool:
      Serialise_vkCreateDescriptorPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateDescriptorSetLayout:
      Serialise_vkCreateDescriptorSetLayout(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateBuffer:
      Serialise_vkCreateBuffer(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateBufferView:
      Serialise_vkCreateBufferView(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateImage:
      Serialise_vkCreateImage(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateImageView:
      Serialise_vkCreateImageView(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateSampler:
      Serialise_vkCreateSampler(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateShaderModule:
      Serialise_vkCreateShaderModule(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreatePipelineLayout:
      Serialise_vkCreatePipelineLayout(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreatePipelineCache:
      Serialise_vkCreatePipelineCache(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateGraphicsPipelines:
      Serialise_vkCreateGraphicsPipelines(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateComputePipelines:
      Serialise_vkCreateComputePipelines(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetSwapchainImagesKHR:
      Serialise_vkGetSwapchainImagesKHR(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, NULL);
      break;

    case VulkanChunk::vkCreateSemaphore:
      Serialise_vkCreateSemaphore(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkCreateFence:
    // these chunks re-use serialisation from vkCreateFence, but have separate chunks for user
    // identification
    case VulkanChunk::vkRegisterDeviceEventEXT:
    case VulkanChunk::vkRegisterDisplayEventEXT:
      Serialise_vkCreateFence(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetFenceStatus:
      Serialise_vkGetFenceStatus(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkResetFences: Serialise_vkResetFences(ser, VK_NULL_HANDLE, 0, NULL); break;
    case VulkanChunk::vkWaitForFences:
      Serialise_vkWaitForFences(ser, VK_NULL_HANDLE, 0, NULL, VK_FALSE, 0);
      break;

    case VulkanChunk::vkCreateEvent:
      Serialise_vkCreateEvent(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;
    case VulkanChunk::vkGetEventStatus:
      Serialise_vkGetEventStatus(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkSetEvent: Serialise_vkSetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE); break;
    case VulkanChunk::vkResetEvent:
      Serialise_vkResetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkCreateQueryPool:
      Serialise_vkCreateQueryPool(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;

    case VulkanChunk::vkAllocateDescriptorSets:
      Serialise_vkAllocateDescriptorSets(ser, VK_NULL_HANDLE, NULL, NULL);
      break;
    case VulkanChunk::vkUpdateDescriptorSets:
      Serialise_vkUpdateDescriptorSets(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);
      break;

    case VulkanChunk::vkBeginCommandBuffer:
      Serialise_vkBeginCommandBuffer(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkEndCommandBuffer: Serialise_vkEndCommandBuffer(ser, VK_NULL_HANDLE); break;

    case VulkanChunk::vkQueueWaitIdle: Serialise_vkQueueWaitIdle(ser, VK_NULL_HANDLE); break;
    case VulkanChunk::vkDeviceWaitIdle: Serialise_vkDeviceWaitIdle(ser, VK_NULL_HANDLE); break;

    case VulkanChunk::vkQueueSubmit:
      Serialise_vkQueueSubmit(ser, VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkBindBufferMemory:
      Serialise_vkBindBufferMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkBindImageMemory:
      Serialise_vkBindImageMemory(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;

    case VulkanChunk::vkQueueBindSparse:
      Serialise_vkQueueBindSparse(ser, VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
      break;

    case VulkanChunk::vkCmdBeginRenderPass:
      Serialise_vkCmdBeginRenderPass(ser, VK_NULL_HANDLE, NULL, VK_SUBPASS_CONTENTS_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdNextSubpass:
      Serialise_vkCmdNextSubpass(ser, VK_NULL_HANDLE, VK_SUBPASS_CONTENTS_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdExecuteCommands:
      Serialise_vkCmdExecuteCommands(ser, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCmdEndRenderPass: Serialise_vkCmdEndRenderPass(ser, VK_NULL_HANDLE); break;

    case VulkanChunk::vkCmdBindPipeline:
      Serialise_vkCmdBindPipeline(ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM,
                                  VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkCmdSetViewport:
      Serialise_vkCmdSetViewport(ser, VK_NULL_HANDLE, 0, 0, NULL);
      break;
    case VulkanChunk::vkCmdSetScissor:
      Serialise_vkCmdSetScissor(ser, VK_NULL_HANDLE, 0, 0, NULL);
      break;
    case VulkanChunk::vkCmdSetLineWidth: Serialise_vkCmdSetLineWidth(ser, VK_NULL_HANDLE, 0); break;
    case VulkanChunk::vkCmdSetDepthBias:
      Serialise_vkCmdSetDepthBias(ser, VK_NULL_HANDLE, 0.0f, 0.0f, 0.0f);
      break;
    case VulkanChunk::vkCmdSetBlendConstants:
      Serialise_vkCmdSetBlendConstants(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdSetDepthBounds:
      Serialise_vkCmdSetDepthBounds(ser, VK_NULL_HANDLE, 0.0f, 0.0f);
      break;
    case VulkanChunk::vkCmdSetStencilCompareMask:
      Serialise_vkCmdSetStencilCompareMask(ser, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdSetStencilWriteMask:
      Serialise_vkCmdSetStencilWriteMask(ser, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdSetStencilReference:
      Serialise_vkCmdSetStencilReference(ser, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdBindDescriptorSets:
      Serialise_vkCmdBindDescriptorSets(ser, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM,
                                        VK_NULL_HANDLE, 0, 0, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdBindIndexBuffer:
      Serialise_vkCmdBindIndexBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, VK_INDEX_TYPE_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdBindVertexBuffers:
      Serialise_vkCmdBindVertexBuffers(ser, VK_NULL_HANDLE, 0, 0, NULL, NULL);
      break;
    case VulkanChunk::vkCmdCopyBufferToImage:
      Serialise_vkCmdCopyBufferToImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                       VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
      break;
    case VulkanChunk::vkCmdCopyImageToBuffer:
      Serialise_vkCmdCopyImageToBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                       VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCmdCopyImage:
      Serialise_vkCmdCopyImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                               VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
      break;
    case VulkanChunk::vkCmdBlitImage:
      Serialise_vkCmdBlitImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                               VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL, VK_FILTER_MAX_ENUM);
      break;
    case VulkanChunk::vkCmdResolveImage:
      Serialise_vkCmdResolveImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                                  VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
      break;
    case VulkanChunk::vkCmdCopyBuffer:
      Serialise_vkCmdCopyBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
      break;
    case VulkanChunk::vkCmdUpdateBuffer:
      Serialise_vkCmdUpdateBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, NULL);
      break;
    case VulkanChunk::vkCmdFillBuffer:
      Serialise_vkCmdFillBuffer(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdPushConstants:
      Serialise_vkCmdPushConstants(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SHADER_STAGE_ALL, 0, 0,
                                   NULL);
      break;
    case VulkanChunk::vkCmdClearColorImage:
      Serialise_vkCmdClearColorImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM,
                                     NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdClearDepthStencilImage:
      Serialise_vkCmdClearDepthStencilImage(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdClearAttachments:
      Serialise_vkCmdClearAttachments(ser, VK_NULL_HANDLE, 0, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdPipelineBarrier:
      Serialise_vkCmdPipelineBarrier(ser, VK_NULL_HANDLE, 0, 0, VK_FALSE, 0, NULL, 0, NULL, 0, NULL);
      break;
    case VulkanChunk::vkCmdWriteTimestamp:
      Serialise_vkCmdWriteTimestamp(ser, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkCmdCopyQueryPoolResults:
      Serialise_vkCmdCopyQueryPoolResults(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_NULL_HANDLE,
                                          0, 0, 0);
      break;
    case VulkanChunk::vkCmdBeginQuery:
      Serialise_vkCmdBeginQuery(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
      break;
    case VulkanChunk::vkCmdEndQuery:
      Serialise_vkCmdEndQuery(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;
    case VulkanChunk::vkCmdResetQueryPool:
      Serialise_vkCmdResetQueryPool(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
      break;

    case VulkanChunk::vkCmdSetEvent:
      Serialise_vkCmdSetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
      break;
    case VulkanChunk::vkCmdResetEvent:
      Serialise_vkCmdResetEvent(ser, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
      break;
    case VulkanChunk::vkCmdWaitEvents:
      Serialise_vkCmdWaitEvents(ser, VK_NULL_HANDLE, 0, NULL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, NULL, 0, NULL, 0, NULL);
      break;

    case VulkanChunk::vkCmdDraw: Serialise_vkCmdDraw(ser, VK_NULL_HANDLE, 0, 0, 0, 0); break;
    case VulkanChunk::vkCmdDrawIndirect:
      Serialise_vkCmdDrawIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDrawIndexed:
      Serialise_vkCmdDrawIndexed(ser, VK_NULL_HANDLE, 0, 0, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDrawIndexedIndirect:
      Serialise_vkCmdDrawIndexedIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
      break;
    case VulkanChunk::vkCmdDispatch: Serialise_vkCmdDispatch(ser, VK_NULL_HANDLE, 0, 0, 0); break;
    case VulkanChunk::vkCmdDispatchIndirect:
      Serialise_vkCmdDispatchIndirect(ser, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
      break;

    case VulkanChunk::vkCmdDebugMarkerBeginEXT:
      Serialise_vkCmdDebugMarkerBeginEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdDebugMarkerInsertEXT:
      Serialise_vkCmdDebugMarkerInsertEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::vkCmdDebugMarkerEndEXT:
      Serialise_vkCmdDebugMarkerEndEXT(ser, VK_NULL_HANDLE);
      break;
    case VulkanChunk::vkDebugMarkerSetObjectNameEXT:
      Serialise_vkDebugMarkerSetObjectNameEXT(ser, VK_NULL_HANDLE, NULL);
      break;
    case VulkanChunk::SetShaderDebugPath:
      Serialise_SetShaderDebugPath(ser, VK_NULL_HANDLE, NULL);
      break;

    case VulkanChunk::vkCreateSwapchainKHR:
      Serialise_vkCreateSwapchainKHR(ser, VK_NULL_HANDLE, NULL, NULL, NULL);
      break;

    case VulkanChunk::CaptureScope: Serialise_CaptureScope(ser); break;
    case VulkanChunk::CaptureEnd:
    {
      SERIALISE_ELEMENT_LOCAL(PresentedImage, ResourceId());

      if(IsLoading(m_State))
      {
        AddEvent();

        DrawcallDescription draw;
        draw.name = "vkQueuePresentKHR()";
        draw.flags |= DrawFlags::Present;

        draw.copyDestination = PresentedImage;

        AddDrawcall(draw, true);
      }
      break;
    }
    default:
    {
      SystemChunk system = (SystemChunk)chunk;
      if(system == SystemChunk::DriverInit)
      {
        VkInitParams InitParams;
        SERIALISE_ELEMENT(InitParams);
      }
      else if(system == SystemChunk::InitialContentsList)
      {
        GetResourceManager()->CreateInitialContents(ser);
      }
      else if(system == SystemChunk::InitialContents)
      {
        Serialise_InitialState(ser, ResourceId(), NULL);
      }
      else if(system < SystemChunk::FirstDriverChunk)
      {
        RDCERR("Unexpected system chunk in capture data: %u", system);
        ser.SkipCurrentChunk();
      }
      else
      {
        RDCERR("Unrecognised Chunk type %d", chunk);
      }
      break;
    }
  }
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

    GetResourceManager()->ReleaseInFrameResources();
  }

  m_State = CaptureState::ActiveReplaying;

  VkMarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal: RenderDoc Replay %d (%d): %u->%u",
                                        (int)replayType, (int)partial, startEventID, endEventID));

  {
    if(!partial)
    {
      RDCASSERT(m_Partial[Primary].resultPartialCmdBuffer == VK_NULL_HANDLE);
      RDCASSERT(m_Partial[Secondary].resultPartialCmdBuffer == VK_NULL_HANDLE);
      m_Partial[Primary].Reset();
      m_Partial[Secondary].Reset();
      m_RenderState = VulkanRenderState(this, &m_CreationInfo);
    }

    VkResult vkr = VK_SUCCESS;

    bool rpWasActive = false;

    // we'll need our own command buffer if we're replaying just a subsection
    // of events within a single command buffer record - always if it's only
    // one drawcall, or if start event ID is > 0 we assume the outside code
    // has chosen a subsection that lies within a command buffer
    if(partial)
    {
      VkCommandBuffer cmd = m_Partial[Primary].outsideCmdBuffer = GetNextCmd();

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      rpWasActive = m_Partial[Primary].renderPassActive;

      if(m_Partial[Primary].renderPassActive)
      {
        // first apply implicit transitions to the right subpass
        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        // don't transition from undefined, or contents will be discarded, instead transition from
        // the current state.
        for(size_t i = 0; i < imgBarriers.size(); i++)
        {
          if(imgBarriers[i].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
          {
            // TODO find overlapping range and transition that instead
            imgBarriers[i].oldLayout =
                m_ImageLayouts[GetResourceManager()->GetNonDispWrapper(imgBarriers[i].image)->id]
                    .subresourceStates[0]
                    .newLayout;
          }
        }

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[GetResID(cmd)].imgbarriers,
                                             m_ImageLayouts, (uint32_t)imgBarriers.size(),
                                             &imgBarriers[0]);

        ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 0, NULL, 0, NULL,
                                         (uint32_t)imgBarriers.size(), &imgBarriers[0]);

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
      else if(m_RenderState.compute.pipeline != ResourceId())
      {
        // if we had a compute pipeline, need to bind that
        m_RenderState.BindPipeline(cmd, VulkanRenderState::BindCompute, false);
      }
    }

    if(replayType == eReplay_Full)
    {
      ContextReplayLog(m_State, startEventID, endEventID, partial);
    }
    else if(replayType == eReplay_WithoutDraw)
    {
      ContextReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    }
    else if(replayType == eReplay_OnlyDraw)
    {
      ContextReplayLog(m_State, endEventID, endEventID, partial);
    }
    else
      RDCFATAL("Unexpected replay type");

    if(m_Partial[Primary].outsideCmdBuffer != VK_NULL_HANDLE)
    {
      VkCommandBuffer cmd = m_Partial[Primary].outsideCmdBuffer;

      // check if the render pass is active - it could have become active
      // even if it wasn't before (if the above event was a CmdBeginRenderPass)
      if(m_Partial[Primary].renderPassActive)
        m_RenderState.EndRenderPass(cmd);

      // we might have replayed a CmdBeginRenderPass or CmdEndRenderPass,
      // but we want to keep the partial replay data state intact, so restore
      // whether or not a render pass was active.
      m_Partial[Primary].renderPassActive = rpWasActive;

      ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

      SubmitCmds();

      m_Partial[Primary].outsideCmdBuffer = VK_NULL_HANDLE;
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
  msg.eventID = 0;
  if(IsActiveReplaying(m_State))
  {
    // look up the EID this drawcall came from
    DrawcallUse use(m_CurChunkOffset, 0);
    auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
    RDCASSERT(it != m_DrawcallUses.end());

    msg.eventID = it->eventID;
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

VkBool32 WrappedVulkan::DebugCallback(VkDebugReportFlagsEXT flags,
                                      VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                      size_t location, int32_t messageCode,
                                      const char *pLayerPrefix, const char *pMessage)
{
  bool isDS = false, isMEM = false, isSC = false, isOBJ = false, isSWAP = false, isDL = false,
       isIMG = false, isPARAM = false;

  if(!strcmp(pLayerPrefix, "DS"))
    isDS = true;
  else if(!strcmp(pLayerPrefix, "MEM"))
    isMEM = true;
  else if(!strcmp(pLayerPrefix, "SC"))
    isSC = true;
  else if(!strcmp(pLayerPrefix, "OBJTRACK"))
    isOBJ = true;
  else if(!strcmp(pLayerPrefix, "SWAP_CHAIN") || !strcmp(pLayerPrefix, "Swapchain"))
    isSWAP = true;
  else if(!strcmp(pLayerPrefix, "DL"))
    isDL = true;
  else if(!strcmp(pLayerPrefix, "Image"))
    isIMG = true;
  else if(!strcmp(pLayerPrefix, "PARAMCHECK") || !strcmp(pLayerPrefix, "ParameterValidation"))
    isPARAM = true;

  if(IsCaptureMode(m_State))
  {
    ScopedDebugMessageSink *sink = GetDebugMessageSink();

    if(sink)
    {
      DebugMessage msg;

      msg.eventID = 0;
      msg.category = MessageCategory::Miscellaneous;
      msg.description = pMessage;
      msg.severity = MessageSeverity::Low;
      msg.messageID = messageCode;
      msg.source = MessageSource::API;

      if(flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        msg.severity = MessageSeverity::Info;
      else if(flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        msg.severity = MessageSeverity::Low;
      else if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        msg.severity = MessageSeverity::Medium;
      else if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        msg.severity = MessageSeverity::High;

      if(flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        msg.category = MessageCategory::Performance;
      else if(isDS)
        msg.category = MessageCategory::Execution;
      else if(isMEM)
        msg.category = MessageCategory::Resource_Manipulation;
      else if(isSC)
        msg.category = MessageCategory::Shaders;
      else if(isOBJ)
        msg.category = MessageCategory::State_Setting;
      else if(isSWAP)
        msg.category = MessageCategory::Miscellaneous;
      else if(isDL)
        msg.category = MessageCategory::Portability;
      else if(isIMG)
        msg.category = MessageCategory::State_Creation;
      else if(isPARAM)
        msg.category = MessageCategory::Miscellaneous;

      if(isIMG || isPARAM)
        msg.source = MessageSource::IncorrectAPIUse;

      sink->msgs.push_back(msg);
    }
  }

  {
    // All access mask/barrier messages.
    // These are just too spammy/false positive/unreliable to keep
    if(isDS && messageCode == 10)
      return false;

    // ignore perf warnings
    if(flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
      return false;

    // Memory is aliased between image and buffer
    // ignore memory aliasing warning - we make use of the memory in disjoint ways
    // and copy image data over separately, so our use is safe
    // no location set for this one, so ignore by code (maybe too coarse)
    if(isMEM && messageCode == 3)
      return false;

    RDCWARN("[%s:%u/%d] %s", pLayerPrefix, (uint32_t)location, messageCode, pMessage);
  }

  return false;
}

bool WrappedVulkan::ShouldRerecordCmd(ResourceId cmdid)
{
  if(m_Partial[Primary].outsideCmdBuffer != VK_NULL_HANDLE)
    return true;

  if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
    return true;

  return cmdid == m_Partial[Primary].partialParent || cmdid == m_Partial[Secondary].partialParent;
}

bool WrappedVulkan::InRerecordRange(ResourceId cmdid)
{
  if(m_Partial[Primary].outsideCmdBuffer != VK_NULL_HANDLE)
    return true;

  if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
    return true;

  for(int p = 0; p < ePartialNum; p++)
  {
    if(cmdid == m_Partial[p].partialParent)
    {
      return m_BakedCmdBufferInfo[m_Partial[p].partialParent].curEventID <=
             m_LastEventID - m_Partial[p].baseEvent;
    }
  }

  return false;
}

VkCommandBuffer WrappedVulkan::RerecordCmdBuf(ResourceId cmdid, PartialReplayIndex partialType)
{
  if(m_Partial[Primary].outsideCmdBuffer != VK_NULL_HANDLE)
    return m_Partial[Primary].outsideCmdBuffer;

  if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
  {
    auto it = m_RerecordCmds.find(cmdid);

    RDCASSERT(it != m_RerecordCmds.end());

    return it->second;
  }

  if(partialType != ePartialNum)
    return m_Partial[partialType].resultPartialCmdBuffer;

  for(int p = 0; p < ePartialNum; p++)
    if(cmdid == m_Partial[p].partialParent)
      return m_Partial[p].resultPartialCmdBuffer;

  RDCERR("Calling re-record for invalid command buffer id");

  return VK_NULL_HANDLE;
}

void WrappedVulkan::AddDrawcall(const DrawcallDescription &d, bool hasEvents)
{
  m_AddedDrawcall = true;

  DrawcallDescription draw = d;
  draw.eventID = m_LastCmdBufferID != ResourceId()
                     ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID
                     : m_RootEventID;
  draw.drawcallID = m_LastCmdBufferID != ResourceId()
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
      vector<VulkanCreationInfo::Framebuffer::Attachment> &atts =
          m_CreationInfo.m_Framebuffer[fb].attachments;

      RDCASSERT(sp < m_CreationInfo.m_RenderPass[rp].subpasses.size());

      vector<uint32_t> &colAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].colorAttachments;
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

  if(m_LastCmdBufferID != ResourceId())
    m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount++;
  else
    m_RootDrawcallID++;

  if(hasEvents)
  {
    vector<APIEvent> &srcEvents = m_LastCmdBufferID != ResourceId()
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

void WrappedVulkan::AddUsage(VulkanDrawcallTreeNode &drawNode, vector<DebugMessage> &debugMessages)
{
  DrawcallDescription &d = drawNode.draw;

  const BakedCmdBufferInfo::CmdBufferState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;
  VulkanCreationInfo &c = m_CreationInfo;
  uint32_t e = d.eventID;

  DrawFlags DrawMask = DrawFlags::Drawcall | DrawFlags::Dispatch;
  if(!(d.flags & DrawMask))
    return;

  //////////////////////////////
  // Vertex input

  if(d.flags & DrawFlags::UseIBuffer && state.ibuffer != ResourceId())
    drawNode.resourceUsage.push_back(
        std::make_pair(state.ibuffer, EventUsage(e, ResourceUsage::IndexBuffer)));

  for(size_t i = 0; i < state.vbuffers.size(); i++)
    drawNode.resourceUsage.push_back(
        std::make_pair(state.vbuffers[i], EventUsage(e, ResourceUsage::VertexBuffer)));

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
    const vector<BakedCmdBufferInfo::CmdBufferState::DescriptorAndOffsets> &descSets =
        (shad == 5 ? state.computeDescSets : state.graphicsDescSets);

    RDCASSERT(sh.mapping);

    struct ResUsageType
    {
      ResUsageType(rdcarray<BindpointMap> &a, ResourceUsage u) : bindmap(a), usage(u) {}
      rdcarray<BindpointMap> &bindmap;
      ResourceUsage usage;
    };

    ResUsageType types[] = {
        ResUsageType(sh.mapping->ReadOnlyResources, ResourceUsage::VS_Resource),
        ResUsageType(sh.mapping->ReadWriteResources, ResourceUsage::VS_RWResource),
        ResUsageType(sh.mapping->ConstantBlocks, ResourceUsage::VS_Constants),
    };

    DebugMessage msg;
    msg.eventID = e;
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
        if(t == 2 && !sh.refl->ConstantBlocks[i].bufferBacked)
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

        ResourceId origId = GetResourceManager()->GetOriginalID(descSets[bindset].descSet);
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
          DescriptorSetSlot &slot = descset.currentBindings[bind][a];

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

          drawNode.resourceUsage.push_back(std::make_pair(id, EventUsage(e, usage)));
        }
      }
    }
  }

  //////////////////////////////
  // Framebuffer/renderpass

  if(state.renderPass != ResourceId() && state.framebuffer != ResourceId())
  {
    VulkanCreationInfo::RenderPass &rp = c.m_RenderPass[state.renderPass];
    VulkanCreationInfo::Framebuffer &fb = c.m_Framebuffer[state.framebuffer];

    RDCASSERT(state.subpass < rp.subpasses.size());

    for(size_t i = 0; i < rp.subpasses[state.subpass].inputAttachments.size(); i++)
    {
      uint32_t att = rp.subpasses[state.subpass].inputAttachments[i];
      if(att == VK_ATTACHMENT_UNUSED)
        continue;
      drawNode.resourceUsage.push_back(
          std::make_pair(c.m_ImageView[fb.attachments[att].view].image,
                         EventUsage(e, ResourceUsage::InputTarget, fb.attachments[att].view)));
    }

    for(size_t i = 0; i < rp.subpasses[state.subpass].colorAttachments.size(); i++)
    {
      uint32_t att = rp.subpasses[state.subpass].colorAttachments[i];
      if(att == VK_ATTACHMENT_UNUSED)
        continue;
      drawNode.resourceUsage.push_back(
          std::make_pair(c.m_ImageView[fb.attachments[att].view].image,
                         EventUsage(e, ResourceUsage::ColorTarget, fb.attachments[att].view)));
    }

    if(rp.subpasses[state.subpass].depthstencilAttachment >= 0)
    {
      int32_t att = rp.subpasses[state.subpass].depthstencilAttachment;
      drawNode.resourceUsage.push_back(std::make_pair(
          c.m_ImageView[fb.attachments[att].view].image,
          EventUsage(e, ResourceUsage::DepthStencilTarget, fb.attachments[att].view)));
    }
  }
}

void WrappedVulkan::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_LastCmdBufferID != ResourceId()
                         ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID
                         : m_RootEventID;

  // TODO structured data?
  apievent.eventDesc = "TODO";

  apievent.callstack = m_ChunkMetadata.callstack;

  for(size_t i = 0; i < m_EventMessages.size(); i++)
    m_EventMessages[i].eventID = apievent.eventID;

  if(m_LastCmdBufferID != ResourceId())
  {
    m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.push_back(apievent);

    std::vector<DebugMessage> &msgs = m_BakedCmdBufferInfo[m_LastCmdBufferID].debugMessages;

    msgs.insert(msgs.end(), m_EventMessages.begin(), m_EventMessages.end());
  }
  else
  {
    m_RootEvents.push_back(apievent);
    m_Events.push_back(apievent);

    m_DebugMessages.insert(m_DebugMessages.end(), m_EventMessages.begin(), m_EventMessages.end());
  }

  m_EventMessages.clear();
}

const APIEvent &WrappedVulkan::GetEvent(uint32_t eventID)
{
  for(const APIEvent &e : m_Events)
  {
    if(e.eventID >= eventID)
      return e;
  }

  return m_Events.back();
}

const DrawcallDescription *WrappedVulkan::GetDrawcall(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
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