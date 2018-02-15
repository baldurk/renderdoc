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
#include "common/common.h"
#include "strings/string_utils.h"

enum GLsyncbitfield
{
};

DECLARE_REFLECTION_ENUM(GLsyncbitfield);

template <>
std::string DoStringise(const GLsyncbitfield &el)
{
  RDCCOMPILE_ASSERT(
      sizeof(GLsyncbitfield) == sizeof(GLbitfield) && sizeof(GLsyncbitfield) == sizeof(uint32_t),
      "Fake bitfield enum must be uint32_t sized");

  BEGIN_BITFIELD_STRINGISE(GLsyncbitfield);
  {
    STRINGISE_BITFIELD_BIT(GL_SYNC_FLUSH_COMMANDS_BIT);
  }
  END_BITFIELD_STRINGISE();
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFenceSync(SerialiserType &ser, GLsync real, GLenum condition,
                                          GLbitfield flags)
{
  SERIALISE_ELEMENT_LOCAL(sync, GetResourceManager()->GetSyncID(real)).TypedAs("GLsync");
  SERIALISE_ELEMENT(condition);
  SERIALISE_ELEMENT_TYPED(GLsyncbitfield, flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // if we've already sync'd, delete the old one
    if(GetResourceManager()->HasLiveResource(sync))
    {
      GLResource res = GetResourceManager()->GetLiveResource(sync);
      GLsync oldSyncObj = GetResourceManager()->GetSync(res.name);

      m_Real.glDeleteSync(oldSyncObj);

      GetResourceManager()->UnregisterResource(res);
      GetResourceManager()->EraseLiveResource(sync);
    }

    real = m_Real.glFenceSync(condition, flags);

    GLuint name = 0;
    ResourceId liveid = ResourceId();
    GetResourceManager()->RegisterSync(GetCtx(), real, name, liveid);

    GLResource res = SyncRes(GetCtx(), name);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(sync, res);

    AddResource(sync, ResourceType::Sync, "Sync");
  }

  return true;
}

GLsync WrappedOpenGL::glFenceSync(GLenum condition, GLbitfield flags)
{
  GLsync sync;
  SERIALISE_TIME_CALL(sync = m_Real.glFenceSync(condition, flags));

  GLuint name = 0;
  ResourceId id = ResourceId();
  GetResourceManager()->RegisterSync(GetCtx(), sync, name, id);
  GLResource res = SyncRes(GetCtx(), name);

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glFenceSync(ser, sync, condition, flags);

      chunk = scope.Get();
    }

    m_ContextRecord->AddChunk(chunk);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, res);
  }

  return sync;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClientWaitSync(SerialiserType &ser, GLsync sync_, GLbitfield flags,
                                               GLuint64 timeout)
{
  SERIALISE_ELEMENT_LOCAL(sync, GetResourceManager()->GetSyncID(sync_)).TypedAs("GLsync");
  SERIALISE_ELEMENT_TYPED(GLsyncbitfield, flags);
  SERIALISE_ELEMENT(timeout);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && GetResourceManager()->HasLiveResource(sync))
  {
    GLResource res = GetResourceManager()->GetLiveResource(sync);
    m_Real.glClientWaitSync(GetResourceManager()->GetSync(res.name), flags, timeout);
  }

  return true;
}

GLenum WrappedOpenGL::glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  GLenum ret;
  SERIALISE_TIME_CALL(ret = m_Real.glClientWaitSync(sync, flags, timeout));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClientWaitSync(ser, sync, flags, timeout);

    m_ContextRecord->AddChunk(scope.Get());
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glWaitSync(SerialiserType &ser, GLsync sync_, GLbitfield flags,
                                         GLuint64 timeout)
{
  SERIALISE_ELEMENT_LOCAL(sync, GetResourceManager()->GetSyncID(sync_)).TypedAs("GLsync");
  SERIALISE_ELEMENT_TYPED(GLsyncbitfield, flags);
  SERIALISE_ELEMENT(timeout);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && GetResourceManager()->HasLiveResource(sync))
  {
    GLResource res = GetResourceManager()->GetLiveResource(sync);
    m_Real.glWaitSync(GetResourceManager()->GetSync(res.name), flags, timeout);
  }

  return true;
}

void WrappedOpenGL::glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  SERIALISE_TIME_CALL(m_Real.glWaitSync(sync, flags, timeout));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glWaitSync(ser, sync, flags, timeout);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDeleteSync(GLsync sync)
{
  m_Real.glDeleteSync(sync);

  ResourceId id = GetResourceManager()->GetSyncID(sync);

  if(GetResourceManager()->HasCurrentResource(id))
    GetResourceManager()->UnregisterResource(GetResourceManager()->GetCurrentResource(id));
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenQueries(SerialiserType &ser, GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(query, GetResourceManager()->GetID(QueryRes(GetCtx(), *ids)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glGenQueries(1, &real);

    GLResource res = QueryRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(query, res);

    AddResource(query, ResourceType::Query, "Query");
  }

  return true;
}

void WrappedOpenGL::glGenQueries(GLsizei count, GLuint *ids)
{
  SERIALISE_TIME_CALL(m_Real.glGenQueries(count, ids));

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = QueryRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenQueries(ser, 1, ids + i);

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
bool WrappedOpenGL::Serialise_glCreateQueries(SerialiserType &ser, GLenum target, GLsizei n,
                                              GLuint *ids)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(query, GetResourceManager()->GetID(SamplerRes(GetCtx(), *ids)))
      .TypedAs("GLResource");

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glCreateQueries(target, 1, &real);

    GLResource res = QueryRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(query, res);

    AddResource(query, ResourceType::Query, "Query");
  }

  return true;
}

void WrappedOpenGL::glCreateQueries(GLenum target, GLsizei count, GLuint *ids)
{
  SERIALISE_TIME_CALL(m_Real.glCreateQueries(target, count, ids));

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = QueryRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateQueries(ser, target, 1, ids + i);

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
bool WrappedOpenGL::Serialise_glBeginQuery(SerialiserType &ser, GLenum target, GLuint qid)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(query, QueryRes(GetCtx(), qid));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Queries in the log interfere with the queries from FetchCounters.
    if(!m_FetchCounters)
    {
      m_Real.glBeginQuery(target, query.name);
      m_ActiveQueries[QueryIdx(target)][0] = true;
    }
  }

  return true;
}

void WrappedOpenGL::glBeginQuery(GLenum target, GLuint id)
{
  SERIALISE_TIME_CALL(m_Real.glBeginQuery(target, id));
  if(m_ActiveQueries[QueryIdx(target)][0])
    RDCLOG("Query already active %s", ToStr(target).c_str());
  m_ActiveQueries[QueryIdx(target)][0] = true;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBeginQuery(ser, target, id);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBeginQueryIndexed(SerialiserType &ser, GLenum target, GLuint index,
                                                  GLuint qid)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT_LOCAL(query, QueryRes(GetCtx(), qid));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glBeginQueryIndexed(target, index, query.name);
    m_ActiveQueries[QueryIdx(target)][index] = true;
  }

  return true;
}

void WrappedOpenGL::glBeginQueryIndexed(GLenum target, GLuint index, GLuint id)
{
  SERIALISE_TIME_CALL(m_Real.glBeginQueryIndexed(target, index, id));
  m_ActiveQueries[QueryIdx(target)][index] = true;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBeginQueryIndexed(ser, target, index, id);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEndQuery(SerialiserType &ser, GLenum target)
{
  SERIALISE_ELEMENT(target);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Queries in the log interfere with the queries from FetchCounters.
    if(!m_FetchCounters)
    {
      m_ActiveQueries[QueryIdx(target)][0] = false;
      m_Real.glEndQuery(target);
    }
  }

  return true;
}

void WrappedOpenGL::glEndQuery(GLenum target)
{
  SERIALISE_TIME_CALL(m_Real.glEndQuery(target));
  m_ActiveQueries[QueryIdx(target)][0] = false;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEndQuery(ser, target);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEndQueryIndexed(SerialiserType &ser, GLenum target, GLuint index)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Real.glEndQueryIndexed(target, index);
    m_ActiveQueries[QueryIdx(target)][index] = false;
  }

  return true;
}

void WrappedOpenGL::glEndQueryIndexed(GLenum target, GLuint index)
{
  SERIALISE_TIME_CALL(m_Real.glEndQueryIndexed(target, index));
  m_ActiveQueries[QueryIdx(target)][index] = false;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEndQueryIndexed(ser, target, index);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBeginConditionalRender(SerialiserType &ser, GLuint id, GLenum mode)
{
  SERIALISE_ELEMENT_LOCAL(query, QueryRes(GetCtx(), id));
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_ActiveConditional = true;
    m_Real.glBeginConditionalRender(query.name, mode);
  }

  return true;
}

void WrappedOpenGL::glBeginConditionalRender(GLuint id, GLenum mode)
{
  SERIALISE_TIME_CALL(m_Real.glBeginConditionalRender(id, mode));

  m_ActiveConditional = true;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBeginConditionalRender(ser, id, mode);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEndConditionalRender(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    m_ActiveConditional = false;
    m_Real.glEndConditionalRender();
  }

  return true;
}

void WrappedOpenGL::glEndConditionalRender()
{
  SERIALISE_TIME_CALL(m_Real.glEndConditionalRender());
  m_ActiveConditional = false;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEndConditionalRender(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glQueryCounter(SerialiserType &ser, GLuint query_, GLenum target)
{
  SERIALISE_ELEMENT_LOCAL(query, QueryRes(GetCtx(), query_));
  SERIALISE_ELEMENT(target);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    m_Real.glQueryCounter(query.name, target);

  return true;
}

void WrappedOpenGL::glQueryCounter(GLuint query, GLenum target)
{
  SERIALISE_TIME_CALL(m_Real.glQueryCounter(query, target));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glQueryCounter(ser, query, target);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), query), eFrameRef_Read);
  }
}

void WrappedOpenGL::glDeleteQueries(GLsizei n, const GLuint *ids)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = QueryRes(GetCtx(), ids[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteQueries(n, ids);
}

INSTANTIATE_FUNCTION_SERIALISED(void, glFenceSync, GLsync real, GLenum condition, GLbitfield flags);
INSTANTIATE_FUNCTION_SERIALISED(void, glClientWaitSync, GLsync sync_, GLbitfield flags,
                                GLuint64 timeout);
INSTANTIATE_FUNCTION_SERIALISED(void, glWaitSync, GLsync sync_, GLbitfield flags, GLuint64 timeout);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenQueries, GLsizei n, GLuint *ids);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateQueries, GLenum target, GLsizei n, GLuint *ids);
INSTANTIATE_FUNCTION_SERIALISED(void, glBeginQuery, GLenum target, GLuint qid);
INSTANTIATE_FUNCTION_SERIALISED(void, glBeginQueryIndexed, GLenum target, GLuint index, GLuint qid);
INSTANTIATE_FUNCTION_SERIALISED(void, glEndQuery, GLenum target);
INSTANTIATE_FUNCTION_SERIALISED(void, glEndQueryIndexed, GLenum target, GLuint index);
INSTANTIATE_FUNCTION_SERIALISED(void, glBeginConditionalRender, GLuint id, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glEndConditionalRender);
INSTANTIATE_FUNCTION_SERIALISED(void, glQueryCounter, GLuint query_, GLenum target);
