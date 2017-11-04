/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "streamio.h"
#include <errno.h>

Compressor::~Compressor()
{
  if(m_Ownership == Ownership::Stream && m_Write)
    delete m_Write;
}

Decompressor::~Decompressor()
{
  if(m_Ownership == Ownership::Stream && m_Read)
    delete m_Read;
}

static const uint64_t initialBufferSize = 64 * 1024;
const byte StreamWriter::empty[128] = {};

StreamReader::StreamReader(const byte *buffer, uint64_t bufferSize)
{
  m_InputSize = m_BufferSize = bufferSize;
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  memcpy(m_BufferHead, buffer, (size_t)m_BufferSize);

  m_Ownership = Ownership::Nothing;
}

StreamReader::StreamReader(const std::vector<byte> &buffer)
{
  m_InputSize = m_BufferSize = buffer.size();
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  memcpy(m_BufferHead, buffer.data(), (size_t)m_BufferSize);

  m_Ownership = Ownership::Nothing;
}

StreamReader::StreamReader(StreamInvalidType)
{
  m_InputSize = 0;

  m_BufferSize = 0;
  m_BufferHead = m_BufferBase = NULL;

  m_Ownership = Ownership::Nothing;
}

StreamReader::StreamReader(Network::Socket *sock, Ownership own)
{
  m_Sock = sock;

  m_InputSize = m_BufferSize = initialBufferSize;
  m_BufferBase = AllocAlignedBuffer(m_BufferSize);
  // place head at the *end* of the buffer, because for sockets we pretend the buffer is constantly
  // exhausted, and just do a read of the minimum number of bytes each time to satisfy each read
  m_BufferHead = m_BufferBase + m_BufferSize;

  m_Ownership = own;
}

StreamReader::StreamReader(FILE *file, uint64_t fileSize, Ownership own)
{
  if(file == NULL)
  {
    m_InputSize = 0;

    m_BufferSize = 0;
    m_BufferHead = m_BufferBase = NULL;

    m_Ownership = Ownership::Nothing;
    return;
  }

  m_File = file;
  m_InputSize = fileSize;

  m_BufferSize = initialBufferSize;
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  ReadFromExternal(0, RDCMIN(m_InputSize, m_BufferSize));

  m_Ownership = own;
}

StreamReader::StreamReader(FILE *file)
{
  if(file == NULL)
  {
    m_InputSize = 0;

    m_BufferSize = 0;
    m_BufferHead = m_BufferBase = NULL;

    m_Ownership = Ownership::Nothing;
    return;
  }

  FileIO::fseek64(file, 0, SEEK_END);
  m_InputSize = FileIO::ftell64(file);
  FileIO::fseek64(file, 0, SEEK_SET);

  m_File = file;

  m_BufferSize = initialBufferSize;
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  ReadFromExternal(0, RDCMIN(m_InputSize, m_BufferSize));

  m_Ownership = Ownership::Stream;
}

StreamReader::StreamReader(StreamReader *reader, uint64_t bufferSize)
{
  m_InputSize = m_BufferSize = bufferSize;
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  reader->Read(m_BufferBase, bufferSize);

  m_Ownership = Ownership::Nothing;
}

StreamReader::StreamReader(Decompressor *decompressor, uint64_t uncompressedSize, Ownership own)
{
  m_Decompressor = decompressor;
  m_InputSize = uncompressedSize;

  m_BufferSize = initialBufferSize;
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  m_Ownership = own;

  ReadFromExternal(0, RDCMIN(uncompressedSize, m_BufferSize));
}

StreamReader::~StreamReader()
{
  for(StreamCloseCallback cb : m_Callbacks)
    cb();

  FreeAlignedBuffer(m_BufferBase);

  if(m_Ownership == Ownership::Stream)
  {
    if(m_File)
      FileIO::fclose(m_File);

    if(m_Decompressor)
      delete m_Decompressor;
  }
}

void StreamReader::SetOffset(uint64_t offs)
{
  if(m_File || m_Decompressor)
  {
    RDCERR("File and decompress stream readers do not support seeking");
    return;
  }

  m_BufferHead = m_BufferBase + offs;
}

bool StreamReader::Reserve(uint64_t numBytes)
{
  if(m_Sock)
  {
    // if we're reading more bytes than our buffer size, resize up
    if(numBytes > m_BufferSize)
    {
      FreeAlignedBuffer(m_BufferBase);
      m_InputSize = m_BufferSize = AlignUp<uint64_t>(numBytes, 256ULL);
      m_BufferBase = AllocAlignedBuffer(m_BufferSize);
      m_BufferHead = m_BufferBase + m_BufferSize;
    }

    m_ReadOffset += numBytes;

    // read into the end of the buffer, so that the subsequent read re-exhausts the buffer
    m_BufferHead = m_BufferBase + m_BufferSize - numBytes;
    return ReadFromExternal(m_BufferSize - numBytes, numBytes);
  }

  RDCASSERT(m_File || m_Decompressor);

  // store old buffer and the read data, so we can move it into the new buffer
  byte *oldBuffer = m_BufferBase;

  // always keep at least a certain window behind what we read.
  uint64_t backwardsWindow = RDCMIN<uint64_t>(64ULL, m_BufferHead - m_BufferBase);

  byte *currentData = m_BufferHead - backwardsWindow;
  uint64_t currentDataSize = m_BufferSize - (m_BufferHead - m_BufferBase) + backwardsWindow;

  uint64_t BufferOffset = m_BufferHead - m_BufferBase;

  // if we are reading more than our current buffer size, expand the buffer size
  if(numBytes + backwardsWindow > m_BufferSize)
  {
    // very conservative resizing - don't do "double and add" - to avoid
    // a 1GB buffer being read and needing to allocate 2GB. The cost is we
    // will reallocate a bit more often
    m_BufferSize = numBytes + backwardsWindow;
    m_BufferBase = AllocAlignedBuffer(m_BufferSize);
  }

  // move the unread data into the buffer
  memmove(m_BufferBase, currentData, (size_t)currentDataSize);

  if(BufferOffset > backwardsWindow)
  {
    m_ReadOffset += BufferOffset - backwardsWindow;
    m_BufferHead = m_BufferBase + backwardsWindow;
  }
  else
  {
    m_BufferHead = m_BufferBase + BufferOffset;
  }

  // if there's anything left of the file to read in, do so now
  bool ret = false;

  ret = ReadFromExternal(currentDataSize, RDCMIN(m_BufferSize - currentDataSize,
                                                 m_InputSize - m_ReadOffset - currentDataSize));

  if(oldBuffer != m_BufferBase && m_BufferBase)
    FreeAlignedBuffer(oldBuffer);

  return ret;
}

bool StreamReader::ReadFromExternal(uint64_t bufferOffs, uint64_t length)
{
  bool success = true;

  if(m_Decompressor)
  {
    success = m_Decompressor->Read(m_BufferBase + bufferOffs, length);
  }
  else if(m_File)
  {
    uint64_t numRead = FileIO::fread(m_BufferBase + bufferOffs, 1, (size_t)length, m_File);
    success = (numRead == length);
  }
  else if(m_Sock)
  {
    if(!m_Sock->Connected())
    {
      success = false;
    }
    else
    {
      success = m_Sock->RecvDataBlocking(m_BufferBase + bufferOffs, (uint32_t)length);
    }
  }
  else
  {
    // we're in an error-state, nothing to read from
    return false;
  }

  if(!success)
  {
    if(m_File)
      RDCERR("Error reading from file, errno %d", errno);
    else if(m_Sock)
      RDCWARN("Error reading from socket");

    m_HasError = true;

    // move to error state
    FreeAlignedBuffer(m_BufferBase);

    if(m_Ownership == Ownership::Stream)
    {
      if(m_File)
        FileIO::fclose(m_File);

      if(m_Sock)
        delete m_Sock;

      if(m_Decompressor)
        delete m_Decompressor;
    }

    m_File = NULL;
    m_Sock = NULL;
    m_Decompressor = NULL;
    m_ReadOffset = 0;
    m_InputSize = 0;

    m_BufferSize = 0;
    m_BufferHead = m_BufferBase = NULL;

    m_Ownership = Ownership::Nothing;
  }

  return success;
}

StreamWriter::StreamWriter(uint64_t initialBufSize)
{
  m_BufferBase = m_BufferHead = AllocAlignedBuffer(initialBufSize);
  m_BufferEnd = m_BufferBase + initialBufSize;

  m_Ownership = Ownership::Nothing;
}

StreamWriter::StreamWriter(StreamInvalidType)
{
  m_BufferBase = m_BufferHead = m_BufferEnd = NULL;

  m_Ownership = Ownership::Nothing;
  m_InMemory = false;
}

StreamWriter::StreamWriter(Network::Socket *sock, Ownership own)
{
  m_BufferBase = m_BufferHead = m_BufferEnd = NULL;

  m_Sock = sock;

  m_Ownership = own;
  m_InMemory = false;
}

StreamWriter::StreamWriter(FILE *file, Ownership own)
{
  m_BufferBase = m_BufferHead = m_BufferEnd = NULL;

  m_File = file;

  m_Ownership = own;
  m_InMemory = false;
}

StreamWriter::StreamWriter(Compressor *compressor, Ownership own)
{
  m_BufferBase = m_BufferHead = m_BufferEnd = NULL;

  m_Compressor = compressor;

  m_Ownership = own;
  m_InMemory = false;
}

StreamWriter::~StreamWriter()
{
  for(StreamCloseCallback cb : m_Callbacks)
    cb();

  FreeAlignedBuffer(m_BufferBase);

  if(m_Ownership == Ownership::Stream)
  {
    if(m_File)
      FileIO::fclose(m_File);

    if(m_Compressor)
      delete m_Compressor;
  }
}

void StreamWriter::HandleError()
{
  if(m_File)
    RDCERR("Error writing to file, errno %d", errno);
  else if(m_Sock)
    RDCWARN("Error writing to socket");

  m_HasError = true;

  FreeAlignedBuffer(m_BufferBase);

  if(m_Ownership == Ownership::Stream)
  {
    if(m_File)
      FileIO::fclose(m_File);

    if(m_Sock)
      delete m_Sock;

    if(m_Compressor)
      delete m_Compressor;
  }

  m_BufferBase = m_BufferHead = m_BufferEnd = NULL;

  m_WriteSize = 0;
  m_File = NULL;
  m_Sock = NULL;
  m_Compressor = NULL;

  m_Ownership = Ownership::Nothing;
  m_InMemory = false;
}

void StreamTransfer(StreamWriter *writer, StreamReader *reader, float *progress)
{
  uint64_t totalSize = reader->GetSize();

  // copy 1MB at a time
  const uint64_t StreamIOChunkSize = 1024 * 1024;

  const uint64_t bufSize = RDCMIN(StreamIOChunkSize, totalSize);
  uint64_t numBufs = totalSize / bufSize;
  // last remaining partial buffer
  if(totalSize % (uint64_t)bufSize > 0)
    numBufs++;

  byte *buf = new byte[(size_t)bufSize];

  if(progress)
    *progress = 0.0001f;

  for(uint64_t i = 0; i < numBufs; i++)
  {
    uint64_t payloadLength = RDCMIN(bufSize, totalSize);

    reader->Read(buf, payloadLength);
    writer->Write(buf, payloadLength);

    totalSize -= payloadLength;
    if(progress)
      *progress = float(i + 1) / float(numBufs);
  }

  delete[] buf;
}