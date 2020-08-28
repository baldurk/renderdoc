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
RDOC_DEBUG_CONFIG(bool, Vulkan_Debug_ShaderDebugLogging, false,
                  "Output verbose debug logging messages when debugging shaders.");

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
  Tex2D = 2,
  Tex3D = 3,
  Tex2DMS = 4,
  TexCube = 5,
  Buffer = 6,
  Sampler = 7,
  Constants = 8,
  Count,
};

struct Vec3i
{
  int32_t x, y, z;
};

struct GatherOffsets
{
  int32_t u0, v0, u1, v1, u2, v2, u3, v3;
};

struct ShaderConstParameters
{
  uint32_t operation;
  VkBool32 useGradOrGatherOffsets;
  ShaderDebugBind dim;
  rdcspv::GatherChannel gatherChannel;
  GatherOffsets gatherOffsets;

  uint32_t hashKey(uint32_t shaderIndex) const
  {
    uint32_t hash = 5381;
    hash = ((hash << 5) + hash) + shaderIndex;
    hash = ((hash << 5) + hash) + operation;
    hash = ((hash << 5) + hash) + useGradOrGatherOffsets;
    hash = ((hash << 5) + hash) + (uint32_t)dim;
    hash = ((hash << 5) + hash) + (uint32_t)gatherChannel;
    hash = ((hash << 5) + hash) + gatherOffsets.u0;
    hash = ((hash << 5) + hash) + gatherOffsets.v0;
    hash = ((hash << 5) + hash) + gatherOffsets.u1;
    hash = ((hash << 5) + hash) + gatherOffsets.v1;
    hash = ((hash << 5) + hash) + gatherOffsets.u2;
    hash = ((hash << 5) + hash) + gatherOffsets.v2;
    hash = ((hash << 5) + hash) + gatherOffsets.u3;
    hash = ((hash << 5) + hash) + gatherOffsets.v3;
    return hash;
  }
};

struct ShaderUniformParameters
{
  Vec3i texel_uvw;
  int texel_lod;
  Vec4f uvwa;
  Vec3f ddx;
  Vec3f ddy;
  Vec3i offset;
  int sampleIdx;
  float compare;
  float lod;
  float minlod;
};

class VulkanAPIWrapper : public rdcspv::DebugAPIWrapper
{
  rdcarray<DescSetSnapshot> m_DescSets;

public:
  VulkanAPIWrapper(WrappedVulkan *vk, VulkanCreationInfo &creation, VkShaderStageFlagBits stage,
                   uint32_t eid)
      : m_DebugData(vk->GetReplay()->GetShaderDebugData()), m_Creation(creation), m_EventID(eid)
  {
    m_pDriver = vk;

    // when we're first setting up, the state is pristine and no replay is needed
    m_ResourcesDirty = false;

    const VulkanRenderState &state = m_pDriver->GetRenderState();

    const bool compute = (stage == VK_SHADER_STAGE_COMPUTE_BIT);

    // snapshot descriptor set contents
    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descSets =
        compute ? state.compute.descSets : state.graphics.descSets;

    const VulkanCreationInfo::Pipeline &pipe =
        m_Creation.m_Pipeline[compute ? state.compute.pipeline : state.graphics.pipeline];
    const VulkanCreationInfo::PipelineLayout &pipeLayout = m_Creation.m_PipelineLayout[pipe.layout];

    for(const VkPushConstantRange &range : pipeLayout.pushRanges)
    {
      if(range.stageFlags & stage)
      {
        pushData.resize(RDCMAX((uint32_t)pushData.size(), range.offset + range.size));

        RDCASSERT(range.offset + range.size < sizeof(state.pushconsts));

        memcpy(pushData.data() + range.offset, state.pushconsts + range.offset, range.size);
      }
    }

    m_DescSets.resize(RDCMIN(descSets.size(), pipeLayout.descSetLayouts.size()));
    for(size_t set = 0; set < m_DescSets.size(); set++)
    {
      uint32_t dynamicOffset = 0;

      // skip invalid descriptor set binds, we assume these aren't present because they will not be
      // accessed statically
      if(descSets[set].descSet == ResourceId() || descSets[set].pipeLayout == ResourceId())
        continue;

      DescSetSnapshot &dstSet = m_DescSets[set];

      const BindingStorage &bindStorage =
          m_pDriver->GetCurrentDescSetBindingStorage(descSets[set].descSet);
      const rdcarray<DescriptorSetSlot *> &curBinds = bindStorage.binds;
      const bytebuf &curInline = bindStorage.inlineBytes;

      // use the descriptor set layout from when it was bound. If the pipeline layout declared a
      // descriptor set layout for this set, but it's statically unused, it may be complete
      // garbage and doesn't match what the shader uses. However the pipeline layout at descriptor
      // set bind time must have been compatible and valid so we can use it. If this set *is* used
      // then the pipeline layout at bind time must be compatible with the pipeline's pipeline
      // layout, so we're fine too.
      const DescSetLayout &setLayout =
          m_Creation
              .m_DescSetLayout[m_Creation.m_PipelineLayout[descSets[set].pipeLayout].descSetLayouts[set]];

      for(size_t bind = 0; bind < setLayout.bindings.size(); bind++)
      {
        const DescSetLayout::Binding &bindLayout = setLayout.bindings[bind];

        uint32_t descriptorCount = bindLayout.descriptorCount;

        if(bindLayout.variableSize)
          descriptorCount = bindStorage.variableDescriptorCount;

        if(descriptorCount == 0)
          continue;

        if(bindLayout.stageFlags & stage)
        {
          DescriptorSetSlot *curSlots = curBinds[bind];

          dstSet.bindings.resize_for_index(bind);

          DescSetBindingSnapshot &dstBind = dstSet.bindings[bind];

          switch(bindLayout.descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
              dstBind.imageInfos.resize(descriptorCount);
              for(uint32_t i = 0; i < descriptorCount; i++)
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
              dstBind.texelBuffers.resize(descriptorCount);
              for(uint32_t i = 0; i < descriptorCount; i++)
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
              dstBind.buffers.resize(descriptorCount);
              for(uint32_t i = 0; i < descriptorCount; i++)
              {
                dstBind.buffers[i].offset = curSlots[i].bufferInfo.offset;
                dstBind.buffers[i].range = curSlots[i].bufferInfo.range;
                dstBind.buffers[i].buffer =
                    m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(
                        curSlots[i].bufferInfo.buffer);
              }
              break;
            }
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
            {
              // push directly into the buffer cache from the inline data
              BindpointIndex idx;
              idx.bindset = (int32_t)set;
              idx.bind = (int32_t)bind;
              idx.arrayIndex = 0;
              bufferCache[idx].assign(curInline.data() + curSlots->inlineOffset, descriptorCount);
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
              dstBind.buffers.resize(descriptorCount);
              for(uint32_t i = 0; i < descriptorCount; i++)
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
        else
        {
          // still need to skip past dynamic offsets for stages that aren't of interest

          switch(bindLayout.descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: dynamicOffset += descriptorCount; break;
            default: break;
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

  void ResetReplay()
  {
    if(!m_ResourcesDirty)
    {
      VkMarkerRegion region("ResetReplay");
      // replay the draw to get back to 'normal' state for this event, and mark that we need to
      // replay back to pristine state next time we need to fetch data.
      m_pDriver->ReplayLog(0, m_EventID, eReplay_OnlyDraw);
    }
    m_ResourcesDirty = true;
  }

  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                               rdcstr d) override
  {
    m_pDriver->AddDebugMessage(c, sv, src, d);
  }

  virtual uint64_t GetBufferLength(BindpointIndex bind) override
  {
    return PopulateBuffer(bind).size();
  }

  virtual void ReadBufferValue(BindpointIndex bind, uint64_t offset, uint64_t byteSize,
                               void *dst) override
  {
    const bytebuf &data = PopulateBuffer(bind);

    if(offset + byteSize <= data.size())
      memcpy(dst, data.data() + (size_t)offset, (size_t)byteSize);
  }

  virtual void WriteBufferValue(BindpointIndex bind, uint64_t offset, uint64_t byteSize,
                                const void *src) override
  {
    bytebuf &data = PopulateBuffer(bind);

    if(offset + byteSize <= data.size())
      memcpy(data.data() + (size_t)offset, src, (size_t)byteSize);
  }

  virtual bool ReadTexel(BindpointIndex imageBind, const ShaderVariable &coord, uint32_t sample,
                         ShaderVariable &output) override
  {
    ImageData &data = PopulateImage(imageBind);

    if(data.width == 0)
      return false;

    if(coord.value.uv[0] > data.width || coord.value.uv[1] > data.height ||
       coord.value.uv[2] > data.depth)
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Out of bounds access to image, coord %u,%u,%u outside of dimensions %ux%ux%u",
              coord.value.uv[0], coord.value.uv[1], coord.value.uv[2], data.width, data.height,
              data.depth));
      return false;
    }

    memcpy(output.value.uv, data.texel(coord.value.uv, sample), data.texelSize);

    return true;
  }

  virtual bool WriteTexel(BindpointIndex imageBind, const ShaderVariable &coord, uint32_t sample,
                          const ShaderVariable &value) override
  {
    ImageData &data = PopulateImage(imageBind);

    if(data.width == 0)
      return false;

    if(coord.value.uv[0] > data.width || coord.value.uv[1] > data.height ||
       coord.value.uv[2] > data.depth)
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Out of bounds access to image, coord %u,%u,%u outside of dimensions %ux%ux%u",
              coord.value.uv[0], coord.value.uv[1], coord.value.uv[2], data.width, data.height,
              data.depth));
      return false;
    }

    memcpy(data.texel(coord.value.uv, sample), value.value.uv, data.texelSize);

    return true;
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

    if(location < location_inputs.size())
    {
      const uint32_t typeSize = VarTypeByteSize(var.type);
      if(var.rows == 1)
      {
        if(component > 3)
          RDCERR("Unexpected component %u ", component);

        if(typeSize == 8)
          memcpy(var.value.u64v, &location_inputs[location].value.u64v[component],
                 var.rows * var.columns * typeSize);
        else
          memcpy(var.value.uv, &location_inputs[location].value.uv[component],
                 var.rows * var.columns * typeSize);
      }
      else
      {
        for(uint8_t r = 0; r < var.rows; r++)
        {
          for(uint8_t c = 0; c < var.columns; c++)
          {
            if(typeSize == 8)
              var.value.u64v[r * var.columns + c] = location_inputs[location + c].value.u64v[r];
            else
              var.value.uv[r * var.columns + c] = location_inputs[location + c].value.uv[r];
          }
        }
      }
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

    if(location < location_derivatives.size())
    {
      const DerivativeDeltas &deriv = location_derivatives[location];

      DerivativeDeltas ret;

      RDCASSERT(component < 4, component);

      // rebase from component into [0]..

      for(uint32_t src = component, dst = 0; src < 4; src++, dst++)
      {
        ret.ddxcoarse.fv[dst] = deriv.ddxcoarse.fv[src];
        ret.ddxfine.fv[dst] = deriv.ddxfine.fv[src];
        ret.ddycoarse.fv[dst] = deriv.ddycoarse.fv[src];
        ret.ddyfine.fv[dst] = deriv.ddyfine.fv[src];
      }

      return ret;
    }

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
    ShaderConstParameters constParams = {};
    ShaderUniformParameters uniformParams = {};

    const bool buffer = (texType & DebugAPIWrapper::Buffer_Texture) != 0;
    const bool uintTex = (texType & DebugAPIWrapper::UInt_Texture) != 0;
    const bool sintTex = (texType & DebugAPIWrapper::SInt_Texture) != 0;

    // fetch the right type of descriptor depending on if we're buffer or not
    bool valid = true;
    rdcstr access = StringFormat::Fmt("performing %s operation", ToStr(opcode).c_str());
    const VkDescriptorImageInfo &imageInfo =
        buffer ? GetDescriptor<VkDescriptorImageInfo>(access, invalidBind, valid)
               : GetDescriptor<VkDescriptorImageInfo>(access, imageBind, valid);
    const VkBufferView &bufferView = buffer
                                         ? GetDescriptor<VkBufferView>(access, imageBind, valid)
                                         : GetDescriptor<VkBufferView>(access, invalidBind, valid);

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

    VkDevice dev = m_pDriver->GetDev();

    // how many co-ordinates should there be
    int coords = 0, gradCoords = 0;
    if(buffer)
    {
      constParams.dim = ShaderDebugBind::Buffer;
      coords = gradCoords = 1;
    }
    else
    {
      switch(viewProps.viewType)
      {
        case VK_IMAGE_VIEW_TYPE_1D:
          coords = 1;
          gradCoords = 1;
          constParams.dim = ShaderDebugBind::Tex1D;
          break;
        case VK_IMAGE_VIEW_TYPE_2D:
          coords = 2;
          gradCoords = 2;
          constParams.dim = ShaderDebugBind::Tex2D;
          break;
        case VK_IMAGE_VIEW_TYPE_3D:
          coords = 3;
          gradCoords = 3;
          constParams.dim = ShaderDebugBind::Tex3D;
          break;
        case VK_IMAGE_VIEW_TYPE_CUBE:
          coords = 3;
          gradCoords = 3;
          constParams.dim = ShaderDebugBind::TexCube;
          break;
        case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
          coords = 2;
          gradCoords = 1;
          constParams.dim = ShaderDebugBind::Tex1D;
          break;
        case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
          coords = 3;
          gradCoords = 2;
          constParams.dim = ShaderDebugBind::Tex2D;
          break;
        case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
          coords = 4;
          gradCoords = 3;
          constParams.dim = ShaderDebugBind::TexCube;
          break;
        case VK_IMAGE_VIEW_TYPE_MAX_ENUM:
          RDCERR("Invalid image view type %s", ToStr(viewProps.viewType).c_str());
          return false;
      }

      if(imageProps.samples > 1)
        constParams.dim = ShaderDebugBind::Tex2DMS;
    }

    // handle query opcodes now
    switch(opcode)
    {
      case rdcspv::Op::ImageQueryLevels:
      {
        output.value.u.x = viewProps.range.levelCount;
        if(viewProps.range.levelCount == VK_REMAINING_MIP_LEVELS)
          output.value.u.x = imageProps.mipLevels - viewProps.range.baseMipLevel;
        return true;
      }
      case rdcspv::Op::ImageQuerySamples:
      {
        output.value.u.x = (uint32_t)imageProps.samples;
        return true;
      }
      case rdcspv::Op::ImageQuerySize:
      case rdcspv::Op::ImageQuerySizeLod:
      {
        uint32_t mip = viewProps.range.baseMipLevel;

        if(opcode == rdcspv::Op::ImageQuerySizeLod)
          mip += lane.GetSrc(operands.lod).value.u.x;

        int i = 0;
        output.value.uv[i++] = RDCMAX(1U, imageProps.extent.width >> mip);
        if(coords >= 2)
          output.value.uv[i++] = RDCMAX(1U, imageProps.extent.height >> mip);
        if(viewProps.viewType == VK_IMAGE_VIEW_TYPE_3D)
          output.value.uv[i++] = RDCMAX(1U, imageProps.extent.depth >> mip);

        if(viewProps.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY ||
           viewProps.viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
          output.value.uv[i++] = imageProps.arrayLayers;
        else if(viewProps.viewType == VK_IMAGE_VIEW_TYPE_CUBE ||
                viewProps.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
          output.value.uv[i++] = imageProps.arrayLayers / 6;

        if(buffer)
        {
          const VulkanCreationInfo::BufferView &bufViewProps =
              m_Creation.m_BufferView[GetResID(bufferView)];

          VkDeviceSize size = bufViewProps.size;

          if(size == VK_WHOLE_SIZE)
          {
            const VulkanCreationInfo::Buffer &bufProps = m_Creation.m_Buffer[bufViewProps.buffer];
            size = bufProps.size - bufViewProps.offset;
          }

          output.value.uv[0] = uint32_t(size / GetByteSize(1, 1, 1, bufViewProps.format, 0));
          output.value.uv[1] = output.value.uv[2] = output.value.uv[3] = 0;
        }

        return true;
      }
      default: break;
    }

    // create our own view (if we haven't already for this view) so we can promote to array
    VkImageView sampleView = m_SampleViews[GetResID(view)];
    if(sampleView == VK_NULL_HANDLE && view != VK_NULL_HANDLE)
    {
      VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      viewInfo.image = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(viewProps.image);
      viewInfo.format = viewProps.format;
      viewInfo.viewType = viewProps.viewType;
      if(viewInfo.viewType == VK_IMAGE_VIEW_TYPE_1D)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
      else if(viewInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      else if(viewInfo.viewType == VK_IMAGE_VIEW_TYPE_CUBE &&
              m_pDriver->GetDeviceEnabledFeatures().imageCubeArray)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

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
          sampInfo.anisotropyEnable = samplerProps.maxAnisotropy >= 1.0f;
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

          VkSamplerCustomBorderColorCreateInfoEXT borderInfo = {
              VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT};
          if(samplerProps.customBorder)
          {
            borderInfo.customBorderColor = samplerProps.customBorderColor;
            borderInfo.format = samplerProps.customBorderFormat;

            borderInfo.pNext = sampInfo.pNext;
            sampInfo.pNext = &borderInfo;
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

    constParams.operation = (uint32_t)opcode;

    // proj opcodes have an extra q parameter, but we do the divide ourselves and 'demote' these to
    // non-proj variants
    bool proj = false;
    switch(opcode)
    {
      case rdcspv::Op::ImageSampleProjExplicitLod:
      {
        constParams.operation = (uint32_t)rdcspv::Op::ImageSampleExplicitLod;
        proj = true;
        break;
      }
      case rdcspv::Op::ImageSampleProjImplicitLod:
      {
        constParams.operation = (uint32_t)rdcspv::Op::ImageSampleImplicitLod;
        proj = true;
        break;
      }
      case rdcspv::Op::ImageSampleProjDrefExplicitLod:
      {
        constParams.operation = (uint32_t)rdcspv::Op::ImageSampleDrefExplicitLod;
        proj = true;
        break;
      }
      case rdcspv::Op::ImageSampleProjDrefImplicitLod:
      {
        constParams.operation = (uint32_t)rdcspv::Op::ImageSampleDrefImplicitLod;
        proj = true;
        break;
      }
      default: break;
    }

    bool useCompare = false;
    switch(opcode)
    {
      case rdcspv::Op::ImageDrefGather:
      case rdcspv::Op::ImageSampleDrefExplicitLod:
      case rdcspv::Op::ImageSampleDrefImplicitLod:
      case rdcspv::Op::ImageSampleProjDrefExplicitLod:
      case rdcspv::Op::ImageSampleProjDrefImplicitLod:
      {
        useCompare = true;

        if(m_pDriver->GetDriverInfo().QualcommDrefNon2DCompileCrash() &&
           constParams.dim != ShaderDebugBind::Tex2D)
        {
          m_pDriver->AddDebugMessage(
              MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
              "Dref sample against non-2D texture, this cannot be debugged due to a driver bug");
        }

        break;
      }
      default: break;
    }

    switch(opcode)
    {
      case rdcspv::Op::ImageFetch:
      {
        // co-ordinates after the used ones are read as 0s. This allows us to then read an implicit
        // 0 for array layer when we promote accesses to arrays.
        uniformParams.texel_uvw.x = uv.value.u.x;
        if(coords >= 2)
          uniformParams.texel_uvw.y = uv.value.u.y;
        if(coords >= 3)
          uniformParams.texel_uvw.z = uv.value.u.z;

        if(!buffer && operands.flags & rdcspv::ImageOperands::Lod)
          uniformParams.texel_lod = lane.GetSrc(operands.lod).value.i.x;
        else
          uniformParams.texel_lod = 0;

        if(operands.flags & rdcspv::ImageOperands::Sample)
          uniformParams.sampleIdx = lane.GetSrc(operands.sample).value.u.x;

        break;
      }
      case rdcspv::Op::ImageGather:
      case rdcspv::Op::ImageDrefGather:
      {
        uniformParams.uvwa.x = uv.value.f.x;
        if(coords >= 2)
          uniformParams.uvwa.y = uv.value.f.y;
        if(coords >= 3)
          uniformParams.uvwa.z = uv.value.f.z;
        if(coords >= 4)
          uniformParams.uvwa.z = uv.value.f.w;

        if(useCompare)
          uniformParams.compare = compare.value.f.x;

        constParams.gatherChannel = gatherChannel;

        if(operands.flags & rdcspv::ImageOperands::ConstOffsets)
        {
          ShaderVariable constOffsets = lane.GetSrc(operands.constOffsets);

          constParams.useGradOrGatherOffsets = VK_TRUE;

          // should be an array of ivec2
          RDCASSERT(constOffsets.members.size() == 4);

          constParams.gatherOffsets.u0 = constOffsets.members[0].value.i.x;
          constParams.gatherOffsets.v0 = constOffsets.members[0].value.i.y;
          constParams.gatherOffsets.u1 = constOffsets.members[1].value.i.x;
          constParams.gatherOffsets.v1 = constOffsets.members[1].value.i.y;
          constParams.gatherOffsets.u2 = constOffsets.members[1].value.i.x;
          constParams.gatherOffsets.v2 = constOffsets.members[1].value.i.y;
          constParams.gatherOffsets.u3 = constOffsets.members[1].value.i.x;
          constParams.gatherOffsets.v3 = constOffsets.members[1].value.i.y;
        }

        if(operands.flags & rdcspv::ImageOperands::ConstOffset)
        {
          ShaderVariable constOffset = lane.GetSrc(operands.constOffset);
          uniformParams.offset.x = constOffset.value.i.x;
          if(gradCoords >= 2)
            uniformParams.offset.y = constOffset.value.i.y;
          if(gradCoords >= 3)
            uniformParams.offset.z = constOffset.value.i.z;
        }
        else if(operands.flags & rdcspv::ImageOperands::Offset)
        {
          ShaderVariable offset = lane.GetSrc(operands.offset);
          uniformParams.offset.x = offset.value.i.x;
          if(gradCoords >= 2)
            uniformParams.offset.y = offset.value.i.y;
          if(gradCoords >= 3)
            uniformParams.offset.z = offset.value.i.z;
        }

        break;
      }
      case rdcspv::Op::ImageQueryLod:
      case rdcspv::Op::ImageSampleExplicitLod:
      case rdcspv::Op::ImageSampleImplicitLod:
      case rdcspv::Op::ImageSampleProjExplicitLod:
      case rdcspv::Op::ImageSampleProjImplicitLod:
      case rdcspv::Op::ImageSampleDrefExplicitLod:
      case rdcspv::Op::ImageSampleDrefImplicitLod:
      case rdcspv::Op::ImageSampleProjDrefExplicitLod:
      case rdcspv::Op::ImageSampleProjDrefImplicitLod:
      {
        uniformParams.uvwa.x = uv.value.f.x;
        if(coords >= 2)
          uniformParams.uvwa.y = uv.value.f.y;
        if(coords >= 3)
          uniformParams.uvwa.z = uv.value.f.z;
        if(coords >= 4)
          uniformParams.uvwa.z = uv.value.f.w;

        if(proj)
        {
          // coords shouldn't be 4 because that's only valid for cube arrays which can't be
          // projected
          RDCASSERT(coords < 4);

          // do the divide ourselves rather than severely complicating the sample shader (as proj
          // variants need non-arrayed textures)
          float q = uv.value.fv[coords];
          uniformParams.uvwa.x /= q;
          uniformParams.uvwa.y /= q;
          uniformParams.uvwa.z /= q;
        }

        if(operands.flags & rdcspv::ImageOperands::MinLod)
          uniformParams.minlod = lane.GetSrc(operands.minLod).value.f.x;

        if(useCompare)
          uniformParams.compare = compare.value.f.x;

        if(operands.flags & rdcspv::ImageOperands::Lod)
        {
          uniformParams.lod = lane.GetSrc(operands.lod).value.f.x;
          constParams.useGradOrGatherOffsets = VK_FALSE;
        }
        else if(operands.flags & rdcspv::ImageOperands::Grad)
        {
          ShaderVariable ddx = lane.GetSrc(operands.grad.first);
          ShaderVariable ddy = lane.GetSrc(operands.grad.second);

          constParams.useGradOrGatherOffsets = VK_TRUE;

          uniformParams.ddx.x = ddx.value.f.x;
          if(gradCoords >= 2)
            uniformParams.ddx.y = ddx.value.f.y;
          if(gradCoords >= 3)
            uniformParams.ddx.z = ddx.value.f.z;

          uniformParams.ddy.x = ddy.value.f.x;
          if(gradCoords >= 2)
            uniformParams.ddy.y = ddy.value.f.y;
          if(gradCoords >= 3)
            uniformParams.ddy.z = ddy.value.f.z;
        }

        if(opcode == rdcspv::Op::ImageSampleImplicitLod ||
           opcode == rdcspv::Op::ImageSampleProjImplicitLod || opcode == rdcspv::Op::ImageQueryLod)
        {
          // use grad to sub in for the implicit lod
          constParams.useGradOrGatherOffsets = VK_TRUE;

          uniformParams.ddx.x = ddxCalc.value.f.x;
          if(gradCoords >= 2)
            uniformParams.ddx.y = ddxCalc.value.f.y;
          if(gradCoords >= 3)
            uniformParams.ddx.z = ddxCalc.value.f.z;

          uniformParams.ddy.x = ddyCalc.value.f.x;
          if(gradCoords >= 2)
            uniformParams.ddy.y = ddyCalc.value.f.y;
          if(gradCoords >= 3)
            uniformParams.ddy.z = ddyCalc.value.f.z;
        }

        if(operands.flags & rdcspv::ImageOperands::ConstOffset)
        {
          ShaderVariable constOffset = lane.GetSrc(operands.constOffset);
          uniformParams.offset.x = constOffset.value.i.x;
          if(gradCoords >= 2)
            uniformParams.offset.y = constOffset.value.i.y;
          if(gradCoords >= 3)
            uniformParams.offset.z = constOffset.value.i.z;
        }
        else if(operands.flags & rdcspv::ImageOperands::Offset)
        {
          ShaderVariable offset = lane.GetSrc(operands.offset);
          uniformParams.offset.x = offset.value.i.x;
          if(gradCoords >= 2)
            uniformParams.offset.y = offset.value.i.y;
          if(gradCoords >= 3)
            uniformParams.offset.z = offset.value.i.z;
        }

        break;
      }
      default:
      {
        RDCERR("Unsupported opcode %s", ToStr(opcode).c_str());
        return false;
      }
    }

    // we don't support constant offsets, they're always promoted to dynamic offsets to avoid
    // needing to potentially compile lots of pipelines with different offsets. If we're actually
    // using them and the device doesn't support the extended gather feature, the result will be
    // wrong.
    if(!m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
       (uniformParams.offset.x != 0 || uniformParams.offset.y != 0 || uniformParams.offset.z != 0))
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Use of constant offsets %d/%d/%d is not supported without "
                            "shaderImageGatherExtended device feature",
                            uniformParams.offset.x, uniformParams.offset.y, uniformParams.offset.z));
    }

    VkPipeline pipe = MakePipe(constParams, uintTex, sintTex);

    if(pipe == VK_NULL_HANDLE)
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 "Failed to compile graphics pipeline for sampling operation");
      return false;
    }

    VkDescriptorImageInfo samplerWriteInfo = {Unwrap(sampler), VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_UNDEFINED};
    VkDescriptorImageInfo imageWriteInfo = {VK_NULL_HANDLE, Unwrap(sampleView), layout};

    VkDescriptorBufferInfo uniformWriteInfo = {};
    m_DebugData.ConstantsBuffer.FillDescriptor(uniformWriteInfo);

    VkWriteDescriptorSet writeSets[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_DebugData.DescSet),
            (uint32_t)ShaderDebugBind::Constants, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL,
            &uniformWriteInfo, NULL,
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_DebugData.DescSet),
            (uint32_t)constParams.dim, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageWriteInfo,
            NULL, NULL,
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_DebugData.DescSet),
            (uint32_t)ShaderDebugBind::Sampler, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, &samplerWriteInfo,
            NULL, NULL,
        },
    };

    if(buffer)
    {
      writeSets[1].pTexelBufferView = UnwrapPtr(bufferView);
      writeSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    }

    // reset descriptor sets to dummy state
    uint32_t resetIndex = 0;
    if(uintTex)
      resetIndex = 1;
    else if(sintTex)
      resetIndex = 2;
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(m_DebugData.DummyWrites[resetIndex]),
                                       m_DebugData.DummyWrites[resetIndex], 0, NULL);

    // overwrite with our data
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), sampler != VK_NULL_HANDLE ? 3 : 2, writeSets, 0,
                                       NULL);

    void *constants = m_DebugData.ConstantsBuffer.Map(NULL, 0);

    memcpy(constants, &uniformParams, sizeof(uniformParams));

    m_DebugData.ConstantsBuffer.Unmap();

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

      // push uvw/ddx/ddy for the vertex shader
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                     sizeof(Vec4f) * 0, sizeof(Vec4f), &uniformParams.uvwa);
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                     sizeof(Vec4f) * 1, sizeof(Vec3f), &uniformParams.ddx);
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                     sizeof(Vec4f) * 2, sizeof(Vec3f), &uniformParams.ddy);

      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 3, 1, 0, 0);

      ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));

      VkBufferImageCopy region = {
          0, sizeof(Vec4f), 1, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {1, 1, 1},
      };
      ObjDisp(cmd)->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(m_DebugData.Image),
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         Unwrap(m_DebugData.ReadbackBuffer.buf), 1, &region);

      VkBufferMemoryBarrier bufBarrier = {
          VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_HOST_READ_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(m_DebugData.ReadbackBuffer.buf),
          0,
          VK_WHOLE_SIZE,
      };

      // wait for copy to finish before reading back to host
      DoPipelineBarrier(cmd, 1, &bufBarrier);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    Vec4f *ret = (Vec4f *)m_DebugData.ReadbackBuffer.Map(NULL, 0);

    memcpy(output.value.uv, ret, sizeof(Vec4f));

    m_DebugData.ReadbackBuffer.Unmap();

    return true;
  }

  virtual bool CalculateMathOp(rdcspv::ThreadState &lane, rdcspv::GLSLstd450 op,
                               const rdcarray<ShaderVariable> &params, ShaderVariable &output) override
  {
    RDCASSERT(params.size() <= 3, params.size());

    if(m_DebugData.MathPipe == VK_NULL_HANDLE)
    {
      ShaderConstParameters pipeParams = {};
      pipeParams.operation = (uint32_t)rdcspv::Op::ExtInst;
      m_DebugData.MathPipe = MakePipe(pipeParams, false, false);

      if(m_DebugData.MathPipe == VK_NULL_HANDLE)
      {
        m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                   MessageSource::RuntimeWarning,
                                   "Failed to compile graphics pipeline for math operation");
        return false;
      }
    }

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

      ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    Unwrap(m_DebugData.MathPipe));

      // push the parameters
      for(size_t i = 0; i < params.size(); i++)
      {
        float p[4] = {};
        memcpy(p, params[i].value.fv, sizeof(float) * params[i].columns);
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout),
                                       VK_SHADER_STAGE_ALL, uint32_t(sizeof(Vec4f) * i),
                                       sizeof(Vec4f), p);
      }

      // push the operation afterwards
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout),
                                     VK_SHADER_STAGE_ALL, sizeof(Vec4f) * 3, sizeof(uint32_t), &op);

      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 3, 1, 0, 0);

      ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));

      VkBufferImageCopy region = {
          0, sizeof(Vec4f), 1, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {1, 1, 1},
      };
      ObjDisp(cmd)->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(m_DebugData.Image),
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         Unwrap(m_DebugData.ReadbackBuffer.buf), 1, &region);

      VkBufferMemoryBarrier bufBarrier = {
          VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_HOST_READ_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(m_DebugData.ReadbackBuffer.buf),
          0,
          VK_WHOLE_SIZE,
      };

      // wait for copy to finish before reading back to host
      DoPipelineBarrier(cmd, 1, &bufBarrier);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    Vec4f *ret = (Vec4f *)m_DebugData.ReadbackBuffer.Map(NULL, 0);

    memcpy(output.value.uv, ret, sizeof(Vec4f));

    m_DebugData.ReadbackBuffer.Unmap();

    // these two operations change the type of the output
    if(op == rdcspv::GLSLstd450::Length || op == rdcspv::GLSLstd450::Distance)
      output.columns = 1;

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

  bool m_ResourcesDirty = false;
  uint32_t m_EventID;

  std::map<ResourceId, VkImageView> m_SampleViews;

  typedef rdcpair<ResourceId, float> SamplerBiasKey;
  std::map<SamplerBiasKey, VkSampler> m_BiasSamplers;

  bytebuf pushData;

  std::map<BindpointIndex, bytebuf> bufferCache;

  struct ImageData
  {
    uint32_t width = 0, height = 0, depth = 0;
    uint32_t texelSize = 0, rowPitch = 0, slicePitch = 0, samplePitch = 0;
    bytebuf bytes;

    byte *texel(const uint32_t *coord, uint32_t sample)
    {
      byte *ret = bytes.data();

      ret += samplePitch * sample;
      ret += slicePitch * coord[2];
      ret += rowPitch * coord[1];
      ret += texelSize * coord[0];

      return ret;
    }
  };

  std::map<BindpointIndex, ImageData> imageCache;

  template <typename T>
  const T &GetDescriptor(const rdcstr &access, BindpointIndex index, bool &valid)
  {
    static T dummy = {};

    if(index == invalidBind)
    {
      // invalid index, return a dummy data but don't mark as invalid
      return dummy;
    }

    if(index.bindset < 0 || index.bindset >= m_DescSets.count())
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

    if(index.bind < 0 || index.bind >= setData.bindings.count())
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

  bytebuf &PopulateBuffer(BindpointIndex bind)
  {
    auto insertIt = bufferCache.insert(std::make_pair(bind, bytebuf()));
    bytebuf &data = insertIt.first->second;
    if(insertIt.second)
    {
      if(bind.bindset == PushConstantBindSet)
      {
        data = pushData;
      }
      else
      {
        bool valid = true;
        const VkDescriptorBufferInfo &bufData =
            GetDescriptor<VkDescriptorBufferInfo>("accessing buffer value", bind, valid);
        if(valid)
        {
          // if the resources might be dirty from side-effects from the draw, replay back to right
          // before it.
          if(m_ResourcesDirty)
          {
            VkMarkerRegion region("un-dirtying resources");
            m_pDriver->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
            m_ResourcesDirty = false;
          }

          if(bufData.buffer != VK_NULL_HANDLE)
          {
            m_pDriver->GetDebugManager()->GetBufferData(GetResID(bufData.buffer), bufData.offset,
                                                        bufData.range, data);
          }
        }
      }
    }

    return data;
  }

  ImageData &PopulateImage(BindpointIndex bind)
  {
    auto insertIt = imageCache.insert(std::make_pair(bind, ImageData()));
    ImageData &data = insertIt.first->second;
    if(insertIt.second)
    {
      bool valid = true;
      const VkDescriptorImageInfo &imgData =
          GetDescriptor<VkDescriptorImageInfo>("performing image load/store", bind, valid);
      if(valid)
      {
        // if the resources might be dirty from side-effects from the draw, replay back to right
        // before it.
        if(m_ResourcesDirty)
        {
          VkMarkerRegion region("un-dirtying resources");
          m_pDriver->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
          m_ResourcesDirty = false;
        }

        if(imgData.imageView != VK_NULL_HANDLE)
        {
          const VulkanCreationInfo::ImageView &viewProps =
              m_Creation.m_ImageView[GetResID(imgData.imageView)];
          const VulkanCreationInfo::Image &imageProps = m_Creation.m_Image[viewProps.image];

          uint32_t mip = viewProps.range.baseMipLevel;

          data.width = RDCMAX(1U, imageProps.extent.width >> mip);
          data.height = RDCMAX(1U, imageProps.extent.height >> mip);
          if(imageProps.type == VK_IMAGE_TYPE_3D)
          {
            data.depth = RDCMAX(1U, imageProps.extent.depth >> mip);
          }
          else
          {
            data.depth = viewProps.range.layerCount;
            if(data.depth == VK_REMAINING_ARRAY_LAYERS)
              data.depth = imageProps.arrayLayers - viewProps.range.baseArrayLayer;
          }

          data.texelSize = GetByteSize(1, 1, 1, imageProps.format, 0);
          data.rowPitch = GetByteSize(data.width, 1, 1, imageProps.format, 0);
          data.slicePitch = GetByteSize(data.width, data.height, 1, imageProps.format, 0);
          data.samplePitch = GetByteSize(data.width, data.height, data.depth, imageProps.format, 0);

          const uint32_t numSlices = imageProps.type == VK_IMAGE_TYPE_3D ? 1 : data.depth;
          const uint32_t numSamples = (uint32_t)imageProps.samples;

          data.bytes.reserve(data.samplePitch * numSamples);

          // defaults are fine - no interpretation. Maybe we could use the view's typecast?
          const GetTextureDataParams params = GetTextureDataParams();

          for(uint32_t sample = 0; sample < numSamples; sample++)
          {
            for(uint32_t slice = 0; slice < numSlices; slice++)
            {
              bytebuf subBytes;
              m_pDriver->GetReplay()->GetTextureData(
                  viewProps.image, Subresource(mip, slice, sample), params, subBytes);

              // fast path, swap into output if there's only one slice and one sample (common case)
              if(numSlices == 1 && numSamples == 1)
              {
                subBytes.swap(data.bytes);
              }
              else
              {
                data.bytes.append(subBytes);
              }
            }
          }
        }
      }
    }

    return data;
  }

  VkPipeline MakePipe(const ShaderConstParameters &params, bool uintTex, bool sintTex)
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

    if(params.operation == (uint32_t)rdcspv::Op::ExtInst)
      shaderIndex = 3;

    if(m_DebugData.Module[shaderIndex] == VK_NULL_HANDLE)
    {
      rdcarray<uint32_t> spirv;

      if(shaderIndex == 3)
        GenerateMathShaderModule(spirv);
      else
        GenerateSamplingShaderModule(spirv, uintTex, sintTex);

      VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      moduleCreateInfo.pCode = spirv.data();
      moduleCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);

      VkResult vkr = m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &moduleCreateInfo, NULL,
                                                     &m_DebugData.Module[shaderIndex]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      const char *filename[] = {
          "/debug_psgather_float.spv", "/debug_psgather_uint.spv", "/debug_psgather_sint.spv",
          "/debug_psmath.spv",
      };

      if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
        FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + filename[shaderIndex], spirv);
    }

    uint32_t key = params.hashKey(shaderIndex);

    if(m_DebugData.m_Pipelines[key] != VK_NULL_HANDLE)
      return m_DebugData.m_Pipelines[key];

    RDCLOG(
        "Making new pipeline for shader type %u, operation %s, dim %u, useGrad/useGatherOffsets %u,"
        " gather channel %u, gather offsets...",
        shaderIndex, ToStr((rdcspv::Op)params.operation).c_str(), params.dim,
        params.useGradOrGatherOffsets, params.gatherChannel);

    const VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,
         m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::ShaderDebugSampleVS), "main",
         NULL},
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
    VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(m_pDriver->GetDev(),
                                                        m_pDriver->GetShaderCache()->GetPipeCache(),
                                                        1, &graphicsPipeInfo, NULL, &pipe);
    if(vkr != VK_SUCCESS)
    {
      RDCERR("Failed creating debug pipeline: %s", ToStr(vkr).c_str());
      return VK_NULL_HANDLE;
    }

    m_DebugData.m_Pipelines[key] = pipe;

    return pipe;
  }

  void GenerateMathShaderModule(rdcarray<uint32_t> &spirv)
  {
    rdcspv::Editor editor(spirv);

    // create as SPIR-V 1.0 for best compatibility
    editor.CreateEmpty(1, 0);

    editor.AddCapability(rdcspv::Capability::Shader);

    rdcspv::Id entryId = editor.MakeId();

    editor.AddOperation(
        editor.Begin(rdcspv::Section::MemoryModel),
        rdcspv::OpMemoryModel(rdcspv::AddressingModel::Logical, rdcspv::MemoryModel::GLSL450));

    rdcspv::Id glsl450 = editor.ImportExtInst("GLSL.std.450");

    rdcspv::Id u32 = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id f32 = editor.DeclareType(rdcspv::scalar<float>());
    rdcspv::Id v4f32 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));

    rdcspv::Id pushStructID =
        editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {v4f32, v4f32, v4f32, u32}));
    editor.AddDecoration(rdcspv::OpDecorate(pushStructID, rdcspv::Decoration::Block));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 1, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f))));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 2, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f) * 2)));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 3, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f) * 3)));
    editor.SetMemberName(pushStructID, 0, "a");
    editor.SetMemberName(pushStructID, 1, "b");
    editor.SetMemberName(pushStructID, 2, "c");
    editor.SetMemberName(pushStructID, 3, "op");

    rdcspv::Id pushPtrType =
        editor.DeclareType(rdcspv::Pointer(pushStructID, rdcspv::StorageClass::PushConstant));
    rdcspv::Id pushVar = editor.AddVariable(
        rdcspv::OpVariable(pushPtrType, editor.MakeId(), rdcspv::StorageClass::PushConstant));
    editor.SetName(pushVar, "pushData");

    rdcspv::Id pushv4f32Type =
        editor.DeclareType(rdcspv::Pointer(v4f32, rdcspv::StorageClass::PushConstant));
    rdcspv::Id pushu32Type =
        editor.DeclareType(rdcspv::Pointer(u32, rdcspv::StorageClass::PushConstant));

    rdcspv::Id outPtrType = editor.DeclareType(rdcspv::Pointer(v4f32, rdcspv::StorageClass::Output));

    rdcspv::Id outVar = editor.AddVariable(
        rdcspv::OpVariable(outPtrType, editor.MakeId(), rdcspv::StorageClass::Output));
    editor.AddDecoration(
        rdcspv::OpDecorate(outVar, rdcspv::DecorationParam<rdcspv::Decoration::Location>(0)));

    editor.SetName(outVar, "output");

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

    rdcspv::Id consts[] = {
        editor.AddConstantImmediate<uint32_t>(0), editor.AddConstantImmediate<uint32_t>(1),
        editor.AddConstantImmediate<uint32_t>(2), editor.AddConstantImmediate<uint32_t>(3),
    };

    // load the parameters and the op
    rdcspv::Id aPtr =
        func.add(rdcspv::OpAccessChain(pushv4f32Type, editor.MakeId(), pushVar, {consts[0]}));
    rdcspv::Id bPtr =
        func.add(rdcspv::OpAccessChain(pushv4f32Type, editor.MakeId(), pushVar, {consts[1]}));
    rdcspv::Id cPtr =
        func.add(rdcspv::OpAccessChain(pushv4f32Type, editor.MakeId(), pushVar, {consts[2]}));
    rdcspv::Id opPtr =
        func.add(rdcspv::OpAccessChain(pushu32Type, editor.MakeId(), pushVar, {consts[3]}));
    rdcspv::Id a = func.add(rdcspv::OpLoad(v4f32, editor.MakeId(), aPtr));
    rdcspv::Id b = func.add(rdcspv::OpLoad(v4f32, editor.MakeId(), bPtr));
    rdcspv::Id c = func.add(rdcspv::OpLoad(v4f32, editor.MakeId(), cPtr));
    rdcspv::Id opParam = func.add(rdcspv::OpLoad(u32, editor.MakeId(), opPtr));

    rdcspv::Id breakLabel = editor.MakeId();
    rdcspv::Id defaultLabel = editor.MakeId();

    rdcarray<rdcspv::PairLiteralIntegerIdRef> targets;

    rdcspv::OperationList cases;

    // all these operations take one parameter and only operate on floats (possibly vectors)
    for(rdcspv::GLSLstd450 op : {
            rdcspv::GLSLstd450::Sin,       rdcspv::GLSLstd450::Cos,
            rdcspv::GLSLstd450::Tan,       rdcspv::GLSLstd450::Asin,
            rdcspv::GLSLstd450::Acos,      rdcspv::GLSLstd450::Atan,
            rdcspv::GLSLstd450::Sinh,      rdcspv::GLSLstd450::Cosh,
            rdcspv::GLSLstd450::Tanh,      rdcspv::GLSLstd450::Asinh,
            rdcspv::GLSLstd450::Acosh,     rdcspv::GLSLstd450::Atanh,
            rdcspv::GLSLstd450::Exp,       rdcspv::GLSLstd450::Log,
            rdcspv::GLSLstd450::Exp2,      rdcspv::GLSLstd450::Log2,
            rdcspv::GLSLstd450::Sqrt,      rdcspv::GLSLstd450::InverseSqrt,
            rdcspv::GLSLstd450::Normalize,
        })
    {
      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result = cases.add(rdcspv::OpGLSL450(v4f32, editor.MakeId(), glsl450, op, {a}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    // these take two parameters, but are otherwise identical
    for(rdcspv::GLSLstd450 op : {rdcspv::GLSLstd450::Atan2, rdcspv::GLSLstd450::Pow})
    {
      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result = cases.add(rdcspv::OpGLSL450(v4f32, editor.MakeId(), glsl450, op, {a, b}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    rdcspv::Id zerof = editor.AddConstantImmediate<float>(0.0f);

    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Fma;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result =
          cases.add(rdcspv::OpGLSL450(v4f32, editor.MakeId(), glsl450, op, {a, b, c}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    // these ones are special
    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Length;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result = cases.add(rdcspv::OpGLSL450(f32, editor.MakeId(), glsl450, op, {a}));
      rdcspv::Id resultvec = cases.add(
          rdcspv::OpCompositeConstruct(v4f32, editor.MakeId(), {result, zerof, zerof, zerof}));
      cases.add(rdcspv::OpStore(outVar, resultvec));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Distance;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result = cases.add(rdcspv::OpGLSL450(f32, editor.MakeId(), glsl450, op, {a, b}));
      rdcspv::Id resultvec = cases.add(
          rdcspv::OpCompositeConstruct(v4f32, editor.MakeId(), {result, zerof, zerof, zerof}));
      cases.add(rdcspv::OpStore(outVar, resultvec));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Refract;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id eta = cases.add(rdcspv::OpCompositeExtract(f32, editor.MakeId(), c, {0}));
      rdcspv::Id result =
          cases.add(rdcspv::OpGLSL450(v4f32, editor.MakeId(), glsl450, op, {a, b, eta}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    func.add(rdcspv::OpSelectionMerge(breakLabel, rdcspv::SelectionControl::None));
    func.add(rdcspv::OpSwitch(opParam, defaultLabel, targets));

    func.append(cases);

    // default: store NULL data
    func.add(rdcspv::OpLabel(defaultLabel));
    func.add(rdcspv::OpStore(outVar,
                             editor.AddConstant(rdcspv::OpConstantNull(v4f32, editor.MakeId()))));
    func.add(rdcspv::OpBranch(breakLabel));

    func.add(rdcspv::OpLabel(breakLabel));
    func.add(rdcspv::OpReturn());
    func.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(func);
  }

  void GenerateSamplingShaderModule(rdcarray<uint32_t> &spirv, bool uintTex, bool sintTex)
  {
    // this could be done as a glsl shader, but glslang has some bugs compiling the specialisation
    // constants, so we generate it by hand - which isn't too hard

    rdcspv::Editor editor(spirv);

    // create as SPIR-V 1.0 for best compatibility
    editor.CreateEmpty(1, 0);

    editor.AddCapability(rdcspv::Capability::Shader);
    editor.AddCapability(rdcspv::Capability::ImageQuery);
    editor.AddCapability(rdcspv::Capability::Sampled1D);
    editor.AddCapability(rdcspv::Capability::SampledBuffer);

    if(m_pDriver->GetDeviceEnabledFeatures().shaderResourceMinLod)
      editor.AddCapability(rdcspv::Capability::MinLod);
    if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended)
      editor.AddCapability(rdcspv::Capability::ImageGatherExtended);

    const bool cubeArray = (m_pDriver->GetDeviceEnabledFeatures().imageCubeArray != VK_FALSE);

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
    rdcspv::Id v4f32 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));

    // int2[4]
    rdcspv::Id a4v2i32 = editor.AddType(
        rdcspv::OpTypeArray(editor.MakeId(), v2i32, editor.AddConstantImmediate<uint32_t>(4)));

    rdcspv::Scalar base = rdcspv::scalar<float>();
    if(uintTex)
      base = rdcspv::scalar<uint32_t>();
    else if(sintTex)
      base = rdcspv::scalar<int32_t>();

    rdcspv::Id resultType = editor.DeclareType(rdcspv::Vector(base, 4));
    rdcspv::Id scalarResultType = editor.DeclareType(base);

// add specialisation constants for all the parameters
#define MEMBER_IDX(struct, name) uint32_t(offsetof(struct, name) / sizeof(uint32_t))

#define DECL_SPECID(type, name, value)                                                     \
  rdcspv::Id name =                                                                        \
      editor.AddSpecConstantImmediate<type>(0U, MEMBER_IDX(ShaderConstParameters, value)); \
  editor.SetName(name, "spec_" #name);

    DECL_SPECID(uint32_t, operation, operation);
    DECL_SPECID(bool, useGradOrGatherOffsets, useGradOrGatherOffsets);
    DECL_SPECID(uint32_t, dim, dim);
    DECL_SPECID(int32_t, gatherChannel, gatherChannel);
    DECL_SPECID(int32_t, gather_u0, gatherOffsets.u0);
    DECL_SPECID(int32_t, gather_v0, gatherOffsets.v0);
    DECL_SPECID(int32_t, gather_u1, gatherOffsets.u1);
    DECL_SPECID(int32_t, gather_v1, gatherOffsets.v1);
    DECL_SPECID(int32_t, gather_u2, gatherOffsets.u2);
    DECL_SPECID(int32_t, gather_v2, gatherOffsets.v2);
    DECL_SPECID(int32_t, gather_u3, gatherOffsets.u3);
    DECL_SPECID(int32_t, gather_v3, gatherOffsets.v3);

    struct StructMember
    {
      rdcspv::Id loadedType;
      rdcspv::Id ptrType;
      rdcspv::Id loadedId;
      const char *name;
      uint32_t memberIndex;
    };

    rdcarray<rdcspv::Id> memberIds;
    rdcarray<StructMember> cbufferMembers;

    rdcspv::Id type_int32_t = editor.DeclareType(rdcspv::scalar<int32_t>());
    rdcspv::Id type_float = editor.DeclareType(rdcspv::scalar<float>());

    rdcspv::Id uniformptr_int32_t =
        editor.DeclareType(rdcspv::Pointer(type_int32_t, rdcspv::StorageClass::Uniform));
    rdcspv::Id uniformptr_float =
        editor.DeclareType(rdcspv::Pointer(type_float, rdcspv::StorageClass::Uniform));

#define DECL_UNIFORM(type, name, value)                                                  \
  rdcspv::Id name = editor.MakeId();                                                     \
  editor.SetName(name, "uniform_" #name);                                                \
  cbufferMembers.push_back({CONCAT(type_, type), CONCAT(uniformptr_, type), name, #name, \
                            MEMBER_IDX(ShaderUniformParameters, value)});                \
  memberIds.push_back(CONCAT(type_, type));
    DECL_UNIFORM(int32_t, texel_u, texel_uvw.x);
    DECL_UNIFORM(int32_t, texel_v, texel_uvw.y);
    DECL_UNIFORM(int32_t, texel_w, texel_uvw.z);
    DECL_UNIFORM(int32_t, texel_lod, texel_lod);
    DECL_UNIFORM(float, u, uvwa.x);
    DECL_UNIFORM(float, v, uvwa.y);
    DECL_UNIFORM(float, w, uvwa.z);
    DECL_UNIFORM(float, cube_a, uvwa.w);
    DECL_UNIFORM(float, dudx, ddx.x);
    DECL_UNIFORM(float, dvdx, ddx.y);
    DECL_UNIFORM(float, dwdx, ddx.z);
    DECL_UNIFORM(float, dudy, ddy.x);
    DECL_UNIFORM(float, dvdy, ddy.y);
    DECL_UNIFORM(float, dwdy, ddy.z);
    DECL_UNIFORM(int32_t, offset_x, offset.x);
    DECL_UNIFORM(int32_t, offset_y, offset.y);
    DECL_UNIFORM(int32_t, offset_z, offset.z);
    DECL_UNIFORM(int32_t, sampleIdx, sampleIdx);
    DECL_UNIFORM(float, compare, compare);
    DECL_UNIFORM(float, lod, lod);
    DECL_UNIFORM(float, minlod, minlod);

    rdcspv::Id cbufferStructID = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), memberIds));
    editor.AddDecoration(rdcspv::OpDecorate(cbufferStructID, rdcspv::Decoration::Block));
    for(const StructMember &m : cbufferMembers)
    {
      editor.AddDecoration(rdcspv::OpMemberDecorate(
          cbufferStructID, m.memberIndex,
          rdcspv::DecorationParam<rdcspv::Decoration::Offset>(m.memberIndex * sizeof(uint32_t))));
      editor.SetMemberName(cbufferStructID, m.memberIndex, m.name);
    }

    rdcspv::Id gather_0 = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {gather_u0, gather_v0}));
    rdcspv::Id gather_1 = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {gather_u1, gather_v1}));
    rdcspv::Id gather_2 = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {gather_u2, gather_v2}));
    rdcspv::Id gather_3 = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {gather_u3, gather_v3}));

    rdcspv::Id gatherOffsets = editor.AddConstant(rdcspv::OpSpecConstantComposite(
        a4v2i32, editor.MakeId(), {gather_0, gather_1, gather_2, gather_3}));

    editor.SetName(gatherOffsets, "gatherOffsets");

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
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::_3D, 0, 0, 0, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::_2D, 0, 1, 1, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::Cube, 0, cubeArray ? 1 : 0, 0, 1, unk)),
        editor.DeclareType(rdcspv::Image(base, rdcspv::Dim::Buffer, 0, 0, 0, 1, unk)),
        editor.DeclareType(rdcspv::Sampler()),
        cbufferStructID,
    };
    rdcspv::Id bindVars[(uint32_t)ShaderDebugBind::Count];
    rdcspv::Id texSampCombinedTypes[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[1])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[2])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[3])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[4])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[5])),
        editor.DeclareType(rdcspv::SampledImage(texSampTypes[6])),
        rdcspv::Id(),
        rdcspv::Id(),
    };

    for(size_t i = (size_t)ShaderDebugBind::First; i < (size_t)ShaderDebugBind::Count; i++)
    {
      rdcspv::StorageClass storageClass = rdcspv::StorageClass::UniformConstant;

      if(i == (size_t)ShaderDebugBind::Constants)
        storageClass = rdcspv::StorageClass::Uniform;

      rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(texSampTypes[i], storageClass));

      bindVars[i] = editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), storageClass));

      editor.AddDecoration(rdcspv::OpDecorate(
          bindVars[i], rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0U)));
      editor.AddDecoration(rdcspv::OpDecorate(
          bindVars[i], rdcspv::DecorationParam<rdcspv::Decoration::Binding>((uint32_t)i)));
    }

    editor.SetName(bindVars[(size_t)ShaderDebugBind::Tex1D], "Tex1D");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Tex2D], "Tex2D");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Tex3D], "Tex3D");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Tex2DMS], "Tex2DMS");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::TexCube], "TexCube");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Buffer], "Buffer");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Sampler], "Sampler");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Constants], "CBuffer");

    rdcspv::Id uvwa_ptr = editor.DeclareType(rdcspv::Pointer(v4f32, rdcspv::StorageClass::Input));
    rdcspv::Id input_uvwa_var = editor.AddVariable(
        rdcspv::OpVariable(uvwa_ptr, editor.MakeId(), rdcspv::StorageClass::Input));
    editor.AddDecoration(rdcspv::OpDecorate(
        input_uvwa_var, rdcspv::DecorationParam<rdcspv::Decoration::Location>(0)));

    editor.SetName(input_uvwa_var, "input_uvwa");

    // register the entry point
    editor.AddOperation(editor.Begin(rdcspv::Section::EntryPoints),
                        rdcspv::OpEntryPoint(rdcspv::ExecutionModel::Fragment, entryId, "main",
                                             {input_uvwa_var, outVar}));
    editor.AddOperation(editor.Begin(rdcspv::Section::ExecutionMode),
                        rdcspv::OpExecutionMode(entryId, rdcspv::ExecutionMode::OriginUpperLeft));

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());
    rdcspv::Id funcType = editor.DeclareType(rdcspv::FunctionType(voidType, {}));

    rdcspv::OperationList func;
    func.add(rdcspv::OpFunction(voidType, entryId, rdcspv::FunctionControl::None, funcType));
    func.add(rdcspv::OpLabel(editor.MakeId()));

    // access chain and load all the cbuffer variables
    for(const StructMember &m : cbufferMembers)
    {
      rdcspv::Id ptr = func.add(rdcspv::OpAccessChain(
          m.ptrType, editor.MakeId(), bindVars[(size_t)ShaderDebugBind::Constants],
          {editor.AddConstantImmediate<uint32_t>(m.memberIndex)}));
      func.add(rdcspv::OpLoad(m.loadedType, m.loadedId, ptr));
    }

    // declare cbuffer composites
    rdcspv::Id texel_uv =
        func.add(rdcspv::OpCompositeConstruct(v2i32, editor.MakeId(), {texel_u, texel_v}));
    rdcspv::Id texel_uvw =
        func.add(rdcspv::OpCompositeConstruct(v3i32, editor.MakeId(), {texel_u, texel_v, texel_w}));

    editor.SetName(texel_uv, "texel_uv");
    editor.SetName(texel_uvw, "texel_uvw");

    rdcspv::Id uv = func.add(rdcspv::OpCompositeConstruct(v2f32, editor.MakeId(), {u, v}));
    rdcspv::Id uvw = func.add(rdcspv::OpCompositeConstruct(v3f32, editor.MakeId(), {u, v, w}));
    rdcspv::Id uvwa =
        func.add(rdcspv::OpCompositeConstruct(v4f32, editor.MakeId(), {u, v, w, cube_a}));

    editor.SetName(uv, "uv");
    editor.SetName(uvw, "uvw");
    editor.SetName(uvwa, "uvwa");

    rdcspv::Id ddx_uv = func.add(rdcspv::OpCompositeConstruct(v2f32, editor.MakeId(), {dudx, dvdx}));
    rdcspv::Id ddx_uvw =
        func.add(rdcspv::OpCompositeConstruct(v3f32, editor.MakeId(), {dudx, dvdx, dwdx}));

    editor.SetName(ddx_uv, "ddx_uv");
    editor.SetName(ddx_uvw, "ddx_uvw");

    rdcspv::Id ddy_uv = func.add(rdcspv::OpCompositeConstruct(v2f32, editor.MakeId(), {dudy, dvdy}));
    rdcspv::Id ddy_uvw =
        func.add(rdcspv::OpCompositeConstruct(v3f32, editor.MakeId(), {dudy, dvdy, dwdy}));

    editor.SetName(ddy_uv, "ddy_uv");
    editor.SetName(ddy_uvw, "ddy_uvw");

    rdcspv::Id offset_xy =
        func.add(rdcspv::OpCompositeConstruct(v2i32, editor.MakeId(), {offset_x, offset_y}));
    rdcspv::Id offset_xyz = func.add(
        rdcspv::OpCompositeConstruct(v3i32, editor.MakeId(), {offset_x, offset_y, offset_z}));

    editor.SetName(offset_xy, "offset_xy");
    editor.SetName(offset_xyz, "offset_xyz");

    rdcspv::Id input_uvwa = func.add(rdcspv::OpLoad(v4f32, editor.MakeId(), input_uvwa_var));
    rdcspv::Id input_uvw =
        func.add(rdcspv::OpVectorShuffle(v3f32, editor.MakeId(), input_uvwa, input_uvwa, {0, 1, 2}));
    rdcspv::Id input_uv =
        func.add(rdcspv::OpVectorShuffle(v2f32, editor.MakeId(), input_uvw, input_uvw, {0, 1}));
    rdcspv::Id input_u = func.add(rdcspv::OpCompositeExtract(f32, editor.MakeId(), input_uvw, {0}));

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

    rdcspv::Id texel_coord[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        texel_uv,        // 1D - u and array
        texel_uvw,       // 2D - u,v and array
        texel_uvw,       // 3D - u,v,w
        texel_uvw,       // 2DMS - u,v and array
        rdcspv::Id(),    // Cube
        texel_u,         // Buffer - u
    };

    // only used for QueryLod, so we can ignore MSAA/Buffer
    rdcspv::Id input_coord[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        input_u,         // 1D - u
        input_uv,        // 2D - u,v
        input_uvw,       // 3D - u,v,w
        rdcspv::Id(),    // 2DMS
        input_uvw,       // Cube - u,v,w
        rdcspv::Id(),    // Buffer
    };

    rdcspv::Id coord[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        uv,                        // 1D - u and array
        uvw,                       // 2D - u,v and array
        uvw,                       // 3D - u,v,w
        uvw,                       // 2DMS - u,v and array
        cubeArray ? uvwa : uvw,    // Cube - u,v,w and array (if supported)
        u,                         // Buffer - u
    };

    rdcspv::Id offsets[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        offset_x,        // 1D - u
        offset_xy,       // 2D - u,v
        offset_xyz,      // 3D - u,v,w
        offset_xy,       // 2DMS - u,v
        rdcspv::Id(),    // Cube - not valid
        offset_x,        // Buffer - u
    };

    rdcspv::Id ddxs[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        dudx,       // 1D - u
        ddx_uv,     // 2D - u,v
        ddx_uvw,    // 3D - u,v,w
        ddx_uv,     // 2DMS - u,v
        ddx_uvw,    // Cube - u,v,w
        dudx,       // Buffer - u
    };

    rdcspv::Id ddys[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        dudy,       // 1D - u
        ddy_uv,     // 2D - u,v
        ddy_uvw,    // 3D - u,v,w
        ddy_uv,     // 2DMS - u,v
        ddy_uvw,    // Cube - u,v,w
        dudy,       // Buffer - u
    };

    uint32_t sampIdx = (uint32_t)ShaderDebugBind::Sampler;

    rdcspv::Id zerof = editor.AddConstantImmediate<float>(0.0f);

    for(uint32_t i = (uint32_t)ShaderDebugBind::First; i < (uint32_t)ShaderDebugBind::Count; i++)
    {
      if(i == sampIdx || i == (uint32_t)ShaderDebugBind::Constants)
        continue;

      // can't fetch from cubemaps
      if(i != (uint32_t)ShaderDebugBind::TexCube)
      {
        rdcspv::Op op = rdcspv::Op::ImageFetch;

        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        rdcspv::ImageOperandsAndParamDatas imageOperands;

        if(i != (uint32_t)ShaderDebugBind::Buffer && i != (uint32_t)ShaderDebugBind::Tex2DMS)
          imageOperands.setLod(texel_lod);

        if(i == (uint32_t)ShaderDebugBind::Tex2DMS)
          imageOperands.setSample(sampleIdx);

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loaded = cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), bindVars[i]));
        rdcspv::Id sampleResult = cases.add(rdcspv::OpImageFetch(
            resultType, editor.MakeId(), loaded, texel_coord[i], imageOperands));
        cases.add(rdcspv::OpStore(outVar, sampleResult));
        cases.add(rdcspv::OpBranch(breakLabel));
      }

      // buffers and multisampled images don't support sampling, so skip the other operations at
      // this point
      if(i == (uint32_t)ShaderDebugBind::Buffer || i == (uint32_t)ShaderDebugBind::Tex2DMS)
        continue;

      {
        rdcspv::Op op = rdcspv::Op::ImageQueryLod;

        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loadedImage =
            cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), bindVars[i]));
        rdcspv::Id loadedSampler =
            cases.add(rdcspv::OpLoad(texSampTypes[sampIdx], editor.MakeId(), bindVars[sampIdx]));

        rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
            texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

        rdcspv::Id sampleResult =
            cases.add(rdcspv::OpImageQueryLod(v2f32, editor.MakeId(), combined, input_coord[i]));
        sampleResult = cases.add(rdcspv::OpVectorShuffle(v4f32, editor.MakeId(), sampleResult,
                                                         sampleResult, {0, 1, 0, 1}));

        // if we're sampling from an integer texture the output variable will be the same type.
        // Just bitcast the float bits into it, which will come out the other side the right type.
        if(uintTex || sintTex)
          sampleResult = cases.add(rdcspv::OpBitcast(resultType, editor.MakeId(), sampleResult));

        cases.add(rdcspv::OpStore(outVar, sampleResult));
        cases.add(rdcspv::OpBranch(breakLabel));
      }

      for(rdcspv::Op op : {rdcspv::Op::ImageSampleExplicitLod, rdcspv::Op::ImageSampleImplicitLod})
      {
        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        rdcspv::ImageOperandsAndParamDatas imageOperands;

        if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
           offsets[i] != rdcspv::Id())
          imageOperands.setOffset(offsets[i]);

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loadedImage =
            cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), bindVars[i]));
        rdcspv::Id loadedSampler =
            cases.add(rdcspv::OpLoad(texSampTypes[sampIdx], editor.MakeId(), bindVars[sampIdx]));

        rdcspv::Id mergeLabel = editor.MakeId();
        rdcspv::Id gradCase = editor.MakeId();
        rdcspv::Id lodCase = editor.MakeId();
        cases.add(rdcspv::OpSelectionMerge(mergeLabel, rdcspv::SelectionControl::None));
        cases.add(rdcspv::OpBranchConditional(useGradOrGatherOffsets, gradCase, lodCase));

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
          if(m_pDriver->GetDeviceEnabledFeatures().shaderResourceMinLod)
            operands.setMinLod(minlod);
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

      bool emitDRef = true;

      // on Qualcomm we only emit Dref instructions against 2D textures, otherwise the compiler may
      // crash.
      if(m_pDriver->GetDriverInfo().QualcommDrefNon2DCompileCrash())
        emitDRef = (i == (uint32_t)ShaderDebugBind::Tex2D);

      if(emitDRef)
      {
        for(rdcspv::Op op :
            {rdcspv::Op::ImageSampleDrefExplicitLod, rdcspv::Op::ImageSampleDrefImplicitLod})
        {
          rdcspv::Id label = editor.MakeId();
          targets.push_back({(uint32_t)op * 10 + i, label});

          rdcspv::ImageOperandsAndParamDatas imageOperands;

          if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
             offsets[i] != rdcspv::Id())
            imageOperands.setOffset(offsets[i]);

          cases.add(rdcspv::OpLabel(label));
          rdcspv::Id loadedImage =
              cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), bindVars[i]));
          rdcspv::Id loadedSampler =
              cases.add(rdcspv::OpLoad(texSampTypes[sampIdx], editor.MakeId(), bindVars[sampIdx]));

          rdcspv::Id mergeLabel = editor.MakeId();
          rdcspv::Id gradCase = editor.MakeId();
          rdcspv::Id lodCase = editor.MakeId();
          cases.add(rdcspv::OpSelectionMerge(mergeLabel, rdcspv::SelectionControl::None));
          cases.add(rdcspv::OpBranchConditional(useGradOrGatherOffsets, gradCase, lodCase));

          rdcspv::Id lodResult;
          {
            cases.add(rdcspv::OpLabel(lodCase));
            rdcspv::ImageOperandsAndParamDatas operands = imageOperands;
            operands.setLod(lod);
            rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
                texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

            lodResult = cases.add(rdcspv::OpImageSampleDrefExplicitLod(
                scalarResultType, editor.MakeId(), combined, coord[i], compare, operands));

            cases.add(rdcspv::OpBranch(mergeLabel));
          }

          rdcspv::Id gradResult;
          {
            cases.add(rdcspv::OpLabel(gradCase));
            rdcspv::ImageOperandsAndParamDatas operands = imageOperands;
            operands.setGrad(ddxs[i], ddys[i]);
            if(m_pDriver->GetDeviceEnabledFeatures().shaderResourceMinLod)
              operands.setMinLod(minlod);
            rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
                texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

            gradResult = cases.add(rdcspv::OpImageSampleDrefExplicitLod(
                scalarResultType, editor.MakeId(), combined, coord[i], compare, operands));

            cases.add(rdcspv::OpBranch(mergeLabel));
          }

          cases.add(rdcspv::OpLabel(mergeLabel));
          rdcspv::Id scalarSampleResult = cases.add(rdcspv::OpPhi(
              scalarResultType, editor.MakeId(), {{lodResult, lodCase}, {gradResult, gradCase}}));
          rdcspv::Id sampleResult = cases.add(rdcspv::OpCompositeConstruct(
              resultType, editor.MakeId(), {scalarSampleResult, zerof, zerof, zerof}));
          cases.add(rdcspv::OpStore(outVar, sampleResult));
          cases.add(rdcspv::OpBranch(breakLabel));
        }
      }

      // can only gather with 2D/Cube textures
      if(i == (uint32_t)ShaderDebugBind::Tex1D || i == (uint32_t)ShaderDebugBind::Tex3D)
        continue;

      for(rdcspv::Op op : {rdcspv::Op::ImageGather, rdcspv::Op::ImageDrefGather})
      {
        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        rdcspv::ImageOperandsAndParamDatas imageOperands;

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loadedImage =
            cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), bindVars[i]));
        rdcspv::Id loadedSampler =
            cases.add(rdcspv::OpLoad(texSampTypes[sampIdx], editor.MakeId(), bindVars[sampIdx]));

        rdcspv::Id sampleResult;
        if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended)
        {
          rdcspv::Id mergeLabel = editor.MakeId();
          rdcspv::Id constsCase = editor.MakeId();
          rdcspv::Id baseCase = editor.MakeId();
          cases.add(rdcspv::OpSelectionMerge(mergeLabel, rdcspv::SelectionControl::None));
          cases.add(rdcspv::OpBranchConditional(useGradOrGatherOffsets, constsCase, baseCase));

          rdcspv::Id baseResult;
          {
            cases.add(rdcspv::OpLabel(baseCase));
            rdcspv::ImageOperandsAndParamDatas operands = imageOperands;

            if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
               offsets[i] != rdcspv::Id())
              imageOperands.setOffset(offsets[i]);

            rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
                texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

            if(op == rdcspv::Op::ImageGather)
              baseResult = cases.add(rdcspv::OpImageGather(resultType, editor.MakeId(), combined,
                                                           coord[i], gatherChannel, imageOperands));
            else
              baseResult = cases.add(rdcspv::OpImageDrefGather(
                  resultType, editor.MakeId(), combined, coord[i], compare, imageOperands));

            cases.add(rdcspv::OpBranch(mergeLabel));
          }

          rdcspv::Id constsResult;
          {
            cases.add(rdcspv::OpLabel(constsCase));
            rdcspv::ImageOperandsAndParamDatas operands = imageOperands;

            // if this feature isn't available, this path will never be exercised (since we only
            // come in here when the actual shader used const offsets) so it's fine to drop it in
            // that case to ensure the module is still legal.
            if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended)
              operands.setConstOffsets(gatherOffsets);

            rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
                texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

            if(op == rdcspv::Op::ImageGather)
              constsResult = cases.add(rdcspv::OpImageGather(
                  resultType, editor.MakeId(), combined, coord[i], gatherChannel, imageOperands));
            else
              constsResult = cases.add(rdcspv::OpImageDrefGather(
                  resultType, editor.MakeId(), combined, coord[i], compare, imageOperands));

            cases.add(rdcspv::OpBranch(mergeLabel));
          }

          cases.add(rdcspv::OpLabel(mergeLabel));
          sampleResult = cases.add(rdcspv::OpPhi(
              resultType, editor.MakeId(), {{baseResult, baseCase}, {constsResult, constsCase}}));
        }
        else
        {
          if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
             offsets[i] != rdcspv::Id())
            imageOperands.setOffset(offsets[i]);

          rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
              texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

          if(op == rdcspv::Op::ImageGather)
            sampleResult = cases.add(rdcspv::OpImageGather(resultType, editor.MakeId(), combined,
                                                           coord[i], gatherChannel, imageOperands));
          else
            sampleResult = cases.add(rdcspv::OpImageDrefGather(
                resultType, editor.MakeId(), combined, coord[i], compare, imageOperands));
        }

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
  AddressMSB,
  Count,
};

static const uint32_t validMagicNumber = 12345;

struct PSHit
{
  Vec4f pos;
  uint32_t prim;
  uint32_t sample;
  uint32_t valid;
  float ddxDerivCheck;
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

    // same for decorations
    it = editor.Begin(rdcspv::Section::Annotations);
    end = editor.End(rdcspv::Section::Annotations);
    while(it < end)
    {
      if(it.opcode() == rdcspv::Op::Decorate)
      {
        rdcspv::OpDecorate dec(it);

        if(removedIds.contains(dec.target))
          editor.Remove(it);
      }
      else if(it.opcode() == rdcspv::Op::DecorateId)
      {
        rdcspv::OpDecorateId dec(it);

        if(removedIds.contains(dec.target))
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
    rdcspv::Id type;
    uint32_t member = ~0U;
  } fragCoord, primitiveID, sampleIndex;

  // look to see which ones are already provided
  for(size_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
  {
    const SigParameter &param = shadRefl.refl.inputSignature[i];

    BuiltinAccess *access = NULL;

    if(param.systemValue == ShaderBuiltin::Position)
    {
      access = &fragCoord;
    }
    else if(param.systemValue == ShaderBuiltin::PrimitiveIndex)
    {
      access = &primitiveID;

      access->type = VarTypeCompType(param.varType) == CompType::SInt
                         ? editor.DeclareType(rdcspv::scalar<int32_t>())
                         : editor.DeclareType(rdcspv::scalar<uint32_t>());
    }
    else if(param.systemValue == ShaderBuiltin::MSAASampleIndex)
    {
      access = &sampleIndex;

      access->type = VarTypeCompType(param.varType) == CompType::SInt
                         ? editor.DeclareType(rdcspv::scalar<int32_t>())
                         : editor.DeclareType(rdcspv::scalar<uint32_t>());
    }

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
    fragCoord.type = type;

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
    primitiveID.type = type;

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
    sampleIndex.type = type;

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

      rdcspv::Scalar base = rdcspv::scalar(param.varType);

      values[i].structIndex = (uint32_t)offsets.size();

      uint32_t width = (base.width / 8);

      // treat bools as uints
      if(base.type == rdcspv::Op::TypeBool)
        width = 4;

      offsets.push_back(structStride);
      structStride += param.compCount * width;

      if(param.compCount == 1)
        values[i].valueType = editor.DeclareType(base);
      else
        values[i].valueType = editor.DeclareType(rdcspv::Vector(base, param.compCount));

      if(values[i].valueType == boolType)
        ids.push_back(uint32Type);
      else
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
      // float ddxDerivCheck;
      floatType,

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
    editor.SetMemberName(PSHit, member, "ddxDerivCheck");
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
      // uint test;
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
        bufBase, 1, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(uint32_t))));
    editor.SetMemberName(bufBase, 1, "total_count");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        bufBase, 2, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f))));
    editor.SetMemberName(bufBase, 2, "hits");
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
    if(storageMode == KHR_bda)
    {
      rdcspv::Id addressConstantLSB =
          editor.AddSpecConstantImmediate<uint32_t>(0U, (uint32_t)InputSpecConstant::Address);
      rdcspv::Id addressConstantMSB =
          editor.AddSpecConstantImmediate<uint32_t>(0U, (uint32_t)InputSpecConstant::AddressMSB);

      rdcspv::Id uint2 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 2));

      addressConstant = editor.AddConstant(rdcspv::OpSpecConstantComposite(
          uint2, editor.MakeId(), {addressConstantLSB, addressConstantMSB}));
    }
    else
    {
      addressConstant =
          editor.AddSpecConstantImmediate<uint64_t>(0ULL, (uint32_t)InputSpecConstant::Address);
    }

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
  rdcspv::Id floatBufPtr = editor.DeclareType(rdcspv::Pointer(floatType, bufferClass));

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

        rdcspv::Id valueType = values[i].valueType;

        rdcspv::Id ptrType =
            editor.DeclareType(rdcspv::Pointer(valueType, rdcspv::StorageClass::Input));

        // if we have no access chain it's a global pointer of the type we want, so just load
        // straight out of it
        rdcspv::Id ptr;
        if(accessIndices.empty())
          ptr = access.ID;
        else
          ptr = ops.add(rdcspv::OpAccessChain(ptrType, editor.MakeId(), access.ID, accessIndices));

        rdcspv::Id base = ops.add(rdcspv::OpLoad(valueType, editor.MakeId(), ptr));

        if(valueType == boolType)
        {
          valueType = uint32Type;
          // can't store bools directly, need to convert to uint
          base = ops.add(
              rdcspv::OpSelect(valueType, editor.MakeId(), base, getUIntConst(1), getUIntConst(0)));
        }

        values[i].data[Variant_Base] = base;

        editor.SetName(base, StringFormat::Fmt("__rd_base_%zu_%s", i, param.varName.c_str()));

        // only float values have derivatives
        if(VarTypeCompType(param.varType) == CompType::Float)
        {
          values[i].data[Variant_ddxcoarse] =
              ops.add(rdcspv::OpDPdxCoarse(valueType, editor.MakeId(), base));
          values[i].data[Variant_ddycoarse] =
              ops.add(rdcspv::OpDPdyCoarse(valueType, editor.MakeId(), base));
          values[i].data[Variant_ddxfine] =
              ops.add(rdcspv::OpDPdxFine(valueType, editor.MakeId(), base));
          values[i].data[Variant_ddyfine] =
              ops.add(rdcspv::OpDPdyFine(valueType, editor.MakeId(), base));

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
                  editor.AddConstant(rdcspv::OpConstantNull(valueType, editor.MakeId()));

          editor.SetName(values[i].data[Variant_ddxcoarse],
                         StringFormat::Fmt("__rd_noderiv_%zu_%s", i, param.varName.c_str()));
        }
      }

      rdcspv::Id structPtr = ssboVar;

      if(structPtr == rdcspv::Id())
      {
        // if we don't have the struct as a bind, we need to cast it from the pointer. In
        // KHR_buffer_device_address we bitcast since we store it as a uint2
        if(storageMode == KHR_bda)
          structPtr = ops.add(rdcspv::OpBitcast(bufptrtype, editor.MakeId(), addressConstant));
        else
          structPtr = ops.add(rdcspv::OpConvertUToPtr(bufptrtype, editor.MakeId(), addressConstant));

        editor.SetName(structPtr, "HitBuffer");
      }

      rdcspv::Id uintPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));

      // get a pointer to buffer.hit_count
      rdcspv::Id hit_count =
          ops.add(rdcspv::OpAccessChain(uintPtr, editor.MakeId(), structPtr, {getUIntConst(0)}));

      // get a pointer to buffer.total_count
      rdcspv::Id total_count =
          ops.add(rdcspv::OpAccessChain(uintPtr, editor.MakeId(), structPtr, {getUIntConst(1)}));

      rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Device);
      rdcspv::Id semantics =
          editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::MemorySemantics::AcquireRelease);

      // increment total_count
      ops.add(rdcspv::OpAtomicIAdd(uint32Type, editor.MakeId(), total_count, scope, semantics,
                                   getUIntConst(1)));

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

      rdcspv::Id fragCoord_ddx =
          ops.add(rdcspv::OpDPdx(float4Type, editor.MakeId(), fragCoordLoaded));

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
          ops.add(rdcspv::OpAccessChain(hitptr, editor.MakeId(), structPtr, {getUIntConst(2), slot}));

      // store fixed properties

      rdcspv::Id storePtr =
          ops.add(rdcspv::OpAccessChain(float4BufPtr, editor.MakeId(), hit, {getUIntConst(0)}));
      ops.add(rdcspv::OpStore(storePtr, fragCoordLoaded, alignedAccess));

      rdcspv::Id loaded;
      if(primitiveID.base != rdcspv::Id())
      {
        if(primitiveID.member == ~0U)
        {
          loaded = ops.add(rdcspv::OpLoad(primitiveID.type, editor.MakeId(), primitiveID.base));
        }
        else
        {
          rdcspv::Id inPtrType =
              editor.DeclareType(rdcspv::Pointer(primitiveID.type, rdcspv::StorageClass::Input));

          rdcspv::Id posptr =
              ops.add(rdcspv::OpAccessChain(inPtrType, editor.MakeId(), primitiveID.base,
                                            {editor.AddConstantImmediate(primitiveID.member)}));
          loaded = ops.add(rdcspv::OpLoad(primitiveID.type, editor.MakeId(), posptr));
        }

        // if it was loaded as signed int by the shader and not as unsigned by us, bitcast to
        // unsigned.
        if(primitiveID.type != uint32Type)
          loaded = ops.add(rdcspv::OpBitcast(uint32Type, editor.MakeId(), loaded));
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
          loaded = ops.add(rdcspv::OpLoad(sampleIndex.type, editor.MakeId(), sampleIndex.base));
        }
        else
        {
          rdcspv::Id inPtrType =
              editor.DeclareType(rdcspv::Pointer(sampleIndex.type, rdcspv::StorageClass::Input));

          rdcspv::Id posptr =
              ops.add(rdcspv::OpAccessChain(inPtrType, editor.MakeId(), sampleIndex.base,
                                            {editor.AddConstantImmediate(sampleIndex.member)}));
          loaded = ops.add(rdcspv::OpLoad(sampleIndex.type, editor.MakeId(), posptr));
        }

        // if it was loaded as signed int by the shader and not as unsigned by us, bitcast to
        // unsigned.
        if(sampleIndex.type != uint32Type)
          loaded = ops.add(rdcspv::OpBitcast(uint32Type, editor.MakeId(), loaded));
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

      // store ddx(gl_FragCoord.x) to check that derivatives are working
      storePtr = ops.add(rdcspv::OpAccessChain(floatBufPtr, editor.MakeId(), hit, {getUIntConst(4)}));
      rdcspv::Id fragCoord_ddx_x =
          ops.add(rdcspv::OpCompositeExtract(floatType, editor.MakeId(), fragCoord_ddx, {0}));
      ops.add(rdcspv::OpStore(storePtr, fragCoord_ddx_x, alignedAccess));

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
          rdcspv::Id valueType = values[i].valueType;
          if(valueType == boolType)
            valueType = uint32Type;
          rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(valueType, bufferClass));

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

  rdcstr regionName =
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
  {
    RDCLOG("No drawcall selected");
    return new ShaderDebugTrace();
  }

  uint32_t vertOffset = 0, instOffset = 0;
  if(!(draw->flags & DrawFlags::Indexed))
    vertOffset = draw->vertexOffset;

  if(draw->flags & DrawFlags::Instanced)
    instOffset = draw->instanceOffset;

  // get ourselves in pristine state before this draw (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[0].module];
  rdcstr entryPoint = pipe.shaders[0].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[0].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  if(!shadRefl.refl.debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl.debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, VK_SHADER_STAGE_VERTEX_BIT, eventId);

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::BaseInstance] = ShaderVariable(rdcstr(), draw->instanceOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::BaseVertex] = ShaderVariable(
      rdcstr(), (draw->flags & DrawFlags::Indexed) ? draw->baseVertex : draw->vertexOffset, 0U, 0U,
      0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);
  if(draw->flags & DrawFlags::Indexed)
    builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), idx, 0U, 0U, 0U);
  else
    builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), vertid + vertOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::InstanceIndex] = ShaderVariable(rdcstr(), instid + instOffset, 0U, 0U, 0U);

  rdcarray<ShaderVariable> &locations = apiWrapper->location_inputs;
  for(const VulkanCreationInfo::Pipeline::Attribute &attr : pipe.vertexAttrs)
  {
    locations.resize_for_index(attr.location);

    if(Vulkan_Debug_ShaderDebugLogging())
      RDCLOG("Populating location %u", attr.location);

    ShaderValue &val = locations[attr.location].value;

    bytebuf data;

    size_t size = GetByteSize(1, 1, 1, attr.format, 0);

    bool found = false;

    for(const VulkanCreationInfo::Pipeline::Binding &bind : pipe.vertexBindings)
    {
      if(bind.vbufferBinding != attr.binding)
        continue;

      if(bind.vbufferBinding < state.vbuffers.size())
      {
        const VulkanRenderState::VertBuffer &vb = state.vbuffers[bind.vbufferBinding];

        if(vb.buf != ResourceId())
        {
          VkDeviceSize vertexOffset = 0;

          if(bind.perInstance)
          {
            if(bind.instanceDivisor == 0)
              vertexOffset = instOffset * vb.stride;
            else
              vertexOffset = (instOffset + (instid / bind.instanceDivisor)) * vb.stride;
          }
          else
          {
            vertexOffset = (idx + vertOffset) * vb.stride;
          }

          if(Vulkan_Debug_ShaderDebugLogging())
          {
            RDCLOG("Fetching from %s at %llu offset %zu bytes", ToStr(vb.buf).c_str(),
                   vb.offs + attr.byteoffset + vertexOffset, size);
          }

          if(attr.byteoffset + vertexOffset < vb.size)
            GetDebugManager()->GetBufferData(vb.buf, vb.offs + attr.byteoffset + vertexOffset, size,
                                             data);
        }
      }
      else if(Vulkan_Debug_ShaderDebugLogging())
      {
        RDCLOG("Vertex binding %u out of bounds from %zu vertex buffers", bind.vbufferBinding,
               state.vbuffers.size());
      }
    }

    if(!found)
    {
      if(Vulkan_Debug_ShaderDebugLogging())
      {
        RDCLOG("Attribute binding %u out of bounds from %zu bindings", attr.binding,
               pipe.vertexBindings.size());
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
      ResourceFormat fmt = MakeResourceFormat(attr.format);

      // integer formats need to be read as-is, rather than converted to floats
      if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt)
      {
        if(fmt.type == ResourceFormatType::R10G10B10A2)
        {
          // this is the only packed UINT format
          Vec4u decoded = ConvertFromR10G10B10A2UInt(*(uint32_t *)data.data());

          val.u.x = decoded.x;
          val.u.y = decoded.y;
          val.u.z = decoded.z;
          val.u.w = decoded.w;
        }
        else
        {
          for(uint32_t i = 0; i < fmt.compCount; i++)
          {
            const byte *src = data.data() + i * fmt.compByteWidth;
            if(fmt.compByteWidth == 8)
              memcpy(&val.u64v[i], src, fmt.compByteWidth);
            else
              memcpy(&val.uv[i], src, fmt.compByteWidth);
          }
        }
      }
      else
      {
        FloatVector decoded = DecodeFormattedComponents(fmt, data.data());

        val.f.x = decoded.x;
        val.f.y = decoded.y;
        val.f.z = decoded.z;
        val.f.w = decoded.w;
      }
    }
  }

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);
  apiWrapper->ResetReplay();

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

  if(!m_pDriver->GetDeviceEnabledFeatures().fragmentStoresAndAtomics)
  {
    RDCWARN("Pixel debugging is not supported without fragment stores");
    return new ShaderDebugTrace;
  }

  VkDevice dev = m_pDriver->GetDev();
  VkResult vkr = VK_SUCCESS;

  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  rdcstr regionName = StringFormat::Fmt("DebugPixel @ %u of (%u,%u) sample %u primitive %u",
                                        eventId, x, y, sample, primitive);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
  {
    RDCLOG("No drawcall selected");
    return new ShaderDebugTrace();
  }

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];

  if(pipe.shaders[4].module == ResourceId())
  {
    RDCLOG("No pixel shader bound at draw");
    return new ShaderDebugTrace();
  }

  // get ourselves in pristine state before this draw (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[4].module];
  rdcstr entryPoint = pipe.shaders[4].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[4].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  if(!shadRefl.refl.debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl.debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, VK_SHADER_STAGE_FRAGMENT_BIT, eventId);

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);
  builtins[ShaderBuiltin::Position] = ShaderVariable(rdcstr(), x, y, 0U, 0U);

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
        if(Vulkan_Debug_ShaderDebugLogging())
        {
          RDCLOG("Geometry shader exports primitive ID, can use");
        }

        usePrimitiveID = true;
        break;
      }
    }

    if(Vulkan_Debug_ShaderDebugLogging())
    {
      if(!usePrimitiveID)
        RDCLOG("Geometry shader doesn't export primitive ID, can't use");
    }
  }
  else
  {
    // no geometry shader - safe to use as long as the geometry shader capability is available
    usePrimitiveID = m_pDriver->GetDeviceEnabledFeatures().geometryShader != VK_FALSE;

    if(Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("usePrimitiveID is %u because of bare capability", usePrimitiveID);
    }
  }

  bool useSampleID = m_pDriver->GetDeviceEnabledFeatures().sampleRateShading != VK_FALSE;

  if(Vulkan_Debug_ShaderDebugLogging())
  {
    RDCLOG("useSampleID is %u because of bare capability", useSampleID);
  }

  StorageMode storageMode = Binding;

  if(m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address)
  {
    storageMode = KHR_bda;

    if(Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("Using KHR_buffer_device_address");
    }
  }
  else if(m_pDriver->GetExtensions(NULL).ext_EXT_buffer_device_address)
  {
    if(m_pDriver->GetDeviceEnabledFeatures().shaderInt64)
    {
      storageMode = EXT_bda;

      if(Vulkan_Debug_ShaderDebugLogging())
      {
        RDCLOG("Using EXT_buffer_device_address");
      }
    }
    else if(Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG(
          "EXT_buffer_device_address is available but shaderInt64 isn't, falling back to binding "
          "storage mode");
    }
  }

  if(Vulkan_Debug_DisableBufferDeviceAddress())
    storageMode = Binding;

  rdcarray<uint32_t> fragspv = shader.spirv.GetSPIRV();

  if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_psinput_before.spv", fragspv);

  uint32_t structStride = 0;
  CreatePSInputFetcher(fragspv, structStride, shadRefl, storageMode, usePrimitiveID, useSampleID);

  if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_psinput_after.spv", fragspv);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  VkGraphicsPipelineCreateInfo graphicsInfo = {};

  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(graphicsInfo, state.graphics.pipeline);

  // struct size is PSHit header plus 5x structStride = base, ddxcoarse, ddycoarse, ddxfine, ddyfine
  uint32_t structSize = sizeof(PSHit) + structStride * 5;

  VkDeviceSize feedbackStorageSize = overdrawLevels * structSize + sizeof(Vec4f) + 1024;

  if(Vulkan_Debug_ShaderDebugLogging())
  {
    RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
           feedbackStorageSize);
  }

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
  } specData = {};

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

    if(Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("Got buffer address of %llu", specData.bufferAddress);
    }
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

    // if the pool failed due to limits, it will be NULL so bail now
    if(descpool == VK_NULL_HANDLE)
    {
      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = ShaderStage::Pixel;

      return ret;
    }

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
          (uint32_t)InputSpecConstant::Address, offsetof(SpecData, bufferAddress), sizeof(uint32_t),
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
      {
          (uint32_t)InputSpecConstant::AddressMSB, offsetof(SpecData, bufferAddress) + 4,
          sizeof(uint32_t),
      },
  };

  VkSpecializationInfo specInfo = {};
  specInfo.dataSize = sizeof(specData);
  specInfo.pData = &specData;
  specInfo.mapEntryCount = ARRAY_COUNT(specMaps);
  specInfo.pMapEntries = specMaps;

  RDCCOMPILE_ASSERT((size_t)InputSpecConstant::Count == ARRAY_COUNT(specMaps),
                    "Spec constants changed");

  if(storageMode == EXT_bda)
  {
    // don't pass AddressMSB for EXT_buffer_device_address, we pass a uint64
    specInfo.mapEntryCount--;
    specMaps[0].size = sizeof(SpecData::bufferAddress);
  }

  rdcarray<VkShaderModule> modules;

  for(uint32_t i = 0; i < graphicsInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &stage =
        (VkPipelineShaderStageCreateInfo &)graphicsInfo.pStages[i];

    if(stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
      moduleCreateInfo.pCode = fragspv.data();
      moduleCreateInfo.codeSize = fragspv.size() * sizeof(uint32_t);

      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &stage.module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      stage.pSpecializationInfo = &specInfo;

      modules.push_back(stage.module);
    }
    else if(storageMode == Binding)
    {
      // if we're stealing a binding point, we need to patch all other shaders
      rdcarray<uint32_t> spirv = c.m_ShaderModule[GetResID(stage.module)].spirv.GetSPIRV();

      {
        rdcspv::Editor editor(spirv);

        editor.Prepare();

        // patch all bindings up by 1
        for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                         end = editor.End(rdcspv::Section::Annotations);
            it < end; ++it)
        {
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
      }

      moduleCreateInfo.pCode = spirv.data();
      moduleCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);

      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &stage.module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      modules.push_back(stage.module);
    }
  }

  // we don't use a pipeline cache here because our spec constants will cause failures often and
  // bloat the cache. Even if we avoided the high-frequency x/y and stored them e.g. in the feedback
  // buffer, we'd still want to spec-constant the address when possible so we're always going to
  // have some varying value.
  VkPipeline inputsPipe;
  vkr = m_pDriver->vkCreateGraphicsPipelines(dev, NULL, 1, &graphicsInfo, NULL, &inputsPipe);
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
  uint32_t numHits = ((uint32_t *)base)[0];
  uint32_t totalHits = ((uint32_t *)base)[1];

  if(numHits > overdrawLevels)
  {
    RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", numHits, overdrawLevels);
    numHits = overdrawLevels;
  }

  base += sizeof(Vec4f);

  PSHit *winner = NULL;

  RDCLOG("Got %u hit candidates out of %u total instances", numHits, totalHits);

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

  VkCompareOp depthOp = state.depthCompareOp;

  // depth tests disabled acts the same as always compare mode
  if(!state.depthTestEnable)
    depthOp = VK_COMPARE_OP_ALWAYS;

  for(uint32_t i = 0; i < numHits; i++)
  {
    PSHit *hit = (PSHit *)(base + structSize * i);

    if(hit->valid != validMagicNumber)
    {
      RDCWARN("Hit %u doesn't have valid magic number", i);
      continue;
    }

    if(hit->ddxDerivCheck != 1.0f)
    {
      RDCWARN("Hit %u doesn't have valid derivatives", i);
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

      uint32_t comp = Bits::CountTrailingZeroes(uint32_t(param.regChannelMask));

      const size_t sz = sizeof(Vec4f) - sizeof(uint32_t) * comp;

      memcpy(&var.value.uv[comp], &value[i], sz);
      memcpy(&deriv.ddxcoarse.fv[comp], &ddxcoarse[i], sz);
      memcpy(&deriv.ddycoarse.fv[comp], &ddycoarse[i], sz);
      memcpy(&deriv.ddxfine.fv[comp], &ddxfine[i], sz);
      memcpy(&deriv.ddyfine.fv[comp], &ddyfine[i], sz);
    }

    ret = debugger->BeginDebug(apiWrapper, ShaderStage::Pixel, entryPoint, spec,
                               shadRefl.instructionLines, shadRefl.patchData, destIdx);
    apiWrapper->ResetReplay();
  }
  else
  {
    RDCLOG("Didn't get any valid hit to debug");
    delete apiWrapper;

    ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Pixel;
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

  // delete shader modules
  for(VkShaderModule s : modules)
    m_pDriver->vkDestroyShaderModule(dev, s, NULL);

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

  rdcstr regionName =
      StringFormat::Fmt("DebugThread @ %u of (%u,%u,%u) (%u,%u,%u)", eventId, groupid[0],
                        groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Dispatch))
  {
    RDCLOG("No dispatch selected");
    return new ShaderDebugTrace();
  }

  // get ourselves in pristine state before this dispatch (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.compute.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[5].module];
  rdcstr entryPoint = pipe.shaders[5].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[5].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.compute.pipeline);

  if(!shadRefl.refl.debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl.debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, VK_SHADER_STAGE_COMPUTE_BIT, eventId);

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
  apiWrapper->ResetReplay();

  return ret;
}

rdcarray<ShaderDebugState> VulkanReplay::ContinueDebug(ShaderDebugger *debugger)
{
  rdcspv::Debugger *spvDebugger = (rdcspv::Debugger *)debugger;

  if(!spvDebugger)
    return {};

  VkMarkerRegion region("ContinueDebug Simulation Loop");

  for(size_t fmt = 0; fmt < ARRAY_COUNT(m_TexRender.DummyImageViews); fmt++)
  {
    for(size_t dim = 0; dim < ARRAY_COUNT(m_TexRender.DummyImageViews[0]); dim++)
    {
      m_ShaderDebugData.DummyImageInfos[fmt][dim].imageLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      m_ShaderDebugData.DummyImageInfos[fmt][dim].imageView =
          Unwrap(m_TexRender.DummyImageViews[fmt][dim]);

      m_ShaderDebugData.DummyWrites[fmt][dim].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      m_ShaderDebugData.DummyWrites[fmt][dim].descriptorCount = 1;
      m_ShaderDebugData.DummyWrites[fmt][dim].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      m_ShaderDebugData.DummyWrites[fmt][dim].dstBinding = uint32_t(dim + 1);
      m_ShaderDebugData.DummyWrites[fmt][dim].dstSet = Unwrap(m_ShaderDebugData.DescSet);
      m_ShaderDebugData.DummyWrites[fmt][dim].pImageInfo =
          &m_ShaderDebugData.DummyImageInfos[fmt][dim];
    }

    m_ShaderDebugData.DummyImageInfos[fmt][5].sampler = Unwrap(m_TexRender.DummySampler);

    m_ShaderDebugData.DummyWrites[fmt][5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    m_ShaderDebugData.DummyWrites[fmt][5].descriptorCount = 1;
    m_ShaderDebugData.DummyWrites[fmt][5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    m_ShaderDebugData.DummyWrites[fmt][5].dstBinding = (uint32_t)ShaderDebugBind::Sampler;
    m_ShaderDebugData.DummyWrites[fmt][5].dstSet = Unwrap(m_ShaderDebugData.DescSet);
    m_ShaderDebugData.DummyWrites[fmt][5].pImageInfo = &m_ShaderDebugData.DummyImageInfos[fmt][5];

    m_ShaderDebugData.DummyWrites[fmt][6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    m_ShaderDebugData.DummyWrites[fmt][6].descriptorCount = 1;
    m_ShaderDebugData.DummyWrites[fmt][6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    m_ShaderDebugData.DummyWrites[fmt][6].dstBinding = (uint32_t)ShaderDebugBind::Buffer;
    m_ShaderDebugData.DummyWrites[fmt][6].dstSet = Unwrap(m_ShaderDebugData.DescSet);
    m_ShaderDebugData.DummyWrites[fmt][6].pTexelBufferView =
        UnwrapPtr(m_TexRender.DummyBufferView[fmt]);
  }

  rdcarray<ShaderDebugState> ret = spvDebugger->ContinueDebug();

  VulkanAPIWrapper *api = (VulkanAPIWrapper *)spvDebugger->GetAPIWrapper();
  api->ResetReplay();

  return ret;
}

void VulkanReplay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
}
