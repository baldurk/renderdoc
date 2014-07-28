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

bool WrappedOpenGL::Serialise_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(int32_t, First, first);
	SERIALISE_ELEMENT(uint32_t, Count, count);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawArrays(Mode, First, Count);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS, desc);
		string name = "glDrawArrays(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = First;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		m_LastDrawMode = Mode;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	m_Real.glDrawArrays(mode, first, count);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS);
		Serialise_glDrawArrays(mode, first, count);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(int32_t, First, first);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(uint32_t, InstanceCount, instancecount);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawArraysInstanced(Mode, First, Count, InstanceCount);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS_INSTANCED, desc);
		string name = "glDrawArraysInstanced(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(InstanceCount) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstanceCount;
		draw.indexOffset = 0;
		draw.vertexOffset = First;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		m_LastDrawMode = Mode;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
	m_Real.glDrawArraysInstanced(mode, first, count, instancecount);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCED);
		Serialise_glDrawArraysInstanced(mode, first, count, instancecount);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(int32_t, First, first);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(uint32_t, InstanceCount, instancecount);
	SERIALISE_ELEMENT(uint32_t, BaseInstance, baseinstance);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawArraysInstancedBaseInstance(Mode, First, Count, InstanceCount, BaseInstance);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS_INSTANCEDBASEINSTANCE, desc);
		string name = "glDrawArraysInstancedBaseInstance(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(InstanceCount) + ", " +
						ToStr::Get(BaseInstance) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstanceCount;
		draw.indexOffset = 0;
		draw.vertexOffset = First;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		m_LastDrawMode = Mode;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
	m_Real.glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCEDBASEINSTANCE);
		Serialise_glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElements(Mode, Count, Type, (const void *)IdxOffset);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS, desc);
		string name = "glDrawElements(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	m_Real.glDrawElements(mode, count, type, indices);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS);
		Serialise_glDrawElements(mode, count, type, indices);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Start, start);
	SERIALISE_ELEMENT(uint32_t, End, end);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawRangeElements(Mode, Start, End, Count, Type, (const void *)IdxOffset);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS, desc);
		string name = "glDrawRangeElements(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
	m_Real.glDrawRangeElements(mode, start, end, count, type, indices);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTS);
		Serialise_glDrawRangeElements(mode, start, end, count, type, indices);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	SERIALISE_ELEMENT(int32_t, BaseVtx, basevertex);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsBaseVertex(Mode, Count, Type, (const void *)IdxOffset, BaseVtx);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_BASEVERTEX, desc);
		string name = "glDrawElementsBaseVertex(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(BaseVtx) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = BaseVtx;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
	m_Real.glDrawElementsBaseVertex(mode, count, type, indices, basevertex);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_BASEVERTEX);
		Serialise_glDrawElementsBaseVertex(mode, count, type, indices, basevertex);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstanced(Mode, Count, Type, (const void *)IdxOffset, InstCount);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCED, desc);
		string name = "glDrawElementsInstanced(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
	m_Real.glDrawElementsInstanced(mode, count, type, indices, instancecount);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCED);
		Serialise_glDrawElementsInstanced(mode, count, type, indices, instancecount);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);
	SERIALISE_ELEMENT(uint32_t, BaseInstance, baseinstance);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstancedBaseInstance(Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseInstance);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCEDBASEINSTANCE, desc);
		string name = "glDrawElementsInstancedBaseInstance(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ", " + 
						ToStr::Get(BaseInstance) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = 0;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
	m_Real.glDrawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEINSTANCE);
		Serialise_glDrawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);
	SERIALISE_ELEMENT(int32_t, BaseVertex, basevertex);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstancedBaseVertex(Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseVertex);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCEDBASEVERTEX, desc);
		string name = "glDrawElementsInstancedBaseVertex(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ", " + 
						ToStr::Get(BaseVertex) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = BaseVertex;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
	m_Real.glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEX);
		Serialise_glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	SERIALISE_ELEMENT(uint32_t, InstCount, instancecount);
	SERIALISE_ELEMENT(int32_t, BaseVertex, basevertex);
	SERIALISE_ELEMENT(uint32_t, BaseInstance, baseinstance);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstancedBaseVertexBaseInstance(Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseVertex, BaseInstance);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE, desc);
		string name = "glDrawElementsInstancedBaseVertexBaseInstance(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ", " + 
						ToStr::Get(BaseVertex) + ", " +
						ToStr::Get(BaseInstance) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = (uint32_t)IdxOffset;
		draw.vertexOffset = BaseVertex;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		m_LastDrawMode = Mode;
		m_LastIndexSize = Type;
		m_LastIndexOffset = (GLuint)IdxOffset;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
	m_Real.glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE);
		Serialise_glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	
	if(buf != eGL_DEPTH)
	{
		Vec4f v;
		if(value) v = *((Vec4f *)value);

		m_pSerialiser->Serialise<4>("value", (float *)&v.x);
		
		if(m_State <= EXECUTING)
			m_Real.glClearBufferfv(buf, draw, &v.x);
	}
	else
	{
		SERIALISE_ELEMENT(float, val, *value);

		if(m_State <= EXECUTING)
			m_Real.glClearBufferfv(buf, draw, &val);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERF, desc);
		string name = "glClearBufferfv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}


	return true;
}

void WrappedOpenGL::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	m_Real.glClearBufferfv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERF);
		Serialise_glClearBufferfv(buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	
	if(buf != eGL_STENCIL)
	{
		int32_t v[4];
		if(value) memcpy(v, value, sizeof(v));

		m_pSerialiser->Serialise<4>("value", v);
		
		if(m_State <= EXECUTING)
			m_Real.glClearBufferiv(buf, draw, v);
	}
	else
	{
		SERIALISE_ELEMENT(int32_t, val, *value);

		if(m_State <= EXECUTING)
			m_Real.glClearBufferiv(buf, draw, &val);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERI, desc);
		string name = "glClearBufferiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}


	return true;
}

void WrappedOpenGL::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
	m_Real.glClearBufferiv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERI);
		Serialise_glClearBufferiv(buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	
	{
		uint32_t v[4];
		if(value) memcpy(v, value, sizeof(v));

		m_pSerialiser->Serialise<4>("value", v);
		
		if(m_State <= EXECUTING)
			m_Real.glClearBufferuiv(buf, draw, v);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERUI, desc);
		string name = "glClearBufferuiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	m_Real.glClearBufferuiv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERUI);
		Serialise_glClearBufferuiv(buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}
		
bool WrappedOpenGL::Serialise_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(int32_t, s, stencil);
	
	if(m_State <= EXECUTING)
		m_Real.glClearBufferfi(buf, draw, d, s);
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERFI, desc);
		string name = "glClearBufferfi(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
	m_Real.glClearBufferfi(buffer, drawbuffer, depth, stencil);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERFI);
		Serialise_glClearBufferfi(buffer, drawbuffer, depth, stencil);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClear(GLbitfield mask)
{
	SERIALISE_ELEMENT(uint32_t, Mask, mask);

	if(m_State <= EXECUTING)
		m_Real.glClear(Mask);
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERF, desc);
		string name = "glClear(";
		if(Mask & GL_DEPTH_BUFFER_BIT)
			name += "GL_DEPTH_BUFFER_BIT | ";
		if(Mask & GL_COLOR_BUFFER_BIT)
			name += "GL_COLOR_BUFFER_BIT | ";
		if(Mask & GL_STENCIL_BUFFER_BIT)
			name += "GL_STENCIL_BUFFER_BIT | ";

		if(Mask & (eGL_DEPTH_BUFFER_BIT|eGL_COLOR_BUFFER_BIT|eGL_STENCIL_BUFFER_BIT))
		{
			name.pop_back(); // ' '
			name.pop_back(); // '|'
			name.pop_back(); // ' '
		}

		name += ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glClear(GLbitfield mask)
{
	m_Real.glClear(mask);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR);
		Serialise_glClear(mask);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}
