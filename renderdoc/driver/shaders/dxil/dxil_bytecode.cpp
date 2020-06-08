/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "dxil_bytecode.h"
#include <ctype.h>
#include <stdio.h>
#include <string>
#include "common/common.h"
#include "common/formatting.h"
#include "maths/half_convert.h"
#include "os/os_specific.h"
#include "llvm_decoder.h"

// undef some annoying defines that might come from OS headers
#undef VOID
#undef FLOAT
#undef LABEL
#undef OPAQUE

namespace DXIL
{
struct ProgramHeader
{
  uint16_t ProgramVersion;
  uint16_t ProgramType;
  uint32_t SizeInUint32;     // Size in uint32_t units including this header.
  uint32_t DxilMagic;        // 0x4C495844, ASCII "DXIL".
  uint32_t DxilVersion;      // DXIL version.
  uint32_t BitcodeOffset;    // Offset to LLVM bitcode (from DxilMagic).
  uint32_t BitcodeSize;      // Size of LLVM bitcode.
};

enum class KnownBlocks : uint32_t
{
  BLOCKINFO = 0,

  // 1-7 reserved,

  MODULE_BLOCK = 8,
  PARAMATTR_BLOCK = 9,
  PARAMATTR_GROUP_BLOCK = 10,
  CONSTANTS_BLOCK = 11,
  FUNCTION_BLOCK = 12,
  TYPE_SYMTAB_BLOCK = 13,
  VALUE_SYMTAB_BLOCK = 14,
  METADATA_BLOCK = 15,
  METADATA_ATTACHMENT = 16,
  TYPE_BLOCK = 17,
};

enum class ModuleRecord : uint32_t
{
  VERSION = 1,
  TRIPLE = 2,
  DATALAYOUT = 3,
  GLOBALVAR = 7,
  FUNCTION = 8,
  ALIAS = 14,
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
  INST_CLEANUPRET = 48,
  INST_CATCHRET = 49,
  INST_CATCHPAD = 50,
  INST_CLEANUPPAD = 51,
  INST_CATCHSWITCH = 52,
  OPERAND_BUNDLE = 55,
  INST_UNOP = 56,
  INST_CALLBR = 57,
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

#define IS_KNOWN(val, KnownID) (decltype(KnownID)(val) == KnownID)

void ParseConstant(const LLVMBC::BlockOrRecord &constant, const Type *&curType,
                   std::function<Type *(uint64_t)> getType,
                   std::function<const Value *(uint64_t)> getValue,
                   std::function<void(const Value &)> addValue)
{
  if(IS_KNOWN(constant.id, ConstantsRecord::SETTYPE))
  {
    curType = getType(constant.ops[0]);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL) ||
          IS_KNOWN(constant.id, ConstantsRecord::UNDEF))
  {
    Value v;
    v.type = curType;
    v.nullconst = IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL);
    v.undef = IS_KNOWN(constant.id, ConstantsRecord::UNDEF);
    addValue(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::INTEGER))
  {
    Value v;
    v.type = curType;
    v.val.u64v[0] = constant.ops[0];
    if(v.val.u64v[0] & 0x1)
      v.val.s64v[0] = -int64_t(v.val.u64v[0] >> 1);
    else
      v.val.u64v[0] >>= 1;
    addValue(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::FLOAT))
  {
    Value v;
    v.type = curType;
    if(curType->bitWidth == 16)
      v.val.fv[0] = ConvertFromHalf(uint16_t(constant.ops[0] & 0xffff));
    else if(curType->bitWidth == 32)
      memcpy(&v.val.fv[0], &constant.ops[0], sizeof(float));
    else
      memcpy(&v.val.dv[0], &constant.ops[0], sizeof(double));
    addValue(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::STRING))
  {
    Value v;
    v.type = curType;
    v.str = constant.getString(0);
    addValue(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::AGGREGATE))
  {
    Value v;
    v.type = curType;
    if(v.type->type == Type::Vector)
    {
      // inline vectors
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        const Value *member = getValue(constant.ops[m]);

        if(member)
        {
          if(v.type->bitWidth <= 32)
            v.val.uv[m] = member->val.uv[m];
          else
            v.val.u64v[m] = member->val.u64v[m];
        }
        else
        {
          RDCERR("Index %llu out of bounds for values array", constant.ops[m]);
        }
      }
    }
    else
    {
      for(uint64_t m : constant.ops)
      {
        const Value *member = getValue(m);

        if(member)
        {
          v.members.push_back(*member);
        }
        else
        {
          v.members.push_back(Value());
          RDCERR("Index %llu out of bounds for values array", m);
        }
      }
    }
    addValue(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::DATA))
  {
    Value v;
    v.type = curType;
    if(v.type->type == Type::Vector)
    {
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        if(v.type->bitWidth <= 32)
          v.val.uv[m] = constant.ops[m] & ((1ULL << v.type->bitWidth) - 1);
        else
          v.val.u64v[m] = constant.ops[m];
      }
    }
    else
    {
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        Value el;
        el.type = v.type->inner;
        if(el.type->bitWidth <= 32)
          el.val.uv[0] = constant.ops[m] & ((1ULL << el.type->bitWidth) - 1);
        else
          el.val.u64v[m] = constant.ops[m];
        v.members.push_back(el);
      }
    }
    addValue(v);
  }
  else
  {
    RDCERR("Unknown record ID %u encountered in constants block", constant.id);
  }
}

bool Program::Valid(const byte *bytes, size_t length)
{
  if(length < sizeof(ProgramHeader))
    return false;

  const byte *ptr = bytes;
  const ProgramHeader *header = (const ProgramHeader *)ptr;
  if(header->DxilMagic != MAKE_FOURCC('D', 'X', 'I', 'L'))
    return false;

  size_t expected = offsetof(ProgramHeader, DxilMagic) + header->BitcodeOffset + header->BitcodeSize;

  if(expected != length)
    return false;

  return LLVMBC::BitcodeReader::Valid(
      ptr + offsetof(ProgramHeader, DxilMagic) + header->BitcodeOffset, header->BitcodeSize);
}

Program::Program(const byte *bytes, size_t length)
{
  const byte *ptr = bytes;
  const ProgramHeader *header = (const ProgramHeader *)ptr;
  RDCASSERT(header->DxilMagic == MAKE_FOURCC('D', 'X', 'I', 'L'));

  const byte *bitcode = ((const byte *)&header->DxilMagic) + header->BitcodeOffset;
  RDCASSERT(bitcode + header->BitcodeSize == ptr + length);

  LLVMBC::BitcodeReader reader(bitcode, header->BitcodeSize);

  LLVMBC::BlockOrRecord root = reader.ReadToplevelBlock();

  // the top-level block should be MODULE_BLOCK
  RDCASSERT(KnownBlocks(root.id) == KnownBlocks::MODULE_BLOCK);

  // we should have consumed all bits, only one top-level block
  RDCASSERT(reader.AtEndOfStream());

  m_Type = DXBC::ShaderType(header->ProgramType);
  m_Major = (header->ProgramVersion & 0xf0) >> 4;
  m_Minor = header->ProgramVersion & 0xf;

  // Input signature and Output signature haven't changed.
  // Pipeline Runtime Information we have decoded just not implemented here

  rdcstr datalayout, triple;

  rdcarray<size_t> functionDecls;

  for(const LLVMBC::BlockOrRecord &rootchild : root.children)
  {
    if(rootchild.IsRecord())
    {
      if(IS_KNOWN(rootchild.id, ModuleRecord::VERSION))
      {
        if(rootchild.ops[0] != 1)
        {
          RDCERR("Unsupported LLVM bitcode version %u", rootchild.ops[0]);
          break;
        }
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::TRIPLE))
      {
        m_Triple = rootchild.getString();
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::DATALAYOUT))
      {
        m_Datalayout = rootchild.getString();
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::GLOBALVAR))
      {
        // [pointer type, isconst, initid, linkage, alignment, section, visibility, threadlocal,
        // unnamed_addr, externally_initialized, dllstorageclass, comdat]
        GlobalVar g;

        g.type = &m_Types[(size_t)rootchild.ops[0]];
        g.isconst = (rootchild.ops[1] & 0x1);

        switch(rootchild.ops[3])
        {
          case 0:
          case 5:
          case 6:
          case 7:
          case 15: g.external = true; break;
          default: g.external = false; break;
        }

        g.align = (1U << rootchild.ops[4]) >> 1;

        // symbols refer into any of N types in declaration order
        m_Symbols.push_back({SymbolType::GlobalVar, m_GlobalVars.size()});

        // all global symbols are 'values' in LLVM, we don't need this but need to keep indexing the
        // same
        Value v;
        v.type = g.type;
        v.symbol = true;

        for(size_t ty = 0; ty < m_Types.size(); ty++)
        {
          if(m_Types[ty].type == Type::Pointer && m_Types[ty].inner == g.type)
          {
            v.type = &m_Types[ty];
            break;
          }
        }

        if(v.type == g.type)
          RDCERR("Expected to find pointer type for global variable");

        m_Values.push_back(v);
        m_GlobalVars.push_back(g);
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::FUNCTION))
      {
        // [type, callingconv, isproto, linkage, paramattrs, alignment, section, visibility, gc,
        // unnamed_addr]
        Function f;

        f.funcType = &m_Types[(size_t)rootchild.ops[0]];
        // ignore callingconv
        f.external = (rootchild.ops[2] != 0);
        // ignore linkage
        if(rootchild.ops[4] > 0 && rootchild.ops[4] - 1 < m_Attributes.size())
          f.attrs = &m_Attributes[(size_t)rootchild.ops[4] - 1];
        // ignore rest of properties

        // symbols refer into any of N types in declaration order
        m_Symbols.push_back({SymbolType::Function, m_Functions.size()});

        // all global symbols are 'values' in LLVM, we don't need this but need to keep indexing the
        // same
        Value v;
        v.symbol = true;
        v.type = f.funcType;

        for(size_t ty = 0; ty < m_Types.size(); ty++)
        {
          if(m_Types[ty].type == Type::Pointer && m_Types[ty].inner == f.funcType)
          {
            v.type = &m_Types[ty];
            break;
          }
        }

        if(v.type == f.funcType)
          RDCERR("Expected to find pointer type for function");

        m_Values.push_back(v);

        if(!f.external)
          functionDecls.push_back(m_Functions.size());

        m_Functions.push_back(f);
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::ALIAS))
      {
        // [alias value type, addrspace, aliasee val#, linkage, visibility]
        Alias a;

        // symbols refer into any of N types in declaration order
        m_Symbols.push_back({SymbolType::Alias, m_Aliases.size()});

        // all global symbols are 'values' in LLVM, we don't need this but need to keep indexing the
        // same
        Value v;
        v.type = &m_Types[(size_t)rootchild.ops[0]];
        v.symbol = true;
        m_Values.push_back(v);

        m_Aliases.push_back(a);
      }
      else
      {
        RDCERR("Unknown record ID %u encountered at module scope", rootchild.id);
      }
    }
    else if(rootchild.IsBlock())
    {
      if(IS_KNOWN(rootchild.id, KnownBlocks::BLOCKINFO))
      {
        // do nothing, this is internal parse data
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::PARAMATTR_GROUP_BLOCK))
      {
        for(const LLVMBC::BlockOrRecord &attrgroup : rootchild.children)
        {
          if(attrgroup.IsBlock())
          {
            RDCERR("Unexpected subblock in PARAMATTR_GROUP_BLOCK");
            continue;
          }

          if(!IS_KNOWN(attrgroup.id, ParamAttrGroupRecord::ENTRY))
          {
            RDCERR("Unexpected attribute group record ID %u", attrgroup.id);
            continue;
          }

          Attributes group;

          size_t id = (size_t)attrgroup.ops[0];
          group.index = attrgroup.ops[1];

          for(size_t i = 2; i < attrgroup.ops.size(); i++)
          {
            switch(attrgroup.ops[i])
            {
              case 0:
              {
                group.params |= Attribute(1U << (attrgroup.ops[i + 1]));
                i++;
                break;
              }
              case 1:
              {
                uint64_t param = attrgroup.ops[i + 2];
                Attribute attr = Attribute(1U << attrgroup.ops[i + 1]);
                group.params |= attr;
                switch(attr)
                {
                  case Attribute::Alignment: group.align = param; break;
                  case Attribute::StackAlignment: group.stackAlign = param; break;
                  case Attribute::Dereferenceable: group.derefBytes = param; break;
                  case Attribute::DereferenceableOrNull: group.derefOrNullBytes = param; break;
                  default: RDCERR("Unexpected attribute %llu with parameter", attr);
                }
                i += 2;
                break;
              }
              default:
              {
                rdcstr a = attrgroup.getString(i + 1);
                rdcstr b = attrgroup.getString(i + 1 + a.size() + 1);
                group.strs.push_back({a, b});
                break;
              }
            }
          }

          m_AttributeGroups.resize_for_index(id);
          m_AttributeGroups[id] = group;
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::PARAMATTR_BLOCK))
      {
        for(const LLVMBC::BlockOrRecord &paramattr : rootchild.children)
        {
          if(paramattr.IsBlock())
          {
            RDCERR("Unexpected subblock in PARAMATTR_BLOCK");
            continue;
          }

          if(!IS_KNOWN(paramattr.id, ParamAttrRecord::ENTRY))
          {
            RDCERR("Unexpected attribute record ID %u", paramattr.id);
            continue;
          }

          Attributes attrs;
          attrs.index = m_Attributes.size();

          for(uint64_t g : paramattr.ops)
          {
            if(g < m_AttributeGroups.size())
            {
              Attributes &other = m_AttributeGroups[(size_t)g];
              attrs.params |= other.params;
              attrs.align = RDCMAX(attrs.align, other.align);
              attrs.stackAlign = RDCMAX(attrs.stackAlign, other.stackAlign);
              attrs.derefBytes = RDCMAX(attrs.derefBytes, other.derefBytes);
              attrs.derefOrNullBytes = RDCMAX(attrs.derefOrNullBytes, other.derefOrNullBytes);
              attrs.strs.append(other.strs);
            }
            else
            {
              RDCERR("Attribute refers to out of bounds group %llu", g);
            }
          }

          m_Attributes.push_back(attrs);
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::TYPE_BLOCK))
      {
        rdcstr structname;

        if(!rootchild.children.empty() && !IS_KNOWN(rootchild.children[0].id, TypeRecord::NUMENTRY))
        {
          RDCWARN("No NUMENTRY record, resizing conservatively to number of records");
          m_Types.resize(rootchild.children.size());
        }

        size_t typeIndex = 0;
        for(const LLVMBC::BlockOrRecord &typ : rootchild.children)
        {
          if(typ.IsBlock())
          {
            RDCERR("Unexpected subblock in TYPE_BLOCK");
            continue;
          }

          if(IS_KNOWN(typ.id, TypeRecord::NUMENTRY))
          {
            RDCASSERT(m_Types.size() < (size_t)typ.ops[0], m_Types.size(), typ.ops[0]);
            m_Types.resize((size_t)typ.ops[0]);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::VOID))
          {
            m_Types[typeIndex].type = Type::Scalar;
            m_Types[typeIndex].scalarType = Type::Void;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::LABEL))
          {
            m_Types[typeIndex].type = Type::Label;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::METADATA))
          {
            m_Types[typeIndex].type = Type::Metadata;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::HALF))
          {
            m_Types[typeIndex].type = Type::Scalar;
            m_Types[typeIndex].scalarType = Type::Float;
            m_Types[typeIndex].bitWidth = 16;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::FLOAT))
          {
            m_Types[typeIndex].type = Type::Scalar;
            m_Types[typeIndex].scalarType = Type::Float;
            m_Types[typeIndex].bitWidth = 32;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::DOUBLE))
          {
            m_Types[typeIndex].type = Type::Scalar;
            m_Types[typeIndex].scalarType = Type::Float;
            m_Types[typeIndex].bitWidth = 64;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::INTEGER))
          {
            m_Types[typeIndex].type = Type::Scalar;
            m_Types[typeIndex].scalarType = Type::Int;
            m_Types[typeIndex].bitWidth = typ.ops[0] & 0xffffffff;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::VECTOR))
          {
            m_Types[typeIndex].type = Type::Vector;
            m_Types[typeIndex].elemCount = typ.ops[0] & 0xffffffff;
            m_Types[typeIndex].inner = &m_Types[(size_t)typ.ops[1]];

            // copy properties out of the inner for convenience
            m_Types[typeIndex].scalarType = m_Types[typeIndex].inner->scalarType;
            m_Types[typeIndex].bitWidth = m_Types[typeIndex].inner->bitWidth;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::ARRAY))
          {
            m_Types[typeIndex].type = Type::Array;
            m_Types[typeIndex].elemCount = typ.ops[0] & 0xffffffff;
            m_Types[typeIndex].inner = &m_Types[(size_t)typ.ops[1]];

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::POINTER))
          {
            m_Types[typeIndex].type = Type::Pointer;
            m_Types[typeIndex].inner = &m_Types[(size_t)typ.ops[0]];

            if(typ.ops.size() > 1 && typ.ops[1] != 0)
              RDCERR("Ignoring address space on pointer type");

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::OPAQUE))
          {
            // pretend opaque types are empty structs
            m_Types[typeIndex].type = Type::Struct;

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::STRUCT_NAME))
          {
            structname = typ.getString(0);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::STRUCT_ANON) ||
                  IS_KNOWN(typ.id, TypeRecord::STRUCT_NAMED))
          {
            m_Types[typeIndex].type = Type::Struct;
            m_Types[typeIndex].packedStruct = (typ.ops[0] != 0);

            for(size_t o = 1; o < typ.ops.size(); o++)
              m_Types[typeIndex].members.push_back(&m_Types[(size_t)typ.ops[o]]);

            if(IS_KNOWN(typ.id, TypeRecord::STRUCT_NAMED))
            {
              // may we want a reverse map name -> type? probably not, this is only relevant for
              // disassembly or linking and disassembly we can do just by iterating all types
              m_Types[typeIndex].name = structname;
              structname.clear();
            }

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::FUNCTION_OLD) ||
                  IS_KNOWN(typ.id, TypeRecord::FUNCTION))
          {
            m_Types[typeIndex].type = Type::Function;

            m_Types[typeIndex].vararg = (typ.ops[0] != 0);

            size_t o = 1;

            // skip attrid
            if(IS_KNOWN(typ.id, TypeRecord::FUNCTION_OLD))
              o++;

            // return type
            m_Types[typeIndex].inner = &m_Types[(size_t)typ.ops[o]];
            o++;

            for(; o < typ.ops.size(); o++)
              m_Types[typeIndex].members.push_back(&m_Types[(size_t)typ.ops[o]]);

            typeIndex++;
          }
          else
          {
            RDCERR("Unknown record ID %u encountered in type block", typ.id);
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::CONSTANTS_BLOCK))
      {
        const Type *t = NULL;
        for(const LLVMBC::BlockOrRecord &constant : rootchild.children)
        {
          if(constant.IsBlock())
          {
            RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
            continue;
          }

          ParseConstant(constant, t, [this](uint64_t op) { return &m_Types[(size_t)op]; },
                        [this](uint64_t v) {
                          size_t idx = (size_t)v;
                          return idx < m_Values.size() ? &m_Values[idx] : NULL;
                        },
                        [this](const Value &v) {
                          m_Symbols.push_back({SymbolType::Constant, m_Values.size()});
                          m_Values.push_back(v);
                        });
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::VALUE_SYMTAB_BLOCK))
      {
        for(const LLVMBC::BlockOrRecord &symtab : rootchild.children)
        {
          if(symtab.IsBlock())
          {
            RDCERR("Unexpected subblock in VALUE_SYMTAB_BLOCK");
            continue;
          }

          if(!IS_KNOWN(symtab.id, ValueSymtabRecord::ENTRY))
          {
            RDCERR("Unexpected symbol table record ID %u", symtab.id);
            continue;
          }

          size_t s = (size_t)symtab.ops[0];
          if(s < m_Symbols.size())
          {
            size_t idx = m_Symbols[s].idx;
            switch(m_Symbols[s].type)
            {
              case SymbolType::Unknown:
              case SymbolType::Constant:
              case SymbolType::Argument:
              case SymbolType::Instruction:
              case SymbolType::Metadata:
              case SymbolType::Literal:
                RDCERR("Unexpected global symbol referring to %d", m_Symbols[s].type);
                break;
              case SymbolType::GlobalVar:
                m_Values[s].str = m_GlobalVars[idx].name = symtab.getString(1);
                break;
              case SymbolType::Function:
                m_Values[s].str = m_Functions[idx].name = symtab.getString(1);
                break;
              case SymbolType::Alias:
                m_Values[s].str = m_Aliases[idx].name = symtab.getString(1);
                break;
            }
          }
          else
          {
            RDCERR("Symbol %llu referenced out of bounds", s);
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::METADATA_BLOCK))
      {
        m_Metadata.reserve(rootchild.children.size());
        for(size_t i = 0; i < rootchild.children.size(); i++)
        {
          const LLVMBC::BlockOrRecord &metaRecord = rootchild.children[i];

          if(metaRecord.IsBlock())
          {
            RDCERR("Unexpected subblock in METADATA_BLOCK");
            continue;
          }

          if(IS_KNOWN(metaRecord.id, MetaDataRecord::NAME))
          {
            NamedMetadata meta;

            meta.name = metaRecord.getString();
            i++;
            const LLVMBC::BlockOrRecord &namedNode = rootchild.children[i];
            RDCASSERT(IS_KNOWN(namedNode.id, MetaDataRecord::NAMED_NODE));

            for(uint64_t op : namedNode.ops)
              meta.children.push_back(&m_Metadata[(size_t)op]);

            m_NamedMeta.push_back(meta);
          }
          else if(IS_KNOWN(metaRecord.id, MetaDataRecord::KIND))
          {
            size_t kind = (size_t)metaRecord.ops[0];
            m_Kinds.resize(RDCMAX(m_Kinds.size(), kind + 1));
            m_Kinds[kind] = metaRecord.getString(1);
            continue;
          }
          else
          {
            m_Metadata.resize_for_index(i);
            Metadata &meta = m_Metadata[i];

            auto getMetaOrNull = [this](uint64_t id) {
              return id ? &m_Metadata[size_t(id - 1)] : NULL;
            };
            auto getMetaStringOrNull = [this](uint64_t id) {
              return id ? &m_Metadata[size_t(id - 1)].str : NULL;
            };

            if(IS_KNOWN(metaRecord.id, MetaDataRecord::STRING_OLD))
            {
              meta.value = true;
              meta.str = metaRecord.getString();
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::VALUE))
            {
              meta.value = true;
              meta.val = &m_Values[(size_t)metaRecord.ops[1]];
              meta.type = &m_Types[(size_t)metaRecord.ops[0]];
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::NODE) ||
                    IS_KNOWN(metaRecord.id, MetaDataRecord::DISTINCT_NODE))
            {
              if(IS_KNOWN(metaRecord.id, MetaDataRecord::DISTINCT_NODE))
                meta.distinct = true;

              for(uint64_t op : metaRecord.ops)
                meta.children.push_back(getMetaOrNull(op));
            }
            else
            {
              bool parsed = ParseDebugMetaRecord(metaRecord, meta);
              if(!parsed)
              {
                RDCERR("unhandled metadata type %u", metaRecord.id);
              }
            }
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::FUNCTION_BLOCK))
      {
        Function &f = m_Functions[functionDecls[0]];
        functionDecls.erase(0);

        auto getValue = [this, &f](uint64_t v) { return GetFunctionValue(f, v); };
        auto getMetaOrNull = [this, &f](uint64_t v) {
          size_t idx = (size_t)v;
          return idx == 0 ? NULL : (idx - 1 < m_Metadata.size() ? &m_Metadata[idx - 1]
                                                                : &f.metadata[idx - 1]);
        };

        size_t prevNumSymbols = m_Symbols.size();
        size_t instrSymbolStart = 0;

        for(size_t i = 0; i < f.funcType->members.size(); i++)
        {
          Instruction arg;
          arg.type = f.funcType->members[i];
          arg.name = StringFormat::Fmt("arg%zu", i);
          f.args.push_back(arg);
          m_Symbols.push_back({SymbolType::Argument, i});
        }

        auto getSymbol = [this](uint64_t id) {
          id = uint32_t((uint64_t)m_Symbols.size() - id);
          return id < m_Symbols.size() ? m_Symbols[id] : Symbol(SymbolType::Unknown, id);
        };

        Block *curBlock = NULL;
        int32_t debugLocIndex = -1;

        for(const LLVMBC::BlockOrRecord &funcChild : rootchild.children)
        {
          if(funcChild.IsBlock())
          {
            if(IS_KNOWN(funcChild.id, KnownBlocks::CONSTANTS_BLOCK))
            {
              f.values.reserve(funcChild.children.size());

              const Type *t = NULL;
              for(const LLVMBC::BlockOrRecord &constant : funcChild.children)
              {
                if(constant.IsBlock())
                {
                  RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
                  continue;
                }

                ParseConstant(
                    constant, t, [this](uint64_t op) { return &m_Types[(size_t)op]; }, getValue,
                    [this, &f](const Value &v) {
                      m_Symbols.push_back({SymbolType::Constant, m_Values.size() + f.values.size()});
                      f.values.push_back(v);
                    });
              }

              instrSymbolStart = m_Symbols.size();
            }
            else if(IS_KNOWN(funcChild.id, KnownBlocks::METADATA_BLOCK))
            {
              f.metadata.resize(funcChild.children.size());

              size_t m = 0;

              for(const LLVMBC::BlockOrRecord &metaRecord : funcChild.children)
              {
                if(metaRecord.IsBlock())
                {
                  RDCERR("Unexpected subblock in function METADATA_BLOCK");
                  continue;
                }

                Metadata &meta = f.metadata[m];

                if(IS_KNOWN(metaRecord.id, MetaDataRecord::VALUE))
                {
                  meta.value = true;
                  size_t idx = metaRecord.ops[1];
                  if(idx < m_Values.size())
                  {
                    // global value reference
                    meta.val = &m_Values[idx];
                  }
                  else
                  {
                    idx -= m_Values.size();
                    if(idx < f.values.size())
                    {
                      // function-local value reference
                      meta.val = &f.values[idx];
                    }
                    else
                    {
                      // forward reference to instruction
                      meta.func = &f;
                      meta.instruction = idx - f.values.size();
                    }
                  }
                  meta.type = &m_Types[(size_t)metaRecord.ops[0]];
                }
                else
                {
                  RDCERR("Unexpected record %u in function METADATA_BLOCK", metaRecord.id);
                }

                m++;
              }
            }
            else if(IS_KNOWN(funcChild.id, KnownBlocks::VALUE_SYMTAB_BLOCK))
            {
              for(const LLVMBC::BlockOrRecord &symtab : funcChild.children)
              {
                if(symtab.IsBlock())
                {
                  RDCERR("Unexpected subblock in VALUE_SYMTAB_BLOCK");
                  continue;
                }

                if(symtab.id != 1)
                {
                  RDCERR("Unexpected symbol table record ID %u", symtab.id);
                  continue;
                }

                size_t idx = (size_t)symtab.ops[0];

                if(idx >= m_Symbols.size())
                {
                  RDCERR("Out of bounds symbol index %zu (%s) in function symbol table", idx,
                         symtab.getString(1).c_str());
                  continue;
                }

                Symbol s = m_Symbols[idx];

                switch(s.type)
                {
                  case SymbolType::Unknown:
                  case SymbolType::Constant:
                    if(s.idx < m_Values.size())
                      RDCERR("Unexpected local symbol referring to global value");
                    else
                      f.values[s.idx - m_Values.size()].str = symtab.getString(1);
                    break;
                  case SymbolType::Argument: f.args[s.idx].name = symtab.getString(1); break;
                  case SymbolType::Instruction:
                    f.instructions[s.idx].name = symtab.getString(1);
                    break;
                  case SymbolType::GlobalVar:
                  case SymbolType::Function:
                  case SymbolType::Alias:
                  case SymbolType::Metadata:
                  case SymbolType::Literal:
                    RDCERR("Unexpected local symbol referring to %d", s.type);
                    break;
                }
              }
            }
            else
            {
              RDCERR("Unexpected subblock %u in FUNCTION_BLOCK", funcChild.id);
              continue;
            }
          }
          else
          {
            const LLVMBC::BlockOrRecord &op = funcChild;
            if(IS_KNOWN(op.id, FunctionRecord::DECLAREBLOCKS))
            {
              f.blocks.resize(op.ops[0]);

              curBlock = &f.blocks[0];
            }
            else if(IS_KNOWN(op.id, FunctionRecord::DEBUG_LOC))
            {
              DebugLocation debugLoc;
              debugLoc.line = op.ops[0];
              debugLoc.col = op.ops[1];
              debugLoc.scope = getMetaOrNull(op.ops[2]);
              debugLoc.inlinedAt = getMetaOrNull(op.ops[3]);

              debugLocIndex = m_DebugLocations.indexOf(debugLoc);

              if(debugLocIndex < 0)
              {
                m_DebugLocations.push_back(debugLoc);
                debugLocIndex = int32_t(m_DebugLocations.size() - 1);
              }

              f.instructions.back().debugLoc = (uint32_t)debugLocIndex;
            }
            else if(IS_KNOWN(op.id, FunctionRecord::DEBUG_LOC_AGAIN))
            {
              f.instructions.back().debugLoc = (uint32_t)debugLocIndex;
            }
            else if(IS_KNOWN(op.id, FunctionRecord::INST_CALL))
            {
              size_t n = 0;
              Instruction inst;
              inst.op = Instruction::Call;
              inst.paramAttrs = &m_Attributes[op.ops[n++]];

              uint64_t callingFlags = op.ops[n++];

              uint64_t fastMathFlags = 0;
              if(callingFlags & (1ULL << 17))
                fastMathFlags = op.ops[n++];

              if(callingFlags & (1ULL << 15))
                n++;    // funcCallType

              Symbol s = getSymbol(op.ops[n++]);

              if(s.type != SymbolType::Function)
              {
                RDCERR("Unexpected symbol type %d called in INST_CALL", s.type);
                continue;
              }

              inst.funcCall = &m_Functions[s.idx];
              inst.type = inst.funcCall->funcType->inner;

              for(size_t i = 0; n < op.ops.size(); n++, i++)
              {
                s = getSymbol(op.ops[n]);
                if(inst.funcCall->funcType->members[i]->type == Type::Metadata)
                  s.type = SymbolType::Metadata;
                inst.args.push_back(s);
              }

              RDCASSERTEQUAL(inst.args.size(), inst.funcCall->funcType->members.size());

              if(!inst.type->isVoid())
                m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(IS_KNOWN(op.id, FunctionRecord::INST_CAST))
            {
              Instruction inst;

              inst.args.push_back(getSymbol(op.ops[0]));
              inst.type = &m_Types[op.ops[1]];

              switch(op.ops[2])
              {
                case 0: inst.op = Instruction::Trunc; break;
                case 1: inst.op = Instruction::ZExt; break;
                case 2: inst.op = Instruction::SExt; break;
                case 3: inst.op = Instruction::FToU; break;
                case 4: inst.op = Instruction::FToS; break;
                case 5: inst.op = Instruction::UToF; break;
                case 6: inst.op = Instruction::SToF; break;
                case 7: inst.op = Instruction::FPTrunc; break;
                case 8: inst.op = Instruction::FPExt; break;
                case 9: inst.op = Instruction::PtrToI; break;
                case 10: inst.op = Instruction::IToPtr; break;
                case 11: inst.op = Instruction::Bitcast; break;
                case 12: inst.op = Instruction::AddrSpaceCast; break;
                default: RDCERR("Unhandled cast type %d", op.ops[2]);
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(IS_KNOWN(op.id, FunctionRecord::INST_EXTRACTVAL))
            {
              Instruction inst;

              inst.op = Instruction::ExtractVal;

              inst.args.push_back(getSymbol(op.ops[0]));
              inst.type = GetSymbolType(f, inst.args.back());
              for(size_t n = 1; n < op.ops.size(); n++)
              {
                if(inst.type->type == Type::Array)
                  inst.type = inst.type->inner;
                else
                  inst.type = inst.type->members[op.ops[n]];
                inst.args.push_back({SymbolType::Literal, op.ops[n]});
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(IS_KNOWN(op.id, FunctionRecord::INST_RET))
            {
              Instruction inst;

              inst.op = Instruction::Ret;

              if(op.ops.empty())
              {
                for(size_t i = 0; i < m_Types.size(); i++)
                {
                  if(m_Types[i].isVoid())
                  {
                    inst.type = &m_Types[i];
                    break;
                  }
                }

                RDCASSERT(inst.type);
              }
              else
              {
                inst.args.push_back(getSymbol(op.ops[0]));
                inst.type = GetSymbolType(f, inst.args.back());

                m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});
              }

              f.instructions.push_back(inst);
            }
            else
            {
              RDCERR("Unexpected record in FUNCTION_BLOCK");
              continue;
            }
          }
        }

        for(size_t i = 0, resultID = 1; i < f.instructions.size(); i++)
        {
          if(f.instructions[i].type->isVoid())
            continue;

          if(!f.instructions[i].name.empty())
            continue;

          f.instructions[i].resultID = (uint32_t)resultID++;
        }

        // rebase metadata, we get indices that skip void results, so look up the Symbols directory
        // to get to a normal instruction index
        for(Metadata &m : f.metadata)
          if(m.func)
            m.instruction = m_Symbols[instrSymbolStart + m.instruction].idx;

        m_Symbols.resize(prevNumSymbols);
      }
      else
      {
        RDCERR("Unknown block ID %u encountered at module scope", rootchild.id);
      }
    }
  }

  RDCASSERT(functionDecls.empty());
}

void Program::FetchComputeProperties(DXBC::Reflection *reflection)
{
  RDCERR("Unimplemented DXIL::Program::FetchComputeProperties()");
  reflection->DispatchThreadsDimension[0] = 1;
  reflection->DispatchThreadsDimension[1] = 1;
  reflection->DispatchThreadsDimension[2] = 1;
}

DXBC::Reflection *Program::GetReflection()
{
  RDCWARN("Unimplemented DXIL::Program::GetReflection()");
  return new DXBC::Reflection;
}

D3D_PRIMITIVE_TOPOLOGY Program::GetOutputTopology()
{
  RDCERR("Unimplemented DXIL::Program::GetOutputTopology()");
  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

uint32_t Program::GetOrAssignMetaID(Metadata *m)
{
  if(m->id != ~0U)
    return m->id;

  m->id = m_NextMetaID++;
  m_NumberedMeta.push_back(m);

  // assign meta IDs to the children now
  for(Metadata *c : m->children)
  {
    if(!c || c->value)
      continue;

    GetOrAssignMetaID(c);
  }

  return m->id;
}

uint32_t Program::GetOrAssignMetaID(DebugLocation &l)
{
  if(l.id != ~0U)
    return l.id;

  l.id = m_NextMetaID++;

  return l.id;
}

const Type *Program::GetSymbolType(const Function &f, Symbol s)
{
  const Type *ret = NULL;
  switch(s.type)
  {
    case SymbolType::Constant:
      if(s.idx < m_Values.size())
        ret = m_Values[s.idx].type;
      else
        ret = f.values[s.idx - m_Values.size()].type;
      break;
    case SymbolType::Argument: ret = f.funcType->members[s.idx]; break;
    case SymbolType::Instruction: ret = f.instructions[s.idx].type; break;
    case SymbolType::GlobalVar: ret = m_GlobalVars[s.idx].type; break;
    case SymbolType::Function: ret = m_Functions[s.idx].funcType; break;
    case SymbolType::Unknown:
    case SymbolType::Alias:
    case SymbolType::Metadata:
    case SymbolType::Literal: RDCERR("Unexpected symbol to get type for %d", s.type); break;
  }
  return ret;
}

const Value *Program::GetFunctionValue(const Function &f, uint64_t v)
{
  size_t idx = (size_t)v;
  return idx < m_Values.size() ? &m_Values[idx] : &f.values[idx - m_Values.size()];
}

const Metadata *Program::GetFunctionMetadata(const Function &f, uint64_t v)
{
  size_t idx = (size_t)v;
  return idx < m_Metadata.size() ? &m_Metadata[idx] : &f.metadata[idx - m_Metadata.size()];
}

Metadata::~Metadata()
{
  SAFE_DELETE(dwarf);
}

};    // namespace DXIL
