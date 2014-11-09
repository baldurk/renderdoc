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

#pragma region Buffers

bool WrappedOpenGL::Serialise_glGenBuffers(GLsizei n, GLuint* textures)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), *textures)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenBuffers(1, &real);
		
		GLResource res = BufferRes(GetCtx(), real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);

		m_Buffers[live].resource = res;
		m_Buffers[live].curType = eGL_NONE;
	}

	return true;
}

void WrappedOpenGL::glGenBuffers(GLsizei n, GLuint *buffers)
{
	m_Real.glGenBuffers(n, buffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = BufferRes(GetCtx(), buffers[i]);
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

bool WrappedOpenGL::Serialise_glBindBuffer(GLenum target, GLuint buffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId()));
	
	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		if(buffer != 0)
			m_BufferRecord[idx]->datatype = Target;
	}
	else if(m_State < WRITING)
	{
		if(Id == ResourceId())
		{
			m_Real.glBindBuffer(Target, 0);
		}
		else
		{
			GLResource res = GetResourceManager()->GetLiveResource(Id);
			m_Real.glBindBuffer(Target, res.name);

			m_Buffers[GetResourceManager()->GetLiveID(Id)].curType = Target;
		}
	}

	return true;
}

void WrappedOpenGL::glBindBuffer(GLenum target, GLuint buffer)
{
	m_Real.glBindBuffer(target, buffer);
	
	size_t idx = BufferIdx(target);

	if(m_State == WRITING_CAPFRAME)
	{
		Chunk *chunk = NULL;
		
		if(buffer == 0)
			m_BufferRecord[idx] = NULL;
		else
			m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

		{
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
			Serialise_glBindBuffer(target, buffer);

			chunk = scope.Get();
		}
		
		m_ContextRecord->AddChunk(chunk);
	}

	if(buffer == 0)
	{
		m_BufferRecord[idx] = NULL;
		return;
	}

	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

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

bool WrappedOpenGL::Serialise_glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
	SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);

	byte *dummy = NULL;

	if(m_State >= WRITING && data == NULL)
	{
		dummy = new byte[size];
		data = dummy;
	}

	SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

	uint64_t offs = m_pSerialiser->GetOffset();

	SERIALISE_ELEMENT(uint32_t, Flags, flags);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glNamedBufferStorageEXT(res.name, (GLsizeiptr)Bytesize, bytes, Flags);
		
		m_Buffers[GetResourceManager()->GetLiveID(id)].size = Bytesize;

		SAFE_DELETE_ARRAY(bytes);
	}
	else if(m_State >= WRITING)
	{
		GetResourceManager()->GetResourceRecord(id)->SetDataOffset(offs - Bytesize);
	}

	if(dummy)
		delete[] dummy;

	return true;
}

void WrappedOpenGL::glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
	m_Real.glNamedBufferStorageEXT(buffer, size, data, flags);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(BUFFERSTORAGE);
		Serialise_glNamedBufferStorageEXT(buffer, size, data, flags);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			record->AddChunk(chunk);
			record->SetDataPtr(chunk->GetData());
			record->Length = (int32_t)size;
		}
	}
}

void WrappedOpenGL::glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
	m_Real.glBufferStorage(target, size, data, flags);
	
	size_t idx = BufferIdx(target);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(BUFFERSTORAGE);
		Serialise_glNamedBufferStorageEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																   size, data, flags);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			record->AddChunk(chunk);
			record->SetDataPtr(chunk->GetData());
			record->Length = (int32_t)size;
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
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

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glNamedBufferDataEXT(res.name, (GLsizeiptr)Bytesize, bytes, Usage);
		
		m_Buffers[GetResourceManager()->GetLiveID(id)].size = Bytesize;

		SAFE_DELETE_ARRAY(bytes);
	}
	else if(m_State >= WRITING)
	{
		GetResourceManager()->GetResourceRecord(id)->SetDataOffset(offs - Bytesize);
	}

	if(dummy)
		delete[] dummy;

	return true;
}

void WrappedOpenGL::glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
	m_Real.glNamedBufferDataEXT(buffer, size, data, usage);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		RDCASSERT(record);
		
		// detect buffer orphaning and just update backing store
		if(m_State == WRITING_IDLE && record->GetDataPtr() != NULL && size == record->Length && usage == record->usage)
		{
			if(data)
				memcpy(record->GetDataPtr(), data, (size_t)size);
			else
				memset(record->GetDataPtr(), 0xbe, (size_t)size);
			return;
		}

		SCOPED_SERIALISE_CONTEXT(BUFFERDATA);
		Serialise_glNamedBufferDataEXT(buffer, size, data, usage);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			record->AddChunk(chunk);
			record->SetDataPtr(chunk->GetData());
			record->Length = (int32_t)size;
			record->usage = usage;
		}
	}
}

void WrappedOpenGL::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	m_Real.glBufferData(target, size, data, usage);
	
	size_t idx = BufferIdx(target);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		// detect buffer orphaning and just update backing store
		if(m_State == WRITING_IDLE && record->GetDataPtr() != NULL && size == record->Length && usage == record->usage)
		{
			if(data)
				memcpy(record->GetDataPtr(), data, (size_t)size);
			else
				memset(record->GetDataPtr(), 0xbe, (size_t)size);
			return;
		}

		SCOPED_SERIALISE_CONTEXT(BUFFERDATA);
		Serialise_glNamedBufferDataEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																   size, data, usage);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			record->AddChunk(chunk);
			record->SetDataPtr(chunk->GetData());
			record->Length = (int32_t)size;
			record->usage = usage;
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);
	SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glNamedBufferSubDataEXT(res.name, (GLintptr)Offset, (GLsizeiptr)Bytesize, bytes);

		SAFE_DELETE_ARRAY(bytes);
	}

	return true;
}

void WrappedOpenGL::glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
	m_Real.glNamedBufferSubDataEXT(buffer, offset, size, data);
	
	if(m_State >= WRITING)
	{
		if(m_HighTrafficResources.find(BufferRes(GetCtx(), buffer)) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			return;

		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(BUFFERSUBDATA);
		Serialise_glNamedBufferSubDataEXT(buffer, offset, size, data);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			record->AddChunk(chunk);
			record->UpdateCount++;
				
			if(record->UpdateCount > 60)
			{
				m_HighTrafficResources.insert(BufferRes(GetCtx(), buffer));
				GetResourceManager()->MarkDirtyResource(record->GetResourceID());
			}
		}
	}
}

void WrappedOpenGL::glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
	m_Real.glBufferSubData(target, offset, size, data);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		GLResource res = GetResourceManager()->GetCurrentResource(record->GetResourceID());
		
		if(m_HighTrafficResources.find(res) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			return;

		SCOPED_SERIALISE_CONTEXT(BUFFERSUBDATA);
		Serialise_glNamedBufferSubDataEXT(res.name, offset, size, data);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(chunk);
		else
		{
			record->AddChunk(chunk);
			record->UpdateCount++;
				
			if(record->UpdateCount > 60)
			{
				m_HighTrafficResources.insert(res);
				GetResourceManager()->MarkDirtyResource(record->GetResourceID());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
	SERIALISE_ELEMENT(ResourceId, readid, GetResourceManager()->GetID(BufferRes(GetCtx(), readBuffer)));
	SERIALISE_ELEMENT(ResourceId, writeid, GetResourceManager()->GetID(BufferRes(GetCtx(), writeBuffer)));
	SERIALISE_ELEMENT(uint64_t, ReadOffset, (uint64_t)readOffset);
	SERIALISE_ELEMENT(uint64_t, WriteOffset, (uint64_t)writeOffset);
	SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);
	
	if(m_State < WRITING)
	{
		GLResource readres = GetResourceManager()->GetLiveResource(readid);
		GLResource writeres = GetResourceManager()->GetLiveResource(writeid);
		m_Real.glNamedCopyBufferSubDataEXT(readres.name, writeres.name, (GLintptr)ReadOffset, (GLintptr)WriteOffset, (GLsizeiptr)Bytesize);
	}

	return true;
}

void WrappedOpenGL::glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
	m_Real.glNamedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *readrecord = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), readBuffer));
		GLResourceRecord *writerecord = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), writeBuffer));
		RDCASSERT(readrecord && writerecord);

		SCOPED_SERIALISE_CONTEXT(COPYBUFFERSUBDATA);
		Serialise_glNamedCopyBufferSubDataEXT(readBuffer, writeBuffer, readOffset, writeOffset, size);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			writerecord->AddChunk(chunk);
			writerecord->AddParent(readrecord);
		}
	}
}

void WrappedOpenGL::glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
	m_Real.glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *readrecord = m_BufferRecord[BufferIdx(readTarget)];
		GLResourceRecord *writerecord = m_BufferRecord[BufferIdx(writeTarget)];
		RDCASSERT(readrecord && writerecord);

		SCOPED_SERIALISE_CONTEXT(COPYBUFFERSUBDATA);
		Serialise_glNamedCopyBufferSubDataEXT(GetResourceManager()->GetCurrentResource(readrecord->GetResourceID()).name,
																          GetResourceManager()->GetCurrentResource(writerecord->GetResourceID()).name,
																          readOffset, writeOffset, size);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			writerecord->AddChunk(chunk);
			writerecord->AddParent(readrecord);
		}
	}
}

bool WrappedOpenGL::Serialise_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId()));

	if(m_State < WRITING)
	{
		if(id == ResourceId())
		{
			m_Real.glBindBuffer(Target, 0);
		}
		else
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindBufferBase(Target, Index, res.name);
		}
	}

	return true;
}

void WrappedOpenGL::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		if(buffer == 0)
			m_BufferRecord[idx] = NULL;
		else
			m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
	}

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
	SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId()));
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);

	if(m_State < WRITING)
	{
		if(id == ResourceId())
		{
			m_Real.glBindBuffer(Target, 0);
		}
		else
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindBufferRange(Target, Index, res.name, (GLintptr)Offset, (GLsizeiptr)Size);
		}
	}

	return true;
}

void WrappedOpenGL::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		if(buffer == 0)
			m_BufferRecord[idx] = NULL;
		else
			m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_RANGE);
		Serialise_glBindBufferRange(target, index, buffer, offset, size);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glBindBufferRange(target, index, buffer, offset, size);
}

#pragma endregion

#pragma region Mapping

void *WrappedOpenGL::glMapNamedBufferEXT(GLuint buffer, GLenum access)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

		GLint length;
		m_Real.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, &length);
		
		bool straightUp = false;
		if(m_HighTrafficResources.find(BufferRes(GetCtx(), buffer)) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			straightUp = true;
		
		if(GetResourceManager()->IsResourceDirty(record->GetResourceID()) && m_State != WRITING_CAPFRAME)
			straightUp = true;

		if(!straightUp && (access == eGL_WRITE_ONLY || access == eGL_READ_WRITE) && m_State != WRITING_CAPFRAME)
		{
			straightUp = true;
			m_HighTrafficResources.insert(BufferRes(GetCtx(), buffer));
			if(m_State != WRITING_CAPFRAME)
				GetResourceManager()->MarkDirtyResource(record->GetResourceID());
		}
		
		record->Map.offset = 0;
		record->Map.length = length;
		record->Map.invalidate = false;
		     if(access == eGL_READ_ONLY)  record->Map.access = GL_MAP_READ_BIT;
		else if(access == eGL_WRITE_ONLY) record->Map.access = GL_MAP_WRITE_BIT;
		else if(access == eGL_READ_WRITE) record->Map.access = GL_MAP_READ_BIT|GL_MAP_WRITE_BIT;

		if(straightUp && m_State == WRITING_IDLE)
		{
			record->Map.ptr = (byte *)m_Real.glMapNamedBufferEXT(buffer, access);
			record->Map.status = GLResourceRecord::Mapped_Ignore_Real;

			return record->Map.ptr;
		}

		// TODO align return pointer to GL_MIN_MAP_BUFFER_ALIGNMENT (min 64)

		if(access == eGL_READ_ONLY)
		{
			record->Map.status = GLResourceRecord::Mapped_Read_Real;
			return m_Real.glMapNamedBufferEXT(buffer, access);
		}

		byte *ptr = record->GetDataPtr();

		if(ptr == NULL)
		{
			RDCWARN("Mapping buffer that hasn't been allocated");
			
			ptr = (byte *)m_Real.glMapNamedBufferEXT(buffer, access);

			record->Map.ptr = ptr;
			record->Map.status = GLResourceRecord::Mapped_Write_Real;
		}
		else
		{
			if(m_State == WRITING_CAPFRAME)
			{
				byte *shadow = (byte *)record->GetShadowPtr(0);

				if(shadow == NULL)
				{
					record->AllocShadowStorage(length);
					shadow = (byte *)record->GetShadowPtr(0);

					if(GetResourceManager()->IsResourceDirty(record->GetResourceID()))
					{
						// TODO get contents from frame initial state

						m_Real.glGetNamedBufferSubDataEXT(buffer, 0, length, ptr);
						memcpy(shadow, ptr, length);
					}
					else
					{
						memcpy(shadow, ptr, length);
					}

					memcpy(record->GetShadowPtr(1), shadow, length);
				}

				record->Map.ptr = ptr = shadow;
				record->Map.status = GLResourceRecord::Mapped_Write;
			}
			else
			{
				record->Map.ptr = ptr;
				record->Map.status = GLResourceRecord::Mapped_Write;

				record->UpdateCount++;
				
				if(record->UpdateCount > 60)
					m_HighTrafficResources.insert(BufferRes(GetCtx(), buffer));
			}
		}

		m_Real.glGetNamedBufferSubDataEXT(buffer, 0, length, ptr);

		return ptr;
	}

	return m_Real.glMapNamedBufferEXT(buffer, access);
}

void *WrappedOpenGL::glMapBuffer(GLenum target, GLenum access)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_BufferRecord[BufferIdx(target)];
		RDCASSERT(m_BufferRecord[BufferIdx(target)]);

		if(record)
			return glMapNamedBufferEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name, access);

		RDCERR("glMapBuffer: Couldn't get resource record for target %x - no buffer bound?", target);
	}

	return m_Real.glMapBuffer(target, access);
}

void *WrappedOpenGL::glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

		bool straightUp = false;
		if(m_HighTrafficResources.find(BufferRes(GetCtx(), buffer)) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			straightUp = true;
		
		if(GetResourceManager()->IsResourceDirty(record->GetResourceID()) && m_State != WRITING_CAPFRAME)
			straightUp = true;

		bool invalidateMap = (access & (GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_INVALIDATE_RANGE_BIT)) != 0;
		
		if(!straightUp && !invalidateMap && (access & GL_MAP_WRITE_BIT) && m_State != WRITING_CAPFRAME)
		{
			straightUp = true;
			m_HighTrafficResources.insert(BufferRes(GetCtx(), buffer));
			if(m_State != WRITING_CAPFRAME)
				GetResourceManager()->MarkDirtyResource(record->GetResourceID());
		}
		
		record->Map.offset = offset;
		record->Map.length = length;
		record->Map.access = access;
		record->Map.invalidate = invalidateMap;

		if(straightUp && m_State == WRITING_IDLE)
		{
			record->Map.ptr = (byte *)m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);
			record->Map.status = GLResourceRecord::Mapped_Ignore_Real;

			return record->Map.ptr;
		}

		// TODO align return pointer to GL_MIN_MAP_BUFFER_ALIGNMENT (min 64)

		if((access & (GL_MAP_COHERENT_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_FLUSH_EXPLICIT_BIT|GL_MAP_UNSYNCHRONIZED_BIT)) != 0)
			RDCUNIMPLEMENTED("haven't implemented coherent/persistant/flush explicit/unsynchronized glMap calls");

		if((access & GL_MAP_READ_BIT) != 0)
		{
			byte *ptr = record->GetDataPtr();

			if(ptr == NULL)
			{
				RDCWARN("Mapping buffer that hasn't been allocated");

				record->Map.status = GLResourceRecord::Mapped_Read_Real;
				return m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);
			}

			ptr += offset;

			m_Real.glGetNamedBufferSubDataEXT(buffer, offset, length, ptr);
			
			record->Map.status = GLResourceRecord::Mapped_Read;

			return ptr;
		}
		
		byte *ptr = record->GetDataPtr();

		if(ptr == NULL)
		{
			RDCWARN("Mapping buffer that hasn't been allocated");
			
			ptr = (byte *)m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);

			record->Map.ptr = ptr;
			record->Map.status = GLResourceRecord::Mapped_Write_Real;
		}
		else
		{
			if(m_State == WRITING_CAPFRAME)
			{
				byte *shadow = (byte *)record->GetShadowPtr(0);

				if(shadow == NULL)
				{
					GLint buflength;
					m_Real.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, &buflength);

					record->AllocShadowStorage(buflength);
					shadow = (byte *)record->GetShadowPtr(0);

					if(!invalidateMap)
					{
						if(GetResourceManager()->IsResourceDirty(record->GetResourceID()))
						{
							// TODO get contents from frame initial state
							
							m_Real.glGetNamedBufferSubDataEXT(buffer, 0, buflength, ptr);
							memcpy(shadow, ptr, buflength);
			
						}
						else
						{
							memcpy(shadow+offset, ptr+offset, length);
						}
					}

					memcpy(record->GetShadowPtr(1), shadow, buflength);
				}

				if(invalidateMap)
				{
					memset(shadow+offset, 0xcc, length);
					memset(record->GetShadowPtr(1)+offset, 0xcc, length);
				}

				record->Map.ptr = ptr = shadow;
				record->Map.status = GLResourceRecord::Mapped_Write;
			}
			else
			{
				ptr += offset;
				
				record->Map.ptr = ptr;
				record->Map.status = GLResourceRecord::Mapped_Write;

				record->UpdateCount++;
				
				if(record->UpdateCount > 60)
					m_HighTrafficResources.insert(BufferRes(GetCtx(), buffer));
			}
		}

		return ptr;
	}

	return m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);
}

void *WrappedOpenGL::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_BufferRecord[BufferIdx(target)];
		RDCASSERT(m_BufferRecord[BufferIdx(target)]);

		if(record)
			return glMapNamedBufferRangeEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name, offset, length, access);

		RDCERR("glMapBufferRange: Couldn't get resource record for target %x - no buffer bound?", target);
	}

	return m_Real.glMapBufferRange(target, offset, length, access);
}

bool WrappedOpenGL::Serialise_glUnmapNamedBufferEXT(GLuint buffer)
{
	GLResourceRecord *record = NULL;

	if(m_State >= WRITING)
		record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

	ResourceId bufID;
	uint64_t offs = 0;
	uint64_t len = 0;

	if(m_State < WRITING || m_State == WRITING_CAPFRAME)
	{
		SERIALISE_ELEMENT(ResourceId, ID, record->GetResourceID());
		SERIALISE_ELEMENT(uint64_t, offset, record->Map.offset);
		SERIALISE_ELEMENT(uint64_t, length, record->Map.length);

		bufID = ID;
		offs = offset;
		len = length;
	}
	else if(m_State == WRITING_IDLE)
	{
		bufID = record->GetResourceID();
		offs = record->Map.offset;
		len = record->Map.length;
		
		void *ptr = m_Real.glMapNamedBufferRangeEXT(buffer, (GLintptr)offs, (GLsizeiptr)len, GL_MAP_WRITE_BIT);
		memcpy(ptr, record->Map.ptr, (size_t)len);
		m_Real.glUnmapNamedBufferEXT(buffer);
		return true;
	}

	uint64_t bufBindStart = 0;

	size_t diffStart = 0;
	size_t diffEnd = (size_t)len;

	if(m_State == WRITING_CAPFRAME && len > 512 && !record->Map.invalidate)
	{
		bool found = FindDiffRange(record->Map.ptr, record->GetShadowPtr(1), (size_t)len, diffStart, diffEnd);
		if(found)
		{
			static size_t saved = 0;

			saved += (size_t)len - (diffEnd-diffStart);

			RDCDEBUG("Mapped resource size %u, difference: %u -> %u. Total bytes saved so far: %u",
				(uint32_t)len, (uint32_t)diffStart, (uint32_t)diffEnd, (uint32_t)saved);

			len = diffEnd-diffStart;
		}
		else
		{
			diffStart = 0;
			diffEnd = 0;

			len = 1;
		}
	}
	
	if(m_State == WRITING_CAPFRAME && record->GetShadowPtr(1))
	{
		memcpy(record->GetShadowPtr(1)+diffStart, record->Map.ptr+diffStart, diffEnd-diffStart);
	}
		
	SERIALISE_ELEMENT(uint32_t, DiffStart, (uint32_t)diffStart);
	SERIALISE_ELEMENT(uint32_t, DiffEnd, (uint32_t)diffEnd);

	SERIALISE_ELEMENT_BUF(byte *, data, record->Map.ptr+diffStart, (size_t)len);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(bufID);
		buffer = res.name;
	}

	if(DiffEnd > DiffStart)
	{
		void *ptr = m_Real.glMapNamedBufferRangeEXT(buffer, (GLintptr)(offs+DiffStart), GLsizeiptr(DiffEnd-DiffStart), GL_MAP_WRITE_BIT);
		memcpy(ptr, data, size_t(DiffEnd-DiffStart));
		m_Real.glUnmapNamedBufferEXT(buffer);
	}

	if(m_State < WRITING)
		delete[] data;

	return true;
}

GLboolean WrappedOpenGL::glUnmapNamedBufferEXT(GLuint buffer)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		auto status = record->Map.status;
		
		GLboolean ret = GL_TRUE;

		switch(status)
		{
			case GLResourceRecord::Unmapped:
				RDCERR("Unmapped buffer being passed to glUnmapBuffer");
				break;
			case GLResourceRecord::Mapped_Read:
				// can ignore
				break;
			case GLResourceRecord::Mapped_Ignore_Real:
				if(m_State == WRITING_CAPFRAME)
					RDCWARN("Failed to cap frame - uncapped Map/Unmap");
				// deliberate fallthrough
			case GLResourceRecord::Mapped_Read_Real:
				// need to do real unmap
				ret = m_Real.glUnmapNamedBufferEXT(buffer);
				break;
			case GLResourceRecord::Mapped_Write:
			{
				if(m_State == WRITING_CAPFRAME)
				{
					SCOPED_SERIALISE_CONTEXT(UNMAP);
					Serialise_glUnmapNamedBufferEXT(buffer);
					m_ContextRecord->AddChunk(scope.Get());
				}
				else
					Serialise_glUnmapNamedBufferEXT(buffer);
				
				break;
			}
			case GLResourceRecord::Mapped_Write_Real:
			{
				// Throwing away map contents as we don't have datastore allocated
				// Could init chunk here using known data (although maybe it's only partial).
				ret = m_Real.glUnmapNamedBufferEXT(buffer);

				GetResourceManager()->MarkDirtyResource(record->GetResourceID());
				break;
			}
		}

		record->Map.status = GLResourceRecord::Unmapped;

		return ret;
	}
	
	return m_Real.glUnmapNamedBufferEXT(buffer);
}

GLboolean WrappedOpenGL::glUnmapBuffer(GLenum target)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_BufferRecord[BufferIdx(target)];
		RDCASSERT(m_BufferRecord[BufferIdx(target)]);

		if(record)
			return glUnmapNamedBufferEXT( GetResourceManager()->GetCurrentResource(record->GetResourceID()).name );

		RDCERR("glUnmapBuffer: Couldn't get resource record for target %x - no buffer bound?", target);
	}

	return m_Real.glUnmapBuffer(target);
}

void WrappedOpenGL::glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
	m_Real.glFlushMappedBufferRange(target, offset, length);

	if(m_State >= WRITING)
	{
		RDCUNIMPLEMENTED("Persistant/unsynchronised/explicit flash maps are not yet supported");
	}
}

#pragma endregion

#pragma region Vertex Arrays

bool WrappedOpenGL::VertexArrayUpdateCheck()
{
	// if no VAO is bound, nothing to check
	if(m_VertexArrayRecord == NULL) return true;

	// if we've already stopped tracking this VAO, return as such
	if(m_VertexArrayRecord && m_VertexArrayRecord->UpdateCount > 64)
		return false;

	// increase update count
	m_VertexArrayRecord->UpdateCount++;

	// if update count is high, mark as dirty
	if(m_VertexArrayRecord && m_VertexArrayRecord->UpdateCount > 64)
	{
		GetResourceManager()->MarkDirtyResource( m_VertexArrayRecord->GetResourceID() );

		return false;
	}

	return true;
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
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
				Serialise_glVertexAttribPointer(index, size, type, normalized, stride, pointer);

				r->AddChunk(scope.Get());
			}
		}
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
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIPOINTER);
				Serialise_glVertexAttribIPointer(index, size, type, stride, pointer);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
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

		m_Real.glVertexAttribLPointer(Index, Size, Type, Stride, (const void *)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribLPointer(index, size, type, stride, pointer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBLPOINTER);
				Serialise_glVertexAttribLPointer(index, size, type, stride, pointer);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
	SERIALISE_ELEMENT(uint32_t, aidx, attribindex);
	SERIALISE_ELEMENT(uint32_t, bidx, bindingindex);
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

		m_Real.glVertexAttribBinding(aidx, bidx);
	}
	return true;
}

void WrappedOpenGL::glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
	m_Real.glVertexAttribBinding(attribindex, bindingindex);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBBINDING);
				Serialise_glVertexAttribBinding(attribindex, bindingindex);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
	SERIALISE_ELEMENT(uint32_t, Index, attribindex);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(bool, Norm, normalized ? true : false);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
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

		m_Real.glVertexAttribFormat(Index, Size, Type, Norm, Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
	m_Real.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBFORMAT);
				Serialise_glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	SERIALISE_ELEMENT(uint32_t, Index, attribindex);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
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

		m_Real.glVertexAttribIFormat(Index, Size, Type, Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	m_Real.glVertexAttribIFormat(attribindex, size, type, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIFORMAT);
				Serialise_glVertexAttribIFormat(attribindex, size, type, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	SERIALISE_ELEMENT(uint32_t, Index, attribindex);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
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

		m_Real.glVertexAttribLFormat(Index, Size, Type, Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	m_Real.glVertexAttribLFormat(attribindex, size, type, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBLFORMAT);
				Serialise_glVertexAttribLFormat(attribindex, size, type, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexAttribDivisor(GLuint index, GLuint divisor)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(uint32_t, Divisor, divisor);
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

		m_Real.glVertexAttribDivisor(Index, Divisor);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribDivisor(GLuint index, GLuint divisor)
{
	m_Real.glVertexAttribDivisor(index, divisor);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBDIVISOR);
				Serialise_glVertexAttribDivisor(index, divisor);

				r->AddChunk(scope.Get());
			}
		}
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
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
				Serialise_glEnableVertexAttribArray(index);

				r->AddChunk(scope.Get());
			}
		}
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
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : m_VertexArrayRecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !VertexArrayUpdateCheck())
				return;
			if(m_State == WRITING_CAPFRAME && m_VertexArrayRecord)
				GetResourceManager()->MarkResourceFrameReferenced(m_VertexArrayRecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(DISABLEVERTEXATTRIBARRAY);
				Serialise_glDisableVertexAttribArray(index);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glGenVertexArrays(GLsizei n, GLuint* arrays)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(VertexArrayRes(GetCtx(), *arrays)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenVertexArrays(1, &real);
		
		GLResource res = VertexArrayRes(GetCtx(), real);

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
		GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
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
	SERIALISE_ELEMENT(ResourceId, id, (array ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), array)) : ResourceId()));

	if(m_State <= EXECUTING)
	{
		if(id == ResourceId())
		{
			m_Real.glBindVertexArray(0);
		}
		else
		{
			GLuint live = GetResourceManager()->GetLiveResource(id).name;
			m_Real.glBindVertexArray(live);
		}
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
			m_VertexArrayRecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), array));
		}
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BINDVERTEXARRAY);
		Serialise_glBindVertexArray(array);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	SERIALISE_ELEMENT(uint32_t, idx, bindingindex);
	SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId()));
	SERIALISE_ELEMENT(uint64_t, offs, offset);
	SERIALISE_ELEMENT(uint64_t, str, stride);

	if(m_State <= EXECUTING)
	{
		GLuint live = 0;
		if(id != ResourceId())
			live = GetResourceManager()->GetLiveResource(id).name;

		m_Real.glBindVertexBuffer(idx, live, (GLintptr)offs, (GLsizei)str);
	}

	return true;
}

void WrappedOpenGL::glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	m_Real.glBindVertexBuffer(bindingindex, buffer, offset, stride);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BINDVERTEXBUFFER);
		Serialise_glBindVertexBuffer(bindingindex, buffer, offset, stride);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
	SERIALISE_ELEMENT(uint32_t, idx, bindingindex);
	SERIALISE_ELEMENT(uint32_t, d, divisor);

	if(m_State <= EXECUTING)
		m_Real.glVertexBindingDivisor(idx, d);

	return true;
}

void WrappedOpenGL::glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
	m_Real.glVertexBindingDivisor(bindingindex, divisor);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(VERTEXBINDINGDIVISOR);
		Serialise_glVertexBindingDivisor(bindingindex, divisor);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	m_Real.glDeleteBuffers(n, buffers);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(BufferRes(GetCtx(), buffers[i]));
}

void WrappedOpenGL::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
	m_Real.glDeleteVertexArrays(n, arrays);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(VertexArrayRes(GetCtx(), arrays[i]));
}

#pragma endregion

#pragma region Horrible glVertexAttrib variants

bool WrappedOpenGL::Serialise_glVertexAttrib(GLuint index, int count, GLenum type, GLboolean normalized, const void *value, int attribtype)
{
	SERIALISE_ELEMENT(uint32_t, idx, index);
	SERIALISE_ELEMENT(int32_t, Count, count);
	SERIALISE_ELEMENT(int, Type, attribtype);
	SERIALISE_ELEMENT(bool, norm, normalized == GL_TRUE);
	SERIALISE_ELEMENT(GLenum, packedType, type);

	AttribType attr = AttribType(Type & Attrib_typemask);
	
	size_t elemSize = 1;
	switch(attr)
	{
			case Attrib_GLdouble:
				elemSize = 8;
				break;
			case Attrib_GLfloat:
			case Attrib_GLint:
			case Attrib_GLuint:
			case Attrib_packed:
				elemSize = 4;
				break;
			case Attrib_GLshort:
			case Attrib_GLushort:
				elemSize = 2;
				break;
			case Attrib_GLbyte:
			case Attrib_GLubyte:
				elemSize = 1;
				break;
	}

	size_t valueSize = elemSize*Count;
	if(Type == Attrib_packed)
		valueSize = sizeof(uint32_t);
	
	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, valueSize);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(valueSize);

		if(Type == Attrib_packed)
		{
			     if(Count == 1) m_Real.glVertexAttribP1uiv(idx, packedType, norm, (GLuint*)value);
			else if(Count == 2) m_Real.glVertexAttribP2uiv(idx, packedType, norm, (GLuint*)value);
			else if(Count == 3) m_Real.glVertexAttribP3uiv(idx, packedType, norm, (GLuint*)value);
			else if(Count == 4) m_Real.glVertexAttribP4uiv(idx, packedType, norm, (GLuint*)value);
		}
		else if(Type & Attrib_I)
		{
			if(Count == 1)
			{
				     if(attr == Attrib_GLint)  m_Real.glVertexAttribI1iv(idx, (GLint*)value);
				else if(attr == Attrib_GLuint) m_Real.glVertexAttribI1uiv(idx, (GLuint*)value);
			}
			else if(Count == 2)
			{
				     if(attr == Attrib_GLint)  m_Real.glVertexAttribI2iv(idx, (GLint*)value);
				else if(attr == Attrib_GLuint) m_Real.glVertexAttribI2uiv(idx, (GLuint*)value);
			}
			else if(Count == 2)
			{
				     if(attr == Attrib_GLint)  m_Real.glVertexAttribI3iv(idx, (GLint*)value);
				else if(attr == Attrib_GLuint) m_Real.glVertexAttribI3uiv(idx, (GLuint*)value);
			}
			else
			{
				     if(attr == Attrib_GLbyte)   m_Real.glVertexAttribI4bv(idx, (GLbyte*)value);
				else if(attr == Attrib_GLint)    m_Real.glVertexAttribI4iv(idx, (GLint*)value);
				else if(attr == Attrib_GLshort)  m_Real.glVertexAttribI4sv(idx, (GLshort*)value);
				else if(attr == Attrib_GLubyte)  m_Real.glVertexAttribI4ubv(idx, (GLubyte*)value);
				else if(attr == Attrib_GLuint)   m_Real.glVertexAttribI4uiv(idx, (GLuint*)value);
				else if(attr == Attrib_GLushort) m_Real.glVertexAttribI4usv(idx, (GLushort*)value);
			}
		}
		else if(Type & Attrib_L)
		{
			     if(Count == 1) m_Real.glVertexAttribL1dv(idx, (GLdouble*)value);
			else if(Count == 2) m_Real.glVertexAttribL2dv(idx, (GLdouble*)value);
			else if(Count == 3) m_Real.glVertexAttribL3dv(idx, (GLdouble*)value);
			else if(Count == 4) m_Real.glVertexAttribL4dv(idx, (GLdouble*)value);
		}
		else if(Type & Attrib_N)
		{
			     if(attr == Attrib_GLbyte)   m_Real.glVertexAttrib4Nbv(idx, (GLbyte*)value);
			else if(attr == Attrib_GLint)    m_Real.glVertexAttrib4Niv(idx, (GLint*)value);
			else if(attr == Attrib_GLshort)  m_Real.glVertexAttrib4Nsv(idx, (GLshort*)value);
			else if(attr == Attrib_GLubyte)  m_Real.glVertexAttrib4Nubv(idx, (GLubyte*)value);
			else if(attr == Attrib_GLuint)   m_Real.glVertexAttrib4Nuiv(idx, (GLuint*)value);
			else if(attr == Attrib_GLushort) m_Real.glVertexAttrib4Nusv(idx, (GLushort*)value);
		}
		else
		{
			if(Count == 1)
			{
				     if(attr == Attrib_GLdouble) m_Real.glVertexAttrib1dv(idx, (GLdouble*)value);
				else if(attr == Attrib_GLfloat)  m_Real.glVertexAttrib1fv(idx, (GLfloat*)value);
				else if(attr == Attrib_GLshort)  m_Real.glVertexAttrib1sv(idx, (GLshort*)value);
			}
			else if(Count == 2)
			{
				     if(attr == Attrib_GLdouble) m_Real.glVertexAttrib2dv(idx, (GLdouble*)value);
				else if(attr == Attrib_GLfloat)  m_Real.glVertexAttrib2fv(idx, (GLfloat*)value);
				else if(attr == Attrib_GLshort)  m_Real.glVertexAttrib2sv(idx, (GLshort*)value);
			}
			else if(Count == 3)
			{
				     if(attr == Attrib_GLdouble) m_Real.glVertexAttrib3dv(idx, (GLdouble*)value);
				else if(attr == Attrib_GLfloat)  m_Real.glVertexAttrib3fv(idx, (GLfloat*)value);
				else if(attr == Attrib_GLshort)  m_Real.glVertexAttrib3sv(idx, (GLshort*)value);
			}
			else
			{
				     if(attr == Attrib_GLdouble) m_Real.glVertexAttrib4dv(idx, (GLdouble*)value);
				else if(attr == Attrib_GLfloat)  m_Real.glVertexAttrib4fv(idx, (GLfloat*)value);
				else if(attr == Attrib_GLbyte)   m_Real.glVertexAttrib4bv(idx, (GLbyte*)value);
				else if(attr == Attrib_GLint)    m_Real.glVertexAttrib4iv(idx, (GLint*)value);
				else if(attr == Attrib_GLshort)  m_Real.glVertexAttrib4sv(idx, (GLshort*)value);
				else if(attr == Attrib_GLubyte)  m_Real.glVertexAttrib4ubv(idx, (GLubyte*)value);
				else if(attr == Attrib_GLuint)   m_Real.glVertexAttrib4uiv(idx, (GLuint*)value);
				else if(attr == Attrib_GLushort) m_Real.glVertexAttrib4usv(idx, (GLushort*)value);
			}
		}
	}

	return true;
}

#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype, ...) \
void WrappedOpenGL::CONCAT(glVertexAttrib, suffix)(GLuint index, __VA_ARGS__) \
{ \
	m_Real.CONCAT(glVertexAttrib, suffix)(index, ARRAYLIST); \
\
	if(m_State >= WRITING_CAPFRAME) \
	{ \
		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIB_GENERIC); \
		const paramtype vals[] = { ARRAYLIST }; \
		Serialise_glVertexAttrib(index, count, eGL_NONE, GL_FALSE, vals, TypeOr | CONCAT(Attrib_, paramtype)); \
\
		m_ContextRecord->AddChunk(scope.Get()); \
	} \
}

#define ARRAYLIST x

ATTRIB_FUNC(1, 1f,   0,         GLfloat,  GLfloat  x)
ATTRIB_FUNC(1, 1s,   0,         GLshort,  GLshort  x)
ATTRIB_FUNC(1, 1d,   0,         GLdouble, GLdouble x)
ATTRIB_FUNC(1, L1d,  Attrib_L,  GLdouble, GLdouble x)
ATTRIB_FUNC(1, I1i,  Attrib_I,  GLint,    GLint    x)
ATTRIB_FUNC(1, I1ui, Attrib_I,  GLuint,   GLuint   x)

#undef ARRAYLIST
#define ARRAYLIST x, y

ATTRIB_FUNC(2, 2f,   0,         GLfloat,  GLfloat  x, GLfloat  y)
ATTRIB_FUNC(2, 2s,   0,         GLshort,  GLshort  x, GLshort  y)
ATTRIB_FUNC(2, 2d,   0,         GLdouble, GLdouble x, GLdouble y)
ATTRIB_FUNC(2, L2d,  Attrib_L,  GLdouble, GLdouble x, GLdouble y)
ATTRIB_FUNC(2, I2i,  Attrib_I,  GLint,    GLint    x, GLint    y)
ATTRIB_FUNC(2, I2ui, Attrib_I,  GLuint,   GLuint   x, GLuint   y)

#undef ARRAYLIST
#define ARRAYLIST x, y, z

ATTRIB_FUNC(3, 3f,   0,         GLfloat,  GLfloat  x, GLfloat  y, GLfloat  z)
ATTRIB_FUNC(3, 3s,   0,         GLshort,  GLshort  x, GLshort  y, GLshort  z)
ATTRIB_FUNC(3, 3d,   0,         GLdouble, GLdouble x, GLdouble y, GLdouble z)
ATTRIB_FUNC(3, L3d,  Attrib_L,  GLdouble, GLdouble x, GLdouble y, GLdouble z)
ATTRIB_FUNC(3, I3i,  Attrib_I,  GLint,    GLint    x, GLint    y, GLint    z)
ATTRIB_FUNC(3, I3ui, Attrib_I,  GLuint,   GLuint   x, GLuint   y, GLuint   z)

#undef ARRAYLIST
#define ARRAYLIST x, y, z, w

ATTRIB_FUNC(4, 4f,   0,         GLfloat,  GLfloat  x, GLfloat  y, GLfloat  z, GLfloat  w)
ATTRIB_FUNC(4, 4s,   0,         GLshort,  GLshort  x, GLshort  y, GLshort  z, GLshort  w)
ATTRIB_FUNC(4, 4d,   0,         GLdouble, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
ATTRIB_FUNC(4, L4d,  Attrib_L,  GLdouble, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
ATTRIB_FUNC(4, I4i,  Attrib_I,  GLint,    GLint    x, GLint    y, GLint    z, GLint    w)
ATTRIB_FUNC(4, I4ui, Attrib_I,  GLuint,   GLuint   x, GLuint   y, GLuint   z, GLuint   w)
ATTRIB_FUNC(4, 4Nub, Attrib_N,  GLubyte,  GLubyte  x, GLubyte  y, GLubyte  z, GLubyte  w)

#undef ATTRIB_FUNC
#define ATTRIB_FUNC(count, suffix, TypeOr, paramtype) \
void WrappedOpenGL::CONCAT(glVertexAttrib, suffix)(GLuint index, const paramtype *value) \
{ \
	m_Real.CONCAT(glVertexAttrib, suffix)(index, value); \
\
	if(m_State >= WRITING_CAPFRAME) \
	{ \
		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIB_GENERIC); \
		Serialise_glVertexAttrib(index, count, eGL_NONE, GL_FALSE, value, TypeOr | CONCAT(Attrib_, paramtype)); \
\
		m_ContextRecord->AddChunk(scope.Get()); \
	} \
}

ATTRIB_FUNC(1, 1dv,   0,        GLdouble)
ATTRIB_FUNC(2, 2dv,   0,        GLdouble)
ATTRIB_FUNC(3, 3dv,   0,        GLdouble)
ATTRIB_FUNC(4, 4dv,   0,        GLdouble)
ATTRIB_FUNC(1, 1sv,   0,        GLshort)
ATTRIB_FUNC(2, 2sv,   0,        GLshort)
ATTRIB_FUNC(3, 3sv,   0,        GLshort)
ATTRIB_FUNC(4, 4sv,   0,        GLshort)
ATTRIB_FUNC(1, 1fv,   0,        GLfloat)
ATTRIB_FUNC(2, 2fv,   0,        GLfloat)
ATTRIB_FUNC(3, 3fv,   0,        GLfloat)
ATTRIB_FUNC(4, 4fv,   0,        GLfloat)
ATTRIB_FUNC(4, 4bv,   0,        GLbyte)
ATTRIB_FUNC(4, 4iv,   0,        GLint)
ATTRIB_FUNC(4, 4uiv,  0,        GLuint)
ATTRIB_FUNC(4, 4usv,  0,        GLushort)
ATTRIB_FUNC(4, 4ubv,  0,        GLubyte)

ATTRIB_FUNC(1, L1dv,  Attrib_L, GLdouble)
ATTRIB_FUNC(2, L2dv,  Attrib_L, GLdouble)
ATTRIB_FUNC(3, L3dv,  Attrib_L, GLdouble)
ATTRIB_FUNC(4, L4dv,  Attrib_L, GLdouble)

ATTRIB_FUNC(1, I1iv,  Attrib_I, GLint)
ATTRIB_FUNC(1, I1uiv, Attrib_I, GLuint)
ATTRIB_FUNC(2, I2iv,  Attrib_I, GLint)
ATTRIB_FUNC(2, I2uiv, Attrib_I, GLuint)
ATTRIB_FUNC(3, I3iv,  Attrib_I, GLint)
ATTRIB_FUNC(3, I3uiv, Attrib_I, GLuint)

ATTRIB_FUNC(4, I4bv,  Attrib_I, GLbyte)
ATTRIB_FUNC(4, I4iv,  Attrib_I, GLint)
ATTRIB_FUNC(4, I4sv,  Attrib_I, GLshort)
ATTRIB_FUNC(4, I4ubv, Attrib_I, GLubyte)
ATTRIB_FUNC(4, I4uiv, Attrib_I, GLuint)
ATTRIB_FUNC(4, I4usv, Attrib_I, GLushort)

ATTRIB_FUNC(4, 4Nbv,  Attrib_N, GLbyte)
ATTRIB_FUNC(4, 4Niv,  Attrib_N, GLint)
ATTRIB_FUNC(4, 4Nsv,  Attrib_N, GLshort)
ATTRIB_FUNC(4, 4Nubv, Attrib_N, GLubyte)
ATTRIB_FUNC(4, 4Nuiv, Attrib_N, GLuint)
ATTRIB_FUNC(4, 4Nusv, Attrib_N, GLushort)

#undef ATTRIB_FUNC
#define ATTRIB_FUNC(count, suffix, funcparam, passparam) \
void WrappedOpenGL::CONCAT(CONCAT(glVertexAttribP, count), suffix)(GLuint index, GLenum type, GLboolean normalized, funcparam) \
{ \
	m_Real.CONCAT(CONCAT(glVertexAttribP, count), suffix)(index, type, normalized, value); \
\
	if(m_State >= WRITING_CAPFRAME) \
	{ \
		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIB_GENERIC); \
		Serialise_glVertexAttrib(index, count, type, normalized, passparam, Attrib_packed); \
\
		m_ContextRecord->AddChunk(scope.Get()); \
	} \
}

ATTRIB_FUNC(1, ui, GLuint value, &value)
ATTRIB_FUNC(2, ui, GLuint value, &value)
ATTRIB_FUNC(3, ui, GLuint value, &value)
ATTRIB_FUNC(4, ui, GLuint value, &value)
ATTRIB_FUNC(1, uiv, const GLuint *value, value)
ATTRIB_FUNC(2, uiv, const GLuint *value, value)
ATTRIB_FUNC(3, uiv, const GLuint *value, value)
ATTRIB_FUNC(4, uiv, const GLuint *value, value)

#pragma endregion