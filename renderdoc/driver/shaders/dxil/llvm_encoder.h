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

#pragma once

#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"
#include "llvm_bitwriter.h"
#include "llvm_common.h"

namespace LLVMBC
{
class BitcodeWriter
{
public:
  BitcodeWriter(bytebuf &buf);
  ~BitcodeWriter();

  void BeginBlock(KnownBlock block);

  void EndBlock();

  void ModuleBlockInfo(uint32_t numTypes);

  void Unabbrev(uint32_t record, uint32_t val);
  void Unabbrev(uint32_t record, uint64_t val);
  void Unabbrev(uint32_t record, const rdcarray<uint32_t> &vals);
  void Unabbrev(uint32_t record, const rdcarray<uint64_t> &vals);

private:
  BitWriter b;

  size_t abbrevSize;
  KnownBlock curBlock;

  rdcarray<rdcpair<KnownBlock, size_t>> blockStack;
};

};    // namespace LLVMBC
