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

#include <float.h>
#include <math.h>
#include <algorithm>
#include "core/settings.h"
#include "data/glsl_shaders.h"
#include "driver/shaders/spirv/spirv_common.h"
#include "driver/shaders/spirv/spirv_gen.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

struct VulkanQuadOverdrawCallback : public VulkanActionCallback
{
  VulkanQuadOverdrawCallback(WrappedVulkan *vk, VkDescriptorSetLayout descSetLayout,
                             VkDescriptorSet descSet, const rdcarray<uint32_t> &events)
      : m_pDriver(vk), m_DescSetLayout(descSetLayout), m_DescSet(descSet), m_Events(events)
  {
    m_pDriver->SetActionCB(this);
  }
  ~VulkanQuadOverdrawCallback()
  {
    m_pDriver->SetActionCB(NULL);

    VkDevice dev = m_pDriver->GetDev();

    for(auto it = m_PipelineCache.begin(); it != m_PipelineCache.end(); ++it)
    {
      m_pDriver->vkDestroyPipeline(dev, it->second.pipe, NULL);
      m_pDriver->vkDestroyPipelineLayout(dev, it->second.pipeLayout, NULL);
    }

    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
    {
      if(it->second.shad != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(dev, it->second.shad, NULL);
      m_pDriver->vkDestroyPipelineLayout(dev, it->second.pipeLayout, NULL);
    }
  }
  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;

    // we customise the pipeline to disable framebuffer writes, but perform normal testing
    // and substitute our quad calculation fragment shader that writes to a storage image
    // that is bound in a new descriptor set.

    VkResult vkr = VK_SUCCESS;

    m_PrevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();

    // check cache first
    CachedPipeline pipe = m_PipelineCache[pipestate.graphics.pipeline];
    CachedShader shad = m_ShaderCache[pipestate.shaderObjects[4]];

    // if we don't get a hit, create a modified pipeline
    if(pipestate.graphics.shaderObject ? shad.shad == VK_NULL_HANDLE : pipe.pipe == VK_NULL_HANDLE)
    {
      const VulkanCreationInfo::Pipeline &p =
          m_pDriver->GetDebugManager()->GetPipelineInfo(pipestate.graphics.pipeline);

      const ResourceId layoutID =
          (pipestate.graphics.shaderObject)
              ? pipestate.graphics.descSets[pipestate.graphics.lastBoundSet].pipeLayout
              : p.vertLayout;

      const VulkanCreationInfo::PipelineLayout &layout =
          m_pDriver->GetDebugManager()->GetPipelineLayoutInfo(layoutID);

      const rdcarray<ResourceId> &origDescSetLayouts =
          (pipestate.graphics.shaderObject) ? layout.descSetLayouts : p.descSetLayouts;

      VkDescriptorSetLayout *descSetLayouts;

      // descSet will be the index of our new descriptor set
      uint32_t descSet = (uint32_t)origDescSetLayouts.size();

      descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

      for(uint32_t i = 0; i < descSet; i++)
        descSetLayouts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
            origDescSetLayouts[i]);

      // this layout has storage image and
      descSetLayouts[descSet] = m_DescSetLayout;

      // don't have to handle separate vert/frag layouts as push constant ranges must be identical
      const rdcarray<VkPushConstantRange> &push = layout.pushRanges;

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
      if(pipestate.graphics.shaderObject)
        vkr = m_pDriver->vkCreatePipelineLayout(m_pDriver->GetDev(), &pipeLayoutInfo, NULL,
                                                &shad.pipeLayout);
      else
        vkr = m_pDriver->vkCreatePipelineLayout(m_pDriver->GetDev(), &pipeLayoutInfo, NULL,
                                                &pipe.pipeLayout);
      m_pDriver->CheckVkResult(vkr);

      VkGraphicsPipelineCreateInfo pipeCreateInfo;
      m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                            pipestate.graphics.pipeline);

      if(!pipestate.graphics.shaderObject)
      {
        // repoint pipeline layout
        pipeCreateInfo.layout = pipe.pipeLayout;

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
      }

      rdcarray<uint32_t> spirv =
          *m_pDriver->GetShaderCache()->GetBuiltinBlob(BuiltinShader::QuadWriteFS);

      // patch spirv, change descriptor set to descSet value
      size_t it = 5;
      while(it < spirv.size())
      {
        uint16_t WordCount = spirv[it] >> rdcspv::WordCountShift;
        rdcspv::Op opcode = rdcspv::Op(spirv[it] & rdcspv::OpCodeMask);

        if(opcode == rdcspv::Op::Decorate &&
           spirv[it + 2] == (uint32_t)rdcspv::Decoration::DescriptorSet)
        {
          spirv[it + 3] = descSet;
          break;
        }

        it += WordCount;
      }

      if(pipestate.graphics.shaderObject)
      {
        VkDevice dev = m_pDriver->GetDev();

        VkShaderCreateInfoEXT shadCreateInfo;
        m_pDriver->GetShaderCache()->MakeShaderObjectInfo(shadCreateInfo, pipestate.shaderObjects[4]);

        shadCreateInfo.pSetLayouts = descSetLayouts;
        shadCreateInfo.setLayoutCount = descSet + 1;
        shadCreateInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
        shadCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);
        shadCreateInfo.pCode = &spirv[0];
        shadCreateInfo.pName = "main";
        shadCreateInfo.pSpecializationInfo = NULL;

        vkr = m_pDriver->vkCreateShadersEXT(dev, 1, &shadCreateInfo, NULL, &shad.shad);
        m_pDriver->CheckVkResult(vkr);

        shad.descSet = descSet;

        m_ShaderCache[pipestate.shaderObjects[4]] = shad;
      }
      else
      {
        VkShaderModuleCreateInfo modinfo = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            NULL,
            0,
            spirv.size() * sizeof(uint32_t),
            &spirv[0],
        };

        VkShaderModule module;

        VkDevice dev = m_pDriver->GetDev();

        vkr = m_pDriver->vkCreateShaderModule(dev, &modinfo, NULL, &module);
        m_pDriver->CheckVkResult(vkr);

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
                                                   &pipe.pipe);
        m_pDriver->CheckVkResult(vkr);

        m_pDriver->vkDestroyShaderModule(dev, module, NULL);

        pipe.descSet = descSet;

        m_PipelineCache[pipestate.graphics.pipeline] = pipe;
      }

      SAFE_DELETE_ARRAY(descSetLayouts);
    }

    // modify state for first draw call
    if(pipestate.graphics.shaderObject)
    {
      pipestate.shaderObjects[4] = GetResID(shad.shad);
      pipestate.graphics.lastBoundSet = shad.descSet;
      pipestate.graphics.pipeline = ResourceId();
      RDCASSERT(pipestate.graphics.descSets.size() >= shad.descSet);
      pipestate.graphics.descSets.resize(shad.descSet + 1);
      pipestate.graphics.descSets[shad.descSet].pipeLayout = GetResID(shad.pipeLayout);
      pipestate.graphics.descSets[shad.descSet].descSet = GetResID(m_DescSet);
    }
    else
    {
      pipestate.graphics.pipeline = GetResID(pipe.pipe);
      RDCASSERT(pipestate.graphics.descSets.size() >= pipe.descSet);
      pipestate.graphics.descSets.resize(pipe.descSet + 1);
      pipestate.graphics.descSets[pipe.descSet].pipeLayout = GetResID(pipe.pipeLayout);
      pipestate.graphics.descSets[pipe.descSet].descSet = GetResID(m_DescSet);
    }

    // modify dynamic state
    {
      // disable colour writes/blends
      for(uint32_t i = 0; i < pipestate.colorBlendEnable.size(); i++)
        pipestate.colorBlendEnable[i] = false;

      for(uint32_t i = 0; i < pipestate.colorWriteMask.size(); i++)
        pipestate.colorWriteMask[i] = 0x0;

      // disable depth/stencil writes but keep any tests enabled
      pipestate.depthWriteEnable = false;
      pipestate.front.passOp = pipestate.front.failOp = pipestate.front.depthFailOp =
          VK_STENCIL_OP_KEEP;
      pipestate.back.passOp = pipestate.back.failOp = pipestate.back.depthFailOp = VK_STENCIL_OP_KEEP;

      // don't discard
      pipestate.rastDiscardEnable = false;
    }

    if(cmd)
    {
      if(pipestate.graphics.shaderObject)
        pipestate.BindShaderObjects(m_pDriver, cmd, VulkanRenderState::BindGraphics);
      else
        pipestate.BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);
    }
  }

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return false;

    // restore the render state and go ahead with the real draw
    m_pDriver->GetCmdRenderState() = m_PrevState;

    RDCASSERT(cmd);
    if(m_PrevState.graphics.shaderObject)
      m_pDriver->GetCmdRenderState().BindShaderObjects(m_pDriver, cmd,
                                                       VulkanRenderState::BindGraphics);
    else
      m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                  false);

    return true;
  }

  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // don't care
  }
  bool SplitSecondary() { return false; }
  bool ForceLoadRPs() { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
  }

  WrappedVulkan *m_pDriver;
  VkDescriptorSetLayout m_DescSetLayout;
  VkDescriptorSet m_DescSet;
  const rdcarray<uint32_t> &m_Events;

  // cache modified pipelines
  struct CachedPipeline
  {
    uint32_t descSet;
    VkPipelineLayout pipeLayout;
    VkPipeline pipe;
  };
  std::map<ResourceId, CachedPipeline> m_PipelineCache;
  // cache modified shader objects
  struct CachedShader
  {
    uint32_t descSet;
    VkPipelineLayout pipeLayout;
    VkShaderEXT shad;
  };
  std::map<ResourceId, CachedShader> m_ShaderCache;
  VulkanRenderState m_PrevState;
};

void VulkanDebugManager::PatchFixedColShader(VkShaderModule &mod, float col[4])
{
  union
  {
    uint32_t *spirv;
    float *data;
  } alias;

  rdcarray<uint32_t> spv = *m_pDriver->GetShaderCache()->GetBuiltinBlob(BuiltinShader::FixedColFS);

  alias.spirv = &spv[0];
  size_t spirvLength = spv.size();

  int patched = 0;

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = alias.spirv[it] >> rdcspv::WordCountShift;
    rdcspv::Op opcode = rdcspv::Op(alias.spirv[it] & rdcspv::OpCodeMask);

    if(opcode == rdcspv::Op::Constant)
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
  CheckVkResult(vkr);
}

void VulkanDebugManager::PatchFixedColShaderObject(VkShaderEXT &shad, float col[4])
{
  union
  {
    uint32_t *spirv;
    float *data;
  } alias;

  rdcarray<uint32_t> spv = *m_pDriver->GetShaderCache()->GetBuiltinBlob(BuiltinShader::FixedColFS);

  alias.spirv = &spv[0];
  size_t spirvLength = spv.size();

  int patched = 0;

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = alias.spirv[it] >> rdcspv::WordCountShift;
    rdcspv::Op opcode = rdcspv::Op(alias.spirv[it] & rdcspv::OpCodeMask);

    if(opcode == rdcspv::Op::Constant)
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

  VkShaderCreateInfoEXT shadInfo = {VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                                    NULL,
                                    0,
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    0,
                                    VK_SHADER_CODE_TYPE_SPIRV_EXT,
                                    spv.size() * sizeof(uint32_t),
                                    alias.spirv,
                                    "main",
                                    0,
                                    NULL,
                                    0,
                                    NULL,
                                    NULL};

  VkResult vkr = m_pDriver->vkCreateShadersEXT(m_Device, 1, &shadInfo, NULL, &shad);

  CheckVkResult(vkr);
}

void VulkanDebugManager::PatchLineStripIndexBuffer(const ActionDescription *action,
                                                   GPUBuffer &indexBuffer, uint32_t &indexCount)
{
  VulkanRenderState &rs = m_pDriver->m_RenderState;

  bytebuf indices;

  uint8_t *idx8 = NULL;
  uint16_t *idx16 = NULL;
  uint32_t *idx32 = NULL;

  if(action->flags & ActionFlags::Indexed)
  {
    GetBufferData(rs.ibuffer.buf,
                  rs.ibuffer.offs + uint64_t(action->indexOffset) * rs.ibuffer.bytewidth,
                  uint64_t(action->numIndices) * rs.ibuffer.bytewidth, indices);

    if(rs.ibuffer.bytewidth == 4)
      idx32 = (uint32_t *)indices.data();
    else if(rs.ibuffer.bytewidth == 1)
      idx8 = (uint8_t *)indices.data();
    else
      idx16 = (uint16_t *)indices.data();
  }

  // we just patch up to 32-bit since we'll be adding more indices and we might overflow 16-bit.
  rdcarray<uint32_t> patchedIndices;

  ::PatchLineStripIndexBuffer(action, MakePrimitiveTopology(rs.primitiveTopology, 3), idx8, idx16,
                              idx32, patchedIndices);

  indexBuffer.Create(m_pDriver, m_Device, patchedIndices.size() * sizeof(uint32_t), 1,
                     GPUBuffer::eGPUBufferIBuffer);

  void *ptr = indexBuffer.Map(0, patchedIndices.size() * sizeof(uint32_t));
  if(!ptr)
    return;
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

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = ObjDisp(m_Device)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  // ensure host writes finish before using as index buffer
  DoPipelineBarrier(cmd, 1, &uploadbarrier);

  ObjDisp(m_Device)->EndCommandBuffer(Unwrap(cmd));

  indexCount = (uint32_t)patchedIndices.size();
}

RenderOutputSubresource VulkanReplay::GetRenderOutputSubresource(ResourceId id)
{
  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  for(ResourceId viewid : state.GetFramebufferAttachments())
  {
    const VulkanCreationInfo::ImageView &viewInfo = c.m_ImageView[viewid];

    if(viewid == id || viewInfo.image == id)
    {
      return RenderOutputSubresource(viewInfo.range.baseMipLevel,
                                     c.m_ImageView[viewid].range.baseArrayLayer,
                                     c.m_ImageView[viewid].range.layerCount);
    }
  }

  return RenderOutputSubresource(~0U, ~0U, 0);
}

ResourceId VulkanReplay::RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                       uint32_t eventId, const rdcarray<uint32_t> &passEvents)
{
  const VkDevDispatchTable *vt = ObjDisp(m_Device);

  RenderOutputSubresource sub = GetRenderOutputSubresource(texid);

  if(sub.slice == ~0U)
  {
    RDCERR("Rendering overlay for %s couldn't find output to get subresource.", ToStr(texid).c_str());
    sub = RenderOutputSubresource(0, 0, 1);
  }

  VulkanShaderCache *shaderCache = m_pDriver->GetShaderCache();

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  if(cmd == VK_NULL_HANDLE)
    return ResourceId();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  VkMarkerRegion::Begin(StringFormat::Fmt("RenderOverlay %d", overlay), cmd);

  uint32_t multiviewMask = m_Overlay.MultiViewMask;

  VulkanRenderState &state = m_pDriver->m_RenderState;
  if(state.dynamicRendering.active)
  {
    multiviewMask = state.dynamicRendering.viewMask;
  }
  else if(state.GetRenderPass() != ResourceId())
  {
    const VulkanCreationInfo::RenderPass &rp =
        m_pDriver->m_CreationInfo.m_RenderPass[state.GetRenderPass()];

    multiviewMask = 0;
    for(uint32_t v : rp.subpasses[state.subpass].multiviews)
      multiviewMask |= 1U << v;
  }

  // if the overlay image is the wrong size, free it
  if(m_Overlay.Image != VK_NULL_HANDLE &&
     (iminfo.extent.width != m_Overlay.ImageDim.width ||
      iminfo.extent.height != m_Overlay.ImageDim.height || iminfo.samples != m_Overlay.Samples ||
      iminfo.mipLevels != m_Overlay.MipLevels || iminfo.arrayLayers != m_Overlay.ArrayLayers ||
      multiviewMask != m_Overlay.MultiViewMask))
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

  VkImageSubresourceRange subRange = {VK_IMAGE_ASPECT_COLOR_BIT, sub.mip, 1, sub.slice,
                                      sub.numSlices};

  const VkFormat overlayFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

  VkRenderPassMultiviewCreateInfo multiviewRP = {VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
  multiviewRP.correlationMaskCount = 1;
  multiviewRP.pCorrelationMasks = &multiviewMask;
  multiviewRP.subpassCount = 1;
  multiviewRP.pViewMasks = &multiviewMask;

  // create the overlay image if we don't have one already
  // we go through the driver's creation functions so creation info
  // is saved and the resources are registered as live resources for
  // their IDs.
  if(m_Overlay.Image == VK_NULL_HANDLE)
  {
    m_Overlay.ImageDim.width = iminfo.extent.width;
    m_Overlay.ImageDim.height = iminfo.extent.height;
    m_Overlay.MipLevels = iminfo.mipLevels;
    m_Overlay.ArrayLayers = iminfo.arrayLayers;
    m_Overlay.Samples = iminfo.samples;
    m_Overlay.MultiViewMask = multiviewMask;

    VkImageCreateInfo imInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        overlayFormat,
        {m_Overlay.ImageDim.width, m_Overlay.ImageDim.height, 1},
        (uint32_t)iminfo.mipLevels,
        (uint32_t)iminfo.arrayLayers,
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
    CheckVkResult(vkr);

    NameVulkanObject(m_Overlay.Image, "m_Overlay.Image");

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
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          NULL,
          mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &m_Overlay.ImageMem);
      CheckVkResult(vkr);

      if(vkr != VK_SUCCESS)
        return ResourceId();

      m_Overlay.ImageMemSize = mrq.size;
    }

    vkr = m_pDriver->vkBindImageMemory(m_Device, m_Overlay.Image, m_Overlay.ImageMem, 0);
    CheckVkResult(vkr);

    // need to update image layout into valid state

    m_pDriver->FindImageState(GetResID(m_Overlay.Image))
        ->InlineTransition(
            cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());

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

    VkSubpassDescription subp = {
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
        &subp,
        0,
        NULL,    // dependencies
    };

    if(multiviewMask > 0)
      rpinfo.pNext = &multiviewRP;

    vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &m_Overlay.NoDepthRP);
    CheckVkResult(vkr);
  }

  if(m_Overlay.ViewMip != sub.mip || m_Overlay.ViewSlice != sub.slice ||
     m_Overlay.ViewNumSlices != sub.numSlices || m_Overlay.ImageView == VK_NULL_HANDLE)
  {
    m_pDriver->vkDestroyFramebuffer(m_Device, m_Overlay.NoDepthFB, NULL);
    m_pDriver->vkDestroyImageView(m_Device, m_Overlay.ImageView, NULL);

    m_Overlay.ViewMip = sub.mip;
    m_Overlay.ViewSlice = sub.slice;
    m_Overlay.ViewNumSlices = sub.numSlices;

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        NULL,
        0,
        m_Overlay.Image,
        VK_IMAGE_VIEW_TYPE_2D,
        overlayFormat,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        subRange,
    };

    vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &m_Overlay.ImageView);
    CheckVkResult(vkr);

    // Create framebuffer rendering just to overlay image, no depth
    VkFramebufferCreateInfo fbinfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        m_Overlay.NoDepthRP,
        1,
        &m_Overlay.ImageView,
        RDCMAX(1U, m_Overlay.ImageDim.width >> sub.mip),
        RDCMAX(1U, m_Overlay.ImageDim.height >> sub.mip),
        sub.numSlices,
    };

    vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &m_Overlay.NoDepthFB);
    CheckVkResult(vkr);

    // can't create a framebuffer or renderpass for overlay image + depth as that
    // needs to match the depth texture type wherever our draw is.
  }

  // bail out if the render area is outside our image.
  // This is an order-of-operations problem, if the overlay is set when the event is changed it is
  // refreshed before the UI layer can update the current texture.
  if(state.renderArea.offset.x + state.renderArea.extent.width >
         (m_Overlay.ImageDim.width >> sub.mip) ||
     state.renderArea.offset.y + state.renderArea.extent.height >
         (m_Overlay.ImageDim.height >> sub.mip))
  {
    return GetResID(m_Overlay.Image);
  }

  {
    VkImageSubresourceRange fullSubRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS,
                                            0, VK_REMAINING_ARRAY_LAYERS};

    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_Overlay.Image),
        fullSubRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    float black[4] = {};
    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &fullSubRange);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);
  }

  const ActionDescription *mainDraw = m_pDriver->GetAction(eventId);

  const VulkanCreationInfo::Pipeline &pipeInfo =
      m_pDriver->m_CreationInfo.m_Pipeline[state.graphics.pipeline];

  bool rpActive = m_pDriver->IsPartialRenderPassActive();

  if((mainDraw && !(mainDraw->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall))) ||
     !rpActive)
  {
    // don't do anything, no action capable of making overlays selected
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
                                    subRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subRange);

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
                                    subRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);
  }
  else if(overlay == DebugOverlay::Drawcall || overlay == DebugOverlay::Wireframe)
  {
    float highlightCol[] = {0.8f, 0.1f, 0.8f, 1.0f};
    float bgclearCol[] = {0.0f, 0.0f, 0.0f, 0.5f};

    if(overlay == DebugOverlay::Wireframe)
    {
      highlightCol[0] = 200 / 255.0f;
      highlightCol[1] = 1.0f;
      highlightCol[2] = 0.0f;

      bgclearCol[0] = 200 / 255.0f;
      bgclearCol[1] = 1.0f;
      bgclearCol[2] = 0.0f;
      bgclearCol[3] = 0.0f;
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
                                    subRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)bgclearCol, 1, &subRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    if(!state.rastDiscardEnable)
    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      // backup state
      VulkanRenderState prevstate = state;

      // make patched shader
      VkShaderModule mod = VK_NULL_HANDLE;

      GetDebugManager()->PatchFixedColShader(mod, highlightCol);

      // make patched pipeline
      VkGraphicsPipelineCreateInfo pipeCreateInfo;

      m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                            prevstate.graphics.pipeline);

      // make patched shader object
      VkShaderEXT shad = VK_NULL_HANDLE;

      if(state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShaderObject(shad, highlightCol);

      // disable all tests possible
      VkPipelineDepthStencilStateCreateInfo *ds =
          (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
      if(ds)
      {
        ds->depthTestEnable = false;
        ds->depthWriteEnable = false;
        ds->stencilTestEnable = false;
        ds->depthBoundsTestEnable = false;
      }

      VkPipelineRasterizationStateCreateInfo *rs =
          (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
      if(rs)
      {
        rs->cullMode = VK_CULL_MODE_NONE;
        rs->rasterizerDiscardEnable = false;
      }

      VkPipelineMultisampleStateCreateInfo *msaa =
          (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;
      if(msaa)
        msaa->pSampleMask = NULL;

      // disable tests in dynamic state too
      state.depthTestEnable = VK_FALSE;
      state.depthWriteEnable = VK_FALSE;
      state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      state.stencilTestEnable = VK_FALSE;
      state.depthBoundsTestEnable = VK_FALSE;
      state.cullMode = VK_CULL_MODE_NONE;

      // disable all discard rectangles
      RemoveNextStruct(&pipeCreateInfo,
                       VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT);

      if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
      {
        if(rs)
          rs->depthClampEnable = true;
        state.depthClampEnable = true;
      }

      // disable line stipple
      VkPipelineRasterizationLineStateCreateInfoEXT *lineRasterState =
          (VkPipelineRasterizationLineStateCreateInfoEXT *)FindNextStruct(
              rs, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT);

      if(lineRasterState)
      {
        lineRasterState->stippledLineEnable = VK_FALSE;
      }
      state.stippledLineEnable = false;

      uint32_t patchedIndexCount = 0;
      GPUBuffer patchedIB;

      if(overlay == DebugOverlay::Wireframe)
      {
        if(rs)
          rs->lineWidth = 1.0f;

        if(mainDraw == NULL)
        {
          // do nothing
        }
        else if(m_pDriver->GetDeviceEnabledFeatures().fillModeNonSolid)
        {
          if(rs)
            rs->polygonMode = VK_POLYGON_MODE_LINE;
          state.polygonMode = VK_POLYGON_MODE_LINE;

          if(m_pDriver->ShaderObject())
            state.dynamicStates[VkDynamicLineWidth] = true;
        }
        else if(prevstate.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
                prevstate.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
                prevstate.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN ||
                prevstate.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ||
                prevstate.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
        {
          // bad drivers (aka mobile) won't have non-solid fill mode, so we have to fall back to
          // manually patching the index buffer and using a line list. This doesn't work with
          // adjacency or patchlist topologies since those imply a vertex processing pipeline that
          // requires a particular topology, or can't be implicitly converted to lines at input
          // stage.
          // It's unlikely those features will be used on said poor hw, so this should still catch
          // most cases.
          VkPipelineInputAssemblyStateCreateInfo *ia =
              (VkPipelineInputAssemblyStateCreateInfo *)pipeCreateInfo.pInputAssemblyState;

          if(ia)
            ia->topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
          state.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;

          // thankfully, primitive restart is always supported! This makes the index buffer a bit
          // more
          // compact in the common cases where we don't need to repeat two indices for a triangle's
          // three lines, instead we have a single restart index after each triangle.
          if(ia)
            ia->primitiveRestartEnable = true;
          state.primRestartEnable = true;

          GetDebugManager()->PatchLineStripIndexBuffer(mainDraw, patchedIB, patchedIndexCount);

          if(m_pDriver->ShaderObject())
            state.dynamicStates[VkDynamicLineWidth] = true;
        }
        else
        {
          RDCWARN("Unable to draw wireframe overlay for %s topology draw via software patching",
                  ToStr(prevstate.primitiveTopology).c_str());
        }
      }

      VkPipelineColorBlendStateCreateInfo *cb =
          (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
      if(cb)
      {
        cb->logicOpEnable = false;
        cb->attachmentCount = 1;    // only one colour attachment
        for(uint32_t i = 0; i < cb->attachmentCount; i++)
        {
          VkPipelineColorBlendAttachmentState *att =
              (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
          att->blendEnable = false;
          att->colorWriteMask = 0xf;
        }
      }

      // set scissors to max for drawcall
      if(overlay == DebugOverlay::Drawcall && pipeCreateInfo.pViewportState)
      {
        for(size_t i = 0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
        {
          VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
          sc.offset.x = 0;
          sc.offset.y = 0;
          sc.extent.width = 16384;
          sc.extent.height = 16384;
        }
      }

      // set our renderpass and shader
      pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
      pipeCreateInfo.subpass = 0;

      // don't use dynamic rendering
      RemoveNextStruct(&pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

      if(!state.graphics.shaderObject)
      {
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
      }

      VkPipeline pipe = VK_NULL_HANDLE;

      if(!state.graphics.shaderObject)
      {
        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &pipe);
        CheckVkResult(vkr);
      }

      // modify state
      state.SetRenderPass(GetResID(m_Overlay.NoDepthRP));
      state.subpass = 0;
      state.SetFramebuffer(m_pDriver, GetResID(m_Overlay.NoDepthFB));

      state.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
      state.dynamicRendering.flags &= ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

      state.graphics.pipeline = GetResID(pipe);

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(shad);
      }

      // set dynamic scissors in case pipeline was using them
      if(overlay == DebugOverlay::Drawcall)
      {
        for(size_t i = 0; i < state.scissors.size(); i++)
        {
          state.scissors[i].offset.x = 0;
          state.scissors[i].offset.y = 0;
          state.scissors[i].extent.width = 16384;
          state.scissors[i].extent.height = 16384;
        }
      }

      if(overlay == DebugOverlay::Wireframe)
        state.lineWidth = 1.0f;

      if(overlay == DebugOverlay::Drawcall || overlay == DebugOverlay::Wireframe)
        state.conditionalRendering.forceDisable = true;

      if(patchedIndexCount == 0)
      {
        m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);
      }
      else
      {
        // if we patched the index buffer we need to manually play the draw with a higher index
        // count
        // and no index offset.
        cmd = m_pDriver->GetNextCmd();

        if(cmd == VK_NULL_HANDLE)
          return ResourceId();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        CheckVkResult(vkr);

        // do single draw
        state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);
        ActionDescription action = *mainDraw;
        action.numIndices = patchedIndexCount;
        action.baseVertex = 0;
        action.indexOffset = 0;
        m_pDriver->ReplayDraw(cmd, action);
        state.EndRenderPass(cmd);

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);
      }

      // submit & flush so that we don't have to keep pipeline around for a while
      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      // restore state
      state = prevstate;

      patchedIB.Destroy();

      if(shad != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(m_Device, shad, NULL);

      m_pDriver->vkDestroyPipeline(m_Device, pipe, NULL);
      m_pDriver->vkDestroyShaderModule(m_Device, mod, NULL);
    }
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    // clear the whole image to opaque black. We'll overwite the render area with transparent black
    // before rendering the viewport/scissors
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
                                    subRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    black[3] = 0.0f;

    if(!state.rastDiscardEnable)
    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      float highlightCol[] = {1.0f, 0.0f, 0.0f, 1.0f};

      // backup state
      VulkanRenderState prevstate = state;

      // make patched shader
      VkShaderModule mod[2] = {0};
      VkPipeline pipe[2] = {0};
      VkShaderEXT shad[2] = {0};

      // first shader, no culling, writes red
      if(!state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShader(mod[0], highlightCol);
      else
        GetDebugManager()->PatchFixedColShaderObject(shad[0], highlightCol);

      highlightCol[0] = 0.0f;
      highlightCol[1] = 1.0f;

      // second shader, normal culling, writes green
      if(!state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShader(mod[1], highlightCol);
      else
        GetDebugManager()->PatchFixedColShaderObject(shad[1], highlightCol);

      // make patched pipeline
      VkGraphicsPipelineCreateInfo pipeCreateInfo;

      m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                            prevstate.graphics.pipeline);

      if(!state.graphics.shaderObject)
      {
        // disable all tests possible
        VkPipelineDepthStencilStateCreateInfo *ds =
            (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
        ds->depthTestEnable = false;
        ds->depthWriteEnable = false;
        ds->stencilTestEnable = false;
        ds->depthBoundsTestEnable = false;

        VkPipelineRasterizationStateCreateInfo *rs =
            (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
        rs->cullMode = VK_CULL_MODE_NONE;    // first render without any culling
        rs->rasterizerDiscardEnable = false;

        VkPipelineMultisampleStateCreateInfo *msaa =
            (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;
        msaa->pSampleMask = NULL;

        if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
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

        // set our renderpass and shader
        pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
        pipeCreateInfo.subpass = 0;

        // don't use dynamic rendering
        RemoveNextStruct(&pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

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

        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &pipe[0]);
        CheckVkResult(vkr);

        fragShader->module = mod[1];

        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &pipe[1]);
        CheckVkResult(vkr);
      }

      // disable tests in dynamic state too
      state.depthTestEnable = VK_FALSE;
      state.depthWriteEnable = VK_FALSE;
      state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      state.stencilTestEnable = VK_FALSE;
      state.depthBoundsTestEnable = VK_FALSE;
      state.cullMode = VK_CULL_MODE_NONE;

      // enable dynamic depth clamp
      if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
        state.depthClampEnable = true;

      // modify state
      state.SetRenderPass(GetResID(m_Overlay.NoDepthRP));
      state.subpass = 0;
      state.SetFramebuffer(m_pDriver, GetResID(m_Overlay.NoDepthFB));

      state.graphics.pipeline = GetResID(pipe[0]);
      state.scissors = prevstate.scissors;

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(shad[0]);
      }

      for(VkRect2D &sc : state.scissors)
      {
        sc.offset.x = 0;
        sc.offset.y = 0;
        sc.extent.width = 16384;
        sc.extent.height = 16384;
      }

      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

      state.graphics.pipeline = GetResID(pipe[1]);
      state.scissors = prevstate.scissors;

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(shad[1]);
      }

      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

      // restore state
      state = prevstate;

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      {
        VkClearValue clearval = {};
        VkRenderPassBeginInfo rpbegin = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            NULL,
            Unwrap(m_Overlay.NoDepthRP),
            Unwrap(m_Overlay.NoDepthFB),
            state.renderArea,
            1,
            &clearval,
        };
        vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = state.views[0];
        vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

        uint32_t uboOffs = 0;

        CheckerboardUBOData *ubo = (CheckerboardUBOData *)m_Overlay.m_CheckerUBO.Map(&uboOffs);
        if(!ubo)
          return ResourceId();

        ubo->BorderWidth = 3;
        ubo->CheckerSquareDimension = 16.0f;

        // set primary/secondary to the same to 'disable' checkerboard
        ubo->PrimaryColor = ubo->SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
        ubo->InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.4f);

        // set viewport rect
        ubo->RectPosition = Vec2f(viewport.x, viewport.y);
        ubo->RectSize = Vec2f(viewport.width, viewport.height);

        if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_AMD_negative_viewport_height ||
           m_pDriver->GetExtensions(GetRecord(m_Device)).ext_KHR_maintenance1)
        {
          ubo->RectSize.y = fabsf(viewport.height);

          // VK_KHR_maintenance1 requires the position to be adjusted as well
          if(m_pDriver->GetExtensions(GetRecord(m_Device)).ext_KHR_maintenance1 &&
             viewport.height < 0.0f)
            ubo->RectPosition.y += viewport.height;
        }

        m_Overlay.m_CheckerUBO.Unmap();

        vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Unwrap(m_Overlay.m_CheckerF16Pipeline[SampleIndex(iminfo.samples)]));
        vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(m_Overlay.m_CheckerPipeLayout), 0, 1,
                                  UnwrapPtr(m_Overlay.m_CheckerDescSet), 1, &uboOffs);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

        if(!state.scissors.empty())
        {
          Vec4f scissor((float)state.scissors[0].offset.x, (float)state.scissors[0].offset.y,
                        (float)state.scissors[0].extent.width,
                        (float)state.scissors[0].extent.height);

          ubo = (CheckerboardUBOData *)m_Overlay.m_CheckerUBO.Map(&uboOffs);
          if(!ubo)
            return ResourceId();

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

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      // submit & flush so that we don't have to keep pipeline around for a while
      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      for(int i = 0; i < 2; i++)
      {
        if(shad[i] != VK_NULL_HANDLE)
          m_pDriver->vkDestroyShaderEXT(m_Device, shad[i], NULL);
        m_pDriver->vkDestroyPipeline(m_Device, pipe[i], NULL);
        m_pDriver->vkDestroyShaderModule(m_Device, mod[i], NULL);
      }
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
                                    subRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)highlightCol, 1, &subRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    highlightCol[0] = 1.0f;
    highlightCol[1] = 0.0f;
    highlightCol[3] = 1.0f;

    if(!state.rastDiscardEnable)
    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      // backup state
      VulkanRenderState prevstate = state;

      // make patched shader
      VkShaderModule mod[2] = {0};
      VkPipeline pipe[2] = {0};
      VkShaderEXT shad[2] = {0};

      // first shader, no culling, writes red
      if(!state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShader(mod[0], highlightCol);
      else
        GetDebugManager()->PatchFixedColShaderObject(shad[0], highlightCol);

      highlightCol[0] = 0.0f;
      highlightCol[1] = 1.0f;

      // second shader, normal culling, writes green
      if(!state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShader(mod[1], highlightCol);
      else
        GetDebugManager()->PatchFixedColShaderObject(shad[1], highlightCol);

      // save original state
      VkCullModeFlags origCullMode = prevstate.cullMode;

      // make patched pipeline
      VkGraphicsPipelineCreateInfo pipeCreateInfo;

      if(!state.graphics.shaderObject)
      {
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
        rs->cullMode = VK_CULL_MODE_NONE;    // first render without any culling
        rs->rasterizerDiscardEnable = false;

        VkPipelineMultisampleStateCreateInfo *msaa =
            (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;
        msaa->pSampleMask = NULL;

        if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
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

        // set our renderpass and shader
        pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
        pipeCreateInfo.subpass = 0;

        // don't use dynamic rendering
        RemoveNextStruct(&pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

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

        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &pipe[0]);
        CheckVkResult(vkr);

        fragShader->module = mod[1];
        rs->cullMode = origCullMode;

        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &pipe[1]);
        CheckVkResult(vkr);
      }

      // disable tests in dynamic state too
      state.depthTestEnable = VK_FALSE;
      state.depthWriteEnable = VK_FALSE;
      state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      state.stencilTestEnable = VK_FALSE;
      state.depthBoundsTestEnable = VK_FALSE;
      state.cullMode = VK_CULL_MODE_NONE;

      // enable dynamic depth clamp
      if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
        state.depthClampEnable = true;

      // modify state
      state.SetRenderPass(GetResID(m_Overlay.NoDepthRP));
      state.subpass = 0;
      state.SetFramebuffer(m_pDriver, GetResID(m_Overlay.NoDepthFB));

      state.graphics.pipeline = GetResID(pipe[0]);

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(shad[0]);
      }

      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

      state.graphics.pipeline = GetResID(pipe[1]);
      state.cullMode = origCullMode;

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(shad[1]);
      }

      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

      // submit & flush so that we don't have to keep pipeline around for a while
      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      // restore state
      state = prevstate;

      for(int i = 0; i < 2; i++)
      {
        if(shad[i] != VK_NULL_HANDLE)
          m_pDriver->vkDestroyShaderEXT(m_Device, shad[i], NULL);
        m_pDriver->vkDestroyPipeline(m_Device, pipe[i], NULL);
        m_pDriver->vkDestroyShaderModule(m_Device, mod[i], NULL);
      }
    }
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    VkImage dsTempImage = VK_NULL_HANDLE;
    VkDeviceMemory dsTempImageMem = VK_NULL_HANDLE;
    VkImage dsDepthImage = VK_NULL_HANDLE;

    float highlightCol[] = {0.0f, 0.0f, 0.0f, 0.0f};

    {
      VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      NULL,
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      VK_QUEUE_FAMILY_IGNORED,
                                      Unwrap(m_Overlay.Image),
                                      subRange};

      DoPipelineBarrier(cmd, 1, &barrier);

      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             (VkClearColorValue *)highlightCol, 1, &subRange);

      std::swap(barrier.oldLayout, barrier.newLayout);
      std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
      barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

      DoPipelineBarrier(cmd, 1, &barrier);
    }

    if(!state.rastDiscardEnable)
    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);
      cmd = VK_NULL_HANDLE;

      VkFramebuffer depthFB = VK_NULL_HANDLE;
      VkRenderPass depthRP = VK_NULL_HANDLE;

      VulkanCreationInfo &createinfo = m_pDriver->m_CreationInfo;

      ResourceId depthStencilView;
      VkFormat dsFmt = VK_FORMAT_UNDEFINED;

      if(state.dynamicRendering.active)
      {
        depthStencilView = GetResID(state.dynamicRendering.depth.imageView);
        if(depthStencilView == ResourceId())
          depthStencilView = GetResID(state.dynamicRendering.stencil.imageView);
      }
      else
      {
        RDCASSERT(state.subpass < createinfo.m_RenderPass[state.GetRenderPass()].subpasses.size());
        int32_t dsIdx =
            createinfo.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].depthstencilAttachment;

        // make a renderpass and framebuffer for rendering to overlay color and using
        // depth buffer from the orignial render
        if(dsIdx >= 0 &&
           dsIdx < (int32_t)createinfo.m_Framebuffer[state.GetFramebuffer()].attachments.size())
        {
          depthStencilView = state.GetFramebufferAttachments()[dsIdx];
        }
      }

      bool useDepthWriteStencilPass = false;
      bool needDepthCopyToDepthStencil = false;

      size_t fmtIndex = ARRAY_COUNT(m_Overlay.m_DepthCopyPipeline);
      size_t sampleIndex = SampleIndex(iminfo.samples);

      if(depthStencilView != ResourceId())
      {
        if(overlay == DebugOverlay::Depth)
          useDepthWriteStencilPass = true;

        if(useDepthWriteStencilPass)
        {
          useDepthWriteStencilPass = false;
          const VulkanCreationInfo::ShaderEntry &ps = pipeInfo.shaders[4];
          if(ps.module != ResourceId())
          {
            ShaderReflection *reflection = ps.refl;
            if(reflection)
            {
              for(SigParameter &output : reflection->outputSignature)
              {
                if(output.systemValue == ShaderBuiltin::DepthOutput)
                  useDepthWriteStencilPass = true;
              }
            }
          }
        }

        VkAttachmentDescription attDescs[] = {
            {0, overlayFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
             VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
             VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            {0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
             VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
             VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
        };

        VulkanCreationInfo::ImageView &depthViewInfo = createinfo.m_ImageView[depthStencilView];

        ResourceId depthIm = depthViewInfo.image;
        VulkanCreationInfo::Image &depthImageInfo = createinfo.m_Image[depthIm];
        dsDepthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(depthIm);

        dsFmt = depthImageInfo.format;
        VkFormat dsNewFmt = dsFmt;
        if(useDepthWriteStencilPass)
        {
          if(dsFmt == VK_FORMAT_D32_SFLOAT_S8_UINT)
            dsNewFmt = VK_FORMAT_D32_SFLOAT_S8_UINT;
          else if(dsFmt == VK_FORMAT_D24_UNORM_S8_UINT)
            dsNewFmt = VK_FORMAT_D24_UNORM_S8_UINT;
          else if(dsFmt == VK_FORMAT_D32_SFLOAT)
            dsNewFmt = VK_FORMAT_D32_SFLOAT_S8_UINT;
          else if(dsFmt == VK_FORMAT_D16_UNORM)
            dsNewFmt = m_Overlay.m_DefaultDepthStencilFormat;
          else
            dsNewFmt = m_Overlay.m_DefaultDepthStencilFormat;

          RDCASSERT((dsNewFmt == VK_FORMAT_D24_UNORM_S8_UINT) ||
                    (dsNewFmt == VK_FORMAT_D32_SFLOAT_S8_UINT));
          fmtIndex = (dsNewFmt == VK_FORMAT_D24_UNORM_S8_UINT) ? 0 : 1;
          if(m_Overlay.m_DepthResolvePipeline[fmtIndex][sampleIndex] == 0)
          {
            RDCERR("Unhandled depth resolve format : %s", ToStr(dsNewFmt).c_str());
            useDepthWriteStencilPass = false;
          }
          if(dsNewFmt != dsFmt)
          {
            needDepthCopyToDepthStencil = true;
            if(m_Overlay.m_DepthCopyPipeline[fmtIndex][sampleIndex] == 0)
            {
              RDCERR("Unhandled depth copy format : %s", ToStr(dsNewFmt).c_str());
              useDepthWriteStencilPass = false;
              needDepthCopyToDepthStencil = false;
            }
          }
          // Currently depth-copy is only supported for Texture2D and Texture2DMS
          if(dsFmt != dsNewFmt)
          {
            if(depthImageInfo.type != VK_IMAGE_TYPE_2D)
              useDepthWriteStencilPass = false;
          }
          if(!useDepthWriteStencilPass)
          {
            RDCWARN("Depth overlay using fallback method instead of stencil mask");
            dsNewFmt = dsFmt;
          }
        }

        attDescs[1].format = dsNewFmt;
        attDescs[0].samples = attDescs[1].samples = iminfo.samples;

        {
          LockedConstImageStateRef imState = m_pDriver->FindConstImageState(depthIm);
          if(imState)
          {
            // find the state that overlaps the view's subresource range start. We assume all
            // subresources are correctly in the same state (as they should be) so we just need to
            // find the first match.
            auto it = imState->subresourceStates.RangeBegin(depthViewInfo.range);
            if(it != imState->subresourceStates.end())
              attDescs[1].initialLayout = attDescs[1].finalLayout = it->state().newLayout;
          }
        }

        VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference dsRef = {1, attDescs[1].initialLayout};

        VkSubpassDescription subp = {
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
            &subp,
            0,
            NULL,    // dependencies
        };

        if(multiviewMask > 0)
          rpinfo.pNext = &multiviewRP;

        vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &depthRP);
        CheckVkResult(vkr);

        VkImageView dsView =
            m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(depthStencilView);

        if(needDepthCopyToDepthStencil)
        {
          VkImageSubresourceRange dsSubRange = {
              VK_IMAGE_ASPECT_DEPTH_BIT, sub.mip, 1, sub.slice, sub.numSlices,
          };

          VkImageViewCreateInfo dsViewInfo = {
              VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
              NULL,
              0,
              dsDepthImage,
              VK_IMAGE_VIEW_TYPE_2D,
              dsFmt,
              {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
              dsSubRange,
          };

          vkr = m_pDriver->vkCreateImageView(m_Device, &dsViewInfo, NULL, &dsView);

          // update descriptor to point to copy of original depth buffer
          VkDescriptorImageInfo imdesc = {0};
          imdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
          imdesc.sampler = VK_NULL_HANDLE;
          imdesc.imageView = Unwrap(dsView);

          VkWriteDescriptorSet write = {
              VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              NULL,
              Unwrap(m_Overlay.m_DepthCopyDescSet),
              0,
              0,
              1,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              &imdesc,
              NULL,
              NULL,
          };
          vt->UpdateDescriptorSets(Unwrap(m_Device), 1, &write, 0, NULL);

          // Create texture for new depth buffer
          VkImageCreateInfo dsNewImInfo = {
              VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
              NULL,
              0,
              VK_IMAGE_TYPE_2D,
              dsNewFmt,
              depthImageInfo.extent,
              (uint32_t)depthImageInfo.mipLevels,
              (uint32_t)depthImageInfo.arrayLayers,
              depthImageInfo.samples,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              VK_SHARING_MODE_EXCLUSIVE,
              0,
              NULL,
              VK_IMAGE_LAYOUT_UNDEFINED,
          };

          vkr = m_pDriver->vkCreateImage(m_Device, &dsNewImInfo, NULL, &dsTempImage);
          CheckVkResult(vkr);

          NameVulkanObject(dsTempImage, "Overlay Depth+Stencil Image");

          VkMemoryRequirements mrq = {0};
          m_pDriver->vkGetImageMemoryRequirements(m_Device, dsTempImage, &mrq);

          VkMemoryAllocateInfo allocInfo = {
              VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
              NULL,
              mrq.size,
              m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
          };

          vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &dsTempImageMem);
          CheckVkResult(vkr);

          if(vkr != VK_SUCCESS)
            return ResourceId();

          vkr = m_pDriver->vkBindImageMemory(m_Device, dsTempImage, dsTempImageMem, 0);
          CheckVkResult(vkr);

          cmd = m_pDriver->GetNextCmd();
          if(cmd == VK_NULL_HANDLE)
            return ResourceId();

          vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);

          // move original depth buffer to shader read state
          m_pDriver->FindImageState(depthIm)->InlineTransition(
              cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_GENERAL,
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
              m_pDriver->GetImageTransitionInfo());

          // transition new depth buffer to depth write state
          m_pDriver->FindImageState(GetResID(dsTempImage))
              ->InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_GENERAL, 0,
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                 m_pDriver->GetImageTransitionInfo());

          vkr = vt->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);
          cmd = VK_NULL_HANDLE;

          dsSubRange = {
              VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
              sub.mip,
              1,
              sub.slice,
              sub.numSlices,
          };

          dsViewInfo = {
              VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
              NULL,
              0,
              dsTempImage,
              VK_IMAGE_VIEW_TYPE_2D,
              dsNewFmt,
              {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
              dsSubRange,
          };

          vkr = m_pDriver->vkCreateImageView(m_Device, &dsViewInfo, NULL, &dsView);
          CheckVkResult(vkr);
          dsDepthImage = dsTempImage;
        }

        VkImageView views[] = {
            m_Overlay.ImageView,
            dsView,
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
        CheckVkResult(vkr);

        // Fullscreen pass using shader to copy original depth buffer -> new depth buffer
        // Pipeline also writes 0 to the stencil during the pass
        if(needDepthCopyToDepthStencil)
        {
          cmd = m_pDriver->GetNextCmd();
          if(cmd == VK_NULL_HANDLE)
            return ResourceId();

          vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);

          VkClearValue clearval = {};
          VkRenderPassBeginInfo rpbegin = {
              VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
              NULL,
              Unwrap(depthRP),
              Unwrap(depthFB),
              state.renderArea,
              1,
              &clearval,
          };
          vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

          vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_Overlay.m_DepthCopyPipeline[fmtIndex][sampleIndex]));
          vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    Unwrap(m_Overlay.m_DepthCopyPipeLayout), 0, 1,
                                    UnwrapPtr(m_Overlay.m_DepthCopyDescSet), 0, NULL);

          VkViewport viewport = {
              0.0f, 0.0f, (float)m_Overlay.ImageDim.width, (float)m_Overlay.ImageDim.height,
              0.0f, 1.0f,
          };
          vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

          vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
          vt->CmdEndRenderPass(Unwrap(cmd));

          vkr = vt->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);
          cmd = VK_NULL_HANDLE;
        }
        dsFmt = dsNewFmt;
      }

      // if depthRP is NULL, so is depthFB, and it means no depth buffer was
      // bound, so we just render green.

      highlightCol[0] = 1.0f;
      highlightCol[1] = 0.0f;
      highlightCol[3] = 1.0f;

      // backup state
      VulkanRenderState prevstate = state;

      // make patched shader
      VkShaderModule failmod = {}, passmod = {};
      VkShaderEXT failshad = {}, passshad = {};
      VkPipeline failpipe = {}, passpipe = {}, depthWriteStencilPipe = {};

      // first shader, no depth/stencil testing, writes red
      if(!state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShader(failmod, highlightCol);
      else
        GetDebugManager()->PatchFixedColShaderObject(failshad, highlightCol);

      highlightCol[0] = 0.0f;
      highlightCol[1] = 1.0f;

      // second shader, enabled depth/stencil testing, writes green
      if(!state.graphics.shaderObject)
        GetDebugManager()->PatchFixedColShader(passmod, highlightCol);
      else
        GetDebugManager()->PatchFixedColShaderObject(passshad, highlightCol);

      // save original state
      VkBool32 origDepthTest = prevstate.depthTestEnable;
      VkBool32 origStencilTest = prevstate.stencilTestEnable;

      // make patched pipeline
      VkGraphicsPipelineCreateInfo pipeCreateInfo;

      m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                            prevstate.graphics.pipeline);

      if(!state.graphics.shaderObject)
      {
        // disable all tests possible
        VkPipelineDepthStencilStateCreateInfo *ds =
            (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
        ds->depthTestEnable = false;
        ds->depthWriteEnable = false;
        ds->stencilTestEnable = false;
        ds->depthBoundsTestEnable = false;

        VkPipelineMultisampleStateCreateInfo *msaa =
            (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;
        msaa->pSampleMask = NULL;

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

        // subpass 0 in either render pass
        pipeCreateInfo.subpass = 0;

        VkPipelineShaderStageCreateInfo orgFragShader = {};
        VkPipelineShaderStageCreateInfo *fragShader = NULL;

        for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
        {
          VkPipelineShaderStageCreateInfo &sh =
              (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
          if(sh.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
          {
            orgFragShader = sh;
            sh.pName = "main";
            fragShader = &sh;
            break;
          }
        }

        if(fragShader == NULL)
        {
          useDepthWriteStencilPass = false;
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
          {
            ds->front.passOp = ds->front.failOp = ds->front.depthFailOp = VK_STENCIL_OP_KEEP;
            ds->back.passOp = ds->back.failOp = ds->back.depthFailOp = VK_STENCIL_OP_KEEP;
            ds->stencilTestEnable = origStencilTest;
          }
          pipeCreateInfo.renderPass = depthRP;
        }
        else
        {
          pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;
        }

        // don't use dynamic rendering
        RemoveNextStruct(&pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &passpipe);
        CheckVkResult(vkr);

        fragShader->module = failmod;

        // set our renderpass and shader
        pipeCreateInfo.renderPass = m_Overlay.NoDepthRP;

        // disable culling/discard and enable depth clamp. That way we show any failures due to these
        VkPipelineRasterizationStateCreateInfo *rs =
            (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
        VkPipelineRasterizationStateCreateInfo orgRS = *rs;
        rs->cullMode = VK_CULL_MODE_NONE;
        rs->rasterizerDiscardEnable = false;

        if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
          rs->depthClampEnable = true;

        vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                   NULL, &failpipe);
        CheckVkResult(vkr);

        if(useDepthWriteStencilPass)
        {
          pipeCreateInfo.renderPass = depthRP;
          *rs = orgRS;

          // disable colour write
          for(uint32_t i = 0; i < cb->attachmentCount; i++)
          {
            VkPipelineColorBlendAttachmentState *att =
                (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
            att->blendEnable = false;
            att->colorWriteMask = 0x0;
          }

          // Write stencil 0x1 for depth passing pixels
          ds->stencilTestEnable = true;
          ds->front.compareOp = VK_COMPARE_OP_ALWAYS;
          ds->front.failOp = VK_STENCIL_OP_KEEP;
          ds->front.depthFailOp = VK_STENCIL_OP_KEEP;
          ds->front.passOp = VK_STENCIL_OP_REPLACE;
          ds->front.compareMask = 0xff;
          ds->front.reference = 0x1;
          ds->front.writeMask = 0xff;
          ds->back = ds->front;

          // Use original shader
          *fragShader = orgFragShader;

          vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                     NULL, &depthWriteStencilPipe);
          CheckVkResult(vkr);
        }
      }

      // modify state
      state.SetRenderPass(GetResID(m_Overlay.NoDepthRP));
      state.subpass = 0;
      state.SetFramebuffer(m_pDriver, GetResID(m_Overlay.NoDepthFB));

      state.graphics.pipeline = GetResID(failpipe);

      // disable tests in dynamic state too
      state.depthTestEnable = VK_FALSE;
      state.depthWriteEnable = VK_FALSE;
      state.stencilTestEnable = VK_FALSE;
      state.depthBoundsTestEnable = VK_FALSE;
      state.cullMode = VK_CULL_MODE_NONE;

      // enable dynamic depth clamp
      if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
        state.depthClampEnable = true;

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(failshad);
      }

      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

      if(useDepthWriteStencilPass)
      {
        // override stencil dynamic state
        state.front.compare = 0xff;
        state.front.write = 0xff;
        state.front.ref = 0x1;
        state.front.failOp = VK_STENCIL_OP_KEEP;
        state.front.passOp = VK_STENCIL_OP_REPLACE;
        state.front.depthFailOp = VK_STENCIL_OP_KEEP;
        state.front.compare = VK_COMPARE_OP_ALWAYS;
        state.back = state.front;
        state.graphics.pipeline = GetResID(depthWriteStencilPipe);
      }
      else
      {
        state.graphics.pipeline = GetResID(passpipe);
      }

      if(depthRP != VK_NULL_HANDLE)
      {
        state.SetRenderPass(GetResID(depthRP));
        state.SetFramebuffer(m_pDriver, GetResID(depthFB));
      }

      if(overlay == DebugOverlay::Depth)
        state.depthTestEnable = origDepthTest;
      else
        state.stencilTestEnable = origStencilTest;

      if(useDepthWriteStencilPass)
      {
        cmd = m_pDriver->GetNextCmd();
        if(cmd == VK_NULL_HANDLE)
          return ResourceId();

        vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        CheckVkResult(vkr);

        VkImageSubresourceRange sSubRange = {
            VK_IMAGE_ASPECT_STENCIL_BIT, sub.mip, 1, sub.slice, sub.numSlices,
        };
        VkImageSubresourceRange dsSubRange = sSubRange;
        dsSubRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

        VkClearDepthStencilValue depthStencilClear = {};
        depthStencilClear.depth = 0.0f;
        depthStencilClear.stencil = 0;

        {
          VkImageLayout startLayout = m_pDriver->GetDebugManager()->GetImageLayout(
              GetResID(dsDepthImage), (VkImageAspectFlagBits)dsSubRange.aspectMask,
              dsSubRange.baseMipLevel, dsSubRange.baseArrayLayer);

          VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                          NULL,
                                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                          VK_ACCESS_TRANSFER_WRITE_BIT,
                                          startLayout,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_QUEUE_FAMILY_IGNORED,
                                          VK_QUEUE_FAMILY_IGNORED,
                                          Unwrap(dsDepthImage),
                                          dsSubRange};
          DoPipelineBarrier(cmd, 1, &barrier);

          vt->CmdClearDepthStencilImage(Unwrap(cmd), Unwrap(dsDepthImage),
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthStencilClear, 1,
                                        &sSubRange);
          std::swap(barrier.oldLayout, barrier.newLayout);
          std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        vkr = vt->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);
        cmd = VK_NULL_HANDLE;
      }

      if(state.graphics.shaderObject)
      {
        state.graphics.pipeline = ResourceId();
        state.shaderObjects[4] = GetResID(passshad);
      }

      m_pDriver->ReplayLog(0, eventId, eReplay_OnlyDraw);

      if(useDepthWriteStencilPass)
      {
        // Resolve stencil = 0x1 pixels to green
        cmd = m_pDriver->GetNextCmd();

        if(cmd == VK_NULL_HANDLE)
          return ResourceId();

        RDCASSERT((dsFmt == VK_FORMAT_D24_UNORM_S8_UINT) || (dsFmt == VK_FORMAT_D32_SFLOAT_S8_UINT));
        vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        CheckVkResult(vkr);

        VkClearValue clearval = {};
        VkRenderPassBeginInfo rpbegin = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            NULL,
            Unwrap(depthRP),
            Unwrap(depthFB),
            state.renderArea,
            1,
            &clearval,
        };
        vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

        vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Unwrap(m_Overlay.m_DepthResolvePipeline[fmtIndex][sampleIndex]));

        VkViewport viewport = {
            0.0f, 0.0f, (float)m_Overlay.ImageDim.width, (float)m_Overlay.ImageDim.height,
            0.0f, 1.0f,
        };
        vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
        vt->CmdEndRenderPass(Unwrap(cmd));

        vkr = vt->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);
        cmd = VK_NULL_HANDLE;
      }

      // submit & flush so that we don't have to keep pipeline around for a while
      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      // restore state
      state = prevstate;

      m_pDriver->vkDestroyPipeline(m_Device, failpipe, NULL);
      m_pDriver->vkDestroyShaderModule(m_Device, failmod, NULL);
      m_pDriver->vkDestroyPipeline(m_Device, passpipe, NULL);
      m_pDriver->vkDestroyShaderModule(m_Device, passmod, NULL);
      m_pDriver->vkDestroyPipeline(m_Device, depthWriteStencilPipe, NULL);
      m_pDriver->vkDestroyImage(m_Device, dsTempImage, NULL);
      m_pDriver->vkFreeMemory(m_Device, dsTempImageMem, NULL);

      if(failshad != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(m_Device, failshad, NULL);
      if(passshad != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(m_Device, passshad, NULL);

      if(depthRP != VK_NULL_HANDLE)
      {
        m_pDriver->vkDestroyRenderPass(m_Device, depthRP, NULL);
        m_pDriver->vkDestroyFramebuffer(m_Device, depthFB, NULL);
      }
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
                                    subRange};

    DoPipelineBarrier(cmd, 1, &barrier);

    vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (VkClearColorValue *)black, 1, &subRange);

    std::swap(barrier.oldLayout, barrier.newLayout);
    std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
    barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    DoPipelineBarrier(cmd, 1, &barrier);

    rdcarray<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    {
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      if(Vulkan_Debug_SingleSubmitFlushing())
        m_pDriver->SubmitCmds();

      size_t startEvent = 0;

      // if we're ClearBeforePass the first event will be a vkBeginRenderPass.
      // if there are any other events, we need to play up to right before them
      // so that we have all the render state set up to do
      // BeginRenderPassAndApplyState and a clear. If it's just the begin, we
      // just play including it, do the clear, then we won't replay anything
      // in the loop below
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        const ActionDescription *action = m_pDriver->GetAction(events[0]);
        if(action && action->flags & ActionFlags::BeginPass)
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

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);

      VkClearAttachment clearatt = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {}};
      memcpy(clearatt.clearValue.color.float32, &clearCol, sizeof(clearatt.clearValue.color.float32));
      rdcarray<VkClearAttachment> atts;

      if(state.dynamicRendering.active)
      {
        for(size_t i = 0; i < state.dynamicRendering.color.size(); i++)
        {
          if(state.dynamicRendering.color[i].imageView == VK_NULL_HANDLE)
            continue;

          clearatt.colorAttachment = (uint32_t)i;
          atts.push_back(clearatt);
        }
      }
      else
      {
        VulkanCreationInfo::RenderPass &rp =
            m_pDriver->m_CreationInfo.m_RenderPass[state.GetRenderPass()];

        for(size_t i = 0; i < rp.subpasses[state.subpass].colorAttachments.size(); i++)
        {
          clearatt.colorAttachment = (uint32_t)i;
          atts.push_back(clearatt);
        }
      }

      // Try to clear depth as well, to help debug shadow rendering
      if((state.graphics.pipeline != ResourceId() || state.graphics.shaderObject) &&
         IsDepthOrStencilFormat(iminfo.format))
      {
        VkCompareOp depthCompareOp = state.depthCompareOp;

        // If the depth func is equal or not equal, don't clear at all since the output would be
        // altered in an way that would cause replay to produce mostly incorrect results.
        // Similarly, skip if the depth func is always, as we'd have a 50% chance of guessing the
        // wrong clear value.
        if(depthCompareOp != VK_COMPARE_OP_EQUAL && depthCompareOp != VK_COMPARE_OP_NOT_EQUAL &&
           depthCompareOp != VK_COMPARE_OP_ALWAYS)
        {
          // If the depth func is less or less equal, clear to 1 instead of 0
          bool depthFuncLess =
              depthCompareOp == VK_COMPARE_OP_LESS || depthCompareOp == VK_COMPARE_OP_LESS_OR_EQUAL;
          float depthClear = depthFuncLess ? 1.0f : 0.0f;

          VkClearAttachment clearDepthAtt = {
              VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, {}};
          clearDepthAtt.clearValue.depthStencil.depth = depthClear;
          clearDepthAtt.clearValue.depthStencil.stencil = 0;

          atts.push_back(clearDepthAtt);
        }
      }

      VkClearRect rect = {
          state.renderArea,
          0,
          1,
      };

      vt->CmdClearAttachments(Unwrap(cmd), (uint32_t)atts.size(), &atts[0], 1, &rect);

      state.EndRenderPass(cmd);

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      for(size_t i = startEvent; i < events.size(); i++)
      {
        m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDriver->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);
    }
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    if(m_Overlay.m_QuadResolvePipeline[0] != VK_NULL_HANDLE && !state.rastDiscardEnable)
    {
      VulkanRenderState prevstate = state;

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
                                      subRange};

      DoPipelineBarrier(cmd, 1, &barrier);

      vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1,
                             &subRange);

      std::swap(barrier.oldLayout, barrier.newLayout);
      std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
      barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

      DoPipelineBarrier(cmd, 1, &barrier);

      rdcarray<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventId);

      // if we're rendering the whole pass, and the first action is a BeginRenderPass, don't include
      // it in the list. We want to start by replaying into the renderpass so that we have the
      // correct state being applied.
      if(overlay == DebugOverlay::QuadOverdrawPass)
      {
        const ActionDescription *action = m_pDriver->GetAction(events[0]);
        if(action->flags & ActionFlags::BeginPass)
          events.erase(0);
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
      CheckVkResult(vkr);

      NameVulkanObject(quadImg, "m_Overlay.quadImg");

      VkMemoryRequirements mrq = {0};

      m_pDriver->vkGetImageMemoryRequirements(m_Device, quadImg, &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          NULL,
          mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &quadImgMem);
      CheckVkResult(vkr);

      if(vkr != VK_SUCCESS)
        return ResourceId();

      vkr = m_pDriver->vkBindImageMemory(m_Device, quadImg, quadImgMem, 0);
      CheckVkResult(vkr);

      VkImageViewCreateInfo viewinfo = {
          VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          NULL,
          0,
          quadImg,
          VK_IMAGE_VIEW_TYPE_2D_ARRAY,
          VK_FORMAT_R32_UINT,
          {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 4},
      };

      vkr = m_pDriver->vkCreateImageView(m_Device, &viewinfo, NULL, &quadImgView);
      CheckVkResult(vkr);

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
          VK_STRUCTURE_TYPE_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_ALL_WRITE_BITS,
          VK_ACCESS_ALL_READ_BITS,
      };

      DoPipelineBarrier(cmd, 1, &memBarrier);

      // end this cmd buffer so the image is in the right state for the next part
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      if(Vulkan_Debug_SingleSubmitFlushing())
        m_pDriver->SubmitCmds();

      m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);

      {
        // declare callback struct here
        VulkanQuadOverdrawCallback cb(m_pDriver, m_Overlay.m_QuadDescSetLayout,
                                      m_Overlay.m_QuadDescSet, events);

        m_pDriver->ReplayLog(events.front(), events.back(), eReplay_Full);

        // resolve pass
        {
          cmd = m_pDriver->GetNextCmd();

          if(cmd == VK_NULL_HANDLE)
            return ResourceId();

          vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);

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
              state.renderArea,
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
          CheckVkResult(vkr);
        }

        m_pDriver->SubmitCmds();
        m_pDriver->FlushQ();

        m_pDriver->vkDestroyImageView(m_Device, quadImgView, NULL);
        m_pDriver->vkDestroyImage(m_Device, quadImg, NULL);
        m_pDriver->vkFreeMemory(m_Device, quadImgMem, NULL);
      }

      // restore back to normal
      m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);
    }
  }
  else if(overlay == DebugOverlay::TriangleSizePass || overlay == DebugOverlay::TriangleSizeDraw)
  {
    if(!state.rastDiscardEnable)
    {
      VulkanRenderState prevstate = state;

      VkPipelineShaderStageCreateInfo stages[3] = {
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
           shaderCache->GetBuiltinModule(BuiltinShader::MeshVS), "main", NULL},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
           shaderCache->GetBuiltinModule(BuiltinShader::TrisizeFS), "main", NULL},
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_GEOMETRY_BIT,
           shaderCache->GetBuiltinModule(BuiltinShader::TrisizeGS), "main", NULL},
      };

      if(stages[0].module != VK_NULL_HANDLE && stages[1].module != VK_NULL_HANDLE &&
         stages[2].module != VK_NULL_HANDLE)
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
                                        subRange};

        DoPipelineBarrier(cmd, 1, &barrier);

        vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_Overlay.Image),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1,
                               &subRange);

        std::swap(barrier.oldLayout, barrier.newLayout);
        std::swap(barrier.srcAccessMask, barrier.dstAccessMask);
        barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        DoPipelineBarrier(cmd, 1, &barrier);

        // end this cmd buffer so the image is in the right state for the next part
        vkr = vt->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);

        if(Vulkan_Debug_SingleSubmitFlushing())
          m_pDriver->SubmitCmds();

        rdcarray<uint32_t> events = passEvents;

        if(overlay == DebugOverlay::TriangleSizeDraw)
          events.clear();

        while(!events.empty())
        {
          const ActionDescription *action = m_pDriver->GetAction(events[0]);

          // remove any non-drawcalls, like the pass boundary.
          if(!action || !(action->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall)))
            events.erase(0);
          else
            break;
        }

        events.push_back(eventId);

        m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);

        uint32_t meshOffs = 0;
        MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&meshOffs);
        if(!data)
          return ResourceId();

        data->mvp = Matrix4f::Identity();
        data->invProj = Matrix4f::Identity();
        data->color = Vec4f();
        data->homogenousInput = 1;
        data->pointSpriteSize = Vec2f(0.0f, 0.0f);
        data->displayFormat = 0;
        data->rawoutput = 1;
        data->flipY = 0;
        data->vtxExploderSNorm = 0.0f;
        data->exploderScale = 0.0f;
        data->exploderCentre = Vec3f();
        m_MeshRender.UBO.Unmap();

        uint32_t viewOffs = 0;
        Vec4f *ubo = (Vec4f *)m_Overlay.m_TriSizeUBO.Map(&viewOffs);
        if(!ubo)
          return ResourceId();
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

        ResourceId depthStencilView;

        if(state.dynamicRendering.active)
        {
          depthStencilView = GetResID(state.dynamicRendering.depth.imageView);
          if(depthStencilView == ResourceId())
            depthStencilView = GetResID(state.dynamicRendering.stencil.imageView);
        }
        else
        {
          RDCASSERT(state.subpass < createinfo.m_RenderPass[state.GetRenderPass()].subpasses.size());
          int32_t dsIdx =
              createinfo.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].depthstencilAttachment;

          // make a renderpass and framebuffer for rendering to overlay color and using
          // depth buffer from the orignial render
          if(dsIdx >= 0 &&
             dsIdx < (int32_t)createinfo.m_Framebuffer[state.GetFramebuffer()].attachments.size())
          {
            depthStencilView = state.GetFramebufferAttachments()[dsIdx];
          }
        }

        if(depthStencilView != ResourceId())
        {
          VkAttachmentDescription attDescs[] = {
              {0, overlayFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD,
               VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
              {0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT,    // will patch this just below
               VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
               VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
          };

          VulkanCreationInfo::ImageView &depthViewInfo = createinfo.m_ImageView[depthStencilView];

          ResourceId depthIm = depthViewInfo.image;
          VulkanCreationInfo::Image &depthImageInfo = createinfo.m_Image[depthIm];

          attDescs[1].format = depthImageInfo.format;
          attDescs[0].samples = attDescs[1].samples = iminfo.samples;

          {
            LockedConstImageStateRef imState = m_pDriver->FindConstImageState(depthIm);
            if(imState)
            {
              // find the state that overlaps the view's subresource range start. We assume all
              // subresources are correctly in the same state (as they should be) so we just need to
              // find the first match.
              auto it = imState->subresourceStates.RangeBegin(depthViewInfo.range);
              if(it != imState->subresourceStates.end())
                attDescs[1].initialLayout = attDescs[1].finalLayout = it->state().newLayout;
            }
          }

          VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
          VkAttachmentReference dsRef = {1, attDescs[1].initialLayout};

          VkSubpassDescription subp = {
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
              &subp,
              0,
              NULL,    // dependencies
          };

          if(multiviewMask > 0)
            rpinfo.pNext = &multiviewRP;

          vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &RP);
          CheckVkResult(vkr);

          VkImageView views[] = {
              m_Overlay.ImageView,
              m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(depthStencilView),
          };

          // Create framebuffer rendering just to overlay image, no depth
          VkFramebufferCreateInfo fbinfo = {
              VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              NULL,
              0,
              RP,
              2,
              views,
              RDCMAX(1U, m_Overlay.ImageDim.width >> sub.mip),
              RDCMAX(1U, m_Overlay.ImageDim.height >> sub.mip),
              sub.numSlices,
          };

          vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &FB);
          CheckVkResult(vkr);
        }

        VkGraphicsPipelineCreateInfo pipeCreateInfo;

        m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo,
                                                              state.graphics.pipeline);

        VkPipelineInputAssemblyStateCreateInfo ia = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

        // ia.topology will be set below on a per-draw basis

        VkVertexInputBindingDescription binds[] = {
            // primary
            {0, 0, VK_VERTEX_INPUT_RATE_VERTEX},
            // secondary
            {1, 0, VK_VERTEX_INPUT_RATE_VERTEX},
        };

        VkVertexInputAttributeDescription vertAttrs[] = {
            {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
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

        // don't use dynamic rendering
        RemoveNextStruct(&pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

        if(pipeCreateInfo.pDynamicState)
        {
          uint32_t &dynamicStateCount = (uint32_t &)pipeCreateInfo.pDynamicState->dynamicStateCount;
          VkDynamicState *dynamicStateList =
              (VkDynamicState *)pipeCreateInfo.pDynamicState->pDynamicStates;

          // remove any dynamic states we don't want
          for(uint32_t i = 0; i < dynamicStateCount;)
          {
            // we are controlling the vertex binding so we don't need the stride or input to be
            // dynamic.
            // Similarly we're controlling the topology so that doesn't need to be dynamic
            if(dynamicStateList[i] == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE ||
               dynamicStateList[i] == VK_DYNAMIC_STATE_VERTEX_INPUT_EXT ||
               dynamicStateList[i] == VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY)
            {
              // swap with the last item if this isn't the last one
              if(i != dynamicStateCount - 1)
                std::swap(dynamicStateList[i], dynamicStateList[dynamicStateCount - 1]);

              // then pop the last item.
              dynamicStateCount--;

              // process this item again. If we swapped we'll then consider that dynamic state, and
              // if we didn't then this was the last item and i will be past dynamicStateCount now
              continue;
            }

            i++;
          }
        }

        typedef rdcpair<uint32_t, Topology> PipeKey;

        std::map<PipeKey, VkPipeline> pipes;

        // shader object vertex state
        VkVertexInputBindingDescription2EXT soBinds[2];
        VkVertexInputAttributeDescription2EXT soAttrs[2];

        VkShaderStageFlagBits stageFlags[3] = {
            VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_GEOMETRY_BIT};
        VkShaderEXT shaders[3] = {0};
        VkShaderEXT unwrappedShaders[3] = {0};

        if(state.graphics.shaderObject)
        {
          // create the tri-size shader objects
          const VulkanCreationInfo::PipelineLayout &layoutInfo =
              createinfo.m_PipelineLayout[GetResID(m_Overlay.m_TriSizePipeLayout)];

          rdcarray<VkDescriptorSetLayout> descSetLayouts;
          for(ResourceId setLayout : layoutInfo.descSetLayouts)
            descSetLayouts.push_back(
                m_pDriver->GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(setLayout));

          VkShaderCreateInfoEXT shadInfo = {
              VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
              NULL,
              0,
              VK_SHADER_STAGE_VERTEX_BIT,
              VK_SHADER_STAGE_GEOMETRY_BIT,
              VK_SHADER_CODE_TYPE_SPIRV_EXT,
              shaderCache->GetBuiltinBlob(BuiltinShader::MeshVS)->size() * sizeof(uint32_t),
              shaderCache->GetBuiltinBlob(BuiltinShader::MeshVS)->data(),
              "main",
              (uint32_t)descSetLayouts.size(),
              descSetLayouts.data(),
              (uint32_t)layoutInfo.pushRanges.size(),
              layoutInfo.pushRanges.data(),
              NULL};

          vkr = m_pDriver->vkCreateShadersEXT(m_Device, 1, &shadInfo, NULL, &shaders[0]);

          shadInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
          shadInfo.nextStage = 0;
          shadInfo.codeSize =
              shaderCache->GetBuiltinBlob(BuiltinShader::TrisizeFS)->size() * sizeof(uint32_t);
          shadInfo.pCode = shaderCache->GetBuiltinBlob(BuiltinShader::TrisizeFS)->data();

          vkr = m_pDriver->vkCreateShadersEXT(m_Device, 1, &shadInfo, NULL, &shaders[1]);

          shadInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
          shadInfo.nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
          shadInfo.codeSize =
              shaderCache->GetBuiltinBlob(BuiltinShader::TrisizeGS)->size() * sizeof(uint32_t);
          shadInfo.pCode = shaderCache->GetBuiltinBlob(BuiltinShader::TrisizeGS)->data();

          vkr = m_pDriver->vkCreateShadersEXT(m_Device, 1, &shadInfo, NULL, &shaders[2]);

          // vertex state
          // primary
          soBinds[0] = {VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
                        0,
                        0,
                        0,
                        VK_VERTEX_INPUT_RATE_VERTEX,
                        1};
          // secondary
          soBinds[1] = {VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
                        0,
                        1,
                        0,
                        VK_VERTEX_INPUT_RATE_VERTEX,
                        1};

          soAttrs[0] = {VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                        0,
                        0,
                        0,
                        VK_FORMAT_R32G32B32A32_SFLOAT,
                        0};
          soAttrs[1] = {VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                        0,
                        1,
                        0,
                        VK_FORMAT_R32G32B32A32_SFLOAT,
                        0};

          unwrappedShaders[0] = Unwrap(shaders[0]);
          unwrappedShaders[1] = Unwrap(shaders[1]);
          unwrappedShaders[2] = Unwrap(shaders[2]);
        }

        for(size_t i = 0; i < events.size(); i++)
        {
          cmd = m_pDriver->GetNextCmd();

          if(cmd == VK_NULL_HANDLE)
            return ResourceId();

          vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);

          VkClearValue clearval = {};
          VkRenderPassBeginInfo rpbegin = {
              VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
              NULL,
              Unwrap(RP),
              Unwrap(FB),
              {
                  {0, 0},
                  {
                      RDCMAX(1U, m_Overlay.ImageDim.width >> sub.mip),
                      RDCMAX(1U, m_Overlay.ImageDim.height >> sub.mip),
                  },
              },
              1,
              &clearval,
          };
          vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

          const ActionDescription *action = m_pDriver->GetAction(events[i]);

          for(uint32_t inst = 0; action && inst < RDCMAX(1U, action->numInstances); inst++)
          {
            MeshFormat fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::GSOut);
            if(fmt.vertexResourceId == ResourceId())
              fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::VSOut);
            if(fmt.vertexResourceId == ResourceId())
              fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::MeshOut);

            if(fmt.vertexResourceId != ResourceId())
            {
              ia.topology = MakeVkPrimitiveTopology(fmt.topology);

              binds[0].stride = binds[1].stride = fmt.vertexByteStride;
              soBinds[0].stride = soBinds[1].stride = fmt.vertexByteStride;

              PipeKey key = make_rdcpair(fmt.vertexByteStride, fmt.topology);
              VkPipeline pipe = pipes[key];

              if(pipe == VK_NULL_HANDLE && !state.graphics.shaderObject)
              {
                vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
                                                           &pipeCreateInfo, NULL, &pipe);
                CheckVkResult(vkr);
              }

              VkBuffer vb =
                  m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.vertexResourceId);

              VkDeviceSize offs = fmt.vertexByteOffset;
              vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);

              pipes[key] = pipe;

              vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Unwrap(m_Overlay.m_TriSizePipeLayout), 0, 1,
                                        UnwrapPtr(m_Overlay.m_TriSizeDescSet), 2, offsets);

              if(state.graphics.shaderObject)
                vt->CmdBindShadersEXT(Unwrap(cmd), 3, stageFlags, unwrappedShaders);
              else
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
                  vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT,
                                             state.front.write);
                }
                else if(d == VK_DYNAMIC_STATE_STENCIL_REFERENCE)
                {
                  vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.ref);
                  vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.ref);
                }
                else if(d == VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT)
                {
                  vt->CmdSetViewportWithCountEXT(Unwrap(cmd), (uint32_t)state.views.size(),
                                                 state.views.data());
                }
                else if(d == VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT)
                {
                  vt->CmdSetScissorWithCountEXT(Unwrap(cmd), (uint32_t)state.scissors.size(),
                                                state.scissors.data());
                }
                else if(d == VK_DYNAMIC_STATE_CULL_MODE)
                {
                  vt->CmdSetCullModeEXT(Unwrap(cmd), state.cullMode);
                }
                else if(d == VK_DYNAMIC_STATE_FRONT_FACE)
                {
                  vt->CmdSetFrontFaceEXT(Unwrap(cmd), state.frontFace);
                }
                else if(d == VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY)
                {
                  RDCERR("Primitive topology dynamic state found, should have been stripped");
                }
                else if(d == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE)
                {
                  RDCERR(
                      "Vertex input binding stride dynamic state found, should have been stripped");
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE)
                {
                  vt->CmdSetDepthTestEnableEXT(Unwrap(cmd), state.depthTestEnable);
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE)
                {
                  vt->CmdSetDepthWriteEnableEXT(Unwrap(cmd), state.depthWriteEnable);
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_COMPARE_OP)
                {
                  vt->CmdSetDepthCompareOpEXT(Unwrap(cmd), state.depthCompareOp);
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE)
                {
                  vt->CmdSetDepthBoundsTestEnableEXT(Unwrap(cmd), state.depthBoundsTestEnable);
                }
                else if(d == VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE)
                {
                  vt->CmdSetStencilTestEnableEXT(Unwrap(cmd), state.stencilTestEnable);
                }
                else if(d == VK_DYNAMIC_STATE_STENCIL_OP)
                {
                  vt->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.failOp,
                                         state.front.passOp, state.front.depthFailOp,
                                         state.front.compareOp);
                  vt->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.front.failOp,
                                         state.front.passOp, state.front.depthFailOp,
                                         state.front.compareOp);
                }
                else if(d == VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT)
                {
                  vt->CmdSetColorWriteEnableEXT(Unwrap(cmd), (uint32_t)state.colorWriteEnable.size(),
                                                state.colorWriteEnable.data());
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE)
                {
                  vt->CmdSetDepthBiasEnableEXT(Unwrap(cmd), state.depthBiasEnable);
                }
                else if(d == VK_DYNAMIC_STATE_LOGIC_OP_EXT)
                {
                  vt->CmdSetLogicOpEXT(Unwrap(cmd), state.logicOp);
                }
                else if(d == VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT)
                {
                  vt->CmdSetPatchControlPointsEXT(Unwrap(cmd), state.patchControlPoints);
                }
                else if(d == VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE)
                {
                  vt->CmdSetPrimitiveRestartEnableEXT(Unwrap(cmd), state.primRestartEnable);
                }
                else if(d == VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE)
                {
                  vt->CmdSetRasterizerDiscardEnableEXT(Unwrap(cmd), state.rastDiscardEnable);
                }
                else if(d == VK_DYNAMIC_STATE_VERTEX_INPUT_EXT)
                {
                  RDCERR("Vertex input dynamic state found, should have been stripped");
                }
                else if(d == VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT)
                {
                  vt->CmdSetAttachmentFeedbackLoopEnableEXT(Unwrap(cmd), state.feedbackAspects);
                }
                else if(d == VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT)
                {
                  vt->CmdSetAlphaToCoverageEnableEXT(Unwrap(cmd), state.alphaToCoverageEnable);
                }
                else if(d == VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT)
                {
                  vt->CmdSetAlphaToOneEnableEXT(Unwrap(cmd), state.alphaToOneEnable);
                }
                else if(!state.colorBlendEnable.empty() &&
                        d == VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT)
                {
                  vt->CmdSetColorBlendEnableEXT(Unwrap(cmd), 0,
                                                (uint32_t)state.colorBlendEnable.size(),
                                                state.colorBlendEnable.data());
                }
                else if(!state.colorBlendEquation.empty() &&
                        d == VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT)
                {
                  vt->CmdSetColorBlendEquationEXT(Unwrap(cmd), 0,
                                                  (uint32_t)state.colorBlendEquation.size(),
                                                  state.colorBlendEquation.data());
                }
                else if(!state.colorWriteMask.empty() && d == VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT)
                {
                  vt->CmdSetColorWriteMaskEXT(Unwrap(cmd), 0, (uint32_t)state.colorWriteMask.size(),
                                              state.colorWriteMask.data());
                }
                else if(d == VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT)
                {
                  vt->CmdSetConservativeRasterizationModeEXT(Unwrap(cmd), state.conservativeRastMode);
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT)
                {
                  vt->CmdSetDepthClampEnableEXT(Unwrap(cmd), state.depthClampEnable);
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT)
                {
                  vt->CmdSetDepthClipEnableEXT(Unwrap(cmd), state.depthClipEnable);
                }
                else if(d == VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT)
                {
                  vt->CmdSetDepthClipNegativeOneToOneEXT(Unwrap(cmd), state.negativeOneToOne);
                }
                else if(d == VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT)
                {
                  vt->CmdSetExtraPrimitiveOverestimationSizeEXT(Unwrap(cmd),
                                                                state.primOverestimationSize);
                }
                else if(d == VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT)
                {
                  vt->CmdSetLineRasterizationModeEXT(Unwrap(cmd), state.lineRasterMode);
                }
                else if(d == VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT)
                {
                  vt->CmdSetLineStippleEnableEXT(Unwrap(cmd), state.stippledLineEnable);
                }
                else if(d == VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT)
                {
                  vt->CmdSetLogicOpEnableEXT(Unwrap(cmd), state.logicOpEnable);
                }
                else if(d == VK_DYNAMIC_STATE_POLYGON_MODE_EXT)
                {
                  vt->CmdSetPolygonModeEXT(Unwrap(cmd), state.polygonMode);
                }
                else if(d == VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT)
                {
                  vt->CmdSetProvokingVertexModeEXT(Unwrap(cmd), state.provokingVertexMode);
                }
                else if(d == VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT)
                {
                  vt->CmdSetRasterizationSamplesEXT(Unwrap(cmd), state.rastSamples);
                }
                else if(d == VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT)
                {
                  vt->CmdSetRasterizationStreamEXT(Unwrap(cmd), state.rasterStream);
                }
                else if(d == VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT)
                {
                  vt->CmdSetSampleLocationsEnableEXT(Unwrap(cmd), state.sampleLocEnable);
                }
                else if(d == VK_DYNAMIC_STATE_SAMPLE_MASK_EXT)
                {
                  vt->CmdSetSampleMaskEXT(Unwrap(cmd), state.rastSamples, state.sampleMask.data());
                }
                else if(d == VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT)
                {
                  vt->CmdSetTessellationDomainOriginEXT(Unwrap(cmd), state.domainOrigin);
                }
              }

              if(state.graphics.shaderObject)
              {
                if(!state.views.empty() && state.dynamicStates[VkDynamicViewport])
                {
                  vt->CmdSetViewport(Unwrap(cmd), 0, (uint32_t)state.views.size(), &state.views[0]);
                }
                if(!state.scissors.empty() && state.dynamicStates[VkDynamicScissor])
                {
                  vt->CmdSetScissor(Unwrap(cmd), 0, (uint32_t)state.scissors.size(),
                                    &state.scissors[0]);
                }
                if(state.dynamicStates[VkDynamicLineWidth])
                {
                  vt->CmdSetLineWidth(Unwrap(cmd), state.lineWidth);
                }
                if(state.dynamicStates[VkDynamicDepthBias])
                {
                  vt->CmdSetDepthBias(Unwrap(cmd), state.bias.depth, state.bias.biasclamp,
                                      state.bias.slope);
                }
                if(state.dynamicStates[VkDynamicBlendConstants])
                {
                  vt->CmdSetBlendConstants(Unwrap(cmd), state.blendConst);
                }
                if(state.dynamicStates[VkDynamicDepthBounds])
                {
                  vt->CmdSetDepthBounds(Unwrap(cmd), state.mindepth, state.maxdepth);
                }
                if(state.dynamicStates[VkDynamicStencilCompareMask])
                {
                  vt->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT,
                                               state.back.compare);
                  vt->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT,
                                               state.front.compare);
                }
                if(state.dynamicStates[VkDynamicStencilWriteMask])
                {
                  vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.write);
                  vt->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT,
                                             state.front.write);
                }
                if(state.dynamicStates[VkDynamicStencilReference])
                {
                  vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.back.ref);
                  vt->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.ref);
                }
                if(state.dynamicStates[VkDynamicViewportCount])
                {
                  vt->CmdSetViewportWithCountEXT(Unwrap(cmd), (uint32_t)state.views.size(),
                                                 state.views.data());
                }
                if(state.dynamicStates[VkDynamicScissorCount])
                {
                  vt->CmdSetScissorWithCountEXT(Unwrap(cmd), (uint32_t)state.scissors.size(),
                                                state.scissors.data());
                }
                if(state.dynamicStates[VkDynamicCullMode])
                {
                  vt->CmdSetCullModeEXT(Unwrap(cmd), state.cullMode);
                }
                if(state.dynamicStates[VkDynamicFrontFace])
                {
                  vt->CmdSetFrontFaceEXT(Unwrap(cmd), state.frontFace);
                }

                // overriding topology
                vt->CmdSetPrimitiveTopologyEXT(Unwrap(cmd), MakeVkPrimitiveTopology(fmt.topology));

                // VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE unnecessary since utilizing vertex input

                if(state.dynamicStates[VkDynamicDepthTestEnable])
                {
                  vt->CmdSetDepthTestEnableEXT(Unwrap(cmd), state.depthTestEnable);
                }
                if(state.dynamicStates[VkDynamicDepthWriteEnable])
                {
                  vt->CmdSetDepthWriteEnableEXT(Unwrap(cmd), state.depthWriteEnable);
                }
                if(state.dynamicStates[VkDynamicDepthCompareOp])
                {
                  vt->CmdSetDepthCompareOpEXT(Unwrap(cmd), state.depthCompareOp);
                }
                if(state.dynamicStates[VkDynamicDepthBoundsTestEnable])
                {
                  vt->CmdSetDepthBoundsTestEnableEXT(Unwrap(cmd), state.depthBoundsTestEnable);
                }
                if(state.dynamicStates[VkDynamicStencilTestEnable])
                {
                  vt->CmdSetStencilTestEnableEXT(Unwrap(cmd), state.stencilTestEnable);
                }
                if(state.dynamicStates[VkDynamicStencilOp])
                {
                  vt->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, state.front.failOp,
                                         state.front.passOp, state.front.depthFailOp,
                                         state.front.compareOp);
                  vt->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, state.front.failOp,
                                         state.front.passOp, state.front.depthFailOp,
                                         state.front.compareOp);
                }
                if(!state.colorWriteEnable.empty() && state.dynamicStates[VkDynamicColorWriteEXT])
                {
                  vt->CmdSetColorWriteEnableEXT(Unwrap(cmd), (uint32_t)state.colorWriteEnable.size(),
                                                state.colorWriteEnable.data());
                }
                if(state.dynamicStates[VkDynamicDepthBiasEnable])
                {
                  vt->CmdSetDepthBiasEnableEXT(Unwrap(cmd), state.depthBiasEnable);
                }
                if(state.dynamicStates[VkDynamicLogicOpEXT])
                {
                  vt->CmdSetLogicOpEXT(Unwrap(cmd), state.logicOp);
                }
                if(state.dynamicStates[VkDynamicControlPointsEXT])
                {
                  vt->CmdSetPatchControlPointsEXT(Unwrap(cmd), state.patchControlPoints);
                }
                if(state.dynamicStates[VkDynamicPrimRestart])
                {
                  vt->CmdSetPrimitiveRestartEnableEXT(Unwrap(cmd), state.primRestartEnable);
                }
                if(state.dynamicStates[VkDynamicRastDiscard])
                {
                  vt->CmdSetRasterizerDiscardEnableEXT(Unwrap(cmd), state.rastDiscardEnable);
                }

                // overriding vertex input
                vt->CmdSetVertexInputEXT(Unwrap(cmd), 2, soBinds, 2, soAttrs);

                if(state.dynamicStates[VkDynamicAttachmentFeedbackLoopEnableEXT])
                {
                  vt->CmdSetAttachmentFeedbackLoopEnableEXT(Unwrap(cmd), state.feedbackAspects);
                }
                if(state.dynamicStates[VkDynamicAlphaToCoverageEXT])
                {
                  vt->CmdSetAlphaToCoverageEnableEXT(Unwrap(cmd), state.alphaToCoverageEnable);
                }
                if(state.dynamicStates[VkDynamicAlphaToOneEXT])
                {
                  vt->CmdSetAlphaToOneEnableEXT(Unwrap(cmd), state.alphaToOneEnable);
                }
                if(!state.colorBlendEnable.empty() &&
                   state.dynamicStates[VkDynamicColorBlendEnableEXT])
                {
                  vt->CmdSetColorBlendEnableEXT(Unwrap(cmd), 0,
                                                (uint32_t)state.colorBlendEnable.size(),
                                                state.colorBlendEnable.data());
                }
                if(!state.colorBlendEquation.empty() &&
                   state.dynamicStates[VkDynamicColorBlendEquationEXT])
                {
                  vt->CmdSetColorBlendEquationEXT(Unwrap(cmd), 0,
                                                  (uint32_t)state.colorBlendEquation.size(),
                                                  state.colorBlendEquation.data());
                }
                if(!state.colorWriteMask.empty() && state.dynamicStates[VkDynamicColorWriteMaskEXT])
                {
                  vt->CmdSetColorWriteMaskEXT(Unwrap(cmd), 0, (uint32_t)state.colorWriteMask.size(),
                                              state.colorWriteMask.data());
                }
                if(state.dynamicStates[VkDynamicConservativeRastModeEXT])
                {
                  vt->CmdSetConservativeRasterizationModeEXT(Unwrap(cmd), state.conservativeRastMode);
                }
                if(state.dynamicStates[VkDynamicDepthClampEnableEXT])
                {
                  vt->CmdSetDepthClampEnableEXT(Unwrap(cmd), state.depthClampEnable);
                }
                if(state.dynamicStates[VkDynamicDepthClipEnableEXT])
                {
                  vt->CmdSetDepthClipEnableEXT(Unwrap(cmd), state.depthClipEnable);
                }
                if(state.dynamicStates[VkDynamicDepthClipNegativeOneEXT])
                {
                  vt->CmdSetDepthClipNegativeOneToOneEXT(Unwrap(cmd), state.negativeOneToOne);
                }
                if(state.dynamicStates[VkDynamicOverstimationSizeEXT])
                {
                  vt->CmdSetExtraPrimitiveOverestimationSizeEXT(Unwrap(cmd),
                                                                state.primOverestimationSize);
                }
                if(state.dynamicStates[VkDynamicLineRastModeEXT])
                {
                  vt->CmdSetLineRasterizationModeEXT(Unwrap(cmd), state.lineRasterMode);
                }
                if(state.dynamicStates[VkDynamicLineStippleEnableEXT])
                {
                  vt->CmdSetLineStippleEnableEXT(Unwrap(cmd), state.stippledLineEnable);
                }
                if(state.dynamicStates[VkDynamicLogicOpEnableEXT])
                {
                  vt->CmdSetLogicOpEnableEXT(Unwrap(cmd), state.logicOpEnable);
                }
                if(state.dynamicStates[VkDynamicPolygonModeEXT])
                {
                  vt->CmdSetPolygonModeEXT(Unwrap(cmd), state.polygonMode);
                }
                if(state.dynamicStates[VkDynamicProvokingVertexModeEXT])
                {
                  vt->CmdSetProvokingVertexModeEXT(Unwrap(cmd), state.provokingVertexMode);
                }
                if(state.dynamicStates[VkDynamicRasterizationSamplesEXT])
                {
                  vt->CmdSetRasterizationSamplesEXT(Unwrap(cmd), state.rastSamples);
                }
                if(state.dynamicStates[VkDynamicRasterizationStreamEXT])
                {
                  vt->CmdSetRasterizationStreamEXT(Unwrap(cmd), state.rasterStream);
                }
                if(state.dynamicStates[VkDynamicSampleLocationsEnableEXT])
                {
                  vt->CmdSetSampleLocationsEnableEXT(Unwrap(cmd), state.sampleLocEnable);
                }
                if(state.dynamicStates[VkDynamicSampleMaskEXT])
                {
                  vt->CmdSetSampleMaskEXT(Unwrap(cmd), state.rastSamples, state.sampleMask.data());
                }
                if(state.dynamicStates[VkDynamicTessDomainOriginEXT])
                {
                  vt->CmdSetTessellationDomainOriginEXT(Unwrap(cmd), state.domainOrigin);
                }
              }

              if(fmt.indexByteStride)
              {
                VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
                if(fmt.indexByteStride == 4)
                  idxtype = VK_INDEX_TYPE_UINT32;
                else if(fmt.indexByteStride == 1)
                  idxtype = VK_INDEX_TYPE_UINT8_KHR;

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

          vt->CmdEndRenderPass(Unwrap(cmd));

          vkr = vt->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);

          if(overlay == DebugOverlay::TriangleSizePass)
          {
            m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

            if(i + 1 < events.size())
              m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
          }
        }

        m_pDriver->SubmitCmds();
        m_pDriver->FlushQ();

        if(depthStencilView != ResourceId())
        {
          m_pDriver->vkDestroyFramebuffer(m_Device, FB, NULL);
          m_pDriver->vkDestroyRenderPass(m_Device, RP, NULL);
        }

        for(auto it = pipes.begin(); it != pipes.end(); ++it)
          m_pDriver->vkDestroyPipeline(m_Device, it->second, NULL);

        for(uint32_t i = 0; i < 3; i++)
        {
          if(shaders[i] != VK_NULL_HANDLE)
            m_pDriver->vkDestroyShaderEXT(m_Device, shaders[i], NULL);
        }
      }

      // restore back to normal
      m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

      // restore state
      state = prevstate;

      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return ResourceId();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);
    }
  }

  VkMarkerRegion::End(cmd);

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  CheckVkResult(vkr);

  if(Vulkan_Debug_SingleSubmitFlushing())
    m_pDriver->SubmitCmds();

  return GetResID(m_Overlay.Image);
}
