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

#include "dxbc_bytecode.h"
#include "common/formatting.h"
#include "os/os_specific.h"
#include "dxbc_bytecode_ops.h"

static ShaderVariable makeReg(rdcstr name)
{
  ShaderVariable ret(name, 0U, 0U, 0U, 0U);
  ret.type = VarType::Unknown;
  return ret;
}

namespace DXBCBytecode
{
Program::Program(const byte *bytes, size_t length)
{
  RDCASSERT((length % 4) == 0);
  m_ProgramWords.resize(length / 4);
  memcpy(m_ProgramWords.data(), bytes, length);

  if(m_ProgramWords.empty())
    return;

  uint32_t *begin = &m_ProgramWords.front();
  uint32_t *cur = begin;

  m_Type = VersionToken::ProgramType.Get(cur[0]);
  m_Major = VersionToken::MajorVersion.Get(cur[0]);
  m_Minor = VersionToken::MinorVersion.Get(cur[0]);
}

Program::Program(const rdcarray<uint32_t> &words)
{
  m_ProgramWords = words;

  if(m_ProgramWords.empty())
    return;

  uint32_t *begin = &m_ProgramWords.front();
  uint32_t *cur = begin;

  m_Type = VersionToken::ProgramType.Get(cur[0]);
  m_Major = VersionToken::MajorVersion.Get(cur[0]);
  m_Minor = VersionToken::MinorVersion.Get(cur[0]);
}

void HandleResourceArrayIndices(const rdcarray<DXBCBytecode::RegIndex> &indices,
                                DXBC::ShaderInputBind &desc)
{
  // If there are 3 indices, we're using SM5.1 and this binding may be a resource array
  if(indices.size() == 3)
  {
    // With SM5.1, the first index is the logical identifier,
    // and the 2nd index is the starting shader register
    desc.reg = (uint32_t)indices[1].index;

    // Start/end registers are inclusive, so one resource will have the same start/end register
    desc.bindCount = uint32_t(indices[2].index - indices[1].index + 1);

    // If it's an unbounded resource array, mark the bind count as ~0U
    if(indices[2].index == 0xffffffff)
      desc.bindCount = ~0U;
  }
}

bool Program::UsesExtensionUAV(uint32_t slot, uint32_t space, const byte *bytes, size_t length)
{
  uint32_t *begin = (uint32_t *)bytes;
  uint32_t *cur = begin;
  uint32_t *end = begin + (length / sizeof(uint32_t));

  const bool sm51 = (VersionToken::MajorVersion.Get(cur[0]) == 0x5 &&
                     VersionToken::MinorVersion.Get(cur[0]) == 0x1);

  if(sm51 && space == ~0U)
    return false;

  // skip version and length
  cur += 2;

  while(cur < end)
  {
    uint32_t OpcodeToken0 = cur[0];

    OpcodeType op = Opcode::Type.Get(OpcodeToken0);

    // nvidia is a structured buffer with counter
    // AMD is a RW byte address buffer
    if((op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED &&
        Decl::HasOrderPreservingCounter.Get(OpcodeToken0)) ||
       op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW)
    {
      uint32_t *tokenStream = cur;

      // skip opcode and length
      tokenStream++;

      uint32_t indexDim = Oper::IndexDimension.Get(tokenStream[0]);
      OperandIndexType idx0Type = Oper::Index0.Get(tokenStream[0]);
      OperandIndexType idx1Type = Oper::Index1.Get(tokenStream[0]);
      OperandIndexType idx2Type = Oper::Index2.Get(tokenStream[0]);

      // expect only one immediate index for the operand on SM <= 5.0, and three immediate indices
      // on SM5.1
      if((indexDim == 1 && idx0Type == INDEX_IMMEDIATE32) ||
         (indexDim == 3 && idx0Type == INDEX_IMMEDIATE32 && idx1Type == INDEX_IMMEDIATE32 &&
          idx2Type == INDEX_IMMEDIATE32))
      {
        bool extended = Oper::Extended.Get(tokenStream[0]);

        tokenStream++;

        while(extended)
        {
          extended = ExtendedOperand::Extended.Get(tokenStream[0]) == 1;

          tokenStream++;
        }

        uint32_t opreg = tokenStream[0];
        tokenStream++;

        // on 5.1 opreg is just the identifier which means nothing, the binding comes next as a
        // range, like U1[7:7] is bound to slot 7
        if(indexDim == 3)
        {
          uint32_t lower = tokenStream[0];
          uint32_t upper = tokenStream[1];
          tokenStream += 2;

          // the magic UAV should be lower == upper. If that isn't the case, don't match this even
          // if the range includes our target register
          if(lower == upper)
            opreg = lower;
          else
            opreg = ~0U;
        }

        if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED)
        {
          // stride
          tokenStream++;
        }

        if(sm51)
        {
          uint32_t opspace = tokenStream[0];

          if(space == opspace && slot == opreg)
            return true;
        }
        else
        {
          if(slot == opreg)
            return true;
        }
      }
    }

    if(op == OPCODE_CUSTOMDATA)
    {
      // length in opcode token is 0, full length is in second dword
      cur += cur[1];
    }
    else
    {
      cur += Opcode::Length.Get(OpcodeToken0);
    }
  }

  return false;
}

DXBC::Reflection *Program::GuessReflection()
{
  DecodeProgram();

  // we don't store this, since it's just a guess. If our m_Reflection is NULL that indicates no
  // useful reflection is present
  DXBC::Reflection *ret = new DXBC::Reflection;

  for(size_t i = 0; i < m_Declarations.size(); i++)
  {
    Declaration &dcl = m_Declarations[i];

    switch(dcl.declaration)
    {
      case OPCODE_DCL_SAMPLER:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_SAMPLER);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("sampler%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_SAMPLER;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.retType = DXBC::RETURN_TYPE_UNKNOWN;
        desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN;
        desc.numComps = 0;

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        ret->Samplers.push_back(desc);

        break;
      }
      case OPCODE_DCL_RESOURCE:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_RESOURCE);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("texture%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_TEXTURE;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.retType = dcl.resource.resType[0];

        switch(dcl.resource.dim)
        {
          case RESOURCE_DIMENSION_BUFFER: desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER; break;
          case RESOURCE_DIMENSION_TEXTURE1D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1D;
            break;
          case RESOURCE_DIMENSION_TEXTURE2D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2D;
            break;
          case RESOURCE_DIMENSION_TEXTURE3D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE3D;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBE:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBE;
            break;
          case RESOURCE_DIMENSION_TEXTURE1DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMS:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMS;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY;
            break;
          default: desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN; break;
        }
        // can't tell, fxc seems to default to 4
        desc.numComps = 4;

        RDCASSERT(desc.dimension != DXBC::ShaderInputBind::DIM_UNKNOWN);

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        ret->SRVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
      case OPCODE_DCL_RESOURCE_RAW:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_RESOURCE ||
                  dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name =
            StringFormat::Fmt("%sbytebuffer%u", dcl.operand.type != TYPE_RESOURCE ? "rw" : "", idx);
        desc.type = dcl.operand.type == TYPE_RESOURCE
                        ? DXBC::ShaderInputBind::TYPE_BYTEADDRESS
                        : DXBC::ShaderInputBind::TYPE_UAV_RWBYTEADDRESS;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.retType = DXBC::RETURN_TYPE_MIXED;
        desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER;
        desc.numComps = 0;

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        if(dcl.operand.type == TYPE_RESOURCE)
          ret->SRVs.push_back(desc);
        else
          ret->UAVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_RESOURCE_STRUCTURED:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_RESOURCE);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("structuredbuffer%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_STRUCTURED;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.retType = DXBC::RETURN_TYPE_MIXED;
        desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER;
        desc.numComps = dcl.structured.stride;

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        ret->SRVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("uav%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED;    // doesn't seem to be anything
                                                                     // that determines append vs
                                                                     // consume vs rwstructured
        if(dcl.structured.hasCounter)
          desc.type = DXBC::ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.retType = DXBC::RETURN_TYPE_MIXED;
        desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER;
        desc.numComps = dcl.structured.stride;

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        ret->UAVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_UNORDERED_ACCESS_VIEW);
        RDCASSERT(dcl.operand.indices.size() == 1 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute);

        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;

        desc.name = StringFormat::Fmt("uav%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_UAV_RWTYPED;
        desc.space = dcl.space;
        desc.reg = idx;
        desc.bindCount = 1;
        desc.retType = dcl.uav_typed.resType[0];

        switch(dcl.uav_typed.dim)
        {
          case RESOURCE_DIMENSION_BUFFER: desc.dimension = DXBC::ShaderInputBind::DIM_BUFFER; break;
          case RESOURCE_DIMENSION_TEXTURE1D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1D;
            break;
          case RESOURCE_DIMENSION_TEXTURE2D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2D;
            break;
          case RESOURCE_DIMENSION_TEXTURE3D:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE3D;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBE:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBE;
            break;
          case RESOURCE_DIMENSION_TEXTURE1DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE1DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURECUBEARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURECUBEARRAY;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMS:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMS;
            break;
          case RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
            desc.dimension = DXBC::ShaderInputBind::DIM_TEXTURE2DMSARRAY;
            break;
          default: desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN; break;
        }
        desc.numComps = 4;

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        ret->UAVs.push_back(desc);

        break;
      }
      case OPCODE_DCL_CONSTANT_BUFFER:
      {
        DXBC::ShaderInputBind desc;

        RDCASSERT(dcl.operand.type == TYPE_CONSTANT_BUFFER);
        RDCASSERT(dcl.operand.indices.size() == 2 || dcl.operand.indices.size() == 3);
        RDCASSERT(dcl.operand.indices[0].absolute && dcl.operand.indices[1].absolute);

        // Constant buffer declarations differ between SM5 and SM5.1.  For SM5.1, the indices are
        // logical identifier, start shader register, and end shader register. Register space and
        // buffer size are stored elsewhere in the declaration. For SM5 and earlier, the indices
        // are the shader register and buffer size (measured in float4's)
        bool isShaderModel51 = IsShaderModel51();
        uint32_t idx = (uint32_t)dcl.operand.indices[0].index;
        uint32_t reg = isShaderModel51 ? (uint32_t)dcl.operand.indices[1].index : idx;
        uint32_t numVecs =
            isShaderModel51 ? dcl.cbuffer.vectorSize : (uint32_t)dcl.operand.indices[1].index;

        desc.name = StringFormat::Fmt("cbuffer%u", idx);
        desc.type = DXBC::ShaderInputBind::TYPE_CBUFFER;
        desc.space = dcl.space;
        desc.reg = reg;
        desc.bindCount = 1;
        desc.retType = DXBC::RETURN_TYPE_UNKNOWN;
        desc.dimension = DXBC::ShaderInputBind::DIM_UNKNOWN;
        desc.numComps = 0;

        HandleResourceArrayIndices(dcl.operand.indices, desc);

        DXBC::CBuffer cb;

        cb.name = desc.name;

        cb.identifier = idx;
        cb.space = dcl.space;
        cb.reg = reg;
        cb.bindCount = desc.bindCount;

        cb.descriptor.byteSize = numVecs * 4 * sizeof(float);
        cb.descriptor.type = DXBC::CBuffer::Descriptor::TYPE_CBUFFER;

        bool isArray = desc.bindCount > 1;
        if(isArray)
        {
          // If the constant buffer is an array, then we need to add an entry for the struct
          // itself. This mimics what is loaded for a constant buffer array when reflection
          // information is not stripped.
          DXBC::CBufferVariable var;
          var.name = cb.name;
          var.offset = 0;
          var.type.varClass = DXBC::VariableClass::CLASS_STRUCT;
          var.type.varType = VarType::Unknown;
          var.type.rows = 1;
          var.type.cols = 4;
          var.type.elements = 1;
          var.type.bytesize = 4 * sizeof(float);
          var.type.name = "struct";
          cb.variables.push_back(var);
        }
        rdcarray<DXBC::CBufferVariable> &fillVars =
            isArray ? cb.variables[0].type.members : cb.variables;
        fillVars.reserve(numVecs);

        for(uint32_t v = 0; v < numVecs; v++)
        {
          DXBC::CBufferVariable var;

          var.name = StringFormat::Fmt("cb%u_v%u", cb.identifier, v);

          var.offset = 4 * sizeof(float) * v;

          var.type.bytesize = 4 * sizeof(float);
          var.type.rows = 1;
          var.type.cols = 4;
          var.type.elements = 0;
          var.type.varType = VarType::Float;
          var.type.varClass = DXBC::CLASS_VECTOR;
          var.type.name = TypeName(var.type);

          fillVars.push_back(var);
        }

        ret->CBuffers.push_back(cb);

        break;
      }
      default: break;
    }
  }

  return ret;
}

rdcstr Program::GetDebugStatus()
{
  // if there are no vendor extensions this is always debuggable
  if(m_ShaderExt.second == ~0U)
    return rdcstr();

  // otherwise we need to check that no unsupported vendor extensions are used
  DecodeProgram();

  for(const Operation &op : m_Instructions)
  {
    if(op.operation >= OPCODE_VENDOR_FIRST)
    {
      bool supported = false;

      // whitelist supported instructions here
      switch(op.operation)
      {
        case OPCODE_AMD_U64_ATOMIC:
        case OPCODE_NV_U64_ATOMIC: supported = true; break;
        default: break;
      }

      if(!supported)
        return StringFormat::Fmt("Unsupported shader extension '%s' used",
                                 ToStr(op.operation).c_str());
    }
  }

  // no unsupported instructions used
  return rdcstr();
}

D3D_PRIMITIVE_TOPOLOGY Program::GetOutputTopology()
{
  DecodeProgram();

  if(m_Type != DXBC::ShaderType::Geometry && m_Type != DXBC::ShaderType::Domain)
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  for(const DXBCBytecode::Declaration &decl : m_Declarations)
  {
    if(decl.declaration == DXBCBytecode::OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
      return decl.geomOutputTopology;
    if(decl.declaration == DXBCBytecode::OPCODE_DCL_TESS_DOMAIN)
    {
      if(decl.tessDomain == DXBCBytecode::DOMAIN_ISOLINE)
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
      else
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      break;
    }
  }

  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

D3D_PRIMITIVE_TOPOLOGY Program::GetOutputTopology(const byte *bytes, size_t length)
{
  uint32_t *begin = (uint32_t *)bytes;
  uint32_t *cur = begin;
  uint32_t *end = begin + (length / sizeof(uint32_t));

  // skip version and length
  cur += 2;

  while(cur < end)
  {
    uint32_t OpcodeToken0 = cur[0];

    OpcodeType op = Opcode::Type.Get(OpcodeToken0);

    // nvidia is a structured buffer with counter
    // AMD is a RW byte address buffer
    if(op == OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
    {
      uint32_t *tokenStream = cur;

      // skip opcode and length
      tokenStream++;

      return Decl::OutputPrimitiveTopology.Get(tokenStream[0]);
    }

    if(op == OPCODE_CUSTOMDATA)
    {
      // length in opcode token is 0, full length is in second dword
      cur += cur[1];
    }
    else
    {
      cur += Opcode::Length.Get(OpcodeToken0);
    }
  }

  return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

void Program::SetupRegisterFile(rdcarray<ShaderVariable> &registers) const
{
  size_t numRegisters = m_NumTemps + m_IndexTempSizes.size() + m_NumOutputs;

  if(m_OutputDepth)
    numRegisters++;
  if(m_OutputStencil)
    numRegisters++;
  if(m_OutputCoverage)
    numRegisters++;

  registers.reserve(numRegisters);

  for(uint32_t i = 0; i < m_NumTemps; i++)
    registers.push_back(makeReg(GetRegisterName(TYPE_TEMP, i)));

  for(size_t i = 0; i < m_IndexTempSizes.size(); i++)
  {
    registers.push_back(makeReg(GetRegisterName(TYPE_INDEXABLE_TEMP, (uint32_t)i)));
    registers.back().members.resize(m_IndexTempSizes[i]);
    for(uint32_t t = 0; t < m_IndexTempSizes[i]; t++)
      registers.back().members[t] = makeReg(StringFormat::Fmt("[%u]", t));
  }

  for(uint32_t i = 0; i < m_NumOutputs; i++)
    registers.push_back(makeReg(rdcstr()));

  // this could be oDepthGE or oDepthLE, that will be fixed up when the external code sets up the
  // names etc of all outputs with reflection info
  if(m_OutputDepth)
    registers.push_back(makeReg(rdcstr()));
  if(m_OutputStencil)
    registers.push_back(makeReg(rdcstr()));
  if(m_OutputCoverage)
    registers.push_back(makeReg(rdcstr()));
}

const Declaration *Program::FindDeclaration(OperandType declType, uint32_t identifier) const
{
  // Given a declType and identifier (together defining a binding such as t0, s1, etc.),
  // return the matching declaration if it exists. The logic for this is the same for all
  // shader model versions.
  size_t numDeclarations = m_Declarations.size();
  for(size_t i = 0; i < numDeclarations; ++i)
  {
    const Declaration &decl = m_Declarations[i];
    if(decl.operand.type == declType)
    {
      if(decl.operand.indices[0].index == identifier)
        return &decl;
    }
  }

  return NULL;
}

uint32_t Program::GetRegisterIndex(OperandType type, uint32_t index) const
{
  if(type == TYPE_TEMP)
  {
    RDCASSERT(index < m_NumTemps, index, m_NumTemps);
    return index;
  }
  else if(type == TYPE_INDEXABLE_TEMP)
  {
    RDCASSERT(index < m_IndexTempSizes.size(), index, m_IndexTempSizes.size());
    return m_NumTemps + index;
  }
  else if(type == TYPE_OUTPUT)
  {
    RDCASSERT(index < m_NumOutputs, index, m_NumOutputs);
    return m_NumTemps + (uint32_t)m_IndexTempSizes.size() + index;
  }
  else if(type == TYPE_OUTPUT_DEPTH)
  {
    RDCASSERT(m_OutputDepth);
    return m_NumTemps + (uint32_t)m_IndexTempSizes.size() + m_NumOutputs;
  }
  else if(type == TYPE_OUTPUT_STENCIL_REF)
  {
    RDCASSERT(m_OutputStencil);
    return m_NumTemps + (uint32_t)m_IndexTempSizes.size() + m_NumOutputs + (m_OutputDepth ? 1 : 0);
  }
  else if(type == TYPE_OUTPUT_COVERAGE_MASK)
  {
    RDCASSERT(m_OutputCoverage);
    return m_NumTemps + (uint32_t)m_IndexTempSizes.size() + m_NumOutputs + (m_OutputDepth ? 1 : 0) +
           (m_OutputStencil ? 1 : 0);
  }

  RDCERR("Unexpected type for register index: %s", ToStr(type).c_str());

  return ~0U;
}

rdcstr Program::GetRegisterName(OperandType oper, uint32_t index) const
{
  if(oper == TYPE_TEMP)
    return StringFormat::Fmt("r%u", index);
  else if(oper == TYPE_INDEXABLE_TEMP)
    return StringFormat::Fmt("x%u", index);
  else if(oper == TYPE_INPUT)
    return StringFormat::Fmt("v%u", index);
  else if(oper == TYPE_CONSTANT_BUFFER)
    return StringFormat::Fmt("%s%u", IsShaderModel51() ? "CB" : "cb", index);
  else if(oper == TYPE_OUTPUT)
    return StringFormat::Fmt("o%u", index);
  else if(oper == TYPE_OUTPUT_DEPTH)
    return "oDepth";
  else if(oper == TYPE_OUTPUT_DEPTH_LESS_EQUAL)
    return "oDepthLessEqual";
  else if(oper == TYPE_OUTPUT_DEPTH_GREATER_EQUAL)
    return "oDepthGreaterEqual";
  else if(oper == TYPE_OUTPUT_COVERAGE_MASK)
    return "oMask";
  else if(oper == TYPE_OUTPUT_STENCIL_REF)
    return "oStencilRef";
  else if(oper == TYPE_OUTPUT_CONTROL_POINT_ID)
    return "vOutputControlPointID";
  else if(oper == TYPE_INPUT_DOMAIN_POINT)
    return "vDomain";
  else if(oper == TYPE_INPUT_PRIMITIVEID)
    return "vPrim";
  else if(oper == TYPE_INPUT_COVERAGE_MASK)
    return "vCoverageMask";
  else if(oper == TYPE_INPUT_GS_INSTANCE_ID)
    return "vGSInstanceID";
  else if(oper == TYPE_INPUT_THREAD_ID)
    return "vThreadID";
  else if(oper == TYPE_INPUT_THREAD_GROUP_ID)
    return "vThreadGroupID";
  else if(oper == TYPE_INPUT_THREAD_ID_IN_GROUP)
    return "vThreadIDInGroup";
  else if(oper == TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED)
    return "vThreadIDInGroupFlattened";
  else if(oper == TYPE_INPUT_FORK_INSTANCE_ID)
    return "vForkInstanceID";
  else if(oper == TYPE_INPUT_JOIN_INSTANCE_ID)
    return "vJoinInstanceID";

  RDCERR("Unknown register requiring name: %s", ToStr(oper).c_str());
  return "??";
}

// see http://msdn.microsoft.com/en-us/library/windows/desktop/bb219840(v=vs.85).aspx
// for details of these opcodes
size_t NumOperands(OpcodeType op)
{
  switch(op)
  {
    case OPCODE_BREAK:
    case OPCODE_CONTINUE:
    case OPCODE_CUT:
    case OPCODE_DEFAULT:
    case OPCODE_ELSE:
    case OPCODE_EMIT:
    case OPCODE_EMITTHENCUT:
    case OPCODE_ENDIF:
    case OPCODE_ENDLOOP:
    case OPCODE_ENDSWITCH:
    case OPCODE_LOOP:
    case OPCODE_NOP:
    case OPCODE_RET:
    case OPCODE_SYNC:
    case OPCODE_ABORT:
    case OPCODE_DEBUGBREAK:

    case OPCODE_HS_CONTROL_POINT_PHASE:
    case OPCODE_HS_FORK_PHASE:
    case OPCODE_HS_JOIN_PHASE:
    case OPCODE_HS_DECLS: return 0;
    case OPCODE_BREAKC:
    case OPCODE_CONTINUEC:
    case OPCODE_CALL:
    case OPCODE_CASE:
    case OPCODE_CUT_STREAM:
    case OPCODE_DISCARD:
    case OPCODE_EMIT_STREAM:
    case OPCODE_EMITTHENCUT_STREAM:
    case OPCODE_IF:
    case OPCODE_INTERFACE_CALL:
    case OPCODE_LABEL:
    case OPCODE_RETC:
    case OPCODE_SWITCH: return 1;
    case OPCODE_BFREV:
    case OPCODE_BUFINFO:
    case OPCODE_CALLC:
    case OPCODE_COUNTBITS:
    case OPCODE_DERIV_RTX:
    case OPCODE_DERIV_RTY:
    case OPCODE_DERIV_RTX_COARSE:
    case OPCODE_DERIV_RTX_FINE:
    case OPCODE_DERIV_RTY_COARSE:
    case OPCODE_DERIV_RTY_FINE:
    case OPCODE_DMOV:
    case OPCODE_DTOF:
    case OPCODE_EXP:
    case OPCODE_F32TOF16:
    case OPCODE_F16TOF32:
    case OPCODE_FIRSTBIT_HI:
    case OPCODE_FIRSTBIT_LO:
    case OPCODE_FIRSTBIT_SHI:
    case OPCODE_FRC:
    case OPCODE_FTOD:
    case OPCODE_FTOI:
    case OPCODE_FTOU:
    case OPCODE_IMM_ATOMIC_ALLOC:
    case OPCODE_IMM_ATOMIC_CONSUME:
    case OPCODE_INEG:
    case OPCODE_ITOF:
    case OPCODE_LOG:
    case OPCODE_MOV:
    case OPCODE_NOT:
    case OPCODE_RCP:
    case OPCODE_ROUND_NE:
    case OPCODE_ROUND_NI:
    case OPCODE_ROUND_PI:
    case OPCODE_ROUND_Z:
    case OPCODE_RSQ:
    case OPCODE_SAMPLE_INFO:
    case OPCODE_SQRT:
    case OPCODE_UTOF:
    case OPCODE_EVAL_CENTROID:
    case OPCODE_DRCP:
    case OPCODE_DTOI:
    case OPCODE_DTOU:
    case OPCODE_ITOD:
    case OPCODE_UTOD:
    case OPCODE_CHECK_ACCESS_FULLY_MAPPED: return 2;
    case OPCODE_AND:
    case OPCODE_ADD:
    case OPCODE_ATOMIC_AND:
    case OPCODE_ATOMIC_OR:
    case OPCODE_ATOMIC_XOR:
    case OPCODE_ATOMIC_IADD:
    case OPCODE_ATOMIC_IMAX:
    case OPCODE_ATOMIC_IMIN:
    case OPCODE_ATOMIC_UMAX:
    case OPCODE_ATOMIC_UMIN:
    case OPCODE_DADD:
    case OPCODE_DIV:
    case OPCODE_DP2:
    case OPCODE_DP3:
    case OPCODE_DP4:
    case OPCODE_DEQ:
    case OPCODE_DGE:
    case OPCODE_DLT:
    case OPCODE_DMAX:
    case OPCODE_DMIN:
    case OPCODE_DMUL:
    case OPCODE_DNE:
    case OPCODE_EQ:
    case OPCODE_GE:
    case OPCODE_IADD:
    case OPCODE_IEQ:
    case OPCODE_IGE:
    case OPCODE_ILT:
    case OPCODE_IMAX:
    case OPCODE_IMIN:
    case OPCODE_INE:
    case OPCODE_ISHL:
    case OPCODE_ISHR:
    case OPCODE_LD:
    case OPCODE_LD_RAW:
    case OPCODE_LD_UAV_TYPED:
    case OPCODE_LT:
    case OPCODE_MAX:
    case OPCODE_MIN:
    case OPCODE_MUL:
    case OPCODE_NE:
    case OPCODE_OR:
    case OPCODE_RESINFO:
    case OPCODE_SAMPLE_POS:
    case OPCODE_SINCOS:
    case OPCODE_STORE_RAW:
    case OPCODE_STORE_UAV_TYPED:
    case OPCODE_UGE:
    case OPCODE_ULT:
    case OPCODE_UMAX:
    case OPCODE_UMIN:
    case OPCODE_USHR:
    case OPCODE_XOR:
    case OPCODE_EVAL_SNAPPED:
    case OPCODE_EVAL_SAMPLE_INDEX:
    case OPCODE_DDIV: return 3;
    case OPCODE_ATOMIC_CMP_STORE:
    case OPCODE_DMOVC:
    case OPCODE_GATHER4:
    case OPCODE_IBFE:
    case OPCODE_IMAD:
    case OPCODE_IMM_ATOMIC_IADD:
    case OPCODE_IMM_ATOMIC_AND:
    case OPCODE_IMM_ATOMIC_OR:
    case OPCODE_IMM_ATOMIC_XOR:
    case OPCODE_IMM_ATOMIC_EXCH:
    case OPCODE_IMM_ATOMIC_IMAX:
    case OPCODE_IMM_ATOMIC_IMIN:
    case OPCODE_IMM_ATOMIC_UMAX:
    case OPCODE_IMM_ATOMIC_UMIN:
    case OPCODE_IMUL:
    case OPCODE_LD_MS:
    case OPCODE_LD_STRUCTURED:
    case OPCODE_LOD:
    case OPCODE_MAD:
    case OPCODE_MOVC:
    case OPCODE_SAMPLE:
    case OPCODE_STORE_STRUCTURED:
    case OPCODE_UADDC:
    case OPCODE_UBFE:
    case OPCODE_UDIV:
    case OPCODE_UMAD:
    case OPCODE_UMUL:
    case OPCODE_USUBB:
    case OPCODE_DFMA:
    case OPCODE_MSAD:
    case OPCODE_LD_FEEDBACK:
    case OPCODE_LD_RAW_FEEDBACK:
    case OPCODE_LD_UAV_TYPED_FEEDBACK: return 4;
    case OPCODE_BFI:
    case OPCODE_GATHER4_C:
    case OPCODE_GATHER4_PO:
    case OPCODE_IMM_ATOMIC_CMP_EXCH:
    case OPCODE_SAMPLE_C:
    case OPCODE_SAMPLE_C_LZ:
    case OPCODE_SAMPLE_L:
    case OPCODE_SAMPLE_B:
    case OPCODE_SWAPC:
    case OPCODE_GATHER4_FEEDBACK:
    case OPCODE_LD_MS_FEEDBACK:
    case OPCODE_LD_STRUCTURED_FEEDBACK: return 5;
    case OPCODE_GATHER4_PO_C:
    case OPCODE_SAMPLE_D:
    case OPCODE_SAMPLE_CLAMP_FEEDBACK:
    case OPCODE_SAMPLE_C_CLAMP_FEEDBACK:
    case OPCODE_SAMPLE_C_LZ_FEEDBACK:
    case OPCODE_SAMPLE_L_FEEDBACK:
    case OPCODE_SAMPLE_B_CLAMP_FEEDBACK:
    case OPCODE_GATHER4_C_FEEDBACK:
    case OPCODE_GATHER4_PO_FEEDBACK: return 6;
    case OPCODE_SAMPLE_D_CLAMP_FEEDBACK:
    case OPCODE_GATHER4_PO_C_FEEDBACK: return 7;

    // custom data doesn't have particular operands
    case OPCODE_CUSTOMDATA:
    default: break;
  }

  RDCERR("Unknown opcode: %u", op);
  return 0xffffffff;
}

bool IsDeclaration(OpcodeType op)
{
  // isDecl means not a real instruction, just a declaration type token
  bool isDecl = false;
  isDecl = isDecl || (op >= OPCODE_DCL_RESOURCE && op <= OPCODE_DCL_GLOBAL_FLAGS);
  isDecl = isDecl || (op >= OPCODE_DCL_STREAM && op <= OPCODE_DCL_RESOURCE_STRUCTURED);
  isDecl = isDecl || (op == OPCODE_DCL_GS_INSTANCE_COUNT);
  isDecl = isDecl || (op == OPCODE_HS_DECLS);
  isDecl = isDecl || (op == OPCODE_CUSTOMDATA);

  return isDecl;
}

bool IsInput(OperandType oper)
{
  switch(oper)
  {
    case TYPE_INPUT:
    case TYPE_INPUT_PRIMITIVEID:
    case TYPE_INPUT_FORK_INSTANCE_ID:
    case TYPE_INPUT_JOIN_INSTANCE_ID:
    case TYPE_INPUT_CONTROL_POINT:
    // this is an input, yes it's confusing.
    case TYPE_OUTPUT_CONTROL_POINT_ID:
    case TYPE_INPUT_PATCH_CONSTANT:
    case TYPE_INPUT_DOMAIN_POINT:
    case TYPE_INPUT_THREAD_ID:
    case TYPE_INPUT_THREAD_GROUP_ID:
    case TYPE_INPUT_THREAD_ID_IN_GROUP:
    case TYPE_INPUT_COVERAGE_MASK:
    case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
    case TYPE_INPUT_GS_INSTANCE_ID: return true;
    default: break;
  }
  return false;
}

bool IsOutput(OperandType oper)
{
  switch(oper)
  {
    case TYPE_OUTPUT:
    case TYPE_OUTPUT_DEPTH:
    case TYPE_OUTPUT_COVERAGE_MASK:
    case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
    case TYPE_OUTPUT_DEPTH_LESS_EQUAL:
    case TYPE_OUTPUT_STENCIL_REF: return true;
    default: break;
  }
  return false;
}

};    // namespace DXBCBytecode
