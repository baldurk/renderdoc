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

#include <math.h>
#include "common/common.h"
#include "core/core.h"
#include "core/settings.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "dxbc_bytecode.h"

#include "dxbc_container.h"

#include "driver/ihv/nv/nvapi_wrapper.h"

RDOC_CONFIG(bool, DXBC_Disassembly_FriendlyNaming, true,
            "Where possible (i.e. it is completely unambiguous) replace register names with "
            "high-level variable names.");
RDOC_CONFIG(bool, DXBC_Disassembly_ProcessVendorShaderExts, true,
            "Process vendor shader extensions from magic UAV encoded instructions into the real "
            "operations.\n"
            "If this is disabled, shader debugging won't produce correct results.");

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

namespace AMDInstruction
{
// ha ha these are different :(
enum class DX11Op
{
  Readfirstlane = 0x01,
  Readlane = 0x02,
  LaneId = 0x03,
  Swizzle = 0x04,
  Ballot = 0x05,
  MBCnt = 0x06,
  Min3U = 0x08,
  Min3F = 0x09,
  Med3U = 0x0a,
  Med3F = 0x0b,
  Max3U = 0x0c,
  Max3F = 0x0d,
  BaryCoord = 0x0e,
  VtxParam = 0x0f,
  ViewportIndex = 0x10,
  RtArraySlice = 0x11,
  WaveReduce = 0x12,
  WaveScan = 0x13,
  DrawIndex = 0x17,
  AtomicU64 = 0x18,
  GetWaveSize = 0x19,
  BaseInstance = 0x1a,
  BaseVertex = 0x1b,
};

enum class DX12Op
{
  Readfirstlane = 0x01,
  Readlane = 0x02,
  LaneId = 0x03,
  Swizzle = 0x04,
  Ballot = 0x05,
  MBCnt = 0x06,
  Min3U = 0x07,
  Min3F = 0x08,
  Med3U = 0x09,
  Med3F = 0x0a,
  Max3U = 0x0b,
  Max3F = 0x0c,
  BaryCoord = 0x0d,
  VtxParam = 0x0e,
  ViewportIndex = 0x10,    // DX11 only
  RtArraySlice = 0x11,     // DX11 only
  WaveReduce = 0x12,
  WaveScan = 0x13,
  LoadDwAtAddr = 0x14,
  DrawIndex = 0x17,
  AtomicU64 = 0x18,
  GetWaveSize = 0x19,
  BaseInstance = 0x1a,
  BaseVertex = 0x1b,
};

DX12Op convert(DX11Op op)
{
  switch(op)
  {
    // convert opcodes that don't match up
    case DX11Op::Min3U: return DX12Op::Min3U;
    case DX11Op::Min3F: return DX12Op::Min3F;
    case DX11Op::Med3U: return DX12Op::Med3U;
    case DX11Op::Med3F: return DX12Op::Med3F;
    case DX11Op::Max3U: return DX12Op::Max3U;
    case DX11Op::Max3F: return DX12Op::Max3F;
    case DX11Op::BaryCoord: return DX12Op::BaryCoord;
    case DX11Op::VtxParam:
      return DX12Op::VtxParam;
    // others match up exactly
    default: return DX12Op(op);
  }
}

enum BaryInterpMode
{
  LinearCenter = 1,
  LinearCentroid = 2,
  LinearSample = 3,
  PerspCenter = 4,
  PerspCentroid = 5,
  PerspSample = 6,
  PerspPullModel = 7,
};

enum SwizzleMask
{
  SwapX1 = 0x041f,
  SwapX2 = 0x081f,
  SwapX4 = 0x101f,
  SwapX8 = 0x201f,
  SwapX16 = 0x401f,
  ReverseX4 = 0x0c1f,
  ReverseX8 = 0x1c1f,
  ReverseX16 = 0x3c1f,
  ReverseX32 = 0x7c1f,
  BCastX2 = 0x003e,
  BCastX4 = 0x003c,
  BCastX8 = 0x0038,
  BCastX16 = 0x0030,
  BCastX32 = 0x0020,
};

enum AMDAtomic
{
  Min = 0x01,
  Max = 0x02,
  And = 0x03,
  Or = 0x04,
  Xor = 0x05,
  Add = 0x06,
  Xchg = 0x07,
  CmpXchg = 0x08,
};

VendorAtomicOp convert(AMDAtomic op)
{
  switch(op)
  {
    case Min: return ATOMIC_OP_MIN;
    case Max: return ATOMIC_OP_MAX;
    case And: return ATOMIC_OP_AND;
    case Or: return ATOMIC_OP_OR;
    case Xor: return ATOMIC_OP_XOR;
    case Add: return ATOMIC_OP_ADD;
    case Xchg: return ATOMIC_OP_SWAP;
    case CmpXchg: return ATOMIC_OP_CAS;
    default: return ATOMIC_OP_NONE;
  }
}

static MaskedElement<uint32_t, 0xF0000000> Magic;
static MaskedElement<uint32_t, 0x03000000> Phase;
static MaskedElement<uint32_t, 0x00FFFF00> Data;
static MaskedElement<BaryInterpMode, 0x00FFFF00> BaryInterp;
static MaskedElement<SwizzleMask, 0x00FFFF00> SwizzleOp;
static MaskedElement<DX11Op, 0x000000FF> Opcode11;
static MaskedElement<DX12Op, 0x000000FF> Opcode12;

static MaskedElement<uint8_t, 0x00018000> VtxParamComponent;
static MaskedElement<uint32_t, 0x00001F00> VtxParamParameter;
static MaskedElement<uint32_t, 0x00006000> VtxParamVertex;

static MaskedElement<uint32_t, 0x0000FF00> WaveOp;
static MaskedElement<uint32_t, 0x00FF0000> WaveOpFlags;

static MaskedElement<AMDAtomic, 0x0000FF00> AtomicOp;
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
        Opcode::HasOrderPreservingCounter.Get(OpcodeToken0)) ||
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

  if(DXBC_Disassembly_ProcessVendorShaderExts() && m_ShaderExt.second != ~0U)
    PostprocessVendorExtensions();
}

void Program::PostprocessVendorExtensions()
{
  const bool friendly = DXBC_Disassembly_FriendlyNaming();

  uint32_t magicID = ~0U;

  for(size_t i = 0; i < m_Declarations.size(); i++)
  {
    const Declaration &decl = m_Declarations[i];
    if((decl.operand.indices.size() == 1 && decl.operand.indices[0].index == m_ShaderExt.second) ||
       (decl.operand.indices.size() == 3 && decl.operand.indices[1].index == m_ShaderExt.second &&
        decl.space == m_ShaderExt.first))
    {
      magicID = (uint32_t)decl.operand.indices[0].index;
      m_Declarations.erase(i);
      break;
    }
  }

  // now we know the UAV, iterate the instructions looking for patterns to replace.
  //
  // AMD is nice and easy. Every instruction works on a scalar (vector versions repeat for each
  // component) and is encoded into a single InterlockedCompareExchange on the UAV.
  // So we can simply replace them in-place by decoding.
  //
  // NV's are not as nice. They are demarcated by IncrementCounter on the UAV so we know we'll see
  // a linear stream without re-ordering, but they *can* be intermixed with other non-intrinsic
  // instructions. Parameters and data are set by writing to specific offsets within the structure
  //
  // There are two types:
  //
  // Simpler, instructions that work purely on vars and not on resources. Shuffle/ballot/etc
  //
  // These come in the form:
  // index = magicUAV.IncrementCounter()
  // set params and opcode by writing to magicUAV[index].member...
  // retval = magicUAV.IncrementCounter()
  // [optional (see below): retval2 = magicUAV.IncrementCounter()]
  //
  // This type of operand returns the result with the closing IncrementCounter(). There could be
  // multiple results, so numOutputs is set before any, and then that many IncrementCounter() are
  // emitted with each result.
  //
  // More complex, instructions that use UAVs. Mostly atomics
  //
  // index1 = magicUAV.IncrementCounter()
  // magicUAV[index1].markUAV = 1;
  // userUAV[index1] = 0; // or some variation of such
  // index2 = magicUAV.IncrementCounter()
  // set params and opcode as above in magicUAV[index2].member...
  // retval = magicUAV[index2].dst
  //
  // Also note that if the shader doesn't use the return result of an atomic, the dst may never be
  // read!
  //
  // The difficulty then is distinguishing between the two and knowing where the boundaries are.
  // We do this with a simple state machine tracking where we are in an opcode:
  //
  //         +----------> Nothing
  //         |              v
  //         |              |
  //         |      IncrementCounter()
  // Emit instruction       |
  //         |              v
  //         |         Instruction >--write markUAV---> UAV instruction header
  //         |              v                           (wait for other UAV write)
  //         |              |                             v
  //         |              |                             |
  //         |         write opcode    ]                  |
  //         |              |          ]                  |
  //         |              v          ] simple           |
  //         |        Instruction Body ] case             |
  //         |              v          ]                  |
  //         |              |          ]                  |
  //         |      IncrementCounter() ]                  |
  //         |              |                             |
  //         +----<---------+                             |
  //         |                                    IncrementCounter()
  //         |                                            |
  //         |    UAV instruction body <------------------+
  //         |              v
  //         |              |
  //         |         write opcode
  //         |              |
  //         +--------------+
  //
  // so most state transitions are marked by an IncrementCounter(). The exceptions being
  // Instruction where we wait for a write to either markUAV or opcode to move to either simple
  // instruction body or to the UAV instruction header, and UAV instruction body which leaves
  // when we see an opcode write.
  //
  // We assume that markUAV will be written BEFORE the fake UAV write. It's not entirely clear if
  // this is guaranteed to not be re-ordered but it seems to be true and it's implied that NV's
  // driver relies on this. This simplifies tracking since we can use it as a state transition.
  //
  // We also assume that multiple accesses to the UAV don't overlap. This should be guaranteed by
  // the use of the index from the counter being used for access. However we don't actually check
  // the index itself.
  //
  // all src/dst are uint4, others are all uint

  enum class InstructionState
  {
    // if something goes wrong we enter this state and stop patching
    Broken,

    Nothing,

    // this is a state only used for AMD's UAV atomic op, which takes more parameters and uses the
    // operation phases.
    AMDUAVAtomic,

    // this is the state when we're not sure what type we are. Either markUAV is written, in which
    // case we move to UAVInstructionHeader1, or opcode is written, in which case we move to
    // Instruction1Out. We should see one or the other.
    //
    // FP16 UAV instructions (NV_EXTN_OP_FP16_ATOMIC) that operate on float4 resources have two
    // return values. Unfortunately we can't reliably detect this from the bytecode, so what
    // happens is that when we see opode get written if it's NV_EXTN_OP_FP16_ATOMIC then we jump
    // straight to UAVInstructionBody and re-use the UAV instruction header from last time. We
    // know this MUST be a continuation because otherwise NV_EXTN_OP_FP16_ATOMIC is always
    // preceeded by a UAV instruction header (via markUAV).
    InstructionHeader,
    InstructionBody,
    // we move from Instruction1Out to this state when markUAV is written. The next UAV write is
    // used to determine the 'target' UAV.
    // We then move to header2 so we don't consume any other UAV writes.
    UAVInstructionHeader1,
    // here we do nothing but sit and wait for the IncrementCounter() so we can move to the UAV
    // body state
    UAVInstructionHeader2,
    // in this state we aren't sure exactly when to leave it. We wait *at least* until opcode is
    // written, but there may be more instructions after that to read from dst :(
    UAVInstructionBody,
  };

  enum class NvUAVParam
  {
    opcode = 0,
    src0 = 76,
    src1 = 92,
    src2 = 108,
    src3 = 28,
    src4 = 44,
    src5 = 60,
    dst = 124,
    markUAV = 140,
    numOutputs = 144,
  };

  InstructionState state = InstructionState::Nothing;

  NvShaderOpcode nvopcode = NvShaderOpcode::Unknown;
  Operand srcParam[8];
  Operand dstParam[4];
  Operand uavParam;
  int numOutputs = 0, outputsNeeded = 0;

  ToString flags = friendly ? ToString::FriendlyNameRegisters : ToString::None;

  for(size_t i = 0; i < m_Instructions.size(); i++)
  {
    // reserve space for an added instruction so that curOp can stay valid even if we insert a new
    // op. This only actually does work the first time (or after we've inserted a new
    // instruction).
    m_Instructions.reserve(m_Instructions.size() + 1);

    Operation &curOp = m_Instructions[i];

    if(state == InstructionState::Broken)
      break;

    if((curOp.operation == OPCODE_IMM_ATOMIC_CMP_EXCH &&
        curOp.operands[1].indices[0].index == magicID) ||
       (curOp.operation == OPCODE_ATOMIC_CMP_STORE && curOp.operands[0].indices[0].index == magicID))
    {
      // AMD operations where the return value isn't used becomes an atomic_cmp_store instead of
      // imm_atomic_cmp_exch

      const int32_t instructionIndex = curOp.operation == OPCODE_ATOMIC_CMP_STORE ? 1 : 2;
      const int32_t param0Index = instructionIndex + 1;
      const int32_t param1Index = param0Index + 1;

      Operand dstOperand = curOp.operands[0];
      // if we have a store there's no destination, so set it to null
      if(curOp.operation == OPCODE_ATOMIC_CMP_STORE)
      {
        dstOperand = Operand();
        dstOperand.type = TYPE_NULL;
        dstOperand.setComps(0xff, 0xff, 0xff, 0xff);
      }

      // AMD operation
      if(curOp.operands[instructionIndex].type != TYPE_IMMEDIATE32)
      {
        RDCERR(
            "Expected literal value for AMD extension instruction. Was the shader compiled with "
            "optimisations disabled?");
        state = InstructionState::Broken;
        break;
      }

      uint32_t instruction = curOp.operands[instructionIndex].values[0];

      if(AMDInstruction::Magic.Get(instruction) == 5)
      {
        AMDInstruction::DX12Op amdop;

        if(m_API == GraphicsAPI::D3D11)
          amdop = AMDInstruction::convert(AMDInstruction::Opcode11.Get(instruction));
        else
          amdop = AMDInstruction::Opcode12.Get(instruction);

        uint32_t phase = AMDInstruction::Phase.Get(instruction);
        if(phase == 0)
        {
          srcParam[0] = curOp.operands[param0Index];
          srcParam[1] = curOp.operands[param1Index];
        }
        else if(phase == 1)
        {
          srcParam[2] = curOp.operands[param0Index];
          srcParam[3] = curOp.operands[param1Index];
        }
        else if(phase == 2)
        {
          srcParam[4] = curOp.operands[param0Index];
          srcParam[5] = curOp.operands[param1Index];
        }
        else if(phase == 3)
        {
          srcParam[6] = curOp.operands[param0Index];
          srcParam[7] = curOp.operands[param1Index];
        }

        Operation op;

        switch(amdop)
        {
          case AMDInstruction::DX12Op::Readfirstlane:
          {
            op.operation = OPCODE_AMD_READFIRSTLANE;
            op.operands.resize(2);
            op.operands[0] = dstOperand;
            op.operands[1].name = "src";
            op.operands[1] = srcParam[0];
            break;
          }
          case AMDInstruction::DX12Op::Readlane:
          {
            op.operation = OPCODE_AMD_READLANE;
            op.operands.resize(3);
            op.operands[0] = dstOperand;
            op.operands[1].name = "src";
            op.operands[1] = srcParam[0];
            // lane is encoded in instruction data
            op.operands[2].name = "lane";
            op.operands[2].type = TYPE_IMMEDIATE32;
            op.operands[2].numComponents = NUMCOMPS_1;
            op.operands[2].values[0] = AMDInstruction::Data.Get(instruction);
            break;
          }
          case AMDInstruction::DX12Op::LaneId:
          {
            op.operation = OPCODE_AMD_LANEID;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::Swizzle:
          {
            op.operation = OPCODE_AMD_SWIZZLE;
            op.operands.resize(2);
            op.operands[0] = dstOperand;
            op.operands[1].name = "src";
            op.operands[1] = srcParam[0];
            break;
          }
          case AMDInstruction::DX12Op::Ballot:
          {
            if(phase == 0)
            {
              // srcParams already stored, store the dst for phase 0
              dstParam[0] = dstOperand;
            }
            else if(phase == 1)
            {
              op.operation = OPCODE_AMD_BALLOT;
              op.operands.resize(3);
              op.operands[0] = dstParam[0];
              op.operands[1] = dstOperand;
              op.operands[2] = srcParam[0];
              op.operands[2].name = "predicate";
            }
            break;
          }
          case AMDInstruction::DX12Op::MBCnt:
          {
            op.operation = OPCODE_AMD_MBCNT;
            op.operands.resize(3);
            op.operands[0] = dstOperand;
            op.operands[1] = srcParam[0];
            op.operands[2] = srcParam[1];
            break;
          }
          case AMDInstruction::DX12Op::Min3U:
          case AMDInstruction::DX12Op::Min3F:
          case AMDInstruction::DX12Op::Med3U:
          case AMDInstruction::DX12Op::Med3F:
          case AMDInstruction::DX12Op::Max3U:
          case AMDInstruction::DX12Op::Max3F:
          {
            if(phase == 0)
            {
              // don't need the output at all, it's just used to chain the instructions
            }
            else if(phase == 1)
            {
              switch(amdop)
              {
                case AMDInstruction::DX12Op::Min3U: op.operation = OPCODE_AMD_MIN3U; break;
                case AMDInstruction::DX12Op::Min3F: op.operation = OPCODE_AMD_MIN3F; break;
                case AMDInstruction::DX12Op::Med3U: op.operation = OPCODE_AMD_MED3U; break;
                case AMDInstruction::DX12Op::Med3F: op.operation = OPCODE_AMD_MED3F; break;
                case AMDInstruction::DX12Op::Max3U: op.operation = OPCODE_AMD_MAX3U; break;
                case AMDInstruction::DX12Op::Max3F: op.operation = OPCODE_AMD_MAX3F; break;
                default: break;
              }
              op.operands.resize(4);
              op.operands[0] = dstOperand;
              op.operands[1] = srcParam[0];
              op.operands[2] = srcParam[1];
              op.operands[3] = srcParam[2];
            }
            break;
          }
          case AMDInstruction::DX12Op::BaryCoord:
          {
            if(phase == 0)
            {
              // srcParams already stored, store the dst for phase 0
              dstParam[0] = dstOperand;
            }
            else if(phase == 1)
            {
              if(AMDInstruction::BaryInterp.Get(instruction) != AMDInstruction::PerspPullModel)
              {
                // all modes except pull model have two outputs
                op.operation = OPCODE_AMD_BARYCOORD;
                op.operands.resize(2);
                op.operands[0].name = "i";
                op.operands[0] = dstParam[0];
                op.operands[0].name = "j";
                op.operands[1] = dstOperand;
              }
              else
              {
                dstParam[1] = dstOperand;
              }
            }
            else if(phase == 2)
            {
              // all modes except pull model have two outputs
              op.operation = OPCODE_AMD_BARYCOORD;
              op.operands.resize(3);
              op.operands[0].name = "invW";
              op.operands[0] = dstParam[0];
              op.operands[1].name = "invI";
              op.operands[1] = dstParam[1];
              op.operands[2].name = "invJ";
              op.operands[2] = dstOperand;
            }
            break;
          }
          case AMDInstruction::DX12Op::VtxParam:
          {
            op.operation = OPCODE_AMD_VTXPARAM;
            op.operands.resize(3);
            op.operands[0] = dstOperand;
            // vertexIndex is encoded in instruction data
            op.operands[1].name = "vertexIndex";
            op.operands[1].type = TYPE_IMMEDIATE32;
            op.operands[1].numComponents = NUMCOMPS_1;
            op.operands[1].values[0] = AMDInstruction::VtxParamVertex.Get(instruction);

            // decode and pretty-ify the parameter index and component
            op.operands[2].name = "parameter";
            op.operands[2].type = TYPE_INPUT;
            op.operands[2].numComponents = NUMCOMPS_1;
            op.operands[2].indices.resize(1);
            op.operands[2].indices[0].absolute = true;
            op.operands[2].indices[0].index = AMDInstruction::VtxParamParameter.Get(instruction);
            op.operands[2].setComps(AMDInstruction::VtxParamComponent.Get(instruction), 0xff, 0xff,
                                    0xff);

            break;
          }
          case AMDInstruction::DX12Op::ViewportIndex:
          {
            op.operation = OPCODE_AMD_GET_VIEWPORTINDEX;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::RtArraySlice:
          {
            op.operation = OPCODE_AMD_GET_RTARRAYSLICE;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::WaveReduce:
          case AMDInstruction::DX12Op::WaveScan:
          {
            if(amdop == AMDInstruction::DX12Op::WaveReduce)
              op.operation = OPCODE_AMD_WAVE_REDUCE;
            else
              op.operation = OPCODE_AMD_WAVE_SCAN;

            op.preciseValues = AMDInstruction::WaveOp.Get(instruction);
            break;
          }
          case AMDInstruction::DX12Op::LoadDwAtAddr:
          {
            if(phase == 0)
            {
              // don't need the output at all, it's just used to chain the instructions
            }
            else if(phase == 1)
            {
              op.operation = OPCODE_AMD_LOADDWATADDR;
              op.operands.resize(4);
              op.operands[0] = dstOperand;
              op.operands[1] = srcParam[0];
              op.operands[1].name = "gpuVaLoBits";
              op.operands[2] = srcParam[1];
              op.operands[2].name = "gpuVaHiBits";
              op.operands[3] = srcParam[2];
              op.operands[3].name = "offset";
            }
            break;
          }
          case AMDInstruction::DX12Op::DrawIndex:
          {
            op.operation = OPCODE_AMD_GET_DRAWINDEX;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::GetWaveSize:
          {
            op.operation = OPCODE_AMD_GET_WAVESIZE;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::BaseInstance:
          {
            op.operation = OPCODE_AMD_GET_BASEINSTANCE;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::BaseVertex:
          {
            op.operation = OPCODE_AMD_GET_BASEVERTEX;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::AtomicU64:
          {
            // if we're in the nothing state, move to the AMD UAV state so we watch for a UAV access
            // and nop it out
            if(state == InstructionState::Nothing)
              state = InstructionState::AMDUAVAtomic;

            VendorAtomicOp atomicop = convert(AMDInstruction::AtomicOp.Get(instruction));
            op.preciseValues = atomicop;

            bool isCAS = (atomicop == ATOMIC_OP_CAS);

            // for CAS we have four phases, only exit the state when we're in phase 3. For all other
            // instructions we have three phases so exit in phase 2.
            if(phase == 3 || (phase == 2 && !isCAS))
            {
              op.operation = OPCODE_AMD_U64_ATOMIC;
              state = InstructionState::Nothing;

              // output values first
              op.operands.push_back(dstParam[0]);
              op.operands.push_back(dstOperand);

              // then the saved UAV
              op.operands.push_back(uavParam);

              // then the address. This is in params [0], [1], [2]. If they all come from the same
              // register we can compact this
              if(srcParam[0].indices == srcParam[1].indices &&
                 srcParam[1].indices == srcParam[2].indices)
              {
                op.operands.push_back(srcParam[0]);
                op.operands.back().setComps(srcParam[0].comps[0], srcParam[1].comps[0],
                                            srcParam[2].comps[0], 0xff);
                op.operands.back().name = "address";

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[0] = 1;
              }
              else
              {
                op.operands.push_back(srcParam[0]);
                op.operands.back().name = "address.x";
                op.operands.back().setComps(srcParam[0].comps[0], 0xff, 0xff, 0xff);
                op.operands.push_back(srcParam[1]);
                op.operands.back().name = "address.y";
                op.operands.back().setComps(srcParam[1].comps[0], 0xff, 0xff, 0xff);
                op.operands.push_back(srcParam[2]);
                op.operands.back().name = "address.z";
                op.operands.back().setComps(srcParam[2].comps[0], 0xff, 0xff, 0xff);

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[0] = 2;
              }

              // for CAS, the compare value next
              if(isCAS)
              {
                if(srcParam[5].indices == srcParam[6].indices)
                {
                  op.operands.push_back(srcParam[5]);
                  op.operands.back().setComps(srcParam[5].comps[0], srcParam[6].comps[0], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[6].values[0];
                  op.operands.back().name = "compare_value";

                  // store in texelOffset whether the parameter is combined (1) or split (2)
                  op.texelOffset[1] = 1;
                }
                else
                {
                  op.operands.push_back(srcParam[5].swizzle(0));
                  op.operands.back().name = "compare_value.x";
                  op.operands.back().setComps(srcParam[5].comps[0], 0xff, 0xff, 0xff);
                  op.operands.push_back(srcParam[6].swizzle(0));
                  op.operands.back().name = "compare_value.y";
                  op.operands.back().setComps(srcParam[6].comps[0], 0xff, 0xff, 0xff);

                  // store in texelOffset whether the parameter is combined (1) or split (2)
                  op.texelOffset[1] = 2;
                }
              }

              // then the value
              if(srcParam[3].indices == srcParam[4].indices)
              {
                op.operands.push_back(srcParam[3]);
                op.operands.back().setComps(srcParam[3].comps[0], srcParam[4].comps[0], 0xff, 0xff);
                op.operands.back().values[1] = srcParam[4].values[0];
                op.operands.back().name = "value";

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[2] = 1;
              }
              else
              {
                op.operands.push_back(srcParam[3].swizzle(0));
                op.operands.back().name = "value.x";
                op.operands.back().setComps(srcParam[3].comps[0], 0xff, 0xff, 0xff);
                op.operands.push_back(srcParam[4].swizzle(0));
                op.operands.back().name = "value.y";
                op.operands.back().setComps(srcParam[4].comps[0], 0xff, 0xff, 0xff);

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[2] = 2;
              }
            }

            // phase 0's destination is the first destination
            if(phase == 0)
              dstParam[0] = dstOperand;

            break;
          }
        }

        // if the operation wasn't set we're on an intermediate phase. operands were saved,
        // wait until we have the full operation
        if(op.operation != NUM_REAL_OPCODES)
        {
          op.offset = curOp.offset;
          op.str = ToStr(op.operation);

          if(op.operation == OPCODE_AMD_BARYCOORD)
          {
            switch(AMDInstruction::BaryInterp.Get(instruction))
            {
              case AMDInstruction::LinearCenter: op.str += "_linear_center"; break;
              case AMDInstruction::LinearCentroid: op.str += "_linear_centroid"; break;
              case AMDInstruction::LinearSample: op.str += "_linear_sample"; break;
              case AMDInstruction::PerspCenter: op.str += "_persp_center"; break;
              case AMDInstruction::PerspCentroid: op.str += "_persp_centroid"; break;
              case AMDInstruction::PerspSample: op.str += "_persp_sample"; break;
              case AMDInstruction::PerspPullModel: op.str += "_persp_pullmodel"; break;
              default: op.str += "_unknown"; break;
            }
          }
          else if(op.operation == OPCODE_AMD_SWIZZLE)
          {
            switch(AMDInstruction::SwizzleOp.Get(instruction))
            {
              case AMDInstruction::SwapX1: op.str += "_swap1"; break;
              case AMDInstruction::SwapX2: op.str += "_swap2"; break;
              case AMDInstruction::SwapX4: op.str += "_swap4"; break;
              case AMDInstruction::SwapX8: op.str += "_swap8"; break;
              case AMDInstruction::SwapX16: op.str += "_swap16"; break;
              case AMDInstruction::ReverseX4: op.str += "_reverse4"; break;
              case AMDInstruction::ReverseX8: op.str += "_reverse8"; break;
              case AMDInstruction::ReverseX16: op.str += "_reverse16:"; break;
              case AMDInstruction::ReverseX32: op.str += "_reverse32:"; break;
              case AMDInstruction::BCastX2: op.str += "_bcast2"; break;
              case AMDInstruction::BCastX4: op.str += "_bcast4"; break;
              case AMDInstruction::BCastX8: op.str += "_bcast8"; break;
              case AMDInstruction::BCastX16: op.str += "_bcast16"; break;
              case AMDInstruction::BCastX32: op.str += "_bcast32"; break;
            }
          }
          else if(op.operation == OPCODE_AMD_WAVE_REDUCE || op.operation == OPCODE_AMD_WAVE_SCAN)
          {
            switch((VendorWaveOp)op.preciseValues)
            {
              default: break;
              case WAVE_OP_ADD_FLOAT: op.str += "_addf"; break;
              case WAVE_OP_ADD_SINT: op.str += "_addi"; break;
              case WAVE_OP_ADD_UINT: op.str += "_addu"; break;
              case WAVE_OP_MUL_FLOAT: op.str += "_mulf"; break;
              case WAVE_OP_MUL_SINT: op.str += "_muli"; break;
              case WAVE_OP_MUL_UINT: op.str += "_mulu"; break;
              case WAVE_OP_MIN_FLOAT: op.str += "_minf"; break;
              case WAVE_OP_MIN_SINT: op.str += "_mini"; break;
              case WAVE_OP_MIN_UINT: op.str += "_minu"; break;
              case WAVE_OP_MAX_FLOAT: op.str += "_maxf"; break;
              case WAVE_OP_MAX_SINT: op.str += "_maxi"; break;
              case WAVE_OP_MAX_UINT: op.str += "_maxu"; break;
              case WAVE_OP_AND: op.str += "_and"; break;
              case WAVE_OP_OR: op.str += "_or"; break;
              case WAVE_OP_XOR: op.str += "_xor"; break;
            }

            if(op.operation == OPCODE_AMD_WAVE_SCAN)
            {
              if(AMDInstruction::WaveOpFlags.Get(instruction) & 0x1)
                op.str += "_incl";
              if(AMDInstruction::WaveOpFlags.Get(instruction) & 0x2)
                op.str += "_excl";
            }
          }

          for(size_t a = 0; a < op.operands.size(); a++)
          {
            if(a == 0)
              op.str += " ";
            else
              op.str += ", ";
            op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
          }

          m_Instructions.insert(i + 1, op);
        }
      }
      else
      {
        RDCERR("Expected magic value of 5 in encoded AMD instruction %x", instruction);
        state = InstructionState::Broken;
        break;
      }

      if(state == InstructionState::Broken)
        continue;

      // remove this operation, but keep the old operation so we can undo this if things go
      // wrong
      curOp.syncFlags = curOp.operation;
      curOp.operation = OPCODE_VENDOR_REMOVED;
    }
    else if(curOp.operation == OPCODE_IMM_ATOMIC_ALLOC &&
            curOp.operands[1].indices[0].index == magicID)
    {
      // NV IncrementCounter()
      switch(state)
      {
        case InstructionState::Broken:
        case InstructionState::AMDUAVAtomic:
          break;
        // in Nothing an increment marks the beginning of an instruction of some type
        case InstructionState::Nothing:
        {
          state = InstructionState::InstructionHeader;
          break;
        }
        case InstructionState::InstructionHeader:
        {
          // the transition from instruction to any other state should happen via a markUAV or
          // opcode write, not with a counter increment
          RDCERR(
              "Expected either markUAV or opcode write before counter increment in unknown "
              "instruction header!");
          state = InstructionState::Broken;
          break;
        }
        case InstructionState::InstructionBody:
        {
          outputsNeeded--;
          if(outputsNeeded <= 0)
          {
            // once we've emitted all outputs, move to Nothing state
            state = InstructionState::Nothing;

            // and emit vendor instruction
            Operation op;

            switch(nvopcode)
            {
              case NvShaderOpcode::Shuffle:
              case NvShaderOpcode::ShuffleUp:
              case NvShaderOpcode::ShuffleDown:
              case NvShaderOpcode::ShuffleXor:
              {
                if(nvopcode == NvShaderOpcode::Shuffle)
                  op.operation = OPCODE_NV_SHUFFLE;
                else if(nvopcode == NvShaderOpcode::ShuffleUp)
                  op.operation = OPCODE_NV_SHUFFLE_UP;
                else if(nvopcode == NvShaderOpcode::ShuffleDown)
                  op.operation = OPCODE_NV_SHUFFLE_DOWN;
                else if(nvopcode == NvShaderOpcode::ShuffleXor)
                  op.operation = OPCODE_NV_SHUFFLE_XOR;

                op.operands.resize(4);
                op.operands[0] = curOp.operands[0];

                op.operands[1].name = "value";
                op.operands[1] = srcParam[0].swizzle(0);
                if(nvopcode == NvShaderOpcode::Shuffle)
                  op.operands[2].name = "srcLane";
                else if(nvopcode == NvShaderOpcode::ShuffleXor)
                  op.operands[2].name = "laneMask";
                else
                  op.operands[2].name = "delta";
                op.operands[2] = srcParam[0].swizzle(1);
                op.operands[3].name = "width";
                op.operands[3] = srcParam[0].swizzle(3);
                break;
              }
              case NvShaderOpcode::VoteAll:
              case NvShaderOpcode::VoteAny:
              case NvShaderOpcode::VoteBallot:
              {
                if(nvopcode == NvShaderOpcode::VoteAll)
                  op.operation = OPCODE_NV_VOTE_ALL;
                else if(nvopcode == NvShaderOpcode::VoteAny)
                  op.operation = OPCODE_NV_VOTE_ANY;
                else if(nvopcode == NvShaderOpcode::VoteBallot)
                  op.operation = OPCODE_NV_VOTE_BALLOT;

                op.operands.resize(2);
                op.operands[0] = curOp.operands[0];
                op.operands[1] = srcParam[0];
                op.operands[1].name = "predicate";
                break;
              }
              case NvShaderOpcode::GetLaneId:
              {
                op.operation = OPCODE_NV_GET_LANEID;
                op.operands = {curOp.operands[0]};
                break;
              }
              case NvShaderOpcode::GetSpecial:
              {
                if(srcParam[0].type != TYPE_IMMEDIATE32)
                {
                  RDCERR("Expected literal value for special subopcode");
                  state = InstructionState::Broken;
                  break;
                }

                NvShaderSpecial special = (NvShaderSpecial)srcParam[0].values[0];

                if(special == NvShaderSpecial::ThreadLtMask)
                {
                  op.operation = OPCODE_NV_GET_THREADLTMASK;
                }
                else if(special == NvShaderSpecial::FootprintSingleLOD)
                {
                  op.operation = OPCODE_NV_GET_FOOTPRINT_SINGLELOD;
                }
                else
                {
                  RDCERR("Unexpected special subopcode");
                  state = InstructionState::Broken;
                  break;
                }
                op.operands = {curOp.operands[0]};
                break;
              }
              case NvShaderOpcode::MatchAny:
              {
                op.operation = OPCODE_NV_MATCH_ANY;
                op.operands.resize(2);
                op.operands[0] = curOp.operands[0];
                op.operands[1] = srcParam[0];
                // we don't need src1, it only indicates the number of components in the value,
                // which we already have
                break;
              }
              case NvShaderOpcode::GetShadingRate:
              {
                op.operation = OPCODE_NV_GET_SHADING_RATE;

                if(dstParam[0].indices == curOp.operands[0].indices &&
                   dstParam[1].indices == curOp.operands[0].indices)
                {
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result";

                  // fixup the comps according to the shuffle
                  op.operands.back().setComps(
                      // x
                      dstParam[1].comps[0],
                      // y
                      dstParam[0].comps[0],
                      // z
                      curOp.operands[0].comps[0], 0xff);
                }
                else
                {
                  // these are in reverse order because we read them as numOutputs was decrementing
                  op.operands.push_back(dstParam[1]);
                  op.operands.back().name = "result.x";
                  op.operands.push_back(dstParam[0]);
                  op.operands.back().name = "result.y";
                  // z is last
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result.z";
                }

                break;
              }
              // all footprint ops are very similar
              case NvShaderOpcode::Footprint:
              case NvShaderOpcode::FootprintBias:
              case NvShaderOpcode::FootprintLevel:
              case NvShaderOpcode::FootprintGrad:
              {
                if(nvopcode == NvShaderOpcode::Footprint)
                  op.operation = OPCODE_NV_FOOTPRINT;
                else if(nvopcode == NvShaderOpcode::FootprintBias)
                  op.operation = OPCODE_NV_FOOTPRINT_BIAS;
                else if(nvopcode == NvShaderOpcode::FootprintLevel)
                  op.operation = OPCODE_NV_FOOTPRINT_LEVEL;
                else if(nvopcode == NvShaderOpcode::FootprintGrad)
                  op.operation = OPCODE_NV_FOOTPRINT_GRAD;

                // four output values, could be assigned to different registers depending on packing
                // because they come back as scalars from increment counter. In general we have to
                // have them separately, but see if they all neatly line up into one output first.

                if(dstParam[0].indices == curOp.operands[0].indices &&
                   dstParam[1].indices == curOp.operands[0].indices &&
                   dstParam[2].indices == curOp.operands[0].indices)
                {
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result";

                  // fixup the comps according to the shuffle
                  op.operands.back().setComps(
                      // x
                      dstParam[2].comps[0],
                      // y
                      dstParam[1].comps[0],
                      // z
                      dstParam[0].comps[0],
                      // w
                      curOp.operands[0].comps[0]);
                }
                else
                {
                  // these are in reverse order because we read them as numOutputs was decrementing
                  op.operands.push_back(dstParam[2]);
                  op.operands.back().name = "result.x";
                  op.operands.push_back(dstParam[1]);
                  op.operands.back().name = "result.y";
                  op.operands.push_back(dstParam[0]);
                  op.operands.back().name = "result.z";
                  // w is last
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result.w";
                }

                // peel out the source parameters
                op.operands.push_back(srcParam[3].swizzle(0));
                op.operands.back().name = "texSpace";
                op.operands.push_back(srcParam[0].swizzle(0));
                op.operands.back().name = "texIndex";
                op.operands.push_back(srcParam[3].swizzle(1));
                op.operands.back().name = "smpSpace";
                op.operands.push_back(srcParam[0].swizzle(1));
                op.operands.back().name = "smpIndex";
                op.operands.push_back(srcParam[3].swizzle(2));
                op.operands.back().name = "texType";
                op.operands.push_back(srcParam[1]);
                op.operands.back().comps[3] = 0xff;    // location is a float3
                op.operands.back().values[3] = 0;
                op.operands.back().name = "location";
                op.operands.push_back(srcParam[3].swizzle(3));
                op.operands.back().name = "coarse";
                op.operands.push_back(srcParam[1].swizzle(3));
                op.operands.back().name = "gran";

                if(nvopcode == NvShaderOpcode::FootprintBias)
                {
                  op.operands.push_back(srcParam[2].swizzle(0));
                  op.operands.back().name = "bias";
                }
                else if(nvopcode == NvShaderOpcode::FootprintLevel)
                {
                  op.operands.push_back(srcParam[2].swizzle(0));
                  op.operands.back().name = "lodLevel";
                }
                else if(nvopcode == NvShaderOpcode::FootprintGrad)
                {
                  op.operands.push_back(srcParam[2]);
                  op.operands.back().name = "ddx";
                  op.operands.push_back(srcParam[5]);
                  op.operands.back().name = "ddy";
                }

                op.operands.push_back(srcParam[4]);
                op.operands.back().name = "offset";

                break;
              }
              case NvShaderOpcode::ShuffleGeneric:
              {
                op.operation = OPCODE_NV_SHUFFLE_GENERIC;
                op.operands.resize(5);
                // first output is the actual result
                op.operands[0] = curOp.operands[0];
                // second output is the laneValid we stored previously
                op.operands[1] = dstParam[0];
                op.operands[1].name = "out laneValid";

                // we expect the params are packed into srcParam[0]

                op.operands[2] = srcParam[0].swizzle(0);
                op.operands[2].name = "value";
                op.operands[3] = srcParam[0].swizzle(1);
                op.operands[3].name = "srcLane";
                op.operands[4] = srcParam[0].swizzle(2);
                op.operands[4].name = "width";
                break;
              }
              case NvShaderOpcode::VPRSEvalAttribAtSample:
              case NvShaderOpcode::VPRSEvalAttribSnapped:
              {
                if(nvopcode == NvShaderOpcode::VPRSEvalAttribAtSample)
                  op.operation = OPCODE_NV_VPRS_EVAL_ATTRIB_SAMPLE;
                else if(nvopcode == NvShaderOpcode::VPRSEvalAttribSnapped)
                  op.operation = OPCODE_NV_VPRS_EVAL_ATTRIB_SNAPPED;

                // up to four output values, could be assigned to different registers depending on
                // packing because they come back as scalars from increment counter. In general we
                // have to have them separately, but see if they all neatly line up into one output
                // first.

                bool allSameReg = true;

                for(int o = 0; o < numOutputs - 1; o++)
                {
                  if(!(dstParam[o].indices == curOp.operands[0].indices))
                  {
                    allSameReg = false;
                    break;
                  }
                }

                if(allSameReg)
                {
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result";

                  for(int o = 0; o < 4; o++)
                  {
                    if(o >= numOutputs)
                      op.operands.back().comps[o] = 0xff;
                    else if(o + 1 == numOutputs)
                      op.operands.back().comps[o] = curOp.operands[0].comps[0];
                    else
                      op.operands.back().comps[o] = dstParam[numOutputs - 2 - o].comps[0];
                  }
                }
                else
                {
                  const char swz[] = "xyzw";
                  for(int o = 0; o < numOutputs - 1; o++)
                  {
                    // these are in reverse order because we read them as numOutputs was
                    // decrementing
                    op.operands.push_back(dstParam[numOutputs - 2 - o]);
                    op.operands.back().name = "result.";
                    op.operands.back().name += swz[o];
                  }
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result.";
                  op.operands.back().name += swz[numOutputs - 1];
                }

                op.operands.push_back(srcParam[0]);
                op.operands.back().name = "attrib";

                if(nvopcode == NvShaderOpcode::VPRSEvalAttribAtSample)
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().name = "sampleIndex";
                  op.operands.push_back(srcParam[2]);
                  op.operands.back().name = "pixelOffset";
                }
                else if(nvopcode == NvShaderOpcode::VPRSEvalAttribSnapped)
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().name = "offset";
                }

                break;
              }
              default:
                RDCERR("Unexpected non-UAV opcode %d.", nvopcode);
                state = InstructionState::Broken;
                break;
            }

            if(state == InstructionState::Broken)
              break;

            op.offset = curOp.offset;
            op.str = ToStr(op.operation);

            for(size_t a = 0; a < op.operands.size(); a++)
            {
              if(a == 0)
                op.str += " ";
              else
                op.str += ", ";
              op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
            }

            m_Instructions.insert(i + 1, op);
          }
          else
          {
            dstParam[outputsNeeded - 1] = curOp.operands[0];
          }
          break;
        }
        case InstructionState::UAVInstructionHeader1:
        {
          RDCERR("Expected other UAV write before counter increment in UAV instruction header!");
          state = InstructionState::Broken;
          break;
        }
        case InstructionState::UAVInstructionHeader2:
        {
          // now that we've gotten the UAV, we can go to the body
          state = InstructionState::UAVInstructionBody;
          break;
        }
        case InstructionState::UAVInstructionBody:
        {
          RDCERR(
              "Unexpected counter increment while processing UAV instruction body. Expected "
              "opcode!");
          state = InstructionState::Broken;
          break;
        }
      }

      if(state == InstructionState::Broken)
        continue;

      // remove this operation, but keep the old operation so we can undo this if things go
      // wrong
      curOp.syncFlags = curOp.operation;
      curOp.operation = OPCODE_VENDOR_REMOVED;
    }
    else if(curOp.operation == OPCODE_STORE_STRUCTURED &&
            curOp.operands[0].indices[0].index == magicID)
    {
      if(curOp.operands[2].type != TYPE_IMMEDIATE32)
      {
        RDCERR("Expected literal value for UAV write offset");
        state = InstructionState::Broken;
        break;
      }

      // NV magic UAV write
      NvUAVParam param = (NvUAVParam)curOp.operands[2].values[0];

      switch(param)
      {
        case NvUAVParam::opcode:
        {
          if(curOp.operands[3].type != TYPE_IMMEDIATE32)
          {
            RDCERR(
                "Expected literal value being written as opcode. Was the shader compiled with "
                "optimisations disabled?");
            state = InstructionState::Broken;
            break;
          }

          nvopcode = (NvShaderOpcode)curOp.operands[3].values[0];

          // if this is NV_EXTN_OP_FP16_ATOMIC we should have come here in UAVInstructionBody.
          // That we're here now means this is the continuation of an earlier instruction.
          if(state == InstructionState::InstructionHeader && nvopcode == NvShaderOpcode::FP16Atomic)
            state = InstructionState::UAVInstructionBody;

          // if we're in instruction, this is the simple case so move to the output
          if(state == InstructionState::InstructionHeader)
          {
            // if we haven't gotten a number of outputs at all, set it to 1
            if(outputsNeeded <= 0)
              numOutputs = outputsNeeded = 1;
            state = InstructionState::InstructionBody;
          }
          else if(state == InstructionState::UAVInstructionBody)
          {
            // emit the instruction now, writing to the index register (which we know is
            // 'unused'). There might be nothing to read the result value. We'll look out for
            // loads and post-patch it.
            // once we've emitted all outputs, move to Nothing state
            state = InstructionState::Nothing;

            // and emit vendor instruction
            Operation op;
            // write to the index register at first. If there's a subsequent read of dst we'll patch
            // this instruction with the destination for that.
            op.operands.push_back(curOp.operands[1]);
            // also include the UAV we noted elsewhere
            op.operands.push_back(uavParam);

            NvShaderAtomic atomicop = NvShaderAtomic::Unknown;

            switch(nvopcode)
            {
              case NvShaderOpcode::FP16Atomic:
              {
                op.operation = OPCODE_NV_FP16_ATOMIC;

                if(srcParam[2].type != TYPE_IMMEDIATE32)
                {
                  RDCERR(
                      "Expected literal value as atomic opcode. Was the shader compiled with "
                      "optimisations disabled?");
                  state = InstructionState::Broken;
                  break;
                }

                atomicop = (NvShaderAtomic)srcParam[2].values[0];

                op.operands.push_back(srcParam[0]);
                op.operands.back().name = "address";
                op.operands.push_back(srcParam[1]);
                op.operands.back().name = "value";

                break;
              }
              case NvShaderOpcode::FP32Atomic:
              {
                op.operation = OPCODE_NV_FP32_ATOMIC;

                if(srcParam[2].type != TYPE_IMMEDIATE32)
                {
                  RDCERR(
                      "Expected literal value as atomic opcode. Was the shader compiled with "
                      "optimisations disabled?");
                  state = InstructionState::Broken;
                  break;
                }

                atomicop = (NvShaderAtomic)srcParam[2].values[0];

                op.operands.push_back(srcParam[0].swizzle(0));
                op.operands.back().name = "byteAddress";
                op.operands.push_back(srcParam[1].swizzle(0));
                op.operands.back().name = "value";

                break;
              }
              case NvShaderOpcode::U64Atomic:
              {
                op.operation = OPCODE_NV_U64_ATOMIC;

                if(srcParam[2].type != TYPE_IMMEDIATE32)
                {
                  RDCERR(
                      "Expected literal value as atomic opcode. Was the shader compiled with "
                      "optimisations disabled?");
                  state = InstructionState::Broken;
                  break;
                }

                // insert second dummy return value for high bits
                op.operands.insert(0, curOp.operands[1]);

                // make both of them NULL
                op.operands[0].type = TYPE_NULL;
                op.operands[0].setComps(0xff, 0xff, 0xff, 0xff);
                op.operands[1].type = TYPE_NULL;
                op.operands[1].setComps(0xff, 0xff, 0xff, 0xff);

                atomicop = (NvShaderAtomic)srcParam[2].values[0];

                op.operands.push_back(srcParam[0]);
                op.operands.back().numComponents = NUMCOMPS_1;
                op.operands.back().name = "address";

                // store in texelOffset whether the parameter is combined (1) or split (2).
                // on nv we assume the parameters are always combined
                op.texelOffset[0] = 1;
                op.texelOffset[1] = 1;
                op.texelOffset[2] = 1;

                if(atomicop == NvShaderAtomic::CompareAndSwap)
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().numComponents = NUMCOMPS_4;
                  op.operands.back().setComps(srcParam[1].comps[0], srcParam[1].comps[1], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[1].values[1];
                  op.operands.back().name = "compareValue";
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().numComponents = NUMCOMPS_4;
                  op.operands.back().setComps(srcParam[1].comps[2], srcParam[1].comps[3], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[1].values[3];
                  op.operands.back().name = "value";
                }
                else
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().numComponents = NUMCOMPS_4;
                  op.operands.back().setComps(srcParam[1].comps[0], srcParam[1].comps[1], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[1].values[1];
                  op.operands.back().name = "value";
                }

                break;
              }
              default:
                RDCERR("Unexpected UAV opcode %d.", nvopcode);
                state = InstructionState::Broken;
                break;
            }

            if(state == InstructionState::Broken)
              break;

            if(atomicop == NvShaderAtomic::Unknown)
            {
              RDCERR("Couldn't determine atomic op");
              state = InstructionState::Broken;
              break;
            }

            op.offset = curOp.offset;
            op.preciseValues = (uint32_t)atomicop;
            op.str = ToStr(op.operation);

            switch(atomicop)
            {
              case NvShaderAtomic::Unknown: break;
              case NvShaderAtomic::And:
                op.str += "_and";
                op.preciseValues = ATOMIC_OP_AND;
                break;
              case NvShaderAtomic::Or:
                op.str += "_or";
                op.preciseValues = ATOMIC_OP_OR;
                break;
              case NvShaderAtomic::Xor:
                op.str += "_xor";
                op.preciseValues = ATOMIC_OP_XOR;
                break;
              case NvShaderAtomic::Add:
                op.str += "_add";
                op.preciseValues = ATOMIC_OP_ADD;
                break;
              case NvShaderAtomic::Max:
                op.str += "_max";
                op.preciseValues = ATOMIC_OP_MAX;
                break;
              case NvShaderAtomic::Min:
                op.str += "_min";
                op.preciseValues = ATOMIC_OP_MIN;
                break;
              case NvShaderAtomic::Swap:
                op.str += "_swap";
                op.preciseValues = ATOMIC_OP_SWAP;
                break;
              case NvShaderAtomic::CompareAndSwap:
                op.str += "_comp_swap";
                op.preciseValues = ATOMIC_OP_CAS;
                break;
            }

            for(size_t a = 0; a < op.operands.size(); a++)
            {
              if(a == 0)
                op.str += " ";
              else
                op.str += ", ";
              op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
            }

            m_Instructions.insert(i + 1, op);

            // move into nothing state
            state = InstructionState::Nothing;
          }
          else
          {
            // no other state should be writing an opcode.
            RDCERR("Writing opcode in unexpected state %d.", state);
            state = InstructionState::Broken;
          }
          break;
        }
        case NvUAVParam::markUAV:
        {
          if(curOp.operands[3].type != TYPE_IMMEDIATE32 || curOp.operands[3].values[0] != 1)
          {
            RDCERR(
                "Expected literal 1 being written to markUAV. Was the shader compiled with "
                "optimisations disabled?");
            state = InstructionState::Broken;
            break;
          }

          if(state == InstructionState::InstructionHeader)
          {
            // start waiting for the user's UAV write
            state = InstructionState::UAVInstructionHeader1;
          }
          else
          {
            // no other state should be writing an opcode.
            RDCERR("Writing markUAV in unexpected state %d.", state);
            state = InstructionState::Broken;
          }
          break;
        }
        // store the src params unconditionally, don't care about the state.
        case NvUAVParam::src0:
        {
          srcParam[0] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src1:
        {
          srcParam[1] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src2:
        {
          srcParam[2] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src3:
        {
          srcParam[3] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src4:
        {
          srcParam[4] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src5:
        {
          srcParam[5] = curOp.operands[3];
          break;
        }
        case NvUAVParam::dst:
        {
          RDCERR("Unexpected store to dst");
          state = InstructionState::Broken;
          break;
        }
        case NvUAVParam::numOutputs:
        {
          if(curOp.operands[3].type != TYPE_IMMEDIATE32)
          {
            RDCERR(
                "Expected literal value being written as numOutputs. Was the shader compiled "
                "with optimisations disabled?");
            state = InstructionState::Broken;
            break;
          }

          if(state == InstructionState::InstructionHeader ||
             state == InstructionState::InstructionBody)
          {
            // allow writing number of outputs in either header or body (before or after
            // simple
            // opcode)
            numOutputs = outputsNeeded = (int)curOp.operands[3].values[0];
          }
          else
          {
            // no other state should be writing an opcode.
            RDCERR("Writing numOutputs in unexpected state %d.", state);
            state = InstructionState::Broken;
          }
          break;
        }
        default:
        {
          RDCERR("Unexpected offset %u in nvidia magic UAV write.", param);
          state = InstructionState::Broken;
          break;
        }
      }

      if(state == InstructionState::Broken)
        continue;

      // remove this operation, but keep the old operation so we can undo this if things go
      // wrong
      curOp.syncFlags = curOp.operation;
      curOp.operation = OPCODE_VENDOR_REMOVED;
    }
    else if(curOp.operation == OPCODE_LD_STRUCTURED && curOp.operands[3].indices[0].index == magicID)
    {
      // NV magic UAV load. This should only be of dst and only in the Nothing state after
      // we've
      // emitted a UAV instruction.
      if(state == InstructionState::Nothing)
      {
        if(curOp.operands[2].type == TYPE_IMMEDIATE32)
        {
          // NV magic UAV read
          NvUAVParam param = (NvUAVParam)curOp.operands[2].values[0];

          if(param == NvUAVParam::dst)
          {
            // search backwards for the last vendor operation. That's the one we're reading
            // from
            for(size_t j = i; j > 0; j--)
            {
              if(m_Instructions[j].operation >= OPCODE_VENDOR_FIRST)
              {
                // re-emit the instruction writing to the actual output now
                Operation op = m_Instructions[j];
                op.offset = curOp.offset;
                op.operands[0] = curOp.operands[0];
                op.str = ToStr(op.operation);

                // if this is an atomic64, the low/high bits are separate operands
                if(op.operation == OPCODE_NV_U64_ATOMIC)
                {
                  op.operands[1] = curOp.operands[0];
                  op.operands[0].setComps(curOp.operands[0].comps[0], 0xff, 0xff, 0xff);
                  op.operands[1].setComps(curOp.operands[0].comps[1], 0xff, 0xff, 0xff);
                }

                switch((VendorAtomicOp)op.preciseValues)
                {
                  case ATOMIC_OP_NONE: break;
                  case ATOMIC_OP_AND: op.str += "_and"; break;
                  case ATOMIC_OP_OR: op.str += "_or"; break;
                  case ATOMIC_OP_XOR: op.str += "_xor"; break;
                  case ATOMIC_OP_ADD: op.str += "_add"; break;
                  case ATOMIC_OP_MAX: op.str += "_max"; break;
                  case ATOMIC_OP_MIN: op.str += "_min"; break;
                  case ATOMIC_OP_SWAP: op.str += "_swap"; break;
                  case ATOMIC_OP_CAS: op.str += "_comp_swap"; break;
                }

                for(size_t a = 0; a < op.operands.size(); a++)
                {
                  if(a == 0)
                    op.str += " ";
                  else
                    op.str += ", ";
                  op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
                }

                m_Instructions.insert(i + 1, op);

                // remove the old one, we've replaced it
                m_Instructions[j].operation = OPCODE_VENDOR_REMOVED;
                // if we break and try to revert this one, keep it removed
                m_Instructions[j].syncFlags = OPCODE_VENDOR_REMOVED;
                // also remove the current one! but back up the original in case something
                // goes
                // wrong
                curOp.syncFlags = curOp.operation;
                curOp.operation = OPCODE_VENDOR_REMOVED;
                break;
              }
            }
          }
          else
          {
            RDCERR("Unexpected read of UAV at offset %d instead of dst (%d)", param, NvUAVParam::dst);
            state = InstructionState::Broken;
          }
        }
        else
        {
          RDCERR("Expected literal value for UAV read offset");
          state = InstructionState::Broken;
        }
      }
      else
      {
        RDCERR("Unexpected UAV read in state %d.", state);
        state = InstructionState::Broken;
      }
    }
    else if(state == InstructionState::UAVInstructionHeader1)
    {
      // while we're here the next UAV write is snooped
      if(curOp.operation == OPCODE_STORE_RAW || curOp.operation == OPCODE_STORE_UAV_TYPED)
      {
        uavParam = curOp.operands[0];
        state = InstructionState::UAVInstructionHeader2;

        // remove this operation, but keep the old operation so we can undo this if things go
        // wrong
        curOp.syncFlags = curOp.operation;
        curOp.operation = OPCODE_VENDOR_REMOVED;
      }
    }
    else if(state == InstructionState::AMDUAVAtomic)
    {
      // similarly for AMD we store the UAV referenced, but we don't change state - that happens
      // when we see the appropriate phase instruction.
      if(curOp.operation == OPCODE_STORE_RAW || curOp.operation == OPCODE_STORE_UAV_TYPED)
      {
        uavParam = curOp.operands[0];
        state = InstructionState::UAVInstructionHeader2;

        // remove this operation, but keep the old operation so we can undo this if things go
        // wrong
        curOp.syncFlags = curOp.operation;
        curOp.operation = OPCODE_VENDOR_REMOVED;
      }
    }

    // any other operation we completely ignore
  }

  if(state == InstructionState::Broken)
  {
    // if we broke, restore the operations and remove any added vendor operations
    for(size_t i = 0; i < m_Instructions.size(); i++)
    {
      if(m_Instructions[i].operation == OPCODE_VENDOR_REMOVED)
        m_Instructions[i].operation = (OpcodeType)m_Instructions[i].syncFlags;
      else if(m_Instructions[i].operation >= OPCODE_VENDOR_FIRST)
        m_Instructions[i].operation = OPCODE_VENDOR_REMOVED;
    }
  }

  // erase any OPCODE_VENDOR_REMOVED instructions now
  for(int32_t i = m_Instructions.count() - 1; i >= 0; i--)
  {
    if(m_Instructions[i].operation == OPCODE_VENDOR_REMOVED)
      m_Instructions.erase(i);
  }
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

  if(!name.empty())
    str = name + "=" + str;

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

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_READFIRSTLANE, "amd_readfirstlane")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_READLANE, "amd_readlane")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_LANEID, "amd_laneid")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_SWIZZLE, "amd_swizzle")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_BALLOT, "amd_ballot")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MBCNT, "amd_mbcnt")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MIN3U, "amd_min3u")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MIN3F, "amd_min3f")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MED3U, "amd_med3u")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MED3F, "amd_med3f")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MAX3U, "amd_max3u")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_MAX3F, "amd_max3f")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_BARYCOORD, "amd_barycoord")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_VTXPARAM, "amd_vtxparam")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_GET_VIEWPORTINDEX, "amd_get_viewportindex")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_GET_RTARRAYSLICE, "amd_get_rtarrayslice")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_WAVE_REDUCE, "amd_wave_reduce")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_WAVE_SCAN, "amd_wave_scan")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_LOADDWATADDR, "amd_load_dw_at_addr")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_GET_DRAWINDEX, "amd_get_drawindex")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_U64_ATOMIC, "amd_u64_atomic")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_GET_WAVESIZE, "amd_get_wavesize")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_GET_BASEINSTANCE, "amd_get_baseinstance")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_AMD_GET_BASEVERTEX, "amd_get_basevertex")

    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_SHUFFLE, "nv_shuffle")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_SHUFFLE_UP, "nv_shuffle_up")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_SHUFFLE_DOWN, "nv_shuffle_down")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_SHUFFLE_XOR, "nv_shuffle_xor")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_VOTE_ALL, "nv_vote_all")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_VOTE_ANY, "nv_vote_any")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_VOTE_BALLOT, "nv_vote_ballot")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_GET_LANEID, "nv_get_laneid")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_FP16_ATOMIC, "nv_fp16_atomic")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_FP32_ATOMIC, "nv_fp32_atomic")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_GET_THREADLTMASK, "nv_get_threadltmask")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_GET_FOOTPRINT_SINGLELOD, "nv_get_footprint_singlelod")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_U64_ATOMIC, "nv_u64_atomic")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_MATCH_ANY, "nv_match_any")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_FOOTPRINT, "nv_footprint")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_FOOTPRINT_BIAS, "nv_footprint_bias")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_GET_SHADING_RATE, "nv_get_shading_rate")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_FOOTPRINT_LEVEL, "nv_footprint_level")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_FOOTPRINT_GRAD, "nv_footprint_grad")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_SHUFFLE_GENERIC, "nv_shuffle_generic")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_VPRS_EVAL_ATTRIB_SAMPLE, "nv_vprs_eval_attrib_sample")
    STRINGISE_ENUM_CLASS_NAMED(OPCODE_NV_VPRS_EVAL_ATTRIB_SNAPPED, "nv_vprs_eval_attrib_snapped")
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
