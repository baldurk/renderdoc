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

#include <float.h>
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "data/glsl_shaders.h"
#include "driver/shaders/spirv/spirv_common.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

struct VulkanQuadOverdrawCallback : public VulkanDrawcallCallback
{
  VulkanQuadOverdrawCallback(WrappedVulkan *vk, VkDescriptorSetLayout descSetLayout,
                             VkDescriptorSet descSet, const std::vector<uint32_t> &events)
      : m_pDriver(vk),
        m_DescSetLayout(descSetLayout),
        m_DescSet(descSet),
        m_Events(events),
        m_PrevState(vk, NULL)
  {
    m_pDriver->SetDrawcallCB(this);
  }
  ~VulkanQuadOverdrawCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return;

    // we customise the pipeline to disable framebuffer writes, but perform normal testing
    // and substitute our quad calculation fragment shader that writes to a storage image
    // that is bound in a new descriptor set.

    VkResult vkr = VK_SUCCESS;

    m_PrevState = m_pDriver->GetRenderState();
    VulkanRenderState &pipestate = m_pDriver->GetRenderState();

    // check cache first
    rdcpair<uint32_t, VkPipeline> pipe = m_PipelineCache[pipestate.graphics.pipeline];

    // if we don't get a hit, create a modified pipeline
    if(pipe.second == VK_NULL_HANDLE)
    {
      VulkanCreationInfo &c = *pipestate.m_CreationInfo;

      VulkanCreationInfo::Pipeline &p = c.m_Pipeline[pipestate.graphics.pipeline];

      VkDescriptorSetLayout *descSetLayouts;

      // descSet will be the index of our new descriptor set
      uint32_t descSet = (uint32_t)c.m_PipelineLayout[p.layout].descSetLayouts.size();

      descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

      for(uint32_t i = 0; i < descSet; i++)
        descSetLayouts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
            c.m_PipelineLayout[p.layout].descSetLayouts[i]);

      // this layout has storage image and
      descSetLayouts[descSet] = m_DescSetLayout;

      const std::vector<VkPushConstantRange> &push = c.m_PipelineLayout[p.layout].pushRanges;

      VkPipelineLayoutCreateInfo pipeLayoutInfo = {
          VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          NULL,
          0,
          descSet + 1,
          descSetLayouts,
          (uint32_t)push.size(),
          push.empty() ? NULL : &push[0],
      };

      // create pipeline layout with same descriptor set layouts, plus our mesh output set
      VkPipelineLayout pipeLayout;
      vkr =
          m_pDriver->vkCreatePipelineLayout(m_pDriver->GetDev(), &pipeLayoutInfo, NULL, &pipeLayout);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      SAFE_DELETE_ARRAY(descSetLayouts);

      VkGraphicsPipelineCreateInfo pipeCreateInfo;
      m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                            pipestate.graphics.pipeline);

      // repoint pipeline layout
      pipeCreateInfo.layout = pipeLayout;

      // disable colour writes/blends
      VkPipelineColorBlendStateCreateInfo *cb =
          (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
      for(uint32_t i = 0; i < cb->attachmentCount; i++)
      {
        VkPipelineColorBlendAttachmentState *att =
            (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
        att->blendEnable = false;
        att->colorWriteMask = 0x0;
      }

      // disable depth/stencil writes but keep any tests enabled
      VkPipelineDepthStencilStateCreateInfo *ds =
          (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
      ds->depthWriteEnable = false;
      ds->front.passOp = ds->front.failOp = ds->front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds->back.passOp = ds->back.failOp = ds->back.depthFailOp = VK_STENCIL_OP_KEEP;

      // don't discard
      VkPipelineRasterizationStateCreateInfo *rs =
          (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
      rs->rasterizerDiscardEnable = false;

      std::vector<uint32_t> spirv =
          *m_pDriver->GetShaderCache()->GetBuiltinBlob(BuiltinShader::QuadWriteFS);

      // patch spirv, change descriptor set to descSet value
      size_t it = 5;
      while(it < spirv.size())
      {
        uint16_t WordCount = spirv[it] >> spv::WordCountShift;
        spv::Op opcode = spv::Op(spirv[it] & spv::OpCodeMask);

        if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationDescriptorSet)
        {
          spirv[it + 3] = descSet;
          break;
        }

        it += WordCount;
      }

      VkShaderModuleCreateInfo modinfo = {
          VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          NULL,
          0,
          spirv.size() * sizeof(uint32_t),
          &spirv[0],
      };

      VkShaderModule module;

      VkDevice dev = m_pDriver->GetDev();

      vkr = ObjDisp(dev)->CreateShaderModule(Unwrap(dev), &modinfo, NULL, &module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), module);

      m_pDriver->GetResourceManager()->AddLiveResource(GetResID(module), module);

      bool found = false;
      for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
      {
        VkPipelineShaderStageCreateInfo &sh =
            (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
        if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
          sh.module = module;
          sh.pName = "main";
          found = true;
          break;
        }
      }

      if(!found)
      {
        // we know this is safe because it's pointing to a static array that's
        // big enough for all shaders

        VkPipelineShaderStageCreateInfo &sh =
            (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
        sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        sh.pNext = NULL;
        sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        sh.module = module;
        sh.pName = "main";
        sh.pSpecializationInfo = NULL;
      }

      vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                                 &pipe.second);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      ObjDisp(dev)->DestroyShaderModule(Unwrap(dev), Unwrap(module), NULL);
      m_pDriver->GetResourceManager()->ReleaseWrappedResource(module);

      pipe.first = descSet;

      m_PipelineCache[pipestate.graphics.pipeline] = pipe;
    }

    // modify state for first draw call
    pipestate.graphics.pipeline = GetResID(pipe.second);
    RDCASSERT(pipestate.graphics.descSets.size() >= pipe.first);
    pipestate.graphics.descSets.resize(pipe.first + 1);
    pipestate.graphics.descSets[pipe.first].descSet = GetResID(m_DescSet);

    if(cmd)
      pipestate.BindPipeline(cmd, VulkanRenderState::BindGraphics, false);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) == m_Events.end())
      return false;

    // restore the render state and go ahead with the real draw
    m_pDriver->GetRenderState() = m_PrevState;

    RDCASSERT(cmd);
    m_pDriver->GetRenderState().BindPipeline(cmd, VulkanRenderState::BindGraphics, false);

    return true;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // don't care
  }

  WrappedVulkan *m_pDriver;
  VkDescriptorSetLayout m_DescSetLayout;
  VkDescriptorSet m_DescSet;
  const std::vector<uint32_t> &m_Events;

  // cache modified pipelines
  std::map<ResourceId, rdcpair<uint32_t, VkPipeline> > m_PipelineCache;
  VulkanRenderState m_PrevState;
};

void VulkanDebugManager::PatchFixedColShader(VkShaderModule &mod, float col[4])
{
  union
  {
    uint32_t *spirv;
    float *data;
  } alias;

  std::vector<uint32_t> spv = *m_pDriver->GetShaderCache()->GetBuiltinBlob(BuiltinShader::FixedColFS);

  alias.spirv = &spv[0];
  size_t spirvLength = spv.size();

  int patched = 0;

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = alias.spirv[it] >> spv::WordCountShift;
    spv::Op opcode = spv::Op(alias.spirv[it] & spv::OpCodeMask);

    if(opcode == spv::OpConstant)
    {
      if(alias.data[it + 3] >= 1.0f && alias.data[it + 3] <= 1.5f)
        alias.data[it + 3] = col[0];
      else if(alias.data[it + 3] >= 2.0f && alias.data[it + 3] <= 2.5f)
        alias.data[it + 3] = col[1];
      else if(alias.data[it + 3] >= 3.0f && alias.data[it + 3] <= 3.5f)
        alias.data[it + 3] = col[2];
      else if(alias.data[it + 3] >= 4.0f && alias.data[it + 3] <= 4.5f)
        alias.data[it + 3] = col[3];
      else
        RDCERR("Unexpected constant value");

      patched++;
    }

    it += WordCount;
  }

  if(patched != 4)
    RDCERR("Didn't patch all constants");

  VkShaderModuleCreateInfo modinfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      NULL,
      0,
      spv.size() * sizeof(uint32_t),
      alias.spirv,
  };

  VkResult vkr = m_pDriver->vkCreateShaderModule(m_Device, &modinfo, NULL, &mod);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanDebugManager::PatchLineStripIndexBuffer(const DrawcallDescription *draw,
                                                   GPUBuffer &indexBuffer, uint32_t &indexCount)
{
  VulkanRenderState &rs = m_pDriver->m_RenderState;

  bytebuf indices;

  uint16_t *idx16 = NULL;
  uint32_t *idx32 = NULL;

  if(draw->flags & DrawFlags::Indexed)
  {
    GetBufferData(rs.ibuffer.buf,
                  rs.ibuffer.offs + uint64_t(draw->indexOffset) * draw->indexByteWidth,
                  uint64_t(draw->numIndices) * draw->indexByteWidth, indices);

    if(rs.ibuffer.bytewidth == 2)
      idx16 = (uint16_t *)indices.data();
    else
      idx32 = (uint32_t *)indices.data();
  }

  // we just patch up to 32-bit since we'll be adding more indices and we might overflow 16-bit.
  std::vector<uint32_t> patchedIndices;

  ::PatchLineStripIndexBuffer(draw, NULL, idx16, idx32, patchedIndices);

  indexBuffer.Create(m_pDriver, m_Device, patchedIndices.size() * sizeof(uint32_t), 1,
                     GPUBuffer::eGPUBufferIBuffer);

  void *ptr = indexBuffer.Map(0, patchedIndices.size() * sizeof(uint32_t));
  memcpy(ptr, patchedIndices.data(), patchedIndices.size() * sizeof(uint32_t));
  indexBuffer.Unmap();

  rs.ibuffer.offs = 0;
  rs.ibuffer.bytewidth = 4;
  rs.ibuffer.buf = GetResID(indexBuffer.buf);

  VkBufferMemoryBarrier uploadbarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_HOST_WRITE_BIT,
      VK_ACCESS_INDEX_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(indexBuffer.buf),
      0,
      indexBuffer.totalsize,
  };

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = ObjDisp(m_Device)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // ensure host writes finish before using as index buffer
  DoPipelineBarrier(cmd, 1, &uploadbarrier);

  ObjDisp(m_Device)->EndCommandBuffer(Unwrap(cmd));

  indexCount = (uint32_t)patchedIndices.size();
}

ResourceId VulkanReplay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                       uint32_t eventId, const std::vector<uint32_t> &passEvents)
{
  const VkLayerDispatchTable *vt = ObjDisp(m_Device);

  VulkanShaderCache *shaderCache = m_pDriver->GetShaderCache();

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];

  // bail out if the framebuffer dimensions don't match the current framebuffer, or draws will fail.
  // This is an order-of-operations problem, if the overlay is set when the event is changed it is
  // refreshed before the UI layer can update the current texture.
  {
    const VulkanCreationInfo::Framebuffer &fb =
        m_pDriver->m_CreationInfo.m_Framebuffer[m_pDriver->m_RenderState.framebuffer];

    if(fb.width != iminfo.extent.width || fb.height != iminfo.extent.height)
      return GetResID(m_Overlay.Image);
  }

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMarkerRegion::Begin(StringFormat::Fmt("RenderOverlay %d", overlay), cmd);

  // if the overlay image is the wrong size, free it
  if(m_Overlay.Image != VK_NULL_HANDLE && (iminfo.extent.width != m_Overlay.ImageDim.width ||
                                           iminfo.extent.height != m_Overlay.ImageDim.height))
  {
    m_pDriver->vkDestroyRenderPass(m_Device, m_Overlay.NoDepthRP, NULL);
    m_pDriver->vkDestroyFramebuffer(m_Device, m_Overlay.NoDepthFB, NULL);
    m_pDriver->vkDestroyImageView(m_Device, m_Overlay.ImageView, NULL);
    m_pDriver->vkDestroyImage(m_Device, m_Overlay.Image, NULL);

    m_Overlay.Image = VK_NULL_HANDLE;
    m_Overlay.ImageView = VK_NULL_HANDLE;
    m_Overlay.NoDepthRP = VK_NULL_HANDLE;
    m_Overlay.NoDepthFB = VK_NULL_HANDLE;
  }

  // create the overlay image if we don't have one already
  // we go through the driver's creation functions so creation info
  // is saved and the resources are registered as live resources for
  // their IDs.
  if(m_Overlay.Image == VK_NULL_HANDLE)
  {
    m_Overlay.ImageDim.width = iminfo.extent.width;
    m_Overlay.ImageDim.height = iminfo.extent.height;

    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        {m_Overlay.ImageDim.width, m_Overlay.ImageDim.height, 1},
        1,
        1,
        iminfo.samples,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, NULL, &m_Overlay.Image);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetImageMemoryRequirements(m_Device, m_Overlay.Image, &mrq);

    // if no memory is allocated, or it's not enough,
    // then allocate
    if(m_Overlay.ImageMem == VK_NULL_HANDLE || mrq.size > m_Overlay.ImageMemSize)
    {
      if(m_Overlay.ImageMem != VK_NULL_HANDLE)
      {
        m_pDriver->vkFreeMemory(m_Device, m_Overlay.ImageMem, NULL);
      }

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &m_Overlay.ImageMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_Overlay.ImageMemSize = mrq.size;
    }

    vkr = m_pDriver->vkBindImageMemory(m_Device, m_Overlay.Image, m_Overlay.ImageMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        m_Overlay.Image,
        VK_IMAGE_VIEW_TYPE_2D,
        imInfo.format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &m_Overlay.ImageView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // need to update image layout into valid state

    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_Overlay.Image),
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    m_pDriver->m_ImageLayouts[GetResID(m_Overlay.Image)].subresourceStates[0].newLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    DoPipelineBarrier(cmd, 1, &barrier);

    VkAttachmentDescription colDesc = {
        0,
        imInfo.format,
        imInfo.samples,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub = {
        0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,    NULL,       // inputs
        1,    &colRef,    // color
        NULL,             // resolve
        NULL,             // depth-stencil
        0,    NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        1,
        &colDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &m_Overlay.NoDepthRP);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // Create framebuffer rendering just to overlay image, no depth
    VkFramebufferCreateInfo fbinfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        m_Overlay.NoDepthRP,
        1,
        &m_Overlay.ImageView,
        (uint32_t)m_Overlay.ImageDim.width,
        (uint32_t)m_Overlay.ImageDim.height,
        1,
    };

    vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_Overlay.NoDepthFB);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // can't create a framebuffer or renderpass for overlay image + depth as that
    // needs to match the depth texture type wherever our draw is.
  }

  VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  const DrawcallDescription *mainDraw = m_pDriver->GetDrawcall(eventId);

  // Secondary commands can't have render passes
  if((mainDraw && !(mainDraw->flags & DrawFlags::Drawcall)) ||
     !m_pDriver->m_Partial[WrappedVulkan::Primary].renderPassActive)
  {
    // don't do anything, no drawcall capable of making overlays selected
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);
  }
  else if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);
  }
  else if(overlay == DebugOverlay::Drawcall || overlay == DebugOverlay::Wireframe)
  {
    float highlightCol[] = {0.8f, 0.1f, 0.8f, 1.0f};
    float clearCol[] = {0.0f, 0.0f, 0.0f, 0.5f};

    if(overlay == DebugOverlay::Wireframe)
    {
      highlightCol[0] = 200 / 255.0f;
      highlightCol[1] = 1.0f;
      highlightCol[2] = 0.0f;

      clearCol[0] = 200 / 255.0f;
      clearCol[1] = 1.0f;
      clearCol[2] = 0.0f;
      clearCol[3] = 0.0f;
    }

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)clearCol, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // backup state
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    // make patched shader
    VkShaderModule mod = VK_NULL_HANDLE;

    GetDebugManager()->PatchFixedColShader(mod, highlightCol);

    // make patched pipeline
    VkGraphicsPipelineCreateInfo pipeCreateInfo;

    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                          prevstate.graphics.pipeline);

    // disable all tests possible
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    ds->depthTestEnable = false;
    ds->depthWriteEnable = false;
    ds->stencilTestEnable = false;
    ds->depthBoundsTestEnable = false;

    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    rs->cullMode = VK_CULL_MODE_NONE;
    rs->rasterizerDiscardEnable = false;

    if(m_pDriver->GetDeviceFeatures().depthClamp)
    {
      rs->depthClampEnable = true;
    }

    uint32_t patchedIndexCount = 0;
    GPUBuffer patchedIB;

    if(overlay == DebugOverlay::Wireframe)
    {
      rs->lineWidth = 1.0f;

      if(mainDraw == NULL)
      {
        // do nothing
      }
      else if(m_pDriver->GetDeviceFeatures().fillModeNonSolid)
      {
        rs->polygonMode = VK_POLYGON_MODE_LINE;
      }
      else if(mainDraw->topology == Topology::TriangleList ||
              mainDraw->topology == Topology::TriangleStrip ||
              mainDraw->topology == Topology::TriangleFan ||
              mainDraw->topology == Topology::TriangleList_Adj ||
              mainDraw->topology == Topology::TriangleStrip_Adj)
      {
        // bad drivers (aka mobile) won't have non-solid fill mode, so we have to fall back to
        // manually patching the index buffer and using a line list. This doesn't work with
        // adjacency or patchlist topologies since those imply a vertex processing pipeline that
        // requires a particular topology, or can't be implicitly converted to lines at input stage.
        // It's unlikely those features will be used on said poor hw, so this should still catch
        // most cases.
        VkPipelineInputAssemblyStateCreateInfo *ia =
            (VkPipelineInputAssemblyStateCreateInfo *)pipeCreateInfo.pInputAssemblyState;

        ia->topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;

        // thankfully, primitive restart is always supported! This makes the index buffer a bit more
        // compact in the common cases where we don't need to repeat two indices for a triangle's
        // three lines, instead we have a single restart index after each triangle.
        ia->primitiveRestartEnable = true;

        GetDebugManager()->PatchLineStripIndexBuffer(mainDraw, patchedIB, patchedIndexCount);
      }
      else
      {
        RDCWARN("Unable to draw wireframe overlay for %s topology draw via software patching",
                ToStr(mainDraw->topology).c_str());
      }
    }

    VkPipelineColorBlendStateCreateInfo *cb =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    cb->logicOpEnable = false;
    cb->attachmentCount = 1;    // only one colour attachment
    for(uint32_t i = 0; i < cb->attachmentCount; i++)
    {
      VkPipelineColorBlendAttachmentState *att =
          (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
      att->blendEnable = false;
      att->colorWriteMask = 0xf;
    }

    // set scissors to max
    for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
    {
      VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
      sc.offset.x = 0;
      sc.offset.y = 0;
      sc.extent.width = 16384;
      sc.extent.height = 16384;
    }

    // set our renderpass and shader
    pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
    pipeCreateInfo.subpass = 0;

    bool found = false;
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
      if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        sh.module = mod;
        sh.pName = "main";
        found = true;
        break;
      }
    }

    if(!found)
    {
      // we know this is safe because it's pointing to a static array that's
      // big enough for all shaders

      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
      sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.pNext = NULL;
      sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      sh.module = mod;
      sh.pName = "main";
      sh.pSpecializationInfo = NULL;
    }

    VkPipeline pipe = VK_NULL_HANDLE;

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // modify state
    m_pDriver->m_RenderState.renderPass = GetResID(m_Overlay.NoDepthRP);
    m_pDriver->m_RenderState.subpass = 0;
    m_pDriver->m_RenderState.framebuffer = GetResID(m_Overlay.NoDepthFB);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe);

    // set dynamic scissors in case pipeline was using them
    for(size_t i = 0; i < m_pDriver->m_RenderState.scissors.size(); i++)
    {
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].extent.width = 16384;
      m_pDriver->m_RenderState.scissors[i].extent.height = 16384;
    }

    if(overlay == DebugOverlay::Wireframe)
      m_pDriver->m_RenderState.lineWidth = 1.0f;

    if(overlay == DebugOverlay::Drawcall || overlay == DebugOverlay::Wireframe)
      m_pDriver->m_RenderState.conditionalRendering.forceDisable = true;

    if(patchedIndexCount == 0)
    {
      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);
    }
    else
    {
      // if we patched the index buffer we need to manually play the draw with a higher index count
      // and no index offset.
      cmd = m_pDriver->GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      // do single draw
      m_pDriver->m_RenderState.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), patchedIndexCount, mainDraw->numInstances, 0, 0,
                                   mainDraw->instanceOffset);
      m_pDriver->m_RenderState.EndRenderPass(cmd);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    patchedIB.Destroy();

    m_pDriver->vkDestroyPipeline(m_Device, pipe, NULL);
    m_pDriver->vkDestroyShaderModule(m_Device, mod, NULL);
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    // clear the whole image to opaque black. We'll overwite the render area with transparent black
    // before rendering the viewport/scissors
    float black[] = {0.0f, 0.0f, 0.0f, 1.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    black[3] = 0.0f;

    {
      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          Unwrap(m_Overlay.NoDepthRP),
          Unwrap(m_Overlay.NoDepthFB),
          m_pDriver->m_RenderState.renderArea,
          1,
          &clearval,
      };
      vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      VkClearRect rect = {
          {
              {
                  m_pDriver->m_RenderState.renderArea.offset.x,
                  m_pDriver->m_RenderState.renderArea.offset.y,
              },
              {
                  m_pDriver->m_RenderState.renderArea.extent.width,
                  m_pDriver->m_RenderState.renderArea.extent.height,
              },
          },
          0,
          1,
      };
      VkClearAttachment blackclear = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
      vt->CmdClearAttachments(Unwrap(cmd), 1, &blackclear, 1, &rect);

      VkViewport viewport = m_pDriver->m_RenderState.views[0];
      vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

      uint32_t uboOffs = 0;

      CheckerboardUBOData *ubo = (CheckerboardUBOData *)m_Overlay.m_CheckerUBO.Map(&uboOffs);

      ubo->BorderWidth = 3;
      ubo->CheckerSquareDimension = 16.0f;

      // set primary/secondary to the same to 'disable' checkerboard
      ubo->PrimaryColor = ubo->SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
      ubo->InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);

      // set viewport rect
      ubo->RectPosition = Vec2f(viewport.x, viewport.y);
      ubo->RectSize = Vec2f(viewport.width, viewport.height);

      if(m_pDriver->m_ExtensionsEnabled[VkCheckExt_AMD_neg_viewport] ||
         m_pDriver->m_ExtensionsEnabled[VkCheckExt_KHR_maintenance1])
        ubo->RectSize.y = fabs(viewport.height);

      m_Overlay.m_CheckerUBO.Unmap();

      vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          Unwrap(m_Overlay.m_CheckerF16Pipeline[SampleIndex(iminfo.samples)]));
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_Overlay.m_CheckerPipeLayout), 0, 1,
                                UnwrapPtr(m_Overlay.m_CheckerDescSet), 1, &uboOffs);

      vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

      if(!m_pDriver->m_RenderState.scissors.empty())
      {
        Vec4f scissor((float)m_pDriver->m_RenderState.scissors[0].offset.x,
                      (float)m_pDriver->m_RenderState.scissors[0].offset.y,
                      (float)m_pDriver->m_RenderState.scissors[0].extent.width,
                      (float)m_pDriver->m_RenderState.scissors[0].extent.height);

        ubo = (CheckerboardUBOData *)m_Overlay.m_CheckerUBO.Map(&uboOffs);

        ubo->BorderWidth = 3;
        ubo->CheckerSquareDimension = 16.0f;

        // black/white checkered border
        ubo->PrimaryColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
        ubo->SecondaryColor = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

        // nothing at all inside
        ubo->InnerColor = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

        ubo->RectPosition = Vec2f(scissor.x, scissor.y);
        ubo->RectSize = Vec2f(scissor.z, scissor.w);

        m_Overlay.m_CheckerUBO.Unmap();

        viewport.x = scissor.x;
        viewport.y = scissor.y;
        viewport.width = scissor.z;
        viewport.height = scissor.w;

        vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
        vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(m_Overlay.m_CheckerPipeLayout), 0, 1,
                                  UnwrapPtr(m_Overlay.m_CheckerDescSet), 1, &uboOffs);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }

      vt->CmdEndRenderPass(Unwrap(cmd));
    }
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    float highlightCol[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)highlightCol, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    highlightCol[0] = 1.0f;
    highlightCol[3] = 1.0f;

    // backup state
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    // make patched shader
    VkShaderModule mod[2] = {0};
    VkPipeline pipe[2] = {0};

    // first shader, no culling, writes red
    GetDebugManager()->PatchFixedColShader(mod[0], highlightCol);

    highlightCol[0] = 0.0f;
    highlightCol[1] = 1.0f;

    // second shader, normal culling, writes green
    GetDebugManager()->PatchFixedColShader(mod[1], highlightCol);

    // make patched pipeline
    VkGraphicsPipelineCreateInfo pipeCreateInfo;

    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                          prevstate.graphics.pipeline);

    // disable all tests possible
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    ds->depthTestEnable = false;
    ds->depthWriteEnable = false;
    ds->stencilTestEnable = false;
    ds->depthBoundsTestEnable = false;

    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    VkCullModeFlags origCullMode = rs->cullMode;
    rs->cullMode = VK_CULL_MODE_NONE;    // first render without any culling
    rs->rasterizerDiscardEnable = false;

    if(m_pDriver->GetDeviceFeatures().depthClamp)
      rs->depthClampEnable = true;

    VkPipelineColorBlendStateCreateInfo *cb =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    cb->logicOpEnable = false;
    cb->attachmentCount = 1;    // only one colour attachment
    for(uint32_t i = 0; i < cb->attachmentCount; i++)
    {
      VkPipelineColorBlendAttachmentState *att =
          (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
      att->blendEnable = false;
      att->colorWriteMask = 0xf;
    }

    // set scissors to max
    for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
    {
      VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
      sc.offset.x = 0;
      sc.offset.y = 0;
      sc.extent.width = 16384;
      sc.extent.height = 16384;
    }

    // set our renderpass and shader
    pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
    pipeCreateInfo.subpass = 0;

    VkPipelineShaderStageCreateInfo *fragShader = NULL;

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
      if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        sh.module = mod[0];
        sh.pName = "main";
        fragShader = &sh;
        break;
      }
    }

    if(fragShader == NULL)
    {
      // we know this is safe because it's pointing to a static array that's
      // big enough for all shaders

      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
      sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.pNext = NULL;
      sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      sh.module = mod[0];
      sh.pName = "main";
      sh.pSpecializationInfo = NULL;

      fragShader = &sh;
    }

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    fragShader->module = mod[1];
    rs->cullMode = origCullMode;

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe[1]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // modify state
    m_pDriver->m_RenderState.renderPass = GetResID(m_Overlay.NoDepthRP);
    m_pDriver->m_RenderState.subpass = 0;
    m_pDriver->m_RenderState.framebuffer = GetResID(m_Overlay.NoDepthFB);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe[0]);

    // set dynamic scissors in case pipeline was using them
    for(size_t i = 0; i < m_pDriver->m_RenderState.scissors.size(); i++)
    {
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].extent.width = 16384;
      m_pDriver->m_RenderState.scissors[i].extent.height = 16384;
    }

    m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(pipe[1]);

    m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    for(int i = 0; i < 2; i++)
    {
      m_pDriver->vkDestroyPipeline(m_Device, pipe[i], NULL);
      m_pDriver->vkDestroyShaderModule(m_Device, mod[i], NULL);
    }
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    float highlightCol[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)highlightCol, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    VkFramebuffer depthFB = VK_NULL_HANDLE;
    VkRenderPass depthRP = VK_NULL_HANDLE;

    const VulkanRenderState &state = m_pDriver->m_RenderState;
    VulkanCreationInfo &createinfo = m_pDriver->m_CreationInfo;

    RDCASSERT(state.subpass < createinfo.m_RenderPass[state.renderPass].subpasses.size());
    int32_t dsIdx =
        createinfo.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;

    // make a renderpass and framebuffer for rendering to overlay color and using
    // depth buffer from the orignial render
    if(dsIdx >= 0 && dsIdx < (int32_t)createinfo.m_Framebuffer[state.framebuffer].attachments.size())
    {
      VkAttachmentDescription attDescs[] = {
          {0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
           VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
           VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
          {0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
           VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
           VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
      };

      ResourceId depthView = createinfo.m_Framebuffer[state.framebuffer].attachments[dsIdx].view;
      VulkanCreationInfo::ImageView &depthViewInfo = createinfo.m_ImageView[depthView];

      ResourceId depthIm = depthViewInfo.image;
      VulkanCreationInfo::Image &depthImageInfo = createinfo.m_Image[depthIm];

      attDescs[1].format = depthImageInfo.format;
      attDescs[0].samples = attDescs[1].samples = iminfo.samples;

      std::vector<ImageRegionState> &depthStates =
          m_pDriver->m_ImageLayouts[depthIm].subresourceStates;

      for(ImageRegionState &ds : depthStates)
      {
        // find the state that overlaps the view's subresource range start. We assume all
        // subresources are correctly in the same state (as they should be) so we just need to find
        // the first match.
        if(ds.subresourceRange.baseArrayLayer <= depthViewInfo.range.baseArrayLayer &&
           ds.subresourceRange.baseArrayLayer + 1 > depthViewInfo.range.baseArrayLayer &&
           ds.subresourceRange.baseMipLevel <= depthViewInfo.range.baseMipLevel &&
           ds.subresourceRange.baseMipLevel + ds.subresourceRange.levelCount + 1 >
               depthViewInfo.range.baseMipLevel)
        {
          attDescs[1].initialLayout = attDescs[1].finalLayout = ds.newLayout;
          break;
        }
      }

      VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
      VkAttachmentReference dsRef = {1, attDescs[1].initialLayout};

      VkSubpassDescription sub = {
          0,      VK_PIPELINE_BIND_POINT_GRAPHICS,
          0,      NULL,       // inputs
          1,      &colRef,    // color
          NULL,               // resolve
          &dsRef,             // depth-stencil
          0,      NULL,       // preserve
      };

      VkRenderPassCreateInfo rpinfo = {
          VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
          NULL,
          0,
          2,
          attDescs,
          1,
          &sub,
          0,
          NULL,    // dependencies
      };

      vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &depthRP);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageView views[] = {
          m_Overlay.ImageView,
          m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(depthView),
      };

      // Create framebuffer rendering just to overlay image, no depth
      VkFramebufferCreateInfo fbinfo = {
          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          NULL,
          0,
          depthRP,
          2,
          views,
          (uint32_t)m_Overlay.ImageDim.width,
          (uint32_t)m_Overlay.ImageDim.height,
          1,
      };

      vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &depthFB);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    // if depthRP is NULL, so is depthFB, and it means no depth buffer was
    // bound, so we just render green.

    highlightCol[0] = 1.0f;
    highlightCol[3] = 1.0f;

    // backup state
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    // make patched shader
    VkShaderModule failmod = {}, passmod = {};
    VkPipeline failpipe = {}, passpipe = {};

    // first shader, no depth/stencil testing, writes red
    GetDebugManager()->PatchFixedColShader(failmod, highlightCol);

    highlightCol[0] = 0.0f;
    highlightCol[1] = 1.0f;

    // second shader, enabled depth/stencil testing, writes green
    GetDebugManager()->PatchFixedColShader(passmod, highlightCol);

    // make patched pipeline
    VkGraphicsPipelineCreateInfo pipeCreateInfo;

    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                          prevstate.graphics.pipeline);

    // disable all tests possible
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    VkBool32 origDepthTest = ds->depthTestEnable;
    ds->depthTestEnable = false;
    ds->depthWriteEnable = false;
    VkBool32 origStencilTest = ds->stencilTestEnable;
    ds->stencilTestEnable = false;
    ds->depthBoundsTestEnable = false;

    VkPipelineColorBlendStateCreateInfo *cb =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    cb->logicOpEnable = false;
    cb->attachmentCount = 1;    // only one colour attachment
    for(uint32_t i = 0; i < cb->attachmentCount; i++)
    {
      VkPipelineColorBlendAttachmentState *att =
          (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
      att->blendEnable = false;
      att->colorWriteMask = 0xf;
    }

    // set scissors to max
    for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
    {
      VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
      sc.offset.x = 0;
      sc.offset.y = 0;
      sc.extent.width = 16384;
      sc.extent.height = 16384;
    }

    // subpass 0 in either render pass
    pipeCreateInfo.subpass = 0;

    VkPipelineShaderStageCreateInfo *fragShader = NULL;

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
      if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        sh.pName = "main";
        fragShader = &sh;
        break;
      }
    }

    if(fragShader == NULL)
    {
      // we know this is safe because it's pointing to a static array that's
      // big enough for all shaders

      VkPipelineShaderStageCreateInfo &sh =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
      sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.pNext = NULL;
      sh.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      sh.pName = "main";
      sh.pSpecializationInfo = NULL;

      fragShader = &sh;
    }

    fragShader->module = passmod;

    if(depthRP != VK_NULL_HANDLE)
    {
      if(overlay == DebugOverlay::Depth)
        ds->depthTestEnable = origDepthTest;
      else
        ds->stencilTestEnable = origStencilTest;
      pipeCreateInfo.renderPass = depthRP;
    }
    else
    {
      pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &passpipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    fragShader->module = failmod;

    // set our renderpass and shader
    pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;

    // disable culling/discard and enable depth clamp. That way we show any failures due to these
    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    rs->cullMode = VK_CULL_MODE_NONE;
    rs->rasterizerDiscardEnable = false;

    if(m_pDriver->GetDeviceFeatures().depthClamp)
      rs->depthClampEnable = true;

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &failpipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // modify state
    m_pDriver->m_RenderState.renderPass = GetResID(m_Overlay.NoDepthRP);
    m_pDriver->m_RenderState.subpass = 0;
    m_pDriver->m_RenderState.framebuffer = GetResID(m_Overlay.NoDepthFB);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(failpipe);

    // set dynamic scissors in case pipeline was using them
    for(size_t i = 0; i < m_pDriver->m_RenderState.scissors.size(); i++)
    {
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].offset.x = 0;
      m_pDriver->m_RenderState.scissors[i].extent.width = 16384;
      m_pDriver->m_RenderState.scissors[i].extent.height = 16384;
    }

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

    m_pDriver->m_RenderState.graphics.pipeline = GetResID(passpipe);
    if(depthRP != VK_NULL_HANDLE)
    {
      m_pDriver->m_RenderState.renderPass = GetResID(depthRP);
      m_pDriver->m_RenderState.framebuffer = GetResID(depthFB);
    }

    m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    m_pDriver->vkDestroyPipeline(m_Device, failpipe, NULL);
    m_pDriver->vkDestroyShaderModule(m_Device, failmod, NULL);
    m_pDriver->vkDestroyPipeline(m_Device, passpipe, NULL);
    m_pDriver->vkDestroyShaderModule(m_Device, passmod, NULL);

    if(depthRP != VK_NULL_HANDLE)
    {
      m_pDriver->vkDestroyRenderPass(m_Device, depthRP, NULL);
      m_pDriver->vkDestroyFramebuffer(m_Device, depthFB, NULL);
    }
  }
  else if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
  {
    // clear the overlay image itself
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    NULL,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    VK_QUEUE_FAMILY_IGNORED,
                                    Unwrap(m_Overlay.Image),
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subresourceRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    std::vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif

      size_t startEvent = 0;

      // if we're ClearBeforePass the first event will be a vkBeginRenderPass.
      // if there are any other events, we need to play up to right before them
      // so that we have all the render state set up to do
      // BeginRenderPassAndApplyState and a clear. If it's just the begin, we
      // just play including it, do the clear, then we won't replay anything
      // in the loop below
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[0]);
        if(draw && draw->flags & DrawFlags::BeginPass)
        {
          if(events.size() == 1)
          {
            m_pDriver->ReplayLog(0, events[0], eReplay_Full);
          }
          else
          {
            startEvent = 1;
            m_pDriver->ReplayLog(0, events[1], eReplay_WithoutDraw);
          }
        }
      }
      else
      {
        m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->m_RenderState.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

      VkClearAttachment blackclear = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
      std::vector<VkClearAttachment> atts;

      VulkanCreationInfo::Framebuffer &fb =
          m_pDriver->m_CreationInfo.m_Framebuffer[m_pDriver->m_RenderState.framebuffer];
      VulkanCreationInfo::RenderPass &rp =
          m_pDriver->m_CreationInfo.m_RenderPass[m_pDriver->m_RenderState.renderPass];

      for(size_t i = 0; i < rp.subpasses[m_pDriver->m_RenderState.subpass].colorAttachments.size();
          i++)
      {
        blackclear.colorAttachment = (uint32_t)i;
        atts.push_back(blackclear);
      }

      VkClearRect rect = {
          {{0, 0}, {fb.width, fb.height}}, 0, 1,
      };

      vt->CmdClearAttachments(Unwrap(cmd), (uint32_t)atts.size(), &atts[0], 1, &rect);

      m_pDriver->m_RenderState.EndRenderPass(cmd);

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      for(size_t i = startEvent; i < events.size(); i++)
      {
        m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDriver->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    if(m_Overlay.m_QuadResolvePipeline[0] != VK_NULL_HANDLE)
    {
      SCOPED_TIMER("Quad Overdraw");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

      VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      NULL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      Unwrap(m_Overlay.Image),
                                      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      DoPipelineBarrier(cmd, 1, &barrier);

      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1,
                             &subresourceRange);

      std::swap(barrier.oldLayout, barrier.newLayout);
      std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
      barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

      DoPipelineBarrier(cmd, 1, &barrier);

      std::vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventId);

      // if we're rendering the whole pass, and the first draw is a BeginRenderPass, don't include
      // it in the list. We want to start by replaying into the renderpass so that we have the
      // correct state being applied.
      if(overlay == DebugOverlay::QuadOverdrawPass)
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[0]);
        if(draw->flags & DrawFlags::BeginPass)
          events.erase(events.begin());
      }

      VkImage quadImg;
      VkDeviceMemory quadImgMem;
      VkImageView quadImgView;

      VkImageCreateInfo imInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          NULL,
          0,
          VK_IMAGE_TYPE_2D,
          VK_FORMAT_R32_UINT,
          {RDCMAX(1U, m_Overlay.ImageDim.width >> 1), RDCMAX(1U, m_Overlay.ImageDim.height >> 1), 1},
          1,
          4,
          VK_SAMPLE_COUNT_1_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_SHARING_MODE_EXCLUSIVE,
          0,
          NULL,
          VK_IMAGE_LAYOUT_UNDEFINED,
      };

      vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, NULL, &quadImg);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkMemoryRequirements mrq = {0};

      m_pDriver->vkGetImageMemoryRequirements(m_Device, quadImg, &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &quadImgMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      vkr = m_pDriver->vkBindImageMemory(m_Device, quadImg, quadImgMem, 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageViewCreateInfo viewinfo = {
          VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          NULL,
          0,
          quadImg,
          VK_IMAGE_VIEW_TYPE_2D_ARRAY,
          VK_FORMAT_R32_UINT,
          {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
           VK_COMPONENT_SWIZZLE_ONE},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 4},
      };

      vkr = m_pDriver->vkCreateImageView(m_Device, &viewinfo, NULL, &quadImgView);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      // update descriptor to point to our R32 result image
      VkDescriptorImageInfo imdesc = {0};
      imdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      imdesc.sampler = VK_NULL_HANDLE;
      imdesc.imageView = Unwrap(quadImgView);

      VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    NULL,
                                    Unwrap(m_Overlay.m_QuadDescSet),
                                    0,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                    &imdesc,
                                    NULL,
                                    NULL};
      vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

      VkImageMemoryBarrier quadImBarrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          0,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_GENERAL,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(quadImg),
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 4},
      };

      // clear all to black
      DoPipelineBarrier(cmd, 1, &quadImBarrier);
      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(quadImg), VK_IMAGE_LAYOUT_GENERAL,
                             (VkClearColorValue *)&black, 1, &quadImBarrier.subresourceRange);

      quadImBarrier.srcAccessMask = quadImBarrier.dstAccessMask;
      quadImBarrier.oldLayout = quadImBarrier.newLayout;

      quadImBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

      // set to general layout, for load/store operations
      DoPipelineBarrier(cmd, 1, &quadImBarrier);

      VkMemoryBarrier memBarrier = {
          VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_ALL_WRITE_BITS, VK_ACCESS_ALL_READ_BITS,
      };

      DoPipelineBarrier(cmd, 1, &memBarrier);

      // end this cmd buffer so the image is in the right state for the next part
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif

      m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);

      // declare callback struct here
      VulkanQuadOverdrawCallback cb(m_pDriver, m_Overlay.m_QuadDescSetLayout,
                                    m_Overlay.m_QuadDescSet, events);

      m_pDriver->ReplayLog(events.front(), events.back(), eReplay_Full);

      // resolve pass
      {
        cmd = m_pDriver->GetNextCmd();

        vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        quadImBarrier.srcAccessMask = quadImBarrier.dstAccessMask;
        quadImBarrier.oldLayout = quadImBarrier.newLayout;

        quadImBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // wait for writing to finish
        DoPipelineBarrier(cmd, 1, &quadImBarrier);

        VkClearValue clearval = {};
        VkRenderPassBeginInfo rpbegin = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            NULL,
            Unwrap(m_Overlay.NoDepthRP),
            Unwrap(m_Overlay.NoDepthFB),
            m_pDriver->m_RenderState.renderArea,
            1,
            &clearval,
        };
        vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

        vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Unwrap(m_Overlay.m_QuadResolvePipeline[SampleIndex(iminfo.samples)]));
        vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(m_Overlay.m_QuadResolvePipeLayout), 0, 1,
                                  UnwrapPtr(m_Overlay.m_QuadDescSet), 0, NULL);

        VkViewport viewport = {
            0.0f, 0.0f, (float)m_Overlay.ImageDim.width, (float)m_Overlay.ImageDim.height,
            0.0f, 1.0f};
        vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
        vt->CmdEndRenderPass(Unwrap(cmd));

        vkr = vt->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->vkDestroyImageView(m_Device, quadImgView, NULL);
      m_pDriver->vkDestroyImage(m_Device, quadImg, NULL);
      m_pDriver->vkFreeMemory(m_Device, quadImgMem, NULL);

      for(auto it = cb.m_PipelineCache.begin(); it != cb.m_PipelineCache.end(); ++it)
      {
        m_pDriver->vkDestroyPipeline(m_Device, it->second.second, NULL);
      }
    }

    // restore back to normal
    m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else if(overlay == DebugOverlay::TriangleSizePass || overlay == DebugOverlay::TriangleSizeDraw)
  {
    VulkanRenderState prevstate = m_pDriver->m_RenderState;

    {
      SCOPED_TIMER("Triangle Size");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};

      VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      NULL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      Unwrap(m_Overlay.Image),
                                      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      DoPipelineBarrier(cmd, 1, &barrier);

      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1,
                             &subresourceRange);

      std::swap(barrier.oldLayout, barrier.newLayout);
      std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
      barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

      DoPipelineBarrier(cmd, 1, &barrier);

      // end this cmd buffer so the image is in the right state for the next part
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif

      std::vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::TriangleSizeDraw)
        events.clear();

      while(!events.empty())
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[0]);

        // remove any non-drawcalls, like the pass boundary.
        if(!draw || !(draw->flags & DrawFlags::Drawcall))
          events.erase(events.begin());
        else
          break;
      }

      events.push_back(eventId);

      m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);

      VulkanRenderState &state = m_pDriver->GetRenderState();

      uint32_t meshOffs = 0;
      MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&meshOffs);

      data->mvp = Matrix4f::Identity();
      data->invProj = Matrix4f::Identity();
      data->color = Vec4f();
      data->homogenousInput = 1;
      data->pointSpriteSize = Vec2f(0.0f, 0.0f);
      data->displayFormat = 0;
      data->rawoutput = 1;
      data->padding = Vec3f();
      m_MeshRender.UBO.Unmap();

      uint32_t viewOffs = 0;
      Vec4f *ubo = (Vec4f *)m_Overlay.m_TriSizeUBO.Map(&viewOffs);
      *ubo = Vec4f(state.views[0].width, state.views[0].height);
      m_Overlay.m_TriSizeUBO.Unmap();

      uint32_t offsets[2] = {meshOffs, viewOffs};

      VkDescriptorBufferInfo bufdesc;
      m_MeshRender.UBO.FillDescriptor(bufdesc);

      VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    NULL,
                                    Unwrap(m_Overlay.m_TriSizeDescSet),
                                    0,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    NULL,
                                    &bufdesc,
                                    NULL};
      vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

      m_Overlay.m_TriSizeUBO.FillDescriptor(bufdesc);
      write.dstBinding = 2;
      vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

      VkRenderPass RP = m_Overlay.NoDepthRP;
      VkFramebuffer FB = m_Overlay.NoDepthFB;

      VulkanCreationInfo &createinfo = m_pDriver->m_CreationInfo;

      RDCASSERT(state.subpass < createinfo.m_RenderPass[state.renderPass].subpasses.size());
      int32_t dsIdx =
          createinfo.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;

      bool depthUsed = false;

      // make a renderpass and framebuffer for rendering to overlay color and using
      // depth buffer from the orignial render
      if(dsIdx >= 0 && dsIdx < (int32_t)createinfo.m_Framebuffer[state.framebuffer].attachments.size())
      {
        depthUsed = true;

        VkAttachmentDescription attDescs[] = {
            {0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
             VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
             VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
             VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
             VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
        };

        ResourceId depthView = createinfo.m_Framebuffer[state.framebuffer].attachments[dsIdx].view;
        VulkanCreationInfo::ImageView &depthViewInfo = createinfo.m_ImageView[depthView];

        ResourceId depthIm = depthViewInfo.image;
        VulkanCreationInfo::Image &depthImageInfo = createinfo.m_Image[depthIm];

        attDescs[1].format = depthImageInfo.format;
        attDescs[0].samples = attDescs[1].samples = iminfo.samples;

        std::vector<ImageRegionState> &depthStates =
            m_pDriver->m_ImageLayouts[depthIm].subresourceStates;

        for(ImageRegionState &ds : depthStates)
        {
          // find the state that overlaps the view's subresource range start. We assume all
          // subresources are correctly in the same state (as they should be) so we just need to
          // find the first match.
          if(ds.subresourceRange.baseArrayLayer <= depthViewInfo.range.baseArrayLayer &&
             ds.subresourceRange.baseArrayLayer + 1 > depthViewInfo.range.baseArrayLayer &&
             ds.subresourceRange.baseMipLevel <= depthViewInfo.range.baseMipLevel &&
             ds.subresourceRange.baseMipLevel + ds.subresourceRange.levelCount + 1 >
                 depthViewInfo.range.baseMipLevel)
          {
            attDescs[1].initialLayout = attDescs[1].finalLayout = ds.newLayout;
            break;
          }
        }

        VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference dsRef = {1, attDescs[1].initialLayout};

        VkSubpassDescription sub = {
            0,      VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,      NULL,       // inputs
            1,      &colRef,    // color
            NULL,               // resolve
            &dsRef,             // depth-stencil
            0,      NULL,       // preserve
        };

        VkRenderPassCreateInfo rpinfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            NULL,
            0,
            2,
            attDescs,
            1,
            &sub,
            0,
            NULL,    // dependencies
        };

        vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &RP);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageView views[] = {
            m_Overlay.ImageView,
            m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(depthView),
        };

        // Create framebuffer rendering just to overlay image, no depth
        VkFramebufferCreateInfo fbinfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            NULL,
            0,
            RP,
            2,
            views,
            (uint32_t)m_Overlay.ImageDim.width,
            (uint32_t)m_Overlay.ImageDim.height,
            1,
        };

        vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &FB);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      VkGraphicsPipelineCreateInfo pipeCreateInfo;

      m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

      VkPipelineShaderStageCreateInfo stages[3] = {
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
           shaderCache->GetBuiltinModule(BuiltinShader::MeshVS), "main", NULL},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
           shaderCache->GetBuiltinModule(BuiltinShader::TrisizeFS), "main", NULL},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_GEOMETRY_BIT,
           shaderCache->GetBuiltinModule(BuiltinShader::TrisizeGS), "main", NULL},
      };

      VkPipelineInputAssemblyStateCreateInfo ia = {
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
      ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

      VkVertexInputBindingDescription binds[] = {
          // primary
          {0, 0, VK_VERTEX_INPUT_RATE_VERTEX},
          // secondary
          {1, 0, VK_VERTEX_INPUT_RATE_VERTEX},
      };

      VkVertexInputAttributeDescription vertAttrs[] = {
          {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
      };

      VkPipelineVertexInputStateCreateInfo vi = {
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          NULL,
          0,
          1,
          binds,
          2,
          vertAttrs,
      };

      VkPipelineColorBlendAttachmentState attState = {
          false,
          VK_BLEND_FACTOR_ONE,
          VK_BLEND_FACTOR_ZERO,
          VK_BLEND_OP_ADD,
          VK_BLEND_FACTOR_ONE,
          VK_BLEND_FACTOR_ZERO,
          VK_BLEND_OP_ADD,
          0xf,
      };

      VkPipelineColorBlendStateCreateInfo cb = {
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          NULL,
          0,
          false,
          VK_LOGIC_OP_NO_OP,
          1,
          &attState,
          {1.0f, 1.0f, 1.0f, 1.0f},
      };

      pipeCreateInfo.stageCount = 3;
      pipeCreateInfo.pStages = stages;
      pipeCreateInfo.pTessellationState = NULL;
      pipeCreateInfo.renderPass = RP;
      pipeCreateInfo.subpass = 0;
      pipeCreateInfo.layout = m_Overlay.m_TriSizePipeLayout;
      pipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
      pipeCreateInfo.basePipelineIndex = 0;
      pipeCreateInfo.pInputAssemblyState = &ia;
      pipeCreateInfo.pVertexInputState = &vi;
      pipeCreateInfo.pColorBlendState = &cb;

      typedef rdcpair<uint32_t, Topology> PipeKey;

      std::map<PipeKey, VkPipeline> pipes;

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          Unwrap(RP),
          Unwrap(FB),
          {{0, 0}, m_Overlay.ImageDim},
          1,
          &clearval,
      };
      vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {
          0.0f, 0.0f, (float)m_Overlay.ImageDim.width, (float)m_Overlay.ImageDim.height,
          0.0f, 1.0f};
      vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

      for(size_t i = 0; i < events.size(); i++)
      {
        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::GSOut);
          if(fmt.vertexResourceId == ResourceId())
            fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::VSOut);

          if(fmt.vertexResourceId != ResourceId())
          {
            ia.topology = MakeVkPrimitiveTopology(fmt.topology);

            binds[0].stride = binds[1].stride = fmt.vertexByteStride;

            PipeKey key = make_rdcpair(fmt.vertexByteStride, fmt.topology);
            VkPipeline pipe = pipes[key];

            if(pipe == VK_NULL_HANDLE)
            {
              vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
                                                         &pipeCreateInfo, NULL, &pipe);
              RDCASSERTEQUAL(vkr, VK_SUCCESS);
            }

            VkBuffer vb =
                m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.vertexResourceId);

            VkDeviceSize offs = fmt.vertexByteOffset;
            vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);

            pipes[key] = pipe;

            vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Unwrap(m_Overlay.m_TriSizePipeLayout), 0, 1,
                                      UnwrapPtr(m_Overlay.m_TriSizeDescSet), 2, offsets);

            vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

            const VkPipelineDynamicStateCreateInfo *dyn = pipeCreateInfo.pDynamicState;

            for(uint32_t dynState = 0; dyn && dynState < dyn->dynamicStateCount; dynState++)
            {
              VkDynamicState d = dyn->pDynamicStates[dynState];

              if(!state.views.empty() && d == VK_DYNAMIC_STATE_VIEWPORT)
              {
                vt->CmdSetViewport(Unwrap(cmd), 0, (uint32_t)state.views.size(), &state.views[0]);
              }
              else if(!state.scissors.empty() && d == VK_DYNAMIC_STATE_SCISSOR)
              {
                vt->CmdSetScissor(Unwrap(cmd), 0, (uint32_t)state.scissors.size(),
                                  &state.scissors[0]);
              }
              else if(d == VK_DYNAMIC_STATE_LINE_WIDTH)
              {
                vt->CmdSetLineWidth(Unwrap(cmd), state.lineWidth);
              }
              else if(d == VK_DYNAMIC_STATE_DEPTH_BIAS)
              {
                vt->CmdSetDepthBias(Unwrap(cmd), state.bias.depth, state.bias.biasclamp,
                                    state.bias.slope);
              }
              else if(d == VK_DYNAMIC_STATE_BLEND_CONSTANTS)
              {
                vt->CmdSetBlendConstants(Unwrap(cmd), state.blendConst);
              }
              else if(d == VK_DYNAMIC_STATE_DEPTH_BOUNDS)
              {
                vt->CmdSetDepthBounds(Unwrap(cmd), state.mindepth, state.maxdepth);
              }
              else if(d == VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK)
              {
                vt->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT,
                                             state.back.compare);
                vt->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT,
                                             state.front.compare);
              }
              else if(d == VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)
              {
                vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.write);
                vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.write);
              }
              else if(d == VK_DYNAMIC_STATE_STENCIL_REFERENCE)
              {
                vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.ref);
                vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.ref);
              }
            }

            if(fmt.indexByteStride)
            {
              VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
              if(fmt.indexByteStride == 4)
                idxtype = VK_INDEX_TYPE_UINT32;

              if(fmt.indexResourceId != ResourceId())
              {
                VkBuffer ib =
                    m_pDriver->GetResourceManager()->GetLiveHandle<VkBuffer>(fmt.indexResourceId);

                vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), fmt.indexByteOffset, idxtype);
                vt->CmdDrawIndexed(Unwrap(cmd), fmt.numIndices, 1, 0, fmt.baseVertex, 0);
              }
            }
            else
            {
              vt->CmdDraw(Unwrap(cmd), fmt.numIndices, 1, 0, 0);
            }
          }
        }
      }

      vt->CmdEndRenderPass(Unwrap(cmd));

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      if(depthUsed)
      {
        m_pDriver->vkDestroyFramebuffer(m_Device, FB, NULL);
        m_pDriver->vkDestroyRenderPass(m_Device, RP, NULL);
      }

      for(auto it = pipes.begin(); it != pipes.end(); ++it)
        m_pDriver->vkDestroyPipeline(m_Device, it->second, NULL);
    }

    // restore back to normal
    m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

    // restore state
    m_pDriver->m_RenderState = prevstate;

    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkMarkerRegion::End(cmd);

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  return GetResID(m_Overlay.Image);
}
