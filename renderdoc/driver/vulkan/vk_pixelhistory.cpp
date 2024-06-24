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

/*
 * The general algorithm for pixel history is this:
 *
 * we get passed a list of all events that could have touched the target texture
 * We replay all events (up to and including the last event that could have
 * touched the target texture) with a number of callbacks:
 *
 * - First callback: Occlusion callback (VulkanOcclusionCallback)
 * This callback performs an occlusion query around each draw event that was
 * passed in. Execute the draw with a modified pipeline that disables most tests,
 * and uses a fixed color fragment shader, so that we get a non 0 occlusion
 * result even if a test failed for the event.
 *
 * After this callback we collect all events where occlusion result > 0 and all
 * other non-draw events (copy, render pass boundaries, resolve). We also filter
 * out events where the image view used did not overlap in the array layer.
 * The callbacks below will only deal with these events.
 *
 * - Second callback: Color and stencil callback (VulkanColorAndStencilCallback)
 * This callback retrieves color/depth values before and after each event, and
 * uses a stencil increment to count the number of fragments for each event.
 * We have to stop the render pass before and after each event if there is one active,
 * since we can't run vkCmdCopy* or vkCmdExecuteCommands inside a render pass.
 * We then copy colour information and associated depth value, and resume a
 * render pass, if there was one. Before each draw event we also execute the same
 * draw twice with a stencil increment state: 1) with a fixed color fragment shader
 * to count the number of fragments not accounting for shader discard, 2) with the
 * original fragment shader to count the number of fragments accounting for shader
 * discard.
 *
 * - Third callback: Tests failed callback (TestsFailedCallback)
 * This callback is used to determine which tests (culling, depth, stencil, etc)
 * failed (if any) for each draw event. This replays each draw event a number of times
 * with an occlusion query for each test that might have failed (leaves the test
 * under question in the original state, and disables all tests that come after).
 *
 * At this point we retrieve the stencil results that represent the number of fragments,
 * and duplicate events that have multiple fragments.
 *
 * - Fourth callback: Per fragment callback (VulkanPixelHistoryPerFragmentCallback)
 * This callback is used to get per fragment data for each event and fragment (primitive ID,
 * shader output value, post event value for each fragment).
 * For each fragment the draw is replayed 3 times:
 * 1) with a fragment shader that outputs primitive ID only
 * 2) with blending OFF, to get shader output value
 * 3) with blending ON, to get post modification value
 * For each such replay we set the stencil reference to the fragment number and set the
 * stencil compare to equal, so it passes for that particular fragment only.
 *
 * - Fifth callback: Discarded fragments callback (VulkanPixelHistoryDiscardedFragmentsCallback)
 * This callback is used to determine which individual fragments were discarded in a fragment
 * shader.
 * Only runs for the events where the number of fragments accounting for shader discard is less
 * that the number of fragments not accounting for shader discard.
 * This replays the particular fragment (by adjusting parameters in vkCmdDraw* call) with an
 * occlusion query.
 *
 * We slot the per frament data correctly accounting for the fragments that were discarded.
 *
 * Current Limitations:
 *
 * - Multiple subpasses
 * Currently if there are multiple subpasses used in a single render pass, pixel history will
 * return only partial information. This is primarily because current implementation relies
 * on stopping/resuming render passes. This only afects VulkanColorAndStencilCallback and
 * VulkanPixelHistoryPerFragmentCallback callbacks, since they rely on copy operations.
 * To support multiple subpasses we will have to:
 * Create a mirror render pass for each render pass we need to stop, where the all
 * loadOps are set to VK_ATTACHMENT_LOAD_OP_LOAD and store ops are set to
 * VK_ATTACHMENT_STORE_OP_STORE.
 * When we need to stop a render passs, we will repeatedly call vkCmdNextSubpass until we are
 * on the last subpass. When we resume the subpass we will call vkCmdNextSubpass until we reach
 * the original subpass. BeginRenderPassAndApplyState can be extended to provide an option
 * not to override the used render pass (right now overrides with the load RP that has a single
 * subpass).
 */

#include <float.h>
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "maths/formatpacking.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

static bool IsDirectWrite(ResourceUsage usage)
{
  return ((usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::CS_RWResource) ||
          usage == ResourceUsage::CopyDst || usage == ResourceUsage::Copy ||
          usage == ResourceUsage::Resolve || usage == ResourceUsage::ResolveDst ||
          usage == ResourceUsage::GenMips);
}

enum : uint32_t
{
  TestEnabled_DepthClipping = 1 << 0,
  TestEnabled_Culling = 1 << 1,
  TestEnabled_Scissor = 1 << 2,
  TestEnabled_SampleMask = 1 << 3,
  TestEnabled_DepthBounds = 1 << 4,
  TestEnabled_StencilTesting = 1 << 5,
  TestEnabled_DepthTesting = 1 << 6,
  TestEnabled_FragmentDiscard = 1 << 7,

  Blending_Enabled = 1 << 8,
  UnboundFragmentShader = 1 << 9,
  TestMustFail_Culling = 1 << 10,
  TestMustFail_Scissor = 1 << 11,
  TestMustPass_Scissor = 1 << 12,
  TestMustFail_DepthTesting = 1 << 13,
  TestMustFail_StencilTesting = 1 << 14,
  TestMustFail_SampleMask = 1 << 15,

  DepthTest_Shift = 29,
  DepthTest_Always = 0U << DepthTest_Shift,
  DepthTest_Never = 1U << DepthTest_Shift,
  DepthTest_Equal = 2U << DepthTest_Shift,
  DepthTest_NotEqual = 3U << DepthTest_Shift,
  DepthTest_Less = 4U << DepthTest_Shift,
  DepthTest_LessEqual = 5U << DepthTest_Shift,
  DepthTest_Greater = 6U << DepthTest_Shift,
  DepthTest_GreaterEqual = 7U << DepthTest_Shift,
};

struct VkCopyPixelParams
{
  VkImage srcImage;
  VkFormat srcImageFormat;
  VkImageLayout srcImageLayout;
  bool multisampled;
  bool multiview;
  Subresource sub;
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
  PixelHistoryShaderCache(WrappedVulkan *vk) : m_pDriver(vk)
  {
    if(m_pDriver->GetDriverInfo().IntelBrokenOcclusionQueries())
    {
      if(m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address)
      {
        dummybuf.Create(vk, vk->GetDev(), 1024, 1,
                        GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO |
                            GPUBuffer::eGPUBufferAddressable);
      }
      else
      {
        m_pDriver->AddDebugMessage(
            MessageCategory::Miscellaneous, MessageSeverity::High, MessageSource::RuntimeWarning,
            "Intel drivers currently require a workaround to return proper pixel history results, "
            "but KHR_buffer_device_address is not supported so the workaround cannot be "
            "implemented. Results will be inaccurate.");
      }
    }
  }

  ~PixelHistoryShaderCache()
  {
    if(dummybuf.device != VK_NULL_HANDLE)
      dummybuf.Destroy();
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
    PatchOutputLocation(sh, BuiltinShader::FixedColFS, framebufferIndex);
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
    PatchOutputLocation(sh, BuiltinShader::PixelHistoryPrimIDFS, framebufferIndex);
    m_PrimIDFS.insert(std::make_pair(framebufferIndex, sh));
    return sh;
  }

  // Returns a shader that is equivalent to the given shader, but attempts to remove
  // side effects of shader execution for the given entry point (for ex., writes
  // to storage buffers/images).
  VkShaderModule GetShaderWithoutSideEffects(ResourceId shaderId, const rdcstr &entryPoint,
                                             ShaderStage stage)
  {
    ShaderKey shaderKey = make_rdcpair(shaderId, entryPoint);
    auto it = m_ShaderReplacements.find(shaderKey);
    // Check if we processed this shader before.
    if(it != m_ShaderReplacements.end())
      return it->second;

    VkShaderModule shaderModule = CreateShaderReplacement(shaderId, entryPoint, stage);
    m_ShaderReplacements.insert(std::make_pair(shaderKey, shaderModule));
    return shaderModule;
  }

private:
  VkShaderModule CreateShaderReplacement(ResourceId shaderId, const rdcstr &entryName,
                                         ShaderStage stage)
  {
    const VulkanCreationInfo::ShaderModule &moduleInfo =
        m_pDriver->GetDebugManager()->GetShaderInfo(shaderId);
    rdcarray<uint32_t> modSpirv = moduleInfo.spirv.GetSPIRV();

    bool modified = false;
    bool found = false;

    {
      rdcspv::Editor editor(modSpirv);
      editor.Prepare();

      for(const rdcspv::EntryPoint &entry : editor.GetEntries())
      {
        if(entry.name == entryName && MakeShaderStage(entry.executionModel) == stage)
        {
          // In some cases a shader might just be binding a RW resource but not writing to it.
          // If there are no writes (shader was not modified), no need to replace the shader,
          // just insert VK_NULL_HANDLE to indicate that this shader has been processed.
          found = true;
          modified = StripShaderSideEffects(editor, entry.id);
          break;
        }
      }
    }

    if(found)
    {
      VkShaderModule module = VK_NULL_HANDLE;
      if(modified)
      {
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.pCode = modSpirv.data();
        moduleCreateInfo.codeSize = modSpirv.byteSize();
        VkResult vkr =
            m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &moduleCreateInfo, NULL, &module);
        m_pDriver->CheckVkResult(vkr);
      }

      return module;
    }

    RDCERR("Entry point %s not found", entryName.c_str());
    return VK_NULL_HANDLE;
  }

  void PatchOutputLocation(VkShaderModule &mod, BuiltinShader shaderType, uint32_t framebufferIndex)
  {
    rdcarray<uint32_t> spv = *m_pDriver->GetShaderCache()->GetBuiltinBlob(shaderType);

    bool patched = false;

    {
      rdcspv::Editor editor(spv);
      editor.Prepare();

      for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                       end = editor.End(rdcspv::Section::Annotations);
          it < end; ++it)
      {
        if(it.opcode() == rdcspv::Op::Decorate)
        {
          rdcspv::OpDecorate dec(it);
          if(dec.decoration == rdcspv::Decoration::Location)
          {
            dec.decoration.location = framebufferIndex;
            it = dec;

            patched = true;
            break;
          }
        }
      }

      // implement workaround for Intel drivers to force shader to have unimportant side-effects
      if(dummybuf.device != VK_NULL_HANDLE)
      {
        VkBufferDeviceAddressInfo getAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        getAddressInfo.buffer = dummybuf.buf;

        VkDeviceAddress bufferAddress =
            m_pDriver->vkGetBufferDeviceAddress(m_pDriver->GetDev(), &getAddressInfo);

        rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
        rdcspv::Id bufptrtype = editor.DeclareType(
            rdcspv::Pointer(uint32Type, rdcspv::StorageClass::PhysicalStorageBuffer));

        rdcspv::Id uint1 = editor.AddConstantImmediate<uint32_t>(uint32_t(1));

        editor.AddExtension("SPV_KHR_physical_storage_buffer");

        {
          // change the memory model to physical storage buffer 64
          rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
          rdcspv::OpMemoryModel model(it);
          model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
          it = model;
        }

        editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);

        rdcspv::Id addressConstantLSB =
            editor.AddConstantImmediate<uint32_t>(bufferAddress & 0xFFFFFFFF);
        rdcspv::Id addressConstantMSB =
            editor.AddConstantImmediate<uint32_t>((bufferAddress >> 32) & 0xFFFFFFFF);

        rdcspv::Id uintPair = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 2));

        rdcspv::Id addressConstant = editor.AddConstant(rdcspv::OpSpecConstantComposite(
            uintPair, editor.MakeId(), {addressConstantLSB, addressConstantMSB}));

        rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Device);
        rdcspv::Id semantics =
            editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::MemorySemantics::AcquireRelease);

        // patch every function to include a BDA write just to be safe
        for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions),
                         end = editor.End(rdcspv::Section::Functions);
            it < end; ++it)
        {
          if(it.opcode() == rdcspv::Op::Function)
          {
            // continue to the first label so we can insert a write at the start of the function
            for(; it; ++it)
            {
              if(it.opcode() == rdcspv::Op::Label)
              {
                ++it;
                break;
              }
            }

            // skip past any local variables
            while(it.opcode() == rdcspv::Op::Variable || it.opcode() == rdcspv::Op::Line ||
                  it.opcode() == rdcspv::Op::NoLine)
              ++it;

            rdcspv::Id structPtr = editor.AddOperation(
                it, rdcspv::OpBitcast(bufptrtype, editor.MakeId(), addressConstant));
            it++;

            editor.AddOperation(it, rdcspv::OpAtomicUMax(uint32Type, editor.MakeId(), structPtr,
                                                         scope, semantics, uint1));
            it++;
          }
        }
      }
    }

    if(!patched)
      RDCERR("Didn't patch the output location");

    VkShaderModuleCreateInfo modinfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        NULL,
        0,
        spv.size() * sizeof(uint32_t),
        spv.data(),
    };

    VkResult vkr = m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &modinfo, NULL, &mod);
    m_pDriver->CheckVkResult(vkr);
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

  GPUBuffer dummybuf;
};

// VulkanPixelHistoryCallback is a generic VulkanActionCallback that can be used for
// pixel history replays.
struct VulkanPixelHistoryCallback : public VulkanActionCallback
{
  VulkanPixelHistoryCallback(WrappedVulkan *vk, PixelHistoryShaderCache *shaderCache,
                             const PixelHistoryCallbackInfo &callbackInfo, VkQueryPool occlusionPool)
      : m_pDriver(vk),
        m_ShaderCache(shaderCache),
        m_CallbackInfo(callbackInfo),
        m_OcclusionPool(occlusionPool)
  {
    m_pDriver->SetActionCB(this);

    if(m_pDriver->GetDeviceEnabledFeatures().occlusionQueryPrecise)
      m_QueryFlags |= VK_QUERY_CONTROL_PRECISE_BIT;
  }

  virtual ~VulkanPixelHistoryCallback()
  {
    m_pDriver->SetActionCB(NULL);
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
  VkImageLayout GetImageLayout(ResourceId id, VkImageAspectFlagBits aspect, const Subresource &sub)
  {
    const ImageState *latestState = m_pDriver->GetRecordingLayoutWithinActionCallback(id);

    VkImageLayout ret = VK_IMAGE_LAYOUT_UNDEFINED;

    if(latestState)
    {
      for(auto it = latestState->subresourceStates.begin();
          it != latestState->subresourceStates.end(); ++it)
      {
        const ImageSubresourceRange &range = it->range();
        if(VkImageAspectFlagBits(range.aspectMask & aspect) == aspect &&
           // check slice (respecting layerCount == ~0U for 'all')
           range.baseArrayLayer <= sub.slice &&
           (sub.slice - range.baseArrayLayer) < range.layerCount &&
           // check mip (respecting levelCount == ~0U for 'all')
           range.baseMipLevel <= sub.mip && (sub.mip - range.baseMipLevel) < range.levelCount)
        {
          ret = it->state().newLayout;
        }
      }
    }

    if(ret == VK_IMAGE_LAYOUT_UNDEFINED)
      ret = m_pDriver->GetDebugManager()->GetImageLayout(id, aspect, sub.mip, sub.slice);

    return ret;
  }

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
                                      bool disableTests, bool patchDepthAttachment)
  {
    const VulkanCreationInfo::Pipeline &p = m_pDriver->GetDebugManager()->GetPipelineInfo(pipe);
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, pipe);

    // make state we want to control dynamic
    SetupDynamicStates(pipeCreateInfo);

    // remove any dynamic states where we want to use exactly the fixed ones we're setting up
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_STENCIL_OP);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);

    if(disableTests)
    {
      m_DynamicStates.removeOne(VK_DYNAMIC_STATE_CULL_MODE);
      m_DynamicStates.removeOne(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
    }

    ApplyDynamicStates(pipeCreateInfo);

    // need to patch the pipeline rendering info if it exists and we're patching the depth
    // attachment
    VkPipelineRenderingCreateInfo *dynRenderCreate = (VkPipelineRenderingCreateInfo *)FindNextStruct(
        &pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

    if(patchDepthAttachment && dynRenderCreate)
    {
      RDCASSERT(dynRenderCreate);

      dynRenderCreate->depthAttachmentFormat = m_CallbackInfo.dsFormat;
      dynRenderCreate->stencilAttachmentFormat = m_CallbackInfo.dsFormat;
    }

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
      {
        atts[i].colorWriteMask = 0;

        // on intel with a color write mask of 0, shader discards seem to be ignored for occlusion
        // queries
        if(m_pDriver->GetDriverInfo().IntelBrokenOcclusionQueries())
        {
          // only writemask alpha just in case some other bug manifests
          atts[i].colorWriteMask = 0x8;

          // disable logic ops
          cbs->logicOpEnable = VK_FALSE;

          // set blend state to source=0 and dest=1 with ADD, which is equivalent to a disabled
          // write mask
          atts[i].srcColorBlendFactor = atts[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
          atts[i].dstColorBlendFactor = atts[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
          atts[i].colorBlendOp = atts[i].alphaBlendOp = VK_BLEND_OP_ADD;
          atts[i].blendEnable = VK_TRUE;
        }
      }
    }

    stages.resize(pipeCreateInfo.stageCount);
    memcpy(stages.data(), pipeCreateInfo.pStages, stages.byteSize());

    EventFlags eventFlags = m_pDriver->GetEventFlags(eid);
    VkShaderModule replacementShaders[NumShaderStages] = {};

    // Clean shaders
    for(size_t i = 0; i < NumShaderStages; i++)
    {
      if((eventFlags & PipeStageRWEventFlags(StageFromIndex(i))) != EventFlags::NoFlags)
        replacementShaders[i] = m_ShaderCache->GetShaderWithoutSideEffects(
            p.shaders[i].module, p.shaders[i].entryPoint, p.shaders[i].stage);
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
  void SetupDynamicStates(VkGraphicsPipelineCreateInfo &pipeCreateInfo)
  {
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

    ApplyDynamicStates(pipeCreateInfo);
  }

  void ApplyDynamicStates(VkGraphicsPipelineCreateInfo &pipeCreateInfo)
  {
    VkPipelineDynamicStateCreateInfo *dynState =
        (VkPipelineDynamicStateCreateInfo *)pipeCreateInfo.pDynamicState;

    // now point at our storage for the array
    dynState->pDynamicStates = m_DynamicStates.data();
    dynState->dynamicStateCount = (uint32_t)m_DynamicStates.size();
  }

  // PatchRenderPass creates a new VkRenderPass based on the original that has a separate
  // depth-stencil attachment, and covers a single subpass. This will be used to replay a single
  // draw. The new renderpass also replaces the depth stencil attachment, so it can be used to count
  // the number of fragments. Optionally, the new renderpass changes the format for the color image
  // that corresponds to colorIdx attachment. It also modifies the render state to use the new
  // renderpass, and returns it for further use.
  //
  // When dynamic rendering is in use, it doesn't create a new renderpass it just modifies the
  // passed in state and returns a NULL handle.
  VkRenderPass PatchRenderPass(VulkanRenderState &pipestate, bool &multiview,
                               VkFormat newColorFormat = VK_FORMAT_UNDEFINED, uint32_t attIdx = 0,
                               uint32_t colorIdx = 0)
  {
    if(pipestate.dynamicRendering.active)
    {
      VulkanRenderState::DynamicRendering &dyn = pipestate.dynamicRendering;
      // replicate work done below on renderpass objects.

      multiview = false;
      if(pipestate.dynamicRendering.viewMask > 1)
      {
        pipestate.dynamicRendering.viewMask = 1U << m_CallbackInfo.targetSubresource.slice;
        multiview = true;
      }

      // strip any resolving, change load/store ops to load/store
      for(size_t i = 0; i < dyn.color.size(); i++)
      {
        dyn.color[i].resolveMode = VK_RESOLVE_MODE_NONE;
        dyn.color[i].resolveImageView = VK_NULL_HANDLE;

        if(dyn.color[i].loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
          dyn.color[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if(dyn.color[i].storeOp != VK_ATTACHMENT_STORE_OP_NONE)
          dyn.color[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // nothing to do with newColorFormat here - we patch that when creating the pipeline
      }

      if(colorIdx >= (uint32_t)dyn.color.size())
      {
        VkRenderingAttachmentInfo c = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            NULL,
            VK_NULL_HANDLE,    // the image view will be bound in PatchFramebuffer
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_RESOLVE_MODE_NONE,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            {},
        };

        dyn.color.push_back(c);
      }

      dyn.depth.resolveMode = VK_RESOLVE_MODE_NONE;
      dyn.depth.resolveImageView = VK_NULL_HANDLE;
      dyn.depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      dyn.depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      dyn.stencil.resolveMode = VK_RESOLVE_MODE_NONE;
      dyn.stencil.resolveImageView = VK_NULL_HANDLE;
      dyn.stencil.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      dyn.stencil.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      // we'll be using our own depth/stencil attachment but it will be updated in PatchFramebuffer.
      // Just set the image layout here
      dyn.depth.imageLayout = dyn.stencil.imageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      return VK_NULL_HANDLE;
    }

    ResourceId rp = pipestate.GetRenderPass();

    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(rp);
    // TODO: this should retrieve the correct subpass, once multiple subpasses
    // are supported.
    // Currently only single subpass render passes are supported.
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();

    // Copy color and input attachments, and ignore resolve attachments.
    // Since we are only using this renderpass to replay a single draw, we don't
    // need to do resolve operations.
    rdcarray<VkAttachmentReference> colorAttachments;
    rdcarray<VkAttachmentReference> inputAttachments;
    colorAttachments.resize(sub.colorAttachments.size());
    inputAttachments.resize(sub.inputAttachments.size());

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

    rdcarray<VkAttachmentDescription> descs;
    descs.resize(rpInfo.attachments.size());
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
      if(rpInfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_NONE_KHR)
        descs[i].loadOp = VK_ATTACHMENT_LOAD_OP_NONE_KHR;
      if(rpInfo.attachments[i].storeOp == VK_ATTACHMENT_STORE_OP_NONE)
        descs[i].storeOp = VK_ATTACHMENT_STORE_OP_NONE;
      if(rpInfo.attachments[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_NONE_KHR)
        descs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_NONE_KHR;
      if(rpInfo.attachments[i].stencilStoreOp == VK_ATTACHMENT_STORE_OP_NONE)
        descs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_NONE;

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
    dsAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    dsAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    dsAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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
      if(attIdx < descs.size())
      {
        // It is an existing attachment.
        descs[attIdx].format = newColorFormat;
      }
      else
      {
        // We are adding a new color attachment.
        VkAttachmentReference attRef = {};
        attRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attRef.attachment = attIdx;
        if(colorIdx < colorAttachments.size())
          colorAttachments[colorIdx] = attRef;
        else
          colorAttachments.push_back(attRef);
        subpassDesc.colorAttachmentCount = (uint32_t)colorAttachments.size();
        subpassDesc.pColorAttachments = colorAttachments.data();

        RDCASSERT(subpassDesc.colorAttachmentCount <= 8);

        RDCASSERT(descs.size() == attIdx);
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

    uint32_t multiviewMask = 0;

    VkRenderPassMultiviewCreateInfo multiviewRP = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
    multiviewRP.correlationMaskCount = 1;
    multiviewRP.pCorrelationMasks = &multiviewMask;
    multiviewRP.subpassCount = 1;
    multiviewRP.pViewMasks = &multiviewMask;

    multiview = false;
    if(sub.multiviews.size() > 1)
    {
      multiviewMask = 1U << m_CallbackInfo.targetSubresource.slice;
      rpCreateInfo.pNext = &multiviewRP;
      multiview = true;
    }

    VkRenderPass renderpass;
    VkResult vkr =
        m_pDriver->vkCreateRenderPass(m_pDriver->GetDev(), &rpCreateInfo, NULL, &renderpass);
    m_pDriver->CheckVkResult(vkr);
    m_RpsToDestroy.push_back(renderpass);

    pipestate.SetRenderPass(GetResID(renderpass));

    return renderpass;
  }

  // PatchFramebuffer creates a new VkFramebuffer that is based on the original, but
  // substitutes the depth stencil image view. If there is no depth stencil attachment,
  // it will be added. Optionally, also substitutes the original target image view with
  // the newColorAtt. It also modifies the render state to use the new
  // framebuffer, and returns it for further use.
  //
  // When dynamic rendering is in use, it doesn't create a new framebuffer it just modifies the
  // passed in state and returns a NULL handle.
  VkFramebuffer PatchFramebuffer(VulkanRenderState &pipestate, VkRenderPass newRp,
                                 VkImageView newColorAtt = VK_NULL_HANDLE,
                                 VkFormat newColorFormat = VK_FORMAT_UNDEFINED,
                                 uint32_t attachIdx = 0, uint32_t colorIdx = 0)
  {
    if(pipestate.dynamicRendering.active)
    {
      VulkanRenderState::DynamicRendering &dyn = pipestate.dynamicRendering;

      // patch the color, if we're doing that
      if(newColorAtt != VK_NULL_HANDLE)
      {
        // colorIdx shouldn't be greater as we pushed back a new attachment in PatchRenderPass, but
        // just in case fixup here
        if(colorIdx >= (uint32_t)dyn.color.size())
          colorIdx = uint32_t(dyn.color.size() - 1);

        dyn.color[colorIdx].imageView = newColorAtt;
      }

      // patch the D/S view
      dyn.depth.imageView = dyn.stencil.imageView = m_CallbackInfo.dsImageView;

      return VK_NULL_HANDLE;
    }

    ResourceId rp = pipestate.GetRenderPass();
    ResourceId origFb = pipestate.GetFramebuffer();
    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(rp);
    // Currently only single subpass render passes are supported.
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();
    const VulkanCreationInfo::Framebuffer &fbInfo =
        m_pDriver->GetDebugManager()->GetFramebufferInfo(origFb);
    rdcarray<VkImageView> atts;
    atts.resize(fbInfo.attachments.size());

    for(uint32_t i = 0; i < fbInfo.attachments.size(); i++)
    {
      atts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(
          pipestate.GetFramebufferAttachments()[i]);
    }

    // Either modify the existing color attachment view, or add a new one.
    if(newColorAtt != VK_NULL_HANDLE)
    {
      if(attachIdx < atts.size())
        atts[attachIdx] = newColorAtt;
      else
        atts.push_back(newColorAtt);
    }

    // Either modify the existing depth stencil attachment, or add one.
    if(sub.depthstencilAttachment != -1 && (size_t)sub.depthstencilAttachment < atts.size())
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
    m_pDriver->CheckVkResult(vkr);
    m_FbsToDestroy.push_back(framebuffer);

    NameVulkanObject(
        framebuffer,
        StringFormat::Fmt("Pixel history patched framebuffer %s fmt %s",
                          newColorAtt == VK_NULL_HANDLE ? "no new attachment" : "new attachment",
                          ToStr(newColorFormat).c_str()));

    pipestate.SetFramebuffer(m_pDriver, GetResID(framebuffer));

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
      uint32_t bs = (uint32_t)GetByteSize(1, 1, 1, format, 0);

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
    m_pDriver->CheckVkResult(vkr);
    m_ImageViewsToDestroy.push_back(imageView);

    VkImageView imageView2 = VK_NULL_HANDLE;
    if(IsStencilFormat(format))
    {
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      vkr = m_pDriver->vkCreateImageView(m_pDriver->GetDev(), &viewInfo, NULL, &imageView2);
      m_pDriver->CheckVkResult(vkr);
      m_ImageViewsToDestroy.push_back(imageView2);
    }

    VkDescriptorSet descSet = m_pDriver->GetReplay()->GetPixelHistoryDescriptor();
    m_pDriver->GetReplay()->UpdatePixelHistoryDescriptor(descSet, m_CallbackInfo.dstBuffer,
                                                         imageView, imageView2);
    m_CopyDescriptors.insert(std::make_pair(image, descSet));
    return descSet;
  }

  void CopyImagePixel(VkCommandBuffer cmd, VkCopyPixelParams &p, size_t offset)
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

    uint32_t baseMip = p.sub.mip;
    uint32_t baseSlice = p.sub.slice;
    // The images that are created specifically for evaluating pixel history are
    // already based on the target mip/slice. Unless we're using multiview, in which case the output
    // is still view-dependent.
    if(!p.multiview &&
       ((p.srcImage == m_CallbackInfo.subImage) || (p.srcImage == m_CallbackInfo.dsImage)))
    {
      baseMip = 0;
      baseSlice = 0;
    }
    // For pipeline barriers.
    VkImageSubresourceRange subresource = {aspectFlags, baseMip, 1, baseSlice, 1};

    // For multi-sampled images can't call vkCmdCopyImageToBuffer directly,
    // copy using a compute shader into a staging image first.
    if(p.multisampled)
    {
      VkImageMemoryBarrier barrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
              VK_ACCESS_MEMORY_WRITE_BIT,
          VK_ACCESS_SHADER_READ_BIT,
          p.srcImageLayout,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(p.srcImage),
          subresource,
      };
      SanitiseOldImageLayout(barrier.oldLayout);
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
      SanitiseNewImageLayout(barrier.newLayout);

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
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
              VK_ACCESS_MEMORY_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT,
          p.srcImageLayout,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(p.srcImage),
          subresource,
      };
      SanitiseOldImageLayout(barrier.oldLayout);
      DoPipelineBarrier(cmd, 1, &barrier);

      ObjDisp(cmd)->CmdCopyImageToBuffer(
          Unwrap(cmd), Unwrap(p.srcImage), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          Unwrap(m_CallbackInfo.dstBuffer), (uint32_t)regions.size(), regions.data());

      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout = p.srcImageLayout;
      SanitiseNewImageLayout(barrier.newLayout);
      DoPipelineBarrier(cmd, 1, &barrier);
    }
  }

  bool HasMultipleSubpasses()
  {
    ResourceId rp = m_pDriver->GetCmdRenderState().GetRenderPass();
    if(rp == ResourceId())
      return false;
    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(rp);
    return (rpInfo.subpasses.size() > 1);
  }

  // Returns teh color attachment index that corresponds to the target image for
  // pixel history.
  uint32_t GetColorAttachmentIndex(const VulkanRenderState &renderstate,
                                   uint32_t *framebufferIndex = NULL)
  {
    uint32_t fbIndex = 0;
    const rdcarray<ResourceId> &atts = renderstate.GetFramebufferAttachments();

    for(uint32_t i = 0; i < atts.size(); i++)
    {
      ResourceId img = m_pDriver->GetDebugManager()->GetImageViewInfo(atts[i]).image;
      if(img == GetResID(m_CallbackInfo.targetImage))
      {
        fbIndex = i;
        break;
      }
    }

    if(framebufferIndex)
      *framebufferIndex = fbIndex;

    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      if(framebufferIndex && renderstate.dynamicRendering.active)
        *framebufferIndex = ~0U;
      return 0;
    }

    if(renderstate.dynamicRendering.active)
    {
      for(size_t i = 0; i < renderstate.dynamicRendering.color.size(); i++)
      {
        VkImageView v = renderstate.dynamicRendering.color[i].imageView;
        if(v != VK_NULL_HANDLE)
        {
          ResourceId img = m_pDriver->GetDebugManager()->GetImageViewInfo(GetResID(v)).image;
          if(img == GetResID(m_CallbackInfo.targetImage))
          {
            if(framebufferIndex)
              *framebufferIndex = (uint32_t)i;
            return (uint32_t)i;
          }
        }
      }
      return 0;
    }

    const VulkanCreationInfo::RenderPass &rpInfo =
        m_pDriver->GetDebugManager()->GetRenderPassInfo(renderstate.GetRenderPass());
    const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();
    for(uint32_t i = 0; i < sub.colorAttachments.size(); i++)
    {
      if(fbIndex == sub.colorAttachments[i])
        return i;
    }
    return 0;
  }

  WrappedVulkan *m_pDriver;
  VkQueryControlFlags m_QueryFlags = 0;
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

  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;
    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();

    if(prevState.graphics.shaderObject)
      return;

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
    // ensure the render state sets any dynamic state the pipeline needs
    pipestate.SetDynamicStatesFromPipeline(m_pDriver);
    ReplayDrawWithQuery(cmd, eid);

    m_pDriver->GetCmdRenderState() = prevState;
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);
  }

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return; }
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return; }
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}
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
    m_pDriver->CheckVkResult(vkr);
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
    const ActionDescription *action = m_pDriver->GetAction(eventId);
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);

    uint32_t occlIndex = (uint32_t)m_OcclusionQueries.size();
    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, occlIndex, m_QueryFlags);

    m_pDriver->ReplayDraw(cmd, *action);

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
    MakeIncrementStencilPipelineCI(eid, pipeline, pipeCreateInfo, stages, true, false);

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
    m_pDriver->CheckVkResult(vkr);
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

  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
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
    // if dynamic rendering is in use and the renderpass just suspended, we need to be sure it is
    // really finished. This will just store as we always patch the load/store ops.
    pipestate.FinishSuspendedRenderPass(cmd);

    // Get pre-modification values
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);

    CopyPixel(eid, cmd, storeOffset);

    {
      bool multiview = false;
      VkRenderPass newRp = PatchRenderPass(pipestate, multiview);
      PatchFramebuffer(pipestate, newRp);

      PipelineReplacements replacements = GetPipelineReplacements(
          eid, pipestate.graphics.pipeline, newRp, GetColorAttachmentIndex(prevState));

      for(uint32_t i = 0; i < pipestate.views.size(); i++)
        ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);

      // TODO: should fill depth value from the original DS attachment.

      // Replay the draw with a fixed color shader that never discards, and stencil
      // increment to count number of fragments. We will get the number of fragments
      // not accounting for shader discard.
      pipestate.graphics.pipeline = GetResID(replacements.fixedShaderStencil);
      pipestate.front.compare = pipestate.front.write = 0xff;
      pipestate.front.ref = 0;
      pipestate.back = pipestate.front;
      // ensure the render state sets any dynamic state the pipeline needs
      pipestate.SetDynamicStatesFromPipeline(m_pDriver);
      ReplayDraw(cmd, eid, true);

      VkCopyPixelParams params = {};
      params.srcImage = m_CallbackInfo.dsImage;
      params.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      params.srcImageFormat = m_CallbackInfo.dsFormat;
      params.multisampled = (m_CallbackInfo.samples != VK_SAMPLE_COUNT_1_BIT);
      params.multiview = multiview;
      params.sub = m_CallbackInfo.targetSubresource;
      // Copy stencil value that indicates the number of fragments ignoring
      // shader discard.
      CopyImagePixel(cmd, params, storeOffset + offsetof(struct EventInfo, dsWithoutShaderDiscard));

      // TODO: in between reset the depth value.

      // Replay the draw with the original fragment shader to get the actual number
      // of fragments, accounting for potential shader discard.
      pipestate.graphics.pipeline = GetResID(replacements.originalShaderStencil);
      // ensure the render state sets any dynamic state the pipeline needs
      pipestate.SetDynamicStatesFromPipeline(m_pDriver);
      ReplayDraw(cmd, eid, true);

      CopyImagePixel(cmd, params, storeOffset + offsetof(struct EventInfo, dsWithShaderDiscard));
    }

    // Restore the state.
    pipestate = prevState;

    // TODO: Need to re-start on the correct subpass.
    if(pipestate.graphics.pipeline != ResourceId())
      pipestate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, true);
  }

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
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
    // if dynamic rendering is in use and the renderpass just suspended, we need to be sure it is
    // really finished. This will just store as we always patch the load/store ops.
    m_pDriver->GetCmdRenderState().FinishSuspendedRenderPass(cmd);

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);

    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod));

    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
        m_pDriver, cmd, VulkanRenderState::BindGraphics, true);

    // Get post-modification values
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }

  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
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

    if(m_pDriver->GetCmdRenderState().ActiveRenderPass())
    {
      m_pDriver->GetCmdRenderState().EndRenderPass(cmd);
      // if dynamic rendering is in use and the renderpass just suspended, we need to be sure it is
      // really finished. This will just store as we always patch the load/store ops.
      m_pDriver->GetCmdRenderState().FinishSuspendedRenderPass(cmd);
    }

    // Copy
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eventId, cmd, storeOffset);
    m_EventIndices.insert(std::make_pair(eventId, m_EventIndices.size()));

    if(m_pDriver->GetCmdRenderState().ActiveRenderPass())
      m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
          m_pDriver, cmd, VulkanRenderState::BindNone, true);
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

    if(m_pDriver->GetCmdRenderState().ActiveRenderPass())
    {
      m_pDriver->GetCmdRenderState().EndRenderPass(cmd);

      // if dynamic rendering is in use and the renderpass just suspended, we need to be sure it is
      // really finished. This will just store as we always patch the load/store ops.
      m_pDriver->GetCmdRenderState().FinishSuspendedRenderPass(cmd);
    }

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

    if(m_pDriver->GetCmdRenderState().ActiveRenderPass())
      m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
          m_pDriver, cmd, VulkanRenderState::BindNone, true);
  }

  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset, false);
  }
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return false;
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod), false);
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
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
    PreDispatch(eid, flags, cmd);
  }
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
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
    if(flags & ActionFlags::BeginPass)
    {
      m_pDriver->GetCmdRenderState().EndRenderPass(cmd);

      // if dynamic rendering is in use and the renderpass just suspended, we need to be sure it is
      // really finished. This will just store as we always patch the load/store ops.
      m_pDriver->GetCmdRenderState().FinishSuspendedRenderPass(cmd);
    }

    bool ret = PostDispatch(eid, flags, cmd);

    if(flags & ActionFlags::BeginPass)
      m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
          m_pDriver, cmd, VulkanRenderState::BindNone, true);

    return ret;
  }

  bool SplitSecondary() { return true; }
  bool ForceLoadRPs() { return true; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
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
  void CopyPixel(uint32_t eid, VkCommandBuffer cmd, size_t offset, bool autoDepthCopy = true)
  {
    VkCopyPixelParams targetCopyParams = {};
    targetCopyParams.srcImage = m_CallbackInfo.targetImage;
    targetCopyParams.srcImageFormat = m_CallbackInfo.targetImageFormat;
    targetCopyParams.multisampled = (m_CallbackInfo.samples != VK_SAMPLE_COUNT_1_BIT);
    targetCopyParams.sub = m_CallbackInfo.targetSubresource;
    VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      offset += offsetof(struct PixelHistoryValue, depth);
      aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    targetCopyParams.srcImageLayout = GetImageLayout(GetResID(m_CallbackInfo.targetImage), aspect,
                                                     m_CallbackInfo.targetSubresource);
    CopyImagePixel(cmd, targetCopyParams, offset);

    // If the target image is a depth/stencil attachment, we already
    // copied the value above. Or if we are not wanting to automatically copy the depth (e.g. in the
    // case that this is a clear, dispatch, or other write where copying depth is irrelevant and the
    // pixel co-ordinate we're tracking may even be out of bounds)
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat) || autoDepthCopy == false)
      return;

    const VulkanRenderState &state = m_pDriver->GetCmdRenderState();

    ResourceId depthImageId;
    VkImageLayout depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Subresource depthSub;

    if(state.dynamicRendering.active)
    {
      VkImageView dsView = state.dynamicRendering.depth.imageView;
      depthLayout = state.dynamicRendering.depth.imageLayout;
      if(dsView == VK_NULL_HANDLE && state.dynamicRendering.stencil.imageView != VK_NULL_HANDLE)
      {
        dsView = state.dynamicRendering.stencil.imageView;
        depthLayout = state.dynamicRendering.stencil.imageLayout;
      }

      if(dsView != VK_NULL_HANDLE)
      {
        const VulkanCreationInfo::ImageView &viewInfo =
            m_pDriver->GetDebugManager()->GetImageViewInfo(GetResID(dsView));
        depthImageId = viewInfo.image;
        depthSub = {viewInfo.range.baseMipLevel, viewInfo.range.baseArrayLayer};
      }
    }
    else if(state.GetRenderPass() != ResourceId())
    {
      const VulkanCreationInfo::RenderPass &rpInfo =
          m_pDriver->GetDebugManager()->GetRenderPassInfo(state.GetRenderPass());
      const rdcarray<ResourceId> &atts = state.GetFramebufferAttachments();

      int32_t att = rpInfo.subpasses[state.subpass].depthstencilAttachment;

      if(att >= 0)
      {
        const VulkanCreationInfo::ImageView &viewInfo =
            m_pDriver->GetDebugManager()->GetImageViewInfo(atts[att]);
        depthImageId = viewInfo.image;
        depthSub = {viewInfo.range.baseMipLevel, viewInfo.range.baseArrayLayer};
        depthLayout = rpInfo.subpasses[state.subpass].depthLayout;
      }
    }

    if(depthImageId != ResourceId())
    {
      VkImage depthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(depthImageId);

      const VulkanCreationInfo::Image &imginfo =
          m_pDriver->GetDebugManager()->GetImageInfo(depthImageId);
      VkCopyPixelParams depthCopyParams = targetCopyParams;
      depthCopyParams.srcImage = depthImage;
      depthCopyParams.srcImageLayout = depthLayout;
      depthCopyParams.srcImageFormat = imginfo.format;
      depthCopyParams.multisampled = (imginfo.samples != VK_SAMPLE_COUNT_1_BIT);
      depthCopyParams.sub = depthSub;
      CopyImagePixel(cmd, depthCopyParams, offset + offsetof(struct PixelHistoryValue, depth));
      m_DepthFormats.insert(std::make_pair(eid, imginfo.format));
    }
  }

  // ReplayDraw begins renderpass, executes a single draw defined by the eventId and
  // ends the renderpass.
  void ReplayDraw(VkCommandBuffer cmd, uint32_t eventId, bool clear = false)
  {
    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
        m_pDriver, cmd, VulkanRenderState::BindGraphics, false);

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
      rect.layerCount = m_CallbackInfo.layers;
      ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 1, &att, 1, &rect);
    }

    const ActionDescription *action = m_pDriver->GetAction(eventId);
    m_pDriver->ReplayDraw(cmd, *action);

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
    MakeIncrementStencilPipelineCI(eid, pipeline, pipeCreateInfo, stages, false, true);
    // No need to change depth stencil state, it is already
    // set to always pass, and increment.
    pipeCreateInfo.renderPass = rp;

    PipelineReplacements replacements = {};
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL,
                                                        &replacements.originalShaderStencil);
    m_pDriver->CheckVkResult(vkr);

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
    m_pDriver->CheckVkResult(vkr);

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
  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(!m_Events.contains(eid))
      return;

    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();
    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetDebugManager()->GetPipelineInfo(pipestate.graphics.pipeline);
    uint32_t eventFlags = CalculateEventFlags(p, pipestate);
    m_EventFlags[eid] = eventFlags;
    if(pipestate.depthBoundsTestEnable)
      m_EventDepthBounds[eid] = {pipestate.mindepth, pipestate.maxdepth};
    else
      m_EventDepthBounds[eid] = {};

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

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // TODO: handle aliased events.
  }

  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
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
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  bool HasEventFlags(uint32_t eventId) { return m_EventFlags.find(eventId) != m_EventFlags.end(); }
  uint32_t GetEventFlags(uint32_t eventId)
  {
    auto it = m_EventFlags.find(eventId);
    if(it == m_EventFlags.end())
      RDCERR("Can't find event flags for event %u", eventId);
    return it->second;
  }
  rdcpair<float, float> GetEventDepthBounds(uint32_t eventId)
  {
    auto it = m_EventDepthBounds.find(eventId);
    if(it == m_EventDepthBounds.end())
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
    m_pDriver->CheckVkResult(vkr);
  }

  uint64_t GetOcclusionResult(uint32_t eventId, uint32_t test) const
  {
    auto it = m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test));
    if(it == m_OcclusionQueries.end())
    {
      RDCERR("Can't locate occlusion query for event id %u and test flags %u", eventId, test);
      return 0;
    }
    if(it->second >= m_OcclusionResults.size())
    {
      RDCERR("Event %u, occlusion index is %u, and the total # of occlusion query data %zu",
             eventId, it->second, m_OcclusionResults.size());
      return 0;
    }
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
      if(p.depthClipEnable && !p.depthClampEnable)
        flags |= TestEnabled_DepthClipping;

      if(pipestate.cullMode != VK_CULL_MODE_NONE)
        flags |= TestEnabled_Culling;

      if(pipestate.cullMode == VK_CULL_MODE_FRONT_AND_BACK)
        flags |= TestMustFail_Culling;
    }

    bool hasDepthStencil = false;

    if(pipestate.dynamicRendering.active)
    {
      hasDepthStencil = pipestate.dynamicRendering.depth.imageView != VK_NULL_HANDLE ||
                        pipestate.dynamicRendering.stencil.imageView != VK_NULL_HANDLE;
    }
    else
    {
      // Depth and Stencil tests.
      const VulkanCreationInfo::RenderPass &rpInfo =
          m_pDriver->GetDebugManager()->GetRenderPassInfo(pipestate.GetRenderPass());
      // TODO: this should retrieve the correct subpass, once multiple subpasses
      // are supported.
      const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();

      hasDepthStencil = (sub.depthstencilAttachment != -1);
    }

    if(hasDepthStencil)
    {
      if(pipestate.depthBoundsTestEnable)
        flags |= TestEnabled_DepthBounds;

      if(pipestate.depthTestEnable)
      {
        if(pipestate.depthCompareOp != VK_COMPARE_OP_ALWAYS)
          flags |= TestEnabled_DepthTesting;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_NEVER)
          flags |= TestMustFail_DepthTesting;

        if(pipestate.depthCompareOp == VK_COMPARE_OP_NEVER)
          flags |= DepthTest_Never;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_LESS)
          flags |= DepthTest_Less;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_EQUAL)
          flags |= DepthTest_Equal;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_LESS_OR_EQUAL)
          flags |= DepthTest_LessEqual;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_GREATER)
          flags |= DepthTest_Greater;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_NOT_EQUAL)
          flags |= DepthTest_NotEqual;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_GREATER_OR_EQUAL)
          flags |= DepthTest_GreaterEqual;
        if(pipestate.depthCompareOp == VK_COMPARE_OP_ALWAYS)
          flags |= DepthTest_Always;
      }
      else
      {
        flags |= DepthTest_Always;
      }

      if(pipestate.stencilTestEnable)
      {
        if(pipestate.front.compareOp != VK_COMPARE_OP_ALWAYS ||
           pipestate.back.compareOp != VK_COMPARE_OP_ALWAYS)
          flags |= TestEnabled_StencilTesting;

        if(pipestate.front.compareOp == VK_COMPARE_OP_NEVER &&
           pipestate.back.compareOp == VK_COMPARE_OP_NEVER)
          flags |= TestMustFail_StencilTesting;
        else if(pipestate.front.compareOp == VK_COMPARE_OP_NEVER &&
                pipestate.cullMode == VK_CULL_MODE_BACK_BIT)
          flags |= TestMustFail_StencilTesting;
        else if(pipestate.cullMode == VK_CULL_MODE_FRONT_BIT &&
                pipestate.back.compareOp == VK_COMPARE_OP_NEVER)
          flags |= TestMustFail_StencilTesting;
      }
    }

    // Scissor
    {
      bool inRegion = false;
      bool inAllRegions = true;
      // Do we even need to know viewerport here?
      const VkRect2D *pScissors = pipestate.scissors.data();
      uint32_t scissorCount = (uint32_t)pipestate.scissors.size();

      for(uint32_t i = 0; i < scissorCount; i++)
      {
        const VkOffset2D &offset = pScissors[i].offset;
        const VkExtent2D &extent = pScissors[i].extent;
        if(((int32_t)m_CallbackInfo.x >= offset.x) && ((int32_t)m_CallbackInfo.y >= offset.y) &&
           ((int32_t)m_CallbackInfo.x < ((int64_t)offset.x + (int64_t)extent.width)) &&
           ((int32_t)m_CallbackInfo.y < ((int64_t)offset.y + (int64_t)extent.height)))
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
    PipelineCreationFlags_DisableDepthClipping = 1 << 4,
    PipelineCreationFlags_FixedColorShader = 1 << 5,
    PipelineCreationFlags_IntersectOriginalScissor = 1 << 6,
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
    rdcarray<VkShaderModule> replacementShaders;
    replacementShaders.resize(NumShaderStages);
    // Replace fragment shader because it might have early fragments
    for(size_t i = 0; i < replacementShaders.size(); i++)
    {
      if(p.shaders[i].module == ResourceId())
        continue;
      ShaderStage stage = StageFromIndex(i);
      bool rwInStage = (eventShaderFlags & PipeStageRWEventFlags(stage)) != EventFlags::NoFlags;
      if(rwInStage || (stage == ShaderStage::Fragment))
        replacementShaders[i] = m_ShaderCache->GetShaderWithoutSideEffects(
            p.shaders[i].module, p.shaders[i].entryPoint, p.shaders[i].stage);
    }

    VulkanRenderState &pipestate = m_pDriver->GetCmdRenderState();
    rdcarray<VkRect2D> prevScissors = pipestate.scissors;
    for(uint32_t i = 0; i < pipestate.views.size(); i++)
      ScissorToPixel(pipestate.views[i], pipestate.scissors[i]);

    if(eventFlags & TestEnabled_Culling)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_DisableDepthClipping |
          PipelineCreationFlags_DisableDepthBoundsTest | PipelineCreationFlags_DisableStencilTest |
          PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      VkMarkerRegion::Set(StringFormat::Fmt("Test culling on %u", eid), cmd);
      ReplayDraw(cmd, pipe, eid, TestEnabled_Culling);
    }

    if(eventFlags & TestEnabled_DepthClipping)
    {
      uint32_t pipeFlags =
          PipelineCreationFlags_DisableDepthTest | PipelineCreationFlags_DisableDepthBoundsTest |
          PipelineCreationFlags_DisableStencilTest | PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      VkMarkerRegion::Set(StringFormat::Fmt("Test depth clipping on %u", eid), cmd);
      ReplayDraw(cmd, pipe, eid, TestEnabled_DepthClipping);
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
      VkMarkerRegion::Set(StringFormat::Fmt("Test scissor on %u", eid), cmd);
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
      VkMarkerRegion::Set(StringFormat::Fmt("Test sample mask on %u", eid), cmd);
      ReplayDraw(cmd, pipe, eid, TestEnabled_SampleMask);
    }

    // Depth bounds
    if(eventFlags & TestEnabled_DepthBounds)
    {
      uint32_t pipeFlags = PipelineCreationFlags_DisableStencilTest |
                           PipelineCreationFlags_DisableDepthTest |
                           PipelineCreationFlags_FixedColorShader;
      VkPipeline pipe = CreatePipeline(basePipeline, pipeFlags, replacementShaders, outputIndex);
      VkMarkerRegion::Set(StringFormat::Fmt("Test depth bounds on %u", eid), cmd);
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
      VkMarkerRegion::Set(StringFormat::Fmt("Test stencil on %u", eid), cmd);
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
      VkMarkerRegion::Set(StringFormat::Fmt("Test depth on %u", eid), cmd);
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
      VkMarkerRegion::Set(StringFormat::Fmt("Test shader discard on %u", eid), cmd);
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

    SetupDynamicStates(ci);

    // remove any dynamic states where we want to use exactly the fixed ones we're setting up
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);

    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthTest)
      m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthBoundsTest)
      m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
    if(pipeCreateFlags & PipelineCreationFlags_DisableStencilTest)
      m_DynamicStates.removeOne(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);

    if(pipeCreateFlags & PipelineCreationFlags_DisableCulling)
      m_DynamicStates.removeOne(VK_DYNAMIC_STATE_CULL_MODE);

    ApplyDynamicStates(ci);

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
    if(pipeCreateFlags & PipelineCreationFlags_DisableDepthClipping)
      rs->depthClampEnable = VK_TRUE;

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
    m_pDriver->CheckVkResult(vkr);
    m_PipeCache.insert(std::make_pair(pipeKey, pipe));
    return pipe;
  }

  void ReplayDraw(VkCommandBuffer cmd, VkPipeline pipe, int eventId, uint32_t test)
  {
    m_pDriver->GetCmdRenderState().graphics.pipeline = GetResID(pipe);
    // ensure the render state sets any dynamic state the pipeline needs
    m_pDriver->GetCmdRenderState().SetDynamicStatesFromPipeline(m_pDriver);
    m_pDriver->GetCmdRenderState().BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                                false);

    uint32_t index = (uint32_t)m_OcclusionQueries.size();
    if(m_OcclusionQueries.find(rdcpair<uint32_t, uint32_t>(eventId, test)) != m_OcclusionQueries.end())
      RDCERR("A query already exist for event id %u and test %u", eventId, test);
    m_OcclusionQueries.insert(std::make_pair(rdcpair<uint32_t, uint32_t>(eventId, test), index));

    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, index, m_QueryFlags);

    const ActionDescription *action = m_pDriver->GetAction(eventId);
    m_pDriver->ReplayDraw(cmd, *action);

    ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_OcclusionPool, index);
  }

  rdcarray<uint32_t> m_Events;
  // Key is event ID, value is the flags for that event.
  std::map<uint32_t, uint32_t> m_EventFlags;
  std::map<uint32_t, rdcpair<float, float>> m_EventDepthBounds;
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

  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(m_EventFragments.find(eid) == m_EventFragments.end())
      return;

    VulkanRenderState prevState = m_pDriver->GetCmdRenderState();
    VulkanRenderState &state = m_pDriver->GetCmdRenderState();
    ResourceId curPipeline = state.graphics.pipeline;
    state.EndRenderPass(cmd);
    // if dynamic rendering is in use and the renderpass just suspended, we need to be sure it is
    // really finished. This will just store as we always patch the load/store ops.
    state.FinishSuspendedRenderPass(cmd);

    uint32_t numFragmentsInEvent = m_EventFragments[eid];

    uint32_t framebufferIndex = 0;
    uint32_t colorOutputIndex = GetColorAttachmentIndex(prevState, &framebufferIndex);

    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      if(prevState.dynamicRendering.active)
      {
        framebufferIndex = colorOutputIndex = (uint32_t)prevState.dynamicRendering.color.size();
      }
      else
      {
        const VulkanCreationInfo::RenderPass &rpInfo =
            m_pDriver->GetDebugManager()->GetRenderPassInfo(prevState.GetRenderPass());
        const VulkanCreationInfo::RenderPass::Subpass &sub = rpInfo.subpasses.front();

        // Going to add another color attachment.
        framebufferIndex = (uint32_t)prevState.GetFramebufferAttachments().size();
        colorOutputIndex = (uint32_t)sub.colorAttachments.size();

        while(colorOutputIndex > 0 &&
              sub.colorAttachments[colorOutputIndex - 1] == VK_ATTACHMENT_UNUSED)
          colorOutputIndex--;
      }
    }

    bool multiview = false;
    VkRenderPass origRpWithDepth = PatchRenderPass(state, multiview);
    state.SetRenderPass(prevState.GetRenderPass());
    state.dynamicRendering = prevState.dynamicRendering;
    VkRenderPass newRp = PatchRenderPass(state, multiview, VK_FORMAT_R32G32B32A32_SFLOAT,
                                         framebufferIndex, colorOutputIndex);
    PatchFramebuffer(state, newRp, m_CallbackInfo.subImageView, VK_FORMAT_R32G32B32A32_SFLOAT,
                     framebufferIndex, colorOutputIndex);

    Pipelines pipes = CreatePerFragmentPipelines(curPipeline, newRp, origRpWithDepth, eid, 0,
                                                 VK_FORMAT_R32G32B32A32_SFLOAT, colorOutputIndex);

    for(uint32_t i = 0; i < state.views.size(); i++)
    {
      ScissorToPixel(state.views[i], state.scissors[i]);

      state.scissors[i].offset.x &= ~0x1;
      state.scissors[i].offset.y &= ~0x1;
      state.scissors[i].extent = {2, 2};
    }

    VkPipeline pipesIter[2];
    pipesIter[0] = pipes.primitiveIdPipe;
    pipesIter[1] = pipes.shaderOutPipe;

    VkCopyPixelParams colourCopyParams = {};
    colourCopyParams.srcImage = m_CallbackInfo.subImage;
    colourCopyParams.srcImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    colourCopyParams.multisampled = (m_CallbackInfo.samples != VK_SAMPLE_COUNT_1_BIT);
    colourCopyParams.multiview = multiview;
    colourCopyParams.sub = m_CallbackInfo.targetSubresource;
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
    {
      colourCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
      // Use the layout of the image we are substituting for.
      VkImageLayout srcImageLayout =
          GetImageLayout(GetResID(m_CallbackInfo.targetImage), VK_IMAGE_ASPECT_COLOR_BIT,
                         m_CallbackInfo.targetSubresource);
      colourCopyParams.srcImageLayout = srcImageLayout;
    }

    bool depthEnabled = prevState.depthTestEnable != VK_FALSE;

    VkMarkerRegion::Set(StringFormat::Fmt("Event %u has %u fragments", eid, numFragmentsInEvent),
                        cmd);

    // Get primitive ID and shader output value for each fragment.
    for(uint32_t f = 0; f < numFragmentsInEvent; f++)
    {
      for(uint32_t i = 0; i < 2; i++)
      {
        uint32_t storeOffset = (fragsProcessed + f) * sizeof(PerFragmentInfo);

        VkMarkerRegion region(cmd, StringFormat::Fmt("Getting %s for %u",
                                                     i == 0 ? "primitive ID" : "shader output", eid));

        if(i == 0 && !m_pDriver->GetDeviceEnabledFeatures().geometryShader)
        {
          // without geometryShader, can't read primitive ID in pixel shader
          VkMarkerRegion::Set("Can't get primitive ID without geometryShader feature", cmd);

          ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(m_CallbackInfo.dstBuffer), storeOffset,
                                      16, ~0U);
          continue;
        }

        if(pipesIter[i] == VK_NULL_HANDLE)
        {
          // without one of the pipelines (e.g. if there was a geometry shader in use and we can't
          // read primitive ID in the fragment shader) we can't continue.
          // technically we can if the geometry shader outs a primitive ID, but that is unlikely.
          VkMarkerRegion::Set("Can't get primitive ID with geometry shader in use", cmd);

          ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(m_CallbackInfo.dstBuffer), storeOffset,
                                      16, ~0U);
          continue;
        }

        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        NULL,
                                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                        VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        Unwrap(m_CallbackInfo.dsImage),
                                        {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0,
                                         1, 0, m_CallbackInfo.layers}};

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
        range.layerCount = m_CallbackInfo.layers;
        range.levelCount = 1;

        ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), Unwrap(m_CallbackInfo.dsImage),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dsValue, 1,
                                                &range);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        DoPipelineBarrier(cmd, 1, &barrier);

        barrier.image = Unwrap(colourCopyParams.srcImage);
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = colourCopyParams.srcImageLayout;
        DoPipelineBarrier(cmd, 1, &barrier);

        m_pDriver->GetCmdRenderState().graphics.pipeline = GetResID(pipesIter[i]);
        // ensure the render state sets any dynamic state the pipeline needs
        m_pDriver->GetCmdRenderState().SetDynamicStatesFromPipeline(m_pDriver);

        m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
            m_pDriver, cmd, VulkanRenderState::BindGraphics, false);

        // Update stencil reference to the current fragment index, so that we get values
        // for a single fragment only.
        ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
        ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
        ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, f);
        const ActionDescription *action = m_pDriver->GetAction(eid);
        m_pDriver->ReplayDraw(cmd, *action);
        state.EndRenderPass(cmd);

        if(i == 1)
        {
          storeOffset += offsetof(struct PerFragmentInfo, shaderOut);
          if(depthEnabled)
          {
            VkCopyPixelParams depthCopyParams = colourCopyParams;
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
    VkImageLayout depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if(prevState.dynamicRendering.active)
    {
      VkImageView dsView = state.dynamicRendering.depth.imageView;
      depthLayout = state.dynamicRendering.depth.imageLayout;
      if(dsView == VK_NULL_HANDLE && state.dynamicRendering.stencil.imageView != VK_NULL_HANDLE)
      {
        dsView = state.dynamicRendering.stencil.imageView;
        depthLayout = state.dynamicRendering.stencil.imageLayout;
      }

      if(dsView != VK_NULL_HANDLE)
      {
        ResourceId resId = m_pDriver->GetDebugManager()->GetImageViewInfo(GetResID(dsView)).image;
        depthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(resId);
        const VulkanCreationInfo::Image &imginfo = m_pDriver->GetDebugManager()->GetImageInfo(resId);
        depthFormat = imginfo.format;
      }
    }
    else
    {
      const VulkanCreationInfo::RenderPass &rpInfo =
          m_pDriver->GetDebugManager()->GetRenderPassInfo(prevState.GetRenderPass());
      const rdcarray<ResourceId> &atts = prevState.GetFramebufferAttachments();

      int32_t depthAtt = rpInfo.subpasses[prevState.subpass].depthstencilAttachment;
      if(depthAtt >= 0)
      {
        ResourceId resId = m_pDriver->GetDebugManager()->GetImageViewInfo(atts[depthAtt]).image;
        depthImage = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(resId);
        const VulkanCreationInfo::Image &imginfo = m_pDriver->GetDebugManager()->GetImageInfo(resId);
        depthFormat = imginfo.format;
        depthLayout = rpInfo.subpasses[prevState.subpass].depthLayout;
      }
    }

    // use the original renderpass and framebuffer attachment, but ensure we have depth and stencil
    state.SetRenderPass(prevState.GetRenderPass());
    state.SetFramebuffer(prevState.GetFramebuffer(), prevState.GetFramebufferAttachments());
    state.dynamicRendering = prevState.dynamicRendering;

    PatchRenderPass(state, multiview);
    PatchFramebuffer(state, origRpWithDepth);

    colourCopyParams.srcImage = m_CallbackInfo.targetImage;
    colourCopyParams.srcImageFormat = m_CallbackInfo.targetImageFormat;
    colourCopyParams.multisampled = (m_CallbackInfo.samples != VK_SAMPLE_COUNT_1_BIT);
    VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
      aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    colourCopyParams.srcImageLayout = GetImageLayout(GetResID(m_CallbackInfo.targetImage), aspect,
                                                     m_CallbackInfo.targetSubresource);

    const ModificationValue &premod = m_EventPremods[eid];
    // For every fragment except the last one, retrieve post-modification
    // value.
    for(uint32_t f = 0; f < numFragmentsInEvent - 1; f++)
    {
      VkMarkerRegion region(cmd, StringFormat::Fmt("Getting postmod for fragment %u in %u", f, eid));

      // Get post-modification value, use the original framebuffer attachment.
      state.graphics.pipeline = GetResID(pipes.postModPipe);
      // ensure the render state sets any dynamic state the pipeline needs
      state.SetDynamicStatesFromPipeline(m_pDriver);
      state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);
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
        memcpy(clearAtts[0].clearValue.color.float32, premod.col.floatValue.data(),
               sizeof(clearAtts[0].clearValue.color));

        clearAtts[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clearAtts[1].clearValue.depthStencil.depth = premod.depth;

        if(IsDepthOrStencilFormat(m_CallbackInfo.targetImageFormat))
          ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 1, clearAtts + 1, 1, &rect);
        else
          ObjDisp(cmd)->CmdClearAttachments(Unwrap(cmd), 2, clearAtts, 1, &rect);
      }

      ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
      ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_AND_BACK, f);
      const ActionDescription *action = m_pDriver->GetAction(eid);
      m_pDriver->ReplayDraw(cmd, *action);
      state.EndRenderPass(cmd);

      CopyImagePixel(cmd, colourCopyParams,
                     (fragsProcessed + f) * sizeof(PerFragmentInfo) +
                         offsetof(struct PerFragmentInfo, postMod));

      if(depthImage != VK_NULL_HANDLE)
      {
        VkCopyPixelParams depthCopyParams = colourCopyParams;
        depthCopyParams.srcImage = m_CallbackInfo.dsImage;
        depthCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthCopyParams.srcImageFormat = m_CallbackInfo.dsFormat;
        CopyImagePixel(cmd, depthCopyParams,
                       (fragsProcessed + f) * sizeof(PerFragmentInfo) +
                           offsetof(struct PerFragmentInfo, postMod) +
                           offsetof(struct PixelHistoryValue, depth));
      }
    }

    m_EventIndices[eid] = fragsProcessed;
    fragsProcessed += numFragmentsInEvent;

    m_pDriver->GetCmdRenderState() = prevState;
    m_pDriver->GetCmdRenderState().BeginRenderPassAndApplyState(
        m_pDriver, cmd, VulkanRenderState::BindGraphics, true);
  }
  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  // CreatePerFragmentPipelines for getting per fragment information.
  Pipelines CreatePerFragmentPipelines(ResourceId pipe, VkRenderPass rp, VkRenderPass origRpWithDepth,
                                       uint32_t eid, uint32_t fragmentIndex,
                                       VkFormat colorOutputFormat, uint32_t colorOutputIndex)
  {
    const VulkanCreationInfo::Pipeline &p = m_pDriver->GetDebugManager()->GetPipelineInfo(pipe);
    VkGraphicsPipelineCreateInfo pipeCreateInfo = {};
    rdcarray<VkPipelineShaderStageCreateInfo> stages;
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, pipe);

    SetupDynamicStates(pipeCreateInfo);

    // remove any dynamic states where we want to use exactly the fixed ones we're setting up
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_STENCIL_OP);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);

    ApplyDynamicStates(pipeCreateInfo);

    // if RP is null we need to patch the pipeline rendering info
    if(rp == VK_NULL_HANDLE)
    {
      VkPipelineRenderingCreateInfo *dynRenderCreate =
          (VkPipelineRenderingCreateInfo *)FindNextStruct(
              &pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

      RDCASSERT(dynRenderCreate);

      // if we're patching color, we know we can safely add a new attachment if needed
      if(colorOutputFormat != VK_NULL_HANDLE)
      {
        if(colorOutputIndex >= dynRenderCreate->colorAttachmentCount)
          dynRenderCreate->colorAttachmentCount++;

        RDCASSERT(colorOutputIndex < dynRenderCreate->colorAttachmentCount);

        VkFormat *fmts = (VkFormat *)dynRenderCreate->pColorAttachmentFormats;
        fmts[colorOutputIndex] = colorOutputFormat;
      }

      dynRenderCreate->depthAttachmentFormat = m_CallbackInfo.dsFormat;
      dynRenderCreate->stencilAttachmentFormat = m_CallbackInfo.dsFormat;
    }

    VkPipelineDepthStencilStateCreateInfo *ds =
        (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;

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
    }

    stages.resize(pipeCreateInfo.stageCount);
    memcpy(stages.data(), pipeCreateInfo.pStages, stages.byteSize());

    EventFlags eventFlags = m_pDriver->GetEventFlags(eid);
    VkShaderModule replacementShaders[NumShaderStages] = {};

    // Clean shaders
    for(size_t i = 0; i < NumShaderStages; i++)
    {
      if((eventFlags & PipeStageRWEventFlags(StageFromIndex(i))) != EventFlags::NoFlags)
        replacementShaders[i] = m_ShaderCache->GetShaderWithoutSideEffects(
            p.shaders[i].module, p.shaders[i].entryPoint, p.shaders[i].stage);
    }
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      VkShaderModule replacement = replacementShaders[StageIndex(stages[i].stage)];
      if(replacement != VK_NULL_HANDLE)
        stages[i].module = replacement;
    }
    pipeCreateInfo.pStages = stages.data();

    // the postmod pipe is used with the renderpass with added depth/stencil
    pipeCreateInfo.renderPass = origRpWithDepth;
    Pipelines pipes = {};
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL, &pipes.postModPipe);
    m_pDriver->CheckVkResult(vkr);
    m_PipesToDestroy.push_back(pipes.postModPipe);

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

    for(uint32_t i = 0; i < cbs->attachmentCount; i++)
    {
      atts[i].blendEnable = 0;
      atts[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }

    {
      ds->depthBoundsTestEnable = VK_FALSE;
      ds->depthWriteEnable = VK_TRUE;
      ds->depthCompareOp = VK_COMPARE_OP_ALWAYS;
    }

    vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                               &pipeCreateInfo, NULL, &pipes.shaderOutPipe);
    m_pDriver->CheckVkResult(vkr);

    m_PipesToDestroy.push_back(pipes.shaderOutPipe);

    {
      ds->depthTestEnable = VK_FALSE;
      ds->depthWriteEnable = VK_FALSE;
    }

    m_DynamicStates.removeOne(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);

    ApplyDynamicStates(pipeCreateInfo);

    // Output the primitive ID.
    VkPipelineShaderStageCreateInfo stageCI = {};
    stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stageCI.module = m_ShaderCache->GetPrimitiveIdShader(colorOutputIndex);
    stageCI.pName = "main";
    bool gsFound = false;
    bool meshFound = false;
    bool fsFound = false;
    for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
    {
      if(stages[i].stage == VK_SHADER_STAGE_GEOMETRY_BIT)
      {
        gsFound = true;
        break;
      }
      if(stages[i].stage == VK_SHADER_STAGE_MESH_BIT_EXT)
      {
        meshFound = true;
        break;
      }
      if(stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        stages[i] = stageCI;
        fsFound = true;
      }
    }
    if(!fsFound)
    {
      stages.push_back(stageCI);
      pipeCreateInfo.stageCount = (uint32_t)stages.size();
      pipeCreateInfo.pStages = stages.data();
    }

    if(!gsFound && !meshFound)
    {
      vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                 &pipeCreateInfo, NULL, &pipes.primitiveIdPipe);
      m_pDriver->CheckVkResult(vkr);
      m_PipesToDestroy.push_back(pipes.primitiveIdPipe);
    }
    else
    {
      pipes.primitiveIdPipe = VK_NULL_HANDLE;
      RDCWARN("Can't get primitive ID at event %u due to geometry shader usage", eid);
    }

    return pipes;
  }

  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}
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
  std::map<uint32_t, rdcarray<int32_t>> m_Events;
  VulkanPixelHistoryDiscardedFragmentsCallback(WrappedVulkan *vk,
                                               PixelHistoryShaderCache *shaderCache,
                                               const PixelHistoryCallbackInfo &callbackInfo,
                                               std::map<uint32_t, rdcarray<int32_t>> events,
                                               VkQueryPool occlusionPool)
      : VulkanPixelHistoryCallback(vk, shaderCache, callbackInfo, occlusionPool), m_Events(events)
  {
  }

  ~VulkanPixelHistoryDiscardedFragmentsCallback()
  {
    for(const VkPipeline &pipe : m_PipesToDestroy)
      m_pDriver->vkDestroyPipeline(m_pDriver->GetDev(), pipe, NULL);
  }

  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
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
    // ensure the render state sets any dynamic state the pipeline needs
    state.SetDynamicStatesFromPipeline(m_pDriver);
    const VulkanCreationInfo::Pipeline &p =
        m_pDriver->GetDebugManager()->GetPipelineInfo(state.graphics.pipeline);
    Topology topo = MakePrimitiveTopology(state.primitiveTopology, p.patchControlPoints);
    state.BindPipeline(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);
    for(uint32_t i = 0; i < primIds.size(); i++)
    {
      uint32_t queryId = (uint32_t)m_OcclusionIndices.size();
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionPool, queryId, m_QueryFlags);
      uint32_t primId = primIds[i];
      ActionDescription action = *m_pDriver->GetAction(eid);
      action.numIndices = RENDERDOC_NumVerticesPerPrimitive(topo);
      action.indexOffset += RENDERDOC_VertexOffset(topo, primId);
      action.vertexOffset += RENDERDOC_VertexOffset(topo, primId);
      // TODO once pixel history distinguishes between instances, draw only the instance for
      // this fragment.
      // TODO replay with a dummy index buffer so that all primitives other than the target one are
      // degenerate - that way the vertex index etc is still the same as it should be.
      m_pDriver->ReplayDraw(cmd, action);
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
    MakeIncrementStencilPipelineCI(eid, pipe, pipeCreateInfo, stages, false, false);

    {
      VkPipelineDepthStencilStateCreateInfo *ds =
          (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
      ds->stencilTestEnable = VK_FALSE;
    }

    VkPipeline newPipe;
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &pipeCreateInfo, NULL, &newPipe);
    m_pDriver->CheckVkResult(vkr);
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
    m_pDriver->CheckVkResult(vkr);
  }

  bool PrimitiveDiscarded(uint32_t eid, uint32_t primId)
  {
    auto it = m_OcclusionIndices.find(make_rdcpair<uint32_t, uint32_t>(eid, primId));
    if(it == m_OcclusionIndices.end())
      return false;
    return m_OcclusionResults[it->second] == 0;
  }

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias) {}
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
  VkMarkerRegion region(
      StringFormat::Fmt("PixelHistorySetupResources %ux%ux%u %s %ux MSAA, %u events", extent.width,
                        extent.height, extent.depth, ToStr(format).c_str(), samples, numEvents));
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
  CheckVkResult(vkr);

  ImageState colorImageState = ImageState(colorImage, ImageInfo(imgInfo), eFrameRef_None);

  VkMemoryRequirements colorImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, colorImage, &colorImageMrq);
  totalMemorySize = colorImageMrq.size;

  imgInfo.format = dsFormat;
  imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &dsImage);
  CheckVkResult(vkr);

  ImageState stencilImageState = ImageState(dsImage, ImageInfo(imgInfo), eFrameRef_None);

  VkMemoryRequirements stencilImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, dsImage, &stencilImageMrq);
  VkDeviceSize offset = AlignUp(totalMemorySize, stencilImageMrq.alignment);
  totalMemorySize = offset + stencilImageMrq.size;

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      NULL,
      totalMemorySize,
      m_pDriver->GetGPULocalMemoryIndex(colorImageMrq.memoryTypeBits),
  };
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &gpuMem);
  CheckVkResult(vkr);

  if(vkr != VK_SUCCESS)
    return false;

  vkr = m_pDriver->vkBindImageMemory(m_Device, colorImage, gpuMem, 0);
  CheckVkResult(vkr);

  vkr = m_pDriver->vkBindImageMemory(m_Device, dsImage, gpuMem, offset);
  CheckVkResult(vkr);

  NameVulkanObject(colorImage, "Pixel History color image");
  NameVulkanObject(dsImage, "Pixel History depth image");

  VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = colorImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, imgInfo.arrayLayers};

  if(imgInfo.arrayLayers != 0)
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &colorImageView);
  CheckVkResult(vkr);

  viewInfo.image = dsImage;
  viewInfo.format = dsFormat;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0,
                               imgInfo.arrayLayers};

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &dsImageView);
  CheckVkResult(vkr);

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = AlignUp((uint32_t)(numEvents * sizeof(EventInfo)), 4096U);
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  vkr = m_pDriver->vkCreateBuffer(m_Device, &bufferInfo, NULL, &dstBuffer);
  CheckVkResult(vkr);

  // Allocate memory
  VkMemoryRequirements mrq = {};
  m_pDriver->vkGetBufferMemoryRequirements(m_Device, dstBuffer, &mrq);
  allocInfo.allocationSize = mrq.size;
  allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &bufferMemory);
  CheckVkResult(vkr);

  if(vkr != VK_SUCCESS)
    return false;

  vkr = m_pDriver->vkBindBufferMemory(m_Device, dstBuffer, bufferMemory, 0);
  CheckVkResult(vkr);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  if(cmd == VK_NULL_HANDLE)
    return false;

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);
  ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(dstBuffer), 0, VK_WHOLE_SIZE, 0);
  colorImageState.InlineTransition(
      cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());
  stencilImageState.InlineTransition(
      cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0,
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  CheckVkResult(vkr);
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

bool VulkanDebugManager::PixelHistorySetupPerFragResources(PixelHistoryResources &resources,
                                                           uint32_t numEvents, uint32_t numFrags)
{
  const uint32_t existingBufferSize = AlignUp((uint32_t)(numEvents * sizeof(EventInfo)), 4096U);
  const uint32_t requiredBufferSize = AlignUp((uint32_t)(numFrags * sizeof(PerFragmentInfo)), 4096U);

  // If the existing buffer is big enough for all of the fragments, we can re-use it.
  const bool canReuseBuffer = existingBufferSize >= requiredBufferSize;

  VkMarkerRegion region(StringFormat::Fmt(
      "PixelHistorySetupPerFragResources %u events %u frags, buffer size %u -> %u, %s old buffer",
      numEvents, numFrags, existingBufferSize, requiredBufferSize,
      canReuseBuffer ? "reusing" : "NOT reusing"));

  if(canReuseBuffer)
    return true;

  // Otherwise, destroy it and create a new one that's big enough in its place.
  VkDevice dev = m_pDriver->GetDev();

  if(resources.dstBuffer != VK_NULL_HANDLE)
    m_pDriver->vkDestroyBuffer(dev, resources.dstBuffer, NULL);
  if(resources.bufferMemory != VK_NULL_HANDLE)
    m_pDriver->vkFreeMemory(dev, resources.bufferMemory, NULL);
  resources.dstBuffer = VK_NULL_HANDLE;
  resources.bufferMemory = VK_NULL_HANDLE;

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = requiredBufferSize;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  VkResult vkr = m_pDriver->vkCreateBuffer(m_Device, &bufferInfo, NULL, &resources.dstBuffer);
  CheckVkResult(vkr);

  // Allocate memory
  VkMemoryRequirements mrq = {};
  m_pDriver->vkGetBufferMemoryRequirements(m_Device, resources.dstBuffer, &mrq);
  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      NULL,
      mrq.size,
      m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
  };
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &resources.bufferMemory);
  CheckVkResult(vkr);

  if(vkr != VK_SUCCESS)
    return false;

  vkr = m_pDriver->vkBindBufferMemory(m_Device, resources.dstBuffer, resources.bufferMemory, 0);
  CheckVkResult(vkr);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  if(cmd == VK_NULL_HANDLE)
    return false;

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);
  ObjDisp(cmd)->CmdFillBuffer(Unwrap(cmd), Unwrap(resources.dstBuffer), 0, VK_WHOLE_SIZE, 0);

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  CheckVkResult(vkr);
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

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
  m_PixelHistory.allocedSets.push_back(descSet);
  return descSet;
}

void VulkanReplay::ResetPixelHistoryDescriptorPool()
{
  for(VkDescriptorSet descset : m_PixelHistory.allocedSets)
    GetResourceManager()->ReleaseWrappedResource(descset, true);
  m_PixelHistory.allocedSets.clear();
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

static void CreateOcclusionPool(WrappedVulkan *vk, uint32_t poolSize, VkQueryPool *pQueryPool)
{
  VkMarkerRegion region(StringFormat::Fmt("CreateOcclusionPool %u", poolSize));

  VkDevice dev = vk->GetDev();
  VkQueryPoolCreateInfo occlusionPoolCreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  occlusionPoolCreateInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
  occlusionPoolCreateInfo.queryCount = poolSize;
  // TODO: check that occlusion feature is available
  VkResult vkr =
      ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &occlusionPoolCreateInfo, NULL, pQueryPool);
  vk->CheckVkResult(vkr);
  VkCommandBuffer cmd = vk->GetNextCmd();
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  if(cmd == VK_NULL_HANDLE)
    return;

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  vk->CheckVkResult(vkr);
  ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), *pQueryPool, 0, poolSize);
  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  vk->CheckVkResult(vkr);
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

static void UpdateTestsFailed(const TestsFailedCallback *tfCb, uint32_t eventId,
                              uint32_t eventFlags, PixelModification &mod)
{
  bool earlyFragmentTests = tfCb->HasEarlyFragments(eventId);

  if((eventFlags & (TestEnabled_Culling | TestMustFail_Culling)) == TestEnabled_Culling)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_Culling);
    mod.backfaceCulled = (occlData == 0);
  }

  if(mod.backfaceCulled)
    return;

  if(eventFlags & TestEnabled_DepthClipping)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthClipping);
    mod.depthClipped = (occlData == 0);
  }

  if(mod.depthClipped)
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
  if(!earlyFragmentTests &&
     (eventFlags & (TestMustFail_DepthTesting | TestMustFail_StencilTesting)) == 0)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_FragmentDiscard);
    mod.shaderDiscarded = (occlData == 0);
    if(mod.shaderDiscarded)
      return;
  }

  if(eventFlags & TestEnabled_DepthBounds)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_DepthBounds);
    mod.depthBoundsFailed = (occlData == 0);
  }
  if(mod.depthBoundsFailed)
    return;

  if(eventFlags & TestMustFail_StencilTesting)
    return;

  if((eventFlags & (TestEnabled_StencilTesting | TestMustFail_StencilTesting)) ==
     TestEnabled_StencilTesting)
  {
    uint64_t occlData = tfCb->GetOcclusionResult(eventId, TestEnabled_StencilTesting);
    mod.stencilTestFailed = (occlData == 0);
  }
  if(mod.stencilTestFailed)
    return;

  if(eventFlags & TestMustFail_DepthTesting)
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

static void FillInColor(ResourceFormat fmt, const PixelHistoryValue &value, ModificationValue &mod)
{
  DecodePixelData(fmt, value.color, mod.col);
}

static float GetDepthValue(VkFormat depthFormat, const PixelHistoryValue &value)
{
  FloatVector v4 = DecodeFormattedComponents(MakeResourceFormat(depthFormat), (byte *)&value.depth);
  return v4.x;
}

rdcarray<PixelModification> VulkanReplay::PixelHistory(rdcarray<EventUsage> events,
                                                       ResourceId target, uint32_t x, uint32_t y,
                                                       const Subresource &sub, CompType typeCast)
{
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
    bool directWrite = IsDirectWrite(events[ev].usage);

    if(events[ev].view != ResourceId())
    {
      VulkanCreationInfo::ImageView viewInfo =
          m_pDriver->GetDebugManager()->GetImageViewInfo(events[ev].view);
      uint32_t layerEnd = viewInfo.range.baseArrayLayer + viewInfo.range.layerCount;
      uint32_t levelEnd = viewInfo.range.baseMipLevel + viewInfo.range.levelCount;
      if(sub.slice < viewInfo.range.baseArrayLayer || sub.slice >= layerEnd ||
         sub.mip < viewInfo.range.baseMipLevel || sub.mip >= levelEnd)
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
    bool directWrite = IsDirectWrite(events[ev].usage);

    if(drawEvents.contains(events[ev].eventId) ||
       (modEvents.contains(events[ev].eventId) && (clear || directWrite)))
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
        if(flags & TestMustFail_StencilTesting)
          mod.stencilTestFailed = true;
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

  // Try to read memory back

  EventInfo *eventsInfo;
  VkResult vkr =
      m_pDriver->vkMapMemory(dev, resources.bufferMemory, 0, VK_WHOLE_SIZE, 0, (void **)&eventsInfo);
  CheckVkResult(vkr);
  if(vkr != VK_SUCCESS)
    return history;
  if(!eventsInfo)
  {
    RDCERR("Manually reporting failed memory map");
    CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
    return history;
  }

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
      mod.preMod.SetInvalid();
      mod.postMod.SetInvalid();
      mod.shaderOut.SetInvalid();
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

    if(frags > 1)
    {
      PixelModification duplicate = mod;
      for(int32_t f = 1; f < frags; f++)
      {
        history.insert(h + 1, duplicate);
      }
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
    uint32_t numFrags = 0;
    for(auto &item : eventsWithFrags)
    {
      numFrags += item.second;
    }

    GetDebugManager()->PixelHistorySetupPerFragResources(resources, (uint32_t)events.size(),
                                                         numFrags);

    callbackInfo.dstBuffer = resources.dstBuffer;

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
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS)
      return history;
    if(!bp)
    {
      RDCERR("Manually reporting failed memory map");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      return history;
    }

    // Retrieve primitive ID values where fragment shader discarded some
    // fragments. For these primitives we are going to perform an occlusion
    // query to see if a primitive was discarded.
    std::map<uint32_t, rdcarray<int32_t>> discardedPrimsEvents;
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
        // Zero out elements the shader didn't write to.
        for(int i = fmt.compCount; i < 4; i++)
          history[h].shaderOut.col.floatValue[i] = 0.0f;

        if((h < history.size() - 1) && (history[h].eventId == history[h + 1].eventId))
        {
          // Get post-modification value if this is not the last fragment for the event.
          FillInColor(fmt, bp[offset].postMod, history[h].postMod);
          // MSAA depth is expanded out to floats in the compute shader
          if((uint32_t)callbackInfo.samples > 1)
            history[h].postMod.depth = bp[offset].postMod.depth.fdepth;
          else
            history[h].postMod.depth =
                GetDepthValue(VK_FORMAT_D32_SFLOAT_S8_UINT, bp[offset].postMod);
          history[h].postMod.stencil = -2;
        }
        // If it is not the first fragment for the event, set the preMod to the
        // postMod of the previous fragment.
        if(h > 0 && (history[h].eventId == history[h - 1].eventId))
        {
          history[h].preMod = history[h - 1].postMod;
        }
      }

      // check the depth value between premod/shaderout against the known test if we have valid
      // depth values, as we don't have per-fragment depth test information.
      if(history[h].preMod.depth >= 0.0f && history[h].shaderOut.depth >= 0.0f && tfCb &&
         tfCb->HasEventFlags(history[h].eventId))
      {
        uint32_t flags = tfCb->GetEventFlags(history[h].eventId);

        flags &= 0x7 << DepthTest_Shift;

        VkFormat dfmt = cb.GetDepthFormat(eid);
        float shadDepth = history[h].shaderOut.depth;

        // quantise depth to match before comparing
        if(dfmt == VK_FORMAT_D24_UNORM_S8_UINT || dfmt == VK_FORMAT_X8_D24_UNORM_PACK32)
        {
          shadDepth = float(uint32_t(float(shadDepth * 0xffffff))) / float(0xffffff);
        }
        else if(dfmt == VK_FORMAT_D16_UNORM || dfmt == VK_FORMAT_D16_UNORM_S8_UINT)
        {
          shadDepth = float(uint32_t(float(shadDepth * 0xffff))) / float(0xffff);
        }

        bool passed = true;
        if(flags == DepthTest_Equal)
          passed = (shadDepth == history[h].preMod.depth);
        else if(flags == DepthTest_NotEqual)
          passed = (shadDepth != history[h].preMod.depth);
        else if(flags == DepthTest_Less)
          passed = (shadDepth < history[h].preMod.depth);
        else if(flags == DepthTest_LessEqual)
          passed = (shadDepth <= history[h].preMod.depth);
        else if(flags == DepthTest_Greater)
          passed = (shadDepth > history[h].preMod.depth);
        else if(flags == DepthTest_GreaterEqual)
          passed = (shadDepth >= history[h].preMod.depth);

        if(!passed)
          history[h].depthTestFailed = true;

        rdcpair<float, float> depthBounds = tfCb->GetEventDepthBounds(history[h].eventId);

        if((history[h].preMod.depth < depthBounds.first ||
            history[h].preMod.depth > depthBounds.second) &&
           depthBounds.second > depthBounds.first)
          history[h].depthBoundsFailed = true;
      }
    }

    m_pDriver->vkUnmapMemory(dev, resources.bufferMemory);
  }

  SAFE_DELETE(tfCb);

  GetDebugManager()->PixelHistoryDestroyResources(resources);
  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), occlusionPool, NULL);
  delete shaderCache;

  return history;
}
