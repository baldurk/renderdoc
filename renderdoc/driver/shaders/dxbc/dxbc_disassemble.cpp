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
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "dxbc_bytecode.h"

#include "dxbc_container.h"

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

size_t NumOperands(OpcodeType op);
rdcstr toString(const uint32_t values[], uint32_t numComps);
char *toString(OpcodeType op);
char *toString(ResourceDimension dim);
char *toString(DXBC::ResourceRetType type);
char *toString(ResinfoRetType type);
char *toString(InterpolationMode type);
char *SystemValueToString(DXBC::SVSemantic type);
bool IsDeclaration(OpcodeType op);

bool Operand::operator==(const Operand &o) const
{
  if(type != o.type)
    return false;
  if(numComponents != o.numComponents)
    return false;
  if(memcmp(comps, o.comps, 4))
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

      param.compType = CompType::UInt;
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

  const bool friendly = RenderDoc::Inst().GetConfigSetting("Disassembly_FriendlyNaming") != "0";

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

  switch(m_Type)
  {
    case DXBC::ShaderType::Pixel: m_Disassembly += "ps_"; break;
    case DXBC::ShaderType::Vertex: m_Disassembly += "vs_"; break;
    case DXBC::ShaderType::Geometry: m_Disassembly += "gs_"; break;
    case DXBC::ShaderType::Hull: m_Disassembly += "hs_"; break;
    case DXBC::ShaderType::Domain: m_Disassembly += "ds_"; break;
    case DXBC::ShaderType::Compute: m_Disassembly += "cs_"; break;
    default: RDCERR("Unknown shader type: %u", m_Type); break;
  }

  m_Disassembly += StringFormat::Fmt("%d_%d\n", m_Major, m_Minor);

  int indent = 0;

  size_t d = 0;

  LineColumnInfo prevLineInfo;

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
          m_Disassembly += "\n";
        break;
      }

      m_Disassembly += "      ";
      m_Disassembly += m_Declarations[d].str;
      m_Disassembly += "\n";
    }

    if(m_Instructions[i].operation == OPCODE_ENDIF || m_Instructions[i].operation == OPCODE_ENDLOOP)
    {
      indent--;
    }

    if(m_DebugInfo)
    {
      LineColumnInfo lineInfo = prevLineInfo;

      m_DebugInfo->GetLineInfo(debugInst, m_Instructions[i].offset, lineInfo);

      if(lineInfo.fileIndex >= 0 && lineInfo.lineStart >= 0 &&
         (lineInfo.fileIndex != prevLineInfo.fileIndex ||
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

        if(((lineInfo.fileIndex != prevLineInfo.fileIndex ||
             lineInfo.callstack.back() != prevLineInfo.callstack.back()) &&
            lineInfo.fileIndex < (int32_t)fileLines.size()) ||
           line == "")
        {
          m_Disassembly += "      ";    // "0000: "
          for(int in = 0; in < indent; in++)
            m_Disassembly += "  ";

          rdcstr func = lineInfo.callstack.back();

          if(!func.empty())
          {
            m_Disassembly += StringFormat::Fmt("%s:%d - %s()\n",
                                               m_DebugInfo->Files[lineInfo.fileIndex].first.c_str(),
                                               lineInfo.lineStart, func.c_str());
          }
          else
          {
            m_Disassembly += StringFormat::Fmt(
                "%s:%d\n", m_DebugInfo->Files[lineInfo.fileIndex].first.c_str(), lineInfo.lineStart);
          }
        }

        if(line != "")
        {
          m_Disassembly += "      ";    // "0000: "
          for(int in = 0; in < indent; in++)
            m_Disassembly += "  ";
          m_Disassembly += line + "\n";
        }
      }

      prevLineInfo = lineInfo;
    }

    int curIndent = indent;
    if(m_Instructions[i].operation == OPCODE_ELSE)
      curIndent--;

    rdcstr whitespace;
    whitespace.fill(curIndent * 2, ' ');

    m_Disassembly +=
        StringFormat::Fmt("% 4u: %s%s\n", i, whitespace.c_str(), m_Instructions[i].str.c_str());

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
          "[%s + 0]",
          retOper.indices[idx].operand.toString(m_Reflection, flags | ToString::ShowSwizzle).c_str());
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
    const uint32_t voffs = byteOffset + v.descriptor.offset;

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
              uint32_t vecOffset = (var->descriptor.offset & 0xf);
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
        if(i == 0 && (type == TYPE_CONSTANT_BUFFER || type == TYPE_INDEXABLE_TEMP))
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

  retDecl.str = toString(op);

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
      uint32_t float4size = tokenStream[0];
      tokenStream++;

      retDecl.str += StringFormat::Fmt("[%u]", float4size);
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

    retDecl.str += retDecl.operand.toString(m_Reflection, flags | ToString::ShowSwizzle);
  }
  else if(op == OPCODE_DCL_TEMPS)
  {
    retDecl.numTemps = tokenStream[0];

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
    retDecl.str += SystemValueToString(retDecl.systemValue);
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
    retDecl.str += toString(retDecl.dim);
    retDecl.str += " ";

    retDecl.str += "(";
    retDecl.str += toString(retDecl.resType[0]);
    retDecl.str += ",";
    retDecl.str += toString(retDecl.resType[1]);
    retDecl.str += ",";
    retDecl.str += toString(retDecl.resType[2]);
    retDecl.str += ",";
    retDecl.str += toString(retDecl.resType[3]);
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
    retDecl.str += toString(retDecl.interpolation);

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
    retDecl.str += toString(retDecl.dim);

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
    retDecl.str += toString(retDecl.resType[0]);
    retDecl.str += ",";
    retDecl.str += toString(retDecl.resType[1]);
    retDecl.str += ",";
    retDecl.str += toString(retDecl.resType[2]);
    retDecl.str += ",";
    retDecl.str += toString(retDecl.resType[3]);
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

  retOp.str = toString(op);

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

        retOp.str +=
            StringFormat::Fmt("_indexable(%s, stride=%u)", toString(retOp.resDim), retOp.stride);
      }
      else
      {
        retOp.str += "(";
        retOp.str += toString(retOp.resDim);
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
      retOp.str += toString(retOp.resType[0]);
      retOp.str += ",";
      retOp.str += toString(retOp.resType[1]);
      retOp.str += ",";
      retOp.str += toString(retOp.resType[2]);
      retOp.str += ",";
      retOp.str += toString(retOp.resType[3]);
      retOp.str += ")";
    }

    extended = ExtendedOpcode::Extended.Get(OpcodeTokenN) == 1;

    tokenStream++;
  }

  if(op == OPCODE_RESINFO)
  {
    retOp.str += "_";
    retOp.str += toString(retOp.resinfoRetType);
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

////////////////////////////////////////////////////////////
// boring tedious long switch statement style functions

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
    case OPCODE_GATHER4_PO_C_FEEDBACK:
      return 7;

    // custom data doesn't have particular operands
    case OPCODE_CUSTOMDATA:
    default: break;
  }

  RDCERR("Unknown opcode: %u", op);
  return 0xffffffff;
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

char *toString(OpcodeType op)
{
  switch(op)
  {
    case OPCODE_ADD: return "add";
    case OPCODE_AND: return "and";
    case OPCODE_BREAK: return "break";
    case OPCODE_BREAKC: return "breakc";
    case OPCODE_CALL: return "call";
    case OPCODE_CALLC: return "callc";
    case OPCODE_CASE: return "case";
    case OPCODE_CONTINUE: return "continue";
    case OPCODE_CONTINUEC: return "continuec";
    case OPCODE_CUT: return "cut";
    case OPCODE_DEFAULT: return "default";
    case OPCODE_DERIV_RTX: return "deriv_rtx";
    case OPCODE_DERIV_RTY: return "deriv_rty";
    case OPCODE_DISCARD: return "discard";
    case OPCODE_DIV: return "div";
    case OPCODE_DP2: return "dp2";
    case OPCODE_DP3: return "dp3";
    case OPCODE_DP4: return "dp4";
    case OPCODE_ELSE: return "else";
    case OPCODE_EMIT: return "emit";
    case OPCODE_EMITTHENCUT: return "emitthencut";
    case OPCODE_ENDIF: return "endif";
    case OPCODE_ENDLOOP: return "endloop";
    case OPCODE_ENDSWITCH: return "endswitch";
    case OPCODE_EQ: return "eq";
    case OPCODE_EXP: return "exp";
    case OPCODE_FRC: return "frc";
    case OPCODE_FTOI: return "ftoi";
    case OPCODE_FTOU: return "ftou";
    case OPCODE_GE: return "ge";
    case OPCODE_IADD: return "iadd";
    case OPCODE_IF: return "if";
    case OPCODE_IEQ: return "ieq";
    case OPCODE_IGE: return "ige";
    case OPCODE_ILT: return "ilt";
    case OPCODE_IMAD: return "imad";
    case OPCODE_IMAX: return "imax";
    case OPCODE_IMIN: return "imin";
    case OPCODE_IMUL: return "imul";
    case OPCODE_INE: return "ine";
    case OPCODE_INEG: return "ineg";
    case OPCODE_ISHL: return "ishl";
    case OPCODE_ISHR: return "ishr";
    case OPCODE_ITOF: return "itof";
    case OPCODE_LABEL: return "label";
    case OPCODE_LD: return "ld_indexable";
    case OPCODE_LD_MS: return "ld_ms";
    case OPCODE_LOG: return "log";
    case OPCODE_LOOP: return "loop";
    case OPCODE_LT: return "lt";
    case OPCODE_MAD: return "mad";
    case OPCODE_MIN: return "min";
    case OPCODE_MAX: return "max";
    case OPCODE_CUSTOMDATA: return "customdata";
    case OPCODE_MOV: return "mov";
    case OPCODE_MOVC: return "movc";
    case OPCODE_MUL: return "mul";
    case OPCODE_NE: return "ne";
    case OPCODE_NOP: return "nop";
    case OPCODE_NOT: return "not";
    case OPCODE_OR: return "or";
    case OPCODE_RESINFO: return "resinfo_indexable";
    case OPCODE_RET: return "ret";
    case OPCODE_RETC: return "retc";
    case OPCODE_ROUND_NE: return "round_ne";
    case OPCODE_ROUND_NI: return "round_ni";
    case OPCODE_ROUND_PI: return "round_pi";
    case OPCODE_ROUND_Z: return "round_z";
    case OPCODE_RSQ: return "rsq";
    case OPCODE_SAMPLE: return "sample_indexable";
    case OPCODE_SAMPLE_C: return "sample_c";
    case OPCODE_SAMPLE_C_LZ: return "sample_c_lz";
    case OPCODE_SAMPLE_L: return "sample_l";
    case OPCODE_SAMPLE_D: return "sample_d";
    case OPCODE_SAMPLE_B: return "sample_b";
    case OPCODE_SQRT: return "sqrt";
    case OPCODE_SWITCH: return "switch";
    case OPCODE_SINCOS: return "sincos";
    case OPCODE_UDIV: return "udiv";
    case OPCODE_ULT: return "ult";
    case OPCODE_UGE: return "uge";
    case OPCODE_UMUL: return "umul";
    case OPCODE_UMAD: return "umad";
    case OPCODE_UMAX: return "umax";
    case OPCODE_UMIN: return "umin";
    case OPCODE_USHR: return "ushr";
    case OPCODE_UTOF: return "utof";
    case OPCODE_XOR: return "xor";
    case OPCODE_DCL_RESOURCE: return "dcl_resource";
    case OPCODE_DCL_CONSTANT_BUFFER: return "dcl_constantbuffer";
    case OPCODE_DCL_SAMPLER: return "dcl_sampler";
    case OPCODE_DCL_INDEX_RANGE: return "dcl_indexRange";
    case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY: return "dcl_outputtopology";
    case OPCODE_DCL_GS_INPUT_PRIMITIVE: return "dcl_inputprimitive";
    case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT: return "dcl_maxout";
    case OPCODE_DCL_INPUT: return "dcl_input";
    case OPCODE_DCL_INPUT_SGV: return "dcl_input_sgv";
    case OPCODE_DCL_INPUT_SIV: return "dcl_input_siv";
    case OPCODE_DCL_INPUT_PS: return "dcl_input_ps";
    case OPCODE_DCL_INPUT_PS_SGV: return "dcl_input_ps_sgv";
    case OPCODE_DCL_INPUT_PS_SIV: return "dcl_input_ps_siv";
    case OPCODE_DCL_OUTPUT: return "dcl_output";
    case OPCODE_DCL_OUTPUT_SGV: return "dcl_output_sgv";
    case OPCODE_DCL_OUTPUT_SIV: return "dcl_output_siv";
    case OPCODE_DCL_TEMPS: return "dcl_temps";
    case OPCODE_DCL_INDEXABLE_TEMP: return "dcl_indexableTemp";
    case OPCODE_DCL_GLOBAL_FLAGS: return "dcl_globalFlags";
    case OPCODE_LOD: return "lod";
    case OPCODE_GATHER4: return "gather4";
    case OPCODE_SAMPLE_POS: return "samplepos";
    case OPCODE_SAMPLE_INFO: return "sample_info";
    case OPCODE_HS_DECLS: return "hs_decls";
    case OPCODE_HS_CONTROL_POINT_PHASE: return "hs_control_point_phase";
    case OPCODE_HS_FORK_PHASE: return "hs_fork_phase";
    case OPCODE_HS_JOIN_PHASE: return "hs_join_phase";
    case OPCODE_EMIT_STREAM: return "emit_stream";
    case OPCODE_CUT_STREAM: return "cut_stream";
    case OPCODE_EMITTHENCUT_STREAM: return "emitThenCut_stream";
    case OPCODE_INTERFACE_CALL: return "fcall";
    case OPCODE_BUFINFO: return "bufinfo";
    case OPCODE_DERIV_RTX_COARSE: return "deriv_rtx_coarse";
    case OPCODE_DERIV_RTX_FINE: return "deriv_rtx_fine";
    case OPCODE_DERIV_RTY_COARSE: return "deriv_rty_coarse";
    case OPCODE_DERIV_RTY_FINE: return "deriv_rty_fine";
    case OPCODE_GATHER4_C: return "gather4_c";
    case OPCODE_GATHER4_PO: return "gather4_po";
    case OPCODE_GATHER4_PO_C: return "gather4_po_c";
    case OPCODE_RCP: return "rcp";
    case OPCODE_F32TOF16: return "f32tof16";
    case OPCODE_F16TOF32: return "f16tof32";
    case OPCODE_UADDC: return "uaddc";
    case OPCODE_USUBB: return "usubb";
    case OPCODE_COUNTBITS: return "countbits";
    case OPCODE_FIRSTBIT_HI: return "firstbit_hi";
    case OPCODE_FIRSTBIT_LO: return "firstbit_lo";
    case OPCODE_FIRSTBIT_SHI: return "firstbit_shi";
    case OPCODE_UBFE: return "ubfe";
    case OPCODE_IBFE: return "ibfe";
    case OPCODE_BFI: return "bfi";
    case OPCODE_BFREV: return "bfrev";
    case OPCODE_SWAPC: return "swapc";
    case OPCODE_DCL_STREAM: return "dcl_stream";
    case OPCODE_DCL_FUNCTION_BODY: return "dcl_function_body";
    case OPCODE_DCL_FUNCTION_TABLE: return "dcl_function_table";
    case OPCODE_DCL_INTERFACE: return "dcl_interface";
    case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT: return "dcl_input_control_point_count";
    case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT: return "dcl_output_control_point_count";
    case OPCODE_DCL_TESS_DOMAIN: return "dcl_tessellator_domain";
    case OPCODE_DCL_TESS_PARTITIONING: return "dcl_tessellator_partitioning";
    case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE: return "dcl_tessellator_output_primitive";
    case OPCODE_DCL_HS_MAX_TESSFACTOR: return "dcl_hs_max_tessfactor";
    case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT: return "dcl_hs_fork_phase_instance_count";
    case OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT: return "dcl_hs_join_phase_instance_count";
    case OPCODE_DCL_THREAD_GROUP: return "dcl_thread_group";
    case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: return "dcl_uav_typed";
    case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: return "dcl_uav_raw";
    case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: return "dcl_uav_structured";
    case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW: return "dcl_tgsm_raw";
    case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: return "dcl_tgsm_structured";
    case OPCODE_DCL_RESOURCE_RAW: return "dcl_resource_raw";
    case OPCODE_DCL_RESOURCE_STRUCTURED: return "dcl_resource_structured";
    case OPCODE_LD_UAV_TYPED: return "ld_uav_typed";
    case OPCODE_STORE_UAV_TYPED: return "store_uav_typed";
    case OPCODE_LD_RAW: return "ld_raw";
    case OPCODE_STORE_RAW: return "store_raw";
    case OPCODE_LD_STRUCTURED: return "ld_structured";
    case OPCODE_STORE_STRUCTURED: return "store_structured";
    case OPCODE_ATOMIC_AND: return "atomic_and";
    case OPCODE_ATOMIC_OR: return "atomic_or";
    case OPCODE_ATOMIC_XOR: return "atomic_xor";
    case OPCODE_ATOMIC_CMP_STORE: return "atomic_cmp_store";
    case OPCODE_ATOMIC_IADD: return "atomic_iadd";
    case OPCODE_ATOMIC_IMAX: return "atomic_imax";
    case OPCODE_ATOMIC_IMIN: return "atomic_imin";
    case OPCODE_ATOMIC_UMAX: return "atomic_umax";
    case OPCODE_ATOMIC_UMIN: return "atomic_umin";
    case OPCODE_IMM_ATOMIC_ALLOC: return "imm_atomic_alloc";
    case OPCODE_IMM_ATOMIC_CONSUME: return "imm_atomic_consume";
    case OPCODE_IMM_ATOMIC_IADD: return "imm_atomic_iadd";
    case OPCODE_IMM_ATOMIC_AND: return "imm_atomic_and";
    case OPCODE_IMM_ATOMIC_OR: return "imm_atomic_or";
    case OPCODE_IMM_ATOMIC_XOR: return "imm_atomic_xor";
    case OPCODE_IMM_ATOMIC_EXCH: return "imm_atomic_exch";
    case OPCODE_IMM_ATOMIC_CMP_EXCH: return "imm_atomic_cmp_exch";
    case OPCODE_IMM_ATOMIC_IMAX: return "imm_atomic_imax";
    case OPCODE_IMM_ATOMIC_IMIN: return "imm_atomic_imin";
    case OPCODE_IMM_ATOMIC_UMAX: return "imm_atomic_umax";
    case OPCODE_IMM_ATOMIC_UMIN: return "imm_atomic_umin";
    case OPCODE_SYNC: return "sync";
    case OPCODE_DADD: return "dadd";
    case OPCODE_DMAX: return "dmax";
    case OPCODE_DMIN: return "dmin";
    case OPCODE_DMUL: return "dmul";
    case OPCODE_DEQ: return "deq";
    case OPCODE_DGE: return "dge";
    case OPCODE_DLT: return "dlt";
    case OPCODE_DNE: return "dne";
    case OPCODE_DMOV: return "dmov";
    case OPCODE_DMOVC: return "dmovc";
    case OPCODE_DTOF: return "dtof";
    case OPCODE_FTOD: return "ftod";
    case OPCODE_EVAL_SNAPPED: return "eval_snapped";
    case OPCODE_EVAL_SAMPLE_INDEX: return "eval_sample_index";
    case OPCODE_EVAL_CENTROID: return "eval_centroid";
    case OPCODE_DCL_GS_INSTANCE_COUNT: return "dcl_gs_instance_count";
    case OPCODE_ABORT: return "abort";
    case OPCODE_DEBUGBREAK: return "debugbreak";

    case OPCODE_DDIV: return "ddiv";
    case OPCODE_DFMA: return "dfma";
    case OPCODE_DRCP: return "drcp";

    case OPCODE_MSAD: return "msad";

    case OPCODE_DTOI: return "dtoi";
    case OPCODE_DTOU: return "dtou";
    case OPCODE_ITOD: return "itod";
    case OPCODE_UTOD: return "utod";

    case OPCODE_GATHER4_FEEDBACK: return "gather4_statusk";
    case OPCODE_GATHER4_C_FEEDBACK: return "gather4_c_status";
    case OPCODE_GATHER4_PO_FEEDBACK: return "gather4_po_statusk";
    case OPCODE_GATHER4_PO_C_FEEDBACK: return "gather4_po_c_status";
    case OPCODE_LD_FEEDBACK: return "ld";
    case OPCODE_LD_MS_FEEDBACK: return "ld_ms_status";
    case OPCODE_LD_UAV_TYPED_FEEDBACK: return "ld_uav_typed_status";
    case OPCODE_LD_RAW_FEEDBACK: return "ld_raw_status";
    case OPCODE_LD_STRUCTURED_FEEDBACK: return "ld_structured_status";
    case OPCODE_SAMPLE_L_FEEDBACK: return "sample_l_status";
    case OPCODE_SAMPLE_C_LZ_FEEDBACK: return "sample_c_lz_status";

    case OPCODE_SAMPLE_CLAMP_FEEDBACK: return "sample_status";
    case OPCODE_SAMPLE_B_CLAMP_FEEDBACK: return "sample_b_status";
    case OPCODE_SAMPLE_D_CLAMP_FEEDBACK: return "sample_d_status";
    case OPCODE_SAMPLE_C_CLAMP_FEEDBACK: return "sample_c_status";

    case OPCODE_CHECK_ACCESS_FULLY_MAPPED: return "check_access_fully_mapped";

    default: break;
  }

  RDCERR("Unknown opcode: %u", op);
  return "";
}

char *toString(ResourceDimension dim)
{
  switch(dim)
  {
    case RESOURCE_DIMENSION_UNKNOWN: return "unknown";
    case RESOURCE_DIMENSION_BUFFER: return "buffer";
    case RESOURCE_DIMENSION_TEXTURE1D: return "texture1d";
    case RESOURCE_DIMENSION_TEXTURE2D: return "texture2d";
    case RESOURCE_DIMENSION_TEXTURE2DMS: return "texture2dms";
    case RESOURCE_DIMENSION_TEXTURE3D: return "texture3d";
    case RESOURCE_DIMENSION_TEXTURECUBE: return "texturecube";
    case RESOURCE_DIMENSION_TEXTURE1DARRAY: return "texture1darray";
    case RESOURCE_DIMENSION_TEXTURE2DARRAY: return "texture2darray";
    case RESOURCE_DIMENSION_TEXTURE2DMSARRAY: return "texture2dmsarray";
    case RESOURCE_DIMENSION_TEXTURECUBEARRAY: return "texturecubearray";
    case RESOURCE_DIMENSION_RAW_BUFFER: return "rawbuffer";
    case RESOURCE_DIMENSION_STRUCTURED_BUFFER: return "structured_buffer";
    default: break;
  }

  RDCERR("Unknown dim: %u", dim);
  return "";
}

char *toString(DXBC::ResourceRetType type)
{
  switch(type)
  {
    case DXBC::RETURN_TYPE_UNORM: return "unorm";
    case DXBC::RETURN_TYPE_SNORM: return "snorm";
    case DXBC::RETURN_TYPE_SINT: return "sint";
    case DXBC::RETURN_TYPE_UINT: return "uint";
    case DXBC::RETURN_TYPE_FLOAT: return "float";
    case DXBC::RETURN_TYPE_MIXED: return "mixed";
    case DXBC::RETURN_TYPE_DOUBLE: return "double";
    case DXBC::RETURN_TYPE_CONTINUED: return "continued";
    case DXBC::RETURN_TYPE_UNUSED: return "unused";
    default: break;
  }

  RDCERR("Unknown type: %u", type);
  return "";
}

char *toString(ResinfoRetType type)
{
  switch(type)
  {
    case RETTYPE_FLOAT: return "float";
    case RETTYPE_RCPFLOAT: return "rcpfloat";
    case RETTYPE_UINT: return "uint";
    default: break;
  }

  RDCERR("Unknown type: %u", type);
  return "";
}

char *toString(InterpolationMode interp)
{
  switch(interp)
  {
    case INTERPOLATION_UNDEFINED: return "undefined";
    case INTERPOLATION_CONSTANT: return "constant";
    case INTERPOLATION_LINEAR: return "linear";
    case INTERPOLATION_LINEAR_CENTROID: return "linearCentroid";
    case INTERPOLATION_LINEAR_NOPERSPECTIVE: return "linearNopersp";
    case INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID: return "linearNoperspCentroid";
    case INTERPOLATION_LINEAR_SAMPLE: return "linearSample";
    case INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE: return "linaerNoperspSample";
    default: break;
  }

  RDCERR("Unknown interp: %u", interp);
  return "";
}

char *SystemValueToString(DXBC::SVSemantic name)
{
  switch(name)
  {
    case DXBC::SVNAME_POSITION: return "position";
    case DXBC::SVNAME_CLIP_DISTANCE: return "clipdistance";
    case DXBC::SVNAME_CULL_DISTANCE: return "culldistance";
    case DXBC::SVNAME_RENDER_TARGET_ARRAY_INDEX: return "rendertarget_array_index";
    case DXBC::SVNAME_VIEWPORT_ARRAY_INDEX: return "viewport_array_index";
    case DXBC::SVNAME_VERTEX_ID: return "vertexid";
    case DXBC::SVNAME_PRIMITIVE_ID: return "primitiveid";
    case DXBC::SVNAME_INSTANCE_ID: return "instanceid";
    case DXBC::SVNAME_IS_FRONT_FACE: return "isfrontface";
    case DXBC::SVNAME_SAMPLE_INDEX:
      return "sampleidx";

    // tessellation factors don't correspond directly to their enum values

    // SVNAME_FINAL_QUAD_EDGE_TESSFACTOR
    case DXBC::SVNAME_FINAL_QUAD_EDGE_TESSFACTOR0: return "finalQuadUeq0EdgeTessFactor";
    case DXBC::SVNAME_FINAL_QUAD_EDGE_TESSFACTOR1: return "finalQuadVeq0EdgeTessFactor";
    case DXBC::SVNAME_FINAL_QUAD_EDGE_TESSFACTOR2: return "finalQuadUeq1EdgeTessFactor";
    case DXBC::SVNAME_FINAL_QUAD_EDGE_TESSFACTOR3:
      return "finalQuadVeq1EdgeTessFactor";

    // SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR
    case DXBC::SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR0: return "finalQuadUInsideTessFactor";
    case DXBC::SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR1:
      return "finalQuadVInsideTessFactor";

    // SVNAME_FINAL_TRI_EDGE_TESSFACTOR
    case DXBC::SVNAME_FINAL_TRI_EDGE_TESSFACTOR0: return "finalTriUeq0EdgeTessFactor";
    case DXBC::SVNAME_FINAL_TRI_EDGE_TESSFACTOR1: return "finalTriVeq0EdgeTessFactor";
    case DXBC::SVNAME_FINAL_TRI_EDGE_TESSFACTOR2:
      return "finalTriWeq0EdgeTessFactor";

    // SVNAME_FINAL_TRI_INSIDE_TESSFACTOR
    case DXBC::SVNAME_FINAL_TRI_INSIDE_TESSFACTOR:
      return "finalTriInsideTessFactor";

    // SVNAME_FINAL_LINE_DETAIL_TESSFACTOR
    case DXBC::SVNAME_FINAL_LINE_DETAIL_TESSFACTOR:
      return "finalLineEdgeTessFactor";

    // SVNAME_FINAL_LINE_DENSITY_TESSFACTOR
    case DXBC::SVNAME_FINAL_LINE_DENSITY_TESSFACTOR: return "finalLineInsideTessFactor";

    case DXBC::SVNAME_TARGET: return "target";
    case DXBC::SVNAME_DEPTH: return "depth";
    case DXBC::SVNAME_COVERAGE: return "coverage";
    case DXBC::SVNAME_DEPTH_GREATER_EQUAL: return "depthgreaterequal";
    case DXBC::SVNAME_DEPTH_LESS_EQUAL: return "depthlessequal";
    default: break;
  }

  RDCERR("Unknown name: %u", name);
  return "";
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

};    // namespace DXBCBytecode
