/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include "common/common.h"
#include "common/string_utils.h"
#include "../gl_driver.h"

bool WrappedOpenGL::Serialise_glGenSamplers(GLsizei n, GLuint* samplers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(*samplers)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenSamplers(1, &real);
		
		GLResource res = SamplerRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenSamplers(GLsizei count, GLuint *samplers)
{
	m_Real.glGenSamplers(count, samplers);

	for(GLsizei i=0; i < count; i++)
	{
		GLResource res = SamplerRes(samplers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_SAMPLERS);
				Serialise_glGenSamplers(1, samplers+i);

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
	SERIALISE_ELEMENT(ResourceId, id, sampler ? GetResourceManager()->GetID(SamplerRes(sampler)) : ResourceId());
	
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
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameteri(res.name, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
	m_Real.glSamplerParameteri(sampler, pname, param);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERI);
		Serialise_glSamplerParameteri(sampler, pname, param);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(float, Param, param);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterf(res.name, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
	m_Real.glSamplerParameterf(sampler, pname, param);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERF);
		Serialise_glSamplerParameterf(sampler, pname, param);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameteriv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
	m_Real.glSamplerParameteriv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIV);
		Serialise_glSamplerParameteriv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(float, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterfv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
	m_Real.glSamplerParameterfv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERFV);
		Serialise_glSamplerParameterfv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterIiv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
	m_Real.glSamplerParameterIiv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIIV);
		Serialise_glSamplerParameterIiv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(uint32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterIuiv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params)
{
	m_Real.glSamplerParameterIuiv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIUIV);
		Serialise_glSamplerParameterIuiv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDeleteSamplers(GLsizei n, const GLuint *ids)
{
	m_Real.glDeleteSamplers(n, ids);

	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(SamplerRes(ids[i]));
}
