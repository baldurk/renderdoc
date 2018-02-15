/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

#include "../gl_driver.h"
#include "3rdparty/tinyfiledialogs/tinyfiledialogs.h"
#include "common/common.h"
#include "strings/string_utils.h"

enum GLbufferbitfield
{
};

DECLARE_REFLECTION_ENUM(GLbufferbitfield);

template <>
std::string DoStringise(const GLbufferbitfield &el)
{
  RDCCOMPILE_ASSERT(sizeof(GLbufferbitfield) == sizeof(GLbitfield) &&
                        sizeof(GLbufferbitfield) == sizeof(uint32_t),
                    "Fake bitfield enum must be uint32_t sized");

  BEGIN_BITFIELD_STRINGISE(GLbufferbitfield);
  {
    STRINGISE_BITFIELD_BIT(GL_DYNAMIC_STORAGE_BIT);
    STRINGISE_BITFIELD_BIT(GL_MAP_READ_BIT);
    STRINGISE_BITFIELD_BIT(GL_MAP_WRITE_BIT);
    STRINGISE_BITFIELD_BIT(GL_MAP_PERSISTENT_BIT);
    STRINGISE_BITFIELD_BIT(GL_MAP_COHERENT_BIT);
    STRINGISE_BITFIELD_BIT(GL_CLIENT_STORAGE_BIT);
  }
  END_BITFIELD_STRINGISE();
}

#pragma region Buffers

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenBuffers(SerialiserType &ser, GLsizei n, GLuint *buffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(buffer, GetResourceManager()->GetID(BufferRes(GetCtx(), *buffers)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glGenBuffers(1, &real);

    GLResource res = BufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(buffer, res);

    AddResource(buffer, ResourceType::Buffer, "Buffer");

    m_Buffers[live].resource = res;
    m_Buffers[live].curType = eGL_NONE;
    m_Buffers[live].creationFlags = BufferCategory::NoFlags;
  }

  return true;
}

void WrappedOpenGL::glGenBuffers(GLsizei n, GLuint *buffers)
{
  SERIALISE_TIME_CALL(m_Real.glGenBuffers(n, buffers));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = BufferRes(GetCtx(), buffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenBuffers(ser, 1, buffers + i);

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
      m_Buffers[id].creationFlags = BufferCategory::NoFlags;
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateBuffers(SerialiserType &ser, GLsizei n, GLuint *buffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(buffer, GetResourceManager()->GetID(BufferRes(GetCtx(), *buffers)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glCreateBuffers(1, &real);

    GLResource res = BufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(buffer, res);

    AddResource(buffer, ResourceType::Buffer, "Buffer");

    m_Buffers[live].resource = res;
    m_Buffers[live].curType = eGL_NONE;
    m_Buffers[live].creationFlags = BufferCategory::NoFlags;
  }

  return true;
}

void WrappedOpenGL::glCreateBuffers(GLsizei n, GLuint *buffers)
{
  SERIALISE_TIME_CALL(m_Real.glCreateBuffers(n, buffers));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = BufferRes(GetCtx(), buffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateBuffers(ser, 1, buffers + i);

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
      m_Buffers[id].creationFlags = BufferCategory::NoFlags;
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBuffer(SerialiserType &ser, GLenum target, GLuint bufferHandle)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target == eGL_NONE)
    {
      // ...
    }
    else if(buffer.name == 0)
    {
      m_Real.glBindBuffer(target, 0);
    }
    else
    {
      // if we're just loading, make sure not to trample state (e.g. element array buffer
      // binding in a VAO), since this is just a bind-to-create chunk.
      GLuint prevbuf = 0;
      if(IsLoading(m_State) && m_CurEventID == 0 && target != eGL_NONE)
        m_Real.glGetIntegerv(BufferBinding(target), (GLint *)&prevbuf);

      m_Real.glBindBuffer(target, buffer.name);

      m_Buffers[GetResourceManager()->GetID(buffer)].curType = target;
      m_Buffers[GetResourceManager()->GetID(buffer)].creationFlags |= MakeBufferCategory(target);

      if(IsLoading(m_State) && m_CurEventID == 0 && target != eGL_NONE)
        m_Real.glBindBuffer(target, prevbuf);
    }

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glBindBuffer(GLenum target, GLuint buffer)
{
  SERIALISE_TIME_CALL(m_Real.glBindBuffer(target, buffer));

  ContextData &cd = GetCtxData();

  size_t idx = BufferIdx(target);

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    if(buffer == 0)
      cd.m_BufferRecord[idx] = NULL;
    else
      cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBuffer(ser, target, buffer);

      if(cd.m_BufferRecord[idx])
        cd.m_BufferRecord[idx]->datatype = target;

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

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *r = cd.m_BufferRecord[idx] =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(!r)
    {
      RDCERR("Invalid/unrecognised buffer passed: glBindBuffer(%s, %u)", ToStr(target).c_str(),
             buffer);
      return;
    }

    // it's legal to re-type buffers, generate another BindBuffer chunk to rename
    if(r->datatype != target)
    {
      Chunk *chunk = NULL;

      r->LockChunks();
      for(;;)
      {
        Chunk *end = r->GetLastChunk();

        if(end->GetChunkType<GLChunk>() == GLChunk::glBindBuffer ||
           end->GetChunkType<GLChunk>() == GLChunk::glBindBufferARB)
        {
          SAFE_DELETE(end);

          r->PopChunk();

          continue;
        }

        break;
      }
      r->UnlockChunks();

      r->datatype = target;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glBindBuffer(ser, target, buffer);

        chunk = scope.Get();
      }

      r->AddChunk(chunk);
    }

    // element array buffer binding is vertex array record state, record there (if we've not just
    // stopped)
    if(IsBackgroundCapturing(m_State) && target == eGL_ELEMENT_ARRAY_BUFFER &&
       RecordUpdateCheck(cd.m_VertexArrayRecord))
    {
      GLuint vao = cd.m_VertexArrayRecord->Resource.name;

      // use glVertexArrayElementBuffer to ensure the vertex array is bound when we bind the
      // element buffer
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glVertexArrayElementBuffer);
      Serialise_glVertexArrayElementBuffer(ser, vao, buffer);

      cd.m_VertexArrayRecord->AddChunk(scope.Get());
    }

    // store as transform feedback record state
    if(IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GLuint feedback = cd.m_FeedbackRecord->Resource.name;

      // use glTransformFeedbackBufferBase to ensure the feedback object is bound when we bind the
      // buffer
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glTransformFeedbackBufferBase);
      Serialise_glTransformFeedbackBufferBase(ser, feedback, 0, buffer);

      cd.m_FeedbackRecord->AddChunk(scope.Get());
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
       target == eGL_ATOMIC_COUNTER_BUFFER)
    {
      if(IsBackgroundCapturing(m_State))
        GetResourceManager()->MarkDirtyResource(r->GetResourceID());
      else
        m_MissingTracks.insert(r->GetResourceID());
    }
  }
  else
  {
    m_Buffers[GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))].curType = target;
    m_Buffers[GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))].creationFlags |=
        MakeBufferCategory(target);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedBufferStorageEXT(SerialiserType &ser, GLuint bufferHandle,
                                                      GLsizeiptr size, const void *data,
                                                      GLbitfield flags)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_ELEMENT_LOCAL(bytesize, (uint64_t)size);
  SERIALISE_ELEMENT_ARRAY(data, bytesize);

  if(ser.IsWriting())
  {
    uint64_t offs = ser.GetWriter()->GetOffset() - bytesize;

    RDCASSERT((offs % 64) == 0);

    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(buffer);
    RDCASSERT(record);

    record->SetDataOffset(offs);
  }

  SERIALISE_ELEMENT_TYPED(GLbufferbitfield, flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // remove persistent flag - we will never persistently map so this is a nice
    // hint. It helps especially when self-hosting, as we don't want tons of
    // overhead added when we won't use it.
    flags &= ~GL_MAP_PERSISTENT_BIT;
    // can't have coherent without persistent, so remove as well
    flags &= ~GL_MAP_COHERENT_BIT;

    m_Real.glNamedBufferStorageEXT(buffer.name, (GLsizeiptr)bytesize, data, flags);

    m_Buffers[GetResourceManager()->GetID(buffer)].size = bytesize;

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::Common_glNamedBufferStorageEXT(ResourceId id, GLsizeiptr size, const void *data,
                                                   GLbitfield flags)
{
  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    RDCASSERTMSG("Couldn't identify object used in function. Unbound or bad GLuint?", record);

    if(record == NULL)
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferStorageEXT(ser, record->Resource.name, size, data, flags);

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
    if(flags & GL_MAP_PERSISTENT_BIT)
    {
      record->Map.persistentPtr = (byte *)m_Real.glMapNamedBufferRangeEXT(
          record->Resource.name, 0, size,
          GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_PERSISTENT_BIT);
      RDCASSERT(record->Map.persistentPtr);

      // persistent maps always need both sets of shadow storage, so allocate up front.
      record->AllocShadowStorage(size);

      // ensure shadow pointers have up to date data for diffing
      memcpy(record->GetShadowPtr(0), data, size);
      memcpy(record->GetShadowPtr(1), data, size);
    }
  }
  else
  {
    m_Buffers[id].size = size;
  }
}

void WrappedOpenGL::glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data,
                                            GLbitfield flags)
{
  byte *dummy = NULL;

  if(IsCaptureMode(m_State) && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, 0xdd, size);
    data = dummy;
  }

  SERIALISE_TIME_CALL(m_Real.glNamedBufferStorageEXT(buffer, size, data, flags));

  Common_glNamedBufferStorageEXT(GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)), size,
                                 data, flags);

  SAFE_DELETE_ARRAY(dummy);
}

void WrappedOpenGL::glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data,
                                         GLbitfield flags)
{
  // only difference to EXT function is size parameter, so just upcast
  glNamedBufferStorageEXT(buffer, size, data, flags);
}

void WrappedOpenGL::glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
  byte *dummy = NULL;

  if(IsCaptureMode(m_State) && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, 0xdd, size);
    data = dummy;
  }

  SERIALISE_TIME_CALL(m_Real.glBufferStorage(target, size, data, flags));

  if(IsCaptureMode(m_State))
    Common_glNamedBufferStorageEXT(GetCtxData().m_BufferRecord[BufferIdx(target)]->GetResourceID(),
                                   size, data, flags);
  else
    RDCERR("Internal buffers should be allocated via dsa interfaces");

  SAFE_DELETE_ARRAY(dummy);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedBufferDataEXT(SerialiserType &ser, GLuint bufferHandle,
                                                   GLsizeiptr size, const void *data, GLenum usage)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_ELEMENT_LOCAL(bytesize, (uint64_t)size);
  SERIALISE_ELEMENT_ARRAY(data, bytesize);

  if(ser.IsWriting())
  {
    uint64_t offs = ser.GetWriter()->GetOffset() - bytesize;

    RDCASSERT((offs % 64) == 0);

    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(buffer);
    RDCASSERT(record);

    record->SetDataOffset(offs);
  }

  SERIALISE_ELEMENT(usage);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glNamedBufferDataEXT(buffer.name, (GLsizeiptr)bytesize, data, usage);

    m_Buffers[GetResourceManager()->GetID(buffer)].size = bytesize;

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data,
                                         GLenum usage)
{
  byte *dummy = NULL;

  if(IsCaptureMode(m_State) && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, 0xdd, size);
    data = dummy;
  }

  SERIALISE_TIME_CALL(m_Real.glNamedBufferDataEXT(buffer, size, data, usage));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 buffer);

    if(record == NULL)
    {
      SAFE_DELETE_ARRAY(dummy);

      return;
    }

    // detect buffer orphaning and just update backing store
    if(IsBackgroundCapturing(m_State) && record->HasDataPtr() &&
       size == (GLsizeiptr)record->Length && usage == record->usage)
    {
      if(data)
        memcpy(record->GetDataPtr(), data, (size_t)size);
      else
        memset(record->GetDataPtr(), 0xbe, (size_t)size);

      SAFE_DELETE_ARRAY(dummy);

      return;
    }

    // if we're recreating the buffer, clear the record and add new chunks. Normally
    // we would just mark this record as dirty and pick it up on the capture frame as initial
    // data, but we don't support (if it's even possible) querying out size etc.
    // we need to add only the chunks required - glGenBuffers, glBindBuffer to current target,
    // and this buffer storage. All other chunks have no effect
    if(IsBackgroundCapturing(m_State) &&
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
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glGenBuffers);
        Serialise_glGenBuffers(ser, 1, &buffer);

        record->AddChunk(scope.Get(), id1);
      }

      // add glBindBuffer chunk
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glBindBuffer);
        Serialise_glBindBuffer(ser, record->datatype, buffer);

        record->AddChunk(scope.Get(), id2);
      }

      // we're about to add the buffer data chunk
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferDataEXT(ser, buffer, size, data, usage);

    Chunk *chunk = scope.Get();

    // if we've already created this is a renaming/data updating call. It should go in
    // the frame record so we can 'update' the buffer as it goes in the frame.
    // if we haven't created the buffer at all, it could be a mid-frame create and we
    // should place it in the resource record, to happen before the frame.
    if(IsActiveCapturing(m_State) && record->HasDataPtr())
    {
      // we could perhaps substitute this for a 'fake' glBufferSubData chunk?
      m_ContextRecord->AddChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Write);
    }
    else
    {
      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
      record->Length = (int32_t)size;
      record->usage = usage;
      record->DataInSerialiser = true;
    }
  }
  else
  {
    m_Buffers[GetResourceManager()->GetID(BufferRes(GetCtx(), buffer))].size = size;
  }

  SAFE_DELETE_ARRAY(dummy);
}

void WrappedOpenGL::glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
  // only difference to EXT function is size parameter, so just upcast
  glNamedBufferDataEXT(buffer, size, data, usage);
}

void WrappedOpenGL::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
  byte *dummy = NULL;

  if(IsCaptureMode(m_State) && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, 0xdd, size);
    data = dummy;
  }

  SERIALISE_TIME_CALL(m_Real.glBufferData(target, size, data, usage));

  size_t idx = BufferIdx(target);

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[idx];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record == NULL)
    {
      SAFE_DELETE_ARRAY(dummy);
      return;
    }

    // detect buffer orphaning and just update backing store
    if(IsBackgroundCapturing(m_State) && record->HasDataPtr() &&
       size == (GLsizeiptr)record->Length && usage == record->usage)
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
    if(IsBackgroundCapturing(m_State) &&
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
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glGenBuffers);
        Serialise_glGenBuffers(ser, 1, &buffer);

        record->AddChunk(scope.Get(), id1);
      }

      // add glBindBuffer chunk
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glBindBuffer);
        Serialise_glBindBuffer(ser, record->datatype, buffer);

        record->AddChunk(scope.Get(), id2);
      }

      // we're about to add the buffer data chunk
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferDataEXT(ser, buffer, size, data, usage);

    Chunk *chunk = scope.Get();

    // if we've already created this is a renaming/data updating call. It should go in
    // the frame record so we can 'update' the buffer as it goes in the frame.
    // if we haven't created the buffer at all, it could be a mid-frame create and we
    // should place it in the resource record, to happen before the frame.
    if(IsActiveCapturing(m_State) && record->HasDataPtr())
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
  else
  {
    RDCERR("Internal buffers should be allocated via dsa interfaces");
  }

  SAFE_DELETE_ARRAY(dummy);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedBufferSubDataEXT(SerialiserType &ser, GLuint bufferHandle,
                                                      GLintptr offsetPtr, GLsizeiptr size,
                                                      const void *data)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);

  SERIALISE_ELEMENT_LOCAL(bytesize, (uint64_t)size);
  SERIALISE_ELEMENT_ARRAY(data, bytesize);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glNamedBufferSubDataEXT(buffer.name, (GLintptr)offset, (GLsizeiptr)bytesize, data);
  }

  return true;
}

void WrappedOpenGL::glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                            const void *data)
{
  SERIALISE_TIME_CALL(m_Real.glNamedBufferSubDataEXT(buffer, offset, size, data));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record);

    if(record == NULL)
      return;

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferSubDataEXT(ser, buffer, offset, size, data);

    Chunk *chunk = scope.Get();

    if(IsActiveCapturing(m_State))
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

void WrappedOpenGL::glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                         const void *data)
{
  // only difference to EXT function is size parameter, so just upcast
  glNamedBufferSubDataEXT(buffer, offset, size, data);
}

void WrappedOpenGL::glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
  SERIALISE_TIME_CALL(m_Real.glBufferSubData(target, offset, size, data));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record == NULL)
      return;

    GLResource res = record->Resource;

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferSubDataEXT(ser, res.name, offset, size, data);

    Chunk *chunk = scope.Get();

    if(IsActiveCapturing(m_State))
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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedCopyBufferSubDataEXT(SerialiserType &ser,
                                                          GLuint readBufferHandle,
                                                          GLuint writeBufferHandle,
                                                          GLintptr readOffsetPtr,
                                                          GLintptr writeOffsetPtr, GLsizeiptr sizePtr)
{
  SERIALISE_ELEMENT_LOCAL(readBuffer, BufferRes(GetCtx(), readBufferHandle));
  SERIALISE_ELEMENT_LOCAL(writeBuffer, BufferRes(GetCtx(), writeBufferHandle));
  SERIALISE_ELEMENT_LOCAL(readOffset, (uint64_t)readOffsetPtr);
  SERIALISE_ELEMENT_LOCAL(writeOffset, (uint64_t)writeOffsetPtr);
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glNamedCopyBufferSubDataEXT(readBuffer.name, writeBuffer.name, (GLintptr)readOffset,
                                       (GLintptr)writeOffset, (GLsizeiptr)size);
  }

  return true;
}

void WrappedOpenGL::glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer,
                                                GLintptr readOffset, GLintptr writeOffset,
                                                GLsizeiptr size)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glNamedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *readrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), readBuffer));
    GLResourceRecord *writerecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), writeBuffer));
    RDCASSERT(readrecord && writerecord);

    if(m_HighTrafficResources.find(writerecord->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    if(GetResourceManager()->IsResourceDirty(readrecord->GetResourceID()) &&
       IsBackgroundCapturing(m_State))
    {
      m_HighTrafficResources.insert(writerecord->GetResourceID());
      GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedCopyBufferSubDataEXT(ser, readBuffer, writeBuffer, readOffset, writeOffset,
                                          size);

    Chunk *chunk = scope.Get();

    if(IsActiveCapturing(m_State))
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

void WrappedOpenGL::glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer,
                                             GLintptr readOffset, GLintptr writeOffset,
                                             GLsizeiptr size)
{
  glNamedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void WrappedOpenGL::glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                                        GLintptr writeOffset, GLsizeiptr size)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      m_Real.glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *readrecord = GetCtxData().m_BufferRecord[BufferIdx(readTarget)];
    GLResourceRecord *writerecord = GetCtxData().m_BufferRecord[BufferIdx(writeTarget)];
    RDCASSERT(readrecord && writerecord);

    if(m_HighTrafficResources.find(writerecord->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    if(GetResourceManager()->IsResourceDirty(readrecord->GetResourceID()) &&
       IsBackgroundCapturing(m_State))
    {
      m_HighTrafficResources.insert(writerecord->GetResourceID());
      GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedCopyBufferSubDataEXT(
        ser, readrecord->Resource.name, writerecord->Resource.name, readOffset, writeOffset, size);

    Chunk *chunk = scope.Get();

    if(IsActiveCapturing(m_State))
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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBufferBase(SerialiserType &ser, GLenum target, GLuint index,
                                               GLuint bufferHandle)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindBufferBase(target, index, buffer.name);

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
  ContextData &cd = GetCtxData();

  SERIALISE_TIME_CALL(m_Real.glBindBufferBase(target, index, buffer));

  if(IsCaptureMode(m_State))
  {
    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffer == 0)
      r = cd.m_BufferRecord[idx] = NULL;
    else
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(buffer && IsActiveCapturing(m_State))
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
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glBindBuffer);
        Serialise_glBindBuffer(ser, target, buffer);

        chunk = scope.Get();
      }

      r->datatype = target;

      r->AddChunk(chunk);
    }

    // store as transform feedback record state
    if(IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GLuint feedback = cd.m_FeedbackRecord->Resource.name;

      // use glTransformFeedbackBufferBase to ensure the feedback object is bound when we bind the
      // buffer
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glTransformFeedbackBufferBase);
      Serialise_glTransformFeedbackBufferBase(ser, feedback, index, buffer);

      cd.m_FeedbackRecord->AddChunk(scope.Get());
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(r && (target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
             target == eGL_ATOMIC_COUNTER_BUFFER))
    {
      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(r->GetResourceID());
      else
        GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBufferBase(ser, target, index, buffer);

      m_ContextRecord->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBufferRange(SerialiserType &ser, GLenum target, GLuint index,
                                                GLuint bufferHandle, GLintptr offsetPtr,
                                                GLsizeiptr sizePtr)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindBufferRange(target, index, buffer.name, (GLintptr)offset, (GLsizeiptr)size);

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                      GLsizeiptr size)
{
  ContextData &cd = GetCtxData();

  SERIALISE_TIME_CALL(m_Real.glBindBufferRange(target, index, buffer, offset, size));

  if(IsCaptureMode(m_State))
  {
    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffer == 0)
      r = cd.m_BufferRecord[idx] = NULL;
    else
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(buffer && IsActiveCapturing(m_State))
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
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glBindBuffer);
        Serialise_glBindBuffer(ser, target, buffer);

        chunk = scope.Get();
      }

      r->datatype = target;

      r->AddChunk(chunk);
    }

    // store as transform feedback record state
    if(IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GLuint feedback = cd.m_FeedbackRecord->Resource.name;

      // use glTransformFeedbackBufferRange to ensure the feedback object is bound when we bind the
      // buffer
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glTransformFeedbackBufferRange);
      Serialise_glTransformFeedbackBufferRange(ser, feedback, index, buffer, offset, (GLsizei)size);

      cd.m_FeedbackRecord->AddChunk(scope.Get());
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(r && (target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
             target == eGL_ATOMIC_COUNTER_BUFFER))
    {
      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(r->GetResourceID());
      else
        GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBufferRange(ser, target, index, buffer, offset, size);

      m_ContextRecord->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBuffersBase(SerialiserType &ser, GLenum target, GLuint first,
                                                GLsizei count, const GLuint *bufferHandles)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);

  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  std::vector<GLResource> buffers;

  if(ser.IsWriting())
  {
    buffers.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      buffers.push_back(BufferRes(GetCtx(), bufferHandles[i]));
  }

  SERIALISE_ELEMENT(buffers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<GLuint> bufs;
    bufs.reserve(count);
    for(GLsizei i = 0; i < count; i++)
    {
      bufs.push_back(buffers[i].name);

      AddResourceInitChunk(buffers[i]);
    }

    m_Real.glBindBuffersBase(target, first, count, bufs.data());
  }

  return true;
}

void WrappedOpenGL::glBindBuffersBase(GLenum target, GLuint first, GLsizei count,
                                      const GLuint *buffers)
{
  SERIALISE_TIME_CALL(m_Real.glBindBuffersBase(target, first, count, buffers));

  if(IsCaptureMode(m_State) && buffers && count > 0)
  {
    ContextData &cd = GetCtxData();

    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffers[0] == 0)
      r = cd.m_BufferRecord[idx] = NULL;
    else
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[0]));

    if(IsActiveCapturing(m_State))
    {
      FrameRefType refType = eFrameRef_Read;

      // these targets write to the buffer
      if(target == eGL_ATOMIC_COUNTER_BUFFER || target == eGL_COPY_WRITE_BUFFER ||
         target == eGL_PIXEL_PACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
         target == eGL_TRANSFORM_FEEDBACK_BUFFER)
        refType = eFrameRef_ReadBeforeWrite;

      for(GLsizei i = 0; i < count; i++)
      {
        if(buffers[i])
        {
          ResourceId id = GetResourceManager()->GetID(BufferRes(GetCtx(), buffers[i]));
          GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_ReadBeforeWrite);
          m_MissingTracks.insert(id);
        }
      }
    }

    for(int i = 0; i < count; i++)
    {
      GLResourceRecord *bufrecord =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i]));

      // it's legal to re-type buffers, generate another BindBuffer chunk to rename
      if(bufrecord->datatype != target)
      {
        Chunk *chunk = NULL;

        {
          USE_SCRATCH_SERIALISER();
          SCOPED_SERIALISE_CHUNK(GLChunk::glBindBuffer);
          Serialise_glBindBuffer(ser, target, buffers[i]);

          chunk = scope.Get();
        }

        bufrecord->datatype = target;

        bufrecord->AddChunk(chunk);
      }
    }

    // store as transform feedback record state
    if(IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GLuint feedback = cd.m_FeedbackRecord->Resource.name;

      for(int i = 0; i < count; i++)
      {
        // use glTransformFeedbackBufferBase to ensure the feedback object is bound when we bind the
        // buffer
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glTransformFeedbackBufferBase);
        Serialise_glTransformFeedbackBufferBase(ser, feedback, first + i, buffers[i]);

        cd.m_FeedbackRecord->AddChunk(scope.Get());
      }
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(r && (target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
             target == eGL_ATOMIC_COUNTER_BUFFER))
    {
      if(IsBackgroundCapturing(m_State))
      {
        for(int i = 0; i < count; i++)
          GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffers[i]));
      }
    }

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBuffersBase(ser, target, first, count, buffers);

      m_ContextRecord->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBuffersRange(SerialiserType &ser, GLenum target, GLuint first,
                                                 GLsizei count, const GLuint *bufferHandles,
                                                 const GLintptr *offsetPtrs,
                                                 const GLsizeiptr *sizePtrs)
{
  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  // Likewise need to upcast the offsets and sizes to 64-bit instead of serialising as-is.
  std::vector<GLResource> buffers;
  std::vector<uint64_t> offsets;
  std::vector<uint64_t> sizes;

  if(ser.IsWriting() && bufferHandles)
  {
    buffers.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      buffers.push_back(BufferRes(GetCtx(), bufferHandles[i]));
  }

  if(ser.IsWriting() && offsetPtrs)
  {
    offsets.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      offsets.push_back((uint64_t)offsetPtrs[i]);
  }

  if(ser.IsWriting() && sizePtrs)
  {
    sizes.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      sizes.push_back((uint64_t)sizePtrs[i]);
  }

  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(buffers);
  SERIALISE_ELEMENT(offsets);
  SERIALISE_ELEMENT(sizes);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<GLuint> bufs;
    std::vector<GLintptr> offs;
    std::vector<GLsizeiptr> sz;
    if(!buffers.empty())
    {
      bufs.reserve(count);
      for(GLsizei i = 0; i < count; i++)
      {
        bufs.push_back(buffers[i].name);

        AddResourceInitChunk(buffers[i]);
      }
    }
    if(!offsets.empty())
    {
      offs.reserve(count);
      for(GLsizei i = 0; i < count; i++)
        offs.push_back((GLintptr)offsets[i]);
    }
    if(!sizes.empty())
    {
      sz.reserve(count);
      for(GLsizei i = 0; i < count; i++)
        sz.push_back((GLintptr)sizes[i]);
    }

    m_Real.glBindBuffersRange(target, first, count, bufs.empty() ? NULL : bufs.data(),
                              offs.empty() ? NULL : offs.data(), sz.empty() ? NULL : sz.data());
  }

  return true;
}

void WrappedOpenGL::glBindBuffersRange(GLenum target, GLuint first, GLsizei count,
                                       const GLuint *buffers, const GLintptr *offsets,
                                       const GLsizeiptr *sizes)
{
  SERIALISE_TIME_CALL(m_Real.glBindBuffersRange(target, first, count, buffers, offsets, sizes));

  if(IsCaptureMode(m_State) && buffers && count > 0)
  {
    ContextData &cd = GetCtxData();

    size_t idx = BufferIdx(target);

    if(buffers[0] == 0)
      cd.m_BufferRecord[idx] = NULL;
    else
      cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[0]));

    if(IsActiveCapturing(m_State))
    {
      FrameRefType refType = eFrameRef_Read;

      // these targets write to the buffer
      if(target == eGL_ATOMIC_COUNTER_BUFFER || target == eGL_COPY_WRITE_BUFFER ||
         target == eGL_PIXEL_PACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
         target == eGL_TRANSFORM_FEEDBACK_BUFFER)
        refType = eFrameRef_ReadBeforeWrite;

      for(GLsizei i = 0; i < count; i++)
      {
        if(buffers[i])
        {
          ResourceId id = GetResourceManager()->GetID(BufferRes(GetCtx(), buffers[i]));
          GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_ReadBeforeWrite);
          m_MissingTracks.insert(id);
        }
      }
    }
    else
    {
      for(int i = 0; i < count; i++)
      {
        GLResourceRecord *r =
            GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i]));

        // it's legal to re-type buffers, generate another BindBuffer chunk to rename
        if(r->datatype != target)
        {
          Chunk *chunk = NULL;

          {
            USE_SCRATCH_SERIALISER();
            SCOPED_SERIALISE_CHUNK(GLChunk::glBindBuffer);
            Serialise_glBindBuffer(ser, target, buffers[i]);

            chunk = scope.Get();
          }

          r->datatype = target;

          r->AddChunk(chunk);
        }
      }
    }

    // store as transform feedback record state
    if(IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GLuint feedback = cd.m_FeedbackRecord->Resource.name;

      for(int i = 0; i < count; i++)
      {
        // use glTransformFeedbackBufferRange to ensure the feedback object is bound when we bind
        // the buffer
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glTransformFeedbackBufferRange);
        Serialise_glTransformFeedbackBufferRange(ser, feedback, first + i, buffers[i], offsets[i],
                                                 (GLsizei)sizes[i]);

        cd.m_FeedbackRecord->AddChunk(scope.Get());
      }
    }

    // immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
    if(target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
       target == eGL_ATOMIC_COUNTER_BUFFER)
    {
      if(IsBackgroundCapturing(m_State))
      {
        for(int i = 0; i < count; i++)
          GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffers[i]));
      }
    }

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBuffersRange(ser, target, first, count, buffers, offsets, sizes);

      m_ContextRecord->AddChunk(scope.Get());
    }
  }
}

void WrappedOpenGL::glInvalidateBufferData(GLuint buffer)
{
  m_Real.glInvalidateBufferData(buffer);

  if(IsBackgroundCapturing(m_State))
    GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
  else
    m_MissingTracks.insert(GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
}

void WrappedOpenGL::glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  m_Real.glInvalidateBufferSubData(buffer, offset, length);

  if(IsBackgroundCapturing(m_State))
    GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
  else
    m_MissingTracks.insert(GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
}

#pragma endregion

#pragma region Mapping

/************************************************************************
 *
 * Mapping tends to be the most complex/dense bit of the capturing process, as there are a lot of
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
 * Unmap's handling varies depending on the status of the map, as set above in
 * glMapNamedBufferRangeEXT.
 *
 * GLResourceRecord::Unmapped is an error case, indicating we haven't had a corresponding Map()
 * call.
 *
 * GLResourceRecord::Mapped_Read is a no-op as we can just discard it, the pointer we returned from
 * Map() was into our backing store.
 *
 * GLResourceRecord::Mapped_Ignore_Real is likewise a no-op as the GL pointer was updated directly
 * by user code, we weren't involved. However if we are now capturing a frame, it indicates a Map()
 * was made before this frame began, so this frame cannot be captured - we will need to try again
 * next frame, where a map will not be allowed to go into GLResourceRecord::Mapped_Ignore_Real.
 *
 * GLResourceRecord::Mapped_Write is the only case that will generate a serialised unmap chunk. If
 * we are idle, then all we need to do is map the 'real' GL buffer, copy across our backing store,
 * and unmap. We only map the range that was modified. Then everything is complete as the user code
 * updated our backing store. If we are capturing a frame, then we go into the serialise function
 * and serialise out a chunk.
 *
 * Finally we set the map status back to GLResourceRecord::Unmapped.
 *
 * When serialising out a map, we serialise the details of the map (which buffer, offset, length)
 * and then for non-invalidating maps of >512 byte buffers we perform a difference compare between
 * the two shadow storage buffers that were set up in glMapNamedBufferRangeEXT. We then serialise
 * out a buffer of the difference segment, and on replay we map and update this segment of the
 * buffer.
 *
 * The reason for finding the actual difference segment is that many maps will be of a large region
 * or even the whole buffer, but only update a small section, perhaps once per drawcall. So
 * serialising the entirety of a large buffer many many times can rapidly inflate the size of the
 * log. The savings from this can be many GBs as if a 4MB buffer is updated 1000 times, each time
 * only updating 1KB, this is a difference between 1MB and 4000MB in written data, most of which is
 * redundant in the last case.
 *
 *
 * glFlushMappedNamedBufferRangeEXT:
 *
 * Now consider the specialisation of the above, for maps that have GL_MAP_FLUSH_EXPLICIT_BIT
 * enabled.
 *
 * For the most part, these maps can be treated very similarly to normal maps, however in the case
 * of unmapping we will skip creating an unmap chunk and instead just allow the unmap to be
 * discarded. Instead we will serialise out a chunk for each glFlushMappedNamedBufferRangeEXT call.
 * We will also include flush explicit maps along with the others that we choose to map directly
 * when possible - so if we're capturing idle a flush explicit map will go straight to GL and be
 * handled as with GLResourceRecord::Mapped_Ignore_Real above.
 *
 * For this reason, if a map status is GLResourceRecord::Mapped_Ignore_Real then we simply pass the
 * flush range along to real GL. Again if we are capturing a frame now, this map has been 'missed'
 * and we must try again next frame to capture. Likewise as with Unmap GLResourceRecord::Unmapped is
 * an error, and for flushing we do not need to consider GLResourceRecord::Mapped_Read (it doesn't
 * make sense for this case).
 *
 * So we only serialise out a flush chunk if we are capturing a frame, and the map is correctly
 * GLResourceRecord::Mapped_Write. We clamp the flushed range to the size of the map (in case the
 * user code didn't do this). Unlike map we do not perform any difference compares, but rely on the
 * user to only flush the minimal range, and serialise the entire range out as a buffer. We also
 * update the shadow storage buffers so that if the buffer is subsequently mapped without flush
 * explicit, we have the 'current' contents to perform accurate compares with.
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
 * performance penalty even when RenderDoc is not used and it is simply the user code using GL
 * directly.
 *
 * The important thing is that persistent maps *must always* be intercepted regardless of
 * circumstance, as in theory they may never be mapped again. We get hints to help us with these
 * maps, as the buffers must have been created with glBufferStorage and must have the matching
 * persistent and optionally coherent bits set in the flags bitfield.
 *
 * Note also that non-coherent maps tend to go hand in hand with flush explicit maps (although this
 * is not guaranteed, it is highly likely).
 *
 * Non-coherent mappable buffers are GL-mapped on creation, and remain GL-mapped until their
 * destruction regardless of what user code does. We keep this 'real' GL-mapped buffer around
 * permanently but it is never returned to user code. Instead we handle maps otherwise as above
 * (taking care to always intercept), and return the user a pointer to our backing store. Then every
 * time a map flush happens instead of temporarily mapping and unmapping the GL buffer, we copy into
 * the appropriate place in our persistent map pointer. If an unmap happens and the map wasn't
 * flush-explicit, we copy the mapped region then. In this way we maintain correctness - the copies
 * are "delayed" by the time between user code writing into our memory, and us copying into the real
 * memory. However this is valid as it happens synchronously with a flush, unmap or other event and
 * by definition non-coherent maps aren't visible to the GPU until after those operations.
 *
 * There is also the function glMemoryBarrier with bit GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT. This has
 * the effect of acting as if all currently persistent-mapped regions were simultaneously flushed.
 * This is exactly how we implement it - we store a list of all current user persistent maps and any
 * time this bit is passed to glMemoryBarrier, we manually call into
 * glFlushMappedNamedBufferRangeEXT() with the appropriate parameters and handling is otherwise
 * identical.
 *
 * The final piece of the puzzle is coherent mapped buffers. Since we must break the coherency
 * carefully (see below), we map coherent buffers as non-coherent at creation time, the same as
 * above.
 *
 * To satisfy the demands of being coherent, we need to transparently propogate any changes between
 * the user written data and the 'real' memory, without any call to intercept - there would be no
 * need to call glMemoryBarrier or glFlushMappedNamedBufferRangeEXT. To do this, we have shadow
 * storage allocated as in the "normal" mapping path all the time, and we insert a manual call to
 * essentially the same code as glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT) in every
 * intercepted function call that could depend on the results of the buffer. We then check if any
 * write/change has happened by comparing to the shadow storage, and if so we perform a manual flush
 * of that changed region and update the shadow storage for next time.
 *
 * This "fake coherency" is the reason we can map the buffer as non-coherent, since we will be
 * performing copies and flushes manually to emulate the coherency to allow our interception in the
 * middle.
 *
 * By definition, there will be *many* of these places where the buffer results could be used, not
 * least any buffer copy, any texture copy (since a texture buffer could be created), any draw or
 * dispatch, etc. At each of these points there will be a cost for each coherent map of checking for
 * changes and it will scale with the size of the buffers. This is a large performance penalty but
 * one that can't be easily avoided. This is another reason why coherent maps should be avoided.
 *
 * Note that this also involves a behaviour change that affects correctness - a user write to memory
 * is not visible as soon as the write happens, but only on the next api point where the write could
 * have an effect. In correct code this should not be a problem as relying on any other behaviour
 * would be impossible - if you wrote into memory expecting commands in flight to be affected you
 * could not ensure correct ordering. However, obvious from that description, this is precisely a
 * race condition bug if user code did do that - which means race condition bugs will be hidden by
 * the nature of this tracing. This is unavoidable without the extreme performance hit of making all
 * coherent maps read-write, and performing a read-back at every sync point to find every change.
 * Which by itself may also hide race conditions anyway.
 *
 *
 * Implementation notes:
 *
 * The record->Map.ptr is the *offsetted* pointer, ie. a pointer to the beginning of the mapped
 * region, at record->Map.offset bytes from the start of the buffer.
 *
 * record->Map.persistentPtr points to the *base* of the buffer, not offsetted by any current map.
 *
 * Likewise the shadow storage pointers point to the base of a buffer-sized allocation each.
 *
 ************************************************************************/

void *WrappedOpenGL::glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                              GLbitfield access)
{
  // see above for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    bool directMap = false;

    // first check if we've already given up on these buffers
    if(IsBackgroundCapturing(m_State) &&
       m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end())
      directMap = true;

    if(!directMap && IsBackgroundCapturing(m_State) &&
       GetResourceManager()->IsResourceDirty(record->GetResourceID()))
      directMap = true;

    bool invalidateMap = (access & (GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_INVALIDATE_RANGE_BIT)) != 0;
    bool flushExplicitMap = (access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0;

    // if this map is writing and doesn't invalidate, or is flush explicit, map directly
    if(!directMap && (!invalidateMap || flushExplicitMap) && (access & GL_MAP_WRITE_BIT) &&
       IsBackgroundCapturing(m_State))
      directMap = true;

    // persistent maps must ALWAYS be intercepted
    if((access & GL_MAP_PERSISTENT_BIT) || record->Map.persistentPtr)
      directMap = false;

    bool verifyWrite = (RenderDoc::Inst().GetCaptureOptions().verifyMapWrites != 0);

    // must also intercept to verify writes
    if(verifyWrite)
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
    record->Map.verifyWrite = verifyWrite;

    // store a list of all persistent maps, and subset of all coherent maps
    if(access & GL_MAP_PERSISTENT_BIT)
    {
      Atomic::Inc64(&record->Map.persistentMaps);
      m_PersistentMaps.insert(record);
      if(record->Map.access & GL_MAP_COHERENT_BIT)
        m_CoherentMaps.insert(record);
    }

    // if we're doing a direct map, pass onto GL and return
    if(directMap)
    {
      record->Map.ptr = (byte *)m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);
      record->Map.status = GLResourceRecord::Mapped_Ignore_Real;

      return record->Map.ptr;
    }

    // only squirrel away read-only maps, read-write can just be treated as write-only
    if((access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == GL_MAP_READ_BIT)
    {
      byte *ptr = record->GetDataPtr();

      if(record->Map.persistentPtr)
        ptr = record->GetShadowPtr(0);

      RDCASSERT(ptr);

      ptr += offset;

      m_Real.glGetNamedBufferSubDataEXT(buffer, offset, length, ptr);

      record->Map.ptr = ptr;
      record->Map.status = GLResourceRecord::Mapped_Read;

      return ptr;
    }

    // below here, handle write maps to the backing store
    byte *ptr = record->GetDataPtr();

    RDCASSERT(ptr);
    {
      // persistent maps get particular handling
      if(access & GL_MAP_PERSISTENT_BIT)
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
      else if(IsActiveCapturing(m_State))
      {
        byte *shadow = (byte *)record->GetShadowPtr(0);

        // if we don't have a shadow pointer, need to allocate & initialise
        if(shadow == NULL)
        {
          GLint buflength;
          m_Real.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, &buflength);

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

              m_Real.glGetNamedBufferSubDataEXT(buffer, 0, buflength, shadow);
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

        record->Map.ptr = ptr = shadow + offset;
        record->Map.status = GLResourceRecord::Mapped_Write;
      }
      else if(IsBackgroundCapturing(m_State))
      {
        if(verifyWrite)
        {
          byte *shadow = record->GetShadowPtr(0);

          GLint buflength;
          m_Real.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, &buflength);

          // if we don't have a shadow pointer, need to allocate & initialise
          if(shadow == NULL)
          {
            // allocate our shadow storage
            record->AllocShadowStorage(buflength);
            shadow = (byte *)record->GetShadowPtr(0);
          }

          // if we're not invalidating, we need the existing contents
          if(!invalidateMap)
            memcpy(shadow, record->GetDataPtr(), buflength);
          else
            memset(shadow + offset, 0xcc, length);

          ptr = shadow;
        }

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

  return m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);
}

void *WrappedOpenGL::glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                           GLbitfield access)
{
  // only difference to EXT function is size parameter, so just upcast
  return glMapNamedBufferRangeEXT(buffer, offset, length, access);
}

void *WrappedOpenGL::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                                      GLbitfield access)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record)
      return glMapNamedBufferRangeEXT(record->Resource.name, offset, length, access);

    RDCERR("glMapBufferRange: Couldn't get resource record for target %x - no buffer bound?", target);
  }

  return m_Real.glMapBufferRange(target, offset, length, access);
}

// the glMapBuffer functions are equivalent to glMapBufferRange - so we just pass through
void *WrappedOpenGL::glMapNamedBufferEXT(GLuint buffer, GLenum access)
{
  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 buffer);

    if(record)
    {
      GLbitfield accessBits = 0;

      if(access == eGL_READ_ONLY)
        accessBits = eGL_MAP_READ_BIT;
      else if(access == eGL_WRITE_ONLY)
        accessBits = eGL_MAP_WRITE_BIT;
      else if(access == eGL_READ_WRITE)
        accessBits = eGL_MAP_READ_BIT | eGL_MAP_WRITE_BIT;
      return glMapNamedBufferRangeEXT(record->Resource.name, 0, (GLsizeiptr)record->Length,
                                      accessBits);
    }

    RDCERR("glMapNamedBufferEXT: Couldn't get resource record for buffer %x!", buffer);
  }

  return m_Real.glMapNamedBufferEXT(buffer, access);
}

void *WrappedOpenGL::glMapBuffer(GLenum target, GLenum access)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];

    if(record)
    {
      GLbitfield accessBits = 0;

      if(access == eGL_READ_ONLY)
        accessBits = eGL_MAP_READ_BIT;
      else if(access == eGL_WRITE_ONLY)
        accessBits = eGL_MAP_WRITE_BIT;
      else if(access == eGL_READ_WRITE)
        accessBits = eGL_MAP_READ_BIT | eGL_MAP_WRITE_BIT;
      return glMapNamedBufferRangeEXT(record->Resource.name, 0, (GLsizeiptr)record->Length,
                                      accessBits);
    }

    RDCERR("glMapBuffer: Couldn't get resource record for target %s - no buffer bound?",
           ToStr(target).c_str());
  }

  return m_Real.glMapBuffer(target, access);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glUnmapNamedBufferEXT(SerialiserType &ser, GLuint bufferHandle)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  GLResourceRecord *record = NULL;

  if(ser.IsWriting())
    record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), bufferHandle));

  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)record->Map.offset);
  SERIALISE_ELEMENT_LOCAL(length, (uint64_t)record->Map.length);

  uint64_t diffStart = 0;
  uint64_t diffEnd = (size_t)length;

  byte *MapWrittenData = NULL;

  if(ser.IsWriting())
  {
    MapWrittenData = record->Map.ptr;

    if(IsActiveCapturing(m_State) &&
       // don't bother checking diff range for tiny buffers
       length > 512 &&
       // if the map has a sub-range specified, trust the user to have specified
       // a minimal range, similar to glFlushMappedBufferRange, so don't find diff
       // range.
       record->Map.offset == 0 && length == record->Length &&
       // similarly for invalidate maps, we want to update the whole buffer
       !record->Map.invalidate)
    {
      size_t s = (size_t)diffStart;
      size_t e = (size_t)diffEnd;
      bool found =
          FindDiffRange(record->Map.ptr, record->GetShadowPtr(1) + offset, (size_t)length, s, e);
      diffStart = (uint64_t)s;
      diffEnd = (uint64_t)e;

      if(found)
      {
#if ENABLED(RDOC_DEVEL)
        static uint64_t saved = 0;

        saved += length - (diffEnd - diffStart);

        RDCDEBUG(
            "Mapped resource size %llu, difference: %llu -> %llu. Total bytes saved so far: %llu",
            length, diffStart, diffEnd, saved);
#endif

        length = diffEnd - diffStart;
      }
      else
      {
        diffStart = 0;
        diffEnd = 0;

        length = 1;
      }

      // update the data pointer to be rebased to the start of the diff data.
      MapWrittenData += diffStart;
    }

    // update shadow stores for future diff'ing
    if(IsActiveCapturing(m_State) && record->GetShadowPtr(1))
    {
      memcpy(record->GetShadowPtr(1) + (size_t)offset + (size_t)diffStart, MapWrittenData,
             size_t(diffEnd - diffStart));
    }
  }

  SERIALISE_ELEMENT(diffStart);
  SERIALISE_ELEMENT(diffEnd);

  SERIALISE_ELEMENT_ARRAY(MapWrittenData, length);

  SERIALISE_CHECK_READ_ERRORS();

  if(!IsStructuredExporting(m_State) && diffEnd > diffStart)
  {
    if(record && record->Map.persistentPtr)
    {
      // if we have a persistent mapped pointer, copy the range into the 'real' memory and
      // do a flush. Note the persistent pointer is always to the base of the buffer so we
      // need to account for the offset

      memcpy(record->Map.persistentPtr + (size_t)offset + (size_t)diffStart,
             record->Map.ptr + (size_t)diffStart, size_t(diffEnd - diffStart));
      m_Real.glFlushMappedNamedBufferRangeEXT(buffer.name, GLintptr(offset + diffStart),
                                              GLsizeiptr(diffEnd - diffStart));
    }
    else if(MapWrittenData && length > 0)
    {
      void *ptr = m_Real.glMapNamedBufferRangeEXT(buffer.name, (GLintptr)(offset + diffStart),
                                                  GLsizeiptr(diffEnd - diffStart), GL_MAP_WRITE_BIT);
      memcpy(ptr, MapWrittenData, size_t(diffEnd - diffStart));
      m_Real.glUnmapNamedBufferEXT(buffer.name);
    }
  }

  return true;
}

GLboolean WrappedOpenGL::glUnmapNamedBufferEXT(GLuint buffer)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    auto status = record->Map.status;

    if(IsActiveCapturing(m_State))
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
        if(IsActiveCapturing(m_State))
        {
          RDCERR(
              "Failed to cap frame - we saw an Unmap() that we didn't capture the corresponding "
              "Map() for");
          m_SuccessfulCapture = false;
          m_FailureReason = CaptureFailed_UncappedUnmap;
        }
        // need to do the real unmap
        ret = m_Real.glUnmapNamedBufferEXT(buffer);
        break;
      case GLResourceRecord::Mapped_Write:
      {
        if(record->Map.verifyWrite)
        {
          if(!record->VerifyShadowStorage())
          {
            string msg = StringFormat::Fmt(
                "Overwrite of %llu byte Map()'d buffer detected\n"
                "Breakpoint now to see callstack,\nor click 'Yes' to debugbreak.",
                record->Length);
            int res =
                tinyfd_messageBox("Map() overwrite detected!", msg.c_str(), "yesno", "error", 1);
            if(res == 1)
            {
              OS_DEBUG_BREAK();
            }
          }

          // copy from shadow to backing store, so we're consistent
          memcpy(record->GetDataPtr() + record->Map.offset, record->Map.ptr, record->Map.length);
        }

        if(record->Map.access & GL_MAP_FLUSH_EXPLICIT_BIT)
        {
          // do nothing, any flushes that happened were handled,
          // and we won't do any other updates here or make a chunk.
        }
        else if(IsActiveCapturing(m_State))
        {
          USE_SCRATCH_SERIALISER();
          SCOPED_SERIALISE_CHUNK(gl_CurChunk);
          Serialise_glUnmapNamedBufferEXT(ser, buffer);
          m_ContextRecord->AddChunk(scope.Get());
        }
        else if(IsBackgroundCapturing(m_State))
        {
          if(record->Map.persistentPtr)
          {
            // if we have a persistent mapped pointer, copy the range into the 'real' memory and
            // do a flush. Note the persistent pointer is always to the base of the buffer so we
            // need to account for the offset

            memcpy(record->Map.persistentPtr + record->Map.offset, record->Map.ptr,
                   record->Map.length);
            m_Real.glFlushMappedNamedBufferRangeEXT(buffer, record->Map.offset, record->Map.length);

            // update shadow storage
            memcpy(record->GetShadowPtr(1) + record->Map.offset, record->Map.ptr, record->Map.length);

            GetResourceManager()->MarkDirtyResource(record->GetResourceID());
          }
          else
          {
            // if we are here for background capturing, the app wrote directly into our backing
            // store memory. Just need to copy the data across to GL, no other work needed
            void *ptr =
                m_Real.glMapNamedBufferRangeEXT(buffer, (GLintptr)record->Map.offset,
                                                GLsizeiptr(record->Map.length), GL_MAP_WRITE_BIT);
            memcpy(ptr, record->Map.ptr, record->Map.length);
            m_Real.glUnmapNamedBufferEXT(buffer);
          }
        }

        break;
      }
    }

    // keep list of persistent & coherent maps up to date if we've
    // made the last unmap to a buffer
    if(record->Map.access & GL_MAP_PERSISTENT_BIT)
    {
      int64_t ref = Atomic::Dec64(&record->Map.persistentMaps);
      if(ref == 0)
      {
        m_PersistentMaps.erase(record);
        if(record->Map.access & GL_MAP_COHERENT_BIT)
          m_CoherentMaps.erase(record);
      }
    }

    record->Map.status = GLResourceRecord::Unmapped;

    return ret;
  }

  return m_Real.glUnmapNamedBufferEXT(buffer);
}

GLboolean WrappedOpenGL::glUnmapBuffer(GLenum target)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];

    if(record)
      return glUnmapNamedBufferEXT(record->Resource.name);

    RDCERR("glUnmapBuffer: Couldn't get resource record for target %s - no buffer bound?",
           ToStr(target).c_str());
  }

  return m_Real.glUnmapBuffer(target);
}

// offsetPtr here is from the start of the buffer, not the mapped region
template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFlushMappedNamedBufferRangeEXT(SerialiserType &ser,
                                                               GLuint bufferHandle,
                                                               GLintptr offsetPtr,
                                                               GLsizeiptr lengthPtr)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);
  SERIALISE_ELEMENT_LOCAL(length, (uint64_t)lengthPtr);

  GLResourceRecord *record = NULL;

  byte *FlushedData = NULL;

  if(ser.IsWriting())
  {
    record = GetResourceManager()->GetResourceRecord(buffer);

    FlushedData = record->Map.ptr + offset - record->Map.offset;

    // update the comparison buffer in case this buffer is subsequently mapped and we want to find
    // the difference region
    if(IsActiveCapturing(m_State) && record->GetShadowPtr(1))
    {
      memcpy(record->GetShadowPtr(1) + (size_t)offset, FlushedData, (size_t)length);
    }
  }

  SERIALISE_ELEMENT_ARRAY(FlushedData, length);

  SERIALISE_CHECK_READ_ERRORS();

  if(record && record->Map.persistentPtr)
  {
    // if we have a persistent mapped pointer, copy the range into the 'real' memory and
    // do a flush. Note the persistent pointer is always to the base of the buffer so we
    // need to account for the offset

    memcpy(record->Map.persistentPtr + (size_t)offset,
           record->Map.ptr + (size_t)(offset - record->Map.offset), (size_t)length);
    m_Real.glFlushMappedNamedBufferRangeEXT(buffer.name, (GLintptr)offset, (GLsizeiptr)length);
  }
  else if(buffer.name && FlushedData && length > 0)
  {
    // perform a map of the range and copy the data, to emulate the modified region being flushed
    void *ptr = m_Real.glMapNamedBufferRangeEXT(buffer.name, (GLintptr)offset, (GLsizeiptr)length,
                                                GL_MAP_WRITE_BIT);
    memcpy(ptr, FlushedData, (size_t)length);
    m_Real.glUnmapNamedBufferEXT(buffer.name);
  }

  return true;
}

void WrappedOpenGL::glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
  RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
               buffer);

  // only need to pay attention to flushes when in capframe. Otherwise (see above) we
  // treat the map as a normal map, and let ALL modified regions go through, flushed or not,
  // as this is legal - modified but unflushed regions are 'undefined' so we can just say
  // that modifications applying is our undefined behaviour.

  // note that we only want to flush the range with GL if we've actually
  // mapped it. Otherwise the map is 'virtual' and just pointing to our backing store data
  if(record && record->Map.status == GLResourceRecord::Mapped_Ignore_Real)
  {
    m_Real.glFlushMappedNamedBufferRangeEXT(buffer, offset, length);
  }

  if(IsActiveCapturing(m_State))
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
        if(offset < 0 || offset + length > record->Map.length)
        {
          RDCWARN("Flushed buffer range is outside of mapped range, clamping");

          // maintain the length/end boundary of the flushed range if the flushed offset
          // is below the mapped range
          if(offset < 0)
          {
            offset = 0;
            length += offset;
          }

          // clamp the length if it's beyond the mapped range.
          if(offset + length > record->Map.length)
          {
            length = (record->Map.length - offset);
          }
        }

        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glFlushMappedNamedBufferRangeEXT(ser, buffer, record->Map.offset + offset, length);
        m_ContextRecord->AddChunk(scope.Get());
      }
      // other statuses is GLResourceRecord::Mapped_Read
    }
  }
  else if(IsBackgroundCapturing(m_State))
  {
    // if this is a flush of a persistent map, we need to copy through to
    // the real pointer and perform a real flush.
    if(record && record->Map.persistentPtr)
    {
      memcpy(record->Map.persistentPtr + record->Map.offset + offset, record->Map.ptr + offset,
             length);
      m_Real.glFlushMappedNamedBufferRangeEXT(buffer, offset, length);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  // only difference to EXT function is size parameter, so just upcast
  glFlushMappedNamedBufferRangeEXT(buffer, offset, length);
}

void WrappedOpenGL::glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

    if(record)
      return glFlushMappedNamedBufferRangeEXT(record->Resource.name, offset, length);

    RDCERR(
        "glFlushMappedBufferRange: Couldn't get resource record for target %x - no buffer bound?",
        target);
  }

  return m_Real.glFlushMappedBufferRange(target, offset, length);
}

void WrappedOpenGL::PersistentMapMemoryBarrier(const set<GLResourceRecord *> &maps)
{
  PUSH_CURRENT_CHUNK;

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
      gl_CurChunk = GLChunk::glFlushMappedNamedBufferRangeEXT;
      glFlushMappedNamedBufferRangeEXT(record->Resource.name, GLintptr(diffStart),
                                       GLsizeiptr(diffEnd - diffStart));
    }
  }
}

#pragma endregion

#pragma region Transform Feedback

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenTransformFeedbacks(SerialiserType &ser, GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(feedback, GetResourceManager()->GetID(FeedbackRes(GetCtx(), *ids)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glGenTransformFeedbacks(1, &real);
    m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, real);
    m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);

    GLResource res = FeedbackRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(feedback, res);

    AddResource(feedback, ResourceType::StateObject, "Transform Feedback");
  }

  return true;
}

void WrappedOpenGL::glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
  SERIALISE_TIME_CALL(m_Real.glGenTransformFeedbacks(n, ids));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FeedbackRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenTransformFeedbacks(ser, 1, ids + i);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateTransformFeedbacks(SerialiserType &ser, GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(feedback, GetResourceManager()->GetID(FeedbackRes(GetCtx(), *ids)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glCreateTransformFeedbacks(1, &real);

    GLResource res = FeedbackRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(feedback, res);

    AddResource(feedback, ResourceType::StateObject, "Transform Feedback");
  }

  return true;
}

void WrappedOpenGL::glCreateTransformFeedbacks(GLsizei n, GLuint *ids)
{
  SERIALISE_TIME_CALL(m_Real.glCreateTransformFeedbacks(n, ids));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FeedbackRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateTransformFeedbacks(ser, 1, ids + i);

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

void WrappedOpenGL::glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)
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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTransformFeedbackBufferBase(SerialiserType &ser, GLuint xfbHandle,
                                                            GLuint index, GLuint bufferHandle)
{
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the trivial way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glTransformFeedbackBufferBase(xfb.name, index, buffer.name);
  }

  return true;
}

void WrappedOpenGL::glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
  SERIALISE_TIME_CALL(m_Real.glTransformFeedbackBufferBase(xfb, index, buffer));

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTransformFeedbackBufferBase(ser, xfb, index, buffer);

    if(IsActiveCapturing(m_State))
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                        eFrameRef_ReadBeforeWrite);
    }
    else if(xfb != 0)
    {
      GLResourceRecord *fbrecord =
          GetResourceManager()->GetResourceRecord(FeedbackRes(GetCtx(), xfb));

      fbrecord->AddChunk(scope.Get());

      if(buffer != 0)
        fbrecord->AddParent(GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer)));
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTransformFeedbackBufferRange(SerialiserType &ser, GLuint xfbHandle,
                                                             GLuint index, GLuint bufferHandle,
                                                             GLintptr offsetPtr, GLsizeiptr sizePtr)
{
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glTransformFeedbackBufferRange(xfb.name, index, buffer.name, (GLintptr)offset,
                                          (GLsizei)size);
  }

  return true;
}

void WrappedOpenGL::glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                                   GLintptr offset, GLsizeiptr size)
{
  SERIALISE_TIME_CALL(m_Real.glTransformFeedbackBufferRange(xfb, index, buffer, offset, size));

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTransformFeedbackBufferRange(ser, xfb, index, buffer, offset, size);

    if(IsActiveCapturing(m_State))
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                        eFrameRef_ReadBeforeWrite);
    }
    else if(xfb != 0)
    {
      GLResourceRecord *fbrecord =
          GetResourceManager()->GetResourceRecord(FeedbackRes(GetCtx(), xfb));

      fbrecord->AddChunk(scope.Get());

      if(buffer != 0)
        fbrecord->AddParent(GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer)));
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindTransformFeedback(SerialiserType &ser, GLenum target,
                                                      GLuint xfbHandle)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(xfb, FeedbackRes(GetCtx(), xfbHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBindTransformFeedback(target, xfb.name);
  }

  return true;
}

void WrappedOpenGL::glBindTransformFeedback(GLenum target, GLuint id)
{
  SERIALISE_TIME_CALL(m_Real.glBindTransformFeedback(target, id));

  GLResourceRecord *record = NULL;

  if(IsCaptureMode(m_State))
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

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindTransformFeedback(ser, target, id);

    m_ContextRecord->AddChunk(scope.Get());

    if(record)
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBeginTransformFeedback(SerialiserType &ser, GLenum primitiveMode)
{
  SERIALISE_ELEMENT(primitiveMode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBeginTransformFeedback(primitiveMode);
    m_ActiveFeedback = true;
  }

  return true;
}

void WrappedOpenGL::glBeginTransformFeedback(GLenum primitiveMode)
{
  SERIALISE_TIME_CALL(m_Real.glBeginTransformFeedback(primitiveMode));
  m_ActiveFeedback = true;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBeginTransformFeedback(ser, primitiveMode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPauseTransformFeedback(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    m_Real.glPauseTransformFeedback();
  }

  return true;
}

void WrappedOpenGL::glPauseTransformFeedback()
{
  SERIALISE_TIME_CALL(m_Real.glPauseTransformFeedback());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPauseTransformFeedback(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glResumeTransformFeedback(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    m_Real.glResumeTransformFeedback();
  }

  return true;
}

void WrappedOpenGL::glResumeTransformFeedback()
{
  SERIALISE_TIME_CALL(m_Real.glResumeTransformFeedback());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glResumeTransformFeedback(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEndTransformFeedback(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    m_Real.glEndTransformFeedback();
    m_ActiveFeedback = false;
  }

  return true;
}

void WrappedOpenGL::glEndTransformFeedback()
{
  SERIALISE_TIME_CALL(m_Real.glEndTransformFeedback());
  m_ActiveFeedback = false;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEndTransformFeedback(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

#pragma endregion

#pragma region Vertex Arrays

// NOTE: In each of the vertex array object functions below, we might not have the live buffer
// resource if it's is a pre-capture chunk, and the buffer was never referenced at all in the actual
// frame.
// The reason for this is that the VAO record doesn't add a parent of the buffer record - because
// that parent tracking quickly becomes stale with high traffic VAOs ignoring updates etc, so we
// don't rely on the parent connection and manually reference the buffer wherever it is actually
// used.

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribOffsetEXT(
    SerialiserType &ser, GLuint vaobjHandle, GLuint bufferHandle, GLuint index, GLint size,
    GLenum type, GLboolean normalized, GLsizei stride, GLintptr offsetPtr)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_TYPED(bool, normalized);
  SERIALISE_ELEMENT(stride);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    GLuint prevVAO = 0;
    m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    m_Real.glBindVertexArray(vaobj.name);

    // seems buggy when mixed and matched with new style vertex attrib binding, which we use for VAO
    // initial states. Since the spec defines how this function should work in terms of new style
    // bindings, just do that ourselves.

    // m_Real.glVertexArrayVertexAttribOffsetEXT(vaobj.name, buffer.name, index, size, type,
    //                                           normalized, stride, (GLintptr)offset);
    m_Real.glVertexArrayVertexAttribFormatEXT(vaobj.name, index, size, type, normalized, 0);
    m_Real.glVertexArrayVertexAttribBindingEXT(vaobj.name, index, index);
    if(stride == 0)
    {
      GLenum SizeEnum = size == 1 ? eGL_RED : size == 2 ? eGL_RG : size == 3 ? eGL_RGB : eGL_RGBA;
      stride = (uint32_t)GetByteSize(1, 1, 1, SizeEnum, type);
    }
    if(buffer.name == 0)
    {
      // ES allows client-memory pointers, which we override with temp buffers during capture.
      // For replay, discard these pointers to prevent driver complaining about "negative offsets".
      offset = 0;
    }
    m_Real.glVertexArrayBindVertexBufferEXT(vaobj.name, index, buffer.name, (GLintptr)offset, stride);

    m_Real.glBindVertexArray(prevVAO);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                       GLint size, GLenum type, GLboolean normalized,
                                                       GLsizei stride, GLintptr offset)
{
  SERIALISE_TIME_CALL(m_Real.glVertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type,
                                                                normalized, stride, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribOffsetEXT(ser, vaobj, buffer, index, size, type,
                                                     normalized, stride, offset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                                          GLboolean normalized, GLsizei stride, const void *pointer)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribPointer(index, size, type, normalized, stride, pointer));

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribOffsetEXT(
            ser, varecord ? varecord->Resource.name : 0, bufrecord ? bufrecord->Resource.name : 0,
            index, size, type, normalized, stride, (GLintptr)pointer);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribIOffsetEXT(SerialiserType &ser,
                                                                  GLuint vaobjHandle,
                                                                  GLuint bufferHandle, GLuint index,
                                                                  GLint size, GLenum type,
                                                                  GLsizei stride, GLintptr offsetPtr)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(stride);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    GLuint prevVAO = 0;
    m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    m_Real.glBindVertexArray(vaobj.name);

    // seems buggy when mixed and matched with new style vertex attrib binding, which we use for VAO
    // initial states. Since the spec defines how this function should work in terms of new style
    // bindings, just do that ourselves.

    // m_Real.glVertexArrayVertexAttribIOffsetEXT(vaobj.name, buffer.name, index, size, type,
    //                                            stride, (GLintptr)offset);
    m_Real.glVertexArrayVertexAttribIFormatEXT(vaobj.name, index, size, type, 0);
    m_Real.glVertexArrayVertexAttribBindingEXT(vaobj.name, index, index);
    if(stride == 0)
    {
      GLenum SizeEnum = size == 1 ? eGL_RED : size == 2 ? eGL_RG : size == 3 ? eGL_RGB : eGL_RGBA;
      stride = (uint32_t)GetByteSize(1, 1, 1, SizeEnum, type);
    }
    if(buffer.name == 0)
    {
      // ES allows client-memory pointers, which we override with temp buffers during capture.
      // For replay, discard these pointers to prevent driver complaining about "negative offsets".
      offset = 0;
    }
    m_Real.glVertexArrayBindVertexBufferEXT(vaobj.name, index, buffer.name, (GLintptr)offset, stride);

    m_Real.glBindVertexArray(prevVAO);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                        GLint size, GLenum type, GLsizei stride,
                                                        GLintptr offset)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribIOffsetEXT(ser, vaobj, buffer, index, size, type, stride,
                                                      offset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                           const void *pointer)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribIPointer(index, size, type, stride, pointer));

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribIOffsetEXT(ser, varecord ? varecord->Resource.name : 0,
                                                      bufrecord ? bufrecord->Resource.name : 0,
                                                      index, size, type, stride, (GLintptr)pointer);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribLOffsetEXT(SerialiserType &ser,
                                                                  GLuint vaobjHandle,
                                                                  GLuint bufferHandle, GLuint index,
                                                                  GLint size, GLenum type,
                                                                  GLsizei stride, GLintptr offsetPtr)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(stride);
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    GLuint prevVAO = 0;
    m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    m_Real.glBindVertexArray(vaobj.name);

    // seems buggy when mixed and matched with new style vertex attrib binding, which we use for VAO
    // initial states. Since the spec defines how this function should work in terms of new style
    // bindings, just do that ourselves.

    // m_Real.glVertexArrayVertexAttribIOffsetEXT(vaobj.name, buffer.name, index, size, type,
    //                                            stride, (GLintptr)offset);
    m_Real.glVertexArrayVertexAttribLFormatEXT(vaobj.name, index, size, type, 0);
    m_Real.glVertexArrayVertexAttribBindingEXT(vaobj.name, index, index);
    if(stride == 0)
    {
      GLenum SizeEnum = size == 1 ? eGL_RED : size == 2 ? eGL_RG : size == 3 ? eGL_RGB : eGL_RGBA;
      stride = (uint32_t)GetByteSize(1, 1, 1, SizeEnum, type);
    }
    if(buffer.name == 0)
    {
      // ES allows client-memory pointers, which we override with temp buffers during capture.
      // For replay, discard these pointers to prevent driver complaining about "negative offsets".
      offset = 0;
    }
    m_Real.glVertexArrayBindVertexBufferEXT(vaobj.name, index, buffer.name, (GLintptr)offset, stride);

    m_Real.glBindVertexArray(prevVAO);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                        GLint size, GLenum type, GLsizei stride,
                                                        GLintptr offset)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribLOffsetEXT(ser, vaobj, buffer, index, size, type, stride,
                                                      offset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                           const void *pointer)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribLPointer(index, size, type, stride, pointer));

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribLOffsetEXT(ser, varecord ? varecord->Resource.name : 0,
                                                      bufrecord ? bufrecord->Resource.name : 0,
                                                      index, size, type, stride, (GLintptr)pointer);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribBindingEXT(SerialiserType &ser,
                                                                  GLuint vaobjHandle,
                                                                  GLuint attribindex,
                                                                  GLuint bindingindex)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(attribindex);
  SERIALISE_ELEMENT(bindingindex);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    m_Real.glVertexArrayVertexAttribBindingEXT(vaobj.name, attribindex, bindingindex);
  }
  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex,
                                                        GLuint bindingindex)
{
  SERIALISE_TIME_CALL(m_Real.glVertexArrayVertexAttribBindingEXT(vaobj, attribindex, bindingindex));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribBindingEXT(ser, vaobj, attribindex, bindingindex);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribBinding(attribindex, bindingindex));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribBindingEXT(ser, varecord ? varecord->Resource.name : 0,
                                                      attribindex, bindingindex);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribFormatEXT(SerialiserType &ser,
                                                                 GLuint vaobjHandle,
                                                                 GLuint attribindex, GLint size,
                                                                 GLenum type, GLboolean normalized,
                                                                 GLuint relativeoffset)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(attribindex);
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_TYPED(bool, normalized);
  SERIALISE_ELEMENT(relativeoffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    m_Real.glVertexArrayVertexAttribFormatEXT(vaobj.name, attribindex, size, type, normalized,
                                              relativeoffset);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                       GLenum type, GLboolean normalized,
                                                       GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(m_Real.glVertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type,
                                                                normalized, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribFormatEXT(ser, vaobj, attribindex, size, type,
                                                     normalized, relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                                         GLboolean normalized, GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribFormatEXT(ser, varecord ? varecord->Resource.name : 0,
                                                     attribindex, size, type, normalized,
                                                     relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribIFormatEXT(SerialiserType &ser,
                                                                  GLuint vaobjHandle,
                                                                  GLuint attribindex, GLint size,
                                                                  GLenum type, GLuint relativeoffset)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(attribindex);
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(relativeoffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    m_Real.glVertexArrayVertexAttribIFormatEXT(vaobj.name, attribindex, size, type, relativeoffset);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                        GLenum type, GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribIFormatEXT(ser, vaobj, attribindex, size, type,
                                                      relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                                          GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribIFormat(attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribIFormatEXT(ser, varecord ? varecord->Resource.name : 0,
                                                      attribindex, size, type, relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribLFormatEXT(SerialiserType &ser,
                                                                  GLuint vaobjHandle,
                                                                  GLuint attribindex, GLint size,
                                                                  GLenum type, GLuint relativeoffset)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(attribindex);
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(relativeoffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    m_Real.glVertexArrayVertexAttribLFormatEXT(vaobj.name, attribindex, size, type, relativeoffset);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                        GLenum type, GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribLFormatEXT(ser, vaobj, attribindex, size, type,
                                                      relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type,
                                          GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribLFormat(attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribLFormatEXT(ser, varecord ? varecord->Resource.name : 0,
                                                      attribindex, size, type, relativeoffset);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribDivisorEXT(SerialiserType &ser,
                                                                  GLuint vaobjHandle, GLuint index,
                                                                  GLuint divisor)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(divisor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    // at the time of writing, AMD driver seems to not have this entry point
    if(m_Real.glVertexArrayVertexAttribDivisorEXT)
    {
      m_Real.glVertexArrayVertexAttribDivisorEXT(vaobj.name, index, divisor);
    }
    else
    {
      GLuint VAO = 0;
      m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);
      m_Real.glBindVertexArray(vaobj.name);
      m_Real.glVertexAttribDivisor(index, divisor);
      m_Real.glBindVertexArray(VAO);
    }
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor)
{
  SERIALISE_TIME_CALL(m_Real.glVertexArrayVertexAttribDivisorEXT(vaobj, index, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribDivisorEXT(ser, vaobj, index, divisor);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexAttribDivisor(GLuint index, GLuint divisor)
{
  SERIALISE_TIME_CALL(m_Real.glVertexAttribDivisor(index, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexAttribDivisorEXT(ser, varecord ? varecord->Resource.name : 0,
                                                      index, divisor);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEnableVertexArrayAttribEXT(SerialiserType &ser, GLuint vaobjHandle,
                                                           GLuint index)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    GLint prevVAO = 0;
    m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, &prevVAO);

    m_Real.glEnableVertexArrayAttribEXT(vaobj.name, index);

    // nvidia bug seems to sometimes change VAO binding in glEnableVertexArrayAttribEXT, although it
    // seems like it only happens if GL_DEBUG_OUTPUT_SYNCHRONOUS is NOT enabled.
    m_Real.glBindVertexArray(prevVAO);
  }
  return true;
}

void WrappedOpenGL::glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  SERIALISE_TIME_CALL(m_Real.glEnableVertexArrayAttribEXT(vaobj, index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glEnableVertexArrayAttribEXT(ser, vaobj, index);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glEnableVertexAttribArray(GLuint index)
{
  SERIALISE_TIME_CALL(m_Real.glEnableVertexAttribArray(index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glEnableVertexArrayAttribEXT(ser, varecord ? varecord->Resource.name : 0, index);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDisableVertexArrayAttribEXT(SerialiserType &ser, GLuint vaobjHandle,
                                                            GLuint index)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    GLint prevVAO = 0;
    m_Real.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, &prevVAO);

    m_Real.glDisableVertexArrayAttribEXT(vaobj.name, index);

    // nvidia bug seems to sometimes change VAO binding in glEnableVertexArrayAttribEXT, although it
    // seems like it only happens if GL_DEBUG_OUTPUT_SYNCHRONOUS is NOT enabled.
    m_Real.glBindVertexArray(prevVAO);
  }
  return true;
}

void WrappedOpenGL::glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  SERIALISE_TIME_CALL(m_Real.glDisableVertexArrayAttribEXT(vaobj, index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glDisableVertexArrayAttribEXT(ser, vaobj, index);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glDisableVertexAttribArray(GLuint index)
{
  SERIALISE_TIME_CALL(m_Real.glDisableVertexAttribArray(index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glDisableVertexArrayAttribEXT(ser, varecord ? varecord->Resource.name : 0, index);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenVertexArrays(SerialiserType &ser, GLsizei n, GLuint *arrays)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(array, GetResourceManager()->GetID(VertexArrayRes(GetCtx(), *arrays)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glGenVertexArrays(1, &real);
    m_Real.glBindVertexArray(real);
    m_Real.glBindVertexArray(0);

    GLResource res = VertexArrayRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(array, res);

    AddResource(array, ResourceType::StateObject, "Vertex Array");
  }

  return true;
}

void WrappedOpenGL::glGenVertexArrays(GLsizei n, GLuint *arrays)
{
  SERIALISE_TIME_CALL(m_Real.glGenVertexArrays(n, arrays));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenVertexArrays(ser, 1, arrays + i);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateVertexArrays(SerialiserType &ser, GLsizei n, GLuint *arrays)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(array, GetResourceManager()->GetID(VertexArrayRes(GetCtx(), *arrays)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glCreateVertexArrays(1, &real);

    GLResource res = VertexArrayRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(array, res);

    AddResource(array, ResourceType::StateObject, "Vertex Array");
  }

  return true;
}

void WrappedOpenGL::glCreateVertexArrays(GLsizei n, GLuint *arrays)
{
  SERIALISE_TIME_CALL(m_Real.glCreateVertexArrays(n, arrays));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateVertexArrays(ser, 1, arrays + i);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindVertexArray(SerialiserType &ser, GLuint vaobjHandle)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    m_Real.glBindVertexArray(vaobj.name);
  }

  return true;
}

void WrappedOpenGL::glBindVertexArray(GLuint array)
{
  SERIALISE_TIME_CALL(m_Real.glBindVertexArray(array));

  GLResourceRecord *record = NULL;

  if(IsCaptureMode(m_State))
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

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindVertexArray(ser, array);

    m_ContextRecord->AddChunk(scope.Get());
    if(record)
      GetResourceManager()->MarkVAOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayElementBuffer(SerialiserType &ser, GLuint vaobjHandle,
                                                         GLuint bufferHandle)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    // might not have the live resource if this is a pre-capture chunk, and the buffer was never
    // referenced at all in the actual frame
    if(buffer.name)
    {
      m_Buffers[GetResourceManager()->GetID(buffer)].curType = eGL_ELEMENT_ARRAY_BUFFER;
      m_Buffers[GetResourceManager()->GetID(buffer)].creationFlags |= BufferCategory::Index;
    }

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glVertexArrayElementBuffer(vaobj.name, buffer.name);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
  SERIALISE_TIME_CALL(m_Real.glVertexArrayElementBuffer(vaobj, buffer));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayElementBuffer(ser, vaobj, buffer);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayBindVertexBufferEXT(SerialiserType &ser,
                                                               GLuint vaobjHandle,
                                                               GLuint bindingindex,
                                                               GLuint bufferHandle,
                                                               GLintptr offsetPtr, GLsizei stride)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(bindingindex);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr);
  SERIALISE_ELEMENT(stride);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    if(buffer.name)
    {
      m_Buffers[GetResourceManager()->GetID(buffer)].curType = eGL_ARRAY_BUFFER;
      m_Buffers[GetResourceManager()->GetID(buffer)].creationFlags |= BufferCategory::Vertex;
    }

    m_Real.glVertexArrayBindVertexBufferEXT(vaobj.name, bindingindex, buffer.name, (GLintptr)offset,
                                            (GLsizei)stride);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex,
                                                     GLuint buffer, GLintptr offset, GLsizei stride)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayBindVertexBufferEXT(ser, vaobj, bindingindex, buffer, offset, stride);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset,
                                       GLsizei stride)
{
  SERIALISE_TIME_CALL(m_Real.glBindVertexBuffer(bindingindex, buffer, offset, stride));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      if(IsActiveCapturing(m_State) && bufrecord)
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayBindVertexBufferEXT(ser, varecord ? varecord->Resource.name : 0,
                                                   bindingindex, buffer, offset, stride);

        r->AddChunk(scope.Get());
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexBuffers(SerialiserType &ser, GLuint vaobjHandle,
                                                         GLuint first, GLsizei count,
                                                         const GLuint *bufferHandles,
                                                         const GLintptr *offsetPtrs,
                                                         const GLsizei *strides)
{
  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  // Likewise need to upcast the offsets to 64-bit instead of serialising as-is.
  std::vector<GLResource> buffers;
  std::vector<uint64_t> offsets;

  if(ser.IsWriting() && bufferHandles)
  {
    buffers.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      buffers.push_back(BufferRes(GetCtx(), bufferHandles[i]));
  }

  if(ser.IsWriting() && offsetPtrs)
  {
    offsets.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      offsets.push_back((uint64_t)offsetPtrs[i]);
  }

  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(buffers);
  SERIALISE_ELEMENT(offsets);
  SERIALISE_ELEMENT_ARRAY(strides, count);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<GLuint> bufs;
    std::vector<GLintptr> offs;
    if(!buffers.empty())
    {
      bufs.reserve(count);
      for(GLsizei i = 0; i < count; i++)
        bufs.push_back(buffers[i].name);
    }
    if(!offsets.empty())
    {
      offs.reserve(count);
      for(GLsizei i = 0; i < count; i++)
        offs.push_back((GLintptr)offsets[i]);
    }

    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    m_Real.glVertexArrayVertexBuffers(vaobj.name, first, count, bufs.empty() ? NULL : bufs.data(),
                                      offs.empty() ? NULL : offs.data(), strides);

    if(IsLoading(m_State))
    {
      for(GLsizei i = 0; i < count; i++)
      {
        m_Buffers[GetResourceManager()->GetID(buffers[i])].curType = eGL_ARRAY_BUFFER;
        m_Buffers[GetResourceManager()->GetID(buffers[i])].creationFlags |= BufferCategory::Vertex;
      }
    }
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                                               const GLuint *buffers, const GLintptr *offsets,
                                               const GLsizei *strides)
{
  SERIALISE_TIME_CALL(
      m_Real.glVertexArrayVertexBuffers(vaobj, first, count, buffers, offsets, strides));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexBuffers(ser, vaobj, first, count, buffers, offsets, strides);

        r->AddChunk(scope.Get());
      }

      if(IsActiveCapturing(m_State))
      {
        for(GLsizei i = 0; i < count; i++)
        {
          if(buffers != NULL && buffers[i] != 0)
          {
            GLResourceRecord *bufrecord =
                GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i]));
            if(bufrecord)
              GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(),
                                                                eFrameRef_Read);
          }
        }
      }
    }
  }
}

void WrappedOpenGL::glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers,
                                        const GLintptr *offsets, const GLsizei *strides)
{
  SERIALISE_TIME_CALL(m_Real.glBindVertexBuffers(first, count, buffers, offsets, strides));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexBuffers(ser, varecord ? varecord->Resource.name : 0, first,
                                             count, buffers, offsets, strides);

        r->AddChunk(scope.Get());
      }

      if(IsActiveCapturing(m_State))
      {
        for(GLsizei i = 0; i < count; i++)
        {
          if(buffers != NULL && buffers[i] != 0)
          {
            GLResourceRecord *bufrecord =
                GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i]));
            if(bufrecord)
              GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(),
                                                                eFrameRef_Read);
          }
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexArrayVertexBindingDivisorEXT(SerialiserType &ser,
                                                                   GLuint vaobjHandle,
                                                                   GLuint bindingindex,
                                                                   GLuint divisor)
{
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle));
  SERIALISE_ELEMENT(bindingindex);
  SERIALISE_ELEMENT(divisor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_FakeVAO;

    m_Real.glVertexArrayVertexBindingDivisorEXT(vaobj.name, bindingindex, divisor);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex,
                                                         GLuint divisor)
{
  SERIALISE_TIME_CALL(m_Real.glVertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexBindingDivisorEXT(ser, vaobj, bindingindex, divisor);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
  SERIALISE_TIME_CALL(m_Real.glVertexBindingDivisor(bindingindex, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? m_ContextRecord : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;
      if(IsActiveCapturing(m_State) && varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glVertexArrayVertexBindingDivisorEXT(ser, varecord ? varecord->Resource.name : 0,
                                                       bindingindex, divisor);

        r->AddChunk(scope.Get());
      }
    }
  }
}

void WrappedOpenGL::glDeleteBuffers(GLsizei n, const GLuint *buffers)
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
          if(record->Map.access & GL_MAP_COHERENT_BIT)
            m_CoherentMaps.erase(record);

          m_Real.glUnmapNamedBufferEXT(res.name);
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

void WrappedOpenGL::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glVertexAttrib(SerialiserType &ser, GLuint index, int count,
                                             GLenum type, GLboolean normalized, const void *value,
                                             AttribType attribtype)
{
  // this is used to share serialisation code amongst the brazillion variations
  SERIALISE_ELEMENT(attribtype).Hidden();
  AttribType attr = AttribType(attribtype & Attrib_typemask);

  // this is the number of components in the attribute (1,2,3,4). We hide it because it's part of
  // the function signature
  SERIALISE_ELEMENT(count).Hidden();

  SERIALISE_ELEMENT(index);

  // only serialise the type and normalized flags for packed commands
  if(attr == Attrib_packed)
  {
    SERIALISE_ELEMENT(type);
    SERIALISE_ELEMENT_TYPED(bool, normalized);
  }

  // create a union of all the value types - since we can only have up to 4, we can just make it
  // fixed size
  union
  {
    double d[4];
    float f[4];
    int32_t i32[4];
    uint32_t u32[4];
    int16_t i16[4];
    uint16_t u16[4];
    int8_t i8[4];
    uint8_t u8[4];
  } v;

  if(ser.IsWriting())
  {
    uint32_t byteCount = count;

    if(attr == Attrib_GLbyte)
      byteCount *= sizeof(char);
    else if(attr == Attrib_GLshort)
      byteCount *= sizeof(int16_t);
    else if(attr == Attrib_GLint)
      byteCount *= sizeof(int32_t);
    else if(attr == Attrib_GLubyte)
      byteCount *= sizeof(unsigned char);
    else if(attr == Attrib_GLushort)
      byteCount *= sizeof(uint16_t);
    else if(attr == Attrib_GLuint || attr == Attrib_packed)
      byteCount *= sizeof(uint32_t);

    RDCEraseEl(v);

    memcpy(v.f, value, byteCount);
  }

  // Serialise the array with the right type. We don't want to allocate new storage
  switch(attr)
  {
    case Attrib_GLdouble: ser.Serialise("values", v.d, SerialiserFlags::NoFlags); break;
    case Attrib_GLfloat: ser.Serialise("values", v.f, SerialiserFlags::NoFlags); break;
    case Attrib_GLint: ser.Serialise("values", v.i32, SerialiserFlags::NoFlags); break;
    case Attrib_packed:
    case Attrib_GLuint: ser.Serialise("values", v.u32, SerialiserFlags::NoFlags); break;
    case Attrib_GLshort: ser.Serialise("values", v.i16, SerialiserFlags::NoFlags); break;
    case Attrib_GLushort: ser.Serialise("values", v.u16, SerialiserFlags::NoFlags); break;
    case Attrib_GLbyte: ser.Serialise("values", v.i8, SerialiserFlags::NoFlags); break;
    default:
    case Attrib_GLubyte: ser.Serialise("values", v.u8, SerialiserFlags::NoFlags); break;
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(attr == Attrib_packed)
    {
      if(count == 1)
        m_Real.glVertexAttribP1uiv(index, type, normalized, v.u32);
      else if(count == 2)
        m_Real.glVertexAttribP2uiv(index, type, normalized, v.u32);
      else if(count == 3)
        m_Real.glVertexAttribP3uiv(index, type, normalized, v.u32);
      else if(count == 4)
        m_Real.glVertexAttribP4uiv(index, type, normalized, v.u32);
    }
    else if(attribtype & Attrib_I)
    {
      if(count == 1)
      {
        if(attr == Attrib_GLint)
          m_Real.glVertexAttribI1iv(index, v.i32);
        else if(attr == Attrib_GLuint)
          m_Real.glVertexAttribI1uiv(index, v.u32);
      }
      else if(count == 2)
      {
        if(attr == Attrib_GLint)
          m_Real.glVertexAttribI2iv(index, v.i32);
        else if(attr == Attrib_GLuint)
          m_Real.glVertexAttribI2uiv(index, v.u32);
      }
      else if(count == 3)
      {
        if(attr == Attrib_GLint)
          m_Real.glVertexAttribI3iv(index, v.i32);
        else if(attr == Attrib_GLuint)
          m_Real.glVertexAttribI3uiv(index, v.u32);
      }
      else
      {
        if(attr == Attrib_GLbyte)
          m_Real.glVertexAttribI4bv(index, v.i8);
        else if(attr == Attrib_GLshort)
          m_Real.glVertexAttribI4sv(index, v.i16);
        else if(attr == Attrib_GLint)
          m_Real.glVertexAttribI4iv(index, v.i32);
        else if(attr == Attrib_GLubyte)
          m_Real.glVertexAttribI4ubv(index, v.u8);
        else if(attr == Attrib_GLushort)
          m_Real.glVertexAttribI4usv(index, v.u16);
        else if(attr == Attrib_GLuint)
          m_Real.glVertexAttribI4uiv(index, v.u32);
      }
    }
    else if(attribtype & Attrib_L)
    {
      if(count == 1)
        m_Real.glVertexAttribL1dv(index, v.d);
      else if(count == 2)
        m_Real.glVertexAttribL2dv(index, v.d);
      else if(count == 3)
        m_Real.glVertexAttribL3dv(index, v.d);
      else if(count == 4)
        m_Real.glVertexAttribL4dv(index, v.d);
    }
    else if(attribtype & Attrib_N)
    {
      if(attr == Attrib_GLbyte)
        m_Real.glVertexAttrib4Nbv(index, v.i8);
      else if(attr == Attrib_GLshort)
        m_Real.glVertexAttrib4Nsv(index, v.i16);
      else if(attr == Attrib_GLint)
        m_Real.glVertexAttrib4Niv(index, v.i32);
      else if(attr == Attrib_GLubyte)
        m_Real.glVertexAttrib4Nubv(index, v.u8);
      else if(attr == Attrib_GLushort)
        m_Real.glVertexAttrib4Nusv(index, v.u16);
      else if(attr == Attrib_GLuint)
        m_Real.glVertexAttrib4Nuiv(index, v.u32);
    }
    else
    {
      if(count == 1)
      {
        if(attr == Attrib_GLdouble)
          m_Real.glVertexAttrib1dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib1fv(index, v.f);
        else if(attr == Attrib_GLshort)
          m_Real.glVertexAttrib1sv(index, v.i16);
      }
      else if(count == 2)
      {
        if(attr == Attrib_GLdouble)
          m_Real.glVertexAttrib2dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib2fv(index, v.f);
        else if(attr == Attrib_GLshort)
          m_Real.glVertexAttrib2sv(index, v.i16);
      }
      else if(count == 3)
      {
        if(attr == Attrib_GLdouble)
          m_Real.glVertexAttrib3dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib3fv(index, v.f);
        else if(attr == Attrib_GLshort)
          m_Real.glVertexAttrib3sv(index, v.i16);
      }
      else
      {
        if(attr == Attrib_GLdouble)
          m_Real.glVertexAttrib4dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          m_Real.glVertexAttrib4fv(index, v.f);
        else if(attr == Attrib_GLbyte)
          m_Real.glVertexAttrib4bv(index, v.i8);
        else if(attr == Attrib_GLshort)
          m_Real.glVertexAttrib4sv(index, v.i16);
        else if(attr == Attrib_GLint)
          m_Real.glVertexAttrib4iv(index, v.i32);
        else if(attr == Attrib_GLubyte)
          m_Real.glVertexAttrib4ubv(index, v.u8);
        else if(attr == Attrib_GLushort)
          m_Real.glVertexAttrib4usv(index, v.u16);
        else if(attr == Attrib_GLuint)
          m_Real.glVertexAttrib4uiv(index, v.u32);
      }
    }
  }

  return true;
}

#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype, ...)                        \
                                                                                  \
  void WrappedOpenGL::CONCAT(glVertexAttrib, suffix)(GLuint index, __VA_ARGS__)   \
                                                                                  \
  {                                                                               \
    SERIALISE_TIME_CALL(m_Real.CONCAT(glVertexAttrib, suffix)(index, ARRAYLIST)); \
                                                                                  \
    if(IsActiveCapturing(m_State))                                                \
    {                                                                             \
      USE_SCRATCH_SERIALISER();                                                   \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                        \
      const paramtype vals[] = {ARRAYLIST};                                       \
      Serialise_glVertexAttrib(ser, index, count, eGL_NONE, GL_FALSE, vals,       \
                               AttribType(TypeOr | CONCAT(Attrib_, paramtype)));  \
                                                                                  \
      m_ContextRecord->AddChunk(scope.Get());                                     \
    }                                                                             \
  }

#define ARRAYLIST x

ATTRIB_FUNC(1, 1f, 0, GLfloat, GLfloat x)
ATTRIB_FUNC(1, 1s, 0, GLshort, GLshort x)
ATTRIB_FUNC(1, 1d, 0, GLdouble, GLdouble x)
ATTRIB_FUNC(1, L1d, Attrib_L, GLdouble, GLdouble x)
ATTRIB_FUNC(1, I1i, Attrib_I, GLint, GLint x)
ATTRIB_FUNC(1, I1ui, Attrib_I, GLuint, GLuint x)

#undef ARRAYLIST
#define ARRAYLIST x, y

ATTRIB_FUNC(2, 2f, 0, GLfloat, GLfloat x, GLfloat y)
ATTRIB_FUNC(2, 2s, 0, GLshort, GLshort x, GLshort y)
ATTRIB_FUNC(2, 2d, 0, GLdouble, GLdouble x, GLdouble y)
ATTRIB_FUNC(2, L2d, Attrib_L, GLdouble, GLdouble x, GLdouble y)
ATTRIB_FUNC(2, I2i, Attrib_I, GLint, GLint x, GLint y)
ATTRIB_FUNC(2, I2ui, Attrib_I, GLuint, GLuint x, GLuint y)

#undef ARRAYLIST
#define ARRAYLIST x, y, z

ATTRIB_FUNC(3, 3f, 0, GLfloat, GLfloat x, GLfloat y, GLfloat z)
ATTRIB_FUNC(3, 3s, 0, GLshort, GLshort x, GLshort y, GLshort z)
ATTRIB_FUNC(3, 3d, 0, GLdouble, GLdouble x, GLdouble y, GLdouble z)
ATTRIB_FUNC(3, L3d, Attrib_L, GLdouble, GLdouble x, GLdouble y, GLdouble z)
ATTRIB_FUNC(3, I3i, Attrib_I, GLint, GLint x, GLint y, GLint z)
ATTRIB_FUNC(3, I3ui, Attrib_I, GLuint, GLuint x, GLuint y, GLuint z)

#undef ARRAYLIST
#define ARRAYLIST x, y, z, w

ATTRIB_FUNC(4, 4f, 0, GLfloat, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
ATTRIB_FUNC(4, 4s, 0, GLshort, GLshort x, GLshort y, GLshort z, GLshort w)
ATTRIB_FUNC(4, 4d, 0, GLdouble, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
ATTRIB_FUNC(4, L4d, Attrib_L, GLdouble, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
ATTRIB_FUNC(4, I4i, Attrib_I, GLint, GLint x, GLint y, GLint z, GLint w)
ATTRIB_FUNC(4, I4ui, Attrib_I, GLuint, GLuint x, GLuint y, GLuint z, GLuint w)
ATTRIB_FUNC(4, 4Nub, Attrib_N, GLubyte, GLubyte x, GLubyte y, GLubyte z, GLubyte w)

#undef ATTRIB_FUNC
#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype)                                      \
                                                                                           \
  void WrappedOpenGL::CONCAT(glVertexAttrib, suffix)(GLuint index, const paramtype *value) \
                                                                                           \
  {                                                                                        \
    m_Real.CONCAT(glVertexAttrib, suffix)(index, value);                                   \
                                                                                           \
    if(IsActiveCapturing(m_State))                                                         \
    {                                                                                      \
      USE_SCRATCH_SERIALISER();                                                            \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                                 \
      Serialise_glVertexAttrib(ser, index, count, eGL_NONE, GL_FALSE, value,               \
                               AttribType(TypeOr | CONCAT(Attrib_, paramtype)));           \
                                                                                           \
      m_ContextRecord->AddChunk(scope.Get());                                              \
    }                                                                                      \
  }

ATTRIB_FUNC(1, 1dv, 0, GLdouble)
ATTRIB_FUNC(2, 2dv, 0, GLdouble)
ATTRIB_FUNC(3, 3dv, 0, GLdouble)
ATTRIB_FUNC(4, 4dv, 0, GLdouble)
ATTRIB_FUNC(1, 1sv, 0, GLshort)
ATTRIB_FUNC(2, 2sv, 0, GLshort)
ATTRIB_FUNC(3, 3sv, 0, GLshort)
ATTRIB_FUNC(4, 4sv, 0, GLshort)
ATTRIB_FUNC(1, 1fv, 0, GLfloat)
ATTRIB_FUNC(2, 2fv, 0, GLfloat)
ATTRIB_FUNC(3, 3fv, 0, GLfloat)
ATTRIB_FUNC(4, 4fv, 0, GLfloat)
ATTRIB_FUNC(4, 4bv, 0, GLbyte)
ATTRIB_FUNC(4, 4iv, 0, GLint)
ATTRIB_FUNC(4, 4uiv, 0, GLuint)
ATTRIB_FUNC(4, 4usv, 0, GLushort)
ATTRIB_FUNC(4, 4ubv, 0, GLubyte)

ATTRIB_FUNC(1, L1dv, Attrib_L, GLdouble)
ATTRIB_FUNC(2, L2dv, Attrib_L, GLdouble)
ATTRIB_FUNC(3, L3dv, Attrib_L, GLdouble)
ATTRIB_FUNC(4, L4dv, Attrib_L, GLdouble)

ATTRIB_FUNC(1, I1iv, Attrib_I, GLint)
ATTRIB_FUNC(1, I1uiv, Attrib_I, GLuint)
ATTRIB_FUNC(2, I2iv, Attrib_I, GLint)
ATTRIB_FUNC(2, I2uiv, Attrib_I, GLuint)
ATTRIB_FUNC(3, I3iv, Attrib_I, GLint)
ATTRIB_FUNC(3, I3uiv, Attrib_I, GLuint)

ATTRIB_FUNC(4, I4bv, Attrib_I, GLbyte)
ATTRIB_FUNC(4, I4iv, Attrib_I, GLint)
ATTRIB_FUNC(4, I4sv, Attrib_I, GLshort)
ATTRIB_FUNC(4, I4ubv, Attrib_I, GLubyte)
ATTRIB_FUNC(4, I4uiv, Attrib_I, GLuint)
ATTRIB_FUNC(4, I4usv, Attrib_I, GLushort)

ATTRIB_FUNC(4, 4Nbv, Attrib_N, GLbyte)
ATTRIB_FUNC(4, 4Niv, Attrib_N, GLint)
ATTRIB_FUNC(4, 4Nsv, Attrib_N, GLshort)
ATTRIB_FUNC(4, 4Nubv, Attrib_N, GLubyte)
ATTRIB_FUNC(4, 4Nuiv, Attrib_N, GLuint)
ATTRIB_FUNC(4, 4Nusv, Attrib_N, GLushort)

#undef ATTRIB_FUNC
#define ATTRIB_FUNC(count, suffix, funcparam, passparam)                                       \
                                                                                               \
  void WrappedOpenGL::CONCAT(CONCAT(glVertexAttribP, count), suffix)(                          \
      GLuint index, GLenum type, GLboolean normalized, funcparam)                              \
                                                                                               \
  {                                                                                            \
    m_Real.CONCAT(CONCAT(glVertexAttribP, count), suffix)(index, type, normalized, value);     \
                                                                                               \
    if(IsActiveCapturing(m_State))                                                             \
    {                                                                                          \
      USE_SCRATCH_SERIALISER();                                                                \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                                     \
      Serialise_glVertexAttrib(ser, index, count, type, normalized, passparam, Attrib_packed); \
                                                                                               \
      m_ContextRecord->AddChunk(scope.Get());                                                  \
    }                                                                                          \
  }

ATTRIB_FUNC(1, ui, GLuint value, &value)
ATTRIB_FUNC(2, ui, GLuint value, &value)
ATTRIB_FUNC(3, ui, GLuint value, &value)
ATTRIB_FUNC(4, ui, GLuint value, &value)
ATTRIB_FUNC(1, uiv, const GLuint *value, value)
ATTRIB_FUNC(2, uiv, const GLuint *value, value)
ATTRIB_FUNC(3, uiv, const GLuint *value, value)
ATTRIB_FUNC(4, uiv, const GLuint *value, value)

#pragma endregion

INSTANTIATE_FUNCTION_SERIALISED(void, glGenBuffers, GLsizei n, GLuint *buffers);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateBuffers, GLsizei n, GLuint *buffers);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindBuffer, GLenum target, GLuint bufferHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedBufferStorageEXT, GLuint buffer, GLsizeiptr size,
                                const void *data, GLbitfield flags);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedBufferDataEXT, GLuint buffer, GLsizeiptr size,
                                const void *data, GLenum usage);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedBufferSubDataEXT, GLuint buffer, GLintptr offsetPtr,
                                GLsizeiptr size, const void *data);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedCopyBufferSubDataEXT, GLuint readBufferHandle,
                                GLuint writeBufferHandle, GLintptr readOffsetPtr,
                                GLintptr writeOffsetPtr, GLsizeiptr sizePtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindBufferBase, GLenum target, GLuint index, GLuint buffer);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindBufferRange, GLenum target, GLuint index,
                                GLuint bufferHandle, GLintptr offsetPtr, GLsizeiptr sizePtr);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindBuffersBase, GLenum target, GLuint first, GLsizei count,
                                const GLuint *bufferHandles);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindBuffersRange, GLenum target, GLuint first,
                                GLsizei count, const GLuint *bufferHandles, const GLintptr *offsets,
                                const GLsizeiptr *sizes);
INSTANTIATE_FUNCTION_SERIALISED(void, glUnmapNamedBufferEXT, GLuint buffer);
INSTANTIATE_FUNCTION_SERIALISED(void, glFlushMappedNamedBufferRangeEXT, GLuint buffer,
                                GLintptr offset, GLsizeiptr length);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenTransformFeedbacks, GLsizei n, GLuint *ids);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateTransformFeedbacks, GLsizei n, GLuint *ids);
INSTANTIATE_FUNCTION_SERIALISED(void, glTransformFeedbackBufferBase, GLuint xfbHandle, GLuint index,
                                GLuint bufferHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glTransformFeedbackBufferRange, GLuint xfbHandle,
                                GLuint index, GLuint bufferHandle, GLintptr offset, GLsizeiptr size);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindTransformFeedback, GLenum target, GLuint xfbHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glBeginTransformFeedback, GLenum primitiveMode);
INSTANTIATE_FUNCTION_SERIALISED(void, glPauseTransformFeedback);
INSTANTIATE_FUNCTION_SERIALISED(void, glResumeTransformFeedback);
INSTANTIATE_FUNCTION_SERIALISED(void, glEndTransformFeedback);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribOffsetEXT, GLuint vaobj,
                                GLuint buffer, GLuint index, GLint size, GLenum type,
                                GLboolean normalized, GLsizei stride, GLintptr offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribIOffsetEXT, GLuint vaobj,
                                GLuint buffer, GLuint index, GLint size, GLenum type,
                                GLsizei stride, GLintptr offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribLOffsetEXT, GLuint vaobj,
                                GLuint buffer, GLuint index, GLint size, GLenum type,
                                GLsizei stride, GLintptr pointer);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribBindingEXT, GLuint vaobj,
                                GLuint attribindex, GLuint bindingindex);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribFormatEXT, GLuint vaobj,
                                GLuint attribindex, GLint size, GLenum type, GLboolean normalized,
                                GLuint relativeoffset);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribIFormatEXT, GLuint vaobj,
                                GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribLFormatEXT, GLuint vaobj,
                                GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribDivisorEXT, GLuint vaobj,
                                GLuint index, GLuint divisor);
INSTANTIATE_FUNCTION_SERIALISED(void, glEnableVertexArrayAttribEXT, GLuint vaobj, GLuint index);
INSTANTIATE_FUNCTION_SERIALISED(void, glDisableVertexArrayAttribEXT, GLuint vaobj, GLuint index);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenVertexArrays, GLsizei n, GLuint *arrays);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateVertexArrays, GLsizei n, GLuint *arrays);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindVertexArray, GLuint arrayHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayElementBuffer, GLuint vaobjHandle,
                                GLuint bufferHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayBindVertexBufferEXT, GLuint vaobj,
                                GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexBuffers, GLuint vaobjHandle, GLuint first,
                                GLsizei count, const GLuint *buffers, const GLintptr *offsets,
                                const GLsizei *strides);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexArrayVertexBindingDivisorEXT, GLuint vaobj,
                                GLuint bindingindex, GLuint divisor);
INSTANTIATE_FUNCTION_SERIALISED(void, glVertexAttrib, GLuint index, int count, GLenum type,
                                GLboolean normalized, const void *value, AttribType attribtype);
