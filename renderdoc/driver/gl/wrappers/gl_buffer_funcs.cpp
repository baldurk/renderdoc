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
#include "serialise/string_utils.h"
#include "../gl_driver.h"

#pragma region Buffers

bool WrappedOpenGL::Serialise_glGenBuffers(GLsizei n, GLuint* buffers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), *buffers)));

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
			GetCtxData().m_BufferRecord[idx]->datatype = Target;
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

	ContextData &cd = GetCtxData();
	
	size_t idx = BufferIdx(target);

	if(m_State == WRITING_CAPFRAME)
	{
		Chunk *chunk = NULL;
		
		if(buffer == 0)
			cd.m_BufferRecord[idx] = NULL;
		else
			cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

		{
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
			Serialise_glBindBuffer(target, buffer);

			chunk = scope.Get();
		}
		
		m_ContextRecord->AddChunk(chunk);
	}

	if(buffer == 0)
	{
		cd.m_BufferRecord[idx] = NULL;
		return;
	}

	if(m_State >= WRITING)
	{
		GLResourceRecord *r = cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

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
		
		// element array buffer binding is vertex array record state, record there (if we've not just stopped)
		if(m_State == WRITING_IDLE && target == eGL_ELEMENT_ARRAY_BUFFER && RecordUpdateCheck(cd.m_VertexArrayRecord))
		{
			GLuint vao = cd.m_VertexArrayRecord->Resource.name;

			// use glVertexArrayElementBuffer to ensure the vertex array is bound when we bind the
			// element buffer
			SCOPED_SERIALISE_CONTEXT(VAO_ELEMENT_BUFFER);
			Serialise_glVertexArrayElementBuffer(vao, buffer);

			cd.m_VertexArrayRecord->AddChunk(scope.Get());

			cd.m_VertexArrayRecord->AddParent(r);
		}
		
		// store as transform feedback record state
		if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER && RecordUpdateCheck(cd.m_FeedbackRecord))
		{
			GLuint feedback = cd.m_FeedbackRecord->Resource.name;

			// use glTransformFeedbackBufferBase to ensure the feedback object is bound when we bind the
			// buffer
			SCOPED_SERIALISE_CONTEXT(FEEDBACK_BUFFER_BASE);
			Serialise_glTransformFeedbackBufferBase(feedback, 0, buffer);

			cd.m_FeedbackRecord->AddChunk(scope.Get());
		}
		
		// immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
		if(m_State == WRITING_IDLE &&
			(target == eGL_TRANSFORM_FEEDBACK_BUFFER ||
			 target == eGL_SHADER_STORAGE_BUFFER ||
			 target == eGL_ATOMIC_COUNTER_BUFFER)
			)
		{
			GetResourceManager()->MarkDirtyResource(r->GetResourceID());
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
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(BUFFERSTORAGE);
		Serialise_glNamedBufferStorageEXT(record->Resource.name,
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
		
		// if we're recreating the buffer, clear the record and add new chunks. Normally
		// we would just mark this record as dirty and pick it up on the capture frame as initial
		// data, but we don't support (if it's even possible) querying out size etc.
		// we need to add only the chunks required - glGenBuffers, glBindBuffer to current target,
		// and this buffer storage. All other chunks have no effect
		if(m_State == WRITING_IDLE && (record->GetDataPtr() != NULL || (record->Length > 0 && size != record->Length)))
		{
			// we need to maintain chunk ordering, so fetch the first two chunk IDs.
			// We should have at least two by this point - glGenBuffers and whatever gave the record
			// a size before.
			RDCASSERT(record->NumChunks() >= 2);

			// remove all but the first two chunks
			while(record->NumChunks() > 2)
			{
				Chunk *c = record->GetLastChunk();
				SAFE_DELETE(c);
				record->PopChunk();
			}

			int32_t id2 = record->GetLastChunkID();
			{
				Chunk *c = record->GetLastChunk();
				SAFE_DELETE(c);
				record->PopChunk();
			}

			int32_t id1 = record->GetLastChunkID();
			{
				Chunk *c = record->GetLastChunk();
				SAFE_DELETE(c);
				record->PopChunk();
			}

			RDCASSERT(!record->HasChunks());

			// add glGenBuffers chunk
			{
				SCOPED_SERIALISE_CONTEXT(GEN_BUFFER);
				Serialise_glGenBuffers(1, &buffer);
				
				record->AddChunk(scope.Get(), id1);
			}

			// add glBindBuffer chunk
			{
				SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
				Serialise_glBindBuffer(record->datatype, buffer);
				
				record->AddChunk(scope.Get(), id2);
			}

			// we're about to add the buffer data chunk
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
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
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

		GLuint buffer = record->Resource.name;

		// if we're recreating the buffer, clear the record and add new chunks. Normally
		// we would just mark this record as dirty and pick it up on the capture frame as initial
		// data, but we don't support (if it's even possible) querying out size etc.
		// we need to add only the chunks required - glGenBuffers, glBindBuffer to current target,
		// and this buffer storage. All other chunks have no effect
		if(m_State == WRITING_IDLE && (record->GetDataPtr() != NULL || (record->Length > 0 && size != record->Length)))
		{
			// we need to maintain chunk ordering, so fetch the first two chunk IDs.
			// We should have at least two by this point - glGenBuffers and whatever gave the record
			// a size before.
			RDCASSERT(record->NumChunks() >= 2);

			// remove all but the first two chunks
			while(record->NumChunks() > 2)
			{
				Chunk *c = record->GetLastChunk();
				SAFE_DELETE(c);
				record->PopChunk();
			}

			int32_t id2 = record->GetLastChunkID();
			{
				Chunk *c = record->GetLastChunk();
				SAFE_DELETE(c);
				record->PopChunk();
			}

			int32_t id1 = record->GetLastChunkID();
			{
				Chunk *c = record->GetLastChunk();
				SAFE_DELETE(c);
				record->PopChunk();
			}

			RDCASSERT(!record->HasChunks());

			// add glGenBuffers chunk
			{
				SCOPED_SERIALISE_CONTEXT(GEN_BUFFER);
				Serialise_glGenBuffers(1, &buffer);
				
				record->AddChunk(scope.Get(), id1);
			}

			// add glBindBuffer chunk
			{
				SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
				Serialise_glBindBuffer(record->datatype, buffer);
				
				record->AddChunk(scope.Get(), id2);
			}

			// we're about to add the buffer data chunk
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
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		RDCASSERT(record);
		
		if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			return;

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
				m_HighTrafficResources.insert(record->GetResourceID());
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
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		GLResource res = record->Resource;
		
		if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
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
				m_HighTrafficResources.insert(record->GetResourceID());
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
		GLResourceRecord *readrecord = GetCtxData().m_BufferRecord[BufferIdx(readTarget)];
		GLResourceRecord *writerecord = GetCtxData().m_BufferRecord[BufferIdx(writeTarget)];
		RDCASSERT(readrecord && writerecord);

		if(m_HighTrafficResources.find(writerecord->GetResourceID()) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			return;
	
		SCOPED_SERIALISE_CONTEXT(COPYBUFFERSUBDATA);
		Serialise_glNamedCopyBufferSubDataEXT(readrecord->Resource.name,
																          writerecord->Resource.name,
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
			writerecord->UpdateCount++;

			if(writerecord->UpdateCount > 60)
			{
				m_HighTrafficResources.insert(writerecord->GetResourceID());
				GetResourceManager()->MarkDirtyResource(writerecord->GetResourceID());
			}
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
	ContextData &cd = GetCtxData();

	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		if(buffer == 0)
			cd.m_BufferRecord[idx] = NULL;
		else
			cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
	}

	// store as transform feedback record state
	if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER && RecordUpdateCheck(cd.m_FeedbackRecord))
	{
		GLuint feedback = cd.m_FeedbackRecord->Resource.name;

		// use glTransformFeedbackBufferBase to ensure the feedback object is bound when we bind the
		// buffer
		SCOPED_SERIALISE_CONTEXT(FEEDBACK_BUFFER_BASE);
		Serialise_glTransformFeedbackBufferBase(feedback, index, buffer);

		cd.m_FeedbackRecord->AddChunk(scope.Get());
	}
	
	// immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
	if(m_State == WRITING_IDLE &&
		(target == eGL_TRANSFORM_FEEDBACK_BUFFER ||
		 target == eGL_SHADER_STORAGE_BUFFER ||
		 target == eGL_ATOMIC_COUNTER_BUFFER)
		)
	{
		GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
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
	ContextData &cd = GetCtxData();

	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		if(buffer == 0)
			cd.m_BufferRecord[idx] = NULL;
		else
			cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
	}

	// store as transform feedback record state
	if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER && RecordUpdateCheck(cd.m_FeedbackRecord))
	{
		GLuint feedback = cd.m_FeedbackRecord->Resource.name;

		// use glTransformFeedbackBufferRange to ensure the feedback object is bound when we bind the
		// buffer
		SCOPED_SERIALISE_CONTEXT(FEEDBACK_BUFFER_RANGE);
		Serialise_glTransformFeedbackBufferRange(feedback, index, buffer, offset, (GLsizei)size);

		cd.m_FeedbackRecord->AddChunk(scope.Get());
	}
	
	// immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
	if(m_State == WRITING_IDLE &&
		(target == eGL_TRANSFORM_FEEDBACK_BUFFER ||
		 target == eGL_SHADER_STORAGE_BUFFER ||
		 target == eGL_ATOMIC_COUNTER_BUFFER)
		)
	{
		GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_RANGE);
		Serialise_glBindBufferRange(target, index, buffer, offset, size);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glBindBufferRange(target, index, buffer, offset, size);
}

bool WrappedOpenGL::Serialise_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, First, first);
	SERIALISE_ELEMENT(int32_t, Count, count);

	GLuint *bufs = NULL;
	if(m_State <= EXECUTING) bufs = new GLuint[Count];
	
	for(int32_t i=0; i < Count; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffers[i])));
		
		if(m_State <= EXECUTING)
		{
			if(id != ResourceId())
				bufs[i] = GetResourceManager()->GetLiveResource(id).name;
			else
				bufs[i] = 0;
		}
	}

	if(m_State <= EXECUTING)
	{
		m_Real.glBindBuffersBase(Target, First, Count, bufs);

		delete[] bufs;
	}

	return true;
}

void WrappedOpenGL::glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
	m_Real.glBindBuffersBase(target, first, count, buffers);
	
	ContextData &cd = GetCtxData();

	if(m_State >= WRITING && first == 0 && count > 0)
	{
		size_t idx = BufferIdx(target);

		if(buffers[0] == 0)
			cd.m_BufferRecord[idx] = NULL;
		else
			cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[0]));
	}

	// store as transform feedback record state
	if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER && RecordUpdateCheck(cd.m_FeedbackRecord))
	{
		GLuint feedback = cd.m_FeedbackRecord->Resource.name;

		for(int i=0; i < count; i++)
		{
			// use glTransformFeedbackBufferBase to ensure the feedback object is bound when we bind the
			// buffer
			SCOPED_SERIALISE_CONTEXT(FEEDBACK_BUFFER_BASE);
			Serialise_glTransformFeedbackBufferBase(feedback, first+i, buffers[i]);

			cd.m_FeedbackRecord->AddChunk(scope.Get());
		}
	}
	
	// immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
	if(m_State == WRITING_IDLE &&
		(target == eGL_TRANSFORM_FEEDBACK_BUFFER ||
		 target == eGL_SHADER_STORAGE_BUFFER ||
		 target == eGL_ATOMIC_COUNTER_BUFFER)
		)
	{
		for(int i=0; i < count; i++)
			GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffers[i]));
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFERS_BASE);
		Serialise_glBindBuffersBase(target, first, count, buffers);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, First, first);
	SERIALISE_ELEMENT(int32_t, Count, count);
	
	GLuint *bufs = NULL;
	GLintptr *offs = NULL;
	GLsizeiptr *sz = NULL;
	
	if(m_State <= EXECUTING)
	{
		bufs = new GLuint[Count];
		offs = new GLintptr[Count];
		sz = new GLsizeiptr[Count];
	}
	
	for(int32_t i=0; i < Count; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffers[i])));
		SERIALISE_ELEMENT(uint64_t, offset, (uint64_t)offsets[i]);
		SERIALISE_ELEMENT(uint64_t, size, (uint64_t)sizes[i]);
		
		if(m_State <= EXECUTING)
		{
			if(id != ResourceId())
				bufs[i] = GetResourceManager()->GetLiveResource(id).name;
			else
				bufs[i] = 0;
			offs[i] = (GLintptr)offset;
			sz[i] = (GLsizeiptr)sizes;
		}
	}

	if(m_State <= EXECUTING)
	{
		m_Real.glBindBuffersRange(Target, First, Count, bufs, offs, sz);

		delete[] bufs;
		delete[] offs;
		delete[] sz;
	}

	return true;
}

void WrappedOpenGL::glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes)
{
	m_Real.glBindBuffersRange(target, first, count, buffers, offsets, sizes);

	ContextData &cd = GetCtxData();

	if(m_State >= WRITING && first == 0 && count > 0)
	{
		size_t idx = BufferIdx(target);

		if(buffers[0] == 0)
			cd.m_BufferRecord[idx] = NULL;
		else
			cd.m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[0]));
	}

	// store as transform feedback record state
	if(m_State == WRITING_IDLE && target == eGL_TRANSFORM_FEEDBACK_BUFFER && RecordUpdateCheck(cd.m_FeedbackRecord))
	{
		GLuint feedback = cd.m_FeedbackRecord->Resource.name;

		for(int i=0; i < count; i++)
		{
			// use glTransformFeedbackBufferRange to ensure the feedback object is bound when we bind the
			// buffer
			SCOPED_SERIALISE_CONTEXT(FEEDBACK_BUFFER_RANGE);
			Serialise_glTransformFeedbackBufferRange(feedback, first+i, buffers[i], offsets[i], (GLsizei)sizes[i]);

			cd.m_FeedbackRecord->AddChunk(scope.Get());
		}
	}
	
	// immediately consider buffers bound to transform feedbacks/SSBOs/atomic counters as dirty
	if(m_State == WRITING_IDLE &&
		(target == eGL_TRANSFORM_FEEDBACK_BUFFER ||
		 target == eGL_SHADER_STORAGE_BUFFER ||
		 target == eGL_ATOMIC_COUNTER_BUFFER)
		)
	{
		for(int i=0; i < count; i++)
			GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffers[i]));
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFERS_RANGE);
		Serialise_glBindBuffersRange(target, first, count, buffers, offsets, sizes);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glInvalidateBufferData(GLuint buffer)
{
	m_Real.glInvalidateBufferData(buffer);

	if(m_State == WRITING_IDLE)
		GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
}

void WrappedOpenGL::glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
	m_Real.glInvalidateBufferSubData(buffer, offset, length);

	if(m_State == WRITING_IDLE)
		GetResourceManager()->MarkDirtyResource(BufferRes(GetCtx(), buffer));
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
		if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			straightUp = true;
		
		if(GetResourceManager()->IsResourceDirty(record->GetResourceID()) && m_State != WRITING_CAPFRAME)
			straightUp = true;

		if(!straightUp && (access == eGL_WRITE_ONLY || access == eGL_READ_WRITE) && m_State != WRITING_CAPFRAME)
		{
			straightUp = true;
			m_HighTrafficResources.insert(record->GetResourceID());
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

		// we can only 'ignore' a buffer map if we're idle, if we're capframing
		// we must create shadow stores and intercept it. If we choose to ignore
		// this buffer map because we don't have backing store, it will probably
		// get the buffer marked as dirty (although in theory we could create a
		// backing store from the unmap chunk, provided we have mapped the whole
		// buffer.
		if(ptr == NULL && m_State == WRITING_IDLE)
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
					m_HighTrafficResources.insert(record->GetResourceID());
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
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		if(record)
			return glMapNamedBufferEXT(record->Resource.name, access);

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
		if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() && m_State != WRITING_CAPFRAME)
			straightUp = true;
		
		if(GetResourceManager()->IsResourceDirty(record->GetResourceID()) && m_State != WRITING_CAPFRAME)
			straightUp = true;

		bool invalidateMap = (access & (GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_INVALIDATE_RANGE_BIT)) != 0;
		
		if(!straightUp && !invalidateMap && (access & GL_MAP_WRITE_BIT) && m_State != WRITING_CAPFRAME)
		{
			straightUp = true;
			m_HighTrafficResources.insert(record->GetResourceID());
			if(m_State != WRITING_CAPFRAME)
				GetResourceManager()->MarkDirtyResource(record->GetResourceID());
		}

		record->Map.offset = offset;
		record->Map.length = length;
		record->Map.access = access;
		record->Map.invalidate = invalidateMap;
		
		if((access & (GL_MAP_COHERENT_BIT|GL_MAP_PERSISTENT_BIT)) != 0)
			RDCUNIMPLEMENTED("haven't implemented persistant glMap calls");

		if(straightUp && m_State == WRITING_IDLE)
		{
			record->Map.ptr = (byte *)m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);
			record->Map.status = GLResourceRecord::Mapped_Ignore_Real;

			return record->Map.ptr;
		}

		// TODO align return pointer to GL_MIN_MAP_BUFFER_ALIGNMENT (min 64)

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
		
		// we can only 'ignore' a buffer map if we're idle, if we're capframing
		// we must create shadow stores and intercept it. If we choose to ignore
		// this buffer map because we don't have backing store, it will probably
		// get the buffer marked as dirty (although in theory we could create a
		// backing store from the unmap chunk, provided we have mapped the whole
		// buffer.
		if(ptr == NULL && m_State == WRITING_IDLE)
		{
			RDCWARN("Mapping buffer that hasn't been allocated");
			
			ptr = (byte *)m_Real.glMapNamedBufferRangeEXT(buffer, offset, length, access);

			record->Map.ptr = ptr;
			record->Map.status = GLResourceRecord::Mapped_Write_Real;
		}
		else
		{
			// flush explicit maps are handled particularly:
			// if we're idle, we just return the backing pointer and treat it no differently to a normal write map,
			// as modified-but-unflushed ranges are "undefined", so we can easily just let them
			// be modified.
			// if we're capframing, we won't create a normal unmap chunk that copies over all the data with a
			// given diff range. Instead we'll create a flush chunk for every glFlushMappedBufferRange.

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
					m_HighTrafficResources.insert(record->GetResourceID());
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
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		if(record)
			return glMapNamedBufferRangeEXT(record->Resource.name, offset, length, access);

		RDCERR("glMapBufferRange: Couldn't get resource record for target %x - no buffer bound?", target);
	}

	return m_Real.glMapBufferRange(target, offset, length, access);
}

bool WrappedOpenGL::Serialise_glUnmapNamedBufferEXT(GLuint buffer)
{
	GLResourceRecord *record = NULL;

	if(m_State >= WRITING)
		record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

	SERIALISE_ELEMENT(ResourceId, bufID, record->GetResourceID());
	SERIALISE_ELEMENT(uint64_t, offs, record->Map.offset);
	SERIALISE_ELEMENT(uint64_t, len, record->Map.length);

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

	if(m_State == WRITING_IDLE)
	{
		diffStart = 0;
		diffEnd = (size_t)len;
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
					RDCERR("Failed to cap frame - uncapped Map/Unmap");
				// deliberate fallthrough
			case GLResourceRecord::Mapped_Read_Real:
				// need to do real unmap
				ret = m_Real.glUnmapNamedBufferEXT(buffer);
				break;
			case GLResourceRecord::Mapped_Write:
			{
				if(m_State == WRITING_CAPFRAME)
				{
					if(record->Map.access & GL_MAP_FLUSH_EXPLICIT_BIT)
					{
						// do nothing, any flushes that happened were handled,
						// and we won't do any other updates here or make a chunk.
					}
					else
					{
						SCOPED_SERIALISE_CONTEXT(UNMAP);
						Serialise_glUnmapNamedBufferEXT(buffer);
						m_ContextRecord->AddChunk(scope.Get());
					}
				}
				else
				{
					SCOPED_SERIALISE_CONTEXT(UNMAP);
					Serialise_glUnmapNamedBufferEXT(buffer);
					record->AddChunk(scope.Get());
				}
				
				break;
			}
			case GLResourceRecord::Mapped_Write_Real:
			{
				if(m_State == WRITING_CAPFRAME)
					RDCERR("Failed to cap frame - uncapped Map/Unmap");

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
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		if(record)
			return glUnmapNamedBufferEXT( record->Resource.name );

		RDCERR("glUnmapBuffer: Couldn't get resource record for target %x - no buffer bound?", target);
	}

	return m_Real.glUnmapBuffer(target);
}

bool WrappedOpenGL::Serialise_glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
	GLResourceRecord *record = NULL;

	if(m_State >= WRITING)
		record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));

	SERIALISE_ELEMENT(ResourceId, ID, record->GetResourceID());
	SERIALISE_ELEMENT(uint64_t, offs, offset);
	SERIALISE_ELEMENT(uint64_t, len, length);

	// serialise out the flushed chunk of the shadow pointer
	SERIALISE_ELEMENT_BUF(byte *, data, record->Map.ptr+offs, (size_t)len);

	// update the comparison buffer in case this buffer is subsequently mapped and we want to find
	// the difference region
	if(m_State == WRITING_CAPFRAME && record->GetShadowPtr(1))
	{
		memcpy(record->GetShadowPtr(1)+offs, record->Map.ptr+offs, (size_t)len);
	}

	GLResource res;

	if(m_State < WRITING)
		res = GetResourceManager()->GetLiveResource(ID);
	else
		res = GetResourceManager()->GetCurrentResource(ID);

	// perform a map of the range and copy the data, to emulate the modified region being flushed
	void *ptr = m_Real.glMapNamedBufferRangeEXT(res.name, (GLintptr)offs, (GLsizeiptr)len, GL_MAP_WRITE_BIT);
	memcpy(ptr, data, (size_t)len);
	m_Real.glUnmapNamedBufferEXT(res.name);

	if(m_State < WRITING)
		SAFE_DELETE_ARRAY(data);

	return true;
}

void WrappedOpenGL::glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
	GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
	RDCASSERT(record);
		
	// only need to pay attention to flushes when in capframe. Otherwise (see above) we
	// treat the map as a normal map, and let ALL modified regions go through, flushed or not,
	// as this is legal - modified but unflushed regions are 'undefined' so we can just say
	// that modifications applying is our undefined behaviour.

	// note that when we're idle, we only want to flush the range with GL if we've actually
	// mapped it. Otherwise the map is 'virtual' and just pointing to our backing store data
	if(m_State != WRITING_IDLE || (record && record->Map.status == GLResourceRecord::Mapped_Write_Real))
		m_Real.glFlushMappedNamedBufferRangeEXT(buffer, offset, length);

	if(m_State == WRITING_CAPFRAME)
	{
		if(record)
		{
			if(record->Map.status == GLResourceRecord::Unmapped)
			{
				RDCWARN("Unmapped buffer being flushed, ignoring");
			}
			else if(record->Map.status == GLResourceRecord::Mapped_Ignore_Real ||
			        record->Map.status == GLResourceRecord::Mapped_Write_Real)
			{
				RDCERR("Failed to cap frame - uncapped Map/Unmap being flushed");
			}
			else if(record->Map.status == GLResourceRecord::Mapped_Write)
			{
				if(offset < record->Map.offset || offset + length > record->Map.offset + record->Map.length)
				{
					RDCWARN("Flushed buffer range is outside of mapped range, clamping");
					
					// maintain the length/end boundary of the flushed range if the flushed offset
					// is below the mapped range
					if(offset < record->Map.offset)
					{
						offset += (record->Map.offset-offset);
						length -= (record->Map.offset-offset);
					}

					// clamp the length if it's beyond the mapped range.
					if(offset + length > record->Map.offset + record->Map.length)
					{
						length = (record->Map.offset + record->Map.length - offset);
					}
				}
				
				SCOPED_SERIALISE_CONTEXT(FLUSHMAP);
				Serialise_glFlushMappedNamedBufferRangeEXT(buffer, offset, length);
				m_ContextRecord->AddChunk(scope.Get());
			}
			// other statuses are reading
		}
	}
}

void WrappedOpenGL::glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);

		if(record)
			return glFlushMappedNamedBufferRangeEXT(record->Resource.name, offset, length);

		RDCERR("glFlushMappedBufferRange: Couldn't get resource record for target %x - no buffer bound?", target);
	}

	return m_Real.glFlushMappedBufferRange(target, offset, length);
}

#pragma endregion

#pragma region Transform Feedback

bool WrappedOpenGL::Serialise_glGenTransformFeedbacks(GLsizei n, GLuint* ids)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(FeedbackRes(GetCtx(), *ids)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenTransformFeedbacks(1, &real);
		m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, real);
		m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);
		
		GLResource res = FeedbackRes(GetCtx(), real);

		m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
	m_Real.glGenTransformFeedbacks(n, ids);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = FeedbackRes(GetCtx(), ids[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_FEEDBACK);
				Serialise_glGenTransformFeedbacks(1, ids+i);

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

void WrappedOpenGL::glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = FeedbackRes(GetCtx(), ids[i]);
		if(GetResourceManager()->HasCurrentResource(res))
		{
			GetResourceManager()->MarkCleanResource(res);
			if(GetResourceManager()->HasResourceRecord(res))
				GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
			GetResourceManager()->UnregisterResource(res);
		}
	}
	
	m_Real.glDeleteTransformFeedbacks(n, ids);
}

bool WrappedOpenGL::Serialise_glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
	SERIALISE_ELEMENT(uint32_t, idx, index);
	SERIALISE_ELEMENT(ResourceId, xid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), xfb)));
	SERIALISE_ELEMENT(ResourceId, bid, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
	
	if(m_State <= EXECUTING)
	{
		m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, GetResourceManager()->GetLiveResource(xid).name);

		if(bid == ResourceId())
			m_Real.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, idx, 0);
		else
			m_Real.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, idx, GetResourceManager()->GetLiveResource(bid).name);
	}
	
	return true;
}

bool WrappedOpenGL::Serialise_glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizei size)
{
	SERIALISE_ELEMENT(uint32_t, idx, index);
	SERIALISE_ELEMENT(ResourceId, xid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), xfb)));
	SERIALISE_ELEMENT(ResourceId, bid, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
	SERIALISE_ELEMENT(uint64_t, offs, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, sz, (uint64_t)size);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, GetResourceManager()->GetLiveResource(xid).name);

		if(bid == ResourceId())
			m_Real.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, idx, 0); // if we're unbinding, offset/size don't matter
		else
			m_Real.glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, idx, GetResourceManager()->GetLiveResource(bid).name, (GLintptr)offs, (GLsizei)sz);
	}

	return true;
}

bool WrappedOpenGL::Serialise_glBindTransformFeedback(GLenum target, GLuint id)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));

	if(m_State <= EXECUTING)
	{
		if(fid != ResourceId())
			m_Real.glBindTransformFeedback(Target, GetResourceManager()->GetLiveResource(fid).name);
		else
			m_Real.glBindTransformFeedback(Target, 0);
	}
	
	return true;
}

void WrappedOpenGL::glBindTransformFeedback(GLenum target, GLuint id)
{
	m_Real.glBindTransformFeedback(target, id);

	if(m_State >= WRITING)
	{
		if(id == 0)
		{
			GetCtxData().m_FeedbackRecord = NULL;
		}
		else
		{
			GetCtxData().m_FeedbackRecord = GetResourceManager()->GetResourceRecord(FeedbackRes(GetCtx(), id));
		}
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_FEEDBACK);
		Serialise_glBindTransformFeedback(target, id);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBeginTransformFeedback(GLenum primitiveMode)
{
	SERIALISE_ELEMENT(GLenum, Mode, primitiveMode);

	if(m_State <= EXECUTING)
	{
		m_Real.glBeginTransformFeedback(Mode);
		m_ActiveFeedback = true;
	}
	
	return true;
}

void WrappedOpenGL::glBeginTransformFeedback(GLenum primitiveMode)
{
	m_Real.glBeginTransformFeedback(primitiveMode);
	m_ActiveFeedback = true;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BEGIN_FEEDBACK);
		Serialise_glBeginTransformFeedback(primitiveMode);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glPauseTransformFeedback()
{
	if(m_State <= EXECUTING)
	{
		m_Real.glPauseTransformFeedback();
	}
	
	return true;
}

void WrappedOpenGL::glPauseTransformFeedback()
{
	m_Real.glPauseTransformFeedback();

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(PAUSE_FEEDBACK);
		Serialise_glPauseTransformFeedback();

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glResumeTransformFeedback()
{
	if(m_State <= EXECUTING)
	{
		m_Real.glResumeTransformFeedback();
	}
	
	return true;
}

void WrappedOpenGL::glResumeTransformFeedback()
{
	m_Real.glResumeTransformFeedback();

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(RESUME_FEEDBACK);
		Serialise_glResumeTransformFeedback();

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEndTransformFeedback()
{
	if(m_State <= EXECUTING)
	{
		m_Real.glEndTransformFeedback();
		m_ActiveFeedback = false;
	}
	
	return true;
}

void WrappedOpenGL::glEndTransformFeedback()
{
	m_Real.glEndTransformFeedback();
	m_ActiveFeedback = false;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(END_FEEDBACK);
		Serialise_glEndTransformFeedback();

		m_ContextRecord->AddChunk(scope.Get());
	}
}


#pragma endregion

#pragma region Vertex Arrays

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint8_t, Norm, normalized);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	SERIALISE_ELEMENT(ResourceId, bid, buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;
		buffer = (bid != ResourceId()) ? GetResourceManager()->GetLiveResource(bid).name : 0;

		m_Real.glVertexArrayVertexAttribOffsetEXT(vaobj, buffer, Index, Size, Type, Norm, Stride, (GLintptr)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset)
{
	m_Real.glVertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type, normalized, stride, offset);
	
	if(m_State >= WRITING)
	{
		ContextData &cd = GetCtxData();
		GLResourceRecord *bufrecord = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
				Serialise_glVertexArrayVertexAttribOffsetEXT(vaobj, buffer, index, size, type, normalized, stride, offset);

				r->AddChunk(scope.Get());
			}

			if(bufrecord) r->AddParent(bufrecord);
		}
	}
}

void WrappedOpenGL::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribPointer(index, size, type, normalized, stride, pointer);
	
	if(m_State >= WRITING)
	{
		ContextData &cd = GetCtxData();
		GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
		GLResourceRecord *varecord = cd.m_VertexArrayRecord;
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
				Serialise_glVertexArrayVertexAttribOffsetEXT(varecord->Resource.name, bufrecord->Resource.name, index, size, type, normalized, stride, (GLintptr)pointer);

				r->AddChunk(scope.Get());
			}

			if(bufrecord) r->AddParent(bufrecord);
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	SERIALISE_ELEMENT(ResourceId, bid, buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;
		buffer = (bid != ResourceId()) ? GetResourceManager()->GetLiveResource(bid).name : 0;

		m_Real.glVertexArrayVertexAttribIOffsetEXT(vaobj, buffer, Index, Size, Type, Stride, (GLintptr)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
	m_Real.glVertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset);
	
	if(m_State >= WRITING)
	{
		ContextData &cd = GetCtxData();
		GLResourceRecord *bufrecord = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIPOINTER);
				Serialise_glVertexArrayVertexAttribIOffsetEXT(vaobj, buffer, index, size, type, stride, offset);

				r->AddChunk(scope.Get());
			}

			if(bufrecord) r->AddParent(bufrecord);
		}
	}
}

void WrappedOpenGL::glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribIPointer(index, size, type, stride, pointer);
	
	if(m_State >= WRITING)
	{
		ContextData &cd = GetCtxData();
		GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
		GLResourceRecord *varecord = cd.m_VertexArrayRecord;
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIPOINTER);
				Serialise_glVertexArrayVertexAttribIOffsetEXT(varecord->Resource.name, bufrecord->Resource.name, index, size, type, stride, (GLintptr)pointer);

				r->AddChunk(scope.Get());
			}

			if(bufrecord) r->AddParent(bufrecord);
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr pointer)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)pointer);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	SERIALISE_ELEMENT(ResourceId, bid, buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;
		buffer = (bid != ResourceId()) ? GetResourceManager()->GetLiveResource(bid).name : 0;

		m_Real.glVertexArrayVertexAttribLOffsetEXT(vaobj, buffer, Index, Size, Type, Stride, (GLintptr)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr pointer)
{
	m_Real.glVertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, pointer);
	
	if(m_State >= WRITING)
	{
		ContextData &cd = GetCtxData();
		GLResourceRecord *bufrecord = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBLPOINTER);
				Serialise_glVertexArrayVertexAttribLOffsetEXT(vaobj, buffer, index, size, type, stride, pointer);

				r->AddChunk(scope.Get());
			}

			if(bufrecord) r->AddParent(bufrecord);
		}
	}
}

void WrappedOpenGL::glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribLPointer(index, size, type, stride, pointer);
	
	if(m_State >= WRITING)
	{
		ContextData &cd = GetCtxData();
		GLResourceRecord *bufrecord = cd.m_BufferRecord[BufferIdx(eGL_ARRAY_BUFFER)];
		GLResourceRecord *varecord = cd.m_VertexArrayRecord;
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBLPOINTER);
				Serialise_glVertexArrayVertexAttribLOffsetEXT(varecord->Resource.name, bufrecord->Resource.name, index, size, type, stride, (GLintptr)pointer);

				r->AddChunk(scope.Get());
			}

			if(bufrecord) r->AddParent(bufrecord);
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex, GLuint bindingindex)
{
	SERIALISE_ELEMENT(uint32_t, aidx, attribindex);
	SERIALISE_ELEMENT(uint32_t, bidx, bindingindex);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		m_Real.glVertexArrayVertexAttribBindingEXT((id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO, aidx, bidx);
	}
	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex, GLuint bindingindex)
{
	m_Real.glVertexArrayVertexAttribBindingEXT(vaobj, attribindex, bindingindex);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBBINDING);
				Serialise_glVertexArrayVertexAttribBindingEXT(varecord->Resource.name, attribindex, bindingindex);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
	m_Real.glVertexAttribBinding(attribindex, bindingindex);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBBINDING);
				Serialise_glVertexArrayVertexAttribBindingEXT(varecord->Resource.name, attribindex, bindingindex);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
	SERIALISE_ELEMENT(uint32_t, Index, attribindex);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(bool, Norm, normalized ? true : false);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;

		m_Real.glVertexArrayVertexAttribFormatEXT(vaobj, Index, Size, Type, Norm, Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
	m_Real.glVertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type, normalized, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBFORMAT);
				Serialise_glVertexArrayVertexAttribFormatEXT(vaobj, attribindex, size, type, normalized, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
	m_Real.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBFORMAT);
				Serialise_glVertexArrayVertexAttribFormatEXT(varecord->Resource.name, attribindex, size, type, normalized, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	SERIALISE_ELEMENT(uint32_t, Index, attribindex);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;

		m_Real.glVertexArrayVertexAttribIFormatEXT(vaobj, Index, Size, Type, Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	m_Real.glVertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIFORMAT);
				Serialise_glVertexArrayVertexAttribIFormatEXT(vaobj, attribindex, size, type, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	m_Real.glVertexAttribIFormat(attribindex, size, type, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBIFORMAT);
				Serialise_glVertexArrayVertexAttribIFormatEXT(varecord->Resource.name, attribindex, size, type, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	SERIALISE_ELEMENT(uint32_t, Index, attribindex);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Offset, relativeoffset);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;

		m_Real.glVertexArrayVertexAttribLFormatEXT(vaobj, Index, Size, Type, Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	m_Real.glVertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBLFORMAT);
				Serialise_glVertexArrayVertexAttribLFormatEXT(vaobj, attribindex, size, type, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
	m_Real.glVertexAttribLFormat(attribindex, size, type, relativeoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBLFORMAT);
				Serialise_glVertexArrayVertexAttribLFormatEXT(varecord->Resource.name, attribindex, size, type, relativeoffset);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(uint32_t, Divisor, divisor);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		vaobj = (id != ResourceId()) ? GetResourceManager()->GetLiveResource(id).name : m_FakeVAO;

		m_Real.glVertexArrayVertexAttribDivisorEXT(vaobj, Index, Divisor);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor)
{
	m_Real.glVertexArrayVertexAttribDivisorEXT(vaobj, index, divisor);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBDIVISOR);
				Serialise_glVertexArrayVertexAttribDivisorEXT(vaobj, index, divisor);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glVertexAttribDivisor(GLuint index, GLuint divisor)
{
	m_Real.glVertexAttribDivisor(index, divisor);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;
	
		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBDIVISOR);
				Serialise_glVertexArrayVertexAttribDivisorEXT(varecord->Resource.name, index, divisor);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		if(m_State == READING)
		{
			if(id != ResourceId())
			{
				GLResource res = GetResourceManager()->GetLiveResource(id);
				m_Real.glBindVertexArray(res.name);
			}
			else
			{
				m_Real.glBindVertexArray(m_FakeVAO);
			}
		}

		m_Real.glEnableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
	m_Real.glEnableVertexArrayAttribEXT(vaobj, index);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
				Serialise_glEnableVertexArrayAttribEXT(varecord->Resource.name, index);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glEnableVertexAttribArray(GLuint index)
{
	m_Real.glEnableVertexAttribArray(index);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
				Serialise_glEnableVertexArrayAttribEXT(varecord->Resource.name, index);

				r->AddChunk(scope.Get());
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());
	
	if(m_State < WRITING)
	{
		if(m_State == READING)
		{
			if(id != ResourceId())
			{
				GLResource res = GetResourceManager()->GetLiveResource(id);
				m_Real.glBindVertexArray(res.name);
			}
			else
			{
				m_Real.glBindVertexArray(m_FakeVAO);
			}
		}

		m_Real.glDisableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
	m_Real.glDisableVertexArrayAttribEXT(vaobj, index);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(DISABLEVERTEXATTRIBARRAY);
				Serialise_glDisableVertexArrayAttribEXT(varecord->Resource.name, index);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glDisableVertexAttribArray(GLuint index)
{
	m_Real.glDisableVertexAttribArray(index);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(DISABLEVERTEXATTRIBARRAY);
				Serialise_glDisableVertexArrayAttribEXT(varecord->Resource.name, index);

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
			m_Real.glBindVertexArray(m_FakeVAO);
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
			GetCtxData().m_VertexArrayRecord = NULL;
		}
		else
		{
			GetCtxData().m_VertexArrayRecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), array));
		}
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_VERTEXARRAY);
		Serialise_glBindVertexArray(array);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
	SERIALISE_ELEMENT(ResourceId, vid, (vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId()));
	SERIALISE_ELEMENT(ResourceId, bid, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId()));

	if(m_State <= EXECUTING)
	{
		vaobj = 0;
		if(vid != ResourceId()) vaobj = GetResourceManager()->GetLiveResource(vid).name;

		buffer = 0;
		if(bid != ResourceId()) buffer = GetResourceManager()->GetLiveResource(bid).name;

		// hack for now, since we don't properly serialise ARB_dsa, this is just used from
		// glBindBuffer as a utility serialise function
		m_Real.glBindVertexArray(vaobj);
		m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);
	}

	return true;
}

bool WrappedOpenGL::Serialise_glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	SERIALISE_ELEMENT(uint32_t, idx, bindingindex);
	SERIALISE_ELEMENT(ResourceId, id, (buffer ? GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)) : ResourceId()));
	SERIALISE_ELEMENT(uint64_t, offs, offset);
	SERIALISE_ELEMENT(uint64_t, str, stride);
	SERIALISE_ELEMENT(ResourceId, vid, vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId());

	if(m_State <= EXECUTING)
	{
		vaobj = (vid != ResourceId()) ? GetResourceManager()->GetLiveResource(vid).name : m_FakeVAO;

		GLuint live = 0;
		if(id != ResourceId())
			live = GetResourceManager()->GetLiveResource(id).name;

		m_Real.glVertexArrayBindVertexBufferEXT(vaobj, idx, live, (GLintptr)offs, (GLsizei)str);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	m_Real.glVertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride);

	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(BIND_VERTEXBUFFER);
				Serialise_glVertexArrayBindVertexBufferEXT(vaobj, bindingindex, buffer, offset, stride);

				r->AddChunk(scope.Get());
			}

			if(buffer != 0)
				r->AddParent(GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer)));
		}
	}
}

void WrappedOpenGL::glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	m_Real.glBindVertexBuffer(bindingindex, buffer, offset, stride);

	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(BIND_VERTEXBUFFER);
				Serialise_glVertexArrayBindVertexBufferEXT(varecord->Resource.name, bindingindex, buffer, offset, stride);

				r->AddChunk(scope.Get());
			}

			if(buffer != 0)
				r->AddParent(GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer)));
		}
	}
}

bool WrappedOpenGL::Serialise_glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
	SERIALISE_ELEMENT(uint32_t, First, first);
	SERIALISE_ELEMENT(int32_t, Count, count);
	SERIALISE_ELEMENT(ResourceId, vid, GetCtxData().m_VertexArrayRecord ? GetCtxData().m_VertexArrayRecord->GetResourceID() : ResourceId());

	GLuint *bufs = NULL;
	GLintptr *offs = NULL;
	GLsizei *str = NULL;
	
	if(m_State <= EXECUTING)
	{
		bufs = new GLuint[Count];
		offs = new GLintptr[Count];
		str = new GLsizei[Count];
	}
	
	for(int32_t i=0; i < Count; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffers[i])));
		SERIALISE_ELEMENT(uint64_t, offset, (uint64_t)offsets[i]);
		SERIALISE_ELEMENT(uint64_t, stride, (uint64_t)strides[i]);
		
		if(m_State <= EXECUTING)
		{
			if(id != ResourceId())
				bufs[i] = GetResourceManager()->GetLiveResource(id).name;
			else
				bufs[i] = 0;
			offs[i] = (GLintptr)offset;
			str[i] = (GLsizei)stride;
		}
	}

	if(m_State <= EXECUTING)
	{
		if(m_State == READING)
		{
			if(vid != ResourceId())
			{
				GLResource res = GetResourceManager()->GetLiveResource(vid);
				m_Real.glBindVertexArray(res.name);
			}
			else
			{
				m_Real.glBindVertexArray(m_FakeVAO);
			}
		}

		m_Real.glBindVertexBuffers(First, Count, bufs, offs, str);

		delete[] bufs;
		delete[] offs;
		delete[] str;
	}

	return true;
}

void WrappedOpenGL::glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
	m_Real.glBindVertexBuffers(first, count, buffers, offsets, strides);

	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(BIND_VERTEXBUFFERS);
				Serialise_glBindVertexBuffers(first, count, buffers, offsets, strides);

				r->AddChunk(scope.Get());
			}

			for(GLsizei i=0; i < count; i++)
			{
				if(buffers[i] != 0)
					r->AddParent(GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffers[i])));
			}
		}
	}
}

bool WrappedOpenGL::Serialise_glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
	SERIALISE_ELEMENT(uint32_t, idx, bindingindex);
	SERIALISE_ELEMENT(uint32_t, d, divisor);
	SERIALISE_ELEMENT(ResourceId, vid, (vaobj ? GetResourceManager()->GetID(VertexArrayRes(GetCtx(), vaobj)) : ResourceId()));

	if(m_State <= EXECUTING)
	{
		vaobj = (vid != ResourceId()) ? GetResourceManager()->GetLiveResource(vid).name : m_FakeVAO;

		m_Real.glVertexArrayVertexBindingDivisorEXT(vaobj, idx, d);
	}

	return true;
}

void WrappedOpenGL::glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
	m_Real.glVertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor);

	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(GetCtx(), vaobj));

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXBINDINGDIVISOR);
				Serialise_glVertexArrayVertexBindingDivisorEXT(vaobj, bindingindex, divisor);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
	m_Real.glVertexBindingDivisor(bindingindex, divisor);

	if(m_State >= WRITING)
	{
		GLResourceRecord *varecord = GetCtxData().m_VertexArrayRecord;

		GLResourceRecord *r = m_State == WRITING_CAPFRAME ? m_ContextRecord : varecord;

		if(r)
		{
			if(m_State == WRITING_IDLE && !RecordUpdateCheck(varecord))
				return;
			if(m_State == WRITING_CAPFRAME && varecord)
				GetResourceManager()->MarkResourceFrameReferenced(varecord->GetResourceID(), eFrameRef_Write);

			{
				SCOPED_SERIALISE_CONTEXT(VERTEXBINDINGDIVISOR);
				Serialise_glVertexArrayVertexBindingDivisorEXT(varecord->Resource.name, bindingindex, divisor);

				r->AddChunk(scope.Get());
			}
		}
	}
}

void WrappedOpenGL::glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = BufferRes(GetCtx(), buffers[i]);
		if(GetResourceManager()->HasCurrentResource(res))
		{
			GetResourceManager()->MarkCleanResource(res);
			if(GetResourceManager()->HasResourceRecord(res))
				GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
			GetResourceManager()->UnregisterResource(res);
		}
	}
	
	m_Real.glDeleteBuffers(n, buffers);
}

void WrappedOpenGL::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = VertexArrayRes(GetCtx(), arrays[i]);
		if(GetResourceManager()->HasCurrentResource(res))
		{
			GetResourceManager()->MarkCleanResource(res);
			if(GetResourceManager()->HasResourceRecord(res))
				GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
			GetResourceManager()->UnregisterResource(res);
		}
	}
	
	m_Real.glDeleteVertexArrays(n, arrays);
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
			default:
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