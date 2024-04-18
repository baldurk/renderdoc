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

#include "dxil_bytecode.h"
#include <ctype.h>
#include <stdio.h>
#include "common/common.h"
#include "common/formatting.h"
#include "os/os_specific.h"
#include "llvm_common.h"
#include "llvm_decoder.h"

#define IS_KNOWN(val, KnownID) (decltype(KnownID)(val) == KnownID)

#define BUMP_ALLOC_DEBUG OPTION_OFF

namespace DXIL
{
using namespace LLVMBC;

BumpAllocator::BumpAllocator(size_t totalSize)
{
  m_BlockSize = totalSize;
#if DISABLED(BUMP_ALLOC_DEBUG)
  cur = base = AllocAlignedBuffer(m_BlockSize);
  storage.push_back(base);
#endif
}

static constexpr uint32_t pattern[4] = {0x10101010U, 0xDADADADAU, 0x46464646U, 0x12345678U};

BumpAllocator::~BumpAllocator()
{
  for(byte *b : storage)
  {
#if DISABLED(BUMP_ALLOC_DEBUG)
    memset(b, 0xfe, m_BlockSize);
#endif
    FreeAlignedBuffer(b);
  }
}

void *BumpAllocator::alloc(size_t sz)
{
#if ENABLED(BUMP_ALLOC_DEBUG)
  for(byte *b : storage)
  {
    uint32_t size = *(uint32_t *)b;
    // check preceeding pattern
    RDCASSERT(memcmp(pattern, b + sizeof(pattern), sizeof(pattern)) == 0);
    // check trailing pattern
    RDCASSERT(memcmp(pattern, b + sizeof(pattern) * 2 + size, sizeof(pattern)) == 0);
  }

  byte *ret = AllocAlignedBuffer(sz + 3 * sizeof(pattern));
  storage.push_back(ret);

  // tight allocation size
  uint32_t size = (uint32_t)sz;
  memcpy(ret, &size, sizeof(uint32_t));
  // preceeding pattern
  memcpy(ret + sizeof(pattern), pattern, sizeof(pattern));
  // trailing pattern
  memcpy(ret + sizeof(pattern) * 2 + sz, pattern, sizeof(pattern));

  return ret + sizeof(pattern) * 2;
#else
  // if the current storage can't satisfy this, retire it and make a new one
  if(cur + sz > base + m_BlockSize)
  {
    cur = base = AllocAlignedBuffer(m_BlockSize);
    storage.push_back(base);
  }

  cur = AlignUpPtr(cur, 16U);
  byte *ret = cur;
#if defined(RDOC_RELEASE)
  memset(ret, 0, sz);
#else
  memset(ret, 0xcc, sz);
#endif
  cur += sz;
  return ret;
#endif
}

// helper struct for reading ops
struct OpReader
{
  OpReader(Program *prog, ValueList &values, const LLVMBC::BlockOrRecord &op)
      : prog(prog), values(values), type((FunctionRecord)op.id), ops(op.ops), idx(0)
  {
  }

  FunctionRecord type;

  size_t remaining() { return ops.size() - idx; }
  Value *getSymbol(uint64_t val) { return values[values.getRelativeBackwards(val)]; }
  Value *getSymbol(bool withType = true)
  {
    // get the value
    uint64_t val = get<uint64_t>();

    // non forward reference? return directly
    if(val <= values.curValueIndex())
      return getSymbol(val);

    // sometimes forward references have types
    Value *v = values.createPlaceholderValue(values.getRelativeForwards(-(int32_t)val));
    if(withType)
      v->type = getType();

    return v;
  }

  // some symbols are referenced absolute, not relative
  Value *getSymbolAbsolute() { return values[get<size_t>()]; }
  const Type *getType() { return prog->m_Types[get<size_t>()]; }
  template <typename T>
  T get()
  {
    return (T)ops[idx++];
  }

private:
  const rdcarray<uint64_t> &ops;
  size_t idx;
  Program *prog;
  ValueList &values;
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

const Metadata *Program::GetMetadataByName(const rdcstr &name) const
{
  for(size_t i = 0; i < m_NamedMeta.size(); i++)
    if(m_NamedMeta[i]->name == name)
      return m_NamedMeta[i];

  return NULL;
}

void Program::ParseConstant(ValueList &values, const LLVMBC::BlockOrRecord &constant)
{
  if(IS_KNOWN(constant.id, ConstantsRecord::SETTYPE))
  {
    m_CurParseType = m_Types[(size_t)constant.ops[0]];
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL) ||
          IS_KNOWN(constant.id, ConstantsRecord::UNDEF))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    c->setNULL(IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL));
    c->setUndef(IS_KNOWN(constant.id, ConstantsRecord::UNDEF));
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::INTEGER))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    c->setValue(LLVMBC::BitReader::svbr(constant.ops[0]));
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::FLOAT))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    uint64_t uval = 0;
    memcpy(&uval, &constant.ops[0], m_CurParseType->bitWidth / 8);
    c->setValue(uval);
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::STRING) ||
          IS_KNOWN(constant.id, ConstantsRecord::CSTRING))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    c->str = constant.getString(0);
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_CAST))
  {
    Constant *c = values.nextValue<Constant>();
    c->op = DecodeCast(constant.ops[0]);
    c->type = m_CurParseType;
    c->setInner(values.getOrCreatePlaceholder((size_t)constant.ops[2]));
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_BINOP))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    c->op = DecodeBinOp(c->type, constant.ops[0]);
    rdcarray<Value *> members;
    members.push_back(values.getOrCreatePlaceholder((size_t)constant.ops[1]));
    members.push_back(values.getOrCreatePlaceholder((size_t)constant.ops[2]));
    c->setCompound(alloc, std::move(members));
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_GEP))
  {
    Constant *c = values.nextValue<Constant>();

    c->op = Operation::GetElementPtr;

    size_t idx = 0;
    if(constant.ops.size() & 1)
      c->type = m_Types[(size_t)constant.ops[idx++]];

    rdcarray<Value *> members;

    for(; idx < constant.ops.size(); idx += 2)
    {
      const Type *t = m_Types[(size_t)constant.ops[idx]];
      Value *v = values[(size_t)constant.ops[idx + 1]];
      RDCASSERT(v->type == t);

      members.push_back(v);
    }

    c->type = members[0]->type->inner;

    // walk the type list to get the return type
    for(idx = 2; idx < members.size(); idx++)
    {
      if(c->type->type == Type::Vector || c->type->type == Type::Array)
      {
        c->type = c->type->inner;
      }
      else if(c->type->type == Type::Struct)
      {
        c->type = c->type->members[cast<Constant>(members[idx])->getU32()];
      }
      else
      {
        RDCERR("Unexpected type %d encountered in GEP", c->type->type);
      }
    }

    c->setCompound(alloc, std::move(members));

    // the result is a pointer to the return type
    c->type = GetPointerType(c->type, m_CurParseType->addrSpace);

    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::AGGREGATE))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    rdcarray<Value *> members;
    for(uint64_t m : constant.ops)
      members.push_back(values.getOrCreatePlaceholder((size_t)m));
    c->setCompound(alloc, std::move(members));
    values.addValue();
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::DATA))
  {
    Constant *c = values.nextValue<Constant>();
    c->type = m_CurParseType;
    c->setData(true);

    if(c->type->type == Type::Vector)
    {
      ShaderValue val;
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        if(c->type->bitWidth <= 32)
          val.u32v[m] = constant.ops[m] & ((1ULL << c->type->bitWidth) - 1);
        else
          val.u64v[m] = constant.ops[m];
      }
      c->setValue(alloc, val);
    }
    else
    {
      rdcarray<Value *> members;
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        uint64_t val = 0;
        if(c->type->inner->bitWidth <= 32)
          val = constant.ops[m] & ((1ULL << c->type->inner->bitWidth) - 1);
        else
          val = constant.ops[m];
        members.push_back(new(alloc) Literal(val));
      }
      c->setCompound(alloc, std::move(members));
    }

    values.addValue();
  }
  else
  {
    RDCERR("Unknown record ID %u encountered in constants block", constant.id);
  }
}

Program::Program(const byte *bytes, size_t length) : alloc(32 * 1024)
{
  const byte *ptr = bytes;
  const ProgramHeader *header = (const ProgramHeader *)ptr;
  RDCASSERT(header->DxilMagic == MAKE_FOURCC('D', 'X', 'I', 'L'));

  m_Bytes.assign(bytes, length);

  const byte *bitcode = ((const byte *)&header->DxilMagic) + header->BitcodeOffset;
  RDCASSERT(bitcode + header->BitcodeSize <= ptr + length);

  LLVMBC::BitcodeReader reader(bitcode, header->BitcodeSize);

  LLVMBC::BlockOrRecord root = reader.ReadToplevelBlock();

  // the top-level block should be MODULE_BLOCK
  RDCASSERT(KnownBlock(root.id) == KnownBlock::MODULE_BLOCK);

  // we should have consumed all bits, only one top-level block
  RDCASSERT(reader.AtEndOfStream());

  m_Type = DXBC::ShaderType(header->ProgramType);
  m_Major = (header->ProgramVersion & 0xf0) >> 4;
  m_Minor = header->ProgramVersion & 0xf;
  m_DXILVersion = header->DxilVersion;

  ValueList values(alloc);
  MetadataList metadata(alloc);

  // Input signature and Output signature haven't changed.
  // Pipeline Runtime Information we have decoded just not implemented here

  rdcstr datalayout, triple;

  rdcarray<size_t> functionDecls;

  // conservatively resize these so we can take pointers to put in the values array. There aren't
  // many root entries so this is a reasonable bound
  m_GlobalVars.reserve(root.children.size());
  m_Functions.reserve(root.children.size());
  m_Aliases.reserve(root.children.size());

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
        GlobalVar *g = values.nextValue<GlobalVar>();

        g->type = m_Types[(size_t)rootchild.ops[0]];
        if(rootchild.ops[1] & 0x1)
          g->flags |= GlobalFlags::IsConst;

        Type::PointerAddrSpace addrSpace = g->type->addrSpace;
        if(rootchild.ops[1] & 0x2)
          addrSpace = Type::PointerAddrSpace(rootchild.ops[1] >> 2);

        if(rootchild.ops[2])
          g->initialiser += rootchild.ops[2];

        switch(rootchild.ops[3])
        {
          case 0: g->flags |= GlobalFlags::ExternalLinkage; break;
          case 16: g->flags |= GlobalFlags::WeakAnyLinkage; break;
          case 2: g->flags |= GlobalFlags::AppendingLinkage; break;
          case 3: g->flags |= GlobalFlags::InternalLinkage; break;
          case 18: g->flags |= GlobalFlags::LinkOnceAnyLinkage; break;
          case 7: g->flags |= GlobalFlags::ExternalWeakLinkage; break;
          case 8: g->flags |= GlobalFlags::CommonLinkage; break;
          case 9: g->flags |= GlobalFlags::PrivateLinkage; break;
          case 17: g->flags |= GlobalFlags::WeakODRLinkage; break;
          case 19: g->flags |= GlobalFlags::LinkOnceODRLinkage; break;
          case 12: g->flags |= GlobalFlags::AvailableExternallyLinkage; break;
          default: break;
        }

        g->align = (1ULL << rootchild.ops[4]) >> 1;

        g->section = int32_t(rootchild.ops[5]) - 1;

        if(rootchild.ops.size() > 6)
        {
          RDCASSERTMSG("global has non-default visibility", rootchild.ops[6] == 0);
        }

        if(rootchild.ops.size() > 7)
        {
          RDCASSERTMSG("global has non-default TLS mode", rootchild.ops[7] == 0);
        }

        if(rootchild.ops.size() > 8)
        {
          if(rootchild.ops[8] == 1)
            g->flags |= GlobalFlags::GlobalUnnamedAddr;
          else if(rootchild.ops[8] == 2)
            g->flags |= GlobalFlags::LocalUnnamedAddr;
        }

        if(rootchild.ops.size() > 9)
        {
          if(rootchild.ops[9])
            g->flags |= GlobalFlags::ExternallyInitialised;
        }

        if(rootchild.ops.size() > 10)
        {
          RDCASSERTMSG("global has non-default DLL storage class", rootchild.ops[10] == 0);
        }

        if(rootchild.ops.size() > 11)
        {
          // assume no comdat
          RDCASSERTMSG("global has comdat", rootchild.ops[11] == 0);
        }

        g->type = GetPointerType(g->type, addrSpace);

        m_GlobalVars.push_back(g);
        values.addValue();
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::FUNCTION))
      {
        // [type, callingconv, isproto, linkage, paramattrs, alignment, section, visibility, gc,
        // unnamed_addr, prologuedata, dllstorageclass, comdat, prefixdata]
        Function *f = new(alloc) Function;

        f->type = m_Types[(size_t)rootchild.ops[0]];
        // ignore callingconv
        RDCASSERTMSG("Calling convention is non-default", rootchild.ops[1] == 0);
        f->external = (rootchild.ops[2] != 0);
        // ignore linkage
        if(rootchild.ops[3] == 3)
          f->internalLinkage = true;
        else
          RDCASSERTMSG("Linkage is non-default and not internal", rootchild.ops[3] == 0,
                       rootchild.ops[3]);
        if(rootchild.ops[4] > 0 && rootchild.ops[4] - 1 < m_AttributeSets.size())
          f->attrs = m_AttributeSets[(size_t)rootchild.ops[4] - 1];

        f->align = rootchild.ops[5];

        // ignore rest of properties, assert that if present they are 0
        for(size_t p = 6; p < rootchild.ops.size(); p++)
        {
          // 12, if present, is the comdat index
          if(p == 12 && rootchild.ops[p] > 0)
          {
            RDCASSERT(rootchild.ops[p] - 1 < m_Comdats.size(), rootchild.ops[p], m_Comdats.size());
            f->comdatIdx = uint32_t(rootchild.ops[p] - 1);
            continue;
          }

          RDCASSERT(rootchild.ops[p] == 0, p, rootchild.ops[p]);
        }

        if(!f->external)
          functionDecls.push_back(m_Functions.size());

        m_Functions.push_back(f);
        values.addValue(f);
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::ALIAS))
      {
        // [alias type, aliasee val#, linkage, visibility]
        Alias *a = values.nextValue<Alias>();

        a->type = m_Types[(size_t)rootchild.ops[0]];
        a->val = values[(size_t)rootchild.ops[1]];

        // ignore rest of properties, assert that if present they are 0
        for(size_t p = 2; p < rootchild.ops.size(); p++)
          RDCASSERT(rootchild.ops[p] == 0, p, rootchild.ops[p]);

        m_Aliases.push_back(a);
        values.addValue();
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::SECTIONNAME))
      {
        m_Sections.push_back(rootchild.getString(0));
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::COMDAT))
      {
        // can ignore the length for now, it's implicit anyway as there's nothing after the string
        m_Comdats.push_back({rootchild.ops[0], rootchild.getString(2)});
      }
      else
      {
        RDCERR("Unknown record ID %u encountered at module scope", rootchild.id);
      }
    }
    else if(rootchild.IsBlock())
    {
      if(IS_KNOWN(rootchild.id, KnownBlock::BLOCKINFO))
      {
        // do nothing, this is internal parse data
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::PARAMATTR_GROUP_BLOCK))
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

          AttributeGroup *group = alloc.alloc<AttributeGroup>();

          size_t id = (size_t)attrgroup.ops[0];
          group->slotIndex = (uint32_t)attrgroup.ops[1];

          for(size_t i = 2; i < attrgroup.ops.size(); i++)
          {
            switch(attrgroup.ops[i])
            {
              case 0:
              {
                group->params |= Attribute(1ULL << (attrgroup.ops[i + 1]));
                i++;
                break;
              }
              case 1:
              {
                uint64_t param = attrgroup.ops[i + 2];
                Attribute attr = Attribute(1ULL << attrgroup.ops[i + 1]);
                group->params |= attr;
                switch(attr)
                {
                  case Attribute::Alignment: group->align = param; break;
                  case Attribute::StackAlignment: group->stackAlign = param; break;
                  case Attribute::Dereferenceable: group->derefBytes = param; break;
                  case Attribute::DereferenceableOrNull: group->derefOrNullBytes = param; break;
                  default: RDCERR("Unexpected attribute %llu with parameter", attr);
                }
                i += 2;
                break;
              }
              default:
              {
                rdcstr a, b;

                a = attrgroup.getString(i + 1);
                a.resize(strlen(a.c_str()));

                if(attrgroup.ops[i] == 4)
                {
                  b = attrgroup.getString(i + 1 + a.size() + 1);
                  b.resize(strlen(b.c_str()));
                  i += a.size() + b.size() + 2;
                }
                else
                {
                  i += a.size() + 1;
                }

                group->strs.push_back({a, b});
                break;
              }
            }
          }

          m_AttributeGroups.resize_for_index(id);
          m_AttributeGroups[id] = group;
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::PARAMATTR_BLOCK))
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

          AttributeSet *attrs = alloc.alloc<AttributeSet>();

          attrs->orderedGroups = paramattr.ops;

          for(uint64_t g : paramattr.ops)
          {
            if(g < m_AttributeGroups.size())
            {
              const AttributeGroup *group = m_AttributeGroups[(size_t)g];
              if(group->slotIndex == AttributeGroup::FunctionSlot)
              {
                RDCASSERT(attrs->functionSlot == NULL);
                attrs->functionSlot = group;
              }
              else
              {
                attrs->groupSlots.resize_for_index(group->slotIndex);
                attrs->groupSlots[group->slotIndex] = group;
              }
            }
            else
            {
              RDCERR("Attribute refers to out of bounds group %llu", g);
            }
          }

          m_AttributeSets.push_back(attrs);
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::TYPE_BLOCK))
      {
        rdcstr structname;

        if(!rootchild.children.empty() && !IS_KNOWN(rootchild.children[0].id, TypeRecord::NUMENTRY))
        {
          RDCWARN("No NUMENTRY record, resizing conservatively to number of records");
          m_Types.reserve(rootchild.children.size());
        }

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
            m_Types.reserve((size_t)typ.ops[0]);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::VOID))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Scalar;
            newType->scalarType = Type::Void;

            m_Types.push_back(newType);
            m_VoidType = newType;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::LABEL))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Label;

            m_Types.push_back(newType);
            m_LabelType = newType;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::METADATA))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Metadata;

            m_Types.push_back(newType);
            m_MetaType = newType;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::HALF))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Scalar;
            newType->scalarType = Type::Float;
            newType->bitWidth = 16;

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::FLOAT))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Scalar;
            newType->scalarType = Type::Float;
            newType->bitWidth = 32;

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::DOUBLE))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Scalar;
            newType->scalarType = Type::Float;
            newType->bitWidth = 64;

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::INTEGER))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Scalar;
            newType->scalarType = Type::Int;
            newType->bitWidth = typ.ops[0] & 0xffffffff;

            m_Types.push_back(newType);
            if(newType->bitWidth == 1)
              m_BoolType = newType;
            else if(newType->bitWidth == 8)
              m_Int8Type = newType;
            else if(newType->bitWidth == 32)
              m_Int32Type = newType;
          }
          else if(IS_KNOWN(typ.id, TypeRecord::VECTOR))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Vector;
            newType->elemCount = typ.ops[0] & 0xffffffff;
            newType->inner = m_Types[(size_t)typ.ops[1]];

            // copy properties out of the inner for convenience
            newType->scalarType = newType->inner->scalarType;
            newType->bitWidth = newType->inner->bitWidth;

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::ARRAY))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Array;
            newType->elemCount = typ.ops[0] & 0xffffffff;
            newType->inner = m_Types[(size_t)typ.ops[1]];

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::POINTER))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Pointer;
            newType->inner = m_Types[(size_t)typ.ops[0]];
            newType->addrSpace = Type::PointerAddrSpace(typ.ops[1]);

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::OPAQUE))
          {
            Type *newType = new(alloc) Type;
            // pretend opaque types are empty structs
            newType->type = Type::Struct;
            newType->opaque = true;

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::STRUCT_NAME))
          {
            structname = typ.getString(0);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::STRUCT_ANON) ||
                  IS_KNOWN(typ.id, TypeRecord::STRUCT_NAMED))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Struct;
            newType->packedStruct = (typ.ops[0] != 0);

            for(size_t o = 1; o < typ.ops.size(); o++)
              newType->members.push_back(m_Types[(size_t)typ.ops[o]]);

            if(IS_KNOWN(typ.id, TypeRecord::STRUCT_NAMED))
            {
              // may we want a reverse map name -> type? probably not, this is only relevant for
              // disassembly or linking and disassembly we can do just by iterating all types
              newType->name = structname;
              structname.clear();
            }

            m_Types.push_back(newType);
          }
          else if(IS_KNOWN(typ.id, TypeRecord::FUNCTION_OLD) ||
                  IS_KNOWN(typ.id, TypeRecord::FUNCTION))
          {
            Type *newType = new(alloc) Type;
            newType->type = Type::Function;

            newType->vararg = (typ.ops[0] != 0);

            size_t o = 1;

            // skip attrid
            if(IS_KNOWN(typ.id, TypeRecord::FUNCTION_OLD))
              o++;

            // return type
            newType->inner = m_Types[(size_t)typ.ops[o]];
            o++;

            for(; o < typ.ops.size(); o++)
              newType->members.push_back(m_Types[(size_t)typ.ops[o]]);

            m_Types.push_back(newType);
          }
          else
          {
            RDCERR("Unknown record ID %u encountered in type block", typ.id);
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::CONSTANTS_BLOCK))
      {
        m_CurParseType = NULL;

        values.hintExpansion(rootchild.children.size());

        for(const LLVMBC::BlockOrRecord &constant : rootchild.children)
        {
          if(constant.IsBlock())
          {
            RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
            continue;
          }

          ParseConstant(values, constant);
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::VALUE_SYMTAB_BLOCK))
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

          size_t vidx = (size_t)symtab.ops[0];
          if(vidx < values.curValueIndex())
          {
            Value *v = values[vidx];
            rdcstr str = symtab.getString(1);

            SetValueSymtabString(v, str);

            if(!m_ValueSymtabOrder.empty())
              m_SortedSymtab &= GetValueSymtabString(m_ValueSymtabOrder.back()) < str;

            m_ValueSymtabOrder.push_back(v);
          }
          else
          {
            RDCERR("Value %zu referenced out of bounds", vidx);
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::METADATA_BLOCK))
      {
        metadata.hintExpansion(rootchild.children.size());
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
            NamedMetadata *meta = new(alloc) NamedMetadata;

            meta->name = metaRecord.getString();
            i++;
            const LLVMBC::BlockOrRecord &namedNode = rootchild.children[i];
            RDCASSERT(IS_KNOWN(namedNode.id, MetaDataRecord::NAMED_NODE));

            for(uint64_t op : namedNode.ops)
              meta->children.push_back(metadata[(size_t)op]);

            m_NamedMeta.push_back(meta);
          }
          else if(IS_KNOWN(metaRecord.id, MetaDataRecord::KIND))
          {
            size_t kind = (size_t)metaRecord.ops[0];
            m_Kinds.resize_for_index(kind);
            m_Kinds[kind] = metaRecord.getString(1);
            continue;
          }
          else
          {
            Metadata *meta = metadata[i];

            if(IS_KNOWN(metaRecord.id, MetaDataRecord::STRING_OLD))
            {
              meta->isConstant = true;
              meta->isString = true;
              meta->str = metaRecord.getString();
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::VALUE))
            {
              meta->value = values[(size_t)metaRecord.ops[1]];
              meta->type = m_Types[(size_t)metaRecord.ops[0]];
              meta->isConstant = true;
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::NODE) ||
                    IS_KNOWN(metaRecord.id, MetaDataRecord::DISTINCT_NODE))
            {
              if(IS_KNOWN(metaRecord.id, MetaDataRecord::DISTINCT_NODE))
                meta->isDistinct = true;

              for(uint64_t op : metaRecord.ops)
                meta->children.push_back(metadata.getOrNULL(op));
            }
            else
            {
              bool parsed = ParseDebugMetaRecord(metadata, metaRecord, *meta);
              if(!parsed)
              {
                RDCERR("unhandled metadata type %u", metaRecord.id);
              }
            }
          }
        }
      }
      else if(IS_KNOWN(rootchild.id, KnownBlock::FUNCTION_BLOCK))
      {
        Function *f = m_Functions[functionDecls[0]];
        functionDecls.erase(0);

        // conservative resize here so we can take pointers and have them stay valid
        f->instructions.reserve(rootchild.children.size());

        values.beginFunction();
        metadata.beginFunction();

        f->args.reserve(f->type->members.size());
        for(size_t i = 0; i < f->type->members.size(); i++)
        {
          Instruction *arg = values.nextValue<Instruction>();
          arg->type = f->type->members[i];
          f->args.push_back(arg);
          values.addValue();
        }

        size_t curBlock = 0;
        int32_t debugLocIndex = -1;

        values.hintExpansion(rootchild.children.size());

        for(const LLVMBC::BlockOrRecord &funcChild : rootchild.children)
        {
          if(funcChild.IsBlock())
          {
            if(IS_KNOWN(funcChild.id, KnownBlock::CONSTANTS_BLOCK))
            {
              values.hintExpansion(funcChild.children.size());

              m_CurParseType = NULL;
              for(const LLVMBC::BlockOrRecord &constant : funcChild.children)
              {
                if(constant.IsBlock())
                {
                  RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
                  continue;
                }

                ParseConstant(values, constant);
              }
            }
            else if(IS_KNOWN(funcChild.id, KnownBlock::METADATA_BLOCK))
            {
              metadata.hintExpansion(funcChild.children.size());

              size_t m = metadata.size();

              for(const LLVMBC::BlockOrRecord &metaRecord : funcChild.children)
              {
                if(metaRecord.IsBlock())
                {
                  RDCERR("Unexpected subblock in function METADATA_BLOCK");
                  continue;
                }

                Metadata *meta = metadata[m];

                if(IS_KNOWN(metaRecord.id, MetaDataRecord::VALUE))
                {
                  meta->isConstant = true;
                  meta->value = values.getOrCreatePlaceholder((size_t)metaRecord.ops[1]);
                  meta->type = m_Types[(size_t)metaRecord.ops[0]];
                }
                else
                {
                  RDCERR("Unexpected record %u in function METADATA_BLOCK", metaRecord.id);
                }

                m++;
              }
            }
            else if(IS_KNOWN(funcChild.id, KnownBlock::VALUE_SYMTAB_BLOCK))
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

                  if(idx >= values.curValueIndex())
                  {
                    RDCERR("Out of bounds symbol index %zu (%s) in function symbol table", idx,
                           symtab.getString(1).c_str());
                    continue;
                  }

                  Value *v = values[idx];
                  rdcstr str = symtab.getString(1);

                  SetValueSymtabString(v, str);

                  if(!f->valueSymtabOrder.empty())
                    f->sortedSymtab &= GetValueSymtabString(f->valueSymtabOrder.back()) < str;

                  f->valueSymtabOrder.push_back(v);
                }
                else if(IS_KNOWN(symtab.id, ValueSymtabRecord::BBENTRY))
                {
                  Value *v = f->blocks[(size_t)symtab.ops[0]];
                  rdcstr str = symtab.getString(1);

                  SetValueSymtabString(v, str);

                  if(!f->valueSymtabOrder.empty())
                    f->sortedSymtab &= GetValueSymtabString(f->valueSymtabOrder.back()) < str;

                  f->valueSymtabOrder.push_back(v);
                }
                else
                {
                  RDCERR("Unexpected function symbol table record ID %u", symtab.id);
                  continue;
                }
              }
            }
            else if(IS_KNOWN(funcChild.id, KnownBlock::METADATA_ATTACHMENT))
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

                AttachedMetadata attach;

                if(meta.ops.size() % 2 != 0)
                  idx++;

                for(; idx < meta.ops.size(); idx += 2)
                  attach.push_back(make_rdcpair(meta.ops[idx], metadata.getDirect(meta.ops[idx + 1])));

                if(meta.ops.size() % 2 == 0)
                  f->attachedMeta.swap(attach);
                else
                  f->instructions[(size_t)meta.ops[0]]->extra(alloc).attachedMeta.swap(attach);
              }
            }
            else if(IS_KNOWN(funcChild.id, KnownBlock::USELIST_BLOCK))
            {
              m_Uselists = true;
              for(const LLVMBC::BlockOrRecord &uselist : funcChild.children)
              {
                if(uselist.IsBlock())
                {
                  RDCERR("Unexpected subblock in USELIST_BLOCK");
                  continue;
                }

                const bool bb = IS_KNOWN(uselist.id, UselistRecord::BB);
                if(IS_KNOWN(uselist.id, UselistRecord::DEFAULT) || bb)
                {
                  UselistEntry u;
                  u.block = bb;
                  u.shuffle = uselist.ops;
                  u.value = values[(size_t)u.shuffle.back()];
                  u.shuffle.pop_back();
                  f->uselist.push_back(u);
                }
                else
                {
                  RDCERR("Unexpected record %u in USELIST_BLOCK", uselist.id);
                  continue;
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
            OpReader op(this, values, funcChild);

            if(op.type == FunctionRecord::DECLAREBLOCKS)
            {
              f->blocks.resize(op.get<size_t>());
              for(size_t b = 0; b < f->blocks.size(); b++)
                f->blocks[b] = new(alloc) Block(m_LabelType);

              curBlock = 0;
            }
            else if(op.type == FunctionRecord::DEBUG_LOC)
            {
              DebugLocation debugLoc;
              debugLoc.line = op.get<uint64_t>();
              debugLoc.col = op.get<uint64_t>();
              debugLoc.scope = metadata.getOrNULL(op.get<uint64_t>());
              debugLoc.inlinedAt = metadata.getOrNULL(op.get<uint64_t>());

              debugLocIndex = m_DebugLocations.indexOf(debugLoc);

              if(debugLocIndex < 0)
              {
                m_DebugLocations.push_back(debugLoc);
                debugLocIndex = int32_t(m_DebugLocations.size() - 1);
              }

              f->instructions.back()->debugLoc = (uint32_t)debugLocIndex;
            }
            else if(op.type == FunctionRecord::DEBUG_LOC_AGAIN)
            {
              f->instructions.back()->debugLoc = (uint32_t)debugLocIndex;
            }
            else if(op.type == FunctionRecord::INST_CALL)
            {
              size_t paramAttrs = op.get<size_t>();

              uint64_t callingFlags = op.get<uint64_t>();

              InstructionFlags flags = InstructionFlags::NoFlags;

              if(callingFlags & (1ULL << 17))
              {
                flags = op.get<InstructionFlags>();
                RDCASSERT(flags != InstructionFlags::NoFlags);

                callingFlags &= ~(1ULL << 17);
              }

              const Type *funcCallType = NULL;

              if(callingFlags & (1ULL << 15))
              {
                funcCallType = op.getType();    // funcCallType

                callingFlags &= ~(1ULL << 15);
              }

              RDCASSERTMSG("Calling flags should only have at most two known bits set",
                           callingFlags == 0, callingFlags);

              Function *funcCall = cast<Function>(op.getSymbol());

              if(!funcCall)
              {
                RDCERR("Unexpected symbol type called in INST_CALL");
                continue;
              }

              Instruction *inst = NULL;

              bool voidCall = funcCall->type->inner->isVoid();

              if(!voidCall)
                inst = values.nextValue<Instruction>();
              else
                inst = new(alloc) Instruction();

              inst->op = Operation::Call;
              inst->extra(alloc).funcCall = funcCall;
              inst->type = funcCall->type->inner;
              inst->opFlags() = flags;
              if(paramAttrs > 0)
                inst->extra(alloc).paramAttrs = m_AttributeSets[paramAttrs - 1];

              if(funcCallType)
              {
                RDCASSERT(funcCallType == funcCall->type);
              }

              for(size_t i = 0; op.remaining() > 0; i++)
              {
                Value *arg = NULL;
                if(funcCall->type->members[i]->type == Type::Metadata)
                {
                  int32_t offs = (int32_t)op.get<uint32_t>();
                  size_t idx = values.curValueIndex() - offs;
                  arg = metadata[idx];
                }
                else
                {
                  arg = op.getSymbol(false);
                }
                inst->args.push_back(arg);
              }

              RDCASSERTEQUAL(inst->args.size(), funcCall->type->members.size());

              f->instructions.push_back(inst);

              if(!voidCall)
                values.addValue();
              if(funcCall->name == "dx.op.createHandleFromHeap")
                m_directHeapAccessCount++;
            }
            else if(op.type == FunctionRecord::INST_CAST)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->args.push_back(op.getSymbol());
              inst->type = op.getType();

              uint64_t opcode = op.get<uint64_t>();
              inst->op = DecodeCast(opcode);

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_EXTRACTVAL)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::ExtractVal;

              inst->args.push_back(op.getSymbol());
              inst->type = inst->args.back()->type;
              while(op.remaining() > 0)
              {
                uint64_t val = op.get<uint64_t>();
                if(inst->type->type == Type::Array)
                  inst->type = inst->type->inner;
                else
                  inst->type = inst->type->members[(size_t)val];
                inst->args.push_back(new(alloc) Literal(val));
              }

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_RET)
            {
              // even rets returning a value are still void
              Instruction *inst = new(alloc) Instruction;
              inst->type = GetVoidType();

              if(op.remaining() != 0)
                inst->args.push_back(op.getSymbol());

              inst->op = Operation::Ret;

              curBlock++;

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_BINOP)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->args.push_back(op.getSymbol());
              inst->type = inst->args.back()->type;
              inst->args.push_back(op.getSymbol(false));

              inst->op = DecodeBinOp(inst->type, op.get<uint64_t>());

              if(op.remaining() > 0)
              {
                uint64_t flags = op.get<uint64_t>();
                if(inst->op == Operation::Add || inst->op == Operation::Sub ||
                   inst->op == Operation::Mul || inst->op == Operation::ShiftLeft)
                {
                  if(flags & 0x2)
                    inst->opFlags() |= InstructionFlags::NoSignedWrap;
                  if(flags & 0x1)
                    inst->opFlags() |= InstructionFlags::NoUnsignedWrap;
                }
                else if(inst->op == Operation::SDiv || inst->op == Operation::UDiv ||
                        inst->op == Operation::LogicalShiftRight ||
                        inst->op == Operation::ArithShiftRight)
                {
                  if(flags & 0x1)
                    inst->opFlags() |= InstructionFlags::Exact;
                }
                else if(inst->type->scalarType == Type::Float)
                {
                  // fast math flags overlap
                  inst->opFlags() = InstructionFlags(flags);
                }

                RDCASSERT(inst->opFlags() != InstructionFlags::NoFlags);
              }

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_UNREACHABLE)
            {
              Instruction *inst = new(alloc) Instruction;

              inst->op = Operation::Unreachable;

              inst->type = GetVoidType();

              curBlock++;

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_ALLOCA)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::Alloca;

              inst->type = op.getType();

              // we now have the inner type, but this instruction returns a pointer to that type so
              // adjust
              inst->type = GetPointerType(inst->type, Type::PointerAddrSpace::Default);

              RDCASSERT(inst->type->type == Type::Pointer);

              // type of the size - ignored
              const Type *sizeType = op.getType();
              // size
              inst->args.push_back(op.getSymbolAbsolute());

              RDCASSERT(sizeType == inst->args.back()->type);

              uint64_t align = op.get<uint64_t>();

              if(align & 0x20)
              {
                // argument alloca
                inst->opFlags() |= InstructionFlags::ArgumentAlloca;
              }
              if((align & 0x40) == 0)
              {
                RDCASSERT(inst->type->type == Type::Pointer);
                inst->type = inst->type->inner;
              }

              align &= ~0xE0;

              RDCASSERT(align < 0x100);
              inst->align = align & 0xff;

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_INBOUNDS_GEP_OLD ||
                    op.type == FunctionRecord::INST_GEP_OLD || op.type == FunctionRecord::INST_GEP)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::GetElementPtr;

              if(op.type == FunctionRecord::INST_INBOUNDS_GEP_OLD)
                inst->opFlags() |= InstructionFlags::InBounds;

              if(op.type == FunctionRecord::INST_GEP)
              {
                if(op.get<uint64_t>())
                  inst->opFlags() |= InstructionFlags::InBounds;
                inst->type = op.getType();
              }

              while(op.remaining() > 0)
              {
                inst->args.push_back(op.getSymbol());

                if(inst->type == NULL && inst->args.size() == 1)
                  inst->type = inst->args.back()->type;
              }

              // walk the type list to get the return type
              for(size_t idx = 2; idx < inst->args.size(); idx++)
              {
                if(inst->type->type == Type::Vector || inst->type->type == Type::Array)
                {
                  inst->type = inst->type->inner;
                }
                else if(inst->type->type == Type::Struct)
                {
                  // if it's a struct the index must be constant
                  Constant *c = cast<Constant>(inst->args[idx]);
                  RDCASSERT(c);
                  inst->type = inst->type->members[c->getU32()];
                }
                else
                {
                  RDCERR("Unexpected type %d encountered in GEP", inst->type->type);
                }
              }

              // get the pointer type
              inst->type = GetPointerType(inst->type, inst->args[0]->type->addrSpace);

              RDCASSERT(inst->type->type == Type::Pointer);

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_LOAD)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::Load;

              inst->args.push_back(op.getSymbol());

              if(op.remaining() == 3)
              {
                inst->type = op.getType();
              }
              else
              {
                inst->type = inst->args.back()->type;
                RDCASSERT(inst->type->type == Type::Pointer);
                inst->type = inst->type->inner;
              }

              inst->align = op.get<uint8_t>();
              inst->opFlags() |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                           : InstructionFlags::NoFlags;

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_STORE_OLD || op.type == FunctionRecord::INST_STORE)
            {
              Instruction *inst = new(alloc) Instruction;

              inst->op = Operation::Store;

              inst->type = GetVoidType();

              inst->args.push_back(op.getSymbol());
              if(op.type == FunctionRecord::INST_STORE_OLD)
                inst->args.push_back(op.getSymbol(false));
              else
                inst->args.push_back(op.getSymbol());

              inst->align = op.get<uint8_t>();
              inst->opFlags() |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                           : InstructionFlags::NoFlags;

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_CMP ||
                    IS_KNOWN(op.type, FunctionRecord::INST_CMP2))
            {
              Instruction *inst = values.nextValue<Instruction>();

              // a
              inst->args.push_back(op.getSymbol());

              const Type *argType = inst->args.back()->type;

              // b
              inst->args.push_back(op.getSymbol(false));

              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->op = Operation::FOrdFalse; break;
                case 1: inst->op = Operation::FOrdEqual; break;
                case 2: inst->op = Operation::FOrdGreater; break;
                case 3: inst->op = Operation::FOrdGreaterEqual; break;
                case 4: inst->op = Operation::FOrdLess; break;
                case 5: inst->op = Operation::FOrdLessEqual; break;
                case 6: inst->op = Operation::FOrdNotEqual; break;
                case 7: inst->op = Operation::FOrd; break;
                case 8: inst->op = Operation::FUnord; break;
                case 9: inst->op = Operation::FUnordEqual; break;
                case 10: inst->op = Operation::FUnordGreater; break;
                case 11: inst->op = Operation::FUnordGreaterEqual; break;
                case 12: inst->op = Operation::FUnordLess; break;
                case 13: inst->op = Operation::FUnordLessEqual; break;
                case 14: inst->op = Operation::FUnordNotEqual; break;
                case 15: inst->op = Operation::FOrdTrue; break;

                case 32: inst->op = Operation::IEqual; break;
                case 33: inst->op = Operation::INotEqual; break;
                case 34: inst->op = Operation::UGreater; break;
                case 35: inst->op = Operation::UGreaterEqual; break;
                case 36: inst->op = Operation::ULess; break;
                case 37: inst->op = Operation::ULessEqual; break;
                case 38: inst->op = Operation::SGreater; break;
                case 39: inst->op = Operation::SGreaterEqual; break;
                case 40: inst->op = Operation::SLess; break;
                case 41: inst->op = Operation::SLessEqual; break;

                default:
                  inst->op = Operation::FOrdFalse;
                  RDCERR("Unexpected comparison %llu", opcode);
                  break;
              }

              // fast math flags
              if(op.remaining() > 0)
              {
                inst->opFlags() = op.get<InstructionFlags>();

                RDCASSERTNOTEQUAL((uint64_t)inst->opFlags(), 0);
              }

              inst->type = GetBoolType();

              // if we're comparing vectors, the return type is an equal sized bool vector
              if(argType->type == Type::Vector)
              {
                for(const Type *t : m_Types)
                {
                  if(t->type == Type::Vector && t->inner == inst->type &&
                     t->elemCount == argType->elemCount)
                  {
                    inst->type = t;
                    break;
                  }
                }
              }

              RDCASSERT(inst->type->type == argType->type &&
                        inst->type->elemCount == argType->elemCount);

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_SELECT || op.type == FunctionRecord::INST_VSELECT)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::Select;

              // if true
              inst->args.push_back(op.getSymbol());

              inst->type = inst->args.back()->type;

              // if false
              inst->args.push_back(op.getSymbol(false));
              // selector
              if(op.type == FunctionRecord::INST_SELECT)
                inst->args.push_back(op.getSymbol(false));
              else
                inst->args.push_back(op.getSymbol());

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_BR)
            {
              Instruction *inst = new(alloc) Instruction;

              inst->op = Operation::Branch;

              inst->type = GetVoidType();

              // true destination
              uint64_t trueDest = op.get<uint64_t>();
              inst->args.push_back(f->blocks[(size_t)trueDest]);
              f->blocks[(size_t)trueDest]->preds.insert(0, f->blocks[curBlock]);

              if(op.remaining() > 0)
              {
                // false destination
                uint64_t falseDest = op.get<uint64_t>();
                inst->args.push_back(f->blocks[(size_t)falseDest]);
                f->blocks[(size_t)falseDest]->preds.insert(0, f->blocks[curBlock]);

                // predicate
                inst->args.push_back(op.getSymbol(false));
              }

              curBlock++;

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_SWITCH)
            {
              Instruction *inst = new(alloc) Instruction;

              inst->op = Operation::Switch;

              inst->type = GetVoidType();

              uint64_t typeIdx = op.get<uint64_t>();

              static const uint64_t SWITCH_INST_MAGIC = 0x4B5;
              if((typeIdx >> 16) == SWITCH_INST_MAGIC)
              {
                // type of condition
                const Type *condType = op.getType();

                RDCASSERT(condType->bitWidth <= 64);

                // condition
                inst->args.push_back(op.getSymbol(false));

                // default block
                size_t defaultDest = op.get<size_t>();
                inst->args.push_back(f->blocks[defaultDest]);
                f->blocks[defaultDest]->preds.insert(0, f->blocks[curBlock]);

                RDCERR("Unsupported switch instruction version");
              }
              else
              {
                // condition
                inst->args.push_back(op.getSymbol(false));

                // default block
                size_t defaultDest = op.get<size_t>();
                inst->args.push_back(f->blocks[defaultDest]);
                f->blocks[defaultDest]->preds.insert(0, f->blocks[curBlock]);

                uint64_t numCases = op.remaining() / 2;

                for(uint64_t c = 0; c < numCases; c++)
                {
                  // case value, absolute not relative
                  inst->args.push_back(op.getSymbolAbsolute());

                  // case block
                  size_t caseDest = op.get<size_t>();
                  inst->args.push_back(f->blocks[caseDest]);
                  f->blocks[caseDest]->preds.insert(0, f->blocks[curBlock]);
                }
              }

              curBlock++;

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_PHI)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::Phi;

              inst->type = op.getType();

              while(op.remaining() > 0)
              {
                int64_t valSrc = LLVMBC::BitReader::svbr(op.get<uint64_t>());
                uint64_t blockSrc = op.get<uint64_t>();

                if(valSrc < 0)
                {
                  inst->args.push_back(
                      values.createPlaceholderValue(values.getRelativeForwards(-valSrc)));
                }
                else
                {
                  inst->args.push_back(op.getSymbol((uint64_t)valSrc));
                }
                inst->args.push_back(f->blocks[(size_t)blockSrc]);
              }

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_LOADATOMIC)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::LoadAtomic;

              inst->args.push_back(op.getSymbol());

              if(op.remaining() == 5)
              {
                inst->type = op.getType();
              }
              else
              {
                inst->type = inst->args.back()->type;
                RDCASSERT(inst->type->type == Type::Pointer);
                inst->type = inst->type->inner;
              }

              inst->align = op.get<uint8_t>();
              inst->opFlags() |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                           : InstructionFlags::NoFlags;

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst->opFlags() |= InstructionFlags::SuccessUnordered; break;
                case 2: inst->opFlags() |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst->opFlags() |= InstructionFlags::SuccessAcquire; break;
                case 4: inst->opFlags() |= InstructionFlags::SuccessRelease; break;
                case 5: inst->opFlags() |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->opFlags() |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_STOREATOMIC_OLD ||
                    op.type == FunctionRecord::INST_STOREATOMIC)
            {
              Instruction *inst = new(alloc) Instruction;

              inst->op = Operation::StoreAtomic;

              inst->type = GetVoidType();

              inst->args.push_back(op.getSymbol());
              if(op.type == FunctionRecord::INST_STOREATOMIC_OLD)
                inst->args.push_back(op.getSymbol(false));
              else
                inst->args.push_back(op.getSymbol());

              inst->align = op.get<uint8_t>();
              inst->opFlags() |= (op.get<uint64_t>() != 0) ? InstructionFlags::Volatile
                                                           : InstructionFlags::NoFlags;

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst->opFlags() |= InstructionFlags::SuccessUnordered; break;
                case 2: inst->opFlags() |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst->opFlags() |= InstructionFlags::SuccessAcquire; break;
                case 4: inst->opFlags() |= InstructionFlags::SuccessRelease; break;
                case 5: inst->opFlags() |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->opFlags() |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_ATOMICRMW)
            {
              Instruction *inst = values.nextValue<Instruction>();

              // pointer to atomically modify
              inst->args.push_back(op.getSymbol());

              // type is the pointee of the first argument
              inst->type = inst->args.back()->type;
              RDCASSERT(inst->type->type == Type::Pointer);
              inst->type = inst->type->inner;

              // parameter value
              inst->args.push_back(op.getSymbol(false));

              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->op = Operation::AtomicExchange; break;
                case 1: inst->op = Operation::AtomicAdd; break;
                case 2: inst->op = Operation::AtomicSub; break;
                case 3: inst->op = Operation::AtomicAnd; break;
                case 4: inst->op = Operation::AtomicNand; break;
                case 5: inst->op = Operation::AtomicOr; break;
                case 6: inst->op = Operation::AtomicXor; break;
                case 7: inst->op = Operation::AtomicMax; break;
                case 8: inst->op = Operation::AtomicMin; break;
                case 9: inst->op = Operation::AtomicUMax; break;
                case 10: inst->op = Operation::AtomicUMin; break;
                default:
                  RDCERR("Unhandled atomicrmw op %llu", opcode);
                  inst->op = Operation::AtomicExchange;
                  break;
              }

              if(op.get<uint64_t>())
                inst->opFlags() |= InstructionFlags::Volatile;

              // success ordering
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst->opFlags() |= InstructionFlags::SuccessUnordered; break;
                case 2: inst->opFlags() |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst->opFlags() |= InstructionFlags::SuccessAcquire; break;
                case 4: inst->opFlags() |= InstructionFlags::SuccessRelease; break;
                case 5: inst->opFlags() |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->opFlags() |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_CMPXCHG ||
                    op.type == FunctionRecord::INST_CMPXCHG_OLD)
            {
              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::CompareExchange;

              // pointer to atomically modify
              inst->args.push_back(op.getSymbol());

              // type is the pointee of the first argument
              inst->type = inst->args.back()->type;
              RDCASSERT(inst->type->type == Type::Pointer);
              inst->type = inst->type->inner;

              // combined with a bool, search for a struct like that
              const Type *boolType = GetBoolType();

              for(const Type *t : m_Types)
              {
                if(t->type == Type::Struct && t->members.size() == 2 &&
                   t->members[0] == inst->type && t->members[1] == boolType)
                {
                  inst->type = t;
                  break;
                }
              }

              RDCASSERT(inst->type->type == Type::Struct);

              // expect modern encoding with weak parameters.
              RDCASSERT(funcChild.ops.size() >= 8);

              // compare value
              if(op.type == FunctionRecord::INST_CMPXCHG_OLD)
                inst->args.push_back(op.getSymbol(false));
              else
                inst->args.push_back(op.getSymbol());

              // new replacement value
              inst->args.push_back(op.getSymbol(false));

              if(op.get<uint64_t>())
                inst->opFlags() |= InstructionFlags::Volatile;

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst->opFlags() |= InstructionFlags::SuccessUnordered; break;
                case 2: inst->opFlags() |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst->opFlags() |= InstructionFlags::SuccessAcquire; break;
                case 4: inst->opFlags() |= InstructionFlags::SuccessRelease; break;
                case 5: inst->opFlags() |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->opFlags() |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              // failure ordering
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst->opFlags() |= InstructionFlags::FailureUnordered; break;
                case 2: inst->opFlags() |= InstructionFlags::FailureMonotonic; break;
                case 3: inst->opFlags() |= InstructionFlags::FailureAcquire; break;
                case 4: inst->opFlags() |= InstructionFlags::FailureRelease; break;
                case 5: inst->opFlags() |= InstructionFlags::FailureAcquireRelease; break;
                case 6: inst->opFlags() |= InstructionFlags::FailureSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected failure ordering %llu", opcode);
                  inst->opFlags() |= InstructionFlags::FailureSequentiallyConsistent;
                  break;
              }

              if(op.get<uint64_t>())
                inst->opFlags() |= InstructionFlags::Weak;

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_FENCE)
            {
              Instruction *inst = new(alloc) Instruction;

              inst->op = Operation::Fence;

              inst->type = GetVoidType();

              // success ordering
              uint64_t opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: break;
                case 1: inst->opFlags() |= InstructionFlags::SuccessUnordered; break;
                case 2: inst->opFlags() |= InstructionFlags::SuccessMonotonic; break;
                case 3: inst->opFlags() |= InstructionFlags::SuccessAcquire; break;
                case 4: inst->opFlags() |= InstructionFlags::SuccessRelease; break;
                case 5: inst->opFlags() |= InstructionFlags::SuccessAcquireRelease; break;
                case 6: inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent; break;
                default:
                  RDCERR("Unexpected success ordering %llu", opcode);
                  inst->opFlags() |= InstructionFlags::SuccessSequentiallyConsistent;
                  break;
              }

              // synchronisation scope
              opcode = op.get<uint64_t>();
              switch(opcode)
              {
                case 0: inst->opFlags() |= InstructionFlags::SingleThread; break;
                case 1: break;
                default: RDCERR("Unexpected synchronisation scope %llu", opcode); break;
              }

              f->instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_EXTRACTELT)
            {
              // DXIL claims to be scalarised but lol that's a lie

              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::ExtractElement;

              // vector
              inst->args.push_back(op.getSymbol());

              // result is the scalar type within the vector
              inst->type = inst->args.back()->type->inner;

              // index
              inst->args.push_back(op.getSymbol());

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_INSERTELT)
            {
              // DXIL claims to be scalarised but lol that's a lie

              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::InsertElement;

              // vector
              inst->args.push_back(op.getSymbol());

              // result is the vector type
              inst->type = inst->args.back()->type;

              // replacement element
              inst->args.push_back(op.getSymbol(false));
              // index
              inst->args.push_back(op.getSymbol());

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_SHUFFLEVEC)
            {
              // DXIL claims to be scalarised but is not. Surprise surprise!

              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::ShuffleVector;

              // vector 1
              inst->args.push_back(op.getSymbol());

              const Type *vecType = inst->args.back()->type;

              // vector 2
              inst->args.push_back(op.getSymbol(false));
              // indexes
              inst->args.push_back(op.getSymbol());

              // result is a vector with the inner type of the first two vectors and the element
              // count of the last vector
              const Type *maskType = inst->args.back()->type;

              for(const Type *t : m_Types)
              {
                if(t->type == Type::Vector && t->inner == vecType->inner &&
                   t->elemCount == maskType->elemCount)
                {
                  inst->type = t;
                  break;
                }
              }

              RDCASSERT(inst->type);

              f->instructions.push_back(inst);
              values.addValue();
            }
            else if(op.type == FunctionRecord::INST_INSERTVAL)
            {
              // DXIL claims to be scalarised so should this appear?
              RDCWARN("Unexpected aggregate instruction insertvalue in DXIL");

              Instruction *inst = values.nextValue<Instruction>();

              inst->op = Operation::InsertValue;

              // aggregate
              inst->args.push_back(op.getSymbol());

              // result is the aggregate type
              inst->type = inst->args.back()->type;

              // replacement element
              inst->args.push_back(op.getSymbol());
              // indices as literals
              while(op.remaining() > 0)
                inst->args.push_back(new(alloc) Literal(op.get<uint64_t>()));

              f->instructions.push_back(inst);
              values.addValue();
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

        RDCASSERT(curBlock == f->blocks.size());

        curBlock = 0;
        for(size_t i = 0; i < f->instructions.size(); i++)
        {
          Instruction &inst = *f->instructions[i];
          if(inst.op == Operation::Branch || inst.op == Operation::Unreachable ||
             inst.op == Operation::Switch || inst.op == Operation::Ret)
          {
            curBlock++;

            if(i == f->instructions.size() - 1)
              break;

            continue;
          }

          if(inst.type->isVoid())
            continue;

          if(!inst.getName().empty())
            continue;
        }

        values.endFunction();
        metadata.endFunction();
      }
      else
      {
        RDCERR("Unknown block ID %u encountered at module scope", rootchild.id);
      }
    }
  }

  // pointer fixups. This is only needed for global variabls as it has forward references to
  // constants before we can even reserve the constants.
  for(GlobalVar *g : m_GlobalVars)
  {
    if(g->initialiser)
    {
      size_t idx = g->initialiser - (Constant *)NULL;
      g->initialiser = cast<Constant>(values[idx - 1]);
    }
  }

  RDCASSERT(functionDecls.empty());
}

rdcstr Program::GetValueSymtabString(Value *v)
{
  if(Constant *c = cast<Constant>(v))
    return c->str;
  else if(Instruction *i = cast<Instruction>(v))
    return i->extra(alloc).name;
  else if(Block *b = cast<Block>(v))
    return b->name;
  else if(GlobalVar *g = cast<GlobalVar>(v))
    return g->name;
  else if(Function *f = cast<Function>(v))
    return f->name;
  else if(Alias *a = cast<Alias>(v))
    return a->name;

  return "";
}

void Program::SetValueSymtabString(Value *v, const rdcstr &s)
{
  if(Constant *c = cast<Constant>(v))
    c->str = s;
  else if(Instruction *i = cast<Instruction>(v))
    i->extra(alloc).name = s;
  else if(Block *b = cast<Block>(v))
    b->name = s;
  else if(GlobalVar *g = cast<GlobalVar>(v))
    g->name = s;
  else if(Function *f = cast<Function>(v))
    f->name = s;
  else if(Alias *a = cast<Alias>(v))
    a->name = s;
}

uint32_t Program::GetMetaSlot(const Metadata *m) const
{
  RDCASSERTNOTEQUAL(m->slot, ~0U);
  return m->slot;
}

void Program::AssignMetaSlot(rdcarray<Metadata *> &metaSlots, uint32_t &nextMetaSlot, Metadata *m)
{
  if(m->slot != ~0U)
    return;

  m->slot = nextMetaSlot++;
  metaSlots.push_back(m);

  // assign meta IDs to the children now
  for(Metadata *c : m->children)
  {
    if(!c || c->isConstant)
      continue;

    AssignMetaSlot(metaSlots, nextMetaSlot, c);
  }
}

uint32_t Program::GetMetaSlot(const DebugLocation *l) const
{
  RDCASSERTNOTEQUAL(l->slot, ~0U);
  return l->slot;
}

void Program::AssignMetaSlot(rdcarray<Metadata *> &metaSlots, uint32_t &nextMetaSlot,
                             DebugLocation &l)
{
  if(l.slot != ~0U)
    return;

  l.slot = nextMetaSlot++;

  if(l.scope)
    AssignMetaSlot(metaSlots, nextMetaSlot, l.scope);
  if(l.inlinedAt)
    AssignMetaSlot(metaSlots, nextMetaSlot, l.inlinedAt);
}

const Type *Program::GetPointerType(const Type *type, Type::PointerAddrSpace addrSpace)
{
  for(const Type *t : m_Types)
    if(t->type == Type::Pointer && t->inner == type && t->addrSpace == addrSpace)
      return t;

  RDCWARN("Couldn't find pointer type as expected. Adding transient type");

  Type *newType = new(alloc) Type;
  newType->type = Type::Pointer;
  newType->inner = type;
  newType->addrSpace = addrSpace;
  m_Types.push_back(newType);

  return m_Types.back();
}

Metadata::~Metadata()
{
  SAFE_DELETE(dwarf);
  SAFE_DELETE(debugLoc);
}

static const uint16_t unvisitedTypeId = 0xffff;

void LLVMOrderAccumulator::reset(GlobalVar *g)
{
  g->id = Value::UnvisitedID;
  reset((Constant *)g->initialiser);
}

void LLVMOrderAccumulator::reset(Alias *a)
{
  a->id = Value::UnvisitedID;
  reset(a->val);
}

void LLVMOrderAccumulator::reset(Constant *c)
{
  if(!c || c->id == Value::UnvisitedID)
    return;

  c->id = Value::UnvisitedID;
  c->refCount = 0;
  if(c->isCast())
  {
    reset(c->getInner());
  }
  else if(c->isCompound())
  {
    for(Value *v : c->getMembers())
      reset((Value *)v);
  }
}

void LLVMOrderAccumulator::reset(Block *b)
{
  b->id = Value::UnvisitedID;
}

void LLVMOrderAccumulator::reset(Metadata *m)
{
  if(!m || m->id == Value::UnvisitedID)
    return;
  m->id = Value::UnvisitedID;

  reset(m->value);

  for(Metadata *c : m->children)
    reset(c);
}

void LLVMOrderAccumulator::reset(Instruction *i)
{
  if(!i || i->id == Value::UnvisitedID)
    return;

  i->id = Value::UnvisitedID;

  for(Value *a : i->args)
    reset(a);

  for(const rdcpair<uint64_t, Metadata *> &m : i->getAttachedMeta())
    reset(m.second);
}

void LLVMOrderAccumulator::reset(Function *f)
{
  f->id = Value::UnvisitedID;
  for(Instruction *i : f->args)
    reset(i);
  for(Instruction *i : f->instructions)
    reset(i);
  for(Block *b : f->blocks)
    reset(b);
  for(rdcpair<uint64_t, Metadata *> &m : f->attachedMeta)
    reset(m.second);
}

void LLVMOrderAccumulator::reset(Value *v)
{
  if(Constant *c = cast<Constant>(v))
    reset(c);
  else if(Instruction *i = cast<Instruction>(v))
    reset(i);
  else if(Block *b = cast<Block>(v))
    reset(b);
  else if(GlobalVar *g = cast<GlobalVar>(v))
    reset(g);
  else if(Metadata *m = cast<Metadata>(v))
    reset(m);
  else if(Function *f = cast<Function>(v))
    reset(f);
  else if(Alias *a = cast<Alias>(v))
    reset(a);
}

void LLVMOrderAccumulator::processGlobals(Program *prog, bool doLiveChecking)
{
  // reset all IDs, so we know if we're encountering a new value/metadata or not when walking
  for(Type *t : prog->m_Types)
    t->id = unvisitedTypeId;
  for(GlobalVar *v : prog->m_GlobalVars)
    reset(v);
  for(Alias *a : prog->m_Aliases)
    reset(a);
  for(Metadata *m : prog->m_NamedMeta)
    reset(m);
  for(Function *f : prog->m_Functions)
    reset(f);

  liveChecking = doLiveChecking;

  // just for extra fun, the search order for types for printing, and types enumerated while getting
  // values is slightly different! yay yay yay!
  for(const GlobalVar *g : prog->m_GlobalVars)
  {
    accumulateTypePrintOrder(g->type);
    if(g->initialiser)
      accumulateTypePrintOrder(g->initialiser->type);
  }

  for(const Alias *a : prog->m_Aliases)
  {
    accumulateTypePrintOrder(a->type);
    accumulateTypePrintOrder(a->val->type);
  }

  // use same array to avoid resizes, but we clear it each time instead of keeping a mega-list to
  // reduce the cost of lookups
  rdcarray<const Metadata *> visited;
  visited.reserve(128);

  for(const Function *func : prog->m_Functions)
  {
    accumulateTypePrintOrder(func->type);
    for(const Instruction *arg : func->args)
      accumulateTypePrintOrder(arg->type);
    for(const Instruction *inst : func->instructions)
    {
      accumulateTypePrintOrder(inst->type);
      for(size_t a = 0; a < inst->args.size(); a++)
        if(inst->args[a]->kind() != ValueKind::Instruction)
          accumulateTypePrintOrder(inst->args[a]->type);

      for(size_t m = 0; m < inst->getAttachedMeta().size(); m++)
      {
        visited.clear();
        accumulateTypePrintOrder(visited, inst->getAttachedMeta()[m].second);
      }
    }
  }

  for(Metadata *meta : prog->m_NamedMeta)
  {
    visited.clear();
    accumulateTypePrintOrder(visited, meta);
  }

  if(!liveChecking)
  {
    for(const GlobalVar *g : prog->m_GlobalVars)
      accumulate(g);

    for(const Function *f : prog->m_Functions)
    {
      accumulate(f);
      assignTypeId(prog->GetPointerType(f->type, Type::PointerAddrSpace::Default));
    }

    for(const Alias *a : prog->m_Aliases)
      accumulate(a);
  }

  firstConst = values.size();

  if(!liveChecking)
  {
    for(const GlobalVar *g : prog->m_GlobalVars)
      if(g->initialiser)
        accumulate(g->initialiser);

    for(const Alias *a : prog->m_Aliases)
      accumulate(a->val);

    for(const Value *v : prog->m_ValueSymtabOrder)
      accumulate(v);
  }

  assignTypeId(prog->m_MetaType);

  for(size_t i = 0; i < prog->m_NamedMeta.size(); i++)
  {
    // named meta node itself doesn't go into meta list, so manually iterate children here
    for(const Metadata *child : prog->m_NamedMeta[i]->children)
      accumulate(child);

    // reset its id though so we don't permanently mark named meta as unvisited and be unable to
    // reset all meta again
    prog->m_NamedMeta[i]->id = Value::NoID;
  }

  // accumulate metadata in functions, and constants referenced from there
  for(const Function *func : prog->m_Functions)
  {
    for(const Instruction *arg : func->args)
      assignTypeId(arg->type);
    for(size_t m = 0; m < func->attachedMeta.size(); m++)
      accumulate(func->attachedMeta[m].second);
    for(const Instruction *inst : func->instructions)
    {
      for(size_t a = 0; a < inst->args.size(); a++)
      {
        assignTypeId(inst->args[a]->type);
        accumulate(cast<Metadata>(inst->args[a]));
        assignTypeId(cast<Constant>(inst->args[a]));
      }
      assignTypeId(inst->type);
      for(size_t m = 0; m < inst->getAttachedMeta().size(); m++)
        accumulate(inst->getAttachedMeta()[m].second);
    }
  }

  numConsts = values.size() - firstConst;
  // don't skip constants when doing live checking, because then constants won't be contiguous as
  // globals referenced later will be pulled into values later. When skipping globals we only care
  // if they are seen at all (and given a value id)
  sortConsts = !prog->m_Uselists && !liveChecking;

  if(sortConsts)
  {
    // mimic LLVM's sorting, by type ID then refcount
    std::stable_sort(values.begin() + firstConst, values.end(), [](const Value *a, const Value *b) {
      const Constant *ca = cast<const Constant>(a);
      const Constant *cb = cast<const Constant>(b);
      if(ca->type->id != cb->type->id)
        return ca->type->id < cb->type->id;

      return ca->refCount > cb->refCount;
    });

    // int or int vectors before everything else
    std::partition(values.begin() + firstConst, values.end(),
                   [](const Value *a) { return a->type->scalarType == Type::Int; });

    // reassign value IDs after sort
    for(size_t i = firstConst; i < firstConst + numConsts; i++)
    {
      Value *value = (Value *)values[i];
      RDCASSERT(value->id >= firstConst && value->id < firstConst + numConsts, value->id,
                firstConst, numConsts);
      value->id = i;
    }
  }
}

void LLVMOrderAccumulator::processFunction(const Function *f)
{
  const Function &func = *f;

  functionWaterMark = values.size();

  for(size_t j = 0; j < func.args.size(); j++)
    accumulate(func.args[j]);

  firstFuncConst = values.size();

  for(const Instruction *inst : func.instructions)
  {
    for(size_t a = 0; a < inst->args.size(); a++)
      accumulate(cast<Constant>(inst->args[a]));
    accumulate(inst->getFuncCall());
  }

  numFuncConsts = values.size() - firstFuncConst;

  if(sortConsts)
  {
    // mimic LLVM's sorting, by type ID then refcount
    std::stable_sort(values.begin() + firstFuncConst, values.end(),
                     [](const Value *a, const Value *b) {
                       const Constant *ca = cast<const Constant>(a);
                       const Constant *cb = cast<const Constant>(b);
                       if(ca->type->id != cb->type->id)
                         return ca->type->id < cb->type->id;

                       return ca->refCount > cb->refCount;
                     });

    std::partition(values.begin() + firstFuncConst, values.end(),
                   [](const Value *a) { return a->type->scalarType == Type::Int; });

    // reassign value IDs after sort
    for(size_t i = firstFuncConst; i < firstFuncConst + numFuncConsts; i++)
    {
      Value *value = (Value *)values[i];
      RDCASSERT(value->id >= firstFuncConst && value->id < firstFuncConst + numFuncConsts,
                value->id, firstFuncConst, numFuncConsts);
      value->id = i;
    }
  }

  for(size_t j = 0; j < func.blocks.size(); j++)
    func.blocks[j]->id = j;

  uint32_t slot = 0;
  uint32_t curBlock = 0;

  for(Instruction *arg : func.args)
    if(arg->getName().isEmpty())
      arg->slot = slot++;

  if(!func.blocks.empty() && func.blocks[0]->name.empty())
    func.blocks[0]->slot = slot++;

  for(Instruction *inst : func.instructions)
  {
    RDCASSERT(curBlock < func.blocks.size());

    for(size_t m = 0; m < inst->getAttachedMeta().size(); m++)
      accumulate(inst->getAttachedMeta()[m].second);

    for(size_t a = 0; a < inst->args.size(); a++)
      if(inst->args[a]->kind() == ValueKind::Constant || liveChecking)
        accumulate(inst->args[a]);

    if(inst->type->isVoid())
    {
      inst->id = Value::NoID;
    }
    else
    {
      accumulate(inst);

      if(inst->getName().isEmpty())
        inst->slot = slot++;
    }

    if(inst->op == Operation::Branch || inst->op == Operation::Unreachable ||
       inst->op == Operation::Switch || inst->op == Operation::Ret)
    {
      curBlock++;

      if(curBlock < func.blocks.size() && func.blocks[curBlock]->name.empty())
        func.blocks[curBlock]->slot = slot++;
    }
  }
}

void LLVMOrderAccumulator::exitFunction()
{
  values.resize(functionWaterMark);
}

void LLVMOrderAccumulator::accumulateTypePrintOrder(rdcarray<const Metadata *> &visited,
                                                    const Metadata *m)
{
  // metadata can be self-referential (why???) so need to check if we have visited this one to avoid
  // infinite recursion. We don't set the ID as a flag since then we'd need a type-only reset pass.
  // Blech
  if(visited.contains(m))
    return;

  visited.push_back(m);

  accumulateTypePrintOrder(m->type);

  if(m->value)
    accumulateTypePrintOrder(m->value->type);

  for(const Metadata *c : m->children)
    if(c)
      accumulateTypePrintOrder(visited, c);
}

void LLVMOrderAccumulator::accumulateTypePrintOrder(const Type *t)
{
  if(!t || printOrderTypes.contains(t))
    return;

  Type *type = (Type *)t;

  // LLVM doesn't do quite a depth-first search for ordering its types for *printing*, so we
  // replicate its search order to ensure types are printed in the same order.
  rdcarray<const Type *> workingSet;
  workingSet.push_back(type);
  do
  {
    const Type *cur = workingSet.back();
    workingSet.pop_back();

    printOrderTypes.push_back(cur);

    for(size_t i = 0; i < cur->members.size(); i++)
    {
      const Type *member = cur->members[cur->members.size() - 1 - i];
      if(!printOrderTypes.contains(member) && !workingSet.contains(member))
      {
        workingSet.push_back((Type *)member);
      }
    }

    if(cur->inner && !printOrderTypes.contains(cur->inner) && !workingSet.contains(cur->inner))
    {
      workingSet.push_back((Type *)cur->inner);
    }
  } while(!workingSet.empty());
}

void LLVMOrderAccumulator::assignTypeId(const Type *t)
{
  if(!t || t->id != unvisitedTypeId)
    return;

  assignTypeId((Type *)t->inner);
  for(size_t i = 0; i < t->members.size(); i++)
    assignTypeId((Type *)t->members[i]);

  Type *type = (Type *)t;
  type->id = types.size() & 0xffff;
  types.push_back(t);
}

void LLVMOrderAccumulator::assignTypeId(const Constant *c)
{
  if(!c)
    return;

  assignTypeId(c->type);
  if(c->isCast())
    assignTypeId(cast<Constant>(c->getInner()));
  else if(c->isCompound())
    for(Value *v : c->getMembers())
      assignTypeId(cast<Constant>(v));
}

void LLVMOrderAccumulator::accumulate(const Value *v)
{
  Value *value = (Value *)v;
  if(!v || v->id != Value::UnvisitedID)
  {
    Constant *c = cast<Constant>(value);
    if(c)
      c->refCount++;
    return;
  }

  RDCASSERT(v->kind() != ValueKind::Metadata);

  assignTypeId(value->type);

  value->id = Value::VisitedID;

  if(Constant *c = cast<Constant>(value))
  {
    if(c->isCast())
    {
      accumulate(c->getInner());
    }
    else if(c->isCompound())
    {
      for(Value *m : c->getMembers())
        accumulate(m);
    }

    c->refCount = 1;
  }

  value->id = values.size();
  values.push_back(v);
}

void LLVMOrderAccumulator::accumulate(const Metadata *m)
{
  if(!m || m->id != Value::UnvisitedID)
    return;

  Metadata *meta = (Metadata *)m;
  meta->id = Value::VisitedID;

  for(const Metadata *c : m->children)
    if(c)
      accumulate(c);

  if(const Constant *c = cast<Constant>(m->value))
    accumulate(c);

  meta->id = metadata.size();
  metadata.push_back(meta);
}

};    // namespace DXIL
