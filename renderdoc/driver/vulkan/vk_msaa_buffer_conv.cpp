/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

void VulkanDebugManager::CopyTex2DMSToBuffer(VkBuffer destBuffer, VkImage srcMS, VkExtent3D extent,
                                             uint32_t slice, uint32_t sample, VkFormat fmt)
{
  if(IsDepthOrStencilFormat(fmt))
  {
    CopyDepthTex2DMSToBuffer(destBuffer, srcMS, extent, slice, sample, fmt);
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

  VkMarkerRegion region("CopyTex2DMSToBuffer");

  if(IsStencilOnlyFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcView);
  CheckVkResult(vkr);
  NameUnwrappedVulkanObject(srcView, "MS -> Buffer srcView");

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return;

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(m_MS2BufferPipe));

  VkDescriptorImageInfo srcdesc = {0};
  srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  srcdesc.imageView = srcView;
  srcdesc.sampler = Unwrap(m_ArrayMSSampler);    // not used - we use texelFetch

  VkDescriptorBufferInfo destdesc = {0};
  destdesc.buffer = destBuffer;
  destdesc.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MS2BufferDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc, NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MS2BufferDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &destdesc, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_MS2BufferPipeLayout), 0, 1,
                                      UnwrapPtr(m_MS2BufferDescSet), 0, NULL);

  Vec4u params = {1, slice, sample, bs};

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_MS2BufferPipeLayout), VK_SHADER_STAGE_ALL, 0,
                                 sizeof(Vec4u), &params);

  // if the byte size is less than 4, we need to multisample.
  if(bs < 4)
  {
    uint32_t ms = 4 / bs;
    // Use a 1D workgroup size so that we don't have to worry about width or height
    // being a multiple of our multisample size
    ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), AlignUp(extent.width * extent.height, ms) / ms, 1, 1);
  }
  else
  {
    ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), extent.width, extent.height, 1);
  }

  // finished, submit and flush
  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  cmd = VK_NULL_HANDLE;

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcView, NULL);
}

void VulkanDebugManager::CopyDepthTex2DMSToBuffer(VkBuffer destBuffer, VkImage srcMS,
                                                  VkExtent3D extent, uint32_t slice,
                                                  uint32_t sample, VkFormat fmt)
{
  if(m_DepthMS2BufferPipe == VK_NULL_HANDLE)
    return;

  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  uint32_t fmtIndex = 0;
  switch(fmt)
  {
    case VK_FORMAT_D16_UNORM: fmtIndex = 0; break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      fmtIndex = 1;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: fmtIndex = 2; break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      fmtIndex = 3;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D32_SFLOAT: fmtIndex = 4; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      fmtIndex = 5;
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
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

  vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcDepthView);
  CheckVkResult(vkr);
  NameUnwrappedVulkanObject(srcDepthView, "Depth MS -> Array srcDepthView");

  if(aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &srcStencilView);
    CheckVkResult(vkr);
    NameUnwrappedVulkanObject(srcStencilView, "Depth MS -> Array srcStencilView");
  }

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return;

  ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                Unwrap(m_DepthMS2BufferPipe));

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

  VkDescriptorBufferInfo destdesc = {0};
  destdesc.buffer = destBuffer;
  destdesc.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MS2BufferDescSet), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[0], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MS2BufferDescSet), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc[1], NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_MS2BufferDescSet), 2, 0, 1,
       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &destdesc, NULL},
  };

  ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_MS2BufferPipeLayout), 0, 1,
                                      UnwrapPtr(m_MS2BufferDescSet), 0, NULL);

  Vec4u params = {1, slice, sample, fmtIndex};

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_MS2BufferPipeLayout), VK_SHADER_STAGE_ALL, 0,
                                 sizeof(Vec4u), &params);

  // for D16 textures, we need to multisample.
  if(fmtIndex == 0)
  {
    const uint32_t ms = 2;
    // Use a 1D workgroup size so that we don't have to worry about width or height
    // being a multiple of our multisample size
    ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), AlignUp(extent.width * extent.height, ms) / ms, 1, 1);
  }
  else
  {
    ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), extent.width, extent.height, 1);
  }

  // finished, submit and flush
  ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

  cmd = VK_NULL_HANDLE;

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcDepthView, NULL);
  if(srcStencilView != VK_NULL_HANDLE)
    ObjDisp(dev)->DestroyImageView(Unwrap(dev), srcStencilView, NULL);
}
