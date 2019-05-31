/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "spirv_common.h"
#include "common/common.h"

template <>
rdcstr DoStringise(const rdcspv::Id &el)
{
  uint32_t id;
  RDCCOMPILE_ASSERT(sizeof(el) == sizeof(id), "SPIR-V Id isn't 32-bit!");
  memcpy(&id, &el, sizeof(el));
  return StringFormat::Fmt("%u", id);
}

void rdcspv::Iter::nopRemove(size_t idx, size_t count)
{
  RDCASSERT(idx >= 1);
  size_t oldSize = size();

  if(count == 0)
    count = oldSize - idx;

  // reduce the size of this op
  word(0) = rdcspv::Operation::MakeHeader(opcode(), oldSize - count);

  if(idx + count < oldSize)
  {
    // move any words on the end into the middle, then nop them
    for(size_t i = 0; i < count; i++)
    {
      word(idx + i) = word(idx + count + i);
      word(oldSize - i - 1) = OpNopWord;
    }
  }
  else
  {
    for(size_t i = 0; i < count; i++)
    {
      word(idx + i) = OpNopWord;
    }
  }
}

void rdcspv::Iter::nopRemove()
{
  for(size_t i = 0, sz = size(); i < sz; i++)
    word(i) = OpNopWord;
}