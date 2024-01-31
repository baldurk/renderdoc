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

#include <stdio.h>
#include <functional>
#include "api/replay/replay_enums.h"
#include "common/common.h"
#include "common/formatting.h"
#include "common/threading.h"
#include "os/os_specific.h"

enum class Ownership
{
  Nothing,
  Stream,
};

class StreamWriter;
class StreamReader;

typedef std::function<void()> StreamCloseCallback;

class Compressor
{
public:
  Compressor(StreamWriter *write, Ownership own) : m_Write(write), m_Ownership(own) {}
  virtual ~Compressor();
  RDResult GetError() { return m_Error; }
  virtual bool Write(const void *data, uint64_t numBytes) = 0;
  virtual bool Finish() = 0;

protected:
  StreamWriter *m_Write;
  Ownership m_Ownership;
  RDResult m_Error;
};

class Decompressor
{
public:
  Decompressor(StreamReader *read, Ownership own) : m_Read(read), m_Ownership(own) {}
  virtual ~Decompressor();
  RDResult GetError() { return m_Error; }
  virtual bool Recompress(Compressor *comp) = 0;
  virtual bool Read(void *data, uint64_t numBytes) = 0;

protected:
  StreamReader *m_Read;
  Ownership m_Ownership;
  RDResult m_Error;
};

class StreamReader
{
public:
  enum StreamInvalidType
  {
    InvalidStream
  };
  enum StreamDummyType
  {
    DummyStream
  };

  StreamReader(StreamInvalidType, RDResult res);
  StreamReader(StreamDummyType);
  StreamReader(const byte *buffer, uint64_t bufferSize);
  StreamReader(const bytebuf &buffer);

  StreamReader(Network::Socket *sock, Ownership own);
  StreamReader(FILE *file, uint64_t fileSize, Ownership own);
  StreamReader(FILE *file);
  StreamReader(StreamReader *reader, uint64_t bufferSize);
  StreamReader(Decompressor *decompressor, uint64_t uncompressedSize, Ownership own);

  ~StreamReader();

  bool IsErrored() { return m_Error != ResultCode::Succeeded; }
  RDResult GetError() { return m_Error; }
  void SetError(RDResult res)
  {
    if(m_Error == ResultCode::Succeeded && res != ResultCode::Succeeded)
      m_Error = res;
  }
  void SetOffset(uint64_t offs);

  inline uint64_t GetOffset() { return m_BufferHead - m_BufferBase + m_ReadOffset; }
  inline uint64_t GetSize() { return m_InputSize; }
  inline bool AtEnd()
  {
    if(m_Dummy)
      return false;
    if(m_Sock)
      return Available() == 0;
    return GetOffset() >= GetSize();
  }
  template <uint64_t alignment>
  bool AlignTo()
  {
    uint64_t offs = GetOffset();
    uint64_t alignedOffs = AlignUp(offs, alignment);

    uint64_t bytesToAlign = alignedOffs - offs;

    if(bytesToAlign > 0)
      return Read(NULL, bytesToAlign);

    return true;
  }

  void Clear(void *data, size_t numBytes)
  {
    if(!m_Dummy)
      memset(data, 0, numBytes);
  }

  bool Read(void *data, uint64_t numBytes)
  {
    if(numBytes == 0 || m_Dummy)
      return true;

    if(!m_BufferBase || IsErrored())
    {
      // read 0s if we're in an error state
      if(data)
        memset(data, 0, (size_t)numBytes);

      return false;
    }

    // if we're reading past the end, error, read nothing (no partial reads) and return
    if(m_Sock == NULL && GetOffset() + numBytes > GetSize())
    {
      m_BufferHead = m_BufferBase + m_BufferSize;
      if(data)
        memset(data, 0, (size_t)numBytes);
      SET_ERROR_RESULT(m_Error, ResultCode::FileIOFailed, "Reading off the end of data stream");
      return false;
    }

    // if we're reading from an external source, reserve enough bytes to do the read
    if(m_File || m_Sock || m_Decompressor)
    {
      // This preserves everything from min(m_BufferBase, m_BufferHead - 64) -> end of buffer
      // which will still be in place relative to m_BufferHead.
      // In other words - reservation will keep the already-read data that's after the head pointer,
      // as well as up to 64 bytes *behind* the head if it exists.
      if(numBytes > Available())
      {
        bool success = false;
        bool alreadyread = false;

        // if we're reading 10MB or more then read directly into the output memory rather than
        // resizing up, reading all of that, then memcpy'ing out of our window.
        // To simplify the implementation of ReadLargeBuffer if we can *almost* satisfy this with
        // what we have without leaving 128 bytes left over, we go through the normal path.
        // This does mean that you could do incrementally larger reads and get the window larger
        // and larger by just skating over the limit each time, but that's fine because the main
        // case we want to catch is a window that's only a few MB and then suddenly we read 100s of
        // MB.
        // We don't do this on sockets since we want to opportunistically read more into the window
        // to batch lots of small reads together.
        if(m_Sock == NULL && numBytes >= 10 * 1024 * 1024 && Available() + 128 < numBytes)
        {
          success = ReadLargeBuffer(data, numBytes);
          alreadyread = true;
        }
        else
        {
          success = Reserve(numBytes);
        }

        if(!success)
        {
          if(data)
            memset(data, 0, (size_t)numBytes);
          return false;
        }

        if(alreadyread)
          return true;
      }
    }

    // perform the actual copy
    if(data)
      memcpy(data, m_BufferHead, (size_t)numBytes);

    // advance head
    m_BufferHead += numBytes;

    return true;
  }

  bool SkipBytes(uint64_t numBytes)
  {
    // fast path for file skipping
    if(m_File && numBytes > Available())
    {
      // first, completely exhaust the buffer
      numBytes -= Available();
      Read(NULL, Available());

      // then just seek for the rest
      FileIO::fseek64(m_File, numBytes, SEEK_CUR);
      m_ReadOffset += numBytes;

      // the next read will re-fill the buffer, just the same as if we'd done a perfectly sized read
      return true;
    }

    return Read(NULL, numBytes);
  }

  // compile-time constant element to let the compiler inline the memcpy
  template <typename T>
  bool Read(T &data)
  {
    return Read(&data, sizeof(T));
  }

  void AddCloseCallback(StreamCloseCallback callback) { m_Callbacks.push_back(callback); }
private:
  inline uint64_t Available()
  {
    if(m_Sock)
      return m_InputSize - (m_BufferHead - m_BufferBase);
    return m_BufferSize - (m_BufferHead - m_BufferBase);
  }
  bool Reserve(uint64_t numBytes);
  bool ReadLargeBuffer(void *buffer, uint64_t length);
  bool ReadFromExternal(void *buffer, uint64_t length);

  // base of the buffer allocation
  byte *m_BufferBase;

  // where we are currently reading from in the buffer
  byte *m_BufferHead;

  // the size of the buffer (just a window if reading from external source)
  uint64_t m_BufferSize;

  // the total size of the total input. This is how many bytes you can read, regardless
  // of how many bytes might actually be stored on the other side of the source (i.e.
  // this is the uncompressed output size)
  uint64_t m_InputSize;

  // file pointer, if we're reading from a file
  FILE *m_File = NULL;

  // socket, if we're reading from a socket
  Network::Socket *m_Sock = NULL;

  // the decompressor, if reading from it
  Decompressor *m_Decompressor = NULL;

  // the offset in the file/decompressor that corresponds to the start of m_BufferBase
  uint64_t m_ReadOffset = 0;

  // result indicating if an error has been encountered and the stream is now invalid, with details
  // of what happened
  RDResult m_Error;

  // flag indicating this reader is a dummy and doesn't read anything or clear inputs. Used with a
  // structured serialiser to 'read' pre-existing data.
  bool m_Dummy = false;

  // do we own the file/compressor? are we responsible for
  // cleaning it up?
  Ownership m_Ownership;

  // callbacks that will be invoked when this stream is being destroyed
  rdcarray<StreamCloseCallback> m_Callbacks;
};

class FileWriter
{
public:
  static FileWriter *MakeDefault(FILE *file, Ownership own);
  static FileWriter *MakeThreaded(FILE *file, Ownership own);

  ~FileWriter();

  RDResult Write(const void *data, uint64_t length);
  RDResult Flush();

private:
  FileWriter(FILE *file, Ownership own) : m_File(file), m_Ownership(own) {}
  RDResult WriteThreaded(const void *data, uint64_t length);
  RDResult WriteUnthreaded(const void *data, uint64_t length);
  void ThreadEntry();

  FILE *m_File;

  // do we own the file/compressor? are we responsible for
  // cleaning it up?
  Ownership m_Ownership;

  static const uint64_t BlockSize = 4 * 1024 * 1024;
  static const uint64_t NumBlocks = 8;

  // <base_pointer, byte_offset/size>
  using Block = rdcpair<byte *, uint64_t>;

  int32_t m_ThreadRunning = 0;
  int32_t m_ThreadKill = 0;
  Threading::ThreadHandle m_Thread = 0;

  // only touched by the producer, set of blocks allocated for easy cleanup. These blocks are in at
  // most one of the arrays below
  Block m_AllocBlocks[NumBlocks] = {};

  // list of blocks the producer owns. The last in this list is the one we're writing to currently
  rdcarray<Block> m_ProducerOwned;
  // list of blocks the consumer owns. This list is being churned through on the work thread
  rdcarray<Block> m_ConsumerOwned;

  // the lock protects everything below
  Threading::SpinLock m_Lock;
  // work to be pushed onto m_ConsumerOwned from the producer
  rdcarray<Block> m_PendingForConsumer;
  // blocks that can be pulled into m_ProducerOwned by the producer
  rdcarray<Block> m_CompletedFromConsumer;
  // any error that has appeared
  RDResult m_Error;
};

class StreamWriter
{
public:
  enum StreamInvalidType
  {
    InvalidStream
  };

  StreamWriter(StreamInvalidType, RDResult res);
  StreamWriter(uint64_t initialBufSize);
  StreamWriter(FileWriter *file, Ownership own);
  // when given a FILE* make a default filewriter and own it ourselves, but the filewriter uses the
  // ownership of the FILE that was specified
  StreamWriter(FILE *file, Ownership own)
      : StreamWriter(FileWriter::MakeDefault(file, own), Ownership::Stream)
  {
  }
  StreamWriter(Network::Socket *sock, Ownership own);
  StreamWriter(Compressor *compressor, Ownership own);

  bool IsErrored() { return m_Error != ResultCode::Succeeded; }
  RDResult GetError() { return m_Error; }
  void SetError(RDResult res)
  {
    if(m_Error == ResultCode::Succeeded && res != ResultCode::Succeeded)
      m_Error = res;
  }
  static const int DefaultScratchSize = 32 * 1024;

  ~StreamWriter();

  void Rewind()
  {
    if(m_InMemory)
    {
      m_BufferHead = m_BufferBase;
      m_WriteSize = 0;
      return;
    }

    RDCERR("Can't rewind a file/compressor stream writer");
  }

  uint64_t GetOffset() { return m_WriteSize; }
  const byte *GetData() { return m_BufferBase; }
  byte *StealDataAndRewind()
  {
    byte *ret = m_BufferBase;

    const uint64_t bufferSize = m_BufferEnd - m_BufferBase;
    m_BufferBase = m_BufferHead = AllocAlignedBuffer(bufferSize);
    m_BufferEnd = m_BufferBase + bufferSize;
    m_WriteSize = 0;

    return ret;
  }
  template <uint64_t alignment>
  bool AlignTo()
  {
    uint64_t offs;
    if(m_InMemory)
      offs = m_BufferHead - m_BufferBase;
    else
      offs = GetOffset();

    uint64_t alignedOffs = AlignUp(offs, alignment);

    uint64_t bytesToAlign = alignedOffs - offs;

    RDCCOMPILE_ASSERT(alignment <= sizeof(empty),
                      "Empty array is not large enough - increase size to support alignment");

    if(bytesToAlign > 0)
      return Write(empty, bytesToAlign);

    return true;
  }

  bool Write(const void *data, uint64_t numBytes)
  {
    if(numBytes == 0)
      return true;

    m_WriteSize += numBytes;

    if(m_InMemory)
    {
      // in-memory path

      // are we about to write outside the buffer? Resize it larger
      if(m_BufferHead + numBytes >= m_BufferEnd)
        EnsureSized(numBytes);

      // perform the actual copy
      memcpy(m_BufferHead, data, (size_t)numBytes);

      // advance head
      m_BufferHead += numBytes;

      return true;
    }
    else if(m_Compressor)
    {
      return m_Compressor->Write(data, numBytes);
    }
    else if(m_File)
    {
      RDResult result = m_File->Write(data, numBytes);

      if(result == ResultCode::Succeeded)
        return true;

      HandleError(result);
      return false;
    }
    else if(m_Sock)
    {
      return SendSocketData(data, numBytes);
    }
    else
    {
      // we're in an error-state, nothing to write to
      return false;
    }
  }

  // compile-time constant amount of data to let the compiler inline the memcpy
  template <typename T>
  bool Write(const T &data)
  {
    const uint64_t numBytes = sizeof(T);

    if(m_InMemory)
    {
      // we duplicate the implementation here instead of calling the Write(void *, size_t)
      // overload above since then the compiler may not be able to optimise out the memcpy
      m_WriteSize += numBytes;

      // are we about to write outside the buffer? Resize it larger
      if(m_BufferHead + numBytes >= m_BufferEnd)
        EnsureSized(numBytes);

      // perform the actual copy
      memcpy(m_BufferHead, &data, (size_t)numBytes);

      // advance head
      m_BufferHead += numBytes;

      return true;
    }
    else
    {
      return Write(&data, numBytes);
    }
  }

  // write a particular value at an offset (not necessarily just append).
  template <typename T>
  bool WriteAt(uint64_t offs, const T &data)
  {
    if(!m_File && !m_Sock && !m_Compressor)
    {
      RDCASSERT(ptrdiff_t(offs + sizeof(data)) <= m_BufferHead - m_BufferBase);
      byte *oldHead = m_BufferHead;
      uint64_t oldWriteSize = m_WriteSize;

      m_BufferHead = m_BufferBase + offs;
      bool ret = Write(data);

      m_WriteSize = oldWriteSize;
      m_BufferHead = oldHead;
      return ret;
    }

    RDResult result;
    SET_ERROR_RESULT(result, ResultCode::InternalError,
                     "Can't seek a file/socket/compressor stream writer");
    HandleError(result);

    return false;
  }

  bool Flush()
  {
    if(m_Compressor)
    {
      return true;
    }
    else if(m_File)
    {
      RDResult result = m_File->Flush();

      if(result == ResultCode::Succeeded)
        return true;

      HandleError(result);

      return false;
    }
    else if(m_Sock)
    {
      return FlushSocketData();
    }

    return true;
  }

  bool Finish()
  {
    if(m_Compressor)
    {
      return m_Compressor->Finish();
    }
    else if(m_File)
    {
      RDResult result = m_File->Flush();

      if(result == ResultCode::Succeeded)
        return true;

      HandleError(result);

      return false;
    }
    else if(m_Sock)
    {
      return true;
    }

    return true;
  }

  void AddCloseCallback(StreamCloseCallback callback) { m_Callbacks.push_back(callback); }
private:
  inline void EnsureSized(const uint64_t numBytes)
  {
    uint64_t bufferSize = m_BufferEnd - m_BufferBase;
    const uint64_t newSize = (m_BufferHead - m_BufferBase) + numBytes;

    if(bufferSize < newSize)
    {
      // reallocate to a conservative size, don't 'double and allocate'
      while(bufferSize < newSize)
        bufferSize += 128 * 1024;

      byte *newBuf = AllocAlignedBuffer(bufferSize);

      uint64_t curUsed = m_BufferHead - m_BufferBase;

      memcpy(newBuf, m_BufferBase, (size_t)curUsed);

      FreeAlignedBuffer(m_BufferBase);

      m_BufferBase = newBuf;
      m_BufferHead = newBuf + curUsed;
      m_BufferEnd = m_BufferBase + bufferSize;
    }
  }

  void HandleError(RDResult result);

  bool SendSocketData(const void *data, uint64_t numBytes);
  bool FlushSocketData();

  // used for aligned writes
  static const byte empty[128];

  // base of the buffer allocation if we're writing to a buffer
  byte *m_BufferBase;

  // where we are currently writing to in the buffer
  byte *m_BufferHead;

  // the end of the buffer
  byte *m_BufferEnd;

  // the total size of the file/compressor (ie. how much data flushed through it)
  uint64_t m_WriteSize = 0;

  // file writer, if we're writing to a file
  FileWriter *m_File = NULL;

  // the compressor, if writing to it
  Compressor *m_Compressor = NULL;

  // the socket, if writing to it
  Network::Socket *m_Sock = NULL;

  // true if we're not writing to file/compressor, used to optimise checks in Write
  bool m_InMemory = true;

  // result indicating if an error has been encountered and the stream is now invalid, with details
  // of what happened
  RDResult m_Error;

  // do we own the file/compressor? are we responsible for
  // cleaning it up?
  Ownership m_Ownership;

  // callbacks that will be invoked when this stream is being destroyed
  rdcarray<StreamCloseCallback> m_Callbacks;
};

void StreamTransfer(StreamWriter *writer, StreamReader *reader, RENDERDOC_ProgressCallback progress);
