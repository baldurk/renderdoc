/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Baldur Karlsson
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

#include "llvm_encoder.h"
#include "os/os_specific.h"

namespace LLVMBC
{
static bool isChar6(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= '|') || (c >= '0' && c <= '9') || c == '_' ||
         c == '.';
}

static uint32_t GetBlockAbbrevSize(KnownBlock block)
{
  uint32_t ret = 0;

  // the abbrev sizes seem to be hardcoded in llvm? At least this matches dxc's llvm
  switch(block)
  {
    case KnownBlock::BLOCKINFO: ret = 2; break;
    case KnownBlock::MODULE_BLOCK: ret = 3; break;
    case KnownBlock::PARAMATTR_BLOCK: ret = 3; break;
    case KnownBlock::PARAMATTR_GROUP_BLOCK: ret = 3; break;
    case KnownBlock::CONSTANTS_BLOCK: ret = 4; break;
    case KnownBlock::FUNCTION_BLOCK: ret = 4; break;
    case KnownBlock::VALUE_SYMTAB_BLOCK: ret = 4; break;
    case KnownBlock::METADATA_BLOCK: ret = 3; break;
    case KnownBlock::METADATA_ATTACHMENT: ret = 3; break;
    case KnownBlock::TYPE_BLOCK: ret = 4; break;
    case KnownBlock::USELIST_BLOCK: ret = 3; break;
    case KnownBlock::Count: break;
  }

  return ret;
}

#define MagicFixedSizeNumTypes 99

#define AbbFixed(n)          \
  {                          \
    AbbrevEncoding::Fixed, n \
  }
#define AbbVBR(n)          \
  {                        \
    AbbrevEncoding::VBR, n \
  }
#define AbbArray()           \
  {                          \
    AbbrevEncoding::Array, 0 \
  }
#define AbbLiteral(lit)                    \
  {                                        \
    AbbrevEncoding::Literal, uint64_t(lit) \
  }
#define AbbChar6()           \
  {                          \
    AbbrevEncoding::Char6, 0 \
  }

#define AbbFixedTypes() AbbFixed(MagicFixedSizeNumTypes)

using AbbrevDefinition = AbbrevParam[8];

// known abbreviations. Encoded as an array of abbrevs, with each one being an array of params (the
// last param will have AbbrevEncoding::Unknown == 0)
enum class ValueSymtabAbbrev
{
  Entry8,
  Entry7,
  Entry6,
  BBEntry6,
};

AbbrevDefinition ValueSymtabAbbrevDefs[] = {
    // Entry8
    {
        AbbFixed(3), AbbVBR(8), AbbArray(), AbbFixed(8),
    },
    // Entry7
    {
        AbbLiteral(ValueSymtabRecord::ENTRY), AbbVBR(8), AbbArray(), AbbFixed(7),
    },
    // Entry6
    {
        AbbLiteral(ValueSymtabRecord::ENTRY), AbbVBR(8), AbbArray(), AbbChar6(),
    },
    // BBEntry6
    {
        AbbLiteral(ValueSymtabRecord::BBENTRY), AbbVBR(8), AbbArray(), AbbChar6(),
    },
};

enum class ConstantsAbbrev
{
  SetType,
  Integer,
  EvalCast,
  Null,
};

AbbrevDefinition ConstantsAbbrevDefs[] = {
    // SetType
    {
        AbbLiteral(ConstantsRecord::SETTYPE), AbbFixedTypes(),
    },
    // Integer
    {
        AbbLiteral(ConstantsRecord::INTEGER), AbbVBR(8),
    },
    // EvalCast
    {
        AbbLiteral(ConstantsRecord::EVAL_CAST), AbbFixed(4), AbbFixedTypes(), AbbVBR(8),
    },
    // Null
    {
        AbbLiteral(ConstantsRecord::CONST_NULL),
    },
};

enum class FunctionAbbrev
{
  Load,
  BinOp,
  BinOpFlags,
  Cast,
  RetVoid,
  RetValue,
  Unreachable,
  GEP,
};

AbbrevDefinition FunctionAbbrevDefs[] = {
    // Load
    {
        AbbLiteral(FunctionRecord::INST_LOAD), AbbVBR(6), AbbFixedTypes(), AbbVBR(4), AbbFixed(1),
    },
    // BinOp
    {
        AbbLiteral(FunctionRecord::INST_BINOP), AbbVBR(6), AbbVBR(6), AbbFixed(4),
    },
    // BinOpFlags
    {
        AbbLiteral(FunctionRecord::INST_BINOP), AbbVBR(6), AbbVBR(6), AbbFixed(4), AbbFixed(7),
    },
    // Cast
    {
        AbbLiteral(FunctionRecord::INST_CAST), AbbVBR(6), AbbFixedTypes(), AbbFixed(4),
    },
    // RetVoid
    {
        AbbLiteral(FunctionRecord::INST_RET),
    },
    // RetValue
    {
        AbbLiteral(FunctionRecord::INST_RET), AbbVBR(6),
    },
    // Unreachable
    {
        AbbLiteral(FunctionRecord::INST_UNREACHABLE),
    },
    // GEP
    {
        AbbLiteral(FunctionRecord::INST_GEP), AbbFixed(1), AbbFixedTypes(), AbbArray(), AbbVBR(6),
    },
};

enum class TypeAbbrev
{
  Pointer,
  Function,
  AnonStruct,
  StructName,
  NamedStruct,
  Array,
};

AbbrevDefinition TypeAbbrevDefs[] = {
    // Pointer
    {
        AbbLiteral(TypeRecord::POINTER), AbbFixedTypes(), AbbLiteral(0),
    },
    // Function
    {
        AbbLiteral(TypeRecord::FUNCTION), AbbFixed(1), AbbArray(), AbbFixedTypes(),
    },
    // AnonStruct
    {
        AbbLiteral(TypeRecord::STRUCT_ANON), AbbFixed(1), AbbArray(), AbbFixedTypes(),
    },
    // StructName
    {
        AbbLiteral(TypeRecord::STRUCT_NAME), AbbArray(), AbbChar6(),
    },
    // NamedStruct
    {
        AbbLiteral(TypeRecord::STRUCT_NAMED), AbbFixed(1), AbbArray(), AbbFixedTypes(),
    },
    // Array
    {
        AbbLiteral(TypeRecord::ARRAY), AbbVBR(8), AbbFixedTypes(),
    },
};

static AbbrevDefinition *GetAbbrevDefs(KnownBlock block)
{
  AbbrevDefinition *ret = NULL;

  switch(block)
  {
    case KnownBlock::VALUE_SYMTAB_BLOCK: ret = ValueSymtabAbbrevDefs; break;
    case KnownBlock::CONSTANTS_BLOCK: ret = ConstantsAbbrevDefs; break;
    case KnownBlock::FUNCTION_BLOCK: ret = FunctionAbbrevDefs; break;
    case KnownBlock::TYPE_BLOCK: ret = TypeAbbrevDefs; break;
    default: break;
  }

  return ret;
}

static uint32_t GetNumAbbrevDefs(KnownBlock block)
{
  uint32_t ret = 0;

  switch(block)
  {
    case KnownBlock::VALUE_SYMTAB_BLOCK: ret = ARRAY_COUNT(ValueSymtabAbbrevDefs); break;
    case KnownBlock::CONSTANTS_BLOCK: ret = ARRAY_COUNT(ConstantsAbbrevDefs); break;
    case KnownBlock::FUNCTION_BLOCK: ret = ARRAY_COUNT(FunctionAbbrevDefs); break;
    case KnownBlock::TYPE_BLOCK: ret = ARRAY_COUNT(TypeAbbrevDefs); break;
    default: break;
  }

  return ret;
}

BitcodeWriter::BitcodeWriter(bytebuf &buf) : b(buf)
{
  b.Write(LLVMBC::BitcodeMagic);

  curBlock = KnownBlock::Count;
  abbrevSize = 2;

  numAbbrevs = 0;

  m_GlobalVarAbbrev = ~0U;
}

BitcodeWriter::~BitcodeWriter()
{
}

void BitcodeWriter::BeginBlock(KnownBlock block)
{
  uint32_t newAbbrevSize = GetBlockAbbrevSize(block);

  if(newAbbrevSize == 0)
  {
    RDCERR("Encoding error: unrecognised block %u", block);
    return;
  }

  b.fixed(abbrevSize, ENTER_SUBBLOCK);
  b.vbr(8, block);
  b.vbr(4, newAbbrevSize);
  b.align32bits();

  size_t offs = b.GetByteOffset();

  // write a placeholder length
  b.Write<uint32_t>(0U);

  curBlock = block;
  abbrevSize = newAbbrevSize;
  blockStack.push_back({block, offs, numAbbrevs});

  // emit known abbrevs here that aren't in blockinfo
  switch(block)
  {
    case KnownBlock::VALUE_SYMTAB_BLOCK:
    case KnownBlock::CONSTANTS_BLOCK:
    case KnownBlock::FUNCTION_BLOCK:
    {
      // these blocks have abbrevs but we shouldn't emit them as they're handled in blockinfo
      break;
    }
    default:
    {
      uint32_t numAbbrevDefs = GetNumAbbrevDefs(block);
      AbbrevDefinition *abbrevs = GetAbbrevDefs(block);
      for(uint32_t i = 0; i < numAbbrevDefs; i++)
        WriteAbbrevDefinition(abbrevs[i]);
      break;
    }
  }
}

void BitcodeWriter::EndBlock()
{
  b.vbr(abbrevSize, END_BLOCK);
  b.align32bits();

  size_t offs = blockStack.back().offset;

  // -4 because we don't include the word with the length itself
  size_t lengthInBytes = b.GetByteOffset() - offs - 4;

  b.PatchLengthWord(offs, uint32_t(lengthInBytes / 4));

  blockStack.pop_back();
  if(blockStack.empty())
  {
    curBlock = KnownBlock::Count;
    abbrevSize = 2;
  }
  else
  {
    curBlock = blockStack.back().block;
    numAbbrevs = blockStack.back().numAbbrevs;
    abbrevSize = GetBlockAbbrevSize(curBlock);
  }
}

void BitcodeWriter::WriteAbbrevDefinition(AbbrevParam *abbrev)
{
  numAbbrevs++;

  b.fixed(abbrevSize, DEFINE_ABBREV);

  uint32_t numParams = 0;
  while(abbrev[numParams].encoding != AbbrevEncoding::Unknown)
    numParams++;

  b.vbr(5, numParams);

  for(uint32_t p = 0; p < numParams; p++)
  {
    AbbrevParam param = abbrev[p];

    if(param.value == MagicFixedSizeNumTypes)
      param.value = m_NumTypeBits;

    const bool lit = param.encoding == AbbrevEncoding::Literal;
    b.fixed<bool>(1, lit);
    if(lit)
    {
      b.vbr(8, param.value);
    }
    else
    {
      b.fixed(3, param.encoding);
      if(param.encoding == AbbrevEncoding::VBR || param.encoding == AbbrevEncoding::Fixed)
        b.vbr(5, param.value);
    }
  }
}

void BitcodeWriter::ConfigureSizes(size_t numTypes, size_t numSections, uint64_t maxAlign,
                                   uint32_t maxGlobalType)
{
  m_NumTypeBits = 32 - Bits::CountLeadingZeroes((uint32_t)numTypes);

  m_GlobalTypeBits = 32 - Bits::CountLeadingZeroes(maxGlobalType);

  if(numSections == 0)
    m_NumSectionBits = 0;
  else
    m_NumSectionBits = 32 - Bits::CountLeadingZeroes((uint32_t)numSections);

  if(maxAlign == 0)
  {
    m_AlignBits = 0;
  }
  else
  {
    uint32_t encodedAlign = 32 - Bits::CountLeadingZeroes((uint32_t)maxAlign);
    m_AlignBits = 32 - Bits::CountLeadingZeroes(encodedAlign);
  }
}

void BitcodeWriter::ModuleBlockInfo()
{
  // these abbrevs are hardcoded in llvm, at least at dxc's version
  BeginBlock(KnownBlock::BLOCKINFO);

  // the module-level blockinfo contains abbrevs for these block types that can be repeated
  // subblocks
  for(KnownBlock block :
      {KnownBlock::VALUE_SYMTAB_BLOCK, KnownBlock::CONSTANTS_BLOCK, KnownBlock::FUNCTION_BLOCK})
  {
    Unabbrev((uint32_t)BlockInfoRecord::SETBID, (uint32_t)block);
    AbbrevDefinition *abbrevs = GetAbbrevDefs(block);
    uint32_t numAbbrevDefs = GetNumAbbrevDefs(block);

    for(uint32_t i = 0; i < numAbbrevDefs; i++)
      WriteAbbrevDefinition(abbrevs[i]);
  }

  EndBlock();
}

void BitcodeWriter::EmitGlobalVarAbbrev()
{
  m_GlobalVarAbbrev = numAbbrevs;

  AbbrevParam align = AbbFixed(m_AlignBits);
  if(m_AlignBits == 0)
    align = AbbLiteral(0);

  AbbrevParam section = AbbFixed(m_NumSectionBits);
  if(m_NumSectionBits == 0)
    section = AbbLiteral(0);

  m_GlobalVarAbbrevDef[0] = AbbLiteral(ModuleRecord::GLOBALVAR);
  m_GlobalVarAbbrevDef[1] = AbbFixed(m_GlobalTypeBits);
  m_GlobalVarAbbrevDef[2] = AbbVBR(6);
  m_GlobalVarAbbrevDef[3] = AbbVBR(6);
  m_GlobalVarAbbrevDef[4] = AbbFixed(5);
  m_GlobalVarAbbrevDef[5] = align;
  m_GlobalVarAbbrevDef[6] = section;

  WriteAbbrevDefinition(m_GlobalVarAbbrevDef);
}

uint32_t BitcodeWriter::GetAbbrevID(uint32_t id)
{
  // the id is a block-local index, starting from 0, of the abbrevs defined for that block.

  // when we begin a block, we store the number of previously defined abbrevs from earlier blocks so
  // we add that on
  id += blockStack.back().numAbbrevs;

  // the ID we need to encode starts at APPLICATION_ABBREV for the first one, so add that
  return APPLICATION_ABBREV + id;
}

void BitcodeWriter::AutoRecord(uint32_t record, bool param, uint64_t val)
{
  AbbrevDefinition *abbrevs = GetAbbrevDefs(curBlock);
  uint32_t numAbbrevDefs = GetNumAbbrevDefs(curBlock);

  uint32_t idx = numAbbrevDefs;

  // the records with abbrevs are hardcoded, so just determine if this record in this block has an
  // abbrev and select it here
  switch(curBlock)
  {
    case KnownBlock::VALUE_SYMTAB_BLOCK:
      RDCERR("Symbol table entry needs multiple parameters");
      break;
    case KnownBlock::MODULE_BLOCK:
      switch(ModuleRecord(record))
      {
        case ModuleRecord::GLOBALVAR: RDCERR("global var needs multiple parameters"); break;
        default: break;
      }
      break;
    case KnownBlock::TYPE_BLOCK:
      switch(TypeRecord(record))
      {
        case TypeRecord::POINTER: RDCERR("Pointer type needs multiple parameters"); break;
        case TypeRecord::FUNCTION: idx = (uint32_t)TypeAbbrev::Function; break;
        case TypeRecord::STRUCT_ANON: idx = (uint32_t)TypeAbbrev::AnonStruct; break;
        case TypeRecord::STRUCT_NAME: idx = (uint32_t)TypeAbbrev::StructName; break;
        case TypeRecord::STRUCT_NAMED: idx = (uint32_t)TypeAbbrev::NamedStruct; break;
        case TypeRecord::ARRAY: idx = (uint32_t)TypeAbbrev::Array; break;
        default: break;
      }
      break;
    default: break;
  }

  // if we got a valid abbrev, use it, otherwise emit unabbrev
  if(idx < numAbbrevDefs)
    Abbrev(abbrevs[idx], GetAbbrevID(record), val);
  else
    Unabbrev(record, param, val);
}

void BitcodeWriter::AutoRecord(uint32_t record, const rdcarray<uint64_t> &vals)
{
  AbbrevDefinition *abbrevs = GetAbbrevDefs(curBlock);
  uint32_t numAbbrevDefs = GetNumAbbrevDefs(curBlock);

  uint32_t idx = ~0U;

  // the records with abbrevs are hardcoded, so just determine if this record in this block has an
  // abbrev and select it here
  switch(curBlock)
  {
    case KnownBlock::VALUE_SYMTAB_BLOCK:
      // the selection of abbrev here depends on the data
      break;
    case KnownBlock::MODULE_BLOCK:
      switch(ModuleRecord(record))
      {
        case ModuleRecord::GLOBALVAR:
        {
          idx = m_GlobalVarAbbrev;
          // if any of the later values are non-zero, can't use the global var abbrev
          for(size_t i = 6; i < vals.size(); i++)
            if(vals[i] != 0)
              idx = ~0U;
          break;
        }
        default: break;
      }
      break;
    case KnownBlock::TYPE_BLOCK:
      switch(TypeRecord(record))
      {
        case TypeRecord::POINTER:
          // only use pointer abbrev if address space is 0
          if(vals.size() == 2 && vals[1] == 0)
            idx = (uint32_t)TypeAbbrev::Pointer;
          break;
        case TypeRecord::FUNCTION: idx = (uint32_t)TypeAbbrev::Function; break;
        case TypeRecord::STRUCT_ANON: idx = (uint32_t)TypeAbbrev::AnonStruct; break;
        case TypeRecord::STRUCT_NAME:
        {
          idx = (uint32_t)TypeAbbrev::StructName;
          for(size_t i = 0; i < vals.size(); i++)
          {
            if(!isChar6(char(vals[i])))
            {
              idx = ~0U;
              break;
            }
          }
          break;
        }
        case TypeRecord::STRUCT_NAMED: idx = (uint32_t)TypeAbbrev::NamedStruct; break;
        case TypeRecord::ARRAY: idx = (uint32_t)TypeAbbrev::Array; break;
        default: break;
      }
      break;
    default: break;
  }

  // if we got a valid abbrev, use it, otherwise emit unabbrev
  if(idx == m_GlobalVarAbbrev && idx != ~0U)
  {
    // write the abbrev ID
    b.fixed(abbrevSize, GetAbbrevID(m_GlobalVarAbbrev));

    Abbrev(m_GlobalVarAbbrevDef, record, vals);
  }
  else if(idx < numAbbrevDefs)
  {
    // write the abbrev ID
    b.fixed(abbrevSize, GetAbbrevID(idx));

    Abbrev(abbrevs[idx], record, vals);
  }
  else
  {
    Unabbrev(record, vals);
  }
}

void BitcodeWriter::Abbrev(AbbrevParam *abbr, uint32_t record, uint64_t val)
{
  WriteAbbrevParam(abbr[0], record);
  // if this abbrev has a parameter, encode it - it may be parameterless in which case we ignore val
  if(abbr[1].encoding != AbbrevEncoding::Unknown)
    WriteAbbrevParam(abbr[1], val);
}

void BitcodeWriter::Abbrev(AbbrevParam *abbr, uint32_t record, const rdcarray<uint64_t> &vals)
{
  WriteAbbrevParam(abbr[0], record);

  size_t i = 0;
  abbr++;
  while(abbr->encoding != AbbrevEncoding::Unknown)
  {
    RDCASSERT(i < vals.size());

    // only one array per abbrev, consume the rest of the vals
    if(abbr->encoding == AbbrevEncoding::Array)
    {
      abbr++;
      RDCASSERT(abbr->encoding != AbbrevEncoding::Unknown);

      b.vbr(6, vals.size() - i);

      for(; i < vals.size(); i++)
        WriteAbbrevParam(abbr[0], vals[i]);

      // end now
      break;
    }
    else if(abbr->encoding == AbbrevEncoding::Blob)
    {
      // expect vals to be length then blob pointer packed into uint64_t
      size_t length = (size_t)vals[i];
      byte *blob = (byte *)vals[i + 1];
      b.WriteBlob(blob, length);
    }
    else
    {
      WriteAbbrevParam(abbr[0], vals[i++]);
      abbr++;
    }
  }
}

void BitcodeWriter::WriteAbbrevParam(const AbbrevParam &abbrev, uint64_t val)
{
  // if the encoding is a literal we don't have to do anything
  if(abbrev.encoding == AbbrevEncoding::Literal)
    return;

  if(abbrev.encoding == AbbrevEncoding::Fixed)
    b.fixed(abbrev.value == MagicFixedSizeNumTypes ? m_NumTypeBits : abbrev.value, val);
  else if(abbrev.encoding == AbbrevEncoding::VBR)
    b.vbr(abbrev.value, val);
  else if(abbrev.encoding == AbbrevEncoding::Char6)
    b.c6(char(val));
  else
    RDCERR("Unexpected abbrev param type: %d", abbrev.encoding);
}

void BitcodeWriter::Unabbrev(uint32_t record, bool param, uint64_t val)
{
  b.fixed(abbrevSize, UNABBREV_RECORD);
  b.vbr(6, record);
  b.vbr(6, param ? 1U : 0U);
  if(param)
    b.vbr(6, val);
}

void BitcodeWriter::Unabbrev(uint32_t record, const rdcarray<uint64_t> &vals)
{
  b.fixed(abbrevSize, UNABBREV_RECORD);
  b.vbr(6, record);
  b.vbr(6, vals.size());
  for(uint64_t v : vals)
    b.vbr(6, v);
}
};

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

#include "llvm_decoder.h"

TEST_CASE("Check LLVM bitwriter", "[llvm]")
{
  bytebuf bits;

  SECTION("Check simple writing of bytes")
  {
    LLVMBC::BitWriter w(bits);

    w.Write<byte>(0x01);
    w.Write<byte>(0x02);
    w.Write<byte>(0x40);
    w.Write<byte>(0x80);
    w.Write<byte>(0xff);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    CHECK(r.Read<byte>() == 0x01);
    CHECK(r.Read<byte>() == 0x02);
    CHECK(r.Read<byte>() == 0x40);
    CHECK(r.Read<byte>() == 0x80);
    CHECK(r.Read<byte>() == 0xff);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check fixed encoding")
  {
    uint32_t val = 0x3CA5F096;

    LLVMBC::BitWriter w(bits);

    for(uint32_t i = 0; i < 32; i++)
      w.fixed(i + 1, val);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    for(uint32_t i = 0; i < 32; i++)
    {
      CHECK(r.fixed<uint32_t>(i + 1) == (val & ((1ULL << (i + 1)) - 1)));
    }

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check variable encoding")
  {
    LLVMBC::BitWriter w(bits);

    w.vbr<uint32_t>(8, 0x12);
    w.vbr<uint32_t>(6, 0x12);
    w.vbr<uint32_t>(5, 0x12);
    w.vbr<uint32_t>(4, 0x12);
    w.vbr<uint32_t>(3, 0x12);

    w.vbr<uint32_t>(8, 0x12345678);
    w.vbr<uint32_t>(6, 0x12345678);
    w.vbr<uint32_t>(5, 0x12345678);
    w.vbr<uint32_t>(4, 0x12345678);
    w.vbr<uint32_t>(3, 0x12345678);

    w.vbr<uint64_t>(8, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(6, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(5, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(4, 0x123456789ABCDEFULL);
    w.vbr<uint64_t>(3, 0x123456789ABCDEFULL);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    CHECK(r.vbr<uint32_t>(8) == 0x12);
    CHECK(r.vbr<uint32_t>(6) == 0x12);
    CHECK(r.vbr<uint32_t>(5) == 0x12);
    CHECK(r.vbr<uint32_t>(4) == 0x12);
    CHECK(r.vbr<uint32_t>(3) == 0x12);

    CHECK(r.vbr<uint32_t>(8) == 0x12345678);
    CHECK(r.vbr<uint32_t>(6) == 0x12345678);
    CHECK(r.vbr<uint32_t>(5) == 0x12345678);
    CHECK(r.vbr<uint32_t>(4) == 0x12345678);
    CHECK(r.vbr<uint32_t>(3) == 0x12345678);

    CHECK(r.vbr<uint64_t>(8) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(6) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(5) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(4) == 0x123456789ABCDEFULL);
    CHECK(r.vbr<uint64_t>(3) == 0x123456789ABCDEFULL);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check signed vbr encoding")
  {
    LLVMBC::BitWriter w(bits);

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(0x12));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-0x12));

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(0x1234));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-0x1234));

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(0x12345678));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-0x12345678));

    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(INT_MAX));
    w.vbr<uint64_t>(4, LLVMBC::BitWriter::svbr(-INT_MAX));

    w.align32bits();

    CHECK(bits.size() == 28);

    LLVMBC::BitReader r(bits.data(), bits.size());

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == 0x12);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -0x12);

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == 0x1234);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -0x1234);

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == 0x12345678);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -0x12345678);

    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == INT_MAX);
    CHECK(LLVMBC::BitReader::svbr(r.vbr<uint32_t>(4)) == -INT_MAX);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check char6 encoding")
  {
    LLVMBC::BitWriter w(bits);

    const char string[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";

    for(size_t i = 0; i < strlen(string); i++)
      w.c6(string[i]);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    for(size_t i = 0; i < strlen(string); i++)
      CHECK(r.c6() == string[i]);

    r.align32bits();

    CHECK(r.AtEndOfStream());
  }

  SECTION("Check blobs")
  {
    bytebuf foo = {0x01, 0x02, 0x40, 0x80, 0xff};
    for(byte i = 0; i < 250; i++)
      foo.push_back(i);

    foo.push_back(0x80);
    foo.push_back(0x70);
    foo.push_back(0x60);

    LLVMBC::BitWriter w(bits);

    w.WriteBlob(foo);

    w.align32bits();

    LLVMBC::BitReader r(bits.data(), bits.size());

    const byte *ptr = NULL;
    size_t len = 0;
    r.ReadBlob(ptr, len);

    r.align32bits();

    CHECK(r.AtEndOfStream());

    REQUIRE(len == foo.size());
    CHECK(bytebuf(ptr, len) == foo);
  }
}

#endif
