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

#pragma once

#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"
#include "common/common.h"
#include "driver/dx/official/d3dcommon.h"
#include "dxbc_common.h"

namespace DXBC
{
struct CBuffer;
struct ShaderInputBind;
class IDebugInfo;
struct Reflection;
};

namespace DXBCBytecode
{
/////////////////////////////////////////////////////////////////////////
// Enums for use below. If you're reading this you might want to skip to
// the main structures after this section.
/////////////////////////////////////////////////////////////////////////

enum ProgramType
{
  TYPE_PIXEL = 0,
  TYPE_VERTEX,
  TYPE_GEOMETRY,
  TYPE_HULL,
  TYPE_DOMAIN,
  TYPE_COMPUTE,

  NUM_TYPES,
};

enum OpcodeType
{
  OPCODE_ADD = 0,
  OPCODE_AND,
  OPCODE_BREAK,
  OPCODE_BREAKC,
  OPCODE_CALL,
  OPCODE_CALLC,
  OPCODE_CASE,
  OPCODE_CONTINUE,
  OPCODE_CONTINUEC,
  OPCODE_CUT,
  OPCODE_DEFAULT,
  OPCODE_DERIV_RTX,
  OPCODE_DERIV_RTY,
  OPCODE_DISCARD,
  OPCODE_DIV,
  OPCODE_DP2,
  OPCODE_DP3,
  OPCODE_DP4,
  OPCODE_ELSE,
  OPCODE_EMIT,
  OPCODE_EMITTHENCUT,
  OPCODE_ENDIF,
  OPCODE_ENDLOOP,
  OPCODE_ENDSWITCH,
  OPCODE_EQ,
  OPCODE_EXP,
  OPCODE_FRC,
  OPCODE_FTOI,
  OPCODE_FTOU,
  OPCODE_GE,
  OPCODE_IADD,
  OPCODE_IF,
  OPCODE_IEQ,
  OPCODE_IGE,
  OPCODE_ILT,
  OPCODE_IMAD,
  OPCODE_IMAX,
  OPCODE_IMIN,
  OPCODE_IMUL,
  OPCODE_INE,
  OPCODE_INEG,
  OPCODE_ISHL,
  OPCODE_ISHR,
  OPCODE_ITOF,
  OPCODE_LABEL,
  OPCODE_LD,
  OPCODE_LD_MS,
  OPCODE_LOG,
  OPCODE_LOOP,
  OPCODE_LT,
  OPCODE_MAD,
  OPCODE_MIN,
  OPCODE_MAX,
  OPCODE_CUSTOMDATA,
  OPCODE_MOV,
  OPCODE_MOVC,
  OPCODE_MUL,
  OPCODE_NE,
  OPCODE_NOP,
  OPCODE_NOT,
  OPCODE_OR,
  OPCODE_RESINFO,
  OPCODE_RET,
  OPCODE_RETC,
  OPCODE_ROUND_NE,
  OPCODE_ROUND_NI,
  OPCODE_ROUND_PI,
  OPCODE_ROUND_Z,
  OPCODE_RSQ,
  OPCODE_SAMPLE,
  OPCODE_SAMPLE_C,
  OPCODE_SAMPLE_C_LZ,
  OPCODE_SAMPLE_L,
  OPCODE_SAMPLE_D,
  OPCODE_SAMPLE_B,
  OPCODE_SQRT,
  OPCODE_SWITCH,
  OPCODE_SINCOS,
  OPCODE_UDIV,
  OPCODE_ULT,
  OPCODE_UGE,
  OPCODE_UMUL,
  OPCODE_UMAD,
  OPCODE_UMAX,
  OPCODE_UMIN,
  OPCODE_USHR,
  OPCODE_UTOF,
  OPCODE_XOR,
  OPCODE_DCL_RESOURCE,
  OPCODE_DCL_CONSTANT_BUFFER,
  OPCODE_DCL_SAMPLER,
  OPCODE_DCL_INDEX_RANGE,
  OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY,
  OPCODE_DCL_GS_INPUT_PRIMITIVE,
  OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT,
  OPCODE_DCL_INPUT,
  OPCODE_DCL_INPUT_SGV,
  OPCODE_DCL_INPUT_SIV,
  OPCODE_DCL_INPUT_PS,
  OPCODE_DCL_INPUT_PS_SGV,
  OPCODE_DCL_INPUT_PS_SIV,
  OPCODE_DCL_OUTPUT,
  OPCODE_DCL_OUTPUT_SGV,
  OPCODE_DCL_OUTPUT_SIV,
  OPCODE_DCL_TEMPS,
  OPCODE_DCL_INDEXABLE_TEMP,
  OPCODE_DCL_GLOBAL_FLAGS,

  OPCODE_RESERVED0,

  OPCODE_LOD,
  OPCODE_GATHER4,
  OPCODE_SAMPLE_POS,
  OPCODE_SAMPLE_INFO,

  OPCODE_RESERVED1,

  OPCODE_HS_DECLS,
  OPCODE_HS_CONTROL_POINT_PHASE,
  OPCODE_HS_FORK_PHASE,
  OPCODE_HS_JOIN_PHASE,

  OPCODE_EMIT_STREAM,
  OPCODE_CUT_STREAM,
  OPCODE_EMITTHENCUT_STREAM,
  OPCODE_INTERFACE_CALL,

  OPCODE_BUFINFO,
  OPCODE_DERIV_RTX_COARSE,
  OPCODE_DERIV_RTX_FINE,
  OPCODE_DERIV_RTY_COARSE,
  OPCODE_DERIV_RTY_FINE,
  OPCODE_GATHER4_C,
  OPCODE_GATHER4_PO,
  OPCODE_GATHER4_PO_C,
  OPCODE_RCP,
  OPCODE_F32TOF16,
  OPCODE_F16TOF32,
  OPCODE_UADDC,
  OPCODE_USUBB,
  OPCODE_COUNTBITS,
  OPCODE_FIRSTBIT_HI,
  OPCODE_FIRSTBIT_LO,
  OPCODE_FIRSTBIT_SHI,
  OPCODE_UBFE,
  OPCODE_IBFE,
  OPCODE_BFI,
  OPCODE_BFREV,
  OPCODE_SWAPC,

  OPCODE_DCL_STREAM,
  OPCODE_DCL_FUNCTION_BODY,
  OPCODE_DCL_FUNCTION_TABLE,
  OPCODE_DCL_INTERFACE,

  OPCODE_DCL_INPUT_CONTROL_POINT_COUNT,
  OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT,
  OPCODE_DCL_TESS_DOMAIN,
  OPCODE_DCL_TESS_PARTITIONING,
  OPCODE_DCL_TESS_OUTPUT_PRIMITIVE,
  OPCODE_DCL_HS_MAX_TESSFACTOR,
  OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT,
  OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT,

  OPCODE_DCL_THREAD_GROUP,
  OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED,
  OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW,
  OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED,
  OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW,
  OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED,
  OPCODE_DCL_RESOURCE_RAW,
  OPCODE_DCL_RESOURCE_STRUCTURED,
  OPCODE_LD_UAV_TYPED,
  OPCODE_STORE_UAV_TYPED,
  OPCODE_LD_RAW,
  OPCODE_STORE_RAW,
  OPCODE_LD_STRUCTURED,
  OPCODE_STORE_STRUCTURED,
  OPCODE_ATOMIC_AND,
  OPCODE_ATOMIC_OR,
  OPCODE_ATOMIC_XOR,
  OPCODE_ATOMIC_CMP_STORE,
  OPCODE_ATOMIC_IADD,
  OPCODE_ATOMIC_IMAX,
  OPCODE_ATOMIC_IMIN,
  OPCODE_ATOMIC_UMAX,
  OPCODE_ATOMIC_UMIN,
  OPCODE_IMM_ATOMIC_ALLOC,
  OPCODE_IMM_ATOMIC_CONSUME,
  OPCODE_IMM_ATOMIC_IADD,
  OPCODE_IMM_ATOMIC_AND,
  OPCODE_IMM_ATOMIC_OR,
  OPCODE_IMM_ATOMIC_XOR,
  OPCODE_IMM_ATOMIC_EXCH,
  OPCODE_IMM_ATOMIC_CMP_EXCH,
  OPCODE_IMM_ATOMIC_IMAX,
  OPCODE_IMM_ATOMIC_IMIN,
  OPCODE_IMM_ATOMIC_UMAX,
  OPCODE_IMM_ATOMIC_UMIN,
  OPCODE_SYNC,

  OPCODE_DADD,
  OPCODE_DMAX,
  OPCODE_DMIN,
  OPCODE_DMUL,
  OPCODE_DEQ,
  OPCODE_DGE,
  OPCODE_DLT,
  OPCODE_DNE,
  OPCODE_DMOV,
  OPCODE_DMOVC,
  OPCODE_DTOF,
  OPCODE_FTOD,

  OPCODE_EVAL_SNAPPED,
  OPCODE_EVAL_SAMPLE_INDEX,
  OPCODE_EVAL_CENTROID,

  OPCODE_DCL_GS_INSTANCE_COUNT,

  OPCODE_ABORT,
  OPCODE_DEBUGBREAK,

  OPCODE_RESERVED2,

  OPCODE_DDIV,
  OPCODE_DFMA,
  OPCODE_DRCP,

  OPCODE_MSAD,

  OPCODE_DTOI,
  OPCODE_DTOU,
  OPCODE_ITOD,
  OPCODE_UTOD,

  OPCODE_RESERVED3,

  OPCODE_GATHER4_FEEDBACK,
  OPCODE_GATHER4_C_FEEDBACK,
  OPCODE_GATHER4_PO_FEEDBACK,
  OPCODE_GATHER4_PO_C_FEEDBACK,
  OPCODE_LD_FEEDBACK,
  OPCODE_LD_MS_FEEDBACK,
  OPCODE_LD_UAV_TYPED_FEEDBACK,
  OPCODE_LD_RAW_FEEDBACK,
  OPCODE_LD_STRUCTURED_FEEDBACK,
  OPCODE_SAMPLE_L_FEEDBACK,
  OPCODE_SAMPLE_C_LZ_FEEDBACK,

  OPCODE_SAMPLE_CLAMP_FEEDBACK,
  OPCODE_SAMPLE_B_CLAMP_FEEDBACK,
  OPCODE_SAMPLE_D_CLAMP_FEEDBACK,
  OPCODE_SAMPLE_C_CLAMP_FEEDBACK,

  OPCODE_CHECK_ACCESS_FULLY_MAPPED,

  NUM_REAL_OPCODES,

  OPCODE_VENDOR_REMOVED,

  OPCODE_VENDOR_FIRST,

  OPCODE_AMD_READFIRSTLANE,
  OPCODE_AMD_READLANE,
  OPCODE_AMD_LANEID,
  OPCODE_AMD_SWIZZLE,
  OPCODE_AMD_BALLOT,
  OPCODE_AMD_MBCNT,
  OPCODE_AMD_MIN3U,
  OPCODE_AMD_MIN3F,
  OPCODE_AMD_MED3U,
  OPCODE_AMD_MED3F,
  OPCODE_AMD_MAX3U,
  OPCODE_AMD_MAX3F,
  OPCODE_AMD_BARYCOORD,
  OPCODE_AMD_VTXPARAM,
  OPCODE_AMD_GET_VIEWPORTINDEX,
  OPCODE_AMD_GET_RTARRAYSLICE,
  OPCODE_AMD_WAVE_REDUCE,
  OPCODE_AMD_WAVE_SCAN,
  OPCODE_AMD_LOADDWATADDR,
  OPCODE_AMD_GET_DRAWINDEX,
  OPCODE_AMD_U64_ATOMIC,
  OPCODE_AMD_GET_WAVESIZE,
  OPCODE_AMD_GET_BASEINSTANCE,
  OPCODE_AMD_GET_BASEVERTEX,

  OPCODE_NV_SHUFFLE,
  OPCODE_NV_SHUFFLE_UP,
  OPCODE_NV_SHUFFLE_DOWN,
  OPCODE_NV_SHUFFLE_XOR,
  OPCODE_NV_VOTE_ALL,
  OPCODE_NV_VOTE_ANY,
  OPCODE_NV_VOTE_BALLOT,
  OPCODE_NV_GET_LANEID,
  OPCODE_NV_FP16_ATOMIC,
  OPCODE_NV_FP32_ATOMIC,
  OPCODE_NV_GET_THREADLTMASK,
  OPCODE_NV_GET_FOOTPRINT_SINGLELOD,
  OPCODE_NV_U64_ATOMIC,
  OPCODE_NV_MATCH_ANY,
  OPCODE_NV_FOOTPRINT,
  OPCODE_NV_FOOTPRINT_BIAS,
  OPCODE_NV_GET_SHADING_RATE,
  OPCODE_NV_FOOTPRINT_LEVEL,
  OPCODE_NV_FOOTPRINT_GRAD,
  OPCODE_NV_SHUFFLE_GENERIC,
  OPCODE_NV_VPRS_EVAL_ATTRIB_SAMPLE,
  OPCODE_NV_VPRS_EVAL_ATTRIB_SNAPPED,

  OPCODE_DCL_IMMEDIATE_CONSTANT_BUFFER,
  OPCODE_OPAQUE_CUSTOMDATA,
  OPCODE_SHADER_MESSAGE,
};

size_t NumOperands(OpcodeType op);
bool IsDeclaration(OpcodeType op);

enum CustomDataClass
{
  CUSTOMDATA_COMMENT = 0,
  CUSTOMDATA_DEBUGINFO,
  CUSTOMDATA_OPAQUE,
  CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER,
  CUSTOMDATA_SHADER_MESSAGE,
  CUSTOMDATA_SHADER_CLIP_PLANE_CONSTANT_BUFFER_MAPPINGS,

  NUM_CUSTOMDATA_CLASSES,
};

enum ResinfoRetType : uint8_t
{
  RETTYPE_FLOAT = 0,
  RETTYPE_RCPFLOAT,
  RETTYPE_UINT,

  NUM_RETTYPES,
};

enum ExtendedOpcodeType
{
  EXTENDED_OPCODE_EMPTY = 0,
  EXTENDED_OPCODE_SAMPLE_CONTROLS,
  EXTENDED_OPCODE_RESOURCE_DIM,
  EXTENDED_OPCODE_RESOURCE_RETURN_TYPE,

  NUM_EXTENDED_TYPES,
};

enum NumOperandComponents : uint8_t
{
  NUMCOMPS_0 = 0,
  NUMCOMPS_1,
  NUMCOMPS_4,
  NUMCOMPS_N,

  MAX_COMPONENTS,
};

enum SelectionMode
{
  SELECTION_MASK = 0,
  SELECTION_SWIZZLE,
  SELECTION_SELECT_1,
};

enum OperandType : uint8_t
{
  TYPE_TEMP = 0,
  TYPE_INPUT,
  TYPE_OUTPUT,
  TYPE_INDEXABLE_TEMP,
  TYPE_IMMEDIATE32,
  TYPE_IMMEDIATE64,
  TYPE_SAMPLER,
  TYPE_RESOURCE,
  TYPE_CONSTANT_BUFFER,
  TYPE_IMMEDIATE_CONSTANT_BUFFER,
  TYPE_LABEL,
  TYPE_INPUT_PRIMITIVEID,
  TYPE_OUTPUT_DEPTH,
  TYPE_NULL,

  TYPE_RASTERIZER,
  TYPE_OUTPUT_COVERAGE_MASK,

  TYPE_STREAM,
  TYPE_FUNCTION_BODY,
  TYPE_FUNCTION_TABLE,
  TYPE_INTERFACE,
  TYPE_FUNCTION_INPUT,
  TYPE_FUNCTION_OUTPUT,
  TYPE_OUTPUT_CONTROL_POINT_ID,
  TYPE_INPUT_FORK_INSTANCE_ID,
  TYPE_INPUT_JOIN_INSTANCE_ID,
  TYPE_INPUT_CONTROL_POINT,
  TYPE_OUTPUT_CONTROL_POINT,
  TYPE_INPUT_PATCH_CONSTANT,
  TYPE_INPUT_DOMAIN_POINT,
  TYPE_THIS_POINTER,
  TYPE_UNORDERED_ACCESS_VIEW,
  TYPE_THREAD_GROUP_SHARED_MEMORY,
  TYPE_INPUT_THREAD_ID,
  TYPE_INPUT_THREAD_GROUP_ID,
  TYPE_INPUT_THREAD_ID_IN_GROUP,
  TYPE_INPUT_COVERAGE_MASK,
  TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED,
  TYPE_INPUT_GS_INSTANCE_ID,
  TYPE_OUTPUT_DEPTH_GREATER_EQUAL,
  TYPE_OUTPUT_DEPTH_LESS_EQUAL,
  TYPE_CYCLE_COUNTER,
  TYPE_OUTPUT_STENCIL_REF,
  TYPE_INNER_COVERAGE,

  NUM_OPERAND_TYPES,
};

bool IsInput(OperandType oper);
bool IsOutput(OperandType oper);

enum OperandIndexType
{
  INDEX_IMMEDIATE32 = 0,              // 0
  INDEX_IMMEDIATE64,                  // 0
  INDEX_RELATIVE,                     // [r1]
  INDEX_IMMEDIATE32_PLUS_RELATIVE,    // [r1+0]
  INDEX_IMMEDIATE64_PLUS_RELATIVE,    // [r1+0]

  NUM_INDEX_TYPES
};

enum ExtendedOperandType
{
  EXTENDED_OPERAND_EMPTY = 0,
  EXTENDED_OPERAND_MODIFIER,

  NUM_EXTENDED_OPERAND_TYPES,
};

enum OperandModifier : uint8_t
{
  OPERAND_MODIFIER_NONE = 0,
  OPERAND_MODIFIER_NEG,
  OPERAND_MODIFIER_ABS,
  OPERAND_MODIFIER_ABSNEG,

  NUM_MODIFIERS,
};

enum MinimumPrecision : uint8_t
{
  PRECISION_DEFAULT,
  PRECISION_FLOAT16,
  PRECISION_FLOAT10,
  PRECISION_UNUSED,
  PRECISION_SINT16,
  PRECISION_UINT16,
  PRECISION_ANY16,
  PRECISION_ANY10,

  NUM_PRECISIONS,
};

enum SamplerMode
{
  SAMPLER_MODE_DEFAULT = 0,
  SAMPLER_MODE_COMPARISON,
  SAMPLER_MODE_MONO,

  NUM_SAMPLERS,
};

enum CBufferAccessPattern
{
  ACCESS_IMMEDIATE_INDEXED = 0,
  ACCESS_DYNAMIC_INDEXED,

  NUM_PATTERNS,
};

enum TessellatorDomain
{
  DOMAIN_UNDEFINED = 0,
  DOMAIN_ISOLINE,
  DOMAIN_TRI,
  DOMAIN_QUAD,

  NUM_DOMAINS,
};

enum TessellatorPartitioning
{
  PARTITIONING_UNDEFINED = 0,
  PARTITIONING_INTEGER,
  PARTITIONING_POW2,
  PARTITIONING_FRACTIONAL_ODD,
  PARTITIONING_FRACTIONAL_EVEN,

  NUM_PARTITIONINGS,
};

enum TessellatorOutputPrimitive
{
  OUTPUT_PRIMITIVE_UNDEFINED = 0,
  OUTPUT_PRIMITIVE_POINT,
  OUTPUT_PRIMITIVE_LINE,
  OUTPUT_PRIMITIVE_TRIANGLE_CW,
  OUTPUT_PRIMITIVE_TRIANGLE_CCW,

  NUM_OUTPUT_PRIMITIVES,
};

enum InterpolationMode
{
  INTERPOLATION_UNDEFINED = 0,
  INTERPOLATION_CONSTANT,
  INTERPOLATION_LINEAR,
  INTERPOLATION_LINEAR_CENTROID,
  INTERPOLATION_LINEAR_NOPERSPECTIVE,
  INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID,
  INTERPOLATION_LINEAR_SAMPLE,
  INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,

  NUM_INTERPOLATIONS,
};

enum PrimitiveType
{
  PRIMITIVE_UNDEFINED = 0,
  PRIMITIVE_POINT,
  PRIMITIVE_LINE,
  PRIMITIVE_TRIANGLE,
  PRIMITIVE_LINE_ADJ,
  PRIMITIVE_TRIANGLE_ADJ,
  PRIMITIVE_1_CONTROL_POINT_PATCH,
  PRIMITIVE_2_CONTROL_POINT_PATCH,
  PRIMITIVE_3_CONTROL_POINT_PATCH,
  PRIMITIVE_4_CONTROL_POINT_PATCH,
  PRIMITIVE_5_CONTROL_POINT_PATCH,
  PRIMITIVE_6_CONTROL_POINT_PATCH,
  PRIMITIVE_7_CONTROL_POINT_PATCH,
  PRIMITIVE_8_CONTROL_POINT_PATCH,
  PRIMITIVE_9_CONTROL_POINT_PATCH,
  PRIMITIVE_10_CONTROL_POINT_PATCH,
  PRIMITIVE_11_CONTROL_POINT_PATCH,
  PRIMITIVE_12_CONTROL_POINT_PATCH,
  PRIMITIVE_13_CONTROL_POINT_PATCH,
  PRIMITIVE_14_CONTROL_POINT_PATCH,
  PRIMITIVE_15_CONTROL_POINT_PATCH,
  PRIMITIVE_16_CONTROL_POINT_PATCH,
  PRIMITIVE_17_CONTROL_POINT_PATCH,
  PRIMITIVE_18_CONTROL_POINT_PATCH,
  PRIMITIVE_19_CONTROL_POINT_PATCH,
  PRIMITIVE_20_CONTROL_POINT_PATCH,
  PRIMITIVE_21_CONTROL_POINT_PATCH,
  PRIMITIVE_22_CONTROL_POINT_PATCH,
  PRIMITIVE_23_CONTROL_POINT_PATCH,
  PRIMITIVE_24_CONTROL_POINT_PATCH,
  PRIMITIVE_25_CONTROL_POINT_PATCH,
  PRIMITIVE_26_CONTROL_POINT_PATCH,
  PRIMITIVE_27_CONTROL_POINT_PATCH,
  PRIMITIVE_28_CONTROL_POINT_PATCH,
  PRIMITIVE_29_CONTROL_POINT_PATCH,
  PRIMITIVE_30_CONTROL_POINT_PATCH,
  PRIMITIVE_31_CONTROL_POINT_PATCH,
  PRIMITIVE_32_CONTROL_POINT_PATCH,

  NUM_PRIMITIVES,
};

enum SemanticName
{
  SEMANTIC_UNDEFINED = 0,
  SEMANTIC_POSITION,
  SEMANTIC_CLIP_DISTANCE,
  SEMANTIC_CULL_DISTANCE,
  SEMANTIC_RENDER_TARGET_ARRAY_INDEX,
  SEMANTIC_VIEWPORT_ARRAY_INDEX,
  SEMANTIC_VERTEX_ID,
  SEMANTIC_PRIMITIVE_ID,
  SEMANTIC_INSTANCE_ID,
  SEMANTIC_IS_FRONT_FACE,
  SEMANTIC_SAMPLE_INDEX,
  SEMANTIC_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_QUAD_U_INSIDE_TESSFACTOR,
  SEMANTIC_FINAL_QUAD_V_INSIDE_TESSFACTOR,
  SEMANTIC_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR,
  SEMANTIC_FINAL_TRI_INSIDE_TESSFACTOR,
  SEMANTIC_FINAL_LINE_DETAIL_TESSFACTOR,
  SEMANTIC_FINAL_LINE_DENSITY_TESSFACTOR,

  NUM_SEMANTICS,
};

enum ResourceDimension : uint8_t
{
  RESOURCE_DIMENSION_UNKNOWN = 0,
  RESOURCE_DIMENSION_BUFFER,
  RESOURCE_DIMENSION_TEXTURE1D,
  RESOURCE_DIMENSION_TEXTURE2D,
  RESOURCE_DIMENSION_TEXTURE2DMS,
  RESOURCE_DIMENSION_TEXTURE3D,
  RESOURCE_DIMENSION_TEXTURECUBE,
  RESOURCE_DIMENSION_TEXTURE1DARRAY,
  RESOURCE_DIMENSION_TEXTURE2DARRAY,
  RESOURCE_DIMENSION_TEXTURE2DMSARRAY,
  RESOURCE_DIMENSION_TEXTURECUBEARRAY,
  RESOURCE_DIMENSION_RAW_BUFFER,
  RESOURCE_DIMENSION_STRUCTURED_BUFFER,

  NUM_DIMENSIONS,
};

enum VendorAtomicOp : uint8_t
{
  ATOMIC_OP_NONE = 0,
  ATOMIC_OP_AND,
  ATOMIC_OP_OR,
  ATOMIC_OP_XOR,
  ATOMIC_OP_ADD,
  ATOMIC_OP_MAX,
  ATOMIC_OP_MIN,
  ATOMIC_OP_SWAP,
  ATOMIC_OP_CAS,
};

enum VendorWaveOp
{
  WAVE_OP_NONE = 0,
  WAVE_OP_ADD_FLOAT,
  WAVE_OP_ADD_SINT,
  WAVE_OP_ADD_UINT,
  WAVE_OP_MUL_FLOAT,
  WAVE_OP_MUL_SINT,
  WAVE_OP_MUL_UINT,
  WAVE_OP_MIN_FLOAT,
  WAVE_OP_MIN_SINT,
  WAVE_OP_MIN_UINT,
  WAVE_OP_MAX_FLOAT,
  WAVE_OP_MAX_SINT,
  WAVE_OP_MAX_UINT,
  WAVE_OP_AND,
  WAVE_OP_OR,
  WAVE_OP_XOR,
};

/////////////////////////////////////////////////////////////////////////
// Main structures
/////////////////////////////////////////////////////////////////////////

struct RegIndex;
struct Declaration;

enum class ToString
{
  None = 0x0,
  IsDecl = 0x1,
  ShowSwizzle = 0x2,
  FriendlyNameRegisters = 0x4,
};

constexpr inline ToString operator|(ToString a, ToString b)
{
  return ToString(int(a) | int(b));
}

constexpr inline bool operator&(ToString a, ToString b)
{
  return (int(a) & int(b)) != 0;
}

struct Operand
{
  Operand()
  {
    type = NUM_OPERAND_TYPES;
    numComponents = MAX_COMPONENTS;
    comps[0] = comps[1] = comps[2] = comps[3] = 0xff;
    values[0] = values[1] = values[2] = values[3] = 0;
    flags = FLAG_NONE;
    precision = PRECISION_DEFAULT;
    declaration = NULL;
  }

  bool operator==(const Operand &o) const;
  bool operator<(const Operand &o) const;

  // helper function that compares operands by their type and first index (for resources the logical
  // identifier - excluding register range on SM5.1)
  bool sameResource(const Operand &o) const;

  rdcstr toString(const DXBC::Reflection *reflection, ToString flags) const;

  ///////////////////////////////////////

  // operands can be given names to make the assembly easier to read.
  // mostly used on vendor extensions where the syntax is non-standard/undocumented
  rdcinflexiblestr name;
  // temp register, constant buffer, input, output, other more specialised types
  OperandType type;
  // scalar, 4-vector or N-vector (currently unused)
  NumOperandComponents numComponents;

  enum Flags : uint8_t
  {
    FLAG_NONE = 0x00,
    // these correspond to the bits in OperandModifier
    FLAG_NEG = 0x01,
    FLAG_ABS = 0x02,

    FLAG_NONUNIFORM = 0x04,

    // these are only used to ensure we can output using the same register swizzling as fxc, since
    // fxc unpredictably uses masks and swizzles for .xyzw
    FLAG_SELECTED = 0x08,
    FLAG_SWIZZLED = 0x10,
    FLAG_MASKED = 0x20,

    // for some reason fxc sometimes emits extended operands with... nothing. No modifier or
    // precision. To try and round-trip cleanly we store the extended flag here instead of only
    // emitting an extended operand when we see a modifier or precision
    FLAG_EXTENDED = 0x40,
  } flags;

  MinimumPrecision precision;

  uint8_t comps[4];    // the components. each is 0,1,2,3 for x,y,z,w or 0xff if unused.
                       // e.g. .x    = {  0, -1, -1, -1 }
                       //		.zzw  = {  2,  2,  3, -1 }
                       //		.zyx  = {  2,  1,  0, -1 }
                       //		.zxxw = {  2,  0,  0,  3 }
                       //		.xyzw = {  0,  1,  2,  3 }
                       //		.wzyx = {  3,  2,  1,  0 }

  Operand &swizzle(uint8_t c)
  {
    // you'd think this would use NUMCOMPS_1 but fxc doesn't like that. Those only
    // seem to be used for declarations or other special situations, so we keep to NUMCOMPS_4
    flags = Flags(flags & ~(FLAG_SWIZZLED | FLAG_MASKED | FLAG_SELECTED));
    flags = Flags(flags | FLAG_SELECTED);
    numComponents = NUMCOMPS_4;

    comps[0] = c;
    comps[1] = 0xff;
    comps[2] = 0xff;
    comps[3] = 0xff;

    return *this;
  }

  Operand &swizzle(uint8_t x, uint8_t y, uint8_t z, uint8_t w)
  {
    numComponents = NUMCOMPS_4;
    flags = Flags(flags & ~(FLAG_SWIZZLED | FLAG_MASKED | FLAG_SELECTED));

    if(x == 0xff || y == 0xff || z == 0xff || w == 0xff)
      flags = Flags(flags | FLAG_MASKED);
    else
      flags = Flags(flags | FLAG_SWIZZLED);

    comps[0] = x;
    comps[1] = y;
    comps[2] = z;
    comps[3] = w;

    return *this;
  }

  Operand &reswizzle(uint8_t c)
  {
    // you'd think this would use NUMCOMPS_1 but fxc doesn't like that. Those only
    // seem to be used for declarations or other special situations, so we keep to NUMCOMPS_4
    flags = Flags(flags & ~(FLAG_SWIZZLED | FLAG_MASKED | FLAG_SELECTED));
    flags = Flags(flags | FLAG_SELECTED);
    numComponents = NUMCOMPS_4;

    comps[0] = comps[c];
    comps[1] = 0xff;
    comps[2] = 0xff;
    comps[3] = 0xff;

    values[0] = values[c];
    values[1] = 0;
    values[2] = 0;
    values[3] = 0;

    return *this;
  }

  Operand &reswizzle(uint8_t x, uint8_t y, uint8_t z, uint8_t w)
  {
    rdcfixedarray<uint32_t, 4> oldVals = values;
    rdcfixedarray<uint8_t, 4> oldComps = comps;

    numComponents = NUMCOMPS_4;
    flags = Flags(flags & ~(FLAG_SWIZZLED | FLAG_MASKED | FLAG_SELECTED));

    if(x == 0xff || y == 0xff || z == 0xff || w == 0xff)
      flags = Flags(flags | FLAG_MASKED);
    else
      flags = Flags(flags | FLAG_SWIZZLED);

    comps[0] = oldComps[x];
    comps[1] = y < 4 ? oldComps[y] : 0xff;
    comps[2] = z < 4 ? oldComps[z] : 0xff;
    comps[3] = w < 4 ? oldComps[w] : 0xff;

    values[0] = oldVals[x];
    values[1] = z < 4 ? oldVals[y] : 0;
    values[2] = z < 4 ? oldVals[z] : 0;
    values[3] = w < 4 ? oldVals[w] : 0;

    return *this;
  }

  void setComps(uint8_t x, uint8_t y, uint8_t z, uint8_t w)
  {
    flags = Flags(flags & ~(FLAG_SWIZZLED | FLAG_MASKED | FLAG_SELECTED));

    // you'd think this would use NUMCOMPS_1 but fxc doesn't like that. Those only
    // seem to be used for declarations or other special situations, so we keep to NUMCOMPS_4
    numComponents = NUMCOMPS_4;
    if(y == 0xff && z == 0xff && w == 0xff)
      flags = Flags(flags | FLAG_SELECTED);
    else if(x == 0xff || y == 0xff || z == 0xff || w == 0xff)
      flags = Flags(flags | FLAG_MASKED);
    else
      flags = Flags(flags | FLAG_SWIZZLED);

    comps[0] = x;
    comps[1] = y;
    comps[2] = z;
    comps[3] = w;
  }

  // indices for this register.
  // 0 means this is a special register, specified by type alone.
  // 1 is probably most common. Indicates RegIndex specifies the register
  // 2 is for constant buffers, array inputs etc. [0] indicates the cbuffer, [1] indicates the
  // cbuffer member
  // 3 is rare but follows the above pattern
  rdcarray<RegIndex> indices;

  // the declaration of the resource in this operand (not always present)
  Declaration *declaration;

  uint32_t values[4];    // if this operand is immediate, the values are here
};

struct RegIndex
{
  RegIndex()
  {
    absolute = false;
    relative = false;
    index = 0;
  }

  bool operator!=(const RegIndex &o) const { return !(*this == o); }
  bool operator==(const RegIndex &o) const
  {
    if(absolute == o.absolute && relative == o.relative)
    {
      if(absolute && !relative)
        return index == o.index;
      else if(relative && !absolute)
        return operand == o.operand;

      return index == o.index && operand == o.operand;
    }

    return false;
  }
  bool operator<(const RegIndex &o) const
  {
    if(absolute != o.absolute)
      return absolute < o.absolute;
    if(relative != o.relative)
      return relative < o.relative;
    if(index != o.index)
      return index < o.index;
    return operand < o.operand;
  }

  rdcstr toString(const DXBC::Reflection *reflection, ToString flags) const;

  ///////////////////////////////////////

  bool absolute;    // if true, use uint64 index below as an absolute value
  bool relative;    // if true, use the operand below.

  // note, absolute == relative == true IS VALID. It means you must add the two.
  // both cannot be false, at least one must be true.

  uint64_t index;
  Operand operand;
};

struct Declaration
{
  rdcstr str;
  // many decls use an operand to declare things
  Operand operand;
  // function table for interface operations
  rdcarray<uint32_t> functionTableContents;

  OpcodeType declaration = NUM_REAL_OPCODES;
  // all the resource/cbuffer declarations can have a space in SM5.1, extract it here
  uint32_t space = 0;

  // opcode specific data in anonymous union
  union
  {
    // OPCODE_DCL_GLOBAL_FLAGS
    struct
    {
      bool refactoringAllowed;
      bool doublePrecisionFloats;
      bool forceEarlyDepthStencil;
      bool enableRawAndStructuredBuffers;
      bool skipOptimisation;
      bool enableMinPrecision;
      bool enableD3D11_1DoubleExtensions;
      bool enableD3D11_1ShaderExtensions;
      bool enableD3D12AllResourcesBound;
    } global_flags;

    // OPCODE_DCL_RESOURCE
    struct
    {
      DXBC::ResourceRetType resType[4];
      ResourceDimension dim;
      uint32_t sampleCount;
    } resource;

    // OPCODE_DCL_RESOURCE_STRUCTURED
    // OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED
    struct
    {
      uint32_t stride;
      bool hasCounter;
      bool globallyCoherant;
      bool rov;
    } structured;

    // OPCODE_DCL_RESOURCE_RAW
    // OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW
    struct
    {
      ResourceDimension dim;
      bool globallyCoherant;
      bool rov;
      DXBC::ResourceRetType resType[4];
    } raw;

    // OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED
    struct
    {
      ResourceDimension dim;
      bool globallyCoherant;
      bool rov;
      DXBC::ResourceRetType resType[4];
    } uav_typed;

    // OPCODE_DCL_TEMPS
    uint32_t numTemps;

    // OPCODE_DCL_INDEXABLE_TEMP
    struct
    {
      uint32_t numTemps;
      uint32_t tempReg;
      uint32_t tempComponentCount;
    } indexable_temp;

    // OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW
    uint32_t tgsmCount;

    // OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED
    struct
    {
      uint32_t count;
      uint32_t stride;
    } tsgm_structured;

    // OPCODE_DCL_THREAD_GROUP
    uint32_t groupSize[3];

    // OPCODE_DCL_CONSTANT_BUFFER
    struct
    {
      CBufferAccessPattern accessPattern;
      uint32_t vectorSize;
    } cbuffer;

    // OPCODE_DCL_INPUT_PS
    // OPCODE_DCL_INPUT_PS_SIV
    // OPCODE_DCL_INPUT_SIV
    // OPCODE_DCL_INPUT_SGV
    // OPCODE_DCL_INPUT_PS_SGV
    struct
    {
      // only used for PS inputs
      InterpolationMode inputInterpolation;
      DXBC::SVSemantic systemValue;
    } inputOutput;

    // OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT
    uint32_t maxVertexOutCount;

    // OPCODE_DCL_SAMPLER
    SamplerMode samplerMode;

    // OPCODE_DCL_TESS_DOMAIN
    TessellatorDomain tessDomain;

    // OPCODE_DCL_INPUT_CONTROL_POINT_COUNT
    uint32_t controlPointCount;

    // OPCODE_DCL_TESS_PARTITIONING
    TessellatorPartitioning tessPartition;

    // OPCODE_DCL_TESS_OUTPUT_PRIMITIVE
    TessellatorOutputPrimitive tessOutputPrimitive;

    // OPCODE_DCL_GS_INPUT_PRIMITIVE
    PrimitiveType geomInputPrimitive;

    // OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY
    D3D_PRIMITIVE_TOPOLOGY geomOutputTopology;

    // OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT
    // OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT
    // OPCODE_DCL_GS_INSTANCE_COUNT
    uint32_t instanceCount;

    // OPCODE_DCL_INDEX_RANGE
    uint32_t indexRange;

    // OPCODE_DCL_HS_MAX_TESSFACTOR
    float maxTessFactor;

    // OPCODE_DCL_FUNCTION_BODY
    uint32_t functionBody;

    // OPCODE_DCL_FUNCTION_TABLE
    uint32_t functionTable;

    // OPCODE_CUSTOMDATA
    uint32_t customDataIndex;

    // OPCODE_DCL_INTERFACE
    struct
    {
      uint32_t interfaceID;
      uint32_t numInterfaces;
      uint32_t numTypes;
    } iface;
  };
};

struct Operation
{
  Operation()
  {
    offset = 0;
    line = 0;
    stride = 0;
    operation = NUM_REAL_OPCODES;
    flags = FLAG_NONE;
    preciseValues = 0;
    infoRetType = NUM_RETTYPES;
    syncFlags = 0;
    texelOffset[0] = texelOffset[1] = texelOffset[2] = 0;
    resDim = RESOURCE_DIMENSION_UNKNOWN;
    resType[0] = resType[1] = resType[2] = resType[3] = DXBC::RETURN_TYPE_UNUSED;
  }

  rdcstr str;

  // for if, etc. If it checks for zero or nonzero
  bool nonzero() const { return (flags & FLAG_NONZERO) != 0; }
  // should the result be saturated.
  bool saturate() const { return (flags & FLAG_SATURATE) != 0; }
  ///////////////////////////////////////

  uintptr_t offset;
  uint32_t line;

  OpcodeType operation;

  union
  {
    uint32_t stride;
    uint32_t customDataIndex;
  };

  uint8_t preciseValues;    // for multiple output operand operations

  union
  {
    ResinfoRetType infoRetType;    // return type of resinfo
    uint8_t syncFlags;             // sync flags (for compute shader sync operations)
  };

  int8_t texelOffset[3] = {0};         // U,V,W texel offset
  ResourceDimension resDim;            // resource dimension (tex2d etc)
  DXBC::ResourceRetType resType[4];    // return type (e.g. for a sample operation)

  enum Flags : uint8_t
  {
    FLAG_NONE = 0x0,
    FLAG_NONZERO = 0x01,
    FLAG_SATURATE = 0x02,
    FLAG_TEXEL_OFFSETS = 0x04,
    FLAG_RESOURCE_DIMS = 0x08,
    FLAG_RET_TYPE = 0x10,

    // sometimes fxc emits a 0 dword after the operand. For bitwise-compatibility we store a flag
    // here for that case to emit it again
    FLAG_TRAILING_ZERO_TOKEN = 0x20,
  } flags;

  rdcarray<Operand> operands;
};

class Program
{
public:
  Program(const byte *bytes, size_t length);
  Program(const Program &o) = delete;
  Program(Program &&o) = delete;
  Program &operator=(const Program &o) = delete;

  void SetShaderEXTUAV(GraphicsAPI api, uint32_t space, uint32_t reg)
  {
    m_API = api;
    m_ShaderExt = {space, reg};
  }
  void FetchComputeProperties(DXBC::Reflection *reflection);
  DXBC::Reflection *GuessReflection();

  const rdcarray<uint32_t> &GetTokens() const { return m_ProgramWords; }
  rdcstr GetDebugStatus();

  void SetReflection(const DXBC::Reflection *refl) { m_Reflection = refl; }
  void SetDebugInfo(const DXBC::IDebugInfo *debug) { m_DebugInfo = debug; }
  DXBC::ShaderType GetShaderType() const { return m_Type; }
  uint32_t GetMajorVersion() const { return m_Major; }
  uint32_t GetMinorVersion() const { return m_Minor; }
  bool IsShaderModel51() const { return m_Major == 5 && m_Minor == 1; }
  D3D_PRIMITIVE_TOPOLOGY GetOutputTopology();
  const rdcstr &GetDisassembly()
  {
    if(m_Disassembly.empty())
      MakeDisassemblyString();
    return m_Disassembly;
  }
  size_t GetNumDeclarations() const { return m_Declarations.size(); }
  const Declaration &GetDeclaration(size_t i) const { return m_Declarations[i]; }
  const Declaration *FindDeclaration(OperandType declType, uint32_t identifier) const;
  size_t GetNumInstructions() const { return m_Instructions.size(); }
  const Operation &GetInstruction(size_t i) const { return m_Instructions[i]; }
  const rdcarray<uint32_t> &GetImmediateConstantBuffer() const { return m_Immediate; }
  void SetupRegisterFile(rdcarray<ShaderVariable> &registers) const;
  uint32_t GetRegisterIndex(OperandType type, uint32_t index) const;
  bool HasCoverageInput() const { return m_InputCoverage; }
  rdcstr GetRegisterName(OperandType oper, uint32_t index) const;

  static bool UsesExtensionUAV(uint32_t slot, uint32_t space, const byte *bytes, size_t length);
  static D3D_PRIMITIVE_TOPOLOGY GetOutputTopology(const byte *bytes, size_t length);

protected:
  friend class Program;

  Program(const rdcarray<uint32_t> &words);
  void DecodeProgram();
  rdcarray<uint32_t> EncodeProgram();

  void PostprocessVendorExtensions();

  void MakeDisassemblyString();

  const DXBC::Reflection *m_Reflection = NULL;
  const DXBC::IDebugInfo *m_DebugInfo = NULL;

  DXBC::ShaderType m_Type = DXBC::ShaderType::Max;
  uint32_t m_Major = 0, m_Minor = 0;

  rdcarray<uint32_t> m_ProgramWords;

  rdcarray<uint32_t> m_Immediate;

  rdcarray<rdcpair<CustomDataClass, rdcarray<uint32_t>>> m_CustomDatas;

  uint32_t m_NumTemps = 0;
  rdcarray<uint32_t> m_IndexTempSizes;

  // most regular outputs, including system value outputs like primitive ID are given a register
  // number
  uint32_t m_NumOutputs = 0;
  // these outputs are different and have no index
  bool m_OutputCoverage = false, m_OutputDepth = false, m_OutputStencil = false;

  bool m_InputCoverage = false;

  bool m_Disassembled = false;

  GraphicsAPI m_API = GraphicsAPI::D3D11;
  rdcpair<uint32_t, uint32_t> m_ShaderExt = {~0U, ~0U};

  rdcstr m_Disassembly;

  // declarations of inputs, outputs, constant buffers, temp registers etc.
  rdcarray<Declaration> m_Declarations;
  rdcarray<Operation> m_Instructions;

  // declarations later in shaders - only used for different phases in hull shaders where
  // declarations can happen after some instructions
  rdcarray<rdcarray<Declaration>> m_LateDeclarations;

  // these functions modify tokenStream pointer to point after the item
  // ExtractOperation/ExtractDecl returns false if not an operation (ie. it's a declaration)
  bool DecodeOperation(uint32_t *&tokenStream, Operation &op, bool friendlyName);
  bool DecodeDecl(uint32_t *&tokenStream, Declaration &decl, bool friendlyName);
  bool DecodeOperand(uint32_t *&tokenStream, ToString flags, Operand &oper);

  void EncodeOperand(rdcarray<uint32_t> &tokenStream, const Operand &oper);
  void EncodeDecl(rdcarray<uint32_t> &tokenStream, const Declaration &decl);
  void EncodeOperation(rdcarray<uint32_t> &tokenStream, const Operation &op);
};
};
