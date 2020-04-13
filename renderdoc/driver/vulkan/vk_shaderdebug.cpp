/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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
#include "driver/shaders/spirv/spirv_debug.h"
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "maths/formatpacking.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

#undef None

RDOC_DEBUG_CONFIG(rdcstr, Vulkan_Debug_PSDebugDumpDirPath, "",
                  "Path to dump pixel shader debugging generated SPIR-V files.");
RDOC_DEBUG_CONFIG(bool, Vulkan_Debug_DisableBufferDeviceAddress, false,
                  "Disable use of buffer device address for PS Input fetch.");

struct DescSetBindingSnapshot
{
  rdcarray<VkDescriptorImageInfo> imageInfos;
  rdcarray<VkDescriptorBufferInfo> buffers;
  rdcarray<VkBufferView> texelBuffers;

  template <typename T>
  const rdcarray<T> &get() const;
};

template <>
const rdcarray<VkDescriptorImageInfo> &DescSetBindingSnapshot::get() const
{
  return imageInfos;
}
template <>
const rdcarray<VkDescriptorBufferInfo> &DescSetBindingSnapshot::get() const
{
  return buffers;
}
template <>
const rdcarray<VkBufferView> &DescSetBindingSnapshot::get() const
{
  return texelBuffers;
}

struct DescSetSnapshot
{
  rdcarray<DescSetBindingSnapshot> bindings;
};

// should match the descriptor set layout created in ShaderDebugData::Init()
enum class ShaderDebugBind
{
  Tex1D = 1,
  First = Tex1D,
  Tex2D,
  Tex3D,
  Tex2DMS,
  Buffer,
  Sampler,
  Count,
};

struct Vec3i
{
  int32_t x, y, z;
};

struct ShaderDebugParameters
{
  uint32_t operation;
  VkBool32 useGrad;
  ShaderDebugBind dim;
  Vec3i texel_uvw;
  int texel_lod;
  Vec3f uvw;
  Vec3f ddx;
  Vec3f ddy;
  Vec3i offset;
  int sampleIdx;
  float compare;
  float lod;
  float minlod;
  rdcspv::GatherChannel gatherChannel;
};

class VulkanAPIWrapper : public rdcspv::DebugAPIWrapper
{
  rdcarray<DescSetSnapshot> m_DescSets;

public:
  VulkanAPIWrapper(WrappedVulkan *vk, VulkanCreationInfo &creation, VkShaderStageFlagBits stage)
      : m_DebugData(vk->GetReplay()->GetShaderDebugData()), m_Creation(creation)
  {
    m_pDriver = vk;

    const VulkanRenderState &state = m_pDriver->GetRenderState();

    const bool compute = (stage == VK_SHADER_STAGE_COMPUTE_BIT);

    // snapshot descriptor set contents
    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descSets =
        compute ? state.compute.descSets : state.graphics.descSets;

    const VulkanCreationInfo::Pipeline &pipe =
        m_Creation.m_Pipeline[compute ? state.compute.pipeline : state.graphics.pipeline];
    const VulkanCreationInfo::PipelineLayout &pipeLayout = m_Creation.m_PipelineLayout[pipe.layout];

    m_DescSets.resize(RDCMIN(descSets.size(), pipeLayout.descSetLayouts.size()));
    for(size_t set = 0; set < m_DescSets.size(); set++)
    {
      uint32_t dynamicOffset = 0;

      DescSetSnapshot &dstSet = m_DescSets[set];

      const rdcarray<DescriptorSetSlot *> &curBinds =
          m_pDriver->GetCurrentDescSetBindings(descSets[set].descSet);
      const DescSetLayout &setLayout = m_Creation.m_DescSetLayout[pipeLayout.descSetLayouts[set]];

      for(size_t bind = 0; bind < setLayout.bindings.size(); bind++)
      {
        const DescSetLayout::Binding &bindLayout = setLayout.bindings[bind];

        if(bindLayout.stageFlags & stage)
        {
          DescriptorSetSlot *curSlots = curBinds[bind];

          dstSet.bindings.resize(bind + 1);

          DescSetBindingSnapshot &dstBind = dstSet.bindings[bind];

          switch(bindLayout.descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
              dstBind.imageInfos.resize(bindLayout.descriptorCount);
              for(uint32_t i = 0; i < bindLayout.descriptorCount; i++)
              {
                dstBind.imageInfos[i].imageLayout = curSlots[i].imageInfo.imageLayout;
                dstBind.imageInfos[i].imageView =
                    m_pDriver->GetResourceManager()->GetCurrentHandle<VkImageView>(
                        curSlots[i].imageInfo.imageView);
                dstBind.imageInfos[i].sampler =
                    m_pDriver->GetResourceManager()->GetCurrentHandle<VkSampler>(
                        bindLayout.immutableSampler ? bindLayout.immutableSampler[i]
                                                    : curSlots[i].imageInfo.sampler);
              }
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
              dstBind.texelBuffers.resize(bindLayout.descriptorCount);
              for(uint32_t i = 0; i < bindLayout.descriptorCount; i++)
              {
                dstBind.texelBuffers[i] =
                    m_pDriver->GetResourceManager()->GetCurrentHandle<VkBufferView>(
                        curSlots[i].texelBufferView);
              }
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
              dstBind.buffers.resize(bindLayout.descriptorCount);
              for(uint32_t i = 0; i < bindLayout.descriptorCount; i++)
              {
                dstBind.buffers[i].offset = curSlots[i].bufferInfo.offset;
                dstBind.buffers[i].range = curSlots[i].bufferInfo.range;
                dstBind.buffers[i].buffer =
                    m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(
                        curSlots[i].bufferInfo.buffer);
              }
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
              dstBind.buffers.resize(bindLayout.descriptorCount);
              for(uint32_t i = 0; i < bindLayout.descriptorCount; i++)
              {
                dstBind.buffers[i].offset = curSlots[i].bufferInfo.offset;
                dstBind.buffers[i].range = curSlots[i].bufferInfo.range;
                dstBind.buffers[i].buffer =
                    m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(
                        curSlots[i].bufferInfo.buffer);

                dstBind.buffers[i].offset += descSets[set].offsets[dynamicOffset++];
              }
              break;
            }
            default: RDCERR("Unexpected descriptor type");
          }
        }
      }
    }
  }

  ~VulkanAPIWrapper()
  {
    m_pDriver->FlushQ();

    VkDevice dev = m_pDriver->GetDev();
    for(auto it = m_SampleViews.begin(); it != m_SampleViews.end(); it++)
      m_pDriver->vkDestroyImageView(dev, it->second, NULL);
    for(auto it = m_BiasSamplers.begin(); it != m_BiasSamplers.end(); it++)
      m_pDriver->vkDestroySampler(dev, it->second, NULL);
  }

  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                               rdcstr d) override
  {
    m_pDriver->AddDebugMessage(c, sv, src, d);
  }

  virtual void ReadConstantBufferValue(uint32_t set, uint32_t bind, uint32_t offset,
                                       uint32_t byteSize, void *dst) override
  {
    rdcpair<uint32_t, uint32_t> key = make_rdcpair(set, bind);
    auto insertIt = cbufferCache.insert(std::make_pair(key, bytebuf()));
    bytebuf &data = insertIt.first->second;
    if(insertIt.second)
    {
      // TODO handle arrays here
      BindpointIndex index(set, bind, 0);

      bool valid = true;
      const VkDescriptorBufferInfo &bufData =
          GetDescriptor<VkDescriptorBufferInfo>("reading constant buffer value", index, valid);
      if(valid)
        m_pDriver->GetDebugManager()->GetBufferData(GetResID(bufData.buffer), bufData.offset,
                                                    bufData.range, data);
    }

    if(offset + byteSize <= data.size())
      memcpy(dst, data.data() + offset, byteSize);
  }

  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t location,
                              uint32_t component) override
  {
    if(builtin != ShaderBuiltin::Undefined)
    {
      auto it = builtin_inputs.find(builtin);
      if(it != builtin_inputs.end())
      {
        var.value = it->second.value;
        return;
      }

      RDCERR("Couldn't get input for %s", ToStr(builtin).c_str());
      return;
    }

    // TODO handle components
    RDCASSERT(component == 0);

    if(location < location_inputs.size())
    {
      var.value = location_inputs[location].value;
      return;
    }

    RDCERR("Couldn't get input for %s at location=%u, component=%u", var.name.c_str(), location,
           component);
  }

  virtual DerivativeDeltas GetDerivative(ShaderBuiltin builtin, uint32_t location,
                                         uint32_t component) override
  {
    if(builtin != ShaderBuiltin::Undefined)
    {
      auto it = builtin_derivatives.find(builtin);
      if(it != builtin_derivatives.end())
        return it->second;

      RDCERR("Couldn't get input for %s", ToStr(builtin).c_str());
      return DerivativeDeltas();
    }

    // TODO handle components
    RDCASSERT(component == 0);

    if(location < location_derivatives.size())
      return location_derivatives[location];

    RDCERR("Couldn't get derivative for location=%u, component=%u", location, component);
    return DerivativeDeltas();
  }

  bool CalculateSampleGather(rdcspv::ThreadState &lane, rdcspv::Op opcode,
                             DebugAPIWrapper::TextureType texType, BindpointIndex imageBind,
                             BindpointIndex samplerBind, const ShaderVariable &uv,
                             const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                             const ShaderVariable &compare, rdcspv::GatherChannel gatherChannel,
                             const rdcspv::ImageOperandsAndParamDatas &operands,
                             ShaderVariable &output) override
  {
    ShaderDebugParameters params = {};

    const bool buffer = (texType & DebugAPIWrapper::Buffer_Texture) != 0;
    const bool uintTex = (texType & DebugAPIWrapper::UInt_Texture) != 0;
    const bool sintTex = (texType & DebugAPIWrapper::SInt_Texture) != 0;

    // fetch the right type of descriptor depending on if we're buffer or not
    BindpointIndex invalidIndex(-1, -1, ~0U);
    bool valid = true;
    rdcstr access = StringFormat::Fmt("performing %s operation", ToStr(opcode).c_str());
    const VkDescriptorImageInfo &imageInfo =
        buffer ? GetDescriptor<VkDescriptorImageInfo>(access, invalidIndex, valid)
               : GetDescriptor<VkDescriptorImageInfo>(access, imageBind, valid);
    const VkBufferView &bufferView = buffer
                                         ? GetDescriptor<VkBufferView>(access, imageBind, valid)
                                         : GetDescriptor<VkBufferView>(access, invalidIndex, valid);

    // fetch the sampler (if there's no sampler, this will silently return dummy data without
    // marking invalid
    const VkDescriptorImageInfo &samplerInfo =
        GetDescriptor<VkDescriptorImageInfo>(access, samplerBind, valid);

    // if any descriptor lookup failed, return now
    if(!valid)
      return false;

    VkMarkerRegion markerRegion("CalculateSampleGather");

    VkSampler sampler = samplerInfo.sampler;
    VkImageView view = imageInfo.imageView;
    VkImageLayout layout = imageInfo.imageLayout;

    // promote view to Array view

    const VulkanCreationInfo::ImageView &viewProps = m_Creation.m_ImageView[GetResID(view)];
    const VulkanCreationInfo::Image &imageProps = m_Creation.m_Image[viewProps.image];
    VkImageType imageType = imageProps.type;
    uint32_t samples = (uint32_t)imageProps.samples;

    VkDevice dev = m_pDriver->GetDev();

    // how many co-ordinates should there be
    int coords = 0, gradCoords;
    switch(viewProps.viewType)
    {
      case VK_IMAGE_VIEW_TYPE_1D:
        coords = 1;
        gradCoords = 1;
        break;
      case VK_IMAGE_VIEW_TYPE_2D:
        coords = 2;
        gradCoords = 2;
        break;
      case VK_IMAGE_VIEW_TYPE_3D:
        coords = 3;
        gradCoords = 3;
        break;
      case VK_IMAGE_VIEW_TYPE_CUBE:
        coords = 3;
        gradCoords = 3;
        break;
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        coords = 2;
        gradCoords = 1;
        break;
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        coords = 3;
        gradCoords = 2;
        break;
      case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        coords = 4;
        gradCoords = 3;
        break;
      case VK_IMAGE_VIEW_TYPE_RANGE_SIZE:
      case VK_IMAGE_VIEW_TYPE_MAX_ENUM:
        RDCERR("Invalid image view type %s", ToStr(viewProps.viewType).c_str());
        return false;
    }

    // create our own view (if we haven't already for this view) so we can promote to array
    VkImageView sampleView = m_SampleViews[GetResID(view)];
    if(sampleView == VK_NULL_HANDLE)
    {
      VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      viewInfo.image = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(viewProps.image);
      viewInfo.format = viewProps.format;
      viewInfo.viewType = viewProps.viewType;
      if(viewInfo.viewType == VK_IMAGE_VIEW_TYPE_1D)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
      else if(viewInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

      viewInfo.components = viewProps.componentMapping;
      viewInfo.subresourceRange = viewProps.range;

      // if KHR_maintenance2 is available, ensure we have sampled usage available
      VkImageViewUsageCreateInfo usageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
      if(m_pDriver->GetExtensions(NULL).ext_KHR_maintenance2)
      {
        usageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        viewInfo.pNext = &usageCreateInfo;
      }

      VkResult vkr = m_pDriver->vkCreateImageView(dev, &viewInfo, NULL, &sampleView);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_SampleViews[GetResID(view)] = sampleView;
    }

    if(operands.flags & rdcspv::ImageOperands::Bias)
    {
      float bias = lane.GetSrc(operands.bias).value.f.x;
      if(bias != 0.0f)
      {
        // bias can only be used with implicit lod operations, but we want to do everything with
        // explicit lod operations. So we instead push the bias into a new sampler, which is
        // entirely equivalent.

        // first check to see if we have one already, since the bias is probably going to be
        // coherent.
        SamplerBiasKey key = {GetResID(sampler), bias};

        auto insertIt = m_BiasSamplers.insert(std::make_pair(key, VkSampler()));
        if(insertIt.second)
        {
          const VulkanCreationInfo::Sampler &samplerProps = m_Creation.m_Sampler[key.first];

          VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
          sampInfo.magFilter = samplerProps.magFilter;
          sampInfo.minFilter = samplerProps.minFilter;
          sampInfo.mipmapMode = samplerProps.mipmapMode;
          sampInfo.addressModeU = samplerProps.address[0];
          sampInfo.addressModeV = samplerProps.address[1];
          sampInfo.addressModeW = samplerProps.address[2];
          sampInfo.mipLodBias = samplerProps.mipLodBias;
          sampInfo.maxAnisotropy = samplerProps.maxAnisotropy;
          sampInfo.compareEnable = samplerProps.compareEnable;
          sampInfo.compareOp = samplerProps.compareOp;
          sampInfo.minLod = samplerProps.minLod;
          sampInfo.maxLod = samplerProps.maxLod;
          sampInfo.borderColor = samplerProps.borderColor;
          sampInfo.unnormalizedCoordinates = samplerProps.unnormalizedCoordinates;

          VkSamplerReductionModeCreateInfo reductionInfo = {
              VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO};
          if(samplerProps.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE)
          {
            reductionInfo.reductionMode = samplerProps.reductionMode;
            reductionInfo.pNext = sampInfo.pNext;
            sampInfo.pNext = &reductionInfo;
          }

          VkSamplerYcbcrConversionInfo ycbcrInfo = {VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};
          if(samplerProps.ycbcr != ResourceId())
          {
            ycbcrInfo.conversion =
                m_pDriver->GetResourceManager()->GetCurrentHandle<VkSamplerYcbcrConversion>(
                    viewProps.image);
            ycbcrInfo.pNext = sampInfo.pNext;
            sampInfo.pNext = &ycbcrInfo;
          }

          // now add the shader's bias on
          sampInfo.mipLodBias += bias;

          VkResult vkr = m_pDriver->vkCreateSampler(dev, &sampInfo, NULL, &sampler);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          insertIt.first->second = sampler;
        }
        else
        {
          sampler = insertIt.first->second;
        }
      }
    }

    params.operation = (uint32_t)opcode;

    switch(imageType)
    {
      case VK_IMAGE_TYPE_1D: params.dim = ShaderDebugBind::Tex1D; break;
      case VK_IMAGE_TYPE_2D:
        params.dim = ShaderDebugBind::Tex2D;
        if(samples > 1)
          params.dim = ShaderDebugBind::Tex2DMS;
        break;
      case VK_IMAGE_TYPE_3D: params.dim = ShaderDebugBind::Tex3D; break;
      default:
      {
        RDCERR("Unsupported image type %s", ToStr(imageType).c_str());
        return false;
      }
    }

    if(buffer)
      params.dim = ShaderDebugBind::Buffer;

    switch(opcode)
    {
      case rdcspv::Op::ImageFetch:
      {
        // co-ordinates after the used ones are read as 0s. This allows us to then read an implicit
        // 0 for array layer when we promote accesses to arrays.
        params.texel_uvw.x = uv.value.u.x;
        if(coords >= 2)
          params.texel_uvw.y = uv.value.u.y;
        if(coords >= 3)
          params.texel_uvw.z = uv.value.u.z;

        if(!buffer)
          params.texel_lod = lane.GetSrc(operands.lod).value.i.x;

        if(operands.flags & rdcspv::ImageOperands::Sample)
          params.sampleIdx = lane.GetSrc(operands.sample).value.u.x;

        break;
      }
      case rdcspv::Op::ImageSampleExplicitLod:
      case rdcspv::Op::ImageSampleImplicitLod:
      {
        params.uvw.x = uv.value.f.x;
        if(coords >= 2)
          params.uvw.y = uv.value.f.y;
        if(coords >= 3)
          params.uvw.z = uv.value.f.z;

        if(operands.flags & rdcspv::ImageOperands::MinLod)
          params.minlod = lane.GetSrc(operands.minLod).value.f.x;

        if(operands.flags & rdcspv::ImageOperands::Lod)
        {
          params.lod = lane.GetSrc(operands.lod).value.f.x;
          params.useGrad = VK_FALSE;
        }
        else if(operands.flags & rdcspv::ImageOperands::Grad)
        {
          ShaderVariable ddx = lane.GetSrc(operands.grad.first);
          ShaderVariable ddy = lane.GetSrc(operands.grad.second);

          params.useGrad = VK_TRUE;

          params.ddx.x = ddx.value.f.x;
          if(gradCoords >= 2)
            params.ddx.y = ddx.value.f.y;
          if(gradCoords >= 3)
            params.ddx.z = ddx.value.f.z;

          params.ddy.x = ddy.value.f.x;
          if(gradCoords >= 2)
            params.ddy.y = ddy.value.f.y;
          if(gradCoords >= 3)
            params.ddy.z = ddy.value.f.z;
        }

        if(operands.flags & rdcspv::ImageOperands::ConstOffset)
        {
          ShaderVariable constOffset = lane.GetSrc(operands.constOffset);
          params.offset.x = constOffset.value.i.x;
          if(gradCoords >= 2)
            params.offset.y = constOffset.value.i.y;
          if(gradCoords >= 3)
            params.offset.z = constOffset.value.i.z;
        }
        else if(operands.flags & rdcspv::ImageOperands::Offset)
        {
          ShaderVariable offset = lane.GetSrc(operands.offset);
          params.offset.x = offset.value.i.x;
          if(gradCoords >= 2)
            params.offset.y = offset.value.i.y;
          if(gradCoords >= 3)
            params.offset.z = offset.value.i.z;
        }

        break;
      }
      default:
      {
        RDCERR("Unsupported opcode %s", ToStr(opcode).c_str());
        return false;
      }
    }

    VkPipeline pipe = MakePipe(params, uintTex, sintTex);

    VkDescriptorImageInfo samplerWriteInfo = {Unwrap(sampler), VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_UNDEFINED};
    VkDescriptorImageInfo imageWriteInfo = {VK_NULL_HANDLE, Unwrap(sampleView), layout};

    VkWriteDescriptorSet writeSets[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_DebugData.DescSet),
            (uint32_t)params.dim, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageWriteInfo, NULL, NULL,
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_DebugData.DescSet),
            (uint32_t)ShaderDebugBind::Sampler, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &samplerWriteInfo,
            NULL, NULL,
        },
    };

    if(buffer)
    {
      writeSets[0].pTexelBufferView = &bufferView;
      writeSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    }

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), sampler != VK_NULL_HANDLE ? 2 : 1, writeSets, 0,
                                       NULL);

    {
      VkCommandBuffer cmd = m_pDriver->GetNextCmd();

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkClearValue clear = {};

      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          Unwrap(m_DebugData.RenderPass),
          Unwrap(m_DebugData.Framebuffer),
          {{0, 0}, {1, 1}},
          1,
          &clear,
      };
      ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
      ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          Unwrap(m_DebugData.PipeLayout), 0, 1,
                                          UnwrapPtr(m_DebugData.DescSet), 0, NULL);

      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

      ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));

      VkBufferImageCopy region = {
          0, sizeof(Vec4f), 1, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {1, 1, 1},
      };
      ObjDisp(cmd)->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(m_DebugData.Image),
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         Unwrap(m_DebugData.ReadbackBuffer.buf), 1, &region);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

    Vec4f *ret = (Vec4f *)m_DebugData.ReadbackBuffer.Map(NULL, 0);

    memcpy(output.value.uv, ret, sizeof(Vec4f));

    m_DebugData.ReadbackBuffer.Unmap();

    return true;
  }

  std::map<ShaderBuiltin, ShaderVariable> builtin_inputs;
  rdcarray<ShaderVariable> location_inputs;

  std::map<ShaderBuiltin, DerivativeDeltas> builtin_derivatives;
  rdcarray<DerivativeDeltas> location_derivatives;

private:
  WrappedVulkan *m_pDriver = NULL;
  ShaderDebugData &m_DebugData;
  VulkanCreationInfo &m_Creation;

  std::map<ResourceId, VkImageView> m_SampleViews;

  typedef rdcpair<ResourceId, float> SamplerBiasKey;
  std::map<SamplerBiasKey, VkSampler> m_BiasSamplers;

  std::map<rdcpair<uint32_t, uint32_t>, bytebuf> cbufferCache;
  template <typename T>
  const T &GetDescriptor(const rdcstr &access, BindpointIndex index, bool &valid)
  {
    static T dummy = {};

    if(index.bindset < 0)
    {
      // invalid index, return a dummy data but don't mark as invalid
      return dummy;
    }

    if(index.bindset >= m_DescSets.count())
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Out of bounds access to unbound descriptor set %u (binding %u) when %s",
              index.bindset, index.bind, access.c_str()));
      valid = false;
      return dummy;
    }

    const DescSetSnapshot &setData = m_DescSets[index.bindset];

    if(index.bind >= setData.bindings.count())
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Out of bounds access to non-existant descriptor set %u binding %u when %s",
              index.bindset, index.bind, access.c_str()));
      valid = false;
      return dummy;
    }

    const DescSetBindingSnapshot &bindData = setData.bindings[index.bind];

    const rdcarray<T> &elemData = bindData.get<T>();

    if(elemData.empty())
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt("descriptor set %u binding %u is not bound, when %s", index.bindset,
                            index.bind, access.c_str()));
      valid = false;
      return dummy;
    }

    if(index.arrayIndex >= elemData.size())
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("descriptor set %u binding %u has %zu "
                                                   "descriptors, index %u is out of bounds when %s",
                                                   index.bindset, index.bind, elemData.size(),
                                                   index.arrayIndex, access.c_str()));
      valid = false;
      return dummy;
    }

    return elemData[index.arrayIndex];
  }

  VkPipeline MakePipe(const ShaderDebugParameters &params, bool uintTex, bool sintTex)
  {
    VkSpecializationMapEntry specMaps[sizeof(params) / sizeof(uint32_t)];
    for(size_t i = 0; i < ARRAY_COUNT(specMaps); i++)
    {
      specMaps[i].constantID = uint32_t(i);
      specMaps[i].offset = uint32_t(sizeof(uint32_t) * i);
      specMaps[i].size = sizeof(uint32_t);
    }

    VkSpecializationInfo specInfo = {};
    specInfo.dataSize = sizeof(params);
    specInfo.pData = &params;
    specInfo.mapEntryCount = ARRAY_COUNT(specMaps);
    specInfo.pMapEntries = specMaps;

    uint32_t shaderIndex = 0;
    if(uintTex)
      shaderIndex = 1;
    else if(sintTex)
      shaderIndex = 2;

    if(m_DebugData.Module[shaderIndex] == VK_NULL_HANDLE)
    {
      rdcarray<uint32_t> spirv;
      GenerateShaderModule(spirv, uintTex, sintTex);

      VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      moduleCreateInfo.pCode = spirv.data();
      moduleCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);

      VkResult vkr = m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &moduleCreateInfo, NULL,
                                                     &m_DebugData.Module[shaderIndex]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      const char *filename[] = {
          "/debug_psgather_float.spv", "/debug_psgather_uint.spv", "/debug_psgather_sint.spv",
      };

      if(!Vulkan_Debug_PSDebugDumpDirPath.empty())
        FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath + filename[shaderIndex], spirv);
    }

    const VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
         m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::BlitVS), "main", NULL},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
         m_DebugData.Module[shaderIndex], "main", &specInfo},
    };

    const VkPipelineDynamicStateCreateInfo dynamicState = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    };

    const VkPipelineMultisampleStateCreateInfo msaa = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, VK_SAMPLE_COUNT_1_BIT,
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencil = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    VkPipelineColorBlendAttachmentState colAttach = {};
    colAttach.colorWriteMask = 0xf;

    const VkPipelineColorBlendStateCreateInfo colorBlend = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        NULL,
        0,
        false,
        VK_LOGIC_OP_NO_OP,
        1,
        &colAttach,
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    };

    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkRect2D s = {};
    s.extent.width = s.extent.height = 1;

    VkViewport v = {};
    v.width = v.height = v.maxDepth = 1.0f;

    VkPipelineViewportStateCreateInfo viewScissor = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    };
    viewScissor.viewportCount = viewScissor.scissorCount = 1;
    viewScissor.pScissors = &s;
    viewScissor.pViewports = &v;

    VkPipelineRasterizationStateCreateInfo raster = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    };

    raster.lineWidth = 1.0f;

    const VkGraphicsPipelineCreateInfo graphicsPipeInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        NULL,
        0,
        2,
        shaderStages,
        &vertexInput,
        &inputAssembly,
        NULL,    // tess
        &viewScissor,
        &raster,
        &msaa,
        &depthStencil,
        &colorBlend,
        &dynamicState,
        m_DebugData.PipeLayout,
        m_DebugData.RenderPass,
        0,                 // sub pass
        VK_NULL_HANDLE,    // base pipeline handle
        -1,                // base pipeline index
    };

    VkPipeline pipe = VK_NULL_HANDLE;
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(), VK_NULL_HANDLE, 1,
                                                        &graphicsPipeInfo, NULL, &pipe);
    if(vkr != VK_SUCCESS)
    {
      RDCERR("Failed creating debug pipeline");
      return VK_NULL_HANDLE;
    }

    return pipe;
  }

  void GenerateShaderModule(rdcarray<uint32_t> &spirv, bool uintTex, bool sintTex)
  {
    // this could be done as a glsl shader, but glslang has some bugs compiling the specialisation
    // constants, so we generate it by hand - which isn't too hard

    rdcspv::Editor editor(spirv);

    // create as SPIR-V 1.0 for best compatibility
    editor.CreateEmpty(1, 0);

    editor.AddCapability(rdcspv::Capability::Shader);
    editor.AddCapability(rdcspv::Capability::Sampled1D);
    editor.AddCapability(rdcspv::Capability::SampledBuffer);

    if(m_pDriver->GetDeviceFeatures().shaderResourceMinLod)
      editor.AddCapability(rdcspv::Capability::MinLod);
    if(m_pDriver->GetDeviceFeatures().shaderImageGatherExtended)
      editor.AddCapability(rdcspv::Capability::ImageGatherExtended);

    rdcspv::Id entryId = editor.MakeId();

    editor.AddOperation(
        editor.Begin(rdcspv::Section::MemoryModel),
        rdcspv::OpMemoryModel(rdcspv::AddressingModel::Logical, rdcspv::MemoryModel::GLSL450));

    rdcspv::Id u32 = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id i32 = editor.DeclareType(rdcspv::scalar<int32_t>());
    rdcspv::Id f32 = editor.DeclareType(rdcspv::scalar<float>());

    rdcspv::Id v2i32 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<int32_t>(), 2));
    rdcspv::Id v3i32 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<int32_t>(), 3));
    rdcspv::Id v2f32 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 2));
    rdcspv::Id v3f32 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 3));

    rdcspv::Scalar base = rdcspv::scalar<float>();
    if(uintTex)
      base = rdcspv::scalar<uint32_t>();
    else if(sintTex)
      base = rdcspv::scalar<int32_t>();

    rdcspv::Id resultType = editor.DeclareType(rdcspv::Vector(base, 4));

// add specialisation constants for all the parameters
#define SPEC_ID(name) uint32_t(offsetof(ShaderDebugParameters, name) / sizeof(uint32_t))

#define DECL_SPECID(type, name, value)                                         \
  rdcspv::Id name = editor.AddSpecConstantImmediate<type>(0U, SPEC_ID(value)); \
  editor.SetName(name, "spec_" #name);

    DECL_SPECID(uint32_t, operation, operation);
    DECL_SPECID(bool, useGrad, useGrad);
    DECL_SPECID(uint32_t, dim, dim);
    DECL_SPECID(int32_t, texel_u, texel_uvw.x);
    DECL_SPECID(int32_t, texel_v, texel_uvw.y);
    DECL_SPECID(int32_t, texel_w, texel_uvw.z);
    DECL_SPECID(int32_t, texel_lod, texel_lod);
    DECL_SPECID(float, u, uvw.x);
    DECL_SPECID(float, v, uvw.y);
    DECL_SPECID(float, w, uvw.z);
    DECL_SPECID(float, dudx, ddx.x);
    DECL_SPECID(float, dvdx, ddx.y);
    DECL_SPECID(float, dwdx, ddx.z);
    DECL_SPECID(float, dudy, ddy.x);
    DECL_SPECID(float, dvdy, ddy.y);
    DECL_SPECID(float, dwdy, ddy.z);
    DECL_SPECID(int32_t, offset_x, offset.x);
    DECL_SPECID(int32_t, offset_y, offset.y);
    DECL_SPECID(int32_t, offset_z, offset.z);
    DECL_SPECID(int32_t, sampleIdx, sampleIdx);
    DECL_SPECID(float, compare, compare);
    DECL_SPECID(float, lod, lod);
    DECL_SPECID(float, minlod, minlod);
    DECL_SPECID(int32_t, gatherChannel, gatherChannel);

    rdcspv::Id texel_uv = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {texel_u, texel_v}));
    rdcspv::Id texel_uvw = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v3i32, editor.MakeId(), {texel_u, texel_v, texel_w}));

    editor.SetName(texel_uv, "texel_uv");
    editor.SetName(texel_uvw, "texel_uvw");

    rdcspv::Id uv =
        editor.AddConstant(rdcspv::OpSpecConstantComposite(v2f32, editor.MakeId(), {u, v}));
    rdcspv::Id uvw =
        editor.AddConstant(rdcspv::OpSpecConstantComposite(v3f32, editor.MakeId(), {u, v, w}));

    editor.SetName(uv, "uv");
    editor.SetName(uvw, "uvw");

    rdcspv::Id ddx_uv =
        editor.AddConstant(rdcspv::OpSpecConstantComposite(v2f32, editor.MakeId(), {dudx, dvdx}));
    rdcspv::Id ddx_uvw = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v3f32, editor.MakeId(), {dudx, dvdx, dwdx}));

    editor.SetName(ddx_uv, "ddx_uv");
    editor.SetName(ddx_uvw, "ddx_uvw");

    rdcspv::Id ddy_uv =
        editor.AddConstant(rdcspv::OpSpecConstantComposite(v2f32, editor.MakeId(), {dudy, dvdy}));
    rdcspv::Id ddy_uvw = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v3f32, editor.MakeId(), {dudy, dvdy, dwdy}));

    editor.SetName(ddy_uv, "ddy_uv");
    editor.SetName(ddy_uvw, "ddy_uvw");

    rdcspv::Id offset_xy = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {offset_x, offset_y}));
    rdcspv::Id offset_xyz = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v3i32, editor.MakeId(), {offset_x, offset_y, offset_z}));

    editor.SetName(offset_xy, "offset_xy");
    editor.SetName(offset_xyz, "offset_xyz");

    // create the output. It's always a 4-wide vector
    rdcspv::Id outPtrType =
        editor.DeclareType(rdcspv::Pointer(resultType, rdcspv::StorageClass::Output));

    rdcspv::Id outVar = editor.AddVariable(
        rdcspv::OpVariable(outPtrType, editor.MakeId(), rdcspv::StorageClass::Output));
    editor.AddDecoration(
        rdcspv::OpDecorate(outVar, rdcspv::DecorationParam<rdcspv::Decoration::Location>(0)));

    editor.SetName(outVar, "output");

    rdcspv::ImageFormat unk = rdcspv::ImageFormat::Unknown;

    // create the five textures and sampler
    rdcspv::Id texSampTypes[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::_1D, 0, 1, 0, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::_2D, 0, 1, 0, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::_3D, 0, 1, 0, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::_2D, 0, 1, 1, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::Buffer, 0, 1, 0, 1, unk)),
        editor.DeclareType(rdcspv::Sampler()),
    };
    rdcspv::Id texSampVars[(uint32_t)ShaderDebugBind::Count];
    rdcspv::Id texSampCombinedTypes[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[1])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[2])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[3])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[4])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[5])),
        rdcspv::Id(),
    };

    for(size_t i = (size_t)ShaderDebugBind::First; i < (size_t)ShaderDebugBind::Count; i++)
    {
      rdcspv::Id ptrType =
          editor.DeclareType(rdcspv::Pointer(texSampTypes[i], rdcspv::StorageClass::UniformConstant));

      texSampVars[i] = editor.AddVariable(
          rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::UniformConstant));

      editor.AddDecoration(rdcspv::OpDecorate(
          texSampVars[i], rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0U)));
      editor.AddDecoration(rdcspv::OpDecorate(
          texSampVars[i], rdcspv::DecorationParam<rdcspv::Decoration::Binding>((uint32_t)i)));
    }

    editor.SetName(texSampVars[(size_t)ShaderDebugBind::Tex1D], "Tex1D");
    editor.SetName(texSampVars[(size_t)ShaderDebugBind::Tex2D], "Tex2D");
    editor.SetName(texSampVars[(size_t)ShaderDebugBind::Tex3D], "Tex3D");
    editor.SetName(texSampVars[(size_t)ShaderDebugBind::Tex2DMS], "Tex2DMS");
    editor.SetName(texSampVars[(size_t)ShaderDebugBind::Buffer], "Buffer");
    editor.SetName(texSampVars[(size_t)ShaderDebugBind::Sampler], "Sampler");

    rdcspv::Id sampVar = texSampVars[(size_t)ShaderDebugBind::Sampler];

    // register the entry point
    editor.AddOperation(
        editor.Begin(rdcspv::Section::EntryPoints),
        rdcspv::OpEntryPoint(rdcspv::ExecutionModel::Fragment, entryId, "main", {outVar}));
    editor.AddOperation(editor.Begin(rdcspv::Section::ExecutionMode),
                        rdcspv::OpExecutionMode(entryId, rdcspv::ExecutionMode::OriginUpperLeft));

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());
    rdcspv::Id funcType = editor.DeclareType(rdcspv::FunctionType(voidType, {}));

    rdcspv::OperationList func;
    func.add(rdcspv::OpFunction(voidType, entryId, rdcspv::FunctionControl::None, funcType));
    func.add(rdcspv::OpLabel(editor.MakeId()));

    // first store NULL data in, so the output is always initialised

    rdcspv::Id breakLabel = editor.MakeId();
    rdcspv::Id defaultLabel = editor.MakeId();

    // combine the operation with the image type:
    // operation * 10 + dim
    RDCCOMPILE_ASSERT(size_t(ShaderDebugBind::Count) < 10, "Combining value ranges will overlap!");
    rdcspv::Id switchVal = func.add(rdcspv::OpIMul(u32, editor.MakeId(), operation,
                                                   editor.AddConstantImmediate<uint32_t>(10U)));
    switchVal = func.add(rdcspv::OpIAdd(u32, editor.MakeId(), switchVal, dim));

    // switch on the combined operation and image type value
    rdcarray<rdcspv::PairLiteralIntegerIdRef> targets;

    rdcspv::OperationList cases;

    rdcspv::Op op;

    rdcspv::Id texel_coord[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        texel_uv,     // 1D - u and array
        texel_uvw,    // 2D - u,v and array
        texel_uvw,    // 3D - u,v,w
        texel_uvw,    // 2DMS - u,v and array
        texel_u,      // Buffer - u
    };

    rdcspv::Id coord[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        uv,     // 1D - u and array
        uvw,    // 2D - u,v and array
        uvw,    // 3D - u,v,w
        uvw,    // 2DMS - u,v and array
        u,      // Buffer - u
    };

    rdcspv::Id offsets[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        offset_x,      // 1D - u
        offset_xy,     // 2D - u,v
        offset_xyz,    // 3D - u,v,w
        offset_xy,     // 2DMS - u,v
        offset_x,      // Buffer - u
    };

    rdcspv::Id ddxs[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        dudx,       // 1D - u
        ddx_uv,     // 2D - u,v
        ddx_uvw,    // 3D - u,v,w
        ddx_uv,     // 2DMS - u,v
        dudx,       // Buffer - u
    };

    rdcspv::Id ddys[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        dudy,       // 1D - u
        ddy_uv,     // 2D - u,v
        ddy_uvw,    // 3D - u,v,w
        ddy_uv,     // 2DMS - u,v
        dudy,       // Buffer - u
    };

    uint32_t sampIdx = (uint32_t)ShaderDebugBind::Sampler;

    // for(uint32_t i = (uint32_t)ShaderDebugBind::First; i < (uint32_t)ShaderDebugBind::Count; i++)
    uint32_t i = (uint32_t)ShaderDebugBind::Tex2D;
    {
      {
        op = rdcspv::Op::ImageFetch;

        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        rdcspv::ImageOperandsAndParamDatas imageOperands;

        if(i != (uint32_t)ShaderDebugBind::Buffer)
          imageOperands.setLod(texel_lod);

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loaded =
            cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), texSampVars[i]));
        rdcspv::Id sampleResult = cases.add(rdcspv::OpImageFetch(
            resultType, editor.MakeId(), loaded, texel_coord[i], imageOperands));
        cases.add(rdcspv::OpStore(outVar, sampleResult));
        cases.add(rdcspv::OpBranch(breakLabel));
      }

      {
        op = rdcspv::Op::ImageSampleExplicitLod;
        op = rdcspv::Op::ImageSampleImplicitLod;

        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        rdcspv::ImageOperandsAndParamDatas imageOperands;

        imageOperands.setConstOffset(offsets[i]);

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loadedImage =
            cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), texSampVars[i]));
        rdcspv::Id loadedSampler =
            cases.add(rdcspv::OpLoad(texSampTypes[sampIdx], editor.MakeId(), texSampVars[sampIdx]));

        rdcspv::Id mergeLabel = editor.MakeId();
        rdcspv::Id gradCase = editor.MakeId();
        rdcspv::Id lodCase = editor.MakeId();
        cases.add(rdcspv::OpSelectionMerge(mergeLabel, rdcspv::SelectionControl::None));
        cases.add(rdcspv::OpBranchConditional(useGrad, gradCase, lodCase));

        rdcspv::Id lodResult;
        {
          cases.add(rdcspv::OpLabel(lodCase));
          rdcspv::ImageOperandsAndParamDatas operands = imageOperands;
          operands.setLod(lod);
          rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
              texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));
          lodResult = cases.add(rdcspv::OpImageSampleExplicitLod(resultType, editor.MakeId(),
                                                                 combined, coord[i], operands));
          cases.add(rdcspv::OpBranch(mergeLabel));
        }

        rdcspv::Id gradResult;
        {
          cases.add(rdcspv::OpLabel(gradCase));
          rdcspv::ImageOperandsAndParamDatas operands = imageOperands;
          operands.setGrad(ddxs[i], ddys[i]);
          rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
              texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));
          gradResult = cases.add(rdcspv::OpImageSampleExplicitLod(resultType, editor.MakeId(),
                                                                  combined, coord[i], operands));
          cases.add(rdcspv::OpBranch(mergeLabel));
        }

        cases.add(rdcspv::OpLabel(mergeLabel));
        rdcspv::Id sampleResult = cases.add(rdcspv::OpPhi(
            resultType, editor.MakeId(), {{lodResult, lodCase}, {gradResult, gradCase}}));
        cases.add(rdcspv::OpStore(outVar, sampleResult));
        cases.add(rdcspv::OpBranch(breakLabel));
      }
    }

    func.add(rdcspv::OpSelectionMerge(breakLabel, rdcspv::SelectionControl::None));
    func.add(rdcspv::OpSwitch(switchVal, defaultLabel, targets));

    func.append(cases);

    // default: store NULL data
    func.add(rdcspv::OpLabel(defaultLabel));
    func.add(rdcspv::OpStore(
        outVar, editor.AddConstant(rdcspv::OpConstantNull(resultType, editor.MakeId()))));
    func.add(rdcspv::OpBranch(breakLabel));

    func.add(rdcspv::OpLabel(breakLabel));
    func.add(rdcspv::OpReturn());
    func.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(func);
  }
};

enum StorageMode
{
  Binding,
  EXT_bda,
  KHR_bda,
};

enum class InputSpecConstant
{
  Address = 0,
  ArrayLength,
  DestX,
  DestY,
  Count,
};

static const uint32_t validMagicNumber = 12345;

struct PSHit
{
  Vec4f pos;
  uint32_t prim;
  uint32_t sample;
  uint32_t valid;
  uint32_t padding;
  // PSInput base, ddx, ....
};

static void CreatePSInputFetcher(rdcarray<uint32_t> &fragspv, uint32_t &structStride,
                                 VulkanCreationInfo::ShaderModuleReflection &shadRefl,
                                 StorageMode storageMode, bool usePrimitiveID, bool useSampleID)
{
  rdcspv::Editor editor(fragspv);

  editor.Prepare();

  // first delete all functions. We will recreate the entry point with just what we need
  {
    rdcarray<rdcspv::Id> removedIds;

    rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions);
    rdcspv::Iter end = editor.End(rdcspv::Section::Functions);
    while(it < end)
    {
      removedIds.push_back(rdcspv::OpDecoder(it).result);
      editor.Remove(it);
      it++;
    }

    // remove any OpName that refers to deleted IDs - functions or results
    it = editor.Begin(rdcspv::Section::Debug);
    end = editor.End(rdcspv::Section::Debug);
    while(it < end)
    {
      if(it.opcode() == rdcspv::Op::Name)
      {
        rdcspv::OpName name(it);

        if(removedIds.contains(name.target))
          editor.Remove(it);
      }
      it++;
    }
  }

  rdcspv::MemoryAccessAndParamDatas alignedAccess;
  alignedAccess.setAligned(sizeof(uint32_t));

  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id floatType = editor.DeclareType(rdcspv::scalar<float>());
  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());

  rdcarray<rdcspv::Id> uintConsts;

  auto getUIntConst = [&uintConsts, &editor](uint32_t c) {
    for(uint32_t i = (uint32_t)uintConsts.size(); i <= c; i++)
      uintConsts.push_back(editor.AddConstantImmediate<uint32_t>(uint32_t(i)));

    return uintConsts[c];
  };

  rdcspv::StorageClass bufferClass;

  if(storageMode == Binding)
    bufferClass = editor.StorageBufferClass();
  else
    bufferClass = rdcspv::StorageClass::PhysicalStorageBuffer;

  // remove all other entry point
  rdcspv::Id entryID;
  for(const rdcspv::EntryPoint &e : editor.GetEntries())
  {
    if(e.name == shadRefl.entryPoint)
    {
      entryID = e.id;
      break;
    }
  }

  rdcarray<rdcspv::Id> addedInputs;

  // builtin inputs we need
  struct BuiltinAccess
  {
    rdcspv::Id base;
    uint32_t member = ~0U;
  } fragCoord, primitiveID, sampleIndex;

  // look to see which ones are already provided
  for(size_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
  {
    const SigParameter &param = shadRefl.refl.inputSignature[i];

    BuiltinAccess *access = NULL;

    if(param.systemValue == ShaderBuiltin::Position)
      access = &fragCoord;
    else if(param.systemValue == ShaderBuiltin::PrimitiveIndex)
      access = &primitiveID;
    else if(param.systemValue == ShaderBuiltin::MSAASampleIndex)
      access = &sampleIndex;

    if(access)
    {
      SPIRVInterfaceAccess &patch = shadRefl.patchData.inputs[i];
      access->base = patch.ID;
      // should only be one deep at most, built-in interface block isn't allowed to be nested
      RDCASSERT(patch.accessChain.size() <= 1);
      if(!patch.accessChain.empty())
        access->member = patch.accessChain[0];
    }
  }

  // now declare any variables we didn't already have
  if(fragCoord.base == rdcspv::Id())
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    fragCoord.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));

    editor.AddDecoration(rdcspv::OpDecorate(
        fragCoord.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::FragCoord)));

    addedInputs.push_back(fragCoord.base);
  }
  if(primitiveID.base == rdcspv::Id() && usePrimitiveID)
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    primitiveID.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));

    editor.AddDecoration(rdcspv::OpDecorate(
        primitiveID.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::PrimitiveId)));
    editor.AddDecoration(rdcspv::OpDecorate(primitiveID.base, rdcspv::Decoration::Flat));

    addedInputs.push_back(primitiveID.base);

    editor.AddCapability(rdcspv::Capability::Geometry);
  }
  if(sampleIndex.base == rdcspv::Id() && useSampleID)
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    sampleIndex.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));

    editor.AddDecoration(rdcspv::OpDecorate(
        sampleIndex.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::SampleId)));
    editor.AddDecoration(rdcspv::OpDecorate(sampleIndex.base, rdcspv::Decoration::Flat));

    addedInputs.push_back(sampleIndex.base);

    editor.AddCapability(rdcspv::Capability::SampleRateShading);
  }

  // add our inputs to the entry point's ID list. Since we're expanding the list we have to copy,
  // erase, and insert. Modifying in-place doesn't support expanding
  if(!addedInputs.empty())
  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    // this copies into the helper struct
    rdcspv::OpEntryPoint entry(it);

    // add our IDs
    entry.iface.append(addedInputs);

    // erase the old one
    editor.Remove(it);

    editor.AddOperation(it, entry);
  }

  rdcspv::Id PSInput;

  enum Variant
  {
    Variant_Base,
    Variant_ddxcoarse,
    Variant_ddycoarse,
    Variant_ddxfine,
    Variant_ddyfine,
    Variant_Count,
  };

  struct valueAndDerivs
  {
    rdcspv::Id valueType;
    rdcspv::Id data[Variant_Count];
    uint32_t structIndex;
    rdcspv::OperationList storeOps;
  };

  rdcarray<valueAndDerivs> values;
  values.resize(shadRefl.refl.inputSignature.size());

  {
    rdcarray<rdcspv::Id> ids;
    rdcarray<uint32_t> offsets;
    rdcarray<uint32_t> indices;
    for(size_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
    {
      const SigParameter &param = shadRefl.refl.inputSignature[i];

      rdcspv::Scalar base;
      switch(param.compType)
      {
        case CompType::Float: base = rdcspv::scalar<float>(); break;
        case CompType::UInt: base = rdcspv::scalar<uint32_t>(); break;
        case CompType::SInt: base = rdcspv::scalar<int32_t>(); break;
        case CompType::Double: base = rdcspv::scalar<double>(); break;
        default: RDCERR("Unexpected type %s", ToStr(param.compType).c_str());
      }

      values[i].structIndex = (uint32_t)offsets.size();

      offsets.push_back(structStride);
      structStride += param.compCount * (base.width / 8);

      if(param.compCount == 1)
        values[i].valueType = editor.DeclareType(base);
      else
        values[i].valueType = editor.DeclareType(rdcspv::Vector(base, param.compCount));

      ids.push_back(values[i].valueType);

      // align offset conservatively, to 16-byte aligned. We do this with explicit uints so we can
      // preview with spirv-cross (and because it doesn't cost anything particularly)
      uint32_t paddingWords = ((16 - (structStride % 16)) / 4) % 4;
      for(uint32_t p = 0; p < paddingWords; p++)
      {
        ids.push_back(uint32Type);
        offsets.push_back(structStride);
        structStride += 4;
      }
    }

    PSInput = editor.DeclareStructType(ids);

    for(size_t i = 0; i < offsets.size(); i++)
    {
      editor.AddDecoration(rdcspv::OpMemberDecorate(
          PSInput, uint32_t(i), rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offsets[i])));
    }

    for(size_t i = 0; i < values.size(); i++)
      editor.SetMemberName(PSInput, values[i].structIndex, shadRefl.refl.inputSignature[i].varName);

    editor.SetName(PSInput, "__rd_PSInput");
  }

  rdcspv::Id float4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));
  rdcspv::Id float2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 2));

  rdcspv::Id arrayLength =
      editor.AddSpecConstantImmediate<uint32_t>(1U, (uint32_t)InputSpecConstant::ArrayLength);

  editor.SetName(arrayLength, "arrayLength");

  rdcspv::Id destX = editor.AddSpecConstantImmediate<float>(0.0f, (uint32_t)InputSpecConstant::DestX);
  rdcspv::Id destY = editor.AddSpecConstantImmediate<float>(0.0f, (uint32_t)InputSpecConstant::DestY);

  editor.SetName(destX, "destX");
  editor.SetName(destY, "destY");

  rdcspv::Id destXY = editor.AddConstant(
      rdcspv::OpSpecConstantComposite(float2Type, editor.MakeId(), {destX, destY}));

  editor.SetName(destXY, "destXY");

  rdcspv::Id PSHit = editor.DeclareStructType({
      // float4 pos;
      float4Type,
      // uint prim;
      uint32Type,
      // uint sample;
      uint32Type,
      // uint valid;
      uint32Type,
      // uint padding;
      uint32Type,

      // IN
      PSInput,
      // INddxcoarse
      PSInput,
      // INddycoarse
      PSInput,
      // INddxfine
      PSInput,
      // INddxfine
      PSInput,
  });

  {
    editor.SetName(PSHit, "__rd_PSHit");

    uint32_t offs = 0, member = 0;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "pos");
    offs += sizeof(Vec4f);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "prim");
    offs += sizeof(uint32_t);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "sample");
    offs += sizeof(uint32_t);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "valid");
    offs += sizeof(uint32_t);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "padding");
    offs += sizeof(uint32_t);
    member++;

    RDCASSERT((offs % sizeof(Vec4f)) == 0);
    RDCASSERT((structStride % sizeof(Vec4f)) == 0);

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "IN");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddxcoarse");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddycoarse");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddxfine");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddyfine");
    offs += structStride;
    member++;
  }

  rdcspv::Id PSHitRTArray = editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), PSHit));

  editor.AddDecoration(rdcspv::OpDecorate(
      PSHitRTArray, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(structStride * 5 +
                                                                             sizeof(Vec4f) * 2)));

  rdcspv::Id bufBase = editor.DeclareStructType({
      // uint hit_count;
      uint32Type,
      // <uint3 padding>

      //  PSHit hits[];
      PSHitRTArray,
  });

  {
    editor.SetName(bufBase, "__rd_HitStorage");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        bufBase, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));
    editor.SetMemberName(bufBase, 0, "hit_count");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        bufBase, 1, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f))));
    editor.SetMemberName(bufBase, 1, "hits");
  }

  rdcspv::Id bufptrtype;
  rdcspv::Id ssboVar;
  rdcspv::Id addressConstant;

  if(storageMode == Binding)
  {
    // the pointers are SSBO pointers
    bufptrtype = editor.DeclareType(rdcspv::Pointer(bufBase, bufferClass));

    // patch all bindings up by 1
    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                     end = editor.End(rdcspv::Section::Annotations);
        it < end; ++it)
    {
      // we will use descriptor set 0 for our own purposes if we don't have a buffer address.
      //
      // Since bindings are arbitrary, we just increase all user bindings to make room, and we'll
      // redeclare the descriptor set layouts and pipeline layout. This is inevitable in the case
      // where all descriptor sets are already used. In theory we only have to do this with set 0,
      // but that requires knowing which variables are in set 0 and it's simpler to increase all
      // bindings.
      if(it.opcode() == rdcspv::Op::Decorate)
      {
        rdcspv::OpDecorate dec(it);
        if(dec.decoration == rdcspv::Decoration::Binding)
        {
          RDCASSERT(dec.decoration.binding != 0xffffffff);
          dec.decoration.binding += 1;
          it = dec;
        }
      }
    }

    // add our SSBO variable, at set 0 binding 0
    ssboVar = editor.MakeId();
    editor.AddVariable(rdcspv::OpVariable(bufptrtype, ssboVar, bufferClass));
    editor.AddDecoration(
        rdcspv::OpDecorate(ssboVar, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0)));
    editor.AddDecoration(
        rdcspv::OpDecorate(ssboVar, rdcspv::DecorationParam<rdcspv::Decoration::Binding>(0)));

    editor.SetName(ssboVar, "__rd_HitBuffer");

    editor.DecorateStorageBufferStruct(bufBase);
  }
  else
  {
    bufptrtype = editor.DeclareType(rdcspv::Pointer(bufBase, bufferClass));

    // add the extension
    editor.AddExtension(storageMode == KHR_bda ? "SPV_KHR_physical_storage_buffer"
                                               : "SPV_EXT_physical_storage_buffer");

    // change the memory model to physical storage buffer 64
    rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
    rdcspv::OpMemoryModel model(it);
    model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
    it = model;

    // add capabilities
    editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);
    editor.AddCapability(rdcspv::Capability::Int64);

    // declare the address constant which we will specialise later. There is a chicken-and-egg where
    // this function determines how big the buffer needs to be so instead of hardcoding the address
    // here we let it be allocated later and specialised in.
    addressConstant =
        editor.AddSpecConstantImmediate<uint64_t>(0ULL, (uint32_t)InputSpecConstant::Address);

    editor.SetName(addressConstant, "__rd_bufAddress");

    // struct is block decorated
    editor.AddDecoration(rdcspv::OpDecorate(bufBase, rdcspv::Decoration::Block));
  }

  rdcspv::Id float4InPtr =
      editor.DeclareType(rdcspv::Pointer(float4Type, rdcspv::StorageClass::Input));
  rdcspv::Id float4BufPtr = editor.DeclareType(rdcspv::Pointer(float4Type, bufferClass));

  rdcspv::Id uint32InPtr =
      editor.DeclareType(rdcspv::Pointer(uint32Type, rdcspv::StorageClass::Input));
  rdcspv::Id uint32BufPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));

  rdcspv::Id glsl450 = editor.ImportExtInst("GLSL.std.450");

  editor.AddCapability(rdcspv::Capability::DerivativeControl);

  {
    rdcspv::OperationList ops;

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());

    ops.add(rdcspv::OpFunction(voidType, entryID, rdcspv::FunctionControl::None,
                               editor.DeclareType(rdcspv::FunctionType(voidType, {}))));

    ops.add(rdcspv::OpLabel(editor.MakeId()));
    {
      // grab all the values here and get any derivatives we need now before we branch non-uniformly
      for(size_t i = 0; i < values.size(); i++)
      {
        const SPIRVInterfaceAccess &access = shadRefl.patchData.inputs[i];
        const SigParameter &param = shadRefl.refl.inputSignature[i];

        rdcarray<rdcspv::Id> accessIndices;
        for(uint32_t idx : access.accessChain)
          accessIndices.push_back(getUIntConst(idx));

        rdcspv::Id ptrType =
            editor.DeclareType(rdcspv::Pointer(values[i].valueType, rdcspv::StorageClass::Input));
        rdcspv::Id ptr =
            ops.add(rdcspv::OpAccessChain(ptrType, editor.MakeId(), access.ID, accessIndices));
        rdcspv::Id base = ops.add(rdcspv::OpLoad(values[i].valueType, editor.MakeId(), ptr));

        values[i].data[Variant_Base] = base;

        editor.SetName(base, StringFormat::Fmt("__rd_base_%zu_%s", i, param.varName.c_str()));

        // only float values have derivatives
        if(param.compType == CompType::Float)
        {
          values[i].data[Variant_ddxcoarse] =
              ops.add(rdcspv::OpDPdxCoarse(values[i].valueType, editor.MakeId(), base));
          values[i].data[Variant_ddycoarse] =
              ops.add(rdcspv::OpDPdyCoarse(values[i].valueType, editor.MakeId(), base));
          values[i].data[Variant_ddxfine] =
              ops.add(rdcspv::OpDPdxFine(values[i].valueType, editor.MakeId(), base));
          values[i].data[Variant_ddyfine] =
              ops.add(rdcspv::OpDPdyFine(values[i].valueType, editor.MakeId(), base));

          editor.SetName(values[i].data[Variant_ddxcoarse],
                         StringFormat::Fmt("__rd_ddxcoarse_%zu_%s", i, param.varName.c_str()));
          editor.SetName(values[i].data[Variant_ddycoarse],
                         StringFormat::Fmt("__rd_ddycoarse_%zu_%s", i, param.varName.c_str()));
          editor.SetName(values[i].data[Variant_ddxfine],
                         StringFormat::Fmt("__rd_ddxfine_%zu_%s", i, param.varName.c_str()));
          editor.SetName(values[i].data[Variant_ddyfine],
                         StringFormat::Fmt("__rd_ddyfine_%zu_%s", i, param.varName.c_str()));
        }
        else
        {
          values[i].data[Variant_ddxcoarse] = values[i].data[Variant_ddycoarse] =
              values[i].data[Variant_ddxfine] = values[i].data[Variant_ddyfine] =
                  editor.AddConstant(rdcspv::OpConstantNull(values[i].valueType, editor.MakeId()));

          editor.SetName(values[i].data[Variant_ddxcoarse],
                         StringFormat::Fmt("__rd_noderiv_%zu_%s", i, param.varName.c_str()));
        }
      }

      rdcspv::Id structPtr = ssboVar;

      if(structPtr == rdcspv::Id())
      {
        // if we don't have the struct as a bind, we need to cast it from the pointer
        structPtr = ops.add(rdcspv::OpConvertUToPtr(bufptrtype, editor.MakeId(), addressConstant));

        editor.SetName(structPtr, "HitBuffer");
      }

      rdcspv::Id uintPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));

      // get a pointer to buffer.hit_count
      rdcspv::Id hit_count =
          ops.add(rdcspv::OpAccessChain(uintPtr, editor.MakeId(), structPtr, {getUIntConst(0)}));

      rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Device);
      rdcspv::Id semantics =
          editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::MemorySemantics::AcquireRelease);

      // look up the fragcoord
      rdcspv::Id fragCoordLoaded = editor.MakeId();
      if(fragCoord.member == ~0U)
      {
        ops.add(rdcspv::OpLoad(float4Type, fragCoordLoaded, fragCoord.base));
      }
      else
      {
        rdcspv::Id posptr =
            ops.add(rdcspv::OpAccessChain(float4InPtr, editor.MakeId(), fragCoord.base,
                                          {editor.AddConstantImmediate(fragCoord.member)}));
        ops.add(rdcspv::OpLoad(float4Type, fragCoordLoaded, posptr));
      }

      rdcspv::Id bool2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), 2));

      // grab x and y
      rdcspv::Id fragXY = ops.add(rdcspv::OpVectorShuffle(
          float2Type, editor.MakeId(), fragCoordLoaded, fragCoordLoaded, {0, 1}));

      // subtract from the destination co-ord
      rdcspv::Id fragXYRelative =
          ops.add(rdcspv::OpFSub(float2Type, editor.MakeId(), fragXY, destXY));

      // abs()
      rdcspv::Id fragXYAbs = ops.add(rdcspv::OpGLSL450(float2Type, editor.MakeId(), glsl450,
                                                       rdcspv::GLSLstd450::FAbs, {fragXYRelative}));

      rdcspv::Id half = editor.AddConstantImmediate<float>(0.5f);
      rdcspv::Id threshold =
          editor.AddConstant(rdcspv::OpConstantComposite(float2Type, editor.MakeId(), {half, half}));

      // less than 0.5
      rdcspv::Id inPixelXY =
          ops.add(rdcspv::OpFOrdLessThan(bool2Type, editor.MakeId(), fragXYAbs, threshold));

      // both less than 0.5
      rdcspv::Id inPixel = ops.add(rdcspv::OpAll(boolType, editor.MakeId(), inPixelXY));

      // bool inPixel = all(abs(gl_FragCoord.xy - dest.xy) < 0.5f);

      rdcspv::Id killLabel = editor.MakeId();
      rdcspv::Id continueLabel = editor.MakeId();
      ops.add(rdcspv::OpSelectionMerge(killLabel, rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(inPixel, continueLabel, killLabel));
      ops.add(rdcspv::OpLabel(continueLabel));

      // allocate a slot with atomic add
      rdcspv::Id slot = ops.add(rdcspv::OpAtomicIAdd(uint32Type, editor.MakeId(), hit_count, scope,
                                                     semantics, getUIntConst(1)));

      editor.SetName(slot, "slot");

      rdcspv::Id inRange = ops.add(rdcspv::OpULessThan(boolType, editor.MakeId(), slot, arrayLength));

      rdcspv::Id killLabel2 = editor.MakeId();
      continueLabel = editor.MakeId();
      ops.add(rdcspv::OpSelectionMerge(killLabel2, rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(inRange, continueLabel, killLabel2));
      ops.add(rdcspv::OpLabel(continueLabel));

      rdcspv::Id hitptr = editor.DeclareType(rdcspv::Pointer(PSHit, bufferClass));

      // get a pointer to the hit for our slot
      rdcspv::Id hit =
          ops.add(rdcspv::OpAccessChain(hitptr, editor.MakeId(), structPtr, {getUIntConst(1), slot}));

      // store fixed properties

      rdcspv::Id storePtr =
          ops.add(rdcspv::OpAccessChain(float4BufPtr, editor.MakeId(), hit, {getUIntConst(0)}));
      ops.add(rdcspv::OpStore(storePtr, fragCoordLoaded, alignedAccess));

      rdcspv::Id loaded;
      if(primitiveID.base != rdcspv::Id())
      {
        if(primitiveID.member == ~0U)
        {
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), primitiveID.base));
        }
        else
        {
          rdcspv::Id posptr =
              ops.add(rdcspv::OpAccessChain(uint32InPtr, editor.MakeId(), primitiveID.base,
                                            {editor.AddConstantImmediate(primitiveID.member)}));
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), posptr));
        }
      }
      else
      {
        // explicitly store 0
        loaded = getUIntConst(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(1)}));
      ops.add(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      if(sampleIndex.base != rdcspv::Id())
      {
        if(sampleIndex.member == ~0U)
        {
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), sampleIndex.base));
        }
        else
        {
          rdcspv::Id posptr =
              ops.add(rdcspv::OpAccessChain(uint32InPtr, editor.MakeId(), sampleIndex.base,
                                            {editor.AddConstantImmediate(sampleIndex.member)}));
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), posptr));
        }
      }
      else
      {
        // explicitly store 0
        loaded = getUIntConst(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(2)}));
      ops.add(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(3)}));
      ops.add(rdcspv::OpStore(storePtr, editor.AddConstantImmediate(validMagicNumber), alignedAccess));

      // store 0 in the padding
      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(4)}));
      ops.add(rdcspv::OpStore(storePtr, getUIntConst(0), alignedAccess));

      {
        rdcspv::Id inputPtrType = editor.DeclareType(rdcspv::Pointer(PSInput, bufferClass));

        rdcspv::Id outputPtrs[Variant_Count] = {
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(5)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(6)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(7)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(8)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(9)})),
        };

        for(size_t i = 0; i < values.size(); i++)
        {
          rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(values[i].valueType, bufferClass));

          for(size_t j = 0; j < Variant_Count; j++)
          {
            rdcspv::Id ptr = ops.add(rdcspv::OpAccessChain(ptrType, editor.MakeId(), outputPtrs[j],
                                                           {getUIntConst(values[i].structIndex)}));
            ops.add(rdcspv::OpStore(ptr, values[i].data[j], alignedAccess));
          }
        }
      }

      // join up with the early-outs we did
      ops.add(rdcspv::OpBranch(killLabel2));
      ops.add(rdcspv::OpLabel(killLabel2));
      ops.add(rdcspv::OpBranch(killLabel));
      ops.add(rdcspv::OpLabel(killLabel));
    }
    // don't return, kill. This makes it well-defined that we don't write anything to our outputs
    ops.add(rdcspv::OpKill());

    ops.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(ops);
  }
}

ShaderDebugTrace *VulkanReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                            uint32_t idx)
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Vertex debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[0].module];
  rdcstr entryPoint = pipe.shaders[0].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[0].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver, c, VK_SHADER_STAGE_VERTEX_BIT);

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::BaseInstance] = ShaderVariable(rdcstr(), draw->instanceOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::BaseVertex] = ShaderVariable(
      rdcstr(), (draw->flags & DrawFlags::Indexed) ? draw->baseVertex : draw->vertexOffset, 0U, 0U,
      0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);
  builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), vertid, 0U, 0U, 0U);
  builtins[ShaderBuiltin::InstanceIndex] = ShaderVariable(rdcstr(), instid, 0U, 0U, 0U);

  rdcarray<ShaderVariable> &locations = apiWrapper->location_inputs;
  for(const VulkanCreationInfo::Pipeline::Attribute &attr : pipe.vertexAttrs)
  {
    if(attr.location >= locations.size())
      locations.resize(attr.location + 1);

    ShaderValue &val = locations[attr.location].value;

    bytebuf data;

    size_t size = GetByteSize(1, 1, 1, attr.format, 0);

    if(attr.binding < pipe.vertexBindings.size())
    {
      const VulkanCreationInfo::Pipeline::Binding &bind = pipe.vertexBindings[attr.binding];

      if(bind.vbufferBinding < state.vbuffers.size())
      {
        const VulkanRenderState::VertBuffer &vb = state.vbuffers[bind.vbufferBinding];

        uint32_t vertexOffset = 0;

        if(bind.perInstance)
        {
          if(bind.instanceDivisor == 0)
            vertexOffset = draw->instanceOffset * bind.bytestride;
          else
            vertexOffset = draw->instanceOffset + (instid / bind.instanceDivisor) * bind.bytestride;
        }
        else
        {
          vertexOffset = idx * bind.bytestride;
        }

        GetDebugManager()->GetBufferData(vb.buf, vb.offs + attr.byteoffset + vertexOffset, size,
                                         data);
      }
    }

    if(size > data.size())
    {
      // out of bounds read
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Attribute location %u from binding %u reads out of bounds at vertex %u "
              "(index %u) in instance %u.",
              attr.location, attr.binding, vertid, idx, instid));

      if(IsUIntFormat(attr.format) || IsSIntFormat(attr.format))
        val.u = {0, 0, 0, 1};
      else
        val.f = {0.0f, 0.0f, 0.0f, 1.0f};
    }
    else
    {
      FloatVector decoded = ConvertComponents(MakeResourceFormat(attr.format), data.data());

      val.f.x = decoded.x;
      val.f.y = decoded.y;
      val.f.z = decoded.z;
      val.f.w = decoded.w;
    }
  }

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                           uint32_t sample, uint32_t primitive)
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Pixel debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  if(!m_pDriver->GetDeviceFeatures().fragmentStoresAndAtomics)
  {
    RDCWARN("Pixel debugging is not supported without fragment stores");
    return new ShaderDebugTrace;
  }

  VkDevice dev = m_pDriver->GetDev();
  VkResult vkr = VK_SUCCESS;

  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(StringFormat::Fmt("DebugPixel @ %u of (%u,%u) sample %u primitive %u",
                                          eventId, x, y, sample, primitive));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[4].module];
  rdcstr entryPoint = pipe.shaders[4].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[4].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver, c, VK_SHADER_STAGE_FRAGMENT_BIT);

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);

  // If the pipe contains a geometry shader, then Primitive ID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  bool usePrimitiveID = false;
  if(pipe.shaders[3].module != ResourceId())
  {
    VulkanCreationInfo::ShaderModuleReflection &gsRefl =
        c.m_ShaderModule[pipe.shaders[3].module].GetReflection(pipe.shaders[3].entryPoint,
                                                               state.graphics.pipeline);

    // check to see if the shader outputs a primitive ID
    for(const SigParameter &e : gsRefl.refl.outputSignature)
    {
      if(e.systemValue == ShaderBuiltin::PrimitiveIndex)
      {
        usePrimitiveID = true;
        break;
      }
    }
  }
  else
  {
    // no geometry shader - safe to use as long as the geometry shader capability is available
    usePrimitiveID = m_pDriver->GetDeviceFeatures().geometryShader != VK_FALSE;
  }

  bool useSampleID = m_pDriver->GetDeviceFeatures().sampleRateShading != VK_FALSE;

  StorageMode storageMode = Binding;

  if(m_pDriver->GetDeviceFeatures().shaderInt64)
  {
    if(m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address)
      storageMode = KHR_bda;
    else if(m_pDriver->GetExtensions(NULL).ext_EXT_buffer_device_address)
      storageMode = EXT_bda;
  }

  if(Vulkan_Debug_DisableBufferDeviceAddress)
    storageMode = Binding;

  rdcarray<uint32_t> fragspv = shader.spirv.GetSPIRV();

  if(!Vulkan_Debug_PSDebugDumpDirPath.empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath + "/debug_psinput_before.spv", fragspv);

  uint32_t structStride = 0;
  CreatePSInputFetcher(fragspv, structStride, shadRefl, storageMode, usePrimitiveID, useSampleID);

  if(!Vulkan_Debug_PSDebugDumpDirPath.empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath + "/debug_psinput_after.spv", fragspv);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  VkGraphicsPipelineCreateInfo graphicsInfo = {};

  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(graphicsInfo, state.graphics.pipeline);

  // struct size is PSHit header plus 5x structStride = base, ddxcoarse, ddycoarse, ddxfine, ddyfine
  uint32_t structSize = sizeof(PSHit) + structStride * 5;

  VkDeviceSize feedbackStorageSize = overdrawLevels * structSize + sizeof(Vec4f) + 1024;

  if(feedbackStorageSize > m_BindlessFeedback.FeedbackBuffer.sz)
  {
    uint32_t flags = GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO;

    if(storageMode != Binding)
      flags |= GPUBuffer::eGPUBufferAddressable;

    m_BindlessFeedback.FeedbackBuffer.Destroy();
    m_BindlessFeedback.FeedbackBuffer.Create(m_pDriver, dev, feedbackStorageSize, 1, flags);
  }

  struct SpecData
  {
    VkDeviceAddress bufferAddress;
    uint32_t arrayLength;
    float destX;
    float destY;
  } specData;

  specData.arrayLength = overdrawLevels;
  specData.destX = float(x) + 0.5f;
  specData.destY = float(y) + 0.5f;

  VkDescriptorPool descpool = VK_NULL_HANDLE;
  rdcarray<VkDescriptorSetLayout> setLayouts;
  rdcarray<VkDescriptorSet> descSets;

  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;

  if(storageMode != Binding)
  {
    RDCCOMPILE_ASSERT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO ==
                          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT,
                      "KHR and EXT buffer_device_address should be interchangeable here.");
    VkBufferDeviceAddressInfo getAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    getAddressInfo.buffer = m_BindlessFeedback.FeedbackBuffer.buf;

    if(storageMode == KHR_bda)
      specData.bufferAddress = m_pDriver->vkGetBufferDeviceAddress(dev, &getAddressInfo);
    else
      specData.bufferAddress = m_pDriver->vkGetBufferDeviceAddressEXT(dev, &getAddressInfo);
  }
  else
  {
    VkDescriptorSetLayoutBinding newBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT),
         NULL},
    };
    RDCCOMPILE_ASSERT(ARRAY_COUNT(newBindings) == 1,
                      "Should only be one new descriptor for fetching PS inputs");

    // create a duplicate set of descriptor sets, all visible to compute, with bindings shifted to
    // account for new ones we need. This also copies the existing bindings into the new sets
    PatchReservedDescriptors(state.graphics, descpool, setLayouts, descSets,
                             VkShaderStageFlagBits(), newBindings, ARRAY_COUNT(newBindings));

    // create pipeline layout with new descriptor set layouts
    const rdcarray<VkPushConstantRange> &push = c.m_PipelineLayout[pipe.layout].pushRanges;

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        (uint32_t)setLayouts.size(),
        setLayouts.data(),
        (uint32_t)push.size(),
        push.data(),
    };

    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    graphicsInfo.layout = pipeLayout;

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo desc = {0};

    m_BindlessFeedback.FeedbackBuffer.FillDescriptor(desc);

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        NULL,
        Unwrap(descSets[0]),
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        NULL,
        &desc,
        NULL,
    };

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 1, &write, 0, NULL);
  }

  // create fragment shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

  VkSpecializationMapEntry specMaps[] = {
      {
          (uint32_t)InputSpecConstant::Address, offsetof(SpecData, bufferAddress),
          sizeof(SpecData::bufferAddress),
      },
      {
          (uint32_t)InputSpecConstant::ArrayLength, offsetof(SpecData, arrayLength),
          sizeof(SpecData::arrayLength),
      },
      {
          (uint32_t)InputSpecConstant::DestX, offsetof(SpecData, destX), sizeof(SpecData::destX),
      },
      {
          (uint32_t)InputSpecConstant::DestY, offsetof(SpecData, destY), sizeof(SpecData::destY),
      },
  };

  VkShaderModule module = VK_NULL_HANDLE;
  VkSpecializationInfo specInfo = {};
  specInfo.dataSize = sizeof(specData);
  specInfo.pData = &specData;
  specInfo.mapEntryCount = ARRAY_COUNT(specMaps);
  specInfo.pMapEntries = specMaps;

  RDCCOMPILE_ASSERT((size_t)InputSpecConstant::Count == ARRAY_COUNT(specMaps),
                    "Spec constants changed");

  for(uint32_t i = 0; i < graphicsInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &stage =
        (VkPipelineShaderStageCreateInfo &)graphicsInfo.pStages[i];

    if(stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
      moduleCreateInfo.pCode = fragspv.data();
      moduleCreateInfo.codeSize = fragspv.size() * sizeof(uint32_t);

      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      stage.module = module;

      stage.pSpecializationInfo = &specInfo;

      break;
    }
  }

  if(module == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't find fragment shader in pipeline info!");
  }

  VkPipeline inputsPipe;
  vkr =
      m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &graphicsInfo, NULL, &inputsPipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.graphics.pipeline = GetResID(inputsPipe);

  if(storageMode == Binding)
  {
    // Treplace descriptor set IDs with our temporary sets. The offsets we keep the same. If the
    // original draw had no sets, we ensure there's room (with no offsets needed)
    if(modifiedstate.graphics.descSets.empty())
      modifiedstate.graphics.descSets.resize(1);

    for(size_t i = 0; i < descSets.size(); i++)
    {
      modifiedstate.graphics.descSets[i].pipeLayout = GetResID(pipeLayout);
      modifiedstate.graphics.descSets[i].descSet = GetResID(descSets[i]);
    }
  }

  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // fill destination buffer with 0s to ensure a baseline to then feedback against
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(m_BindlessFeedback.FeedbackBuffer.buf), 0,
                                feedbackStorageSize, 0);

    VkBufferMemoryBarrier feedbackbufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_BindlessFeedback.FeedbackBuffer.buf),
        0,
        feedbackStorageSize,
    };

    // wait for the above fill to finish.
    DoPipelineBarrier(cmd, 1, &feedbackbufBarrier);

    modifiedstate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics);

    if(draw->flags & DrawFlags::Indexed)
    {
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), draw->numIndices, draw->numInstances,
                                   draw->indexOffset, draw->baseVertex, draw->instanceOffset);
    }
    else
    {
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), draw->numIndices, draw->numInstances, draw->vertexOffset,
                            draw->instanceOffset);
    }

    modifiedstate.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  bytebuf data;
  GetBufferData(GetResID(m_BindlessFeedback.FeedbackBuffer.buf), 0, 0, data);

  byte *base = data.data();
  uint32_t numHits = *(uint32_t *)base;

  if(numHits > overdrawLevels)
  {
    RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", numHits, overdrawLevels);
    numHits = overdrawLevels;
  }

  base += sizeof(Vec4f);

  PSHit *winner = NULL;

  RDCLOG("Got %u hits", numHits);

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  // figure out the TL pixel's coords. Assume even top left (towards 0,0)
  // this isn't spec'd but is a reasonable assumption.
  int xTL = x & (~1);
  int yTL = y & (~1);

  // get the index of our desired pixel
  int destIdx = (x - xTL) + 2 * (y - yTL);

  VkCompareOp depthOp = pipe.depthCompareOp;

  // depth tests disabled acts the same as always compare mode
  if(!pipe.depthTestEnable)
    depthOp = VK_COMPARE_OP_ALWAYS;

  for(uint32_t i = 0; i < numHits; i++)
  {
    PSHit *hit = (PSHit *)(base + structStride * i);

    if(hit->valid != validMagicNumber)
    {
      RDCWARN("Hit %u doesn't have valid magic number");
      continue;
    }

    // see if this hit is a closer match than the previous winner.

    // if there's no previous winner it's clearly better
    if(winner == NULL)
    {
      winner = hit;
      continue;
    }

    // if we're looking for a specific primitive
    if(primitive != ~0U)
    {
      // and this hit is a match and the winner isn't, it's better
      if(winner->prim != primitive && hit->prim == primitive)
      {
        winner = hit;
        continue;
      }

      // if the winner is a match and we're not, we can't be better so stop now
      if(winner->prim == primitive && hit->prim != primitive)
      {
        continue;
      }
    }

    // if we're looking for a particular sample, check that
    if(sample != ~0U)
    {
      if(winner->sample != sample && hit->sample == sample)
      {
        winner = hit;
        continue;
      }

      if(winner->sample == sample && hit->sample != sample)
      {
        continue;
      }
    }

    // otherwise apply depth test
    switch(depthOp)
    {
      case VK_COMPARE_OP_NEVER:
      case VK_COMPARE_OP_EQUAL:
      case VK_COMPARE_OP_NOT_EQUAL:
      case VK_COMPARE_OP_ALWAYS:
      default:
        // don't emulate equal or not equal since we don't know the reference value. Take any hit
        // (thus meaning the last hit)
        winner = hit;
        break;
      case VK_COMPARE_OP_LESS:
        if(hit->pos.z < winner->pos.z)
          winner = hit;
        break;
      case VK_COMPARE_OP_LESS_OR_EQUAL:
        if(hit->pos.z <= winner->pos.z)
          winner = hit;
        break;
      case VK_COMPARE_OP_GREATER:
        if(hit->pos.z > winner->pos.z)
          winner = hit;
        break;
      case VK_COMPARE_OP_GREATER_OR_EQUAL:
        if(hit->pos.z >= winner->pos.z)
          winner = hit;
        break;
    }
  }

  ShaderDebugTrace *ret = NULL;

  if(winner)
  {
    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shader.spirv.GetSPIRV());

    // the data immediately follows the PSHit header. Every piece of data is vec4 aligned, and the
    // output is in input signature order.
    byte *PSInputs = (byte *)(winner + 1);
    Vec4f *value = (Vec4f *)(PSInputs + 0 * structStride);
    Vec4f *ddxcoarse = (Vec4f *)(PSInputs + 1 * structStride);
    Vec4f *ddycoarse = (Vec4f *)(PSInputs + 2 * structStride);
    Vec4f *ddxfine = (Vec4f *)(PSInputs + 3 * structStride);
    Vec4f *ddyfine = (Vec4f *)(PSInputs + 4 * structStride);

    for(size_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
    {
      const SigParameter &param = shadRefl.refl.inputSignature[i];

      bool builtin = true;
      if(param.systemValue == ShaderBuiltin::Undefined)
      {
        builtin = false;
        apiWrapper->location_inputs.resize(
            RDCMAX((uint32_t)apiWrapper->location_inputs.size(), param.regIndex + 1));
        apiWrapper->location_derivatives.resize(
            RDCMAX((uint32_t)apiWrapper->location_derivatives.size(), param.regIndex + 1));
      }

      ShaderVariable &var = builtin ? apiWrapper->builtin_inputs[param.systemValue]
                                    : apiWrapper->location_inputs[param.regIndex];
      rdcspv::DebugAPIWrapper::DerivativeDeltas &deriv =
          builtin ? apiWrapper->builtin_derivatives[param.systemValue]
                  : apiWrapper->location_derivatives[param.regIndex];

      memcpy(&var.value.uv, &value[i], sizeof(Vec4f));
      memcpy(&deriv.ddxcoarse, &ddxcoarse[i], sizeof(Vec4f));
      memcpy(&deriv.ddycoarse, &ddycoarse[i], sizeof(Vec4f));
      memcpy(&deriv.ddxfine, &ddxfine[i], sizeof(Vec4f));
      memcpy(&deriv.ddyfine, &ddyfine[i], sizeof(Vec4f));
    }

    ret = debugger->BeginDebug(apiWrapper, ShaderStage::Pixel, entryPoint, spec,
                               shadRefl.instructionLines, shadRefl.patchData, destIdx);
  }
  else
  {
    RDCLOG("Didn't get any valid hit to debug");
    delete apiWrapper;

    ret = new ShaderDebugTrace;
  }

  if(descpool != VK_NULL_HANDLE)
  {
    // delete descriptors. Technically we don't have to free the descriptor sets, but our tracking
    // on replay doesn't handle destroying children of pooled objects so we do it explicitly anyway.
    m_pDriver->vkFreeDescriptorSets(dev, descpool, (uint32_t)descSets.size(), descSets.data());

    m_pDriver->vkDestroyDescriptorPool(dev, descpool, NULL);
  }

  for(VkDescriptorSetLayout layout : setLayouts)
    m_pDriver->vkDestroyDescriptorSetLayout(dev, layout, NULL);

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, inputsPipe, NULL);

  // delete shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                            const uint32_t threadid[3])
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Compute debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(StringFormat::Fmt("DebugThread @ %u of (%u,%u,%u) (%u,%u,%u)", eventId,
                                          groupid[0], groupid[1], groupid[2], threadid[0],
                                          threadid[1], threadid[2]));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Dispatch))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.compute.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[5].module];
  rdcstr entryPoint = pipe.shaders[5].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[5].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.compute.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver, c, VK_SHADER_STAGE_COMPUTE_BIT);

  uint32_t threadDim[3];
  threadDim[0] = shadRefl.refl.dispatchThreadsDimension[0];
  threadDim[1] = shadRefl.refl.dispatchThreadsDimension[1];
  threadDim[2] = shadRefl.refl.dispatchThreadsDimension[2];

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DispatchSize] =
      ShaderVariable(rdcstr(), draw->dispatchDimension[0], draw->dispatchDimension[1],
                     draw->dispatchDimension[2], 0U);
  builtins[ShaderBuiltin::DispatchThreadIndex] = ShaderVariable(
      rdcstr(), groupid[0] * threadDim[0] + threadid[0], groupid[1] * threadDim[1] + threadid[1],
      groupid[2] * threadDim[2] + threadid[2], 0U);
  builtins[ShaderBuiltin::GroupIndex] =
      ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);
  builtins[ShaderBuiltin::GroupSize] =
      ShaderVariable(rdcstr(), threadDim[0], threadDim[1], threadDim[2], 0U);
  builtins[ShaderBuiltin::GroupThreadIndex] =
      ShaderVariable(rdcstr(), threadid[0], threadid[1], threadid[2], 0U);
  builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
      rdcstr(), threadid[2] * threadDim[0] * threadDim[1] + threadid[1] * threadDim[0] + threadid[0],
      0U, 0U, 0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Compute, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

rdcarray<ShaderDebugState> VulkanReplay::ContinueDebug(ShaderDebugger *debugger)
{
  rdcspv::Debugger *spvDebugger = (rdcspv::Debugger *)debugger;

  if(!spvDebugger)
    return {};

  VkMarkerRegion region("ContinueDebug Simulation Loop");

  return spvDebugger->ContinueDebug();
}
