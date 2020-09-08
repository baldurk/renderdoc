/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "vk_core.h"
#include "vk_debug.h"

// VKTODOLOW there's a lot of duplicated code in this file for creating a buffer to do
// a memory copy and saving to disk.

// VKTODOLOW in general we do a lot of "create buffer, use it, flush/sync then destroy".
// I don't know what the exact cost is, but it would be nice to batch up the buffers/etc
// used across init state use, and only do a single flush. Also we could then get some
// nice command buffer reuse (although need to be careful we don't create too large a
// command buffer that stalls the GPU).
// See INITSTATEBATCH

RDOC_DEBUG_CONFIG(
    bool, Vulkan_Debug_HideInitialDescriptors, false,
    "Hide the initial contents of descriptor sets. "
    "For extremely large descriptor sets this can drastically reduce memory consumption.");

bool WrappedVulkan::Prepare_InitialState(WrappedVkRes *res)
{
  ResourceId id = GetResourceManager()->GetID(res);

  VkResourceType type = IdentifyTypeByPtr(res);

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

    return Prepare_SparseInitialState(buffer);
  }
  else if(type == eResImage)
  {
    VkResult vkr = VK_SUCCESS;

    WrappedVkImage *im = (WrappedVkImage *)res;
    const ResourceInfo &resInfo = *im->record->resInfo;
    const ImageInfo &imageInfo = resInfo.imageInfo;

    if(resInfo.IsSparse())
    {
      // if the image is sparse we have to do a different kind of initial state prepare,
      // to serialise out the page mapping. The fetching of memory is also different
      return Prepare_SparseInitialState((WrappedVkImage *)res);
    }

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
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(imageInfo.format))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, imageInfo.format, 0);

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VkImage arrayIm = VK_NULL_HANDLE;

    VkImage realim = im->real.As<VkImage>();
    int numLayers = imageInfo.layerCount;

    if(imageInfo.sampleCount > 1)
    {
      // first decompose to array
      numLayers *= imageInfo.sampleCount;

      VkImageCreateInfo arrayInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
          VK_IMAGE_TYPE_2D, imageInfo.format, imageInfo.extent, (uint32_t)imageInfo.levelCount,
          (uint32_t)numLayers, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
              VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED,
      };

      if(IsDepthOrStencilFormat(imageInfo.format))
        arrayInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      else
        arrayInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

      vkr = ObjDisp(d)->CreateImage(Unwrap(d), &arrayInfo, NULL, &arrayIm);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), arrayIm);

      MemoryAllocation arrayMem =
          AllocateMemoryForResource(arrayIm, MemoryScope::InitialContents, MemoryType::GPULocal);

      vkr = ObjDisp(d)->BindImageMemory(Unwrap(d), Unwrap(arrayIm), Unwrap(arrayMem.mem),
                                        arrayMem.offs);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      // we don't use the memory after this, so we don't need to keep a reference. It's needed for
      // backing the array image only.
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

    VkFormat sizeFormat = GetDepthOnlyFormat(imageInfo.format);

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
    // so we manually create & then just wrap.
    VkBuffer dstBuf;

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), dstBuf);

    MemoryAllocation readbackmem =
        AllocateMemoryForResource(dstBuf, MemoryScope::InitialContents, MemoryType::Readback);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(dstBuf), Unwrap(readbackmem.mem),
                                       readbackmem.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageAspectFlags aspectFlags = FormatImageAspects(imageInfo.format);

    ImageBarrierSequence setupBarriers, cleanupBarriers;

    VkImageLayout readingLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if(arrayIm != VK_NULL_HANDLE)
      readingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    state->TempTransition(m_QueueFamilyIdx, readingLayout,
                          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, setupBarriers,
                          cleanupBarriers, GetImageTransitionInfo());
    InlineSetupImageBarriers(cmd, setupBarriers);
    m_setupImageBarriers.Merge(setupBarriers);
    if(arrayIm != VK_NULL_HANDLE)
    {
      VkImageMemoryBarrier arrayimBarrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          0,
          0,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_GENERAL,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(arrayIm),
          {aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
      };

      DoPipelineBarrier(cmd, 1, &arrayimBarrier);

      vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetDebugManager()->CopyTex2DMSToArray(Unwrap(arrayIm), realim, imageInfo.extent,
                                            imageInfo.layerCount, imageInfo.sampleCount,
                                            imageInfo.format);

      cmd = GetNextCmd();

      vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      arrayimBarrier.srcAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      arrayimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      arrayimBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      arrayimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

      DoPipelineBarrier(cmd, 1, &arrayimBarrier);

      realim = Unwrap(arrayIm);
    }

    VkDeviceSize bufOffset = 0;

    // loop over every slice/mip, copying it to the appropriate point in the buffer
    for(int a = 0; a < numLayers; a++)
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

            ObjDisp(d)->CmdCopyImageToBuffer(Unwrap(cmd), realim,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(dstBuf),
                                             1, &region);
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
              Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(dstBuf), 1, &region);

          if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
          {
            // if we have combined stencil to process, copy that separately now.
            bufOffset = AlignUp(bufOffset, bufAlignment);

            region.bufferOffset = bufOffset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

            bufOffset += GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                     imageInfo.extent.depth, VK_FORMAT_S8_UINT, m);

            ObjDisp(d)->CmdCopyImageToBuffer(Unwrap(cmd), realim,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(dstBuf),
                                             1, &region);
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

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
    SubmitCmds();
    FlushQ();
    SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);

    ObjDisp(d)->DestroyBuffer(Unwrap(d), Unwrap(dstBuf), NULL);
    GetResourceManager()->ReleaseWrappedResource(dstBuf);

    if(arrayIm != VK_NULL_HANDLE)
    {
      ObjDisp(d)->DestroyImage(Unwrap(d), Unwrap(arrayIm), NULL);
      GetResourceManager()->ReleaseWrappedResource(arrayIm);
    }

    GetResourceManager()->SetInitialContents(id, VkInitialContents(type, readbackmem));

    return true;
  }
  else if(type == eResDeviceMemory)
  {
    VkResult vkr = VK_SUCCESS;

    VkDevice d = GetDev();
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
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
    // so we manually create & then just wrap.
    VkBuffer dstBuf;

    bufInfo.size = datasize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), dstBuf);

    MemoryAllocation readbackmem =
        AllocateMemoryForResource(dstBuf, MemoryScope::InitialContents, MemoryType::Readback);

    // dummy request to keep the validation layers happy - the buffers are identical so the
    // requirements must be identical
    {
      VkMemoryRequirements mrq = {0};
      ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(dstBuf), &mrq);
    }

    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(dstBuf), Unwrap(readbackmem.mem),
                                       readbackmem.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferCopy region = {0, 0, datasize};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), Unwrap(record->memMapState->wholeMemBuf), Unwrap(dstBuf),
                              1, &region);

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), Unwrap(dstBuf), NULL);
    GetResourceManager()->ReleaseWrappedResource(dstBuf);

    GetResourceManager()->SetInitialContents(id, VkInitialContents(type, readbackmem));

    return true;
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }

  return false;
}

uint64_t WrappedVulkan::GetSize_InitialState(ResourceId id, const VkInitialContents &initial)
{
  if(initial.type == eResDescriptorSet)
  {
    return 32 + initial.numDescriptors * sizeof(DescriptorSetSlot) + initial.inlineByteSize;
  }
  else if(initial.type == eResBuffer)
  {
    // buffers only have initial states when they're sparse
    return GetSize_SparseInitialState(id, initial);
  }
  else if(initial.type == eResImage || initial.type == eResDeviceMemory)
  {
    if(initial.tag == VkInitialContents::Sparse)
      return GetSize_SparseInitialState(id, initial);

    // the size primarily comes from the buffer, the size of which we conveniently have stored.
    return uint64_t(128 + initial.mem.size + WriteSerialiser::GetChunkAlignment());
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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_InitialState(SerialiserType &ser, ResourceId id, VkResourceRecord *,
                                           const VkInitialContents *initial)
{
  bool ret = true;

  SERIALISE_ELEMENT_LOCAL(type, initial->type);
  SERIALISE_ELEMENT(id).TypedAs(NameOfType(type));

  if(IsReplayingAndReading())
  {
    AddResourceCurChunk(id);
  }

  if(type == eResDescriptorSet)
  {
    DescriptorSetSlot *Bindings = NULL;
    uint32_t NumBindings = 0;
    bytebuf InlineData;

    const bool hide = Vulkan_Debug_HideInitialDescriptors();

    if(hide)
      ser.PushInternal();

    // while writing, fetching binding information from prepared initial contents
    if(ser.IsWriting())
    {
      Bindings = initial->descriptorSlots;
      NumBindings = initial->numDescriptors;

      InlineData.assign(initial->inlineData, initial->inlineByteSize);
    }

    SERIALISE_ELEMENT_ARRAY(Bindings, NumBindings);
    SERIALISE_ELEMENT(NumBindings);

    if(ser.VersionAtLeast(0x12))
    {
      SERIALISE_ELEMENT(InlineData);
    }

    if(hide)
      ser.PopInternal();

    SERIALISE_CHECK_READ_ERRORS();

    // while reading, fetch the binding information and allocate a VkWriteDescriptorSet array
    if(IsReplayingAndReading())
    {
      WrappedVkRes *res = GetResourceManager()->GetLiveResource(id);
      ResourceId liveid = GetResourceManager()->GetLiveID(id);

      const DescSetLayout &layout =
          m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[liveid].layout];

      if(layout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
      {
        RDCERR("Push descriptor set with initial contents!");
        return true;
      }

      VkInitialContents initialContents(type, VkInitialContents::DescriptorSet);

      initialContents.numDescriptors = (uint32_t)layout.bindings.size();
      initialContents.descriptorInfo = new VkDescriptorBufferInfo[NumBindings];
      initialContents.inlineInfo = NULL;

      if(layout.inlineCount > 0)
      {
        initialContents.inlineInfo =
            new VkWriteDescriptorSetInlineUniformBlockEXT[layout.inlineCount];
        initialContents.inlineData = AllocAlignedBuffer(InlineData.size());
        RDCASSERTEQUAL(layout.inlineByteSize, InlineData.size());
        memcpy(initialContents.inlineData, InlineData.data(), InlineData.size());
      }

      // if we have partially-valid arrays, we need to split up writes. The worst case will never be
      // == number of bindings since that implies all arrays are valid, but it is an upper bound as
      // we'll never need more writes than bindings
      initialContents.descriptorWrites = new VkWriteDescriptorSet[NumBindings];

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Descriptor structs sizes are unexpected, ensure largest size is used");

      VkWriteDescriptorSet *writes = initialContents.descriptorWrites;
      VkDescriptorBufferInfo *dstData = initialContents.descriptorInfo;
      VkWriteDescriptorSetInlineUniformBlockEXT *dstInline = initialContents.inlineInfo;
      DescriptorSetSlot *srcData = Bindings;

      byte *srcInlineData = initialContents.inlineData;

      // validBinds counts up as we make a valid VkWriteDescriptorSet, so can be used to index into
      // writes[] along the way as the 'latest' write.
      uint32_t bind = 0;

      for(uint32_t j = 0; j < initialContents.numDescriptors; j++)
      {
        uint32_t descriptorCount = layout.bindings[j].descriptorCount;

        if(layout.bindings[j].variableSize)
          descriptorCount = m_DescriptorSetState[liveid].data.variableDescriptorCount;

        if(descriptorCount == 0)
          continue;

        uint32_t inlineSize = 0;

        if(layout.bindings[j].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          inlineSize = descriptorCount;
          descriptorCount = 1;
        }

        writes[bind].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[bind].pNext = NULL;

        // template for this write. We will expand it to include more descriptors as we find valid
        // descriptors to update.
        writes[bind].dstSet = (VkDescriptorSet)(uint64_t)res;
        writes[bind].dstBinding = j;
        writes[bind].dstArrayElement = 0;
        // descriptor count starts at 0. We increment it as we find valid descriptors
        writes[bind].descriptorCount = 0;
        writes[bind].descriptorType = layout.bindings[j].descriptorType;

        ResourceId *immutableSamplers = layout.bindings[j].immutableSampler;

        DescriptorSetSlot *src = srcData;
        srcData += descriptorCount;

        // will be cast to the appropriate type, we just need to increment
        // the dstData pointer by worst case size
        VkDescriptorBufferInfo *dstBuffer = dstData;
        VkDescriptorImageInfo *dstImage = (VkDescriptorImageInfo *)dstData;
        VkBufferView *dstTexelBuffer = (VkBufferView *)dstData;
        dstData += descriptorCount;

        RDCCOMPILE_ASSERT(
            sizeof(VkDescriptorImageInfo) <= sizeof(VkDescriptorBufferInfo),
            "VkDescriptorBufferInfo should be large enough for all descriptor write types");
        RDCCOMPILE_ASSERT(
            sizeof(VkBufferView) <= sizeof(VkDescriptorBufferInfo),
            "VkDescriptorBufferInfo should be large enough for all descriptor write types");

        // the correct one will be set below
        writes[bind].pBufferInfo = NULL;
        writes[bind].pImageInfo = NULL;
        writes[bind].pTexelBufferView = NULL;

        // check that the resources we need for this write are present, as some might have been
        // skipped due to stale descriptor set slots or otherwise unreferenced objects (the
        // descriptor set initial contents do not cause a frame reference for their resources).
        //
        // For the non-array case it's trivial as either the descriptor is valid, in which case it
        // gets a write, or not, in which case we skip.
        // For the array case we batch up updates as much as possible, iterating along the array and
        // skipping any invalid descriptors.

        if(writes[bind].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          // handle inline uniform block specially because the descriptorCount doesn't mean what it
          // normally means in the write.

          dstInline->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
          dstInline->pNext = NULL;
          dstInline->pData = srcInlineData + src->inlineOffset;
          dstInline->dataSize = inlineSize;

          writes[bind].pNext = dstInline;
          writes[bind].descriptorCount = inlineSize;
          bind++;

          dstInline++;
        }
        // quick check for slots that were completely uninitialised and so don't have valid data
        else if(!NULLDescriptorsAllowed() && descriptorCount == 1 &&
                src->texelBufferView == ResourceId() && src->imageInfo.sampler == ResourceId() &&
                src->imageInfo.imageView == ResourceId() && src->bufferInfo.buffer == ResourceId())
        {
          // do nothing - don't increment bind so that the same write descriptor is used next time.
          continue;
        }
        else
        {
          // first we copy the right data over unconditionally
          switch(writes[bind].descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
              for(uint32_t d = 0; d < descriptorCount; d++)
              {
                if(writes[bind].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER &&
                   GetResourceManager()->HasLiveResource(src[d].imageInfo.imageView))
                  dstImage[d].imageView =
                      GetResourceManager()->GetLiveHandle<VkImageView>(src[d].imageInfo.imageView);
                else
                  dstImage[d].imageView = VK_NULL_HANDLE;

                if((writes[bind].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                    writes[bind].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
                   GetResourceManager()->HasLiveResource(src[d].imageInfo.sampler))
                  dstImage[d].sampler =
                      GetResourceManager()->GetLiveHandle<VkSampler>(src[d].imageInfo.sampler);
                else
                  dstImage[d].sampler = VK_NULL_HANDLE;

                dstImage[d].imageLayout = src[d].imageInfo.imageLayout;
              }

              // if we're not updating a SAMPLER descriptor fill in immutable samplers so that our
              // validity checking doesn't have to look them up.
              if(immutableSamplers && writes[bind].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER)
              {
                for(uint32_t d = 0; d < descriptorCount; d++)
                  dstImage[d].sampler =
                      GetResourceManager()->GetCurrentHandle<VkSampler>(immutableSamplers[d]);
              }

              writes[bind].pImageInfo = dstImage;
              // NULL the others
              dstBuffer = NULL;
              dstTexelBuffer = NULL;
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
              for(uint32_t d = 0; d < descriptorCount; d++)
              {
                if(GetResourceManager()->HasLiveResource(src[d].texelBufferView))
                  dstTexelBuffer[d] =
                      GetResourceManager()->GetLiveHandle<VkBufferView>(src[d].texelBufferView);
                else
                  dstTexelBuffer[d] = VK_NULL_HANDLE;
              }

              writes[bind].pTexelBufferView = dstTexelBuffer;
              // NULL the others
              dstBuffer = NULL;
              dstImage = NULL;
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
              for(uint32_t d = 0; d < descriptorCount; d++)
              {
                if(GetResourceManager()->HasLiveResource(src[d].bufferInfo.buffer))
                  dstBuffer[d].buffer =
                      GetResourceManager()->GetLiveHandle<VkBuffer>(src[d].bufferInfo.buffer);
                else
                  dstBuffer[d].buffer = VK_NULL_HANDLE;
                dstBuffer[d].offset = src[d].bufferInfo.offset;
                dstBuffer[d].range = src[d].bufferInfo.range;
              }

              writes[bind].pBufferInfo = dstBuffer;
              // NULL the others
              dstImage = NULL;
              dstTexelBuffer = NULL;
              break;
            }
            default:
            {
              RDCERR("Unexpected descriptor type %d", writes[bind].descriptorType);
              ret = false;
            }
          }

          // iterate over all the descriptors coalescing valid writes. At all times writes[bind] is
          // the 'current' batched update
          for(uint32_t d = 0; d < descriptorCount; d++)
          {
            // is this array element in the write valid? Note that below when we encounter an
            // invalid write, the next one starts from a later point in the array, so we need to
            // check relative to the dstArrayElement
            if(IsValid(NULLDescriptorsAllowed(), writes[bind], d - writes[bind].dstArrayElement))
            {
              // if this descriptor is valid, just increment the number of descriptors. The data
              // and dstArrayElement is pointing to the start of the valid range
              writes[bind].descriptorCount++;
            }
            else
            {
              // if this descriptor is *invalid* we must skip it. First see if we have some
              // previously valid range and commit it
              if(writes[bind].descriptorCount)
              {
                bind++;

                // copy over the previous data for the sake of the things that won't be reset below
                writes[bind] = writes[bind - 1];
              }

              // now offset to the next potentially valid descriptor. Note that at the end of the
              // iteration there is no next descriptor so these pointer values will be off the end
              // of the array, but descriptorCount will be 0 so this will be treated as invalid and
              // skipped
              writes[bind].dstArrayElement = d + 1;

              // start counting from 0 again
              writes[bind].descriptorCount = 0;

              // offset the array being used
              if(dstBuffer)
                writes[bind].pBufferInfo = dstBuffer + d + 1;
              else if(dstImage)
                writes[bind].pImageInfo = dstImage + d + 1;
              else if(dstTexelBuffer)
                writes[bind].pTexelBufferView = dstTexelBuffer + d + 1;
            }
          }

          // after the loop there may be a valid write which hasn't been accounted for. If the
          // current write has a descriptor count that means it has some descriptors, so
          // increment i and validBinds so that it's accounted for.
          if(writes[bind].descriptorCount)
            bind++;
        }
      }

      initialContents.numDescriptors = bind;

      GetResourceManager()->SetInitialContents(id, initialContents);
    }
  }
  else if(type == eResBuffer)
  {
    // buffers only have initial states when they're sparse
    return Serialise_SparseBufferInitialState(ser, id, initial);
  }
  else if(type == eResDeviceMemory || type == eResImage)
  {
    VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;

    // if we have a blob of data, this contains sparse mapping so re-direct to the sparse
    // implementation of this function
    SERIALISE_ELEMENT_LOCAL(IsSparse, initial && initial->tag == VkInitialContents::Sparse);

    if(IsSparse)
    {
      ret = false;

      if(type == eResImage)
      {
        ret = Serialise_SparseImageInitialState(ser, id, initial);
      }
      else
      {
        RDCERR("Invalid initial state - sparse marker for device memory");
        ret = false;
      }

      return ret;
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
        mappedMem = initial->mem;
        vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), initial->mem.offs,
                                    initial->mem.size, 0, (void **)&Contents);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        // invalidate the cpu cache for this memory range to avoid reading stale data
        VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            NULL,
            Unwrap(mappedMem.mem),
            mappedMem.offs,
            mappedMem.size,
        };

        vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }
    }
    else if(IsReplayingAndReading() && !ser.IsErrored())
    {
      // create a buffer with memory attached, which we will fill with the initial contents
      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          NULL,
          0,
          ContentsSize,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      };

      vkr = vkCreateBuffer(d, &bufInfo, NULL, &uploadBuf);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      uploadMemory =
          AllocateMemoryForResource(uploadBuf, MemoryScope::InitialContents, MemoryType::Upload);

      vkr = vkBindBufferMemory(d, uploadBuf, uploadMemory.mem, uploadMemory.offs);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      mappedMem = uploadMemory;

      ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs,
                            AlignUp(mappedMem.size, nonCoherentAtomSize), 0, (void **)&Contents);
    }

    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("Contents"_lit, Contents, ContentsSize, SerialiserFlags::NoFlags);

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
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
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

        VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[liveid];

        // for non-MSAA images, we're done - we'll do buffer-to-image copies with appropriate
        // offsets to copy out the subresources into the image itself.
        if(c.samples == VK_SAMPLE_COUNT_1_BIT)
        {
          initialContents.buf = uploadBuf;
        }
        else
        {
          // MSAA textures we upload into an array image, then the apply does an array-to-MSAA copy
          // instead of the usual buffer-to-image copies.
          uint32_t numLayers = c.arrayLayers * (uint32_t)c.samples;

          VkImageCreateInfo arrayInfo = {
              VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
              NULL,
              VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
              VK_IMAGE_TYPE_2D,
              c.format,
              c.extent,
              c.mipLevels,
              numLayers,
              VK_SAMPLE_COUNT_1_BIT,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              VK_SHARING_MODE_EXCLUSIVE,
              0,
              NULL,
              VK_IMAGE_LAYOUT_UNDEFINED,
          };

          VkImage arrayIm;

          vkr = vkCreateImage(d, &arrayInfo, NULL, &arrayIm);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          MemoryAllocation arrayMem =
              AllocateMemoryForResource(arrayIm, MemoryScope::InitialContents, MemoryType::GPULocal);

          vkr = vkBindImageMemory(d, arrayIm, arrayMem.mem, arrayMem.offs);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          VkCommandBuffer cmd = GetNextCmd();

          VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

          vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          VkExtent3D extent = c.extent;

          VkFormat fmt = c.format;
          VkImageAspectFlags aspectFlags = FormatImageAspects(fmt);

          VkImageMemoryBarrier dstimBarrier = {
              VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
              NULL,
              0,
              0,
              VK_IMAGE_LAYOUT_UNDEFINED,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_QUEUE_FAMILY_IGNORED,
              VK_QUEUE_FAMILY_IGNORED,
              Unwrap(arrayIm),
              {aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

          DoPipelineBarrier(cmd, 1, &dstimBarrier);

          VkDeviceSize bufOffset = 0;

          // must ensure offset remains valid. Must be multiple of block size, or 4, depending on
          // format
          VkDeviceSize bufAlignment = 4;
          if(IsBlockFormat(fmt))
            bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, fmt, 0);

          rdcarray<VkBufferImageCopy> mainCopies, stencilCopies;

          // copy each slice/mip individually
          for(uint32_t a = 0; a < numLayers; a++)
          {
            extent = c.extent;

            for(uint32_t m = 0; m < c.mipLevels; m++)
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

              bufOffset = AlignUp(bufOffset, bufAlignment);

              region.bufferOffset = bufOffset;

              // for depth/stencil copies, copy depth first
              if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

              VkFormat sizeFormat = GetDepthOnlyFormat(fmt);

              // pass 0 for mip since we've already pre-downscaled extent
              bufOffset += GetByteSize(extent.width, extent.height, extent.depth, sizeFormat, 0);

              mainCopies.push_back(region);

              if(aspectFlags == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
              {
                // if it's a depth/stencil format, copy stencil now
                bufOffset = AlignUp(bufOffset, bufAlignment);

                region.bufferOffset = bufOffset;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

                bufOffset +=
                    GetByteSize(extent.width, extent.height, extent.depth, VK_FORMAT_S8_UINT, 0);

                stencilCopies.push_back(region);
              }

              // update the extent for the next mip
              extent.width = RDCMAX(extent.width >> 1, 1U);
              extent.height = RDCMAX(extent.height >> 1, 1U);
              extent.depth = RDCMAX(extent.depth >> 1, 1U);
            }
          }

          ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(uploadBuf), Unwrap(arrayIm),
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             (uint32_t)mainCopies.size(), &mainCopies[0]);

          if(!stencilCopies.empty())
            ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(uploadBuf), Unwrap(arrayIm),
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                               (uint32_t)stencilCopies.size(), &stencilCopies[0]);

          // once transfers complete, get ready for copy array->ms
          dstimBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          dstimBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          dstimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          dstimBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

          DoPipelineBarrier(cmd, 1, &dstimBarrier);

          vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          // INITSTATEBATCH
          SubmitCmds();
          FlushQ();

          // destroy the buffer as it's no longer needed.
          vkDestroyBuffer(d, uploadBuf, NULL);
          FreeMemoryAllocation(uploadMemory);

          initialContents.buf = VK_NULL_HANDLE;
          initialContents.img = arrayIm;
          initialContents.mem = arrayMem;
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

std::map<uint32_t, rdcarray<VkImageMemoryBarrier> > GetExtQBarriers(
    const rdcarray<VkImageMemoryBarrier> &barriers)
{
  std::map<uint32_t, rdcarray<VkImageMemoryBarrier> > extQBarriers;

  for(auto barrierIt = barriers.begin(); barrierIt != barriers.end(); ++barrierIt)
  {
    if(barrierIt->srcQueueFamilyIndex != barrierIt->dstQueueFamilyIndex)
    {
      extQBarriers[barrierIt->srcQueueFamilyIndex].push_back(*barrierIt);
    }
  }
  return extQBarriers;
}

void WrappedVulkan::Apply_InitialState(WrappedVkRes *live, const VkInitialContents &initial)
{
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

      if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
      {
        VkWriteDescriptorSetInlineUniformBlockEXT *inlineWrite =
            (VkWriteDescriptorSetInlineUniformBlockEXT *)FindNextStruct(
                &writes[i], VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT);
        memcpy(inlineData.data() + bind->inlineOffset + writes[i].dstArrayElement,
               inlineWrite->pData, inlineWrite->dataSize);
        continue;
      }

      for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
      {
        uint32_t idx = writes[i].dstArrayElement + d;

        if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind[idx].texelBufferView = GetResID(writes[i].pTexelBufferView[d]);
        }
        else if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
          bind[idx].bufferInfo.SetFrom(writes[i].pBufferInfo[d]);
        }
        else
        {
          // we don't ever pass invalid parameters so we can unconditionally set both. Invalid
          // elements are set to VK_NULL_HANDLE which is safe
          bind[idx].imageInfo.SetFrom(writes[i].pImageInfo[d], true, true);
        }
      }
    }
  }
  else if(type == eResBuffer)
  {
    Apply_SparseInitialState((WrappedVkBuffer *)live, initial);
  }
  else if(type == eResImage)
  {
    VkResult vkr = VK_SUCCESS;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

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

    if(initialized && boundMemory != ResourceId())
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

    if(initial.tag == VkInitialContents::Sparse)
    {
      Apply_SparseInitialState((WrappedVkImage *)live, initial);
      return;
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

        VkCommandBuffer cmd = GetNextCmd();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        ImageBarrierSequence setupBarriers;
        state->DiscardContents();
        state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                          VK_ACCESS_TRANSFER_WRITE_BIT, setupBarriers, GetImageTransitionInfo());
        InlineSetupImageBarriers(cmd, setupBarriers);
        m_setupImageBarriers.Merge(setupBarriers);

        VkClearColorValue clearval = {};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
                                         VK_REMAINING_ARRAY_LAYERS};

        ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), ToUnwrappedHandle<VkImage>(live),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1, &range);

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
#if ENABLED(SINGLE_FLUSH_VALIDATE)
        SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
        SubmitCmds();
        FlushQ();
        SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
#endif
      }
      else if(initial.tag == VkInitialContents::ClearDepthStencilImage)
      {
        VkCommandBuffer cmd = GetNextCmd();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        ImageBarrierSequence setupBarriers;    // , cleanupBarriers;
        state->DiscardContents();
        state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                          VK_ACCESS_TRANSFER_WRITE_BIT, setupBarriers, GetImageTransitionInfo());
        InlineSetupImageBarriers(cmd, setupBarriers);
        m_setupImageBarriers.Merge(setupBarriers);

        VkClearDepthStencilValue clearval = {1.0f, 0};
        VkImageSubresourceRange range = imageInfo.FullRange();

        ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), ToUnwrappedHandle<VkImage>(live),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1,
                                                &range);

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
#if ENABLED(SINGLE_FLUSH_VALIDATE)
        SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
        SubmitCmds();
        FlushQ();
        SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
#endif
      }
      else
      {
        RDCERR("Unexpected initial state tag %u", initial.tag);
      }

      return;
    }

    if(m_CreationInfo.m_Image[id].samples != VK_SAMPLE_COUNT_1_BIT)
    {
      VkCommandBuffer cmd = GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[id];

      VkFormat fmt = c.format;

      ImageBarrierSequence setupBarriers;    // , cleanupBarriers;
      state->DiscardContents();
      state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_GENERAL, 0,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        setupBarriers, GetImageTransitionInfo());
      InlineSetupImageBarriers(cmd, setupBarriers);
      m_setupImageBarriers.Merge(setupBarriers);

      VkImage arrayIm = initial.img;

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetDebugManager()->CopyArrayToTex2DMS(ToUnwrappedHandle<VkImage>(live), Unwrap(arrayIm),
                                            c.extent, c.arrayLayers, (uint32_t)c.samples, fmt);

      cmd = GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
#if ENABLED(SINGLE_FLUSH_VALIDATE)
      SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
      SubmitCmds();
      FlushQ();
      SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
#endif
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
      VkCommandBuffer cmd = GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkMarkerRegion::Begin(StringFormat::Fmt("Initial state for %s", ToStr(orig).c_str()), cmd);

      ImageBarrierSequence setupBarriers;
      state->Transition(m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                        VK_ACCESS_TRANSFER_WRITE_BIT, setupBarriers, GetImageTransitionInfo());
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

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitAndFlushImageStateBarriers(m_setupImageBarriers);
    SubmitCmds();
    FlushQ();
    SubmitAndFlushImageStateBarriers(m_cleanupImageBarriers);
#endif
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

    VkResult vkr = VK_SUCCESS;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    VkBuffer srcBuf = initial.buf;

    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;
    if(dstBuf == VK_NULL_HANDLE)
    {
      RDCERR("Whole memory buffer not present for %s", ToStr(id).c_str());
      return;
    }

    if(resetReq.size() == 1 && resetReq.begin()->value() == eInitReq_None)
    {
      RDCDEBUG("Apply_InitialState (Mem %s): skipped", ToStr(orig).c_str());
      return;    // no copy or clear required
    }

    VkCommandBuffer cmd = GetNextCmd();

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMarkerRegion::Begin(StringFormat::Fmt("Initial state for %s", ToStr(orig).c_str()), cmd);

    rdcarray<VkBufferCopy> regions;
    uint32_t fillCount = 0;
    for(auto it = resetReq.begin(); it != resetReq.end(); it++)
    {
      if(it->start() >= initial.mem.size)
        continue;
      VkDeviceSize start = it->start();
      VkDeviceSize finish = RDCMIN(it->finish(), initial.mem.size);
      VkDeviceSize size = finish - start;
      switch(it->value())
      {
        case eInitReq_Clear:
          if(finish >= initial.mem.size)
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
      ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf),
                                  (uint32_t)regions.size(), regions.data());

    VkMarkerRegion::End(cmd);

    vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitCmds();
#endif
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }
}
