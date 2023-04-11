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

#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_debug.h"

#define VULKAN 1
#include "data/glsl/glsl_globals.h"
#include "data/glsl/glsl_ubos_cpp.h"

void VulkanDebugManager::CopyTex2DMSToBuffer(VkCommandBuffer cmd, VkBuffer destBuffer,
                                             VkImage srcMS, VkExtent3D extent, uint32_t baseSlice,
                                             uint32_t numSlices, uint32_t baseSample,
                                             uint32_t numSamples, VkFormat fmt)
{
  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthTex2DMSToBuffer(cmd, destBuffer, srcMS, extent, baseSlice, numSlices, baseSample,
                             numSamples, fmt);
    return;
  }

  if(m_MS2BufferPipe == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcView;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcMS,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_FORMAT_UNDEFINED,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  uint32_t bs = GetByteSize(1, 1, 1, fmt, 0);

  if(bs == 1)
    viewInfo.format = VK_FORMAT_R8_UINT;
  else if(bs == 2)
    viewInfo.format = VK_FORMAT_R16_UINT;
  else if(bs == 4)
    viewInfo.format = VK_FORMAT_R32_UINT;
  else if(bs == 8)
    viewInfo.format = VK_FORMAT_R32G32_UINT;
  else if(bs == 16)
    viewInfo.format = VK_FORMAT_R32G32B32A32_UINT;

  if(viewInfo.format == VK_FORMAT_UNDEFINED)
  {
    RDCERR("Can't copy 2D to Buffer with format %s", ToStr(fmt).c_str());
    return;
  }

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcView);
  CheckVkResult(vkr);
  NameUnwrappedVulkanObject(srcView, "MS -> Buffer srcView");

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  bool endCommand = false;

  if(cmd == VK_NULL_HANDLE)
  {
    cmd = m_pDriver->GetNextCmd();
    if(cmd != VK_NULL_HANDLE)
      ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    endCommand = true;
  }

  if(cmd == VK_NULL_HANDLE)
    return;

  {
    VkMarkerRegion region(cmd, "CopyTex2DMSToBuffer");

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                  Unwrap(m_MS2BufferPipe));

    const uint32_t dispatchBufferSize =
        GetByteSize(extent.width, extent.height, extent.depth, fmt, 0);
    uint32_t dispatchOffset = 0;

    VkDescriptorImageInfo srcdesc = {0};
    srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    srcdesc.imageView = srcView;
    srcdesc.sampler = VK_NULL_HANDLE;    // not used - we use texelFetch

    VkDescriptorBufferInfo destdesc = {0};
    destdesc.buffer = destBuffer;
    destdesc.offset = 0;
    destdesc.range = VK_WHOLE_SIZE;

    VkDescriptorSet descSet = GetBufferMSDescSet();

    VkWriteDescriptorSet writeSet[] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 0, 0, 1,
         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &srcdesc, NULL, NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 2, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &destdesc, NULL},
    };

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                        Unwrap(m_BufferMSPipeLayout), 0, 1, UnwrapPtr(descSet), 0,
                                        NULL);

    for(uint32_t currentSlice = baseSlice; currentSlice < numSlices + baseSlice; currentSlice++)
    {
      for(uint32_t currentSample = baseSample; currentSample < numSamples + baseSample;
          currentSample++)
      {
        // if the byte size is less than 4, we need to multisample.
        const uint32_t msDivider = bs < 4 ? (4 / bs) : 1;
        const uint32_t workGroupDivider = MS_DISPATCH_LOCAL_SIZE * msDivider;
        const uint32_t numWorkGroup =
            AlignUp(extent.width * extent.height, workGroupDivider) / workGroupDivider;
        const uint32_t maxInvoc = AlignUp(extent.width * extent.height, msDivider) / msDivider;

        Vec4u params[2] = {
            {extent.width, currentSlice, currentSample, bs}, {maxInvoc, dispatchOffset, 0, 0},
        };

        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_BufferMSPipeLayout),
                                       VK_SHADER_STAGE_ALL, 0, sizeof(Vec4u) * 2, &params);

        // Use a 1D workgroup size so that we don't have to worry about width or height
        // being a multiple of our multisample size
        ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), numWorkGroup, 1, 1);

        dispatchOffset += dispatchBufferSize / 4;
      }
    }
  }

  if(endCommand)
    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  m_pDriver->AddPendingObjectCleanup([this, dev, srcView]() {
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcView, NULL);
    ResetBufferMSDescriptorPools();
  });
}

void VulkanDebugManager::CopyDepthTex2DMSToBuffer(VkCommandBuffer cmd, VkBuffer destBuffer,
                                                  VkImage srcMS, VkExtent3D extent,
                                                  uint32_t baseSlice, uint32_t numSlices,
                                                  uint32_t baseSample, uint32_t numSamples,
                                                  VkFormat fmt)
{
  if(m_DepthMS2BufferPipe == VK_NULL_HANDLE)
    return;

  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  uint32_t fmtIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM: fmtIndex = SHADER_D16_UNORM; break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      fmtIndex = SHADER_D16_UNORM_S8_UINT;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: fmtIndex = SHADER_X8_D24_UNORM_PACK32; break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      fmtIndex = SHADER_D24_UNORM_S8_UINT;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D32_SFLOAT: fmtIndex = SHADER_D32_SFLOAT; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      fmtIndex = SHADER_D32_SFLOAT_S8_UINT;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_S8_UINT:
      fmtIndex = SHADER_S8_UINT;
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default: RDCERR("Unexpected depth format: %d", fmt); return;
  }

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcDepthView = VK_NULL_HANDLE, srcStencilView = VK_NULL_HANDLE;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcMS,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      fmt,
      {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
       VK_COMPONENT_SWIZZLE_ZERO},
      {
          VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  if(aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT)
  {
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcDepthView);
    CheckVkResult(vkr);
    NameUnwrappedVulkanObject(srcDepthView, "Depth MS -> Array srcDepthView");
  }

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcStencilView);
    CheckVkResult(vkr);
    NameUnwrappedVulkanObject(srcStencilView, "Depth MS -> Array srcStencilView");
  }

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  bool endCommand = false;

  if(cmd == VK_NULL_HANDLE)
  {
    cmd = m_pDriver->GetNextCmd();
    if(cmd != VK_NULL_HANDLE)
      ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    endCommand = true;
  }

  if(cmd == VK_NULL_HANDLE)
    return;

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                Unwrap(m_DepthMS2BufferPipe));

  VkDescriptorImageInfo srcdesc[2];
  srcdesc[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[0].imageView = srcDepthView;
  srcdesc[0].sampler = VK_NULL_HANDLE;    // not used - we use texelFetch
  srcdesc[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[1].imageView = srcStencilView;
  srcdesc[1].sampler = VK_NULL_HANDLE;    // not used - we use texelFetch

  if((aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT) == 0)
  {
    if(m_DummyDepthView != VK_NULL_HANDLE)
    {
      srcdesc[0].imageView = Unwrap(m_DummyDepthView);
    }
    else
    {
      // as a last fallback, hope that setting an incompatible view (float not int) will not break
      // too badly. This only gets hit when the implementation has such poor format support that
      // there are no float formats that can be sampled as MSAA.
      srcdesc[0].imageView = srcStencilView;
    }
  }

  if((aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT) == 0)
  {
    if(m_DummyStencilView != VK_NULL_HANDLE)
    {
      srcdesc[1].imageView = Unwrap(m_DummyStencilView);
    }
    else
    {
      // as a last fallback, hope that setting an incompatible view (float not int) will not break
      // too badly. This only gets hit when the implementation has such poor format support that
      // there are no uint formats that can be sampled as MSAA.
      srcdesc[1].imageView = srcDepthView;
    }
  }

  const uint32_t dispatchBufferSize = GetByteSize(extent.width, extent.height, extent.depth, fmt, 0);
  uint32_t dispatchOffset = 0;

  VkDescriptorBufferInfo destdesc = {0};
  destdesc.buffer = destBuffer;
  destdesc.offset = 0;
  destdesc.range = VK_WHOLE_SIZE;

  VkDescriptorSet descSet = GetBufferMSDescSet();

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &srcdesc[0], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &srcdesc[1], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &destdesc, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_BufferMSPipeLayout), 0, 1, UnwrapPtr(descSet), 0,
                                      NULL);

  for(uint32_t currentSlice = baseSlice; currentSlice < numSlices + baseSlice; currentSlice++)
  {
    for(uint32_t currentSample = baseSample; currentSample < numSamples + baseSample; currentSample++)
    {
      // for D16 and S8 textures, we need to multisample.
      const uint32_t msDivider =
          (fmt == VK_FORMAT_D16_UNORM) ? 2 : (fmt == VK_FORMAT_S8_UINT) ? 4 : 1;
      const uint32_t workGroupDivider = MS_DISPATCH_LOCAL_SIZE * msDivider;
      const uint32_t numWorkGroup =
          AlignUp(extent.width * extent.height, workGroupDivider) / workGroupDivider;
      const uint32_t maxInvoc = AlignUp(extent.width * extent.height, msDivider) / msDivider;

      Vec4u params[2] = {
          {extent.width, currentSlice, currentSample, fmtIndex}, {maxInvoc, dispatchOffset, 0, 0},
      };

      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_BufferMSPipeLayout), VK_SHADER_STAGE_ALL,
                                     0, sizeof(Vec4u) * 2, &params);

      ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), numWorkGroup, 1, 1);

      dispatchOffset += dispatchBufferSize / 4;
    }
  }

  if(endCommand)
    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  m_pDriver->AddPendingObjectCleanup([this, dev, srcDepthView, srcStencilView]() {
    if(srcDepthView != VK_NULL_HANDLE)
      ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcDepthView, NULL);
    if(srcStencilView != VK_NULL_HANDLE)
      ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcStencilView, NULL);
    ResetBufferMSDescriptorPools();
  });
}

void VulkanDebugManager::CopyBufferToTex2DMS(VkCommandBuffer cmd, VkImage destMS,
                                             VkBuffer srcBuffer, VkExtent3D extent,
                                             uint32_t numSlices, uint32_t numSamples, VkFormat fmt)
{
  if(!m_pDriver->GetDeviceEnabledFeatures().shaderStorageImageMultisample ||
     !m_pDriver->GetDeviceEnabledFeatures().shaderStorageImageWriteWithoutFormat)
    return;

  if(m_Buffer2MSPipe == VK_NULL_HANDLE)
    return;

  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthBufferToTex2DMS(cmd, destMS, srcBuffer, extent, numSlices, numSamples, fmt);
    return;
  }

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView destView;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      destMS,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_FORMAT_UNDEFINED,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  uint32_t bs = GetByteSize(1, 1, 1, fmt, 0);

  if(bs == 1)
    viewInfo.format = VK_FORMAT_R8_UINT;
  else if(bs == 2)
    viewInfo.format = VK_FORMAT_R16_UINT;
  else if(bs == 4)
    viewInfo.format = VK_FORMAT_R32_UINT;
  else if(bs == 8)
    viewInfo.format = VK_FORMAT_R32G32_UINT;
  else if(bs == 16)
    viewInfo.format = VK_FORMAT_R32G32B32A32_UINT;

  if(viewInfo.format == VK_FORMAT_UNDEFINED)
  {
    RDCERR("Can't copy Array to MS with format %s", ToStr(fmt).c_str());
    return;
  }

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView);
  CheckVkResult(vkr);
  NameUnwrappedVulkanObject(destView, "Array -> MS destView");

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  bool endCommand = false;

  if(cmd == VK_NULL_HANDLE)
  {
    cmd = m_pDriver->GetNextCmd();
    if(cmd != VK_NULL_HANDLE)
      ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    endCommand = true;
  }

  if(cmd == VK_NULL_HANDLE)
    return;

  VkDescriptorSet descSet = GetBufferMSDescSet();

  {
    VkMarkerRegion region(cmd, "CopyBufferToTex2DMS");

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                  Unwrap(m_Buffer2MSPipe));

    const uint32_t dispatchBufferSize =
        GetByteSize(extent.width, extent.height, extent.depth, fmt, 0);
    uint32_t dispatchOffset = 0;

    VkDescriptorBufferInfo srcdesc = {0};
    srcdesc.buffer = srcBuffer;
    srcdesc.offset = 0;
    srcdesc.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo destdesc = {0};
    destdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    destdesc.imageView = destView;
    destdesc.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet writeSet[] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 2, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &srcdesc, NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 3, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &destdesc, NULL, NULL},
    };

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                        Unwrap(m_BufferMSPipeLayout), 0, 1, UnwrapPtr(descSet), 0,
                                        NULL);

    for(uint32_t currentSlice = 0; currentSlice < numSlices; currentSlice++)
    {
      for(uint32_t currentSample = 0; currentSample < numSamples; currentSample++)
      {
        // if the byte size is less than 4, we need to multisample.
        const uint32_t msDivider = bs < 4 ? (4 / bs) : 1;
        const uint32_t workGroupDivider = MS_DISPATCH_LOCAL_SIZE * msDivider;
        const uint32_t numWorkGroup =
            AlignUp(extent.width * extent.height, workGroupDivider) / workGroupDivider;
        const uint32_t maxInvoc = AlignUp(extent.width * extent.height, msDivider) / msDivider;

        Vec4u params[2] = {
            {extent.width, currentSlice, currentSample, bs}, {maxInvoc, dispatchOffset, 0, 0},
        };

        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_BufferMSPipeLayout),
                                       VK_SHADER_STAGE_ALL, 0, sizeof(Vec4u) * 2, &params);

        ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), numWorkGroup, 1, 1);

        dispatchOffset += dispatchBufferSize / 4;
      }
    }
  }

  if(endCommand)
    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  m_pDriver->AddPendingObjectCleanup([this, dev, destView]() {
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView, NULL);
    ResetBufferMSDescriptorPools();
  });
}

void VulkanDebugManager::CopyDepthBufferToTex2DMS(VkCommandBuffer cmd, VkImage destMS,
                                                  VkBuffer srcBuffer, VkExtent3D extent,
                                                  uint32_t numSlices, uint32_t numSamples,
                                                  VkFormat fmt)
{
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

  int pipeIndex = 0;
  int formatIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM:
      pipeIndex = 0;
      formatIndex = SHADER_D16_UNORM;
      break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      pipeIndex = 1;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      formatIndex = SHADER_D16_UNORM_S8_UINT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
      pipeIndex = 2;
      formatIndex = SHADER_X8_D24_UNORM_PACK32;
      break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      pipeIndex = 3;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      formatIndex = SHADER_D24_UNORM_S8_UINT;
      break;
    case VK_FORMAT_D32_SFLOAT:
      pipeIndex = 4;
      formatIndex = SHADER_D32_SFLOAT;
      break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      pipeIndex = 5;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      formatIndex = SHADER_D32_SFLOAT_S8_UINT;
      break;
    case VK_FORMAT_S8_UINT:
      pipeIndex = 6;
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
      formatIndex = SHADER_S8_UINT;
      break;
    default: RDCERR("Unexpected depth format: %d", fmt); return;
  }

  // 0-based from 2x MSAA
  uint32_t sampleIndex = SampleIndex((VkSampleCountFlagBits)numSamples) - 1;

  if(sampleIndex >= ARRAY_COUNT(m_DepthArray2MSPipe[0]))
  {
    RDCERR("Unsupported sample count %u", numSamples);
    return;
  }

  VkPipeline pipe = m_DepthArray2MSPipe[pipeIndex][sampleIndex];

  if(pipe == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  rdcarray<VkImageView> destView;
  destView.resize(numSlices);

  VkDescriptorBufferInfo srcdesc = {0};
  srcdesc.buffer = srcBuffer;
  srcdesc.offset = 0;
  srcdesc.range = VK_WHOLE_SIZE;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      destMS,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      fmt,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  for(uint32_t i = 0; i < numSlices; i++)
  {
    viewInfo.subresourceRange.baseArrayLayer = i;
    viewInfo.subresourceRange.layerCount = 1;

    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView[i]);
    CheckVkResult(vkr);
    NameUnwrappedVulkanObject(destView[i], "Depth Array -> MS destView[i]");
  }

  VkDescriptorSet descSet = GetBufferMSDescSet();

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &srcdesc, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  // create a bespoke framebuffer and renderpass for rendering
  VkAttachmentDescription attDesc = {0,
                                     fmt,
                                     (VkSampleCountFlagBits)numSamples,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     VK_IMAGE_LAYOUT_GENERAL};

  VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_GENERAL};

  VkSubpassDescription sub = {};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.pDepthStencilAttachment = &attRef;

  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      NULL,
      0,
      1,
      &attDesc,
      1,
      &sub,
      0,
      NULL,    // dependencies
  };

  VkRenderPass rp = VK_NULL_HANDLE;

  ObjDisp(dev)->CreateRenderPass(Unwrap(dev), &rpinfo, NULL, &rp);

  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      NULL,
      0,
      rp,
      1,
      NULL,
      extent.width,
      extent.height,
      1,
  };

  rdcarray<VkFramebuffer> fb;
  fb.resize(numSlices);

  for(uint32_t i = 0; i < numSlices; i++)
  {
    fbinfo.pAttachments = destView.data() + i;

    vkr = ObjDisp(dev)->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &fb[i]);
    CheckVkResult(vkr);
  }

  bool endCommand = false;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  if(cmd == VK_NULL_HANDLE)
  {
    cmd = m_pDriver->GetNextCmd();
    if(cmd != VK_NULL_HANDLE)
      ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    endCommand = true;
  }

  if(cmd == VK_NULL_HANDLE)
    return;

  VkClearValue clearval = {};

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp,        VK_NULL_HANDLE,
      {{0, 0}, {extent.width, extent.height}},  1,    &clearval,
  };

  uint32_t numStencil = 1;

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    numStencil = 256;

  Vec4u params[2];
  params[0].x = numSamples;
  params[0].y = formatIndex;
  params[1].x = extent.width;
  params[1].y = extent.height;

  {
    VkMarkerRegion region(cmd, "CopyDepthArrayToTex2DMS");

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

    for(uint32_t i = 0; i < numSlices; i++)
    {
      rpbegin.framebuffer = fb[i];

      ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          Unwrap(m_BufferMSPipeLayout), 0, 1, UnwrapPtr(descSet), 0,
                                          NULL);

      VkViewport viewport = {0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
      ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

      params[0].z = i;    // currentSlice;

      for(uint32_t s = 0; s < numStencil; s++)
      {
        params[0].w = numStencil == 1 ? 1000 : s;    // currentStencil;

        ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, s);
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_BufferMSPipeLayout), VK_SHADER_STAGE_ALL,
                                       0, sizeof(Vec4u) * ARRAY_COUNT(params), &params);
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }

      ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
    }
  }

  if(endCommand)
    ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  m_pDriver->AddPendingObjectCleanup([this, dev, fb, rp, destView]() {
    for(uint32_t i = 0; i < fb.size(); i++)
      ObjDisp(dev)->DestroyFramebuffer(Unwrap(dev), fb[i], NULL);
    ObjDisp(dev)->DestroyRenderPass(Unwrap(dev), rp, NULL);

    for(uint32_t i = 0; i < destView.size(); i++)
      ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView[i], NULL);

    ResetBufferMSDescriptorPools();
  });
}
