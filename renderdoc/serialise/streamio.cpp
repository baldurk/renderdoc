/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include "api/replay/stringise.h"
#include "common/timing.h"

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

StreamReader::StreamReader(const bytebuf &buffer)
{
  m_InputSize = m_BufferSize = buffer.size();
  m_BufferHead = m_BufferBase = AllocAlignedBuffer(m_BufferSize);

  memcpy(m_BufferHead, buffer.data(), (size_t)m_BufferSize);

  m_Ownership = Ownership::Nothing;
}

StreamReader::StreamReader(StreamInvalidType, RDResult res)
{
  m_InputSize = 0;

  m_BufferSize = 0;
  m_BufferHead = m_BufferBase = NULL;

  m_Ownership = Ownership::Nothing;

  m_Error = res;
  if(m_Error == ResultCode::Succeeded)
    SET_ERROR_RESULT(m_Error, ResultCode::InvalidParameter,
                     "Invalid stream created with no error code");
}

StreamReader::StreamReader(StreamDummyType)
{
  m_InputSize = 0;

  m_BufferSize = 0;
  m_BufferHead = m_BufferBase = NULL;

  m_Ownership = Ownership::Nothing;

  m_Dummy = true;
}

StreamReader::StreamReader(Network::Socket *sock, Ownership own)
{
  m_Sock = sock;

  m_BufferSize = initialBufferSize;
  m_BufferBase = AllocAlignedBuffer(m_BufferSize);
  m_BufferHead = m_BufferBase;

  // for sockets we use m_InputSize to indicate how much data has been read into the buffer.
  m_InputSize = 0;

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

  ReadFromExternal(m_BufferBase, RDCMIN(m_InputSize, m_BufferSize));

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

  ReadFromExternal(m_BufferBase, RDCMIN(m_InputSize, m_BufferSize));

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

  ReadFromExternal(m_BufferBase, RDCMIN(uncompressedSize, m_BufferSize));
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
  RDCASSERT(m_Sock || m_File || m_Decompressor);

  // store old buffer and the read data, so we can move it into the new buffer
  byte *oldBuffer = m_BufferBase;

  // always keep at least a certain window behind what we read.
  uint64_t backwardsWindow = RDCMIN<uint64_t>(64ULL, m_BufferHead - m_BufferBase);

  byte *currentData = m_BufferHead - backwardsWindow;
  uint64_t currentDataSize = m_BufferSize - (m_BufferHead - m_BufferBase) + backwardsWindow;

  if(m_Sock)
    currentDataSize = m_InputSize - (m_BufferHead - m_BufferBase) + backwardsWindow;

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

  if(m_Sock)
    m_InputSize = currentDataSize;

  // if there's anything left of the file to read in, do so now
  bool ret = false;

  uint64_t readSize =
      RDCMIN(m_BufferSize - currentDataSize, m_InputSize - m_ReadOffset - currentDataSize);

  // we'll read as much as possible anyway using m_BufferSize, but we need to know how much we
  // *must* read.
  if(m_Sock)
    readSize = numBytes - Available();

  ret = ReadFromExternal(m_BufferBase + currentDataSize, readSize);

  if(oldBuffer != m_BufferBase && m_BufferBase)
    FreeAlignedBuffer(oldBuffer);

  return ret;
}

bool StreamReader::ReadLargeBuffer(void *buffer, uint64_t length)
{
  RDCASSERT(m_Sock || m_File || m_Decompressor);

  byte *dest = (byte *)buffer;

  // first exhaust whatever we have in the current buffer.
  {
    const uint64_t avail = Available();

    // if we don't have 128 bytes left over we shouldn't be in here
    RDCASSERT(avail + 128 <= length, avail, length);

    // don't actually read if the destination buffer is NULL
    if(dest)
    {
      memcpy(dest, m_BufferHead, (size_t)avail);
      dest += avail;
    }
    length -= avail;
    m_ReadOffset += m_BufferSize;
  }

  // now read everything but the last 128 bytes directly from the external source
  if(length > 128)
  {
    uint64_t directReadLength = length - 128;

    length -= directReadLength;
    m_ReadOffset += directReadLength;

    if(buffer)
    {
      bool ret = ReadFromExternal(dest, directReadLength);

      dest += directReadLength;

      // if we failed, return now
      if(!ret)
        return ret;
    }
    else
    {
      // if we have no buffer to read into, just seek the stream in buffer-sized chunks using the
      // existing buffer. Ensure the buffer is big enough to do this at a reasonable rate.
      if(m_BufferSize < 1024 * 1024)
      {
        m_BufferSize = 1024 * 1024;
        FreeAlignedBuffer(m_BufferBase);
        m_BufferBase = AllocAlignedBuffer(m_BufferSize);
      }

      while(directReadLength > 0)
      {
        uint64_t chunkRead = RDCMIN(m_BufferSize, directReadLength);

        bool ret = ReadFromExternal(m_BufferBase, chunkRead);

        // if we failed, return now
        if(!ret)
          return ret;

        directReadLength -= chunkRead;
      }
    }
  }

  // we now have exactly 128 bytes to read, guaranteed by how the function is called.
  // we read that into the end of our buffer deliberately so that we can leave the buffer in the
  // right state to have a backwards window (though it shouldn't be needed for large serialises like
  // this).
  if(m_BufferSize < 128)
  {
    m_BufferSize = 128;
    FreeAlignedBuffer(m_BufferBase);
    m_BufferBase = AllocAlignedBuffer(m_BufferSize);
  }

  // set the head to *after* where we're reading, this is where it'll end up after the read.
  m_BufferHead = m_BufferBase + m_BufferSize;

  // read the 128 bytes
  m_ReadOffset += 128;
  bool ret = ReadFromExternal(m_BufferHead - 128, 128);

  // memcpy it where it's needed
  if(dest && ret)
    memcpy(dest, m_BufferHead - 128, 128);

  // adjust read offset back for the 'fake' buffer we leave behind
  m_ReadOffset -= m_BufferSize;

  return ret;
}

bool StreamReader::ReadFromExternal(void *buffer, uint64_t length)
{
  bool success = true;

  if(m_Decompressor)
  {
    success = m_Decompressor->Read(buffer, length);

    if(!success)
      m_Error = m_Decompressor->GetError();
  }
  else if(m_File)
  {
    uint64_t numRead = FileIO::fread(buffer, 1, (size_t)length, m_File);
    success = (numRead == length);

    if(!success)
      SET_ERROR_RESULT(m_Error, ResultCode::FileIOFailed, "Error reading from file: %s",
                       FileIO::ErrorString().c_str());
  }
  else if(m_Sock)
  {
    if(!m_Sock->Connected())
    {
      success = false;
      m_Error = m_Sock->GetError();
      if(m_Error == ResultCode::Succeeded)
        SET_ERROR_RESULT(m_Error, ResultCode::NetworkIOFailed,
                         "Socket unexpectedly disconnected during reading");
    }
    else
    {
      // first get the required data blocking (this will sleep the thread until it comes in).
      byte *readDest = (byte *)buffer;

      success = m_Sock->RecvDataBlocking(readDest, (uint32_t)length);

      if(success)
      {
        m_InputSize += length;
        readDest += length;

        uint32_t bufSize = uint32_t(m_BufferSize - m_InputSize);

        // now read more, as much as possible, to try and batch future reads
        success = m_Sock->RecvDataNonBlocking(readDest, bufSize);

        if(success)
          m_InputSize += bufSize;
      }

      if(!success)
      {
        m_Error = m_Sock->GetError();
        if(m_Error == ResultCode::Succeeded)
          SET_WARNING_RESULT(m_Error, ResultCode::NetworkIOFailed,
                             "Socket unexpectedly disconnected during reading");
      }
    }
  }
  else
  {
    // we're in an error-state, nothing to read from
    return false;
  }

  if(!success)
  {
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

StreamWriter::StreamWriter(StreamInvalidType, RDResult res)
{
  m_BufferBase = m_BufferHead = m_BufferEnd = NULL;

  m_Ownership = Ownership::Nothing;
  m_InMemory = false;

  m_Error = res;
  if(m_Error == ResultCode::Succeeded)
    SET_ERROR_RESULT(m_Error, ResultCode::InvalidParameter,
                     "Invalid stream created with no error code");
}

StreamWriter::StreamWriter(Network::Socket *sock, Ownership own)
{
  m_BufferBase = m_BufferHead = AllocAlignedBuffer(initialBufferSize);
  m_BufferEnd = m_BufferBase + initialBufferSize;

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

bool StreamWriter::SendSocketData(const void *data, uint64_t numBytes)
{
  // try to coalesce small writes without doing blocking sends, at least until we're flushed.
  // if the buffer is already full, flush it.
  if(m_BufferHead + numBytes >= m_BufferEnd)
  {
    bool success = FlushSocketData();
    if(!success)
      return false;
  }

  // if it's larger than our buffer (even after flushing) just write directly
  if(m_BufferHead + numBytes >= m_BufferEnd)
  {
    bool success = m_Sock->SendDataBlocking(data, (uint32_t)numBytes);
    if(!success)
    {
      RDResult res = m_Sock->GetError();
      if(res == ResultCode::Succeeded)
        SET_ERROR_RESULT(res, ResultCode::NetworkIOFailed,
                         "Socket unexpectedly disconnected during sending");
      HandleError(res);
      return false;
    }
  }
  else
  {
    // otherwise, write it into the in-memory buffer
    memcpy(m_BufferHead, data, (size_t)numBytes);
    m_BufferHead += numBytes;
  }

  return true;
}

bool StreamWriter::FlushSocketData()
{
  // send out what we have buffered up
  bool success = m_Sock->SendDataBlocking(m_BufferBase, uint32_t(m_BufferHead - m_BufferBase));
  if(!success)
  {
    RDResult res = m_Sock->GetError();
    if(res == ResultCode::Succeeded)
      SET_ERROR_RESULT(res, ResultCode::NetworkIOFailed,
                       "Socket unexpectedly disconnected during sending");
    HandleError(res);
    return false;
  }

  // reset buffer to the start
  m_BufferHead = m_BufferBase;

  return true;
}

void StreamWriter::HandleError(RDResult result)
{
  if(m_Error == ResultCode::Succeeded)
    m_Error = result;

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

void StreamTransfer(StreamWriter *writer, StreamReader *reader, RENDERDOC_ProgressCallback progress)
{
  uint64_t totalSize = reader->GetSize();

  if(totalSize == 0)
  {
    if(progress)
      progress(1.0f);
    return;
  }

  // copy 1MB at a time
  const uint64_t StreamIOChunkSize = 1024 * 1024;

  const uint64_t bufSize = RDCMIN(StreamIOChunkSize, totalSize);
  uint64_t numBufs = totalSize / bufSize;
  // last remaining partial buffer
  if(totalSize % (uint64_t)bufSize > 0)
    numBufs++;

  byte *buf = new byte[(size_t)bufSize];

  if(progress)
    progress(0.0001f);

  for(uint64_t i = 0; i < numBufs; i++)
  {
    uint64_t payloadLength = RDCMIN(bufSize, totalSize);

    reader->Read(buf, payloadLength);
    writer->Write(buf, payloadLength);

    totalSize -= payloadLength;
    if(progress)
      progress(float(i + 1) / float(numBufs));
  }

  if(progress)
    progress(1.0f);

  delete[] buf;
}
