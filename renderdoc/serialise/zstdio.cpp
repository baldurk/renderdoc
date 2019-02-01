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

#define ZSTD_STATIC_LINKING_ONLY
#include "zstdio.h"

static const uint64_t zstdBlockSize = 128 * 1024;
static const uint64_t compressBlockSize = ZSTD_compressBound(zstdBlockSize);

ZSTDCompressor::ZSTDCompressor(StreamWriter *write, Ownership own) : Compressor(write, own)
{
  m_Page = AllocAlignedBuffer(zstdBlockSize);
  m_CompressBuffer = AllocAlignedBuffer(compressBlockSize);

  m_PageOffset = 0;

  m_Stream = ZSTD_createCStream();
}

ZSTDCompressor::~ZSTDCompressor()
{
  ZSTD_freeCStream(m_Stream);

  FreeAlignedBuffer(m_Page);
  FreeAlignedBuffer(m_CompressBuffer);
}

bool ZSTDCompressor::Write(const void *data, uint64_t numBytes)
{
  // if we encountered a stream error this will be NULL
  if(!m_CompressBuffer)
    return false;

  if(numBytes == 0)
    return true;

  // this is largely similar to LZ4Compressor, so check the comments there for more details.
  // The only difference is that the lz4 streaming compression assumes a history of 64kb, where
  // here we use a larger block size but no history must be maintained.

  if(m_PageOffset + numBytes <= zstdBlockSize)
  {
    // simplest path, no page wrapping/spanning at all
    memcpy(m_Page + m_PageOffset, data, (size_t)numBytes);
    m_PageOffset += numBytes;

    return true;
  }
  else
  {
    const byte *src = (const byte *)data;

    // copy whatever will fit on this page
    {
      uint64_t firstBytes = zstdBlockSize - m_PageOffset;
      memcpy(m_Page + m_PageOffset, src, (size_t)firstBytes);

      m_PageOffset += firstBytes;
      numBytes -= firstBytes;
      src += firstBytes;
    }

    bool success = true;

    while(success && numBytes > 0)
    {
      // flush page
      success &= FlushPage();

      if(!success)
        return success;

      // how many bytes can we copy in this page?
      uint64_t partialBytes = RDCMIN(zstdBlockSize, numBytes);
      memcpy(m_Page, src, (size_t)partialBytes);

      // advance the source pointer, dest offset, and remove the bytes we read
      m_PageOffset += partialBytes;
      numBytes -= partialBytes;
      src += partialBytes;
    }

    return success;
  }
}

bool ZSTDCompressor::Finish()
{
  // This function just writes the current page and closes zstd. Since we assume all blocks are
  // precisely 64kb in size
  // only the last one can be smaller, so we only write a partial page when finishing.
  // Calling Write() after Finish() is illegal

  return FlushPage();
}

bool ZSTDCompressor::FlushPage()
{
  // if we encountered a stream error this will be NULL
  if(!m_CompressBuffer)
    return false;

  ZSTD_inBuffer in = {m_Page, (size_t)m_PageOffset, 0};
  ZSTD_outBuffer out = {m_CompressBuffer, ZSTD_CStreamOutSize(), 0};

  bool success = true;

  success &= CompressZSTDFrame(in, out);

  // if there was an error, bail
  if(!m_CompressBuffer)
    return false;

  // a bit redundant to write this but it means we can read the entire frame without
  // doing multiple reads
  success &= m_Write->Write((uint32_t)out.pos);
  success &= m_Write->Write(m_CompressBuffer, out.pos);

  // start writing to the start of the page again
  m_PageOffset = 0;

  return success;
}

bool ZSTDCompressor::CompressZSTDFrame(ZSTD_inBuffer &in, ZSTD_outBuffer &out)
{
  size_t err = ZSTD_initCStream(m_Stream, 7);

  if(ZSTD_isError(err))
  {
    RDCERR("Error compressing: %s", ZSTD_getErrorName(err));
    FreeAlignedBuffer(m_Page);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page = m_CompressBuffer = NULL;
    return false;
  }

  // keep calling compressStream until everything is consumed
  while(in.pos < in.size)
  {
    size_t inpos = in.pos;
    size_t outpos = out.pos;

    err = ZSTD_compressStream(m_Stream, &out, &in);

    if(ZSTD_isError(err) || (inpos == in.pos && outpos == out.pos))
    {
      if(ZSTD_isError(err))
        RDCERR("Error compressing: %s", ZSTD_getErrorName(err));
      else
        RDCERR("Error compressing, no progress made");
      FreeAlignedBuffer(m_Page);
      FreeAlignedBuffer(m_CompressBuffer);
      m_Page = m_CompressBuffer = NULL;
      return false;
    }
  }

  err = ZSTD_endStream(m_Stream, &out);

  if(ZSTD_isError(err) || err != 0)
  {
    if(ZSTD_isError(err))
      RDCERR("Error compressing: %s", ZSTD_getErrorName(err));
    else
      RDCERR("Error compressing, couldn't end stream");
    FreeAlignedBuffer(m_Page);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page = m_CompressBuffer = NULL;
    return false;
  }

  return true;
}

ZSTDDecompressor::ZSTDDecompressor(StreamReader *read, Ownership own) : Decompressor(read, own)
{
  m_Page = AllocAlignedBuffer(zstdBlockSize);
  m_CompressBuffer = AllocAlignedBuffer(compressBlockSize);

  m_PageOffset = 0;
  m_PageLength = 0;

  m_Stream = ZSTD_createDStream();
}

ZSTDDecompressor::~ZSTDDecompressor()
{
  ZSTD_freeDStream(m_Stream);
  FreeAlignedBuffer(m_Page);
  FreeAlignedBuffer(m_CompressBuffer);
}

bool ZSTDDecompressor::Recompress(Compressor *comp)
{
  bool success = true;

  while(success && !m_Read->AtEnd())
  {
    success &= FillPage();
    if(success)
      success &= comp->Write(m_Page, m_PageLength);
  }
  success &= comp->Finish();

  return success;
}

bool ZSTDDecompressor::Read(void *data, uint64_t numBytes)
{
  // if we encountered a stream error this will be NULL
  if(!m_CompressBuffer)
    return false;

  if(numBytes == 0)
    return true;

  // this is simpler than the ZstdWriter::Write() implementation.
  // At any point, m_Page contains the current window with uncompressed bytes.
  // If we can satisfy a read from it, then we just memcpy and increment m_PageOffset.
  // When we wrap around, we do a partial memcpy from m_Page, then decompress more bytes.

  // if we already have all the data in-memory, just copy and return
  uint64_t available = m_PageLength - m_PageOffset;

  if(numBytes <= available)
  {
    memcpy(data, m_Page + m_PageOffset, (size_t)numBytes);
    m_PageOffset += numBytes;
    return true;
  }

  byte *dst = (byte *)data;

  // copy what remains in m_Page
  memcpy(dst, m_Page + m_PageOffset, (size_t)available);

  // adjust what needs to be copied
  dst += available;
  numBytes -= available;

  bool success = true;

  while(success && numBytes > 0)
  {
    success &= FillPage();

    if(!success)
      return success;

    // if we can now satisfy the remainder of the read, do so and return
    if(numBytes <= m_PageLength)
    {
      memcpy(dst, m_Page, (size_t)numBytes);
      m_PageOffset += numBytes;
      return success;
    }

    // otherwise copy this page in and continue
    memcpy(dst, m_Page, (size_t)m_PageLength);
    dst += m_PageLength;
    numBytes -= m_PageLength;
  }

  return success;
}

bool ZSTDDecompressor::FillPage()
{
  uint32_t compSize = 0;

  bool success = true;

  success &= m_Read->Read(compSize);
  success &= m_Read->Read(m_CompressBuffer, compSize);

  if(!success)
  {
    FreeAlignedBuffer(m_Page);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page = m_CompressBuffer = NULL;
    return false;
  }

  size_t err = ZSTD_initDStream(m_Stream);

  if(ZSTD_isError(err))
  {
    RDCERR("Error decompressing: %s", ZSTD_getErrorName(err));
    FreeAlignedBuffer(m_Page);
    FreeAlignedBuffer(m_CompressBuffer);
    m_Page = m_CompressBuffer = NULL;
    return false;
  }

  ZSTD_inBuffer in = {m_CompressBuffer, compSize, 0};
  ZSTD_outBuffer out = {m_Page, zstdBlockSize, 0};

  // keep calling compressStream until everything is consumed
  while(in.pos < in.size)
  {
    size_t inpos = in.pos;
    size_t outpos = out.pos;

    err = ZSTD_decompressStream(m_Stream, &out, &in);

    if(ZSTD_isError(err) || (inpos == in.pos && outpos == out.pos))
    {
      if(ZSTD_isError(err))
        RDCERR("Error decompressing: %s", ZSTD_getErrorName(err));
      else
        RDCERR("Error decompressing, no progress made");
      FreeAlignedBuffer(m_Page);
      FreeAlignedBuffer(m_CompressBuffer);
      m_Page = m_CompressBuffer = NULL;
      return false;
    }
  }

  m_PageOffset = 0;
  m_PageLength = out.pos;

  return success;
}
