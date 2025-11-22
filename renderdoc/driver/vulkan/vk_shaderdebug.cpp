/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2025 Baldur Karlsson
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
#include "replay/common/var_dispatch_helpers.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

#undef None

RDOC_CONFIG(rdcstr, Vulkan_Debug_PSDebugDumpDirPath, "",
            "Path to dump shader debugging generated SPIR-V files.");
RDOC_CONFIG(bool, Vulkan_Debug_ShaderDebugLogging, false,
            "Output verbose debug logging messages when debugging shaders.");

// needed for old linux compilers
namespace std
{
template <>
struct hash<ShaderBuiltin>
{
  std::size_t operator()(const ShaderBuiltin &e) const { return size_t(e); }
};
}

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

#if ENABLED(RDOC_RELEASE)
#define CHECK_DEVICE_THREAD()
#else
#define CHECK_DEVICE_THREAD() \
  RDCASSERTMSG("API Wrapper function called from non-device thread!", IsDeviceThread());
#endif    // #if ENABLED(RDOC_RELEASE)

class VulkanAPIWrapper : public rdcspv::DebugAPIWrapper
{
public:
  VulkanAPIWrapper(WrappedVulkan *vk, VulkanCreationInfo &creation, ShaderStage stage, uint32_t eid,
                   ResourceId shadId)
      : m_DebugData(vk->GetReplay()->GetShaderDebugData()),
        m_Creation(creation),
        m_EventID(eid),
        m_ShaderID(shadId),
        deviceThreadID(Threading::GetCurrentID())
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
         ranges.back().offset + ranges.back().descriptorSize == acc.byteOffset &&
         ranges.back().type == acc.type)
      {
        ranges.back().count++;
        continue;
      }

      DescriptorRange range = acc;
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

      const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &src =
          (stage == ShaderStage::Compute ? state.compute.descSets : state.graphics.descSets);

      for(size_t i = 0; i < src.size(); i++)
      {
        const VulkanStatePipeline::DescriptorAndOffsets &srcData = src[i];
        ResourceId sourceSet = srcData.descSet;
        const uint32_t *srcOffset = srcData.offsets.begin();

        // this could be either an unbound set, or descriptor buffers (which can't use dynamic offsets anyway)
        if(sourceSet == ResourceId())
          continue;

        const VulkanCreationInfo::PipelineLayout &pipeLayoutInfo =
            m_Creation.GetPipelineLayoutInfo(srcData.pipeLayout);

        ResourceId setOrig = m_pDriver->GetResourceManager()->GetOriginalID(sourceSet);

        const BindingStorage &bindStorage =
            m_pDriver->GetCurrentDescSetBindingStorage(srcData.descSet);
        const DescriptorSetSlot *first = bindStorage.binds.empty() ? NULL : bindStorage.binds[0];
        for(size_t b = 0; b < bindStorage.binds.size(); b++)
        {
          const DescSetLayout::Binding &layoutBind =
              m_Creation.GetDescSetLayout(pipeLayoutInfo.descSetLayouts[i]).bindings[b];

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
    RDCASSERT(ShaderDebugData::MAX_QUEUED_OPS * mathOpResultByteSize <
              m_DebugData.ReadbackBuffer.TotalSize());
    RDCASSERT(sampleGatherOpResultsStart +
                  ShaderDebugData::MAX_QUEUED_OPS * sampleGatherOpResultByteSize <
              m_DebugData.ReadbackBuffer.TotalSize());
  }

  ~VulkanAPIWrapper()
  {
    CHECK_DEVICE_THREAD();
    m_pDriver->FlushQ();

    VkDevice dev = m_pDriver->GetDev();
    for(auto it = m_SampleViews.begin(); it != m_SampleViews.end(); it++)
      m_pDriver->vkDestroyImageView(dev, it->second, NULL);
    for(auto it = m_BiasSamplers.begin(); it != m_BiasSamplers.end(); it++)
      m_pDriver->vkDestroySampler(dev, it->second, NULL);
  }

  void ResetReplay()
  {
    CHECK_DEVICE_THREAD();
    if(!m_ResourcesDirty)
    {
      VkMarkerRegion region("ResetReplay");
      // replay the action to get back to 'normal' state for this event, and mark that we need to
      // replay back to pristine state next time we need to fetch data.
      m_pDriver->ReplayLog(0, m_EventID, eReplay_OnlyDraw);
    }
    m_ResourcesDirty = true;
  }

  virtual void AddDebugMessage(MessageCategory cat, MessageSeverity sev, MessageSource src,
                               rdcstr desc) override
  {
    CHECK_DEVICE_THREAD();
    m_pDriver->AddDebugMessage(cat, sev, src, desc);
  }

  virtual ResourceId GetShaderID() override { return m_ShaderID; }

  virtual uint64_t GetBufferLength(const ShaderBindIndex &bind) override
  {
    rdcspv::DeviceOpResult opResult;
    size_t length = 0;
    // BufferFunction guarantees the buffer cache readlock whilst the function is called
    bool succeeded = BufferFunction(
        bind, [&length](bytebuf *data) { length = data->size(); }, opResult);
    RDCASSERT(succeeded);
    RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Succeeded);
    return length;
  }

  virtual void ReadBufferValue(const ShaderBindIndex &bind, uint64_t offset, uint64_t byteSize,
                               void *dst) override
  {
    rdcspv::DeviceOpResult opResult;
    // BufferFunction guarantees the buffer cache readlock whilst the function is called
    bool succeeded = BufferFunction(
        bind,
        [offset, byteSize, dst](bytebuf *data) {
          if(offset + byteSize <= data->size())
            memcpy(dst, data->data() + (size_t)offset, (size_t)byteSize);
        },
        opResult);
    RDCASSERT(succeeded);
    RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Succeeded);
  }

  virtual void WriteBufferValue(const ShaderBindIndex &bind, uint64_t offset, uint64_t byteSize,
                                const void *src) override
  {
    rdcspv::DeviceOpResult opResult;
    // BufferFunction guarantees the buffer cache readlock whilst the function is called
    bool succeeded = BufferFunction(
        bind,
        [offset, byteSize, src](bytebuf *data) {
          if(offset + byteSize <= data->size())
            memcpy(data->data() + (size_t)offset, src, (size_t)byteSize);
        },
        opResult);
    RDCASSERT(succeeded);
    RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Succeeded);
  }

  virtual void ReadAddress(uint64_t address, uint64_t byteSize, void *dst) override
  {
    rdcspv::DeviceOpResult opResult;
    // BufferFunction guarantees the buffer cache readlock whilst the function is called
    bool succeeded = BufferFunction(
        address,
        [byteSize, dst](bytebuf *data, size_t offset) {
          if(offset + byteSize <= data->size())
            memcpy(dst, data->data() + offset, (size_t)byteSize);
        },
        opResult);
    RDCASSERT(succeeded);
    RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Succeeded);
  }

  virtual void WriteAddress(uint64_t address, uint64_t byteSize, const void *src) override
  {
    rdcspv::DeviceOpResult opResult;
    // BufferFunction guarantees the buffer cache readlock whilst the function is called
    bool succeeded = BufferFunction(
        address,
        [byteSize, src](bytebuf *data, size_t offset) {
          if(offset + byteSize <= data->size())
            memcpy(data->data() + offset, src, (size_t)byteSize);
        },
        opResult);
    RDCASSERT(succeeded);
    RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Succeeded);
  }

  // Called from any thread
  // Caller guarantees that if the image data is not cached then we are on the device thread
  virtual rdcspv::DeviceOpResult ReadTexel(const ShaderBindIndex &imageBind,
                                           const ShaderVariable &coord, uint32_t sample,
                                           ShaderVariable &output) override
  {
    rdcspv::DeviceOpResult opResult;
    bool isCached = false;
    {
      SCOPED_READLOCK(imageCacheLock);
      isCached = GetImageDataFromCache(imageBind, opResult) != NULL;
      RDCASSERTNOTEQUAL(opResult, rdcspv::DeviceOpResult::NeedsDevice);
    }

    if(!isCached)
    {
      // Add image data to the cache : cache should not be locked by this thread
      PopulateImage(imageBind);
    }

    {
      SCOPED_READLOCK(imageCacheLock);
      ImageData *result = GetImageDataFromCache(imageBind, opResult);
      if(!result)
      {
        RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Failed);
        return rdcspv::DeviceOpResult::Failed;
      }

      ImageData &data = *result;
      if(data.width == 0)
        return rdcspv::DeviceOpResult::Failed;

      uint32_t coords[4];
      for(int i = 0; i < 4; i++)
        coords[i] = uintComp(coord, i);

      if(coords[0] >= data.width || coords[1] >= data.height || coords[2] >= data.depth)
      {
        if(!IsDeviceThread())
          return rdcspv::DeviceOpResult::NeedsDevice;

        CHECK_DEVICE_THREAD();
        m_pDriver->AddDebugMessage(
            MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
            StringFormat::Fmt(
                "Out of bounds access to image, coord %u,%u,%u outside of dimensions %ux%ux%u",
                coords[0], coords[1], coords[2], data.width, data.height, data.depth));
        return rdcspv::DeviceOpResult::Failed;
      }

      CompType varComp = VarTypeCompType(output.type);

      set0001(output);

      ShaderVariable input;
      input.columns = data.fmt.compCount;

      // the only 'irregular' format we need to worry about handling for integer types is
      // 10:10:10:2. All others are float/uint
      if(data.fmt.type == ResourceFormatType::R10G10B10A2)
      {
        PixelValue val;
        DecodePixelData(data.fmt, data.texel(coords, sample), val);

        if(data.fmt.compType == CompType::UInt)
          input.type = VarType::UInt;
        else if(data.fmt.compType == CompType::SInt)
          input.type = VarType::SInt;
        else
          input.type = VarType::Float;

        memcpy(input.value.u32v.data(), val.uintValue.data(), val.uintValue.byteSize());

        for(uint8_t c = 0; c < RDCMIN(output.columns, input.columns); c++)
        {
          if(data.fmt.compType == CompType::UInt)
            setUintComp(output, c, uintComp(input, c));
          else if(data.fmt.compType == CompType::SInt)
            setIntComp(output, c, intComp(input, c));
          else
            setFloatComp(output, c, input.value.f32v[c]);
        }
      }
      else if(data.fmt.compType == CompType::UInt)
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
    }

    return rdcspv::DeviceOpResult::Succeeded;
  }

  // Called from any thread
  // Caller guarantees that if the image data is not cached then we are on the device thread
  virtual rdcspv::DeviceOpResult WriteTexel(const ShaderBindIndex &imageBind,
                                            const ShaderVariable &coord, uint32_t sample,
                                            const ShaderVariable &input) override
  {
    rdcspv::DeviceOpResult opResult;
    ImageData *result = NULL;
    {
      SCOPED_READLOCK(imageCacheLock);
      result = GetImageDataFromCache(imageBind, opResult);
      RDCASSERTNOTEQUAL(opResult, rdcspv::DeviceOpResult::NeedsDevice);
    }

    if(!result)
    {
      // Add image data to the cache : cache should not be locked by this thread
      PopulateImage(imageBind);
    }

    {
      SCOPED_READLOCK(imageCacheLock);
      result = GetImageDataFromCache(imageBind, opResult);
      if(!result)
        return rdcspv::DeviceOpResult::Failed;

      ImageData &data = *result;
      if(data.width == 0)
        return rdcspv::DeviceOpResult::Failed;

      uint32_t coords[4];
      for(int i = 0; i < 4; i++)
        coords[i] = uintComp(coord, i);

      if(coords[0] >= data.width || coords[1] >= data.height || coords[2] >= data.depth)
      {
        if(!IsDeviceThread())
          return rdcspv::DeviceOpResult::NeedsDevice;

        CHECK_DEVICE_THREAD();
        m_pDriver->AddDebugMessage(
            MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
            StringFormat::Fmt(
                "Out of bounds access to image, coord %u,%u,%u outside of dimensions %ux%ux%u",
                coords[0], coords[1], coords[2], data.width, data.height, data.depth));
        return rdcspv::DeviceOpResult::Failed;
      }

      CompType varComp = VarTypeCompType(input.type);

      ShaderVariable output;
      output.columns = data.fmt.compCount;

      // the only 'irregular' format we need to worry about handling for integer types is
      // 10:10:10:2. All others are float/uint
      if(data.fmt.type == ResourceFormatType::R10G10B10A2)
      {
        // image writes are required to write a whole texel so we know we should have 4 components
        RDCASSERTEQUAL(input.columns, 4);

        uint32_t encoded = 0;

        if(data.fmt.compType == CompType::SNorm)
          encoded = ConvertToR10G10B10A2SNorm(Vec4f(input.value.f32v[0], input.value.f32v[1],
                                                    input.value.f32v[2], input.value.f32v[3]));
        else if(data.fmt.compType == CompType::UInt)
          encoded = ConvertToR10G10B10A2(Vec4u(input.value.u32v[0], input.value.u32v[1],
                                               input.value.u32v[2], input.value.u32v[3]));
        else
          encoded = ConvertToR10G10B10A2(Vec4f(input.value.f32v[0], input.value.f32v[1],
                                               input.value.f32v[2], input.value.f32v[3]));

        memcpy(data.texel(coords, sample), &encoded, sizeof(uint32_t));
      }
      else if(data.fmt.compType == CompType::UInt)
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
    }
    return rdcspv::DeviceOpResult::Succeeded;
  }

  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t threadIndex,
                              uint32_t location, uint32_t component) override
  {
    CHECK_DEVICE_THREAD();
    if(builtin != ShaderBuiltin::Undefined)
    {
      if(threadIndex < thread_builtins.size())
      {
        auto it = thread_builtins[threadIndex].find(builtin);
        if(it != thread_builtins[threadIndex].end())
        {
          var.value = it->second.value;
          return;
        }
      }

      auto it = global_builtins.find(builtin);
      if(it != global_builtins.end())
      {
        var.value = it->second.value;
        return;
      }

      RDCERR("Couldn't get input for %s", ToStr(builtin).c_str());
      return;
    }

    if(threadIndex < location_inputs.size())
    {
      if(location < location_inputs[threadIndex].size())
      {
        if(var.rows == 1)
        {
          if(component + var.columns > 4)
            RDCERR("Unexpected component %u for column count %u", component, var.columns);

          for(uint8_t c = 0; c < var.columns; c++)
            copyComp(var, c, location_inputs[threadIndex][location], component + c);
        }
        else
        {
          RDCASSERTEQUAL(component, 0);
          for(uint8_t r = 0; r < var.rows; r++)
            for(uint8_t c = 0; c < var.columns; c++)
              copyComp(var, r * var.columns + c, location_inputs[threadIndex][location + c], r);
        }
        return;
      }
    }

    RDCERR("Couldn't get input for %s at thread=%u, location=%u, component=%u", var.name.c_str(),
           threadIndex, location, component);
  }

  uint32_t GetThreadProperty(uint32_t threadIndex, rdcspv::ThreadProperty prop) override
  {
    CHECK_DEVICE_THREAD();
    if(prop >= rdcspv::ThreadProperty::Count)
      return 0;
    if(threadIndex >= thread_props.size())
      return 0;

    return thread_props[threadIndex][(size_t)prop];
  }

  bool QueueSampleGather(rdcspv::ThreadState &lane, rdcspv::Op opcode,
                         DebugAPIWrapper::TextureType texType, const ShaderBindIndex &imageBind,
                         const ShaderBindIndex &samplerBind, const ShaderVariable &uv,
                         const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                         const ShaderVariable &compare, rdcspv::GatherChannel gatherChannel,
                         const rdcspv::ImageOperandsAndParamDatas &operands, ShaderVariable &output,
                         bool &hasResult) override
  {
    CHECK_DEVICE_THREAD();
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
    {
      hasResult = false;
      return false;
    }

    VkMarkerRegion markerRegion("QueueSampleGather");

    VkBufferView bufferView =
        m_pDriver->GetResourceManager()->GetLiveHandle<VkBufferView>(bufferViewDescriptor.view);

    VkSampler sampler =
        m_pDriver->GetResourceManager()->GetLiveHandle<VkSampler>(samplerDescriptor.object);
    VkImageView view =
        m_pDriver->GetResourceManager()->GetLiveHandle<VkImageView>(imageDescriptor.view);
    VkImageLayout layout = convert((DescriptorSlotImageLayout)imageDescriptor.byteOffset);

    // NULL view : return 0,0,0,0
    if(!buffer && (view == VK_NULL_HANDLE))
    {
      memset(&output.value, 0, sizeof(output.value));
      hasResult = true;
      return true;
    }

    // promote view to Array view
    VulkanCreationInfo::ImageView defaultViewProps = {};
    VulkanCreationInfo::Image defaultImageProps = {};
    VulkanCreationInfo::Buffer defaultBufferProps = {};
    VulkanCreationInfo::Sampler defaultSamplerProps = {};

    const VulkanCreationInfo::ImageView &viewProps =
        buffer ? defaultViewProps : m_Creation.GetImageViewInfo(GetResID(view));

    // NULL image : return 0,0,0,0
    if(!buffer && (viewProps.image == ResourceId()))
    {
      memset(&output.value, 0, sizeof(output.value));
      hasResult = true;
      return true;
    }

    const VulkanCreationInfo::Image &imageProps =
        buffer ? defaultImageProps : m_Creation.GetImageInfo(viewProps.image);

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
        hasResult = true;
        return true;
      }
      case rdcspv::Op::ImageQuerySamples:
      {
        output.value.u32v[0] = (uint32_t)imageProps.samples;
        hasResult = true;
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
          VkDeviceSize size;
          VkFormat format;
          if(bufferView == VK_NULL_HANDLE)
          {
            // descriptor buffer case - there is no buffer view so read directly out of the determined descriptor
            format = MakeVkFormat(bufferViewDescriptor.format);
            // size is not allowed to be VK_WHOLE_SIZE
            size = bufferViewDescriptor.byteSize;
          }
          else
          {
            const VulkanCreationInfo::BufferView &bufViewProps =
                m_Creation.GetBufferViewInfo(GetResID(bufferView));

            size = bufViewProps.size;
            format = bufViewProps.format;

            if(size == VK_WHOLE_SIZE)
            {
              const VulkanCreationInfo::Buffer &bufProps =
                  bufViewProps.buffer == ResourceId()
                      ? defaultBufferProps
                      : m_Creation.GetBufferInfo(bufViewProps.buffer);
              size = bufProps.size - bufViewProps.offset;
            }
          }

          setUintComp(output, 0, uint32_t(size / GetByteSize(1, 1, 1, format, 0)));
        }

        hasResult = true;
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
      CHECK_VKR(m_pDriver, vkr);

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
          const VulkanCreationInfo::Sampler &samplerProps =
              (sampler == VK_NULL_HANDLE) ? defaultSamplerProps
                                          : m_Creation.GetSamplerInfo(key.first);

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
          CHECK_VKR(m_pDriver, vkr);

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

    VkCommandBuffer cmd = queuedOpCmdBuffer;
    if(cmd == VK_NULL_HANDLE)
    {
      if(StartQueuedOps())
        cmd = queuedOpCmdBuffer;
    }
    if(cmd == VK_NULL_HANDLE)
      return false;

    if(queueIndex >= ShaderDebugData::MAX_QUEUED_OPS)
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning, "Too many GPU queued operations");
      return false;
    }

    VkDescriptorImageInfo samplerWriteInfo = {Unwrap(sampler), VK_NULL_HANDLE,
                                              VK_IMAGE_LAYOUT_UNDEFINED};
    VkDescriptorImageInfo imageWriteInfo = {VK_NULL_HANDLE, Unwrap(sampleView), layout};

    VkDescriptorBufferInfo uniformWriteInfo = {};
    m_DebugData.ConstantsBuffer.FillDescriptor(uniformWriteInfo);
    const VkDescriptorSet realDescSet = Unwrap(m_DebugData.DescSets[queueIndex++]);

    VkWriteDescriptorSet writeSets[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            realDescSet,
            (uint32_t)ShaderDebugBind::Constants,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            NULL,
            &uniformWriteInfo,
            NULL,
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            realDescSet,
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
            realDescSet,
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
      if(bufferView == VK_NULL_HANDLE)
      {
        // descriptor buffer, must create our own

        BufViewKey key = {bufferViewDescriptor.resource, bufferViewDescriptor.byteOffset,
                          bufferViewDescriptor.byteSize, MakeVkFormat(bufferViewDescriptor.format)};

        bufferView = m_SampleBufViews[key];
        if(bufferView == VK_NULL_HANDLE)
        {
          VkBufferViewCreateInfo viewInfo = {
              VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
              NULL,
              0,
              m_pDriver->GetResourceManager()->GetLiveHandle<VkBuffer>(bufferViewDescriptor.resource),
              key.format,
              bufferViewDescriptor.byteOffset,
              bufferViewDescriptor.byteSize,
          };

          VkResult vkr = m_pDriver->vkCreateBufferView(dev, &viewInfo, NULL, &bufferView);
          CHECK_VKR(m_pDriver, vkr);

          m_SampleBufViews[key] = bufferView;
        }
      }

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
        {
          m_DebugData.DummyWrites[resetIndex][i].dstSet = realDescSet;
          writes.push_back(m_DebugData.DummyWrites[resetIndex][i]);
        }
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

      for(size_t i = 0; i < ARRAY_COUNT(m_DebugData.DummyWrites[resetIndex]); i++)
      {
        if(m_DebugData.DummyWrites[resetIndex][i].descriptorCount != 0)
          m_DebugData.DummyWrites[resetIndex][i].dstSet = realDescSet;
      }
      ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev),
                                         ARRAY_COUNT(m_DebugData.DummyWrites[resetIndex]),
                                         m_DebugData.DummyWrites[resetIndex], 0, NULL);
    }

    // overwrite with our data
    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), sampler != VK_NULL_HANDLE ? 3 : 2, writeSets, 0,
                                       NULL);

    uint32_t constsOffset = 0;
    void *constants = m_DebugData.ConstantsBuffer.Map(&constsOffset, 0);
    if(!constants)
      return false;

    memcpy(constants, &uniformParams, sizeof(uniformParams));

    m_DebugData.ConstantsBuffer.Unmap();

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
                                        Unwrap(m_DebugData.PipeLayout), 0, 1, &realDescSet, 1,
                                        &constsOffset);

    // push uvw/ddx/ddy for the vertex shader
    ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                   sizeof(Vec4f) * 0, sizeof(Vec4f), &uniformParams.uvwa);
    ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                   sizeof(Vec4f) * 1, sizeof(Vec3f), &uniformParams.ddx);
    ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                   sizeof(Vec4f) * 2, sizeof(Vec3f), &uniformParams.ddy);

    ObjDisp(cmd)->CmdDraw(Unwrap(cmd), 3, 1, 0, 0);

    ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));

    RDCASSERT(sampleGatherOpResultOffset + sampleGatherOpResultByteSize <=
                  m_DebugData.ReadbackBuffer.TotalSize(),
              sampleGatherOpResultOffset, sampleGatherOpResultByteSize,
              m_DebugData.ReadbackBuffer.TotalSize());
    VkBufferImageCopy region = {
        sampleGatherOpResultOffset,           sizeof(Vec4f), 1,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0},     {1, 1, 1},
    };
    sampleGatherOpResultOffset += sampleGatherOpResultByteSize;
    ObjDisp(cmd)->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(m_DebugData.Image),
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       m_DebugData.ReadbackBuffer.UnwrappedBuffer(), 1, &region);

    hasResult = false;
    return true;
  }

  virtual bool QueueCalculateMathOp(rdcspv::GLSLstd450 op,
                                    const rdcarray<ShaderVariable> &params) override
  {
    CHECK_DEVICE_THREAD();
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

    VkCommandBuffer cmd = queuedOpCmdBuffer;
    if(cmd == VK_NULL_HANDLE)
    {
      if(StartQueuedOps())
        cmd = queuedOpCmdBuffer;
    }
    if(cmd == VK_NULL_HANDLE)
      return false;

    if(queueIndex >= ShaderDebugData::MAX_QUEUED_OPS)
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning, "Too many GPU queued operations");
      return false;
    }
    VkDescriptorBufferInfo storageWriteInfo = {};
    m_DebugData.MathResult.FillDescriptor(storageWriteInfo);

    const VkDescriptorSet realDescSet = Unwrap(m_DebugData.DescSets[queueIndex++]);

    VkWriteDescriptorSet writeSets[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            NULL,
            realDescSet,
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

    VkMarkerRegion markerRegion("QueueCalculateMathOp");

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                  Unwrap(m_DebugData.MathPipe[floatSizeIdx]));

    uint32_t constsOffset = 0;
    ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                        Unwrap(m_DebugData.PipeLayout), 0, 1, &realDescSet, 1,
                                        &constsOffset);

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
    ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_DebugData.PipeLayout), VK_SHADER_STAGE_ALL,
                                   sizeof(Vec4f) * 6, sizeof(uint32_t), &op);

    ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), 1, 1, 1);

    VkBufferMemoryBarrier bufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        m_DebugData.MathResult.UnwrappedBuffer(),
        0,
        VK_WHOLE_SIZE,
    };

    DoPipelineBarrier(cmd, 1, &bufBarrier);

    RDCASSERT(mathOpResultOffset + mathOpResultByteSize <= m_DebugData.ReadbackBuffer.TotalSize(),
              mathOpResultOffset, mathOpResultByteSize, m_DebugData.ReadbackBuffer.TotalSize());
    VkBufferCopy bufCopy = {0, mathOpResultOffset, mathOpResultByteSize};
    mathOpResultOffset += mathOpResultByteSize;
    ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), m_DebugData.MathResult.UnwrappedBuffer(),
                                m_DebugData.ReadbackBuffer.UnwrappedBuffer(), 1, &bufCopy);
    return true;
  }

  bool StartQueuedOps()
  {
    CHECK_DEVICE_THREAD();

    RDCASSERTEQUAL(queueIndex, 0);
    RDCASSERTEQUAL(queuedOpCmdBuffer, VK_NULL_HANDLE);
    RDCASSERTEQUAL(mathOpResultOffset, 0);
    RDCASSERTEQUAL(sampleGatherOpResultOffset, 0);

    if(queuedOpCmdBuffer != VK_NULL_HANDLE)
      return false;

    queuedOpCmdBuffer = m_pDriver->GetNextCmd();
    if(queuedOpCmdBuffer == VK_NULL_HANDLE)
      return false;

    VkCommandBuffer cmd = queuedOpCmdBuffer;
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CHECK_VKR(m_pDriver, vkr);

    sampleGatherOpResultOffset = sampleGatherOpResultsStart;
    return true;
  }

  virtual bool GetQueuedResults(rdcarray<ShaderVariable *> &mathOpResults,
                                rdcarray<ShaderVariable *> &sampleGatherResults) override
  {
    CHECK_DEVICE_THREAD();
    if(queuedOpCmdBuffer == VK_NULL_HANDLE)
      return false;

    VkBufferMemoryBarrier bufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_HOST_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        m_DebugData.ReadbackBuffer.UnwrappedBuffer(),
        0,
        VK_WHOLE_SIZE,
    };

    VkCommandBuffer cmd = queuedOpCmdBuffer;
    if(cmd == VK_NULL_HANDLE)
      return false;

    // wait for copy to finish before reading back to host
    DoPipelineBarrier(cmd, 1, &bufBarrier);

    VkResult vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    CHECK_VKR(m_pDriver, vkr);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    queueIndex = 0;
    queuedOpCmdBuffer = VK_NULL_HANDLE;
    mathOpResultOffset = 0;
    sampleGatherOpResultOffset = 0;

    byte *gpuResults = (byte *)m_DebugData.ReadbackBuffer.Map(NULL, 0);
    if(!gpuResults)
      return false;

    uintptr_t bufferEnd = (uintptr_t)(gpuResults + m_DebugData.ReadbackBuffer.TotalSize());

    byte *gpuMathOpResults = gpuResults;
    for(ShaderVariable *result : mathOpResults)
    {
      size_t countBytes = VarTypeByteSize(result->type) * result->columns;
      RDCASSERT((uintptr_t)gpuMathOpResults + countBytes <= bufferEnd, (uintptr_t)gpuMathOpResults,
                countBytes, bufferEnd);
      RDCASSERT(countBytes <= mathOpResultByteSize, countBytes, mathOpResultByteSize);
      memcpy(result->value.u32v.data(), gpuMathOpResults, countBytes);
      gpuMathOpResults += mathOpResultByteSize;
    }

    byte *gpuSampleGatherOpResults = gpuResults + sampleGatherOpResultsStart;
    for(ShaderVariable *result : sampleGatherResults)
    {
      float *retf = (float *)gpuSampleGatherOpResults;
      uint32_t *retu = (uint32_t *)gpuSampleGatherOpResults;
      int32_t *reti = (int32_t *)gpuSampleGatherOpResults;

      size_t countBytes = 16;
      RDCASSERT((uintptr_t)gpuSampleGatherOpResults + countBytes <= bufferEnd,
                (uintptr_t)gpuSampleGatherOpResults, countBytes, bufferEnd);
      RDCASSERT(countBytes <= sampleGatherOpResultByteSize, countBytes, sampleGatherOpResultByteSize);
      // convert full precision results, we did all sampling at 32-bit precision
      ShaderVariable &output = *result;
      for(uint8_t c = 0; c < 4; c++)
      {
        if(VarTypeCompType(output.type) == CompType::Float)
          setFloatComp(output, c, retf[c]);
        else if(VarTypeCompType(output.type) == CompType::SInt)
          setIntComp(output, c, reti[c]);
        else
          setUintComp(output, c, retu[c]);
      }
      gpuSampleGatherOpResults += sampleGatherOpResultByteSize;
    }

    m_DebugData.ReadbackBuffer.Unmap();
    return true;
  }

  virtual bool QueuedOpsHasSpace() override { return queueIndex < ShaderDebugData::MAX_QUEUED_OPS; }

  // global over all threads
  std::unordered_map<ShaderBuiltin, ShaderVariable> global_builtins;

  // per-thread builtins
  rdcarray<std::unordered_map<ShaderBuiltin, ShaderVariable>> thread_builtins;

  // per-thread custom inputs by location [thread][location]
  rdcarray<rdcarray<ShaderVariable>> location_inputs;

  rdcarray<rdcfixedarray<uint32_t, arraydim<rdcspv::ThreadProperty>()>> thread_props;

  uint64_t GetDeviceThreadID() const { return deviceThreadID; }
  bool IsDeviceThread() const { return Threading::GetCurrentID() == GetDeviceThreadID(); }

private:
  WrappedVulkan *m_pDriver = NULL;
  ShaderDebugData &m_DebugData;
  const VulkanCreationInfo &m_Creation;

  bool m_ResourcesDirty = false;
  uint32_t m_EventID;
  ResourceId m_ShaderID;

  rdcarray<DescriptorAccess> m_Access;
  rdcarray<Descriptor> m_Descriptors;
  rdcarray<SamplerDescriptor> m_SamplerDescriptors;

  std::map<ResourceId, VkImageView> m_SampleViews;
  struct BufViewKey
  {
    ResourceId buf;
    VkDeviceSize offs, size;
    VkFormat format;
    bool operator==(const BufViewKey &o) const
    {
      return buf == o.buf && offs == o.offs && size == o.size && format == o.format;
    }
    bool operator<(const BufViewKey &o) const
    {
      if(buf != o.buf)
        return buf < o.buf;
      if(offs != o.offs)
        return offs < o.offs;
      if(size != o.size)
        return size < o.size;
      if(format != o.format)
        return format < o.format;
      return false;
    }
  };
  rdcflatmap<BufViewKey, VkBufferView> m_SampleBufViews;

  typedef rdcpair<ResourceId, float> SamplerBiasKey;
  std::map<SamplerBiasKey, VkSampler> m_BiasSamplers;

  Threading::RWLock bufferCacheLock;
  std::map<ShaderBindIndex, bytebuf> bufferCache;

  VkCommandBuffer queuedOpCmdBuffer = VK_NULL_HANDLE;
  VkDeviceSize mathOpResultOffset = 0;
  VkDeviceSize sampleGatherOpResultOffset = 0;
  uint32_t queueIndex = 0;
  const VkDeviceSize mathOpResultByteSize = sizeof(Vec4f) * 2;
  const VkDeviceSize sampleGatherOpResultByteSize = sizeof(Vec4f);
  const VkDeviceSize sampleGatherOpResultsStart =
      ShaderDebugData::MAX_QUEUED_OPS * mathOpResultByteSize;

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

  Threading::RWLock imageCacheLock;
  std::map<ShaderBindIndex, ImageData> imageCache;

  const Descriptor &GetDescriptor(const rdcstr &access, const ShaderBindIndex &index, bool &valid)
  {
    CHECK_DEVICE_THREAD();
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

  const SamplerDescriptor &GetSamplerDescriptor(const rdcstr &access, const ShaderBindIndex &index,
                                                bool &valid)
  {
    CHECK_DEVICE_THREAD();
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

  // Called from any thread
  ShaderBindIndex GenerateBufferBind(const uint64_t address, size_t &offs)
  {
    ResourceId id;
    uint64_t ptrOffs;
    m_pDriver->GetResIDFromAddr(address, id, ptrOffs);
    offs = size_t(ptrOffs);

    ShaderBindIndex bind;
    bind.arrayElement = (address - offs) & 0xFFFFFFFFU;

    return bind;
  }

  // Called from any thread
  bool IsBufferCached(const ShaderBindIndex &bind) override
  {
    SCOPED_READLOCK(bufferCacheLock);
    return bufferCache.find(bind) != bufferCache.end();
  }

  // Called from any thread
  bool IsBufferCached(uint64_t address) override
  {
    size_t offs = 0;
    ShaderBindIndex bind = GenerateBufferBind(address, offs);
    return IsBufferCached(bind);
  }

  // Called from any thread
  bytebuf *GetBufferDataFromCache(const ShaderBindIndex &bind, rdcspv::DeviceOpResult &opResult)
  {
    // Calling function responsible for acquiring bufferCache Read lock
    auto findIt = bufferCache.find(bind);
    if(findIt != bufferCache.end())
    {
      opResult = rdcspv::DeviceOpResult::Succeeded;
      return &findIt->second;
    }

    opResult = rdcspv::DeviceOpResult::Failed;

    // Not in the cache : populate must happen on the device thread
    if(!IsDeviceThread())
      opResult = rdcspv::DeviceOpResult::NeedsDevice;

    return NULL;
  }

  // Called from any thread
  bool BufferFunction(const ShaderBindIndex &bind, const std::function<void(bytebuf *data)> &func,
                      rdcspv::DeviceOpResult &opResult)
  {
    bool isCached = false;
    {
      SCOPED_READLOCK(bufferCacheLock);
      isCached = GetBufferDataFromCache(bind, opResult) != NULL;
      if(opResult == rdcspv::DeviceOpResult::NeedsDevice)
        return false;
    }

    if(!isCached)
    {
      // Add buffer data to the cache : cache should not be locked by this thread
      PopulateBuffer(bind);
    }

    {
      SCOPED_READLOCK(bufferCacheLock);
      bytebuf *result = GetBufferDataFromCache(bind, opResult);
      if(result)
      {
        // Guarantee the buffer cache readlock whilst the function is called
        func(result);
        return true;
      }

      RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Failed);
      opResult = rdcspv::DeviceOpResult::Failed;
      return false;
    }
  }

  // Called from any thread
  bool BufferFunction(uint64_t address, const std::function<void(bytebuf *data, size_t offset)> &func,
                      rdcspv::DeviceOpResult &opResult)
  {
    size_t offs = 0;
    ShaderBindIndex bind = GenerateBufferBind(address, offs);
    bool isCached = false;
    {
      SCOPED_READLOCK(bufferCacheLock);
      isCached = GetBufferDataFromCache(bind, opResult) != NULL;
      if(opResult == rdcspv::DeviceOpResult::NeedsDevice)
        return false;
    }

    if(!isCached)
    {
      // Add buffer data to the cache : cache should not be locked by this thread
      PopulateBuffer(address, bind);
    }

    {
      SCOPED_READLOCK(bufferCacheLock);
      bytebuf *result = GetBufferDataFromCache(bind, opResult);
      if(result)
      {
        // Guarantee the buffer cache readlock whilst the function is called
        func(result, offs);
        return true;
      }

      RDCASSERTEQUAL(opResult, rdcspv::DeviceOpResult::Failed);
      opResult = rdcspv::DeviceOpResult::Failed;
      return false;
    }
  }

  // Must be called from the replay manager thread (the debugger thread)
  void PopulateBuffer(uint64_t address, ShaderBindIndex bind)
  {
    CHECK_DEVICE_THREAD();
    // pick a non-overlapping bind namespace for direct pointer access
    ResourceId id;
    uint64_t ptrOffs;

    m_pDriver->GetResIDFromAddr(address, id, ptrOffs);
    if(id == ResourceId())
    {
      ShaderBindIndex noBind;
      CHECK_DEVICE_THREAD();
      auto insertIt = bufferCache.insert(std::make_pair(noBind, bytebuf()));
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("invalid or OOB pointer access detected ."));
      return;
    }

    // if the resources might be dirty from side-effects from the action, replay back to right
    // before it.
    if(m_ResourcesDirty)
    {
      VkMarkerRegion region("un-dirtying resources");
      m_pDriver->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
      m_ResourcesDirty = false;
    }

    bytebuf data;
    m_pDriver->GetDebugManager()->GetBufferData(id, 0, 0, data);

    {
      // Insert atomically with all the data filled in : to prevent race conditions
      SCOPED_WRITELOCK(bufferCacheLock);
      auto insertIt = bufferCache.insert(std::make_pair(bind, data));
      RDCASSERT(insertIt.second);
    }
  }

  // Must be called from the replay manager thread (the debugger thread)
  void PopulateBuffer(const ShaderBindIndex &bind)
  {
    CHECK_DEVICE_THREAD();
    bytebuf data;

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

    {
      // Insert atomically with all the data filled in : to prevent race conditions
      SCOPED_WRITELOCK(bufferCacheLock);
      auto insertIt = bufferCache.insert(std::make_pair(bind, data));
      RDCASSERT(insertIt.second);
    }
  }

  // Must be called from the replay manager thread (the debugger thread)
  void PopulateImage(const ShaderBindIndex &bind)
  {
    CHECK_DEVICE_THREAD();

    ImageData data;
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

      if(imgData.type == DescriptorType::TypedBuffer ||
         imgData.type == DescriptorType::ReadWriteTypedBuffer)
      {
        VkFormat format;
        VkDeviceSize byteWidth;
        VkDeviceSize offset;
        ResourceId buffer;

        if(imgData.view == ResourceId())
        {
          // descriptor buffer, no buffer view
          buffer = m_pDriver->GetResourceManager()->GetLiveID(imgData.resource);
          offset = imgData.byteOffset;
          format = MakeVkFormat(imgData.format);
          byteWidth = imgData.byteSize;
        }
        else
        {
          const VulkanCreationInfo::BufferView &viewProps =
              m_Creation.GetBufferViewInfo(m_pDriver->GetResourceManager()->GetLiveID(imgData.view));
          buffer = viewProps.buffer;
          offset = viewProps.offset;
          format = viewProps.format;
          byteWidth = viewProps.size;
        }

        VulkanCreationInfo::Buffer defaultBufferProps = {};
        const VulkanCreationInfo::Buffer &bufferProps =
            (buffer == ResourceId()) ? defaultBufferProps : m_Creation.GetBufferInfo(buffer);

        // width in bytes, either from the view or from the remainder of the buffer
        if(byteWidth == VK_WHOLE_SIZE)
          byteWidth = bufferProps.size - offset;

        data.fmt = MakeResourceFormat(format);
        data.texelSize = (uint32_t)GetByteSize(1, 1, 1, format, 0);

        // convert to a texel width, rounding down as per spec (only possible from VK_WHOLE_SIZE)
        data.width = uint32_t(byteWidth / data.texelSize);
        data.height = 1;
        data.depth = 1;

        data.samplePitch = data.slicePitch = data.rowPitch = data.width * data.texelSize;

        m_pDriver->GetReplay()->GetBufferData(
            m_pDriver->GetResourceManager()->GetLiveID(imgData.resource), offset, data.rowPitch,
            data.bytes);
      }
      else if(imgData.view != ResourceId())
      {
        const VulkanCreationInfo::ImageView &viewProps =
            m_Creation.GetImageViewInfo(m_pDriver->GetResourceManager()->GetLiveID(imgData.view));
        if(viewProps.image != ResourceId())
        {
          const VulkanCreationInfo::Image &imageProps = m_Creation.GetImageInfo(viewProps.image);

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

    {
      // Insert atomically with all the data filled in : to prevent race conditions
      SCOPED_WRITELOCK(imageCacheLock);
      auto insertIt = imageCache.insert(std::make_pair(bind, data));
      RDCASSERT(insertIt.second);
    }
  }

  // Called from any thread
  bool IsImageCached(const ShaderBindIndex &bind) override
  {
    SCOPED_READLOCK(imageCacheLock);
    return imageCache.find(bind) != imageCache.end();
  }

  // Called from any thread
  ImageData *GetImageDataFromCache(const ShaderBindIndex &bind, rdcspv::DeviceOpResult &opResult)
  {
    // Calling function responsible for acquiring imageCache Read lock
    auto findIt = imageCache.find(bind);
    if(findIt != imageCache.end())
    {
      opResult = rdcspv::DeviceOpResult::Succeeded;
      return &findIt->second;
    }

    opResult = rdcspv::DeviceOpResult::Failed;

    // Not in the cache : populate must happen on the device thread
    if(!IsDeviceThread())
      opResult = rdcspv::DeviceOpResult::NeedsDevice;

    return NULL;
  }

  VkPipeline MakePipe(const ShaderConstParameters &params, uint32_t floatBitSize, bool depthTex,
                      bool uintTex, bool sintTex)
  {
    CHECK_DEVICE_THREAD();
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
      CHECK_VKR(m_pDriver, vkr);

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
    CHECK_DEVICE_THREAD();
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
    CHECK_DEVICE_THREAD();
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

  const uint64_t deviceThreadID;
};

enum class InputSpecConstant
{
  Address = 0,
  AddressMSB,
  ArrayLength,
  DestX,
  DestY,
  DestThreadIDX,
  DestThreadIDY,
  DestThreadIDZ,
  DestInstance,
  DestVertex,
  DestView,
  Count,
};

struct SpecData
{
  VkDeviceAddress bufferAddress;
  uint32_t arrayLength;
  uint32_t destVertex;
  uint32_t destInstance;
  uint32_t destView;
  float destX;
  float destY;
  uint32_t globalThreadIdX;
  uint32_t globalThreadIdY;
  uint32_t globalThreadIdZ;
};

static const VkSpecializationMapEntry specMapsTemplate[] = {
    {
        (uint32_t)InputSpecConstant::Address,
        offsetof(SpecData, bufferAddress),
        // EXT_bda uses a 64-bit constant, as well as KHR_bda64
        0,
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
        (uint32_t)InputSpecConstant::DestThreadIDX,
        offsetof(SpecData, globalThreadIdX),
        sizeof(SpecData::globalThreadIdX),
    },
    {
        (uint32_t)InputSpecConstant::DestThreadIDY,
        offsetof(SpecData, globalThreadIdY),
        sizeof(SpecData::globalThreadIdY),
    },
    {
        (uint32_t)InputSpecConstant::DestThreadIDZ,
        offsetof(SpecData, globalThreadIdZ),
        sizeof(SpecData::globalThreadIdZ),
    },
    {
        (uint32_t)InputSpecConstant::DestInstance,
        offsetof(SpecData, destInstance),
        sizeof(SpecData::destInstance),
    },
    {
        (uint32_t)InputSpecConstant::DestVertex,
        offsetof(SpecData, destVertex),
        sizeof(SpecData::destVertex),
    },
    {
        (uint32_t)InputSpecConstant::DestView,
        offsetof(SpecData, destView),
        sizeof(SpecData::destView),
    },
    {
        (uint32_t)InputSpecConstant::AddressMSB,
        offsetof(SpecData, bufferAddress) + 4,
        sizeof(uint32_t),
    },
};

RDCCOMPILE_ASSERT((size_t)InputSpecConstant::Count == ARRAY_COUNT(specMapsTemplate),
                  "Spec constants changed");

enum class SubgroupCapability : uint32_t
{
  None = 0,
  EXTBallot,
  Vulkan1_1_NoBallot,
  Vulkan1_1,
};

static const uint32_t validMagicNumber = 12345;
static const uint32_t NumReservedBindings = 1;

// things we need to readback once per hit thread
struct ResultDataBase
{
  Vec4f pos;

  uint32_t prim;
  uint32_t sample;
  uint32_t view;
  uint32_t valid;

  float ddxDerivCheck;
  uint32_t quadLaneIndex;
  uint32_t laneIndex;
  uint32_t subgroupSize;

  uint32_t globalBallot[4];
  uint32_t electBallot[4];
  uint32_t helperBallot[4];

  uint32_t numSubgroups;    // may be packed oddly so we don't assume we can calculate
  uint32_t padding[3];

  // LaneData lanes[N]
  // each LaneData is prefixed by the subgroup struct below if needed, and then the stage struct unconditionally
};

// things we need per-lane with subgroups active, before any per-stage data
struct SubgroupLaneData
{
  uint32_t elect;       // for OpGroupNonUniformElect, if we don't have ballot
  uint32_t isActive;    // per lane active mask
  uint32_t padding[2];
};

struct VertexLaneData
{
  uint32_t inst;    // allow/expect instance to vary across subgroup just in case
  uint32_t vert;    // vertex id (either auto-generated or index)
  uint32_t view;    // multiview view (if used)
  uint32_t padding;
};

struct PixelLaneData
{
  Vec4f fragCoord;      // per-lane coord
  uint32_t isHelper;    // per-lane helper bit
  uint32_t quadId;    // the per-quad ID shared among all 4 threads, to differentiate between quads.
                      // is the laneIndex of the top-left thread (with an offset, so we can see 0 as invalid)
  uint32_t quadLaneIndex;    // the quadLaneIndex for quad-neighbours, in case we are fetching a subgroup
  uint32_t padding;
};

struct ComputeLaneData
{
  uint32_t threadid[3];    // per-lane thread id (in case it's not trivial)
  uint32_t subIdxInGroup;
};

// we use the message passing method from the quadoverdraw to swap data between quad neighbours
// using fine derivatives. This is based on "Shader Amortization using Pixel Quad Message Passing",
// Eric Penner, GPU Pro 2.
//
// broadly, if we take ddx_fine and either add or subtract it we can swap horizontal information,
// and similarly vertical for ddy_fine. To swap across the diagonal we perform one fine derivative
// swap, and then use the other fine derivative on the result of that swap.
//
// +---+---+
// | 0 | 1 |
// +---+---+
// | 2 | 3 |
// +---+---+
//
// fine derivatives are obtained by subtracting the right-most neighbour from left-most in each row,
// and the bottom-most neighbour from top-most in each column.
//
// the pseudocode is as follows (following the quad overdraw closely) where X is the type of our value:
//
//
// bool quadX = (quadLaneIndex & 1) != 0;
// bool quadY = (quadLaneIndex & 2) != 0;
//
// bool readX = (readIndex & 1) != 0;
// bool readY = (readIndex & 2) != 0;
//
// float sign_x = quadX ? -1 : 1;
// float sign_y = quadY ? -1 : 1;
//
// X c1 = c0 + sign_x * ddx_fine(c0);
// X c2 = c0 + sign_y * ddy_fine(c0);
// X c3 = c2 + sign_x * ddx_fine(c2);
//
// if(readIndex == quadLaneIndex) // identity, handle gracefully
//   return c0;
// else if(readY == quadY) // horizontal neighbour
//   return c1;
// else if(readX == quadX) // vertical neighbour
//	return c2;
// else
//   return c3; // diagonal neighbour
//

static rdcspv::Id AddQuadSwizzleHelper(rdcspv::Editor &editor, uint32_t count)
{
  rdcspv::Id func = editor.MakeId();

  rdcspv::OperationList ops;

  rdcspv::Id u32 = editor.DeclareType(rdcspv::scalar<uint32_t>());

  rdcspv::Id type;
  if(count == 1)
    type = editor.DeclareType(rdcspv::scalar<float>());
  else
    type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), count));

  rdcspv::Id funcType = editor.DeclareType(rdcspv::FunctionType(type, {type, u32, u32}));

  ops.add(rdcspv::OpFunction(type, func, rdcspv::FunctionControl::None, funcType));
  rdcspv::Id c0 = ops.add(rdcspv::OpFunctionParameter(type, editor.MakeId()));
  rdcspv::Id quadLaneIndex = ops.add(rdcspv::OpFunctionParameter(u32, editor.MakeId()));
  rdcspv::Id readIndex = ops.add(rdcspv::OpFunctionParameter(u32, editor.MakeId()));
  ops.add(rdcspv::OpLabel(editor.MakeId()));

  editor.SetName(c0, "c0");
  editor.SetName(quadLaneIndex, "quadLaneIndex");
  editor.SetName(readIndex, "readIndex");

  rdcspv::Id zero = editor.AddConstantImmediate<uint32_t>(0U);
  rdcspv::Id one = editor.AddConstantImmediate<uint32_t>(1U);
  rdcspv::Id two = editor.AddConstantImmediate<uint32_t>(2U);

  rdcspv::Id posOne = editor.AddConstantImmediate<float>(1.0f);
  rdcspv::Id negOne = editor.AddConstantImmediate<float>(-1.0f);

  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());

  rdcspv::Id quadX = ops.add(rdcspv::OpBitwiseAnd(u32, editor.MakeId(), quadLaneIndex, one));
  quadX = ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), quadX, zero));
  rdcspv::Id quadY = ops.add(rdcspv::OpBitwiseAnd(u32, editor.MakeId(), quadLaneIndex, two));
  quadY = ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), quadY, zero));

  rdcspv::Id readX = ops.add(rdcspv::OpBitwiseAnd(u32, editor.MakeId(), readIndex, one));
  readX = ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), readX, zero));
  rdcspv::Id readY = ops.add(rdcspv::OpBitwiseAnd(u32, editor.MakeId(), readIndex, two));
  readY = ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), readY, zero));

  rdcspv::Id horizNeighbour =
      ops.add(rdcspv::OpLogicalEqual(boolType, editor.MakeId(), readY, quadY));
  rdcspv::Id vertNeighbour = ops.add(rdcspv::OpLogicalEqual(boolType, editor.MakeId(), readX, quadX));
  rdcspv::Id isIdentity =
      ops.add(rdcspv::OpIEqual(boolType, editor.MakeId(), quadLaneIndex, readIndex));

  editor.SetName(quadX, "quadX");
  editor.SetName(quadY, "quadY");
  editor.SetName(readX, "readX");
  editor.SetName(readY, "readY");

  if(count >= 2)
  {
    rdcspv::Id floatNtype = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), count));
    rdcspv::Id boolNtype = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), count));

    rdcarray<rdcspv::Id> bcast;

    bcast.fill(count, posOne);
    posOne = ops.add(rdcspv::OpCompositeConstruct(floatNtype, editor.MakeId(), bcast));
    bcast.fill(count, negOne);
    negOne = ops.add(rdcspv::OpCompositeConstruct(floatNtype, editor.MakeId(), bcast));

    bcast.fill(count, quadX);
    quadX = ops.add(rdcspv::OpCompositeConstruct(boolNtype, editor.MakeId(), bcast));
    bcast.fill(count, quadY);
    quadY = ops.add(rdcspv::OpCompositeConstruct(boolNtype, editor.MakeId(), bcast));
  }

  rdcspv::Id sign_x = ops.add(rdcspv::OpSelect(type, editor.MakeId(), quadX, negOne, posOne));
  rdcspv::Id sign_y = ops.add(rdcspv::OpSelect(type, editor.MakeId(), quadY, negOne, posOne));
  editor.SetName(sign_x, "sign_x");
  editor.SetName(sign_y, "sign_y");

  rdcspv::Id ddxFine = ops.add(rdcspv::OpDPdxFine(type, editor.MakeId(), c0));
  rdcspv::Id ddyFine = ops.add(rdcspv::OpDPdyFine(type, editor.MakeId(), c0));

  rdcspv::Id c1 = ops.add(rdcspv::OpFMul(type, editor.MakeId(), sign_x, ddxFine));
  c1 = ops.add(rdcspv::OpFAdd(type, editor.MakeId(), c0, c1));
  editor.SetName(c1, "c1");

  rdcspv::Id c2 = ops.add(rdcspv::OpFMul(type, editor.MakeId(), sign_y, ddyFine));
  c2 = ops.add(rdcspv::OpFAdd(type, editor.MakeId(), c0, c2));
  editor.SetName(c2, "c2");

  rdcspv::Id ddxC2 = ops.add(rdcspv::OpDPdxFine(type, editor.MakeId(), c2));

  rdcspv::Id c3 = ops.add(rdcspv::OpFMul(type, editor.MakeId(), sign_x, ddxC2));
  c3 = ops.add(rdcspv::OpFAdd(type, editor.MakeId(), c2, c3));
  editor.SetName(c3, "c3");

  rdcspv::Id trueBranch = editor.MakeId(), falseBranch = editor.MakeId(),
             mergeBranch = editor.MakeId();

  // we'll want to read lane 0 on all lanes for the quadId, so handle that specially
  ops.add(rdcspv::OpSelectionMerge(mergeBranch, rdcspv::SelectionControl::None));
  ops.add(rdcspv::OpBranchConditional(isIdentity, trueBranch, falseBranch));

  ops.add(rdcspv::OpLabel(trueBranch));
  ops.add(rdcspv::OpReturnValue(c0));
  ops.add(rdcspv::OpLabel(falseBranch));
  ops.add(rdcspv::OpBranch(mergeBranch));
  ops.add(rdcspv::OpLabel(mergeBranch));

  // expanded flow control. Impossible labels happen after returns on both branches
  //
  // if(isIdentity) {
  //   ident;
  // }
  // notident;
  //
  // if(horizNeighbour) {
  //   horiz;
  // } else {
  //   notHoriz;
  //   if(vertNeighbour) { vert; } else { diagonal; }
  //   impossible1;
  // }
  // impossible2;
  rdcspv::Id horiz = editor.MakeId(), notHoriz = editor.MakeId(), vert = editor.MakeId(),
             diagonal = editor.MakeId(), imposs1 = editor.MakeId(), imposs2 = editor.MakeId();

  ops.add(rdcspv::OpSelectionMerge(imposs2, rdcspv::SelectionControl::None));
  ops.add(rdcspv::OpBranchConditional(horizNeighbour, horiz, notHoriz));

  ops.add(rdcspv::OpLabel(horiz));
  ops.add(rdcspv::OpReturnValue(c1));
  ops.add(rdcspv::OpLabel(notHoriz));

  ops.add(rdcspv::OpSelectionMerge(imposs1, rdcspv::SelectionControl::None));
  ops.add(rdcspv::OpBranchConditional(vertNeighbour, vert, diagonal));

  ops.add(rdcspv::OpLabel(vert));
  ops.add(rdcspv::OpReturnValue(c2));
  ops.add(rdcspv::OpLabel(diagonal));
  ops.add(rdcspv::OpReturnValue(c3));

  ops.add(rdcspv::OpLabel(imposs1));
  ops.add(rdcspv::OpUnreachable());
  ops.add(rdcspv::OpLabel(imposs2));
  ops.add(rdcspv::OpUnreachable());

  ops.add(rdcspv::OpFunctionEnd());

  editor.AddFunction(ops);
  editor.SetName(func, "quadSwizzleHelper");

  return func;
}

static void CreateInputFetcher(rdcarray<uint32_t> &spv,
                               VulkanCreationInfo::ShaderModuleReflection &shadRefl,
                               BufferStorageMode storageMode, bool usePrimitiveID, bool useSampleID,
                               bool useViewIndex, SubgroupCapability subgroupCapability,
                               uint32_t maxSubgroupSize)
{
  rdcspv::Editor editor(spv);

  ShaderStage stage = ShaderStage(shadRefl.stageIndex);
  rdcspv::ThreadScope threadScope = shadRefl.patchData.threadScope;

  uint32_t paramAlign = 16;

  for(const SigParameter &sig : shadRefl.refl->inputSignature)
  {
    if(VarTypeByteSize(sig.varType) * sig.compCount > paramAlign)
      paramAlign = 32;
  }

  // conservatively calculate structure stride with full amount for every input element
  uint32_t structStride = (uint32_t)shadRefl.refl->inputSignature.size() * paramAlign;

  switch(stage)
  {
    case ShaderStage::Vertex: structStride += sizeof(VertexLaneData); break;
    case ShaderStage::Pixel: structStride += sizeof(PixelLaneData); break;
    case ShaderStage::Task:
    case ShaderStage::Mesh:
    case ShaderStage::Compute: structStride += sizeof(ComputeLaneData); break;
    default: break;
  }

  if(threadScope & rdcspv::ThreadScope::Subgroup)
  {
    structStride += sizeof(SubgroupLaneData);
  }

  // simulating full subgroups with ballot ability to read other lanes, we read all lanes data
  const bool fullSubgroups = (subgroupCapability == SubgroupCapability::EXTBallot ||
                              subgroupCapability == SubgroupCapability::Vulkan1_1) &&
                             (threadScope & rdcspv::ThreadScope::Subgroup);
  // faking subgroups without reading a subgroup's worth of data but we still read lane index and elect value
  const bool minimalSubgroups = subgroupCapability != SubgroupCapability::None &&
                                (threadScope & rdcspv::ThreadScope::Subgroup);

  editor.Prepare();
  editor.SetBufferStorageMode(storageMode);

  // remove any OpSource
  {
    // remove any OpName that refers to deleted IDs - functions or results
    rdcspv::Iter it = editor.Begin(rdcspv::Section::DebugStringSource);
    rdcspv::Iter end = editor.End(rdcspv::Section::DebugStringSource);
    while(it < end)
    {
      if(it.opcode() == rdcspv::Op::Source || it.opcode() == rdcspv::Op::SourceContinued)
      {
        editor.Remove(it);
      }
      it++;
    }
  }

  editor.OffsetBindingsToMatchReservation(NumReservedBindings);

  // the original entry ID we're patching
  rdcspv::Id originalEntry = editor.FindEntryID({shadRefl.entryPoint, stage});
  // the new wrapped entry function we'll add
  rdcspv::Id entryID = editor.MakeId();

  // repoint the entry declaration
  editor.ChangeEntry(originalEntry, entryID);

  rdcspv::MemoryAccessAndParamDatas alignedAccess;
  alignedAccess.setAligned(sizeof(uint32_t));

  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id floatType = editor.DeclareType(rdcspv::scalar<float>());
  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());
  rdcspv::Id uint3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
  rdcspv::Id uint4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 4));
  rdcspv::Id float4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));
  rdcspv::Id float3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 3));
  rdcspv::Id float2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 2));
  rdcspv::Id bool4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), 4));

  rdcarray<rdcspv::Id> newGlobals;

  rdcspv::Id LaneDataStruct;

  enum ResultBaseMember
  {
    ResultBase_pos,
    ResultBase_prim,
    ResultBase_sample,
    ResultBase_view,
    ResultBase_valid,
    ResultBase_ddxDerivCheck,
    ResultBase_quadLaneIndex,
    ResultBase_laneIndex,
    ResultBase_subgroupSize,
    ResultBase_globalBallot,
    ResultBase_electBallot,
    ResultBase_helperBallot,
    ResultBase_numSubgroups,
    ResultBase_firstUser,
  };

  struct laneValue
  {
    rdcstr name;
    // index in the LaneData struct
    size_t structIndex;
    // type ID of the value (float4, uint, etc)
    rdcspv::Id type;
    // direct loaded value per-lane
    rdcspv::Id base;

    // for pixel shaders, the quad's worth of swizzled data to load helper info from, whether or not
    // we have subgroups active
    rdcarray<rdcspv::Id> quadSwizzledData;

    // the loadOps to prepare the base value, done separately so we can push each fixed value into
    // an array while creating the struct to all be processed agnostically, instead of separating adding
    // them to the struct from loading them OR interleaving struct definition while preparing our function
    rdcspv::OperationList loadOps;

    bool flat = false;
  };

  rdcarray<laneValue> laneValues;

  rdcspv::Id subgroupScope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Subgroup);

  rdcspv::Id isHelper, quadLaneIndex, quadId;

  {
    rdcarray<rdcspv::StructMember> structMembers;
    uint32_t offset = 0;

    // declare fixed lane data first

    if(threadScope & rdcspv::ThreadScope::Subgroup)
    {
      laneValue elect;
      elect.name = "__rd_globalElect";
      elect.structIndex = structMembers.size();
      elect.type = uint32Type;
      // don't store elect value for legacy KHR path
      if(subgroupCapability == SubgroupCapability::Vulkan1_1 ||
         subgroupCapability == SubgroupCapability::Vulkan1_1_NoBallot)
      {
        elect.base = elect.loadOps.add(
            rdcspv::OpGroupNonUniformElect(boolType, editor.MakeId(), subgroupScope));
        elect.base = elect.loadOps.add(rdcspv::OpSelect(uint32Type, editor.MakeId(), elect.base,
                                                        editor.AddConstantImmediate<uint32_t>(1),
                                                        editor.AddConstantImmediate<uint32_t>(0)));
        editor.SetName(elect.base, elect.name);
      }
      else
      {
        elect.base = editor.AddConstantImmediate<uint32_t>(0);
        elect.flat = true;
      }
      laneValues.push_back(elect);
      structMembers.push_back(
          {uint32Type, elect.name, offset + (uint32_t)offsetof(SubgroupLaneData, elect)});

      // we implicitly only write data for active lanes so we just set isActive to 1 always
      laneValue isActive;
      isActive.name = "__rd_active";
      isActive.structIndex = structMembers.size();
      isActive.type = uint32Type;
      isActive.base = editor.AddConstantImmediate<uint32_t>(1);
      isActive.flat = true;
      laneValues.push_back(isActive);
      structMembers.push_back(
          {uint32Type, isActive.name, offset + (uint32_t)offsetof(SubgroupLaneData, isActive)});

      structMembers.push_back(
          {uint32Type, "__pad", offset + (uint32_t)offsetof(SubgroupLaneData, padding)});
      structMembers.push_back(
          {uint32Type, "__pad",
           uint32_t(offset + offsetof(SubgroupLaneData, padding) + sizeof(uint32_t))});

      offset += sizeof(SubgroupLaneData);
      RDCCOMPILE_ASSERT(
          (sizeof(SubgroupLaneData) / sizeof(Vec4f)) * sizeof(Vec4f) == sizeof(SubgroupLaneData),
          "SubgroupLaneData is misaligned, ensure 16-byte aligned");
    }

    if(stage == ShaderStage::Vertex)
    {
      laneValue inst;
      inst.name = "__rd_inst";
      inst.structIndex = structMembers.size();
      inst.type = uint32Type;
      inst.base = editor.AddBuiltinInputLoad(inst.loadOps, newGlobals, stage,
                                             rdcspv::BuiltIn::InstanceIndex, uint32Type);
      editor.SetName(inst.base, inst.name);
      laneValues.push_back(inst);
      structMembers.push_back(
          {uint32Type, inst.name, offset + (uint32_t)offsetof(VertexLaneData, inst)});

      laneValue vert;
      vert.name = "__rd_vert";
      vert.structIndex = structMembers.size();
      vert.type = uint32Type;
      vert.base = editor.AddBuiltinInputLoad(vert.loadOps, newGlobals, stage,
                                             rdcspv::BuiltIn::VertexIndex, uint32Type);
      editor.SetName(vert.base, vert.name);
      laneValues.push_back(vert);
      structMembers.push_back(
          {uint32Type, vert.name, offset + (uint32_t)offsetof(VertexLaneData, vert)});

      if(useViewIndex)
      {
        laneValue view;
        view.name = "__rd_view";
        view.structIndex = structMembers.size();
        view.type = uint32Type;
        view.base = editor.AddBuiltinInputLoad(view.loadOps, newGlobals, stage,
                                               rdcspv::BuiltIn::ViewIndex, uint32Type);
        editor.SetName(view.base, view.name);
        laneValues.push_back(view);
        structMembers.push_back(
            {uint32Type, view.name, offset + (uint32_t)offsetof(VertexLaneData, view)});

        structMembers.push_back(
            {uint32Type, "__pad", offset + (uint32_t)offsetof(VertexLaneData, padding)});
      }
      else
      {
        structMembers.push_back(
            {uint32Type, "__rd_view", offset + (uint32_t)offsetof(VertexLaneData, view)});
        structMembers.push_back(
            {uint32Type, "__pad", offset + (uint32_t)offsetof(VertexLaneData, padding)});
      }

      offset += sizeof(VertexLaneData);
      RDCCOMPILE_ASSERT(
          (sizeof(VertexLaneData) / sizeof(Vec4f)) * sizeof(Vec4f) == sizeof(VertexLaneData),
          "VertexLaneData is misaligned, ensure 16-byte aligned");
    }
    else if(stage == ShaderStage::Pixel)
    {
      laneValue fragCoord;
      fragCoord.name = "__rd_pixelPos";
      fragCoord.structIndex = structMembers.size();
      fragCoord.type = float4Type;
      fragCoord.base = editor.AddBuiltinInputLoad(fragCoord.loadOps, newGlobals, stage,
                                                  rdcspv::BuiltIn::FragCoord, float4Type);
      editor.SetName(fragCoord.base, fragCoord.name);
      laneValues.push_back(fragCoord);
      structMembers.push_back(
          {float4Type, fragCoord.name, offset + (uint32_t)offsetof(PixelLaneData, fragCoord)});

      laneValue helper;
      helper.name = "__rd_isHelper";
      helper.structIndex = structMembers.size();
      helper.type = uint32Type;
      helper.base = editor.AddBuiltinInputLoad(helper.loadOps, newGlobals, stage,
                                               rdcspv::BuiltIn::HelperInvocation, boolType);
      helper.base = helper.loadOps.add(rdcspv::OpSelect(uint32Type, editor.MakeId(), helper.base,
                                                        editor.AddConstantImmediate<uint32_t>(1),
                                                        editor.AddConstantImmediate<uint32_t>(0)));
      editor.SetName(helper.base, helper.name);
      laneValues.push_back(helper);
      structMembers.push_back(
          {uint32Type, helper.name, offset + (uint32_t)offsetof(PixelLaneData, isHelper)});

      laneValue quad;
      quad.name = "__rd_quadId";
      quad.structIndex = structMembers.size();
      quad.type = uint32Type;
      quad.flat = true;
      // this will be handled specially similarly to helper
      quad.base = editor.MakeId();
      editor.SetName(quad.base, quad.name);
      laneValues.push_back(quad);
      structMembers.push_back(
          {uint32Type, quad.name, offset + (uint32_t)offsetof(PixelLaneData, quadId)});

      laneValue quadLane;
      quadLane.name = "__rd_quadLane";
      quadLane.structIndex = structMembers.size();
      quadLane.type = uint32Type;
      quadLane.base = editor.MakeId();
      editor.SetName(quadLane.base, quadLane.name);
      laneValues.push_back(quadLane);
      structMembers.push_back(
          {uint32Type, quadLane.name, offset + (uint32_t)offsetof(PixelLaneData, quadLaneIndex)});

      // quad properties will be handled specially
      isHelper = helper.base;
      quadId = quad.base;
      quadLaneIndex = quadLane.base;

      structMembers.push_back(
          {uint32Type, "__pad", offset + (uint32_t)offsetof(PixelLaneData, padding)});

      offset += sizeof(PixelLaneData);
      RDCCOMPILE_ASSERT(
          (sizeof(PixelLaneData) / sizeof(Vec4f)) * sizeof(Vec4f) == sizeof(PixelLaneData),
          "PixelLaneData is misaligned, ensure 16-byte aligned");
    }
    else if(stage == ShaderStage::Compute || stage == ShaderStage::Task || stage == ShaderStage::Mesh)
    {
      laneValue threadid;
      threadid.name = "__rd_threadid";
      threadid.structIndex = structMembers.size();
      threadid.type = uint3Type;
      threadid.base = editor.AddBuiltinInputLoad(threadid.loadOps, newGlobals, stage,
                                                 rdcspv::BuiltIn::LocalInvocationId, uint3Type);
      editor.SetName(threadid.base, threadid.name);
      laneValues.push_back(threadid);
      structMembers.push_back(
          {uint3Type, threadid.name, offset + (uint32_t)offsetof(ComputeLaneData, threadid)});

      laneValue subid;
      subid.name = "__rd_subgroupid";
      subid.structIndex = structMembers.size();
      subid.type = uint32Type;
      subid.base = editor.AddBuiltinInputLoad(subid.loadOps, newGlobals, stage,
                                              rdcspv::BuiltIn::SubgroupId, uint32Type);
      editor.SetName(subid.base, subid.name);
      laneValues.push_back(subid);
      structMembers.push_back(
          {uint32Type, subid.name, offset + (uint32_t)offsetof(ComputeLaneData, subIdxInGroup)});

      offset += sizeof(ComputeLaneData);
      RDCCOMPILE_ASSERT(
          (sizeof(ComputeLaneData) / sizeof(Vec4f)) * sizeof(Vec4f) == sizeof(ComputeLaneData),
          "ComputeLaneData is misaligned, ensure 16-byte aligned");
    }

    // now add input signature values

    for(size_t i = 0; i < shadRefl.refl->inputSignature.size(); i++)
    {
      const SPIRVInterfaceAccess &access = shadRefl.patchData.inputs[i];
      const SigParameter &param = shadRefl.refl->inputSignature[i];

      rdcspv::Scalar base = rdcspv::scalar(param.varType);

      uint32_t width = (base.width / 8);

      rdcspv::Id loadType;

      if(param.compCount == 1)
        loadType = editor.DeclareType(base);
      else
        loadType = editor.DeclareType(rdcspv::Vector(base, param.compCount));

      rdcspv::Id valueType;

      // treat bools as uints
      if(base.type == rdcspv::Op::TypeBool)
        width = 4;

      // we immediately upconvert any sub-32-bit types
      if(width < 4)
      {
        width = 4;
        base.width = 32;

        if(param.compCount == 1)
          valueType = editor.DeclareType(base);
        else
          valueType = editor.DeclareType(rdcspv::Vector(base, param.compCount));
      }
      else
      {
        valueType = loadType;
      }

      rdcarray<rdcspv::Id> accessIndices;
      for(uint32_t idx : access.accessChain)
        accessIndices.push_back(editor.AddConstantImmediate<uint32_t>(idx));

      rdcspv::Id inputPtrType =
          editor.DeclareType(rdcspv::Pointer(loadType, rdcspv::StorageClass::Input));

      laneValue value;
      value.name = param.varName;
      value.structIndex = structMembers.size();
      value.type = valueType;

      if(value.name.beginsWith("gl_"))
        value.name = "__rd_" + value.name.substr(3);

      // if we have no access chain it's a global pointer of the type we want, so just load
      // straight out of it
      rdcspv::Id ptr;
      if(accessIndices.empty())
        ptr = access.ID;
      else
        ptr = value.loadOps.add(
            rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), access.ID, accessIndices));

      value.base = value.loadOps.add(rdcspv::OpLoad(loadType, editor.MakeId(), ptr));
      if(valueType == boolType)
      {
        valueType = uint32Type;
        // can't store bools directly, need to convert to uint
        value.base = value.loadOps.add(rdcspv::OpSelect(valueType, editor.MakeId(), value.base,
                                                        editor.AddConstantImmediate<uint32_t>(1),
                                                        editor.AddConstantImmediate<uint32_t>(0)));
      }
      if(valueType != loadType)
      {
        if(VarTypeCompType(param.varType) == CompType::Float)
          value.base = value.loadOps.add(rdcspv::OpFConvert(valueType, editor.MakeId(), value.base));
        else if(VarTypeCompType(param.varType) == CompType::SInt)
          value.base = value.loadOps.add(rdcspv::OpSConvert(valueType, editor.MakeId(), value.base));
        else if(VarTypeCompType(param.varType) == CompType::UInt)
          value.base = value.loadOps.add(rdcspv::OpUConvert(valueType, editor.MakeId(), value.base));
      }
      editor.SetName(value.base, StringFormat::Fmt("__rd_base_%zu_%s", i, param.varName.c_str()));
      // non-float inputs are considered flat
      value.flat = VarTypeCompType(param.varType) != CompType::Float;

      // mark this as non-flat so we still derive it for helper lanes as it will vary
      if(param.systemValue == ShaderBuiltin::IndexInSubgroup)
        value.flat = false;

      laneValues.push_back(value);

      if(valueType == boolType)
        structMembers.push_back({uint32Type, value.name, offset});
      else
        structMembers.push_back({valueType, value.name, offset});
      offset += param.compCount * width;

      // align offset conservatively, to 16-byte aligned. We do this with explicit uints so we can
      // preview with spirv-cross (and because it doesn't cost anything particularly)
      uint32_t paddingWords = ((paramAlign - (offset % 16)) / 4) % 4;
      for(uint32_t p = 0; p < paddingWords; p++)
      {
        structMembers.push_back({uint32Type, "__pad", offset});
        offset += 4;
      }
    }

    RDCASSERT(offset <= structStride);

    LaneDataStruct = editor.DeclareStructType("__rd_LaneData", structMembers);
  }

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

  rdcspv::Id destThreadIDX =
      editor.AddSpecConstantImmediate<uint32_t>(0, (uint32_t)InputSpecConstant::DestThreadIDX);
  rdcspv::Id destThreadIDY =
      editor.AddSpecConstantImmediate<uint32_t>(0, (uint32_t)InputSpecConstant::DestThreadIDY);
  rdcspv::Id destThreadIDZ =
      editor.AddSpecConstantImmediate<uint32_t>(0, (uint32_t)InputSpecConstant::DestThreadIDZ);

  editor.SetName(destThreadIDX, "destThreadIDX");
  editor.SetName(destThreadIDY, "destThreadIDY");
  editor.SetName(destThreadIDZ, "destThreadIDZ");

  rdcspv::Id destThreadID = editor.AddConstant(rdcspv::OpSpecConstantComposite(
      uint3Type, editor.MakeId(), {destThreadIDX, destThreadIDY, destThreadIDZ}));

  editor.SetName(destThreadID, "destThreadID");

  rdcspv::Id destInstance =
      editor.AddSpecConstantImmediate<uint32_t>(0, (uint32_t)InputSpecConstant::DestInstance);
  rdcspv::Id destVertex =
      editor.AddSpecConstantImmediate<uint32_t>(0, (uint32_t)InputSpecConstant::DestVertex);
  rdcspv::Id destView =
      editor.AddSpecConstantImmediate<uint32_t>(0, (uint32_t)InputSpecConstant::DestView);

  editor.SetName(destInstance, "destInstance");
  editor.SetName(destVertex, "destVertex");

  rdcspv::Id ResultDataBaseType;

  uint32_t numLanes = 1;

  if(stage == ShaderStage::Pixel)
    numLanes = 4;

  if(threadScope & rdcspv::ThreadScope::Quad)
    numLanes = 4;

  // if we need full subgroup scope (and we have ballots to read the lanes) declare a subgroup's worth of data
  if(fullSubgroups)
    numLanes = RDCMAX(numLanes, maxSubgroupSize);

  // note we don't need to care about workgroup access - that is only possible on compute and we fill
  // in the rest of the workgroup without reading its inputs, since on compute the only subgroup data
  // we need is size + layout and we assume we can figure out the layout with one subgroup's worth of data

  {
    rdcarray<rdcspv::StructMember> members;

    members.push_back({float4Type, "pos", offsetof(ResultDataBase, pos)});
    members.push_back({uint32Type, "prim", offsetof(ResultDataBase, prim)});
    members.push_back({uint32Type, "sample", offsetof(ResultDataBase, sample)});
    members.push_back({uint32Type, "view", offsetof(ResultDataBase, view)});
    members.push_back({uint32Type, "valid", offsetof(ResultDataBase, valid)});
    members.push_back({floatType, "ddxDerivCheck", offsetof(ResultDataBase, ddxDerivCheck)});
    members.push_back({uint32Type, "quadLaneIndex", offsetof(ResultDataBase, quadLaneIndex)});
    members.push_back({uint32Type, "laneIndex", offsetof(ResultDataBase, laneIndex)});
    members.push_back({uint32Type, "subgroupSize", offsetof(ResultDataBase, subgroupSize)});
    members.push_back({uint4Type, "globalBallot", offsetof(ResultDataBase, globalBallot)});
    members.push_back({uint4Type, "electBallot", offsetof(ResultDataBase, electBallot)});
    members.push_back({uint4Type, "helperBallot", offsetof(ResultDataBase, helperBallot)});
    members.push_back({uint32Type, "numSubgroups", offsetof(ResultDataBase, numSubgroups)});

    // uint3 padding

    const uint32_t dataStart = (uint32_t)AlignUp(sizeof(ResultDataBase), sizeof(Vec4f));

    RDCASSERT((structStride % sizeof(Vec4f)) == 0);

    rdcspv::Id LaneDataArray = editor.AddType(rdcspv::OpTypeArray(
        editor.MakeId(), LaneDataStruct, editor.AddConstantImmediate<uint32_t>(numLanes)));
    editor.AddDecoration(rdcspv::OpDecorate(
        LaneDataArray, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(structStride)));

    members.push_back({LaneDataArray, "LaneData", dataStart});

    ResultDataBaseType = editor.DeclareStructType("ResultData", members);
  }

  rdcspv::Id ResultDataRTArray =
      editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), ResultDataBaseType));

  editor.AddDecoration(rdcspv::OpDecorate(ResultDataRTArray,
                                          rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(
                                              structStride * numLanes + sizeof(ResultDataBase))));

  rdcspv::Id bufBase =
      editor.DeclareStructType("__rd_HitStorage", {
                                                      {uint32Type, "hit_count", 0},
                                                      {uint32Type, "total_count", sizeof(uint32_t)},
                                                      {uint32Type, "dummy", sizeof(uint32_t) * 2},
                                                      // uint padding

                                                      {ResultDataRTArray, "hits", sizeof(Vec4f)},
                                                  });

  rdcspv::StorageClass bufferClass = editor.PrepareAddedBufferAccess();

  rdcpair<rdcspv::Id, rdcspv::Id> hitBuffer = editor.AddBufferVariable(
      newGlobals, bufBase, "__rd_HitBuffer", 0, (uint32_t)InputSpecConstant::Address, 0);

  RDCCOMPILE_ASSERT(
      uint32_t(InputSpecConstant::Address) + 1 == (uint32_t)InputSpecConstant::AddressMSB,
      "Address spec constant IDs must be contiguous with MSB second");

  rdcspv::Id float4InPtr =
      editor.DeclareType(rdcspv::Pointer(float4Type, rdcspv::StorageClass::Input));
  rdcspv::Id float4BufPtr = editor.DeclareType(rdcspv::Pointer(float4Type, bufferClass));

  rdcspv::Id uint32InPtr =
      editor.DeclareType(rdcspv::Pointer(uint32Type, rdcspv::StorageClass::Input));
  rdcspv::Id uint32BufPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));
  rdcspv::Id uint4BufPtr = editor.DeclareType(rdcspv::Pointer(uint4Type, bufferClass));
  rdcspv::Id floatBufPtr = editor.DeclareType(rdcspv::Pointer(floatType, bufferClass));

  rdcspv::Id glsl450 = editor.ImportExtInst("GLSL.std.450");

  // allow pixel shaders to use fine derivatives
  if(stage == ShaderStage::Pixel)
    editor.AddCapability(rdcspv::Capability::DerivativeControl);

  // declare capabilities we might need
  if(threadScope & rdcspv::ThreadScope::Subgroup)
  {
    if(subgroupCapability == SubgroupCapability::None)
    {
      // nothing, ignore, this could only happen if the shader uses only EXT_shader_subgroup_vote
      // which we treat as degenerate
    }
    else if(subgroupCapability == SubgroupCapability::EXTBallot)
    {
      // this should already be present but let's be sure
      editor.AddCapability(rdcspv::Capability::SubgroupBallotKHR);
    }
    else if(subgroupCapability == SubgroupCapability::Vulkan1_1_NoBallot)
    {
      // this should also already be present
      editor.AddCapability(rdcspv::Capability::GroupNonUniform);
    }
    else if(subgroupCapability == SubgroupCapability::Vulkan1_1)
    {
      editor.AddCapability(rdcspv::Capability::GroupNonUniform);

      // add this, the shader might not have used it but we need it to read other lanes
      editor.AddCapability(rdcspv::Capability::GroupNonUniformBallot);
      editor.AddCapability(rdcspv::Capability::GroupNonUniformVote);
    }
  }

  rdcspv::Id vecNType[5] = {rdcspv::Id(), floatType, float2Type, float3Type, float4Type};
  rdcspv::Id quadSwizzleHelper[5] = {};

  if(stage == ShaderStage::Pixel)
  {
    for(uint32_t i = 1; i <= 4; i++)
    {
      quadSwizzleHelper[i] = AddQuadSwizzleHelper(editor, i);
    }
  }

  {
    rdcspv::OperationList ops;

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());
    rdcspv::Id uintPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));

    rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Device);
    rdcspv::Id semantics =
        editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::MemorySemantics::AcquireRelease);

    ops.add(rdcspv::OpFunction(voidType, entryID, rdcspv::FunctionControl::None,
                               editor.DeclareType(rdcspv::FunctionType(voidType, {}))));

    rdcspv::Id structPtr;

    ops.add(rdcspv::OpLabel(editor.MakeId()));
    {
      structPtr = editor.LoadBufferVariable(ops, hitBuffer);

      // we store ddx as a derivative check - it is expected to be 1.0 so store that as fixed for other stages
      rdcspv::Id fragCoord, ddxDerivativeCheck = editor.AddConstantImmediate<float>(1.0f);
      rdcspv::Id laneIndex;

      // identify the candidate thread in a stage-specific way
      rdcspv::Id candidateThread;

      // prepare stage-specific inputs and condition
      if(stage == ShaderStage::Vertex)
      {
        // we should only be fetching data like this for full subgroups
        RDCASSERT(fullSubgroups);

        rdcspv::Id vert = editor.AddBuiltinInputLoad(ops, newGlobals, stage,
                                                     rdcspv::BuiltIn::VertexIndex, uint32Type);
        rdcspv::Id inst = editor.AddBuiltinInputLoad(ops, newGlobals, stage,
                                                     rdcspv::BuiltIn::InstanceIndex, uint32Type);

        rdcspv::Id equalVert = ops.add(rdcspv::OpIEqual(boolType, editor.MakeId(), vert, destVertex));
        rdcspv::Id equalInstance =
            ops.add(rdcspv::OpIEqual(boolType, editor.MakeId(), inst, destInstance));

        candidateThread =
            ops.add(rdcspv::OpLogicalAnd(boolType, editor.MakeId(), equalVert, equalInstance));

        if(useViewIndex)
        {
          rdcspv::Id view = editor.AddBuiltinInputLoad(ops, newGlobals, stage,
                                                       rdcspv::BuiltIn::ViewIndex, uint32Type);
          rdcspv::Id equalView = ops.add(rdcspv::OpIEqual(boolType, editor.MakeId(), view, destView));
          candidateThread =
              ops.add(rdcspv::OpLogicalAnd(boolType, editor.MakeId(), candidateThread, equalView));
        }
      }
      else if(stage == ShaderStage::Pixel)
      {
        fragCoord = editor.AddBuiltinInputLoad(ops, newGlobals, stage, rdcspv::BuiltIn::FragCoord,
                                               float4Type);

        ddxDerivativeCheck = ops.add(rdcspv::OpDPdx(float4Type, editor.MakeId(), fragCoord));
        editor.SetName(ddxDerivativeCheck, "ddxDerivativeCheck");
        ddxDerivativeCheck =
            ops.add(rdcspv::OpCompositeExtract(floatType, editor.MakeId(), ddxDerivativeCheck, {0}));
        editor.SetName(ddxDerivativeCheck, "ddxDerivativeCheck_x");

        // grab x and y
        rdcspv::Id fragXY = ops.add(
            rdcspv::OpVectorShuffle(float2Type, editor.MakeId(), fragCoord, fragCoord, {0, 1}));

        /*
        // figure out the TL pixel's coords and calculate our index relative to it. Assume even top
        // left (towards 0,0) though the spec does not guarantee this is the actual quad
        int yTL = y & (~1);

        // get the index of our desired pixel
        */

        rdcspv::Id mask = editor.AddConstantImmediate<uint32_t>(1);

        // int x01 = x & 1;
        rdcspv::Id xInt =
            ops.add(rdcspv::OpCompositeExtract(floatType, editor.MakeId(), fragXY, {0}));
        xInt = ops.add(rdcspv::OpConvertFToU(uint32Type, editor.MakeId(), xInt));
        rdcspv::Id x01 = ops.add(rdcspv::OpBitwiseAnd(uint32Type, editor.MakeId(), xInt, mask));

        // int y01 = y & 1;
        rdcspv::Id yInt =
            ops.add(rdcspv::OpCompositeExtract(floatType, editor.MakeId(), fragXY, {1}));
        yInt = ops.add(rdcspv::OpConvertFToU(uint32Type, editor.MakeId(), yInt));
        rdcspv::Id y01 = ops.add(rdcspv::OpBitwiseAnd(uint32Type, editor.MakeId(), yInt, mask));

        // int destIdx = x01 + 2 * y01;
        rdcspv::Id sum = ops.add(rdcspv::OpIMul(uint32Type, editor.MakeId(),
                                                editor.AddConstantImmediate<uint32_t>(2), y01));
        ops.add(rdcspv::OpIAdd(uint32Type, quadLaneIndex, sum, x01));
        laneIndex = quadLaneIndex;
        editor.SetName(quadLaneIndex, "quadLaneIndex");

        // bool candidateThread = all(abs(gl_FragCoord.xy - dest.xy) < 0.5f);
        rdcspv::Id bool2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), 2));

        // subtract frag coord from the destination co-ord in x-y to get relative
        rdcspv::Id fragXYRelative =
            ops.add(rdcspv::OpFSub(float2Type, editor.MakeId(), fragXY, destXY));

        // abs()
        rdcspv::Id fragXYAbs = ops.add(rdcspv::OpGLSL450(
            float2Type, editor.MakeId(), glsl450, rdcspv::GLSLstd450::FAbs, {fragXYRelative}));

        rdcspv::Id half = editor.AddConstantImmediate<float>(0.5f);
        rdcspv::Id threshold = editor.AddConstant(
            rdcspv::OpConstantComposite(float2Type, editor.MakeId(), {half, half}));

        // less than 0.5
        rdcspv::Id inPixelXY =
            ops.add(rdcspv::OpFOrdLessThan(bool2Type, editor.MakeId(), fragXYAbs, threshold));

        // both less than 0.5
        candidateThread = ops.add(rdcspv::OpAll(boolType, editor.MakeId(), inPixelXY));
      }
      else if(stage == ShaderStage::Compute || stage == ShaderStage::Task ||
              stage == ShaderStage::Mesh)
      {
        // we should only be fetching data like this for full subgroups
        RDCASSERT(fullSubgroups);

        rdcspv::Id globalThread = editor.AddBuiltinInputLoad(
            ops, newGlobals, stage, rdcspv::BuiltIn::GlobalInvocationId, uint3Type);

        rdcspv::Id bool3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), 3));
        rdcspv::Id equal3 =
            ops.add(rdcspv::OpIEqual(bool3Type, editor.MakeId(), globalThread, destThreadID));

        candidateThread = ops.add(rdcspv::OpAll(boolType, editor.MakeId(), equal3));
      }

      rdcspv::Id quadIdxConst[4] = {
          editor.AddConstantImmediate<uint32_t>(0),
          editor.AddConstantImmediate<uint32_t>(1),
          editor.AddConstantImmediate<uint32_t>(2),
          editor.AddConstantImmediate<uint32_t>(3),
      };

      // load all data per-thread and calculate quad swizzled neighbour data as needed
      for(laneValue &val : laneValues)
      {
        ops.append(val.loadOps);

        // for pixel shaders we always need to grab quad swizzled data.
        // we skip this for values we classify as flat as well as for the magic isHelper/quadLaneIndex
        // which are handled specially and will be fixed up later when we go to store these
        if(stage == ShaderStage::Pixel && val.base != isHelper && val.base != quadLaneIndex &&
           !val.flat)
        {
          val.quadSwizzledData.resize(4);

          const rdcspv::DataType &dataType = editor.GetDataType(val.type);

          if(dataType.IsU32())
          {
            for(uint32_t q = 0; q < 4; q++)
            {
              rdcspv::Id valQ = val.base;
              valQ = ops.add(rdcspv::OpConvertUToF(floatType, editor.MakeId(), valQ));
              valQ = ops.add(rdcspv::OpFunctionCall(floatType, editor.MakeId(), quadSwizzleHelper[1],
                                                    {valQ, quadLaneIndex, quadIdxConst[q]}));
              // named as convenience because spirv-cross declares a variable here and then does the cast at the usage
              editor.SetName(valQ, StringFormat::Fmt("%s_swiz%u", val.name.c_str(), q));

              valQ = ops.add(rdcspv::OpConvertFToU(uint32Type, editor.MakeId(), valQ));
              editor.SetName(valQ, StringFormat::Fmt("%s_swiz%u_u", val.name.c_str(), q));

              val.quadSwizzledData[q] = valQ;
            }
          }
          else
          {
            // all other inputs that aren't uint32 should be floats, otherwise they should have been marked as flat
            RDCASSERT(dataType.scalar().type == rdcspv::Op::TypeFloat);

            uint32_t width = RDCMAX(1U, dataType.vector().count);

            for(uint32_t q = 0; q < 4; q++)
            {
              val.quadSwizzledData[q] = ops.add(
                  rdcspv::OpFunctionCall(vecNType[width], editor.MakeId(), quadSwizzleHelper[width],
                                         {val.base, quadLaneIndex, quadIdxConst[q]}));
              editor.SetName(val.quadSwizzledData[q],
                             StringFormat::Fmt("%s_swiz%u", val.name.c_str(), q));
            }
          }
        }
      }

      rdcspv::Id subgroupSize, numSubgroups, globalBallot, electBallot, helperBallot;

      // if we are doing even minimal subgroups, read the subgroup-relative lane index and subgroup size
      if(minimalSubgroups || fullSubgroups)
      {
        if(subgroupCapability == SubgroupCapability::EXTBallot)
        {
          globalBallot = ops.add(rdcspv::OpSubgroupBallotKHR(
              uint4Type, editor.MakeId(), editor.AddConstantImmediate<bool>(true)));
          electBallot = editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()));

          if(stage == ShaderStage::Pixel)
          {
            helperBallot = ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), isHelper,
                                                       editor.AddConstantImmediate<uint32_t>(0)));
            helperBallot =
                ops.add(rdcspv::OpSubgroupBallotKHR(uint4Type, editor.MakeId(), helperBallot));
          }
          else
          {
            helperBallot = editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()));
          }
        }
        else
        {
          globalBallot = ops.add(rdcspv::OpGroupNonUniformBallot(
              uint4Type, editor.MakeId(), subgroupScope, editor.AddConstantImmediate<bool>(true)));
          electBallot =
              ops.add(rdcspv::OpGroupNonUniformElect(boolType, editor.MakeId(), subgroupScope));
          electBallot = ops.add(rdcspv::OpGroupNonUniformBallot(uint4Type, editor.MakeId(),
                                                                subgroupScope, electBallot));

          if(stage == ShaderStage::Pixel)
          {
            helperBallot = ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), isHelper,
                                                       editor.AddConstantImmediate<uint32_t>(0)));
            helperBallot = ops.add(rdcspv::OpGroupNonUniformBallot(uint4Type, editor.MakeId(),
                                                                   subgroupScope, helperBallot));
          }
          else
          {
            helperBallot = editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()));
          }
        }

        laneIndex = editor.AddBuiltinInputLoad(
            ops, newGlobals, stage, rdcspv::BuiltIn::SubgroupLocalInvocationId, uint32Type);
        subgroupSize = editor.AddBuiltinInputLoad(ops, newGlobals, stage,
                                                  rdcspv::BuiltIn::SubgroupSize, uint32Type);
        editor.SetName(laneIndex, "laneIndex");
        editor.SetName(subgroupSize, "subgroupSize");

        // subgroup ID & num subgroups is only available for compute
        if(stage == ShaderStage::Compute || stage == ShaderStage::Task || stage == ShaderStage::Mesh)
        {
          numSubgroups = editor.AddBuiltinInputLoad(ops, newGlobals, stage,
                                                    rdcspv::BuiltIn::NumSubgroups, uint32Type);
          editor.SetName(numSubgroups, "numSubgroups");
        }
        else
        {
          numSubgroups = editor.AddConstantImmediate<uint32_t>(0);
        }
      }
      else
      {
        globalBallot = editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()));
        electBallot = editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()));
        helperBallot = editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()));
        subgroupSize = editor.AddConstantImmediate<uint32_t>(0);
        numSubgroups = editor.AddConstantImmediate<uint32_t>(0);
      }
      editor.SetName(globalBallot, "globalBallot");

      // in a pixel shader we need to take extra steps to ensure we get helper data, and it depends
      // on if we're fetching subgroups or not.
      // if we're not fetching subgroups, we always fetch all 4 helpers and store them since that's
      // all our data. if we ARE fetching subgroups, helper lanes will not write their own data so
      // we do that from the candidate thread (only for the helper lanes, conditionally). We also
      // store into the lane index of each quad with subgroups, as opposed to just 0-3 for a plain
      // quad.
      // if we're not in a pixel shader we don't do any of this
      rdcspv::Id isHelperPerQuad[4] = {};
      rdcspv::Id shouldStoreHelperPerQuad[4] = {};
      rdcspv::Id quadLaneStoreIdx[4] = {};

      if(stage == ShaderStage::Pixel)
      {
        // calculate the quadId that we need for pixels, the top-left thread's lane index
        rdcspv::Id quadIdSwizzle =
            ops.add(rdcspv::OpConvertUToF(floatType, editor.MakeId(), laneIndex));
        quadIdSwizzle =
            ops.add(rdcspv::OpFunctionCall(floatType, editor.MakeId(), quadSwizzleHelper[1],
                                           {quadIdSwizzle, quadLaneIndex, quadIdxConst[0]}));

        quadIdSwizzle = ops.add(rdcspv::OpConvertFToU(uint32Type, editor.MakeId(), quadIdSwizzle));
        // add offset so that quad IDs are always non-zero
        quadId = ops.add(rdcspv::OpIAdd(uint32Type, quadId, quadIdSwizzle,
                                        editor.AddConstantImmediate<uint32_t>(10000)));

        for(uint32_t q = 0; q < 4; q++)
        {
          isHelperPerQuad[q] = ops.add(rdcspv::OpConvertUToF(floatType, editor.MakeId(), isHelper));
          isHelperPerQuad[q] =
              ops.add(rdcspv::OpFunctionCall(floatType, editor.MakeId(), quadSwizzleHelper[1],
                                             {isHelperPerQuad[q], quadLaneIndex, quadIdxConst[q]}));
          isHelperPerQuad[q] =
              ops.add(rdcspv::OpConvertFToU(uint32Type, editor.MakeId(), isHelperPerQuad[q]));
        }

        if(fullSubgroups)
        {
          for(uint32_t q = 0; q < 4; q++)
          {
            shouldStoreHelperPerQuad[q] =
                ops.add(rdcspv::OpINotEqual(boolType, editor.MakeId(), isHelperPerQuad[q],
                                            editor.AddConstantImmediate<uint32_t>(0)));

            quadLaneStoreIdx[q] =
                ops.add(rdcspv::OpConvertUToF(floatType, editor.MakeId(), laneIndex));
            quadLaneStoreIdx[q] = ops.add(
                rdcspv::OpFunctionCall(floatType, editor.MakeId(), quadSwizzleHelper[1],
                                       {quadLaneStoreIdx[q], quadLaneIndex, quadIdxConst[q]}));
            quadLaneStoreIdx[q] =
                ops.add(rdcspv::OpConvertFToU(uint32Type, editor.MakeId(), quadLaneStoreIdx[q]));

            editor.SetName(isHelperPerQuad[q], StringFormat::Fmt("isHelper%u", q));
            editor.SetName(shouldStoreHelperPerQuad[q], StringFormat::Fmt("shouldStore%u", q));
            editor.SetName(quadLaneStoreIdx[q], StringFormat::Fmt("quadLaneStoreIdx%u", q));
          }
        }
        else
        {
          for(uint32_t q = 0; q < 4; q++)
          {
            shouldStoreHelperPerQuad[q] = editor.AddConstantImmediate<bool>(true);
            quadLaneStoreIdx[q] = quadIdxConst[q];
          }
        }
      }

      // get a pointer to buffer.hit_count
      rdcspv::Id hit_count = ops.add(rdcspv::OpAccessChain(
          uintPtr, editor.MakeId(), structPtr, {editor.AddConstantImmediate<uint32_t>(0)}));

      // get a pointer to buffer.total_count
      rdcspv::Id total_count = ops.add(rdcspv::OpAccessChain(
          uintPtr, editor.MakeId(), structPtr, {editor.AddConstantImmediate<uint32_t>(1)}));

      editor.SetName(candidateThread, "candidateThread");

      // if we are fetching full subgroups in a pixel shader, we need to know which threads are quad
      // neighbours of the candidate so we can write their quad lane index properly
      rdcspv::Id candidateThreadInQuad;
      if(fullSubgroups && stage == ShaderStage::Pixel)
      {
        rdcspv::Id zeroF = editor.AddConstantImmediate<float>(0.0f);

        // we do a simple check here - if candidateThread is true, or any ddx/ddy is non-zero, we're
        // in the candidate quad

        rdcspv::Id candidateThreadF = ops.add(rdcspv::OpSelect(
            uint32Type, editor.MakeId(), candidateThread, editor.AddConstantImmediate<uint32_t>(1),
            editor.AddConstantImmediate<uint32_t>(0)));
        candidateThreadF =
            ops.add(rdcspv::OpConvertUToF(floatType, editor.MakeId(), candidateThreadF));

        rdcspv::Id candidateThreadDDXFine =
            ops.add(rdcspv::OpDPdxFine(floatType, editor.MakeId(), candidateThreadF));
        candidateThreadDDXFine = ops.add(
            rdcspv::OpFOrdGreaterThan(boolType, editor.MakeId(), candidateThreadDDXFine, zeroF));

        rdcspv::Id candidateThreadDDYFine =
            ops.add(rdcspv::OpDPdyFine(floatType, editor.MakeId(), candidateThreadF));
        candidateThreadDDYFine = ops.add(
            rdcspv::OpFOrdGreaterThan(boolType, editor.MakeId(), candidateThreadDDYFine, zeroF));

        rdcspv::Id candidateThreadDDXCoarse =
            ops.add(rdcspv::OpDPdxCoarse(floatType, editor.MakeId(), candidateThreadF));
        candidateThreadDDXCoarse = ops.add(
            rdcspv::OpFOrdGreaterThan(boolType, editor.MakeId(), candidateThreadDDXCoarse, zeroF));

        rdcspv::Id candidateThreadDDYCoarse =
            ops.add(rdcspv::OpDPdyCoarse(floatType, editor.MakeId(), candidateThreadF));
        candidateThreadDDYCoarse = ops.add(
            rdcspv::OpFOrdGreaterThan(boolType, editor.MakeId(), candidateThreadDDYCoarse, zeroF));

        candidateThreadInQuad = ops.add(rdcspv::OpLogicalOr(
            boolType, editor.MakeId(), candidateThread, candidateThreadDDXFine));
        candidateThreadInQuad = ops.add(rdcspv::OpLogicalOr(
            boolType, editor.MakeId(), candidateThreadInQuad, candidateThreadDDYFine));
        candidateThreadInQuad = ops.add(rdcspv::OpLogicalOr(
            boolType, editor.MakeId(), candidateThreadInQuad, candidateThreadDDXCoarse));
        candidateThreadInQuad = ops.add(rdcspv::OpLogicalOr(
            boolType, editor.MakeId(), candidateThreadInQuad, candidateThreadDDYCoarse));
        editor.SetName(candidateThreadInQuad, "candidateThreadInQuad");
      }

      rdcarray<rdcspv::Id> killLabels;
      killLabels.push_back(editor.MakeId());
      rdcspv::Id writeLabel = editor.MakeId();

      rdcspv::Id writeCondition = candidateThread;

      // if we're doing proper subgroup readback, keep the whole subgroup, otherwise branch non-uniformly
      if(fullSubgroups)
      {
        if(subgroupCapability == SubgroupCapability::Vulkan1_1)
        {
          writeCondition = ops.add(rdcspv::OpGroupNonUniformAny(boolType, editor.MakeId(),
                                                                subgroupScope, candidateThread));
        }
        else
        {
          // KHR path, emulate a vote with any(ballot() != 0u) so we don't depend on the vote
          // extension - we probably can, but don't have to
          writeCondition =
              ops.add(rdcspv::OpSubgroupBallotKHR(uint4Type, editor.MakeId(), candidateThread));
          writeCondition = ops.add(rdcspv::OpINotEqual(
              bool4Type, editor.MakeId(), writeCondition,
              editor.AddConstant(rdcspv::OpConstantNull(uint4Type, editor.MakeId()))));
          writeCondition = ops.add(rdcspv::OpAny(boolType, editor.MakeId(), writeCondition));
        }
      }

      ops.add(rdcspv::OpSelectionMerge(killLabels.back(), rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(writeCondition, writeLabel, killLabels.back()));

      ops.add(rdcspv::OpLabel(writeLabel));

      // for pixel shaders with subgroups, ensure we mask off helper lanes from the subgroup so they
      // don't take part in the elect
      if(fullSubgroups && stage == ShaderStage::Pixel)
      {
        killLabels.push_back(editor.MakeId());
        writeLabel = editor.MakeId();
        rdcspv::Id helperCondition = ops.add(rdcspv::OpIEqual(
            boolType, editor.MakeId(), isHelper, editor.AddConstantImmediate<uint32_t>(0)));
        ops.add(rdcspv::OpSelectionMerge(killLabels.back(), rdcspv::SelectionControl::None));
        ops.add(rdcspv::OpBranchConditional(helperCondition, writeLabel, killLabels.back()));
        ops.add(rdcspv::OpLabel(writeLabel));
      }

      rdcspv::Id slotAllocLabel = editor.MakeId(), slotMergeLabel = editor.MakeId();

      // for subgroups the whole subgroup is in here, ensure we only alloc a slot with one lane
      if(fullSubgroups)
      {
        rdcspv::Id nonHelperElected;

        if(subgroupCapability == SubgroupCapability::Vulkan1_1)
          nonHelperElected =
              ops.add(rdcspv::OpGroupNonUniformElect(boolType, editor.MakeId(), subgroupScope));
        else
          nonHelperElected = ops.add(rdcspv::OpSubgroupFirstInvocationKHR(
              boolType, editor.MakeId(), editor.AddConstantImmediate<bool>(true)));
        ops.add(rdcspv::OpSelectionMerge(slotMergeLabel, rdcspv::SelectionControl::None));
        ops.add(rdcspv::OpBranchConditional(nonHelperElected, slotAllocLabel, slotMergeLabel));
      }
      else
      {
        ops.add(rdcspv::OpBranch(slotAllocLabel));
      }

      ops.add(rdcspv::OpLabel(slotAllocLabel));

      // increment total_count
      ops.add(rdcspv::OpAtomicIAdd(uint32Type, editor.MakeId(), total_count, scope, semantics,
                                   editor.AddConstantImmediate<uint32_t>(1)));

      // allocate a slot with atomic add
      rdcspv::Id slot =
          ops.add(rdcspv::OpAtomicIAdd(uint32Type, editor.MakeId(), hit_count, scope, semantics,
                                       editor.AddConstantImmediate<uint32_t>(1)));

      editor.SetName(slot, "slotAlloc");

      ops.add(rdcspv::OpBranch(slotMergeLabel));

      ops.add(rdcspv::OpLabel(slotMergeLabel));

      // now if we're in a subgroup we need to broadcast the slot to the whole group, and also OpPhi
      // the previous slot depending on where we got it from
      if(fullSubgroups)
      {
        slot = ops.add(rdcspv::OpPhi(
            uint32Type, editor.MakeId(),
            {{slot, slotAllocLabel}, {editor.AddConstantImmediate<uint32_t>(0U), writeLabel}}));
        editor.SetName(slot, "slotToBroadcast");

        if(subgroupCapability == SubgroupCapability::Vulkan1_1)
          slot = ops.add(rdcspv::OpGroupNonUniformBroadcastFirst(uint32Type, editor.MakeId(),
                                                                 subgroupScope, slot));
        else
          slot = ops.add(rdcspv::OpSubgroupFirstInvocationKHR(uint32Type, editor.MakeId(), slot));
        editor.SetName(slot, "slot");
      }

      rdcspv::Id inRange = ops.add(rdcspv::OpULessThan(boolType, editor.MakeId(), slot, arrayLength));

      killLabels.push_back(editor.MakeId());
      writeLabel = editor.MakeId();
      ops.add(rdcspv::OpSelectionMerge(killLabels.back(), rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(inRange, writeLabel, killLabels.back()));
      ops.add(rdcspv::OpLabel(writeLabel));

      rdcspv::Id hitptr = editor.DeclareType(rdcspv::Pointer(ResultDataBaseType, bufferClass));

      // get a pointer to the hit for our slot
      rdcspv::Id hit = ops.add(rdcspv::OpAccessChain(
          hitptr, editor.MakeId(), structPtr, {editor.AddConstantImmediate<uint32_t>(3), slot}));

      // store fixed properties. In the subgroup case this needs to be conditional for only the candidate thread
      rdcspv::Id fixedDataLabel = editor.MakeId(), fixedDataMerge = editor.MakeId();

      if(fullSubgroups)
      {
        ops.add(rdcspv::OpSelectionMerge(fixedDataMerge, rdcspv::SelectionControl::None));
        ops.add(rdcspv::OpBranchConditional(candidateThread, fixedDataLabel, fixedDataMerge));
      }
      else
      {
        ops.add(rdcspv::OpBranch(fixedDataLabel));
      }

      ops.add(rdcspv::OpLabel(fixedDataLabel));

      rdcspv::Id storePtr =
          ops.add(rdcspv::OpAccessChain(float4BufPtr, editor.MakeId(), hit,
                                        {editor.AddConstantImmediate<uint32_t>(ResultBase_pos)}));
      if(fragCoord != rdcspv::Id())
        ops.add(rdcspv::OpStore(storePtr, fragCoord, alignedAccess));

      rdcspv::Id primitiveID;
      if(usePrimitiveID)
      {
        primitiveID = editor.AddBuiltinInputLoad(ops, newGlobals, stage,
                                                 rdcspv::BuiltIn::PrimitiveId, uint32Type);
        editor.AddCapability(rdcspv::Capability::Geometry);
      }
      else
      {
        primitiveID = editor.AddConstantImmediate<uint32_t>(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                        {editor.AddConstantImmediate<uint32_t>(ResultBase_prim)}));
      ops.add(rdcspv::OpStore(storePtr, primitiveID, alignedAccess));

      rdcspv::Id sampleIndex;
      if(useSampleID)
      {
        sampleIndex = editor.AddBuiltinInputLoad(ops, newGlobals, stage, rdcspv::BuiltIn::SampleId,
                                                 uint32Type);
        editor.AddCapability(rdcspv::Capability::SampleRateShading);
      }
      else
      {
        sampleIndex = editor.AddConstantImmediate<uint32_t>(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                        {editor.AddConstantImmediate<uint32_t>(ResultBase_sample)}));
      ops.add(rdcspv::OpStore(storePtr, sampleIndex, alignedAccess));

      rdcspv::Id viewIndex;
      if(useViewIndex)
      {
        viewIndex = editor.AddBuiltinInputLoad(ops, newGlobals, stage, rdcspv::BuiltIn::ViewIndex,
                                               uint32Type);
        editor.AddCapability(rdcspv::Capability::MultiView);
        editor.AddExtension("SPV_KHR_multiview");
      }
      else
      {
        viewIndex = editor.AddConstantImmediate<uint32_t>(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                        {editor.AddConstantImmediate<uint32_t>(ResultBase_view)}));
      ops.add(rdcspv::OpStore(storePtr, viewIndex, alignedAccess));

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                        {editor.AddConstantImmediate<uint32_t>(ResultBase_valid)}));
      ops.add(rdcspv::OpStore(storePtr, editor.AddConstantImmediate(validMagicNumber), alignedAccess));

      // store derivative health check for pixel shaders
      storePtr = ops.add(
          rdcspv::OpAccessChain(floatBufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_ddxDerivCheck)}));
      ops.add(rdcspv::OpStore(storePtr, ddxDerivativeCheck, alignedAccess));

      // store the quadLaneIndex (in case it's different to laneIndex)
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_quadLaneIndex)}));
      if(quadLaneIndex != rdcspv::Id())
        ops.add(rdcspv::OpStore(storePtr, quadLaneIndex, alignedAccess));

      // store the laneIndex
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_laneIndex)}));
      ops.add(rdcspv::OpStore(storePtr, laneIndex, alignedAccess));

      // if we have them, store subgroup properties, if they're not present they will be 0
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_subgroupSize)}));
      ops.add(rdcspv::OpStore(storePtr, subgroupSize, alignedAccess));
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint4BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_globalBallot)}));
      ops.add(rdcspv::OpStore(storePtr, globalBallot, alignedAccess));
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint4BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_electBallot)}));
      ops.add(rdcspv::OpStore(storePtr, electBallot, alignedAccess));
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint4BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_helperBallot)}));
      ops.add(rdcspv::OpStore(storePtr, helperBallot, alignedAccess));
      storePtr = ops.add(
          rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit,
                                {editor.AddConstantImmediate<uint32_t>(ResultBase_numSubgroups)}));
      ops.add(rdcspv::OpStore(storePtr, numSubgroups, alignedAccess));

      // merge after doing the fixed data section
      ops.add(rdcspv::OpBranch(fixedDataMerge));
      ops.add(rdcspv::OpLabel(fixedDataMerge));

      rdcspv::Id LaneDataPtrType = editor.DeclareType(rdcspv::Pointer(LaneDataStruct, bufferClass));

      // now we conditionally store each helper lane. Only relevant for pixel shaders but we need
      // all helper lanes for all active lanes to ensure we can get derivatives for any of them
      if(stage == ShaderStage::Pixel)
      {
        for(uint32_t q = 0; q < 4; q++)
        {
          rdcspv::Id doHelperLabel = editor.MakeId(), skipHelperLabel = editor.MakeId();

          ops.add(rdcspv::OpSelectionMerge(skipHelperLabel, rdcspv::SelectionControl::None));
          ops.add(rdcspv::OpBranchConditional(shouldStoreHelperPerQuad[q], doHelperLabel,
                                              skipHelperLabel));
          ops.add(rdcspv::OpLabel(doHelperLabel));

          rdcspv::Id outputPtr = ops.add(rdcspv::OpAccessChain(
              LaneDataPtrType, editor.MakeId(), hit,
              {editor.AddConstantImmediate<uint32_t>(ResultBase_firstUser), quadLaneStoreIdx[q]}));

          for(laneValue &val : laneValues)
          {
            rdcspv::Id valueType = val.type;
            if(valueType == boolType)
              valueType = uint32Type;
            rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(valueType, bufferClass));

            rdcspv::Id valPtr = ops.add(rdcspv::OpAccessChain(
                ptrType, editor.MakeId(), outputPtr,
                {editor.AddConstantImmediate<uint32_t>((uint32_t)val.structIndex)}));

            if(val.base == isHelper)
            {
              ops.add(rdcspv::OpStore(valPtr, isHelperPerQuad[q], alignedAccess));
            }
            else if(val.base == quadLaneIndex)
            {
              ops.add(rdcspv::OpStore(valPtr, quadIdxConst[q], alignedAccess));
            }
            else if(val.flat)
            {
              ops.add(rdcspv::OpStore(valPtr, val.base, alignedAccess));
            }
            else
            {
              RDCASSERT(!val.quadSwizzledData.empty());
              ops.add(rdcspv::OpStore(valPtr, val.quadSwizzledData[q], alignedAccess));
            }
          }

          ops.add(rdcspv::OpBranch(skipHelperLabel));
          ops.add(rdcspv::OpLabel(skipHelperLabel));
        }
      }

      // if we have full subgroups, each subgroup now writes its own data here, if we are in a
      // non-pixel shader without subgroups we store the single thread's data here.
      // the non-subgroup pixel shader case is handled above in the helper lanes (which will all store)
      if(fullSubgroups || stage != ShaderStage::Pixel)
      {
        rdcspv::Id idx;

        if(fullSubgroups)
          idx = laneIndex;
        else
          idx = editor.AddConstantImmediate<uint32_t>(0U);

        rdcspv::Id outputPtr = ops.add(rdcspv::OpAccessChain(
            LaneDataPtrType, editor.MakeId(), hit,
            {editor.AddConstantImmediate<uint32_t>(ResultBase_firstUser), idx}));

        for(laneValue &val : laneValues)
        {
          rdcspv::Id valueType = val.type;
          if(valueType == boolType)
            valueType = uint32Type;
          rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(valueType, bufferClass));

          rdcspv::Id valPtr = ops.add(rdcspv::OpAccessChain(
              ptrType, editor.MakeId(), outputPtr,
              {editor.AddConstantImmediate<uint32_t>((uint32_t)val.structIndex)}));

          ops.add(rdcspv::OpStore(valPtr, val.base, alignedAccess));
        }
      }

      // join up with the early-outs we did, in reverse order
      for(size_t i = 0; i < killLabels.size(); i++)
      {
        rdcspv::Id label = killLabels[killLabels.size() - 1 - i];
        ops.add(rdcspv::OpBranch(label));
        ops.add(rdcspv::OpLabel(label));
      }
    }

    // we want to "call" the original function to ensure the compiler does hopefully as close
    // codegen as possible to the original but we don't want to actually execute it. To do this we
    // use an atomic max with a dummy value and only call the function if the value is *larger* -
    // the compiler can't know what value was pre-existing in the buffer (though we know it was
    // zero) so it can't eliminate either branch, but in practice we will always return

    rdcspv::Id trueLabel = editor.MakeId();
    rdcspv::Id falseLabel = editor.MakeId();

    // get a pointer to buffer.dummy
    rdcspv::Id dummy = ops.add(rdcspv::OpAccessChain(uintPtr, editor.MakeId(), structPtr,
                                                     {editor.AddConstantImmediate<uint32_t>(2)}));

    dummy = ops.add(rdcspv::OpAtomicUMax(uint32Type, editor.MakeId(), dummy, scope, semantics,
                                         editor.AddConstantImmediate<uint32_t>(1)));
    editor.SetName(dummy, "dummy");
    rdcspv::Id dummyCompare = ops.add(rdcspv::OpULessThan(
        boolType, editor.MakeId(), dummy, editor.AddConstantImmediate<uint32_t>(2)));

    ops.add(rdcspv::OpSelectionMerge(falseLabel, rdcspv::SelectionControl::None));
    ops.add(rdcspv::OpBranchConditional(dummyCompare, trueLabel, falseLabel));

    ops.add(rdcspv::OpLabel(trueLabel));

    //  don't return, kill. This makes it well-defined that we don't write anything to our outputs
    if(ShaderStage(shadRefl.stageIndex) == ShaderStage::Pixel)
      ops.add(rdcspv::OpKill());
    else
      ops.add(rdcspv::OpReturn());

    ops.add(rdcspv::OpLabel(falseLabel));

    ops.add(rdcspv::OpFunctionCall(voidType, editor.MakeId(), originalEntry));

    ops.add(rdcspv::OpReturn());

    ops.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(ops);
  }

  editor.AddEntryGlobals(entryID, newGlobals);
}

rdcpair<uint32_t, uint32_t> GetAlignAndOutputSize(VulkanCreationInfo::ShaderModuleReflection &shadRefl)
{
  uint32_t paramAlign = 16;

  for(const SigParameter &sig : shadRefl.refl->inputSignature)
  {
    if(VarTypeByteSize(sig.varType) * sig.compCount > paramAlign)
      paramAlign = 32;
  }

  // conservatively calculate structure stride with full amount for every input element
  uint32_t structStride = (uint32_t)shadRefl.refl->inputSignature.size() * paramAlign;

  if(shadRefl.refl->stage == ShaderStage::Vertex)
    structStride += sizeof(VertexLaneData);
  else if(shadRefl.refl->stage == ShaderStage::Pixel)
    structStride += sizeof(PixelLaneData);
  else if(shadRefl.refl->stage == ShaderStage::Compute ||
          shadRefl.refl->stage == ShaderStage::Task || shadRefl.refl->stage == ShaderStage::Mesh)
    structStride += sizeof(ComputeLaneData);

  if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    structStride += sizeof(SubgroupLaneData);
  }

  return {paramAlign, structStride};
}

VkDescriptorSetLayoutBinding MakeNewBinding(VkShaderStageFlagBits stage)
{
  return {
      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL,
  };
}

void VulkanReplay::CalculateSubgroupProperties(uint32_t &maxSubgroupSize,
                                               SubgroupCapability &subgroupCapability)
{
  maxSubgroupSize = 4;

  // if we don't have subgroup ballots we assume we have no real meaningful subgroup capabilities at
  // all except for 'basic'. The only thing basic lets you do is fetch the subgroup ID, and
  // determine which lane is the first active (OpGroupNonUniformElect).
  // in this case we effectively consider it non-subgroup and just read those values directly to
  // fill in, but otherwise simulate as if there were no subgroup use.

  // for our purposes vulkan 1.1 fully deprecated the old EXT_shader_subgroup_* pair of extensions
  // as the only thing that wasn't deprecated was a non-constant broadcast ID which we don't need
  if(m_pDriver->GetExtensions(NULL).vulkanVersion >= VK_API_VERSION_1_1)
  {
    VkPhysicalDeviceSubgroupProperties subProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
    };

    VkPhysicalDeviceProperties2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    availBase.pNext = &subProps;
    m_pDriver->vkGetPhysicalDeviceProperties2(m_pDriver->GetPhysDev(), &availBase);

    maxSubgroupSize = subProps.subgroupSize;
    subgroupCapability = SubgroupCapability::Vulkan1_1_NoBallot;
    const VkSubgroupFeatureFlags requiredFlags =
        (VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT);
    if((subProps.supportedOperations & requiredFlags) == requiredFlags)
      subgroupCapability = SubgroupCapability::Vulkan1_1;

    if(m_pDriver->GetExtensions(NULL).ext_EXT_subgroup_size_control)
    {
      VkPhysicalDeviceSubgroupSizeControlProperties subSizeProps = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES,
      };
      availBase.pNext = &subSizeProps;
      m_pDriver->vkGetPhysicalDeviceProperties2(m_pDriver->GetPhysDev(), &availBase);

      // use new upper bound in case it's higher with variable sizes
      maxSubgroupSize = RDCMAX(maxSubgroupSize, subSizeProps.maxSubgroupSize);
    }
  }
  else if(m_pDriver->GetExtensions(NULL).ext_EXT_shader_subgroup_ballot)
  {
    // the ballot extension only proides the subgroup size on the GPU so we need to allocate worst case up front

    RDCWARN("Subgroup ballot extension is best extension enabled - using worst case subgroup size");

    maxSubgroupSize = 128;
    subgroupCapability = SubgroupCapability::EXTBallot;
  }
  else if(m_pDriver->GetExtensions(NULL).ext_EXT_shader_subgroup_vote)
  {
    // if only the vote extension is enabled we have no way to determine the subgroup size or
    // anything, so we just fall back to treating this as a degenerate case with a single thread

    RDCWARN("Subgroup vote extension is only subgroup feature enabled - treating as degenerate");

    maxSubgroupSize = 1;
    subgroupCapability = SubgroupCapability::None;
  }
}

VkSpecializationInfo VulkanReplay::MakeSpecInfo(SpecData &specData, VkSpecializationMapEntry *specMaps)
{
  memcpy(specMaps, specMapsTemplate, sizeof(specMapsTemplate));

  specMaps[(uint32_t)InputSpecConstant::Address].size =
      (m_StorageMode == BufferStorageMode::KHR_bda32 ? sizeof(uint32_t) : sizeof(uint64_t));

  VkSpecializationInfo ret = {};
  ret.dataSize = sizeof(specData);
  ret.pData = &specData;
  ret.mapEntryCount = (uint32_t)InputSpecConstant::Count;
  ret.pMapEntries = specMaps;
  return ret;
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
  const VulkanCreationInfo::ShaderEntry &shaderEntry =
      state.graphics.shaderObject ? c.m_ShaderObject[state.shaderObjects[0]].shad : pipe.shaders[0];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[shaderEntry.module];
  rdcstr entryPoint = shaderEntry.entryPoint;
  const rdcarray<SpecConstant> &spec = shaderEntry.specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(ShaderStage::Vertex, entryPoint, state.graphics.pipeline);

  if(!shadRefl.refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  if((shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup) &&
     !m_pDriver->GetDeviceEnabledFeatures().vertexPipelineStoresAndAtomics)
  {
    RDCWARN("Subgroup vertex debugging is not supported without vertex stores");
    return new ShaderDebugTrace;
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

  SubgroupCapability subgroupCapability = SubgroupCapability::None;
  uint32_t maxSubgroupSize = 1;
  CalculateSubgroupProperties(maxSubgroupSize, subgroupCapability);

  uint32_t numThreads = 1;

  if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    numThreads = RDCMAX(numThreads, maxSubgroupSize);

  apiWrapper->location_inputs.resize(numThreads);
  apiWrapper->thread_builtins.resize(numThreads);
  apiWrapper->thread_props.resize(numThreads);

  apiWrapper->thread_props[0][(size_t)rdcspv::ThreadProperty::Active] = 1;

  std::unordered_map<ShaderBuiltin, ShaderVariable> &global_builtins = apiWrapper->global_builtins;
  global_builtins[ShaderBuiltin::BaseInstance] =
      ShaderVariable(rdcstr(), action->instanceOffset, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::BaseVertex] = ShaderVariable(
      rdcstr(), (action->flags & ActionFlags::Indexed) ? action->baseVertex : action->vertexOffset,
      0U, 0U, 0U);
  global_builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), action->drawIndex, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::ViewportIndex] = ShaderVariable(rdcstr(), view, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::MultiViewIndex] = ShaderVariable(rdcstr(), view, 0U, 0U, 0U);

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
      useViewIndex =
          (state.dynamicRendering.active ? state.dynamicRendering.viewMask : pipe.viewMask) != 0;
      if(!useViewIndex && Vulkan_Debug_ShaderDebugLogging())
        RDCLOG("Disabling useViewIndex because viewMask is zero");
    }
  }
  else
  {
    if(Vulkan_Debug_ShaderDebugLogging())
      RDCLOG("Disabling useViewIndex from input view %u", view);
  }

  // if we need to fetch subgroup data, do that now.
  uint32_t laneIndex = 0;
  if(numThreads > 1)
  {
    SpecData specData = {};

    if(action->flags & ActionFlags::Indexed)
      specData.destVertex = idx;
    else
      specData.destVertex = vertid + vertOffset;
    specData.destInstance = instid + instOffset;
    specData.destView = view == ~0U ? 0 : view;

    uint32_t paramAlign, structStride;
    rdctie(paramAlign, structStride) = GetAlignAndOutputSize(shadRefl);

    uint32_t maxHits = 4;    // we should only ever get one hit

    // struct size is ResultDataBase header plus Nx structStride for the number of threads
    uint32_t structSize = sizeof(ResultDataBase) + structStride * numThreads;

    VkDeviceSize feedbackStorageSize = maxHits * structSize + 1024;

    if(Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
             feedbackStorageSize);
    }

    m_PatchedShaderFeedback.ResizeFeedbackBuffer(m_pDriver, feedbackStorageSize);

    specData.arrayLength = maxHits;

    // make copy of state to draw from
    VulkanRenderState modifiedstate = state;

    RDCCOMPILE_ASSERT(NumReservedBindings == 1, "NumReservedBindings is wrong");
    AddedDescriptorData patchedBufferdata = PrepareExtraBufferDescriptor(
        modifiedstate, false, {MakeNewBinding(VK_SHADER_STAGE_VERTEX_BIT)}, false);

    if(patchedBufferdata.empty())
    {
      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = ShaderStage::Vertex;

      return ret;
    }

    if(!patchedBufferdata.descSets.empty())
      m_PatchedShaderFeedback.FeedbackBuffer.WriteDescriptor(Unwrap(patchedBufferdata.descSets[0]),
                                                             0, 0);

    specData.bufferAddress = m_PatchedShaderFeedback.FeedbackBuffer.Address();
    if(specData.bufferAddress && Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("Got buffer address of %llu", specData.bufferAddress);
    }

    // create shader with modified code

    VkSpecializationMapEntry specMaps[(size_t)InputSpecConstant::Count];
    RDCCOMPILE_ASSERT(sizeof(specMaps) == sizeof(specMapsTemplate),
                      "Specialisation maps have changed");

    VkSpecializationInfo patchedSpecInfo = MakeSpecInfo(specData, specMaps);

    auto patchCallback = [this, &shadRefl, &patchedSpecInfo, useViewIndex, subgroupCapability,
                          maxSubgroupSize](const AddedDescriptorData &patchedBufferdata,
                                           VkShaderStageFlagBits stage, const char *entryName,
                                           const rdcarray<uint32_t> &origSpirv,
                                           rdcarray<uint32_t> &modSpirv,
                                           const VkSpecializationInfo *&specInfo) {
      if(stage != VK_SHADER_STAGE_VERTEX_BIT)
        return false;

      modSpirv = origSpirv;

      if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
        FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_vsinput_before.spv", modSpirv);

      CreateInputFetcher(modSpirv, shadRefl, m_StorageMode, false, false, useViewIndex,
                         subgroupCapability, maxSubgroupSize);

      if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
        FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_vsinput_after.spv", modSpirv);

      // overwrite user's specialisation info, assuming that the old specialisation info is not
      // relevant for codegen (the only thing it would be used for)
      specInfo = &patchedSpecInfo;

      return true;
    };

    PrepareStateForPatchedShader(patchedBufferdata, modifiedstate, false, patchCallback);

    if(!RunFeedbackAction(feedbackStorageSize, action, modifiedstate))
    {
      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = ShaderStage::Vertex;

      return ret;
    }

    bytebuf data;
    GetDebugManager()->GetBufferData(m_PatchedShaderFeedback.FeedbackBuffer, 0, 0, data);

    byte *base = data.data();
    uint32_t hit_count = ((uint32_t *)base)[0];
    // uint32_t total_count = ((uint32_t *)base)[1];

    RDCASSERTMSG("Should only get one hit for vertex shaders", hit_count == 1, hit_count);

    base += sizeof(Vec4f);

    ResultDataBase *winner = (ResultDataBase *)base;

    if(winner->valid != validMagicNumber)
    {
      RDCWARN("Hit doesn't have valid magic number");

      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = ShaderStage::Vertex;

      return ret;
    }

    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shader.spirv.GetSPIRV());

    // the per-thread data immediately follows the ResultDataBase header. Every piece of data is
    // uniformly aligned, either 16-byte by default or 32-byte if larger components exist. The
    // output is in input signature order.
    byte *LaneData = (byte *)(winner + 1);

    numThreads = 4;

    if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    {
      RDCASSERTNOTEQUAL(winner->subgroupSize, 0);
      numThreads = RDCMAX(numThreads, winner->subgroupSize);
    }

    apiWrapper->location_inputs.resize(numThreads);
    apiWrapper->thread_builtins.resize(numThreads);
    apiWrapper->thread_props.resize(numThreads);

    for(uint32_t t = 0; t < numThreads; t++)
    {
      byte *value = LaneData + t * structStride;

      {
        SubgroupLaneData *subgroupData = (SubgroupLaneData *)value;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Active] = subgroupData->isActive;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Elected] = subgroupData->elect;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::SubgroupId] = t;

        value += sizeof(SubgroupLaneData);
      }

      // read VertexLaneData
      {
        VertexLaneData *vertData = (VertexLaneData *)value;

        apiWrapper->thread_builtins[t][ShaderBuiltin::InstanceIndex] =
            ShaderVariable("InstanceIndex"_lit, vertData->inst, 0U, 0U, 0U);
        apiWrapper->thread_builtins[t][ShaderBuiltin::VertexIndex] =
            ShaderVariable("VertexIndex"_lit, vertData->vert, 0U, 0U, 0U);
        apiWrapper->thread_builtins[t][ShaderBuiltin::MultiViewIndex] =
            ShaderVariable("VertexIndex"_lit, vertData->view, 0U, 0U, 0U);

        if(view != ~0U)
          RDCASSERTEQUAL(vertData->view, view);
      }
      value += sizeof(VertexLaneData);

      for(size_t i = 0; i < shadRefl.refl->inputSignature.size(); i++)
      {
        const SigParameter &param = shadRefl.refl->inputSignature[i];

        bool builtin = true;
        if(param.systemValue == ShaderBuiltin::Undefined)
        {
          builtin = false;
          apiWrapper->location_inputs[t].resize(
              RDCMAX((uint32_t)apiWrapper->location_inputs.size(), param.regIndex + 1));
        }

        ShaderVariable &var = builtin ? apiWrapper->thread_builtins[t][param.systemValue]
                                      : apiWrapper->location_inputs[t][param.regIndex];

        var.rows = 1;
        var.columns = param.compCount & 0xff;
        var.type = param.varType;

        const uint32_t comp = Bits::CountTrailingZeroes(uint32_t(param.regChannelMask));
        const uint32_t elemSize = VarTypeByteSize(param.varType);

        const size_t sz = elemSize * param.compCount;

        memcpy((var.value.u8v.data()) + elemSize * comp, value + i * paramAlign, sz);
      }
    }
    apiWrapper->global_builtins[ShaderBuiltin::SubgroupSize] =
        ShaderVariable(rdcstr(), numThreads, 0U, 0U, 0U);

    ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                                 shadRefl.instructionLines, shadRefl.patchData,
                                                 winner->laneIndex, numThreads, numThreads);
    apiWrapper->ResetReplay();

    return ret;
  }
  else
  {
    // otherwise we can do a simple manual fetch of vertex inputs
    laneIndex = 0;

    std::unordered_map<ShaderBuiltin, ShaderVariable> &thread_builtins =
        apiWrapper->thread_builtins[laneIndex];
    if(action->flags & ActionFlags::Indexed)
      thread_builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), idx, 0U, 0U, 0U);
    else
      thread_builtins[ShaderBuiltin::VertexIndex] =
          ShaderVariable(rdcstr(), vertid + vertOffset, 0U, 0U, 0U);
    thread_builtins[ShaderBuiltin::InstanceIndex] =
        ShaderVariable(rdcstr(), instid + instOffset, 0U, 0U, 0U);

    rdcarray<ShaderVariable> &locations = apiWrapper->location_inputs[laneIndex];
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

    apiWrapper->global_builtins[ShaderBuiltin::SubgroupSize] =
        ShaderVariable(rdcstr(), numThreads, 0U, 0U, 0U);

    ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                                 shadRefl.instructionLines, shadRefl.patchData,
                                                 laneIndex, numThreads, numThreads);
    apiWrapper->ResetReplay();

    return ret;
  }
}

ShaderDebugTrace *VulkanReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                           const DebugPixelInputs &inputs)
{
  if(!m_pDriver->GetDeviceEnabledFeatures().fragmentStoresAndAtomics)
  {
    RDCWARN("Pixel debugging is not supported without fragment stores");
    return new ShaderDebugTrace;
  }

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
  ResourceId fragId = state.graphics.shaderObject ? state.shaderObjects[4] : pipe.shaders[4].module;

  if(fragId == ResourceId())
  {
    RDCLOG("No pixel shader bound at draw");
    return new ShaderDebugTrace();
  }

  // get ourselves in pristine state before this action (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  const VulkanCreationInfo::ShaderEntry &fragEntry =
      state.graphics.shaderObject ? c.m_ShaderObject[state.shaderObjects[4]].shad : pipe.shaders[4];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[fragEntry.module];
  rdcstr entryPoint = fragEntry.entryPoint;
  const rdcarray<SpecConstant> &spec = fragEntry.specialization;

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

  SubgroupCapability subgroupCapability = SubgroupCapability::None;
  uint32_t maxSubgroupSize = 1;
  CalculateSubgroupProperties(maxSubgroupSize, subgroupCapability);

  uint32_t numThreads = 4;

  if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    numThreads = RDCMAX(numThreads, maxSubgroupSize);

  std::unordered_map<ShaderBuiltin, ShaderVariable> &global_builtins = apiWrapper->global_builtins;
  global_builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), action->drawIndex, 0U, 0U, 0U);

  // If the pipe contains a geometry shader, then Primitive ID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  bool usePrimitiveID = false;
  ResourceId gsId = state.graphics.shaderObject ? state.shaderObjects[3] : pipe.shaders[3].module;
  if(gsId != ResourceId())
  {
    const VulkanCreationInfo::ShaderEntry &gsEntry =
        state.graphics.shaderObject ? c.m_ShaderObject[state.shaderObjects[3]].shad : pipe.shaders[3];
    VulkanCreationInfo::ShaderModuleReflection &gsRefl =
        c.m_ShaderModule[gsEntry.module].GetReflection(ShaderStage::Geometry, gsEntry.entryPoint,
                                                       state.graphics.pipeline);

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
      const VulkanCreationInfo::RenderPass &rpInfo = GetDebugManager()->GetRenderPassInfo(rp);
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
      useViewIndex =
          (state.dynamicRendering.active ? state.dynamicRendering.viewMask : pipe.viewMask) != 0;
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
    global_builtins[ShaderBuiltin::MultiViewIndex] = ShaderVariable(rdcstr(), view, 0U, 0U, 0U);
  }

  uint32_t paramAlign, structStride;
  rdctie(paramAlign, structStride) = GetAlignAndOutputSize(shadRefl);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  // struct size is ResultDataBase header plus Nx structStride for the number of threads
  uint32_t structSize = sizeof(ResultDataBase) + structStride * numThreads;

  VkDeviceSize feedbackStorageSize = overdrawLevels * structSize + sizeof(Vec4f) + 1024;

  if(Vulkan_Debug_ShaderDebugLogging())
  {
    RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
           feedbackStorageSize);
  }

  m_PatchedShaderFeedback.ResizeFeedbackBuffer(m_pDriver, feedbackStorageSize);

  SpecData specData = {};

  specData.arrayLength = overdrawLevels;
  specData.destX = float(x) + 0.5f;
  specData.destY = float(y) + 0.5f;

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  AddedDescriptorData patchedBufferdata = PrepareExtraBufferDescriptor(
      modifiedstate, false, {MakeNewBinding(VK_SHADER_STAGE_FRAGMENT_BIT)}, false);

  if(patchedBufferdata.empty())
  {
    delete apiWrapper;

    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Pixel;

    return ret;
  }

  if(!patchedBufferdata.descSets.empty())
    m_PatchedShaderFeedback.FeedbackBuffer.WriteDescriptor(Unwrap(patchedBufferdata.descSets[0]), 0,
                                                           0);

  specData.bufferAddress = m_PatchedShaderFeedback.FeedbackBuffer.Address();
  if(specData.bufferAddress && Vulkan_Debug_ShaderDebugLogging())
  {
    RDCLOG("Got buffer address of %llu", specData.bufferAddress);
  }

  // create  shader with modified code

  VkSpecializationMapEntry specMaps[(size_t)InputSpecConstant::Count];
  RDCCOMPILE_ASSERT(sizeof(specMaps) == sizeof(specMapsTemplate),
                    "Specialisation maps have changed");

  VkSpecializationInfo patchedSpecInfo = MakeSpecInfo(specData, specMaps);

  auto patchCallback = [this, &shadRefl, &patchedSpecInfo, usePrimitiveID, useSampleID,
                        useViewIndex, subgroupCapability, maxSubgroupSize](
                           const AddedDescriptorData &patchedBufferdata, VkShaderStageFlagBits stage,
                           const char *entryName, const rdcarray<uint32_t> &origSpirv,
                           rdcarray<uint32_t> &modSpirv, const VkSpecializationInfo *&specInfo) {
    if(stage != VK_SHADER_STAGE_FRAGMENT_BIT)
      return false;

    modSpirv = origSpirv;

    if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
      FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_psinput_before.spv", modSpirv);

    CreateInputFetcher(modSpirv, shadRefl, m_StorageMode, usePrimitiveID, useSampleID, useViewIndex,
                       subgroupCapability, maxSubgroupSize);

    if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
      FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/debug_psinput_after.spv", modSpirv);

    // overwrite user's specialisation info, assuming that the old specialisation info is not
    // relevant for codegen (the only thing it would be used for)
    specInfo = &patchedSpecInfo;

    return true;
  };

  PrepareStateForPatchedShader(patchedBufferdata, modifiedstate, false, patchCallback);

  if(!RunFeedbackAction(feedbackStorageSize, action, modifiedstate))
  {
    delete apiWrapper;

    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Pixel;

    return ret;
  }

  bytebuf data;
  GetDebugManager()->GetBufferData(m_PatchedShaderFeedback.FeedbackBuffer, 0, 0, data);

  byte *base = data.data();
  uint32_t hit_count = ((uint32_t *)base)[0];
  uint32_t total_count = ((uint32_t *)base)[1];

  if(hit_count > overdrawLevels)
  {
    RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", hit_count, overdrawLevels);
    hit_count = overdrawLevels;
  }

  base += sizeof(Vec4f);

  ResultDataBase *winner = NULL;

  RDCLOG("Got %u hit candidates out of %u total instances", hit_count, total_count);

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  VkCompareOp depthOp = state.depthCompareOp;

  // depth tests disabled acts the same as always compare mode
  if(!state.depthTestEnable)
    depthOp = VK_COMPARE_OP_ALWAYS;

  for(uint32_t i = 0; i < hit_count; i++)
  {
    ResultDataBase *hit = (ResultDataBase *)(base + structSize * i);

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

    // the per-thread data immediately follows the ResultDataBase header. Every piece of data is
    // uniformly aligned, either 16-byte by default or 32-byte if larger components exist. The
    // output is in input signature order.
    byte *LaneData = (byte *)(winner + 1);

    numThreads = 4;

    if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    {
      RDCASSERTNOTEQUAL(winner->subgroupSize, 0);
      numThreads = RDCMAX(numThreads, winner->subgroupSize);
    }

    apiWrapper->location_inputs.resize(numThreads);
    apiWrapper->thread_builtins.resize(numThreads);
    apiWrapper->thread_props.resize(numThreads);

    for(uint32_t t = 0; t < numThreads; t++)
    {
      byte *value = LaneData + t * structStride;

      if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
      {
        SubgroupLaneData *subgroupData = (SubgroupLaneData *)value;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Active] = subgroupData->isActive;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Elected] = subgroupData->elect;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::SubgroupId] = t;

        value += sizeof(SubgroupLaneData);
      }

      // read PixelLaneData
      {
        PixelLaneData *pixelData = (PixelLaneData *)value;

        {
          ShaderVariable &var = apiWrapper->thread_builtins[t][ShaderBuiltin::Position];

          var.rows = 1;
          var.columns = 4;
          var.type = VarType::Float;

          memcpy(var.value.u8v.data(), &pixelData->fragCoord, sizeof(Vec4f));
        }

        {
          ShaderVariable &var = apiWrapper->thread_builtins[t][ShaderBuiltin::IsHelper];

          var.rows = 1;
          var.columns = 1;
          var.type = VarType::Bool;

          memcpy(var.value.u8v.data(), &pixelData->isHelper, sizeof(uint32_t));
        }

        if(numThreads == 4)
          apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Active] = 1;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Helper] = pixelData->isHelper;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::QuadId] = pixelData->quadId;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::QuadLane] =
            pixelData->quadLaneIndex;
      }
      value += sizeof(PixelLaneData);

      for(size_t i = 0; i < shadRefl.refl->inputSignature.size(); i++)
      {
        const SigParameter &param = shadRefl.refl->inputSignature[i];

        bool builtin = true;
        if(param.systemValue == ShaderBuiltin::Undefined)
        {
          builtin = false;
          apiWrapper->location_inputs[t].resize(
              RDCMAX((uint32_t)apiWrapper->location_inputs.size(), param.regIndex + 1));
        }

        ShaderVariable &var = builtin ? apiWrapper->thread_builtins[t][param.systemValue]
                                      : apiWrapper->location_inputs[t][param.regIndex];

        var.rows = 1;
        var.columns = param.compCount & 0xff;
        var.type = param.varType;

        const uint32_t firstComp = Bits::CountTrailingZeroes(uint32_t(param.regChannelMask));
        const uint32_t elemSize = VarTypeByteSize(param.varType);

        // we always store in 32-bit types
        const size_t sz = RDCMAX(4U, elemSize) * param.compCount;

        memcpy((var.value.u8v.data()) + elemSize * firstComp, value + i * paramAlign, sz);

        // convert down from stored 32-bit types if they were smaller
        if(elemSize == 1)
        {
          ShaderVariable tmp = var;

          for(uint32_t comp = 0; comp < param.compCount; comp++)
            var.value.u8v[comp] = tmp.value.u32v[comp] & 0xff;
        }
        else if(elemSize == 2)
        {
          ShaderVariable tmp = var;

          for(uint32_t comp = 0; comp < param.compCount; comp++)
          {
            if(VarTypeCompType(param.varType) == CompType::Float)
              var.value.f16v[comp] = rdhalf::make(tmp.value.f32v[comp]);
            else
              var.value.u16v[comp] = tmp.value.u32v[comp] & 0xffff;
          }
        }
      }
    }

    apiWrapper->global_builtins[ShaderBuiltin::SubgroupSize] =
        ShaderVariable(rdcstr(), numThreads, 0U, 0U, 0U);

    ret = debugger->BeginDebug(apiWrapper, ShaderStage::Pixel, entryPoint, spec,
                               shadRefl.instructionLines, shadRefl.patchData, winner->laneIndex,
                               numThreads, numThreads);
    apiWrapper->ResetReplay();
  }
  else
  {
    RDCLOG("Didn't get any valid hit to debug");
    delete apiWrapper;

    ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Pixel;
  }

  patchedBufferdata.Free();
  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugThread(uint32_t eventId,
                                            const rdcfixedarray<uint32_t, 3> &groupid,
                                            const rdcfixedarray<uint32_t, 3> &threadid)
{
  return DebugComputeCommon(ShaderStage::Compute, eventId, groupid, threadid);
}

ShaderDebugTrace *VulkanReplay::DebugMeshThread(uint32_t eventId,
                                                const rdcfixedarray<uint32_t, 3> &groupid,
                                                const rdcfixedarray<uint32_t, 3> &threadid)
{
  return DebugComputeCommon(ShaderStage::Mesh, eventId, groupid, threadid);
}

ShaderDebugTrace *VulkanReplay::DebugComputeCommon(ShaderStage stage, uint32_t eventId,
                                                   const rdcfixedarray<uint32_t, 3> &groupid,
                                                   const rdcfixedarray<uint32_t, 3> &threadid)
{
  const VulkanRenderState &state = m_pDriver->GetRenderState();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  rdcstr regionName =
      StringFormat::Fmt("Debug %s @ %u of (%u,%u,%u) (%u,%u,%u)", ToStr(stage).c_str(), eventId,
                        groupid[0], groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]);

  VkMarkerRegion region(regionName);

  if(Vulkan_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(stage == ShaderStage::Compute)
  {
    if(!(action->flags & ActionFlags::Dispatch))
    {
      RDCLOG("No dispatch selected");
      return new ShaderDebugTrace();
    }
  }
  else
  {
    if(!(action->flags & ActionFlags::MeshDispatch))
    {
      RDCLOG("No dispatch selected");
      return new ShaderDebugTrace();
    }
  }

  // get ourselves in pristine state before this dispatch (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  const VulkanStatePipeline &stagePipeState =
      stage == ShaderStage::Compute ? state.compute : state.graphics;
  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[stagePipeState.pipeline];
  const VulkanCreationInfo::ShaderEntry &shaderEntry =
      stagePipeState.shaderObject ? c.m_ShaderObject[state.shaderObjects[(size_t)stage]].shad
                                  : pipe.shaders[(size_t)stage];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[shaderEntry.module];
  rdcstr entryPoint = shaderEntry.entryPoint;
  const rdcarray<SpecConstant> &spec = shaderEntry.specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(stage, entryPoint, stagePipeState.pipeline);

  if(!shadRefl.refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", shadRefl.refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadRefl.PopulateDisassembly(shader.spirv);

  VulkanAPIWrapper *apiWrapper =
      new VulkanAPIWrapper(m_pDriver, c, stage, eventId, shadRefl.refl->resourceId);

  uint32_t threadDim[3];
  threadDim[0] = shadRefl.refl->dispatchThreadsDimension[0];
  threadDim[1] = shadRefl.refl->dispatchThreadsDimension[1];
  threadDim[2] = shadRefl.refl->dispatchThreadsDimension[2];

  SubgroupCapability subgroupCapability = SubgroupCapability::None;
  uint32_t maxSubgroupSize = 1;
  CalculateSubgroupProperties(maxSubgroupSize, subgroupCapability);

  uint32_t numThreads = 1;

  if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    numThreads = RDCMAX(numThreads, maxSubgroupSize);
  if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Workgroup)
    numThreads = RDCMAX(numThreads, threadDim[0] * threadDim[1] * threadDim[2]);

  apiWrapper->thread_builtins.resize(numThreads);
  apiWrapper->thread_props.resize(numThreads);

  std::unordered_map<ShaderBuiltin, ShaderVariable> &global_builtins = apiWrapper->global_builtins;
  global_builtins[ShaderBuiltin::DispatchSize] =
      ShaderVariable(rdcstr(), action->dispatchDimension[0], action->dispatchDimension[1],
                     action->dispatchDimension[2], 0U);
  global_builtins[ShaderBuiltin::GroupSize] =
      ShaderVariable(rdcstr(), threadDim[0], threadDim[1], threadDim[2], 0U);
  global_builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::GroupIndex] =
      ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);

  // if we need to fetch subgroup data, do that now
  uint32_t laneIndex = 0;
  if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    SpecData specData = {};

    specData.globalThreadIdX = groupid[0] * threadDim[0] + threadid[0];
    specData.globalThreadIdY = groupid[1] * threadDim[1] + threadid[1];
    specData.globalThreadIdZ = groupid[2] * threadDim[2] + threadid[2];

    uint32_t paramAlign, structStride;
    rdctie(paramAlign, structStride) = GetAlignAndOutputSize(shadRefl);

    uint32_t maxHits = 4;    // we should only ever get one hit

    // struct size is ResultDataBase header plus Nx structStride for the number of threads
    uint32_t structSize = sizeof(ResultDataBase) + structStride * maxSubgroupSize;

    VkDeviceSize feedbackStorageSize = maxHits * structSize + 1024;

    if(Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
             feedbackStorageSize);
    }

    m_PatchedShaderFeedback.ResizeFeedbackBuffer(m_pDriver, feedbackStorageSize);

    specData.arrayLength = maxHits;

    // make copy of state to draw from
    VulkanRenderState modifiedstate = state;

    VkShaderStageFlagBits stageBit = (VkShaderStageFlagBits)ShaderMaskFromIndex(shadRefl.stageIndex);
    AddedDescriptorData patchedBufferdata = PrepareExtraBufferDescriptor(
        modifiedstate, stage == ShaderStage::Compute, {MakeNewBinding(stageBit)}, false);

    if(patchedBufferdata.empty())
    {
      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = stage;

      return ret;
    }

    if(!patchedBufferdata.descSets.empty())
      m_PatchedShaderFeedback.FeedbackBuffer.WriteDescriptor(Unwrap(patchedBufferdata.descSets[0]),
                                                             0, 0);

    specData.bufferAddress = m_PatchedShaderFeedback.FeedbackBuffer.Address();
    if(specData.bufferAddress && Vulkan_Debug_ShaderDebugLogging())
    {
      RDCLOG("Got buffer address of %llu", specData.bufferAddress);
    }

    // create shader with modified code

    VkSpecializationMapEntry specMaps[(size_t)InputSpecConstant::Count];
    RDCCOMPILE_ASSERT(sizeof(specMaps) == sizeof(specMapsTemplate),
                      "Specialisation maps have changed");

    VkSpecializationInfo patchedSpecInfo = MakeSpecInfo(specData, specMaps);

    auto patchCallback = [this, stageBit, &shadRefl, &patchedSpecInfo, subgroupCapability,
                          maxSubgroupSize](const AddedDescriptorData &patchedBufferdata,
                                           VkShaderStageFlagBits stage, const char *entryName,
                                           const rdcarray<uint32_t> &origSpirv,
                                           rdcarray<uint32_t> &modSpirv,
                                           const VkSpecializationInfo *&specInfo) {
      if(stage != stageBit)
        return false;

      modSpirv = origSpirv;

      uint32_t idx = shadRefl.stageIndex;

      static const rdcstr filename[NumShaderStages] = {
          "shadinput_vertex.spv",   "shadinput_hull.spv",  "shadinput_domain.spv",
          "shadinput_geometry.spv", "shadinput_pixel.spv", "shadinput_compute.spv",
          "shadinput_task.spv",     "shadinput_mesh.spv",
      };

      if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
        FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/before_" + filename[idx], modSpirv);

      CreateInputFetcher(modSpirv, shadRefl, m_StorageMode, false, false, false, subgroupCapability,
                         maxSubgroupSize);

      if(!Vulkan_Debug_PSDebugDumpDirPath().empty())
        FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath() + "/after_" + filename[idx], modSpirv);

      // overwrite user's specialisation info, assuming that the old specialisation info is not
      // relevant for codegen (the only thing it would be used for)
      specInfo = &patchedSpecInfo;

      return true;
    };

    PrepareStateForPatchedShader(patchedBufferdata, modifiedstate, stage == ShaderStage::Compute,
                                 patchCallback);

    if(!RunFeedbackAction(feedbackStorageSize, action, modifiedstate))
    {
      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = stage;

      return ret;
    }

    bytebuf data;
    GetDebugManager()->GetBufferData(m_PatchedShaderFeedback.FeedbackBuffer, 0, 0, data);

    byte *base = data.data();
    uint32_t hit_count = ((uint32_t *)base)[0];
    // uint32_t total_count = ((uint32_t *)base)[1];

    if(hit_count > maxHits)
    {
      RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", hit_count, maxHits);
      hit_count = maxHits;
    }

    base += sizeof(Vec4f);

    ResultDataBase *winner = (ResultDataBase *)base;

    if(winner->valid != validMagicNumber)
    {
      RDCWARN("Hit doesn't have valid magic number");

      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = stage;

      return ret;
    }

    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shader.spirv.GetSPIRV());

    // the per-thread data immediately follows the ResultDataBase header. Every piece of data is
    // uniformly aligned, either 16-byte by default or 32-byte if larger components exist. The
    // output is in input signature order.
    byte *LaneData = (byte *)(winner + 1);

    numThreads = 4;
    const uint32_t subgroupSize = winner->subgroupSize;

    if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    {
      RDCASSERTNOTEQUAL(subgroupSize, 0);
      numThreads = RDCMAX(numThreads, subgroupSize);
    }

    if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Workgroup)
    {
      numThreads = RDCMAX(numThreads, threadDim[0] * threadDim[1] * threadDim[2]);
    }

    apiWrapper->global_builtins[ShaderBuiltin::NumSubgroups] =
        ShaderVariable(rdcstr(), winner->numSubgroups, 0U, 0U, 0U);

    apiWrapper->location_inputs.resize(numThreads);
    apiWrapper->thread_builtins.resize(numThreads);
    apiWrapper->thread_props.resize(numThreads);

    laneIndex = ~0U;

    for(uint32_t t = 0; t < subgroupSize; t++)
    {
      byte *value = LaneData + t * structStride;

      SubgroupLaneData *subgroupData = (SubgroupLaneData *)value;
      value += sizeof(SubgroupLaneData);

      ComputeLaneData *compData = (ComputeLaneData *)value;
      value += sizeof(ComputeLaneData);

      // should we try to verify that the GPU assigned subgroups as we expect? this assumes tightly wrapped subgroups
      uint32_t lane = t;

      if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Workgroup)
      {
        lane = compData->threadid[2] * threadDim[0] * threadDim[1] +
               compData->threadid[1] * threadDim[0] + compData->threadid[0];
      }

      if(rdcfixedarray<uint32_t, 3>(compData->threadid) == threadid && subgroupData->isActive)
        laneIndex = lane;

      apiWrapper->thread_props[lane][(size_t)rdcspv::ThreadProperty::Active] = subgroupData->isActive;
      apiWrapper->thread_props[lane][(size_t)rdcspv::ThreadProperty::Elected] = subgroupData->elect;
      apiWrapper->thread_props[lane][(size_t)rdcspv::ThreadProperty::SubgroupId] = t;

      apiWrapper->thread_builtins[lane][ShaderBuiltin::DispatchThreadIndex] =
          ShaderVariable(rdcstr(), groupid[0] * threadDim[0] + compData->threadid[0],
                         groupid[1] * threadDim[1] + compData->threadid[1],
                         groupid[2] * threadDim[2] + compData->threadid[2], 0U);
      apiWrapper->thread_builtins[lane][ShaderBuiltin::GroupThreadIndex] = ShaderVariable(
          rdcstr(), compData->threadid[0], compData->threadid[1], compData->threadid[2], 0U);
      apiWrapper->thread_builtins[lane][ShaderBuiltin::GroupFlatIndex] =
          ShaderVariable(rdcstr(),
                         compData->threadid[2] * threadDim[0] * threadDim[1] +
                             compData->threadid[1] * threadDim[0] + compData->threadid[0],
                         0U, 0U, 0U);
      apiWrapper->thread_builtins[lane][ShaderBuiltin::IndexInSubgroup] =
          ShaderVariable(rdcstr(), t, 0U, 0U, 0U);
      apiWrapper->thread_builtins[lane][ShaderBuiltin::SubgroupIndexInWorkgroup] =
          ShaderVariable(rdcstr(), compData->subIdxInGroup, 0U, 0U, 0U);
    }

    if(laneIndex == ~0U)
    {
      RDCERR("Didn't find desired lane in subgroup data");
      laneIndex = 0;
    }

    // if we're simulating the whole workgroup we need to fill in the thread IDs of other threads
    if(shadRefl.patchData.threadScope & rdcspv::ThreadScope::Workgroup)
    {
      uint32_t i = 0;
      for(uint32_t tz = 0; tz < threadDim[2]; tz++)
      {
        for(uint32_t ty = 0; ty < threadDim[1]; ty++)
        {
          for(uint32_t tx = 0; tx < threadDim[0]; tx++)
          {
            std::unordered_map<ShaderBuiltin, ShaderVariable> &thread_builtins =
                apiWrapper->thread_builtins[i];

            thread_builtins[ShaderBuiltin::GroupThreadIndex] =
                ShaderVariable(rdcstr(), tx, ty, tz, 0U);
            thread_builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
                rdcstr(), tz * threadDim[0] * threadDim[1] + ty * threadDim[0] + tx, 0U, 0U, 0U);

            if(apiWrapper->thread_props[i][(size_t)rdcspv::ThreadProperty::Active])
            {
              // assert that this is the thread we expect it to be
              RDCASSERTEQUAL(thread_builtins[ShaderBuiltin::DispatchThreadIndex].value.u32v[0],
                             groupid[0] * threadDim[0] + tx);
              RDCASSERTEQUAL(thread_builtins[ShaderBuiltin::DispatchThreadIndex].value.u32v[1],
                             groupid[1] * threadDim[1] + ty);
              RDCASSERTEQUAL(thread_builtins[ShaderBuiltin::DispatchThreadIndex].value.u32v[2],
                             groupid[2] * threadDim[2] + tz);

              RDCASSERTEQUAL(thread_builtins[ShaderBuiltin::IndexInSubgroup].value.u32v[0],
                             i % subgroupSize);
              RDCASSERTEQUAL(thread_builtins[ShaderBuiltin::SubgroupIndexInWorkgroup].value.u32v[0],
                             i / subgroupSize);
            }
            else
            {
              thread_builtins[ShaderBuiltin::DispatchThreadIndex] =
                  ShaderVariable(rdcstr(), groupid[0] * threadDim[0] + tx,
                                 groupid[1] * threadDim[1] + ty, groupid[2] * threadDim[2] + tz, 0U);
              // tightly wrap subgroups, this is likely not how the GPU actually assigns them
              thread_builtins[ShaderBuiltin::IndexInSubgroup] =
                  ShaderVariable(rdcstr(), i % subgroupSize, 0U, 0U, 0U);
              thread_builtins[ShaderBuiltin::SubgroupIndexInWorkgroup] =
                  ShaderVariable(rdcstr(), i / subgroupSize, 0U, 0U, 0U);
              apiWrapper->thread_props[i][(size_t)rdcspv::ThreadProperty::Active] = 1;
              apiWrapper->thread_props[i][(size_t)rdcspv::ThreadProperty::SubgroupId] =
                  i % subgroupSize;
            }

            i++;
          }
        }
      }
    }

    // Add inactive padding lanes to round up to the subgroup size
    const uint32_t numPaddingThreads = AlignUp(numThreads, subgroupSize) - numThreads;
    if(numPaddingThreads > 0)
    {
      uint32_t newNumThreads = numThreads + numPaddingThreads;
      apiWrapper->thread_props.resize(newNumThreads);
      apiWrapper->thread_builtins.resize(newNumThreads);
      for(uint32_t i = numThreads; i < newNumThreads; ++i)
      {
        std::unordered_map<ShaderBuiltin, ShaderVariable> &thread_builtins =
            apiWrapper->thread_builtins[i];

        thread_builtins[ShaderBuiltin::DispatchThreadIndex] =
            ShaderVariable(rdcstr(), -1, -1, -1, -1);
        thread_builtins[ShaderBuiltin::GroupThreadIndex] = ShaderVariable(rdcstr(), -1, -1, -1, -1);
        thread_builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(rdcstr(), -1, -1, -1, -1);
        thread_builtins[ShaderBuiltin::IndexInSubgroup] =
            ShaderVariable(rdcstr(), i % subgroupSize, 0U, 0U, 0U);
        thread_builtins[ShaderBuiltin::SubgroupIndexInWorkgroup] =
            ShaderVariable(rdcstr(), i / subgroupSize, 0U, 0U, 0U);
        apiWrapper->thread_props[i][(size_t)rdcspv::ThreadProperty::Active] = 0;
        apiWrapper->thread_props[i][(size_t)rdcspv::ThreadProperty::SubgroupId] = i % subgroupSize;
      }
      numThreads = newNumThreads;
    }
    apiWrapper->global_builtins[ShaderBuiltin::SubgroupSize] =
        ShaderVariable(rdcstr(), subgroupSize, 0U, 0U, 0U);

    ShaderDebugTrace *ret =
        debugger->BeginDebug(apiWrapper, stage, entryPoint, spec, shadRefl.instructionLines,
                             shadRefl.patchData, laneIndex, numThreads, subgroupSize);
    apiWrapper->ResetReplay();

    return ret;
  }
  else
  {
    // if we have more than one thread here, that means we need to simulate the whole workgroup.
    // we assume the layout of this is irrelevant and don't attempt to read it back from the GPU
    // like we do with subgroups. We lay things out in plain linear order, along X and then Y and
    // then Z, with groups iterated together.
    if(numThreads > 1)
    {
      uint32_t i = 0;
      for(uint32_t tz = 0; tz < threadDim[2]; tz++)
      {
        for(uint32_t ty = 0; ty < threadDim[1]; ty++)
        {
          for(uint32_t tx = 0; tx < threadDim[0]; tx++)
          {
            std::unordered_map<ShaderBuiltin, ShaderVariable> &thread_builtins =
                apiWrapper->thread_builtins[i];
            thread_builtins[ShaderBuiltin::DispatchThreadIndex] =
                ShaderVariable(rdcstr(), groupid[0] * threadDim[0] + tx,
                               groupid[1] * threadDim[1] + ty, groupid[2] * threadDim[2] + tz, 0U);
            thread_builtins[ShaderBuiltin::GroupThreadIndex] =
                ShaderVariable(rdcstr(), tx, ty, tz, 0U);
            thread_builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
                rdcstr(), tz * threadDim[0] * threadDim[1] + ty * threadDim[0] + tx, 0U, 0U, 0U);
            apiWrapper->thread_props[i][(size_t)rdcspv::ThreadProperty::Active] = 1;

            if(rdcfixedarray<uint32_t, 3>({tx, ty, tz}) == threadid)
            {
              laneIndex = i;
            }

            i++;
          }
        }
      }
    }
    else
    {
      // simple single-thread case
      apiWrapper->thread_props[0][(size_t)rdcspv::ThreadProperty::Active] = 1;
      apiWrapper->thread_props[0][(size_t)rdcspv::ThreadProperty::SubgroupId] = 0;

      std::unordered_map<ShaderBuiltin, ShaderVariable> &thread_builtins =
          apiWrapper->thread_builtins[0];

      thread_builtins[ShaderBuiltin::DispatchThreadIndex] = ShaderVariable(
          rdcstr(), groupid[0] * threadDim[0] + threadid[0],
          groupid[1] * threadDim[1] + threadid[1], groupid[2] * threadDim[2] + threadid[2], 0U);
      thread_builtins[ShaderBuiltin::GroupThreadIndex] =
          ShaderVariable(rdcstr(), threadid[0], threadid[1], threadid[2], 0U);
      thread_builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
          rdcstr(),
          threadid[2] * threadDim[0] * threadDim[1] + threadid[1] * threadDim[0] + threadid[0], 0U,
          0U, 0U);
    }

    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shader.spirv.GetSPIRV());

    apiWrapper->global_builtins[ShaderBuiltin::SubgroupSize] =
        ShaderVariable(rdcstr(), 1U, 0U, 0U, 0U);

    ShaderDebugTrace *ret =
        debugger->BeginDebug(apiWrapper, stage, entryPoint, spec, shadRefl.instructionLines,
                             shadRefl.patchData, laneIndex, numThreads, 1);
    apiWrapper->ResetReplay();

    return ret;
  }
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
      m_ShaderDebugData.DummyWrites[fmt][dim].dstSet = VK_NULL_HANDLE;
      m_ShaderDebugData.DummyWrites[fmt][dim].pImageInfo =
          &m_ShaderDebugData.DummyImageInfos[fmt][dim];
    }

    m_ShaderDebugData.DummyImageInfos[fmt][5].sampler = Unwrap(m_TexRender.DummySampler);

    m_ShaderDebugData.DummyWrites[fmt][5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    m_ShaderDebugData.DummyWrites[fmt][5].descriptorCount = 1;
    m_ShaderDebugData.DummyWrites[fmt][5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    m_ShaderDebugData.DummyWrites[fmt][5].dstBinding = (uint32_t)ShaderDebugBind::Sampler;
    m_ShaderDebugData.DummyWrites[fmt][5].dstSet = VK_NULL_HANDLE;
    m_ShaderDebugData.DummyWrites[fmt][5].pImageInfo = &m_ShaderDebugData.DummyImageInfos[fmt][5];

    if(m_TexRender.DummyBufferView[fmt] != VK_NULL_HANDLE)
    {
      m_ShaderDebugData.DummyWrites[fmt][6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      m_ShaderDebugData.DummyWrites[fmt][6].descriptorCount = 1;
      m_ShaderDebugData.DummyWrites[fmt][6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
      m_ShaderDebugData.DummyWrites[fmt][6].dstBinding = (uint32_t)ShaderDebugBind::Buffer;
      m_ShaderDebugData.DummyWrites[fmt][6].dstSet = VK_NULL_HANDLE;
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
  Threading::JobSystem::SyncAllJobs();
}
