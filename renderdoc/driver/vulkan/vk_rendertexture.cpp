/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_debug.h"

#define VULKAN 1
#include "data/glsl/debuguniforms.h"

void VulkanReplay::CreateTexImageView(VkImageAspectFlags aspectFlags, VkImage liveIm,
                                      VulkanCreationInfo::Image &iminfo)
{
  VkDevice dev = m_pDriver->GetDev();

  if(aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    if(iminfo.stencilView != VK_NULL_HANDLE)
      return;
  }
  else
  {
    if(iminfo.view != VK_NULL_HANDLE)
      return;
  }

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      Unwrap(liveIm),
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      iminfo.format,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          aspectFlags, 0, RDCMAX(1U, (uint32_t)iminfo.mipLevels), 0,
          RDCMAX(1U, (uint32_t)iminfo.arrayLayers),
      },
  };

  if(iminfo.type == VK_IMAGE_TYPE_1D)
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
  if(iminfo.type == VK_IMAGE_TYPE_3D)
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

  if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT)
  {
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_ZERO;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_ZERO;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_ZERO;
  }
  else if(aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT)
  {
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_ZERO;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_ZERO;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_ZERO;
  }

  VkImageView view;

  VkResult vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &view);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ResourceId viewid = m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), view);
  // register as a live-only resource, so it is cleaned up properly
  m_pDriver->GetResourceManager()->AddLiveResource(viewid, view);

  if(aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT)
    iminfo.stencilView = view;
  else
    iminfo.view = view;
}

bool VulkanReplay::RenderTexture(TextureDisplay cfg)
{
  auto it = m_OutputWindows.find(m_ActiveWinID);
  if(it == m_OutputWindows.end())
  {
    RDCERR("output window not bound");
    return false;
  }

  OutputWindow &outw = it->second;

  // if the swapchain failed to create, do nothing. We will try to recreate it
  // again in CheckResizeOutputWindow (once per render 'frame')
  if(outw.swap == VK_NULL_HANDLE)
    return false;

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(outw.rp),
      Unwrap(outw.fb),
      {{
           0, 0,
       },
       {m_DebugWidth, m_DebugHeight}},
      0,
      NULL,
  };

  return RenderTextureInternal(cfg, rpbegin, eTexDisplay_MipShift | eTexDisplay_BlendAlpha);
}

bool VulkanReplay::RenderTextureInternal(TextureDisplay cfg, VkRenderPassBeginInfo rpbegin, int flags)
{
  const bool blendAlpha = (flags & eTexDisplay_BlendAlpha) != 0;
  const bool mipShift = (flags & eTexDisplay_MipShift) != 0;
  const bool f16render = (flags & eTexDisplay_F16Render) != 0;
  const bool f32render = (flags & eTexDisplay_F32Render) != 0;

  VkDevice dev = m_pDriver->GetDev();
  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  const VkLayerDispatchTable *vt = ObjDisp(dev);

  ImageLayouts &layouts = m_pDriver->m_ImageLayouts[cfg.resourceId];
  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[cfg.resourceId];
  VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(cfg.resourceId);

  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

  int displayformat = 0;
  uint32_t descSetBinding = 0;

  if(IsUIntFormat(iminfo.format))
  {
    descSetBinding = 10;
    displayformat |= TEXDISPLAY_UINT_TEX;
  }
  else if(IsSIntFormat(iminfo.format))
  {
    descSetBinding = 15;
    displayformat |= TEXDISPLAY_SINT_TEX;
  }
  else
  {
    descSetBinding = 5;
  }

  if(IsDepthOnlyFormat(layouts.format))
  {
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  else if(IsDepthOrStencilFormat(layouts.format))
  {
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if(layouts.format == VK_FORMAT_S8_UINT || (!cfg.red && cfg.green))
    {
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
      descSetBinding = 10;
      displayformat |= TEXDISPLAY_UINT_TEX;

      // rescale the range so that stencil seems to fit to 0-1
      cfg.rangeMin *= 255.0f;
      cfg.rangeMax *= 255.0f;
    }
  }

  CreateTexImageView(aspectFlags, liveIm, iminfo);

  VkImageView liveImView =
      (aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT ? iminfo.stencilView : iminfo.view);

  RDCASSERT(liveImView != VK_NULL_HANDLE);

  uint32_t uboOffs = 0;

  TexDisplayUBOData *data = (TexDisplayUBOData *)m_TexRender.UBO.Map(&uboOffs);

  data->Padding = 0;

  float x = cfg.xOffset;
  float y = cfg.yOffset;

  data->Position.x = x;
  data->Position.y = y;
  data->HDRMul = cfg.hdrMultiplier;

  int32_t tex_x = iminfo.extent.width;
  int32_t tex_y = iminfo.extent.height;
  int32_t tex_z = iminfo.extent.depth;

  if(cfg.scale <= 0.0f)
  {
    float xscale = float(m_DebugWidth) / float(tex_x);
    float yscale = float(m_DebugHeight) / float(tex_y);

    // update cfg.scale for use below
    float scale = cfg.scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      data->Position.x = 0;
      data->Position.y = (float(m_DebugHeight) - (tex_y * scale)) * 0.5f;
    }
    else
    {
      data->Position.y = 0;
      data->Position.x = (float(m_DebugWidth) - (tex_x * scale)) * 0.5f;
    }
  }

  data->Channels.x = cfg.red ? 1.0f : 0.0f;
  data->Channels.y = cfg.green ? 1.0f : 0.0f;
  data->Channels.z = cfg.blue ? 1.0f : 0.0f;
  data->Channels.w = cfg.alpha ? 1.0f : 0.0f;

  if(cfg.rangeMax <= cfg.rangeMin)
    cfg.rangeMax += 0.00001f;

  data->RangeMinimum = cfg.rangeMin;
  data->InverseRangeSize = 1.0f / (cfg.rangeMax - cfg.rangeMin);

  data->FlipY = cfg.flipY ? 1 : 0;

  data->MipLevel = (int)cfg.mip;
  data->Slice = 0;
  if(iminfo.type != VK_IMAGE_TYPE_3D)
    data->Slice = (float)cfg.sliceFace + 0.001f;
  else
    data->Slice = (float)(cfg.sliceFace >> cfg.mip);

  data->TextureResolutionPS.x = float(RDCMAX(1, tex_x >> cfg.mip));
  data->TextureResolutionPS.y = float(RDCMAX(1, tex_y >> cfg.mip));
  data->TextureResolutionPS.z = float(RDCMAX(1, tex_z >> cfg.mip));

  if(mipShift)
    data->MipShift = float(1 << cfg.mip);
  else
    data->MipShift = 1.0f;

  data->Scale = cfg.scale;

  int sampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, (uint32_t)SampleCount(iminfo.samples));

  sampleIdx = cfg.sampleIdx;

  if(cfg.sampleIdx == ~0U)
    sampleIdx = -SampleCount(iminfo.samples);

  data->SampleIdx = sampleIdx;

  data->OutputRes.x = (float)m_DebugWidth;
  data->OutputRes.y = (float)m_DebugHeight;

  int textype = 0;

  if(iminfo.type == VK_IMAGE_TYPE_1D)
    textype = RESTYPE_TEX1D;
  if(iminfo.type == VK_IMAGE_TYPE_3D)
    textype = RESTYPE_TEX3D;
  if(iminfo.type == VK_IMAGE_TYPE_2D)
  {
    textype = RESTYPE_TEX2D;
    if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
      textype = RESTYPE_TEX2DMS;
  }

  displayformat |= textype;

  descSetBinding += textype;

  if(!IsSRGBFormat(iminfo.format) && cfg.linearDisplayAsGamma)
    displayformat |= TEXDISPLAY_GAMMA_CURVE;

  if(cfg.overlay == DebugOverlay::NaN)
    displayformat |= TEXDISPLAY_NANS;

  if(cfg.overlay == DebugOverlay::Clipping)
    displayformat |= TEXDISPLAY_CLIPPING;

  data->OutputDisplayFormat = displayformat;

  data->RawOutput = cfg.rawOutput ? 1 : 0;

  if(cfg.customShaderId != ResourceId())
  {
    // must match struct declared in user shader (see documentation / Shader Viewer window helper
    // menus)
    struct CustomTexDisplayUBOData
    {
      Vec4u texDim;
      uint32_t selectedMip;
      uint32_t texType;
      uint32_t selectedSliceFace;
      int32_t selectedSample;
    };

    CustomTexDisplayUBOData *customData = (CustomTexDisplayUBOData *)data;

    customData->texDim.x = iminfo.extent.width;
    customData->texDim.y = iminfo.extent.height;
    customData->texDim.z = iminfo.extent.depth;
    customData->texDim.w = iminfo.mipLevels;
    customData->selectedMip = cfg.mip;
    customData->selectedSliceFace = cfg.sliceFace;
    customData->selectedSample = sampleIdx;
    customData->texType = (uint32_t)textype;
  }

  m_TexRender.UBO.Unmap();

  VkDescriptorImageInfo imdesc = {0};
  imdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imdesc.imageView = Unwrap(liveImView);
  imdesc.sampler = Unwrap(m_General.PointSampler);
  if(cfg.mip == 0 && cfg.scale < 1.0f)
    imdesc.sampler = Unwrap(m_TexRender.LinearSampler);

  VkDescriptorSet descset = m_TexRender.GetDescSet();

  VkDescriptorBufferInfo ubodesc = {0};
  m_TexRender.UBO.FillDescriptor(ubodesc);

  VkWriteDescriptorSet writeSet[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descset), descSetBinding, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imdesc, NULL, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descset), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &ubodesc, NULL},
  };

  vector<VkWriteDescriptorSet> writeSets;
  for(size_t i = 0; i < ARRAY_COUNT(writeSet); i++)
    writeSets.push_back(writeSet[i]);

  for(size_t i = 0; i < ARRAY_COUNT(m_TexRender.DummyWrites); i++)
  {
    VkWriteDescriptorSet &write = m_TexRender.DummyWrites[i];

    // don't write dummy data in the actual slot
    if(write.dstBinding == descSetBinding)
      continue;

    write.dstSet = Unwrap(descset);
    writeSets.push_back(write);
  }

  vt->UpdateDescriptorSets(Unwrap(dev), (uint32_t)writeSets.size(), &writeSets[0], 0, NULL);

  VkImageMemoryBarrier srcimBarrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      NULL,
      0,
      0,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      layouts.queueFamilyIndex,
      m_pDriver->GetQueueFamilyIndex(),
      Unwrap(liveIm),
      {0, 0, 1, 0, 1}    // will be overwritten by subresourceRange
  };

  // ensure all previous writes have completed
  srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
  // before we go reading
  srcimBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  VkCommandBuffer extQCmd = VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  if(srcimBarrier.srcQueueFamilyIndex != srcimBarrier.dstQueueFamilyIndex)
  {
    extQCmd = m_pDriver->GetExtQueueCmd(srcimBarrier.srcQueueFamilyIndex);

    vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  for(size_t si = 0; si < layouts.subresourceStates.size(); si++)
  {
    srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
    srcimBarrier.oldLayout = layouts.subresourceStates[si].newLayout;
    srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS | MakeAccessMask(srcimBarrier.oldLayout);
    DoPipelineBarrier(cmd, 1, &srcimBarrier);

    if(extQCmd != VK_NULL_HANDLE)
      DoPipelineBarrier(extQCmd, 1, &srcimBarrier);
  }

  if(extQCmd != VK_NULL_HANDLE)
  {
    vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitAndFlushExtQueue(layouts.queueFamilyIndex);
  }

  srcimBarrier.oldLayout = srcimBarrier.newLayout;
  srcimBarrier.srcAccessMask = srcimBarrier.dstAccessMask;

  {
    vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    VkPipeline pipe = m_TexRender.Pipeline;

    if(cfg.customShaderId != ResourceId())
    {
      GetDebugManager()->CreateCustomShaderPipeline(cfg.customShaderId, m_TexRender.PipeLayout);
      pipe = GetDebugManager()->GetCustomPipeline();
    }
    else if(f16render)
    {
      pipe = m_TexRender.F16Pipeline;
    }
    else if(f32render)
    {
      pipe = m_TexRender.F32Pipeline;
    }
    else if(!cfg.rawOutput && blendAlpha && cfg.customShaderId == ResourceId())
    {
      pipe = m_TexRender.BlendPipeline;
    }

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_TexRender.PipeLayout), 0, 1, UnwrapPtr(descset), 1, &uboOffs);

    VkViewport viewport = {(float)rpbegin.renderArea.offset.x,
                           (float)rpbegin.renderArea.offset.y,
                           (float)m_DebugWidth,
                           (float)m_DebugHeight,
                           0.0f,
                           1.0f};
    vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

    if(m_pDriver->GetDriverVersion().QualcommLeakingUBOOffsets())
    {
      uboOffs = 0;
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_TexRender.PipeLayout), 0, 1, UnwrapPtr(descset), 1,
                                &uboOffs);
    }

    vt->CmdEndRenderPass(Unwrap(cmd));
  }

  std::swap(srcimBarrier.srcQueueFamilyIndex, srcimBarrier.dstQueueFamilyIndex);

  if(extQCmd != VK_NULL_HANDLE)
  {
    vkr = ObjDisp(extQCmd)->BeginCommandBuffer(Unwrap(extQCmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  for(size_t si = 0; si < layouts.subresourceStates.size(); si++)
  {
    srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
    srcimBarrier.newLayout = layouts.subresourceStates[si].newLayout;
    srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);
    DoPipelineBarrier(cmd, 1, &srcimBarrier);

    if(extQCmd != VK_NULL_HANDLE)
      DoPipelineBarrier(extQCmd, 1, &srcimBarrier);
  }

  vt->EndCommandBuffer(Unwrap(cmd));

  if(extQCmd != VK_NULL_HANDLE)
  {
    // ensure work is completed before we pass ownership back to original queue
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    vkr = ObjDisp(extQCmd)->EndCommandBuffer(Unwrap(extQCmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitAndFlushExtQueue(layouts.queueFamilyIndex);
  }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  return true;
}
