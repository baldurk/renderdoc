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


bool WrappedOpenGL::Serialise_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
	ResourceId liveid;

	string Label;
	if(m_State >= WRITING)
	{
		if(length == 0)
			Label = "";
		else
			Label = string(label, label + (length > 0 ? length : strlen(label)));

		switch(identifier)
		{
			case eGL_TEXTURE:
				liveid = GetResourceManager()->GetID(TextureRes(GetCtx(), name));
				break;
			case eGL_BUFFER:
				liveid = GetResourceManager()->GetID(BufferRes(GetCtx(), name));
				break;
			default:
				RDCERR("Unhandled namespace in glObjectLabel");
		}
	}

	SERIALISE_ELEMENT(GLenum, Identifier, identifier);
	SERIALISE_ELEMENT(ResourceId, id, liveid);
	SERIALISE_ELEMENT(uint32_t, Length, length);
	SERIALISE_ELEMENT(bool, HasLabel, label != NULL);

	m_pSerialiser->SerialiseString("label", Label);
	
	if(m_State == READING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glObjectLabel(Identifier, res.name, Length, HasLabel ? Label.c_str() : NULL);
	}

	return true;
}

void WrappedOpenGL::glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
	m_Real.glObjectLabel(identifier, name, length, label);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(OBJECT_LABEL);
		Serialise_glObjectLabel(identifier, name, length, label);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
	m_Real.glObjectPtrLabel(ptr, length, label);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(OBJECT_LABEL);
		ResourceId id = GetResourceManager()->GetSyncID((GLsync)ptr);
		Serialise_glObjectLabel(eGL_SYNC_FENCE, GetResourceManager()->GetCurrentResource(id).name, length, label);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam)
{
	m_RealDebugFunc = callback;
	m_RealDebugFuncParam = userParam;

	m_Real.glDebugMessageCallback(&DebugSnoopStatic, this);
}

void WrappedOpenGL::glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)
{
	// we could exert control over debug messages here
	m_Real.glDebugMessageControl(source, type, severity, count, ids, enabled);
}

bool WrappedOpenGL::Serialise_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)
{
	wstring name = buf ? widen(string(buf, buf+length)) : L"";

	m_pSerialiser->Serialise("Name", name);

	if(m_State == READING)
	{
		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_SetMarker;

		AddDrawcall(draw, false);
	}

	return true;
}

void WrappedOpenGL::glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)
{
	if(m_State == WRITING_CAPFRAME && type == eGL_DEBUG_TYPE_MARKER)
	{
		SCOPED_SERIALISE_CONTEXT(SET_MARKER);
		Serialise_glDebugMessageInsert(source, type, id, severity, length, buf);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glDebugMessageInsert(source, type, id, severity, length, buf);
}

bool WrappedOpenGL::Serialise_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
	wstring name = message ? widen(string(message, message+length)) : L"";

	m_pSerialiser->Serialise("Name", name);

	if(m_State == READING)
	{
		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, false);
	}

	return true;
}

void WrappedOpenGL::glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
		Serialise_glPushDebugGroup(source, id, length, message);

		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glPushDebugGroup(source, id, length, message);
}

bool WrappedOpenGL::Serialise_glPopDebugGroup()
{
	if(m_State == READING && !m_CurEvents.empty())
	{
		FetchDrawcall draw;
		draw.name = L"API Calls";
		draw.flags |= eDraw_SetMarker;

		AddDrawcall(draw, true);
	}

	return true;
}
void WrappedOpenGL::glPopDebugGroup()
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(END_EVENT);
		Serialise_glPopDebugGroup();

		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glPopDebugGroup();
}
