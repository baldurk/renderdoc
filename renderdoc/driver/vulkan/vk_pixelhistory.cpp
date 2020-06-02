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
#include "maths/formatpacking.h"
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
  UnboundFragmentShader = 1 << 8,
  TestMustFail_Culling = 1 << 9,
  TestMustFail_Scissor = 1 << 10,
  TestMustPass_Scissor = 1 << 11,
  TestMustFail_DepthTesting = 1 << 12,
  TestMustFail_StencilTesting = 1 << 13,
  TestMustFail_SampleMask = 1 << 14,
};

struct CopyPixelParams
{
  VkImage srcImage;
  VkFormat srcImageFormat;
  VkImageLayout srcImageLayout;
};

struct PixelHistoryResources
{
  VkBuffer dstBuffer;
  VkDeviceMemory bufferMemory;

  // Used for offscreen rendering for draw call events.
  VkImage colorImage;
  VkImageView colorImageView;
  VkFormat dsFormat;
  VkImage dsImage;
  VkImageView dsImageView;
  VkDeviceMemory gpuMem;
};

struct PixelHistoryCallbackInfo
{
  // Original image for which pixel history is requested.
  VkImage targetImage;
  // Information about the original target image.
  VkFormat targetImageFormat;
  uint32_t layers;
  uint32_t mipLevels;
  VkSampleCountFlagBits samples;
  VkExtent3D extent;
  // Information about the location of the pixel for which history was requested.
  Subresource targetSubresource;
  uint32_t x;
  uint32_t y;
  uint32_t sampleMask;

  // Image used to get per fragment data.
  VkImage subImage;
  VkImageView subImageView;

  // Image used to get stencil counts.
  VkFormat dsFormat;
  VkImage dsImage;
  VkImageView dsImageView;

  // Buffer used to copy colour and depth information
  VkBuffer dstBuffer;
};

struct PixelHistoryValue
{
  // Max size is 4 component with 8 byte component width
  uint8_t color[32];
  union
  {
    uint32_t udepth;
    float fdepth;
  } depth;
  int8_t stencil;
  uint8_t padding[3 + 8];
};

struct EventInfo
{
  PixelHistoryValue premod;
  PixelHistoryValue postmod;
  uint8_t dsWithoutShaderDiscard[8];
  uint8_t padding[8];
  uint8_t dsWithShaderDiscard[8];
  uint8_t padding1[8];
};

struct PerFragmentInfo
{
  // primitive ID is copied from a R32G32B32A32 texture.
  int32_t primitiveID;
  uint32_t padding[3];
  PixelHistoryValue shaderOut;
  PixelHistoryValue postMod;
};

struct PipelineReplacements
{
  VkPipeline fixedShaderStencil;
  VkPipeline originalShaderStencil;
};

// PixelHistoryShaderCache manages temporary shaders created for pixel history.
struct PixelHistoryShaderCache
{
  PixelHistoryShaderCache(WrappedVulkan *vk) : m_pDriver(vk) {}
  ~PixelHistoryShaderCache()
  {
    for(auto it = m_ShaderReplacements.begin(); it != m_ShaderReplacements.end(); ++it)
    {
      if(it->second != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderModule(m_pDriver->GetDev(), it->second, NULL);
    }
    for(auto it = m_FixedColFS.begin(); it != m_FixedColFS.end(); it++)
      m_pDriver->vkDestroyShaderModule(m_pDriver->GetDev(), it->second, NULL);
    for(auto it = m_PrimIDFS.begin(); it != m_PrimIDFS.end(); it++)
      m_pDriver->vkDestroyShaderModule(m_pDriver->GetDev(), it->second, NULL);
  }

  // Returns a fragment shader module that outputs a fixed color to the given
  // color attachment.
  VkShaderModule GetFixedColShader(uint32_t framebufferIndex)
  {
    auto it = m_FixedColFS.find(framebufferIndex);
    if(it != m_FixedColFS.end())
      return it->second;
    VkShaderModule sh;
    m_pDriver->GetDebugManager()->PatchOutputLocation(sh, BuiltinShader::FixedColFS,
                                                      framebufferIndex);
    m_FixedColFS.insert(std::make_pair(framebufferIndex, sh));
    return sh;
  }

  // Returns a fragment shader module that outputs primitive ID to the given
  // color attachment.
  VkShaderModule GetPrimitiveIdShader(uint32_t framebufferIndex)
  {
    auto it = m_PrimIDFS.find(framebufferIndex);
    if(it != m_PrimIDFS.end())
      return it->second;
    VkShaderModule sh;
    m_pDriver->GetDebugManager()->PatchOutputLocation(sh, BuiltinShader::PixelHistoryPrimIDFS,
                                                      framebufferIndex);
    m_PrimIDFS.insert(std::make_pair(framebufferIndex, sh));
    return sh;
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
        m_pDriver->GetDebugManager()->GetShaderInfo(shaderId);
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
  std::map<uint32_t, VkShaderModule> m_FixedColFS;
  std::map<uint32_t, VkShaderModule> m_PrimIDFS;

  // ShaderKey consists of original shader module ID and entry point name.
  typedef rdcpair<ResourceId, rdcstr> ShaderKey;
  std::map<ShaderKey, VkShaderModule> m_ShaderReplacements;
};

// VulkanPixelHistoryCallback is a generic VulkanDrawcallCallback that can be used for
// pixel history replays.
struct VulkanPixelHistoryCallback : public VulkanDrawcallCallback
{
  VulkanPixelHistoryCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                             const PixelHistoryCallbackInfo &callbackInfo, VkQueryPool occlusionPool)
      : m_pDriver(vk),
        m_ShaderCache(shaderCache),
        m_CallbackInfo(callbackInfo),
        m_OcclusionPool(occlusionPool)
  {
    m_pDriver->SetDrawcallCB(this);
  }

  virtual ~VulkanPixelHistoryCallback()
  {
    m_pDriver->SetDrawcallCB(NULL);
    for(const VkRenderPass &rp : m_RpsToDestroy)
      m_pDriver->vkDestroyRenderPass(m_pDriver->GetDev(), rp, NULL);
    for(const VkFramebuffer &fb : m_FbsToDestroy)
      m_pDriver->vkDestroyFramebuffer(m_pDriver->GetDev(), fb, NULL);
    for(const VkImageView &imageView : m_ImageViewsToDestroy)
      m_pDriver->vkDestroyImageView(m_pDriver->GetDev(), imageView, NULL);
    m_pDriver->GetReplay()->ResetPixelHistoryDescriptorPool();
  }
  // Update the given scissor to just the pixel for which pixel history was requested.
  void ScissorToPixel(const VkViewport &view, VkRect2D &scissor)
  {
    float fx = (float)m_CallbackInfo.x;
    float fy = (float)m_CallbackInfo.y;
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
      scissor.offset.x = m_CallbackInfo.x;
      scissor.offset.y = m_CallbackInfo.y;
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
  // MakeIncrementStencilPipelineCI fills in the provided pipeCreateInfo
  // to create a graphics pipeline that is based on the original. The modifications
  // to the original pipeline: disables depth test and write, stencil is set
  // to always pass and increment, scissor is set to scissor around target pixel,
  // all shaders are replaced with their "clean" versions (attempts to remove side
  // effects), all color modifications are disabled.
  // Optionally disables other tests like culling, depth bounds.
  void MakeIncrementStencilPipelineCI(uint32_t eid, ResourceId pipe,
                                      VkGraphicsPipelineCreateInfo &pipeCreateInfo,
                                      rdcarray<VkPipelineShaderStageCreateInfo> &stages,
                                      bool disableTests)
  {
    const VulkanCreationInfo::Pipeline &p = m_pDriver->GetDebugManager()->GetPipelineInfo(pipe);
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, pipe);

    // make state we want to control dynamic
    AddDynamicStates(pipeCreateInfo);

    VkPipelineRasterizationStateCreateInfo *rs =
        (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    VkPipelineMultisampleStateCreateInfo *ms =
        (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;

    // TODO: should leave in the original state.
    ds->depthTestEnable = VK_FALSE;
    ds->depthWriteEnable = VK_FALSE;
    ds->depthBoundsTestEnable = VK_FALSE;

    if(disableTests)
    {
      rs->cullMode = VK_CULL_MODE_NONE;
      rs->rasterizerDiscardEnable = VK_FALSE;
      if(m_pDriver->GetDeviceEnabledFeatures().depthClamp)
        rs->depthClampEnable = true;
    }

    // Set up the stencil state.
    {
      ds->stencilTestEnable = VK_TRUE;
      ds->front.compareOp = VK_COMPARE_OP_ALWAYS;
      ds->front.failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.compareMask = 0xff;
      ds->front.writeMask = 0xff;
      ds->front.reference = 0;
      ds->back = ds->front;
    }

    // Narrow on the specific pixel and sample.
    {
      ms->pSampleMask = &m_CallbackInfo.sampleMask;
    }

    // Turn off all color modifications.
    {
      VkPipelineColorBlendStateCreateInfo *cbs =
          (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
      VkPipelineColorBlendAttachmentState *atts =
          (VkPipelineColorBlendAttachmentState *)cbs->pAttachments;
      for(uint32_t i = 0; i < cbs->attachmentCount; i++)
        atts[i].colorWriteMask = 0;
    }

    stages.resize(pipeCreateInfo.stageCount);
    memcpy(stages.data(), pipeCreateInfo.pStages, stages.byteSize());

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
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkShaderModule replacement = replacementShaders[StageIndex(stages[i].stage)];
      if(replacement != VK_NULL_HANDLE)
        stages[i].module = replacement;
    }
    pipeCreateInfo.pStages = stages.data();
  }

  // ensures the state we want to control is dynamic, whether or not it was dynamic in the original
  // pipeline
  void AddDynamicStates(VkGraphicsPipelineCreateInfo &pipeCreateInfo)
  {
    // we need to add it. Check if the dynamic state is already pointing to our internal array (in
    // the case of adding multiple dynamic states in a row), and otherwise initialise our array from
    // it and repoint. Then we can add the new state
    VkPipelineDynamicStateCreateInfo *dynState =
        (VkPipelineDynamicStateCreateInfo *)pipeCreateInfo.pDynamicState;

    // copy over the original dynamic states
    m_DynamicStates.assign(dynState->pDynamicStates, dynState->dynamicStateCount);

    // add the ones we want
    if(!m_DynamicStates.contains(VK_DYNAMIC_STATE_SCISSOR))
      m_DynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    if(!m_DynamicStates.contains(VK_DYNAMIC_STATE_STENCIL_REFERENCE))
      m_DynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    if(!m_DynamicStates.contains(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK))
      m_DynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
    if(!m_DynamicStates.contains(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK))
      m_DynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);

    // now point at our storage for the array
    dynState->pDynamicStates = m_DynamicStates.data();
    dynState->dynamicStateCount = (uint32_t)m_DynamicStates.size();
  }

  // CreateRenderPass creates a new VkRenderPass based on the original that has a separate
  // depth-stencil attachment, and covers a single subpass. This will be used to replay
  // a single draw. The new renderpass also replaces the depth stencil attachment, so
  // it can be used to count the number of fragments. Optionally, the new renderpass
  // changes the format for the color image that corresponds to colorIdx attachment.
  VkRenderPass CreateRenderPass(ResourceId rp, VkFormat newColorFormat = VK_FORMAT_UNDEFINED,
                                uint32_t colorIdx = 0)
  {
    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(rp);
    // Currently only single subpass render passes are supported.
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();

    // Copy color and input attachments, and ignore resolve attachments.
    // Since we are only using this renderpass to replay a single draw, we don't
    // need to do resolve operations.
    rdcarray<VkAttachmentReference> colorAttachments(sub.colorAttachments.size());
    rdcarray<VkAttachmentReference> inputAttachments(sub.inputAttachments.size());

    for(size_t i = 0; i < sub.colorAttachments.size(); i++)
    {
      colorAttachments[i].attachment = sub.colorAttachments[i];
      colorAttachments[i].layout = sub.colorLayouts[i];
    }
    for(size_t i = 0; i < sub.inputAttachments.size(); i++)
    {
      inputAttachments[i].attachment = sub.inputAttachments[i];
      inputAttachments[i].layout = sub.inputLayouts[i];
    }

    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.inputAttachmentCount = (uint32_t)sub.inputAttachments.size();
    subpassDesc.pInputAttachments = inputAttachments.data();
    subpassDesc.colorAttachmentCount = (uint32_t)sub.colorAttachments.size();
    subpassDesc.pColorAttachments = colorAttachments.data();

    rdcarray<VkAttachmentDescription> descs(rpInfo.attachments.size());
    for(uint32_t i = 0; i < rpInfo.attachments.size(); i++)
    {
      descs[i] = {};
      descs[i].flags = rpInfo.attachments[i].flags;
      descs[i].format = rpInfo.attachments[i].format;
      descs[i].samples = rpInfo.attachments[i].samples;
      descs[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      descs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      descs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      descs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      descs[i].initialLayout = rpInfo.attachments[i].initialLayout;
      descs[i].finalLayout = rpInfo.attachments[i].finalLayout;
    }
    for(uint32_t a = 0; a < subpassDesc.colorAttachmentCount; a++)
    {
      if(subpassDesc.pColorAttachments[a].attachment != VK_ATTACHMENT_UNUSED)
      {
        descs[subpassDesc.pColorAttachments[a].attachment].initialLayout =
            descs[subpassDesc.pColorAttachments[a].attachment].finalLayout =
                subpassDesc.pColorAttachments[a].layout;
      }
    }

    for(uint32_t a = 0; a < subpassDesc.inputAttachmentCount; a++)
    {
      if(subpassDesc.pInputAttachments[a].attachment != VK_ATTACHMENT_UNUSED)
      {
        descs[subpassDesc.pInputAttachments[a].attachment].initialLayout =
            descs[subpassDesc.pInputAttachments[a].attachment].finalLayout =
                subpassDesc.pInputAttachments[a].layout;
      }
    }

    VkAttachmentDescription dsAtt = {};
    dsAtt.format = m_CallbackInfo.dsFormat;
    dsAtt.samples = m_CallbackInfo.samples;
    dsAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    dsAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dsAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    dsAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    dsAtt.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    dsAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // If there is already a depth stencil attachment, substitute it.
    // Otherwise, add it at the end of all attachments.
    VkAttachmentReference dsAttachment = {};
    dsAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if(sub.depthstencilAttachment != -1)
    {
      descs[sub.depthstencilAttachment] = dsAtt;
      dsAttachment.attachment = sub.depthstencilAttachment;
    }
    else
    {
      descs.push_back(dsAtt);
      dsAttachment.attachment = (uint32_t)rpInfo.attachments.size();
    }
    subpassDesc.pDepthStencilAttachment = &dsAttachment;

    // If needed substitute the color attachment with the new format.
    if(newColorFormat != VK_FORMAT_UNDEFINED)
    {
      if(colorIdx < descs.size())
      {
        // It is an existing attachment.
        descs[colorIdx].format = newColorFormat;
      }
      else
      {
        // We are adding a new color attachment.
        VkAttachmentReference attRef = {};
        attRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attRef.attachment = colorIdx;
        colorAttachments.push_back(attRef);
        subpassDesc.colorAttachmentCount = (uint32_t)colorAttachments.size();
        subpassDesc.pColorAttachments = colorAttachments.data();

        VkAttachmentDescription attDesc = {};
        attDesc.format = newColorFormat;
        attDesc.samples = m_CallbackInfo.samples;
        attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        descs.push_back(attDesc);
      }
    }

    VkRenderPassCreateInfo rpCreateInfo = {};
    rpCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCreateInfo.attachmentCount = (uint32_t)descs.size();
    rpCreateInfo.subpassCount = 1;
    rpCreateInfo.pSubpasses = &subpassDesc;
    rpCreateInfo.pAttachments = descs.data();
    rpCreateInfo.dependencyCount = 0;
    rpCreateInfo.pDependencies = NULL;

    VkRenderPass renderpass;
    VkResult vkr =
        m_pDriver->vkCreateRenderPass(m_pDriver->GetDev(), &rpCreateInfo, NULL, &renderpass);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_RpsToDestroy.push_back(renderpass);
    return renderpass;
  }

  // CreateFrambuffer creates a new VkFramebuffer that is based on the original, but
  // substitutes the depth stencil image view. If there is no depth stencil attachment,
  // it will be added. Optionally, also substitutes the original target image view with
  // the newColorAtt.
  VkFramebuffer CreateFramebuffer(ResourceId rp, VkRenderPass newRp, ResourceId origFb,
                                  VkImageView newColorAtt = VK_NULL_HANDLE, uint32_t colorIdx = 0)
  {
    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(rp);
    // Currently only single subpass render passes are supported.
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();
    const VulkanCreationInfo::Framebuffer &fbInfo =
        m_pDriver->GetDebugManager()->GetFramebufferInfo(origFb);
    rdcarray<VkImageView> atts(fbInfo.attachments.size());

    for(uint32_t i = 0; i < fbInfo.attachments.size(); i++)
    {
      atts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(
          fbInfo.attachments[i].createdView);
    }

    // Either modify the existing color attachment view, or add a new one.
    if(newColorAtt != VK_NULL_HANDLE)
    {
      if(colorIdx < atts.size())
        atts[colorIdx] = newColorAtt;
      else
        atts.push_back(newColorAtt);
    }

    // Either modify the existing depth stencil attachment, or add one.
    if(sub.depthstencilAttachment != -1)
      atts[sub.depthstencilAttachment] = m_CallbackInfo.dsImageView;
    else
      atts.push_back(m_CallbackInfo.dsImageView);

    VkFramebufferCreateInfo fbCI = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbCI.renderPass = newRp;
    fbCI.attachmentCount = (uint32_t)atts.size();
    fbCI.pAttachments = atts.data();
    fbCI.width = fbInfo.width;
    fbCI.height = fbInfo.height;
    fbCI.layers = fbInfo.layers;

    VkFramebuffer framebuffer;
    VkResult vkr = m_pDriver->vkCreateFramebuffer(m_pDriver->GetDev(), &fbCI, NULL, &framebuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_FbsToDestroy.push_back(framebuffer);
    return framebuffer;
  }

  VkDescriptorSet GetCopyDescriptor(VkImage image, VkFormat format, uint32_t baseMip,
                                    uint32_t baseSlice)
  {
    auto it = m_CopyDescriptors.find(image);
    if(it != m_CopyDescriptors.end())
      return it->second;

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = format;
    viewInfo.subresourceRange = {0, baseMip, 1, baseSlice, 1};

    if(IsDepthOrStencilFormat(format))
    {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else
    {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      uint32_t bs = GetByteSize(1, 1, 1, format, 0);

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
    }

    VkImageView imageView;
    VkResult vkr = m_pDriver->vkCreateImageView(m_pDriver->GetDev(), &viewInfo, NULL, &imageView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_ImageViewsToDestroy.push_back(imageView);

    VkImageView imageView2 = VK_NULL_HANDLE;
    if(IsStencilFormat(format))
    {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      vkr = m_pDriver->vkCreateImageView(m_pDriver->GetDev(), &viewInfo, NULL, &imageView2);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
      m_ImageViewsToDestroy.push_back(imageView2);
    }

    VkDescriptorSet descSet = m_pDriver->GetReplay()->GetPixelHistoryDescriptor();
    m_pDriver->GetReplay()->UpdatePixelHistoryDescriptor(descSet, m_CallbackInfo.dstBuffer,
                                                         imageView, imageView2);
    m_CopyDescriptors.insert(std::make_pair(image, descSet));
    return descSet;
  }

  void CopyImagePixel(VkCommandBuffer cmd, CopyPixelParams &p, size_t offset)
  {
    VkImageAspectFlags aspectFlags = 0;
    bool depthCopy = IsDepthOrStencilFormat(p.srcImageFormat);
    if(depthCopy)
    {
      if(IsDepthOnlyFormat(p.srcImageFormat) || IsDepthAndStencilFormat(p.srcImageFormat))
        aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
      if(IsStencilFormat(p.srcImageFormat))
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else
    {
      aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    uint32_t baseMip = m_CallbackInfo.targetSubresource.mip;
    uint32_t baseSlice = m_CallbackInfo.targetSubresource.slice;
    // The images that are created specifically for evaluating pixel history are
    // already based on the target mip/slice.
    if((p.srcImage == m_CallbackInfo.subImage) || (p.srcImage == m_CallbackInfo.dsImage))
    {
      baseMip = 0;
      baseSlice = 0;
    }
    // For pipeline barriers.
    VkImageSubresourceRange subresource = {aspectFlags, baseMip, 1, baseSlice, 1};

    // For multi-sampled images can't call vkCmdCopyImageToBuffer directly,
    // copy using a compute shader into a staging image first.
    if(m_CallbackInfo.samples != VK_SAMPLE_COUNT_1_BIT)
    {
      VkImageMemoryBarrier barrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
              VK_ACCESS_MEMORY_WRITE_BIT,
          VK_ACCESS_SHADER_READ_BIT, p.srcImageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, Unwrap(p.srcImage), subresource};
      VkDescriptorSet descSet = GetCopyDescriptor(p.srcImage, p.srcImageFormat, baseMip, baseSlice);

      // Transition src image to SHADER_READ_ONLY_OPTIMAL.
      DoPipelineBarrier(cmd, 1, &barrier);

      m_pDriver->GetReplay()->CopyPixelForPixelHistory(
          cmd, {(int32_t)m_CallbackInfo.x, (int32_t)m_CallbackInfo.y},
          m_CallbackInfo.targetSubresource.sample, (uint32_t)offset / 16, p.srcImageFormat, descSet);

      // Transition src image back to its layout.
      barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.newLayout = p.srcImageLayout;

      DoPipelineBarrier(cmd, 1, &barrier);
    }
    else
    {
      rdcarray<VkBufferImageCopy> regions;
      VkBufferImageCopy region = {};
      region.bufferOffset = (uint64_t)offset;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;
      region.imageOffset.x = m_CallbackInfo.x;
      region.imageOffset.y = m_CallbackInfo.y;
      region.imageOffset.z = 0;
      region.imageExtent.width = 1U;
      region.imageExtent.height = 1U;
      region.imageExtent.depth = 1U;
      region.imageSubresource.baseArrayLayer = baseSlice;
      region.imageSubresource.mipLevel = baseMip;
      region.imageSubresource.layerCount = 1;

      if(!depthCopy)
      {
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions.push_back(region);
      }
      else
      {
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if(IsDepthOnlyFormat(p.srcImageFormat) || IsDepthAndStencilFormat(p.srcImageFormat))
        {
          regions.push_back(region);
        }
        if(IsStencilFormat(p.srcImageFormat))
        {
          region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
          region.bufferOffset = offset + 4;
          regions.push_back(region);
        }
      }

      VkImageMemoryBarrier barrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
              VK_ACCESS_MEMORY_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT, p.srcImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, Unwrap(p.srcImage), subresource};
      DoPipelineBarrier(cmd, 1, &barrier);

      ObjDisp(cmd)->CmdCopyImageToBuffer(
          Unwrap(cmd), Unwrap(p.srcImage), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          Unwrap(m_CallbackInfo.dstBuffer), (uint32_t)regions.size(), regions.data());

      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout = p.srcImageLayout;
      DoPipelineBarrier(cmd, 1, &barrier);
    }
  }

  bool HasMultipleSubpasses()
  {
    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(m_pDriver->GetCmdRenderState().renderPass);
    return (rpInfo.subpasses.size() > 1);
  }

  // Returns teh color attachment index that corresponds to the target image for
  // pixel history.
  uint32_t GetColorAttachmentIndex(const VulkanRenderState &renderstate)
  {
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
      return 0;

    uint32_t framebufferIndex = 0;
    const rdcarray<ResourceId> &atts = renderstate.GetFramebufferAttachments();

    for(uint32_t i = 0; i < atts.size(); i++)
    {
      ResourceId img = m_pDriver->GetDebugManager()->GetImageViewInfo(atts[i]).image;
      if(img == GetResID(m_CallbackInfo.targetImage))
      {
        framebufferIndex = i;
        break;
      }
    }

    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(renderstate.renderPass);
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();
    for(uint32_t i = 0; i < sub.colorAttachments.size(); i++)
    {
      if(framebufferIndex == sub.colorAttachments[i])
        return i;
    }
    return 0;
  }

  WrappedVulkan *m_pDriver;
  PixelHistoryShaderCache *m_ShaderCache;
  PixelHistoryCallbackInfo m_CallbackInfo;
  VkQueryPool m_OcclusionPool;
  rdcarray<VkRenderPass> m_RpsToDestroy;
  rdcarray<VkFramebuffer> m_FbsToDestroy;
  rdcarray<VkDynamicState> m_DynamicStates;
  std::map<VkImage, VkDescriptorSet> m_CopyDescriptors;
  rdcarray<VkImageView> m_ImageViewsToDestroy;
};

// VulkanOcclusionCallback callback is used to determine which draw events might have
// modified the pixel by doing an occlusion query.
struct VulkanOcclusionCallback : public VulkanPixelHistoryCallback
{
  VulkanOcclusionCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                          const PixelHistoryCallbackInfo &callbackInfo, VkQueryPool occlusionPool,
                          const rdcarray<EventUsage> &allEvents)
      : VulkanPixelHistoryCallback(vk, shaderCache, callbackInfo, occlusionPool)
  {
    for(size_t i = 0; i < allEvents.size(); i++)
      m_Events.push_back(allEvents[i].eventId);
  }

  ~VulkanOcclusionCallback()
  {
    for(auto it = m_PipeCache.begin(); it != m_PipeCache.end(); ++it)
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), it->second, NULL);
  }

  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;
    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();

    VkPipeline pipe = GetPixelOcclusionPipeline(eid, prevState.graphics.pipeline,
                                                GetColorAttachmentIndex(prevState));
    // set the scissor
    for(uint32_t i = 0; i < pipestate.views.size(); i++)
      ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);
    // set stencil state (though it's unused here)
    pipestate.front.compare = pipestate.front.write = 0xff;
    pipestate.front.ref = 0;
    pipestate.back = pipestate.front;
    pipestate.graphics.pipeline = GetResID(pipe);
    ReplayDrawWithQuery(cmd, eid);

    m_pDriver->GetCmdRenderState() = prevState;
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) {}
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) { return; }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return; }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}
  bool SplitSecondary() { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
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
    if(it == m_OcclusionQueries.end())
      return 0;
    RDCASSERT(it->second < m_OcclusionResults.size());
    return m_OcclusionResults[it->second];
  }

private:
  // ReplayDrawWithQuery binds the pipeline in the current state, and replays a single
  // draw with an occlusion query.
  void ReplayDrawWithQuery(VkCommandBuffer cmd, uint32_t eventId)
  {
    const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);

    uint32_t occlIndex = (uint32_t)m_OcclusionQueries.size();
    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, occlIndex, 0);

    if(drawcall->flags & DrawFlags::Indexed)
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                   drawcall->indexOffset, drawcall->baseVertex,
                                   drawcall->instanceOffset);
    else
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                            drawcall->vertexOffset, drawcall->instanceOffset);

    ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_OcclusionPool, occlIndex);
    m_OcclusionQueries.insert(std::make_pair(eventId, occlIndex));
  }

  VkPipeline GetPixelOcclusionPipeline(uint32_t eid, ResourceId pipeline, uint32_t outputIndex)
  {
    auto it = m_PipeCache.find(pipeline);
    if(it != m_PipeCache.end())
      return it->second;

    VkGraphicsPipelineCreateInfo pipeCreateInfo = {};
    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    MakeIncrementStencilPipelineCI(eid, pipeline, pipeCreateInfo, stages, true);

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      if(stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        stages[i].module = m_ShaderCache->GetFixedColShader(outputIndex);
        stages[i].pName = "main";
        break;
      }
    }
    VkPipeline pipe;
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL, &pipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_PipeCache.insert(std::make_pair(pipeline, pipe));
    return pipe;
  }

private:
  std::map<ResourceId, VkPipeline> m_PipeCache;
  rdcarray<uint32_t> m_Events;
  // Key is event ID, and value is an index of where the occlusion result.
  std::map<uint32_t, uint32_t> m_OcclusionQueries;
  rdcarray<uint64_t> m_OcclusionResults;
};

struct VulkanColorAndStencilCallback : public VulkanPixelHistoryCallback
{
  VulkanColorAndStencilCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                                const PixelHistoryCallbackInfo &callbackInfo,
                                const rdcarray<uint32_t> &events)
      : VulkanPixelHistoryCallback(vk, shaderCache, callbackInfo, VK_NULL_HANDLE),
        m_Events(events),
        multipleSubpassWarningPrinted(false)
  {
  }

  ~VulkanColorAndStencilCallback()
  {
    for(auto it = m_PipeCache.begin(); it != m_PipeCache.end(); ++it)
    {
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), it->second.fixedShaderStencil, NULL);
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), it->second.originalShaderStencil, NULL);
    }
  }

  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid) || !m_pDriver->IsCmdPrimary())
      return;

    if(HasMultipleSubpasses())
    {
      if(!multipleSubpassWarningPrinted)
      {
        RDCWARN("Multiple subpasses in a render pass are not supported for pixel history.");
        multipleSubpassWarningPrinted = true;
      }
      return;
    }

    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();

    pipestate.EndRenderPass(cmd);

    // Get pre-modification values
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);

    CopyPixel(eid, cmd, storeOffset);

    ResourceId prevRenderpass = pipestate.renderPass;
    ResourceId prevFramebuffer = pipestate.GetFramebuffer();
    rdcarray<ResourceId> prevFBattachments = pipestate.GetFramebufferAttachments();

    uint32_t prevSubpass = pipestate.subpass;

    {
      VkRenderPass newRp = CreateRenderPass(pipestate.renderPass);
      VkFramebuffer newFb =
          CreateFramebuffer(pipestate.renderPass, newRp, pipestate.GetFramebuffer());

      PipelineReplacements replacements = GetPipelineReplacements(
          eid, pipestate.graphics.pipeline, newRp, GetColorAttachmentIndex(prevState));

      for(uint32_t i = 0; i < pipestate.views.size(); i++)
        ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);

      // TODO: should fill depth value from the original DS attachment.

      // Replay the draw with a fixed color shader that never discards, and stencil
      // increment to count number of fragments. We will get the number of fragments
      // not accounting for shader discard.
      pipestate.SetFramebuffer(m_pDriver, GetResID(newFb));
      pipestate.renderPass = GetResID(newRp);
      pipestate.graphics.pipeline = GetResID(replacements.fixedShaderStencil);
      pipestate.front.compare = pipestate.front.write = 0xff;
      pipestate.front.ref = 0;
      pipestate.back = pipestate.front;
      ReplayDraw(cmd, eid, true);

      CopyPixelParams params = {};
      params.srcImage = m_CallbackInfo.dsImage;
      params.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      params.srcImageFormat = m_CallbackInfo.dsFormat;
      // Copy stencil value that indicates the number of fragments ignoring
      // shader discard.
      CopyImagePixel(cmd, params, storeOffset + offsetof(struct EventInfo, dsWithoutShaderDiscard));

      // TODO: in between reset the depth value.

      // Replay the draw with the original fragment shader to get the actual number
      // of fragments, accounting for potential shader discard.
      pipestate.graphics.pipeline = GetResID(replacements.originalShaderStencil);
      ReplayDraw(cmd, eid, true);

      CopyImagePixel(cmd, params, storeOffset + offsetof(struct EventInfo, dsWithShaderDiscard));
    }

    // Restore the state.
    m_pDriver->GetCmdRenderState() = prevState;
    pipestate.SetFramebuffer(prevFramebuffer, prevFBattachments);
    pipestate.renderPass = prevRenderpass;
    pipestate.subpass = prevSubpass;

    // TODO: Need to re-start on the correct subpass.
    if(pipestate.graphics.pipeline != ResourceId())
      pipestate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid) || !m_pDriver->IsCmdPrimary())
      return false;

    if(HasMultipleSubpasses())
    {
      if(!multipleSubpassWarningPrinted)
      {
        RDCWARN("Multiple subpasses in a render pass are not supported for pixel history.");
        multipleSubpassWarningPrinted = true;
      }
      return false;
    }

    m_pDriver->GetCmdRenderState().EndRenderPass(cmd);

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);

    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod));

    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(m_pDriver, cmd,
                                                                VulkanRenderState::BindGraphics);

    // Get post-modification values
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
    uint32_t eventId = 0;
    if(m_Events.size() == 0)
      return;
    for(size_t i = 0; i < m_Events.size(); i++)
    {
      // Find the first event in range
      if(m_Events[i] >= secondaryFirst && m_Events[i] <= secondaryLast)
      {
        eventId = m_Events[i];
        break;
      }
    }
    if(eventId == 0)
      return;

    if(HasMultipleSubpasses())
    {
      if(!multipleSubpassWarningPrinted)
      {
        RDCWARN("Multiple subpasses in a render pass are not supported for pixel history.");
        multipleSubpassWarningPrinted = true;
      }
      return;
    }

    m_pDriver->GetCmdRenderState().EndRenderPass(cmd);

    // Copy
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eventId, cmd, storeOffset);
    m_EventIndices.insert(std::make_pair(eventId, m_EventIndices.size()));

    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(m_pDriver, cmd,
                                                                VulkanRenderState::BindNone);
  }

  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
    uint32_t eventId = 0;
    if(m_Events.size() == 0)
      return;
    for(int32_t i = (int32_t)m_Events.size() - 1; i >= 0; i--)
    {
      // Find the last event in range.
      if(m_Events[i] >= secondaryFirst && m_Events[i] <= secondaryLast)
      {
        eventId = m_Events[i];
        break;
      }
    }
    if(eventId == 0)
      return;

    if(HasMultipleSubpasses())
    {
      if(!multipleSubpassWarningPrinted)
      {
        RDCWARN("Multiple subpasses in a render pass are not supported for pixel history.");
        multipleSubpassWarningPrinted = true;
      }
      return;
    }

    m_pDriver->GetCmdRenderState().EndRenderPass(cmd);
    size_t storeOffset = 0;
    auto it = m_EventIndices.find(eventId);
    if(it != m_EventIndices.end())
    {
      storeOffset = it->second * sizeof(EventInfo);
    }
    else
    {
      storeOffset = m_EventIndices.size() * sizeof(EventInfo);
      m_EventIndices.insert(std::make_pair(eventId, m_EventIndices.size()));
    }
    CopyPixel(eventId, cmd, storeOffset + offsetof(struct EventInfo, postmod));
    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(m_pDriver, cmd,
                                                                VulkanRenderState::BindNone);
  }

  void PreDispatch(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset);
  }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return false;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod));
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { PreDispatch(eid, cmd); }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return false;
    if(HasMultipleSubpasses())
    {
      if(!multipleSubpassWarningPrinted)
      {
        RDCWARN("Multiple subpasses in a render pass are not supported for pixel history.");
        multipleSubpassWarningPrinted = true;
      }
      return false;
    }
    if(flags & DrawFlags::BeginPass)
      m_pDriver->GetCmdRenderState().EndRenderPass(cmd);

    bool ret = PostDispatch(eid, cmd);

    if(flags & DrawFlags::BeginPass)
      m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(m_pDriver, cmd,
                                                                  VulkanRenderState::BindNone);

    return ret;
  }

  bool SplitSecondary() { return true; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    RDCWARN(
        "Alised events are not supported, results might be inaccurate. Primary event id: %u, "
        "alias: %u.",
        primary, alias);
  }

  int32_t GetEventIndex(uint32_t eventId)
  {
    auto it = m_EventIndices.find(eventId);
    if(it == m_EventIndices.end())
      // Most likely a secondary command buffer event for which there is no
      // information.
      return -1;
    RDCASSERT(it != m_EventIndices.end());
    return (int32_t)it->second;
  }

  VkFormat GetDepthFormat(uint32_t eventId)
  {
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
      return m_CallbackInfo.targetImageFormat;
    auto it = m_DepthFormats.find(eventId);
    if(it == m_DepthFormats.end())
      return VK_FORMAT_UNDEFINED;
    return it->second;
  }

private:
  void CopyPixel(uint32_t eid, VkCommandBuffer cmd, size_t offset)
  {
    CopyPixelParams targetCopyParams = {};
    targetCopyParams.srcImage = m_CallbackInfo.targetImage;
    targetCopyParams.srcImageFormat = m_CallbackInfo.targetImageFormat;
    VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      offset += offsetof(struct PixelHistoryValue, depth);
      aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    targetCopyParams.srcImageLayout = m_pDriver->GetDebugManager()->GetImageLayout(
        GetResID(m_CallbackInfo.targetImage), aspect, m_CallbackInfo.targetSubresource.mip,
        m_CallbackInfo.targetSubresource.slice);
    CopyImagePixel(cmd, targetCopyParams, offset);

    // If the target image is a depth/stencil attachment, we already
    // copied the value above.
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
      return;

    const DrawcallDescription *draw = m_pDriver->GetDrawcall(eid);
    if(draw && draw->depthOut != ResourceId())
    {
      ResourceId resId = m_pDriver->GetResourceManager()->GetLiveID(draw->depthOut);
      VkImage depthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(resId);
      const VulkanCreationInfo::Image &imginfo = m_pDriver->GetDebugManager()->GetImageInfo(resId);
      CopyPixelParams depthCopyParams = targetCopyParams;
      depthCopyParams.srcImage = depthImage;
      depthCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthCopyParams.srcImageFormat = imginfo.format;
      CopyImagePixel(cmd, depthCopyParams, offset + offsetof(struct PixelHistoryValue, depth));
      m_DepthFormats.insert(std::make_pair(eid, imginfo.format));
    }
  }

  // ReplayDraw begins renderpass, executes a single draw defined by the eventId and
  // ends the renderpass.
  void ReplayDraw(VkCommandBuffer cmd, uint32_t eventId, bool clear = false)
  {
    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(m_pDriver, cmd,
                                                                VulkanRenderState::BindGraphics);

    ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
    ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);

    if(clear)
    {
      VkClearAttachment att = {};
      att.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      VkClearRect rect = {};
      rect.rect.offset.x = m_CallbackInfo.x;
      rect.rect.offset.y = m_CallbackInfo.y;
      rect.rect.extent.width = 1;
      rect.rect.extent.height = 1;
      rect.baseArrayLayer = 0;
      rect.layerCount = 1;
      ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 1, &att, 1, &rect);
    }

    const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);
    if(drawcall->flags & DrawFlags::Indexed)
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                   drawcall->indexOffset, drawcall->baseVertex,
                                   drawcall->instanceOffset);
    else
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                            drawcall->vertexOffset, drawcall->instanceOffset);

    m_pDriver->GetCmdRenderState().EndRenderPass(cmd);
  }

  // GetPipelineReplacements creates pipeline replacements that disable all tests,
  // and use either fixed or original fragment shader, and shaders that don't
  // have side effects.
  PipelineReplacements GetPipelineReplacements(uint32_t eid, ResourceId pipeline, VkRenderPass rp,
                                               uint32_t outputIndex)
  {
    // The map does not keep track of the event ID, event ID is only used to figure out
    // which shaders need to be modified. Those flags are based on the shaders bound,
    // so in theory all events should share those flags if they are using the same
    // pipeline.
    auto pipeIt = m_PipeCache.find(pipeline);
    if(pipeIt != m_PipeCache.end())
      return pipeIt->second;

    VkGraphicsPipelineCreateInfo pipeCreateInfo = {};
    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    MakeIncrementStencilPipelineCI(eid, pipeline, pipeCreateInfo, stages, false);
    // No need to change depth stencil state, it is already
    // set to always pass, and increment.
    pipeCreateInfo.renderPass = rp;

    PipelineReplacements replacements = {};
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL,
                                                        &replacements.originalShaderStencil);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      if(stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        stages[i].module = m_ShaderCache->GetFixedColShader(outputIndex);
        stages[i].pName = "main";
        break;
      }
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                               &pipeCreateInfo, NULL,
                                               &replacements.fixedShaderStencil);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_PipeCache.insert(std::make_pair(pipeline, replacements));

    return replacements;
  }

  std::map<ResourceId, PipelineReplacements> m_PipeCache;
  rdcarray<uint32_t> m_Events;
  // Key is event ID, and value is an index of where the event data is stored.
  std::map<uint32_t, size_t> m_EventIndices;
  bool multipleSubpassWarningPrinted;
  std::map<uint32_t, VkFormat> m_DepthFormats;
};

// TestsFailedCallback replays draws to figure out which tests failed (for ex., depth,
// stencil test etc).
struct TestsFailedCallback : public VulkanPixelHistoryCallback
{
  TestsFailedCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                      const PixelHistoryCallbackInfo &callbackInfo, VkQueryPool occlusionPool,
                      rdcarray<uint32_t> events)
      : VulkanPixelHistoryCallback(vk, shaderCache, callbackInfo, occlusionPool), m_Events(events)
  {
  }

  ~TestsFailedCallback() {}
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;

    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();
    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetDebugManager()->GetPipelineInfo(pipestate.graphics.pipeline);
    uint32_t eventFlags = CalculateEventFlags(p, pipestate);
    m_EventFlags[eid] = eventFlags;

    // TODO: figure out if the shader has early fragments tests turned on,
    // based on the currently bound fragment shader.
    bool earlyFragmentTests = false;
    m_HasEarlyFragments[eid] = earlyFragmentTests;

    ResourceId curPipeline = pipestate.graphics.pipeline;
    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();

    ReplayDrawWithTests(cmd, eid, eventFlags, curPipeline, GetColorAttachmentIndex(prevState));

    m_pDriver->GetCmdRenderState() = prevState;
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);
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
  bool SplitSecondary() { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
  }
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
        if((m_CallbackInfo.x >= (uint32_t)offset.x) && (m_CallbackInfo.y >= (uint32_t)offset.y) &&
           (m_CallbackInfo.x < (offset.x + extent.width)) &&
           (m_CallbackInfo.y < (offset.y + extent.height)))
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
      if(m_pDriver->GetDeviceEnabledFeatures().independentBlend)
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

    if(p.shaders[StageIndex(VK_SHADER_STAGE_FRAGMENT_BIT)].module == ResourceId())
      flags |= UnboundFragmentShader;

    // Samples
    {
      // TODO: figure out if we always need to check this.
      flags |= TestEnabled_SampleMask;

      // compare to ms->pSampleMask
      if((p.sampleMask & m_CallbackInfo.sampleMask) == 0)
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
                           ResourceId basePipeline, uint32_t outputIndex)
  {
    // Backface culling
    if(eventFlags & TestMustFail_Culling)
      return;

    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetDebugManager()->GetPipelineInfo(basePipeline);
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

    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();
    rdcarray<VkRect2D> prevScissors = pipestate.scissors;
    for(uint32_t i = 0; i < pipestate.views.size(); i++)
      ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);

    if(eventFlags & TestEnabled_Culling)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_DisableDepthBoundsTest |
          PipelineCreationFlags_DisableStencilTest | PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
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
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      // This will change the scissor for the later tests, but since those
      // tests happen later in the pipeline, it does not matter.
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
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      ReplayDraw(cmd, pipe, eid, TestEnabled_SampleMask);
    }

    // Depth bounds
    if(eventFlags & TestEnabled_DepthBounds)
    {
      uint32_t pipeFlags = PipelineCreationFlags_DisableStencilTest |
                           PipelineCreationFlags_DisableDepthTest |
                           PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      ReplayDraw(cmd, pipe, eid, TestEnabled_DepthBounds);
    }

    // Stencil test
    if(eventFlags & TestMustFail_StencilTesting)
      return;

    if(eventFlags & TestEnabled_StencilTesting)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      ReplayDraw(cmd, pipe, eid, TestEnabled_StencilTesting);
    }

    // Depth test
    if(eventFlags & TestMustFail_DepthTesting)
      return;

    if(eventFlags & TestEnabled_DepthTesting)
    {
      // Previous test might have modified the stencil state, which could
      // cause this event to fail.
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableStencilTest | PipelineCreationFlags_FixedColorShader;

      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
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
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      ReplayDraw(cmd, pipe, eid, TestEnabled_FragmentDiscard);
    }
  }

  // Creates a pipeline that is based on the given pipeline and the given
  // pipeline flags. Modifies the base pipeline according to the flags, and
  // leaves the original pipeline behavior if a flag is not set.
  VkPipeline CreatePipeline(ResourceId basePipeline, uint32_t pipeCreateFlags,
                            const rdcarray<VkShaderModule> &replacementShaders, uint32_t outputIndex)
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
    VkPipelineMultisampleStateCreateInfo *ms =
        (VkPipelineMultisampleStateCreateInfo *)ci.pMultisampleState;

    AddDynamicStates(ci);

    // Only interested in a single sample.
    ms->pSampleMask = &m_CallbackInfo.sampleMask;
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
        stages[i].module = m_ShaderCache->GetFixedColShader(outputIndex);
        stages[i].pName = "main";
      }
      else if(replacementShaders[StageIndex(stages[i].stage)] != VK_NULL_HANDLE)
      {
        stages[i].module = replacementShaders[StageIndex(stages[i].stage)];
      }
    }
    ci.pStages = stages.data();

    VkPipeline pipe;
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1, &ci,
                                                        NULL, &pipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_PipeCache.insert(std::make_pair(pipeKey, pipe));
    return pipe;
  }

  void ReplayDraw(VkCommandBuffer cmd, VkPipeline pipe, int eventId, uint32_t test)
  {
    m_pDriver->GetCmdRenderState().graphics.pipeline = GetResID(pipe);
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);

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

// Callback used to get values for each fragment.
struct VulkanPixelHistoryPerFragmentCallback : VulkanPixelHistoryCallback
{
  VulkanPixelHistoryPerFragmentCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                                        const PixelHistoryCallbackInfo &callbackInfo,
                                        const std::map<uint32_t, uint32_t> &eventFragments,
                                        const std::map<uint32_t, ModificationValue> &eventPremods)
      : VulkanPixelHistoryCallback(vk, shaderCache, callbackInfo, VK_NULL_HANDLE),
        m_EventFragments(eventFragments),
        m_EventPremods(eventPremods)
  {
  }

  ~VulkanPixelHistoryPerFragmentCallback()
  {
    for(const VkPipeline &pipe : m_PipesToDestroy)
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), pipe, NULL);
  }

  struct Pipelines
  {
    // Disable all tests, use the new render pass to render into a separate
    // attachment, and use fragment shader that outputs primitive ID.
    VkPipeline primitiveIdPipe;
    // Turn off blending.
    VkPipeline shaderOutPipe;
    // Enable blending to get post event values.
    VkPipeline postModPipe;
  };

  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_EventFragments.find(eid) == m_EventFragments.end())
      return;

    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &state = m_pDriver->GetCmdRenderState();
    ResourceId curPipeline = state.graphics.pipeline;
    state.EndRenderPass(cmd);

    uint32_t numFragmentsInEvent = m_EventFragments[eid];

    uint32_t framebufferIndex = 0;
    uint32_t colorOutputIndex = 0;
    const rdcarray<ResourceId> &atts = prevState.GetFramebufferAttachments();
    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(prevState.renderPass);
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();

    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      // Going to add another color attachment.
      framebufferIndex = (uint32_t)atts.size();
      colorOutputIndex = (uint32_t)sub.colorAttachments.size();
    }
    else
    {
      for(uint32_t i = 0; i < atts.size(); i++)
      {
        ResourceId img = m_pDriver->GetDebugManager()->GetImageViewInfo(atts[i]).image;
        if(img == GetResID(m_CallbackInfo.targetImage))
        {
          framebufferIndex = i;
          break;
        }
      }
      for(uint32_t i = 0; i < sub.colorAttachments.size(); i++)
      {
        if(framebufferIndex == sub.colorAttachments[i])
        {
          colorOutputIndex = i;
          break;
        }
      }
    }

    VkRenderPass newRp =
        CreateRenderPass(state.renderPass, VK_FORMAT_R32G32B32A32_SFLOAT, framebufferIndex);

    VkFramebuffer newFb = CreateFramebuffer(state.renderPass, newRp, state.GetFramebuffer(),
                                            m_CallbackInfo.subImageView, framebufferIndex);

    Pipelines pipes = CreatePerFragmentPipelines(curPipeline, newRp, eid, 0, colorOutputIndex);

    for(uint32_t i = 0; i < state.views.size(); i++)
      ScissorToPixel(state.views[i], state.scissors[i]);

    state.renderPass = GetResID(newRp);
    state.SetFramebuffer(m_pDriver, GetResID(newFb));

    VkPipeline pipesIter[2];
    pipesIter[0] = pipes.primitiveIdPipe;
    pipesIter[1] = pipes.shaderOutPipe;

    CopyPixelParams colourCopyParams = {};
    colourCopyParams.srcImage = m_CallbackInfo.subImage;
    colourCopyParams.srcImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      colourCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
      // Use the layout of the image we are substituting for.
      VkImageLayout srcImageLayout = m_pDriver->GetDebugManager()->GetImageLayout(
          GetResID(m_CallbackInfo.targetImage), VK_IMAGE_ASPECT_COLOR_BIT,
          m_CallbackInfo.targetSubresource.mip, m_CallbackInfo.targetSubresource.slice);
      colourCopyParams.srcImageLayout = srcImageLayout;
    }

    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetDebugManager()->GetPipelineInfo(prevState.graphics.pipeline);
    bool depthEnabled = p.depthTestEnable;

    // Get primitive ID and shader output value for each fragment.
    for(uint32_t f = 0; f < numFragmentsInEvent; f++)
    {
      for(uint32_t i = 0; i < 2; i++)
      {
        if(i == 0 && !m_pDriver->GetDeviceEnabledFeatures().geometryShader)
        {
          // without geometryShader, can't read primitive ID in pixel shader
          continue;
        }

        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            Unwrap(m_CallbackInfo.dsImage),
            {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1}};

        DoPipelineBarrier(cmd, 1, &barrier);

        // Reset depth to 0.0f, depth test is set to always pass.
        // This way we get the value for just that fragment.
        // Reset stencil to 0.
        VkClearDepthStencilValue dsValue = {};
        dsValue.depth = 0.0f;
        dsValue.stencil = 0;
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        range.baseArrayLayer = 0;
        range.baseMipLevel = 0;
        range.layerCount = 1;
        range.levelCount = 1;

        ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), Unwrap(m_CallbackInfo.dsImage),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dsValue, 1,
                                                &range);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        DoPipelineBarrier(cmd, 1, &barrier);

        m_pDriver->GetCmdRenderState().graphics.pipeline = GetResID(pipesIter[i]);

        m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
            m_pDriver, cmd, VulkanRenderState::BindGraphics);

        // Update stencil reference to the current fragment index, so that we get values
        // for a single fragment only.
        ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
        ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
        ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, f);
        const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eid);
        if(drawcall->flags & DrawFlags::Indexed)
          ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                       drawcall->indexOffset, drawcall->baseVertex,
                                       drawcall->instanceOffset);
        else
          ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                drawcall->vertexOffset, drawcall->instanceOffset);
        state.EndRenderPass(cmd);

        uint32_t storeOffset = (fragsProcessed + f) * sizeof(PerFragmentInfo);
        if(i == 1)
        {
          storeOffset += offsetof(struct PerFragmentInfo, shaderOut);
          if(depthEnabled)
          {
            CopyPixelParams depthCopyParams = colourCopyParams;
            depthCopyParams.srcImage = m_CallbackInfo.dsImage;
            depthCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthCopyParams.srcImageFormat = m_CallbackInfo.dsFormat;
            CopyImagePixel(cmd, depthCopyParams,
                           storeOffset + offsetof(struct PixelHistoryValue, depth));
          }
        }
        CopyImagePixel(cmd, colourCopyParams, storeOffset);
      }
    }

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

    const ModificationValue &premod = m_EventPremods[eid];
    // For every fragment except the last one, retrieve post-modification
    // value.
    for(uint32_t f = 0; f < numFragmentsInEvent - 1; f++)
    {
      // Get post-modification value, use the original framebuffer attachment.
      state.graphics.pipeline = GetResID(pipes.postModPipe);
      state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics);
      // Have to reset stencil.
      VkClearAttachment att = {};
      att.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      VkClearRect rect = {};
      rect.rect.offset.x = m_CallbackInfo.x;
      rect.rect.offset.y = m_CallbackInfo.y;
      rect.rect.extent.width = 1;
      rect.rect.extent.height = 1;
      rect.baseArrayLayer = 0;
      rect.layerCount = 1;
      ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 1, &att, 1, &rect);

      if(f == 0)
      {
        // Before starting the draw, initialize the pixel to the premodification value
        // for this event, for both color and depth.
        VkClearAttachment clearAtts[2] = {};

        clearAtts[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAtts[0].colorAttachment = colorOutputIndex;
        memcpy(clearAtts[0].clearValue.color.float32, premod.col.floatValue,
               sizeof(clearAtts[0].clearValue.color));

        clearAtts[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clearAtts[1].clearValue.depthStencil.depth = premod.depth;

        ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 2, clearAtts, 1, &rect);
      }

      ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
      ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, f);
      const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eid);
      if(drawcall->flags & DrawFlags::Indexed)
        ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                     drawcall->indexOffset, drawcall->baseVertex,
                                     drawcall->instanceOffset);
      else
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                              drawcall->vertexOffset, drawcall->instanceOffset);
      state.EndRenderPass(cmd);

      CopyImagePixel(cmd, colourCopyParams, (fragsProcessed + f) * sizeof(PerFragmentInfo) +
                                                offsetof(struct PerFragmentInfo, postMod));

      if(depthImage != VK_NULL_HANDLE)
      {
        CopyPixelParams depthCopyParams = colourCopyParams;
        depthCopyParams.srcImage = m_CallbackInfo.dsImage;
        depthCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthCopyParams.srcImageFormat = m_CallbackInfo.dsFormat;
        CopyImagePixel(cmd, depthCopyParams, (fragsProcessed + f) * sizeof(PerFragmentInfo) +
                                                 offsetof(struct PerFragmentInfo, postMod) +
                                                 offsetof(struct PixelHistoryValue, depth));
      }
    }

    m_EventIndices[eid] = fragsProcessed;
    fragsProcessed += numFragmentsInEvent;

    m_pDriver->GetCmdRenderState() = prevState;
    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(m_pDriver, cmd,
                                                                VulkanRenderState::BindGraphics);
  }
  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) {}
  // CreatePerFragmentPipelines for getting per fragment information.
  Pipelines CreatePerFragmentPipelines(ResourceId pipe, VkRenderPass rp, uint32_t eid,
                                       uint32_t fragmentIndex, uint32_t colorOutputIndex)
  {
    const VulkanCreationInfo::Pipeline &p = m_pDriver->GetDebugManager()->GetPipelineInfo(pipe);
    VkGraphicsPipelineCreateInfo pipeCreateInfo = {};
    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, pipe);

    AddDynamicStates(pipeCreateInfo);

    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
    VkPipelineMultisampleStateCreateInfo *ms =
        (VkPipelineMultisampleStateCreateInfo *)pipeCreateInfo.pMultisampleState;

    VkRect2D newScissors[16];
    memset(newScissors, 0, sizeof(newScissors));
    // Modify the stencil state, so that only one fragment passes.
    {
      ds->stencilTestEnable = VK_TRUE;
      ds->front.compareOp = VK_COMPARE_OP_EQUAL;
      ds->front.failOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      ds->front.compareMask = 0xff;
      ds->front.writeMask = 0xff;
      ds->front.reference = 0;
      ds->back = ds->front;

      ms->pSampleMask = &m_CallbackInfo.sampleMask;
    }

    stages.resize(pipeCreateInfo.stageCount);
    memcpy(stages.data(), pipeCreateInfo.pStages, stages.byteSize());

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
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkShaderModule replacement = replacementShaders[StageIndex(stages[i].stage)];
      if(replacement != VK_NULL_HANDLE)
        stages[i].module = replacement;
    }
    pipeCreateInfo.pStages = stages.data();
    pipeCreateInfo.renderPass = rp;

    VkPipelineColorBlendStateCreateInfo *cbs =
        (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
    // Turn off blending so that we can get shader output values.
    VkPipelineColorBlendAttachmentState *atts =
        (VkPipelineColorBlendAttachmentState *)cbs->pAttachments;
    rdcarray<VkPipelineColorBlendAttachmentState> newAtts;

    // Check if we need to add a new color attachment.
    if(colorOutputIndex == cbs->attachmentCount)
    {
      newAtts.resize(cbs->attachmentCount + 1);
      memcpy(newAtts.data(), cbs->pAttachments,
             cbs->attachmentCount * sizeof(VkPipelineColorBlendAttachmentState));
      VkPipelineColorBlendAttachmentState newAtt = {};
      if(cbs->attachmentCount > 0)
      {
        // If there are existing color attachments, copy the blend information from it.
        // It will be adjusted later.
        newAtt = cbs->pAttachments[0];
      }
      else
      {
        newAtt.blendEnable = VK_FALSE;
        newAtt.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
      }
      newAtts[cbs->attachmentCount] = newAtt;
      cbs->attachmentCount = (uint32_t)newAtts.size();
      cbs->pAttachments = newAtts.data();

      atts = newAtts.data();
    }

    Pipelines pipes = {};
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL, &pipes.postModPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_PipesToDestroy.push_back(pipes.postModPipe);

    for(uint32_t i = 0; i < cbs->attachmentCount; i++)
    {
      atts[i].blendEnable = 0;
      atts[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }

    {
      ds->depthBoundsTestEnable = VK_FALSE;
      ds->depthCompareOp = VK_COMPARE_OP_ALWAYS;
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                               &pipeCreateInfo, NULL, &pipes.shaderOutPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_PipesToDestroy.push_back(pipes.shaderOutPipe);

    {
      ds->depthTestEnable = VK_FALSE;
      ds->depthWriteEnable = VK_FALSE;
    }

    // Output the primitive ID.
    VkPipelineShaderStageCreateInfo stageCI = {};
    stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stageCI.module = m_ShaderCache->GetPrimitiveIdShader(colorOutputIndex);
    stageCI.pName = "main";
    bool fsFound = false;
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      if(stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        stages[i] = stageCI;
        fsFound = true;
        break;
      }
    }
    if(!fsFound)
    {
      stages.push_back(stageCI);
      pipeCreateInfo.stageCount = (uint32_t)stages.size();
      pipeCreateInfo.pStages = stages.data();
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                               &pipeCreateInfo, NULL, &pipes.primitiveIdPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_PipesToDestroy.push_back(pipes.primitiveIdPipe);

    return pipes;
  }

  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}
  bool SplitSecondary() { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
  }

  uint32_t GetEventOffset(uint32_t eid)
  {
    auto it = m_EventIndices.find(eid);
    RDCASSERT(it != m_EventIndices.end());
    return it->second;
  }

private:
  // For each event, specifies where the occlusion query results start.
  std::map<uint32_t, uint32_t> m_EventIndices;
  // Number of fragments for each event.
  std::map<uint32_t, uint32_t> m_EventFragments;
  // Pre-modification values for events to initialize attachments to,
  // so that we can get blended post-modification values.
  std::map<uint32_t, ModificationValue> m_EventPremods;
  // Number of fragments processed so far.
  uint32_t fragsProcessed = 0;

  rdcarray<VkPipeline> m_PipesToDestroy;
};

// Callback used to determine the shader discard status for each fragment, where
// an event has multiple fragments with some being discarded in a fragment shader.
struct VulkanPixelHistoryDiscardedFragmentsCallback : VulkanPixelHistoryCallback
{
  // Key is event ID and value is a list of primitive IDs
  std::map<uint32_t, rdcarray<int32_t> > m_Events;
  VulkanPixelHistoryDiscardedFragmentsCallback(WrappedVulkan *vk,
                                               PixelHistoryShaderCache *shaderCache,
                                               const PixelHistoryCallbackInfo &callbackInfo,
                                               std::map<uint32_t, rdcarray<int32_t> > events,
                                               VkQueryPool occlusionPool)
      : VulkanPixelHistoryCallback(vk, shaderCache, callbackInfo, occlusionPool), m_Events(events)
  {
  }

  ~VulkanPixelHistoryDiscardedFragmentsCallback()
  {
    for(const VkPipeline &pipe : m_PipesToDestroy)
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), pipe, NULL);
  }

  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return;

    const rdcarray<int32_t> primIds = m_Events[eid];

    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &state = m_pDriver->GetCmdRenderState();
    // Create a pipeline with a scissor and colorWriteMask = 0, and disable all tests.
    VkPipeline newPipe = CreatePipeline(state.graphics.pipeline, eid);
    for(uint32_t i = 0; i < state.views.size(); i++)
      ScissorToPixel(state.views[i], state.scissors[i]);
    state.graphics.pipeline = GetResID(newPipe);
    state.BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);
    for(uint32_t i = 0; i < primIds.size(); i++)
    {
      uint32_t queryId = (uint32_t)m_OcclusionIndices.size();
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, queryId, 0);
      const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eid);
      uint32_t primId = primIds[i];
      // TODO once pixel history distinguishes between instances, draw only the instance for
      // this fragment
      if(drawcall->flags & DrawFlags::Indexed)
        ObjDisp(cmd)->CmdDrawIndexed(
            Unwrap(cmd), RENDERDOC_NumVerticesPerPrimitive(drawcall->topology),
            RDCMAX(1U, drawcall->numInstances),
            drawcall->indexOffset + RENDERDOC_VertexOffset(drawcall->topology, primId),
            drawcall->baseVertex, drawcall->instanceOffset);
      else
        ObjDisp(cmd)->CmdDraw(
            Unwrap(cmd), RENDERDOC_NumVerticesPerPrimitive(drawcall->topology),
            RDCMAX(1U, drawcall->numInstances),
            drawcall->vertexOffset + RENDERDOC_VertexOffset(drawcall->topology, primId),
            drawcall->instanceOffset);
      ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_OcclusionPool, queryId);

      m_OcclusionIndices[make_rdcpair<uint32_t, uint32_t>(eid, primId)] = queryId;
    }
    m_pDriver->GetCmdRenderState() = prevState;
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);
  }

  VkPipeline CreatePipeline(ResourceId pipe, uint32_t eid)
  {
    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    VkGraphicsPipelineCreateInfo pipeCreateInfo = {};
    MakeIncrementStencilPipelineCI(eid, pipe, pipeCreateInfo, stages, false);

    {
      VkPipelineDepthStencilStateCreateInfo *ds =
          (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
      ds->stencilTestEnable = VK_FALSE;
    }

    VkPipeline newPipe;
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL, &newPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    m_PipesToDestroy.push_back(newPipe);
    return newPipe;
  }

  void FetchOcclusionResults()
  {
    m_OcclusionResults.resize(m_OcclusionIndices.size());
    VkResult vkr = ObjDisp(m_pDriver->GetDev())
                       ->GetQueryPoolResults(Unwrap(m_pDriver->GetDev()), m_OcclusionPool, 0,
                                             (uint32_t)m_OcclusionIndices.size(),
                                             m_OcclusionResults.byteSize(),
                                             m_OcclusionResults.data(), sizeof(uint64_t),
                                             VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  bool PrimitiveDiscarded(uint32_t eid, uint32_t primId)
  {
    auto it = m_OcclusionIndices.find(make_rdcpair<uint32_t, uint32_t>(eid, primId));
    if(it == m_OcclusionIndices.end())
      return false;
    return m_OcclusionResults[it->second] == 0;
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) {}
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}
  bool SplitSecondary() { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
  }

private:
  std::map<rdcpair<uint32_t, uint32_t>, uint32_t> m_OcclusionIndices;
  rdcarray<uint64_t> m_OcclusionResults;

  rdcarray<VkPipeline> m_PipesToDestroy;
};

bool VulkanDebugManager::PixelHistorySetupResources(PixelHistoryResources &resources,
                                                    VkImage targetImage, VkExtent3D extent,
                                                    VkFormat format, VkSampleCountFlagBits samples,
                                                    const Subresource &sub, uint32_t numEvents)
{
  VkMarkerRegion region(StringFormat::Fmt("PixelHistorySetupResources %ux%ux%u %s %ux MSAA",
                                          extent.width, extent.height, extent.depth,
                                          ToStr(format).c_str(), samples));
  VulkanCreationInfo::Image targetImageInfo = GetImageInfo(GetResID(targetImage));

  VkImage colorImage;
  VkImageView colorImageView;

  VkImage dsImage;
  VkImageView dsImageView;

  VkDeviceMemory gpuMem;

  VkBuffer dstBuffer;
  VkDeviceMemory bufferMemory;

  VkResult vkr;
  VkDevice dev = m_pDriver->GetDev();

  VkDeviceSize totalMemorySize = 0;

  VkFormat dsFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

  if(!(m_pDriver->GetFormatProperties(dsFormat).optimalTilingFeatures &
       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    dsFormat = VK_FORMAT_D24_UNORM_S8_UINT;

  RDCDEBUG("Using depth-stencil format %s", ToStr(dsFormat).c_str());

  // Create Images
  VkImageCreateInfo imgInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imgInfo.imageType = VK_IMAGE_TYPE_2D;
  imgInfo.mipLevels = targetImageInfo.mipLevels;
  imgInfo.arrayLayers = targetImageInfo.arrayLayers;
  imgInfo.samples = samples;
  imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // Device local resources:
  imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  imgInfo.extent.width = extent.width;
  imgInfo.extent.height = extent.height;
  imgInfo.extent.depth = 1;
  imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if(samples != VK_SAMPLE_COUNT_1_BIT)
    imgInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &colorImage);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ImageState colorImageState = ImageState(colorImage, ImageInfo(imgInfo), eFrameRef_None);

  VkMemoryRequirements colorImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, colorImage, &colorImageMrq);
  totalMemorySize = colorImageMrq.size;

  imgInfo.format = dsFormat;
  imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &dsImage);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ImageState stencilImageState = ImageState(dsImage, ImageInfo(imgInfo), eFrameRef_None);

  VkMemoryRequirements stencilImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, dsImage, &stencilImageMrq);
  VkDeviceSize offset = AlignUp(totalMemorySize, stencilImageMrq.alignment);
  totalMemorySize = offset + stencilImageMrq.size;

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, totalMemorySize,
      m_pDriver->GetGPULocalMemoryIndex(colorImageMrq.memoryTypeBits),
  };
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &gpuMem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindImageMemory(m_Device, colorImage, gpuMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindImageMemory(m_Device, dsImage, gpuMem, offset);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = colorImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  if(samples != VK_SAMPLE_COUNT_1_BIT)
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &colorImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = dsImage;
  viewInfo.format = dsFormat;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &dsImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  // TODO: the size for memory is calculated to fit pre and post modification values and
  // stencil values. But we might run out of space when getting per fragment data.
  bufferInfo.size = AlignUp((uint32_t)(numEvents * sizeof(EventInfo)), 4096U);
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

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
  ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(dstBuffer), 0, VK_WHOLE_SIZE, 0);
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
  resources.dsFormat = dsFormat;
  resources.dsImage = dsImage;
  resources.dsImageView = dsImageView;
  resources.gpuMem = gpuMem;

  resources.bufferMemory = bufferMemory;
  resources.dstBuffer = dstBuffer;

  return true;
}

VkDescriptorSet VulkanReplay::GetPixelHistoryDescriptor()
{
  VkDescriptorSet descSet;

  VkDescriptorSetAllocateInfo descSetAllocInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      NULL,
      m_PixelHistory.MSCopyDescPool,
      1,
      &m_PixelHistory.MSCopyDescSetLayout,
  };

  // don't expect this to fail (or if it does then it should be immediately obvious, not transient).
  VkResult vkr =
      m_pDriver->vkAllocateDescriptorSets(m_pDriver->GetDev(), &descSetAllocInfo, &descSet);
  if(vkr != VK_SUCCESS)
    RDCERR("Failed creating object");
  return descSet;
}

void VulkanReplay::ResetPixelHistoryDescriptorPool()
{
  m_pDriver->vkResetDescriptorPool(m_pDriver->GetDev(), m_PixelHistory.MSCopyDescPool, 0);
}

void VulkanReplay::UpdatePixelHistoryDescriptor(VkDescriptorSet descSet, VkBuffer buffer,
                                                VkImageView imgView1, VkImageView imgView2)
{
  VkDescriptorBufferInfo destdesc = {0};
  destdesc.buffer = Unwrap(buffer);
  destdesc.range = VK_WHOLE_SIZE;

  {
    VkDescriptorImageInfo srcdesc = {};
    srcdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    srcdesc.imageView = Unwrap(imgView1);
    srcdesc.sampler = Unwrap(m_General.PointSampler);    // not used - we use texelFetch

    VkDescriptorImageInfo srcdesc2 = {};
    srcdesc2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if(imgView2 != VK_NULL_HANDLE)
      srcdesc2.imageView = Unwrap(imgView2);
    else
      srcdesc2.imageView = Unwrap(imgView1);
    srcdesc2.sampler = Unwrap(m_General.PointSampler);

    VkWriteDescriptorSet writeSet[] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 0, 0, 1,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc, NULL, NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 1, 0, 1,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &srcdesc2, NULL, NULL},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(descSet), 2, 0, 1,
         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &destdesc, NULL},
    };

    ObjDisp(m_pDriver->GetDev())
        ->UpdateDescriptorSets(Unwrap(m_pDriver->GetDev()), ARRAY_COUNT(writeSet), writeSet, 0, NULL);
  }
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
  if(r.dsImage != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImage(dev, r.dsImage, NULL);
  if(r.dsImageView != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImageView(dev, r.dsImageView, NULL);
  if(r.dstBuffer != VK_NULL_HANDLE)
    m_pDriver->vkDestroyBuffer(dev, r.dstBuffer, NULL);
  if(r.bufferMemory != VK_NULL_HANDLE)
    m_pDriver->vkFreeMemory(dev, r.bufferMemory, NULL);
  return true;
}

void CreateOcclusionPool(WrappedVulkan *vk, uint32_t poolSize, VkQueryPool *pQueryPool)
{
  VkMarkerRegion region(StringFormat::Fmt("CreateOcclusionPool %u", poolSize));

  VkDevice dev = vk->GetDev();
  VkQueryPoolCreateInfo occlusionPoolCreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  occlusionPoolCreateInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
  occlusionPoolCreateInfo.queryCount = poolSize;
  // TODO: check that occlusion feature is available
  VkResult vkr =
      ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &occlusionPoolCreateInfo, NULL, pQueryPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
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
  VkImageLayout ret = VK_IMAGE_LAYOUT_UNDEFINED;

  auto state = m_pDriver->FindConstImageState(image);
  if(!state)
  {
    RDCERR("Could not find image state for %s", ToStr(image).c_str());
    return ret;
  }

  if(state->GetImageInfo().extent.depth > 1)
    ret = state->GetImageLayout(aspect, mip, 0);
  else
    ret = state->GetImageLayout(aspect, mip, slice);

  SanitiseReplayImageLayout(ret);

  return ret;
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

void FillInColor(ResourceFormat fmt, const PixelHistoryValue &value, ModificationValue &mod)
{
  FloatVector v4 = DecodeFormattedComponents(fmt, value.color);
  memcpy(mod.col.floatValue, &v4.x, sizeof(v4));
}

float GetDepthValue(VkFormat depthFormat, const PixelHistoryValue &value)
{
  FloatVector v4 = DecodeFormattedComponents(MakeResourceFormat(depthFormat), (byte *)&value.depth);
  return v4.x;
}

rdcarray<PixelModification> VulkanReplay::PixelHistory(rdcarray<EventUsage> events,
                                                       ResourceId target, uint32_t x, uint32_t y,
                                                       const Subresource &sub, CompType typeCast)
{
  if(!GetAPIProperties().pixelHistory)
  {
    VULKANNOTIMP("PixelHistory");
    return rdcarray<PixelModification>();
  }

  rdcarray<PixelModification> history;

  if(events.empty())
    return history;

  const VulkanCreationInfo::Image &imginfo = GetDebugManager()->GetImageInfo(target);
  if(imginfo.format == VK_FORMAT_UNDEFINED)
    return history;

  rdcstr regionName = StringFormat::Fmt(
      "PixelHistory: pixel: (%u, %u) on %s subresource (%u, %u, %u) cast to %s with %zu events", x, y,
      ToStr(target).c_str(), sub.mip, sub.slice, sub.sample, ToStr(typeCast).c_str(), events.size());

  RDCDEBUG("%s", regionName.c_str());

  VkMarkerRegion region(regionName);

  uint32_t sampleIdx = sub.sample;

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
  // TODO: perhaps should do this after making an occlusion query, since we will
  // get a smaller subset of events that passed the occlusion query.
  VkImage targetImage = GetResourceManager()->GetCurrentHandle<VkImage>(target);
  GetDebugManager()->PixelHistorySetupResources(resources, targetImage, imginfo.extent,
                                                imginfo.format, imginfo.samples, sub,
                                                (uint32_t)events.size());

  PixelHistoryShaderCache *shaderCache = new PixelHistoryShaderCache(m_pDriver);

  PixelHistoryCallbackInfo callbackInfo = {};
  callbackInfo.targetImage = targetImage;
  callbackInfo.targetImageFormat = imginfo.format;
  callbackInfo.layers = imginfo.arrayLayers;
  callbackInfo.mipLevels = imginfo.mipLevels;
  callbackInfo.samples = imginfo.samples;
  callbackInfo.extent = imginfo.extent;
  callbackInfo.targetSubresource = sub;
  callbackInfo.x = x;
  callbackInfo.y = y;
  callbackInfo.sampleMask = sampleMask;
  callbackInfo.subImage = resources.colorImage;
  callbackInfo.subImageView = resources.colorImageView;
  callbackInfo.dsImage = resources.dsImage;
  callbackInfo.dsFormat = resources.dsFormat;
  callbackInfo.dsImageView = resources.dsImageView;
  callbackInfo.dstBuffer = resources.dstBuffer;

  VulkanOcclusionCallback occlCb(m_pDriver, shaderCache, callbackInfo, occlusionPool, events);
  {
    VkMarkerRegion occlRegion("VulkanOcclusionCallback");
    m_pDriver->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
    occlCb.FetchOcclusionResults();
  }

  // Gather all draw events that could have written to pixel for another replay pass,
  // to determine if these draws failed for some reason (for ex., depth test).
  rdcarray<uint32_t> modEvents;
  rdcarray<uint32_t> drawEvents;
  for(size_t ev = 0; ev < events.size(); ev++)
  {
    bool clear = (events[ev].usage == ResourceUsage::Clear);
    bool directWrite = isDirectWrite(events[ev].usage);

    if(events[ev].view != ResourceId())
    {
      // TODO: Check that the slice and mip matches.
      VulkanCreationInfo::ImageView viewInfo =
          m_pDriver->GetDebugManager()->GetImageViewInfo(events[ev].view);
      uint32_t layerEnd = viewInfo.range.baseArrayLayer + viewInfo.range.layerCount;
      if(sub.slice < viewInfo.range.baseArrayLayer || sub.slice >= layerEnd)
      {
        RDCDEBUG("Usage %d at %u didn't refer to the matching mip/slice (%u/%u)", events[ev].usage,
                 events[ev].eventId, sub.mip, sub.slice);
        continue;
      }
    }

    if(directWrite || clear)
    {
      modEvents.push_back(events[ev].eventId);
    }
    else
    {
      uint64_t occlData = occlCb.GetOcclusionResult((uint32_t)events[ev].eventId);
      VkMarkerRegion::Set(StringFormat::Fmt("%u has occl %llu", events[ev].eventId, occlData));
      if(occlData > 0)
      {
        drawEvents.push_back(events[ev].eventId);
        modEvents.push_back(events[ev].eventId);
      }
    }
  }

  VulkanColorAndStencilCallback cb(m_pDriver, shaderCache, callbackInfo, modEvents);
  {
    VkMarkerRegion colorStencilRegion("VulkanColorAndStencilCallback");
    m_pDriver->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  // If there are any draw events, do another replay pass, in order to figure out
  // which tests failed for each draw event.
  TestsFailedCallback *tfCb = NULL;
  if(drawEvents.size() > 0)
  {
    VkMarkerRegion testsRegion("TestsFailedCallback");
    VkQueryPool tfOcclusionPool;
    CreateOcclusionPool(m_pDriver, (uint32_t)drawEvents.size() * 6, &tfOcclusionPool);

    tfCb = new TestsFailedCallback(m_pDriver, shaderCache, callbackInfo, tfOcclusionPool, drawEvents);
    m_pDriver->ReplayLog(0, events.back().eventId, eReplay_Full);
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
    tfCb->FetchOcclusionResults();
    ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), tfOcclusionPool, NULL);
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
        VkMarkerRegion::Set(StringFormat::Fmt("%u has flags %x", eventId, flags));
        if(flags & TestMustFail_Culling)
          mod.backfaceCulled = true;
        if(flags & TestMustFail_DepthTesting)
          mod.depthTestFailed = true;
        if(flags & TestMustFail_Scissor)
          mod.scissorClipped = true;
        if(flags & TestMustFail_SampleMask)
          mod.sampleMasked = true;
        if(flags & UnboundFragmentShader)
          mod.unboundPS = true;

        UpdateTestsFailed(tfCb, eventId, flags, mod);
      }
      history.push_back(mod);
    }
  }

  SAFE_DELETE(tfCb);

  // Try to read memory back

  EventInfo *eventsInfo;
  VkResult vkr =
      m_pDriver->vkMapMemory(dev, resources.bufferMemory, 0, VK_WHOLE_SIZE, 0, (void **)&eventsInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  std::map<uint32_t, uint32_t> eventsWithFrags;
  std::map<uint32_t, ModificationValue> eventPremods;
  ResourceFormat fmt = MakeResourceFormat(imginfo.format);

  for(size_t h = 0; h < history.size();)
  {
    PixelModification &mod = history[h];

    int32_t eventIndex = cb.GetEventIndex(mod.eventId);
    if(eventIndex == -1)
    {
      // There is no information, skip the event.
      h++;
      continue;
    }
    const EventInfo &ei = eventsInfo[eventIndex];
    FillInColor(fmt, ei.premod, mod.preMod);
    FillInColor(fmt, ei.postmod, mod.postMod);
    VkFormat depthFormat = cb.GetDepthFormat(mod.eventId);
    if(depthFormat != VK_FORMAT_UNDEFINED)
    {
      mod.preMod.stencil = ei.premod.stencil;
      mod.postMod.stencil = ei.postmod.stencil;
      if(multisampled)
      {
        mod.preMod.depth = ei.premod.depth.fdepth;
        mod.postMod.depth = ei.postmod.depth.fdepth;
      }
      else
      {
        mod.preMod.depth = GetDepthValue(depthFormat, ei.premod);
        mod.postMod.depth = GetDepthValue(depthFormat, ei.postmod);
      }
    }

    int32_t frags = int32_t(ei.dsWithoutShaderDiscard[4]);
    int32_t fragsClipped = int32_t(ei.dsWithShaderDiscard[4]);
    mod.shaderOut.col.intValue[0] = frags;
    mod.shaderOut.col.intValue[1] = fragsClipped;
    bool someFragsClipped = (fragsClipped < frags);
    mod.primitiveID = someFragsClipped;
    // Draws in secondary command buffers will fail this check,
    // so nothing else needs to be checked in the callback itself.
    if(frags > 0)
    {
      eventsWithFrags[mod.eventId] = frags;
      eventPremods[mod.eventId] = mod.preMod;
    }

    for(int32_t f = 1; f < frags; f++)
    {
      history.insert(h + 1, mod);
    }
    for(int32_t f = 0; f < frags; f++)
      history[h + f].fragIndex = f;
    h += RDCMAX(1, frags);
    RDCDEBUG(
        "PixelHistory event id: %u, fixed shader stencilValue = %u, original shader stencilValue = "
        "%u",
        mod.eventId, ei.dsWithoutShaderDiscard[4], ei.dsWithShaderDiscard[4]);
  }
  m_pDriver->vkUnmapMemory(dev, resources.bufferMemory);

  if(eventsWithFrags.size() > 0)
  {
    // Replay to get shader output value, post modification value and primitive ID for every
    // fragment.
    VulkanPixelHistoryPerFragmentCallback perFragmentCB(m_pDriver, shaderCache, callbackInfo,
                                                        eventsWithFrags, eventPremods);
    {
      VkMarkerRegion perFragmentRegion("VulkanPixelHistoryPerFragmentCallback");
      m_pDriver->ReplayLog(0, eventsWithFrags.rbegin()->first, eReplay_Full);
      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    PerFragmentInfo *bp = NULL;
    vkr = m_pDriver->vkMapMemory(dev, resources.bufferMemory, 0, VK_WHOLE_SIZE, 0, (void **)&bp);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // Retrieve primitive ID values where fragment shader discarded some
    // fragments. For these primitives we are going to perform an occlusion
    // query to see if a primitive was discarded.
    std::map<uint32_t, rdcarray<int32_t> > discardedPrimsEvents;
    uint32_t primitivesToCheck = 0;
    for(size_t h = 0; h < history.size(); h++)
    {
      uint32_t eid = history[h].eventId;
      if(eventsWithFrags.find(eid) == eventsWithFrags.end())
        continue;
      uint32_t f = history[h].fragIndex;
      bool someFragsClipped = (history[h].primitiveID == 1);
      int32_t primId = bp[perFragmentCB.GetEventOffset(eid) + f].primitiveID;
      history[h].primitiveID = primId;
      if(someFragsClipped)
      {
        discardedPrimsEvents[eid].push_back(primId);
        primitivesToCheck++;
      }
    }

    // without the geometry shader feature we can't get the primitive ID, so we can't establish
    // discard per-primitive so we assume all shaders don't discard.
    if(m_pDriver->GetDeviceEnabledFeatures().geometryShader)
    {
      if(primitivesToCheck > 0)
      {
        VkMarkerRegion discardedRegion("VulkanPixelHistoryDiscardedFragmentsCallback");
        VkQueryPool occlPool;
        CreateOcclusionPool(m_pDriver, primitivesToCheck, &occlPool);

        // Replay to see which primitives were discarded.
        VulkanPixelHistoryDiscardedFragmentsCallback discardedCb(
            m_pDriver, shaderCache, callbackInfo, discardedPrimsEvents, occlPool);
        m_pDriver->ReplayLog(0, eventsWithFrags.rbegin()->first, eReplay_Full);
        m_pDriver->SubmitCmds();
        m_pDriver->FlushQ();
        discardedCb.FetchOcclusionResults();
        ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), occlPool, NULL);

        for(size_t h = 0; h < history.size(); h++)
          history[h].shaderDiscarded =
              discardedCb.PrimitiveDiscarded(history[h].eventId, history[h].primitiveID);
      }
    }
    else
    {
      // mark that we have no primitive IDs
      for(size_t h = 0; h < history.size(); h++)
        history[h].primitiveID = ~0U;
    }

    uint32_t discardOffset = 0;
    ResourceFormat shaderOutFormat = MakeResourceFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
    for(size_t h = 0; h < history.size(); h++)
    {
      uint32_t eid = history[h].eventId;
      uint32_t f = history[h].fragIndex;
      // Reset discard offset if this is a new event.
      if(h > 0 && (eid != history[h - 1].eventId))
        discardOffset = 0;
      if(eventsWithFrags.find(eid) != eventsWithFrags.end())
      {
        if(history[h].shaderDiscarded)
        {
          discardOffset++;
          // Copy previous post-mod value if its not the first event
          if(h > 0)
            history[h].postMod = history[h - 1].postMod;
          continue;
        }
        uint32_t offset = perFragmentCB.GetEventOffset(eid) + f - discardOffset;
        FillInColor(shaderOutFormat, bp[offset].shaderOut, history[h].shaderOut);
        history[h].shaderOut.depth = bp[offset].shaderOut.depth.fdepth;

        if((h < history.size() - 1) && (history[h].eventId == history[h + 1].eventId))
        {
          // Get post-modification value if this is not the last fragment for the event.
          FillInColor(shaderOutFormat, bp[offset].postMod, history[h].postMod);
          history[h].postMod.depth = bp[offset].postMod.depth.fdepth;
        }
        // If it is not the first fragment for the event, set the preMod to the
        // postMod of the previous fragment.
        if(h > 0 && (history[h].eventId == history[h - 1].eventId))
        {
          history[h].preMod = history[h - 1].postMod;
        }
      }
    }
  }

  GetDebugManager()->PixelHistoryDestroyResources(resources);
  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), occlusionPool, NULL);
  delete shaderCache;

  return history;
}
