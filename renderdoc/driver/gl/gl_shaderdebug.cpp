/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
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

#include "gl_shaderdebug.h"
#include "core/settings.h"
#include "data/glsl_shaders.h"
#include "driver/shaders/spirv/spirv_debug.h"
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "maths/formatpacking.h"
#include "replay/common/var_dispatch_helpers.h"
#include "gl_driver.h"
#include "gl_replay.h"

RDOC_CONFIG(rdcstr, OpenGL_Debug_ShaderDebugDumpDirPath, "",
            "Path to dump shader debugging generated SPIR-V files.");
RDOC_CONFIG(bool, OpenGL_Debug_ShaderDebugLogging, false,
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

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

#if ENABLED(RDOC_DEVEL)
#define CHECK_DEVICE_THREAD() \
  RDCASSERTMSG("API Wrapper function called from non-device thread!", IsDeviceThread());
#else
#define CHECK_DEVICE_THREAD()
#endif

class GLAPIWrapper : public rdcspv::DebugAPIWrapper
{
public:
  GLAPIWrapper(WrappedOpenGL *gl, ShaderStage stage, uint32_t eid, ResourceId shadId,
               const ShaderReflection *autoMappedRefl)
      : m_EventID(eid), m_ShaderID(shadId), deviceThreadID(Threading::GetCurrentID())
  {
    m_pDriver = gl;

    // if we were passed a reflection, we need to look up in the existing program by name all the
    // uniform locations since they may not match what glslang assigned out
    // this is only necessary for bare uniforms: resources like textures are referenced by their interface
    // index (via the DescriptorAccess) not whatever binding they might have been given, and input
    // variables are filled out without ever going to the real program via the input fetcher
    if(autoMappedRefl)
    {
      GLint prog = 0;
      GL.glGetIntegerv(eGL_CURRENT_PROGRAM, &prog);
      for(const ConstantBlock &cblock : autoMappedRefl->constantBlocks)
      {
        // uniforms need in-depth handling of their own to query out all locations
        if(!cblock.bufferBacked && cblock.name == "$Globals")
        {
          const rdcstr prefix;
          for(const ShaderConstant &c : cblock.variables)
          {
            // only the base value has a location so we just set it here, then GetUniformMapping
            // increments as it goes
            int location = c.byteOffset;
            GetUniformMapping(prog, location, prefix, c);
          }
        }
      }
    }

    // when we're first setting up, the state is pristine and no replay is needed
    m_ResourcesDirty = false;

    GLReplay *replay = m_pDriver->GetReplay();

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

        store = acc.descriptorStore;
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
  }

  ~GLAPIWrapper()
  {
    CHECK_DEVICE_THREAD();

    GL.glDeleteBuffers(1, &m_UBO);
    GL.glDeleteBuffers(1, &m_MathBuffer);
    GL.glDeleteBuffers(1, &m_SampleBuffer);

    GL.glDeleteTextures(1, &m_ReadbackTex);
    GL.glDeleteFramebuffers(1, &m_ReadbackFBO);
  }

  void ResetReplay()
  {
    CHECK_DEVICE_THREAD();
    if(!m_ResourcesDirty)
    {
      GLMarkerRegion region("ResetReplay");
      // replay the action to get back to 'normal' state for this event, and mark that we need to
      // replay back to pristine state next time we need to fetch data.
      m_pDriver->GetReplay()->ReplayLog(m_EventID, eReplay_OnlyDraw);
    }
    m_ResourcesDirty = true;
  }

  virtual void AddDebugMessage(MessageCategory cat, MessageSeverity sev, MessageSource src,
                               rdcstr desc) override
  {
    CHECK_DEVICE_THREAD();
    m_pDriver->AddDebugMessage(cat, sev, src, desc);
  }

  virtual GraphicsAPI GetGraphicsAPI() override { return GraphicsAPI::OpenGL; }
  virtual bool SimulateThreaded() override { return false; }

  virtual ResourceId GetShaderID() override { return m_ShaderID; }

  virtual void ReadAddress(uint64_t address, uint64_t byteSize, void *dst) override
  {
    RDCERR("Unsupported address operation");
    return;
  }
  virtual void WriteAddress(uint64_t address, uint64_t byteSize, const void *src) override
  {
    RDCERR("Unsupported address operation");
    return;
  }
  virtual bool IsBufferCached(uint64_t address) override
  {
    RDCERR("Unsupported address operation");
    return false;
  }

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

  virtual void ReadLocationValue(int32_t location, ShaderVariable &var) override
  {
    GLint prog = 0;
    GL.glGetIntegerv(eGL_CURRENT_PROGRAM, &prog);

    if(!m_UniformLocationRemap.empty())
      location = m_UniformLocationRemap[location];

    if(location < 0)
    {
      var.value.u8v.clear();
      return;
    }

    if(var.type == VarType::Float)
    {
      GL.glGetUniformfv(prog, location, var.value.f32v.data());
    }
    else if(var.type == VarType::UInt)
    {
      GL.glGetUniformuiv(prog, location, var.value.u32v.data());
    }
    else if(var.type == VarType::SInt)
    {
      GL.glGetUniformiv(prog, location, var.value.s32v.data());
    }
    else if(var.type == VarType::Bool)
    {
      GL.glGetUniformuiv(prog, location, var.value.u32v.data());
    }
    else
    {
      RDCERR("Unexpected type of variable");
    }

    // If the uniform queried is a matrix, the values of the matrix are returned in column major order.
    if(var.columns > 1)
    {
      ShaderVariable tmp = var;
      for(uint8_t r = 0; r < var.rows; r++)
        for(uint8_t c = 0; c < var.columns; c++)
          copyComp(var, r * var.columns + c, tmp, c * var.rows + r);
    }
  }

  virtual void ReadBufferValue(const ShaderBindIndex &bind, uint64_t offset, uint64_t byteSize,
                               void *dst) override
  {
    if(bind.category == DescriptorCategory::Unknown)
    {
      // invalid index, return no data
      memset(dst, 0, (size_t)byteSize);
      return;
    }

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

    DebugSampleUBO uniformParams = {};

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

    GLMarkerRegion markerRegion("QueueSampleGather");

    GLResource texture = m_pDriver->GetResourceManager()->GetResource(imageDescriptor.resource);
    GLResource bufTexture =
        m_pDriver->GetResourceManager()->GetResource(bufferViewDescriptor.resource);
    GLResource sampler = m_pDriver->GetResourceManager()->GetResource(samplerDescriptor.object);

    // NULL texture : return 0,0,0,0
    if(!buffer && (texture.name == 0))
    {
      memset(&output.value, 0, sizeof(output.value));
      hasResult = true;
      return true;
    }

    WrappedOpenGL::TextureData &texDetails = m_pDriver->m_Textures[imageDescriptor.resource];

    SamplingProgramConfig config;

    config.resType = SamplingProgramConfig::Float;
    if(uintTex)
      config.resType = SamplingProgramConfig::UInt;
    else if(sintTex)
      config.resType = SamplingProgramConfig::SInt;

    // how many co-ordinates should there be
    int coords = 0, gradCoords = 0;
    if(buffer)
    {
      config.dim = SamplingProgramConfig::TexBuffer;
      coords = gradCoords = 1;
    }
    else
    {
      switch(texDetails.curType)
      {
        case eGL_TEXTURE_1D:
          coords = 1;
          gradCoords = 1;
          config.dim = SamplingProgramConfig::Tex1D;
          break;
        case eGL_TEXTURE_2D:
          coords = 2;
          gradCoords = 2;
          config.dim = SamplingProgramConfig::Tex2D;
          break;
        case eGL_TEXTURE_3D:
          coords = 3;
          gradCoords = 3;
          config.dim = SamplingProgramConfig::Tex3D;
          break;
        case eGL_TEXTURE_CUBE_MAP:
          coords = 3;
          gradCoords = 3;
          config.dim = SamplingProgramConfig::TexCube;
          break;
        case eGL_TEXTURE_1D_ARRAY:
          coords = 2;
          gradCoords = 1;
          config.dim = SamplingProgramConfig::Tex1DArray;
          break;
        case eGL_TEXTURE_2D_ARRAY:
          coords = 3;
          gradCoords = 2;
          config.dim = SamplingProgramConfig::Tex2DArray;
          break;
        case eGL_TEXTURE_CUBE_MAP_ARRAY:
          coords = 4;
          gradCoords = 3;
          config.dim = SamplingProgramConfig::TexCubeArray;
          break;
        case eGL_TEXTURE_RECTANGLE:
          coords = 2;
          gradCoords = 2;
          config.dim = SamplingProgramConfig::Tex2DRect;
          break;
        case eGL_TEXTURE_BUFFER:
          coords = 1;
          gradCoords = 1;
          config.dim = SamplingProgramConfig::TexBuffer;
          break;
        case eGL_TEXTURE_2D_MULTISAMPLE:
          coords = 2;
          gradCoords = 2;
          config.dim = SamplingProgramConfig::Tex2DMS;
          break;
        case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
          coords = 3;
          gradCoords = 2;
          config.dim = SamplingProgramConfig::Tex2DMSArray;
          break;
        default: RDCERR("Invalid texture type %s", ToStr(texDetails.curType).c_str()); return false;
      }
    }

    GLint firstMip = 0, numMips = 1;
    if(texture.name)
    {
      GL.glGetTextureParameterivEXT(texture.name, texDetails.curType, eGL_TEXTURE_BASE_LEVEL,
                                    &firstMip);
      GL.glGetTextureParameterivEXT(texture.name, texDetails.curType, eGL_TEXTURE_MAX_LEVEL,
                                    &numMips);
    }

    // handle query opcodes now
    switch(opcode)
    {
      case rdcspv::Op::ImageQueryLevels:
      {
        output.value.u32v[0] = numMips;
        hasResult = true;
        return true;
      }
      case rdcspv::Op::ImageQuerySamples:
      {
        output.value.u32v[0] = (uint32_t)RDCMAX(1, texDetails.samples);
        hasResult = true;
        return true;
      }
      case rdcspv::Op::ImageQuerySize:
      case rdcspv::Op::ImageQuerySizeLod:
      {
        uint32_t mip = firstMip;

        if(opcode == rdcspv::Op::ImageQuerySizeLod)
          mip += uintComp(lane.GetSrc(operands.lod), 0);

        RDCEraseEl(output.value);

        int i = 0;
        setUintComp(output, i++, RDCMAX(1, texDetails.width >> mip));
        if(coords >= 2)
          setUintComp(output, i++, RDCMAX(1, texDetails.height >> mip));
        if(texDetails.curType == eGL_TEXTURE_3D)
          setUintComp(output, i++, RDCMAX(1, texDetails.depth >> mip));

        if(texDetails.curType == eGL_TEXTURE_1D_ARRAY)
          setUintComp(output, i++, texDetails.height);
        else if(texDetails.curType == eGL_TEXTURE_2D_ARRAY)
          setUintComp(output, i++, texDetails.depth);
        else if(texDetails.curType == eGL_TEXTURE_CUBE_MAP ||
                texDetails.curType == eGL_TEXTURE_CUBE_MAP_ARRAY)
          setUintComp(output, i++, texDetails.depth / 6);

        if(buffer)
        {
          uint64_t size = bufferViewDescriptor.byteSize;
          GLenum format = MakeGLFormat(bufferViewDescriptor.format);

          setUintComp(
              output, 0,
              uint32_t(size / GetByteSize(1, 1, 1, GetBaseFormat(format), GetDataType(format))));
        }

        hasResult = true;
        return true;
      }
      default: break;
    }

    bool lodBiasRestore = false;
    float lodBiasRestoreValue = 0.0f;

    if(operands.flags & rdcspv::ImageOperands::Bias)
    {
      const ShaderVariable &biasVar = lane.GetSrc(operands.bias);

      // silently cast parameters to 32-bit floats
      float bias = floatComp(biasVar, 0);

      if(bias != 0.0f)
      {
        // bias can only be used with implicit lod operations, but we want to do everything with
        // explicit lod operations. So we instead push the bias into the sampler itself, which is
        // entirely equivalent.

        // can't do this on GLES, so we have to use implicit lod path
        if(IsGLES)
        {
          uniformParams.gles_bias = bias;
          config.manualBias = true;
        }
        else
        {
          lodBiasRestore = true;
          if(sampler.name)
          {
            GL.glGetSamplerParameterfv(sampler.name, eGL_TEXTURE_LOD_BIAS, &lodBiasRestoreValue);
            GL.glSamplerParameterf(sampler.name, eGL_TEXTURE_LOD_BIAS, lodBiasRestoreValue + bias);
          }
          else
          {
            GL.glGetTextureParameterfvEXT(texture.name, texDetails.curType, eGL_TEXTURE_LOD_BIAS,
                                          &lodBiasRestoreValue);
            float val = lodBiasRestoreValue + bias;
            GL.glTextureParameterfvEXT(texture.name, texDetails.curType, eGL_TEXTURE_LOD_BIAS, &val);
          }
        }
      }
    }

    switch(opcode)
    {
      case rdcspv::Op::ImageFetch: config.op = SamplingProgramConfig::Fetch; break;
      case rdcspv::Op::ImageQueryLod: config.op = SamplingProgramConfig::QueryLod; break;
      case rdcspv::Op::ImageSampleExplicitLod:
      case rdcspv::Op::ImageSampleImplicitLod:
      case rdcspv::Op::ImageSampleProjExplicitLod:
      case rdcspv::Op::ImageSampleProjImplicitLod: config.op = SamplingProgramConfig::Sample; break;
      case rdcspv::Op::ImageSampleDrefExplicitLod:
      case rdcspv::Op::ImageSampleDrefImplicitLod:
      case rdcspv::Op::ImageSampleProjDrefExplicitLod:
      case rdcspv::Op::ImageSampleProjDrefImplicitLod:
        config.op = SamplingProgramConfig::SampleDref;
        break;
      case rdcspv::Op::ImageGather: config.op = SamplingProgramConfig::Gather; break;
      case rdcspv::Op::ImageDrefGather: config.op = SamplingProgramConfig::GatherDref; break;
      default:
      {
        RDCERR("Unsupported opcode %s", ToStr(opcode).c_str());
        hasResult = false;
        return false;
      }
    }

    // proj opcodes have an extra q parameter, but we do the divide ourselves and 'demote' these to
    // non-proj variants
    bool proj = false;
    switch(opcode)
    {
      case rdcspv::Op::ImageSampleProjExplicitLod:
      case rdcspv::Op::ImageSampleProjImplicitLod:
      case rdcspv::Op::ImageSampleProjDrefExplicitLod:
      case rdcspv::Op::ImageSampleProjDrefImplicitLod:
      {
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
          uniformParams.uvwa.fv[i] = floatComp(uv, i);

        if(useCompare)
          uniformParams.compare = floatComp(compare, 0);

        config.gatherChannel = (uint32_t)gatherChannel;

        if(operands.flags & rdcspv::ImageOperands::ConstOffsets)
        {
          ShaderVariable constOffsets = lane.GetSrc(operands.constOffsets);

          config.useGatherOffs = true;

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

          config.gatherOffsets[0] = constOffsets.members[0].value.s32v[0];
          config.gatherOffsets[1] = constOffsets.members[0].value.s32v[1];
          config.gatherOffsets[2] = constOffsets.members[1].value.s32v[0];
          config.gatherOffsets[3] = constOffsets.members[1].value.s32v[1];
          config.gatherOffsets[4] = constOffsets.members[2].value.s32v[0];
          config.gatherOffsets[5] = constOffsets.members[2].value.s32v[1];
          config.gatherOffsets[6] = constOffsets.members[3].value.s32v[0];
          config.gatherOffsets[7] = constOffsets.members[3].value.s32v[1];
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
          uniformParams.uvwa.fv[i] = floatComp(uv, i);

        if(proj)
        {
          // coords shouldn't be 4 because that's only valid for cube arrays which can't be
          // projected
          RDCASSERT(coords < 4);

          // do the divide ourselves rather than severely complicating the sample shader (as proj
          // variants need non-arrayed textures)
          float q = floatComp(uv, coords);

          uniformParams.uvwa.fv[0] /= q;
          uniformParams.uvwa.fv[1] /= q;
          uniformParams.uvwa.fv[2] /= q;
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
          config.useGrad = false;
        }
        else if(operands.flags & rdcspv::ImageOperands::Grad)
        {
          ShaderVariable ddx = lane.GetSrc(operands.grad.first);
          ShaderVariable ddy = lane.GetSrc(operands.grad.second);

          config.useGrad = true;

          // silently cast parameters to 32-bit floats
          RDCASSERTEQUAL(ddx.type, ddy.type);
          for(int i = 0; i < gradCoords; i++)
          {
            uniformParams.ddx_uvw.fv[i] = floatComp(ddx, i);
            uniformParams.ddy_uvw.fv[i] = floatComp(ddy, i);
          }
        }

        if(opcode == rdcspv::Op::ImageSampleImplicitLod ||
           opcode == rdcspv::Op::ImageSampleProjImplicitLod || opcode == rdcspv::Op::ImageQueryLod)
        {
          // use grad to sub in for the implicit lod
          config.useGrad = true;

          // silently cast parameters to 32-bit floats
          RDCASSERTEQUAL(ddxCalc.type, ddyCalc.type);
          for(int i = 0; i < gradCoords; i++)
          {
            uniformParams.ddx_uvw.fv[i] = floatComp(ddxCalc, i);
            uniformParams.ddy_uvw.fv[i] = floatComp(ddyCalc, i);
          }
        }

        break;
      }
      default: break;
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
      if(gatherOp)
      {
        uniformParams.dynoffset.x = constOffset.value.s32v[0];
        if(gradCoords >= 2)
          uniformParams.dynoffset.y = constOffset.value.s32v[1];
        if(gradCoords >= 3)
          uniformParams.dynoffset.z = constOffset.value.s32v[2];
      }
      else
      {
        config.fetchOffset.x = constOffset.value.s32v[0];
        if(gradCoords >= 2)
          config.fetchOffset.y = constOffset.value.s32v[1];
        if(gradCoords >= 3)
          config.fetchOffset.z = constOffset.value.s32v[2];
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
      uniformParams.dynoffset.x = offset.value.s32v[0];
      if(gradCoords >= 2)
        uniformParams.dynoffset.y = offset.value.s32v[1];
      if(gradCoords >= 3)
        uniformParams.dynoffset.z = offset.value.s32v[2];
    }

    GLuint prog = m_pDriver->GetReplay()->MakeShaderDebugSampleProg(config);

    if(prog == 0)
    {
      m_pDriver->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 "Failed to compile graphics program for sampling operation");
      return false;
    }

    m_pDriver->GetReplay()->UseReplayContext();

    GLRenderState rs;
    rs.FetchState(m_pDriver);

    // do this 'lazily' so we are already inside the state push and pop
    if(m_UBO == 0)
    {
      GL.glGenBuffers(1, &m_UBO);
      GL.glBindBuffer(eGL_UNIFORM_BUFFER, m_UBO);
      GL.glNamedBufferDataEXT(m_UBO, 2048, NULL, eGL_DYNAMIC_DRAW);

      GL.glGenFramebuffers(1, &m_ReadbackFBO);
      GL.glBindFramebuffer(eGL_FRAMEBUFFER, m_ReadbackFBO);

      GL.glGenTextures(1, &m_ReadbackTex);
      GL.glBindTexture(eGL_TEXTURE_2D, m_ReadbackTex);

      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glTextureImage2DEXT(m_ReadbackTex, eGL_TEXTURE_2D, 0, eGL_RGBA32F, 1, 1, 0, eGL_RGBA,
                             eGL_FLOAT, NULL);
      GL.glTextureParameteriEXT(m_ReadbackTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
      GL.glTextureParameteriEXT(m_ReadbackTex, eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
      GL.glTextureParameteriEXT(m_ReadbackTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
      GL.glTextureParameteriEXT(m_ReadbackTex, eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
      GL.glTextureParameteriEXT(m_ReadbackTex, eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      GL.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                                m_ReadbackTex, 0);
    }

    if(m_SampleOffset >= m_SampleBufferSize || m_SampleBuffer == 0)
    {
      m_SampleBufferSize = m_SampleBufferSize * 2 + 1024 * mathOpResultByteSize;

      GLuint oldBuf = m_SampleBuffer;
      GLsizeiptr oldSize = m_SampleBufferSize;

      // resize the buffer up
      GL.glGenBuffers(1, &m_SampleBuffer);
      GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, m_SampleBuffer);
      GL.glNamedBufferDataEXT(m_SampleBuffer, m_SampleBufferSize, NULL, eGL_DYNAMIC_DRAW);

      if(oldBuf)
        GL.glNamedCopyBufferSubDataEXT(oldBuf, m_SampleBuffer, 0, 0, oldSize);
      GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
    }

    GL.glUseProgram(prog);

    GL.glActiveTexture(eGL_TEXTURE0);
    if(texture.name)
      GL.glBindTexture(texDetails.curType, texture.name);
    if(bufTexture.name)
      GL.glBindTexture(eGL_TEXTURE_BUFFER, bufTexture.name);
    if(sampler.name)
      GL.glBindSampler(0, sampler.name);

    GL.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, m_UBO);
    DebugSampleUBO *cdata =
        (DebugSampleUBO *)GL.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(DebugSampleUBO),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    memcpy(cdata, &uniformParams, sizeof(uniformParams));
    GL.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    // set UVW/DDX/DDY for vertex shader
    GL.glUniform4fv(GL.glGetUniformLocation(prog, "in_uvwa"), 1, &uniformParams.uvwa.x);
    GL.glUniform4fv(GL.glGetUniformLocation(prog, "in_ddx"), 1, &uniformParams.ddx_uvw.x);
    GL.glUniform4fv(GL.glGetUniformLocation(prog, "in_ddy"), 1, &uniformParams.ddy_uvw.x);

    GL.glBindFramebuffer(eGL_FRAMEBUFFER, m_ReadbackFBO);

    float pixel[4] = {};
    GL.glClearBufferfv(eGL_COLOR, 0, pixel);

    if(HasExt[EXT_depth_bounds_test])
      GL.glDisable(eGL_DEPTH_BOUNDS_TEST_EXT);
    GL.glDisable(eGL_DEPTH_TEST);
    GL.glDisable(eGL_STENCIL_TEST);
    GL.glDisable(eGL_CULL_FACE);
    if(HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample])
      GL.glDisable(eGL_SAMPLE_MASK);
    GL.glDisable(eGL_SCISSOR_TEST);
    GL.glDisable(eGL_BLEND);
    GL.glViewport(0, 0, 1, 1);
    GL.glDrawArrays(eGL_TRIANGLES, 0, 3);

    RDCASSERT(m_SampleOffset + sampleGatherOpResultByteSize <= m_SampleBufferSize, m_SampleOffset,
              sampleGatherOpResultByteSize, m_SampleBufferSize);

    GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, m_SampleBuffer);
    GL.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)m_SampleOffset);
    GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
    m_SampleOffset += sampleGatherOpResultByteSize;

    hasResult = false;

    if(lodBiasRestore)
    {
      if(sampler.name)
        GL.glSamplerParameterf(sampler.name, eGL_TEXTURE_LOD_BIAS, lodBiasRestoreValue);
      else
        GL.glTextureParameterfvEXT(texture.name, texDetails.curType, eGL_TEXTURE_LOD_BIAS,
                                   &lodBiasRestoreValue);
    }

    rs.ApplyState(m_pDriver);

    return true;
  }

  virtual bool QueueCalculateMathOp(rdcspv::GLSLstd450 op,
                                    const rdcarray<ShaderVariable> &params) override
  {
    CHECK_DEVICE_THREAD();
    RDCASSERT(params.size() <= 3, params.size());

    RDCASSERTEQUAL(params[0].type, VarType::Float);

    GLMarkerRegion markerRegion("QueueCalculateMathOp");

    m_pDriver->GetReplay()->UseReplayContext();

    GLRenderState rs;
    rs.FetchState(m_pDriver);

    RDCCOMPILE_ASSERT(SPV_OpSin == (int)rdcspv::GLSLstd450::Sin, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpCos == (int)rdcspv::GLSLstd450::Cos, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpTan == (int)rdcspv::GLSLstd450::Tan, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAsin == (int)rdcspv::GLSLstd450::Asin, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAcos == (int)rdcspv::GLSLstd450::Acos, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAtan == (int)rdcspv::GLSLstd450::Atan, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpSinh == (int)rdcspv::GLSLstd450::Sinh, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpCosh == (int)rdcspv::GLSLstd450::Cosh, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpTanh == (int)rdcspv::GLSLstd450::Tanh, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAsinh == (int)rdcspv::GLSLstd450::Asinh,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAcosh == (int)rdcspv::GLSLstd450::Acosh,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAtanh == (int)rdcspv::GLSLstd450::Atanh,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpExp == (int)rdcspv::GLSLstd450::Exp, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpLog == (int)rdcspv::GLSLstd450::Log, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpExp2 == (int)rdcspv::GLSLstd450::Exp2, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpLog2 == (int)rdcspv::GLSLstd450::Log2, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpSqrt == (int)rdcspv::GLSLstd450::Sqrt, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpInverseSqrt == (int)rdcspv::GLSLstd450::InverseSqrt,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpNormalize == (int)rdcspv::GLSLstd450::Normalize,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpAtan2 == (int)rdcspv::GLSLstd450::Atan2,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpPow == (int)rdcspv::GLSLstd450::Pow, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpFma == (int)rdcspv::GLSLstd450::Fma, "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpLength == (int)rdcspv::GLSLstd450::Length,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpDistance == (int)rdcspv::GLSLstd450::Distance,
                      "Shader defines are mismatched");
    RDCCOMPILE_ASSERT(SPV_OpRefract == (int)rdcspv::GLSLstd450::Refract,
                      "Shader defines are mismatched");

    GLuint mathProg = m_pDriver->GetReplay()->GetShaderDebugMathProg();

    GL.glUniform1i(GL.glGetUniformLocation(mathProg, "outputs"), 0);

    if(m_MathOffset >= m_MathBufferSize || m_MathBuffer == 0)
    {
      m_MathBufferSize = m_MathBufferSize * 2 + 1024 * mathOpResultByteSize;

      GLuint oldBuf = m_MathBuffer;
      GLsizeiptr oldSize = m_MathBufferSize;

      // resize the buffer up
      GL.glGenBuffers(1, &m_MathBuffer);
      GL.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, m_MathBuffer);
      GL.glNamedBufferDataEXT(m_MathBuffer, m_MathBufferSize, NULL, eGL_DYNAMIC_DRAW);

      if(oldBuf)
        GL.glNamedCopyBufferSubDataEXT(oldBuf, m_MathBuffer, 0, 0, oldSize);
    }

    GL.glBindBufferRange(eGL_SHADER_STORAGE_BUFFER, 0, m_MathBuffer, (GLintptr)m_MathOffset,
                         (GLsizeiptr)mathOpResultByteSize);

    m_MathOffset += mathOpResultByteSize;

    GL.glUseProgram(mathProg);

    const char *names[] = {"a", "b", "c"};

    // push the parameters
    for(size_t i = 0; i < params.size(); i++)
    {
      RDCASSERTEQUAL(params[i].type, params[0].type);
      GL.glUniform4fv(GL.glGetUniformLocation(mathProg, names[i]), 1, params[i].value.f32v.data());
    }

    // push the operation afterwards
    GL.glUniform1i(GL.glGetUniformLocation(mathProg, "op"), (int32_t)op);

    GL.glDispatchCompute(1, 1, 1);

    rs.ApplyState(m_pDriver);

    return true;
  }

  virtual bool GetQueuedResults(rdcarray<ShaderVariable *> &mathOpResults,
                                rdcarray<ShaderVariable *> &sampleGatherResults) override
  {
    CHECK_DEVICE_THREAD();

    bytebuf gpuResults;
    gpuResults.resize(m_MathBufferSize + m_SampleBufferSize);
    if(m_MathBuffer)
    {
      GL.glBindBuffer(eGL_COPY_READ_BUFFER, m_MathBuffer);
      GL.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, m_MathBufferSize, gpuResults.data());
    }
    if(m_SampleBuffer)
    {
      GL.glBindBuffer(eGL_COPY_READ_BUFFER, m_SampleBuffer);
      GL.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, m_SampleBufferSize,
                            gpuResults.data() + m_MathBufferSize);
    }

    m_MathOffset = m_SampleOffset = 0;

    uintptr_t bufferEnd = (uintptr_t)gpuResults.end();

    byte *gpuMathOpResults = gpuResults.data();
    for(ShaderVariable *result : mathOpResults)
    {
      size_t countBytes = VarTypeByteSize(result->type) * result->columns;
      RDCASSERT((uintptr_t)gpuMathOpResults + countBytes <= bufferEnd, (uintptr_t)gpuMathOpResults,
                countBytes, bufferEnd);
      RDCASSERT(countBytes <= mathOpResultByteSize, countBytes, mathOpResultByteSize);
      memcpy(result->value.u32v.data(), gpuMathOpResults, countBytes);
      gpuMathOpResults += mathOpResultByteSize;
    }

    byte *gpuSampleGatherOpResults = gpuResults.data() + m_MathBufferSize;
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

    return true;
  }

  GLuint m_UBO = 0;
  GLuint m_ReadbackTex = 0;
  GLuint m_ReadbackFBO = 0;

  GLuint m_MathBuffer = 0;
  size_t m_MathBufferSize = 0;
  GLuint m_SampleBuffer = 0;
  size_t m_SampleBufferSize = 0;

  size_t m_MathOffset = 0, m_SampleOffset = 0;

  const size_t mathOpResultByteSize = sizeof(Vec4f) * 2;
  const size_t sampleGatherOpResultByteSize = sizeof(Vec4f);

  virtual bool QueuedOpsHasSpace() override { return true; }

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
  WrappedOpenGL *m_pDriver = NULL;

  bool m_ResourcesDirty = false;
  uint32_t m_EventID;
  ResourceId m_ShaderID;

  std::map<int, int> m_UniformLocationRemap;

  rdcarray<DescriptorAccess> m_Access;
  rdcarray<Descriptor> m_Descriptors;
  rdcarray<SamplerDescriptor> m_SamplerDescriptors;

  Threading::RWLock bufferCacheLock;
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

  Threading::RWLock imageCacheLock;
  std::map<ShaderBindIndex, ImageData> imageCache;

  void GetUniformMapping(GLint prog, int &location, const rdcstr &prefix, const ShaderConstant &c)
  {
    rdcstr name = prefix + c.name;

    if(c.type.members.empty())
    {
      if(c.type.elements > 1 || (c.type.flags & ShaderVariableFlags::SingleElementArray))
      {
        for(uint32_t e = 0; e < c.type.elements; e++)
        {
          rdcstr elemName = StringFormat::Fmt("%s[%u]", name.c_str(), e);
          GLint loc = GL.glGetUniformLocation(prog, elemName.c_str());

          m_UniformLocationRemap[location] = loc;

          location++;
        }
      }
      else
      {
        GLint loc = GL.glGetUniformLocation(prog, name.c_str());

        m_UniformLocationRemap[location] = loc;

        location++;
      }
    }
    else
    {
      rdcstr prefixName = name;
      for(uint32_t e = 0; e < c.type.elements; e++)
      {
        prefixName = name;
        if(c.type.elements > 1 || (c.type.flags & ShaderVariableFlags::SingleElementArray))
          prefixName += StringFormat::Fmt("[%u]", e);
        if(!c.type.members[0].name.empty() && c.type.members[0].name[0] != '[')
          prefixName += ".";
        for(const ShaderConstant &mem : c.type.members)
          GetUniformMapping(prog, location, prefixName, mem);
      }
    }
  }

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
  bool IsBufferCached(const ShaderBindIndex &bind) override
  {
    SCOPED_READLOCK(bufferCacheLock);
    return bufferCache.find(bind) != bufferCache.end();
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
        GLMarkerRegion region("un-dirtying resources");
        m_pDriver->GetReplay()->ReplayLog(m_EventID, eReplay_WithoutDraw);
        m_ResourcesDirty = false;
      }

      if(bufData.resource != ResourceId())
      {
        m_pDriver->GetReplay()->GetBufferData(bufData.resource, bufData.byteOffset,
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
        GLMarkerRegion region("un-dirtying resources");
        m_pDriver->GetReplay()->ReplayLog(m_EventID, eReplay_WithoutDraw);
        m_ResourcesDirty = false;
      }

      if(imgData.type == DescriptorType::TypedBuffer ||
         imgData.type == DescriptorType::ReadWriteTypedBuffer)
      {
        ResourceId buffer = imgData.resource;
        uint64_t offset = imgData.byteOffset;
        GLenum format = MakeGLFormat(imgData.format);
        uint64_t byteWidth = imgData.byteSize;

        data.fmt = imgData.format;
        data.texelSize = (uint32_t)GetByteSize(1, 1, 1, GetBaseFormat(format), GetDataType(format));

        // convert to a texel width, rounding down as per spec
        data.width = uint32_t(byteWidth / data.texelSize);
        data.height = 1;
        data.depth = 1;

        data.samplePitch = data.slicePitch = data.rowPitch = data.width * data.texelSize;

        m_pDriver->GetReplay()->GetBufferData(imgData.resource, offset, data.rowPitch, data.bytes);
      }
      else if(imgData.resource != ResourceId())
      {
        ResourceId id = imgData.resource;
        const WrappedOpenGL::TextureData &texProps = m_pDriver->m_Textures[id];

        uint32_t mip = imgData.firstMip;

        data.width = RDCMAX(1, texProps.width >> mip);
        data.height = RDCMAX(1, texProps.height >> mip);
        if(texProps.curType == eGL_TEXTURE_3D)
        {
          data.depth = RDCMAX(1, texProps.depth >> mip);
        }
        else
        {
          data.depth = texProps.depth;
        }

        GLenum format = MakeGLFormat(imgData.format);

        data.fmt = imgData.format;
        data.texelSize = (uint32_t)GetByteSize(1, 1, 1, GetBaseFormat(format), GetDataType(format));
        data.rowPitch =
            (uint32_t)GetByteSize(data.width, 1, 1, GetBaseFormat(format), GetDataType(format));
        data.slicePitch =
            GetByteSize(data.width, data.height, 1, GetBaseFormat(format), GetDataType(format));
        data.samplePitch = GetByteSize(data.width, data.height, data.depth, GetBaseFormat(format),
                                       GetDataType(format));

        const uint32_t numSlices = texProps.curType == eGL_TEXTURE_3D ? 1 : data.depth;
        const uint32_t numSamples = (uint32_t)RDCMAX(1, texProps.samples);

        data.bytes.reserve(size_t(data.samplePitch * numSamples));

        // defaults are fine - no interpretation. Maybe we could use the view's typecast?
        const GetTextureDataParams params = GetTextureDataParams();

        for(uint32_t sample = 0; sample < numSamples; sample++)
        {
          for(uint32_t slice = 0; slice < numSlices; slice++)
          {
            bytebuf subBytes;
            m_pDriver->GetReplay()->GetTextureData(id, Subresource(mip, slice, sample), params,
                                                   subBytes);

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

  const uint64_t deviceThreadID;
};

enum class SubgroupSupport : uint32_t
{
  None = 0,
  Basic,
  Ballot,
  Vote,
  Quad,
};

BITMASK_OPERATORS(SubgroupSupport);

static const uint32_t validMagicNumber = 12345;
static const uint32_t maxHits = 100;    // maximum number of overdraw levels

struct InputFetcherConfig
{
  uint32_t x = 0, y = 0;

  uint32_t vert = 0, inst = 0;

  rdcfixedarray<uint32_t, 3> threadid = {0, 0, 0};
  rdcfixedarray<uint32_t, 3> groupid = {0, 0, 0};

  bool usePrimitiveID = false;
  bool useSampleID = false;
};

static void sanitiseVarName(rdcstr &name)
{
  for(char &c : name)
  {
    if(isalnum(c))
      continue;

    c = '_';
  }
}

static bool DeclareSignatureElement(const ShaderReflection *refl, size_t i, rdcstr &name,
                                    rdcstr &sigDecl, rdcstr &storeDecl)
{
  const SigParameter &sig = refl->inputSignature[i];

  if(name.empty())
    name = sig.varName;

  // sanitise name
  sanitiseVarName(name);

  if(name.beginsWith("gl_"))
    name.insert(0, '_');

  char prefix = (sig.varType == VarType::Float)  ? ' '
                : (sig.varType == VarType::UInt) ? 'u'
                : (sig.varType == VarType::SInt) ? 'i'
                                                 : 'x';
  if(sig.compCount == 1)
  {
    sigDecl += ToStr(sig.varType);
  }
  else
  {
    sigDecl += StringFormat::Fmt("%cvec%u", prefix, sig.compCount);
  }

  if(sig.varName.contains("[0]"))
  {
    // handle arrays
    storeDecl = sigDecl + " " + name + ";";

    size_t nonArrayLength = sig.varName.indexOf('[');
    rdcstr basename = name.substr(0, nonArrayLength);

    uint32_t arraySize = 1;

    for(size_t j = i + 1; j < refl->inputSignature.size(); j++)
    {
      if(refl->inputSignature[j].varName[nonArrayLength] == '[' &&
         refl->inputSignature[j].varName.beginsWith(basename))
      {
        // account for potential holes, take array size from this signature's index
        arraySize = 0;
        const char *c = &refl->inputSignature[j].varName[nonArrayLength + 1];
        while(*c >= '0' && *c <= '9')
        {
          arraySize *= 10;
          arraySize += int((*c) - '0');
          c++;
        }
        arraySize++;
        continue;
      }

      break;
    }

    sigDecl += StringFormat::Fmt(" %s[%u];", basename.c_str(), arraySize);
  }
  else if(sig.varName.contains('['))
  {
    storeDecl = sigDecl + " " + name + ";";
    sigDecl = "";
    return true;
  }
  else if(sig.varName.contains(":col"))
  {
    size_t nonColLength = sig.varName.find(":col");
    name = name.substr(0, nonColLength);

    // only return a declaration for the first column
    if(sig.varName.contains(":col0"))
    {
      uint32_t numCols = 1;

      for(size_t j = i + 1; j < refl->inputSignature.size(); j++)
      {
        if(refl->inputSignature[j].varName[nonColLength] == ':' &&
           refl->inputSignature[j].varName.beginsWith(name))
        {
          numCols++;
          continue;
        }

        break;
      }

      storeDecl = sigDecl =
          StringFormat::Fmt("%cmat%ux%u %s;", prefix, numCols, sig.compCount, name.c_str());
      return true;
    }

    storeDecl.clear();
    sigDecl.clear();
    return false;
  }
  else
  {
    sigDecl += " " + name + ";";
    storeDecl = sigDecl;
  }

  return true;
}

static GLuint CreateInputFetcher(const WrappedOpenGL::ShaderData &shadDetails,
                                 uint32_t storageBufferBinding, InputFetcherConfig cfg,
                                 SubgroupSupport subgroupSupport, uint32_t paramAlign,
                                 uint32_t numThreads)
{
  rdcstr source;

  ShaderType shaderType;
  int glslVersion;
  int glslBaseVer;
  int glslCSVer;    // compute shader
  GetGLSLVersions(shaderType, glslVersion, glslBaseVer, glslCSVer);

  // require at least version 400 since that's what SSBOs need but use the newest version available for safety
  if(shaderType == ShaderType::GLSL)
  {
    glslVersion = glslBaseVer = 400;

    if(GLCoreVersion >= 41)
      glslVersion = glslBaseVer = 410;

    if(GLCoreVersion >= 42)
      glslVersion = glslBaseVer = 420;

    if(GLCoreVersion >= 43)
      glslVersion = glslBaseVer = 430;

    // if we want to use GL_ARB_ES3_1_compatibility for gl_HelperInvocation we need minimum 440
    if(HasExt[ARB_ES3_1_compatibility] || GLCoreVersion >= 44)
      glslVersion = glslBaseVer = 440;

    // when compiling to SPIR-V might as well use a modern version
    if(!shadDetails.spirvWords.empty() || GLCoreVersion >= 45)
      glslVersion = glslBaseVer = 450;

    if(GLCoreVersion >= 46)
      glslVersion = glslBaseVer = 460;
  }

  if(shadDetails.spirvWords.empty())
    source += "#define USE_SPIRV 0\n";
  else
    source += "#define USE_SPIRV 1\n";

  source += StringFormat::Fmt(
      "#define VALID_MAGIC %uu\n"
      "#define STAGE_VS %u\n"
      "#define STAGE_PS %u\n"
      "#define STAGE_CS %u\n"
      "#define STAGE %u\n"
      "#define MAXHIT %uu\n"
      "#define STORAGE_BINDING %u\n"
      "#define NUMLANES %u\n"
      "#define USEPRIM %u\n"
      "#define USESAMP %u\n"
      "#define SUBGROUP_BASIC %u\n"
      "#define SUBGROUP_BALLOT %u\n"
      "#define SUBGROUP_VOTE %u\n"
      "#define SUBGROUP_QUAD %u\n"
      "#define HELPER %u\n"
      "#define PROPER_DERIVS %u\n",
      validMagicNumber, eGL_VERTEX_SHADER, eGL_FRAGMENT_SHADER, eGL_COMPUTE_SHADER,
      shadDetails.type, maxHits, storageBufferBinding, numThreads, cfg.usePrimitiveID ? 1 : 0,
      cfg.useSampleID ? 1 : 0, (subgroupSupport & SubgroupSupport::Basic) ? 1 : 0,
      (subgroupSupport & SubgroupSupport::Ballot) ? 1 : 0,
      (subgroupSupport & SubgroupSupport::Vote) ? 1 : 0,
      (subgroupSupport & SubgroupSupport::Quad) ? 1 : 0,
      // helpers (gl_HelperInvocation)
      HasExt[ARB_ES3_1_compatibility] ? 1 : 0,
      // fine derivatives (dFdxFine)
      HasExt[ARB_derivative_control] ? 1 : 0);

  if(shadDetails.type == eGL_VERTEX_SHADER)
  {
    source += StringFormat::Fmt(
        "#define DEST_VERT %u\n"
        "#define DEST_INST %u\n",
        cfg.vert, cfg.inst);
  }
  else if(shadDetails.type == eGL_FRAGMENT_SHADER)
  {
    source += StringFormat::Fmt(
        "#define DESTX %u.5\n"
        "#define DESTY %u.5\n",
        cfg.x, cfg.y);
  }
  else if(shadDetails.type == eGL_COMPUTE_SHADER)
  {
    source += StringFormat::Fmt(
        "#define DESTX %u\n"
        "#define DESTY %u\n"
        "#define DESTZ %u\n",
        cfg.threadid[0], cfg.threadid[1], cfg.threadid[2]);
    source += StringFormat::Fmt(
        "#define GROUPX %u\n"
        "#define GROUPY %u\n"
        "#define GROUPZ %u\n",
        cfg.groupid[0], cfg.groupid[1], cfg.groupid[2]);
    source += StringFormat::Fmt("layout(local_size_x = %u,local_size_y = %u, local_size_z = %u)\n",
                                shadDetails.GetReflection()->dispatchThreadsDimension[0],
                                shadDetails.GetReflection()->dispatchThreadsDimension[1],
                                shadDetails.GetReflection()->dispatchThreadsDimension[2]);
  }
  else
  {
    RDCERR("Unexpected type of shader");
  }

  source += R"EOSHADER(

#ifdef OPENGL_ES
precision highp float;
#endif

#ifdef OPENGL_CORE
  #extension GL_ARB_shader_storage_buffer_object : require
#endif

#if PROPER_DERIVS
  #extension GL_ARB_derivative_control : require
#endif

#ifdef OPENGL_CORE
#if HELPER && !USE_SPIRV
  // required for gl_HelperInvocation, but don't enable with glslang due to a bug -
  // we compile at a high enough core version to satisfy the requirement that way
  #extension GL_ARB_ES3_1_compatibility : require
#endif
#endif

#if SUBGROUP_BASIC
  #extension GL_KHR_shader_subgroup_basic : require
#endif

#if SUBGROUP_VOTE
  #extension GL_KHR_shader_subgroup_vote : require
#endif

#if SUBGROUP_BALLOT
  #extension GL_KHR_shader_subgroup_ballot : require
#endif

#if SUBGROUP_QUAD
  #extension GL_KHR_shader_subgroup_quad : require
#endif

// bool signature elements get reflected as ints, make macros for their access to cast to int
#define gl_FrontFacing (gl_FrontFacing ? 1u : 0u)
#define gl_HelperInvocation (gl_HelperInvocation ? 1u : 0u)

)EOSHADER";

  rdcarray<rdcpair<rdcstr, rdcstr>> floatInputs;
  rdcarray<rdcpair<rdcstr, rdcstr>> nonfloatInputs;
  if(shadDetails.type == eGL_COMPUTE_SHADER)
  {
    glslVersion = glslCSVer;

    // can't have an empty struct in GLSL
    source = R"(
struct Inputs { vec4 dummy; };
void SetInputs(out Inputs inputs) {}
)";
  }
  else
  {
    rdcstr inputDecl, inputFetch;

    inputDecl += "struct Inputs {\n";
    inputFetch += "void SetInputs(out Inputs inputs) {\n";

    // use the converted SPIR-V compiled reflection as it's more likely to have accurate data -
    // driver reflection can omit inputs even if they're declared and needed for matching
    const ShaderReflection *refl =
        shadDetails.convertedSPIRV ? &shadDetails.convertedRefl : shadDetails.GetReflection();
    const SPIRVPatchData &patchData =
        shadDetails.convertedSPIRV ? shadDetails.convertedPatchData : shadDetails.patchData;

    rdcarray<rdcpair<rdcstr, size_t>> blockVarsToDeclare;

    for(size_t i = 0; i < refl->inputSignature.size(); i++)
    {
      const SigParameter &sig = refl->inputSignature[i];

      rdcstr name;
      rdcstr sigDecl;
      rdcstr storeDecl;
      bool doDeclare = DeclareSignatureElement(refl, i, name, sigDecl, storeDecl);

      if(sig.varName.contains(":col"))
      {
        rdcstr inName = sig.varName;
        inName.replace(inName.find(":col"), 4, "[");
        inName += "]";

        if(sig.varType == VarType::Float)
          floatInputs.push_back({name + inName.substr(inName.size() - 3), inName});
        else
          nonfloatInputs.push_back({name + inName.substr(inName.size() - 3), inName});
      }
      else
      {
        rdcstr inName = sig.varName;

        if(sig.varType == VarType::Float)
          floatInputs.push_back({name, inName});
        else
          nonfloatInputs.push_back({name, inName});
      }

      if(!doDeclare)
        continue;
      inputDecl += "  " + storeDecl + "\n";

      const uint32_t byteSize = sig.compCount * VarTypeByteSize(sig.varType);

      // don't pad after matrices, these don't have padding that can be explicitly filled
      if(!sig.varName.contains(":col"))
      {
        for(size_t pad = byteSize; pad < AlignUp(byteSize, paramAlign); pad += 4)
        {
          inputDecl += StringFormat::Fmt("  uint pad%u%u;\n", i, pad);
        }
      }

      // don't declare builtins!
      if(sig.systemValue == ShaderBuiltin::Undefined && !sigDecl.empty())
      {
        // block variables with a . like blockName.variable need to be declared specially
        if(sig.varName.contains('.'))
        {
          blockVarsToDeclare.push_back({sig.varName, i});
        }
        else
        {
          // if the register index is high, it was auto-mapped so don't declare it
          if(sig.regIndex < 256)
            source += StringFormat::Fmt("layout(location = %u) ", sig.regIndex);
          if(sig.varType != VarType::Float && shadDetails.type == eGL_FRAGMENT_SHADER)
            source += "flat ";
          else if(shadDetails.type == eGL_FRAGMENT_SHADER)
          {
            SPIRVInterpolationMode interpMode = patchData.inputs[i].interpMode;
            if(interpMode != SPIRVInterpolationMode::Smooth)
              source += ToStr(interpMode) + " ";
          }
          source += StringFormat::Fmt("in %s\n", sigDecl.c_str());
        }
      }

      rdcstr inputName = sig.varName;

      // copy whole matrices, if we get the :col0 element
      int trim = inputName.find(":col");
      if(trim >= 0)
        inputName.erase(trim, ~0U);

      switch(sig.systemValue)
      {
        case ShaderBuiltin::BaseInstance: inputName = "gl_BaseInstance"; break;
        case ShaderBuiltin::BaseVertex: inputName = "gl_BaseVertex"; break;
        case ShaderBuiltin::DispatchSize: inputName = "gl_NumWorkGroups"; break;
        case ShaderBuiltin::DispatchThreadIndex: inputName = "gl_GlobalInvocationID"; break;
        case ShaderBuiltin::DomainLocation: inputName = "gl_TessCoord"; break;
        case ShaderBuiltin::DrawIndex: inputName = "gl_DrawID"; break;
        case ShaderBuiltin::GroupFlatIndex: inputName = "gl_LocalInvocationIndex"; break;
        case ShaderBuiltin::GroupIndex: inputName = "gl_WorkGroupID"; break;
        case ShaderBuiltin::GroupThreadIndex: inputName = "gl_LocalInvocationID"; break;
        case ShaderBuiltin::GSInstanceIndex: inputName = "gl_InvocationID"; break;
        case ShaderBuiltin::InstanceIndex: inputName = "gl_InstanceID"; break;
        case ShaderBuiltin::IsFrontFace: inputName = "gl_FrontFacing"; break;
        case ShaderBuiltin::MSAACoverage: inputName = "gl_SampleMaskIn"; break;
        case ShaderBuiltin::MSAASampleIndex: inputName = "gl_SampleID"; break;
        case ShaderBuiltin::MSAASamplePosition: inputName = "gl_SamplePosition"; break;
        case ShaderBuiltin::OutputControlPointIndex: inputName = "gl_InvocationID"; break;
        case ShaderBuiltin::PatchNumVertices: inputName = "gl_PatchVerticesIn"; break;
        case ShaderBuiltin::Position: inputName = "gl_FragCoord"; break;
        case ShaderBuiltin::PrimitiveIndex: inputName = "gl_PrimitiveID"; break;
        case ShaderBuiltin::RTIndex:
          if(shadDetails.type == eGL_FRAGMENT_SHADER)
            inputName = "gl_PointCoord";
          else
            inputName = "gl_Layer";
          break;
        case ShaderBuiltin::VertexIndex: inputName = "gl_VertexID"; break;
        case ShaderBuiltin::ViewportIndex: inputName = "gl_ViewportIndex"; break;
        default: break;
      }

      // works for arrays with modified name per element
      inputFetch += StringFormat::Fmt("inputs.%s = %s;\n", name.c_str(), inputName.c_str());
    }

    if(refl->inputSignature.empty())
      inputDecl += "  vec4 dummy;\n";

    inputDecl += "};\n";
    inputFetch += "}\n";

    while(!blockVarsToDeclare.empty())
    {
      rdcpair<rdcstr, size_t> var = blockVarsToDeclare.takeAt(0);
      rdcstr base = var.first.substr(0, var.first.indexOf('.'));

      rdcarray<rdcpair<rdcstr, size_t>> siblings;

      siblings.push_back(var);

      // collect all other vars with the same base. We don't really care about doing this
      // efficiently since we expect extremely few variables and all in the same block
      for(size_t j = 0; j < blockVarsToDeclare.size();)
      {
        rdcstr jbase =
            blockVarsToDeclare[j].first.substr(0, blockVarsToDeclare[j].first.indexOf('.'));
        if(base == jbase)
        {
          siblings.push_back(blockVarsToDeclare.takeAt(j));
          // continue with the new [j], if it exists
          continue;
        }
        j++;
      }

      // need to get the block name and it's not available via normal GL reflection
      rdcstr blockName = shadDetails.spirv.GetDataType(patchData.inputs[var.second].structID).name;

      // if the register index is high, it was auto-mapped so don't declare it
      // assume locations are tightly packed with the first location, it's all we can do.
      if(refl->inputSignature[var.second].regIndex < 256)
        source +=
            StringFormat::Fmt("layout(location = %u) ", refl->inputSignature[var.second].regIndex);
      source += StringFormat::Fmt("in %s {\n", blockName.c_str());

      rdcstr name;
      for(const rdcpair<rdcstr, size_t> &sig : siblings)
      {
        name = sig.first.substr(base.size() + 1);
        source += "  ";

        if(refl->inputSignature[sig.second].varType != VarType::Float &&
           shadDetails.type == eGL_FRAGMENT_SHADER)
          source += "flat ";

        rdcstr sigDecl, storeDecl;
        if(DeclareSignatureElement(refl, sig.second, name, sigDecl, storeDecl))
          source += sigDecl + "\n";
      }

      source += StringFormat::Fmt("} %s;\n", base.c_str());
    }

    source += inputDecl + inputFetch;
  }

  source += R"EOSHADER(
#if STAGE == STAGE_VS
struct VSLaneData
{
  int inst;
  int vert;
  uint view;
  uint pad;
};
#endif

#if STAGE == STAGE_PS
struct PSLaneData
{
  vec4 fragCoord;

  uint isHelper;
  uint quadId;
  uint quadLane;
  uint pad;
};
#endif

#if STAGE == STAGE_CS
struct CSLaneData
{
  uvec3 threadid;
  uint activeSubgroup;
};
#endif

struct SubgroupLaneData
{
  uint elect;
  uint rd_active;
  uint pad;
  uint pad2;
};

struct LaneData
{
#if SUBGROUP_BASIC
  SubgroupLaneData sub;
#endif

#if STAGE == STAGE_VS
  VSLaneData vs;
#elif STAGE == STAGE_PS
  PSLaneData ps;
#else
  CSLaneData cs;
#endif

  Inputs inputs;
};

struct ResultData
{
  vec4 pos;

  int prim;
  int rd_sample;
  uint view;
  uint valid;

  float ddxDerivCheck;
  uint quadLaneIndex;
  uint laneIndex;
  uint subgroupSize;

  uvec4 globalBallot;
  uvec4 electBallot;
  uvec4 helperBallot;

  uint numSubgroups;
  // split out because we use std140 packing which won't pack {uint, uvec3}
  uint pad1;
  uint pad2;
  uint pad3;

  LaneData laneData[NUMLANES];
};

#if USE_SPIRV || defined(OPENGL_ES)
layout(binding = STORAGE_BINDING)
#endif
layout(std140) buffer Output
{
  uint hit_count;
  uint total_count;
  uvec2 pad;
  
  ResultData hits[];
} outbuffer;

#if STAGE == STAGE_PS

#if !SUBGROUP_QUAD

// a couple of define helpers to get the hlsl to compile :)

#if PROPER_DERIVS
#define ddx_fine dFdxFine
#define ddy_fine dFdyFine
#else
#define ddx_fine dFdx
#define ddy_fine dFdy
#endif

#define float4 vec4
#define float3 vec3
#define float2 vec2
#define uint4 uvec4
#define uint3 uvec3
#define uint2 uvec2
#define int4 ivec4
#define int3 ivec3
#define int2 ivec2
#include "quadswizzle.hlsl"

#else

#define quadSwizzleHelper(value, quadLaneIndex, readIndex) subgroupQuadBroadcast(value, readIndex)

#endif

#endif

void main()
{
  vec4 debug_pixelPos = vec4(0,0,0,0);
  int primitive = 0;
  int rd_sample = 0;
  uint isFrontFace = 0u;

#if STAGE == STAGE_VS
  int vert = gl_VertexID;
  int inst = gl_InstanceID;
#elif STAGE == STAGE_PS
  debug_pixelPos = gl_FragCoord;

#if USEPRIM 
  primitive = gl_PrimitiveID;
#endif

#if USESAMP
  rd_sample = gl_SampleID;
#endif

  isFrontFace = gl_FrontFacing;

#endif

#if STAGE == STAGE_VS
  VSLaneData vs;
  vs.pad = 0u;
#elif STAGE == STAGE_PS
  PSLaneData ps;
  ps.pad = 0u;
#else
  CSLaneData cs;
#endif

#if SUBGROUP_BASIC
  SubgroupLaneData sub;
  sub.elect = subgroupElect() ? 1u : 0u;
  sub.rd_active = 1u;
#endif

  uint isHelper = 0u;
  uint quadLaneIndex = 0u;
  uint quadId = 0u;
  uint laneIndex = 0u;
  uvec4 globalBallot = uvec4(0u,0u,0u,0u);
  uvec4 electBallot = uvec4(0u,0u,0u,0u);
  uvec4 helperBallot = uvec4(0u,0u,0u,0u);
  float derivValid = 1.0f;

  quadLaneIndex = (2u * (uint(debug_pixelPos.y) & 1u)) + (uint(debug_pixelPos.x) & 1u);

#if SUBGROUP_BALLOT
  globalBallot = subgroupBallot(true);
  electBallot = subgroupBallot(subgroupElect());
#endif

#if SUBGROUP_BASIC
  laneIndex = gl_SubgroupInvocationID;
#endif

#if STAGE == STAGE_VS
  bool candidateThread = (vert == DEST_VERT && inst == DEST_INST);

  vs.vert = vert;
  vs.inst = inst;
#elif STAGE == STAGE_PS
  bool candidateThread = (abs(debug_pixelPos.x - DESTX) < 0.5f && abs(debug_pixelPos.y - DESTY) < 0.5f);

  derivValid = dFdx(debug_pixelPos.x);
#if HELPER
  isHelper = gl_HelperInvocation;
#else
  // must just assume all non-candidate threads are helpers since helpers can't store their
  // own data so the candidate must do it.
  isHelper = candidateThread ? 0 : 1;
#endif

#if !SUBGROUP_BASIC
  laneIndex = quadLaneIndex;
#endif

#if SUBGROUP_BALLOT
  helperBallot = subgroupBallot(isHelper != 0);
#endif

  // quadId is a single value that's unique for this quad and uniform across the quad. Degenerate
  // for the simple quad case
  quadId = 1000u+quadSwizzleHelper(laneIndex, quadLaneIndex, 0u);

  LaneData helper0data;
  LaneData helper1data;
  LaneData helper2data;
  LaneData helper3data;

  uint helper0lane;
  uint helper1lane;
  uint helper2lane;
  uint helper3lane;

)EOSHADER";

  for(uint32_t q = 0; q < 4; q++)
  {
    source += StringFormat::Fmt("  // quad %u\n", q);
    source += "  {\n";
    source += StringFormat::Fmt(
        "    helper%ulane = quadSwizzleHelper(laneIndex, quadLaneIndex, %uu);\n", q, q);
    source += StringFormat::Fmt(
        "    helper%udata.ps.fragCoord = quadSwizzleHelper(debug_pixelPos, quadLaneIndex, %uu);\n",
        q, q);
    source += StringFormat::Fmt(
        "    helper%udata.ps.isHelper = quadSwizzleHelper(isHelper, quadLaneIndex, %uu);\n", q, q);
    source += StringFormat::Fmt("    helper%udata.ps.quadId = quadId;\n", q);
    source += StringFormat::Fmt("    helper%udata.ps.quadLane = %uu;\n", q, q);
    for(size_t i = 0; i < floatInputs.size(); i++)
    {
      source += StringFormat::Fmt(
          "    helper%udata.inputs.%s = quadSwizzleHelper(%s, quadLaneIndex, %uu);\n", q,
          floatInputs[i].first.c_str(), floatInputs[i].second.c_str(), q);
    }
    if(!nonfloatInputs.empty())
    {
      source += "#if SUBGROUP_QUAD\n";
      for(size_t i = 0; i < nonfloatInputs.size(); i++)
      {
        source += StringFormat::Fmt(
            "    helper%udata.inputs.%s = quadSwizzleHelper(%s, quadLaneIndex, %uu);\n", q,
            nonfloatInputs[i].first.c_str(), nonfloatInputs[i].second.c_str(), q);
      }
      source += "#else\n";
      for(size_t i = 0; i < nonfloatInputs.size(); i++)
      {
        source +=
            StringFormat::Fmt("    helper%udata.inputs.%s = %s;\n", q,
                              nonfloatInputs[i].first.c_str(), nonfloatInputs[i].second.c_str(), q);
      }
      source += "#endif\n";
    }
    source += "  }\n\n";
  }

  source += R"EOSHADER(

  ps.fragCoord = debug_pixelPos;

  ps.isHelper = isHelper;
  ps.quadId = quadId;
  ps.quadLane = quadLaneIndex;
#elif STAGE == STAGE_CS
  bool candidateThread = (dtid.x == DESTX && dtid.y == DESTY && dtid.z == DESTZ);
  cs.threadid = threadid;
#endif

  ResultData result;

#if SUBGROUP_VOTE
  bool activeSubgroup = subgroupAny(candidateThread);
#else
  bool activeSubgroup = candidateThread;
#endif

#if STAGE == STAGE_CS
  cs.activeSubgroup = activeSubgroup ? 1 : 0;
#endif

  if(activeSubgroup)
  {
    if(isHelper == 0u)
    {
      uint idx = MAXHIT;
#if SUBGROUP_BALLOT
      if(subgroupElect())
      {
        atomicAdd(outbuffer.total_count, 1u);
        idx = atomicAdd(outbuffer.hit_count, 1u);
      }
      idx = subgroupBroadcastFirst(idx);
#else
      atomicAdd(outbuffer.total_count, 1u);
      idx = atomicAdd(outbuffer.hit_count, 1u);
#endif
      if(idx < MAXHIT)
      {
        if(candidateThread)
        {
          outbuffer.hits[idx].pos = debug_pixelPos;
          outbuffer.hits[idx].prim = primitive;
          outbuffer.hits[idx].valid = VALID_MAGIC;
          outbuffer.hits[idx].rd_sample = rd_sample;
          outbuffer.hits[idx].ddxDerivCheck = derivValid;
          outbuffer.hits[idx].laneIndex = laneIndex;
          outbuffer.hits[idx].quadLaneIndex = quadLaneIndex;
#if SUBGROUP_BASIC
          outbuffer.hits[idx].subgroupSize = gl_SubgroupSize;
          outbuffer.hits[idx].numSubgroups = gl_NumSubgroups;
#else
          outbuffer.hits[idx].subgroupSize = 0u;
          outbuffer.hits[idx].numSubgroups = 0u;
#endif
          outbuffer.hits[idx].globalBallot = globalBallot;
          outbuffer.hits[idx].electBallot = electBallot;
          outbuffer.hits[idx].helperBallot = helperBallot;
        }

#if STAGE == STAGE_PS

// with subgroups, only store helpers since the whole subgroup will be in here
#if SUBGROUP_BASIC
        if(helper0data.ps.isHelper != 0u)
          outbuffer.hits[idx].laneData[helper0lane] = helper0data;

        if(helper1data.ps.isHelper != 0u)
          outbuffer.hits[idx].laneData[helper1lane] = helper1data;

        if(helper2data.ps.isHelper != 0u)
          outbuffer.hits[idx].laneData[helper2lane] = helper2data;

        if(helper3data.ps.isHelper != 0u)
          outbuffer.hits[idx].laneData[helper3lane] = helper3data;
#else
        // without subgroups only the candidate thread is in here, so it should store its helpers
        outbuffer.hits[idx].laneData[helper0lane] = helper0data;
        outbuffer.hits[idx].laneData[helper1lane] = helper1data;
        outbuffer.hits[idx].laneData[helper2lane] = helper2data;
        outbuffer.hits[idx].laneData[helper3lane] = helper3data;
#endif

#endif

#if SUBGROUP_BASIC
        outbuffer.hits[idx].laneData[laneIndex].sub = sub;
#endif

#if STAGE == STAGE_VS
        outbuffer.hits[idx].laneData[laneIndex].vs = vs;
#elif STAGE == STAGE_PS
        outbuffer.hits[idx].laneData[laneIndex].ps = ps;
#else
        outbuffer.hits[idx].laneData[laneIndex].cs = cs;
#endif
        SetInputs(outbuffer.hits[idx].laneData[laneIndex].inputs);
      }
    }
  }
}

)EOSHADER";

  if(!OpenGL_Debug_ShaderDebugDumpDirPath().empty())
    FileIO::WriteAll(OpenGL_Debug_ShaderDebugDumpDirPath() + "/input_fetch_raw.glsl", source);

  source = GenerateGLSLShader(source, shaderType, glslVersion);

  if(!OpenGL_Debug_ShaderDebugDumpDirPath().empty())
    FileIO::WriteAll(OpenGL_Debug_ShaderDebugDumpDirPath() + "/input_fetch_pp.glsl", source);

  if(shadDetails.spirvWords.empty())
    return CreateShader(shadDetails.type, source);

  return CreateSPIRVShader(shadDetails.type, source);
}

void CalculateSubgroupProperties(uint32_t &maxSubgroupSize, SubgroupSupport &subgroupSupport)
{
  if(HasExt[KHR_shader_subgroup])
  {
    GL.glGetIntegerv(eGL_SUBGROUP_SIZE_KHR, (GLint *)&maxSubgroupSize);

    GLbitfield features = 0;
    GL.glGetIntegerv(eGL_SUBGROUP_SUPPORTED_FEATURES_KHR, (GLint *)&features);

    if((features & eGL_SUBGROUP_FEATURE_BASIC_BIT_KHR))
      subgroupSupport |= SubgroupSupport::Basic;

    if((features & eGL_SUBGROUP_FEATURE_VOTE_BIT_KHR))
      subgroupSupport |= SubgroupSupport::Vote;

    if((features & eGL_SUBGROUP_FEATURE_BALLOT_BIT_KHR))
      subgroupSupport |= SubgroupSupport::Ballot;

    if((features & eGL_SUBGROUP_FEATURE_QUAD_BIT_KHR))
      subgroupSupport |= SubgroupSupport::Quad;
  }
}

rdcpair<uint32_t, uint32_t> GetAlignAndOutputSize(const ShaderReflection *refl,
                                                  const SPIRVPatchData &patchData)
{
  uint32_t paramAlign = 16;

  for(const SigParameter &sig : refl->inputSignature)
  {
    if(VarTypeByteSize(sig.varType) * sig.compCount > paramAlign)
      paramAlign = 32;
  }

  // conservatively calculate structure stride with full amount for every input element
  uint32_t structStride = (uint32_t)refl->inputSignature.size() * paramAlign;

  // GLSL doesn't allow empty structs, so pad with 16 minimum
  structStride = RDCMAX(paramAlign, structStride);

  if(refl->stage == ShaderStage::Vertex)
    structStride += sizeof(rdcspv::VertexLaneData);
  else if(refl->stage == ShaderStage::Pixel)
    structStride += sizeof(rdcspv::PixelLaneData);
  else if(refl->stage == ShaderStage::Compute || refl->stage == ShaderStage::Task ||
          refl->stage == ShaderStage::Mesh)
    structStride += sizeof(rdcspv::ComputeLaneData);

  if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    structStride += sizeof(rdcspv::SubgroupLaneData);
  }

  return {paramAlign, structStride};
}

uint32_t GetStorageBufferBinding(WrappedOpenGL *driver,
                                 const ResourceId stagePrograms[NumShaderStages],
                                 const ResourceId stageShaders[NumShaderStages], ShaderStage stage)
{
  rdcarray<uint32_t> avail;

  GLint ssboCount = 0;
  GL.glGetIntegerv(eGL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &ssboCount);

  avail.resize(ssboCount);
  for(GLint i = 0; i < ssboCount; i++)
    avail[i] = i;

  for(size_t i = 0; i < NumShaderStages; i++)
  {
    if(stageShaders[i] == ResourceId())
      continue;

    // we can use SSBOs from the stage we're replacing
    if(ShaderStage(i) == stage)
      continue;

    const ShaderReflection *refl = driver->GetShader(stageShaders[i]).GetReflection();

    GLuint prog = driver->GetResourceManager()->GetResource(stagePrograms[i]).name;

    for(const ShaderResource &res : refl->readWriteResources)
    {
      // storage images use separate bindings
      if(res.isTexture)
        continue;

      uint32_t slot = 0;
      bool used = false;
      GetCurrentBinding(prog, refl, res, slot, used);

      if(used)
        avail.removeOne(slot);
    }
  }

  if(avail.empty())
    return ~0U;
  return avail.front();
}

ShaderDebugTrace *GLReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                        uint32_t idx, uint32_t view)
{
  MakeCurrentReplayContext(&m_ReplayCtx);

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  rdcstr regionName =
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u,%u)", eventId, vertid, instid, idx, view);

  GLMarkerRegion region(regionName);

  if(OpenGL_Debug_ShaderDebugLogging())
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

  uint32_t storageBufferBinding = ~0U;

  GLuint prog = 0;
  ResourceId vert;
  if(rs.Program.name)
  {
    ResourceId id = m_pDriver->GetResourceManager()->GetResID(rs.Program);
    const WrappedOpenGL::ProgramData &progDetails = m_pDriver->GetProgram(id);

    prog = rs.Program.name;

    vert = progDetails.stageShaders[(uint32_t)ShaderStage::Vertex];

    ResourceId stagePrograms[NumShaderStages];
    for(size_t i = 0; i < NumShaderStages; i++)
      stagePrograms[i] = id;
    storageBufferBinding = GetStorageBufferBinding(m_pDriver, stagePrograms,
                                                   progDetails.stageShaders, ShaderStage::Vertex);
  }
  else if(rs.Pipeline.name)
  {
    ResourceId id = m_pDriver->GetResourceManager()->GetResID(rs.Pipeline);
    const WrappedOpenGL::PipelineData &pipeDetails = m_pDriver->GetPipeline(id);

    prog = m_pDriver->GetResourceManager()
               ->GetResource(pipeDetails.stagePrograms[(uint32_t)ShaderStage::Vertex])
               .name;

    vert = pipeDetails.stageShaders[(uint32_t)ShaderStage::Vertex];

    storageBufferBinding = GetStorageBufferBinding(m_pDriver, pipeDetails.stagePrograms,
                                                   pipeDetails.stageShaders, ShaderStage::Vertex);
  }

  if(vert == ResourceId())
  {
    RDCLOG("No vertex shader bound at draw!");
    return new ShaderDebugTrace();
  }

  if(storageBufferBinding == ~0U)
  {
    RDCLOG("No spare SSBO available in program");
    return new ShaderDebugTrace();
  }

  WrappedOpenGL::ShaderData &shadDetails = m_pDriver->GetWriteableShader(vert);

  rdcstr entryPoint = shadDetails.entryPoint;
  rdcarray<SpecConstant> spec;
  for(size_t i = 0; i < shadDetails.specIDs.size() && i < shadDetails.specValues.size(); i++)
    spec.push_back(SpecConstant(shadDetails.specIDs[i], shadDetails.specValues[i], 4));

  // use converted reflection unless we had native SPIR-V reflection because the input fetcher will
  // use it as well and we might have extra inputs (that were stripped from the driver's GL reflection)
  const ShaderReflection *refl =
      shadDetails.convertedSPIRV ? &shadDetails.convertedRefl : shadDetails.GetReflection();

  if(!refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadDetails.Disassemble(entryPoint);

  GLAPIWrapper *apiWrapper = new GLAPIWrapper(m_pDriver, ShaderStage::Vertex, eventId, vert,
                                              shadDetails.convertedAutomapped ? refl : NULL);

  SubgroupSupport subgroupSupport = SubgroupSupport::None;
  uint32_t numThreads = 1;

  const SPIRVPatchData &patchData =
      shadDetails.convertedSPIRV ? shadDetails.convertedPatchData : shadDetails.patchData;

  if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    uint32_t maxSubgroupSize = 1;
    CalculateSubgroupProperties(maxSubgroupSize, subgroupSupport);
    numThreads = RDCMAX(numThreads, maxSubgroupSize);
  }

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

  // use the input fetcher for all cases rather than implementing manual fetching.
  InputFetcherConfig cfg;

  if(action->flags & ActionFlags::Indexed)
    cfg.vert = idx;
  else
    cfg.vert = vertid + vertOffset;
  cfg.inst = instid;

  uint32_t paramAlign, structStride;
  rdctie(paramAlign, structStride) = GetAlignAndOutputSize(refl, patchData);

  const uint32_t numHits = 10;    // we should only ever get one hit, but can get more with re-use.

  // struct size is rdcspv::ResultDataBase header plus Nx structStride for the number of threads
  uint32_t structSize = sizeof(rdcspv::ResultDataBase) + structStride * numThreads;

  GLuint feedbackStorageSize = numHits * structSize + 1024;

  if(OpenGL_Debug_ShaderDebugLogging())
  {
    RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
           feedbackStorageSize);
  }

  bytebuf data;
  {
    GLRenderState push;
    push.FetchState(m_pDriver);

    GLuint shaderFeedback;
    GL.glGenBuffers(1, &shaderFeedback);
    GL.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, shaderFeedback);
    GL.glNamedBufferDataEXT(shaderFeedback, feedbackStorageSize, NULL, eGL_DYNAMIC_DRAW);
    byte *clear = (byte *)GL.glMapBufferRange(eGL_SHADER_STORAGE_BUFFER, 0, feedbackStorageSize,
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    memset(clear, 0, feedbackStorageSize);
    GL.glUnmapBuffer(eGL_SHADER_STORAGE_BUFFER);

    GLuint inputFetcher = CreateInputFetcher(shadDetails, storageBufferBinding, cfg,
                                             subgroupSupport, paramAlign, numThreads);

    GLuint replacementProgram = GL.glCreateProgram();

    GLuint fragShader = 0;

    // on GLES we must have a pixel shader, it's not optional.
    if(IsGLES)
    {
      ShaderType shaderType;
      int glslVersion;
      int glslBaseVer;
      int glslCSVer;
      GetGLSLVersions(shaderType, glslVersion, glslBaseVer, glslCSVer);

      rdcstr source =
          GenerateGLSLShader(GetEmbeddedResource(glsl_fixedcol_frag), shaderType, glslVersion);
      fragShader = CreateShader(eGL_FRAGMENT_SHADER, source);
      GL.glAttachShader(replacementProgram, fragShader);
    }

    {
      GL.glAttachShader(replacementProgram, inputFetcher);

      // we do need to copy attributes for non-SPIR_V

      if(shadDetails.spirvWords.empty())
        CopyProgramAttribBindings(prog, replacementProgram, shadDetails.GetReflection());

      GL.glLinkProgram(replacementProgram);

      char buffer[1024] = {};
      GLint status = 0;
      GL.glGetProgramiv(replacementProgram, eGL_LINK_STATUS, &status);
      if(status == 0)
      {
        GL.glGetProgramInfoLog(replacementProgram, 1024, NULL, buffer);
        RDCERR("Link error: %s", buffer);
      }

      GL.glDetachShader(replacementProgram, inputFetcher);
    }

    GL.glUseProgram(replacementProgram);

    if(shadDetails.spirvWords.empty() && !IsGLES)
    {
      GLuint ssboIdx =
          GL.glGetProgramResourceIndex(replacementProgram, eGL_SHADER_STORAGE_BLOCK, "Output");
      GL.glShaderStorageBlockBinding(replacementProgram, ssboIdx, storageBufferBinding);
    }
    GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, storageBufferBinding, shaderFeedback);

    m_pDriver->ReplayLog(eventId, eventId, eReplay_OnlyDraw);

    GL.glDeleteProgram(replacementProgram);
    GL.glDeleteShader(inputFetcher);
    if(fragShader)
      GL.glDeleteShader(fragShader);

    data.resize(feedbackStorageSize);
    GL.glGetBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, feedbackStorageSize, data.data());

    push.ApplyState(m_pDriver);
  }

  byte *base = data.data();
  uint32_t hit_count = ((uint32_t *)base)[0];
  // uint32_t total_count = ((uint32_t *)base)[1];

  // there can be more than one hit here with vertex re-use, but we expect them to be identical so
  // we use the first
  (void)hit_count;
  // RDCASSERTMSG("Should only get one hit for vertex shaders", hit_count == 1, hit_count);

  base += sizeof(Vec4f);

  rdcspv::ResultDataBase *winner = (rdcspv::ResultDataBase *)base;

  if(winner->valid != validMagicNumber)
  {
    RDCWARN("Hit doesn't have valid magic number");

    delete apiWrapper;

    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Vertex;

    return ret;
  }

  if(!OpenGL_Debug_ShaderDebugDumpDirPath().empty())
    FileIO::WriteAll(
        OpenGL_Debug_ShaderDebugDumpDirPath() + "/vertex_debug.spv",
        shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords : shadDetails.spirvWords);

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords
                                             : shadDetails.spirvWords);

  // the per-thread data immediately follows the rdcspv::ResultDataBase header. Every piece of
  // data is uniformly aligned, either 16-byte by default or 32-byte if larger components exist.
  // The output is in input signature order.
  byte *LaneData = (byte *)(winner + 1);

  numThreads = 1;

  if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
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

    if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    {
      rdcspv::SubgroupLaneData *subgroupData = (rdcspv::SubgroupLaneData *)value;
      apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Active] = subgroupData->isActive;
      apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Elected] = subgroupData->elect;
      apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::SubgroupId] = t;

      value += sizeof(rdcspv::SubgroupLaneData);
    }

    // read VertexLaneData
    {
      rdcspv::VertexLaneData *vertData = (rdcspv::VertexLaneData *)value;

      apiWrapper->thread_builtins[t][ShaderBuiltin::InstanceIndex] =
          ShaderVariable("InstanceIndex"_lit, vertData->inst, 0U, 0U, 0U);
      apiWrapper->thread_builtins[t][ShaderBuiltin::VertexIndex] =
          ShaderVariable("VertexIndex"_lit, vertData->vert, 0U, 0U, 0U);
    }
    value += sizeof(rdcspv::VertexLaneData);

    for(size_t i = 0; i < refl->inputSignature.size(); i++)
    {
      const SigParameter &param = refl->inputSignature[i];

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
                                               shadDetails.spirvInstructionLines, patchData,
                                               winner->laneIndex, numThreads, numThreads);
  apiWrapper->ResetReplay();

  return ret;
}

ShaderDebugTrace *GLReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                       const DebugPixelInputs &inputs)
{
  uint32_t sample = inputs.sample;
  uint32_t primitive = inputs.primitive;
  uint32_t view = inputs.view;

  MakeCurrentReplayContext(&m_ReplayCtx);

  // try to get fine derivatives
  if(IsGLES)
    GL.glHint(eGL_FRAGMENT_SHADER_DERIVATIVE_HINT, eGL_NICEST);

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  // When RenderDoc passed y, the value being passed in is with the Y axis starting from the top
  // However, we need to have it starting from the bottom, so flip it by subtracting y from the
  // height.
  {
    ContextPair &ctx = m_pDriver->GetCtx();
    uint32_t height = 0;

    GLint numCols = 8;
    GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

    GLuint obj = 0;
    GLenum type = eGL_TEXTURE;
    for(GLint i = 0; i < numCols; i++)
    {
      GL.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&obj);

      if(obj)
      {
        GL.glGetFramebufferAttachmentParameteriv(
            eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
            eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

        ResourceId id = m_pDriver->GetResourceManager()->GetResID(
            type == eGL_RENDERBUFFER ? RenderbufferRes(ctx, obj) : TextureRes(ctx, obj));

        GLint firstMip = 0, firstSlice = 0;
        GetFramebufferMipAndLayer(rs.DrawFBO.name, GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                  (GLint *)&firstMip, (GLint *)&firstSlice);

        height = (uint32_t)RDCMAX(1, m_pDriver->m_Textures[id].height >> firstMip);
        break;
      }
    }

    if(height == 0)
    {
      GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                               eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&obj);

      if(obj)
      {
        GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                 eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                 (GLint *)&type);

        ResourceId id = m_pDriver->GetResourceManager()->GetResID(
            type == eGL_RENDERBUFFER ? RenderbufferRes(ctx, obj) : TextureRes(ctx, obj));

        GLint firstMip = 0, firstSlice = 0;
        GetFramebufferMipAndLayer(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, (GLint *)&firstMip,
                                  (GLint *)&firstSlice);

        height = (uint32_t)RDCMAX(1, m_pDriver->m_Textures[id].height >> firstMip);
      }
    }

    if(height == 0)
    {
      GL.glGetFramebufferParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                     (GLint *)&height);
    }

    y = height - y - 1;
  }

  rdcstr regionName = StringFormat::Fmt("DebugPixel @ %u of (%u,%u) sample %u primitive %u view %u",
                                        eventId, x, y, sample, primitive, view);

  GLMarkerRegion region(regionName);

  if(OpenGL_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(!(action->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall)))
  {
    RDCLOG("No drawcall selected");
    return new ShaderDebugTrace();
  }

  uint32_t storageBufferBinding = ~0U;

  ResourceId geom;
  ResourceId pixel;
  if(rs.Program.name)
  {
    ResourceId id = m_pDriver->GetResourceManager()->GetResID(rs.Program);
    const WrappedOpenGL::ProgramData &progDetails = m_pDriver->GetProgram(id);

    geom = progDetails.stageShaders[(uint32_t)ShaderStage::Geometry];
    pixel = progDetails.stageShaders[(uint32_t)ShaderStage::Pixel];

    ResourceId stagePrograms[NumShaderStages];
    for(size_t i = 0; i < NumShaderStages; i++)
      stagePrograms[i] = id;
    storageBufferBinding = GetStorageBufferBinding(m_pDriver, stagePrograms,
                                                   progDetails.stageShaders, ShaderStage::Pixel);
  }
  else if(rs.Pipeline.name)
  {
    ResourceId id = m_pDriver->GetResourceManager()->GetResID(rs.Pipeline);
    const WrappedOpenGL::PipelineData &pipeDetails = m_pDriver->GetPipeline(id);

    geom = pipeDetails.stageShaders[(uint32_t)ShaderStage::Geometry];
    pixel = pipeDetails.stageShaders[(uint32_t)ShaderStage::Pixel];

    storageBufferBinding = GetStorageBufferBinding(m_pDriver, pipeDetails.stagePrograms,
                                                   pipeDetails.stageShaders, ShaderStage::Pixel);
  }

  if(pixel == ResourceId())
  {
    RDCLOG("No pixel shader bound at draw");
    return new ShaderDebugTrace();
  }

  if(storageBufferBinding == ~0U)
  {
    RDCLOG("No spare SSBO available in program");
    return new ShaderDebugTrace();
  }

  // get ourselves in pristine state before this action (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  WrappedOpenGL::ShaderData &shadDetails = m_pDriver->GetWriteableShader(pixel);

  rdcstr entryPoint = shadDetails.entryPoint;
  rdcarray<SpecConstant> spec;
  for(size_t i = 0; i < shadDetails.specIDs.size() && i < shadDetails.specValues.size(); i++)
    spec.push_back(SpecConstant(shadDetails.specIDs[i], shadDetails.specValues[i], 4));

  // use converted reflection unless we had native SPIR-V reflection because the input fetcher will
  // use it as well and we might have extra inputs (that were stripped from the driver's GL reflection)
  const ShaderReflection *refl =
      shadDetails.convertedSPIRV ? &shadDetails.convertedRefl : shadDetails.GetReflection();

  if(!refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadDetails.Disassemble(entryPoint);

  GLAPIWrapper *apiWrapper = new GLAPIWrapper(m_pDriver, ShaderStage::Pixel, eventId, pixel,
                                              shadDetails.convertedAutomapped ? refl : NULL);

  SubgroupSupport subgroupSupport = SubgroupSupport::None;
  uint32_t numThreads = 4;

  const SPIRVPatchData &patchData =
      shadDetails.convertedSPIRV ? shadDetails.convertedPatchData : shadDetails.patchData;

  if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    uint32_t maxSubgroupSize = 1;
    CalculateSubgroupProperties(maxSubgroupSize, subgroupSupport);
    numThreads = RDCMAX(numThreads, maxSubgroupSize);
  }

  std::unordered_map<ShaderBuiltin, ShaderVariable> &global_builtins = apiWrapper->global_builtins;
  global_builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  global_builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), action->drawIndex, 0U, 0U, 0U);

  InputFetcherConfig cfg;

  cfg.x = x;
  cfg.y = y;

  // If the pipe contains a geometry shader, then Primitive ID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  cfg.usePrimitiveID = false;
  if(geom != ResourceId())
  {
    const WrappedOpenGL::ShaderData &gsDetails = m_pDriver->GetShader(geom);

    const ShaderReflection *gsRefl = gsDetails.GetReflection();

    // check to see if the shader outputs a primitive ID
    for(const SigParameter &e : gsRefl->outputSignature)
    {
      if(e.systemValue == ShaderBuiltin::PrimitiveIndex)
      {
        if(OpenGL_Debug_ShaderDebugLogging())
        {
          RDCLOG("Geometry shader exports primitive ID, can use");
        }

        cfg.usePrimitiveID = true;
        break;
      }
    }

    if(OpenGL_Debug_ShaderDebugLogging())
    {
      if(!cfg.usePrimitiveID)
        RDCLOG("Geometry shader doesn't export primitive ID, can't use");
    }
  }
  else
  {
    // no geometry shader - safe to use
    cfg.usePrimitiveID = true;
  }

  cfg.useSampleID = HasExt[ARB_sample_shading];

  if(OpenGL_Debug_ShaderDebugLogging())
  {
    RDCLOG("useSampleID is %u because of bare capability", cfg.useSampleID);
  }

  uint32_t paramAlign, structStride;
  rdctie(paramAlign, structStride) = GetAlignAndOutputSize(refl, patchData);

  // struct size is ResultDataBase header plus Nx structStride for the number of threads
  uint32_t structSize = sizeof(rdcspv::ResultDataBase) + structStride * numThreads;

  GLuint feedbackStorageSize = maxHits * structSize + sizeof(Vec4f) + 1024;

  if(OpenGL_Debug_ShaderDebugLogging())
  {
    RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
           feedbackStorageSize);
  }

  bytebuf data;
  {
    GLRenderState push;
    push.FetchState(m_pDriver);

    GLuint shaderFeedback;
    GL.glGenBuffers(1, &shaderFeedback);
    GL.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, shaderFeedback);
    GL.glNamedBufferDataEXT(shaderFeedback, feedbackStorageSize, NULL, eGL_DYNAMIC_DRAW);
    byte *clear = (byte *)GL.glMapBufferRange(eGL_SHADER_STORAGE_BUFFER, 0, feedbackStorageSize,
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    memset(clear, 0, feedbackStorageSize);
    GL.glUnmapBuffer(eGL_SHADER_STORAGE_BUFFER);

    GLuint inputFetcher = CreateInputFetcher(shadDetails, storageBufferBinding, cfg,
                                             subgroupSupport, paramAlign, numThreads);

    GLuint inputShader = 0;
    GLuint inputShaderSPIRV = 0;

    if(shadDetails.spirvWords.empty())
      inputShader = inputFetcher;
    else
      inputShaderSPIRV = inputFetcher;

    GLuint replacementProgram = GL.glCreateProgram();

    CreateShaderReplacementProgram(rs.Program.name, rs.Pipeline.name, replacementProgram,
                                   ShaderStage::Pixel, inputShader, inputShaderSPIRV);

    GL.glUseProgram(replacementProgram);

    if(inputShader && !IsGLES)
    {
      GLuint ssboIdx =
          GL.glGetProgramResourceIndex(replacementProgram, eGL_SHADER_STORAGE_BLOCK, "Output");
      GL.glShaderStorageBlockBinding(replacementProgram, ssboIdx, storageBufferBinding);
    }
    GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, storageBufferBinding, shaderFeedback);

    m_pDriver->ReplayLog(eventId, eventId, eReplay_OnlyDraw);

    GL.glDeleteProgram(replacementProgram);
    GL.glDeleteShader(inputFetcher);

    data.resize(feedbackStorageSize);
    GL.glGetBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, feedbackStorageSize, data.data());

    push.ApplyState(m_pDriver);
  }

  byte *base = data.data();

  uint32_t hit_count = ((uint32_t *)base)[0];
  uint32_t total_count = ((uint32_t *)base)[1];

  if(hit_count > maxHits)
  {
    RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", hit_count, maxHits);
    hit_count = maxHits;
  }

  base += sizeof(Vec4f);

  rdcspv::ResultDataBase *winner = NULL;

  RDCLOG("Got %u hit candidates out of %u total instances", hit_count, total_count);

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  GLenum depthOp = rs.DepthFunc;

  // depth tests disabled acts the same as always compare mode
  if(!rs.Enabled[GLRenderState::eEnabled_DepthTest])
    depthOp = eGL_ALWAYS;

  for(uint32_t i = 0; i < hit_count; i++)
  {
    rdcspv::ResultDataBase *hit = (rdcspv::ResultDataBase *)(base + structSize * i);

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
      case eGL_NEVER:
      case eGL_EQUAL:
      case eGL_NOTEQUAL:
      case eGL_ALWAYS:
      default:
        // don't emulate equal or not equal since we don't know the reference value. Take any hit
        // (thus meaning the last hit)
        winner = hit;
        break;
      case eGL_LESS:
        if(hit->pos.z < winner->pos.z)
          winner = hit;
        break;
      case eGL_LEQUAL:
        if(hit->pos.z <= winner->pos.z)
          winner = hit;
        break;
      case eGL_GREATER:
        if(hit->pos.z > winner->pos.z)
          winner = hit;
        break;
      case eGL_GEQUAL:
        if(hit->pos.z >= winner->pos.z)
          winner = hit;
        break;
    }
  }

  ShaderDebugTrace *ret = NULL;

  if(!OpenGL_Debug_ShaderDebugDumpDirPath().empty())
    FileIO::WriteAll(
        OpenGL_Debug_ShaderDebugDumpDirPath() + "/pixel_debug.spv",
        shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords : shadDetails.spirvWords);

  if(winner)
  {
    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords
                                               : shadDetails.spirvWords);

    // the per-thread data immediately follows the rdcspv::ResultDataBase header. Every piece of
    // data is uniformly aligned, either 16-byte by default or 32-byte if larger components exist.
    // The output is in input signature order.
    byte *LaneData = (byte *)(winner + 1);

    numThreads = 4;

    if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
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

      if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
      {
        rdcspv::SubgroupLaneData *subgroupData = (rdcspv::SubgroupLaneData *)value;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Active] = subgroupData->isActive;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::Elected] = subgroupData->elect;
        apiWrapper->thread_props[t][(size_t)rdcspv::ThreadProperty::SubgroupId] = t;

        value += sizeof(rdcspv::SubgroupLaneData);
      }

      // read PixelLaneData
      {
        rdcspv::PixelLaneData *pixelData = (rdcspv::PixelLaneData *)value;

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
      value += sizeof(rdcspv::PixelLaneData);

      for(size_t i = 0; i < refl->inputSignature.size(); i++)
      {
        const SigParameter &param = refl->inputSignature[i];

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
                               shadDetails.spirvInstructionLines, patchData, winner->laneIndex,
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

  return ret;
}

ShaderDebugTrace *GLReplay::DebugThread(uint32_t eventId, const rdcfixedarray<uint32_t, 3> &groupid,
                                        const rdcfixedarray<uint32_t, 3> &threadid)
{
  rdcstr regionName =
      StringFormat::Fmt("Debug Thread @ %u of (%u,%u,%u) (%u,%u,%u)", eventId, groupid[0],
                        groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]);

  GLMarkerRegion region(regionName);

  if(OpenGL_Debug_ShaderDebugLogging())
    RDCLOG("%s", regionName.c_str());

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(!(action->flags & ActionFlags::Dispatch))
  {
    RDCLOG("No dispatch selected");
    return new ShaderDebugTrace();
  }

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  // get ourselves in pristine state before this dispatch (without any side effects it may have had)
  m_pDriver->ReplayLog(0, eventId, eReplay_WithoutDraw);

  ResourceId comp;
  ResourceId pixel;
  if(rs.Program.name)
  {
    ResourceId id = m_pDriver->GetResourceManager()->GetResID(rs.Program);
    const WrappedOpenGL::ProgramData &progDetails = m_pDriver->GetProgram(id);

    comp = progDetails.stageShaders[(uint32_t)ShaderStage::Compute];
  }
  else if(rs.Pipeline.name)
  {
    ResourceId id = m_pDriver->GetResourceManager()->GetResID(rs.Pipeline);
    const WrappedOpenGL::PipelineData &pipeDetails = m_pDriver->GetPipeline(id);

    comp = pipeDetails.stageShaders[(uint32_t)ShaderStage::Compute];
  }

  if(comp == ResourceId())
  {
    RDCLOG("No compute shader bound at dispatch!");
    return new ShaderDebugTrace();
  }

  WrappedOpenGL::ShaderData &shadDetails = m_pDriver->GetWriteableShader(comp);

  rdcstr entryPoint = shadDetails.entryPoint;
  rdcarray<SpecConstant> spec;
  for(size_t i = 0; i < shadDetails.specIDs.size() && i < shadDetails.specValues.size(); i++)
    spec.push_back(SpecConstant(shadDetails.specIDs[i], shadDetails.specValues[i], 4));

  // use converted reflection unless we had native SPIR-V reflection because the input fetcher will
  // use it as well and we might have extra inputs (that were stripped from the driver's GL reflection)
  const ShaderReflection *refl =
      shadDetails.convertedSPIRV ? &shadDetails.convertedRefl : shadDetails.GetReflection();

  if(!refl->debugInfo.debuggable)
  {
    RDCLOG("Shader is not debuggable: %s", refl->debugInfo.debugStatus.c_str());
    return new ShaderDebugTrace();
  }

  shadDetails.Disassemble(entryPoint);

  GLAPIWrapper *apiWrapper = new GLAPIWrapper(m_pDriver, ShaderStage::Compute, eventId, comp,
                                              shadDetails.convertedAutomapped ? refl : NULL);

  uint32_t threadDim[3];
  threadDim[0] = refl->dispatchThreadsDimension[0];
  threadDim[1] = refl->dispatchThreadsDimension[1];
  threadDim[2] = refl->dispatchThreadsDimension[2];

  SubgroupSupport subgroupSupport = SubgroupSupport::None;
  uint32_t numThreads = 1;

  const SPIRVPatchData &patchData =
      shadDetails.convertedSPIRV ? shadDetails.convertedPatchData : shadDetails.patchData;

  uint32_t maxSubgroupSize = 1;
  if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    CalculateSubgroupProperties(maxSubgroupSize, subgroupSupport);
    numThreads = RDCMAX(numThreads, maxSubgroupSize);
  }

  if(patchData.threadScope & rdcspv::ThreadScope::Workgroup)
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
  if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
  {
    InputFetcherConfig cfg;

    cfg.threadid = threadid;
    cfg.groupid = groupid;

    uint32_t paramAlign, structStride;
    rdctie(paramAlign, structStride) = GetAlignAndOutputSize(refl, patchData);

    const uint32_t numHits = 4;    // we should only ever get one hit

    // struct size is rdcspv::ResultDataBase header plus Nx structStride for the number of threads
    uint32_t structSize = sizeof(rdcspv::ResultDataBase) + structStride * maxSubgroupSize;

    GLuint feedbackStorageSize = numHits * structSize + 1024;

    if(OpenGL_Debug_ShaderDebugLogging())
    {
      RDCLOG("Output structure is %u sized, output buffer is %llu bytes", structStride,
             feedbackStorageSize);
    }

    bytebuf data;
    {
      GLRenderState push;
      push.FetchState(m_pDriver);

      GLuint shaderFeedback;
      GL.glGenBuffers(1, &shaderFeedback);
      GL.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, shaderFeedback);
      GL.glNamedBufferDataEXT(shaderFeedback, feedbackStorageSize, NULL, eGL_DYNAMIC_DRAW);
      byte *clear = (byte *)GL.glMapBufferRange(eGL_SHADER_STORAGE_BUFFER, 0, feedbackStorageSize,
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      memset(clear, 0, feedbackStorageSize);
      GL.glUnmapBuffer(eGL_SHADER_STORAGE_BUFFER);

      //  because there are no other stages to worry about we can pick whichever binding we want
      const uint32_t storageBufferBinding = 0;
      GLuint inputFetcher = CreateInputFetcher(shadDetails, storageBufferBinding, cfg,
                                               subgroupSupport, paramAlign, numThreads);

      GLuint replacementProgram = GL.glCreateProgram();

      {
        GL.glAttachShader(replacementProgram, inputFetcher);

        GL.glLinkProgram(replacementProgram);

        char buffer[1024] = {};
        GLint status = 0;
        GL.glGetProgramiv(replacementProgram, eGL_LINK_STATUS, &status);
        if(status == 0)
        {
          GL.glGetProgramInfoLog(replacementProgram, 1024, NULL, buffer);
          RDCERR("Link error: %s", buffer);
        }

        GL.glDetachShader(replacementProgram, inputFetcher);
      }

      GL.glUseProgram(replacementProgram);

      if(shadDetails.spirvWords.empty() && !IsGLES)
      {
        GLuint ssboIdx =
            GL.glGetProgramResourceIndex(replacementProgram, eGL_SHADER_STORAGE_BLOCK, "Output");
        GL.glShaderStorageBlockBinding(replacementProgram, ssboIdx, storageBufferBinding);
      }
      GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, storageBufferBinding, shaderFeedback);

      m_pDriver->ReplayLog(eventId, eventId, eReplay_OnlyDraw);

      GL.glDeleteProgram(replacementProgram);
      GL.glDeleteShader(inputFetcher);

      data.resize(feedbackStorageSize);
      GL.glGetBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, feedbackStorageSize, data.data());

      push.ApplyState(m_pDriver);
    }

    byte *base = data.data();
    uint32_t hit_count = ((uint32_t *)base)[0];
    // uint32_t total_count = ((uint32_t *)base)[1];

    if(hit_count > maxHits)
    {
      RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", hit_count, maxHits);
      hit_count = maxHits;
    }

    base += sizeof(Vec4f);

    rdcspv::ResultDataBase *winner = (rdcspv::ResultDataBase *)base;

    if(winner->valid != validMagicNumber)
    {
      RDCWARN("Hit doesn't have valid magic number");

      delete apiWrapper;

      ShaderDebugTrace *ret = new ShaderDebugTrace;
      ret->stage = ShaderStage::Compute;

      return ret;
    }

    if(!OpenGL_Debug_ShaderDebugDumpDirPath().empty())
      FileIO::WriteAll(
          OpenGL_Debug_ShaderDebugDumpDirPath() + "/compute_debug.spv",
          shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords : shadDetails.spirvWords);

    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords
                                               : shadDetails.spirvWords);

    // the per-thread data immediately follows the rdcspv::ResultDataBase header. Every piece of
    // data is uniformly aligned, either 16-byte by default or 32-byte if larger components exist.
    // The output is in input signature order.
    byte *LaneData = (byte *)(winner + 1);

    numThreads = 4;
    const uint32_t subgroupSize = winner->subgroupSize;

    if(patchData.threadScope & rdcspv::ThreadScope::Subgroup)
    {
      RDCASSERTNOTEQUAL(subgroupSize, 0);
      numThreads = RDCMAX(numThreads, subgroupSize);
    }

    if(patchData.threadScope & rdcspv::ThreadScope::Workgroup)
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

      rdcspv::SubgroupLaneData *subgroupData = (rdcspv::SubgroupLaneData *)value;
      value += sizeof(rdcspv::SubgroupLaneData);

      rdcspv::ComputeLaneData *compData = (rdcspv::ComputeLaneData *)value;
      value += sizeof(rdcspv::ComputeLaneData);

      // should we try to verify that the GPU assigned subgroups as we expect? this assumes tightly wrapped subgroups
      uint32_t lane = t;

      if(patchData.threadScope & rdcspv::ThreadScope::Workgroup)
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
    if(patchData.threadScope & rdcspv::ThreadScope::Workgroup)
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

    ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Compute, entryPoint, spec,
                                                 shadDetails.spirvInstructionLines, patchData,
                                                 laneIndex, numThreads, subgroupSize);
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

    if(!OpenGL_Debug_ShaderDebugDumpDirPath().empty())
      FileIO::WriteAll(
          OpenGL_Debug_ShaderDebugDumpDirPath() + "/compute_debug.spv",
          shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords : shadDetails.spirvWords);

    rdcspv::Debugger *debugger = new rdcspv::Debugger;
    debugger->Parse(shadDetails.convertedSPIRV ? shadDetails.convertedSpirvWords
                                               : shadDetails.spirvWords);

    apiWrapper->global_builtins[ShaderBuiltin::SubgroupSize] =
        ShaderVariable(rdcstr(), 1U, 0U, 0U, 0U);

    ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Compute, entryPoint, spec,
                                                 shadDetails.spirvInstructionLines, patchData,
                                                 laneIndex, numThreads, 1);
    apiWrapper->ResetReplay();

    return ret;
  }
}

ShaderDebugTrace *GLReplay::DebugMeshThread(uint32_t eventId,
                                            const rdcfixedarray<uint32_t, 3> &groupid,
                                            const rdcfixedarray<uint32_t, 3> &threadid)
{
  GLNOTIMP("DebugMeshThread");
  return new ShaderDebugTrace();
}

rdcarray<ShaderDebugState> GLReplay::ContinueDebug(ShaderDebugger *debugger)
{
  rdcspv::Debugger *spvDebugger = (rdcspv::Debugger *)debugger;

  if(!spvDebugger)
    return {};

  GLMarkerRegion region("ContinueDebug Simulation Loop");

  rdcarray<ShaderDebugState> ret = spvDebugger->ContinueDebug();

  GLAPIWrapper *api = (GLAPIWrapper *)spvDebugger->GetAPIWrapper();
  api->ResetReplay();

  return ret;
}

void GLReplay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
}
