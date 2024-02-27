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

#pragma once

#include <map>
#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"
#include "llvm_bitreader.h"

namespace LLVMBC
{
struct BlockOrRecord
{
  uint32_t id;
  uint32_t blockDwordLength = 0;    // 0 for records

  bool IsBlock() const { return blockDwordLength > 0; }
  bool IsRecord() const { return blockDwordLength == 0; }
  // if a block, the child blocks/records
  rdcarray<BlockOrRecord> children;

  rdcstr getString(size_t startOffset = 0) const;

  // if a record, the ops
  rdcarray<uint64_t> ops;
  // if this is an abbreviated record with a blob, this is the last operand
  // this points into the overall byte storage, so the lifetime is limited.
  const byte *blob = NULL;
  size_t blobLength = 0;
};

struct AbbrevParam;
struct AbbrevDesc;
struct BlockContext;
struct BlockInfo;

class BitcodeReader
{
public:
  BitcodeReader(const byte *bitcode, size_t length);
  ~BitcodeReader();
  BlockOrRecord ReadToplevelBlock();
  bool AtEndOfStream();

  static bool Valid(const byte *bitcode, size_t length);

private:
  BitReader b;
  size_t abbrevSize;

  void ReadBlockContents(BlockOrRecord &block);
  const AbbrevDesc &getAbbrev(uint32_t blockId, uint32_t abbrevID);
  uint64_t decodeAbbrevParam(const AbbrevParam &param);

  rdcarray<BlockContext *> blockStack;
  std::map<uint32_t, BlockInfo *> blockInfo;
};

};    // namespace LLVMBC
