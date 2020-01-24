/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

bool isDirectWrite(ResourceUsage usage)
{
  return ((usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::CS_RWResource) ||
          usage == ResourceUsage::CopyDst || usage == ResourceUsage::Copy ||
          usage == ResourceUsage::Resolve || usage == ResourceUsage::ResolveDst ||
          usage == ResourceUsage::GenMips);
}

enum
{
  TestEnabled_Culling = 1 << 0,
  TestEnabled_Scissor = 1 << 1,
  TestEnabled_SampleMask = 1 << 2,
  TestEnabled_DepthBounds = 1 << 3,
  TestEnabled_StencilTesting = 1 << 4,
  TestEnabled_DepthTesting = 1 << 5,
  TestEnabled_FragmentDiscard = 1 << 6,

  Blending_Enabled = 1 << 7,
  TestMustFail_Culling = 1 << 8,
  TestMustFail_Scissor = 1 << 9,
  TestMustPass_Scissor = 1 << 10,
  TestMustFail_DepthTesting = 1 << 11,
  TestMustFail_StencilTesting = 1 << 12,
  TestMustFail_SampleMask = 1 << 13,
};

struct CopyPixelParams
{
  bool multisampled;
  bool floatTex;
  bool uintTex;
  bool intTex;

  bool depthCopy;
  bool stencilOnly;
  VkImage srcImage;
  VkFormat srcImageFormat;
  VkImageLayout srcImageLayout;
  VkOffset3D imageOffset;

  VkBuffer dstBuffer;
};

struct PixelHistoryResources
{
  VkBuffer dstBuffer;
  VkDeviceMemory bufferMemory;

  // Used for offscreen rendering for draw call events.
  VkImage colorImage;
  VkImageView colorImageView;
  VkImage stencilImage;
  VkImageView stencilImageView;
  VkDeviceMemory gpuMem;
};

struct PixelHistoryValue
{
  uint8_t color[16];
  union
  {
    uint32_t udepth;
    float fdepth;
  } depth;
  int8_t stencil;
  uint8_t padding[3];
};

struct EventInfo
{
  PixelHistoryValue premod;
  PixelHistoryValue postmod;
  PixelHistoryValue shadout;
  uint8_t dsWithoutShaderDiscard[8];
  uint8_t dsWithShaderDiscard[8];
};

struct PipelineReplacements
{
  VkPipeline fixedShaderStencil;
  VkPipeline originalShaderStencil;
};

// PixelHistoryShaderCache manages temporary shaders created for pixel history.
struct PixelHistoryShaderCache
{
  PixelHistoryShaderCache(WrappedVulkan *vk) : m_pDriver(vk), m_FixedColFS(VK_NULL_HANDLE) {}
  ~PixelHistoryShaderCache()
  {
    for(auto it = m_ShaderReplacements.begin(); it != m_ShaderReplacements.end(); ++it)
    {
      if(it->second != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderModule(m_pDriver->GetDev(), it->second, NULL);
    }
    if(m_FixedColFS != VK_NULL_HANDLE)
      m_pDriver->vkDestroyShaderModule(m_pDriver->GetDev(), m_FixedColFS, NULL);
  }

  // Returns a fragment shader module that outputs a fixed color.
  VkShaderModule GetFixedColShader()
  {
    // Create fixed colour shader if we haven't already.
    if(m_FixedColFS == VK_NULL_HANDLE)
    {
      float col[4] = {1.0, 0.0, 0.0, 1.0};
      m_pDriver->GetDebugManager()->PatchFixedColShader(m_FixedColFS, col);
    }
    return m_FixedColFS;
  }

  // Returns a shader that is equivalent to the given shader, but attempts to remove
  // side effects of shader execution for the given entry point (for ex., writes
  // to storage buffers/images).
  VkShaderModule GetShaderWithoutSideEffects(ResourceId shaderId, const rdcstr &entryPoint)
  {
    ShaderKey shaderKey = make_rdcpair(shaderId, entryPoint);
    auto it = m_ShaderReplacements.find(shaderKey);
    // Check if we processed this shader before.
    if(it != m_ShaderReplacements.end())
      return it->second;

    VkShaderModule shaderModule = CreateShaderReplacement(shaderId, entryPoint);
    m_ShaderReplacements.insert(std::make_pair(shaderKey, shaderModule));
    return shaderModule;
  }

private:
  VkShaderModule CreateShaderReplacement(ResourceId shaderId, const rdcstr &entryName)
  {
    const VulkanCreationInfo::ShaderModule &moduleInfo =
        m_pDriver->GetRenderState().m_CreationInfo->m_ShaderModule[shaderId];
    rdcarray<uint32_t> modSpirv = moduleInfo.spirv.GetSPIRV();
    rdcspv::Editor editor(modSpirv);
    editor.Prepare();

    for(const rdcspv::EntryPoint &entry : editor.GetEntries())
    {
      if(entry.name == entryName)
      {
        // In some cases a shader might just be binding a RW resource but not writing to it.
        // If there are no writes (shader was not modified), no need to replace the shader,
        // just insert VK_NULL_HANDLE to indicate that this shader has been processed.
        VkShaderModule module = VK_NULL_HANDLE;
        bool modified = StripShaderSideEffects(editor, entry.id);
        if(modified)
        {
          VkShaderModuleCreateInfo moduleCreateInfo = {};
          moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
          moduleCreateInfo.pCode = modSpirv.data();
          moduleCreateInfo.codeSize = modSpirv.byteSize();
          VkResult vkr =
              m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &moduleCreateInfo, NULL, &module);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);
        }
        return module;
      }
    }
    RDCERR("Entry point %s not found", entryName.c_str());
    return VK_NULL_HANDLE;
  }

  // Removes instructions from the shader that would produce side effects (writing
  // to storage buffers, or images). Returns true if the shader was modified, and
  // false if there were no instructions to remove.
  bool StripShaderSideEffects(rdcspv::Editor &editor, const rdcspv::Id &entryId)
  {
    bool modified = false;

    std::set<rdcspv::Id> patchedFunctions;
    std::set<rdcspv::Id> functionPatchQueue;
    functionPatchQueue.insert(entryId);

    while(!functionPatchQueue.empty())
    {
      rdcspv::Id funcId;
      {
        auto it = functionPatchQueue.begin();
        funcId = *functionPatchQueue.begin();
        functionPatchQueue.erase(it);
        patchedFunctions.insert(funcId);
      }

      rdcspv::Iter it = editor.GetID(funcId);
      RDCASSERT(it.opcode() == rdcspv::Op::Function);

      it++;

      for(; it; ++it)
      {
        rdcspv::Op opcode = it.opcode();
        if(opcode == rdcspv::Op::FunctionEnd)
          break;

        switch(opcode)
        {
          case rdcspv::Op::FunctionCall:
          {
            rdcspv::OpFunctionCall call(it);
            if(functionPatchQueue.find(call.function) == functionPatchQueue.end() &&
               patchedFunctions.find(call.function) == patchedFunctions.end())
              functionPatchQueue.insert(call.function);
            break;
          }
          case rdcspv::Op::CopyMemory:
          case rdcspv::Op::AtomicStore:
          case rdcspv::Op::Store:
          {
            rdcspv::Id pointer = rdcspv::Id::fromWord(it.word(1));
            rdcspv::Id pointerType = editor.GetIDType(pointer);
            RDCASSERT(pointerType != rdcspv::Id());
            rdcspv::Iter pointerTypeIt = editor.GetID(pointerType);
            rdcspv::OpTypePointer ptr(pointerTypeIt);
            if(ptr.storageClass == rdcspv::StorageClass::Uniform ||
               ptr.storageClass == rdcspv::StorageClass::StorageBuffer)
            {
              editor.Remove(it);
              modified = true;
            }
            break;
          }
          case rdcspv::Op::ImageWrite:
          {
            editor.Remove(it);
            modified = true;
            break;
          }
          case rdcspv::Op::AtomicExchange:
          case rdcspv::Op::AtomicCompareExchange:
          case rdcspv::Op::AtomicCompareExchangeWeak:
          case rdcspv::Op::AtomicIIncrement:
          case rdcspv::Op::AtomicIDecrement:
          case rdcspv::Op::AtomicIAdd:
          case rdcspv::Op::AtomicISub:
          case rdcspv::Op::AtomicSMin:
          case rdcspv::Op::AtomicUMin:
          case rdcspv::Op::AtomicSMax:
          case rdcspv::Op::AtomicUMax:
          case rdcspv::Op::AtomicAnd:
          case rdcspv::Op::AtomicOr:
          case rdcspv::Op::AtomicXor:
          {
            rdcspv::IdResultType resultType = rdcspv::IdResultType::fromWord(it.word(1));
            rdcspv::IdResult result = rdcspv::IdResult::fromWord(it.word(2));
            rdcspv::Id pointer = rdcspv::Id::fromWord(it.word(3));
            rdcspv::IdScope memory = rdcspv::IdScope::fromWord(it.word(4));
            rdcspv::IdMemorySemantics semantics = rdcspv::IdMemorySemantics::fromWord(it.word(5));
            editor.Remove(it);
            // All of these instructions produce a result ID that is the original
            // value stored at the pointer. Since we removed the original instruction
            // we replace it with an OpAtomicLoad in case the result ID is used.
            // This is currently best effort and might be incorrect in some cases
            // (for ex. if shader invocations need to see the updated value).
            editor.AddOperation(
                it, rdcspv::OpAtomicLoad(resultType, result, pointer, memory, semantics));
            modified = true;
            break;
          }
          default: break;
        }
      }
    }
    return modified;
  }

  WrappedVulkan *m_pDriver;
  VkShaderModule m_FixedColFS;

  // ShaderKey consists of original shader module ID and entry point name.
  typedef rdcpair<ResourceId, rdcstr> ShaderKey;
  std::map<ShaderKey, VkShaderModule> m_ShaderReplacements;
};

// VulkanPixelHistoryCallback is a generic VulkanDrawcallCallback that can be used for
// pixel history replays.
struct VulkanPixelHistoryCallback : public VulkanDrawcallCallback
{
  VulkanPixelHistoryCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache, uint32_t x,
                             uint32_t y, uint32_t sampleMask, VkQueryPool occlusionPool)
      : m_pDriver(vk),
        m_ShaderCache(shaderCache),
        m_X(x),
        m_Y(y),
        m_SampleMask(sampleMask),
        m_OcclusionPool(occlusionPool)
  {
    m_pDriver->SetDrawcallCB(this);
  }

  ~VulkanPixelHistoryCallback() { m_pDriver->SetDrawcallCB(NULL); }
  // Update the given scissor to just the pixel for which pixel history was requested.
  void ScissorToPixel(const VkViewport &view, VkRect2D &scissor)
  {
    float fx = (float)m_X;
    float fy = (float)m_Y;
    float y_start = view.y;
    float y_end = view.y + view.height;
    if(view.height < 0)
    {
      y_start = view.y + view.height;
      y_end = view.y;
    }

    if(fx < view.x || fy < y_start || fx >= view.x + view.width || fy >= y_end)
    {
      scissor.offset.x = scissor.offset.y = scissor.extent.width = scissor.extent.height = 0;
    }
    else
    {
      scissor.offset.x = m_X;
      scissor.offset.y = m_Y;
      scissor.extent.width = scissor.extent.height = 1;
    }
  }

  // Intersects the originalScissor and newScissor and writes intersection to the newScissor.
  // newScissor always covers a single pixel, so if originalScissor does not touch that pixel
  // returns an empty scissor.
  void IntersectScissors(const VkRect2D &originalScissor, VkRect2D &newScissor)
  {
    RDCASSERT(newScissor.extent.height == 1);
    RDCASSERT(newScissor.extent.width == 1);
    if(originalScissor.offset.x > newScissor.offset.x ||
       originalScissor.offset.x + originalScissor.extent.width <
           newScissor.offset.x + newScissor.extent.width ||
       originalScissor.offset.y > newScissor.offset.y ||
       originalScissor.offset.y + originalScissor.extent.height <
           newScissor.offset.y + newScissor.extent.height)
    {
      // scissor does not touch our target pixel, make it empty
      newScissor.offset.x = newScissor.offset.y = newScissor.extent.width =
          newScissor.extent.height = 0;
    }
  }

protected:
  WrappedVulkan *m_pDriver;
  PixelHistoryShaderCache *m_ShaderCache;
  uint32_t m_X;
  uint32_t m_Y;
  uint32_t m_SampleMask;
  VkQueryPool m_OcclusionPool;
};

struct VulkanOcclusionAndStencilCallback : public VulkanPixelHistoryCallback
{
  VulkanOcclusionAndStencilCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                                    uint32_t x, uint32_t y, VkImage image, VkFormat format,
                                    VkExtent3D extent, uint32_t sampleMask,
                                    VkQueryPool occlusionPool, VkImageView colorImageView,
                                    VkImageView stencilImageView, VkImage stencilImage,
                                    VkBuffer dstBuffer, const rdcarray<EventUsage> &events)
      : VulkanPixelHistoryCallback(vk, shaderCache, x, y, sampleMask, occlusionPool),
        m_Image(image),
        m_Format(format),
        m_Extent(extent),
        m_DstBuffer(dstBuffer),
        m_ColorImageView(colorImageView),
        m_StencilImageView(stencilImageView),
        m_StencilImage(stencilImage),
        m_PrevState(vk, NULL)
  {
    for(size_t i = 0; i < events.size(); i++)
      m_Events.insert(std::make_pair(events[i].eventId, events[i]));
  }

  ~VulkanOcclusionAndStencilCallback() {}
  // CreateResources creates fixed colour shader, offscreen framebuffer and
  // a renderpass for occlusion and stencil tests.
  void CreateResources()
  {
    VkResult vkr;

    VkAttachmentReference colRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference stencilRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub = {};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colRef;
    sub.pDepthStencilAttachment = &stencilRef;

    VkAttachmentDescription attDesc[2] = {};
    attDesc[0].format = m_Format;
    attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attDesc[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attDesc[1] = attDesc[0];
    attDesc[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkRenderPassCreateInfo rpinfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpinfo.attachmentCount = 2;
    rpinfo.pAttachments = attDesc;
    rpinfo.subpassCount = 1;
    rpinfo.pSubpasses = &sub;
    vkr = m_pDriver->vkCreateRenderPass(m_pDriver->GetDev(), &rpinfo, NULL, &m_RenderPass);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageView attachments[2] = {m_ColorImageView, m_StencilImageView};
    VkFramebufferCreateInfo fbinfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbinfo.renderPass = m_RenderPass;
    fbinfo.attachmentCount = 2;
    fbinfo.pAttachments = attachments;
    fbinfo.width = m_Extent.width;
    fbinfo.height = m_Extent.height;
    fbinfo.layers = 1;

    vkr = m_pDriver->vkCreateFramebuffer(m_pDriver->GetDev(), &fbinfo, NULL, &m_OffscreenFB);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  void DestroyResources()
  {
    m_pDriver->vkDestroyFramebuffer(m_pDriver->GetDev(), m_OffscreenFB, NULL);
    m_pDriver->vkDestroyRenderPass(m_pDriver->GetDev(), m_RenderPass, NULL);

    for(auto it = m_PipelineCache.begin(); it != m_PipelineCache.end(); ++it)
    {
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), it->second.fixedShaderStencil, NULL);
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), it->second.originalShaderStencil, NULL);
    }
  }

  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    auto it = m_Events.find(eid);
    if(it == m_Events.end())
      return;
    EventUsage event = it->second;
    m_PrevState = m_pDriver->GetRenderState();

    // TODO: handle secondary command buffers.
    // TODO: can't end renderpass if we are not on the last subpass.
    m_pDriver->GetRenderState().EndRenderPass(cmd);
    // Get pre-modification values
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    VkImage depthImage = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    const DrawcallDescription *draw = m_pDriver->GetDrawcall(eid);
    if(draw && draw->depthOut != ResourceId())
    {
      ResourceId resId = m_pDriver->GetResourceManager()->GetLiveID(draw->depthOut);
      depthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(resId);
      const VulkanCreationInfo::Image &imginfo = m_pDriver->GetDebugManager()->GetImageInfo(resId);
      depthFormat = imginfo.format;
    }

    CopyPixel(m_Image, m_Format, depthImage, depthFormat, cmd, storeOffset);

    VulkanRenderState &pipestate = m_pDriver->GetRenderState();
    ResourceId prevState = pipestate.graphics.pipeline;
    ResourceId prevRenderpass = pipestate.renderPass;
    ResourceId prevFramebuffer = pipestate.GetFramebuffer();
    rdcarray<ResourceId> prevFBattachments = pipestate.GetFramebufferAttachments();
    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetRenderState().m_CreationInfo->m_Pipeline[pipestate.graphics.pipeline];
    uint32_t prevSubpass = pipestate.subpass;

    {
      PipelineReplacements replacements = GetPipelineReplacements(eid, pipestate.graphics.pipeline);

      if(p.dynamicStates[VkDynamicViewport])
        for(uint32_t i = 0; i < pipestate.views.size(); i++)
          ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);

      // Replay the draw with a fixed color shader that never discards, stencil
      // increment to count number of fragments, and an occlusion query around
      // the draw. We will get occlusion data to figure out if anything wrote to
      // the pixel, as well as number of fragments not accounting for potential
      // shader discard.
      pipestate.SetFramebuffer(GetResID(m_OffscreenFB));
      pipestate.renderPass = GetResID(m_RenderPass);
      pipestate.subpass = 0;
      pipestate.graphics.pipeline = GetResID(replacements.fixedShaderStencil);
      ReplayDraw(cmd, m_OcclusionQueries.size(), eid, true, true);

      m_OcclusionQueries.insert(
          std::pair<uint32_t, uint32_t>(eid, (uint32_t)m_OcclusionQueries.size()));

      CopyPixelParams params = {};
      params.multisampled = false;
      params.srcImage = m_StencilImage;
      params.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      params.srcImageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
      params.imageOffset.x = int32_t(m_X);
      params.imageOffset.y = int32_t(m_Y);
      params.imageOffset.z = 0;
      params.dstBuffer = m_DstBuffer;
      params.depthCopy = true;
      params.stencilOnly = true;
      // Copy stencil value that indicates the number of fragments ignoring
      // shader discard.
      m_pDriver->GetDebugManager()->PixelHistoryCopyPixel(
          cmd, params, storeOffset + offsetof(struct EventInfo, dsWithoutShaderDiscard));

      // Replay the draw with the original fragment shader to get the actual number
      // of fragments, accounting for potential shader discard.
      pipestate.graphics.pipeline = GetResID(replacements.originalShaderStencil);
      ReplayDraw(cmd, 0, eid, false, true);

      m_pDriver->GetDebugManager()->PixelHistoryCopyPixel(
          cmd, params, storeOffset + offsetof(struct EventInfo, dsWithShaderDiscard));
    }

    // Restore the state.
    m_pDriver->GetRenderState() = m_PrevState;
    pipestate.SetFramebuffer(prevFramebuffer, prevFBattachments);
    pipestate.renderPass = prevRenderpass;
    pipestate.subpass = prevSubpass;

    // TODO: Need to re-start on the correct subpass.
    if(m_PrevState.graphics.pipeline != ResourceId())
      m_pDriver->GetRenderState().BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return false;

    m_pDriver->GetRenderState().EndRenderPass(cmd);

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    VkImage depthImage = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    const DrawcallDescription *draw = m_pDriver->GetDrawcall(eid);
    if(draw && draw->depthOut != ResourceId())
    {
      ResourceId resId = m_pDriver->GetResourceManager()->GetLiveID(draw->depthOut);
      depthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(resId);
      const VulkanCreationInfo::Image &imginfo = m_pDriver->GetDebugManager()->GetImageInfo(resId);
      depthFormat = imginfo.format;
    }

    CopyPixel(m_Image, m_Format, depthImage, depthFormat, cmd,
              storeOffset + offsetof(struct EventInfo, postmod));

    m_pDriver->GetRenderState().BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

    // Get post-modification values
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  void PreDispatch(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(m_Image, m_Format, VK_NULL_HANDLE, VK_FORMAT_UNDEFINED, cmd, storeOffset);
  }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return false;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(m_Image, m_Format, VK_NULL_HANDLE, VK_FORMAT_UNDEFINED, cmd,
              storeOffset + offsetof(struct EventInfo, postmod));
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { PreDispatch(eid, cmd); }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd)
  {
    return PostDispatch(eid, cmd);
  }

  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // TODO: handle aliased events.
  }

  void FetchOcclusionResults()
  {
    if(m_OcclusionQueries.size() == 0)
      return;

    m_OcclusionResults.resize(m_OcclusionQueries.size());
    VkResult vkr = ObjDisp(m_pDriver->GetDev())
                       ->GetQueryPoolResults(Unwrap(m_pDriver->GetDev()), m_OcclusionPool, 0,
                                             (uint32_t)m_OcclusionResults.size(),
                                             m_OcclusionResults.byteSize(),
                                             m_OcclusionResults.data(), sizeof(uint64_t),
                                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  uint64_t GetOcclusionResult(uint32_t eventId)
  {
    auto it = m_OcclusionQueries.find(eventId);
    RDCASSERT(it != m_OcclusionQueries.end());
    RDCASSERT(it->second < m_OcclusionResults.size());
    return m_OcclusionResults[it->second];
  }

  size_t GetEventIndex(uint32_t eventId)
  {
    auto it = m_EventIndices.find(eventId);
    RDCASSERT(it != m_EventIndices.end());
    return it->second;
  }

private:
  void CopyPixel(VkImage srcImage, VkFormat srcFormat, VkImage depthImage, VkFormat depthFormat,
                 VkCommandBuffer cmd, size_t offset)
  {
    CopyPixelParams colourCopyParams = {};
    colourCopyParams.multisampled = false;    // TODO: multisampled
    colourCopyParams.srcImage = srcImage;
    colourCopyParams.srcImageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;    // TODO: image layout
    colourCopyParams.srcImageFormat = srcFormat;
    colourCopyParams.imageOffset.x = int32_t(m_X);
    colourCopyParams.imageOffset.y = int32_t(m_Y);
    colourCopyParams.imageOffset.z = 0;
    colourCopyParams.dstBuffer = m_DstBuffer;

    m_pDriver->GetDebugManager()->PixelHistoryCopyPixel(cmd, colourCopyParams, offset);

    if(depthImage != VK_NULL_HANDLE)
    {
      CopyPixelParams depthCopyParams = colourCopyParams;
      depthCopyParams.depthCopy = true;
      depthCopyParams.srcImage = depthImage;
      depthCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthCopyParams.srcImageFormat = depthFormat;
      m_pDriver->GetDebugManager()->PixelHistoryCopyPixel(
          cmd, depthCopyParams, offset + offsetof(struct PixelHistoryValue, depth));
    }
  }

  // ReplayDraw begins renderpass, executes a single draw defined by the eventId and
  // ends the renderpass.
  void ReplayDraw(VkCommandBuffer cmd, size_t eventIndex, int eventId, bool doQuery,
                  bool clear = false)
  {
    VulkanRenderState &state = m_pDriver->GetRenderState();
    const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);
    state.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

    if(clear)
    {
      VkClearAttachment clearAttachments[2] = {};
      clearAttachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clearAttachments[0].colorAttachment = 0;
      clearAttachments[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      VkClearRect rect = {};
      rect.rect.offset.x = m_X;
      rect.rect.offset.y = m_Y;
      rect.rect.extent.width = 1;
      rect.rect.extent.height = 1;
      rect.baseArrayLayer = 0;
      rect.layerCount = 1;
      ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 2, clearAttachments, 1, &rect);
    }

    if(doQuery)
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, (uint32_t)eventIndex, 0);

    if(drawcall->flags & DrawFlags::Indexed)
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                   drawcall->indexOffset, drawcall->baseVertex,
                                   drawcall->instanceOffset);
    else
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                            drawcall->vertexOffset, drawcall->instanceOffset);

    if(doQuery)
      ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_OcclusionPool, (uint32_t)eventIndex);

    state.EndRenderPass(cmd);
  }

  // GetPipelineReplacements creates pipeline replacements that disable all tests,
  // and use either fixed or original fragment shader, and shaders that don't
  // have side effects.
  PipelineReplacements GetPipelineReplacements(uint32_t eid, ResourceId pipeline)
  {
    // The map does not keep track of the event ID, event ID is only used to figure out
    // which shaders need to be modified. Those flags are based on the shaders bound,
    // so in theory all events should share those flags if they are using the same
    // pipeline.
    auto pipeIt = m_PipelineCache.find(pipeline);
    if(pipeIt != m_PipelineCache.end())
      return pipeIt->second;

    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetRenderState().m_CreationInfo->m_Pipeline[pipeline];

    EventFlags eventFlags = m_pDriver->GetEventFlags(eid);
    VkShaderModule replacementShaders[5] = {};

    // Clean shaders
    uint32_t numberOfStages = 5;
    for(size_t i = 0; i < numberOfStages; i++)
    {
      if((eventFlags & PipeStageRWEventFlags(StageFromIndex(i))) != EventFlags::NoFlags)
        replacementShaders[i] =
            m_ShaderCache->GetShaderWithoutSideEffects(p.shaders[i].module, p.shaders[i].entryPoint);
    }

    VkGraphicsPipelineCreateInfo pipeCreateInfo = {};
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, pipeline);
    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    VkPipelineMultisampleStateCreateInfo *ms =
        (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;
    VkPipelineViewportStateCreateInfo *vs =
        (VkPipelineViewportStateCreateInfo *)pipeCreateInfo.pViewportState;

    VkRect2D newScissors[16];
    memset(newScissors, 0, sizeof(newScissors));
    {
      // Disable all tests, but stencil.
      rs->cullMode = VK_CULL_MODE_NONE;
      rs->rasterizerDiscardEnable = VK_FALSE;
      ds->depthTestEnable = VK_FALSE;
      ds->depthWriteEnable = VK_FALSE;
      ds->stencilTestEnable = VK_TRUE;
      ds->depthBoundsTestEnable = VK_FALSE;
      ds->front.compareOp = VK_COMPARE_OP_ALWAYS;
      ds->front.failOp = VK_STENCIL_OP_KEEP;
      ds->front.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds->front.compareMask = 0xff;
      ds->front.writeMask = 0xff;
      ds->front.reference = 1;
      ds->back = ds->front;
      ms->pSampleMask = &m_SampleMask;

      if(m_pDriver->GetDeviceFeatures().depthClamp)
        rs->depthClampEnable = true;

      // Change scissors unless they are set dynamically.
      if(!p.dynamicStates[VkDynamicScissor])
      {
        for(uint32_t i = 0; i < vs->viewportCount; i++)
        {
          ScissorToPixel(vs->pViewports[i], newScissors[i]);
        }
        vs->pScissors = newScissors;
      }
    }
    // TODO: Disable discard rectangles from VK_EXT_discard_rectangles.

    pipeCreateInfo.renderPass = m_RenderPass;
    pipeCreateInfo.subpass = 0;

    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    stages.resize(pipeCreateInfo.stageCount);
    memcpy(stages.data(), pipeCreateInfo.pStages, stages.byteSize());

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkShaderModule replacement = replacementShaders[StageIndex(stages[i].stage)];
      if(replacement != VK_NULL_HANDLE)
        stages[i].module = replacement;
    }
    pipeCreateInfo.pStages = stages.data();

    PipelineReplacements replacements = {};
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL,
                                                        &replacements.originalShaderStencil);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      if(stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        stages[i].module = m_ShaderCache->GetFixedColShader();
        stages[i].pName = "main";
        break;
      }
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                               &pipeCreateInfo, NULL,
                                               &replacements.fixedShaderStencil);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_PipelineCache.insert(std::make_pair(pipeline, replacements));

    return replacements;
  }

  VkImage m_Image;
  VkFormat m_Format;
  VkExtent3D m_Extent;
  VkBuffer m_DstBuffer;

  VkImageView m_ColorImageView;
  VkImageView m_StencilImageView;
  VkImage m_StencilImage;

  VkRenderPass m_RenderPass;
  VkFramebuffer m_OffscreenFB;
  std::map<ResourceId, PipelineReplacements> m_PipelineCache;
  std::map<uint32_t, EventUsage> m_Events;
  // Key is event ID, and value is an index of where the event data is stored.
  std::map<uint32_t, size_t> m_EventIndices;
  // Key is event ID, and value is an index of where the occlusion result.
  std::map<uint32_t, uint32_t> m_OcclusionQueries;
  rdcarray<uint64_t> m_OcclusionResults;

  VulkanRenderState m_PrevState;
};

// TestsFailedCallback replays draws to figure out which tests failed (for ex., depth,
// stencil test etc).
struct TestsFailedCallback : public VulkanPixelHistoryCallback
{
  TestsFailedCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache, uint32_t x, uint32_t y,
                      uint32_t sampleMask, VkQueryPool occlusionPool, rdcarray<uint32_t> events)
      : VulkanPixelHistoryCallback(vk, shaderCache, x, y, sampleMask, occlusionPool),
        m_Events(events)
  {
  }

  ~TestsFailedCallback() {}
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;

    VulkanRenderState &pipestate = m_pDriver->GetRenderState();
    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetRenderState().m_CreationInfo->m_Pipeline[pipestate.graphics.pipeline];
    uint32_t eventFlags = CalculateEventFlags(p, pipestate);
    m_EventFlags[eid] = eventFlags;

    // TODO: figure out if the shader has early fragments tests turned on,
    // based on the currently bound fragment shader.
    bool earlyFragmentTests = false;
    m_HasEarlyFragments[eid] = earlyFragmentTests;

    ResourceId curPipeline = pipestate.graphics.pipeline;
    VulkanRenderState m_PrevState = m_pDriver->GetRenderState();

    ReplayDrawWithTests(cmd, eid, eventFlags, curPipeline);

    m_pDriver->GetRenderState() = m_PrevState;
    m_pDriver->GetRenderState().BindPipeline(cmd, VulkanRenderState::BindGraphics, false);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // TODO: handle aliased events.
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  uint32_t GetEventFlags(uint32_t eventId)
  {
    auto it = m_EventFlags.find(eventId);
    if(it == m_EventFlags.end())
      RDCERR("Can't find event flags for event %u", eventId);
    return it->second;
  }

  void FetchOcclusionResults()
  {
    if(m_OcclusionQueries.empty())
      return;
    m_OcclusionResults.resize(m_OcclusionQueries.size());
    VkResult vkr =
        ObjDisp(m_pDriver->GetDev())
            ->GetQueryPoolResults(Unwrap(m_pDriver->GetDev()), m_OcclusionPool, 0,
                                  (uint32_t)m_OcclusionResults.size(), m_OcclusionResults.byteSize(),
                                  m_OcclusionResults.data(), sizeof(m_OcclusionResults[0]),
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  uint64_t GetOcclusionResult(uint32_t eventId, uint32_t test) const
  {
    auto it = m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test));
    if(it == m_OcclusionQueries.end())
      RDCERR("Can't locate occlusion query for event id %u and test flags %u", eventId, test);
    if(it->second >= m_OcclusionResults.size())
      RDCERR("Event %u, occlusion index is %u, and the total # of occlusion query data %zu",
             eventId, it->second, m_OcclusionResults.size());
    return m_OcclusionResults[it->second];
  }

  bool HasEarlyFragments(uint32_t eventId) const
  {
    auto it = m_HasEarlyFragments.find(eventId);
    RDCASSERT(it != m_HasEarlyFragments.end());
    return it->second;
  }

private:
  uint32_t CalculateEventFlags(const VulkanCreationInfo::Pipeline &p,
                               const VulkanRenderState &pipestate)
  {
    uint32_t flags = 0;

    // Culling
    {
      if(p.cullMode != VK_CULL_MODE_NONE)
        flags |= TestEnabled_Culling;

      if(p.cullMode == VK_CULL_MODE_FRONT_AND_BACK)
        flags |= TestMustFail_Culling;
    }

    // Depth and Stencil tests.
    {
      if(p.depthBoundsEnable)
        flags |= TestEnabled_DepthBounds;

      if(p.depthTestEnable)
      {
        if(p.depthCompareOp != VK_COMPARE_OP_ALWAYS)
          flags |= TestEnabled_DepthTesting;
        if(p.depthCompareOp == VK_COMPARE_OP_NEVER)
          flags |= TestMustFail_DepthTesting;
      }

      if(p.stencilTestEnable)
      {
        if(p.front.compareOp != VK_COMPARE_OP_ALWAYS || p.back.compareOp != VK_COMPARE_OP_ALWAYS)
          flags |= TestEnabled_StencilTesting;

        if(p.front.compareOp == VK_COMPARE_OP_NEVER && p.back.compareOp == VK_COMPARE_OP_NEVER)
          flags |= TestMustFail_StencilTesting;
        else if(p.front.compareOp == VK_COMPARE_OP_NEVER && p.cullMode == VK_CULL_MODE_BACK_BIT)
          flags |= TestMustFail_StencilTesting;
        else if(p.cullMode == VK_CULL_MODE_FRONT_BIT && p.back.compareOp == VK_COMPARE_OP_NEVER)
          flags |= TestMustFail_StencilTesting;
      }
    }

    // Scissor
    {
      bool inRegion = false;
      bool inAllRegions = true;
      // Do we even need to know viewerport here?
      const VkRect2D *pScissors;
      uint32_t scissorCount;
      if(p.dynamicStates[VkDynamicScissor])
      {
        pScissors = pipestate.scissors.data();
        scissorCount = (uint32_t)pipestate.scissors.size();
      }
      else
      {
        pScissors = p.scissors.data();
        scissorCount = (uint32_t)p.scissors.size();
      }
      for(uint32_t i = 0; i < scissorCount; i++)
      {
        const VkOffset2D &offset = pScissors[i].offset;
        const VkExtent2D &extent = pScissors[i].extent;
        if((m_X >= (uint32_t)offset.x) && (m_Y >= (uint32_t)offset.y) &&
           (m_X < (offset.x + extent.width)) && (m_Y < (offset.y + extent.height)))
          inRegion = true;
        else
          inAllRegions = false;
      }
      if(!inRegion)
        flags |= TestMustFail_Scissor;
      if(inAllRegions)
        flags |= TestMustPass_Scissor;
    }

    // Blending
    {
      if(m_pDriver->GetDeviceFeatures().independentBlend)
      {
        for(size_t i = 0; i < p.attachments.size(); i++)
        {
          if(p.attachments[i].blendEnable)
          {
            flags |= Blending_Enabled;
            break;
          }
        }
      }
      else
      {
        // Might not have attachments if rasterization is disabled
        if(p.attachments.size() > 0 && p.attachments[0].blendEnable)
          flags |= Blending_Enabled;
      }
    }

    // Samples
    {
      // TODO: figure out if we always need to check this.
      flags |= TestEnabled_SampleMask;

      // compare to ms->pSampleMask
      if((p.sampleMask & m_SampleMask) == 0)
        flags |= TestMustFail_SampleMask;
    }

    // TODO: is shader discard always possible?
    flags |= TestEnabled_FragmentDiscard;
    return flags;
  }

  // Flags to create a pipeline for tests, can be combined to control how
  // a pipeline is created.
  enum
  {
    PipelineCreationFlags_DisableCulling = 1 << 0,
    PipelineCreationFlags_DisableDepthTest = 1 << 1,
    PipelineCreationFlags_DisableStencilTest = 1 << 2,
    PipelineCreationFlags_DisableDepthBoundsTest = 1 << 3,
    PipelineCreationFlags_FixedColorShader = 1 << 4,
    PipelineCreationFlags_IntersectOriginalScissor = 1 << 5,
  };

  void ReplayDrawWithTests(VkCommandBuffer cmd, uint32_t eid, uint32_t eventFlags,
                           ResourceId basePipeline)
  {
    // Backface culling
    if(eventFlags & TestMustFail_Culling)
      return;

    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetRenderState().m_CreationInfo->m_Pipeline[basePipeline];
    EventFlags eventShaderFlags = m_pDriver->GetEventFlags(eid);
    uint32_t numberOfStages = 5;
    rdcarray<VkShaderModule> replacementShaders;
    replacementShaders.resize(numberOfStages);
    // Replace fragment shader because it might have early fragments
    for(size_t i = 0; i < numberOfStages; i++)
    {
      if(p.shaders[i].module == ResourceId())
        continue;
      ShaderStage stage = StageFromIndex(i);
      bool rwInStage = (eventShaderFlags & PipeStageRWEventFlags(stage)) != EventFlags::NoFlags;
      if(rwInStage || (stage == ShaderStage::Fragment))
        replacementShaders[i] =
            m_ShaderCache->GetShaderWithoutSideEffects(p.shaders[i].module, p.shaders[i].entryPoint);
    }

    bool dynamicScissor = p.dynamicStates[VkDynamicScissor];
    VulkanRenderState &pipestate = m_pDriver->GetRenderState();
    rdcarray<VkRect2D> prevScissors = pipestate.scissors;
    if(dynamicScissor)
      for(uint32_t i = 0; i < pipestate.views.size(); i++)
        ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);

    if(eventFlags & TestEnabled_Culling)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_DisableDepthBoundsTest |
          PipelineCreationFlags_DisableStencilTest | PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      ReplayDraw(cmd, pipe, eid, TestEnabled_Culling);
    }

    // Scissor
    if(eventFlags & TestMustFail_Scissor)
      return;

    if((eventFlags & (TestEnabled_Scissor | TestMustPass_Scissor)) == TestEnabled_Scissor)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_IntersectOriginalScissor | PipelineCreationFlags_DisableDepthTest |
          PipelineCreationFlags_DisableDepthBoundsTest | PipelineCreationFlags_DisableStencilTest |
          PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      // This will change the dynamic scissor state for the later tests, but since those
      // tests happen later in the pipeline, it does not matter.
      if(dynamicScissor)
        for(uint32_t i = 0; i < pipestate.views.size(); i++)
          IntersectScissors(prevScissors[i], pipestate.scissors[i]);
      ReplayDraw(cmd, pipe, eid, TestEnabled_Scissor);
    }

    // Sample mask
    if(eventFlags & TestMustFail_SampleMask)
      return;

    if(eventFlags & TestEnabled_SampleMask)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthBoundsTest | PipelineCreationFlags_DisableStencilTest |
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      ReplayDraw(cmd, pipe, eid, TestEnabled_SampleMask);
    }

    // Depth bounds
    if(eventFlags & TestEnabled_DepthBounds)
    {
      uint32_t pipeFlags = PipelineCreationFlags_DisableStencilTest |
                           PipelineCreationFlags_DisableDepthTest |
                           PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      ReplayDraw(cmd, pipe, eid, TestEnabled_DepthBounds);
    }

    // Stencil test
    if(eventFlags & TestMustFail_StencilTesting)
      return;

    if(eventFlags & TestEnabled_StencilTesting)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      ReplayDraw(cmd, pipe, eid, TestEnabled_StencilTesting);
    }

    // Depth test
    if(eventFlags & TestMustFail_DepthTesting)
      return;

    if(eventFlags & TestEnabled_DepthTesting)
    {
      uint32_t pipeFlags = PipelineCreationFlags_FixedColorShader;

      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      ReplayDraw(cmd, pipe, eid, TestEnabled_DepthTesting);
    }

    // Shader discard
    if(eventFlags & TestEnabled_FragmentDiscard)
    {
      // With early fragment tests, sample counting (occlusion query) will be done before the shader
      // executes.
      // TODO: remove early fragment tests if it is ON.
      uint32_t pipeFlags = PipelineCreationFlags_DisableDepthBoundsTest |
                           PipelineCreationFlags_DisableStencilTest |
                           PipelineCreationFlags_DisableDepthTest;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, dynamicScissor, replacementShaders);
      ReplayDraw(cmd, pipe, eid, TestEnabled_FragmentDiscard);
    }
  }

  // Creates a pipeline that is based on the given pipeline and the given
  // pipeline flags. Modifies the base pipeline according to the flags, and
  // leaves the original pipeline behavior if a flag is not set.
  VkPipeline CreatePipeline(ResourceId basePipeline, uint32_t pipeCreateFlags, bool dynamicScissor,
                            const rdcarray<VkShaderModule> &replacementShaders)
  {
    rdcpair<ResourceId, uint32_t> pipeKey(basePipeline, pipeCreateFlags);
    auto it = m_PipeCache.find(pipeKey);
    // Check if we processed this pipeline before.
    if(it != m_PipeCache.end())
      return it->second;

    VkGraphicsPipelineCreateInfo ci = {};
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(ci, basePipeline);
    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)ci.pRasterizationState;
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)ci.pDepthStencilState;
    VkPipelineViewportStateCreateInfo *vs = (VkPipelineViewportStateCreateInfo *)ci.pViewportState;
    VkPipelineMultisampleStateCreateInfo *ms =
        (VkPipelineMultisampleStateCreateInfo *)ci.pMultisampleState;

    // Only interested in a single sample.
    ms->pSampleMask = &m_SampleMask;
    // We are going to replay a draw multiple times, don't want to modify the
    // depth value, not to influence later tests.
    ds->depthWriteEnable = VK_FALSE;

    if(pipeCreateFlags & PipelineCreationFlags_DisableCulling)
      rs->cullMode = VK_CULL_MODE_NONE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthTest)
      ds->depthTestEnable = VK_FALSE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableStencilTest)
      ds->stencilTestEnable = VK_FALSE;
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthBoundsTest)
      ds->depthBoundsTestEnable = VK_FALSE;

    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    stages.resize(ci.stageCount);
    memcpy(stages.data(), ci.pStages, stages.byteSize());

    for(size_t i = 0; i < ci.stageCount; i++)
    {
      if((ci.pStages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT) &&
         (pipeCreateFlags & PipelineCreationFlags_FixedColorShader))
      {
        stages[i].module = m_ShaderCache->GetFixedColShader();
        stages[i].pName = "main";
      }
      else if(replacementShaders[StageIndex(stages[i].stage)] != VK_NULL_HANDLE)
      {
        stages[i].module = replacementShaders[StageIndex(stages[i].stage)];
      }
    }
    ci.pStages = stages.data();

    if(!dynamicScissor)
    {
      VkRect2D *pScissors = (VkRect2D *)vs->pScissors;
      for(uint32_t i = 0; i < vs->viewportCount; i++)
      {
        ScissorToPixel(vs->pViewports[i], pScissors[i]);
        if(pipeCreateFlags & PipelineCreationFlags_IntersectOriginalScissor)
          IntersectScissors(vs->pScissors[i], pScissors[i]);
      }
    }

    VkPipeline pipe;
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1, &ci,
                                                        NULL, &pipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_PipeCache.insert(std::make_pair(pipeKey, pipe));
    return pipe;
  }

  void ReplayDraw(VkCommandBuffer cmd, VkPipeline pipe, int eventId, uint32_t test)
  {
    m_pDriver->GetRenderState().graphics.pipeline = GetResID(pipe);
    m_pDriver->GetRenderState().BindPipeline(cmd, VulkanRenderState::BindGraphics, false);

    uint32_t index = (uint32_t)m_OcclusionQueries.size();
    if(m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test)) != m_OcclusionQueries.end())
      RDCERR("A query already exist for event id %u and test %u", eventId, test);
    m_OcclusionQueries.insert(std::make_pair(rdcpair<uint32_t, uint32_t>(eventId, test), index));

    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, index, 0);

    const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);
    if(drawcall->flags & DrawFlags::Indexed)
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                   drawcall->indexOffset, drawcall->baseVertex,
                                   drawcall->instanceOffset);
    else
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                            drawcall->vertexOffset, drawcall->instanceOffset);

    ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_OcclusionPool, index);
  }

  rdcarray<uint32_t> m_Events;
  // Key is event ID, value is the flags for that event.
  std::map<uint32_t, uint32_t> m_EventFlags;
  // Key is a pair <Base pipeline, pipeline flags>
  std::map<rdcpair<ResourceId, uint32_t>, VkPipeline> m_PipeCache;
  // Key: pair <event ID, test>
  // value: the index where occlusion query is in m_OcclusionResults
  std::map<rdcpair<uint32_t, uint32_t>, uint32_t> m_OcclusionQueries;
  std::map<uint32_t, bool> m_HasEarlyFragments;
  rdcarray<uint64_t> m_OcclusionResults;
};

bool VulkanDebugManager::PixelHistorySetupResources(PixelHistoryResources &resources,
                                                    VkExtent3D extent, VkFormat format,
                                                    uint32_t numEvents)
{
  VkImage colorImage;
  VkImageView colorImageView;
  VkImage stencilImage;
  VkImageView stencilImageView;
  VkDeviceMemory gpuMem;

  VkBuffer dstBuffer;
  VkDeviceMemory bufferMemory;

  VkResult vkr;
  VkDevice dev = m_pDriver->GetDev();

  // Create Images
  VkImageCreateInfo imgInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imgInfo.imageType = VK_IMAGE_TYPE_2D;
  imgInfo.mipLevels = 1;
  imgInfo.arrayLayers = 1;
  imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // Device local resources:
  imgInfo.format = format;
  imgInfo.extent.width = extent.width;
  imgInfo.extent.height = extent.height;
  imgInfo.extent.depth = 1;
  imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &colorImage);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImage wrappedColorImage = colorImage;
  m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), wrappedColorImage);
  ImageState colorImageState = ImageState(wrappedColorImage, ImageInfo(imgInfo), eFrameRef_None);

  VkMemoryRequirements colorImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, colorImage, &colorImageMrq);

  imgInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
  imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &stencilImage);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImage wrappedStencilImage = stencilImage;
  m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), wrappedStencilImage);
  ImageState stencilImageState = ImageState(wrappedStencilImage, ImageInfo(imgInfo), eFrameRef_None);

  VkMemoryRequirements stencilImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, stencilImage, &stencilImageMrq);
  VkDeviceSize offset = AlignUp(colorImageMrq.size, stencilImageMrq.alignment);

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, offset + stencilImageMrq.size,
      m_pDriver->GetGPULocalMemoryIndex(colorImageMrq.memoryTypeBits),
  };
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &gpuMem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindImageMemory(m_Device, colorImage, gpuMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindImageMemory(m_Device, stencilImage, gpuMem, offset);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = colorImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &colorImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = stencilImage;
  viewInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &stencilImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = AlignUp((uint32_t)(numEvents * sizeof(EventInfo)), 512U);
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  vkr = m_pDriver->vkCreateBuffer(m_Device, &bufferInfo, NULL, &dstBuffer);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // Allocate memory
  VkMemoryRequirements mrq = {};
  m_pDriver->vkGetBufferMemoryRequirements(m_Device, dstBuffer, &mrq);
  allocInfo.allocationSize = mrq.size;
  allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &bufferMemory);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindBufferMemory(m_Device, dstBuffer, bufferMemory, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  colorImageState.InlineTransition(
      cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());
  stencilImageState.InlineTransition(
      cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  resources.colorImage = colorImage;
  resources.colorImageView = colorImageView;
  resources.stencilImage = stencilImage;
  resources.stencilImageView = stencilImageView;
  resources.gpuMem = gpuMem;

  resources.bufferMemory = bufferMemory;
  resources.dstBuffer = dstBuffer;

  return true;
}

bool VulkanDebugManager::PixelHistoryDestroyResources(const PixelHistoryResources &r)
{
  VkDevice dev = m_pDriver->GetDev();
  if(r.gpuMem != VK_NULL_HANDLE)
    m_pDriver->vkFreeMemory(dev, r.gpuMem, NULL);
  if(r.colorImage != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImage(dev, r.colorImage, NULL);
  if(r.colorImageView != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImageView(dev, r.colorImageView, NULL);
  if(r.stencilImage != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImage(dev, r.stencilImage, NULL);
  if(r.stencilImageView != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImageView(dev, r.stencilImageView, NULL);
  if(r.dstBuffer != VK_NULL_HANDLE)
    m_pDriver->vkDestroyBuffer(dev, r.dstBuffer, NULL);
  if(r.bufferMemory != VK_NULL_HANDLE)
    m_pDriver->vkFreeMemory(dev, r.bufferMemory, NULL);
  return true;
}

void VulkanDebugManager::PixelHistoryCopyPixel(VkCommandBuffer cmd, CopyPixelParams &p, size_t offset)
{
  rdcarray<VkBufferImageCopy> regions;
  // Check if depth image includes depth and stencil
  VkImageAspectFlags aspectFlags = 0;
  VkBufferImageCopy region = {};
  region.bufferOffset = (uint64_t)offset;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageOffset = p.imageOffset;
  region.imageExtent.width = 1U;
  region.imageExtent.height = 1U;
  region.imageExtent.depth = 1U;
  if(!p.depthCopy)
  {
    region.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    regions.push_back(region);
    aspectFlags = VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
  }
  else if(p.stencilOnly)
  {
    region.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_STENCIL_BIT, 0, 0, 1};
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    regions.push_back(region);
  }
  else
  {
    region.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
    if(IsDepthOnlyFormat(p.srcImageFormat) || IsDepthAndStencilFormat(p.srcImageFormat))
    {
      regions.push_back(region);
      aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if(IsStencilFormat(p.srcImageFormat))
    {
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      region.bufferOffset = offset + 4;
      regions.push_back(region);
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  }

  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                  NULL,
                                  VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                  VK_ACCESS_TRANSFER_READ_BIT,
                                  p.srcImageLayout,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_QUEUE_FAMILY_IGNORED,
                                  VK_QUEUE_FAMILY_IGNORED,
                                  Unwrap(p.srcImage),
                                  {aspectFlags, 0, 1, 0, 1}};

  DoPipelineBarrier(cmd, 1, &barrier);

  ObjDisp(cmd)->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(p.srcImage),
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(p.dstBuffer),
                                     (uint32_t)regions.size(), regions.data());

  barrier.image = Unwrap(p.srcImage);
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.newLayout = p.srcImageLayout;
  DoPipelineBarrier(cmd, 1, &barrier);
}

void CreateOcclusionPool(WrappedVulkan *vk, uint32_t poolSize, VkQueryPool *pQueryPool)
{
  VkDevice dev = vk->GetDev();
  VkQueryPoolCreateInfo occlusionPoolCreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  occlusionPoolCreateInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
  occlusionPoolCreateInfo.queryCount = poolSize;
  // TODO: check that occlusion feature is available
  VkResult vkr =
      ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &occlusionPoolCreateInfo, NULL, pQueryPool);
  VkCommandBuffer cmd = vk->GetNextCmd();
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
  ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), *pQueryPool, 0, poolSize);
  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
  vk->SubmitCmds();
  vk->FlushQ();
}

VkImageLayout VulkanDebugManager::GetImageLayout(ResourceId image, VkImageAspectFlagBits aspect,
                                                 uint32_t mip, uint32_t slice)
{
  auto state = m_pDriver->FindConstImageState(image);
  if(!state)
  {
    RDCERR("Could not find image state for %s", ToStr(image).c_str());
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
  if(state->GetImageInfo().extent.depth > 1)
    return state->GetImageLayout(aspect, mip, 0);
  else
    return state->GetImageLayout(aspect, mip, slice);
}

void UpdateTestsFailed(const TestsFailedCallback *tfCb, uint32_t eventId, uint32_t eventFlags,
                       PixelModification &mod)
{
  bool earlyFragmentTests = tfCb->HasEarlyFragments(eventId);

  if((eventFlags & (TestEnabled_Culling | TestMustFail_Culling)) == TestEnabled_Culling)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_Culling);
    mod.backfaceCulled = (occlData == 0);
  }

  if(mod.backfaceCulled)
    return;

  if((eventFlags & (TestEnabled_Scissor | TestMustPass_Scissor | TestMustFail_Scissor)) ==
     TestEnabled_Scissor)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_Scissor);
    mod.scissorClipped = (occlData == 0);
  }
  if(mod.scissorClipped)
    return;

  // TODO: Exclusive Scissor Test if NV extension is turned on.

  if((eventFlags & (TestEnabled_SampleMask | TestMustFail_SampleMask)) == TestEnabled_SampleMask)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_SampleMask);
    mod.sampleMasked = (occlData == 0);
  }
  if(mod.sampleMasked)
    return;

  // Shader discard with default fragment tests order.
  if(!earlyFragmentTests)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_FragmentDiscard);
    mod.shaderDiscarded = (occlData == 0);
    if(mod.shaderDiscarded)
      return;
  }

  if(eventFlags & TestEnabled_DepthBounds)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthBounds);
    mod.depthClipped = (occlData == 0);
  }
  if(mod.depthClipped)
    return;

  if((eventFlags & (TestEnabled_StencilTesting | TestMustFail_StencilTesting)) ==
     TestEnabled_StencilTesting)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_StencilTesting);
    mod.stencilTestFailed = (occlData == 0);
  }
  if(mod.stencilTestFailed)
    return;

  if((eventFlags & (TestEnabled_DepthTesting | TestMustFail_DepthTesting)) == TestEnabled_DepthTesting)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthTesting);
    mod.depthTestFailed = (occlData == 0);
  }
  if(mod.depthTestFailed)
    return;

  // Shader discard with early fragment tests order.
  if(earlyFragmentTests)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_FragmentDiscard);
    mod.shaderDiscarded = (occlData == 0);
  }
}

rdcarray<PixelModification> VulkanReplay::PixelHistory(rdcarray<EventUsage> events,
                                                       ResourceId target, uint32_t x, uint32_t y,
                                                       const Subresource &sub, CompType typeCast)
{
#if 1
  if(!events.empty())
  {
    VULKANNOTIMP("PixelHistory");
    return rdcarray<PixelModification>();
  }
#endif

  RDCDEBUG("PixelHistory: pixel: (%u, %u) with %u events", x, y, events.size());
  rdcarray<PixelModification> history;

  if(events.empty())
    return history;

  const VulkanCreationInfo::Image &imginfo = GetDebugManager()->GetImageInfo(target);
  if(imginfo.format == VK_FORMAT_UNDEFINED)
    return history;

  uint32_t mip = sub.mip;
  uint32_t slice = sub.slice;
  uint32_t sampleIdx = sub.sample;

  // TODO: figure out correct aspect.
  VkImageLayout imgLayout =
      GetDebugManager()->GetImageLayout(target, VK_IMAGE_ASPECT_COLOR_BIT, mip, slice);
  RDCASSERTNOTEQUAL(imgLayout, VK_IMAGE_LAYOUT_UNDEFINED);

  // TODO: use the given type hint for typeless textures
  SCOPED_TIMER("VkDebugManager::PixelHistory");

  if(sampleIdx > (uint32_t)imginfo.samples)
    sampleIdx = 0;

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
    sampleMask = 1U << sampleIdx;

  bool multisampled = (imginfo.samples > 1);

  if(sampleIdx == ~0U || !multisampled)
    sampleIdx = 0;

  VkDevice dev = m_pDriver->GetDev();
  VkQueryPool occlusionPool;
  CreateOcclusionPool(m_pDriver, (uint32_t)events.size(), &occlusionPool);

  PixelHistoryResources resources = {};
  GetDebugManager()->PixelHistorySetupResources(resources, imginfo.extent, imginfo.format,
                                                (uint32_t)events.size());

  PixelHistoryShaderCache *shaderCache = new PixelHistoryShaderCache(m_pDriver);

  VulkanOcclusionAndStencilCallback cb(
      m_pDriver, shaderCache, x, y, GetResourceManager()->GetCurrentHandle<VkImage>(target),
      imginfo.format, imginfo.extent, sampleMask, occlusionPool, resources.colorImageView,
      resources.stencilImageView, resources.stencilImage, resources.dstBuffer, events);
  cb.CreateResources();
  m_pDriver->ReplayLog(0, events.back().eventId, eReplay_Full);
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();
  cb.DestroyResources();

  cb.FetchOcclusionResults();

  // Gather all draw events that could have written to pixel for another replay pass,
  // to determine if these draws failed for some reason (for ex., depth test).
  rdcarray<uint32_t> drawEvents;
  for(size_t ev = 0; ev < events.size(); ev++)
  {
    bool clear = (events[ev].usage == ResourceUsage::Clear);
    bool directWrite = isDirectWrite(events[ev].usage);

    if(events[ev].view != ResourceId())
    {
      // TODO
    }

    if(!directWrite && !clear)
    {
      uint64_t occlData = cb.GetOcclusionResult((uint32_t)events[ev].eventId);
      if(occlData > 0)
        drawEvents.push_back(events[ev].eventId);
    }
  }

  // If there are any draw events, do another replay pass, in order to figure out
  // which tests failed for each draw event.
  TestsFailedCallback *tfCb = NULL;
  if(drawEvents.size() > 0)
  {
    VkQueryPool tfOcclusionPool;
    CreateOcclusionPool(m_pDriver, (uint32_t)drawEvents.size() * 6, &tfOcclusionPool);

    tfCb = new TestsFailedCallback(m_pDriver, shaderCache, x, y, sampleMask, tfOcclusionPool,
                                   drawEvents);
    m_pDriver->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
    tfCb->FetchOcclusionResults();
  }

  for(size_t ev = 0; ev < events.size(); ev++)
  {
    uint32_t eventId = events[ev].eventId;
    bool clear = (events[ev].usage == ResourceUsage::Clear);
    bool directWrite = isDirectWrite(events[ev].usage);
    if(drawEvents.contains(events[ev].eventId) || clear || directWrite)
    {
      PixelModification mod;
      RDCEraseEl(mod);

      mod.eventId = eventId;
      mod.directShaderWrite = directWrite;
      mod.unboundPS = false;

      if(!clear && !directWrite)
      {
        RDCASSERT(tfCb != NULL);
        uint32_t flags = tfCb->GetEventFlags(eventId);
        if(flags & TestMustFail_Culling)
          mod.backfaceCulled = true;
        if(flags & TestMustFail_DepthTesting)
          mod.depthTestFailed = true;
        if(flags & TestMustFail_Scissor)
          mod.scissorClipped = true;
        if(flags & TestMustFail_SampleMask)
          mod.sampleMasked = true;

        UpdateTestsFailed(tfCb, eventId, flags, mod);
      }
      history.push_back(mod);
    }
  }

  // Try to read memory back

  void *bufPtr = NULL;
  VkResult vkr =
      m_pDriver->vkMapMemory(dev, resources.bufferMemory, 0, VK_WHOLE_SIZE, 0, (void **)&bufPtr);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  EventInfo *eventsInfo = (EventInfo *)bufPtr;

  for(size_t h = 0; h < history.size(); h++)
  {
    PixelModification &mod = history[h];

    ModificationValue preMod, postMod, shadout;
    const EventInfo &ei = eventsInfo[cb.GetEventIndex(mod.eventId)];
    preMod.col.floatValue[0] = (float)(ei.premod.color[2] / 255.0);
    preMod.col.floatValue[1] = (float)(ei.premod.color[1] / 255.0);
    preMod.col.floatValue[2] = (float)(ei.premod.color[0] / 255.0);
    preMod.col.floatValue[3] = (float)(ei.premod.color[3] / 255.0);
    postMod.col.floatValue[0] = (float)(ei.postmod.color[2] / 255.0);
    postMod.col.floatValue[1] = (float)(ei.postmod.color[1] / 255.0);
    postMod.col.floatValue[2] = (float)(ei.postmod.color[0] / 255.0);
    postMod.col.floatValue[3] = (float)(ei.shadout.color[3] / 255.0);
    shadout.col.floatValue[0] = (float)(ei.shadout.color[2] / 255.0);
    shadout.col.floatValue[1] = (float)(ei.shadout.color[1] / 255.0);
    shadout.col.floatValue[2] = (float)(ei.shadout.color[0] / 255.0);
    preMod.depth = ei.premod.depth.fdepth;
    preMod.stencil = ei.premod.stencil;
    postMod.depth = ei.postmod.depth.fdepth;
    postMod.stencil = ei.postmod.stencil;

    mod.preMod = preMod;
    mod.shaderOut = shadout;
    mod.postMod = postMod;

    RDCDEBUG("PixelHistory event id: %u, stencilValue = %u", mod.eventId,
             ei.dsWithoutShaderDiscard[0]);
  }

  m_pDriver->vkUnmapMemory(dev, resources.bufferMemory);
  GetDebugManager()->PixelHistoryDestroyResources(resources);
  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), occlusionPool, NULL);
  delete shaderCache;

  return history;
}
