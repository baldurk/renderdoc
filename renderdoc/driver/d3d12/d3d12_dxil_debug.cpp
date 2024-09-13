/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_state.h"

using namespace DXIL;
using namespace DXILDebug;

namespace DXILDebug
{
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

static void FillViewFmt(DXGI_FORMAT format, GlobalState::ViewFmt &viewFmt)
{
  if(format != DXGI_FORMAT_UNKNOWN)
  {
    ResourceFormat fmt = MakeResourceFormat(format);

    viewFmt.byteWidth = fmt.compByteWidth;
    viewFmt.numComps = fmt.compCount;
    viewFmt.fmt = fmt.compType;

    if(format == DXGI_FORMAT_R11G11B10_FLOAT)
      viewFmt.byteWidth = 11;
    else if(format == DXGI_FORMAT_R10G10B10A2_UINT || format == DXGI_FORMAT_R10G10B10A2_UNORM)
      viewFmt.byteWidth = 10;
  }
}

static void LookupUAVFormatFromShaderReflection(const DXBC::Reflection &reflection,
                                                const BindingSlot &slot,
                                                GlobalState::ViewFmt &viewFmt)
{
  for(const DXBC::ShaderInputBind &bind : reflection.UAVs)
  {
    if(bind.reg == slot.shaderRegister && bind.space == slot.registerSpace &&
       bind.dimension == DXBC::ShaderInputBind::DIM_BUFFER &&
       bind.retType < DXBC::RETURN_TYPE_MIXED && bind.retType != DXBC::RETURN_TYPE_UNKNOWN)
    {
      viewFmt.byteWidth = 4;
      viewFmt.numComps = bind.numComps;

      if(bind.retType == DXBC::RETURN_TYPE_UNORM)
        viewFmt.fmt = CompType::UNorm;
      else if(bind.retType == DXBC::RETURN_TYPE_SNORM)
        viewFmt.fmt = CompType::SNorm;
      else if(bind.retType == DXBC::RETURN_TYPE_UINT)
        viewFmt.fmt = CompType::UInt;
      else if(bind.retType == DXBC::RETURN_TYPE_SINT)
        viewFmt.fmt = CompType::SInt;
      else
        viewFmt.fmt = CompType::Float;

      break;
    }
  }
}

static void LookupSRVFormatFromShaderReflection(const DXBC::Reflection &reflection,
                                                const BindingSlot &slot,
                                                GlobalState::ViewFmt &viewFmt)
{
  for(const DXBC::ShaderInputBind &bind : reflection.SRVs)
  {
    if(bind.reg == slot.shaderRegister && bind.space == slot.registerSpace &&
       bind.dimension == DXBC::ShaderInputBind::DIM_BUFFER &&
       bind.retType < DXBC::RETURN_TYPE_MIXED && bind.retType != DXBC::RETURN_TYPE_UNKNOWN)
    {
      viewFmt.byteWidth = 4;
      viewFmt.numComps = bind.numComps;

      if(bind.retType == DXBC::RETURN_TYPE_UNORM)
        viewFmt.fmt = CompType::UNorm;
      else if(bind.retType == DXBC::RETURN_TYPE_SNORM)
        viewFmt.fmt = CompType::SNorm;
      else if(bind.retType == DXBC::RETURN_TYPE_UINT)
        viewFmt.fmt = CompType::UInt;
      else if(bind.retType == DXBC::RETURN_TYPE_SINT)
        viewFmt.fmt = CompType::SInt;
      else
        viewFmt.fmt = CompType::Float;

      break;
    }
  }
}

static void FlattenSingleVariable(const rdcstr &cbufferName, uint32_t byteOffset,
                                  const rdcstr &basename, const ShaderVariable &v,
                                  rdcarray<ShaderVariable> &outvars,
                                  rdcarray<SourceVariableMapping> &sourcevars)
{
  size_t outIdx = byteOffset / 16;
  size_t outComp = (byteOffset % 16) / 4;

  if(v.RowMajor())
    outvars.resize(RDCMAX(outIdx + v.rows, outvars.size()));
  else
    outvars.resize(RDCMAX(outIdx + v.columns, outvars.size()));

  if(outvars[outIdx].columns > 0)
  {
    // if we already have a variable in this slot, just copy the data for this variable and add the
    // source mapping.
    // We should not overlap into the next register as that's not allowed.
    memcpy(&outvars[outIdx].value.u32v[outComp], &v.value.u32v[0], sizeof(uint32_t) * v.columns);

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

    sourcevars.push_back(mapping);
  }
  else
  {
    const uint32_t numRegisters = v.RowMajor() ? v.rows : v.columns;
    for(uint32_t reg = 0; reg < numRegisters; reg++)
    {
      outvars[outIdx + reg].rows = 1;
      outvars[outIdx + reg].type = VarType::Unknown;
      outvars[outIdx + reg].columns = v.columns;
      outvars[outIdx + reg].flags = v.flags;
    }

    if(v.RowMajor())
    {
      for(size_t ri = 0; ri < v.rows; ri++)
        memcpy(&outvars[outIdx + ri].value.u32v[0], &v.value.u32v[ri * v.columns],
               sizeof(uint32_t) * v.columns);
    }
    else
    {
      // if we have a matrix stored in column major order, we need to transpose it back so we can
      // unroll it into vectors.
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

    sourcevars.push_back(mapping);
  }
}

static void FlattenVariables(const rdcstr &cbufferName, const rdcarray<ShaderConstant> &constants,
                             const rdcarray<ShaderVariable> &invars,
                             rdcarray<ShaderVariable> &outvars, const rdcstr &prefix,
                             uint32_t baseOffset, rdcarray<SourceVariableMapping> &sourceVars)
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
        FlattenVariables(cbufferName, c.type.members, v.members, outvars, basename + ".",
                         byteOffset, sourceVars);
      }
      else
      {
        for(int m = 0; m < v.members.count(); m++)
        {
          FlattenVariables(cbufferName, c.type.members, v.members[m].members, outvars,
                           StringFormat::Fmt("%s[%zu].", basename.c_str(), m),
                           byteOffset + m * c.type.arrayByteStride, sourceVars);
        }
      }
    }
    else if(c.type.elements > 1 || (v.rows == 0 && v.columns == 0) || !v.members.empty())
    {
      for(int m = 0; m < v.members.count(); m++)
      {
        FlattenSingleVariable(cbufferName, byteOffset + m * c.type.arrayByteStride,
                              StringFormat::Fmt("%s[%zu]", basename.c_str(), m), v.members[m],
                              outvars, sourceVars);
      }
    }
    else
    {
      FlattenSingleVariable(cbufferName, byteOffset, basename, v, outvars, sourceVars);
    }
  }
}
static void AddCBufferToGlobalState(const DXIL::Program *program, GlobalState &global,
                                    rdcarray<SourceVariableMapping> &sourceVars,
                                    const ShaderReflection &refl, const BindingSlot &slot,
                                    bytebuf &cbufData)
{
  // Find the identifier
  size_t numCBs = refl.constantBlocks.size();
  for(size_t i = 0; i < numCBs; ++i)
  {
    const ConstantBlock &cb = refl.constantBlocks[i];
    if(slot.registerSpace == (uint32_t)cb.fixedBindSetOrSpace &&
       slot.shaderRegister >= (uint32_t)cb.fixedBindNumber &&
       slot.shaderRegister < (uint32_t)(cb.fixedBindNumber + cb.bindArraySize))
    {
      uint32_t arrayIndex = slot.shaderRegister - cb.fixedBindNumber;

      rdcarray<ShaderVariable> &targetVars =
          cb.bindArraySize > 1 ? global.constantBlocks[i].members[arrayIndex].members
                               : global.constantBlocks[i].members;
      RDCASSERTMSG("Reassigning previously filled cbuffer", targetVars.empty());

      global.constantBlocks[i].name =
          Debugger::GetResourceReferenceName(program, ResourceClass::CBuffer, slot);

      SourceVariableMapping cbSourceMapping;
      cbSourceMapping.name = refl.constantBlocks[i].name;
      cbSourceMapping.variables.push_back(
          DebugVariableReference(DebugVariableType::Constant, global.constantBlocks[i].name));
      sourceVars.push_back(cbSourceMapping);

      rdcstr identifierPrefix = global.constantBlocks[i].name;
      rdcstr variablePrefix = refl.constantBlocks[i].name;
      if(cb.bindArraySize > 1)
      {
        identifierPrefix =
            StringFormat::Fmt("%s[%u]", global.constantBlocks[i].name.c_str(), arrayIndex);
        variablePrefix = StringFormat::Fmt("%s[%u]", refl.constantBlocks[i].name.c_str(), arrayIndex);

        // The above sourceVar is for the logical identifier, and FlattenVariables adds the
        // individual elements of the constant buffer. For CB arrays, add an extra source
        // var for the CB array index
        SourceVariableMapping cbArrayMapping;
        global.constantBlocks[i].members[arrayIndex].name = StringFormat::Fmt("[%u]", arrayIndex);
        cbArrayMapping.name = variablePrefix;
        cbArrayMapping.variables.push_back(
            DebugVariableReference(DebugVariableType::Constant, identifierPrefix));
        sourceVars.push_back(cbArrayMapping);
      }
      const rdcarray<ShaderConstant> &constants =
          (cb.bindArraySize > 1) ? refl.constantBlocks[i].variables[0].type.members
                                 : refl.constantBlocks[i].variables;

      rdcarray<ShaderVariable> vars;
      StandardFillCBufferVariables(refl.resourceId, constants, vars, cbufData);
      FlattenVariables(identifierPrefix, constants, vars, targetVars, variablePrefix + ".", 0,
                       sourceVars);
      for(size_t c = 0; c < targetVars.size(); c++)
        targetVars[c].name = StringFormat::Fmt("[%u]", (uint32_t)c);

      return;
    }
  }
}

void FetchConstantBufferData(WrappedID3D12Device *device, const DXIL::Program *program,
                             const D3D12RenderState::RootSignature &rootsig,
                             const ShaderReflection &refl, GlobalState &global,
                             rdcarray<SourceVariableMapping> &sourceVars)
{
  WrappedID3D12RootSignature *pD3D12RootSig =
      device->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootsig.rootsig);
  const DXBC::ShaderType shaderType = program->GetShaderType();

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
        AddCBufferToGlobalState(program, global, sourceVars, refl, slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && element.type == eRootCBV)
      {
        BindingSlot slot(rootSigParam.Descriptor.ShaderRegister,
                         rootSigParam.Descriptor.RegisterSpace);
        ID3D12Resource *cbv = device->GetResourceManager()->GetCurrentAs<ID3D12Resource>(element.id);
        bytebuf cbufData;
        device->GetDebugManager()->GetBufferData(cbv, element.offset, 0, cbufData);
        AddCBufferToGlobalState(program, global, sourceVars, refl, slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
              element.type == eRootTable)
      {
        UINT prevTableOffset = 0;
        WrappedID3D12DescriptorHeap *heap =
            device->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

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
                device->GetResourceManager()->GetCurrentAs<ID3D12Resource>(resId);
            cbufData.clear();

            if(cbv.SizeInBytes > 0)
              device->GetDebugManager()->GetBufferData(pCbvResource, byteOffset, cbv.SizeInBytes,
                                                       cbufData);
            AddCBufferToGlobalState(program, global, sourceVars, refl, slot, cbufData);

            desc++;
          }
        }
      }
    }
  }
}

InterpolationMode GetInterpolationModeForInputParam(const SigParameter &sig,
                                                    const rdcarray<SigParameter> &stageInputSig,
                                                    const DXIL::Program *program)
{
  if(sig.varType == VarType::SInt || sig.varType == VarType::UInt)
    return InterpolationMode::INTERPOLATION_CONSTANT;

  if(sig.varType == VarType::Float)
  {
    // if we're packed with ints on either side, we must be nointerpolation
    size_t numInputs = stageInputSig.size();
    for(size_t j = 0; j < numInputs; j++)
    {
      if(sig.regIndex == stageInputSig[j].regIndex && stageInputSig[j].varType != VarType::Float)
        return DXBC::InterpolationMode::INTERPOLATION_CONSTANT;
    }

    InterpolationMode interpolation = DXBC::InterpolationMode::INTERPOLATION_UNDEFINED;

    // TODO SEARCH THE DXIL PROGRAM INPUTS FOR THE INTERPOLATION MODE
#if 0
    if(program)
    {
      for(size_t d = 0; d < program->GetNumDeclarations(); d++)
      {
        const DXBCBytecode::Declaration &decl = program->GetDeclaration(d);

        if(decl.declaration == DXBCBytecode::OPCODE_DCL_INPUT_PS &&
           decl.operand.indices[0].absolute && decl.operand.indices[0].index == sig.regIndex)
        {
          interpolation = decl.inputOutput.inputInterpolation;
          break;
        }
      }
    }
#endif
    return interpolation;
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

D3D12APIWrapper::D3D12APIWrapper(WrappedID3D12Device *device,
                                 const DXBC::DXBCContainer *dxbcContainer, GlobalState &globalState,
                                 uint32_t eventId)
    : m_Device(device),
      m_DXBC(dxbcContainer),
      m_GlobalState(globalState),
      m_ShaderType(dxbcContainer->m_Type),
      m_EventId(eventId)
{
}

D3D12APIWrapper::~D3D12APIWrapper()
{
  // if we replayed to before the action for fetching some UAVs
  // replay back to after the action to keep the state consistent.
  if(m_DidReplay)
  {
    D3D12MarkerRegion region(m_Device->GetQueue()->GetReal(), "ResetReplay");
    // replay the action to get back to 'normal' state for this event
    m_Device->ReplayLog(0, m_EventId, eReplay_OnlyDraw);
  }
}

void D3D12APIWrapper::FetchSRV(const BindingSlot &slot)
{
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

  DXILDebug::GlobalState::SRVData &srvData = m_GlobalState.srvs[slot];

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

              // DXBC allows root buffers to have a stride of up to 16 bytes in the shader, which
              // means encoding the byte offset into the first element here is wrong without knowing
              // what the actual accessed stride is. Instead we only fetch the data from that offset
              // onwards.

              // Root buffers are typeless, try with the resource desc format
              // The debugger code will handle DXGI_FORMAT_UNKNOWN
              if(resDesc.Format == DXGI_FORMAT_UNKNOWN)
              {
                // If we didn't get a format from the resource, try to pull it from the
                // shader reflection info
                DXILDebug::LookupSRVFormatFromShaderReflection(*m_DXBC->GetReflection(), slot,
                                                               srvData.format);
              }
              else
              {
                DXILDebug::FillViewFmt(resDesc.Format, srvData.format);
              }
              srvData.firstElement = 0;
              // root arguments have no bounds checking, so use the most conservative number of
              // elements
              srvData.numElements = uint32_t(resDesc.Width - element.offset);

              if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                m_Device->GetDebugManager()->GetBufferData(pResource, element.offset, 0,
                                                           srvData.data);
            }
            return;
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
              if(desc)
              {
                ResourceId srvId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
                if(pResource)
                {
                  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc->GetSRV();
                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
                    srvDesc = MakeSRVDesc(pResource->GetDesc());

                  if(srvDesc.Format != DXGI_FORMAT_UNKNOWN)
                  {
                    DXILDebug::FillViewFmt(srvDesc.Format, srvData.format);
                  }
                  else
                  {
                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                      srvData.format.stride = srvDesc.Buffer.StructureByteStride;

                      // If we didn't get a type from the SRV description, try to pull it from the
                      // shader reflection info
                      DXILDebug::LookupSRVFormatFromShaderReflection(*m_DXBC->GetReflection(), slot,
                                                                     srvData.format);
                    }
                  }

                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
                  {
                    srvData.firstElement = (uint32_t)srvDesc.Buffer.FirstElement;
                    srvData.numElements = srvDesc.Buffer.NumElements;

                    m_Device->GetDebugManager()->GetBufferData(pResource, 0, 0, srvData.data);
                  }

                  // Textures are sampled via a pixel shader, so there's no need to copy their data
                }

                return;
              }
            }
          }
        }
      }
    }

    RDCERR("Couldn't find root signature parameter corresponding to SRV %u in space %u",
           slot.shaderRegister, slot.registerSpace);
    return;
  }

  RDCERR("No root signature bound, couldn't identify SRV %u in space %u", slot.shaderRegister,
         slot.registerSpace);
}

bool D3D12APIWrapper::IsSRVBound(const BindingSlot &slot)
{
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
            return pResource != NULL;
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
              if(desc)
              {
                ResourceId srvId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
                return pResource != NULL;
              }
            }
          }
        }
      }
    }
  }
  return false;
}

void D3D12APIWrapper::FetchUAV(const BindingSlot &slot)
{
  // if the UAV might be dirty from side-effects from the action, replay back to right
  // before it.
  if(!m_DidReplay)
  {
    D3D12MarkerRegion region(m_Device->GetQueue()->GetReal(), "un-dirtying resources");
    m_Device->ReplayLog(0, m_EventId, eReplay_WithoutDraw);
    m_DidReplay = true;
  }

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

  DXILDebug::GlobalState::UAVData &uavData = m_GlobalState.uavs[slot];

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
              // DXBC allows root buffers to have a stride of up to 16 bytes in the shader, which
              // means encoding the byte offset into the first element here is wrong without knowing
              // what the actual accessed stride is. Instead we only fetch the data from that offset
              // onwards.

              // Root buffers are typeless, try with the resource desc format
              // The debugger code will handle DXGI_FORMAT_UNKNOWN
              if(resDesc.Format == DXGI_FORMAT_UNKNOWN)
              {
                // If we didn't get a format from the resource, try to pull it from the
                // shader reflection info
                DXILDebug::LookupUAVFormatFromShaderReflection(*m_DXBC->GetReflection(), slot,
                                                               uavData.format);
              }
              else
              {
                DXILDebug::FillViewFmt(resDesc.Format, uavData.format);
              }
              uavData.firstElement = 0;
              // root arguments have no bounds checking, use the most conservative number of elements
              uavData.numElements = uint32_t(resDesc.Width - element.offset);

              if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                m_Device->GetDebugManager()->GetBufferData(pResource, element.offset, 0,
                                                           uavData.data);
            }

            return;
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
              if(desc)
              {
                ResourceId uavId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);

                if(pResource)
                {
                  // TODO: Need to fetch counter resource if applicable

                  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = desc->GetUAV();

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
                    uavDesc = MakeUAVDesc(pResource->GetDesc());

                  if(uavDesc.Format != DXGI_FORMAT_UNKNOWN)
                  {
                    DXILDebug::FillViewFmt(uavDesc.Format, uavData.format);
                  }
                  else
                  {
                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                      uavData.format.stride = uavDesc.Buffer.StructureByteStride;

                      // TODO: Try looking up UAV from shader reflection info?
                    }
                  }

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
                  {
                    uavData.firstElement = (uint32_t)uavDesc.Buffer.FirstElement;
                    uavData.numElements = uavDesc.Buffer.NumElements;

                    m_Device->GetDebugManager()->GetBufferData(pResource, 0, 0, uavData.data);
                  }
                  else
                  {
                    uavData.tex = true;
                    m_Device->GetReplay()->GetTextureData(uavId, Subresource(),
                                                          GetTextureDataParams(), uavData.data);

                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    uavData.rowPitch = GetByteSize((int)resDesc.Width, 1, 1, uavDesc.Format, 0);
                  }
                }

                return;
              }
            }
          }
        }
      }
    }

    RDCERR("Couldn't find root signature parameter corresponding to UAV %u in space %u",
           slot.shaderRegister, slot.registerSpace);
    return;
  }

  RDCERR("No root signature bound, couldn't identify UAV %u in space %u", slot.shaderRegister,
         slot.registerSpace);
}

bool D3D12APIWrapper::IsUAVBound(const BindingSlot &slot)
{
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
            return pResource != NULL;
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
              if(desc)
              {
                ResourceId uavId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);
                return pResource != NULL;
              }
            }
          }
        }
      }
    }
  }
  return false;
}

bool D3D12APIWrapper::CalculateMathIntrinsic(DXIL::DXOp dxOp, const ShaderVariable &input,
                                             ShaderVariable &output)
{
  D3D12MarkerRegion region(m_Device->GetQueue()->GetReal(), "CalculateMathIntrinsic");

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

  ShaderVariable ignored;
  return D3D12ShaderDebug::CalculateMathIntrinsic(true, m_Device, mathOp, input, output, ignored);
}

bool D3D12APIWrapper::CalculateSampleGather(
    DXIL::DXOp dxOp, SampleGatherResourceData resourceData, SampleGatherSamplerData samplerData,
    const ShaderVariable &uv, const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
    const int8_t texelOffsets[3], int multisampleIndex, float lodOrCompareValue,
    const uint8_t swizzle[4], GatherChannel gatherChannel, DXBC::ShaderType shaderType,
    uint32_t instructionIdx, const char *opString, ShaderVariable &output)
{
  int sampleOp;
  switch(dxOp)
  {
    case DXOp::Sample: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE; break;
    case DXOp::SampleLevel: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_LEVEL; break;
    case DXOp::SampleBias: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_BIAS; break;
    case DXOp::SampleCmp: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP; break;
    case DXOp::SampleGrad: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_GRAD; break;
    case DXOp::SampleCmpLevelZero: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO; break;
    case DXOp::TextureGather: sampleOp = DEBUG_SAMPLE_TEX_GATHER4; break;
    case DXOp::TextureGatherCmp: sampleOp = DEBUG_SAMPLE_TEX_GATHER4_CMP; break;
    case DXOp::CalculateLOD: sampleOp = DEBUG_SAMPLE_TEX_LOD; break;
    case DXOp::TextureLoad: sampleOp = DEBUG_SAMPLE_TEX_LOAD; break;
    // TODO: consider these DXIL opcode operations
    // DXOp::SampleCmpBias
    // DXOp::SampleCmpGrad
    // DXOp::SampleCmpLevel
    // DXOp::TextureGatherRaw
    // TODO: consider these DXBC opcode operations
    // DEBUG_SAMPLE_TEX_GATHER4_PARAM_OFFSET_CMP
    // DEBUG_SAMPLE_TEX_LOAD_MS
    default:
      // To support a new instruction, the shader created in
      // D3D12DebugManager::CreateShaderDebugResources will need updating
      RDCERR("Unsupported instruction for CalculateSampleGather: %s %u", ToStr(dxOp).c_str(), dxOp);
      return false;
  }

  return D3D12ShaderDebug::CalculateSampleGather(
      true, m_Device, sampleOp, resourceData, samplerData, uv, ddxCalc, ddyCalc, texelOffsets,
      multisampleIndex, lodOrCompareValue, swizzle, gatherChannel, shaderType, instructionIdx,
      opString, output);
}

ShaderVariable D3D12APIWrapper::GetResourceInfo(DXIL::ResourceClass resClass,
                                                const DXDebug::BindingSlot &slot, uint32_t mipLevel,
                                                const DXBC::ShaderType shaderType, int &dim)
{
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
  return D3D12ShaderDebug::GetResourceInfo(m_Device, descType, slot, mipLevel, shaderType, dim, true);
}

ShaderVariable D3D12APIWrapper::GetSampleInfo(DXIL::ResourceClass resClass,
                                              const DXDebug::BindingSlot &slot,
                                              const DXBC::ShaderType shaderType, const char *opString)
{
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
  return D3D12ShaderDebug::GetSampleInfo(m_Device, descType, slot, shaderType, opString);
}

ShaderVariable D3D12APIWrapper::GetRenderTargetSampleInfo(const DXBC::ShaderType shaderType,
                                                          const char *opString)
{
  return D3D12ShaderDebug::GetRenderTargetSampleInfo(m_Device, shaderType, opString);
}

bool D3D12APIWrapper::IsResourceBound(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot)
{
  if(resClass == ResourceClass::SRV)
  {
    GlobalState::SRVIterator srvIter = m_GlobalState.srvs.find(slot);
    if(srvIter != m_GlobalState.srvs.end())
      return true;
    return IsSRVBound(slot);
  }
  else if(resClass == ResourceClass::UAV)
  {
    GlobalState::UAVIterator uavIter = m_GlobalState.uavs.find(slot);
    if(uavIter != m_GlobalState.uavs.end())
      return true;
    return IsUAVBound(slot);
  }
  else
  {
    RDCERR("Unhanded resource class %s", ToStr(resClass).c_str());
  }
  return false;
}

};
