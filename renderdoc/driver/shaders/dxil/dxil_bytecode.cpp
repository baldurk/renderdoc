/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

namespace DXIL
{
using namespace LLVMBC;

void ParseConstant(const LLVMBC::BlockOrRecord &constant, const Type *&curType,
                   std::function<const Type *(uint64_t)> getType,
                   std::function<const Type *(const Type *, Type::PointerAddrSpace)> getPtrType,
                   std::function<const Value *(uint64_t)> getValue,
                   std::function<void(const Constant &)> addConstant)
{
  if(IS_KNOWN(constant.id, ConstantsRecord::SETTYPE))
  {
    curType = getType(constant.ops[0]);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL) ||
          IS_KNOWN(constant.id, ConstantsRecord::UNDEF))
  {
    Constant c;
    c.type = curType;
    c.nullconst = IS_KNOWN(constant.id, ConstantsRecord::CONST_NULL);
    c.undef = IS_KNOWN(constant.id, ConstantsRecord::UNDEF);
    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::INTEGER))
  {
    Constant c;
    c.type = curType;
    c.val.s64v[0] = LLVMBC::BitReader::svbr(constant.ops[0]);
    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::FLOAT))
  {
    Constant c;
    c.type = curType;
    memcpy(&c.val.u64v[0], &constant.ops[0], curType->bitWidth / 8);
    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::STRING) ||
          IS_KNOWN(constant.id, ConstantsRecord::CSTRING))
  {
    Constant c;
    c.type = curType;
    c.str = constant.getString(0);
    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_CAST))
  {
    Constant c;
    c.op = DecodeCast(constant.ops[0]);
    c.type = curType;
    const Value *v = getValue(constant.ops[2]);
    if(v->type == ValueType::Unknown)
    {
      c.inner = Value(Value::ForwardRef, v);
    }
    else
    {
      RDCASSERT(v->GetType() == getType(constant.ops[1]));
      c.inner = *v;
    }
    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::EVAL_GEP))
  {
    Constant c;

    c.op = Operation::GetElementPtr;

    size_t idx = 0;
    if(constant.ops.size() & 1)
      c.type = getType(constant.ops[idx++]);

    for(; idx < constant.ops.size(); idx += 2)
    {
      const Type *t = getType(constant.ops[idx]);
      const Value *v = getValue(constant.ops[idx + 1]);
      RDCASSERT(v->type != ValueType::Unknown && v->GetType() == t);

      c.members.push_back(*v);
    }

    c.type = c.members[0].GetType()->inner;

    // walk the type list to get the return type
    for(idx = 2; idx < c.members.size(); idx++)
    {
      if(c.type->type == Type::Vector || c.type->type == Type::Array)
      {
        c.type = c.type->inner;
      }
      else if(c.type->type == Type::Struct)
      {
        c.type = c.type->members[c.members[idx].constant->val.u32v[0]];
      }
      else
      {
        RDCERR("Unexpected type %d encountered in GEP", c.type->type);
      }
    }

    // the result is a pointer to the return type
    c.type = getPtrType(c.type, curType->addrSpace);

    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::AGGREGATE))
  {
    Constant c;
    c.type = curType;
    if(c.type->type == Type::Vector)
    {
      // only handle 4-wide vectors right now
      RDCASSERT(constant.ops.size() <= 4);

      // inline vectors
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        const Value *member = getValue(constant.ops[m]);
        RDCASSERT(member->type != ValueType::Unknown);

        c.members.push_back(*member);

        if(member->type != ValueType::Unknown)
        {
          if(c.type->bitWidth <= 32)
            c.val.u32v[m] = member->constant->val.u32v[m];
          else
            c.val.u64v[m] = member->constant->val.u64v[m];
        }
        else
        {
          RDCERR("Forward reference unexpected for vector");
        }
      }
    }
    else
    {
      for(uint64_t m : constant.ops)
        c.members.push_back(Value(Value::ForwardRef, getValue(m)));
    }
    addConstant(c);
  }
  else if(IS_KNOWN(constant.id, ConstantsRecord::DATA))
  {
    Constant c;
    c.type = curType;
    c.data = true;
    if(c.type->type == Type::Vector)
    {
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        if(c.type->bitWidth <= 32)
          c.val.u32v[m] = constant.ops[m] & ((1ULL << c.type->bitWidth) - 1);
        else
          c.val.u64v[m] = constant.ops[m];
      }
    }
    else
    {
      for(size_t m = 0; m < constant.ops.size(); m++)
      {
        uint64_t val = 0;
        if(c.type->inner->bitWidth <= 32)
          val = constant.ops[m] & ((1ULL << c.type->inner->bitWidth) - 1);
        else
          val = constant.ops[m];
        c.members.push_back(Value(val));
      }
    }
    addConstant(c);
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
  Value getSymbol(uint64_t val) { return prog->m_Values[prog->m_Values.size() - (size_t)val]; }
  Value getSymbol(bool withType = true)
  {
    // get the value
    uint64_t val = get<uint64_t>();

    // if it's not a forward reference, resolve the relative-ness and return
    if(val <= prog->m_Values.size())
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
      return Value(Value::ForwardRef, &prog->m_Values.back() + 1 - (int32_t)val);
    }
  }

  // some symbols are referenced absolute, not relative
  Value getSymbolAbsolute() { return prog->m_Values[get<size_t>()]; }
  const Type *getType() { return &prog->m_Types[get<size_t>()]; }
  const Type *getType(const Function &f, Value v)
  {
    if(v.type == ValueType::Unknown)
      return m_LastType;
    return v.GetType();
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

const Type *Value::GetType() const
{
  switch(type)
  {
    case ValueType::Constant: return constant->type;
    case ValueType::Instruction: return instruction->type;
    case ValueType::GlobalVar: return global->type;
    case ValueType::Function: return function->funcType;
    case ValueType::Metadata: return meta->type;
    case ValueType::Unknown:
    case ValueType::Alias:
    case ValueType::BasicBlock:
    case ValueType::Literal: RDCERR("Unexpected symbol to get type for %d", type); break;
  }
  return NULL;
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

const Metadata *Program::GetMetadataByName(const rdcstr &name) const
{
  for(size_t i = 0; i < m_NamedMeta.size(); i++)
    if(m_NamedMeta[i].name == name)
      return &m_NamedMeta[i];

  return NULL;
}

void ResolveForwardReference(Value &v)
{
  if(!v.empty() && v.type == ValueType::Unknown)
  {
    v = *v.value;
    RDCASSERT(v.type == ValueType::Constant || v.type == ValueType::Literal);
  }
}

Program::Program(const byte *bytes, size_t length)
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
        GlobalVar g;

        g.type = &m_Types[(size_t)rootchild.ops[0]];
        if(rootchild.ops[1] & 0x1)
          g.flags |= GlobalFlags::IsConst;

        Type::PointerAddrSpace addrSpace = g.type->addrSpace;
        if(rootchild.ops[1] & 0x2)
          addrSpace = Type::PointerAddrSpace(rootchild.ops[1] >> 2);

        if(rootchild.ops[2])
          g.initialiser += rootchild.ops[2];

        switch(rootchild.ops[3])
        {
          case 0: g.flags |= GlobalFlags::ExternalLinkage; break;
          case 16: g.flags |= GlobalFlags::WeakAnyLinkage; break;
          case 2: g.flags |= GlobalFlags::AppendingLinkage; break;
          case 3: g.flags |= GlobalFlags::InternalLinkage; break;
          case 18: g.flags |= GlobalFlags::LinkOnceAnyLinkage; break;
          case 7: g.flags |= GlobalFlags::ExternalWeakLinkage; break;
          case 8: g.flags |= GlobalFlags::CommonLinkage; break;
          case 9: g.flags |= GlobalFlags::PrivateLinkage; break;
          case 17: g.flags |= GlobalFlags::WeakODRLinkage; break;
          case 19: g.flags |= GlobalFlags::LinkOnceODRLinkage; break;
          case 12: g.flags |= GlobalFlags::AvailableExternallyLinkage; break;
          default: break;
        }

        g.align = (1ULL << rootchild.ops[4]) >> 1;

        g.section = int32_t(rootchild.ops[5]) - 1;

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
            g.flags |= GlobalFlags::GlobalUnnamedAddr;
          else if(rootchild.ops[8] == 2)
            g.flags |= GlobalFlags::LocalUnnamedAddr;
        }

        if(rootchild.ops.size() > 9)
        {
          if(rootchild.ops[9])
            g.flags |= GlobalFlags::ExternallyInitialised;
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

        const Type *ptrType = GetPointerType(g.type, addrSpace);

        if(ptrType == g.type)
          RDCERR("Expected to find pointer type for global variable");

        g.type = ptrType;

        m_GlobalVars.push_back(g);
        m_Values.push_back(Value(&m_GlobalVars.back()));
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::FUNCTION))
      {
        // [type, callingconv, isproto, linkage, paramattrs, alignment, section, visibility, gc,
        // unnamed_addr, prologuedata, dllstorageclass, comdat, prefixdata]
        Function f;

        f.funcType = &m_Types[(size_t)rootchild.ops[0]];
        // ignore callingconv
        RDCASSERTMSG("Calling convention is non-default", rootchild.ops[1] == 0);
        f.external = (rootchild.ops[2] != 0);
        // ignore linkage
        RDCASSERTMSG("Linkage is non-default", rootchild.ops[3] == 0);
        if(rootchild.ops[4] > 0 && rootchild.ops[4] - 1 < m_AttributeSets.size())
          f.attrs = &m_AttributeSets[(size_t)rootchild.ops[4] - 1];

        f.align = rootchild.ops[5];

        // ignore rest of properties, assert that if present they are 0
        for(size_t p = 6; p < rootchild.ops.size(); p++)
          RDCASSERT(rootchild.ops[p] == 0, p, rootchild.ops[p]);

        if(!f.external)
          functionDecls.push_back(m_Functions.size());

        m_Functions.push_back(f);
        m_Values.push_back(Value(&m_Functions.back()));
      }
      else if(IS_KNOWN(rootchild.id, ModuleRecord::ALIAS))
      {
        // [alias type, aliasee val#, linkage, visibility]
        Alias a;

        a.type = &m_Types[(size_t)rootchild.ops[0]];
        a.val = m_Values[(size_t)rootchild.ops[1]];

        // ignore rest of properties, assert that if present they are 0
        for(size_t p = 2; p < rootchild.ops.size(); p++)
          RDCASSERT(rootchild.ops[p] == 0, p, rootchild.ops[p]);

        m_Aliases.push_back(a);
        m_Values.push_back(Value(&m_Aliases.back()));
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

          AttributeGroup group;

          size_t id = (size_t)attrgroup.ops[0];
          group.slotIndex = (uint32_t)attrgroup.ops[1];

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

                group.strs.push_back({a, b});
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

          AttributeSet attrs;

          attrs.orderedGroups = paramattr.ops;

          for(uint64_t g : paramattr.ops)
          {
            if(g < m_AttributeGroups.size())
            {
              const AttributeGroup &group = m_AttributeGroups[(size_t)g];
              if(group.slotIndex == AttributeGroup::FunctionSlot)
              {
                RDCASSERT(attrs.functionSlot == NULL);
                attrs.functionSlot = &group;
              }
              else
              {
                attrs.groupSlots.resize_for_index(group.slotIndex);
                attrs.groupSlots[group.slotIndex] = &m_AttributeGroups[(size_t)g];
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
            m_Types[typeIndex].opaque = true;

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
      else if(IS_KNOWN(rootchild.id, KnownBlock::CONSTANTS_BLOCK))
      {
        const Type *t = NULL;

        // resize then clear to ensure the constants array memory that we reserve is cleared to 0
        m_Constants.resize(rootchild.children.size());
        m_Constants.clear();

        // ensure forward references stay valid until we resolve them after the loop
        size_t sz = m_Values.size();
        m_Values.resize(sz + rootchild.children.size());
        m_Values.resize(sz);

        for(const LLVMBC::BlockOrRecord &constant : rootchild.children)
        {
          if(constant.IsBlock())
          {
            RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
            continue;
          }

          ParseConstant(constant, t, [this](uint64_t op) { return &m_Types[(size_t)op]; },
                        [this](const Type *t, Type::PointerAddrSpace addrSpace) {
                          return GetPointerType(t, addrSpace);
                        },
                        [this](uint64_t v) { return &m_Values[(size_t)v]; },
                        [this](const Constant &v) {
                          m_Constants.push_back(v);
                          m_Values.push_back(Value(&m_Constants.back()));
                        });
        }

        for(size_t i = 0; i < m_Constants.size(); i++)
        {
          for(Value &v : m_Constants[i].members)
            ResolveForwardReference(v);
          ResolveForwardReference(m_Constants[i].inner);
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
          if(vidx < m_Values.size())
          {
            const Value &v = m_Values[vidx];
            rdcstr str = symtab.getString(1);

            GetValueSymtabString(v) = str;

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
            m_Kinds.resize_for_index(kind);
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
              meta.isString = true;
              meta.str = metaRecord.getString();
            }
            else if(IS_KNOWN(metaRecord.id, MetaDataRecord::VALUE))
            {
              meta.isConstant = true;
              meta.value = m_Values[(size_t)metaRecord.ops[1]];
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
      else if(IS_KNOWN(rootchild.id, KnownBlock::FUNCTION_BLOCK))
      {
        Function &f = m_Functions[functionDecls[0]];
        functionDecls.erase(0);

        // conservative resize here so we can take pointers and have them stay valid
        f.instructions.reserve(rootchild.children.size());

        auto getMeta = [this, &f](uint64_t v) {
          size_t idx = (size_t)v;
          return idx - 1 < m_Metadata.size() ? &m_Metadata[idx] : &f.metadata[idx];
        };
        auto getMetaOrNull = [this, &f](uint64_t v) {
          size_t idx = (size_t)v;
          return idx == 0 ? NULL : (idx - 1 < m_Metadata.size() ? &m_Metadata[idx - 1]
                                                                : &f.metadata[idx - 1]);
        };

        size_t prevNumSymbols = m_Values.size();
        size_t instrSymbolStart = 0;

        f.args.reserve(f.funcType->members.size());
        for(size_t i = 0; i < f.funcType->members.size(); i++)
        {
          Instruction arg;
          arg.type = f.funcType->members[i];
          arg.name = StringFormat::Fmt("arg%zu", i);
          f.args.push_back(arg);
          m_Values.push_back(Value(&f.args.back()));
        }

        size_t curBlock = 0;
        int32_t debugLocIndex = -1;

        // reserve enough values for the instructions (conservatively)
        {
          size_t sz = m_Values.size();
          m_Values.resize(sz + rootchild.children.size());
          m_Values.resize(sz);
        }
        const Value *valueStorage = m_Values.data();

        for(const LLVMBC::BlockOrRecord &funcChild : rootchild.children)
        {
          if(funcChild.IsBlock())
          {
            if(IS_KNOWN(funcChild.id, KnownBlock::CONSTANTS_BLOCK))
            {
              // resize then clear to ensure the constants array memory that we reserve is cleared
              // to 0
              f.constants.resize(funcChild.children.size());
              f.constants.clear();

              // reserve enough values for constants and instructions. We should encounter this
              // before anything that can forward reference values
              {
                size_t sz = m_Values.size();
                m_Values.resize(sz + rootchild.children.size() + funcChild.children.size());
                m_Values.resize(sz);
              }
              valueStorage = m_Values.data();

              const Type *t = NULL;
              for(const LLVMBC::BlockOrRecord &constant : funcChild.children)
              {
                if(constant.IsBlock())
                {
                  RDCERR("Unexpected subblock in CONSTANTS_BLOCK");
                  continue;
                }

                ParseConstant(constant, t, [this](uint64_t op) { return &m_Types[(size_t)op]; },
                              [this](const Type *t, Type::PointerAddrSpace addrSpace) {
                                return GetPointerType(t, addrSpace);
                              },
                              [this](uint64_t v) { return &m_Values[(size_t)v]; },
                              [this, &f](const Constant &v) {
                                f.constants.push_back(v);
                                m_Values.push_back(Value(&f.constants.back()));
                              });
              }

              for(size_t i = 0; i < f.constants.size(); i++)
              {
                for(Value &v : f.constants[i].members)
                  ResolveForwardReference(v);
                ResolveForwardReference(f.constants[i].inner);
              }

              instrSymbolStart = m_Values.size();
            }
            else if(IS_KNOWN(funcChild.id, KnownBlock::METADATA_BLOCK))
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
                  if(idx < m_Values.size())
                  {
                    meta.value = m_Values[idx];
                  }
                  else
                  {
                    // forward reference
                    meta.value = Value(Value::ForwardRef, &m_Values[idx]);
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

                  if(idx >= m_Values.size())
                  {
                    RDCERR("Out of bounds symbol index %zu (%s) in function symbol table", idx,
                           symtab.getString(1).c_str());
                    continue;
                  }

                  const Value &v = m_Values[idx];
                  rdcstr str = symtab.getString(1);

                  GetValueSymtabString(v) = str;

                  if(!f.valueSymtabOrder.empty())
                    f.sortedSymtab &= GetValueSymtabString(f.valueSymtabOrder.back()) < str;

                  f.valueSymtabOrder.push_back(v);
                }
                else if(IS_KNOWN(symtab.id, ValueSymtabRecord::BBENTRY))
                {
                  Value v(&f.blocks[(size_t)symtab.ops[0]]);
                  rdcstr str = symtab.getString(1);

                  GetValueSymtabString(v) = str;

                  if(!f.valueSymtabOrder.empty())
                    f.sortedSymtab &= GetValueSymtabString(f.valueSymtabOrder.back()) < str;

                  f.valueSymtabOrder.push_back(v);
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
            else if(IS_KNOWN(funcChild.id, KnownBlock::USELIST_BLOCK))
            {
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
                  u.value = m_Values[(size_t)u.shuffle.back()];
                  u.shuffle.pop_back();
                  f.uselist.push_back(u);
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
              size_t attr = op.get<size_t>();
              if(attr > 0)
                inst.paramAttrs = &m_AttributeSets[attr - 1];

              uint64_t callingFlags = op.get<uint64_t>();

              if(callingFlags & (1ULL << 17))
              {
                inst.opFlags = op.get<InstructionFlags>();
                RDCASSERT(inst.opFlags != InstructionFlags::NoFlags);

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

              Value v = op.getSymbol();

              if(v.type != ValueType::Function)
              {
                RDCERR("Unexpected symbol type %d called in INST_CALL", v.type);
                continue;
              }

              inst.funcCall = v.function;
              inst.type = inst.funcCall->funcType->inner;

              if(funcCallType)
              {
                RDCASSERT(funcCallType == inst.funcCall->funcType);
              }

              for(size_t i = 0; op.remaining() > 0; i++)
              {
                if(inst.funcCall->funcType->members[i]->type == Type::Metadata)
                {
                  int32_t offs = (int32_t)op.get<uint32_t>();
                  size_t idx = m_Values.size() - offs;
                  if(idx < m_Metadata.size())
                    v = Value(&m_Metadata[idx]);
                  else
                    v = Value(&f.metadata[idx - m_Metadata.size()]);
                }
                else
                {
                  v = op.getSymbol(false);
                }
                inst.args.push_back(v);
              }

              RDCASSERTEQUAL(inst.args.size(), inst.funcCall->funcType->members.size());

              f.instructions.push_back(inst);

              if(!inst.type->isVoid())
                m_Values.push_back(Value(&f.instructions.back()));
              if(inst.funcCall->name == "dx.op.createHandleFromHeap")
                m_directHeapAccessCount++;
            }
            else if(op.type == FunctionRecord::INST_CAST)
            {
              Instruction inst;

              inst.args.push_back(op.getSymbol());
              inst.type = op.getType();

              uint64_t opcode = op.get<uint64_t>();
              inst.op = DecodeCast(opcode);

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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
                inst.args.push_back(Value(val));
              }

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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
              }

              curBlock++;

              f.instructions.push_back(inst);

              if(!inst.args.empty())
                m_Values.push_back(Value(&f.instructions.back()));
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

                RDCASSERT(inst.opFlags != InstructionFlags::NoFlags);
              }

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
            }
            else if(op.type == FunctionRecord::INST_UNREACHABLE)
            {
              Instruction inst;

              inst.op = Operation::Unreachable;

              inst.type = GetVoidType();

              curBlock++;

              f.instructions.push_back(inst);
            }
            else if(op.type == FunctionRecord::INST_ALLOCA)
            {
              Instruction inst;

              inst.op = Operation::Alloca;

              inst.type = op.getType();

              // we now have the inner type, but this instruction returns a pointer to that type so
              // adjust
              inst.type = GetPointerType(inst.type, Type::PointerAddrSpace::Default);

              RDCASSERT(inst.type->type == Type::Pointer);

              // type of the size - ignored
              const Type *sizeType = op.getType();
              // size
              inst.args.push_back(op.getSymbolAbsolute());

              RDCASSERT(sizeType == inst.args.back().GetType());

              uint64_t align = op.get<uint64_t>();

              if(align & 0x20)
              {
                // argument alloca
                inst.opFlags |= InstructionFlags::ArgumentAlloca;
              }
              if((align & 0x40) == 0)
              {
                RDCASSERT(inst.type->type == Type::Pointer);
                inst.type = inst.type->inner;
              }

              align &= ~0xE0;

              inst.align = (1U << align) >> 1;

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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
                  Value v = inst.args[idx];
                  // if it's a struct the index must be constant
                  RDCASSERT(v.type == ValueType::Constant);
                  inst.type = inst.type->members[v.constant->val.u32v[0]];
                }
                else
                {
                  RDCERR("Unexpected type %d encountered in GEP", inst.type->type);
                }
              }

              // get the pointer type
              inst.type = GetPointerType(inst.type, op.getType(f, inst.args[0])->addrSpace);

              RDCASSERT(inst.type->type == Type::Pointer);

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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
              {
                inst.opFlags = op.get<InstructionFlags>();

                RDCASSERTNOTEQUAL((uint64_t)inst.opFlags, 0);
              }

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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
            }
            else if(op.type == FunctionRecord::INST_BR)
            {
              Instruction inst;

              inst.op = Operation::Branch;

              inst.type = GetVoidType();

              // true destination
              uint64_t trueDest = op.get<uint64_t>();
              inst.args.push_back(Value(&f.blocks[(size_t)trueDest]));
              f.blocks[(size_t)trueDest].preds.insert(0, &f.blocks[curBlock]);

              if(op.remaining() > 0)
              {
                // false destination
                uint64_t falseDest = op.get<uint64_t>();
                inst.args.push_back(Value(&f.blocks[(size_t)falseDest]));
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
                size_t defaultDest = op.get<size_t>();
                inst.args.push_back(Value(&f.blocks[defaultDest]));
                f.blocks[defaultDest].preds.insert(0, &f.blocks[curBlock]);

                RDCERR("Unsupported switch instruction version");
              }
              else
              {
                // condition
                inst.args.push_back(op.getSymbol(false));

                // default block
                size_t defaultDest = op.get<size_t>();
                inst.args.push_back(Value(&f.blocks[defaultDest]));
                f.blocks[defaultDest].preds.insert(0, &f.blocks[curBlock]);

                uint64_t numCases = op.remaining() / 2;

                for(uint64_t c = 0; c < numCases; c++)
                {
                  // case value, absolute not relative
                  inst.args.push_back(op.getSymbolAbsolute());

                  // case block
                  size_t caseDest = op.get<size_t>();
                  inst.args.push_back(Value(&f.blocks[caseDest]));
                  f.blocks[caseDest].preds.insert(0, &f.blocks[curBlock]);
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

                if(valSrc <= 0)
                {
                  inst.args.push_back(Value(Value::ForwardRef,
                                            &m_Values[size_t((int64_t)m_Values.size() - valSrc)]));
                }
                else
                {
                  inst.args.push_back(op.getSymbol((uint64_t)valSrc));
                }
                inst.args.push_back(Value(&f.blocks[(size_t)blockSrc]));
              }

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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
              // DXIL claims to be scalarised but lol that's a lie

              Instruction inst;

              inst.op = Operation::ExtractElement;

              // vector
              inst.args.push_back(op.getSymbol());

              // result is the scalar type within the vector
              inst.type = op.getType(f, inst.args.back())->inner;

              // index
              inst.args.push_back(op.getSymbol());

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
            }
            else if(op.type == FunctionRecord::INST_INSERTELT)
            {
              // DXIL claims to be scalarised but lol that's a lie

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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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
                inst.args.push_back(Value(op.get<uint64_t>()));

              f.instructions.push_back(inst);
              m_Values.push_back(Value(&f.instructions.back()));
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

        RDCASSERT(valueStorage == m_Values.data());

        RDCASSERT(curBlock == f.blocks.size());

        size_t resultID = 0;

        if(f.blocks[0].name.empty())
          f.blocks[0].resultID = (uint32_t)resultID++;

        for(size_t i = 0; i < f.metadata.size(); i++)
        {
          Value &v = f.metadata[i].value;
          if(!v.empty() && v.type == ValueType::Unknown)
          {
            v = *v.value;
            RDCASSERT(v.type == ValueType::Instruction);
          }
        }

        curBlock = 0;
        for(size_t i = 0; i < f.instructions.size(); i++)
        {
          // fix up forward references here, we couldn't write them up front because we didn't know
          // how many actual symbols (non-void instructions) existed after the given instruction
          for(Value &s : f.instructions[i].args)
          {
            if(s.type == ValueType::Unknown)
            {
              s = *s.value;
              RDCASSERT(s.type == ValueType::Instruction);
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

        f.values.assign(m_Values.data() + prevNumSymbols, m_Values.size() - prevNumSymbols);
        m_Values.resize(prevNumSymbols);
      }
      else
      {
        RDCERR("Unknown block ID %u encountered at module scope", rootchild.id);
      }
    }
  }

  // pointer fixups. This is only needed for global variabls as it has forward references to
  // constants before we can even reserve the constants.
  for(GlobalVar &g : m_GlobalVars)
  {
    if(g.initialiser)
    {
      size_t idx = g.initialiser - (Constant *)NULL;
      Value v = m_Values[idx - 1];
      RDCASSERT(v.type == ValueType::Constant);
      g.initialiser = v.constant;
    }
  }

  RDCASSERT(functionDecls.empty());
}

rdcstr &Program::GetValueSymtabString(const Value &v)
{
  static rdcstr err;
  switch(v.type)
  {
    case ValueType::Constant: return (rdcstr &)v.constant->str;
    case ValueType::Instruction: return (rdcstr &)v.instruction->name;
    case ValueType::BasicBlock: return (rdcstr &)v.block->name;
    case ValueType::GlobalVar: return (rdcstr &)v.global->name;
    case ValueType::Function: return (rdcstr &)v.function->name;
    case ValueType::Alias: return (rdcstr &)v.alias->name;
    case ValueType::Unknown:
    case ValueType::Metadata:
    case ValueType::Literal: RDCERR("Unexpected value symtab entry referring to %d", v.type); break;
  }

  return err;
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

const DXIL::Type *Program::GetVoidType(bool precache)
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

  if(!m_VoidType && !precache)
    RDCERR("Couldn't find void type");

  return m_VoidType;
}

const DXIL::Type *Program::GetBoolType(bool precache)
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

  if(!m_BoolType && !precache)
    RDCERR("Couldn't find bool type");

  return m_BoolType;
}

const Type *Program::GetInt32Type(bool precache)
{
  if(m_Int32Type)
    return m_Int32Type;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    if(m_Types[i].type == Type::Scalar && m_Types[i].scalarType == Type::Int &&
       m_Types[i].bitWidth == 32)
    {
      m_Int32Type = &m_Types[i];
      break;
    }
  }

  if(!m_Int32Type && !precache)
    RDCERR("Couldn't find int32 type");

  return m_Int32Type;
}

const Type *Program::GetInt8Type()
{
  if(m_Int8Type)
    return m_Int8Type;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    if(m_Types[i].type == Type::Scalar && m_Types[i].scalarType == Type::Int &&
       m_Types[i].bitWidth == 8)
    {
      m_Int8Type = &m_Types[i];
      break;
    }
  }

  if(!m_Int8Type)
    RDCERR("Couldn't find int8 type");

  return m_Int8Type;
}

const Type *Program::GetPointerType(const Type *type, Type::PointerAddrSpace addrSpace) const
{
  for(const Type &t : m_Types)
  {
    if(t.type == Type::Pointer && t.inner == type && t.addrSpace == addrSpace)
    {
      return &t;
    }
  }

  return NULL;
}

Metadata::~Metadata()
{
  SAFE_DELETE(dwarf);
  SAFE_DELETE(debugLoc);
}
};    // namespace DXIL
