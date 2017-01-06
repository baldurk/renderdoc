/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "serialise/string_utils.h"

bool WrappedOpenGL::Serialise_glFenceSync(GLsync real, GLenum condition, GLbitfield flags)
{
  SERIALISE_ELEMENT(GLenum, Condition, condition);
  SERIALISE_ELEMENT(uint32_t, Flags, flags);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetSyncID(real));

  if(m_State < WRITING)
  {
    real = m_Real.glFenceSync(Condition, Flags);

    GLuint name = 0;
    ResourceId liveid = ResourceId();
    GetResourceManager()->RegisterSync(GetCtx(), real, name, liveid);

    GLResource res = SyncRes(GetCtx(), name);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

GLsync WrappedOpenGL::glFenceSync(GLenum condition, GLbitfield flags)
{
  GLsync sync = m_Real.glFenceSync(condition, flags);

  GLuint name = 0;
  ResourceId id = ResourceId();
  GetResourceManager()->RegisterSync(GetCtx(), sync, name, id);
  GLResource res = SyncRes(GetCtx(), name);

  if(m_State == WRITING_CAPFRAME)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(FENCE_SYNC);
      Serialise_glFenceSync(sync, condition, flags);

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

bool WrappedOpenGL::Serialise_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  SERIALISE_ELEMENT(uint32_t, Flags, flags);
  SERIALISE_ELEMENT(uint64_t, Timeout, timeout);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetSyncID(sync));

  if(m_State < WRITING)
  {
    if(GetResourceManager()->HasLiveResource(id))
    {
      GLResource res = GetResourceManager()->GetLiveResource(id);
      m_Real.glClientWaitSync(GetResourceManager()->GetSync(res.name), Flags, Timeout);
    }
  }

  return true;
}

GLenum WrappedOpenGL::glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  GLenum ret = m_Real.glClientWaitSync(sync, flags, timeout);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLIENTWAIT_SYNC);
    Serialise_glClientWaitSync(sync, flags, timeout);

    m_ContextRecord->AddChunk(scope.Get());
  }

  return ret;
}

bool WrappedOpenGL::Serialise_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  SERIALISE_ELEMENT(uint32_t, Flags, flags);
  SERIALISE_ELEMENT(uint64_t, Timeout, timeout);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetSyncID(sync));

  if(m_State < WRITING)
  {
    if(GetResourceManager()->HasLiveResource(id))
    {
      GLResource res = GetResourceManager()->GetLiveResource(id);
      m_Real.glWaitSync(GetResourceManager()->GetSync(res.name), Flags, Timeout);
    }
  }

  return true;
}

void WrappedOpenGL::glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  m_Real.glWaitSync(sync, flags, timeout);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(WAIT_SYNC);
    Serialise_glWaitSync(sync, flags, timeout);

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

bool WrappedOpenGL::Serialise_glGenQueries(GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), *ids)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenQueries(1, &real);

    GLResource res = QueryRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glGenQueries(GLsizei count, GLuint *ids)
{
  m_Real.glGenQueries(count, ids);

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = QueryRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_QUERIES);
        Serialise_glGenQueries(1, ids + i);

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

bool WrappedOpenGL::Serialise_glCreateQueries(GLenum target, GLsizei n, GLuint *ids)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), *ids)));
  SERIALISE_ELEMENT(GLenum, Target, target);

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glCreateQueries(Target, 1, &real);

    GLResource res = QueryRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glCreateQueries(GLenum target, GLsizei count, GLuint *ids)
{
  m_Real.glCreateQueries(target, count, ids);

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = QueryRes(GetCtx(), ids[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_QUERIES);
        Serialise_glCreateQueries(target, 1, ids + i);

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

bool WrappedOpenGL::Serialise_glBeginQuery(GLenum target, GLuint qid)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), qid)));

  if(m_State < WRITING)
  {
    // Queries in the log interfere with the queries from FetchCounters.
    if(!m_FetchCounters)
    {
      m_Real.glBeginQuery(Target, GetResourceManager()->GetLiveResource(id).name);
      m_ActiveQueries[QueryIdx(Target)][0] = true;
    }
  }

  return true;
}

void WrappedOpenGL::glBeginQuery(GLenum target, GLuint id)
{
  m_Real.glBeginQuery(target, id);
  if(m_ActiveQueries[QueryIdx(target)][0])
    RDCLOG("Query already active %s", ToStr::Get(target).c_str());
  m_ActiveQueries[QueryIdx(target)][0] = true;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_QUERY);
    Serialise_glBeginQuery(target, id);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

bool WrappedOpenGL::Serialise_glBeginQueryIndexed(GLenum target, GLuint index, GLuint qid)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Index, index);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), qid)));

  if(m_State < WRITING)
  {
    m_Real.glBeginQueryIndexed(Target, Index, GetResourceManager()->GetLiveResource(id).name);
    m_ActiveQueries[QueryIdx(Target)][Index] = true;
  }

  return true;
}

void WrappedOpenGL::glBeginQueryIndexed(GLenum target, GLuint index, GLuint id)
{
  m_Real.glBeginQueryIndexed(target, index, id);
  m_ActiveQueries[QueryIdx(target)][index] = true;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_QUERY_INDEXED);
    Serialise_glBeginQueryIndexed(target, index, id);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

bool WrappedOpenGL::Serialise_glEndQuery(GLenum target)
{
  SERIALISE_ELEMENT(GLenum, Target, target);

  if(m_State < WRITING)
  {
    // Queries in the log interfere with the queries from FetchCounters.
    if(!m_FetchCounters)
    {
      m_ActiveQueries[QueryIdx(Target)][0] = false;
      m_Real.glEndQuery(Target);
    }
  }

  return true;
}

void WrappedOpenGL::glEndQuery(GLenum target)
{
  m_Real.glEndQuery(target);
  m_ActiveQueries[QueryIdx(target)][0] = false;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_QUERY);
    Serialise_glEndQuery(target);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glEndQueryIndexed(GLenum target, GLuint index)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Index, index);

  if(m_State < WRITING)
  {
    m_Real.glEndQueryIndexed(Target, Index);
    m_ActiveQueries[QueryIdx(Target)][Index] = false;
  }

  return true;
}

void WrappedOpenGL::glEndQueryIndexed(GLenum target, GLuint index)
{
  m_Real.glEndQueryIndexed(target, index);
  m_ActiveQueries[QueryIdx(target)][index] = false;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_QUERY_INDEXED);
    Serialise_glEndQueryIndexed(target, index);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBeginConditionalRender(GLuint id, GLenum mode)
{
  SERIALISE_ELEMENT(ResourceId, qid, GetResourceManager()->GetID(QueryRes(GetCtx(), id)));
  SERIALISE_ELEMENT(GLenum, Mode, mode);

  if(m_State < WRITING)
  {
    m_ActiveConditional = true;
    m_Real.glBeginConditionalRender(GetResourceManager()->GetLiveResource(qid).name, Mode);
  }

  return true;
}

void WrappedOpenGL::glBeginConditionalRender(GLuint id, GLenum mode)
{
  m_Real.glBeginConditionalRender(id, mode);

  m_ActiveConditional = true;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_CONDITIONAL);
    Serialise_glBeginConditionalRender(id, mode);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

bool WrappedOpenGL::Serialise_glEndConditionalRender()
{
  if(m_State < WRITING)
  {
    m_ActiveConditional = false;
    m_Real.glEndConditionalRender();
  }

  return true;
}

void WrappedOpenGL::glEndConditionalRender()
{
  m_Real.glEndConditionalRender();
  m_ActiveConditional = false;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_CONDITIONAL);
    Serialise_glEndConditionalRender();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glQueryCounter(GLuint query, GLenum target)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), query)));
  SERIALISE_ELEMENT(GLenum, Target, target);

  if(m_State < WRITING)
  {
    m_Real.glQueryCounter(GetResourceManager()->GetLiveResource(id).name, Target);
  }

  return true;
}

void WrappedOpenGL::glQueryCounter(GLuint query, GLenum target)
{
  m_Real.glQueryCounter(query, target);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(QUERY_COUNTER);
    Serialise_glQueryCounter(query, target);

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
