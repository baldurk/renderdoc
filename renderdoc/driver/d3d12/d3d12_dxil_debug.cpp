/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2025 Baldur Karlsson
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

#pragma once

#include "d3d12_dxil_debug.h"
#include "data/hlsl/hlsl_cbuffers.h"
#include "driver/dxgi/dxgi_common.h"
#include "maths/formatpacking.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_state.h"

using namespace DXIL;
using namespace DXILDebug;

#if ENABLED(RDOC_RELEASE)
#define CHECK_DEVICE_THREAD()
#else
#define CHECK_DEVICE_THREAD() \
  RDCASSERTMSG("API Wrapper function called from non-device thread!", IsDeviceThread());
#endif    // #if ENABLED(RDOC_RELEASE)

namespace DXILDebug
{
static ShaderValue TypedUAVLoad(const DXILDebug::ViewFmt &fmt, const byte *base, uint64_t offset)
{
  const byte *data = base + offset;
  ShaderValue result;
  result.f32v[0] = 0.0f;
  result.f32v[1] = 0.0f;
  result.f32v[2] = 0.0f;
  result.f32v[3] = 0.0f;

  if(fmt.byteWidth == 10)
  {
    uint32_t u;
    memcpy(&u, data, sizeof(uint32_t));

    if(fmt.compType == CompType::UInt)
    {
      result.u32v[0] = (u >> 0) & 0x3ff;
      result.u32v[1] = (u >> 10) & 0x3ff;
      result.u32v[2] = (u >> 20) & 0x3ff;
      result.u32v[3] = (u >> 30) & 0x003;
    }
    else if(fmt.compType == CompType::UNorm)
    {
      Vec4f res = ConvertFromR10G10B10A2(u);
      result.f32v[0] = res.x;
      result.f32v[1] = res.y;
      result.f32v[2] = res.z;
      result.f32v[3] = res.w;
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u;
    memcpy(&u, data, sizeof(uint32_t));

    Vec3f res = ConvertFromR11G11B10(u);
    result.f32v[0] = res.x;
    result.f32v[1] = res.y;
    result.f32v[2] = res.z;
    result.f32v[3] = 1.0f;
  }
  else
  {
    if(fmt.byteWidth == 4)
    {
      const uint32_t *u = (const uint32_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
        result.u32v[c] = u[c];
    }
    else if(fmt.byteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
      {
        const uint16_t *u = (const uint16_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.f32v[c] = ConvertFromHalf(u[c]);
      }
      else if(fmt.compType == CompType::UInt)
      {
        const uint16_t *u = (const uint16_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.u32v[c] = u[c];
      }
      else if(fmt.compType == CompType::SInt)
      {
        const int16_t *in = (const int16_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.s32v[c] = in[c];
      }
      else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
      {
        const uint16_t *u = (const uint16_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.f32v[c] = float(u[c]) / float(0xffff);
      }
      else if(fmt.compType == CompType::SNorm)
      {
        const int16_t *in = (const int16_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
        {
          // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
          if(in[c] == -32768)
            result.f32v[c] = -1.0f;
          else
            result.f32v[c] = float(in[c]) / 32767.0f;
        }
      }
      else
      {
        RDCERR("Unexpected format type on buffer resource");
      }
    }
    else if(fmt.byteWidth == 1)
    {
      if(fmt.compType == CompType::UInt)
      {
        const uint8_t *u = (const uint8_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.u32v[c] = u[c];
      }
      else if(fmt.compType == CompType::SInt)
      {
        const int8_t *in = (const int8_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.s32v[c] = in[c];
      }
      else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
      {
        const uint8_t *u = (const uint8_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
          result.f32v[c] = float(u[c]) / float(0xff);
      }
      else if(fmt.compType == CompType::SNorm)
      {
        const int8_t *in = (const int8_t *)data;

        for(int c = 0; c < fmt.numComps; c++)
        {
          // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
          if(in[c] == -128)
            result.f32v[c] = -1.0f;
          else
            result.f32v[c] = float(in[c]) / 127.0f;
        }
      }
      else
      {
        RDCERR("Unexpected format type on buffer resource");
      }
    }

    // fill in alpha with 1.0 or 1 as appropriate
    if(fmt.numComps < 4)
    {
      if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB ||
         fmt.compType == CompType::SNorm || fmt.compType == CompType::Float)
        result.f32v[3] = 1.0f;
      else
        result.u32v[3] = 1;
    }
  }

  return result;
}

static void TypedUAVStore(const DXILDebug::ViewFmt &fmt, byte *base, uint64_t offset,
                          const ShaderValue &value)
{
  byte *data = base + offset;
  if(fmt.byteWidth == 10)
  {
    uint32_t u = 0;

    if(fmt.compType == CompType::UInt)
    {
      u |= (value.u32v[0] & 0x3ff) << 0;
      u |= (value.u32v[1] & 0x3ff) << 10;
      u |= (value.u32v[2] & 0x3ff) << 20;
      u |= (value.u32v[3] & 0x3) << 30;
    }
    else if(fmt.compType == CompType::UNorm)
    {
      u = ConvertToR10G10B10A2(Vec4f(value.f32v[0], value.f32v[1], value.f32v[2], value.f32v[3]));
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
    memcpy(data, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u = ConvertToR11G11B10(Vec3f(value.f32v[0], value.f32v[1], value.f32v[2]));
    memcpy(data, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 4)
  {
    uint32_t *u = (uint32_t *)data;

    for(int c = 0; c < fmt.numComps; c++)
      u[c] = value.u32v[c];
  }
  else if(fmt.byteWidth == 2)
  {
    if(fmt.compType == CompType::Float)
    {
      uint16_t *u = (uint16_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = ConvertToHalf(value.f32v[c]);
    }
    else if(fmt.compType == CompType::UInt)
    {
      uint16_t *u = (uint16_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = value.u32v[c] & 0xffff;
    }
    else if(fmt.compType == CompType::SInt)
    {
      int16_t *i = (int16_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int16_t)RDCCLAMP(value.s32v[c], (int32_t)INT16_MIN, (int32_t)INT16_MAX);
    }
    else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
    {
      uint16_t *u = (uint16_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], 0.0f, 1.0f) * float(0xffff) + 0.5f;
        u[c] = uint16_t(f);
      }
    }
    else if(fmt.compType == CompType::SNorm)
    {
      int16_t *i = (int16_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], -1.0f, 1.0f) * 0x7fff;

        if(f < 0.0f)
          i[c] = int16_t(f - 0.5f);
        else
          i[c] = int16_t(f + 0.5f);
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
  else if(fmt.byteWidth == 1)
  {
    if(fmt.compType == CompType::UInt)
    {
      uint8_t *u = (uint8_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = value.u32v[c] & 0xff;
    }
    else if(fmt.compType == CompType::SInt)
    {
      int8_t *i = (int8_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int8_t)RDCCLAMP(value.s32v[c], (int32_t)INT8_MIN, (int32_t)INT8_MAX);
    }
    else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
    {
      uint8_t *u = (uint8_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], 0.0f, 1.0f) * float(0xff) + 0.5f;
        u[c] = uint8_t(f);
      }
    }
    else if(fmt.compType == CompType::SNorm)
    {
      int8_t *i = (int8_t *)data;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], -1.0f, 1.0f) * 0x7f;

        if(f < 0.0f)
          i[c] = int8_t(f - 0.5f);
        else
          i[c] = int8_t(f + 0.5f);
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
}

static DXBC::ResourceRetType ConvertCompTypeToResourceRetType(const CompType compType)
{
  switch(compType)
  {
    case CompType::Float: return DXBC::ResourceRetType::RETURN_TYPE_FLOAT;
    case CompType::UNormSRGB:
    case CompType::UNorm: return DXBC::ResourceRetType::RETURN_TYPE_UNORM;
    case CompType::SNorm: return DXBC::ResourceRetType::RETURN_TYPE_SNORM;
    case CompType::UInt: return DXBC::ResourceRetType::RETURN_TYPE_UINT;
    case CompType::SInt: return DXBC::ResourceRetType::RETURN_TYPE_SINT;
    case CompType::Typeless:
    case CompType::UScaled:
    case CompType::SScaled:
    case CompType::Depth:
    default:
      RDCERR("Unexpected component type %s", ToStr(compType).c_str());
      return DXBC::ResourceRetType ::RETURN_TYPE_UNKNOWN;
  }
}

static DXBCBytecode::ResourceDimension ConvertSRVResourceDimensionToResourceDimension(
    D3D12_SRV_DIMENSION dim)
{
  switch(dim)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_UNKNOWN;
    case D3D12_SRV_DIMENSION_BUFFER:
      return DXBCBytecode::ResourceDimension ::RESOURCE_DIMENSION_BUFFER;
    case D3D12_SRV_DIMENSION_TEXTURE1D:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE1D;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE1DARRAY;
    case D3D12_SRV_DIMENSION_TEXTURE2D:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2D;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DARRAY;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DMS;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DMSARRAY;
    case D3D12_SRV_DIMENSION_TEXTURE3D:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE3D;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURECUBE;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURECUBEARRAY;
    default:
      RDCERR("Unexpected SRV dimension %s", ToStr(dim).c_str());
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_UNKNOWN;
  }
}

static DXDebug::SamplerMode ConvertSamplerFilterToSamplerMode(D3D12_FILTER filter)
{
  switch(filter)
  {
    case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT:
    case D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
    case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
    case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
    case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
    case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
    case D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
    case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR:
    case D3D12_FILTER_COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT:
    case D3D12_FILTER_COMPARISON_ANISOTROPIC:
      return DXBCBytecode::SamplerMode::SAMPLER_MODE_COMPARISON;
      break;
    default: break;
  }
  return DXBCBytecode::SamplerMode::SAMPLER_MODE_DEFAULT;
}

static bool IsShaderParameterVisible(DXBC::ShaderType shaderType,
                                     D3D12_SHADER_VISIBILITY shaderVisibility)
{
  if(shaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
    return true;

  if(shaderType == DXBC::ShaderType::Vertex && shaderVisibility == D3D12_SHADER_VISIBILITY_VERTEX)
    return true;

  if(shaderType == DXBC::ShaderType::Pixel && shaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL)
    return true;

  if(shaderType == DXBC::ShaderType::Amplification &&
     shaderVisibility == D3D12_SHADER_VISIBILITY_AMPLIFICATION)
    return true;

  if(shaderType == DXBC::ShaderType::Mesh && shaderVisibility == D3D12_SHADER_VISIBILITY_MESH)
    return true;

  return false;
}

static void FillViewFmtFromResourceFormat(DXGI_FORMAT format, ViewFmt &viewFmt)
{
  RDCASSERT(format != DXGI_FORMAT_UNKNOWN);
  ResourceFormat fmt = MakeResourceFormat(format);

  viewFmt.byteWidth = fmt.compByteWidth;
  viewFmt.numComps = fmt.compCount;
  viewFmt.compType = fmt.compType;

  if(format == DXGI_FORMAT_R11G11B10_FLOAT)
    viewFmt.byteWidth = 11;
  else if(format == DXGI_FORMAT_R10G10B10A2_UINT || format == DXGI_FORMAT_R10G10B10A2_UNORM)
    viewFmt.byteWidth = 10;

  if(viewFmt.byteWidth == 10 || viewFmt.byteWidth == 11)
    viewFmt.stride = 4;    // 10 10 10 2 or 11 11 10
  else
    viewFmt.stride = viewFmt.byteWidth * viewFmt.numComps;
}

// Only valid/used fo root descriptors
// Root descriptors only have RawBuffer and StructuredBuffer types
static uint32_t GetUAVBufferStrideFromShaderMetadata(const DXIL::EntryPointInterface *reflection,
                                                     const BindingSlot &slot)
{
  for(const DXIL::EntryPointInterface::ResourceBase &bind : reflection->uavs)
  {
    if(bind.MatchesBinding(slot.shaderRegister, slot.shaderRegister, slot.registerSpace))
    {
      if(bind.uavData.shape == ResourceKind::RawBuffer ||
         bind.uavData.shape == ResourceKind::StructuredBuffer)
      {
        return bind.uavData.elementStride;
      }
    }
  }
  return 0;
}

static uint32_t GetSRVBufferStrideFromShaderMetadata(const DXIL::EntryPointInterface *reflection,
                                                     const BindingSlot &slot)
{
  for(const DXIL::EntryPointInterface::ResourceBase &bind : reflection->srvs)
  {
    if(bind.MatchesBinding(slot.shaderRegister, slot.shaderRegister, slot.registerSpace))
    {
      if(bind.srvData.shape == ResourceKind::RawBuffer ||
         bind.srvData.shape == ResourceKind::StructuredBuffer)
      {
        return bind.srvData.elementStride;
      }
    }
  }
  return 0;
}

InterpolationMode GetInterpolationModeForInputParam(const SigParameter &sig,
                                                    const rdcarray<SigParameter> &stageInputSig,
                                                    const DXIL::Program *program)
{
  if(sig.varType == VarType::SInt || sig.varType == VarType::UInt)
    return InterpolationMode::INTERPOLATION_CONSTANT;

  if((sig.varType == VarType::Float) || (sig.varType == VarType::Half))
  {
    // if we're packed with a different type on either side, we must be nointerpolation
    size_t numInputs = stageInputSig.size();
    for(size_t j = 0; j < numInputs; j++)
    {
      if(sig.regIndex == stageInputSig[j].regIndex && (stageInputSig[j].varType != sig.varType))
        return DXBC::InterpolationMode::INTERPOLATION_CONSTANT;
    }

    if(!program)
    {
      RDCERR("No DXIL program");
      return DXBC::InterpolationMode::INTERPOLATION_UNDEFINED;
    }
    // Search the DXIL shader meta data to get the interpolation mode
    const DXIL::EntryPointInterface *entryPoint = program->GetEntryPointInterface();
    if(!entryPoint)
    {
      RDCERR("No entry point interface found in DXIL program");
      return DXBC::InterpolationMode::INTERPOLATION_UNDEFINED;
    }
    for(size_t j = 0; j < entryPoint->inputs.size(); ++j)
    {
      const EntryPointInterface::Signature &dxilParam = entryPoint->inputs[j];
      int row = sig.regIndex;
      if((dxilParam.startRow <= row) && (row < (int)(dxilParam.startRow + dxilParam.rows)))
      {
        const int firstElem = sig.regChannelMask & 0x1   ? 0
                              : sig.regChannelMask & 0x2 ? 1
                              : sig.regChannelMask & 0x4 ? 2
                              : sig.regChannelMask & 0x8 ? 3
                                                         : -1;
        if(dxilParam.startCol == firstElem)
        {
          if(sig.semanticName == dxilParam.name)
          {
            return (InterpolationMode)dxilParam.interpolation;
          }
        }
      }
    }
    return DXBC::InterpolationMode::INTERPOLATION_UNDEFINED;
  }

  RDCERR("Unexpected input signature type: %s", ToStr(sig.varType).c_str());
  return InterpolationMode::INTERPOLATION_UNDEFINED;
}

void GetInterpolationModeForInputParams(const rdcarray<SigParameter> &inputSig,
                                        const DXIL::Program *program,
                                        rdcarray<DXBC::InterpolationMode> &interpModes)
{
  size_t numInputs = inputSig.size();
  interpModes.resize(numInputs);
  for(size_t i = 0; i < numInputs; i++)
  {
    const SigParameter &sig = inputSig[i];
    interpModes[i] = GetInterpolationModeForInputParam(sig, inputSig, program);
  }
}

D3D12APIWrapper::D3D12APIWrapper(WrappedID3D12Device *device, const DXIL::Program *dxilProgram,
                                 const ShaderReflection &refl, uint32_t eventId,
                                 const rdcarray<SigParameter> &inputSig)
    : m_Device(device),
      m_EntryPointInterface(dxilProgram->GetEntryPointInterface()),
      m_ShaderType(dxilProgram->GetShaderType()),
      m_EventId(eventId),
      m_Program(dxilProgram),
      m_Reflection(refl),
      m_DeviceThreadID(Threading::GetCurrentID()),
      m_QueuedOpCmdList(NULL),
      m_QueuedMathOpIndex(0),
      m_QueuedSampleGatherOpIndex(0),
      m_MathOpResultOffset(0),
      m_MaxQueuedOps(ShaderDebugConstants::MAX_SHADER_DEBUG_QUEUED_OPS),
      m_SampleGatherOpResultsStart(ShaderDebugConstants::MAX_SHADER_DEBUG_QUEUED_OPS *
                                   m_MathOpResultByteSize)
{
  // Create the storage layout for the constant buffers
  // The constant buffer data and details are filled in outside of this method
  size_t count = refl.constantBlocks.size();
  m_ConstantBlocks.resize(count);
  for(uint32_t i = 0; i < count; i++)
  {
    m_ConstantBlocks[i].type = VarType::ConstantBlock;
    const ConstantBlock &cb = m_Reflection.constantBlocks[i];
    uint32_t bindCount = cb.bindArraySize;
    if(bindCount > 1)
    {
      // Create nested structure for constant buffer array
      m_ConstantBlocks[i].members.resize(bindCount);
    }
  }

  // Add inputs to the shader trace
  const rdcarray<EntryPointInterface::Signature> &inputs = m_EntryPointInterface->inputs;

  const uint32_t countInParams = (uint32_t)inputs.size();
  if(countInParams)
  {
    // Make fake ShaderVariable struct to hold all the inputs
    ShaderVariable &inStruct = m_InputPlaceholder;
    inStruct.name = DXIL_FAKE_INPUT_STRUCT_NAME;
    inStruct.rows = 0;
    inStruct.columns = 0;
    inStruct.type = VarType::Struct;
    inStruct.members.resize(countInParams);

    for(uint32_t i = 0; i < countInParams; ++i)
    {
      const EntryPointInterface::Signature &sig = inputs[i];

      ShaderVariable &v = inStruct.members[i];

      // Get the name from the DXBC reflection
      SigParameter sigParam;
      if(FindSigParameter(inputSig, sig, sigParam))
      {
        v.name = sigParam.semanticIdxName;
      }
      else
      {
        v.name = sig.name;
      }
      v.rows = (uint8_t)sig.rows;
      v.columns = (uint8_t)sig.cols;
      v.type = VarTypeForComponentType(sig.type);
      if(v.rows <= 1)
      {
        v.rows = 1;
      }
      else
      {
        v.members.resize(v.rows);
        for(uint32_t r = 0; r < v.rows; r++)
        {
          v.members[r].rows = 1;
          v.members[r].columns = (uint8_t)sig.cols;
          v.members[r].type = v.type;
          v.members[r].name = StringFormat::Fmt("[%u]", r);
        }
        v.rows = 0;
        v.columns = 0;
        v.type = VarType::Struct;
      }

      if(v.rows == 1)
      {
        SourceVariableMapping inputMapping;
        inputMapping.name = v.name;
        inputMapping.type = v.type;
        inputMapping.rows = sig.rows;
        inputMapping.columns = sig.cols;
        inputMapping.variables.reserve(sig.cols);
        inputMapping.signatureIndex = i;
        inputMapping.variables.reserve(sig.cols);
        for(uint32_t c = 0; c < sig.cols; ++c)
        {
          DebugVariableReference ref;
          ref.type = DebugVariableType::Input;
          ref.name = inStruct.name + "." + v.name;
          ref.component = c;
          inputMapping.variables.push_back(ref);
        }
        m_SourceVars.push_back(inputMapping);
      }
      else
      {
        // Make a mapping per element
        for(const ShaderVariable &member : v.members)
        {
          SourceVariableMapping inputMapping;
          inputMapping.name = v.name + member.name;
          inputMapping.type = member.type;
          inputMapping.rows = 1;
          inputMapping.columns = member.columns;
          inputMapping.signatureIndex = -1;
          for(uint32_t c = 0; c < member.columns; ++c)
          {
            DebugVariableReference ref;
            ref.type = DebugVariableType::Input;
            ref.name = inStruct.name + "." + v.name + member.name;
            ref.component = c;
            inputMapping.variables.push_back(ref);
          }
          m_SourceVars.push_back(inputMapping);
        }
      }
    }

    // Make a single source variable mapping for the whole input struct
    SourceVariableMapping inputMapping;
    inputMapping.name = inStruct.name;
    inputMapping.type = VarType::Struct;
    inputMapping.rows = 0;
    inputMapping.columns = 0;
    inputMapping.variables.resize(1);
    inputMapping.variables.push_back(DebugVariableReference(DebugVariableType::Input, inStruct.name));
    m_SourceVars.push_back(inputMapping);
  }
}

// Must be called from the replay manager thread (the debugger thread)
D3D12APIWrapper::~D3D12APIWrapper()
{
  CHECK_DEVICE_THREAD();
  ResetReplay();
}

// Must be called from the replay manager thread (the debugger thread)
void D3D12APIWrapper::ResetReplay()
{
  CHECK_DEVICE_THREAD();
  if(!m_ResourcesDirty)
  {
    // replay the action to get back to 'normal' state for this event
    D3D12MarkerRegion region(m_Device->GetQueue()->GetReal(), "ResetReplay");
    m_Device->ReplayLog(0, m_EventId, eReplay_OnlyDraw);
    m_ResourcesDirty = true;
  }
}

// Must be called from the replay manager thread (the debugger thread)
void D3D12APIWrapper::PrepareReplayForResources()
{
  CHECK_DEVICE_THREAD();
  // if the resources are dirty, replay back to right before it.
  if(m_ResourcesDirty)
  {
    D3D12MarkerRegion region(m_Device->GetQueue()->GetReal(), "un-dirtying resources");
    m_Device->ReplayLog(0, m_EventId, eReplay_WithoutDraw);
    m_ResourcesDirty = false;
  }
}

void D3D12APIWrapper::FlattenSingleVariable(const rdcstr &cbufferName, uint32_t byteOffset,
                                            const rdcstr &basename, const ShaderVariable &v,
                                            rdcarray<ShaderVariable> &outvars)
{
  size_t outIdx = byteOffset / 16;
  size_t outComp = (byteOffset % 16) / 4;

  if(v.RowMajor())
    outvars.resize(RDCMAX(outIdx + v.rows, outvars.size()));
  else
    outvars.resize(RDCMAX(outIdx + v.columns, outvars.size()));

  if(outvars[outIdx].columns > 0)
  {
    // if we already have a variable in this slot, just copy the data for this variable and add
    // the source mapping. We should not overlap into the next register as that's not allowed.
    memcpy(&outvars[outIdx].value.u32v[outComp], &v.value.u32v[0], sizeof(uint32_t) * v.columns);
    uint32_t oldColumns = outvars[outIdx].columns;
    uint32_t newColumns = (uint32_t)(outComp + v.columns);
    uint32_t numColumns = RDCMAX(oldColumns, newColumns);
    numColumns = RDCMIN(4U, numColumns);
    outvars[outIdx].columns = (uint8_t)numColumns;

    SourceVariableMapping mapping;
    mapping.name = basename;
    mapping.type = v.type;
    mapping.rows = v.rows;
    mapping.columns = v.columns;
    mapping.offset = byteOffset;
    mapping.variables.resize(v.columns);

    for(int i = 0; i < v.columns; i++)
    {
      mapping.variables[i].type = DebugVariableType::Constant;
      mapping.variables[i].name = StringFormat::Fmt("%s[%u]", cbufferName.c_str(), outIdx);
      mapping.variables[i].component = uint16_t(outComp + i);
    }

    m_SourceVars.push_back(mapping);
  }
  else
  {
    const uint32_t numRegisters = v.RowMajor() ? v.rows : v.columns;
    for(uint32_t reg = 0; reg < numRegisters; reg++)
    {
      outvars[outIdx + reg].rows = 1;
      outvars[outIdx + reg].type = v.type;
      outvars[outIdx + reg].columns = v.columns + (uint8_t)outComp;
      outvars[outIdx + reg].flags = v.flags;
    }

    if(v.RowMajor())
    {
      for(size_t ri = 0; ri < v.rows; ri++)
        memcpy(&outvars[outIdx + ri].value.u32v[outComp], &v.value.u32v[ri * v.columns],
               sizeof(uint32_t) * v.columns);
    }
    else
    {
      // if we have a matrix stored in column major order, we need to transpose it back so we
      // can unroll it into vectors.
      for(size_t ci = 0; ci < v.columns; ci++)
        for(size_t ri = 0; ri < v.rows; ri++)
          outvars[outIdx + ci].value.u32v[ri] = v.value.u32v[ri * v.columns + ci];
    }

    SourceVariableMapping mapping;
    mapping.name = basename;
    mapping.type = v.type;
    mapping.rows = v.rows;
    mapping.columns = v.columns;
    mapping.offset = byteOffset;
    mapping.variables.resize(v.rows * v.columns);

    RDCASSERT(outComp == 0 || v.rows == 1, outComp, v.rows);

    size_t i = 0;
    for(uint8_t r = 0; r < v.rows; r++)
    {
      for(uint8_t c = 0; c < v.columns; c++)
      {
        size_t regIndex = outIdx + (v.RowMajor() ? r : c);
        size_t compIndex = outComp + (v.RowMajor() ? c : r);

        mapping.variables[i].type = DebugVariableType::Constant;
        mapping.variables[i].name = StringFormat::Fmt("%s[%zu]", cbufferName.c_str(), regIndex);
        mapping.variables[i].component = uint16_t(compIndex);
        i++;
      }
    }

    m_SourceVars.push_back(mapping);
  }
}

void D3D12APIWrapper::FlattenVariables(const rdcstr &cbufferName,
                                       const rdcarray<ShaderConstant> &constants,
                                       const rdcarray<ShaderVariable> &invars,
                                       rdcarray<ShaderVariable> &outvars, const rdcstr &prefix,
                                       uint32_t baseOffset)
{
  RDCASSERTEQUAL(constants.size(), invars.size());

  for(size_t i = 0; i < constants.size(); i++)
  {
    const ShaderConstant &c = constants[i];
    const ShaderVariable &v = invars[i];

    uint32_t byteOffset = baseOffset + c.byteOffset;

    rdcstr basename = prefix + rdcstr(v.name);

    if(v.type == VarType::Struct)
    {
      // check if this is an array of structs or not
      if(c.type.elements == 1)
      {
        FlattenVariables(cbufferName, c.type.members, v.members, outvars, basename + ".", byteOffset);
      }
      else
      {
        for(int m = 0; m < v.members.count(); m++)
        {
          FlattenVariables(cbufferName, c.type.members, v.members[m].members, outvars,
                           StringFormat::Fmt("%s[%zu].", basename.c_str(), m),
                           byteOffset + m * c.type.arrayByteStride);
        }
      }
    }
    else if(c.type.elements > 1 || (v.rows == 0 && v.columns == 0) || !v.members.empty())
    {
      for(int m = 0; m < v.members.count(); m++)
      {
        FlattenSingleVariable(cbufferName, byteOffset + m * c.type.arrayByteStride,
                              StringFormat::Fmt("%s[%zu]", basename.c_str(), m), v.members[m],
                              outvars);
      }
    }
    else
    {
      FlattenSingleVariable(cbufferName, byteOffset, basename, v, outvars);
    }
  }
}

// Must be called from the replay manager thread (the debugger thread)
void D3D12APIWrapper::FetchConstantBufferData(const D3D12RenderState::RootSignature &rootsig)
{
  CHECK_DEVICE_THREAD();
  WrappedID3D12RootSignature *pD3D12RootSig =
      m_Device->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootsig.rootsig);
  const DXBC::ShaderType shaderType = m_Program->GetShaderType();

  size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), rootsig.sigelems.size());
  for(size_t i = 0; i < numParams; i++)
  {
    const D3D12RootSignatureParameter &rootSigParam = pD3D12RootSig->sig.Parameters[i];
    const D3D12RenderState::SignatureElement &element = rootsig.sigelems[i];
    if(IsShaderParameterVisible(shaderType, rootSigParam.ShaderVisibility))
    {
      if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
         element.type == eRootConst)
      {
        BindingSlot slot(rootSigParam.Constants.ShaderRegister, rootSigParam.Constants.RegisterSpace);
        UINT sizeBytes = sizeof(uint32_t) * RDCMIN(rootSigParam.Constants.Num32BitValues,
                                                   (UINT)element.constants.size());
        bytebuf cbufData((const byte *)element.constants.data(), sizeBytes);
        AddCBufferToGlobalState(slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && element.type == eRootCBV)
      {
        BindingSlot slot(rootSigParam.Descriptor.ShaderRegister,
                         rootSigParam.Descriptor.RegisterSpace);
        ID3D12Resource *cbv =
            m_Device->GetResourceManager()->GetCurrentAs<ID3D12Resource>(element.id);
        bytebuf cbufData;
        m_Device->GetDebugManager()->GetBufferData(cbv, element.offset, 0, cbufData);
        AddCBufferToGlobalState(slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
              element.type == eRootTable)
      {
        UINT prevTableOffset = 0;
        WrappedID3D12DescriptorHeap *heap =
            m_Device->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

        size_t numRanges = rootSigParam.ranges.size();
        for(size_t r = 0; r < numRanges; r++)
        {
          // For this traversal we only care about CBV descriptor ranges, but we still need to
          // calculate the table offsets in case a descriptor table has a combination of
          // different range types
          const D3D12_DESCRIPTOR_RANGE1 &range = rootSigParam.ranges[r];

          UINT offset = range.OffsetInDescriptorsFromTableStart;
          if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
            offset = prevTableOffset;

          D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
          desc += element.offset;
          desc += offset;

          UINT numDescriptors = range.NumDescriptors;
          if(numDescriptors == UINT_MAX)
          {
            // Find out how many descriptors are left after
            numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

            // TODO: Look up the bind point in the D3D12 state to try to get
            // a better guess at the number of descriptors
          }

          prevTableOffset = offset + numDescriptors;

          if(range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
            continue;

          BindingSlot slot(range.BaseShaderRegister, range.RegisterSpace);

          bytebuf cbufData;
          for(UINT n = 0; n < numDescriptors; ++n, ++slot.shaderRegister)
          {
            const D3D12_CONSTANT_BUFFER_VIEW_DESC &cbv = desc->GetCBV();
            ResourceId resId;
            uint64_t byteOffset = 0;
            WrappedID3D12Resource::GetResIDFromAddr(cbv.BufferLocation, resId, byteOffset);
            ID3D12Resource *pCbvResource =
                m_Device->GetResourceManager()->GetCurrentAs<ID3D12Resource>(resId);
            cbufData.clear();

            if(cbv.SizeInBytes > 0)
              m_Device->GetDebugManager()->GetBufferData(pCbvResource, byteOffset, cbv.SizeInBytes,
                                                         cbufData);
            AddCBufferToGlobalState(slot, cbufData);

            desc++;
          }
        }
      }
    }
  }
}

void D3D12APIWrapper::AddCBufferToGlobalState(const BindingSlot &slot, bytebuf &cbufData)
{
  // Find the identifier
  size_t numCBs = m_Reflection.constantBlocks.size();
  for(size_t i = 0; i < numCBs; ++i)
  {
    const ConstantBlock &cb = m_Reflection.constantBlocks[i];
    if(slot.registerSpace == (uint32_t)cb.fixedBindSetOrSpace &&
       slot.shaderRegister >= (uint32_t)cb.fixedBindNumber &&
       slot.shaderRegister < (uint32_t)(cb.fixedBindNumber + cb.bindArraySize))
    {
      uint32_t arrayIndex = slot.shaderRegister - cb.fixedBindNumber;

      rdcarray<ShaderVariable> &targetVars = cb.bindArraySize > 1
                                                 ? m_ConstantBlocks[i].members[arrayIndex].members
                                                 : m_ConstantBlocks[i].members;
      RDCASSERTMSG("Reassigning previously filled cbuffer", targetVars.empty());

      ConstantBlockReference constantBlockRef = {i, arrayIndex};
      m_ConstantBlocksDatas[constantBlockRef] = cbufData;
      rdcstr resName = Debugger::GetResourceReferenceName(m_Program, ResourceClass::CBuffer, slot);
      m_ConstantBlocks[i].name = resName;

      SourceVariableMapping cbSourceMapping;
      cbSourceMapping.name = m_Reflection.constantBlocks[i].name;
      cbSourceMapping.variables.push_back(
          DebugVariableReference(DebugVariableType::Constant, m_ConstantBlocks[i].name));
      m_SourceVars.push_back(cbSourceMapping);

      rdcstr identifierPrefix = m_ConstantBlocks[i].name;
      rdcstr variablePrefix = m_Reflection.constantBlocks[i].name;
      if(cb.bindArraySize > 1)
      {
        identifierPrefix = StringFormat::Fmt("%s[%u]", m_ConstantBlocks[i].name.c_str(), arrayIndex);
        variablePrefix =
            StringFormat::Fmt("%s[%u]", m_Reflection.constantBlocks[i].name.c_str(), arrayIndex);

        // The above sourceVar is for the logical identifier, and FlattenVariables adds the
        // individual elements of the constant buffer. For CB arrays, add an extra source
        // var for the CB array index
        SourceVariableMapping cbArrayMapping;
        m_ConstantBlocks[i].members[arrayIndex].name = StringFormat::Fmt("[%u]", arrayIndex);
        cbArrayMapping.name = variablePrefix;
        cbArrayMapping.variables.push_back(
            DebugVariableReference(DebugVariableType::Constant, identifierPrefix));
        m_SourceVars.push_back(cbArrayMapping);
      }
      const rdcarray<ShaderConstant> &constants =
          (cb.bindArraySize > 1) ? m_Reflection.constantBlocks[i].variables[0].type.members
                                 : m_Reflection.constantBlocks[i].variables;

      rdcarray<ShaderVariable> vars;
      StandardFillCBufferVariables(m_Reflection.resourceId, constants, vars, cbufData);
      FlattenVariables(identifierPrefix, constants, vars, targetVars, variablePrefix + ".", 0);
      for(size_t c = 0; c < targetVars.size(); c++)
        targetVars[c].name = StringFormat::Fmt("[%u]", (uint32_t)c);

      return;
    }
  }
}

// Called from any thread
// Resource must be cached
ShaderValue D3D12APIWrapper::TypedSRVLoad(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                                          uint64_t dataOffset) const
{
  SCOPED_READLOCK(m_SRVsLock);
  auto it = m_SRVBuffers.find(slot);
  if(it == m_SRVBuffers.end())
  {
    RDCERR("Load SRV slot %u space %u no cached data", slot.shaderRegister, slot.registerSpace);
    return ShaderValue();
  }
  const bytebuf &data = it->second;
  return DXILDebug::TypedUAVLoad(fmt, data.data(), dataOffset);
}

// Called from any thread
// Resource must be cached
bool D3D12APIWrapper::TypedSRVStore(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                                    uint64_t dataOffset, const ShaderValue &value)
{
  SCOPED_READLOCK(m_SRVsLock);
  auto it = m_SRVBuffers.find(slot);
  if(it == m_SRVBuffers.end())
  {
    RDCERR("Store SRV slot %u space %u no cached data", slot.shaderRegister, slot.registerSpace);
    return false;
  }
  bytebuf &data = it->second;
  DXILDebug::TypedUAVStore(fmt, data.data(), dataOffset, value);
  return true;
}

// Must be called from the replay manager thread (the debugger thread)
SRVInfo D3D12APIWrapper::FetchSRV(const D3D12Descriptor *resDescriptor, const BindingSlot &slot)
{
  CHECK_DEVICE_THREAD();
  SRVInfo srvData;
  bytebuf data;
  if(resDescriptor)
  {
    D3D12ResourceManager *rm = m_Device->GetResourceManager();
    ResourceId srvId = resDescriptor->GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
    if(pResource)
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = resDescriptor->GetSRV();
      if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
        srvDesc = MakeSRVDesc(pResource->GetDesc());

      if(srvDesc.Format != DXGI_FORMAT_UNKNOWN)
      {
        DXILDebug::FillViewFmtFromResourceFormat(srvDesc.Format, srvData.resInfo.format);
      }
      else
      {
        D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
        if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
          srvData.resInfo.format.stride = srvDesc.Buffer.StructureByteStride;
      }

      if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
      {
        srvData.resInfo.firstElement = (uint32_t)srvDesc.Buffer.FirstElement;
        srvData.resInfo.numElements = srvDesc.Buffer.NumElements;
        srvData.resInfo.isByteBuffer =
            ((srvDesc.Buffer.Flags & D3D12_BUFFER_SRV_FLAG_RAW) != 0) ? true : false;
        // Get the buffer stride from the shader metadata (for StructuredBuffer, RawBuffer)
        uint32_t mdStride =
            DXILDebug::GetSRVBufferStrideFromShaderMetadata(m_EntryPointInterface, slot);
        if(mdStride != 0)
          srvData.resInfo.format.stride = mdStride;

        m_Device->GetDebugManager()->GetBufferData(pResource, 0, 0, data);
      }
      // Textures are sampled via a pixel shader, so there's no need to copy their data
    }
  }
  srvData.resInfo.hasData = data.data() != NULL;
  srvData.resInfo.dataSize = data.size();
  {
    SCOPED_WRITELOCK(m_SRVsLock);
    auto it = m_SRVInfos.insert(std::make_pair(slot, srvData));
    RDCASSERT(it.second);
    auto bufferIt = m_SRVBuffers.insert(std::make_pair(slot, data));
    RDCASSERT(bufferIt.second);
  }
  return srvData;
}

// Must be called from the replay manager thread (the debugger thread)
SRVInfo D3D12APIWrapper::FetchSRV(const BindingSlot &slot)
{
  CHECK_DEVICE_THREAD();
  // the resources might be dirty from side-effects, replay back to right before it.
  PrepareReplayForResources();

  // Direct access resource
  if(slot.heapType != DXDebug::HeapDescriptorType::NoHeap)
  {
    const HeapDescriptorType heapType = slot.heapType;
    const uint32_t descriptorIndex = slot.descriptorIndex;
    const D3D12Descriptor srvDesc =
        D3D12ShaderDebug::FindDescriptor(m_Device, heapType, descriptorIndex);
    return FetchSRV(&srvDesc, slot);
  }

  SRVInfo srvData;
  bytebuf data;
  const D3D12RenderState &rs = m_Device->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_Device->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(m_ShaderType == DXBC::ShaderType::Compute)
  {
    if(rs.compute.rootsig != ResourceId())
    {
      pRootSignature = &rs.compute;
    }
  }
  else if(rs.graphics.rootsig != ResourceId())
  {
    pRootSignature = &rs.graphics;
  }

  if(pRootSignature)
  {
    WrappedID3D12RootSignature *pD3D12RootSig =
        rm->GetCurrentAs<WrappedID3D12RootSignature>(pRootSignature->rootsig);

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(m_ShaderType, param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && element.type == eRootSRV)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            // Found the requested SRV
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);
            if(pResource)
            {
              D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

              // DXC allows root buffers to have a stride of up to 16 bytes in the shader, which
              // means encoding the byte offset into the first element here is wrong without
              // knowing what the actual accessed stride is. Instead we only fetch the data from
              // that offset onwards.

              // Root buffers are typeless, try with the resource desc format
              // The debugger code will handle DXGI_FORMAT_UNKNOWN
              if(resDesc.Format != DXGI_FORMAT_UNKNOWN)
                DXILDebug::FillViewFmtFromResourceFormat(resDesc.Format, srvData.resInfo.format);

              srvData.resInfo.isRootDescriptor = true;
              srvData.resInfo.firstElement = 0;
              // root arguments have no bounds checking, so use the most conservative number of elements
              srvData.resInfo.numElements = uint32_t(resDesc.Width - element.offset);

              if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
              {
                // Get the buffer stride from the shader metadata (for StructuredBuffer, RawBuffer)
                uint32_t mdStride =
                    DXILDebug::GetSRVBufferStrideFromShaderMetadata(m_EntryPointInterface, slot);
                if(mdStride != 0)
                  srvData.resInfo.format.stride = mdStride;
                m_Device->GetDebugManager()->GetBufferData(pResource, element.offset, 0, data);
              }
            }
            srvData.resInfo.hasData = data.data() != NULL;
            srvData.resInfo.dataSize = data.size();
            {
              SCOPED_WRITELOCK(m_SRVsLock);
              auto it = m_SRVInfos.insert(std::make_pair(slot, srvData));
              RDCASSERT(it.second);
              auto bufferIt = m_SRVBuffers.insert(std::make_pair(slot, data));
              RDCASSERT(bufferIt.second);
            }
            return srvData;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                element.type == eRootTable)
        {
          UINT prevTableOffset = 0;
          WrappedID3D12DescriptorHeap *heap =
              rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

          size_t numRanges = param.ranges.size();
          for(size_t r = 0; r < numRanges; ++r)
          {
            const D3D12_DESCRIPTOR_RANGE1 &range = param.ranges[r];

            // For every range, check the number of descriptors so that we are accessing the
            // correct data for append descriptor tables, even if the range type doesn't match
            // what we need to fetch
            UINT offset = range.OffsetInDescriptorsFromTableStart;
            if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
              offset = prevTableOffset;

            D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
            desc += element.offset;
            desc += offset;

            UINT numDescriptors = range.NumDescriptors;
            if(numDescriptors == UINT_MAX)
            {
              // Find out how many descriptors are left after
              numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

              // TODO: Should we look up the bind point in the D3D12 state to try to get
              // a better guess at the number of descriptors?
            }

            prevTableOffset = offset + numDescriptors;

            // Check if the range is for SRVs and the slot we want is contained
            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV &&
               slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              return FetchSRV(desc, slot);
            }
          }
        }
      }
    }

    RDCERR("Couldn't find root signature parameter corresponding to SRV %u in space %u",
           slot.shaderRegister, slot.registerSpace);
    {
      SCOPED_WRITELOCK(m_SRVsLock);
      m_SRVInfos[slot] = srvData;
    }
    return srvData;
  }

  RDCERR("No root signature bound, couldn't identify SRV %u in space %u", slot.shaderRegister,
         slot.registerSpace);
  {
    SCOPED_WRITELOCK(m_SRVsLock);
    m_SRVInfos[slot] = srvData;
  }
  return srvData;
}

// Called from any thread
// Resource must be cached
ShaderValue D3D12APIWrapper::TypedUAVLoad(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                                          uint64_t dataOffset) const
{
  SCOPED_READLOCK(m_UAVsLock);
  auto it = m_UAVBuffers.find(slot);
  if(it == m_UAVBuffers.end())
  {
    RDCERR("Load UAV slot %u space %u no cached data", slot.shaderRegister, slot.registerSpace);
    return ShaderValue();
  }
  const bytebuf &data = it->second;
  return DXILDebug::TypedUAVLoad(fmt, data.data(), dataOffset);
}

// Called from any thread
// Resource must be cached
bool D3D12APIWrapper::TypedUAVStore(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                                    uint64_t dataOffset, const ShaderValue &value)
{
  SCOPED_READLOCK(m_UAVsLock);
  auto it = m_UAVBuffers.find(slot);
  if(it == m_UAVBuffers.end())
  {
    RDCERR("Store UAV slot %u space %u no cached data", slot.shaderRegister, slot.registerSpace);
    return false;
  }
  bytebuf &data = it->second;
  DXILDebug::TypedUAVStore(fmt, data.data(), dataOffset, value);
  return true;
}

// Must be called from the replay manager thread (the debugger thread)
UAVInfo D3D12APIWrapper::FetchUAV(const D3D12Descriptor *resDescriptor, const BindingSlot &slot)
{
  CHECK_DEVICE_THREAD();
  UAVInfo uavData;
  bytebuf data;
  if(resDescriptor)
  {
    D3D12ResourceManager *rm = m_Device->GetResourceManager();
    ResourceId uavId = resDescriptor->GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);

    if(pResource)
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = resDescriptor->GetUAV();

      if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
        uavDesc = MakeUAVDesc(pResource->GetDesc());

      if(uavDesc.Format != DXGI_FORMAT_UNKNOWN)
      {
        DXILDebug::FillViewFmtFromResourceFormat(uavDesc.Format, uavData.resInfo.format);
      }

      if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
      {
        uavData.resInfo.firstElement = (uint32_t)uavDesc.Buffer.FirstElement;
        uavData.resInfo.numElements = uavDesc.Buffer.NumElements;
        uavData.resInfo.isByteBuffer =
            ((uavDesc.Buffer.Flags & D3D12_BUFFER_UAV_FLAG_RAW) != 0) ? true : false;
        // Get the buffer stride from the shader metadata (for StructuredBuffer, RawBuffer)
        uint32_t mdStride =
            DXILDebug::GetUAVBufferStrideFromShaderMetadata(m_EntryPointInterface, slot);
        if(mdStride != 0)
          uavData.resInfo.format.stride = mdStride;

        m_Device->GetDebugManager()->GetBufferData(pResource, 0, 0, data);

        ResourceId counterId = resDescriptor->GetCounterResourceId();
        if(counterId != ResourceId())
        {
          uint64_t counterByteOffset = uavDesc.Buffer.CounterOffsetInBytes;
          ID3D12Resource *pCounterResource = rm->GetCurrentAs<ID3D12Resource>(counterId);
          if(pCounterResource)
          {
            bytebuf counterData;
            m_Device->GetDebugManager()->GetBufferData(pCounterResource, counterByteOffset, 4,
                                                       counterData);
            // Initialise the UAV counter from the buffer
            if(counterData.size() == 4)
              uavData.hiddenCounter = *((uint32_t *)counterData.data());
            else
              RDCERR("Couldn't read UAV counter data for UAV in slot %u space %u",
                     slot.shaderRegister, slot.registerSpace);
          }
          else
          {
            RDCERR("NULL counter resource for UAV in slot %u space %u", slot.shaderRegister,
                   slot.registerSpace);
          }
        }
      }
      else
      {
        uavData.tex = true;
        m_Device->GetReplay()->GetTextureData(uavId, Subresource(), GetTextureDataParams(), data);

        uavDesc.Format = D3D12ShaderDebug::GetUAVResourceFormat(uavDesc, pResource);
        DXILDebug::FillViewFmtFromResourceFormat(uavDesc.Format, uavData.resInfo.format);
        D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
        uavData.rowPitch = GetByteSize((int)resDesc.Width, 1, 1, uavDesc.Format, 0);
        uavData.depthPitch =
            GetByteSize((int)resDesc.Width, (int)(resDesc.Height), 1, uavDesc.Format, 0);
      }
    }
  }
  uavData.resInfo.hasData = data.data() != NULL;
  uavData.resInfo.dataSize = data.size();
  {
    SCOPED_WRITELOCK(m_UAVsLock);
    auto it = m_UAVInfos.insert(std::make_pair(slot, uavData));
    RDCASSERT(it.second);
    auto bufferIt = m_UAVBuffers.insert(std::make_pair(slot, data));
    RDCASSERT(bufferIt.second);
  }
  return uavData;
}

// Must be called from the replay manager thread (the debugger thread)
UAVInfo D3D12APIWrapper::FetchUAV(const BindingSlot &slot)
{
  CHECK_DEVICE_THREAD();
  // the resources might be dirty from side-effects, replay back to right before it.
  PrepareReplayForResources();

  // Direct access resource
  if(slot.heapType != DXDebug::HeapDescriptorType::NoHeap)
  {
    const HeapDescriptorType heapType = slot.heapType;
    const uint32_t descriptorIndex = slot.descriptorIndex;
    const D3D12Descriptor uavDesc =
        D3D12ShaderDebug::FindDescriptor(m_Device, heapType, descriptorIndex);
    return FetchUAV(&uavDesc, slot);
  }

  UAVInfo uavData;
  bytebuf data;
  const D3D12RenderState &rs = m_Device->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_Device->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(m_ShaderType == DXBC::ShaderType::Compute)
  {
    if(rs.compute.rootsig != ResourceId())
    {
      pRootSignature = &rs.compute;
    }
  }
  else if(rs.graphics.rootsig != ResourceId())
  {
    pRootSignature = &rs.graphics;
  }

  if(pRootSignature)
  {
    WrappedID3D12RootSignature *pD3D12RootSig =
        rm->GetCurrentAs<WrappedID3D12RootSignature>(pRootSignature->rootsig);

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(m_ShaderType, param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && element.type == eRootUAV)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            // Found the requested UAV
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);

            if(pResource)
            {
              D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
              // DXC allows root buffers to have a stride of up to 16 bytes in the shader, which
              // means encoding the byte offset into the first element here is wrong without
              // knowing what the actual accessed stride is. Instead we only fetch the data from
              // that offset onwards.

              // Root buffers are typeless, try with the resource desc format
              // The debugger code will handle DXGI_FORMAT_UNKNOWN
              if(resDesc.Format != DXGI_FORMAT_UNKNOWN)
              {
                DXILDebug::FillViewFmtFromResourceFormat(resDesc.Format, uavData.resInfo.format);
              }

              uavData.resInfo.isRootDescriptor = true;
              uavData.resInfo.firstElement = 0;
              // root arguments have no bounds checking, use the most conservative number of elements
              uavData.resInfo.numElements = uint32_t(resDesc.Width - element.offset);

              if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
              {
                // Get the buffer stride from the shader metadata (for StructuredBuffer, RawBuffer)
                uint32_t mdStride =
                    DXILDebug::GetUAVBufferStrideFromShaderMetadata(m_EntryPointInterface, slot);
                if(mdStride != 0)
                  uavData.resInfo.format.stride = mdStride;
                m_Device->GetDebugManager()->GetBufferData(pResource, element.offset, 0, data);
              }
            }

            uavData.resInfo.hasData = data.data() != NULL;
            uavData.resInfo.dataSize = data.size();
            {
              SCOPED_WRITELOCK(m_UAVsLock);
              auto it = m_UAVInfos.insert(std::make_pair(slot, uavData));
              RDCASSERT(it.second);
              auto bufferIt = m_UAVBuffers.insert(std::make_pair(slot, data));
              RDCASSERT(bufferIt.second);
            }
            return uavData;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                element.type == eRootTable)
        {
          UINT prevTableOffset = 0;
          WrappedID3D12DescriptorHeap *heap =
              rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

          size_t numRanges = param.ranges.size();
          for(size_t r = 0; r < numRanges; ++r)
          {
            const D3D12_DESCRIPTOR_RANGE1 &range = param.ranges[r];

            // For every range, check the number of descriptors so that we are accessing the
            // correct data for append descriptor tables, even if the range type doesn't match
            // what we need to fetch
            UINT offset = range.OffsetInDescriptorsFromTableStart;
            if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
              offset = prevTableOffset;

            D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
            desc += element.offset;
            desc += offset;

            UINT numDescriptors = range.NumDescriptors;
            if(numDescriptors == UINT_MAX)
            {
              // Find out how many descriptors are left after
              numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

              // TODO: Should we look up the bind point in the D3D12 state to try to get
              // a better guess at the number of descriptors?
            }

            prevTableOffset = offset + numDescriptors;

            // Check if the range is for UAVs and the slot we want is contained
            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV &&
               slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              return FetchUAV(desc, slot);
            }
          }
        }
      }
    }

    RDCERR("Couldn't find root signature parameter corresponding to UAV %u in space %u",
           slot.shaderRegister, slot.registerSpace);
    {
      SCOPED_WRITELOCK(m_UAVsLock);
      m_UAVInfos[slot] = uavData;
    }
    return uavData;
  }

  RDCERR("No root signature bound, couldn't identify UAV %u in space %u", slot.shaderRegister,
         slot.registerSpace);
  {
    SCOPED_WRITELOCK(m_UAVsLock);
    m_UAVInfos[slot] = uavData;
  }
  return uavData;
}

// Called from any thread
bool D3D12APIWrapper::IsSRVCached(const BindingSlot &slot) const
{
  SCOPED_READLOCK(m_SRVsLock);
  return m_SRVInfos.find(slot) != m_SRVInfos.end();
}

// Called from any thread
SRVInfo D3D12APIWrapper::GetSRV(const BindingSlot &slot)
{
  {
    SCOPED_READLOCK(m_SRVsLock);
    auto it = m_SRVInfos.find(slot);
    if(it != m_SRVInfos.end())
      return it->second;
  }

  return FetchSRV(slot);
}

// Called from any thread
bool D3D12APIWrapper::IsUAVCached(const BindingSlot &slot) const
{
  SCOPED_READLOCK(m_UAVsLock);
  return m_UAVInfos.find(slot) != m_UAVInfos.end();
}

// Called from any thread
UAVInfo D3D12APIWrapper::GetUAV(const BindingSlot &slot)
{
  {
    SCOPED_READLOCK(m_UAVsLock);
    auto it = m_UAVInfos.find(slot);
    if(it != m_UAVInfos.end())
      return it->second;
  }

  return FetchUAV(slot);
}

// Must be called from the replay manager thread (the debugger thread)
bool D3D12APIWrapper::QueueMathIntrinsic(DXIL::DXOp dxOp, const ShaderVariable &input)
{
  CHECK_DEVICE_THREAD();
  ID3D12GraphicsCommandListX *cmdList = m_QueuedOpCmdList;
  if(!cmdList)
  {
    if(StartQueuedOps())
      cmdList = m_QueuedOpCmdList;
  }
  if(!cmdList)
    return false;

  if(!QueuedOpsHasSpace())
  {
    m_Device->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                              MessageSource::RuntimeWarning, "Too many GPU queued operations");
    return false;
  }

  D3D12MarkerRegion region(m_Device->GetQueue()->GetReal(), "QueueMathIntrinsic");

  int mathOp;
  switch(dxOp)
  {
    case DXOp::Cos: mathOp = DEBUG_SAMPLE_MATH_DXIL_COS; break;
    case DXOp::Sin: mathOp = DEBUG_SAMPLE_MATH_DXIL_SIN; break;
    case DXOp::Tan: mathOp = DEBUG_SAMPLE_MATH_DXIL_TAN; break;
    case DXOp::Acos: mathOp = DEBUG_SAMPLE_MATH_DXIL_ACOS; break;
    case DXOp::Asin: mathOp = DEBUG_SAMPLE_MATH_DXIL_ASIN; break;
    case DXOp::Atan: mathOp = DEBUG_SAMPLE_MATH_DXIL_ATAN; break;
    case DXOp::Hcos: mathOp = DEBUG_SAMPLE_MATH_DXIL_HCOS; break;
    case DXOp::Hsin: mathOp = DEBUG_SAMPLE_MATH_DXIL_HSIN; break;
    case DXOp::Htan: mathOp = DEBUG_SAMPLE_MATH_DXIL_HTAN; break;
    case DXOp::Exp: mathOp = DEBUG_SAMPLE_MATH_DXIL_EXP; break;
    case DXOp::Log: mathOp = DEBUG_SAMPLE_MATH_DXIL_LOG; break;
    case DXOp::Sqrt: mathOp = DEBUG_SAMPLE_MATH_DXIL_SQRT; break;
    case DXOp::Rsqrt: mathOp = DEBUG_SAMPLE_MATH_DXIL_RSQRT; break;
    default:
      // To support a new instruction, the shader created in
      // D3D12DebugManager::CreateShaderDebugResources will need updating
      RDCERR("Unsupported opcode for DXIL CalculateMathIntrinsic: %s %u", ToStr(dxOp).c_str(),
             (uint)dxOp);
      return false;
  }

  if(D3D12ShaderDebug::QueueMathIntrinsic(false, m_Device, cmdList, mathOp, input,
                                          m_QueuedMathOpIndex))
  {
    m_QueuedMathOpIndex++;
    return true;
  }
  return false;
}

// Must be called from the replay manager thread (the debugger thread)
bool D3D12APIWrapper::QueueSampleGather(DXIL::DXOp dxOp, SampleGatherResourceData resourceData,
                                        SampleGatherSamplerData samplerData,
                                        const ShaderVariable &uv, const ShaderVariable &ddxCalc,
                                        const ShaderVariable &ddyCalc, const int8_t texelOffsets[3],
                                        int multisampleIndex, float lodValue, float compareValue,
                                        GatherChannel gatherChannel, uint32_t instructionIdx,
                                        int &sampleRetType)
{
  CHECK_DEVICE_THREAD();
  ID3D12GraphicsCommandListX *cmdList = m_QueuedOpCmdList;
  if(!cmdList)
  {
    if(StartQueuedOps())
      cmdList = m_QueuedOpCmdList;
  }
  if(!cmdList)
    return false;

  if(!QueuedOpsHasSpace())
  {
    m_Device->AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                              MessageSource::RuntimeWarning, "Too many GPU queued operations");
    return false;
  }

  int sampleOp;
  switch(dxOp)
  {
    case DXOp::Sample: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE; break;
    case DXOp::SampleBias: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_BIAS; break;
    case DXOp::SampleLevel: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_LEVEL; break;
    case DXOp::SampleGrad: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_GRAD; break;
    case DXOp::SampleCmp: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP; break;
    case DXOp::SampleCmpBias: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP_BIAS; break;
    case DXOp::SampleCmpLevel: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL; break;
    case DXOp::SampleCmpGrad: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP_GRAD; break;
    case DXOp::SampleCmpLevelZero: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO; break;
    case DXOp::TextureGather: sampleOp = DEBUG_SAMPLE_TEX_GATHER4; break;
    case DXOp::TextureGatherCmp: sampleOp = DEBUG_SAMPLE_TEX_GATHER4_CMP; break;
    case DXOp::CalculateLOD: sampleOp = DEBUG_SAMPLE_TEX_LOD; break;
    // In the shader DEBUG_SAMPLE_TEX_LOAD and DEBUG_SAMPLE_TEX_LOAD_MS behave equivalently
    case DXOp::TextureLoad: sampleOp = DEBUG_SAMPLE_TEX_LOAD; break;
    default:
      // To support a new instruction, the shader created in
      // D3D12DebugManager::CreateShaderDebugResources will need updating
      RDCERR("Unsupported instruction for CalculateSampleGather: %s %u", ToStr(dxOp).c_str(), dxOp);
      return false;
  }

  const char *opString = ToStr(dxOp).c_str();
  uint8_t swizzle[4] = {0, 1, 2, 3};
  if(D3D12ShaderDebug::QueueSampleGather(
         true, m_Device, m_QueuedOpCmdList, sampleOp, resourceData, samplerData, uv, ddxCalc,
         ddyCalc, texelOffsets, multisampleIndex, lodValue, compareValue, swizzle, gatherChannel,
         m_ShaderType, instructionIdx, opString, m_QueuedSampleGatherOpIndex, sampleRetType))
  {
    m_QueuedSampleGatherOpIndex++;
    return true;
  }
  return false;
}

// Must be called from the replay manager thread (the debugger thread)
bool D3D12APIWrapper::StartQueuedOps()
{
  CHECK_DEVICE_THREAD();

  RDCASSERTEQUAL(m_QueuedMathOpIndex, 0);
  RDCASSERTEQUAL(m_QueuedSampleGatherOpIndex, 0);
  RDCASSERTEQUAL(m_QueuedOpCmdList, NULL);
  RDCASSERTEQUAL(m_MathOpResultOffset, 0);

  if(m_QueuedOpCmdList)
    return false;

  m_QueuedOpCmdList = m_Device->GetDebugManager()->ResetDebugList();
  if(!m_QueuedOpCmdList)
    return false;

  return true;
}

// Must be called from the replay manager thread (the debugger thread)
bool D3D12APIWrapper::GetQueuedResults(rdcarray<ShaderVariable *> &mathOpResults,
                                       rdcarray<ShaderVariable *> &sampleGatherResults,
                                       const rdcarray<int> &sampleRetTypes)
{
  const uint32_t countMathResultsPerGpuOp = 1;
  rdcarray<const uint8_t *> swizzles;
  uint8_t swizzle[4] = {0, 1, 2, 3};
  for(size_t i = 0; i < sampleGatherResults.size(); ++i)
    swizzles.push_back(swizzle);

  bool ret = D3D12ShaderDebug::GetQueuedResults(m_Device, m_QueuedOpCmdList, mathOpResults,
                                                countMathResultsPerGpuOp, sampleGatherResults,
                                                sampleRetTypes, swizzles);

  m_QueuedOpCmdList = NULL;
  m_QueuedMathOpIndex = 0;
  m_QueuedSampleGatherOpIndex = 0;
  m_MathOpResultOffset = 0;

  return ret;
}

// Must be called from the replay manager thread (the debugger thread)
bool D3D12APIWrapper::QueuedOpsHasSpace() const
{
  return (m_QueuedMathOpIndex + m_QueuedSampleGatherOpIndex) < m_MaxQueuedOps;
}

// Called from any thread
bool D3D12APIWrapper::IsResourceInfoCached(const DXDebug::BindingSlot &slot, uint32_t mipLevel)
{
  SCOPED_READLOCK(m_ResourceInfosLock);
  ResourceInfoMiplevel resInfoMip = {slot, mipLevel};
  return m_ResourceInfos.find(resInfoMip) != m_ResourceInfos.end();
}

// Called from any thread
// Caller guarantees that if the data is not cached then we are on the device thread
ShaderVariable D3D12APIWrapper::GetResourceInfo(DXIL::ResourceClass resClass,
                                                const DXDebug::BindingSlot &slot, uint32_t mipLevel)
{
  ResourceInfoMiplevel resInfoMip = {slot, mipLevel};
  {
    SCOPED_READLOCK(m_ResourceInfosLock);
    auto it = m_ResourceInfos.find(resInfoMip);
    if(it != m_ResourceInfos.end())
      return it->second;
  }
  CHECK_DEVICE_THREAD();
  ShaderVariable resourceInfo = FetchResourceInfo(resClass, slot, mipLevel);
  {
    SCOPED_WRITELOCK(m_ResourceInfosLock);
    m_ResourceInfos[resInfoMip] = resourceInfo;
    return resourceInfo;
  }
}

// Must be called from the replay manager thread (the debugger thread)
ShaderVariable D3D12APIWrapper::FetchResourceInfo(DXIL::ResourceClass resClass,
                                                  const DXDebug::BindingSlot &slot, uint32_t mipLevel)
{
  CHECK_DEVICE_THREAD();
  D3D12_DESCRIPTOR_RANGE_TYPE descType;
  switch(resClass)
  {
    case DXIL::ResourceClass::SRV: descType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
    case DXIL::ResourceClass::UAV: descType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
    case DXIL::ResourceClass::CBuffer: descType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
    case DXIL::ResourceClass::Sampler: descType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
    default:
      RDCERR("Unsupported resource class %s", ToStr(resClass).c_str());
      return ShaderVariable();
  }
  int dim;
  return D3D12ShaderDebug::GetResourceInfo(m_Device, descType, slot, mipLevel, m_ShaderType, dim,
                                           true);
}

// Called from any thread
bool D3D12APIWrapper::IsSampleInfoCached(const DXDebug::BindingSlot &slot)
{
  SCOPED_READLOCK(m_SampleInfosLock);
  return m_SampleInfos.find(slot) != m_SampleInfos.end();
}

// Called from any thread
// Caller guarantees that if the data is not cached then we are on the device thread
ShaderVariable D3D12APIWrapper::GetSampleInfo(DXIL::ResourceClass resClass,
                                              const DXDebug::BindingSlot &slot, const char *opString)
{
  {
    SCOPED_READLOCK(m_SampleInfosLock);
    auto it = m_SampleInfos.find(slot);
    if(it != m_SampleInfos.end())
      return it->second;
  }
  CHECK_DEVICE_THREAD();
  ShaderVariable sampleInfo = FetchSampleInfo(resClass, slot, opString);
  {
    SCOPED_WRITELOCK(m_SampleInfosLock);
    m_SampleInfos[slot] = sampleInfo;
    return sampleInfo;
  }
}

// Must be called from the replay manager thread (the debugger thread)
ShaderVariable D3D12APIWrapper::FetchSampleInfo(DXIL::ResourceClass resClass,
                                                const DXDebug::BindingSlot &slot,
                                                const char *opString)
{
  CHECK_DEVICE_THREAD();
  D3D12_DESCRIPTOR_RANGE_TYPE descType;
  switch(resClass)
  {
    case DXIL::ResourceClass::SRV: descType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
    case DXIL::ResourceClass::UAV: descType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
    case DXIL::ResourceClass::CBuffer: descType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
    case DXIL::ResourceClass::Sampler: descType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
    default:
      RDCERR("Unsupported resource class %s", ToStr(resClass).c_str());
      return ShaderVariable();
  }
  return D3D12ShaderDebug::GetSampleInfo(m_Device, descType, slot, m_ShaderType, opString);
}

// Called from any thread
bool D3D12APIWrapper::IsRenderTargetSampleInfoCached()
{
  SCOPED_READLOCK(m_RenderTargetSampleInfoLock);
  return m_RenderTargetSampleInfoValid;
}

// Called from any thread
// Caller guarantees that if the data is not cached then we are on the device thread
ShaderVariable D3D12APIWrapper::GetRenderTargetSampleInfo(const char *opString)
{
  {
    SCOPED_READLOCK(m_RenderTargetSampleInfoLock);
    if(m_RenderTargetSampleInfoValid)
      return m_RenderTargetSampleInfo;
  }
  CHECK_DEVICE_THREAD();
  ShaderVariable renderTargetSampleInfo =
      D3D12ShaderDebug::GetRenderTargetSampleInfo(m_Device, m_ShaderType, opString);
  {
    SCOPED_WRITELOCK(m_RenderTargetSampleInfoLock);
    m_RenderTargetSampleInfoValid = true;
    m_RenderTargetSampleInfo = renderTargetSampleInfo;
    return m_RenderTargetSampleInfo;
  }
}

// Called from any thread
bool D3D12APIWrapper::IsResourceReferenceInfoCached(const DXDebug::BindingSlot &slot)
{
  SCOPED_READLOCK(m_ResourceReferenceInfosLock);
  return m_ResourceReferenceInfos.find(slot) != m_ResourceReferenceInfos.end();
}

// Called from any thread
// Caller guarantees that if the data is not cached then we are on the device thread
ResourceReferenceInfo D3D12APIWrapper::GetResourceReferenceInfo(const DXDebug::BindingSlot &slot)
{
  {
    SCOPED_READLOCK(m_ResourceReferenceInfosLock);
    auto it = m_ResourceReferenceInfos.find(slot);
    if(it != m_ResourceReferenceInfos.end())
      return it->second;
  }
  CHECK_DEVICE_THREAD();
  ResourceReferenceInfo resRefInfo = FetchResourceReferenceInfo(slot);
  {
    SCOPED_WRITELOCK(m_ResourceReferenceInfosLock);
    m_ResourceReferenceInfos[slot] = resRefInfo;
    return resRefInfo;
  }
}

// Must be called from the replay manager thread (the debugger thread)
ResourceReferenceInfo D3D12APIWrapper::FetchResourceReferenceInfo(const DXDebug::BindingSlot &slot)
{
  CHECK_DEVICE_THREAD();
  const HeapDescriptorType heapType = slot.heapType;
  RDCASSERT(heapType != HeapDescriptorType::NoHeap);
  const uint32_t descriptorIndex = slot.descriptorIndex;
  D3D12Descriptor desc = D3D12ShaderDebug::FindDescriptor(m_Device, heapType, descriptorIndex);

  D3D12ResourceManager *rm = m_Device->GetResourceManager();

  ResourceReferenceInfo resRefInfo;
  resRefInfo.binding.heapType = heapType;
  resRefInfo.binding.descriptorIndex = descriptorIndex;

  switch(desc.GetType())
  {
    case D3D12DescriptorType::CBV:
    {
      resRefInfo.resClass = DXIL::ResourceClass::CBuffer;
      resRefInfo.descType = DescriptorType::ConstantBuffer;
      resRefInfo.varType = VarType::ConstantBlock;
    }
    case D3D12DescriptorType::SRV:
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc.GetSRV();
      ResourceId srvId = desc.GetResResourceId();
      ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
      if(pResource)
      {
        resRefInfo.resClass = DXIL::ResourceClass::SRV;
        resRefInfo.varType = VarType::ReadOnlyResource;
        if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
          srvDesc = MakeSRVDesc(pResource->GetDesc());

        ViewFmt viewFmt;
        if(srvDesc.Format != DXGI_FORMAT_UNKNOWN)
        {
          resRefInfo.srvData.dim =
              (DXDebug::ResourceDimension)ConvertSRVResourceDimensionToResourceDimension(
                  srvDesc.ViewDimension);

          DXILDebug::FillViewFmtFromResourceFormat(srvDesc.Format, viewFmt);
          resRefInfo.srvData.compType =
              (DXDebug::ResourceRetType)ConvertCompTypeToResourceRetType(viewFmt.compType);

          D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
          resRefInfo.srvData.sampleCount = resDesc.SampleDesc.Count;
        }
      }
      else
      {
        RDCERR("Unknown SRV resource at Descriptor Index %u", descriptorIndex);
        return ResourceReferenceInfo();
      }

      if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE)
        resRefInfo.descType = DescriptorType::AccelerationStructure;
      else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER &&
              (srvDesc.Buffer.StructureByteStride > 0 || srvDesc.Format == DXGI_FORMAT_UNKNOWN))
        resRefInfo.descType = DescriptorType::Buffer;
      else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
        resRefInfo.descType = DescriptorType::TypedBuffer;
      else
        resRefInfo.descType = DescriptorType::Image;

      break;
    }
    case D3D12DescriptorType::UAV:
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = desc.GetUAV();
      resRefInfo.resClass = DXIL::ResourceClass::UAV;
      resRefInfo.varType = VarType::ReadWriteResource;
      if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
        resRefInfo.descType = uavDesc.Format != DXGI_FORMAT_UNKNOWN
                                  ? DescriptorType::ReadWriteTypedBuffer
                                  : DescriptorType::ReadWriteBuffer;
      else
        resRefInfo.descType = DescriptorType::ReadWriteImage;
      break;
    }
    case D3D12DescriptorType::Sampler:
    {
      resRefInfo.resClass = DXIL::ResourceClass::Sampler;
      resRefInfo.descType = DescriptorType::Sampler;
      resRefInfo.varType = VarType::Sampler;
      D3D12_SAMPLER_DESC2 samplerDesc = desc.GetSampler();
      // Don't think SAMPLER_MODE_MONO is supported in D3D12 (set for filter mode D3D10_FILTER_TEXT_1BIT)
      resRefInfo.samplerData.samplerMode =
          (DXDebug::SamplerMode)ConvertSamplerFilterToSamplerMode(samplerDesc.Filter);
      break;
    }
    default:
      RDCERR("Unhandled Descriptor Type %s", ToStr(desc.GetType()).c_str());
      return ResourceReferenceInfo();
  }
  return resRefInfo;
}

// Called from any thread
bool D3D12APIWrapper::IsShaderDirectAccessCached(const DXDebug::BindingSlot &slot)
{
  SCOPED_READLOCK(m_ShaderDirectAccessesLock);
  return m_ShaderDirectAccesses.find(slot) != m_ShaderDirectAccesses.end();
}

// Called from any thread
// Caller guarantees that if the data is not cached then we are on the device thread
ShaderDirectAccess D3D12APIWrapper::GetShaderDirectAccess(DescriptorType type,
                                                          const DXDebug::BindingSlot &slot)
{
  {
    SCOPED_READLOCK(m_ShaderDirectAccessesLock);
    auto it = m_ShaderDirectAccesses.find(slot);
    if(it != m_ShaderDirectAccesses.end())
      return it->second;
  }
  CHECK_DEVICE_THREAD();
  ShaderDirectAccess access = FetchShaderDirectAccess(type, slot);
  {
    SCOPED_WRITELOCK(m_ShaderDirectAccessesLock);
    m_ShaderDirectAccesses[slot] = access;
    return access;
  }
}

// Must be called from the replay manager thread (the debugger thread)
ShaderDirectAccess D3D12APIWrapper::FetchShaderDirectAccess(DescriptorType type,
                                                            const DXDebug::BindingSlot &slot)
{
  CHECK_DEVICE_THREAD();
  const HeapDescriptorType heapType = slot.heapType;
  RDCASSERT(heapType != HeapDescriptorType::NoHeap);
  uint32_t descriptorIndex = slot.descriptorIndex;

  const D3D12RenderState &rs = m_Device->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_Device->GetResourceManager();

  ShaderDirectAccess access;
  uint32_t byteSize = DXILDebug::D3D12_DESCRIPTOR_BYTESIZE;
  uint32_t byteOffset = descriptorIndex * byteSize;

  // Fetch the correct heap sampler and resource descriptor heap
  rdcarray<ResourceId> descHeaps = rs.heaps;
  for(ResourceId heapId : descHeaps)
  {
    WrappedID3D12DescriptorHeap *pD3D12Heap = rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(heapId);
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = pD3D12Heap->GetDesc();
    if(heapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
      if(heapType == HeapDescriptorType::Sampler)
      {
        RDCASSERTEQUAL(CategoryForDescriptorType(type), DescriptorCategory::Sampler);
        return ShaderDirectAccess(type, rm->GetOriginalID(heapId), byteOffset, byteSize);
      }
    }
    else
    {
      RDCASSERT(heapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      if(heapType == HeapDescriptorType::CBV_SRV_UAV)
      {
        RDCASSERTNOTEQUAL(CategoryForDescriptorType(type), DescriptorCategory::Sampler);
        return ShaderDirectAccess(type, rm->GetOriginalID(heapId), byteOffset, byteSize);
      }
    }
  }
  RDCERR("Failed to find descriptor %u %u", (uint32_t)heapType, descriptorIndex);
  return ShaderDirectAccess();
}
};
