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

    VkInitialContents initialContents(type, VK_NULL_HANDLE, 0);

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
        initialContents.descriptorSlots[e++] = record->descInfo->descBindings[i][b];
      }
    }

    GetResourceManager()->SetInitialContents(id, initialContents);
    return true;
  }
  else if(type == eResBuffer)
  {
    WrappedVkBuffer *buffer = (WrappedVkBuffer *)res;

    // buffers are only dirty if they are sparse
    RDCASSERT(buffer->record->sparseInfo);

    return Prepare_SparseInitialState(buffer);
  }
  else if(type == eResImage)
  {
    VkResult vkr = VK_SUCCESS;

    WrappedVkImage *im = (WrappedVkImage *)res;

    if(im->record->sparseInfo)
    {
      // if the image is sparse we have to do a different kind of initial state prepare,
      // to serialise out the page mapping. The fetching of memory is also different
      return Prepare_SparseInitialState((WrappedVkImage *)res);
    }

    VkDevice d = GetDev();
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    ImageLayouts *layout = NULL;
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      layout = &m_ImageLayouts[im->id];
    }

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(layout->format))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, layout->format, 0);

    VkDeviceMemory readbackmem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VkImage arrayIm = VK_NULL_HANDLE;
    VkDeviceMemory arrayMem = VK_NULL_HANDLE;

    VkImage realim = im->real.As<VkImage>();
    int numLayers = layout->layerCount;

    if(layout->sampleCount > 1)
    {
      // first decompose to array
      numLayers *= layout->sampleCount;

      VkImageCreateInfo arrayInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
          VK_IMAGE_TYPE_2D, layout->format, layout->extent, (uint32_t)layout->levelCount,
          (uint32_t)numLayers, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
              VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED,
      };

      if(IsDepthOrStencilFormat(layout->format))
        arrayInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      else
        arrayInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

      vkr = ObjDisp(d)->CreateImage(Unwrap(d), &arrayInfo, NULL, &arrayIm);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkMemoryRequirements mrq = {0};

      ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), arrayIm, &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &arrayMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      vkr = ObjDisp(d)->BindImageMemory(Unwrap(d), arrayIm, arrayMem, 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    VkFormat sizeFormat = GetDepthOnlyFormat(layout->format);

    for(int a = 0; a < numLayers; a++)
    {
      for(int m = 0; m < layout->levelCount; m++)
      {
        bufInfo.size = AlignUp(bufInfo.size, bufAlignment);

        bufInfo.size += GetByteSize(layout->extent.width, layout->extent.height,
                                    layout->extent.depth, sizeFormat, m);

        if(sizeFormat != layout->format)
        {
          // if there's stencil and depth, allocate space for stencil
          bufInfo.size = AlignUp(bufInfo.size, bufAlignment);

          bufInfo.size += GetByteSize(layout->extent.width, layout->extent.height,
                                      layout->extent.depth, VK_FORMAT_S8_UINT, m);
        }
      }
    }

    // since this is very short lived, it is not wrapped
    VkBuffer dstBuf;

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &readbackmem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsStencilOnlyFormat(layout->format))
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(IsDepthOrStencilFormat(layout->format))
      aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

    VkImageMemoryBarrier srcimBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        0,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        realim,
        {aspectFlags, 0, (uint32_t)layout->levelCount, 0, (uint32_t)numLayers}};

    if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(layout->format))
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
      DoPipelineBarrier(cmd, 1, &srcimBarrier);
    }

    if(arrayIm != VK_NULL_HANDLE)
    {
      VkImageMemoryBarrier arrayimBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                             NULL,
                                             0,
                                             0,
                                             VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             arrayIm,
                                             {srcimBarrier.subresourceRange.aspectMask, 0,
                                              VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

      DoPipelineBarrier(cmd, 1, &arrayimBarrier);

      vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetDebugManager()->CopyTex2DMSToArray(arrayIm, realim, layout->extent, layout->layerCount,
                                            layout->sampleCount, layout->format);

      cmd = GetNextCmd();

      vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      arrayimBarrier.srcAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      arrayimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      arrayimBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      arrayimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

      DoPipelineBarrier(cmd, 1, &arrayimBarrier);

      realim = arrayIm;
    }

    VkDeviceSize bufOffset = 0;

    // loop over every slice/mip, copying it to the appropriate point in the buffer
    for(int a = 0; a < numLayers; a++)
    {
      VkExtent3D extent = layout->extent;

      for(int m = 0; m < layout->levelCount; m++)
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

        bufOffset += GetByteSize(layout->extent.width, layout->extent.height, layout->extent.depth,
                                 sizeFormat, m);

        ObjDisp(d)->CmdCopyImageToBuffer(Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         dstBuf, 1, &region);

        if(sizeFormat != layout->format)
        {
          // if we removed stencil from the format, copy that separately now.
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;
          region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

          bufOffset += GetByteSize(layout->extent.width, layout->extent.height,
                                   layout->extent.depth, VK_FORMAT_S8_UINT, m);

          ObjDisp(d)->CmdCopyImageToBuffer(
              Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuf, 1, &region);
        }

        // update the extent for the next mip
        extent.width = RDCMAX(extent.width >> 1, 1U);
        extent.height = RDCMAX(extent.height >> 1, 1U);
        extent.depth = RDCMAX(extent.depth >> 1, 1U);
      }
    }

    RDCASSERTMSG("buffer wasn't sized sufficiently!", bufOffset <= bufInfo.size, bufOffset,
                 mrq.size, layout->extent, layout->format, numLayers, layout->levelCount);

    // transfer back to whatever it was
    srcimBarrier.oldLayout = srcimBarrier.newLayout;

    srcimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcimBarrier.dstAccessMask = 0;

    for(size_t si = 0; si < layout->subresourceStates.size(); si++)
    {
      srcimBarrier.subresourceRange = layout->subresourceStates[si].subresourceRange;
      srcimBarrier.newLayout = layout->subresourceStates[si].newLayout;
      srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);
      DoPipelineBarrier(cmd, 1, &srcimBarrier);
    }

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL);

    if(arrayIm != VK_NULL_HANDLE)
    {
      ObjDisp(d)->DestroyImage(Unwrap(d), arrayIm, NULL);
      ObjDisp(d)->FreeMemory(Unwrap(d), arrayMem, NULL);
    }

    GetResourceManager()->SetInitialContents(id, VkInitialContents(type, readbackmem, mrq.size));

    return true;
  }
  else if(type == eResDeviceMemory)
  {
    VkResult vkr = VK_SUCCESS;

    VkDevice d = GetDev();
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    VkDeviceSize dataoffs = 0;
    VkDeviceMemory datamem = ToHandle<VkDeviceMemory>(res);
    VkDeviceSize datasize = record->Length;

    RDCASSERT(datamem != VK_NULL_HANDLE);

    RDCASSERT(record->Length > 0);
    VkDeviceSize memsize = record->Length;

    VkDeviceMemory readbackmem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    // since these are very short lived, they are not wrapped
    VkBuffer srcBuf, dstBuf;

    // dstBuf is just over the allocated memory, so only the image's size
    bufInfo.size = datasize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // srcBuf spans the entire memory, then we copy out the sub-region we're interested in
    bufInfo.size = memsize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &srcBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), srcBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, datasize,
        GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };

    allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &readbackmem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, datamem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferCopy region = {dataoffs, 0, datasize};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), srcBuf, NULL);
    ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL);

    GetResourceManager()->SetInitialContents(id, VkInitialContents(type, readbackmem, datasize));

    return true;
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }

  return false;
}

uint32_t WrappedVulkan::GetSize_InitialState(ResourceId id, WrappedVkRes *res)
{
  VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
  VkResourceType type = IdentifyTypeByPtr(record->Resource);
  VkInitialContents initContents = GetResourceManager()->GetInitialContents(id);

  if(type == eResDescriptorSet)
  {
    RDCASSERT(record->descInfo && record->descInfo->layout);
    const DescSetLayout &layout = *record->descInfo->layout;

    uint32_t NumBindings = 0;

    for(size_t i = 0; i < layout.bindings.size(); i++)
      NumBindings += layout.bindings[i].descriptorCount;

    return 32 + NumBindings * sizeof(DescriptorSetSlot);
  }
  else if(type == eResBuffer)
  {
    // buffers only have initial states when they're sparse
    return GetSize_SparseInitialState(id, res);
  }
  else if(type == eResImage || type == eResDeviceMemory)
  {
    if(initContents.tag == VkInitialContents::Sparse)
      return GetSize_SparseInitialState(id, res);

    // the size primarily comes from the buffer, the size of which we conveniently have stored.
    return uint32_t(128 + initContents.size + WriteSerialiser::GetChunkAlignment());
  }

  RDCERR("Unhandled resource type %s", ToStr(type).c_str());
  return 128;
}

// second parameter isn't used, as we might be serialising init state for a deleted resource
template <typename SerialiserType>
bool WrappedVulkan::Serialise_InitialState(SerialiserType &ser, ResourceId id, WrappedVkRes *)
{
  VkResourceType type;

  VkResourceRecord *record = NULL;
  if(ser.IsWriting())
  {
    record = GetResourceManager()->GetResourceRecord(id);
    // use the record's resource, not the one passed in, because the passed in one
    // might be null if it was deleted
    type = IdentifyTypeByPtr(record->Resource);
  }

  bool ret = true;

  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(id);

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
      VkInitialContents initContents = GetResourceManager()->GetInitialContents(id);

      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      Bindings = (DescriptorSetSlot *)initContents.descriptorSlots;

      for(size_t i = 0; i < layout.bindings.size(); i++)
        NumBindings += layout.bindings[i].descriptorCount;
    }

    SERIALISE_ELEMENT_ARRAY(Bindings, NumBindings);

    SERIALISE_CHECK_READ_ERRORS();

    // while reading, fetch the binding information and allocate a VkWriteDescriptorSet array
    if(IsReplayingAndReading())
    {
      WrappedVkRes *res = GetResourceManager()->GetLiveResource(id);
      ResourceId liveid = GetResourceManager()->GetLiveID(id);

      const DescSetLayout &layout =
          m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[liveid].layout];

      VkInitialContents initialContents(type, VK_NULL_HANDLE, 0);

      initialContents.numDescriptors = (uint32_t)layout.bindings.size();
      initialContents.descriptorInfo = new VkDescriptorBufferInfo[NumBindings];
      initialContents.descriptorWrites = new VkWriteDescriptorSet[initialContents.numDescriptors];

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Descriptor structs sizes are unexpected, ensure largest size is used");

      VkWriteDescriptorSet *writes = initialContents.descriptorWrites;
      VkDescriptorBufferInfo *dstData = initialContents.descriptorInfo;
      DescriptorSetSlot *srcData = Bindings;

      uint32_t validBinds = initialContents.numDescriptors;

      // i is the writedescriptor that we're updating, could be
      // lower than j if a writedescriptor ended up being no-op and
      // was skipped. j is the actual index.
      for(uint32_t i = 0, j = 0; j < initialContents.numDescriptors; j++)
      {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = NULL;

        // update whole element (array or single)
        writes[i].dstSet = (VkDescriptorSet)(uint64_t)res;
        writes[i].dstBinding = j;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = layout.bindings[j].descriptorCount;
        writes[i].descriptorType = layout.bindings[j].descriptorType;

        DescriptorSetSlot *src = srcData;
        srcData += layout.bindings[j].descriptorCount;

        // will be cast to the appropriate type, we just need to increment
        // the dstData pointer by worst case size
        VkDescriptorBufferInfo *dstBuffer = dstData;
        VkDescriptorImageInfo *dstImage = (VkDescriptorImageInfo *)dstData;
        VkBufferView *dstTexelBuffer = (VkBufferView *)dstData;
        dstData += layout.bindings[j].descriptorCount;

        // the correct one will be set below
        writes[i].pBufferInfo = NULL;
        writes[i].pImageInfo = NULL;
        writes[i].pTexelBufferView = NULL;

        // check that the resources we need for this write are present,
        // as some might have been skipped due to stale descriptor set
        // slots or otherwise unreferenced objects (the descriptor set
        // initial contents do not cause a frame reference for their
        // resources
        //
        // While we go, we copy from the DescriptorSetSlot structures to
        // the appropriate array in the VkWriteDescriptorSet for the
        // descriptor type
        bool valid = true;

        // quick check for slots that were completely uninitialised
        // and so don't have valid data
        if(src->texelBufferView == VK_NULL_HANDLE && src->imageInfo.sampler == VK_NULL_HANDLE &&
           src->imageInfo.imageView == VK_NULL_HANDLE && src->bufferInfo.buffer == VK_NULL_HANDLE)
        {
          valid = false;
        }
        else
        {
          switch(writes[i].descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstImage[d] = src[d].imageInfo;
                valid &= (src[d].imageInfo.sampler != VK_NULL_HANDLE);
              }
              writes[i].pImageInfo = dstImage;
              break;
            }
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstImage[d] = src[d].imageInfo;
                valid &= (src[d].imageInfo.sampler != VK_NULL_HANDLE) ||
                         (layout.bindings[j].immutableSampler &&
                          layout.bindings[j].immutableSampler[d] != ResourceId());
                valid &= (src[d].imageInfo.imageView != VK_NULL_HANDLE);
              }
              writes[i].pImageInfo = dstImage;
              break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstImage[d] = src[d].imageInfo;
                valid &= (src[d].imageInfo.imageView != VK_NULL_HANDLE);
              }
              writes[i].pImageInfo = dstImage;
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstTexelBuffer[d] = src[d].texelBufferView;
                valid &= (src[d].texelBufferView != VK_NULL_HANDLE);
              }
              writes[i].pTexelBufferView = dstTexelBuffer;
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstBuffer[d] = src[d].bufferInfo;
                valid &= (src[d].bufferInfo.buffer != VK_NULL_HANDLE);
              }
              writes[i].pBufferInfo = dstBuffer;
              break;
            }
            default:
            {
              RDCERR("Unexpected descriptor type %d", writes[i].descriptorType);
              ret = false;
            }
          }
        }

        // if this write is not valid, skip it
        // and start writing the next one in here
        if(!valid)
          validBinds--;
        else
          i++;
      }

      initialContents.numDescriptors = validBinds;

      GetResourceManager()->SetInitialContents(id, initialContents);
    }
  }
  else if(type == eResBuffer)
  {
    // buffers only have initial states when they're sparse
    return Serialise_SparseBufferInitialState(ser, id, GetResourceManager()->GetInitialContents(id));
  }
  else if(type == eResDeviceMemory || type == eResImage)
  {
    VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;
    VkInitialContents initContents = GetResourceManager()->GetInitialContents(id);

    // if we have a blob of data, this contains sparse mapping so re-direct to the sparse
    // implementation of this function
    SERIALISE_ELEMENT_LOCAL(IsSparse, initContents.tag == VkInitialContents::Sparse);

    if(IsSparse)
    {
      ret = false;

      if(type == eResImage)
      {
        ret = Serialise_SparseImageInitialState(ser, id, initContents);
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
    uint64_t ContentsSize = initContents.size;
    VkDeviceMemory mappedMem = VK_NULL_HANDLE;

    // Serialise this separately so that it can be used on reading to prepare the upload memory
    SERIALISE_ELEMENT(ContentsSize);

    // the memory/buffer that we allocated on read, to upload the initial contents.
    VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
    VkBuffer uploadBuf = VK_NULL_HANDLE;

    // during writing, we already have the memory copied off - we just need to map it.
    if(ser.IsWriting())
    {
      mappedMem = initContents.mem;
      vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem), 0, VK_WHOLE_SIZE, 0,
                                  (void **)&Contents);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
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

      vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &uploadBuf);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), uploadBuf);

      VkMemoryRequirements mrq = {0};

      ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(uploadBuf), &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          GetUploadMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &uploadMemory);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), uploadMemory);

      vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(uploadBuf), Unwrap(uploadMemory), 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      mappedMem = uploadMemory;

      ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(uploadMemory), 0, VK_WHOLE_SIZE, 0, (void **)&Contents);
    }

    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("Contents", Contents, ContentsSize, SerialiserFlags::NoFlags);

    // unmap the resource we mapped before - we need to do this on read and on write.
    if(!IsStructuredExporting(m_State) && mappedMem != VK_NULL_HANDLE)
      ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem));

    SERIALISE_CHECK_READ_ERRORS();

    // if we're handling a device memory object, we're done - we note the memory object to delete at
    // the end of the program, and store the buffer to copy off in Apply
    if(IsReplayingAndReading())
    {
      ResourceId liveid = GetResourceManager()->GetLiveID(id);

      if(type == eResDeviceMemory)
      {
        VkInitialContents initialContents(type, uploadMemory, ContentsSize);
        initialContents.buf = uploadBuf;

        GetResourceManager()->SetInitialContents(id, initialContents);
      }
      else
      {
        VkInitialContents initial(type, uploadMemory, 0);

        VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[liveid];

        // for non-MSAA images, we're done - we'll do buffer-to-image copies with appropriate
        // offsets to copy out the subresources into the image itself.
        if(c.samples == VK_SAMPLE_COUNT_1_BIT)
        {
          initial.buf = uploadBuf;
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

          vkr = ObjDisp(d)->CreateImage(Unwrap(d), &arrayInfo, NULL, &arrayIm);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          GetResourceManager()->WrapResource(Unwrap(d), arrayIm);

          VkMemoryRequirements mrq = {0};
          ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), Unwrap(arrayIm), &mrq);

          VkMemoryAllocateInfo allocInfo = {
              VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
              GetGPULocalMemoryIndex(mrq.memoryTypeBits),
          };

          VkDeviceMemory arrayMem;

          vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &arrayMem);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          GetResourceManager()->WrapResource(Unwrap(d), arrayMem);

          vkr = ObjDisp(d)->BindImageMemory(Unwrap(d), Unwrap(arrayIm), Unwrap(arrayMem), 0);
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

          // destroy the buffer and de-allocate as it's no longer needed
          vkDestroyBuffer(d, uploadBuf, NULL);
          vkFreeMemory(d, uploadMemory, NULL);

          initial.buf = VK_NULL_HANDLE;
          initial.img = arrayIm;
          initial.mem = arrayMem;
        }

        GetResourceManager()->SetInitialContents(id, initial);
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

template bool WrappedVulkan::Serialise_InitialState(ReadSerialiser &ser, ResourceId resid,
                                                    WrappedVkRes *);
template bool WrappedVulkan::Serialise_InitialState(WriteSerialiser &ser, ResourceId resid,
                                                    WrappedVkRes *);

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

void WrappedVulkan::Apply_InitialState(WrappedVkRes *live, VkInitialContents initial)
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
    vector<DescriptorSetSlot *> &bindings = m_DescriptorSetState[id].currentBindings;

    for(uint32_t i = 0; i < initial.numDescriptors; i++)
    {
      RDCASSERT(writes[i].dstBinding < bindings.size());
      RDCASSERT(writes[i].dstArrayElement == 0);

      DescriptorSetSlot *bind = bindings[writes[i].dstBinding];

      for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
      {
        if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind[d].texelBufferView = writes[i].pTexelBufferView[d];
        }
        else if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
          bind[d].bufferInfo = writes[i].pBufferInfo[d];
        }
        else
        {
          bind[d].imageInfo = writes[i].pImageInfo[d];
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
        if(IsBlockFormat(m_ImageLayouts[id].format))
        {
          RDCWARN(
              "Trying to clear a compressed image %llu - should have initial states or be "
              "stripped.",
              id);
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
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ToHandle<VkImage>(live),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        // finish any pending work before clear
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        // clear completes before subsequent operations
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          DoPipelineBarrier(cmd, 1, &barrier);
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

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          barrier.dstAccessMask |= MakeAccessMask(barrier.newLayout);
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ToHandle<VkImage>(live),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        // finish any pending work before clear
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        // clear completes before subsequent operations
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          DoPipelineBarrier(cmd, 1, &barrier);
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

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          ToHandle<VkImage>(live),
          {aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
      };

      barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
      barrier.dstAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
      {
        barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
        barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
        DoPipelineBarrier(cmd, 1, &barrier);
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

      for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
      {
        barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
        barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
        barrier.dstAccessMask |= MakeAccessMask(barrier.newLayout);
        DoPipelineBarrier(cmd, 1, &barrier);
      }

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
        ToHandle<VkImage>(live),
        {aspectFlags, 0, 1, 0, (uint32_t)m_CreationInfo.m_Image[id].arrayLayers}};

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

    for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
    {
      dstimBarrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
      dstimBarrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
      dstimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS | MakeAccessMask(dstimBarrier.oldLayout);
      DoPipelineBarrier(cmd, 1, &dstimBarrier);
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

    for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
    {
      dstimBarrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
      dstimBarrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
      dstimBarrier.dstAccessMask |= MakeAccessMask(dstimBarrier.newLayout);
      DoPipelineBarrier(cmd, 1, &dstimBarrier);
    }

    vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitCmds();
#endif
  }
  else if(type == eResDeviceMemory)
  {
    VkResult vkr = VK_SUCCESS;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    VkBuffer srcBuf = initial.buf;
    VkDeviceSize datasize = initial.size;
    VkDeviceSize dstMemOffs = 0;

    VkCommandBuffer cmd = GetNextCmd();

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;

    VkBufferCopy region = {0, dstMemOffs, datasize};

    ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);

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
