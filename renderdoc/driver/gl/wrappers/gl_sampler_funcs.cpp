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

bool WrappedOpenGL::Serialise_glGenSamplers(GLsizei n, GLuint *samplers)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), *samplers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenSamplers(1, &real);
    m_Real.glBindSampler(0, real);
    m_Real.glBindSampler(0, 0);

    GLResource res = SamplerRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glGenSamplers(GLsizei count, GLuint *samplers)
{
  m_Real.glGenSamplers(count, samplers);

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = SamplerRes(GetCtx(), samplers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_SAMPLERS);
        Serialise_glGenSamplers(1, samplers + i);

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

bool WrappedOpenGL::Serialise_glCreateSamplers(GLsizei n, GLuint *samplers)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), *samplers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glCreateSamplers(1, &real);

    GLResource res = SamplerRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glCreateSamplers(GLsizei count, GLuint *samplers)
{
  m_Real.glCreateSamplers(count, samplers);

  for(GLsizei i = 0; i < count; i++)
  {
    GLResource res = SamplerRes(GetCtx(), samplers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLERS);
        Serialise_glCreateSamplers(1, samplers + i);

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

bool WrappedOpenGL::Serialise_glBindSampler(GLuint unit, GLuint sampler)
{
  SERIALISE_ELEMENT(uint32_t, Unit, unit);
  SERIALISE_ELEMENT(ResourceId, id, sampler
                                        ? GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler))
                                        : ResourceId());

  if(m_State < WRITING)
  {
    if(id == ResourceId())
    {
      m_Real.glBindSampler(Unit, 0);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(id);
      m_Real.glBindSampler(Unit, res.name);
    }
  }

  return true;
}

void WrappedOpenGL::glBindSampler(GLuint unit, GLuint sampler)
{
  m_Real.glBindSampler(unit, sampler);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BIND_SAMPLER);
    Serialise_glBindSampler(unit, sampler);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler), eFrameRef_Read);
  }
}

bool WrappedOpenGL::Serialise_glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers)
{
  SERIALISE_ELEMENT(uint32_t, First, first);
  SERIALISE_ELEMENT(int32_t, Count, count);

  GLuint *samps = NULL;
  if(m_State <= EXECUTING)
    samps = new GLuint[Count];

  for(int32_t i = 0; i < Count; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id,
                      samplers && samplers[i]
                          ? GetResourceManager()->GetID(SamplerRes(GetCtx(), samplers[i]))
                          : ResourceId());

    if(m_State <= EXECUTING)
    {
      if(id != ResourceId())
        samps[i] = GetResourceManager()->GetLiveResource(id).name;
      else
        samps[i] = 0;
    }
  }

  if(m_State <= EXECUTING)
  {
    m_Real.glBindSamplers(First, Count, samps);

    delete[] samps;
  }

  return true;
}

void WrappedOpenGL::glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers)
{
  m_Real.glBindSamplers(first, count, samplers);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BIND_SAMPLERS);
    Serialise_glBindSamplers(first, count, samplers);

    m_ContextRecord->AddChunk(scope.Get());
    for(GLsizei i = 0; i < count; i++)
      if(samplers != NULL && samplers[i] != 0)
        GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), samplers[i]),
                                                          eFrameRef_Read);
  }
}

bool WrappedOpenGL::Serialise_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler)));
  SERIALISE_ELEMENT(GLenum, PName, pname);

  int32_t ParamValue = 0;

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(PName == GL_TEXTURE_WRAP_S || PName == GL_TEXTURE_WRAP_T || PName == GL_TEXTURE_WRAP_R ||
     PName == GL_TEXTURE_MIN_FILTER || PName == GL_TEXTURE_MAG_FILTER ||
     PName == GL_TEXTURE_COMPARE_MODE || PName == GL_TEXTURE_COMPARE_FUNC)
  {
    SERIALISE_ELEMENT(GLenum, Param, (GLenum)param);

    ParamValue = (int32_t)Param;
  }
  else
  {
    SERIALISE_ELEMENT(int32_t, Param, param);

    ParamValue = Param;
  }

  if(m_State < WRITING)
  {
    GLResource res = GetResourceManager()->GetLiveResource(id);
    m_Real.glSamplerParameteri(res.name, PName, ParamValue);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
  m_Real.glSamplerParameteri(sampler, pname, param);

  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(param == eGL_CLAMP)
    param = eGL_CLAMP_TO_EDGE;

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERI);
    Serialise_glSamplerParameteri(sampler, pname, param);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler))->AddChunk(scope.Get());
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler)));
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(float, Param, param);

  if(m_State < WRITING)
  {
    GLResource res = GetResourceManager()->GetLiveResource(id);
    m_Real.glSamplerParameterf(res.name, PName, Param);
  }

  return true;
}

void WrappedOpenGL::glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
  m_Real.glSamplerParameterf(sampler, pname, param);

  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(param == (float)eGL_CLAMP)
    param = (float)eGL_CLAMP_TO_EDGE;

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERF);
    Serialise_glSamplerParameterf(sampler, pname, param);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler))->AddChunk(scope.Get());
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler)));
  SERIALISE_ELEMENT(GLenum, PName, pname);
  const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
  SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

  if(m_State < WRITING)
  {
    GLResource res = GetResourceManager()->GetLiveResource(id);
    m_Real.glSamplerParameteriv(res.name, PName, Params);
  }

  delete[] Params;

  return true;
}

void WrappedOpenGL::glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
  m_Real.glSamplerParameteriv(sampler, pname, params);

  GLint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIV);
    Serialise_glSamplerParameteriv(sampler, pname, params);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler))->AddChunk(scope.Get());
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler)));
  SERIALISE_ELEMENT(GLenum, PName, pname);
  const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
  SERIALISE_ELEMENT_ARR(float, Params, params, nParams);

  if(m_State < WRITING)
  {
    GLResource res = GetResourceManager()->GetLiveResource(id);
    m_Real.glSamplerParameterfv(res.name, PName, Params);
  }

  delete[] Params;

  return true;
}

void WrappedOpenGL::glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
  m_Real.glSamplerParameterfv(sampler, pname, params);

  GLfloat clamptoedge[4] = {(float)eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == (float)eGL_CLAMP)
    params = clamptoedge;

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERFV);
    Serialise_glSamplerParameterfv(sampler, pname, params);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler))->AddChunk(scope.Get());
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler)));
  SERIALISE_ELEMENT(GLenum, PName, pname);
  const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
  SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

  if(m_State < WRITING)
  {
    GLResource res = GetResourceManager()->GetLiveResource(id);
    m_Real.glSamplerParameterIiv(res.name, PName, Params);
  }

  delete[] Params;

  return true;
}

void WrappedOpenGL::glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
  m_Real.glSamplerParameterIiv(sampler, pname, params);

  GLint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIIV);
    Serialise_glSamplerParameterIiv(sampler, pname, params);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler))->AddChunk(scope.Get());
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glSamplerParameterIuiv(GLuint sampler, GLenum pname,
                                                     const GLuint *params)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(GetCtx(), sampler)));
  SERIALISE_ELEMENT(GLenum, PName, pname);
  const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
  SERIALISE_ELEMENT_ARR(uint32_t, Params, params, nParams);

  if(m_State < WRITING)
  {
    GLResource res = GetResourceManager()->GetLiveResource(id);
    m_Real.glSamplerParameterIuiv(res.name, PName, Params);
  }

  delete[] Params;

  return true;
}

void WrappedOpenGL::glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params)
{
  m_Real.glSamplerParameterIuiv(sampler, pname, params);

  GLuint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIUIV);
    Serialise_glSamplerParameterIuiv(sampler, pname, params);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->GetResourceRecord(SamplerRes(GetCtx(), sampler))->AddChunk(scope.Get());
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(SamplerRes(GetCtx(), sampler),
                                                        eFrameRef_Read);
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

  m_Real.glDeleteSamplers(n, ids);
}
