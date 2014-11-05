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


#include "driver/gl/gl_manager.h"
#include "driver/gl/gl_driver.h"

struct VertexArrayInitialData
{
	VertexArrayInitialData()
	{
		RDCEraseEl(*this);
	}
	uint32_t enabled;
	uint32_t vbslot;
	uint32_t offset;
	GLenum   type;
	int32_t  normalized;
	uint32_t integer;
	uint32_t size;
};

bool GLResourceManager::SerialisableResource(ResourceId id, GLResourceRecord *record)
{
	if(id == m_GL->GetContextResourceID())
		return false;
	return true;
}

bool GLResourceManager::Need_InitialStateChunk(GLResource res)
{
	return res.Namespace != eResBuffer;
}

bool GLResourceManager::Prepare_InitialState(GLResource res)
{
	ResourceId Id = GetID(res);

	if(res.Namespace == eResBuffer)
	{
		GLResourceRecord *record = GetResourceRecord(res);

		// TODO copy this to an immutable buffer elsewhere and SetInitialContents() it.
		// then only do the readback in Serialise_InitialState
		
		GLint length;
		m_GL->glGetNamedBufferParameterivEXT(res.name, eGL_BUFFER_SIZE, &length);
	
		m_GL->glGetNamedBufferSubDataEXT(res.name, 0, length, record->GetDataPtr());
	}
	else if(res.Namespace == eResVertexArray)
	{
		GLuint VAO = 0;
		m_GL->glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);

		m_GL->glBindVertexArray(res.name);

		VertexArrayInitialData *data = (VertexArrayInitialData *)new byte[sizeof(VertexArrayInitialData)*16];

		for(GLuint i=0; i < 16; i++)
		{
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED, (GLint *)&data[i].enabled);
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING, (GLint *)&data[i].vbslot);
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET, (GLint*)&data[i].offset);
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&data[i].type);
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint *)&data[i].normalized);
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER, (GLint *)&data[i].integer);
			m_GL->glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&data[i].size);
		}

		SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, (byte *)data));

		m_GL->glBindVertexArray(VAO);
	}
	else
	{
		RDCERR("Unexpected type of resource requiring initial state");
	}

	return true;
}

bool GLResourceManager::Force_InitialState(GLResource res)
{
	return false;
}

bool GLResourceManager::Serialise_InitialState(GLResource res)
{
	ResourceId Id = ResourceId();

	if(m_State >= WRITING)
	{
		Id = GetID(res);

		if(res.Namespace != eResBuffer)
			m_pSerialiser->Serialise("Id", Id);
	}
	else
	{
		m_pSerialiser->Serialise("Id", Id);
	}
	
	if(m_State < WRITING)
	{
		if(HasLiveResource(Id))
			res = GetLiveResource(Id);
		else
			res = GLResource(MakeNullResource);
	}

	if(res.Namespace == eResBuffer)
	{
		// Nothing to serialize
	}
	else if(res.Namespace == eResVertexArray)
	{
		VertexArrayInitialData data[16];

		if(m_State >= WRITING)
		{
			VertexArrayInitialData *initialdata = (VertexArrayInitialData *)GetInitialContents(Id).blob;
			memcpy(data, initialdata, sizeof(data));
		}

		for(GLuint i=0; i < 16; i++)
		{
			m_pSerialiser->Serialise("data[].enabled", data[i].enabled);
			m_pSerialiser->Serialise("data[].vbslot", data[i].vbslot);
			m_pSerialiser->Serialise("data[].offset", data[i].offset);
			m_pSerialiser->Serialise("data[].type", data[i].type);
			m_pSerialiser->Serialise("data[].normalized", data[i].normalized);
			m_pSerialiser->Serialise("data[].integer", data[i].integer);
			m_pSerialiser->Serialise("data[].size", data[i].size);
		}

		if(m_State < WRITING)
		{
			byte *blob = new byte[sizeof(data)];
			memcpy(blob, data, sizeof(data));

			SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
		}
	}
	else
	{
		RDCERR("Unexpected type of resource requiring initial state");
	}

	return true;
}

void GLResourceManager::Create_InitialState(ResourceId id, GLResource live, bool hasData)
{
	if(live.Namespace != eResBuffer)
	{
		RDCUNIMPLEMENTED("Expect all initial states to be created & not skipped, presently");
	}
}

void GLResourceManager::Apply_InitialState(GLResource live, InitialContentData initial)
{
	if(live.Namespace == eResVertexArray)
	{
		GLuint VAO = 0;
		m_GL->glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);

		m_GL->glBindVertexArray(live.name);
	
		VertexArrayInitialData *initialdata = (VertexArrayInitialData *)initial.blob;	

		for(GLuint i=0; i < 16; i++)
		{
			if(initialdata[i].enabled)
				m_GL->glEnableVertexAttribArray(i);
			else
				m_GL->glDisableVertexAttribArray(i);

			m_GL->glVertexAttribBinding(i, initialdata[i].vbslot);

			if(initialdata[i].integer == 0)
				m_GL->glVertexAttribFormat(i, initialdata[i].size, initialdata[i].type, (GLboolean)initialdata[i].normalized, initialdata[i].offset);
			else
				m_GL->glVertexAttribIFormat(i, initialdata[i].size, initialdata[i].type, initialdata[i].offset);
		}

		m_GL->glBindVertexArray(VAO);
	}
	else
	{
		RDCERR("Unexpected type of resource requiring initial state");
	}
}
