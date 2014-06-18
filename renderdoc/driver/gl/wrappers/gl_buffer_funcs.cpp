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


bool WrappedOpenGL::Serialise_glGenBuffers(GLsizei n, GLuint* textures)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(*textures)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenBuffers(1, &real);
		
		GLResource res = BufferRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);

		m_Buffers[live].resource = res;
		m_Buffers[live].curType = eGL_UNKNOWN_ENUM;
	}

	return true;
}

void WrappedOpenGL::glGenBuffers(GLsizei n, GLuint *buffers)
{
	m_Real.glGenBuffers(n, buffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = BufferRes(buffers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_BUFFER);
				Serialise_glGenBuffers(1, buffers+i);

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

size_t WrappedOpenGL::BufferIdx(GLenum buf)
{
	switch(buf)
	{
		case eGL_ARRAY_BUFFER:              return 0;
		case eGL_ATOMIC_COUNTER_BUFFER:     return 1;
		case eGL_COPY_READ_BUFFER:          return 2;
		case eGL_COPY_WRITE_BUFFER:         return 3;
		case eGL_DRAW_INDIRECT_BUFFER:      return 4;
		case eGL_DISPATCH_INDIRECT_BUFFER:  return 5;
		case eGL_ELEMENT_ARRAY_BUFFER:      return 6;
		case eGL_PIXEL_PACK_BUFFER:         return 7;
		case eGL_PIXEL_UNPACK_BUFFER:       return 8;
		case eGL_QUERY_BUFFER:              return 9;
		case eGL_SHADER_STORAGE_BUFFER:     return 10;
		case eGL_TEXTURE_BUFFER:            return 11;
		case eGL_TRANSFORM_FEEDBACK_BUFFER: return 12;
		case eGL_UNIFORM_BUFFER:            return 13;
		default:
			RDCERR("Unexpected enum as buffer target: %hs", ToStr::Get(buf).c_str());
	}

	return 0;
}

bool WrappedOpenGL::Serialise_glBindBuffer(GLenum target, GLuint buffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(BufferRes(buffer)));
	
	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		m_BufferRecord[idx]->datatype = Target;
	}
	else if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(Id);
		m_Real.glBindBuffer(Target, res.name);

		m_Buffers[GetResourceManager()->GetLiveID(Id)].curType = Target;
	}

	return true;
}

void WrappedOpenGL::glBindBuffer(GLenum target, GLuint buffer)
{
	m_Real.glBindBuffer(target, buffer);
	
	if(m_State == WRITING_CAPFRAME)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
			Serialise_glBindBuffer(target, buffer);

			chunk = scope.Get();
		}
		
		m_ContextRecord->AddChunk(chunk);
	}

	size_t idx = BufferIdx(target);

	if(buffer == 0)
	{
		m_BufferRecord[idx] = NULL;
		return;
	}

	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(buffer));

		// it's legal to re-type buffers, generate another BindBuffer chunk to rename
		if(r->datatype != target)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
				Serialise_glBindBuffer(target, buffer);

				chunk = scope.Get();
			}

			r->AddChunk(chunk);
		}
	}
}

bool WrappedOpenGL::Serialise_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);

	byte *dummy = NULL;

	if(m_State >= WRITING && data == NULL)
	{
		dummy = new byte[size];
		data = dummy;
	}

	SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

	uint64_t offs = m_pSerialiser->GetOffset();

	SERIALISE_ELEMENT(GLenum, Usage, usage);
	SERIALISE_ELEMENT(ResourceId, id, m_BufferRecord[BufferIdx(target)]->GetResourceID());

	if(m_State == READING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glBindBuffer(Target, res.name);
		m_Real.glBufferData(Target, (GLsizeiptr)Bytesize, bytes, Usage);
		
		m_Buffers[GetResourceManager()->GetLiveID(id)].size = Bytesize;

		SAFE_DELETE_ARRAY(bytes);
	}
	else if(m_State >= WRITING)
	{
		m_BufferRecord[BufferIdx(target)]->SetDataOffset(offs - Bytesize);
	}

	if(dummy)
		delete[] dummy;

	return true;
}

void WrappedOpenGL::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	m_Real.glBufferData(target, size, data, usage);
	
	size_t idx = BufferIdx(target);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_BufferRecord[idx]);

		SCOPED_SERIALISE_CONTEXT(BUFFERDATA);
		Serialise_glBufferData(target, size, data, usage);

		Chunk *chunk = scope.Get();

		m_BufferRecord[idx]->AddChunk(chunk);
		m_BufferRecord[idx]->SetDataPtr(chunk->GetData());
		m_BufferRecord[idx]->Length = (int32_t)size;
	}
}

bool WrappedOpenGL::Serialise_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_BufferRecord[BufferIdx(target)]->GetResourceID());

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glBindBufferBase(Target, Index, res.name);
	}

	return true;
}

void WrappedOpenGL::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_BASE);
		Serialise_glBindBufferBase(target, index, buffer);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glBindBufferBase(target, index, buffer);
}

bool WrappedOpenGL::Serialise_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_BufferRecord[BufferIdx(target)]->GetResourceID());
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glBindBufferRange(Target, Index, res.name, (GLintptr)Offset, (GLsizeiptr)Size);
	}

	return true;
}

void WrappedOpenGL::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_RANGE);
		Serialise_glBindBufferRange(target, index, buffer, offset, size);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glBindBufferRange(target, index, buffer, offset, size);
}

void *WrappedOpenGL::glMapBuffer(GLenum target, GLenum access)
{
	if(m_State >= WRITING)
	{
		GLint length;
		m_Real.glGetBufferParameteriv(target, eGL_BUFFER_SIZE, &length);

		// TODO align return pointer to GL_MIN_MAP_BUFFER_ALIGNMENT (min 64)

		m_BufferRecord[BufferIdx(target)]->Map.offset = 0;
		m_BufferRecord[BufferIdx(target)]->Map.length = length;
		     if(access == eGL_READ_ONLY)  m_BufferRecord[BufferIdx(target)]->Map.access = GL_MAP_READ_BIT;
		else if(access == eGL_WRITE_ONLY) m_BufferRecord[BufferIdx(target)]->Map.access = GL_MAP_WRITE_BIT;
		else if(access == eGL_READ_WRITE) m_BufferRecord[BufferIdx(target)]->Map.access = GL_MAP_READ_BIT|GL_MAP_WRITE_BIT;

		if(access == eGL_READ_ONLY)
		{
			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Read_Real;
			return m_Real.glMapBuffer(target, access);
		}

		byte *ptr = m_BufferRecord[BufferIdx(target)]->GetDataPtr();

		if(ptr == NULL)
		{
			ptr = new byte[length];

			RDCWARN("Mapping buffer that hasn't been allocated");

			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write_Alloc;
		}
		else
		{
			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write;
		}

		m_Real.glGetBufferSubData(target, 0, length, ptr);

		return ptr;
	}

	return m_Real.glMapBuffer(target, access);
}

void *WrappedOpenGL::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
	if(m_State >= WRITING)
	{
		// TODO align return pointer to GL_MIN_MAP_BUFFER_ALIGNMENT (min 64)

		if((access & (GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_READ_BIT)) == 0)
			RDCUNIMPLEMENTED("haven't implemented non-invalidating glMap WRITE");
		
		if((access & (GL_MAP_COHERENT_BIT|GL_MAP_PERSISTENT_BIT)) != 0)
			RDCUNIMPLEMENTED("haven't implemented coherent/persistant glMap calls");

		m_BufferRecord[BufferIdx(target)]->Map.offset = offset;
		m_BufferRecord[BufferIdx(target)]->Map.length = length;
		m_BufferRecord[BufferIdx(target)]->Map.access = access;

		if((access & GL_MAP_READ_BIT) != 0)
		{
			byte *ptr = m_BufferRecord[BufferIdx(target)]->GetDataPtr();

			if(ptr == NULL)
			{
				RDCWARN("Mapping buffer that hasn't been allocated");

				m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Read_Real;
				return m_Real.glMapBufferRange(target, offset, length, access);
			}

			ptr += offset;

			m_Real.glGetBufferSubData(target, offset, length, ptr);
			
			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Read;

			return ptr;
		}
		
		byte *ptr = m_BufferRecord[BufferIdx(target)]->GetDataPtr();

		if(ptr == NULL)
		{
			RDCWARN("Mapping buffer that hasn't been allocated");
			
			ptr = (byte *)m_Real.glMapBufferRange(target, offset, length, access);

			m_BufferRecord[BufferIdx(target)]->Map.ptr = ptr;
			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write_Real;
		}
		else
		{
			if(m_State == WRITING_CAPFRAME)
			{
				ptr = new byte[length];
				
				m_BufferRecord[BufferIdx(target)]->Map.ptr = ptr;
				m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write_Alloc;
			}
			else
			{
				ptr += offset;
				
				m_BufferRecord[BufferIdx(target)]->Map.ptr = ptr;
				m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write;
			}
		}

		return ptr;
	}

	return m_Real.glMapBufferRange(target, offset, length, access);
}

bool WrappedOpenGL::Serialise_glUnmapBuffer(GLenum target)
{
	GLResourceRecord *record = NULL;

	if(m_State >= WRITING)
		record = m_BufferRecord[BufferIdx(target)];

	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, bufID, record->GetResourceID());
	SERIALISE_ELEMENT(uint64_t, offs, record->Map.offset);
	SERIALISE_ELEMENT(uint64_t, len, record->Map.length);

	uint64_t bufBindStart = 0;

	if(m_State >= WRITING)
	{
		if(Target == eGL_ATOMIC_COUNTER_BUFFER)
			m_Real.glGetInteger64i_v(eGL_ATOMIC_COUNTER_BUFFER_START, 0, (GLint64 *)&bufBindStart);
		if(Target == eGL_SHADER_STORAGE_BUFFER)
			m_Real.glGetInteger64i_v(eGL_SHADER_STORAGE_BUFFER_START, 0, (GLint64 *)&bufBindStart);
		if(Target == eGL_TRANSFORM_FEEDBACK_BUFFER)
			m_Real.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, 0, (GLint64 *)&bufBindStart);
		if(Target == eGL_UNIFORM_BUFFER)
			m_Real.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, 0, (GLint64 *)&bufBindStart);
	}

	SERIALISE_ELEMENT(uint64_t, bufOffs, bufBindStart);

	SERIALISE_ELEMENT_BUF(byte *, data, record->Map.ptr, (size_t)len);

	if(m_State >= WRITING && record && record->GetDataPtr() == NULL)
		record->SetDataOffset(m_pSerialiser->GetOffset() - (uint64_t)len);

	if(m_State < WRITING ||
			(m_State >= WRITING &&
				(record->Map.status == GLResourceRecord::Mapped_Write || record->Map.status == GLResourceRecord::Mapped_Write_Alloc)
			)
		)
	{
		GLuint oldBuf = 0;
		GLuint64 oldBufBase = 0;
		GLuint64 oldBufSize = 0;

		if(m_State == READING)
		{
			GLResource res = GetResourceManager()->GetLiveResource(bufID);
			m_Real.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 0, (GLint *)&oldBuf);
			m_Real.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, 0, (GLint64 *)&oldBufBase);
			m_Real.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, 0, (GLint64 *)&oldBufSize);
			m_Real.glBindBufferRange(eGL_UNIFORM_BUFFER, 0, res.name, (GLintptr)bufOffs, (GLsizeiptr)len);
		}

		void *ptr = m_Real.glMapBufferRange(Target, (GLintptr)offs, (GLsizeiptr)len, GL_MAP_WRITE_BIT);
		memcpy(ptr, data, (size_t)len);
		m_Real.glUnmapBuffer(Target);
		
		if(m_State == READING)
		{
			if(oldBufBase == 0 && oldBufSize == 0)
				m_Real.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, oldBuf);
			else
				m_Real.glBindBufferRange(eGL_UNIFORM_BUFFER, 0, oldBuf, (GLintptr)oldBufBase, (GLsizeiptr)oldBufSize);
		}
	}

	if(m_State < WRITING)
		delete[] data;

	return true;
}

GLboolean WrappedOpenGL::glUnmapBuffer(GLenum target)
{
	if(m_State >= WRITING)
	{
		RDCASSERT(m_BufferRecord[BufferIdx(target)]);

		auto status = m_BufferRecord[BufferIdx(target)]->Map.status;
		
		GLboolean ret = GL_TRUE;

		switch(status)
		{
			case GLResourceRecord::Unmapped:
				RDCERR("Unmapped buffer being passed to glUnmapBuffer");
				break;
			case GLResourceRecord::Mapped_Read:
				// can ignore
				break;
			case GLResourceRecord::Mapped_Read_Real:
				// need to do real unmap
				ret = m_Real.glUnmapBuffer(target);
				break;
			case GLResourceRecord::Mapped_Write:
			{
				if(m_State == WRITING_CAPFRAME)
					RDCWARN("Failed to cap frame - uncapped Map/Unmap");
				
				SCOPED_SERIALISE_CONTEXT(UNMAP);
				Serialise_glUnmapBuffer(target);
				
				if(m_State == WRITING_CAPFRAME)
					m_ContextRecord->AddChunk(scope.Get());
				else
					m_BufferRecord[BufferIdx(target)]->AddChunk(scope.Get());
				
				break;
			}
			case GLResourceRecord::Mapped_Write_Alloc:
			{
				SCOPED_SERIALISE_CONTEXT(UNMAP);
				Serialise_glUnmapBuffer(target);
				
				if(m_State == WRITING_CAPFRAME)
				{
					m_ContextRecord->AddChunk(scope.Get());
				}
				else
				{
					Chunk *chunk = scope.Get();
					m_BufferRecord[BufferIdx(target)]->AddChunk(chunk);
					m_BufferRecord[BufferIdx(target)]->SetDataPtr(chunk->GetData());
				}

				delete[] m_BufferRecord[BufferIdx(target)]->Map.ptr;

				break;
			}
			case GLResourceRecord::Mapped_Write_Real:
			{
				// Throwing away map contents as we don't have datastore allocated
				// Could init chunk here using known data (although maybe it's only partial).
				ret = m_Real.glUnmapBuffer(target);

				GetResourceManager()->MarkDirtyResource(m_BufferRecord[BufferIdx(target)]->GetResourceID());
				break;
			}
		}

		m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Unmapped;

		return ret;
	}

	return m_Real.glUnmapBuffer(target);
}

bool WrappedOpenGL::Serialise_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint8_t, Norm, normalized);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)pointer);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glVertexAttribPointer(Index, Size, Type, Norm, Stride, (const void *)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribPointer(index, size, type, normalized, stride, pointer);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	if(m_State >= WRITING)
	{
		RDCASSERT(r);

		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
		Serialise_glVertexAttribPointer(index, size, type, normalized, stride, pointer);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)pointer);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glVertexAttribIPointer(Index, Size, Type, Stride, (const void *)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribIPointer(index, size, type, stride, pointer);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	if(m_State >= WRITING)
	{
		RDCASSERT(r);

		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIPOINTER);
		Serialise_glVertexAttribIPointer(index, size, type, stride, pointer);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEnableVertexAttribArray(GLuint index)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glEnableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glEnableVertexAttribArray(GLuint index)
{
	m_Real.glEnableVertexAttribArray(index);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	if(m_State >= WRITING)
	{
		RDCASSERT(r);

		SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
		Serialise_glEnableVertexAttribArray(index);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDisableVertexAttribArray(GLuint index)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glDisableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glDisableVertexAttribArray(GLuint index)
{
	m_Real.glDisableVertexAttribArray(index);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	RDCASSERT(r);
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(DISABLEVERTEXATTRIBARRAY);
		Serialise_glDisableVertexAttribArray(index);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glGenVertexArrays(GLsizei n, GLuint* arrays)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(VertexArrayRes(*arrays)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenVertexArrays(1, &real);
		
		GLResource res = VertexArrayRes(real);

		m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenVertexArrays(GLsizei n, GLuint *arrays)
{
	m_Real.glGenVertexArrays(n, arrays);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = VertexArrayRes(arrays[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_VERTEXARRAY);
				Serialise_glGenVertexArrays(1, arrays+i);

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

bool WrappedOpenGL::Serialise_glBindVertexArray(GLuint array)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(VertexArrayRes(array)));

	if(m_State <= EXECUTING)
	{
		m_Real.glBindVertexArray(GetResourceManager()->GetLiveResource(id).name);
	}

	return true;
}

void WrappedOpenGL::glBindVertexArray(GLuint array)
{
	m_Real.glBindVertexArray(array);

	if(m_State >= WRITING)
	{
		if(array == 0)
		{
			m_VertexArrayRecord = NULL;
		}
		else
		{
			m_VertexArrayRecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(array));
		}
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BINDVERTEXARRAY);
		Serialise_glBindVertexArray(array);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	m_Real.glDeleteBuffers(n, buffers);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(BufferRes(buffers[i]));
}

void WrappedOpenGL::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
	m_Real.glDeleteVertexArrays(n, arrays);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(VertexArrayRes(arrays[i]));
}
