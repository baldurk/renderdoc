/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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
#include <math.h>
#include "common/common.h"
#include "core/core.h"
#include "core/settings.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "dxbc_bytecode_ops.h"

#include "dxbc_container.h"

RDOC_CONFIG(bool, DXBC_Disassembly_FriendlyNaming, true,
            "Where possible (i.e. it is completely unambiguous) replace register names with "
            "high-level variable names.");
RDOC_CONFIG(bool, DXBC_Disassembly_ProcessVendorShaderExts, true,
            "Process vendor shader extensions from magic UAV encoded instructions into the real "
            "operations.\n"
            "If this is disabled, shader debugging won't produce correct results.");

namespace DXBCBytecode
{
rdcstr toString(const uint32_t values[], uint32_t numComps);

const DXBC::CBufferVariable *FindCBufferVar(const uint32_t minOffset, const uint32_t maxOffset,
                                            const rdcarray<DXBC::CBufferVariable> &variables,
                                            uint32_t &byteOffset, rdcstr &prefix)
{
  for(const DXBC::CBufferVariable &v : variables)
  {
    // absolute byte offset of this variable in the cbuffer
    const uint32_t voffs = byteOffset + v.offset;

    // does minOffset-maxOffset reside in this variable? We don't handle the case where the range
    // crosses a variable (and I don't think FXC emits that anyway).
    if(voffs <= minOffset && voffs + v.type.descriptor.bytesize > maxOffset)
    {
      byteOffset = voffs;

      // if it is a struct with members, recurse to find a closer match
      if(!v.type.members.empty())
      {
        prefix += v.name + ".";
        return FindCBufferVar(minOffset, maxOffset, v.type.members, byteOffset, prefix);
      }

      // otherwise return this variable.
      return &v;
    }
  }

  return NULL;
}

bool Operand::operator==(const Operand &o) const
{
  if(type != o.type)
    return false;
  if(numComponents != o.numComponents)
    return false;
  if(memcmp(comps, o.comps, 4) != 0)
    return false;
  if(modifier != o.modifier)
    return false;

  if(indices.size() != o.indices.size())
    return false;

  for(size_t i = 0; i < indices.size(); i++)
    if(indices[i] != o.indices[i])
      return false;

  for(size_t i = 0; i < 4; i++)
    if(values[i] != o.values[i])
      return false;

  return true;
}

bool Operand::sameResource(const Operand &o) const
{
  if(type != o.type)
    return false;

  if(indices.size() == o.indices.size() && indices.empty())
    return true;

  if(indices.size() < 1 || o.indices.size() < 1)
    return false;

  return indices[0] == o.indices[0];
}

rdcstr Operand::toString(const DXBC::Reflection *reflection, ToString flags) const
{
  rdcstr str, regstr;

  const bool decl = flags & ToString::IsDecl;
  const bool swizzle = flags & ToString::ShowSwizzle;
  const bool friendly = flags & ToString::FriendlyNameRegisters;

  char swiz[6] = {0, 0, 0, 0, 0, 0};

  char compchars[] = {'x', 'y', 'z', 'w'};

  for(int i = 0; i < 4; i++)
  {
    if(comps[i] < 4)
    {
      swiz[0] = '.';
      swiz[i + 1] = compchars[comps[i]];
    }
  }

  if(type == TYPE_NULL)
  {
    str = "null";
  }
  else if(type == TYPE_INTERFACE)
  {
    RDCASSERT(indices.size() == 2);

    str = StringFormat::Fmt("fp%s[%s][%u]", indices[0].str.c_str(), indices[1].str.c_str(), funcNum);
  }
  else if(type == TYPE_RESOURCE || type == TYPE_SAMPLER || type == TYPE_UNORDERED_ACCESS_VIEW)
  {
    // pre-DX11, just an index
    if(indices.size() == 1)
    {
      if(type == TYPE_RESOURCE)
        str = "t";
      if(type == TYPE_SAMPLER)
        str = "s";
      if(type == TYPE_UNORDERED_ACCESS_VIEW)
        str = "u";

      str += indices[0].str;

      if(friendly && reflection && indices[0].absolute)
      {
        uint32_t idx = (uint32_t)indices[0].index;

        const rdcarray<DXBC::ShaderInputBind> *list = NULL;

        if(type == TYPE_RESOURCE)
          list = &reflection->SRVs;
        else if(type == TYPE_UNORDERED_ACCESS_VIEW)
          list = &reflection->UAVs;
        else if(type == TYPE_SAMPLER)
          list = &reflection->Samplers;

        if(list)
        {
          for(const DXBC::ShaderInputBind &b : *list)
          {
            if(b.reg != idx || b.space != 0)
              continue;

            if(decl)
              regstr = str;
            str = b.name;
            break;
          }
        }
      }
    }
    else if(indices.size() == 3)
    {
      if(type == TYPE_RESOURCE)
        str = "T";
      if(type == TYPE_SAMPLER)
        str = "S";
      if(type == TYPE_UNORDERED_ACCESS_VIEW)
        str = "U";

      // DX12 declaration

      // if declaration pointer is NULL we're printing inside the declaration itself.
      // Upper/lower bounds are printed with the space too, but print them here as
      // operand indices refer relative to those bounds.

      // detect common case of non-arrayed resources and simplify
      RDCASSERT(indices[1].absolute && indices[2].absolute);
      if(indices[1].index == indices[2].index)
      {
        str += indices[0].str;
      }
      else
      {
        if(indices[2].index == 0xffffffff)
          str += StringFormat::Fmt("%s[%s:unbound]", indices[0].str.c_str(), indices[1].str.c_str());
        else
          str += StringFormat::Fmt("%s[%s:%s]", indices[0].str.c_str(), indices[1].str.c_str(),
                                   indices[2].str.c_str());
      }
    }
    else if(indices.size() == 2)
    {
      if(type == TYPE_RESOURCE)
        str = "T";
      if(type == TYPE_SAMPLER)
        str = "S";
      if(type == TYPE_UNORDERED_ACCESS_VIEW)
        str = "U";

      // DX12 lookup

      // if we have a declaration, see if it's non-arrayed
      if(declaration && declaration->operand.indices[1].index == declaration->operand.indices[2].index)
      {
        // resource index should be equal to the bound
        RDCASSERT(indices[1].absolute && indices[1].index == declaration->operand.indices[1].index);

        // just include ID
        str += indices[0].str;
      }
      else
      {
        if(indices[1].relative)
          str += StringFormat::Fmt("%s%s", indices[0].str.c_str(), indices[1].str.c_str());
        else
          str += StringFormat::Fmt("%s[%s]", indices[0].str.c_str(), indices[1].str.c_str());
      }
    }
    else
    {
      RDCERR("Unexpected dimensions for resource-type operand: %x, %u", type,
             (uint32_t)indices.size());
    }
  }
  else if(type == TYPE_CONSTANT_BUFFER)
  {
    if(indices.size() == 3)
    {
      str = "CB";

      if(declaration)
      {
        // see if the declaration was non-arrayed
        if(declaration->operand.indices[1].index == declaration->operand.indices[2].index)
        {
          // resource index should be equal to the bound
          RDCASSERT(indices[1].absolute && indices[1].index == declaration->operand.indices[1].index);

          // just include ID and vector index
          if(indices[2].relative)
            str += StringFormat::Fmt("%s%s", indices[0].str.c_str(), indices[2].str.c_str());
          else
            str += StringFormat::Fmt("%s[%s]", indices[0].str.c_str(), indices[2].str.c_str());
        }
        else
        {
          str += indices[0].str;

          if(indices[1].relative)
            str += indices[1].str;
          else
            str += "[" + indices[1].str + "]";

          if(indices[2].relative)
            str += indices[1].str;
          else
            str += "[" + indices[2].str + "]";
        }
      }
      else
      {
        // if declaration pointer is NULL we're printing inside the declaration itself.
        // Because of the operand format, the size of the constant buffer is also in a
        // separate DWORD printed elsewhere.
        // Upper/lower bounds are printed with the space too, but print them here as
        // operand indices refer relative to those bounds.

        // detect common case of non-arrayed resources and simplify
        RDCASSERT(indices[1].absolute && indices[2].absolute);
        if(indices[1].index == indices[2].index)
        {
          str += indices[0].str;
        }
        else
        {
          if(indices[2].index == 0xffffffff)
            str +=
                StringFormat::Fmt("%s[%s:unbound]", indices[0].str.c_str(), indices[1].str.c_str());
          else
            str += StringFormat::Fmt("%s[%s:%s]", indices[0].str.c_str(), indices[1].str.c_str(),
                                     indices[2].str.c_str());
        }
      }
    }
    else
    {
      str = "cb";

      if(indices[1].relative)
        str += StringFormat::Fmt("%s%s", indices[0].str.c_str(), indices[1].str.c_str());
      else
        str += StringFormat::Fmt("%s[%s]", indices[0].str.c_str(), indices[1].str.c_str());

      if(friendly && reflection && indices[0].absolute)
      {
        const DXBC::CBuffer *cbuffer = NULL;

        for(const DXBC::CBuffer &cb : reflection->CBuffers)
        {
          if(cb.space == 0 && cb.reg == uint32_t(indices[0].index))
          {
            cbuffer = &cb;
            break;
          }
        }

        if(cbuffer)
        {
          // if the second index is constant then this is easy enough, we just find the matching
          // cbuffer variable and use its name, possibly rebasing the swizzle.
          // Unfortunately for many cases it's something like cbX[r0.x + 0] then in the next
          // instruction cbX[r0.x + 1] and so on, and it's obvious that it's indexing into the same
          // array for subsequent entries. However without knowing r0 we have no way to look up the
          // matching variable
          if(indices[1].absolute && !indices[1].relative)
          {
            uint8_t minComp = comps[0];
            uint8_t maxComp = comps[0];
            for(int i = 1; i < 4; i++)
            {
              if(comps[i] < 4)
              {
                minComp = RDCMIN(minComp, comps[i]);
                maxComp = RDCMAX(maxComp, comps[i]);
              }
            }

            uint32_t minOffset = uint32_t(indices[1].index) * 16 + minComp * 4;
            uint32_t maxOffset = uint32_t(indices[1].index) * 16 + maxComp * 4;

            uint32_t baseOffset = 0;

            rdcstr prefix;
            const DXBC::CBufferVariable *var =
                FindCBufferVar(minOffset, maxOffset, cbuffer->variables, baseOffset, prefix);

            if(var)
            {
              str = prefix + var->name;

              // for indices, look at just which register is selected
              minOffset &= ~0xf;
              uint32_t varOffset = minOffset - baseOffset;

              // if it's an array, add the index based on the relative index to the base offset
              if(var->type.descriptor.elements > 1)
              {
                uint32_t byteSize = var->type.descriptor.bytesize;

                // round up the byte size to a the nearest vec4 in case it's not quite a multiple
                byteSize = AlignUp16(byteSize);

                const uint32_t elementSize = byteSize / var->type.descriptor.elements;

                const uint32_t elementIndex = varOffset / elementSize;

                str += StringFormat::Fmt("[%u]", elementIndex);

                // subtract off so that if there's any further offset, it can be processed
                varOffset -= elementIndex;
              }

              // or if it's a matrix
              if((var->type.descriptor.varClass == DXBC::CLASS_MATRIX_ROWS &&
                  var->type.descriptor.cols > 1) ||
                 (var->type.descriptor.varClass == DXBC::CLASS_MATRIX_COLUMNS &&
                  var->type.descriptor.rows > 1))
              {
                str += StringFormat::Fmt("[%u]", varOffset / 16);
              }

              // rebase swizzle if necessary
              uint32_t vecOffset = (var->offset & 0xf);
              if(vecOffset > 0)
              {
                for(int i = 0; i < 4; i++)
                {
                  if(swiz[i + 1])
                    swiz[i + 1] = compchars[comps[i] - uint8_t(vecOffset / 4)];
                }
              }
            }
          }
        }
      }
    }
  }
  else if(type == TYPE_TEMP || type == TYPE_OUTPUT || type == TYPE_STREAM ||
          type == TYPE_THREAD_GROUP_SHARED_MEMORY || type == TYPE_FUNCTION_BODY)
  {
    if(type == TYPE_TEMP)
      str = "r";
    if(type == TYPE_OUTPUT)
      str = "o";
    if(type == TYPE_STREAM)
      str = "m";
    if(type == TYPE_THREAD_GROUP_SHARED_MEMORY)
      str = "g";
    if(type == TYPE_FUNCTION_BODY)
      str = "fb";

    RDCASSERTEQUAL(indices.size(), 1);

    str += indices[0].str;
  }
  else if(type == TYPE_IMMEDIATE_CONSTANT_BUFFER || type == TYPE_INDEXABLE_TEMP ||
          type == TYPE_INPUT || type == TYPE_INPUT_CONTROL_POINT ||
          type == TYPE_INPUT_PATCH_CONSTANT || type == TYPE_THIS_POINTER ||
          type == TYPE_OUTPUT_CONTROL_POINT)
  {
    if(type == TYPE_IMMEDIATE_CONSTANT_BUFFER)
      str = "icb";
    if(type == TYPE_INDEXABLE_TEMP)
      str = "x";
    if(type == TYPE_INPUT)
      str = "v";
    if(type == TYPE_INPUT_CONTROL_POINT)
      str = "vicp";
    if(type == TYPE_INPUT_PATCH_CONSTANT)
      str = "vpc";
    if(type == TYPE_OUTPUT_CONTROL_POINT)
      str = "vocp";
    if(type == TYPE_THIS_POINTER)
      str = "this";

    if(indices.size() == 1 && type != TYPE_IMMEDIATE_CONSTANT_BUFFER)
    {
      str += indices[0].str;
    }
    else
    {
      for(size_t i = 0; i < indices.size(); i++)
      {
        if(i == 0 && type == TYPE_INDEXABLE_TEMP)
        {
          str += indices[i].str;
          continue;
        }

        if(indices[i].relative)
          str += indices[i].str;
        else
          str += "[" + indices[i].str + "]";
      }
    }
  }
  else if(type == TYPE_IMMEDIATE32)
  {
    RDCASSERT(indices.size() == 0);

    str = "l(" + DXBCBytecode::toString(values, numComponents == NUMCOMPS_1 ? 1U : 4U) + ")";
  }
  else if(type == TYPE_IMMEDIATE64)
  {
    double *dv = (double *)values;
    str += StringFormat::Fmt("d(%lfl, %lfl)", dv[0], dv[1]);
  }
  else if(type == TYPE_RASTERIZER)
    str = "rasterizer";
  else if(type == TYPE_OUTPUT_CONTROL_POINT_ID)
    str = "vOutputControlPointID";
  else if(type == TYPE_INPUT_DOMAIN_POINT)
    str = "vDomain";
  else if(type == TYPE_INPUT_PRIMITIVEID)
    str = "vPrim";
  else if(type == TYPE_INPUT_COVERAGE_MASK)
    str = "vCoverageMask";
  else if(type == TYPE_INPUT_GS_INSTANCE_ID)
    str = "vGSInstanceID";
  else if(type == TYPE_INPUT_THREAD_ID)
    str = "vThreadID";
  else if(type == TYPE_INPUT_THREAD_GROUP_ID)
    str = "vThreadGroupID";
  else if(type == TYPE_INPUT_THREAD_ID_IN_GROUP)
    str = "vThreadIDInGroup";
  else if(type == TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED)
    str = "vThreadIDInGroupFlattened";
  else if(type == TYPE_INPUT_FORK_INSTANCE_ID)
    str = "vForkInstanceID";
  else if(type == TYPE_INPUT_JOIN_INSTANCE_ID)
    str = "vJoinInstanceID";
  else if(type == TYPE_OUTPUT_DEPTH)
    str = "oDepth";
  else if(type == TYPE_OUTPUT_DEPTH_LESS_EQUAL)
    str = "oDepthLessEqual";
  else if(type == TYPE_OUTPUT_DEPTH_GREATER_EQUAL)
    str = "oDepthGreaterEqual";
  else if(type == TYPE_OUTPUT_COVERAGE_MASK)
    str = "oMask";
  else if(type == TYPE_OUTPUT_STENCIL_REF)
    str = "oStencilRef";
  else
  {
    RDCERR("Unsupported system value semantic %d", type);
    str = "oUnsupported";
  }

  if(swizzle)
    str += swiz;

  if(precision != PRECISION_DEFAULT)
  {
    str += " {";
    if(precision == PRECISION_FLOAT10)
      str += "min10f";
    if(precision == PRECISION_FLOAT16)
      str += "min16f";
    if(precision == PRECISION_UINT16)
      str += "min16u";
    if(precision == PRECISION_SINT16)
      str += "min16i";
    if(precision == PRECISION_ANY16)
      str += "any16";
    if(precision == PRECISION_ANY10)
      str += "any10";
    str += "}";
  }

  if(modifier == OPERAND_MODIFIER_NEG)
    str = "-" + str;
  if(modifier == OPERAND_MODIFIER_ABS)
    str = "abs(" + str + ")";
  if(modifier == OPERAND_MODIFIER_ABSNEG)
    str = "-abs(" + str + ")";

  if(decl && !regstr.empty())
    str += StringFormat::Fmt(" (%s)", regstr.c_str());

  if(!name.empty())
    str = name + "=" + str;

  return str;
}

void Program::FetchComputeProperties(DXBC::Reflection *reflection)
{
  if(m_ProgramWords.empty())
    return;

  uint32_t *begin = &m_ProgramWords.front();
  uint32_t *cur = begin;
  uint32_t *end = &m_ProgramWords.back();

  // skip header dword above
  cur++;

  // skip length dword
  cur++;

  while(cur < end)
  {
    uint32_t OpcodeToken0 = cur[0];

    OpcodeType op = Opcode::Type.Get(OpcodeToken0);

    if(op == OPCODE_DCL_THREAD_GROUP)
    {
      reflection->DispatchThreadsDimension[0] = cur[1];
      reflection->DispatchThreadsDimension[1] = cur[2];
      reflection->DispatchThreadsDimension[2] = cur[3];
    }
    else if(op == OPCODE_DCL_INPUT)
    {
      OperandType type = Oper::Type.Get(cur[1]);

      SigParameter param;

      param.varType = VarType::UInt;
      param.regIndex = ~0U;

      switch(type)
      {
        case TYPE_INPUT_THREAD_ID:
          param.systemValue = ShaderBuiltin::DispatchThreadIndex;
          param.compCount = 3;
          param.regChannelMask = param.channelUsedMask = 0x7;
          param.semanticIdxName = param.semanticName = "vThreadID";
          reflection->InputSig.push_back(param);
          break;
        case TYPE_INPUT_THREAD_GROUP_ID:
          param.systemValue = ShaderBuiltin::GroupIndex;
          param.compCount = 3;
          param.regChannelMask = param.channelUsedMask = 0x7;
          param.semanticIdxName = param.semanticName = "vThreadGroupID";
          reflection->InputSig.push_back(param);
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP:
          param.systemValue = ShaderBuiltin::GroupThreadIndex;
          param.compCount = 3;
          param.regChannelMask = param.channelUsedMask = 0x7;
          param.semanticIdxName = param.semanticName = "vThreadIDInGroup";
          reflection->InputSig.push_back(param);
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          param.systemValue = ShaderBuiltin::GroupFlatIndex;
          param.compCount = 1;
          param.regChannelMask = param.channelUsedMask = 0x1;
          param.semanticIdxName = param.semanticName = "vThreadIDInGroupFlattened";
          reflection->InputSig.push_back(param);
          break;
        default: RDCERR("Unexpected input parameter %d", type); break;
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
}

void Program::DecodeProgram()
{
  if(m_Disassembled)
    return;

  if(m_ProgramWords.empty())
    return;

  m_Disassembled = true;

  uint32_t *begin = &m_ProgramWords.front();
  uint32_t *cur = begin;
  uint32_t *end = &m_ProgramWords.back();

  // check supported types
  if(!(m_Major == 0x5 && m_Minor == 0x1) && !(m_Major == 0x5 && m_Minor == 0x0) &&
     !(m_Major == 0x4 && m_Minor == 0x1) && !(m_Major == 0x4 && m_Minor == 0x0))
  {
    RDCERR("Unsupported shader bytecode version: %u.%u", m_Major, m_Minor);
    return;
  }

  RDCASSERT(LengthToken::Length.Get(cur[1]) == m_ProgramWords.size());    // length token

  cur += 2;

  // count how many declarations are so we can get the vector statically sized
  size_t numDecls = 0;
  uint32_t *tmp = cur;

  while(tmp < end)
  {
    uint32_t OpcodeToken0 = tmp[0];

    OpcodeType op = Opcode::Type.Get(OpcodeToken0);

    if(IsDeclaration(op))
      numDecls++;

    if(op == OPCODE_CUSTOMDATA)
    {
      // length in opcode token is 0, full length is in second dword
      tmp += tmp[1];
    }
    else
    {
      tmp += Opcode::Length.Get(OpcodeToken0);
    }
  }

  m_Declarations.reserve(numDecls);

  const bool friendly = DXBC_Disassembly_FriendlyNaming();

  while(cur < end)
  {
    Operation op;
    Declaration decl;

    uintptr_t offset = cur - begin;

    decl.instruction = m_Instructions.size();
    decl.offset = offset * sizeof(uint32_t);
    op.offset = offset * sizeof(uint32_t);

    if(!DecodeOperation(cur, op, friendly))
    {
      if(!DecodeDecl(cur, decl, friendly))
      {
        RDCERR("Unexpected non-operation and non-decl in token stream at 0x%x", cur - begin);
      }
      else
      {
        m_Declarations.push_back(decl);
      }
    }
    else
    {
      m_Instructions.push_back(op);
    }
  }

  RDCASSERT(m_Declarations.size() <= numDecls);

  if(m_Instructions.empty() || m_Instructions.back().operation != OPCODE_RET)
  {
    Operation implicitRet;
    implicitRet.length = 1;
    implicitRet.offset = (end - begin) * sizeof(uint32_t);
    implicitRet.operation = OPCODE_RET;
    implicitRet.str = "ret";

    m_Instructions.push_back(implicitRet);
  }

  if(DXBC_Disassembly_ProcessVendorShaderExts() && m_ShaderExt.second != ~0U)
    PostprocessVendorExtensions();
}

void Program::MakeDisassemblyString()
{
  DecodeProgram();

  if(m_ProgramWords.empty())
  {
    m_Disassembly = "No bytecode in this blob";
    return;
  }

  rdcstr shadermodel = "xs_";

  switch(m_Type)
  {
    case DXBC::ShaderType::Pixel: shadermodel = "ps_"; break;
    case DXBC::ShaderType::Vertex: shadermodel = "vs_"; break;
    case DXBC::ShaderType::Geometry: shadermodel = "gs_"; break;
    case DXBC::ShaderType::Hull: shadermodel = "hs_"; break;
    case DXBC::ShaderType::Domain: shadermodel = "ds_"; break;
    case DXBC::ShaderType::Compute: shadermodel = "cs_"; break;
    default: RDCERR("Unknown shader type: %u", m_Type); break;
  }

  m_Disassembly = StringFormat::Fmt("%s%d_%d\n", shadermodel.c_str(), m_Major, m_Minor);

  uint32_t linenum = 2;

  int indent = 0;

  size_t d = 0;

  LineColumnInfo prevLineInfo;
  rdcarray<rdcstr> prevCallstack;

  size_t debugInst = 0;

  rdcarray<rdcarray<rdcstr>> fileLines;

  // generate fileLines by splitting each file in the debug info
  if(m_DebugInfo)
  {
    fileLines.resize(m_DebugInfo->Files.size());

    for(size_t i = 0; i < m_DebugInfo->Files.size(); i++)
      split(m_DebugInfo->Files[i].second, fileLines[i], '\n');
  }

  for(size_t i = 0; i < m_Instructions.size(); i++)
  {
    for(; d < m_Declarations.size(); d++)
    {
      if(m_Declarations[d].instruction > i)
      {
        if(i == 0)
        {
          m_Disassembly += "\n";
          linenum++;
        }

        break;
      }

      m_Disassembly += StringFormat::Fmt("% 4s  %s\n", "", m_Declarations[d].str.c_str());
      linenum++;

      int32_t nl = m_Declarations[d].str.indexOf('\n');
      while(nl >= 0)
      {
        linenum++;
        nl = m_Declarations[d].str.indexOf('\n', nl + 1);
      }
    }

    if(m_Instructions[i].operation == OPCODE_ENDIF || m_Instructions[i].operation == OPCODE_ENDLOOP)
    {
      indent--;
    }

    if(m_DebugInfo)
    {
      LineColumnInfo lineInfo;
      rdcarray<rdcstr> callstack;

      m_DebugInfo->GetLineInfo(debugInst, m_Instructions[i].offset, lineInfo);
      m_DebugInfo->GetCallstack(debugInst, m_Instructions[i].offset, callstack);

      if(lineInfo.fileIndex >= 0 && (lineInfo.fileIndex != prevLineInfo.fileIndex ||
                                     lineInfo.lineStart != prevLineInfo.lineStart))
      {
        rdcstr line = "";
        if(lineInfo.fileIndex >= (int32_t)fileLines.size())
        {
          line = "Unknown file";
        }
        else if(fileLines[lineInfo.fileIndex].empty())
        {
          line = "";
        }
        else
        {
          rdcarray<rdcstr> &lines = fileLines[lineInfo.fileIndex];

          int32_t lineIdx = RDCMIN(lineInfo.lineStart, (uint32_t)lines.size() - 1);

          // line numbers are 1-based but we want a 0-based index
          if(lineIdx > 0)
            lineIdx--;
          line = lines[lineIdx];
        }

        int startLine = line.find_first_not_of(" \t");

        if(startLine >= 0)
          line = line.substr(startLine);

        m_Disassembly += "\n";
        linenum++;

        if(((lineInfo.fileIndex != prevLineInfo.fileIndex || callstack.back() != prevCallstack.back()) &&
            lineInfo.fileIndex < (int32_t)fileLines.size()) ||
           line == "")
        {
          m_Disassembly += "      ";    // "0000: "
          for(int in = 0; in < indent; in++)
            m_Disassembly += "  ";

          rdcstr func = callstack.back();

          if(!func.empty())
          {
            m_Disassembly += StringFormat::Fmt("%s:%d - %s()\n",
                                               m_DebugInfo->Files[lineInfo.fileIndex].first.c_str(),
                                               lineInfo.lineStart, func.c_str());
            linenum++;
          }
          else
          {
            m_Disassembly += StringFormat::Fmt(
                "%s:%d\n", m_DebugInfo->Files[lineInfo.fileIndex].first.c_str(), lineInfo.lineStart);
            linenum++;
          }
        }

        if(line != "")
        {
          m_Disassembly += "      ";    // "0000: "
          for(int in = 0; in < indent; in++)
            m_Disassembly += "  ";
          m_Disassembly += line + "\n";
          linenum++;
        }
      }

      prevLineInfo = lineInfo;
      prevCallstack = callstack;
    }

    int curIndent = indent;
    if(m_Instructions[i].operation == OPCODE_ELSE)
      curIndent--;

    rdcstr whitespace;
    whitespace.fill(curIndent * 2, ' ');

    m_Instructions[i].line = linenum;

    m_Disassembly +=
        StringFormat::Fmt("% 4u: %s%s\n", i, whitespace.c_str(), m_Instructions[i].str.c_str());
    linenum++;

    if(m_Instructions[i].operation == OPCODE_IF || m_Instructions[i].operation == OPCODE_LOOP)
    {
      indent++;
    }

    if(m_Instructions[i].operation != OPCODE_HS_CONTROL_POINT_PHASE &&
       m_Instructions[i].operation != OPCODE_HS_FORK_PHASE &&
       m_Instructions[i].operation != OPCODE_HS_JOIN_PHASE)
      debugInst++;
  }
}

bool Program::DecodeOperand(uint32_t *&tokenStream, ToString flags, Operand &retOper)
{
  uint32_t OperandToken0 = tokenStream[0];

  retOper.type = Oper::Type.Get(OperandToken0);
  retOper.numComponents = Oper::NumComponents.Get(OperandToken0);

  SelectionMode selMode = Oper::SelectionMode.Get(OperandToken0);

  if(selMode == SELECTION_MASK)
  {
    int i = 0;

    if(Oper::ComponentMaskX.Get(OperandToken0))
      retOper.comps[i++] = 0;
    if(Oper::ComponentMaskY.Get(OperandToken0))
      retOper.comps[i++] = 1;
    if(Oper::ComponentMaskZ.Get(OperandToken0))
      retOper.comps[i++] = 2;
    if(Oper::ComponentMaskW.Get(OperandToken0))
      retOper.comps[i++] = 3;
  }
  else if(selMode == SELECTION_SWIZZLE)
  {
    retOper.comps[0] = Oper::ComponentSwizzleX.Get(OperandToken0);
    retOper.comps[1] = Oper::ComponentSwizzleY.Get(OperandToken0);
    retOper.comps[2] = Oper::ComponentSwizzleZ.Get(OperandToken0);
    retOper.comps[3] = Oper::ComponentSwizzleW.Get(OperandToken0);
  }
  else if(selMode == SELECTION_SELECT_1)
  {
    retOper.comps[0] = Oper::ComponentSel1.Get(OperandToken0);
  }

  uint32_t indexDim = Oper::IndexDimension.Get(OperandToken0);

  OperandIndexType rep[] = {
      Oper::Index0.Get(OperandToken0), Oper::Index1.Get(OperandToken0),
      Oper::Index2.Get(OperandToken0),
  };

  bool extended = Oper::Extended.Get(OperandToken0);

  tokenStream++;

  while(extended)
  {
    uint32_t OperandTokenN = tokenStream[0];

    ExtendedOperandType type = ExtendedOperand::Type.Get(OperandTokenN);

    if(type == EXTENDED_OPERAND_MODIFIER)
    {
      retOper.modifier = ExtendedOperand::Modifier.Get(OperandTokenN);
      retOper.precision = ExtendedOperand::MinPrecision.Get(OperandTokenN);
    }
    else
    {
      RDCERR("Unexpected extended operand modifier");
    }

    extended = ExtendedOperand::Extended.Get(OperandTokenN) == 1;

    tokenStream++;
  }

  retOper.indices.resize(indexDim);

  if(retOper.type == TYPE_IMMEDIATE32 || retOper.type == TYPE_IMMEDIATE64)
  {
    RDCASSERT(retOper.indices.empty());

    uint32_t numComps = 1;

    if(retOper.numComponents == NUMCOMPS_1)
      numComps = 1;
    else if(retOper.numComponents == NUMCOMPS_4)
      numComps = 4;
    else
      RDCERR("N-wide vectors not supported.");

    for(uint32_t i = 0; i < numComps; i++)
    {
      retOper.values[i] = tokenStream[0];
      tokenStream++;
    }
  }

  for(int idx = 0; idx < (int)indexDim; idx++)
  {
    if(rep[idx] == INDEX_IMMEDIATE32_PLUS_RELATIVE || rep[idx] == INDEX_IMMEDIATE32)
    {
      retOper.indices[idx].absolute = true;
      retOper.indices[idx].index = tokenStream[0];

      tokenStream++;
    }
    else if(rep[idx] == INDEX_IMMEDIATE64_PLUS_RELATIVE || rep[idx] == INDEX_IMMEDIATE64)
    {
      retOper.indices[idx].absolute = true;

      // hi/lo words
      retOper.indices[idx].index = tokenStream[0];
      retOper.indices[idx].index <<= 32;
      tokenStream++;

      retOper.indices[idx].index |= tokenStream[0];
      tokenStream++;

      RDCCOMPILE_ASSERT(sizeof(retOper.indices[idx].index) == 8, "Index is the wrong byte width");
    }

    if(rep[idx] == INDEX_IMMEDIATE64_PLUS_RELATIVE || rep[idx] == INDEX_IMMEDIATE32_PLUS_RELATIVE ||
       rep[idx] == INDEX_RELATIVE)
    {
      // relative addressing
      retOper.indices[idx].relative = true;

      bool ret = DecodeOperand(tokenStream, flags, retOper.indices[idx].operand);
      RDCASSERT(ret);
    }

    RDCASSERT(retOper.indices[idx].relative || retOper.indices[idx].absolute);

    if(retOper.indices[idx].relative)
    {
      retOper.indices[idx].str = StringFormat::Fmt(
          "[%s + %llu]",
          retOper.indices[idx].operand.toString(m_Reflection, flags | ToString::ShowSwizzle).c_str(),
          retOper.indices[idx].index);
    }
    else
    {
      retOper.indices[idx].str = ToStr(retOper.indices[idx].index);
    }
  }

  if(retOper.type == TYPE_RESOURCE || retOper.type == TYPE_SAMPLER ||
     retOper.type == TYPE_UNORDERED_ACCESS_VIEW || retOper.type == TYPE_CONSTANT_BUFFER)
  {
    // try and find a declaration with a matching ID
    RDCASSERT(retOper.indices.size() > 0 && retOper.indices[0].absolute);
    for(size_t i = 0; i < m_Declarations.size(); i++)
    {
      // does the ID match, if so, it's our declaration
      if(m_Declarations[i].operand.type == retOper.type &&
         m_Declarations[i].operand.indices[0] == retOper.indices[0])
      {
        retOper.declaration = &m_Declarations[i];
        break;
      }
    }
  }

  return true;
}

bool Program::DecodeDecl(uint32_t *&tokenStream, Declaration &retDecl, bool friendlyName)
{
  uint32_t *begin = tokenStream;
  uint32_t OpcodeToken0 = tokenStream[0];

  ToString flags = friendlyName ? ToString::FriendlyNameRegisters : ToString::None;
  flags = flags | ToString::IsDecl;

  const bool sm51 = (m_Major == 0x5 && m_Minor == 0x1);

  OpcodeType op = Opcode::Type.Get(OpcodeToken0);

  RDCASSERT(op < NUM_REAL_OPCODES);

  if(!IsDeclaration(op))
    return false;

  if(op == OPCODE_CUSTOMDATA)
  {
    CustomDataClass customClass = Opcode::CustomClass.Get(OpcodeToken0);

    tokenStream++;
    // DWORD length including OpcodeToken0 and this length token
    uint32_t customDataLength = tokenStream[0];
    tokenStream++;

    RDCASSERT(customDataLength >= 2);

    switch(customClass)
    {
      case CUSTOMDATA_SHADER_MESSAGE:
      {
        // handle as opcode
        tokenStream = begin;
        return false;
      }
      case CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER:
      {
        retDecl.str = "dcl_immediateConstantBuffer {";

        uint32_t dataLength = customDataLength - 2;

        RDCASSERT(dataLength % 4 == 0);

        for(uint32_t i = 0; i < dataLength; i++)
        {
          if(i % 4 == 0)
            retDecl.str += "\n\t\t\t{ ";

          m_Immediate.push_back(tokenStream[0]);

          retDecl.str += toString(tokenStream, 1);

          tokenStream++;

          if((i + 1) % 4 == 0)
            retDecl.str += "}";

          if(i + 1 < dataLength)
            retDecl.str += ", ";
        }

        retDecl.str += " }";

        break;
      }

      default:
      {
        RDCWARN("Unsupported custom data class %d!", customClass);

        uint32_t dataLength = customDataLength - 2;
        RDCLOG("Data length seems to be %d uint32s", dataLength);

#if 0
        for(uint32_t i = 0; i < dataLength; i++)
        {
          char *str = (char *)tokenStream;
          RDCDEBUG("uint32 %d: 0x%08x   %c %c %c %c", i, tokenStream[0], str[0], str[1], str[2],
                   str[3]);
          tokenStream++;
        }
#else
        tokenStream += dataLength;
#endif

        break;
      }
    }

    return true;
  }

  retDecl.declaration = op;
  retDecl.length = Opcode::Length.Get(OpcodeToken0);

  tokenStream++;

  retDecl.str = ToStr(op);

  if(op == OPCODE_DCL_GLOBAL_FLAGS)
  {
    retDecl.refactoringAllowed = Decl::RefactoringAllowed.Get(OpcodeToken0);
    retDecl.doublePrecisionFloats = Decl::DoubleFloatOps.Get(OpcodeToken0);
    retDecl.forceEarlyDepthStencil = Decl::ForceEarlyDepthStencil.Get(OpcodeToken0);
    retDecl.enableRawAndStructuredBuffers = Decl::EnableRawStructuredBufs.Get(OpcodeToken0);
    retDecl.skipOptimisation = Decl::SkipOptimisation.Get(OpcodeToken0);
    retDecl.enableMinPrecision = Decl::EnableMinPrecision.Get(OpcodeToken0);
    retDecl.enableD3D11_1DoubleExtensions = Decl::EnableD3D11_1DoubleExtensions.Get(OpcodeToken0);
    retDecl.enableD3D11_1ShaderExtensions = Decl::EnableD3D11_1ShaderExtensions.Get(OpcodeToken0);
    retDecl.enableD3D12AllResourcesBound = Decl::EnableD3D12AllResourcesBound.Get(OpcodeToken0);

    retDecl.str += " ";

    bool added = false;

    if(retDecl.refactoringAllowed)
    {
      retDecl.str += "refactoringAllowed";
      added = true;
    }
    if(retDecl.doublePrecisionFloats)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "doublePrecisionFloats";
      added = true;
    }
    if(retDecl.forceEarlyDepthStencil)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "forceEarlyDepthStencil";
      added = true;
    }
    if(retDecl.enableRawAndStructuredBuffers)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "enableRawAndStructuredBuffers";
      added = true;
    }
    if(retDecl.skipOptimisation)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "skipOptimisation";
      added = true;
    }
    if(retDecl.enableMinPrecision)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "enableMinPrecision";
      added = true;
    }
    if(retDecl.enableD3D11_1DoubleExtensions)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "doubleExtensions";
      added = true;
    }
    if(retDecl.enableD3D11_1ShaderExtensions)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "shaderExtensions";
      added = true;
    }
    if(retDecl.enableD3D12AllResourcesBound)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "d3d12AllResourcesBound";
      added = true;
    }
  }
  else if(op == OPCODE_DCL_CONSTANT_BUFFER)
  {
    CBufferAccessPattern accessPattern = Decl::AccessPattern.Get(OpcodeToken0);

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    if(sm51)
    {
      // Store the size provided. If there's no reflection data, this will be
      // necessary to guess the buffer size properly
      retDecl.float4size = tokenStream[0];
      tokenStream++;

      retDecl.str += StringFormat::Fmt("[%u]", retDecl.float4size);
    }

    retDecl.str += ", ";

    if(accessPattern == ACCESS_IMMEDIATE_INDEXED)
      retDecl.str += "immediateIndexed";
    else if(accessPattern == ACCESS_DYNAMIC_INDEXED)
      retDecl.str += "dynamicIndexed";
    else
      RDCERR("Unexpected cbuffer access pattern");

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else if(retDecl.operand.indices[2].index == 0xffffffff)
        retDecl.str += StringFormat::Fmt(",regs=%u:unbound", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_INPUT)
  {
    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    if(retDecl.operand.type == TYPE_INPUT_COVERAGE_MASK)
      m_InputCoverage = true;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);
  }
  else if(op == OPCODE_DCL_TEMPS)
  {
    retDecl.numTemps = tokenStream[0];

    m_NumTemps = retDecl.numTemps;

    tokenStream++;

    retDecl.str += StringFormat::Fmt(" %u", retDecl.numTemps);
  }
  else if(op == OPCODE_DCL_INDEXABLE_TEMP)
  {
    retDecl.tempReg = tokenStream[0];
    tokenStream++;

    retDecl.numTemps = tokenStream[0];
    tokenStream++;

    retDecl.tempComponentCount = tokenStream[0];
    tokenStream++;

    // I don't think the compiler will ever declare a non-compact list of indexable temps, but just
    // to be sure our indexing works let's be safe.
    if(retDecl.tempReg >= m_IndexTempSizes.size())
      m_IndexTempSizes.resize(retDecl.tempReg + 1);
    m_IndexTempSizes[retDecl.tempReg] = retDecl.numTemps;

    retDecl.str += StringFormat::Fmt(" x%u[%u], %u", retDecl.tempReg, retDecl.numTemps,
                                     retDecl.tempComponentCount);
  }
  else if(op == OPCODE_DCL_OUTPUT)
  {
    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);
  }
  else if(op == OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT)
  {
    retDecl.str += " ";

    retDecl.maxOut = tokenStream[0];

    tokenStream++;

    retDecl.str += StringFormat::Fmt(" %u", retDecl.maxOut);
  }
  else if(op == OPCODE_DCL_INPUT_SIV || op == OPCODE_DCL_INPUT_SGV ||
          op == OPCODE_DCL_INPUT_PS_SIV || op == OPCODE_DCL_INPUT_PS_SGV ||
          op == OPCODE_DCL_OUTPUT_SIV || op == OPCODE_DCL_OUTPUT_SGV)
  {
    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.systemValue = (DXBC::SVSemantic)tokenStream[0];
    tokenStream++;

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);

    retDecl.str += ", ";
    retDecl.str += ToStr(retDecl.systemValue);
  }
  else if(op == OPCODE_DCL_STREAM)
  {
    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
  }
  else if(op == OPCODE_DCL_SAMPLER)
  {
    retDecl.samplerMode = Decl::SamplerMode.Get(OpcodeToken0);

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags);

    retDecl.str += ", ";
    if(retDecl.samplerMode == SAMPLER_MODE_DEFAULT)
      retDecl.str += "mode_default";
    if(retDecl.samplerMode == SAMPLER_MODE_COMPARISON)
      retDecl.str += "mode_comparison";
    if(retDecl.samplerMode == SAMPLER_MODE_MONO)
      retDecl.str += "mode_mono";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_RESOURCE)
  {
    retDecl.dim = Decl::ResourceDim.Get(OpcodeToken0);

    retDecl.sampleCount = 0;
    if(retDecl.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
       retDecl.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
    {
      retDecl.sampleCount = Decl::SampleCount.Get(OpcodeToken0);
    }

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    uint32_t ResourceReturnTypeToken = tokenStream[0];
    tokenStream++;

    retDecl.resType[0] = Decl::ReturnTypeX.Get(ResourceReturnTypeToken);
    retDecl.resType[1] = Decl::ReturnTypeY.Get(ResourceReturnTypeToken);
    retDecl.resType[2] = Decl::ReturnTypeZ.Get(ResourceReturnTypeToken);
    retDecl.resType[3] = Decl::ReturnTypeW.Get(ResourceReturnTypeToken);

    retDecl.str += "_";
    retDecl.str += ToStr(retDecl.dim);
    if(retDecl.sampleCount > 0)
    {
      retDecl.str += "(";
      retDecl.str += ToStr(retDecl.sampleCount);
      retDecl.str += ")";
    }
    retDecl.str += " ";

    retDecl.str += "(";
    retDecl.str += ToStr(retDecl.resType[0]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resType[1]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resType[2]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resType[3]);
    retDecl.str += ")";

    retDecl.str += " " + retDecl.operand.toString(m_Reflection, flags);

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_INPUT_PS)
  {
    retDecl.interpolation = Decl::InterpolationMode.Get(OpcodeToken0);

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += ToStr(retDecl.interpolation);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);
  }
  else if(op == OPCODE_DCL_INDEX_RANGE)
  {
    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);

    retDecl.indexRange = tokenStream[0];
    tokenStream++;

    retDecl.str += StringFormat::Fmt(" %u", retDecl.indexRange);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP)
  {
    retDecl.groupSize[0] = tokenStream[0];
    tokenStream++;

    retDecl.groupSize[1] = tokenStream[0];
    tokenStream++;

    retDecl.groupSize[2] = tokenStream[0];
    tokenStream++;

    retDecl.str += StringFormat::Fmt(" %u, %u, %u", retDecl.groupSize[0], retDecl.groupSize[1],
                                     retDecl.groupSize[2]);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW)
  {
    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.count = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str += StringFormat::Fmt(", %u", retDecl.count);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED)
  {
    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.stride = tokenStream[0];
    tokenStream++;

    retDecl.count = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str += StringFormat::Fmt(", %u, %u", retDecl.stride, retDecl.count);
  }
  else if(op == OPCODE_DCL_INPUT_CONTROL_POINT_COUNT || op == OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT)
  {
    retDecl.controlPointCount = Decl::ControlPointCount.Get(OpcodeToken0);

    retDecl.str += StringFormat::Fmt(" %u", retDecl.controlPointCount);
  }
  else if(op == OPCODE_DCL_TESS_DOMAIN)
  {
    retDecl.domain = Decl::TessDomain.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.domain == DOMAIN_ISOLINE)
      retDecl.str += "domain_isoline";
    else if(retDecl.domain == DOMAIN_TRI)
      retDecl.str += "domain_tri";
    else if(retDecl.domain == DOMAIN_QUAD)
      retDecl.str += "domain_quad";
    else
      RDCERR("Unexpected Tessellation domain");
  }
  else if(op == OPCODE_DCL_TESS_PARTITIONING)
  {
    retDecl.partition = Decl::TessPartitioning.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.partition == PARTITIONING_INTEGER)
      retDecl.str += "partitioning_integer";
    else if(retDecl.partition == PARTITIONING_POW2)
      retDecl.str += "partitioning_pow2";
    else if(retDecl.partition == PARTITIONING_FRACTIONAL_ODD)
      retDecl.str += "partitioning_fractional_odd";
    else if(retDecl.partition == PARTITIONING_FRACTIONAL_EVEN)
      retDecl.str += "partitioning_fractional_even";
    else
      RDCERR("Unexpected Partitioning");
  }
  else if(op == OPCODE_DCL_GS_INPUT_PRIMITIVE)
  {
    retDecl.inPrim = Decl::InputPrimitive.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.inPrim == PRIMITIVE_POINT)
      retDecl.str += "point";
    else if(retDecl.inPrim == PRIMITIVE_LINE)
      retDecl.str += "line";
    else if(retDecl.inPrim == PRIMITIVE_TRIANGLE)
      retDecl.str += "triangle";
    else if(retDecl.inPrim == PRIMITIVE_LINE_ADJ)
      retDecl.str += "line_adj";
    else if(retDecl.inPrim == PRIMITIVE_TRIANGLE_ADJ)
      retDecl.str += "triangle_adj";
    else if(retDecl.inPrim >= PRIMITIVE_1_CONTROL_POINT_PATCH &&
            retDecl.inPrim <= PRIMITIVE_32_CONTROL_POINT_PATCH)
    {
      retDecl.str += StringFormat::Fmt("control_point_patch_%u",
                                       1 + int(retDecl.inPrim - PRIMITIVE_1_CONTROL_POINT_PATCH));
    }
    else
      RDCERR("Unexpected primitive type");
  }
  else if(op == OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
  {
    retDecl.outTopology = Decl::OutputPrimitiveTopology.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_POINTLIST)
      retDecl.str += "point";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_LINELIST)
      retDecl.str += "linelist";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP)
      retDecl.str += "linestrip";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
      retDecl.str += "trianglelist";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
      retDecl.str += "trianglestrip";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
      retDecl.str += "linelist_adj";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
      retDecl.str += "linestrip_adj";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ)
      retDecl.str += "trianglelist_adj";
    else if(retDecl.outTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
      retDecl.str += "trianglestrip_adj";
    else
      RDCERR("Unexpected primitive topology");
  }
  else if(op == OPCODE_DCL_TESS_OUTPUT_PRIMITIVE)
  {
    retDecl.outPrim = Decl::OutputPrimitive.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.outPrim == OUTPUT_PRIMITIVE_POINT)
      retDecl.str += "output_point";
    else if(retDecl.outPrim == OUTPUT_PRIMITIVE_LINE)
      retDecl.str += "output_line";
    else if(retDecl.outPrim == OUTPUT_PRIMITIVE_TRIANGLE_CW)
      retDecl.str += "output_triangle_cw";
    else if(retDecl.outPrim == OUTPUT_PRIMITIVE_TRIANGLE_CCW)
      retDecl.str += "output_triangle_ccw";
    else
      RDCERR("Unexpected output primitive");
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW || op == OPCODE_DCL_RESOURCE_RAW)
  {
    retDecl.rov = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) &&
                  Decl::RasterizerOrderedAccess.Get(OpcodeToken0);

    retDecl.globallyCoherant =
        (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) & Decl::GloballyCoherent.Get(OpcodeToken0);

    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);

    if(retDecl.globallyCoherant)
      retDecl.str += ", globallyCoherant";

    if(retDecl.rov)
      retDecl.str += ", rasterizerOrderedAccess";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED || op == OPCODE_DCL_RESOURCE_STRUCTURED)
  {
    retDecl.hasCounter = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &&
                         Opcode::HasOrderPreservingCounter.Get(OpcodeToken0);

    retDecl.rov = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &&
                  Decl::RasterizerOrderedAccess.Get(OpcodeToken0);

    retDecl.globallyCoherant = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &
                               Decl::GloballyCoherent.Get(OpcodeToken0);

    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.stride = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str += StringFormat::Fmt(", %u", retDecl.stride);

    if(retDecl.hasCounter)
      retDecl.str += ", hasOrderPreservingCounter";

    if(retDecl.globallyCoherant)
      retDecl.str += ", globallyCoherant";

    if(retDecl.rov)
      retDecl.str += ", rasterizerOrderedAccess";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED)
  {
    retDecl.dim = Decl::ResourceDim.Get(OpcodeToken0);

    retDecl.globallyCoherant = Decl::GloballyCoherent.Get(OpcodeToken0);

    retDecl.rov = Decl::RasterizerOrderedAccess.Get(OpcodeToken0);

    retDecl.str += "_";
    retDecl.str += ToStr(retDecl.dim);

    if(retDecl.globallyCoherant)
      retDecl.str += "_glc";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    uint32_t ResourceReturnTypeToken = tokenStream[0];
    tokenStream++;

    retDecl.resType[0] = Decl::ReturnTypeX.Get(ResourceReturnTypeToken);
    retDecl.resType[1] = Decl::ReturnTypeY.Get(ResourceReturnTypeToken);
    retDecl.resType[2] = Decl::ReturnTypeZ.Get(ResourceReturnTypeToken);
    retDecl.resType[3] = Decl::ReturnTypeW.Get(ResourceReturnTypeToken);

    retDecl.str += " ";

    retDecl.str += "(";
    retDecl.str += ToStr(retDecl.resType[0]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resType[1]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resType[2]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resType[3]);
    retDecl.str += ")";

    retDecl.str += " ";

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);

    if(retDecl.rov)
      retDecl.str += ", rasterizerOrderedAccess";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT ||
          op == OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT || op == OPCODE_DCL_GS_INSTANCE_COUNT)
  {
    retDecl.instanceCount = tokenStream[0];
    tokenStream++;

    retDecl.str += StringFormat::Fmt(" %u", retDecl.instanceCount);
  }
  else if(op == OPCODE_DCL_HS_MAX_TESSFACTOR)
  {
    float *f = (float *)tokenStream;
    retDecl.maxTessFactor = *f;
    tokenStream++;

    retDecl.str += StringFormat::Fmt(" l(%f)", retDecl.maxTessFactor);
  }
  else if(op == OPCODE_DCL_FUNCTION_BODY)
  {
    retDecl.functionBody = tokenStream[0];
    tokenStream++;

    retDecl.str += StringFormat::Fmt(" fb%u", retDecl.functionBody);
  }
  else if(op == OPCODE_DCL_FUNCTION_TABLE)
  {
    retDecl.functionTable = tokenStream[0];
    tokenStream++;

    retDecl.str += StringFormat::Fmt(" ft%u", retDecl.functionTable);

    uint32_t TableLength = tokenStream[0];
    tokenStream++;

    retDecl.str += " = {";

    for(uint32_t i = 0; i < TableLength; i++)
    {
      retDecl.str += StringFormat::Fmt("fb%u", tokenStream[0]);

      if(i + 1 < TableLength)
        retDecl.str += ", ";

      retDecl.immediateData.push_back(tokenStream[0]);
      tokenStream++;
    }

    retDecl.str += "}";
  }
  else if(op == OPCODE_DCL_INTERFACE)
  {
    retDecl.interfaceID = tokenStream[0];
    tokenStream++;

    retDecl.numTypes = tokenStream[0];
    tokenStream++;

    uint32_t CountToken = tokenStream[0];
    tokenStream++;

    retDecl.numInterfaces = Decl::NumInterfaces.Get(CountToken);
    uint32_t TableLength = Decl::TableLength.Get(CountToken);

    retDecl.str += StringFormat::Fmt(" fp%u[%u][%u]", retDecl.interfaceID, retDecl.numInterfaces,
                                     retDecl.numTypes);

    retDecl.str += " = {";

    for(uint32_t i = 0; i < TableLength; i++)
    {
      retDecl.str += StringFormat::Fmt("ft%u", tokenStream[0]);

      if(i + 1 < TableLength)
        retDecl.str += ", ";

      retDecl.immediateData.push_back(tokenStream[0]);
      tokenStream++;
    }

    retDecl.str += "}";
  }
  else if(op == OPCODE_HS_DECLS)
  {
  }
  else
  {
    RDCERR("Unexpected opcode decl %d", op);
  }

  if(op == OPCODE_DCL_OUTPUT || op == OPCODE_DCL_OUTPUT_SIV || op == OPCODE_DCL_OUTPUT_SGV)
  {
    if(retDecl.operand.type == TYPE_OUTPUT_COVERAGE_MASK)
      m_OutputCoverage = true;
    else if(retDecl.operand.type == TYPE_OUTPUT_STENCIL_REF)
      m_OutputStencil = true;
    else if(retDecl.operand.type == TYPE_OUTPUT_DEPTH ||
            retDecl.operand.type == TYPE_OUTPUT_DEPTH_GREATER_EQUAL ||
            retDecl.operand.type == TYPE_OUTPUT_DEPTH_LESS_EQUAL)
      m_OutputDepth = true;
    else if(retDecl.operand.indices[0].absolute && retDecl.operand.indices[0].index < 0xffff)
      m_NumOutputs = RDCMAX(m_NumOutputs, uint32_t(retDecl.operand.indices[0].index) + 1);
  }

  // make sure we consumed all uint32s
  RDCASSERT((uint32_t)(tokenStream - begin) == retDecl.length);

  return true;
}

bool Program::DecodeOperation(uint32_t *&tokenStream, Operation &retOp, bool friendlyName)
{
  uint32_t *begin = tokenStream;
  uint32_t OpcodeToken0 = tokenStream[0];

  ToString flags = friendlyName ? ToString::FriendlyNameRegisters : ToString::None;

  OpcodeType op = Opcode::Type.Get(OpcodeToken0);

  RDCASSERT(op < NUM_REAL_OPCODES);

  if(IsDeclaration(op) && op != OPCODE_CUSTOMDATA)
    return false;

  // possibly only set these when applicable
  retOp.operation = op;
  retOp.length = Opcode::Length.Get(OpcodeToken0);
  retOp.nonzero = Opcode::TestNonZero.Get(OpcodeToken0) == 1;
  retOp.saturate = Opcode::Saturate.Get(OpcodeToken0) == 1;
  retOp.preciseValues = Opcode::PreciseValues.Get(OpcodeToken0);
  retOp.resinfoRetType = Opcode::ResinfoReturn.Get(OpcodeToken0);
  retOp.syncFlags = Opcode::SyncFlags.Get(OpcodeToken0);

  bool extended = Opcode::Extended.Get(OpcodeToken0) == 1;

  if(op == OPCODE_CUSTOMDATA)
  {
    CustomDataClass customClass = Opcode::CustomClass.Get(OpcodeToken0);

    tokenStream++;
    // DWORD length including OpcodeToken0 and this length token
    uint32_t customDataLength = tokenStream[0];
    tokenStream++;

    RDCASSERT(customDataLength >= 2);

    switch(customClass)
    {
      case CUSTOMDATA_SHADER_MESSAGE:
      {
        uint32_t *end = tokenStream + customDataLength - 2;

        // uint32_t infoQueueMsgId = tokenStream[0];
        uint32_t messageFormat = tokenStream[1];    // enum. 0 == text only, 1 == printf
        // uint32_t formatStringLen = tokenStream[2]; // length NOT including null terminator
        retOp.operands.resize(tokenStream[3]);
        // uint32_t operandDwordLen = tokenStream[4];

        tokenStream += 5;

        for(uint32_t i = 0; i < retOp.operands.size(); i++)
        {
          bool ret = DecodeOperand(tokenStream, flags, retOp.operands[i]);
          RDCASSERT(ret);
        }

        rdcstr formatString = (char *)&tokenStream[0];

        // escape any newlines
        int32_t nl = formatString.find("\n");
        while(nl >= 0)
        {
          formatString[nl] = '\\';
          formatString.insert(nl + 1, 'n');
          nl = formatString.find("\n", nl);
        }

        retOp.str = (messageFormat ? "errorf" : "error");
        retOp.str += " \"" + formatString + "\"";

        for(uint32_t i = 0; i < retOp.operands.size(); i++)
        {
          retOp.str += ", ";
          retOp.str += retOp.operands[i].toString(m_Reflection, flags | ToString::ShowSwizzle);
        }

        tokenStream = end;

        break;
      }

      default:
      {
        // handle as declaration
        tokenStream = begin;
        return false;
      }
    }

    return true;
  }

  tokenStream++;

  retOp.str = ToStr(op);

  while(extended)
  {
    uint32_t OpcodeTokenN = tokenStream[0];

    ExtendedOpcodeType type = ExtendedOpcode::Type.Get(OpcodeTokenN);

    if(type == EXTENDED_OPCODE_SAMPLE_CONTROLS)
    {
      retOp.texelOffset[0] = ExtendedOpcode::TexelOffsetU.Get(OpcodeTokenN);
      retOp.texelOffset[1] = ExtendedOpcode::TexelOffsetV.Get(OpcodeTokenN);
      retOp.texelOffset[2] = ExtendedOpcode::TexelOffsetW.Get(OpcodeTokenN);

      // apply 4-bit two's complement as per spec
      if(retOp.texelOffset[0] > 7)
        retOp.texelOffset[0] -= 16;
      if(retOp.texelOffset[1] > 7)
        retOp.texelOffset[1] -= 16;
      if(retOp.texelOffset[2] > 7)
        retOp.texelOffset[2] -= 16;

      retOp.str += StringFormat::Fmt("(%d,%d,%d)", retOp.texelOffset[0], retOp.texelOffset[1],
                                     retOp.texelOffset[2]);
    }
    else if(type == EXTENDED_OPCODE_RESOURCE_DIM)
    {
      retOp.resDim = ExtendedOpcode::ResourceDim.Get(OpcodeTokenN);

      if(op == OPCODE_LD_STRUCTURED)
      {
        retOp.stride = ExtendedOpcode::BufferStride.Get(OpcodeTokenN);

        retOp.str += StringFormat::Fmt("_indexable(%s, stride=%u)", ToStr(retOp.resDim).c_str(),
                                       retOp.stride);
      }
      else
      {
        retOp.str += "(";
        retOp.str += ToStr(retOp.resDim);
        retOp.str += ")";
      }
    }
    else if(type == EXTENDED_OPCODE_RESOURCE_RETURN_TYPE)
    {
      retOp.resType[0] = ExtendedOpcode::ReturnTypeX.Get(OpcodeTokenN);
      retOp.resType[1] = ExtendedOpcode::ReturnTypeY.Get(OpcodeTokenN);
      retOp.resType[2] = ExtendedOpcode::ReturnTypeZ.Get(OpcodeTokenN);
      retOp.resType[3] = ExtendedOpcode::ReturnTypeW.Get(OpcodeTokenN);

      retOp.str += "(";
      retOp.str += ToStr(retOp.resType[0]);
      retOp.str += ",";
      retOp.str += ToStr(retOp.resType[1]);
      retOp.str += ",";
      retOp.str += ToStr(retOp.resType[2]);
      retOp.str += ",";
      retOp.str += ToStr(retOp.resType[3]);
      retOp.str += ")";
    }

    extended = ExtendedOpcode::Extended.Get(OpcodeTokenN) == 1;

    tokenStream++;
  }

  if(op == OPCODE_RESINFO)
  {
    retOp.str += "_";
    retOp.str += ToStr(retOp.resinfoRetType);
  }

  if(op == OPCODE_SYNC)
  {
    if(Opcode::Sync_UAV_Global.Get(retOp.syncFlags))
    {
      retOp.str += "_uglobal";
    }
    if(Opcode::Sync_UAV_Group.Get(retOp.syncFlags))
    {
      retOp.str += "_ugroup";
    }
    if(Opcode::Sync_TGSM.Get(retOp.syncFlags))
    {
      retOp.str += "_g";
    }
    if(Opcode::Sync_Threads.Get(retOp.syncFlags))
    {
      retOp.str += "_t";
    }
  }

  uint32_t func = 0;
  if(op == OPCODE_INTERFACE_CALL)
  {
    func = tokenStream[0];
    tokenStream++;
  }

  retOp.operands.resize(NumOperands(op));

  for(size_t i = 0; i < retOp.operands.size(); i++)
  {
    bool ret = DecodeOperand(tokenStream, flags, retOp.operands[i]);
    RDCASSERT(ret);
  }

  if(op == OPCODE_INTERFACE_CALL)
  {
    retOp.operands[0].funcNum = func;
  }

  if(op == OPCODE_IF || op == OPCODE_BREAKC || op == OPCODE_CALLC || op == OPCODE_CONTINUEC ||
     op == OPCODE_RETC || op == OPCODE_DISCARD)
    retOp.str += retOp.nonzero ? "_nz" : "_z";

  if(op != OPCODE_SYNC)
  {
    retOp.str += retOp.saturate ? "_sat" : "";
  }

  if(retOp.preciseValues)
  {
    rdcstr preciseStr;
    if(retOp.preciseValues & 0x1)
      preciseStr += "x";
    if(retOp.preciseValues & 0x2)
      preciseStr += "y";
    if(retOp.preciseValues & 0x4)
      preciseStr += "z";
    if(retOp.preciseValues & 0x8)
      preciseStr += "w";

    retOp.str += StringFormat::Fmt(" [precise(%s)] ", preciseStr.c_str());
  }

  for(size_t i = 0; i < retOp.operands.size(); i++)
  {
    if(i == 0)
      retOp.str += " ";
    else
      retOp.str += ", ";
    retOp.str += retOp.operands[i].toString(m_Reflection, flags | ToString::ShowSwizzle);
  }

#if ENABLED(RDOC_DEVEL)
  if((uint32_t)(tokenStream - begin) > retOp.length)
  {
    RDCERR("Consumed too many tokens for %d!", retOp.operation);

    // try to recover by rewinding the stream, this instruction will be garbage but at least the
    // next ones will be correct
    uint32_t overread = (uint32_t)(tokenStream - begin) - retOp.length;
    tokenStream -= overread;
  }
  else if((uint32_t)(tokenStream - begin) < retOp.length)
  {
    // sometimes this just happens, which is why we only print this in non-release so we can
    // inspect it. There's probably not much we can do though, it's just magic.
    RDCWARN("Consumed too few tokens for %d!", retOp.operation);
    uint32_t missing = retOp.length - (uint32_t)(tokenStream - begin);
    for(uint32_t i = 0; i < missing; i++)
    {
      RDCLOG("missing token %d: 0x%08x", i, tokenStream[0]);
      tokenStream++;
    }
  }

  // make sure we consumed all uint32s
  RDCASSERT((uint32_t)(tokenStream - begin) == retOp.length);
#else
  // there's no good documentation for this, we're freewheeling blind in a nightmarish hellscape.
  // Instead of assuming we can predictably decode the whole of every opcode, just advance by the
  // defined length.
  tokenStream = begin + retOp.length;
#endif

  return true;
}

};    // namespace DXBCBytecode
