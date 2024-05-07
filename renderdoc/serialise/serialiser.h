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

#include <set>
#include "api/replay/replay_enums.h"
#include "api/replay/structured_data.h"
#include "common/formatting.h"
#include "common/result.h"
#include "streamio.h"

// function to deallocate anything from a serialise. Default impl
// does no deallocation of anything.
template <class T>
void Deserialise(const T &el)
{
}

#define DECLARE_DESERIALISE_TYPE(type) \
  template <>                          \
  void Deserialise(const type &el);

// this is a bit of a hack, but necessary. We don't want to have all of our DoSerialise functions
// defined in global cross-project headers and compiled everywhere, we just want to define them in
// .cpp files.
// However because they are template specializations, so they need to be explicitly instantiated in
// one file to link elsewhere.
#define INSTANTIATE_SERIALISE_TYPE(type)                                    \
  template void DoSerialise(Serialiser<SerialiserMode::Writing> &, type &); \
  template void DoSerialise(Serialiser<SerialiserMode::Reading> &, type &);

typedef rdcstr (*ChunkLookup)(uint32_t chunkType);

enum class SerialiserFlags
{
  NoFlags = 0x0,
  AllocateMemory = 0x1,
};

BITMASK_OPERATORS(SerialiserFlags);

// This class is used to read and write arbitrary structured data from a stream. The primary
// mechanism is in template overloads of DoSerialise functions for each struct that can be
// serialised, down to primitive types (ints, floats, strings, etc).
//
// A serialised stream is defined in terms of 'chunks', each with an ID and some metadata attached
// like timestamp, duration, callstack, etc. Each chunk can then have objects serialised into it
// which automatically recurse and serialise all their members. When reading from/writing to a
// stream then the binary form is purely data - no structure encoded. When reading, the data can be
// optionally pulled out into a structured form that can then be manipulated or exported.
//
// Since a stream can be an in-memory buffer, a file, or a network socket this class is used to
// serialised complex data anywhere that we need structured I/O.

enum class SerialiserMode
{
  Writing,
  Reading,
};

struct CompressedFileIO;

template <SerialiserMode sertype>
class Serialiser
{
public:
  static constexpr bool IsReading() { return sertype != SerialiserMode::Writing; }
  static constexpr bool IsWriting() { return sertype == SerialiserMode::Writing; }
  bool IsStructurising() const { return m_Structuriser; }
  bool ExportStructure() const
  {
    // in debug builds, allow structured export during write for debugging. In release, only allow
    // it on read to compile out the extra code on the writing path
    return
#if ENABLED(RDOC_RELEASE)
        sertype == SerialiserMode::Reading &&
#endif
        m_ExportStructured && m_InternalElement == 0;
  }

  enum ChunkFlags
  {
    ChunkIndexMask = 0x0000ffff,
    ChunkCallstack = 0x00010000,
    ChunkThreadID = 0x00020000,
    ChunkDuration = 0x00040000,
    ChunkTimestamp = 0x00080000,
    Chunk64BitSize = 0x00100000,
  };

  //////////////////////////////////////////
  // Init and error handling
  ~Serialiser();

  // no copies
  Serialiser(const Serialiser &other) = delete;

  bool IsErrored() { return IsReading() ? m_Read->IsErrored() : m_Write->IsErrored(); }
  RDResult GetError() { return IsReading() ? m_Read->GetError() : m_Write->GetError(); }
  void SetError(RDResult result)
  {
    IsReading() ? m_Read->SetError(result) : m_Write->SetError(result);
  }
  StreamWriter *GetWriter() { return m_Write; }
  StreamReader *GetReader() { return m_Read; }
  uint32_t GetChunkMetadataRecording() { return m_ChunkFlags; }
  void SetChunkMetadataRecording(uint32_t flags);
  void SetChunkTimestampBasis(uint64_t base, double freq)
  {
    m_TimerBase = base;
    m_TimerFrequency = freq;
  }

  // debug-only option to dump out (roughly) the data going through the serialiser as it happens
  void EnableDumping(FileIO::LogFileHandle *debugLog) { m_DebugDumpLog = debugLog; }
  SDChunkMetaData &ChunkMetadata() { return m_ChunkMetadata; }
  //////////////////////////////////////////
  // Utility functions

  static uint64_t GetChunkAlignment() { return ChunkAlignment; }
  void *GetUserData() { return m_pUserData; }
  void SetUserData(void *userData) { m_pUserData = userData; }
  void SetStringDatabase(std::set<rdcstr> *db) { m_ExtStringDB = db; }
  // jumps to the byte after the current chunk, can be called any time after BeginChunk
  void SkipCurrentChunk();

  //////////////////////////////////////////
  // Version checking

  void SetVersion(uint64_t version) { m_Version = version; }
  // assume that we always write the latest version, so on writing the version check always passes.
  bool VersionAtLeast(uint64_t req) const { return IsWriting() || m_Version >= req; }
  bool VersionLess(uint64_t req) const { return IsReading() && m_Version < req; }
  // enable 'streaming mode' for ephemeral transfers like temporary I/O over sockets, where there's
  // no need for the chunk length - avoids needing to seek internally in a stream that might not
  // support seeking to fixup lengths, while also not requiring conservative length estimates
  // up-front
  void SetStreamingMode(bool stream) { m_DataStreaming = stream; }
  SDFile &GetStructuredFile() { return *m_StructuredFile; }
  void WriteStructuredFile(const SDFile &file, RENDERDOC_ProgressCallback progress);
  void SetActionChunk() { m_ActionChunk = true; }
  // the struct argument allows nested structs to pass a bit of data so a child struct can have
  // context from a parent struct if needed to serialise properly. Rarely used, primarily to be able
  // to flag if some context-sensitive members might be invalid
  void SetStructArg(uint64_t arg) { m_StructArg = arg; }
  uint64_t GetStructArg() { return m_StructArg; }
  //////////////////////////////////////////
  // Public serialisation interface

  void ConfigureStructuredExport(ChunkLookup lookup, bool includeBuffers, uint64_t timeBase,
                                 double timeFreq)
  {
    m_ChunkLookup = lookup;
    m_ExportBuffers = includeBuffers;
    m_ExportStructured = (lookup != NULL);
    m_TimerBase = timeBase;
    m_TimerFrequency = timeFreq;
  }

  uint32_t BeginChunk(uint32_t chunkID, uint64_t byteLength);
  void EndChunk();

  rdcstr GetCurChunkName()
  {
    if(m_ChunkLookup)
      return m_ChunkLookup(m_ChunkMetadata.chunkID);

    return StringFormat::Fmt("<No Chunk Lookup: %u>", m_ChunkMetadata.chunkID);
  }
  ChunkLookup GetChunkLookup() { return m_ChunkLookup; }
  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////
  // Templated serialisation functions

  // Users of the serialisation call Serialise() on the objects they want
  // to serialise

  // for structure type members or external use, main entry point

  template <typename T>
  void SerialiseStringify(const T el)
  {
    if(ExportStructure())
    {
      m_StructureStack.back()->data.str = ToStr(el);
      m_StructureStack.back()->type.flags |= SDTypeFlags::HasCustomString;
    }
  }

  template <class T>
  void ClearObj(T &el)
  {
    if(IsReading())
      m_Read->Clear(&el, sizeof(T));
  }

  // serialise an object (either loose, or a structure member).
  template <class T>
  Serialiser &Serialise(const rdcliteral &name, T &el,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &current = *m_StructureStack.back();

      SDObject &obj = *current.AddAndOwnChild(new SDObject(name, TypeName<T>()));
      m_StructureStack.push_back(&obj);

      obj.type.byteSize = sizeof(T);
      if(std::is_union<T>::value)
        obj.type.flags |= SDTypeFlags::Union;
    }

    SerialiseDispatch<Serialiser, T>::Do(*this, el);

    if(ExportStructure())
      m_StructureStack.pop_back();

    return *this;
  }

  // special function for serialising buffers
  Serialiser &Serialise(const rdcliteral &name, byte *&el, uint64_t byteSize,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    // silently handle NULL buffers
    if(IsWriting() && el == NULL)
      byteSize = 0;

    {
      m_InternalElement++;
      DoSerialise(*this, byteSize);
      m_InternalElement--;
    }

    if(IsReading())
    {
      VerifyArraySize(byteSize);
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &current = *m_StructureStack.back();

      SDObject &obj = *current.AddAndOwnChild(new SDObject(name, "Byte Buffer"_lit));
      m_StructureStack.push_back(&obj);

      obj.type.basetype = SDBasic::Buffer;
      obj.type.byteSize = byteSize;
    }

    byte *tempAlloc = NULL;

    {
      if(IsWriting())
      {
        // ensure byte alignment
        m_Write->AlignTo<ChunkAlignment>();

        if(el)
          m_Write->Write(el, byteSize);
        else
          RDCASSERT(byteSize == 0);
      }
      else if(IsReading())
      {
        // ensure byte alignment
        m_Read->AlignTo<ChunkAlignment>();

// Coverity is unable to tie this allocation together with the automatic scoped deallocation in the
// ScopedDeseralise* classes. We can verify with e.g. valgrind that there are no leaks, so to keep
// the analysis non-spammy we just don't allocate for coverity builds
#if !defined(__COVERITY__)
        if(!m_Structuriser && (flags & SerialiserFlags::AllocateMemory))
        {
          if(byteSize > 0)
            el = AllocAlignedBuffer(byteSize);
          else
            el = NULL;
        }

        // if we're exporting the buffers, make sure to always alloc space to read the data, so we
        // can save it out, even if the external code has no use for it and has asked for no
        // allocation.
        if(el == NULL && ExportStructure() && m_ExportBuffers)
        {
          if(byteSize > 0)
            el = tempAlloc = AllocAlignedBuffer(byteSize);
          else
            el = NULL;
        }
#endif

        m_Read->Read(el, byteSize);
      }
    }

    if(ExportStructure())
    {
      if(m_ExportBuffers)
      {
        SDObject &obj = *m_StructureStack.back();

        obj.data.basic.u = m_StructuredFile->buffers.size();

        bytebuf *alloc = new bytebuf;
        alloc->resize((size_t)byteSize);
        if(el)
          memcpy(alloc->data(), el, (size_t)byteSize);

        m_StructuredFile->buffers.push_back(alloc);
      }

      m_StructureStack.pop_back();
    }

#if !defined(__COVERITY__)
    if(tempAlloc)
    {
      FreeAlignedBuffer(tempAlloc);
      el = NULL;
    }
#endif

    return *this;
  }

  Serialiser &Serialise(const rdcliteral &name, bytebuf &el,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    uint64_t count = (uint64_t)el.size();

    {
      m_InternalElement++;
      DoSerialise(*this, count);
      m_InternalElement--;
    }

    if(IsReading())
    {
      VerifyArraySize(count);
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &current = *m_StructureStack.back();

      SDObject &obj = *current.AddAndOwnChild(new SDObject(name, "Byte Buffer"_lit));
      m_StructureStack.push_back(&obj);

      obj.type.basetype = SDBasic::Buffer;
      obj.type.byteSize = count;
    }

    {
      if(IsWriting())
      {
        // ensure byte alignment
        m_Write->AlignTo<ChunkAlignment>();
        m_Write->Write(el.data(), count);
      }
      else if(IsReading())
      {
        // ensure byte alignment
        m_Read->AlignTo<ChunkAlignment>();

        el.resize((size_t)count);

        m_Read->Read(el.data(), count);
      }
    }

    if(ExportStructure())
    {
      if(m_ExportBuffers)
      {
        SDObject &obj = *m_StructureStack.back();

        obj.data.basic.u = m_StructuredFile->buffers.size();

        bytebuf *alloc = new bytebuf;
        alloc->assign(el);

        m_StructuredFile->buffers.push_back(alloc);
      }

      m_StructureStack.pop_back();
    }

    return *this;
  }

  Serialiser &Serialise(const rdcliteral &name, void *&el, uint64_t byteSize,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return Serialise(name, (byte *&)el, byteSize, flags);
  }

  Serialiser &Serialise(const rdcliteral &name, const void *&el, uint64_t byteSize,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return Serialise(name, (byte *&)el, byteSize, flags);
  }

#if ENABLED(RDOC_SIZET_SEP_TYPE)
  Serialiser &Serialise(const rdcliteral &name, void *&el, size_t &byteSize,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return Serialise(name, (byte *&)el, byteSize, flags);
  }

  Serialiser &Serialise(const rdcliteral &name, const void *&el, size_t &byteSize,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return Serialise(name, (byte *&)el, byteSize, flags);
  }
#endif

#if ENABLED(RDOC_WIN32)
  // annoyingly, windows SIZE_T is unsigned long on win32 which is a different type to
  // uint32_t/uint64_t. So we add a special overload here for its sake.
  Serialiser &Serialise(const rdcliteral &name, const void *&el, unsigned long &byteSize,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    uint64_t bs = byteSize;
    Serialiser &ret = Serialise(name, (byte *&)el, bs, flags);
    byteSize = (unsigned long)bs;
    return ret;
  }
#endif

  // serialise a fixed array like foo[4];
  // never needs to allocate, just needs to be iterated
  template <class T, size_t N>
  Serialiser &Serialise(const rdcliteral &name, T (&el)[N],
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    // for consistency with other arrays, even though this is redundant, we serialise out and in the
    // size
    uint64_t count = N;
    {
      m_InternalElement++;
      DoSerialise(*this, count);
      m_InternalElement--;

      if(count != N)
        RDCWARN("Fixed-size array length %zu serialised with different size %llu", N, count);
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &parent = *m_StructureStack.back();

      SDObject &arr = *parent.AddAndOwnChild(new SDObject(name, TypeName<T>()));
      m_StructureStack.push_back(&arr);

      arr.type.basetype = SDBasic::Array;
      arr.type.byteSize = N;
      arr.type.flags |= SDTypeFlags::FixedArray;

      arr.ReserveChildren(N);

      for(size_t i = 0; i < N; i++)
      {
        SDObject &obj = *arr.AddAndOwnChild(new SDObject("$el"_lit, TypeName<T>()));
        m_StructureStack.push_back(&obj);

        // default to struct. This will be overwritten if appropriate
        obj.type.basetype = SDBasic::Struct;
        obj.type.byteSize = sizeof(T);
        if(std::is_union<T>::value)
          obj.type.flags |= SDTypeFlags::Union;

        // Check against the serialised count here - on read if we don't have the right size this
        // means we won't read past the provided data.
        if(i < count)
        {
          SerialiseDispatch<Serialiser, T>::Do(*this, el[i]);
        }
        else
        {
          // we should have data for these elements, but we don't. Just default initialise
          el[i] = T();
        }

        m_StructureStack.pop_back();
      }

      // if we have more data than the fixed sized array allows, we must simply discard the excess
      if(count > N)
      {
        // prevent any trashing of structured data by these
        m_InternalElement++;
        T dummy;
        SerialiseDispatch<Serialiser, T>::Do(*this, dummy);
        m_InternalElement--;
      }

      m_StructureStack.pop_back();
    }
    else
    {
      for(size_t i = 0; i < N && i < count; i++)
        SerialiseDispatch<Serialiser, T>::Do(*this, el[i]);

      for(size_t i = N; i < count; i++)
      {
        T dummy = T();
        SerialiseDispatch<Serialiser, T>::Do(*this, dummy);
      }
    }

    return *this;
  }

  // specialisation for fixed character arrays, serialising as strings
  template <size_t N>
  Serialiser &Serialise(const rdcliteral &name, char (&el)[N],
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    rdcstr str;
    if(IsWriting())
      str = el;
    Serialise(name, str, flags);
    if(str.length() >= N)
    {
      RDCWARN("Serialising string too large for fixed-size array '%s', will be truncated",
              name.c_str());
      memcpy(el, str.c_str(), N - 1);
      el[N - 1] = 0;
    }
    else
    {
      // copy the string & trailing NULL into el.
      memcpy(el, str.c_str(), str.length() + 1);
    }

    return *this;
  }

  // special function for serialising dynamically sized arrays
  template <class T>
  Serialiser &Serialise(const rdcliteral &name, T *&el, uint64_t arrayCount,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    // silently handle NULL arrays
    if(IsWriting() && el == NULL)
      arrayCount = 0;

    {
      m_InternalElement++;
      DoSerialise(*this, arrayCount);
      m_InternalElement--;
    }

    if(IsReading())
    {
      VerifyArraySize(arrayCount);
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &parent = *m_StructureStack.back();

      SDObject &arr = *parent.AddAndOwnChild(new SDObject(name, TypeName<T>()));
      m_StructureStack.push_back(&arr);

      arr.type.basetype = SDBasic::Array;
      arr.type.byteSize = arrayCount;

      arr.ReserveChildren((size_t)arrayCount);

// Coverity is unable to tie this allocation together with the automatic scoped deallocation in the
// ScopedDeseralise* classes. We can verify with e.g. valgrind that there are no leaks, so to keep
// the analysis non-spammy we just don't allocate for coverity builds
#if !defined(__COVERITY__)
      if(IsReading() && !m_Structuriser && (flags & SerialiserFlags::AllocateMemory))
      {
        if(arrayCount > 0)
          el = new T[(size_t)arrayCount];
        else
          el = NULL;
      }
#endif

      if(m_LazyThreshold > 0 && arrayCount > m_LazyThreshold)
      {
        PushInternal();

        for(uint64_t i = 0; el && i < arrayCount; i++)
          SerialiseDispatch<Serialiser, T>::Do(*this, el[i]);

        PopInternal();

        arr.SetLazyArray(arrayCount, el, MakeLazySerialiser<T>());
      }
      else
      {
        for(uint64_t i = 0; el && i < arrayCount; i++)
        {
          SDObject &obj = *arr.AddAndOwnChild(new SDObject("$el"_lit, TypeName<T>()));
          m_StructureStack.push_back(&obj);

          // default to struct. This will be overwritten if appropriate
          obj.type.basetype = SDBasic::Struct;
          obj.type.byteSize = sizeof(T);
          if(std::is_union<T>::value)
            obj.type.flags |= SDTypeFlags::Union;

          SerialiseDispatch<Serialiser, T>::Do(*this, el[i]);

          m_StructureStack.pop_back();
        }
      }

      m_StructureStack.pop_back();
    }
    else
    {
// Coverity is unable to tie this allocation together with the automatic scoped deallocation in the
// ScopedDeseralise* classes. We can verify with e.g. valgrind that there are no leaks, so to keep
// the analysis non-spammy we just don't allocate for coverity builds
#if !defined(__COVERITY__)
      if(IsReading() && !m_Structuriser && (flags & SerialiserFlags::AllocateMemory))
      {
        if(arrayCount > 0)
          el = new T[(size_t)arrayCount];
        else
          el = NULL;
      }
#endif

      for(size_t i = 0; el && i < arrayCount; i++)
        SerialiseDispatch<Serialiser, T>::Do(*this, el[i]);
    }

    return *this;
  }

  // specialisations for container types
  template <class U>
  Serialiser &Serialise(const rdcliteral &name, rdcarray<U> &el,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    uint64_t size = (uint64_t)el.size();

    {
      m_InternalElement++;
      DoSerialise(*this, size);
      m_InternalElement--;
    }

    if(IsReading())
    {
      VerifyArraySize(size);
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &parent = *m_StructureStack.back();

      SDObject &arr = *parent.AddAndOwnChild(new SDObject(name, TypeName<U>()));
      m_StructureStack.push_back(&arr);

      arr.type.basetype = SDBasic::Array;
      arr.type.byteSize = size;

      arr.ReserveChildren((size_t)size);

      if(IsReading())
        el.resize((size_t)size);

      if(m_LazyThreshold > 0 && size > m_LazyThreshold)
      {
        PushInternal();

        for(size_t i = 0; i < (size_t)size; i++)
          SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);

        PopInternal();

        arr.SetLazyArray(size, el.data(), MakeLazySerialiser<U>());
      }
      else
      {
        for(size_t i = 0; i < (size_t)size; i++)
        {
          SDObject &obj = *arr.AddAndOwnChild(new SDObject("$el"_lit, TypeName<U>()));
          m_StructureStack.push_back(&obj);

          // default to struct. This will be overwritten if appropriate
          obj.type.basetype = SDBasic::Struct;
          obj.type.byteSize = sizeof(U);

          SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);

          m_StructureStack.pop_back();
        }
      }

      m_StructureStack.pop_back();
    }
    else
    {
      if(IsReading())
        el.resize((size_t)size);

      for(size_t i = 0; i < (size_t)size; i++)
        SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);
    }

    return *this;
  }

  template <class U, size_t N>
  Serialiser &Serialise(const rdcliteral &name, rdcfixedarray<U, N> &el,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    // for consistency with other arrays, even though this is redundant, we serialise out and in the
    // size
    uint64_t count = N;
    {
      m_InternalElement++;
      DoSerialise(*this, count);
      m_InternalElement--;

      if(count != N)
        RDCWARN("Fixed-size array length %zu serialised with different size %llu", N, count);
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &parent = *m_StructureStack.back();

      SDObject &arr = *parent.AddAndOwnChild(new SDObject(name, TypeName<U>()));
      m_StructureStack.push_back(&arr);

      arr.type.basetype = SDBasic::Array;
      arr.type.byteSize = N;
      arr.type.flags |= SDTypeFlags::FixedArray;

      arr.ReserveChildren(N);

      for(size_t i = 0; i < N; i++)
      {
        SDObject &obj = *arr.AddAndOwnChild(new SDObject("$el"_lit, TypeName<U>()));
        m_StructureStack.push_back(&obj);

        // default to struct. This will be overwritten if appropriate
        obj.type.basetype = SDBasic::Struct;
        obj.type.byteSize = sizeof(U);
        if(std::is_union<U>::value)
          obj.type.flags |= SDTypeFlags::Union;

        // Check against the serialised count here - on read if we don't have the right size this
        // means we won't read past the provided data.
        if(i < count)
        {
          SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);
        }
        else
        {
          // we should have data for these elements, but we don't. Just default initialise
          el[i] = U();
        }

        m_StructureStack.pop_back();
      }

      // if we have more data than the fixed sized array allows, we must simply discard the excess
      if(count > N)
      {
        // prevent any trashing of structured data by these
        m_InternalElement++;
        U dummy;
        SerialiseDispatch<Serialiser, U>::Do(*this, dummy);
        m_InternalElement--;
      }

      m_StructureStack.pop_back();
    }
    else
    {
      for(size_t i = 0; i < N && i < count; i++)
        SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);

      for(size_t i = N; i < count; i++)
      {
        U dummy = U();
        SerialiseDispatch<Serialiser, U>::Do(*this, dummy);
      }
    }

    return *this;
  }

  template <class U, class V>
  Serialiser &Serialise(const rdcliteral &name, rdcpair<U, V> &el,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &parent = *m_StructureStack.back();

      SDObject &arr = *parent.AddAndOwnChild(new SDObject(name, "pair"_lit));
      m_StructureStack.push_back(&arr);

      arr.type.basetype = SDBasic::Struct;
      arr.type.byteSize = 2;

      arr.ReserveChildren(2);

      {
        SDObject &obj = *arr.AddAndOwnChild(new SDObject("first"_lit, TypeName<U>()));
        m_StructureStack.push_back(&obj);

        // default to struct. This will be overwritten if appropriate
        obj.type.basetype = SDBasic::Struct;
        obj.type.byteSize = sizeof(U);

        SerialiseDispatch<Serialiser, U>::Do(*this, el.first);

        m_StructureStack.pop_back();
      }

      {
        SDObject &obj = *arr.AddAndOwnChild(new SDObject("second"_lit, TypeName<V>()));
        m_StructureStack.push_back(&obj);

        // default to struct. This will be overwritten if appropriate
        obj.type.basetype = SDBasic::Struct;
        obj.type.byteSize = sizeof(V);

        SerialiseDispatch<Serialiser, V>::Do(*this, el.second);

        m_StructureStack.pop_back();
      }

      m_StructureStack.pop_back();
    }
    else
    {
      SerialiseDispatch<Serialiser, U>::Do(*this, el.first);
      SerialiseDispatch<Serialiser, V>::Do(*this, el.second);
    }

    return *this;
  }

  // for const types, to cast away the const!
  // caller is responsible for ensuring that on write, it's safe to write into
  // this pointer on the other end
  template <class T>
  Serialiser &Serialise(const rdcliteral &name, const T &el,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return Serialise(name, (T &)el, flags);
  }

  // dynamic array variant
  template <class T>
  Serialiser &Serialise(const rdcliteral &name, const T *&el, uint64_t arrayCount,
                        SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return Serialise(name, (T *&)el, arrayCount, flags);
  }

  template <class T>
  Serialiser &SerialiseNullable(const rdcliteral &name, T *&el,
                                SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    bool present = (el != NULL);

    {
      m_InternalElement++;
      DoSerialise(*this, present);
      m_InternalElement--;
    }

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

// Coverity is unable to tie this allocation together with the automatic scoped deallocation in the
// ScopedDeseralise* classes. We can verify with e.g. valgrind that there are no leaks, so to keep
// the analysis non-spammy we just don't allocate for coverity builds
#if !defined(__COVERITY__)
      if(IsReading())
      {
        if(present)
          el = new T;
        else
          el = NULL;
      }
#endif

      if(el)
      {
        Serialise(name, *el, flags);

        SDObject &parent = *m_StructureStack.back();

        SDObject &nullable = *parent.GetChild(parent.NumChildren() - 1);

        nullable.type.flags |= SDTypeFlags::Nullable;
        if(std::is_union<T>::value)
          nullable.type.flags |= SDTypeFlags::Union;
      }
      else
      {
        SDObject &parent = *m_StructureStack.back();

        SDObject &nullable = *parent.AddAndOwnChild(new SDObject(name, TypeName<T>()));

        nullable.type.basetype = SDBasic::Null;
        nullable.type.byteSize = 0;
        nullable.type.flags |= SDTypeFlags::Nullable;
        if(std::is_union<T>::value)
          nullable.type.flags |= SDTypeFlags::Union;
      }
    }
    else
    {
// Coverity is unable to tie this allocation together with the automatic scoped deallocation in the
// ScopedDeseralise* classes. We can verify with e.g. valgrind that there are no leaks, so to keep
// the analysis non-spammy we just don't allocate for coverity builds
#if !defined(__COVERITY__)
      if(IsReading())
      {
        if(present)
          el = new T;
        else
          el = NULL;
      }
#endif

      if(el)
        Serialise(name, *el, flags);
    }

    return *this;
  }

  template <class T>
  Serialiser &SerialiseNullable(const rdcliteral &name, const T *&el,
                                SerialiserFlags flags = SerialiserFlags::NoFlags)
  {
    return SerialiseNullable(name, (T *&)el, flags);
  }

  Serialiser &SerialiseStream(const rdcstr &name, StreamReader &stream,
                              RENDERDOC_ProgressCallback progress = RENDERDOC_ProgressCallback())
  {
    RDCCOMPILE_ASSERT(IsWriting(), "Can't read into a StreamReader");

    uint64_t totalSize = stream.GetSize();

    {
      m_InternalElement++;
      DoSerialise(*this, totalSize);
      m_InternalElement--;
    }

    // ensure byte alignment
    m_Write->AlignTo<ChunkAlignment>();

    StreamTransfer(m_Write, &stream, progress);

    return *this;
  }

  Serialiser &SerialiseStream(const rdcstr &name, StreamWriter &stream,
                              RENDERDOC_ProgressCallback progress)
  {
    RDCCOMPILE_ASSERT(IsReading(), "Can't write from a StreamWriter");

    uint64_t totalSize = 0;

    {
      m_InternalElement++;
      DoSerialise(*this, totalSize);
      m_InternalElement--;
    }

    size_t byteSize = (size_t)totalSize;

    byte *structBuf = NULL;

    if(ExportStructure())
    {
      if(m_StructureStack.empty())
      {
        RDCERR("Serialising object outside of chunk context! Start Chunk before any Serialise!");
        return *this;
      }

      SDObject &current = *m_StructureStack.back();

      SDObject &obj = *current.AddAndOwnChild(new SDObject(name, "Byte Buffer"_lit));
      m_StructureStack.push_back(&obj);

      obj.type.basetype = SDBasic::Buffer;
      obj.type.byteSize = totalSize;

      if(m_ExportBuffers)
      {
        obj.data.basic.u = m_StructuredFile->buffers.size();

        m_StructuredFile->buffers.push_back(new bytebuf);
        m_StructuredFile->buffers.back()->resize((size_t)totalSize);

        // this will be filled as we read below
        structBuf = m_StructuredFile->buffers.back()->data();
      }

      m_StructureStack.pop_back();
    }

    // ensure byte alignment
    m_Read->AlignTo<ChunkAlignment>();

    if(totalSize > 0)
    {
      // copy 1MB at a time
      const uint64_t StreamIOChunkSize = 1024 * 1024;

      // copy 1MB at a time
      const uint64_t bufSize = RDCMIN(StreamIOChunkSize, totalSize);
      uint64_t numBufs = totalSize / bufSize;
      // last remaining partial buffer
      if(totalSize % (uint64_t)bufSize > 0)
        numBufs++;

      byte *buf = new byte[byteSize];

      if(progress)
        progress(0.0001f);

      for(uint64_t i = 0; i < numBufs; i++)
      {
        uint64_t payloadLength = RDCMIN(bufSize, totalSize);

        m_Read->Read(buf, payloadLength);
        stream.Write(buf, payloadLength);

        if(structBuf)
        {
          memcpy(structBuf, buf, (size_t)payloadLength);
          structBuf += payloadLength;
        }

        totalSize -= payloadLength;
        if(progress)
          progress(float(i + 1) / float(numBufs));
      }

      delete[] buf;
    }
    else
    {
      if(progress)
        progress(1.0f);
    }

    return *this;
  }

  // these functions can be chained onto the end of a Serialise() call or macro to
  // set additional properties or change things
  Serialiser &Hidden()
  {
    if(ExportStructure() && !m_StructureStack.empty())
    {
      SDObject &current = *m_StructureStack.back();

      current.type.flags |= SDTypeFlags::HiddenChildren;

      if(current.NumChildren() > 0)
        current.GetChild(current.NumChildren() - 1)->type.flags |= SDTypeFlags::Hidden;
    }

    return *this;
  }

  Serialiser &TypedAs(const rdcstr &name)
  {
    if(ExportStructure() && !m_StructureStack.empty())
    {
      SDObject &current = *m_StructureStack.back();

      if(current.NumChildren() > 0)
      {
        SDObject *last = current.GetChild(current.NumChildren() - 1);
        last->type.name = name;

        if(last->type.basetype == SDBasic::Array)
        {
          for(SDObject *obj : *last)
            obj->type.name = name;
        }
      }
    }

    return *this;
  }

  Serialiser &Important()
  {
    if(ExportStructure() && !m_StructureStack.empty())
    {
      SDObject &current = *m_StructureStack.back();

      current.type.flags |= SDTypeFlags::ImportantChildren;

      if(current.NumChildren() > 0)
      {
        SDObject *last = current.GetChild(current.NumChildren() - 1);
        last->type.flags |= SDTypeFlags::Important;
      }
    }

    return *this;
  }

  Serialiser &Unimportant()
  {
    if(ExportStructure() && !m_StructureStack.empty())
    {
      SDObject &current = *m_StructureStack.back();

      // similar to Important() above but we *don't* set the important flag, we just mark the
      // parent has
      current.type.flags |= SDTypeFlags::ImportantChildren;
    }

    return *this;
  }

  Serialiser &Named(const rdcstr &name)
  {
    if(ExportStructure() && !m_StructureStack.empty())
    {
      SDObject &current = *m_StructureStack.back();

      if(current.NumChildren() > 0)
        current.GetChild(current.NumChildren() - 1)->name = name;
    }

    return *this;
  }

  Serialiser &OffsetOrSize()
  {
    if(ExportStructure() && !m_StructureStack.empty())
    {
      SDObject &current = *m_StructureStack.back();

      if(current.NumChildren() > 0)
        current.GetChild(current.NumChildren() - 1)->type.flags |= SDTypeFlags::OffsetOrSize;
    }

    return *this;
  }

  // these functions should be used very carefully, they completely disable structured export for
  // anything serialised while internal is set.
  void PushInternal() { m_InternalElement++; }
  void PopInternal() { m_InternalElement--; }
  // this sets the current threshold for making structured data lazy for arrays above this size.
  // If set to 0, structured data is never set as lazy
  void SetLazyThreshold(uint32_t arraySize) { m_LazyThreshold = arraySize; }
  /////////////////////////////////////////////////////////////////////////////

  // for basic/leaf types. Read/written just as byte soup, MUST be plain old data
  template <class T>
  void SerialiseValue(SDBasic type, size_t byteSize, T &el)
  {
    if(IsWriting())
    {
      m_Write->Write(el);
    }
    else if(IsReading())
    {
      m_Read->Read(el);
    }

    if(!ExportStructure())
      return;

    SDObject &current = *m_StructureStack.back();

    current.type.basetype = type;
    current.type.byteSize = byteSize;

    switch(type)
    {
      case SDBasic::Chunk:
      case SDBasic::Struct:
      case SDBasic::Array:
      case SDBasic::Buffer:
      case SDBasic::Null: RDCFATAL("Cannot call SerialiseValue for type %d!", type); break;
      case SDBasic::String: RDCFATAL("eString should be specialised!"); break;
      case SDBasic::Enum:
      case SDBasic::Resource:
      case SDBasic::UnsignedInteger:
        if(byteSize == 1)
          current.data.basic.u = (uint64_t)(uint8_t)el;
        else if(byteSize == 2)
          current.data.basic.u = (uint64_t)(uint16_t)el;
        else if(byteSize == 4)
          current.data.basic.u = (uint64_t)(uint32_t)el;
        else if(byteSize == 8)
          current.data.basic.u = (uint64_t)el;
        else
          RDCFATAL("Unsupported unsigned integer byte width: %u", byteSize);
        break;
      case SDBasic::SignedInteger:
        if(byteSize == 1)
          current.data.basic.i = (int64_t)(int8_t)el;
        else if(byteSize == 2)
          current.data.basic.i = (int64_t)(int16_t)el;
        else if(byteSize == 4)
          current.data.basic.i = (int64_t)(int32_t)el;
        else if(byteSize == 8)
          current.data.basic.i = (int64_t)el;
        else
          RDCFATAL("Unsupported signed integer byte width: %u", byteSize);
        break;
      case SDBasic::Float:
        if(byteSize == 4)
          current.data.basic.d = (double)(float)el;
        else if(byteSize == 8)
          current.data.basic.d = (double)el;
        else
          RDCFATAL("Unsupported floating point byte width: %u", byteSize);
        break;
      case SDBasic::Boolean:
        // co-erce to boolean
        current.data.basic.b = !!(el);
        break;
      case SDBasic::Character: current.data.basic.c = (char)(el); break;
    }
  }

  void SerialiseValue(SDBasic type, size_t byteSize, rdcstr &el)
  {
    uint32_t len = 0;

    if(IsReading())
    {
      m_Read->Read(len);
      VerifyArraySize(len);
      el.resize((size_t)len);
      if(len > 0)
        m_Read->Read(&el[0], len);
    }
    else
    {
      len = (uint32_t)el.size();
      m_Write->Write(len);
      m_Write->Write(el.c_str(), len);
    }

    if(ExportStructure())
    {
      SDObject &current = *m_StructureStack.back();

      current.type.basetype = type;
      current.type.byteSize = len;
      current.data.str = el;
    }
  }

  void SerialiseValue(SDBasic type, size_t byteSize, rdcinflexiblestr &el)
  {
    if(IsReading())
    {
      rdcstr str;
      SerialiseValue(type, byteSize, str);
      el = str;
    }
    else
    {
      rdcstr str;
      str = el;
      SerialiseValue(type, byteSize, str);
    }
  }

  void SerialiseValue(SDBasic type, size_t byteSize, char *&el)
  {
    int32_t len = 0;

    if(IsReading())
    {
      m_Read->Read(len);
      if(len == -1)
      {
        el = NULL;
      }
      else
      {
        rdcstr str;
        str.resize(len);
        if(len > 0)
          m_Read->Read(&str[0], len);
        el = (char *)StringDB(str);
      }
    }
    else
    {
      len = el ? (int32_t)strlen(el) : -1;
      m_Write->Write(len);
      if(len > 0)
        m_Write->Write(el, len);
    }

    if(ExportStructure())
    {
      SDObject &current = *m_StructureStack.back();

      current.type.basetype = type;
      current.type.byteSize = RDCMAX(len, 0);
      current.data.str = el ? el : "";
      if(len == -1)
        current.type.flags |= SDTypeFlags::NullString;
    }
  }

  void SerialiseValue(SDBasic type, size_t byteSize, const char *&el)
  {
    SerialiseValue(type, byteSize, (char *&)el);
  }

  template <typename U>
  void SerialiseArrayValue(SDBasic type, rdcarray<U> &el)
  {
    uint64_t size = (uint64_t)el.size();
    {
      m_InternalElement++;
      DoSerialise(*this, size);
      m_InternalElement--;
    }

    if(IsReading())
    {
      VerifyArraySize(size);
    }

    if(ExportStructure())
    {
      SDObject &current = *m_StructureStack.back();

      current.type.basetype = SDBasic::Array;
      current.type.byteSize = size;

      current.ReserveChildren((size_t)size);

      if(IsReading())
        el.resize((size_t)size);

      if(m_LazyThreshold > 0 && size > m_LazyThreshold)
      {
        PushInternal();

        for(size_t i = 0; i < (size_t)size; i++)
          SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);

        PopInternal();

        current.SetLazyArray(size, el.data(), MakeLazySerialiser<U>());
      }
      else
      {
        for(size_t i = 0; i < (size_t)size; i++)
        {
          SDObject &obj = *current.AddAndOwnChild(new SDObject("$el"_lit, TypeName<U>()));
          m_StructureStack.push_back(&obj);

          // default to struct. This will be overwritten if appropriate
          obj.type.basetype = SDBasic::Struct;
          obj.type.byteSize = sizeof(U);

          SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);

          m_StructureStack.pop_back();
        }
      }
    }
    else
    {
      if(IsReading())
        el.resize((size_t)size);

      for(size_t i = 0; i < (size_t)size; i++)
        SerialiseDispatch<Serialiser, U>::Do(*this, el[i]);
    }
  }

  // constructors only available by the derived classes for each serialiser type
protected:
  Serialiser(StreamWriter *writer, Ownership own);
  Serialiser(StreamReader *reader, Ownership own, SDObject *rootStructuredObj);

  template <SerialiserMode othertype>
  friend class Serialiser;

  void SetStructuriser(bool s) { m_Structuriser = s; }
private:
  static const uint64_t ChunkAlignment = 64;
  template <class SerialiserMode, typename T, bool isEnum = std::is_enum<T>::value>
  struct SerialiseDispatch
  {
    static void Do(SerialiserMode &ser, T &el) { DoSerialise(ser, el); }
  };

  template <class SerialiserMode, typename T>
  struct SerialiseDispatch<SerialiserMode, T, true>
  {
    static void Do(SerialiserMode &ser, T &el)
    {
      typedef typename std::underlying_type<T>::type etype;
      constexpr bool is_valid_type =
          std::is_same<etype, uint64_t>::value || std::is_same<etype, uint32_t>::value ||
          std::is_same<etype, uint16_t>::value || std::is_same<etype, uint8_t>::value ||
          std::is_same<etype, int>::value;
      RDCCOMPILE_ASSERT(is_valid_type, "enum isn't expected type");
      ser.SerialiseValue(SDBasic::Enum, sizeof(T), (etype &)(el));
      ser.SerialiseStringify(el);
    }
  };

  template <typename intSize>
  void VerifyArraySize(intSize &count)
  {
    uint64_t size = m_Read->GetSize();

// for streaming, just take 4GB as a 'semi reasonable' upper limit for array sizes
// use 1GB on 32-bit to avoid overflows
#if ENABLED(RDOC_X64)
    if(m_DataStreaming)
      size = 0xFFFFFFFFU;
#else
    if(m_DataStreaming)
      size = 0x3FFFFFFFU;
#endif

    if(count > size)
    {
      RDResult result;
      SET_ERROR_RESULT(
          result, ResultCode::FileCorrupted,
          "Reading invalid array or byte buffer - %llu larger than total stream size %llu.", count,
          size);

      // if we owned the previous stream, delete it
      if(m_Ownership == Ownership::Stream)
        delete m_Read;

      // replace our stream with an invalid one so all subsequent reads fail
      m_Read = new StreamReader(StreamReader::InvalidStream, result);
      m_Ownership = Ownership::Stream;

      // set the count to 0
      count = 0;
    }
  }

  template <typename T>
  LazyGenerator MakeLazySerialiser()
  {
    ChunkLookup lookup = m_ChunkLookup;
    void *userData = m_pUserData;
    bool buffers = m_ExportBuffers;
    uint64_t ver = m_Version;
    std::set<rdcstr> *stringDB = m_ExtStringDB;
    return [lookup, userData, buffers, ver, stringDB](const void *ptr) {
      T &input = *(T *)ptr;
      static StreamReader dummy(StreamReader::DummyStream);

      SDObject *ret = new SDObject("$el"_lit, TypeName<T>());

      ret->type.byteSize = sizeof(T);
      if(std::is_union<T>::value)
        ret->type.flags |= SDTypeFlags::Union;

      Serialiser<SerialiserMode::Reading> ser(&dummy, Ownership::Nothing, ret);

      ser.SetVersion(ver);
      ser.ConfigureStructuredExport(lookup, buffers, 0, 1.0);
      ser.SetStreamingMode(true);
      ser.SetStructuriser(true);
      ser.SetUserData(userData);
      ser.SetStringDatabase(stringDB);

      SerialiseDispatch<Serialiser<SerialiserMode::Reading>, T>::Do(ser, input);

      return ret;
    };
  }

  void *m_pUserData = NULL;
  uint64_t m_Version = 0;

  uint64_t m_StructArg = 0;

  StreamWriter *m_Write = NULL;
  StreamReader *m_Read = NULL;

  Ownership m_Ownership;

  // See SetStreamingMode
  bool m_DataStreaming = false;
  bool m_ActionChunk = false;
  bool m_Structuriser = false;

  uint64_t m_LastChunkOffset = 0;
  uint64_t m_ChunkFixup = 0;

  bool m_ExportStructured = false;
  bool m_ExportBuffers = false;
  int m_InternalElement = 0;
  uint32_t m_LazyThreshold = 0;
  SDFile m_StructData;
  SDFile *m_StructuredFile = &m_StructData;
  rdcarray<SDObject *> m_StructureStack;

  uint32_t m_ChunkFlags = 0;
  SDChunkMetaData m_ChunkMetadata;
  double m_TimerFrequency = 1.0;
  uint64_t m_TimerBase = 0;

  // a database of strings read from the file, useful when serialised structures
  // expect a char* to return and point to static memory
  std::set<rdcstr> m_StringDB;

  // external storage - so the string storage can persist after the lifetime of the serialiser
  std::set<rdcstr> *m_ExtStringDB = NULL;

  const char *StringDB(const rdcstr &s)
  {
    if(m_ExtStringDB)
    {
      auto it = m_ExtStringDB->insert(s);
      return it.first->c_str();
    }

    auto it = m_StringDB.insert(s);
    return it.first->c_str();
  }

  ChunkLookup m_ChunkLookup = NULL;
  FileIO::LogFileHandle *m_DebugDumpLog = NULL;
};

#ifndef SERIALISER_IMPL
class WriteSerialiser : public Serialiser<SerialiserMode::Writing>
{
public:
  WriteSerialiser(StreamWriter *writer, Ownership own) : Serialiser(writer, own) {}
  void WriteChunk(uint32_t chunkID, uint64_t byteLength = 0) { BeginChunk(chunkID, byteLength); }
};

class ReadSerialiser : public Serialiser<SerialiserMode::Reading>
{
public:
  ReadSerialiser(StreamReader *reader, Ownership own) : Serialiser(reader, own, NULL) {}
  template <typename ChunkType>
  ChunkType ReadChunk()
  {
    // parameters are ignored when reading
    return (ChunkType)BeginChunk(0, 0);
  }
};

class StructuredSerialiser : public Serialiser<SerialiserMode::Reading>
{
public:
  StructuredSerialiser(SDObject *obj, ChunkLookup lookup)
      : Serialiser(new StreamReader(StreamReader::DummyStream), Ownership::Stream, obj)
  {
    ConfigureStructuredExport(lookup, false, 0, 1.0);
    SetStreamingMode(true);
    SetStructuriser(true);
  }
};
#endif

#define BASIC_TYPE_SERIALISE(typeName, member, type, byteSize) \
  DECLARE_STRINGISE_TYPE(typeName)                             \
  DECLARE_STRINGISE_TYPE(rdcarray<typeName>)                   \
  template <class SerialiserType>                              \
  void DoSerialise(SerialiserType &ser, typeName &el)          \
  {                                                            \
    ser.SerialiseValue(type, byteSize, member);                \
  }

#define BASIC_TYPE_SERIALISE_STRINGIFY(typeName, member, type, byteSize) \
  template <class SerialiserType>                                        \
  void DoSerialise(SerialiserType &ser, typeName &el)                    \
  {                                                                      \
    ser.SerialiseValue(type, byteSize, member);                          \
    ser.SerialiseStringify(el);                                          \
  }

BASIC_TYPE_SERIALISE(int64_t, el, SDBasic::SignedInteger, 8);
BASIC_TYPE_SERIALISE(uint64_t, el, SDBasic::UnsignedInteger, 8);
BASIC_TYPE_SERIALISE(int32_t, el, SDBasic::SignedInteger, 4);
BASIC_TYPE_SERIALISE(uint32_t, el, SDBasic::UnsignedInteger, 4);
BASIC_TYPE_SERIALISE(int16_t, el, SDBasic::SignedInteger, 2);
BASIC_TYPE_SERIALISE(uint16_t, el, SDBasic::UnsignedInteger, 2);
BASIC_TYPE_SERIALISE(int8_t, el, SDBasic::SignedInteger, 1);
BASIC_TYPE_SERIALISE(uint8_t, el, SDBasic::UnsignedInteger, 1);

BASIC_TYPE_SERIALISE(double, el, SDBasic::Float, 8);
BASIC_TYPE_SERIALISE(float, el, SDBasic::Float, 4);

BASIC_TYPE_SERIALISE(bool, el, SDBasic::Boolean, 1);

BASIC_TYPE_SERIALISE(char, el, SDBasic::Character, 1);

template <>
inline rdcliteral TypeName<char *>()
{
  return "string"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, char *&el)
{
  ser.SerialiseValue(SDBasic::String, 0, el);
}

template <>
inline rdcliteral TypeName<const char *>()
{
  return "string"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, const char *&el)
{
  ser.SerialiseValue(SDBasic::String, 0, el);
}

template <>
inline rdcliteral TypeName<rdcstr>()
{
  return "string"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, rdcstr &el)
{
  ser.SerialiseValue(SDBasic::String, 0, el);
}

template <>
inline rdcliteral TypeName<rdcinflexiblestr>()
{
  return "string"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, rdcinflexiblestr &el)
{
  ser.SerialiseValue(SDBasic::String, 0, el);
}

template <class SerialiserType, typename U>
void DoSerialise(SerialiserType &ser, rdcarray<U> &el)
{
  ser.SerialiseArrayValue(SDBasic::Array, el);
}

DECLARE_STRINGISE_TYPE(SDObject *);

class ScopedChunk;

struct ChunkPage
{
  // compare just with the ID, so that old pages w hich have been reset in the pool don't get reset
  // again if an allocator subsequently tries to free them
  bool operator==(const ChunkPage &o) { return ID == o.ID; }
  size_t ID;

  // we allocate at two granularities, chunks are 16 bytes, buffers are multiples of 64-bytes
  // to keep things simple we allocate the chunk memory as 16/64 = a quarter the size of the
  // buffer memory. This will waste a bit of memory because we expect buffers to be on average
  // larger than 64 bytes.

  // base of the buffer
  byte *bufferBase;
  // head of the buffer
  byte *bufferHead;

  byte *chunkBase;
  byte *chunkHead;
};

// this is the first level, it allocates whole pages and returns them to allocators for finer
// grained allocation, and those allocators can return whole pages back. This is necessary because
// when fine-grained resetting is allowed we need to associate whole pages with objects and if those
// objects are allocating interleaved we need to immediately associate pages with them.
class ChunkPagePool
{
public:
  ChunkPagePool(size_t PageSize) : BufferPageSize(PageSize), ChunkPageSize(PageSize / 32) {}
  ChunkPagePool(const ChunkPagePool &) = delete;
  ChunkPagePool(ChunkPagePool &&) = delete;
  ChunkPagePool &operator=(const ChunkPagePool &) = delete;
  ~ChunkPagePool();

  // Allocate a page
  ChunkPage AllocPage();

  // really free any unused pages
  void Trim();

  // reset all pages to free
  void Reset();

  // reset a page set, other pages will remain in use
  void ResetPageSet(const rdcarray<ChunkPage> &pages);

  size_t GetBufferPageSize() { return BufferPageSize; }
  size_t GetChunkPageSize() { return ChunkPageSize; }
private:
  size_t BufferPageSize;
  size_t ChunkPageSize;

  size_t m_ID = 1;

  // a page is in precisely ONE of these arrays at any time.
  // Reset() will move all allocated pages back to free pages and reclaim all that memory
  // ResetPageSet() will move any referenced pages from allocatedPages back to freePages
  rdcarray<ChunkPage> freePages;
  rdcarray<ChunkPage> allocatedPages;
};

// this is the second level, it should only be used by one object (or a group of objects that are
// always reset together). It pulls pages from the pool and allocates from them, and can then
// release those pages back again with a reset operation.
class ChunkAllocator
{
public:
  ChunkAllocator(ChunkPagePool &pool) : m_Pool(pool) {}
  ChunkAllocator(const ChunkAllocator &) = delete;
  ChunkAllocator(ChunkAllocator &&) = delete;
  ChunkAllocator &operator=(const ChunkAllocator &) = delete;
  ~ChunkAllocator();

  // swap with another chunk allocator - must be from the same pool
  void swap(ChunkAllocator &alloc);

  byte *AllocAlignedBuffer(uint64_t size);
  byte *AllocChunk();

  void Reset();

private:
  ChunkPagePool &m_Pool;

  // as we're recording each new page we start gets added here. The last page is the one we're
  // currently allocating from.
  rdcarray<ChunkPage> pages;

  // given a page and the known page size, how much is left
  inline size_t GetRemainingBufferBytes(const ChunkPage &p)
  {
    return m_Pool.GetBufferPageSize() - (p.bufferHead - p.bufferBase);
  }
  inline size_t GetRemainingChunkBytes(const ChunkPage &p)
  {
    return m_Pool.GetChunkPageSize() - (p.chunkHead - p.chunkBase);
  }
  inline size_t GetRemainingBytes(bool chunkAlloc, const ChunkPage &p)
  {
    return chunkAlloc ? GetRemainingChunkBytes(p) : GetRemainingBufferBytes(p);
  }

  byte *AllocateFromPages(bool chunkAlloc, size_t size);
};

// holds the memory, length and type for a given chunk, so that it can be
// passed around and moved between owners before being serialised out
class Chunk
{
  Chunk(bool fromAllocator) : m_FromAllocator(fromAllocator) {}
  ~Chunk()
  {
    FreeAlignedBuffer(m_Data);

#if ENABLED(RDOC_DEVEL)
    Atomic::Dec64(&m_LiveChunks);
    Atomic::ExchAdd64(&m_TotalMem, -int64_t(m_Length));
#endif
  }

public:
  void Delete() { Delete(m_FromAllocator); }
  bool IsFromAllocator() { return m_FromAllocator; }
  void Delete(bool fromAllocator)
  {
    if(!fromAllocator)
      delete this;
  }

  template <typename ChunkType>
  ChunkType GetChunkType()
  {
    return (ChunkType)m_ChunkType;
  }
#if ENABLED(RDOC_DEVEL)
  static uint64_t NumLiveChunks() { return m_LiveChunks; }
  static uint64_t TotalMem() { return m_TotalMem; }
#else
  static uint64_t NumLiveChunks() { return 0; }
  static uint64_t TotalMem() { return 0; }
#endif

  // grab current contents of the serialiser into a new chunk
  static Chunk *Create(Serialiser<SerialiserMode::Writing> &ser, uint16_t chunkType,
                       ChunkAllocator *allocator = NULL, bool stealDataFromWriter = false);

  byte *GetData() const { return m_Data; }
  Chunk *Duplicate()
  {
    Chunk *ret = new Chunk();
    ret->m_Length = m_Length;
    ret->m_ChunkType = m_ChunkType;

    ret->m_Data = AllocAlignedBuffer(m_Length);
    ret->m_FromAllocator = false;

    memcpy(ret->m_Data, m_Data, (size_t)m_Length);

#if ENABLED(RDOC_DEVEL)
    Atomic::Inc64(&m_LiveChunks);
    Atomic::ExchAdd64(&m_TotalMem, int64_t(m_Length));
#endif

    return ret;
  }

  void Write(Serialiser<SerialiserMode::Writing> &ser)
  {
    ser.GetWriter()->Write((const void *)m_Data, (size_t)m_Length);
  }

private:
  Chunk() = default;
  Chunk(const Chunk &) = delete;
  Chunk &operator=(const Chunk &) = delete;

  uint16_t m_ChunkType;

  bool m_FromAllocator = false;

  uint32_t m_Length;
  byte *m_Data;

#if ENABLED(RDOC_DEVEL)
  static int64_t m_LiveChunks, m_TotalMem;
#endif
};

#ifndef SERIALISER_IMPL
class ScopedChunk
{
public:
  template <typename ChunkType>
  ScopedChunk(WriteSerialiser &s, ChunkType i, uint64_t byteLength = 0)
      : m_Idx(uint16_t(i)), m_Ser(s), m_Ended(false)
  {
    m_Ser.WriteChunk(m_Idx, byteLength);
  }
  ~ScopedChunk()
  {
    if(!m_Ended)
      End();
  }

  Chunk *Steal()
  {
    End();
    return Chunk::Create(m_Ser, m_Idx, NULL, true);
  }

  Chunk *Get(ChunkAllocator *allocator = NULL)
  {
    End();
    return Chunk::Create(m_Ser, m_Idx, allocator, false);
  }

private:
  WriteSerialiser &m_Ser;
  uint16_t m_Idx;
  bool m_Ended;

  void End()
  {
    RDCASSERT(!m_Ended);

    m_Ser.EndChunk();

    m_Ended = true;
  }
};
#endif

template <class SerialiserType, class T>
struct ScopedDeserialise
{
  ScopedDeserialise(const SerialiserType &ser, const T &el) : m_Ser(ser), m_El(el) {}
  ~ScopedDeserialise()
  {
    if(m_Ser.IsReading())
      Deserialise(m_El);
  }
  const SerialiserType &m_Ser;
  const T &m_El;
};

template <class SerialiserType, class ptrT>
struct ScopedDeserialiseNullable
{
  typedef typename std::remove_pointer<ptrT>::type T;

  ScopedDeserialiseNullable(const SerialiserType &ser, T **el) : m_Ser(ser), m_El(el) {}
  ~ScopedDeserialiseNullable()
  {
    if(m_Ser.IsReading() && *m_El != NULL)
    {
      Deserialise(**m_El);
      delete *m_El;
    }
  }
  const SerialiserType &m_Ser;
  T **m_El;
};

template <class SerialiserType, class ptrT>
struct ScopedDeserialiseArray
{
  typedef typename std::remove_pointer<ptrT>::type T;

  ScopedDeserialiseArray(const SerialiserType &ser, T **el, uint64_t c)
      : m_Ser(ser), m_El(el), count(c)
  {
  }
  ~ScopedDeserialiseArray()
  {
    if(m_Ser.IsReading() && *m_El)
    {
      for(uint64_t i = 0; i < count; i++)
        Deserialise((*m_El)[i]);
      delete[] * m_El;
    }
  }
  const SerialiserType &m_Ser;
  T **m_El;
  uint64_t count;
};

template <class SerialiserType>
struct ScopedDeserialiseArray<SerialiserType, void *>
{
  ScopedDeserialiseArray(const SerialiserType &ser, void **el, uint64_t) : m_Ser(ser), m_El(el) {}
  ~ScopedDeserialiseArray()
  {
    if(m_Ser.IsReading())
      FreeAlignedBuffer((byte *)*m_El);
  }
  const SerialiserType &m_Ser;
  void **m_El;
};

template <class SerialiserType>
struct ScopedDeserialiseArray<SerialiserType, const void *>
{
  ScopedDeserialiseArray(const SerialiserType &ser, const void **el, uint64_t)
      : m_Ser(ser), m_El(el)
  {
  }
  ~ScopedDeserialiseArray()
  {
    if(m_Ser.IsReading())
      FreeAlignedBuffer((byte *)*m_El);
  }
  const SerialiserType &m_Ser;
  const void **m_El;
};

template <class SerialiserType>
struct ScopedDeserialiseArray<SerialiserType, byte *>
{
  ScopedDeserialiseArray(const SerialiserType &ser, byte **el, uint64_t) : m_Ser(ser), m_El(el) {}
  ~ScopedDeserialiseArray()
  {
    if(m_Ser.IsReading())
      FreeAlignedBuffer(*m_El);
  }
  const SerialiserType &m_Ser;
  byte **m_El;
};

// can be overridden to change the name in the helper macros locally
#define GET_SERIALISER (ser)

#define SCOPED_SERIALISE_CHUNK(...) ScopedChunk scope(GET_SERIALISER, __VA_ARGS__);

// these helper macros are intended for use when serialising objects in chunks
#define SERIALISE_ELEMENT(obj)                                                               \
  ScopedDeserialise<decltype(GET_SERIALISER), decltype(obj)> CONCAT(deserialise_, __LINE__)( \
      GET_SERIALISER, obj);                                                                  \
  GET_SERIALISER.Serialise(STRING_LITERAL(#obj), obj)

// for _TYPED serialises, we need to first clear the object to 0 since the typed alias might not
// write it all. This is mostly only when co-oercing a BOOL type to bool, where only one byte gets
// written. We go via ClearObj() so that when StructuredSerialiser is in use and the reader is a
// dummy, it can skip the write.
#define SERIALISE_ELEMENT_TYPED(type, obj) \
  ser.ClearObj(obj);                       \
  union                                    \
  {                                        \
    type *t;                               \
    decltype(obj) *o;                      \
  } CONCAT(union, __LINE__);               \
  CONCAT(union, __LINE__).o = &obj;        \
  GET_SERIALISER.template Serialise<type>(STRING_LITERAL(#obj), *CONCAT(union, __LINE__).t)

#define SERIALISE_ELEMENT_ARRAY_TYPED(type, arrayObj, countObj)                                  \
  RDCCOMPILE_ASSERT(sizeof(*arrayObj) == sizeof(type),                                           \
                    "Array serialised co-erced type must be identically sized");                 \
  union                                                                                          \
  {                                                                                              \
    type **t;                                                                                    \
    decltype(arrayObj) *o;                                                                       \
  } CONCAT(union, __LINE__);                                                                     \
  CONCAT(union, __LINE__).o = &arrayObj;                                                         \
  GET_SERIALISER.template Serialise<type>(STRING_LITERAL(#arrayObj), *CONCAT(union, __LINE__).t, \
                                          countObj, SerialiserFlags::AllocateMemory)

#define SERIALISE_ELEMENT_ARRAY(obj, count)                                                       \
  uint64_t CONCAT(dummy_array_count, __LINE__) = 0;                                               \
  (void)CONCAT(dummy_array_count, __LINE__);                                                      \
  ScopedDeserialiseArray<decltype(GET_SERIALISER), decltype(obj)> CONCAT(deserialise_, __LINE__)( \
      GET_SERIALISER, &obj, count);                                                               \
  GET_SERIALISER.Serialise(STRING_LITERAL(#obj), obj, count, SerialiserFlags::AllocateMemory)

#define SERIALISE_ELEMENT_OPT(obj)                                           \
  ScopedDeserialiseNullable<decltype(GET_SERIALISER), decltype(obj)> CONCAT( \
      deserialise_, __LINE__)(GET_SERIALISER, &obj);                         \
  GET_SERIALISER.SerialiseNullable(STRING_LITERAL(#obj), obj)

#define SERIALISE_ELEMENT_LOCAL(obj, inValue)                                                 \
  typename std::remove_cv<typename std::remove_reference<decltype(inValue)>::type>::type obj; \
  ScopedDeserialise<decltype(GET_SERIALISER), decltype(obj)> CONCAT(deserialise_, __LINE__)(  \
      GET_SERIALISER, obj);                                                                   \
  if(GET_SERIALISER.IsWriting())                                                              \
    obj = (inValue);                                                                          \
  GET_SERIALISER.Serialise(STRING_LITERAL(#obj), obj)

// these macros are for use when implementing a DoSerialise function
#define SERIALISE_MEMBER(obj) ser.Serialise(STRING_LITERAL(#obj), el.obj)

#define SERIALISE_MEMBER_TYPED(type, obj) \
  ser.ClearObj(el.obj);                   \
  union                                   \
  {                                       \
    type *t;                              \
    decltype(el.obj) *o;                  \
  } CONCAT(union, __LINE__);              \
  CONCAT(union, __LINE__).o = &el.obj;    \
  ser.template Serialise<type>(STRING_LITERAL(#obj), *CONCAT(union, __LINE__).t)

#define SERIALISE_MEMBER_ARRAY(arrayObj, countObj) \
  ser.Serialise(STRING_LITERAL(#arrayObj), el.arrayObj, el.countObj, SerialiserFlags::AllocateMemory)

#define SERIALISE_MEMBER_ARRAY_TYPED(type, arrayObj, countObj)                                     \
  RDCCOMPILE_ASSERT(sizeof(*el.arrayObj) == sizeof(type),                                          \
                    "Array serialised co-erced type must be identically sized");                   \
  union                                                                                            \
  {                                                                                                \
    type **t;                                                                                      \
    decltype(el.arrayObj) *o;                                                                      \
  } CONCAT(union, __LINE__);                                                                       \
  CONCAT(union, __LINE__).o = &el.arrayObj;                                                        \
  ser.template Serialise<type>(STRING_LITERAL(#arrayObj), *CONCAT(union, __LINE__).t, el.countObj, \
                               SerialiserFlags::AllocateMemory)

// a member that is a pointer and could be NULL, so needs a hidden 'present'
// flag serialised out
#define SERIALISE_MEMBER_OPT(obj) ser.SerialiseNullable(STRING_LITERAL(#obj), el.obj)

// this is used when we want to serialise a member that should not be read from, if we know from
// context that it should always be serialised as NULL. It avoids the need to declare local dummy
// variables. It serialises elements as empty/NULL/etc from a local while writing, and sets to
// default on reading
#define SERIALISE_MEMBER_EMPTY(obj)             \
  {                                             \
    decltype(el.obj) dummy = {};                \
    ser.Serialise(STRING_LITERAL(#obj), dummy); \
    if(ser.IsReading())                         \
      el.obj = decltype(el.obj)();              \
  }
#define SERIALISE_MEMBER_OPT_EMPTY(obj)                 \
  {                                                     \
    decltype(el.obj) dummy = NULL;                      \
    ser.SerialiseNullable(STRING_LITERAL(#obj), dummy); \
    if(ser.IsReading())                                 \
      el.obj = NULL;                                    \
  }
#define SERIALISE_MEMBER_ARRAY_EMPTY(arrayObj)                                                    \
  {                                                                                               \
    decltype(el.arrayObj) dummy = NULL;                                                           \
    uint64_t dummycount = 0;                                                                      \
    ser.Serialise(STRING_LITERAL(#arrayObj), dummy, dummycount, SerialiserFlags::AllocateMemory); \
    if(ser.IsReading())                                                                           \
    {                                                                                             \
      el.arrayObj = NULL;                                                                         \
    }                                                                                             \
  }

// simple utility function for inside serialise functions, to check if the serialiser has hit an
// error and then bail, without trying to replay anything.
#define SERIALISE_CHECK_READ_ERRORS()                                       \
  if(ser.IsReading() && ser.IsErrored())                                    \
  {                                                                         \
    RDCERR("Serialisation failed in '%s'.", ser.GetCurChunkName().c_str()); \
    return false;                                                           \
  }
