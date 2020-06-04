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

static rdcstr getName(uint32_t parentBlock, const LLVMBC::BlockOrRecord &block)
{
  const char *name = NULL;

  if(block.IsBlock())
  {
    // GetBlockName in BitcodeAnalyzer.cpp
    switch(KnownBlocks(block.id))
    {
      case KnownBlocks::BLOCKINFO: name = "BLOCKINFO"; break;
      case KnownBlocks::MODULE_BLOCK: name = "MODULE_BLOCK"; break;
      case KnownBlocks::PARAMATTR_BLOCK: name = "PARAMATTR_BLOCK"; break;
      case KnownBlocks::PARAMATTR_GROUP_BLOCK: name = "PARAMATTR_GROUP_BLOCK"; break;
      case KnownBlocks::CONSTANTS_BLOCK: name = "CONSTANTS_BLOCK"; break;
      case KnownBlocks::FUNCTION_BLOCK: name = "FUNCTION_BLOCK"; break;
      case KnownBlocks::TYPE_SYMTAB_BLOCK: name = "TYPE_SYMTAB_BLOCK"; break;
      case KnownBlocks::VALUE_SYMTAB_BLOCK: name = "VALUE_SYMTAB_BLOCK"; break;
      case KnownBlocks::METADATA_BLOCK: name = "METADATA_BLOCK"; break;
      case KnownBlocks::METADATA_ATTACHMENT: name = "METADATA_ATTACHMENT"; break;
      case KnownBlocks::TYPE_BLOCK: name = "TYPE_BLOCK"; break;
      default: break;
    }
  }
  else
  {
#define STRINGISE_RECORD(a) \
  case decltype(code)::a: name = #a; break;

    // GetCodeName in BitcodeAnalyzer.cpp
    switch(KnownBlocks(parentBlock))
    {
      case KnownBlocks::BLOCKINFO:
      case KnownBlocks::TYPE_SYMTAB_BLOCK:
      case KnownBlocks::METADATA_ATTACHMENT: break;
      case KnownBlocks::MODULE_BLOCK:
      {
        ModuleRecord code = ModuleRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(VERSION);
          STRINGISE_RECORD(TRIPLE);
          STRINGISE_RECORD(DATALAYOUT);
          STRINGISE_RECORD(GLOBALVAR);
          STRINGISE_RECORD(FUNCTION);
          STRINGISE_RECORD(ALIAS);
          default: break;
        }
        break;
      }
      case KnownBlocks::PARAMATTR_BLOCK:
      case KnownBlocks::PARAMATTR_GROUP_BLOCK: return StringFormat::Fmt("ENTRY%u", block.id); break;
      case KnownBlocks::CONSTANTS_BLOCK:
      {
        ConstantsRecord code = ConstantsRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(SETTYPE);
          STRINGISE_RECORD(UNDEF);
          STRINGISE_RECORD(INTEGER);
          STRINGISE_RECORD(FLOAT);
          STRINGISE_RECORD(AGGREGATE);
          STRINGISE_RECORD(STRING);
          STRINGISE_RECORD(DATA);
          case ConstantsRecord::CONST_NULL: name = "NULL"; break;
          default: break;
        }
        break;
      }
      case KnownBlocks::FUNCTION_BLOCK:
      {
        FunctionRecord code = FunctionRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(DECLAREBLOCKS);
          STRINGISE_RECORD(INST_BINOP);
          STRINGISE_RECORD(INST_CAST);
          STRINGISE_RECORD(INST_GEP_OLD);
          STRINGISE_RECORD(INST_SELECT);
          STRINGISE_RECORD(INST_EXTRACTELT);
          STRINGISE_RECORD(INST_INSERTELT);
          STRINGISE_RECORD(INST_SHUFFLEVEC);
          STRINGISE_RECORD(INST_CMP);
          STRINGISE_RECORD(INST_RET);
          STRINGISE_RECORD(INST_BR);
          STRINGISE_RECORD(INST_SWITCH);
          STRINGISE_RECORD(INST_INVOKE);
          STRINGISE_RECORD(INST_UNREACHABLE);
          STRINGISE_RECORD(INST_PHI);
          STRINGISE_RECORD(INST_ALLOCA);
          STRINGISE_RECORD(INST_LOAD);
          STRINGISE_RECORD(INST_VAARG);
          STRINGISE_RECORD(INST_STORE_OLD);
          STRINGISE_RECORD(INST_EXTRACTVAL);
          STRINGISE_RECORD(INST_INSERTVAL);
          STRINGISE_RECORD(INST_CMP2);
          STRINGISE_RECORD(INST_VSELECT);
          STRINGISE_RECORD(INST_INBOUNDS_GEP_OLD);
          STRINGISE_RECORD(INST_INDIRECTBR);
          STRINGISE_RECORD(DEBUG_LOC_AGAIN);
          STRINGISE_RECORD(INST_CALL);
          STRINGISE_RECORD(DEBUG_LOC);
          STRINGISE_RECORD(INST_FENCE);
          STRINGISE_RECORD(INST_CMPXCHG_OLD);
          STRINGISE_RECORD(INST_ATOMICRMW);
          STRINGISE_RECORD(INST_RESUME);
          STRINGISE_RECORD(INST_LANDINGPAD_OLD);
          STRINGISE_RECORD(INST_LOADATOMIC);
          STRINGISE_RECORD(INST_STOREATOMIC_OLD);
          STRINGISE_RECORD(INST_GEP);
          STRINGISE_RECORD(INST_STORE);
          STRINGISE_RECORD(INST_STOREATOMIC);
          STRINGISE_RECORD(INST_CMPXCHG);
          STRINGISE_RECORD(INST_LANDINGPAD);
          STRINGISE_RECORD(INST_CLEANUPRET);
          STRINGISE_RECORD(INST_CATCHRET);
          STRINGISE_RECORD(INST_CATCHPAD);
          STRINGISE_RECORD(INST_CLEANUPPAD);
          STRINGISE_RECORD(INST_CATCHSWITCH);
          STRINGISE_RECORD(OPERAND_BUNDLE);
          STRINGISE_RECORD(INST_UNOP);
          STRINGISE_RECORD(INST_CALLBR);
          default: break;
        }
        break;
      }
      case KnownBlocks::VALUE_SYMTAB_BLOCK:
      {
        ValueSymtabRecord code = ValueSymtabRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(ENTRY);
          STRINGISE_RECORD(BBENTRY);
          STRINGISE_RECORD(FNENTRY);
          STRINGISE_RECORD(COMBINED_ENTRY);
          default: break;
        }
        break;
      }
      case KnownBlocks::METADATA_BLOCK:
      {
        MetaDataRecord code = MetaDataRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(STRING_OLD);
          STRINGISE_RECORD(VALUE);
          STRINGISE_RECORD(NODE);
          STRINGISE_RECORD(NAME);
          STRINGISE_RECORD(DISTINCT_NODE);
          STRINGISE_RECORD(KIND);
          STRINGISE_RECORD(LOCATION);
          STRINGISE_RECORD(OLD_NODE);
          STRINGISE_RECORD(OLD_FN_NODE);
          STRINGISE_RECORD(NAMED_NODE);
          STRINGISE_RECORD(ATTACHMENT);
          STRINGISE_RECORD(GENERIC_DEBUG);
          STRINGISE_RECORD(SUBRANGE);
          STRINGISE_RECORD(ENUMERATOR);
          STRINGISE_RECORD(BASIC_TYPE);
          STRINGISE_RECORD(FILE);
          STRINGISE_RECORD(DERIVED_TYPE);
          STRINGISE_RECORD(COMPOSITE_TYPE);
          STRINGISE_RECORD(SUBROUTINE_TYPE);
          STRINGISE_RECORD(COMPILE_UNIT);
          STRINGISE_RECORD(SUBPROGRAM);
          STRINGISE_RECORD(LEXICAL_BLOCK);
          STRINGISE_RECORD(LEXICAL_BLOCK_FILE);
          STRINGISE_RECORD(NAMESPACE);
          STRINGISE_RECORD(TEMPLATE_TYPE);
          STRINGISE_RECORD(TEMPLATE_VALUE);
          STRINGISE_RECORD(GLOBAL_VAR);
          STRINGISE_RECORD(LOCAL_VAR);
          STRINGISE_RECORD(EXPRESSION);
          STRINGISE_RECORD(OBJC_PROPERTY);
          STRINGISE_RECORD(IMPORTED_ENTITY);
          STRINGISE_RECORD(MODULE);
          STRINGISE_RECORD(MACRO);
          STRINGISE_RECORD(MACRO_FILE);
          STRINGISE_RECORD(STRINGS);
          STRINGISE_RECORD(GLOBAL_DECL_ATTACHMENT);
          STRINGISE_RECORD(GLOBAL_VAR_EXPR);
          STRINGISE_RECORD(INDEX_OFFSET);
          STRINGISE_RECORD(INDEX);
          STRINGISE_RECORD(LABEL);
          STRINGISE_RECORD(COMMON_BLOCK);
          default: break;
        }
        break;
      }
      case KnownBlocks::TYPE_BLOCK:
      {
        TypeRecord code = TypeRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(NUMENTRY);
          STRINGISE_RECORD(VOID);
          STRINGISE_RECORD(FLOAT);
          STRINGISE_RECORD(DOUBLE);
          STRINGISE_RECORD(LABEL);
          STRINGISE_RECORD(OPAQUE);
          STRINGISE_RECORD(INTEGER);
          STRINGISE_RECORD(POINTER);
          STRINGISE_RECORD(FUNCTION_OLD);
          STRINGISE_RECORD(HALF);
          STRINGISE_RECORD(ARRAY);
          STRINGISE_RECORD(VECTOR);
          STRINGISE_RECORD(METADATA);
          STRINGISE_RECORD(STRUCT_ANON);
          STRINGISE_RECORD(STRUCT_NAME);
          STRINGISE_RECORD(STRUCT_NAMED);
          STRINGISE_RECORD(FUNCTION);
          default: break;
        }
        break;
      }
    }
  }

  // fallback
  if(name)
  {
    return name;
  }
  else
  {
    if(block.IsBlock())
      return StringFormat::Fmt("BLOCK%u", block.id);
    else
      return StringFormat::Fmt("RECORD%u", block.id);
  }
}

bool needsEscaping(const rdcstr &name)
{
  return name.find_first_not_of(
             "-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ$._0123456789") >= 0;
}

rdcstr escapeString(rdcstr str)
{
  for(size_t i = 0; i < str.size(); i++)
  {
    if(str[i] == '\'' || str[i] == '\\')
    {
      str.insert(i, "\\", 1);
      i++;
    }
    else if(str[i] == '\r' || str[i] == '\n' || str[i] == '\t' || !isprint(str[i]))
    {
      str.insert(i + 1, StringFormat::Fmt("%02X", str[i]));
      str[i] = '\\';
    }
  }

  str.push_back('"');
  str.insert(0, '"');

  return str;
}

static void dumpRecord(size_t idx, uint32_t parentBlock, const LLVMBC::BlockOrRecord &record,
                       int indent)
{
  rdcstr line;
  line.fill(indent, ' ');

  line += StringFormat::Fmt("[%u] = ", idx);

  line += "<" + getName(parentBlock, record);

  if(KnownBlocks(parentBlock) == KnownBlocks::METADATA_BLOCK &&
     (MetaDataRecord(record.id) == MetaDataRecord::STRING_OLD ||
      MetaDataRecord(record.id) == MetaDataRecord::NAME ||
      MetaDataRecord(record.id) == MetaDataRecord::KIND))
  {
    line += " record string = " + escapeString(record.getString());
  }
  else
  {
    bool allASCII = true;
    for(size_t i = 0; i < record.ops.size(); i++)
    {
      if(record.ops[i] < 0x20 || record.ops[i] > 0x7f)
      {
        allASCII = false;
        break;
      }
    }

    if(allASCII && record.ops.size() > 3)
      line += " record string = " + escapeString(record.getString());

    for(size_t i = 0; i < record.ops.size(); i++)
      line += StringFormat::Fmt(" op%u=%llu", (uint32_t)i, record.ops[i]);
  }

  if(record.blob)
    line += StringFormat::Fmt(" with blob of %u bytes", (uint32_t)record.blobLength);

  line += "/>";

  RDCLOG("%s", line.c_str());
}

static void dumpBlock(const LLVMBC::BlockOrRecord &block, int indent)
{
  rdcstr line;
  line.fill(indent, ' ');

  if(block.children.empty() || KnownBlocks(block.id) == KnownBlocks::BLOCKINFO)
  {
    line += StringFormat::Fmt("<%s/>", getName(0, block).c_str());
    RDCLOG("%s", line.c_str());
    return;
  }

  line += StringFormat::Fmt("<%s NumWords=%u>", getName(0, block).c_str(), block.blockDwordLength);
  RDCLOG("%s", line.c_str());

  for(size_t i = 0; i < block.children.size(); i++)
  {
    const LLVMBC::BlockOrRecord &child = block.children[i];

    if(child.IsBlock())
      dumpBlock(child, indent + 2);
    else
      dumpRecord(i, block.id, child, indent + 2);
  }

  line.fill(indent, ' ');
  line += StringFormat::Fmt("</%s>", getName(0, block).c_str());
  RDCLOG("%s", line.c_str());
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

#define IS_KNOWN(val, KnownID) (decltype(KnownID)(val) == KnownID)

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
          if(IS_KNOWN(constant.id, ConstantsRecord::SETTYPE))
          {
            t = &m_Types[(size_t)constant.ops[0]];
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL) ||
                  IS_KNOWN(constant.id, ConstantsRecord::UNDEF))
          {
            Value v;
            v.type = t;
            v.undef = IS_KNOWN(constant.id, ConstantsRecord::UNDEF);
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::INTEGER))
          {
            Value v;
            v.type = t;
            v.val.u64v[0] = constant.ops[0];
            if(v.val.u64v[0] & 0x1)
              v.val.s64v[0] = -int64_t(v.val.u64v[0] >> 1);
            else
              v.val.u64v[0] >>= 1;
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::FLOAT))
          {
            Value v;
            v.type = t;
            if(t->bitWidth == 16)
              v.val.fv[0] = ConvertFromHalf(uint16_t(constant.ops[0] & 0xffff));
            else if(t->bitWidth == 32)
              memcpy(&v.val.fv[0], &constant.ops[0], sizeof(float));
            else
              memcpy(&v.val.dv[0], &constant.ops[0], sizeof(float));
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::STRING))
          {
            Value v;
            v.type = t;
            v.str = constant.getString(0);
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::AGGREGATE))
          {
            Value v;
            v.type = t;
            if(v.type->type == Type::Vector)
            {
              // inline vectors
              for(size_t m = 0; m < constant.ops.size(); m++)
              {
                size_t idx = (size_t)constant.ops[m];
                if(idx < m_Values.size())
                {
                  if(v.type->bitWidth <= 32)
                    v.val.uv[m] = m_Values[idx].val.uv[m];
                  else
                    v.val.u64v[m] = m_Values[idx].val.u64v[m];
                }
                else
                {
                  RDCERR("Index %zu out of bounds for values array", idx);
                }
              }
            }
            else
            {
              for(uint64_t m : constant.ops)
              {
                size_t idx = (size_t)m;
                if(idx < m_Values.size())
                {
                  v.members.push_back(m_Values[idx]);
                }
                else
                {
                  v.members.push_back(Value());
                  RDCERR("Index %zu out of bounds for values array", idx);
                }
              }
            }
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::DATA))
          {
            Value v;
            v.type = t;
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
            m_Values.push_back(v);
          }
          else
          {
            RDCERR("Unknown record ID %u encountered in constants block", constant.id);
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlocks::VALUE_SYMTAB_BLOCK))
      {
        for(const LLVMBC::BlockOrRecord &symtab : rootchild.children)
        {
          if(symtab.id != 1)
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
        m_Metadata.resize_for_index(rootchild.children.size() - 1);
        for(size_t i = 0; i < rootchild.children.size(); i++)
        {
          const LLVMBC::BlockOrRecord &metaRecord = rootchild.children[i];
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
            Metadata &meta = m_Metadata[i];

            auto getMeta = [this](uint64_t id) { return id ? &m_Metadata[size_t(id - 1)] : NULL; };
            auto getMetaString = [this](uint64_t id) {
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
                meta.children.push_back(getMeta(op));
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
        RDCLOG("Skipping function body for %s", m_Functions[functionDecls[0]].name.c_str());
        functionDecls.erase(0);
      }
      else
      {
        RDCERR("Unknown block ID %u encountered at module scope", rootchild.id);
      }
    }
  }

  (void)&dumpBlock;
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

void Program::MakeDisassemblyString()
{
  const char *shaderName[] = {
      "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
      "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
      "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
  };

  m_Disassembly = StringFormat::Fmt("; %s Shader, compiled under SM%u.%u\n\n",
                                    shaderName[int(m_Type)], m_Major, m_Minor);
  m_Disassembly += StringFormat::Fmt("target datalayout = \"%s\"\n", m_Datalayout.c_str());
  m_Disassembly += StringFormat::Fmt("target triple = \"%s\"\n\n", m_Triple.c_str());

  int instructionLine = 6;

  bool typesPrinted = false;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    const Type &typ = m_Types[i];

    if(typ.type == Type::Struct && !typ.name.empty())
    {
      rdcstr name = typ.toString();
      m_Disassembly += StringFormat::Fmt("%s = type {", name.c_str());
      bool first = true;
      for(const Type *t : typ.members)
      {
        if(!first)
          m_Disassembly += ", ";
        first = false;
        m_Disassembly += StringFormat::Fmt(" %s", t->toString().c_str());
      }
      m_Disassembly += " }\n";
      typesPrinted = true;

      instructionLine++;
    }
  }

  if(typesPrinted)
  {
    m_Disassembly += "\n";
    instructionLine++;
  }

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = m_GlobalVars[i];

    m_Disassembly += StringFormat::Fmt(
        "@%s = ", needsEscaping(g.name) ? escapeString(g.name).c_str() : g.name.c_str());
    if(g.external)
      m_Disassembly += "external ";
    if(g.isconst)
      m_Disassembly += "constant ";
    m_Disassembly += g.type->toString();

    if(g.align > 0)
      m_Disassembly += StringFormat::Fmt(", align %u", g.align);

    m_Disassembly += "\n";
    instructionLine++;
  }

  if(!m_GlobalVars.empty())
  {
    m_Disassembly += "\n";
    instructionLine++;
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = m_Functions[i];

    if(func.attrs)
    {
      m_Disassembly += StringFormat::Fmt("; Function Attrs: %s\n", func.attrs->toString().c_str());
      instructionLine++;
    }

    m_Disassembly += (func.external ? "declare " : "define ");
    m_Disassembly += func.funcType->declFunction("@" + func.name);

    if(func.attrs)
      m_Disassembly += StringFormat::Fmt(" #%u", func.attrs->index);

    if(!func.external)
    {
      m_Disassembly += " {\n";
      instructionLine++;
      m_Disassembly += "  ; ...\n";
      instructionLine++;
      m_Disassembly += "}\n\n";
      instructionLine += 2;
    }
    else
    {
      m_Disassembly += "\n\n";
      instructionLine += 2;
    }
  }

  for(size_t i = 0; i < m_Attributes.size(); i++)
    m_Disassembly +=
        StringFormat::Fmt("attributes #%zu = { %s }\n", i, m_Attributes[i].toString().c_str());

  if(!m_Attributes.empty())
    m_Disassembly += "\n";

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    m_Disassembly += StringFormat::Fmt("!%s = %s!{", m_NamedMeta[i].name.c_str(),
                                       m_NamedMeta[i].distinct ? "distinct " : "");
    for(size_t m = 0; m < m_NamedMeta[i].children.size(); m++)
    {
      if(m != 0)
        m_Disassembly += ", ";
      if(m_NamedMeta[i].children[m])
        m_Disassembly += StringFormat::Fmt("!%u", GetOrAssignMetaID(m_NamedMeta[i].children[m]));
      else
        m_Disassembly += "null";
    }

    m_Disassembly += "}\n";
  }

  m_Disassembly += "\n";

  for(size_t i = 0; i < m_NumberedMeta.size(); i++)
    m_Disassembly += StringFormat::Fmt("%s = %s%s\n", m_NumberedMeta[i]->refString(),
                                       m_NumberedMeta[i]->distinct ? "distinct " : "",
                                       m_NumberedMeta[i]->valString().c_str());

  m_Disassembly += "\n";
}

uint32_t Program::GetOrAssignMetaID(Metadata *m)
{
  if(m->id != ~0U)
    return m->id;

  m->id = (uint32_t)m_NumberedMeta.size();
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

rdcstr Type::toString() const
{
  if(!name.empty())
  {
    // needs escaping
    if(needsEscaping(name))
      return "%" + escapeString(name);

    return "%" + name;
  }

  switch(type)
  {
    case Void: return "void";
    case Scalar:
    {
      switch(scalarType)
      {
        case Void: return "void";
        case Int: return StringFormat::Fmt("i%u", bitWidth);
        case Float:
          switch(bitWidth)
          {
            case 16: return "half";
            case 32: return "float";
            case 64: return "double";
            default: return StringFormat::Fmt("fp%u", bitWidth);
          }
      }
    }
    case Vector: return StringFormat::Fmt("<%u x %s>", elemCount, inner->toString().c_str());
    case Pointer: return StringFormat::Fmt("%s*", inner->toString().c_str());
    case Array: return StringFormat::Fmt("[%u x %s]", elemCount, inner->toString().c_str());
    case Function: return declFunction(rdcstr());
    case Struct:
    {
      rdcstr ret;
      if(packedStruct)
        ret = "<{";
      else
        ret = "{";
      for(size_t i = 0; i < members.size(); i++)
      {
        if(i > 0)
          ret += ", ";
        ret += members[i]->toString();
      }
      if(packedStruct)
        ret += "}>";
      else
        ret += "}";
      return ret;
    }

    case Metadata: return "metadata";
    case Label: return "label";
    default: return "unknown_type";
  }
}

rdcstr Type::declFunction(rdcstr funcName) const
{
  rdcstr ret = inner->toString();
  ret += " " + funcName + "(";
  for(size_t i = 0; i < members.size(); i++)
  {
    if(i > 0)
      ret += ", ";
    ret += members[i]->toString();
  }
  ret += ")";
  return ret;
}

rdcstr Attributes::toString() const
{
  rdcstr ret = "";
  Attribute p = params;

  if(p & Attribute::Alignment)
  {
    ret += StringFormat::Fmt(" align=%llu", align);
    p &= ~Attribute::Alignment;
  }
  if(p & Attribute::StackAlignment)
  {
    ret += StringFormat::Fmt(" alignstack=%llu", stackAlign);
    p &= ~Attribute::StackAlignment;
  }
  if(p & Attribute::Dereferenceable)
  {
    ret += StringFormat::Fmt(" dereferenceable=%llu", derefBytes);
    p &= ~Attribute::Dereferenceable;
  }
  if(p & Attribute::DereferenceableOrNull)
  {
    ret += StringFormat::Fmt(" dereferenceable_or_null=%llu", derefOrNullBytes);
    p &= ~Attribute::DereferenceableOrNull;
  }

  if(p != Attribute::None)
  {
    ret = ToStr(p) + " " + ret;
    int offs = ret.indexOf('|');
    while(offs >= 0)
    {
      ret.erase((size_t)offs, 2);
      offs = ret.indexOf('|');
    }
  }

  for(const rdcpair<rdcstr, rdcstr> &str : strs)
    ret += " " + escapeString(str.first) + "=" + escapeString(str.second);

  return ret.trimmed();
}

Metadata::~Metadata()
{
  SAFE_DELETE(dwarf);
}

rdcstr Metadata::refString() const
{
  if(id == ~0U)
    return valString();
  return StringFormat::Fmt("!%u", id);
}

rdcstr Metadata::valString() const
{
  if(dwarf)
  {
    return dwarf->toString();
  }
  else if(value)
  {
    if(type == NULL)
    {
      return StringFormat::Fmt("!%s", escapeString(str).c_str());
    }
    else
    {
      if(type != val->type)
        RDCERR("Type mismatch in metadata");
      return val->toString();
    }
  }
  else
  {
    rdcstr ret = "!{";
    for(size_t i = 0; i < children.size(); i++)
    {
      if(i > 0)
        ret += ", ";
      if(!children[i])
        ret += "null";
      else if(children[i]->value)
        ret += children[i]->valString();
      else
        ret += StringFormat::Fmt("!%u", children[i]->id);
    }
    ret += "}";

    return ret;
  }
}

rdcstr Value::toString() const
{
  if(type == NULL)
    return escapeString(str);

  rdcstr ret;
  ret += type->toString() + " ";
  if(undef)
  {
    ret += "undef";
  }
  else if(symbol)
  {
    ret += StringFormat::Fmt("@%s", needsEscaping(str) ? escapeString(str).c_str() : str.c_str());
  }
  else if(type->type == Type::Scalar)
  {
    if(type->scalarType == Type::Float)
    {
      // TODO need to know how to determine signedness here
      if(type->bitWidth > 32)
        ret += StringFormat::Fmt("%lf", val.dv[0]);
      else
        ret += StringFormat::Fmt("%f", val.fv[0]);
    }
    else if(type->scalarType == Type::Int)
    {
      // TODO need to know how to determine signedness here
      if(type->bitWidth > 32)
        ret += StringFormat::Fmt("%llu", val.u64v[0]);
      else
        ret += StringFormat::Fmt("%u", val.uv[0]);
    }
  }
  else if(type->type == Type::Vector)
  {
    ret += "<";
    for(uint32_t i = 0; i < type->elemCount; i++)
    {
      if(type->scalarType == Type::Float)
      {
        // TODO need to know how to determine signedness here
        if(type->bitWidth > 32)
          ret += StringFormat::Fmt("%lf", val.dv[i]);
        else
          ret += StringFormat::Fmt("%f", val.fv[i]);
      }
      else if(type->scalarType == Type::Int)
      {
        // TODO need to know how to determine signedness here
        if(type->bitWidth > 32)
          ret += StringFormat::Fmt("%llu", val.u64v[i]);
        else
          ret += StringFormat::Fmt("%u", val.uv[i]);
      }
    }
    ret += ">";
  }
  else if(type->type == Type::Array)
  {
    ret += "[";
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i > 0)
        ret += ", ";

      ret += members[i].toString();
    }
    ret += "]";
  }
  else if(type->type == Type::Struct)
  {
    ret += "{";
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i > 0)
        ret += ", ";

      ret += members[i].toString();
    }
    ret += "}";
  }
  else
  {
    ret += StringFormat::Fmt("unsupported type %u", type->type);
  }

  return ret;
}
};    // namespace DXIL

template <>
rdcstr DoStringise(const DXIL::Attribute &el)
{
  BEGIN_BITFIELD_STRINGISE(DXIL::Attribute);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(None, "");

    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Alignment, "align");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(AlwaysInline, "alwaysinline");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ByVal, "byval");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InlineHint, "inlinehint");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InReg, "inreg");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(MinSize, "minsize");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Naked, "naked");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Nest, "nest");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoAlias, "noalias");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoBuiltin, "nobuiltin");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoCapture, "nocapture");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoDuplicate, "noduplicate");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoImplicitFloat, "noimplicitfloat");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoInline, "noinline");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NonLazyBind, "nonlazybind");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoRedZone, "noredzone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoReturn, "noreturn");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoUnwind, "nounwind");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(OptimizeForSize, "optsize");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReadNone, "readnone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReadOnly, "readonly");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Returned, "returned");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReturnsTwice, "returns_twice");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SExt, "signext");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackAlignment, "alignstack");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtect, "ssp");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtectReq, "sspreq");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtectStrong, "sspstrong");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StructRet, "sret");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeAddress, "sanitize_address");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeThread, "sanitize_thread");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeMemory, "sanitize_memory");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(UWTable, "uwtable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ZExt, "zeroext");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Builtin, "builtin");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Cold, "cold");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(OptimizeNone, "optnone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InAlloca, "inalloca");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NonNull, "nonnull");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(JumpTable, "jumptable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Dereferenceable, "dereferenceable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(DereferenceableOrNull, "dereferenceable_or_null");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Convergent, "convergent");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SafeStack, "safestack");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ArgMemOnly, "argmemonly");
  }
  END_BITFIELD_STRINGISE();
}
