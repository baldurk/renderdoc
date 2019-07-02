/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

static constexpr uint32_t numParams(GLenum pname)
{
  return pname == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenSamplers(SerialiserType &ser, GLsizei n, GLuint *samplers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(sampler, GetResourceManager()->GetID(SamplerRes(GetCtx(), *samplers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenSamplers(1, &real);
    GL.glBindSampler(0, real);
    GL.glBindSampler(0, 0);

    GLResource res = SamplerRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(sampler, res);

    AddResource(sampler, ResourceType::Sampler, "Sampler");
  }

  return true;
}

void WrappedOpenGL::glGenSamplers(GLsizei count, GLuint *samplers)
{
  SERIALISE_TIME_CALL(GL.glGenSamplers(count, samplers));

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = SamplerRes(GetCtx(), samplers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenSamplers(ser, 1, samplers + i);

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
bool WrappedOpenGL::Serialise_glCreateSamplers(SerialiserType &ser, GLsizei n, GLuint *samplers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(sampler, GetResourceManager()->GetID(SamplerRes(GetCtx(), *samplers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateSamplers(1, &real);

    GLResource res = SamplerRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(sampler, res);

    AddResource(sampler, ResourceType::Sampler, "Sampler");
  }

  return true;
}

void WrappedOpenGL::glCreateSamplers(GLsizei count, GLuint *samplers)
{
  SERIALISE_TIME_CALL(GL.glCreateSamplers(count, samplers));

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = SamplerRes(GetCtx(), samplers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateSamplers(ser, 1, samplers + i);

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
bool WrappedOpenGL::Serialise_glBindSampler(SerialiserType &ser, GLuint unit, GLuint samplerHandle)
{
  SERIALISE_ELEMENT(unit);
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    GL.glBindSampler(unit, sampler.name);

  return true;
}

void WrappedOpenGL::glBindSampler(GLuint unit, GLuint sampler)
{
  SERIALISE_TIME_CALL(GL.glBindSampler(unit, sampler));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindSampler(ser, unit, sampler);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindSamplers(SerialiserType &ser, GLuint first, GLsizei count,
                                             const GLuint *samplerHandles)
{
  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  std::vector<GLResource> samplers;

  if(ser.IsWriting())
  {
    samplers.reserve(count);
    for(int32_t i = 0; i < count; i++)
      samplers.push_back(SamplerRes(GetCtx(), samplerHandles ? samplerHandles[i] : 0));
  }

  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT(samplers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    std::vector<GLuint> samps;
    samps.reserve(count);
    for(int32_t i = 0; i < count; i++)
      samps.push_back(samplers[i].name);

    GL.glBindSamplers(first, count, samps.data());
  }

  return true;
}

void WrappedOpenGL::glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers)
{
  SERIALISE_TIME_CALL(GL.glBindSamplers(first, count, samplers));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindSamplers(ser, first, count, samplers);

    GetContextRecord()->AddChunk(scope.Get());
    for(GLsizei i = 0; i < count; i++)
      if(samplers != NULL && samplers[i] != 0)
        GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), samplers[i]),
                                                          eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSamplerParameteri(SerialiserType &ser, GLuint samplerHandle,
                                                  GLenum pname, GLint param)
{
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));
  SERIALISE_ELEMENT(pname);

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R ||
     pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER ||
     pname == GL_TEXTURE_COMPARE_MODE || pname == GL_TEXTURE_COMPARE_FUNC)
  {
    SERIALISE_ELEMENT_TYPED(GLenum, param);
  }
  else
  {
    SERIALISE_ELEMENT(param);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSamplerParameteri(sampler.name, pname, param);

    AddResourceInitChunk(sampler);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
  SERIALISE_TIME_CALL(GL.glSamplerParameteri(sampler, pname, param));

  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(param == eGL_CLAMP)
    param = eGL_CLAMP_TO_EDGE;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSamplerParameteri(ser, sampler, pname, param);

    if(IsBackgroundCapturing(m_State))
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->UpdateCount > 20)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSamplerParameterf(SerialiserType &ser, GLuint samplerHandle,
                                                  GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSamplerParameterf(sampler.name, pname, param);

    AddResourceInitChunk(sampler);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
  SERIALISE_TIME_CALL(GL.glSamplerParameterf(sampler, pname, param));

  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(param == (float)eGL_CLAMP)
    param = (float)eGL_CLAMP_TO_EDGE;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSamplerParameterf(ser, sampler, pname, param);

    if(IsBackgroundCapturing(m_State))
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->UpdateCount > 20)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSamplerParameteriv(SerialiserType &ser, GLuint samplerHandle,
                                                   GLenum pname, const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSamplerParameteriv(sampler.name, pname, params);

    AddResourceInitChunk(sampler);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
  SERIALISE_TIME_CALL(GL.glSamplerParameteriv(sampler, pname, params));

  GLint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSamplerParameteriv(ser, sampler, pname, params);

    if(IsBackgroundCapturing(m_State))
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->UpdateCount > 20)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSamplerParameterfv(SerialiserType &ser, GLuint samplerHandle,
                                                   GLenum pname, const GLfloat *params)
{
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSamplerParameterfv(sampler.name, pname, params);

    AddResourceInitChunk(sampler);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
  SERIALISE_TIME_CALL(GL.glSamplerParameterfv(sampler, pname, params));

  GLfloat clamptoedge[4] = {(float)eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == (float)eGL_CLAMP)
    params = clamptoedge;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSamplerParameterfv(ser, sampler, pname, params);

    if(IsBackgroundCapturing(m_State))
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->UpdateCount > 20)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSamplerParameterIiv(SerialiserType &ser, GLuint samplerHandle,
                                                    GLenum pname, const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSamplerParameterIiv(sampler.name, pname, params);

    AddResourceInitChunk(sampler);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
  SERIALISE_TIME_CALL(GL.glSamplerParameterIiv(sampler, pname, params));

  GLint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSamplerParameterIiv(ser, sampler, pname, params);

    if(IsBackgroundCapturing(m_State))
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->UpdateCount > 20)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSamplerParameterIuiv(SerialiserType &ser, GLuint samplerHandle,
                                                     GLenum pname, const GLuint *params)
{
  SERIALISE_ELEMENT_LOCAL(sampler, SamplerRes(GetCtx(), samplerHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSamplerParameterIuiv(sampler.name, pname, params);

    AddResourceInitChunk(sampler);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params)
{
  SERIALISE_TIME_CALL(GL.glSamplerParameterIuiv(sampler, pname, params));

  GLuint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSamplerParameterIuiv(ser, sampler, pname, params);

    if(IsBackgroundCapturing(m_State))
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);

      if(record->UpdateCount > 20)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_ReadBeforeWrite);
    }
  }
}

void WrappedOpenGL::glDeleteSamplers(GLsizei n, const GLuint *ids)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = SamplerRes(GetCtx(), ids[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteSamplers(n, ids);
}

INSTANTIATE_FUNCTION_SERIALISED(void, glGenSamplers, GLsizei n, GLuint *samplers);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateSamplers, GLsizei n, GLuint *samplers);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindSampler, GLuint unit, GLuint sampler);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindSamplers, GLuint first, GLsizei count,
                                const GLuint *samplers);
INSTANTIATE_FUNCTION_SERIALISED(void, glSamplerParameteri, GLuint sampler, GLenum pname, GLint param);
INSTANTIATE_FUNCTION_SERIALISED(void, glSamplerParameterf, GLuint sampler, GLenum pname,
                                GLfloat param);
INSTANTIATE_FUNCTION_SERIALISED(void, glSamplerParameteriv, GLuint sampler, GLenum pname,
                                const GLint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glSamplerParameterfv, GLuint sampler, GLenum pname,
                                const GLfloat *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glSamplerParameterIiv, GLuint sampler, GLenum pname,
                                const GLint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glSamplerParameterIuiv, GLuint sampler, GLenum pname,
                                const GLuint *params);
