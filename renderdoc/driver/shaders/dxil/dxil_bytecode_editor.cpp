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

bytebuf DXIL::ProgramEditor::EncodeProgram() const
{
  bytebuf ret;

  std::map<Symbol, uint32_t> valueId;

  LLVMBC::BitcodeWriter writer(ret);

  uint64_t maxAlign = 0;
  uint32_t maxGlobalType = 0;

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    maxAlign = RDCMAX(m_GlobalVars[i].align, maxAlign);
    RDCASSERT(m_GlobalVars[i].type->type == Type::Pointer);
    uint32_t typeIndex = uint32_t(m_GlobalVars[i].type->inner - m_Types.begin());
    maxGlobalType = RDCMAX(typeIndex, maxGlobalType);
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
    maxAlign = RDCMAX(m_Functions[i].align, maxAlign);

  writer.ConfigureSizes(m_Types.size(), m_Sections.size(), maxAlign, maxGlobalType);

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
        size_t innerTypeIndex = m_Types[i].inner - m_Types.begin();
        writer.Record(LLVMBC::TypeRecord::VECTOR, {m_Types[i].elemCount, innerTypeIndex});
      }
      else if(m_Types[i].type == Type::Array)
      {
        size_t innerTypeIndex = m_Types[i].inner - m_Types.begin();
        writer.Record(LLVMBC::TypeRecord::ARRAY, {m_Types[i].elemCount, innerTypeIndex});
      }
      else if(m_Types[i].type == Type::Pointer)
      {
        size_t innerTypeIndex = m_Types[i].inner - m_Types.begin();
        writer.Record(LLVMBC::TypeRecord::POINTER, {innerTypeIndex, (uint64_t)m_Types[i].addrSpace});
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
            vals.push_back(t - m_Types.begin());

          writer.Record(type, vals);
        }
      }
      else if(m_Types[i].type == Type::Function)
      {
        rdcarray<uint64_t> vals;

        vals.push_back(m_Types[i].vararg ? 1 : 0);

        vals.push_back(m_Types[i].inner - m_Types.begin());

        for(const Type *t : m_Types[i].members)
          vals.push_back(t - m_Types.begin());

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
    size_t typeIndex = g.type->inner - m_Types.begin();

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
                      g.initialiser.type != SymbolType::Unknown ? valueId[g.initialiser] + 1 : 0,
                      linkageValue, 32 - Bits::CountLeadingZeroes(g.align), uint64_t(g.section + 1),
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
    size_t typeIndex = f.funcType - m_Types.begin();

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
    size_t typeIndex = a.type - m_Types.begin();

    writer.Record(LLVMBC::ModuleRecord::ALIAS, {
                                                   typeIndex, a.valID,
                                                   // linkage
                                                   0U,
                                                   // visibility
                                                   0U,
                                               });
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
