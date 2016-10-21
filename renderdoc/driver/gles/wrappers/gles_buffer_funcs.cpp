/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "../gles_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"

#pragma region Buffers

bool WrappedGLES::Serialise_glGenBuffers(GLsizei n, GLuint *buffers)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), *buffers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenBuffers(1, &real);

    GLResource res = BufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);

    m_Buffers[live].resource = res;
    m_Buffers[live].curType = eGL_NONE;
  }

  return true;
}

void WrappedGLES::glGenBuffers(GLsizei n, GLuint *buffers)
{
  m_Real.glGenBuffers(n, buffers);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = BufferRes(GetCtx(), buffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_BUFFER);
        Serialise_glGenBuffers(1, buffers + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
      m_Buffers[id].resource = res;
      m_Buffers[id].curType = eGL_NONE;
    }
  }
}

bool WrappedGLES::Serialise_glBindBuffer(GLenum target, GLuint buffer)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(ResourceId, Id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))
                                            : ResourceId()));

  if(m_State >= WRITING)
  {
    if(Id != ResourceId())
      GetResourceManager()->GetResourceRecord(Id)->datatype = Target;
  }
  else if(m_State < WRITING)
  {
    if(Id == ResourceId())
    {
      m_Real.glBindBuffer(Target, 0);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(Id);
      m_Real.glBindBuffer(Target, res.name);

      ResourceId liveId = GetResourceManager()->GetLiveID(Id);
      m_Buffers[liveId].curType = Target;

      // PEPE: Doing the same aproach as at the textures. The ResourceId of the bound buffer is
      // saved in the chunk so it is no longer needed to track the bindings in read mode.
      // GetCtxData().m_BufferRecord[BufferIdx(Target)] = GetResourceManager()->GetResourceRecord(liveId);
    }
  }

  return true;
}

void WrappedGLES::glBindBuffer(GLenum target, GLuint buffer)
{
  m_Real.glBindBuffer(target, buffer);

  ContextData &cd = GetCtxData();

  size_t idx = BufferIdx(target);

  if(m_State == WRITING_CAPFRAME)
  {
    Chunk *chunk = NULL;

    if(buffer == 0)
      cd.m_BufferRecord[idx] = NULL;
    else
      cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    {
      SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
      Serialise_glBindBuffer(target, buffer);

      chunk = scope.Get();
    }

    if(buffer)
    {
      FrameRefType refType = eFrameRef_Read;

      // these targets write to the buffer
      if(target == eGL_ATOMIC_COUNTER_BUFFER || target == eGL_COPY_WRITE_BUFFER ||
         target == eGL_PIXEL_PACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
         target == eGL_TRANSFORM_FEEDBACK_BUFFER)
        refType = eFrameRef_ReadBeforeWrite;

      GetResourceManager()->MarkResourceFrameReferenced(cd.m_BufferRecord[idx]->GetResourceID(),
                                                        refType);
    }

    m_ContextRecord->AddChunk(chunk);
  }

  if(buffer == 0)
  {
    cd.m_BufferRecord[idx] = NULL;
    return;
  }

  if(m_State >= WRITING)
  {
    GLResourceRecord *r = cd.m_BufferRecord[idx] =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    // it's legal to re-type buffers, generate another BindBuffer chunk to rename
    if(r->datatype != target)
    {
      Chunk *chunk = NULL;

      r->LockChunks();
      for(;;)
      {
        Chunk *end = r->GetLastChunk();

        if(end->GetChunkType() == BIND_BUFFER)
        {
          SAFE_DELETE(end);

          r->PopChunk();

          continue;
        }

        break;
      }
      r->UnlockChunks();

      {
        SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
        Serialise_glBindBuffer(target, buffer);

        r->AddChunk(scope.Get());
      }

    }


    // element array buffer binding is vertex array record state, record there (if we've not just
    // stopped)
    if(m_State == WRITING_IDLE && (target == eGL_ELEMENT_ARRAY_BUFFER)
        && RecordUpdateCheck(cd.m_VertexArrayRecord))
    {
      GetResourceManager()->MarkDirtyResource(cd.m_VertexArrayRecord->GetResourceID());
    }

    // store as transform feedback record state
    if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GetResourceManager()->MarkDirtyResource(cd.m_FeedbackRecord->GetResourceID());
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
       target == eGL_ATOMIC_COUNTER_BUFFER)
    {
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(r->GetResourceID());
      else
        m_MissingTracks.insert(r->GetResourceID());
    }
  }
  else
  {
    m_Buffers[GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))].curType = target;
  }
}

bool WrappedGLES::Serialise_glBufferStorageEXT(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveBufferRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);

  // for satisfying GL_MIN_MAP_BUFFER_ALIGNMENT
  m_pSerialiser->AlignNextBuffer(64);

  SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

  uint64_t offs = m_pSerialiser->GetOffset();

  SERIALISE_ELEMENT(uint32_t, Flags, flags);

  if(m_State < WRITING)
  {
    SafeBufferBinder safeBufferBinder(m_Real, Target, GetResourceManager()->GetLiveResource(id).name);
    Compat_glBufferStorageEXT(Target, (GLsizeiptr)Bytesize, bytes, Flags);
    m_Buffers[GetResourceManager()->GetLiveID(id)].size = Bytesize;
    SAFE_DELETE_ARRAY(bytes);
  }
  else if(m_State >= WRITING)
  {
    GetResourceManager()->GetResourceRecord(id)->SetDataOffset(offs - Bytesize);
  }
  return true;
}

void WrappedGLES::glBufferStorageEXT(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
  byte *dummy = NULL;

  if(m_State >= WRITING && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, 0xdd, size);
    data = dummy;
  }

  Compat_glBufferStorageEXT(target, size, data, flags);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveBufferRecord(target);
    if (record == NULL)
      RDCERR("Calling non-DSA buffer function with no buffer bound to active slot");

    RDCASSERT(record);

    SCOPED_SERIALISE_CONTEXT(BUFFERSTORAGE);
    Serialise_glBufferStorageEXT(target, size, data, flags);

    Chunk *chunk = scope.Get();

    {
      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
      record->Length = (int32_t)size;
      record->DataInSerialiser = true;
    }

    // We immediately map the whole range with appropriate flags, to be copied into whenever we
    // need to propogate changes. Note: Coherent buffers are not mapped coherent, but this is
    // because the user code isn't writing into them anyway and we're inserting invisible sync
    // points - so there's no need for it to be coherently mapped (and there's no requirement
    // that a buffer declared as coherent must ALWAYS be mapped as coherent).
    if(flags & eGL_MAP_PERSISTENT_BIT_EXT)
    {
      record->Map.persistentPtr = (byte *)Compat_glMapBufferRangeEXT(
          target, 0, size,
          eGL_MAP_WRITE_BIT | eGL_MAP_FLUSH_EXPLICIT_BIT | eGL_MAP_PERSISTENT_BIT_EXT);
      RDCASSERT(record->Map.persistentPtr);

      // persistent maps always need both sets of shadow storage, so allocate up front.
      record->AllocShadowStorage(size);
    }
  }
  else
  {
    GLint id;
    m_Real.glGetIntegerv(BufferBinding(target), &id);
    m_Buffers[GetResourceManager()->GetID(BufferRes(GetCtx(), id))].size = size;
  }
  SAFE_DELETE_ARRAY(dummy);
}

bool WrappedGLES::Serialise_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveBufferRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);

  // for satisfying GL_MIN_MAP_BUFFER_ALIGNMENT
  m_pSerialiser->AlignNextBuffer(64);

  SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

  uint64_t offs = m_pSerialiser->GetOffset();

  SERIALISE_ELEMENT(GLenum, Usage, usage);

  if(m_State < WRITING)
  {
    SafeBufferBinder safeBufferBinder(m_Real, Target, GetResourceManager()->GetLiveResource(id).name);
    m_Real.glBufferData(Target, (GLsizeiptr)Bytesize, bytes, Usage);
    m_Buffers[GetResourceManager()->GetLiveID(id)].size = Bytesize;

    SAFE_DELETE_ARRAY(bytes);
  }
  else if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    record->DataInSerialiser = true;
    record->SetDataOffset(offs - Bytesize);
  }

  return true;
}

void WrappedGLES::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
  byte *dummy = NULL;

  if(m_State >= WRITING && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, 0xdd, size);
    data = dummy;
  }

  m_Real.glBufferData(target, size, data, usage);

  size_t idx = BufferIdx(target);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[idx];
    RDCASSERT(record);

    // detect buffer orphaning and just update backing store
    if(m_State == WRITING_IDLE && record->HasDataPtr() && size == (GLsizeiptr)record->Length &&
       usage == record->usage)
    {
      if(data)
        memcpy(record->GetDataPtr(), data, (size_t)size);
      else
        memset(record->GetDataPtr(), 0xbe, (size_t)size);

      SAFE_DELETE_ARRAY(dummy);

      return;
    }

    GLuint buffer = record->Resource.name;

    // if we're recreating the buffer, clear the record and add new chunks. Normally
    // we would just mark this record as dirty and pick it up on the capture frame as initial
    // data, but we don't support (if it's even possible) querying out size etc.
    // we need to add only the chunks required - glGenBuffers, glBindBuffer to current target,
    // and this buffer storage. All other chunks have no effect
    if(m_State == WRITING_IDLE &&
       (record->HasDataPtr() || (record->Length > 0 && size != (GLsizeiptr)record->Length)))
    {
      // we need to maintain chunk ordering, so fetch the first two chunk IDs.
      // We should have at least two by this point - glGenBuffers and whatever gave the record
      // a size before.
      RDCASSERT(record->NumChunks() >= 2);

      // remove all but the first two chunks
      while(record->NumChunks() > 2)
      {
        Chunk *c = record->GetLastChunk();
        SAFE_DELETE(c);
        record->PopChunk();
      }

      int32_t id2 = record->GetLastChunkID();
      {
        Chunk *c = record->GetLastChunk();
        SAFE_DELETE(c);
        record->PopChunk();
      }

      int32_t id1 = record->GetLastChunkID();
      {
        Chunk *c = record->GetLastChunk();
        SAFE_DELETE(c);
        record->PopChunk();
      }

      RDCASSERT(!record->HasChunks());

      // add glGenBuffers chunk
      {
        SCOPED_SERIALISE_CONTEXT(GEN_BUFFER);
        Serialise_glGenBuffers(1, &buffer);

        record->AddChunk(scope.Get(), id1);
      }

      // add glBindBuffer chunk
      {
        SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
        Serialise_glBindBuffer(record->datatype, buffer);

        record->AddChunk(scope.Get(), id2);
      }

      // we're about to add the buffer data chunk
    }

    SCOPED_SERIALISE_CONTEXT(BUFFERDATA);
    Serialise_glBufferData(target, size, data, usage);

    Chunk *chunk = scope.Get();

    // if we've already created this is a renaming/data updating call. It should go in
    // the frame record so we can 'update' the buffer as it goes in the frame.
    // if we haven't created the buffer at all, it could be a mid-frame create and we
    // should place it in the resource record, to happen before the frame.
    if(m_State == WRITING_CAPFRAME && record->GetDataPtr())
    {
      // we could perhaps substitute this for a 'fake' glBufferSubData chunk?
      m_ContextRecord->AddChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Write);
    }
    else
    {
      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
      record->Length = size;
      record->usage = usage;
      record->DataInSerialiser = true;
    }
  }

  SAFE_DELETE_ARRAY(dummy);
}

bool WrappedGLES::Serialise_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveBufferRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
  SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);
  SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

  if(m_State < WRITING)
  {
    SafeBufferBinder safeBufferBinder(m_Real, Target, GetResourceManager()->GetLiveResource(id).name);
    m_Real.glBufferSubData(Target, (GLintptr)Offset, (GLsizeiptr)Bytesize, bytes);

    SAFE_DELETE_ARRAY(bytes);
  }

  return true;
}

void WrappedGLES::glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
  m_Real.glBufferSubData(target, offset, size, data);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveBufferRecord(target);
    if (record == NULL)
      RDCERR("Calling non-DSA buffer function with no buffer bound to active slot");

    RDCASSERT(record);
    if (record)
    {
      GLResource res = record->Resource;

      if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
         m_State != WRITING_CAPFRAME)
        return;

      SCOPED_SERIALISE_CONTEXT(BUFFERSUBDATA);
      Serialise_glBufferSubData(target, offset, size, data);

      Chunk *chunk = scope.Get();

      if(m_State == WRITING_CAPFRAME)
      {
        m_ContextRecord->AddChunk(chunk);
        m_MissingTracks.insert(record->GetResourceID());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_ReadBeforeWrite);
      }
      else
      {
        record->AddChunk(chunk);
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
  }
}

bool WrappedGLES::Serialise_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                                                GLintptr readOffset, GLintptr writeOffset,
                                                GLsizeiptr size)
{
  SERIALISE_ELEMENT(ResourceId, readid, GetCtxData().GetActiveBufferRecord(readTarget)->GetResourceID());
  SERIALISE_ELEMENT(ResourceId, writeid, GetCtxData().GetActiveBufferRecord(writeTarget)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, ReadTarget, readTarget);
  SERIALISE_ELEMENT(GLenum, WriteTarget, writeTarget);
  SERIALISE_ELEMENT(uint64_t, ReadOffset, (uint64_t)readOffset);
  SERIALISE_ELEMENT(uint64_t, WriteOffset, (uint64_t)writeOffset);
  SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);

  if(m_State < WRITING)
  {
    SafeBufferBinder safeBufferBinderRead(m_Real, ReadTarget, GetResourceManager()->GetLiveResource(readid).name);
    SafeBufferBinder safeBufferBinderWrite(m_Real, WriteTarget, GetResourceManager()->GetLiveResource(writeid).name);
    m_Real.glCopyBufferSubData(ReadTarget, WriteTarget, (GLintptr)ReadOffset,
                              (GLintptr)WriteOffset, (GLsizeiptr)Bytesize);
  }

  return true;
}


void WrappedGLES::glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                                        GLintptr writeOffset, GLsizeiptr size)
{
  CoherentMapImplicitBarrier();

  m_Real.glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);

  if(m_State >= WRITING)
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *readrecord = cd.GetActiveBufferRecord(readTarget);
    GLResourceRecord *writerecord = cd.GetActiveBufferRecord(writeTarget);
    RDCASSERT(readrecord && writerecord);

    if(m_HighTrafficResources.find(writerecord->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    if(GetResourceManager()->IsResourceDirty(readrecord->GetResourceID()) &&
       m_State != WRITING_CAPFRAME)
    {
      m_HighTrafficResources.insert(writerecord->GetResourceID());
      GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
      return;
    }

    SCOPED_SERIALISE_CONTEXT(COPYBUFFERSUBDATA);
    Serialise_glCopyBufferSubData(readTarget, writeTarget,
                                  readOffset, writeOffset, size);

    Chunk *chunk = scope.Get();

    if(m_State == WRITING_CAPFRAME)
    {
      m_ContextRecord->AddChunk(chunk);
      m_MissingTracks.insert(writerecord->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(writerecord->GetResourceID(),
                                                        eFrameRef_ReadBeforeWrite);
    }
    else
    {
      writerecord->AddChunk(chunk);
      writerecord->AddParent(readrecord);
      writerecord->UpdateCount++;

      if(writerecord->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(writerecord->GetResourceID());
        GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
      }
    }
  }
}

bool WrappedGLES::Serialise_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Index, index);
  SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))
                                            : ResourceId()));

  if(m_State < WRITING)
  {
    if(id == ResourceId())
    {
      m_Real.glBindBuffer(Target, 0);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(id);
      m_Real.glBindBufferBase(Target, Index, res.name);
    }
  }

  return true;
}

void WrappedGLES::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
  ContextData &cd = GetCtxData();

  if(m_State >= WRITING)
  {
    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffer == 0)
      r = cd.m_BufferRecord[idx] = NULL;
    else
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(buffer && m_State == WRITING_CAPFRAME)
    {
      FrameRefType refType = eFrameRef_Read;

      // these targets write to the buffer
      if(target == eGL_ATOMIC_COUNTER_BUFFER || target == eGL_COPY_WRITE_BUFFER ||
         target == eGL_PIXEL_PACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
         target == eGL_TRANSFORM_FEEDBACK_BUFFER)
        refType = eFrameRef_ReadBeforeWrite;

      GetResourceManager()->MarkResourceFrameReferenced(cd.m_BufferRecord[idx]->GetResourceID(),
                                                        refType);
    }

    // it's legal to re-type buffers, generate another BindBuffer chunk to rename
    if(r && r->datatype != target)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
        Serialise_glBindBuffer(target, buffer);

        chunk = scope.Get();
      }

      r->AddChunk(chunk);
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(r && (target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
             target == eGL_ATOMIC_COUNTER_BUFFER))
    {
      if(m_State == WRITING_CAPFRAME)
        m_MissingTracks.insert(r->GetResourceID());
      else
        GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }

    if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_BASE);
      Serialise_glBindBufferBase(target, index, buffer);

      m_ContextRecord->AddChunk(scope.Get());
    }
  }

  m_Real.glBindBufferBase(target, index, buffer);
}

bool WrappedGLES::Serialise_glBindBufferRange(GLenum target, GLuint index, GLuint buffer,
                                                GLintptr offset, GLsizeiptr size)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Index, index);
  SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))
                                            : ResourceId()));
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
  SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);

  if(m_State < WRITING)
  {
    if(id == ResourceId())
    {
      m_Real.glBindBuffer(Target, 0);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(id);
      m_Real.glBindBufferRange(Target, Index, res.name, (GLintptr)Offset, (GLsizeiptr)Size);
    }
  }

  return true;
}

void WrappedGLES::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                      GLsizeiptr size)
{
  ContextData &cd = GetCtxData();

  if(m_State >= WRITING)
  {
    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffer == 0)
      r = cd.m_BufferRecord[idx] = NULL;
    else
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(buffer && m_State == WRITING_CAPFRAME)
    {
      FrameRefType refType = eFrameRef_Read;

      // these targets write to the buffer
      if(target == eGL_ATOMIC_COUNTER_BUFFER || target == eGL_COPY_WRITE_BUFFER ||
         target == eGL_PIXEL_PACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
         target == eGL_TRANSFORM_FEEDBACK_BUFFER)
        refType = eFrameRef_ReadBeforeWrite;

      GetResourceManager()->MarkResourceFrameReferenced(cd.m_BufferRecord[idx]->GetResourceID(),
                                                        refType);
    }

    // it's legal to re-type buffers, generate another BindBuffer chunk to rename
    if(r && r->datatype != target)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
        Serialise_glBindBuffer(target, buffer);

        chunk = scope.Get();
      }

      r->AddChunk(chunk);
    }

    // store as transform feedback record state
    if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GetResourceManager()->MarkDirtyResource(cd.m_FeedbackRecord->GetResourceID());
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(r && (target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
             target == eGL_ATOMIC_COUNTER_BUFFER))
    {
      if(m_State == WRITING_CAPFRAME)
        m_MissingTracks.insert(r->GetResourceID());
      else
        GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }

    if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_RANGE);
      Serialise_glBindBufferRange(target, index, buffer, offset, size);

      m_ContextRecord->AddChunk(scope.Get());
    }
  }

  m_Real.glBindBufferRange(target, index, buffer, offset, size);
}


#pragma endregion

#pragma region Mapping

///************************************************************************

 /* Mapping tends to be the most complex/dense bit of the capturing process, as there are a lot of
 * carefully considered use cases and edge cases to be aware of.
 *
 * The primary motivation is, obviously, correctness - where we have to sacrifice performance,
 * clarity for correctness, we do. Second to that, we try and keep things simple/clear where the
 * performance sacrifice will be minimal, and generally we try to remove overhead entirely for
 * high-traffic maps, such that we only step in where necessary.
 *
 * We'll consider "normal" maps of buffers, and persistent maps, separately. Note that in all cases
 * we can guarantee that the buffer being mapped has correctly-sized backing store available,
 * created in the glBufferData or glBufferStorage call. We also only need to consider the case of
 * glMapNamedBufferRangeEXT, glUnmapNamedBufferEXT and glFlushMappedNamedBufferRange - all other
 * entry points are mapped to one of these in a fairly simple fashion.
 *
 *
 * glMapNamedBufferRangeEXT:
 *
 * For a normal map, we decide to either record/intercept it, or to step out of the way and allow
 * the application to map directly to the GL buffer. We can only map directly when idle capturing,
 * when capturing a frame we must capture all maps to be correct. Generally we perform a direct map
 * either if this resource is being mapped often and we want to remove overhead, or if the map
 * interception would be more complex than it's worth.
 *
 * The first checks are to see if we've already "given up" on a buffer, in which case we map
 * directly again.
 *
 * Next, if the map is for write and the buffer is not invalidated, we also map directly.
 * [NB: Since our buffer contents should be perfect at this point, we may not need to worry about
 * non-invalidating maps. Potential future improvement.]
 *
 * At this point, if the map is to be done directly, we pass the parameters onto GL and return
 * the result, marking the map with status GLResourceRecord::Mapped_Ignore_Real. Note that this
 * means we have no idea what happens with the map, and the buffer contents after that are to us
 * undefined.
 *
 * If not, we will be intercepting the map. If it's read-only this is relatively simple to satisfy,
 * as we just need to fetch the current buffer contents and return the appropriately offsetted
 * pointer. [NB: Again our buffer contents should still be perfect here, this fetch may be
 * redundant.] The map status is recorded as GLResourceRecord::Mapped_Read
 *
 * At this point we are intercepting a map for write, and it depends on whether or not we are
 * capturing a frame or just idle.
 *
 * If idle the handling is relatively simple, we just offset the pointer and return, marking the
 * map as GLResourceRecord::Mapped_Write. Note that here we also increment a counter, and if this
 * counter reaches a high enough number (arbitrary limit), we mark the buffer as high-traffic so
 * that we'll stop intercepting maps and reduce overhead on this buffer.
 *
 * If frame capturing it is more complex. The backing store of the buffer must be preserved as it
 * will contain the contents at the start of the frame. Instead we allocate two shadow storage
 * copies on first use. Shadow storage [1] contains the 'current' contents of the buffer -
 * when first allocated, if the map is non-invalidating, it will be filled with the buffer contents
 * at that point. If the map is invalidating, it will be reset to 0xcc to help find bugs caused by
 * leaving valid data behind in invalidated buffer memory.
 *
 * Shadow buffer [0] is the buffer that is returned to the user code. Every time it is updated
 * with the contents of [1]. This way both buffers are always identical and contain the latest
 * buffer contents. These buffers are used later in unmap, but Map() will return the appropriately
 * offsetted pointer, and mark the map as GLResourceRecord::Mapped_Write.
 *
 *
 * glUnmapNamedBufferEXT:
 *
 * The unmap becomes an actual chunk for serialisation when necessary, so we'll discuss the handling
 * of the unmap call, and then how it is serialised.
 *
 * Unmap's handling varies depending on the status of the map, as set above in glMapNamedBufferRangeEXT.
 *
 * GLResourceRecord::Unmapped is an error case, indicating we haven't had a corresponding Map() call.
 *
 * GLResourceRecord::Mapped_Read is a no-op as we can just discard it, the pointer we returned from Map()
 *   was into our backing store.
 *
 * GLResourceRecord::Mapped_Ignore_Real is likewise a no-op as the GL pointer was updated directly by
 *   user code, we weren't involved. However if we are now capturing a frame, it indicates a Map() was
 *   made before this frame began, so this frame cannot be captured - we will need to try again next
 *   frame, where a map will not be allowed to go into GLResourceRecord::Mapped_Ignore_Real.
 *
 * GLResourceRecord::Mapped_Write is the only case that will generate a serialised unmap chunk. If we
 *   are idle, then all we need to do is map the 'real' GL buffer, copy across our backing store, and
 *   unmap. We only map the range that was modified. Then everything is complete as the user code updated
 *   our backing store. If we are capturing a frame, then we go into the serialise function and serialise
 *   out a chunk.
 *
 * Finally we set the map status back to GLResourceRecord::Unmapped.
 *
 * When serialising out a map, we serialise the details of the map (which buffer, offset, length) and
 * then for non-invalidating maps of >512 byte buffers we perform a difference compare between the two
 * shadow storage buffers that were set up in glMapNamedBufferRangeEXT. We then serialise out a buffer
 * of the difference segment, and on replay we map and update this segment of the buffer.
 *
 * The reason for finding the actual difference segment is that many maps will be of a large region
 * or even the whole buffer, but only update a small section, perhaps once per drawcall. So serialising
 * the entirety of a large buffer many many times can rapidly inflate the size of the log. The savings
 * from this can be many GBs as if a 4MB buffer is updated 1000 times, each time only updating 1KB,
 * this is a difference between 1MB and 4000MB in written data, most of which is redundant in the last
 * case.
 *
 *
 * glFlushMappedNamedBufferRangeEXT:
 *
 * Now consider the specialisation of the above, for maps that have GL_MAP_FLUSH_EXPLICIT_BIT enabled.
 *
 * For the most part, these maps can be treated very similarly to normal maps, however in the case of
 * unmapping we will skip creating an unmap chunk and instead just allow the unmap to be discarded.
 * Instead we will serialise out a chunk for each glFlushMappedNamedBufferRangeEXT call. We will also
 * include flush explicit maps along with the others that we choose to map directly when possible - so
 * if we're capturing idle a flush explicit map will go straight to GL and be handled as with
 * GLResourceRecord::Mapped_Ignore_Real above.
 *
 * For this reason, if a map status is GLResourceRecord::Mapped_Ignore_Real then we simply pass the
 * flush range along to real GL. Again if we are capturing a frame now, this map has been 'missed' and
 * we must try again next frame to capture. Likewise as with Unmap GLResourceRecord::Unmapped is an
 * error, and for flushing we do not need to consider GLResourceRecord::Mapped_Read (it doesn't make
 * sense for this case).
 *
 * So we only serialise out a flush chunk if we are capturing a frame, and the map is correctly
 * GLResourceRecord::Mapped_Write. We clamp the flushed range to the size of the map (in case the user
 * code didn't do this). Unlike map we do not perform any difference compares, but rely on the user to
 * only flush the minimal range, and serialise the entire range out as a buffer. We also update the
 * shadow storage buffers so that if the buffer is subsequently mapped without flush explicit, we have
 * the 'current' contents to perform accurate compares with.
 *
 *
 *
 *
 *
 * Persistant maps:
 *
 * The above process handles "normal" maps that happen between other GL commands that use the buffer
 * contents. Maps that are persistent need to be handled carefully since there are other knock-ons
 * for correctness and proper tracking. They come in two major forms - coherent and non-coherent.
 *
 * Non-coherent maps are the 'easy' case, and in all cases should be recommended whenever users do
 * persistent mapping. Indeed because of the implementation details, coherent maps may come at a
 * performance penalty even when RenderDoc is not used and it is simply the user code using GL directly.
 *
 * The important thing is that persistent maps *must always* be intercepted regardless of circumstance,
 * as in theory they may never be mapped again. We get hints to help us with these maps, as the buffers
 * must have been created with glBufferStorage and must have the matching persistent and optionally
 * coherent bits set in the flags bitfield.
 *
 * Note also that non-coherent maps tend to go hand in hand with flush explicit maps (although this is
 * not guaranteed, it is highly likely).
 *
 * Non-coherent mappable buffers are GL-mapped on creation, and remain GL-mapped until their destruction
 * regardless of what user code does. We keep this 'real' GL-mapped buffer around permanently but it is
 * never returned to user code. Instead we handle maps otherwise as above (taking care to always
 * intercept), and return the user a pointer to our backing store. Then every time a map flush happens
 * instead of temporarily mapping and unmapping the GL buffer, we copy into the appropriate place in our
 * persistent map pointer. If an unmap happens and the map wasn't flush-explicit, we copy the mapped
 * region then. In this way we maintain correctness - the copies are "delayed" by the time between
 * user code writing into our memory, and us copying into the real memory. However this is valid as it
 * happens synchronously with a flush, unmap or other event and by definition non-coherent maps aren't
 * visible to the GPU until after those operations.
 *
 * There is also the function glMemoryBarrier with bit GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT. This has the
 * effect of acting as if all currently persistent-mapped regions were simultaneously flushed. This is
 * exactly how we implement it - we store a list of all current user persistent maps and any time this
 * bit is passed to glMemoryBarrier, we manually call into glFlushMappedNamedBufferRangeEXT() with the
 * appropriate parameters and handling is otherwise identical.
 *
 * The final piece of the puzzle is coherent mapped buffers. Since we must break the coherency carefully
 * (see below), we map coherent buffers as non-coherent at creation time, the same as above.
 *
 * To satisfy the demands of being coherent, we need to transparently propogate any changes between the
 * user written data and the 'real' memory, without any call to intercept - there would be no need to
 * call glMemoryBarrier or glFlushMappedNamedBufferRangeEXT. To do this, we have shadow storage
 * allocated as in the "normal" mapping path all the time, and we insert a manual call to essentially
 * the same code as glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT) in every intercepted function
 * call that could depend on the results of the buffer. We then check if any write/change has happened
 * by comparing to the shadow storage, and if so we perform a manual flush of that changed region and
 * update the shadow storage for next time.
 *
 * This "fake coherency" is the reason we can map the buffer as non-coherent, since we will be performing
 * copies and flushes manually to emulate the coherency to allow our interception in the middle.
 *
 * By definition, there will be *many* of these places where the buffer results could be used, not least
 * any buffer copy, any texture copy (since a texture buffer could be created), any draw or dispatch,
 * etc. At each of these points there will be a cost for each coherent map of checking for changes and it
 * will scale with the size of the buffers. This is a large performance penalty but one that can't be
 * easily avoided. This is another reason why coherent maps should be avoided.
 *
 * Note that this also involves a behaviour change that affects correctness - a user write to memory is
 * not visible as soon as the write happens, but only on the next api point where the write could have
 * an effect. In correct code this should not be a problem as relying on any other behaviour would be
 * impossible - if you wrote into memory expecting commands in flight to be affected you could not ensure
 * correct ordering. However, obvious from that description, this is precisely a race condition bug if
 * user code did do that - which means race condition bugs will be hidden by the nature of this tracing.
 * This is unavoidable without the extreme performance hit of making all coherent maps read-write, and
 * performing a read-back at every sync point to find every change. Which by itself may also hide race
 * conditions anyway.
 *
 *
 * Implementation notes:
 *
 * The record->Map.ptr is the *offsetted* pointer, ie. a pointer to the beginning of the mapped region,
 * at record->Map.offset bytes from the start of the buffer.
 *
 * record->Map.persistentPtr points to the *base* of the buffer, not offsetted by any current map.
 *
 * Likewise the shadow storage pointers point to the base of a buffer-sized allocation each.
 *
 ************************************************************************/

void *WrappedGLES::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                                      GLbitfield access)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveBufferRecord(target);
    if (record)
    {
      bool directMap = false;

      // first check if we've already given up on these buffers
      if(m_State != WRITING_CAPFRAME &&
         m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end())
        directMap = true;

      if(!directMap && m_State != WRITING_CAPFRAME &&
         GetResourceManager()->IsResourceDirty(record->GetResourceID()))
        directMap = true;

      bool invalidateMap = (access & (eGL_MAP_INVALIDATE_BUFFER_BIT | eGL_MAP_INVALIDATE_RANGE_BIT)) != 0;
      bool flushExplicitMap = (access & eGL_MAP_FLUSH_EXPLICIT_BIT) != 0;

      // if this map is writing and doesn't invalidate, or is flush explicit, map directly
      if(!directMap && (!invalidateMap || flushExplicitMap) && (access & GL_MAP_WRITE_BIT) &&
         m_State != WRITING_CAPFRAME)
        directMap = true;

      // persistent maps must ALWAYS be intercepted
      if(access & eGL_MAP_PERSISTENT_BIT_EXT)
        directMap = false;

      if(directMap)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }

      record->Map.offset = offset;
      record->Map.length = length;
      record->Map.access = access;
      record->Map.invalidate = invalidateMap;

      // store a list of all persistent maps, and subset of all coherent maps
      if(access & eGL_MAP_PERSISTENT_BIT_EXT)
      {
        Atomic::Inc64(&record->Map.persistentMaps);
        m_PersistentMaps.insert(record);
        if(record->Map.access & eGL_MAP_COHERENT_BIT_EXT)
          m_CoherentMaps.insert(record);
      }

      // if we're doing a direct map, pass onto GL and return
      if(directMap)
      {
        record->Map.ptr = (byte *)m_Real.glMapBufferRange(target, offset, length, access);
        record->Map.status = GLResourceRecord::Mapped_Ignore_Real;

        return record->Map.ptr;
      }

      // only squirrel away read-only maps, read-write can just be treated as write-only
      if((access & (eGL_MAP_READ_BIT | eGL_MAP_WRITE_BIT)) == eGL_MAP_READ_BIT)
      {
        byte *ptr = record->GetDataPtr();

        if(record->Map.persistentPtr)
          ptr = record->GetShadowPtr(0);

        RDCASSERT(ptr);

        ptr += offset;

        glGetNamedBufferSubDataEXT(record->Resource.name, record->datatype, offset, length, ptr);

        record->Map.ptr = ptr;
        record->Map.status = GLResourceRecord::Mapped_Read;

        return ptr;
      }

      // below here, handle write maps to the backing store
      byte *ptr = record->GetDataPtr();

      RDCASSERT(ptr);
      {
        // persistent maps get particular handling
        if(access & eGL_MAP_PERSISTENT_BIT_EXT)
        {
          // persistent pointers are always into the shadow storage, this way we can use the backing
          // store for 'initial' buffer contents as with any other buffer. We also need to keep a
          // comparison & modified buffer in case the application calls glMemoryBarrier(..) at any
          // time.

          // if we're invalidating, mark the whole range as 0xcc
          if(invalidateMap)
          {
            memset(record->GetShadowPtr(0) + offset, 0xcc, length);
            memset(record->GetShadowPtr(1) + offset, 0xcc, length);
          }

          record->Map.ptr = ptr = record->GetShadowPtr(0) + offset;
          record->Map.status = GLResourceRecord::Mapped_Write;
        }
        else if(m_State == WRITING_CAPFRAME)
        {
          byte *shadow = (byte *)record->GetShadowPtr(0);

          // if we don't have a shadow pointer, need to allocate & initialise
          if(shadow == NULL)
          {
            GLint buflength;
            m_Real.glGetBufferParameteriv(target, eGL_BUFFER_SIZE, &buflength);

            // allocate our shadow storage
            record->AllocShadowStorage(buflength);
            shadow = (byte *)record->GetShadowPtr(0);

            // if we're not invalidating, we need the existing contents
            if(!invalidateMap)
            {
              // need to fetch the whole buffer's contents, not just the mapped range,
              // as next time we won't re-fetch and might need the rest of the contents
              if(GetResourceManager()->IsResourceDirty(record->GetResourceID()))
              {
                // Perhaps we could get these contents from the frame initial state buffer?
                glGetNamedBufferSubDataEXT(record->Resource.name, record->datatype, 0, buflength, shadow);
              }
              else
              {
                memcpy(shadow, record->GetDataPtr(), buflength);
              }
            }

            // copy into second shadow buffer ready for comparison later
            memcpy(record->GetShadowPtr(1), shadow, buflength);
          }

          // if we're invalidating, mark the whole range as 0xcc
          if(invalidateMap)
          {
            memset(shadow + offset, 0xcc, length);
            memset(record->GetShadowPtr(1) + offset, 0xcc, length);
          }

          record->Map.ptr = ptr = shadow;
          record->Map.status = GLResourceRecord::Mapped_Write;
        }
        else if(m_State == WRITING_IDLE)
        {
          // return buffer backing store pointer, offsetted
          ptr += offset;

          record->Map.ptr = ptr;
          record->Map.status = GLResourceRecord::Mapped_Write;

          record->UpdateCount++;

          // mark as high-traffic if we update it often enough
          if(record->UpdateCount > 60)
          {
            m_HighTrafficResources.insert(record->GetResourceID());
            GetResourceManager()->MarkDirtyResource(record->GetResourceID());
          }
        }
      }

      return ptr;
    }
    RDCERR("glMapBufferRange: Couldn't get resource record for target %x - no buffer bound?", target);
  }
  return m_Real.glMapBufferRange(target, offset, length, access);
}

void *WrappedGLES::glMapBufferOES(GLenum target, GLenum access)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveBufferRecord(target);
    if(record)
    {
      GLbitfield accessBits = 0;

      if(access == eGL_READ_ONLY)
        accessBits = eGL_MAP_READ_BIT;
      else if(access == eGL_WRITE_ONLY)
        accessBits = eGL_MAP_WRITE_BIT;
      else if(access == eGL_READ_WRITE)
        accessBits = eGL_MAP_READ_BIT | eGL_MAP_WRITE_BIT;
      return glMapBufferRange(target, 0, (GLsizeiptr)record->Length, accessBits);
    }

    RDCERR("glMapBuffer: Couldn't get resource record for target %x - no buffer bound?", target);
  }

  return m_Real.glMapBufferOES(target, access);
}

bool WrappedGLES::Serialise_glUnmapBuffer(GLenum target)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  GLResourceRecord *record = NULL;

  if(m_State >= WRITING)
    record = GetCtxData().GetActiveBufferRecord(target);

  SERIALISE_ELEMENT(ResourceId, bufID, record->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint64_t, offs, record->Map.offset);
  SERIALISE_ELEMENT(uint64_t, len, record->Map.length);

  size_t diffStart = 0;
  size_t diffEnd = (size_t)len;

  if(m_State == WRITING_CAPFRAME &&
     // don't bother checking diff range for tiny buffers
     len > 512 &&
     // if the map has a sub-range specified, trust the user to have specified
     // a minimal range, similar to glFlushMappedBufferRange, so don't find diff
     // range.
     record->Map.offset == 0 && record->Map.length == (GLsizeiptr)record->Length &&
     // similarly for invalidate maps, we want to update the whole buffer
     !record->Map.invalidate)
  {
    bool found = FindDiffRange(record->Map.ptr, record->GetShadowPtr(1) + offs, (size_t)len,
                               diffStart, diffEnd);
    if(found)
    {
      static size_t saved = 0;

      saved += (size_t)len - (diffEnd - diffStart);

      RDCDEBUG("Mapped resource size %u, difference: %u -> %u. Total bytes saved so far: %u",
               (uint32_t)len, (uint32_t)diffStart, (uint32_t)diffEnd, (uint32_t)saved);

      len = diffEnd - diffStart;
    }
    else
    {
      diffStart = 0;
      diffEnd = 0;

      len = 1;
    }
  }

  if(m_State == WRITING_CAPFRAME && record->GetShadowPtr(1))
  {
    memcpy(record->GetShadowPtr(1) + diffStart, record->Map.ptr + diffStart, diffEnd - diffStart);
  }

  if(m_State == WRITING_IDLE)
  {
    diffStart = 0;
    diffEnd = (size_t)len;
  }

  SERIALISE_ELEMENT(uint32_t, DiffStart, (uint32_t)diffStart);
  SERIALISE_ELEMENT(uint32_t, DiffEnd, (uint32_t)diffEnd);

  SERIALISE_ELEMENT_BUF(byte *, data, record->Map.ptr + diffStart, (size_t)len);

  if(DiffEnd > DiffStart)
  {
    if(record && record->Map.persistentPtr)
    {
      // if we have a persistent mapped pointer, copy the range into the 'real' memory and
      // do a flush. Note the persistent pointer is always to the base of the buffer so we
      // need to account for the offset
      memcpy(record->Map.persistentPtr + offs + DiffStart, record->Map.ptr + DiffStart,
             DiffEnd - DiffStart);
      Compat_glFlushMappedBufferRangeEXT(Target, GLintptr(offs + DiffStart),
                                              DiffEnd - DiffStart);
    }
    else
    {
      SafeBufferBinder safeBufferBinder(m_Real);
      if (m_State < WRITING)
          safeBufferBinder.saveBinding(Target, GetResourceManager()->GetLiveResource(bufID).name);

      void *ptr = Compat_glMapBufferRangeEXT(Target, (GLintptr)(offs + DiffStart), GLsizeiptr(DiffEnd - DiffStart), GL_MAP_WRITE_BIT);
      memcpy(ptr, data, size_t(DiffEnd - DiffStart));
      m_Real.glUnmapBuffer(Target);
    }
  }

  if(m_State < WRITING)
    delete[] data;

  return true;
}

GLboolean WrappedGLES::glUnmapBuffer(GLenum target)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveBufferRecord(target);
    RDCASSERT(record);

    if(record) {

      auto status = record->Map.status;

      if(m_State == WRITING_CAPFRAME)
      {
        m_MissingTracks.insert(record->GetResourceID());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_ReadBeforeWrite);
      }

      GLboolean ret = GL_TRUE;

      switch(status)
      {
        case GLResourceRecord::Unmapped:
          RDCERR("Unmapped buffer being passed to glUnmapBuffer");
          break;
        case GLResourceRecord::Mapped_Read:
          // can ignore
          break;
        case GLResourceRecord::Mapped_Ignore_Real:
          if(m_State == WRITING_CAPFRAME)
          {
            RDCERR(
                "Failed to cap frame - we saw an Unmap() that we didn't capture the corresponding "
                "Map() for");
            m_SuccessfulCapture = false;
            m_FailureReason = CaptureFailed_UncappedUnmap;
          }
          // need to do the real unmap
          ret = m_Real.glUnmapBuffer(target);
          break;
        case GLResourceRecord::Mapped_Write:
        {
          if(record->Map.access & GL_MAP_FLUSH_EXPLICIT_BIT)
          {
            // do nothing, any flushes that happened were handled,
            // and we won't do any other updates here or make a chunk.
          }
          else if(m_State == WRITING_CAPFRAME)
          {
            SCOPED_SERIALISE_CONTEXT(UNMAP);
            Serialise_glUnmapBuffer(target);
            m_ContextRecord->AddChunk(scope.Get());
          }
          else if(m_State == WRITING_IDLE)
          {
            if(record->Map.persistentPtr)
            {
              // if we have a persistent mapped pointer, copy the range into the 'real' memory and
              // do a flush. Note the persistent pointer is always to the base of the buffer so we
              // need to account for the offset

              memcpy(record->Map.persistentPtr + record->Map.offset, record->Map.ptr,
                     record->Map.length);
              Compat_glFlushMappedBufferRangeEXT(target, record->Map.offset, record->Map.length);

              // update shadow storage
              memcpy(record->GetShadowPtr(1) + record->Map.offset, record->Map.ptr, record->Map.length);

              GetResourceManager()->MarkDirtyResource(record->GetResourceID());
            }
            else
            {

              // if we are here for WRITING_IDLE, the app wrote directly into our backing
              // store memory. Just need to copy the data across to GL, no other work needed
              void *ptr =
                  Compat_glMapBufferRangeEXT(target, (GLintptr)record->Map.offset,
                                                  GLsizeiptr(record->Map.length), GL_MAP_WRITE_BIT);
              memcpy(ptr, record->Map.ptr, record->Map.length);
              m_Real.glUnmapBuffer(target);

            }
          }

          break;
        }
      }

      // keep list of persistent & coherent maps up to date if we've
      // made the last unmap to a buffer
      if(record->Map.access & eGL_MAP_PERSISTENT_BIT_EXT)
      {
        int64_t ref = Atomic::Dec64(&record->Map.persistentMaps);
        if(ref == 0)
        {
          m_PersistentMaps.erase(record);
          if(record->Map.access & eGL_MAP_COHERENT_BIT_EXT)
            m_CoherentMaps.erase(record);
        }
      }

      record->Map.status = GLResourceRecord::Unmapped;

      return ret;
    }

    RDCERR("glUnmapBuffer: Couldn't get resource record for target %x - no buffer bound?", target);
  }

  return m_Real.glUnmapBuffer(target);
}

GLboolean WrappedGLES::glUnmapBufferOES(GLenum target)
{
  return glUnmapBuffer(target);
}

bool WrappedGLES::Serialise_glFlushMappedBufferRange(GLenum target, GLintptr offset,
                                                               GLsizeiptr length)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  GLResourceRecord *record = NULL;

  if(m_State >= WRITING)
    record = GetCtxData().GetActiveBufferRecord(target);

  SERIALISE_ELEMENT(ResourceId, bufID, record->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint64_t, offs, offset);
  SERIALISE_ELEMENT(uint64_t, len, length);

  // serialise out the flushed chunk of the shadow pointer
  SERIALISE_ELEMENT_BUF(byte *, data, record->Map.ptr + offs, (size_t)len);

  // update the comparison buffer in case this buffer is subsequently mapped and we want to find
  // the difference region
  if(m_State == WRITING_CAPFRAME && record->GetShadowPtr(1))
  {
    memcpy(record->GetShadowPtr(1) + offs, record->Map.ptr + offs, (size_t)len);
  }

  if(record && record->Map.persistentPtr)
  {
    // if we have a persistent mapped pointer, copy the range into the 'real' memory and
    // do a flush. Note the persistent pointer is always to the base of the buffer so we
    // need to account for the offset

    memcpy(record->Map.persistentPtr + offs, record->Map.ptr - record->Map.offset + offs,
           (size_t)len);
    m_Real.glFlushMappedBufferRange(Target, (GLintptr)offs, (GLsizeiptr)len);
  }
  else
  {
    // perform a map of the range and copy the data, to emulate the modified region being flushed
    SafeBufferBinder safeBufferBinder(m_Real);
    if (m_State < WRITING)
        safeBufferBinder.saveBinding(Target, GetResourceManager()->GetLiveResource(bufID).name);

    void *ptr =
        m_Real.glMapBufferRange(Target, (GLintptr)offs, (GLsizeiptr)len, GL_MAP_WRITE_BIT);
    memcpy(ptr, data, (size_t)len);
    m_Real.glUnmapBuffer(Target);
  }

  if(m_State < WRITING)
    SAFE_DELETE_ARRAY(data);

  return true;
}

void WrappedGLES::glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled
  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveBufferRecord(target);
    if (record == NULL)
      RDCERR("glFlushMappedBufferRange: Couldn't get resource record for target %x - no buffer bound?", target);

    RDCASSERT(record);

    if(record)
    {
      // only need to pay attention to flushes when in capframe. Otherwise (see above) we
      // treat the map as a normal map, and let ALL modified regions go through, flushed or not,
      // as this is legal - modified but unflushed regions are 'undefined' so we can just say
      // that modifications applying is our undefined behaviour.

      // note that we only want to flush the range with GL if we've actually
      // mapped it. Otherwise the map is 'virtual' and just pointing to our backing store data
      if(record && record->Map.status == GLResourceRecord::Mapped_Ignore_Real)
      {
        m_Real.glFlushMappedBufferRange(target, offset, length);
      }

      if(m_State == WRITING_CAPFRAME)
      {
        if(record)
        {
          m_MissingTracks.insert(record->GetResourceID());
          GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                            eFrameRef_ReadBeforeWrite);

          if(record->Map.status == GLResourceRecord::Unmapped)
          {
            RDCWARN("Unmapped buffer being flushed, ignoring");
          }
          else if(record->Map.status == GLResourceRecord::Mapped_Ignore_Real)
          {
            RDCERR(
                "Failed to cap frame - we saw an FlushMappedBuffer() that we didn't capture the "
                "corresponding Map() for");
            m_SuccessfulCapture = false;
            m_FailureReason = CaptureFailed_UncappedUnmap;
          }
          else if(record->Map.status == GLResourceRecord::Mapped_Write)
          {
            if(offset < record->Map.offset || offset + length > record->Map.offset + record->Map.length)
            {
              RDCWARN("Flushed buffer range is outside of mapped range, clamping");

              // maintain the length/end boundary of the flushed range if the flushed offset
              // is below the mapped range
              if(offset < record->Map.offset)
              {
                offset += (record->Map.offset - offset);
                length -= (record->Map.offset - offset);
              }

              // clamp the length if it's beyond the mapped range.
              if(offset + length > record->Map.offset + record->Map.length)
              {
                length = (record->Map.offset + record->Map.length - offset);
              }
            }

            SCOPED_SERIALISE_CONTEXT(FLUSHMAP);
            Serialise_glFlushMappedBufferRange(target, offset, length);
            m_ContextRecord->AddChunk(scope.Get());
          }
          // other statuses is GLResourceRecord::Mapped_Read
        }
      }
      else if(m_State == WRITING_IDLE)
      {
        // if this is a flush of a persistent map, we need to copy through to
        // the real pointer and perform a real flush.
        if(record && record->Map.persistentPtr)
        {
          memcpy(record->Map.persistentPtr + offset, record->Map.ptr - record->Map.offset + offset,
                 length);
          m_Real.glFlushMappedBufferRange(target, offset, length);

          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    return;
  }

  return m_Real.glFlushMappedBufferRange(target, offset, length);
}

void WrappedGLES::PersistentMapMemoryBarrier(const set<GLResourceRecord *> &maps)
{
  // this function iterates over all the maps, checking for any changes between
  // the shadow pointers, and propogates that to 'real' GL

  for(set<GLResourceRecord *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
  {
    GLResourceRecord *record = *it;

    RDCASSERT(record && record->Map.persistentPtr);

    size_t diffStart = 0, diffEnd = 0;
    bool found = FindDiffRange(record->GetShadowPtr(0), record->GetShadowPtr(1),
                               (size_t)record->Length, diffStart, diffEnd);
    if(found)
    {
      // update the modified region in the 'comparison' shadow buffer for next check
      memcpy(record->GetShadowPtr(1) + diffStart, record->GetShadowPtr(0) + diffStart,
             diffEnd - diffStart);

      // we use our own flush function so it will serialise chunks when necessary, and it
      // also handles copying into the persistent mapped pointer and flushing the real GL
      // buffer
      glFlushMappedBufferRange(record->datatype, GLintptr(diffStart), GLsizeiptr(diffEnd - diffStart));
      RDCWARN("CHECK PEPE %s:%d", __FILE__ ,__LINE__);
    }
  }
}

#pragma endregion

#pragma region Transform Feedback

bool WrappedGLES::Serialise_glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(FeedbackRes(GetCtx(), *ids)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenTransformFeedbacks(1, &real);
    m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, real);
    m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);

    GLResource res = FeedbackRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedGLES::glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
  m_Real.glGenTransformFeedbacks(n, ids);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FeedbackRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_FEEDBACK);
        Serialise_glGenTransformFeedbacks(1, ids + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

void WrappedGLES::glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FeedbackRes(GetCtx(), ids[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      GetResourceManager()->MarkCleanResource(res);
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteTransformFeedbacks(n, ids);
}

bool WrappedGLES::Serialise_glBindTransformFeedback(GLenum target, GLuint id)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));

  if(m_State <= EXECUTING)
  {
    if(fid != ResourceId())
      m_Real.glBindTransformFeedback(Target, GetResourceManager()->GetLiveResource(fid).name);
    else
      m_Real.glBindTransformFeedback(Target, 0);
  }

  return true;
}

void WrappedGLES::glBindTransformFeedback(GLenum target, GLuint id)
{
  m_Real.glBindTransformFeedback(target, id);

  GLResourceRecord *record = NULL;

  if(m_State >= WRITING)
  {
    if(id == 0)
    {
      GetCtxData().m_FeedbackRecord = record = NULL;
    }
    else
    {
      GetCtxData().m_FeedbackRecord = record =
          GetResourceManager()->GetResourceRecord(FeedbackRes(GetCtx(), id));
    }
  }

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BIND_FEEDBACK);
    Serialise_glBindTransformFeedback(target, id);

    m_ContextRecord->AddChunk(scope.Get());

    if(record)
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }
}

bool WrappedGLES::Serialise_glBeginTransformFeedback(GLenum primitiveMode)
{
  SERIALISE_ELEMENT(GLenum, Mode, primitiveMode);

  if(m_State <= EXECUTING)
  {
    m_Real.glBeginTransformFeedback(Mode);
    m_ActiveFeedback = true;
  }

  return true;
}

void WrappedGLES::glBeginTransformFeedback(GLenum primitiveMode)
{
  m_Real.glBeginTransformFeedback(primitiveMode);
  m_ActiveFeedback = true;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_FEEDBACK);
    Serialise_glBeginTransformFeedback(primitiveMode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glPauseTransformFeedback()
{
  if(m_State <= EXECUTING)
  {
    m_Real.glPauseTransformFeedback();
  }

  return true;
}

void WrappedGLES::glPauseTransformFeedback()
{
  m_Real.glPauseTransformFeedback();

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PAUSE_FEEDBACK);
    Serialise_glPauseTransformFeedback();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glResumeTransformFeedback()
{
  if(m_State <= EXECUTING)
  {
    m_Real.glResumeTransformFeedback();
  }

  return true;
}

void WrappedGLES::glResumeTransformFeedback()
{
  m_Real.glResumeTransformFeedback();

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(RESUME_FEEDBACK);
    Serialise_glResumeTransformFeedback();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glEndTransformFeedback()
{
  if(m_State <= EXECUTING)
  {
    m_Real.glEndTransformFeedback();
    m_ActiveFeedback = false;
  }

  return true;
}

void WrappedGLES::glEndTransformFeedback()
{
  m_Real.glEndTransformFeedback();
  m_ActiveFeedback = false;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_FEEDBACK);
    Serialise_glEndTransformFeedback();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

#pragma endregion

#pragma region Vertex Arrays

// NOTE: In each of the vertex array object functions below, we might not have the live buffer resource
// if it's is a pre-capture chunk, and the buffer was never referenced at all in the actual frame.
// The reason for this is that the VAO record doesn't add a parent of the buffer record - because that
// parent tracking quickly becomes stale with high traffic VAOs ignoring updates etc, so we don't rely
// on the parent connection and manually reference the buffer wherever it is actually uesd.

bool WrappedGLES::Serialise_glVertexAttribPointerEXT(GLuint vaobj, GLuint buffer,
                                                     GLuint index, GLint size, GLenum type,
                                                     GLboolean normalized, GLsizei stride, const void *pointer, size_t dataSize,
                                                     bool isInteger)
{

  SERIALISE_ELEMENT(uint32_t, Index, index);
  SERIALISE_ELEMENT(int32_t, Size, size);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint8_t, Norm, normalized);
  SERIALISE_ELEMENT(uint32_t, Stride, stride);
  SERIALISE_ELEMENT(bool, LocalData, dataSize != 0);
  SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)pointer);
  SERIALISE_ELEMENT_BUF(byte *, bytes, pointer, dataSize);
  SERIALISE_ELEMENT(bool, IsIntegerMode, isInteger);
  SERIALISE_ELEMENT(ResourceId, id,
                    vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
  SERIALISE_ELEMENT(ResourceId, bid,
                    buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId());

  if(m_State < WRITING)
  {
    vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;
    buffer = (bid != ResourceId() && GetResourceManager()->HasLiveResource(bid))
                 ? GetResourceManager()->GetLiveResource(bid).name
                 : 0;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    SafeVAOBinder safeVAOBinder(m_Real, vaobj);
    SafeBufferBinder SafeBufferBinder(m_Real, eGL_ARRAY_BUFFER, buffer);

    if (IsIntegerMode) // TODO(elecro): do we really need both here?
    {
      if (LocalData)
        m_Real.glVertexAttribIPointer(Index, Size, Type, Stride, (const void*)bytes);
      else
        m_Real.glVertexAttribIPointer(Index, Size, Type, Stride, (const void*)Offset);
    }
    else
    {
      if (LocalData)
        m_Real.glVertexAttribPointer(Index, Size, Type, Norm, Stride, (const void*)bytes);
      else
        m_Real.glVertexAttribPointer(Index, Size, Type, Norm, Stride, (const void*)Offset);
    }
    // store pointers to local data buffers in order to release them at the end of the replay
    m_localDataBuffers.push_back(bytes);
  }
  return true;
}

void WrappedGLES::glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                                          GLboolean normalized, GLsizei stride, const void *pointer)
{
  m_Real.glVertexAttribPointer(index, size, type, normalized, stride, pointer);

  if(m_State >= WRITING)
  {

    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.GetActiveBufferRecord(eGL_ARRAY_BUFFER);
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(m_State == WRITING_CAPFRAME && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      if (bufrecord != NULL) {
        SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
        Serialise_glVertexAttribPointerEXT(
            varecord ? varecord->Resource.name : 0,
            bufrecord ? bufrecord->Resource.name : 0,
            index,
            size,
            type,
            normalized,
            stride,
            pointer,
            0,
            false);
        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedGLES::glVertexAttribIPointer(GLuint index, GLint size, GLenum type,
                                         GLsizei stride, const void *pointer)
{
  m_Real.glVertexAttribIPointer(index, size, type, stride, pointer);

  if(m_State >= WRITING)
  {

    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.GetActiveBufferRecord(eGL_ARRAY_BUFFER);
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(m_State == WRITING_CAPFRAME && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      if (bufrecord != NULL) {
        SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
        Serialise_glVertexAttribPointerEXT(
            varecord ? varecord->Resource.name : 0,
            bufrecord ? bufrecord->Resource.name : 0,
            index,
            size,
            type,
            GL_FALSE,
            stride,
            pointer,
            0,
            true);
        r->AddChunk(scope.Get());
      }
    }
  }
}


bool WrappedGLES::Serialise_glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
  SERIALISE_ELEMENT(uint32_t, aidx, attribindex);
  SERIALISE_ELEMENT(uint32_t, bidx, bindingindex);
  SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

  if(m_State < WRITING)
  {
    SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
    m_Real.glVertexAttribBinding(aidx, bidx);
  }
  return true;
}

void WrappedGLES::glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
  m_Real.glVertexAttribBinding(attribindex, bindingindex);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBBINDING);
        Serialise_glVertexAttribBinding(attribindex, bindingindex);

        r->AddChunk(scope.Get());
      }
    }
  }
}

bool WrappedGLES::Serialise_glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                                         GLboolean normalized, GLuint relativeoffset)
{
  SERIALISE_ELEMENT(uint32_t, Index, attribindex);
  SERIALISE_ELEMENT(int32_t, Size, size);
  SERIALISE_ELEMENT(bool, Norm, normalized ? true : false);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
  SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

  if(m_State < WRITING)
  {
    SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
    m_Real.glVertexAttribFormat(Index, Size, Type, Norm, Offset);
  }

  return true;
}

void WrappedGLES::glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                                         GLboolean normalized, GLuint relativeoffset)
{
  m_Real.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBFORMAT);
        Serialise_glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}


bool WrappedGLES::Serialise_glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                                          GLuint relativeoffset)
{
  SERIALISE_ELEMENT(uint32_t, Index, attribindex);
  SERIALISE_ELEMENT(int32_t, Size, size);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
  SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

  if(m_State < WRITING)
  {
    SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
    m_Real.glVertexAttribIFormat(Index, Size, Type, Offset);
  }

  return true;

}

void WrappedGLES::glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                                          GLuint relativeoffset)
{
  m_Real.glVertexAttribIFormat(attribindex, size, type, relativeoffset);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIFORMAT);
        Serialise_glVertexAttribIFormat(attribindex, size, type, relativeoffset);
        r->AddChunk(scope.Get());
      }
    }
  }
}

bool WrappedGLES::Serialise_glVertexAttribDivisor(GLuint index, GLuint divisor)
{
  SERIALISE_ELEMENT(uint32_t, Index, index);
  SERIALISE_ELEMENT(uint32_t, Divisor, divisor);
  SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

  if(m_State < WRITING)
  {
    SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
    m_Real.glVertexAttribDivisor(Index, Divisor);
  }

  return true;
}

void WrappedGLES::glVertexAttribDivisor(GLuint index, GLuint divisor)
{
  m_Real.glVertexAttribDivisor(index, divisor);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBDIVISOR);
        Serialise_glVertexAttribDivisor(index, divisor);

        r->AddChunk(scope.Get());
      }
    }
  }
}

bool WrappedGLES::Serialise_glEnableVertexAttribArray(GLuint index)
{
    SERIALISE_ELEMENT(uint32_t, Index, index);
    SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

    if(m_State < WRITING)
    {
      SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
      m_Real.glEnableVertexAttribArray(Index);
    }
    return true;
}

void WrappedGLES::glEnableVertexAttribArray(GLuint index)
{
  m_Real.glEnableVertexAttribArray(index);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
        Serialise_glEnableVertexAttribArray(index);

        r->AddChunk(scope.Get());
      }
    }
  }
}

bool WrappedGLES::Serialise_glDisableVertexAttribArray(GLuint index)
{
    SERIALISE_ELEMENT(uint32_t, Index, index);
    SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

    if(m_State < WRITING)
    {
      SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
      m_Real.glDisableVertexAttribArray(Index);
    }
    return true;
}

void WrappedGLES::glDisableVertexAttribArray(GLuint index)
{
  m_Real.glDisableVertexAttribArray(index);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(DISABLEVERTEXATTRIBARRAY);
        Serialise_glDisableVertexAttribArray(index);

        r->AddChunk(scope.Get());
      }
    }
  }
}

bool WrappedGLES::Serialise_glGenVertexArrays(GLsizei n, GLuint *arrays)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(VertexArrayRes(GetCtx(), *arrays)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenVertexArrays(1, &real);

    GLResource res = VertexArrayRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedGLES::glGenVertexArrays(GLsizei n, GLuint *arrays)
{
  m_Real.glGenVertexArrays(n, arrays);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_VERTEXARRAY);
        Serialise_glGenVertexArrays(1, arrays + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

bool WrappedGLES::Serialise_glBindVertexArray(GLuint array)
{
  SERIALISE_ELEMENT(
      ResourceId, id,
      (array ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), array)) : ResourceId()));

  if(m_State <= EXECUTING)
  {
    if(id == ResourceId())
    {
      m_Real.glBindVertexArray(m_FakeVAO);
    }
    else
    {
      GLuint live = GetResourceManager()->GetLiveResource(id).name;
      m_Real.glBindVertexArray(live);
    }
  }

  return true;
}

void WrappedGLES::glBindVertexArray(GLuint array)
{
  m_Real.glBindVertexArray(array);

  GLResourceRecord *record = NULL;

  if(m_State >= WRITING)
  {
    if(array == 0)
    {
      GetCtxData().m_VertexArrayRecord = record = NULL;
    }
    else
    {
      GetCtxData().m_VertexArrayRecord = record =
          GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), array));
    }
  }

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BIND_VERTEXARRAY);
    Serialise_glBindVertexArray(array);
    m_ContextRecord->AddChunk(scope.Get());
    if (record)
      GetResourceManager()->MarkVAOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
  }
}

bool WrappedGLES::Serialise_glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset,
                                       GLsizei stride)
{
  SERIALISE_ELEMENT(uint32_t, idx, bindingindex);
  SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))
                                            : ResourceId()));
  SERIALISE_ELEMENT(uint64_t, offs, offset);
  SERIALISE_ELEMENT(uint64_t, str, stride);

  if(m_State <= EXECUTING)
  {
    GLuint live = 0;
    if(id != ResourceId() && GetResourceManager()->HasLiveResource(id))
    {
      live = GetResourceManager()->GetLiveResource(id).name;
      m_Buffers[GetResourceManager()->GetLiveID(id)].curType = eGL_ARRAY_BUFFER;
    }

    m_Real.glBindVertexBuffer(idx, live, (GLintptr)offs, (GLsizei)str);
  }

  return true;
}

void WrappedGLES::glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset,
                                       GLsizei stride)
{
  m_Real.glBindVertexBuffer(bindingindex, buffer, offset, stride);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(m_State == WRITING_CAPFRAME && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        SCOPED_SERIALISE_CONTEXT(BIND_VERTEXBUFFER);
        Serialise_glBindVertexBuffer(bindingindex, buffer, offset, stride);

        r->AddChunk(scope.Get());
      }
    }
  }
}

bool WrappedGLES::Serialise_glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
  SERIALISE_ELEMENT(uint32_t, idx, bindingindex);
  SERIALISE_ELEMENT(uint32_t, d, divisor);
  SERIALISE_ELEMENT(
      ResourceId, id,
      GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

  if(m_State <= EXECUTING)
  {
    SafeVAOBinder safeVAOBinder(m_Real, id != ResourceId() ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO);
    m_Real.glVertexBindingDivisor(idx, d);
  }

  return true;
}

void WrappedGLES::glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
  m_Real.glVertexBindingDivisor(bindingindex, divisor);

  if(m_State >= WRITING)
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

    if(r)
    {
      if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
        return;
      if(m_State == WRITING_CAPFRAME && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        SCOPED_SERIALISE_CONTEXT(VERTEXBINDINGDIVISOR);
        Serialise_glVertexBindingDivisor(bindingindex, divisor);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedGLES::glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = BufferRes(GetCtx(), buffers[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      GLResourceRecord *record = GetResourceManager()->GetResourceRecord(res);
      if(record)
      {
        // if we have a persistent pointer, make sure to unmap it
        if(record->Map.persistentPtr)
        {
          m_PersistentMaps.erase(record);
          if(record->Map.access & eGL_MAP_COHERENT_BIT_EXT)
            m_CoherentMaps.erase(record);

          GLint prevBinding;
          m_Real.glGetIntegerv(BufferBinding(record->datatype), &prevBinding);
          m_Real.glBindBuffer(record->datatype, res.name);
          m_Real.glUnmapBuffer(record->datatype);
          m_Real.glBindBuffer(record->datatype, prevBinding);
        }

        // free any shadow storage
        record->FreeShadowStorage();
      }

      GetResourceManager()->MarkCleanResource(res);
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteBuffers(n, buffers);
}

void WrappedGLES::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      GetResourceManager()->MarkCleanResource(res);
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteVertexArrays(n, arrays);
}

#pragma endregion

#pragma region Horrible glVertexAttrib variants

bool WrappedGLES::Serialise_glVertexAttrib(GLuint index, int count, GLenum type,
                                             GLboolean normalized, const void *value, int attribtype)
{
  SERIALISE_ELEMENT(uint32_t, idx, index);
  SERIALISE_ELEMENT(int32_t, Count, count);
  SERIALISE_ELEMENT(int, Type, attribtype);
  SERIALISE_ELEMENT(bool, norm, normalized == GL_TRUE);
  SERIALISE_ELEMENT(GLenum, packedType, type);

  AttribType attr = AttribType(Type & Attrib_typemask);

  size_t elemSize = 4;
  size_t valueSize = elemSize * Count;

  if(m_State >= WRITING)
  {
    m_pSerialiser->RawWriteBytes(value, valueSize);
  }
  else if(m_State <= EXECUTING)
  {
    value = m_pSerialiser->RawReadBytes(valueSize);

    if(Type & Attrib_I)
    {
      if (Count == 4)
      {
        if(attr == Attrib_GLint)
          m_Real.glVertexAttribI4iv(idx, (GLint *)value);
        else if(attr == Attrib_GLuint)
          m_Real.glVertexAttribI4uiv(idx, (GLuint *)value);
      }
    }
    else
    {
      if(Count == 1)
      {
        if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib1fv(idx, (GLfloat *)value);
      }
      else if(Count == 2)
      {
        if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib2fv(idx, (GLfloat *)value);
      }
      else if(Count == 3)
      {
        if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib3fv(idx, (GLfloat *)value);
      }
      else if(Count == 4)
      {
        if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib4fv(idx, (GLfloat *)value);
      }
    }
  }

  return true;
}

#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype, ...)                      \
                                                                                \
  void WrappedGLES::CONCAT(glVertexAttrib, suffix)(GLuint index, __VA_ARGS__) \
                                                                                \
  {                                                                             \
    m_Real.CONCAT(glVertexAttrib, suffix)(index, ARRAYLIST);                    \
                                                                                \
    if(m_State >= WRITING_CAPFRAME)                                             \
    {                                                                           \
      SCOPED_SERIALISE_CONTEXT(VERTEXATTRIB_GENERIC);                           \
      const paramtype vals[] = {ARRAYLIST};                                     \
      Serialise_glVertexAttrib(index, count, eGL_NONE, GL_FALSE, vals,          \
                               TypeOr | CONCAT(Attrib_, paramtype));            \
                                                                                \
      m_ContextRecord->AddChunk(scope.Get());                                   \
    }                                                                           \
  }

#define ARRAYLIST x

ATTRIB_FUNC(1, 1f, 0, GLfloat, GLfloat x)

#undef ARRAYLIST
#define ARRAYLIST x, y

ATTRIB_FUNC(2, 2f, 0, GLfloat, GLfloat x, GLfloat y)

#undef ARRAYLIST
#define ARRAYLIST x, y, z

ATTRIB_FUNC(3, 3f, 0, GLfloat, GLfloat x, GLfloat y, GLfloat z)

#undef ARRAYLIST
#define ARRAYLIST x, y, z, w

ATTRIB_FUNC(4, 4f, 0, GLfloat, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
ATTRIB_FUNC(4, I4i, Attrib_I, GLint, GLint x, GLint y, GLint z, GLint w)
ATTRIB_FUNC(4, I4ui, Attrib_I, GLuint, GLuint x, GLuint y, GLuint z, GLuint w)

#undef ATTRIB_FUNC
#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype)                                      \
                                                                                           \
  void WrappedGLES::CONCAT(glVertexAttrib, suffix)(GLuint index, const paramtype *value) \
                                                                                           \
  {                                                                                        \
    m_Real.CONCAT(glVertexAttrib, suffix)(index, value);                                   \
                                                                                           \
    if(m_State >= WRITING_CAPFRAME)                                                        \
    {                                                                                      \
      SCOPED_SERIALISE_CONTEXT(VERTEXATTRIB_GENERIC);                                      \
      Serialise_glVertexAttrib(index, count, eGL_NONE, GL_FALSE, value,                    \
                               TypeOr | CONCAT(Attrib_, paramtype));                       \
                                                                                           \
      m_ContextRecord->AddChunk(scope.Get());                                              \
    }                                                                                      \
  }

ATTRIB_FUNC(1, 1fv, 0, GLfloat)
ATTRIB_FUNC(2, 2fv, 0, GLfloat)
ATTRIB_FUNC(3, 3fv, 0, GLfloat)
ATTRIB_FUNC(4, 4fv, 0, GLfloat)


ATTRIB_FUNC(4, I4iv, Attrib_I, GLint)
ATTRIB_FUNC(4, I4uiv, Attrib_I, GLuint)

#undef ATTRIB_FUNC

#pragma endregion
