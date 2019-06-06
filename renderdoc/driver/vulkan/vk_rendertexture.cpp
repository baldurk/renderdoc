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

#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_debug.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

void VulkanReplay::CreateTexImageView(VkImage liveIm, const VulkanCreationInfo::Image &iminfo,
                                      CompType typeHint, TextureDisplayViews &views)
{
  VkDevice dev = m_pDriver->GetDev();

  if(views.typeHint != typeHint)
  {
    // if the type hint has changed, recreate the image views
    for(size_t i = 0; i < ARRAY_COUNT(views.views); i++)
    {
      m_pDriver->vkDestroyImageView(dev, views.views[i], NULL);
      views.views[i] = VK_NULL_HANDLE;
    }
  }

  views.typeHint = typeHint;

  VkFormat fmt = views.castedFormat = GetViewCastedFormat(iminfo.format, typeHint);

  // all types have at least views[0] populated, so if it's still there, we can just return
  if(views.views[0] != VK_NULL_HANDLE)
    return;

  VkImageViewCreateInfo viewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      0,
      liveIm,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      fmt,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, RDCMAX(1U, (uint32_t)iminfo.mipLevels), 0,
          RDCMAX(1U, (uint32_t)iminfo.arrayLayers),
      },
  };

  // for the stencil-only format, the first view is stencil only
  if(fmt == VK_FORMAT_S8_UINT)
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
  // otherwise for depth or stencil formats, the first view is depth.
  else if(IsDepthOrStencilFormat(fmt))
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  if(iminfo.type == VK_IMAGE_TYPE_1D)
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
  else if(iminfo.type == VK_IMAGE_TYPE_3D)
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

  VkResult vkr = VK_SUCCESS;

  if(IsYUVFormat(fmt))
  {
    const uint32_t planeCount = GetYUVPlaneCount(fmt);

    for(uint32_t i = 0; i < planeCount; i++)
    {
      viewInfo.format = GetYUVViewPlaneFormat(fmt, i);

      if(planeCount > 1)
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT << i;

      // create as wrapped
      vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &views.views[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }
  else
  {
    // create first view
    vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &views.views[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // for depth-stencil images, create a second view for stencil only
    if(IsDepthAndStencilFormat(fmt))
    {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

      vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &views.views[1]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }
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
  if(outw.m_WindowSystem != WindowingSystem::Headless && outw.swap == VK_NULL_HANDLE)
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
  const bool greenonly = (flags & eTexDisplay_GreenOnly) != 0;
  const bool f32render = (flags & eTexDisplay_F32Render) != 0;

  VkDevice dev = m_pDriver->GetDev();
  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  const VkLayerDispatchTable *vt = ObjDisp(dev);

  ImageLayouts &layouts = m_pDriver->m_ImageLayouts[cfg.resourceId];
  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[cfg.resourceId];
  TextureDisplayViews &texviews = m_TexRender.TextureViews[cfg.resourceId];
  VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(cfg.resourceId);
  const ImageInfo &imageInfo = layouts.imageInfo;

  CreateTexImageView(liveIm, iminfo, cfg.typeHint, texviews);

  int displayformat = 0;
  uint32_t descSetBinding = 0;

  if(IsUIntFormat(texviews.castedFormat))
  {
    descSetBinding = 10;
    displayformat |= TEXDISPLAY_UINT_TEX;
  }
  else if(IsSIntFormat(texviews.castedFormat))
  {
    descSetBinding = 15;
    displayformat |= TEXDISPLAY_SINT_TEX;
  }
  else
  {
    descSetBinding = 5;
  }

  // by default we use view 0
  int viewIndex = 0;

  // if we're displaying the stencil, set up for stencil display
  if(imageInfo.format == VK_FORMAT_S8_UINT ||
     (IsStencilFormat(imageInfo.format) && !cfg.red && cfg.green))
  {
    descSetBinding = 10;
    displayformat |= TEXDISPLAY_UINT_TEX;

    // for stencil we use view 1 as long as it's a depth-stencil texture
    if(IsDepthAndStencilFormat(imageInfo.format))
      viewIndex = 1;

    // rescale the range so that stencil seems to fit to 0-1
    cfg.rangeMin *= 255.0f;
    cfg.rangeMax *= 255.0f;

    // shuffle the channel selection, since stencil comes back in red
    cfg.red = true;
    cfg.green = false;
  }

  VkImageView liveImView = texviews.views[viewIndex];

  RDCASSERT(liveImView != VK_NULL_HANDLE);

  uint32_t uboOffs = 0;

  TexDisplayUBOData *data = (TexDisplayUBOData *)m_TexRender.UBO.Map(&uboOffs);

  data->Padding = 0;

  float x = cfg.xOffset;
  float y = cfg.yOffset;

  data->Position.x = x;
  data->Position.y = y;
  data->HDRMul = cfg.hdrMultiplier;
  data->DecodeYUV = cfg.decodeYUV ? 1 : 0;

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(texviews.castedFormat, YUVDownsampleRate, YUVAChannels);

  data->YUVDownsampleRate = YUVDownsampleRate;
  data->YUVAChannels = YUVAChannels;

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
  {
    uint32_t numSlices =
        RDCMAX((uint32_t)iminfo.arrayLayers, 1U) * RDCMAX((uint32_t)iminfo.samples, 1U);

    uint32_t sliceFace = RDCCLAMP(cfg.sliceFace, 0U, numSlices - 1);
    data->Slice = (float)sliceFace + 0.001f;
  }
  else
  {
    uint32_t sliceFace = RDCCLAMP(cfg.sliceFace, 0U, iminfo.extent.depth - 1);
    data->Slice = (float)(sliceFace >> cfg.mip);
  }

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
  {
    textype = RESTYPE_TEX1D;
  }
  else if(iminfo.type == VK_IMAGE_TYPE_3D)
  {
    textype = RESTYPE_TEX3D;
  }
  else if(iminfo.type == VK_IMAGE_TYPE_2D)
  {
    textype = RESTYPE_TEX2D;
    if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
      textype = RESTYPE_TEX2DMS;
  }

  displayformat |= textype;

  descSetBinding += textype;

  if(!IsSRGBFormat(texviews.castedFormat) && cfg.linearDisplayAsGamma)
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
      Vec4u YUVDownsampleRate;
      Vec4u YUVAChannels;
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
    customData->YUVDownsampleRate = YUVDownsampleRate;
    customData->YUVAChannels = YUVAChannels;
  }

  m_TexRender.UBO.Unmap();

  HeatmapData heatmapData = {};

  {
    if(cfg.overlay == DebugOverlay::QuadOverdrawDraw || cfg.overlay == DebugOverlay::QuadOverdrawPass)
    {
      heatmapData.HeatmapMode = HEATMAP_LINEAR;
    }
    else if(cfg.overlay == DebugOverlay::TriangleSizeDraw ||
            cfg.overlay == DebugOverlay::TriangleSizePass)
    {
      heatmapData.HeatmapMode = HEATMAP_TRISIZE;
    }

    if(heatmapData.HeatmapMode)
    {
      memcpy(heatmapData.ColorRamp, colorRamp, sizeof(colorRamp));

      RDCCOMPILE_ASSERT(sizeof(heatmapData.ColorRamp) == sizeof(colorRamp),
                        "C++ color ramp array is not the same size as the shader array");
    }
  }

  uint32_t heatUboOffs = 0;

  {
    HeatmapData *ptr = (HeatmapData *)m_TexRender.HeatmapUBO.Map(&heatUboOffs);
    memcpy(ptr, &heatmapData, sizeof(HeatmapData));
    m_TexRender.HeatmapUBO.Unmap();
  }

  VkDescriptorImageInfo imdesc = {0};
  imdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imdesc.imageView = Unwrap(liveImView);
  imdesc.sampler = Unwrap(m_General.PointSampler);
  if(cfg.mip == 0 && cfg.scale < 1.0f)
    imdesc.sampler = Unwrap(m_TexRender.LinearSampler);

  VkDescriptorImageInfo altimdesc[2] = {};
  for(uint32_t i = 1; i < GetYUVPlaneCount(texviews.castedFormat); i++)
  {
    RDCASSERT(texviews.views[i] != VK_NULL_HANDLE);
    altimdesc[i - 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    altimdesc[i - 1].imageView = Unwrap(texviews.views[i]);
    altimdesc[i - 1].sampler = Unwrap(m_General.PointSampler);
    if(cfg.mip == 0 && cfg.scale < 1.0f)
      altimdesc[i - 1].sampler = Unwrap(m_TexRender.LinearSampler);
  }

  VkDescriptorSet descset = m_TexRender.GetDescSet();

  VkDescriptorBufferInfo ubodesc = {}, heatubodesc = {};
  m_TexRender.UBO.FillDescriptor(ubodesc);
  m_TexRender.HeatmapUBO.FillDescriptor(heatubodesc);

  VkWriteDescriptorSet writeSet[] = {
      // sampled view
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descset), descSetBinding, 0, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imdesc, NULL, NULL},
      // YUV secondary planes (if needed)
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descset), 10, 0,
       GetYUVPlaneCount(texviews.castedFormat) - 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       altimdesc, NULL, NULL},
      // UBOs
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descset), 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &ubodesc, NULL},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descset), 1, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NULL, &heatubodesc, NULL},
  };

  std::vector<VkWriteDescriptorSet> writeSets;
  for(size_t i = 0; i < ARRAY_COUNT(writeSet); i++)
  {
    if(writeSet[i].descriptorCount > 0)
      writeSets.push_back(writeSet[i]);
  }

  for(size_t i = 0; i < ARRAY_COUNT(m_TexRender.DummyWrites); i++)
  {
    VkWriteDescriptorSet &write = m_TexRender.DummyWrites[i];

    // don't write dummy data in the actual slot
    if(write.dstBinding == descSetBinding)
      continue;

    // don't overwrite YUV texture slots if it's a YUV planar format
    if(write.dstBinding == 10)
    {
      if(write.dstArrayElement == 0 && GetYUVPlaneCount(texviews.castedFormat) >= 2)
        continue;
      if(write.dstArrayElement == 1 && GetYUVPlaneCount(texviews.castedFormat) >= 3)
        continue;
    }

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

    SanitiseOldImageLayout(srcimBarrier.oldLayout);

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

    VkPipeline pipe = greenonly ? m_TexRender.PipelineGreenOnly : m_TexRender.Pipeline;

    if(cfg.customShaderId != ResourceId())
    {
      GetDebugManager()->CreateCustomShaderPipeline(cfg.customShaderId, m_TexRender.PipeLayout);
      pipe = GetDebugManager()->GetCustomPipeline();
    }
    else if(f16render)
    {
      pipe = greenonly ? m_TexRender.F16PipelineGreenOnly : m_TexRender.F16Pipeline;
    }
    else if(f32render)
    {
      pipe = greenonly ? m_TexRender.F32PipelineGreenOnly : m_TexRender.F32Pipeline;
    }
    else if(!cfg.rawOutput && blendAlpha && cfg.customShaderId == ResourceId())
    {
      pipe = m_TexRender.BlendPipeline;
    }

    uint32_t offsets[] = {uboOffs, heatUboOffs};

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_TexRender.PipeLayout), 0, 1, UnwrapPtr(descset), 2, offsets);

    VkViewport viewport = {(float)rpbegin.renderArea.offset.x,
                           (float)rpbegin.renderArea.offset.y,
                           (float)m_DebugWidth,
                           (float)m_DebugHeight,
                           0.0f,
                           1.0f};
    vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

    if(m_pDriver->GetDriverInfo().QualcommLeakingUBOOffsets())
    {
      offsets[0] = offsets[1] = 0;
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_TexRender.PipeLayout), 0, 1, UnwrapPtr(descset), 2, offsets);
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
