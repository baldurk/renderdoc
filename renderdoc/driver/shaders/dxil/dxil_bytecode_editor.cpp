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

  LLVMBC::BitcodeWriter writer(ret);

  writer.BeginBlock(LLVMBC::KnownBlock::MODULE_BLOCK);

  writer.Record(LLVMBC::ModuleRecord::VERSION, 1U);

  {
    writer.ModuleBlockInfo((uint32_t)m_Types.size());
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
