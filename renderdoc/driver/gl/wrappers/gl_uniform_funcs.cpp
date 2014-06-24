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

bool WrappedOpenGL::Serialise_glUniformMatrix(GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(int32_t, Loc, location);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(uint8_t, Transpose, transpose);

	size_t elemsPerMat = 0;

	switch(Type)
	{
		case MAT4FV: elemsPerMat = 16; break;
		default:
			RDCERR("Unexpected uniform type to Serialise_glUniformMatrix: %d", Type);
	}

	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, sizeof(float)*elemsPerMat*Count);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(sizeof(float)*elemsPerMat*Count);

		switch(Type)
		{
			case MAT4FV: m_Real.glUniformMatrix4fv(Loc, Count, Transpose, (const GLfloat *)value); break;
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformMatrix: %d", Type);
		}
	}

	if(m_pSerialiser->GetDebugText())
	{
		switch(Type)
		{
			case MAT4FV:
			{
				float *f = (float *)value;
				if(Transpose)
				{
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0], f[4], f[8],  f[12]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[1], f[5], f[9],  f[13]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[2], f[6], f[10], f[14]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[3], f[7], f[11], f[15]);
				}
				else
				{
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0],  f[1],  f[2],  f[3]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[4],  f[5],  f[6],  f[7]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[8],  f[9],  f[10], f[11]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[12], f[13], f[14], f[15]);
				}
				break;
			}
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
		}
	}
	
	return true;
}

void WrappedOpenGL::glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	m_Real.glUniformMatrix4fv(location, count, transpose, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_MATRIX);
		Serialise_glUniformMatrix(location, count, transpose, value, MAT4FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glUniformVector(GLint location, GLsizei count, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(int32_t, Loc, location);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	
	size_t elemsPerVec = 0;

	switch(Type)
	{
		case VEC1IV:
		case VEC1UIV:
		case VEC1FV: elemsPerVec = 1; break;
		case VEC2FV: elemsPerVec = 2; break;
		case VEC3FV: elemsPerVec = 3; break;
		case VEC4FV: elemsPerVec = 4; break;
		default:
			RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
	}

	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, sizeof(float)*elemsPerVec*Count);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(sizeof(float)*elemsPerVec*Count);

		switch(Type)
		{
			case VEC1FV: m_Real.glUniform1fv(Loc, Count, (const GLfloat *)value); break;
			case VEC1IV: m_Real.glUniform1iv(Loc, Count, (const GLint *)value); break;
			case VEC1UIV: m_Real.glUniform1uiv(Loc, Count, (const GLuint *)value); break;
			case VEC2FV: m_Real.glUniform2fv(Loc, Count, (const GLfloat *)value); break;
			case VEC3FV: m_Real.glUniform3fv(Loc, Count, (const GLfloat *)value); break;
			case VEC4FV: m_Real.glUniform4fv(Loc, Count, (const GLfloat *)value); break;
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
		}
	}

	if(m_pSerialiser->GetDebugText())
	{
		switch(Type)
		{
			case VEC1FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f}\n", f[0]);
				break;
			}
			case VEC1IV:
			{
				int32_t *i = (int32_t *)value;
				m_pSerialiser->DebugPrint("value: {%d}\n", i[0]);
				break;
			}
			case VEC1UIV:
			{
				uint32_t *u = (uint32_t *)value;
				m_pSerialiser->DebugPrint("value: {%u}\n", u[0]);
				break;
			}
			case VEC2FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f}\n", f[0], f[1]);
				break;
			}
			case VEC3FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f}\n", f[0], f[1], f[2]);
				break;
			}
			case VEC4FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0], f[1], f[2], f[3]);
				break;
			}
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
		}
	}

	return true;
}

void WrappedOpenGL::glUniform1f(GLint location, GLfloat value)
{
	m_Real.glUniform1f(location, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, 1, &value, VEC1FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1i(GLint location, GLint value)
{
	m_Real.glUniform1i(location, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, 1, &value, VEC1IV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1ui(GLint location, GLuint value)
{
	m_Real.glUniform1ui(location, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, 1, &value, VEC1UIV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform1fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC1FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
	m_Real.glUniform1iv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC1IV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
	m_Real.glUniform1uiv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC1UIV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform2fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC2FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform3fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC3FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform4fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC4FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glProgramUniformVector(GLuint program, GLint location, GLsizei count, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(int32_t, Loc, location);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	
	size_t elemsPerVec = 0;

	switch(Type)
	{
		case VEC1IV:
		case VEC1UIV:
		case VEC1FV: elemsPerVec = 1; break;
		case VEC2FV: elemsPerVec = 2; break;
		case VEC3FV: elemsPerVec = 3; break;
		case VEC4FV: elemsPerVec = 4; break;
		default:
			RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
	}

	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, sizeof(float)*elemsPerVec*Count);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(sizeof(float)*elemsPerVec*Count);

		GLuint live = GetResourceManager()->GetLiveResource(id).name;

		switch(Type)
		{
			case VEC1IV: m_Real.glProgramUniform1iv(live, Loc, Count, (const GLint *)value); break;
			case VEC1UIV: m_Real.glProgramUniform1uiv(live, Loc, Count, (const GLuint *)value); break;
			case VEC1FV: m_Real.glProgramUniform1fv(live, Loc, Count, (const GLfloat *)value); break;
			case VEC2FV: m_Real.glProgramUniform2fv(live, Loc, Count, (const GLfloat *)value); break;
			case VEC3FV: m_Real.glProgramUniform3fv(live, Loc, Count, (const GLfloat *)value); break;
			case VEC4FV: m_Real.glProgramUniform4fv(live, Loc, Count, (const GLfloat *)value); break;
			default:
				RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
		}
	}

	if(m_pSerialiser->GetDebugText())
	{
		switch(Type)
		{
			case VEC1FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f}\n", f[0]);
				break;
			}
			case VEC1IV:
			{
				int32_t *i = (int32_t *)value;
				m_pSerialiser->DebugPrint("value: {%d}\n", i[0]);
				break;
			}
			case VEC1UIV:
			{
				uint32_t *u = (uint32_t *)value;
				m_pSerialiser->DebugPrint("value: {%u}\n", u[0]);
				break;
			}
			case VEC2FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f}\n", f[0], f[1]);
				break;
			}
			case VEC3FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f}\n", f[0], f[1], f[2]);
				break;
			}
			case VEC4FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0], f[1], f[2], f[3]);
				break;
			}
			default:
				RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
		}
	}

	return true;
}

void WrappedOpenGL::glProgramUniform1i(GLuint program, GLint location, GLint v0)
{
	m_Real.glProgramUniform1i(program, location, v0);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, 1, &v0, VEC1IV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
	m_Real.glProgramUniform1iv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC1IV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform1fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC1FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
	m_Real.glProgramUniform1uiv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC1UIV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform2fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC2FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform3fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC3FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform4fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC4FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
		else
		{
			// TODO grab this at capture time as initial state for program resources
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
	}
}
