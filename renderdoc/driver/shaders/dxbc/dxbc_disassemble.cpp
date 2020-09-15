/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <math.h>
#include "common/common.h"
#include "core/core.h"
#include "core/settings.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "dxbc_bytecode.h"

#include "dxbc_container.h"

RDOC_CONFIG(bool, DXBC_Disassembly_FriendlyNaming, true,
            "Where possible (i.e. it is completely unambiguous) replace register names with "
            "high-level variable names.");

namespace DXBCBytecode
{
// little utility function to both document and easily extract an arbitrary mask
// out of the tokens. Makes the assumption that we always take some masked off
// bits and shift them all the way to the LSB. Then casts it to whatever type
template <typename T, uint32_t M>
class MaskedElement
{
public:
  static T Get(uint32_t token)
  {
    unsigned long shift = 0;
    unsigned long mask = M;
    byte hasBit = _BitScanForward(&shift, mask);
    RDCASSERT(hasBit != 0);

    T ret = (T)((token & mask) >> shift);

    return ret;
  }
};

// bools need a comparison to be safe, rather than casting.
template <uint32_t M>
class MaskedElement<bool, M>
{
public:
  static bool Get(uint32_t token)
  {
    unsigned long shift = 0;
    unsigned long mask = M;
    byte hasBit = _BitScanForward(&shift, mask);
    RDCASSERT(hasBit != 0);

    bool ret = ((token & mask) >> shift) != 0;

    return ret;
  }
};

////////////////////////////////////////////////////////////////////////////
// The token stream appears as a series of uint32 tokens.
// First is a version token, then a length token, then a series of Opcodes
// (which are N tokens).
// An Opcode consists of an Opcode token, then optionally some ExtendedOpcode
// tokens. Then depending on the type of Opcode some number of further tokens -
// typically Operands, although occasionally other DWORDS.
// An Operand is a single Operand token then possibly some more DWORDS again,
// indices and such like.

namespace VersionToken
{
static MaskedElement<uint32_t, 0x000000f0> MajorVersion;
static MaskedElement<uint32_t, 0x0000000f> MinorVersion;

static MaskedElement<DXBC::ShaderType, 0xffff0000> ProgramType;
};

namespace LengthToken
{
static MaskedElement<uint32_t, 0xffffffff> Length;
};

namespace Opcode
{
// generic
static MaskedElement<OpcodeType, 0x000007FF> Type;
static MaskedElement<uint32_t, 0x7F000000> Length;
static MaskedElement<bool, 0x80000000> Extended;
static MaskedElement<CustomDataClass, 0xFFFFF800> CustomClass;

// opcode specific
static MaskedElement<uint32_t, 0x00780000> PreciseValues;

// several
static MaskedElement<bool, 0x00002000> Saturate;
static MaskedElement<bool, 0x00040000> TestNonZero;

// OPCODE_RESINFO
static MaskedElement<ResinfoRetType, 0x00001800> ResinfoReturn;

// OPCODE_SYNC
static MaskedElement<uint32_t, 0x00007800> SyncFlags;
// relative to above uint32! ie. post shift.
static MaskedElement<bool, 0x00000001> Sync_Threads;
static MaskedElement<bool, 0x00000002> Sync_TGSM;
static MaskedElement<bool, 0x00000004> Sync_UAV_Group;
static MaskedElement<bool, 0x00000008> Sync_UAV_Global;

// OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED
// OPCODE_DCL_RESOURCE_STRUCTURED
static MaskedElement<bool, 0x00800000> HasOrderPreservingCounter;
};    // Opcode

// Declarations are Opcode tokens, but with their own particular definitions
// of most of the bits (aside from the generice type/length/extended bits above)
namespace Decl
{
// OPCODE_DCL_GLOBAL_FLAGS
static MaskedElement<bool, 0x00000800> RefactoringAllowed;
static MaskedElement<bool, 0x00001000> DoubleFloatOps;
static MaskedElement<bool, 0x00002000> ForceEarlyDepthStencil;
static MaskedElement<bool, 0x00004000> EnableRawStructuredBufs;
static MaskedElement<bool, 0x00008000> SkipOptimisation;
static MaskedElement<bool, 0x00010000> EnableMinPrecision;
static MaskedElement<bool, 0x00020000> EnableD3D11_1DoubleExtensions;
static MaskedElement<bool, 0x00040000> EnableD3D11_1ShaderExtensions;
static MaskedElement<bool, 0x00080000> EnableD3D12AllResourcesBound;

// OPCODE_DCL_CONSTANT_BUFFER
static MaskedElement<CBufferAccessPattern, 0x00000800> AccessPattern;

// OPCODE_DCL_SAMPLER
static MaskedElement<SamplerMode, 0x00007800> SamplerMode;

// OPCODE_DCL_RESOURCE
static MaskedElement<ResourceDimension, 0x0000F800> ResourceDim;
static MaskedElement<uint32_t, 0x007F0000> SampleCount;
// below come in a second token (ResourceReturnTypeToken). See extract functions below
static MaskedElement<DXBC::ResourceRetType, 0x0000000F> ReturnTypeX;
static MaskedElement<DXBC::ResourceRetType, 0x000000F0> ReturnTypeY;
static MaskedElement<DXBC::ResourceRetType, 0x00000F00> ReturnTypeZ;
static MaskedElement<DXBC::ResourceRetType, 0x0000F000> ReturnTypeW;

// OPCODE_DCL_INPUT_PS
static MaskedElement<InterpolationMode, 0x00007800> InterpolationMode;

// OPCODE_DCL_INPUT_CONTROL_POINT_COUNT
// OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT
static MaskedElement<uint32_t, 0x0001F800> ControlPointCount;

// OPCODE_DCL_TESS_DOMAIN
static MaskedElement<TessellatorDomain, 0x00001800> TessDomain;

// OPCODE_DCL_TESS_PARTITIONING
static MaskedElement<TessellatorPartitioning, 0x00003800> TessPartitioning;

// OPCODE_DCL_GS_INPUT_PRIMITIVE
static MaskedElement<PrimitiveType, 0x0001F800> InputPrimitive;

// OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY
static MaskedElement<D3D_PRIMITIVE_TOPOLOGY, 0x0001F800> OutputPrimitiveTopology;

// OPCODE_DCL_TESS_OUTPUT_PRIMITIVE
static MaskedElement<TessellatorOutputPrimitive, 0x00003800> OutputPrimitive;

// OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED
static MaskedElement<bool, 0x00010000> GloballyCoherent;
static MaskedElement<bool, 0x00020000> RasterizerOrderedAccess;

// OPCODE_DCL_INTERFACE
static MaskedElement<uint32_t, 0x0000FFFF> TableLength;
static MaskedElement<uint32_t, 0xFFFF0000> NumInterfaces;
};    // Declaration

namespace ExtendedOpcode
{
static MaskedElement<bool, 0x80000000> Extended;
static MaskedElement<ExtendedOpcodeType, 0x0000003F> Type;

// OPCODE_EX_SAMPLE_CONTROLS
static MaskedElement<int, 0x00001E00> TexelOffsetU;
static MaskedElement<int, 0x0001E000> TexelOffsetV;
static MaskedElement<int, 0x001E0000> TexelOffsetW;

// OPCODE_EX_RESOURCE_DIM
static MaskedElement<ResourceDimension, 0x000007C0> ResourceDim;
static MaskedElement<uint32_t, 0x007FF800> BufferStride;

// OPCODE_EX_RESOURCE_RETURN_TYPE
static MaskedElement<DXBC::ResourceRetType, 0x000003C0> ReturnTypeX;
static MaskedElement<DXBC::ResourceRetType, 0x00003C00> ReturnTypeY;
static MaskedElement<DXBC::ResourceRetType, 0x0003C000> ReturnTypeZ;
static MaskedElement<DXBC::ResourceRetType, 0x003C0000> ReturnTypeW;
};    // ExtendedOpcode

namespace Oper
{
static MaskedElement<NumOperandComponents, 0x00000003> NumComponents;
static MaskedElement<SelectionMode, 0x0000000C> SelectionMode;

// SELECTION_MASK
static MaskedElement<bool, 0x00000010> ComponentMaskX;
static MaskedElement<bool, 0x00000020> ComponentMaskY;
static MaskedElement<bool, 0x00000040> ComponentMaskZ;
static MaskedElement<bool, 0x00000080> ComponentMaskW;

// SELECTION_SWIZZLE
static MaskedElement<uint8_t, 0x00000030> ComponentSwizzleX;
static MaskedElement<uint8_t, 0x000000C0> ComponentSwizzleY;
static MaskedElement<uint8_t, 0x00000300> ComponentSwizzleZ;
static MaskedElement<uint8_t, 0x00000C00> ComponentSwizzleW;

// SELECTION_SELECT_1
static MaskedElement<uint8_t, 0x00000030> ComponentSel1;

static MaskedElement<OperandType, 0x000FF000> Type;
static MaskedElement<uint32_t, 0x00300000> IndexDimension;

static MaskedElement<OperandIndexType, 0x01C00000> Index0;
static MaskedElement<OperandIndexType, 0x0E000000> Index1;
static MaskedElement<OperandIndexType, 0x70000000> Index2;

static MaskedElement<bool, 0x80000000> Extended;
};    // Operand

namespace ExtendedOperand
{
static MaskedElement<ExtendedOperandType, 0x0000003F> Type;
static MaskedElement<bool, 0x80000000> Extended;

// EXTENDED_OPERAND_MODIFIER
static MaskedElement<OperandModifier, 0x00003FC0> Modifier;
static MaskedElement<MinimumPrecision, 0x0001C000> MinPrecision;
static MaskedElement<bool, 0x00020000> NonUniform;
};

rdcstr toString(const uint32_t values[], uint32_t numComps);

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

void Program::FetchTypeVersion()
{
  if(m_HexDump.empty())
    return;

  uint32_t *begin = &m_HexDump.front();
  uint32_t *cur = begin;

  m_Type = VersionToken::ProgramType.Get(cur[0]);
  m_Major = VersionToken::MajorVersion.Get(cur[0]);
  m_Minor = VersionToken::MinorVersion.Get(cur[0]);
}

void Program::FetchComputeProperties(DXBC::Reflection *reflection)
{
  if(m_HexDump.empty())
    return;

  uint32_t *begin = &m_HexDump.front();
  uint32_t *cur = begin;
  uint32_t *end = &m_HexDump.back();

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

void Program::DisassembleHexDump()
{
  if(m_Disassembled)
    return;

  if(m_HexDump.empty())
    return;

  m_Disassembled = true;

  uint32_t *begin = &m_HexDump.front();
  uint32_t *cur = begin;
  uint32_t *end = &m_HexDump.back();

  // check supported types
  if(!(m_Major == 0x5 && m_Minor == 0x1) && !(m_Major == 0x5 && m_Minor == 0x0) &&
     !(m_Major == 0x4 && m_Minor == 0x1) && !(m_Major == 0x4 && m_Minor == 0x0))
  {
    RDCERR("Unsupported shader bytecode version: %u.%u", m_Major, m_Minor);
    return;
  }

  RDCASSERT(LengthToken::Length.Get(cur[1]) == m_HexDump.size());    // length token

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

    if(!ExtractOperation(cur, op, friendly))
    {
      if(!ExtractDecl(cur, decl, friendly))
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

  Operation implicitRet;
  implicitRet.length = 1;
  implicitRet.offset = (end - begin) * sizeof(uint32_t);
  implicitRet.operation = OPCODE_RET;
  implicitRet.str = "ret";

  m_Instructions.push_back(implicitRet);
}

void Program::MakeDisassemblyString()
{
  DisassembleHexDump();

  if(m_HexDump.empty())
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

bool Program::ExtractOperand(uint32_t *&tokenStream, ToString flags, Operand &retOper)
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

    uint32_t numRead = 1;

    if(retOper.numComponents == NUMCOMPS_1)
      numRead = 1;
    else if(retOper.numComponents == NUMCOMPS_4)
      numRead = 4;
    else
      RDCERR("N-wide vectors not supported.");

    for(uint32_t i = 0; i < numRead; i++)
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

      bool ret = ExtractOperand(tokenStream, flags, retOper.indices[idx].operand);
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

  return str;
}

bool Program::ExtractDecl(uint32_t *&tokenStream, Declaration &retDecl, bool friendlyName)
{
  uint32_t *begin = tokenStream;
  uint32_t OpcodeToken0 = tokenStream[0];

  ToString flags = friendlyName ? ToString::FriendlyNameRegisters : ToString::None;
  flags = flags | ToString::IsDecl;

  const bool sm51 = (m_Major == 0x5 && m_Minor == 0x1);

  OpcodeType op = Opcode::Type.Get(OpcodeToken0);

  RDCASSERT(op < NUM_OPCODES);

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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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
    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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
    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
  }
  else if(op == OPCODE_DCL_SAMPLER)
  {
    retDecl.samplerMode = Decl::SamplerMode.Get(OpcodeToken0);

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.str += " ";
    retDecl.str += ToStr(retDecl.interpolation);

    retDecl.str += " ";
    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);
  }
  else if(op == OPCODE_DCL_INDEX_RANGE)
  {
    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
    RDCASSERT(ret);

    retDecl.count = tokenStream[0];
    tokenStream++;

    retDecl.str += retDecl.operand.toString(m_Reflection, flags);
    retDecl.str += StringFormat::Fmt(", %u", retDecl.count);
  }
  else if(op == OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED)
  {
    retDecl.str += " ";

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

    bool ret = ExtractOperand(tokenStream, flags, retDecl.operand);
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

bool Program::ExtractOperation(uint32_t *&tokenStream, Operation &retOp, bool friendlyName)
{
  uint32_t *begin = tokenStream;
  uint32_t OpcodeToken0 = tokenStream[0];

  ToString flags = friendlyName ? ToString::FriendlyNameRegisters : ToString::None;

  OpcodeType op = Opcode::Type.Get(OpcodeToken0);

  RDCASSERT(op < NUM_OPCODES);

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
          bool ret = ExtractOperand(tokenStream, flags, retOp.operands[i]);
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
    bool ret = ExtractOperand(tokenStream, flags, retOp.operands[i]);
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

rdcstr toString(const uint32_t values[], uint32_t numComps)
{
  rdcstr str = "";

  // fxc actually guesses these types it seems.
  // try setting an int value to 1085276160, it will be displayed in disasm as 5.500000.
  // I don't know the exact heuristic but I'm guessing something along the lines of
  // checking if it's float-looking.
  // My heuristic is:
  // * is exponent 0 or 0x7f8? It's either inf, NaN, other special value. OR it's 0, which is
  //   identical in int or float anyway - so interpret it as an int. Small ints display as numbers,
  //   larger ints in raw hex
  // * otherwise, assume it's a float.
  // * If any component is a float, they are all floats.
  //
  // this means this will break if an inf/nan is set as a param, and is kind of a kludge, but the
  // behaviour seems to match d3dcompiler.dll's behaviour in most cases. There are a couple of
  // exceptions that I don't follow: 0 is always displayed as a float in vectors, however
  // sometimes it can be an int.

  bool floatOutput = false;

  for(uint32_t i = 0; i < numComps; i++)
  {
    int32_t *vi = (int32_t *)&values[i];

    uint32_t exponent = vi[0] & 0x7f800000;

    if(exponent != 0 && exponent != 0x7f800000)
      floatOutput = true;
  }

  for(uint32_t i = 0; i < numComps; i++)
  {
    float *vf = (float *)&values[i];
    int32_t *vi = (int32_t *)&values[i];

    if(floatOutput)
    {
      str += ToStr(vf[0]);
    }
    else
    {
      // print small ints straight up, otherwise as hex
      if(vi[0] <= 10000 && vi[0] >= -10000)
        str += ToStr(vi[0]);
      else
        str += StringFormat::Fmt("0x%08x", vi[0]);
    }

    if(i + 1 < numComps)
      str += ", ";
  }

  return str;
}

};    // namespace DXBCBytecode

template <>
rdcstr DoStringise(const DXBCBytecode::OpcodeType &el)
{
  BEGIN_ENUM_STRINGISE(DXBCBytecode::OpcodeType)
  {
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ADD, "add")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AND, "and")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_BREAK, "break")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_BREAKC, "breakc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CALL, "call")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CALLC, "callc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CASE, "case")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CONTINUE, "continue")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CONTINUEC, "continuec")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CUT, "cut")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DEFAULT, "default")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DERIV_RTX, "deriv_rtx")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DERIV_RTY, "deriv_rty")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DISCARD, "discard")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DIV, "div")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DP2, "dp2")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DP3, "dp3")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DP4, "dp4")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ELSE, "else")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EMIT, "emit")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EMITTHENCUT, "emitthencut")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ENDIF, "endif")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ENDLOOP, "endloop")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ENDSWITCH, "endswitch")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EQ, "eq")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EXP, "exp")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FRC, "frc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FTOI, "ftoi")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FTOU, "ftou")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GE, "ge")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IADD, "iadd")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IF, "if")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IEQ, "ieq")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IGE, "ige")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ILT, "ilt")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMAD, "imad")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMAX, "imax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMIN, "imin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMUL, "imul")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_INE, "ine")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_INEG, "ineg")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ISHL, "ishl")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ISHR, "ishr")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ITOF, "itof")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LABEL, "label")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD, "ld_indexable")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_MS, "ld_ms")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LOG, "log")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LOOP, "loop")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LT, "lt")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MAD, "mad")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MIN, "min")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MAX, "max")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CUSTOMDATA, "customdata")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MOV, "mov")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MOVC, "movc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MUL, "mul")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NE, "ne")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NOP, "nop")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NOT, "not")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_OR, "or")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_RESINFO, "resinfo_indexable")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_RET, "ret")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_RETC, "retc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ROUND_NE, "round_ne")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ROUND_NI, "round_ni")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ROUND_PI, "round_pi")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ROUND_Z, "round_z")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_RSQ, "rsq")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE, "sample_indexable")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_C, "sample_c")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_C_LZ, "sample_c_lz")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_L, "sample_l")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_D, "sample_d")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_B, "sample_b")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SQRT, "sqrt")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SWITCH, "switch")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SINCOS, "sincos")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UDIV, "udiv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ULT, "ult")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UGE, "uge")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UMUL, "umul")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UMAD, "umad")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UMAX, "umax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UMIN, "umin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_USHR, "ushr")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UTOF, "utof")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_XOR, "xor")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_RESOURCE, "dcl_resource")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_CONSTANT_BUFFER, "dcl_constantbuffer")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_SAMPLER, "dcl_sampler")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INDEX_RANGE, "dcl_indexRange")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY, "dcl_outputtopology")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_GS_INPUT_PRIMITIVE, "dcl_inputprimitive")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT, "dcl_maxout")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT, "dcl_input")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT_SGV, "dcl_input_sgv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT_SIV, "dcl_input_siv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT_PS, "dcl_input_ps")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT_PS_SGV, "dcl_input_ps_sgv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT_PS_SIV, "dcl_input_ps_siv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_OUTPUT, "dcl_output")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_OUTPUT_SGV, "dcl_output_sgv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_OUTPUT_SIV, "dcl_output_siv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_TEMPS, "dcl_temps")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INDEXABLE_TEMP, "dcl_indexableTemp")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_GLOBAL_FLAGS, "dcl_globalFlags")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LOD, "lod")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4, "gather4")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_POS, "samplepos")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_INFO, "sample_info")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_HS_DECLS, "hs_decls")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_HS_CONTROL_POINT_PHASE, "hs_control_point_phase")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_HS_FORK_PHASE, "hs_fork_phase")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_HS_JOIN_PHASE, "hs_join_phase")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EMIT_STREAM, "emit_stream")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CUT_STREAM, "cut_stream")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EMITTHENCUT_STREAM, "emitThenCut_stream")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_INTERFACE_CALL, "fcall")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_BUFINFO, "bufinfo")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DERIV_RTX_COARSE, "deriv_rtx_coarse")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DERIV_RTX_FINE, "deriv_rtx_fine")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DERIV_RTY_COARSE, "deriv_rty_coarse")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DERIV_RTY_FINE, "deriv_rty_fine")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_C, "gather4_c")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_PO, "gather4_po")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_PO_C, "gather4_po_c")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_RCP, "rcp")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_F32TOF16, "f32tof16")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_F16TOF32, "f16tof32")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UADDC, "uaddc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_USUBB, "usubb")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_COUNTBITS, "countbits")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FIRSTBIT_HI, "firstbit_hi")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FIRSTBIT_LO, "firstbit_lo")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FIRSTBIT_SHI, "firstbit_shi")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UBFE, "ubfe")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IBFE, "ibfe")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_BFI, "bfi")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_BFREV, "bfrev")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SWAPC, "swapc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_STREAM, "dcl_stream")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_FUNCTION_BODY, "dcl_function_body")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_FUNCTION_TABLE, "dcl_function_table")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INTERFACE, "dcl_interface")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_INPUT_CONTROL_POINT_COUNT,
                               "dcl_input_control_point_count")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT,
                               "dcl_output_control_point_count")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_TESS_DOMAIN, "dcl_tessellator_domain")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_TESS_PARTITIONING, "dcl_tessellator_partitioning")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_TESS_OUTPUT_PRIMITIVE, "dcl_tessellator_output_primitive")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_HS_MAX_TESSFACTOR, "dcl_hs_max_tessfactor")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT,
                               "dcl_hs_fork_phase_instance_count")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT,
                               "dcl_hs_join_phase_instance_count")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_THREAD_GROUP, "dcl_thread_group")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED, "dcl_uav_typed")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW, "dcl_uav_raw")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED, "dcl_uav_structured")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW, "dcl_tgsm_raw")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED,
                               "dcl_tgsm_structured")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_RESOURCE_RAW, "dcl_resource_raw")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_RESOURCE_STRUCTURED, "dcl_resource_structured")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_UAV_TYPED, "ld_uav_typed")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_STORE_UAV_TYPED, "store_uav_typed")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_RAW, "ld_raw")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_STORE_RAW, "store_raw")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_STRUCTURED, "ld_structured")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_STORE_STRUCTURED, "store_structured")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_AND, "atomic_and")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_OR, "atomic_or")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_XOR, "atomic_xor")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_CMP_STORE, "atomic_cmp_store")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_IADD, "atomic_iadd")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_IMAX, "atomic_imax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_IMIN, "atomic_imin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_UMAX, "atomic_umax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ATOMIC_UMIN, "atomic_umin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_ALLOC, "imm_atomic_alloc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_CONSUME, "imm_atomic_consume")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_IADD, "imm_atomic_iadd")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_AND, "imm_atomic_and")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_OR, "imm_atomic_or")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_XOR, "imm_atomic_xor")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_EXCH, "imm_atomic_exch")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_CMP_EXCH, "imm_atomic_cmp_exch")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_IMAX, "imm_atomic_imax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_IMIN, "imm_atomic_imin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_UMAX, "imm_atomic_umax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_IMM_ATOMIC_UMIN, "imm_atomic_umin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SYNC, "sync")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DADD, "dadd")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DMAX, "dmax")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DMIN, "dmin")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DMUL, "dmul")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DEQ, "deq")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DGE, "dge")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DLT, "dlt")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DNE, "dne")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DMOV, "dmov")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DMOVC, "dmovc")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DTOF, "dtof")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_FTOD, "ftod")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EVAL_SNAPPED, "eval_snapped")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EVAL_SAMPLE_INDEX, "eval_sample_index")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_EVAL_CENTROID, "eval_centroid")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DCL_GS_INSTANCE_COUNT, "dcl_gs_instance_count")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ABORT, "abort")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DEBUGBREAK, "debugbreak")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DDIV, "ddiv")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DFMA, "dfma")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DRCP, "drcp")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_MSAD, "msad")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DTOI, "dtoi")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_DTOU, "dtou")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_ITOD, "itod")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_UTOD, "utod")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_FEEDBACK, "gather4_statusk")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_C_FEEDBACK, "gather4_c_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_PO_FEEDBACK, "gather4_po_statusk")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_GATHER4_PO_C_FEEDBACK, "gather4_po_c_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_FEEDBACK, "ld")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_MS_FEEDBACK, "ld_ms_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_UAV_TYPED_FEEDBACK, "ld_uav_typed_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_RAW_FEEDBACK, "ld_raw_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_LD_STRUCTURED_FEEDBACK, "ld_structured_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_L_FEEDBACK, "sample_l_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_C_LZ_FEEDBACK, "sample_c_lz_status")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_CLAMP_FEEDBACK, "sample_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_B_CLAMP_FEEDBACK, "sample_b_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_D_CLAMP_FEEDBACK, "sample_d_status")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_SAMPLE_C_CLAMP_FEEDBACK, "sample_c_status")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_CHECK_ACCESS_FULLY_MAPPED, "check_access_fully_mapped")
  }
  END_ENUM_STRINGISE();
}
template <>
rdcstr DoStringise(const DXBCBytecode::OperandType &el)
{
  BEGIN_ENUM_STRINGISE(DXBCBytecode::OperandType)
  {
    STRINGISE_ENUM_CLASS_NAMED(TYPE_TEMP, "TEMP");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT, "INPUT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT, "OUTPUT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INDEXABLE_TEMP, "INDEXABLE_TEMP");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_IMMEDIATE32, "IMMEDIATE32");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_IMMEDIATE64, "IMMEDIATE64");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_SAMPLER, "SAMPLER");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_RESOURCE, "RESOURCE");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_CONSTANT_BUFFER, "CONSTANT_BUFFER");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_IMMEDIATE_CONSTANT_BUFFER, "IMMEDIATE_CONSTANT_BUFFER");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_LABEL, "LABEL");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_PRIMITIVEID, "INPUT_PRIMITIVEID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_DEPTH, "OUTPUT_DEPTH");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_NULL, "NULL");

    STRINGISE_ENUM_CLASS_NAMED(TYPE_RASTERIZER, "RASTERIZER");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_COVERAGE_MASK, "OUTPUT_COVERAGE_MASK");

    STRINGISE_ENUM_CLASS_NAMED(TYPE_STREAM, "STREAM");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_FUNCTION_BODY, "FUNCTION_BODY");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_FUNCTION_TABLE, "FUNCTION_TABLE");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INTERFACE, "INTERFACE");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_FUNCTION_INPUT, "FUNCTION_INPUT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_FUNCTION_OUTPUT, "FUNCTION_OUTPUT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_CONTROL_POINT_ID, "OUTPUT_CONTROL_POINT_ID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_FORK_INSTANCE_ID, "INPUT_FORK_INSTANCE_ID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_JOIN_INSTANCE_ID, "INPUT_JOIN_INSTANCE_ID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_CONTROL_POINT, "INPUT_CONTROL_POINT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_CONTROL_POINT, "OUTPUT_CONTROL_POINT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_PATCH_CONSTANT, "INPUT_PATCH_CONSTANT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_DOMAIN_POINT, "INPUT_DOMAIN_POINT");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_THIS_POINTER, "THIS_POINTER");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_UNORDERED_ACCESS_VIEW, "UNORDERED_ACCESS_VIEW");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_THREAD_GROUP_SHARED_MEMORY, "THREAD_GROUP_SHARED_MEMORY");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_THREAD_ID, "INPUT_THREAD_ID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_THREAD_GROUP_ID, "INPUT_THREAD_GROUP_ID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_THREAD_ID_IN_GROUP, "INPUT_THREAD_ID_IN_GROUP");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_COVERAGE_MASK, "INPUT_COVERAGE_MASK");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED,
                               "INPUT_THREAD_ID_IN_GROUP_FLATTENED");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INPUT_GS_INSTANCE_ID, "INPUT_GS_INSTANCE_ID");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_DEPTH_GREATER_EQUAL, "OUTPUT_DEPTH_GREATER_EQUAL");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_DEPTH_LESS_EQUAL, "OUTPUT_DEPTH_LESS_EQUAL");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_CYCLE_COUNTER, "CYCLE_COUNTER");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_OUTPUT_STENCIL_REF, "OUTPUT_STENCIL_REF");
    STRINGISE_ENUM_CLASS_NAMED(TYPE_INNER_COVERAGE, "INNER_COVERAGE");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXBCBytecode::ResourceDimension &el)
{
  BEGIN_ENUM_STRINGISE(DXBCBytecode::ResourceDimension)
  {
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_UNKNOWN, "unknown");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_BUFFER, "buffer");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE1D, "texture1d");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE2D, "texture2d");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE2DMS, "texture2dms");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE3D, "texture3d");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURECUBE, "texturecube");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE1DARRAY, "texture1darray");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE2DARRAY, "texture2darray");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURE2DMSARRAY, "texture2dmsarray");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_TEXTURECUBEARRAY, "texturecubearray");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_RAW_BUFFER, "rawbuffer");
    STRINGISE_ENUM_CLASS_NAMED(RESOURCE_DIMENSION_STRUCTURED_BUFFER, "structured_buffer");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXBC::ResourceRetType &el)
{
  BEGIN_ENUM_STRINGISE(DXBC::ResourceRetType)
  {
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_UNORM, "unorm");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_SNORM, "snorm");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_SINT, "sint");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_UINT, "uint");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_FLOAT, "float");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_MIXED, "mixed");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_DOUBLE, "double");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_CONTINUED, "continued");
    STRINGISE_ENUM_CLASS_NAMED(RETURN_TYPE_UNUSED, "unused");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXBCBytecode::ResinfoRetType &el)
{
  BEGIN_ENUM_STRINGISE(DXBCBytecode::ResinfoRetType)
  {
    STRINGISE_ENUM_CLASS_NAMED(RETTYPE_FLOAT, "float");
    STRINGISE_ENUM_CLASS_NAMED(RETTYPE_RCPFLOAT, "rcpfloat");
    STRINGISE_ENUM_CLASS_NAMED(RETTYPE_UINT, "uint");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXBCBytecode::InterpolationMode &el)
{
  BEGIN_ENUM_STRINGISE(DXBCBytecode::InterpolationMode)
  {
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_UNDEFINED, "undefined");
    // differs slightly from fxc but it's very convenient to use the hlsl terms, which are used in
    // all other cases
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_CONSTANT, "nointerpolation");
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_LINEAR, "linear");
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_LINEAR_CENTROID, "linear centroid");
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_LINEAR_NOPERSPECTIVE, "linear noperspective");
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID,
                               "linear noperspective centroid");
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_LINEAR_SAMPLE, "linear sample");
    STRINGISE_ENUM_CLASS_NAMED(INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,
                               "linear noperspective sample");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXBC::SVSemantic &el)
{
  BEGIN_ENUM_STRINGISE(DXBC::SVSemantic)
  {
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_POSITION, "position");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_CLIP_DISTANCE, "clipdistance");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_CULL_DISTANCE, "culldistance");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_RENDER_TARGET_ARRAY_INDEX, "rendertarget_array_index");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_VIEWPORT_ARRAY_INDEX, "viewport_array_index");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_VERTEX_ID, "vertexid");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_PRIMITIVE_ID, "primitiveid");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_INSTANCE_ID, "instanceid");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_IS_FRONT_FACE, "isfrontface");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_SAMPLE_INDEX, "sampleidx");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_QUAD_EDGE_TESSFACTOR0, "finalQuadUeq0EdgeTessFactor");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_QUAD_EDGE_TESSFACTOR1, "finalQuadVeq0EdgeTessFactor");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_QUAD_EDGE_TESSFACTOR2, "finalQuadUeq1EdgeTessFactor");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_QUAD_EDGE_TESSFACTOR3, "finalQuadVeq1EdgeTessFactor");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR0, "finalQuadUInsideTessFactor");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR1, "finalQuadVInsideTessFactor");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_TRI_EDGE_TESSFACTOR0, "finalTriUeq0EdgeTessFactor");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_TRI_EDGE_TESSFACTOR1, "finalTriVeq0EdgeTessFactor");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_TRI_EDGE_TESSFACTOR2, "finalTriWeq0EdgeTessFactor");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_TRI_INSIDE_TESSFACTOR, "finalTriInsideTessFactor");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_LINE_DETAIL_TESSFACTOR, "finalLineEdgeTessFactor");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_FINAL_LINE_DENSITY_TESSFACTOR, "finalLineInsideTessFactor");

    STRINGISE_ENUM_CLASS_NAMED(SVNAME_TARGET, "target");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_DEPTH, "depth");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_COVERAGE, "coverage");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_DEPTH_GREATER_EQUAL, "depthgreaterequal");
    STRINGISE_ENUM_CLASS_NAMED(SVNAME_DEPTH_LESS_EQUAL, "depthlessequal");
  }
  END_ENUM_STRINGISE();
}
