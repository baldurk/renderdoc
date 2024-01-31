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

#include "api/replay/rdcarray.h"
#include "common/common.h"

// undef some annoying defines that might come from OS headers
#undef VOID
#undef FLOAT
#undef LABEL
#undef OPAQUE

namespace LLVMBC
{
static const uint32_t BitcodeMagic = MAKE_FOURCC('B', 'C', 0xC0, 0xDE);

enum class AbbrevEncoding : uint8_t
{
  Unknown = 0,
  Fixed = 1,
  VBR = 2,
  Array = 3,
  Char6 = 4,
  Blob = 5,
  // the abbrev encoding is only 3 bits, so 8 is not representable, we can store whether or not
  // we're a literal this way.
  Literal = 8,
};

enum AbbrevId
{
  END_BLOCK = 0,
  ENTER_SUBBLOCK = 1,
  DEFINE_ABBREV = 2,
  UNABBREV_RECORD = 3,
  APPLICATION_ABBREV = 4,
};

enum class BlockInfoRecord
{
  SETBID = 1,
  BLOCKNAME = 2,
  SETRECORDNAME = 3,
};

enum class KnownBlock : uint32_t
{
  BLOCKINFO = 0,

  // 1-7 reserved,

  MODULE_BLOCK = 8,
  PARAMATTR_BLOCK = 9,
  PARAMATTR_GROUP_BLOCK = 10,
  CONSTANTS_BLOCK = 11,
  FUNCTION_BLOCK = 12,
  // TYPE_SYMTAB_BLOCK deprecated?
  VALUE_SYMTAB_BLOCK = 14,
  METADATA_BLOCK = 15,
  METADATA_ATTACHMENT = 16,
  TYPE_BLOCK = 17,
  USELIST_BLOCK = 18,

  Count,
};

enum class ModuleRecord : uint32_t
{
  VERSION = 1,
  TRIPLE = 2,
  DATALAYOUT = 3,
  SECTIONNAME = 5,
  GLOBALVAR = 7,
  FUNCTION = 8,
  ALIAS = 9,
  COMDAT = 12,
};

enum class ConstantsRecord : uint32_t
{
  SETTYPE = 1,
  CONST_NULL = 2,
  UNDEF = 3,
  INTEGER = 4,
  FLOAT = 6,
  AGGREGATE = 7,
  STRING = 8,
  CSTRING = 9,
  EVAL_BINOP = 10,
  EVAL_CAST = 11,
  EVAL_GEP = 20,
  DATA = 22,
};

enum class FunctionRecord : uint32_t
{
  DECLAREBLOCKS = 1,
  INST_BINOP = 2,
  INST_CAST = 3,
  INST_GEP_OLD = 4,
  INST_SELECT = 5,
  INST_EXTRACTELT = 6,
  INST_INSERTELT = 7,
  INST_SHUFFLEVEC = 8,
  INST_CMP = 9,
  INST_RET = 10,
  INST_BR = 11,
  INST_SWITCH = 12,
  INST_INVOKE = 13,
  INST_UNREACHABLE = 15,
  INST_PHI = 16,
  INST_ALLOCA = 19,
  INST_LOAD = 20,
  INST_VAARG = 23,
  INST_STORE_OLD = 24,
  INST_EXTRACTVAL = 26,
  INST_INSERTVAL = 27,
  INST_CMP2 = 28,
  INST_VSELECT = 29,
  INST_INBOUNDS_GEP_OLD = 30,
  INST_INDIRECTBR = 31,
  DEBUG_LOC_AGAIN = 33,
  INST_CALL = 34,
  DEBUG_LOC = 35,
  INST_FENCE = 36,
  INST_CMPXCHG_OLD = 37,
  INST_ATOMICRMW = 38,
  INST_RESUME = 39,
  INST_LANDINGPAD_OLD = 40,
  INST_LOADATOMIC = 41,
  INST_STOREATOMIC_OLD = 42,
  INST_GEP = 43,
  INST_STORE = 44,
  INST_STOREATOMIC = 45,
  INST_CMPXCHG = 46,
  INST_LANDINGPAD = 47,
};

enum class ParamAttrRecord : uint32_t
{
  ENTRY = 2,
};

enum class ParamAttrGroupRecord : uint32_t
{
  ENTRY = 3,
};

enum class ValueSymtabRecord : uint32_t
{
  ENTRY = 1,
  BBENTRY = 2,
  FNENTRY = 3,
  COMBINED_ENTRY = 5,
};

enum class UselistRecord : uint32_t
{
  DEFAULT = 1,
  BB = 2,
};

enum class MetaDataRecord : uint32_t
{
  STRING_OLD = 1,
  VALUE = 2,
  NODE = 3,
  NAME = 4,
  DISTINCT_NODE = 5,
  KIND = 6,
  LOCATION = 7,
  OLD_NODE = 8,
  OLD_FN_NODE = 9,
  NAMED_NODE = 10,
  ATTACHMENT = 11,
  GENERIC_DEBUG = 12,
  SUBRANGE = 13,
  ENUMERATOR = 14,
  BASIC_TYPE = 15,
  FILE = 16,
  DERIVED_TYPE = 17,
  COMPOSITE_TYPE = 18,
  SUBROUTINE_TYPE = 19,
  COMPILE_UNIT = 20,
  SUBPROGRAM = 21,
  LEXICAL_BLOCK = 22,
  LEXICAL_BLOCK_FILE = 23,
  NAMESPACE = 24,
  TEMPLATE_TYPE = 25,
  TEMPLATE_VALUE = 26,
  GLOBAL_VAR = 27,
  LOCAL_VAR = 28,
  EXPRESSION = 29,
  OBJC_PROPERTY = 30,
  IMPORTED_ENTITY = 31,
  MODULE = 32,
  MACRO = 33,
  MACRO_FILE = 34,
  STRINGS = 35,
  GLOBAL_DECL_ATTACHMENT = 36,
  GLOBAL_VAR_EXPR = 37,
  INDEX_OFFSET = 38,
  INDEX = 39,
  LABEL = 40,
  COMMON_BLOCK = 44,
};

enum class TypeRecord : uint32_t
{
  NUMENTRY = 1,
  VOID = 2,
  FLOAT = 3,
  DOUBLE = 4,
  LABEL = 5,
  OPAQUE = 6,
  INTEGER = 7,
  POINTER = 8,
  FUNCTION_OLD = 9,
  HALF = 10,
  ARRAY = 11,
  VECTOR = 12,
  METADATA = 16,
  STRUCT_ANON = 18,
  STRUCT_NAME = 19,
  STRUCT_NAMED = 20,
  FUNCTION = 21,
};

struct AbbrevParam
{
  AbbrevEncoding encoding;
  uint64_t value;    // this is also the bitwidth for Fixed/VBR
};

struct AbbrevDesc
{
  rdcarray<AbbrevParam> params;
};
};
