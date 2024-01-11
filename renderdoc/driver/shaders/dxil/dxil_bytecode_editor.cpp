/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2023 Baldur Karlsson
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

#include "dxil_bytecode_editor.h"
#include "driver/dx/official/dxcapi.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "maths/half_convert.h"
#include "dxil_bytecode.h"
#include "llvm_encoder.h"

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);
typedef DXC_API_IMPORT HRESULT(__stdcall *pDxcCreateInstance)(REFCLSID rclsid, REFIID riid,
                                                              LPVOID *ppv);

namespace DXIL
{
ProgramEditor::ProgramEditor(const DXBC::DXBCContainer *container, bytebuf &outBlob)
    : Program(container->GetNonDebugDXILByteCode(), container->GetNonDebugDXILByteCodeSize()),
      m_OutBlob(outBlob)
{
  m_OutBlob = container->GetShaderBlob();

  if(!m_VoidType)
  {
    Type *t = new(alloc) Type;
    m_VoidType = t;
    t->type = Type::Scalar;
    t->scalarType = Type::Void;
    m_Types.push_back(t);
  }
  if(!m_BoolType)
  {
    Type *t = new(alloc) Type;
    m_BoolType = t;
    t->type = Type::Scalar;
    t->scalarType = Type::Int;
    t->bitWidth = 1;
    m_Types.push_back(t);
  }
  if(!m_Int32Type)
  {
    Type *t = new(alloc) Type;
    m_Int32Type = t;
    t->type = Type::Scalar;
    t->scalarType = Type::Int;
    t->bitWidth = 32;
    m_Types.push_back(t);
  }
  if(!m_Int8Type)
  {
    Type *t = new(alloc) Type;
    m_Int8Type = t;
    t->type = Type::Scalar;
    t->scalarType = Type::Int;
    t->bitWidth = 8;
    m_Types.push_back(t);
  }

  // enumerate constants for deduplicating. The encoding automatically partitions these into global
  // (if they're referenced globally) and function, we don't need to.
  //
  // We use the accumulator here not because it's efficient, but because it handles all the
  // potential cycles that llvm puts in :(

  LLVMOrderAccumulator accum;
  accum.processGlobals(this, false);

  for(size_t idx = accum.firstConst; idx < accum.firstConst + accum.numConsts; idx++)
    m_Constants.push_back((Constant *)cast<const Constant>(accum.values[idx]));

  for(Function *f : m_Functions)
  {
    accum.processFunction(f);
    for(size_t idx = accum.firstFuncConst; idx < accum.firstFuncConst + accum.numFuncConsts; idx++)
      m_Constants.push_back((Constant *)cast<const Constant>(accum.values[idx]));
    accum.exitFunction();
  }

  m_Constants.removeIf([](Constant *a) { return a == NULL; });
}

ProgramEditor::~ProgramEditor()
{
  LLVMOrderAccumulator accum;
  accum.processGlobals(this, true);

  // delete any functions that aren't referenced by call instructions
  rdcarray<const Function *> keep;
  for(Function *f : m_Functions)
  {
    accum.processFunction(f);
    accum.exitFunction();
  }

  RDCCOMPILE_ASSERT(Value::VisitedID < Value::UnvisitedID && Value::UnvisitedID < Value::NoID,
                    "ID constants should be ordered");

  m_Functions.removeIf(
      [&keep](Function *f) { return f->instructions.empty() && f->id >= Value::UnvisitedID; });

  // delete any globals that aren't referenced
  m_GlobalVars.removeIf([&accum](GlobalVar *var) { return var->id >= Value::UnvisitedID; });

  m_ValueSymtabOrder.removeIf([this](Value *v) {
    if(v->kind() == ValueKind::Function && !m_Functions.contains(cast<Function>(v)))
      return true;

    if(v->kind() == ValueKind::GlobalVar && !m_GlobalVars.contains(cast<GlobalVar>(v)))
      return true;

    return false;
  });

  // replace the DXIL bytecode in the container with
  DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, DXBC::FOURCC_DXIL, EncodeProgram());

  // strip ILDB because it's valid code (with debug info) and who knows what might use it
  DXBC::DXBCContainer::StripChunk(m_OutBlob, DXBC::FOURCC_ILDB);

  // also strip STAT because it might have stale reflection info
  DXBC::DXBCContainer::StripChunk(m_OutBlob, DXBC::FOURCC_STAT);

#if ENABLED(RDOC_DEVEL) && 1
  // on debug builds, run through dxil for "validation" if it's available.
  // we need BOTH of htese because dxil.dll's interface is incomplete, it lacks the library
  // functionality that we only need to create blobs
  HMODULE dxil = GetModuleHandleA("dxil.dll");
  HMODULE dxc = GetModuleHandleA("dxcompiler.dll");

  if(dxc != NULL && dxil != NULL)
  {
    pDxcCreateInstance dxcCreate = (pDxcCreateInstance)GetProcAddress(dxc, "DxcCreateInstance");
    pDxcCreateInstance dxilCreate = (pDxcCreateInstance)GetProcAddress(dxil, "DxcCreateInstance");

    IDxcValidator *validator = NULL;
    HRESULT hr = dxilCreate(CLSID_DxcValidator, __uuidof(IDxcValidator), (void **)&validator);

    if(FAILED(hr))
    {
      RDCWARN("Couldn't create DXC validator");
      SAFE_RELEASE(validator);
      return;
    }

    IDxcLibrary *library = NULL;
    hr = dxcCreate(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void **)&library);

    if(FAILED(hr))
    {
      RDCWARN("Couldn't create DXC library");
      SAFE_RELEASE(validator);
      SAFE_RELEASE(library);
      return;
    }

    IDxcBlobEncoding *blob = NULL;
    hr = library->CreateBlobWithEncodingFromPinned(m_OutBlob.data(), (UINT32)m_OutBlob.size(), 0,
                                                   &blob);

    if(FAILED(hr))
    {
      RDCWARN("Couldn't create DXC byte blob");
      SAFE_RELEASE(validator);
      SAFE_RELEASE(library);
      SAFE_RELEASE(blob);
      return;
    }

    IDxcOperationResult *result;
    validator->Validate(blob, DxcValidatorFlags_Default, &result);

    if(!result)
    {
      RDCWARN("Couldn't validate shader blob");
      SAFE_RELEASE(validator);
      SAFE_RELEASE(library);
      SAFE_RELEASE(blob);
      return;
    }

    SAFE_RELEASE(blob);

    result->GetStatus(&hr);
    if(FAILED(hr))
    {
      rdcstr err;
      result->GetErrorBuffer(&blob);
      if(blob)
      {
        IDxcBlobEncoding *utf8 = NULL;
        library->GetBlobAsUtf8(blob, &utf8);

        if(utf8)
          err = (char *)utf8->GetBufferPointer();

        SAFE_RELEASE(utf8);
      }
      SAFE_RELEASE(blob);

      if(err.empty())
        RDCWARN("DXIL validation failed but couldn't get error string");
      else
        RDCWARN("DXIL validation failed: %s", err.c_str());
    }
    else
    {
      RDCDEBUG("Edited DXIL validated successfully");
    }

    SAFE_RELEASE(validator);
    SAFE_RELEASE(library);
    SAFE_RELEASE(result);
  }
#endif
}

Type *ProgramEditor::CreateNewType()
{
  m_Types.push_back(new(alloc) Type);
  return m_Types.back();
}

const AttributeSet *ProgramEditor::GetAttributeSet(Attribute desiredAttrs)
{
  for(const AttributeSet *attrs : m_AttributeSets)
    if(attrs && attrs->functionSlot && attrs->functionSlot->params == desiredAttrs)
      return attrs;

  m_AttributeGroups.push_back(alloc.alloc<AttributeGroup>());
  m_AttributeGroups.back()->slotIndex = AttributeGroup::FunctionSlot;
  m_AttributeGroups.back()->params = desiredAttrs;

  m_AttributeSets.push_back(alloc.alloc<AttributeSet>());
  m_AttributeSets.back()->functionSlot = m_AttributeGroups.back();
  m_AttributeSets.back()->orderedGroups = {m_AttributeGroups.size() - 1};

  return m_AttributeSets.back();
}

Type *ProgramEditor::CreateScalarType(Type::ScalarKind scalarType, uint32_t bitWidth)
{
  for(size_t i = 0; i < m_Types.size(); i++)
    if(m_Types[i]->scalarType == scalarType && m_Types[i]->bitWidth == bitWidth)
      return m_Types[i];

  Type *t = CreateNewType();
  t->type = Type::Scalar;
  t->scalarType = scalarType;
  t->bitWidth = bitWidth;
  return t;
}

Type *ProgramEditor::CreateNamedStructType(const rdcstr &name, rdcarray<const Type *> members)
{
  for(size_t i = 0; i < m_Types.size(); i++)
    if(m_Types[i]->name == name)
      return m_Types[i];

  if(members.empty())
    return NULL;

  Type *structType = CreateNewType();
  structType->type = Type::Struct;
  structType->name = name;
  structType->members = members;
  return structType;
}

DXIL::Type *ProgramEditor::CreateFunctionType(const Type *retType, rdcarray<const Type *> params)
{
  for(Type *type : m_Types)
    if(type->type == Type::Function && type->inner == retType && type->members == params)
      return type;

  Type *funcType = CreateNewType();
  funcType->type = Type::Function;
  funcType->inner = retType;
  funcType->members = params;
  return funcType;
}

DXIL::Type *ProgramEditor::CreatePointerType(const Type *inner, Type::PointerAddrSpace addrSpace)
{
  for(Type *type : m_Types)
    if(type->type == Type::Pointer && type->inner == inner && type->addrSpace == addrSpace)
      return type;

  Type *ptrType = CreateNewType();
  ptrType->type = Type::Pointer;
  ptrType->inner = inner;
  ptrType->addrSpace = addrSpace;
  return ptrType;
}

Function *ProgramEditor::GetFunctionByName(const rdcstr &name)
{
  for(size_t i = 0; i < m_Functions.size(); i++)
    if(m_Functions[i]->name == name)
      return m_Functions[i];

  return NULL;
}

Function *ProgramEditor::GetFunctionByPrefix(const rdcstr &name)
{
  for(size_t i = 0; i < m_Functions.size(); i++)
    if(m_Functions[i]->name.beginsWith(name))
      return m_Functions[i];

  return NULL;
}

Function *ProgramEditor::DeclareFunction(const rdcstr &name, const Type *retType,
                                         rdcarray<const Type *> params, Attribute desiredAttrs)
{
  Function *ret = GetFunctionByName(name);

  if(!ret)
  {
    const Type *funcType = CreateFunctionType(retType, params);

    Function functionDef;
    functionDef.name = name;
    functionDef.type = funcType;
    functionDef.external = true;
    functionDef.attrs = GetAttributeSet(desiredAttrs);

    ret = DeclareFunction(functionDef);
  }

  return ret;
}

Block *ProgramEditor::CreateBlock()
{
  if(m_LabelType == NULL)
  {
    Type *label = CreateNewType();
    label->type = Type::Label;
    m_LabelType = label;
  }
  return new(alloc) Block(m_LabelType);
}

Metadata *ProgramEditor::GetMetadataByName(const rdcstr &name)
{
  for(size_t i = 0; i < m_NamedMeta.size(); i++)
    if(m_NamedMeta[i]->name == name)
      return m_NamedMeta[i];

  return NULL;
}

Function *ProgramEditor::DeclareFunction(const Function &f)
{
  // only accept function declarations, not definitions
  if(!f.instructions.empty())
  {
    RDCERR("Only function declarations are allowed");
    return NULL;
  }

  m_Functions.push_back(new(alloc) Function(f));
  Function *ret = m_Functions.back();

  // functions need to be added to the symtab or dxc complains
  if(m_SortedSymtab)
  {
    // if the symtab was sorted, add in sorted order
    size_t idx = 0;
    for(; idx < m_ValueSymtabOrder.size(); idx++)
    {
      if(f.name < GetValueSymtabString(m_ValueSymtabOrder[idx]))
        break;
    }

    m_ValueSymtabOrder.insert(idx, ret);
  }
  else
  {
    // otherwise just append
    m_ValueSymtabOrder.push_back(ret);
  }

  return ret;
}

Metadata *ProgramEditor::CreateMetadata()
{
  return new(alloc) Metadata;
}

Metadata *ProgramEditor::CreateConstantMetadata(uint32_t val)
{
  Metadata *m = CreateMetadata();
  m->isConstant = true;
  m->type = m_Int32Type;
  m->value = CreateConstant(Constant(m_Int32Type, val));
  return m;
}

Metadata *ProgramEditor::CreateConstantMetadata(uint8_t val)
{
  Metadata *m = CreateMetadata();
  m->isConstant = true;
  m->type = m_Int8Type;
  m->value = CreateConstant(Constant(m_Int8Type, val));
  return m;
}

Metadata *ProgramEditor::CreateConstantMetadata(const rdcstr &str)
{
  Metadata *m = CreateMetadata();
  m->isConstant = true;
  m->isString = true;
  m->str = str;
  return m;
}

Metadata *ProgramEditor::CreateConstantMetadata(bool val)
{
  Metadata *m = CreateMetadata();
  m->isConstant = true;
  m->type = m_BoolType;
  m->value = CreateConstant(Constant(m_BoolType, val));
  return m;
}

Metadata *ProgramEditor::CreateConstantMetadata(Constant *val)
{
  Metadata *m = CreateMetadata();
  m->isConstant = true;
  m->type = val->type;
  m->value = val;
  return m;
}

NamedMetadata *ProgramEditor::CreateNamedMetadata(const rdcstr &name)
{
  for(NamedMetadata *m : m_NamedMeta)
    if(m->name == name)
      return m;

  m_NamedMeta.push_back(new(alloc) NamedMetadata);
  m_NamedMeta.back()->name = name;
  return m_NamedMeta.back();
}

Literal *ProgramEditor::CreateLiteral(uint64_t val)
{
  return new(alloc) Literal(val);
}

Constant *ProgramEditor::CreateConstant(const Constant &c)
{
  // for scalars, check for an existing constant
  if(c.type->type == Type::Scalar)
  {
    for(Constant *existing : m_Constants)
    {
      if(existing->type == c.type)
      {
        if(existing->isUndef() && c.isUndef())
          return existing;
        if(existing->isNULL() && c.isNULL())
          return existing;
        if(existing->isLiteral() && c.isLiteral() && existing->getU64() == c.getU64())
          return existing;
      }
    }
  }

  m_Constants.push_back(new(alloc) Constant(c));
  return m_Constants.back();
}

Constant *ProgramEditor::CreateConstant(const Type *type, const rdcarray<Value *> &members)
{
  Constant *ret = new(alloc) Constant;
  ret->type = type;
  ret->setCompound(alloc, members);
  return ret;
}

Constant *ProgramEditor::CreateConstantGEP(const Type *resultType,
                                           const rdcarray<Value *> &pointerAndIdxs)
{
  Constant *ret = new(alloc) Constant;
  ret->op = Operation::GetElementPtr;
  ret->type = resultType;
  ret->setCompound(alloc, pointerAndIdxs);
  return ret;
}

Constant *ProgramEditor::CreateUndef(const Type *t)
{
  Constant c;
  c.type = t;
  c.setUndef(true);
  return CreateConstant(c);
}

Constant *ProgramEditor::CreateNULL(const Type *t)
{
  Constant c;
  c.type = t;
  c.setNULL(true);
  return CreateConstant(c);
}

Instruction *ProgramEditor::CreateInstruction(Operation op)
{
  Instruction *ret = new(alloc) Instruction;
  ret->op = op;
  return ret;
}

Instruction *ProgramEditor::CreateInstruction(const Function *f)
{
  Instruction *ret = CreateInstruction(Operation::Call);
  ret->extra(alloc).funcCall = f;
  return ret;
}

Instruction *ProgramEditor::CreateInstruction(Operation op, const Type *retType,
                                              const rdcarray<Value *> &args)
{
  Instruction *ret = new(alloc) Instruction;
  ret->op = op;
  ret->type = retType;
  ret->args = args;
  return ret;
}

Instruction *ProgramEditor::CreateInstruction(const Function *f, DXOp op,
                                              const rdcarray<Value *> &args)
{
  Instruction *ret = CreateInstruction(f);
  ret->type = f->type->inner;
  ret->args = args;
  ret->args.insert(0, CreateConstant((uint32_t)op));
  return ret;
}

#define getAttribID(a) uint64_t(m_AttributeSets.indexOf((AttributeSet *)a))
#define getTypeID(t) uint64_t(t->id)
#define getMetaID(m) uint64_t(m->id)
#define getValueID(v) uint64_t(v->id)
#define getMetaIDOrNull(m) (m ? (getMetaID(m) + 1) : 0ULL)

bytebuf ProgramEditor::EncodeProgram()
{
  bytebuf ret;

  LLVMBC::BitcodeWriter writer(ret);

  LLVMBC::BitcodeWriter::Config cfg = {};

  LLVMOrderAccumulator accum;
  accum.processGlobals(this, false);

  const rdcarray<const Value *> &values = accum.values;
  const rdcarray<const Metadata *> &metadata = accum.metadata;

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    cfg.maxAlign = RDCMAX(m_GlobalVars[i]->align, cfg.maxAlign);
    RDCASSERT(m_GlobalVars[i]->type->type == Type::Pointer);
    uint32_t typeIndex = uint32_t(getTypeID(m_GlobalVars[i]->type->inner));
    cfg.maxGlobalType = RDCMAX(typeIndex, cfg.maxGlobalType);
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
    cfg.maxAlign = RDCMAX(m_Functions[i]->align, cfg.maxAlign);

  for(size_t i = 0; i < metadata.size(); i++)
  {
    if(metadata[i]->isString)
      cfg.hasMetaString = true;

    if(metadata[i]->debugLoc)
      cfg.hasDebugLoc = true;
  }

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    if(m_NamedMeta[i]->isString)
      cfg.hasMetaString = true;

    if(m_NamedMeta[i]->debugLoc)
      cfg.hasDebugLoc = true;
  }

  cfg.hasNamedMeta = !m_NamedMeta.empty();

  cfg.numTypes = accum.types.size();
  cfg.numSections = m_Sections.size();
  cfg.numGlobalValues = values.size();

  writer.ConfigureSizes(cfg);

  writer.BeginBlock(LLVMBC::KnownBlock::MODULE_BLOCK);

  writer.Record(LLVMBC::ModuleRecord::VERSION, 1U);

  {
    writer.ModuleBlockInfo();
  }

  if(!m_AttributeGroups.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::PARAMATTR_GROUP_BLOCK);

    rdcarray<uint64_t> vals;

    for(size_t i = 0; i < m_AttributeGroups.size(); i++)
    {
      if(m_AttributeGroups[i] && m_AttributeGroups[i]->slotIndex != AttributeGroup::InvalidSlot)
      {
        const AttributeGroup *group = m_AttributeGroups[i];

        vals.clear();
        vals.push_back(i);
        vals.push_back(group->slotIndex);

        // decompose params bitfield into bits
        if(group->params != Attribute::None)
        {
          uint64_t params = (uint64_t)group->params;
          for(uint64_t p = 0; p < 64; p++)
          {
            if((params & (1ULL << p)) != 0)
            {
              switch(Attribute(1ULL << p))
              {
                case Attribute::Alignment:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group->align);
                  break;
                }
                case Attribute::StackAlignment:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group->stackAlign);
                  break;
                }
                case Attribute::Dereferenceable:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group->derefBytes);
                  break;
                }
                case Attribute::DereferenceableOrNull:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group->derefOrNullBytes);
                  break;
                }
                default:
                {
                  // this attribute just exists or doesn't
                  vals.push_back(0);
                  vals.push_back(p);
                }
              }
            }
          }
        }

        if(!group->strs.empty())
        {
          for(const rdcpair<rdcstr, rdcstr> &strAttr : group->strs)
          {
            if(strAttr.second.empty())
              vals.push_back(3);
            else
              vals.push_back(4);

            // iterate including NULL terminator
            for(size_t c = 0; c < strAttr.first.size() + 1; c++)
              vals.push_back(uint64_t(strAttr.first[c]));

            for(size_t c = 0; !strAttr.second.empty() && c < strAttr.second.size() + 1; c++)
              vals.push_back(uint64_t(strAttr.second[c]));
          }
        }

        writer.Record(LLVMBC::ParamAttrGroupRecord::ENTRY, vals);
      }
    }

    writer.EndBlock();
  }

  if(!m_AttributeSets.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::PARAMATTR_BLOCK);

    for(size_t i = 0; i < m_AttributeSets.size(); i++)
      writer.Record(LLVMBC::ParamAttrRecord::ENTRY, m_AttributeSets[i]->orderedGroups);

    writer.EndBlock();
  }

  {
    writer.BeginBlock(LLVMBC::KnownBlock::TYPE_BLOCK);

    writer.Record(LLVMBC::TypeRecord::NUMENTRY, (uint32_t)accum.types.size());

    for(size_t i = 0; i < accum.types.size(); i++)
    {
      const Type &t = *accum.types[i];
      if(t.isVoid())
      {
        writer.Record(LLVMBC::TypeRecord::VOID);
      }
      else if(t.type == Type::Label)
      {
        writer.Record(LLVMBC::TypeRecord::LABEL);
      }
      else if(t.type == Type::Metadata)
      {
        writer.Record(LLVMBC::TypeRecord::METADATA);
      }
      else if(t.type == Type::Scalar && t.scalarType == Type::Float)
      {
        if(t.bitWidth == 16)
          writer.Record(LLVMBC::TypeRecord::HALF);
        else if(t.bitWidth == 32)
          writer.Record(LLVMBC::TypeRecord::FLOAT);
        else if(t.bitWidth == 64)
          writer.Record(LLVMBC::TypeRecord::DOUBLE);
      }
      else if(t.type == Type::Scalar && t.scalarType == Type::Int)
      {
        writer.Record(LLVMBC::TypeRecord::INTEGER, t.bitWidth);
      }
      else if(t.type == Type::Vector)
      {
        writer.Record(LLVMBC::TypeRecord::VECTOR, {t.elemCount, getTypeID(t.inner)});
      }
      else if(t.type == Type::Array)
      {
        writer.Record(LLVMBC::TypeRecord::ARRAY, {t.elemCount, getTypeID(t.inner)});
      }
      else if(t.type == Type::Pointer)
      {
        writer.Record(LLVMBC::TypeRecord::POINTER, {getTypeID(t.inner), (uint64_t)t.addrSpace});
      }
      else if(t.type == Type::Struct)
      {
        if(t.opaque)
        {
          writer.Record(LLVMBC::TypeRecord::OPAQUE);
        }
        else
        {
          LLVMBC::TypeRecord type = LLVMBC::TypeRecord::STRUCT_ANON;

          if(!t.name.empty())
          {
            writer.Record(LLVMBC::TypeRecord::STRUCT_NAME, t.name);
            type = LLVMBC::TypeRecord::STRUCT_NAMED;
          }

          rdcarray<uint64_t> vals;

          vals.push_back(t.packedStruct ? 1 : 0);

          for(const Type *member : t.members)
            vals.push_back(getTypeID(member));

          writer.Record(type, vals);
        }
      }
      else if(t.type == Type::Function)
      {
        rdcarray<uint64_t> vals;

        vals.push_back(t.vararg ? 1 : 0);

        vals.push_back(getTypeID(t.inner));

        for(const Type *member : t.members)
          vals.push_back(getTypeID(member));

        writer.Record(LLVMBC::TypeRecord::FUNCTION, vals);
      }
    }

    writer.EndBlock();
  }

  for(size_t i = 0; i < m_Comdats.size(); i++)
  {
    rdcarray<uint64_t> vals;
    vals.push_back(m_Comdats[i].first);
    for(char c : m_Comdats[i].second)
      vals.push_back(c);
    writer.Record(LLVMBC::ModuleRecord::COMDAT, vals);
  }

  if(!m_Triple.empty())
    writer.Record(LLVMBC::ModuleRecord::TRIPLE, m_Triple);

  if(!m_Datalayout.empty())
    writer.Record(LLVMBC::ModuleRecord::DATALAYOUT, m_Datalayout);

  // inline asm would go here

  // write the sections
  for(size_t i = 0; i < m_Sections.size(); i++)
    writer.Record(LLVMBC::ModuleRecord::SECTIONNAME, m_Sections[i]);

  if(!m_GlobalVars.empty())
    writer.EmitGlobalVarAbbrev();

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = *m_GlobalVars[i];

    // global vars write the value type, not the pointer
    uint64_t typeIndex = getTypeID(g.type->inner);

    RDCASSERT((size_t)typeIndex < accum.types.size());

    uint64_t linkageValue = 0;

    switch(g.flags & GlobalFlags::LinkageMask)
    {
      case GlobalFlags::ExternalLinkage: linkageValue = 0; break;
      case GlobalFlags::WeakAnyLinkage: linkageValue = 16; break;
      case GlobalFlags::AppendingLinkage: linkageValue = 2; break;
      case GlobalFlags::InternalLinkage: linkageValue = 3; break;
      case GlobalFlags::LinkOnceAnyLinkage: linkageValue = 18; break;
      case GlobalFlags::ExternalWeakLinkage: linkageValue = 7; break;
      case GlobalFlags::CommonLinkage: linkageValue = 8; break;
      case GlobalFlags::PrivateLinkage: linkageValue = 9; break;
      case GlobalFlags::WeakODRLinkage: linkageValue = 17; break;
      case GlobalFlags::LinkOnceODRLinkage: linkageValue = 19; break;
      case GlobalFlags::AvailableExternallyLinkage: linkageValue = 12; break;
      default: break;
    }

    uint64_t unnamedAddr = 0;

    if(g.flags & GlobalFlags::GlobalUnnamedAddr)
      unnamedAddr = 1;
    else if(g.flags & GlobalFlags::LocalUnnamedAddr)
      unnamedAddr = 2;

    writer.Record(LLVMBC::ModuleRecord::GLOBALVAR,
                  {
                      typeIndex,
                      uint64_t(((g.flags & GlobalFlags::IsConst) ? 1 : 0) | 0x2 |
                               ((uint32_t)g.type->addrSpace << 2)),
                      g.initialiser ? getValueID(g.initialiser) + 1 : 0,
                      linkageValue,
                      Log2Floor((uint32_t)g.align) + 1,
                      uint64_t(g.section + 1),
                      // visibility
                      0U,
                      // TLS mode
                      0U,
                      // unnamed addr
                      unnamedAddr,
                      (g.flags & GlobalFlags::ExternallyInitialised) ? 1U : 0U,
                      // DLL storage class
                      0U,
                      // comdat
                      0U,
                  });
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &f = *m_Functions[i];
    uint64_t typeIndex = getTypeID(f.type);

    RDCASSERT((size_t)typeIndex < accum.types.size());

    writer.Record(LLVMBC::ModuleRecord::FUNCTION,
                  {
                      typeIndex,
                      // calling convention
                      0U,
                      // external/declaration
                      f.external ? 1U : 0U,
                      // linkage
                      f.internalLinkage ? 3U : 0U,
                      // attributes
                      uint64_t(f.attrs ? 1U + getAttribID(f.attrs) : 0U),
                      // alignment
                      f.align,
                      // section
                      0U,
                      // visibility
                      0U,
                      // gc
                      0U,
                      // unnamed_addr
                      0U,
                      // prologuedata
                      0U,
                      // dllstorageclass
                      0U,
                      // comdat
                      uint64_t(f.comdatIdx != ~0U ? 1U + f.comdatIdx : 0U),
                      // prefixdata
                      0U,
                      // personality
                      0U,
                  });
  }

  for(size_t i = 0; i < m_Aliases.size(); i++)
  {
    const Alias &a = *m_Aliases[i];
    uint64_t typeIndex = getTypeID(a.type);

    writer.Record(LLVMBC::ModuleRecord::ALIAS, {
                                                   typeIndex,
                                                   getValueID(a.val),
                                                   // linkage
                                                   0U,
                                                   // visibility
                                                   0U,
                                               });
  }

  // the symbols for constants start after the global variables and functions which we just
  // outputted
  if(accum.numConsts)
  {
    writer.BeginBlock(LLVMBC::KnownBlock::CONSTANTS_BLOCK);

    EncodeConstants(writer, values, accum.firstConst, accum.numConsts);

    writer.EndBlock();
  }

  if(!metadata.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::METADATA_BLOCK);

    writer.EmitMetaDataAbbrev();

    EncodeMetadata(writer, metadata);

    rdcarray<uint64_t> vals;

    for(size_t i = 0; i < m_NamedMeta.size(); i++)
    {
      writer.Record(LLVMBC::MetaDataRecord::NAME, m_NamedMeta[i]->name);

      vals.clear();
      for(size_t m = 0; m < m_NamedMeta[i]->children.size(); m++)
        vals.push_back(getMetaID(m_NamedMeta[i]->children[m]));

      writer.Record(LLVMBC::MetaDataRecord::NAMED_NODE, vals);
    }

    writer.EndBlock();
  }

  if(!m_Kinds.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::METADATA_BLOCK);

    rdcarray<uint64_t> vals;

    for(size_t i = 0; i < m_Kinds.size(); i++)
    {
      if(m_Kinds[i].empty())
        continue;

      vals.clear();
      vals.push_back(i);
      for(char c : m_Kinds[i])
        vals.push_back(c);

      writer.Record(LLVMBC::MetaDataRecord::KIND, vals);
    }

    writer.EndBlock();
  }

  if(!m_ValueSymtabOrder.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::VALUE_SYMTAB_BLOCK);

    for(Value *v : m_ValueSymtabOrder)
    {
      const rdcstr *str = NULL;
      switch(v->kind())
      {
        case ValueKind::GlobalVar: str = &cast<GlobalVar>(v)->name; break;
        case ValueKind::Function: str = &cast<Function>(v)->name; break;
        case ValueKind::Alias: str = &cast<Alias>(v)->name; break;
        default: break;
      }

      if(str)
        writer.RecordSymTabEntry(getValueID(v), *str);
    }

    writer.EndBlock();
  }

#define encodeRelativeValueID(v)                                 \
  {                                                              \
    uint64_t valID = getValueID(v);                              \
    if(valID <= zeroIdxValueId)                                  \
    {                                                            \
      vals.push_back(zeroIdxValueId - valID);                    \
    }                                                            \
    else                                                         \
    {                                                            \
      forwardRefs = true;                                        \
      /* signed integer two's complement for negative    */      \
      /* values referencing forward from the instruction */      \
      vals.push_back(0x100000000ULL - (valID - zeroIdxValueId)); \
      vals.push_back(getTypeID(v->type));                        \
    }                                                            \
  }

// some cases don't encode the type even for forward refs, if it's implicit (e.g. second parameter
// in a binop). This also doesn't count as a forward ref for the case of breaking the abbrev use
#define encodeRelativeValueIDTypeless(v)                         \
  {                                                              \
    uint64_t valID = getValueID(v);                              \
    if(valID <= zeroIdxValueId)                                  \
    {                                                            \
      vals.push_back(zeroIdxValueId - valID);                    \
    }                                                            \
    else                                                         \
    {                                                            \
      vals.push_back(0x100000000ULL - (valID - zeroIdxValueId)); \
    }                                                            \
  }

  for(Function *f : m_Functions)
  {
    if(f->external)
      continue;

    writer.BeginBlock(LLVMBC::KnownBlock::FUNCTION_BLOCK);

    writer.Record(LLVMBC::FunctionRecord::DECLAREBLOCKS, f->blocks.size());

    accum.processFunction(f);

    if(accum.numFuncConsts)
    {
      writer.BeginBlock(LLVMBC::KnownBlock::CONSTANTS_BLOCK);

      EncodeConstants(writer, values, accum.firstFuncConst, accum.numFuncConsts);

      writer.EndBlock();
    }

    uint32_t debugLoc = ~0U;

    bool forwardRefs = false;
    rdcarray<uint64_t> vals;

    bool needMetaAttach = !f->attachedMeta.empty();

    uint32_t lastValidInstId = uint32_t(accum.firstFuncConst + accum.numFuncConsts) - 1;

    for(const Instruction *inst : f->instructions)
    {
      forwardRefs = false;
      vals.clear();

      if(inst->id != Value::NoID)
        lastValidInstId = inst->id;

      // a reference to this value ID is '0'. Usually the current instruction, 1 is then the
      // previous, etc
      uint32_t zeroIdxValueId = inst->id;
      // except if the current instruction isn't a value. Then '0' is impossible, 1 still refers to
      // the previous. In order to have a value ID to construct relative references, we pretend we
      // are on the next value
      if(zeroIdxValueId == Value::NoID)
        zeroIdxValueId = lastValidInstId + 1;

      needMetaAttach |= !inst->getAttachedMeta().empty();

      switch(inst->op)
      {
        case Operation::NoOp: RDCERR("Unexpected no-op encoding"); continue;
        case Operation::Call:
        {
          vals.push_back(inst->getParamAttrs() ? getAttribID(inst->getParamAttrs()) + 1 : 0);
          // always emit func type
          uint64_t flags = 1 << 15;
          if(inst->opFlags() != InstructionFlags::NoFlags)
            flags |= 1 << 17;
          vals.push_back(flags);
          if(inst->opFlags() != InstructionFlags::NoFlags)
            vals.push_back((uint64_t)inst->opFlags());
          vals.push_back(getTypeID(inst->getFuncCall()->type));
          encodeRelativeValueID(inst->getFuncCall());
          for(size_t a = 0; a < inst->args.size(); a++)
          {
            if(inst->args[a]->kind() == ValueKind::Metadata)
            {
              vals.push_back(getMetaID(cast<Metadata>(inst->args[a])));
            }
            else
            {
              encodeRelativeValueIDTypeless(inst->args[a]);
            }
          }
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CALL, vals, forwardRefs);
          break;
        }
        case Operation::Trunc:
        case Operation::ZExt:
        case Operation::SExt:
        case Operation::FToU:
        case Operation::FToS:
        case Operation::UToF:
        case Operation::SToF:
        case Operation::FPTrunc:
        case Operation::FPExt:
        case Operation::PtrToI:
        case Operation::IToPtr:
        case Operation::Bitcast:
        case Operation::AddrSpaceCast:
        {
          encodeRelativeValueID(inst->args[0]);
          vals.push_back(getTypeID(inst->type));
          vals.push_back(EncodeCast(inst->op));

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CAST, vals, forwardRefs);
          break;
        }
        case Operation::ExtractVal:
        {
          encodeRelativeValueID(inst->args[0]);
          for(size_t i = 1; i < inst->args.size(); i++)
            vals.push_back(cast<Literal>(inst->args[i])->literal);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_EXTRACTVAL, vals, forwardRefs);
          break;
        }
        case Operation::Ret:
        {
          if(!inst->args.empty())
          {
            encodeRelativeValueID(inst->args[0]);
          }
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_RET, vals, forwardRefs);
          break;
        }
        case Operation::FAdd:
        case Operation::FSub:
        case Operation::FMul:
        case Operation::FDiv:
        case Operation::FRem:
        case Operation::Add:
        case Operation::Sub:
        case Operation::Mul:
        case Operation::UDiv:
        case Operation::SDiv:
        case Operation::URem:
        case Operation::SRem:
        case Operation::ShiftLeft:
        case Operation::LogicalShiftRight:
        case Operation::ArithShiftRight:
        case Operation::And:
        case Operation::Or:
        case Operation::Xor:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueIDTypeless(inst->args[1]);

          const Type *t = inst->args[0]->type;

          const bool isFloatOp = (t->scalarType == Type::Float);

          uint64_t opcode = 0;
          switch(inst->op)
          {
            case Operation::FAdd:
            case Operation::Add: opcode = 0; break;
            case Operation::FSub:
            case Operation::Sub: opcode = 1; break;
            case Operation::FMul:
            case Operation::Mul: opcode = 2; break;
            case Operation::UDiv: opcode = 3; break;
            case Operation::FDiv:
            case Operation::SDiv: opcode = 4; break;
            case Operation::URem: opcode = 5; break;
            case Operation::FRem:
            case Operation::SRem: opcode = 6; break;
            case Operation::ShiftLeft: opcode = 7; break;
            case Operation::LogicalShiftRight: opcode = 8; break;
            case Operation::ArithShiftRight: opcode = 9; break;
            case Operation::And: opcode = 10; break;
            case Operation::Or: opcode = 11; break;
            case Operation::Xor: opcode = 12; break;
            default: break;
          }
          vals.push_back(opcode);

          if(inst->opFlags() != InstructionFlags::NoFlags)
          {
            uint64_t flags = 0;
            if(inst->op == Operation::Add || inst->op == Operation::Sub ||
               inst->op == Operation::Mul || inst->op == Operation::ShiftLeft)
            {
              if(inst->opFlags() & InstructionFlags::NoSignedWrap)
                flags |= 0x2;
              if(inst->opFlags() & InstructionFlags::NoUnsignedWrap)
                flags |= 0x1;
              vals.push_back(flags);
            }
            else if(inst->op == Operation::SDiv || inst->op == Operation::UDiv ||
                    inst->op == Operation::LogicalShiftRight ||
                    inst->op == Operation::ArithShiftRight)
            {
              if(inst->opFlags() & InstructionFlags::Exact)
                flags |= 0x1;
              vals.push_back(flags);
            }
            else if(isFloatOp)
            {
              // fast math flags overlap
              vals.push_back(uint64_t(inst->opFlags()));
            }
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_BINOP, vals, forwardRefs);
          break;
        }
        case Operation::Unreachable:
        {
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_UNREACHABLE, {}, false);
          break;
        }
        case Operation::Alloca:
        {
          vals.push_back(getTypeID(inst->type->inner));
          vals.push_back(getTypeID(inst->args[0]->type));
          vals.push_back(getValueID(inst->args[0]));
          uint64_t alignAndFlags = inst->align;
          // DXC always sets this bit, as the type is a pointer
          alignAndFlags |= 1U << 6;
          if(inst->opFlags() & InstructionFlags::ArgumentAlloca)
            alignAndFlags |= 1U << 5;
          vals.push_back(alignAndFlags);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_ALLOCA, vals, forwardRefs);
          break;
        }
        case Operation::GetElementPtr:
        {
          vals.push_back((inst->opFlags() & InstructionFlags::InBounds) ? 1U : 0U);
          vals.push_back(getTypeID(inst->args[0]->type->inner));

          for(size_t i = 0; i < inst->args.size(); i++)
          {
            encodeRelativeValueID(inst->args[i]);
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_GEP, vals, forwardRefs);
          break;
        }
        case Operation::Load:
        {
          encodeRelativeValueID(inst->args[0]);
          vals.push_back(getTypeID(inst->type));
          vals.push_back(inst->align);
          vals.push_back((inst->opFlags() & InstructionFlags::Volatile) ? 1U : 0U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_LOAD, vals, forwardRefs);
          break;
        }
        case Operation::Store:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueID(inst->args[1]);
          vals.push_back(inst->align);
          vals.push_back((inst->opFlags() & InstructionFlags::Volatile) ? 1U : 0U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_STORE, vals, forwardRefs);
          break;
        }
        case Operation::FOrdFalse:
        case Operation::FOrdEqual:
        case Operation::FOrdGreater:
        case Operation::FOrdGreaterEqual:
        case Operation::FOrdLess:
        case Operation::FOrdLessEqual:
        case Operation::FOrdNotEqual:
        case Operation::FOrd:
        case Operation::FUnord:
        case Operation::FUnordEqual:
        case Operation::FUnordGreater:
        case Operation::FUnordGreaterEqual:
        case Operation::FUnordLess:
        case Operation::FUnordLessEqual:
        case Operation::FUnordNotEqual:
        case Operation::FOrdTrue:
        case Operation::IEqual:
        case Operation::INotEqual:
        case Operation::UGreater:
        case Operation::UGreaterEqual:
        case Operation::ULess:
        case Operation::ULessEqual:
        case Operation::SGreater:
        case Operation::SGreaterEqual:
        case Operation::SLess:
        case Operation::SLessEqual:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueIDTypeless(inst->args[1]);

          uint64_t opcode = 0;
          switch(inst->op)
          {
            case Operation::FOrdFalse: opcode = 0; break;
            case Operation::FOrdEqual: opcode = 1; break;
            case Operation::FOrdGreater: opcode = 2; break;
            case Operation::FOrdGreaterEqual: opcode = 3; break;
            case Operation::FOrdLess: opcode = 4; break;
            case Operation::FOrdLessEqual: opcode = 5; break;
            case Operation::FOrdNotEqual: opcode = 6; break;
            case Operation::FOrd: opcode = 7; break;
            case Operation::FUnord: opcode = 8; break;
            case Operation::FUnordEqual: opcode = 9; break;
            case Operation::FUnordGreater: opcode = 10; break;
            case Operation::FUnordGreaterEqual: opcode = 11; break;
            case Operation::FUnordLess: opcode = 12; break;
            case Operation::FUnordLessEqual: opcode = 13; break;
            case Operation::FUnordNotEqual: opcode = 14; break;
            case Operation::FOrdTrue: opcode = 15; break;

            case Operation::IEqual: opcode = 32; break;
            case Operation::INotEqual: opcode = 33; break;
            case Operation::UGreater: opcode = 34; break;
            case Operation::UGreaterEqual: opcode = 35; break;
            case Operation::ULess: opcode = 36; break;
            case Operation::ULessEqual: opcode = 37; break;
            case Operation::SGreater: opcode = 38; break;
            case Operation::SGreaterEqual: opcode = 39; break;
            case Operation::SLess: opcode = 40; break;
            case Operation::SLessEqual: opcode = 41; break;

            default: break;
          }

          vals.push_back(opcode);

          if(inst->opFlags() != InstructionFlags::NoFlags)
            vals.push_back((uint64_t)inst->opFlags());

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CMP2, vals, forwardRefs);
          break;
        }
        case Operation::Select:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueIDTypeless(inst->args[1]);
          encodeRelativeValueID(inst->args[2]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_VSELECT, vals, forwardRefs);
          break;
        }
        case Operation::ExtractElement:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueID(inst->args[1]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_EXTRACTELT, vals, forwardRefs);
          break;
        }
        case Operation::InsertElement:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueIDTypeless(inst->args[1]);
          encodeRelativeValueID(inst->args[2]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_INSERTELT, vals, forwardRefs);
          break;
        }
        case Operation::ShuffleVector:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueIDTypeless(inst->args[1]);
          encodeRelativeValueID(inst->args[2]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_SHUFFLEVEC, vals, forwardRefs);
          break;
        }
        case Operation::InsertValue:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueID(inst->args[1]);
          for(size_t i = 2; i < inst->args.size(); i++)
            vals.push_back(cast<Literal>(inst->args[i])->literal);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_INSERTVAL, vals, forwardRefs);
          break;
        }
        case Operation::Branch:
        {
          vals.push_back(inst->args[0]->id);

          if(inst->args.size() > 1)
          {
            vals.push_back(inst->args[1]->id);
            encodeRelativeValueIDTypeless(inst->args[2]);
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_BR, vals, forwardRefs);
          break;
        }
        case Operation::Phi:
        {
          vals.push_back(getTypeID(inst->type));

          for(size_t i = 0; i < inst->args.size(); i += 2)
          {
            uint64_t valID = getValueID(inst->args[i]);
            int64_t valRef = int64_t(inst->id) - int64_t(valID);

            vals.push_back(LLVMBC::BitWriter::svbr(valRef));
            vals.push_back(inst->args[i + 1]->id);
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_PHI, vals, forwardRefs);
          break;
        }
        case Operation::Switch:
        {
          vals.push_back(getTypeID(inst->args[0]->type));
          encodeRelativeValueIDTypeless(inst->args[0]);

          vals.push_back(inst->args[1]->id);

          for(size_t i = 2; i < inst->args.size(); i += 2)
          {
            vals.push_back(getValueID(inst->args[i]));
            vals.push_back(inst->args[i + 1]->id);
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_SWITCH, vals, forwardRefs);
          break;
        }
        case Operation::Fence:
        {
          vals.push_back(
              ((uint64_t)inst->opFlags() & (uint64_t)InstructionFlags::SuccessOrderMask) >> 12U);
          vals.push_back((inst->opFlags() & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_FENCE, vals, forwardRefs);
          break;
        }
        case Operation::CompareExchange:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueID(inst->args[1]);
          encodeRelativeValueIDTypeless(inst->args[2]);
          vals.push_back((inst->opFlags() & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(
              ((uint64_t)inst->opFlags() & (uint64_t)InstructionFlags::SuccessOrderMask) >> 12U);
          vals.push_back((inst->opFlags() & InstructionFlags::SingleThread) ? 0U : 1U);
          vals.push_back(
              ((uint64_t)inst->opFlags() & (uint64_t)InstructionFlags::FailureOrderMask) >> 15U);
          vals.push_back((inst->opFlags() & InstructionFlags::Weak) ? 1U : 0U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CMPXCHG, vals, forwardRefs);
          break;
        }
        case Operation::LoadAtomic:
        {
          encodeRelativeValueID(inst->args[0]);
          vals.push_back(getTypeID(inst->type));
          vals.push_back(inst->align);
          vals.push_back((inst->opFlags() & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(
              ((uint64_t)inst->opFlags() & (uint64_t)InstructionFlags::SuccessOrderMask) >> 12U);
          vals.push_back((inst->opFlags() & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_LOADATOMIC, vals, forwardRefs);
          break;
        }
        case Operation::StoreAtomic:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueID(inst->args[1]);
          vals.push_back(inst->align);
          vals.push_back((inst->opFlags() & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(
              ((uint64_t)inst->opFlags() & (uint64_t)InstructionFlags::SuccessOrderMask) >> 12U);
          vals.push_back((inst->opFlags() & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_STOREATOMIC, vals, forwardRefs);
          break;
        }
        case Operation::AtomicExchange:
        case Operation::AtomicAdd:
        case Operation::AtomicSub:
        case Operation::AtomicAnd:
        case Operation::AtomicNand:
        case Operation::AtomicOr:
        case Operation::AtomicXor:
        case Operation::AtomicMax:
        case Operation::AtomicMin:
        case Operation::AtomicUMax:
        case Operation::AtomicUMin:
        {
          encodeRelativeValueID(inst->args[0]);
          encodeRelativeValueIDTypeless(inst->args[1]);

          uint64_t opcode = 0;
          switch(inst->op)
          {
            case Operation::AtomicExchange: opcode = 0; break;
            case Operation::AtomicAdd: opcode = 1; break;
            case Operation::AtomicSub: opcode = 2; break;
            case Operation::AtomicAnd: opcode = 3; break;
            case Operation::AtomicNand: opcode = 4; break;
            case Operation::AtomicOr: opcode = 5; break;
            case Operation::AtomicXor: opcode = 6; break;
            case Operation::AtomicMax: opcode = 7; break;
            case Operation::AtomicMin: opcode = 8; break;
            case Operation::AtomicUMax: opcode = 9; break;
            case Operation::AtomicUMin: opcode = 10; break;

            default: break;
          }

          vals.push_back(opcode);

          vals.push_back((inst->opFlags() & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(
              ((uint64_t)inst->opFlags() & (uint64_t)InstructionFlags::SuccessOrderMask) >> 12U);
          vals.push_back((inst->opFlags() & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_ATOMICRMW, vals, forwardRefs);
          break;
        }
      }

      // no debug location? omit
      if(inst->debugLoc == ~0U)
        continue;

      // same as last time? emit 'again' record
      if(inst->debugLoc == debugLoc)
        writer.Record(LLVMBC::FunctionRecord::DEBUG_LOC_AGAIN);

      // new debug location
      const DebugLocation &loc = m_DebugLocations[inst->debugLoc];

      writer.Record(LLVMBC::FunctionRecord::DEBUG_LOC, {
                                                           loc.line,
                                                           loc.col,
                                                           getMetaIDOrNull(loc.scope),
                                                           getMetaIDOrNull(loc.inlinedAt),
                                                       });

      debugLoc = inst->debugLoc;
    }

    if(!f->valueSymtabOrder.empty())
    {
      writer.BeginBlock(LLVMBC::KnownBlock::VALUE_SYMTAB_BLOCK);

      for(Value *v : f->valueSymtabOrder)
      {
        bool found = true;
        rdcstr str;
        switch(v->kind())
        {
          case ValueKind::Instruction: str = cast<Instruction>(v)->getName(); break;
          case ValueKind::Constant: str = cast<Constant>(v)->str; break;
          case ValueKind::BasicBlock: str = cast<Block>(v)->name; break;
          default: found = false; break;
        }

        if(found)
        {
          if(v->kind() == ValueKind::BasicBlock)
            writer.RecordSymTabEntry(v->id, str, true);
          else
            writer.RecordSymTabEntry(getValueID(v), str);
        }
      }

      writer.EndBlock();
    }

    if(needMetaAttach)
    {
      writer.BeginBlock(LLVMBC::KnownBlock::METADATA_ATTACHMENT);

      vals.clear();

      for(const rdcpair<uint64_t, Metadata *> &m : f->attachedMeta)
      {
        vals.push_back(m.first);
        vals.push_back(getMetaID(m.second));
      }

      if(!vals.empty())
        writer.Record(LLVMBC::MetaDataRecord::ATTACHMENT, vals);

      for(size_t i = 0; i < f->instructions.size(); i++)
      {
        if(f->instructions[i]->getAttachedMeta().empty())
          continue;

        vals.clear();

        vals.push_back(uint64_t(i));

        for(const rdcpair<uint64_t, Metadata *> &m : f->instructions[i]->getAttachedMeta())
        {
          vals.push_back(m.first);
          vals.push_back(getMetaID(m.second));
        }

        writer.Record(LLVMBC::MetaDataRecord::ATTACHMENT, vals);
      }

      writer.EndBlock();
    }

    if(!f->uselist.empty())
    {
      writer.BeginBlock(LLVMBC::KnownBlock::USELIST_BLOCK);

      for(const UselistEntry &u : f->uselist)
      {
        vals = u.shuffle;
        vals.push_back(getValueID(u.value));

        writer.Record(u.block ? LLVMBC::UselistRecord::BB : LLVMBC::UselistRecord::DEFAULT, vals);
      }

      writer.EndBlock();
    }

    writer.EndBlock();

    accum.exitFunction();
  }

  writer.EndBlock();

  ProgramHeader header;

  header.ProgramVersion = ((m_Major & 0xf) << 4) | (m_Minor & 0xf);
  header.ProgramType = (uint16_t)m_Type;
  header.DxilMagic = DXBC::FOURCC_DXIL;
  header.DxilVersion = m_DXILVersion;
  header.BitcodeOffset = sizeof(ProgramHeader) - offsetof(ProgramHeader, DxilMagic);
  header.BitcodeSize = (uint32_t)ret.size();
  header.SizeInUint32 = (uint32_t)AlignUp4(ret.size() + sizeof(ProgramHeader)) / sizeof(uint32_t);

  ret.insert(0, (const byte *)&header, sizeof(header));

  ret.resize(AlignUp4(ret.size()));

  return ret;
}

struct ResourceBind0
{
  DXILResourceType type;
  uint32_t space;
  uint32_t regBase;
  uint32_t regEnd;
};

struct ResourceBind1 : ResourceBind0
{
  ResourceKind kind;
  uint32_t flags;
};

// this function should be expanded in future, maybe to automatically re-write the PSV0 from the
// DXIL data in ~ProgramEditor()
void ProgramEditor::RegisterUAV(DXILResourceType type, uint32_t space, uint32_t regBase,
                                uint32_t regEnd, ResourceKind kind)
{
  size_t sz = 0;
  const byte *psv0 = DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_PSV0, sz);

  ResourceBind1 bind = {};
  bind.type = type;
  bind.space = space;
  bind.regBase = regBase;
  bind.regEnd = regEnd;
  bind.kind = kind;

  if(psv0)
  {
    bytebuf psv0blob(psv0, sz);

    byte *begin = psv0blob.data();
    byte *end = begin + sz;

    byte *cur = begin;

    uint32_t *headerSize = (uint32_t *)cur;
    cur += sizeof(uint32_t);
    if(cur >= end)
      return;

    // don't need to patch the header
    cur += *headerSize;
    if(cur >= end)
      return;

    uint32_t *numResources = (uint32_t *)cur;
    cur += sizeof(uint32_t);
    if(cur >= end)
      return;

    if(*numResources > 0)
    {
      uint32_t *resourceBindSize = (uint32_t *)cur;
      cur += sizeof(uint32_t);
      if(cur >= end)
        return;

      // fortunately UAVs are the last entry so we don't need to walk the list to insert in the
      // right place, we can just add it at the end
      cur += (*resourceBindSize) * (*numResources);
      if(cur >= end)
        return;

      // add an extra resource
      (*numResources)++;

      if(*resourceBindSize == sizeof(ResourceBind1) || *resourceBindSize == sizeof(ResourceBind0))
      {
        psv0blob.insert(cur - begin, (byte *)&bind, *resourceBindSize);
      }
      else
      {
        RDCERR("Unexpected resource bind size %u", *resourceBindSize);
        return;
      }
    }
    else
    {
      // from definitions in dxc
      const uint32_t headerSizeVer0 = 6 * sizeof(uint32_t);
      const uint32_t headerSizeVer1 = headerSizeVer0 + sizeof(uint16_t) + 10 * sizeof(uint8_t);
      const uint32_t headerSizeVer2 = headerSizeVer1 + 3 * sizeof(uint32_t);

      // If there is no resource in the chunk we also need to insert the size of a resource bind
      *numResources = 1;
      size_t insertOffset = cur - begin;
      uint32_t resourceBindSize =
          *headerSize == headerSizeVer2 ? sizeof(ResourceBind1) : sizeof(ResourceBind0);
      psv0blob.insert(insertOffset, (byte *)&resourceBindSize, sizeof(resourceBindSize));
      psv0blob.insert(insertOffset + sizeof(resourceBindSize), (byte *)&bind, resourceBindSize);
    }

    DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, DXBC::FOURCC_PSV0, psv0blob);
  }

  // patch SFI0 here for non-CS non-PS shaders
  if(m_Type != DXBC::ShaderType::Compute && m_Type != DXBC::ShaderType::Pixel)
  {
    PatchGlobalShaderFlags(
        [](DXBC::GlobalShaderFlags &flags) { flags |= DXBC::GlobalShaderFlags::UAVsEveryStage; });
  }

  // strip the root signature, we shouldn't need it and it may no longer match and fail validation
  DXBC::DXBCContainer::StripChunk(m_OutBlob, DXBC::FOURCC_RTS0);
}

void ProgramEditor::SetNumThreads(uint32_t dim[3])
{
  size_t sz = 0;
  const byte *psv0 = DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_PSV0, sz);

  if(psv0)
  {
    bytebuf psv0blob(psv0, sz);

    byte *begin = psv0blob.data();
    byte *end = begin + sz;

    byte *cur = begin;

    uint32_t *headerSize = (uint32_t *)cur;
    cur += sizeof(uint32_t);
    if(cur >= end)
      return;

    // from definitions in dxc
    const uint32_t headerSizeVer0 = 6 * sizeof(uint32_t);
    const uint32_t headerSizeVer1 = headerSizeVer0 + sizeof(uint16_t) + 10 * sizeof(uint8_t);
    const uint32_t headerSizeVer2 = headerSizeVer1 + 3 * sizeof(uint32_t);

    if(*headerSize >= headerSizeVer2)
    {
      cur += headerSizeVer0;
      cur += headerSizeVer1;
      memcpy(cur, dim, sizeof(uint32_t) * 3);
    }

    DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, DXBC::FOURCC_PSV0, psv0blob);
  }
}

void ProgramEditor::SetASPayloadSize(uint32_t payloadSize)
{
  size_t sz = 0;
  const byte *psv0 = DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_PSV0, sz);

  if(psv0)
  {
    bytebuf psv0blob(psv0, sz);

    byte *begin = psv0blob.data();
    byte *end = begin + sz;

    byte *cur = begin;

    cur += sizeof(uint32_t);
    if(cur >= end)
      return;

    // the AS info with the payload size is immediately at the start of the header
    memcpy(cur, &payloadSize, sizeof(uint32_t));

    DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, DXBC::FOURCC_PSV0, psv0blob);
  }
}

void ProgramEditor::SetMSPayloadSize(uint32_t payloadSize)
{
  size_t sz = 0;
  const byte *psv0 = DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_PSV0, sz);

  if(psv0)
  {
    bytebuf psv0blob(psv0, sz);

    byte *begin = psv0blob.data();
    byte *end = begin + sz;

    byte *cur = begin;

    cur += sizeof(uint32_t);
    if(cur >= end)
      return;

    // the MS info is immediately at the start of the header
    // the first two uint32s are groupshared related, then comes the payload size
    memcpy(cur + sizeof(uint32_t) * 2, &payloadSize, sizeof(uint32_t));

    DXBC::DXBCContainer::ReplaceChunk(m_OutBlob, DXBC::FOURCC_PSV0, psv0blob);
  }
}

void ProgramEditor::PatchGlobalShaderFlags(std::function<void(DXBC::GlobalShaderFlags &)> patcher)
{
  size_t sz = 0;
  // cheekily cast away const since this returns the blob in-place
  DXBC::GlobalShaderFlags *flags =
      (DXBC::GlobalShaderFlags *)DXBC::DXBCContainer::FindChunk(m_OutBlob, DXBC::FOURCC_SFI0, sz);

  // this *should* always be present, so we can just add our flag
  if(flags)
    patcher(*flags);
  else
    RDCWARN("Feature flags chunk not present");
}

void ProgramEditor::EncodeConstants(LLVMBC::BitcodeWriter &writer,
                                    const rdcarray<const Value *> &values, size_t firstIdx,
                                    size_t count) const
{
  const Type *curType = NULL;

  for(size_t i = firstIdx; i < firstIdx + count; i++)
  {
    const Constant *c = cast<const Constant>(values[i]);

    if(c->type != curType)
    {
      writer.Record(LLVMBC::ConstantsRecord::SETTYPE, getTypeID(c->type));
      curType = c->type;
    }

    if(c->isNULL())
    {
      writer.Record(LLVMBC::ConstantsRecord::CONST_NULL);
    }
    else if(c->isUndef())
    {
      writer.Record(LLVMBC::ConstantsRecord::UNDEF);
    }
    else if(c->op == Operation::GetElementPtr)
    {
      rdcarray<uint64_t> vals;
      vals.reserve(c->getMembers().size() * 2 + 1);

      // DXC's version of llvm always writes the explicit type here
      vals.push_back(getTypeID(c->getMembers()[0]->type->inner));

      for(size_t m = 0; m < c->getMembers().size(); m++)
      {
        vals.push_back(getTypeID(c->getMembers()[m]->type));
        vals.push_back(getValueID(c->getMembers()[m]));
      }

      writer.Record(LLVMBC::ConstantsRecord::EVAL_GEP, vals);
    }
    else if(IsCast(c->op))
    {
      uint64_t cast = EncodeCast(c->op);
      RDCASSERT(cast != ~0U);

      writer.Record(LLVMBC::ConstantsRecord::EVAL_CAST,
                    {cast, getTypeID(c->getInner()->type), getValueID(c->getInner())});
    }
    else if(c->op != Operation::NoOp)
    {
      uint64_t binop = EncodeBinOp(c->op);
      RDCASSERT(binop != ~0U);

      writer.Record(LLVMBC::ConstantsRecord::EVAL_BINOP,
                    {binop, getValueID(c->getMembers()[0]), getValueID(c->getMembers()[1])});
    }
    else if(c->isData())
    {
      rdcarray<uint64_t> vals;

      if(c->type->type == Type::Vector)
      {
        vals.reserve(c->type->elemCount);
        for(uint32_t m = 0; m < c->type->elemCount; m++)
          vals.push_back(c->type->bitWidth <= 32 ? c->getShaderVal().u32v[m]
                                                 : c->getShaderVal().u64v[m]);
      }
      else
      {
        vals.reserve(c->getMembers().size());
        for(size_t m = 0; m < c->getMembers().size(); m++)
          vals.push_back(cast<Literal>(c->getMembers()[m])->literal);
      }

      writer.Record(LLVMBC::ConstantsRecord::DATA, vals);
    }
    else if(c->type->type == Type::Vector || c->type->type == Type::Array ||
            c->type->type == Type::Struct)
    {
      rdcarray<uint64_t> vals;
      vals.reserve(c->getMembers().size());

      for(size_t m = 0; m < c->getMembers().size(); m++)
        vals.push_back(getValueID(c->getMembers()[m]));

      writer.Record(LLVMBC::ConstantsRecord::AGGREGATE, vals);
    }
    else if(c->type->scalarType == Type::Int)
    {
      writer.Record(LLVMBC::ConstantsRecord::INTEGER, LLVMBC::BitWriter::svbr(c->getS64()));
    }
    else if(c->type->scalarType == Type::Float)
    {
      writer.Record(LLVMBC::ConstantsRecord::FLOAT, c->getU64());
    }
    else if(!c->str.empty())
    {
      if(c->str.indexOf('\0') < 0)
      {
        writer.Record(LLVMBC::ConstantsRecord::CSTRING, c->str);
      }
      else
      {
        writer.Record(LLVMBC::ConstantsRecord::STRING, c->str);
      }
    }
  }
}

void ProgramEditor::EncodeMetadata(LLVMBC::BitcodeWriter &writer,
                                   const rdcarray<const Metadata *> &meta) const
{
  rdcarray<uint64_t> vals;

  bool errored = false;

  for(size_t i = 0; i < meta.size(); i++)
  {
    if(meta[i]->isString)
    {
      writer.Record(LLVMBC::MetaDataRecord::STRING_OLD, meta[i]->str);
    }
    else if(meta[i]->isConstant)
    {
      writer.Record(LLVMBC::MetaDataRecord::VALUE,
                    {getTypeID(meta[i]->type), getValueID(meta[i]->value)});
    }
    else if(meta[i]->dwarf || meta[i]->debugLoc)
    {
      if(!errored)
        RDCERR("Unexpected debug metadata node - expect to only encode stripped DXIL chunks");
      errored = true;

      // replace this with an error. This is an error to reference but we can't get away from that
      writer.Record(LLVMBC::MetaDataRecord::STRING_OLD, "unexpected_debug_metadata");
    }
    else
    {
      vals.clear();
      for(size_t m = 0; m < meta[i]->children.size(); m++)
        vals.push_back(getMetaIDOrNull(meta[i]->children[m]));

      writer.Record(meta[i]->isDistinct ? LLVMBC::MetaDataRecord::DISTINCT_NODE
                                        : LLVMBC::MetaDataRecord::NODE,
                    vals);
    }
  }
}

};    // namespace DXIL
