/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "api/replay/renderdoc_replay.h"
#include "driver/dx/official/d3dcommon.h"

namespace DXBC
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

  NUM_OPCODES,
};

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

enum ResinfoRetType
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

enum NumOperandComponents
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

enum OperandType
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

enum SVSemantic
{
  SVNAME_UNDEFINED = 0,
  SVNAME_POSITION,
  SVNAME_CLIP_DISTANCE,
  SVNAME_CULL_DISTANCE,
  SVNAME_RENDER_TARGET_ARRAY_INDEX,
  SVNAME_VIEWPORT_ARRAY_INDEX,
  SVNAME_VERTEX_ID,
  SVNAME_PRIMITIVE_ID,
  SVNAME_INSTANCE_ID,
  SVNAME_IS_FRONT_FACE,
  SVNAME_SAMPLE_INDEX,

  // following are non-contiguous
  SVNAME_FINAL_QUAD_EDGE_TESSFACTOR0,
  SVNAME_FINAL_QUAD_EDGE_TESSFACTOR = SVNAME_FINAL_QUAD_EDGE_TESSFACTOR0,
  SVNAME_FINAL_QUAD_EDGE_TESSFACTOR1,
  SVNAME_FINAL_QUAD_EDGE_TESSFACTOR2,
  SVNAME_FINAL_QUAD_EDGE_TESSFACTOR3,

  SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR0,
  SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR = SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR0,
  SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR1,

  SVNAME_FINAL_TRI_EDGE_TESSFACTOR0,
  SVNAME_FINAL_TRI_EDGE_TESSFACTOR = SVNAME_FINAL_TRI_EDGE_TESSFACTOR0,
  SVNAME_FINAL_TRI_EDGE_TESSFACTOR1,
  SVNAME_FINAL_TRI_EDGE_TESSFACTOR2,

  SVNAME_FINAL_TRI_INSIDE_TESSFACTOR,

  SVNAME_FINAL_LINE_DETAIL_TESSFACTOR,

  SVNAME_FINAL_LINE_DENSITY_TESSFACTOR,

  SVNAME_TARGET = 64,
  SVNAME_DEPTH,
  SVNAME_COVERAGE,
  SVNAME_DEPTH_GREATER_EQUAL,
  SVNAME_DEPTH_LESS_EQUAL,
};

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

enum OperandModifier
{
  OPERAND_MODIFIER_NONE = 0,
  OPERAND_MODIFIER_NEG,
  OPERAND_MODIFIER_ABS,
  OPERAND_MODIFIER_ABSNEG,

  NUM_MODIFIERS,
};

enum MinimumPrecision
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

enum ResourceDimension
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

enum ResourceRetType
{
  RETURN_TYPE_UNKNOWN = 0,
  RETURN_TYPE_UNORM = 1,
  RETURN_TYPE_SNORM,
  RETURN_TYPE_SINT,
  RETURN_TYPE_UINT,
  RETURN_TYPE_FLOAT,
  RETURN_TYPE_MIXED,
  RETURN_TYPE_DOUBLE,
  RETURN_TYPE_CONTINUED,
  RETURN_TYPE_UNUSED,

  NUM_RETURN_TYPES,
};

enum ComponentType
{
  COMPONENT_TYPE_UNKNOWN = 0,
  COMPONENT_TYPE_UINT32,
  COMPONENT_TYPE_SINT32,
  COMPONENT_TYPE_FLOAT32,

  NUM_COMP_TYPES,
};

/////////////////////////////////////////////////////////////////////////
// Main structures
/////////////////////////////////////////////////////////////////////////

class DXBCFile;

struct ASMIndex;
struct ASMDecl;

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

struct ASMOperand
{
  ASMOperand()
  {
    type = NUM_OPERAND_TYPES;
    numComponents = MAX_COMPONENTS;
    comps[0] = comps[1] = comps[2] = comps[3] = 0xff;
    values[0] = values[1] = values[2] = values[3] = 0;
    modifier = OPERAND_MODIFIER_NONE;
    precision = PRECISION_DEFAULT;
    funcNum = 0;
    declaration = NULL;
  }

  bool operator==(const ASMOperand &o) const;

  std::string toString(DXBCFile *dxbc, ToString flags) const;

  ///////////////////////////////////////

  OperandType
      type;    // temp register, constant buffer, input, output, other more specialised types
  NumOperandComponents numComponents;    // scalar, 4-vector or N-vector (currently unused)

  uint8_t comps[4];    // the components. each is 0,1,2,3 for x,y,z,w or 0xff if unused.
                       // e.g. .x    = {  0, -1, -1, -1 }
                       //		.zzw  = {  2,  2,  3, -1 }
                       //		.zyx  = {  2,  1,  0, -1 }
                       //		.zxxw = {  2,  0,  0,  3 }
                       //		.xyzw = {  0,  1,  2,  3 }
                       //		.wzyx = {  3,  2,  1,  0 }

  std::vector<ASMIndex> indices;    // indices for this register.
                                    // 0 means this is a special register, specified by type alone.
  // 1 is probably most common. Indicates ASMIndex specifies the register
  // 2 is for constant buffers, array inputs etc. [0] indicates the cbuffer, [1] indicates the
  // cbuffer member
  // 3 is rare but follows the above pattern

  // the declaration of the resource in this operand (not always present)
  ASMDecl *declaration;

  uint32_t values[4];    // if this operand is immediate, the values are here

  OperandModifier modifier;    // modifier, neg, abs(), -abs() etc. Could potentially be multiple
                               // modifiers in future
  MinimumPrecision precision;

  uint32_t funcNum;    // interface this operand refers to
};

struct ASMIndex
{
  ASMIndex()
  {
    absolute = false;
    relative = false;
    index = 0;
  }

  bool operator!=(const ASMIndex &o) const { return !(*this == o); }
  bool operator==(const ASMIndex &o) const
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

  std::string str;

  ///////////////////////////////////////

  bool absolute;    // if true, use uint64 index below as an absolute value
  bool relative;    // if true, use the operand below.

  // note, absolute == relative == true IS VALID. It means you must add the two.
  // both cannot be false, at least one must be true.

  uint64_t index;
  ASMOperand operand;
};

struct ASMDecl
{
  ASMDecl()
  {
    offset = 0;
    length = 0;
    instruction = 0;
    declaration = NUM_OPCODES;
    refactoringAllowed = doublePrecisionFloats = forceEarlyDepthStencil =
        enableRawAndStructuredBuffers = skipOptimisation = enableMinPrecision =
            enableD3D11_1DoubleExtensions = enableD3D11_1ShaderExtensions =
                enableD3D12AllResourcesBound = false;
    stride = 0;
    hasCounter = false;
    rov = false;
    numTemps = 0;
    tempReg = 0;
    tempComponentCount = 0;
    count = 0;
    groupSize[0] = groupSize[1] = groupSize[2] = 0;
    space = 0;
    resType[0] = resType[1] = resType[2] = resType[3] = NUM_RETURN_TYPES;
    dim = RESOURCE_DIMENSION_UNKNOWN;
    sampleCount = 0;
    interpolation = INTERPOLATION_UNDEFINED;
    systemValue = SVNAME_UNDEFINED;
    maxOut = 0;
    samplerMode = NUM_SAMPLERS;
    domain = DOMAIN_UNDEFINED;
    controlPointCount = 0;
    partition = PARTITIONING_UNDEFINED;
    outPrim = OUTPUT_PRIMITIVE_UNDEFINED;
    inPrim = PRIMITIVE_UNDEFINED;
    outTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    instanceCount = 0;
    indexRange = 0;
    maxTessFactor = 0.0f;
    globallyCoherant = false;
    functionBody = 0;
    functionTable = 0;
    interfaceID = 0;
    numInterfaces = 0;
    numTypes = 0;
  }

  std::string str;

  ///////////////////////////////////////

  uintptr_t offset;
  uint32_t length;

  size_t instruction;    // happens before this instruction. Usually 0 as all decls are up front,
                         // but can be non-zero for e.g. HS control point and join phase
  OpcodeType declaration;

  ASMOperand operand;    // many decls use an operand to declare things

  std::vector<uint32_t> immediateData;    // raw data (like default value of operand) for immediate
                                          // constant buffer decl

  // opcode specific data

  // OPCODE_DCL_GLOBAL_FLAGS
  bool refactoringAllowed;
  bool doublePrecisionFloats;
  bool forceEarlyDepthStencil;
  bool enableRawAndStructuredBuffers;
  bool skipOptimisation;
  bool enableMinPrecision;
  bool enableD3D11_1DoubleExtensions;
  bool enableD3D11_1ShaderExtensions;
  bool enableD3D12AllResourcesBound;

  // OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED
  uint32_t stride;
  bool hasCounter;
  bool rov;

  // OPCODE_DCL_TEMPS, OPCODE_DCL_INDEXABLE_TEMP
  uint32_t numTemps;

  // OPCODE_DCL_INDEXABLE_TEMP
  uint32_t tempReg;
  uint32_t tempComponentCount;

  // OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED
  uint32_t count;

  // OPCODE_DCL_THREAD_GROUP
  uint32_t groupSize[3];

  // OPCODE_DCL_RESOURCE
  uint32_t space;
  ResourceRetType resType[4];
  ResourceDimension dim;
  uint32_t sampleCount;

  // OPCODE_DCL_INPUT_PS
  InterpolationMode interpolation;

  // OPCODE_DCL_INPUT_SIV
  SVSemantic systemValue;

  // OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT
  uint32_t maxOut;

  // OPCODE_DCL_SAMPLER
  SamplerMode samplerMode;

  // OPCODE_DCL_TESS_DOMAIN
  TessellatorDomain domain;

  // OPCODE_DCL_INPUT_CONTROL_POINT_COUNT
  uint32_t controlPointCount;

  // OPCODE_DCL_TESS_PARTITIONING
  TessellatorPartitioning partition;

  // OPCODE_DCL_TESS_OUTPUT_PRIMITIVE
  TessellatorOutputPrimitive outPrim;

  // OPCODE_DCL_GS_INPUT_PRIMITIVE
  PrimitiveType inPrim;

  // OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY
  D3D_PRIMITIVE_TOPOLOGY outTopology;

  // OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT
  // OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT
  // OPCODE_DCL_GS_INSTANCE_COUNT
  uint32_t instanceCount;

  // OPCODE_DCL_INDEX_RANGE
  uint32_t indexRange;

  // OPCODE_DCL_HS_MAX_TESSFACTOR
  float maxTessFactor;

  // OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED
  bool globallyCoherant;

  // OPCODE_DCL_FUNCTION_BODY
  uint32_t functionBody;

  // OPCODE_DCL_FUNCTION_TABLE
  uint32_t functionTable;

  // OPCODE_DCL_INTERFACE
  uint32_t interfaceID;
  uint32_t numInterfaces;
  uint32_t numTypes;
};

struct ASMOperation
{
  ASMOperation()
  {
    offset = 0;
    length = 0;
    stride = 0;
    operation = NUM_OPCODES;
    nonzero = false;
    saturate = false;
    preciseValues = 0;
    resinfoRetType = NUM_RETTYPES;
    syncFlags = 0;
    texelOffset[0] = texelOffset[1] = texelOffset[2] = 0;
    resDim = RESOURCE_DIMENSION_UNKNOWN;
    resType[0] = resType[1] = resType[2] = resType[3] = RETURN_TYPE_UNUSED;
  }

  std::string str;

  ///////////////////////////////////////

  uintptr_t offset;
  uint32_t length;

  OpcodeType operation;
  bool nonzero;                     // for if, etc. If it checks for zero or nonzero
  bool saturate;                    // should the result be saturated.
  uint32_t preciseValues;           // for multiple output operand operations
  ResinfoRetType resinfoRetType;    // return type of resinfo
  uint32_t syncFlags;               // sync flags (for compute shader sync operations)

  int texelOffset[3];            // U,V,W texel offset
  ResourceDimension resDim;      // resource dimension (tex2d etc)
  ResourceRetType resType[4];    // return type (e.g. for a sample operation)
  uint32_t stride;

  std::vector<ASMOperand> operands;
};

};    // DXBC