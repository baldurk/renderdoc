/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
    if(voffs <= minOffset && voffs + v.type.bytesize > maxOffset)
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
  if((flags & (FLAG_ABS | FLAG_NEG)) != (o.flags & (FLAG_ABS | FLAG_NEG)))
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

bool Operand::operator<(const Operand &o) const
{
  if(type != o.type)
    return type < o.type;
  if(numComponents != o.numComponents)
    return numComponents < o.numComponents;
  int c = memcmp(comps, o.comps, 4);
  if(c != 0)
    return c < 0;
  if((flags & (FLAG_ABS | FLAG_NEG)) != (o.flags & (FLAG_ABS | FLAG_NEG)))
    return (flags & (FLAG_ABS | FLAG_NEG)) < (o.flags & (FLAG_ABS | FLAG_NEG));

  if(indices != o.indices)
    return indices < o.indices;

  for(size_t i = 0; i < 4; i++)
    if(values[i] != o.values[i])
      return values[i] < o.values[i];

  return false;
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

rdcstr Operand::toString(const DXBC::Reflection *reflection, ToString toStrFlags) const
{
  rdcstr str, regstr;

  const bool decl = toStrFlags & ToString::IsDecl;
  const bool swizzle = toStrFlags & ToString::ShowSwizzle;
  const bool friendly = toStrFlags & ToString::FriendlyNameRegisters;

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

    str = StringFormat::Fmt("fp%s[%s][%u]", indices[0].toString(reflection, toStrFlags).c_str(),
                            indices[1].toString(reflection, toStrFlags).c_str(), values[0]);
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

      str += indices[0].toString(reflection, toStrFlags);

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
        str += indices[0].toString(reflection, toStrFlags);
      }
      else
      {
        if(indices[2].index == 0xffffffff)
          str += StringFormat::Fmt("%s[%s:unbound]",
                                   indices[0].toString(reflection, toStrFlags).c_str(),
                                   indices[1].toString(reflection, toStrFlags).c_str());
        else
          str += StringFormat::Fmt("%s[%s:%s]", indices[0].toString(reflection, toStrFlags).c_str(),
                                   indices[1].toString(reflection, toStrFlags).c_str(),
                                   indices[2].toString(reflection, toStrFlags).c_str());
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
        str += indices[0].toString(reflection, toStrFlags);
      }
      else
      {
        if(indices[1].relative)
          str += StringFormat::Fmt("%s%s", indices[0].toString(reflection, toStrFlags).c_str(),
                                   indices[1].toString(reflection, toStrFlags).c_str());
        else
          str += StringFormat::Fmt("%s[%s]", indices[0].toString(reflection, toStrFlags).c_str(),
                                   indices[1].toString(reflection, toStrFlags).c_str());
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
            str += StringFormat::Fmt("%s%s", indices[0].toString(reflection, toStrFlags).c_str(),
                                     indices[2].toString(reflection, toStrFlags).c_str());
          else
            str += StringFormat::Fmt("%s[%s]", indices[0].toString(reflection, toStrFlags).c_str(),
                                     indices[2].toString(reflection, toStrFlags).c_str());
        }
        else
        {
          str += indices[0].toString(reflection, toStrFlags);

          if(indices[1].relative)
            str += indices[1].toString(reflection, toStrFlags);
          else
            str += "[" + indices[1].toString(reflection, toStrFlags) + "]";

          if(indices[2].relative)
            str += indices[1].toString(reflection, toStrFlags);
          else
            str += "[" + indices[2].toString(reflection, toStrFlags) + "]";
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
          str += indices[0].toString(reflection, toStrFlags);
        }
        else
        {
          if(indices[2].index == 0xffffffff)
            str += StringFormat::Fmt("%s[%s:unbound]",
                                     indices[0].toString(reflection, toStrFlags).c_str(),
                                     indices[1].toString(reflection, toStrFlags).c_str());
          else
            str += StringFormat::Fmt("%s[%s:%s]", indices[0].toString(reflection, toStrFlags).c_str(),
                                     indices[1].toString(reflection, toStrFlags).c_str(),
                                     indices[2].toString(reflection, toStrFlags).c_str());
        }
      }
    }
    else
    {
      str = "cb";

      if(indices[1].relative)
        str += StringFormat::Fmt("%s%s", indices[0].toString(reflection, toStrFlags).c_str(),
                                 indices[1].toString(reflection, toStrFlags).c_str());
      else
        str += StringFormat::Fmt("%s[%s]", indices[0].toString(reflection, toStrFlags).c_str(),
                                 indices[1].toString(reflection, toStrFlags).c_str());

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

        if(cbuffer && decl)
        {
          // for declarations don't look up the variable name. This actually lists the size in
          // float4s of the constant buffer, and in the case of dead code elimination there could be
          // variables past that point which have been removed which we'd find

          if(!cbuffer->name.empty())
            str += " (" + cbuffer->name + ")";
        }
        else if(cbuffer)
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
              if(var->type.elements > 1)
              {
                uint32_t byteSize = var->type.bytesize;

                // round up the byte size to a the nearest vec4 in case it's not quite a multiple
                byteSize = AlignUp16(byteSize);

                const uint32_t elementSize = byteSize / var->type.elements;

                const uint32_t elementIndex = varOffset / elementSize;

                str += StringFormat::Fmt("[%u]", elementIndex);

                // subtract off so that if there's any further offset, it can be processed
                varOffset -= elementIndex;
              }

              // or if it's a matrix
              if((var->type.varClass == DXBC::CLASS_MATRIX_ROWS && var->type.cols > 1) ||
                 (var->type.varClass == DXBC::CLASS_MATRIX_COLUMNS && var->type.rows > 1))
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

    str += indices[0].toString(reflection, toStrFlags);
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
      str += indices[0].toString(reflection, toStrFlags);
    }
    else
    {
      for(size_t i = 0; i < indices.size(); i++)
      {
        if(i == 0 && type == TYPE_INDEXABLE_TEMP)
        {
          str += indices[i].toString(reflection, toStrFlags);
          continue;
        }

        if(indices[i].relative)
          str += indices[i].toString(reflection, toStrFlags);
        else
          str += "[" + indices[i].toString(reflection, toStrFlags) + "]";
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
  else if(type == TYPE_INNER_COVERAGE)
    str = "vInnerCoverage";
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

  if(flags & FLAG_ABS)
    str = "abs(" + str + ")";
  if(flags & FLAG_NEG)
    str = "-" + str;

  if(decl && !regstr.empty())
    str += StringFormat::Fmt(" (%s)", regstr.c_str());

  if(flags & FLAG_NONUNIFORM)
    str += " { nonuniform }";

  if(!name.empty())
  {
    rdcstr n = name;
    str = n + "=" + str;
  }

  return str;
}

rdcstr RegIndex::toString(const DXBC::Reflection *reflection, ToString toStrFlags) const
{
  if(relative)
  {
    return StringFormat::Fmt(
        "[%s + %llu]", operand.toString(reflection, toStrFlags | ToString::ShowSwizzle).c_str(),
        index);
  }
  else
  {
    return ToStr(index);
  }
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

    op.offset = offset * sizeof(uint32_t);

    if(!DecodeOperation(cur, op, friendly))
    {
      if(!DecodeDecl(cur, decl, friendly))
      {
        RDCERR("Unexpected non-operation and non-decl in token stream at 0x%x", cur - begin);
      }
      else
      {
        // once we have a set of late declarations, push it into the most recent one. Otherwise
        if(m_LateDeclarations.empty())
          m_Declarations.push_back(decl);
        else
          m_LateDeclarations.back().push_back(decl);
      }
    }
    else
    {
      m_Instructions.push_back(op);

      // each HS phase, start a new set of late declarations
      if(op.operation == OPCODE_HS_CONTROL_POINT_PHASE || op.operation == OPCODE_HS_FORK_PHASE ||
         op.operation == OPCODE_HS_JOIN_PHASE)
        m_LateDeclarations.push_back({});
    }
  }

  RDCASSERT(m_Declarations.size() <= numDecls);

  OpcodeType lastRealOp = NUM_REAL_OPCODES;

  for(size_t i = 0; i < m_Instructions.size(); i++)
  {
    OpcodeType o = m_Instructions[m_Instructions.size() - i - 1].operation;
    if(o == OPCODE_CUSTOMDATA || o == OPCODE_OPAQUE_CUSTOMDATA)
      continue;

    lastRealOp = o;
    break;
  }

  if(lastRealOp != OPCODE_RET)
  {
    Operation implicitRet;
    implicitRet.offset = (end - begin) * sizeof(uint32_t);
    implicitRet.operation = OPCODE_RET;
    implicitRet.str = "ret";

    m_Instructions.push_back(implicitRet);
  }

  if(DXBC_Disassembly_ProcessVendorShaderExts() && m_ShaderExt.second != ~0U)
    PostprocessVendorExtensions();
}

rdcarray<uint32_t> Program::EncodeProgram()
{
  rdcarray<uint32_t> tokenStream;

  tokenStream.push_back(0U);
  tokenStream.push_back(0U);

  VersionToken::ProgramType.Set(tokenStream[0], m_Type);
  VersionToken::MajorVersion.Set(tokenStream[0], m_Major);
  VersionToken::MinorVersion.Set(tokenStream[0], m_Minor);

  for(size_t d = 0; d < m_Declarations.size(); d++)
    EncodeDecl(tokenStream, m_Declarations[d]);

  size_t lt = 0;

  for(size_t i = 0; i < m_Instructions.size(); i++)
  {
    EncodeOperation(tokenStream, m_Instructions[i]);

    if(m_Instructions[i].operation == OPCODE_HS_CONTROL_POINT_PHASE ||
       m_Instructions[i].operation == OPCODE_HS_FORK_PHASE ||
       m_Instructions[i].operation == OPCODE_HS_JOIN_PHASE)
    {
      if(lt < m_LateDeclarations.size())
      {
        for(size_t d = 0; d < m_LateDeclarations[lt].size(); d++)
          EncodeDecl(tokenStream, m_LateDeclarations[lt][d]);

        lt++;
      }
    }
  }

  LengthToken::Length.Set(tokenStream[1], (uint32_t)tokenStream.size());

  return tokenStream;
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

  LineColumnInfo prevLineInfo;
  rdcarray<rdcstr> prevCallstack;

  size_t debugInst = 0;

  rdcarray<rdcarray<rdcstr>> fileLines;

  // generate fileLines by splitting each file in the debug info
  if(m_DebugInfo)
  {
    fileLines.resize(m_DebugInfo->Files.size());

    for(size_t i = 0; i < m_DebugInfo->Files.size(); i++)
      split(m_DebugInfo->Files[i].contents, fileLines[i], '\n');
  }

  for(size_t d = 0; d < m_Declarations.size(); d++)
  {
    m_Disassembly += StringFormat::Fmt("% 4s  %s\n", "", m_Declarations[d].str.c_str());
    linenum++;

    int32_t nl = m_Declarations[d].str.indexOf('\n');
    while(nl >= 0)
    {
      linenum++;
      nl = m_Declarations[d].str.indexOf('\n', nl + 1);
    }
  }

  size_t lt = 0;

  for(size_t i = 0; i < m_Instructions.size(); i++)
  {
    if(m_Instructions[i].operation == OPCODE_CUSTOMDATA ||
       m_Instructions[i].operation == OPCODE_OPAQUE_CUSTOMDATA)
    {
      continue;
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
            m_Disassembly += StringFormat::Fmt(
                "%s:%d - %s()\n", m_DebugInfo->Files[lineInfo.fileIndex].filename.c_str(),
                lineInfo.lineStart, func.c_str());
            linenum++;
          }
          else
          {
            m_Disassembly +=
                StringFormat::Fmt("%s:%d\n", m_DebugInfo->Files[lineInfo.fileIndex].filename.c_str(),
                                  lineInfo.lineStart);
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
    {
      debugInst++;
    }
    else
    {
      if(lt < m_LateDeclarations.size())
      {
        for(size_t d = 0; d < m_LateDeclarations[lt].size(); d++)
        {
          m_Disassembly += StringFormat::Fmt("% 4s  %s\n", "", m_LateDeclarations[lt][d].str.c_str());
          linenum++;

          int32_t nl = m_LateDeclarations[lt][d].str.indexOf('\n');
          while(nl >= 0)
          {
            linenum++;
            nl = m_LateDeclarations[lt][d].str.indexOf('\n', nl + 1);
          }
        }

        lt++;
      }
    }
  }
}

void Program::EncodeOperand(rdcarray<uint32_t> &tokenStream, const Operand &oper)
{
  uint32_t OperandToken0 = 0;

  Oper::Type.Set(OperandToken0, oper.type);
  Oper::NumComponents.Set(OperandToken0, oper.numComponents);

  if(oper.flags & Operand::FLAG_SELECTED)
  {
    Oper::SelectionMode.Set(OperandToken0, SELECTION_SELECT_1);

    Oper::ComponentSel1.Set(OperandToken0, oper.comps[0]);
  }
  else if(oper.flags & Operand::FLAG_MASKED)
  {
    Oper::SelectionMode.Set(OperandToken0, SELECTION_MASK);

    for(int i = 0; i < 4; i++)
    {
      if(oper.comps[i] == 0)
        Oper::ComponentMaskX.Set(OperandToken0, true);
      if(oper.comps[i] == 1)
        Oper::ComponentMaskY.Set(OperandToken0, true);
      if(oper.comps[i] == 2)
        Oper::ComponentMaskZ.Set(OperandToken0, true);
      if(oper.comps[i] == 3)
        Oper::ComponentMaskW.Set(OperandToken0, true);
    }
  }
  else
  {
    Oper::SelectionMode.Set(OperandToken0, SELECTION_SWIZZLE);

    Oper::ComponentSwizzleX.Set(OperandToken0, oper.comps[0]);
    Oper::ComponentSwizzleY.Set(OperandToken0, oper.comps[1]);
    Oper::ComponentSwizzleZ.Set(OperandToken0, oper.comps[2]);
    Oper::ComponentSwizzleW.Set(OperandToken0, oper.comps[3]);
  }

  Oper::IndexDimension.Set(OperandToken0, (uint32_t)oper.indices.size());

  OperandIndexType rep[3] = {};

  for(size_t i = 0; i < oper.indices.size(); i++)
  {
    OperandIndexType indextype = INDEX_IMMEDIATE32;

    if(oper.indices[i].relative)
    {
      if(oper.indices[i].absolute)
        indextype = INDEX_IMMEDIATE32_PLUS_RELATIVE;
      else
        indextype = INDEX_RELATIVE;
    }

    // technically this means immediate64 with indices that can be truncated changes, but we don't
    // expect fxc to emit that (I've never seen it emit immediate64).
    // immediate64 types come after coresponding immediate32 types
    RDCCOMPILE_ASSERT(INDEX_IMMEDIATE32 + 1 == INDEX_IMMEDIATE64, "enums aren't in order expected");
    RDCCOMPILE_ASSERT(INDEX_IMMEDIATE32_PLUS_RELATIVE + 1 == INDEX_IMMEDIATE64_PLUS_RELATIVE,
                      "enums aren't in order expected");

    if((oper.indices[i].index & 0xFFFFFFFFULL) != oper.indices[i].index)
      indextype = OperandIndexType(indextype + 1);

    rep[i] = indextype;

    if(i == 0)
      Oper::Index0.Set(OperandToken0, indextype);
    if(i == 1)
      Oper::Index1.Set(OperandToken0, indextype);
    if(i == 2)
      Oper::Index2.Set(OperandToken0, indextype);
  }

  bool extended = false;
  if(oper.flags & Operand::FLAG_EXTENDED)
  {
    Oper::Extended.Set(OperandToken0, true);
    extended = true;
  }

  tokenStream.push_back(OperandToken0);

  if(extended)
  {
    uint32_t ExtendedToken = 0;
    ExtendedOperand::Type.Set(ExtendedToken, EXTENDED_OPERAND_MODIFIER);

    if((oper.flags & (Operand::FLAG_ABS | Operand::FLAG_NEG)) != 0)
      ExtendedOperand::Modifier.Set(
          ExtendedToken, OperandModifier(oper.flags & (Operand::FLAG_ABS | Operand::FLAG_NEG)));
    if(oper.precision != PRECISION_DEFAULT)
      ExtendedOperand::MinPrecision.Set(ExtendedToken, oper.precision);
    if(oper.flags & Operand::FLAG_NONUNIFORM)
      ExtendedOperand::NonUniform.Set(ExtendedToken, true);

    tokenStream.push_back(ExtendedToken);
  }

  // immediates now have 1 or 4 values
  if(oper.type == TYPE_IMMEDIATE32 || oper.type == TYPE_IMMEDIATE64)
  {
    RDCASSERT(oper.indices.empty());

    uint32_t numComps = 1;

    if(oper.numComponents == NUMCOMPS_1)
      numComps = 1;
    else if(oper.numComponents == NUMCOMPS_4)
      numComps = 4;
    else
      RDCERR("N-wide vectors not supported.");

    tokenStream.append(oper.values, numComps);
  }

  // now encode the indices
  for(size_t idx = 0; idx < oper.indices.size(); idx++)
  {
    // if there's an immediate, push it first
    if(rep[idx] == INDEX_IMMEDIATE32_PLUS_RELATIVE || rep[idx] == INDEX_IMMEDIATE32)
    {
      tokenStream.push_back(oper.indices[idx].index & 0xFFFFFFFFU);
    }
    else if(rep[idx] == INDEX_IMMEDIATE64_PLUS_RELATIVE || rep[idx] == INDEX_IMMEDIATE64)
    {
      // for 64-bit values, push the high value first
      tokenStream.push_back(oper.indices[idx].index >> 32U);
      tokenStream.push_back(oper.indices[idx].index & 0xFFFFFFFFU);
    }

    // if there's a relative component, push another operand here
    if(rep[idx] == INDEX_IMMEDIATE64_PLUS_RELATIVE || rep[idx] == INDEX_IMMEDIATE32_PLUS_RELATIVE ||
       rep[idx] == INDEX_RELATIVE)
    {
      EncodeOperand(tokenStream, oper.indices[idx].operand);
    }
  }
}

bool Program::DecodeOperand(uint32_t *&tokenStream, ToString flags, Operand &retOper)
{
  RDCCOMPILE_ASSERT(sizeof(Operand) <= 64, "Operand shouldn't increase in size");

  uint32_t OperandToken0 = tokenStream[0];

  retOper.type = Oper::Type.Get(OperandToken0);
  retOper.numComponents = Oper::NumComponents.Get(OperandToken0);

  SelectionMode selMode = Oper::SelectionMode.Get(OperandToken0);

  if(selMode == SELECTION_MASK)
  {
    retOper.flags = Operand::FLAG_MASKED;

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
    retOper.flags = Operand::FLAG_SWIZZLED;

    retOper.comps[0] = Oper::ComponentSwizzleX.Get(OperandToken0);
    retOper.comps[1] = Oper::ComponentSwizzleY.Get(OperandToken0);
    retOper.comps[2] = Oper::ComponentSwizzleZ.Get(OperandToken0);
    retOper.comps[3] = Oper::ComponentSwizzleW.Get(OperandToken0);
  }
  else if(selMode == SELECTION_SELECT_1)
  {
    retOper.flags = Operand::FLAG_SELECTED;

    retOper.comps[0] = Oper::ComponentSel1.Get(OperandToken0);
  }

  uint32_t indexDim = Oper::IndexDimension.Get(OperandToken0);

  OperandIndexType rep[] = {
      Oper::Index0.Get(OperandToken0),
      Oper::Index1.Get(OperandToken0),
      Oper::Index2.Get(OperandToken0),
  };

  bool extended = Oper::Extended.Get(OperandToken0);

  tokenStream++;

  while(extended)
  {
    uint32_t OperandTokenN = tokenStream[0];

    ExtendedOperandType type = ExtendedOperand::Type.Get(OperandTokenN);

    retOper.flags = Operand::Flags(retOper.flags | Operand::FLAG_EXTENDED);

    if(type == EXTENDED_OPERAND_MODIFIER)
    {
      retOper.flags = Operand::Flags(retOper.flags | ExtendedOperand::Modifier.Get(OperandTokenN));
      retOper.precision = ExtendedOperand::MinPrecision.Get(OperandTokenN);
      if(ExtendedOperand::NonUniform.Get(OperandTokenN))
        retOper.flags = Operand::Flags(retOper.flags | Operand::FLAG_NONUNIFORM);
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
  }

  if(retOper.type == TYPE_RESOURCE || retOper.type == TYPE_SAMPLER ||
     retOper.type == TYPE_UNORDERED_ACCESS_VIEW || retOper.type == TYPE_CONSTANT_BUFFER)
  {
    // shader linkage can access cbuffers with a relative first index
    if(retOper.indices[0].relative)
    {
      // ignore, we can't find the declaration
    }
    else
    {
      // try and find a declaration with a matching ID
      RDCASSERT(retOper.indices.size() > 0);
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
  }

  return true;
}

void Program::EncodeDecl(rdcarray<uint32_t> &tokenStream, const Declaration &decl)
{
  rdcarray<uint32_t> declStream;

  declStream.reserve(16);

  uint32_t OpcodeToken0 = 0;

  const bool sm51 = (m_Major == 0x5 && m_Minor == 0x1);

  Opcode::Type.Set(OpcodeToken0, decl.declaration);

  OpcodeType op = decl.declaration;

  if(op == OPCODE_DCL_IMMEDIATE_CONSTANT_BUFFER)
  {
    OpcodeToken0 = 0;
    Opcode::Type.Set(OpcodeToken0, OPCODE_CUSTOMDATA);
    Opcode::CustomClass.Set(OpcodeToken0, CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER);

    declStream.push_back(OpcodeToken0);

    // push overall length, including OpcodeToken0 and this length
    declStream.push_back((uint32_t)m_Immediate.size() + 2);

    declStream.append(m_Immediate);

    // append immediately and return, so we don't do any length fixup
    tokenStream.append(declStream);

    return;
  }
  else if(op == OPCODE_OPAQUE_CUSTOMDATA)
  {
    OpcodeToken0 = 0;
    Opcode::Type.Set(OpcodeToken0, OPCODE_CUSTOMDATA);
    Opcode::CustomClass.Set(OpcodeToken0, m_CustomDatas[decl.customDataIndex].first);

    declStream.push_back(OpcodeToken0);

    const rdcarray<uint32_t> &customData = m_CustomDatas[decl.customDataIndex].second;

    // push overall length, including OpcodeToken0 and this length
    declStream.push_back((uint32_t)customData.size() + 2);

    declStream.append(customData);

    // append immediately and return, so we don't do any length fixup
    tokenStream.append(declStream);

    return;
  }
  else if(op == OPCODE_CUSTOMDATA)
  {
    RDCERR("Unexpected raw customdata declaration");
    return;
  }
  else if(op == OPCODE_DCL_GLOBAL_FLAGS)
  {
    Decl::RefactoringAllowed.Set(OpcodeToken0, decl.global_flags.refactoringAllowed);
    Decl::DoubleFloatOps.Set(OpcodeToken0, decl.global_flags.doublePrecisionFloats);
    Decl::ForceEarlyDepthStencil.Set(OpcodeToken0, decl.global_flags.forceEarlyDepthStencil);
    Decl::EnableRawStructuredBufs.Set(OpcodeToken0, decl.global_flags.enableRawAndStructuredBuffers);
    Decl::SkipOptimisation.Set(OpcodeToken0, decl.global_flags.skipOptimisation);
    Decl::EnableMinPrecision.Set(OpcodeToken0, decl.global_flags.enableMinPrecision);
    Decl::EnableD3D11_1DoubleExtensions.Set(OpcodeToken0,
                                            decl.global_flags.enableD3D11_1DoubleExtensions);
    Decl::EnableD3D11_1ShaderExtensions.Set(OpcodeToken0,
                                            decl.global_flags.enableD3D11_1ShaderExtensions);
    Decl::EnableD3D12AllResourcesBound.Set(OpcodeToken0,
                                           decl.global_flags.enableD3D12AllResourcesBound);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_CONSTANT_BUFFER)
  {
    Decl::AccessPattern.Set(OpcodeToken0, decl.cbuffer.accessPattern);

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    if(sm51)
    {
      declStream.push_back(decl.cbuffer.vectorSize);
      declStream.push_back(decl.space);
    }
  }
  else if(op == OPCODE_DCL_INPUT)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);
  }
  else if(op == OPCODE_DCL_TEMPS)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.numTemps);
  }
  else if(op == OPCODE_DCL_INDEXABLE_TEMP)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.indexable_temp.tempReg);
    declStream.push_back(decl.indexable_temp.numTemps);
    declStream.push_back(decl.indexable_temp.tempComponentCount);
  }
  else if(op == OPCODE_DCL_OUTPUT)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);
  }
  else if(op == OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.maxVertexOutCount);
  }
  else if(op == OPCODE_DCL_INPUT_SIV || op == OPCODE_DCL_INPUT_SGV || op == OPCODE_DCL_OUTPUT_SIV ||
          op == OPCODE_DCL_OUTPUT_SGV)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    declStream.push_back((uint32_t)decl.inputOutput.systemValue);
  }
  else if(op == OPCODE_DCL_INPUT_PS || op == OPCODE_DCL_INPUT_PS_SIV || op == OPCODE_DCL_INPUT_PS_SGV)
  {
    // the (minimal) spec says this is 0 for SGV, but it's not and fxc displays the interp mode
    Decl::InterpolationMode.Set(OpcodeToken0, decl.inputOutput.inputInterpolation);

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    if(op == OPCODE_DCL_INPUT_PS_SIV || op == OPCODE_DCL_INPUT_PS_SGV)
      declStream.push_back((uint32_t)decl.inputOutput.systemValue);
  }
  else if(op == OPCODE_DCL_STREAM)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);
  }
  else if(op == OPCODE_DCL_SAMPLER)
  {
    Decl::SamplerMode.Set(OpcodeToken0, decl.samplerMode);

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    if(sm51)
      declStream.push_back(decl.space);
  }
  else if(op == OPCODE_DCL_RESOURCE)
  {
    Decl::ResourceDim.Set(OpcodeToken0, decl.resource.dim);

    if(decl.resource.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
       decl.resource.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
    {
      Decl::SampleCount.Set(OpcodeToken0, decl.resource.sampleCount);
    }

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    uint32_t ResourceReturnTypeToken = 0;

    Decl::ReturnTypeX.Set(ResourceReturnTypeToken, decl.resource.resType[0]);
    Decl::ReturnTypeY.Set(ResourceReturnTypeToken, decl.resource.resType[1]);
    Decl::ReturnTypeZ.Set(ResourceReturnTypeToken, decl.resource.resType[2]);
    Decl::ReturnTypeW.Set(ResourceReturnTypeToken, decl.resource.resType[3]);

    declStream.push_back(ResourceReturnTypeToken);

    if(sm51)
      declStream.push_back(decl.space);
  }
  else if(op == OPCODE_DCL_INDEX_RANGE)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    declStream.push_back(decl.indexRange);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP)
  {
    declStream.push_back(OpcodeToken0);

    declStream.push_back(decl.groupSize[0]);
    declStream.push_back(decl.groupSize[1]);
    declStream.push_back(decl.groupSize[2]);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    declStream.push_back(decl.tgsmCount);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED)
  {
    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    declStream.push_back(decl.tsgm_structured.stride);
    declStream.push_back(decl.tsgm_structured.count);
  }
  else if(op == OPCODE_DCL_INPUT_CONTROL_POINT_COUNT || op == OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT)
  {
    Decl::ControlPointCount.Set(OpcodeToken0, decl.controlPointCount);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_TESS_DOMAIN)
  {
    Decl::TessDomain.Set(OpcodeToken0, decl.tessDomain);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_TESS_PARTITIONING)
  {
    Decl::TessPartitioning.Set(OpcodeToken0, decl.tessPartition);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_GS_INPUT_PRIMITIVE)
  {
    Decl::InputPrimitive.Set(OpcodeToken0, decl.geomInputPrimitive);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
  {
    Decl::OutputPrimitiveTopology.Set(OpcodeToken0, decl.geomOutputTopology);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_TESS_OUTPUT_PRIMITIVE)
  {
    Decl::OutputPrimitive.Set(OpcodeToken0, decl.tessOutputPrimitive);

    declStream.push_back(OpcodeToken0);
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW || op == OPCODE_DCL_RESOURCE_RAW)
  {
    if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW)
    {
      Decl::RasterizerOrderedAccess.Set(OpcodeToken0, decl.raw.rov);
      Decl::GloballyCoherent.Set(OpcodeToken0, decl.raw.globallyCoherant);
    }

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    if(sm51)
      declStream.push_back(decl.space);
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED || op == OPCODE_DCL_RESOURCE_STRUCTURED)
  {
    if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED)
    {
      Decl::HasOrderPreservingCounter.Set(OpcodeToken0, decl.structured.hasCounter);
      Decl::RasterizerOrderedAccess.Set(OpcodeToken0, decl.structured.rov);
      Decl::GloballyCoherent.Set(OpcodeToken0, decl.structured.globallyCoherant);
    }

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    declStream.push_back(decl.structured.stride);

    if(sm51)
      declStream.push_back(decl.space);
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED)
  {
    Decl::ResourceDim.Set(OpcodeToken0, decl.uav_typed.dim);
    Decl::GloballyCoherent.Set(OpcodeToken0, decl.uav_typed.globallyCoherant);
    Decl::RasterizerOrderedAccess.Set(OpcodeToken0, decl.uav_typed.rov);

    declStream.push_back(OpcodeToken0);

    EncodeOperand(declStream, decl.operand);

    uint32_t ResourceReturnTypeToken = 0;

    Decl::ReturnTypeX.Set(ResourceReturnTypeToken, decl.uav_typed.resType[0]);
    Decl::ReturnTypeY.Set(ResourceReturnTypeToken, decl.uav_typed.resType[1]);
    Decl::ReturnTypeZ.Set(ResourceReturnTypeToken, decl.uav_typed.resType[2]);
    Decl::ReturnTypeW.Set(ResourceReturnTypeToken, decl.uav_typed.resType[3]);

    declStream.push_back(ResourceReturnTypeToken);

    if(sm51)
      declStream.push_back(decl.space);
  }
  else if(op == OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT ||
          op == OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT || op == OPCODE_DCL_GS_INSTANCE_COUNT)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.instanceCount);
  }
  else if(op == OPCODE_DCL_HS_MAX_TESSFACTOR)
  {
    declStream.push_back(OpcodeToken0);

    uint32_t token;
    memcpy(&token, &decl.maxTessFactor, sizeof(token));
    declStream.push_back(token);
  }
  else if(op == OPCODE_DCL_FUNCTION_BODY)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.functionBody);
  }
  else if(op == OPCODE_DCL_FUNCTION_TABLE)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.functionTable);

    declStream.push_back((uint32_t)decl.functionTableContents.size());

    declStream.append(decl.functionTableContents);
  }
  else if(op == OPCODE_DCL_INTERFACE)
  {
    declStream.push_back(OpcodeToken0);
    declStream.push_back(decl.iface.interfaceID);
    declStream.push_back(decl.iface.numTypes);

    uint32_t CountToken = 0;
    Decl::NumInterfaces.Set(CountToken, decl.iface.numInterfaces);
    Decl::TableLength.Set(CountToken, (uint32_t)decl.functionTableContents.size());
    declStream.push_back(CountToken);

    declStream.append(decl.functionTableContents);
  }
  else if(op == OPCODE_HS_DECLS)
  {
    declStream.push_back(OpcodeToken0);
  }
  else
  {
    RDCERR("Unexpected opcode decl %d", op);
  }

  // fixup the declaration now and append
  Opcode::Length.Set(declStream[0], (uint32_t)declStream.size());
  tokenStream.append(declStream);
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

  retDecl.declaration = op;

  if(op == OPCODE_CUSTOMDATA)
  {
    CustomDataClass customClass = Opcode::CustomClass.Get(OpcodeToken0);

    tokenStream++;
    // DWORD length including OpcodeToken0 and this length token
    uint32_t customDataLength = tokenStream[0];
    tokenStream++;

    uint32_t dataLength = customDataLength - 2;

    RDCASSERT(customDataLength >= 2);

    switch(customClass)
    {
      case CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER:
      {
        retDecl.declaration = OPCODE_DCL_IMMEDIATE_CONSTANT_BUFFER;
        retDecl.str = "dcl_immediateConstantBuffer {";

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
        // add this as an opaque declaration
        retDecl.declaration = OPCODE_OPAQUE_CUSTOMDATA;

        retDecl.customDataIndex = (uint32_t)m_CustomDatas.size();

        m_CustomDatas.push_back(make_rdcpair(customClass, rdcarray<uint32_t>()));
        m_CustomDatas.back().second.assign(tokenStream, dataLength);

        // unsupported custom data should be treated in operation as it's first
        RDCWARN("Unsupported custom data class %d treated as declaration", customClass);
        tokenStream += dataLength;
      }
    }

    return true;
  }

  uint32_t declLength = Opcode::Length.Get(OpcodeToken0);

  tokenStream++;

  retDecl.str = ToStr(op);

  if(op == OPCODE_DCL_GLOBAL_FLAGS)
  {
    retDecl.global_flags.refactoringAllowed = Decl::RefactoringAllowed.Get(OpcodeToken0);
    retDecl.global_flags.doublePrecisionFloats = Decl::DoubleFloatOps.Get(OpcodeToken0);
    retDecl.global_flags.forceEarlyDepthStencil = Decl::ForceEarlyDepthStencil.Get(OpcodeToken0);
    retDecl.global_flags.enableRawAndStructuredBuffers =
        Decl::EnableRawStructuredBufs.Get(OpcodeToken0);
    retDecl.global_flags.skipOptimisation = Decl::SkipOptimisation.Get(OpcodeToken0);
    retDecl.global_flags.enableMinPrecision = Decl::EnableMinPrecision.Get(OpcodeToken0);
    retDecl.global_flags.enableD3D11_1DoubleExtensions =
        Decl::EnableD3D11_1DoubleExtensions.Get(OpcodeToken0);
    retDecl.global_flags.enableD3D11_1ShaderExtensions =
        Decl::EnableD3D11_1ShaderExtensions.Get(OpcodeToken0);
    retDecl.global_flags.enableD3D12AllResourcesBound =
        Decl::EnableD3D12AllResourcesBound.Get(OpcodeToken0);

    retDecl.str += " ";

    bool added = false;

    if(retDecl.global_flags.refactoringAllowed)
    {
      retDecl.str += "refactoringAllowed";
      added = true;
    }
    if(retDecl.global_flags.doublePrecisionFloats)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "doublePrecisionFloats";
      added = true;
    }
    if(retDecl.global_flags.forceEarlyDepthStencil)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "forceEarlyDepthStencil";
      added = true;
    }
    if(retDecl.global_flags.enableRawAndStructuredBuffers)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "enableRawAndStructuredBuffers";
      added = true;
    }
    if(retDecl.global_flags.skipOptimisation)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "skipOptimisation";
      added = true;
    }
    if(retDecl.global_flags.enableMinPrecision)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "enableMinPrecision";
      added = true;
    }
    if(retDecl.global_flags.enableD3D11_1DoubleExtensions)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "doubleExtensions";
      added = true;
    }
    if(retDecl.global_flags.enableD3D11_1ShaderExtensions)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "shaderExtensions";
      added = true;
    }
    if(retDecl.global_flags.enableD3D12AllResourcesBound)
    {
      if(added)
        retDecl.str += ", ";
      retDecl.str += "d3d12AllResourcesBound";
      added = true;
    }
  }
  else if(op == OPCODE_DCL_CONSTANT_BUFFER)
  {
    retDecl.cbuffer.accessPattern = Decl::AccessPattern.Get(OpcodeToken0);

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    if(sm51)
    {
      // Store the size provided. If there's no reflection data, this will be
      // necessary to guess the buffer size properly
      retDecl.cbuffer.vectorSize = tokenStream[0];
      tokenStream++;

      retDecl.str += StringFormat::Fmt("[%u]", retDecl.cbuffer.vectorSize);
    }

    retDecl.str += ", ";

    if(retDecl.cbuffer.accessPattern == ACCESS_IMMEDIATE_INDEXED)
      retDecl.str += "immediateIndexed";
    else if(retDecl.cbuffer.accessPattern == ACCESS_DYNAMIC_INDEXED)
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
        retDecl.str += StringFormat::Fmt(",regs=%u:unbounded", retDecl.operand.indices[1].index);
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
    retDecl.indexable_temp.tempReg = tokenStream[0];
    tokenStream++;

    retDecl.indexable_temp.numTemps = tokenStream[0];
    tokenStream++;

    retDecl.indexable_temp.tempComponentCount = tokenStream[0];
    tokenStream++;

    // I don't think the compiler will ever declare a non-compact list of indexable temps, but just
    // to be sure our indexing works let's be safe.
    if(retDecl.indexable_temp.tempReg >= m_IndexTempSizes.size())
      m_IndexTempSizes.resize(retDecl.indexable_temp.tempReg + 1);
    m_IndexTempSizes[retDecl.indexable_temp.tempReg] = retDecl.indexable_temp.numTemps;

    retDecl.str += StringFormat::Fmt(" x%u[%u], %u", retDecl.indexable_temp.tempReg,
                                     retDecl.indexable_temp.numTemps,
                                     retDecl.indexable_temp.tempComponentCount);
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

    retDecl.maxVertexOutCount = tokenStream[0];

    tokenStream++;

    retDecl.str += StringFormat::Fmt(" %u", retDecl.maxVertexOutCount);
  }
  else if(op == OPCODE_DCL_INPUT_SIV || op == OPCODE_DCL_INPUT_SGV || op == OPCODE_DCL_OUTPUT_SIV ||
          op == OPCODE_DCL_OUTPUT_SGV)
  {
    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.inputOutput.systemValue = (DXBC::SVSemantic)tokenStream[0];
    tokenStream++;

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);

    retDecl.str += ", ";
    retDecl.str += ToStr(retDecl.inputOutput.systemValue);
  }
  else if(op == OPCODE_DCL_INPUT_PS || op == OPCODE_DCL_INPUT_PS_SIV || op == OPCODE_DCL_INPUT_PS_SGV)
  {
    // the (minimal) spec says this is 0 for SGV, but it's not and fxc displays the interp mode
    retDecl.inputOutput.inputInterpolation = Decl::InterpolationMode.Get(OpcodeToken0);

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    if(op == OPCODE_DCL_INPUT_PS_SIV || op == OPCODE_DCL_INPUT_PS_SGV)
    {
      retDecl.inputOutput.systemValue = (DXBC::SVSemantic)tokenStream[0];
      tokenStream++;
    }

    retDecl.str += " ";
    retDecl.str += ToStr(retDecl.inputOutput.inputInterpolation);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);

    if(op == OPCODE_DCL_INPUT_PS_SIV || op == OPCODE_DCL_INPUT_PS_SGV)
    {
      retDecl.str += ", ";
      retDecl.str += ToStr(retDecl.inputOutput.systemValue);
    }
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
      else if(retDecl.operand.indices[2].index == 0xffffffff)
        retDecl.str += StringFormat::Fmt(",regs=%u:unbounded", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_RESOURCE)
  {
    retDecl.resource.dim = Decl::ResourceDim.Get(OpcodeToken0);

    retDecl.resource.sampleCount = 0;
    if(retDecl.resource.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
       retDecl.resource.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
    {
      retDecl.resource.sampleCount = Decl::SampleCount.Get(OpcodeToken0);
    }

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    uint32_t ResourceReturnTypeToken = tokenStream[0];
    tokenStream++;

    retDecl.resource.resType[0] = Decl::ReturnTypeX.Get(ResourceReturnTypeToken);
    retDecl.resource.resType[1] = Decl::ReturnTypeY.Get(ResourceReturnTypeToken);
    retDecl.resource.resType[2] = Decl::ReturnTypeZ.Get(ResourceReturnTypeToken);
    retDecl.resource.resType[3] = Decl::ReturnTypeW.Get(ResourceReturnTypeToken);

    retDecl.str += "_";
    retDecl.str += ToStr(retDecl.resource.dim);
    if(retDecl.resource.sampleCount > 0)
    {
      retDecl.str += "(";
      retDecl.str += ToStr(retDecl.resource.sampleCount);
      retDecl.str += ")";
    }
    retDecl.str += " ";

    retDecl.str += "(";
    retDecl.str += ToStr(retDecl.resource.resType[0]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resource.resType[1]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resource.resType[2]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.resource.resType[3]);
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
      else if(retDecl.operand.indices[2].index == 0xffffffff)
        retDecl.str += StringFormat::Fmt(",regs=%u:unbounded", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
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

    retDecl.tgsmCount = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str += StringFormat::Fmt(", %u", retDecl.tgsmCount);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED)
  {
    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.tsgm_structured.stride = tokenStream[0];
    tokenStream++;

    retDecl.tsgm_structured.count = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str +=
        StringFormat::Fmt(", %u, %u", retDecl.tsgm_structured.stride, retDecl.tsgm_structured.count);
  }
  else if(op == OPCODE_DCL_INPUT_CONTROL_POINT_COUNT || op == OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT)
  {
    retDecl.controlPointCount = Decl::ControlPointCount.Get(OpcodeToken0);

    retDecl.str += StringFormat::Fmt(" %u", retDecl.controlPointCount);
  }
  else if(op == OPCODE_DCL_TESS_DOMAIN)
  {
    retDecl.tessDomain = Decl::TessDomain.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.tessDomain == DOMAIN_ISOLINE)
      retDecl.str += "domain_isoline";
    else if(retDecl.tessDomain == DOMAIN_TRI)
      retDecl.str += "domain_tri";
    else if(retDecl.tessDomain == DOMAIN_QUAD)
      retDecl.str += "domain_quad";
    else
      RDCERR("Unexpected Tessellation domain");
  }
  else if(op == OPCODE_DCL_TESS_PARTITIONING)
  {
    retDecl.tessPartition = Decl::TessPartitioning.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.tessPartition == PARTITIONING_INTEGER)
      retDecl.str += "partitioning_integer";
    else if(retDecl.tessPartition == PARTITIONING_POW2)
      retDecl.str += "partitioning_pow2";
    else if(retDecl.tessPartition == PARTITIONING_FRACTIONAL_ODD)
      retDecl.str += "partitioning_fractional_odd";
    else if(retDecl.tessPartition == PARTITIONING_FRACTIONAL_EVEN)
      retDecl.str += "partitioning_fractional_even";
    else
      RDCERR("Unexpected Partitioning");
  }
  else if(op == OPCODE_DCL_GS_INPUT_PRIMITIVE)
  {
    retDecl.geomInputPrimitive = Decl::InputPrimitive.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.geomInputPrimitive == PRIMITIVE_POINT)
      retDecl.str += "point";
    else if(retDecl.geomInputPrimitive == PRIMITIVE_LINE)
      retDecl.str += "line";
    else if(retDecl.geomInputPrimitive == PRIMITIVE_TRIANGLE)
      retDecl.str += "triangle";
    else if(retDecl.geomInputPrimitive == PRIMITIVE_LINE_ADJ)
      retDecl.str += "line_adj";
    else if(retDecl.geomInputPrimitive == PRIMITIVE_TRIANGLE_ADJ)
      retDecl.str += "triangle_adj";
    else if(retDecl.geomInputPrimitive >= PRIMITIVE_1_CONTROL_POINT_PATCH &&
            retDecl.geomInputPrimitive <= PRIMITIVE_32_CONTROL_POINT_PATCH)
    {
      retDecl.str +=
          StringFormat::Fmt("control_point_patch_%u",
                            1 + int(retDecl.geomInputPrimitive - PRIMITIVE_1_CONTROL_POINT_PATCH));
    }
    else
      RDCERR("Unexpected primitive type");
  }
  else if(op == OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
  {
    retDecl.geomOutputTopology = Decl::OutputPrimitiveTopology.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_POINTLIST)
      retDecl.str += "point";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_LINELIST)
      retDecl.str += "linelist";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP)
      retDecl.str += "linestrip";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
      retDecl.str += "trianglelist";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
      retDecl.str += "trianglestrip";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
      retDecl.str += "linelist_adj";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
      retDecl.str += "linestrip_adj";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ)
      retDecl.str += "trianglelist_adj";
    else if(retDecl.geomOutputTopology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
      retDecl.str += "trianglestrip_adj";
    else
      RDCERR("Unexpected primitive topology");
  }
  else if(op == OPCODE_DCL_TESS_OUTPUT_PRIMITIVE)
  {
    retDecl.tessOutputPrimitive = Decl::OutputPrimitive.Get(OpcodeToken0);

    retDecl.str += " ";
    if(retDecl.tessOutputPrimitive == OUTPUT_PRIMITIVE_POINT)
      retDecl.str += "output_point";
    else if(retDecl.tessOutputPrimitive == OUTPUT_PRIMITIVE_LINE)
      retDecl.str += "output_line";
    else if(retDecl.tessOutputPrimitive == OUTPUT_PRIMITIVE_TRIANGLE_CW)
      retDecl.str += "output_triangle_cw";
    else if(retDecl.tessOutputPrimitive == OUTPUT_PRIMITIVE_TRIANGLE_CCW)
      retDecl.str += "output_triangle_ccw";
    else
      RDCERR("Unexpected output primitive");
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW || op == OPCODE_DCL_RESOURCE_RAW)
  {
    retDecl.raw.rov = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) &&
                      Decl::RasterizerOrderedAccess.Get(OpcodeToken0);

    retDecl.raw.globallyCoherant =
        (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) & Decl::GloballyCoherent.Get(OpcodeToken0);

    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);

    if(retDecl.raw.globallyCoherant)
      retDecl.str += ", globallyCoherant";

    if(retDecl.raw.rov)
      retDecl.str += ", rasterizerOrderedAccess";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else if(retDecl.operand.indices[2].index == 0xffffffff)
        retDecl.str += StringFormat::Fmt(",regs=%u:unbounded", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED || op == OPCODE_DCL_RESOURCE_STRUCTURED)
  {
    retDecl.structured.hasCounter = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &&
                                    Decl::HasOrderPreservingCounter.Get(OpcodeToken0);

    retDecl.structured.rov = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &&
                             Decl::RasterizerOrderedAccess.Get(OpcodeToken0);

    retDecl.structured.globallyCoherant = (op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &
                                          Decl::GloballyCoherent.Get(OpcodeToken0);

    retDecl.str += " ";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.structured.stride = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str += StringFormat::Fmt(", %u", retDecl.structured.stride);

    if(retDecl.structured.hasCounter)
      retDecl.str += ", hasOrderPreservingCounter";

    if(retDecl.structured.globallyCoherant)
      retDecl.str += ", globallyCoherant";

    if(retDecl.structured.rov)
      retDecl.str += ", rasterizerOrderedAccess";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else if(retDecl.operand.indices[2].index == 0xffffffff)
        retDecl.str += StringFormat::Fmt(",regs=%u:unbounded", retDecl.operand.indices[1].index);
      else
        retDecl.str += StringFormat::Fmt(",regs=%u:%u", retDecl.operand.indices[1].index,
                                         retDecl.operand.indices[2].index);
    }
  }
  else if(op == OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED)
  {
    retDecl.uav_typed.dim = Decl::ResourceDim.Get(OpcodeToken0);

    retDecl.uav_typed.globallyCoherant = Decl::GloballyCoherent.Get(OpcodeToken0);

    retDecl.uav_typed.rov = Decl::RasterizerOrderedAccess.Get(OpcodeToken0);

    retDecl.str += "_";
    retDecl.str += ToStr(retDecl.uav_typed.dim);

    if(retDecl.uav_typed.globallyCoherant)
      retDecl.str += "_glc";

    bool ret = DecodeOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    uint32_t ResourceReturnTypeToken = tokenStream[0];
    tokenStream++;

    retDecl.uav_typed.resType[0] = Decl::ReturnTypeX.Get(ResourceReturnTypeToken);
    retDecl.uav_typed.resType[1] = Decl::ReturnTypeY.Get(ResourceReturnTypeToken);
    retDecl.uav_typed.resType[2] = Decl::ReturnTypeZ.Get(ResourceReturnTypeToken);
    retDecl.uav_typed.resType[3] = Decl::ReturnTypeW.Get(ResourceReturnTypeToken);

    retDecl.str += " ";

    retDecl.str += "(";
    retDecl.str += ToStr(retDecl.uav_typed.resType[0]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.uav_typed.resType[1]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.uav_typed.resType[2]);
    retDecl.str += ",";
    retDecl.str += ToStr(retDecl.uav_typed.resType[3]);
    retDecl.str += ")";

    retDecl.str += " ";

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);

    if(retDecl.uav_typed.rov)
      retDecl.str += ", rasterizerOrderedAccess";

    retDecl.space = 0;

    if(sm51)
    {
      retDecl.space = tokenStream[0];
      tokenStream++;
      retDecl.str += StringFormat::Fmt(" space=%u", retDecl.space);

      if(retDecl.operand.indices[1].index == retDecl.operand.indices[2].index)
        retDecl.str += StringFormat::Fmt(",reg=%u", retDecl.operand.indices[1].index);
      else if(retDecl.operand.indices[2].index == 0xffffffff)
        retDecl.str += StringFormat::Fmt(",regs=%u:unbounded", retDecl.operand.indices[1].index);
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

      retDecl.functionTableContents.push_back(tokenStream[0]);
      tokenStream++;
    }

    retDecl.str += "}";
  }
  else if(op == OPCODE_DCL_INTERFACE)
  {
    retDecl.iface.interfaceID = tokenStream[0];
    tokenStream++;

    retDecl.iface.numTypes = tokenStream[0];
    tokenStream++;

    uint32_t CountToken = tokenStream[0];
    tokenStream++;

    retDecl.iface.numInterfaces = Decl::NumInterfaces.Get(CountToken);
    uint32_t TableLength = Decl::TableLength.Get(CountToken);

    retDecl.str += StringFormat::Fmt(" fp%u[%u][%u]", retDecl.iface.interfaceID,
                                     retDecl.iface.numInterfaces, retDecl.iface.numTypes);

    retDecl.str += " = {";

    for(uint32_t i = 0; i < TableLength; i++)
    {
      retDecl.str += StringFormat::Fmt("ft%u", tokenStream[0]);

      if(i + 1 < TableLength)
        retDecl.str += ", ";

      retDecl.functionTableContents.push_back(tokenStream[0]);
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
  RDCASSERT((uint32_t)(tokenStream - begin) == declLength);

  return true;
}

void Program::EncodeOperation(rdcarray<uint32_t> &tokenStream, const Operation &op)
{
  rdcarray<uint32_t> opStream;

  opStream.reserve(16);

  uint32_t OpcodeToken0 = 0;

  Opcode::Type.Set(OpcodeToken0, op.operation);

  Opcode::TestNonZero.Set(OpcodeToken0, op.nonzero());
  Opcode::Saturate.Set(OpcodeToken0, op.saturate());
  Opcode::PreciseValues.Set(OpcodeToken0, op.preciseValues);

  if(op.operation == OPCODE_RESINFO)
    Opcode::ResinfoReturn.Set(OpcodeToken0, op.infoRetType);

  if(op.operation == OPCODE_SYNC)
    Opcode::SyncFlags.Set(OpcodeToken0, op.syncFlags);

  if(op.operation == OPCODE_SHADER_MESSAGE || op.operation == OPCODE_OPAQUE_CUSTOMDATA)
  {
    OpcodeToken0 = 0;
    Opcode::Type.Set(OpcodeToken0, OPCODE_CUSTOMDATA);
    Opcode::CustomClass.Set(OpcodeToken0, m_CustomDatas[op.customDataIndex].first);

    opStream.push_back(OpcodeToken0);

    const rdcarray<uint32_t> &customData = m_CustomDatas[op.customDataIndex].second;

    // push overall length, including OpcodeToken0 and this length
    opStream.push_back((uint32_t)customData.size() + 2);

    opStream.append(customData);

    // append immediately and return, so we don't do any length fixup
    tokenStream.append(opStream);

    return;
  }
  else if(op.operation == OPCODE_CUSTOMDATA)
  {
    RDCERR("Unexpected raw customdata operation");
    return;
  }

  opStream.push_back(OpcodeToken0);

  if(op.flags & Operation::FLAG_TEXEL_OFFSETS)
  {
    ExtendedOpcode::Extended.Set(opStream.back(), true);

    uint32_t OpcodeTokenN = 0;
    ExtendedOpcode::Type.Set(OpcodeTokenN, EXTENDED_OPCODE_SAMPLE_CONTROLS);

    ExtendedOpcode::TexelOffsetU.Set(OpcodeTokenN, op.texelOffset[0]);
    ExtendedOpcode::TexelOffsetV.Set(OpcodeTokenN, op.texelOffset[1]);
    ExtendedOpcode::TexelOffsetW.Set(OpcodeTokenN, op.texelOffset[2]);

    opStream.push_back(OpcodeTokenN);
  }

  if(op.flags & Operation::FLAG_RESOURCE_DIMS)
  {
    ExtendedOpcode::Extended.Set(opStream.back(), true);

    uint32_t OpcodeTokenN = 0;
    ExtendedOpcode::Type.Set(OpcodeTokenN, EXTENDED_OPCODE_RESOURCE_DIM);

    ExtendedOpcode::ResourceDim.Set(OpcodeTokenN, op.resDim);

    if(op.resDim == RESOURCE_DIMENSION_STRUCTURED_BUFFER)
      ExtendedOpcode::BufferStride.Set(OpcodeTokenN, op.stride);

    opStream.push_back(OpcodeTokenN);
  }

  if(op.flags & Operation::FLAG_RET_TYPE)
  {
    ExtendedOpcode::Extended.Set(opStream.back(), true);

    uint32_t OpcodeTokenN = 0;
    ExtendedOpcode::Type.Set(OpcodeTokenN, EXTENDED_OPCODE_RESOURCE_RETURN_TYPE);

    ExtendedOpcode::ReturnTypeX.Set(OpcodeTokenN, op.resType[0]);
    ExtendedOpcode::ReturnTypeY.Set(OpcodeTokenN, op.resType[1]);
    ExtendedOpcode::ReturnTypeZ.Set(OpcodeTokenN, op.resType[2]);
    ExtendedOpcode::ReturnTypeW.Set(OpcodeTokenN, op.resType[3]);

    opStream.push_back(OpcodeTokenN);
  }

  if(op.operation == OPCODE_INTERFACE_CALL)
    opStream.push_back(op.operands[0].values[0]);

  for(size_t i = 0; i < op.operands.size(); i++)
    EncodeOperand(opStream, op.operands[i]);

  if(op.flags & Operation::FLAG_TRAILING_ZERO_TOKEN)
    opStream.push_back(0x0);

  // fixup the declaration now and append
  Opcode::Length.Set(opStream[0], (uint32_t)opStream.size());
  tokenStream.append(opStream);
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

  uint32_t opLength = Opcode::Length.Get(OpcodeToken0);

  // possibly only set these when applicable
  retOp.operation = op;
  if(Opcode::TestNonZero.Get(OpcodeToken0))
    retOp.flags = Operation::Flags(retOp.flags | Operation::FLAG_NONZERO);
  if(Opcode::Saturate.Get(OpcodeToken0))
    retOp.flags = Operation::Flags(retOp.flags | Operation::FLAG_SATURATE);
  retOp.preciseValues = Opcode::PreciseValues.Get(OpcodeToken0);

  if(op == OPCODE_RESINFO || op == OPCODE_SAMPLE_INFO)
    retOp.infoRetType = Opcode::ResinfoReturn.Get(OpcodeToken0);

  if(op == OPCODE_SYNC)
    retOp.syncFlags = Opcode::SyncFlags.Get(OpcodeToken0);

  bool extended = Opcode::Extended.Get(OpcodeToken0) == 1;

  if(op == OPCODE_CUSTOMDATA)
  {
    CustomDataClass customClass = Opcode::CustomClass.Get(OpcodeToken0);

    tokenStream++;
    // DWORD length including OpcodeToken0 and this length token
    uint32_t customDataLength = tokenStream[0];
    tokenStream++;

    uint32_t dataLength = customDataLength - 2;

    RDCASSERT(customDataLength >= 2);

    switch(customClass)
    {
      case CUSTOMDATA_SHADER_MESSAGE:
      {
        uint32_t *end = tokenStream + dataLength;

        retOp.operation = OPCODE_SHADER_MESSAGE;
        retOp.customDataIndex = (uint32_t)m_CustomDatas.size();

        m_CustomDatas.push_back(make_rdcpair(customClass, rdcarray<uint32_t>()));
        m_CustomDatas.back().second.assign(tokenStream, dataLength);

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
      case CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER:
      {
        // handle as declaration
        tokenStream = begin;
        return false;
      }
      default:
      {
        // if we haven't seen any instructions yet, handle as declaration
        if(m_Instructions.empty())
        {
          tokenStream = begin;
          return false;
        }

        RDCWARN("Unsupported custom data class %d!", customClass);

        RDCLOG("Data length seems to be %d uint32s", dataLength);

        retOp.operation = OPCODE_OPAQUE_CUSTOMDATA;

        retOp.customDataIndex = (uint32_t)m_CustomDatas.size();

        m_CustomDatas.push_back(make_rdcpair(customClass, rdcarray<uint32_t>()));
        m_CustomDatas.back().second.assign(tokenStream, dataLength);

        tokenStream += dataLength;

        return true;
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
      retOp.flags = Operation::Flags(retOp.flags | Operation::FLAG_TEXEL_OFFSETS);

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
      retOp.flags = Operation::Flags(retOp.flags | Operation::FLAG_RESOURCE_DIMS);

      retOp.resDim = ExtendedOpcode::ResourceDim.Get(OpcodeTokenN);

      if(retOp.resDim == RESOURCE_DIMENSION_STRUCTURED_BUFFER)
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
      retOp.flags = Operation::Flags(retOp.flags | Operation::FLAG_RET_TYPE);

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
    retOp.str += ToStr(retOp.infoRetType);
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
    retOp.operands[0].values[0] = func;
  }

  if(op == OPCODE_IF || op == OPCODE_BREAKC || op == OPCODE_CALLC || op == OPCODE_CONTINUEC ||
     op == OPCODE_RETC || op == OPCODE_DISCARD)
    retOp.str += retOp.nonzero() ? "_nz" : "_z";

  if(op != OPCODE_SYNC)
  {
    retOp.str += retOp.saturate() ? "_sat" : "";
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

  uint32_t consumedTokens = (uint32_t)(tokenStream - begin);

  if(consumedTokens + 1 == opLength && begin[consumedTokens] == 0x0)
  {
    retOp.flags = Operation::Flags(retOp.flags | Operation::FLAG_TRAILING_ZERO_TOKEN);
    RDCDEBUG("Consuming extra unused 0 dword");

    tokenStream++;
    consumedTokens++;
  }

#if ENABLED(RDOC_DEVEL)
  if(consumedTokens > opLength)
  {
    RDCERR("Consumed too many tokens for %d!", retOp.operation);

    // try to recover by rewinding the stream, this instruction will be garbage but at least the
    // next ones will be correct
    uint32_t overread = consumedTokens - opLength;
    tokenStream -= overread;
  }
  else if(consumedTokens < opLength)
  {
    // sometimes this just happens, which is why we only print this in non-release so we can
    // inspect it. There's probably not much we can do though, it's just magic.
    RDCWARN("Consumed too few tokens for %d!", retOp.operation);
    uint32_t missing = opLength - consumedTokens;
    for(uint32_t i = 0; i < missing; i++)
    {
      RDCLOG("missing token %d: 0x%08x", i, tokenStream[0]);
      tokenStream++;
    }
  }

  // make sure we consumed all uint32s
  RDCASSERT((uint32_t)(tokenStream - begin) == opLength);
#else
  // there's no good documentation for this, we're freewheeling blind in a nightmarish hellscape.
  // Instead of assuming we can predictably decode the whole of every opcode, just advance by the
  // defined length.
  tokenStream = begin + opLength;
#endif

  return true;
}

};    // namespace DXBCBytecode
