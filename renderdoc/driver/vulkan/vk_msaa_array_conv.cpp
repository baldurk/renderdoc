/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
#include "data/glsl/glsl_ubos_cpp.h"

void VulkanDebugManager::CopyTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent,
                                            uint32_t layers, uint32_t samples, VkFormat fmt)
{
  if(!m_pDriver->GetDeviceFeatures().shaderStorageImageMultisample ||
     !m_pDriver->GetDeviceFeatures().shaderStorageImageWriteWithoutFormat)
    return;

  if(m_MS2ArrayPipe == VK_NULL_HANDLE)
    return;

  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthTex2DMSToArray(destArray, srcMS, extent, layers, samples, fmt);
    return;
  }

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcView, destView;

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
    RDCERR("Can't copy 2D to Array with format %s", ToStr(fmt).c_str());
    return;
  }

  if(IsStencilOnlyFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
  else if(IsDepthOrStencilFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = destArray;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkDescriptorImageInfo srcdesc = {0};
  srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc.imageView = srcView;
  srcdesc.sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch

  VkDescriptorImageInfo destdesc = {0};
  destdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  destdesc.imageView = destView;

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc, NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &destdesc, NULL, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_MS2ArrayPipe));
  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                      UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

  Vec4u params = {samples, 0, 0, 0};

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL, 0,
                                 sizeof(Vec4u), &params);

  ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), extent.width, extent.height, layers * samples);

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcView, NULL);
  ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView, NULL);
}

void VulkanDebugManager::CopyDepthTex2DMSToArray(VkImage destArray, VkImage srcMS, VkExtent3D extent,
                                                 uint32_t layers, uint32_t samples, VkFormat fmt)
{
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

  int pipeIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM: pipeIndex = 0; break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      pipeIndex = 1;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: pipeIndex = 2; break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      pipeIndex = 3;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D32_SFLOAT: pipeIndex = 4; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      pipeIndex = 5;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default: RDCERR("Unexpected depth format: %d", fmt); return;
  }

  VkPipeline pipe = m_DepthMS2ArrayPipe[pipeIndex];

  if(pipe == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcDepthView = VK_NULL_HANDLE, srcStencilView = VK_NULL_HANDLE;
  VkImageView *destView = new VkImageView[layers * samples];

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

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcDepthView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcStencilView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.image = destArray;

  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  for(uint32_t i = 0; i < layers * samples; i++)
  {
    viewInfo.subresourceRange.baseArrayLayer = i;
    viewInfo.subresourceRange.layerCount = 1;

    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkDescriptorImageInfo srcdesc[2];
  srcdesc[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[0].imageView = srcDepthView;
  srcdesc[0].sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch
  srcdesc[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[1].imageView = srcStencilView;
  srcdesc[1].sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch

  if((aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT) == 0)
  {
    if(m_DummyStencilView[1] != VK_NULL_HANDLE)
    {
      srcdesc[1].imageView = Unwrap(m_DummyStencilView[1]);
    }
    else
    {
      // as a last fallback, hope that setting an incompatible view (float not int) will not break
      // too badly. This only gets hit when the implementation has such poor format support that
      // there are no uint formats that can be sampled as MSAA.
      srcdesc[1].imageView = srcDepthView;
    }
  }

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[0], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[1], NULL, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 2, writeSet, 0, NULL);

  // create a bespoke framebuffer and renderpass for rendering
  VkAttachmentDescription attDesc = {0,
                                     fmt,
                                     VK_SAMPLE_COUNT_1_BIT,
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

  VkFramebuffer *fb = new VkFramebuffer[layers * samples];

  for(uint32_t i = 0; i < layers * samples; i++)
  {
    fbinfo.pAttachments = destView + i;

    vkr = ObjDisp(dev)->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &fb[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  VkClearValue clearval = {};

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp,        VK_NULL_HANDLE,
      {{0, 0}, {extent.width, extent.height}},  1,    &clearval,
  };

  uint32_t numStencil = 1;

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    numStencil = 256;

  Vec4u params;
  params.x = samples;

  for(uint32_t i = 0; i < layers * samples; i++)
  {
    rpbegin.framebuffer = fb[i];

    ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                        UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

    VkViewport viewport = {0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    params.y = i % samples;    // currentSample;
    params.z = i / samples;    // currentSlice;

    for(uint32_t s = 0; s < numStencil; s++)
    {
      params.w = numStencil == 1 ? 1000 : s;    // currentStencil;

      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FRONT_AND_BACK, s);
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL,
                                     0, sizeof(Vec4u), &params);
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
    }

    ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
  }

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  for(uint32_t i = 0; i < layers * samples; i++)
    ObjDisp(dev)->DestroyFramebuffer(Unwrap(dev), fb[i], NULL);
  ObjDisp(dev)->DestroyRenderPass(Unwrap(dev), rp, NULL);

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcDepthView, NULL);
  if(srcStencilView != VK_NULL_HANDLE)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcStencilView, NULL);
  for(uint32_t i = 0; i < layers * samples; i++)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView[i], NULL);

  SAFE_DELETE_ARRAY(destView);
  SAFE_DELETE_ARRAY(fb);
}

void VulkanDebugManager::CopyArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent,
                                            uint32_t layers, uint32_t samples, VkFormat fmt)
{
  if(!m_pDriver->GetDeviceFeatures().shaderStorageImageMultisample ||
     !m_pDriver->GetDeviceFeatures().shaderStorageImageWriteWithoutFormat)
    return;

  if(m_Array2MSPipe == VK_NULL_HANDLE)
    return;

  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthArrayToTex2DMS(destMS, srcArray, extent, layers, samples, fmt);
    return;
  }

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcView, destView;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcArray,
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

  if(IsStencilOnlyFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
  else if(IsDepthOrStencilFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = destMS;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkDescriptorImageInfo srcdesc = {0};
  srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc.imageView = srcView;
  srcdesc.sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch

  VkDescriptorImageInfo destdesc = {0};
  destdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  destdesc.imageView = destView;

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc, NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &destdesc, NULL, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_Array2MSPipe));
  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                      UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

  Vec4u params = {samples, 0, 0, 0};

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL, 0,
                                 sizeof(Vec4u), &params);

  ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), extent.width, extent.height, layers * samples);

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcView, NULL);
  ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView, NULL);
}

void VulkanDebugManager::CopyDepthArrayToTex2DMS(VkImage destMS, VkImage srcArray, VkExtent3D extent,
                                                 uint32_t layers, uint32_t samples, VkFormat fmt)
{
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

  int pipeIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM: pipeIndex = 0; break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      pipeIndex = 1;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: pipeIndex = 2; break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      pipeIndex = 3;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D32_SFLOAT: pipeIndex = 4; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      pipeIndex = 5;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default: RDCERR("Unexpected depth format: %d", fmt); return;
  }

  // 0-based from 2x MSAA
  uint32_t sampleIndex = SampleIndex((VkSampleCountFlagBits)samples) - 1;

  if(sampleIndex >= ARRAY_COUNT(m_DepthArray2MSPipe[0]))
  {
    RDCERR("Unsupported sample count %u", samples);
    return;
  }

  VkPipeline pipe = m_DepthArray2MSPipe[pipeIndex][sampleIndex];

  if(pipe == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_Device;

  VkResult vkr = VK_SUCCESS;

  VkImageView srcDepthView = VK_NULL_HANDLE, srcStencilView = VK_NULL_HANDLE;
  VkImageView *destView = new VkImageView[layers];

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      srcArray,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      fmt,
      {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
       VK_COMPONENT_SWIZZLE_ZERO},
      {
          VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS,
      },
  };

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcDepthView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcStencilView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.image = destMS;

  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

  for(uint32_t i = 0; i < layers; i++)
  {
    viewInfo.subresourceRange.baseArrayLayer = i;
    viewInfo.subresourceRange.layerCount = 1;

    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &destView[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkDescriptorImageInfo srcdesc[2];
  srcdesc[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[0].imageView = srcDepthView;
  srcdesc[0].sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch
  srcdesc[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc[1].imageView = srcStencilView;
  srcdesc[1].sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch

  if((aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT) == 0)
  {
    if(m_DummyStencilView[0] != VK_NULL_HANDLE)
    {
      srcdesc[1].imageView = Unwrap(m_DummyStencilView[0]);
    }
    else
    {
      // as a last fallback, hope that setting an incompatible view (float not int) will not break
      // too badly. This only gets hit when the implementation has such poor format support that
      // there are no uint formats that can be sampled as MSAA.
      srcdesc[1].imageView = srcDepthView;
    }
  }

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[0], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_ArrayMSDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[1], NULL, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 2, writeSet, 0, NULL);

  // create a bespoke framebuffer and renderpass for rendering
  VkAttachmentDescription attDesc = {0,
                                     fmt,
                                     (VkSampleCountFlagBits)samples,
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

  VkFramebuffer *fb = new VkFramebuffer[layers];

  for(uint32_t i = 0; i < layers; i++)
  {
    fbinfo.pAttachments = destView + i;

    vkr = ObjDisp(dev)->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &fb[i]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  VkClearValue clearval = {};

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp,        VK_NULL_HANDLE,
      {{0, 0}, {extent.width, extent.height}},  1,    &clearval,
  };

  uint32_t numStencil = 1;

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
    numStencil = 256;

  Vec4u params;
  params.x = samples;
  params.y = 0;    // currentSample;

  for(uint32_t i = 0; i < layers; i++)
  {
    rpbegin.framebuffer = fb[i];

    ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Unwrap(m_ArrayMSPipeLayout), 0, 1,
                                        UnwrapPtr(m_ArrayMSDescSet), 0, NULL);

    VkViewport viewport = {0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
    ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    params.z = i;    // currentSlice;

    for(uint32_t s = 0; s < numStencil; s++)
    {
      params.w = numStencil == 1 ? 1000 : s;    // currentStencil;

      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FRONT_AND_BACK, s);
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_ArrayMSPipeLayout), VK_SHADER_STAGE_ALL,
                                     0, sizeof(Vec4u), &params);
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
    }

    ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
  }

  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  for(uint32_t i = 0; i < layers; i++)
    ObjDisp(dev)->DestroyFramebuffer(Unwrap(dev), fb[i], NULL);
  ObjDisp(dev)->DestroyRenderPass(Unwrap(dev), rp, NULL);

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcDepthView, NULL);
  if(srcStencilView != VK_NULL_HANDLE)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcStencilView, NULL);
  for(uint32_t i = 0; i < layers; i++)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), destView[i], NULL);

  SAFE_DELETE_ARRAY(destView);
  SAFE_DELETE_ARRAY(fb);
}
