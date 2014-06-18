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


GLenum WrappedOpenGL::glGetError()
{
	return m_Real.glGetError();
}

void WrappedOpenGL::glFlush()
{
	m_Real.glFlush();
}

void WrappedOpenGL::glFinish()
{
	m_Real.glFinish();
}

GLboolean WrappedOpenGL::glIsEnabled(GLenum cap)
{
	return m_Real.glIsEnabled(cap);
}

GLboolean WrappedOpenGL::glIsEnabledi(GLenum cap, GLuint index)
{
	return m_Real.glIsEnabledi(cap, index);
}

void WrappedOpenGL::glGetFloatv(GLenum pname, GLfloat *params)
{
	m_Real.glGetFloatv(pname, params);
}

void WrappedOpenGL::glGetDoublev(GLenum pname, GLdouble *params)
{
	m_Real.glGetDoublev(pname, params);
}

void WrappedOpenGL::glGetIntegerv(GLenum pname, GLint *params)
{
	if(pname == GL_NUM_EXTENSIONS)
	{
		if(params)
			*params = (GLint)glExts.size();
		return;
	}

	m_Real.glGetIntegerv(pname, params);
}

void WrappedOpenGL::glGetBooleanv(GLenum pname, GLboolean *data)
{
	m_Real.glGetBooleanv(pname, data);
}

void WrappedOpenGL::glGetBooleani_v(GLenum pname, GLuint index, GLboolean *data)
{
	m_Real.glGetBooleani_v(pname, index, data);
}

void WrappedOpenGL::glGetIntegeri_v(GLenum pname, GLuint index, GLint *data)
{
	m_Real.glGetIntegeri_v(pname, index, data);
}

void WrappedOpenGL::glGetFloati_v(GLenum pname, GLuint index, GLfloat *data)
{
	m_Real.glGetFloati_v(pname, index, data);
}

void WrappedOpenGL::glGetDoublei_v(GLenum pname, GLuint index, GLdouble *data)
{
	m_Real.glGetDoublei_v(pname, index, data);
}

void WrappedOpenGL::glGetInteger64i_v(GLenum pname, GLuint index, GLint64 *data)
{
	m_Real.glGetInteger64i_v(pname, index, data);
}

void WrappedOpenGL::glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	m_Real.glGetTexLevelParameteriv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	m_Real.glGetTexLevelParameterfv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	m_Real.glGetTexParameterfv(target, pname, params);
}

void WrappedOpenGL::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	m_Real.glGetTexParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
	m_Real.glGetTexImage(target, level, format, type, pixels);
}

void WrappedOpenGL::glGetCompressedTexImage(GLenum target, GLint level, void *img)
{
	m_Real.glGetCompressedTexImage(target, level, img);
}

void WrappedOpenGL::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params)
{
	m_Real.glGetInternalformativ(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params)
{
	m_Real.glGetInternalformati64v(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
	m_Real.glGetBufferParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
	m_Real.glGetBufferSubData(target, offset, size, data);
}

const GLubyte *WrappedOpenGL::glGetString(GLenum name)
{
	if(name == GL_EXTENSIONS)
	{
		return (const GLubyte *)glExtsString.c_str();
	}
	return m_Real.glGetString(name);
}

const GLubyte *WrappedOpenGL::glGetStringi(GLenum name, GLuint i)
{
	if(name == GL_EXTENSIONS)
	{
		if((size_t)i < glExts.size())
			return (const GLubyte *)glExts[i].c_str();

		return (const GLubyte *)"";
	}
	return m_Real.glGetStringi(name, i);
}

void WrappedOpenGL::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
	m_Real.glGetVertexAttribiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
	m_Real.glGetVertexAttribPointerv(index, pname, pointer);
}

void WrappedOpenGL::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
	m_Real.glGetShaderiv(shader, pname, params);
}

void WrappedOpenGL::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	m_Real.glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
	m_Real.glGetProgramiv(program, pname, params);
}

void WrappedOpenGL::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	m_Real.glGetProgramInfoLog(program, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
	m_Real.glGetProgramPipelineiv(pipeline, pname, params);
}

void WrappedOpenGL::glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	m_Real.glGetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params)
{
	m_Real.glGetProgramInterfaceiv(program, programInterface, pname, params);
}

void WrappedOpenGL::glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params)
{
	m_Real.glGetProgramResourceiv(program, programInterface, index, propCount, props, bufSize, length, params);
}

void WrappedOpenGL::glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
	m_Real.glGetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

GLint WrappedOpenGL::glGetUniformLocation(GLuint program, const GLchar *name)
{
	return m_Real.glGetUniformLocation(program, name);
}

void WrappedOpenGL::glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices)
{
	m_Real.glGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

GLuint WrappedOpenGL::glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
	return m_Real.glGetUniformBlockIndex(program, uniformBlockName);
}

GLint WrappedOpenGL::glGetAttribLocation(GLuint program, const GLchar *name)
{
	return m_Real.glGetAttribLocation(program, name);
}

void WrappedOpenGL::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
	m_Real.glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params)
{
	m_Real.glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

void WrappedOpenGL::glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
	m_Real.glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
	m_Real.glGetUniformfv(program, location, params);
}

void WrappedOpenGL::glGetUniformiv(GLuint program, GLint location, GLint *params)
{
	m_Real.glGetUniformiv(program, location, params);
}

void WrappedOpenGL::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
	m_Real.glReadPixels(x, y, width, height, format, type, pixels);
}
