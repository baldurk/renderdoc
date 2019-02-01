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

#include "lz4io.h"

static const uint64_t lz4BlockSize = 64 * 1024;

LZ4Compressor::LZ4Compressor(StreamWriter *write, Ownership own) : Compressor(write, own)
{
  m_Page[0] = AllocAlignedBuffer(lz4BlockSize);
  m_Page[1] = AllocAlignedBuffer(lz4BlockSize);
  m_CompressBuffer = AllocAlignedBuffer(LZ4_COMPRESSBOUND(lz4BlockSize));

  m_PageOffset = 0;

  LZ4_resetStream(&m_LZ4Comp);
}

LZ4Compressor::~LZ4Compressor()
{
  FreeAlignedBuffer(m_Page[0]);
  FreeAlignedBuffer(m_Page[1]);
  FreeAlignedBuffer(m_CompressBuffer);
}

bool LZ4Compressor::Write(const void *data, uint64_t numBytes)
{
  // if we encountered a stream error this will be NULL
  if(!m_CompressBuffer)
    return false;

  if(numBytes == 0)
    return true;

  // The basic plan is:
  // Write into page N incrementally until it is completely full. When full, flush it out to lz4 and
  // swap pages.
  // This keeps lz4 happy with 64kb of history each time it compresses.
  // If we are writing some data the crosses the boundary between pages, we write the part that will
  // fit on one page, flush & swap, write the rest into the next page.

  if(m_PageOffset + numBytes <= lz4BlockSize)
  {
    // simplest path, no page wrapping/spanning at all
    memcpy(m_Page[0] + m_PageOffset, data, (size_t)numBytes);
    m_PageOffset += numBytes;

    return true;
  }
  else
  {
    // do partial copies that span pages and flush as necessary

    const byte *src = (const byte *)data;

    // copy whatever will fit on this page
    {
      uint64_t firstBytes = lz4BlockSize - m_PageOffset;
      memcpy(m_Page[0] + m_PageOffset, src, (size_t)firstBytes);

      m_PageOffset += firstBytes;
      numBytes -= firstBytes;
      src += firstBytes;
    }

    bool success = true;

    while(success && numBytes > 0)
    {
      // flush and swap pages
      success &= FlushPage0();

      if(!success)
        return success;

      // how many bytes can we copy in this page?
      uint64_t partialBytes = RDCMIN(lz4BlockSize, numBytes);
      memcpy(m_Page[0], src, (size_t)partialBytes);

      // advance the source pointer, dest offset, and remove the bytes we read
      m_PageOffset += partialBytes;
      numBytes -= partialBytes;
      src += partialBytes;
    }

    return success;
  }
}

bool LZ4Compressor::Finish()
{
  // This function just writes the current page and closes lz4. Since we assume all blocks are
  // precisely 64kb in size
  // only the last one can be smaller, so we only write a partial page when finishing.
  // Calling Write() after Finish() is illegal
  return FlushPage0();
}

bool LZ4Compressor::FlushPage0()
{
  // if we encountered a stream error this will be NULL
  if(!m_CompressBuffer)
    return false;

  // m_PageOffset is the amount written, usually equal to lz4BlockSize except the last block.
  int32_t compSize =
      LZ4_compress_fast_continue(&m_LZ4Comp, (const char *)m_Page[0], (char *)m_CompressBuffer,
                                 (int)m_PageOffset, (int)LZ4_COMPRESSBOUND(lz4BlockSize), 1);

  if(compSize < 0)
  {
    RDCERR("Error compressing: %i", compSize);
    FreeAlignedBuffer(m_Page[0]);
    FreeAlignedBuffer(m_Page[1]);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page[0] = m_Page[1] = m_CompressBuffer = NULL;
    return false;
  }

  bool success = true;

  success &= m_Write->Write(compSize);
  success &= m_Write->Write(m_CompressBuffer, compSize);

  // swap pages
  std::swap(m_Page[0], m_Page[1]);

  // start writing to the start of the page again
  m_PageOffset = 0;

  return success;
}

LZ4Decompressor::LZ4Decompressor(StreamReader *read, Ownership own) : Decompressor(read, own)
{
  m_Page[0] = AllocAlignedBuffer(lz4BlockSize);
  m_Page[1] = AllocAlignedBuffer(lz4BlockSize);
  m_CompressBuffer = AllocAlignedBuffer(LZ4_COMPRESSBOUND(lz4BlockSize));

  m_PageOffset = 0;
  m_PageLength = 0;

  LZ4_setStreamDecode(&m_LZ4Decomp, NULL, 0);
}

LZ4Decompressor::~LZ4Decompressor()
{
  FreeAlignedBuffer(m_Page[0]);
  FreeAlignedBuffer(m_Page[1]);
  FreeAlignedBuffer(m_CompressBuffer);
}

bool LZ4Decompressor::Recompress(Compressor *comp)
{
  bool success = true;

  while(success && !m_Read->AtEnd())
  {
    success &= FillPage0();
    if(success)
      success &= comp->Write(m_Page[0], m_PageLength);
  }
  success &= comp->Finish();

  return success;
}

bool LZ4Decompressor::Read(void *data, uint64_t numBytes)
{
  // if we encountered a stream error this will be NULL
  if(!m_CompressBuffer)
    return false;

  if(numBytes == 0)
    return true;

  // At any point, m_Page[0] contains the current window with uncompressed bytes.
  // If we can satisfy a read from it, then we just memcpy and increment m_PageOffset.
  // When we wrap around, we do a partial memcpy from m_Page[0], then swap the pages and
  // decompress some more bytes into m_Page[0]. Thus, m_Page[1] contains the history (if
  // it exists)

  // if we already have all the data in-memory, just copy and return
  uint64_t available = m_PageLength - m_PageOffset;

  if(numBytes <= available)
  {
    memcpy(data, m_Page[0] + m_PageOffset, (size_t)numBytes);
    m_PageOffset += numBytes;
    return true;
  }

  byte *dst = (byte *)data;

  // copy what remains in m_Page[0]
  memcpy(dst, m_Page[0] + m_PageOffset, (size_t)available);

  // adjust what needs to be copied
  dst += available;
  numBytes -= available;

  bool success = true;

  while(success && numBytes > 0)
  {
    success &= FillPage0();

    if(!success)
      return success;

    // if we can now satisfy the remainder of the read, do so and return
    if(numBytes <= m_PageLength)
    {
      memcpy(dst, m_Page[0], (size_t)numBytes);
      m_PageOffset += numBytes;
      return success;
    }

    // otherwise copy this page in and continue
    memcpy(dst, m_Page[0], (size_t)m_PageLength);
    dst += m_PageLength;
    numBytes -= m_PageLength;
  }

  return success;
}

bool LZ4Decompressor::FillPage0()
{
  // swap pages
  std::swap(m_Page[0], m_Page[1]);

  int32_t compSize = 0;

  bool success = true;

  success &= m_Read->Read(compSize);
  if(!success || compSize < 0 || compSize > (int)LZ4_COMPRESSBOUND(lz4BlockSize))
  {
    RDCERR("Error reading size: %i", compSize);
    FreeAlignedBuffer(m_Page[0]);
    FreeAlignedBuffer(m_Page[1]);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page[0] = m_Page[1] = m_CompressBuffer = NULL;
    return false;
  }
  success &= m_Read->Read(m_CompressBuffer, compSize);

  if(!success)
  {
    RDCERR("Error reading block: %i", compSize);
    FreeAlignedBuffer(m_Page[0]);
    FreeAlignedBuffer(m_Page[1]);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page[0] = m_Page[1] = m_CompressBuffer = NULL;
    return false;
  }

  int32_t decompSize = LZ4_decompress_safe_continue(&m_LZ4Decomp, (const char *)m_CompressBuffer,
                                                    (char *)m_Page[0], compSize, lz4BlockSize);

  if(decompSize < 0)
  {
    RDCERR("Error decompressing: %i", decompSize);
    FreeAlignedBuffer(m_Page[0]);
    FreeAlignedBuffer(m_Page[1]);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page[0] = m_Page[1] = m_CompressBuffer = NULL;
    return false;
  }

  m_PageOffset = 0;
  m_PageLength = decompSize;

  return success;
}
