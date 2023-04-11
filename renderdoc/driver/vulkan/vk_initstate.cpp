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

#include "core/settings.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_debug.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

// VKTODOLOW there's a lot of duplicated code in this file for creating a buffer to do
// a memory copy and saving to disk.

// VKTODOLOW in general we do a lot of "create buffer, use it, flush/sync then destroy".
// I don't know what the exact cost is, but it would be nice to batch up the buffers/etc
// used across init state use, and only do a single flush. Also we could then get some
// nice command buffer reuse (although need to be careful we don't create too large a
// command buffer that stalls the GPU).
// See INITSTATEBATCH

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, AspectSparseTable &el)
{
  SERIALISE_MEMBER(aspectMask);
  SERIALISE_MEMBER(table);
}

void WrappedVulkan::Begin_PrepareInitialBatch()
{
  m_PrepareInitStateBatching = true;
}

void WrappedVulkan::End_PrepareInitialBatch()
{
  CloseInitStateCmd();
  SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
  SubmitCmds();
  FlushQ();
  SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
  m_PrepareInitStateBatching = false;
}

bool WrappedVulkan::Prepare_InitialState(WrappedVkRes *res)
{
  ResourceId id = GetResourceManager()->GetID(res);

  RDCASSERT(m_PrepareInitStateBatching);

  VkResourceType type = IdentifyTypeByPtr(res);

  uint64_t estimatedSize = 0;
  if(type == eResDeviceMemory)
  {
    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    if(record)
      estimatedSize = record->Length;
  }
  else if(type == eResImage)
  {
    WrappedVkImage *im = (WrappedVkImage *)res;
    const ResourceInfo &resInfo = *im->record->resInfo;
    const ImageInfo &imageInfo = resInfo.imageInfo;

    estimatedSize = GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                imageInfo.extent.depth, imageInfo.format, 0);
    if(imageInfo.sampleCount > 1)
      estimatedSize *= imageInfo.sampleCount;
    if(imageInfo.layerCount > 1)
      estimatedSize *= imageInfo.layerCount;
    // conservative estimate of full mip chain impact
    if(imageInfo.levelCount > 1)
      estimatedSize *= 2;
  }

  uint32_t softMemoryLimit = RenderDoc::Inst().GetCaptureOptions().softMemoryLimit;
  if(softMemoryLimit > 0 && !m_PreparedNotSerialisedInitStates.empty() &&
     CurMemoryUsage(MemoryScope::InitialContents) + estimatedSize > softMemoryLimit * 1024 * 1024ULL)
  {
    CloseInitStateCmd();
    SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
    SubmitCmds();
    FlushQ();
    SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);

    RDCLOG("Flushing batch of initial states to disk with %llu bytes allocated",
           CurMemoryUsage(MemoryScope::InitialContents));
    rdcstr tempFile = StringFormat::Fmt(
        "%s/rdoc_%llu_%llu.bin", get_dirname(RenderDoc::Inst().GetCaptureFileTemplate()).c_str(),
        Timing::GetTick(), Threading::GetCurrentID());
    FileIO::CreateParentDirectory(tempFile);
    m_InitTempFiles.push_back(tempFile);
    WriteSerialiser ser(
        new StreamWriter(FileIO::fopen(tempFile, FileIO::WriteBinary), Ownership::Stream),
        Ownership::Stream);

    for(ResourceId flushId : m_PreparedNotSerialisedInitStates)
    {
      VkInitialContents initData = GetResourceManager()->GetInitialContents(flushId);

      GetResourceManager()->SetInitialContents(flushId, VkInitialContents());

      uint64_t start = ser.GetWriter()->GetOffset();
      {
        uint64_t size = GetSize_InitialState(id, initData);

        SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContents, size);

        // record is not needed on vulkan
        Serialise_InitialState(ser, flushId, NULL, &initData);
      }
      uint64_t end = ser.GetWriter()->GetOffset();

      if(ser.IsErrored())
        break;

      GetResourceManager()->SetInitialFileStore(flushId, tempFile, start, end);
    }

    m_PreparedNotSerialisedInitStates.clear();

    if(ser.IsErrored())
    {
      m_CaptureFailure = true;
      m_LastCaptureError = ser.GetError();
      return false;
    }
  }

  if(type == eResDescriptorSet)
  {
    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    RDCASSERT(record->descInfo && record->descInfo->layout);
    const DescSetLayout &layout = *record->descInfo->layout;

    VkInitialContents initialContents(type, VkInitialContents::DescriptorSet);

    if((layout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0)
    {
      record->descInfo->data.copy(initialContents.descriptorSlots, initialContents.numDescriptors,
                                  initialContents.inlineData, initialContents.inlineByteSize);
    }
    else
    {
      RDCERR("Push descriptor set with initial contents! Should never have been marked dirty");
      initialContents.numDescriptors = 0;
      initialContents.descriptorSlots = NULL;
    }

    GetResourceManager()->SetInitialContents(id, initialContents);
    return true;
  }
  else if(type == eResBuffer)
  {
    WrappedVkBuffer *buffer = (WrappedVkBuffer *)res;

    // buffers are only dirty if they are sparse
    RDCASSERT(buffer->record->resInfo && buffer->record->resInfo->IsSparse());

    VkInitialContents initialContents(type, VkInitialContents::SparseTableOnly);

    initialContents.SnapshotPageTable(*buffer->record->resInfo);

    GetResourceManager()->SetInitialContents(id, initialContents);
    return true;
  }
  else if(type == eResImage)
  {
    VkResult vkr = VK_SUCCESS;

    WrappedVkImage *im = (WrappedVkImage *)res;
    const ResourceInfo &resInfo = *im->record->resInfo;
    const ImageInfo &imageInfo = resInfo.imageInfo;
    const bool wasms = imageInfo.sampleCount > 1;

    LockedImageStateRef state = FindImageState(im->id);

    // if the image has no memory bound, nothing is to be fetched
    if(!state || !state->isMemoryBound)
      return true;

    for(auto it = state->subresourceStates.begin(); it != state->subresourceStates.end(); ++it)
    {
      if(it->state().newQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT ||
         it->state().newQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL)
      {
        // This image has a subresource owned by an external/foreign queue family, so we can't fetch
        // the initial contents.
        return true;
      }
    }

    VkDevice d = GetDev();
    VkCommandBuffer cmd = GetInitStateCmd();

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(imageInfo.format))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, imageInfo.format, 0);

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    VkImage realim = im->real.As<VkImage>();
    int numLayers = imageInfo.layerCount;

    if(wasms)
    {
      // first decompose to array
      numLayers *= imageInfo.sampleCount;
    }

    uint32_t planeCount = GetYUVPlaneCount(imageInfo.format);
    uint32_t horizontalPlaneShift = 0;
    uint32_t verticalPlaneShift = 0;

    if(planeCount > 1)
    {
      switch(imageInfo.format)
      {
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
          horizontalPlaneShift = verticalPlaneShift = 1;
          break;
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: horizontalPlaneShift = 1; break;
        default: break;
      }
    }

    // keep actual format to size multisampled buffers (the compute path interleaves in-place)
    VkFormat sizeFormat = wasms ? imageInfo.format : GetDepthOnlyFormat(imageInfo.format);

    for(int a = 0; a < numLayers; a++)
    {
      for(int m = 0; m < imageInfo.levelCount; m++)
      {
        bufInfo.size = AlignUp(bufInfo.size, bufAlignment);

        if(planeCount > 1)
        {
          // need to consider each plane aspect separately. We simplify the calculation by just
          // aligning up the width to a multiple of 4, that ensures each plane will start at a
          // multiple of 4 because the rowpitch must be a multiple of 4
          bufInfo.size += GetByteSize(AlignUp4(imageInfo.extent.width), imageInfo.extent.height,
                                      imageInfo.extent.depth, sizeFormat, m);
        }
        else
        {
          bufInfo.size += GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                      imageInfo.extent.depth, sizeFormat, m);

          if(sizeFormat != imageInfo.format)
          {
            // if there's stencil and depth, allocate space for stencil
            bufInfo.size = AlignUp(bufInfo.size, bufAlignment);

            bufInfo.size += GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                        imageInfo.extent.depth, VK_FORMAT_S8_UINT, m);
          }
        }
      }
    }

    // since this happens during capture, we don't want to start serialising extra buffer creates,
    // leave this buffer as unwrapped
    VkBuffer dstBuf;

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    CheckVkResult(vkr);

    VkMemoryRequirements dstBufMrq = {};
    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &dstBufMrq);
    MemoryAllocation readbackmem = AllocateMemoryForResource(
        true, dstBufMrq, MemoryScope::InitialContents, MemoryType::Readback);

    if(readbackmem.mem == VK_NULL_HANDLE)
    {
      SET_ERROR_RESULT(m_LastCaptureError, ResultCode::OutOfMemory,
                       "Couldn't allocate readback memory");
      m_CaptureFailure = true;
      return false;
    }

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem.mem), readbackmem.offs);
    CheckVkResult(vkr);

    VkImageAspectFlags aspectFlags = FormatImageAspects(imageInfo.format);

    ImageBarrierSequence setupBarriers, cleanupBarriers;

    const VkImageLayout readingLayout =
        wasms ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    state->TempTransition(m_QueueFamilyIdx, readingLayout,
                          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, setupBarriers,
                          cleanupBarriers, GetImageTransitionInfo());
    InlineSetupImageBarriers(cmd, setupBarriers);
    m_setupImageBarriers.Merge(setupBarriers);
    if(wasms)
    {
      GetDebugManager()->CopyTex2DMSToBuffer(cmd, dstBuf, realim, imageInfo.extent, 0,
                                             imageInfo.layerCount, 0, imageInfo.sampleCount,
                                             imageInfo.format);

      VkBufferMemoryBarrier bufBarrier = {
          VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_SHADER_WRITE_BIT,
          VK_ACCESS_HOST_READ_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          dstBuf,
          0,
          bufInfo.size,
      };

      // wait for copy to finish before reading back to host
      DoPipelineBarrier(cmd, 1, &bufBarrier);
    }

    VkDeviceSize bufOffset = 0;
    const int numLayersToCopy = wasms ? 0 : numLayers;

    // loop over every slice/mip, copying it to the appropriate point in the buffer
    for(int a = 0; a < numLayersToCopy; a++)
    {
      VkExtent3D extent = imageInfo.extent;

      for(int m = 0; m < imageInfo.levelCount; m++)
      {
        VkBufferImageCopy region = {
            0,
            0,
            0,
            {aspectFlags, (uint32_t)m, (uint32_t)a, 1},
            {
                0, 0, 0,
            },
            extent,
        };

        if(planeCount > 1)
        {
          // need to consider each plane aspect separately
          for(uint32_t i = 0; i < planeCount; i++)
          {
            bufOffset = AlignUp(bufOffset, bufAlignment);

            region.imageExtent = extent;
            region.bufferOffset = bufOffset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT << i;

            if(i > 0)
            {
              region.imageExtent.width >>= horizontalPlaneShift;
              region.imageExtent.height >>= verticalPlaneShift;
            }

            bufOffset += GetPlaneByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                          imageInfo.extent.depth, sizeFormat, m, i);

            ObjDisp(d)->CmdCopyImageToBuffer(
                Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuf, 1, &region);
          }
        }
        else
        {
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;

          // for depth/stencil copies, copy depth first
          if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

          bufOffset += GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                   imageInfo.extent.depth, sizeFormat, m);

          ObjDisp(d)->CmdCopyImageToBuffer(
              Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuf, 1, &region);

          if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
          {
            // if we have combined stencil to process, copy that separately now.
            bufOffset = AlignUp(bufOffset, bufAlignment);

            region.bufferOffset = bufOffset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

            bufOffset += GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                     imageInfo.extent.depth, VK_FORMAT_S8_UINT, m);

            ObjDisp(d)->CmdCopyImageToBuffer(
                Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuf, 1, &region);
          }
        }

        // update the extent for the next mip
        extent.width = RDCMAX(extent.width >> 1, 1U);
        extent.height = RDCMAX(extent.height >> 1, 1U);
        extent.depth = RDCMAX(extent.depth >> 1, 1U);
      }
    }

    RDCASSERTMSG("buffer wasn't sized sufficiently!", bufOffset <= bufInfo.size, bufOffset,
                 readbackmem.size, imageInfo.extent, imageInfo.format, numLayers,
                 imageInfo.levelCount);
    InlineCleanupImageBarriers(cmd, cleanupBarriers);
    m_cleanupImageBarriers.Merge(cleanupBarriers);

    if(Vulkan_Debug_SingleSubmitFlushing())
    {
      CloseInitStateCmd();
      SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
      SubmitCmds();
      FlushQ();
      SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
    }

    AddPendingObjectCleanup([d, dstBuf]() { ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL); });

    VkInitialContents initialContents(type, readbackmem);

    // include the sparse page table if it exists
    if(resInfo.IsSparse())
    {
      initialContents.SnapshotPageTable(resInfo);
    }

    GetResourceManager()->SetInitialContents(id, initialContents);
    m_PreparedNotSerialisedInitStates.push_back(id);

    return true;
  }
  else if(type == eResDeviceMemory)
  {
    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);

    // if the memory has no wholeMemBuf we cannot fetch its contents. We shouldn't get here with
    // only images bound to the memory so something has gone wrong
    if(record->memMapState->wholeMemBuf == VK_NULL_HANDLE)
    {
      RDCERR("Trying to fetch device memory initial states without wholeMemBuf");
      return true;
    }

    VkResult vkr = VK_SUCCESS;

    VkDevice d = GetDev();
    VkCommandBuffer cmd = GetInitStateCmd();

    VkDeviceMemory datamem = ToUnwrappedHandle<VkDeviceMemory>(res);
    VkDeviceSize datasize = record->Length;

    if(!GetResourceManager()->FindMemRefs(id))
      GetResourceManager()->AddMemoryFrameRefs(id);

    RDCASSERT(datamem != VK_NULL_HANDLE);

    RDCASSERT(record->Length > 0);

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    // we make the buffer concurrently accessible by all queue families to not invalidate the
    // contents of the memory we're reading back from.
    bufInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    bufInfo.queueFamilyIndexCount = (uint32_t)m_QueueFamilyIndices.size();
    bufInfo.pQueueFamilyIndices = m_QueueFamilyIndices.data();

    // spec requires that CONCURRENT must specify more than one queue family. If there is only one
    // queue family, we can safely use exclusive.
    if(bufInfo.queueFamilyIndexCount == 1)
      bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // since this happens during capture, we don't want to start serialising extra buffer creates,
    // leave this buffer as unwrapped
    VkBuffer dstBuf;

    bufInfo.size = datasize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    CheckVkResult(vkr);

    VkMemoryRequirements dstBufMrq = {};
    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &dstBufMrq);
    MemoryAllocation readbackmem = AllocateMemoryForResource(
        true, dstBufMrq, MemoryScope::InitialContents, MemoryType::Readback);

    if(readbackmem.mem == VK_NULL_HANDLE)
    {
      SET_ERROR_RESULT(m_LastCaptureError, ResultCode::OutOfMemory,
                       "Couldn't allocate readback memory");
      m_CaptureFailure = true;
      return false;
    }

    CheckVkResult(vkr);
    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem.mem), readbackmem.offs);
    CheckVkResult(vkr);

    VkBufferCopy region = {0, 0, datasize};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), Unwrap(record->memMapState->wholeMemBuf), dstBuf, 1,
                              &region);

    AddPendingObjectCleanup([d, dstBuf]() { ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL); });

    if(Vulkan_Debug_SingleSubmitFlushing())
    {
      SubmitCmds();
      FlushQ();
    }

    GetResourceManager()->SetInitialContents(id, VkInitialContents(type, readbackmem));
    m_PreparedNotSerialisedInitStates.push_back(id);

    return true;
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }

  m_CaptureFailure = true;
  SET_ERROR_RESULT(m_LastCaptureError, ResultCode::InternalError, "Unknown resource encountered");
  return false;
}

uint64_t WrappedVulkan::GetSize_InitialState(ResourceId id, const VkInitialContents &initial)
{
  uint64_t ret = 0;

  // account for sparse page tables when present
  if(initial.sparseTables)
  {
    // array count overheads
    ret += 128;
    for(size_t i = 0; i < initial.sparseTables->size(); i++)
    {
      ret += sizeof(VkImageAspectFlagBits);
      ret += initial.sparseTables->at(i).table.GetSerialiseSize();
    }
  }

  if(initial.type == eResDescriptorSet)
  {
    // shouldn't have a sparse table here!
    RDCASSERTEQUAL(ret, 0);
    return 128 + initial.numDescriptors * sizeof(DescriptorSetSlot) + initial.inlineByteSize;
  }
  else if(initial.type == eResBuffer)
  {
    // buffers only have initial states when they're sparse
    return ret;
  }
  else if(initial.type == eResImage || initial.type == eResDeviceMemory)
  {
    // the size primarily comes from the buffer, the size of which we conveniently have stored.
    return ret + uint64_t(128 + initial.mem.size + WriteSerialiser::GetChunkAlignment());
  }

  RDCERR("Unhandled resource type %s", ToStr(initial.type).c_str());
  return 128;
}

static rdcliteral NameOfType(VkResourceType type)
{
  switch(type)
  {
    case eResDescriptorSet: return "VkDescriptorSet"_lit;
    case eResDeviceMemory: return "VkDeviceMemory"_lit;
    case eResBuffer: return "VkBuffer"_lit;
    case eResImage: return "VkImage"_lit;
    default: break;
  }
  return "VkResource"_lit;
}

SparseBinding::SparseBinding(WrappedVulkan *vk, VkBuffer unwrappedBuffer,
                             const rdcarray<AspectSparseTable> &tables)
{
  sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
  pNext = NULL;
  waitSemaphoreCount = 0;
  pWaitSemaphores = NULL;
  imageOpaqueBindCount = 0;
  pImageOpaqueBinds = NULL;
  imageBindCount = 0;
  pImageBinds = NULL;
  signalSemaphoreCount = 0;
  pSignalSemaphores = NULL;

  if(tables.empty())
  {
    RDCERR("Expected a page table initialising buffer sparse bindings");
    invalid = true;
    return;
  }

  VkDevice device = vk->GetDev();

  VkMemoryRequirements mrq = {0};
  ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), unwrappedBuffer, &mrq);

  const Sparse::PageTable &table = tables[0].table;

  if(mrq.alignment != table.getPageByteSize())
  {
    RDCERR("Captured page table uses page size %llu, but on replay page size is %llu",
           table.getPageByteSize(), mrq.alignment);
    invalid = true;
    return;
  }

  bufBind.buffer = unwrappedBuffer;

  RDCASSERTEQUAL(table.getMipTail().mappings.size(), 1);

  const Sparse::PageRangeMapping &mapping = table.getMipTail().mappings[0];

  if(mapping.hasSingleMapping())
  {
    opaqueBinds.resize(1);
    opaqueBinds[0].flags = 0;
    opaqueBinds[0].resourceOffset = 0;
    opaqueBinds[0].memory =
        Unwrap(vk->GetResourceManager()->GetLiveHandle<VkDeviceMemory>(mapping.singleMapping.memory));
    opaqueBinds[0].memoryOffset = mapping.singleMapping.offset;
    opaqueBinds[0].size = table.getMipTail().totalPackedByteSize;
  }
  else
  {
    // always bind a page at once
    VkSparseMemoryBind bind = {};
    bind.flags = 0;
    bind.size = mrq.alignment;

    opaqueBinds.reserve(mapping.pages.size());
    for(size_t i = 0; i < mapping.pages.size(); i++)
    {
      bind.memory =
          Unwrap(vk->GetResourceManager()->GetLiveHandle<VkDeviceMemory>(mapping.pages[i].memory));
      bind.memoryOffset = mapping.pages[i].offset;

      VkSparseMemoryBind &previousBind = opaqueBinds.back();

      // simple coalescing for consecutive binds.
      // We know the previous bind was for the previous resource page so we don't have to compare
      // resourceOffset, just memory properties
      if(
          // if there's a previous bind already - guaranteed after i==0
          i > 0
          // and the memory is the same (either both NULL or both the same memory object)
          && bind.memory == previousBind.memory
          // and either
          && (
                 // the memory is NULL (then offsets don't matter)
                 bind.memory == VK_NULL_HANDLE
                 // or the offset immediately follows the last
                 || bind.memoryOffset == previousBind.memoryOffset + mrq.alignment
                 //
                 )
          //
          )
      {
        // apply the previous bind for one more page
        previousBind.size += mrq.alignment;
        continue;
      }

      // otherwise push a new binding for this memory
      bind.resourceOffset = i * mrq.alignment;
      opaqueBinds.push_back(bind);
    }
  }

  bufBind.bindCount = (uint32_t)opaqueBinds.size();
  bufBind.pBinds = opaqueBinds.data();

  bufferBindCount = 1;
  pBufferBinds = &bufBind;
}

SparseBinding::SparseBinding(WrappedVulkan *vk, VkImage unwrappedImage,
                             const rdcarray<AspectSparseTable> &tables)
{
  sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
  pNext = NULL;
  waitSemaphoreCount = 0;
  pWaitSemaphores = NULL;
  bufferBindCount = 0;
  pBufferBinds = NULL;
  signalSemaphoreCount = 0;
  pSignalSemaphores = NULL;

  VkDevice device = vk->GetDev();

  VkMemoryRequirements mrq = {0};
  ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), unwrappedImage, &mrq);

  uint32_t numreqs = 8;
  VkSparseImageMemoryRequirements reqs[8];
  ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), unwrappedImage, &numreqs, reqs);

  // if we have a different number of aspwects, we can't apply this page table
  if(numreqs != tables.size())
  {
    rdcstr tablesStr, replayStr;

    for(size_t i = 0; i < tables.size(); i++)
      tablesStr += ToStr((VkImageAspectFlagBits)tables[i].aspectMask) + ", ";

    if(tablesStr.size() > 2)
      tablesStr.resize(tablesStr.size() - 2);

    for(uint32_t i = 0; i < numreqs; i++)
      replayStr += ToStr((VkImageAspectFlagBits)reqs[i].formatProperties.aspectMask) + ", ";

    if(replayStr.size() > 2)
      replayStr.resize(replayStr.size() - 2);

    RDCERR("Captured page table has %zu aspects (%s), but on replay we need %u aspects (%s)",
           tables.size(), tablesStr.c_str(), numreqs, replayStr.c_str());
    invalid = true;
    return;
  }

  for(uint32_t a = 0; a < numreqs; a++)
  {
    // can't apply if the aspects mismatch
    if(tables[a].aspectMask != reqs[a].formatProperties.aspectMask)
    {
      RDCERR("Captured page table aspect %u is %s, but on replay it is %s", a,
             ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(),
             ToStr((VkImageAspectFlagBits)reqs[a].formatProperties.aspectMask).c_str());
      invalid = true;
      return;
    }

    VkImageAspectFlags aspect = reqs[a].formatProperties.aspectMask;
    const Sparse::PageTable &table = tables[a].table;

    // can't apply if page size changed
    if(mrq.alignment != table.getPageByteSize())
    {
      RDCERR("Captured page table for %s uses page size %llu, but on replay page size is %llu",
             ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(), table.getPageByteSize(),
             mrq.alignment);
      invalid = true;
      return;
    }

    Sparse::Coord blockSize = table.getPageTexelSize();
    VkExtent3D gran = reqs[a].formatProperties.imageGranularity;

    // can't apply if the page texel dimension has changed
    if(blockSize.x != RDCMAX(1U, gran.width) || blockSize.y != RDCMAX(1U, gran.height) ||
       blockSize.z != RDCMAX(1U, gran.depth))
    {
      RDCERR("Captured page table for %s uses %ux%ux%u pages, but on replay pages are %ux%ux%u",
             ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(), blockSize.x, blockSize.y,
             blockSize.z, gran.width, gran.height, gran.depth);
      invalid = true;
      return;
    }

    const Sparse::MipTail &mipTail = table.getMipTail();

    // can't apply if the mip tail is differently shaped/sized
    if(mipTail.firstMip != RDCMIN(table.getMipCount(), reqs[a].imageMipTailFirstLod))
    {
      RDCERR("Captured mip tail for %s begins at mip %u, on replay it begins at %u",
             ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(), mipTail.firstMip,
             reqs[a].imageMipTailFirstLod);
      invalid = true;
      return;
    }

    // if we have a miptail, check its parameters
    if(mipTail.firstMip < table.getMipCount())
    {
      if(mipTail.byteOffset != reqs[a].imageMipTailOffset)
      {
        RDCERR("Captured mip tail for %s begins at offset %llu, on replay it begins at %llu",
               ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(), mipTail.byteOffset,
               reqs[a].imageMipTailOffset);
        invalid = true;
        return;
      }

      if(reqs[a].formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT)
      {
        if(mipTail.totalPackedByteSize != reqs[a].imageMipTailSize)
        {
          RDCERR("Captured single mip tail for %s is %llu bytes, on replay it is %llu",
                 ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(),
                 mipTail.totalPackedByteSize, reqs[a].imageMipTailSize);
          invalid = true;
          return;
        }
      }
      else
      {
        if(mipTail.totalPackedByteSize / table.getArraySize() != reqs[a].imageMipTailSize ||
           mipTail.byteStride != reqs[a].imageMipTailStride)
        {
          RDCERR(
              "Captured mip tail per slice for %s is %llu bytes with stride %llu, "
              "on replay it is %llu bytes with stride %llu",
              ToStr((VkImageAspectFlagBits)tables[a].aspectMask).c_str(),
              mipTail.totalPackedByteSize / table.getArraySize(), mipTail.byteStride,
              reqs[a].imageMipTailSize, reqs[a].imageMipTailStride);
          invalid = true;
          return;
        }
      }
    }

    {
      VkSparseImageMemoryBind bind = {};
      bind.subresource.aspectMask = aspect;
      if(aspect & VK_IMAGE_ASPECT_METADATA_BIT)
        bind.flags = VK_SPARSE_MEMORY_BIND_METADATA_BIT;

      const Sparse::Coord texDim = table.getResourceSize();

      for(uint32_t slice = 0; slice < table.getArraySize(); slice++)
      {
        bind.subresource.arrayLayer = slice;
        for(uint32_t mip = 0; mip < table.getMipCount(); mip++)
        {
          const uint32_t sub = table.calcSubresource(slice, mip);

          if(table.isSubresourceInMipTail(sub))
            continue;

          bind.subresource.mipLevel = mip;

          const Sparse::PageRangeMapping &mapping = table.getSubresource(sub);

          Sparse::Coord mipDim = {
              RDCMAX(1U, texDim.x >> mip), RDCMAX(1U, texDim.y >> mip), RDCMAX(1U, texDim.z >> mip),
          };

          Sparse::Coord dim = table.calcSubresourcePageDim(sub);

          if(mapping.hasSingleMapping())
          {
            // vulkan allows us to bind more than the subresource size as long as it's a case where
            // we
            // have less than a page used on the edges.
            bind.offset = {};
            bind.extent.width = mipDim.x;
            bind.extent.height = mipDim.y;
            bind.extent.depth = mipDim.z;

            bind.memory = Unwrap(vk->GetResourceManager()->GetLiveHandle<VkDeviceMemory>(
                mapping.singleMapping.memory));
            bind.memoryOffset = mapping.singleMapping.offset;

            imgBinds.push_back(bind);
          }
          else
          {
            uint32_t page = 0;

            // bind each block individually. Slow path :(
            bind.extent = {blockSize.x, blockSize.y, blockSize.z};
            for(uint32_t z = 0; z < dim.z; z++)
            {
              bind.offset.z = z * blockSize.z;

              // clamp edge bindings to subresource dimensions
              if(z == dim.z - 1)
                bind.extent.depth = RDCMIN(bind.extent.depth, mipDim.z - bind.offset.z);

              for(uint32_t y = 0; y < dim.y; y++)
              {
                bind.offset.y = y * blockSize.y;

                // clamp edge bindings to subresource dimensions
                if(y == dim.y - 1)
                  bind.extent.height = RDCMIN(bind.extent.height, mipDim.y - bind.offset.y);

                for(uint32_t x = 0; x < dim.x; x++)
                {
                  bind.offset.x = x * blockSize.x;

                  // clamp edge bindings to subresource dimensions
                  if(x == dim.x - 1)
                    bind.extent.width = RDCMIN(bind.extent.width, mipDim.x - bind.offset.x);

                  bind.memory = Unwrap(vk->GetResourceManager()->GetLiveHandle<VkDeviceMemory>(
                      mapping.pages[page].memory));
                  bind.memoryOffset = mapping.pages[page].offset;

                  page++;

                  imgBinds.push_back(bind);
                }

                // reset extent width in case it was clamped in the last iteration of the x loop
                bind.extent.width = blockSize.x;
              }

              // reset extent height in case it was clamped in the last iteration of the y loop

              bind.extent.height = blockSize.y;
            }
          }
        }
      }
    }

    if(mipTail.totalPackedByteSize > 0)
    {
      VkSparseMemoryBind bind = {};

      for(uint32_t slice = 0; slice < mipTail.mappings.size(); slice++)
      {
        const Sparse::PageRangeMapping &mapping = mipTail.mappings[slice];

        // if all slices are in a combined mip tail, byteStride will be 0 and there will only be one
        // mapping. Otherwise we look at each mapping individually and remap the resource offset as
        // appropriate

        bind.resourceOffset = mipTail.byteOffset + mipTail.byteStride * slice;

        if(mapping.hasSingleMapping())
        {
          bind.memory = Unwrap(
              vk->GetResourceManager()->GetLiveHandle<VkDeviceMemory>(mapping.singleMapping.memory));
          bind.memoryOffset = mapping.singleMapping.offset;

          // if stride is 0, we bind the whole mip tail at once. Otherwise only bind the section of
          // memory backing it
          if(mipTail.byteStride == 0)
            bind.size = mipTail.totalPackedByteSize;
          else
            bind.size = mipTail.totalPackedByteSize / mipTail.mappings.size();

          opaqueBinds.push_back(bind);
        }
        else
        {
          // bind a block at a time
          bind.size = mrq.alignment;

          for(size_t i = 0; i < mapping.pages.size(); i++)
          {
            bind.memory = Unwrap(
                vk->GetResourceManager()->GetLiveHandle<VkDeviceMemory>(mapping.pages[i].memory));
            bind.memoryOffset = mapping.pages[i].offset;

            opaqueBinds.push_back(bind);
            bind.resourceOffset += bind.size;
          }
        }
      }
    }
  }

  imgOpaqueBind.image = unwrappedImage;
  imgOpaqueBind.bindCount = (uint32_t)opaqueBinds.size();
  imgOpaqueBind.pBinds = opaqueBinds.data();

  imgBind.image = unwrappedImage;
  imgBind.bindCount = (uint32_t)imgBinds.size();
  imgBind.pBinds = imgBinds.data();

  if(imgOpaqueBind.bindCount > 0)
  {
    imageOpaqueBindCount = 1;
    pImageOpaqueBinds = &imgOpaqueBind;
  }
  else
  {
    imageOpaqueBindCount = 0;
    pImageOpaqueBinds = NULL;
  }
  imageBindCount = 1;
  pImageBinds = &imgBind;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_InitialState(SerialiserType &ser, ResourceId id, VkResourceRecord *,
                                           const VkInitialContents *initial)
{
  bool ret = true;

  SERIALISE_ELEMENT_LOCAL(type, initial->type);
  SERIALISE_ELEMENT(id).TypedAs(NameOfType(type)).Important();

  if(IsReplayingAndReading())
  {
    AddResourceCurChunk(id);
  }

  // we will never have a case where we prepare some resources, serialise one, and then don't
  // serialise all the rest of those resources before any further prepares. Whenever we see a
  // serialise we can then reset the memory for re-use in any prepares that do then come later.
  //
  // The expected order is:
  // 1. Prepare all non-postponed resources.
  //    As part of this in Prepare_initialState we might serialise all pending resources if we
  //    bump into the memory limit. This means memory can be re-used so complies with our
  //    stipulations above
  // 2. Prepare postponed resources last minute
  //    These are individually synchronised to the GPU (not batched) to ensure we have the right
  //    data, but otherwise are treated as above - they will be left pending unless flushed due to
  //    the memory limit and if they are flushed they work exactly the same way
  // 3. At the end of the frame, all postponed resources are prepared in a batch
  // 4. Finally any resources that must be serialised are.
  //
  // Note that with no memory limit, all prepares will happen in steps 1-3 (in two large batches 1
  // and 3 and a batch-per-resource in 2) before any serialises in 4, so this reset is effectively
  // redundant at that point.
  //
  // Note also that with a memory limit resources may overlap between these cases - the only
  // Serialise calls happen when the memory limit is hit and we flush everything pending, but it is
  // likely that some resources will be prepared in step 1, and not be flushed and so still be
  // pending through some or all of the last-minute prepares in step 2 and into step 3.
  // This is still fine though as Serialise is not called in between there for other resources.

  if(IsCaptureMode(m_State))
  {
    ResetMemoryBlocks(MemoryScope::InitialContents);

    // if got here through 'natural' serialises, this array will not be cleared otherwise so we
    // clear it here just for sanity. It should not be used otherwise though
    m_PreparedNotSerialisedInitStates.clear();
  }

  if(type == eResDescriptorSet)
  {
    DescriptorSetSlot *Bindings = NULL;
    uint32_t NumBindings = 0;
    bytebuf InlineData;

    // there's no point in setting up a lazy array when we're structured exporting because we KNOW
    // we're going to need all the data anyway.
    if(!IsStructuredExporting(m_State))
      ser.SetLazyThreshold(1000);

    // while writing, fetching binding information from prepared initial contents
    if(ser.IsWriting())
    {
      Bindings = initial->descriptorSlots;
      NumBindings = initial->numDescriptors;

      InlineData.assign(initial->inlineData, initial->inlineByteSize);
    }

    SERIALISE_ELEMENT_ARRAY(Bindings, NumBindings);
    SERIALISE_ELEMENT(NumBindings).Important();

    ser.SetLazyThreshold(0);

    if(ser.VersionAtLeast(0x12))
    {
      SERIALISE_ELEMENT(InlineData);
    }

    SERIALISE_CHECK_READ_ERRORS();

    // while reading, fetch the binding information and allocate a VkWriteDescriptorSet array
    if(IsReplayingAndReading())
    {
      WrappedVkRes *res = GetResourceManager()->GetLiveResource(id);
      ResourceId liveid = GetResourceManager()->GetLiveID(id);

      VkDescriptorSet set = (VkDescriptorSet)(uint64_t)res;

      const DescSetLayout &layout =
          m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[liveid].layout];

      if(layout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
      {
        RDCERR("Push descriptor set with initial contents!");
        return true;
      }

      if(!ser.VersionAtLeast(0x15))
      {
        // this is from before mutable descriptors, so we serialised the bindings contents above but
        // we need to set their types
        DescriptorSetSlot *bind = Bindings;
        for(uint32_t b = 0; b < (uint32_t)layout.bindings.size(); b++)
        {
          const DescSetLayout::Binding &layoutBind = layout.bindings[b];

          uint32_t descriptorCount = layoutBind.descriptorCount;

          if(layoutBind.variableSize)
            descriptorCount = m_DescriptorSetState[liveid].data.variableDescriptorCount;

          if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
          {
            bind->type = DescriptorSlotType::InlineBlock;
            bind++;
            continue;
          }

          for(uint32_t d = 0; d < descriptorCount; d++)
          {
            bind->type = convert(layoutBind.layoutDescType);
            bind++;
          }
        }
      }

      VkInitialContents initialContents(type, VkInitialContents::DescriptorSet);

      initialContents.descriptorInfo = new VkDescriptorBufferInfo[NumBindings];
      initialContents.inlineInfo = NULL;

      if(layout.inlineCount > 0)
      {
        initialContents.inlineInfo = new VkWriteDescriptorSetInlineUniformBlock[layout.inlineCount];
        initialContents.inlineData = AllocAlignedBuffer(InlineData.size());
        RDCASSERTEQUAL(layout.inlineByteSize, InlineData.size());
        memcpy(initialContents.inlineData, InlineData.data(), InlineData.size());
      }

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Descriptor structs sizes are unexpected, ensure largest size is used");

      rdcarray<VkWriteDescriptorSet> writes;

      VkDescriptorBufferInfo *writeScratch = initialContents.descriptorInfo;
      VkWriteDescriptorSetInlineUniformBlock *dstInline = initialContents.inlineInfo;
      DescriptorSetSlot *srcBindings = Bindings;
      byte *srcInlineData = initialContents.inlineData;

      for(uint32_t bind = 0; bind < (uint32_t)layout.bindings.size(); bind++)
      {
        const DescSetLayout::Binding &layoutBind = layout.bindings[bind];

        uint32_t descriptorCount = layoutBind.descriptorCount;

        if(layoutBind.variableSize)
          descriptorCount = m_DescriptorSetState[liveid].data.variableDescriptorCount;

        if(descriptorCount == 0)
          continue;

        uint32_t inlineSize = 0;

        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          inlineSize = descriptorCount;
          descriptorCount = 1;
        }

        DescriptorSetSlot *slots = srcBindings;
        srcBindings += descriptorCount;

        // check that the resources we need for this write are present, as some might have been
        // skipped due to stale descriptor set slots or otherwise unreferenced objects (the
        // descriptor set initial contents do not cause a frame reference for their resources).
        //
        // For the non-array case it's trivial as either the descriptor is valid, in which case it
        // gets a write, or not, in which case we skip.
        // For the array case we batch up updates as much as possible, iterating along the array and
        // skipping any invalid descriptors.
        // We also use this loop for handling mutable descriptor types, since descriptors in a
        // mutable array could have various different types and each contiguous block will need a
        // separate write.

        // inline block can't be mutable and can't be arrayed, handle it directly here
        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          // handle inline uniform block specially because the descriptorCount doesn't mean what it
          // normally means in the write.
          dstInline->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
          dstInline->pNext = NULL;
          dstInline->pData = srcInlineData + slots->offset;
          dstInline->dataSize = inlineSize;

          VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
          write.pNext = dstInline;
          write.dstSet = set;
          write.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
          write.dstBinding = bind;
          write.descriptorCount = inlineSize;

          writes.push_back(write);

          dstInline++;
        }
        // quick check for slots that were completely uninitialised and so don't have valid data
        else if(!NULLDescriptorsAllowed() && descriptorCount == 1 &&
                slots->resource == ResourceId() && slots->sampler == ResourceId())
        {
          // do nothing - don't increment bind so that the same write descriptor is used next time.
          continue;
        }
        else
        {
          bool success = CreateDescriptorWritesForSlotData(this, writes, writeScratch, slots,
                                                           descriptorCount, set, bind, layoutBind);
          if(!success)
            ret = false;
        }
      }

      initialContents.descriptorWrites = new VkWriteDescriptorSet[writes.size()];
      memcpy(initialContents.descriptorWrites, writes.data(), writes.byteSize());

      initialContents.numDescriptors = (uint32_t)writes.size();

      GetResourceManager()->SetInitialContents(id, initialContents);
    }
  }
  else if(type == eResBuffer)
  {
    // check for legacy captures
    if(ser.IsReading() && ser.VersionLess(0x13))
    {
      RDCWARN(
          "Skipping sparse initial states of buffer from old capture. "
          "Please re-capture with this version of RenderDoc.");

      // serialise without allocating, this makes for a skip
      VkSparseMemoryBind *binds = NULL;
      ser.Serialise("binds"_lit, binds, 0, SerialiserFlags::NoFlags).Hidden();
      uint32_t numBinds = 0;
      SERIALISE_ELEMENT(numBinds).Hidden();

      Sparse::Page *memDataOffs = NULL;
      ser.Serialise("memDataOffs"_lit, memDataOffs, 0, SerialiserFlags::NoFlags).Hidden();
      uint32_t numUniqueMems = 0;
      SERIALISE_ELEMENT(numUniqueMems).Hidden();

      VkDeviceSize totalSize = 0;
      SERIALISE_ELEMENT(totalSize).Hidden();

      uint64_t ContentsSize = 0;
      SERIALISE_ELEMENT(ContentsSize).Hidden();

      byte *Contents = NULL;
      ser.Serialise("Contents"_lit, Contents, ContentsSize, SerialiserFlags::NoFlags).Hidden();

      SERIALISE_CHECK_READ_ERRORS();

      return true;
    }

    rdcarray<AspectSparseTable> sparseTables;

    // while writing, fetch sparse table from prepared initial contents
    if(ser.IsWriting())
    {
      sparseTables = *initial->sparseTables;
    }

    SERIALISE_ELEMENT(sparseTables);

    SERIALISE_CHECK_READ_ERRORS();

    // while reading, store the bindings in initial contents
    if(IsReplayingAndReading())
    {
      VkInitialContents initialContents(type, VkInitialContents::SparseTableOnly);

      WrappedVkRes *res = GetResourceManager()->GetLiveResource(id);
      initialContents.sparseBind =
          new SparseBinding(this, ToUnwrappedHandle<VkBuffer>(res), sparseTables);

      // if something went wrong the sparse binding information is not valid, have to abort
      if(initialContents.sparseBind->invalid)
      {
        delete initialContents.sparseBind;
        return false;
      }

      GetResourceManager()->SetInitialContents(id, initialContents);
    }
  }
  else if(type == eResDeviceMemory || type == eResImage)
  {
    VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;

    SERIALISE_ELEMENT_LOCAL(IsSparse, initial->sparseTables != NULL);

    rdcarray<AspectSparseTable> sparseTables;

    if(IsSparse)
    {
      if(type == eResImage)
      {
        // check for legacy captures
        if(ser.IsReading() && ser.VersionLess(0x13))
        {
          RDCWARN(
              "Skipping sparse initial states of buffer from old capture. "
              "Please re-capture with this version of RenderDoc.");

          // serialise without allocating, this makes for a skip
          VkSparseMemoryBind *opaque = NULL;
          ser.Serialise("opaque"_lit, opaque, 0, SerialiserFlags::NoFlags).Hidden();
          uint32_t opaqueCount = 0;
          SERIALISE_ELEMENT(opaqueCount).Hidden();

          VkExtent3D imgdim = {};
          SERIALISE_ELEMENT(imgdim).Hidden();

          VkExtent3D pagedim = {};
          SERIALISE_ELEMENT(pagedim).Hidden();

          static const uint32_t numLegacyAspects = 4;
          for(uint32_t a = 0; a < numLegacyAspects; a++)
          {
            Sparse::Page *pages = NULL;
            ser.Serialise("pages"_lit, pages, 0, SerialiserFlags::NoFlags).Hidden();
          }

          uint32_t pageCount[numLegacyAspects] = {};
          SERIALISE_ELEMENT(pageCount).Hidden();

          Sparse::Page *memDataOffs = NULL;
          ser.Serialise("memDataOffs"_lit, memDataOffs, 0, SerialiserFlags::NoFlags).Hidden();
          uint32_t numUniqueMems = 0;
          SERIALISE_ELEMENT(numUniqueMems).Hidden();

          VkDeviceSize totalSize = 0;
          SERIALISE_ELEMENT(totalSize).Hidden();

          uint64_t ContentsSize = 0;
          SERIALISE_ELEMENT(ContentsSize).Hidden();

          byte *Contents = NULL;
          ser.Serialise("Contents"_lit, Contents, ContentsSize, SerialiserFlags::NoFlags).Hidden();

          SERIALISE_CHECK_READ_ERRORS();

          return true;
        }

        // while writing, fetch sparse table from prepared initial contents
        if(ser.IsWriting())
          sparseTables = *initial->sparseTables;

        SERIALISE_ELEMENT(sparseTables);
      }
      else
      {
        RDCERR("Invalid initial state - sparse marker for device memory");
        return false;
      }
    }

    VkResult vkr = VK_SUCCESS;

    byte *Contents = NULL;
    uint64_t ContentsSize = initial ? initial->mem.size : 0;
    MemoryAllocation mappedMem;

    // Serialise this separately so that it can be used on reading to prepare the upload memory
    SERIALISE_ELEMENT(ContentsSize);

    const VkDeviceSize nonCoherentAtomSize = GetDeviceProps().limits.nonCoherentAtomSize;

    // the memory/buffer that we allocated on read, to upload the initial contents.
    MemoryAllocation uploadMemory;
    VkBuffer uploadBuf = VK_NULL_HANDLE;

    // during writing, we already have the memory copied off - we just need to map it.
    if(ser.IsWriting())
    {
      if(initial && initial->mem.mem != VK_NULL_HANDLE)
      {
        VkDeviceSize size = AlignUp(initial->mem.size, nonCoherentAtomSize);

        mappedMem = initial->mem;
        vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), initial->mem.offs, size, 0,
                                    (void **)&Contents);
        CheckVkResult(vkr);

        // invalidate the cpu cache for this memory range to avoid reading stale data
        VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            NULL,
            Unwrap(mappedMem.mem),
            mappedMem.offs,
            size,
        };

        vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
        CheckVkResult(vkr);
      }
    }
    else if(IsReplayingAndReading() && !ser.IsErrored())
    {
      // create a buffer with memory attached, which we will fill with the initial contents
      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, ContentsSize,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};

      vkr = vkCreateBuffer(d, &bufInfo, NULL, &uploadBuf);
      CheckVkResult(vkr);

      uploadMemory =
          AllocateMemoryForResource(uploadBuf, MemoryScope::InitialContents, MemoryType::Upload);

      if(uploadMemory.mem == VK_NULL_HANDLE)
        return false;

      vkr = vkBindBufferMemory(d, uploadBuf, uploadMemory.mem, uploadMemory.offs);
      CheckVkResult(vkr);

      mappedMem = uploadMemory;

      vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs,
                                  AlignUp(mappedMem.size, nonCoherentAtomSize), 0,
                                  (void **)&Contents);
      CheckVkResult(vkr);

      if(!Contents)
      {
        RDCERR("Manually reporting failed memory map");
        CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
        return false;
      }

      if(vkr != VK_SUCCESS)
        return false;
    }

    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("Contents"_lit, Contents, ContentsSize, SerialiserFlags::NoFlags).Important();

    // unmap the resource we mapped before - we need to do this on read and on write.
    if(!IsStructuredExporting(m_State) && mappedMem.mem != VK_NULL_HANDLE)
    {
      if(IsReplayingAndReading())
      {
        // first ensure we flush the writes from the cpu to gpu memory
        VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            NULL,
            Unwrap(mappedMem.mem),
            mappedMem.offs,
            AlignUp(mappedMem.size, nonCoherentAtomSize),
        };

        vkr = ObjDisp(d)->FlushMappedMemoryRanges(Unwrap(d), 1, &range);
        CheckVkResult(vkr);
      }

      ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem.mem));
    }

    SERIALISE_CHECK_READ_ERRORS();

    // if we're handling a device memory object, we're done - we note the memory object to delete at
    // the end of the program, and store the buffer to copy off in Apply
    if(IsReplayingAndReading() && ContentsSize > 0)
    {
      ResourceId liveid = GetResourceManager()->GetLiveID(id);

      if(type == eResDeviceMemory)
      {
        VkInitialContents initialContents(type, uploadMemory);
        initialContents.buf = uploadBuf;

        GetResourceManager()->SetInitialContents(id, initialContents);
      }
      else
      {
        VkInitialContents initialContents(type, uploadMemory);

        // if we have sparse page tables, store them here now
        if(!sparseTables.empty())
        {
          WrappedVkRes *res = GetResourceManager()->GetLiveResource(id);
          initialContents.sparseBind =
              new SparseBinding(this, ToUnwrappedHandle<VkImage>(res), sparseTables);

          // if something went wrong the sparse binding information is not valid, have to abort
          if(initialContents.sparseBind->invalid)
          {
            delete initialContents.sparseBind;
            return false;
          }
        }

        VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[liveid];

        // for non-MSAA images, we're done - we'll do buffer-to-image copies with appropriate
        // offsets to copy out the subresources into the image itself.
        if(c.samples == VK_SAMPLE_COUNT_1_BIT)
        {
          initialContents.buf = uploadBuf;
        }
        else
        {
          // use a GPU-local buffer for the MSAA SSBO for read speeds on non-mobile HW.
          VkBuffer gpuBuf = VK_NULL_HANDLE;

          // create a buffer with memory attached, which we will fill with the initial contents
          VkBufferCreateInfo gpuBufInfo = {
              VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, ContentsSize,
              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          };

          vkr = vkCreateBuffer(d, &gpuBufInfo, NULL, &gpuBuf);
          CheckVkResult(vkr);

          MemoryAllocation gpuUploadMemory =
              AllocateMemoryForResource(gpuBuf, MemoryScope::InitialContents, MemoryType::GPULocal);

          if(gpuUploadMemory.mem == VK_NULL_HANDLE)
            return false;

          vkr = vkBindBufferMemory(d, gpuBuf, gpuUploadMemory.mem, gpuUploadMemory.offs);
          CheckVkResult(vkr);

          VkCommandBuffer cmd = GetNextCmd();

          if(cmd == VK_NULL_HANDLE)
            return false;

          VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

          vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);

          VkBufferCopy bufCopy = {0, 0, ContentsSize};
          ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(uploadBuf), Unwrap(gpuBuf), 1, &bufCopy);

          // wait for copy
          VkBufferMemoryBarrier bufBarrier = {
              VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
              NULL,
              VK_ACCESS_TRANSFER_WRITE_BIT,
              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
              VK_QUEUE_FAMILY_IGNORED,
              VK_QUEUE_FAMILY_IGNORED,
              Unwrap(gpuBuf),
              0,
              VK_WHOLE_SIZE,
          };
          DoPipelineBarrier(cmd, 1, &bufBarrier);

          vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);

          SubmitCmds();
          FlushQ();

          // destroy the buffer as it's no longer needed.
          vkDestroyBuffer(d, uploadBuf, NULL);
          FreeMemoryAllocation(uploadMemory);

          initialContents.buf = gpuBuf;
          initialContents.mem = gpuUploadMemory;
        }

        GetResourceManager()->SetInitialContents(id, initialContents);
      }
    }
  }
  else
  {
    RDCERR("Unhandled resource type %s", ToStr(type).c_str());
    ret = false;
  }

  return ret;
}

template bool WrappedVulkan::Serialise_InitialState(ReadSerialiser &ser, ResourceId id,
                                                    VkResourceRecord *record,
                                                    const VkInitialContents *initial);
template bool WrappedVulkan::Serialise_InitialState(WriteSerialiser &ser, ResourceId id,
                                                    VkResourceRecord *record,
                                                    const VkInitialContents *initial);

void WrappedVulkan::Create_InitialState(ResourceId id, WrappedVkRes *live, bool)
{
  if(IsStructuredExporting(m_State))
    return;

  VkResourceType type = IdentifyTypeByPtr(live);

  if(type == eResDescriptorSet)
  {
    // There is no sensible default for a descriptor set to create. The contents are
    // undefined until written to. This means if a descriptor set was alloc'd within a
    // frame (the only time we won't have initial contents tracked for it) then the
    // contents are undefined, so using whatever is currently in the set is fine. Reading
    // from it (and thus getting data from later in the frame potentially) is an error.
    //
    // Note the same kind of problem applies if a descriptor set is alloc'd before the
    // frame and then say slot 5 is never written to until the middle of the frame, then
    // used. The initial states we have prepared won't have anything valid for 5 so when
    // we apply we won't even write anything into slot 5 - the same case as if we had
    // no initial states at all for that descriptor set
  }
  else if(type == eResImage)
  {
    ResourceId liveid = GetResourceManager()->GetLiveID(id);

    VkInitialContents::Tag tag = VkInitialContents::ClearColorImage;
    LockedImageStateRef state = FindImageState(liveid);
    if(!state)
    {
      RDCERR("Couldn't find image info for %s", ToStr(id).c_str());
      GetResourceManager()->SetInitialContents(
          id, VkInitialContents(type, VkInitialContents::ClearColorImage));
      return;
    }
    else if(IsDepthOrStencilFormat(state->GetImageInfo().format))
    {
      tag = VkInitialContents::ClearDepthStencilImage;
    }

    GetResourceManager()->SetInitialContents(id, VkInitialContents(type, tag));
  }
  else if(type == eResDeviceMemory || type == eResBuffer)
  {
    // ignore, it was probably dirty but not referenced in the frame
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }
}

void WrappedVulkan::Apply_InitialState(WrappedVkRes *live, const VkInitialContents &initial)
{
  if(HasFatalError())
    return;

  VkResourceType type = initial.type;

  ResourceId id = GetResourceManager()->GetID(live);

  if(type == eResDescriptorSet)
  {
    VkWriteDescriptorSet *writes = initial.descriptorWrites;

    // if it ended up that no descriptors were valid, just skip
    if(initial.numDescriptors == 0)
      return;

    // deliberately go through our wrapper implementation, to unwrap the VkWriteDescriptorSet
    // structs
    vkUpdateDescriptorSets(GetDev(), initial.numDescriptors, writes, 0, NULL);

    // need to blat over the current descriptor set contents, so these are available
    // when we want to fetch pipeline state
    rdcarray<DescriptorSetSlot *> &bindings = m_DescriptorSetState[id].data.binds;
    bytebuf &inlineData = m_DescriptorSetState[id].data.inlineBytes;

    for(uint32_t i = 0; i < initial.numDescriptors; i++)
    {
      RDCASSERT(writes[i].dstBinding < bindings.size());

      DescriptorSetSlot *bind = bindings[writes[i].dstBinding];

      if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        VkWriteDescriptorSetInlineUniformBlock *inlineWrite =
            (VkWriteDescriptorSetInlineUniformBlock *)FindNextStruct(
                &writes[i], VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);
        memcpy(inlineData.data() + bind->offset + writes[i].dstArrayElement, inlineWrite->pData,
               inlineWrite->dataSize);
        continue;
      }

      for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
      {
        uint32_t idx = writes[i].dstArrayElement + d;

        if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind[idx].SetTexelBuffer(writes[i].descriptorType, GetResID(writes[i].pTexelBufferView[d]));
        }
        else if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
          bind[idx].SetBuffer(writes[i].descriptorType, writes[i].pBufferInfo[d]);
        }
        else
        {
          // we don't ever pass invalid parameters so we can unconditionally set both. Invalid
          // elements are set to VK_NULL_HANDLE which is safe
          bind[idx].SetImage(writes[i].descriptorType, writes[i].pImageInfo[d], true);
        }
      }
    }
  }
  else if(type == eResBuffer)
  {
    // we should only get here if we have a sparse page table to apply
    RDCASSERT(initial.tag == VkInitialContents::SparseTableOnly, (uint32_t)initial.tag);

    // only apply sparse bindings the first time we apply initial contents, OR if there are sparse
    // bindings of this resource in the capture. This is a simple optimisation to avoid needing to
    // re-bind the sparse pages every time if they don't change.
    if(initial.sparseBind &&
       (IsLoading(m_State) || m_SparseBindResources.find(id) != m_SparseBindResources.end()))
      ObjDisp(m_Queue)->QueueBindSparse(Unwrap(m_Queue), 1, initial.sparseBind, VK_NULL_HANDLE);
  }
  else if(type == eResImage)
  {
    ResourceId orig = GetResourceManager()->GetOriginalID(id);

    bool initialized = false;
    InitPolicy policy = GetResourceManager()->GetInitPolicy();

    LockedImageStateRef state = FindImageState(id);
    if(!state)
    {
      RDCWARN("No image state found for image %s", ToStr(id).c_str());
      return;
    }
    ResourceId boundMemory = state->boundMemory;
    VkDeviceSize boundMemoryOffset = state->boundMemoryOffset;
    VkDeviceSize boundMemorySize = state->boundMemorySize;
    const ImageInfo &imageInfo = state->GetImageInfo();
    initialized = IsActiveReplaying(m_State);

    // apply sparse page table mappings and skip memory-bound optimisations
    if(initial.sparseBind)
    {
      // only apply sparse bindings the first time we apply initial contents, OR if there are sparse
      // bindings of this resource in the capture. This is a simple optimisation to avoid needing to
      // re-bind the sparse pages every time if they don't change.
      if(IsLoading(m_State) || m_SparseBindResources.find(id) != m_SparseBindResources.end())
        ObjDisp(m_Queue)->QueueBindSparse(Unwrap(m_Queue), 1, initial.sparseBind, VK_NULL_HANDLE);

      // however we don't track the memory bound to it, so always consider it uninitialised.
      initialized = false;
    }
    else if(initialized && boundMemory != ResourceId())
    {
      ResourceId origMem = GetResourceManager()->GetOriginalID(boundMemory);
      if(origMem != ResourceId())
      {
        MemRefs *memRefs = GetResourceManager()->FindMemRefs(origMem);
        // Check whether any portion of the device memory range bound to this image is written.
        // The memory might be written by a captured command (e.g. mapped memory), or might have
        // initial contents.
        if(memRefs == NULL)
        {
          // The device memory allocation is missing reference info, and so the memory will be reset
          // before each frame; this means the image needs to be treated as if it is uninitialized
          // at the beginning of each replay.
          initialized = false;
        }
        else
        {
          for(auto it = memRefs->rangeRefs.find(boundMemoryOffset);
              it != memRefs->rangeRefs.end() && it->start() < boundMemoryOffset + boundMemorySize;
              ++it)
          {
            if(IncludesWrite(it->value()) ||
               InitReq(it->value(), policy, initialized) != eInitReq_None)
            {
              // The bound memory is written, either by a captured command or by the device memory
              // initialization policy.
              initialized = false;
              break;
            }
          }
        }
      }
    }

    // handle any 'created' initial states, without an actual image with contents
    if(initial.tag != VkInitialContents::BufferCopy)
    {
      // ignore images with no memory bound
      if(boundMemory == ResourceId())
        return;

      if(initial.tag == VkInitialContents::ClearColorImage)
      {
        VkFormat format = imageInfo.format;

        // can't clear these, so leave them alone.
        if(IsBlockFormat(format) || IsYUVFormat(format))
          return;

        VkCommandBuffer cmd = GetInitStateCmd();

        VkMarkerRegion::Begin(StringFormat::Fmt("Clear colour state for %s", ToStr(orig).c_str()),
                              cmd);

        ImageBarrierSequence setupBarriers;
        state->DiscardContents();
        state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, setupBarriers,
                          GetImageTransitionInfo());
        InlineSetupImageBarriers(cmd, setupBarriers);
        m_setupImageBarriers.Merge(setupBarriers);

        VkClearColorValue clearval = {};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
                                         VK_REMAINING_ARRAY_LAYERS};

        ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), ToUnwrappedHandle<VkImage>(live),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1, &range);

        VkMarkerRegion::End(cmd);

        if(Vulkan_Debug_SingleSubmitFlushing())
        {
          CloseInitStateCmd();
          SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
          SubmitCmds();
          FlushQ();
          SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
        }
      }
      else if(initial.tag == VkInitialContents::ClearDepthStencilImage)
      {
        VkCommandBuffer cmd = GetInitStateCmd();

        VkMarkerRegion::Begin(StringFormat::Fmt("Clear depth state for %s", ToStr(orig).c_str()),
                              cmd);

        ImageBarrierSequence setupBarriers;    // , cleanupBarriers;
        state->DiscardContents();
        state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, setupBarriers,
                          GetImageTransitionInfo());
        InlineSetupImageBarriers(cmd, setupBarriers);
        m_setupImageBarriers.Merge(setupBarriers);

        VkClearDepthStencilValue clearval = {1.0f, 0};
        VkImageSubresourceRange range = imageInfo.FullRange();

        ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), ToUnwrappedHandle<VkImage>(live),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1,
                                                &range);

        VkMarkerRegion::End(cmd);

        if(Vulkan_Debug_SingleSubmitFlushing())
        {
          CloseInitStateCmd();
          SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
          SubmitCmds();
          FlushQ();
          SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
        }
      }
      else
      {
        RDCERR("Unexpected initial state tag %u", initial.tag);
      }

      return;
    }

    if(m_CreationInfo.m_Image[id].samples != VK_SAMPLE_COUNT_1_BIT)
    {
      VkCommandBuffer cmd = GetInitStateCmd();

      if(cmd == VK_NULL_HANDLE)
        return;

      VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[id];

      VkFormat fmt = c.format;

      ImageBarrierSequence setupBarriers;    // , cleanupBarriers;
      state->DiscardContents();
      state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        setupBarriers, GetImageTransitionInfo());
      InlineSetupImageBarriers(cmd, setupBarriers);
      m_setupImageBarriers.Merge(setupBarriers);

      VkBuffer buf = initial.buf;

      GetDebugManager()->CopyBufferToTex2DMS(cmd, ToUnwrappedHandle<VkImage>(live), Unwrap(buf),
                                             c.extent, c.arrayLayers, (uint32_t)c.samples, fmt);

      if(Vulkan_Debug_SingleSubmitFlushing())
      {
        CloseInitStateCmd();
        SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
        SubmitCmds();
        FlushQ();
        SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
      }
      return;
    }

    VkBuffer buf = initial.buf;

    VkExtent3D extent = m_CreationInfo.m_Image[id].extent;

    VkFormat fmt = m_CreationInfo.m_Image[id].format;
    uint32_t planeCount = GetYUVPlaneCount(fmt);
    uint32_t horizontalPlaneShift = 0;
    uint32_t verticalPlaneShift = 0;

    VkImageAspectFlags aspectFlags = FormatImageAspects(fmt);

    if(planeCount > 1)
    {
      switch(fmt)
      {
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
          horizontalPlaneShift = verticalPlaneShift = 1;
          break;
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: horizontalPlaneShift = 1; break;
        default: break;
      }
    }

    VkDeviceSize bufOffset = 0;

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(fmt))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, fmt, 0);

    rdcarray<VkBufferImageCopy> copyRegions;
    rdcarray<VkImageSubresourceRange> clearRegions;

    // copy each slice/mip individually
    for(uint32_t a = 0; a < m_CreationInfo.m_Image[id].arrayLayers; a++)
    {
      extent = m_CreationInfo.m_Image[id].extent;

      for(uint32_t m = 0; m < m_CreationInfo.m_Image[id].mipLevels; m++)
      {
        VkBufferImageCopy region = {
            0,
            0,
            0,
            {aspectFlags, m, a, 1},
            {
                0, 0, 0,
            },
            extent,
        };
        VkImageSubresourceRange range = ImageRange(region.imageSubresource);
        InitReqType initReq;

        if(planeCount > 1)
        {
          // need to consider each plane aspect separately
          for(uint32_t i = 0; i < planeCount; i++)
          {
            bufOffset = AlignUp(bufOffset, bufAlignment);

            region.imageExtent = extent;
            region.bufferOffset = bufOffset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT << i;

            if(i > 0)
            {
              region.imageExtent.width >>= horizontalPlaneShift;
              region.imageExtent.height >>= verticalPlaneShift;
            }

            bufOffset += GetPlaneByteSize(extent.width, extent.height, extent.depth, fmt, 0, i);

            if(!initialized)
            {
              initReq = eInitReq_Copy;
            }
            else
            {
              initReq = state->MaxInitReq(range, policy, initialized);
            }

            // you can't clear YUV textures, so force them to be copied either way
            if(initReq == eInitReq_Copy || initReq == eInitReq_Clear)
              copyRegions.push_back(region);
          }
        }
        else if(IsDepthAndStencilFormat(fmt))
        {
          bufOffset = AlignUp(bufOffset, bufAlignment);

          range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

          if(!initialized)
          {
            initReq = eInitReq_Copy;
          }
          else
          {
            initReq = state->MaxInitReq(range, policy, initialized);
          }
          if(initReq == eInitReq_None)
            continue;

          region.bufferOffset = bufOffset;
          region.imageSubresource.aspectMask = range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

          VkFormat sizeFormat = GetDepthOnlyFormat(fmt);

          // pass 0 for mip since we've already pre-downscaled extent
          bufOffset += GetByteSize(extent.width, extent.height, extent.depth, sizeFormat, 0);
          if(initReq == eInitReq_Copy)
            copyRegions.push_back(region);
          else if(initReq == eInitReq_Clear)
            clearRegions.push_back(range);

          // we removed stencil from the format, copy that separately now.
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;
          region.imageSubresource.aspectMask = range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

          bufOffset += GetByteSize(extent.width, extent.height, extent.depth, VK_FORMAT_S8_UINT, 0);

          if(initReq == eInitReq_Copy)
            copyRegions.push_back(region);
          else if(initReq == eInitReq_Clear)
            clearRegions.push_back(range);
        }
        else
        {
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;

          // pass 0 for mip since we've already pre-downscaled extent
          bufOffset += GetByteSize(extent.width, extent.height, extent.depth, fmt, 0);

          if(!initialized)
          {
            initReq = eInitReq_Copy;
          }
          else
          {
            initReq = state->MaxInitReq(range, policy, initialized);
          }

          // you can't clear compressed textures, so fall back to copying them
          if(initReq == eInitReq_Copy ||
             (IsBlockFormat(imageInfo.format) && initReq == eInitReq_Clear))
            copyRegions.push_back(region);
          else if(initReq == eInitReq_Clear)
            clearRegions.push_back(range);
        }

        // update the extent for the next mip
        extent.width = RDCMAX(extent.width >> 1, 1U);
        extent.height = RDCMAX(extent.height >> 1, 1U);
        extent.depth = RDCMAX(extent.depth >> 1, 1U);
      }
    }

    if(copyRegions.size() + clearRegions.size() > 0)
    {
      VkCommandBuffer cmd = GetInitStateCmd();

      VkMarkerRegion::Begin(StringFormat::Fmt("Initial state for %s", ToStr(orig).c_str()), cmd);

      ImageBarrierSequence setupBarriers;
      state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, setupBarriers,
                        GetImageTransitionInfo());
      InlineSetupImageBarriers(cmd, setupBarriers);
      m_setupImageBarriers.Merge(setupBarriers);

      if(copyRegions.size() > 0)
        ObjDisp(cmd)->CmdCopyBufferToImage(
            Unwrap(cmd), Unwrap(buf), ToUnwrappedHandle<VkImage>(live),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copyRegions.size(), copyRegions.data());

      if(clearRegions.size() > 0)
      {
        if(IsDepthOrStencilFormat(fmt))
        {
          VkClearDepthStencilValue val = {0, 0};
          ObjDisp(cmd)->CmdClearDepthStencilImage(
              Unwrap(cmd), ToUnwrappedHandle<VkImage>(live), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              &val, (uint32_t)clearRegions.size(), clearRegions.data());
        }
        else
        {
          VkClearColorValue val;
          memset(&val, 0, sizeof(val));
          ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), ToUnwrappedHandle<VkImage>(live),
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &val,
                                           (uint32_t)clearRegions.size(), clearRegions.data());
        }
      }

      VkMarkerRegion::End(cmd);
    }

    if(Vulkan_Debug_SingleSubmitFlushing())
    {
      CloseInitStateCmd();
      SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
      SubmitCmds();
      FlushQ();
      SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
    }
  }
  else if(type == eResDeviceMemory)
  {
    Intervals<InitReqType> resetReq;
    ResourceId orig = GetResourceManager()->GetOriginalID(id);
    MemRefs *memRefs = GetResourceManager()->FindMemRefs(orig);

    if(!memRefs)
    {
      // No information about the memory usage in the frame.
      // Pessimistically assume the entire memory needs to be reset.
      resetReq.update(0, initial.mem.size, eInitReq_Copy,
                      [](InitReqType x, InitReqType y) -> InitReqType { return RDCMAX(x, y); });
    }
    else
    {
      bool initialized = memRefs->initializedLiveRes == live;
      memRefs->initializedLiveRes = live;
      InitPolicy policy = GetResourceManager()->GetInitPolicy();
      for(auto it = memRefs->rangeRefs.begin(); it != memRefs->rangeRefs.end(); it++)
      {
        InitReqType t = InitReq(it->value(), policy, initialized);
        if(t == eInitReq_Copy)
          resetReq.update(it->start(), it->finish(), eInitReq_Copy,
                          [](InitReqType x, InitReqType y) -> InitReqType { return RDCMAX(x, y); });
        else if(t == eInitReq_Clear)
          resetReq.update(it->start(), it->finish(), eInitReq_Clear,
                          [](InitReqType x, InitReqType y) -> InitReqType { return RDCMAX(x, y); });
      }
    }

    VkBuffer srcBuf = initial.buf;

    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;
    VkDeviceSize dstBufSize = RDCMIN(initial.mem.size, m_CreationInfo.m_Memory[id].wholeMemBufSize);
    if(dstBuf == VK_NULL_HANDLE)
    {
      RDCERR("Whole memory buffer not present for %s", ToStr(orig).c_str());
      return;
    }

    if(resetReq.size() == 1 && resetReq.begin()->value() == eInitReq_None)
    {
      RDCDEBUG("Apply_InitialState (Mem %s): skipped", ToStr(orig).c_str());
      return;    // no copy or clear required
    }

    VkCommandBuffer cmd = GetInitStateCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    VkMarkerRegion::Begin(StringFormat::Fmt("Initial state for %s", ToStr(orig).c_str()), cmd);

    rdcarray<VkBufferCopy> regions;
    uint32_t fillCount = 0;
    for(auto it = resetReq.begin(); it != resetReq.end(); it++)
    {
      if(it->start() >= dstBufSize)
        continue;
      VkDeviceSize start = it->start();
      VkDeviceSize finish = RDCMIN(it->finish(), dstBufSize);
      VkDeviceSize size = finish - start;
      switch(it->value())
      {
        case eInitReq_Clear:
          if(finish >= dstBufSize)
            size = VK_WHOLE_SIZE;
          ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(dstBuf), start, size, 0);
          fillCount++;
          break;
        case eInitReq_Copy: regions.push_back({start, start, size}); break;
        default: break;
      }
    }
    RDCDEBUG("Apply_InitialState (Mem %s): %d fills, %d copies", ToStr(orig).c_str(), fillCount,
             regions.size());
    if(regions.size() > 0)
    {
      VkBufferMemoryBarrier bufBarrier = {
          VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(dstBuf),
          0,
          VK_WHOLE_SIZE,
      };

      // be conservative about whether or not the above fills overlap with the below copies
      DoPipelineBarrier(cmd, 1, &bufBarrier);

      ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf),
                                  (uint32_t)regions.size(), regions.data());
    }

    VkMarkerRegion::End(cmd);

    if(Vulkan_Debug_SingleSubmitFlushing())
    {
      CloseInitStateCmd();
      SubmitCmds();
      FlushQ();
    }
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }
}
