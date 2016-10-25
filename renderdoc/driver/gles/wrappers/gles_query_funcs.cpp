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

bool WrappedGLES::Serialise_glFenceSync(GLsync real, GLenum condition, GLbitfield flags)
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

GLsync WrappedGLES::glFenceSync(GLenum condition, GLbitfield flags)
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

bool WrappedGLES::Serialise_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
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

GLenum WrappedGLES::glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
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

bool WrappedGLES::Serialise_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
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

void WrappedGLES::glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
  m_Real.glWaitSync(sync, flags, timeout);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(WAIT_SYNC);
    Serialise_glWaitSync(sync, flags, timeout);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedGLES::glDeleteSync(GLsync sync)
{
  m_Real.glDeleteSync(sync);

  ResourceId id = GetResourceManager()->GetSyncID(sync);

  if(GetResourceManager()->HasCurrentResource(id))
    GetResourceManager()->UnregisterResource(GetResourceManager()->GetCurrentResource(id));
}

bool WrappedGLES::Serialise_glGenQueries(GLsizei n, GLuint *ids)
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

void WrappedGLES::glGenQueries(GLsizei count, GLuint *ids)
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

bool WrappedGLES::Serialise_glBeginQuery(GLenum target, GLuint qid)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), qid)));

  if(m_State < WRITING)
  {
    m_Real.glBeginQuery(Target, GetResourceManager()->GetLiveResource(id).name);
    m_ActiveQueries[QueryIdx(Target)] = true;
  }

  return true;
}

void WrappedGLES::glBeginQuery(GLenum target, GLuint id)
{
  m_Real.glBeginQuery(target, id);
  m_ActiveQueries[QueryIdx(target)] = true;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_QUERY);
    Serialise_glBeginQuery(target, id);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

bool WrappedGLES::Serialise_glEndQuery(GLenum target)
{
  SERIALISE_ELEMENT(GLenum, Target, target);

  if(m_State < WRITING)
  {
    m_ActiveQueries[QueryIdx(Target)] = false;
    m_Real.glEndQuery(Target);
  }

  return true;
}

void WrappedGLES::glEndQuery(GLenum target)
{
  m_Real.glEndQuery(target);
  m_ActiveQueries[QueryIdx(target)] = false;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_QUERY);
    Serialise_glEndQuery(target);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glBeginConditionalRenderNV(GLuint id, GLenum mode)
{
  SERIALISE_ELEMENT(ResourceId, qid, GetResourceManager()->GetID(QueryRes(GetCtx(), id)));
  SERIALISE_ELEMENT(GLenum, Mode, mode);

  if(m_State < WRITING)
  {
    m_ActiveConditional = true;
    m_Real.glBeginConditionalRenderNV(GetResourceManager()->GetLiveResource(qid).name, Mode);
  }

  return true;
}

void WrappedGLES::glBeginConditionalRenderNV(GLuint id, GLenum mode)
{
  m_Real.glBeginConditionalRenderNV(id, mode);

  m_ActiveConditional = true;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_CONDITIONAL);
    Serialise_glBeginConditionalRenderNV(id, mode);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), id), eFrameRef_Read);
  }
}

bool WrappedGLES::Serialise_glEndConditionalRenderNV()
{
  if(m_State < WRITING)
  {
    m_ActiveConditional = false;
    m_Real.glEndConditionalRenderNV();
  }

  return true;
}

void WrappedGLES::glEndConditionalRenderNV()
{
  m_Real.glEndConditionalRenderNV();
  m_ActiveConditional = false;

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(END_CONDITIONAL);
    Serialise_glEndConditionalRenderNV();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glQueryCounterEXT(GLuint query, GLenum target)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(QueryRes(GetCtx(), query)));
  SERIALISE_ELEMENT(GLenum, Target, target);

  if(m_State < WRITING)
  {
    m_Real.glQueryCounterEXT(GetResourceManager()->GetLiveResource(id).name, Target);
  }

  return true;
}

void WrappedGLES::glQueryCounterEXT(GLuint query, GLenum target)
{
  m_Real.glQueryCounterEXT(query, target);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(QUERY_COUNTER);
    Serialise_glQueryCounterEXT(query, target);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(QueryRes(GetCtx(), query), eFrameRef_Read);
  }
}

void WrappedGLES::glDeleteQueries(GLsizei n, const GLuint *ids)
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
