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

bool WrappedOpenGL::Serialise_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
	SERIALISE_ELEMENT(uint32_t, X, num_groups_x);
	SERIALISE_ELEMENT(uint32_t, Y, num_groups_y);
	SERIALISE_ELEMENT(uint32_t, Z, num_groups_z);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glDispatchCompute(X, Y, Z);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DISPATCH_COMPUTE, desc);
		string name = "glDispatchCompute(" +
						ToStr::Get(X) + ", " +
						ToStr::Get(Y) + ", " +
						ToStr::Get(Z) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Dispatch;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
	CoherentMapImplicitBarrier();

	m_Real.glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISPATCH_COMPUTE);
		Serialise_glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDispatchComputeGroupSizeARB(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z, GLuint group_size_x, GLuint group_size_y, GLuint group_size_z)
{
	SERIALISE_ELEMENT(uint32_t, X, num_groups_x);
	SERIALISE_ELEMENT(uint32_t, Y, num_groups_y);
	SERIALISE_ELEMENT(uint32_t, Z, num_groups_z);
	SERIALISE_ELEMENT(uint32_t, sX, group_size_x);
	SERIALISE_ELEMENT(uint32_t, sY, group_size_y);
	SERIALISE_ELEMENT(uint32_t, sZ, group_size_z);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glDispatchComputeGroupSizeARB(X, Y, Z, sX, sY, sZ);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DISPATCH_COMPUTE, desc);
		string name = "glDispatchComputeGroupSizeARB(" +
						ToStr::Get(X) + ", " +
						ToStr::Get(Y) + ", " +
						ToStr::Get(Z) + ", " +
						ToStr::Get(sX) + ", " +
						ToStr::Get(sY) + ", " +
						ToStr::Get(sZ) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Dispatch;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDispatchComputeGroupSizeARB(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z, GLuint group_size_x, GLuint group_size_y, GLuint group_size_z)
{
	CoherentMapImplicitBarrier();

	m_Real.glDispatchComputeGroupSizeARB(num_groups_x, num_groups_y, num_groups_z, group_size_x, group_size_y, group_size_z);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISPATCH_COMPUTE_GROUP_SIZE);
		Serialise_glDispatchComputeGroupSizeARB(num_groups_x, num_groups_y, num_groups_z, group_size_x, group_size_y, group_size_z);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDispatchComputeIndirect(GLintptr indirect)
{
	SERIALISE_ELEMENT(uint64_t, offs, indirect);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glDispatchComputeIndirect((GLintptr)offs);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		uint32_t groupSizes[3];
		m_Real.glGetBufferSubData(eGL_DISPATCH_INDIRECT_BUFFER, (GLintptr)offs, sizeof(uint32_t)*3, groupSizes);

		AddEvent(DISPATCH_COMPUTE_INDIRECT, desc);
		string name = "glDispatchComputeIndirect(" +
						ToStr::Get(groupSizes[0]) + ", " +
						ToStr::Get(groupSizes[1]) + ", " +
						ToStr::Get(groupSizes[2]) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Dispatch|eDraw_Indirect;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDispatchComputeIndirect(GLintptr indirect)
{
	CoherentMapImplicitBarrier();

	m_Real.glDispatchComputeIndirect(indirect);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISPATCH_COMPUTE_INDIRECT);
		Serialise_glDispatchComputeIndirect(indirect);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMemoryBarrier(GLbitfield barriers)
{
	SERIALISE_ELEMENT(uint32_t, Barriers, barriers);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glMemoryBarrier(Barriers);
	}
	
	return true;
}

void WrappedOpenGL::glMemoryBarrier(GLbitfield barriers)
{
	if(barriers & GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT)
	{
		// perform a forced flush of all persistent mapped buffers,
		// coherent or not.
		PersistentMapMemoryBarrier(m_PersistentMaps);
	}

	m_Real.glMemoryBarrier(barriers);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MEMORY_BARRIER);
		Serialise_glMemoryBarrier(barriers);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMemoryBarrierByRegion(GLbitfield barriers)
{
	SERIALISE_ELEMENT(uint32_t, Barriers, barriers);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glMemoryBarrierByRegion(Barriers);
	}
	
	return true;
}

void WrappedOpenGL::glMemoryBarrierByRegion(GLbitfield barriers)
{
	if(barriers & GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT)
	{
		// perform a forced flush of all persistent mapped buffers,
		// coherent or not.
		PersistentMapMemoryBarrier(m_PersistentMaps);
	}

	m_Real.glMemoryBarrierByRegion(barriers);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MEMORY_BARRIER_BY_REGION);
		Serialise_glMemoryBarrierByRegion(barriers);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureBarrier()
{
	if(m_State <= EXECUTING)
	{
		m_Real.glTextureBarrier();
	}
	
	return true;
}

void WrappedOpenGL::glTextureBarrier()
{
	CoherentMapImplicitBarrier();

	m_Real.glTextureBarrier();

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(TEXTURE_BARRIER);
		Serialise_glTextureBarrier();

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedback(GLenum mode, GLuint id)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawTransformFeedback(Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_FEEDBACK, desc);
		string name = "glDrawTransformFeedback(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ")";
		
		RDCUNIMPLEMENTED("Not fetching feedback object count for glDrawTransformFeedback() display");

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = 1;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawTransformFeedback(GLenum mode, GLuint id)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawTransformFeedback(mode, id);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_FEEDBACK);
		Serialise_glDrawTransformFeedback(mode, id);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));
	SERIALISE_ELEMENT(uint32_t, Count, instancecount);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawTransformFeedbackInstanced(Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name, Count);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_FEEDBACK_INSTANCED, desc);
		string name = "glDrawTransformFeedbackInstanced(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ")";
		
		RDCUNIMPLEMENTED("Not fetching feedback object count for glDrawTransformFeedbackInstanced() display");

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = 1;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawTransformFeedbackInstanced(mode, id, instancecount);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_FEEDBACK_INSTANCED);
		Serialise_glDrawTransformFeedbackInstanced(mode, id, instancecount);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));
	SERIALISE_ELEMENT(uint32_t, Stream, stream);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawTransformFeedbackStream(Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name, Stream);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_FEEDBACK_STREAM, desc);
		string name = "glDrawTransformFeedbackStream(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ")";
		
		RDCUNIMPLEMENTED("Not fetching feedback object count for glDrawTransformFeedbackStream() display");

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = 1;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawTransformFeedbackStream(mode, id, stream);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_FEEDBACK_STREAM);
		Serialise_glDrawTransformFeedbackStream(mode, id, stream);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(ResourceId, fid, GetResourceManager()->GetID(FeedbackRes(GetCtx(), id)));
	SERIALISE_ELEMENT(uint32_t, Stream, stream);
	SERIALISE_ELEMENT(uint32_t, Count, instancecount);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawTransformFeedbackStreamInstanced(Mode, fid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(fid).name, Stream, Count);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAW_FEEDBACK_STREAM_INSTANCED, desc);
		string name = "glDrawTransformFeedbackStreamInstanced(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ")";
		
		RDCUNIMPLEMENTED("Not fetching feedback object count for glDrawTransformFeedbackStreamInstanced() display");

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = 1;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_FEEDBACK_STREAM_INSTANCED);
		Serialise_glDrawTransformFeedbackStreamInstanced(mode, id, stream, instancecount);

		m_ContextRecord->AddChunk(scope.Get());
	}
}


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
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS, desc);
		string name = "glDrawArrays(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = First;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawArrays(mode, first, count);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS);
		Serialise_glDrawArrays(mode, first, count);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawArraysIndirect(GLenum mode, const void *indirect)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawArraysIndirect(Mode, (const void *)Offset);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		DrawArraysIndirectCommand params;
		m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Offset, sizeof(params), &params);
		
		AddEvent(DRAWARRAYS_INDIRECT, desc);
		string name = "glDrawArraysIndirect(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", <" +
						ToStr::Get(params.first) + ", " +
						ToStr::Get(params.count) + ", " +
						ToStr::Get(params.instanceCount) + ", " +
						ToStr::Get(params.baseInstance) + ">)";
		
		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = params.count;
		draw.numInstances = params.instanceCount;
		draw.vertexOffset = params.first;
		draw.instanceOffset = params.baseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced|eDraw_Indirect;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArraysIndirect(GLenum mode, const void *indirect)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawArraysIndirect(mode, indirect);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INDIRECT);
		Serialise_glDrawArraysIndirect(mode, indirect);

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
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS_INSTANCED, desc);
		string name = "glDrawArraysInstanced(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(InstanceCount) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = InstanceCount;
		draw.indexOffset = 0;
		draw.vertexOffset = First;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
	CoherentMapImplicitBarrier();

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
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS_INSTANCEDBASEINSTANCE, desc);
		string name = "glDrawArraysInstancedBaseInstance(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(InstanceCount) + ", " +
						ToStr::Get(BaseInstance) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = InstanceCount;
		draw.indexOffset = 0;
		draw.vertexOffset = First;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCEDBASEINSTANCE);
		Serialise_glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

byte *WrappedOpenGL::Common_preElements(GLsizei Count, GLenum Type, uint64_t &IdxOffset)
{
	GLint idxbuf = 0;
	// while writing, check to see if an index buffer is bound
	if(m_State >= WRITING)
		m_Real.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);
	
	// serialise whether we're reading indices as memory
	SERIALISE_ELEMENT(bool, IndicesFromMemory, idxbuf == 0);

	if(IndicesFromMemory)
	{
		uint32_t IdxSize =
				Type == eGL_UNSIGNED_BYTE  ? 1
			: Type == eGL_UNSIGNED_SHORT ? 2
			: /*Type == eGL_UNSIGNED_INT*/ 4;
		
		// serialise the actual data (IdxOffset is a pointer not an offset in this case)
		SERIALISE_ELEMENT_BUF(byte *, idxdata, (void *)IdxOffset, size_t(IdxSize*Count));

		if(m_State <= EXECUTING)
		{
			GLsizeiptr idxlen = GLsizeiptr(IdxSize*Count);

			// resize fake index buffer if necessary
			if(idxlen > m_FakeIdxSize)
			{
				m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);
				m_Real.glDeleteBuffers(1, &m_FakeIdxBuf);
				
				m_FakeIdxSize = idxlen;

				m_Real.glGenBuffers(1, &m_FakeIdxBuf);
				m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, m_FakeIdxBuf);
				m_Real.glNamedBufferStorageEXT(m_FakeIdxBuf, m_FakeIdxSize, NULL, GL_DYNAMIC_STORAGE_BIT);
			}
			
			// bind and update fake index buffer, to draw from the 'immediate' index data
			m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, m_FakeIdxBuf);

			m_Real.glNamedBufferSubDataEXT(m_FakeIdxBuf, 0, idxlen, idxdata);

			// Set offset to 0 - means we read data from start of our fake index buffer
			IdxOffset = 0;
		
			// we'll delete this later (only when replaying)
			return idxdata;
		}

		// can just return NULL, since we don't need to do any cleanup or deletion
	}

	return NULL;
}

void WrappedOpenGL::Common_postElements(byte *idxDelete)
{
	// unbind temporary fake index buffer we used to pass 'immediate' index data
	if(idxDelete)
	{
		m_Real.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);

		AddDebugMessage(eDbgCategory_Deprecated, eDbgSeverity_High, eDbgSource_IncorrectAPIUse,
			"Assuming GL core profile is used then specifying indices as a raw array, "
			"not as offset into element array buffer, is illegal.");

		// delete serialised data
		SAFE_DELETE_ARRAY(idxDelete);
	}
}

bool WrappedOpenGL::Serialise_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	
	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElements(Mode, Count, Type, (const void *)IdxOffset);

		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS, desc);
		string name = "glDrawElements(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawElements(mode, count, type, indices);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS);
		Serialise_glDrawElements(mode, count, type, indices);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsIndirect(Mode, Type, (const void *)Offset);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		DrawElementsIndirectCommand params;
		m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Offset, sizeof(params), &params);

		AddEvent(DRAWELEMENTS_INDIRECT, desc);
		string name = "glDrawElementsIndirect(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Type) + ", <" +
						ToStr::Get(params.count) + ", " +
						ToStr::Get(params.instanceCount) + ", " + 
						ToStr::Get(params.baseVertex) + ", " +
						ToStr::Get(params.baseInstance) + ">)";
		
		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = params.count;
		draw.numInstances = params.instanceCount;
		draw.indexOffset = params.firstIndex;
		draw.vertexOffset = params.baseVertex;
		draw.instanceOffset = params.baseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer|eDraw_Instanced|eDraw_Indirect;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawElementsIndirect(mode, type, indirect);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INDIRECT);
		Serialise_glDrawElementsIndirect(mode, type, indirect);

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
	
	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawRangeElements(Mode, Start, End, Count, Type, (const void *)IdxOffset);
		
		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWRANGEELEMENTS, desc);
		string name = "glDrawRangeElements(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawRangeElements(mode, start, end, count, type, indices);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTS);
		Serialise_glDrawRangeElements(mode, start, end, count, type, indices);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Start, start);
	SERIALISE_ELEMENT(uint32_t, End, end);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, IdxOffset, (uint64_t)indices);
	SERIALISE_ELEMENT(uint32_t, BaseVtx, basevertex);

	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawRangeElementsBaseVertex(Mode, Start, End, Count, Type, (const void *)IdxOffset, BaseVtx);

		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWRANGEELEMENTSBASEVERTEX, desc);
		string name = "glDrawRangeElementsBaseVertex(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(BaseVtx) + ")";
		
		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = BaseVtx;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWRANGEELEMENTSBASEVERTEX);
		Serialise_glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);

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
	
	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsBaseVertex(Mode, Count, Type, (const void *)IdxOffset, BaseVtx);
		
		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_BASEVERTEX, desc);
		string name = "glDrawElementsBaseVertex(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(BaseVtx) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = BaseVtx;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
	CoherentMapImplicitBarrier();

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

	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstanced(Mode, Count, Type, (const void *)IdxOffset, InstCount);

		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCED, desc);
		string name = "glDrawElementsInstanced(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
	CoherentMapImplicitBarrier();

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
	
	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstancedBaseInstance(Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseInstance);
		
		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCEDBASEINSTANCE, desc);
		string name = "glDrawElementsInstancedBaseInstance(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ", " + 
						ToStr::Get(BaseInstance) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = 0;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
	CoherentMapImplicitBarrier();

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

	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstancedBaseVertex(Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseVertex);

		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCEDBASEVERTEX, desc);
		string name = "glDrawElementsInstancedBaseVertex(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ", " + 
						ToStr::Get(BaseVertex) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = BaseVertex;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
	CoherentMapImplicitBarrier();

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
	
	byte *idxDelete = Common_preElements(Count, Type, IdxOffset);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawElementsInstancedBaseVertexBaseInstance(Mode, Count, Type, (const void *)IdxOffset, InstCount, BaseVertex, BaseInstance);
		
		Common_postElements(idxDelete);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE, desc);
		string name = "glDrawElementsInstancedBaseVertexBaseInstance(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(Type) + ", " +
						ToStr::Get(IdxOffset) + ", " +
						ToStr::Get(InstCount) + ", " + 
						ToStr::Get(BaseVertex) + ", " +
						ToStr::Get(BaseInstance) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = Count;
		draw.numInstances = InstCount;
		draw.indexOffset = uint32_t(IdxOffset)/IdxSize;
		draw.vertexOffset = BaseVertex;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
	CoherentMapImplicitBarrier();

	m_Real.glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE);
		Serialise_glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint32_t, Count, drawcount);

	SERIALISE_ELEMENT_ARR(int32_t, firstArray, first, Count);
	SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);

	if(m_State == READING)
	{
		m_Real.glMultiDrawArrays(Mode, firstArray, countArray, Count);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawArrays(Mode, firstArray, countArray, RDCMIN(Count, m_LastEventID - baseEventID + 1));
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);

			m_Real.glDrawArrays(Mode, firstArray[drawidx], countArray[drawidx]);
		}
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		string name = "glMultiDrawArrays(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + "," +
						ToStr::Get(Count) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);

		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		for(uint32_t i=0; i < Count; i++)
		{
			FetchDrawcall multidraw;
			multidraw.numIndices = countArray[i];
			multidraw.vertexOffset = firstArray[i];
		
			multidraw.name = "glMultiDrawArrays[" + ToStr::Get(i) + "](" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.vertexOffset) + ")";

			multidraw.flags |= eDraw_Drawcall;

			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
			
			AddEvent(MULTI_DRAWARRAYS, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += Count+1;
	}

	SAFE_DELETE_ARRAY(firstArray);
	SAFE_DELETE_ARRAY(countArray);

	return true;
}

void WrappedOpenGL::glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawArrays(mode, first, count, drawcount);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS);
		Serialise_glMultiDrawArrays(mode, first, count, drawcount);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Count, drawcount);
	
	SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);

	void **idxOffsArray = new void*[Count];
	size_t len = Count;

	// serialise pointer array as uint64s
	if(m_State >= WRITING)
	{
		for(uint32_t i=0; i < Count; i++)
		{
			uint64_t ptr = (uint64_t)indices[i];
			m_pSerialiser->Serialise("idxOffsArray", ptr);
		}
	}
	else
	{
		for(uint32_t i=0; i < Count; i++)
		{
			uint64_t ptr = 0;
			m_pSerialiser->Serialise("idxOffsArray", ptr);
			idxOffsArray[i] = (void *)ptr;
		}
	}

	if(m_State == READING)
	{
		m_Real.glMultiDrawElements(Mode, countArray, Type, idxOffsArray, Count);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawElements(Mode, countArray, Type, idxOffsArray, RDCMIN(Count, m_LastEventID - baseEventID + 1));
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);

			m_Real.glDrawElements(Mode, countArray[drawidx], Type, idxOffsArray[drawidx]);
		}
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();
	
	if(m_State == READING)
	{
		string name = "glMultiDrawElements(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + "," +
						ToStr::Get(Type) + "," + 
						ToStr::Get(Count) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		
		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		for(uint32_t i=0; i < Count; i++)
		{
			FetchDrawcall multidraw;
			multidraw.numIndices = countArray[i];
			multidraw.indexOffset = (uint32_t) uint64_t(idxOffsArray[i])&0xFFFFFFFF;
		
			multidraw.name = "glMultiDrawElements[" + ToStr::Get(i) + "](" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.indexOffset) + ")";

			multidraw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;
			
			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);

			AddEvent(MULTI_DRAWELEMENTS, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += Count+1;
	}

	SAFE_DELETE_ARRAY(countArray);
	SAFE_DELETE_ARRAY(idxOffsArray);

	return true;
}

void WrappedOpenGL::glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawElements(mode, count, type, indices, drawcount);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS);
		Serialise_glMultiDrawElements(mode, count, type, indices, drawcount);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint32_t, Count, drawcount);
	
	SERIALISE_ELEMENT_ARR(int32_t, countArray, count, Count);
	SERIALISE_ELEMENT_ARR(int32_t, baseArray, basevertex, Count);

	void **idxOffsArray = new void*[Count];
	size_t len = Count;

	// serialise pointer array as uint64s
	if(m_State >= WRITING)
	{
		for(uint32_t i=0; i < Count; i++)
		{
			uint64_t ptr = (uint64_t)indices[i];
			m_pSerialiser->Serialise("idxOffsArray", ptr);
		}
	}
	else
	{
		for(uint32_t i=0; i < Count; i++)
		{
			uint64_t ptr = 0;
			m_pSerialiser->Serialise("idxOffsArray", ptr);
			idxOffsArray[i] = (void *)ptr;
		}
	}

	if(m_State == READING)
	{
		m_Real.glMultiDrawElementsBaseVertex(Mode, countArray, Type, idxOffsArray, Count, baseArray);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawElementsBaseVertex(Mode, countArray, Type, idxOffsArray, RDCMIN(Count, m_LastEventID - baseEventID + 1), baseArray);
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);

			m_Real.glDrawElementsBaseVertex(Mode, countArray[drawidx], Type, idxOffsArray[drawidx], baseArray[drawidx]);
		}
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		string name = "glMultiDrawElementsBaseVertex(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + "," +
						ToStr::Get(Type) + "," + 
						ToStr::Get(Count) + ")";

		uint32_t IdxSize =
		    Type == eGL_UNSIGNED_BYTE  ? 1
		  : Type == eGL_UNSIGNED_SHORT ? 2
		  : /*Type == eGL_UNSIGNED_INT*/ 4;

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		for(uint32_t i=0; i < Count; i++)
		{
			FetchDrawcall multidraw;
			multidraw.numIndices = countArray[i];
			multidraw.indexOffset = (uint32_t) uint64_t(idxOffsArray[i])&0xFFFFFFFF;
			multidraw.vertexOffset = baseArray[i];
		
			multidraw.name = "glMultiDrawElementsBaseVertex[" + ToStr::Get(i) + "](" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.indexOffset) + ", " +
						ToStr::Get(multidraw.vertexOffset) + ")";

			multidraw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;
			
			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
			multidraw.indexByteWidth = IdxSize;

			AddEvent(MULTI_DRAWELEMENTSBASEVERTEX, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += Count+1;
	}

	SAFE_DELETE_ARRAY(countArray);
	SAFE_DELETE_ARRAY(baseArray);
	SAFE_DELETE_ARRAY(idxOffsArray);

	return true;
}

void WrappedOpenGL::glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTSBASEVERTEX);
		Serialise_glMultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
	SERIALISE_ELEMENT(uint32_t, Count, drawcount);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);

	if(m_State == READING)
	{
		m_Real.glMultiDrawArraysIndirect(Mode, (const void *)Offset, Count, Stride);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawArraysIndirect(Mode, (const void *)Offset, RDCMIN(Count, m_LastEventID - baseEventID + 1), Stride);
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);
			
			DrawArraysIndirectCommand params;
			
			GLintptr offs = (GLintptr)Offset;
			if(Stride != 0)
				offs += Stride*drawidx;
			else
				offs += sizeof(params)*drawidx;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			m_Real.glDrawArraysInstancedBaseInstance(Mode, params.first, params.count, params.instanceCount, params.baseInstance);
		}
	}
	
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		string name = "glMultiDrawArraysIndirect(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + "," +
						ToStr::Get(Count) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);

		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		GLintptr offs = (GLintptr)Offset;

		for(uint32_t i=0; i < Count; i++)
		{
			DrawArraysIndirectCommand params;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			if(Stride)
				offs += Stride;
			else
				offs += sizeof(params);

			FetchDrawcall multidraw;
			multidraw.numIndices = params.count;
			multidraw.numInstances = params.instanceCount;
			multidraw.vertexOffset = params.first;
			multidraw.instanceOffset = params.baseInstance;
		
			multidraw.name = "glMultiDrawArraysIndirect[" + ToStr::Get(i) + "](<" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.numInstances) + ", " +
						ToStr::Get(multidraw.vertexOffset) + ", " +
						ToStr::Get(multidraw.instanceOffset) + ">)";

			multidraw.flags |= eDraw_Drawcall|eDraw_Instanced|eDraw_Indirect;

			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
			
			AddEvent(MULTI_DRAWARRAYS_INDIRECT, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += Count+1;
	}

	return true;
}

void WrappedOpenGL::glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS_INDIRECT);
		Serialise_glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
	SERIALISE_ELEMENT(uint32_t, Count, drawcount);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	
	uint32_t IdxSize =
			Type == eGL_UNSIGNED_BYTE  ? 1
		: Type == eGL_UNSIGNED_SHORT ? 2
		: /*Type == eGL_UNSIGNED_INT*/ 4;
		
	if(m_State == READING)
	{
		m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset, Count, Stride);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset, RDCMIN(Count, m_LastEventID - baseEventID + 1), Stride);
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);
			
			DrawElementsIndirectCommand params;
			
			GLintptr offs = (GLintptr)Offset;
			if(Stride != 0)
				offs += Stride*drawidx;
			else
				offs += sizeof(params)*drawidx;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			m_Real.glDrawElementsInstancedBaseVertexBaseInstance(Mode, params.count, Type, (const void *)ptrdiff_t(params.firstIndex*IdxSize),
			                                                     params.instanceCount, params.baseVertex, params.baseInstance);
		}
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		string name = "glMultiDrawElementsIndirect(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + "," +
						ToStr::Get(Type) + "," + 
						ToStr::Get(Count) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		GLintptr offs = (GLintptr)Offset;

		for(uint32_t i=0; i < Count; i++)
		{
			DrawElementsIndirectCommand params;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			if(Stride)
				offs += Stride;
			else
				offs += sizeof(params);

			FetchDrawcall multidraw;
			multidraw.numIndices = params.count;
			multidraw.numInstances = params.instanceCount;
			multidraw.indexOffset = params.firstIndex;
			multidraw.vertexOffset = params.baseVertex;
			multidraw.instanceOffset = params.baseInstance;
		
			multidraw.name = "glMultiDrawElementsIndirect[" + ToStr::Get(i) + "](<" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.numInstances) + ", " +
						ToStr::Get(multidraw.indexOffset) + ", " +
						ToStr::Get(multidraw.instanceOffset) + ">)";

			multidraw.flags |= eDraw_Drawcall|eDraw_UseIBuffer|eDraw_Instanced|eDraw_Indirect;

			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
			multidraw.indexByteWidth = IdxSize;
			
			AddEvent(MULTI_DRAWELEMENTS_INDIRECT, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += Count+1;
	}

	return true;
}

void WrappedOpenGL::glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS_INDIRECT);
		Serialise_glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawArraysIndirectCountARB(GLenum mode, GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
	SERIALISE_ELEMENT(uint64_t, Count, (uint64_t)drawcount);
	SERIALISE_ELEMENT(uint32_t, MaxCount, maxdrawcount);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	
	uint32_t realdrawcount = 0;

	if(m_State < WRITING)
	{
		m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Count, sizeof(realdrawcount), &realdrawcount);

		realdrawcount = RDCMIN(MaxCount, realdrawcount);
	}

	if(m_State == READING)
	{
		m_Real.glMultiDrawArraysIndirectCountARB(Mode, (GLintptr)Offset, (GLintptr)Count, MaxCount, Stride);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawArraysIndirect(Mode, (const void *)Offset, RDCMIN(realdrawcount, m_LastEventID - baseEventID + 1), Stride);
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);
			
			DrawArraysIndirectCommand params;
			
			GLintptr offs = (GLintptr)Offset;
			if(Stride != 0)
				offs += Stride*drawidx;
			else
				offs += sizeof(params)*drawidx;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			m_Real.glDrawArraysInstancedBaseInstance(Mode, params.first, params.count, params.instanceCount, params.baseInstance);
		}
	}
	
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		string name = "glMultiDrawArraysIndirectCountARB(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + ", <" +
						ToStr::Get(realdrawcount) + "> " +
						ToStr::Get(MaxCount) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);

		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		GLintptr offs = (GLintptr)Offset;

		for(uint32_t i=0; i < realdrawcount; i++)
		{
			DrawArraysIndirectCommand params;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			if(Stride)
				offs += Stride;
			else
				offs += sizeof(params);

			FetchDrawcall multidraw;
			multidraw.numIndices = params.count;
			multidraw.numInstances = params.instanceCount;
			multidraw.vertexOffset = params.first;
			multidraw.instanceOffset = params.baseInstance;
		
			multidraw.name = "glMultiDrawArraysIndirect[" + ToStr::Get(i) + "](<" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.numInstances) + ", " +
						ToStr::Get(multidraw.vertexOffset) + ", " +
						ToStr::Get(multidraw.instanceOffset) + ">)";

			multidraw.flags |= eDraw_Drawcall|eDraw_Instanced|eDraw_Indirect;

			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
			
			AddEvent(MULTI_DRAWARRAYS_INDIRECT, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += realdrawcount+1;
	}

	return true;
}


void WrappedOpenGL::glMultiDrawArraysIndirectCountARB(GLenum mode, GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawArraysIndirectCountARB(mode, indirect, drawcount, maxdrawcount, stride);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWARRAYS_INDIRECT_COUNT);
		Serialise_glMultiDrawArraysIndirectCountARB(mode, indirect, drawcount, maxdrawcount, stride);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)indirect);
	SERIALISE_ELEMENT(uint64_t, Count, (uint64_t)drawcount);
	SERIALISE_ELEMENT(uint32_t, MaxCount, maxdrawcount);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	
	uint32_t IdxSize =
			Type == eGL_UNSIGNED_BYTE  ? 1
		: Type == eGL_UNSIGNED_SHORT ? 2
		: /*Type == eGL_UNSIGNED_INT*/ 4;
		
	uint32_t realdrawcount = 0;

	if(m_State < WRITING)
	{
		m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, (GLintptr)Count, sizeof(realdrawcount), &realdrawcount);

		realdrawcount = RDCMIN(MaxCount, realdrawcount);
	}

	if(m_State == READING)
	{
		m_Real.glMultiDrawElementsIndirectCountARB(Mode, Type, (GLintptr)Offset, (GLintptr)Count, MaxCount, Stride);
	}
	else if(m_State <= EXECUTING)
	{
		size_t i=0;
		for(; i < m_Events.size(); i++)
		{
			if(m_Events[i].eventID >= m_CurEventID)
				break;
		}

		while(i > 1 && m_Events[i-1].fileOffset == m_Events[i].fileOffset) i--;

		uint32_t baseEventID = m_Events[i].eventID;

		if(m_LastEventID < baseEventID)
		{
			// To add the multidraw, we made an event N that is the 'parent' marker, then
			// N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
			// then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
			// the first sub-draw in that range.
		}
		else if(m_FirstEventID <= baseEventID && m_LastEventID >= baseEventID)
		{
			// if we're replaying part-way into a multidraw, we can replay the first part 'easily'
			// by just reducing the Count parameter to however many we want to replay. This only
			// works if we're replaying from the first multidraw to the nth (n less than Count)
			m_Real.glMultiDrawElementsIndirect(Mode, Type, (const void *)Offset, RDCMIN(realdrawcount, m_LastEventID - baseEventID + 1), Stride);
		}
		else
		{
			// otherwise we do the 'hard' case, draw only one multidraw
			// note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
			// a single draw.
			RDCASSERT(m_LastEventID == m_FirstEventID);

			uint32_t drawidx = (m_LastEventID - baseEventID);
			
			DrawElementsIndirectCommand params;
			
			GLintptr offs = (GLintptr)Offset;
			if(Stride != 0)
				offs += Stride*drawidx;
			else
				offs += sizeof(params)*drawidx;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			m_Real.glDrawElementsInstancedBaseVertexBaseInstance(Mode, params.count, Type, (const void *)ptrdiff_t(params.firstIndex*IdxSize),
			                                                     params.instanceCount, params.baseVertex, params.baseInstance);
		}
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();
	
	if(m_State == READING)
	{
		string name = "glMultiDrawElementsIndirectCountARB(" +
						( Mode == eGL_POINTS ? "GL_POINTS" : ToStr::Get(Mode) ) + "," +
						ToStr::Get(Type) + ", <" + 
						ToStr::Get(realdrawcount) + ">, " +
						ToStr::Get(MaxCount) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_MultiDraw;

		draw.topology = MakePrimitiveTopology(m_Real, Mode);
		draw.indexByteWidth = IdxSize;
		
		AddDrawcall(draw, false);

		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		GLintptr offs = (GLintptr)Offset;

		for(uint32_t i=0; i < realdrawcount; i++)
		{
			DrawElementsIndirectCommand params;

			m_Real.glGetBufferSubData(eGL_DRAW_INDIRECT_BUFFER, offs, sizeof(params), &params);

			if(Stride)
				offs += Stride;
			else
				offs += sizeof(params);

			FetchDrawcall multidraw;
			multidraw.numIndices = params.count;
			multidraw.numInstances = params.instanceCount;
			multidraw.indexOffset = params.firstIndex;
			multidraw.vertexOffset = params.baseVertex;
			multidraw.instanceOffset = params.baseInstance;
		
			multidraw.name = "glMultiDrawElementsIndirect[" + ToStr::Get(i) + "](" +
						ToStr::Get(multidraw.numIndices) + ", " +
						ToStr::Get(multidraw.numInstances) + ", " +
						ToStr::Get(multidraw.indexOffset) + ", " +
						ToStr::Get(multidraw.instanceOffset) + ")";

			multidraw.flags |= eDraw_Drawcall|eDraw_UseIBuffer|eDraw_Instanced|eDraw_Indirect;

			multidraw.topology = MakePrimitiveTopology(m_Real, Mode);
			multidraw.indexByteWidth = IdxSize;
			
			AddEvent(MULTI_DRAWELEMENTS_INDIRECT, desc);
			AddDrawcall(multidraw, true);

			m_CurEventID++;
		}
		
		m_DrawcallStack.pop_back();
	}
	else
	{
		m_CurEventID += realdrawcount+1;
	}

	return true;
}

void WrappedOpenGL::glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
	CoherentMapImplicitBarrier();

	m_Real.glMultiDrawElementsIndirectCountARB(mode, type, indirect, drawcount, maxdrawcount, stride);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(MULTI_DRAWELEMENTS_INDIRECT_COUNT);
		Serialise_glMultiDrawElementsIndirectCountARB(mode, type, indirect, drawcount, maxdrawcount, stride);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	SERIALISE_ELEMENT(ResourceId, Id, (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)) : ResourceId()));
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, drawbuf, drawbuffer);

	if(m_State <= EXECUTING)
	{
		if(Id == ResourceId())
			framebuffer = m_FakeBB_FBO;
		else
			framebuffer = GetResourceManager()->GetLiveResource(Id).name;
	}
	
	string name;

	if(buf != eGL_DEPTH)
	{
		Vec4f v;
		if(value) v = *((Vec4f *)value);

		m_pSerialiser->Serialise<4>("value", (float *)&v.x);
		
		if(m_State == READING)
			name = "glClearBufferfv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(drawbuf) + ", " +
						ToStr::Get(v.x) + ", " +
						ToStr::Get(v.y) + ", " +
						ToStr::Get(v.z) + ", " +
						ToStr::Get(v.w) + ")";

		// use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
		// we are running without ARB_dsa support, these functions are emulated in the obvious way. This is
		// necessary since these functions can be serialised even if ARB_dsa was not used originally, and
		// we need to support this case.
		if(m_State <= EXECUTING)
			m_Real.glClearNamedFramebufferfv(framebuffer, buf, drawbuf, &v.x);
	}
	else
	{
		SERIALISE_ELEMENT(float, val, *value);
		
		if(m_State == READING)
			name = "glClearBufferfv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(drawbuf) + ", " +
						ToStr::Get(val) + ")";

		if(m_State <= EXECUTING)
			m_Real.glClearNamedFramebufferfv(framebuffer, buf, drawbuf, &val);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERF, desc);

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Clear;
		if(buf == eGL_COLOR)
			draw.flags |= eDraw_ClearColour;
		else
			draw.flags |= eDraw_ClearDepthStencil;
		
		AddDrawcall(draw, true);
		
		GLuint attachment = 0;
		GLenum attachName = buf == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf) : eGL_DEPTH_ATTACHMENT;
		GLenum type = eGL_TEXTURE;
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

		if(attachment)
		{
			if(type == eGL_TEXTURE)
				m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			else
				m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
		}
	}


	return true;
}

void WrappedOpenGL::glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERF);
		Serialise_glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearBufferfv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		GLuint framebuffer = 0;
		if(GetCtxData().m_DrawFramebufferRecord)
			framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERF);
		Serialise_glClearNamedFramebufferfv(framebuffer, buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value)
{
	SERIALISE_ELEMENT(ResourceId, Id, (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)) : ResourceId()));
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, drawbuf, drawbuffer);
	
	if(m_State <= EXECUTING)
	{
		if(Id == ResourceId())
			framebuffer = m_FakeBB_FBO;
		else
			framebuffer = GetResourceManager()->GetLiveResource(Id).name;
	}

	string name;

	if(buf != eGL_STENCIL)
	{
		int32_t v[4];
		if(value) memcpy(v, value, sizeof(v));

		m_pSerialiser->Serialise<4>("value", v);
		
		if(m_State == READING)
			name = "glClearBufferiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(drawbuf) + ", " +
						ToStr::Get(v[0]) + ", " +
						ToStr::Get(v[1]) + ", " +
						ToStr::Get(v[2]) + ", " +
						ToStr::Get(v[3]) + ")";
		
		// use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
		// we are running without ARB_dsa support, these functions are emulated in the obvious way. This is
		// necessary since these functions can be serialised even if ARB_dsa was not used originally, and
		// we need to support this case.
		if(m_State <= EXECUTING)
			m_Real.glClearNamedFramebufferiv(framebuffer, buf, drawbuf, v);
	}
	else
	{
		SERIALISE_ELEMENT(int32_t, val, *value);
		
		if(m_State == READING)
			name = "glClearBufferiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(drawbuf) + ", " +
						ToStr::Get(val) + ")";

		if(m_State <= EXECUTING)
			m_Real.glClearNamedFramebufferiv(framebuffer, buf, drawbuf, &val);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERI, desc);

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Clear;
		if(buf == eGL_COLOR)
			draw.flags |= eDraw_ClearColour;
		else
			draw.flags |= eDraw_ClearDepthStencil;
		
		AddDrawcall(draw, true);
		
		GLuint attachment = 0;
		GLenum attachName = buf == eGL_COLOR ? GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf) : eGL_STENCIL_ATTACHMENT;
		GLenum type = eGL_TEXTURE;
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

		if(attachment)
		{
			if(type == eGL_TEXTURE)
				m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			else
				m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
		}
	}


	return true;
}

void WrappedOpenGL::glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERI);
		Serialise_glClearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearBufferiv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		GLuint framebuffer = 0;
		if(GetCtxData().m_DrawFramebufferRecord)
			framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERI);
		Serialise_glClearNamedFramebufferiv(framebuffer, buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	SERIALISE_ELEMENT(ResourceId, Id, (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)) : ResourceId()));
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, drawbuf, drawbuffer);
	
	if(m_State <= EXECUTING)
	{
		if(Id == ResourceId())
			framebuffer = m_FakeBB_FBO;
		else
			framebuffer = GetResourceManager()->GetLiveResource(Id).name;
	}

	string name;

	{
		uint32_t v[4];
		if(value) memcpy(v, value, sizeof(v));

		m_pSerialiser->Serialise<4>("value", v);
		
		if(m_State == READING)
			name = "glClearBufferuiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(drawbuf) + ", " +
						ToStr::Get(v[0]) + ", " +
						ToStr::Get(v[1]) + ", " +
						ToStr::Get(v[2]) + ", " +
						ToStr::Get(v[3]) + ")";
		
		// use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
		// we are running without ARB_dsa support, these functions are emulated in the obvious way. This is
		// necessary since these functions can be serialised even if ARB_dsa was not used originally, and
		// we need to support this case.
		if(m_State <= EXECUTING)
			m_Real.glClearNamedFramebufferuiv(framebuffer, buf, drawbuf, v);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERUI, desc);

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Clear|eDraw_ClearColour;
		
		AddDrawcall(draw, true);
		
		GLuint attachment = 0;
		GLenum attachName = GLenum(eGL_COLOR_ATTACHMENT0 + drawbuf);
		GLenum type = eGL_TEXTURE;
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

		if(attachment)
		{
			if(type == eGL_TEXTURE)
				m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			else
				m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
		}
	}

	return true;
}

void WrappedOpenGL::glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERUI);
		Serialise_glClearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearBufferuiv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		GLuint framebuffer = 0;
		if(GetCtxData().m_DrawFramebufferRecord)
			framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERUI);
		Serialise_glClearNamedFramebufferuiv(framebuffer, buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}
		
bool WrappedOpenGL::Serialise_glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLfloat depth, GLint stencil)
{
	SERIALISE_ELEMENT(ResourceId, Id, (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)) : ResourceId()));
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(int32_t, s, stencil);
	
	if(m_State <= EXECUTING)
	{
		if(Id == ResourceId())
			framebuffer = m_FakeBB_FBO;
		else
			framebuffer = GetResourceManager()->GetLiveResource(Id).name;
	}
	
	// use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
	// we are running without ARB_dsa support, these functions are emulated in the obvious way. This is
	// necessary since these functions can be serialised even if ARB_dsa was not used originally, and
	// we need to support this case.
	if(m_State <= EXECUTING)
		m_Real.glClearNamedFramebufferfi(framebuffer, buf, d, s);
	
	const string desc = m_pSerialiser->GetDebugStr();
	
	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERFI, desc);
		string name = "glClearBufferfi(" +
						ToStr::Get(d) +
						ToStr::Get(s) + ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Clear|eDraw_ClearDepthStencil;
		
		AddDrawcall(draw, true);
		
		GLuint attachment = 0;
		GLenum type = eGL_TEXTURE;
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

		if(attachment)
		{
			if(type == eGL_TEXTURE)
				m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			else
				m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
		}
		
		attachment = 0;
		type = eGL_TEXTURE;
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
		m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

		if(attachment)
		{
			if(type == eGL_TEXTURE)
				m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			else
				m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
		}
	}

	return true;
}

void WrappedOpenGL::glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLfloat depth, GLint stencil)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearNamedFramebufferfi(framebuffer, buffer, depth, stencil);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERFI);
		Serialise_glClearNamedFramebufferfi(framebuffer, buffer, depth, stencil);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearBufferfi(buffer, drawbuffer, depth, stencil);

	if(m_State == WRITING_CAPFRAME)
	{
		GLuint framebuffer = 0;
		if(GetCtxData().m_DrawFramebufferRecord)
			framebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;
		
		// drawbuffer is ignored, as it must be 0 anyway
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERFI);
		Serialise_glClearNamedFramebufferfi(framebuffer, buffer, depth, stencil);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
	SERIALISE_ELEMENT(GLenum, InternalFormat, internalformat);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);

	uint64_t val[4] = {0};

	if(m_State >= WRITING)
	{
		size_t s = 1;
		switch(Format)
		{
			default:
				RDCWARN("Unexpected format %x, defaulting to single component", Format);
			case eGL_RED:
			case eGL_DEPTH_COMPONENT:
				s = 1; break;
			case eGL_RG:
			case eGL_DEPTH_STENCIL:
				s = 2; break;
			case eGL_RGB:
			case eGL_BGR:
				s = 3; break;
			case eGL_RGBA:
			case eGL_BGRA:
				s = 4; break;
		}
		switch(Type)
		{
			case eGL_UNSIGNED_BYTE:
			case eGL_BYTE:
				s *= 1; break;
			case eGL_UNSIGNED_SHORT:
			case eGL_SHORT:
				s *= 2; break;
			case eGL_UNSIGNED_INT:
			case eGL_INT:
			case eGL_FLOAT:
				s *= 4; break;
			default:
				RDCWARN("Unexpected type %x, defaulting to 1 byte single component type", Format);
			case eGL_UNSIGNED_BYTE_3_3_2:
			case eGL_UNSIGNED_BYTE_2_3_3_REV:
				s = 1; break;
			case eGL_UNSIGNED_SHORT_5_6_5:
			case eGL_UNSIGNED_SHORT_5_6_5_REV:
			case eGL_UNSIGNED_SHORT_4_4_4_4:
			case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
			case eGL_UNSIGNED_SHORT_5_5_5_1:
			case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
			case eGL_UNSIGNED_INT_8_8_8_8:
			case eGL_UNSIGNED_INT_8_8_8_8_REV:
				s = 2; break;
			case eGL_UNSIGNED_INT_10_10_10_2:
			case eGL_UNSIGNED_INT_2_10_10_10_REV:
				s = 4; break;
		}
		memcpy(val, data, s);
	}

	m_pSerialiser->Serialise<4>("data", val);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glClearNamedBufferDataEXT(GetResourceManager()->GetLiveResource(id).name, InternalFormat, Format, Type, (const void *)&val[0]);
	}

	return true;
}

void WrappedOpenGL::glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearNamedBufferDataEXT(buffer, internalformat, format, type, data);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERDATA);
		Serialise_glClearNamedBufferDataEXT(buffer, internalformat, format, type, data);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearBufferData(target, internalformat, format, type, data);

	if(m_State == WRITING_CAPFRAME)
	{
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);
		
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERDATA);
		Serialise_glClearNamedBufferDataEXT(record->Resource.name, internalformat, format, type, data);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));
	SERIALISE_ELEMENT(GLenum, InternalFormat, internalformat);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);

	uint64_t val[4] = {0};

	if(m_State >= WRITING)
	{
		size_t s = 1;
		switch(Format)
		{
			default:
				RDCWARN("Unexpected format %x, defaulting to single component", Format);
			case eGL_RED:
			case eGL_DEPTH_COMPONENT:
				s = 1; break;
			case eGL_RG:
			case eGL_DEPTH_STENCIL:
				s = 2; break;
			case eGL_RGB:
			case eGL_BGR:
				s = 3; break;
			case eGL_RGBA:
			case eGL_BGRA:
				s = 4; break;
		}
		switch(Type)
		{
			case eGL_UNSIGNED_BYTE:
			case eGL_BYTE:
				s *= 1; break;
			case eGL_UNSIGNED_SHORT:
			case eGL_SHORT:
				s *= 2; break;
			case eGL_UNSIGNED_INT:
			case eGL_INT:
			case eGL_FLOAT:
				s *= 4; break;
			default:
				RDCWARN("Unexpected type %x, defaulting to 1 byte single component type", Format);
			case eGL_UNSIGNED_BYTE_3_3_2:
			case eGL_UNSIGNED_BYTE_2_3_3_REV:
				s = 1; break;
			case eGL_UNSIGNED_SHORT_5_6_5:
			case eGL_UNSIGNED_SHORT_5_6_5_REV:
			case eGL_UNSIGNED_SHORT_4_4_4_4:
			case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
			case eGL_UNSIGNED_SHORT_5_5_5_1:
			case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
			case eGL_UNSIGNED_INT_8_8_8_8:
			case eGL_UNSIGNED_INT_8_8_8_8_REV:
				s = 2; break;
			case eGL_UNSIGNED_INT_10_10_10_2:
			case eGL_UNSIGNED_INT_2_10_10_10_REV:
				s = 4; break;
		}
		memcpy(val, data, s);
	}

	m_pSerialiser->Serialise<4>("data", val);

	if(m_State <= EXECUTING)
	{
		m_Real.glClearNamedBufferSubDataEXT(GetResourceManager()->GetLiveResource(id).name, InternalFormat, (GLintptr)Offset, (GLsizeiptr)Size, Format, Type, (const void *)&val[0]);
	}

	return true;
}

void WrappedOpenGL::glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERSUBDATA);
		Serialise_glClearNamedBufferSubDataEXT(buffer, internalformat, offset, size, format, type, data);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizei size, GLenum format, GLenum type, const void *data)
{
	// only difference to EXT function is size parameter, so just upcast
	glClearNamedBufferSubDataEXT(buffer, internalformat, offset, (GLsizeiptr)size, format, type, data);
}

void WrappedOpenGL::glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearBufferSubData(target, internalformat, offset, size, format, type, data);

	if(m_State == WRITING_CAPFRAME)
	{
		GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
		RDCASSERT(record);
		
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERSUBDATA);
		Serialise_glClearNamedBufferSubDataEXT(record->Resource.name, internalformat, offset, size, format, type, data);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClear(GLbitfield mask)
{
	SERIALISE_ELEMENT(uint32_t, Mask, mask);

	if(m_State <= EXECUTING)
		m_Real.glClear(Mask);
	
	const string desc = m_pSerialiser->GetDebugStr();

	Serialise_DebugMessages();

	if(m_State == READING)
	{
		AddEvent(CLEAR, desc);
		string name = "glClear(";
		if(Mask & GL_COLOR_BUFFER_BIT)
		{
			float col[4] = {0};
			m_Real.glGetFloatv(eGL_COLOR_CLEAR_VALUE, &col[0]);
			name += StringFormat::Fmt("Color = <%f, %f, %f, %f>, ", col[0], col[1], col[2], col[3]);
		}
		if(Mask & GL_DEPTH_BUFFER_BIT)
		{
			float depth = 0;
			m_Real.glGetFloatv(eGL_DEPTH_CLEAR_VALUE, &depth);
			name += StringFormat::Fmt("Depth = <%f>, ", depth);
		}
		if(Mask & GL_STENCIL_BUFFER_BIT)
		{
			GLint stencil = 0;
			m_Real.glGetIntegerv(eGL_STENCIL_CLEAR_VALUE, &stencil);
			name += StringFormat::Fmt("Stencil = <0x%02x>, ", stencil);
		}

		if(Mask & (eGL_DEPTH_BUFFER_BIT|eGL_COLOR_BUFFER_BIT|eGL_STENCIL_BUFFER_BIT))
		{
			name.pop_back(); // ','
			name.pop_back(); // ' '
		}

		name += ")";

		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_Clear;
		if(Mask & GL_COLOR_BUFFER_BIT)
			draw.flags |= eDraw_ClearColour;
		if(Mask & (eGL_DEPTH_BUFFER_BIT|eGL_STENCIL_BUFFER_BIT))
			draw.flags |= eDraw_ClearDepthStencil;
		
		AddDrawcall(draw, true);
		
		GLuint attachment = 0;
		GLenum type = eGL_TEXTURE;

		if(Mask & GL_DEPTH_BUFFER_BIT)
		{
			m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
			m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

			if(attachment)
			{
				if(type == eGL_TEXTURE)
					m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
				else
					m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			}
		}
		
		attachment = 0;
		type = eGL_TEXTURE;
		
		if(Mask & GL_STENCIL_BUFFER_BIT)
		{
			m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
			m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

			if(attachment)
			{
				if(type == eGL_TEXTURE)
					m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
				else
					m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
			}
		}
		
		if(Mask & GL_COLOR_BUFFER_BIT)
		{
			GLint numCols = 8;
			m_Real.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

			for(int i=0; i < numCols; i++)
			{
				attachment = 0;
				type = eGL_TEXTURE;

				m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&attachment);
				m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);

				if(attachment)
				{
					if(type == eGL_TEXTURE)
						m_ResourceUses[GetResourceManager()->GetID(TextureRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
					else
						m_ResourceUses[GetResourceManager()->GetID(RenderbufferRes(GetCtx(), attachment))].push_back(EventUsage(m_CurEventID, eUsage_Clear));
				}
			}
		}
	}

	return true;
}

void WrappedOpenGL::glClear(GLbitfield mask)
{
	CoherentMapImplicitBarrier();

	m_Real.glClear(mask);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR);
		Serialise_glClear(mask);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void *data)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);

	uint64_t val[4] = {0};

	if(m_State >= WRITING)
	{
		size_t s = 1;
		switch(Format)
		{
			default:
				RDCWARN("Unexpected format %x, defaulting to single component", Format);
			case eGL_RED:
			case eGL_DEPTH_COMPONENT:
				s = 1; break;
			case eGL_RG:
			case eGL_DEPTH_STENCIL:
				s = 2; break;
			case eGL_RGB:
			case eGL_BGR:
				s = 3; break;
			case eGL_RGBA:
			case eGL_BGRA:
				s = 4; break;
		}
		switch(Type)
		{
			case eGL_UNSIGNED_BYTE:
			case eGL_BYTE:
				s *= 1; break;
			case eGL_UNSIGNED_SHORT:
			case eGL_SHORT:
				s *= 2; break;
			case eGL_UNSIGNED_INT:
			case eGL_INT:
			case eGL_FLOAT:
				s *= 4; break;
			default:
				RDCWARN("Unexpected type %x, defaulting to 1 byte single component type", Format);
			case eGL_UNSIGNED_BYTE_3_3_2:
			case eGL_UNSIGNED_BYTE_2_3_3_REV:
				s = 1; break;
			case eGL_UNSIGNED_SHORT_5_6_5:
			case eGL_UNSIGNED_SHORT_5_6_5_REV:
			case eGL_UNSIGNED_SHORT_4_4_4_4:
			case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
			case eGL_UNSIGNED_SHORT_5_5_5_1:
			case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
			case eGL_UNSIGNED_INT_8_8_8_8:
			case eGL_UNSIGNED_INT_8_8_8_8_REV:
				s = 2; break;
			case eGL_UNSIGNED_INT_10_10_10_2:
			case eGL_UNSIGNED_INT_2_10_10_10_REV:
				s = 4; break;
		}
		memcpy(val, data, s);
	}

	m_pSerialiser->Serialise<4>("data", val);

	if(m_State <= EXECUTING)
	{
		m_Real.glClearTexImage(GetResourceManager()->GetLiveResource(id).name, Level, Format, Type, (const void *)&val[0]);
	}

	return true;
}

void WrappedOpenGL::glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void *data)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearTexImage(texture, level, format, type, data);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARTEXIMAGE);
		Serialise_glClearTexImage(texture, level, format, type, data);

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State == WRITING_IDLE)
	{
		GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
	}
}

bool WrappedOpenGL::Serialise_glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, Xoffs, xoffset);
	SERIALISE_ELEMENT(int32_t, Yoffs, yoffset);
	SERIALISE_ELEMENT(int32_t, Zoffs, zoffset);
	SERIALISE_ELEMENT(int32_t, w, width);
	SERIALISE_ELEMENT(int32_t, h, height);
	SERIALISE_ELEMENT(int32_t, d, depth);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);

	uint64_t val[4] = {0};

	if(m_State >= WRITING)
	{
		size_t s = 1;
		switch(Format)
		{
			default:
				RDCWARN("Unexpected format %x, defaulting to single component", Format);
			case eGL_RED:
			case eGL_DEPTH_COMPONENT:
				s = 1; break;
			case eGL_RG:
			case eGL_DEPTH_STENCIL:
				s = 2; break;
			case eGL_RGB:
			case eGL_BGR:
				s = 3; break;
			case eGL_RGBA:
			case eGL_BGRA:
				s = 4; break;
		}
		switch(Type)
		{
			case eGL_UNSIGNED_BYTE:
			case eGL_BYTE:
				s *= 1; break;
			case eGL_UNSIGNED_SHORT:
			case eGL_SHORT:
				s *= 2; break;
			case eGL_UNSIGNED_INT:
			case eGL_INT:
			case eGL_FLOAT:
				s *= 4; break;
			default:
				RDCWARN("Unexpected type %x, defaulting to 1 byte single component type", Format);
			case eGL_UNSIGNED_BYTE_3_3_2:
			case eGL_UNSIGNED_BYTE_2_3_3_REV:
				s = 1; break;
			case eGL_UNSIGNED_SHORT_5_6_5:
			case eGL_UNSIGNED_SHORT_5_6_5_REV:
			case eGL_UNSIGNED_SHORT_4_4_4_4:
			case eGL_UNSIGNED_SHORT_4_4_4_4_REV:
			case eGL_UNSIGNED_SHORT_5_5_5_1:
			case eGL_UNSIGNED_SHORT_1_5_5_5_REV:
			case eGL_UNSIGNED_INT_8_8_8_8:
			case eGL_UNSIGNED_INT_8_8_8_8_REV:
				s = 2; break;
			case eGL_UNSIGNED_INT_10_10_10_2:
			case eGL_UNSIGNED_INT_2_10_10_10_REV:
				s = 4; break;
		}
		memcpy(val, data, s);
	}

	m_pSerialiser->Serialise<4>("data", val);

	if(m_State <= EXECUTING)
	{
		m_Real.glClearTexSubImage(GetResourceManager()->GetLiveResource(id).name, Level, Xoffs, Yoffs, Zoffs, w, h, d, Format, Type, (const void *)&val[0]);
	}

	return true;
}

void WrappedOpenGL::glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data)
{
	CoherentMapImplicitBarrier();

	m_Real.glClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARTEXSUBIMAGE);
		Serialise_glClearTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);

		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State == WRITING_IDLE)
	{
		GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
	}
}
