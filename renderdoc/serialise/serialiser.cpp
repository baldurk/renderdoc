/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

// used to avoid instantiating templates too early in the header
#define SERIALISER_IMPL

#include "serialiser.h"
#include "core/core.h"
#include "strings/string_utils.h"

#if ENABLED(RDOC_DEVEL)

int64_t Chunk::m_LiveChunks = 0;
int64_t Chunk::m_TotalMem = 0;

#endif

void DumpObject(FileIO::LogFileHandle *log, const rdcstr &indent, SDObject *obj)
{
  if(obj->NumChildren() > 0)
  {
    rdcstr msg =
        StringFormat::Fmt("%s%s%s %s:\n", indent.c_str(), obj->type.name.c_str(),
                          obj->type.basetype == SDBasic::Array ? "[]" : "", obj->name.c_str());
    FileIO::logfile_append(log, msg.c_str(), msg.size());
    for(size_t i = 0; i < obj->NumChildren(); i++)
      DumpObject(log, indent + "  ", obj->GetChild(i));
  }
  else
  {
    rdcstr val;
    switch(obj->type.basetype)
    {
      case SDBasic::Chunk: val = "{Chunk}"; break;
      case SDBasic::Struct: val = "{Struct}"; break;
      case SDBasic::Array:
        // this must be an empty array, or it would have children above
        val = "{}";
        break;
      case SDBasic::Buffer: val = "[buffer]"; break;
      case SDBasic::Null: val = "NULL"; break;
      case SDBasic::String: val = obj->data.str; break;
      case SDBasic::Enum: val = obj->data.str; break;
      case SDBasic::UnsignedInteger: val = ToStr(obj->data.basic.u); break;
      case SDBasic::SignedInteger: val = ToStr(obj->data.basic.i); break;
      case SDBasic::Float: val = ToStr(obj->data.basic.d); break;
      case SDBasic::Boolean: val = ToStr(obj->data.basic.b); break;
      case SDBasic::Character: val = ToStr(obj->data.basic.c); break;
      case SDBasic::Resource: val = ToStr(obj->data.basic.id); break;
    }
    rdcstr msg = StringFormat::Fmt("%s%s %s = %s\n", indent.c_str(), obj->type.name.c_str(),
                                   obj->name.c_str(), val.c_str());
    FileIO::logfile_append(log, msg.c_str(), msg.size());
  }
}

void DumpChunk(bool reading, FileIO::LogFileHandle *log, SDChunk *chunk)
{
  rdcstr msg = StringFormat::Fmt("%s %s @ %llu:\n", reading ? "Read" : "Wrote", chunk->name.c_str(),
                                 chunk->metadata.timestampMicro);
  FileIO::logfile_append(log, msg.c_str(), msg.size());
  DumpObject(log, "  ", chunk);
}

/////////////////////////////////////////////////////////////
// Read Serialiser functions

template <>
Serialiser<SerialiserMode::Reading>::Serialiser(StreamReader *reader, Ownership own,
                                                SDObject *rootStructuredObj)
{
  m_Read = reader;
  m_Write = NULL;

  m_Ownership = own;

  if(rootStructuredObj)
    m_StructureStack.push_back(rootStructuredObj);
}

template <>
Serialiser<SerialiserMode::Reading>::~Serialiser()
{
  if(m_Ownership == Ownership::Stream && m_Read)
    delete m_Read;
}

template <>
uint32_t Serialiser<SerialiserMode::Reading>::BeginChunk(uint32_t, uint64_t)
{
  uint32_t chunkID = 0;

  m_ChunkMetadata = SDChunkMetaData();

  {
    uint32_t c = 0;
    bool success = m_Read->Read(c);

    // Chunk index 0 is not allowed in normal situations, and allows us to indicate some control
    // bytes. Currently this is unused
    RDCASSERT(c != 0 || !success);

    chunkID = c & ChunkIndexMask;

    /////////////////

    m_ChunkMetadata.chunkID = chunkID;

    if(c & ChunkCallstack)
    {
      uint32_t numFrames = 0;
      m_Read->Read(numFrames);

      // try to sanity check the number of frames
      if(numFrames < 4096)
      {
        m_ChunkMetadata.flags |= SDChunkFlags::HasCallstack;

        m_ChunkMetadata.callstack.resize((size_t)numFrames);
        m_Read->Read(m_ChunkMetadata.callstack.data(), m_ChunkMetadata.callstack.byteSize());
      }
      else
      {
        RDCERR("Read invalid number of callstack frames: %u", numFrames);
        // still read the size that we should, even though we expect this to be broken after here
        m_Read->Read(NULL, numFrames * sizeof(uint64_t));
      }
    }

    if(c & ChunkThreadID)
      m_Read->Read(m_ChunkMetadata.threadID);

    if(c & ChunkDuration)
    {
      m_Read->Read(m_ChunkMetadata.durationMicro);
      if(m_TimerFrequency != 1.0)
        m_ChunkMetadata.durationMicro =
            int64_t(double(m_ChunkMetadata.durationMicro) / m_TimerFrequency);
    }

    if(c & ChunkTimestamp)
    {
      m_Read->Read(m_ChunkMetadata.timestampMicro);
      if(m_TimerFrequency != 1.0 || m_TimerBase != 0)
        m_ChunkMetadata.timestampMicro =
            int64_t(double(m_ChunkMetadata.timestampMicro - m_TimerBase) / m_TimerFrequency);
    }

    if(c & Chunk64BitSize)
    {
      m_Read->Read(m_ChunkMetadata.length);
    }
    else
    {
      uint32_t chunkSize = 0;
      m_Read->Read(chunkSize);
      m_ChunkMetadata.length = chunkSize;
    }

    m_LastChunkOffset = m_Read->GetOffset();
  }

  if(ExportStructure())
  {
    rdcstr name = m_ChunkLookup ? m_ChunkLookup(chunkID) : "";

    if(name.empty())
      name = "<Unknown Chunk>";

    SDChunk *chunk = new SDChunk(name.c_str());
    chunk->metadata = m_ChunkMetadata;

    m_StructuredFile->chunks.push_back(chunk);
    m_StructureStack.push_back(chunk);

    m_InternalElement = 0;
  }

  return chunkID;
}

template <>
void Serialiser<SerialiserMode::Reading>::SkipCurrentChunk()
{
  if(ExportStructure())
  {
    RDCASSERTMSG("Skipping chunk after we've begun serialising!", m_StructureStack.size() == 1,
                 m_StructureStack.size());

    SDObject &current = *m_StructureStack.back();

    current.data.basic.numChildren++;
    current.data.children.push_back(new SDObject("Opaque chunk"_lit, "Byte Buffer"_lit));

    SDObject &obj = *current.data.children.back();
    obj.type.basetype = SDBasic::Buffer;
    obj.type.byteSize = m_ChunkMetadata.length;

    if(m_StructureStack.size() == 1)
    {
      SDChunk *chunk = (SDChunk *)m_StructureStack.back();
      chunk->metadata.flags |= SDChunkFlags::OpaqueChunk;
    }
  }

  {
    uint64_t readBytes = m_Read->GetOffset() - m_LastChunkOffset;

    if(readBytes > m_ChunkMetadata.length)
    {
      RDCERR("Can't skip current chunk outside of {BeginChunk, EndChunk}");
      return;
    }

    if(readBytes > 0)
    {
      RDCWARN("Partially consumed bytes at SkipCurrentChunk - blob data will be truncated");
    }

    uint64_t chunkBytes = m_ChunkMetadata.length - readBytes;

    if(ExportStructure() && m_ExportBuffers)
    {
      SDObject &current = *m_StructureStack.back();

      SDObject &obj = *current.data.children.back();

      obj.data.basic.u = m_StructuredFile->buffers.size();

      bytebuf *alloc = new bytebuf;
      alloc->resize((size_t)chunkBytes);
      m_Read->Read(alloc->data(), (size_t)chunkBytes);

      m_StructuredFile->buffers.push_back(alloc);
    }
    else
    {
      m_Read->SkipBytes(chunkBytes);
    }
  }
}

template <>
void Serialiser<SerialiserMode::Reading>::EndChunk()
{
  if(ExportStructure())
  {
    RDCASSERTMSG("Object Stack is imbalanced!", m_StructureStack.size() <= 1,
                 m_StructureStack.size());

    if(!m_StructureStack.empty())
    {
      m_StructureStack.back()->type.byteSize = m_ChunkMetadata.length;
      m_StructureStack.pop_back();
    }

    if(m_DebugDumpLog && !m_StructuredFile->chunks.empty())
    {
      DumpChunk(true, m_DebugDumpLog, m_StructuredFile->chunks.back());
    }
  }

  // only skip remaining bytes if we have a valid length - if we have a length of 0 we wrote this
  // chunk in 'streaming mode' (see SetStreamingMode and the Writing EndChunk() impl) so there's
  // nothing to skip.
  if(m_ChunkMetadata.length > 0 && !m_Read->IsErrored())
  {
    // this will be a no-op if the last chunk length was accurate. If it was a
    // conservative estimate of the length then we'll skip some padding bytes
    uint64_t readBytes = m_Read->GetOffset() - m_LastChunkOffset;

    if(m_ChunkMetadata.length < readBytes)
    {
      RDCERR(
          "!!! "
          "READ %llu BYTES, OVERRUNNING CHUNK LENGTH %u. "
          "CAPTURE IS CORRUPTED, OR REPLAY MISMATCHED CAPTURED CHUNK. "
          "!!!",
          readBytes, m_ChunkMetadata.length);
    }
    else
    {
      m_Read->SkipBytes(size_t(m_ChunkMetadata.length - readBytes));
    }
  }

  // align to the natural chunk alignment
  m_Read->AlignTo<ChunkAlignment>();
}

/////////////////////////////////////////////////////////////
// Write Serialiser functions

template <>
Serialiser<SerialiserMode::Writing>::Serialiser(StreamWriter *writer, Ownership own)
{
  m_Write = writer;
  m_Read = NULL;

  m_Ownership = own;
}

template <>
Serialiser<SerialiserMode::Writing>::~Serialiser()
{
  if(m_Ownership == Ownership::Stream && m_Write)
  {
    m_Write->Finish();
    delete m_Write;
  }
}

template <>
void Serialiser<SerialiserMode::Writing>::SetChunkMetadataRecording(uint32_t flags)
{
  // cannot change this mid-chunk
  RDCASSERT(m_Write->GetOffset() == 0);

  m_ChunkFlags = flags;
}

template <>
uint32_t Serialiser<SerialiserMode::Writing>::BeginChunk(uint32_t chunkID, uint64_t byteLength)
{
  {
    // chunk index needs to be valid
    RDCASSERT(chunkID > 0);

    {
      uint32_t c = chunkID & ChunkIndexMask;
      RDCASSERT(chunkID <= ChunkIndexMask);

      c |= m_ChunkFlags;
      if(byteLength > 0xffffffff)
        c |= Chunk64BitSize;

      m_ChunkMetadata.chunkID = chunkID;

      /////////////////

      m_Write->Write(c);

      if(c & ChunkCallstack)
      {
        if(m_ChunkMetadata.callstack.empty())
        {
          bool collect = RenderDoc::Inst().GetCaptureOptions().captureCallstacks;

          if(RenderDoc::Inst().GetCaptureOptions().captureCallstacksOnlyDraws)
            collect = collect && m_DrawChunk;

          if(collect)
          {
            Callstack::Stackwalk *stack = Callstack::Collect();
            if(stack && stack->NumLevels() > 0)
            {
              m_ChunkMetadata.callstack.assign(stack->GetAddrs(), stack->NumLevels());
            }

            SAFE_DELETE(stack);
          }
        }

        m_ChunkMetadata.flags |= SDChunkFlags::HasCallstack;

        uint32_t numFrames = (uint32_t)m_ChunkMetadata.callstack.size();
        m_Write->Write(numFrames);

        m_Write->Write(m_ChunkMetadata.callstack.data(), m_ChunkMetadata.callstack.byteSize());
      }

      if(c & ChunkThreadID)
      {
        if(m_ChunkMetadata.threadID == 0)
          m_ChunkMetadata.threadID = Threading::GetCurrentID();

        m_Write->Write(m_ChunkMetadata.threadID);
      }

      if(c & ChunkDuration)
      {
        if(m_ChunkMetadata.durationMicro < 0)
          m_ChunkMetadata.durationMicro = 0;
        m_Write->Write(m_ChunkMetadata.durationMicro);
      }

      if(c & ChunkTimestamp)
      {
        if(m_ChunkMetadata.timestampMicro == 0)
          m_ChunkMetadata.timestampMicro = Timing::GetTick();

        m_Write->Write(m_ChunkMetadata.timestampMicro);
      }

      if(byteLength > 0 || m_DataStreaming)
      {
        // write length, assuming it is an upper bound
        m_ChunkFixup = 0;
        RDCASSERT(byteLength < 0x100000000 || (c & Chunk64BitSize) != 0);
        if(c & Chunk64BitSize)
        {
          m_Write->Write(byteLength);
        }
        else
        {
          m_Write->Write(uint32_t(byteLength & 0xffffffff));
        }
        m_LastChunkOffset = m_Write->GetOffset();
        m_ChunkMetadata.length = byteLength;
      }
      else
      {
        // length will be fixed up in EndChunk
        // assume that this case will not produce chunks with size larger than can fit in 32 bit
        // value
        uint32_t chunkSize = 0xbeebfeed;
        m_ChunkFixup = m_Write->GetOffset();
        m_Write->Write(chunkSize);
      }
    }
  }

  if(ExportStructure())
  {
    rdcstr name = m_ChunkLookup ? m_ChunkLookup(chunkID) : "";

    if(name.empty())
      name = "<Unknown Chunk>";

    SDChunk *chunk = new SDChunk(name.c_str());
    chunk->metadata = m_ChunkMetadata;

    m_StructuredFile->chunks.push_back(chunk);
    m_StructureStack.push_back(chunk);

    m_InternalElement = 0;
  }

  return chunkID;
}

template <>
void Serialiser<SerialiserMode::Writing>::EndChunk()
{
  m_DrawChunk = false;

  if(m_DataStreaming)
  {
    // nothing to fixup, length is unused
  }
  else if(m_ChunkFixup != 0)
  {
    // fix up the chunk header
    uint64_t chunkOffset = m_ChunkFixup;
    m_ChunkFixup = 0;

    uint64_t curOffset = m_Write->GetOffset();

    RDCASSERT(curOffset > chunkOffset);

    uint64_t chunkLength = (curOffset - chunkOffset) - sizeof(uint32_t);
    if(chunkLength > 0xffffffff)
    {
      RDCERR("!!! CHUNK LENGTH %llu EXCEEDED 32 BIT VALUE. CAPTURE WILL BE CORRUPTED. !!!",
             chunkLength);
    }

    m_Write->WriteAt(chunkOffset, uint32_t(chunkLength & 0xffffffff));

    m_ChunkMetadata.length = chunkLength;
  }
  else
  {
    uint64_t writtenLength = (m_Write->GetOffset() - m_LastChunkOffset);

    if(writtenLength < m_ChunkMetadata.length)
    {
      uint64_t numPadBytes = m_ChunkMetadata.length - writtenLength;

      // need to write some padding bytes so that the length is accurate
      for(uint64_t i = 0; i < numPadBytes; i++)
      {
        byte padByte = 0xbb;
        m_Write->Write(padByte);
      }

      RDCDEBUG("Chunk estimated at %llu bytes, actual length %llu. Added %llu bytes padding.",
               m_ChunkMetadata.length, writtenLength, numPadBytes);
    }
    else if(writtenLength > m_ChunkMetadata.length)
    {
      RDCERR(
          "!!! "
          "ESTIMATED UPPER BOUND CHUNK LENGTH %llu EXCEEDED: %llu. "
          "CAPTURE WILL BE CORRUPTED. "
          "!!!",
          m_ChunkMetadata.length, writtenLength);
    }
    else
    {
      RDCDEBUG("Chunk was exactly the estimate of %llu bytes.", m_ChunkMetadata.length);
    }
  }

  if(ExportStructure())
  {
    RDCASSERTMSG("Object Stack is imbalanced!", m_StructureStack.size() <= 1,
                 m_StructureStack.size());

    if(!m_StructureStack.empty())
    {
      m_StructureStack.back()->type.byteSize = m_ChunkMetadata.length;
      m_StructureStack.pop_back();
    }

    if(m_DebugDumpLog && !m_StructuredFile->chunks.empty())
    {
      DumpChunk(false, m_DebugDumpLog, m_StructuredFile->chunks.back());
    }
  }

  // align to the natural chunk alignment
  m_Write->AlignTo<ChunkAlignment>();

  m_ChunkMetadata = SDChunkMetaData();

  m_Write->Flush();
}

template <>
void Serialiser<SerialiserMode::Writing>::WriteStructuredFile(const SDFile &file,
                                                              RENDERDOC_ProgressCallback progress)
{
  Serialiser<SerialiserMode::Writing> scratchWriter(
      new StreamWriter(StreamWriter::DefaultScratchSize), Ownership::Stream);

  // slightly cheeky to cast away the const, but we don't modify it in a writing serialiser
  scratchWriter.m_StructuredFile = m_StructuredFile = (SDFile *)&file;

  for(size_t i = 0; i < file.chunks.size(); i++)
  {
    const SDChunk &chunk = *file.chunks[i];

    m_ChunkMetadata = chunk.metadata;

    m_ChunkFlags = 0;

    if(m_ChunkMetadata.flags & SDChunkFlags::HasCallstack)
      m_ChunkFlags |= ChunkCallstack;

    if(m_ChunkMetadata.threadID != 0)
      m_ChunkFlags |= ChunkThreadID;

    if(m_ChunkMetadata.durationMicro >= 0)
      m_ChunkFlags |= ChunkDuration;

    if(m_ChunkMetadata.timestampMicro != 0)
      m_ChunkFlags |= ChunkTimestamp;

    Serialiser<SerialiserMode::Writing> *ser = this;

    if(m_ChunkMetadata.length == 0)
    {
      ser = &scratchWriter;
      scratchWriter.m_ChunkMetadata = m_ChunkMetadata;
      scratchWriter.m_ChunkFlags = m_ChunkFlags;
    }

    ser->BeginChunk(m_ChunkMetadata.chunkID, m_ChunkMetadata.length);

    if(chunk.metadata.flags & SDChunkFlags::OpaqueChunk)
    {
      RDCASSERT(chunk.data.children.size() == 1);

      size_t bufID = (size_t)chunk.data.children[0]->data.basic.u;
      byte *ptr = m_StructuredFile->buffers[bufID]->data();
      size_t len = m_StructuredFile->buffers[bufID]->size();

      ser->GetWriter()->Write(ptr, len);
    }
    else
    {
      for(size_t o = 0; o < chunk.data.children.size(); o++)
      {
        // note, we don't need names because we aren't exporting structured data
        ser->Serialise(""_lit, chunk.data.children[o]);
      }
    }

    ser->EndChunk();

    if(m_ChunkMetadata.length == 0)
    {
      m_Write->Write(scratchWriter.GetWriter()->GetData(), scratchWriter.GetWriter()->GetOffset());
      scratchWriter.GetWriter()->Rewind();
    }

    if(progress)
      progress(float(i) / float(file.chunks.size()));
  }

  if(progress)
    progress(1.0f);

  m_StructuredFile = &m_StructData;
  scratchWriter.m_StructuredFile = &scratchWriter.m_StructData;
}

template <>
rdcstr DoStringise(const SDTypeFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(SDTypeFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE(NoFlags);

    STRINGISE_BITFIELD_CLASS_BIT(HasCustomString);
    STRINGISE_BITFIELD_CLASS_BIT(Hidden);
    STRINGISE_BITFIELD_CLASS_BIT(Nullable);
    STRINGISE_BITFIELD_CLASS_BIT(NullString);
    STRINGISE_BITFIELD_CLASS_BIT(FixedArray);
    STRINGISE_BITFIELD_CLASS_BIT(Union);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const SDChunkFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(SDChunkFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE(NoFlags);

    STRINGISE_BITFIELD_CLASS_BIT(OpaqueChunk);
    STRINGISE_BITFIELD_CLASS_BIT(HasCallstack);
  }
  END_BITFIELD_STRINGISE();
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDType &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(basetype);
  SERIALISE_MEMBER(flags);
  SERIALISE_MEMBER(byteSize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDChunkMetaData &el)
{
  SERIALISE_MEMBER(chunkID);
  SERIALISE_MEMBER(flags);
  SERIALISE_MEMBER(length);
  SERIALISE_MEMBER(threadID);
  SERIALISE_MEMBER(durationMicro);
  SERIALISE_MEMBER(timestampMicro);
  SERIALISE_MEMBER(callstack);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDObjectPODData &el)
{
  SERIALISE_MEMBER(u);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, StructuredObjectList &el)
{
  // since structured objects aren't intended to be exported as nice structured data, only for pure
  // transfer purposes, we don't make a proper array here and instead just manually serialise count
  // + elements
  uint64_t count = el.size();
  ser.Serialise("count"_lit, count);

  if(ser.IsReading())
    el.resize((size_t)count);

  for(size_t c = 0; c < (size_t)count; c++)
  {
    // we also assume that the caller serialising these objects will handle lifetime management.
    if(ser.IsReading())
      el[c] = new SDObject("", "");

    ser.Serialise("$el"_lit, *el[c]);
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDObjectData &el)
{
  SERIALISE_MEMBER(basic);
  SERIALISE_MEMBER(str);
  SERIALISE_MEMBER(children);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDObject &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(data);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDChunk &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(data);
  SERIALISE_MEMBER(metadata);
}

INSTANTIATE_SERIALISE_TYPE(SDChunk);

// serialise the pointer version - special case for writing a structured file, so can assume writing
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, SDObject *el)
{
  // clang barfs if we try to do ser.IsWriting() here for some reason, claiming it isn't static and
  // const enough for static_assert. As a workaround we call IsWriting as a static member of the
  // type itself, and that works.
  RDCCOMPILE_ASSERT(SerialiserType::IsWriting(),
                    "SDObject pointer only supported for writing serialisation");
  if(el->type.flags & SDTypeFlags::Nullable)
  {
    bool present = el->type.basetype != SDBasic::Null;
    ser.Serialise(""_lit, present);
  }

  const SDFile &file = ser.GetStructuredFile();

  switch(el->type.basetype)
  {
    case SDBasic::Chunk: RDCERR("Unexpected chunk inside object!"); break;
    case SDBasic::Struct:
      for(size_t o = 0; o < el->data.children.size(); o++)
        ser.Serialise(""_lit, el->data.children[o]);
      break;
    case SDBasic::Array: ser.Serialise(""_lit, (rdcarray<SDObject *> &)el->data.children); break;
    case SDBasic::Null:
      // nothing to do, we serialised present flag above
      RDCASSERT(el->type.flags & SDTypeFlags::Nullable);
      break;
    case SDBasic::Buffer:
    {
      size_t bufID = (size_t)el->data.basic.u;
      byte *buf = file.buffers[bufID]->data();
      uint64_t size = file.buffers[bufID]->size();
      ser.Serialise(""_lit, buf, size);
      break;
    }
    case SDBasic::String:
    {
      if(el->type.flags & SDTypeFlags::NullString)
      {
        const char *nullstring = NULL;
        ser.Serialise(""_lit, nullstring);
      }
      else
      {
        ser.Serialise(""_lit, el->data.str);
      }
      break;
    }
    case SDBasic::Enum:
    {
      uint32_t e = (uint32_t)el->data.basic.u;
      ser.Serialise(""_lit, e);
      break;
    }
    case SDBasic::Boolean: ser.Serialise(""_lit, el->data.basic.b); break;
    case SDBasic::Character: ser.Serialise(""_lit, el->data.basic.c); break;
    case SDBasic::Resource: ser.Serialise(""_lit, el->data.basic.id); break;
    case SDBasic::UnsignedInteger:
      if(el->type.byteSize == 1)
      {
        uint8_t u = uint8_t(el->data.basic.u);
        ser.Serialise(""_lit, u);
      }
      else if(el->type.byteSize == 2)
      {
        uint16_t u = uint16_t(el->data.basic.u);
        ser.Serialise(""_lit, u);
      }
      else if(el->type.byteSize == 4)
      {
        uint32_t u = uint32_t(el->data.basic.u);
        ser.Serialise(""_lit, u);
      }
      else if(el->type.byteSize == 8)
      {
        ser.Serialise(""_lit, el->data.basic.u);
      }
      else
      {
        RDCERR("Unexpeted integer size %u", el->type.byteSize);
      }
      break;
    case SDBasic::SignedInteger:
      if(el->type.byteSize == 1)
      {
        int8_t i = int8_t(el->data.basic.i);
        ser.Serialise(""_lit, i);
      }
      else if(el->type.byteSize == 2)
      {
        int16_t i = int16_t(el->data.basic.i);
        ser.Serialise(""_lit, i);
      }
      else if(el->type.byteSize == 4)
      {
        int32_t i = int32_t(el->data.basic.i);
        ser.Serialise(""_lit, i);
      }
      else if(el->type.byteSize == 8)
      {
        ser.Serialise(""_lit, el->data.basic.i);
      }
      else
      {
        RDCERR("Unexpeted integer size %u", el->type.byteSize);
      }
      break;
    case SDBasic::Float:
      if(el->type.byteSize == 4)
      {
        float f = float(el->data.basic.d);
        ser.Serialise(""_lit, f);
      }
      else if(el->type.byteSize == 8)
      {
        ser.Serialise(""_lit, el->data.basic.d);
      }
      else
      {
        RDCERR("Unexpeted float size %u", el->type.byteSize);
      }
      break;
  }
}

/////////////////////////////////////////////////////////////
// Basic types

template <>
rdcstr DoStringise(const rdcstr &el)
{
  return el;
}

template <>
rdcstr DoStringise(void *const &el)
{
  return StringFormat::Fmt("%#p", el);
}

template <>
rdcstr DoStringise(const int64_t &el)
{
  return StringFormat::Fmt("%lld", el);
}

#if ENABLED(RDOC_SIZET_SEP_TYPE)
template <>
rdcstr DoStringise(const size_t &el)
{
  return StringFormat::Fmt("%llu", (uint64_t)el);
}
#endif

template <>
rdcstr DoStringise(const uint64_t &el)
{
  return StringFormat::Fmt("%llu", el);
}

template <>
rdcstr DoStringise(const uint32_t &el)
{
  return StringFormat::Fmt("%u", el);
}

template <>
rdcstr DoStringise(const char &el)
{
  return StringFormat::Fmt("'%c'", el);
}

template <>
rdcstr DoStringise(const wchar_t &el)
{
  return StringFormat::Fmt("'%lc'", el);
}

template <>
rdcstr DoStringise(const byte &el)
{
  return StringFormat::Fmt("%hhu", el);
}

template <>
rdcstr DoStringise(const uint16_t &el)
{
  return StringFormat::Fmt("%hu", el);
}

template <>
rdcstr DoStringise(const int32_t &el)
{
  return StringFormat::Fmt("%d", el);
}

template <>
rdcstr DoStringise(const int16_t &el)
{
  return StringFormat::Fmt("%hd", el);
}

template <>
rdcstr DoStringise(const float &el)
{
  return StringFormat::Fmt("%0.4f", el);
}

template <>
rdcstr DoStringise(const double &el)
{
  return StringFormat::Fmt("%0.4lf", el);
}

template <>
rdcstr DoStringise(const bool &el)
{
  if(el)
    return "True";

  return "False";
}

Chunk *Chunk::Create(Serialiser<SerialiserMode::Writing> &ser, uint16_t chunkType,
                     ChunkAllocator *allocator)
{
  RDCCOMPILE_ASSERT(sizeof(Chunk) <= 16, "Chunk should be no more than 16 bytes");

  RDCASSERT(ser.GetWriter()->GetOffset() < 0xffffffff);
  uint32_t length = (uint32_t)ser.GetWriter()->GetOffset();

  byte *data = NULL;
  if(allocator)
  {
    // try to allocate from the allocator
    data = allocator->AllocAlignedBuffer(length);

    // if we couldn't satisfy the allocation then pretend we never had an allocator in the first
    // place. We'll externally allocate the chunk and the data.
    if(!data)
      allocator = NULL;
  }

  // if we don't have an allocator or we gave up on it above, allocate the data externally
  if(!allocator)
    data = AllocAlignedBuffer(length);

  memcpy(data, ser.GetWriter()->GetData(), (size_t)length);

  ser.GetWriter()->Rewind();

  Chunk *ret = NULL;

  // if allocator wasn't NULL'd above, use it to allocate the chunk as well. We always either
  // allocate both chunk and data from the allocator (so we don't have anything to do on
  // destruction) or neither. Otherwise if we allocated the chunk from the allocator and the data
  // externally, our data pointer might be corrupted or the bool indicating that it's external.
  // Consider the case where we allocate some chunks from an allocator - one of which allocated
  // external data - then the allocator is reset. Now the chunk could be overwritten by subsequent
  // recording before it is deleted. We don't want the allocator to have to explicitly delete all
  // chunks that were allocated from it and external data allocations should be rare (only really
  // massive chunks bigger than a page) so we can afford to externally allocate the chunk too.
  if(allocator)
    ret = new(allocator->AllocChunk()) Chunk(true);
  else
    ret = new Chunk(false);

  ret->m_Length = length;
  ret->m_ChunkType = chunkType;
  ret->m_Data = data;

#if ENABLED(RDOC_DEVEL)
  Atomic::Inc64(&m_LiveChunks);
  Atomic::ExchAdd64(&m_TotalMem, int64_t(length));
#endif

  return ret;
}

ChunkAllocator::~ChunkAllocator()
{
  for(Page &p : freePages)
  {
    FreeAlignedBuffer(p.chunkBase);
    FreeAlignedBuffer(p.bufferBase);
  }

  for(Page &p : fullPages)
  {
    FreeAlignedBuffer(p.chunkBase);
    FreeAlignedBuffer(p.bufferBase);
  }
}

byte *ChunkAllocator::AllocAlignedBuffer(uint64_t size)
{
  // always allocate 64-bytes at a time even if the size is smaller
  return AllocateFromPages(false, AlignUp((size_t)size, (size_t)64));
}

byte *ChunkAllocator::AllocChunk()
{
  RDCCOMPILE_ASSERT(sizeof(ChunkAllocator) <= 128, "foo");
  return AllocateFromPages(true, sizeof(Chunk));
}

void ChunkAllocator::Trim()
{
  for(Page &p : freePages)
  {
    FreeAlignedBuffer(p.chunkBase);
    FreeAlignedBuffer(p.bufferBase);
  }

  freePages.clear();
}

void ChunkAllocator::Reset()
{
  freePages.append(fullPages);
  fullPages.clear();

  for(Page &p : freePages)
  {
    p.bufferHead = p.bufferBase;
    p.chunkHead = p.chunkBase;
  }
}

void ChunkAllocator::ResetPageSet(const rdcarray<uint32_t> &pages)
{
  // any full pages in this set go back into the free pages list
  for(size_t i = 0; i < fullPages.size();)
  {
    Page &p = fullPages[i];
    if(pages.contains(p.ID))
    {
      p.bufferHead = p.bufferBase;
      p.chunkHead = p.chunkBase;
      freePages.push_back(p);
      fullPages.erase(i);
      continue;
    }
    i++;
  }
}

rdcarray<uint32_t> ChunkAllocator::GetPageSet()
{
  // see if the last free page has been partially used
  if(!freePages.empty())
  {
    Page &p = freePages.back();

    if(p.bufferBase != p.bufferHead || p.chunkBase != p.chunkHead)
    {
      usedPages.push_back(freePages.back().ID);

      fullPages.push_back(freePages.back());
      freePages.pop_back();
    }
  }

  rdcarray<uint32_t> ret;
  ret.swap(usedPages);
  return ret;
}

byte *ChunkAllocator::AllocateFromPages(bool chunkAlloc, size_t size)
{
  // if the size can't be satisfied in a page, return NULL and we'll force a full allocation which
  // will be freed on its own
  if(size > BufferPageSize)
    return NULL;

  if(!freePages.empty())
  {
    // if the last free page can't satisfy this allocation, retire it to the full list
    if((chunkAlloc && GetRemainingChunkBytes(freePages.back()) < size) ||
       (!chunkAlloc && GetRemainingBufferBytes(freePages.back()) < size))
    {
      // mark this page as used in the current set
      usedPages.push_back(freePages.back().ID);

      fullPages.push_back(freePages.back());
      freePages.pop_back();
    }
  }

  // if there are no free pages, allocate a new one
  if(freePages.empty())
  {
    // the first free ID is the sum of the free and full lists, because all pages are in one or the
    // other
    uint32_t ID = uint32_t(freePages.size() + fullPages.size());
    byte *buffers = ::AllocAlignedBuffer(BufferPageSize);
    byte *chunks = ::AllocAlignedBuffer(ChunkPageSize);
    freePages.push_back({ID, buffers, buffers, chunks, chunks});
  }

  Page &p = freePages.back();

  byte *ret = NULL;

  if(chunkAlloc)
  {
    ret = p.chunkHead;
    p.chunkHead += size;
  }
  else
  {
    ret = p.bufferHead;
    p.bufferHead += size;
  }

  return ret;
}
