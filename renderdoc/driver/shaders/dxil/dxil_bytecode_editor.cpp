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

bytebuf DXIL::ProgramEditor::EncodeProgram()
{
  bytebuf ret;

  LLVMBC::BitcodeWriter writer(ret);

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
