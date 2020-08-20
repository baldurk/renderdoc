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
  SECTIONNAME = 5,
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
  CSTRING = 9,
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

static Operation DecodeCast(uint64_t opcode)
{
  switch(opcode)
  {
    case 0: return Operation::Trunc; break;
    case 1: return Operation::ZExt; break;
    case 2: return Operation::SExt; break;
    case 3: return Operation::FToU; break;
    case 4: return Operation::FToS; break;
    case 5: return Operation::UToF; break;
    case 6: return Operation::SToF; break;
    case 7: return Operation::FPTrunc; break;
    case 8: return Operation::FPExt; break;
    case 9: return Operation::PtrToI; break;
    case 10: return Operation::IToPtr; break;
    case 11: return Operation::Bitcast; break;
    case 12: return Operation::AddrSpaceCast; break;
    default: RDCERR("Unhandled cast type %llu", opcode); return Operation::Bitcast;
  }
}

void ParseConstant(const LLVMBC::BlockOrRecord &constant, const Type *&curType,
                   std::function<const Type *(uint64_t)> getType,
                   std::function<const Type *(const Type *)> getPtrType,
                   std::function<const Constant *(uint64_t)> getConstant,
                   std::function<void(const Constant &)> addConstant)
{
  if(IS_KNOWN(constant.id, ConstantsRecord::SETTYPE))
  {
    curType = getType(constant.ops[0]);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL) ||
          IS_KNOWN(constant.id, ConstantsRecord::UNDEF))
  {
    Constant v;
    v.type = curType;
    v.nullconst = IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL);
    v.undef = IS_KNOWN(constant.id, ConstantsRecord::UNDEF);
    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::INTEGER))
  {
    Constant v;
    v.type = curType;
    v.val.s64v[0] = LLVMBC::BitReader::svbr(constant.ops[0]);
    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::FLOAT))
  {
    Constant v;
    v.type = curType;
    if(curType->bitWidth == 16)
      v.val.fv[0] = ConvertFromHalf(uint16_t(constant.ops[0] & 0xffff));
    else if(curType->bitWidth == 32)
      memcpy(&v.val.fv[0], &constant.ops[0], sizeof(float));
    else
      memcpy(&v.val.dv[0], &constant.ops[0], sizeof(double));
    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::STRING) ||
          IS_KNOWN(constant.id, ConstantsRecord::CSTRING))
  {
    Constant v;
    v.type = curType;
    v.str = constant.getString(0);
    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_CAST))
  {
    Constant v;
    v.op = DecodeCast(constant.ops[0]);
    v.type = curType;
    // getType(constant.ops[1]); type of the constant, which we ignore
    v.inner = getConstant(constant.ops[2]);
    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_GEP))
  {
    Constant v;

    v.op = Operation::GetElementPtr;

    size_t idx = 0;
    if(constant.ops.size() & 1)
      v.type = getType(constant.ops[idx++]);

    for(; idx < constant.ops.size(); idx += 2)
    {
      const Type *t = getType(constant.ops[idx]);
      const Constant *a = getConstant(constant.ops[idx + 1]);
      RDCASSERT(t == a->type);

      v.members.push_back(*a);
    }

    if(!v.type)
      v.type = v.members[0].type;

    // walk the type list to get the return type
    for(idx = 2; idx < v.members.size(); idx++)
    {
      if(v.type->type == Type::Vector || v.type->type == Type::Array)
      {
        v.type = v.type->inner;
      }
      else if(v.type->type == Type::Struct)
      {
        v.type = v.type->members[v.members[idx].val.uv[0]];
      }
      else
      {
        RDCERR("Unexpected type %d encountered in GEP", v.type->type);
      }
    }

    // the result is a pointer to the return type
    v.type = getPtrType(v.type);

    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::AGGREGATE))
  {
    Constant v;
    v.type = curType;
    if(v.type->type == Type::Vector)
    {
      // inline vectors
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        const Constant *member = getConstant(constant.ops[m]);

        if(member)
        {
          if(v.type->bitWidth <= 32)
            v.val.uv[m] = member->val.uv[m];
          else
            v.val.u64v[m] = member->val.u64v[m];
        }
        else
        {
          RDCERR("Index %llu out of bounds for constants array", constant.ops[m]);
        }
      }
    }
    else
    {
      for(uint64_t m : constant.ops)
      {
        const Constant *member = getConstant(m);

        if(member)
        {
          v.members.push_back(*member);
        }
        else
        {
          v.members.push_back(Constant());
          RDCERR("Index %llu out of bounds for constants array", m);
        }
      }
    }
    addConstant(v);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::DATA))
  {
    Constant v;
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
        Constant el;
        el.type = v.type->inner;
        if(el.type->bitWidth <= 32)
          el.val.uv[0] = constant.ops[m] & ((1ULL << el.type->bitWidth) - 1);
        else
          el.val.u64v[m] = constant.ops[m];
        v.members.push_back(el);
      }
    }
    addConstant(v);
  }
  else
  {
    RDCERR("Unknown record ID %u encountered in constants block", constant.id);
  }
}

// helper struct for reading ops
struct OpReader
{
  OpReader(Program *prog, const LLVMBC::BlockOrRecord &op)
      : prog(prog), type((FunctionRecord)op.id), values(op.ops), idx(0)
  {
  }

  FunctionRecord type;

  size_t remaining() { return values.size() - idx; }
  Symbol getSymbol(uint64_t val) { return prog->m_Symbols[prog->m_Symbols.size() - (size_t)val]; }
  Symbol getSymbol(bool withType = true)
  {
    // get the value
    uint64_t val = get<uint64_t>();

    // if it's not a forward reference, resolve the relative-ness and return
    if(val <= prog->m_Symbols.size())
    {
      return getSymbol(val);
    }
    else
    {
      // sometimes forward references have types, which we store here in case we need the type
      // later.
      if(withType)
        m_LastType = getType();

      // return the forward reference symbol
      return Symbol(SymbolType::Unknown, prog->m_Symbols.size() - (int32_t)val);
    }
  }

  // some symbols are referenced absolute, not relative
  Symbol getSymbolAbsolute() { return prog->m_Symbols[get<size_t>()]; }
  const Type *getType() { return &prog->m_Types[get<size_t>()]; }
  const Type *getType(const Function &f, Symbol s)
  {
    if(s.type == SymbolType::Unknown)
      return m_LastType;
    return prog->GetSymbolType(f, s);
  }

  template <typename T>
  T get()
  {
    return (T)values[idx++];
  }

private:
  const rdcarray<uint64_t> &values;
  size_t idx;
  Program *prog;
  const Type *m_LastType = NULL;
};

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
  RDCASSERT(bitcode + header->BitcodeSize <= ptr + length);

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
        if(rootchild.ops[1] & 0x1)
          g.flags |= GlobalFlags::IsConst;

        if(rootchild.ops.size() > 8)
        {
          if(rootchild.ops[8] == 1)
            g.flags |= GlobalFlags::GlobalUnnamedAddr;
          else if(rootchild.ops[8] == 2)
            g.flags |= GlobalFlags::LocalUnnamedAddr;
        }

        if(rootchild.ops[2])
          g.initialiser = Symbol(SymbolType::Constant, rootchild.ops[2] - 1);

        switch(rootchild.ops[3])
        {
          case 0:
          case 5:
          case 6:
          case 7:
          case 15: g.flags |= GlobalFlags::IsExternal; break;
          case 2: g.flags |= GlobalFlags::IsAppending; break;
          default: break;
        }

        g.align = (1ULL << rootchild.ops[4]) >> 1;

        g.section = int32_t(rootchild.ops[5]) - 1;

        // symbols refer into any of N types in declaration order
        m_Symbols.push_back({SymbolType::GlobalVar, m_GlobalVars.size()});

        // all global symbols are 'values' in LLVM, we don't need this but need to keep indexing the
        // same
        Constant v;
        v.symbol = true;

        v.type = GetPointerType(g.type);

        if(v.type == g.type)
          RDCERR("Expected to find pointer type for global variable");

        g.type = v.type;

        m_Constants.push_back(v);
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
        Constant v;
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

        m_Constants.push_back(v);

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
        Constant v;
        v.type = &m_Types[(size_t)rootchild.ops[0]];
        v.symbol = true;
        m_Constants.push_back(v);

        m_Aliases.push_back(a);
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::SECTIONNAME))
      {
        m_Sections.push_back(rootchild.getString(0));
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
                group.params |= Attribute(1ULL << (attrgroup.ops[i + 1]));
                i++;
                break;
              }
              case 1:
              {
                uint64_t param = attrgroup.ops[i + 2];
                Attribute attr = Attribute(1ULL << attrgroup.ops[i + 1]);
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
            m_Types[typeIndex].addrSpace = Type::PointerAddrSpace(typ.ops[1]);

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
                        [this](const Type *t) { return GetPointerType(t); },
                        [this](uint64_t v) {
                          size_t idx = (size_t)v;
                          return idx < m_Constants.size() ? &m_Constants[idx] : NULL;
                        },
                        [this](const Constant &v) {
                          m_Symbols.push_back({SymbolType::Constant, m_Constants.size()});
                          m_Constants.push_back(v);
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
            size_t idx = (size_t)m_Symbols[s].idx;
            switch(m_Symbols[s].type)
            {
              case SymbolType::Unknown:
              case SymbolType::Constant:
              case SymbolType::Argument:
              case SymbolType::Instruction:
              case SymbolType::Metadata:
              case SymbolType::Literal:
              case SymbolType::BasicBlock:
                RDCERR("Unexpected global symbol referring to %d", m_Symbols[s].type);
                break;
              case SymbolType::GlobalVar:
                m_Constants[s].str = m_GlobalVars[idx].name = symtab.getString(1);
                break;
              case SymbolType::Function:
                m_Constants[s].str = m_Functions[idx].name = symtab.getString(1);
                break;
              case SymbolType::Alias:
                m_Constants[s].str = m_Aliases[idx].name = symtab.getString(1);
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
              meta.isConstant = true;
              meta.str = metaRecord.getString();
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::VALUE))
            {
              meta.isConstant = true;
              meta.constant = &m_Constants[(size_t)metaRecord.ops[1]];
              meta.type = &m_Types[(size_t)metaRecord.ops[0]];
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::NODE) ||
                    IS_KNOWN(metaRecord.id, MetaDataRecord::DISTINCT_NODE))
            {
              if(IS_KNOWN(metaRecord.id, MetaDataRecord::DISTINCT_NODE))
                meta.isDistinct = true;

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

        auto getConstant = [this, &f](uint64_t v) { return GetFunctionConstant(f, v); };
        auto getMeta = [this, &f](uint64_t v) {
          size_t idx = (size_t)v;
          return idx - 1 < m_Metadata.size() ? &m_Metadata[idx] : &f.metadata[idx];
        };
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

        size_t curBlock = 0;
        int32_t debugLocIndex = -1;

        for(const LLVMBC::BlockOrRecord &funcChild : rootchild.children)
        {
          if(funcChild.IsBlock())
          {
            if(IS_KNOWN(funcChild.id, KnownBlocks::CONSTANTS_BLOCK))
            {
              f.constants.reserve(funcChild.children.size());

              const Type *t = NULL;
              for(const LLVMBC::BlockOrRecord &constant : funcChild.children)
              {
                if(constant.IsBlock())
                {
                  RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
                  continue;
                }

                ParseConstant(constant, t, [this](uint64_t op) { return &m_Types[(size_t)op]; },
                              [this](const Type *t) { return GetPointerType(t); }, getConstant,
                              [this, &f](const Constant &v) {
                                m_Symbols.push_back({SymbolType::Constant,
                                                     m_Constants.size() + f.constants.size()});
                                f.constants.push_back(v);
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
                  meta.isConstant = true;
                  size_t idx = (size_t)metaRecord.ops[1];
                  if(idx < m_Constants.size())
                  {
                    // global constant reference
                    meta.constant = &m_Constants[idx];
                  }
                  else
                  {
                    idx -= m_Constants.size();
                    if(idx < f.constants.size())
                    {
                      // function-local constant reference
                      meta.constant = &f.constants[idx];
                    }
                    else
                    {
                      // forward reference to instruction
                      meta.func = &f;
                      meta.instruction = idx - f.constants.size();
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
                  RDCERR("Unexpected subblock in function VALUE_SYMTAB_BLOCK");
                  continue;
                }

                if(IS_KNOWN(symtab.id, ValueSymtabRecord::ENTRY))
                {
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
                      if(s.idx < m_Constants.size())
                        RDCERR("Unexpected local symbol referring to global value");
                      else
                        f.constants[(size_t)s.idx - m_Constants.size()].str = symtab.getString(1);
                      break;
                    case SymbolType::Argument:
                      f.args[(size_t)s.idx].name = symtab.getString(1);
                      break;
                    case SymbolType::Instruction:
                      f.instructions[(size_t)s.idx].name = symtab.getString(1);
                      break;
                    case SymbolType::BasicBlock:
                      f.blocks[(size_t)s.idx].name = symtab.getString(1);
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
                else if(IS_KNOWN(symtab.id, ValueSymtabRecord::BBENTRY))
                {
                  f.blocks[(size_t)symtab.ops[0]].name = symtab.getString(1);
                }
                else
                {
                  RDCERR("Unexpected function symbol table record ID %u", symtab.id);
                  continue;
                }
              }
            }
            else if(IS_KNOWN(funcChild.id, KnownBlocks::METADATA_ATTACHMENT))
            {
              for(const LLVMBC::BlockOrRecord &meta : funcChild.children)
              {
                if(meta.IsBlock())
                {
                  RDCERR("Unexpected subblock in METADATA_ATTACHMENT");
                  continue;
                }

                if(!IS_KNOWN(meta.id, MetaDataRecord::ATTACHMENT))
                {
                  RDCERR("Unexpected record %u in METADATA_ATTACHMENT", meta.id);
                  continue;
                }

                size_t idx = 0;

                rdcarray<rdcpair<uint64_t, Metadata *>> attach;

                if(meta.ops.size() % 2 != 0)
                  idx++;

                for(; idx < meta.ops.size(); idx += 2)
                  attach.push_back(make_rdcpair(meta.ops[idx], getMeta(meta.ops[idx + 1])));

                if(meta.ops.size() % 2 == 0)
                  f.attachedMeta.swap(attach);
                else
                  f.instructions[(size_t)meta.ops[0]].attachedMeta.swap(attach);
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
            OpReader op(this, funcChild);

            if(op.type == FunctionRecord::DECLAREBLOCKS)
            {
              f.blocks.resize(op.get<size_t>());

              curBlock = 0;
            }
            else if(op.type == FunctionRecord::DEBUG_LOC)
            {
              DebugLocation debugLoc;
              debugLoc.line = op.get<uint64_t>();
              debugLoc.col = op.get<uint64_t>();
              debugLoc.scope = getMetaOrNull(op.get<uint64_t>());
              debugLoc.inlinedAt = getMetaOrNull(op.get<uint64_t>());

              debugLocIndex = m_DebugLocations.indexOf(debugLoc);

              if(debugLocIndex < 0)
              {
                m_DebugLocations.push_back(debugLoc);
                debugLocIndex = int32_t(m_DebugLocations.size() - 1);
              }

              f.instructions.back().debugLoc = (uint32_t)debugLocIndex;
            }
            else if(op.type == FunctionRecord::DEBUG_LOC_AGAIN)
            {
              f.instructions.back().debugLoc = (uint32_t)debugLocIndex;
            }
            else if(op.type == FunctionRecord::INST_CALL)
            {
              Instruction inst;
              inst.op = Operation::Call;
              inst.paramAttrs = &m_Attributes[op.get<size_t>()];

              uint64_t callingFlags = op.get<uint64_t>();

              if(callingFlags & (1ULL << 17))
                inst.opFlags = op.get<InstructionFlags>();

              if(callingFlags & (1ULL << 15))
                op.get<uint64_t>();    // funcCallType

              Symbol s = op.getSymbol();

              if(s.type != SymbolType::Function)
              {
                RDCERR("Unexpected symbol type %d called in INST_CALL", s.type);
                continue;
              }

              inst.funcCall = &m_Functions[(size_t)s.idx];
              inst.type = inst.funcCall->funcType->inner;

              for(size_t i = 0; op.remaining() > 0; i++)
              {
                if(inst.funcCall->funcType->members[i]->type == Type::Metadata)
                {
                  s.type = SymbolType::Metadata;
                  s.idx = uint32_t((uint64_t)m_Symbols.size() - op.get<uint64_t>());
                }
                else
                {
                  s = op.getSymbol(false);
                }
                inst.args.push_back(s);
              }

              RDCASSERTEQUAL(inst.args.size(), inst.funcCall->funcType->members.size());

              if(!inst.type->isVoid())
                m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_CAST)
            {
              Instruction inst;

              inst.args.push_back(op.getSymbol());
              inst.type = op.getType();

              uint64_t opcode = op.get<uint64_t>();
              inst.op = DecodeCast(opcode);

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_EXTRACTVAL)
            {
              Instruction inst;

              inst.op = Operation::ExtractVal;

              inst.args.push_back(op.getSymbol());
              inst.type = op.getType(f, inst.args.back());
              while(op.remaining() > 0)
              {
                uint64_t val = op.get<uint64_t>();
                if(inst.type->type == Type::Array)
                  inst.type = inst.type->inner;
                else
                  inst.type = inst.type->members[(size_t)val];
                inst.args.push_back({SymbolType::Literal, val});
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_RET)
            {
              Instruction inst;

              inst.op = Operation::Ret;

              if(op.remaining() == 0)
              {
                inst.type = GetVoidType();

                RDCASSERT(inst.type);
              }
              else
              {
                inst.args.push_back(op.getSymbol());
                inst.type = op.getType(f, inst.args.back());

                m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});
              }

              curBlock++;

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_BINOP)
            {
              Instruction inst;

              inst.args.push_back(op.getSymbol());
              inst.type = op.getType(f, inst.args.back());
              inst.args.push_back(op.getSymbol(false));

              bool isFloatOp = (inst.type->scalarType == Type::Float);

              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.op = isFloatOp ? Operation::FAdd : Operation::Add; break;
                case 1: inst.op = isFloatOp ? Operation::FSub : Operation::Sub; break;
                case 2: inst.op = isFloatOp ? Operation::FMul : Operation::Mul; break;
                case 3: inst.op = Operation::UDiv; break;
                case 4: inst.op = isFloatOp ? Operation::FDiv : Operation::SDiv; break;
                case 5: inst.op = Operation::URem; break;
                case 6: inst.op = isFloatOp ? Operation::FRem : Operation::SRem; break;
                case 7: inst.op = Operation::ShiftLeft; break;
                case 8: inst.op = Operation::LogicalShiftRight; break;
                case 9: inst.op = Operation::ArithShiftRight; break;
                case 10: inst.op = Operation::And; break;
                case 11: inst.op = Operation::Or; break;
                case 12: inst.op = Operation::Xor; break;
                default:
                  inst.op = Operation::And;
                  RDCERR("Unhandled binop type %llu", opcode);
                  break;
              }

              if(op.remaining() > 0)
              {
                uint64_t flags = op.get<uint64_t>();
                if(inst.op == Operation::Add || inst.op == Operation::Sub ||
                   inst.op == Operation::Mul || inst.op == Operation::ShiftLeft)
                {
                  if(flags & 0x2)
                    inst.opFlags |= InstructionFlags::NoSignedWrap;
                  if(flags & 0x1)
                    inst.opFlags |= InstructionFlags::NoUnsignedWrap;
                }
                else if(inst.op == Operation::SDiv || inst.op == Operation::UDiv ||
                        inst.op == Operation::LogicalShiftRight ||
                        inst.op == Operation::ArithShiftRight)
                {
                  if(flags & 0x1)
                    inst.opFlags |= InstructionFlags::Exact;
                }
                else if(isFloatOp)
                {
                  // fast math flags overlap
                  inst.opFlags = InstructionFlags(flags);
                }
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_UNREACHABLE)
            {
              Instruction inst;

              inst.op = Operation::Unreachable;
            }
            else if(op.type == FunctionRecord::INST_ALLOCA)
            {
              Instruction inst;

              inst.op = Operation::Alloca;

              inst.type = op.getType();

              // we now have the inner type, but this instruction returns a pointer to that type so
              // adjust
              inst.type = GetPointerType(inst.type);

              RDCASSERT(inst.type->type == Type::Pointer);

              // type of the size - ignored
              (void)op.getType();
              // size
              inst.args.push_back(op.getSymbolAbsolute());

              uint64_t align = op.get<uint64_t>();

              if(align & 0x20)
              {
                // argument alloca
              }
              if((align & 0x40) == 0)
              {
                RDCASSERT(inst.type->type == Type::Pointer);
                inst.type = inst.type->inner;
              }

              align &= ~0xE0;

              inst.align = (1U << align) >> 1;

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_INBOUNDS_GEP_OLD ||
                    op.type == FunctionRecord::INST_GEP_OLD || op.type == FunctionRecord::INST_GEP)
            {
              Instruction inst;

              inst.op = Operation::GetElementPtr;

              if(op.type == FunctionRecord::INST_INBOUNDS_GEP_OLD)
                inst.opFlags |= InstructionFlags::InBounds;

              if(op.type == FunctionRecord::INST_GEP)
              {
                if(op.get<uint64_t>())
                  inst.opFlags |= InstructionFlags::InBounds;
                inst.type = op.getType();
              }

              while(op.remaining() > 0)
              {
                inst.args.push_back(op.getSymbol());

                if(inst.type == NULL && inst.args.size() == 1)
                  inst.type = op.getType(f, inst.args.back());
              }

              // walk the type list to get the return type
              for(size_t idx = 2; idx < inst.args.size(); idx++)
              {
                if(inst.type->type == Type::Vector || inst.type->type == Type::Array)
                {
                  inst.type = inst.type->inner;
                }
                else if(inst.type->type == Type::Struct)
                {
                  Symbol s = inst.args[idx];
                  // if it's a struct the index must be constant
                  RDCASSERT(s.type == SymbolType::Constant);
                  inst.type = inst.type->members[GetFunctionConstant(f, s.idx)->val.uv[0]];
                }
                else
                {
                  RDCERR("Unexpected type %d encountered in GEP", inst.type->type);
                }
              }

              // get the pointer type
              inst.type = GetPointerType(inst.type);

              RDCASSERT(inst.type->type == Type::Pointer);

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_LOAD)
            {
              Instruction inst;

              inst.op = Operation::Load;

              inst.args.push_back(op.getSymbol());

              if(op.remaining() == 3)
              {
                inst.type = op.getType();
              }
              else
              {
                inst.type = op.getType(f, inst.args.back());
                RDCASSERT(inst.type->type == Type::Pointer);
                inst.type = inst.type->inner;
              }

              inst.align = (1U << op.get<uint64_t>()) >> 1;
              inst.opFlags |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                        : InstructionFlags::NoFlags;

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_STORE_OLD || op.type == FunctionRecord::INST_STORE)
            {
              Instruction inst;

              inst.op = Operation::Store;

              inst.type = GetVoidType();

              inst.args.push_back(op.getSymbol());
              if(op.type == FunctionRecord::INST_STORE_OLD)
                inst.args.push_back(op.getSymbol(false));
              else
                inst.args.push_back(op.getSymbol());

              inst.align = (1U << op.get<uint64_t>()) >> 1;
              inst.opFlags |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                        : InstructionFlags::NoFlags;

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_CMP ||
                    IS_KNOWN(op.type, FunctionRecord::INST_CMP2))
            {
              Instruction inst;

              // a
              inst.args.push_back(op.getSymbol());

              const Type *argType = op.getType(f, inst.args.back());

              // b
              inst.args.push_back(op.getSymbol(false));

              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.op = Operation::FOrdFalse; break;
                case 1: inst.op = Operation::FOrdEqual; break;
                case 2: inst.op = Operation::FOrdGreater; break;
                case 3: inst.op = Operation::FOrdGreaterEqual; break;
                case 4: inst.op = Operation::FOrdLess; break;
                case 5: inst.op = Operation::FOrdLessEqual; break;
                case 6: inst.op = Operation::FOrdNotEqual; break;
                case 7: inst.op = Operation::FOrd; break;
                case 8: inst.op = Operation::FUnord; break;
                case 9: inst.op = Operation::FUnordEqual; break;
                case 10: inst.op = Operation::FUnordGreater; break;
                case 11: inst.op = Operation::FUnordGreaterEqual; break;
                case 12: inst.op = Operation::FUnordLess; break;
                case 13: inst.op = Operation::FUnordLessEqual; break;
                case 14: inst.op = Operation::FUnordNotEqual; break;
                case 15: inst.op = Operation::FOrdTrue; break;

                case 32: inst.op = Operation::IEqual; break;
                case 33: inst.op = Operation::INotEqual; break;
                case 34: inst.op = Operation::UGreater; break;
                case 35: inst.op = Operation::UGreaterEqual; break;
                case 36: inst.op = Operation::ULess; break;
                case 37: inst.op = Operation::ULessEqual; break;
                case 38: inst.op = Operation::SGreater; break;
                case 39: inst.op = Operation::SGreaterEqual; break;
                case 40: inst.op = Operation::SLess; break;
                case 41: inst.op = Operation::SLessEqual; break;

                default:
                  inst.op = Operation::FOrdFalse;
                  RDCERR("Unexpected comparison %llu", opcode);
                  break;
              }

              // fast math flags
              if(op.remaining() > 0)
                inst.opFlags = op.get<InstructionFlags>();

              inst.type = GetBoolType();

              // if we're comparing vectors, the return type is an equal sized bool vector
              if(argType->type == Type::Vector)
              {
                for(const Type &t : m_Types)
                {
                  if(t.type == Type::Vector && t.inner == inst.type &&
                     t.elemCount == argType->elemCount)
                  {
                    inst.type = &t;
                    break;
                  }
                }
              }

              RDCASSERT(inst.type->type == argType->type &&
                        inst.type->elemCount == argType->elemCount);

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_SELECT || op.type == FunctionRecord::INST_VSELECT)
            {
              Instruction inst;

              inst.op = Operation::Select;

              // if true
              inst.args.push_back(op.getSymbol());

              inst.type = op.getType(f, inst.args.back());

              // if false
              inst.args.push_back(op.getSymbol(false));
              // selector
              if(op.type == FunctionRecord::INST_SELECT)
                inst.args.push_back(op.getSymbol(false));
              else
                inst.args.push_back(op.getSymbol());

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_BR)
            {
              Instruction inst;

              inst.op = Operation::Branch;

              inst.type = GetVoidType();

              // true destination
              uint64_t trueDest = op.get<uint64_t>();
              inst.args.push_back(Symbol(SymbolType::BasicBlock, trueDest));
              f.blocks[(size_t)trueDest].preds.insert(0, &f.blocks[curBlock]);

              if(op.remaining() > 0)
              {
                // false destination
                uint64_t falseDest = op.get<uint64_t>();
                inst.args.push_back(Symbol(SymbolType::BasicBlock, falseDest));
                f.blocks[(size_t)falseDest].preds.insert(0, &f.blocks[curBlock]);

                // predicate
                inst.args.push_back(op.getSymbol(false));
              }

              curBlock++;

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_SWITCH)
            {
              Instruction inst;

              inst.op = Operation::Switch;

              inst.type = GetVoidType();

              uint64_t typeIdx = op.get<uint64_t>();

              static const uint64_t SWITCH_INST_MAGIC = 0x4B5;
              if((typeIdx >> 16) == SWITCH_INST_MAGIC)
              {
                // type of condition
                const Type *condType = op.getType();

                RDCASSERT(condType->bitWidth <= 64);

                // condition
                inst.args.push_back(op.getSymbol(false));

                // default block
                uint64_t defaultDest = op.get<uint64_t>();
                inst.args.push_back(Symbol(SymbolType::BasicBlock, defaultDest));
                f.blocks[(size_t)defaultDest].preds.insert(0, &f.blocks[curBlock]);

                RDCERR("Unsupported switch instruction version");
              }
              else
              {
                // condition
                inst.args.push_back(op.getSymbol(false));

                // default block
                uint64_t defaultDest = op.get<uint64_t>();
                inst.args.push_back(Symbol(SymbolType::BasicBlock, defaultDest));
                f.blocks[(size_t)defaultDest].preds.insert(0, &f.blocks[curBlock]);

                uint64_t numCases = op.remaining() / 2;

                for(uint64_t c = 0; c < numCases; c++)
                {
                  // case value, absolute not relative
                  inst.args.push_back(op.getSymbolAbsolute());

                  // case block
                  uint64_t caseDest = op.get<uint64_t>();
                  inst.args.push_back(Symbol(SymbolType::BasicBlock, caseDest));
                  f.blocks[(size_t)caseDest].preds.insert(0, &f.blocks[curBlock]);
                }
              }

              curBlock++;

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_PHI)
            {
              Instruction inst;

              inst.op = Operation::Phi;

              inst.type = op.getType();

              while(op.remaining() > 0)
              {
                int64_t valSrc = LLVMBC::BitReader::svbr(op.get<uint64_t>());
                uint64_t blockSrc = op.get<uint64_t>();

                if(valSrc < 0)
                {
                  inst.args.push_back(Symbol(SymbolType::Unknown, m_Symbols.size() - valSrc));
                }
                else
                {
                  inst.args.push_back(op.getSymbol((uint64_t)valSrc));
                }
                inst.args.push_back(Symbol(SymbolType::BasicBlock, blockSrc));
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_LOADATOMIC)
            {
              Instruction inst;

              inst.op = Operation::LoadAtomic;

              inst.args.push_back(op.getSymbol());

              if(op.remaining() == 5)
              {
                inst.type = op.getType();
              }
              else
              {
                inst.type = op.getType(f, inst.args.back());
                RDCASSERT(inst.type->type == Type::Pointer);
                inst.type = inst.type->inner;
              }

              inst.align = (1U << op.get<uint64_t>()) >> 1;
              inst.opFlags |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                        : InstructionFlags::NoFlags;

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst.opFlags |= InstructionFlags::SuccessUnordered; break;
                case 2: inst.opFlags |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst.opFlags |= InstructionFlags::SuccessAcquire; break;
                case 4: inst.opFlags |= InstructionFlags::SuccessRelease; break;
                case 5: inst.opFlags |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.opFlags |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_STOREATOMIC_OLD ||
                    op.type == FunctionRecord::INST_STOREATOMIC)
            {
              Instruction inst;

              inst.op = Operation::StoreAtomic;

              inst.type = GetVoidType();

              inst.args.push_back(op.getSymbol());
              if(op.type == FunctionRecord::INST_STOREATOMIC_OLD)
                inst.args.push_back(op.getSymbol(false));
              else
                inst.args.push_back(op.getSymbol());

              inst.align = (1U << op.get<uint64_t>()) >> 1;
              inst.opFlags |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                        : InstructionFlags::NoFlags;

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst.opFlags |= InstructionFlags::SuccessUnordered; break;
                case 2: inst.opFlags |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst.opFlags |= InstructionFlags::SuccessAcquire; break;
                case 4: inst.opFlags |= InstructionFlags::SuccessRelease; break;
                case 5: inst.opFlags |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.opFlags |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_ATOMICRMW)
            {
              Instruction inst;

              // pointer to atomically modify
              inst.args.push_back(op.getSymbol());

              // type is the pointee of the first argument
              inst.type = op.getType(f, inst.args.back());
              RDCASSERT(inst.type->type == Type::Pointer);
              inst.type = inst.type->inner;

              // parameter value
              inst.args.push_back(op.getSymbol(false));

              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.op = Operation::AtomicExchange; break;
                case 1: inst.op = Operation::AtomicAdd; break;
                case 2: inst.op = Operation::AtomicSub; break;
                case 3: inst.op = Operation::AtomicAnd; break;
                case 4: inst.op = Operation::AtomicNand; break;
                case 5: inst.op = Operation::AtomicOr; break;
                case 6: inst.op = Operation::AtomicXor; break;
                case 7: inst.op = Operation::AtomicMax; break;
                case 8: inst.op = Operation::AtomicMin; break;
                case 9: inst.op = Operation::AtomicUMax; break;
                case 10: inst.op = Operation::AtomicUMin; break;
                default:
                  RDCERR("Unhandled atomicrmw op %llu", opcode);
                  inst.op = Operation::AtomicExchange;
                  break;
              }

              if(op.get<uint64_t>())
                inst.opFlags |= InstructionFlags::Volatile;

              // success ordering
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst.opFlags |= InstructionFlags::SuccessUnordered; break;
                case 2: inst.opFlags |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst.opFlags |= InstructionFlags::SuccessAcquire; break;
                case 4: inst.opFlags |= InstructionFlags::SuccessRelease; break;
                case 5: inst.opFlags |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.opFlags |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_CMPXCHG ||
                    op.type == FunctionRecord::INST_CMPXCHG_OLD)
            {
              Instruction inst;

              inst.op = Operation::CompareExchange;

              // pointer to atomically modify
              inst.args.push_back(op.getSymbol());

              // type is the pointee of the first argument
              inst.type = op.getType(f, inst.args.back());
              RDCASSERT(inst.type->type == Type::Pointer);
              inst.type = inst.type->inner;

              // combined with a bool, search for a struct like that
              const Type *boolType = GetBoolType();

              for(const Type &t : m_Types)
              {
                if(t.type == Type::Struct && t.members.size() == 2 && t.members[0] == inst.type &&
                   t.members[1] == boolType)
                {
                  inst.type = &t;
                  break;
                }
              }

              RDCASSERT(inst.type->type == Type::Struct);

              // expect modern encoding with weak parameters.
              RDCASSERT(funcChild.ops.size() >= 8);

              // compare value
              if(op.type == FunctionRecord::INST_CMPXCHG_OLD)
                inst.args.push_back(op.getSymbol(false));
              else
                inst.args.push_back(op.getSymbol());

              // new replacement value
              inst.args.push_back(op.getSymbol(false));

              if(op.get<uint64_t>())
                inst.opFlags |= InstructionFlags::Volatile;

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst.opFlags |= InstructionFlags::SuccessUnordered; break;
                case 2: inst.opFlags |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst.opFlags |= InstructionFlags::SuccessAcquire; break;
                case 4: inst.opFlags |= InstructionFlags::SuccessRelease; break;
                case 5: inst.opFlags |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.opFlags |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              // failure ordering
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst.opFlags |= InstructionFlags::FailureUnordered; break;
                case 2: inst.opFlags |= InstructionFlags::FailureMonotonic; break;
                case 3: inst.opFlags |= InstructionFlags::FailureAcquire; break;
                case 4: inst.opFlags |= InstructionFlags::FailureRelease; break;
                case 5: inst.opFlags |= InstructionFlags::FailureAcquireRelease; break;
                case 6: inst.opFlags |= InstructionFlags::FailureSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected failure ordering %llu", opcode);
                  inst.opFlags |= InstructionFlags::FailureSequentiallyConsistent;
                  break;
              }

              if(op.get<uint64_t>())
                inst.opFlags |= InstructionFlags::Weak;

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_FENCE)
            {
              Instruction inst;

              inst.op = Operation::Fence;

              inst.type = GetVoidType();

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst.opFlags |= InstructionFlags::SuccessUnordered; break;
                case 2: inst.opFlags |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst.opFlags |= InstructionFlags::SuccessAcquire; break;
                case 4: inst.opFlags |= InstructionFlags::SuccessRelease; break;
                case 5: inst.opFlags |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst.opFlags |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst.opFlags |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_EXTRACTELT)
            {
              // DXIL claims to be scalarised so should this appear?
              RDCWARN("Unexpected vector instruction extractelement in DXIL");

              Instruction inst;

              inst.op = Operation::ExtractElement;

              // vector
              inst.args.push_back(op.getSymbol());

              // result is the scalar type within the vector
              inst.type = op.getType(f, inst.args.back())->inner;

              // index
              inst.args.push_back(op.getSymbol());

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_INSERTELT)
            {
              // DXIL claims to be scalarised so should this appear?
              RDCWARN("Unexpected vector instruction insertelement in DXIL");

              Instruction inst;

              inst.op = Operation::InsertElement;

              // vector
              inst.args.push_back(op.getSymbol());

              // result is the vector type
              inst.type = op.getType(f, inst.args.back());

              // replacement element
              inst.args.push_back(op.getSymbol(false));
              // index
              inst.args.push_back(op.getSymbol());

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_SHUFFLEVEC)
            {
              // DXIL claims to be scalarised so should this appear?
              RDCWARN("Unexpected vector instruction shufflevector in DXIL");

              Instruction inst;

              inst.op = Operation::ShuffleVector;

              // vector 1
              inst.args.push_back(op.getSymbol());

              const Type *vecType = op.getType(f, inst.args.back());

              // vector 2
              inst.args.push_back(op.getSymbol(false));
              // indexes
              inst.args.push_back(op.getSymbol());

              // result is a vector with the inner type of the first two vectors and the element
              // count of the last vector
              const Type *maskType = op.getType(f, inst.args.back());

              for(const Type &t : m_Types)
              {
                if(t.type == Type::Vector && t.inner == vecType->inner &&
                   t.elemCount == maskType->elemCount)
                {
                  inst.type = &t;
                  break;
                }
              }

              RDCASSERT(inst.type);

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_INSERTVAL)
            {
              // DXIL claims to be scalarised so should this appear?
              RDCWARN("Unexpected aggregate instruction insertvalue in DXIL");

              Instruction inst;

              inst.op = Operation::InsertValue;

              // aggregate
              inst.args.push_back(op.getSymbol());

              // result is the aggregate type
              inst.type = op.getType(f, inst.args.back());

              // replacement element
              inst.args.push_back(op.getSymbol());
              // indices as literals
              while(op.remaining() > 0)
                inst.args.push_back(Symbol(SymbolType::Literal, op.get<uint64_t>()));

              m_Symbols.push_back({SymbolType::Instruction, f.instructions.size()});

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_VAARG)
            {
              // don't expect vararg instructions
              RDCERR("Unexpected vararg instruction %u in DXIL", op.type);
            }
            else if(op.type == FunctionRecord::INST_LANDINGPAD ||
                    op.type == FunctionRecord::INST_LANDINGPAD_OLD ||
                    op.type == FunctionRecord::INST_INVOKE || op.type == FunctionRecord::INST_RESUME)
            {
              // don't expect exception handling instructions
              RDCERR("Unexpected exception handling instruction %u in DXIL", op.type);
            }
            else
            {
              RDCERR("Unexpected record in FUNCTION_BLOCK");
              continue;
            }
          }
        }

        RDCASSERT(curBlock == f.blocks.size());

        size_t resultID = 0;

        if(f.blocks[0].name.empty())
          f.blocks[0].resultID = (uint32_t)resultID++;

        curBlock = 0;
        for(size_t i = 0; i < f.instructions.size(); i++)
        {
          // fix up forward references here, we couldn't write them up front because we didn't know
          // how many actual symbols (non-void instructions) existed after the given instruction
          for(Symbol &s : f.instructions[i].args)
          {
            if(s.type == SymbolType::Unknown)
            {
              s = m_Symbols[(size_t)s.idx];
              RDCASSERT(s.type == SymbolType::Instruction);
            }
          }

          if(f.instructions[i].op == Operation::Branch ||
             f.instructions[i].op == Operation::Unreachable ||
             f.instructions[i].op == Operation::Switch || f.instructions[i].op == Operation::Ret)
          {
            curBlock++;

            if(i == f.instructions.size() - 1)
              break;

            if(f.blocks[curBlock].name.empty())
              f.blocks[curBlock].resultID = (uint32_t)resultID++;
            continue;
          }

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
            m.instruction = (size_t)m_Symbols[instrSymbolStart + m.instruction].idx;

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

uint32_t Program::GetOrAssignMetaID(Metadata *m)
{
  if(m->id != ~0U)
    return m->id;

  m->id = m_NextMetaID++;
  m_NumberedMeta.push_back(m);

  // assign meta IDs to the children now
  for(Metadata *c : m->children)
  {
    if(!c || c->isConstant)
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

  if(l.scope)
    GetOrAssignMetaID(l.scope);
  if(l.inlinedAt)
    GetOrAssignMetaID(l.inlinedAt);

  return l.id;
}

const Type *Program::GetSymbolType(const Function &f, Symbol s)
{
  const Type *ret = NULL;
  switch(s.type)
  {
    case SymbolType::Constant:
      if(s.idx < m_Constants.size())
        ret = m_Constants[(size_t)s.idx].type;
      else
        ret = f.constants[(size_t)s.idx - m_Constants.size()].type;
      break;
    case SymbolType::Argument: ret = f.funcType->members[(size_t)s.idx]; break;
    case SymbolType::Instruction: ret = f.instructions[(size_t)s.idx].type; break;
    case SymbolType::GlobalVar: ret = m_GlobalVars[(size_t)s.idx].type; break;
    case SymbolType::Function: ret = m_Functions[(size_t)s.idx].funcType; break;
    case SymbolType::Metadata:
      if(s.idx < m_Metadata.size())
        ret = m_Metadata[(size_t)s.idx].type;
      else
        ret = f.metadata[(size_t)s.idx - m_Metadata.size()].type;
      break;
    case SymbolType::Unknown:
    case SymbolType::Alias:
    case SymbolType::BasicBlock:
    case SymbolType::Literal: RDCERR("Unexpected symbol to get type for %d", s.type); break;
  }
  return ret;
}

const Constant *Program::GetFunctionConstant(const Function &f, uint64_t v)
{
  size_t idx = (size_t)v;
  return idx < m_Constants.size() ? &m_Constants[idx] : &f.constants[idx - m_Constants.size()];
}

const Metadata *Program::GetFunctionMetadata(const Function &f, uint64_t v)
{
  size_t idx = (size_t)v;
  return idx < m_Metadata.size() ? &m_Metadata[idx] : &f.metadata[idx - m_Metadata.size()];
}

const DXIL::Type *Program::GetVoidType()
{
  if(m_VoidType)
    return m_VoidType;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    if(m_Types[i].isVoid())
    {
      m_VoidType = &m_Types[i];
      break;
    }
  }

  if(!m_VoidType)
    RDCERR("Couldn't find void type");

  return m_VoidType;
}

const DXIL::Type *Program::GetBoolType()
{
  if(m_BoolType)
    return m_BoolType;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    if(m_Types[i].type == Type::Scalar && m_Types[i].scalarType == Type::Int &&
       m_Types[i].bitWidth == 1)
    {
      m_BoolType = &m_Types[i];
      break;
    }
  }

  if(!m_BoolType)
    RDCERR("Couldn't find void type");

  return m_BoolType;
}

const Type *Program::GetPointerType(const Type *type)
{
  for(const Type &t : m_Types)
  {
    if(t.type == Type::Pointer && t.inner == type)
    {
      return &t;
    }
  }

  RDCERR("Couldn't find pointer type");

  return type;
}

Metadata::~Metadata()
{
  SAFE_DELETE(dwarf);
  SAFE_DELETE(debugLoc);
}
};    // namespace DXIL
