/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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
#include "driver/shaders/spirv/var_dispatch_helpers.h"
#include "maths/formatpacking.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

#undef None

RDOC_CONFIG(rdcstr, Vulkan_Debug_PSDebugDumpDirPath, "",
            "Path to dump pixel shader debugging generated SPIR-V files.");
RDOC_CONFIG(bool, Vulkan_Debug_DisableBufferDeviceAddress, false,
            "Disable use of buffer device address for PS Input fetch.");
RDOC_CONFIG(bool, Vulkan_Debug_ShaderDebugLogging, false,
            "Output verbose debug logging messages when debugging shaders.");

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
  MathResult = 9,
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
  union
  {
    GatherOffsets gatherOffsets;
    Vec3i constOffsets;
  };

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
  float uvwa[4];
  float ddx[3];
  float ddy[3];
  Vec3i offset;
  int sampleIdx;
  float compare;
  float lod;
  float minlod;
};

class VulkanAPIWrapper : public rdcspv::DebugAPIWrapper
{
public:
  VulkanAPIWrapper(WrappedVulkan *vk, VulkanCreationInfo &creation, ShaderStage stage, uint32_t eid,
                   ResourceId shadId)
      : m_DebugData(vk->GetReplay()->GetShaderDebugData()),
        m_Creation(creation),
        m_EventID(eid),
        m_ShaderID(shadId)
  {
    m_pDriver = vk;

    // when we're first setting up, the state is pristine and no replay is needed
    m_ResourcesDirty = false;

    VulkanReplay *replay = m_pDriver->GetReplay();

    // cache the descriptor access. This should be a superset of all descriptors we need to read from
    m_Access = replay->GetDescriptorAccess(eid);

    // filter to only accesses from the stage we care about, as access lookups will be stage-specific
    m_Access.removeIf([stage](const DescriptorAccess &access) { return access.stage != stage; });

    // fetch all descriptor contents now too
    m_Descriptors.reserve(m_Access.size());
    m_SamplerDescriptors.reserve(m_Access.size());

    // we could collate ranges by descriptor store, but in practice we don't expect descriptors to
    // be scattered across multiple stores. So to keep the code simple for now we do a linear sweep
    ResourceId store;
    rdcarray<DescriptorRange> ranges;

    for(const DescriptorAccess &acc : m_Access)
    {
      if(acc.descriptorStore != store)
      {
        if(store != ResourceId())
        {
          m_Descriptors.append(replay->GetDescriptors(store, ranges));
          m_SamplerDescriptors.append(replay->GetSamplerDescriptors(store, ranges));
        }

        store = replay->GetLiveID(acc.descriptorStore);
        ranges.clear();
      }

      // if the last range is contiguous with this access, append this access as a new range to query
      if(!ranges.empty() && ranges.back().descriptorSize == acc.byteSize &&
         ranges.back().offset + ranges.back().descriptorSize == acc.byteOffset)
      {
        ranges.back().count++;
        continue;
      }

      DescriptorRange range;
      range.offset = acc.byteOffset;
      range.descriptorSize = acc.byteSize;
      ranges.push_back(range);
    }

    if(store != ResourceId())
    {
      m_Descriptors.append(replay->GetDescriptors(store, ranges));
      m_SamplerDescriptors.append(replay->GetSamplerDescriptors(store, ranges));
    }

    // apply dynamic offsets to our cached descriptors
    // we iterate over descriptors first to find dynamic ones, then iterate over our cached set to
    // apply. Neither array should be large but there should be fewer dynamic descriptors in total
    {
      const VulkanRenderState &state = m_pDriver->GetRenderState();

      const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> *srcs[] = {
          &state.graphics.descSets,
          &state.compute.descSets,
      };

      for(size_t p = 0; p < ARRAY_COUNT(srcs); p++)
      {
        for(size_t i = 0; i < srcs[p]->size(); i++)
        {
          const VulkanStatePipeline::DescriptorAndOffsets &srcData = srcs[p]->at(i);
          ResourceId sourceSet = srcData.descSet;
          const uint32_t *srcOffset = srcData.offsets.begin();

          if(sourceSet == ResourceId())
            continue;

          const VulkanCreationInfo::PipelineLayout &pipeLayoutInfo =
              m_Creation.m_PipelineLayout[srcData.pipeLayout];

          ResourceId setOrig = m_pDriver->GetResourceManager()->GetOriginalID(sourceSet);

          const BindingStorage &bindStorage =
              m_pDriver->GetCurrentDescSetBindingStorage(srcData.descSet);
          const DescriptorSetSlot *first = bindStorage.binds.empty() ? NULL : bindStorage.binds[0];
          for(size_t b = 0; b < bindStorage.binds.size(); b++)
          {
            const DescSetLayout::Binding &layoutBind =
                m_Creation.m_DescSetLayout[pipeLayoutInfo.descSetLayouts[i]].bindings[b];

            if(layoutBind.layoutDescType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
               layoutBind.layoutDescType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
              continue;

            uint64_t descriptorByteOffset = bindStorage.binds[b] - first;

            // inline UBOs aren't dynamic and variable size can't be used with dynamic buffers, so
            // the count is what it is at definition time
            for(uint32_t a = 0; a < layoutBind.descriptorCount; a++)
            {
              uint32_t dynamicBufferByteOffset = *srcOffset;
              srcOffset++;

              for(size_t accIdx = 0; accIdx < m_Access.size(); accIdx++)
              {
                if(m_Access[accIdx].descriptorStore == setOrig &&
                   m_Access[accIdx].byteOffset == descriptorByteOffset + a)
                {
                  m_Descriptors[accIdx].byteOffset += dynamicBufferByteOffset;
                  break;
                }
              }
            }
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
      // replay the action to get back to 'normal' state for this event, and mark that we need to
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

  virtual ResourceId GetShaderID() override { return m_ShaderID; }

  virtual uint64_t GetBufferLength(ShaderBindIndex bind) override
  {
    return PopulateBuffer(bind).size();
  }

  virtual void ReadBufferValue(ShaderBindIndex bind, uint64_t offset, uint64_t byteSize,
                               void *dst) override
  {
    const bytebuf &data = PopulateBuffer(bind);

    if(offset + byteSize <= data.size())
      memcpy(dst, data.data() + (size_t)offset, (size_t)byteSize);
  }

  virtual void WriteBufferValue(ShaderBindIndex bind, uint64_t offset, uint64_t byteSize,
                                const void *src) override
  {
    bytebuf &data = PopulateBuffer(bind);

    if(offset + byteSize <= data.size())
      memcpy(data.data() + (size_t)offset, src, (size_t)byteSize);
  }

  virtual void ReadAddress(uint64_t address, uint64_t byteSize, void *dst) override
  {
    size_t offset;
    const bytebuf &data = PopulateBuffer(address, offset);
    if(offset + byteSize <= data.size())
      memcpy(dst, data.data() + offset, (size_t)byteSize);
  }

  virtual void WriteAddress(uint64_t address, uint64_t byteSize, const void *src) override
  {
    size_t offset;
    bytebuf &data = PopulateBuffer(address, offset);
    if(offset + byteSize <= data.size())
      memcpy(data.data() + offset, src, (size_t)byteSize);
  }

  virtual bool ReadTexel(ShaderBindIndex imageBind, const ShaderVariable &coord, uint32_t sample,
                         ShaderVariable &output) override
  {
    ImageData &data = PopulateImage(imageBind);

    if(data.width == 0)
      return false;

    uint32_t coords[4];
    for(int i = 0; i < 4; i++)
      coords[i] = uintComp(coord, i);

    if(coords[0] > data.width || coords[1] > data.height || coords[2] > data.depth)
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Out of bounds access to image, coord %u,%u,%u outside of dimensions %ux%ux%u",
              coords[0], coords[1], coords[2], data.width, data.height, data.depth));
      return false;
    }

    CompType varComp = VarTypeCompType(output.type);

    set0001(output);

    ShaderVariable input;
    input.columns = data.fmt.compCount;

    if(data.fmt.compType == CompType::UInt)
    {
      RDCASSERT(varComp == CompType::UInt, varComp);

      // set up input type for proper expansion below
      if(data.fmt.compByteWidth == 1)
        input.type = VarType::UByte;
      else if(data.fmt.compByteWidth == 2)
        input.type = VarType::UShort;
      else if(data.fmt.compByteWidth == 4)
        input.type = VarType::UInt;
      else if(data.fmt.compByteWidth == 8)
        input.type = VarType::ULong;

      memcpy(input.value.u8v.data(), data.texel(coords, sample), data.texelSize);

      for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        setUintComp(output, c, uintComp(input, c));
    }
    else if(data.fmt.compType == CompType::SInt)
    {
      RDCASSERT(varComp == CompType::SInt, varComp);

      // set up input type for proper expansion below
      if(data.fmt.compByteWidth == 1)
        input.type = VarType::SByte;
      else if(data.fmt.compByteWidth == 2)
        input.type = VarType::SShort;
      else if(data.fmt.compByteWidth == 4)
        input.type = VarType::SInt;
      else if(data.fmt.compByteWidth == 8)
        input.type = VarType::SLong;

      memcpy(input.value.u8v.data(), data.texel(coords, sample), data.texelSize);

      for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        setIntComp(output, c, intComp(input, c));
    }
    else
    {
      RDCASSERT(varComp == CompType::Float, varComp);

      // do the decode of whatever unorm/float/etc the format is
      FloatVector v = DecodeFormattedComponents(data.fmt, data.texel(coords, sample));

      // set it into f32v
      input.value.f32v[0] = v.x;
      input.value.f32v[1] = v.y;
      input.value.f32v[2] = v.z;
      input.value.f32v[3] = v.w;

      // read as floats
      input.type = VarType::Float;

      for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        setFloatComp(output, c, input.value.f32v[c]);
    }

    return true;
  }

  virtual bool WriteTexel(ShaderBindIndex imageBind, const ShaderVariable &coord, uint32_t sample,
                          const ShaderVariable &input) override
  {
    ImageData &data = PopulateImage(imageBind);

    if(data.width == 0)
      return false;

    uint32_t coords[4];
    for(int i = 0; i < 4; i++)
      coords[i] = uintComp(coord, i);

    if(coords[0] > data.width || coords[1] > data.height || coords[2] > data.depth)
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Out of bounds access to image, coord %u,%u,%u outside of dimensions %ux%ux%u",
              coords[0], coords[1], coords[2], data.width, data.height, data.depth));
      return false;
    }

    CompType varComp = VarTypeCompType(input.type);

    ShaderVariable output;
    output.columns = data.fmt.compCount;

    if(data.fmt.compType == CompType::UInt)
    {
      RDCASSERT(varComp == CompType::UInt, varComp);

      // set up output type for proper expansion below
      if(data.fmt.compByteWidth == 1)
        output.type = VarType::UByte;
      else if(data.fmt.compByteWidth == 2)
        output.type = VarType::UShort;
      else if(data.fmt.compByteWidth == 4)
        output.type = VarType::UInt;
      else if(data.fmt.compByteWidth == 8)
        output.type = VarType::ULong;

      for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        setUintComp(output, c, uintComp(input, c));

      memcpy(data.texel(coords, sample), output.value.u8v.data(), data.texelSize);
    }
    else if(data.fmt.compType == CompType::SInt)
    {
      RDCASSERT(varComp == CompType::SInt, varComp);

      // set up input type for proper expansion below
      if(data.fmt.compByteWidth == 1)
        output.type = VarType::SByte;
      else if(data.fmt.compByteWidth == 2)
        output.type = VarType::SShort;
      else if(data.fmt.compByteWidth == 4)
        output.type = VarType::SInt;
      else if(data.fmt.compByteWidth == 8)
        output.type = VarType::SLong;

      for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        setIntComp(output, c, intComp(input, c));

      memcpy(data.texel(coords, sample), output.value.u8v.data(), data.texelSize);
    }
    else
    {
      RDCASSERT(varComp == CompType::Float, varComp);

      // read as floats
      output.type = VarType::Float;

      for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        setFloatComp(output, c, input.value.f32v[c]);

      FloatVector v;

      // set it into f32v
      v.x = input.value.f32v[0];
      v.y = input.value.f32v[1];
      v.z = input.value.f32v[2];
      v.w = input.value.f32v[3];

      EncodeFormattedComponents(data.fmt, v, data.texel(coords, sample));
    }

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
      if(var.rows == 1)
      {
        if(component + var.columns > 4)
          RDCERR("Unexpected component %u for column count %u", component, var.columns);

        for(uint8_t c = 0; c < var.columns; c++)
          copyComp(var, c, location_inputs[location], component + c);
      }
      else
      {
        RDCASSERTEQUAL(component, 0);
        for(uint8_t r = 0; r < var.rows; r++)
          for(uint8_t c = 0; c < var.columns; c++)
            copyComp(var, r * var.columns + c, location_inputs[location + c], r);
      }
      return;
    }

    RDCERR("Couldn't get input for %s at location=%u, component=%u", var.name.c_str(), location,
           component);
  }

  virtual DerivativeDeltas GetDerivative(ShaderBuiltin builtin, uint32_t location,
                                         uint32_t component, VarType type) override
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

      ret.ddxcoarse.type = type;
      ret.ddxfine.type = type;
      ret.ddycoarse.type = type;
      ret.ddyfine.type = type;

      RDCASSERT(component < 4, component);

      // rebase from component into [0]..

      for(uint32_t src = component, dst = 0; src < 4; src++, dst++)
      {
        copyComp(ret.ddxcoarse, dst, deriv.ddxcoarse, src);
        copyComp(ret.ddxfine, dst, deriv.ddxfine, src);
        copyComp(ret.ddycoarse, dst, deriv.ddycoarse, src);
        copyComp(ret.ddyfine, dst, deriv.ddyfine, src);
      }

      return ret;
    }

    RDCERR("Couldn't get derivative for location=%u, component=%u", location, component);
    return DerivativeDeltas();
  }

  bool CalculateSampleGather(rdcspv::ThreadState &lane, rdcspv::Op opcode,
                             DebugAPIWrapper::TextureType texType, ShaderBindIndex imageBind,
                             ShaderBindIndex samplerBind, const ShaderVariable &uv,
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
    const Descriptor &imageDescriptor = buffer ? GetDescriptor(access, ShaderBindIndex(), valid)
                                               : GetDescriptor(access, imageBind, valid);
    const Descriptor &bufferViewDescriptor = buffer
                                                 ? GetDescriptor(access, imageBind, valid)
                                                 : GetDescriptor(access, ShaderBindIndex(), valid);

    // fetch the sampler (if there's no sampler, this will silently return dummy data without
    // marking invalid
    const SamplerDescriptor &samplerDescriptor = GetSamplerDescriptor(access, samplerBind, valid);

    // if any descriptor lookup failed, return now
    if(!valid)
      return false;

    VkMarkerRegion markerRegion("CalculateSampleGather");

    VkBufferView bufferView =
        m_pDriver->GetResourceManager()->GetLiveHandle<VkBufferView>(bufferViewDescriptor.view);

    VkSampler sampler =
        m_pDriver->GetResourceManager()->GetLiveHandle<VkSampler>(samplerDescriptor.object);
    VkImageView view =
        m_pDriver->GetResourceManager()->GetLiveHandle<VkImageView>(imageDescriptor.view);
    VkImageLayout layout = convert((DescriptorSlotImageLayout)imageDescriptor.byteOffset);

    // promote view to Array view

    const VulkanCreationInfo::ImageView &viewProps = m_Creation.m_ImageView[GetResID(view)];
    const VulkanCreationInfo::Image &imageProps = m_Creation.m_Image[viewProps.image];

    const bool depthTex = IsDepthOrStencilFormat(viewProps.format);

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
        output.value.u32v[0] = viewProps.range.levelCount;
        if(viewProps.range.levelCount == VK_REMAINING_MIP_LEVELS)
          output.value.u32v[0] = imageProps.mipLevels - viewProps.range.baseMipLevel;
        return true;
      }
      case rdcspv::Op::ImageQuerySamples:
      {
        output.value.u32v[0] = (uint32_t)imageProps.samples;
        return true;
      }
      case rdcspv::Op::ImageQuerySize:
      case rdcspv::Op::ImageQuerySizeLod:
      {
        uint32_t mip = viewProps.range.baseMipLevel;

        if(opcode == rdcspv::Op::ImageQuerySizeLod)
          mip += uintComp(lane.GetSrc(operands.lod), 0);

        RDCEraseEl(output.value);

        int i = 0;
        setUintComp(output, i++, RDCMAX(1U, imageProps.extent.width >> mip));
        if(coords >= 2)
          setUintComp(output, i++, RDCMAX(1U, imageProps.extent.height >> mip));
        if(viewProps.viewType == VK_IMAGE_VIEW_TYPE_3D)
          setUintComp(output, i++, RDCMAX(1U, imageProps.extent.depth >> mip));

        if(viewProps.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY ||
           viewProps.viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
          setUintComp(output, i++, imageProps.arrayLayers);
        else if(viewProps.viewType == VK_IMAGE_VIEW_TYPE_CUBE ||
                viewProps.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
          setUintComp(output, i++, imageProps.arrayLayers / 6);

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

          setUintComp(output, 0, uint32_t(size / GetByteSize(1, 1, 1, bufViewProps.format, 0)));
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
      m_pDriver->CheckVkResult(vkr);

      m_SampleViews[GetResID(view)] = sampleView;
    }

    if(operands.flags & rdcspv::ImageOperands::Bias)
    {
      const ShaderVariable &biasVar = lane.GetSrc(operands.bias);

      // silently cast parameters to 32-bit floats
      float bias = floatComp(biasVar, 0);

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
          m_pDriver->CheckVkResult(vkr);

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

    bool gatherOp = false;

    switch(opcode)
    {
      case rdcspv::Op::ImageFetch:
      {
        // co-ordinates after the used ones are read as 0s. This allows us to then read an implicit
        // 0 for array layer when we promote accesses to arrays.
        uniformParams.texel_uvw.x = uintComp(uv, 0);
        if(coords >= 2)
          uniformParams.texel_uvw.y = uintComp(uv, 1);
        if(coords >= 3)
          uniformParams.texel_uvw.z = uintComp(uv, 2);

        if(!buffer && operands.flags & rdcspv::ImageOperands::Lod)
          uniformParams.texel_lod = uintComp(lane.GetSrc(operands.lod), 0);
        else
          uniformParams.texel_lod = 0;

        if(operands.flags & rdcspv::ImageOperands::Sample)
          uniformParams.sampleIdx = uintComp(lane.GetSrc(operands.sample), 0);

        break;
      }
      case rdcspv::Op::ImageGather:
      case rdcspv::Op::ImageDrefGather:
      {
        gatherOp = true;

        // silently cast parameters to 32-bit floats
        for(int i = 0; i < coords; i++)
          uniformParams.uvwa[i] = floatComp(uv, i);

        if(useCompare)
          uniformParams.compare = floatComp(compare, 0);

        constParams.gatherChannel = gatherChannel;

        if(operands.flags & rdcspv::ImageOperands::ConstOffsets)
        {
          ShaderVariable constOffsets = lane.GetSrc(operands.constOffsets);

          constParams.useGradOrGatherOffsets = VK_TRUE;

          // should be an array of ivec2
          RDCASSERT(constOffsets.members.size() == 4);

          // sign extend variables lower than 32-bits
          for(int i = 0; i < 4; i++)
          {
            if(constOffsets.members[i].type == VarType::SByte)
            {
              constOffsets.members[i].value.s32v[0] = constOffsets.members[i].value.s8v[0];
              constOffsets.members[i].value.s32v[1] = constOffsets.members[i].value.s8v[1];
            }
            else if(constOffsets.members[i].type == VarType::SShort)
            {
              constOffsets.members[i].value.s32v[0] = constOffsets.members[i].value.s16v[0];
              constOffsets.members[i].value.s32v[1] = constOffsets.members[i].value.s16v[1];
            }
          }

          constParams.gatherOffsets.u0 = constOffsets.members[0].value.s32v[0];
          constParams.gatherOffsets.v0 = constOffsets.members[0].value.s32v[1];
          constParams.gatherOffsets.u1 = constOffsets.members[1].value.s32v[0];
          constParams.gatherOffsets.v1 = constOffsets.members[1].value.s32v[1];
          constParams.gatherOffsets.u2 = constOffsets.members[2].value.s32v[0];
          constParams.gatherOffsets.v2 = constOffsets.members[2].value.s32v[1];
          constParams.gatherOffsets.u3 = constOffsets.members[3].value.s32v[0];
          constParams.gatherOffsets.v3 = constOffsets.members[3].value.s32v[1];
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
        // silently cast parameters to 32-bit floats
        for(int i = 0; i < coords; i++)
          uniformParams.uvwa[i] = floatComp(uv, i);

        if(proj)
        {
          // coords shouldn't be 4 because that's only valid for cube arrays which can't be
          // projected
          RDCASSERT(coords < 4);

          // do the divide ourselves rather than severely complicating the sample shader (as proj
          // variants need non-arrayed textures)
          float q = floatComp(uv, coords);

          uniformParams.uvwa[0] /= q;
          uniformParams.uvwa[1] /= q;
          uniformParams.uvwa[2] /= q;
        }

        if(operands.flags & rdcspv::ImageOperands::MinLod)
        {
          const ShaderVariable &minLodVar = lane.GetSrc(operands.minLod);

          // silently cast parameters to 32-bit floats
          uniformParams.minlod = floatComp(minLodVar, 0);
        }

        if(useCompare)
        {
          // silently cast parameters to 32-bit floats
          uniformParams.compare = floatComp(compare, 0);
        }

        if(operands.flags & rdcspv::ImageOperands::Lod)
        {
          const ShaderVariable &lodVar = lane.GetSrc(operands.lod);

          // silently cast parameters to 32-bit floats
          uniformParams.lod = floatComp(lodVar, 0);
          constParams.useGradOrGatherOffsets = VK_FALSE;
        }
        else if(operands.flags & rdcspv::ImageOperands::Grad)
        {
          ShaderVariable ddx = lane.GetSrc(operands.grad.first);
          ShaderVariable ddy = lane.GetSrc(operands.grad.second);

          constParams.useGradOrGatherOffsets = VK_TRUE;

          // silently cast parameters to 32-bit floats
          RDCASSERTEQUAL(ddx.type, ddy.type);
          for(int i = 0; i < gradCoords; i++)
          {
            uniformParams.ddx[i] = floatComp(ddx, i);
            uniformParams.ddy[i] = floatComp(ddy, i);
          }
        }

        if(opcode == rdcspv::Op::ImageSampleImplicitLod ||
           opcode == rdcspv::Op::ImageSampleProjImplicitLod || opcode == rdcspv::Op::ImageQueryLod)
        {
          // use grad to sub in for the implicit lod
          constParams.useGradOrGatherOffsets = VK_TRUE;

          // silently cast parameters to 32-bit floats
          RDCASSERTEQUAL(ddxCalc.type, ddyCalc.type);
          for(int i = 0; i < gradCoords; i++)
          {
            uniformParams.ddx[i] = floatComp(ddxCalc, i);
            uniformParams.ddy[i] = floatComp(ddyCalc, i);
          }
        }

        break;
      }
      default:
      {
        RDCERR("Unsupported opcode %s", ToStr(opcode).c_str());
        return false;
      }
    }

    if(operands.flags & rdcspv::ImageOperands::ConstOffset)
    {
      ShaderVariable constOffset = lane.GetSrc(operands.constOffset);

      // sign extend variables lower than 32-bits
      for(uint8_t c = 0; c < constOffset.columns; c++)
      {
        if(constOffset.type == VarType::SByte)
          constOffset.value.s32v[c] = constOffset.value.s8v[c];
        else if(constOffset.type == VarType::SShort)
          constOffset.value.s32v[c] = constOffset.value.s16v[c];
      }

      // pass offsets as uniform where possible - when the feature (widely available) on gather
      // operations. On non-gather operations we are forced to use const offsets and must specialise
      // the pipeline.
      if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended && gatherOp)
      {
        uniformParams.offset.x = constOffset.value.s32v[0];
        if(gradCoords >= 2)
          uniformParams.offset.y = constOffset.value.s32v[1];
        if(gradCoords >= 3)
          uniformParams.offset.z = constOffset.value.s32v[2];
      }
      else
      {
        constParams.constOffsets.x = constOffset.value.s32v[0];
        if(gradCoords >= 2)
          constParams.constOffsets.y = constOffset.value.s32v[1];
        if(gradCoords >= 3)
          constParams.constOffsets.z = constOffset.value.s32v[2];
      }
    }
    else if(operands.flags & rdcspv::ImageOperands::Offset)
    {
      ShaderVariable offset = lane.GetSrc(operands.offset);

      // sign extend variables lower than 32-bits
      for(uint8_t c = 0; c < offset.columns; c++)
      {
        if(offset.type == VarType::SByte)
          offset.value.s32v[c] = offset.value.s8v[c];
        else if(offset.type == VarType::SShort)
          offset.value.s32v[c] = offset.value.s16v[c];
      }

      // if the app's shader used a dynamic offset, we can too!
      uniformParams.offset.x = offset.value.s32v[0];
      if(gradCoords >= 2)
        uniformParams.offset.y = offset.value.s32v[1];
      if(gradCoords >= 3)
        uniformParams.offset.z = offset.value.s32v[2];
    }

    if(!m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
       (uniformParams.offset.x != 0 || uniformParams.offset.y != 0 || uniformParams.offset.z != 0))
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Use of constant offsets %d/%d/%d is not supported without "
                            "shaderImageGatherExtended device feature",
                            uniformParams.offset.x, uniformParams.offset.y, uniformParams.offset.z));
    }

    VkPipeline pipe = MakePipe(constParams, 32, depthTex, uintTex, sintTex);

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
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            Unwrap(m_DebugData.DescSet),
            (uint32_t)ShaderDebugBind::Constants,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            NULL,
            &uniformWriteInfo,
            NULL,
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            Unwrap(m_DebugData.DescSet),
            (uint32_t)constParams.dim,
            0,
            1,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            &imageWriteInfo,
            NULL,
            NULL,
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            Unwrap(m_DebugData.DescSet),
            (uint32_t)ShaderDebugBind::Sampler,
            0,
            1,
            VK_DESCRIPTOR_TYPE_SAMPLER,
            &samplerWriteInfo,
            NULL,
            NULL,
        },
    };

    if(buffer)
    {
      writeSets[1].pTexelBufferView = UnwrapPtr(bufferView);
      writeSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    }

    // reset descriptor sets to dummy state
    if(depthTex)
    {
      uint32_t resetIndex = 3;

      rdcarray<VkWriteDescriptorSet> writes;

      for(size_t i = 0; i < ARRAY_COUNT(m_DebugData.DummyWrites[resetIndex]); i++)
      {
        // not all textures may be supported for depth, so only update those that are valid
        if(m_DebugData.DummyWrites[resetIndex][i].descriptorCount != 0)
          writes.push_back(m_DebugData.DummyWrites[resetIndex][i]);
      }

      ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), (uint32_t)writes.count(), writes.data(), 0,
                                         NULL);
    }
    else
    {
      uint32_t resetIndex = 0;
      if(uintTex)
        resetIndex = 1;
      else if(sintTex)
        resetIndex = 2;

      ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev),
                                         ARRAY_COUNT(m_DebugData.DummyWrites[resetIndex]),
                                         m_DebugData.DummyWrites[resetIndex], 0, NULL);
    }

    // overwrite with our data
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), sampler != VK_NULL_HANDLE ? 3 : 2, writeSets, 0,
                                       NULL);

    void *constants = m_DebugData.ConstantsBuffer.Map(NULL, 0);
    if(!constants)
      return false;

    memcpy(constants, &uniformParams, sizeof(uniformParams));

    m_DebugData.ConstantsBuffer.Unmap();

    {
      VkCommandBuffer cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return false;

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      m_pDriver->CheckVkResult(vkr);

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
      m_pDriver->CheckVkResult(vkr);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    float *retf = (float *)m_DebugData.ReadbackBuffer.Map(NULL, 0);
    if(!retf)
      return false;

    uint32_t *retu = (uint32_t *)retf;
    int32_t *reti = (int32_t *)retf;

    // convert full precision results, we did all sampling at 32-bit precision
    for(uint8_t c = 0; c < 4; c++)
    {
      if(VarTypeCompType(output.type) == CompType::Float)
        setFloatComp(output, c, retf[c]);
      else if(VarTypeCompType(output.type) == CompType::SInt)
        setIntComp(output, c, reti[c]);
      else
        setUintComp(output, c, retu[c]);
    }

    m_DebugData.ReadbackBuffer.Unmap();

    return true;
  }

  virtual bool CalculateMathOp(rdcspv::ThreadState &lane, rdcspv::GLSLstd450 op,
                               const rdcarray<ShaderVariable> &params, ShaderVariable &output) override
  {
    RDCASSERT(params.size() <= 3, params.size());

    int floatSizeIdx = 0;
    if(params[0].type == VarType::Half)
      floatSizeIdx = 1;
    else if(params[0].type == VarType::Double)
      floatSizeIdx = 2;

    if(m_DebugData.MathPipe[floatSizeIdx] == VK_NULL_HANDLE)
    {
      ShaderConstParameters pipeParams = {};
      pipeParams.operation = (uint32_t)rdcspv::Op::ExtInst;
      m_DebugData.MathPipe[floatSizeIdx] =
          MakePipe(pipeParams, VarTypeByteSize(params[0].type) * 8, false, false, false);

      if(m_DebugData.MathPipe[floatSizeIdx] == VK_NULL_HANDLE)
      {
        m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                   MessageSource::RuntimeWarning,
                                   "Failed to compile graphics pipeline for math operation");
        return false;
      }
    }

    VkDescriptorBufferInfo storageWriteInfo = {};
    m_DebugData.MathResult.FillDescriptor(storageWriteInfo);

    VkWriteDescriptorSet writeSets[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            Unwrap(m_DebugData.DescSet),
            (uint32_t)ShaderDebugBind::MathResult,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            NULL,
            &storageWriteInfo,
            NULL,
        },
    };

    VkDevice dev = m_pDriver->GetDev();

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 1, writeSets, 0, NULL);

    {
      VkCommandBuffer cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return false;

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      m_pDriver->CheckVkResult(vkr);

      ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                    Unwrap(m_DebugData.MathPipe[floatSizeIdx]));

      ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                          Unwrap(m_DebugData.PipeLayout), 0, 1,
                                          UnwrapPtr(m_DebugData.DescSet), 0, NULL);

      // push the parameters
      for(size_t i = 0; i < params.size(); i++)
      {
        RDCASSERTEQUAL(params[i].type, params[0].type);
        double p[4] = {};
        memcpy(p, params[i].value.f32v.data(), VarTypeByteSize(params[i].type) * params[i].columns);
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout),
                                       VK_SHADER_STAGE_ALL, uint32_t(sizeof(p) * i), sizeof(p), p);
      }

      // push the operation afterwards
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout),
                                     VK_SHADER_STAGE_ALL, sizeof(Vec4f) * 6, sizeof(uint32_t), &op);

      ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), 1, 1, 1);

      VkBufferMemoryBarrier bufBarrier = {
          VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          NULL,
          VK_ACCESS_SHADER_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          Unwrap(m_DebugData.MathResult.buf),
          0,
          VK_WHOLE_SIZE,
      };

      DoPipelineBarrier(cmd, 1, &bufBarrier);

      VkBufferCopy bufCopy = {0, 0, 0};
      bufCopy.size = sizeof(Vec4f) * 2;
      ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_DebugData.MathResult.buf),
                                  Unwrap(m_DebugData.ReadbackBuffer.buf), 1, &bufCopy);

      bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      bufBarrier.buffer = Unwrap(m_DebugData.ReadbackBuffer.buf);

      // wait for copy to finish before reading back to host
      DoPipelineBarrier(cmd, 1, &bufBarrier);

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      m_pDriver->CheckVkResult(vkr);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    byte *ret = (byte *)m_DebugData.ReadbackBuffer.Map(NULL, 0);
    if(!ret)
      return false;

    // these two operations change the type of the output
    if(op == rdcspv::GLSLstd450::Length || op == rdcspv::GLSLstd450::Distance)
      output.columns = 1;

    memcpy(output.value.u32v.data(), ret, VarTypeByteSize(output.type) * output.columns);

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

  bool m_ResourcesDirty = false;
  uint32_t m_EventID;
  ResourceId m_ShaderID;

  rdcarray<DescriptorAccess> m_Access;
  rdcarray<Descriptor> m_Descriptors;
  rdcarray<SamplerDescriptor> m_SamplerDescriptors;

  std::map<ResourceId, VkImageView> m_SampleViews;

  typedef rdcpair<ResourceId, float> SamplerBiasKey;
  std::map<SamplerBiasKey, VkSampler> m_BiasSamplers;

  std::map<ShaderBindIndex, bytebuf> bufferCache;

  struct ImageData
  {
    uint32_t width = 0, height = 0, depth = 0;
    uint32_t texelSize = 0;
    uint64_t rowPitch = 0, slicePitch = 0, samplePitch = 0;
    ResourceFormat fmt;
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

  std::map<ShaderBindIndex, ImageData> imageCache;

  const Descriptor &GetDescriptor(const rdcstr &access, ShaderBindIndex index, bool &valid)
  {
    static Descriptor dummy;

    if(index.category == DescriptorCategory::Unknown)
    {
      // invalid index, return a dummy data but don't mark as invalid
      return dummy;
    }

    int32_t a = m_Access.indexOf(index);

    // this should not happen unless the debugging references an array element that we didn't
    // detect dynamically. We could improve this by retrieving a more conservative access set
    // internally so that all descriptors are 'accessed'
    if(a < 0)
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Internal error: Binding %s %u[%u] did not "
                                                   "exist in calculated descriptor access when %s.",
                                                   ToStr(index.category).c_str(), index.index,
                                                   index.arrayElement, access.c_str()));
      valid = false;
      return dummy;
    }

    return m_Descriptors[a];
  }

  const SamplerDescriptor &GetSamplerDescriptor(const rdcstr &access, ShaderBindIndex index,
                                                bool &valid)
  {
    static SamplerDescriptor dummy;

    if(index.category == DescriptorCategory::Unknown)
    {
      // invalid index, return a dummy data but don't mark as invalid
      return dummy;
    }

    int32_t a = m_Access.indexOf(index);

    // this should not happen unless the debugging references an array element that we didn't
    // detect dynamically. We could improve this by retrieving a more conservative access set
    // internally so that all descriptors are 'accessed'
    if(a < 0)
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Internal error: Binding %s %u[%u] did not "
                                                   "exist in calculated descriptor access when %s.",
                                                   ToStr(index.category).c_str(), index.index,
                                                   index.arrayElement, access.c_str()));
      valid = false;
      return dummy;
    }

    return m_SamplerDescriptors[a];
  }

  bytebuf &PopulateBuffer(uint64_t address, size_t &offs)
  {
    // pick a non-overlapping bind namespace for direct pointer access
    ShaderBindIndex bind;
    uint64_t base;
    uint64_t end;
    ResourceId id;
    bool valid = false;
    if(m_Creation.m_BufferAddresses.empty())
    {
      bind.arrayElement = 0;
      auto insertIt = bufferCache.insert(std::make_pair(bind, bytebuf()));
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt("pointer access detected but no address-capable buffers allocated."));
      return insertIt.first->second;
    }
    else
    {
      auto it = m_Creation.m_BufferAddresses.lower_bound(address);
      // lower_bound puts us at the same or next item. Since we want the buffer that contains
      // this address, we go to the previous iter unless we're already on the first or
      // it's an exact match
      if(address != it->first && it != m_Creation.m_BufferAddresses.begin())
        it--;
      // use the index in the map as a unique buffer identifier that's not 64-bit
      bind.arrayElement = uint32_t(it - m_Creation.m_BufferAddresses.begin());
      {
        base = it->first;
        id = it->second;
        end = base + m_Creation.m_Buffer[id].size;
        if(base <= address && address < end)
        {
          offs = (size_t)(address - base);
          valid = true;
        }
      }
    }
    if(!valid)
    {
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt("out of bounds pointer access of address %#18llx detected.Closest "
                            "buffer is address range %#18llx -> %#18llx (%s)",
                            address, base, end, ToStr(id).c_str()));
    }
    auto insertIt = bufferCache.insert(std::make_pair(bind, bytebuf()));
    bytebuf &data = insertIt.first->second;
    if(insertIt.second && valid)
    {
      // if the resources might be dirty from side-effects from the action, replay back to right
      // before it.
      if(m_ResourcesDirty)
      {
        VkMarkerRegion region("un-dirtying resources");
        m_pDriver->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
        m_ResourcesDirty = false;
      }
      m_pDriver->GetDebugManager()->GetBufferData(id, 0, 0, data);
    }
    return data;
  }

  bytebuf &PopulateBuffer(ShaderBindIndex bind)
  {
    auto insertIt = bufferCache.insert(std::make_pair(bind, bytebuf()));
    bytebuf &data = insertIt.first->second;
    if(insertIt.second)
    {
      bool valid = true;
      const Descriptor &bufData = GetDescriptor("accessing buffer value", bind, valid);
      if(valid)
      {
        // if the resources might be dirty from side-effects from the action, replay back to right
        // before it.
        if(m_ResourcesDirty)
        {
          VkMarkerRegion region("un-dirtying resources");
          m_pDriver->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
          m_ResourcesDirty = false;
        }

        if(bufData.resource != ResourceId())
        {
          m_pDriver->GetReplay()->GetBufferData(
              m_pDriver->GetResourceManager()->GetLiveID(bufData.resource), bufData.byteOffset,
              bufData.byteSize, data);
        }
      }
    }

    return data;
  }

  ImageData &PopulateImage(ShaderBindIndex bind)
  {
    auto insertIt = imageCache.insert(std::make_pair(bind, ImageData()));
    ImageData &data = insertIt.first->second;
    if(insertIt.second)
    {
      bool valid = true;
      const Descriptor &imgData = GetDescriptor("performing image load/store", bind, valid);
      if(valid)
      {
        // if the resources might be dirty from side-effects from the action, replay back to right
        // before it.
        if(m_ResourcesDirty)
        {
          VkMarkerRegion region("un-dirtying resources");
          m_pDriver->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
          m_ResourcesDirty = false;
        }

        if(imgData.view != ResourceId())
        {
          const VulkanCreationInfo::ImageView &viewProps =
              m_Creation.m_ImageView[m_pDriver->GetResourceManager()->GetLiveID(imgData.view)];
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

          ResourceFormat fmt = MakeResourceFormat(imageProps.format);

          data.fmt = MakeResourceFormat(imageProps.format);
          data.texelSize = (uint32_t)GetByteSize(1, 1, 1, imageProps.format, 0);
          data.rowPitch = (uint32_t)GetByteSize(data.width, 1, 1, imageProps.format, 0);
          data.slicePitch = GetByteSize(data.width, data.height, 1, imageProps.format, 0);
          data.samplePitch = GetByteSize(data.width, data.height, data.depth, imageProps.format, 0);

          const uint32_t numSlices = imageProps.type == VK_IMAGE_TYPE_3D ? 1 : data.depth;
          const uint32_t numSamples = (uint32_t)imageProps.samples;

          data.bytes.reserve(size_t(data.samplePitch * numSamples));

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

  VkPipeline MakePipe(const ShaderConstParameters &params, uint32_t floatBitSize, bool depthTex,
                      bool uintTex, bool sintTex)
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
    if(depthTex)
      shaderIndex = 1;
    if(uintTex)
      shaderIndex = 2;
    else if(sintTex)
      shaderIndex = 3;

    if(params.operation == (uint32_t)rdcspv::Op::ExtInst)
    {
      shaderIndex = 4;
      if(floatBitSize == 16)
        shaderIndex = 5;
      else if(floatBitSize == 64)
        shaderIndex = 6;
    }

    if(m_DebugData.Module[shaderIndex] == VK_NULL_HANDLE)
    {
      rdcarray<uint32_t> spirv;

      if(params.operation == (uint32_t)rdcspv::Op::ExtInst)
      {
        GenerateMathShaderModule(spirv, floatBitSize);
      }
      else
      {
        RDCASSERTMSG("Assume sampling happens with 32-bit float inputs", floatBitSize == 32,
                     floatBitSize);
        GenerateSamplingShaderModule(spirv, depthTex, uintTex, sintTex);
      }

      VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      moduleCreateInfo.pCode = spirv.data();
      moduleCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);

      VkResult vkr = m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &moduleCreateInfo, NULL,
                                                     &m_DebugData.Module[shaderIndex]);
      m_pDriver->CheckVkResult(vkr);

      const char *filename[] = {
          "/debug_psgather_float.spv", "/debug_psgather_depth.spv", "/debug_psgather_uint.spv",
          "/debug_psgather_sint.spv",  "/debug_psmath32.spv",       "/debug_psmath16.spv",
          "/debug_psmath64.spv",
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

    if(params.operation == (uint32_t)rdcspv::Op::ExtInst)
    {
      VkComputePipelineCreateInfo computePipeInfo = {
          VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          NULL,
          0,
          {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
           VK_SHADER_STAGE_COMPUTE_BIT, m_DebugData.Module[shaderIndex], "main", NULL},
          m_DebugData.PipeLayout,
          VK_NULL_HANDLE,
          0,
      };

      VkPipeline pipe = VK_NULL_HANDLE;
      VkResult vkr = m_pDriver->vkCreateComputePipelines(m_pDriver->GetDev(),
                                                         m_pDriver->GetShaderCache()->GetPipeCache(),
                                                         1, &computePipeInfo, NULL, &pipe);
      if(vkr != VK_SUCCESS)
      {
        RDCERR("Failed creating debug pipeline: %s", ToStr(vkr).c_str());
        return VK_NULL_HANDLE;
      }

      m_DebugData.m_Pipelines[key] = pipe;

      return pipe;
    }

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
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        NULL,
        0,
        VK_SAMPLE_COUNT_1_BIT,
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

  void GenerateMathShaderModule(rdcarray<uint32_t> &spirv, uint32_t floatBitSize)
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

    rdcspv::Scalar sizedScalar;
    if(floatBitSize == 32)
      sizedScalar = rdcspv::scalar<float>();
    else if(floatBitSize == 64)
      sizedScalar = rdcspv::scalar<double>();
    else
      sizedScalar = rdcspv::scalar<half_float::half>();

    rdcspv::Id u32 = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id floatType = editor.DeclareType(sizedScalar);
    rdcspv::Id vec4Type = editor.DeclareType(rdcspv::Vector(sizedScalar, 4));

    rdcspv::Id pushStructID =
        editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {vec4Type, vec4Type, vec4Type, u32}));
    editor.AddDecoration(rdcspv::OpDecorate(pushStructID, rdcspv::Decoration::Block));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 1, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f) * 2)));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 2, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f) * 4)));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        pushStructID, 3, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f) * 6)));
    editor.SetMemberName(pushStructID, 0, "a");
    editor.SetMemberName(pushStructID, 1, "b");
    editor.SetMemberName(pushStructID, 2, "c");
    editor.SetMemberName(pushStructID, 3, "op");

    rdcspv::Id pushPtrType =
        editor.DeclareType(rdcspv::Pointer(pushStructID, rdcspv::StorageClass::PushConstant));
    rdcspv::Id pushVar = editor.AddVariable(
        rdcspv::OpVariable(pushPtrType, editor.MakeId(), rdcspv::StorageClass::PushConstant));
    editor.SetName(pushVar, "pushData");

    rdcspv::Id pushv4Type =
        editor.DeclareType(rdcspv::Pointer(vec4Type, rdcspv::StorageClass::PushConstant));
    rdcspv::Id pushu32Type =
        editor.DeclareType(rdcspv::Pointer(u32, rdcspv::StorageClass::PushConstant));

    rdcspv::Id storageStructType = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {vec4Type}));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        storageStructType, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));
    editor.DecorateStorageBufferStruct(storageStructType);

    rdcspv::Id storageStructPtrType =
        editor.DeclareType(rdcspv::Pointer(storageStructType, editor.StorageBufferClass()));
    rdcspv::Id storageVec4PtrType =
        editor.DeclareType(rdcspv::Pointer(vec4Type, editor.StorageBufferClass()));

    rdcspv::Id storageVar = editor.AddVariable(
        rdcspv::OpVariable(storageStructPtrType, editor.MakeId(), editor.StorageBufferClass()));
    editor.AddDecoration(rdcspv::OpDecorate(
        storageVar, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0U)));
    editor.AddDecoration(rdcspv::OpDecorate(
        storageVar,
        rdcspv::DecorationParam<rdcspv::Decoration::Binding>((uint32_t)ShaderDebugBind::MathResult)));

    editor.SetName(storageVar, "resultStorage");

    // register the entry point
    editor.AddOperation(editor.Begin(rdcspv::Section::EntryPoints),
                        rdcspv::OpEntryPoint(rdcspv::ExecutionModel::GLCompute, entryId, "main", {}));
    editor.AddExecutionMode(rdcspv::OpExecutionMode(
        entryId, rdcspv::ExecutionModeParam<rdcspv::ExecutionMode::LocalSize>(1, 1, 1)));

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());
    rdcspv::Id funcType = editor.DeclareType(rdcspv::FunctionType(voidType, {}));

    rdcspv::OperationList func;
    func.add(rdcspv::OpFunction(voidType, entryId, rdcspv::FunctionControl::None, funcType));
    func.add(rdcspv::OpLabel(editor.MakeId()));

    rdcspv::Id consts[] = {
        editor.AddConstantImmediate<uint32_t>(0),
        editor.AddConstantImmediate<uint32_t>(1),
        editor.AddConstantImmediate<uint32_t>(2),
        editor.AddConstantImmediate<uint32_t>(3),
    };

    rdcspv::Id zerof;
    if(floatBitSize == 32)
      zerof = editor.AddConstantImmediate<float>(0.0f);
    else if(floatBitSize == 64)
      zerof = editor.AddConstantImmediate<double>(0.0);
    else
      zerof = editor.AddConstantImmediate<half_float::half>(half_float::half(0.0f));

    // load the parameters and the op
    rdcspv::Id aPtr =
        func.add(rdcspv::OpAccessChain(pushv4Type, editor.MakeId(), pushVar, {consts[0]}));
    rdcspv::Id bPtr =
        func.add(rdcspv::OpAccessChain(pushv4Type, editor.MakeId(), pushVar, {consts[1]}));
    rdcspv::Id cPtr =
        func.add(rdcspv::OpAccessChain(pushv4Type, editor.MakeId(), pushVar, {consts[2]}));
    rdcspv::Id opPtr =
        func.add(rdcspv::OpAccessChain(pushu32Type, editor.MakeId(), pushVar, {consts[3]}));
    rdcspv::Id a = func.add(rdcspv::OpLoad(vec4Type, editor.MakeId(), aPtr));
    rdcspv::Id b = func.add(rdcspv::OpLoad(vec4Type, editor.MakeId(), bPtr));
    rdcspv::Id c = func.add(rdcspv::OpLoad(vec4Type, editor.MakeId(), cPtr));
    rdcspv::Id opParam = func.add(rdcspv::OpLoad(u32, editor.MakeId(), opPtr));

    // access chain the output
    rdcspv::Id outVar =
        func.add(rdcspv::OpAccessChain(storageVec4PtrType, editor.MakeId(), storageVar, {consts[0]}));

    rdcspv::Id breakLabel = editor.MakeId();
    rdcspv::Id defaultLabel = editor.MakeId();

    rdcarray<rdcspv::SwitchPairU32LiteralId> targets;

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
      // most operations aren't allowed on doubles
      if(floatBitSize == 64 && op != rdcspv::GLSLstd450::Sqrt &&
         op != rdcspv::GLSLstd450::InverseSqrt && op != rdcspv::GLSLstd450::Normalize)
        continue;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result = cases.add(rdcspv::OpGLSL450(vec4Type, editor.MakeId(), glsl450, op, {a}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    if(floatBitSize != 64)
    {
      // these take two parameters, but are otherwise identical
      for(rdcspv::GLSLstd450 op : {rdcspv::GLSLstd450::Atan2, rdcspv::GLSLstd450::Pow})
      {
        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op, label});

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id result =
            cases.add(rdcspv::OpGLSL450(vec4Type, editor.MakeId(), glsl450, op, {a, b}));
        cases.add(rdcspv::OpStore(outVar, result));
        cases.add(rdcspv::OpBranch(breakLabel));
      }
    }

    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Fma;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result =
          cases.add(rdcspv::OpGLSL450(vec4Type, editor.MakeId(), glsl450, op, {a, b, c}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    // these ones are special
    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Length;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result = cases.add(rdcspv::OpGLSL450(floatType, editor.MakeId(), glsl450, op, {a}));
      rdcspv::Id resultvec = cases.add(
          rdcspv::OpCompositeConstruct(vec4Type, editor.MakeId(), {result, zerof, zerof, zerof}));
      cases.add(rdcspv::OpStore(outVar, resultvec));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Distance;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id result =
          cases.add(rdcspv::OpGLSL450(floatType, editor.MakeId(), glsl450, op, {a, b}));
      rdcspv::Id resultvec = cases.add(
          rdcspv::OpCompositeConstruct(vec4Type, editor.MakeId(), {result, zerof, zerof, zerof}));
      cases.add(rdcspv::OpStore(outVar, resultvec));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    {
      rdcspv::GLSLstd450 op = rdcspv::GLSLstd450::Refract;

      rdcspv::Id label = editor.MakeId();
      targets.push_back({(uint32_t)op, label});

      cases.add(rdcspv::OpLabel(label));
      rdcspv::Id eta = cases.add(rdcspv::OpCompositeExtract(floatType, editor.MakeId(), c, {0}));
      rdcspv::Id result =
          cases.add(rdcspv::OpGLSL450(vec4Type, editor.MakeId(), glsl450, op, {a, b, eta}));
      cases.add(rdcspv::OpStore(outVar, result));
      cases.add(rdcspv::OpBranch(breakLabel));
    }

    func.add(rdcspv::OpSelectionMerge(breakLabel, rdcspv::SelectionControl::None));
    func.add(rdcspv::OpSwitch32(opParam, defaultLabel, targets));

    func.append(cases);

    // default: store NULL data
    func.add(rdcspv::OpLabel(defaultLabel));
    func.add(rdcspv::OpStore(
        outVar, editor.AddConstant(rdcspv::OpConstantNull(vec4Type, editor.MakeId()))));
    func.add(rdcspv::OpBranch(breakLabel));

    func.add(rdcspv::OpLabel(breakLabel));
    func.add(rdcspv::OpReturn());
    func.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(func);
  }

  void GenerateSamplingShaderModule(rdcarray<uint32_t> &spirv, bool depthTex, bool uintTex,
                                    bool sintTex)
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
    DECL_UNIFORM(float, u, uvwa[0]);
    DECL_UNIFORM(float, v, uvwa[1]);
    DECL_UNIFORM(float, w, uvwa[2]);
    DECL_UNIFORM(float, cube_a, uvwa[3]);
    DECL_UNIFORM(float, dudx, ddx[0]);
    DECL_UNIFORM(float, dvdx, ddx[1]);
    DECL_UNIFORM(float, dwdx, ddx[2]);
    DECL_UNIFORM(float, dudy, ddy[0]);
    DECL_UNIFORM(float, dvdy, ddy[1]);
    DECL_UNIFORM(float, dwdy, ddy[2]);
    DECL_UNIFORM(int32_t, dynoffset_u, offset.x);
    DECL_UNIFORM(int32_t, dynoffset_v, offset.y);
    DECL_UNIFORM(int32_t, dynoffset_w, offset.z);
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

    rdcspv::Id constoffset_u = gather_u0;
    rdcspv::Id constoffset_uv = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v2i32, editor.MakeId(), {gather_u0, gather_v0}));
    rdcspv::Id constoffset_uvw = editor.AddConstant(
        rdcspv::OpSpecConstantComposite(v3i32, editor.MakeId(), {gather_u0, gather_v0, gather_u1}));
    editor.SetName(constoffset_u, "constoffset_u");
    editor.SetName(constoffset_uv, "constoffset_uv");
    editor.SetName(constoffset_uvw, "constoffset_uvw");

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

      if(depthTex)
      {
        if(i == (size_t)ShaderDebugBind::Tex3D && !m_pDriver->GetReplay()->Depth3DSupported())
          continue;
        else if(i == (size_t)ShaderDebugBind::TexCube && !m_pDriver->GetReplay()->DepthCubeSupported())
          continue;
      }

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
    if(bindVars[(size_t)ShaderDebugBind::Tex3D] != rdcspv::Id())
      editor.SetName(bindVars[(size_t)ShaderDebugBind::Tex3D], "Tex3D");
    editor.SetName(bindVars[(size_t)ShaderDebugBind::Tex2DMS], "Tex2DMS");
    if(bindVars[(size_t)ShaderDebugBind::TexCube] != rdcspv::Id())
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
    editor.AddExecutionMode(rdcspv::OpExecutionMode(entryId, rdcspv::ExecutionMode::OriginUpperLeft));

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

    rdcspv::Id dynoffset_uv =
        func.add(rdcspv::OpCompositeConstruct(v2i32, editor.MakeId(), {dynoffset_u, dynoffset_v}));
    rdcspv::Id dynoffset_uvw = func.add(rdcspv::OpCompositeConstruct(
        v3i32, editor.MakeId(), {dynoffset_u, dynoffset_v, dynoffset_w}));

    editor.SetName(dynoffset_uv, "dynoffset_uv");
    editor.SetName(dynoffset_uvw, "dynoffset_uvw");

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
    rdcarray<rdcspv::SwitchPairU32LiteralId> targets;

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

    rdcspv::Id constoffset[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        constoffset_u,      // 1D - u
        constoffset_uv,     // 2D - u,v
        constoffset_uvw,    // 3D - u,v,w
        constoffset_uv,     // 2DMS - u,v
        rdcspv::Id(),       // Cube - not valid
        constoffset_u,      // Buffer - u
    };

    rdcspv::Id dynoffset[(uint32_t)ShaderDebugBind::Count] = {
        rdcspv::Id(),
        dynoffset_u,      // 1D - u
        dynoffset_uv,     // 2D - u,v
        dynoffset_uvw,    // 3D - u,v,w
        dynoffset_uv,     // 2DMS - u,v
        rdcspv::Id(),     // Cube - not valid
        dynoffset_u,      // Buffer - u
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

      if(bindVars[i] == rdcspv::Id())
        continue;

      rdcspv::ImageOperandsAndParamDatas imageOperandsWithOffsets;

      // most operations support offsets, set the operands commonly here.
      // with the shaderImageGatherExtended feature, gather opcodes will always get their operands
      // via uniforms to cut down on pipeline specialisations a little, but all other cases the
      // offsets must be constant.
      if(constoffset[i] != rdcspv::Id())
        imageOperandsWithOffsets.setConstOffset(constoffset[i]);

      // can't fetch from cubemaps
      if(i != (uint32_t)ShaderDebugBind::TexCube)
      {
        rdcspv::Op op = rdcspv::Op::ImageFetch;

        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

        rdcspv::ImageOperandsAndParamDatas operands = imageOperandsWithOffsets;

        if(i != (uint32_t)ShaderDebugBind::Buffer && i != (uint32_t)ShaderDebugBind::Tex2DMS)
          operands.setLod(texel_lod);

        if(i == (uint32_t)ShaderDebugBind::Tex2DMS)
          operands.setSample(sampleIdx);

        cases.add(rdcspv::OpLabel(label));
        rdcspv::Id loaded = cases.add(rdcspv::OpLoad(texSampTypes[i], editor.MakeId(), bindVars[i]));
        rdcspv::Id sampleResult = cases.add(
            rdcspv::OpImageFetch(resultType, editor.MakeId(), loaded, texel_coord[i], operands));
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
          rdcspv::ImageOperandsAndParamDatas operands = imageOperandsWithOffsets;
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
          rdcspv::ImageOperandsAndParamDatas operands = imageOperandsWithOffsets;
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

      // on Qualcomm we only emit Dref instructions against 2D textures, otherwise the compiler may
      // crash.
      if(m_pDriver->GetDriverInfo().QualcommDrefNon2DCompileCrash())
        depthTex &= (i == (uint32_t)ShaderDebugBind::Tex2D);

      // VUID-StandaloneSpirv-OpImage-04777
      // OpImage*Dref must not consume an image whose Dim is 3D
      if(i == (uint32_t)ShaderDebugBind::Tex3D)
        depthTex = false;

      // don't emit dref's for uint/sint textures
      if(uintTex || sintTex)
        depthTex = false;

      if(depthTex)
      {
        for(rdcspv::Op op :
            {rdcspv::Op::ImageSampleDrefExplicitLod, rdcspv::Op::ImageSampleDrefImplicitLod})
        {
          rdcspv::Id label = editor.MakeId();
          targets.push_back({(uint32_t)op * 10 + i, label});

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
            rdcspv::ImageOperandsAndParamDatas operands = imageOperandsWithOffsets;
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
            rdcspv::ImageOperandsAndParamDatas operands = imageOperandsWithOffsets;
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
        if(op == rdcspv::Op::ImageDrefGather && !depthTex)
          continue;

        rdcspv::Id label = editor.MakeId();
        targets.push_back({(uint32_t)op * 10 + i, label});

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

            rdcspv::ImageOperandsAndParamDatas operands;

            if(dynoffset[i] != rdcspv::Id())
              operands.setOffset(dynoffset[i]);

            rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
                texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

            if(op == rdcspv::Op::ImageGather)
              baseResult = cases.add(rdcspv::OpImageGather(resultType, editor.MakeId(), combined,
                                                           coord[i], gatherChannel, operands));
            else
              baseResult = cases.add(rdcspv::OpImageDrefGather(
                  resultType, editor.MakeId(), combined, coord[i], compare, operands));

            cases.add(rdcspv::OpBranch(mergeLabel));
          }

          rdcspv::Id constsResult;
          {
            cases.add(rdcspv::OpLabel(constsCase));
            rdcspv::ImageOperandsAndParamDatas operands;    // don't use the offsets above

            // if this feature isn't available, this path will never be exercised (since we only
            // come in here when the actual shader used const offsets) so it's fine to drop it in
            // that case to ensure the module is still legal.
            if(m_pDriver->GetDeviceEnabledFeatures().shaderImageGatherExtended &&
               i != (uint32_t)ShaderDebugBind::TexCube)
              operands.setConstOffsets(gatherOffsets);

            rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
                texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

            if(op == rdcspv::Op::ImageGather)
              constsResult = cases.add(rdcspv::OpImageGather(resultType, editor.MakeId(), combined,
                                                             coord[i], gatherChannel, operands));
            else
              constsResult = cases.add(rdcspv::OpImageDrefGather(
                  resultType, editor.MakeId(), combined, coord[i], compare, operands));

            cases.add(rdcspv::OpBranch(mergeLabel));
          }

          cases.add(rdcspv::OpLabel(mergeLabel));
          sampleResult = cases.add(rdcspv::OpPhi(
              resultType, editor.MakeId(), {{baseResult, baseCase}, {constsResult, constsCase}}));
        }
        else
        {
          rdcspv::ImageOperandsAndParamDatas operands = imageOperandsWithOffsets;

          rdcspv::Id combined = cases.add(rdcspv::OpSampledImage(
              texSampCombinedTypes[i], editor.MakeId(), loadedImage, loadedSampler));

          if(op == rdcspv::Op::ImageGather)
            sampleResult = cases.add(rdcspv::OpImageGather(resultType, editor.MakeId(), combined,
                                                           coord[i], gatherChannel, operands));
          else
            sampleResult = cases.add(rdcspv::OpImageDrefGather(
                resultType, editor.MakeId(), combined, coord[i], compare, operands));
        }

        cases.add(rdcspv::OpStore(outVar, sampleResult));
        cases.add(rdcspv::OpBranch(breakLabel));
      }
    }

    func.add(rdcspv::OpSelectionMerge(breakLabel, rdcspv::SelectionControl::None));
    func.add(rdcspv::OpSwitch32(switchVal, defaultLabel, targets));

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
  uint32_t view;
  uint32_t valid;
  float ddxDerivCheck;
  uint32_t padding[3];
  // PSInput base, ddx, ....
};

static void CreatePSInputFetcher(rdcarray<uint32_t> &fragspv, uint32_t &structStride,
                                 VulkanCreationInfo::ShaderModuleReflection &shadRefl,
                                 const uint32_t paramAlign, StorageMode storageMode,
                                 bool usePrimitiveID, bool useSampleID, bool useViewIndex)
{
  rdcspv::Editor editor(fragspv);

  editor.Prepare();

  rdcspv::Id entryID;

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

    it = editor.Begin(rdcspv::Section::EntryPoints);
    end = editor.End(rdcspv::Section::EntryPoints);
    while(it < end)
    {
      rdcspv::OpEntryPoint e(it);
      if(e.name == shadRefl.entryPoint && e.executionModel == rdcspv::ExecutionModel::Fragment)
      {
        // remember the Id of our entry point
        entryID = e.entryPoint;
      }
      else
      {
        // remove all other entry points
        removedIds.push_back(e.entryPoint);
        editor.Remove(it);
      }
      it++;
    }

    it = editor.Begin(rdcspv::Section::ExecutionMode);
    end = editor.End(rdcspv::Section::ExecutionMode);
    while(it < end)
    {
      // this can also handle ExecutionModeId and we don't care about the difference
      rdcspv::OpExecutionMode execMode(it);

      // remove any execution modes not for our entry
      if(execMode.entryPoint != entryID)
        editor.Remove(it);
      it++;
    }

    // remove any OpName that refers to deleted IDs - functions or results
    it = editor.Begin(rdcspv::Section::DebugNames);
    end = editor.End(rdcspv::Section::DebugNames);
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

  rdcarray<rdcspv::Id> addedInputs;

  // builtin inputs we need
  struct BuiltinAccess
  {
    rdcspv::Id base;
    rdcspv::Id type;
    uint32_t member = ~0U;
  } fragCoord, primitiveID, sampleIndex, viewIndex;

  // look to see which ones are already provided
  for(size_t i = 0; i < shadRefl.refl->inputSignature.size(); i++)
  {
    const SigParameter &param = shadRefl.refl->inputSignature[i];

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
    else if(param.systemValue == ShaderBuiltin::MultiViewIndex)
    {
      access = &viewIndex;

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

  if(viewIndex.base == rdcspv::Id() && useViewIndex)
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    viewIndex.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));
    viewIndex.type = type;

    editor.AddDecoration(rdcspv::OpDecorate(
        viewIndex.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::ViewIndex)));
    editor.AddDecoration(rdcspv::OpDecorate(viewIndex.base, rdcspv::Decoration::Flat));

    addedInputs.push_back(viewIndex.base);

    editor.AddCapability(rdcspv::Capability::MultiView);
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
  values.resize(shadRefl.refl->inputSignature.size());

  {
    rdcarray<rdcspv::Id> ids;
    rdcarray<uint32_t> offsets;
    rdcarray<uint32_t> indices;
    for(size_t i = 0; i < shadRefl.refl->inputSignature.size(); i++)
    {
      const SigParameter &param = shadRefl.refl->inputSignature[i];

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
      editor.SetMemberName(PSInput, values[i].structIndex, shadRefl.refl->inputSignature[i].varName);

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
      // uint view;
      uint32Type,
      // uint valid;
      uint32Type,
      // float ddxDerivCheck;
      floatType,
      // <uint3 padding>

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
    editor.SetMemberName(PSHit, member, "view");
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

    // <uint3 padding>
    offs += sizeof(uint32_t) * 3;

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
                                                                             sizeof(Vec4f) * 3)));

  rdcspv::Id bufBase = editor.DeclareStructType({
      // uint hit_count;
      uint32Type,
      // uint total_count;
      uint32Type,
      // <uint2 padding>

      //  PSHit hits[];
      PSHitRTArray,
  });

  rdcspv::Id ssboVar;

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
    if(useViewIndex)
      editor.AddExtension("SPV_KHR_multiview");

    // change the memory model to physical storage buffer 64
    rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
    rdcspv::OpMemoryModel model(it);
    model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
    it = model;

    // add capabilities
    editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);

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
      editor.AddCapability(rdcspv::Capability::Int64);

      addressConstant =
          editor.AddSpecConstantImmediate<uint64_t>(0ULL, (uint32_t)InputSpecConstant::Address);
    }

    editor.SetName(addressConstant, "__rd_bufAddress");

    // struct is block decorated
    editor.AddDecoration(rdcspv::OpDecorate(bufBase, rdcspv::Decoration::Block));
  }

  if(editor.EntryPointAllGlobals() && ssboVar != rdcspv::Id())
    addedInputs.push_back(ssboVar);

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
        const SigParameter &param = shadRefl.refl->inputSignature[i];

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

      if(viewIndex.base != rdcspv::Id())
      {
        if(viewIndex.member == ~0U)
        {
          loaded = ops.add(rdcspv::OpLoad(viewIndex.type, editor.MakeId(), viewIndex.base));
        }
        else
        {
          rdcspv::Id inPtrType =
              editor.DeclareType(rdcspv::Pointer(viewIndex.type, rdcspv::StorageClass::Input));

          rdcspv::Id viewidxptr =
              ops.add(rdcspv::OpAccessChain(inPtrType, editor.MakeId(), viewIndex.base,
                                            {editor.AddConstantImmediate(viewIndex.member)}));
          loaded = ops.add(rdcspv::OpLoad(viewIndex.type, editor.MakeId(), viewidxptr));
        }

        // if it was loaded as signed int by the shader and not as unsigned by us, bitcast to
        // unsigned.
        if(viewIndex.type != uint32Type)
          loaded = ops.add(rdcspv::OpBitcast(uint32Type, editor.MakeId(), loaded));
      }
      else
      {
        // explicitly store 0
        loaded = getUIntConst(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(3)}));
      ops.add(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(4)}));
      ops.add(rdcspv::OpStore(storePtr, editor.AddConstantImmediate(validMagicNumber), alignedAccess));

      // store ddx(gl_FragCoord.x) to check that derivatives are working
      storePtr = ops.add(rdcspv::OpAccessChain(floatBufPtr, editor.MakeId(), hit, {getUIntConst(5)}));
      rdcspv::Id fragCoord_ddx_x =
          ops.add(rdcspv::OpCompositeExtract(floatType, editor.MakeId(), fragCoord_ddx, {0}));
      ops.add(rdcspv::OpStore(storePtr, fragCoord_ddx_x, alignedAccess));

      {
        rdcspv::Id inputPtrType = editor.DeclareType(rdcspv::Pointer(PSInput, bufferClass));

        rdcspv::Id outputPtrs[Variant_Count] = {
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(6)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(7)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(8)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(9)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(10)})),
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
                                            uint32_t idx, uint32_t view)
{
  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  rdcstr regionName =
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u,%u)", eventId, vertid, instid, idx, view);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(!(action->flags & ActionFlags::Drawcall))
  {
    RDCLOG("No drawcall selected");
    return new ShaderDebugTrace();
  }

  uint32_t vertOffset = 0, instOffset = 0;
  if(!(action->flags & ActionFlags::Indexed))
    vertOffset = action->vertexOffset;

  if(action->flags & ActionFlags::Instanced)
    instOffset = action->instanceOffset;

  // get ourselves in pristine state before this action (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[0].module];
  rdcstr entryPoint = pipe.shaders[0].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[0].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(ShaderStage::Vertex, entryPoint, state.graphics.pipeline);

  if(!shadRefl.refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, ShaderStage::Vertex, eventId, shadRefl.refl->resourceId);

  // clamp the view index to the number of multiviews, just to be sure
  size_t numViews;

  if(state.dynamicRendering.active)
    numViews = Log2Ceil(state.dynamicRendering.viewMask + 1);
  else
    numViews = c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].multiviews.size();
  if(numViews > 1)
    view = RDCMIN((uint32_t)numViews - 1, view);
  else
    view = 0;

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::BaseInstance] =
      ShaderVariable(rdcstr(), action->instanceOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::BaseVertex] = ShaderVariable(
      rdcstr(), (action->flags & ActionFlags::Indexed) ? action->baseVertex : action->vertexOffset,
      0U, 0U, 0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), action->drawIndex, 0U, 0U, 0U);
  if(action->flags & ActionFlags::Indexed)
    builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), idx, 0U, 0U, 0U);
  else
    builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), vertid + vertOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::InstanceIndex] = ShaderVariable(rdcstr(), instid + instOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::ViewportIndex] = ShaderVariable(rdcstr(), view, 0U, 0U, 0U);
  builtins[ShaderBuiltin::MultiViewIndex] = ShaderVariable(rdcstr(), view, 0U, 0U, 0U);

  rdcarray<ShaderVariable> &locations = apiWrapper->location_inputs;
  for(const VkVertexInputAttributeDescription2EXT &attr : state.vertexAttributes)
  {
    locations.resize_for_index(attr.location);

    if(Vulkan_Debug_ShaderDebugLogging())
      RDCLOG("Populating location %u", attr.location);

    ShaderVariable &var = locations[attr.location];

    bytebuf data;

    size_t size = (size_t)GetByteSize(1, 1, 1, attr.format, 0);

    bool found = false;

    for(const VkVertexInputBindingDescription2EXT &bind : state.vertexBindings)
    {
      if(bind.binding != attr.binding)
        continue;

      if(bind.binding < state.vbuffers.size())
      {
        const VulkanRenderState::VertBuffer &vb = state.vbuffers[bind.binding];

        if(vb.buf != ResourceId())
        {
          VkDeviceSize vertexOffset = 0;

          found = true;

          if(bind.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE)
          {
            if(bind.divisor == 0)
              vertexOffset = instOffset * vb.stride;
            else
              vertexOffset = (instOffset + (instid / bind.divisor)) * vb.stride;
          }
          else
          {
            vertexOffset = (idx + vertOffset) * vb.stride;
          }

          if(Vulkan_Debug_ShaderDebugLogging())
          {
            RDCLOG("Fetching from %s at %llu offset %zu bytes", ToStr(vb.buf).c_str(),
                   vb.offs + attr.offset + vertexOffset, size);
          }

          if(attr.offset + vertexOffset < vb.size)
            GetDebugManager()->GetBufferData(vb.buf, vb.offs + attr.offset + vertexOffset, size,
                                             data);
        }
      }
      else if(Vulkan_Debug_ShaderDebugLogging())
      {
        RDCLOG("Vertex binding %u out of bounds from %zu vertex buffers", bind.binding,
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
        var.type = VarType::UInt;
      else
        var.type = VarType::Float;

      set0001(var);
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

          var.type = VarType::UInt;

          setUintComp(var, 0, decoded.x);
          setUintComp(var, 1, decoded.y);
          setUintComp(var, 2, decoded.z);
          setUintComp(var, 3, decoded.w);
        }
        else
        {
          var.type = VarType::UInt;

          if(fmt.compType == CompType::UInt)
          {
            if(fmt.compByteWidth == 1)
              var.type = VarType::UByte;
            else if(fmt.compByteWidth == 2)
              var.type = VarType::UShort;
            else if(fmt.compByteWidth == 4)
              var.type = VarType::UInt;
            else if(fmt.compByteWidth == 8)
              var.type = VarType::ULong;
          }
          else if(fmt.compType == CompType::SInt)
          {
            if(fmt.compByteWidth == 1)
              var.type = VarType::SByte;
            else if(fmt.compByteWidth == 2)
              var.type = VarType::SShort;
            else if(fmt.compByteWidth == 4)
              var.type = VarType::SInt;
            else if(fmt.compByteWidth == 8)
              var.type = VarType::SLong;
          }

          RDCASSERTEQUAL(fmt.compByteWidth, VarTypeByteSize(var.type));
          memcpy(var.value.u8v.data(), data.data(), fmt.compByteWidth * fmt.compCount);
        }
      }
      else
      {
        FloatVector decoded = DecodeFormattedComponents(fmt, data.data());

        var.type = VarType::Float;

        setFloatComp(var, 0, decoded.x);
        setFloatComp(var, 1, decoded.y);
        setFloatComp(var, 2, decoded.z);
        setFloatComp(var, 3, decoded.w);
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
                                           const DebugPixelInputs &inputs)
{
  if(!m_pDriver->GetDeviceEnabledFeatures().fragmentStoresAndAtomics)
  {
    RDCWARN("Pixel debugging is not supported without fragment stores");
    return new ShaderDebugTrace;
  }

  VkDevice dev = m_pDriver->GetDev();
  VkResult vkr = VK_SUCCESS;

  uint32_t sample = inputs.sample;
  uint32_t primitive = inputs.primitive;
  uint32_t view = inputs.view;

  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  rdcstr regionName = StringFormat::Fmt("DebugPixel @ %u of (%u,%u) sample %u primitive %u view %u",
                                        eventId, x, y, sample, primitive, view);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(!(action->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall)))
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

  // get ourselves in pristine state before this action (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[4].module];
  rdcstr entryPoint = pipe.shaders[4].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[4].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(ShaderStage::Pixel, entryPoint, state.graphics.pipeline);

  if(!shadRefl.refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, ShaderStage::Pixel, eventId, shadRefl.refl->resourceId);

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), action->drawIndex, 0U, 0U, 0U);
  builtins[ShaderBuiltin::Position] =
      ShaderVariable(rdcstr(), float(x) + 0.5f, float(y) + 0.5f, 0.0f, 0.0f);

  // If the pipe contains a geometry shader, then Primitive ID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  bool usePrimitiveID = false;
  if(pipe.shaders[3].module != ResourceId())
  {
    VulkanCreationInfo::ShaderModuleReflection &gsRefl =
        c.m_ShaderModule[pipe.shaders[3].module].GetReflection(
            ShaderStage::Geometry, pipe.shaders[3].entryPoint, state.graphics.pipeline);

    // check to see if the shader outputs a primitive ID
    for(const SigParameter &e : gsRefl.refl->outputSignature)
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

  bool useViewIndex = (view == ~0U) ? false : true;
  if(useViewIndex)
  {
    ResourceId rp = state.GetRenderPass();
    if(rp != ResourceId())
    {
      const VulkanCreationInfo::RenderPass &rpInfo =
          m_pDriver->GetDebugManager()->GetRenderPassInfo(rp);
      for(auto it = rpInfo.subpasses.begin(); it != rpInfo.subpasses.end(); ++it)
      {
        if(it->multiviews.isEmpty())
        {
          if(Vulkan_Debug_ShaderDebugLogging())
            RDCLOG(
                "Disabling useViewIndex because at least one subpass does not have multiple views");
          useViewIndex = false;
          break;
        }
      }
    }
    else
    {
      useViewIndex = pipe.viewMask != 0;
      if(!useViewIndex && Vulkan_Debug_ShaderDebugLogging())
        RDCLOG("Disabling useViewIndex because viewMask is zero");
    }
  }
  else
  {
    if(Vulkan_Debug_ShaderDebugLogging())
      RDCLOG("Disabling useViewIndex from input view %u", view);
  }
  if(useViewIndex)
  {
    builtins[ShaderBuiltin::MultiViewIndex] = ShaderVariable(rdcstr(), view, 0U, 0U, 0U);
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

  if(Vulkan_Debug_DisableBufferDeviceAddress() ||
     m_pDriver->GetDriverInfo().BufferDeviceAddressBrokenDriver())
    storageMode = Binding;

  rdcarray<uint32_t> fragspv = shader.spirv.GetSPIRV();

  if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_psinput_before.spv", fragspv);

  uint32_t paramAlign = 16;

  for(const SigParameter &sig : shadRefl.refl->inputSignature)
  {
    if(VarTypeByteSize(sig.varType) * sig.compCount > paramAlign)
      paramAlign = 32;
  }

  uint32_t structStride = 0;
  CreatePSInputFetcher(fragspv, structStride, shadRefl, paramAlign, storageMode, usePrimitiveID,
                       useSampleID, useViewIndex);

  if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_psinput_after.spv", fragspv);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  VkGraphicsPipelineCreateInfo graphicsInfo = {};

  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(graphicsInfo, state.graphics.pipeline);

  // use the load RP if an RP is specified
  if(graphicsInfo.renderPass != VK_NULL_HANDLE)
  {
    graphicsInfo.renderPass =
        c.m_RenderPass[GetResID(graphicsInfo.renderPass)].loadRPs[graphicsInfo.subpass];
    graphicsInfo.subpass = 0;
  }

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

    NameVulkanObject(m_BindlessFeedback.FeedbackBuffer.buf, "m_BindlessFeedback.FeedbackBuffer");
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
    // don't have to handle separate vert/frag layouts as push constant ranges must be identical
    const rdcarray<VkPushConstantRange> &push = c.m_PipelineLayout[pipe.vertLayout].pushRanges;

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
    CheckVkResult(vkr);

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
          (uint32_t)InputSpecConstant::Address,
          offsetof(SpecData, bufferAddress),
          sizeof(uint32_t),
      },
      {
          (uint32_t)InputSpecConstant::ArrayLength,
          offsetof(SpecData, arrayLength),
          sizeof(SpecData::arrayLength),
      },
      {
          (uint32_t)InputSpecConstant::DestX,
          offsetof(SpecData, destX),
          sizeof(SpecData::destX),
      },
      {
          (uint32_t)InputSpecConstant::DestY,
          offsetof(SpecData, destY),
          sizeof(SpecData::destY),
      },
      {
          (uint32_t)InputSpecConstant::AddressMSB,
          offsetof(SpecData, bufferAddress) + 4,
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
      CheckVkResult(vkr);

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
      CheckVkResult(vkr);

      modules.push_back(stage.module);
    }
  }

  // we don't use a pipeline cache here because our spec constants will cause failures often and
  // bloat the cache. Even if we avoided the high-frequency x/y and stored them e.g. in the feedback
  // buffer, we'd still want to spec-constant the address when possible so we're always going to
  // have some varying value.
  VkPipeline inputsPipe;
  vkr =
      m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &graphicsInfo, NULL, &inputsPipe);
  CheckVkResult(vkr);

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

  modifiedstate.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
  modifiedstate.dynamicRendering.flags &= ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return new ShaderDebugTrace;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

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

    modifiedstate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                               false);

    m_pDriver->ReplayDraw(cmd, *action);

    modifiedstate.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

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

    // if we're looking for a specific view, ignore hits from the wrong view
    if(useViewIndex)
    {
      if(hit->view != view)
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

    // the data immediately follows the PSHit header. Every piece of data is uniformly aligned,
    // either 16-byte by default or 32-byte if larger components exist. The output is in input
    // signature order.
    byte *PSInputs = (byte *)(winner + 1);
    byte *value = (byte *)(PSInputs + 0 * structStride);
    byte *ddxcoarse = (byte *)(PSInputs + 1 * structStride);
    byte *ddycoarse = (byte *)(PSInputs + 2 * structStride);
    byte *ddxfine = (byte *)(PSInputs + 3 * structStride);
    byte *ddyfine = (byte *)(PSInputs + 4 * structStride);

    for(size_t i = 0; i < shadRefl.refl->inputSignature.size(); i++)
    {
      const SigParameter &param = shadRefl.refl->inputSignature[i];

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

      var.rows = 1;
      var.columns = param.compCount & 0xff;
      var.type = param.varType;

      deriv.ddxcoarse = var;
      deriv.ddycoarse = var;
      deriv.ddxfine = var;
      deriv.ddyfine = var;

      const uint32_t comp = Bits::CountTrailingZeroes(uint32_t(param.regChannelMask));
      const uint32_t elemSize = VarTypeByteSize(param.varType);

      const size_t sz = elemSize * param.compCount;

      memcpy((var.value.u8v.data()) + elemSize * comp, value + i * paramAlign, sz);
      memcpy((deriv.ddxcoarse.value.u8v.data()) + elemSize * comp, ddxcoarse + i * paramAlign, sz);
      memcpy((deriv.ddycoarse.value.u8v.data()) + elemSize * comp, ddycoarse + i * paramAlign, sz);
      memcpy((deriv.ddxfine.value.u8v.data()) + elemSize * comp, ddxfine + i * paramAlign, sz);
      memcpy((deriv.ddyfine.value.u8v.data()) + elemSize * comp, ddyfine + i * paramAlign, sz);
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

ShaderDebugTrace *VulkanReplay::DebugThread(uint32_t eventId,
                                            const rdcfixedarray<uint32_t, 3> &groupid,
                                            const rdcfixedarray<uint32_t, 3> &threadid)
{
  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  rdcstr regionName =
      StringFormat::Fmt("DebugThread @ %u of (%u,%u,%u) (%u,%u,%u)", eventId, groupid[0],
                        groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(!(action->flags & ActionFlags::Dispatch))
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
      shader.GetReflection(ShaderStage::Compute, entryPoint, state.compute.pipeline);

  if(!shadRefl.refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, ShaderStage::Compute, eventId, shadRefl.refl->resourceId);

  uint32_t threadDim[3];
  threadDim[0] = shadRefl.refl->dispatchThreadsDimension[0];
  threadDim[1] = shadRefl.refl->dispatchThreadsDimension[1];
  threadDim[2] = shadRefl.refl->dispatchThreadsDimension[2];

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DispatchSize] =
      ShaderVariable(rdcstr(), action->dispatchDimension[0], action->dispatchDimension[1],
                     action->dispatchDimension[2], 0U);
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
      if(m_TexRender.DummyImageViews[fmt][dim] == VK_NULL_HANDLE)
        continue;

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

    if(m_TexRender.DummyBufferView[fmt] != VK_NULL_HANDLE)
    {
      m_ShaderDebugData.DummyWrites[fmt][6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      m_ShaderDebugData.DummyWrites[fmt][6].descriptorCount = 1;
      m_ShaderDebugData.DummyWrites[fmt][6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
      m_ShaderDebugData.DummyWrites[fmt][6].dstBinding = (uint32_t)ShaderDebugBind::Buffer;
      m_ShaderDebugData.DummyWrites[fmt][6].dstSet = Unwrap(m_ShaderDebugData.DescSet);
      m_ShaderDebugData.DummyWrites[fmt][6].pTexelBufferView =
          UnwrapPtr(m_TexRender.DummyBufferView[fmt]);
    }
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
