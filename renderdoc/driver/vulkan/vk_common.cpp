/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

#include "vk_common.h"
#include "vk_core.h"
#include "vk_manager.h"
#include "vk_resources.h"

// utility struct for firing one-shot command buffers to begin/end markers
struct ScopedCommandBuffer
{
  ScopedCommandBuffer(VkCommandBuffer cmdbuf, WrappedVulkan *vk)
  {
    core = vk;
    cmd = cmdbuf;
    local = (cmd == VK_NULL_HANDLE);

    if(local)
    {
      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      cmd = vk->GetNextCmd();

      VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }
  ~ScopedCommandBuffer()
  {
    VkResult vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    core->SubmitCmds();
  }

  WrappedVulkan *core;
  VkCommandBuffer cmd;
  bool local;
};

WrappedVulkan *VkMarkerRegion::vk = NULL;

VkMarkerRegion::VkMarkerRegion(const std::string &marker, VkCommandBuffer cmd)
{
  if(cmd == VK_NULL_HANDLE)
  {
    RDCERR("Cannot auto-allocate a command buffer for a scoped VkMarkerRegion");
    return;
  }

  cmdbuf = cmd;
  Begin(marker, cmd);
}

VkMarkerRegion::~VkMarkerRegion()
{
  if(cmdbuf)
    End(cmdbuf);
}

void VkMarkerRegion::Begin(const std::string &marker, VkCommandBuffer cmd)
{
  if(!vk)
    return;

  // check for presence of the marker extension
  if(!ObjDisp(vk->GetDev())->CmdDebugMarkerBeginEXT)
    return;

  ScopedCommandBuffer scope(cmd, vk);

  VkDebugMarkerMarkerInfoEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
  markerInfo.pMarkerName = marker.c_str();
  ObjDisp(scope.cmd)->CmdDebugMarkerBeginEXT(Unwrap(scope.cmd), &markerInfo);
}

void VkMarkerRegion::Set(const std::string &marker, VkCommandBuffer cmd)
{
  // check for presence of the marker extension
  if(!ObjDisp(vk->GetDev())->CmdDebugMarkerBeginEXT)
    return;

  ScopedCommandBuffer scope(cmd, vk);

  VkDebugMarkerMarkerInfoEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
  markerInfo.pMarkerName = marker.c_str();
  ObjDisp(scope.cmd)->CmdDebugMarkerInsertEXT(Unwrap(scope.cmd), &markerInfo);
}

void VkMarkerRegion::End(VkCommandBuffer cmd)
{
  // check for presence of the marker extension
  if(!ObjDisp(vk->GetDev())->CmdDebugMarkerBeginEXT)
    return;

  ScopedCommandBuffer scope(cmd, vk);

  ObjDisp(scope.cmd)->CmdDebugMarkerEndEXT(Unwrap(scope.cmd));
}

void GPUBuffer::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t ringSize,
                       uint32_t flags)
{
  m_pDriver = driver;
  device = dev;

  align = (VkDeviceSize)driver->GetDeviceProps().limits.minUniformBufferOffsetAlignment;

  sz = size;
  // offset must be aligned, so ensure we have at least ringSize
  // copies accounting for that
  totalsize = ringSize == 1 ? size : AlignUp(size, align) * ringSize;
  curoffset = 0;

  ringCount = ringSize;

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, totalsize, 0,
  };

  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  if(flags & eGPUBufferVBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if(flags & eGPUBufferIBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  if(flags & eGPUBufferSSBO)
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if(flags & eGPUBufferIndirectBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

  VkResult vkr = driver->vkCreateBuffer(dev, &bufInfo, NULL, &buf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {};
  driver->vkGetBufferMemoryRequirements(dev, buf, &mrq);

  VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size, 0};

  if(flags & eGPUBufferReadback)
    allocInfo.memoryTypeIndex = driver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  else if(flags & eGPUBufferGPULocal)
    allocInfo.memoryTypeIndex = driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits);
  else
    allocInfo.memoryTypeIndex = driver->GetUploadMemoryIndex(mrq.memoryTypeBits);

  vkr = driver->vkAllocateMemory(dev, &allocInfo, NULL, &mem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = driver->vkBindBufferMemory(dev, buf, mem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void GPUBuffer::FillDescriptor(VkDescriptorBufferInfo &desc)
{
  desc.buffer = Unwrap(buf);
  desc.offset = 0;
  desc.range = sz;
}

void GPUBuffer::Destroy()
{
  if(device != VK_NULL_HANDLE)
  {
    m_pDriver->vkDestroyBuffer(device, buf, NULL);
    m_pDriver->vkFreeMemory(device, mem, NULL);
  }
}

void *GPUBuffer::Map(uint32_t *bindoffset, VkDeviceSize usedsize)
{
  VkDeviceSize offset = bindoffset ? curoffset : 0;
  VkDeviceSize size = usedsize > 0 ? usedsize : sz;

  // wrap around the ring, assuming the ring is large enough
  // that this memory is now free
  if(offset + sz > totalsize)
    offset = 0;
  RDCASSERT(offset + sz <= totalsize);

  // offset must be aligned
  curoffset = AlignUp(offset + size, align);

  if(bindoffset)
    *bindoffset = (uint32_t)offset;

  void *ptr = NULL;
  VkResult vkr = m_pDriver->vkMapMemory(device, mem, offset, size, 0, (void **)&ptr);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
  return ptr;
}

void *GPUBuffer::Map(VkDeviceSize &bindoffset, VkDeviceSize usedsize)
{
  uint32_t offs = 0;

  void *ret = Map(&offs, usedsize);

  bindoffset = offs;

  return ret;
}

void GPUBuffer::Unmap()
{
  m_pDriver->vkUnmapMemory(device, mem);
}

bool VkInitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // 0xD -> 0xE - fixed serialisation directly of size_t members in VkDescriptorUpdateTemplateEntry
  if(ver == 0xD)
    return true;

  // 0xC -> 0xD - supported multiple queues. This didn't cause a large change to the serialisation
  // but there were some slight inconsistencies that required a version bump
  if(ver == 0xC)
    return true;

  // 0xB -> 0xC - generally this is when we started serialising pNext chains that older RenderDoc
  // couldn't support. But we don't need any special backwards compatibiltiy code as it's just added
  // serialisation.
  if(ver == 0xB)
    return true;

  return false;
}

// utility function for when we're modifying one struct in a pNext chain, this
// lets us just copy across a struct unmodified into some temporary memory and
// append it onto a pNext chain we're building
template <typename VkStruct>
void CopyNextChainedStruct(byte *&tempMem, const VkGenericStruct *nextInput,
                           VkGenericStruct *&nextChainTail)
{
  const VkStruct *instruct = (const VkStruct *)nextInput;
  VkStruct *outstruct = (VkStruct *)tempMem;

  tempMem = (byte *)(outstruct + 1);

  // copy the struct, nothing to unwrap
  *outstruct = *instruct;

  // default to NULL. It will be overwritten next time if there is a next object
  outstruct->pNext = NULL;

  // append this onto the chain
  nextChainTail->pNext = (const VkGenericStruct *)outstruct;
  nextChainTail = (VkGenericStruct *)outstruct;
}

// this is similar to the above function, but for use after we've modified a struct locally
// e.g. to unwrap some members or patch flags, etc.
template <typename VkStruct>
void AppendModifiedChainedStruct(byte *&tempMem, VkStruct *outputStruct,
                                 VkGenericStruct *&nextChainTail)
{
  tempMem = (byte *)(outputStruct + 1);

  // default to NULL. It will be overwritten in the next step if there is a next object
  outputStruct->pNext = NULL;

  // append this onto the chain
  nextChainTail->pNext = (const VkGenericStruct *)outputStruct;
  nextChainTail = (VkGenericStruct *)outputStruct;
}

size_t GetNextPatchSize(const void *pNext)
{
  const VkGenericStruct *next = (const VkGenericStruct *)pNext;
  size_t memSize = 0;

  while(next)
  {
    // VkMemoryAllocateInfo
    if(next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV)
      memSize += sizeof(VkDedicatedAllocationMemoryAllocateInfoNV);
    else if(next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO)
      memSize += sizeof(VkMemoryDedicatedAllocateInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)
      memSize += sizeof(VkMemoryAllocateFlagsInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV)
      memSize += sizeof(VkExportMemoryAllocateInfoNV);
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO)
      memSize += sizeof(VkExportMemoryAllocateInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR)
      memSize += sizeof(VkImportMemoryFdInfoKHR);

#ifdef VK_NV_external_memory_win32
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
      memSize += sizeof(VkExportMemoryWin32HandleInfoNV);
    else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
      memSize += sizeof(VkImportMemoryWin32HandleInfoNV);
#else
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
      RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
    else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
      RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
#endif

#ifdef VK_KHR_external_memory_win32
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
      memSize += sizeof(VkExportMemoryWin32HandleInfoKHR);
    else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
      memSize += sizeof(VkImportMemoryWin32HandleInfoKHR);
#else
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
      RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
    else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
      RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif

    // VkSamplerCreateInfo
    else if(next->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO)
      memSize += sizeof(VkSamplerYcbcrConversionInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT)
      memSize += sizeof(VkSamplerReductionModeCreateInfoEXT);

    // VkSemaphoreCreateInfo
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO)
      memSize += sizeof(VkExportSemaphoreCreateInfoKHR);
#ifdef VK_KHR_external_semaphore_win32
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)
      memSize += sizeof(VkExportSemaphoreWin32HandleInfoKHR);
#else
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)
      RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif

    // VkFenceCreateInfo
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO)
      memSize += sizeof(VkExportFenceCreateInfoKHR);
#ifdef VK_KHR_external_fence_win32
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR)
      memSize += sizeof(VkExportFenceWin32HandleInfoKHR);
#else
    else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR)
      RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif

    // VkImageCreateInfo
    else if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO)
      memSize += sizeof(VkExternalMemoryImageCreateInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV)
      memSize += sizeof(VkExternalMemoryImageCreateInfoNV);
    else if(next->sType == VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR)
      memSize += sizeof(VkImageSwapchainCreateInfoKHR);
    else if(next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV)
      memSize += sizeof(VkDedicatedAllocationImageCreateInfoNV);

    // VkBufferCreateInfo
    else if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO)
      memSize += sizeof(VkExternalMemoryBufferCreateInfoKHR);
    else if(next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV)
      memSize += sizeof(VkDedicatedAllocationBufferCreateInfoNV);
    else if(next->sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO)
      memSize += sizeof(VkBindBufferMemoryDeviceGroupInfo);

    // VkBindImageMemoryInfo
    else if(next->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR)
      memSize += sizeof(VkBindImageMemorySwapchainInfoKHR);
    else if(next->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO)
      memSize += sizeof(VkBindImageMemoryDeviceGroupInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO)
      memSize += sizeof(VkBindImagePlaneMemoryInfo);

    // VkSwapchainCreateInfoKHR
    else if(next->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT)
      memSize += sizeof(VkSwapchainCounterCreateInfoEXT);
    else if(next->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR)
      memSize += sizeof(VkDeviceGroupSwapchainCreateInfoKHR);

    // VkDeviceQueueCreateInfo
    else if(next->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT)
      memSize += sizeof(VkDeviceQueueGlobalPriorityCreateInfoEXT);

    // VkSubmitInfo
    else if(next->sType == VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO)
      memSize += sizeof(VkProtectedSubmitInfo);
    else if(next->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO)
      memSize += sizeof(VkDeviceGroupSubmitInfo);

#if defined(VK_KHR_win32_keyed_mutex) || defined(VK_NV_win32_keyed_mutex)
    else if(next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||
            next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)
    {
      // the KHR and NV structs are identical
      memSize += sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR);

      VkWin32KeyedMutexAcquireReleaseInfoKHR *info = (VkWin32KeyedMutexAcquireReleaseInfoKHR *)next;
      memSize += info->acquireCount * sizeof(VkDeviceMemory);
      memSize += info->releaseCount * sizeof(VkDeviceMemory);
    }
#else
    else if(next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||
            next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)
    {
      RDCERR("Support for VK_KHR_win32_keyed_mutex / VK_NV_win32_keyed_mutex not compiled in");
    }
#endif

    // VkBindSparseInfo
    else if(next->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO)
      memSize += sizeof(VkDeviceGroupBindSparseInfo);

    // VkCommandBufferBeginInfo
    else if(next->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO)
      memSize += sizeof(VkDeviceGroupCommandBufferBeginInfo);

    // VkRenderPassBeginInfo
    else if(next->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO)
      memSize += sizeof(VkDeviceGroupRenderPassBeginInfo);

    next = next->pNext;
  }

  return memSize;
}

void UnwrapNextChain(CaptureState state, const char *structName, byte *&tempMem,
                     VkGenericStruct *infoStruct)
{
  // during capture, this walks the pNext chain and either copies structs that can be passed
  // straight through, or copies and modifies any with vulkan objects that need to be unwrapped.
  //
  // during replay, we do the same thing to prepare for dispatching to the driver, but we also strip
  // out any structs we don't want to replay - e.g. external memory. This means the data is
  // serialised and available for future use and for user inspection, but isn't replayed when not
  // necesary.

  VkGenericStruct *nextChainTail = infoStruct;
  const VkGenericStruct *nextInput = (const VkGenericStruct *)infoStruct->pNext;

  // start with an empty chain. Every call to AppendModifiedChainedStruct / CopyNextChainedStruct
  // pushes on a new entry, but if there's only one entry in the list and it's one we want to skip,
  // this needs to start at NULL.
  nextChainTail->pNext = NULL;
  while(nextInput)
  {
    if(nextInput->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV ||
       nextInput->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO)
    {
      // KHR and NV structs are identical
      const VkMemoryDedicatedAllocateInfo *dedicatedIn =
          (const VkMemoryDedicatedAllocateInfo *)nextInput;
      VkMemoryDedicatedAllocateInfo *dedicatedOut = (VkMemoryDedicatedAllocateInfo *)tempMem;

      // copy and unwrap the struct
      dedicatedOut->sType = dedicatedIn->sType;
      dedicatedOut->buffer = Unwrap(dedicatedIn->buffer);
      dedicatedOut->image = Unwrap(dedicatedIn->image);

      // we replay this
      AppendModifiedChainedStruct(tempMem, dedicatedOut, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)
    {
      CopyNextChainedStruct<VkMemoryAllocateFlagsInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportMemoryAllocateInfoNV>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportMemoryAllocateInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkImportMemoryFdInfoKHR>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
    {
#ifdef VK_KHR_external_memory_win32
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportMemoryWin32HandleInfoKHR>(tempMem, nextInput, nextChainTail);
#else
      RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
    {
#ifdef VK_KHR_external_memory_win32
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkImportMemoryWin32HandleInfoKHR>(tempMem, nextInput, nextChainTail);
#else
      RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
    {
#ifdef VK_NV_external_memory_win32
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportMemoryWin32HandleInfoNV>(tempMem, nextInput, nextChainTail);
#else
      RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
    {
#ifdef VK_NV_external_memory_win32
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkImportMemoryWin32HandleInfoNV>(tempMem, nextInput, nextChainTail);
#else
      RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO)
    {
      const VkSamplerYcbcrConversionInfo *ycbcrIn = (const VkSamplerYcbcrConversionInfo *)nextInput;
      VkSamplerYcbcrConversionInfo *ycbcrOut = (VkSamplerYcbcrConversionInfo *)tempMem;

      // copy and unwrap the struct
      ycbcrOut->sType = ycbcrIn->sType;
      ycbcrOut->conversion = Unwrap(ycbcrIn->conversion);

      AppendModifiedChainedStruct(tempMem, ycbcrOut, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT)
    {
      CopyNextChainedStruct<VkSamplerReductionModeCreateInfoEXT>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportSemaphoreCreateInfoKHR>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)
    {
#ifdef VK_KHR_external_semaphore_win32
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportSemaphoreWin32HandleInfoKHR>(tempMem, nextInput, nextChainTail);
#else
      RDCERR("Support for VK_KHR_external_semaphore_win32 not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportFenceCreateInfoKHR>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR)
    {
#ifdef VK_KHR_external_fence_win32

      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExportFenceWin32HandleInfoKHR>(tempMem, nextInput, nextChainTail);
#else
      RDCERR("Support for VK_KHR_external_fence_win32 not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExternalMemoryImageCreateInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExternalMemoryImageCreateInfoNV>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR)
    {
      if(IsCaptureMode(state))
      {
        const VkImageSwapchainCreateInfoKHR *swapIn =
            (const VkImageSwapchainCreateInfoKHR *)nextInput;
        VkImageSwapchainCreateInfoKHR *swapOut = (VkImageSwapchainCreateInfoKHR *)tempMem;

        // copy and unwrap the struct
        swapOut->sType = swapIn->sType;
        swapOut->swapchain = Unwrap(swapIn->swapchain);

        AppendModifiedChainedStruct(tempMem, swapOut, nextChainTail);
      }
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV)
    {
      CopyNextChainedStruct<VkDedicatedAllocationImageCreateInfoNV>(tempMem, nextInput,
                                                                    nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkExternalMemoryBufferCreateInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV)
    {
      CopyNextChainedStruct<VkDedicatedAllocationBufferCreateInfoNV>(tempMem, nextInput,
                                                                     nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO)
    {
      CopyNextChainedStruct<VkBindBufferMemoryDeviceGroupInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR)
    {
      if(IsCaptureMode(state))
      {
        const VkBindImageMemorySwapchainInfoKHR *swapIn =
            (const VkBindImageMemorySwapchainInfoKHR *)nextInput;
        VkBindImageMemorySwapchainInfoKHR *swapOut = (VkBindImageMemorySwapchainInfoKHR *)tempMem;

        // copy and unwrap the struct
        swapOut->sType = swapIn->sType;
        swapOut->swapchain = Unwrap(swapIn->swapchain);

        AppendModifiedChainedStruct(tempMem, swapOut, nextChainTail);
      }
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO)
    {
      CopyNextChainedStruct<VkBindImageMemoryDeviceGroupInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO)
    {
      CopyNextChainedStruct<VkBindImagePlaneMemoryInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkSwapchainCounterCreateInfoEXT>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR)
    {
      CopyNextChainedStruct<VkDeviceGroupSwapchainCreateInfoKHR>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT)
    {
      CopyNextChainedStruct<VkDeviceQueueGlobalPriorityCreateInfoEXT>(tempMem, nextInput,
                                                                      nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO)
    {
      if(IsCaptureMode(state))
        CopyNextChainedStruct<VkProtectedSubmitInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO)
    {
      CopyNextChainedStruct<VkDeviceGroupSubmitInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||
            nextInput->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)
    {
#if defined(VK_KHR_win32_keyed_mutex) || defined(VK_NV_win32_keyed_mutex)
      if(IsCaptureMode(state))
      {
        // KHR and NV structs are identical
        const VkWin32KeyedMutexAcquireReleaseInfoKHR *mutexIn =
            (const VkWin32KeyedMutexAcquireReleaseInfoKHR *)nextInput;
        VkWin32KeyedMutexAcquireReleaseInfoKHR *mutexOut =
            (VkWin32KeyedMutexAcquireReleaseInfoKHR *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, mutexOut, nextChainTail);

        // copy sType and pointers we don't need to patch
        mutexOut->sType = mutexIn->sType;
        mutexOut->acquireCount = mutexIn->acquireCount;
        mutexOut->pAcquireKeys = mutexIn->pAcquireKeys;
        mutexOut->pAcquireTimeouts = mutexIn->pAcquireTimeouts;
        mutexOut->releaseCount = mutexIn->releaseCount;
        mutexOut->pReleaseKeys = mutexIn->pReleaseKeys;

        // allocate unwrapped arrays
        VkDeviceMemory *unwrappedAcquires = (VkDeviceMemory *)tempMem;
        tempMem += sizeof(VkDeviceMemory) * mutexIn->acquireCount;
        VkDeviceMemory *unwrappedReleases = (VkDeviceMemory *)tempMem;
        tempMem += sizeof(VkDeviceMemory) * mutexIn->releaseCount;

        // unwrap the arrays
        for(uint32_t mem = 0; mem < mutexIn->acquireCount; mem++)
          unwrappedAcquires[mem] = Unwrap(mutexIn->pAcquireSyncs[mem]);
        for(uint32_t mem = 0; mem < mutexIn->releaseCount; mem++)
          unwrappedReleases[mem] = Unwrap(mutexIn->pReleaseSyncs[mem]);

        mutexOut->pAcquireSyncs = unwrappedAcquires;
        mutexOut->pReleaseSyncs = unwrappedReleases;
      }
#else
      RDCERR("Support for VK_KHR_win32_keyed_mutex / VK_NV_win32_keyed_mutex not compiled in");
#endif
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO)
    {
      CopyNextChainedStruct<VkDeviceGroupBindSparseInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO)
    {
      CopyNextChainedStruct<VkDeviceGroupCommandBufferBeginInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO)
    {
      CopyNextChainedStruct<VkDeviceGroupRenderPassBeginInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR)
    {
      CopyNextChainedStruct<VkImageFormatListCreateInfoKHR>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO)
    {
      CopyNextChainedStruct<VkRenderPassMultiviewCreateInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO)
    {
      CopyNextChainedStruct<VkImageViewUsageCreateInfo>(tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO)
    {
      CopyNextChainedStruct<VkRenderPassInputAttachmentAspectCreateInfo>(tempMem, nextInput,
                                                                         nextChainTail);
    }
    else if(nextInput->sType ==
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT)
    {
      CopyNextChainedStruct<VkPipelineRasterizationConservativeStateCreateInfoEXT>(
          tempMem, nextInput, nextChainTail);
    }
    else if(nextInput->sType ==
            VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO)
    {
      CopyNextChainedStruct<VkPipelineTessellationDomainOriginStateCreateInfo>(tempMem, nextInput,
                                                                               nextChainTail);
    }
    else if(nextInput->sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT)
    {
      CopyNextChainedStruct<VkPipelineVertexInputDivisorStateCreateInfoEXT>(tempMem, nextInput,
                                                                            nextChainTail);
    }
    else
    {
      RDCERR("unrecognised struct %d in %s pNext chain", nextInput->sType, structName);
      // can't patch this struct, have to just copy it and hope it's the last in the chain
      nextChainTail->pNext = nextInput;
    }

    nextInput = nextInput->pNext;
  }
}

VkAccessFlags MakeAccessMask(VkImageLayout layout)
{
  switch(layout)
  {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VkAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT);
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return VkAccessFlags(VK_ACCESS_HOST_WRITE_BIT);
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VkAccessFlags(VK_ACCESS_TRANSFER_READ_BIT);
    default: break;
  }

  return VkAccessFlags(0);
}

void ReplacePresentableImageLayout(VkImageLayout &layout)
{
  if(layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    layout = VK_IMAGE_LAYOUT_GENERAL;
}

int SampleCount(VkSampleCountFlagBits countFlag)
{
  switch(countFlag)
  {
    case VK_SAMPLE_COUNT_1_BIT: return 1;
    case VK_SAMPLE_COUNT_2_BIT: return 2;
    case VK_SAMPLE_COUNT_4_BIT: return 4;
    case VK_SAMPLE_COUNT_8_BIT: return 8;
    case VK_SAMPLE_COUNT_16_BIT: return 16;
    case VK_SAMPLE_COUNT_32_BIT: return 32;
    case VK_SAMPLE_COUNT_64_BIT: return 64;
    default: RDCERR("Unrecognised/not single flag %x", countFlag); break;
  }

  return 1;
}

int SampleIndex(VkSampleCountFlagBits countFlag)
{
  switch(countFlag)
  {
    case VK_SAMPLE_COUNT_1_BIT: return 0;
    case VK_SAMPLE_COUNT_2_BIT: return 1;
    case VK_SAMPLE_COUNT_4_BIT: return 2;
    case VK_SAMPLE_COUNT_8_BIT: return 3;
    case VK_SAMPLE_COUNT_16_BIT: return 4;
    case VK_SAMPLE_COUNT_32_BIT: return 5;
    case VK_SAMPLE_COUNT_64_BIT: return 6;
    default: RDCERR("Unrecognised/not single flag %x", countFlag); break;
  }

  return 0;
}

int StageIndex(VkShaderStageFlagBits stageFlag)
{
  switch(stageFlag)
  {
    case VK_SHADER_STAGE_VERTEX_BIT: return 0;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return 1;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return 2;
    case VK_SHADER_STAGE_GEOMETRY_BIT: return 3;
    case VK_SHADER_STAGE_FRAGMENT_BIT: return 4;
    case VK_SHADER_STAGE_COMPUTE_BIT: return 5;
    default: RDCERR("Unrecognised/not single flag %x", stageFlag); break;
  }

  return 0;
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkImageMemoryBarrier *barriers)
{
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                   NULL,                // global memory barriers
                                   0, NULL,             // buffer memory barriers
                                   count, barriers);    // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkBufferMemoryBarrier *barriers)
{
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                   NULL,               // global memory barriers
                                   count, barriers,    // buffer memory barriers
                                   0, NULL);           // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkMemoryBarrier *barriers)
{
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, count,
                                   barriers,    // global memory barriers
                                   0, NULL,     // buffer memory barriers
                                   0, NULL);    // image memory barriers
}

ResourceFormat MakeResourceFormat(VkFormat fmt)
{
  ResourceFormat ret;

  ret.type = ResourceFormatType::Regular;
  ret.compByteWidth = 0;
  ret.compCount = 0;
  ret.compType = CompType::Typeless;
  ret.srgbCorrected = false;

  if(fmt == VK_FORMAT_UNDEFINED)
  {
    ret.type = ResourceFormatType::Undefined;
    return ret;
  }

  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8: ret.type = ResourceFormatType::R4G4; break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16: ret.type = ResourceFormatType::R4G4B4A4; break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32: ret.type = ResourceFormatType::R10G10B10A2; break;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: ret.type = ResourceFormatType::R11G11B10; break;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: ret.type = ResourceFormatType::R9G9B9E5; break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16: ret.type = ResourceFormatType::R5G6B5; break;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16: ret.type = ResourceFormatType::R5G5B5A1; break;
    case VK_FORMAT_D16_UNORM_S8_UINT: ret.type = ResourceFormatType::D16S8; break;
    case VK_FORMAT_D24_UNORM_S8_UINT: ret.type = ResourceFormatType::D24S8; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT: ret.type = ResourceFormatType::D32S8; break;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: ret.type = ResourceFormatType::BC1; break;
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK: ret.type = ResourceFormatType::BC2; break;
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK: ret.type = ResourceFormatType::BC3; break;
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK: ret.type = ResourceFormatType::BC4; break;
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK: ret.type = ResourceFormatType::BC5; break;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK: ret.type = ResourceFormatType::BC6; break;
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK: ret.type = ResourceFormatType::BC7; break;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: ret.type = ResourceFormatType::ETC2; break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: ret.type = ResourceFormatType::EAC; break;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: ret.type = ResourceFormatType::ASTC; break;
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: ret.type = ResourceFormatType::PVRTC; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32: ret.bgraOrder = true; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK: ret.compCount = 1; break;
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: ret.compCount = 2; break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB: ret.compCount = 3; break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: ret.compCount = 4; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: ret.srgbCorrected = true; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: ret.compType = CompType::UNorm; break;
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32: ret.compType = CompType::SNorm; break;
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32: ret.compType = CompType::UScaled; break;
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: ret.compType = CompType::SScaled; break;
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    // Maybe S8 should be identified by something else?
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32: ret.compType = CompType::UInt; break;
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: ret.compType = CompType::SInt; break;
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: ret.compType = CompType::Float; break;
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret.compType = CompType::Double; break;
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT: ret.compType = CompType::Depth; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB: ret.compByteWidth = 1; break;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_D16_UNORM: ret.compByteWidth = 2; break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: ret.compByteWidth = 3; break;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT: ret.compByteWidth = 4; break;
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret.compByteWidth = 8; break;
    default: break;
  }

  return ret;
}

VkFormat MakeVkFormat(ResourceFormat fmt)
{
  VkFormat ret = VK_FORMAT_UNDEFINED;

  if(fmt.Special())
  {
    switch(fmt.type)
    {
      case ResourceFormatType::BC1:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        else
          ret = fmt.srgbCorrected ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        break;
      }
      case ResourceFormatType::BC2:
        ret = fmt.srgbCorrected ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC3:
        ret = fmt.srgbCorrected ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC4:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC4_SNORM_BLOCK : VK_FORMAT_BC4_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC5:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC5_SNORM_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC6:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC6H_SFLOAT_BLOCK
                                              : VK_FORMAT_BC6H_UFLOAT_BLOCK;
        break;
      case ResourceFormatType::BC7:
        ret = fmt.srgbCorrected ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
        break;
      case ResourceFormatType::ETC2:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
                                  : VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        else
          ret = fmt.srgbCorrected ? VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
                                  : VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        break;
      }
      case ResourceFormatType::EAC:
      {
        if(fmt.compCount == 1)
          ret = fmt.compType == CompType::SNorm ? VK_FORMAT_EAC_R11_SNORM_BLOCK
                                                : VK_FORMAT_EAC_R11_UNORM_BLOCK;
        else if(fmt.compCount == 2)
          ret = fmt.compType == CompType::SNorm ? VK_FORMAT_EAC_R11G11_SNORM_BLOCK
                                                : VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        break;
      }
      case ResourceFormatType::R10G10B10A2:
        if(fmt.compType == CompType::UNorm)
          ret = fmt.bgraOrder ? VK_FORMAT_A2R10G10B10_UNORM_PACK32
                              : VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        else if(fmt.compType == CompType::UInt)
          ret = fmt.bgraOrder ? VK_FORMAT_A2R10G10B10_UINT_PACK32 : VK_FORMAT_A2B10G10R10_UINT_PACK32;
        else if(fmt.compType == CompType::UScaled)
          ret = fmt.bgraOrder ? VK_FORMAT_A2R10G10B10_USCALED_PACK32
                              : VK_FORMAT_A2B10G10R10_USCALED_PACK32;
        else if(fmt.compType == CompType::SNorm)
          ret = fmt.bgraOrder ? VK_FORMAT_A2R10G10B10_SNORM_PACK32
                              : VK_FORMAT_A2B10G10R10_SNORM_PACK32;
        else if(fmt.compType == CompType::SInt)
          ret = fmt.bgraOrder ? VK_FORMAT_A2R10G10B10_SINT_PACK32 : VK_FORMAT_A2B10G10R10_SINT_PACK32;
        else if(fmt.compType == CompType::SScaled)
          ret = fmt.bgraOrder ? VK_FORMAT_A2R10G10B10_SSCALED_PACK32
                              : VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
        break;
      case ResourceFormatType::R11G11B10: ret = VK_FORMAT_B10G11R11_UFLOAT_PACK32; break;
      case ResourceFormatType::R5G6B5: ret = VK_FORMAT_B5G6R5_UNORM_PACK16; break;
      case ResourceFormatType::R5G5B5A1:
        ret = fmt.bgraOrder ? VK_FORMAT_B5G5R5A1_UNORM_PACK16 : VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        break;
      case ResourceFormatType::R9G9B9E5: ret = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32; break;
      case ResourceFormatType::R4G4B4A4:
        ret = fmt.bgraOrder ? VK_FORMAT_R4G4B4A4_UNORM_PACK16 : VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        break;
      case ResourceFormatType::R4G4: ret = VK_FORMAT_R4G4_UNORM_PACK8; break;
      case ResourceFormatType::D24S8: ret = VK_FORMAT_D24_UNORM_S8_UINT; break;
      case ResourceFormatType::D32S8: ret = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
      default: RDCERR("Unsupported resource format type %u", fmt.type); break;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.srgbCorrected)
    {
      ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB;
    }
    else if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64G64B64A64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64G64B64A64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64G64B64A64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32B32A32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32B32A32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32B32A32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16B16A16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16B16A16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16B16A16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16B16A16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16B16A16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16B16A16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16B16A16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SINT : VK_FORMAT_R8G8B8A8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_UINT : VK_FORMAT_R8G8B8A8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SSCALED : VK_FORMAT_R8G8B8A8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_USCALED : VK_FORMAT_R8G8B8A8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 4-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 3)
  {
    if(fmt.srgbCorrected)
    {
      ret = VK_FORMAT_R8G8B8_SRGB;
    }
    else if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64G64B64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64G64B64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64G64B64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32B32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32B32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32B32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16B16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16B16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16B16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16B16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16B16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16B16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16B16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8G8B8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8G8B8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8G8B8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8G8B8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8G8B8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8G8B8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 2)
  {
    if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64G64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64G64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64G64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8G8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8G8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8G8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8G8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8G8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8G8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 1)
  {
    if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32_UINT;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_D32_SFLOAT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16_UNORM;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_D16_UNORM;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16_USCALED;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16_SSCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8_UNORM;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8_USCALED;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8_SSCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else
  {
    RDCERR("Unrecognised component count: %d", fmt.compCount);
  }

  if(ret == VK_FORMAT_UNDEFINED)
    RDCERR("No known vulkan format corresponding to resource format!");

  return ret;
}

Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints)
{
  switch(Topo)
  {
    default: break;
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return Topology::PointList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: return Topology::LineList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return Topology::LineStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return Topology::TriangleList; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return Topology::TriangleStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: return Topology::TriangleFan; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: return Topology::LineList_Adj; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: return Topology::LineStrip_Adj; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      return Topology::TriangleList_Adj;
      break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      return Topology::TriangleStrip_Adj;
      break;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: return PatchList_Topology(patchControlPoints); break;
  }

  return Topology::Unknown;
}

VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo)
{
  switch(Topo)
  {
    case Topology::LineLoop: RDCWARN("Unsupported primitive topology on Vulkan: %x", Topo); break;
    default: return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    case Topology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Topology::LineStrip_Adj: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case Topology::LineList_Adj: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Topology::TriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Topology::TriangleStrip_Adj: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
    case Topology::TriangleList_Adj: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case Topology::PatchList_1CPs:
    case Topology::PatchList_2CPs:
    case Topology::PatchList_3CPs:
    case Topology::PatchList_4CPs:
    case Topology::PatchList_5CPs:
    case Topology::PatchList_6CPs:
    case Topology::PatchList_7CPs:
    case Topology::PatchList_8CPs:
    case Topology::PatchList_9CPs:
    case Topology::PatchList_10CPs:
    case Topology::PatchList_11CPs:
    case Topology::PatchList_12CPs:
    case Topology::PatchList_13CPs:
    case Topology::PatchList_14CPs:
    case Topology::PatchList_15CPs:
    case Topology::PatchList_16CPs:
    case Topology::PatchList_17CPs:
    case Topology::PatchList_18CPs:
    case Topology::PatchList_19CPs:
    case Topology::PatchList_20CPs:
    case Topology::PatchList_21CPs:
    case Topology::PatchList_22CPs:
    case Topology::PatchList_23CPs:
    case Topology::PatchList_24CPs:
    case Topology::PatchList_25CPs:
    case Topology::PatchList_26CPs:
    case Topology::PatchList_27CPs:
    case Topology::PatchList_28CPs:
    case Topology::PatchList_29CPs:
    case Topology::PatchList_30CPs:
    case Topology::PatchList_31CPs:
    case Topology::PatchList_32CPs: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  }

  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

AddressMode MakeAddressMode(VkSamplerAddressMode addr)
{
  switch(addr)
  {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT: return AddressMode::Wrap;
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return AddressMode::Mirror;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return AddressMode::ClampEdge;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return AddressMode::ClampBorder;
    case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

void MakeBorderColor(VkBorderColor border, FloatVector *BorderColor)
{
  // we don't distinguish float/int, assume it matches
  switch(border)
  {
    case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
    case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
      *BorderColor = FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
      break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
    case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
      *BorderColor = FloatVector(0.0f, 0.0f, 0.0f, 1.0f);
      break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
    case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
      *BorderColor = FloatVector(1.0f, 1.0f, 1.0f, 1.0f);
      break;
    default: memset(BorderColor, 0, sizeof(FloatVector)); break;
  }
}

CompareFunction MakeCompareFunc(VkCompareOp func)
{
  switch(func)
  {
    case VK_COMPARE_OP_NEVER: return CompareFunction::Never;
    case VK_COMPARE_OP_LESS: return CompareFunction::Less;
    case VK_COMPARE_OP_EQUAL: return CompareFunction::Equal;
    case VK_COMPARE_OP_LESS_OR_EQUAL: return CompareFunction::LessEqual;
    case VK_COMPARE_OP_GREATER: return CompareFunction::Greater;
    case VK_COMPARE_OP_NOT_EQUAL: return CompareFunction::NotEqual;
    case VK_COMPARE_OP_GREATER_OR_EQUAL: return CompareFunction::GreaterEqual;
    case VK_COMPARE_OP_ALWAYS: return CompareFunction::AlwaysTrue;
    default: break;
  }

  return CompareFunction::AlwaysTrue;
}

static FilterMode MakeFilterMode(VkFilter f)
{
  switch(f)
  {
    case VK_FILTER_NEAREST: return FilterMode::Point;
    case VK_FILTER_LINEAR: return FilterMode::Linear;
    case VK_FILTER_CUBIC_IMG: return FilterMode::Cubic;
    default: break;
  }

  return FilterMode::NoFilter;
}

static FilterMode MakeFilterMode(VkSamplerMipmapMode f)
{
  switch(f)
  {
    case VK_SAMPLER_MIPMAP_MODE_NEAREST: return FilterMode::Point;
    case VK_SAMPLER_MIPMAP_MODE_LINEAR: return FilterMode::Linear;
    default: break;
  }

  return FilterMode::NoFilter;
}

TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
                         bool anisoEnable, bool compareEnable, VkSamplerReductionModeEXT reduction)
{
  TextureFilter ret;

  if(anisoEnable)
  {
    ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
  }
  else
  {
    ret.minify = MakeFilterMode(minFilter);
    ret.magnify = MakeFilterMode(magFilter);
    ret.mip = MakeFilterMode(mipmapMode);
  }
  ret.filter = compareEnable ? FilterFunction::Comparison : FilterFunction::Normal;

  if(compareEnable)
  {
    ret.filter = FilterFunction::Comparison;
  }
  else
  {
    switch(reduction)
    {
      default:
      case VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT:
        ret.filter = FilterFunction::Normal;
        break;
      case VK_SAMPLER_REDUCTION_MODE_MIN_EXT: ret.filter = FilterFunction::Minimum; break;
      case VK_SAMPLER_REDUCTION_MODE_MAX_EXT: ret.filter = FilterFunction::Maximum; break;
    }
  }

  return ret;
}

LogicOperation MakeLogicOp(VkLogicOp op)
{
  switch(op)
  {
    case VK_LOGIC_OP_CLEAR: return LogicOperation::Clear;
    case VK_LOGIC_OP_AND: return LogicOperation::And;
    case VK_LOGIC_OP_AND_REVERSE: return LogicOperation::AndReverse;
    case VK_LOGIC_OP_COPY: return LogicOperation::Copy;
    case VK_LOGIC_OP_AND_INVERTED: return LogicOperation::AndInverted;
    case VK_LOGIC_OP_NO_OP: return LogicOperation::NoOp;
    case VK_LOGIC_OP_XOR: return LogicOperation::Xor;
    case VK_LOGIC_OP_OR: return LogicOperation::Or;
    case VK_LOGIC_OP_NOR: return LogicOperation::Nor;
    case VK_LOGIC_OP_EQUIVALENT: return LogicOperation::Equivalent;
    case VK_LOGIC_OP_INVERT: return LogicOperation::Invert;
    case VK_LOGIC_OP_OR_REVERSE: return LogicOperation::OrReverse;
    case VK_LOGIC_OP_COPY_INVERTED: return LogicOperation::CopyInverted;
    case VK_LOGIC_OP_OR_INVERTED: return LogicOperation::OrInverted;
    case VK_LOGIC_OP_NAND: return LogicOperation::Nand;
    case VK_LOGIC_OP_SET: return LogicOperation::Set;
    default: break;
  }

  return LogicOperation::NoOp;
}

BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend)
{
  switch(blend)
  {
    case VK_BLEND_FACTOR_ZERO: return BlendMultiplier::Zero;
    case VK_BLEND_FACTOR_ONE: return BlendMultiplier::One;
    case VK_BLEND_FACTOR_SRC_COLOR: return BlendMultiplier::SrcCol;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case VK_BLEND_FACTOR_DST_COLOR: return BlendMultiplier::DstCol;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return BlendMultiplier::InvDstCol;
    case VK_BLEND_FACTOR_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case VK_BLEND_FACTOR_DST_ALPHA: return BlendMultiplier::DstAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case VK_BLEND_FACTOR_CONSTANT_COLOR: return BlendMultiplier::FactorRGB;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return BlendMultiplier::InvFactorRGB;
    case VK_BLEND_FACTOR_CONSTANT_ALPHA: return BlendMultiplier::FactorAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return BlendMultiplier::InvFactorAlpha;
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return BlendMultiplier::SrcAlphaSat;
    case VK_BLEND_FACTOR_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case VK_BLEND_FACTOR_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(VkBlendOp op)
{
  // Need to update this when we support VK_EXT_blend_operation_advanced
  switch(op)
  {
    case VK_BLEND_OP_ADD: return BlendOperation::Add;
    case VK_BLEND_OP_SUBTRACT: return BlendOperation::Subtract;
    case VK_BLEND_OP_REVERSE_SUBTRACT: return BlendOperation::ReversedSubtract;
    case VK_BLEND_OP_MIN: return BlendOperation::Minimum;
    case VK_BLEND_OP_MAX: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

StencilOperation MakeStencilOp(VkStencilOp op)
{
  switch(op)
  {
    case VK_STENCIL_OP_KEEP: return StencilOperation::Keep;
    case VK_STENCIL_OP_ZERO: return StencilOperation::Zero;
    case VK_STENCIL_OP_REPLACE: return StencilOperation::Replace;
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP: return StencilOperation::IncSat;
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP: return StencilOperation::DecSat;
    case VK_STENCIL_OP_INVERT: return StencilOperation::Invert;
    case VK_STENCIL_OP_INCREMENT_AND_WRAP: return StencilOperation::IncWrap;
    case VK_STENCIL_OP_DECREMENT_AND_WRAP: return StencilOperation::DecWrap;
    default: break;
  }

  return StencilOperation::Keep;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkInitParams &el)
{
  SERIALISE_MEMBER(AppName);
  SERIALISE_MEMBER(EngineName);
  SERIALISE_MEMBER(AppVersion);
  SERIALISE_MEMBER(EngineVersion);
  SERIALISE_MEMBER(APIVersion);
  SERIALISE_MEMBER(Layers);
  SERIALISE_MEMBER(Extensions);
  SERIALISE_MEMBER(InstanceID).TypedAs("VkInstance");
}

INSTANTIATE_SERIALISE_TYPE(VkInitParams);

VkDriverInfo::VkDriverInfo(const VkPhysicalDeviceProperties &physProps)
{
  m_Vendor = GPUVendorFromPCIVendor(physProps.vendorID);

  // add non-PCI vendor IDs
  if(physProps.vendorID == 0x10002)
    m_Vendor = GPUVendor::Verisilicon;

  m_Major = VK_VERSION_MAJOR(physProps.driverVersion);
  m_Minor = VK_VERSION_MINOR(physProps.driverVersion);
  m_Patch = VK_VERSION_PATCH(physProps.driverVersion);

  // nvidia uses its own version packing:
  //   10 |  8  |        8       |       6
  // major|minor|secondary_branch|tertiary_branch
  if(m_Vendor == GPUVendor::nVidia)
  {
    m_Major = ((uint32_t)(physProps.driverVersion) >> (8 + 8 + 6)) & 0x3ff;
    m_Minor = ((uint32_t)(physProps.driverVersion) >> (8 + 6)) & 0x0ff;

    uint32_t secondary = ((uint32_t)(physProps.driverVersion) >> 6) & 0x0ff;
    uint32_t tertiary = physProps.driverVersion & 0x03f;

    m_Patch = (secondary << 8) | tertiary;
  }

  if(m_Vendor == GPUVendor::nVidia)
  {
    // drivers before 372.54 did not handle a glslang bugfix about separated samplers,
    // and disabling texelFetch works as a workaround.

    if(Major() < 372 || (Major() == 372 && Minor() < 54))
      texelFetchBrokenDriver = true;
  }

// only check this on windows. This is a bit of a hack, as really we want to check if we're
// using the AMD official driver, but there's not a great other way to distinguish it from
// the RADV open source driver.
#if ENABLED(RDOC_WIN32)
  if(m_Vendor == GPUVendor::AMD)
  {
    // for AMD the bugfix version isn't clear as version numbering wasn't strong for a while, but
    // any driver that reports a version of >= 1.0.0 is fine, as previous versions all reported
    // 0.9.0 as the version.

    if(Major() < 1)
      texelFetchBrokenDriver = true;
  }
#endif

  if(texelFetchBrokenDriver)
  {
    RDCWARN("Detected an older driver, enabling workaround. Try updating to the latest drivers.");
  }

// same as above, only affects the AMD official driver
#if ENABLED(RDOC_WIN32)
  if(m_Vendor == GPUVendor::AMD)
  {
    // not fixed yet
    amdStorageMSAABrokenDriver = true;
  }
#endif

  // not fixed yet
  qualcommLeakingUBOOffsets = m_Vendor == GPUVendor::Qualcomm;
}

FrameRefType GetRefType(VkDescriptorType descType)
{
  switch(descType)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return eFrameRef_Read; break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return eFrameRef_Write; break;
    default: RDCERR("Unexpected descriptor type");
  }

  return eFrameRef_Read;
}

void DescriptorSetSlot::RemoveBindRefs(VkResourceRecord *record)
{
  if(texelBufferView != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(texelBufferView));

    VkResourceRecord *viewRecord = GetRecord(texelBufferView);
    if(viewRecord && viewRecord->baseResource != ResourceId())
      record->RemoveBindFrameRef(viewRecord->baseResource);
  }
  if(imageInfo.imageView != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(imageInfo.imageView));

    VkResourceRecord *viewRecord = GetRecord(imageInfo.imageView);
    if(viewRecord)
    {
      record->RemoveBindFrameRef(viewRecord->baseResource);
      if(viewRecord->baseResourceMem != ResourceId())
        record->RemoveBindFrameRef(viewRecord->baseResourceMem);
    }
  }
  if(imageInfo.sampler != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(imageInfo.sampler));
  }
  if(bufferInfo.buffer != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(bufferInfo.buffer));

    VkResourceRecord *bufRecord = GetRecord(bufferInfo.buffer);
    if(bufRecord && bufRecord->baseResource != ResourceId())
      record->RemoveBindFrameRef(bufRecord->baseResource);
  }

  // NULL everything out now so that we don't accidentally reference an object
  // that was removed already
  texelBufferView = VK_NULL_HANDLE;
  bufferInfo.buffer = VK_NULL_HANDLE;
  imageInfo.imageView = VK_NULL_HANDLE;
  imageInfo.sampler = VK_NULL_HANDLE;
}

void DescriptorSetSlot::AddBindRefs(VkResourceRecord *record, FrameRefType ref)
{
  if(texelBufferView != VK_NULL_HANDLE)
  {
    record->AddBindFrameRef(GetResID(texelBufferView), eFrameRef_Read,
                            GetRecord(texelBufferView)->sparseInfo != NULL);
    if(GetRecord(texelBufferView)->baseResource != ResourceId())
      record->AddBindFrameRef(GetRecord(texelBufferView)->baseResource, ref);
  }
  if(imageInfo.imageView != VK_NULL_HANDLE)
  {
    record->AddBindFrameRef(GetResID(imageInfo.imageView), eFrameRef_Read,
                            GetRecord(imageInfo.imageView)->sparseInfo != NULL);
    record->AddBindFrameRef(GetRecord(imageInfo.imageView)->baseResource, ref);
    if(GetRecord(imageInfo.imageView)->baseResourceMem != ResourceId())
      record->AddBindFrameRef(GetRecord(imageInfo.imageView)->baseResourceMem, eFrameRef_Read);
  }
  if(imageInfo.sampler != VK_NULL_HANDLE)
  {
    record->AddBindFrameRef(GetResID(imageInfo.sampler), eFrameRef_Read);
  }
  if(bufferInfo.buffer != VK_NULL_HANDLE)
  {
    record->AddBindFrameRef(GetResID(bufferInfo.buffer), eFrameRef_Read,
                            GetRecord(bufferInfo.buffer)->sparseInfo != NULL);
    if(GetRecord(bufferInfo.buffer)->baseResource != ResourceId())
      record->AddBindFrameRef(GetRecord(bufferInfo.buffer)->baseResource, ref);
  }
}
