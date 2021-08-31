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

namespace LLVMBC
{
enum class AbbrevEncoding : uint8_t
{
  Fixed = 1,
  VBR = 2,
  Array = 3,
  Char6 = 4,
  Blob = 5,
  // the abbrev encoding is only 3 bits, so 8 is not representable, we can store whether or not
  // we're a literal this way.
  Literal = 8,
};

enum AbbrevId
{
  END_BLOCK = 0,
  ENTER_SUBBLOCK = 1,
  DEFINE_ABBREV = 2,
  UNABBREV_RECORD = 3,
  APPLICATION_ABBREV = 4,
};

enum class BlockInfoRecord
{
  SETBID = 1,
  BLOCKNAME = 2,
  SETRECORDNAME = 3,
};

static const uint32_t BitcodeMagic = MAKE_FOURCC('B', 'C', 0xC0, 0xDE);
};
