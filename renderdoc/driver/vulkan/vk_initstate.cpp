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
#include "vk_debug.h"

// VKTODOLOW for depth-stencil images we are only save/restoring the depth, not the stencil

// VKTODOLOW there's a lot of duplicated code in this file for creating a buffer to do
// a memory copy and saving to disk.
// VKTODOLOW SerialiseComplexArray not having the ability to serialise into an in-memory
// array means some redundant copies.
// VKTODOLOW The code pattern for creating a few contiguous arrays all in one
// AllocAlignedBuffer for the initial contents buffer is ugly.

// VKTODOLOW in general we do a lot of "create buffer, use it, flush/sync then destroy".
// I don't know what the exact cost is, but it would be nice to batch up the buffers/etc
// used across init state use, and only do a single flush. Also we could then get some
// nice command buffer reuse (although need to be careful we don't create too large a
// command buffer that stalls the GPU).
// See INITSTATEBATCH

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
      for(size_t i = 0; i < layout.bindings.size(); i++)
        initialContents.numDescriptors += layout.bindings[i].descriptorCount;

      initialContents.descriptorSlots = new DescriptorSetSlot[initialContents.numDescriptors];
      RDCEraseMem(initialContents.descriptorSlots,
                  sizeof(DescriptorSetSlot) * initialContents.numDescriptors);

      uint32_t e = 0;
      for(size_t i = 0; i < layout.bindings.size(); i++)
      {
        for(uint32_t b = 0; b < layout.bindings[i].descriptorCount; b++)
        {
          initialContents.descriptorSlots[e++].CreateFrom(record->descInfo->descBindings[i][b]);
        }
      }
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

    VkCommandBuffer extQCmd = VK_NULL_HANDLE;

    ImageLayouts *layout = NULL;
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      layout = &m_ImageLayouts[im->id];
    }

    if(layout->queueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL ||
       layout->queueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT)
    {
      RDCWARN("Image %s in external/foreign queue family, initial contents impossible to fetch.",
              ToStr(im->id).c_str());
      return true;
    }

    VkDevice d = GetDev();
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    if(layout->queueFamilyIndex != m_QueueFamilyIdx)
    {
      // get a command buffer for giving up ownership before the copy and acquiring it afterwards.
      extQCmd = GetExtQueueCmd(layout->queueFamilyIndex);
    }

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

    if(extQCmd != VK_NULL_HANDLE)
    {
      vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsStencilOnlyFormat(imageInfo.format))
    {
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if(IsDepthOrStencilFormat(imageInfo.format))
    {
      aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else if(planeCount > 1)
    {
      aspectFlags = VK_IMAGE_ASPECT_PLANE_0_BIT;
      if(planeCount >= 2)
        aspectFlags |= VK_IMAGE_ASPECT_PLANE_1_BIT;
      if(planeCount >= 3)
        aspectFlags |= VK_IMAGE_ASPECT_PLANE_2_BIT;
    }

    VkImageMemoryBarrier srcimBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        0,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        layout->queueFamilyIndex,
        m_QueueFamilyIdx,
        realim,
        {aspectFlags, 0, (uint32_t)imageInfo.levelCount, 0, (uint32_t)numLayers},
    };

    if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(imageInfo.format))
      srcimBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    // update the real image layout into transfer-source
    srcimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if(arrayIm != VK_NULL_HANDLE)
      srcimBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // ensure all previous writes have completed
    srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
    // before we go reading
    srcimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

    for(size_t si = 0; si < layout->subresourceStates.size(); si++)
    {
      srcimBarrier.subresourceRange = layout->subresourceStates[si].subresourceRange;
      srcimBarrier.oldLayout = layout->subresourceStates[si].newLayout;

      SanitiseOldImageLayout(srcimBarrier.oldLayout);

      DoPipelineBarrier(cmd, 1, &srcimBarrier);

      if(srcimBarrier.srcQueueFamilyIndex != srcimBarrier.dstQueueFamilyIndex)
        DoPipelineBarrier(extQCmd, 1, &srcimBarrier);
    }

    if(extQCmd != VK_NULL_HANDLE)
    {
      vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(extQCmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SubmitAndFlushExtQueue(layout->queueFamilyIndex);
    }

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
          {srcimBarrier.subresourceRange.aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0,
           VK_REMAINING_ARRAY_LAYERS},
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

          bufOffset += GetByteSize(imageInfo.extent.width, imageInfo.extent.height,
                                   imageInfo.extent.depth, sizeFormat, m);

          ObjDisp(d)->CmdCopyImageToBuffer(
              Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(dstBuf), 1, &region);

          if(sizeFormat != imageInfo.format)
          {
            // if we removed stencil from the format, copy that separately now.
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

    // transfer back to whatever it was
    srcimBarrier.oldLayout = srcimBarrier.newLayout;

    // on whatever queue
    std::swap(srcimBarrier.srcQueueFamilyIndex, srcimBarrier.dstQueueFamilyIndex);

    srcimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcimBarrier.dstAccessMask = 0;

    if(extQCmd != VK_NULL_HANDLE)
    {
      vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    for(size_t si = 0; si < layout->subresourceStates.size(); si++)
    {
      srcimBarrier.subresourceRange = layout->subresourceStates[si].subresourceRange;
      srcimBarrier.newLayout = layout->subresourceStates[si].newLayout;
      srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);

      SanitiseNewImageLayout(srcimBarrier.newLayout);

      DoPipelineBarrier(cmd, 1, &srcimBarrier);

      if(srcimBarrier.srcQueueFamilyIndex != srcimBarrier.dstQueueFamilyIndex)
        DoPipelineBarrier(extQCmd, 1, &srcimBarrier);
    }

    if(extQCmd != VK_NULL_HANDLE)
    {
      vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(extQCmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SubmitAndFlushExtQueue(layout->queueFamilyIndex);
    }

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

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
    VkDeviceMemory datamem = ToHandle<VkDeviceMemory>(res);
    VkDeviceSize datasize = record->Length;

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
    VkBuffer srcBuf, dstBuf;

    bufInfo.size = datasize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.size = datasize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &srcBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), srcBuf);
    GetResourceManager()->WrapResource(Unwrap(d), dstBuf);

    MemoryAllocation readbackmem =
        AllocateMemoryForResource(srcBuf, MemoryScope::InitialContents, MemoryType::Readback);

    // dummy request to keep the validation layers happy - the buffers are identical so the
    // requirements must be identical
    {
      VkMemoryRequirements mrq = {0};
      ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(dstBuf), &mrq);
    }

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(srcBuf), datamem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(dstBuf), Unwrap(readbackmem.mem),
                                       readbackmem.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferCopy region = {0, 0, datasize};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), Unwrap(srcBuf), NULL);
    ObjDisp(d)->DestroyBuffer(Unwrap(d), Unwrap(dstBuf), NULL);
    GetResourceManager()->ReleaseWrappedResource(srcBuf);
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
    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);

    RDCASSERT(record->descInfo && record->descInfo->layout);
    const DescSetLayout &layout = *record->descInfo->layout;

    uint32_t NumBindings = 0;

    for(size_t i = 0; i < layout.bindings.size(); i++)
      NumBindings += layout.bindings[i].descriptorCount;

    return 32 + NumBindings * sizeof(DescriptorSetBindingElement);
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
bool WrappedVulkan::Serialise_InitialState(SerialiserType &ser, ResourceId id,
                                           VkResourceRecord *record, const VkInitialContents *initial)
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

    // while writing, fetching binding information from prepared initial contents
    if(ser.IsWriting())
    {
      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      Bindings = initial->descriptorSlots;

      for(size_t i = 0; i < layout.bindings.size(); i++)
        NumBindings += layout.bindings[i].descriptorCount;
    }

    SERIALISE_ELEMENT_ARRAY(Bindings, NumBindings);
    SERIALISE_ELEMENT(NumBindings);

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

      // if we have partially-valid arrays, we need to split up writes. The worst case will never be
      // == number of bindings since that implies all arrays are valid, but it is an upper bound as
      // we'll never need more writes than bindings
      initialContents.descriptorWrites = new VkWriteDescriptorSet[NumBindings];

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Descriptor structs sizes are unexpected, ensure largest size is used");

      VkWriteDescriptorSet *writes = initialContents.descriptorWrites;
      VkDescriptorBufferInfo *dstData = initialContents.descriptorInfo;
      DescriptorSetSlot *srcData = Bindings;

      // validBinds counts up as we make a valid VkWriteDescriptorSet, so can be used to index into
      // writes[] along the way as the 'latest' write.
      uint32_t bind = 0;

      for(uint32_t j = 0; j < initialContents.numDescriptors; j++)
      {
        uint32_t descriptorCount = layout.bindings[j].descriptorCount;

        if(descriptorCount == 0)
          continue;

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

        // quick check for slots that were completely uninitialised and so don't have valid data
        if(descriptorCount == 1 && src->texelBufferView == ResourceId() &&
           src->imageInfo.sampler == ResourceId() && src->imageInfo.imageView == ResourceId() &&
           src->bufferInfo.buffer == ResourceId())
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
                dstImage[d].imageView =
                    GetResourceManager()->GetLiveHandle<VkImageView>(src[d].imageInfo.imageView);
                dstImage[d].sampler =
                    GetResourceManager()->GetLiveHandle<VkSampler>(src[d].imageInfo.sampler);
                dstImage[d].imageLayout = src[d].imageInfo.imageLayout;
              }

              if(immutableSamplers)
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
                dstTexelBuffer[d] =
                    GetResourceManager()->GetLiveHandle<VkBufferView>(src[d].texelBufferView);

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
                dstBuffer[d].buffer =
                    GetResourceManager()->GetLiveHandle<VkBuffer>(src[d].bufferInfo.buffer);
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
            if(IsValid(writes[bind], d - writes[bind].dstArrayElement))
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

      ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs, mappedMem.size, 0,
                            (void **)&Contents);
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
            mappedMem.size,
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
          int numLayers = c.arrayLayers * (int)c.samples;

          VkImageCreateInfo arrayInfo = {
              VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
              NULL,
              VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
              VK_IMAGE_TYPE_2D,
              c.format,
              c.extent,
              (uint32_t)c.mipLevels,
              (uint32_t)numLayers,
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

          VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
          VkFormat fmt = c.format;

          if(IsStencilOnlyFormat(fmt))
            aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
          else if(IsDepthOrStencilFormat(fmt))
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

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

          if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(fmt))
            dstimBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

          DoPipelineBarrier(cmd, 1, &dstimBarrier);

          VkDeviceSize bufOffset = 0;

          // must ensure offset remains valid. Must be multiple of block size, or 4, depending on
          // format
          VkDeviceSize bufAlignment = 4;
          if(IsBlockFormat(fmt))
            bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, fmt, 0);

          std::vector<VkBufferImageCopy> mainCopies, stencilCopies;

          // copy each slice/mip individually
          for(int a = 0; a < numLayers; a++)
          {
            extent = c.extent;

            for(int m = 0; m < c.mipLevels; m++)
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

              bufOffset = AlignUp(bufOffset, bufAlignment);

              region.bufferOffset = bufOffset;

              VkFormat sizeFormat = GetDepthOnlyFormat(fmt);

              // pass 0 for mip since we've already pre-downscaled extent
              bufOffset += GetByteSize(extent.width, extent.height, extent.depth, sizeFormat, 0);

              mainCopies.push_back(region);

              if(sizeFormat != fmt)
              {
                // if we removed stencil from the format, copy that separately now.
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

void WrappedVulkan::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
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

    if(m_ImageLayouts.find(liveid) == m_ImageLayouts.end())
    {
      RDCERR("Couldn't find image info for %llu", id);
      GetResourceManager()->SetInitialContents(
          id, VkInitialContents(type, VkInitialContents::ClearColorImage));
      return;
    }

    ImageLayouts &layouts = m_ImageLayouts[liveid];

    if(layouts.subresourceStates[0].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
      GetResourceManager()->SetInitialContents(
          id, VkInitialContents(type, VkInitialContents::ClearColorImage));
    else
      GetResourceManager()->SetInitialContents(
          id, VkInitialContents(type, VkInitialContents::ClearDepthStencilImage));
  }
  else if(type == eResDeviceMemory)
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
    std::vector<DescriptorSetBindingElement *> &bindings = m_DescriptorSetState[id].currentBindings;

    for(uint32_t i = 0; i < initial.numDescriptors; i++)
    {
      RDCASSERT(writes[i].dstBinding < bindings.size());

      DescriptorSetBindingElement *bind = bindings[writes[i].dstBinding];

      for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
      {
        uint32_t idx = writes[i].dstArrayElement + d;

        if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind[idx].texelBufferView = writes[i].pTexelBufferView[d];
        }
        else if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
          bind[idx].bufferInfo = writes[i].pBufferInfo[d];
        }
        else
        {
          bind[idx].imageInfo = writes[i].pImageInfo[d];
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

    if(initial.tag == VkInitialContents::Sparse)
    {
      Apply_SparseInitialState((WrappedVkImage *)live, initial);
      return;
    }

    // handle any 'created' initial states, without an actual image with contents
    if(initial.tag != VkInitialContents::BufferCopy)
    {
      if(initial.tag == VkInitialContents::ClearColorImage)
      {
        VkFormat format = m_ImageLayouts[id].imageInfo.format;

        if(IsBlockFormat(format) || IsYUVFormat(format))
        {
          RDCWARN(
              "Trying to clear a compressed/YUV image %llu with format %s - should have initial "
              "states or be stripped.",
              id, ToStr(format).c_str());
          return;
        }

        VkCommandBuffer cmd = GetNextCmd();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_ImageLayouts[id].queueFamilyIndex,
            m_QueueFamilyIdx,
            ToHandle<VkImage>(live),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        // finish any pending work before clear
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        // clear completes before subsequent operations
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        VkCommandBuffer extQCmd = VK_NULL_HANDLE;

        if(barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex)
        {
          extQCmd = GetExtQueueCmd(barrier.srcQueueFamilyIndex);

          vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);
        }

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

          SanitiseOldImageLayout(barrier.oldLayout);

          DoPipelineBarrier(cmd, 1, &barrier);

          if(extQCmd != VK_NULL_HANDLE)
            DoPipelineBarrier(extQCmd, 1, &barrier);
        }

        if(extQCmd != VK_NULL_HANDLE)
        {
          vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          SubmitAndFlushExtQueue(barrier.srcQueueFamilyIndex);
        }

        VkClearColorValue clearval = {};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
                                         VK_REMAINING_ARRAY_LAYERS};

        ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), ToHandle<VkImage>(live),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1, &range);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        // complete clear before any other work
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

        std::swap(barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex);

        if(extQCmd != VK_NULL_HANDLE)
        {
          vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);
        }

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          barrier.dstAccessMask |= MakeAccessMask(barrier.newLayout);

          SanitiseNewImageLayout(barrier.newLayout);

          DoPipelineBarrier(cmd, 1, &barrier);

          if(extQCmd != VK_NULL_HANDLE)
            DoPipelineBarrier(extQCmd, 1, &barrier);
        }

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        if(extQCmd != VK_NULL_HANDLE)
        {
          // ensure work is completed before we pass ownership back to original queue
          SubmitCmds();
          FlushQ();

          vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          SubmitAndFlushExtQueue(barrier.dstQueueFamilyIndex);
        }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        SubmitCmds();
#endif
      }
      else if(initial.tag == VkInitialContents::ClearDepthStencilImage)
      {
        VkCommandBuffer cmd = GetNextCmd();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_ImageLayouts[id].queueFamilyIndex,
            m_QueueFamilyIdx,
            ToHandle<VkImage>(live),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        // finish any pending work before clear
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        // clear completes before subsequent operations
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkCommandBuffer extQCmd = VK_NULL_HANDLE;

        if(barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex)
        {
          extQCmd = GetExtQueueCmd(barrier.srcQueueFamilyIndex);

          vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);
        }

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

          SanitiseOldImageLayout(barrier.oldLayout);

          DoPipelineBarrier(cmd, 1, &barrier);

          if(extQCmd != VK_NULL_HANDLE)
            DoPipelineBarrier(extQCmd, 1, &barrier);
        }

        if(extQCmd != VK_NULL_HANDLE)
        {
          vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          SubmitAndFlushExtQueue(barrier.srcQueueFamilyIndex);
        }

        VkClearDepthStencilValue clearval = {1.0f, 0};
        VkImageSubresourceRange range = {barrier.subresourceRange.aspectMask, 0,
                                         VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

        ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), ToHandle<VkImage>(live),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1,
                                                &range);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        // complete clear before any other work
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

        std::swap(barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex);

        if(extQCmd != VK_NULL_HANDLE)
        {
          vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);
        }

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

          SanitiseNewImageLayout(barrier.newLayout);

          DoPipelineBarrier(cmd, 1, &barrier);

          if(extQCmd != VK_NULL_HANDLE)
            DoPipelineBarrier(extQCmd, 1, &barrier);
        }

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        if(extQCmd != VK_NULL_HANDLE)
        {
          // ensure work is completed before we pass ownership back to original queue
          SubmitCmds();
          FlushQ();

          vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          SubmitAndFlushExtQueue(barrier.dstQueueFamilyIndex);
        }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        SubmitCmds();
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

      VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

      VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[id];

      VkFormat fmt = c.format;
      if(IsStencilOnlyFormat(fmt))
        aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
      else if(IsDepthOrStencilFormat(fmt))
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

      if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(fmt))
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

      VkImageMemoryBarrier barrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          0,
          0,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_GENERAL,
          m_ImageLayouts[id].queueFamilyIndex,
          m_QueueFamilyIdx,
          ToHandle<VkImage>(live),
          {aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
      };

      barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
      barrier.dstAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      VkCommandBuffer extQCmd = VK_NULL_HANDLE;

      if(barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex)
      {
        extQCmd = GetExtQueueCmd(barrier.srcQueueFamilyIndex);

        vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
      {
        barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
        barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

        SanitiseOldImageLayout(barrier.oldLayout);

        DoPipelineBarrier(cmd, 1, &barrier);

        if(extQCmd != VK_NULL_HANDLE)
          DoPipelineBarrier(extQCmd, 1, &barrier);
      }

      if(extQCmd != VK_NULL_HANDLE)
      {
        vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        SubmitAndFlushExtQueue(barrier.srcQueueFamilyIndex);
      }

      VkImage arrayIm = initial.img;

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetDebugManager()->CopyArrayToTex2DMS(ToHandle<VkImage>(live), Unwrap(arrayIm), c.extent,
                                            (uint32_t)c.arrayLayers, (uint32_t)c.samples, fmt);

      cmd = GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;

      // complete copy before any other work
      barrier.srcAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

      std::swap(barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex);

      if(extQCmd != VK_NULL_HANDLE)
      {
        vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
      {
        barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
        barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

        SanitiseNewImageLayout(barrier.newLayout);

        barrier.dstAccessMask |= MakeAccessMask(barrier.newLayout);

        DoPipelineBarrier(cmd, 1, &barrier);

        if(extQCmd != VK_NULL_HANDLE)
          DoPipelineBarrier(extQCmd, 1, &barrier);
      }

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(extQCmd != VK_NULL_HANDLE)
      {
        // ensure work is completed before we pass ownership back to original queue
        SubmitCmds();
        FlushQ();

        vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        SubmitAndFlushExtQueue(barrier.dstQueueFamilyIndex);
      }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      SubmitCmds();
#endif
      return;
    }

    VkBuffer buf = initial.buf;

    VkCommandBuffer cmd = GetNextCmd();

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkExtent3D extent = m_CreationInfo.m_Image[id].extent;

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    VkFormat fmt = m_CreationInfo.m_Image[id].format;
    uint32_t planeCount = GetYUVPlaneCount(fmt);
    uint32_t horizontalPlaneShift = 0;
    uint32_t verticalPlaneShift = 0;
    if(IsStencilOnlyFormat(fmt))
    {
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if(IsDepthOrStencilFormat(fmt))
    {
      aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else if(planeCount > 1)
    {
      aspectFlags = VK_IMAGE_ASPECT_PLANE_0_BIT;
      if(planeCount >= 2)
        aspectFlags |= VK_IMAGE_ASPECT_PLANE_1_BIT;
      if(planeCount >= 3)
        aspectFlags |= VK_IMAGE_ASPECT_PLANE_2_BIT;
    }

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

    VkImageMemoryBarrier dstimBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        0,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        m_ImageLayouts[id].queueFamilyIndex,
        m_QueueFamilyIdx,
        ToHandle<VkImage>(live),
        {aspectFlags, 0, 1, 0, (uint32_t)m_CreationInfo.m_Image[id].arrayLayers},
    };

    if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(fmt))
      dstimBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkDeviceSize bufOffset = 0;

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(fmt))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, fmt, 0);

    // first update the live image layout into destination optimal (the initial state
    // image is always and permanently in source optimal already).
    dstimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkCommandBuffer extQCmd = VK_NULL_HANDLE;

    if(dstimBarrier.srcQueueFamilyIndex != dstimBarrier.dstQueueFamilyIndex)
    {
      extQCmd = GetExtQueueCmd(dstimBarrier.srcQueueFamilyIndex);

      vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
    {
      dstimBarrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
      dstimBarrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

      SanitiseOldImageLayout(dstimBarrier.oldLayout);

      dstimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS | MakeAccessMask(dstimBarrier.oldLayout);

      DoPipelineBarrier(cmd, 1, &dstimBarrier);

      if(extQCmd != VK_NULL_HANDLE)
        DoPipelineBarrier(extQCmd, 1, &dstimBarrier);
    }

    if(extQCmd != VK_NULL_HANDLE)
    {
      vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SubmitAndFlushExtQueue(dstimBarrier.srcQueueFamilyIndex);
    }

    // copy each slice/mip individually
    for(int a = 0; a < m_CreationInfo.m_Image[id].arrayLayers; a++)
    {
      extent = m_CreationInfo.m_Image[id].extent;

      for(int m = 0; m < m_CreationInfo.m_Image[id].mipLevels; m++)
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

            bufOffset += GetPlaneByteSize(extent.width, extent.height, extent.depth, fmt, 0, i);

            ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), ToHandle<VkImage>(live),
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
          }
        }
        else
        {
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;

          VkFormat sizeFormat = GetDepthOnlyFormat(fmt);

          // pass 0 for mip since we've already pre-downscaled extent
          bufOffset += GetByteSize(extent.width, extent.height, extent.depth, sizeFormat, 0);

          ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), ToHandle<VkImage>(live),
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

          if(sizeFormat != fmt)
          {
            // if we removed stencil from the format, copy that separately now.
            bufOffset = AlignUp(bufOffset, bufAlignment);

            region.bufferOffset = bufOffset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

            bufOffset += GetByteSize(extent.width, extent.height, extent.depth, VK_FORMAT_S8_UINT, 0);

            ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), ToHandle<VkImage>(live),
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
          }
        }

        // update the extent for the next mip
        extent.width = RDCMAX(extent.width >> 1, 1U);
        extent.height = RDCMAX(extent.height >> 1, 1U);
        extent.depth = RDCMAX(extent.depth >> 1, 1U);
      }
    }

    // update the live image layout back
    dstimBarrier.oldLayout = dstimBarrier.newLayout;

    // make sure the apply completes before any further work
    dstimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstimBarrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

    std::swap(dstimBarrier.srcQueueFamilyIndex, dstimBarrier.dstQueueFamilyIndex);

    if(extQCmd != VK_NULL_HANDLE)
    {
      vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
    {
      dstimBarrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
      dstimBarrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;

      SanitiseNewImageLayout(dstimBarrier.newLayout);

      dstimBarrier.dstAccessMask |= MakeAccessMask(dstimBarrier.newLayout);

      DoPipelineBarrier(cmd, 1, &dstimBarrier);

      if(extQCmd != VK_NULL_HANDLE)
        DoPipelineBarrier(extQCmd, 1, &dstimBarrier);
    }

    vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    if(extQCmd != VK_NULL_HANDLE)
    {
      // ensure work is completed before we pass ownership back to original queue
      SubmitCmds();
      FlushQ();

      vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SubmitAndFlushExtQueue(dstimBarrier.dstQueueFamilyIndex);
    }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitCmds();
#endif
  }
  else if(type == eResDeviceMemory)
  {
    Intervals<InitReqType> resetReq;
    ResourceId orig = GetResourceManager()->GetOriginalID(id);
    MemRefs *memRefs = NULL;
    if(GetResourceManager()->OptimizeInitialState())
      memRefs = GetResourceManager()->FindMemRefs(orig);
    if(!memRefs)
    {
      // No information about the memory usage in the frame.
      // Pessimistically assume the entire memory needs to be reset.
      resetReq.update(0, initial.mem.size, eInitReq_Reset,
                      [](InitReqType x, InitReqType y) -> InitReqType { return std::max(x, y); });
    }
    else
    {
      bool initialized = memRefs->initializedLiveRes == live;
      memRefs->initializedLiveRes = live;
      for(auto it = memRefs->rangeRefs.begin(); it != memRefs->rangeRefs.end(); it++)
      {
        InitReqType t = InitReq(it->value());
        if(t == eInitReq_Reset || (t == eInitReq_InitOnce && !initialized))
          resetReq.update(it->start(), it->finish(), eInitReq_Reset,
                          [](InitReqType x, InitReqType y) -> InitReqType { return std::max(x, y); });
        else if(t == eInitReq_Clear || (t == eInitReq_None && !initialized))
          resetReq.update(it->start(), it->finish(), eInitReq_Clear,
                          [](InitReqType x, InitReqType y) -> InitReqType { return std::max(x, y); });
      }
    }

    VkResult vkr = VK_SUCCESS;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    VkBuffer srcBuf = initial.buf;

    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;
    if(dstBuf == VK_NULL_HANDLE)
    {
      RDCERR("Whole memory buffer not present for %llu", id);
      return;
    }

    if(resetReq.size() == 1 && resetReq.begin()->value() == eInitReq_None)
    {
      RDCDEBUG("Apply_InitialState (Mem %llu): skipped", orig);
      return;    // no copy or clear required
    }

    VkCommandBuffer cmd = GetNextCmd();

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    std::vector<VkBufferCopy> regions;
    uint32_t fillCount = 0;
    for(auto it = resetReq.begin(); it != resetReq.end(); it++)
    {
      if(it->start() >= initial.mem.size)
        continue;
      VkDeviceSize finish = RDCMIN(it->finish(), initial.mem.size);
      VkDeviceSize size = finish - it->start();
      switch(it->value())
      {
        case eInitReq_Clear:
          ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(dstBuf), it->start(), size, 0);
          fillCount++;
          break;
        case eInitReq_Reset: regions.push_back({it->start(), it->start(), size}); break;
        default: break;
      }
    }
    RDCDEBUG("Apply_InitialState (Mem %llu): %d fills, %d copies", orig, fillCount, regions.size());
    if(regions.size() > 0)
      ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf),
                                  (uint32_t)regions.size(), regions.data());

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
