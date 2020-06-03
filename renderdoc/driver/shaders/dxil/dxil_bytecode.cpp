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

static rdcstr escapeString(rdcstr str)
{
  for(size_t i = 0; i < str.size(); i++)
  {
    if(str[i] == '\r')
    {
      str[i] = 'r';
      str.insert(i, "\\", 1);
      i++;
    }
    else if(str[i] == '\n')
    {
      str[i] = 'n';
      str.insert(i, "\\", 1);
      i++;
    }
    else if(str[i] == '\t')
    {
      str[i] = 't';
      str.insert(i, "\\", 1);
      i++;
    }
    else if(str[i] == '\'' || str[i] == '\\')
    {
      str.insert(i, "\\", 1);
      i++;
    }
    else if(!isprint(str[i]))
    {
      str.insert(i + 1, StringFormat::Fmt("x%02x", str[i]));
      str[i] = '\\';
    }
  }

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
    line += " record string = '" + escapeString(record.getString()) + "'";
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
      line += " record string = '" + escapeString(record.getString()) + "'";

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
        GlobalVar g;

        // symbols refer into any of N types in declaration order
        m_Symbols.push_back({SymbolType::GlobalVar, m_GlobalVars.size()});

        // all global symbols are 'values' in LLVM, we don't need this but need to keep indexing the
        // same
        m_Values.push_back(Value());

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
        m_Values.push_back(Value());

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
        m_Values.push_back(Value());

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
            m_Types[typeIndex].type = Type::Vector;
            m_Types[typeIndex].elemCount = typ.ops[0] & 0xffffffff;
            m_Types[typeIndex].inner = &m_Types[(size_t)typ.ops[1]];

            typeIndex++;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::POINTER))
          {
            m_Types[typeIndex].type = Type::Pointer;
            m_Types[typeIndex].inner = &m_Types[(size_t)typ.ops[0]];

            if(typ.ops.size() > 1)
              RDCWARN("Ignoring address space on pointer type");

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
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::INTEGER))
          {
            Value v;
            v.type = t;
            v.val.value.u64v[0] = constant.ops[0];
            m_Values.push_back(v);
          }
          else if(IS_KNOWN(constant.id, ConstantsRecord::FLOAT))
          {
            Value v;
            v.type = t;
            if(t->bitWidth == 16)
              v.val.value.fv[0] = ConvertFromHalf(uint16_t(constant.ops[0] & 0xffff));
            else if(t->bitWidth == 32)
              memcpy(&v.val.value.fv[0], &constant.ops[0], sizeof(float));
            else
              memcpy(&v.val.value.dv[0], &constant.ops[0], sizeof(float));
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
                    v.val.value.uv[m] = m_Values[idx].val.value.uv[m];
                  else
                    v.val.value.u64v[m] = m_Values[idx].val.value.u64v[m];
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
                  v.val.members.push_back(m_Values[idx].val);
                }
                else
                {
                  v.val.members.push_back(ShaderVariable());
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
                  v.val.value.uv[m] = constant.ops[m] & ((1ULL << v.type->bitWidth) - 1);
                else
                  v.val.value.u64v[m] = constant.ops[m];
              }
            }
            else
            {
              for(size_t m = 0; m < constant.ops.size(); m++)
              {
                ShaderVariable el;
                if(v.type->bitWidth <= 32)
                  el.value.uv[0] = constant.ops[m] & ((1ULL << v.type->bitWidth) - 1);
                else
                  el.value.u64v[m] = constant.ops[m];
                v.val.members.push_back(el);
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
              case SymbolType::GlobalVar: m_GlobalVars[idx].name = symtab.getString(1); break;
              case SymbolType::Function: m_Functions[idx].name = symtab.getString(1); break;
              case SymbolType::Alias: m_Aliases[idx].name = symtab.getString(1); break;
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
        for(size_t i = 0; i < rootchild.children.size(); i++)
        {
          const LLVMBC::BlockOrRecord &meta = rootchild.children[i];
          if(IS_KNOWN(meta.id, MetaDataRecord::NAME))
          {
            rdcstr metaName = meta.getString();
            i++;
            const LLVMBC::BlockOrRecord &namedNode = rootchild.children[i];
            RDCASSERT(IS_KNOWN(namedNode.id, MetaDataRecord::NAMED_NODE));

            rdcstr namedMeta = StringFormat::Fmt("!%s = !{", metaName.c_str());

            bool first = true;
            for(uint64_t op : namedNode.ops)
            {
              if(!first)
                namedMeta += ", ";
              namedMeta += ToStr(op);
              first = false;
            }
            namedMeta += "}";

            RDCLOG("%s", namedMeta.c_str());
          }
          else
          {
            if(IS_KNOWN(meta.id, MetaDataRecord::KIND))
            {
              size_t kind = (size_t)meta.ops[0];
              m_Kinds.resize(RDCMAX(m_Kinds.size(), kind + 1));
              m_Kinds[kind] = meta.getString(1);
              continue;
            }

            rdcstr metastr = StringFormat::Fmt("!%u = ", (uint32_t)i);

            auto getMetaString = [&rootchild](uint64_t id) -> rdcstr {
              return id ? rootchild.children[size_t(id - 1)].getString() : "NULL";
            };

            if(IS_KNOWN(meta.id, MetaDataRecord::STRING_OLD))
            {
              metastr += "\"" + escapeString(meta.getString()) + "\"";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::FILE))
            {
              if(meta.ops[0])
                metastr += "distinct ";

              metastr += "!DIFile(";
              metastr += StringFormat::Fmt("filename: \"%s\"",
                                           escapeString(getMetaString(meta.ops[1])).c_str());
              metastr += StringFormat::Fmt(", directory: \"%s\"",
                                           escapeString(getMetaString(meta.ops[2])).c_str());
              metastr += ")";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::NODE) ||
                    IS_KNOWN(meta.id, MetaDataRecord::DISTINCT_NODE))
            {
              if(IS_KNOWN(meta.id, MetaDataRecord::DISTINCT_NODE))
                metastr += "distinct ";

              metastr += "!{";
              bool first = true;
              for(uint64_t op : meta.ops)
              {
                if(!first)
                  metastr += ", ";
                metastr += ToStr(op - 1);
                first = false;
              }
              metastr += "}";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::BASIC_TYPE))
            {
              metastr += "!DIBasicType()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::DERIVED_TYPE))
            {
              metastr += "!DIDerivedType()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::COMPOSITE_TYPE))
            {
              metastr += "!DICompositeType()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::SUBROUTINE_TYPE))
            {
              metastr += "!DISubroutineType()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::TEMPLATE_TYPE))
            {
              metastr += "!DITemplateTypeParameter()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::TEMPLATE_VALUE))
            {
              metastr += "!DITemplateValueParameter()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::SUBPROGRAM))
            {
              metastr += "!DISubprogram()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::LOCATION))
            {
              metastr += "!DILocation()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::LOCAL_VAR))
            {
              metastr += "!DILocalVariable()";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::VALUE))
            {
              // need to decode CONSTANTS_BLOCK and TYPE_BLOCK for this
              metastr += StringFormat::Fmt("!{values[%llu] interpreted as types[%llu]}",
                                           meta.ops[1], meta.ops[0]);
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::EXPRESSION))
            {
              // don't decode this yet
              metastr += "!DIExpression(";
              bool first = true;
              for(uint64_t op : meta.ops)
              {
                if(!first)
                  metastr += ", ";
                metastr += ToStr(op);
                first = false;
              }
              metastr += ")";
            }
            else if(IS_KNOWN(meta.id, MetaDataRecord::COMPILE_UNIT))
            {
              // should be at least 14 parameters
              RDCASSERT(meta.ops.size() >= 14);

              // we expect it to be marked as distinct, but we'll always treat it that way
              if(meta.ops[0])
                metastr += "distinct ";
              else
                metastr += "distinct? ";

              metastr += "!DICompileUnit(";
              {
                metastr += StringFormat::Fmt(
                    "language: %s", meta.ops[1] == 0x4 ? "DW_LANG_C_plus_plus" : "DW_LANG_unknown");
                metastr += StringFormat::Fmt(", file: !%llu", meta.ops[2] - 1);
                metastr += StringFormat::Fmt(", producer: \"%s\"",
                                             escapeString(getMetaString(meta.ops[3])).c_str());
                metastr += StringFormat::Fmt(", isOptimized: %s", meta.ops[4] ? "true" : "false");
                metastr += StringFormat::Fmt(", flags: \"%s\"",
                                             escapeString(getMetaString(meta.ops[5])).c_str());
                metastr += StringFormat::Fmt(", runtimeVersion: %llu", meta.ops[6]);
                metastr += StringFormat::Fmt(", splitDebugFilename: \"%s\"",
                                             escapeString(getMetaString(meta.ops[7])).c_str());
                metastr += StringFormat::Fmt(", emissionKind: %llu", meta.ops[8]);
                metastr += StringFormat::Fmt(", enums: !%llu", meta.ops[9] - 1);
                metastr += StringFormat::Fmt(", retainedTypes: !%llu", meta.ops[10] - 1);
                metastr += StringFormat::Fmt(", subprograms: !%llu", meta.ops[11] - 1);
                metastr += StringFormat::Fmt(", globals: !%llu", meta.ops[12] - 1);
                metastr += StringFormat::Fmt(", imports: !%llu", meta.ops[13] - 1);
                if(meta.ops.size() >= 15)
                  metastr += StringFormat::Fmt(", dwoId: 0x%llu", meta.ops[14]);
              }
              metastr += ")";
            }
            else
            {
              RDCERR("unhandled metadata type %u", meta.id);
            }

            RDCLOG("%s", metastr.c_str());
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

  dumpBlock(root, 0);
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

uint32_t Program::GetDisassemblyLine(uint32_t instruction) const
{
  return 0;
}

void Program::MakeDisassemblyString()
{
  RDCWARN("Unimplemented DXIL::Program::MakeDisassemblyString()");

  const char *shaderName[] = {
      "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
      "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
      "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
  };

  m_Disassembly = StringFormat::Fmt("; %s Shader, compiled under SM%u.%u\n\n",
                                    shaderName[int(m_Type)], m_Major, m_Minor);
  m_Disassembly += StringFormat::Fmt("target triple = \"%s\"\n", m_Triple.c_str());
  m_Disassembly += StringFormat::Fmt("target datalayout = \"%s\"\n\n", m_Datalayout.c_str());

  bool typesPrinted = false;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    const Type &typ = m_Types[i];

    if(typ.type == Type::Struct && !typ.name.empty())
    {
      rdcstr name = typ.getTypeName();
      m_Disassembly += StringFormat::Fmt("%s = type {", name.c_str());
      bool first = true;
      for(const Type *t : typ.members)
      {
        if(!first)
          m_Disassembly += ", ";
        first = false;
        m_Disassembly += StringFormat::Fmt(" %s", t->getTypeName().c_str());
      }
      m_Disassembly += " }\n";
      typesPrinted = true;
    }
  }

  if(typesPrinted)
    m_Disassembly += "\n";

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = m_Functions[i];

    if(func.attrs)
      m_Disassembly += StringFormat::Fmt("; Function Attrs: %s\n", func.attrs->toString().c_str());

    m_Disassembly += (func.external ? "declare " : "define ");
    m_Disassembly += func.funcType->declFunction("@" + func.name);

    if(func.attrs)
      m_Disassembly += StringFormat::Fmt(" #%u", func.attrs->index);

    if(!func.external)
    {
      m_Disassembly += " {\n";
      m_Disassembly += "  ; ...\n";
      m_Disassembly += "}\n\n";
    }
    else
    {
      m_Disassembly += "\n\n";
    }
  }

  for(size_t i = 0; i < m_Attributes.size(); i++)
    m_Disassembly +=
        StringFormat::Fmt("attributes #%zu = %s\n", i, m_Attributes[i].toString().c_str());

  m_Disassembly += "; No disassembly implemented";
}

rdcstr Type::getTypeName() const
{
  if(!name.empty())
  {
    // needs escaping
    if(name.find_first_not_of(
           "-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ$._0123456789") >= 0)
    {
      return "%\"" + escapeString(name) + "\"";
    }

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
    case Vector: return StringFormat::Fmt("<%u x %s>", elemCount, inner->getTypeName().c_str());
    case Pointer: return StringFormat::Fmt("%s*", inner->getTypeName().c_str());
    case Array: return StringFormat::Fmt("[%u x %s]", elemCount, inner->getTypeName().c_str());
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
        ret += members[i]->getTypeName();
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
  rdcstr ret = inner->getTypeName();
  ret += " " + funcName + "(";
  for(size_t i = 0; i < members.size(); i++)
  {
    if(i > 0)
      ret += ", ";
    ret += members[i]->getTypeName();
  }
  ret += ")";
  return ret;
}

rdcstr Attributes::toString() const
{
  rdcstr ret = "{";
  Attribute p = params;

  if(p & Attribute::Alignment)
  {
    ret += StringFormat::Fmt(" Alignment(%llu)", align);
    p &= ~Attribute::Alignment;
  }
  if(p & Attribute::StackAlignment)
  {
    ret += StringFormat::Fmt(" StackAlignment(%llu)", stackAlign);
    p &= ~Attribute::StackAlignment;
  }
  if(p & Attribute::Dereferenceable)
  {
    ret += StringFormat::Fmt(" Dereferenceable(%llu)", derefBytes);
    p &= ~Attribute::Dereferenceable;
  }
  if(p & Attribute::DereferenceableOrNull)
  {
    ret += StringFormat::Fmt(" DereferenceableOrNull(%llu)", derefOrNullBytes);
    p &= ~Attribute::DereferenceableOrNull;
  }

  if(p != Attribute::None)
    ret += " " + ToStr(p);

  for(const rdcpair<rdcstr, rdcstr> &str : strs)
    ret += " " + str.first + "=" + str.second;
  ret += " }";

  return ret;
}
};

template <>
rdcstr DoStringise(const DXIL::Attribute &el)
{
  BEGIN_BITFIELD_STRINGISE(DXIL::Attribute);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(None, "");

    STRINGISE_BITFIELD_CLASS_BIT(Alignment);
    STRINGISE_BITFIELD_CLASS_BIT(AlwaysInline);
    STRINGISE_BITFIELD_CLASS_BIT(ByVal);
    STRINGISE_BITFIELD_CLASS_BIT(InlineHint);
    STRINGISE_BITFIELD_CLASS_BIT(InReg);
    STRINGISE_BITFIELD_CLASS_BIT(MinSize);
    STRINGISE_BITFIELD_CLASS_BIT(Naked);
    STRINGISE_BITFIELD_CLASS_BIT(Nest);
    STRINGISE_BITFIELD_CLASS_BIT(NoAlias);
    STRINGISE_BITFIELD_CLASS_BIT(NoBuiltin);
    STRINGISE_BITFIELD_CLASS_BIT(NoCapture);
    STRINGISE_BITFIELD_CLASS_BIT(NoDuplicate);
    STRINGISE_BITFIELD_CLASS_BIT(NoImplicitFloat);
    STRINGISE_BITFIELD_CLASS_BIT(NoInline);
    STRINGISE_BITFIELD_CLASS_BIT(NonLazyBind);
    STRINGISE_BITFIELD_CLASS_BIT(NoRedZone);
    STRINGISE_BITFIELD_CLASS_BIT(NoReturn);
    STRINGISE_BITFIELD_CLASS_BIT(NoUnwind);
    STRINGISE_BITFIELD_CLASS_BIT(OptimizeForSize);
    STRINGISE_BITFIELD_CLASS_BIT(ReadNone);
    STRINGISE_BITFIELD_CLASS_BIT(ReadOnly);
    STRINGISE_BITFIELD_CLASS_BIT(Returned);
    STRINGISE_BITFIELD_CLASS_BIT(ReturnsTwice);
    STRINGISE_BITFIELD_CLASS_BIT(SExt);
    STRINGISE_BITFIELD_CLASS_BIT(StackAlignment);
    STRINGISE_BITFIELD_CLASS_BIT(StackProtect);
    STRINGISE_BITFIELD_CLASS_BIT(StackProtectReq);
    STRINGISE_BITFIELD_CLASS_BIT(StackProtectStrong);
    STRINGISE_BITFIELD_CLASS_BIT(StructRet);
    STRINGISE_BITFIELD_CLASS_BIT(SanitizeAddress);
    STRINGISE_BITFIELD_CLASS_BIT(SanitizeThread);
    STRINGISE_BITFIELD_CLASS_BIT(SanitizeMemory);
    STRINGISE_BITFIELD_CLASS_BIT(UWTable);
    STRINGISE_BITFIELD_CLASS_BIT(ZExt);
    STRINGISE_BITFIELD_CLASS_BIT(Builtin);
    STRINGISE_BITFIELD_CLASS_BIT(Cold);
    STRINGISE_BITFIELD_CLASS_BIT(OptimizeNone);
    STRINGISE_BITFIELD_CLASS_BIT(InAlloca);
    STRINGISE_BITFIELD_CLASS_BIT(NonNull);
    STRINGISE_BITFIELD_CLASS_BIT(JumpTable);
    STRINGISE_BITFIELD_CLASS_BIT(Dereferenceable);
    STRINGISE_BITFIELD_CLASS_BIT(DereferenceableOrNull);
    STRINGISE_BITFIELD_CLASS_BIT(Convergent);
    STRINGISE_BITFIELD_CLASS_BIT(SafeStack);
    STRINGISE_BITFIELD_CLASS_BIT(ArgMemOnly);
  }
  END_BITFIELD_STRINGISE();
}
