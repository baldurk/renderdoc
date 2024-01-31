/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

void VulkanReplay::CreateTexImageView(VkImage liveIm, const VulkanCreationInfo::Image &iminfo,
                                      CompType typeCast, TextureDisplayViews &views)
{
  VkDevice dev = m_pDriver->GetDev();

  if(views.typeCast != typeCast)
  {
    // if the type hint has changed, recreate the image views

    // flush any pending commands that might use the old views
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    for(size_t i = 0; i < ARRAY_COUNT(views.views); i++)
    {
      m_pDriver->vkDestroyImageView(dev, views.views[i], NULL);
      views.views[i] = VK_NULL_HANDLE;
    }
  }

  views.typeCast = typeCast;

  VkFormat fmt = views.castedFormat = GetViewCastedFormat(iminfo.format, typeCast);

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
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          RDCMAX(1U, iminfo.mipLevels),
          0,
          RDCMAX(1U, iminfo.arrayLayers),
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
      CheckVkResult(vkr);
    }
  }
  else
  {
    // create first view
    vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &views.views[0]);
    CheckVkResult(vkr);
    NameVulkanObject(views.views[0], StringFormat::Fmt("CreateTexImageView view 0 %s",
                                                       ToStr(GetResID(liveIm)).c_str()));

    // for depth-stencil images, create a second view for stencil only
    if(IsDepthAndStencilFormat(fmt))
    {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

      vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &views.views[1]);
      CheckVkResult(vkr);
      NameVulkanObject(views.views[1], StringFormat::Fmt("CreateTexImageView view 1 %s",
                                                         ToStr(GetResID(liveIm)).c_str()));
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
           0,
           0,
       },
       {m_DebugWidth, m_DebugHeight}},
      0,
      NULL,
  };

  LockedConstImageStateRef imageState = m_pDriver->FindConstImageState(cfg.resourceId);
  if(!imageState)
  {
    RDCWARN("Could not find image info for image %s", ToStr(cfg.resourceId).c_str());
    return false;
  }
  if(!imageState->isMemoryBound)
    return false;

  return RenderTextureInternal(cfg, *imageState, rpbegin,
                               eTexDisplay_MipShift | eTexDisplay_BlendAlpha);
}

bool VulkanReplay::RenderTextureInternal(TextureDisplay cfg, const ImageState &imageState,
                                         VkRenderPassBeginInfo rpbegin, int flags)
{
  const bool blendAlpha = (flags & eTexDisplay_BlendAlpha) != 0;
  const bool mipShift = (flags & eTexDisplay_MipShift) != 0;
  const bool f16render = (flags & eTexDisplay_16Render) != 0;
  const bool greenonly = (flags & eTexDisplay_GreenOnly) != 0;
  const bool f32render = (flags & eTexDisplay_32Render) != 0;

  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  const ImageInfo &imageInfo = imageState.GetImageInfo();

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[cfg.resourceId];
  TextureDisplayViews &texviews = m_TexRender.TextureViews[cfg.resourceId];
  VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(cfg.resourceId);

  CreateTexImageView(liveIm, iminfo, cfg.typeCast, texviews);

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

  if(!data)
    return false;

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

  const bool linearSample = cfg.subresource.mip == 0 && cfg.scale < 1.0f &&
                            (displayformat & (TEXDISPLAY_UINT_TEX | TEXDISPLAY_SINT_TEX)) == 0;

  data->MipLevel = (int)cfg.subresource.mip;
  data->Slice = 0;
  if(iminfo.type != VK_IMAGE_TYPE_3D)
  {
    uint32_t numSlices = RDCMAX((uint32_t)iminfo.arrayLayers, 1U);

    uint32_t sliceFace = RDCCLAMP(cfg.subresource.slice, 0U, numSlices - 1);
    data->Slice = (float)sliceFace + 0.001f;
  }
  else
  {
    float slice = (float)RDCCLAMP(cfg.subresource.slice, 0U, iminfo.extent.depth - 1);

    // when sampling linearly, we need to add half a pixel to ensure we only sample the desired
    // slice
    if(linearSample)
      slice += 0.5f;
    else
      slice += 0.001f;

    data->Slice = slice;
  }

  data->TextureResolutionPS.x = float(RDCMAX(1, tex_x >> cfg.subresource.mip));
  data->TextureResolutionPS.y = float(RDCMAX(1, tex_y >> cfg.subresource.mip));
  data->TextureResolutionPS.z = float(RDCMAX(1, tex_z >> cfg.subresource.mip));

  if(mipShift)
    data->MipShift = float(1 << cfg.subresource.mip);
  else
    data->MipShift = 1.0f;

  data->Scale = cfg.scale;

  int sampleIdx = (int)RDCCLAMP(cfg.subresource.sample, 0U, (uint32_t)SampleCount(iminfo.samples));

  if(cfg.subresource.sample == ~0U)
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

    RD_CustomShader_UBO_Type *customData = (RD_CustomShader_UBO_Type *)data;

    customData->TexDim.x = iminfo.extent.width;
    customData->TexDim.y = iminfo.extent.height;
    customData->TexDim.z = iminfo.type == VK_IMAGE_TYPE_3D ? iminfo.extent.depth : iminfo.arrayLayers;
    customData->TexDim.w = iminfo.mipLevels;
    customData->SelectedMip = cfg.subresource.mip;
    customData->SelectedSliceFace = cfg.subresource.slice;
    customData->SelectedSample = sampleIdx;
    customData->TextureType = (uint32_t)textype;
    customData->YUVDownsampleRate = YUVDownsampleRate;
    customData->YUVAChannels = YUVAChannels;
    customData->SelectedRange.x = cfg.rangeMin;
    customData->SelectedRange.y = cfg.rangeMax;
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
    if(!ptr)
      return false;
    memcpy(ptr, &heatmapData, sizeof(HeatmapData));
    m_TexRender.HeatmapUBO.Unmap();
  }

  VkDescriptorImageInfo imdesc = {0};
  imdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imdesc.imageView = Unwrap(liveImView);
  imdesc.sampler = Unwrap(m_General.PointSampler);
  if(linearSample)
    imdesc.sampler = Unwrap(m_TexRender.LinearSampler);

  VkDescriptorImageInfo altimdesc[2] = {};
  for(uint32_t i = 1; i < GetYUVPlaneCount(texviews.castedFormat); i++)
  {
    RDCASSERT(texviews.views[i] != VK_NULL_HANDLE);
    altimdesc[i - 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    altimdesc[i - 1].imageView = Unwrap(texviews.views[i]);
    altimdesc[i - 1].sampler = Unwrap(m_General.PointSampler);
    if(linearSample)
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

  rdcarray<VkWriteDescriptorSet> writeSets;
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

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return false;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  VkMarkerRegion::Begin("RenderTexture", cmd);

  ImageBarrierSequence setupBarriers, cleanupBarriers;
  imageState.TempTransition(m_pDriver->GetQueueFamilyIndex(),
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                            setupBarriers, cleanupBarriers, m_pDriver->GetImageTransitionInfo());
  m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
  m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);
  {
    vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    VkPipeline pipe = greenonly ? m_TexRender.PipelineGreenOnly : m_TexRender.Pipeline;

    if(cfg.customShaderId != ResourceId())
    {
      GetDebugManager()->CreateCustomShaderPipeline(cfg.customShaderId, m_TexRender.PipeLayout);
      pipe = GetDebugManager()->GetCustomPipeline();
    }
    else if(flags & (eTexDisplay_RemapFloat | eTexDisplay_RemapUInt | eTexDisplay_RemapSInt))
    {
      int i = 0;
      if(flags & eTexDisplay_RemapFloat)
        i = 0;
      else if(flags & eTexDisplay_RemapUInt)
        i = 1;
      else if(flags & eTexDisplay_RemapSInt)
        i = 2;

      int f = 0;
      if(flags & eTexDisplay_32Render)
        f = 2;
      else if(flags & eTexDisplay_16Render)
        f = 1;
      else
        f = 0;

      bool srgb = (flags & eTexDisplay_RemapSRGB) != 0;

      pipe = m_TexRender.RemapPipeline[f][i][(greenonly || srgb) ? 1 : 0];
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
                           (float)rpbegin.renderArea.extent.width,
                           (float)rpbegin.renderArea.extent.height,
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

  m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);
  VkMarkerRegion::End(cmd);
  vt->EndCommandBuffer(Unwrap(cmd));
  if(!cleanupBarriers.empty())
  {
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
    m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);
  }
  else if(Vulkan_Debug_SingleSubmitFlushing())
  {
    m_pDriver->SubmitCmds();
  }

  return true;
}
