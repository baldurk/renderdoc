/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include "spirv_editor.h"
#include "common/common.h"

SPIRVEditor::SPIRVEditor(std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirvWords.size() < 5 || spirvWords[0] != spv::MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    return;
  }

  moduleVersion.major = uint8_t((spirvWords[1] & 0x00ff0000) >> 16);
  moduleVersion.minor = uint8_t((spirvWords[1] & 0x0000ff00) >> 8);
  generator = spirvWords[2];
  idBound = SPIRVIterator(spirvWords, 3);

  // [4] is reserved
  RDCASSERT(spirv[4] == 0);

  SPIRVIterator it(spirvWords, 5);

  while(it)
  {
    spv::Op opcode = it.opcode();

    if(opcode == spv::OpEntryPoint)
    {
      SPIRVEntry entry;
      entry.iter = it;
      entry.id = it.word(2);
      entry.name = (const char *)&it.word(3);

      entries.push_back(entry);
    }

    it++;
  }
}

uint32_t SPIRVEditor::MakeId()
{
  if(!idBound)
    return 0;

  uint32_t ret = *idBound;
  (*idBound)++;
  return ret;
}
