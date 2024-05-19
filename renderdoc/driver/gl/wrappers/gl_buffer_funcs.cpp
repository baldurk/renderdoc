/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "common/common.h"
#include "strings/string_utils.h"
#include "tinyfiledialogs/tinyfiledialogs.h"

enum GLbufferbitfield
{
  DYNAMIC_STORAGE_BIT = 0x0100,
  MAP_READ_BIT = 0x0001,
  MAP_WRITE_BIT = 0x0002,
  MAP_PERSISTENT_BIT = 0x0040,
  MAP_COHERENT_BIT = 0x0080,
  MAP_INVALIDATE_BUFFER_BIT = 0x0008,
  MAP_INVALIDATE_RANGE_BIT = 0x0004,
  MAP_FLUSH_EXPLICIT_BIT = 0x0010,
  MAP_UNSYNCHRONIZED_BIT = 0x0020,
  CLIENT_STORAGE_BIT_EXT = 0x0200,
};

DECLARE_REFLECTION_ENUM(GLbufferbitfield);

template <>
rdcstr DoStringise(const GLbufferbitfield &el)
{
  RDCCOMPILE_ASSERT(sizeof(GLbufferbitfield) == sizeof(GLbitfield) &&
                        sizeof(GLbufferbitfield) == sizeof(uint32_t),
                    "Fake bitfield enum must be uint32_t sized");

  BEGIN_BITFIELD_STRINGISE(GLbufferbitfield);
  {
    STRINGISE_BITFIELD_BIT_NAMED(GL_DYNAMIC_STORAGE_BIT, "GL_DYNAMIC_STORAGE_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_READ_BIT, "GL_MAP_READ_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_WRITE_BIT, "GL_MAP_WRITE_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_PERSISTENT_BIT, "GL_MAP_PERSISTENT_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_COHERENT_BIT, "GL_MAP_COHERENT_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_INVALIDATE_BUFFER_BIT, "GL_MAP_INVALIDATE_BUFFER_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_INVALIDATE_RANGE_BIT, "GL_MAP_INVALIDATE_RANGE_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_FLUSH_EXPLICIT_BIT, "GL_MAP_FLUSH_EXPLICIT_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_MAP_UNSYNCHRONIZED_BIT, "GL_MAP_UNSYNCHRONIZED_BIT");
    STRINGISE_BITFIELD_BIT_NAMED(GL_CLIENT_STORAGE_BIT, "GL_CLIENT_STORAGE_BIT");
  }
  END_BITFIELD_STRINGISE();
}

#pragma region Buffers

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenBuffers(SerialiserType &ser, GLsizei n, GLuint *buffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(buffer, GetResourceManager()->GetResID(BufferRes(GetCtx(), *buffers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenBuffers(1, &real);

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
  SERIALISE_TIME_CALL(GL.glGenBuffers(n, buffers));

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
  SERIALISE_ELEMENT_LOCAL(buffer, GetResourceManager()->GetResID(BufferRes(GetCtx(), *buffers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateBuffers(1, &real);

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
  SERIALISE_TIME_CALL(GL.glCreateBuffers(n, buffers));

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
      GL.glBindBuffer(target, 0);
    }
    else
    {
      // if we're just loading, make sure not to trample state (e.g. element array buffer
      // binding in a VAO), since this is just a bind-to-create chunk.
      GLuint prevbuf = 0;
      if(IsLoading(m_State) && m_CurEventID == 0)
        GL.glGetIntegerv(BufferBinding(target), (GLint *)&prevbuf);

      GL.glBindBuffer(target, buffer.name);

      m_Buffers[GetResourceManager()->GetResID(buffer)].curType = target;
      m_Buffers[GetResourceManager()->GetResID(buffer)].creationFlags |= MakeBufferCategory(target);

      if(IsLoading(m_State) && m_CurEventID == 0)
        GL.glBindBuffer(target, prevbuf);
    }

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glBindBuffer(GLenum target, GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glBindBuffer(target, buffer));

  ContextData &cd = GetCtxData();

  size_t idx = BufferIdx(target);

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    if(buffer == 0)
    {
      cd.m_BufferRecord[idx] = NULL;
    }
    else
    {
      cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

      if(cd.m_BufferRecord[idx] == NULL)
      {
        RDCERR("Called glBindBuffer with unrecognised or deleted buffer");
        return;
      }
    }

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

    // binding this buffer mutates VAO state, mark it as written.
    if(target == eGL_ELEMENT_ARRAY_BUFFER)
    {
      GLResourceRecord *varecord = cd.m_VertexArrayRecord;

      if(varecord)
        GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
    }

    // binding this buffer mutates XFB state, mark it as written.
    if(target == eGL_TRANSFORM_FEEDBACK_BUFFER)
    {
      GLResourceRecord *xfbrecord = cd.m_FeedbackRecord;

      if(xfbrecord)
        GetResourceManager()->MarkResourceFrameReferenced(xfbrecord->Resource,
                                                          eFrameRef_ReadBeforeWrite);
    }

    GetContextRecord()->AddChunk(chunk);
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
          end->Delete();

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
      GetResourceManager()->MarkDirtyResource(r->GetResourceID());
    }
  }
  else
  {
    m_Buffers[GetResourceManager()->GetResID(BufferRes(GetCtx(), buffer))].curType = target;
    m_Buffers[GetResourceManager()->GetResID(BufferRes(GetCtx(), buffer))].creationFlags |=
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

    flags |= GL_MAP_READ_BIT;

    GL.glNamedBufferStorageEXT(buffer.name, (GLsizeiptr)bytesize, data, flags);

    m_Buffers[GetResourceManager()->GetResID(buffer)].size = bytesize;

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
    memset(dummy, RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 0xdd : 0x0, size);
    data = dummy;

    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    if(record)
      record->Map.orphaned = true;
  }

  GLbitfield origflags = flags;

  // we always want to be able to read. This is true for persistent maps, as well as
  // non-invalidating maps which we need to readback the current contents before the map.
  flags |= GL_MAP_READ_BIT;

  SERIALISE_TIME_CALL(GL.glNamedBufferStorageEXT(buffer, size, data, flags));

  Common_glNamedBufferStorageEXT(GetResourceManager()->GetResID(BufferRes(GetCtx(), buffer)), size,
                                 data, origflags);

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
    memset(dummy, RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 0xdd : 0x0, size);
    data = dummy;

    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    if(record)
      record->Map.orphaned = true;
  }

  GLbitfield origflags = flags;

  // we always want to be able to read. This is true for persistent maps, as well as
  // non-invalidating maps which we need to readback the current contents before the map.
  flags |= GL_MAP_READ_BIT;

  SERIALISE_TIME_CALL(GL.glBufferStorage(target, size, data, flags));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify object used in function. Unbound or bad GLuint?", record);

    if(record)
      Common_glNamedBufferStorageEXT(record->GetResourceID(), size, data, origflags);
  }
  else
  {
    RDCERR("Internal buffers should be allocated via dsa interfaces");
  }

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
    ResourceId id = GetResourceManager()->GetResID(buffer);

    // never allow resizing down, even if the application did so. If we encounter that, adjust the
    // size and upload any data with a subdata call
    if(bytesize < m_Buffers[id].size)
    {
      GL.glNamedBufferDataEXT(buffer.name, (GLsizeiptr)m_Buffers[id].size, NULL, usage);

      GL.glNamedBufferSubDataEXT(buffer.name, 0, (GLsizeiptr)bytesize, data);
    }
    else
    {
      if(bytesize == 0)
      {
        // don't create 0 byte buffers, they just cause problems. Instead create the buffer as 4
        // bytes
        GL.glNamedBufferDataEXT(buffer.name, 4, NULL, usage);
      }
      else
      {
        GL.glNamedBufferDataEXT(buffer.name, (GLsizeiptr)bytesize, data, usage);
      }

      m_Buffers[id].size = bytesize;
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

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
    memset(dummy, RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 0xdd : 0x0, size);
    data = dummy;

    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    if(record)
      record->Map.orphaned = true;
  }

  if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                      eFrameRef_PartialWrite);
  }

  SERIALISE_TIME_CALL(GL.glNamedBufferDataEXT(buffer, size, data, usage));

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

      SAFE_DELETE_ARRAY(dummy);

      return;
    }

    const bool isResizingOrphan =
        (record->HasDataPtr() || (record->Length > 0 && size != (GLsizeiptr)record->Length));

    // if we're recreating the buffer, clear the record and add new chunks. Normally
    // we would just mark this record as dirty and pick it up on the capture frame as initial
    // data, but we don't support (if it's even possible) querying out size etc.
    // we need to add only the chunks required - glGenBuffers, glBindBuffer to current target,
    // and this buffer storage. All other chunks have no effect
    if(IsBackgroundCapturing(m_State) && isResizingOrphan)
    {
      // we need to maintain chunk ordering, so fetch the first two chunk IDs.
      // We should have at least two by this point - glGenBuffers and whatever gave the record
      // a size before.
      RDCASSERT(record->NumChunks() >= 2);

      // remove all but the first two chunks
      while(record->NumChunks() > 2)
      {
        Chunk *c = record->GetLastChunk();
        c->Delete();
        record->PopChunk();
      }

      int64_t id2 = record->GetLastChunkID();
      {
        Chunk *c = record->GetLastChunk();
        c->Delete();
        record->PopChunk();
      }

      int64_t id1 = record->GetLastChunkID();
      {
        Chunk *c = record->GetLastChunk();
        c->Delete();
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
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);

      // if this is a resizing call, we also need to store a copy in the record so future captures
      // have an accurate creation chunk. However we can't do that yet because this buffer may not
      // have initial contents. If we store the chunk immediately we'd corrupt data potentially used
      // earlier in the captured frame from the previous creation chunk :(. So push it into a list
      // that we'll 'apply' at the end of the frame capture.
      if(isResizingOrphan)
        m_BufferResizes.push_back({record, chunk->Duplicate()});
    }
    else
    {
      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
      record->DataInSerialiser = true;
    }

    // always update length and usage even during capture. If buffers resize mid-capture we'll
    // record them both into the active frame and the record, but we need an up to date length.
    record->Length = (int32_t)size;
    record->usage = usage;
  }
  else
  {
    m_Buffers[GetResourceManager()->GetResID(BufferRes(GetCtx(), buffer))].size = size;
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

  size_t idx = BufferIdx(target);

  if(IsCaptureMode(m_State) && data == NULL)
  {
    dummy = new byte[size];
    memset(dummy, RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 0xdd : 0x0, size);
    data = dummy;

    GLResourceRecord *record = GetCtxData().m_BufferRecord[idx];
    if(record)
      record->Map.orphaned = true;
  }

  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[idx];
    if(record)
      GetResourceManager()->MarkResourceFrameReferenced(record, eFrameRef_PartialWrite);
  }

  SERIALISE_TIME_CALL(GL.glBufferData(target, size, data, usage));

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
      // if data was NULL, it was set to dummy above.
      RDCASSERT(data);
      memcpy(record->GetDataPtr(), data, (size_t)size);

      SAFE_DELETE_ARRAY(dummy);

      return;
    }

    GLuint buffer = record->Resource.name;

    const bool isResizingOrphan =
        (record->HasDataPtr() || (record->Length > 0 && size != (GLsizeiptr)record->Length));

    // if we're recreating the buffer, clear the record and add new chunks. Normally
    // we would just mark this record as dirty and pick it up on the capture frame as initial
    // data, but we don't support (if it's even possible) querying out size etc.
    // we need to add only the chunks required - glGenBuffers, glBindBuffer to current target,
    // and this buffer storage. All other chunks have no effect
    if(IsBackgroundCapturing(m_State) && isResizingOrphan)
    {
      // we need to maintain chunk ordering, so fetch the first two chunk IDs.
      // We should have at least two by this point - glGenBuffers and whatever gave the record
      // a size before.
      RDCASSERT(record->NumChunks() >= 2);

      // remove all but the first two chunks
      while(record->NumChunks() > 2)
      {
        Chunk *c = record->GetLastChunk();
        c->Delete();
        record->PopChunk();
      }

      int64_t id2 = record->GetLastChunkID();
      {
        Chunk *c = record->GetLastChunk();
        c->Delete();
        record->PopChunk();
      }

      int64_t id1 = record->GetLastChunkID();
      {
        Chunk *c = record->GetLastChunk();
        c->Delete();
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
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);

      // if this is a resizing call, also store a copy in the record so future captures have an
      // accurate creation chunk
      if(isResizingOrphan)
        m_BufferResizes.push_back({record, chunk->Duplicate()});
    }
    else
    {
      record->AddChunk(chunk);
      record->SetDataPtr(chunk->GetData());
      record->DataInSerialiser = true;

      // if we're active capturing then we need to add a duplicate call in so that the data is
      // uploaded mid-frame, even if this is *also* the creation-type call.
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }
    }

    record->Length = size;
    record->usage = usage;
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
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();

  SERIALISE_ELEMENT_LOCAL(bytesize, (uint64_t)size).OffsetOrSize();
  SERIALISE_ELEMENT_ARRAY(data, bytesize).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(buffer)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    GL.glNamedBufferSubDataEXT(buffer.name, (GLintptr)offset, (GLsizeiptr)bytesize, data);

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                            const void *data)
{
  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    if(record)
      GetResourceManager()->MarkResourceFrameReferenced(record, eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glNamedBufferSubDataEXT(buffer, offset, size, data));

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
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
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
  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    if(record)
      GetResourceManager()->MarkResourceFrameReferenced(record, eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glBufferSubData(target, offset, size, data));

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
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
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
  SERIALISE_ELEMENT_LOCAL(readBuffer, BufferRes(GetCtx(), readBufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(writeBuffer, BufferRes(GetCtx(), writeBufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(readOffset, (uint64_t)readOffsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(writeOffset, (uint64_t)writeOffsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glNamedCopyBufferSubDataEXT(readBuffer.name, writeBuffer.name, (GLintptr)readOffset,
                                   (GLintptr)writeOffset, (GLsizeiptr)size);

    if(IsLoading(m_State) && m_CurEventID > 0)
    {
      AddEvent();

      ResourceId srcid = GetResourceManager()->GetResID(readBuffer);
      ResourceId dstid = GetResourceManager()->GetResID(writeBuffer);

      ActionDescription action;
      action.flags |= ActionFlags::Copy;

      action.copySource = GetResourceManager()->GetOriginalID(srcid);
      action.copyDestination = GetResourceManager()->GetOriginalID(dstid);

      AddAction(action);

      if(srcid == dstid)
      {
        m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::Copy));
      }
      else
      {
        m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::CopySrc));
        m_ResourceUses[dstid].push_back(EventUsage(m_CurEventID, ResourceUsage::CopyDst));
      }
    }
  }

  return true;
}

void WrappedOpenGL::glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer,
                                                GLintptr readOffset, GLintptr writeOffset,
                                                GLsizeiptr size)
{
  CoherentMapImplicitBarrier();

  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *writerecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), writeBuffer));

    if(writerecord)
      GetResourceManager()->MarkResourceFrameReferenced(writerecord->GetResourceID(),
                                                        eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(
      GL.glNamedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *readrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), readBuffer));
    GLResourceRecord *writerecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), writeBuffer));
    RDCASSERT(readrecord && writerecord);

    if(!readrecord || !writerecord)
      return;

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
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(readrecord->GetResourceID(), eFrameRef_Read);
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

  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *writerecord = GetCtxData().m_BufferRecord[BufferIdx(writeTarget)];
    if(writerecord)
      GetResourceManager()->MarkResourceFrameReferenced(writerecord->GetResourceID(),
                                                        eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *readrecord = GetCtxData().m_BufferRecord[BufferIdx(readTarget)];
    GLResourceRecord *writerecord = GetCtxData().m_BufferRecord[BufferIdx(writeTarget)];
    RDCASSERT(readrecord && writerecord);

    if(!readrecord || !writerecord)
      return;

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
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
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
    GL.glBindBufferBase(target, index, buffer.name);

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
  ContextData &cd = GetCtxData();

  SERIALISE_TIME_CALL(GL.glBindBufferBase(target, index, buffer));

  if(IsCaptureMode(m_State))
  {
    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffer == 0)
    {
      r = cd.m_BufferRecord[idx] = NULL;
    }
    else
    {
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

      if(r == NULL)
      {
        RDCERR("Called glBindBufferBase with unrecognised or deleted buffer");
        return;
      }
    }

    if(target == eGL_ATOMIC_COUNTER_BUFFER)
      cd.m_MaxAtomicBind = RDCMAX((GLint)index + 1, cd.m_MaxAtomicBind);
    else if(target == eGL_SHADER_STORAGE_BUFFER)
      cd.m_MaxSSBOBind = RDCMAX((GLint)index + 1, cd.m_MaxSSBOBind);

    if(IsActiveCapturing(m_State))
    {
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

      // binding this buffer mutates VAO state, mark it as written.
      if(target == eGL_ELEMENT_ARRAY_BUFFER)
      {
        GLResourceRecord *varecord = cd.m_VertexArrayRecord;

        if(varecord)
          GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      }

      // binding this buffer mutates XFB state, mark it as written.
      if(target == eGL_TRANSFORM_FEEDBACK_BUFFER)
      {
        GLResourceRecord *xfbrecord = cd.m_FeedbackRecord;

        if(xfbrecord)
          GetResourceManager()->MarkResourceFrameReferenced(xfbrecord->Resource,
                                                            eFrameRef_ReadBeforeWrite);
      }
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
      GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBufferBase(ser, target, index, buffer);

      GetContextRecord()->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBufferRange(SerialiserType &ser, GLenum target, GLuint index,
                                                GLuint bufferHandle, GLintptr offsetPtr,
                                                GLsizeiptr sizePtr)
{
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBindBufferRange(target, index, buffer.name, (GLintptr)offset, (GLsizeiptr)size);

    AddResourceInitChunk(buffer);
  }

  return true;
}

void WrappedOpenGL::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                      GLsizeiptr size)
{
  ContextData &cd = GetCtxData();

  SERIALISE_TIME_CALL(GL.glBindBufferRange(target, index, buffer, offset, size));

  if(IsCaptureMode(m_State))
  {
    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffer == 0)
    {
      r = cd.m_BufferRecord[idx] = NULL;
    }
    else
    {
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

      if(r == NULL)
      {
        RDCERR("Called glBindBufferBase with unrecognised or deleted buffer");
        return;
      }
    }

    if(target == eGL_ATOMIC_COUNTER_BUFFER)
      cd.m_MaxAtomicBind = RDCMAX((GLint)index + 1, cd.m_MaxAtomicBind);
    else if(target == eGL_SHADER_STORAGE_BUFFER)
      cd.m_MaxSSBOBind = RDCMAX((GLint)index + 1, cd.m_MaxSSBOBind);

    if(IsActiveCapturing(m_State))
    {
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

      // binding this buffer mutates VAO state, mark it as written.
      if(target == eGL_ELEMENT_ARRAY_BUFFER)
      {
        GLResourceRecord *varecord = cd.m_VertexArrayRecord;

        if(varecord)
          GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      }

      // binding this buffer mutates XFB state, mark it as written.
      if(target == eGL_TRANSFORM_FEEDBACK_BUFFER)
      {
        GLResourceRecord *xfbrecord = cd.m_FeedbackRecord;

        if(xfbrecord)
          GetResourceManager()->MarkResourceFrameReferenced(xfbrecord->Resource,
                                                            eFrameRef_ReadBeforeWrite);
      }
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
      GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindBufferRange(ser, target, index, buffer, offset, size);

      GetContextRecord()->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindBuffersBase(SerialiserType &ser, GLenum target, GLuint first,
                                                GLsizei count, const GLuint *bufferHandles)
{
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(first).Important();
  SERIALISE_ELEMENT(count);

  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  rdcarray<GLResource> buffers;

  if(ser.IsWriting())
  {
    buffers.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      buffers.push_back(BufferRes(GetCtx(), bufferHandles ? bufferHandles[i] : 0));
  }

  SERIALISE_ELEMENT(buffers).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<GLuint> bufs;
    bufs.reserve(count);
    for(GLsizei i = 0; i < count; i++)
    {
      bufs.push_back(buffers[i].name);

      AddResourceInitChunk(buffers[i]);
    }

    GL.glBindBuffersBase(target, first, count, bufs.data());
  }

  return true;
}

void WrappedOpenGL::glBindBuffersBase(GLenum target, GLuint first, GLsizei count,
                                      const GLuint *buffers)
{
  SERIALISE_TIME_CALL(GL.glBindBuffersBase(target, first, count, buffers));

  if(IsCaptureMode(m_State) && count > 0)
  {
    ContextData &cd = GetCtxData();

    size_t idx = BufferIdx(target);

    GLResourceRecord *r = NULL;

    if(buffers == NULL || buffers[0] == 0)
    {
      r = cd.m_BufferRecord[idx] = NULL;
    }
    else
    {
      r = cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[0]));

      if(r == NULL)
      {
        RDCERR("Called glBindBuffersBase with unrecognised or deleted buffer");
        return;
      }
    }

    if(target == eGL_ATOMIC_COUNTER_BUFFER)
      cd.m_MaxAtomicBind = RDCMAX((GLint)first + count, cd.m_MaxAtomicBind);
    else if(target == eGL_SHADER_STORAGE_BUFFER)
      cd.m_MaxSSBOBind = RDCMAX((GLint)first + count, cd.m_MaxSSBOBind);

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
        if(buffers && buffers[i])
        {
          ResourceId id = GetResourceManager()->GetResID(BufferRes(GetCtx(), buffers[i]));
          GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_ReadBeforeWrite);
          GetResourceManager()->MarkDirtyResource(id);
        }
      }

      // binding this buffer mutates VAO state, mark it as written.
      if(target == eGL_ELEMENT_ARRAY_BUFFER)
      {
        GLResourceRecord *varecord = cd.m_VertexArrayRecord;

        if(varecord)
          GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      }

      // binding this buffer mutates XFB state, mark it as written.
      if(target == eGL_TRANSFORM_FEEDBACK_BUFFER)
      {
        GLResourceRecord *xfbrecord = cd.m_FeedbackRecord;

        if(xfbrecord)
          GetResourceManager()->MarkResourceFrameReferenced(xfbrecord->Resource,
                                                            eFrameRef_ReadBeforeWrite);
      }
    }

    for(int i = 0; buffers && i < count; i++)
    {
      GLResourceRecord *bufrecord =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i]));

      // it's legal to re-type buffers, generate another BindBuffer chunk to rename
      if(bufrecord && bufrecord->datatype != target)
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
    if(buffers && IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
       RecordUpdateCheck(cd.m_FeedbackRecord))
    {
      GetResourceManager()->MarkResourceFrameReferenced(cd.m_FeedbackRecord->Resource,
                                                        eFrameRef_ReadBeforeWrite);
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
      if(IsBackgroundCapturing(m_State) && buffers)
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

      GetContextRecord()->AddChunk(scope.Get());
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
  rdcarray<GLResource> buffers;
  rdcarray<uint64_t> offsets;
  rdcarray<uint64_t> sizes;

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

  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(first).Important();
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).OffsetOrSize();
  SERIALISE_ELEMENT(sizes).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<GLuint> bufs;
    rdcarray<GLintptr> offs;
    rdcarray<GLsizeiptr> sz;
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

    GL.glBindBuffersRange(target, first, count, bufs.empty() ? NULL : bufs.data(),
                          offs.empty() ? NULL : offs.data(), sz.empty() ? NULL : sz.data());
  }

  return true;
}

void WrappedOpenGL::glBindBuffersRange(GLenum target, GLuint first, GLsizei count,
                                       const GLuint *buffers, const GLintptr *offsets,
                                       const GLsizeiptr *sizes)
{
  ContextData &cd = GetCtxData();

  if(buffers && IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
     RecordUpdateCheck(cd.m_FeedbackRecord))
  {
    GetResourceManager()->MarkResourceFrameReferenced(cd.m_FeedbackRecord->Resource,
                                                      eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glBindBuffersRange(target, first, count, buffers, offsets, sizes));

  if(IsCaptureMode(m_State) && count > 0)
  {
    size_t idx = BufferIdx(target);

    if(buffers == NULL || buffers[0] == 0)
    {
      cd.m_BufferRecord[idx] = NULL;
    }
    else
    {
      cd.m_BufferRecord[idx] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[0]));

      if(cd.m_BufferRecord[idx] == NULL)
      {
        RDCERR("Called glBindBuffersRange with unrecognised or deleted buffer");
        return;
      }
    }

    if(target == eGL_ATOMIC_COUNTER_BUFFER)
      cd.m_MaxAtomicBind = RDCMAX((GLint)first + count, cd.m_MaxAtomicBind);
    else if(target == eGL_SHADER_STORAGE_BUFFER)
      cd.m_MaxSSBOBind = RDCMAX((GLint)first + count, cd.m_MaxSSBOBind);

    if(IsActiveCapturing(m_State))
    {
      FrameRefType refType = eFrameRef_Read;

      // these targets write to the buffer
      if(target == eGL_ATOMIC_COUNTER_BUFFER || target == eGL_COPY_WRITE_BUFFER ||
         target == eGL_PIXEL_PACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
         target == eGL_TRANSFORM_FEEDBACK_BUFFER)
        refType = eFrameRef_ReadBeforeWrite;

      for(GLsizei i = 0; buffers && i < count; i++)
      {
        if(buffers[i])
        {
          ResourceId id = GetResourceManager()->GetResID(BufferRes(GetCtx(), buffers[i]));
          GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_ReadBeforeWrite);
          GetResourceManager()->MarkDirtyResource(id);
        }
      }

      // binding this buffer mutates VAO state, mark it as written.
      if(target == eGL_ELEMENT_ARRAY_BUFFER)
      {
        GLResourceRecord *varecord = cd.m_VertexArrayRecord;

        if(varecord)
          GetResourceManager()->MarkVAOReferenced(varecord->Resource, eFrameRef_ReadBeforeWrite);
      }

      // binding this buffer mutates XFB state, mark it as written.
      if(target == eGL_TRANSFORM_FEEDBACK_BUFFER)
      {
        GLResourceRecord *xfbrecord = cd.m_FeedbackRecord;

        if(xfbrecord)
          GetResourceManager()->MarkResourceFrameReferenced(xfbrecord->Resource,
                                                            eFrameRef_ReadBeforeWrite);
      }
    }
    else
    {
      for(int i = 0; buffers && i < count; i++)
      {
        GLResourceRecord *r =
            GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i]));

        // it's legal to re-type buffers, generate another BindBuffer chunk to rename
        if(r && r->datatype != target)
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
    if(buffers && IsBackgroundCapturing(m_State) && target == eGL_TRANSFORM_FEEDBACK_BUFFER &&
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
    if(buffers && (target == eGL_TRANSFORM_FEEDBACK_BUFFER || target == eGL_SHADER_STORAGE_BUFFER ||
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
      Serialise_glBindBuffersRange(ser, target, first, count, buffers, offsets, sizes);

      GetContextRecord()->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glInvalidateBufferData(SerialiserType &ser, GLuint bufferHandle)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId id = GetResourceManager()->GetResID(buffer);

    if(IsLoading(m_State))
      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Discard));

    GL.glInvalidateBufferData(buffer.name);

    if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
    {
      GLsizeiptr size = (GLsizeiptr)m_Buffers[id].size;

      bytebuf pattern;
      pattern.resize(AlignUp4(size));

      uint32_t value = 0xD15CAD3D;

      for(size_t i = 0; i < pattern.size(); i += 4)
        memcpy(&pattern[i], &value, sizeof(uint32_t));

      GL.glNamedBufferSubDataEXT(buffer.name, 0, size, pattern.data());
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      ActionDescription action;
      action.flags |= ActionFlags::Clear;

      action.copyDestination = GetResourceManager()->GetOriginalID(id);

      AddAction(action);

      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Discard));
    }
  }

  return true;
}

void WrappedOpenGL::glInvalidateBufferData(GLuint buffer)
{
  if(buffer && IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                      eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glInvalidateBufferData(buffer));

  if(IsCaptureMode(m_State))
  {
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glInvalidateBufferData(ser, buffer);

      GetContextRecord()->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glInvalidateBufferSubData(SerialiserType &ser, GLuint bufferHandle,
                                                        GLintptr offsetPtr, GLsizeiptr lengthPtr)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(length, (uint64_t)lengthPtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId id = GetResourceManager()->GetResID(buffer);

    if(IsLoading(m_State))
      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Discard));

    GL.glInvalidateBufferData(buffer.name);

    if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
    {
      bytebuf pattern;
      pattern.resize(AlignUp4((size_t)length));

      uint32_t value = 0xD15CAD3D;

      for(size_t i = 0; i < pattern.size(); i += 4)
        memcpy(&pattern[i], &value, sizeof(uint32_t));

      GL.glNamedBufferSubDataEXT(buffer.name, (GLintptr)offset, (GLsizeiptr)length, pattern.data());
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      ActionDescription action;
      action.flags |= ActionFlags::Clear;

      action.copyDestination = GetResourceManager()->GetOriginalID(id);

      AddAction(action);

      m_ResourceUses[id].push_back(EventUsage(m_CurEventID, ResourceUsage::Discard));
    }
  }

  return true;
}

void WrappedOpenGL::glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  if(buffer && IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                      eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glInvalidateBufferSubData(buffer, offset, length));

  if(IsCaptureMode(m_State))
  {
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glInvalidateBufferSubData(ser, buffer, offset, length);

      GetContextRecord()->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
    }
  }
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
 * Read-only maps can similarly be mapped directly because we don't need to intercept them at all.
 *
 * At this point, if the map is to be done directly, we pass the parameters onto GL and return
 * the result, marking the map with status GLResourceRecord::Mapped_Direct. Note that this
 * means we have no idea what happens with the map, and the buffer contents after that are to us
 * undefined. [NB: this is not true for persistent maps, see below]
 *
 * At this point we are intercepting a map for write, and it depends on whether or not we are
 * capturing a frame or just idle.
 *
 * If idle the handling is relatively simple, we just offset the pointer into our backing store and
 * return, marking the map as GLResourceRecord::Mapped_Write. Note that here we also increment a
 * counter, and if this counter reaches a high enough number (arbitrary limit), we mark the buffer
 * as high-traffic so that we'll stop intercepting maps and reduce overhead on this buffer.
 *
 * If frame capturing it is more complex. The backing store of the buffer must be preserved as it
 * will contain the contents at the start of the frame. Instead we allocate shadow storage for the
 * map. shadow[1] contains the contents of the mapped region as of the start of the map.
 * When first allocated, if the map is non-invalidating, it will be filled with the buffer contents
 * at that point. If the map is invalidating and we have buffer access verification enabled, it will
 * be reset to 0xcc to help find bugs caused by leaving valid data behind in invalidated buffer
 * memory.
 *
 * shadow[0] is the buffer that is returned to the user code. Whenever the same buffer range is
 * re-mapped shadow[0] is updated with the contents of shadow[1]. This way both buffers are always
 * identical and contain the latest buffer contents. These buffers are used later in unmap, but
 * Map() will return the pointer, and mark the map as GLResourceRecord::Mapped_Write.
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
 * GLResourceRecord::Mapped_Direct is a case we can't handle for normal maps, as the GL pointer was
 * updated directly by user code and we weren't involved. However if we hit this case and are
 * capturing a frame it indicates that a Map() was made before this frame began, so this frame
 * cannot be captured. We will need to try again next frame where a map will not be allowed to go
 * into GLResourceRecord::Mapped_Direct.
 *
 * GLResourceRecord::Mapped_Write is the only case that will generate a serialised unmap chunk. If
 * we are idle, then all we need to do is map the 'real' GL buffer, copy across our backing store
 * from shadow[0] where the user was writing, and unmap. We only map the range that was modified.
 * Then everything is complete as the user code updated our backing store. If we are capturing a
 * frame, then we go into the serialise function and serialise out a chunk.
 *
 * Finally we set the map status back to GLResourceRecord::Unmapped.
 *
 * When serialising out a map, we serialise the details of the map (which buffer, offset, length)
 * and then for non-invalidating maps of >512 byte buffers we perform a difference compare between
 * the two shadow storage buffers that were set up in glMapNamedBufferRangeEXT. As above, shadow[1]
 * contains the unmodified data from the start of the map, and shadow[0] contains the potentially
 * modified data. We then serialise out the difference segment, and on replay we map and update this
 * segment of the buffer.
 *
 * The reason for finding the actual difference segment is that many maps will be of a large region
 * or even the whole buffer, but only update a small section, perhaps once per action. So
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
 * handled as with GLResourceRecord::Mapped_Direct above.
 *
 * For this reason, if a map status is GLResourceRecord::Mapped_Direct then we simply pass the
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
 * Note also that non-coherent maps tend to go hand in hand with flush explicit maps (although this
 * is not guaranteed, it is highly likely). For this reason they don't need any special handling
 * aside from noting when a map exists to do an implicit flush when
 * glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT) is called - see below.
 *
 * Similarly, read-only maps are also 'easy' because we can just pass them through directly and
 * don't need to intercept at all.
 *
 * For coherent maps that can be written to, we modify the GL calls to ensure we can read from them
 * again. We return the GL mapped pointer directly to users so that when we aren't actively
 * capturing there is little overhead and the user code can run as normal. Our special handling only
 * needs to kick in when actively frame capturing.
 *
 * When frame capturing, we insert an implicit call to PersistentMapMemoryBarrier() over all
 * coherent maps whenever any GL function is called that could conceivably read from buffer memory.
 * This is at the very least all action calls but also any texture calls that could read from a PBO
 * or other calls. When PersistentMapMemoryBarrier() is called we check to see what has changed and
 * serialise it - similar in principle to an implicit call to glFlushMappedBufferRange() over the
 * whole buffer.
 *
 * The first time this happens we don't have any data to compare against - because we mapped
 * directly no shadow storage was allocated. This means the whole buffer is serialised the first
 * time, as it may have changed from any initial contents. We generate an implicit
 * glFlushMappedBufferRange call. Every time after that we compare against the shadow storage from
 * previously, serialise out any changes, and update our shadow storage.
 *
 * This is the reason why we make sure all maps can be read from - we compare our shadow storage vs
 * the actual direct GL pointer and not against any other shadow storage, since the direct GL
 * pointer is what the application fetched to write into.
 *
 * Note that this also involves a behaviour change that affects correctness - a user write to memory
 * is not visible as soon as the write happens, but only on the next api point where the write could
 * have an effect. In correct code this should not be a problem as relying on any other behaviour
 * would be impossible - if you wrote into memory expecting commands in flight to be affected you
 * could not ensure correct ordering. However, obvious from that description, this is precisely a
 * race condition bug if user code did do that - which means race condition bugs will be hidden by
 * the nature of this tracing.
 *
 * There is also the function glMemoryBarrier with bit GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT. This has
 * the effect of acting as if all currently persistent-mapped regions were simultaneously flushed.
 * This is exactly how we implement it - we store a list of all current user persistent maps and any
 * time this bit is passed to glMemoryBarrier, we manually call into
 * glFlushMappedNamedBufferRangeEXT() with the appropriate parameters and handling is otherwise
 * identical.
 *
 *
 * Implementation notes:
 *
 * The record->Map.ptr is the *offsetted* pointer, ie. a pointer to the beginning of the mapped
 * region, at record->Map.offset bytes from the start of the buffer.
 *
 * Likewise the shadow storage pointers point to the base of the mapped range and not to the base of
 * the buffer.
 *
 * Coherent persistent maps have their shadow storage freed at the end of every frame capture, to
 * ensure it does not hang around and pollute captures after that with stale data.
 *
 ************************************************************************/

void *WrappedOpenGL::glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                              GLbitfield access)
{
  // see above for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(!record)
    {
      RDCERR("Called glMapNamedBufferRange with unrecognised or deleted buffer");
      return GL.glMapNamedBufferRangeEXT(buffer, offset, length, access);
    }

    // if the buffer was recently orphaned, unset the flag. If the map is unsynchronised then sync
    // ourselves to allow our dummy upload of uninitialised 0xdddddddd to complete.
    if(record->Map.orphaned)
    {
      if(access & GL_MAP_UNSYNCHRONIZED_BIT)
        GL.glFinish();
      record->Map.orphaned = false;
    }

    // are we mapping directly - returning GL's pointer
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

    bool verifyWrite = RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess;

    bool persistent = false;

    // persistent maps are always made directly
    if(access & GL_MAP_PERSISTENT_BIT)
    {
      // reading must be available
      access |= GL_MAP_READ_BIT;

      // can't invalidate if we are reading. This flag is safe to remove because it only has a perf
      // impact - invalid data could by coincidence be the precise previous contents. We've also
      // already set invalidateMap above so if we're verifying buffer contents we know to check for
      // it.
      access &= ~(GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_INVALIDATE_RANGE_BIT);

      // also can't be unsynchronized for some reason
      access &= ~GL_MAP_UNSYNCHRONIZED_BIT;

      directMap = true;
      persistent = true;
    }

    // must also intercept to verify writes, where possible
    if(verifyWrite && (access & GL_MAP_WRITE_BIT) && !persistent)
      directMap = false;

    // read-only maps can be mapped directly as well
    if((access & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)) == GL_MAP_READ_BIT)
      directMap = true;

    // anything which is directly mapped for write becomes dirty/high traffic
    if(directMap && (access & GL_MAP_WRITE_BIT))
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

    // store the map properties
    record->Map.offset = offset;
    record->Map.length = length;
    record->Map.access = access;
    record->Map.invalidate = invalidateMap;
    record->Map.verifyWrite = verifyWrite;
    record->Map.persistent = persistent;

    // store a list of all persistent writing maps, and subset of all coherent maps
    uint32_t persistentWriteFlags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT;
    if((access & persistentWriteFlags) == persistentWriteFlags)
    {
      m_PersistentMaps.insert(record);
      if(record->Map.access & GL_MAP_COHERENT_BIT)
        m_CoherentMaps.insert(record);
    }

    // if we're doing a direct map, pass onto GL now and return the pointer
    if(directMap)
    {
      record->Map.ptr = (byte *)GL.glMapNamedBufferRangeEXT(buffer, offset, length, access);
      record->Map.status = GLResourceRecord::Mapped_Direct;

      return record->Map.ptr;
    }

    // below here, handle write maps to the backing store
    byte *backingStore = record->GetDataPtr();

    // we should have backing store data allocated by either glBufferData or glBufferStorage.
    RDCASSERT(backingStore);

    record->Map.status = GLResourceRecord::Mapped_Write;

    if(IsActiveCapturing(m_State))
    {
      // allocate shadow buffers
      record->AllocShadowStorage(length);

      // if we're not invalidating, we need the existing contents
      if(!invalidateMap)
        GL.glGetNamedBufferSubDataEXT(buffer, offset, length, record->GetShadowPtr(0));

      // copy into second shadow buffer ready for comparison later
      memcpy(record->GetShadowPtr(1), record->GetShadowPtr(0), length);

      // if we're invalidating, mark the whole range as 0xcc
      if(invalidateMap && verifyWrite)
      {
        memset(record->GetShadowPtr(0), 0xcc, length);
        memset(record->GetShadowPtr(1), 0xcc, length);
      }

      // return to the user our shadow[0] buffer to update, we'll check for differences against
      // shadow[1] on unmap
      record->Map.ptr = record->GetShadowPtr(0);
    }
    else if(IsBackgroundCapturing(m_State))
    {
      // return pointer into backing store so that the user updates it
      record->Map.ptr = backingStore + offset;

      // if we're verifying writes intercept with shadow storage
      if(verifyWrite)
      {
        // intercept with shadow storage
        record->AllocShadowStorage(length);

        // if we're not invalidating, we need the existing contents
        if(!invalidateMap)
          GL.glGetNamedBufferSubDataEXT(buffer, offset, length, record->GetShadowPtr(0));
        else
          memset(record->GetShadowPtr(0), 0xcc, length);

        record->Map.ptr = record->GetShadowPtr(0);
      }

      record->UpdateCount++;

      // mark as high-traffic if we update it often enough
      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }

    return record->Map.ptr;
  }

  return GL.glMapNamedBufferRangeEXT(buffer, offset, length, access);
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

  return GL.glMapBufferRange(target, offset, length, access);
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

  return GL.glMapNamedBufferEXT(buffer, access);
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

  return GL.glMapBuffer(target, access);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glUnmapNamedBufferEXT(SerialiserType &ser, GLuint bufferHandle)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  GLResourceRecord *record = NULL;

  if(ser.IsWriting())
    record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), bufferHandle));

  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)record->Map.offset).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(length, (uint64_t)record->Map.length).OffsetOrSize();

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
       !record->Map.invalidate &&
       // also not for persistent maps. If these are being unmapped, we save the whole buffer
       // contents - we check for differences in the case where a persistent map is held open.
       !record->Map.persistent)
    {
      size_t s = (size_t)diffStart;
      size_t e = (size_t)diffEnd;
      bool found = FindDiffRange(record->Map.ptr, record->GetShadowPtr(1), (size_t)length, s, e);
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
  }

  SERIALISE_ELEMENT(diffStart);
  SERIALISE_ELEMENT(diffEnd);

  SERIALISE_ELEMENT_ARRAY(MapWrittenData, length);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && diffEnd > diffStart && MapWrittenData && length > 0)
  {
    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(buffer)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    void *ptr = GL.glMapNamedBufferRangeEXT(buffer.name, (GLintptr)(offset + diffStart),
                                            GLsizeiptr(diffEnd - diffStart), GL_MAP_WRITE_BIT);
    if(ptr)
    {
      memcpy(ptr, MapWrittenData, size_t(diffEnd - diffStart));
    }
    else
    {
      RDCERR("Failed to map GL buffer");
      return false;
    }
    GL.glUnmapNamedBufferEXT(buffer.name);
  }

  return true;
}

GLboolean WrappedOpenGL::glUnmapNamedBufferEXT(GLuint buffer)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    if(!record)
    {
      RDCERR("Called glUnmapNamedBuffer with unrecognised or deleted buffer");
      return GL.glUnmapNamedBufferEXT(buffer);
    }

    auto status = record->Map.status;

    if(IsActiveCapturing(m_State))
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

    GetResourceManager()->MarkResourceFrameReferenced(record, eFrameRef_ReadBeforeWrite);

    GLboolean ret = GL_TRUE;

    switch(status)
    {
      case GLResourceRecord::Unmapped:
      {
        RDCERR("Unmapped buffer being passed to glUnmapBuffer");
        break;
      }
      case GLResourceRecord::Mapped_Direct:
      {
        // if this was a persistent map we either got unlucky and captured right during a frame that
        // unmapped a long-term persistent map, in which case failing the capture isn't a big deal,
        // or the application is doing something very strange and repeatedly mapping and unmapping
        // persistently at high enough frequency that all/most frames have such an unmap. In that
        // case we have to ensure we handle this otherwise it won't be captureable.
        if(IsActiveCapturing(m_State))
        {
          if((record->Map.access & GL_MAP_WRITE_BIT) == 0)
          {
            RDCASSERT(record->Map.persistent);
            RDCDEBUG("Read-only persistent map-unmap detected, ignoring.");
          }
          else if(record->Map.persistent)
          {
            // serialise the write to the buffer
            USE_SCRATCH_SERIALISER();
            SCOPED_SERIALISE_CHUNK(gl_CurChunk);
            Serialise_glUnmapNamedBufferEXT(ser, buffer);
            GetContextRecord()->AddChunk(scope.Get());
          }
          // if it was writeable, this is a problem while capturing a frame
          else if(record->Map.access & GL_MAP_WRITE_BIT)
          {
            RDCERR(
                "Failed to cap frame - we saw an Unmap() that we didn't capture the corresponding "
                "Map() for");
            m_SuccessfulCapture = false;
            m_FailureReason = CaptureFailed_UncappedUnmap;
          }
        }
        // need to do the real unmap
        ret = GL.glUnmapNamedBufferEXT(buffer);
        break;
      }
      case GLResourceRecord::Mapped_Write:
      {
        if(record->Map.verifyWrite)
        {
          if(!record->VerifyShadowStorage())
          {
            rdcstr msg = StringFormat::Fmt(
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

          // copy from updated shadow to backing store, so we're consistent with the non-verifying
          // path
          memcpy(record->GetDataPtr() + record->Map.offset, record->Map.ptr, record->Map.length);
        }

        if(record->Map.access & GL_MAP_FLUSH_EXPLICIT_BIT)
        {
          // do nothing, any flushes that happened were handled,
          // and we won't do any other updates here or make a chunk.
        }
        else if(IsActiveCapturing(m_State))
        {
          // serialise the write to the buffer
          USE_SCRATCH_SERIALISER();
          SCOPED_SERIALISE_CHUNK(gl_CurChunk);
          Serialise_glUnmapNamedBufferEXT(ser, buffer);
          GetContextRecord()->AddChunk(scope.Get());
        }

        {
          // the app wrote directly into our own pointer (either the backing store or a shadow
          // pointer depending on the mode above).
          // We need to copy the data across to GL
          void *ptr = GL.glMapNamedBufferRangeEXT(buffer, (GLintptr)record->Map.offset,
                                                  GLsizeiptr(record->Map.length), GL_MAP_WRITE_BIT);
          if(ptr)
            memcpy(ptr, record->Map.ptr, record->Map.length);
          else
            RDCERR("Failed to map buffer for update");
          GL.glUnmapNamedBufferEXT(buffer);
        }

        break;
      }
    }

    // keep list of persistent & coherent maps up to date if we've
    // made the last unmap to a buffer
    if(record->Map.access & GL_MAP_PERSISTENT_BIT)
    {
      m_PersistentMaps.erase(record);
      if(record->Map.access & GL_MAP_COHERENT_BIT)
        m_CoherentMaps.erase(record);
    }

    record->Map.status = GLResourceRecord::Unmapped;

    return ret;
  }

  return GL.glUnmapNamedBufferEXT(buffer);
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

  return GL.glUnmapBuffer(target);
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
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(length, (uint64_t)lengthPtr).OffsetOrSize();

  GLResourceRecord *record = NULL;

  byte *FlushedData = NULL;
  uint64_t MapOffset = 0;

  if(ser.IsWriting())
  {
    record = GetResourceManager()->GetResourceRecord(buffer);

    if(record->Map.ptr)
      FlushedData = record->Map.ptr + offset;
    MapOffset = record->Map.offset;
  }

  SERIALISE_ELEMENT_ARRAY(FlushedData, length);

  if(ser.VersionAtLeast(0x1F))
  {
    SERIALISE_ELEMENT(MapOffset).Hidden();

    // we don't need any special handling if this is missing - before 0x1F the map offset was baked
    // into the offset parameter, so letting it be 0 is fine.
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && buffer.name && FlushedData && length > 0)
  {
    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(buffer)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    // perform a map of the range and copy the data, to emulate the modified region being flushed
    void *ptr = GL.glMapNamedBufferRangeEXT(buffer.name, (GLintptr)(MapOffset + offset),
                                            (GLsizeiptr)length, GL_MAP_WRITE_BIT);
    if(ptr)
    {
      memcpy(ptr, FlushedData, size_t(length));
    }
    else
    {
      RDCERR("Failed to map GL buffer");
      return false;
    }
    GL.glUnmapNamedBufferEXT(buffer.name);
  }

  return true;
}

void WrappedOpenGL::glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  // see above glMapNamedBufferRangeEXT for high-level explanation of how mapping is handled

  GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
  RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
               buffer);

  if(!record)
    return;

  if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkResourceFrameReferenced(record, eFrameRef_ReadBeforeWrite);
  }

  // only need to pay attention to flushes when in capframe. Otherwise (see above) we
  // treat the map as a normal map, and let ALL modified regions go through, flushed or not,
  // as this is legal - modified but unflushed regions are 'undefined' so we can just say
  // that modifications applying is our undefined behaviour.

  // note that we only want to flush the range with GL if we've actually
  // mapped it. Otherwise the map is 'virtual' and just pointing to our backing store data
  if(record && record->Map.status == GLResourceRecord::Mapped_Direct &&
     gl_CurChunk != GLChunk::CoherentMapWrite)
  {
    GL.glFlushMappedNamedBufferRangeEXT(buffer, offset, length);
  }

  if(IsActiveCapturing(m_State))
  {
    if(record)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->Map.status == GLResourceRecord::Unmapped)
      {
        RDCWARN("Unmapped buffer being flushed, ignoring");
      }
      else if(record->Map.status == GLResourceRecord::Mapped_Direct)
      {
        if((record->Map.access & GL_MAP_WRITE_BIT) == 0)
        {
          // read-only map, we can ignore
        }
        else if(record->Map.persistent)
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
          Serialise_glFlushMappedNamedBufferRangeEXT(ser, buffer, offset, length);
          GetContextRecord()->AddChunk(scope.Get());
        }
        else
        {
          RDCERR(
              "Failed to cap frame - we saw an FlushMappedBuffer() that we didn't capture the "
              "corresponding Map() for");
          m_SuccessfulCapture = false;
          m_FailureReason = CaptureFailed_UncappedUnmap;
        }
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
        Serialise_glFlushMappedNamedBufferRangeEXT(ser, buffer, offset, length);
        GetContextRecord()->AddChunk(scope.Get());

        // update the comparison buffer
        if(IsActiveCapturing(m_State) && record->GetShadowPtr(1))
        {
          memcpy(record->GetShadowPtr(1) + (size_t)offset, record->GetShadowPtr(0) + (size_t)offset,
                 (size_t)length);
        }
      }
    }
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
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

  return GL.glFlushMappedBufferRange(target, offset, length);
}

void WrappedOpenGL::PersistentMapMemoryBarrier(const std::set<GLResourceRecord *> &maps)
{
  PUSH_CURRENT_CHUNK;

  // this function iterates over all the maps, checking for any changes between
  // the shadow pointers, and propogates that to 'real' GL

  for(std::set<GLResourceRecord *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
  {
    GLResourceRecord *record = *it;

    RDCASSERT(record && record->Map.ptr);

    if(record->Map.ptr)
    {
      size_t diffStart = 0, diffEnd = record->Map.length;
      bool found = true;

      if(record->GetShadowPtr(0))
        found = FindDiffRange(record->GetShadowPtr(0), record->Map.ptr, (size_t)record->Map.length,
                              diffStart, diffEnd);

      if(found && diffEnd > diffStart)
      {
        // update the modified region in the 'comparison' shadow buffer for next check
        if(record->GetShadowPtr(0) == NULL)
          record->AllocShadowStorage(record->Map.length);

        memcpy(record->GetShadowPtr(0) + diffStart, record->Map.ptr + diffStart, diffEnd - diffStart);

        // we use our own flush function so it will serialise chunks when necessary, and it
        // also handles copying into the persistent mapped pointer and flushing the real GL
        // buffer
        gl_CurChunk = GLChunk::CoherentMapWrite;
        glFlushMappedNamedBufferRangeEXT(record->Resource.name, GLintptr(diffStart),
                                         GLsizeiptr(diffEnd - diffStart));
      }
    }
  }
}

#pragma endregion

#pragma region Transform Feedback

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenTransformFeedbacks(SerialiserType &ser, GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(feedback, GetResourceManager()->GetResID(FeedbackRes(GetCtx(), *ids)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenTransformFeedbacks(1, &real);
    GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, real);
    GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);

    GLResource res = FeedbackRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(feedback, res);

    AddResource(feedback, ResourceType::StateObject, "Transform Feedback");
  }

  return true;
}

void WrappedOpenGL::glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
  SERIALISE_TIME_CALL(GL.glGenTransformFeedbacks(n, ids));

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
  SERIALISE_ELEMENT_LOCAL(feedback, GetResourceManager()->GetResID(FeedbackRes(GetCtx(), *ids)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateTransformFeedbacks(1, &real);

    GLResource res = FeedbackRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(feedback, res);

    AddResource(feedback, ResourceType::StateObject, "Transform Feedback");
  }

  return true;
}

void WrappedOpenGL::glCreateTransformFeedbacks(GLsizei n, GLuint *ids)
{
  SERIALISE_TIME_CALL(GL.glCreateTransformFeedbacks(n, ids));

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
      if(GetResourceManager()->HasResourceRecord(res))
      {
        GLResourceRecord *record = GetResourceManager()->GetResourceRecord(res);
        record->Delete(GetResourceManager());

        for(auto cd = m_ContextData.begin(); cd != m_ContextData.end(); ++cd)
        {
          if(cd->second.m_FeedbackRecord == record)
            cd->second.m_FeedbackRecord = NULL;
        }
      }
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteTransformFeedbacks(n, ids);
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
    GL.glTransformFeedbackBufferBase(xfb.name, index, buffer.name);

    AddResourceInitChunk(xfb);
  }

  return true;
}

void WrappedOpenGL::glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glTransformFeedbackBufferBase(xfb, index, buffer));

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTransformFeedbackBufferBase(ser, xfb, index, buffer);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
    }
    else if(xfb != 0)
    {
      GLResourceRecord *fbrecord =
          GetResourceManager()->GetResourceRecord(FeedbackRes(GetCtx(), xfb));

      fbrecord->AddChunk(scope.Get());

      if(buffer != 0)
        fbrecord->AddParent(GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer)));
    }

    GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                      eFrameRef_ReadBeforeWrite);
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
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    GL.glTransformFeedbackBufferRange(xfb.name, index, buffer.name, (GLintptr)offset, (GLsizei)size);

    AddResourceInitChunk(xfb);
  }

  return true;
}

void WrappedOpenGL::glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                                   GLintptr offset, GLsizeiptr size)
{
  if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffer),
                                                      eFrameRef_ReadBeforeWrite);
  }

  SERIALISE_TIME_CALL(GL.glTransformFeedbackBufferRange(xfb, index, buffer, offset, size));

  if(IsCaptureMode(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTransformFeedbackBufferRange(ser, xfb, index, buffer, offset, size);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
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
    GL.glBindTransformFeedback(target, xfb.name);
  }

  return true;
}

void WrappedOpenGL::glBindTransformFeedback(GLenum target, GLuint id)
{
  SERIALISE_TIME_CALL(GL.glBindTransformFeedback(target, id));

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

    GetContextRecord()->AddChunk(scope.Get());

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
    GL.glBeginTransformFeedback(primitiveMode);
    m_ActiveFeedback = true;
  }

  return true;
}

void WrappedOpenGL::glBeginTransformFeedback(GLenum primitiveMode)
{
  SERIALISE_TIME_CALL(GL.glBeginTransformFeedback(primitiveMode));
  m_ActiveFeedback = true;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBeginTransformFeedback(ser, primitiveMode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPauseTransformFeedback(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    GL.glPauseTransformFeedback();
  }

  return true;
}

void WrappedOpenGL::glPauseTransformFeedback()
{
  SERIALISE_TIME_CALL(GL.glPauseTransformFeedback());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPauseTransformFeedback(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glResumeTransformFeedback(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    GL.glResumeTransformFeedback();
  }

  return true;
}

void WrappedOpenGL::glResumeTransformFeedback()
{
  SERIALISE_TIME_CALL(GL.glResumeTransformFeedback());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glResumeTransformFeedback(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEndTransformFeedback(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    GL.glEndTransformFeedback();
    m_ActiveFeedback = false;
  }

  return true;
}

void WrappedOpenGL::glEndTransformFeedback()
{
  SERIALISE_TIME_CALL(GL.glEndTransformFeedback());
  m_ActiveFeedback = false;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEndTransformFeedback(ser);

    GetContextRecord()->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(size).OffsetOrSize();
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_TYPED(bool, normalized);
  SERIALISE_ELEMENT(stride).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    GLuint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    GL.glBindVertexArray(vaobj.name);

    // seems buggy when mixed and matched with new style vertex attrib binding, which we use for VAO
    // initial states. Since the spec defines how this function should work in terms of new style
    // bindings, just do that ourselves.

    // GL.glVertexArrayVertexAttribOffsetEXT(vaobj.name, buffer.name, index, size, type, normalized,
    //                                       stride, (GLintptr)offset);
    GL.glVertexArrayVertexAttribFormatEXT(vaobj.name, index, size, type, normalized, 0);
    GL.glVertexArrayVertexAttribBindingEXT(vaobj.name, index, index);
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
    GL.glVertexArrayBindVertexBufferEXT(vaobj.name, index, buffer.name, (GLintptr)offset, stride);

    GL.glBindVertexArray(prevVAO);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                       GLint size, GLenum type, GLboolean normalized,
                                                       GLsizei stride, GLintptr offset)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type,
                                                            normalized, stride, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribPointer(index, size, type, normalized, stride, pointer));

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
            index, size, type, normalized, stride,
            bufrecord ? (GLintptr)pointer : GLintptr(0xDEADBEEF));

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
  SERIALISE_ELEMENT(size).OffsetOrSize();
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(stride).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    GLuint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    GL.glBindVertexArray(vaobj.name);

    // seems buggy when mixed and matched with new style vertex attrib binding, which we use for VAO
    // initial states. Since the spec defines how this function should work in terms of new style
    // bindings, just do that ourselves.

    // GL.glVertexArrayVertexAttribIOffsetEXT(vaobj.name, buffer.name, index, size, type,
    //                                        stride, (GLintptr)offset);
    GL.glVertexArrayVertexAttribIFormatEXT(vaobj.name, index, size, type, 0);
    GL.glVertexArrayVertexAttribBindingEXT(vaobj.name, index, index);
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
    GL.glVertexArrayBindVertexBufferEXT(vaobj.name, index, buffer.name, (GLintptr)offset, stride);

    GL.glBindVertexArray(prevVAO);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                        GLint size, GLenum type, GLsizei stride,
                                                        GLintptr offset)
{
  SERIALISE_TIME_CALL(
      GL.glVertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribIPointer(index, size, type, stride, pointer));

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
        Serialise_glVertexArrayVertexAttribIOffsetEXT(
            ser, varecord ? varecord->Resource.name : 0, bufrecord ? bufrecord->Resource.name : 0,
            index, size, type, stride, bufrecord ? (GLintptr)pointer : GLintptr(0xDEADBEEF));

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
  SERIALISE_ELEMENT(size).OffsetOrSize();
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(stride).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    // some intel drivers don't properly update query states (like GL_VERTEX_ATTRIB_ARRAY_SIZE)
    // unless the VAO is also bound when performing EXT_dsa functions :(
    GLuint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    GL.glBindVertexArray(vaobj.name);

    // seems buggy when mixed and matched with new style vertex attrib binding, which we use for VAO
    // initial states. Since the spec defines how this function should work in terms of new style
    // bindings, just do that ourselves.

    // GL.glVertexArrayVertexAttribIOffsetEXT(vaobj.name, buffer.name, index, size, type,
    //                                        stride, (GLintptr)offset);
    GL.glVertexArrayVertexAttribLFormatEXT(vaobj.name, index, size, type, 0);
    GL.glVertexArrayVertexAttribBindingEXT(vaobj.name, index, index);
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
    GL.glVertexArrayBindVertexBufferEXT(vaobj.name, index, buffer.name, (GLintptr)offset, stride);

    GL.glBindVertexArray(prevVAO);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                        GLint size, GLenum type, GLsizei stride,
                                                        GLintptr offset)
{
  SERIALISE_TIME_CALL(
      GL.glVertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribLPointer(index, size, type, stride, pointer));

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();
    GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
    GLResourceRecord *varecord = cd.m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
        Serialise_glVertexArrayVertexAttribLOffsetEXT(
            ser, varecord ? varecord->Resource.name : 0, bufrecord ? bufrecord->Resource.name : 0,
            index, size, type, stride, bufrecord ? (GLintptr)pointer : GLintptr(0xDEADBEEF));

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
      vaobj.name = m_Global_VAO0;

    GL.glVertexArrayVertexAttribBindingEXT(vaobj.name, attribindex, bindingindex);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex,
                                                        GLuint bindingindex)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayVertexAttribBindingEXT(vaobj, attribindex, bindingindex));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribBinding(attribindex, bindingindex));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_ELEMENT(size).OffsetOrSize();
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT_TYPED(bool, normalized);
  SERIALISE_ELEMENT(relativeoffset).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    GL.glVertexArrayVertexAttribFormatEXT(vaobj.name, attribindex, size, type, normalized,
                                          relativeoffset);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                       GLenum type, GLboolean normalized,
                                                       GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type,
                                                            normalized, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_ELEMENT(size).OffsetOrSize();
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(relativeoffset).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    GL.glVertexArrayVertexAttribIFormatEXT(vaobj.name, attribindex, size, type, relativeoffset);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                        GLenum type, GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(
      GL.glVertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribIFormat(attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_ELEMENT(size).OffsetOrSize();
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(relativeoffset).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    GL.glVertexArrayVertexAttribLFormatEXT(vaobj.name, attribindex, size, type, relativeoffset);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                        GLenum type, GLuint relativeoffset)
{
  SERIALISE_TIME_CALL(
      GL.glVertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribLFormat(attribindex, size, type, relativeoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
      vaobj.name = m_Global_VAO0;

    // at the time of writing, AMD driver seems to not have this entry point
    if(GL.glVertexArrayVertexAttribDivisorEXT)
    {
      GL.glVertexArrayVertexAttribDivisorEXT(vaobj.name, index, divisor);
    }
    else
    {
      GLuint VAO = 0;
      GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);
      GL.glBindVertexArray(vaobj.name);
      GL.glVertexAttribDivisor(index, divisor);
      GL.glBindVertexArray(VAO);
    }

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayVertexAttribDivisorEXT(vaobj, index, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexAttribDivisor(index, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
      vaobj.name = m_Global_VAO0;

    GLint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, &prevVAO);

    GL.glEnableVertexArrayAttribEXT(vaobj.name, index);

    // nvidia bug seems to sometimes change VAO binding in glEnableVertexArrayAttribEXT, although it
    // seems like it only happens if GL_DEBUG_OUTPUT_SYNCHRONOUS is NOT enabled.
    GL.glBindVertexArray(prevVAO);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  SERIALISE_TIME_CALL(GL.glEnableVertexArrayAttribEXT(vaobj, index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glEnableVertexAttribArray(index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
      vaobj.name = m_Global_VAO0;

    GLint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, &prevVAO);

    GL.glDisableVertexArrayAttribEXT(vaobj.name, index);

    // nvidia bug seems to sometimes change VAO binding in glEnableVertexArrayAttribEXT, although it
    // seems like it only happens if GL_DEBUG_OUTPUT_SYNCHRONOUS is NOT enabled.
    GL.glBindVertexArray(prevVAO);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  SERIALISE_TIME_CALL(GL.glDisableVertexArrayAttribEXT(vaobj, index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glDisableVertexAttribArray(index));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_ELEMENT_LOCAL(array, GetResourceManager()->GetResID(VertexArrayRes(GetCtx(), *arrays)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenVertexArrays(1, &real);
    GL.glBindVertexArray(real);
    GL.glBindVertexArray(0);

    GLResource res = VertexArrayRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(array, res);

    AddResource(array, ResourceType::StateObject, "Vertex Array");
  }

  return true;
}

void WrappedOpenGL::glGenVertexArrays(GLsizei n, GLuint *arrays)
{
  SERIALISE_TIME_CALL(GL.glGenVertexArrays(n, arrays));

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
  SERIALISE_ELEMENT_LOCAL(array, GetResourceManager()->GetResID(VertexArrayRes(GetCtx(), *arrays)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateVertexArrays(1, &real);

    GLResource res = VertexArrayRes(GetCtx(), real);

    m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(array, res);

    AddResource(array, ResourceType::StateObject, "Vertex Array");
  }

  return true;
}

void WrappedOpenGL::glCreateVertexArrays(GLsizei n, GLuint *arrays)
{
  SERIALISE_TIME_CALL(GL.glCreateVertexArrays(n, arrays));

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
      vaobj.name = m_Global_VAO0;

    GL.glBindVertexArray(vaobj.name);
  }

  return true;
}

void WrappedOpenGL::glBindVertexArray(GLuint array)
{
  SERIALISE_TIME_CALL(GL.glBindVertexArray(array));

  GLResourceRecord *record = NULL;

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();

    if(array == 0)
    {
      cd.m_VertexArrayRecord = record = NULL;

      cd.m_BufferRecord[BufferIdx(eGL_ELEMENT_ARRAY_BUFFER)] = NULL;
    }
    else
    {
      cd.m_VertexArrayRecord = record =
          GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), array));

      GLuint buffer = 0;
      GL.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&buffer);

      cd.m_BufferRecord[BufferIdx(eGL_ELEMENT_ARRAY_BUFFER)] =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    }
  }

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindVertexArray(ser, array);

    GetContextRecord()->AddChunk(scope.Get());
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
      vaobj.name = m_Global_VAO0;

    // might not have the live resource if this is a pre-capture chunk, and the buffer was never
    // referenced at all in the actual frame
    if(buffer.name)
    {
      m_Buffers[GetResourceManager()->GetResID(buffer)].curType = eGL_ELEMENT_ARRAY_BUFFER;
      m_Buffers[GetResourceManager()->GetResID(buffer)].creationFlags |= BufferCategory::Index;
    }

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    GL.glVertexArrayElementBuffer(vaobj.name, buffer.name);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayElementBuffer(vaobj, buffer));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    ContextData &cd = GetCtxData();

    if(cd.m_VertexArrayRecord == varecord)
    {
      cd.m_BufferRecord[BufferIdx(eGL_ELEMENT_ARRAY_BUFFER)] = bufrecord;
    }

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle)).Important();
  SERIALISE_ELEMENT(bindingindex).Important();
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(offset, (uint64_t)offsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT(stride).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(vaobj.name == 0)
      vaobj.name = m_Global_VAO0;

    if(buffer.name)
    {
      m_Buffers[GetResourceManager()->GetResID(buffer)].curType = eGL_ARRAY_BUFFER;
      m_Buffers[GetResourceManager()->GetResID(buffer)].creationFlags |= BufferCategory::Vertex;
    }

    GL.glVertexArrayBindVertexBufferEXT(vaobj.name, bindingindex, buffer.name, (GLintptr)offset,
                                        (GLsizei)stride);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex,
                                                     GLuint buffer, GLintptr offset, GLsizei stride)
{
  SERIALISE_TIME_CALL(
      GL.glVertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glBindVertexBuffer(bindingindex, buffer, offset, stride));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

    if(r)
    {
      if(IsBackgroundCapturing(m_State) && !RecordUpdateCheck(varecord))
        return;

      GLResourceRecord *bufrecord =
          GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

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
  rdcarray<GLResource> buffers;
  rdcarray<uint64_t> offsets;

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

  SERIALISE_ELEMENT_LOCAL(vaobj, VertexArrayRes(GetCtx(), vaobjHandle)).Important();
  SERIALISE_ELEMENT(first).Important();
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).OffsetOrSize();
  SERIALISE_ELEMENT_ARRAY(strides, count).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<GLuint> bufs;
    rdcarray<GLintptr> offs;
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
      vaobj.name = m_Global_VAO0;

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    GL.glVertexArrayVertexBuffers(vaobj.name, first, count, bufs.empty() ? NULL : bufs.data(),
                                  offs.empty() ? NULL : offs.data(), strides);

    if(IsLoading(m_State))
    {
      for(size_t i = 0; i < buffers.size(); i++)
      {
        m_Buffers[GetResourceManager()->GetResID(buffers[i])].curType = eGL_ARRAY_BUFFER;
        m_Buffers[GetResourceManager()->GetResID(buffers[i])].creationFlags |= BufferCategory::Vertex;
      }
    }

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                                               const GLuint *buffers, const GLintptr *offsets,
                                               const GLsizei *strides)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayVertexBuffers(vaobj, first, count, buffers, offsets, strides));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glBindVertexBuffers(first, count, buffers, offsets, strides));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
      vaobj.name = m_Global_VAO0;

    GL.glVertexArrayVertexBindingDivisorEXT(vaobj.name, bindingindex, divisor);

    AddResourceInitChunk(vaobj);
  }

  return true;
}

void WrappedOpenGL::glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex,
                                                         GLuint divisor)
{
  SERIALISE_TIME_CALL(GL.glVertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord =
        GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
  SERIALISE_TIME_CALL(GL.glVertexBindingDivisor(bindingindex, divisor));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

    GLResourceRecord *r = IsActiveCapturing(m_State) ? GetContextRecord() : varecord;

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
        {
          m_PersistentMaps.erase(record);
          if(record->Map.access & GL_MAP_COHERENT_BIT)
            m_CoherentMaps.erase(record);
        }

        // free any shadow storage
        record->FreeShadowStorage();

        for(auto cd = m_ContextData.begin(); cd != m_ContextData.end(); ++cd)
        {
          for(size_t r = 0; r < ARRAY_COUNT(cd->second.m_BufferRecord); r++)
          {
            if(cd->second.m_BufferRecord[r] == record)
              cd->second.m_BufferRecord[r] = NULL;
          }
        }
      }

      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteBuffers(n, buffers);
}

void WrappedOpenGL::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
    if(GetResourceManager()->HasCurrentResource(res) && arrays[i])
    {
      if(GetResourceManager()->HasResourceRecord(res))
      {
        GLResourceRecord *record = GetResourceManager()->GetResourceRecord(res);
        record->Delete(GetResourceManager());

        for(auto cd = m_ContextData.begin(); cd != m_ContextData.end(); ++cd)
        {
          if(cd->second.m_VertexArrayRecord == record)
            cd->second.m_VertexArrayRecord = NULL;
        }
      }
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteVertexArrays(n, arrays);
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
    uint32_t byteCount = 0u;

    switch(attr)
    {
      case Attrib_GLdouble: byteCount = sizeof(double) * count; break;
      case Attrib_GLfloat: byteCount = sizeof(float) * count; break;
      case Attrib_GLshort:
      case Attrib_GLushort: byteCount = sizeof(int16_t) * count; break;
      default:
      case Attrib_GLbyte:
      case Attrib_GLubyte: byteCount = sizeof(int8_t) * count; break;
      case Attrib_GLint:
      case Attrib_GLuint: byteCount = sizeof(int32_t) * count; break;
      case Attrib_packed: byteCount = sizeof(uint32_t); break;
    }

    RDCEraseEl(v);

    memcpy(&v, value, byteCount);
  }

  // Serialise the array with the right type. We don't want to allocate new storage
  switch(attr)
  {
    case Attrib_GLdouble: ser.Serialise("values"_lit, v.d, SerialiserFlags::NoFlags); break;
    case Attrib_GLfloat: ser.Serialise("values"_lit, v.f, SerialiserFlags::NoFlags); break;
    case Attrib_GLint: ser.Serialise("values"_lit, v.i32, SerialiserFlags::NoFlags); break;
    case Attrib_packed:
    case Attrib_GLuint: ser.Serialise("values"_lit, v.u32, SerialiserFlags::NoFlags); break;
    case Attrib_GLshort: ser.Serialise("values"_lit, v.i16, SerialiserFlags::NoFlags); break;
    case Attrib_GLushort: ser.Serialise("values"_lit, v.u16, SerialiserFlags::NoFlags); break;
    case Attrib_GLbyte: ser.Serialise("values"_lit, v.i8, SerialiserFlags::NoFlags); break;
    default:
    case Attrib_GLubyte: ser.Serialise("values"_lit, v.u8, SerialiserFlags::NoFlags); break;
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(attr == Attrib_packed)
    {
      if(count == 1)
        GL.glVertexAttribP1uiv(index, type, normalized, v.u32);
      else if(count == 2)
        GL.glVertexAttribP2uiv(index, type, normalized, v.u32);
      else if(count == 3)
        GL.glVertexAttribP3uiv(index, type, normalized, v.u32);
      else if(count == 4)
        GL.glVertexAttribP4uiv(index, type, normalized, v.u32);
    }
    else if(attribtype & Attrib_I)
    {
      if(count == 1)
      {
        if(attr == Attrib_GLint)
          GL.glVertexAttribI1iv(index, v.i32);
        else if(attr == Attrib_GLuint)
          GL.glVertexAttribI1uiv(index, v.u32);
      }
      else if(count == 2)
      {
        if(attr == Attrib_GLint)
          GL.glVertexAttribI2iv(index, v.i32);
        else if(attr == Attrib_GLuint)
          GL.glVertexAttribI2uiv(index, v.u32);
      }
      else if(count == 3)
      {
        if(attr == Attrib_GLint)
          GL.glVertexAttribI3iv(index, v.i32);
        else if(attr == Attrib_GLuint)
          GL.glVertexAttribI3uiv(index, v.u32);
      }
      else
      {
        if(attr == Attrib_GLbyte)
          GL.glVertexAttribI4bv(index, v.i8);
        else if(attr == Attrib_GLshort)
          GL.glVertexAttribI4sv(index, v.i16);
        else if(attr == Attrib_GLint)
          GL.glVertexAttribI4iv(index, v.i32);
        else if(attr == Attrib_GLubyte)
          GL.glVertexAttribI4ubv(index, v.u8);
        else if(attr == Attrib_GLushort)
          GL.glVertexAttribI4usv(index, v.u16);
        else if(attr == Attrib_GLuint)
          GL.glVertexAttribI4uiv(index, v.u32);
      }
    }
    else if(attribtype & Attrib_L)
    {
      if(count == 1)
        GL.glVertexAttribL1dv(index, v.d);
      else if(count == 2)
        GL.glVertexAttribL2dv(index, v.d);
      else if(count == 3)
        GL.glVertexAttribL3dv(index, v.d);
      else if(count == 4)
        GL.glVertexAttribL4dv(index, v.d);
    }
    else if(attribtype & Attrib_N)
    {
      if(attr == Attrib_GLbyte)
        GL.glVertexAttrib4Nbv(index, v.i8);
      else if(attr == Attrib_GLshort)
        GL.glVertexAttrib4Nsv(index, v.i16);
      else if(attr == Attrib_GLint)
        GL.glVertexAttrib4Niv(index, v.i32);
      else if(attr == Attrib_GLubyte)
        GL.glVertexAttrib4Nubv(index, v.u8);
      else if(attr == Attrib_GLushort)
        GL.glVertexAttrib4Nusv(index, v.u16);
      else if(attr == Attrib_GLuint)
        GL.glVertexAttrib4Nuiv(index, v.u32);
    }
    else
    {
      if(count == 1)
      {
        if(attr == Attrib_GLdouble)
          GL.glVertexAttrib1dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          GL.glVertexAttrib1fv(index, v.f);
        else if(attr == Attrib_GLshort)
          GL.glVertexAttrib1sv(index, v.i16);
      }
      else if(count == 2)
      {
        if(attr == Attrib_GLdouble)
          GL.glVertexAttrib2dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          GL.glVertexAttrib2fv(index, v.f);
        else if(attr == Attrib_GLshort)
          GL.glVertexAttrib2sv(index, v.i16);
      }
      else if(count == 3)
      {
        if(attr == Attrib_GLdouble)
          GL.glVertexAttrib3dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          GL.glVertexAttrib3fv(index, v.f);
        else if(attr == Attrib_GLshort)
          GL.glVertexAttrib3sv(index, v.i16);
      }
      else
      {
        if(attr == Attrib_GLdouble)
          GL.glVertexAttrib4dv(index, v.d);
        else if(attr == Attrib_GLfloat)
          GL.glVertexAttrib4fv(index, v.f);
        else if(attr == Attrib_GLbyte)
          GL.glVertexAttrib4bv(index, v.i8);
        else if(attr == Attrib_GLshort)
          GL.glVertexAttrib4sv(index, v.i16);
        else if(attr == Attrib_GLint)
          GL.glVertexAttrib4iv(index, v.i32);
        else if(attr == Attrib_GLubyte)
          GL.glVertexAttrib4ubv(index, v.u8);
        else if(attr == Attrib_GLushort)
          GL.glVertexAttrib4usv(index, v.u16);
        else if(attr == Attrib_GLuint)
          GL.glVertexAttrib4uiv(index, v.u32);
      }
    }
  }

  return true;
}

#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype, ...)                       \
                                                                                 \
  void WrappedOpenGL::CONCAT(glVertexAttrib, suffix)(GLuint index, __VA_ARGS__)  \
                                                                                 \
  {                                                                              \
    SERIALISE_TIME_CALL(GL.CONCAT(glVertexAttrib, suffix)(index, ARRAYLIST));    \
                                                                                 \
    if(IsActiveCapturing(m_State))                                               \
    {                                                                            \
      USE_SCRATCH_SERIALISER();                                                  \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                       \
      const paramtype vals[] = {ARRAYLIST};                                      \
      Serialise_glVertexAttrib(ser, index, count, eGL_NONE, GL_FALSE, vals,      \
                               AttribType(TypeOr | CONCAT(Attrib_, paramtype))); \
                                                                                 \
      GetContextRecord()->AddChunk(scope.Get());                                 \
    }                                                                            \
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
    GL.CONCAT(glVertexAttrib, suffix)(index, value);                                       \
                                                                                           \
    if(IsActiveCapturing(m_State))                                                         \
    {                                                                                      \
      USE_SCRATCH_SERIALISER();                                                            \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                                 \
      Serialise_glVertexAttrib(ser, index, count, eGL_NONE, GL_FALSE, value,               \
                               AttribType(TypeOr | CONCAT(Attrib_, paramtype)));           \
                                                                                           \
      GetContextRecord()->AddChunk(scope.Get());                                           \
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
    GL.CONCAT(CONCAT(glVertexAttribP, count), suffix)(index, type, normalized, value);         \
                                                                                               \
    if(IsActiveCapturing(m_State))                                                             \
    {                                                                                          \
      USE_SCRATCH_SERIALISER();                                                                \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                                     \
      Serialise_glVertexAttrib(ser, index, count, type, normalized, passparam, Attrib_packed); \
                                                                                               \
      GetContextRecord()->AddChunk(scope.Get());                                               \
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
INSTANTIATE_FUNCTION_SERIALISED(void, glInvalidateBufferData, GLuint buffer);
INSTANTIATE_FUNCTION_SERIALISED(void, glInvalidateBufferSubData, GLuint buffer, GLintptr offset,
                                GLsizeiptr length);
