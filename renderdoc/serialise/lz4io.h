/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "lz4/lz4.h"
#include "streamio.h"

class LZ4Compressor : public Compressor
{
public:
  LZ4Compressor(StreamWriter *write, Ownership own);
  ~LZ4Compressor();

  bool Write(const void *data, uint64_t numBytes);
  bool Finish();

private:
  bool FlushPage0();

  byte *m_Page[2];
  byte *m_CompressBuffer;
  uint64_t m_PageOffset;

  LZ4_stream_t m_LZ4Comp;
};

class LZ4Decompressor : public Decompressor
{
public:
  LZ4Decompressor(StreamReader *read, Ownership own);
  ~LZ4Decompressor();

  bool Recompress(Compressor *comp);
  bool Read(void *data, uint64_t numBytes);

private:
  bool FillPage0();

  byte *m_Page[2];
  byte *m_CompressBuffer;
  uint64_t m_PageOffset;
  uint64_t m_PageLength;

  LZ4_streamDecode_t m_LZ4Decomp;
};
