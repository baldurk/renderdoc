/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "dxbc_bytecode.h"

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

  static void Set(uint32_t &token, const T &val)
  {
    unsigned long shift = 0;
    unsigned long mask = M;
    byte hasBit = _BitScanForward(&shift, mask);
    RDCASSERT(hasBit != 0);

    token &= ~M;
    token |= (((uint32_t)val) << shift) & M;
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

  static void Set(uint32_t &token, bool val)
  {
    token &= ~M;
    if(val)
      token |= M;
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
static MaskedElement<uint8_t, 0x00780000> PreciseValues;

// several
static MaskedElement<bool, 0x00002000> Saturate;
static MaskedElement<bool, 0x00040000> TestNonZero;

// OPCODE_RESINFO
static MaskedElement<ResinfoRetType, 0x00001800> ResinfoReturn;

// OPCODE_SYNC
static MaskedElement<uint8_t, 0x00007800> SyncFlags;
// relative to above uint32! ie. post shift.
static MaskedElement<bool, 0x00000001> Sync_Threads;
static MaskedElement<bool, 0x00000002> Sync_TGSM;
static MaskedElement<bool, 0x00000004> Sync_UAV_Group;
static MaskedElement<bool, 0x00000008> Sync_UAV_Global;
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
// OPCODE_DCL_INPUT_PS_SIV
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

// OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED
// OPCODE_DCL_RESOURCE_STRUCTURED
static MaskedElement<bool, 0x00800000> HasOrderPreservingCounter;

// OPCODE_DCL_INTERFACE
static MaskedElement<uint32_t, 0x0000FFFF> TableLength;
static MaskedElement<uint32_t, 0xFFFF0000> NumInterfaces;
};    // Declaration

namespace ExtendedOpcode
{
static MaskedElement<bool, 0x80000000> Extended;
static MaskedElement<ExtendedOpcodeType, 0x0000003F> Type;

// OPCODE_EX_SAMPLE_CONTROLS
static MaskedElement<int8_t, 0x00001E00> TexelOffsetU;
static MaskedElement<int8_t, 0x0001E000> TexelOffsetV;
static MaskedElement<int8_t, 0x001E0000> TexelOffsetW;

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

};    // namespace DXBCBytecode
