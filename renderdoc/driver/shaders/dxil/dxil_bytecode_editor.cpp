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

#include "dxil_bytecode_editor.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "maths/half_convert.h"
#include "dxil_bytecode.h"
#include "llvm_encoder.h"

DXIL::ProgramEditor::ProgramEditor(const DXBC::DXBCContainer *container, bytebuf &outBlob)
    : Program(container->GetDXILByteCode()->GetBytes().data(),
              container->GetDXILByteCode()->GetBytes().size()),
      m_OutBlob(outBlob)
{
  m_OutBlob = container->GetShaderBlob();
}

DXIL::ProgramEditor::~ProgramEditor()
{
  DXBC::DXBCContainer::ReplaceDXILBytecode(m_OutBlob, EncodeProgram());
}

#define getAttribID(a) uint64_t(a - m_Attributes.begin())
#define getTypeID(t) uint64_t(t - m_Types.begin())
#define getMetaID(m) uint64_t(m - m_Metadata.begin())
#define getMetaIDOrNull(m) (m ? (getMetaID(m) + 1) : 0ULL)
#define getFunctionMetaID(m)                                                        \
  uint64_t(m >= m_Metadata.begin() && m < m_Metadata.end() ? m - m_Metadata.begin() \
                                                           : m - f.metadata.begin())
#define getFunctionMetaIDOrNull(m) (m ? (getFunctionMetaID(m) + 1) : 0ULL)

#define getValueID(v) uint64_t(values.indexOf(v))

bytebuf DXIL::ProgramEditor::EncodeProgram() const
{
  rdcarray<Value> values = m_Values;

  bytebuf ret;

  LLVMBC::BitcodeWriter writer(ret);

  LLVMBC::BitcodeWriter::Config cfg = {};

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    cfg.maxAlign = RDCMAX(m_GlobalVars[i].align, cfg.maxAlign);
    RDCASSERT(m_GlobalVars[i].type->type == Type::Pointer);
    uint32_t typeIndex = uint32_t(getTypeID(m_GlobalVars[i].type->inner));
    cfg.maxGlobalType = RDCMAX(typeIndex, cfg.maxGlobalType);
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
    cfg.maxAlign = RDCMAX(m_Functions[i].align, cfg.maxAlign);

  for(size_t i = 0; i < m_Metadata.size(); i++)
  {
    if(m_Metadata[i].isString)
      cfg.hasMetaString = true;

    if(m_Metadata[i].debugLoc)
      cfg.hasDebugLoc = true;
  }

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    if(m_NamedMeta[i].isString)
      cfg.hasMetaString = true;

    if(m_NamedMeta[i].debugLoc)
      cfg.hasDebugLoc = true;
  }

  cfg.hasNamedMeta = !m_NamedMeta.empty();

  for(size_t i = m_GlobalVars.size() + m_Functions.size(); i < m_Values.size(); i++)
  {
    // stop once we pass constants
    if(m_Values[i].type != ValueType::Constant)
      break;
  }

  cfg.numTypes = m_Types.size();
  cfg.numSections = m_Sections.size();
  cfg.numGlobalValues = m_Values.size();

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
      if(m_AttributeGroups[i].valid)
      {
        const Attributes &group = m_AttributeGroups[i];

        vals.clear();
        vals.push_back(i);
        vals.push_back(group.index);

        // decompose params bitfield into bits
        if(group.params != Attribute::None)
        {
          uint64_t params = (uint64_t)group.params;
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
                  vals.push_back(group.align);
                  break;
                }
                case Attribute::StackAlignment:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group.stackAlign);
                  break;
                }
                case Attribute::Dereferenceable:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group.derefBytes);
                  break;
                }
                case Attribute::DereferenceableOrNull:
                {
                  vals.push_back(1);
                  vals.push_back(p);
                  vals.push_back(group.derefOrNullBytes);
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

        if(!group.strs.empty())
        {
          for(const rdcpair<rdcstr, rdcstr> &strAttr : group.strs)
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

  if(!m_Attributes.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::PARAMATTR_BLOCK);

    for(size_t i = 0; i < m_Attributes.size(); i++)
      writer.Record(LLVMBC::ParamAttrRecord::ENTRY, m_Attributes[i].groups);

    writer.EndBlock();
  }

  {
    writer.BeginBlock(LLVMBC::KnownBlock::TYPE_BLOCK);

    writer.Record(LLVMBC::TypeRecord::NUMENTRY, (uint32_t)m_Types.size());

    for(size_t i = 0; i < m_Types.size(); i++)
    {
      if(m_Types[i].isVoid())
      {
        writer.Record(LLVMBC::TypeRecord::VOID);
      }
      else if(m_Types[i].type == Type::Label)
      {
        writer.Record(LLVMBC::TypeRecord::LABEL);
      }
      else if(m_Types[i].type == Type::Metadata)
      {
        writer.Record(LLVMBC::TypeRecord::METADATA);
      }
      else if(m_Types[i].type == Type::Scalar && m_Types[i].scalarType == Type::Float)
      {
        if(m_Types[i].bitWidth == 16)
          writer.Record(LLVMBC::TypeRecord::HALF);
        else if(m_Types[i].bitWidth == 32)
          writer.Record(LLVMBC::TypeRecord::FLOAT);
        else if(m_Types[i].bitWidth == 64)
          writer.Record(LLVMBC::TypeRecord::DOUBLE);
      }
      else if(m_Types[i].type == Type::Scalar && m_Types[i].scalarType == Type::Int)
      {
        writer.Record(LLVMBC::TypeRecord::INTEGER, m_Types[i].bitWidth);
      }
      else if(m_Types[i].type == Type::Vector)
      {
        writer.Record(LLVMBC::TypeRecord::VECTOR,
                      {m_Types[i].elemCount, getTypeID(m_Types[i].inner)});
      }
      else if(m_Types[i].type == Type::Array)
      {
        writer.Record(LLVMBC::TypeRecord::ARRAY, {m_Types[i].elemCount, getTypeID(m_Types[i].inner)});
      }
      else if(m_Types[i].type == Type::Pointer)
      {
        writer.Record(LLVMBC::TypeRecord::POINTER,
                      {getTypeID(m_Types[i].inner), (uint64_t)m_Types[i].addrSpace});
      }
      else if(m_Types[i].type == Type::Struct)
      {
        if(m_Types[i].members.empty())
        {
          writer.Record(LLVMBC::TypeRecord::OPAQUE);
        }
        else
        {
          LLVMBC::TypeRecord type = LLVMBC::TypeRecord::STRUCT_ANON;

          if(!m_Types[i].name.empty())
          {
            writer.Record(LLVMBC::TypeRecord::STRUCT_NAME, m_Types[i].name);
            type = LLVMBC::TypeRecord::STRUCT_NAMED;
          }

          rdcarray<uint64_t> vals;

          vals.push_back(m_Types[i].packedStruct ? 1 : 0);

          for(const Type *t : m_Types[i].members)
            vals.push_back(getTypeID(t));

          writer.Record(type, vals);
        }
      }
      else if(m_Types[i].type == Type::Function)
      {
        rdcarray<uint64_t> vals;

        vals.push_back(m_Types[i].vararg ? 1 : 0);

        vals.push_back(getTypeID(m_Types[i].inner));

        for(const Type *t : m_Types[i].members)
          vals.push_back(getTypeID(t));

        writer.Record(LLVMBC::TypeRecord::FUNCTION, vals);
      }
    }

    writer.EndBlock();
  }

  // COMDAT would be next, but we don't read these (DXIL seems not to use them...?)

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
    const GlobalVar &g = m_GlobalVars[i];

    // global vars write the value type, not the pointer
    uint64_t typeIndex = getTypeID(g.type->inner);

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
                      typeIndex, uint64_t(((g.flags & GlobalFlags::IsConst) ? 1 : 0) | 0x2 |
                                          ((uint32_t)g.type->addrSpace << 2)),
                      g.initialiser ? getValueID(Value(g.initialiser)) + 1 : 0, linkageValue,
                      Log2Floor((uint32_t)g.align) + 1, uint64_t(g.section + 1),
                      // visibility
                      0U,
                      // TLS mode
                      0U,
                      // unnamed addr
                      unnamedAddr, (g.flags & GlobalFlags::ExternallyInitialised) ? 1U : 0U,
                      // DLL storage class
                      0U,
                      // comdat
                      0U,
                  });
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &f = m_Functions[i];
    uint64_t typeIndex = getTypeID(f.funcType->inner);

    writer.Record(LLVMBC::ModuleRecord::FUNCTION,
                  {
                      typeIndex,
                      // calling convention
                      0U,
                      // external/declaration
                      f.external ? 1U : 0U,
                      // linkage
                      0U,
                      // attributes
                      uint64_t(f.attrs ? 1U + (f.attrs - m_Attributes.begin()) : 0U),
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
                      0U,
                      // prefixdata
                      0U,
                      // personality
                      0U,
                  });
  }

  for(size_t i = 0; i < m_Aliases.size(); i++)
  {
    const Alias &a = m_Aliases[i];
    uint64_t typeIndex = getTypeID(a.type);

    writer.Record(LLVMBC::ModuleRecord::ALIAS, {
                                                   typeIndex, a.valID,
                                                   // linkage
                                                   0U,
                                                   // visibility
                                                   0U,
                                               });
  }

  // the symbols for constants start after the global variables and functions which we just
  // outputted
  if(!m_Constants.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::CONSTANTS_BLOCK);

    EncodeConstants(writer, values, m_Constants);

    writer.EndBlock();
  }

  if(!m_Metadata.empty())
  {
    writer.BeginBlock(LLVMBC::KnownBlock::METADATA_BLOCK);

    writer.EmitMetaDataAbbrev();

    EncodeMetadata(writer, values, m_Metadata);

    rdcarray<uint64_t> vals;

    for(size_t i = 0; i < m_NamedMeta.size(); i++)
    {
      writer.Record(LLVMBC::MetaDataRecord::NAME, m_NamedMeta[i].name);

      vals.clear();
      for(size_t m = 0; m < m_NamedMeta[i].children.size(); m++)
        vals.push_back(getMetaID(m_NamedMeta[i].children[m]));

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

  {
    writer.BeginBlock(LLVMBC::KnownBlock::VALUE_SYMTAB_BLOCK);

    rdcarray<rdcpair<size_t, const rdcstr *>> entries;

    for(size_t s = 0; s < m_Values.size(); s++)
    {
      const rdcstr *str = NULL;
      switch(m_Values[s].type)
      {
        case ValueType::GlobalVar: str = &m_Values[s].global->name; break;
        case ValueType::Function: str = &m_Values[s].function->name; break;
        case ValueType::Alias: str = &m_Values[s].alias->name; break;
        default: break;
      }

      if(str)
        entries.push_back({s, str});
    }

    // sort the entries by string in order
    std::sort(entries.begin(), entries.end(),
              [](const rdcpair<size_t, const rdcstr *> &a,
                 const rdcpair<size_t, const rdcstr *> &b) { return *a.second < *b.second; });

    // we use a special function to record the entry so it can take the string as-is to check it for
    // validity
    for(const rdcpair<size_t, const rdcstr *> &it : entries)
      writer.RecordSymTabEntry(it.first, *it.second);

    writer.EndBlock();
  }

#define encodeRelativeValueID(v)                              \
  {                                                           \
    uint64_t valID = getValueID(v);                           \
    if(valID <= instValueID)                                  \
    {                                                         \
      vals.push_back(instValueID - valID);                    \
    }                                                         \
    else                                                      \
    {                                                         \
      forwardRefs = true;                                     \
      /* signed integer two's complement for negative    */   \
      /* values referencing forward from the instruction */   \
      vals.push_back(0x100000000ULL - (valID - instValueID)); \
      vals.push_back(getTypeID(v.GetType()));                 \
    }                                                         \
  }

// some cases don't encode the type even for forward refs, if it's implicit (e.g. second parameter
// in a binop). This also doesn't count as a forward ref for the case of breaking the abbrev use
#define encodeRelativeValueIDTypeless(v)                      \
  {                                                           \
    uint64_t valID = getValueID(v);                           \
    if(valID <= instValueID)                                  \
    {                                                         \
      vals.push_back(instValueID - valID);                    \
    }                                                         \
    else                                                      \
    {                                                         \
      vals.push_back(0x100000000ULL - (valID - instValueID)); \
    }                                                         \
  }

  for(const Function &f : m_Functions)
  {
    if(f.external)
      continue;

    values.append(f.values);

    writer.BeginBlock(LLVMBC::KnownBlock::FUNCTION_BLOCK);

    writer.Record(LLVMBC::FunctionRecord::DECLAREBLOCKS, f.blocks.size());

    if(!f.constants.empty())
    {
      writer.BeginBlock(LLVMBC::KnownBlock::CONSTANTS_BLOCK);

      EncodeConstants(writer, values, f.constants);

      writer.EndBlock();
    }

    if(!f.metadata.empty())
    {
      writer.BeginBlock(LLVMBC::KnownBlock::METADATA_BLOCK);

      EncodeMetadata(writer, values, f.metadata);

      writer.EndBlock();
    }

    // value IDs for instructions start after all the constants
    uint32_t instValueID = uint32_t(m_Values.size() + f.constants.size() + f.args.size());

    uint32_t debugLoc = ~0U;

    bool forwardRefs = false;
    rdcarray<uint64_t> vals;

    bool needMetaAttach = !f.attachedMeta.empty();

    for(const Instruction &inst : f.instructions)
    {
      forwardRefs = false;
      vals.clear();

      needMetaAttach |= !inst.attachedMeta.empty();

      switch(inst.op)
      {
        case Operation::NoOp: RDCERR("Unexpected no-op encoding"); continue;
        case Operation::Call:
        {
          vals.push_back(inst.paramAttrs ? getAttribID(inst.paramAttrs) + 1 : 0);
          // always emit func type
          uint64_t flags = 1 << 15;
          if(inst.opFlags != InstructionFlags::NoFlags)
            flags |= 1 << 17;
          vals.push_back(flags);
          if(inst.opFlags != InstructionFlags::NoFlags)
            vals.push_back((uint64_t)inst.opFlags);
          vals.push_back(getTypeID(inst.funcCall->funcType->inner));
          encodeRelativeValueID(Value(inst.funcCall));
          for(size_t a = 0; a < inst.args.size(); a++)
          {
            if(inst.args[a].type == ValueType::Metadata)
            {
              vals.push_back(getFunctionMetaID(inst.args[a].meta));
            }
            else
            {
              encodeRelativeValueIDTypeless(inst.args[a]);
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
          encodeRelativeValueID(inst.args[0]);
          vals.push_back(getTypeID(inst.type));
          vals.push_back(EncodeCast(inst.op));

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CAST, vals, forwardRefs);
          break;
        }
        case Operation::ExtractVal:
        {
          encodeRelativeValueID(inst.args[0]);
          for(size_t i = 1; i < inst.args.size(); i++)
            vals.push_back(inst.args[i].literal);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_EXTRACTVAL, vals, forwardRefs);
          break;
        }
        case Operation::Ret:
        {
          if(!inst.args.empty())
          {
            encodeRelativeValueID(inst.args[0]);
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
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueIDTypeless(inst.args[1]);

          const Type *t = inst.args[0].GetType();

          const bool isFloatOp = (t->scalarType == Type::Float);

          uint64_t opcode = 0;
          switch(inst.op)
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

          if(inst.opFlags != InstructionFlags::NoFlags)
          {
            uint64_t flags = 0;
            if(inst.op == Operation::Add || inst.op == Operation::Sub ||
               inst.op == Operation::Mul || inst.op == Operation::ShiftLeft)
            {
              if(inst.opFlags & InstructionFlags::NoSignedWrap)
                flags |= 0x2;
              if(inst.opFlags & InstructionFlags::NoUnsignedWrap)
                flags |= 0x1;
              vals.push_back(flags);
            }
            else if(inst.op == Operation::SDiv || inst.op == Operation::UDiv ||
                    inst.op == Operation::LogicalShiftRight || inst.op == Operation::ArithShiftRight)
            {
              if(inst.opFlags & InstructionFlags::Exact)
                flags |= 0x1;
              vals.push_back(flags);
            }
            else if(isFloatOp)
            {
              // fast math flags overlap
              vals.push_back(uint64_t(inst.opFlags));
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
          vals.push_back(getTypeID(inst.type->inner));
          vals.push_back(getTypeID(inst.args[0].GetType()));
          vals.push_back(getValueID(inst.args[0]));
          uint64_t alignAndFlags = Log2Floor(inst.align) + 1;
          // DXC always sets this bit, as the type is ap ointer
          alignAndFlags |= 1U << 6;
          if(inst.opFlags & InstructionFlags::ArgumentAlloca)
            alignAndFlags |= 1U << 5;
          vals.push_back(alignAndFlags);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_ALLOCA, vals, forwardRefs);
          break;
        }
        case Operation::GetElementPtr:
        {
          vals.push_back((inst.opFlags & InstructionFlags::InBounds) ? 1U : 0U);
          vals.push_back(getTypeID(inst.args[0].GetType()->inner));

          for(size_t i = 0; i < inst.args.size(); i++)
          {
            encodeRelativeValueID(inst.args[i]);
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_GEP, vals, forwardRefs);
          break;
        }
        case Operation::Load:
        {
          encodeRelativeValueID(inst.args[0]);
          vals.push_back(getTypeID(inst.type));
          vals.push_back(Log2Floor(inst.align) + 1);
          vals.push_back((inst.opFlags & InstructionFlags::Volatile) ? 1U : 0U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_LOAD, vals, forwardRefs);
          break;
        }
        case Operation::Store:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueID(inst.args[1]);
          vals.push_back(Log2Floor(inst.align) + 1);
          vals.push_back((inst.opFlags & InstructionFlags::Volatile) ? 1U : 0U);
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
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueIDTypeless(inst.args[1]);

          uint64_t opcode = 0;
          switch(inst.op)
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

          if(inst.opFlags != InstructionFlags::NoFlags)
            vals.push_back((uint64_t)inst.opFlags);

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CMP2, vals, forwardRefs);
          break;
        }
        case Operation::Select:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueIDTypeless(inst.args[1]);
          encodeRelativeValueID(inst.args[2]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_VSELECT, vals, forwardRefs);
          break;
        }
        case Operation::ExtractElement:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueID(inst.args[1]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_EXTRACTELT, vals, forwardRefs);
          break;
        }
        case Operation::InsertElement:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueIDTypeless(inst.args[1]);
          encodeRelativeValueID(inst.args[2]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_INSERTELT, vals, forwardRefs);
          break;
        }
        case Operation::ShuffleVector:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueIDTypeless(inst.args[1]);
          encodeRelativeValueID(inst.args[2]);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_SHUFFLEVEC, vals, forwardRefs);
          break;
        }
        case Operation::InsertValue:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueID(inst.args[1]);
          for(size_t i = 2; i < inst.args.size(); i++)
            vals.push_back(inst.args[i].literal);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_INSERTVAL, vals, forwardRefs);
          break;
        }
        case Operation::Branch:
        {
          vals.push_back(uint64_t(inst.args[0].block - f.blocks.begin()));

          if(inst.args.size() > 1)
          {
            vals.push_back(uint64_t(inst.args[1].block - f.blocks.begin()));
            encodeRelativeValueIDTypeless(inst.args[2]);
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_BR, vals, forwardRefs);
          break;
        }
        case Operation::Phi:
        {
          vals.push_back(getTypeID(inst.type));

          for(size_t i = 0; i < inst.args.size(); i += 2)
          {
            uint64_t valID = getValueID(inst.args[i]);
            int64_t valRef = int64_t(instValueID) - int64_t(valID);

            vals.push_back(LLVMBC::BitWriter::svbr(valRef));
            vals.push_back(uint64_t(inst.args[i + 1].block - f.blocks.begin()));
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_PHI, vals, forwardRefs);
          break;
        }
        case Operation::Switch:
        {
          vals.push_back(getTypeID(inst.args[0].GetType()));
          encodeRelativeValueIDTypeless(inst.args[0]);

          vals.push_back(uint64_t(inst.args[1].block - f.blocks.begin()));

          for(size_t i = 2; i < inst.args.size(); i += 2)
          {
            vals.push_back(getValueID(inst.args[i]));
            vals.push_back(uint64_t(inst.args[i + 1].block - f.blocks.begin()));
          }

          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_SWITCH, vals, forwardRefs);
          break;
        }
        case Operation::Fence:
        {
          vals.push_back(((uint64_t)inst.opFlags & (uint64_t)InstructionFlags::SuccessOrderMask) >>
                         12U);
          vals.push_back((inst.opFlags & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_FENCE, vals, forwardRefs);
          break;
        }
        case Operation::CompareExchange:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueID(inst.args[1]);
          encodeRelativeValueIDTypeless(inst.args[2]);
          vals.push_back((inst.opFlags & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(((uint64_t)inst.opFlags & (uint64_t)InstructionFlags::SuccessOrderMask) >>
                         12U);
          vals.push_back((inst.opFlags & InstructionFlags::SingleThread) ? 0U : 1U);
          vals.push_back(((uint64_t)inst.opFlags & (uint64_t)InstructionFlags::FailureOrderMask) >>
                         15U);
          vals.push_back((inst.opFlags & InstructionFlags::Weak) ? 1U : 0U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_CMPXCHG, vals, forwardRefs);
          break;
        }
        case Operation::LoadAtomic:
        {
          encodeRelativeValueID(inst.args[0]);
          vals.push_back(getTypeID(inst.type));
          vals.push_back(Log2Floor(inst.align) + 1);
          vals.push_back((inst.opFlags & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(((uint64_t)inst.opFlags & (uint64_t)InstructionFlags::SuccessOrderMask) >>
                         12U);
          vals.push_back((inst.opFlags & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_LOADATOMIC, vals, forwardRefs);
          break;
        }
        case Operation::StoreAtomic:
        {
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueID(inst.args[1]);
          vals.push_back(Log2Floor(inst.align) + 1);
          vals.push_back((inst.opFlags & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(((uint64_t)inst.opFlags & (uint64_t)InstructionFlags::SuccessOrderMask) >>
                         12U);
          vals.push_back((inst.opFlags & InstructionFlags::SingleThread) ? 0U : 1U);
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
          encodeRelativeValueID(inst.args[0]);
          encodeRelativeValueIDTypeless(inst.args[1]);

          uint64_t opcode = 0;
          switch(inst.op)
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

          vals.push_back((inst.opFlags & InstructionFlags::Volatile) ? 1U : 0U);
          vals.push_back(((uint64_t)inst.opFlags & (uint64_t)InstructionFlags::SuccessOrderMask) >>
                         12U);
          vals.push_back((inst.opFlags & InstructionFlags::SingleThread) ? 0U : 1U);
          writer.RecordInstruction(LLVMBC::FunctionRecord::INST_ATOMICRMW, vals, forwardRefs);
          break;
        }
      }

      // instruction IDs are the values (i.e. all instructions that return non-void are a value)
      if(inst.type != m_VoidType)
        instValueID++;

      // no debug location? omit
      if(inst.debugLoc == ~0U)
        continue;

      // same as last time? emit 'again' record
      if(inst.debugLoc == debugLoc)
        writer.Record(LLVMBC::FunctionRecord::DEBUG_LOC_AGAIN);

      // new debug location
      const DebugLocation &loc = m_DebugLocations[inst.debugLoc];

      writer.Record(LLVMBC::FunctionRecord::DEBUG_LOC,
                    {
                        loc.line, loc.col, getFunctionMetaIDOrNull(loc.scope),
                        getFunctionMetaIDOrNull(loc.inlinedAt),
                    });

      debugLoc = inst.debugLoc;
    }

    if(needMetaAttach)
    {
      writer.BeginBlock(LLVMBC::KnownBlock::METADATA_ATTACHMENT);

      vals.clear();

      for(const rdcpair<uint64_t, Metadata *> &m : f.attachedMeta)
      {
        vals.push_back(m.first);
        vals.push_back(getFunctionMetaID(m.second));
      }

      if(!vals.empty())
        writer.Record(LLVMBC::MetaDataRecord::ATTACHMENT, vals);

      for(size_t i = 0; i < f.instructions.size(); i++)
      {
        if(f.instructions[i].attachedMeta.empty())
          continue;

        vals.clear();

        vals.push_back(uint64_t(i));

        for(const rdcpair<uint64_t, Metadata *> &m : f.instructions[i].attachedMeta)
        {
          vals.push_back(m.first);
          vals.push_back(getFunctionMetaID(m.second));
        }

        writer.Record(LLVMBC::MetaDataRecord::ATTACHMENT, vals);
      }

      writer.EndBlock();
    }

    if(!f.uselist.empty())
    {
      writer.BeginBlock(LLVMBC::KnownBlock::USELIST_BLOCK);

      for(const UselistEntry &u : f.uselist)
      {
        vals = u.shuffle;
        vals.push_back(getValueID(u.value));

        writer.Record(u.block ? LLVMBC::UselistRecord::BB : LLVMBC::UselistRecord::DEFAULT, vals);
      }

      writer.EndBlock();
    }

    writer.EndBlock();

    values.resize(values.size() - f.values.size());
  }

  writer.EndBlock();

  ProgramHeader header;

  header.ProgramVersion = ((m_Major & 0xf) << 4) | (m_Minor & 0xf);
  header.ProgramType = (uint16_t)m_Type;
  header.DxilMagic = MAKE_FOURCC('D', 'X', 'I', 'L');
  header.DxilVersion = m_DXILVersion;
  header.BitcodeOffset = sizeof(ProgramHeader) - offsetof(ProgramHeader, DxilMagic);
  header.BitcodeSize = (uint32_t)ret.size();
  header.SizeInUint32 = (uint32_t)AlignUp4(ret.size() + sizeof(ProgramHeader)) / sizeof(uint32_t);

  ret.insert(0, (const byte *)&header, sizeof(header));

  ret.resize(AlignUp4(ret.size()));

  return ret;
}

void DXIL::ProgramEditor::EncodeConstants(LLVMBC::BitcodeWriter &writer,
                                          const rdcarray<Value> &values,
                                          const rdcarray<Constant> &constants) const
{
  const Type *curType = NULL;

  for(const Constant &c : constants)
  {
    if(c.type != curType)
    {
      writer.Record(LLVMBC::ConstantsRecord::SETTYPE, getTypeID(c.type));
      curType = c.type;
    }

    if(c.nullconst)
    {
      writer.Record(LLVMBC::ConstantsRecord::CONST_NULL);
    }
    else if(c.undef)
    {
      writer.Record(LLVMBC::ConstantsRecord::UNDEF);
    }
    else if(c.op == Operation::GetElementPtr)
    {
      rdcarray<uint64_t> vals;
      vals.reserve(c.members.size() * 2 + 1);

      // DXC's version of llvm always writes the explicit type here
      vals.push_back(getTypeID(c.members[0].GetType()->inner));

      for(size_t m = 0; m < c.members.size(); m++)
      {
        vals.push_back(getTypeID(c.members[m].GetType()));
        vals.push_back(getValueID(c.members[m]));
      }

      writer.Record(LLVMBC::ConstantsRecord::EVAL_GEP, vals);
    }
    else if(c.op != Operation::NoOp)
    {
      uint64_t cast = EncodeCast(c.op);
      RDCASSERT(cast != ~0U);

      writer.Record(LLVMBC::ConstantsRecord::EVAL_CAST,
                    {cast, getTypeID(c.inner.GetType()), getValueID(c.inner)});
    }
    else if(c.data)
    {
      rdcarray<uint64_t> vals;
      vals.reserve(c.members.size());

      if(c.type->type == Type::Vector)
      {
        for(uint32_t m = 0; m < c.type->elemCount; m++)
          vals.push_back(c.type->bitWidth <= 32 ? c.val.u32v[m] : c.val.u64v[m]);
      }
      else
      {
        for(size_t m = 0; m < c.members.size(); m++)
          vals.push_back(c.members[m].literal);
      }

      writer.Record(LLVMBC::ConstantsRecord::DATA, vals);
    }
    else if(c.type->type == Type::Vector || c.type->type == Type::Array ||
            c.type->type == Type::Struct)
    {
      rdcarray<uint64_t> vals;
      vals.reserve(c.members.size());

      for(size_t m = 0; m < c.members.size(); m++)
        vals.push_back(getValueID(c.members[m]));

      writer.Record(LLVMBC::ConstantsRecord::AGGREGATE, vals);
    }
    else if(c.type->scalarType == Type::Int)
    {
      writer.Record(LLVMBC::ConstantsRecord::INTEGER, LLVMBC::BitWriter::svbr(c.val.s64v[0]));
    }
    else if(c.type->scalarType == Type::Float)
    {
      writer.Record(LLVMBC::ConstantsRecord::FLOAT, c.val.u64v[0]);
    }
    else if(!c.str.empty())
    {
      if(c.str.indexOf('\0') < 0)
      {
        writer.Record(LLVMBC::ConstantsRecord::CSTRING, c.str);
      }
      else
      {
        writer.Record(LLVMBC::ConstantsRecord::STRING, c.str);
      }
    }
  }
}

void DXIL::ProgramEditor::EncodeMetadata(LLVMBC::BitcodeWriter &writer, const rdcarray<Value> &values,
                                         const rdcarray<Metadata> &meta) const
{
  rdcarray<uint64_t> vals;

  bool errored = false;

  for(size_t i = 0; i < meta.size(); i++)
  {
    if(meta[i].isString)
    {
      writer.Record(LLVMBC::MetaDataRecord::STRING_OLD, meta[i].str);
    }
    else if(meta[i].isConstant)
    {
      writer.Record(LLVMBC::MetaDataRecord::VALUE,
                    {getTypeID(meta[i].type), getValueID(meta[i].value)});
    }
    else if(meta[i].dwarf || meta[i].debugLoc)
    {
      if(!errored)
        RDCERR("Unexpected debug metadata node - expect to only encode stripped DXIL chunks");
      errored = true;

      // replace this with the first NULL constant value
      for(size_t c = 0; c < m_Constants.size(); c++)
      {
        if(m_Constants[c].nullconst)
        {
          writer.Record(LLVMBC::MetaDataRecord::VALUE, {getTypeID(m_Constants[c].type), (uint64_t)c});
        }
      }
    }
    else
    {
      vals.clear();
      for(size_t m = 0; m < meta[i].children.size(); m++)
        vals.push_back(getMetaIDOrNull(meta[i].children[m]));

      writer.Record(
          meta[i].isDistinct ? LLVMBC::MetaDataRecord::DISTINCT_NODE : LLVMBC::MetaDataRecord::NODE,
          vals);
    }
  }
}
